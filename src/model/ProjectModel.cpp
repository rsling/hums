#include "model/ProjectModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

ProjectModel::ProjectModel (juce::ValueTree projectNode)
    : node (std::move (projectNode))
{
    jassert (node.hasType (ids::Project));
}

int ProjectModel::getSchemaVersion() const
{
    return PropertyReader::getRequiredInt (node, ids::schemaVersion);
}

juce::String ProjectModel::getName() const
{
    return PropertyReader::getString (node, ids::name, "");
}

int ProjectModel::getPpq() const
{
    return PropertyReader::getInt (node, ids::ppq, 960);
}

RoutingModel ProjectModel::getRouting() const
{
    return RoutingModel (node.getChildWithName (ids::Routing));
}

TimelineModel ProjectModel::getTimeline() const
{
    return TimelineModel (node.getChildWithName (ids::Timeline));
}

SessionModel ProjectModel::getSession() const
{
    return SessionModel (node.getChildWithName (ids::Session));
}

} // namespace model
