# Architecture: threads, timing, transport

Authoritative, together with `CLAUDE.md` (rules, glossary, layers) and `docs/SCHEMA.md` (document model). Items tagged **[Dn]** are defaults whose status is in `docs/DECISIONS.md`. The layer table and its guardrail stay in `CLAUDE.md → Layers and dependencies`.

## Threads

**Message thread** owns the ValueTree. All edits, all undo, all file I/O happen here. After any change to the undoable subtree, a rebuild produces a new Snapshot and hands it to the engine: debounced at 30 ms, with a hard ceiling of 100 ms between document change and published snapshot during continuous edits, so a knob drag never starves playback of updates. Changes under `UiState` never trigger a rebuild. Three things bypass the Snapshot and go to the engine as commands the moment they change: `Track.mute` and `Track.solo` (an `app/` ValueTree listener forwards every change, including those made by undo and redo, as a live command keyed by track id; the Snapshot carries no mute/solo state), the transport loop (`UiState.loopStart/loopEnd/loopEnabled`, forwarded by the settings path), and live knob values. Export reads mute/solo from the document and passes the audible-track set to the Kernel exactly as the engine passes its live state. Rendering ClipTemplates for large event clips may move to a background thread later; the ValueTree stays on the message thread regardless.

**Engine thread** runs the clock, drains MIDI in, runs the Kernel per look-ahead window, schedules MIDI out, sends MIDI clock. It sees:
- the current Snapshot as a raw pointer plus generation number, received through the command queue;
- a lock-free SPSC command queue from the message thread (transport including loop points, slot launch/stop with generation, mute/solo keyed by track id, live knob values, snapshot publish);
- one lock-free SPSC FIFO per MIDI input, filled on CoreMIDI's thread, drained here;
- an SPSC event FIFO back to the message thread: recorded input events (already converted to ticks), playhead, snapshot acknowledgements, overflow counters.

**Rules on the engine thread:** no allocation, no deallocation, no locks, no ValueTree, no logging, no JUCE message-thread APIs. `std::atomic<std::shared_ptr>` is not used; its lock-freedom is implementation-defined and in libc++ it isn't.

**Snapshot lifetime.** The message thread owns every Snapshot. It publishes generation *n* through the command queue; the engine switches at the next window boundary and acknowledges *n* through the event FIFO; the message thread retires every snapshot older than the last acknowledged one. On shutdown the engine is stopped and joined before any snapshot is released. A snapshot switch does not interrupt sounding notes: the NoteTracker holds pending note-offs by physical destination, independent of which snapshot produced the note-on. Events already handed to the output for the current window are committed and never recalled.

**MIDI input** arrives on CoreMIDI's thread via `MidiInputCallback`. Each input pushes into its own FIFO with the host timestamp; nothing else happens there. SysEx payloads go into a per-input byte ring (64 KB); a message over 4 KB is dropped and counted. FIFO overflow drops the event, increments a counter visible on the message thread, and marks any running take incomplete. Capacities are fixed at engine start; nothing resizes while producers run.

## Timing

Milestone 0 builds `tools/miditiming` and measures, on Roland's actual interface and Mac, with a loopback cable: scheduler wake-up jitter, output lateness relative to intended time, drift over five minutes, and drop count, under dense sixteenths on four tracks with CC traffic, with the GUI being dragged and a save in progress. Reported as median, p99, max. **Targets [D1]:** p99 lateness ≤ 1 ms, max ≤ 2 ms, zero drops. If `HighResolutionTimer` + `MidiOutput::sendMessageNow` can't hit them, the engine sends CoreMIDI packets with future host-time timestamps through a small shim in `engine/`. `MidiOutput::sendBlockOfMessages` is not assumed to schedule anything; whatever is used is measured on the pinned JUCE.

`TempoMap` converts host monotonic time ↔ ticks through the Timeline. The engine's clock is anchored by a (host time, tick) pair set at play start and at every seek; a tempo change during playback re-anchors at the current tick, so the tick count never jumps. Recorded input timestamps are converted to ticks once, on the engine thread as the input FIFOs are drained, against the current anchor, rounded to the nearest tick — a tempo edit in the middle of a take therefore puts each event where the playhead was when it arrived. The event FIFO carries ticks, never host times. The clock emits `0xF8` every 40 ticks.

**MIDI clock out** per physical port that has any `sendClock` Player: clock pulses run continuously while the app has the port, so synth LFOs and arpeggiators stay locked while stopped **[D2]**; Start on play from 0, Continue on play from elsewhere (preceded by Song Position Pointer), Stop on stop. The app is clock master only; external sync is a non-goal.

## Session and transport state

- One active source per track: a launched slot replaces that track's arrangement playback until "back to arrangement" (per track, and one global button). Automation lanes keep playing either way.
- Launching a scene launches every track's slot in that scene; an empty slot stops the track. Retriggering a playing slot restarts it at the next launch boundary.
- Launch quantise is a fixed tick grid from tick 0 (`Session.launchQuantise`), applied to launch, stop and retrigger; 0 = immediate.
- A launched clip starts at phase 0 at the launch tick and free-runs.
- Seek, loop wrap and stop: no note chase (notes that would be sounding are not restarted); pending note-offs are sent; controllers are chased — for every track and CC with a known value before the target tick (clip CC, or automation if an owning placement covers it) the engine sends that value before the first event of the new position; program changes are chased the same way. Chase reads the Snapshot's chase tables (`docs/RENDERING.md → Chase`); it is bounded by the number of placements and allocates nothing, so it runs on the engine thread at the seek. Stop also sends all pending note-offs; panic sends All Notes Off and All Sound Off on every used channel.
- Live mute/solo/knob commands carry no snapshot generation and apply immediately; a later snapshot never undoes them — it cannot, the Snapshot carries no mute/solo state. Undoing a mute arrives as one more live command through the same listener. Slot launch commands carry the generation they were issued against and are dropped if stale.
- Unplugging a bound port: pending note-offs for that destination are discarded, the Player shows "unassigned", playback continues elsewhere. Re-plugging re-binds by identifier and sends nothing retroactively.
