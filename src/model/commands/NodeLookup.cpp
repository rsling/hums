#include "model/commands/NodeLookup.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

namespace model::commands
{

std::vector<juce::ValueTree> NodeLookup::childrenOfType (const juce::ValueTree& parent, const juce::Identifier& type)
{
    std::vector<juce::ValueTree> children;

    for (const auto& child : parent)
        if (child.hasType (type))
            children.push_back (child);

    return children;
}

juce::ValueTree NodeLookup::childWithId (const juce::ValueTree& parent, const juce::Identifier& type, const juce::String& id)
{
    for (const auto& child : parent)
        if (child.hasType (type) && PropertyReader::getString (child, ids::id, "") == id)
            return child;

    return {};
}

juce::ValueTree NodeLookup::childWithNumber (const juce::ValueTree& parent, const juce::Identifier& type, const juce::Identifier& property, std::int64_t value)
{
    for (const auto& child : parent)
    {
        if (! child.hasType (type))
            continue;

        const auto stored = PropertyReader::readInt64 (child, property);

        if (stored.isValid() && stored.value == value)
            return child;
    }

    return {};
}

juce::ValueTree NodeLookup::placementWithId (const juce::ValueTree& project, const juce::String& placementId)
{
    jassert (project.hasType (ids::Project));

    for (const auto& track : childrenOfType (project.getChildWithName (ids::Tracks), ids::Track))
    {
        const auto own = childWithId (track.getChildWithName (ids::Placements), ids::Placement, placementId);

        if (own.isValid())
            return own;

        for (const auto& lane : childrenOfType (track.getChildWithName (ids::AutomationLanes), ids::AutomationLane))
        {
            const auto onLane = childWithId (lane, ids::Placement, placementId);

            if (onLane.isValid())
                return onLane;
        }
    }

    return {};
}

} // namespace model::commands
