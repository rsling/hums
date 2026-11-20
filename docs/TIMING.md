# Timing results

Milestone 0 measurements of the MIDI send path, run with `tools/miditiming` on 20 Nov 2026. `docs/ARCHITECTURE.md → Timing` says what is measured and why, and holds the targets **[D1]** and the send-path decision **[D12]**; this file holds the numbers behind them. The raw reports are in `docs/timing/`. Numbers are valid only for the toolchain in `TOOLCHAIN.md` and the setup below.

The raw reports were printed before the hardware targets were renegotiated and before the JUCE timer path was removed. Their verdict lines therefore judge the U6MIDI Pro runs against p99 ≤ 1 ms, max ≤ 2 ms and read NOT MET where the tables below, which apply **[D1]** as changed, say met; the figures themselves are the same. `backend: coremidi` and `backend: juce` in their headers are the CoreMIDI and JUCE timer rows; the tool now prints `send path:` instead.

## Setup

- Mac Studio, Apple M1 Ultra, macOS 27.0.1; JUCE 9.0.6; plain Release build (`build-release`).
- Software loopback: IAC Driver Bus 1, output to input.
- Hardware: one interface **[D11]**, the U6MIDI Pro (USB, 3 DIN outs and 3 DIN ins), each OUT *n* cabled to IN *n*. Logic and other MIDI apps quit.
- Sender: a real-time thread (mach time-constraint policy, granted on every run) wakes every 2 ms and hands over every message due within the next 10 ms, the way the engine will.
- Traffic: sixteenths at 121 bpm for 5 minutes. Each track sends CC 74 and a note-on at every step start, and a note-off half a step later, on its own channel. 121 rather than 120 bpm so that a sixteenth is not a whole number of milliseconds (see *Pitfalls*).
  - **single**: one track.
  - **stacked**: four tracks on one port, so four CCs and four note-ons leave at one tick.
  - **spread**: the same four tracks round-robin over the three U6MIDI loops (the interface has three, not four).
- Load: *quiet* means the Mac was left alone. *Loaded* means Roland continuously dragged and resized other apps' windows while a multi-GB Finder copy ran.

## How the numbers are computed

- **Lateness** = arrival time − intended time, per message, in ms.
- **Arrival time** is read from the `MonotonicClock` inside JUCE's input callback. JUCE 9.0.6 maps CoreMIDI packet times onto a millisecond counter anchored at a whole millisecond, which would add an unknown constant of up to 1 ms. The callback time also includes CoreMIDI's delivery to the callback, so the figures slightly overstate lateness.
- **Net** (hardware only) = lateness − computed DIN wire time. Wire time is the message's own bytes plus the bytes queued ahead of it on the same line, at 0.32 ms per byte. Consecutive messages on a port never share a status byte, so running status can't shorten the wire time.
- A loopback measures the input path as well as the output path. Hardware figures therefore include the interface's USB input as well as its output, and overstate what a synth sees.
- **Drift** is the least-squares slope of lateness over the run. It was below ±0.01 ms per minute in every run, so it is not tabled.

## Results

Median / p99 / max lateness in ms. Every run: zero drops, zero unexpected messages, nothing handed to the backend after its intended time.

**Software loopback (IAC), judged on raw lateness against p99 ≤ 1, max ≤ 2.**

| send path | density | load | median | p99 | max | [D1] |
|---|---|---|---|---|---|---|
| CoreMIDI | stacked | quiet | 0.090 | 0.162 | 0.453 | met |
| CoreMIDI | single | quiet | 0.101 | 0.178 | 0.390 | met |
| CoreMIDI | stacked | loaded | 0.079 | 0.119 | 0.214 | met |
| JUCE timer | stacked | quiet | 0.649 | 1.159 | 1.239 | **not met** |

**U6MIDI Pro, judged on net lateness against p99 ≤ 2, max ≤ 3.**

| send path | density | load | raw median / p99 / max | net median | net p99 | net max | [D1] |
|---|---|---|---|---|---|---|---|
| CoreMIDI | single | quiet | 2.274 / 3.257 / 3.480 | 1.182 | 1.523 | 2.454 | met |
| CoreMIDI | stacked | quiet | 4.697 / 9.088 / 9.593 | 1.258 | 1.587 | 2.647 | met |
| CoreMIDI | spread | quiet | 2.917 / 5.257 / 5.745 | 1.361 | 1.917 | 2.848 | met |
| CoreMIDI | single | loaded | 2.074 / 3.042 / 3.237 | 1.021 | 1.242 | 1.372 | met |
| CoreMIDI | stacked | loaded | 4.319 / 8.946 / 9.168 | 1.132 | 1.368 | 1.651 | met |
| CoreMIDI | spread | loaded | 2.738 / 5.094 / 5.348 | 1.202 | 1.667 | 1.933 | met |
| JUCE timer | stacked | quiet | 5.110 / 9.879 / 10.281 | 1.795 | 2.422 | 2.807 | not met (p99) |

