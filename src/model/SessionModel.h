#pragma once

#include "model/SceneModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>
#include <vector>

namespace model
{

// Session (live play) settings and scene names (docs/SCHEMA.md -> Session).
// Which slot is playing is engine state and never in the document.
class SessionModel
{
public:
    // The node must be a Session node.
    explicit SessionModel (juce::ValueTree sessionNode);

    // The launch grid in ticks, measured from tick 0; 0 = immediate.
    std::int64_t getLaunchQuantise() const;

    // Only the rows that have a Scene node, in document order.
    std::vector<SceneModel> getScenes() const;

private:
    juce::ValueTree node;
};

} // namespace model
