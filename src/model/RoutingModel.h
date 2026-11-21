#pragma once

#include "model/PadMapModel.h"
#include "model/PerformerModel.h"
#include "model/PlayerModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace model
{

// The logical ports and the pad map (docs/SCHEMA.md -> Routing). Binding
// affects transmission only, never rendering.
class RoutingModel
{
public:
    // The node must be a Routing node.
    explicit RoutingModel (juce::ValueTree routingNode);

    // In document order.
    std::vector<PerformerModel> getPerformers() const;
    // In document order.
    std::vector<PlayerModel> getPlayers() const;
    PadMapModel getPadMap() const;

private:
    juce::ValueTree node;
};

} // namespace model
