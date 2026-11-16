# Owned test-field API checks

The `test_owned_test_field_core.lua`, `test_owned_field_presentation.lua`,
`test_owned_field_configuration.lua`, `test_loaded_content_origin.lua` and
`test_monster_has_ranged_catalog.lua` probes target the complete candidate.
They are explicit native tests, not files to enable in a regular game profile.

Use a separate game copy and an isolated CoE5 profile. Build with the existing
`src/build.sh`, install its output only in that copy, and enable one probe as a
private Lua mod alongside CoE5SE. Create `request.ini` in that probe mod's
`coe5se_data` directory with an `[environment]` section describing the build,
then start at the main menu. Read the probe's `results.ini`; require every
test group and `[complete]` to report `PASS`. Retain failures and stop after
unverified cleanup. Do not run these probes inside a campaign.

The origin test needs a separately enabled fixture that changes an existing
item, spell and school and adds one of each; its expected fixture identities
are recorded in that probe. It deliberately distinguishes untouched,
modified and newly defined loaded content. Third-party content is optional
and is not bundled here. Presentation checks include native readback and
draw callbacks; they do not replace a visible mouse-and-keyboard check.

The two `test_field_terrain_*_test.c` executables test argument parsing and the
accepted terrain policy offline. Compile the argument test with the Lua
sources listed in `src/build.sh` and the platform's required Lua libraries
(`-lbcrypt` with the prescribed Windows toolchain). Compile the policy test
on its own. Neither is evidence of a Mac game test.
