// model::commands for the tracks and their knobs (docs/SCHEMA.md -> Tracks /
// Track, Knobs / Knob).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/TrackModel.h"
#include "model/TracksModel.h"
#include "model/commands/TrackCommands.h"

#include <string>
#include <vector>

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;

const char* const unknownId = "ffffffffffffffffffffffffffffffff";

// The fixture's tracks, from top to bottom.
const char* const bassId = "4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b";
const char* const leadId = "5d7f9b1d3f504c2e9b7d5f3b1d2f4a6c";
const char* const bass2Id = "6e8a0c2e4a614d3fac8e6a4c2e3a5b7d";

std::vector<model::TrackModel> tracksOf (const CommandFixture& fixture)
{
    return model::TracksModel (fixture.project.getChildWithName (ids::Tracks)).getTracks();
}

// The names of the tracks from top to bottom, as one text.
std::string trackOrderOf (const CommandFixture& fixture)
{
    std::string order;

    for (const auto& track : tracksOf (fixture))
        order += track.getName().toStdString() + "|";

    return order;
}

// The controller numbers of a track's knobs from left to right, as one text.
std::string knobOrderOf (const CommandFixture& fixture, int trackPosition)
{
    std::string order;

    for (const auto& knob : model::TrackModel (fixture.track (trackPosition)).getKnobs())
        order += std::to_string (knob.getCc()) + "|";

    return order;
}

} // namespace

