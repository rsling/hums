#include "model/NoteModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

NoteModel::NoteModel (juce::ValueTree noteNode)
    : node (std::move (noteNode))
{
    jassert (node.hasType (ids::Note));
}

std::int64_t NoteModel::getTick() const
{
    return PropertyReader::getRequiredInt64 (node, ids::tick);
}

std::int64_t NoteModel::getLength() const
{
    return PropertyReader::getRequiredInt64 (node, ids::length);
}

int NoteModel::getPitch() const
{
    return PropertyReader::getRequiredInt (node, ids::pitch);
}

int NoteModel::getVelocity() const
{
    return PropertyReader::getInt (node, ids::velocity, 100);
}

int NoteModel::getOffVelocity() const
{
    return PropertyReader::getInt (node, ids::offVelocity, 64);
}

bool NoteModel::isEnabled() const
{
    return PropertyReader::getBool (node, ids::enabled, true);
}

} // namespace model
