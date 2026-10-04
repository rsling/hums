# ValueTree Schema

Authoritative description of the document model. `src/model/Ids.h` mirrors this file exactly; if they disagree, fix one and bump `schemaVersion`.

Revision history: v1 drafted Sept 2026; amended 3 Oct 2026 after external review (renderer decoupled from routing, equal-tick order, loop-length invariants, PadMap moved out of UiState, quantisation added, validated load boundary); quantisation grid origin, wrap rule and validation tightened the same day; later the same day: transport-loop command path, view-state identifiers in `Ids.h`, `stepCount` change rules, automation ownership window, arrangement end. `schemaVersion` is still 1 because nothing has shipped.

## Conventions

- Type names are `PascalCase`, property names `camelCase`. Both are `juce::Identifier` constants in `Ids.h`.
- `id` is a UUID string (`juce::Uuid::toString()`) on every node that can be referenced. References are properties named `<thing>Id`. One deliberate exception: `Slot.scene` is a row index, not an id (see *Slots*). Refcounts are computed, never stored.
- All times and lengths are integer **ticks**, PPQ 960. Stored as `int64`. No negative times anywhere in the document.
- Booleans are stored as `0` / `1`.
- Pitches, velocities, CC numbers and values are 7-bit ints (0–127).
- Colours are `"#AARRGGBB"` strings.
- **Child order is always preserved, never re-sorted on read or save.** Where order is semantic it is noted (Tracks, Lanes, Cells, Knobs, AutomationLanes). For `Note` and `Event` children, order is the tie-breaker at equal ticks (see *Events*). The renderer sorts; the document doesn't.
- Everything under `Project` **except `UiState`** is undoable. `UiState` is edited with a `nullptr` UndoManager through a dedicated settings path.
- Typed access only: `ValueTree::fromXml` yields string properties; the wrappers convert with explicit typed accessors (`getInt64`, `getBool`, …) and never compare `var` types.
- Defaults listed below are what the wrapper returns when the property is absent; the canonical writer omits defaulted properties.
- On load, unknown properties and children are preserved so that older builds don't destroy data from newer ones. This is *data* preservation, not semantic compatibility: a file with `schemaVersion` greater than the running build's is refused (see *Loading*).

## Tree

```
Project
├── Routing
│   ├── Performer*
│   ├── Player*
│   └── PadMap
│       └── Pad*
├── Timeline
│   ├── TempoEvent*
│   ├── TimeSigEvent*
│   └── Marker*
├── ClipPool
│   └── Clip*
│       ├── Events            (kind = "event")
│       │   ├── Note*
│       │   └── Event*
│       └── Grid              (kind = "step")
│           └── Lane*
│               └── Cell*
│                   └── CcValue*
├── Tracks
│   └── Track*
│       ├── Placements
│       │   └── Placement*
│       ├── AutomationLanes
│       │   └── AutomationLane*
│       │       └── Placement*
│       ├── Knobs
│       │   └── Knob*
│       └── Slots
│           └── Slot*
├── Session
│   └── Scene*
└── UiState
```

## Node reference

### Project

| property | type | default | notes |
|---|---|---|---|
| `schemaVersion` | int | — | required; migrations run when lower than current; higher is refused |
| `name` | string | "" | |
| `ppq` | int | 960 | self-description only; the code assumes 960 and refuses other values |

Children: exactly one each of `Routing`, `Timeline`, `ClipPool`, `Tracks`, `Session`, `UiState`.

### Routing

Logical ports and the pad map. Persisted by name; bound to physical devices at runtime by identifier, then by name, else unassigned. **Binding affects transmission only, never rendering** — a Track with no bound Player still renders, exports, and shows up in the piano roll; the live router just has nowhere to send it.

**Performer** — a logical MIDI input.

| property | type | default | notes |
|---|---|---|---|
| `id` | uuid | — | |
| `name` | string | — | what the user sees everywhere |
| `deviceIdentifier` | string | "" | last bound `MidiDeviceInfo::identifier` |
| `deviceName` | string | "" | fallback match if the identifier is gone |
| `channelFilter` | int | 0 | 0 = omni, 1–16 = only that channel |

