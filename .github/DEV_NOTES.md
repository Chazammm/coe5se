# Dev notes

What `dev` has gained since the last release, one line per change. CONTRIBUTING.md, "Dev notes", shows how to add yours.

---

Features
- LAN games over Steam! (Windows only)
- Languages: a mod's text in several languages, picked in Mod Settings
- Greek, Cyrillic and accented letters draw instead of boxes
- --dumpstrings: a translation template and report for each mod
- Optional Windows 5.43 batching for battlefields, local army views, spell particles and projectile sprites, preserving native counts, order, texture detail and fixed-function VFX sampling; restart required.

Commands
- language: your mod's text in another language
- fallbackfont: fonts for Chinese, Japanese, Korean, Thai and emoji
- newaffliction, descr and battleaff: custom afflictions
- makeaff, afftarg, cureoneaff, affres and +hasaffunithere: a custom affliction by string

Lua
- coe5.giveAffliction, coe5.cureAffliction, coe5.afflictionEffect and 7 more: give and cure custom afflictions, and their effects
- coe5.removeAffliction: cures the game's afflictions
- coe5.battleSummon: a new unit mid-battle
- coe5.battleBanishUnit, UNIT_BANISHABLE and BANISH_DESTINATION: banish a unit, a banish hit's effect and destination
- BATTLE_TARGET_SQUARE: an attack's or spell's target square
- IMMORTAL_REFORM: an immortal's reform square, monster and owner
- RITUAL_COST_PERCENT: a ritual's cost
- UNIT_HEAL_RATE: a unit's healing between turns
- coe5.wrap: every observe-only site, 259 more
- 12 income sites: each player's income, per resource
- coe5.translate: a script's text in the picked language
- unitBattleStrength, squareBattleStrength and commanderBattleStrength: battle strength's new names, the old ones still work
- unitStr: a unit's Strength
- Mac: 138 more calls, sites and values, allowMultipleUnique and coe5.inspectTestFieldItem among them

Bugs
- personalmod: view mods now run for the LAN host too
- Events: a refused if no longer runs the branches below it
- +minunits and +minterrains: count a faction with a name of several words on a plane
- Events can name a faction declared last in its file
- newcorpsetype: the game no longer hangs at a save or event
- monsterAttackValue: wasn't reading a Web
- coe5.battleSquareAbility: wasn't reading on Windows
