#include "render/SnapshotBuilder.h"

#include "model/EventModel.h"
#include "model/NoteModel.h"
#include "model/PlacementModel.h"
#include "model/TrackModel.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>

namespace render
{

namespace
{

    ClipTemplate::EventKind kindOf (model::EventModel::Kind kind)
    {
        switch (kind)
        {
            case model::EventModel::Kind::cc:
                return ClipTemplate::EventKind::cc;
            case model::EventModel::Kind::pitchBend:
                return ClipTemplate::EventKind::pitchBend;
            case model::EventModel::Kind::programChange:
                return ClipTemplate::EventKind::programChange;
            case model::EventModel::Kind::channelPressure:
                return ClipTemplate::EventKind::channelPressure;
            case model::EventModel::Kind::polyPressure:
                return ClipTemplate::EventKind::polyPressure;
            case model::EventModel::Kind::sysex:
                return ClipTemplate::EventKind::sysex;
        }

        jassertfalse;
        return ClipTemplate::EventKind::cc;
    }

// Appends the bytes that a sysex event's hexadecimal digits stand for. The
// Validator lets no document in whose payload is anything but pairs of such
// digits (invariant 10).
    void appendBytesOfHex (std::vector<std::uint8_t>& bytes, const juce::String& hexDigits)
    {
        jassert (hexDigits.length() % 2 == 0);

        auto digit = hexDigits.getCharPointer();

        while (! digit.isEmpty())
        {
            const int high = juce::CharacterFunctions::getHexDigitValue (digit.getAndAdvance());
            const int low = juce::CharacterFunctions::getHexDigitValue (digit.getAndAdvance());
            jassert (high >= 0 && low >= 0);

            bytes.push_back (static_cast<std::uint8_t> (high * 16 + low));
        }
    }

// Every clip id that a placement of a track, a placement of one of its
// AutomationLanes, or a slot names.
    std::set<juce::String> referencedClipIdsOf (const std::vector<model::TrackModel>& tracks)
    {
        std::set<juce::String> clipIds;

        for (const auto& track : tracks)
        {
            for (const auto& placement : track.getPlacements())
                clipIds.insert (placement.getClipId());

            for (const auto& lane : track.getAutomationLanes())
                for (const auto& placement : lane.getPlacements())
                    clipIds.insert (placement.getClipId());

            for (const auto& slot : track.getSlots())
                clipIds.insert (slot.getClipId());
        }

        return clipIds;
    }

    TrackPlan planOf (const model::TrackModel& track, const std::map<juce::String, std::size_t>& templateIndexOfClipId)
    {
        TrackPlan plan;
        plan.channel = track.getChannel();

        for (const auto& placement : track.getPlacements())
        {
            const auto templateIndex = templateIndexOfClipId.find (placement.getClipId());

        // Invariant 1: every clipId resolves to a clip.
            jassert (templateIndex != templateIndexOfClipId.end());

            if (templateIndex == templateIndexOfClipId.end())
                continue;

            plan.placements.push_back ({ placement.getStart(), placement.getLength(), placement.getOffset(), templateIndex->second });
        }

    // The document keeps placements in the order they were made. No two of
    // one track share a tick (invariant 3), so their starts all differ.
        std::sort (plan.placements.begin(), plan.placements.end(), [] (const TrackPlan::Placement& earlier, const TrackPlan::Placement& later) {
            return earlier.start < later.start;
        });

        return plan;
    }

} // namespace

Snapshot SnapshotBuilder::build (const model::ProjectModel& project)
{
    const auto tracks = project.getTracks().getTracks();
    const auto referencedClipIds = referencedClipIdsOf (tracks);

    std::vector<ClipTemplate> clipTemplates;
    std::map<juce::String, std::size_t> templateIndexOfClipId;

    for (const auto& clip : project.getClipPool().getClips())
    {
        const auto clipId = clip.getId();

        if (referencedClipIds.count (clipId) == 0)
            continue;

        templateIndexOfClipId[clipId] = clipTemplates.size();
        clipTemplates.push_back (templateOf (clip));
    }

    std::vector<TrackPlan> trackPlans;

    for (const auto& track : tracks)
        trackPlans.push_back (planOf (track, templateIndexOfClipId));

    return Snapshot { TempoMap (tempoSegmentsOf (project.getTimeline())), std::move (clipTemplates), std::move (trackPlans) };
}

ClipTemplate SnapshotBuilder::templateOf (const model::ClipModel& clip)
{
    ClipTemplate clipTemplate;
    clipTemplate.loopLength = clip.getLoopLength();
    jassert (clipTemplate.loopLength > 0);

    if (clip.getKind() == model::ClipModel::Kind::step)
        return clipTemplate;

    for (const auto& note : clip.getNotes())
    {
        if (! note.isEnabled() || note.getTick() >= clipTemplate.loopLength)
            continue;

        clipTemplate.notes.push_back ({ note.getTick(), note.getLength(), note.getPitch(), note.getVelocity(), note.getOffVelocity() });
    }

    for (const auto& event : clip.getEvents())
    {
        if (! event.isEnabled() || event.getTick() >= clipTemplate.loopLength)
            continue;

        ClipTemplate::Event templateEvent;
        templateEvent.tick = event.getTick();
        templateEvent.kind = kindOf (event.getKind());
        templateEvent.a = event.getA();
        templateEvent.b = event.getB();

        if (templateEvent.kind == ClipTemplate::EventKind::sysex)
        {
            templateEvent.dataStart = clipTemplate.sysexBytes.size();
            appendBytesOfHex (clipTemplate.sysexBytes, event.getData());
            templateEvent.dataLength = clipTemplate.sysexBytes.size() - templateEvent.dataStart;
        }

        clipTemplate.events.push_back (templateEvent);
    }

    // A stable sort: what shares a tick stays in child order, which is the
    // tie-breaker at equal ticks. The document itself is never sorted.
    std::stable_sort (clipTemplate.notes.begin(), clipTemplate.notes.end(), [] (const ClipTemplate::Note& earlier, const ClipTemplate::Note& later) {
        return earlier.tick < later.tick;
    });
    std::stable_sort (clipTemplate.events.begin(), clipTemplate.events.end(), [] (const ClipTemplate::Event& earlier, const ClipTemplate::Event& later) {
        return earlier.tick < later.tick;
    });

    // Fixed in size once built: nothing is left over to grow into.
    clipTemplate.notes.shrink_to_fit();
    clipTemplate.events.shrink_to_fit();
    clipTemplate.sysexBytes.shrink_to_fit();

    return clipTemplate;
}

std::vector<TempoMap::Segment> SnapshotBuilder::tempoSegmentsOf (const model::TimelineModel& timeline)
{
    std::vector<TempoMap::Segment> segments;

    for (const auto& tempoEvent : timeline.getTempoEvents())
        segments.push_back ({ tempoEvent.getTick(), TempoMap::microsecondsPerQuarterAt (tempoEvent.getBpm()) });

    // The document keeps tempo events in the order they were made; a
    // Timeline has at most one per tick (invariant 6).
    std::sort (segments.begin(), segments.end(), [] (const TempoMap::Segment& earlier, const TempoMap::Segment& later) {
        return earlier.startTick < later.startTick;
    });

    return segments;
}

} // namespace render
