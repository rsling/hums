#pragma once

#include "model/commands/CommandResult.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model::commands
{

// The commands for the clip pool (docs/SCHEMA.md -> ClipPool / Clip): making
// a clip of either kind, its name and colour, and taking a clip out of the
// pool. What a clip holds — notes and events, the grid with its lanes and
// cells, the loop length of an event clip, its quantise setting — has
// commands of its own.
//
// A clip is named by its id. These are commands on the pool: they name no
// Placement and no Slot through which the clip is edited, so a name or a
// colour that is set here is set for every reference of the clip ("update
// all" by definition, docs/EDITING.md).
//
// Like every command: `project` is the Project node of a valid document; a
// command checks everything before it touches the tree, applies completely
// or refuses and changes nothing, and never begins a transaction.

// Makes an empty event clip with a new id, at the end of the pool: no notes,
// no events, quantise off, no colour. The result carries the id. Nothing
// references the new clip yet. A name may be any text, also an empty one and
// one another clip already has. Refused when the loop length is not at least
// one tick.
CommandResult addEventClip (juce::ValueTree project, const juce::String& name, std::int64_t loopLength, juce::UndoManager& undoManager);

// Makes a step clip with a new id, at the end of the pool. Its Grid is the
// one the schema describes when nothing is stored: 16 columns of a sixteenth
// (240 ticks), none skipped, the loop over all of them — and no lane yet. The
// result carries the id.
CommandResult addStepClip (juce::ValueTree project, const juce::String& name, juce::UndoManager& undoManager);

CommandResult setClipName (juce::ValueTree project, const juce::String& clipId, const juce::String& name, juce::UndoManager& undoManager);

// "#AARRGGBB", or empty for no colour; anything else is refused.
CommandResult setClipColour (juce::ValueTree project, const juce::String& clipId, const juce::String& colour, juce::UndoManager& undoManager);

// Takes the clip out of the pool. Refused while a Placement or a Slot
// references it (refcount > 0): remove those first.
CommandResult removeClip (juce::ValueTree project, const juce::String& clipId, juce::UndoManager& undoManager);

// "Remove unused clips": takes every clip out of the pool that no Placement
// and no Slot references (refcount 0). Applied also when there is none; it
// then performs nothing.
CommandResult removeUnusedClips (juce::ValueTree project, juce::UndoManager& undoManager);

} // namespace model::commands
