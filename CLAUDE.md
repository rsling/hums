# CLAUDE.md — MIDI Sequencer

Working title: none yet. Call it "the sequencer" until Roland picks a name.

**This file is authoritative for behaviour and architecture; `docs/SCHEMA.md` is authoritative for the document model.** Read both before touching anything under `src/`. If they disagree, stop and ask.

Revised 3 Oct 2026 after an external design review; quantisation contract, Transform slot and live-thru non-goal tightened the same day; later the same day: mute/solo and transport-loop command paths, the std-only render core, `EditContext` as a gesture object, retrigger at the step-clip loop point, chase tables, recorded input converted to ticks on the engine. Decisions marked **(default — confirm)** were taken by proposal and still need Roland's explicit yes; treat them as binding until he says otherwise.

## What this is

A MIDI-only sequencer for macOS, built with JUCE. It implements one person's (Roland's) personal workflow for sequencing analog hardware synths over MIDI. No audio, no plugins. It has to be reliable enough to be used in real music-making for years, so maintainability beats cleverness and feature count.

The app is a set of tabs, each a different way of looking at and entering the *same* musical data: a classic piano roll, an Ableton-style clip arrangement, an MPC-style drum grid, a 1970s-style analog step sequencer (Moog 960 / ARP 2500 / Korg SQ-10 workflow), and a parameter recorder for CC sweeps. Plus a Routing tab that maps physical MIDI ports to stable logical names.

## Non-negotiable rules

1. **macOS only.** No Linux/Windows conditionals, no platform abstractions "just in case". Using CoreMIDI directly where JUCE's MIDI output can't deliver the timing is allowed; that is what macOS-only buys us.
2. **No audio path.** No `AudioProcessor`, no `AudioDeviceManager` for sound, no plugin hosting.
3. **All musical content is MIDI.** Notes, CCs, pitch bend, program change, aftertouch, SysEx. If a feature's musical result cannot be expressed as a stream of MIDI events, it does not exist. *Structure* over events (clips, placements, lanes, step grids) is ours and lives only in the project file.
4. **Native project format is the ValueTree serialised to XML.** Standard MIDI File (SMF type 1) is a first-class export and import, always flattened. "First-class" means musical equivalence with reported losses, not byte-identical round-trips. Do not encode structure into SMF meta events.
5. **The document model is `juce::ValueTree` + `juce::UndoManager`.** Never use `juce::MidiMessageSequence` or `juce::MidiFile` as the editing model. They are I/O helpers only.
6. **Views never own musical data.** Every tab is a lens over the one document.
7. **The engine thread never touches the ValueTree.** It consumes plain-data snapshots and never includes a `model/` header.
8. **Rendering is independent of routing.** A Track renders and exports whether or not its Player is bound to a port. Only live transmission depends on hardware.
9. **Readable over compact.** No golfed code, no clever templates where a plain class does. Performance is a concern only on the engine thread.
10. **No throw-away code.** Every model mutation goes through a command function that takes an `UndoManager&`. Every schema change bumps `schemaVersion` and ships a migration plus a test.
11. **Ask rather than guess at workflow.** Roland has strong, specific opinions about how sequencing should feel. A wrong assumption costs more than a question. Ask in plain terms; he has not written GUI or C++ in ten years and does not know JUCE in depth.
12. **The guardrails are not optional.** A change that makes a guardrail test fail is wrong until the guardrail itself has been discussed and changed on purpose. Never weaken, skip, or `// NOLINT` a guardrail to make a feature land. See *Guardrails*.

## Toolchain

- JUCE 8.x, pinned to an exact release tag (recorded in `libs/JUCE_VERSION`), as a git submodule in `libs/JUCE`. Built via JUCE's CMake API. No Projucer. Current master docs are reference, never a reason to move the pin.
- C++20, Xcode's clang. Pinned Xcode, CMake and macOS deployment target recorded in `TOOLCHAIN.md` when milestone 0 lands.
- Tests: Catch2 v3 via CMake `FetchContent`, pinned. Tests cover model, IO, rendering, and engine logic. GUI is not unit-tested; it has a manual checklist (`docs/MANUAL_CHECKS.md`).
- Guardrail checks are plain scripts in `tools/`, registered as CTest tests so `ctest` fails on a violation. Each script ships a fixture that must *fail* it, and that negative test runs too. No separate CI is assumed; the local test run is the gate.
- `clang-format` config checked in. Decide the style once, then never discuss it again.

## Commands

Fill in as the build system lands. Expected shape:

- Configure: `cmake -B build -G Xcode` (or `-G Ninja` for CLI-only builds)
- Build: `cmake --build build --config Debug`
- Test: `ctest --test-dir build -C Debug --output-on-failure` (includes the guardrail checks; always read-only)
- Guardrails only: `ctest --test-dir build -C Debug -R guardrail --output-on-failure`
- Regenerate render golden files: `cmake --build build --target regenerate_golden` (sets `SEQ_REGENERATE_GOLDEN=1` and runs the render test binary) — then read the diff before committing it. CTest does not forward arguments to tests; never rely on `ctest -- flag`.
- Timing diagnostic: `build/tools/miditiming --out "<port>" --in "<port>" --minutes 5`
- Run: `open build/<AppName>_artefacts/Debug/<AppName>.app`

