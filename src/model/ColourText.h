#pragma once

#include <juce_core/juce_core.h>

namespace model
{

// A colour as the document holds it (docs/SCHEMA.md -> Conventions): the
// text "#AARRGGBB", or an empty text for no colour. The one place that says
// what such a text looks like, for the Validator and for every command that
// stores a colour.
class ColourText
{
public:
    // True for '#' and eight hexadecimal digits in either case, and for the
    // empty text.
    static bool isValid (const juce::String& text);
};

} // namespace model
