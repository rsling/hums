#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model
{

// A tempo on the Timeline (docs/SCHEMA.md -> Timeline). bpm is the tempo
// as entered; what is played and exported is TempoMap's business [D13].
class TempoEventModel
{
public:
    // The node must be a TempoEvent node.
    explicit TempoEventModel (juce::ValueTree tempoEventNode);

    std::int64_t getTick() const;
    double getBpm() const;

private:
    juce::ValueTree node;
};

} // namespace model
