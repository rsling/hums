// miditiming: MIDI timing measurement (docs/ARCHITECTURE.md -> Timing,
// results in docs/TIMING.md). Send mode (milestone 0) sends a dense pattern
// through platform::MidiScheduler (CoreMidiScheduler, the engine's send path)
// and measures what comes back in. Listen mode (milestone 3) records what
// another app plays into a loopback and compares it with that app's SMF
// export.
// Measure only on a plain Release build, never an instrumented one.

#include "InputRecorder.h"
#include "ListenReport.h"
#include "ReferenceFile.h"
#include "Sender.h"
#include "TimingReport.h"
#include "TrafficPattern.h"

#include "platform/mac/CoreMidiScheduler.h"
#include "platform/mac/MachMonotonicClock.h"
#include "platform/mac/MachRealtimeThread.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace
{

const char* usage = R"(usage:
  miditiming --list
  miditiming --out <port> --in <port> [--out <port> --in <port> ...] [options]
  miditiming --listen --in <port> [--in <port> ...] --reference <file.mid> [--din] [--report <file>] [--csv <file>]
  miditiming --write-reference <file.mid> [--density ...] [--minutes ...] [--bpm ...]

Send mode: each --out is looped back by cable (or IAC bus) to the --in at the same position.

options:
  --density single|stacked|spread   (default stacked)
  --minutes <m>              run length (default 5, at most 1440)
  --bpm <n>                  tempo of the sixteenths, 1 to 1000 (default 121: a sixteenth is not
                             a whole number of milliseconds, so events don't all land at one
                             phase of a 1 ms timer, as they would at 120)
  --window-ms <ms>           sender wake-up period, 0.001 to 1000 (default 2)
  --lookahead-ms <ms>        how far ahead messages are handed over, 0.001 to 10000 (default 10)
  --din                      the loop is a DIN cable: also report lateness net of wire time
  --load "<text>"            what load the tester produced during the run (recorded in the report)
  --report <file>            append the report to this file
  --csv <file>               write one row per message to this file

listen mode: start miditiming first, then start playback from the beginning in the other app.
Recording begins with the first message and lasts the reference's length plus 2 s.
  --reference <file.mid>     what the other app meant to send: its SMF export of the same project
  --din                      one --in, looped over a DIN cable: also report lateness net of wire time,
                             for the shortest and the longest wire time the line can have had
                             (running status or not, MIDI clock or not); INCONCLUSIVE if only
                             one of the two meets the targets
  Lateness is relative: the best-timed message is lateness 0 (the listener can't know when the
  other app meant to start). Clock, start/stop and SysEx are not compared.
  Messages of one tick must arrive in the reference's order (checked per input).

--write-reference writes the send-mode pattern as an SMF and exits, to check listen mode against
send mode or another app. Needs a tempo whose quarter note is a whole number of microseconds
(e.g. --bpm 128).
)";

struct Options
{
    bool list = false;
    bool listen = false;
    std::string referenceFile, writeReferenceFile;
    std::vector<std::string> outs, ins;
    std::string density = "stacked";
    double minutes = 5, bpm = 121, windowMs = 2, lookAheadMs = 10;
    bool din = false;
    std::string load = "none (quiet)";
    std::string reportFile, csvFile;
};

