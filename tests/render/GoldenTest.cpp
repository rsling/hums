// Guardrail 4: the render golden files (docs/GUARDRAILS.md -> 4. Render golden
// files). Every rendering rule of docs/RENDERING.md has a pair in
// tests/render/golden/:
//
//   <case>.xml   a project, loaded through the real load path
//   <case>.txt   the events it must render to, reasoned out by hand
//
// The expected file begins with a header: lines that begin with "#", which
// say what the case shows and how its events follow from the rules, and at
// most one of each of these lines, which say how the case is played:
//
//   range <start> <end>   the ticks that are rendered; without it, tick 0 to
//                         the arrangement end
//   seek <tick>           playback begins at that tick by a seek: the
//                         controllers are chased for it, and the case is
//                         rendered from there to the end of the range. A
//                         range line beside it begins at that tick
//   loop <start> <end>    the transport loop. The case is rendered from its
//                         start to the loop's end; there playback wraps, once
//                         (the offs of the notes that sound, the controllers
//                         chased for the loop's start), and goes on from the
//                         loop's start to the end of the range, which lies
//                         inside the loop or on its end
//
// Every line after the header is one event, in the order in which it goes
// out:
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
// heard. A seek and a loop are what an export does not have: there the case
// is played as the engine plays, through Renderer::seek.
//
// Every case is the list of this file, goldenCases: the required cases of the
// contract. The directory is held to it, so that a case cannot lose its files
// and a file cannot lie there unrendered. Beyond its expected file each case
// is held to guardrails::RenderChecks (docs/GUARDRAILS.md -> 4, Beyond the
// golden files).
//
// With HUMS_REGENERATE_GOLDEN=1 in the environment (the regenerate_golden
// target) nothing is compared: the event lines of every expected file are
// written anew from what the renderer gives, under the header the file has.
// The diff is read before it is committed.

#include <catch2/catch_test_macros.hpp>