## Repository layout

```
CMakeLists.txt
CLAUDE.md
TOOLCHAIN.md           pinned versions, written at milestone 0
docs/
  SCHEMA.md            ValueTree schema (authoritative)
  MANUAL_CHECKS.md     GUI interaction checklist, run at milestone boundaries
libs/
  JUCE/                submodule, pinned tag
tools/
  check_identifiers.py Ids.h is the only place schema names appear in property APIs
  check_layers.py      include graph obeys the layer rules
  fixtures/            one deliberately violating file per check
  miditiming/          loopback jitter measurement (JUCE console app, kept)
src/
  app/                 Main, MainWindow, tab host, application-level wiring
    TransportFacade.h  the only door from views to engine
  model/               ValueTree schema ids, typed wrappers, commands, validator, refcount gate
    Ids.h              every ValueTree type/property identifier, nowhere else as strings
    Validator          invariants 1–12, run on load and after every command in tests
    commands/          free functions that mutate the document (take UndoManager&)
  io/                  ProjectFile (XML, canonical writer, atomic save), SmfExport, SmfImport, Migrations
  render/              document → plain data, and the playback kernel shared by engine and export
    snapshot/          plain structs only: Snapshot, ClipTemplate, TrackPlan, chase tables — std only
    TempoMap           the one tick ↔ time authority — std only
    Kernel             expands a Snapshot over a tick window into an event stream, allocation-free — std only
    NoteTracker        active-note bookkeeping per physical destination, allocation-free — std only
                       (the four std-only entries above are the "render core")
    SnapshotBuilder    model → Snapshot, including TempoMap data and chase tables; message thread
  engine/              Transport, Clock, Scheduler, MidiRouter, command/event FIFOs, snapshot handoff
  views/
    routing/
    pianoroll/
    arrangement/       arrangement timeline + session slots
    drums/
    stepseq/
    automation/        the parameter recorder
    common/            shared widgets (knob, note grid, timeline ruler)
tests/
  model/
  io/
    fixtures/          valid projects (incl. the schema example with real UUIDs), old-version files, malformed files
  render/
    golden/            fixture project + expected event list, one pair per rendering case
  engine/
  guardrails/          invariant property tests; drives the tools/ scripts and their negative fixtures
```

## Core concepts (glossary)

Use these words exactly, in code, comments, tests, and conversation.

- **Tick** — integer time unit. PPQ = **960** ticks per quarter note, fixed. All positions and lengths are ticks (`int64_t`). Never store seconds or bars in the document. MIDI clock = one pulse every 40 ticks.
- **Timeline** — global tempo map, time-signature map, and markers. Equivalent to an SMF conductor track. Through `TempoMap`, the only authority for tick ↔ time.
- **Performer** — a logical MIDI *input*. Persisted by name, bound to a physical device at runtime.
- **Player** — a logical MIDI *output*. Same. Optionally sends MIDI clock.
- **Track** — a lane in the arrangement. Has exactly one Player and one MIDI channel. Owns Placements, AutomationLanes, Knobs, Slots.
- **Clip** — a unit of musical content in the ClipPool, referenced by id. Two kinds:
  - **EventClip** — notes and other MIDI events at tick positions. Edited in the piano roll. May carry a non-destructive `quantise` setting.
  - **StepClip** — a grid of Columns × monophonic Lanes. Edited in the step sequencer and drum views.
- **Placement** — a reference to a Clip at a position on a Track (or AutomationLane), with a length and an offset into the clip. The clip loops inside the placement. **Resize** changes the length (repeat or trim); there is no stretch.
- **Lane** (in a StepClip) — a monophonic row of Cells. A 960-style sequencer is one lane; a drum pattern is one lane per pad.
- **Cell** — one step in one lane: pitch, on/off, velocity, gate, CC values.
- **Column** — one step index across all lanes of a StepClip; carries the skip flag.
- **AutomationLane** — a track-level layer of CC-only EventClips. A placement *owns* the CC numbers present in its clip for its whole extent and overrides the track's clip CCs for those numbers. Elsewhere clip CCs pass through.
- **Slot** — a launchable clip reference on a track, for session (live) play.
- **Refcount** — the number of Placements and Slots referencing a clip. Computed, never stored.
- **Refcount gate** — the rule that any mutation of a clip with refcount > 1 requires a policy: *update all* or *make unique*.
- **Edit context** — an `EditContext` object the view creates at gesture start and passes to every clip-mutating command of that gesture. It holds the set of references (placement ids, or track + scene for slots) the user is editing through, the policy, and, once make-unique has cloned, the clone's id. Make-unique redirects exactly those references.
- **Gesture** — one user-level action (a drag, a knob move from touch to release, a recording pass). One undo transaction; at most one make-unique clone.
- **Take** — the result of one recording pass: one EventClip per source track.
- **Snapshot** — immutable plain-data rendering of the document: one ClipTemplate per referenced clip, per-track placement plans, the TempoMap and the chase tables. Carries no mute/solo and no transport-loop state; those reach the engine as live commands. Rebuilt on the message thread after edits. Not a flat event list.
- **Kernel** — the function that expands a Snapshot over a tick window into events. The engine runs it per look-ahead window; SMF export runs it over the export range. One implementation.
- **Render core** — `render/snapshot/`, `TempoMap`, `Kernel`, `NoteTracker`: the part of `render/` that includes only std and each other, so the engine can include it without ever seeing `model/` or JUCE.
- **Chase table** — per ClipTemplate, per CC number and for program change: the clip's enabled events of that kind as a sorted (tick, value) list over one loop period. What seek, loop wrap and automation exit read to find "the last value before here" without scanning events.
- **Transform** — a per-track function from event stream to event stream, sitting between Kernel output and NoteTracker. Reserved name for later modulators (arpeggiator, chord player); none exist in v1. Not "effect", not "modulator" in code.
- **Guardrail** — an automated check that fails the test run when a structural rule of this document is broken. Not a style preference; a rule with teeth.

