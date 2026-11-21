#include "model/PadMapModel.h"

#include "model/Ids.h"

#include <utility>

namespace model
{

PadMapModel::PadMapModel (juce::ValueTree padMapNode)
    : node (std::move (padMapNode))
{
    jassert (node.hasType (ids::PadMap));
}

std::vector<PadModel> PadMapModel::getPads() const
{
    std::vector<PadModel> pads;

    for (const auto& child : node)
        if (child.hasType (ids::Pad))
            pads.emplace_back (child);

    return pads;
}

} // namespace model