#include "guardrails/RenderChecks.h"
#include "guardrails/RenderRun.h"
#include "io/ProjectFile.h"
#include "model/ProjectModel.h"
#include "model/Validator.h"
#include "render/Renderer.h"
#include "render/SnapshotBuilder.h"
#include "render/snapshot/Snapshot.h"

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

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
    // Where playback begins by a seek, when the header says so.
    std::optional<std::int64_t> seekTick;
    // The transport loop, when the header says one.
    std::optional<std::int64_t> loopStart;
    std::optional<std::int64_t> loopEnd;
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
        else if (line.startsWith ("seek "))
        {
            INFO ("header line: " << line);
            REQUIRE_FALSE (expected.seekTick.has_value());

            const auto words = juce::StringArray::fromTokens (line, " ", "");
            REQUIRE (words.size() == 2);

            expected.seekTick = tickOf (words[1]);
            REQUIRE (expected.seekTick.has_value());
        }
        else if (line.startsWith ("loop "))
        {
            INFO ("header line: " << line);
            REQUIRE_FALSE (expected.loopStart.has_value());

            const auto words = juce::StringArray::fromTokens (line, " ", "");
            REQUIRE (words.size() == 3);

            expected.loopStart = tickOf (words[1]);
            expected.loopEnd = tickOf (words[2]);
            REQUIRE (expected.loopStart.has_value());
            REQUIRE (expected.loopEnd.has_value());
            REQUIRE (*expected.loopStart < *expected.loopEnd);
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

// The events of a run, one line per event.
juce::StringArray linesOf (const guardrails::RenderRun::Result& result, const render::Snapshot& snapshot)
{
    juce::StringArray lines;

    for (const auto& event : result.events)
        lines.add (lineOf (event, snapshot));

    return lines;
}

// How the header of an expected file says its case is played.
// `arrangementEnd` is where a case without a range line ends.
guardrails::RenderRun::Plan planOf (const Expected& expected, std::int64_t arrangementEnd)
{
    guardrails::RenderRun::Plan plan;
    plan.start = expected.seekTick.value_or (expected.rangeStart.value_or (0));
    plan.end = expected.rangeEnd.value_or (arrangementEnd);
    plan.beginsWithSeek = expected.seekTick.has_value();
    plan.loopStart = expected.loopStart;
    plan.loopEnd = expected.loopEnd;

    // A range line beside a seek line must not say another start.
    if (expected.seekTick.has_value() && expected.rangeStart.has_value())
        REQUIRE (*expected.rangeStart == *expected.seekTick);

    REQUIRE (plan.start <= plan.end);

    if (expected.loopStart.has_value())
    {
        // One wrap: up to the loop's end, back to its start, and on to the
        // end of the range.
        REQUIRE (plan.start < *expected.loopEnd);
        REQUIRE (*expected.loopStart <= plan.end);
        REQUIRE (plan.end <= *expected.loopEnd);
    }

    return plan;
}

// Renders the fixture of a case and holds the result to its expected file,
// or writes it there when the golden files are being regenerated. Then the
// case is held to the render checks: the same events in small windows, no
// allocation, every note-on with its note-off.
void checkGoldenCase (const juce::String& caseName)
{
    juce::ValueTree document;
    juce::UndoManager undoManager;

    const auto loadResult = io::ProjectFile::load (goldenFile (caseName + ".xml"), document, undoManager);
    INFO ("loading " << caseName << ".xml: " << loadResult.message);
    REQUIRE (loadResult.wasLoaded());

    // The load has run the Validator; said here once more, because the
    // contract says it of every fixture by name.
    CHECK (model::Validator::validate (document).empty());

    const juce::File expectedFile = goldenFile (caseName + ".txt");
    const Expected expected = readExpected (expectedFile);

    const model::ProjectModel project (document);
    const auto arrangementEnd = project.getTracks().getArrangementEnd();

    // A case without a placement has nothing to render to its end; it must
    // say its range.
    REQUIRE ((expected.rangeEnd.has_value() || arrangementEnd.has_value()));

    // Rendered as an export renders: the destinations are those without
    // hardware.
    const auto snapshot = render::SnapshotBuilder::build (project);
    const auto destinations = render::SnapshotBuilder::destinationsWithoutHardware (project);
    const guardrails::RenderRun run (snapshot, destinations.destinationOfTrack, destinations.count);
    const auto plan = planOf (expected, arrangementEnd.value_or (0));

    const juce::StringArray rendered = linesOf (run.play (plan, guardrails::RenderRun::oneWindow, 256), snapshot);

    if (isRegenerating())
    {
        juce::StringArray lines = expected.headerLines;
        lines.addArray (rendered);

        REQUIRE (expectedFile.replaceWithText (lines.joinIntoString ("\n") + "\n", false, false, "\n"));
        WARN ("regenerated " << caseName << ".txt: read the diff before committing it");
        return;
    }

    {
        INFO ("rendered:\n"
              << rendered.joinIntoString ("\n"));
        INFO ("expected (" << caseName << ".txt):\n"
                           << expected.eventLines.joinIntoString ("\n"));
        CHECK (rendered == expected.eventLines);
    }

    juce::StringArray checkFailures;

    for (const auto& failure : guardrails::RenderChecks::failuresOf (run, plan, guardrails::RenderChecks::TickByTick::yes))
        checkFailures.add (failure);

    INFO ("render checks:\n"
          << checkFailures.joinIntoString ("\n"));
    CHECK (checkFailures.isEmpty());
}

// The required cases of docs/GUARDRAILS.md -> 4. Render golden files, in the
// order and nearly the words of that list, each with the name of its pair of
// files. This list is what is rendered and what golden/ is held to: a case
// that is added to the contract is added here, and its files with it.
struct GoldenCase
{
    const char* rule;
    const char* fileName;
};

const std::vector<GoldenCase>& goldenCases()
{
    static const std::vector<GoldenCase> cases {
        { "skipped column", "step_skipped_column" },
        { "resetAt shorter than stepCount", "step_reset_at_shorter_than_step_count" },
        { "gate > 1.0 into a different pitch", "step_gate_over_one_into_different_pitch" },
        { "gate > 1.0 into the same pitch (merge)", "step_gate_over_one_into_same_pitch" },
        { "gate reaching into a skipped column", "step_gate_reaching_into_skipped_column" },
        { "gate across the loop point into a different pitch", "step_gate_across_loop_point_into_different_pitch" },
        { "gate across the loop point into the same pitch (retrigger, not merge)", "step_gate_across_loop_point_into_same_pitch" },
        { "fixedPitch lane ignoring cell pitch", "step_fixed_pitch_ignoring_cell_pitch" },
        { "off-cell CC values", "step_off_cell_cc_values" },
        { "placement offset", "placement_offset" },
        { "placement shorter than the clip loop", "placement_shorter_than_loop" },
        { "placement longer than the clip loop", "placement_longer_than_loop" },
        { "two placements of one clip restarting (offset 0)", "placements_restarting" },
        { "two placements of one clip phase-continuous (merged)", "placements_phase_continuous" },
        { "split during a held note", "split_during_held_note" },
        { "same-pitch overlap on one track", "same_pitch_overlap_one_track" },
        { "same-pitch overlap across two tracks on one destination", "same_pitch_overlap_two_tracks" },
        { "bank-select, program change and note at one tick", "bank_select_program_change_note" },
        { "note-off and note-on of one pitch at one tick", "note_off_and_note_on_same_pitch_same_tick" },
        { "AutomationLane overriding", "automation_lane_overriding" },
        { "AutomationLane passing through", "automation_lane_passing_through" },
        { "AutomationLane restoring the underlying value at exit", "automation_lane_restoring_at_exit" },
        { "a lane whose first event comes after its placement start", "automation_lane_first_event_after_placement_start" },
        { "two lanes overriding the same CC", "automation_two_lanes_one_cc" },
        { "event-clip note crossing loopLength", "note_crossing_loop_length" },
        { "content past loopLength silent", "content_past_loop_length" },
        { "quantise at 1/8", "quantise_eighth" },
        { "quantise at 1/8T", "quantise_eighth_triplet" },
        { "quantise moving an onset across the loop point, in a loop that is not a multiple of the grid", "quantise_wrap_across_loop_point" },
        { "two notes of one pitch quantised onto one tick", "quantise_two_notes_one_pitch_one_tick" },
        { "transport loop wrap with a held note", "transport_loop_wrap_with_held_note" },
        { "chase at a seek target: value found in the current placement", "chase_value_in_current_placement" },
        { "chase at a seek target: value found in an earlier iteration of the placement", "chase_value_in_earlier_iteration" },
        { "chase at a seek target: value found in an earlier placement", "chase_value_in_earlier_placement" },
        { "chase at a seek target: no value at all", "chase_no_value_at_all" },
        { "export range cutting a note", "export_range_cutting_note" },
        { "a track with no Player rendering normally", "track_without_player" },
    };

    return cases;
}

} // namespace

