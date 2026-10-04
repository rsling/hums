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
| D1 | Timing targets: p99 output lateness ≤ 1 ms, max ≤ 2 ms, zero drops (software loopback and hardware net of wire time; dense load; tester-produced load in M0, the app's own GUI and save load in M3) | `docs/ARCHITECTURE.md → Timing`, milestones 0 and 3 | pending |
| D2 | MIDI clock pulses run continuously while the app has the port, also while stopped, so synth LFOs and arpeggiators stay locked | `docs/ARCHITECTURE.md → Timing` | pending |
| D3 | The step-clip same-pitch merge (tie) never crosses the loop point: a lane of equal pitches with long gates retriggers once per loop instead of droning | `docs/RENDERING.md → StepClip` | pending |
| D4 | Session recording lands takes on new take tracks that copy the source's Player, channel and name, placed at the record start | `docs/EDITING.md → Session recording` | pending |
| D5 | Looped recording overdubs into the same take (no new take per loop pass) | `docs/EDITING.md → Recording in general` | pending |
| D6 | A knob recorded into a StepClip stores, per column, the knob's value at that column's start | `docs/EDITING.md → Recording in general` | pending |
| D7 | SMF export follows the document's current mute/solo | `docs/RENDERING.md → SMF export` | pending |
| D8 | The step sequencer and drum view are the first editor (milestone 4), before the arrangement and piano roll | `docs/MILESTONES.md` | pending |

## Open questions

| id | question | status |
|---|---|---|
| Q1 | Session layout: Ableton grid (tracks × scenes) or tracker-style rows? `Slots` + `Scenes` support the grid; UI undecided. | open |
| Q2 | CC envelopes: store discrete events only (current decision; a line tool writes many events) or breakpoints rendered at export? | open |
| Q3 | Performer channel filter: the schema has it; the UI may hide it. | open |
| Q4 | Whether tempo changes get any UI in v1. | open |
| Q5 | Guardrail 3: handwritten generator or a property-testing library (rapidcheck)? Start handwritten; revisit if the generator grows past a file. | open |
| Q6 | Launch quantise: fixed tick grid is v1. Meter-aware bars if varying meters ever matter. | open |
