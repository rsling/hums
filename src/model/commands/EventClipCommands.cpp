#include "model/commands/EventClipCommands.h"

#include "model/AutomationClipRule.h"
#include "model/ClipModel.h"
#include "model/Ids.h"
#include "model/NoteModel.h"
#include "model/PlacementModel.h"
#include "model/PropertyReader.h"
#include "model/commands/NodeLookup.h"
#include "model/commands/RefcountGate.h"

#include <limits>
#include <vector>

namespace model::commands
{

namespace
{

    const std::int64_t largestTick = std::numeric_limits<std::int64_t>::max();

    // The payload of a sysex event: at most 64 KB, and a byte is two digits.
    const int mostDataDigits = 2 * 64 * 1024;

    // Why the clip holds no notes and events at all; empty for an event
    // clip.
    juce::String whyNoEventClip (const juce::ValueTree& clip)
    {
        if (ClipModel (clip).getKind() != ClipModel::Kind::event)
            return "The clip is a step clip; notes, events, a stored loop length and quantisation belong to an event clip.";

        return {};
    }

    // Whether the clip the gate will hand out is referenced from an
    // AutomationLane. A clone is referenced by exactly the references of
    // the context, so only those count for it; the clip as it stands is
    // asked for all its references.
    bool landsOnALane (const juce::ValueTree& project, const RefcountGate::Admission& admission, const EditContext& editContext)
    {
        if (! admission.needsClone)
            return AutomationClipRule::isOnALane (project, PropertyReader::getRequiredString (admission.clip, ids::id));

        for (const auto& placementId : editContext.getPlacementIds())
            if (NodeLookup::placementWithId (project, placementId).getParent().hasType (ids::AutomationLane))
                return true;

        return false;
    }

    juce::String whyNotWithin (const juce::String& what, int value, int lowest, int highest)
    {
        if (value < lowest || value > highest)
            return what + " is from " + juce::String (lowest) + " to " + juce::String (highest) + ", not " + juce::String (value) + ".";

        return {};
    }

    // Why these are not the values of a note; empty when they are.
    juce::String whyNoNote (const NoteValues& values)
    {
        if (values.tick < 0)
            return "A note starts at tick 0 or later, not at " + juce::String (values.tick) + ".";

        if (values.length < 1)
            return "A note is at least one tick long, not " + juce::String (values.length) + ".";

        if (values.length > largestTick - values.tick)
            return "A note that starts at tick " + juce::String (values.tick) + " cannot be " + juce::String (values.length) + " ticks long: it would end past the largest tick there is.";

        for (const auto& reason : { whyNotWithin ("A pitch", values.pitch, 0, 127), whyNotWithin ("A velocity", values.velocity, 1, 127), whyNotWithin ("A note-off velocity", values.offVelocity, 0, 127) })
            if (reason.isNotEmpty())
                return reason;

        return {};
    }

    // Why these are not the values of an event of their kind; empty when
    // they are. The table of docs/SCHEMA.md -> Event.
    juce::String whyNoEvent (const EventValues& values)
    {
        using Kind = EventModel::Kind;

        if (values.tick < 0)
            return "An event is at tick 0 or later, not at " + juce::String (values.tick) + ".";

        const auto kindText = EventModel::textOf (values.kind);
        const auto usesB = values.kind == Kind::cc || values.kind == Kind::polyPressure;

        juce::String reason;

        switch (values.kind)
        {
            case Kind::cc:
                reason = whyNotWithin ("A controller number", values.a, 0, 127);
                break;
            case Kind::pitchBend:
                reason = whyNotWithin ("A pitch bend", values.a, 0, 16383);
                break;
            case Kind::programChange:
                reason = whyNotWithin ("A program", values.a, 0, 127);
                break;
            case Kind::channelPressure:
                reason = whyNotWithin ("A pressure", values.a, 0, 127);
                break;
            case Kind::polyPressure:
                reason = whyNotWithin ("A pitch", values.a, 0, 127);
                break;
            case Kind::sysex:
                if (values.a != 0)
                    reason = "A sysex event has no a; it is 0, not " + juce::String (values.a) + ".";
                break;
        }

        if (reason.isNotEmpty())
            return reason;

        if (usesB)
            reason = whyNotWithin (values.kind == Kind::cc ? "A controller value" : "A pressure", values.b, 0, 127);
        else if (values.b != 0)
            reason = "A " + kindText + " event has no b; it is 0, not " + juce::String (values.b) + ".";

        if (reason.isNotEmpty())
            return reason;

        if (values.kind != Kind::sysex)
        {
            if (values.data.isNotEmpty())
                return "Only a sysex event has data, not a " + kindText + " event.";

            return {};
        }

        if (! values.data.containsOnly ("0123456789abcdefABCDEF"))
            return "The data of a sysex event is hexadecimal digits only.";

        if (values.data.length() % 2 != 0)
            return "The data of a sysex event has two hexadecimal digits to a byte; " + juce::String (values.data.length()) + " digits are no whole bytes.";

        if (values.data.length() > mostDataDigits)
            return "The data of a sysex event is at most 65536 bytes, not " + juce::String (values.data.length() / 2) + ".";

        return {};
    }

