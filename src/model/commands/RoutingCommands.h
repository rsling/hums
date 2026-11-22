#pragma once

#include "model/commands/CommandResult.h"

#include <juce_data_structures/juce_data_structures.h>

namespace model::commands
{

// The commands for the logical ports under Routing (docs/SCHEMA.md ->
// Routing): Performers, the logical MIDI inputs, and Players, the logical
// MIDI outputs. The pad map has its own file (PadMapCommands).
//
// Like every command: `project` is the Project node of a valid document; the
// command checks everything before it touches the tree, applies completely
// or refuses and changes nothing, and never begins a transaction.
//
// A name may be any text, also an empty one and one another port already
// has: the schema asks no more, and a port is told from another by its id.

// Makes a Performer with a new id, unbound, listening on every channel. The
// result carries the id.
CommandResult addPerformer (juce::ValueTree project, const juce::String& name, juce::UndoManager& undoManager);

// Removes the Performer. Every Track that records from it becomes
// unassigned (its performerId is emptied) in the same transaction, so one
// undo brings both back (docs/EDITING.md). Refused when no Performer has
// this id.
CommandResult removePerformer (juce::ValueTree project, const juce::String& performerId, juce::UndoManager& undoManager);

CommandResult setPerformerName (juce::ValueTree project, const juce::String& performerId, const juce::String& name, juce::UndoManager& undoManager);

// Which physical device the Performer was last bound to: its identifier and,
// as the fallback match, its name. Both empty = never bound.
CommandResult setPerformerDevice (juce::ValueTree project, const juce::String& performerId, const juce::String& deviceIdentifier, const juce::String& deviceName, juce::UndoManager& undoManager);

// 0 = omni, 1 to 16 = only that channel; anything else is refused.
CommandResult setPerformerChannelFilter (juce::ValueTree project, const juce::String& performerId, int channelFilter, juce::UndoManager& undoManager);

// Makes a Player with a new id, unbound, sending no clock. The result
// carries the id.
CommandResult addPlayer (juce::ValueTree project, const juce::String& name, juce::UndoManager& undoManager);

// Removes the Player. Every Track that plays through it becomes unassigned
// (its playerId is emptied) in the same transaction; such a track still
// renders and exports. Refused when no Player has this id.
CommandResult removePlayer (juce::ValueTree project, const juce::String& playerId, juce::UndoManager& undoManager);

CommandResult setPlayerName (juce::ValueTree project, const juce::String& playerId, const juce::String& name, juce::UndoManager& undoManager);

CommandResult setPlayerDevice (juce::ValueTree project, const juce::String& playerId, const juce::String& deviceIdentifier, const juce::String& deviceName, juce::UndoManager& undoManager);

// Whether the Player sends MIDI clock, Start, Stop, Continue and SPP.
CommandResult setPlayerSendClock (juce::ValueTree project, const juce::String& playerId, bool sendClock, juce::UndoManager& undoManager);

} // namespace model::commands
