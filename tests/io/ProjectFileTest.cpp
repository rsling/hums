// io::ProjectFile: the native project file. The canonical writer and the load
// sequence, docs/SCHEMA.md -> Loading, saving, canonical form.

#include <catch2/catch_test_macros.hpp>

#include "io/Migrations.h"
#include "io/ProjectFile.h"
#include "model/CellModel.h"
#include "model/ClipModel.h"
#include "model/EventModel.h"
#include "model/GridModel.h"
#include "model/Ids.h"
#include "model/KnobModel.h"
#include "model/LaneModel.h"
#include "model/MarkerModel.h"
#include "model/NoteModel.h"
#include "model/PadModel.h"
#include "model/PerformerModel.h"
#include "model/PlacementModel.h"
#include "model/PlayerModel.h"
#include "model/ProjectModel.h"
#include "model/SceneModel.h"
#include "model/SessionModel.h"
#include "model/Settings.h"
#include "model/TrackModel.h"
#include "model/Validator.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace
{

namespace ids = model::ids;

juce::String fixtureText (const juce::String& fileName)
{
    const auto file = juce::File (HUMS_IO_FIXTURES_DIRECTORY).getChildFile (fileName);
    REQUIRE (file.existsAsFile());

    return file.loadFileAsString();
}

// Catch2 prints a std::string when a comparison fails, and not a juce::String.
std::string canonical (const juce::ValueTree& project)
{
    return io::ProjectFile::toCanonicalXml (project).toStdString();
}

// What a save and a reload make of a document: every property text again.
juce::ValueTree reloaded (const juce::ValueTree& project)
{
    return juce::ValueTree::fromXml (io::ProjectFile::toCanonicalXml (project));
}

// The writer takes a whole document, and most tests are about one node. The
// writer knows a node by its type and not by its place, so the node may hang
// directly under the Project.
juce::ValueTree projectHolding (const juce::ValueTree& node)
{
    juce::ValueTree project (ids::Project);
    project.setProperty (ids::schemaVersion, 1, nullptr);
    project.appendChild (node.createCopy(), nullptr);

    return project;
}

// The line the writer makes of one node without children.
std::string lineOf (const juce::ValueTree& node)
{
    const auto lines = juce::StringArray::fromLines (io::ProjectFile::toCanonicalXml (projectHolding (node)));

    // Header, empty line, <Project ...>, the node, </Project>, and the empty
    // string after the last line ending.
    REQUIRE (lines.size() == 6);

    return lines[3].trim().toStdString();
}

juce::ValueTree nodeFromXml (const char* xml)
{
    auto node = juce::ValueTree::fromXml (xml);
    REQUIRE (node.isValid());

    return node;
}

} // namespace

//==============================================================================
// The Berlin fixture: the example of docs/SCHEMA.md with real UUIDs.

TEST_CASE ("The Berlin fixture is a valid document", "[io][ProjectFile]")
{
    const auto project = nodeFromXml (fixtureText ("berlin_test.xml").toRawUTF8());

    CHECK (model::Validator::validate (project).empty());
}

TEST_CASE ("The Berlin fixture is written in the canonical form reasoned out by hand", "[io][ProjectFile]")
{
    // berlin_test.xml spells the example out as the schema prints it, with
    // defaults and aligned columns. berlin_test_canonical.xml is what the
    // rules of the canonical form make of it.
    const auto project = nodeFromXml (fixtureText ("berlin_test.xml").toRawUTF8());

    CHECK (canonical (project) == fixtureText ("berlin_test_canonical.xml").toStdString());
}

TEST_CASE ("Canonical text loaded and written again is the same, byte for byte", "[io][ProjectFile]")
{
    const auto expected = fixtureText ("berlin_test_canonical.xml");

    const auto once = io::ProjectFile::toCanonicalXml (juce::ValueTree::fromXml (expected));
    const auto twice = io::ProjectFile::toCanonicalXml (juce::ValueTree::fromXml (once));

    CHECK (once.toStdString() == expected.toStdString());
    CHECK (twice.toStdString() == once.toStdString());
}

TEST_CASE ("A document written canonically is still valid and reads the same", "[io][ProjectFile]")
{
    const auto original = nodeFromXml (fixtureText ("berlin_test.xml").toRawUTF8());
    const auto saved = reloaded (original);

    CHECK (model::Validator::validate (saved).empty());

    const model::ProjectModel before (original);
    const model::ProjectModel after (saved);

    CHECK (after.getPpq() == before.getPpq());
    CHECK (after.getSession().getLaunchQuantise() == before.getSession().getLaunchQuantise());
    CHECK (after.getSettings().getSharedEditPolicy() == before.getSettings().getSharedEditPolicy());

    const auto bassBefore = before.getClipPool().getClips()[0].getGrid();
    const auto bassAfter = after.getClipPool().getClips()[0].getGrid();

    CHECK (bassAfter.getStepLength() == bassBefore.getStepLength());
    CHECK (bassAfter.getLoopLength() == 1200);
    CHECK (bassAfter.getLanes()[0].getCells()[3].isOn() == false);
    CHECK (bassAfter.getLanes()[0].getCells()[4].getGate() == 0.5);
    CHECK (bassAfter.getLanes()[0].getCells()[4].getVelocity() == 95);

    const auto drumsAfter = after.getClipPool().getClips()[1].getGrid();

    CHECK (drumsAfter.getStepCount() == 16);
    CHECK (drumsAfter.getResetAt() == 16);
    CHECK (drumsAfter.getLanes()[1].getCells()[2].getVelocity() == 70);

    CHECK (after.getTracks().getTracks()[0].getChannel() == 1);
    CHECK (after.getTracks().getTracks()[1].getChannel() == 10);
}

//==============================================================================
// The shape of the text.

