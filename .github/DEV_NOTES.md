# Dev notes

What `dev` has gained since the last release, one line per change. CONTRIBUTING.md, "Dev notes", shows how to add yours.

---

Bugs
- Permanently retire removed owned test-field units on Windows so regeneration cannot return them to battle.
Lua
- Owned `openTestField` accepts a Windows battlefield width floor through `minimumWidth`.
- Owned stock test fields verify native context and distinguish fixed structures from the editable roster on Windows.
- Windows owned test-field actors can have individual names for native Inspect and battle messages.
- Owned test fields can validate exact roster deployment and restore supported actor condition and spell lists on Windows.
- Owned Windows test fields can read and toggle the native combat-log overlay.
