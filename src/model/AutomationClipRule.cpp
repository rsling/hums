#include "model/AutomationClipRule.h"

#include "model/ClipModel.h"
#include "model/Ids.h"
#include "model/TracksModel.h"

namespace model
{

juce::String AutomationClipRule::whyNotForALane (const juce::ValueTree& clip)
{
    const ClipModel model (clip);

    if (model.getKind() != ClipModel::Kind::event)
        return "An automation lane takes only an event clip that holds nothing but cc events; this is a step clip.";

    if (! model.getNotes().empty())
        return "An automation lane takes only a clip that holds nothing but cc events; this clip holds notes.";

    for (const auto& event : model.getEvents())
        if (event.getKind() != EventModel::Kind::cc)
            return "An automation lane takes only a clip that holds nothing but cc events; this clip holds events of other kinds.";

    return {};
}

bool AutomationClipRule::isOnALane (const juce::ValueTree& project, const juce::String& clipId)
{
    jassert (project.hasType (ids::Project));

    for (const auto& track : TracksModel (project.getChildWithName (ids::Tracks)).getTracks())
        for (const auto& lane : track.getAutomationLanes())
            for (const auto& placement : lane.getPlacements())
                if (placement.getClipId() == clipId)
                    return true;

    return false;
}

bool AutomationClipRule::isOnATrackOrInASlot (const juce::ValueTree& project, const juce::String& clipId)
{
    jassert (project.hasType (ids::Project));

    for (const auto& track : TracksModel (project.getChildWithName (ids::Tracks)).getTracks())
    {
        for (const auto& placement : track.getPlacements())
            if (placement.getClipId() == clipId)
                return true;

        for (const auto& slot : track.getSlots())
            if (slot.getClipId() == clipId)
                return true;
    }

    return false;
}

juce::String AutomationClipRule::refusalForAClipOfALane()
{
    return "An automation lane references this clip, and such a clip stays on automation lanes: it is not placed on a track and not put into a slot.";
}

juce::String AutomationClipRule::refusalForAClipOfATrack()
{
    return "A track's placement or a slot references this clip, and an automation lane takes only a clip that is on automation lanes and nowhere else.";
}

} // namespace model
