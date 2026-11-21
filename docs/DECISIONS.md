# Decisions

Defaults that were taken by proposal and still need Roland's explicit yes, and open questions. **This file is the only place their status lives.** The text elsewhere tags each default with its id (**[D1]** …); the tag stays when the status changes.

Rules for Claude:

- A `pending` default is binding until Roland answers. Build to it; don't stall on it.
- Never re-ask an item whose status is `confirmed` or `changed`.
- Ask about a `pending` item only when the current task actually depends on it, and then record the answer here in the same session: set the status (`confirmed YYYY-MM-DD` or `changed YYYY-MM-DD: …`). If the answer is `changed`, also edit the tagged text where the item applies.
- An open question that gets answered becomes a decision: move it under *Defaults* with the next free D-number and tag the text it affects.

## Defaults

| id | default | applies in | status |
|---|---|---|---|
| D1 | Timing targets: p99 output lateness ≤ 1 ms, max ≤ 2 ms, zero drops (software loopback and hardware net of wire time; dense load; tester-produced load in M0, the app's own GUI and save load in M3) | `docs/ARCHITECTURE.md → Timing`, milestones 0 and 3 | changed 2026-11-20: software loopback unchanged (p99 ≤ 1 ms, max ≤ 2 ms); hardware net of wire time p99 ≤ 2 ms, max ≤ 3 ms; zero drops on every run. The interface itself adds 0.7–0.9 ms that no send path removes; numbers in `docs/TIMING.md`. Confirmed the same day: "net of wire time" subtracts each message's own bytes as well as the bytes queued ahead of it on the same line, because a synth can act on a message only once its last byte has arrived |
| D2 | MIDI clock pulses run continuously while the app has the port, also while stopped, so synth LFOs and arpeggiators stay locked | `docs/ARCHITECTURE.md → Timing` | pending |
| D3 | The step-clip same-pitch merge (tie) never crosses the loop point: a lane of equal pitches with long gates retriggers once per loop instead of droning | `docs/RENDERING.md → StepClip` | pending |
| D4 | Session recording lands takes on new take tracks that copy the source's Player, channel and name, placed at the record start | `docs/EDITING.md → Session recording` | pending |
| D5 | Looped recording overdubs into the same take (no new take per loop pass) | `docs/EDITING.md → Recording in general` | pending |
| D6 | A knob recorded into a StepClip stores, per column, the knob's value at that column's start | `docs/EDITING.md → Recording in general` | pending |
| D7 | SMF export follows the document's current mute/solo | `docs/RENDERING.md → SMF export` | pending |
| D8 | The step sequencer and drum view are the first editor (milestone 4), before the arrangement and piano roll | `docs/MILESTONES.md` | pending |
| D9 | The full gate (`tools/gate.sh`: plain, ASan + UBSan, TSan) runs before every commit; plain `ctest` is for the loop in between | `CLAUDE.md → Git and done`, `docs/GUARDRAILS.md → Sanitizer builds` | pending |
| D10 | No RealtimeSanitizer gate configuration in v1: the pinned Xcode 27.0 clang does not ship it (`-fsanitize=realtime` unsupported, checked 20 Nov 2026). The engine thread's no-allocation / no-lock rule may later get a compile-time check instead: `[[clang::nonblocking]]` on engine-thread entry points plus `-Wfunction-effects` as an error, which the pinned clang supports. Formerly open question Q7 | `docs/GUARDRAILS.md → Sanitizer builds`, `TOOLCHAIN.md` | confirmed 2026-11-20 |
| D11 | Milestone 0 measures one hardware interface (the U6MIDI Pro), not two; a second interface is not measured at all | `docs/ARCHITECTURE.md → Timing`, `docs/MILESTONES.md` | confirmed 2026-11-20 |
| D12 | The engine's send path is CoreMIDI with future timestamps (`platform/mac/CoreMidiScheduler`), handed over one look-ahead early; the `HighResolutionTimer` + `sendMessageNow` candidate missed the software target (p99 1.16 ms quiet) and was removed | `docs/ARCHITECTURE.md → Timing`, `CLAUDE.md → Layers and dependencies` | confirmed 2026-11-20 |
| D13 | `TempoMap` realises every tempo as an SMF holds it: `round(60 000 000 / bpm)` whole microseconds per quarter note, for playback, recording and export alike. The document keeps `bpm` as entered. Engine and SMF export therefore agree in time by construction, not only in ticks, and `miditiming --listen` can judge the app against its own export at any tempo. The price: the tempo played differs from the one entered by at most bpm × 0.0005 ms per minute (118 bpm plays as 117.9999) | `docs/ARCHITECTURE.md → Timing`, `docs/SCHEMA.md → Timeline`, `docs/RENDERING.md → SMF export`, `docs/TIMING.md → Listen mode` | confirmed 2026-11-20 |
| D14 | A double in the document (`TempoEvent.bpm`, `Cell.gate`) holds six significant digits: the canonical writer uses `juce::String(double)`, which writes exactly that in the pinned JUCE. For `bpm` that is three decimals up to 999.999 — most DAWs offer two or three, Hums offers three. A command that stores a double first rounds it to the value its saved text reads back as, so a project is the same before and after save and reload. Formerly open question Q9 | `docs/SCHEMA.md → Timeline`, `→ Grid`, `→ Loading, saving, canonical form` | confirmed 2026-11-21 |

## Open questions

| id | question | status |
|---|---|---|
| Q1 | Session layout: Ableton grid (tracks × scenes) or tracker-style rows? `Slots` + `Scenes` support the grid; UI undecided. | open |
| Q2 | CC envelopes: store discrete events only (current decision; a line tool writes many events) or breakpoints rendered at export? | open |
| Q3 | Performer channel filter: the schema has it; the UI may hide it. | open |
| Q4 | Whether tempo changes get any UI in v1. | open |
| Q5 | Guardrail 3: handwritten generator or a property-testing library (rapidcheck)? Start handwritten; revisit if the generator grows past a file. | open |
| Q6 | Launch quantise: fixed tick grid is v1. Meter-aware bars if varying meters ever matter. | open |
| Q8 | Milestone 3 timing check: is relative lateness enough, or should the app record its play-start time so `miditiming --listen` measures absolute lateness? Listen mode aligns arrivals on the best-timed message because it cannot know when the app meant to start, so a constant delay — including the interface's own 0.7–0.9 ms — drops out, and judging those figures against [D1] is about 0.8 ms more lenient on hardware than milestone 0 was. The alternative: the engine records the monotonic time of its play-start anchor (mach host time is shared by all processes) and listen mode takes it as `--start-ns`. Ask Roland when milestone 3 starts, before the engine's transport is built (`docs/TIMING.md → Listen mode`). | open |
| Q10 | Guardrail 1 catches a *schema* name spelled as a string, as `docs/GUARDRAILS.md → 1` defines it. It does not catch a name that is not in `Ids.h` at all — `node.setProperty ("brandNew", …)` passes, although `CLAUDE.md → Conventions` says no type or property name is spelled as a string anywhere else. Closing that would mean refusing every string literal in the ValueTree property and child calls; the cost is false alarms where those call names belong to something other than the document (`juce::DynamicObject`, component properties in `views/`). Found at task B.1 of milestone 1; a guardrail change, so Roland's decision. | open |
