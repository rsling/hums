#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model
{

// A time signature on the Timeline (docs/SCHEMA.md -> Timeline).
class TimeSigEventModel
{
public:
    // The node must be a TimeSigEvent node.
    explicit TimeSigEventModel (juce::ValueTree timeSigEventNode);

    std::int64_t getTick() const;
    // 1-32.
    int getNumerator() const;
    // A power of two, 1-64.
    int getDenominator() const;

private:
    juce::ValueTree node;
};

} // namespace model
