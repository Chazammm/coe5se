# Repeating the source checks

Use an owned CoE5 5.43 Windows x64 copy and an independently isolated profile. Do not use a campaign, normal save/profile or regular installed Extender for these probes. No game, Extender binary, third-party mod or extracted asset is provided here.

Build the selected branch with the compiler/version named in its current `src/build.sh`. Mac uses `src/build_mac.sh` and Zig0.16; it has not been built/tested in this environment. Install the outputs only into the private test copy using the upstream instructions. Keep each branch's source commit and binary hashes with its result; later documentation-only commits do not change those bindings.

The opt-in Lua files under `fuzz/` are **branch-specific acceptance probes**, not mods to enable together:

- Content-origin branch: `test_loaded_content_origin.lua` plus the explicitly activated own `test_loaded_content_origin.c5m` fixture and separately installed CrabCult. Keep the fixture mod named **ZZZContentOriginQA**, as the assertions expect. The fixture needs a locally supplied `banner.tga`; no image is bundled. Actual CrabCult has no custom spells in this installation.
- Core branch: `test_owned_test_field_core.lua` expects presentation/configuration/origin APIs to be absent, so it intentionally refuses an incorrect full-stack test target.
- Presentation branch: `test_owned_field_presentation.lua` similarly expects configuration/origin absent and requires separately installed CrabCult for the Abyss Fragment sprite check.
- Configuration branch: `test_owned_field_configuration.lua` checks raw parsing/refusal/actor identity.

Create a dedicated private QA mod with a mandatory `.c5m` icon/description and that branch's Lua probe as its entry script. Enable it only in the isolated profile. Create its own `coe5se_data/<QA-mod>/request.ini` with an `[environment]` section recording the source commit/branch, exact DLL hash, game/Extender versions and a fresh unique session identifier. The standalone probes run once at the first main-menu wait, write their own result INIs and may run native fights automatically. Do not enable them during ordinary play. The narrow Ranged check enumerates every named loaded monster before opening a field, in a field and after close; compare each native boolean and preserve unrelated no-game refusals.

For full-stack production tests, the public Sandbox repository provides the production Model/Runtime/UI and `tests/battlefield_probe.lua`, `placement_probe.lua`, `loadout_probe.lua`, `panel_probe.lua` and the own source-only `loadout-fixture.c5m`. Run their selected groups in a dedicated QA mod/profile, with current production modules copied exactly. Enable the explicit loadout fixture only for its twenty-position/custom-school/duplicate-name tests. Other people's content stays separately installed. Record every group and completion marker: FINISHED alone is not a pass, and a partial result set is not complete acceptance.

The existing SandboxOfElysium-Testen.bat is for the already prepared local development environment. It still builds that user's separate original experimental checkout; it does not download or silently select this review stack. See the Sandbox [installation guide](https://github.com/Chazammm/SandboxOfElysium/blob/main/INSTALLATION.md) and [bound report](TEST-RESULTS.md) for actual evidence and limits.

Current Sandbox requests use ordinary data tables. Reentrant legacy setup-table callbacks and pathological metamethod arguments were not part of acceptance; do not infer their correctness from the tested raw configuration-parser contract. Full native injury/HP serialization and complete render-scene retention remain unsupported.
