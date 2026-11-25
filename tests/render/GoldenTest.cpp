// Guardrail 4: the render golden files (docs/GUARDRAILS.md -> 4. Render golden
// files). Every rendering rule of docs/RENDERING.md has a pair in
// tests/render/golden/:
//
//   <case>.xml   a project, loaded through the real load path
//   <case>.txt   the events it must render to, reasoned out by hand
//
// The expected file begins with a header: lines that begin with "#", which
// say what the case shows and how its events follow from the rules, and at
// most one line "range <start> <end>", the ticks that are rendered. Without
// that line the case is rendered from tick 0 to the arrangement end. Every
// line after the header is one event, in the order in which it goes out:
//
//   tick track channel type a b [data]
//
// track is the position of the Track among the tracks, from 0; type is
// noteOn, noteOff or the kind of an Event as the document names it; a and b
// are what the type makes of them (pitch and velocity for a note), 0 where
// it uses none; data is the payload of a sysex event in hexadecimal digits,
// without F0 and F7.
//
// The case is rendered as an SMF export would render it: Kernel + NoteTracker
// through render::Renderer, the destinations those without hardware, and at
// the end of the range every note that still sounds is ended. Every track is
// heard.
//
// With HUMS_REGENERATE_GOLDEN=1 in the environment (the regenerate_golden
// target) nothing is compared: the event lines of every expected file are
// written anew from what the renderer gives, under the header the file has.
// The diff is read before it is committed.

#include <catch2/catch_test_macros.hpp>

