#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <string>
#include <vector>

namespace guardrails
{

// What the property tests hold of every document they look at, whether the
// generator has just built it or a command has just changed it
// (docs/GUARDRAILS.md -> 3):
//
//   - it passes the Validator: all twelve invariants of docs/SCHEMA.md;
//   - every node type and property name in the undoable subtree, which is
//     everything but UiState, is declared in model/Ids.h [D15];
//   - canonical save -> load -> canonical save is byte-identical, through the
//     real load path.
//
// The checks give their findings back as text and assert nothing themselves,
// so that a test can also show each of them finding what it is there to find.
class DocumentChecks
{
public:
    // Everything that is wrong with the document, one line of text per
    // finding; empty when all three checks hold. The document is not changed.
    static std::vector<std::string> failuresOf (const juce::ValueTree& project);

    // The three checks one by one.
    static std::vector<std::string> invariantFailuresOf (const juce::ValueTree& project);
    static std::vector<std::string> undeclaredNamesIn (const juce::ValueTree& project);
    static std::vector<std::string> saveLoadSaveFailuresOf (const juce::ValueTree& project);
};

} // namespace guardrails
