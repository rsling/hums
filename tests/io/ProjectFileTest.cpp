// io::ProjectFile: the native project file. The canonical writer, the load
// sequence and the atomic save, docs/SCHEMA.md -> Loading, saving, canonical
// form.

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

#include <filesystem>
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

// JUCE's parser reads a closing tag without looking at its name or at its
// end, and reads nothing after the root. io::XmlWellFormedness asks instead.
TEST_CASE ("XML that JUCE's parser would read although it is not well-formed is refused", "[io][ProjectFile][load]")
{
    OpenProject open;
    const auto valid = smallProject (R"(schemaVersion="1")");
    REQUIRE (open.loadText (valid).wasLoaded());

    const std::vector<juce::String> texts {
        valid + "\nand some more text",                                                      // text after the root
        valid + "\n" + valid,                                                                // a second root
        valid + "\n<Extra/>",
        valid.replace ("</Project>", "</Projekt>"),                                           // closed under another name
        valid.replace ("<Routing><PadMap/></Routing>", "<Routing><PadMap/></Timeline>"),
        valid.dropLastCharacters (1),                                                         // "</Project"
        valid.dropLastCharacters (4),                                                         // "</Proj"
        valid.dropLastCharacters (8),                                                         // "</"
        valid.replace ("</Project>", "</Project/>"),
        "some text first\n" + valid,
        valid + "\n<!-- a comment that is not closed",
    };

    for (const auto& text : texts)
    {
        OpenProject untouched;
        INFO (text.toStdString());
        const auto result = untouched.loadText (text);

        CHECK (result.outcome == Outcome::malformedXml);
        CHECK (result.message.isNotEmpty());
        CHECK (untouched.isUntouched());
    }
}

TEST_CASE ("The refusal of a closing tag names the node and the name it is closed with", "[io][ProjectFile][load]")
{
    OpenProject open;

    const auto result = open.loadText (smallProject (R"(schemaVersion="1")").replace ("<Routing><PadMap/></Routing>", "<Routing><PadMap/></Rooting>"));

    CHECK (result.outcome == Outcome::malformedXml);
    CHECK (result.message.contains ("Routing"));
    CHECK (result.message.contains ("Rooting"));
}

TEST_CASE ("What XML allows around the root is no reason to refuse a file", "[io][ProjectFile][load]")
{
    OpenProject open;

    const auto result = open.loadText ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!-- before -->\n" + smallProject (R"(schemaVersion="1" name='a > b')")
                                       + "\n<!-- after -->\n\n");

    CHECK (result.wasLoaded());
    CHECK (open.document[ids::name].toString() == "a > b");
}