**Player** — a logical MIDI output.

| property | type | default | notes |
|---|---|---|---|
| `id` | uuid | — | |
| `name` | string | — | |
| `deviceIdentifier` | string | "" | |
| `deviceName` | string | "" | |
| `sendClock` | bool | 0 | MIDI clock + Start/Stop/Continue/SPP |

Why no channel on Player: the channel belongs to the Track. One Player, many Tracks, many channels.

Two Players bound to the same physical port both sending clock would emit duplicate pulses. The router sends clock once per physical port if any bound Player has `sendClock`.

**PadMap / Pad** — one map per project, used by the drum view and by the pad Performer. It is input configuration and undoable (moved here from `UiState`: remapping changes what future input produces, so it belongs with routing, not with scroll positions). Remapping a pad never rewrites clips.

| property | type | default | notes |
|---|---|---|---|
| `index` | int | — | pad number, 0-based, unique within the map |
| `inputPitch` | int | — | the note the physical pad sends; how incoming MIDI identifies the pad. Unique within the map |
| `pitch` | int | — | the note the pad produces in a drum Lane (`fixedPitch`) |
| `label` | string | "" | |

### Timeline

SMF conductor-track equivalent. Global, never per track. **The Timeline is the single authority for tick ↔ time conversion**; the engine, the recorder and SMF I/O all go through `render/TempoMap`, never through ad-hoc arithmetic.

**TempoEvent** — `tick` (int64), `bpm` (double, finite, > 0). One at tick 0 is required. At most one per tick; the loader keeps the last and reports the duplicate. v1 UI edits only the one at tick 0; an imported tempo map stays in effect for playback and export until the user flattens it explicitly.

**TimeSigEvent** — `tick`, `numerator` (int, 1–32), `denominator` (int, power of two, 1–64). One at tick 0 is required. Same duplicate rule.

**Marker** — `tick`, `name`. Round-trips to SMF marker meta events.

### ClipPool / Clip

All musical content lives here. Tracks only *reference* clips.

| property | type | default | notes |
|---|---|---|---|
| `id` | uuid | — | |
| `name` | string | "" | |
| `kind` | string | — | `"event"` or `"step"`; immutable after creation |
| `loopLength` | int64 | — | **event clips only.** Authoritative loop length, `> 0`. Content may be shorter or longer |
| `quantise` | string | "" | **event clips only.** Non-destructive playback quantisation of note onsets: `""` (off), `1/2`, `1/4`, `1/8`, `1/2T`, `1/4T`, `1/8T` |
| `colour` | colour | "" | |

For **step clips**, `loopLength` is absent and derived: `stepLength × (number of non-skipped columns before resetAt)`. Storing it would be a second source of truth. That count must be ≥ 1 (invariant 2); a command that would skip the last audible column, or move `resetAt` so that none remains, is rejected.

`quantise` grids in ticks: `1/2` = 1920, `1/4` = 960, `1/8` = 480, `1/2T` = 1280, `1/4T` = 640, `1/8T` = 320. Grid lines are multiples of the grid measured from **clip tick 0**; the arrangement position of a placement plays no role. The candidate grid lines are the multiples of the grid in `[0, loopLength]`, with `loopLength` itself standing for tick 0 — so a loop that is not a multiple of the grid has no grid lines invented beyond its end, and an onset nearer to the loop point than to the last real grid line wraps to 0. The renderer moves each note onset to the nearest candidate (ties round down) and moves the note end with it, so lengths are preserved; non-note events are never quantised. **The quantised onset is the note's position for every later rule** — note window, placement `offset`, equal-tick order, same-pitch handling; the stored `tick` is editing data only. Notes with `tick ≥ loopLength` are not quantised; they are silent either way. The property is undoable and subject to the refcount gate like any other clip mutation. The explicit command `applyQuantise(clip)` rewrites the note ticks exactly as the renderer would place them, leaves notes with `tick ≥ loopLength` untouched, and clears the property; it is a no-op when the property is already `""`.

