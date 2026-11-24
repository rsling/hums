# CLAUDE.md — Hums, the Humane MIDI Sequencer

**Name.** The software is **Hums**, short for **The Humane MIDI Sequencer**. This entry is authoritative for the name. When Roland says "Hums" he means this project, the app built from this repository. Use "Hums" in conversation, docs, the app bundle and window title. "Step sequencer" always means the Step Sequencer tab or a 960-style StepClip, never the app as a whole. Don't call the app "the sequencer". No other file may rename it. `README.md` repeats the name for readers and must stay consistent with this entry.

**Authority.** This file and the behaviour docs in `docs/` (`ARCHITECTURE.md`, `RENDERING.md`, `EDITING.md`, `GUARDRAILS.md`) are authoritative for behaviour and architecture; `docs/SCHEMA.md` is authoritative for the document model; `docs/DECISIONS.md` is authoritative for the status of every default and open question; this file alone is authoritative for the project's name (see *Name*). If any of them disagree, stop and ask. If a change you are about to make would deviate from this file, `docs/SCHEMA.md` or any doc listed under *Where things live*, stop immediately and ask before writing it. If Roland agrees, edit the doc in the same commit as the code (and, for a [Dn] item, its status in `docs/DECISIONS.md`).

**Defaults.** A **[Dn]** tag marks a decision taken by proposal that still needs Roland's explicit yes. Its status lives only in `docs/DECISIONS.md`. A pending default is binding until he answers; never re-ask one that is decided; when he answers, update its status there in the same session.

