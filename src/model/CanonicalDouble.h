#pragma once

#include <juce_core/juce_core.h>

namespace model
{

// A double as the document holds it (docs/SCHEMA.md -> Doubles, [D14]): six
// significant digits, no more, because that is what the canonical writer
// saves.
//
// A command that stores a double (TempoEvent.bpm, Cell.gate) stores textOf()
// of it, the text, never the number. So the precision is given up at the
// edit, where undo can see it, and the document holds in this session exactly
// what a loaded file holds. A juce::var that holds a double would give
// model/PropertyReader, which reads every value through its text, a text of
// fifteen digits, and for a few values that is another number than the one
// stored.
class CanonicalDouble
{
public:
    // The text the canonical writer saves for this value: juce::String
    // (double) in the pinned JUCE ("118.123", "0.333333", "1e-05"). The value
    // must be finite.
    static juce::String textOf (double value);

    // The value that text reads back as.
    static double rounded (double value);
};

} // namespace model
