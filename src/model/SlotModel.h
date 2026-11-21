#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace model
{

// A launchable clip reference on a Track, for session play (docs/SCHEMA.md
// -> Slots / Slot). Slots are sparse: only occupied ones are stored. A Slot
// counts towards its clip's refcount, never towards the arrangement end.
class SlotModel
{
public:
    // The node must be a Slot node.
    explicit SlotModel (juce::ValueTree slotNode);

    // Row index, >= 0, unique within the track. An index, not an id: the
    // scene commands renumber it.
    int getScene() const;
    juce::String getClipId() const;

private:
    juce::ValueTree node;
};

} // namespace model
