#pragma once

#include "model/commands/CommandResult.h"

#include <juce_data_structures/juce_data_structures.h>

namespace model::commands
{

// The commands for the session grid (docs/SCHEMA.md -> Slots / Slot,
// Session): what is in a slot, and the scenes as rows that are inserted,
// removed and moved.
//
// A slot is named by its track and its scene, the row it is in; rows count
// from 0. Slots and scenes are sparse: a slot has a node only while a clip
// is in it, a scene only once it has a name, and there is no last row. A
// row is therefore never "not there": every row from 0 on can be named,
// and a row nothing is stored for is an empty one.
//
// A row is an index, not an id. The three scene commands renumber every
// track's Slots and the Session's Scenes together, so a row keeps its clips
// and its name wherever it goes.
//
// None of these commands changes a clip, so none needs an edit context.
//
// Like every command: `project` is the Project node of a valid document; a
// command checks everything before it touches the tree, applies completely
// or refuses and changes nothing, and never begins a transaction.

// Puts the clip into the track's slot in that scene. A clip that was there
// gives way to it (and stays in the pool, with a smaller refcount). A clip
// of either kind may be in any number of slots, but for a clip that an
// AutomationLane references: that one stays on lanes [D21]. Refused for
// such a clip, and when the track or the clip is not there or the scene is
// negative.
CommandResult setSlot (juce::ValueTree project, const juce::String& trackId, int scene, const juce::String& clipId, juce::UndoManager& undoManager);

// Empties the track's slot in that scene. Its clip stays in the pool, with
// a smaller refcount. Refused when the track is not there or the slot is
// empty already.
CommandResult clearSlot (juce::ValueTree project, const juce::String& trackId, int scene, juce::UndoManager& undoManager);

// Inserts an empty, nameless row at this index: the row that was there and
// every row after it move down by one, on every track, with their names.
// Refused when the index is negative, or when something is stored for the
// largest row an int can number, which could not move down.
CommandResult insertScene (juce::ValueTree project, int atScene, juce::UndoManager& undoManager);

// Removes the row: every track's slot in it is emptied, its name is gone,
// and every row after it moves up by one. The clips stay in the pool.
// Applied also to a row nothing is stored for; the rows after it still move
// up. Refused when the index is negative.
CommandResult removeScene (juce::ValueTree project, int scene, juce::UndoManager& undoManager);

// Moves the row, with every track's slot in it and its name, to another
// index. The rows between the two move by one towards the place it left;
// all others stay. Refused when either index is negative.
CommandResult moveScene (juce::ValueTree project, int fromScene, int toScene, juce::UndoManager& undoManager);

} // namespace model::commands