std::optional<Options> parse (int argc, char** argv)
{
    Options o;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const auto value = [&]() -> std::optional<std::string> {
            if (i + 1 >= argc)
                return std::nullopt;
            return std::string (argv[++i]);
        };
        // The whole text has to be one number in the range: std::stod alone
        // would take "121,5" as 121 and let "inf" through, and a value that
        // rounds to zero nanoseconds would divide by zero further on.
        const auto number = [&] (double& target, double lowest, double highest) {
            const auto v = value();
            if (! v)
                return false;

            std::size_t used = 0;
            try
            {
                target = std::stod (*v, &used);
            }
            catch (...)
            {
                return false;
            }

            return used == v->size() && target >= lowest && target <= highest;
        };

        bool ok = true;
        // One line per option, so the table can be read as a table.
        // clang-format off
        if (arg == "--list") o.list = true;
        else if (arg == "--listen") o.listen = true;
        else if (arg == "--din") o.din = true;
        else if (arg == "--out") { auto v = value(); ok = v.has_value(); if (ok) o.outs.push_back (*v); }
        else if (arg == "--in") { auto v = value(); ok = v.has_value(); if (ok) o.ins.push_back (*v); }
        else if (arg == "--density") { auto v = value(); ok = v.has_value(); if (ok) o.density = *v; }
        else if (arg == "--load") { auto v = value(); ok = v.has_value(); if (ok) o.load = *v; }
        else if (arg == "--report") { auto v = value(); ok = v.has_value(); if (ok) o.reportFile = *v; }
        else if (arg == "--reference") { auto v = value(); ok = v.has_value(); if (ok) o.referenceFile = *v; }
        else if (arg == "--write-reference") { auto v = value(); ok = v.has_value(); if (ok) o.writeReferenceFile = *v; }
        else if (arg == "--csv") { auto v = value(); ok = v.has_value(); if (ok) o.csvFile = *v; }
        else if (arg == "--minutes") ok = number (o.minutes, 0.001, 1440);
        else if (arg == "--bpm") ok = number (o.bpm, 1, 1000);
        else if (arg == "--window-ms") ok = number (o.windowMs, 0.001, 1000);
        else if (arg == "--lookahead-ms") ok = number (o.lookAheadMs, 0.001, 10000);
        else ok = false;
        // clang-format on

        if (! ok)
        {
            std::cerr << "bad or incomplete argument: " << arg << "\n\n"
                      << usage;
            return std::nullopt;
        }
    }

    return o;
}

std::optional<juce::MidiDeviceInfo> findDevice (const juce::Array<juce::MidiDeviceInfo>& devices, const std::string& nameOrId)
{
    for (const auto& d : devices)
        if (d.name.toStdString() == nameOrId)
            return d;
    for (const auto& d : devices)
        if (d.identifier.toStdString() == nameOrId)
            return d;
    return std::nullopt;
}

void listPorts()
{
    platform::CoreMidiScheduler coreMidi;

    std::cout << "outputs (name | identifier | CoreMIDI backend resolves it):\n";
    for (const auto& d : juce::MidiOutput::getAvailableDevices())
    {
        const auto handle = coreMidi.open (d.identifier.toStdString());
        std::cout << "  " << d.name << " | " << d.identifier << " | " << (handle >= 0 ? "yes" : "NO") << "\n";
        coreMidi.close (handle);
    }

    std::cout << "inputs (name | identifier):\n";
    for (const auto& d : juce::MidiInput::getAvailableDevices())
        std::cout << "  " << d.name << " | " << d.identifier << "\n";
}

std::string nowText()
{
    return juce::Time::getCurrentTime().toString (true, true, true, true).toStdString();
}

// Timing figures count only from a plain Release build (docs/GUARDRAILS.md ->
// Sanitizer builds), so every report says which build produced it.
std::string buildText()
{
#ifdef NDEBUG
    return "Release";
#else
    return "Debug (timing figures count only from a plain Release build)";
#endif
}

// Checked before a run starts, so that a mistyped path costs a message, not
// the minutes of the run. An existing file is left as it is.
bool canWriteOutputFiles (const Options& o)
{
    for (const auto* path : { &o.reportFile, &o.csvFile })
    {
        if (path->empty())
            continue;

        std::error_code ignored;
        const bool existed = std::filesystem::exists (*path, ignored);
        const bool opened = std::ofstream (*path, std::ios::app).is_open();

        if (opened && ! existed)
            std::filesystem::remove (*path, ignored);

        if (! opened)
        {
            std::cerr << "cannot write " << *path << "\n";
            return false;
        }
    }

    return true;
}

bool writeTextFile (const std::string& path, const std::string& text, std::ios::openmode mode)
{
    std::ofstream file (path, mode);
    file << text;
    file.close();

    if (file.fail())
    {
        std::cerr << "could not write " << path << "\n";
        return false;
    }

    return true;
}