A clip's refcount = number of `Placement` and `Slot` nodes whose `clipId` matches. Deleting a clip with refcount > 0 is refused by the command layer; "remove unused clips" deletes those with refcount 0.

#### Event clip: `Events`

Notes are objects, not on/off pairs. Everything else is a generic `Event`. This is the one place we deliberately don't mirror raw MIDI, because editing notes needs a length and a position.

**Note**

| property | type | default | notes |
|---|---|---|---|
| `tick` | int64 | — | start, relative to clip start |
| `length` | int64 | — | ticks; > 0 |
| `pitch` | int | — | 0–127 |
| `velocity` | int | 100 | 1–127 |
| `offVelocity` | int | 64 | 0–127; exported as note-off velocity |
| `enabled` | bool | 1 | disabled notes render nothing but stay editable |

Notes have no `id`. Selections and in-progress edits hold `juce::ValueTree` handles, which stay valid across undo/redo (JUCE re-inserts the same node object). Handles do not survive cloning, reload or conversion; selection is reset in those cases. If a later feature needs persistent note identity, add `id` with a migration — don't fake it with indices.

**Event**

| property | type | default | notes |
|---|---|---|---|
| `tick` | int64 | — | |
| `kind` | string | — | see table |
| `a` | int | 0 | meaning depends on `kind` |
| `b` | int | 0 | |
| `data` | string | "" | hex bytes, `sysex` only, excluding F0/F7; even number of hex digits, ≤ 64 KB |
| `enabled` | bool | 1 | |

| `kind` | `a` | `b` |
|---|---|---|
| `cc` | controller 0–127 | value 0–127 |
| `pitchBend` | value 0–16383 (8192 = centre) | — |
| `programChange` | program 0–127 | — |
| `channelPressure` | pressure 0–127 | — |
| `polyPressure` | pitch 0–127 | pressure 0–127 |
| `sysex` | — | — (payload in `data`) |

No channel on notes or events. The Track supplies it. An SMF track mixing channels is split into several Tracks on import.

**Equal-tick order.** Child order of `Note` and `Event` is the tie-breaker at equal ticks and is therefore semantic: SMF import stores events in file order, editors append, and a bank-select → program-change sequence survives save/load/export untouched. The renderer's emitted order at one tick on one track is: note-offs falling due, then non-note events in child order, then note-ons in child order. The renderer never sorts by kind or controller number within the "non-note" group. Full contract in `CLAUDE.md → Rendering rules`.

Content with `tick ≥ loopLength` is kept but never played (like cells past `resetAt`); the piano roll shows it greyed. A note whose onset is inside the loop and whose end is past `loopLength` plays its full length across the loop boundary.

#### Step clip: `Grid`

A grid of Columns × monophonic Lanes. Columns are not nodes; they exist implicitly as cell index 0…`stepCount − 1`, with per-column data held on `Grid`.

**Grid**

| property | type | default | notes |
|---|---|---|---|
| `stepLength` | int64 | 240 | ticks per column, 1–3840 (240 = sixteenth) |
| `stepCount` | int | 16 | number of columns, 1–64 |
| `resetAt` | int | = stepCount | loop end, 1–`stepCount`; cells beyond it keep their data |
| `skipMask` | string | all `'0'` | exactly `stepCount` chars from `{'0','1'}`, `'1'` = column takes no time |

`skipMask` as a string rather than child nodes: it is per column, dense, trivially indexed, and resizes with `stepCount` in one line. Skip and reset are per grid, not per lane — lanes inside one clip never drift apart. Polymetric layering is done with several clips on several tracks, which is what the arrangement already does.

**Changing `stepCount`** is one command. Growing appends default `Cell`s to every lane and `'0'`s to `skipMask`. Shrinking removes the trailing `Cell`s and mask characters. `resetAt` follows: if it equalled the old `stepCount` it becomes the new one (the writer omits exactly that case as a default, so this rule keeps a reloaded clip and an in-session clip behaving the same); otherwise it is kept and clamped to the new count. Like every loop-length change it is rejected if no audible column would remain, and it renormalises placement offsets (see *Loop-length changes*).

