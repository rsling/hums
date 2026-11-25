#include "render/NoteTracker.h"

#include <algorithm>
#include <cassert>

namespace render
{

NoteTracker::NoteTracker (std::size_t destinationCountToUse)
    : destinationCount (destinationCountToUse)
{
    // One sounding note per pitch, channel and destination is all there can
    // be, so the list never grows past this and never allocates again.
    offsInDueOrder.reserve (destinationCount * static_cast<std::size_t> (channelCount) * static_cast<std::size_t> (pitchCount));
}

std::optional<NoteTracker::NoteOff> NoteTracker::noteOn (std::int64_t onTick, const NoteOff& off) noexcept
{
    assert (off.destination < destinationCount);
    assert (off.channel >= 1 && off.channel <= channelCount);
    assert (off.pitch >= 0 && off.pitch < pitchCount);
    assert (off.offVelocity >= 0 && off.offVelocity <= 127);
    assert (off.tick > onTick);

    std::optional<NoteOff> offOfEarlierNote;

    for (std::size_t position = 0; position < offsInDueOrder.size(); ++position)
    {
        const NoteOff& held = offsInDueOrder[position];

        if (held.destination == off.destination && held.channel == off.channel && held.pitch == off.pitch)
        {
            offOfEarlierNote = takeAt (position, onTick);
            break;
        }
    }

    // Behind every off of the same tick, so that offs of one tick come out
    // in the order in which their notes began.
    const auto firstLaterOff = std::upper_bound (offsInDueOrder.begin(),
                                                 offsInDueOrder.end(),
                                                 off.tick,
                                                 [] (std::int64_t tick, const NoteOff& held) { return tick < held.tick; });

    assert (offsInDueOrder.size() < offsInDueOrder.capacity());
    offsInDueOrder.insert (firstLaterOff, off);

    return offOfEarlierNote;
}

std::optional<NoteTracker::NoteOff> NoteTracker::takeOffDueBy (std::int64_t tick) noexcept
{
    if (offsInDueOrder.empty() || offsInDueOrder.front().tick > tick)
        return std::nullopt;

    return takeAt (0, tick);
}

std::optional<NoteTracker::NoteOff> NoteTracker::takeOffAtStop (std::int64_t stopTick) noexcept
{
    if (offsInDueOrder.empty())
        return std::nullopt;

    return takeAt (0, stopTick);
}

std::size_t NoteTracker::soundingCount() const noexcept
{
    return offsInDueOrder.size();
}

NoteTracker::NoteOff NoteTracker::takeAt (std::size_t position, std::int64_t latestTick) noexcept
{
    NoteOff off = offsInDueOrder[position];
    off.tick = std::min (off.tick, latestTick);

    offsInDueOrder.erase (offsInDueOrder.begin() + static_cast<std::ptrdiff_t> (position));

    return off;
}

} // namespace render
