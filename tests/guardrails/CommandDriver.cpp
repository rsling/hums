#include "CommandDriver.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"
#include "model/TracksModel.h"
#include "model/commands/AutomationLaneCommands.h"
#include "model/commands/ClipPoolCommands.h"
#include "model/commands/NodeLookup.h"
#include "model/commands/PadMapCommands.h"
#include "model/commands/PlacementCommands.h"
#include "model/commands/RoutingCommands.h"
#include "model/commands/SessionCommands.h"
#include "model/commands/SlotCommands.h"
#include "model/commands/TimelineCommands.h"
#include "model/commands/TrackCommands.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace guardrails
{

namespace
{

    namespace ids = model::ids;
    namespace commands = model::commands;
    using model::PropertyReader;
    using model::commands::NodeLookup;

    juce::String quoted (const juce::String& text)
    {
        return "\"" + text + "\"";
    }

    bool contains (const std::vector<std::int64_t>& numbers, std::int64_t number)
    {
        return std::find (numbers.begin(), numbers.end(), number) != numbers.end();
    }

    bool anyNodeHasId (const juce::ValueTree& node, const juce::String& id)
    {
        if (PropertyReader::getString (node, ids::id, "") == id)
            return true;

        for (const auto& child : node)
            if (anyNodeHasId (child, id))
                return true;

        return false;
    }

} // namespace

//==============================================================================
// The table: every command of model/commands/.

const std::vector<CommandDriver::TableEntry>& CommandDriver::table()
{
    static const std::vector<TableEntry> entries {
        { "addPerformer", &CommandDriver::addPerformer, false },
        { "removePerformer", &CommandDriver::removePerformer, true },
        { "setPerformerName", &CommandDriver::setPerformerName, true },
        { "setPerformerDevice", &CommandDriver::setPerformerDevice, true },
        { "setPerformerChannelFilter", &CommandDriver::setPerformerChannelFilter, true },
        { "addPlayer", &CommandDriver::addPlayer, false },
        { "removePlayer", &CommandDriver::removePlayer, true },
        { "setPlayerName", &CommandDriver::setPlayerName, true },
        { "setPlayerDevice", &CommandDriver::setPlayerDevice, true },
        { "setPlayerSendClock", &CommandDriver::setPlayerSendClock, true },
        { "addPad", &CommandDriver::addPad, true },
        { "removePad", &CommandDriver::removePad, true },
        { "setPadInputPitch", &CommandDriver::setPadInputPitch, true },
        { "setPadPitch", &CommandDriver::setPadPitch, true },
        { "setPadLabel", &CommandDriver::setPadLabel, true },
        { "setTempo", &CommandDriver::setTempo, true },
        { "removeTempoEvent", &CommandDriver::removeTempoEvent, true },
        { "setTimeSignature", &CommandDriver::setTimeSignature, true },
        { "removeTimeSigEvent", &CommandDriver::removeTimeSigEvent, true },
        { "addMarker", &CommandDriver::addMarker, true },
        { "removeMarker", &CommandDriver::removeMarker, true },
        { "moveMarker", &CommandDriver::moveMarker, true },
        { "renameMarker", &CommandDriver::renameMarker, true },
        { "setLaunchQuantise", &CommandDriver::setLaunchQuantise, true },
        { "setSceneName", &CommandDriver::setSceneName, true },
        { "addTrack", &CommandDriver::addTrack, false },
        { "removeTrack", &CommandDriver::removeTrack, true },
        { "moveTrack", &CommandDriver::moveTrack, true },
        { "setTrackName", &CommandDriver::setTrackName, true },
        { "setTrackColour", &CommandDriver::setTrackColour, true },
        { "setTrackPlayer", &CommandDriver::setTrackPlayer, true },
        { "setTrackChannel", &CommandDriver::setTrackChannel, true },
        { "setTrackPerformer", &CommandDriver::setTrackPerformer, true },
        { "setTrackMute", &CommandDriver::setTrackMute, true },
        { "setTrackSolo", &CommandDriver::setTrackSolo, true },
        { "addKnob", &CommandDriver::addKnob, true },
        { "removeKnob", &CommandDriver::removeKnob, true },
        { "moveKnob", &CommandDriver::moveKnob, true },
        { "setKnobCc", &CommandDriver::setKnobCc, true },
        { "setKnobLabel", &CommandDriver::setKnobLabel, true },
        { "addEventClip", &CommandDriver::addEventClip, true },
        { "addStepClip", &CommandDriver::addStepClip, false },
        { "setClipName", &CommandDriver::setClipName, true },
        { "setClipColour", &CommandDriver::setClipColour, true },
        { "removeClip", &CommandDriver::removeClip, true },
        { "removeUnusedClips", &CommandDriver::removeUnusedClips, false },
        { "addPlacement", &CommandDriver::addPlacement, true },
        { "movePlacement", &CommandDriver::movePlacement, true },
        { "movePlacementToTrack", &CommandDriver::movePlacementToTrack, true },
        { "resizePlacement", &CommandDriver::resizePlacement, true },
        { "splitPlacement", &CommandDriver::splitPlacement, true },
        { "removePlacement", &CommandDriver::removePlacement, true },
        { "setSlot", &CommandDriver::setSlot, true },
        { "clearSlot", &CommandDriver::clearSlot, true },
        { "insertScene", &CommandDriver::insertScene, true },
        { "removeScene", &CommandDriver::removeScene, true },
        { "moveScene", &CommandDriver::moveScene, true },
        { "addAutomationLane", &CommandDriver::addAutomationLane, true },
        { "removeAutomationLane", &CommandDriver::removeAutomationLane, true },
        { "moveAutomationLane", &CommandDriver::moveAutomationLane, true },
        { "setAutomationLaneName", &CommandDriver::setAutomationLaneName, true },
        { "addLanePlacement", &CommandDriver::addLanePlacement, true },
    };

    return entries;
}

std::vector<CommandDriver::KnownCommand> CommandDriver::knownCommands()
{
    std::vector<KnownCommand> known;

    for (const auto& entry : table())
        known.push_back ({ entry.name, entry.hasInvalidForm });

    return known;
}

CommandDriver::CommandDriver (juce::ValueTree projectNode, juce::UndoManager& undoManagerToUse, RandomSource& randomToUse)
    : project (std::move (projectNode)), undoManager (undoManagerToUse), random (randomToUse)
{
}

CommandChecks::Attempt CommandDriver::attemptOne()
{
    const auto& entry = table()[random.indexBelow (table().size())];

    return (this->*entry.attempt)();
}

CommandChecks::Attempt CommandDriver::made (const juce::String& call, bool isValidForm, const commands::CommandResult& result) const
{
    auto description = call + (isValidForm ? " [valid]" : " [invalid]");

    if (! result.wasApplied())
        description << " [refused: " << result.refusal << "]";

    return { description.toStdString(), isValidForm, result.wasApplied() };
}

//==============================================================================
// Values.

bool CommandDriver::wantsValidForm()
{
    return random.chance (70);
}

// A name or a label: often empty, and with the characters XML has to write
// in another form.
juce::String CommandDriver::text()
{
    static const std::vector<juce::String> pieces {
        "Minimoog",
        "Keystep",
        "Cutoff",
        " ",
        "&",
        "<",
        "\"",
        "'",
        "\n",
        "\t",
        juce::String::fromUTF8 ("\xc3\xb6"),         // o with two dots
        juce::String::fromUTF8 ("\xf0\x9f\x8e\xb9"), // a keyboard, outside the 16 bits of UTF-16
    };

    if (random.chance (25))
        return {};

    juce::String result;

    for (auto count = random.intBetween (1, 3); count > 0; --count)
        result << random.oneOf (pieces);

    return result;
}

// An id in the right form that no node has; now and then a text that is no
// id at all.
juce::String CommandDriver::unknownId()
{
    if (random.chance (20))
        return random.chance (50) ? juce::String() : juce::String ("no id");

    static const char* const digits = "0123456789abcdef";

    // Drawn again in the rare case that a node has it: the document's ids
    // come from random numbers too, and with the same seed from the same
    // ones.
    for (;;)
    {
        juce::String id;

        for (int digit = 0; digit < 32; ++digit)
            id << digits[random.indexBelow (16)];

        if (! anyNodeHasId (project, id))
            return id;
    }
}

// A position: mostly on a sixteenth, sometimes anywhere, now and then far
// beyond what an int holds.
std::int64_t CommandDriver::tick()
{
    if (random.chance (60))
        return 240 * random.between (0, 128);

    if (random.chance (95))
        return random.between (0, 40000);

    return random.between (0, std::int64_t { 1 } << 40);
}

std::int64_t CommandDriver::negativeTick()
{
    return random.chance (50) ? -1 : -random.between (1, std::int64_t { 1 } << 40);
}

// Any tempo, with more digits than the document keeps [D14]: the command
// has to round it.
double CommandDriver::tempo()
{
    if (random.chance (30))
        return random.oneOf (std::vector<double> { 60.0, 118.0, 120.0, 174.5, 999.9996, 0.000099, 1.0 / 3.0 });

    if (random.chance (90))
        return static_cast<double> (random.between (20000000, 999999999)) / 1000000.0;

    return random.oneOf (std::vector<double> { 1e-300, 1e-7, 123456.789, 1e20, std::numeric_limits<double>::max() });
}

double CommandDriver::noTempo()
{
    return random.oneOf (std::vector<double> { 0.0, -0.0, -120.0, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
                                                 // Greater than 0, but its saved text reads back as 0.
                                               std::numeric_limits<double>::denorm_min() });
}

int CommandDriver::notAPitch()
{
    return random.oneOf (std::vector<int> { -1, 128, 1000, std::numeric_limits<int>::min() });
}

//==============================================================================
// What the document holds now.

std::vector<juce::String> CommandDriver::idsOf (const juce::Identifier& portType) const
{
    std::vector<juce::String> found;

    for (const auto& port : NodeLookup::childrenOfType (project.getChildWithName (ids::Routing), portType))
        found.push_back (PropertyReader::getRequiredString (port, ids::id));

    return found;
}

std::vector<juce::ValueTree> CommandDriver::pads() const
{
    return NodeLookup::childrenOfType (project.getChildWithName (ids::Routing).getChildWithName (ids::PadMap), ids::Pad);
}

std::vector<std::int64_t> CommandDriver::ticksOf (const juce::Identifier& timelineEventType) const
{
    std::vector<std::int64_t> ticks;

    for (const auto& event : NodeLookup::childrenOfType (project.getChildWithName (ids::Timeline), timelineEventType))
        ticks.push_back (PropertyReader::getRequiredInt64 (event, ids::tick));

    return ticks;
}

std::vector<juce::ValueTree> CommandDriver::markers() const
{
    return NodeLookup::childrenOfType (project.getChildWithName (ids::Timeline), ids::Marker);
}

// A node that is no marker of this project's Timeline.
juce::ValueTree CommandDriver::noMarker()
{
    switch (random.intBetween (0, 2))
    {
        case 0:
            return {};
        case 1:
            return juce::ValueTree (ids::Marker); // a marker of no Timeline
        default:
            return project.getChildWithName (ids::Timeline).getChildWithName (ids::TempoEvent);
    }
}

std::vector<juce::ValueTree> CommandDriver::tracks() const
{
    return NodeLookup::childrenOfType (project.getChildWithName (ids::Tracks), ids::Track);
}

std::vector<juce::ValueTree> CommandDriver::tracksWithKnobs() const
{
    std::vector<juce::ValueTree> found;

    for (const auto& track : tracks())
        if (track.getChildWithName (ids::Knobs).getChildWithName (ids::Knob).isValid())
            found.push_back (track);

    return found;
}

// "#AARRGGBB" in either case, or empty for no colour.
juce::String CommandDriver::colour()
{
    if (random.chance (20))
        return {};

    const auto* const digits = random.chance (50) ? "0123456789abcdef" : "0123456789ABCDEF";
    juce::String result ("#");

    for (int digit = 0; digit < 8; ++digit)
        result << digits[random.indexBelow (16)];

    return result;
}

// A text that is no colour.
juce::String CommandDriver::noColour()
{
    return random.oneOf (std::vector<juce::String> { "red", "#ff0000", "ff00ff00", "#ff00ff0g", "#ff00ff0000", "#", " #ff00ff00" });
}

std::vector<juce::ValueTree> CommandDriver::clips() const
{
    return NodeLookup::childrenOfType (project.getChildWithName (ids::ClipPool), ids::Clip);
}

// The number of Placements and Slots that reference the clip.
int CommandDriver::refcountOf (const juce::ValueTree& clip) const
{
    return model::TracksModel (project.getChildWithName (ids::Tracks)).getRefcount (PropertyReader::getRequiredString (clip, ids::id));
}

//==============================================================================
// Routing.

CommandChecks::Attempt CommandDriver::addPerformer()
{
    const auto name = text();
    return made ("addPerformer (" + quoted (name) + ")", true, commands::addPerformer (project, name, undoManager));
}

CommandChecks::Attempt CommandDriver::removePerformer()
{
    const auto known = idsOf (ids::Performer);
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? random.oneOf (known) : unknownId();

    return made ("removePerformer (" + quoted (id) + ")", isValidForm, commands::removePerformer (project, id, undoManager));
}

CommandChecks::Attempt CommandDriver::setPerformerName()
{
    const auto known = idsOf (ids::Performer);
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? random.oneOf (known) : unknownId();
    const auto name = text();

    return made ("setPerformerName (" + quoted (id) + ", " + quoted (name) + ")", isValidForm, commands::setPerformerName (project, id, name, undoManager));
}

CommandChecks::Attempt CommandDriver::setPerformerDevice()
{
    const auto known = idsOf (ids::Performer);
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? random.oneOf (known) : unknownId();
    const auto deviceIdentifier = text();
    const auto deviceName = text();

    return made ("setPerformerDevice (" + quoted (id) + ", " + quoted (deviceIdentifier) + ", " + quoted (deviceName) + ")", isValidForm, commands::setPerformerDevice (project, id, deviceIdentifier, deviceName, undoManager));
}

CommandChecks::Attempt CommandDriver::setPerformerChannelFilter()
{
    const auto known = idsOf (ids::Performer);
    const auto isValidForm = wantsValidForm() && ! known.empty();

    // Invalid: no such Performer, or no such filter.
    const auto hasKnownId = isValidForm || (! known.empty() && random.chance (50));
    const auto id = hasKnownId ? random.oneOf (known) : unknownId();
    const auto hasValidFilter = isValidForm || ! hasKnownId;
    const auto channelFilter = hasValidFilter ? random.intBetween (0, 16) : random.oneOf (std::vector<int> { -1, 17, 100 });

    return made ("setPerformerChannelFilter (" + quoted (id) + ", " + juce::String (channelFilter) + ")", isValidForm, commands::setPerformerChannelFilter (project, id, channelFilter, undoManager));
}

CommandChecks::Attempt CommandDriver::addPlayer()
{
    const auto name = text();
    return made ("addPlayer (" + quoted (name) + ")", true, commands::addPlayer (project, name, undoManager));
}

CommandChecks::Attempt CommandDriver::removePlayer()
{
    const auto known = idsOf (ids::Player);
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? random.oneOf (known) : unknownId();

    return made ("removePlayer (" + quoted (id) + ")", isValidForm, commands::removePlayer (project, id, undoManager));
}

CommandChecks::Attempt CommandDriver::setPlayerName()
{
    const auto known = idsOf (ids::Player);
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? random.oneOf (known) : unknownId();
    const auto name = text();

    return made ("setPlayerName (" + quoted (id) + ", " + quoted (name) + ")", isValidForm, commands::setPlayerName (project, id, name, undoManager));
}

CommandChecks::Attempt CommandDriver::setPlayerDevice()
{
    const auto known = idsOf (ids::Player);
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? random.oneOf (known) : unknownId();
    const auto deviceIdentifier = text();
    const auto deviceName = text();

    return made ("setPlayerDevice (" + quoted (id) + ", " + quoted (deviceIdentifier) + ", " + quoted (deviceName) + ")", isValidForm, commands::setPlayerDevice (project, id, deviceIdentifier, deviceName, undoManager));
}

CommandChecks::Attempt CommandDriver::setPlayerSendClock()
{
    const auto known = idsOf (ids::Player);
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? random.oneOf (known) : unknownId();
    const auto sendClock = random.chance (50);

    return made ("setPlayerSendClock (" + quoted (id) + ", " + juce::String (sendClock ? 1 : 0) + ")", isValidForm, commands::setPlayerSendClock (project, id, sendClock, undoManager));
}

//==============================================================================
// PadMap.

CommandChecks::Attempt CommandDriver::addPad()
{
    std::vector<int> freeIndexes;
    std::vector<int> takenIndexes;
    std::vector<int> freeInputPitches;
    std::vector<int> takenInputPitches;

    const auto padMap = project.getChildWithName (ids::Routing).getChildWithName (ids::PadMap);

    for (int index = 0; index < 64; ++index)
        (NodeLookup::childWithNumber (padMap, ids::Pad, ids::index, index).isValid() ? takenIndexes : freeIndexes).push_back (index);

    for (int pitch = 0; pitch <= 127; ++pitch)
        (NodeLookup::childWithNumber (padMap, ids::Pad, ids::inputPitch, pitch).isValid() ? takenInputPitches : freeInputPitches).push_back (pitch);

    const auto isValidForm = wantsValidForm() && ! freeIndexes.empty() && ! freeInputPitches.empty();

    auto index = freeIndexes.empty() ? 1000 : random.oneOf (freeIndexes);
    auto inputPitch = freeInputPitches.empty() ? 0 : random.oneOf (freeInputPitches);
    auto pitch = random.intBetween (0, 127);

    if (! isValidForm)
    {
        // One thing wrong; which, depends on what the map holds.
        const auto fault = random.intBetween (0, 4);

        if (fault == 0 && ! takenIndexes.empty())
            index = random.oneOf (takenIndexes);
        else if (fault == 1 && ! takenInputPitches.empty())
            inputPitch = random.oneOf (takenInputPitches);
        else if (fault == 2)
            inputPitch = notAPitch();
        else if (fault == 3)
            pitch = notAPitch();
        else
            index = random.oneOf (std::vector<int> { -1, -64, std::numeric_limits<int>::min() });
    }

    const auto label = text();

    return made ("addPad (" + juce::String (index) + ", " + juce::String (inputPitch) + ", " + juce::String (pitch) + ", " + quoted (label) + ")", isValidForm, commands::addPad (project, index, inputPitch, pitch, label, undoManager));
}

namespace
{

    int indexOfPad (const juce::ValueTree& pad)
    {
        return PropertyReader::getRequiredInt (pad, ids::index);
    }

    // A pad number the map does not have: one past the largest. (No
    // document here holds a pad number anywhere near the largest int.)
    int indexOfNoPad (const std::vector<juce::ValueTree>& pads)
    {
        int index = 0;

        for (const auto& pad : pads)
            index = std::max (index, indexOfPad (pad) + 1);

        return index;
    }

} // namespace

CommandChecks::Attempt CommandDriver::removePad()
{
    const auto known = pads();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto index = isValidForm ? indexOfPad (random.oneOf (known)) : indexOfNoPad (known);

    return made ("removePad (" + juce::String (index) + ")", isValidForm, commands::removePad (project, index, undoManager));
}

CommandChecks::Attempt CommandDriver::setPadInputPitch()
{
    const auto known = pads();
    const auto isValidForm = wantsValidForm() && ! known.empty();

    if (known.empty())
        return made ("setPadInputPitch (0, 60)", false, commands::setPadInputPitch (project, 0, 60, undoManager));

    const auto pad = random.oneOf (known);
    auto index = indexOfPad (pad);
    std::vector<int> pitchesOfOthers;
    std::vector<int> pitchesFreeForThisPad;

    for (int pitch = 0; pitch <= 127; ++pitch)
    {
        const auto holder = NodeLookup::childWithNumber (pad.getParent(), ids::Pad, ids::inputPitch, pitch);
        (holder.isValid() && holder != pad ? pitchesOfOthers : pitchesFreeForThisPad).push_back (pitch);
    }

    // Free for this pad includes the pitch it has.
    auto inputPitch = random.oneOf (pitchesFreeForThisPad);

    if (! isValidForm)
    {
        const auto fault = random.intBetween (0, 2);

        if (fault == 0 && ! pitchesOfOthers.empty())
            inputPitch = random.oneOf (pitchesOfOthers);
        else if (fault == 1)
            inputPitch = notAPitch();
        else
            index = indexOfNoPad (known);
    }

    return made ("setPadInputPitch (" + juce::String (index) + ", " + juce::String (inputPitch) + ")", isValidForm, commands::setPadInputPitch (project, index, inputPitch, undoManager));
}

CommandChecks::Attempt CommandDriver::setPadPitch()
{
    const auto known = pads();
    const auto isValidForm = wantsValidForm() && ! known.empty();

    // Invalid: no such pad, or no such pitch.
    const auto hasKnownPad = isValidForm || (! known.empty() && random.chance (50));
    const auto index = hasKnownPad ? indexOfPad (random.oneOf (known)) : indexOfNoPad (known);
    const auto pitch = (isValidForm || ! hasKnownPad) ? random.intBetween (0, 127) : notAPitch();

    return made ("setPadPitch (" + juce::String (index) + ", " + juce::String (pitch) + ")", isValidForm, commands::setPadPitch (project, index, pitch, undoManager));
}

CommandChecks::Attempt CommandDriver::setPadLabel()
{
    const auto known = pads();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto index = isValidForm ? indexOfPad (random.oneOf (known)) : indexOfNoPad (known);
    const auto label = text();

    return made ("setPadLabel (" + juce::String (index) + ", " + quoted (label) + ")", isValidForm, commands::setPadLabel (project, index, label, undoManager));
}

//==============================================================================
// Timeline.

namespace
{

    // A tick, later than 0, at which the list has nothing.
    std::int64_t tickWithNoneOf (const std::vector<std::int64_t>& ticks)
    {
        std::int64_t tick = 1;

        while (contains (ticks, tick))
            ++tick;

        return tick;
    }

    std::vector<std::int64_t> laterThanZero (const std::vector<std::int64_t>& ticks)
    {
        std::vector<std::int64_t> later;

        for (const auto tick : ticks)
            if (tick > 0)
                later.push_back (tick);

        return later;
    }

} // namespace

CommandChecks::Attempt CommandDriver::setTempo()
{
    const auto isValidForm = wantsValidForm();
    const auto known = ticksOf (ids::TempoEvent);

    // As often a tempo that is there as a new one.
    auto atTick = random.chance (50) ? random.oneOf (known) : tick();
    auto bpm = tempo();

    if (! isValidForm)
    {
        if (random.chance (50))
            atTick = negativeTick();
        else
            bpm = noTempo();
    }

    return made ("setTempo (" + juce::String (atTick) + ", " + juce::String (bpm, 9) + ")", isValidForm, commands::setTempo (project, atTick, bpm, undoManager));
}

CommandChecks::Attempt CommandDriver::removeTempoEvent()
{
    const auto known = ticksOf (ids::TempoEvent);
    const auto removable = laterThanZero (known);
    const auto isValidForm = wantsValidForm() && ! removable.empty();

    std::int64_t atTick = 0;

    if (isValidForm)
        atTick = random.oneOf (removable);
    else if (random.chance (50))
        atTick = random.chance (50) ? tickWithNoneOf (known) : negativeTick();

    return made ("removeTempoEvent (" + juce::String (atTick) + ")", isValidForm, commands::removeTempoEvent (project, atTick, undoManager));
}

CommandChecks::Attempt CommandDriver::setTimeSignature()
{
    const auto isValidForm = wantsValidForm();
    const auto known = ticksOf (ids::TimeSigEvent);

    auto atTick = random.chance (50) ? random.oneOf (known) : tick();
    auto numerator = random.intBetween (1, 32);
    auto denominator = 1 << random.intBetween (0, 6);

    if (! isValidForm)
    {
        const auto fault = random.intBetween (0, 2);

        if (fault == 0)
            atTick = negativeTick();
        else if (fault == 1)
            numerator = random.oneOf (std::vector<int> { 0, -4, 33, 1000 });
        else
            denominator = random.oneOf (std::vector<int> { 0, -4, 3, 6, 128, std::numeric_limits<int>::min() });
    }

    return made ("setTimeSignature (" + juce::String (atTick) + ", " + juce::String (numerator) + ", " + juce::String (denominator) + ")", isValidForm, commands::setTimeSignature (project, atTick, numerator, denominator, undoManager));
}

CommandChecks::Attempt CommandDriver::removeTimeSigEvent()
{
    const auto known = ticksOf (ids::TimeSigEvent);
    const auto removable = laterThanZero (known);
    const auto isValidForm = wantsValidForm() && ! removable.empty();

    std::int64_t atTick = 0;

    if (isValidForm)
        atTick = random.oneOf (removable);
    else if (random.chance (50))
        atTick = random.chance (50) ? tickWithNoneOf (known) : negativeTick();

    return made ("removeTimeSigEvent (" + juce::String (atTick) + ")", isValidForm, commands::removeTimeSigEvent (project, atTick, undoManager));
}

CommandChecks::Attempt CommandDriver::addMarker()
{
    const auto isValidForm = wantsValidForm();
    const auto atTick = isValidForm ? tick() : negativeTick();
    const auto name = text();

    return made ("addMarker (" + juce::String (atTick) + ", " + quoted (name) + ")", isValidForm, commands::addMarker (project, atTick, name, undoManager));
}

CommandChecks::Attempt CommandDriver::removeMarker()
{
    const auto known = markers();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto marker = isValidForm ? random.oneOf (known) : noMarker();

    return made ("removeMarker (" + juce::String (isValidForm ? "a marker" : "no marker") + ")", isValidForm, commands::removeMarker (project, marker, undoManager));
}

CommandChecks::Attempt CommandDriver::moveMarker()
{
    const auto known = markers();
    const auto isValidForm = wantsValidForm() && ! known.empty();

    // Invalid: no marker, or a marker and no position.
    const auto hasMarker = isValidForm || (! known.empty() && random.chance (50));
    const auto marker = hasMarker ? random.oneOf (known) : noMarker();
    const auto toTick = (isValidForm || ! hasMarker) ? tick() : negativeTick();

    return made ("moveMarker (" + juce::String (hasMarker ? "a marker" : "no marker") + ", " + juce::String (toTick) + ")", isValidForm, commands::moveMarker (project, marker, toTick, undoManager));
}

CommandChecks::Attempt CommandDriver::renameMarker()
{
    const auto known = markers();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto marker = isValidForm ? random.oneOf (known) : noMarker();
    const auto name = text();

    return made ("renameMarker (" + juce::String (isValidForm ? "a marker" : "no marker") + ", " + quoted (name) + ")", isValidForm, commands::renameMarker (project, marker, name, undoManager));
}

//==============================================================================
// Session.

CommandChecks::Attempt CommandDriver::setLaunchQuantise()
{
    const auto isValidForm = wantsValidForm();
    const auto ticks = isValidForm ? (random.chance (50) ? random.oneOf (std::vector<std::int64_t> { 0, 960, 1920, 3840 }) : tick()) : negativeTick();

    return made ("setLaunchQuantise (" + juce::String (ticks) + ")", isValidForm, commands::setLaunchQuantise (project, ticks, undoManager));
}

CommandChecks::Attempt CommandDriver::setSceneName()
{
    const auto isValidForm = wantsValidForm();
    const auto sceneIndex = isValidForm ? random.intBetween (0, 20) : random.oneOf (std::vector<int> { -1, -16, std::numeric_limits<int>::min() });
    const auto name = text();

    return made ("setSceneName (" + juce::String (sceneIndex) + ", " + quoted (name) + ")", isValidForm, commands::setSceneName (project, sceneIndex, name, undoManager));
}

//==============================================================================
// Tracks and their knobs.

namespace
{

    juce::String idOf (const juce::ValueTree& node)
    {
        return PropertyReader::getRequiredString (node, ids::id);
    }

    std::vector<juce::ValueTree> knobsOf (const juce::ValueTree& track)
    {
        return NodeLookup::childrenOfType (track.getChildWithName (ids::Knobs), ids::Knob);
    }

    int controllerOf (const juce::ValueTree& knob)
    {
        return PropertyReader::getRequiredInt (knob, ids::cc);
    }

    // The controller numbers the track has a knob for, and those it has
    // none for.
    void sortControllers (const juce::ValueTree& track, std::vector<int>& taken, std::vector<int>& free)
    {
        for (int cc = 0; cc <= 127; ++cc)
            (NodeLookup::childWithNumber (track.getChildWithName (ids::Knobs), ids::Knob, ids::cc, cc).isValid() ? taken : free).push_back (cc);
    }

    int positionThatIsNotThere (RandomSource& random, int count)
    {
        return random.oneOf (std::vector<int> { -1, count, count + 7, std::numeric_limits<int>::min(), std::numeric_limits<int>::max() });
    }

} // namespace

CommandChecks::Attempt CommandDriver::addTrack()
{
    const auto name = text();
    return made ("addTrack (" + quoted (name) + ")", true, commands::addTrack (project, name, undoManager));
}

CommandChecks::Attempt CommandDriver::removeTrack()
{
    const auto known = tracks();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? idOf (random.oneOf (known)) : unknownId();

    return made ("removeTrack (" + quoted (id) + ")", isValidForm, commands::removeTrack (project, id, undoManager));
}

CommandChecks::Attempt CommandDriver::moveTrack()
{
    const auto known = tracks();
    const auto count = static_cast<int> (known.size());
    const auto isValidForm = wantsValidForm() && ! known.empty();

    // Invalid: no such track, or a track and no such position.
    const auto hasKnownId = isValidForm || (! known.empty() && random.chance (50));
    const auto id = hasKnownId ? idOf (random.oneOf (known)) : unknownId();
    const auto toPosition = (isValidForm || ! hasKnownId) ? random.intBetween (0, std::max (count - 1, 0)) : positionThatIsNotThere (random, count);

    return made ("moveTrack (" + quoted (id) + ", " + juce::String (toPosition) + ")", isValidForm, commands::moveTrack (project, id, toPosition, undoManager));
}

CommandChecks::Attempt CommandDriver::setTrackName()
{
    const auto known = tracks();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? idOf (random.oneOf (known)) : unknownId();
    const auto name = text();

    return made ("setTrackName (" + quoted (id) + ", " + quoted (name) + ")", isValidForm, commands::setTrackName (project, id, name, undoManager));
}

CommandChecks::Attempt CommandDriver::setTrackColour()
{
    const auto known = tracks();
    const auto isValidForm = wantsValidForm() && ! known.empty();

    // Invalid: no such track, or a track and no colour.
    const auto hasKnownId = isValidForm || (! known.empty() && random.chance (50));
    const auto id = hasKnownId ? idOf (random.oneOf (known)) : unknownId();
    const auto value = (isValidForm || ! hasKnownId) ? colour() : noColour();

    return made ("setTrackColour (" + quoted (id) + ", " + quoted (value) + ")", isValidForm, commands::setTrackColour (project, id, value, undoManager));
}

CommandChecks::Attempt CommandDriver::setTrackPort (const char* name, const juce::Identifier& portType, commands::CommandResult (*command) (juce::ValueTree, const juce::String&, const juce::String&, juce::UndoManager&))
{
    const auto known = tracks();
    const auto ports = idsOf (portType);
    const auto isValidForm = wantsValidForm() && ! known.empty();

    // Invalid: no such track, or a track and no such port.
    const auto hasKnownId = isValidForm || (! known.empty() && random.chance (50));
    const auto id = hasKnownId ? idOf (random.oneOf (known)) : unknownId();

    juce::String portId; // empty = unassigned, which is valid

    if (isValidForm || ! hasKnownId)
    {
        if (! ports.empty() && random.chance (75))
            portId = random.oneOf (ports);
    }
    else
    {
        // Never empty here: an id in the right form that no port has.
        do
            portId = unknownId();
        while (portId.length() != 32);
    }

    return made (juce::String (name) + " (" + quoted (id) + ", " + quoted (portId) + ")", isValidForm, command (project, id, portId, undoManager));
}

CommandChecks::Attempt CommandDriver::setTrackPlayer()
{
    return setTrackPort ("setTrackPlayer", ids::Player, &commands::setTrackPlayer);
}

CommandChecks::Attempt CommandDriver::setTrackPerformer()
{
    return setTrackPort ("setTrackPerformer", ids::Performer, &commands::setTrackPerformer);
}

CommandChecks::Attempt CommandDriver::setTrackChannel()
{
    const auto known = tracks();
    const auto isValidForm = wantsValidForm() && ! known.empty();

    // Invalid: no such track, or a track and no such channel.
    const auto hasKnownId = isValidForm || (! known.empty() && random.chance (50));
    const auto id = hasKnownId ? idOf (random.oneOf (known)) : unknownId();
    const auto channel = (isValidForm || ! hasKnownId) ? random.intBetween (1, 16) : random.oneOf (std::vector<int> { 0, -1, 17, 100, std::numeric_limits<int>::min() });

    return made ("setTrackChannel (" + quoted (id) + ", " + juce::String (channel) + ")", isValidForm, commands::setTrackChannel (project, id, channel, undoManager));
}

CommandChecks::Attempt CommandDriver::setTrackFlag (const char* name, commands::CommandResult (*command) (juce::ValueTree, const juce::String&, bool, juce::UndoManager&))
{
    const auto known = tracks();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? idOf (random.oneOf (known)) : unknownId();
    const auto flag = random.chance (50);

    return made (juce::String (name) + " (" + quoted (id) + ", " + juce::String (flag ? 1 : 0) + ")", isValidForm, command (project, id, flag, undoManager));
}

CommandChecks::Attempt CommandDriver::setTrackMute()
{
    return setTrackFlag ("setTrackMute", &commands::setTrackMute);
}

CommandChecks::Attempt CommandDriver::setTrackSolo()
{
    return setTrackFlag ("setTrackSolo", &commands::setTrackSolo);
}

CommandChecks::Attempt CommandDriver::addKnob()
{
    const auto known = tracks();
    const auto label = text();

    if (known.empty())
    {
        const auto id = unknownId();
        return made ("addKnob (" + quoted (id) + ", 74, " + quoted (label) + ")", false, commands::addKnob (project, id, 74, label, undoManager));
    }

    const auto track = random.oneOf (known);
    std::vector<int> taken;
    std::vector<int> free;
    sortControllers (track, taken, free);

    const auto isValidForm = wantsValidForm() && ! free.empty();

    auto id = idOf (track);
    auto cc = free.empty() ? 0 : random.oneOf (free);

    if (! isValidForm)
    {
        const auto fault = random.intBetween (0, 2);

        if (fault == 0 && ! taken.empty())
            cc = random.oneOf (taken);
        else if (fault == 1 || free.empty())
            cc = notAPitch(); // a controller number has the range of a pitch
        else
            id = unknownId();
    }

    return made ("addKnob (" + quoted (id) + ", " + juce::String (cc) + ", " + quoted (label) + ")", isValidForm, commands::addKnob (project, id, cc, label, undoManager));
}

CommandChecks::Attempt CommandDriver::removeKnob()
{
    const auto withKnobs = tracksWithKnobs();
    const auto isValidForm = wantsValidForm() && ! withKnobs.empty();

    juce::String id = unknownId();
    int cc = random.intBetween (0, 127);

    if (isValidForm)
    {
        const auto track = random.oneOf (withKnobs);
        id = idOf (track);
        cc = controllerOf (random.oneOf (knobsOf (track)));
    }
    else if (const auto known = tracks(); ! known.empty() && random.chance (50))
    {
        // A track, and a controller it has no knob for.
        const auto track = random.oneOf (known);
        std::vector<int> taken;
        std::vector<int> free;
        sortControllers (track, taken, free);

        id = idOf (track);
        cc = free.empty() ? notAPitch() : random.oneOf (free);
    }

    return made ("removeKnob (" + quoted (id) + ", " + juce::String (cc) + ")", isValidForm, commands::removeKnob (project, id, cc, undoManager));
}

CommandChecks::Attempt CommandDriver::moveKnob()
{
    const auto withKnobs = tracksWithKnobs();
    const auto isValidForm = wantsValidForm() && ! withKnobs.empty();

    juce::String id = unknownId();
    int cc = random.intBetween (0, 127);
    int toPosition = 0;

    if (isValidForm || (! withKnobs.empty() && random.chance (50)))
    {
        // Invalid: a knob, and a position that is not there.
        const auto track = random.oneOf (withKnobs);
        const auto knobs = knobsOf (track);
        const auto count = static_cast<int> (knobs.size());

        id = idOf (track);
        cc = controllerOf (random.oneOf (knobs));
        toPosition = isValidForm ? random.intBetween (0, count - 1) : positionThatIsNotThere (random, count);
    }

    return made ("moveKnob (" + quoted (id) + ", " + juce::String (cc) + ", " + juce::String (toPosition) + ")", isValidForm, commands::moveKnob (project, id, cc, toPosition, undoManager));
}

CommandChecks::Attempt CommandDriver::setKnobCc()
{
    const auto withKnobs = tracksWithKnobs();
    const auto isValidForm = wantsValidForm() && ! withKnobs.empty();

    if (withKnobs.empty())
    {
        const auto id = unknownId();
        return made ("setKnobCc (" + quoted (id) + ", 74, 71)", false, commands::setKnobCc (project, id, 74, 71, undoManager));
    }

    const auto track = random.oneOf (withKnobs);
    std::vector<int> taken;
    std::vector<int> free;
    sortControllers (track, taken, free);

    const auto id = idOf (track);
    auto cc = random.oneOf (taken);

    // Free for this knob: every number no knob has, and its own.
    auto newCc = (free.empty() || random.chance (10)) ? cc : random.oneOf (free);

    if (! isValidForm)
    {
        std::vector<int> ofOthers;

        for (const auto other : taken)
            if (other != cc)
                ofOthers.push_back (other);

        const auto fault = random.intBetween (0, 2);

        if (fault == 0 && ! ofOthers.empty())
            newCc = random.oneOf (ofOthers);
        else if (fault == 1 && ! free.empty())
            cc = random.oneOf (free); // no knob for it
        else
            newCc = notAPitch();
    }

    return made ("setKnobCc (" + quoted (id) + ", " + juce::String (cc) + ", " + juce::String (newCc) + ")", isValidForm, commands::setKnobCc (project, id, cc, newCc, undoManager));
}

CommandChecks::Attempt CommandDriver::setKnobLabel()
{
    const auto withKnobs = tracksWithKnobs();
    const auto isValidForm = wantsValidForm() && ! withKnobs.empty();
    const auto label = text();

    juce::String id = unknownId();
    int cc = random.intBetween (0, 127);

    if (isValidForm)
    {
        const auto track = random.oneOf (withKnobs);
        id = idOf (track);
        cc = controllerOf (random.oneOf (knobsOf (track)));
    }

    return made ("setKnobLabel (" + quoted (id) + ", " + juce::String (cc) + ", " + quoted (label) + ")", isValidForm, commands::setKnobLabel (project, id, cc, label, undoManager));
}

//==============================================================================
// The clip pool.

CommandChecks::Attempt CommandDriver::addEventClip()
{
    const auto isValidForm = wantsValidForm();
    const auto name = text();

    // A loop of whole bars, of any length, or as long as an int64 allows.
    std::int64_t loopLength = random.chance (60) ? 3840 * random.between (1, 8) : random.between (1, 40000);

    if (random.chance (3))
        loopLength = std::numeric_limits<std::int64_t>::max();

    if (! isValidForm)
        loopLength = random.chance (50) ? 0 : negativeTick();

    return made ("addEventClip (" + quoted (name) + ", " + juce::String (loopLength) + ")", isValidForm, commands::addEventClip (project, name, loopLength, undoManager));
}

CommandChecks::Attempt CommandDriver::addStepClip()
{
    const auto name = text();
    return made ("addStepClip (" + quoted (name) + ")", true, commands::addStepClip (project, name, undoManager));
}

CommandChecks::Attempt CommandDriver::setClipName()
{
    const auto known = clips();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto name = text();
    commands::EditContext editContext (policy());

    // Invalid: no such clip, or a clip and a context that names a reference
    // which is none of the clip's.
    const auto hasKnownId = isValidForm || (! known.empty() && random.chance (50));
    const auto clip = hasKnownId ? random.oneOf (known) : juce::ValueTree();
    const auto id = hasKnownId ? idOf (clip) : unknownId();

    juce::String gesture = "at pool level";

    if (isValidForm)
        gesture = nameReferencesOf (clip, editContext);
    else if (hasKnownId)
        gesture = nameAWrongReference (id, editContext);

    return made ("setClipName (" + quoted (id) + ", " + quoted (name) + ", " + gesture + ")", isValidForm, commands::setClipName (project, id, name, editContext, undoManager));
}

CommandChecks::Attempt CommandDriver::setClipColour()
{
    const auto known = clips();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    commands::EditContext editContext (policy());

    // Invalid: no such clip; a clip and no colour; a clip and a context
    // that names a reference which is none of the clip's.
    const auto hasKnownId = isValidForm || (! known.empty() && random.chance (60));
    const auto clip = hasKnownId ? random.oneOf (known) : juce::ValueTree();
    const auto id = hasKnownId ? idOf (clip) : unknownId();
    const auto hasWrongReference = ! isValidForm && hasKnownId && random.chance (50);
    const auto value = (isValidForm || ! hasKnownId || hasWrongReference) ? colour() : noColour();

    juce::String gesture = "at pool level";

    if (isValidForm)
        gesture = nameReferencesOf (clip, editContext);
    else if (hasWrongReference)
        gesture = nameAWrongReference (id, editContext);

    return made ("setClipColour (" + quoted (id) + ", " + quoted (value) + ", " + gesture + ")", isValidForm, commands::setClipColour (project, id, value, editContext, undoManager));
}

CommandChecks::Attempt CommandDriver::removeClip()
{
    std::vector<juce::ValueTree> unused;
    std::vector<juce::ValueTree> inUse;

    for (const auto& clip : clips())
        (refcountOf (clip) == 0 ? unused : inUse).push_back (clip);

    const auto isValidForm = wantsValidForm() && ! unused.empty();

    // Invalid: a clip that a Placement or a Slot references, or no such
    // clip.
    juce::String id;

    if (isValidForm)
        id = idOf (random.oneOf (unused));
    else if (! inUse.empty() && random.chance (60))
        id = idOf (random.oneOf (inUse));
    else
        id = unknownId();

    return made ("removeClip (" + quoted (id) + ")", isValidForm, commands::removeClip (project, id, undoManager));
}

CommandChecks::Attempt CommandDriver::removeUnusedClips()
{
    return made ("removeUnusedClips ()", true, commands::removeUnusedClips (project, undoManager));
}

//==============================================================================
// Placements.

namespace
{

    // The driver's documents hold no placement anywhere near the largest
    // tick, so the plain sums here cannot overflow.
    std::int64_t startOf (const juce::ValueTree& placement)
    {
        return PropertyReader::getRequiredInt64 (placement, ids::start);
    }

    std::int64_t lengthOf (const juce::ValueTree& placement)
    {
        return PropertyReader::getRequiredInt64 (placement, ids::length);
    }

    std::int64_t endOf (const juce::ValueTree& placement)
    {
        return startOf (placement) + lengthOf (placement);
    }

    // The other placements of the same track or lane.
    std::vector<juce::ValueTree> siblingsOf (const juce::ValueTree& parent, const juce::ValueTree& placement)
    {
        std::vector<juce::ValueTree> siblings;

        for (const auto& sibling : NodeLookup::childrenOfType (parent, ids::Placement))
            if (sibling != placement)
                siblings.push_back (sibling);

        return siblings;
    }

    // True when no placement under `parent`, `moving` apart, has a tick in
    // [start, start + length). Written here a second time on purpose: it is
    // what the commands are held to.
    bool isFree (const juce::ValueTree& parent, std::int64_t start, std::int64_t length, const juce::ValueTree& moving)
    {
        for (const auto& sibling : siblingsOf (parent, moving))
            if (start < endOf (sibling) && startOf (sibling) < start + length)
                return false;

        return true;
    }

    // A start at which a placement of this length is in the way of
    // `sibling`: any from one that only just reaches into it to its last
    // tick, and never a negative one.
    std::int64_t startInTheWayOf (RandomSource& random, const juce::ValueTree& sibling, std::int64_t length)
    {
        return random.between (std::max (startOf (sibling) - length + 1, std::int64_t { 0 }), endOf (sibling) - 1);
    }

    // A start that no placement can have, whatever is free: a negative one,
    // or one from which any length passes the largest tick.
    std::int64_t startThatCannotBe (RandomSource& random, std::int64_t negative)
    {
        return random.chance (50) ? negative : std::numeric_limits<std::int64_t>::max() - random.between (0, 3);
    }

    // The largest length there is would be none of these: a placement at
    // tick 0 with nothing behind it may have it.
    std::int64_t noLength (RandomSource& random)
    {
        return random.oneOf (std::vector<std::int64_t> { 0, -1, -960, std::numeric_limits<std::int64_t>::min() });
    }

} // namespace

// Every placement of the project: the tracks' own and those on their
// automation lanes.
std::vector<juce::ValueTree> CommandDriver::placements() const
{
    auto found = ownPlacements();

    for (const auto& track : tracks())
        for (const auto& lane : NodeLookup::childrenOfType (track.getChildWithName (ids::AutomationLanes), ids::AutomationLane))
            for (const auto& placement : NodeLookup::childrenOfType (lane, ids::Placement))
                found.push_back (placement);

    return found;
}

std::vector<juce::ValueTree> CommandDriver::ownPlacements() const
{
    std::vector<juce::ValueTree> found;

    for (const auto& track : tracks())
        for (const auto& placement : NodeLookup::childrenOfType (track.getChildWithName (ids::Placements), ids::Placement))
            found.push_back (placement);

    return found;
}

// A start at which a placement of this length is in nobody's way under
// `parent`: anywhere, if that is free, and else behind the last one.
std::int64_t CommandDriver::freeStartFor (const juce::ValueTree& parent, std::int64_t length, const juce::ValueTree& moving)
{
    const auto anywhere = tick();

    if (isFree (parent, anywhere, length, moving))
        return anywhere;

    std::int64_t behindTheLast = 0;

    for (const auto& sibling : siblingsOf (parent, moving))
        behindTheLast = std::max (behindTheLast, endOf (sibling));

    // Right behind it, or after a gap.
    return behindTheLast + (random.chance (50) ? 0 : random.between (1, 3840));
}

CommandChecks::Attempt CommandDriver::addPlacement()
{
    const auto knownTracks = tracks();
    const auto knownClips = clips();
    const auto isValidForm = wantsValidForm() && ! knownTracks.empty() && ! knownClips.empty();

    // A whole number of bars, or any length.
    auto length = random.chance (50) ? 3840 * random.between (1, 4) : random.between (1, 8000);

    juce::String trackId = unknownId();
    juce::String clipId = unknownId();
    std::int64_t start = tick();

    if (! knownTracks.empty() && ! knownClips.empty())
    {
        const auto track = random.oneOf (knownTracks);
        const auto own = track.getChildWithName (ids::Placements);
        const auto taken = NodeLookup::childrenOfType (own, ids::Placement);

        trackId = idOf (track);
        clipId = idOf (random.oneOf (knownClips));
        start = freeStartFor (own, length, {});

        if (! isValidForm)
        {
            // One thing wrong.
            const auto fault = random.intBetween (0, 4);

            if (fault == 0 && ! taken.empty())
                start = startInTheWayOf (random, random.oneOf (taken), length);
            else if (fault == 1)
                start = startThatCannotBe (random, negativeTick());
            else if (fault == 2)
                length = noLength (random);
            else if (fault == 3)
                clipId = unknownId();
            else
                trackId = unknownId();
        }
    }

    return made ("addPlacement (" + quoted (trackId) + ", " + quoted (clipId) + ", " + juce::String (start) + ", " + juce::String (length) + ")", isValidForm, commands::addPlacement (project, trackId, clipId, start, length, undoManager));
}

CommandChecks::Attempt CommandDriver::movePlacement()
{
    const auto known = placements();
    const auto isValidForm = wantsValidForm() && ! known.empty();

    juce::String id = unknownId();
    std::int64_t toStart = tick();

    if (isValidForm || (! known.empty() && random.chance (70)))
    {
        const auto placement = random.oneOf (known);
        const auto parent = placement.getParent();
        const auto siblings = siblingsOf (parent, placement);

        id = idOf (placement);

        // Now and then to where it is, or by less than its length: it is
        // not in its own way.
        if (random.chance (15))
            toStart = startOf (placement);
        else if (random.chance (15) && isFree (parent, startOf (placement) + lengthOf (placement) / 2, lengthOf (placement), placement))
            toStart = startOf (placement) + lengthOf (placement) / 2;
        else
            toStart = freeStartFor (parent, lengthOf (placement), placement);

        if (! isValidForm)
        {
            if (! siblings.empty() && random.chance (50))
                toStart = startInTheWayOf (random, random.oneOf (siblings), lengthOf (placement));
            else
                toStart = startThatCannotBe (random, negativeTick());
        }
    }

    return made ("movePlacement (" + quoted (id) + ", " + juce::String (toStart) + ")", isValidForm, commands::movePlacement (project, id, toStart, undoManager));
}

CommandChecks::Attempt CommandDriver::movePlacementToTrack()
{
    const auto known = ownPlacements();
    const auto knownTracks = tracks();
    const auto isValidForm = wantsValidForm() && ! known.empty();

    juce::String id = unknownId();
    juce::String toTrackId = knownTracks.empty() ? unknownId() : idOf (random.oneOf (knownTracks));
    std::int64_t toStart = tick();

    if (isValidForm || (! known.empty() && random.chance (80)))
    {
        // A track with a placement of its own is there. Often the track
        // the placement is on.
        const auto placement = random.oneOf (known);
        const auto toTrack = random.chance (30) ? placement.getParent().getParent() : random.oneOf (knownTracks);
        const auto to = toTrack.getChildWithName (ids::Placements);
        const auto inTheWay = siblingsOf (to, placement);

        id = idOf (placement);
        toTrackId = idOf (toTrack);
        toStart = freeStartFor (to, lengthOf (placement), placement);

        if (! isValidForm)
        {
            const auto fault = random.intBetween (0, 3);

            // The placements on lanes are those that are not a track's own.
            std::vector<juce::ValueTree> onLanes;

            for (const auto& any : placements())
                if (any.getParent().hasType (ids::AutomationLane))
                    onLanes.push_back (any);

            if (fault == 0 && ! inTheWay.empty())
                toStart = startInTheWayOf (random, random.oneOf (inTheWay), lengthOf (placement));
            else if (fault == 1 && ! onLanes.empty())
                id = idOf (random.oneOf (onLanes));
            else if (fault == 2)
                toStart = startThatCannotBe (random, negativeTick());
            else
                toTrackId = unknownId();
        }
    }

    return made ("movePlacementToTrack (" + quoted (id) + ", " + quoted (toTrackId) + ", " + juce::String (toStart) + ")", isValidForm, commands::movePlacementToTrack (project, id, toTrackId, toStart, undoManager));
}

CommandChecks::Attempt CommandDriver::resizePlacement()
{
    const auto known = placements();
    const auto isValidForm = wantsValidForm() && ! known.empty();

    juce::String id = unknownId();
    std::int64_t length = random.between (1, 8000);

    if (isValidForm || (! known.empty() && random.chance (70)))
    {
        const auto placement = random.oneOf (known);
        const auto parent = placement.getParent();

        id = idOf (placement);

        // Shorter than it is always fits; longer only into free ticks.
        if (! isFree (parent, startOf (placement), length, placement))
            length = random.between (1, lengthOf (placement));

        if (! isValidForm)
        {
            // The siblings a longer placement would reach.
            std::vector<juce::ValueTree> behind;

            for (const auto& sibling : siblingsOf (parent, placement))
                if (startOf (sibling) >= endOf (placement))
                    behind.push_back (sibling);

            if (! behind.empty() && random.chance (50))
                length = startOf (random.oneOf (behind)) - startOf (placement) + random.between (1, 100);
            else
                length = noLength (random);
        }
    }

    return made ("resizePlacement (" + quoted (id) + ", " + juce::String (length) + ")", isValidForm, commands::resizePlacement (project, id, length, undoManager));
}

CommandChecks::Attempt CommandDriver::splitPlacement()
{
    // A placement of one tick has no tick at which it can be split.
    std::vector<juce::ValueTree> splittable;

    for (const auto& placement : placements())
        if (lengthOf (placement) > 1)
            splittable.push_back (placement);

    const auto known = placements();
    const auto isValidForm = wantsValidForm() && ! splittable.empty();

    juce::String id = unknownId();
    std::int64_t atTick = tick();

    if (isValidForm)
    {
        const auto placement = random.oneOf (splittable);

        id = idOf (placement);
        atTick = startOf (placement) + random.between (1, lengthOf (placement) - 1);
    }
    else if (! known.empty() && random.chance (70))
    {
        // A placement, and a tick that is not inside it.
        const auto placement = random.oneOf (known);

        id = idOf (placement);

        switch (random.intBetween (0, 3))
        {
            case 0:
                atTick = startOf (placement);
                break;
            case 1:
                atTick = endOf (placement);
                break;
            case 2:
                atTick = endOf (placement) + tick();
                break;
            default:
                atTick = startOf (placement) > 0 && random.chance (50) ? random.between (0, startOf (placement) - 1) : negativeTick();
                break;
        }
    }

    return made ("splitPlacement (" + quoted (id) + ", " + juce::String (atTick) + ")", isValidForm, commands::splitPlacement (project, id, atTick, undoManager));
}

CommandChecks::Attempt CommandDriver::removePlacement()
{
    const auto known = placements();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? idOf (random.oneOf (known)) : unknownId();

    return made ("removePlacement (" + quoted (id) + ")", isValidForm, commands::removePlacement (project, id, undoManager));
}

//==============================================================================
// The session grid.

namespace
{

    const int largestRow = std::numeric_limits<int>::max();

    int sceneOf (const juce::ValueTree& slot)
    {
        return PropertyReader::getRequiredInt (slot, ids::scene);
    }

    int negativeRow (RandomSource& random)
    {
        return random.oneOf (std::vector<int> { -1, -2, -1000, std::numeric_limits<int>::min() });
    }

} // namespace

// Every occupied slot of every track.
std::vector<juce::ValueTree> CommandDriver::slots() const
{
    std::vector<juce::ValueTree> found;

    for (const auto& track : tracks())
        for (const auto& slot : NodeLookup::childrenOfType (track.getChildWithName (ids::Slots), ids::Slot))
            found.push_back (slot);

    return found;
}

// True when a Slot or a Scene stands in the last row an int can number;
// no row can then be inserted before it.
bool CommandDriver::isLargestRowInUse() const
{
    for (const auto& slot : slots())
        if (sceneOf (slot) == largestRow)
            return true;

    return NodeLookup::childWithNumber (project.getChildWithName (ids::Session), ids::Scene, ids::index, largestRow).isValid();
}

// A row of the session grid: mostly one of the first few, where the slots
// and the names are, now and then one of the last there are.
int CommandDriver::row()
{
    if (random.chance (95))
        return random.intBetween (0, 9);

    return largestRow - random.intBetween (0, 2);
}

CommandChecks::Attempt CommandDriver::setSlot()
{
    const auto knownTracks = tracks();
    const auto knownClips = clips();
    const auto isValidForm = wantsValidForm() && ! knownTracks.empty() && ! knownClips.empty();

    juce::String trackId = unknownId();
    juce::String clipId = unknownId();
    int scene = row();

    if (! knownTracks.empty() && ! knownClips.empty())
    {
        trackId = idOf (random.oneOf (knownTracks));
        clipId = idOf (random.oneOf (knownClips));

        if (! isValidForm)
        {
            // One thing wrong.
            const auto fault = random.intBetween (0, 2);

            if (fault == 0)
                scene = negativeRow (random);
            else if (fault == 1)
                clipId = unknownId();
            else
                trackId = unknownId();
        }
    }

    return made ("setSlot (" + quoted (trackId) + ", " + juce::String (scene) + ", " + quoted (clipId) + ")", isValidForm, commands::setSlot (project, trackId, scene, clipId, undoManager));
}

CommandChecks::Attempt CommandDriver::clearSlot()
{
    const auto known = slots();
    const auto knownTracks = tracks();
    const auto isValidForm = wantsValidForm() && ! known.empty();

    juce::String trackId = unknownId();
    int scene = row();

    if (isValidForm)
    {
        const auto slot = random.oneOf (known);

        trackId = idOf (slot.getParent().getParent());
        scene = sceneOf (slot);
    }
    else if (! knownTracks.empty() && random.chance (60))
    {
        // A track, and a row in which its slot is empty.
        const auto track = random.oneOf (knownTracks);
        const auto ownSlots = track.getChildWithName (ids::Slots);

        trackId = idOf (track);

        do
            scene = random.chance (80) ? row() : negativeRow (random);
        while (NodeLookup::childWithNumber (ownSlots, ids::Slot, ids::scene, scene).isValid());
    }

    return made ("clearSlot (" + quoted (trackId) + ", " + juce::String (scene) + ")", isValidForm, commands::clearSlot (project, trackId, scene, undoManager));
}

CommandChecks::Attempt CommandDriver::insertScene()
{
    // While the last row is in use there is no valid form.
    const auto isValidForm = wantsValidForm() && ! isLargestRowInUse();

    int atScene = row();

    if (! isValidForm && ! (isLargestRowInUse() && random.chance (50)))
        atScene = negativeRow (random);

    return made ("insertScene (" + juce::String (atScene) + ")", isValidForm, commands::insertScene (project, atScene, undoManager));
}

CommandChecks::Attempt CommandDriver::removeScene()
{
    const auto isValidForm = wantsValidForm();
    const auto scene = isValidForm ? row() : negativeRow (random);

    return made ("removeScene (" + juce::String (scene) + ")", isValidForm, commands::removeScene (project, scene, undoManager));
}

CommandChecks::Attempt CommandDriver::moveScene()
{
    const auto isValidForm = wantsValidForm();

    int fromScene = row();
    int toScene = row();

    if (! isValidForm)
        (random.chance (50) ? fromScene : toScene) = negativeRow (random);

    return made ("moveScene (" + juce::String (fromScene) + ", " + juce::String (toScene) + ")", isValidForm, commands::moveScene (project, fromScene, toScene, undoManager));
}

//==============================================================================
// Automation lanes.

namespace
{

    // True when an automation lane may reference the clip: an event clip
    // with no note and no event of another kind than cc (invariant 9).
    // Written here a second time on purpose: it is what the command is held
    // to.
    bool isFitForALane (const juce::ValueTree& clip)
    {
        if (PropertyReader::getRequiredString (clip, ids::kind) != "event")
            return false;

        const auto content = clip.getChildWithName (ids::Events);

        if (content.getChildWithName (ids::Note).isValid())
            return false;

        for (const auto& event : NodeLookup::childrenOfType (content, ids::Event))
            if (PropertyReader::getRequiredString (event, ids::kind) != "cc")
                return false;

        return true;
    }

} // namespace

// Every automation lane of every track.
std::vector<juce::ValueTree> CommandDriver::lanes() const
{
    std::vector<juce::ValueTree> found;

    for (const auto& track : tracks())
        for (const auto& lane : NodeLookup::childrenOfType (track.getChildWithName (ids::AutomationLanes), ids::AutomationLane))
            found.push_back (lane);

    return found;
}

CommandChecks::Attempt CommandDriver::addAutomationLane()
{
    const auto known = tracks();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? idOf (random.oneOf (known)) : unknownId();
    const auto name = text();

    return made ("addAutomationLane (" + quoted (id) + ", " + quoted (name) + ")", isValidForm, commands::addAutomationLane (project, id, name, undoManager));
}

CommandChecks::Attempt CommandDriver::removeAutomationLane()
{
    const auto known = lanes();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? idOf (random.oneOf (known)) : unknownId();

    return made ("removeAutomationLane (" + quoted (id) + ")", isValidForm, commands::removeAutomationLane (project, id, undoManager));
}

CommandChecks::Attempt CommandDriver::moveAutomationLane()
{
    const auto known = lanes();
    const auto isValidForm = wantsValidForm() && ! known.empty();

    juce::String id = unknownId();
    int toPosition = 0;

    if (isValidForm || (! known.empty() && random.chance (50)))
    {
        // Invalid: a lane, and a position that is not there among the
        // lanes of its track.
        const auto lane = random.oneOf (known);
        const auto count = static_cast<int> (NodeLookup::childrenOfType (lane.getParent(), ids::AutomationLane).size());

        id = idOf (lane);
        toPosition = isValidForm ? random.intBetween (0, count - 1) : positionThatIsNotThere (random, count);
    }

    return made ("moveAutomationLane (" + quoted (id) + ", " + juce::String (toPosition) + ")", isValidForm, commands::moveAutomationLane (project, id, toPosition, undoManager));
}

CommandChecks::Attempt CommandDriver::setAutomationLaneName()
{
    const auto known = lanes();
    const auto isValidForm = wantsValidForm() && ! known.empty();
    const auto id = isValidForm ? idOf (random.oneOf (known)) : unknownId();
    const auto name = text();

    return made ("setAutomationLaneName (" + quoted (id) + ", " + quoted (name) + ")", isValidForm, commands::setAutomationLaneName (project, id, name, undoManager));
}

CommandChecks::Attempt CommandDriver::addLanePlacement()
{
    std::vector<juce::ValueTree> fit;
    std::vector<juce::ValueTree> unfit;

    for (const auto& clip : clips())
        (isFitForALane (clip) ? fit : unfit).push_back (clip);

    const auto knownLanes = lanes();
    const auto isValidForm = wantsValidForm() && ! knownLanes.empty() && ! fit.empty();

    // A whole number of bars, or any length.
    auto length = random.chance (50) ? 3840 * random.between (1, 4) : random.between (1, 8000);

    juce::String laneId = unknownId();
    juce::String clipId = unknownId();
    std::int64_t start = tick();

    if (! knownLanes.empty() && ! fit.empty())
    {
        const auto lane = random.oneOf (knownLanes);
        const auto taken = NodeLookup::childrenOfType (lane, ids::Placement);

        laneId = idOf (lane);
        clipId = idOf (random.oneOf (fit));
        start = freeStartFor (lane, length, {});

        if (! isValidForm)
        {
            // One thing wrong; most often a clip the lane does not take.
            const auto fault = random.intBetween (0, 6);

            if (fault <= 1 && ! unfit.empty())
                clipId = idOf (random.oneOf (unfit));
            else if (fault == 2 && ! taken.empty())
                start = startInTheWayOf (random, random.oneOf (taken), length);
            else if (fault == 3)
                start = startThatCannotBe (random, negativeTick());
            else if (fault == 4)
                length = noLength (random);
            else if (fault == 5)
                clipId = unknownId();
            else
                laneId = unknownId();
        }
    }
    else if (! knownLanes.empty() && ! unfit.empty())
    {
        // A lane and no clip it takes: one it does not take.
        laneId = idOf (random.oneOf (knownLanes));
        clipId = idOf (random.oneOf (unfit));
    }

    return made ("addLanePlacement (" + quoted (laneId) + ", " + quoted (clipId) + ", " + juce::String (start) + ", " + juce::String (length) + ")", isValidForm, commands::addLanePlacement (project, laneId, clipId, start, length, undoManager));
}

//==============================================================================
// Edit contexts.

model::Settings::SharedEditPolicy CommandDriver::policy()
{
    return random.chance (50) ? model::Settings::SharedEditPolicy::makeUnique : model::Settings::SharedEditPolicy::updateAll;
}

// A gesture on this clip: at pool level, or through some of the placements
// and slots that reference it, now and then through all of them.
juce::String CommandDriver::nameReferencesOf (const juce::ValueTree& clip, commands::EditContext& editContext)
{
    const auto clipId = idOf (clip);
    const auto share = random.oneOf (std::vector<int> { 0, 30, 60, 100 });

    for (const auto& placement : placements())
        if (PropertyReader::getRequiredString (placement, ids::clipId) == clipId && random.chance (share))
            editContext.addPlacement (idOf (placement));

    for (const auto& slot : slots())
        if (PropertyReader::getRequiredString (slot, ids::clipId) == clipId && random.chance (share))
            editContext.addSlot (idOf (slot.getParent().getParent()), sceneOf (slot));

    const juce::String policyName = editContext.getPolicy() == model::Settings::SharedEditPolicy::makeUnique ? "make unique" : "update all";

    if (! editContext.hasReferences())
        return policyName + " at pool level";

    return policyName + " through " + juce::String (editContext.getPlacementIds().size()) + " placements and " + juce::String (editContext.getSlots().size()) + " slots of " + juce::String (refcountOf (clip)) + " references";
}

// A gesture that cannot be one on this clip: its context names a placement
// that is not there or is another clip's, a slot that is empty or holds
// another clip, or a slot of a track that is not there.
juce::String CommandDriver::nameAWrongReference (const juce::String& clipId, commands::EditContext& editContext)
{
    std::vector<juce::ValueTree> placementsOfOthers;
    std::vector<juce::ValueTree> slotsOfOthers;

    for (const auto& placement : placements())
        if (PropertyReader::getRequiredString (placement, ids::clipId) != clipId)
            placementsOfOthers.push_back (placement);

    for (const auto& slot : slots())
        if (PropertyReader::getRequiredString (slot, ids::clipId) != clipId)
            slotsOfOthers.push_back (slot);

    const auto known = tracks();
    const auto choice = random.intBetween (0, 4);

    if (choice == 0 && ! placementsOfOthers.empty())
    {
        editContext.addPlacement (idOf (random.oneOf (placementsOfOthers)));
        return "through a placement of another clip";
    }

    if (choice == 1 && ! slotsOfOthers.empty())
    {
        const auto slot = random.oneOf (slotsOfOthers);
        editContext.addSlot (idOf (slot.getParent().getParent()), sceneOf (slot));
        return "through a slot that holds another clip";
    }

    if (choice == 2 && ! known.empty())
    {
        // A row of this track that nothing is stored for.
        const auto track = random.oneOf (known);
        int scene = 0;

        while (NodeLookup::childWithNumber (track.getChildWithName (ids::Slots), ids::Slot, ids::scene, scene).isValid())
            ++scene;

        editContext.addSlot (idOf (track), scene);
        return "through an empty slot";
    }

    if (choice == 3)
    {
        editContext.addSlot (unknownId(), 0);
        return "through a slot of a track that is not there";
    }

    editContext.addPlacement (unknownId());
    return "through a placement that is not there";
}

} // namespace guardrails