**Lane** — a monophonic row. Child order is semantic (top to bottom in the drum view).

| property | type | default | notes |
|---|---|---|---|
| `id` | uuid | — | |
| `name` | string | "" | drum view row label; falls back to the PadMap label for `fixedPitch` |
| `fixedPitch` | int | absent | present ⇒ drum row: every cell plays this pitch and cell `pitch` is ignored. Absent ⇒ melodic row |

A 960-style sequencer clip is a `Grid` with exactly one lane and no `fixedPitch`. A drum clip is a `Grid` with one lane per pad, each with `fixedPitch`. Nothing else distinguishes them. The schema also permits mixed or multi-melodic grids; an editor that doesn't support a grid's shape shows it read-only and never rewrites it into its preferred form.

**Cell** — exactly `stepCount` children *of type `Cell`* per lane, in column order. No `index` property; the column is the position among the `Cell`-typed siblings. Readers count `Cell` children only, so an unknown child node from a newer build can't shift the columns.

| property | type | default | notes |
|---|---|---|---|
| `pitch` | int | 60 | ignored when the lane has `fixedPitch` |
| `on` | bool | 0 | off cells are rests: time passes, nothing plays |
| `velocity` | int | 100 | 1–127 |
| `gate` | double | 0.5 | fraction of `stepLength`, finite, `0 < gate ≤ 4`; > 1.0 reaches into the following cells |

