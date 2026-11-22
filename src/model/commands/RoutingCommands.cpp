#include "model/commands/RoutingCommands.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"
#include "model/commands/NodeLookup.h"

namespace model::commands
{

namespace
{

    juce::ValueTree routingOf (const juce::ValueTree& project)
    {
        jassert (project.hasType (ids::Project));
        return project.getChildWithName (ids::Routing);
    }

    // A Performer or a Player by its id; invalid when there is none.
    juce::ValueTree portOf (const juce::ValueTree& project, const juce::Identifier& portType, const juce::String& id)
    {
        return NodeLookup::childWithId (routingOf (project), portType, id);
    }

    CommandResult noSuchPort (const juce::Identifier& portType, const juce::String& id)
    {
        return CommandResult::refused ("There is no " + portType.toString() + " with the id \"" + id + "\".");
    }

    CommandResult addPort (juce::ValueTree project, const juce::Identifier& portType, const juce::String& name, juce::UndoManager& undoManager)
    {
        auto routing = routingOf (project);

        if (! routing.isValid())
            return CommandResult::refused ("The project has no Routing.");

        const auto id = juce::Uuid().toString();

        // Built apart and added whole, so that undo knows one action.
        juce::ValueTree port (portType);
        port.setProperty (ids::id, id, nullptr);
        port.setProperty (ids::name, name, nullptr);

        routing.appendChild (port, &undoManager);

        return CommandResult::appliedAndCreated (id);
    }

    // Removes the port and empties every reference a Track holds to it.
    // `referenceProperty` is the Track property that names a port of this
    // type.
    CommandResult removePort (juce::ValueTree project, const juce::Identifier& portType, const juce::Identifier& referenceProperty, const juce::String& id, juce::UndoManager& undoManager)
    {
        const auto port = portOf (project, portType, id);

        if (! port.isValid())
            return noSuchPort (portType, id);

        for (auto track : NodeLookup::childrenOfType (project.getChildWithName (ids::Tracks), ids::Track))
            if (PropertyReader::getString (track, referenceProperty, "") == id)
                track.setProperty (referenceProperty, juce::String(), &undoManager);

        port.getParent().removeChild (port, &undoManager);

        return CommandResult::applied();
    }

    CommandResult setPortName (juce::ValueTree project, const juce::Identifier& portType, const juce::String& id, const juce::String& name, juce::UndoManager& undoManager)
    {
        auto port = portOf (project, portType, id);

        if (! port.isValid())
            return noSuchPort (portType, id);

        port.setProperty (ids::name, name, &undoManager);

        return CommandResult::applied();
    }

    CommandResult setPortDevice (juce::ValueTree project, const juce::Identifier& portType, const juce::String& id, const juce::String& deviceIdentifier, const juce::String& deviceName, juce::UndoManager& undoManager)
    {
        auto port = portOf (project, portType, id);

        if (! port.isValid())
            return noSuchPort (portType, id);

        port.setProperty (ids::deviceIdentifier, deviceIdentifier, &undoManager);
        port.setProperty (ids::deviceName, deviceName, &undoManager);

        return CommandResult::applied();
    }

} // namespace

CommandResult addPerformer (juce::ValueTree project, const juce::String& name, juce::UndoManager& undoManager)
{
    return addPort (project, ids::Performer, name, undoManager);
}

CommandResult removePerformer (juce::ValueTree project, const juce::String& performerId, juce::UndoManager& undoManager)
{
    return removePort (project, ids::Performer, ids::performerId, performerId, undoManager);
}

CommandResult setPerformerName (juce::ValueTree project, const juce::String& performerId, const juce::String& name, juce::UndoManager& undoManager)
{
    return setPortName (project, ids::Performer, performerId, name, undoManager);
}

CommandResult setPerformerDevice (juce::ValueTree project, const juce::String& performerId, const juce::String& deviceIdentifier, const juce::String& deviceName, juce::UndoManager& undoManager)
{
    return setPortDevice (project, ids::Performer, performerId, deviceIdentifier, deviceName, undoManager);
}

CommandResult setPerformerChannelFilter (juce::ValueTree project, const juce::String& performerId, int channelFilter, juce::UndoManager& undoManager)
{
    auto performer = portOf (project, ids::Performer, performerId);

    if (! performer.isValid())
        return noSuchPort (ids::Performer, performerId);

    if (channelFilter < 0 || channelFilter > 16)
        return CommandResult::refused ("A channel filter is 0 for every channel or a channel from 1 to 16, not " + juce::String (channelFilter) + ".");

    performer.setProperty (ids::channelFilter, channelFilter, &undoManager);

    return CommandResult::applied();
}

CommandResult addPlayer (juce::ValueTree project, const juce::String& name, juce::UndoManager& undoManager)
{
    return addPort (project, ids::Player, name, undoManager);
}

CommandResult removePlayer (juce::ValueTree project, const juce::String& playerId, juce::UndoManager& undoManager)
{
    return removePort (project, ids::Player, ids::playerId, playerId, undoManager);
}

CommandResult setPlayerName (juce::ValueTree project, const juce::String& playerId, const juce::String& name, juce::UndoManager& undoManager)
{
    return setPortName (project, ids::Player, playerId, name, undoManager);
}

CommandResult setPlayerDevice (juce::ValueTree project, const juce::String& playerId, const juce::String& deviceIdentifier, const juce::String& deviceName, juce::UndoManager& undoManager)
{
    return setPortDevice (project, ids::Player, playerId, deviceIdentifier, deviceName, undoManager);
}

CommandResult setPlayerSendClock (juce::ValueTree project, const juce::String& playerId, bool sendClock, juce::UndoManager& undoManager)
{
    auto player = portOf (project, ids::Player, playerId);

    if (! player.isValid())
        return noSuchPort (ids::Player, playerId);

    // Booleans are stored as 0 / 1 (docs/SCHEMA.md -> Conventions).
    player.setProperty (ids::sendClock, sendClock ? 1 : 0, &undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
