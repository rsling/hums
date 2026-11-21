#pragma once

#include "model/CellModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <optional>
#include <vector>

namespace model
{

// A monophonic row of Cells in a step clip (docs/SCHEMA.md -> Step clip:
// Grid). With a fixed pitch it is a drum row, without one a melodic row;
// nothing else tells the two apart.
class LaneModel
{
public:
    // The node must be a Lane node.
    explicit LaneModel (juce::ValueTree laneNode);

    juce::String getId() const;
    // The drum view's row label; empty means "use the PadMap label".
    juce::String getName() const;
    // The pitch every cell of a drum row plays; empty for a melodic row,
    // whose cells play their own pitch.
    std::optional<int> getFixedPitch() const;

    // In column order: the column of a cell is its index in this list. Only
    // Cell children are listed, so a child this build has no name for cannot
    // shift the columns.
    std::vector<CellModel> getCells() const;

private:
    juce::ValueTree node;
};

} // namespace model
