# CLAUDE.md — Hums, the Humane MIDI Sequencer

**Name.** The software is **Hums**, short for **The Humane MIDI Sequencer**. This entry is authoritative for the name. When Roland says "Hums" he means this project, the app built from this repository. Use "Hums" in conversation, docs, the app bundle and window title. "Step sequencer" always means the Step Sequencer tab or a 960-style StepClip, never the app as a whole. Don't call the app "the sequencer". No other file may rename it. `README.md` repeats the name for readers and must stay consistent with this entry.

**Authority.** This file and the behaviour docs in `docs/` (`ARCHITECTURE.md`, `RENDERING.md`, `EDITING.md`, `GUARDRAILS.md`) are authoritative for behaviour and architecture; `docs/SCHEMA.md` is authoritative for the document model; `docs/DECISIONS.md` is authoritative for the status of every default and open question; this file alone is authoritative for the project's name (see *Name*). If any of them disagree, stop and ask. If a change you are about to make would deviate from this file, `docs/SCHEMA.md` or any doc listed under *Where things live*, stop immediately and ask before writing it. If Roland agrees, edit the doc in the same commit as the code (and, for a [Dn] item, its status in `docs/DECISIONS.md`).

**Defaults.** A **[Dn]** tag marks a decision taken by proposal that still needs Roland's explicit yes. Its status lives only in `docs/DECISIONS.md`. A pending default is binding until he answers; never re-ask one that is decided; when he answers, update its status there in the same session.

Revised 3 Oct 2026 after an external design review; quantisation contract, Transform slot and live-thru non-goal tightened the same day; later the same day: mute/solo and transport-loop command paths, the std-only render core, `EditContext` as a gesture object, retrigger at the step-clip loop point, chase tables, recorded input converted to ticks on the engine. Split 4 Oct 2026 into this file plus path-scoped docs (`.claude/rules/` loads them when matching files are touched); the `(default — confirm)` markers became [Dn] tags tracked in `docs/DECISIONS.md`. Later on 4 Oct 2026: timing measurement split across M0 (software vs hardware, tester load) and M3 (real-app load, listen mode); rule 13 (JUCE source over guessing) and the doc-deviation rule. Named *Hums* on 4 Oct 2026; the working-title note was retired.

## Where things live

| doc | what | read it when |
|---|---|---|
| `docs/SCHEMA.md` | ValueTree schema, invariants 1–12, load/save, canonical form, migrations | before touching `src/model/`, `src/io/`, or any fixture (auto-loaded there) |
| `docs/EDITING.md` | refcount gate, make-unique, split, resize, conversions, quantisation, recording, knobs | before touching `src/model/` or `src/views/` (auto-loaded) |
| `docs/RENDERING.md` | pipeline, note windows, equal-tick order, step clips, same-pitch, automation merge, chase, SMF export/import | before touching `src/render/`, SMF I/O, render tests (auto-loaded) |
| `docs/ARCHITECTURE.md` | threads, snapshot lifetime, MIDI input, timing, clock out, session/transport state | before touching `src/engine/`, `TransportFacade.h`, `tools/miditiming` (auto-loaded) |
| `docs/GUARDRAILS.md` | full contract of the five guardrails, golden case list, manual checks, external review | before touching `tools/`, `tests/guardrails/`, golden files (auto-loaded) |
| `docs/MILESTONES.md` | milestones 0–7 with demo and pass criteria | when starting, planning or closing a milestone |
| `docs/DECISIONS.md` | status of every [Dn] default and open question | when a task depends on one; when Roland answers one |

When a task spans areas, read every doc whose area it touches, not only the ones auto-loaded so far.

## What this is

Hums is a MIDI-only sequencer for macOS, built with JUCE. It implements one person's (Roland's) personal workflow for sequencing analog hardware synths over MIDI. No audio, no plugins. It has to be reliable enough to be used in real music-making for years, so maintainability beats cleverness and feature count.

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
13. **Never guess at a JUCE API.** When unsure of a class, signature, threading guarantee or behaviour, read the source under `libs/JUCE/modules` at the pinned tag; it is the authority, not memory, web docs or current master. If `libs/JUCE` is not checked out, say so and stop rather than guess.

