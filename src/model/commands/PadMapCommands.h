#pragma once

#include "model/commands/CommandResult.h"

#include <juce_data_structures/juce_data_structures.h>

namespace model::commands
{

// The commands for the project's one pad map (docs/SCHEMA.md -> Routing,
// PadMap / Pad). The map is input configuration and undoable; remapping a
// pad never rewrites clips.
//
// A pad is named by its index, the pad number, which is unique within the
// map and never changes. Like every command: `project` is the Project node
// of a valid document; a command applies completely or refuses and changes
// nothing, and never begins a transaction.

// Adds a pad. Refused when the index is negative or already in the map,
// when `inputPitch` (the note the physical pad sends) is outside 0 to 127 or
// already another pad's, or when `pitch` (the note the pad produces) is
// outside 0 to 127. Two pads may produce the same pitch.
CommandResult addPad (juce::ValueTree project, int index, int inputPitch, int pitch, const juce::String& label, juce::UndoManager& undoManager);

// Refused when the map has no pad with this index.
CommandResult removePad (juce::ValueTree project, int index, juce::UndoManager& undoManager);

// Refused when the pitch is outside 0 to 127 or is another pad's input
// pitch.
CommandResult setPadInputPitch (juce::ValueTree project, int index, int inputPitch, juce::UndoManager& undoManager);

// Refused when the pitch is outside 0 to 127.
CommandResult setPadPitch (juce::ValueTree project, int index, int pitch, juce::UndoManager& undoManager);

CommandResult setPadLabel (juce::ValueTree project, int index, const juce::String& label, juce::UndoManager& undoManager);

} // namespace model::commands
