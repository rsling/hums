#include "model/EventModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

EventModel::EventModel (juce::ValueTree eventNode)
    : node (std::move (eventNode))
{
    jassert (node.hasType (ids::Event));
}

std::int64_t EventModel::getTick() const
{
    return PropertyReader::getRequiredInt64 (node, ids::tick);
}

EventModel::Kind EventModel::getKind() const
{
    const auto kind = PropertyReader::getRequiredString (node, ids::kind);

    if (kind == "cc")
        return Kind::cc;

    if (kind == "pitchBend")
        return Kind::pitchBend;

    if (kind == "programChange")
        return Kind::programChange;

    if (kind == "channelPressure")
        return Kind::channelPressure;

    if (kind == "polyPressure")
        return Kind::polyPressure;

    // The Validator lets no document in whose event has any other kind.
    jassert (kind == "sysex");
    return Kind::sysex;
}

int EventModel::getA() const
{
    return PropertyReader::getInt (node, ids::a, 0);
}

int EventModel::getB() const
{
    return PropertyReader::getInt (node, ids::b, 0);
}

juce::String EventModel::getData() const
{
    return PropertyReader::getString (node, ids::data, "");
}

bool EventModel::isEnabled() const
{
    return PropertyReader::getBool (node, ids::enabled, true);
}

} // namespace model