**After the code review (21 Nov 2026).** The review of milestone 0 changed the send path after the runs above: port lookup, flush only when a port has pending messages, large SysEx split into several event lists, a second open refused, and a watcher thread that makes `send` refuse a port that has gone. One software-loopback run was repeated with the changed code (`docs/timing/2026-11-21-after-review.txt`): CoreMIDI, stacked, quiet, 5 minutes: 0.075 / 0.114 / 0.155, zero drops, met. That is within the spread of the rows above. One hardware run was repeated the same day on the U6MIDI Pro (OUT 1 cabled to IN 1): CoreMIDI, stacked, quiet, 5 minutes: raw 4.593 / 9.128 / 9.363, net 1.292 / 1.580 / 2.244, zero drops, met; the row above for the same case reads net 1.258 / 1.587 / 2.647. Both reports are in that file.

**A port that goes away.** With the U6MIDI Pro's USB cable pulled 32 s into a stacked send run, CoreMIDI kept the port in its destination list and marked it offline 0.09 s after the last message arrived. The backend's watcher saw that and `send` refused everything from then on: of 14 428 messages that never arrived, 14 416 were refused and 12 (about 0.12 s) had already been accepted. Without the watcher CoreMIDI accepts every one of them silently.

**Sender thread wake-up lateness** across all runs: median ≈ 0.02 ms, p99 ≤ 0.083 ms, max ≤ 0.293 ms.

## What the numbers say

- **Send path [D12]: CoreMIDI with future timestamps.** On the software loopback it has about six times the headroom the target asks for, quiet or loaded. The 1 ms `HighResolutionTimer` + `sendMessageNow` path misses the target even on a quiet machine, because a message waits up to a whole timer period. It was removed after these runs.
- **The interface sets the hardware floor.** The lowest net lateness over the U6MIDI Pro is 0.7–0.9 ms, against 0.04 ms on IAC with the same send path, so most of the hardware figure is spent inside the interface and its USB link. Hence the hardware targets of p99 ≤ 2 ms, max ≤ 3 ms **[D1]**.
- **Load made things slightly better, not worse.** A busy Mac keeps the CPU out of deep power saving, so the idle machine is the harder case. Milestone 3 repeats the loaded check with the app's own GUI and a save in progress.
- **Queuing on one DIN port dominates raw lateness.** Four CCs and four note-ons at one tick on one port take 7.7 ms of wire time, which shows up in the stacked raw p99 of about 9 ms. Spreading tracks over ports halves it. This is MIDI's bandwidth, not something software can fix.

## Pitfalls

- **Tempo alignment.** At 120 bpm a sixteenth is exactly 125 ms, so with a 1 ms timer every message lands at the same timer phase. A one-minute JUCE-timer run on IAC then showed p99 0.693 ms; at 121 bpm the same run gave 1.073 ms. `miditiming` defaults to 121 bpm for this reason.
- Short runs give meaningless drift figures; use the full 5 minutes.

## Repeating a run

```
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target miditiming
build-release/tools/miditiming/miditiming --list
build-release/tools/miditiming/miditiming --out "U6MIDI Pro Port 1" --in "U6MIDI Pro Port 1" \
    --density stacked --din --load "quiet" --report docs/timing/<date>-<what>.txt
```

`--csv <file>` writes one row per message (intended, hand-over and arrival times, wire time) for closer inspection. The CSVs from these runs are not kept; they are a few MB each and rerunning reproduces them.

## Listen mode (for milestone 3)

`miditiming --listen` records what another app plays into a loopback and compares it with that app's SMF export of the same project, which the same Kernel renders, so it is what the engine meant to send: tick for tick, in the order within each tick, and in time (see *Tempo* below).

```
build-release/tools/miditiming/miditiming --listen --in "<port>" --reference <export.mid> [--din] \
    --load "<what>" --report docs/timing/<date>-<what>.txt
```

Start it first, then start playback from the beginning in the app. Recording begins with the first message and lasts the reference's length plus 2 s. Keep the port quiet until playback starts: a stray message before it is counted as unexpected, and one more than 2 s early also ends the recording before the playback does.

