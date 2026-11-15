# Build and test results — current upstream contribution stack

All native tests below ran in the owned Windows x64 CoE5 5.43 process, isolated executable/profile, with independently installed CrabCult where named. They are not mocked battles or Mac evidence. Source and build are CoE5SE 3.4.4/API14 based on official dev df09497f04a39ff8fb7b0979dedde7dfed120c64. Builds used unchanged current src/build.sh with llvm-mingw20260922, compiler SHA25620ba1856942744291a6aaaf9f2c94f6d0cd7cac5aa9557fce96471dc7ef25d6f. All accepted builds completed without warnings. Binaries/logs/raw diagnostics remain private.

## Exact accepted bindings

The table preserves code/test source commits even when later commits only add review documentation. [Machine-readable sanitized bindings and group names](TEST-BINDINGS.json). FINISHED alone is not a pass: individual group results were checked. Earlier failures and incomplete starts were retained and are not in this accepted table. Some later run contexts record pending edits only to these review-result documents; source_status and the exact working-tree hash are retained in the JSON. Compiled/runtime sources remained unchanged, and the final follow-up commit changes documentation only.

| Test | Result | Source commit | Session | Plugin SHA256 |
|---|---|---|---|---|
| Ranged | Full catalog PASS:1879 records,721 ranged,0 mismatches in menu/field/after close; invalid/other no-game guards retained | `245307837306510e51bf56427add9c1ab003dc89` | `2518e863317f4c0693ce417333fb6be2` | `4496f6bdf80fe8f534a5bbc11387fe05d005c4d095d6c27a4b8660c240c6e875` |
| Content origin | 4 groups PASS | `3adfd71d72a1477329482d01a0136c7504a4d5ad` | `ea242bf7b1a24cb4a4405b1595364b60` | `7fc80e9c45d220c6d2ab0b70863d83ba35f607999e1a927932a68c5ccdff3f14` |
| Core | 7 groups PASS | `b67156dd852435ac0d3a4144a116519f1596c2bd` | `acb5f3096d52426cb6b6a12aecaa4e0c` | `b0522b820be99eca147c9361d6dcd93aa03a15b9a207954d57670b8701fecb4e` |
| Presentation | 4 groups PASS | `c2c9c3a6ab2ac72d560f0bcc938b3cbf247e0dfe` | `886d6c1738b042d5a1fee239cd18f585` | `dc5a32dd66b922679ffdd18d37f73d701f5560e0a0a50052c33e9e29bc4e0f97` |
| Configuration parser | 3 groups PASS | `fcc206fc2293af25d1878166392d476ef9aa9669` | `1ae6b9c648694f24b9db3c069d82826d` | `d987adcb65822658a572e483c32f303fea9007b4f22f21c9b389dd737377af61` |
| Configuration production | 14 groups PASS | `fcc206fc2293af25d1878166392d476ef9aa9669` | `066927aaf5804f46affbcf778de8229d` | `d987adcb65822658a572e483c32f303fea9007b4f22f21c9b389dd737377af61` |
| Combined standard (before lazy init) | 21 groups PASS | `019389868e9fe9a6269236b794d0107367204a26` | `b728fabe30f5435faf4633db368b8fad` | `e2996d24e872926ba4a4dbe4dd5219f7b5dd6c3708bf741274798a2dbeb1f737` |
| Combined fixture (before lazy init) | 14 groups PASS | `019389868e9fe9a6269236b794d0107367204a26` | `4920a83ca30440ab9e85053d02df6e6e` | `e2996d24e872926ba4a4dbe4dd5219f7b5dd6c3708bf741274798a2dbeb1f737` |
| Combined placement (before lazy init) | 4 groups PASS | `3ae06baebd6836ac628ec9b0cff898c329ebf6d5` | `b92ef5a2c7c54f868dea0fa9a26f3f64` | `e2996d24e872926ba4a4dbe4dd5219f7b5dd6c3708bf741274798a2dbeb1f737` |
| Final combined standard | 21 groups PASS | `bc273f4c26a73120827b0fa84d538fce7aaf5aad` | `1205b0ab8e8f4509afc951bb663a51ca` | `0c7d40c491a64e6891bd6e0c7f87e6ef623a7eb2c960d89a2259636a6c8d8ebb` |
| Final combined fixture | 14 groups PASS | `bc273f4c26a73120827b0fa84d538fce7aaf5aad` | `c48de6acd3b341aab3082b94fbb67172` | `0c7d40c491a64e6891bd6e0c7f87e6ef623a7eb2c960d89a2259636a6c8d8ebb` |
| Final combined placement | 4 groups PASS | `bc273f4c26a73120827b0fa84d538fce7aaf5aad` | `bc4298444d884d5da605b116d7f36a14` | `0c7d40c491a64e6891bd6e0c7f87e6ef623a7eb2c960d89a2259636a6c8d8ebb` |

