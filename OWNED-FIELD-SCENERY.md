# Owned test-field scenery

Windows CoE5 5.43 x64 supports placing, moving and retiring one-cell scenery in a Lua script's owned empty-editor test field. Creation delegates to the stock terrain-object constructor. It supplies a verified loaded template ID at the constructor's lookup call, then checks the generation, owner, kind, dimensions and object-layer anchor before publishing the handle. Ordinary model rows and native side-owned siege gates keep their existing lifecycles.

| API | Result |
| --- | --- |
| `testFieldEditorCapabilities().sceneryVersion` | `1` when the Windows field ABI, provenance sites and verified constructor lookup are available. |
| `testFieldSceneryTypes()` | `{version, types}`. Each supported loaded row has `monster`, `owner`, `layer`, `width`, `height`, `standable`, and `role`. Role is `wall`, `parapet`, `stairs`, or `other`. Older builds may omit role. |
| `addTestFieldScenery(monster, x, y)` | Returns the native unit ID, or `nil, reason`. Uses zero-based battle cells. The object layer must be vacant. A standable template can share the cell with an existing verified army unit if the native checks accept its full footprint. |
| `testFieldSceneryState()` | `{version, session, objects}`. Each live authored object has a unit ID, generation, loaded template, HP, owner, object-layer anchor, dimensions, creation source, and `standable`. |
| `moveTestFieldScenery(unit, session, generation, x, y)` | Moves an existing one-cell anchor with the same native generation and HP. |
| `removeTestFieldScenery(unit, session, generation)` | Permanently retires the exact owned generation. Native cleanup verifies retirement and the old cell can be reused. |
| `inspectTestFieldFeature(x, y, final)` | Opens the native read-only battlefield card for verified scenery or a fixed gate. Uses the native unit-first, object-second hit order. Final inspection requires the same live hit, anchor, layer, provenance, generation and captured actor state. |

Use the session and generation together. The mutations require an owned preparation field outside a drawing or deployment transaction. Move and Remove return `true` on success and `nil, reason` on refusal. Unverifiable creation or retirement locks the field. Callers should keep their setup and honor the normal lifecycle recovery rules. The final-state snapshot reports authored objects with `owned_scenery_placement` provenance. The API does not mutate combat AI or winner calculation.

Version 1 exposes exact stock IDs on the tested Windows ABI. Callers can apply narrower authoring policies. Each same-name variant has a separate native ID. The paired test verifies native standing, deployment and cover for every admitted building ID.

| Class | Templates |
| --- | --- |
| Standable | 25 parapets and ramparts, plus 10 stairs. |
| Blocks editor deployment | 107 walls and cliff variants, plus the `other` scenery fixture Cabbage (1634). |

`standable` enables native checks for each actor and cell. Move and exact batch deployment check eligible actor footprints. Moving or retiring an object that supports a current actor returns `scenery_occupied`. For reconstruction with a shared cell, Add temporarily stages the actor's unit-layer anchor during the native scenery constructor. Add restores the original anchor and checks the army actor record, the complete two-layer grid outside the target, the expected target cell and both native standing modes. The constructor lookup redirect is installed for that call and verified restored immediately after it. On a clean standing refusal, Add verifies retirement before restoring the original target. Unexplained changes leave the field locked and are reported to the caller.

Authoring is limited to the one-cell native identities declared in the catalog's exact stock table. Sharing a name does not grant eligibility. Mod-defined scenery, larger objects, gates, water and coastal fields are outside this contract. Stock identity checks cover name, flags, dimensions, native sprite validity and cover and range abilities. In-place edits outside those checks remain unverified. An editor can persist the exact template ID and cell, rebuild the field and verify the returned native state.

Inspect returns `true` after the card closes and the feature and cells are verified. It returns `nil, reason` for an unsupported hit, missing or changed identity, or an unavailable card. The API locks the lifecycle if it detects a mutation it cannot verify. A preparation cell covered by an ordinary unit resolves to that unit first, so callers can preserve their normal unit actions. An object drawn from the final snapshot may outlast its live identity.

Run the opt-in `tests/owned_field_scenery_variants.lua` native test in an isolated muted profile with `[run] only=plain` or `only=castle`. It checks all 143 IDs independently, including exact handle identity, duplicate refusal, native standing and deployment for both sides, cover and range abilities, generation-preserving Move, Copy, retirement and reconstruction. `tests/owned_test_field_scenery.lua` also covers construction under a living actor and rejects stale handles across field sessions.

The review build is based on CoE5SE `dev` at `de4e3a9c55150d4f59488a4da1386d5273639999` (3.8.0). Builds passed for Windows and Apple Silicon. The feature only registers on Windows. The Mac lookup is unmapped and no Mac runtime was tested.