- **Lateness is relative.** The listener cannot know when the app meant to start, so arrivals are aligned so that the best-timed message has (net) lateness 0. Jitter, drift, drops, extra messages and a wrong order within a tick all show; a constant delay does not, and on hardware the interface's own floor (0.7–0.9 ms on the U6MIDI Pro) drops out with it. Whether milestone 3 adds an absolute measurement is open question Q8 (`docs/DECISIONS.md`).
- **Matching** is by content, each kind of message in order, within ±50 ms of its aligned time, so several `--in` ports can be merged. Note-offs match by channel and pitch; a note-on with velocity 0 counts as one, and release velocity is not compared. Clock, start/stop and SysEx are not compared; SysEx in the reference is counted.
- **Order within a tick is checked.** The messages the reference holds at one tick must arrive in the reference's order, which is the export's and so the equal-tick order of `docs/RENDERING.md` (a retrigger's note-off before its note-on, bank select before program change). Each tick with any pair the wrong way round counts once; the report gives the count and names the first, and one such tick makes the run NOT MET. Only messages that arrived on the same `--in` are compared, because which of two cables delivers first says nothing about the order the app sent in. Two identical messages on one tick cannot be told apart. Messages of different ticks are not compared: arriving in the wrong order across ticks takes lateness of a whole step and shows as that. For this the reference is read byte by byte, not through `juce::MidiFile`, whose reader moves a note-off in front of a same-pitch note-on on the same tick and would turn a retrigger exported as on, off, on into off, on, on.
- **Tempo.** Reference times come from the export's tempo events, which an SMF holds as whole microseconds per quarter note. Hums's `TempoMap` plays every tempo as exactly that number **[D13]**, so for a Hums export the reference times are the engine's own at any tempo. A file from an app that plays 118 bpm as exactly 118 would read as drifting: by up to bpm × 0.0005 ms per minute, at 118 bpm −0.05 ms per minute and about 0.25 ms after 5 minutes.
- **Net** (`--din`, one `--in` only): the same wire-time model as send mode, but computed twice, because the listener cannot see two things that change how long a message takes on the line. An interface may or may not use running status (leave out a status byte equal to the previous one), and the app may or may not send MIDI clock, whose bytes the recorder drops. The *shortest* wire time assumes running status wherever the reference allows it and no clock; the *longest* assumes every status byte is sent and a clock byte 24 times per quarter note, ahead of the messages of its tick. Each byte is 0.32 ms. The report gives net lateness and a verdict under both. If both say MET the run is MET, if both say NOT MET it is NOT MET, and if they differ it is **INCONCLUSIVE**: the figures cannot say whether the targets were met. Sparse material gives the same figures under both; they part from about six same-status messages on one tick. Whether the U6MIDI Pro uses running status has not been measured; knowing it would settle an inconclusive run.
- Messages arriving more than 50 ms after the reference's last message (a panic or All Notes Off at stop) are counted as *after the end* and ignored.

`--write-reference <file.mid>` writes the send-mode pattern as an SMF, so listen mode can be checked without the app. It needs a tempo whose quarter note is a whole number of microseconds (`--bpm 128`). Checked on 20 Nov 2026 by running send mode and listen mode at once on one loop:

| loop | send mode (absolute) median / p99 / max | listen mode (relative) median / p99 / max |
|---|---|---|
| IAC, stacked, 2 min | raw 0.097 / 0.142 / 0.181 (min 0.045) | raw 0.041 / 0.085 / 0.115 |
| U6MIDI Pro, stacked, 0.5 min | net 1.099 / 1.324 / 1.391 (min 0.769) | net 0.324 / 0.545 / 0.626 |

Per message the two agree to within 0.04 ms around a constant offset. Listen mode also reported each planted fault: a reference with three tracks the sender didn't play (2304 dropped), a sender playing three tracks the reference lacked (2304 unexpected), and messages handed over late within a 4 ms window (relative lateness spread 0–4 ms).

The order check and the byte-level reader were added after that run and have been checked offline only, with synthetic arrivals against hand-written files: a tick delivered reversed, a retrigger pair swapped, the same swap spread over two inputs (not a fault), running status, a tempo change and a truncated file; and the two wire-time bounds, with one to eight same-status messages per tick on a line that sends every status byte (inconclusive from six, with 0.6 ms of jitter). On 21 Nov 2026 send mode and listen mode ran at once on the IAC loop for one minute (stacked, 128 bpm): send mode raw 0.094 / 0.142 / 0.357, listen mode relative 0.030 / 0.072 / 0.257, no tick in the wrong order, both met. The same was done on the U6MIDI Pro loop with `--din`: send mode net 1.239 / 1.562 / 2.099, listen mode net 0.413 / 0.737 / 1.278 under both wire-time bounds (the pattern leaves running status no room, and no clock was on the line), no tick in the wrong order, both met. The two differ by the interface's floor of about 0.82 ms, which relative lateness drops.