## Toolchain

- JUCE 8.x, pinned to an exact release tag (recorded in `libs/JUCE_VERSION`), as a git submodule in `libs/JUCE`. Built via JUCE's CMake API. No Projucer. Current master docs are reference, never a reason to move the pin; for API questions read `libs/JUCE/modules` (rule 13).
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
- Timing check against the app (M3+): `build/tools/miditiming --listen --in "<port>" --reference <export.mid>`
- Run: `open build/Hums_artefacts/Debug/Hums.app`

## Git and done

- One branch per milestone (`m0-timing`, `m1-model`, …), merged into `main` when the milestone's pass criteria in `docs/MILESTONES.md` are met. One commit per task. Never force-push, never rewrite pushed history.
- Roland reviews through the tests: `git log -p -- tests/` shows every change to the contract. A commit that changes behaviour without touching `tests/` is suspect. A commit that changes a golden file or a guardrail fixture needs its reason in the message.
- Claude may not say a task is "done" until it has pasted the final `ctest` summary line (e.g. `100% tests passed, 0 tests failed out of N`) from a run against that commit. If any test fails, Claude pastes the failure output instead.

## Repository layout

```
CMakeLists.txt
CLAUDE.md
TOOLCHAIN.md           pinned versions, written at milestone 0
.claude/rules/         path-scoped pointers that load the docs below when matching files are touched
docs/
  SCHEMA.md            ValueTree schema (authoritative)
  EDITING.md           editing semantics
  RENDERING.md         rendering rules, SMF export/import
  ARCHITECTURE.md      threads, timing, transport and session state
  GUARDRAILS.md        guardrail contract, golden case list
  MILESTONES.md        milestone plan
  DECISIONS.md         status of defaults and open questions
  MANUAL_CHECKS.md     GUI interaction checklist, run at milestone boundaries
libs/
  JUCE/                submodule, pinned tag
tools/
  check_identifiers.py Ids.h is the only place schema names appear in property APIs
  check_layers.py      include graph obeys the layer rules
  fixtures/            one deliberately violating file per check
  miditiming/          loopback timing measurement, send and listen modes (JUCE console app, kept)
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

Thread model, snapshot handoff, timing and transport behaviour: `docs/ARCHITECTURE.md`. Three things never travel in the Snapshot — mute/solo, the transport loop, live knob values; they reach the engine as live commands.

## Guardrails

Roland is the only maintainer and will not read every line, so structure is enforced by the test run. Full contract in `docs/GUARDRAILS.md`. In short:

1. **Schema names live only in `Ids.h`** — `tools/check_identifiers.py`; exceptions are listed explicitly in the script, never wildcarded.
2. **Layer rules** — `tools/check_layers.py` enforces the table above, including the std-only render core and `TransportFacade.h`.
3. **Invariants as property tests** — random valid documents × random valid and invalid commands; Validator, undo/redo XML identity, canonical save→load→save byte identity. Seeds printed; failures shrunk into named regression tests.
4. **Render golden files** — one fixture + hand-reasoned expected event list per rendering rule, in emission order. Regenerated only via the `regenerate_golden` target, and the diff is read before committing.
5. **Loading is a wall** — every malformed, too-new or invalid fixture is refused with the current document and source file untouched.

Every script ships a fixture that must fail it. Never weaken, skip or `// NOLINT` a guardrail to make a feature land (rule 12). Manual checks: `docs/MANUAL_CHECKS.md`, at milestone boundaries.

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

Audio, plugins, MIDI 2.0 / MPE, notation, external clock sync (clock master only), tempo-change UI (the Timeline stores a map; v1 UI edits a single tempo), synth editor panels and a SysEx patch librarian (a later idea — the design must not prevent it: events are generic, SysEx is an Event kind), internal modulators such as an arpeggiator or chord player (later idea — reserved as a per-track Transform; constraints in `docs/RENDERING.md → Transform slot`), live MIDI thru and input effects — Performer → Player monitoring, input quantisation, live arpeggiation of what is being played (the document never sees these; the engine has no input → output path, only input → recording), Linux, Windows, scripting.
