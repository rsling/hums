#pragma once

#include "model/PadModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace model
{

// The project's one pad map, used by the drum view and by the pad Performer
// (docs/SCHEMA.md -> Routing). Input configuration: remapping a pad never
// rewrites clips.
class PadMapModel
{
public:
    // The node must be a PadMap node.
    explicit PadMapModel (juce::ValueTree padMapNode);

    // In document order, which is not necessarily index order.
    std::vector<PadModel> getPads() const;

private:
    juce::ValueTree node;
};

} // namespace model
