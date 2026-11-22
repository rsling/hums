#pragma once

#include "model/commands/CommandResult.h"
#include "model/commands/EditContext.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>
#include <optional>

namespace model::commands
{

// The commands for what a step clip holds (docs/SCHEMA.md -> Step clip:
// Grid): its lanes, their cells and the cells' CC values, and what belongs to
// the whole grid: the step length, the step count, the reset and the skipped
// columns. An event clip has none of these, and every command here refuses
// one.
//
// Each is a change to a clip, so it passes the refcount gate with the
// gesture's edit context (docs/EDITING.md, RefcountGate): the change is made
// for every reference of the clip, or, under "make unique" through the
// references the context names, to a clone that only those reference. A
// change that changes nothing makes no clone.
//
// A lane is named by its position among the clip's lanes, from 0 at the top,
// and a cell by its lane and its column, from 0. A lane's id would not do:
// under "make unique" the change lands on the clone, whose lanes have ids of
// their own, and the position is the same in both. Removing or moving a lane
// moves the positions of others.
//
// The four commands on the grid change the length of the loop (step length x
// the columns before the reset that are not skipped). One column must be left
// to play (invariant 2); a change after which none would be is refused. Every
// placement that references the changed clip gets its offset brought back
// into the new loop in the same undo step (invariant 4).
//
// Like every command: `project` is the Project node of a valid document; a
// command checks everything before it touches the tree, applies completely
// or refuses and changes nothing, and never begins a transaction.

// What a cell is made of, its CC values aside (docs/SCHEMA.md -> Cell). As
// made here, it is the cell the schema describes when nothing is stored: a
// rest.
struct CellValues
{
    // 0 to 127. Kept, and not played, in a lane with a fixed pitch.
    int pitch = 60;
    // An off cell is a rest.
    bool on = false;
    // 1 to 127.
    int velocity = 100;
    // The note's length as a fraction of the step length: greater than 0
    // and at most 4. The document keeps six significant digits [D14]; the
    // range is asked of the value so rounded.
    double gate = 0.5;
};

// Puts a lane below the clip's others, with a rest in every column. With a
// fixed pitch (0 to 127) it is a drum row, without one a melodic row. The
// result names the new lane's id.
CommandResult addLane (juce::ValueTree project, const juce::String& clipId, const juce::String& name, std::optional<int> fixedPitch, EditContext& editContext, juce::UndoManager& undoManager);

// Takes the lane out of the clip, with its cells. A clip may be left with no
// lane at all.
CommandResult removeLane (juce::ValueTree project, const juce::String& clipId, int laneIndex, EditContext& editContext, juce::UndoManager& undoManager);

// Moves the lane to a position among the clip's lanes, from 0 at the top;
// the others keep their order.
CommandResult moveLane (juce::ValueTree project, const juce::String& clipId, int laneIndex, int toPosition, EditContext& editContext, juce::UndoManager& undoManager);

CommandResult setLaneName (juce::ValueTree project, const juce::String& clipId, int laneIndex, const juce::String& name, EditContext& editContext, juce::UndoManager& undoManager);

// A pitch makes the lane a drum row; none makes it a melodic row again. The
// cells keep their own pitches either way.
CommandResult setLaneFixedPitch (juce::ValueTree project, const juce::String& clipId, int laneIndex, std::optional<int> fixedPitch, EditContext& editContext, juce::UndoManager& undoManager);

// Gives the cell of that lane in that column these values. Its CC values
// stay. The gate is stored as its saved text, six significant digits [D14].
CommandResult setCell (juce::ValueTree project, const juce::String& clipId, int laneIndex, int column, const CellValues& values, EditContext& editContext, juce::UndoManager& undoManager);

// The value the cell sends for that controller (both 0 to 127): changed when
// the cell has one for it, added behind the cell's others when it has none.
CommandResult setCellCc (juce::ValueTree project, const juce::String& clipId, int laneIndex, int column, int cc, int value, EditContext& editContext, juce::UndoManager& undoManager);

// The cell sends nothing for that controller any more. Refused when it has
// no value for it.
CommandResult removeCellCc (juce::ValueTree project, const juce::String& clipId, int laneIndex, int column, int cc, EditContext& editContext, juce::UndoManager& undoManager);

// Ticks per column, 1 to 3840.
CommandResult setStepLength (juce::ValueTree project, const juce::String& clipId, std::int64_t stepLength, EditContext& editContext, juce::UndoManager& undoManager);

// The number of columns, 1 to 64 (docs/SCHEMA.md -> Changing stepCount).
// Growing gives every lane rests in the new columns, which are not skipped;
// shrinking removes the last columns with what their cells held. A reset
// that stood at the end stays at the end; any other is kept, and where the
// grid is now shorter it comes to the new end.
CommandResult setStepCount (juce::ValueTree project, const juce::String& clipId, int stepCount, EditContext& editContext, juce::UndoManager& undoManager);

// The loop ends before this column: 1 to the step count. The columns from
// there on keep their data.
CommandResult setResetAt (juce::ValueTree project, const juce::String& clipId, int resetAt, EditContext& editContext, juce::UndoManager& undoManager);

// A skipped column takes no time; its cells keep their data.
CommandResult setColumnSkipped (juce::ValueTree project, const juce::String& clipId, int column, bool isSkipped, EditContext& editContext, juce::UndoManager& undoManager);

} // namespace model::commands
