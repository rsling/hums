#include "model/ClipModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

ClipModel::ClipModel (juce::ValueTree clipNode)
    : node (std::move (clipNode))
{
    jassert (node.hasType (ids::Clip));
}

juce::String ClipModel::getId() const
{
    return PropertyReader::getRequiredString (node, ids::id);
}

juce::String ClipModel::getName() const
{
    return PropertyReader::getString (node, ids::name, "");
}

ClipModel::Kind ClipModel::getKind() const
{
    const auto kind = PropertyReader::getRequiredString (node, ids::kind);

    if (kind == "step")
        return Kind::step;

    // The Validator lets no document in whose clip has any other kind.
    jassert (kind == "event");
    return Kind::event;
}

juce::String ClipModel::getColour() const
{
    return PropertyReader::getString (node, ids::colour, "");
}

std::int64_t ClipModel::getLoopLength() const
{
    if (getKind() == Kind::step)
        return getGrid().getLoopLength();

    return PropertyReader::getRequiredInt64 (node, ids::loopLength);
}

juce::String ClipModel::getQuantise() const
{
    return PropertyReader::getString (node, ids::quantise, "");
}

std::int64_t ClipModel::getQuantiseGrid() const
{
    const auto quantise = getQuantise();

    // The table of docs/SCHEMA.md -> ClipPool / Clip. A triplet grid is two
    // thirds of the straight one.
    if (quantise == "1/2")
        return 1920;

    if (quantise == "1/4")
        return 960;

    if (quantise == "1/8")
        return 480;

    if (quantise == "1/2T")
        return 1280;

    if (quantise == "1/4T")
        return 640;

    if (quantise == "1/8T")
        return 320;

    // The Validator lets no document in whose quantise is anything else.
    jassert (quantise.isEmpty());
    return 0;
}

std::vector<NoteModel> ClipModel::getNotes() const
{
    std::vector<NoteModel> notes;

    for (const auto& child : node.getChildWithName (ids::Events))
        if (child.hasType (ids::Note))
            notes.emplace_back (child);

    return notes;
}

std::vector<EventModel> ClipModel::getEvents() const
{
    std::vector<EventModel> events;

    for (const auto& child : node.getChildWithName (ids::Events))
        if (child.hasType (ids::Event))
            events.emplace_back (child);

    return events;
}

GridModel ClipModel::getGrid() const
{
    return GridModel (node.getChildWithName (ids::Grid));
}

} // namespace model
