# Rendering rules (document → MIDI)

Authoritative, together with `CLAUDE.md` (rules, glossary, layers) and `docs/SCHEMA.md` (document model). Items tagged **[Dn]** are defaults whose status is in `docs/DECISIONS.md`.

Implemented once in `src/render/`, used by the engine Snapshot and by SMF export. Tested exhaustively — see `docs/GUARDRAILS.md → 4. Render golden files`.

**Pipeline.** `model → ClipTemplate` (per clip, one loop period, quantisation applied, step grids expanded, chase tables derived; message thread) → `Snapshot` (templates + per-track placement plans + automation ownership bitsets + TempoMap) → `Kernel(snapshot, audibleTracks, window)` (loop arithmetic, placement merging, automation merge; engine or export) → `NoteTracker` (same-pitch ownership, pending note-offs per physical destination) → output. `audibleTracks` is the mute/solo result: the engine's live state, or the document's values at export. Export runs Kernel + NoteTracker over `[0, exportLength)` with a one-shot in-memory output.

**Per track.** Adjacent placements of the same clip whose phase is continuous (`next.offset == (prev.offset + prev.length) mod loopLength`, `next.start == prev.start + prev.length`) are merged into one before rendering. For each (merged) placement, loop the clip within `[start, start + length)` applying `offset`; emit enabled notes and events; channel = the Track's channel. Rendering ignores `playerId`.

**Note windows.** A note is emitted if its onset falls inside the placement window; a note whose onset precedes the window (before `offset`, or in a previous iteration) is not chased. A note plays its full length across the clip's loop boundary. Placement end truncates: note-off at `start + length`. Content at `tick ≥ loopLength` is never played.

**Equal-tick order** on one track: note-offs falling due, then non-note events in child order, then note-ons in child order. Within the non-note group nothing is reordered — an imported bank-select → program change sequence stays as imported. Across tracks that share a destination, tracks are processed in child order. The golden format records this order; it is not re-sorted.

**StepClip:** iterate columns in order, skip columns with the skip flag, stop at `resetAt`. Each column's `CcValue`s are emitted at the column start, on and off cells alike, before any note-on of that column. Note length = `round(gate × stepLength)` ticks, ≥ 1, in audible time. A lane with `fixedPitch` ignores cell pitches. **Consecutive on-cells in one lane with the same pitch, where the earlier gate reaches the later cell's start, merge into one note** ending where the later note would have ended (tie); a gate that doesn't reach the next cell retriggers. Overlap between *different* pitches is plain overlap (legato on a monosynth). **The merge never crosses the loop point** **[D3]**: a ClipTemplate is one loop period, so the last audible column's gate runs into the next iteration as plain overlap under the note-window rule, and when the first column has the same pitch the same-pitch rule retriggers it (note-off immediately before the note-on). A lane of equal pitches with long gates therefore retriggers once per loop instead of droning. Tying across the loop point would be a renderer change (a "continues" flag on the last note, suppression of the first note-on on later iterations, offset handling), not a schema change, if it is ever wanted.

**Same-pitch overlap** on one physical destination (port, channel, pitch), any number of tracks: the pending note-off of the earlier note is emitted immediately before the new note-on, and the earlier note's original note-off is cancelled. No stuck notes, no double note-ons, ever. Export applies the same tracker, so the file never contains an overlapping same-pitch pair either.

**AutomationLane merge:** a lane placement owns the set of CC numbers that occur as enabled `cc` events with `tick < loopLength` in its clip, over the whole placement. While owned, the track's clip CC events for those numbers are suppressed and the lane's events are emitted; later lanes win over earlier ones per CC. At placement end, and when seeking out of it, the engine sends the underlying value: the last suppressed clip CC for that number before that tick, if one exists; otherwise nothing. Seeking into an owned interval sends the lane's last value before the seek point. Live knob movements are sent regardless of ownership.

**Transport loop:** at `loopEnd` pending note-offs are sent, then controllers are chased for `loopStart` and playback continues there.

**Chase.** Each ClipTemplate carries, per CC number and for program change, its enabled events of that kind as a sorted (tick, value) list over one loop period. To chase a track at arrangement tick *t*, walk its placements backwards from *t*: inside a placement, "last value before phase *p*" is a binary search in the table; an earlier complete iteration contributes the table's final entry; then the previous placement, and so on. Stop at the first hit per CC. Automation ownership is consulted first — an owning lane placement answers from its own clip's table, and when leaving one the underlying value is the track's own chase result at that tick. Cost is bounded by placements × log events and allocates nothing, which is why the engine may do it at a seek. The same walk serves the transport loop and automation exit; SMF export never needs it, it renders from tick 0.

**SMF export:** type 1, PPQ 960, track 0 = conductor (tempo map, time signatures, markers), one SMF track per Track in track order, track-name meta event, channel from the Track, Players irrelevant. Range `[0, exportLength)` (0 = arrangement end, defined in `docs/SCHEMA.md → UiState`); notes crossing the end get their note-off at the end. Follows the document's current mute/solo, handed to the Kernel as `audibleTracks` **[D7]**. Automation lanes are merged in. No structure in meta events.

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

**Transform slot (later idea, none in v1).** Internal modulators such as an arpeggiator or chord player would sit as a per-track Transform between Kernel output and NoteTracker, so the Kernel's output stays a plain event stream and nothing in the document assumes otherwise; because the engine runs the Kernel per look-ahead window, a Transform there must carry its state across windows, allocate nothing, reset its state on seek, loop wrap and stop, and seed any randomness from the document so export and goldens stay deterministic; adding one means adding its header to the `engine/` row of the layer table and to `check_layers.py` on purpose; a later "freeze" command would render Kernel + Transform output back into a new EventClip, which "crop to new clip" does not do because it reads the document, not Kernel output. The Transform slot is render-side and does not cover live MIDI thru or input effects (the document never sees these; the engine has no input → output path, only input → recording).
