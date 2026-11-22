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

} // namespace model
