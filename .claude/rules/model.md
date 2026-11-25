---
paths:
  - "src/model/**"
  - "tests/model/**"
---

# Model

You are touching the document model or its commands. `docs/SCHEMA.md` (document model, invariants) and `docs/EDITING.md` (editing semantics, refcount gate, edit context) are the contract. They are not loaded with this rule: Read both in full before editing anything here. Once in a session is enough, unless the session has been summarised since.

Deviating from the contract: stop and ask first; if Roland agrees, change the doc in the same commit as the code.
