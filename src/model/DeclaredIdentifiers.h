#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace model
{

// Every identifier of model/Ids.h as a list that code can walk, in the order
// of its declarations there.
//
// Ids.h gives that order only by the order of its lines, and it cannot hold
// the list itself: guardrail 1 allows nothing in it but declarations. So the
// list is written out here a second time, and the test of this class reads
// Ids.h and fails when the two differ in content or in order. An identifier
// added to Ids.h is added here on the same line of the list.
//
// Two things stand on it: the canonical writer emits a node's properties in
// this order (docs/SCHEMA.md -> Canonical form), and guardrail 3 asks
// whether a name in the document is a declared one [D15].
class DeclaredIdentifiers
{
public:
    // Node types first, then properties, then view state: Ids.h from top to
    // bottom.
    static const std::vector<juce::Identifier>& inDeclarationOrder();

    // Where the identifier stands in that list, counted from 0, or -1 for a
    // name Ids.h does not declare.
    static int positionOf (const juce::Identifier& identifier);

    static bool isDeclared (const juce::Identifier& identifier);
};

} // namespace model
