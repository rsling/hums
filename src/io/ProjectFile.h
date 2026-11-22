#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace io
{

// The native project file: the document as XML (docs/SCHEMA.md -> Loading,
// saving, canonical form).
//
// So far it holds the canonical writer. Loading and the atomic save follow.
class ProjectFile
{
public:
    // The document as canonical XML text, the only form Hums saves. Two
    // documents that read the same through the typed wrappers give the same
    // text, whatever their properties are held as (a number set in this
    // session, or its text from a loaded file) and in whatever order they
    // were set.
    //
    //   - one node per line, children indented by two spaces, in stored order;
    //     "\n" line endings; the text is ASCII, every other character is
    //     written as a numeric character reference
    //   - a node's properties in the order of model/Ids.h; a property Ids.h
    //     does not declare (data from a newer build) comes after them, in
    //     stored order
    //   - a property that holds the schema's default is left out
    //   - integers as plain decimal, doubles as juce::String (double) writes
    //     them (six significant digits [D14]), booleans as 0 / 1
    //   - unknown properties and unknown children are written as they are
    //     stored
    //
    // The node must be a Project node, and the document a valid one: the
    // writer does not check it. A value that is not in the text form of its
    // type is written as stored, never repaired; refusing it is the
    // Validator's work.
    static juce::String toCanonicalXml (const juce::ValueTree& project);
};

} // namespace io
