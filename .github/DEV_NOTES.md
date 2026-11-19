# Dev notes

What `dev` has gained since the last release, one line per change. CONTRIBUTING.md, "Dev notes", shows how to add yours.

---

Bugs
- Permanently retire removed owned test-field units on Windows so regeneration cannot return them to battle.
Lua
- Owned `openTestField` accepts a Windows battlefield width floor through `minimumWidth`.
- Windows owned test-field actors can have individual names for native Inspect and battle messages.