Revised 3 Oct 2026 after an external design review; quantisation contract, Transform slot and live-thru non-goal tightened the same day; later the same day: mute/solo and transport-loop command paths, the std-only render core, `EditContext` as a gesture object, retrigger at the step-clip loop point, chase tables, recorded input converted to ticks on the engine. Split 4 Oct 2026 into this file plus path-scoped docs (`.claude/rules/` loads them when matching files are touched); the `(default — confirm)` markers became [Dn] tags tracked in `docs/DECISIONS.md`. Later on 4 Oct 2026: timing measurement split across M0 (software vs hardware, tester load) and M3 (real-app load, listen mode); rule 13 (JUCE source over guessing) and the doc-deviation rule. Named *Hums* on 4 Oct 2026; the working-title note was retired. Revised 18 Nov 2026: platform decision — macOS-only build, portable by construction (`platform/` layer, guardrail 2 extended to OS headers); iPad assessed and rejected. Revised 19 Nov 2026: language decision — C++ stays, Rust assessed and rejected; sanitizer builds and the three-configuration gate added. Revised 20 Nov 2026: Linux and Windows both "possible later"; JUCE pinned to 9.0.6 for the v1 cycle; `TOOLCHAIN.md` started. Later on 20 Nov 2026, after the milestone 0 code review: licence changed to AGPLv3; `TempoMap` realises tempo as an SMF holds it [D13]; JUCE assertion and leak reports fail a test, the canaries notice a lost `-fno-sanitize-recover`, the gate builds `miditiming`; a port is open at most once and a vanished port refuses sends; `hums_io`'s dependency on `hums_render` postponed to milestone 2. Revised 21 Nov 2026: JUCE pin check (`tools/check_juce_pin.py`); guardrail scripts run with the pinned Xcode's Python; guardrail 2 (`tools/check_layers.py`) landed and also refuses what it cannot assign to a layer; guardrail 1 (`tools/check_identifiers.py`) landed with `model/Ids.h` and `model/PropertyReader`, and fixes the form of a declaration in `Ids.h`; a double in the document holds six significant digits, `bpm` three decimals [D14]; invented type and property names are caught by guardrail 1 in `model/`, `io/`, `render/` and by guardrail 3 at runtime [D15]; `Scene.name` and `Marker.name` default to empty [D16]; a hand-edited project file with an error is refused, never repaired, and the Validator checks `UiState`'s own properties, the tab names included, and refuses a double with more than six significant digits [D17]; guardrail 1 widened after a code review to every place a `juce::Identifier` is expected — a function of ours that takes one (read from its declaration), a return value, a list — and to XML tag and attribute names, and narrowed in JUCE's own calls to the argument that is the name, so that a value equal to a schema name (`kind` = `cc`) passes; the body of a lambda with a trailing `ValueTree` or `Identifier` return type is no longer read as a construction, and a list in braces returned as a `ValueTree` is; guardrail 2 refuses a header of ours in angle brackets, which `platform/mac/` had let pass for an OS header, and any file under `src/` it would not read. Revised 22 Nov 2026: guardrail 1 takes a subscript only where its bracket is code, not text in a string literal, and the scripts' shared reader no longer takes the `'` of a digit separator (`1'000`) for the start of a character literal; the canonical writer landed (`io/ProjectFile`), with `model/DeclaredIdentifiers` as the list that gives it the order of `Ids.h`; the load sequence landed (`io/ProjectFile::load`), with `io/Migrations` as the still empty table of steps; the atomic save landed (`io/ProjectFile::save`); guardrail 5 landed (the fixtures under `tests/io/fixtures/`, those that must be refused under `refused/`); the generator of guardrail 3 landed (`tests/guardrails/DocumentGenerator`, seeded; `DocumentChecks` holds every generated document to the Validator, to the names of `Ids.h` and to save → load → save), without commands yet; the first commands landed (`model/commands/`: Routing, PadMap, Timeline, Session), with `model/commands/CommandResult` as what a command reports and `model/CanonicalDouble` as the one place a double is rounded: a command stores a double as its saved text [D14]; removing a Player or Performer in use unassigns its tracks in the same undo step [D18]; guardrail 3 now runs random valid and invalid commands on every generated document (`tests/guardrails/CommandDriver`, `CommandChecks`); the Track commands landed (`model/commands/TrackCommands`: tracks, their properties, mute/solo, knobs); removing a Track leaves the clips it referenced in the pool; the clip pool commands landed (`model/commands/ClipPoolCommands`: a clip of either kind made, its name and colour, a clip removed only at refcount 0, remove unused clips), with `model/ColourText` as the one place that says what a colour looks like; the placement commands landed (`model/commands/PlacementCommands`: a clip placed on a track, a placement moved along its track or lane or to another track, resized, split with the offset of the second half, removed), and none lets two placements of one track or lane share a tick; the slot and scene commands landed (`model/commands/SlotCommands`: a clip put into a slot and taken out, a scene inserted, removed and moved, each renumbering every track's slots and the scene names in one undo step), and the AutomationLane commands (`model/commands/AutomationLaneCommands`: a lane made, named, moved among its track's lanes and removed with its placements; `addLanePlacement` with the placement commands), with `model/AutomationClipRule` as the one place that says which clips a lane takes (invariant 9) and `model/commands/SiblingOrder` as how a Track, a Knob or a lane is moved among its kind; the edit context and the refcount gate landed (`model/commands/EditContext`, `model/commands/RefcountGate`), and a clip's name and colour are the first changes that pass the gate; the EventClip content commands landed (`model/commands/EventClipCommands`: a note and an event added, changed and removed, each named by its position among the clip's notes or events; the loop length of an event clip, which brings the offsets of the clip's placements back into the loop; the quantise setting), all through the gate, and with them the second side of invariant 9: a clip that an AutomationLane references is given nothing but cc events; the StepClip content commands landed (`model/commands/StepClipCommands`: a lane added, named, given a fixed pitch, moved and removed; a cell and its CC values; the grid's step length, step count, reset and skipped columns, each refused when no column would be left to play), all through the gate, lanes and cells named by position, with `model/commands/PlacementOffsets` as the one place that brings the offsets of a clip's placements back into a changed loop; a cell's gate is stored as its saved text [D14]; Roland's answers to the calls taken with the commands recorded [D19]; a make-unique clone is named as a copy, `Bass (copy 1)` (`model/CopyName`) [D20]; a clip on an AutomationLane is on AutomationLanes only, in the commands and in invariant 9 [D21]; a Performer and a Player have a name that no other port of the kind has (invariants 10 and 11) [D22]. Revised 23 Nov 2026, after the milestone 1 code review: a project file in which one node has an attribute twice, or plain and as `base64:`, is refused on load (`docs/SCHEMA.md → Loading`); later the same day: a project file's name does not begin with `~` — such names are reserved for files Hums handles itself, load and save refuse them (`io::ProjectFile::isNameAllowed`), and a "restore backup" command is planned for the UI (`docs/SCHEMA.md → Loading, saving, canonical form`); later again: a project file with a `Note` or a `Placement` that ends past the largest tick an `int64` holds is refused on load (invariant 8), as the commands already refused to make one; later again: a project file is refused whose closing tags do not close their own nodes or are cut short, that holds anything after its root node, that has a DOCTYPE, or whose bytes are not UTF-8 (`io/XmlWellFormedness`, `docs/SCHEMA.md → Loading`); later again: a save gives the new file the permission bits of the file it replaces and promises nothing else of it (Finder tags and comments are not sure to be kept), and a save to a symbolic link saves the file the link leads to and leaves the link (`docs/SCHEMA.md → Save`); later again: a project file with a `bpm` or a `gate` whose exponent has more than three digits is refused on load (`model/PropertyReader`, `docs/SCHEMA.md → Doubles`); a file is refused also for a malformation that looks harmless, which is meant to discourage editing project files outside Hums [D17]. Revised 24 Nov 2026: an edit context that has cloned is known as kept past its gesture whatever a later gesture has done, by a mark the refcount gate leaves in the gesture's undo transaction with the clone (`model/commands/CloneMark`); a count of the transaction's actions, which a later gesture can reach, had been all it went by (`docs/EDITING.md`); later the same day: guardrail 1 sees a name also where a static member is defined outside its class (`const juce::Identifier NoteModel::pitch ("pitch");`, and the same for a class of ours whose constructor takes an Identifier) and in a call with template arguments to a function of ours (`readAs<int> (node, "pitch")`), both of which it had let pass; later the same day: guardrail 1 sees a name also where it becomes a `juce::Identifier` through a member of a struct or class of ours — the struct filled from a list in braces (alone, in a list of them, in `push_back` and `emplace_back`, in a `return`), the member initialised by a constructor, assigned, or named in the list; the script reads the structs under `src/` as it reads the functions; later the same day: guardrail 1 reads what stands in a character literal as a character, not as code — a `'}'` no longer ended the body of a function early (a name after it passed), a `'{'` no longer lets it run on into the next function (legal code there was reported), and a `','` no longer splits an argument; later the same day: the test that shows what the property tests reach counts the commands of the property run itself (`tests/guardrails/PropertyRun`, what the run does with one seed), over the first 300 seeds in every configuration; it had counted a loop of its own, and the run itself left three commands without a make-unique clone in the 200 seeds of an instrumented configuration; later the same day: guardrail 1 sees a name also in the calls of JUCE's it had not known — `sendPropertyChangeMessage`, `setPropertyExcludingListener` (the name is its second argument), a `juce::CachedValue` or `juce::ValueTreePropertyWithDefault` constructed, referred to a property, or initialised as a member, and the tag in `getDocumentElementIfTagMatches` and `parseXMLIfTagMatches` — and in a node's type compared with a text (`getType() == "Track"`), which counts as a hit; later the same day: guardrail 1 no longer refuses two things that name nothing — a `juce::String` made from a text, once any `std::function<juce::String (const juce::Identifier&)>` stood under `src/` (the return type of a function type had been taken for a function of ours), and the default text of a parameter in a declaration that returns a `ValueTree` or an `Identifier` (the declaration had been read as a construction); later the same day: guardrail 1 sees a name also where a reference to a `juce::Identifier` is bound to a literal (`const juce::Identifier& property = "pitch";`, with `=`, brackets or braces) and in the default of a lambda's parameter that is one, both of which it had let pass; later the same day: a project file with an `Event` that has an `a`, a `b` or a `data` its kind does not use is refused on load (invariant 10), as the commands already refused to make one; the Validator had not judged them, and `setEvent` then refused such an event's own values; later the same day: a project file with a step clip that has an `Events` child, or an event clip that has a `Grid`, is refused on load (invariant 12), as no command makes such a clip; the Validator had asked a clip only for the content of its own kind; later the same day: a save replaces the backup of the save before only once the project is saved, so a save that fails leaves that backup as it was, and a save over a file whose `.bak` is read-only is refused (`docs/SCHEMA.md → Save`); the save had made the new backup first.

## Where things live

| doc | what | read it when |
|---|---|---|
| `docs/SCHEMA.md` | ValueTree schema, invariants 1–12, load/save, canonical form, migrations | before touching `src/model/`, `src/io/`, or any fixture (auto-loaded there) |
| `docs/EDITING.md` | refcount gate, make-unique, split, resize, conversions, quantisation, recording, knobs | before touching `src/model/` or `src/views/` (auto-loaded) |
| `docs/RENDERING.md` | pipeline, note windows, equal-tick order, step clips, same-pitch, automation merge, chase, SMF export/import | before touching `src/render/`, SMF I/O, render tests (auto-loaded) |
| `docs/ARCHITECTURE.md` | threads, snapshot lifetime, MIDI input, timing, clock out, session/transport state, platform backends | before touching `src/engine/`, `src/platform/`, `TransportFacade.h`, `tools/miditiming` (auto-loaded) |
| `docs/GUARDRAILS.md` | full contract of the five guardrails, golden case list, sanitizer builds, manual checks, external review | before touching `tools/`, `tests/guardrails/`, golden files, `CMakeLists.txt` (auto-loaded) |
| `docs/MILESTONES.md` | milestones 0–7 with demo and pass criteria | when starting, planning or closing a milestone |
| `docs/DECISIONS.md` | status of every [Dn] default and open question | when a task depends on one; when Roland answers one |
| `docs/TIMING.md` | milestone 0 timing results, run conditions, how to read and repeat them | before changing the send path, the timing targets or `tools/miditiming`; at milestone 3 |
| `TOOLCHAIN.md` | pinned versions (JUCE, Xcode, clang, CMake, Ninja, Catch2, ccache, deployment target), build-time baselines | before changing the build or any pin |

When a task spans areas, read every doc whose area it touches, not only the ones auto-loaded so far.

## What this is

Hums is a MIDI-only sequencer for macOS, built with JUCE. It implements one person's (Roland's) personal workflow for sequencing analog hardware synths over MIDI. No audio, no plugins. It has to be reliable enough to be used in real music-making for years, so maintainability beats cleverness and feature count.

**Licence (decided 20 Nov 2026).** Hums is licensed under the GNU AGPL, version 3 (`LICENSE`), because JUCE 9 is available only under the AGPLv3 or a commercial licence and Hums uses it under the AGPLv3. Any further dependency must have a licence compatible with that; ask Roland before adding one.

**Platform stance (decided 18 Nov 2026, Windows added 20 Nov 2026).** macOS is the only platform that is built, tested and shipped. Linux and Windows versions may be developed later, once everything works on macOS; neither is a target now. The code is written so that such a port would need new backends and a theme, not a redesign: everything that touches the OS sits behind the small `platform/` interfaces (rule 1, *Layers*, guardrail 2). JUCE draws its own widgets inside a native window and is not expected to blend into macOS, GNOME, Windows or any other desktop; multimedia UIs rarely do, and that is accepted. iPad was assessed and dropped (see *Non-goals*).

**Language stance (decided 19 Nov 2026).** Everything under `src/` and `tests/` is C++20 on JUCE; Objective-C++ appears only in `platform/mac/`; `tools/` scripts are Python or shell. Rust was assessed and rejected. JUCE cannot be used from Rust in any way that helps here: the existing bindings (`cxx-juce`) cover the audio modules only — no GUI, no `ValueTree`, no `UndoManager`. A pure-Rust build would therefore mean writing our own document model and betting five custom editors on a GUI toolkit far younger than JUCE. A hybrid (Rust core, JUCE GUI) would put an FFI boundary through the document every view reads, and give a solo maintainer two languages and two build systems. Do not propose Rust, a Rust component, or any FFI bridge. The price of C++ is that lifetime and threading bugs compile cleanly; the sanitizer builds exist to catch them (`docs/GUARDRAILS.md → Sanitizer builds`).

The app is a set of tabs, each a different way of looking at and entering the *same* musical data: a classic piano roll, an Ableton-style clip arrangement, an MPC-style drum grid, a 1970s-style analog step sequencer (Moog 960 / ARP 2500 / Korg SQ-10 workflow), and a parameter recorder for CC sweeps. Plus a Routing tab that maps physical MIDI ports to stable logical names.

## Non-negotiable rules

1. **macOS is the only target.** It is the only platform that is built, tested and shipped. Code that talks to the OS directly — CoreMIDI, mach thread policies, anything Cocoa — lives only in `src/platform/mac/`, behind the std-only interfaces in `src/platform/`; nowhere else includes an OS header, and the engine names only the interfaces. There are no Linux/Windows backends, no `#if` per platform, and no feature is held back or generalised for a platform we don't build. Portability is a property of the include graph (guardrail 2), not a feature. Using CoreMIDI directly where JUCE's MIDI output can't deliver the timing is allowed; that is what macOS-only buys us — inside `platform/mac/`.
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

Exact versions live in `TOOLCHAIN.md`; this list says what is pinned and how.

- JUCE **9.0.6**, pinned for the whole Hums v1 cycle (recorded in `libs/JUCE_VERSION` and `TOOLCHAIN.md`), as a git submodule in `libs/JUCE` checked out at the `9.0.6` release tag. Built via JUCE's CMake API. No Projucer. Current master docs are reference, never a reason to move the pin; for API questions read `libs/JUCE/modules` (rule 13). Moving the pin within v1 is a decision for Roland, not a maintenance task. `tools/check_juce_pin.py` fails the test run when the submodule and `libs/JUCE_VERSION` disagree.
- C++20, Xcode's clang. Pinned Xcode, CMake, Ninja and macOS deployment target recorded in `TOOLCHAIN.md`.
- Tests: Catch2 v3 via CMake `FetchContent`, pinned. Tests cover model, IO, rendering, and engine logic. GUI is not unit-tested; it has a manual checklist (`docs/MANUAL_CHECKS.md`).
- Build shape: each layer (`platform` with its `platform/mac/` backends, `model`, `io`, the render core, the rest of `render`, `engine`) is a library target, and the app links them: a static library, or an INTERFACE library while the layer holds only headers, as the `platform/` interfaces do. Every layer is declared in `src/` from the start; it becomes a target with its first source (`hums_add_layer` in `src/CMakeLists.txt`). The JUCE modules are compiled once, into `hums_juce`, which the JUCE-using layers link; JUCE modules must be linked `PRIVATE` and only once per binary. Test executables link only the layer libraries they test plus the JUCE modules those layers use; they never link the app target or `juce_gui_*`. Render-core tests link no JUCE at all. This keeps an edit-build-test iteration, Roland's or an agent's, from paying the full JUCE and app link. `ccache` is the compiler launcher; its version and wiring are recorded in `TOOLCHAIN.md`.
- Guardrail checks are plain scripts in `tools/`, run with the pinned Xcode's Python (`xcrun python3`) and registered as CTest tests so `ctest` fails on a violation. Each script ships a fixture that must *fail* it, and that negative test runs too. No separate CI is assumed; the local test run is the gate.
- Sanitizers: besides the plain Debug build there are two instrumented configurations, ASan + UBSan and TSan, selected by the CMake option `HUMS_SANITIZE` (`address` / `thread`), each in its own build directory. `tools/gate.sh` runs all three, and builds `tools/miditiming` in the plain one so the timing tool cannot break unnoticed; that script, not a bare `ctest`, is the full gate. In every configuration a failed `jassert` or a JUCE leak report fails the test that printed it; test executables are registered through `hums_discover_tests`, never a bare `catch_discover_tests`. See `docs/GUARDRAILS.md → Sanitizer builds`.
- `.clang-format` is checked in and codifies the style of the milestone 0 code (decided 21 Nov 2026; never discussed again). Format new and changed C++ with the pinned Xcode's formatter, `xcrun clang-format -i <files>`.

## Commands

Fill in as the build system lands. Expected shape:

- Build directories: `build-ninja` (plain Debug, Ninja — the fast loop and the gate's plain run), `build-asan`, `build-tsan` (instrumented, Ninja), `build-release` (plain Release, Ninja — timing measurements only), `build` (Xcode, for GUI debugging only). All are ignored by git.
- Configure: `cmake -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug`; Xcode: `cmake -B build -G Xcode`
- Fast loop (no app build): `cmake --build build-ninja --target hums_tests && ctest --test-dir build-ninja --output-on-failure` — `hums_tests` builds every test executable and nothing of the app.
- Build everything (once an app target exists): `cmake --build build-ninja`; in Xcode `cmake --build build --config Debug`
- Test: `ctest --test-dir build-ninja --output-on-failure` (includes the guardrail checks; always read-only)
- Guardrails only: `ctest --test-dir build-ninja -R guardrail --output-on-failure`
- Property tests (guardrail 3) with more seeds, or one seed again: `HUMS_PROPERTY_SEEDS=10000 ctest --test-dir build-ninja -R guardrail.properties --output-on-failure`; `HUMS_PROPERTY_FIRST_SEED=<seed> HUMS_PROPERTY_SEEDS=1 ctest …` (without the variables: seeds 1 to 1,000, in the instrumented configurations 1 to 200)
- ASan + UBSan: `cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DHUMS_SANITIZE=address`, `cmake --build build-asan`, `ctest --test-dir build-asan --output-on-failure`
- TSan: the same with `build-tsan` and `-DHUMS_SANITIZE=thread`; test run `ctest --test-dir build-tsan -R "engine|canary" --output-on-failure`
- Full gate: `tools/gate.sh` (plain, then ASan + UBSan, then TSan; stops at the first failure)
- Regenerate render golden files: `cmake --build build-ninja --target regenerate_golden` (sets `HUMS_REGENERATE_GOLDEN=1` and runs the render test binary) — then read the diff before committing it. CTest does not forward arguments to tests; never rely on `ctest -- flag`.
- Timing build: `cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release`, `cmake --build build-release --target miditiming`
- Timing diagnostic: `build-release/tools/miditiming/miditiming --out "<port>" --in "<port>" --minutes 5` (plain Release build only; `--list` shows ports, `--help`-style usage on a bad argument)
- Timing check against the app (M3+): `build-release/tools/miditiming/miditiming --listen --in "<port>" --reference <export.mid>`
- Run: `open build/Hums_artefacts/Debug/Hums.app`

## Git and done

- One branch per milestone (`m0-timing`, `m1-model`, …), merged into `main` when the milestone's pass criteria in `docs/MILESTONES.md` are met. One commit per task. Never force-push, never rewrite pushed history.
- Roland reviews through the tests: `git log -p -- tests/` shows every change to the contract. A commit that changes behaviour without touching `tests/` is suspect. A commit that changes a golden file or a guardrail fixture needs its reason in the message.
- The full gate (`tools/gate.sh`) runs before every commit **[D9]**; plain `ctest` is for the loop in between.
- Claude may not say a task is "done" until it has pasted the final summary from a `tools/gate.sh` run against that commit (for each configuration, the `ctest` line such as `100% tests passed, 0 tests failed out of N`). Before the gate exists (early milestone 0), the plain `ctest` summary line is enough. If anything fails, Claude pastes the failure output instead.

## Repository layout

```
CMakeLists.txt
CLAUDE.md
README.md
LICENSE                GNU AGPL, version 3
.clang-format          the code style, decided once
TOOLCHAIN.md           pinned versions and build-time baselines
.claude/rules/         path-scoped pointers that load the docs below when matching files are touched
docs/
  SCHEMA.md            ValueTree schema (authoritative)
  EDITING.md           editing semantics
  RENDERING.md         rendering rules, SMF export/import
  ARCHITECTURE.md      threads, timing, transport and session state, platform backends
  GUARDRAILS.md        guardrail contract, golden case list, sanitizer builds
  MILESTONES.md        milestone plan
  DECISIONS.md         status of defaults and open questions
  TIMING.md            milestone 0 timing results and how they were measured
  timing/              raw miditiming reports behind TIMING.md
  MANUAL_CHECKS.md     GUI interaction checklist, run at milestone boundaries
libs/
  JUCE/                submodule, pinned tag (9.0.6)
  JUCE_VERSION         the pinned tag, one line
tools/
  check_identifiers.py Ids.h is the only place schema names appear in property APIs
  check_layers.py      include graph obeys the layer rules, incl. OS headers only under platform/mac/
  check_juce_pin.py    libs/JUCE is at the tag libs/JUCE_VERSION names
  cpp_text.py          shared by the check scripts: tells comments from code
  gate.sh              the full gate: plain, ASan + UBSan and TSan test runs in sequence
  sanitizers/          suppression files: pinned-JUCE symbols only, each with a reason; ideally empty
  fixtures/            per check, one deliberately violating file or small tree for each rule it enforces
  miditiming/          loopback timing measurement, send and listen modes (JUCE console app, kept)
src/
  platform/            std-only interfaces through which the engine reaches the OS
    MidiScheduler.h    open a port, send a packet at a monotonic time, close
    MonotonicClock.h   nanoseconds since an arbitrary epoch
    RealtimeThread.h   raise the calling thread to real-time priority
    mac/               the only backends: CoreMIDI, mach thread policy — the only place OS headers appear
  app/                 Main, MainWindow, tab host, application-level wiring; constructs the platform/mac/ backends
    TransportFacade.h  the only door from views to engine
  model/               ValueTree schema ids, typed wrappers, commands, validator, refcount gate
    Ids.h              every ValueTree type/property identifier, nowhere else as strings
    DeclaredIdentifiers  the identifiers of Ids.h as a list in declaration order; its test reads Ids.h and holds the two together
    PropertyReader     typed reading of properties (int, int64, bool, double, string); absent vs malformed
    CanonicalDouble    a double as the document holds it: six significant digits, stored as its saved text [D14]
    ColourText         a colour as the document holds it: "#AARRGGBB" or empty; what the Validator and the commands check
    CopyName           the name of a clip made as a copy of another: "Bass (copy 1)" [D20]
    AutomationClipRule what an AutomationLane requires of a clip it references (invariant 9: cc events only, and on lanes only [D21]), for the commands on both sides of it
    Settings           the settings path: reads and writes UiState, always with a nullptr UndoManager
    Validator          invariants 1–12, run on load and after every command in tests
    commands/          free functions that mutate the document (take UndoManager&), one file per part of the tree (RoutingCommands, …);
                       CommandResult is what each reports (applied, or refused with a reason), NodeLookup how they find a node,
                       SiblingOrder how they move one among its siblings, EditContext what a gesture hands to every command
                       that changes a clip, RefcountGate what such a command passes: update all, or the gesture's one clone,
                       CloneMark what the gate leaves in the gesture's undo transaction with a clone, by which a context kept past its gesture is known,
                       PlacementOffsets what every change of a clip's loop length does to the offsets of its placements
  io/                  ProjectFile (XML, canonical writer, load sequence, atomic save), XmlWellFormedness (what the load asks of a file's text
                       that JUCE's parser does not), SmfExport, SmfImport, Migrations (ordered step table)
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
    fixtures/          valid projects (incl. the schema example with real UUIDs), old-version files; under refused/ the files
                       the load path must refuse (malformed, too new, one or more per invariant)
  render/
    golden/            fixture project + expected event list, one pair per rendering case
  engine/
  platform/            the pure parts of the platform/mac/ backends (no MIDI hardware needed)
  guardrails/          invariant property tests (seeded document generator, document checks, random commands and the checks on each); drives the tools/ scripts and their negative fixtures;
                       sanitizer canaries; the two programs that prove a JUCE assertion or leak report fails a test
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
- **Platform layer** — `src/platform/`: the std-only interfaces (`MidiScheduler`, `MonotonicClock`, `RealtimeThread`) through which the engine reaches the OS, and `platform/mac/`, the only backends. The one place OS headers appear.

## Modules (tabs)

- **Routing** — bind Performers and Players to physical ports reported by the OS, and edit the PadMap. Reconnect by device identifier, fall back to name, else show "unassigned". Must be simpler than any DAW's track-type / port setup.
- **Piano Roll** — classic whole-track view. Standard note editing, CC lanes, velocity slide on long-click, quantise selector per clip. StepClips render here read-only; editing one offers "convert to EventClip" (gate applies).
- **Arrangement / Session** — Placements on Tracks over a timeline, plus launchable Slots for jamming. Split, move, resize, crop placements; the refcount gate lives here and in every editor.
- **Drums** — x0x-style grid over a StepClip with one Lane per pad, all lanes `fixedPitch`. Pads map to pitches via the PadMap. Any other grid shape is shown read-only.
- **Step Sequencer** — 960 / SQ-10 workflow over a single melodic Lane: one knob per column for pitch, extra rows for per-cell CC values, on/off, skip, reset. Any other grid shape is shown read-only. Needn't look vintage, must *work* vintage.
- **Automation Recorder** — play the arrangement, turn Track knobs, record the CC moves into an AutomationLane placement from punch-in to punch-out.

## Architecture

### Layers and dependencies

Six source layers, a std-only core inside `render/`, and a std-only interface set inside `platform/`:

| layer | may include |
|---|---|
| `platform/` (interfaces) | other `platform/` interface headers, std — no JUCE, no OS headers |
| `platform/mac/` | `platform/`, CoreMIDI / CoreFoundation / mach / Cocoa, JUCE, std — the only directory where OS headers appear |
| `model/` | `model/`, JUCE, std |
| `io/` | `io/`, `model/`, `render/`, JUCE, std |
| `render/` | `render/`, `model/`, JUCE, std — except the **render core** (`render/snapshot/`, `TempoMap`, `Kernel`, `NoteTracker`, headers and sources alike), which includes only std and other render-core files |
| `engine/` | `engine/`, the render core, the `platform/` interfaces, JUCE, std — never `model/`, never any other `render/` file, never `platform/mac/` |
| `views/` | `views/`, `model/`, `app/TransportFacade.h`, JUCE, std |
| `app/` | anything |

The render core is std-only so that the engine's dependency on it is transitively clean by construction: `check_layers.py` checks the core's own includes, not paths through them. The core therefore never reads the document itself — `SnapshotBuilder` turns the Timeline into TempoMap data and clips into templates and chase tables on the message thread, and `io/` does the same for export.

The `platform/` interfaces are std-only for the same reason the render core is: the engine's dependency on them is clean by construction. Time crosses that boundary as nanoseconds on a monotonic clock; the CoreMIDI backend converts to and from mach host time internally. `app/` constructs the `platform/mac/` backends at startup and hands them to the engine as interface references; the engine never knows which backend it holds. The send path is CoreMIDI with future timestamps (`platform/mac/CoreMidiScheduler`), chosen at milestone 0 **[D12]**; swapping it would still be a one-line change in `app/`. A later Linux or Windows port adds a sibling backend directory (`platform/linux/`, `platform/windows/`) and changes nothing above this layer.

`TransportFacade.h` must not transitively include any other `engine/` header; it exposes an interface class and plain types. Include graphs cannot prove thread behaviour, so the engine additionally asserts in debug builds that it is never called on the message thread with a `ValueTree` in scope — in practice: the engine simply cannot name the type.

Thread model, snapshot handoff, timing and transport behaviour: `docs/ARCHITECTURE.md`. Three things never travel in the Snapshot — mute/solo, the transport loop, live knob values; they reach the engine as live commands.

## Guardrails

Roland is the only maintainer and will not read every line, so structure is enforced by the test run. Full contract in `docs/GUARDRAILS.md`. In short:

1. **Schema names live only in `Ids.h`** — `tools/check_identifiers.py`; exceptions are listed explicitly in the script, never wildcarded. It looks wherever a `juce::Identifier` is expected, calls to our own functions that take one and members of our own structs that are one included, and at XML tag and attribute names. In `model/`, `io/` and `render/` no name at all is spelled as a string, schema name or not [D15].
2. **Layer rules** — `tools/check_layers.py` enforces the table above, including the std-only render core, the std-only `platform/` interfaces, OS headers and `.mm` files only under `platform/mac/`, and `TransportFacade.h`. Our own headers are included with quotes; one in angle brackets fails in every layer. A file under `src/` that is neither source by its suffix nor named in the script as not source fails too.
3. **Invariants as property tests** — random valid documents × random valid and invalid commands; Validator, undo/redo XML identity, canonical save→load→save byte identity. Seeds printed; failures shrunk into named regression tests.
4. **Render golden files** — one fixture + hand-reasoned expected event list per rendering rule, in emission order. Regenerated only via the `regenerate_golden` target, and the diff is read before committing.
5. **Loading is a wall** — every malformed, too-new or invalid fixture is refused with the current document and source file untouched.

Beside the five, `tools/check_juce_pin.py` checks that `libs/JUCE` is at the tag `libs/JUCE_VERSION` names (`docs/GUARDRAILS.md → JUCE pin`).

Every script ships a fixture that must fail it. Never weaken, skip or `// NOLINT` a guardrail to make a feature land (rule 12). Manual checks: `docs/MANUAL_CHECKS.md`, at milestone boundaries, on the ASan + UBSan build of the app.

**Sanitizer builds** — the suite also runs under ASan + UBSan (all tests) and TSan (engine tests, real threads); a sanitizer report is a failing test, and three canaries with planted bugs prove the instrumentation is still there. A failed `jassert` and a JUCE leak report fail a test the same way, with two planted-bug programs to prove it. `tools/gate.sh` runs all three configurations. Full contract in `docs/GUARDRAILS.md → Sanitizer builds`.

## Conventions

- One class per file, file name = class name, under `src/` and `tests/`; `tools/` is exempt (decided 21 Nov 2026: a small tool may keep closely related types in one header). One namespace per layer (`platform`, `model`, `io`, `render`, `engine`, `views`).
- `views/` is mouse-and-keyboard first and JUCE-only: no AppKit, no Objective-C++, no touch affordances. Shortcuts use `juce::ModifierKeys::commandModifier`, never a hard-coded Cmd. The native menu bar is wired in `app/`.
- ValueTree type and property identifiers live in `model/Ids.h` as `juce::Identifier` constants. Nowhere else is a type or property name spelled as a string. Enforced by guardrail 1.
- Typed wrapper classes over ValueTree nodes (`ClipModel`, `TrackModel`, …) are how views *read* the document. Views never call `setProperty`.
- Mutations are free functions in `model/commands/`, taking `UndoManager&` and, for clip content, an `EditContext&`. Commands validate their preconditions before touching the tree and never half-apply. Commands never call `beginNewTransaction`; the view owning the gesture does, once, at gesture start, and creates the gesture's `EditContext` right after it.
- Two deliberate exceptions to the command path: `UiState` is written through `model/Settings` with a `nullptr` UndoManager, and loaded/migrated trees are constructed by `io/` and installed whole.
- Comments say *why*. Names say *what*.
- `juce::` prefix explicit. No `using namespace juce` in headers.
- Prefer `std::` types; JUCE containers only at JUCE API boundaries. Nothing from JUCE in `render/snapshot/`.
- Tests: one file per model type, one per IO format, one per rendering rule.

## Non-goals (for now)

Audio, plugins, MIDI 2.0 / MPE, notation, external clock sync (clock master only), tempo-change UI (the Timeline stores a map; v1 UI edits a single tempo), synth editor panels and a SysEx patch librarian (a later idea — the design must not prevent it: events are generic, SysEx is an Event kind), internal modulators such as an arpeggiator or chord player (later idea — reserved as a per-track Transform; constraints in `docs/RENDERING.md → Transform slot`), live MIDI thru and input effects — Performer → Player monitoring, input quantisation, live arpeggiation of what is being played (the document never sees these; the engine has no input → output path, only input → recording), scripting, Rust in any form — whole app, a component, or an FFI bridge (assessed and rejected 19 Nov 2026; reasons under *What this is → Language stance*), iPad / iOS (assessed and rejected 18 Nov 2026: the engine would port, since iOS is CoreMIDI, but every editor would need a second, touch-first interaction design and the app would need a silent audio session to keep running in the background — not worth it).

**Linux and Windows** are not targets now; either may come later, once everything works on macOS. Nothing is done for either until Roland takes that decision; the `platform/` layer only keeps it cheap. If one comes, it needs at least:

- **Linux:** an ALSA-sequencer `MidiScheduler`, an rtkit-based `RealtimeThread`, a Linux LookAndFeel, a `miditiming` report on real hardware, and a device-identity story (ALSA client:port numbers are not stable across reboots, so the name fallback in Routing would carry the load).
- **Windows:** a `MidiScheduler` on Windows MIDI Services (or WinMM, which has no timestamped send, so the engine would have to time sends itself), an MMCSS-based `RealtimeThread`, a Windows LookAndFeel, a `miditiming` report on real hardware, and a device-identity story (WinMM port names and indices are not stable either). `views/` already uses `commandModifier` and keeps the menu bar in `app/`, so shortcuts and menus need no redesign.
