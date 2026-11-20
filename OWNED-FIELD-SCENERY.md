# Owned test-field scenery

Windows CoE5 5.43 x64 supports placing, moving and retiring one-cell scenery in a Lua script's owned empty-editor test field. Creation delegates to the stock terrain-object constructor. It supplies a verified loaded template ID at the constructor's lookup call, then checks the generation, owner, kind, dimensions and object-layer anchor before publishing the handle. Ordinary model rows and native side-owned siege gates keep their existing lifecycles.

| API | Result |
| --- | --- |
| `testFieldEditorCapabilities().sceneryVersion` | `1` when the Windows field ABI and native provenance sites are available. |
| `testFieldSceneryTypes()` | `{version, types}`. Each supported loaded row has `monster`, `owner`, `layer`, `width`, and `height`. |
| `addTestFieldScenery(monster, x, y)` | Returns the native unit ID, or `nil, reason`. Uses zero-based battle cells. Both layers must be vacant across the footprint. |
| `testFieldSceneryState()` | `{version, session, objects}`. Each live authored object has a unit ID, generation, loaded template, HP, owner, object-layer anchor, dimensions and creation source. |
| `moveTestFieldScenery(unit, session, generation, x, y)` | Moves an existing one-cell anchor with the same native generation and HP. |
| `removeTestFieldScenery(unit, session, generation)` | Permanently retires the exact owned generation. Native cleanup verifies retirement and the old cell can be reused. |
| `inspectTestFieldFeature(x, y, final)` | Opens the native read-only battlefield card for verified scenery or a fixed gate. Uses the native unit-first, object-second hit order during preparation. Final inspection requires a captured, surviving feature with the same live generation and state. |

Use the session and generation together. The mutations require an owned preparation field outside a drawing or deployment transaction. Move and Remove return `true` on success and `nil, reason` on refusal. Unverifiable creation or retirement locks the field. Callers should keep their setup and honor the normal lifecycle recovery rules. The final-state snapshot reports authored objects with `owned_scenery_placement` provenance. The API does not mutate combat AI or winner calculation.

Version 1 exposes stock Stone Wall (1404) and Cabbage (1634) for authoring on the tested Windows ABI. Same-name variants do not acquire eligibility. Authoring excludes mod-defined types, unit-layer actors, gates, larger footprints and water or coastal fields. In-place edits to stock records beyond the eligibility checks remain unverified. Inspect verifies the exact feature record and battlefield cells after the native card closes. Portable object serialization remains outside this API. An editor can persist the template and cell, rebuild the field, and verify the returned native state.

Inspect returns `true` after the card closes and the feature and cells are verified. It returns `nil, reason` for an unsupported hit, missing or changed identity, or an unavailable card. The API locks the lifecycle if it detects a mutation it cannot verify. A preparation cell covered by an ordinary unit resolves to that unit first, so callers can preserve their normal unit actions. An object drawn from the final snapshot may outlast its live identity.

Run the opt-in `tests/owned_test_field_scenery.lua` native test in an isolated muted profile. SandboxOfElysium also exercises search, two independent teams, editor operations and battles on Plain, Castle and Farm. Native cards for an authored Stone Wall, authored Cabbage, a map-generated Stone Parapet and a fixed Iron Gate were opened and closed on Castle. The test confirmed read-only state for each and the expected refusal for a post-battle snapshot whose Stone Wall identity was no longer verifiable. The Windows tests use the 5.43 game binary and a build based on CoE5SE dev `832d5fe92b66873d58baa8b508bf9113298e7e01`. The Apple Silicon cross-build passed. Mac runtime remains unverified.