TEST_CASE ("Canonical text has an XML header, one node per line and \\n line endings", "[io][ProjectFile]")
{
    auto track = nodeFromXml (R"(<Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b" name="A track with a name long enough to make JUCE wrap its attributes"
                                        colour="#ff336699" playerId="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" channel="7" mute="1"/>)");

    const auto text = canonical (projectHolding (track));

    CHECK (text == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "\n"
                   "<Project schemaVersion=\"1\">\n"
                   "  <Track id=\"4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b\" name=\"A track with a name long enough to make JUCE wrap its attributes\""
                   " colour=\"#ff336699\" playerId=\"9b1d6f3a2c4e4f708192a3b4c5d6e7f8\" channel=\"7\" mute=\"1\"/>\n"
                   "</Project>\n");
}

TEST_CASE ("A name outside ASCII and with XML's own characters comes back as it was", "[io][ProjectFile]")
{
    const auto name = juce::String::fromUTF8 ("Bässe & <Höhen> \"laut\" \xe2\x80\xa6");

    juce::ValueTree project (ids::Project);
    project.setProperty (ids::schemaVersion, 1, nullptr);
    project.setProperty (ids::name, name, nullptr);

    const auto text = io::ProjectFile::toCanonicalXml (project);

    CHECK (text.containsOnly (juce::String::fromUTF8 ("\n") + juce::String (" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~")));
    CHECK (juce::ValueTree::fromXml (text)[ids::name].toString() == name);
}

//==============================================================================
// Property order.

TEST_CASE ("Properties are written in the order of Ids.h, whatever order they were set in", "[io][ProjectFile]")
{
    juce::ValueTree placement (ids::Placement);
    placement.setProperty (ids::offset, 480, nullptr);
    placement.setProperty (ids::length, 7680, nullptr);
    placement.setProperty (ids::start, 0, nullptr);
    placement.setProperty (ids::clipId, "5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1", nullptr);
    placement.setProperty (ids::id, "1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a", nullptr);

    CHECK (lineOf (placement) == R"(<Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="0" length="7680" offset="480"/>)");
}

//==============================================================================
// Number and boolean formats.

TEST_CASE ("Numbers and booleans set in this session are written in their text forms", "[io][ProjectFile]")
{
    juce::ValueTree note (ids::Note);
    note.setProperty (ids::tick, static_cast<juce::int64> (9000000000), nullptr);
    note.setProperty (ids::length, 240, nullptr);
    note.setProperty (ids::pitch, 60, nullptr);
    note.setProperty (ids::enabled, false, nullptr);

    CHECK (lineOf (note) == R"(<Note tick="9000000000" length="240" pitch="60" enabled="0"/>)");

    juce::ValueTree player (ids::Player);
    player.setProperty (ids::id, "9b1d6f3a2c4e4f708192a3b4c5d6e7f8", nullptr);
    player.setProperty (ids::name, "Minimoog", nullptr);
    player.setProperty (ids::sendClock, true, nullptr);

    CHECK (lineOf (player) == R"(<Player id="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" name="Minimoog" sendClock="1"/>)");
}

TEST_CASE ("A double is written with six significant digits", "[io][ProjectFile]")
{
    const auto tempoLine = [] (double bpm) {
        juce::ValueTree tempoEvent (ids::TempoEvent);
        tempoEvent.setProperty (ids::tick, 0, nullptr);
        tempoEvent.setProperty (ids::bpm, bpm, nullptr);

        return lineOf (tempoEvent);
    };

    CHECK (tempoLine (118.0) == R"(<TempoEvent tick="0" bpm="118"/>)");
    CHECK (tempoLine (118.123) == R"(<TempoEvent tick="0" bpm="118.123"/>)");
    CHECK (tempoLine (118.123456789) == R"(<TempoEvent tick="0" bpm="118.123"/>)");
    CHECK (tempoLine (999.999) == R"(<TempoEvent tick="0" bpm="999.999"/>)");

    juce::ValueTree cell (ids::Cell);
    cell.setProperty (ids::gate, 1.0 / 3.0, nullptr);

    CHECK (lineOf (cell) == R"(<Cell gate="0.333333"/>)");

    cell.setProperty (ids::gate, 0.00001, nullptr);

    CHECK (lineOf (cell) == R"(<Cell gate="1e-05"/>)");
}

TEST_CASE ("A number spelled another way in a loaded file is written in the one canonical way", "[io][ProjectFile]")
{
    CHECK (lineOf (nodeFromXml (R"(<TempoEvent tick="00" bpm="118.0"/>)")) == R"(<TempoEvent tick="0" bpm="118"/>)");
    CHECK (lineOf (nodeFromXml (R"(<Knob cc="074" label="Cutoff"/>)")) == R"(<Knob cc="74" label="Cutoff"/>)");

    // And a default spelled another way is still the default.
    CHECK (lineOf (nodeFromXml (R"(<Cell velocity="0100" gate="5e-1"/>)")) == "<Cell/>");
}

TEST_CASE ("A value that is not in the text form of its type is written as stored, not repaired", "[io][ProjectFile]")
{
    // The Validator refuses such a document; the writer is not the place
    // where it gets mended.
    CHECK (lineOf (nodeFromXml (R"(<Knob cc="seventy-four" label="Cutoff"/>)")) == R"(<Knob cc="seventy-four" label="Cutoff"/>)");
    CHECK (lineOf (nodeFromXml (R"(<Cell on="yes" gate="half"/>)")) == R"(<Cell on="yes" gate="half"/>)");
}

//==============================================================================
// Defaults are left out. Each test spells out, on a node with nothing but its
// required properties, every default the typed wrapper returns, and expects
// the same text as without them: the writer's defaults are the wrappers'.

TEST_CASE ("A Project's defaults are not written", "[io][ProjectFile]")
{
    juce::ValueTree bare (ids::Project);
    bare.setProperty (ids::schemaVersion, 1, nullptr);

    const model::ProjectModel wrapper (bare);

    auto spelledOut = bare.createCopy();
    spelledOut.setProperty (ids::name, wrapper.getName(), nullptr);
    spelledOut.setProperty (ids::ppq, wrapper.getPpq(), nullptr);

    CHECK (canonical (spelledOut) == canonical (bare));
    CHECK (juce::String (canonical (bare)).contains (R"(<Project schemaVersion="1"/>)"));
}