TEST_CASE ("addTrack makes an unassigned track below the others, and undo takes it away again", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    const auto result = commands::addTrack (fixture.project, "Pad", fixture.undoManager);

    REQUIRE (result.wasApplied());
    CHECK (fixture.isValid());

    const auto tracks = tracksOf (fixture);
    REQUIRE (tracks.size() == 4);

    const auto& added = tracks[3];
    CHECK (added.getId() == result.createdId);
    CHECK (added.getName() == "Pad");
    CHECK (added.getColour().isEmpty());
    CHECK (added.getPlayerId().isEmpty());
    CHECK (added.getChannel() == 1);
    CHECK (added.getPerformerId().isEmpty());
    CHECK (! added.isMuted());
    CHECK (! added.isSoloed());
    CHECK (added.getPlacements().empty());
    CHECK (added.getAutomationLanes().empty());
    CHECK (added.getKnobs().empty());
    CHECK (added.getSlots().empty());

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (! fixture.undoManager.canUndo());
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("Two tracks that are added get two ids, and may share a name", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;

    const auto first = commands::addTrack (fixture.project, "Bass", fixture.undoManager);
    const auto second = commands::addTrack (fixture.project, "", fixture.undoManager);

    REQUIRE (first.wasApplied());
    REQUIRE (second.wasApplied());
    CHECK (first.createdId != second.createdId);
    CHECK (fixture.isValid());
    CHECK (trackOrderOf (fixture) == "Bass|Lead|Bass 2|Bass||");
}

TEST_CASE ("removeTrack removes the track with what it owns, in one undo step", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;
    REQUIRE (commands::addKnob (fixture.project, leadId, 74, "Cutoff", fixture.undoManager).wasApplied());
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removeTrack (fixture.project, leadId, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (trackOrderOf (fixture) == "Bass|Bass 2|");

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (knobOrderOf (fixture, 1) == "74|");
}

TEST_CASE ("The last track can be removed", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;

    for (const auto* id : { bassId, leadId, bass2Id })
        REQUIRE (commands::removeTrack (fixture.project, id, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (tracksOf (fixture).empty());
}

TEST_CASE ("moveTrack puts the track at that position and keeps the order of the others", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::moveTrack (fixture.project, bassId, 2, fixture.undoManager).wasApplied());
    CHECK (trackOrderOf (fixture) == "Lead|Bass 2|Bass|");
    CHECK (fixture.isValid());

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::moveTrack (fixture.project, bassId, 1, fixture.undoManager).wasApplied());
    CHECK (trackOrderOf (fixture) == "Lead|Bass|Bass 2|");

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::moveTrack (fixture.project, bass2Id, 0, fixture.undoManager).wasApplied());
    CHECK (trackOrderOf (fixture) == "Bass 2|Lead|Bass|");

    REQUIRE (fixture.undoManager.undo());
    CHECK (trackOrderOf (fixture) == "Lead|Bass|Bass 2|");
    REQUIRE (fixture.undoManager.undo());
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("moveTrack counts positions among tracks only, not among children of another type", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;

    // What a newer build might have left between the tracks. The schema
    // name of another node stands in for a type this build does not know.
    fixture.project.getChildWithName (ids::Tracks).addChild (juce::ValueTree (ids::Marker), 1, nullptr);

    REQUIRE (commands::moveTrack (fixture.project, bassId, 1, fixture.undoManager).wasApplied());
    CHECK (trackOrderOf (fixture) == "Lead|Bass|Bass 2|");

    REQUIRE (commands::moveTrack (fixture.project, bass2Id, 0, fixture.undoManager).wasApplied());
    CHECK (trackOrderOf (fixture) == "Bass 2|Lead|Bass|");

    REQUIRE (commands::moveTrack (fixture.project, bass2Id, 2, fixture.undoManager).wasApplied());
    CHECK (trackOrderOf (fixture) == "Lead|Bass|Bass 2|");

    CHECK (! commands::moveTrack (fixture.project, bassId, 3, fixture.undoManager).wasApplied());
}

TEST_CASE ("moveTrack to the position the track has is applied and leaves nothing to undo", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (commands::moveTrack (fixture.project, leadId, 1, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("moveTrack refuses a position that is not there", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (! commands::moveTrack (fixture.project, leadId, -1, fixture.undoManager).wasApplied());
    CHECK (! commands::moveTrack (fixture.project, leadId, 3, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("The properties of a Track are set through commands", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;

    REQUIRE (commands::setTrackName (fixture.project, leadId, "Solo", fixture.undoManager).wasApplied());
    REQUIRE (commands::setTrackColour (fixture.project, leadId, "#ff00AA10", fixture.undoManager).wasApplied());
    REQUIRE (commands::setTrackPlayer (fixture.project, leadId, CommandFixture::minimoogId, fixture.undoManager).wasApplied());
    REQUIRE (commands::setTrackChannel (fixture.project, leadId, 16, fixture.undoManager).wasApplied());
    REQUIRE (commands::setTrackPerformer (fixture.project, leadId, CommandFixture::keyboardId, fixture.undoManager).wasApplied());
    REQUIRE (commands::setTrackMute (fixture.project, leadId, true, fixture.undoManager).wasApplied());
    REQUIRE (commands::setTrackSolo (fixture.project, leadId, true, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    const model::TrackModel lead (fixture.track (1));
    CHECK (lead.getName() == "Solo");
    CHECK (lead.getColour() == "#ff00AA10");
    CHECK (lead.getPlayerId() == CommandFixture::minimoogId);
    CHECK (lead.getChannel() == 16);
    CHECK (lead.getPerformerId() == CommandFixture::keyboardId);
    CHECK (lead.isMuted());
    CHECK (lead.isSoloed());

    // The other tracks are as they were.
    CHECK (! model::TrackModel (fixture.track (0)).isMuted());
    CHECK (model::TrackModel (fixture.track (2)).getChannel() == 1);

    REQUIRE (commands::setTrackChannel (fixture.project, leadId, 1, fixture.undoManager).wasApplied());
    REQUIRE (commands::setTrackColour (fixture.project, leadId, "", fixture.undoManager).wasApplied());
    REQUIRE (commands::setTrackMute (fixture.project, leadId, false, fixture.undoManager).wasApplied());
    REQUIRE (commands::setTrackSolo (fixture.project, leadId, false, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (lead.getChannel() == 1);
    CHECK (lead.getColour().isEmpty());
    CHECK (! lead.isMuted());
    CHECK (! lead.isSoloed());
}

TEST_CASE ("An empty id unassigns a track's Player or Performer", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setTrackPlayer (fixture.project, bassId, "", fixture.undoManager).wasApplied());
    REQUIRE (commands::setTrackPerformer (fixture.project, bassId, "", fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (model::TrackModel (fixture.track (0)).getPlayerId().isEmpty());
    CHECK (model::TrackModel (fixture.track (0)).getPerformerId().isEmpty());

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("Muting a track is one undo step of its own", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setTrackMute (fixture.project, bassId, true, fixture.undoManager).wasApplied());
    const auto muted = fixture.xml();

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == muted);
}

TEST_CASE ("Setting a track property to the value it has is applied and leaves nothing to undo", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (commands::setTrackName (fixture.project, bassId, "Bass", fixture.undoManager).wasApplied());
    CHECK (commands::setTrackPlayer (fixture.project, bassId, CommandFixture::minimoogId, fixture.undoManager).wasApplied());
    CHECK (commands::setTrackPerformer (fixture.project, bassId, CommandFixture::keyboardId, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("A track command for a track that is not there is refused and changes nothing", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    const auto results = {
        commands::removeTrack (fixture.project, unknownId, fixture.undoManager),
        commands::moveTrack (fixture.project, unknownId, 0, fixture.undoManager),
        commands::setTrackName (fixture.project, unknownId, "x", fixture.undoManager),
        commands::setTrackColour (fixture.project, unknownId, "", fixture.undoManager),
        commands::setTrackPlayer (fixture.project, unknownId, "", fixture.undoManager),
        commands::setTrackChannel (fixture.project, unknownId, 1, fixture.undoManager),
        commands::setTrackPerformer (fixture.project, unknownId, "", fixture.undoManager),
        commands::setTrackMute (fixture.project, unknownId, true, fixture.undoManager),
        commands::setTrackSolo (fixture.project, unknownId, true, fixture.undoManager),
        commands::addKnob (fixture.project, unknownId, 74, "", fixture.undoManager),
        commands::removeKnob (fixture.project, unknownId, 74, fixture.undoManager),
        commands::moveKnob (fixture.project, unknownId, 74, 0, fixture.undoManager),
        commands::setKnobCc (fixture.project, unknownId, 74, 71, fixture.undoManager),
        commands::setKnobLabel (fixture.project, unknownId, 74, "x", fixture.undoManager),
        // A Player's id names no Track.
        commands::removeTrack (fixture.project, CommandFixture::minimoogId, fixture.undoManager),
        commands::removeTrack (fixture.project, "", fixture.undoManager),
    };

    for (const auto& result : results)
    {
        CHECK (! result.wasApplied());
        CHECK (result.refusal.isNotEmpty());
        CHECK (result.createdId.isEmpty());
    }

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("A track property the schema does not allow is refused", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (! commands::setTrackChannel (fixture.project, bassId, 0, fixture.undoManager).wasApplied());
    CHECK (! commands::setTrackChannel (fixture.project, bassId, 17, fixture.undoManager).wasApplied());
    CHECK (! commands::setTrackChannel (fixture.project, bassId, -1, fixture.undoManager).wasApplied());

    CHECK (! commands::setTrackColour (fixture.project, bassId, "red", fixture.undoManager).wasApplied());
    CHECK (! commands::setTrackColour (fixture.project, bassId, "#ff0000", fixture.undoManager).wasApplied());     // six digits
    CHECK (! commands::setTrackColour (fixture.project, bassId, "ffff00000", fixture.undoManager).wasApplied());   // no '#'
    CHECK (! commands::setTrackColour (fixture.project, bassId, "#ff00000g", fixture.undoManager).wasApplied());
    CHECK (! commands::setTrackColour (fixture.project, bassId, "#ffff000000", fixture.undoManager).wasApplied()); // ten digits

    // No such port; a Performer is no Player, and the other way round.
    CHECK (! commands::setTrackPlayer (fixture.project, bassId, unknownId, fixture.undoManager).wasApplied());
    CHECK (! commands::setTrackPlayer (fixture.project, bassId, CommandFixture::keyboardId, fixture.undoManager).wasApplied());
    CHECK (! commands::setTrackPerformer (fixture.project, bassId, unknownId, fixture.undoManager).wasApplied());
    CHECK (! commands::setTrackPerformer (fixture.project, bassId, CommandFixture::prophetId, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("addKnob adds a knob to the right of the others, and undo takes it away again", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;

    REQUIRE (commands::addKnob (fixture.project, bassId, 74, "Cutoff", fixture.undoManager).wasApplied());
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::addKnob (fixture.project, bassId, 71, "", fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    const auto knobs = model::TrackModel (fixture.track (0)).getKnobs();
    REQUIRE (knobs.size() == 2);
    CHECK (knobs[0].getCc() == 74);
    CHECK (knobs[0].getLabel() == "Cutoff");
    CHECK (knobs[1].getCc() == 71);
    CHECK (knobs[1].getLabel().isEmpty());

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("A controller number is unique within its track only, and both ends of the range are taken", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;

    CHECK (commands::addKnob (fixture.project, bassId, 74, "Cutoff", fixture.undoManager).wasApplied());
    CHECK (commands::addKnob (fixture.project, leadId, 74, "Cutoff", fixture.undoManager).wasApplied());
    CHECK (commands::addKnob (fixture.project, leadId, 0, "", fixture.undoManager).wasApplied());
    CHECK (commands::addKnob (fixture.project, leadId, 127, "", fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (knobOrderOf (fixture, 1) == "74|0|127|");
}

TEST_CASE ("addKnob refuses a controller number that is none or is taken", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;
    REQUIRE (commands::addKnob (fixture.project, bassId, 74, "Cutoff", fixture.undoManager).wasApplied());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    CHECK (! commands::addKnob (fixture.project, bassId, 74, "Again", fixture.undoManager).wasApplied());
    CHECK (! commands::addKnob (fixture.project, bassId, -1, "", fixture.undoManager).wasApplied());
    CHECK (! commands::addKnob (fixture.project, bassId, 128, "", fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("A knob is removed, moved, renumbered and labelled through commands", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;

    for (const auto cc : { 74, 71, 1, 7 })
        REQUIRE (commands::addKnob (fixture.project, bassId, cc, "", fixture.undoManager).wasApplied());

    REQUIRE (commands::moveKnob (fixture.project, bassId, 74, 3, fixture.undoManager).wasApplied());
    CHECK (knobOrderOf (fixture, 0) == "71|1|7|74|");

    REQUIRE (commands::moveKnob (fixture.project, bassId, 7, 0, fixture.undoManager).wasApplied());
    CHECK (knobOrderOf (fixture, 0) == "7|71|1|74|");

    REQUIRE (commands::removeKnob (fixture.project, bassId, 71, fixture.undoManager).wasApplied());
    CHECK (knobOrderOf (fixture, 0) == "7|1|74|");

    // The number that was removed is free again; the knob keeps its place
    // and its label.
    REQUIRE (commands::setKnobLabel (fixture.project, bassId, 1, "Mod", fixture.undoManager).wasApplied());
    REQUIRE (commands::setKnobCc (fixture.project, bassId, 1, 71, fixture.undoManager).wasApplied());
    CHECK (knobOrderOf (fixture, 0) == "7|71|74|");
    CHECK (model::TrackModel (fixture.track (0)).getKnobs()[1].getLabel() == "Mod");

    CHECK (fixture.isValid());
}

TEST_CASE ("A knob change is taken back by one undo", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;

    for (const auto cc : { 74, 71, 1 })
        REQUIRE (commands::addKnob (fixture.project, bassId, cc, "", fixture.undoManager).wasApplied());

    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::moveKnob (fixture.project, bassId, 1, 0, fixture.undoManager).wasApplied());
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setKnobCc (fixture.project, bassId, 71, 10, fixture.undoManager).wasApplied());
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removeKnob (fixture.project, bassId, 71, fixture.undoManager).wasApplied());
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("A knob command that the track cannot take is refused and changes nothing", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;

    for (const auto cc : { 74, 71 })
        REQUIRE (commands::addKnob (fixture.project, bassId, cc, "", fixture.undoManager).wasApplied());

    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    // No knob for controller 1 on this track, and none at all on the next.
    CHECK (! commands::removeKnob (fixture.project, bassId, 1, fixture.undoManager).wasApplied());
    CHECK (! commands::moveKnob (fixture.project, bassId, 1, 0, fixture.undoManager).wasApplied());
    CHECK (! commands::setKnobCc (fixture.project, bassId, 1, 2, fixture.undoManager).wasApplied());
    CHECK (! commands::setKnobLabel (fixture.project, bassId, 1, "x", fixture.undoManager).wasApplied());
    CHECK (! commands::removeKnob (fixture.project, leadId, 74, fixture.undoManager).wasApplied());

    CHECK (! commands::moveKnob (fixture.project, bassId, 74, -1, fixture.undoManager).wasApplied());
    CHECK (! commands::moveKnob (fixture.project, bassId, 74, 2, fixture.undoManager).wasApplied());

    CHECK (! commands::setKnobCc (fixture.project, bassId, 74, 71, fixture.undoManager).wasApplied()); // the other knob's
    CHECK (! commands::setKnobCc (fixture.project, bassId, 74, -1, fixture.undoManager).wasApplied());
    CHECK (! commands::setKnobCc (fixture.project, bassId, 74, 128, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("Setting a knob's controller number to the one it has is applied and leaves nothing to undo", "[model][commands][TrackCommands]")
{
    CommandFixture fixture;
    REQUIRE (commands::addKnob (fixture.project, bassId, 74, "", fixture.undoManager).wasApplied());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    CHECK (commands::setKnobCc (fixture.project, bassId, 74, 74, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}