// The report is appended to --report, the per-message rows replace --csv.
bool writeOutputFiles (const Options& o, const std::string& reportText, const std::string& csvText)
{
    bool written = true;

    if (! o.reportFile.empty())
        written = writeTextFile (o.reportFile, reportText + "\n", std::ios::app) && written;

    if (! o.csvFile.empty())
        written = writeTextFile (o.csvFile, csvText, std::ios::out) && written;

    return written;
}

int run (const Options& o)
{
    bool densityOk = false;
    const auto density = miditiming::densityFromName (o.density, densityOk);

    if (! densityOk)
    {
        std::cerr << usage;
        return 2;
    }

    if (o.outs.empty() || o.outs.size() != o.ins.size())
    {
        std::cerr << "give the same number of --out and --in ports (at least one each)\n";
        return 2;
    }

    if (density != miditiming::Density::spread && o.outs.size() > 1)
    {
        std::cerr << "only --density spread uses more than one port pair\n";
        return 2;
    }

    if (! canWriteOutputFiles (o))
        return 1;

    const auto outputDevices = juce::MidiOutput::getAvailableDevices();
    const auto inputDevices = juce::MidiInput::getAvailableDevices();

    platform::MachMonotonicClock clock;
    platform::MachRealtimeThread realtime;

    platform::CoreMidiScheduler coreMidi;
    if (! coreMidi.isReady())
    {
        std::cerr << "could not create the CoreMIDI client\n";
        return 1;
    }

    // Everything below sees only the interface, as the engine will.
    platform::MidiScheduler& scheduler = coreMidi;

    const miditiming::TrafficPattern pattern (density, static_cast<int> (o.outs.size()), o.bpm, o.minutes);

    // Generous: every message could arrive on one input.
    const auto capacity = pattern.messages().size() + 4096;

    std::vector<std::unique_ptr<miditiming::InputRecorder>> recorders;
    std::vector<std::unique_ptr<juce::MidiInput>> inputs;
    std::vector<platform::MidiScheduler::PortHandle> ports;

    for (std::size_t i = 0; i < o.outs.size(); ++i)
    {
        const auto out = findDevice (outputDevices, o.outs[i]);
        const auto in = findDevice (inputDevices, o.ins[i]);

        if (! out || ! in)
        {
            std::cerr << "no such port: " << (! out ? "output " + o.outs[i] : "input " + o.ins[i]) << " (see --list)\n";
            return 1;
        }

        recorders.push_back (std::make_unique<miditiming::InputRecorder> (clock, capacity));
        auto input = juce::MidiInput::openDevice (in->identifier, recorders.back().get());
        if (input == nullptr)
        {
            std::cerr << "could not open input " << o.ins[i] << "\n";
            return 1;
        }
        inputs.push_back (std::move (input));

        const auto handle = scheduler.open (out->identifier.toStdString());
        if (handle == platform::MidiScheduler::invalidPort)
        {
            std::cerr << "could not open output " << o.outs[i] << " (a port can be open only once: is it given twice?)\n";
            return 1;
        }
        ports.push_back (handle);
    }

    miditiming::Sender::Settings settings;
    settings.windowNanoseconds = static_cast<std::int64_t> (o.windowMs * 1.0e6);
    settings.lookAheadNanoseconds = static_cast<std::int64_t> (o.lookAheadMs * 1.0e6);

    miditiming::Sender sender (scheduler, clock, realtime, pattern, ports, settings);

    for (auto& input : inputs)
        input->start();

    const auto startedAt = nowText();
    std::cout << "running " << o.minutes << " min, " << pattern.messages().size() << " messages, "
              << pattern.describe() << "\n"
              << std::flush;

    // Time zero one second out, so ports and threads have settled.
    const auto start = clock.nowNanoseconds() + 1'000'000'000;
    std::thread senderThread ([&] { sender.run (start); });

    const auto end = start + pattern.lengthNanoseconds();
    while (clock.nowNanoseconds() < end)
    {
        std::this_thread::sleep_for (std::chrono::seconds (1));
        const auto elapsed = (clock.nowNanoseconds() - start) / 1'000'000'000;
        if (elapsed > 0 && elapsed % 30 == 0)
            std::cout << "  " << elapsed << " s\n"
                      << std::flush;
    }

    senderThread.join();

    // Let the last messages arrive: DIN queues can hold several ms.
    std::this_thread::sleep_for (std::chrono::milliseconds (500));

    for (auto& input : inputs)
        input->stop();

    miditiming::TimingReport::Input reportInput;
    reportInput.pattern = &pattern;
    reportInput.sender = &sender;
    reportInput.startNanoseconds = start;
    reportInput.din = o.din;

    for (auto& recorder : recorders)
    {
        reportInput.receivedPerPort.push_back (recorder->received());
        reportInput.inputOverflows += recorder->overflowCount();
    }

    std::string pairs;
    for (std::size_t i = 0; i < o.outs.size(); ++i)
        pairs += (i > 0 ? "; " : "") + o.outs[i] + " -> " + o.ins[i];

    reportInput.header = {
        "started: " + startedAt,
        "send path: CoreMidiScheduler (CoreMIDI, future timestamps)",
        "pattern: " + pattern.describe(),
        "loop: " + pairs + (o.din ? " (DIN)" : " (software)"),
        "length: " + juce::String (o.minutes, 2).toStdString() + " min",
        "sender: window " + juce::String (o.windowMs, 2).toStdString() + " ms, look-ahead "
            + juce::String (o.lookAheadMs, 2).toStdString() + " ms, real-time priority "
            + (sender.realtimeGranted() ? "granted" : "REFUSED"),
        "load: " + o.load,
        "system: " + juce::SystemStats::getOperatingSystemName().toStdString() + ", "
            + juce::SystemStats::getCpuModel().toStdString() + ", " + juce::SystemStats::getJUCEVersion().toStdString(),
        "build: " + buildText(),
        "arrival time: MonotonicClock read in the input callback (includes CoreMIDI delivery)",
    };

    const miditiming::TimingReport report (std::move (reportInput));
    std::cout << "\n"
              << report.text() << std::flush;

    return writeOutputFiles (o, report.text(), o.csvFile.empty() ? std::string() : report.csv()) ? 0 : 1;
}