TEST_CASE ("The defaults of Routing's nodes are not written", "[io][ProjectFile]")
{
    const auto performer = nodeFromXml (R"(<Performer id="3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1" name="Keyboard"/>)");
    const model::PerformerModel performerWrapper (performer);

    auto performerSpelledOut = performer.createCopy();
    performerSpelledOut.setProperty (ids::deviceIdentifier, performerWrapper.getDeviceIdentifier(), nullptr);
    performerSpelledOut.setProperty (ids::deviceName, performerWrapper.getDeviceName(), nullptr);
    performerSpelledOut.setProperty (ids::channelFilter, performerWrapper.getChannelFilter(), nullptr);

    CHECK (lineOf (performerSpelledOut) == R"(<Performer id="3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1" name="Keyboard"/>)");

    const auto player = nodeFromXml (R"(<Player id="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" name="Minimoog"/>)");
    const model::PlayerModel playerWrapper (player);

    auto playerSpelledOut = player.createCopy();
    playerSpelledOut.setProperty (ids::deviceIdentifier, playerWrapper.getDeviceIdentifier(), nullptr);
    playerSpelledOut.setProperty (ids::deviceName, playerWrapper.getDeviceName(), nullptr);
    playerSpelledOut.setProperty (ids::sendClock, playerWrapper.getSendClock(), nullptr);

    CHECK (lineOf (playerSpelledOut) == R"(<Player id="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" name="Minimoog"/>)");

    const auto pad = nodeFromXml (R"(<Pad index="0" inputPitch="36" pitch="36"/>)");

    auto padSpelledOut = pad.createCopy();
    padSpelledOut.setProperty (ids::label, model::PadModel (pad).getLabel(), nullptr);

    CHECK (lineOf (padSpelledOut) == R"(<Pad index="0" inputPitch="36" pitch="36"/>)");
}

TEST_CASE ("The defaults of a Marker, a Scene and the Session are not written", "[io][ProjectFile]")
{
    const auto marker = nodeFromXml (R"(<Marker tick="3840"/>)");

    auto markerSpelledOut = marker.createCopy();
    markerSpelledOut.setProperty (ids::name, model::MarkerModel (marker).getName(), nullptr);

    CHECK (lineOf (markerSpelledOut) == R"(<Marker tick="3840"/>)");

    const auto scene = nodeFromXml (R"(<Scene index="2"/>)");

    auto sceneSpelledOut = scene.createCopy();
    sceneSpelledOut.setProperty (ids::name, model::SceneModel (scene).getName(), nullptr);

    CHECK (lineOf (sceneSpelledOut) == R"(<Scene index="2"/>)");

    const auto session = nodeFromXml ("<Session/>");

    auto sessionSpelledOut = session.createCopy();
    sessionSpelledOut.setProperty (ids::launchQuantise, static_cast<juce::int64> (model::SessionModel (session).getLaunchQuantise()), nullptr);

    CHECK (lineOf (sessionSpelledOut) == "<Session/>");

    // 0 is "immediate", not the default.
    sessionSpelledOut.setProperty (ids::launchQuantise, 0, nullptr);

    CHECK (lineOf (sessionSpelledOut) == R"(<Session launchQuantise="0"/>)");
}

TEST_CASE ("A Clip's defaults are not written", "[io][ProjectFile]")
{
    const auto clip = nodeFromXml (R"(<Clip id="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" kind="event" loopLength="3840"/>)");
    const model::ClipModel wrapper (clip);

    auto spelledOut = clip.createCopy();
    spelledOut.setProperty (ids::name, wrapper.getName(), nullptr);
    spelledOut.setProperty (ids::quantise, wrapper.getQuantise(), nullptr);
    spelledOut.setProperty (ids::colour, wrapper.getColour(), nullptr);

    CHECK (lineOf (spelledOut) == R"(<Clip id="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" kind="event" loopLength="3840"/>)");
}

TEST_CASE ("The defaults of a Note and an Event are not written", "[io][ProjectFile]")
{
    const auto note = nodeFromXml (R"(<Note tick="0" length="240" pitch="60"/>)");
    const model::NoteModel noteWrapper (note);

    auto noteSpelledOut = note.createCopy();
    noteSpelledOut.setProperty (ids::velocity, noteWrapper.getVelocity(), nullptr);
    noteSpelledOut.setProperty (ids::offVelocity, noteWrapper.getOffVelocity(), nullptr);
    noteSpelledOut.setProperty (ids::enabled, noteWrapper.isEnabled(), nullptr);

    CHECK (lineOf (noteSpelledOut) == R"(<Note tick="0" length="240" pitch="60"/>)");

    const auto event = nodeFromXml (R"(<Event tick="0" kind="sysex"/>)");
    const model::EventModel eventWrapper (event);

    auto eventSpelledOut = event.createCopy();
    eventSpelledOut.setProperty (ids::a, eventWrapper.getA(), nullptr);
    eventSpelledOut.setProperty (ids::b, eventWrapper.getB(), nullptr);
    eventSpelledOut.setProperty (ids::data, eventWrapper.getData(), nullptr);
    eventSpelledOut.setProperty (ids::enabled, eventWrapper.isEnabled(), nullptr);

    CHECK (lineOf (eventSpelledOut) == R"(<Event tick="0" kind="sysex"/>)");
}

TEST_CASE ("A Grid's defaults are not written", "[io][ProjectFile]")
{
    const auto grid = nodeFromXml ("<Grid/>");
    const model::GridModel wrapper (grid);

    auto spelledOut = grid.createCopy();
    spelledOut.setProperty (ids::stepLength, static_cast<juce::int64> (wrapper.getStepLength()), nullptr);
    spelledOut.setProperty (ids::stepCount, wrapper.getStepCount(), nullptr);
    spelledOut.setProperty (ids::resetAt, wrapper.getResetAt(), nullptr);
    spelledOut.setProperty (ids::skipMask, wrapper.getSkipMask(), nullptr);

    CHECK (lineOf (spelledOut) == "<Grid/>");
}

