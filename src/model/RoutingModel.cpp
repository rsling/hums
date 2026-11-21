#include "model/RoutingModel.h"

#include "model/Ids.h"

#include <utility>

namespace model
{

RoutingModel::RoutingModel (juce::ValueTree routingNode)
    : node (std::move (routingNode))
{
    jassert (node.hasType (ids::Routing));
}

std::vector<PerformerModel> RoutingModel::getPerformers() const
{
    std::vector<PerformerModel> performers;

    for (const auto& child : node)
        if (child.hasType (ids::Performer))
            performers.emplace_back (child);

    return performers;
}

std::vector<PlayerModel> RoutingModel::getPlayers() const
{
    std::vector<PlayerModel> players;

    for (const auto& child : node)
        if (child.hasType (ids::Player))
            players.emplace_back (child);

    return players;
}

PadMapModel RoutingModel::getPadMap() const
{
    return PadMapModel (node.getChildWithName (ids::PadMap));
}

} // namespace model
