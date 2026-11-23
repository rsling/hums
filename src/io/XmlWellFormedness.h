#pragma once

#include <juce_core/juce_core.h>

namespace io
{

// The part of "is this well-formed XML" that JUCE's parser does not ask
// (docs/SCHEMA.md -> Loading, Parse). juce::XmlDocument reads a closing tag
// without looking at its name or at whether it is complete, and stops reading
// at the end of the root node, whatever follows. A project file with such an
// error would load, and what the parser passed over would be gone at the next
// save. The load path therefore asks here first, and JUCE's parser after.
//
// This is no second parser: it makes nothing of the text and checks neither
// names nor entities nor values, which JUCE's parser and the Validator do. It
// follows the nodes as they open and close, and that is all.
class XmlWellFormedness
{
public:
    struct Result
    {
        enum class Kind
        {
            wellFormed,    // as far as this check looks
            notWellFormed, // a tag that is not complete, a closing tag of another name, something before or after the root
            hasDoctype     // well-formed, but no project file has one (docs/SCHEMA.md -> Loading)
        };

        Kind kind = Kind::wellFormed;

        // What is wrong, as the end of a sentence ("the Project node is
        // closed as Projekt"). Empty when nothing is.
        juce::String reason;
    };

    // Looks for, and reports the first of:
    //  - a DOCTYPE before the root node;
    //  - anything but whitespace, comments and processing instructions
    //    before the root node or after it, a second root included;
    //  - an opening tag that is not complete, or an attribute without a
    //    quoted value;
    //  - a closing tag whose name is not the name of the node it closes, or
    //    that lacks its ">";
    //  - a comment, a processing instruction, a CDATA section or a node that
    //    is still open where the text ends.
    static Result check (const juce::String& xmlText);
};

} // namespace io
