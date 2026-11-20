# Owned test-field scenery

Windows CoE5 5.43 x64 supports placing, moving and retiring one-cell scenery in a Lua script's owned empty-editor test field. Creation delegates to the stock terrain-object constructor. It supplies a verified loaded template ID at the constructor's lookup call, then checks the generation, owner, kind, dimensions and object-layer anchor before publishing the handle. Ordinary model rows and native side-owned siege gates keep their existing lifecycles.

| API | Result |
| --- | --- |
| `testFieldEditorCapabilities().sceneryVersion` | `1` when the Windows field ABI and native provenance sites are available. |
| `testFieldSceneryTypes()` | `{version, types}`. Each supported loaded row has `monster`, `owner`, `layer`, `width`, `height`, and `standable`. |
| `addTestFieldScenery(monster, x, y)` | Returns the native unit ID, or `nil, reason`. Uses zero-based battle cells. The object layer must be vacant. A standable template can share the cell with an existing verified army unit if the native checks accept its full footprint. |
| `testFieldSceneryState()` | `{version, session, objects}`. Each live authored object has a unit ID, generation, loaded template, HP, owner, object-layer anchor, dimensions, creation source, and `standable`. |
| `moveTestFieldScenery(unit, session, generation, x, y)` | Moves an existing one-cell anchor with the same native generation and HP. |
| `removeTestFieldScenery(unit, session, generation)` | Permanently retires the exact owned generation. Native cleanup verifies retirement and the old cell can be reused. |
| `inspectTestFieldFeature(x, y, final)` | Opens the native read-only battlefield card for verified scenery or a fixed gate. Uses the native unit-first, object-second hit order during preparation. Final inspection requires a captured, surviving feature with the same live generation and state. |

Use the session and generation together. The mutations require an owned preparation field outside a drawing or deployment transaction. Move and Remove return `true` on success and `nil, reason` on refusal. Unverifiable creation or retirement locks the field. Callers should keep their setup and honor the normal lifecycle recovery rules. The final-state snapshot reports authored objects with `owned_scenery_placement` provenance. The API does not mutate combat AI or winner calculation.

Version 1 exposes these exact stock IDs on the tested Windows ABI. Callers can apply narrower authoring policies.

| Class | Templates |
| --- | --- |
| Standable | Stone Parapet (Medium Cover) (1394), Stone Parapet (Heavy Cover) (1395), Palisade Parapet (1446), Wooden Stairs (1462), Stone Stairs (1511). |
| Blocks editor deployment | Stone Wall (1404), Palisade Wall (1448), Cabbage (1634). |

`standable` enables native checks and does not guarantee acceptance for every unit or terrain cell. Move and exact batch deployment check eligible actor footprints. Moving or retiring an object that supports a current actor returns `scenery_occupied`. For reconstruction with a shared cell, Add verifies and temporarily stages the actor's unit-layer anchor during the native scenery constructor, restores it and checks the actor and both native standing modes. The normal refusal path keeps both layers intact. Unverified mutation locks the lifecycle.

Authoring is limited to the listed one-cell native identities. Same-name variants and mod definitions do not acquire eligibility. Large authored objects, gates, water and coastal fields are outside its scope. In-place edits to stock records beyond the eligibility checks remain unverified. An editor can persist the exact template and cell, rebuild the field and verify the returned native state.

Inspect returns `true` after the card closes and the feature and cells are verified. It returns `nil, reason` for an unsupported hit, missing or changed identity, or an unavailable card. The API locks the lifecycle if it detects a mutation it cannot verify. A preparation cell covered by an ordinary unit resolves to that unit first, so callers can preserve their normal unit actions. An object drawn from the final snapshot may outlast its live identity.

Run the opt-in `tests/owned_test_field_scenery.lua` native test in an isolated muted profile. It covers creation, movement and retirement of all eight templates, plus reconstruction under a living actor and refusal to retire the occupied scenery. Windows CoE5 5.43 native tests matched the sampled cover values and standing modes on authored and generated stairs and parapets. Sandbox integration exercised both army teams, exact restoration with overlapping layers, and battles on Plain, Farm, Town, Castle, City and Citadel. The visible Castle test included genuine read-only Inspect and restoration after combat. The build is based on CoE5SE dev `832d5fe92b66873d58baa8b508bf9113298e7e01`. The Apple Silicon cross-build passed. Mac runtime remains unverified.