TEST_CASE ("guardrail 4: every required case has its pair of files, and golden/ holds no other file", "[render][golden]")
{
    juce::StringArray listedFiles;

    for (const auto& goldenCase : goldenCases())
    {
        INFO ("case: " << goldenCase.rule);

        for (const char* suffix : { ".xml", ".txt" })
        {
            const juce::String fileName = juce::String (goldenCase.fileName) + suffix;

            CHECK (goldenFile (fileName).existsAsFile());

            // No pair stands for two cases.
            CHECK (! listedFiles.contains (fileName));
            listedFiles.add (fileName);
        }
    }

    // The 37 cases of the contract. A case more is a change of
    // docs/GUARDRAILS.md and of this number; a case fewer is never right.
    CHECK (goldenCases().size() == 37);

    // Everything in golden/, hidden files and directories too: a fixture
    // that no case names would be a golden file that nothing renders.
    const juce::File directory (HUMS_RENDER_GOLDEN_DIRECTORY);
    REQUIRE (directory.isDirectory());

    for (const auto& file : directory.findChildFiles (juce::File::findFilesAndDirectories, false))
    {
        // What the Finder leaves in a directory it has shown, by its exact
        // name, as tools/check_layers.py passes it over under src/.
        if (file.getFileName() == ".DS_Store")
            continue;

        INFO ("in golden/: " << file.getFileName());
        CHECK (listedFiles.contains (file.getFileName()));
    }
}

TEST_CASE ("guardrail 4: every case renders to its expected file, the same in small windows, without an allocation, and every note-on gets its note-off", "[render][golden]")
{
    for (const auto& goldenCase : goldenCases())
    {
        DYNAMIC_SECTION ("golden: " << goldenCase.rule)
        {
            checkGoldenCase (goldenCase.fileName);
        }
    }
}
