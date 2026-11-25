---
paths:
  - "src/views/**"
---

# Views

You are touching a view. `docs/EDITING.md` (editing semantics, refcount gate, edit context, gestures) is the contract. It is not loaded with this rule: Read `docs/EDITING.md` in full before editing anything here. Once in a session is enough, unless the session has been summarised since.

A view reads the document through the typed wrappers and never writes it, so `docs/SCHEMA.md` is not needed for every task here. Read it in full when the task depends on the structure of the document: which nodes and properties there are, what they hold, the invariants.

Deviating from the contract: stop and ask first; if Roland agrees, change the doc in the same commit as the code.
