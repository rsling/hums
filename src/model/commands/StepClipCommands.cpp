#include "model/commands/StepClipCommands.h"

#include "model/CanonicalDouble.h"
#include "model/CcValueModel.h"
#include "model/CellModel.h"
#include "model/ClipModel.h"
#include "model/GridModel.h"
#include "model/Ids.h"
#include "model/LaneModel.h"
#include "model/PropertyReader.h"
#include "model/commands/NodeLookup.h"
#include "model/commands/PlacementOffsets.h"
#include "model/commands/RefcountGate.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace model::commands
{

namespace
{

    // The ranges of docs/SCHEMA.md -> Grid.
    const std::int64_t longestStep = 3840;
    const int mostColumns = 64;

    // What every command here begins with: the gate lets the gesture at the
    // clip, and the clip is a step clip.
    RefcountGate::Admission admitToStepClip (const juce::ValueTree& project, const juce::String& clipId, const EditContext& editContext, const juce::UndoManager& undoManager)
    {
        auto admission = RefcountGate::admit (project, clipId, editContext, undoManager);

        if (admission.wasAdmitted() && ClipModel (admission.clip).getKind() != ClipModel::Kind::step)
            admission.refusal = "The clip is an event clip; lanes, cells and a grid belong to a step clip.";

        return admission;
    }

    juce::ValueTree gridOf (const juce::ValueTree& clip)
    {
        return clip.getChildWithName (ids::Grid);
    }

    // The clip's lanes from top to bottom.
    std::vector<juce::ValueTree> lanesOf (const juce::ValueTree& clip)
    {
        return NodeLookup::childrenOfType (gridOf (clip), ids::Lane);
    }

    bool isPositionIn (int index, std::size_t count)
    {
        return index >= 0 && static_cast<std::size_t> (index) < count;
    }

    juce::String whyNotWithin (const juce::String& what, int value, int lowest, int highest)
    {
        if (value < lowest || value > highest)
            return what + " is from " + juce::String (lowest) + " to " + juce::String (highest) + ", not " + juce::String (value) + ".";

        return {};
    }

    // Why the clip has no lane at that position; empty when it has.
    juce::String whyNoLane (const juce::ValueTree& clip, int laneIndex)
    {
        const auto count = lanesOf (clip).size();

        if (! isPositionIn (laneIndex, count))
            return "The clip has no lane at position " + juce::String (laneIndex) + "; it has " + juce::String (static_cast<juce::int64> (count)) + ".";

        return {};
    }

    // Why the clip has no cell there; empty when it has.
    juce::String whyNoCell (const juce::ValueTree& clip, int laneIndex, int column)
    {
        const auto reason = whyNoLane (clip, laneIndex);

        if (reason.isNotEmpty())
            return reason;

        const auto stepCount = GridModel (gridOf (clip)).getStepCount();

        if (column < 0 || column >= stepCount)
            return "The clip has no column " + juce::String (column) + "; its " + juce::String (stepCount) + " columns are 0 to " + juce::String (stepCount - 1) + ".";

        return {};
    }

    // The lane at a position that whyNoLane has found to be there.
    juce::ValueTree laneAt (const juce::ValueTree& clip, int laneIndex)
    {
        return lanesOf (clip)[static_cast<std::size_t> (laneIndex)];
    }

    // The cell at a place that whyNoCell has found to be there. Only Cell
    // children count, as for the wrappers.
    juce::ValueTree cellAt (const juce::ValueTree& clip, int laneIndex, int column)
    {
        return NodeLookup::childrenOfType (laneAt (clip, laneIndex), ids::Cell)[static_cast<std::size_t> (column)];
    }

    juce::ValueTree ccValueOf (const juce::ValueTree& cell, int cc)
    {
        return NodeLookup::childWithNumber (cell, ids::CcValue, ids::cc, cc);
    }

    // Why these are not the values of a cell; empty when they are.
    juce::String whyNoCellValues (const CellValues& values)
    {
        for (const auto& reason : { whyNotWithin ("A pitch", values.pitch, 0, 127), whyNotWithin ("A velocity", values.velocity, 1, 127) })
            if (reason.isNotEmpty())
                return reason;

        if (! std::isfinite (values.gate))
            return "A gate is a number greater than 0 and at most 4.";

        // The check is on the value the saved text reads back as [D14]: the
        // smallest numbers a double holds read back as 0, and 4.0000001 as 4.
        const auto roundedGate = CanonicalDouble::rounded (values.gate);

        if (! (roundedGate > 0.0 && roundedGate <= 4.0))
            return "A gate is greater than 0 and at most 4, not " + CanonicalDouble::textOf (values.gate) + ".";

        return {};
    }

    // The columns that take time in one pass of the loop: those before the
    // reset that are not skipped.
    int audibleColumnsOf (const juce::String& skipMask, int resetAt)
    {
        int audibleColumns = 0;

        for (int column = 0; column < resetAt; ++column)
            if (skipMask[column] != '1')
                ++audibleColumns;

        return audibleColumns;
    }

    const char* const noColumnLeft = "No column would be left to play: every column before the reset would be skipped.";

    // After a change to the grid: every placement of the changed clip —
    // after a clone, those of the clone — keeps its offset inside the loop
    // (invariant 4).
    void bringOffsetsIntoLoop (juce::ValueTree project, const juce::ValueTree& clip, juce::UndoManager& undoManager)
    {
        const ClipModel changed (clip);

        PlacementOffsets::bringIntoLoop (project, changed.getId(), changed.getLoopLength(), undoManager);
    }

} // namespace

CommandResult addLane (juce::ValueTree project, const juce::String& clipId, const juce::String& name, std::optional<int> fixedPitch, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToStepClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    if (fixedPitch.has_value())
    {
        const auto reason = whyNotWithin ("A fixed pitch", *fixedPitch, 0, 127);

        if (reason.isNotEmpty())
            return CommandResult::refused (reason);
    }

    const auto id = juce::Uuid().toString();

    // Built apart and added whole, so that undo knows one action.
    juce::ValueTree lane (ids::Lane);
    lane.setProperty (ids::id, id, nullptr);
    lane.setProperty (ids::name, name, nullptr);

    if (fixedPitch.has_value())
        lane.setProperty (ids::fixedPitch, *fixedPitch, nullptr);

    // A Cell that stores nothing is a rest.
    for (auto cellsToAdd = GridModel (gridOf (admission.clip)).getStepCount(); cellsToAdd > 0; --cellsToAdd)
        lane.appendChild (juce::ValueTree (ids::Cell), nullptr);

    auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    gridOf (clip).appendChild (lane, &undoManager);

    return CommandResult::appliedAndCreated (id);
}

CommandResult removeLane (juce::ValueTree project, const juce::String& clipId, int laneIndex, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToStepClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    const auto reason = whyNoLane (admission.clip, laneIndex);

    if (reason.isNotEmpty())
        return CommandResult::refused (reason);

    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    gridOf (clip).removeChild (laneAt (clip, laneIndex), &undoManager);

    return CommandResult::applied();
}

CommandResult moveLane (juce::ValueTree project, const juce::String& clipId, int laneIndex, int toPosition, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToStepClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    const auto reason = whyNoLane (admission.clip, laneIndex);

    if (reason.isNotEmpty())
        return CommandResult::refused (reason);

    const auto lanesNow = lanesOf (admission.clip);
    const auto count = static_cast<int> (lanesNow.size());

    if (toPosition < 0 || toPosition >= count)
        return CommandResult::refused ("There are " + juce::String (count) + " lanes, so a position is 0 to " + juce::String (count - 1) + ", not " + juce::String (toPosition) + ".");

    if (toPosition == laneIndex)
        return CommandResult::applied();

    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    auto grid = gridOf (clip);

    // Taking the place of the lane that is there now puts this one behind
    // it when it comes from above and before it when it comes from below:
    // at that position either way.
    grid.moveChild (grid.indexOf (laneAt (clip, laneIndex)), grid.indexOf (laneAt (clip, toPosition)), &undoManager);

    return CommandResult::applied();
}

CommandResult setLaneName (juce::ValueTree project, const juce::String& clipId, int laneIndex, const juce::String& name, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToStepClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    const auto reason = whyNoLane (admission.clip, laneIndex);

    if (reason.isNotEmpty())
        return CommandResult::refused (reason);

    if (LaneModel (laneAt (admission.clip, laneIndex)).getName() == name)
        return CommandResult::applied();

    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    laneAt (clip, laneIndex).setProperty (ids::name, name, &undoManager);

    return CommandResult::applied();
}

CommandResult setLaneFixedPitch (juce::ValueTree project, const juce::String& clipId, int laneIndex, std::optional<int> fixedPitch, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToStepClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    auto reason = whyNoLane (admission.clip, laneIndex);

    if (reason.isEmpty() && fixedPitch.has_value())
        reason = whyNotWithin ("A fixed pitch", *fixedPitch, 0, 127);

    if (reason.isNotEmpty())
        return CommandResult::refused (reason);

    if (LaneModel (laneAt (admission.clip, laneIndex)).getFixedPitch() == fixedPitch)
        return CommandResult::applied();

    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    auto lane = laneAt (clip, laneIndex);

    if (fixedPitch.has_value())
    {
        lane.setProperty (ids::fixedPitch, *fixedPitch, &undoManager);
        return CommandResult::applied();
    }

    // The property has no default: a lane without it is a melodic row, so
    // it has to go. Undo of a removed property puts it back behind the
    // node's other properties (juce_ValueTree.cpp, SetPropertyAction), which
    // is not where it stood, and undo is held to bring back the document as
    // it was to the letter. So the lane gives way to a copy of itself
    // without the property, and undo puts the lane itself back. A handle to
    // the lane, or into it, is stale after this command.
    auto melodicLane = lane.createCopy();
    melodicLane.removeProperty (ids::fixedPitch, nullptr);

    auto grid = gridOf (clip);
    const auto position = grid.indexOf (lane);
    grid.removeChild (position, &undoManager);
    grid.addChild (melodicLane, position, &undoManager);

    return CommandResult::applied();
}

CommandResult setCell (juce::ValueTree project, const juce::String& clipId, int laneIndex, int column, const CellValues& values, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToStepClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    auto reason = whyNoCell (admission.clip, laneIndex, column);

    if (reason.isEmpty())
        reason = whyNoCellValues (values);

    if (reason.isNotEmpty())
        return CommandResult::refused (reason);

    // What is stored is the text that is saved, six significant digits and
    // no more [D14], and the cell is compared with the value it reads as.
    const auto gateText = CanonicalDouble::textOf (values.gate);
    const auto gate = CanonicalDouble::rounded (values.gate);

    const CellModel cellNow (cellAt (admission.clip, laneIndex, column));
    const auto hasPitch = cellNow.getPitch() == values.pitch;
    const auto hasOn = cellNow.isOn() == values.on;
    const auto hasVelocity = cellNow.getVelocity() == values.velocity;
    const auto hasGate = juce::exactlyEqual (cellNow.getGate(), gate);

    if (hasPitch && hasOn && hasVelocity && hasGate)
        return CommandResult::applied();

    // A property is written only where the value differs from what the cell
    // reads as, so that undo holds what changed and a default that was
    // never stored stays unstored. The clone reads as the original does.
    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    auto cell = cellAt (clip, laneIndex, column);

    if (! hasPitch)
        cell.setProperty (ids::pitch, values.pitch, &undoManager);

    if (! hasOn)
        cell.setProperty (ids::on, values.on ? 1 : 0, &undoManager);

    if (! hasVelocity)
        cell.setProperty (ids::velocity, values.velocity, &undoManager);

    if (! hasGate)
        cell.setProperty (ids::gate, gateText, &undoManager);

    return CommandResult::applied();
}

CommandResult setCellCc (juce::ValueTree project, const juce::String& clipId, int laneIndex, int column, int cc, int value, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToStepClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    auto reason = whyNoCell (admission.clip, laneIndex, column);

    if (reason.isEmpty())
        reason = whyNotWithin ("A controller number", cc, 0, 127);

    if (reason.isEmpty())
        reason = whyNotWithin ("A controller value", value, 0, 127);

    if (reason.isNotEmpty())
        return CommandResult::refused (reason);

    const auto ccValueNow = ccValueOf (cellAt (admission.clip, laneIndex, column), cc);

    if (ccValueNow.isValid() && CcValueModel (ccValueNow).getValue() == value)
        return CommandResult::applied();

    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    auto cell = cellAt (clip, laneIndex, column);
    auto ccValue = ccValueOf (cell, cc);

    if (ccValue.isValid())
    {
        ccValue.setProperty (ids::value, value, &undoManager);
        return CommandResult::applied();
    }

    juce::ValueTree added (ids::CcValue);
    added.setProperty (ids::cc, cc, nullptr);
    added.setProperty (ids::value, value, nullptr);

    cell.appendChild (added, &undoManager);

    return CommandResult::applied();
}

CommandResult removeCellCc (juce::ValueTree project, const juce::String& clipId, int laneIndex, int column, int cc, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToStepClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    const auto reason = whyNoCell (admission.clip, laneIndex, column);

    if (reason.isNotEmpty())
        return CommandResult::refused (reason);

    if (! ccValueOf (cellAt (admission.clip, laneIndex, column), cc).isValid())
        return CommandResult::refused ("The cell has no value for controller " + juce::String (cc) + ".");

    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    auto cell = cellAt (clip, laneIndex, column);
    cell.removeChild (ccValueOf (cell, cc), &undoManager);

    return CommandResult::applied();
}

CommandResult setStepLength (juce::ValueTree project, const juce::String& clipId, std::int64_t stepLength, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToStepClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    if (stepLength < 1 || stepLength > longestStep)
        return CommandResult::refused ("A step is 1 to " + juce::String (longestStep) + " ticks long, not " + juce::String (stepLength) + ".");

    if (GridModel (gridOf (admission.clip)).getStepLength() == stepLength)
        return CommandResult::applied();

    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    gridOf (clip).setProperty (ids::stepLength, static_cast<juce::int64> (stepLength), &undoManager);

    bringOffsetsIntoLoop (project, clip, undoManager);

    return CommandResult::applied();
}

CommandResult setStepCount (juce::ValueTree project, const juce::String& clipId, int stepCount, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToStepClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    if (stepCount < 1 || stepCount > mostColumns)
        return CommandResult::refused ("A grid has 1 to " + juce::String (mostColumns) + " columns, not " + juce::String (stepCount) + ".");

    const GridModel gridNow (gridOf (admission.clip));
    const auto stepCountNow = gridNow.getStepCount();

    if (stepCountNow == stepCount)
        return CommandResult::applied();

    // A reset at the end follows the end; any other is kept, or comes to
    // the new end of a grid that is now shorter (docs/SCHEMA.md -> Changing
    // stepCount).
    const auto resetAtNow = gridNow.getResetAt();
    const auto resetAt = resetAtNow == stepCountNow ? stepCount : std::min (resetAtNow, stepCount);

    const auto skipMaskNow = gridNow.getSkipMask();
    const auto skipMask = stepCount > stepCountNow ? skipMaskNow + juce::String::repeatedString ("0", stepCount - stepCountNow) : skipMaskNow.substring (0, stepCount);

    if (audibleColumnsOf (skipMask, resetAt) < 1)
        return CommandResult::refused (noColumnLeft);

    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    auto grid = gridOf (clip);

    grid.setProperty (ids::stepCount, stepCount, &undoManager);

    // A grid that stores no reset, or no skips, has the default of its step
    // count, and has it with the new count as well.
    if (grid.hasProperty (ids::resetAt) && resetAt != resetAtNow)
        grid.setProperty (ids::resetAt, resetAt, &undoManager);

    if (grid.hasProperty (ids::skipMask))
        grid.setProperty (ids::skipMask, skipMask, &undoManager);

    for (auto lane : lanesOf (clip))
    {
        // A Cell that stores nothing is a rest.
        for (auto column = stepCountNow; column < stepCount; ++column)
            lane.appendChild (juce::ValueTree (ids::Cell), &undoManager);

        const auto cells = NodeLookup::childrenOfType (lane, ids::Cell);

        for (auto column = stepCount; column < stepCountNow; ++column)
            lane.removeChild (cells[static_cast<std::size_t> (column)], &undoManager);
    }

    bringOffsetsIntoLoop (project, clip, undoManager);

    return CommandResult::applied();
}

CommandResult setResetAt (juce::ValueTree project, const juce::String& clipId, int resetAt, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToStepClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    const GridModel gridNow (gridOf (admission.clip));
    const auto stepCount = gridNow.getStepCount();

    if (resetAt < 1 || resetAt > stepCount)
        return CommandResult::refused ("The reset is from 1 to the step count of " + juce::String (stepCount) + ", not " + juce::String (resetAt) + ".");

    if (gridNow.getResetAt() == resetAt)
        return CommandResult::applied();

    if (audibleColumnsOf (gridNow.getSkipMask(), resetAt) < 1)
        return CommandResult::refused (noColumnLeft);

    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    gridOf (clip).setProperty (ids::resetAt, resetAt, &undoManager);

    bringOffsetsIntoLoop (project, clip, undoManager);

    return CommandResult::applied();
}

CommandResult setColumnSkipped (juce::ValueTree project, const juce::String& clipId, int column, bool isSkipped, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToStepClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    const GridModel gridNow (gridOf (admission.clip));
    const auto stepCount = gridNow.getStepCount();

    if (column < 0 || column >= stepCount)
        return CommandResult::refused ("The clip has no column " + juce::String (column) + "; its " + juce::String (stepCount) + " columns are 0 to " + juce::String (stepCount - 1) + ".");

    if (gridNow.isColumnSkipped (column) == isSkipped)
        return CommandResult::applied();

    const auto skipMaskNow = gridNow.getSkipMask();
    const auto skipMask = skipMaskNow.substring (0, column) + (isSkipped ? "1" : "0") + skipMaskNow.substring (column + 1);

    if (audibleColumnsOf (skipMask, gridNow.getResetAt()) < 1)
        return CommandResult::refused (noColumnLeft);

    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    gridOf (clip).setProperty (ids::skipMask, skipMask, &undoManager);

    bringOffsetsIntoLoop (project, clip, undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
