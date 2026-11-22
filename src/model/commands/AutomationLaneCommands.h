#pragma once

#include "model/commands/CommandResult.h"

#include <juce_data_structures/juce_data_structures.h>

namespace model::commands
{

// The commands for a track's automation lanes (docs/SCHEMA.md ->
// AutomationLanes / AutomationLane): a lane made, named, moved among the
// others and removed. Putting a clip on a lane, and everything done to a
// placement that is on one, is with the placement commands.
//
// A lane is named by its id, which is unique in the project. A position
// counts from 0 among the lanes of the lane's own track, and the order
// means something: where two lanes cover the same tick and controller
// number, the later one wins.
//
// Like every command: `project` is the Project node of a valid document; a
// command checks everything before it touches the tree, applies completely
// or refuses and changes nothing, and never begins a transaction.

// Makes an empty AutomationLane with a new id behind the track's others, so
// that it overrides them. The result carries the id. A name may be any
// text, also an empty one and one another lane already has. Refused when no
// Track has this id.
CommandResult addAutomationLane (juce::ValueTree project, const juce::String& trackId, const juce::String& name, juce::UndoManager& undoManager);

// Removes the lane with its placements. The clips they referenced stay in
// the pool, with a smaller refcount. Refused when no AutomationLane has
// this id.
CommandResult removeAutomationLane (juce::ValueTree project, const juce::String& laneId, juce::UndoManager& undoManager);

// Moves the lane to that position among the lanes of its track; the others
// keep their order. A lane does not go to another track. Refused when the
// position is below 0 or not below the number of the track's lanes.
CommandResult moveAutomationLane (juce::ValueTree project, const juce::String& laneId, int toPosition, juce::UndoManager& undoManager);

CommandResult setAutomationLaneName (juce::ValueTree project, const juce::String& laneId, const juce::String& name, juce::UndoManager& undoManager);

} // namespace model::commands
