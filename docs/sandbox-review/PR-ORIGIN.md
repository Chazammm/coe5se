# Expose observed loaded item, spell and school definition writers

What this changes, and why:

Observe accepted definition writes and bind provenance to the current loaded record. Distinguish baseline, created, modified and unknown; never infer origin from an ID range or claim full history.

Independent from official dev. Official upstream base: `df09497f04a39ff8fb7b0979dedde7dfed120c64`. Review branch: [review/sandbox-content-origin-20261115](https://github.com/Chazammm/coe5se/tree/review/sandbox-content-origin-20261115).

Current direct APIs added/adapted: `loadedContentOrigin`. Internal supporting hooks/legacy callers and exact behavior are covered by [the matrix](CAPABILITY-MATRIX.md). All operations remain explicit owned-editor calls; no campaign balance/template mutation or script access to external processes/files/network is added. Owned lifecycle hooks are lazy, rather than a new global Mod Settings feature. Existing settings remain authoritative for their existing features.

Build/native results are bound to exact source and DLL in [TEST-RESULTS.md](TEST-RESULTS.md). Windows5.43 was tested using upstream src/build.sh and llvm-mingw20260922. Mac compilation/runtime is not available here; editor operations are guarded and legacy Mac implementation retained. Content origin has portable source paths but no Mac acceptance. Capability presence and host preprocessing are not runtime proof.

Generated mapping changes, where present, are explained in the matrix: manually synchronized address indexes; the configuration stack additionally needs two generated entry-hook rows. No compiled artifact or private evidence is included.

- [x] The base branch is `dev`, not `main`.
- [ ] Every commit is signed off (`git commit -s`), agreeing to the terms in CONTRIBUTING.md.
- [x] Tested in the game on: Windows (CoE5 5.43 x64); Mac untested.
- [x] A line for this change is in `.github/DEV_NOTES.md` (a refactor needs none).

Review draft only. Sign-off remains pending; do not submit this as a compliant PR until the contributor has explicitly accepted the terms and each commit has been signed off. No PR has been opened.