    const char* const onlyCcOnALane = "An automation lane references this clip, and such a clip holds nothing but cc events.";

    bool areSame (const NoteModel& note, const NoteValues& values)
    {
        return note.getTick() == values.tick
            && note.getLength() == values.length
            && note.getPitch() == values.pitch
            && note.getVelocity() == values.velocity
            && note.getOffVelocity() == values.offVelocity
            && note.isEnabled() == values.enabled;
    }

    bool areSame (const EventModel& event, const EventValues& values)
    {
        return event.getTick() == values.tick
            && event.getKind() == values.kind
            && event.getA() == values.a
            && event.getB() == values.b
            && event.getData() == values.data
            && event.isEnabled() == values.enabled;
    }

    // The setters below write a property only where the value differs from
    // what the node reads as, so that undo holds what changed and a default
    // that was never stored stays unstored.
    void setTick (juce::ValueTree node, const juce::Identifier& property, std::int64_t stored, std::int64_t value, juce::UndoManager* undoManager)
    {
        if (stored != value)
            node.setProperty (property, static_cast<juce::int64> (value), undoManager);
    }

    void setInt (juce::ValueTree node, const juce::Identifier& property, int stored, int value, juce::UndoManager* undoManager)
    {
        if (stored != value)
            node.setProperty (property, value, undoManager);
    }

    void setFlag (juce::ValueTree node, const juce::Identifier& property, bool stored, bool value, juce::UndoManager* undoManager)
    {
        if (stored != value)
            node.setProperty (property, value ? 1 : 0, undoManager);
    }

    juce::ValueTree newNote (const NoteValues& values)
    {
        juce::ValueTree note (ids::Note);
        note.setProperty (ids::tick, static_cast<juce::int64> (values.tick), nullptr);
        note.setProperty (ids::length, static_cast<juce::int64> (values.length), nullptr);
        note.setProperty (ids::pitch, values.pitch, nullptr);
        note.setProperty (ids::velocity, values.velocity, nullptr);
        note.setProperty (ids::offVelocity, values.offVelocity, nullptr);
        note.setProperty (ids::enabled, values.enabled ? 1 : 0, nullptr);

        return note;
    }

    juce::ValueTree newEvent (const EventValues& values)
    {
        juce::ValueTree event (ids::Event);
        event.setProperty (ids::tick, static_cast<juce::int64> (values.tick), nullptr);
        event.setProperty (ids::kind, EventModel::textOf (values.kind), nullptr);
        event.setProperty (ids::a, values.a, nullptr);
        event.setProperty (ids::b, values.b, nullptr);

        if (values.data.isNotEmpty())
            event.setProperty (ids::data, values.data, nullptr);

        event.setProperty (ids::enabled, values.enabled ? 1 : 0, nullptr);

        return event;
    }

    // The clip's notes, or its events, in child order.
    std::vector<juce::ValueTree> contentOf (const juce::ValueTree& clip, const juce::Identifier& type)
    {
        return NodeLookup::childrenOfType (clip.getChildWithName (ids::Events), type);
    }

    CommandResult noSuchContent (const juce::String& what, int index, std::size_t count)
    {
        return CommandResult::refused ("The clip has no " + what + " at position " + juce::String (index) + "; it has " + juce::String (static_cast<juce::int64> (count)) + ".");
    }

