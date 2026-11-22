#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace guardrails
{

// Builds random valid documents for the property tests (docs/GUARDRAILS.md ->
// 3): a few tracks, clips of both kinds, placements, lanes, slots and
// automation lanes, with every node type and every property of
// docs/SCHEMA.md somewhere among them.
//
// Handwritten, without a property-testing library (docs/DECISIONS.md, Q5).
//
// A document is made from its seed and from nothing else: the ids are drawn
// from the same random numbers and not from juce::Uuid, so the same seed
// gives the same document, byte for byte, on every run.
//
// What it builds is what the schema allows, not only what an editor of Hums
// would make: content past the loop length, a placement shorter than its
// clip's loop, a grid of no lanes, children in an order that is not the order
// of time, a property that holds its default written out or left out, a value
// held as a number or as its text.
class DocumentGenerator
{
public:
    // A Project node that passes the Validator.
    static juce::ValueTree generate (std::uint64_t seed);
};

} // namespace guardrails
