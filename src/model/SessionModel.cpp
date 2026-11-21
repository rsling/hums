#include "model/SessionModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

SessionModel::SessionModel (juce::ValueTree sessionNode)
    : node (std::move (sessionNode))
{
    jassert (node.hasType (ids::Session));
}

std::int64_t SessionModel::getLaunchQuantise() const
{
    return PropertyReader::getInt64 (node, ids::launchQuantise, 3840);
}

std::vector<SceneModel> SessionModel::getScenes() const
{
    std::vector<SceneModel> scenes;

    for (const auto& child : node)
        if (child.hasType (ids::Scene))
            scenes.emplace_back (child);

    return scenes;
}

} // namespace model