Note length = `round(gate × stepLength)` ticks, at least 1, measured in audible time (skipped columns don't exist in time). Overlap with the following cells is just overlap; same-pitch handling is in `CLAUDE.md → Rendering rules` (consecutive same-pitch cells with `gate > 1` merge into one note).

**CcValue** — per-cell CC output (the 960's second and third rows). Zero or more per cell, one per controller number. Sent at the cell's start tick, **also for off cells** — a rest still moves the CV row on the real thing (confirmed 3 Oct 2026).

| property | type | default | notes |
|---|---|---|---|
| `cc` | int | — | controller number, unique within the cell |
| `value` | int | — | 0–127 |

Which CC rows a step clip *shows* is a view concern (`UiState`), not clip data. The clip stores whatever values exist.

### Tracks / Track

Child order of `Track` is semantic (top to bottom).

| property | type | default | notes |
|---|---|---|---|
| `id` | uuid | — | |
| `name` | string | "" | |
| `colour` | colour | "" | |
| `playerId` | uuid | "" | empty = unassigned. The track still renders and exports; the router has nowhere to send it |
| `channel` | int | 1 | 1–16 |
| `performerId` | uuid | "" | record input for this track; empty = not recordable |
| `mute` | bool | 0 | document state; reaches the engine as a live command from a listener, never through the Snapshot (`CLAUDE.md → Threads`) |
| `solo` | bool | 0 | same |

Two Tracks may share a Player and channel. The engine keys its active-note bookkeeping by physical destination (port, channel, pitch), so the no-stuck-notes rule holds across tracks.

#### Placements / Placement

A reference to a clip on a timeline. Same node type under `Track/Placements` and under `AutomationLane`.

| property | type | default | notes |
|---|---|---|---|
| `id` | uuid | — | |
| `clipId` | uuid | — | |
| `start` | int64 | — | arrangement tick, ≥ 0 |
| `length` | int64 | — | extent on the timeline, > 0; the clip loops inside it |
| `offset` | int64 | 0 | ticks into the clip's loop at which playback starts; `0 ≤ offset < loopLength` |

Changing a placement's `length` repeats or trims the loop (**resize**). There is no time-stretch; the schema has no rate.

Placements on one Track (or one AutomationLane) must not overlap; the command layer enforces it. Splitting a placement at tick *t* produces two placements with the same `clipId`; the second gets `offset = (offset + (t − start)) mod loopLength`. The pool is untouched. The renderer merges adjacent, phase-continuous placements of the same clip before rendering, so a split is audibly a no-op, held notes included.

**Loop-length changes.** Any command that changes a clip's loop length (`loopLength`, `stepLength`, `stepCount`, `resetAt`, `skipMask`) renormalises `offset` to `offset mod newLength` on every referencing placement, in the same undo transaction. Invariant 4 therefore holds after every command, not only after placement commands.

Under an `AutomationLane`, `clipId` must reference an event clip containing only `Event` children of kind `cc` (no notes, no other kinds). The command layer refuses to place any other clip there and refuses to add forbidden content to a clip that any AutomationLane references, whichever editor tries.

#### AutomationLanes / AutomationLane

A track-level override layer for CCs. Child order is semantic: later lanes override earlier ones where both cover the same tick and CC number.

| property | type | default | notes |
|---|---|---|---|
| `id` | uuid | — | |
| `name` | string | "" | |

A lane placement **owns** exactly the set of controller numbers that occur as enabled `cc` events with `tick < loopLength` in its clip, for the whole half-open interval `[start, start + length)`. Events at or past `loopLength` are never played, so they own nothing either — otherwise a dead event would silence the track's CC while the lane sends nothing. Ownership, exit behaviour and chase are in `CLAUDE.md → Rendering rules`. A `mode` property (`override` / `offset`) is the planned extension point; not in v1.

#### Knobs / Knob

Per-track knob assignments used by the step/drum/arrangement views (record into the playing clip) and by the Automation Recorder (record into a lane). Child order is semantic (left to right).

| property | type | default | notes |
|---|---|---|---|
| `cc` | int | — | 0–127, unique within the track |
| `label` | string | "" | |

#### Slots / Slot

Session-view launch slots. Sparse: only occupied slots are stored.

| property | type | default | notes |
|---|---|---|---|
| `scene` | int | — | row index, ≥ 0, unique within the track |
| `clipId` | uuid | — | |

`scene` is a row index, not an id — the one exception to the `<thing>Id` convention. Scene insert/delete/move commands renumber every track's slots in the same transaction. Engine commands that launch a slot carry the snapshot generation they were issued against; the engine drops a stale one rather than launching whatever now sits at that index.

### Session

| property | type | default | notes |
|---|---|---|---|
| `launchQuantise` | int64 | 3840 | launch grid in ticks from tick 0; 0 = immediate. v1 is a fixed grid, not meter-aware |

**Scene** — `index` (int, ≥ 0, unique), `name` (string). Sparse; scenes without a node have no name.

Live state (which slot is playing, playhead, which tracks are "in session") is engine state, never document state.

### UiState

Not undoable, edited through the settings path with a `nullptr` UndoManager. Saved with the project because it's part of "where I was". It holds **no clip content and no routing**, but it does hold persistent session settings that affect playback and export — transport loop, sharing policy, export length. Changing them marks the project dirty. `loopStart`, `loopEnd` and `loopEnabled` reach the engine as a transport command that the settings path sends whenever they change; they never travel in the Snapshot, which carries no transport state.

| property | type | default | notes |
|---|---|---|---|
| `activeTab` | string | "arrangement" | |
| `sharedEditPolicy` | string | "updateAll" | `"updateAll"` or `"makeUnique"` — the refcount gate's current setting |
| `loopStart` | int64 | 0 | transport loop; `loopStart < loopEnd` when enabled |
| `loopEnd` | int64 | 0 | |
| `loopEnabled` | bool | 0 | |
| `exportLength` | int64 | 0 | 0 = arrangement end (below); otherwise render length for SMF export |

**Arrangement end** is the largest `start + length` over every `Placement` on every Track and AutomationLane. Slots never count. With no placements there is nothing to export and the export command is refused with a message.

Each view may add one child node named after itself (`PianoRollState`, `StepSeqState`, …) for zoom, scroll, visible CC rows and the like. Its type and property identifiers live in `Ids.h` like every other identifier — guardrail 1 makes no exception — in a separate, clearly marked view-state section so they are visibly not document model. The Validator does not look inside these nodes. Views apply defaults for missing properties and ignore unknown ones. Adding a view-state property is not a schema bump.

## Invariants (enforced by the command layer, checked by the Validator)

1. Every `clipId`, `playerId`, `performerId` either is empty (where allowed) or resolves to an existing node of the right type. All `id`s are valid UUIDs and unique across the project.
2. `Grid` lanes each have exactly `stepCount` `Cell` children; `skipMask` has exactly `stepCount` chars; `1 ≤ resetAt ≤ stepCount`; at least one non-skipped column before `resetAt`.
3. Placements within one parent do not overlap.
4. `0 ≤ offset < loopLength` of the referenced clip, for every placement, after every command.
5. `Clip.kind` never changes; conversion creates a new clip. (Historical: tested by comparing clip ids before and after a command, not by inspecting one tree.)
6. A `Timeline` has a `TempoEvent` and a `TimeSigEvent` at tick 0, at most one of each per tick.
7. `Project.ppq == 960`.
8. Every event clip has `loopLength > 0`; `loopLength` and `quantise` are absent on step clips; every `Note` has `length > 0`; no negative tick anywhere.
9. A clip referenced by any AutomationLane placement contains only `cc` events.
10. All MIDI ranges hold (pitch, velocity, CC, bend, program, pressure, channel 1–16); `gate` is finite and in `(0, 4]`; `bpm` is finite and > 0; `sysex` payloads are valid hex of even length; `quantise`, when present, is one of the seven listed values.
11. Index-keyed children are unique in their scope: `Pad.index`, `Pad.inputPitch`, `Scene.index`, `Slot.scene` per track, `Knob.cc` per track, `CcValue.cc` per cell.
12. `Project` has exactly one of each required container; every `Track` has exactly one `Placements`, `AutomationLanes`, `Knobs`, `Slots`.

`src/model/Validator` checks all of these on a tree and reports `(node path, property, reason)`. It runs on every loaded document before installation and, in tests, after every command. It distinguishes "absent, default applies" from "present and malformed".

## Loading, saving, canonical form

**Load** is one sequence, in `io/ProjectFile`: parse the XML into a detached candidate tree → check `schemaVersion` (greater than supported: refuse with a message naming the version; the file is not touched) → run migrations on the candidate → run the Validator → only then replace the active document and reset the UndoManager. Any failure leaves the current project untouched and reports the offending node and property.

**Save** writes to a temporary file in the same directory and renames over the target (`juce::TemporaryFile`), keeps the previous file as `<name>.bak`, and reports failure without having touched the original. A dirty project is autosaved to a recovery file in Application Support every two minutes; on launch, an existing recovery file is offered. One project is open at a time.

**Canonical form.** The writer emits properties in `Ids.h` declaration order, omits defaulted properties, writes integers as decimal, doubles with `juce::String(double)`, booleans as `0`/`1`, children in stored order, UTF-8 with `\n` line endings. The guardrail *save → load → save is byte-identical* is tested on canonical output; arbitrary hand-edited XML is tested for model equivalence after one normalising pass, not for byte identity.

## Migrations

`schemaVersion` starts at 1. `src/io/Migrations.cpp` holds one function per step (`migrate1to2`, …), applied in sequence on the detached candidate tree before validation. A failed migration leaves the source file untouched. Every migration has a test with a fixture file of the old version. Bumps are for format changes (new/renamed/retyped nodes and properties), not for prose corrections or view-state additions.

## Example (schematic)

Ids are abbreviated for readability; the executable fixture `tests/io/fixtures/berlin_test.xml` uses real UUIDs and passes the Validator. One player, one track, one 5-step melodic step clip placed twice, and a one-bar drum clip in a slot.

```xml
<Project schemaVersion="1" name="Berlin test" ppq="960">
  <Routing>
    <Player id="p1" name="Minimoog" deviceIdentifier="…" deviceName="MOTU midi express 1" sendClock="1"/>
    <Performer id="k1" name="Keyboard" deviceIdentifier="…" deviceName="Keystep"/>
    <PadMap>
      <Pad index="0" inputPitch="36" pitch="36" label="Kick"/>
      <Pad index="1" inputPitch="37" pitch="42" label="Hat"/>
    </PadMap>
  </Routing>
  <Timeline>
    <TempoEvent tick="0" bpm="118"/>
    <TimeSigEvent tick="0" numerator="4" denominator="4"/>
  </Timeline>
  <ClipPool>
    <Clip id="c1" name="Bass 5-step" kind="step">
      <Grid stepLength="240" stepCount="8" resetAt="6" skipMask="00010000">
        <Lane id="l1">
          <Cell pitch="36" on="1" velocity="110" gate="0.6"/>
          <Cell pitch="36" on="1" velocity="90"  gate="0.4"/>
          <Cell pitch="43" on="1" velocity="100" gate="1.2"/>
          <Cell pitch="48" on="0"/>
          <Cell pitch="41" on="1" velocity="95"  gate="0.5">
            <CcValue cc="74" value="90"/>
          </Cell>
          <Cell pitch="39" on="1" velocity="100" gate="0.5"/>
          <Cell pitch="36" on="1" velocity="100" gate="0.5"/>
          <Cell pitch="36" on="1" velocity="100" gate="0.5"/>
        </Lane>
      </Grid>
    </Clip>
    <Clip id="c2" name="Kick+Hat" kind="step">
      <Grid stepLength="240" stepCount="16">
        <Lane id="l2" name="Kick" fixedPitch="36">
          <Cell on="1"/><Cell/><Cell/><Cell/>
          <Cell on="1"/><Cell/><Cell/><Cell/>
          <Cell on="1"/><Cell/><Cell/><Cell/>
          <Cell on="1"/><Cell/><Cell/><Cell/>
        </Lane>
        <Lane id="l3" name="Hat" fixedPitch="42">
          <Cell/><Cell/><Cell on="1" velocity="70"/><Cell/>
          <Cell/><Cell/><Cell on="1" velocity="70"/><Cell/>
          <Cell/><Cell/><Cell on="1" velocity="70"/><Cell/>
          <Cell/><Cell/><Cell on="1" velocity="70"/><Cell/>
        </Lane>
      </Grid>
    </Clip>
  </ClipPool>
  <Tracks>
    <Track id="t1" name="Bass" playerId="p1" channel="1" performerId="k1">
      <Placements>
        <Placement id="pl1" clipId="c1" start="0"    length="7680"/>
        <Placement id="pl2" clipId="c1" start="7680" length="7680" offset="480"/>
      </Placements>
      <AutomationLanes/>
      <Knobs>
        <Knob cc="74" label="Cutoff"/>
        <Knob cc="71" label="Res"/>
      </Knobs>
      <Slots>
        <Slot scene="0" clipId="c1"/>
      </Slots>
    </Track>
    <Track id="t2" name="Drums" playerId="p1" channel="10">
      <Placements/>
      <AutomationLanes/>
      <Knobs/>
      <Slots>
        <Slot scene="0" clipId="c2"/>
      </Slots>
    </Track>
  </Tracks>
  <Session launchQuantise="3840">
    <Scene index="0" name="A"/>
  </Session>
  <UiState activeTab="stepseq" sharedEditPolicy="updateAll"/>
</Project>
```

Reading `c1`: eight columns, column 3 is skipped, loop ends after column 5 (`resetAt="6"`), so the audible loop is columns 0, 1, 2, 4, 5 — five sixteenths, 1200 ticks. Column 2 has `gate="1.2"`: a 288-tick note that overlaps 48 ticks into column 4 (column 3 doesn't exist in time). Columns 6 and 7 keep their data for when `resetAt` moves. Refcount of `c1` is 3 (two placements, one slot).

The two placements: the first ends at phase `7680 mod 1200 = 480`, and the second starts at `offset="480"`, so the clip free-runs across both without a restart. The renderer merges them into one. A freshly dropped placement defaults to `offset="0"` and restarts the loop — both cases are golden tests. The drums only occupy a slot: in arrangement playback they are silent until scene A is launched.
