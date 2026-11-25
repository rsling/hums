#pragma once

#include "model/commands/CommandResult.h"

#include <juce_data_structures/juce_data_structures.h>

namespace model::commands
{

// The command for the Project node itself (docs/SCHEMA.md -> Project). Its
// other two properties are not for a command: `schemaVersion` belongs to the
// load path, and `ppq` is always 960.
//
// Like every command: `project` is the Project node of a valid document; a
// command applies completely or refuses and changes nothing, and never
// begins a transaction.

// Names the project. Any text is a name, the empty one included, which is
// what a project without a name has; so this command is never refused.
CommandResult setProjectName (juce::ValueTree project, const juce::String& name, juce::UndoManager& undoManager);

} // namespace model::commands
