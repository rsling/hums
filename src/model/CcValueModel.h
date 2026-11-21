#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace model
{

// One CC value of a Cell (docs/SCHEMA.md -> Step clip: Grid): the 960's
// second and third rows. Sent at the cell's start, also when the cell is off.
class CcValueModel
{
public:
    // The node must be a CcValue node.
    explicit CcValueModel (juce::ValueTree ccValueNode);

    // Controller number, unique within the cell.
    int getCc() const;
    int getValue() const;

private:
    juce::ValueTree node;
};

} // namespace model
