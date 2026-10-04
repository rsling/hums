# MILESTONES.md — Hums (The Humane MIDI Sequencer) 

## Milestones

Each milestone names a demonstration and its pass criterion. Do not start UI for a later milestone early.

0. **Timing spike and toolchain.** Pin JUCE, Catch2, Xcode, CMake, deployment target. Build `tools/miditiming`. Demo: a loopback report on Roland's interface. Pass: targets under *Timing* met or renegotiated with numbers in hand. Decides timer + send path for the engine.
1. **Validated model.** Schema ids, typed wrappers, commands with edit context, Validator, canonical XML writer, atomic save, migrations skeleton, guardrails 1, 2, 3, 5 with negative fixtures. Demo: `ctest` green, property generator through 10,000 seeds. Pass: all guardrails present and known to fail on their fixtures.
2. **Render kernel and interchange.** ClipTemplate, Snapshot, Kernel, NoteTracker, TempoMap; guardrail 4 with the full case list; SMF import/export with the policy table. Demo: import a .mid, export it, show the musical diff is empty and the loss report lists what was dropped. Pass: all goldens green.
3. **First musical slice.** App shell, Routing tab, engine (transport, snapshot handoff, FIFOs, clock out), minimal arrangement display. Demo: open the Berlin fixture or an imported file, play it to hardware, stop, seek, loop, unplug and re-plug the interface, save, quit, reopen. Pass: no stuck notes, timing within targets, recovery file works.
4. **Step sequencer and drum view** (first editor), with enough arrangement to place a clip per track and press play. Demo: build the 5-step bass against 16-step drums from scratch, free-running, knob values recorded into cells. Pass: everything in SCHEMA's example reproducible by hand.
5. **Arrangement and session.** Placements (move, resize, split, crop), slots and scenes, launch rules, refcount gate with badge and make-unique, session recording to take tracks. Demo: jam a scene, record the take, play the take back against the original. Pass: take replays each destination identically.
6. **Piano roll and recording.** EventClip editing, recording from a Performer, quantise and `applyQuantise`, both conversions with loss preview. Demo: record a line, quantise it non-destructively, convert a step clip and edit it.
7. **AutomationLanes and Automation Recorder.** Ownership, exit restoration, lane priority, punch-in/out recording. Demo: record a filter sweep over a looping clip, seek into and out of it, export and verify the CC stream.

## Non-goals (for now)

Audio, plugins, MIDI 2.0 / MPE, notation, external clock sync (clock master only), tempo-change UI (the Timeline stores a map; v1 UI edits a single tempo), synth editor panels and a SysEx patch librarian (a later idea — the design must not prevent it: events are generic, SysEx is an Event kind), internal modulators such as an arpeggiator or chord player (later idea — they would sit as a per-track Transform between Kernel output and NoteTracker, so the Kernel's output stays a plain event stream and nothing in the document assumes otherwise; because the engine runs the Kernel per look-ahead window, a Transform there must carry its state across windows, allocate nothing, reset its state on seek, loop wrap and stop, and seed any randomness from the document so export and goldens stay deterministic; adding one means adding its header to the `engine/` row of the layer table and to `check_layers.py` on purpose; a later "freeze" command would render Kernel + Transform output back into a new EventClip, which "crop to new clip" does not do because it reads the document, not Kernel output), live MIDI thru and input effects — Performer → Player monitoring, input quantisation, live arpeggiation of what is being played (the document never sees these; the engine has no input → output path, only input → recording; the Transform slot above is render-side and does not cover them), Linux, Windows, scripting.

## Open questions

- Session layout: Ableton grid (tracks × scenes) or tracker-style rows? `Slots` + `Scenes` support the grid; UI undecided.
- CC envelopes: store discrete events only (current decision; a line tool writes many events) or breakpoints rendered at export?
- Performer channel filter: the schema has it; the UI may hide it.
- Whether tempo changes get any UI in v1.
- Guardrail 3: handwritten generator or a property-testing library (rapidcheck)? Start handwritten; revisit if the generator grows past a file.
- Launch quantise: fixed tick grid is v1. Meter-aware bars if varying meters ever matter.
