# Windows owned test-field terrain contexts

The implementation was reviewed against `6ae712f2a4b8895d148afe45f476ebc44b09fbd0`, the CoE5SE 3.10.0 tag and upstream `dev` at intake.

## Contract

The existing `testFieldTerrains()` and `openTestField{terrain=…}` remain the admission and construction APIs. `testFieldTerrainInfo(id)` is a Windows read-only query for one loaded terrain record. It returns the exact ID, native name, terrain flags and full fort parts. `available` reports whether the owned-field policy and the required stock template admit the record. For an available record it also reports the plane and expected template, where applicable. Unavailable records retain their data and a refusal reason. Invalid and missing IDs return `nil, "bad_terrain"`.

The Windows policy admits six additional Elysium sites (Baron's Castle, King's Castle, Outpost, Motte and Bailey, Repaired Old Castle and Bronze Castle), Nexus and eleven generated Elemental citadels (Granite 210–211, Fire 212–214, Cloud 215–217 and Coral 218–220). Each planar profile captures one stock 3×3 local terrain, ground and temperature layout. With no campaign active, the editor temporarily stages those tiles on plane 8 at the owned test-field square. The game selects its battle template, builds the battlefield, replaces native gate anchors and deploys the armies. The editor verifies the plane, tiles, native template selection, effective fort parts and context flags. If a mod changes a checked terrain identity, overrides the selected map, or a required template is missing, the editor can refuse the field.

Close and early opening failure share the same restoration and readback for the borrowed plane records, tiles, center army entry, and world and battle settings. Actor retirement is checked separately. If either fails verification, the API reports `cleanup_unverified` and refuses another owned-field open. Overlapping planes can prevent staging and return `terrain_init_failed` after verified rollback.

The profiles cover deterministic local battle contexts. Campaign portal links, enchantments, dynamic weather and arbitrary generated surroundings are outside the contract. Checks use stock IDs together with the loaded names, flags and full fort selectors. They do not establish that every property of a loaded mod definition or asset still matches stock. Current official 3.10.0 and the Mac editor retain their existing terrain lists. The API targets Windows CoE5 5.43 x64. Mac runtime and other game versions are unverified.

## Validation

- Three native Elemental preview worlds observed Nexus 223 and Granite 210–211, Fire 212–214, Cloud 215–217 and Coral 218–220. The compiled contexts use real local readings from seed 73. The three worlds had varying elemental neighbors and the same Nexus center.
- Raw terrain reads matched native names, flags and selectors. Renaming Ash Field in the isolated test fixture made the Nexus context unavailable before the editor borrowed any world data. Plain still opened and closed in that fixture.
- A competing plane forced staging to fail. The probe compared the borrowed world before and after the refusal and then reopened Nexus and Plain. A changed open context also exercises native battle refusal and cleanup.
- Each of the eighteen profiles completed two native AI battles in the Sandbox integration. Tests checked defender-owned native gates, occupied gate refusal, unit placement, exact Save/Load, Back to Sandbox, Reset and Rematch. The previous experimental build was also tested visually. The Nexus card reported an Enchanted Gate at 70/70 HP and the Fire Citadel displayed magma.
- The reviewed source built with the Windows compiler and cross-compiled to Apple Silicon using the upstream build scripts. Windows runtime tests used isolated CoE5 5.43 x64 profiles and the Sandbox 0.2.1-beta.4 mod archive.

## Follow-up work

Authored gates need a single-anchor native structure operation that takes a side and preserves the correct gate state, provenance, collision and retirement. The existing map-wide gate replacement derives its owner from the defender. The neutral scenery API provides a verified one-cell object layer. Larger neutral objects need complete footprint and occupancy semantics for placement, moves, inspection and cleanup before they can be exposed.
