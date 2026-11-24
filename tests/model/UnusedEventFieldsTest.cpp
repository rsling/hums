// An event carries only what its kind uses (docs/EDITING.md -> EventClip
// content): the commands refuse an `a` or `b` the kind has no use for, and
// `data` on anything but a sysex event. Finding 16 of the milestone 1 code
// review (23 Nov 2026): the Validator admitted all three, so a document
// loaded that no command could have made, and setEvent then refused the
// values the event already had. The Validator refuses them now (invariant
// 10).

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "CommandFixture.h"
#include "model/ClipModel.h"
#include "model/EventModel.h"
#include "model/Validator.h"
#include "model/commands/ClipPoolCommands.h"
#include "model/commands/EditContext.h"
#include "model/commands/EventClipCommands.h"
#include "model/commands/NodeLookup.h"

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;
using Policy = model::Settings::SharedEditPolicy;

// The command fixture with one event clip that holds one event, written as a
// project file would hold it.
struct ClipWithOneEvent : CommandFixture
{
    juce::String clipId;

    explicit ClipWithOneEvent (const char* eventXml)
    {
        const auto result = commands::addEventClip (project, "Clip", 3840, undoManager);
        REQUIRE (result.wasApplied());
        clipId = result.createdId;

        auto events = clipNode().getChildWithName (ids::Events);
        REQUIRE (events.isValid());
        events.appendChild (juce::ValueTree::fromXml (eventXml), nullptr);
    }

    juce::ValueTree clipNode() const
    {
        return commands::NodeLookup::childWithId (project.getChildWithName (ids::ClipPool), ids::Clip, clipId);
    }

    juce::ValueTree eventNode() const
    {
        return clipNode().getChildWithName (ids::Events).getChildWithName (ids::Event);
    }

    model::EventModel event() const
    {
        return model::ClipModel (clipNode()).getEvents().at (0);
    }
};

// The document has that one violation, on that property of the event.
void requireOnlyViolationOn (const ClipWithOneEvent& song, const juce::Identifier& property)
{
    const auto violations = model::Validator::validate (song.project);

    REQUIRE (violations.size() == 1);
    CHECK (violations.front().nodePath == "Project/ClipPool/Clip[0]/Events/Event[0]");
    CHECK (violations.front().property == property.toString());
}

} // namespace

TEST_CASE ("Invariant 10: an event without what its kind does not use is valid", "[model][Validator]")
{
    // The control: what the cases below add is the only thing judged.
    REQUIRE (ClipWithOneEvent (R"(<Event tick="0" kind="pitchBend" a="8192"/>)").isValid());
    REQUIRE (ClipWithOneEvent (R"(<Event tick="0" kind="sysex" data="7e7f"/>)").isValid());
    REQUIRE (ClipWithOneEvent (R"(<Event tick="0" kind="cc" a="74" b="90"/>)").isValid());
}

TEST_CASE ("Invariant 10: an a or b that the event's kind does not use is 0", "[model][Validator]")
{
    SECTION ("a b on a kind that has none is refused")
    {
        const auto kind = GENERATE ("pitchBend", "programChange", "channelPressure", "sysex");
        ClipWithOneEvent song (R"(<Event tick="0" kind="cc" b="5"/>)");
        song.eventNode().setProperty (ids::kind, kind, nullptr);

        requireOnlyViolationOn (song, ids::b);
    }

    SECTION ("an a on a sysex event is refused")
    {
        requireOnlyViolationOn (ClipWithOneEvent (R"(<Event tick="0" kind="sysex" a="5" data="7e7f"/>)"), ids::a);
    }

    SECTION ("one written as 0 is what an absent one is")
    {
        REQUIRE (ClipWithOneEvent (R"(<Event tick="0" kind="pitchBend" a="8192" b="0"/>)").isValid());
        REQUIRE (ClipWithOneEvent (R"(<Event tick="0" kind="sysex" a="0" b="0" data="7e7f"/>)").isValid());
    }

    SECTION ("one that is no number is malformed, used or not")
    {
        requireOnlyViolationOn (ClipWithOneEvent (R"(<Event tick="0" kind="pitchBend" a="8192" b="x"/>)"), ids::b);
    }
}

TEST_CASE ("Invariant 10: only a sysex event has data", "[model][Validator]")
{
    SECTION ("data on another kind is refused, a valid payload too")
    {
        const auto kind = GENERATE ("cc", "pitchBend", "programChange", "channelPressure", "polyPressure");
        ClipWithOneEvent song (R"(<Event tick="0" kind="cc" data="7e7f"/>)");
        song.eventNode().setProperty (ids::kind, kind, nullptr);

        requireOnlyViolationOn (song, ids::data);
    }

    SECTION ("an empty data is what an absent one is")
    {
        REQUIRE (ClipWithOneEvent (R"(<Event tick="0" kind="cc" a="74" b="90" data=""/>)").isValid());
    }
}

TEST_CASE ("An event of a valid document can be set to the values it has", "[model][commands]")
{
    // What the finding broke: every event the Validator admits is one the
    // command takes. One of each kind, with everything its kind uses.
    const auto* eventXml = GENERATE (R"(<Event tick="0" kind="cc" a="74" b="90"/>)",
                                     R"(<Event tick="0" kind="pitchBend" a="8192"/>)",
                                     R"(<Event tick="0" kind="programChange" a="5"/>)",
                                     R"(<Event tick="0" kind="channelPressure" a="127"/>)",
                                     R"(<Event tick="0" kind="polyPressure" a="60" b="127"/>)",
                                     R"(<Event tick="0" kind="sysex" data="7e7F0901"/>)");
    ClipWithOneEvent song (eventXml);
    REQUIRE (song.isValid());

    const auto event = song.event();

    // Only `enabled` changes; everything else is what the event has.
    commands::EventValues values;
    values.tick = event.getTick();
    values.kind = event.getKind();
    values.a = event.getA();
    values.b = event.getB();
    values.data = event.getData();
    values.enabled = false;

    song.undoManager.beginNewTransaction();
    commands::EditContext editContext (Policy::updateAll);
    const auto result = commands::setEvent (song.project, song.clipId, 0, values, editContext, song.undoManager);

    INFO ("refusal: " << result.refusal);
    CHECK (result.wasApplied());
}
