#pragma once

#include "model/LaneModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>
#include <vector>

namespace model
{

// The content of a step clip: Columns x monophonic Lanes (docs/SCHEMA.md ->
// Step clip: Grid). Columns are not nodes; a column is an index
// 0 ... stepCount - 1, and what belongs to a whole column (skip, reset) is
// held here.
class GridModel
{
public:
    // The node must be a Grid node.
    explicit GridModel (juce::ValueTree gridNode);

    // Ticks per column.
    std::int64_t getStepLength() const;
    // Number of columns.
    int getStepCount() const;
    // The loop ends before this column; equal to the step count when the
    // whole grid plays. Columns from here on keep their data.
    int getResetAt() const;
    // One character per column, '1' = the column is skipped. All '0' when
    // nothing is stored.
    juce::String getSkipMask() const;

    // A skipped column takes no time. The column must be below the step count.
    bool isColumnSkipped (int column) const;
    // The columns that take time in one pass of the loop: those before
    // resetAt that are not skipped.
    int getAudibleColumnCount() const;
    // Derived, never stored: step length x audible columns.
    std::int64_t getLoopLength() const;

    // Top to bottom; the order is part of the document.
    std::vector<LaneModel> getLanes() const;

private:
    juce::ValueTree node;
};

} // namespace model
