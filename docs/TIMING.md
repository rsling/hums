# Timing results

Milestone 0 measurements of the MIDI send path, run with `tools/miditiming` on 20 Nov 2026. `docs/ARCHITECTURE.md → Timing` says what is measured and why, and holds the targets **[D1]** and the send-path decision **[D12]**; this file holds the numbers behind them. The raw reports are in `docs/timing/`. Numbers are valid only for the toolchain in `TOOLCHAIN.md` and the setup below.

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