#include "io/ProjectFile.h"
#include "model/ProjectModel.h"
#include "render/Kernel.h"
#include "render/NoteTracker.h"
#include "render/Renderer.h"
#include "render/SnapshotBuilder.h"
#include "render/snapshot/Snapshot.h"

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{

juce::File goldenFile (const juce::String& fileName)
{
    return juce::File (HUMS_RENDER_GOLDEN_DIRECTORY).getChildFile (fileName);
}

bool isRegenerating()
{
    return juce::SystemStats::getEnvironmentVariable ("HUMS_REGENERATE_GOLDEN", "") == "1";
}

// An expected file, taken apart.
struct Expected
{
    // The lines before the first event, as the file has them.
    juce::StringArray headerLines;
    // The ticks to render, when the header says them.
    std::optional<std::int64_t> rangeStart;
    std::optional<std::int64_t> rangeEnd;
    juce::StringArray eventLines;
};

// A tick as a header line may write it: digits only.
std::optional<std::int64_t> tickOf (const juce::String& text)
{
    if (text.isEmpty() || text.length() > 18 || ! text.containsOnly ("0123456789"))
        return std::nullopt;

    return text.getLargeIntValue();
}

Expected readExpected (const juce::File& file)
{
    REQUIRE (file.existsAsFile());

    juce::StringArray lines;
    lines.addLines (file.loadFileAsString());

    while (lines.size() > 0 && lines[lines.size() - 1].isEmpty())
        lines.remove (lines.size() - 1);

    Expected expected;
    int lineIndex = 0;

    for (; lineIndex < lines.size(); ++lineIndex)
    {
        const juce::String& line = lines[lineIndex];

        if (line.startsWith ("range "))
        {
            INFO ("header line: " << line);
            REQUIRE_FALSE (expected.rangeStart.has_value());

            const auto words = juce::StringArray::fromTokens (line, " ", "");
            REQUIRE (words.size() == 3);

            expected.rangeStart = tickOf (words[1]);
            expected.rangeEnd = tickOf (words[2]);
            REQUIRE (expected.rangeStart.has_value());
            REQUIRE (expected.rangeEnd.has_value());
            REQUIRE (*expected.rangeStart <= *expected.rangeEnd);
        }
        else if (! line.startsWith ("#"))
        {
            break;
        }

        expected.headerLines.add (line);
    }

    for (; lineIndex < lines.size(); ++lineIndex)
        expected.eventLines.add (lines[lineIndex]);

    return expected;
}

juce::String typeOf (render::Renderer::EventKind kind)
{
    switch (kind)
    {
        case render::Renderer::EventKind::noteOff:
            return "noteOff";
        case render::Renderer::EventKind::noteOn:
            return "noteOn";
        case render::Renderer::EventKind::cc:
            return "cc";
        case render::Renderer::EventKind::pitchBend:
            return "pitchBend";
        case render::Renderer::EventKind::programChange:
            return "programChange";
        case render::Renderer::EventKind::channelPressure:
            return "channelPressure";
        case render::Renderer::EventKind::polyPressure:
            return "polyPressure";
        case render::Renderer::EventKind::sysex:
            return "sysex";
    }

    return "unknown";
}

juce::String lineOf (const render::Renderer::Event& event, const render::Snapshot& snapshot)
{
    juce::String line;
    line << juce::String (event.tick) << " " << juce::String (static_cast<juce::int64> (event.track)) << " " << event.channel
         << " " << typeOf (event.kind) << " " << event.a << " " << event.b;

    if (event.kind == render::Renderer::EventKind::sysex)
    {
        const auto& bytes = snapshot.clipTemplates[event.templateIndex].sysexBytes;

        line << " ";

        for (std::size_t index = event.dataStart; index < event.dataStart + event.dataLength; ++index)
            line << juce::String::toHexString (bytes[index]).paddedLeft ('0', 2);
    }

    return line;
}

// What the project renders to over [start, end), one line per event, ended
// as an export ends: with the off of every note that still sounds.
juce::StringArray renderedLines (const model::ProjectModel& project, std::int64_t start, std::int64_t end)
{
    const render::Snapshot snapshot = render::SnapshotBuilder::build (project);
    const auto destinations = render::SnapshotBuilder::destinationsWithoutHardware (project);
    const std::vector<bool> allTracks (snapshot.trackPlans.size(), true);

    render::NoteTracker tracker (destinations.count);
    std::array<render::Renderer::Event, 256> buffer;
    juce::StringArray lines;

    render::Kernel::Position position { start, 0 };

    for (bool isFinished = false; ! isFinished;)
    {
        const auto progress = render::Renderer::render (snapshot, allTracks, destinations.destinationOfTrack, tracker, position, end, buffer);

        for (std::size_t index = 0; index < progress.eventCount; ++index)
            lines.add (lineOf (buffer[index], snapshot));

        position = progress.next;
        isFinished = progress.isFinished;
    }

    do
    {
        const std::size_t offCount = render::Renderer::stop (tracker, end, buffer);

        for (std::size_t index = 0; index < offCount; ++index)
            lines.add (lineOf (buffer[index], snapshot));
    } while (tracker.soundingCount() > 0);

    return lines;
}

// Renders the fixture of a case and holds the result to its expected file,
// or writes it there when the golden files are being regenerated.
void checkGoldenCase (const juce::String& caseName)
{
    juce::ValueTree document;
    juce::UndoManager undoManager;

    const auto loadResult = io::ProjectFile::load (goldenFile (caseName + ".xml"), document, undoManager);
    INFO ("loading " << caseName << ".xml: " << loadResult.message);
    REQUIRE (loadResult.wasLoaded());

    const juce::File expectedFile = goldenFile (caseName + ".txt");
    const Expected expected = readExpected (expectedFile);

    const model::ProjectModel project (document);
    const auto arrangementEnd = project.getTracks().getArrangementEnd();

    // A case without a placement has nothing to render to its end; it must
    // say its range.
    REQUIRE ((expected.rangeEnd.has_value() || arrangementEnd.has_value()));

    const juce::StringArray rendered = renderedLines (project, expected.rangeStart.value_or (0), expected.rangeEnd.value_or (arrangementEnd.value_or (0)));

    if (isRegenerating())
    {
        juce::StringArray lines = expected.headerLines;
        lines.addArray (rendered);

        REQUIRE (expectedFile.replaceWithText (lines.joinIntoString ("\n") + "\n", false, false, "\n"));
        WARN ("regenerated " << caseName << ".txt: read the diff before committing it");
        return;
    }

    INFO ("rendered:\n"
          << rendered.joinIntoString ("\n"));
    INFO ("expected (" << caseName << ".txt):\n"
                       << expected.eventLines.joinIntoString ("\n"));
    CHECK (rendered == expected.eventLines);
}

} // namespace

TEST_CASE ("golden: placement offset", "[render][golden]")
{
    checkGoldenCase ("placement_offset");
}