## Modules (tabs)

- **Routing** — bind Performers and Players to physical ports reported by the OS, and edit the PadMap. Reconnect by device identifier, fall back to name, else show "unassigned". Must be simpler than any DAW's track-type / port setup.
- **Piano Roll** — classic whole-track view. Standard note editing, CC lanes, velocity slide on long-click, quantise selector per clip. StepClips render here read-only; editing one offers "convert to EventClip" (gate applies).
- **Arrangement / Session** — Placements on Tracks over a timeline, plus launchable Slots for jamming. Split, move, resize, crop placements; the refcount gate lives here and in every editor.
- **Drums** — x0x-style grid over a StepClip with one Lane per pad, all lanes `fixedPitch`. Pads map to pitches via the PadMap. Any other grid shape is shown read-only.
- **Step Sequencer** — 960 / SQ-10 workflow over a single melodic Lane: one knob per column for pitch, extra rows for per-cell CC values, on/off, skip, reset. Any other grid shape is shown read-only. Needn't look vintage, must *work* vintage.
- **Automation Recorder** — play the arrangement, turn Track knobs, record the CC moves into an AutomationLane placement from punch-in to punch-out.

## Architecture

### Layers and dependencies

Five source layers, and a std-only core inside `render/`:

| layer | may include |
|---|---|
| `model/` | `model/`, JUCE, std |
| `io/` | `io/`, `model/`, `render/`, JUCE, std |
| `render/` | `render/`, `model/`, JUCE, std — except the **render core** (`render/snapshot/`, `TempoMap`, `Kernel`, `NoteTracker`, headers and sources alike), which includes only std and other render-core files |
| `engine/` | `engine/`, the render core, JUCE, std — never `model/`, never any other `render/` file |
| `views/` | `views/`, `model/`, `app/TransportFacade.h`, JUCE, std |
| `app/` | anything |

The render core is std-only so that the engine's dependency on it is transitively clean by construction: `check_layers.py` checks the core's own includes, not paths through them. The core therefore never reads the document itself — `SnapshotBuilder` turns the Timeline into TempoMap data and clips into templates and chase tables on the message thread, and `io/` does the same for export.

`TransportFacade.h` must not transitively include any other `engine/` header; it exposes an interface class and plain types. Include graphs cannot prove thread behaviour, so the engine additionally asserts in debug builds that it is never called on the message thread with a `ValueTree` in scope — in practice: the engine simply cannot name the type.

### Threads

**Message thread** owns the ValueTree. All edits, all undo, all file I/O happen here. After any change to the undoable subtree, a rebuild produces a new Snapshot and hands it to the engine: debounced at 30 ms, with a hard ceiling of 100 ms between document change and published snapshot during continuous edits, so a knob drag never starves playback of updates. Changes under `UiState` never trigger a rebuild. Three things bypass the Snapshot and go to the engine as commands the moment they change: `Track.mute` and `Track.solo` (an `app/` ValueTree listener forwards every change, including those made by undo and redo, as a live command keyed by track id; the Snapshot carries no mute/solo state), the transport loop (`UiState.loopStart/loopEnd/loopEnabled`, forwarded by the settings path), and live knob values. Export reads mute/solo from the document and passes the audible-track set to the Kernel exactly as the engine passes its live state. Rendering ClipTemplates for large event clips may move to a background thread later; the ValueTree stays on the message thread regardless.

**Engine thread** runs the clock, drains MIDI in, runs the Kernel per look-ahead window, schedules MIDI out, sends MIDI clock. It sees:
- the current Snapshot as a raw pointer plus generation number, received through the command queue;
- a lock-free SPSC command queue from the message thread (transport including loop points, slot launch/stop with generation, mute/solo keyed by track id, live knob values, snapshot publish);
- one lock-free SPSC FIFO per MIDI input, filled on CoreMIDI's thread, drained here;
- an SPSC event FIFO back to the message thread: recorded input events (already converted to ticks), playhead, snapshot acknowledgements, overflow counters.

