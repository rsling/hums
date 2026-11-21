#include "model/CellModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

CellModel::CellModel (juce::ValueTree cellNode)
    : node (std::move (cellNode))
{
    jassert (node.hasType (ids::Cell));
}

int CellModel::getPitch() const
{
    return PropertyReader::getInt (node, ids::pitch, 60);
}

bool CellModel::isOn() const
{
    return PropertyReader::getBool (node, ids::on, false);
}

int CellModel::getVelocity() const
{
    return PropertyReader::getInt (node, ids::velocity, 100);
}

double CellModel::getGate() const
{
    return PropertyReader::getDouble (node, ids::gate, 0.5);
}

std::vector<CcValueModel> CellModel::getCcValues() const
{
    std::vector<CcValueModel> ccValues;

    for (const auto& child : node)
        if (child.hasType (ids::CcValue))
            ccValues.emplace_back (child);

    return ccValues;
}

} // namespace model
