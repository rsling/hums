# Hums – The Humane MIDI Sequencer

## What is Hums?

Hums is the MIDI sequencer for people who own a pure hardware setup and don't like – or simply don't own – synth and effect plugins. It's really just a modern MIDI sequencer without the bloat that comes with a DAW. You could still use a VSTi container on another computer or on the same computer and feed it MIDI from Hums through a physical or virtual cable. It is ensured that Hums syncs with external sequencers.

It is intended to be suitable for live performances, and special care (including hardware timing tests and loopback tests) is taken to keep timing tight.

Hums is written in C++ and uses the JUCE library extensively.

## User interface

Hums opens one project in one window. The main user interaction happens in the tabs:

| Tab | Use it for |
|---|---|
| **Routing** | Connecting tracks to MIDI ports; the drum pad map |
| **Piano Roll** | Editing notes and controller data of event clips; recording |
| **Arrangement / Session** | Laying out clips over time; launching clips and scenes live |
| **Drums** | An x0x-style drum grid |
| **Step Sequencer** | A Moog 960 / Korg SQ-10-style melodic sequencer |
| **Automation** | Recording controller sweeps over the arrangement |

## Timeline

Expects a first sort of usable GUI app (drum sequencer view only) maybe in Q1 of 2027. If we get this far, then maybe we'll see a fully working app in Q3 of 2027. No promises are made, though. It's macOS only, but there is no platform lock-in. Especially a Linux version sounds like an attractive option for further development.

## My role

I don't review code. I just make the decisions on the architecture, I review tests, and I do UI testing. The actual coding is done by Fable 5.2 (or later) in Claude Code.

## Licence

Hums is free software, licensed under the GNU Affero General Public License, version 3 (AGPLv3); the full text is in `LICENSE`. Copyright © 2026 Roland Schäfer.

Hums is built on [JUCE](https://juce.com), which is itself available under the AGPLv3 or a commercial licence; Hums uses it under the AGPLv3. Revisions of this repository from before 20 Nov 2026 were published under the BSD 2-Clause licence.
