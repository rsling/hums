#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace model
{

// Checks a document against the invariants of docs/SCHEMA.md -> Invariants.
// It runs on every loaded document before the document is installed and, in
// tests, after every command.
//
// It never repairs and never guesses [D17]: it only reports. It reads the
// tree directly and not through the typed wrappers, because those may assume
// a document that has already passed here.
//
// All twelve invariants are checked, and with them the tables of
// docs/SCHEMA.md -> Node reference: a property the schema lists without a
// default is there, and a property that is there is in the text form of its
// type and inside its range or its list. A document without violations is a
// valid document.
//
// What it looks at is the tree of docs/SCHEMA.md -> Tree. A node of an
// unknown type, and a known type in a place where the schema does not have
// it, is passed over like any other unknown child.
class Validator
{
public:
    struct Violation
    {
        // Where: the node types from the root down, separated by '/'. A type
        // that can occur several times in its parent carries its position
        // among its siblings of that type, counted from 0:
        //     Project/Tracks/Track[0]/Placements/Placement[1]
        juce::String nodePath;

        // The property the violation is about, or empty when it is about the
        // node as a whole (a missing child, a wrong number of cells).
        juce::String property;

        // What is wrong, in plain words.
        juce::String reason;
    };

    // Every violation found, in no particular order of importance; empty
    // when none was found. The tree is not changed.
    static std::vector<Violation> validate (const juce::ValueTree& project);
};

} // namespace model
