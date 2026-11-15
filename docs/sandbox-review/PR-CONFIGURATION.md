# Configure owned test-field equipment and existing base schools atomically

What this changes, and why:

Validate all twenty positions, actual compatibility, existing base schools and ordered known/memorized lists. Pin session/generation and avoid caller metatables during validation. Require explicit position/item-bound cursed removal, preserve permanent injuries live and verify rollback.

Stacked on `review/sandbox-field-presentation-20261115`. Review against that fork branch; submit/retarget to official dev only after the dependency is integrated. This is an explicit exception to the immediate dev-base checkbox. Official upstream base: `df09497f04a39ff8fb7b0979dedde7dfed120c64`. Review branch: [review/sandbox-field-configuration-20261115](https://github.com/Chazammm/coe5se/tree/review/sandbox-field-configuration-20261115).

Current direct APIs added/adapted: `setTestFieldConfiguration`, `testFieldLoadoutState`. Internal supporting hooks/legacy callers and exact behavior are covered by [the matrix](CAPABILITY-MATRIX.md). All operations remain explicit owned-editor calls; no campaign balance/template mutation or script access to external processes/files/network is added. Owned lifecycle hooks are lazy, rather than a new global Mod Settings feature. Existing settings remain authoritative for their existing features.

Build/native results are bound to exact source and DLL in [TEST-RESULTS.md](TEST-RESULTS.md). Windows5.43 was tested using upstream src/build.sh and llvm-mingw20260922. Mac compilation/runtime is not available here; editor operations are guarded and legacy Mac implementation retained. Content origin has portable source paths but no Mac acceptance. Capability presence and host preprocessing are not runtime proof.

Generated mapping changes, where present, are explained in the matrix: manually synchronized address indexes; the configuration stack additionally needs two generated entry-hook rows. No compiled artifact or private evidence is included.

- [ ] The base branch is `dev`, not `main`.
- [ ] Every commit is signed off (`git commit -s`), agreeing to the terms in CONTRIBUTING.md.
- [x] Tested in the game on: Windows (CoE5 5.43 x64); Mac untested.
- [x] A line for this change is in `.github/DEV_NOTES.md` (a refactor needs none).

Review draft only. Sign-off remains pending; do not submit this as a compliant PR until the contributor has explicitly accepted the terms and each commit has been signed off. No PR has been opened.
