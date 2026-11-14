# Dev notes

What `dev` has gained since the last release, one line per change. CONTRIBUTING.md, "Dev notes", shows how to add yours.

---

Lua
- coe5.set: every content record, the game's or any mod's
- LAN games: a host's content writes reach every player and stay in saves
- LAN games: battle replays run content scripts
- LAN games: clients see which mods have scripts
- monsterMod: the mod a monster belongs to
- MODS_READ: after the game reads the mods
- UNIT_STRENGTH, SQUARE_CITADEL, UNIT_FLAGS and 17 more sites: change the game's computed values
- RITUAL_CAN_CAST and RITUAL_LOCATION_CHECK: change a ritual's cast and location checks
- coe5.set: 18 more writable game values, unitAbility, unitItem, unitApLeft and aiAttackMargin among them
- coe5.nearestUnitAbilityIs: any ability
- Mac: 20 more calls

Bugs
- coe5.set: a refused write no longer logs a second not_writable line
