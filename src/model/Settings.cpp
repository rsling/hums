#include "model/Settings.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

namespace
{

    // The texts of the table in docs/SCHEMA.md -> UiState.
    const char* const updateAllText = "updateAll";
    const char* const makeUniqueText = "makeUnique";

    const char* tabText (Settings::Tab tab)
    {
        switch (tab)
        {
            case Settings::Tab::routing:
                return "routing";
            case Settings::Tab::pianoroll:
                return "pianoroll";
            case Settings::Tab::arrangement:
                return "arrangement";
            case Settings::Tab::drums:
                return "drums";
            case Settings::Tab::stepseq:
                return "stepseq";
            case Settings::Tab::automation:
                return "automation";
        }

        jassertfalse;
        return "arrangement";
    }

} // namespace

Settings::Settings (juce::ValueTree uiStateNode)
    : node (std::move (uiStateNode))
{
    jassert (node.hasType (ids::UiState));
}

Settings::Tab Settings::getActiveTab() const
{
    const auto text = PropertyReader::getString (node, ids::activeTab, tabText (Tab::arrangement));

    for (const auto tab : { Tab::routing, Tab::pianoroll, Tab::drums, Tab::stepseq, Tab::automation })
        if (text == tabText (tab))
            return tab;

    // The Validator lets no document in whose active tab has any other name.
    jassert (text == tabText (Tab::arrangement));
    return Tab::arrangement;
}

void Settings::setActiveTab (Tab tab)
{
    if (tab != getActiveTab())
        node.setProperty (ids::activeTab, tabText (tab), nullptr);
}

Settings::SharedEditPolicy Settings::getSharedEditPolicy() const
{
    const auto policy = PropertyReader::getString (node, ids::sharedEditPolicy, updateAllText);

    if (policy == makeUniqueText)
        return SharedEditPolicy::makeUnique;

    jassert (policy == updateAllText);
    return SharedEditPolicy::updateAll;
}

void Settings::setSharedEditPolicy (SharedEditPolicy policy)
{
    if (policy == getSharedEditPolicy())
        return;

    const auto* text = policy == SharedEditPolicy::makeUnique ? makeUniqueText : updateAllText;
    node.setProperty (ids::sharedEditPolicy, text, nullptr);
}

Settings::TransportLoop Settings::getTransportLoop() const
{
    TransportLoop loop;
    loop.start = PropertyReader::getInt64 (node, ids::loopStart, 0);
    loop.end = PropertyReader::getInt64 (node, ids::loopEnd, 0);
    loop.enabled = PropertyReader::getBool (node, ids::loopEnabled, false);
    return loop;
}

bool Settings::setTransportLoop (const TransportLoop& loop)
{
    if (loop.start < 0 || loop.end < 0)
        return false;

    if (loop.enabled && loop.start >= loop.end)
        return false;

    // Three properties cannot change at once, and a listener hears each
    // write. The order below keeps every state in between a valid one: a
    // listener never sees an enabled loop whose start is not before its end.
    if (! loop.enabled)
    {
        writeLoopEnabled (false);
        writeTick (ids::loopStart, 0, loop.start);
        writeTick (ids::loopEnd, 0, loop.end);
        return true;
    }

    // Moving the start first is safe as long as it stays before the old end;
    // otherwise the loop moves to the right as a whole, and the end goes first.
    if (loop.start < getTransportLoop().end)
    {
        writeTick (ids::loopStart, 0, loop.start);
        writeTick (ids::loopEnd, 0, loop.end);
    }
    else
    {
        writeTick (ids::loopEnd, 0, loop.end);
        writeTick (ids::loopStart, 0, loop.start);
    }

    writeLoopEnabled (true);
    return true;
}

std::int64_t Settings::getExportLength() const
{
    return PropertyReader::getInt64 (node, ids::exportLength, 0);
}

bool Settings::setExportLength (std::int64_t ticks)
{
    if (ticks < 0)
        return false;

    writeTick (ids::exportLength, 0, ticks);
    return true;
}

void Settings::writeTick (const juce::Identifier& property, std::int64_t valueWhenAbsent, std::int64_t ticks)
{
    if (ticks != PropertyReader::getInt64 (node, property, valueWhenAbsent))
        node.setProperty (property, static_cast<juce::int64> (ticks), nullptr);
}

void Settings::writeLoopEnabled (bool enabled)
{
    // Booleans are stored as 0 / 1 (docs/SCHEMA.md -> Conventions).
    if (enabled != PropertyReader::getBool (node, ids::loopEnabled, false))
        node.setProperty (ids::loopEnabled, enabled ? 1 : 0, nullptr);
}

} // namespace model