int writeReference (const Options& o)
{
    bool densityOk = false;
    const auto density = miditiming::densityFromName (o.density, densityOk);
    if (! densityOk)
    {
        std::cerr << usage;
        return 2;
    }

    const miditiming::TrafficPattern pattern (density, std::max<int> (1, static_cast<int> (o.outs.size())), o.bpm, o.minutes);

    std::string error;
    if (! miditiming::ReferenceFile::writePattern (pattern, o.bpm, o.writeReferenceFile, error))
    {
        std::cerr << error << "\n";
        return 1;
    }

    std::cout << "wrote " << o.writeReferenceFile << ": " << pattern.messages().size() << " messages, " << pattern.describe() << "\n";
    return 0;
}

int listen (const Options& o)
{
    if (o.ins.empty() || ! o.outs.empty() || o.referenceFile.empty())
    {
        std::cerr << "listen mode takes one or more --in, a --reference and no --out\n";
        return 2;
    }

    if (o.din && o.ins.size() != 1)
    {
        std::cerr << "--din needs exactly one --in: wire time is computed for one line\n";
        return 2;
    }

    if (! canWriteOutputFiles (o))
        return 1;

    miditiming::ReferenceFile reference;
    std::string error;
    if (! reference.read (o.referenceFile, error))
    {
        std::cerr << error << "\n";
        return 1;
    }

    const auto& messages = reference.messages();
    const auto span = messages.back().offsetNanoseconds - messages.front().offsetNanoseconds;

    platform::MachMonotonicClock clock;
    const auto inputDevices = juce::MidiInput::getAvailableDevices();
    const auto capacity = messages.size() + 4096;

    std::vector<std::unique_ptr<miditiming::InputRecorder>> recorders;
    std::vector<std::unique_ptr<juce::MidiInput>> inputs;

    for (const auto& name : o.ins)
    {
        const auto in = findDevice (inputDevices, name);
        if (! in)
        {
            std::cerr << "no such port: input " << name << " (see --list)\n";
            return 1;
        }

        recorders.push_back (std::make_unique<miditiming::InputRecorder> (clock, capacity));
        auto input = juce::MidiInput::openDevice (in->identifier, recorders.back().get());
        if (input == nullptr)
        {
            std::cerr << "could not open input " << name << "\n";
            return 1;
        }
        inputs.push_back (std::move (input));
    }

    for (auto& input : inputs)
        input->start();

    const auto startedAt = nowText();
    std::cout << "reference: " << messages.size() << " messages over " << juce::String (static_cast<double> (span) / 60.0e9, 2)
              << " min\nlistening; start playback from the beginning now\n"
              << std::flush;

    const auto anyArrived = [&] {
        for (const auto& r : recorders)
            if (r->receivedCount() > 0)
                return true;
        return false;
    };

    while (! anyArrived())
        std::this_thread::sleep_for (std::chrono::milliseconds (5));

    // Everything the reference holds, plus room for DIN queues and stragglers.
    const auto end = clock.nowNanoseconds() + span + 2'000'000'000;
    std::cout << "first message arrived, recording\n"
              << std::flush;

    std::int64_t lastPrinted = 0;
    while (clock.nowNanoseconds() < end)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
        const auto left = (end - clock.nowNanoseconds()) / 1'000'000'000;
        if (left > 0 && left % 30 == 0 && left != lastPrinted)
        {
            std::cout << "  " << left << " s to go\n"
                      << std::flush;
            lastPrinted = left;
        }
    }

    for (auto& input : inputs)
        input->stop();

    miditiming::ListenReport::Input reportInput;
    reportInput.reference = &reference;
    reportInput.din = o.din;

    for (std::size_t i = 0; i < recorders.size(); ++i)
    {
        auto& recorder = recorders[i];
        auto received = recorder->received();
        for (auto& message : received)
            message.input = static_cast<int> (i);
        reportInput.received.insert (reportInput.received.end(), received.begin(), received.end());
        reportInput.inputOverflows += recorder->overflowCount();
    }

    // Stable, so the messages of one input keep the order they arrived in.
    std::stable_sort (reportInput.received.begin(), reportInput.received.end(),
                      [] (const auto& a, const auto& b) { return a.timeNanoseconds < b.timeNanoseconds; });

    std::string ports;
    for (std::size_t i = 0; i < o.ins.size(); ++i)
        ports += (i > 0 ? "; " : "") + o.ins[i];

    reportInput.header = {
        "started: " + startedAt,
        "reference: " + o.referenceFile + " (" + std::to_string (reference.trackCount()) + " tracks, "
            + std::to_string (messages.size()) + " channel messages)",
        "input: " + ports + (o.din ? " (DIN)" : " (software)"),
        "load: " + o.load,
        "system: " + juce::SystemStats::getOperatingSystemName().toStdString() + ", "
            + juce::SystemStats::getCpuModel().toStdString() + ", " + juce::SystemStats::getJUCEVersion().toStdString(),
        "build: " + buildText(),
        "arrival time: MonotonicClock read in the input callback (includes CoreMIDI delivery)",
    };

    const miditiming::ListenReport report (std::move (reportInput));
    std::cout << "\n"
              << report.text() << std::flush;

    return writeOutputFiles (o, report.text(), o.csvFile.empty() ? std::string() : report.csv()) ? 0 : 1;
}

} // namespace

int main (int argc, char** argv)
{
    const auto options = parse (argc, argv);
    if (! options)
        return 2;

    // JUCE's MIDI device list expects a MessageManager to exist; the loop
    // itself never needs to run in a one-shot tool.
    juce::ScopedJuceInitialiser_GUI juce;

    if (options->list)
    {
        listPorts();
        return 0;
    }

    if (! options->writeReferenceFile.empty())
        return writeReference (*options);

    if (options->listen)
        return listen (*options);

    return run (*options);
}
