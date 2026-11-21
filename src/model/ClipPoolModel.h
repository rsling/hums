#pragma once

#include "model/ClipModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <optional>
#include <vector>

namespace model
{

// Where all musical content lives (docs/SCHEMA.md -> ClipPool / Clip).
// Tracks reference its clips by id; a clip can be in the pool with nothing
// referencing it.
class ClipPoolModel
{
public:
    // The node must be a ClipPool node.
    explicit ClipPoolModel (juce::ValueTree clipPoolNode);

    // In document order.
    std::vector<ClipModel> getClips() const;

    // The clip with this id, or nothing if the pool has none.
    std::optional<ClipModel> findClip (const juce::String& clipId) const;

private:
    juce::ValueTree node;
};

} // namespace model