TEST_CASE ("golden: placement shorter than the clip loop", "[render][golden]")
{
    checkGoldenCase ("placement_shorter_than_loop");
}

TEST_CASE ("golden: placement longer than the clip loop", "[render][golden]")
{
    checkGoldenCase ("placement_longer_than_loop");
}

TEST_CASE ("golden: event-clip note crossing loopLength", "[render][golden]")
{
    checkGoldenCase ("note_crossing_loop_length");
}

TEST_CASE ("golden: content past loopLength silent", "[render][golden]")
{
    checkGoldenCase ("content_past_loop_length");
}

TEST_CASE ("golden: export range cutting a note", "[render][golden]")
{
    checkGoldenCase ("export_range_cutting_note");
}

TEST_CASE ("golden: a track with no Player rendering normally", "[render][golden]")
{
    checkGoldenCase ("track_without_player");
}

TEST_CASE ("golden: bank select, program change and note at one tick", "[render][golden]")
{
    checkGoldenCase ("bank_select_program_change_note");
}

TEST_CASE ("golden: note-off and note-on of one pitch at one tick", "[render][golden]")
{
    checkGoldenCase ("note_off_and_note_on_same_pitch_same_tick");
}

TEST_CASE ("golden: same-pitch overlap on one track", "[render][golden]")
{
    checkGoldenCase ("same_pitch_overlap_one_track");
}

TEST_CASE ("golden: same-pitch overlap across two tracks on one destination", "[render][golden]")
{
    checkGoldenCase ("same_pitch_overlap_two_tracks");
}

TEST_CASE ("golden: two placements of one clip, the second restarting", "[render][golden]")
{
    checkGoldenCase ("placements_restarting");
}

TEST_CASE ("golden: two placements of one clip, phase-continuous (merged)", "[render][golden]")
{
    checkGoldenCase ("placements_phase_continuous");
}

TEST_CASE ("golden: split during a held note", "[render][golden]")
{
    checkGoldenCase ("split_during_held_note");
}

TEST_CASE ("golden: quantise at 1/8", "[render][golden]")
{
    checkGoldenCase ("quantise_eighth");
}

TEST_CASE ("golden: quantise at 1/8T", "[render][golden]")
{
    checkGoldenCase ("quantise_eighth_triplet");
}

TEST_CASE ("golden: quantise moving an onset across the loop point, in a loop that is no multiple of the grid", "[render][golden]")
{
    checkGoldenCase ("quantise_wrap_across_loop_point");
}

TEST_CASE ("golden: two notes of one pitch quantised onto one tick", "[render][golden]")
{
    checkGoldenCase ("quantise_two_notes_one_pitch_one_tick");
}

TEST_CASE ("golden: skipped column", "[render][golden]")
{
    checkGoldenCase ("step_skipped_column");
}

TEST_CASE ("golden: resetAt shorter than stepCount", "[render][golden]")
{
    checkGoldenCase ("step_reset_at_shorter_than_step_count");
}

TEST_CASE ("golden: fixedPitch lane ignoring cell pitch", "[render][golden]")
{
    checkGoldenCase ("step_fixed_pitch_ignoring_cell_pitch");
}

TEST_CASE ("golden: off-cell CC values", "[render][golden]")
{
    checkGoldenCase ("step_off_cell_cc_values");
}

TEST_CASE ("golden: gate > 1.0 into a different pitch", "[render][golden]")
{
    checkGoldenCase ("step_gate_over_one_into_different_pitch");
}

TEST_CASE ("golden: gate reaching into a skipped column", "[render][golden]")
{
    checkGoldenCase ("step_gate_reaching_into_skipped_column");
}

TEST_CASE ("golden: gate > 1.0 into the same pitch (merge)", "[render][golden]")
{
    checkGoldenCase ("step_gate_over_one_into_same_pitch");
}

TEST_CASE ("golden: gate across the loop point into a different pitch", "[render][golden]")
{
    checkGoldenCase ("step_gate_across_loop_point_into_different_pitch");
}

TEST_CASE ("golden: gate across the loop point into the same pitch (retrigger, not merge)", "[render][golden]")
{
    checkGoldenCase ("step_gate_across_loop_point_into_same_pitch");
}