TEST_CASE ("resetAt and skipMask are left out when they follow from the Grid's own stepCount", "[io][ProjectFile]")
{
    const auto grid = nodeFromXml (R"(<Grid stepCount="8"/>)");
    const model::GridModel wrapper (grid);

    auto spelledOut = grid.createCopy();
    spelledOut.setProperty (ids::resetAt, wrapper.getResetAt(), nullptr);
    spelledOut.setProperty (ids::skipMask, wrapper.getSkipMask(), nullptr);

    REQUIRE (spelledOut[ids::resetAt].toString() == "8");
    REQUIRE (spelledOut[ids::skipMask].toString() == "00000000");

    CHECK (lineOf (spelledOut) == R"(<Grid stepCount="8"/>)");

    // What is the default for sixteen columns is none for eight, and a
    // column that is skipped is written.
    CHECK (lineOf (nodeFromXml (R"(<Grid stepCount="8" resetAt="6" skipMask="00010000"/>)")) == R"(<Grid stepCount="8" resetAt="6" skipMask="00010000"/>)");
    CHECK (lineOf (nodeFromXml (R"(<Grid resetAt="8"/>)")) == R"(<Grid resetAt="8"/>)");
    CHECK (lineOf (nodeFromXml (R"(<Grid stepCount="8" skipMask="0000000000000000"/>)")) == R"(<Grid stepCount="8" skipMask="0000000000000000"/>)");
}

TEST_CASE ("The defaults of a Lane and a Cell are not written", "[io][ProjectFile]")
{
    const auto lane = nodeFromXml (R"(<Lane id="2b4d6f8a0c1e4a3b8d5f7a9c1e3b5d7f"/>)");

    auto laneSpelledOut = lane.createCopy();
    laneSpelledOut.setProperty (ids::name, model::LaneModel (lane).getName(), nullptr);

    CHECK (lineOf (laneSpelledOut) == R"(<Lane id="2b4d6f8a0c1e4a3b8d5f7a9c1e3b5d7f"/>)");

    // fixedPitch has no default: a Lane without it is a melodic row.
    CHECK_FALSE (model::LaneModel (lane).getFixedPitch().has_value());

    const auto cell = nodeFromXml ("<Cell/>");
    const model::CellModel cellWrapper (cell);

    auto cellSpelledOut = cell.createCopy();
    cellSpelledOut.setProperty (ids::pitch, cellWrapper.getPitch(), nullptr);
    cellSpelledOut.setProperty (ids::on, cellWrapper.isOn(), nullptr);
    cellSpelledOut.setProperty (ids::velocity, cellWrapper.getVelocity(), nullptr);
    cellSpelledOut.setProperty (ids::gate, cellWrapper.getGate(), nullptr);

    CHECK (lineOf (cellSpelledOut) == "<Cell/>");
}

TEST_CASE ("The defaults of a Track and of what hangs under it are not written", "[io][ProjectFile]")
{
    const auto track = nodeFromXml (R"(<Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b"/>)");
    const model::TrackModel trackWrapper (track);

    auto trackSpelledOut = track.createCopy();
    trackSpelledOut.setProperty (ids::name, trackWrapper.getName(), nullptr);
    trackSpelledOut.setProperty (ids::colour, trackWrapper.getColour(), nullptr);
    trackSpelledOut.setProperty (ids::playerId, trackWrapper.getPlayerId(), nullptr);
    trackSpelledOut.setProperty (ids::channel, trackWrapper.getChannel(), nullptr);
    trackSpelledOut.setProperty (ids::performerId, trackWrapper.getPerformerId(), nullptr);
    trackSpelledOut.setProperty (ids::mute, trackWrapper.isMuted(), nullptr);
    trackSpelledOut.setProperty (ids::solo, trackWrapper.isSoloed(), nullptr);

    CHECK (lineOf (trackSpelledOut) == R"(<Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b"/>)");

    const auto placement = nodeFromXml (R"(<Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="0" length="7680"/>)");

    auto placementSpelledOut = placement.createCopy();
    placementSpelledOut.setProperty (ids::offset, static_cast<juce::int64> (model::PlacementModel (placement).getOffset()), nullptr);

    CHECK (lineOf (placementSpelledOut) == R"(<Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="0" length="7680"/>)");

    const auto knob = nodeFromXml (R"(<Knob cc="74"/>)");

    auto knobSpelledOut = knob.createCopy();
    knobSpelledOut.setProperty (ids::label, model::KnobModel (knob).getLabel(), nullptr);

    CHECK (lineOf (knobSpelledOut) == R"(<Knob cc="74"/>)");

    // An AutomationLane's name, like every name, is left out when empty.
    CHECK (lineOf (nodeFromXml (R"(<AutomationLane id="6d8f0a2c4e6b4d1f8a3c5e7a9b1d3f50" name=""/>)")) == R"(<AutomationLane id="6d8f0a2c4e6b4d1f8a3c5e7a9b1d3f50"/>)");
}

TEST_CASE ("UiState's defaults are not written", "[io][ProjectFile]")
{
    // Written through the settings path, which is how they get there.
    auto uiState = nodeFromXml (R"(<UiState activeTab="stepseq" sharedEditPolicy="makeUnique" loopStart="3840" loopEnd="7680" loopEnabled="1" exportLength="15360"/>)");

    CHECK (lineOf (uiState) == R"(<UiState activeTab="stepseq" sharedEditPolicy="makeUnique" loopStart="3840" loopEnd="7680" loopEnabled="1" exportLength="15360"/>)");

    const model::Settings defaults (nodeFromXml ("<UiState/>"));

    model::Settings settings (uiState);
    settings.setActiveTab (defaults.getActiveTab());
    settings.setSharedEditPolicy (defaults.getSharedEditPolicy());
    settings.setTransportLoop (defaults.getTransportLoop());
    settings.setExportLength (defaults.getExportLength());

    CHECK (lineOf (uiState) == "<UiState/>");
}