**Rules on the engine thread:** no allocation, no deallocation, no locks, no ValueTree, no logging, no JUCE message-thread APIs. `std::atomic<std::shared_ptr>` is not used; its lock-freedom is implementation-defined and in libc++ it isn't.

**Snapshot lifetime.** The message thread owns every Snapshot. It publishes generation *n* through the command queue; the engine switches at the next window boundary and acknowledges *n* through the event FIFO; the message thread retires every snapshot older than the last acknowledged one. On shutdown the engine is stopped and joined before any snapshot is released. A snapshot switch does not interrupt sounding notes: the NoteTracker holds pending note-offs by physical destination, independent of which snapshot produced the note-on. Events already handed to the output for the current window are committed and never recalled.

**MIDI input** arrives on CoreMIDI's thread via `MidiInputCallback`. Each input pushes into its own FIFO with the host timestamp; nothing else happens there. SysEx payloads go into a per-input byte ring (64 KB); a message over 4 KB is dropped and counted. FIFO overflow drops the event, increments a counter visible on the message thread, and marks any running take incomplete. Capacities are fixed at engine start; nothing resizes while producers run.

### Timing

Milestone 0 builds `tools/miditiming` and measures, on Roland's actual interface and Mac, with a loopback cable: scheduler wake-up jitter, output lateness relative to intended time, drift over five minutes, and drop count, under dense sixteenths on four tracks with CC traffic, with the GUI being dragged and a save in progress. Reported as median, p99, max. **Targets (default — confirm):** p99 lateness ≤ 1 ms, max ≤ 2 ms, zero drops. If `HighResolutionTimer` + `MidiOutput::sendMessageNow` can't hit them, the engine sends CoreMIDI packets with future host-time timestamps through a small shim in `engine/`. `MidiOutput::sendBlockOfMessages` is not assumed to schedule anything; whatever is used is measured on the pinned JUCE.

`TempoMap` converts host monotonic time ↔ ticks through the Timeline. The engine's clock is anchored by a (host time, tick) pair set at play start and at every seek; a tempo change during playback re-anchors at the current tick, so the tick count never jumps. Recorded input timestamps are converted to ticks once, on the engine thread as the input FIFOs are drained, against the current anchor, rounded to the nearest tick — a tempo edit in the middle of a take therefore puts each event where the playhead was when it arrived. The event FIFO carries ticks, never host times. The clock emits `0xF8` every 40 ticks.

**MIDI clock out** per physical port that has any `sendClock` Player: clock pulses run continuously while the app has the port, so synth LFOs and arpeggiators stay locked while stopped **(default — confirm)**; Start on play from 0, Continue on play from elsewhere (preceded by Song Position Pointer), Stop on stop. The app is clock master only; external sync is a non-goal.

### Session and transport state

- One active source per track: a launched slot replaces that track's arrangement playback until "back to arrangement" (per track, and one global button). Automation lanes keep playing either way.
- Launching a scene launches every track's slot in that scene; an empty slot stops the track. Retriggering a playing slot restarts it at the next launch boundary.
- Launch quantise is a fixed tick grid from tick 0 (`Session.launchQuantise`), applied to launch, stop and retrigger; 0 = immediate.
- A launched clip starts at phase 0 at the launch tick and free-runs.
- Seek, loop wrap and stop: no note chase (notes that would be sounding are not restarted); pending note-offs are sent; controllers are chased — for every track and CC with a known value before the target tick (clip CC, or automation if an owning placement covers it) the engine sends that value before the first event of the new position; program changes are chased the same way. Chase reads the Snapshot's chase tables (*Rendering rules → Chase*); it is bounded by the number of placements and allocates nothing, so it runs on the engine thread at the seek. Stop also sends all pending note-offs; panic sends All Notes Off and All Sound Off on every used channel.
- Live mute/solo/knob commands carry no snapshot generation and apply immediately; a later snapshot never undoes them — it cannot, the Snapshot carries no mute/solo state. Undoing a mute arrives as one more live command through the same listener. Slot launch commands carry the generation they were issued against and are dropped if stale.
- Unplugging a bound port: pending note-offs for that destination are discarded, the Player shows "unassigned", playback continues elsewhere. Re-plugging re-binds by identifier and sends nothing retroactively.

## Editing semantics

