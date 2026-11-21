#pragma once

#include "model/CcValueModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace model
{

// One step in one Lane of a step clip (docs/SCHEMA.md -> Step clip: Grid).
// A Cell does not know its column: that is its position among the Cell
// children of its Lane.
class CellModel
{
public:
    // The node must be a Cell node.
    explicit CellModel (juce::ValueTree cellNode);

    // Ignored when the Lane has a fixed pitch.
    int getPitch() const;
    // An off cell is a rest: time passes, no note plays, its CC values are still sent.
    bool isOn() const;
    int getVelocity() const;
    // Note length as a fraction of the Grid's step length; above 1 the note
    // reaches into the following cells.
    double getGate() const;

    // In document order; at most one per controller number.
    std::vector<CcValueModel> getCcValues() const;

private:
    juce::ValueTree node;
};

} // namespace model