## What each branch established

- Ranged: menu/field/after-close full1879-record catalog with721 native positives and no context mismatches; invalid IDs return nil, unrelated no-game operations retain refusals. The older1851/714 baseline is historical, not a new current-branch run.
- Origin: actual CrabCult Abyss Fragment300 creator/writer; an explicit own QA fixture changes low-ID item153, creates school70/spells1029/1030 and adds a spell to native Hydromancy4. Installed CrabCult has no custom spells; it was not credited for QA content. Lua itemName mutation makes origin unknown, exact restoration rebinds it; bounds/repeated queries remain read-only. Baseline means captured metadata, not all assets or valid nonempty content.
- Core: zero/either-side editor, last deletion/reuse, missing-team refusal unchanged; measured3×3 Arcane Frost Dragon1830/all9 occupied cells, blocked movement, rejected-new-unit cleanup and immediate reuse; other caster unchanged; independent unique201 actors; exact lists/invalid requests; legacy commander guard/two natural fights; Plain/Forest/Mountains/Swamp/Desert initialization and natural fights; invalid contexts. New hooks install only at explicit open, not at startup; capability query advertises support without installing it.
- Presentation: actual540 visible cell projection roundtrips,3×3/all9 preview cells and edge/outside classification, pointer mask and native dragon/Crab item sprite bounds, text focus enter/exit (not fabricated typing), two natural fights and640-cell timed captures. Existing upkeep observer runs once per fight. Final snapshots reject writes, remain immutable and clear on close. Modal Inspect/typing is covered separately by visible observations below.
- Configuration: raw table/row parsing invokes no caller metatables, rejects with verified unchanged state, generation/other actor preserved. Fourteen production groups cover actual diagnostic Crystal Heart configuration, bonus refusal, seven loadout groups and five empty-field groups. All twenty positions/custom school/duplicate spell-name identity and native combat selection are additionally tested in the complete fixture build.
- Combined: standard groups cover actual production runtime/configuration/empty/filter/Crab battles and all five terrains; fixture groups include twenty-position readback, duplicate names, modded provenance and actual native path70 spell selection followed by restoration. Placement groups include Forest obstacles, large deletion/reuse, repeated rejection, diagnostic export, unique preflight, an explicitly injected cleanup refusal/quarantine and explicit exact-spell recovery. No ordinary native rollback failure was manufactured or relabelled as natural.

## Visible production editor test

These are actual mouse/keyboard observations, distinct from the native API groups. Product Lua is unchanged editor.8; English C5M description gives source hash8b8176fe9662c9e4f4a4a1d46bc4d4d7e5f649df4431b3adbb1487e12cd451fb. Combined plugin e2996d24e872926ba4a4dbe4dd5219f7b5dd6c3708bf741274798a2dbeb1f737, source019389868e9fe9a6269236b794d0107367204a26. Later Mac guards produced the identical Windows DLL; the subsequent lazy-startup change has a different DLL and is rechecked natively above, not claimed as a repeated full mouse pass.

Planned context937761ecbcf640f888377128423bd401 spans two desktop launches (own PIDs26120/18204): first production Sandbox menu, then a private button reading the newly saved own q4 through production decode/prepare for the fresh second launch. Its only addition is that QA entry; runtime/model/configuration are real production modules. Automatic probe fights were deferred. Requested1200×900, full visible window962×752, internal800×600. A clipped off-screen window initially caused incorrect injected native-dialog clicks; moving it fully onscreen resolved Load. This is not claimed as a product fix.

