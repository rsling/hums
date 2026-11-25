---
paths:
  - "tools/**"
  - "CMakeLists.txt"
  - "tests/guardrails/**"
  - "tests/render/golden/**"
---

# Guardrails

You are touching a guardrail script, its fixtures, the property tests, golden files, the sanitizer setup or the gate. `docs/GUARDRAILS.md` is the contract. It is not loaded with this rule: Read `docs/GUARDRAILS.md` in full before editing anything here. Once in a session is enough, unless the session has been summarised since. A guardrail is changed only on purpose, after discussion with Roland — never to make a feature land. Any other deviation from the contract: stop and ask first; if Roland agrees, change the doc in the same commit as the code.