    bool isPositionIn (int index, std::size_t count)
    {
        return index >= 0 && static_cast<std::size_t> (index) < count;
    }

    // What every command here begins with: the gate lets the gesture at the
    // clip, and the clip is an event clip.
    RefcountGate::Admission admitToEventClip (const juce::ValueTree& project, const juce::String& clipId, const EditContext& editContext, const juce::UndoManager& undoManager)
    {
        auto admission = RefcountGate::admit (project, clipId, editContext, undoManager);

        if (admission.wasAdmitted())
            admission.refusal = whyNoEventClip (admission.clip);

        return admission;
    }

    // Takes the note or event at that position out of the clip.
    CommandResult removeContent (juce::ValueTree project, const juce::String& clipId, const juce::Identifier& type, const juce::String& what, int index, EditContext& editContext, juce::UndoManager& undoManager)
    {
        const auto admission = admitToEventClip (project, clipId, editContext, undoManager);

        if (! admission.wasAdmitted())
            return CommandResult::refused (admission.refusal);

        const auto count = contentOf (admission.clip, type).size();

        if (! isPositionIn (index, count))
            return noSuchContent (what, index, count);

        const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
        auto node = contentOf (clip, type)[static_cast<std::size_t> (index)];
        node.getParent().removeChild (node, &undoManager);

        return CommandResult::applied();
    }

} // namespace

CommandResult addNote (juce::ValueTree project, const juce::String& clipId, const NoteValues& values, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToEventClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    const auto reason = whyNoNote (values);

    if (reason.isNotEmpty())
        return CommandResult::refused (reason);

    if (landsOnALane (project, admission, editContext))
        return CommandResult::refused (onlyCcOnALane);

    auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    clip.getChildWithName (ids::Events).appendChild (newNote (values), &undoManager);

    return CommandResult::applied();
}

CommandResult setNote (juce::ValueTree project, const juce::String& clipId, int noteIndex, const NoteValues& values, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToEventClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    const auto notesNow = contentOf (admission.clip, ids::Note);

    if (! isPositionIn (noteIndex, notesNow.size()))
        return noSuchContent ("note", noteIndex, notesNow.size());

    const auto reason = whyNoNote (values);

    if (reason.isNotEmpty())
        return CommandResult::refused (reason);

    const auto position = static_cast<std::size_t> (noteIndex);

    if (areSame (NoteModel (notesNow[position]), values))
        return CommandResult::applied();

    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    auto note = contentOf (clip, ids::Note)[position];
    const NoteModel stored (note);

    setTick (note, ids::tick, stored.getTick(), values.tick, &undoManager);
    setTick (note, ids::length, stored.getLength(), values.length, &undoManager);
    setInt (note, ids::pitch, stored.getPitch(), values.pitch, &undoManager);
    setInt (note, ids::velocity, stored.getVelocity(), values.velocity, &undoManager);
    setInt (note, ids::offVelocity, stored.getOffVelocity(), values.offVelocity, &undoManager);
    setFlag (note, ids::enabled, stored.isEnabled(), values.enabled, &undoManager);

    return CommandResult::applied();
}

CommandResult removeNote (juce::ValueTree project, const juce::String& clipId, int noteIndex, EditContext& editContext, juce::UndoManager& undoManager)
{
    return removeContent (project, clipId, ids::Note, "note", noteIndex, editContext, undoManager);
}

CommandResult addEvent (juce::ValueTree project, const juce::String& clipId, const EventValues& values, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToEventClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    const auto reason = whyNoEvent (values);

    if (reason.isNotEmpty())
        return CommandResult::refused (reason);

    if (values.kind != EventModel::Kind::cc && landsOnALane (project, admission, editContext))
        return CommandResult::refused (onlyCcOnALane);

    auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    clip.getChildWithName (ids::Events).appendChild (newEvent (values), &undoManager);

    return CommandResult::applied();
}

CommandResult setEvent (juce::ValueTree project, const juce::String& clipId, int eventIndex, const EventValues& values, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToEventClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    const auto eventsNow = contentOf (admission.clip, ids::Event);

    if (! isPositionIn (eventIndex, eventsNow.size()))
        return noSuchContent ("event", eventIndex, eventsNow.size());

    const auto reason = whyNoEvent (values);

    if (reason.isNotEmpty())
        return CommandResult::refused (reason);

    if (values.kind != EventModel::Kind::cc && landsOnALane (project, admission, editContext))
        return CommandResult::refused (onlyCcOnALane);

    const auto position = static_cast<std::size_t> (eventIndex);

    if (areSame (EventModel (eventsNow[position]), values))
        return CommandResult::applied();

    const auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    auto event = contentOf (clip, ids::Event)[position];
    const EventModel stored (event);

    setTick (event, ids::tick, stored.getTick(), values.tick, &undoManager);

    if (stored.getKind() != values.kind)
        event.setProperty (ids::kind, EventModel::textOf (values.kind), &undoManager);

    setInt (event, ids::a, stored.getA(), values.a, &undoManager);
    setInt (event, ids::b, stored.getB(), values.b, &undoManager);

    if (stored.getData() != values.data)
        event.setProperty (ids::data, values.data, &undoManager);

    setFlag (event, ids::enabled, stored.isEnabled(), values.enabled, &undoManager);

    return CommandResult::applied();
}

CommandResult removeEvent (juce::ValueTree project, const juce::String& clipId, int eventIndex, EditContext& editContext, juce::UndoManager& undoManager)
{
    return removeContent (project, clipId, ids::Event, "event", eventIndex, editContext, undoManager);
}

CommandResult setClipLoopLength (juce::ValueTree project, const juce::String& clipId, std::int64_t loopLength, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToEventClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    if (loopLength < 1)
        return CommandResult::refused ("The loop of an event clip is at least one tick long, not " + juce::String (loopLength) + ".");

    if (ClipModel (admission.clip).getLoopLength() == loopLength)
        return CommandResult::applied();

    auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    clip.setProperty (ids::loopLength, static_cast<juce::int64> (loopLength), &undoManager);

    // Every placement of the changed clip — after a clone, those of the
    // clone — keeps its offset inside the loop (invariant 4). Slots have no
    // offset. A valid document has no negative offset.
    const auto changedClipId = PropertyReader::getRequiredString (clip, ids::id);

    for (const auto& track : NodeLookup::childrenOfType (project.getChildWithName (ids::Tracks), ids::Track))
    {
        auto parents = NodeLookup::childrenOfType (track.getChildWithName (ids::AutomationLanes), ids::AutomationLane);
        parents.push_back (track.getChildWithName (ids::Placements));

        for (const auto& parent : parents)
        {
            for (auto node : NodeLookup::childrenOfType (parent, ids::Placement))
            {
                const PlacementModel placement (node);

                if (placement.getClipId() != changedClipId)
                    continue;

                const auto offset = placement.getOffset();
                const auto offsetInLoop = offset % loopLength;

                // An offset of 0 is the default and is not stored, as on a
                // new placement.
                if (offsetInLoop == offset)
                    continue;

                if (offsetInLoop == 0)
                    node.removeProperty (ids::offset, &undoManager);
                else
                    node.setProperty (ids::offset, static_cast<juce::int64> (offsetInLoop), &undoManager);
            }
        }
    }

    return CommandResult::applied();
}

CommandResult setClipQuantise (juce::ValueTree project, const juce::String& clipId, const juce::String& quantise, EditContext& editContext, juce::UndoManager& undoManager)
{
    const auto admission = admitToEventClip (project, clipId, editContext, undoManager);

    if (! admission.wasAdmitted())
        return CommandResult::refused (admission.refusal);

    // The list of docs/SCHEMA.md -> ClipPool / Clip.
    if (! juce::StringArray { "", "1/2", "1/4", "1/8", "1/2T", "1/4T", "1/8T" }.contains (quantise))
        return CommandResult::refused ("\"" + quantise + "\" is no quantisation: it is one of 1/2, 1/4, 1/8, 1/2T, 1/4T, 1/8T, or nothing for off.");

    if (ClipModel (admission.clip).getQuantise() == quantise)
        return CommandResult::applied();

    auto clip = RefcountGate::open (project, admission, editContext, undoManager);
    clip.setProperty (ids::quantise, quantise, &undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
