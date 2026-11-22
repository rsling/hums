#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>
#include <vector>

namespace model::commands
{

// How the commands find the node they are to change. A node that is not
// there comes back as an invalid juce::ValueTree, and the command refuses.
//
// Like the typed wrappers, this reads a document that has passed the
// Validator: values are read through their text (model/PropertyReader), and
// the containers the schema requires are there.
class NodeLookup
{
public:
    // The children of one type, in document order.
    static std::vector<juce::ValueTree> childrenOfType (const juce::ValueTree& parent, const juce::Identifier& type);

    // The child of that type whose `id` is this text.
    static juce::ValueTree childWithId (const juce::ValueTree& parent, const juce::Identifier& type, const juce::String& id);

    // The child of that type whose whole-number property has this value (a
    // Pad by its index, a TempoEvent by its tick). A child without the
    // property takes no part.
    static juce::ValueTree childWithNumber (const juce::ValueTree& parent, const juce::Identifier& type, const juce::Identifier& property, std::int64_t value);
};

} // namespace model::commands