TEST_CASE ("A property that is not the default is written, also when it is another node type's default", "[io][ProjectFile]")
{
    // 60 is the default pitch of a Cell; a Note and a Pad have none.
    CHECK (lineOf (nodeFromXml (R"(<Note tick="0" length="240" pitch="60" velocity="64" offVelocity="100"/>)")) == R"(<Note tick="0" length="240" pitch="60" velocity="64" offVelocity="100"/>)");
    CHECK (lineOf (nodeFromXml (R"(<Pad index="0" inputPitch="60" pitch="60"/>)")) == R"(<Pad index="0" inputPitch="60" pitch="60"/>)");
}

//==============================================================================
// Child order, and what Hums does not know.

TEST_CASE ("Children are written in stored order, never sorted", "[io][ProjectFile]")
{
    const auto project = nodeFromXml (
        R"(<Project schemaVersion="1">
             <Events>
               <Event tick="0" kind="cc" a="32" b="1"/>
               <Note tick="480" length="240" pitch="62"/>
               <Event tick="0" kind="cc" b="5"/>
               <Event tick="0" kind="programChange" a="12"/>
               <Note tick="0" length="240" pitch="60"/>
             </Events>
           </Project>)");

    CHECK (canonical (project) == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                  "\n"
                                  "<Project schemaVersion=\"1\">\n"
                                  "  <Events>\n"
                                  "    <Event tick=\"0\" kind=\"cc\" a=\"32\" b=\"1\"/>\n"
                                  "    <Note tick=\"480\" length=\"240\" pitch=\"62\"/>\n"
                                  "    <Event tick=\"0\" kind=\"cc\" b=\"5\"/>\n"
                                  "    <Event tick=\"0\" kind=\"programChange\" a=\"12\"/>\n"
                                  "    <Note tick=\"0\" length=\"240\" pitch=\"60\"/>\n"
                                  "  </Events>\n"
                                  "</Project>\n");
}

TEST_CASE ("Unknown properties are kept, after the declared ones and in stored order", "[io][ProjectFile]")
{
    const auto knob = nodeFromXml (R"(<Knob zoom="3" label="Cutoff" curve="log" cc="74"/>)");

    CHECK (lineOf (knob) == R"(<Knob cc="74" label="Cutoff" zoom="3" curve="log"/>)");
}

TEST_CASE ("An unknown property is written as stored, also when it looks like a default", "[io][ProjectFile]")
{
    // velocity is declared, but the schema lists none for a Knob, so nothing
    // is known about its default there. swing is not declared at all.
    const auto knob = nodeFromXml (R"(<Knob cc="74" swing="" velocity="0100"/>)");

    CHECK (lineOf (knob) == R"(<Knob cc="74" velocity="0100" swing=""/>)");
}

TEST_CASE ("Unknown children are kept in their place, with everything in them", "[io][ProjectFile]")
{
    const auto project = nodeFromXml (
        R"(<Project schemaVersion="1">
             <Cell on="1">
               <Ratchet count="03" velocity="100" pitch="60"/>
               <CcValue cc="74" value="90"/>
               <Ratchet count="2">
                 <Cell on="0" curve=""/>
               </Ratchet>
             </Cell>
           </Project>)");

    CHECK (canonical (project) == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                  "\n"
                                  "<Project schemaVersion=\"1\">\n"
                                  "  <Cell on=\"1\">\n"
           // Nothing is known about a Ratchet: its values stay as stored and
           // none is taken for a default. Only the order of Ids.h applies.
                                  "    <Ratchet pitch=\"60\" velocity=\"100\" count=\"03\"/>\n"
                                  "    <CcValue cc=\"74\" value=\"90\"/>\n"
                                  "    <Ratchet count=\"2\">\n"
           // A node of a known type is written by its type's rules wherever
           // it stands.
                                  "      <Cell curve=\"\"/>\n"
                                  "    </Ratchet>\n"
                                  "  </Cell>\n"
                                  "</Project>\n");

    // And such a document, too, is the same text after a reload.
    CHECK (canonical (reloaded (project)) == canonical (project));
}

//==============================================================================
// Loading: parse, schemaVersion, migrations, Validator, and only then install.

namespace
{

using Outcome = io::ProjectFile::LoadResult::Outcome;

// A project that is open and has been worked on: a document, and an undo
// history with one edit in it. A refused load must leave both as they are.
struct OpenProject
{
    OpenProject()
    {
        document = nodeFromXml (fixtureText ("berlin_test.xml").toRawUTF8());
        treeBefore = document;

        undoManager.beginNewTransaction();
        document.setProperty (ids::name, "Edited since it was opened", &undoManager);
        REQUIRE (undoManager.canUndo());

        textBefore = canonical (document);
    }

    io::ProjectFile::LoadResult loadText (const juce::String& xmlText, const io::Migrations::Steps& steps = io::Migrations::steps())
    {
        return io::ProjectFile::loadFromText (xmlText, document, undoManager, steps);
    }

    // The document is the same tree with the same content, and the edit can
    // still be undone.
    bool isUntouched()
    {
        return document == treeBefore && canonical (document) == textBefore && undoManager.canUndo();
    }

    juce::ValueTree document;
    juce::UndoManager undoManager;

    juce::ValueTree treeBefore;
    std::string textBefore;
};

// A valid project, as small as one can be, with room for a version and for
// more properties on the Project node.
juce::String smallProject (const juce::String& projectAttributes)
{
    return "<Project " + projectAttributes + R"(>
              <Routing><PadMap/></Routing>
              <Timeline>
                <TempoEvent tick="0" bpm="120"/>
                <TimeSigEvent tick="0" numerator="4" denominator="4"/>
              </Timeline>
              <ClipPool/>
              <Tracks/>
              <Session/>
              <UiState/>
            </Project>)";
}

