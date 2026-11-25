#include "RenderRun.h"

#include "AllocationCounter.h"
#include "render/Kernel.h"
#include "render/NoteTracker.h"

#include <algorithm>
#include <span>
#include <utility>

namespace guardrails
{

namespace
{

    // One call of play: the tracker lives from one stretch of ticks to the
    // next.
    struct Playback
    {
        Playback (const render::Snapshot& snapshotToPlay,
                  const std::vector<bool>& audibleTracksToUse,
                  const std::vector<std::size_t>& destinationOfTrackToUse,
                  std::size_t destinationCount,
                  std::size_t bufferSize)
            : snapshot (snapshotToPlay),
              audibleTracks (audibleTracksToUse),
              destinationOfTrack (destinationOfTrackToUse),
              tracker (destinationCount),
              buffer (bufferSize)
        {
        }

        void keep (std::size_t eventCount)
        {
            result.events.insert (result.events.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t> (eventCount));
        }

        // The ticks [start, end) in their order, in windows of windowLength.
        void render (std::int64_t start, std::int64_t end, std::int64_t windowLength)
        {
            std::int64_t windowStart = start;

            do
            {
                const std::int64_t windowEnd = windowLength == RenderRun::oneWindow ? end : std::min (end, windowStart + windowLength);
                render::Kernel::Position position { windowStart, 0 };

                for (bool isFinished = false; ! isFinished;)
                {
                    const auto countBefore = AllocationCounter::count();
                    const auto progress = render::Renderer::render (snapshot, audibleTracks, destinationOfTrack, tracker, position, windowEnd, buffer);
                    result.allocationCount += AllocationCounter::count() - countBefore;

                    keep (progress.eventCount);
                    position = progress.next;
                    isFinished = progress.isFinished;
                }

                windowStart = windowEnd;
            } while (windowStart < end);
        }

        // Playback leaves one tick for another: a seek, or the wrap of the
        // loop.
        void seek (std::int64_t leftTick, std::int64_t targetTick)
        {
            render::Renderer::ChasePosition position;

            for (bool isFinished = false; ! isFinished;)
            {
                const auto countBefore = AllocationCounter::count();
                const auto progress = render::Renderer::seek (snapshot, audibleTracks, destinationOfTrack, tracker, leftTick, targetTick, position, buffer);
                result.allocationCount += AllocationCounter::count() - countBefore;

                keep (progress.eventCount);
                position = progress.next;
                isFinished = progress.isFinished;
            }
        }

        // The end, as an export ends: with the off of every note that still
        // sounds.
        void stop (std::int64_t stopTick)
        {
            do
            {
                const auto countBefore = AllocationCounter::count();
                const auto offCount = render::Renderer::stop (tracker, stopTick, buffer);
                result.allocationCount += AllocationCounter::count() - countBefore;

                keep (offCount);
            } while (tracker.soundingCount() > 0);
        }

        const render::Snapshot& snapshot;
        const std::vector<bool>& audibleTracks;
        const std::vector<std::size_t>& destinationOfTrack;
        render::NoteTracker tracker;
        std::vector<render::Renderer::Event> buffer;
        RenderRun::Result result;
    };

} // namespace

RenderRun::RenderRun (const render::Snapshot& snapshotToPlay, std::vector<std::size_t> destinationOfTrackToUse, std::size_t destinationCountToUse)
    : snapshot (snapshotToPlay),
      destinationOfTrack (std::move (destinationOfTrackToUse)),
      destinationCount (destinationCountToUse),
      allTracks (snapshotToPlay.trackPlans.size(), true)
{
}

RenderRun::Result RenderRun::play (const Plan& plan, std::int64_t windowLength, std::size_t bufferSize) const
{
    Playback playback (snapshot, allTracks, destinationOfTrack, destinationCount, bufferSize);

    // Nothing sounds yet, so the seek gives the chase alone.
    if (plan.beginsWithSeek)
        playback.seek (plan.start, plan.start);

    if (plan.loopStart.has_value() && plan.loopEnd.has_value())
    {
        playback.render (plan.start, *plan.loopEnd, windowLength);
        playback.seek (*plan.loopEnd, *plan.loopStart);
        playback.render (*plan.loopStart, plan.end, windowLength);
    }
    else
    {
        playback.render (plan.start, plan.end, windowLength);
    }

    playback.stop (plan.end);

    return std::move (playback.result);
}

const render::Snapshot& RenderRun::getSnapshot() const noexcept
{
    return snapshot;
}

} // namespace guardrails
