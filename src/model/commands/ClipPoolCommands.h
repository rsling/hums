#pragma once

#include "model/commands/CommandResult.h"
#include "model/commands/EditContext.h"

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
// A clip is named by its id. Making and removing are commands on the pool.
// Name and colour are changes to a clip, so they pass the refcount gate with
// the gesture's edit context like every such change (docs/EDITING.md,
// RefcountGate): set for every reference of the clip, or, under "make
// unique" through the references the context names, for a clone that only
// those reference. A context that names no reference sets them for all.
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

// Any text, also an empty one. Setting the name the clip already has
// changes nothing and makes no clone. Refused when the gate refuses.
CommandResult setClipName (juce::ValueTree project, const juce::String& clipId, const juce::String& name, EditContext& editContext, juce::UndoManager& undoManager);

// "#AARRGGBB", or empty for no colour; anything else is refused. Otherwise
// like setClipName.
CommandResult setClipColour (juce::ValueTree project, const juce::String& clipId, const juce::String& colour, EditContext& editContext, juce::UndoManager& undoManager);

// Takes the clip out of the pool. Refused while a Placement or a Slot
// references it (refcount > 0): remove those first.
CommandResult removeClip (juce::ValueTree project, const juce::String& clipId, juce::UndoManager& undoManager);

// "Remove unused clips": takes every clip out of the pool that no Placement
// and no Slot references (refcount 0). Applied also when there is none; it
// then performs nothing.
CommandResult removeUnusedClips (juce::ValueTree project, juce::UndoManager& undoManager);

} // namespace model::commands