struct CountingListener : juce::ValueTree::Listener
{
    void valueTreeRedirected (juce::ValueTree&) override { ++redirections; }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { ++otherCallbacks; }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { ++otherCallbacks; }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { ++otherCallbacks; }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { ++otherCallbacks; }
    void valueTreeParentChanged (juce::ValueTree&) override { ++otherCallbacks; }

    int redirections = 0;
    int otherCallbacks = 0;
};

} // namespace

TEST_CASE ("The small project of the load tests is a valid document", "[io][ProjectFile][load]")
{
    // The refusals below each change one thing in it; they mean something
    // only if the rest is in order.
    const auto project = nodeFromXml (smallProject (R"(schemaVersion="1")").toRawUTF8());

    CHECK (model::Validator::validate (project).empty());
}

TEST_CASE ("A valid file is loaded: it becomes the document and the undo history is empty", "[io][ProjectFile][load]")
{
    OpenProject open;
    const auto file = juce::File (HUMS_IO_FIXTURES_DIRECTORY).getChildFile ("berlin_test.xml");

    const auto result = io::ProjectFile::load (file, open.document, open.undoManager);

    CHECK (result.wasLoaded());
    CHECK (result.outcome == Outcome::loaded);
    CHECK (result.message.isEmpty());
    CHECK (result.violations.empty());

    CHECK (canonical (open.document) == fixtureText ("berlin_test_canonical.xml").toStdString());
    CHECK_FALSE (open.undoManager.canUndo());
    CHECK_FALSE (open.undoManager.canRedo());
}

TEST_CASE ("Loading lets go of the previous tree and does not change it", "[io][ProjectFile][load]")
{
    OpenProject open;

    REQUIRE (open.loadText (smallProject (R"(schemaVersion="1" name="Another")")).wasLoaded());

    CHECK (open.document != open.treeBefore);
    CHECK (model::ProjectModel (open.document).getName() == "Another");
    CHECK (canonical (open.treeBefore) == open.textBefore);
}

TEST_CASE ("A listener on the document hears of a load once, as a redirection", "[io][ProjectFile][load]")
{
    OpenProject open;
    CountingListener listener;
    open.document.addListener (&listener);

    REQUIRE (open.loadText (smallProject (R"(schemaVersion="1")")).wasLoaded());

    CHECK (listener.redirections == 1);
    CHECK (listener.otherCallbacks == 0);

    // And it is still listening, now to the loaded document.
    open.document.setProperty (ids::name, "Renamed", nullptr);
    CHECK (listener.otherCallbacks == 1);

    open.document.removeListener (&listener);
}

TEST_CASE ("A refused load tells a listener nothing", "[io][ProjectFile][load]")
{
    OpenProject open;
    CountingListener listener;
    open.document.addListener (&listener);

    REQUIRE_FALSE (open.loadText (smallProject (R"(schemaVersion="1" ppq="480")")).wasLoaded());

    CHECK (listener.redirections == 0);
    CHECK (listener.otherCallbacks == 0);

    open.document.removeListener (&listener);
}

TEST_CASE ("The first project of a session loads into a document that is not there yet", "[io][ProjectFile][load]")
{
    juce::ValueTree document;
    juce::UndoManager undoManager;

    const auto result = io::ProjectFile::loadFromText (smallProject (R"(schemaVersion="1")"), document, undoManager);

    CHECK (result.wasLoaded());
    CHECK (document.hasType (ids::Project));
}

TEST_CASE ("A valid file written by hand loads, and what Hums does not know in it is kept", "[io][ProjectFile][load]")
{
    OpenProject open;

    // Not canonical (a default spelled out, a number with a leading zero),
    // and with a property and a child from a newer build.
    const auto text = juce::String (R"(<?xml version="1.0" encoding="UTF-8"?>
        <!-- a comment -->
        <Project ppq="960" swing="0.2" schemaVersion="01">
          <Routing><PadMap/></Routing>
          <Timeline>
            <TempoEvent tick="0" bpm="120.0"/>
            <TimeSigEvent tick="0" numerator="4" denominator="4"/>
          </Timeline>
          <ClipPool/>
          <Tracks/>
          <Session/>
          <UiState/>
          <Mixer><Strip gain="3"/></Mixer>
        </Project>)");

    REQUIRE (open.loadText (text).wasLoaded());

    CHECK (canonical (open.document) == "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                        "\n"
                                        "<Project schemaVersion=\"1\" swing=\"0.2\">\n"
                                        "  <Routing>\n"
                                        "    <PadMap/>\n"
                                        "  </Routing>\n"
                                        "  <Timeline>\n"
                                        "    <TempoEvent tick=\"0\" bpm=\"120\"/>\n"
                                        "    <TimeSigEvent tick=\"0\" numerator=\"4\" denominator=\"4\"/>\n"
                                        "  </Timeline>\n"
                                        "  <ClipPool/>\n"
                                        "  <Tracks/>\n"
                                        "  <Session/>\n"
                                        "  <UiState/>\n"
                                        "  <Mixer>\n"
                                        "    <Strip gain=\"3\"/>\n"
                                        "  </Mixer>\n"
                                        "</Project>\n");
}

TEST_CASE ("Canonical text that is loaded and saved is the same text", "[io][ProjectFile][load]")
{
    OpenProject open;
    const auto text = fixtureText ("berlin_test_canonical.xml");

    REQUIRE (open.loadText (text).wasLoaded());

    CHECK (canonical (open.document) == text.toStdString());
}

//------------------------------------------------------------------------------
// Refusals. Each leaves the open project as it was.

