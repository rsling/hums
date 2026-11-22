#include "CommandDriver.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"
#include "model/commands/NodeLookup.h"
#include "model/commands/PadMapCommands.h"
#include "model/commands/RoutingCommands.h"
#include "model/commands/SessionCommands.h"
#include "model/commands/TimelineCommands.h"

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
    juce::String id;

    for (int digit = 0; digit < 32; ++digit)
        id << digits[random.indexBelow (16)];

    return id;
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

} // namespace guardrails
