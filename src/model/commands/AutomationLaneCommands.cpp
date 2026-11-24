#include "model/commands/AutomationLaneCommands.h"

#include "model/AutomationLaneModel.h"
#include "model/Ids.h"
#include "model/commands/NodeLookup.h"
#include "model/commands/SiblingOrder.h"

namespace model::commands
{

namespace
{

    // The lane with this id, on whichever track it is.
    juce::ValueTree laneOf (const juce::ValueTree& project, const juce::String& laneId)
    {
        jassert (project.hasType (ids::Project));

        for (const auto& track : NodeLookup::childrenOfType (project.getChildWithName (ids::Tracks), ids::Track))
        {
            const auto lane = NodeLookup::childWithId (track.getChildWithName (ids::AutomationLanes), ids::AutomationLane, laneId);

            if (lane.isValid())
                return lane;
        }

        return {};
    }

    CommandResult noSuchLane (const juce::String& laneId)
    {
        return CommandResult::refused ("There is no AutomationLane with the id \"" + laneId + "\".");
    }

} // namespace

CommandResult addAutomationLane (juce::ValueTree project, const juce::String& trackId, const juce::String& name, juce::UndoManager& undoManager)
{
    jassert (project.hasType (ids::Project));

    const auto track = NodeLookup::childWithId (project.getChildWithName (ids::Tracks), ids::Track, trackId);

    if (! track.isValid())
        return CommandResult::refused ("There is no Track with the id \"" + trackId + "\".");

    const auto id = juce::Uuid().toString();

    // Built apart and added whole, so that undo knows one action.
    juce::ValueTree lane (ids::AutomationLane);
    lane.setProperty (ids::id, id, nullptr);
    lane.setProperty (ids::name, name, nullptr);

    track.getChildWithName (ids::AutomationLanes).appendChild (lane, &undoManager);

    return CommandResult::appliedAndCreated (id);
}

CommandResult removeAutomationLane (juce::ValueTree project, const juce::String& laneId, juce::UndoManager& undoManager)
{
    const auto lane = laneOf (project, laneId);

    if (! lane.isValid())
        return noSuchLane (laneId);

    lane.getParent().removeChild (lane, &undoManager);

    return CommandResult::applied();
}

CommandResult moveAutomationLane (juce::ValueTree project, const juce::String& laneId, int toPosition, juce::UndoManager& undoManager)
{
    const auto lane = laneOf (project, laneId);

    if (! lane.isValid())
        return noSuchLane (laneId);

    return SiblingOrder::moveTo (lane, toPosition, "automation lanes on the track", undoManager);
}

CommandResult setAutomationLaneName (juce::ValueTree project, const juce::String& laneId, const juce::String& name, juce::UndoManager& undoManager)
{
    auto lane = laneOf (project, laneId);

    if (! lane.isValid())
        return noSuchLane (laneId);

    // A lane that reads as this name already is left alone, so that an
    // empty name that was never stored stays unstored.
    if (AutomationLaneModel (lane).getName() == name)
        return CommandResult::applied();

    lane.setProperty (ids::name, name, &undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