TEST_CASE ("A file that is not there is refused", "[io][ProjectFile][load]")
{
    OpenProject open;
    const auto file = juce::File (HUMS_IO_FIXTURES_DIRECTORY).getChildFile ("no_such_file.xml");

    const auto result = io::ProjectFile::load (file, open.document, open.undoManager);

    CHECK (result.outcome == Outcome::fileNotReadable);
    CHECK (result.message.contains ("no_such_file.xml"));
    CHECK (open.isUntouched());
}

TEST_CASE ("A directory is refused", "[io][ProjectFile][load]")
{
    OpenProject open;

    const auto result = io::ProjectFile::load (juce::File (HUMS_IO_FIXTURES_DIRECTORY), open.document, open.undoManager);

    CHECK (result.outcome == Outcome::fileNotReadable);
    CHECK (open.isUntouched());
}

TEST_CASE ("Malformed XML is refused", "[io][ProjectFile][load]")
{
    OpenProject open;

    const std::vector<juce::String> texts {
        "",
        "this is no XML",
        R"(<Project schemaVersion="1">)",                           // never closed
        R"(<Project schemaVersion="1"><Routing></Project>)",        // unmatched tags
        R"(<Project schemaVersion="1" name="unmatched quotes/>)",
        // JUCE's parser carries on after this one and gives an element.
        smallProject (R"(schemaVersion="1" name="&nosuchentity;")"),
    };

    for (const auto& text : texts)
    {
        INFO (text.toStdString());
        const auto result = open.loadText (text);

        CHECK (result.outcome == Outcome::malformedXml);
        CHECK (result.message.isNotEmpty());
        CHECK (open.isUntouched());
    }
}

TEST_CASE ("XML with text between its nodes is refused, not loaded without the text", "[io][ProjectFile][load]")
{
    OpenProject open;

    const auto result = open.loadText (
        R"(<Project schemaVersion="1">
             <Routing><PadMap>four pads</PadMap></Routing>
           </Project>)");

    CHECK (result.outcome == Outcome::notAProjectFile);
    CHECK (result.message.contains ("PadMap"));
    CHECK (open.isUntouched());
}

TEST_CASE ("XML whose root is not a Project is refused", "[io][ProjectFile][load]")
{
    OpenProject open;

    const auto result = open.loadText (R"(<Clip id="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" kind="event" loopLength="960"/>)");

    CHECK (result.outcome == Outcome::notAProjectFile);
    CHECK (result.message.contains ("Clip"));
    CHECK (open.isUntouched());
}

TEST_CASE ("A file without a schemaVersion, or with one that is no version, is refused", "[io][ProjectFile][load]")
{
    OpenProject open;

    const std::vector<juce::String> attributes {
        R"(name="No version")",
        R"(schemaVersion="")",
        R"(schemaVersion="one")",
        R"(schemaVersion="1.0")",
        R"(schemaVersion="0")",
        R"(schemaVersion="-1")",
    };

    for (const auto& projectAttributes : attributes)
    {
        INFO (projectAttributes.toStdString());
        const auto result = open.loadText (smallProject (projectAttributes));

        CHECK (result.outcome == Outcome::noSchemaVersion);
        REQUIRE (result.violations.size() == 1);
        CHECK (result.violations[0].nodePath == "Project");
        CHECK (result.violations[0].property == "schemaVersion");
        CHECK (result.message.contains ("schemaVersion"));
        CHECK (open.isUntouched());
    }
}

TEST_CASE ("A file of a newer schemaVersion is refused, and the message names the version", "[io][ProjectFile][load]")
{
    OpenProject open;

    // Otherwise a valid version 1 document: it is the number alone that
    // refuses it, and the newer build's data is not looked at.
    const auto result = open.loadText (smallProject (R"(schemaVersion="2")"));

    CHECK (result.outcome == Outcome::versionTooNew);
    CHECK (result.message.contains ("version 2"));
    CHECK (result.message.contains ("version 1"));
    REQUIRE (result.violations.size() == 1);
    CHECK (result.violations[0].nodePath == "Project");
    CHECK (result.violations[0].property == "schemaVersion");
    CHECK (open.isUntouched());

    // Far beyond an int, too.
    CHECK (open.loadText (smallProject (R"(schemaVersion="4000000000")")).outcome == Outcome::versionTooNew);
    CHECK (open.isUntouched());
}

TEST_CASE ("A file that breaks an invariant is refused, with the node and property", "[io][ProjectFile][load]")
{
    OpenProject open;

    const auto result = open.loadText (smallProject (R"(schemaVersion="1" ppq="480")"));

    CHECK (result.outcome == Outcome::invalidDocument);
    REQUIRE (result.violations.size() == 1);
    CHECK (result.violations[0].nodePath == "Project");
    CHECK (result.violations[0].property == "ppq");
    CHECK (result.message.contains ("Project, ppq"));
    CHECK (result.message.contains (result.violations[0].reason));
    CHECK (open.isUntouched());
}

TEST_CASE ("Every violation is reported, and the message says how many more there are", "[io][ProjectFile][load]")
{
    OpenProject open;

    const auto result = open.loadText (
        R"(<Project schemaVersion="1" ppq="480">
             <Routing><PadMap/></Routing>
             <Timeline>
               <TempoEvent tick="0" bpm="fast"/>
               <TimeSigEvent tick="0" numerator="4" denominator="4"/>
             </Timeline>
             <ClipPool/>
             <Tracks/>
             <Session/>
             <UiState loopEnabled="yes"/>
           </Project>)");

    CHECK (result.outcome == Outcome::invalidDocument);
    REQUIRE (result.violations.size() == 3);
    CHECK (result.message.contains ("(and 2 more)"));
    CHECK (open.isUntouched());
}

