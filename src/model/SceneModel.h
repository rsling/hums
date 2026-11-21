#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace model
{

// The name of one session row (docs/SCHEMA.md -> Session). Scenes are
// sparse: a row without a Scene node simply has no name.
class SceneModel
{
public:
    // The node must be a Scene node.
    explicit SceneModel (juce::ValueTree sceneNode);

    // Row index, >= 0, unique.
    int getIndex() const;
    juce::String getName() const;

private:
    juce::ValueTree node;
};

} // namespace model
