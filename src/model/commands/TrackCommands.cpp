#include "model/commands/TrackCommands.h"

#include "model/Ids.h"
#include "model/commands/NodeLookup.h"

#include <cstddef>

namespace model::commands
{

namespace
{

    juce::ValueTree tracksOf (const juce::ValueTree& project)
    {
        jassert (project.hasType (ids::Project));
        return project.getChildWithName (ids::Tracks);
    }

    juce::ValueTree trackOf (const juce::ValueTree& project, const juce::String& trackId)
    {
        return NodeLookup::childWithId (tracksOf (project), ids::Track, trackId);
    }

    juce::ValueTree knobOf (const juce::ValueTree& track, int cc)
    {
        return NodeLookup::childWithNumber (track.getChildWithName (ids::Knobs), ids::Knob, ids::cc, cc);
    }

    CommandResult noSuchTrack (const juce::String& trackId)
    {
        return CommandResult::refused ("There is no Track with the id \"" + trackId + "\".");
    }

    CommandResult noSuchKnob (int cc)
    {
        return CommandResult::refused ("The track has no knob for controller " + juce::String (cc) + ".");
    }

    CommandResult notAController (int cc)
    {
        return CommandResult::refused ("A controller number is 0 to 127, not " + juce::String (cc) + ".");
    }

    bool isController (int cc)
    {
        return cc >= 0 && cc <= 127;
    }

    // "#AARRGGBB", or empty for no colour (docs/SCHEMA.md -> Conventions).
    bool isColour (const juce::String& text)
    {
        if (text.isEmpty())
            return true;

        return text.length() == 9 && text.startsWithChar ('#') && text.substring (1).containsOnly ("0123456789abcdefABCDEF");
    }

    // Moves a Track or a Knob to a position among its siblings of the same
    // type. The position is counted among those only, so a child of another
    // type (data from a newer build) neither counts nor moves. `what` names
    // the siblings in the refusal.
    CommandResult moveAmongItsKind (juce::ValueTree child, int toPosition, const juce::String& what, juce::UndoManager& undoManager)
    {
        auto parent = child.getParent();
        const auto siblings = NodeLookup::childrenOfType (parent, child.getType());
        const auto count = static_cast<int> (siblings.size());

        if (toPosition < 0 || toPosition >= count)
            return CommandResult::refused ("There are " + juce::String (count) + " " + what + ", so a position is 0 to " + juce::String (count - 1) + ", not " + juce::String (toPosition) + ".");

        // Taking the place of the sibling that is there now puts the child
        // behind it when it comes from above and before it when it comes
        // from below: at that position either way.
        parent.moveChild (parent.indexOf (child), parent.indexOf (siblings[static_cast<std::size_t> (toPosition)]), &undoManager);

        return CommandResult::applied();
    }

    CommandResult setTrackText (juce::ValueTree project, const juce::String& trackId, const juce::Identifier& property, const juce::String& text, juce::UndoManager& undoManager)
    {
        auto track = trackOf (project, trackId);

        if (! track.isValid())
            return noSuchTrack (trackId);

        track.setProperty (property, text, &undoManager);

        return CommandResult::applied();
    }

    CommandResult setTrackFlag (juce::ValueTree project, const juce::String& trackId, const juce::Identifier& property, bool flag, juce::UndoManager& undoManager)
    {
        auto track = trackOf (project, trackId);

        if (! track.isValid())
            return noSuchTrack (trackId);

        // Booleans are stored as 0 / 1 (docs/SCHEMA.md -> Conventions).
        track.setProperty (property, flag ? 1 : 0, &undoManager);

        return CommandResult::applied();
    }

