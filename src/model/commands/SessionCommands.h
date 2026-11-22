#pragma once

#include "model/commands/CommandResult.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model::commands
{

// The commands for the Session node (docs/SCHEMA.md -> Session): the launch
// grid and the names of the scenes. Inserting, deleting and moving a scene
// renumber every track's slots and are with the slot commands
// (model/commands/SlotCommands).
//
// Like every command: `project` is the Project node of a valid document; a
// command applies completely or refuses and changes nothing, and never
// begins a transaction.

// The launch grid in ticks from tick 0; 0 = immediate. Refused when
// negative.
CommandResult setLaunchQuantise (juce::ValueTree project, std::int64_t ticks, juce::UndoManager& undoManager);

// Names the scene in this row. Scenes are sparse: a row has a Scene node
// only once it has been named, so this adds the node when there is none. An
// empty name takes the name away again; the node, once there, stays (it
// says nothing and costs nothing). Refused when the index is negative.
CommandResult setSceneName (juce::ValueTree project, int sceneIndex, const juce::String& name, juce::UndoManager& undoManager);

} // namespace model::commands