TEST_CASE ("A value is never repaired on load: what the Validator refuses stays refused", "[io][ProjectFile][load]")
{
    OpenProject open;

    // Each would be easy to clamp, round or default [D17].
    const std::vector<juce::String> texts {
        smallProject (R"(schemaVersion="1")").replace (R"(bpm="120")", R"(bpm="118.1234567")"),
        smallProject (R"(schemaVersion="1")").replace ("<Session/>", R"(<Session launchQuantise="-1"/>)"),
        smallProject (R"(schemaVersion="1")").replace ("<UiState/>", R"(<UiState activeTab="mixer"/>)"),
    };

    for (const auto& text : texts)
    {
        INFO (text.toStdString());

        CHECK (open.loadText (text).outcome == Outcome::invalidDocument);
        CHECK (open.isUntouched());
    }
}

TEST_CASE ("A refused load leaves the file as it was", "[io][ProjectFile][load]")
{
    OpenProject open;
    const juce::TemporaryFile temporary (".xml");
    const auto file = temporary.getFile();

    const std::vector<juce::String> texts {
        "<Project",
        smallProject (R"(schemaVersion="2")"),
        smallProject (R"(schemaVersion="1" ppq="480")"),
    };

    for (const auto& text : texts)
    {
        INFO (text.toStdString());
        // JUCE would otherwise write "\r\n" line endings.
        REQUIRE (file.replaceWithText (text, false, false, "\n"));
        const auto modifiedBefore = file.getLastModificationTime();

        CHECK_FALSE (io::ProjectFile::load (file, open.document, open.undoManager).wasLoaded());

        CHECK (file.loadFileAsString() == text);
        CHECK (file.getLastModificationTime() == modifiedBefore);
        CHECK (open.isUntouched());
    }
}

//------------------------------------------------------------------------------
// Migrations. No step exists at version 1, so the tests hand in their own.

namespace
{

// Stands in for a format change: it leaves its mark in the Project's name,
// which every version has.
io::Migrations::Step appending (const juce::String& mark)
{
    return [mark] (juce::ValueTree& candidate) {
        candidate.setProperty (ids::name, candidate[ids::name].toString() + mark, nullptr);
    };
}

} // namespace

TEST_CASE ("An older file runs through every step from its version on, in order", "[io][ProjectFile][load]")
{
    OpenProject open;
    const io::Migrations::Steps steps { appending (" 1to2"), appending (" 2to3") };

    REQUIRE (open.loadText (smallProject (R"(schemaVersion="1" name="Old")"), steps).wasLoaded());

    const model::ProjectModel project (open.document);
    CHECK (project.getName() == "Old 1to2 2to3");
    CHECK (project.getSchemaVersion() == 3);
    CHECK_FALSE (open.undoManager.canUndo());
}

TEST_CASE ("A file runs only the steps after its own version", "[io][ProjectFile][load]")
{
    OpenProject open;
    const io::Migrations::Steps steps { appending (" 1to2"), appending (" 2to3") };

    SECTION ("one version behind")
    {
        REQUIRE (open.loadText (smallProject (R"(schemaVersion="2" name="Old")"), steps).wasLoaded());

        CHECK (model::ProjectModel (open.document).getName() == "Old 2to3");
        CHECK (model::ProjectModel (open.document).getSchemaVersion() == 3);
    }

    SECTION ("current")
    {
        REQUIRE (open.loadText (smallProject (R"(schemaVersion="3" name="Old")"), steps).wasLoaded());

        CHECK (model::ProjectModel (open.document).getName() == "Old");
    }

    SECTION ("newer than the steps reach")
    {
        CHECK (open.loadText (smallProject (R"(schemaVersion="4" name="Old")"), steps).outcome == Outcome::versionTooNew);
        CHECK (open.isUntouched());
    }
}

TEST_CASE ("A migration step that throws refuses the load", "[io][ProjectFile][load]")
{
    OpenProject open;

    SECTION ("a std::exception, whose text is reported")
    {
        // It has already changed the candidate when it gives up.
        const io::Migrations::Steps steps {
            [] (juce::ValueTree& candidate) {
                candidate.removeAllChildren (nullptr);
                throw std::runtime_error ("a Clip without a kind");
            },
        };

        const auto result = open.loadText (smallProject (R"(schemaVersion="1")"), steps);

        CHECK (result.outcome == Outcome::migrationFailed);
        CHECK (result.message.contains ("from schemaVersion 1 to 2"));
        CHECK (result.message.contains ("a Clip without a kind"));
        CHECK (open.isUntouched());
    }

    SECTION ("anything else")
    {
        const io::Migrations::Steps steps { [] (juce::ValueTree&) { throw 42; } };

        const auto result = open.loadText (smallProject (R"(schemaVersion="1")"), steps);

        CHECK (result.outcome == Outcome::migrationFailed);
        CHECK (result.message.contains ("from schemaVersion 1 to 2"));
        CHECK (open.isUntouched());
    }

    SECTION ("the second of two steps")
    {
        const io::Migrations::Steps steps {
            appending (" 1to2"),
            [] (juce::ValueTree&) { throw std::runtime_error ("no"); },
        };

        const auto result = open.loadText (smallProject (R"(schemaVersion="1")"), steps);

        CHECK (result.outcome == Outcome::migrationFailed);
        CHECK (result.message.contains ("from schemaVersion 2 to 3"));
        CHECK (open.isUntouched());
    }
}

TEST_CASE ("The Validator runs after the migrations, on what they made", "[io][ProjectFile][load]")
{
    OpenProject open;

    SECTION ("a step can make a file valid that was not")
    {
        const io::Migrations::Steps steps {
            [] (juce::ValueTree& candidate) { candidate.setProperty (ids::ppq, 960, nullptr); },
        };

        CHECK (open.loadText (smallProject (R"(schemaVersion="1" ppq="480")"), steps).wasLoaded());
    }

    SECTION ("and what a step leaves invalid is refused")
    {
        const io::Migrations::Steps steps {
            [] (juce::ValueTree& candidate) { candidate.setProperty (ids::ppq, 480, nullptr); },
        };

        const auto result = open.loadText (smallProject (R"(schemaVersion="1")"), steps);

        CHECK (result.outcome == Outcome::invalidDocument);
        CHECK (open.isUntouched());
    }
}
