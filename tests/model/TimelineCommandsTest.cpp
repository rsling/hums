// model::commands for the Timeline: tempo map, time-signature map, markers
// (docs/SCHEMA.md -> Timeline).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/TimelineModel.h"
#include "model/commands/TimelineCommands.h"

#include <cstdint>
#include <limits>

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;

model::TimelineModel timelineOf (const CommandFixture& fixture)
{
    return model::TimelineModel (fixture.timeline());
}

// The fixture's two markers, both at tick 7680: "B" and one without a name.
juce::ValueTree markerNode (const CommandFixture& fixture, int position)
{
    int seen = 0;

    for (const auto& child : fixture.timeline())
        if (child.hasType (ids::Marker) && seen++ == position)
            return child;

    return {};
}

} // namespace

TEST_CASE ("setTempo changes the tempo at a tick that has one", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setTempo (fixture.project, 0, 97.5, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    const auto tempos = timelineOf (fixture).getTempoEvents();
    REQUIRE (tempos.size() == 2);
    CHECK (juce::exactlyEqual (tempos[0].getBpm(), 97.5));
    CHECK (juce::exactlyEqual (tempos[1].getBpm(), 124.5));

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("setTempo adds a tempo at a tick that has none, and undo takes it away again", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setTempo (fixture.project, 3840, 140.0, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    const auto tempos = timelineOf (fixture).getTempoEvents();
    REQUIRE (tempos.size() == 3);
    CHECK (tempos[2].getTick() == 3840);
    CHECK (juce::exactlyEqual (tempos[2].getBpm(), 140.0));

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("setTempo stores a tempo with six significant digits, as the text that is saved [D14]", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;

    REQUIRE (commands::setTempo (fixture.project, 0, 118.1234567, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    const auto stored = fixture.timeline().getChildWithName (ids::TempoEvent)[ids::bpm];
    CHECK (stored.isString());
    CHECK (stored.toString() == "118.123");
    CHECK (juce::exactlyEqual (timelineOf (fixture).getTempoEvents()[0].getBpm(), 118.123));
}

TEST_CASE ("setTempo takes the slowest and the fastest tempo of the range [D23]", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;

    REQUIRE (commands::setTempo (fixture.project, 0, 3.57628, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (fixture.timeline().getChildWithName (ids::TempoEvent)[ids::bpm].toString() == "3.57628");

    REQUIRE (commands::setTempo (fixture.project, 0, 999.999, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (fixture.timeline().getChildWithName (ids::TempoEvent)[ids::bpm].toString() == "999.999");

    // In the range once the digits past the sixth are dropped.
    REQUIRE (commands::setTempo (fixture.project, 0, 3.576279, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (fixture.timeline().getChildWithName (ids::TempoEvent)[ids::bpm].toString() == "3.57628");

    REQUIRE (commands::setTempo (fixture.project, 0, 999.9994, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (fixture.timeline().getChildWithName (ids::TempoEvent)[ids::bpm].toString() == "999.999");
}

TEST_CASE ("setTempo refuses a tempo outside 3.57628 to 999.999 bpm [D23]", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    // Slower than a Standard MIDI File can hold.
    CHECK (! commands::setTempo (fixture.project, 0, 3.57627, fixture.undoManager).wasApplied());
    CHECK (! commands::setTempo (fixture.project, 0, 1.0, fixture.undoManager).wasApplied());
    CHECK (! commands::setTempo (fixture.project, 0, 1e-300, fixture.undoManager).wasApplied());

    CHECK (! commands::setTempo (fixture.project, 0, 1000.0, fixture.undoManager).wasApplied());
    CHECK (! commands::setTempo (fixture.project, 0, 1234.5678, fixture.undoManager).wasApplied());
    CHECK (! commands::setTempo (fixture.project, 0, std::numeric_limits<double>::max(), fixture.undoManager).wasApplied());
    // It would be saved as 1000.
    CHECK (! commands::setTempo (fixture.project, 0, 999.9996, fixture.undoManager).wasApplied());

    const auto refusal = commands::setTempo (fixture.project, 0, 2000.0, fixture.undoManager);
    CHECK (refusal.refusal == "A tempo is 3.57628 to 999.999 bpm.");

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("setTempo to the tempo that is there leaves nothing to undo", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (commands::setTempo (fixture.project, 7680, 124.5, fixture.undoManager).wasApplied());
    // The same tempo once more digits than six are dropped.
    CHECK (commands::setTempo (fixture.project, 7680, 124.5000001, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("setTempo refuses what is no tempo and what is no position", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (! commands::setTempo (fixture.project, 0, 0.0, fixture.undoManager).wasApplied());
    CHECK (! commands::setTempo (fixture.project, 0, -120.0, fixture.undoManager).wasApplied());
    CHECK (! commands::setTempo (fixture.project, 0, std::numeric_limits<double>::infinity(), fixture.undoManager).wasApplied());
    CHECK (! commands::setTempo (fixture.project, 0, std::numeric_limits<double>::quiet_NaN(), fixture.undoManager).wasApplied());
    CHECK (! commands::setTempo (fixture.project, -1, 120.0, fixture.undoManager).wasApplied());
    // Greater than 0, but its saved text ("4.94066e-324") reads back as 0.
    CHECK (! commands::setTempo (fixture.project, 0, std::numeric_limits<double>::denorm_min(), fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("removeTempoEvent removes a later tempo, never the one at tick 0", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (! commands::removeTempoEvent (fixture.project, 0, fixture.undoManager).wasApplied());
    CHECK (! commands::removeTempoEvent (fixture.project, 3840, fixture.undoManager).wasApplied()); // a time signature is there, no tempo
    CHECK (! commands::removeTempoEvent (fixture.project, -1, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removeTempoEvent (fixture.project, 7680, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    REQUIRE (timelineOf (fixture).getTempoEvents().size() == 1);
    CHECK (timelineOf (fixture).getTempoEvents()[0].getTick() == 0);

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("setTimeSignature changes the one at a tick, or adds one, as one undo step", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setTimeSignature (fixture.project, 3840, 7, 8, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    REQUIRE (timelineOf (fixture).getTimeSigEvents().size() == 2);
    CHECK (timelineOf (fixture).getTimeSigEvents()[1].getNumerator() == 7);
    CHECK (timelineOf (fixture).getTimeSigEvents()[1].getDenominator() == 8);

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setTimeSignature (fixture.project, 15360, 32, 64, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    REQUIRE (timelineOf (fixture).getTimeSigEvents().size() == 3);
    CHECK (timelineOf (fixture).getTimeSigEvents()[2].getTick() == 15360);
    CHECK (timelineOf (fixture).getTimeSigEvents()[2].getNumerator() == 32);
    CHECK (timelineOf (fixture).getTimeSigEvents()[2].getDenominator() == 64);

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("setTimeSignature refuses what is no time signature", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (! commands::setTimeSignature (fixture.project, 0, 0, 4, fixture.undoManager).wasApplied());
    CHECK (! commands::setTimeSignature (fixture.project, 0, 33, 4, fixture.undoManager).wasApplied());
    CHECK (! commands::setTimeSignature (fixture.project, 0, 4, 0, fixture.undoManager).wasApplied());
    CHECK (! commands::setTimeSignature (fixture.project, 0, 4, 3, fixture.undoManager).wasApplied());
    CHECK (! commands::setTimeSignature (fixture.project, 0, 4, 128, fixture.undoManager).wasApplied());
    CHECK (! commands::setTimeSignature (fixture.project, 0, 4, -4, fixture.undoManager).wasApplied());
    CHECK (! commands::setTimeSignature (fixture.project, -1, 4, 4, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("removeTimeSigEvent removes a later time signature, never the one at tick 0", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (! commands::removeTimeSigEvent (fixture.project, 0, fixture.undoManager).wasApplied());
    CHECK (! commands::removeTimeSigEvent (fixture.project, 7680, fixture.undoManager).wasApplied()); // a tempo is there, no time signature
    CHECK (fixture.isUntouchedSince (before));

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removeTimeSigEvent (fixture.project, 3840, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (timelineOf (fixture).getTimeSigEvents().size() == 1);

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("addMarker adds a marker, also at a tick that has one", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::addMarker (fixture.project, 7680, "Bridge", fixture.undoManager).wasApplied());
    REQUIRE (commands::addMarker (fixture.project, std::int64_t { 1 } << 40, "", fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    const auto markers = timelineOf (fixture).getMarkers();
    REQUIRE (markers.size() == 4);
    CHECK (markers[2].getTick() == 7680);
    CHECK (markers[2].getName() == "Bridge");
    CHECK (markers[3].getTick() == std::int64_t { 1 } << 40);
    CHECK (markers[3].getName().isEmpty());

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);

    CHECK (! commands::addMarker (fixture.project, -1, "x", fixture.undoManager).wasApplied());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("A marker is moved, renamed and removed by its node", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();
    const auto unnamed = markerNode (fixture, 1);

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::moveMarker (fixture.project, unnamed, 11520, fixture.undoManager).wasApplied());
    REQUIRE (commands::renameMarker (fixture.project, unnamed, "C", fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    auto markers = timelineOf (fixture).getMarkers();
    REQUIRE (markers.size() == 2);
    CHECK (markers[0].getTick() == 7680);
    CHECK (markers[0].getName() == "B");
    CHECK (markers[1].getTick() == 11520);
    CHECK (markers[1].getName() == "C");

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removeMarker (fixture.project, markerNode (fixture, 0), fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    markers = timelineOf (fixture).getMarkers();
    REQUIRE (markers.size() == 1);
    CHECK (markers[0].getName() == "C");

    REQUIRE (fixture.undoManager.undo());
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);

    // The handle is still the node after undo and redo.
    REQUIRE (fixture.undoManager.redo());
    CHECK (commands::renameMarker (fixture.project, unnamed, "D", fixture.undoManager).wasApplied());
    CHECK (timelineOf (fixture).getMarkers()[1].getName() == "D");
}

TEST_CASE ("A marker command for a node that is no marker of this Timeline is refused", "[model][commands][TimelineCommands]")
{
    CommandFixture fixture;
    CommandFixture other;
    const auto before = fixture.xml();

    const juce::ValueTree detached (ids::Marker);
    const auto tempoEvent = fixture.timeline().getChildWithName (ids::TempoEvent);
    const auto markerOfOther = markerNode (other, 0);

    for (const auto& node : { juce::ValueTree(), detached, tempoEvent, markerOfOther })
    {
        CHECK (! commands::removeMarker (fixture.project, node, fixture.undoManager).wasApplied());
        CHECK (! commands::moveMarker (fixture.project, node, 0, fixture.undoManager).wasApplied());
        CHECK (! commands::renameMarker (fixture.project, node, "x", fixture.undoManager).wasApplied());
    }

    CHECK (! commands::moveMarker (fixture.project, markerNode (fixture, 0), -1, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
    CHECK (other.xml() == before);
}