- Clips are **shared by reference** by default. Refcount gate on every mutation of a clip with refcount > 1. Policy comes from a persistent toolbar toggle (`UiState.sharedEditPolicy`), not a modal per edit. Clips with refcount > 1 show a badge (`×3`) in every view. A clip with refcount 1 is simply edited; undo is the safety net.
- **Make unique** acts on the edit context. The view creates one `EditContext` at gesture start, right after `beginNewTransaction`, and passes it to every clip-mutating command of the gesture. The first command that mutates a clip with refcount > 1 under `makeUnique` clones the clip (new UUIDs for the clip and every descendant with an id), redirects exactly the references in the context to the clone, records the clone's id in the context, then applies; every later command in the gesture finds that id and targets the clone. The context dies with the gesture; handing one to a later gesture is a programming error and asserts. A multi-selection of references gets one shared clone. Pool-level editing with no reference selected is "update all" by definition. The original clip stays in the pool (refcount may drop to 0; "remove unused clips" cleans up).
- **Split** defaults to splitting the *placement*: two placements, same clip, the second with an offset. The pool is untouched and playback is unchanged, held notes included. "Crop to new clip" is the explicit command that creates a new clip from a placement's window: notes starting inside the window are kept with their full length, notes starting before it are dropped, events are rebased; `loopLength` = window length.
- **Resize** repeats or trims the loop. No time-stretch exists.
- **Loop-length changes** (step count, skip, reset, step length, event-clip loop length) renormalise every referencing placement's offset in the same transaction. A change that would leave no audible column is rejected. How a `stepCount` change treats cells, `skipMask` and `resetAt` is in `docs/SCHEMA.md → Grid`.
- **StepClip → EventClip** is an explicit, one-way conversion, subject to the gate. **EventClip → StepClip** is an explicit lossy import (quantise to the grid, one pitch per cell per lane) with a preview of what is dropped. Conversion creates a new clip; under "update all" every reference of the old clip is redirected to it, under "make unique" only the context's references. The old clip stays in the pool.
- StepClips are editable only in the step sequencer and drum views; EventClips only in the piano roll. Everything created in the step sequencer and drum views is a StepClip.
- **StepClip playback:** off cells are rests (time passes, their CC values are still sent); skipped columns take no time; `resetAt` is the loop end. Cells past `resetAt` and in skipped columns keep their data.
- **Quantisation:** `Clip.quantise` on an EventClip quantises note onsets at playback and export without touching the stored ticks; `applyQuantise` makes it permanent (gate applies). Both are shown in the piano roll and arrangement. The grid is measured from clip tick 0, never from the arrangement; the exact candidate-line and wrap rule is in `docs/SCHEMA.md → ClipPool / Clip`. It is applied once, when the ClipTemplate is built, and every later rendering rule sees only the quantised onset.
- **Session recording** produces a Take: one EventClip per source track that emitted output, captured after session selection, automation merge, mute/solo and live knobs, before physical-port availability. Takes land on new take tracks that copy the source's Player, channel and name, placed at the record start **(default — confirm)**. Clock and transport messages are not part of a take. The take's own playback is never captured.
- **Recording in general** (details written before milestone 6): takes are buffered on the message thread and committed at record stop as one undo transaction, never per incoming event; held notes get note-offs at the stop tick; looped recording overdubs into the same take **(default — confirm)**; a knob recorded into a StepClip stores, per column, the knob's value at that column's start **(default — confirm)**; a knob move always reaches hardware immediately, recording or not; unmatched note-offs are dropped and reported.
- **Knobs:** `Track.Knobs` assigns CC numbers. In clip views a knob move records into the currently playing clip; in the Automation Recorder it records into an AutomationLane placement. With nothing playing a knob only controls hardware.
- **PadMap** is input configuration under `Routing`, undoable. Remapping a pad never rewrites clips. An incoming note on the pad Performer matches a pad by `inputPitch`.

## Rendering rules (document → MIDI)

Implemented once in `src/render/`, used by the engine Snapshot and by SMF export. Tested exhaustively — see *Guardrails → 4. Render golden files*.

**Pipeline.** `model → ClipTemplate` (per clip, one loop period, quantisation applied, step grids expanded, chase tables derived; message thread) → `Snapshot` (templates + per-track placement plans + automation ownership bitsets + TempoMap) → `Kernel(snapshot, audibleTracks, window)` (loop arithmetic, placement merging, automation merge; engine or export) → `NoteTracker` (same-pitch ownership, pending note-offs per physical destination) → output. `audibleTracks` is the mute/solo result: the engine's live state, or the document's values at export. Export runs Kernel + NoteTracker over `[0, exportLength)` with a one-shot in-memory output.

**Per track.** Adjacent placements of the same clip whose phase is continuous (`next.offset == (prev.offset + prev.length) mod loopLength`, `next.start == prev.start + prev.length`) are merged into one before rendering. For each (merged) placement, loop the clip within `[start, start + length)` applying `offset`; emit enabled notes and events; channel = the Track's channel. Rendering ignores `playerId`.

**Note windows.** A note is emitted if its onset falls inside the placement window; a note whose onset precedes the window (before `offset`, or in a previous iteration) is not chased. A note plays its full length across the clip's loop boundary. Placement end truncates: note-off at `start + length`. Content at `tick ≥ loopLength` is never played.

**Equal-tick order** on one track: note-offs falling due, then non-note events in child order, then note-ons in child order. Within the non-note group nothing is reordered — an imported bank-select → program change sequence stays as imported. Across tracks that share a destination, tracks are processed in child order. The golden format records this order; it is not re-sorted.