    // Sets the Track property that names a Performer or a Player. An empty
    // id unassigns.
    CommandResult setTrackPort (juce::ValueTree project, const juce::String& trackId, const juce::Identifier& portType, const juce::Identifier& referenceProperty, const juce::String& portId, juce::UndoManager& undoManager)
    {
        auto track = trackOf (project, trackId);

        if (! track.isValid())
            return noSuchTrack (trackId);

        if (portId.isNotEmpty() && ! NodeLookup::childWithId (project.getChildWithName (ids::Routing), portType, portId).isValid())
            return CommandResult::refused ("There is no " + portType.toString() + " with the id \"" + portId + "\".");

        track.setProperty (referenceProperty, portId, &undoManager);

        return CommandResult::applied();
    }

} // namespace

CommandResult addTrack (juce::ValueTree project, const juce::String& name, juce::UndoManager& undoManager)
{
    auto tracks = tracksOf (project);

    if (! tracks.isValid())
        return CommandResult::refused ("The project has no Tracks.");

    const auto id = juce::Uuid().toString();

    // Built apart and added whole, so that undo knows one action.
    juce::ValueTree track (ids::Track);
    track.setProperty (ids::id, id, nullptr);
    track.setProperty (ids::name, name, nullptr);
    track.appendChild (juce::ValueTree (ids::Placements), nullptr);
    track.appendChild (juce::ValueTree (ids::AutomationLanes), nullptr);
    track.appendChild (juce::ValueTree (ids::Knobs), nullptr);
    track.appendChild (juce::ValueTree (ids::Slots), nullptr);

    tracks.appendChild (track, &undoManager);

    return CommandResult::appliedAndCreated (id);
}

CommandResult removeTrack (juce::ValueTree project, const juce::String& trackId, juce::UndoManager& undoManager)
{
    const auto track = trackOf (project, trackId);

    if (! track.isValid())
        return noSuchTrack (trackId);

    track.getParent().removeChild (track, &undoManager);

    return CommandResult::applied();
}

CommandResult moveTrack (juce::ValueTree project, const juce::String& trackId, int toPosition, juce::UndoManager& undoManager)
{
    const auto track = trackOf (project, trackId);

    if (! track.isValid())
        return noSuchTrack (trackId);

    return moveAmongItsKind (track, toPosition, "tracks", undoManager);
}

CommandResult setTrackName (juce::ValueTree project, const juce::String& trackId, const juce::String& name, juce::UndoManager& undoManager)
{
    return setTrackText (project, trackId, ids::name, name, undoManager);
}

CommandResult setTrackColour (juce::ValueTree project, const juce::String& trackId, const juce::String& colour, juce::UndoManager& undoManager)
{
    if (! trackOf (project, trackId).isValid())
        return noSuchTrack (trackId);

    if (! isColour (colour))
        return CommandResult::refused ("\"" + colour + "\" is not a colour: '#' and eight hexadecimal digits, or nothing for no colour.");

    return setTrackText (project, trackId, ids::colour, colour, undoManager);
}

CommandResult setTrackPlayer (juce::ValueTree project, const juce::String& trackId, const juce::String& playerId, juce::UndoManager& undoManager)
{
    return setTrackPort (project, trackId, ids::Player, ids::playerId, playerId, undoManager);
}

CommandResult setTrackChannel (juce::ValueTree project, const juce::String& trackId, int channel, juce::UndoManager& undoManager)
{
    auto track = trackOf (project, trackId);

    if (! track.isValid())
        return noSuchTrack (trackId);

    if (channel < 1 || channel > 16)
        return CommandResult::refused ("A MIDI channel is 1 to 16, not " + juce::String (channel) + ".");

    track.setProperty (ids::channel, channel, &undoManager);

    return CommandResult::applied();
}

CommandResult setTrackPerformer (juce::ValueTree project, const juce::String& trackId, const juce::String& performerId, juce::UndoManager& undoManager)
{
    return setTrackPort (project, trackId, ids::Performer, ids::performerId, performerId, undoManager);
}

CommandResult setTrackMute (juce::ValueTree project, const juce::String& trackId, bool mute, juce::UndoManager& undoManager)
{
    return setTrackFlag (project, trackId, ids::mute, mute, undoManager);
}

CommandResult setTrackSolo (juce::ValueTree project, const juce::String& trackId, bool solo, juce::UndoManager& undoManager)
{
    return setTrackFlag (project, trackId, ids::solo, solo, undoManager);
}

CommandResult addKnob (juce::ValueTree project, const juce::String& trackId, int cc, const juce::String& label, juce::UndoManager& undoManager)
{
    const auto track = trackOf (project, trackId);

    if (! track.isValid())
        return noSuchTrack (trackId);

    if (! isController (cc))
        return notAController (cc);

    if (knobOf (track, cc).isValid())
        return CommandResult::refused ("The track already has a knob for controller " + juce::String (cc) + ".");

    juce::ValueTree knob (ids::Knob);
    knob.setProperty (ids::cc, cc, nullptr);
    knob.setProperty (ids::label, label, nullptr);

    track.getChildWithName (ids::Knobs).appendChild (knob, &undoManager);

    return CommandResult::applied();
}

CommandResult removeKnob (juce::ValueTree project, const juce::String& trackId, int cc, juce::UndoManager& undoManager)
{
    const auto track = trackOf (project, trackId);

    if (! track.isValid())
        return noSuchTrack (trackId);

    const auto knob = knobOf (track, cc);

    if (! knob.isValid())
        return noSuchKnob (cc);

    knob.getParent().removeChild (knob, &undoManager);

    return CommandResult::applied();
}

CommandResult moveKnob (juce::ValueTree project, const juce::String& trackId, int cc, int toPosition, juce::UndoManager& undoManager)
{
    const auto track = trackOf (project, trackId);

    if (! track.isValid())
        return noSuchTrack (trackId);

    const auto knob = knobOf (track, cc);

    if (! knob.isValid())
        return noSuchKnob (cc);

    return moveAmongItsKind (knob, toPosition, "knobs", undoManager);
}

CommandResult setKnobCc (juce::ValueTree project, const juce::String& trackId, int cc, int newCc, juce::UndoManager& undoManager)
{
    const auto track = trackOf (project, trackId);

    if (! track.isValid())
        return noSuchTrack (trackId);

    auto knob = knobOf (track, cc);

    if (! knob.isValid())
        return noSuchKnob (cc);

    if (! isController (newCc))
        return notAController (newCc);

    const auto holder = knobOf (track, newCc);

    if (holder.isValid() && holder != knob)
        return CommandResult::refused ("The track already has a knob for controller " + juce::String (newCc) + ".");

    knob.setProperty (ids::cc, newCc, &undoManager);

    return CommandResult::applied();
}

CommandResult setKnobLabel (juce::ValueTree project, const juce::String& trackId, int cc, const juce::String& label, juce::UndoManager& undoManager)
{
    const auto track = trackOf (project, trackId);

    if (! track.isValid())
        return noSuchTrack (trackId);

    auto knob = knobOf (track, cc);

    if (! knob.isValid())
        return noSuchKnob (cc);

    knob.setProperty (ids::label, label, &undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
