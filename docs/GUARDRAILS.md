# Guardrails

Authoritative, together with `CLAUDE.md` (rules, glossary, layers) and `docs/SCHEMA.md` (document model). Items tagged **[Dn]** are defaults whose status is in `docs/DECISIONS.md`. `CLAUDE.md → Guardrails` carries the one-line summary; this file is the full contract.

Roland is the only maintainer and will not read every line. Structure therefore has to be enforced by the test run, not by review. These checks exist so that drift fails loudly while nobody is watching. They land in milestone 1, before there is anything to protect, and run on every `ctest`. Every script has a fixture in `tools/fixtures/` that must make it fail; that negative run is itself a test.

## 1. Schema names live only in `Ids.h`

`tools/check_identifiers.py` parses `src/model/Ids.h`, collects every string literal used to construct a `juce::Identifier`, and scans the rest of `src/` for those literals **inside Identifier construction and property/child APIs** (`juce::Identifier (`, `getProperty (`, `setProperty (`, `getChildWithName (`, `hasType (`, `ValueTree (` and friends). A bare word like `name` in a comment or a UI label is not a hit. Any hit outside `Ids.h` fails the check with `file:line`.

Allowed exceptions, and only these: `src/io/Migrations.cpp`, which by definition must name properties that no longer exist, and `tests/io/fixtures/`. Both are listed explicitly in the script — not matched by a wildcard. View-state identifiers under `UiState` are not an exception; they live in `Ids.h` too.

## 2. The layer rules are checked, not just documented

`tools/check_layers.py` walks the `#include` lines under `src/`, resolves quoted relative paths, and enforces the table under `CLAUDE.md → Layers and dependencies`, including the render-core std-only rule (`render/snapshot/`, `TempoMap`, `Kernel`, `NoteTracker` include only std and each other, so the engine's dependency on them is transitively clean without a transitive check) and the transitive check that `app/TransportFacade.h` pulls in no other `engine/` header. `views/` including any other `engine/` header, `engine/` including anything from `model/` or from `render/` outside the core, and a render-core file including JUCE, are the failures this check exists for.

## 3. Invariants as property tests, not examples

`tests/guardrails/` holds a small generator that builds random *valid* documents (a few tracks, clips of both kinds, placements, lanes, slots, automation lanes) and applies random sequences of commands from `model/commands/` — valid ones *and* deliberately invalid ones (overlapping placements, zero-length loops, notes into an automation clip). After every command it asserts all twelve invariants from `docs/SCHEMA.md` via the Validator, plus:

- a valid command fully applies; an invalid one leaves the undoable subtree XML-identical and the undo history unchanged;
- undo restores the undoable subtree to an XML-identical state, and redo returns to the post-command state (`UiState` excluded from the comparison);
- canonical save → load → canonical save is byte-identical;
- clip ids referenced before a command still reference a clip of the same `kind` after it, or the command was a conversion.

Seeds are printed on failure and reproducible. When the generator finds a failure, shrink it to a minimal document and add that as a named regression test.

## 4. Render golden files

Every rendering rule in `docs/RENDERING.md` gets a pair in `tests/render/golden/`: a fixture project (XML, real UUIDs) and the expected event list, plain text, one event per line, `tick track channel type a b [data]`, **in emission order, not re-sorted**, SysEx with the full payload. The render test runs Kernel + NoteTracker over the fixture's full range and diffs against the file. Expected files are reasoned out by hand from the rules, never produced by the renderer under test on first creation.

Required cases, at minimum: skipped column; `resetAt` shorter than `stepCount`; `gate > 1.0` into a different pitch; `gate > 1.0` into the same pitch (merge); gate reaching into a skipped column; gate across the loop point into a different pitch, and into the same pitch (retrigger, not merge); `fixedPitch` lane ignoring cell pitch; off-cell CC values; placement `offset`; placement shorter and longer than the clip loop; two placements of one clip restarting (offset 0) and phase-continuous (merged); split during a held note; same-pitch overlap on one track and across two tracks on one destination; bank-select → program change → note at one tick; note-off and note-on of one pitch at one tick; AutomationLane overriding, passing through, and restoring the underlying value at exit; a lane whose first event comes after its placement start; two lanes overriding the same CC; event-clip note crossing `loopLength`; content past `loopLength` silent; `quantise` at `1/8` and `1/8T`; `quantise` moving an onset across the loop point (wraps to 0) in a loop that is not a multiple of the grid; two notes of one pitch quantised onto one tick; transport loop wrap with a held note; chase at a seek target: value found in the current placement, in an earlier iteration of it, in an earlier placement, and no value at all; export range cutting a note; a track with no Player rendering normally.

Golden files are regenerated only through the `regenerate_golden` target, and the diff is read before it is committed. A silently regenerated golden file is worse than no test — it turns a behaviour change into a green build.

## 5. Loading is a wall

`tests/io/` loads every fixture in `fixtures/` through the real load path: valid files install; files with a higher `schemaVersion`, malformed XML, invariant violations, and a fixture whose migration throws are all refused with the current document and the source file untouched. Save failure (read-only target) leaves the previous file intact.

## Manual checks

`docs/MANUAL_CHECKS.md` is a short script run at each milestone boundary: selection, dragging, undo across tabs, focus and keyboard shortcuts, tab switching during playback, device unplug/replug during playback, sleep/wake, close with unsaved changes, recovery-file offer. Not automated; not optional.

## External review

Optional, at milestone boundaries only, never per commit. Give the reviewer a read-only checkout, `docs/SCHEMA.md`, `CLAUDE.md` and the other files in `docs/`, and a fixed question list; do not tell it what the code is supposed to do beyond those documents, and never say the code is believed correct — a reviewer primed that way stops finding things. Ask for a ranked, capped list. Findings that matter become guardrail tests; the rest is discarded, not archived.