Observed:

1. Grouped setup menu and Forest selection; native field with objects. Clear changed counters to A0/B0 and removed both initial sprites. Start battle displayed “Place at least one unit for each team before fighting” and stayed in the editor.
2. Real live key search `dru`, select/place Druid's Apprentice594 on A; configure Druidism39/base1→2, learn/memorize Befriend Animal344 without losing original known spells. The explicit QA school70 is present because its fixture is active.
3. Select/equip CrabCult Abyss Fragment300 into zero-based position2 via actual anatomical chooser. Select again deselects the item. Place another Druid on B; search/select/place actual CrabCult Abyssal Shellback2004. Counters A1/B2 and three sprites agree. This Shellback is not used as evidence of a multi-cell footprint; the3×3 dragon/Rider evidence is native.
4. Save a new own q4 file and visibly Load it after the window-position correction. Both casters' ordered lists, trained level and item are in that file. No user scenario was overwritten.
5. Start actual Forest fight with native speed controls: Team B wins round6, reported wall16s, one Shellback survivor. Snapshot shows A0/B1. Right-click survivor → genuine current native Inspect: correct Shellback sprite,62/70HP, one kill. Exit verifies unchanged snapshot.
6. Click Rematch in the post-view: another natural Team B win at round6, wall29s. Back to Sandbox restores A1/B2 and saved positions; Reset works. Export Diagnostics produces diagnostics-937761ecbcf640f88837-1.ini; native issues0 and three model/native actors. Native text from the preceding fight remains in the background; it is not part of a retained complete snapshot scene.
7. Back returns to the main menu. Quit was requested; final cleanup stopped the exact verified owned process if still running. The final window inventory contains no test window. No ordinary installation/profile was touched.

Not newly clicked in this visible session: independent unique toggling/copying,3×3 deletion, every invalid placement/obstacle/drag/zoom, empty-file persistence and all item/spell pages. Those have explicit native/offline or historical evidence, not a new manual full-pass claim. The final lazy-init revision's complete mouse sequence was not repeated; its production native suites were rerun. The test fixture's custom content is not from CrabCult, and no fixture/third-party asset is bundled with Sandbox.

## Offline, platform and preservation checks

127 existing offline Lua contracts and all distributed Lua/probe syntax passed; these do not simulate or prove native compatibility. Current C terrain parser21 cases and policy51 assertions passed with the current bundled Lua/compiler. All project PowerShell and Python sources parse. The translated BAT passes -CheckOnly; a copied BAT without its helper refuses with a readable English error/nonzero exit. No regular game launch or modification occurs in that error fixture.

Mac: required Zig0.16 is not on PATH or in the checked available local tools; no Mac game/runtime exists here. No Mac build/runtime pass. Host preprocessing verifies legacy binding/platform separation, excludes Windows text/fitted UI calls and compares the exact stock legacy Git blob. This is a structural check only, not compilation. Address Mac mappings remain unavailable for new editor features.

Preservation audit: all385 original experimental source files, original dirty status/diffs/index, first three-file Ranged preparation, all88 original Sandbox profile files and three normal controls (config, game executable, installed Extender DLL) match their intake hashes. Product Lua files remain byte-identical. Sandbox changes are English documentation/consolidations, launcher strings, a C5M description and English working conventions. Historical full reports remain at their pinned Git commit; no history was rewritten. No binaries/private diagnostics/third-party assets are staged for publication.

## Failures retained and remaining limits

Retained failed attempts include a wrong assumption that installed CrabCult had custom spells; missing fixture icon; mistaken probe API shapes/monster-size assumption; an invalid BATTLE_DRAW observer instead of wrapper; missing drawText opacity; and a lazy-capability regression caught before publication. Corrected authoritative runs are listed above. No failed result was changed to PASS in place.

Full native HP/injury/affliction state is not serialized. Removal uses native tombstones, not proven complete table compaction/exhaustion handling. Final capture excludes render effects; natural conversion/transformation and arbitrary campaign/mod/platform coverage remain incomplete. No full public ready-made binary install is provided. All commits remain unsigned until explicit personal acceptance of CONTRIBUTING.md terms. No PR, upstream push, merge, contact or binary release occurred.