**StepClip:** iterate columns in order, skip columns with the skip flag, stop at `resetAt`. Each column's `CcValue`s are emitted at the column start, on and off cells alike, before any note-on of that column. Note length = `round(gate × stepLength)` ticks, ≥ 1, in audible time. A lane with `fixedPitch` ignores cell pitches. **Consecutive on-cells in one lane with the same pitch, where the earlier gate reaches the later cell's start, merge into one note** ending where the later note would have ended (tie); a gate that doesn't reach the next cell retriggers. Overlap between *different* pitches is plain overlap (legato on a monosynth). **The merge never crosses the loop point** **(default — confirm)**: a ClipTemplate is one loop period, so the last audible column's gate runs into the next iteration as plain overlap under the note-window rule, and when the first column has the same pitch the same-pitch rule retriggers it (note-off immediately before the note-on). A lane of equal pitches with long gates therefore retriggers once per loop instead of droning. Tying across the loop point would be a renderer change (a "continues" flag on the last note, suppression of the first note-on on later iterations, offset handling), not a schema change, if it is ever wanted.

**Same-pitch overlap** on one physical destination (port, channel, pitch), any number of tracks: the pending note-off of the earlier note is emitted immediately before the new note-on, and the earlier note's original note-off is cancelled. No stuck notes, no double note-ons, ever. Export applies the same tracker, so the file never contains an overlapping same-pitch pair either.

**AutomationLane merge:** a lane placement owns the set of CC numbers that occur as enabled `cc` events with `tick < loopLength` in its clip, over the whole placement. While owned, the track's clip CC events for those numbers are suppressed and the lane's events are emitted; later lanes win over earlier ones per CC. At placement end, and when seeking out of it, the engine sends the underlying value: the last suppressed clip CC for that number before that tick, if one exists; otherwise nothing. Seeking into an owned interval sends the lane's last value before the seek point. Live knob movements are sent regardless of ownership.

**Transport loop:** at `loopEnd` pending note-offs are sent, then controllers are chased for `loopStart` and playback continues there.

**Chase.** Each ClipTemplate carries, per CC number and for program change, its enabled events of that kind as a sorted (tick, value) list over one loop period. To chase a track at arrangement tick *t*, walk its placements backwards from *t*: inside a placement, "last value before phase *p*" is a binary search in the table; an earlier complete iteration contributes the table's final entry; then the previous placement, and so on. Stop at the first hit per CC. Automation ownership is consulted first — an owning lane placement answers from its own clip's table, and when leaving one the underlying value is the track's own chase result at that tick. Cost is bounded by placements × log events and allocates nothing, which is why the engine may do it at a seek. The same walk serves the transport loop and automation exit; SMF export never needs it, it renders from tick 0.

**SMF export:** type 1, PPQ 960, track 0 = conductor (tempo map, time signatures, markers), one SMF track per Track in track order, track-name meta event, channel from the Track, Players irrelevant. Range `[0, exportLength)` (0 = arrangement end, defined in `docs/SCHEMA.md → UiState`); notes crossing the end get their note-off at the end. Follows the document's current mute/solo, handed to the Kernel as `audibleTracks` **(default — confirm)**. Automation lanes are merged in. No structure in meta events.

**SMF import policies:**

| case | policy |
|---|---|
| PPQ ≠ 960 | rational rescale, round to nearest tick, zero-length notes get length 1, reported |
| SMPTE time division | refused with explanation |
| type 0 | accepted, split by channel like any mixed track |
| type 2 | refused |
| mixed channels in one track | one Track per channel; channel-less SysEx goes to the lowest-numbered channel's Track, once |
| note-on velocity 0 | note-off |
| unmatched note-on / note-off | note-on without off ends at the next same-pitch on or at End-of-Track; orphan offs dropped; reported |
| overlapping same pitch | earlier note truncated at the later onset |
| End-of-Track | clip `loopLength` = EOT tick rounded up to a whole bar of the time signature at tick 0 |
| tempo / time signature / markers | merged into the Timeline from any track |
| other meta events, SMF SysEx packets/escapes | dropped and reported |
| Players | left unassigned (rendering unaffected) |

Imported content becomes one EventClip per Track, placed at tick 0 with `length = loopLength`.

## Guardrails

Roland is the only maintainer and will not read every line. Structure therefore has to be enforced by the test run, not by review. These checks exist so that drift fails loudly while nobody is watching. They land in milestone 1, before there is anything to protect, and run on every `ctest`. Every script has a fixture in `tools/fixtures/` that must make it fail; that negative run is itself a test.

### 1. Schema names live only in `Ids.h`

`tools/check_identifiers.py` parses `src/model/Ids.h`, collects every string literal used to construct a `juce::Identifier`, and scans the rest of `src/` for those literals **inside Identifier construction and property/child APIs** (`juce::Identifier (`, `getProperty (`, `setProperty (`, `getChildWithName (`, `hasType (`, `ValueTree (` and friends). A bare word like `name` in a comment or a UI label is not a hit. Any hit outside `Ids.h` fails the check with `file:line`.