// Well-formed XML, but Hums writes no DOCTYPE, and an entity declared in one
// would be written out in full at the next save.
TEST_CASE ("A file with a DOCTYPE is refused, whatever it declares", "[io][ProjectFile][load]")
{
    const std::vector<juce::String> texts {
        "<!DOCTYPE Project>\n" + smallProject (R"(schemaVersion="1")"),
        "<!DOCTYPE Project [ <!ENTITY song \"Berlin\"> ]>\n" + smallProject (R"(schemaVersion="1" name="&song;")"),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!-- a comment -->\n<!DOCTYPE Project>\n" + smallProject (R"(schemaVersion="1")"),
    };

    for (const auto& text : texts)
    {
        OpenProject open;
        INFO (text.toStdString());
        const auto result = open.loadText (text);

        CHECK (result.outcome == Outcome::notAProjectFile);
        CHECK (result.message.contains ("DOCTYPE"));
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

// JUCE's parser keeps both, and the document would hold two properties of one
// name: every reader sees the first, and a command that removes it (an offset
// that becomes 0) brings the second to light.
TEST_CASE ("A file with one attribute twice on a node is refused, and the message names both", "[io][ProjectFile][load]")
{
    OpenProject open;

    const auto text = fixtureText ("all_node_types.xml").replace (R"(length="7680" offset="480"/>)", R"(length="7680" offset="480" offset="900"/>)");
    REQUIRE (text.contains (R"(offset="900")"));

    const auto result = open.loadText (text);

    CHECK (result.outcome == Outcome::malformedXml);
    CHECK (result.message.contains ("Placement"));
    CHECK (result.message.contains ("offset"));
    CHECK (result.violations.empty());
    CHECK (open.isUntouched());
}

// Well-formed XML, but JUCE reads base64:offset as the property offset, so
// the document would hold that property twice all the same.
TEST_CASE ("A file with an attribute plain and as base64 on one node is refused", "[io][ProjectFile][load]")
{
    OpenProject open;

    const auto binaryData = juce::MemoryBlock ("x", 1).toBase64Encoding();
    const auto text = fixtureText ("all_node_types.xml")
                          .replace (R"(length="7680" offset="480"/>)", R"(length="7680" offset="480" base64:offset=")" + binaryData + R"("/>)");
    REQUIRE (text.contains ("base64:offset"));

    const auto result = open.loadText (text);

    CHECK (result.outcome == Outcome::notAProjectFile);
    CHECK (result.message.contains ("Placement"));
    CHECK (result.message.contains ("base64:offset"));
    CHECK (open.isUntouched());
}

// Only where JUCE reads the two as one property: a value that is no binary
// data in JUCE's encoding leaves "base64:offset" a name of its own, which is
// kept like any unknown property.
TEST_CASE ("An attribute named base64: whose value is no binary data is an unknown property like any other", "[io][ProjectFile][load]")
{
    OpenProject open;

    const auto text = fixtureText ("all_node_types.xml")
                          .replace (R"(length="7680" offset="480"/>)", R"(length="7680" offset="480" base64:offset="no binary data"/>)");
    REQUIRE (text.contains ("base64:offset"));

    CHECK (open.loadText (text).wasLoaded());
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

//------------------------------------------------------------------------------
// Saving.

namespace
{

using SaveOutcome = io::ProjectFile::SaveResult::Outcome;

// A directory of its own for one test, removed with everything in it when
// the test ends.
struct SaveDirectory
{
    SaveDirectory()
    {
        REQUIRE (directory.createDirectory().wasOk());
    }

    ~SaveDirectory()
    {
        // A test may have made it read-only.
        directory.setReadOnly (false, true);
        directory.deleteRecursively();
    }

    juce::File file (const juce::String& name) const { return directory.getChildFile (name); }

    // The names of everything in the directory, hidden files included, sorted.
    juce::StringArray fileNames() const
    {
        juce::StringArray names;

        for (const auto& found : directory.findChildFiles (juce::File::findFilesAndDirectories, false))
            names.add (found.getFileName());

        names.sort (false);

        return names;
    }

    // Named with a UUID. "The first name that is free" is the same name for
    // two tests that ask at the same moment (ctest -j), and each then took
    // the other's directory for its own.
    const juce::File directory { juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("hums_save_test_" + juce::Uuid().toString()) };
};

void writeText (const juce::File& file, const juce::String& text)
{
    // JUCE would otherwise write "\r\n" line endings.
    REQUIRE (file.replaceWithText (text, false, false, "\n"));
}

std::string textOf (const juce::File& file)
{
    return file.loadFileAsString().toStdString();
}

juce::ValueTree berlinProject()
{
    return nodeFromXml (fixtureText ("berlin_test.xml").toRawUTF8());
}

juce::ValueTree smallProjectNamed (const juce::String& name)
{
    auto project = nodeFromXml (smallProject (R"(schemaVersion="1")").toRawUTF8());
    project.setProperty (ids::name, name, nullptr);

    return project;
}

} // namespace

TEST_CASE ("A project saved where no file was is there in canonical form, and nothing else", "[io][ProjectFile][save]")
{
    const SaveDirectory directory;
    const auto file = directory.file ("song.xml");
    const auto project = berlinProject();

    const auto result = io::ProjectFile::save (project, file);

    CHECK (result.wasSaved());
    CHECK (result.outcome == SaveOutcome::saved);
    CHECK (result.message.isEmpty());

    CHECK (textOf (file) == canonical (project));
    // No backup, because there was nothing to keep, and no temporary file.
    CHECK (directory.fileNames() == juce::StringArray { "song.xml" });
}

TEST_CASE ("A saved project loads again as the same document", "[io][ProjectFile][save]")
{
    const SaveDirectory directory;
    const auto file = directory.file ("song.xml");
    const auto project = berlinProject();

    REQUIRE (io::ProjectFile::save (project, file).wasSaved());

    juce::ValueTree document;
    juce::UndoManager undoManager;

    REQUIRE (io::ProjectFile::load (file, document, undoManager).wasLoaded());
    CHECK (canonical (document) == canonical (project));
}

TEST_CASE ("Saving does not change the document", "[io][ProjectFile][save]")
{
    const SaveDirectory directory;
    const auto project = berlinProject();
    const auto copyBefore = project.createCopy();

    REQUIRE (io::ProjectFile::save (project, directory.file ("song.xml")).wasSaved());

    CHECK (project.isEquivalentTo (copyBefore));
}

TEST_CASE ("Saving over a file keeps the previous file as .bak, byte for byte", "[io][ProjectFile][save]")
{
    const SaveDirectory directory;
    const auto file = directory.file ("song.xml");
    const auto backup = directory.file ("song.xml.bak");

    // Not canonical, not even a project: the backup is the file that was
    // there, whatever it held.
    const juce::String previous = "<Project   schemaVersion='1'>\n\n  written by hand\n</Project>\n";
    writeText (file, previous);

    const auto project = smallProjectNamed ("First");
    REQUIRE (io::ProjectFile::save (project, file).wasSaved());

    CHECK (textOf (file) == canonical (project));
    CHECK (textOf (backup) == previous.toStdString());
    CHECK (directory.fileNames() == juce::StringArray { "song.xml", "song.xml.bak" });
}

TEST_CASE ("Each save replaces the backup with the file it saves over", "[io][ProjectFile][save]")
{
    const SaveDirectory directory;
    const auto file = directory.file ("song.xml");
    const auto backup = directory.file ("song.xml.bak");

    const auto first = smallProjectNamed ("First");
    const auto second = smallProjectNamed ("Second");
    const auto third = smallProjectNamed ("Third");

    REQUIRE (io::ProjectFile::save (first, file).wasSaved());
    REQUIRE (io::ProjectFile::save (second, file).wasSaved());

    CHECK (textOf (file) == canonical (second));
    CHECK (textOf (backup) == canonical (first));

    REQUIRE (io::ProjectFile::save (third, file).wasSaved());

    CHECK (textOf (file) == canonical (third));
    CHECK (textOf (backup) == canonical (second));
    CHECK (directory.fileNames() == juce::StringArray { "song.xml", "song.xml.bak" });
}

TEST_CASE ("The backup of a file without a suffix, or with two dots, is its whole name plus .bak", "[io][ProjectFile][save]")
{
    const SaveDirectory directory;

    for (const auto* name : { "song", "song.v2.xml" })
    {
        INFO (name);
        const auto file = directory.file (name);

        writeText (file, "previous");
        REQUIRE (io::ProjectFile::save (smallProjectNamed ("New"), file).wasSaved());

        CHECK (textOf (directory.file (juce::String (name) + ".bak")) == "previous");
    }

    CHECK (directory.fileNames() == juce::StringArray { "song", "song.bak", "song.v2.xml", "song.v2.xml.bak" });
}

TEST_CASE ("A read-only file is not saved over, and stays as it was", "[io][ProjectFile][save]")
{
    const SaveDirectory directory;
    const auto file = directory.file ("song.xml");

    const juce::String previous = "the previous file";
    writeText (file, previous);
    REQUIRE (file.setReadOnly (true));
    const auto modifiedBefore = file.getLastModificationTime();

    const auto result = io::ProjectFile::save (smallProjectNamed ("New"), file);

    CHECK_FALSE (result.wasSaved());
    CHECK (result.outcome == SaveOutcome::targetNotWritable);
    CHECK (result.message.contains (file.getFullPathName()));
    CHECK (result.message.contains ("read-only"));

    CHECK (textOf (file) == previous.toStdString());
    CHECK (file.getLastModificationTime() == modifiedBefore);
    CHECK_FALSE (file.hasWriteAccess());
    // Neither a backup nor a temporary file was left behind.
    CHECK (directory.fileNames() == juce::StringArray { "song.xml" });
}

TEST_CASE ("A save into a read-only directory fails, and the file there stays as it was", "[io][ProjectFile][save]")
{
    const SaveDirectory directory;
    const auto file = directory.file ("song.xml");

    const juce::String previous = "the previous file";
    writeText (file, previous);
    const auto modifiedBefore = file.getLastModificationTime();
    REQUIRE (directory.directory.setReadOnly (true));

    const auto result = io::ProjectFile::save (smallProjectNamed ("New"), file);

    CHECK_FALSE (result.wasSaved());
    CHECK (result.outcome == SaveOutcome::writeFailed);
    CHECK (result.message.contains (directory.directory.getFullPathName()));

    CHECK (textOf (file) == previous.toStdString());
    CHECK (file.getLastModificationTime() == modifiedBefore);
    CHECK (directory.fileNames() == juce::StringArray { "song.xml" });
}

TEST_CASE ("A save into a directory that is not there fails and creates nothing", "[io][ProjectFile][save]")
{
    const SaveDirectory directory;
    const auto file = directory.file ("not there").getChildFile ("song.xml");

    const auto result = io::ProjectFile::save (smallProjectNamed ("New"), file);

    CHECK (result.outcome == SaveOutcome::writeFailed);
    CHECK_FALSE (file.getParentDirectory().exists());
    CHECK (directory.fileNames().isEmpty());
}

TEST_CASE ("A directory is not saved over", "[io][ProjectFile][save]")
{
    const SaveDirectory directory;
    const auto target = directory.file ("song.xml");
    REQUIRE (target.createDirectory().wasOk());

    const auto result = io::ProjectFile::save (smallProjectNamed ("New"), target);

    CHECK (result.outcome == SaveOutcome::targetNotWritable);
    CHECK (result.message.contains ("is a directory"));
    CHECK (target.isDirectory());
    CHECK (directory.fileNames() == juce::StringArray { "song.xml" });
}

TEST_CASE ("When the previous file cannot be kept as .bak, nothing is saved", "[io][ProjectFile][save]")
{
    const SaveDirectory directory;
    const auto file = directory.file ("song.xml");
    const auto backup = directory.file ("song.xml.bak");

    const juce::String previous = "the previous file";
    writeText (file, previous);
    const auto modifiedBefore = file.getLastModificationTime();
    // Something a file cannot be renamed over stands where the backup goes.
    REQUIRE (backup.createDirectory().wasOk());
    writeText (backup.getChildFile ("kept"), "kept");

    const auto result = io::ProjectFile::save (smallProjectNamed ("New"), file);

    CHECK_FALSE (result.wasSaved());
    CHECK (result.outcome == SaveOutcome::backupFailed);
    CHECK (result.message.contains (backup.getFullPathName()));

    CHECK (textOf (file) == previous.toStdString());
    CHECK (file.getLastModificationTime() == modifiedBefore);
    CHECK (textOf (backup.getChildFile ("kept")) == "kept");
    CHECK (directory.fileNames() == juce::StringArray { "song.xml", "song.xml.bak" });
}

//------------------------------------------------------------------------------
// File names. A name that begins with "~" is reserved for files Hums handles
// itself (docs/SCHEMA.md -> Loading, saving, canonical form).

namespace
{

// The file of that name in the directory. Not SaveDirectory::file: its
// getChildFile takes a name that begins with "~" for a path of its own, which
// is what made such a file one that could be saved only once (finding 2 of
// the milestone 1 code review).
juce::File fileWithExactName (const SaveDirectory& directory, const juce::String& name)
{
    const juce::File file (directory.directory.getFullPathName() + juce::File::getSeparatorString() + name);

    REQUIRE (file.getFileName() == name);
    REQUIRE (file.getParentDirectory() == directory.directory);

    return file;
}

} // namespace

TEST_CASE ("A file name is allowed unless it begins with a tilde", "[io][ProjectFile][name]")
{
    const SaveDirectory directory;

    CHECK (io::ProjectFile::isNameAllowed (fileWithExactName (directory, "song.xml")));
    CHECK (io::ProjectFile::isNameAllowed (fileWithExactName (directory, "song")));
    CHECK (io::ProjectFile::isNameAllowed (fileWithExactName (directory, "so~ng.xml")));
    CHECK (io::ProjectFile::isNameAllowed (fileWithExactName (directory, "song.xml~")));
    CHECK (io::ProjectFile::isNameAllowed (fileWithExactName (directory, ".~song.xml")));

    CHECK_FALSE (io::ProjectFile::isNameAllowed (fileWithExactName (directory, "~song.xml")));
    CHECK_FALSE (io::ProjectFile::isNameAllowed (fileWithExactName (directory, "~song.xml.bak")));
    CHECK_FALSE (io::ProjectFile::isNameAllowed (fileWithExactName (directory, "~~song")));
    CHECK_FALSE (io::ProjectFile::isNameAllowed (fileWithExactName (directory, "~")));
}

TEST_CASE ("Only the file's own name counts, not a directory on the way to it", "[io][ProjectFile][name]")
{
    const SaveDirectory directory;
    const auto subdirectory = fileWithExactName (directory, "~songs");
    REQUIRE (subdirectory.createDirectory().wasOk());

    const juce::File file (subdirectory.getFullPathName() + juce::File::getSeparatorString() + "song.xml");
    REQUIRE (file.getParentDirectory() == subdirectory);

    CHECK (io::ProjectFile::isNameAllowed (file));

    const auto first = smallProjectNamed ("First");
    const auto second = smallProjectNamed ("Second");

    REQUIRE (io::ProjectFile::save (first, file).wasSaved());
    REQUIRE (io::ProjectFile::save (second, file).wasSaved());

    CHECK (textOf (file) == canonical (second));
    CHECK (textOf (juce::File (file.getFullPathName() + ".bak")) == canonical (first));

    OpenProject open;
    CHECK (io::ProjectFile::load (file, open.document, open.undoManager).wasLoaded());
}

TEST_CASE ("A project is not saved to a name that begins with a tilde, and nothing is created", "[io][ProjectFile][save][name]")
{
    const SaveDirectory directory;
    const auto file = fileWithExactName (directory, "~song.xml");

    const auto result = io::ProjectFile::save (smallProjectNamed ("New"), file);

    CHECK_FALSE (result.wasSaved());
    CHECK (result.outcome == SaveOutcome::nameNotAllowed);
    CHECK (result.message.contains ("~song.xml"));
    CHECK (directory.fileNames().isEmpty());
}

TEST_CASE ("A file whose name begins with a tilde is not saved over, and stays as it was", "[io][ProjectFile][save][name]")
{
    const SaveDirectory directory;
    const auto file = fileWithExactName (directory, "~song.xml");

    const auto previous = juce::String (canonical (smallProjectNamed ("Previous")));
    writeText (file, previous);
    const auto modifiedBefore = file.getLastModificationTime();

    const auto result = io::ProjectFile::save (smallProjectNamed ("New"), file);

    CHECK_FALSE (result.wasSaved());
    CHECK (result.outcome == SaveOutcome::nameNotAllowed);
    CHECK (result.message.contains ("~song.xml"));

    CHECK (textOf (file) == previous.toStdString());
    CHECK (file.getLastModificationTime() == modifiedBefore);
    // No backup and no temporary file, here or anywhere a "~" could lead.
    CHECK (directory.fileNames() == juce::StringArray { "~song.xml" });
    CHECK_FALSE (juce::File::getCurrentWorkingDirectory().getChildFile ("./~song.xml.bak").exists());
}

//------------------------------------------------------------------------------
// Saving: what is kept of the previous file, and symbolic links.

namespace
{

std::filesystem::path pathOf (const juce::File& file)
{
    return std::filesystem::path (file.getFullPathName().toStdString());
}

// A symbolic link that holds exactly this text as where it leads, which may
// be a path relative to the link's own directory.
void makeLink (const juce::File& link, const std::string& leadsTo)
{
    std::error_code error;
    std::filesystem::create_symlink (std::filesystem::path (leadsTo), pathOf (link), error);
    REQUIRE_FALSE (error);
}

bool isLink (const juce::File& file)
{
    return std::filesystem::is_symlink (pathOf (file));
}

std::filesystem::perms permissionsOf (const juce::File& file)
{
    return std::filesystem::status (pathOf (file)).permissions();
}

// How many entries a directory has, a link that leads nowhere included.
int numberOfEntriesIn (const juce::File& directory)
{
    int number = 0;

    for (const auto& entry : std::filesystem::directory_iterator (pathOf (directory)))
    {
        juce::ignoreUnused (entry);
        ++number;
    }

    return number;
}

} // namespace

TEST_CASE ("A save gives the new file the permission bits of the file it saves over", "[io][ProjectFile][save]")
{
    using std::filesystem::perms;

    const SaveDirectory directory;
    const auto project = smallProjectNamed ("New");

    // Closed to everyone but the owner; a new file is not.
    const auto closed = directory.file ("closed.xml");
    writeText (closed, "the previous file");
    std::filesystem::permissions (pathOf (closed), perms::owner_read | perms::owner_write);

    REQUIRE (io::ProjectFile::save (project, closed).wasSaved());
    REQUIRE (textOf (closed) == canonical (project));
    CHECK (permissionsOf (closed) == (perms::owner_read | perms::owner_write));

    // Open further than a new file is.
    const auto open = directory.file ("open.xml");
    writeText (open, "the previous file");
    const auto openToTheGroup = perms::owner_read | perms::owner_write | perms::group_read | perms::group_write | perms::owner_exec;
    std::filesystem::permissions (pathOf (open), openToTheGroup);

    REQUIRE (io::ProjectFile::save (project, open).wasSaved());
    REQUIRE (textOf (open) == canonical (project));
    CHECK (permissionsOf (open) == openToTheGroup);
}

TEST_CASE ("A save to a symbolic link saves the file the link leads to, and the link stays", "[io][ProjectFile][save][link]")
{
    const SaveDirectory directory;
    const auto elsewhere = directory.file ("elsewhere");
    REQUIRE (elsewhere.createDirectory().wasOk());

    const auto target = elsewhere.getChildFile ("song.xml");
    const juce::String previous = "the previous file";
    writeText (target, previous);

    const auto link = directory.file ("link.xml");
    makeLink (link, target.getFullPathName().toStdString());

    const auto project = smallProjectNamed ("New");
    const auto result = io::ProjectFile::save (project, link);

    CHECK (result.wasSaved());
    CHECK (result.message.isEmpty());

    CHECK (isLink (link));
    CHECK_FALSE (isLink (target));
    CHECK (textOf (target) == canonical (project));

    // The backup stands beside the file, not beside the link, and no
    // temporary file is left in either place.
    CHECK (directory.fileNames() == juce::StringArray { "elsewhere", "link.xml" });
    CHECK (numberOfEntriesIn (elsewhere) == 2);
    CHECK (textOf (elsewhere.getChildFile ("song.xml.bak")) == previous.toStdString());
}

TEST_CASE ("A save follows a link to a link, and a link that names its file relative to itself", "[io][ProjectFile][save][link]")
{
    const SaveDirectory directory;
    const auto elsewhere = directory.file ("elsewhere");
    REQUIRE (elsewhere.createDirectory().wasOk());

    const auto target = elsewhere.getChildFile ("song.xml");
    writeText (target, "the previous file");

    const auto inner = directory.file ("inner.xml");
    const auto outer = directory.file ("outer.xml");
    makeLink (inner, "elsewhere/song.xml");
    makeLink (outer, "inner.xml");

    const auto project = smallProjectNamed ("New");

    CHECK (io::ProjectFile::save (project, outer).wasSaved());

    CHECK (isLink (outer));
    CHECK (isLink (inner));
    CHECK (textOf (target) == canonical (project));
    CHECK (directory.fileNames() == juce::StringArray { "elsewhere", "inner.xml", "outer.xml" });
}

TEST_CASE ("A save to a link gives the new file the permission bits of the file the link leads to", "[io][ProjectFile][save][link]")
{
    using std::filesystem::perms;

    const SaveDirectory directory;
    const auto target = directory.file ("song.xml");
    writeText (target, "the previous file");
    std::filesystem::permissions (pathOf (target), perms::owner_read | perms::owner_write);

    const auto link = directory.file ("link.xml");
    makeLink (link, "song.xml");

    REQUIRE (io::ProjectFile::save (smallProjectNamed ("New"), link).wasSaved());

    CHECK (permissionsOf (target) == (perms::owner_read | perms::owner_write));
}

TEST_CASE ("A link that leads to no file is not saved to, and nothing is created", "[io][ProjectFile][save][link]")
{
    const SaveDirectory directory;
    const auto link = directory.file ("link.xml");
    makeLink (link, "gone.xml");

    const auto result = io::ProjectFile::save (smallProjectNamed ("New"), link);

    CHECK_FALSE (result.wasSaved());
    CHECK (result.outcome == SaveOutcome::targetNotWritable);
    CHECK (result.message.contains (link.getFullPathName()));
    CHECK (result.message.contains ("symbolic link"));

    CHECK (isLink (link));
    CHECK_FALSE (directory.file ("gone.xml").exists());
    CHECK (numberOfEntriesIn (directory.directory) == 1);
}

TEST_CASE ("Links that lead round in a circle are not saved to", "[io][ProjectFile][save][link]")
{
    const SaveDirectory directory;
    const auto one = directory.file ("one.xml");
    const auto other = directory.file ("other.xml");
    makeLink (one, "other.xml");
    makeLink (other, "one.xml");

    const auto result = io::ProjectFile::save (smallProjectNamed ("New"), one);

    CHECK_FALSE (result.wasSaved());
    CHECK (result.outcome == SaveOutcome::targetNotWritable);
    CHECK (result.message.contains (one.getFullPathName()));

    CHECK (isLink (one));
    CHECK (isLink (other));
    CHECK (numberOfEntriesIn (directory.directory) == 2);
}

TEST_CASE ("A link to a file whose name begins with a tilde is not saved to, and the file stays as it was", "[io][ProjectFile][save][link][name]")
{
    const SaveDirectory directory;
    const auto target = fileWithExactName (directory, "~song.xml");
    const juce::String previous = "the previous file";
    writeText (target, previous);
    const auto modifiedBefore = target.getLastModificationTime();

    const auto link = directory.file ("link.xml");
    makeLink (link, "~song.xml");

    const auto result = io::ProjectFile::save (smallProjectNamed ("New"), link);

    CHECK_FALSE (result.wasSaved());
    CHECK (result.outcome == SaveOutcome::nameNotAllowed);
    CHECK (result.message.contains ("~song.xml"));

    CHECK (isLink (link));
    CHECK (textOf (target) == previous.toStdString());
    CHECK (target.getLastModificationTime() == modifiedBefore);
    CHECK (directory.fileNames() == juce::StringArray { "link.xml", "~song.xml" });
}

TEST_CASE ("A link to a read-only file is not saved to, and the file stays as it was", "[io][ProjectFile][save][link]")
{
    const SaveDirectory directory;
    const auto target = directory.file ("song.xml");
    const juce::String previous = "the previous file";
    writeText (target, previous);
    REQUIRE (target.setReadOnly (true));
    const auto modifiedBefore = target.getLastModificationTime();

    const auto link = directory.file ("link.xml");
    makeLink (link, "song.xml");

    const auto result = io::ProjectFile::save (smallProjectNamed ("New"), link);

    CHECK_FALSE (result.wasSaved());
    CHECK (result.outcome == SaveOutcome::targetNotWritable);
    CHECK (result.message.contains ("read-only"));

    CHECK (isLink (link));
    CHECK (textOf (target) == previous.toStdString());
    CHECK (target.getLastModificationTime() == modifiedBefore);
    CHECK (directory.fileNames() == juce::StringArray { "link.xml", "song.xml" });
}

TEST_CASE ("A valid project in a file whose name begins with a tilde is not loaded", "[io][ProjectFile][load][name]")
{
    const SaveDirectory directory;
    const auto text = juce::String (canonical (smallProjectNamed ("In the file")));

    // The same text under a plain name loads, so it is the name that is
    // refused.
    {
        const auto plain = fileWithExactName (directory, "song.xml");
        writeText (plain, text);

        OpenProject open;
        REQUIRE (io::ProjectFile::load (plain, open.document, open.undoManager).wasLoaded());
    }

    const auto file = fileWithExactName (directory, "~song.xml");
    writeText (file, text);
    const auto modifiedBefore = file.getLastModificationTime();

    OpenProject open;
    CountingListener listener;
    open.document.addListener (&listener);

    const auto result = io::ProjectFile::load (file, open.document, open.undoManager);

    CHECK_FALSE (result.wasLoaded());
    CHECK (result.outcome == Outcome::nameNotAllowed);
    CHECK (result.message.contains ("~song.xml"));
    CHECK (result.violations.empty());

    CHECK (open.isUntouched());
    CHECK (listener.redirections == 0);
    CHECK (listener.otherCallbacks == 0);
    CHECK (textOf (file) == text.toStdString());
    CHECK (file.getLastModificationTime() == modifiedBefore);
}

TEST_CASE ("A name that begins with a tilde is refused whether or not the file is there", "[io][ProjectFile][load][name]")
{
    const SaveDirectory directory;
    OpenProject open;

    const auto result = io::ProjectFile::load (fileWithExactName (directory, "~song.xml"), open.document, open.undoManager);

    CHECK (result.outcome == Outcome::nameNotAllowed);
    CHECK (open.isUntouched());
}

//==============================================================================
// The bytes of a project file are UTF-8.

namespace
{

// The small project as a file with exactly these bytes for the Project's name.
juce::File fileWithNameBytes (const SaveDirectory& directory, const std::string& nameBytes)
{
    std::string bytes = smallProject (R"(schemaVersion="1" name="@")").toStdString();
    bytes.replace (bytes.find ('@'), 1, nameBytes);

    const auto file = directory.file ("song.xml");
    REQUIRE (file.replaceWithData (bytes.data(), bytes.size()));

    return file;
}

} // namespace

TEST_CASE ("A file with a name in UTF-8 loads with that name", "[io][ProjectFile][load]")
{
    const SaveDirectory directory;
    OpenProject open;

    // "Bär", the ä as the two bytes of UTF-8.
    const auto result = io::ProjectFile::load (fileWithNameBytes (directory, "B\xC3\xA4r"), open.document, open.undoManager);

    CHECK (result.wasLoaded());
    CHECK (open.document[ids::name].toString() == juce::String::fromUTF8 ("B\xC3\xA4r"));
}

// JUCE would read the byte as Latin-1, and the next save would write two
// bytes where the file had one.
TEST_CASE ("A file with bytes that are no UTF-8 is refused, not read as Latin-1", "[io][ProjectFile][load]")
{
    const SaveDirectory directory;
    OpenProject open;

    // "Bär", the ä as the one byte of Latin-1.
    const auto result = io::ProjectFile::load (fileWithNameBytes (directory, "B\xE4r"), open.document, open.undoManager);

    CHECK (result.outcome == Outcome::malformedXml);
    CHECK (result.message.contains ("UTF-8"));
    CHECK (open.isUntouched());
}

// A juce::String ends at a zero byte: what follows one would not be seen.
TEST_CASE ("A file with a zero byte is refused, wherever the byte stands", "[io][ProjectFile][load]")
{
    const SaveDirectory directory;
    const std::string zeroByte (1, '\0');

    SECTION ("inside the document")
    {
        OpenProject open;
        const auto result = io::ProjectFile::load (fileWithNameBytes (directory, "B" + zeroByte + "r"), open.document, open.undoManager);

        CHECK (result.outcome == Outcome::malformedXml);
        CHECK (open.isUntouched());
    }

    SECTION ("after the root node, with more text behind it")
    {
        OpenProject open;
        const auto file = fileWithNameBytes (directory, "Berlin");
        REQUIRE (file.appendData (zeroByte.data(), 1));
        REQUIRE (file.appendText ("and some more text"));

        const auto result = io::ProjectFile::load (file, open.document, open.undoManager);

        CHECK (result.outcome == Outcome::malformedXml);
        CHECK (open.isUntouched());
    }
}

TEST_CASE ("A file in UTF-16 is refused", "[io][ProjectFile][load]")
{
    const SaveDirectory directory;
    OpenProject open;
    const auto file = directory.file ("song.xml");
    REQUIRE (file.replaceWithText (smallProject (R"(schemaVersion="1")"), true, true, "\n"));

    const auto result = io::ProjectFile::load (file, open.document, open.undoManager);

    CHECK (result.outcome == Outcome::malformedXml);
    CHECK (open.isUntouched());
}

//==============================================================================
// Guardrail 5: loading is a wall (docs/GUARDRAILS.md -> 5). Every file under
// fixtures/ goes through the real load path. A valid one installs; every
// other one is refused, and the open project and the file stay as they were.
//
// The test names begin with "guardrail" so that `ctest -R guardrail` runs
// them with the other guardrails.

namespace
{

// The fixtures that load, relative to the fixtures directory.
const std::vector<const char*> validFixtures {
    "all_node_types.xml",
    "berlin_test.xml",
    "berlin_test_canonical.xml",
};

// A fixture that is refused, and how. For a file the Validator refuses, the
// one node and property it is refused for; an empty property stands for the
// node as a whole.
struct RefusedFixture
{
    const char* fileName;
    Outcome outcome;
    const char* nodePath = "";
    const char* property = "";
};

const std::vector<RefusedFixture> refusedFixtures {
    { "refused/malformed_empty.xml", Outcome::malformedXml },
    { "refused/malformed_not_xml.xml", Outcome::malformedXml },
    { "refused/malformed_unclosed_tag.xml", Outcome::malformedXml },
    { "refused/malformed_unknown_entity.xml", Outcome::malformedXml },
    { "refused/malformed_attribute_twice.xml", Outcome::malformedXml },
    { "refused/malformed_text_after_root.xml", Outcome::malformedXml },
    { "refused/malformed_second_root.xml", Outcome::malformedXml },
    { "refused/malformed_closing_tag_of_another_name.xml", Outcome::malformedXml },
    { "refused/malformed_last_closing_tag_cut_short.xml", Outcome::malformedXml },
    { "refused/malformed_not_utf8.xml", Outcome::malformedXml },
    { "refused/doctype.xml", Outcome::notAProjectFile },
    { "refused/attribute_plain_and_base64.xml", Outcome::notAProjectFile },
    { "refused/not_a_project_root.xml", Outcome::notAProjectFile },
    { "refused/text_between_nodes.xml", Outcome::notAProjectFile },
    { "refused/schema_version_absent.xml", Outcome::noSchemaVersion, "Project", "schemaVersion" },
    { "refused/schema_version_not_a_number.xml", Outcome::noSchemaVersion, "Project", "schemaVersion" },
    { "refused/schema_version_too_new.xml", Outcome::versionTooNew, "Project", "schemaVersion" },
    // No step exists at version 1, so the test hands in the one that throws.
    { "refused/migration_throws.xml", Outcome::migrationFailed },
    { "refused/invariant_01_slot_references_no_clip.xml", Outcome::invalidDocument, "Project/Tracks/Track[0]/Slots/Slot[0]", "clipId" },
    { "refused/invariant_02_lane_with_seven_cells.xml", Outcome::invalidDocument, "Project/ClipPool/Clip[0]/Grid/Lane[0]", "" },
    { "refused/invariant_03_overlapping_placements.xml", Outcome::invalidDocument, "Project/Tracks/Track[0]/Placements/Placement[1]", "start" },
    { "refused/invariant_04_offset_at_loop_length.xml", Outcome::invalidDocument, "Project/Tracks/Track[0]/Placements/Placement[1]", "offset" },
    { "refused/invariant_05_clip_of_another_kind.xml", Outcome::invalidDocument, "Project/ClipPool/Clip[2]", "kind" },
    { "refused/invariant_06_no_tempo_at_tick_0.xml", Outcome::invalidDocument, "Project/Timeline", "" },
    { "refused/invariant_07_ppq_480.xml", Outcome::invalidDocument, "Project", "ppq" },
    { "refused/invariant_08_note_ends_past_the_largest_tick.xml", Outcome::invalidDocument, "Project/ClipPool/Clip[2]/Events/Note[0]", "length" },
    { "refused/invariant_08_note_without_length.xml", Outcome::invalidDocument, "Project/ClipPool/Clip[2]/Events/Note[0]", "length" },
    { "refused/invariant_08_placement_ends_past_the_largest_tick.xml", Outcome::invalidDocument, "Project/Tracks/Track[0]/Placements/Placement[1]", "length" },
    { "refused/invariant_09_lane_clip_in_a_slot.xml", Outcome::invalidDocument, "Project/Tracks/Track[0]/Slots/Slot[0]", "clipId" },
    { "refused/invariant_09_note_in_automation_clip.xml", Outcome::invalidDocument, "Project/ClipPool/Clip[1]/Events/Note[0]", "" },
    { "refused/invariant_10_bool_spelled_yes.xml", Outcome::invalidDocument, "Project/UiState", "loopEnabled" },
    { "refused/invariant_10_bpm_with_seven_digits.xml", Outcome::invalidDocument, "Project/Timeline/TempoEvent[0]", "bpm" },
    { "refused/invariant_10_bpm_with_a_huge_exponent.xml", Outcome::invalidDocument, "Project/Timeline/TempoEvent[0]", "bpm" },
    { "refused/invariant_10_player_with_an_empty_name.xml", Outcome::invalidDocument, "Project/Routing/Player[0]", "name" },
    { "refused/invariant_10_pitch_128.xml", Outcome::invalidDocument, "Project/ClipPool/Clip[2]/Events/Note[0]", "pitch" },
    { "refused/invariant_10_pitch_bend_with_a_b.xml", Outcome::invalidDocument, "Project/ClipPool/Clip[2]/Events/Event[2]", "b" },
    { "refused/invariant_11_two_pads_with_one_index.xml", Outcome::invalidDocument, "Project/Routing/PadMap/Pad[1]", "index" },
    { "refused/invariant_11_two_players_with_one_name.xml", Outcome::invalidDocument, "Project/Routing/Player[1]", "name" },
    { "refused/invariant_12_no_session.xml", Outcome::invalidDocument, "Project", "" },
    { "refused/invariant_12_step_clip_with_events.xml", Outcome::invalidDocument, "Project/ClipPool/Clip[0]", "" },
    { "refused/invariant_12_event_clip_with_a_grid.xml", Outcome::invalidDocument, "Project/ClipPool/Clip[2]", "" },
};

juce::File fixtureFile (const char* fileName)
{
    return juce::File (HUMS_IO_FIXTURES_DIRECTORY).getChildFile (fileName);
}

// A file as it is on disk at one moment: its bytes and when it was last
// written.
struct FileState
{
    explicit FileState (const juce::File& file)
        : modified (file.getLastModificationTime())
    {
        REQUIRE (file.loadFileAsData (bytes));
    }

    bool operator== (const FileState& other) const
    {
        return bytes == other.bytes && modified == other.modified;
    }

    juce::MemoryBlock bytes;
    juce::Time modified;
};

// The steps a file meets whose migration fails.
io::Migrations::Steps stepsWithOneThatThrows()
{
    return { [] (juce::ValueTree&) { throw std::runtime_error ("this step cannot migrate the file"); } };
}

std::string sortedLines (juce::StringArray names)
{
    names.sort (false);

    return names.joinIntoString ("\n").toStdString();
}

} // namespace

TEST_CASE ("guardrail 5: every file under fixtures/ is listed here as valid or as refused", "[io][ProjectFile][guardrail]")
{
    // So that a fixture cannot be added and never loaded: the tests below
    // walk the two lists, and this one holds the lists to the directory.
    const juce::File directory (HUMS_IO_FIXTURES_DIRECTORY);

    juce::StringArray onDisk;

    for (const auto& file : directory.findChildFiles (juce::File::findFiles | juce::File::ignoreHiddenFiles, true))
        onDisk.add (file.getRelativePathFrom (directory));

    juce::StringArray listed;

    for (const auto* fileName : validFixtures)
        listed.add (fileName);

    for (const auto& fixture : refusedFixtures)
        listed.add (fixture.fileName);

    CHECK (sortedLines (onDisk) == sortedLines (listed));
}

TEST_CASE ("guardrail 5: every valid fixture installs and is left as it was on disk", "[io][ProjectFile][guardrail]")
{
    for (const auto* fileName : validFixtures)
    {
        INFO (fileName);
        OpenProject open;
        const auto file = fixtureFile (fileName);
        const FileState before (file);

        const auto result = io::ProjectFile::load (file, open.document, open.undoManager);

        CHECK (result.wasLoaded());
        CHECK (result.message.isEmpty());
        CHECK (open.document != open.treeBefore);
        CHECK (model::Validator::validate (open.document).empty());
        CHECK_FALSE (open.undoManager.canUndo());
        CHECK (FileState (file) == before);
    }
}

TEST_CASE ("guardrail 5: every other fixture is refused, and the open project and the file stay as they were", "[io][ProjectFile][guardrail]")
{
    for (const auto& fixture : refusedFixtures)
    {
        INFO (fixture.fileName);
        OpenProject open;
        CountingListener listener;
        open.document.addListener (&listener);

        const auto file = fixtureFile (fixture.fileName);
        const FileState before (file);
        const auto steps = fixture.outcome == Outcome::migrationFailed ? stepsWithOneThatThrows() : io::Migrations::steps();

        const auto result = io::ProjectFile::load (file, open.document, open.undoManager, steps);

        CHECK_FALSE (result.wasLoaded());
        CHECK (result.outcome == fixture.outcome);
        CHECK (result.message.isNotEmpty());

        // Refused for the one thing the fixture is there for, and for
        // nothing else.
        const juce::String expectedNodePath (fixture.nodePath);

        if (expectedNodePath.isEmpty())
        {
            CHECK (result.violations.empty());
        }
        else
        {
            REQUIRE (result.violations.size() == 1);
            CHECK (result.violations[0].nodePath.toStdString() == expectedNodePath.toStdString());
            CHECK (result.violations[0].property.toStdString() == std::string (fixture.property));
            CHECK (result.message.contains (expectedNodePath));
        }

        CHECK (open.isUntouched());
        CHECK (listener.redirections == 0);
        CHECK (listener.otherCallbacks == 0);
        CHECK (FileState (file) == before);

        open.document.removeListener (&listener);
    }
}

TEST_CASE ("guardrail 5: the fixture whose migration throws is refused for that alone", "[io][ProjectFile][guardrail]")
{
    // With the steps of this build, which are none, it is a valid document.
    // When the first real step lands, a file of the old version that the step
    // cannot migrate takes this fixture's place.
    OpenProject open;

    CHECK (io::ProjectFile::load (fixtureFile ("refused/migration_throws.xml"), open.document, open.undoManager).wasLoaded());
}

TEST_CASE ("guardrail 5: a save that fails leaves the previous file as it was", "[io][ProjectFile][guardrail]")
{
    // A project file that was loaded, worked on, and cannot be saved over,
    // because it is read-only.
    const SaveDirectory directory;
    const auto file = directory.file ("song.xml");
    REQUIRE (fixtureFile ("all_node_types.xml").copyFileTo (file));

    juce::ValueTree document;
    juce::UndoManager undoManager;
    REQUIRE (io::ProjectFile::load (file, document, undoManager).wasLoaded());
    const auto loadedText = canonical (document);

    document.setProperty (ids::name, "Edited since it was loaded", &undoManager);

    REQUIRE (file.setReadOnly (true));
    const FileState before (file);

    const auto result = io::ProjectFile::save (document, file);

    CHECK_FALSE (result.wasSaved());
    CHECK (result.message.isNotEmpty());
    CHECK (FileState (file) == before);
    CHECK (directory.fileNames() == juce::StringArray { "song.xml" });

    // And it is still the project that was loaded, not the edited one.
    juce::ValueTree reopened;
    juce::UndoManager otherUndoManager;
    REQUIRE (io::ProjectFile::load (file, reopened, otherUndoManager).wasLoaded());
    CHECK (canonical (reopened) == loadedText);
}
