#include "model/SceneModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

SceneModel::SceneModel (juce::ValueTree sceneNode)
    : node (std::move (sceneNode))
{
    jassert (node.hasType (ids::Scene));
}

int SceneModel::getIndex() const
{
    return PropertyReader::getRequiredInt (node, ids::index);
}

juce::String SceneModel::getName() const
{
    return PropertyReader::getRequiredString (node, ids::name);
}

} // namespace model