Allowed exceptions, and only these: `src/io/Migrations.cpp`, which by definition must name properties that no longer exist, and `tests/io/fixtures/`. Both are listed explicitly in the script — not matched by a wildcard. View-state identifiers under `UiState` are not an exception; they live in `Ids.h` too.

### 2. The layer rules are checked, not just documented

`tools/check_layers.py` walks the `#include` lines under `src/`, resolves quoted relative paths, and enforces the table under *Layers and dependencies*, including the render-core std-only rule (`render/snapshot/`, `TempoMap`, `Kernel`, `NoteTracker` include only std and each other, so the engine's dependency on them is transitively clean without a transitive check) and the transitive check that `app/TransportFacade.h` pulls in no other `engine/` header. `views/` including any other `engine/` header, `engine/` including anything from `model/` or from `render/` outside the core, and a render-core file including JUCE, are the failures this check exists for.

### 3. Invariants as property tests, not examples

`tests/guardrails/` holds a small generator that builds random *valid* documents (a few tracks, clips of both kinds, placements, lanes, slots, automation lanes) and applies random sequences of commands from `model/commands/` — valid ones *and* deliberately invalid ones (overlapping placements, zero-length loops, notes into an automation clip). After every command it asserts all twelve invariants from `docs/SCHEMA.md` via the Validator, plus:

- a valid command fully applies; an invalid one leaves the undoable subtree XML-identical and the undo history unchanged;
- undo restores the undoable subtree to an XML-identical state, and redo returns to the post-command state (`UiState` excluded from the comparison);
- canonical save → load → canonical save is byte-identical;
- clip ids referenced before a command still reference a clip of the same `kind` after it, or the command was a conversion.

Seeds are printed on failure and reproducible. When the generator finds a failure, shrink it to a minimal document and add that as a named regression test.

### 4. Render golden files

Every rendering rule above gets a pair in `tests/render/golden/`: a fixture project (XML, real UUIDs) and the expected event list, plain text, one event per line, `tick track channel type a b [data]`, **in emission order, not re-sorted**, SysEx with the full payload. The render test runs Kernel + NoteTracker over the fixture's full range and diffs against the file. Expected files are reasoned out by hand from the rules, never produced by the renderer under test on first creation.

Required cases, at minimum: skipped column; `resetAt` shorter than `stepCount`; `gate > 1.0` into a different pitch; `gate > 1.0` into the same pitch (merge); gate reaching into a skipped column; gate across the loop point into a different pitch, and into the same pitch (retrigger, not merge); `fixedPitch` lane ignoring cell pitch; off-cell CC values; placement `offset`; placement shorter and longer than the clip loop; two placements of one clip restarting (offset 0) and phase-continuous (merged); split during a held note; same-pitch overlap on one track and across two tracks on one destination; bank-select → program change → note at one tick; note-off and note-on of one pitch at one tick; AutomationLane overriding, passing through, and restoring the underlying value at exit; a lane whose first event comes after its placement start; two lanes overriding the same CC; event-clip note crossing `loopLength`; content past `loopLength` silent; `quantise` at `1/8` and `1/8T`; `quantise` moving an onset across the loop point (wraps to 0) in a loop that is not a multiple of the grid; two notes of one pitch quantised onto one tick; transport loop wrap with a held note; chase at a seek target: value found in the current placement, in an earlier iteration of it, in an earlier placement, and no value at all; export range cutting a note; a track with no Player rendering normally.

Golden files are regenerated only through the `regenerate_golden` target, and the diff is read before it is committed. A silently regenerated golden file is worse than no test — it turns a behaviour change into a green build.

### 5. Loading is a wall

`tests/io/` loads every fixture in `fixtures/` through the real load path: valid files install; files with a higher `schemaVersion`, malformed XML, invariant violations, and a fixture whose migration throws are all refused with the current document and the source file untouched. Save failure (read-only target) leaves the previous file intact.

### Manual checks

`docs/MANUAL_CHECKS.md` is a short script run at each milestone boundary: selection, dragging, undo across tabs, focus and keyboard shortcuts, tab switching during playback, device unplug/replug during playback, sleep/wake, close with unsaved changes, recovery-file offer. Not automated; not optional.

### External review

Optional, at milestone boundaries only, never per commit. Give the reviewer a read-only checkout, `docs/SCHEMA.md`, this file, and a fixed question list; do not tell it what the code is supposed to do beyond those documents, and never say the code is believed correct — a reviewer primed that way stops finding things. Ask for a ranked, capped list. Findings that matter become guardrail tests; the rest is discarded, not archived.

## Milestones

Each milestone names a demonstration and its pass criterion. Do not start UI for a later milestone early.

0. **Timing spike and toolchain.** Pin JUCE, Catch2, Xcode, CMake, deployment target. Build `tools/miditiming`. Demo: a loopback report on Roland's interface. Pass: targets under *Timing* met or renegotiated with numbers in hand. Decides timer + send path for the engine.
1. **Validated model.** Schema ids, typed wrappers, commands with edit context, Validator, canonical XML writer, atomic save, migrations skeleton, guardrails 1, 2, 3, 5 with negative fixtures. Demo: `ctest` green, property generator through 10,000 seeds. Pass: all guardrails present and known to fail on their fixtures.
2. **Render kernel and interchange.** ClipTemplate, Snapshot, Kernel, NoteTracker, TempoMap; guardrail 4 with the full case list; SMF import/export with the policy table. Demo: import a .mid, export it, show the musical diff is empty and the loss report lists what was dropped. Pass: all goldens green.
3. **First musical slice.** App shell, Routing tab, engine (transport, snapshot handoff, FIFOs, clock out), minimal arrangement display. Demo: open the Berlin fixture or an imported file, play it to hardware, stop, seek, loop, unplug and re-plug the interface, save, quit, reopen. Pass: no stuck notes, timing within targets, recovery file works.
4. **Step sequencer and drum view** (first editor **— default — confirm**), with enough arrangement to place a clip per track and press play. Demo: build the 5-step bass against 16-step drums from scratch, free-running, knob values recorded into cells. Pass: everything in SCHEMA's example reproducible by hand.
5. **Arrangement and session.** Placements (move, resize, split, crop), slots and scenes, launch rules, refcount gate with badge and make-unique, session recording to take tracks. Demo: jam a scene, record the take, play the take back against the original. Pass: take replays each destination identically.
6. **Piano roll and recording.** EventClip editing, recording from a Performer, quantise and `applyQuantise`, both conversions with loss preview. Demo: record a line, quantise it non-destructively, convert a step clip and edit it.
7. **AutomationLanes and Automation Recorder.** Ownership, exit restoration, lane priority, punch-in/out recording. Demo: record a filter sweep over a looping clip, seek into and out of it, export and verify the CC stream.

## Conventions

- One class per file, file name = class name. One namespace per layer (`model`, `io`, `render`, `engine`, `views`).
- ValueTree type and property identifiers live in `model/Ids.h` as `juce::Identifier` constants. Nowhere else is a type or property name spelled as a string. Enforced by guardrail 1.
- Typed wrapper classes over ValueTree nodes (`ClipModel`, `TrackModel`, …) are how views *read* the document. Views never call `setProperty`.
- Mutations are free functions in `model/commands/`, taking `UndoManager&` and, for clip content, an `EditContext&`. Commands validate their preconditions before touching the tree and never half-apply. Commands never call `beginNewTransaction`; the view owning the gesture does, once, at gesture start, and creates the gesture's `EditContext` right after it.
- Two deliberate exceptions to the command path: `UiState` is written through `model/Settings` with a `nullptr` UndoManager, and loaded/migrated trees are constructed by `io/` and installed whole.
- Comments say *why*. Names say *what*.
- `juce::` prefix explicit. No `using namespace juce` in headers.
- Prefer `std::` types; JUCE containers only at JUCE API boundaries. Nothing from JUCE in `render/snapshot/`.
- Tests: one file per model type, one per IO format, one per rendering rule.

## Non-goals (for now)

Audio, plugins, MIDI 2.0 / MPE, notation, external clock sync (clock master only), tempo-change UI (the Timeline stores a map; v1 UI edits a single tempo), synth editor panels and a SysEx patch librarian (a later idea — the design must not prevent it: events are generic, SysEx is an Event kind), internal modulators such as an arpeggiator or chord player (later idea — they would sit as a per-track Transform between Kernel output and NoteTracker, so the Kernel's output stays a plain event stream and nothing in the document assumes otherwise; because the engine runs the Kernel per look-ahead window, a Transform there must carry its state across windows, allocate nothing, reset its state on seek, loop wrap and stop, and seed any randomness from the document so export and goldens stay deterministic; adding one means adding its header to the `engine/` row of the layer table and to `check_layers.py` on purpose; a later "freeze" command would render Kernel + Transform output back into a new EventClip, which "crop to new clip" does not do because it reads the document, not Kernel output), live MIDI thru and input effects — Performer → Player monitoring, input quantisation, live arpeggiation of what is being played (the document never sees these; the engine has no input → output path, only input → recording; the Transform slot above is render-side and does not cover them), Linux, Windows, scripting.

## Open questions

- Session layout: Ableton grid (tracks × scenes) or tracker-style rows? `Slots` + `Scenes` support the grid; UI undecided.
- CC envelopes: store discrete events only (current decision; a line tool writes many events) or breakpoints rendered at export?
- Performer channel filter: the schema has it; the UI may hide it.
- Whether tempo changes get any UI in v1.
- Guardrail 3: handwritten generator or a property-testing library (rapidcheck)? Start handwritten; revisit if the generator grows past a file.
- Launch quantise: fixed tick grid is v1. Meter-aware bars if varying meters ever matter.
- Everything marked **(default — confirm)** above: clock while stopped, timing targets, take tracks, overdub on looped recording, knob-to-step sampling, export honouring mute/solo, step sequencer as first editor.
