---
paths:
  - "src/engine/**"
  - "src/app/TransportFacade.h"
  - "tests/engine/**"
  - "tools/miditiming/**"
---

# Engine

You are touching the engine, the transport facade or the timing tool. `docs/ARCHITECTURE.md` is the contract (threads, snapshot lifetime, MIDI input, timing, clock, transport); it is imported below. If its text is not shown below, Read `docs/ARCHITECTURE.md` in full before editing anything here. The engine includes only the render core — see `CLAUDE.md → Layers and dependencies`.

Deviating from the contract below: stop and ask first; if Roland agrees, change the doc in the same commit as the code.

@../../docs/ARCHITECTURE.md
