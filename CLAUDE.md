# CLAUDE.md — Hums (The Humane MIDI Sequencer) 

**This file is authoritative for behaviour and architecture; `docs/SCHEMA.md` is authoritative for the document model.** Read both before touching anything under `src/`. If they disagree, stop and ask.

Revised 3 Oct 2026 after an external design review; quantisation contract, Transform slot and live-thru non-goal tightened the same day; later the same day: mute/solo and transport-loop command paths, the std-only render core, `EditContext` as a gesture object, retrigger at the step-clip loop point, chase tables, recorded input converted to ticks on the engine.

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
- **Session recording** produces a Take: one EventClip per source track that emitted output, captured after session selection, automation merge, mute/solo and live knobs, before physical-port availability. Takes land on new take tracks that copy the source's Player, channel and name, placed at the record start. Clock and transport messages are not part of a take. The take's own playback is never captured.
- **Recording in general** (details written before milestone 6): takes are buffered on the message thread and committed at record stop as one undo transaction, never per incoming event; held notes get note-offs at the stop tick; looped recording overdubs into the same take; a knob recorded into a StepClip stores, per column, the knob's value at that column's start; a knob move always reaches hardware immediately, recording or not; unmatched note-offs are dropped and reported.
- **Knobs:** `Track.Knobs` assigns CC numbers. In clip views a knob move records into the currently playing clip; in the Automation Recorder it records into an AutomationLane placement. With nothing playing a knob only controls hardware.
- **PadMap** is input configuration under `Routing`, undoable. Remapping a pad never rewrites clips. An incoming note on the pad Performer matches a pad by `inputPitch`.

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
- When unsure about a JUCE API, read libs/JUCE/modules; never guess.
- If you want to deviate from SCHEMA.md or CLAUDE.md, stop and ask; if agreed, edit the doc in the same commit.
