#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace model
{

// The name of a clip that is made as a copy of another [D20]: the name of
// the original with " (copy n)" behind it, n counted from 1. The one place
// that says what such a name looks like.
//
// The suffix is replaced, never stacked: a copy of "Bass (copy 1)" is not
// "Bass (copy 1) (copy 1)" but the next free "Bass (copy n)". n is the
// smallest number for which no clip of the pool has that name yet. A clip
// without a name gives "(copy n)".
class CopyName
{
public:
    // The name for a copy of a clip called `originalName`, to be added to
    // `clipPool` (the ClipPool node).
    static juce::String forACopyOf (const juce::String& originalName, const juce::ValueTree& clipPool);

    // The name without a " (copy n)" at its end; the name itself when it
    // has none.
    static juce::String withoutSuffix (const juce::String& name);
};

} // namespace model
