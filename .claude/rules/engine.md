---
paths:
  - "src/engine/**"
  - "src/platform/**"
  - "src/app/TransportFacade.h"
  - "tests/engine/**"
  - "tests/platform/**"
  - "tools/miditiming/**"
---

# Engine

You are touching the engine, the platform layer or its tests, the transport facade or the timing tool. `docs/ARCHITECTURE.md` is the contract (threads, snapshot lifetime, MIDI input, timing, clock, transport). It is not loaded with this rule: Read `docs/ARCHITECTURE.md` in full before editing anything here. Once in a session is enough, unless the session has been summarised since. The engine includes only the render core and the `platform/` interfaces; OS headers appear only under `src/platform/mac/` — see `CLAUDE.md → Layers and dependencies`.

Deviating from the contract: stop and ask first; if Roland agrees, change the doc in the same commit as the code.
