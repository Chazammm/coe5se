---@meta
-- CoE5SE Lua API definitions and editor hover notes for LuaLS (VS Code / Cursor), in two files.
-- coe5.lua, this file: how scripts run, the cheat sheet, the game's words and numbers, the sites
-- to wrap and observe, and the game values coe5.get and coe5.set take.
-- coe5_calls.lua: every coe5.* call, with the tables they take and return.
-- CoE5SE never executes either file: it runs only <ModName>.lua, <ModName>_commands.lua, and
-- files they load with require(). Place both beside your script for autocomplete and hover
-- documentation.
--
-- ================================================================================================
-- 1. Script files and load order
-- ================================================================================================
-- Only enabled mods run scripts.
-- - <ModName>_commands.lua loads first, before the startup mod pass reads any mod content,
--   in the same Lua state as <ModName>.lua. Keep coe5.addModCommand and command/event hooks
--   (coe5.alterCommand, coe5.alterCommandAfter, coe5.alterEventTrigger, coe5.alterEventAction)
--   here.
--   While it loads, only those calls plus coe5.log are allowed on coe5.*; any other coe5.*
--   call returns nil (lua_refused reason=commands_file). require, print, warn, and standard
--   Lua libraries work normally. If _commands.lua errors, <ModName>.lua does not load.
-- - <ModName>.lua loads on the first main menu frame after all DLLs start, and runs its
--   top-level code once. It sees globals defined by _commands.lua, and
--   require("<ModName>_commands") returns that file's return value.
--
-- ================================================================================================
-- 2. Sandbox, logging, and limits
-- ================================================================================================
-- Libraries: Lua 5.5 base (without load, dofile, loadfile, or collectgarbage; with CoE5SE's
-- sandboxed require, print, and xpcall), plus string, table, math, utf8, and coroutine.
-- There is no io, os, package, or debug. File access is limited to require() inside your
-- mod folder, per-mod INI files (coe5.readIni, writeIni, listIni, deleteIni), and
-- coe5.saveNewMod (writes a new mod folder into the mods directory).
--
-- Logging: coe5.log and print write lua_log / lua_print lines to coe5mod.log while CoE5SE's
-- "Enable copious logs" (script_log) setting is enabled (off by default). warn writes
-- lua_warning unless disabled with warn("@off"). CoE5SE lifecycle and error lines
-- (lua_script_loaded, lua_error, lua_refused) are always written.
--
-- Step budget: Each outermost call into a script gets 20,000,000 Lua steps, counting
-- resumed coroutines, __gc finalizers, library loops (table.move, concat, unpack, sort,
-- insert, remove; string.find, match, gmatch, gsub at 1 step per element/byte/step), and
-- 10,000 steps per coe5.route call. Inside a long call, the budget resets at coe5.ui.wait
-- and coe5.ui.nameBox, at next() if a frame passed while it ran, and at a nested call after
-- a frame passed. Exceeding the budget stops the script permanently
-- (lua_refused reason=step_limit); pcall, xpcall, __close, and coroutines cannot catch it.
--
-- Memory cap: 1.5 GB per script (stops that script with memory_cap), 6 GB across all
-- scripts combined (stops the script holding the most memory with memory_total).
-- Calling coe5.* outside a script callback (such as in a __gc finalizer while CoE5SE closes
-- a stopped script) returns nil (reason=not_running).
--
-- ================================================================================================
-- 3. Battle and LAN determinism
-- ================================================================================================
-- Battles run on the host (or in single player) and are refought identically in replays:
-- - Inside a wrap or observer of any site that fires while a battle's rounds run
--   (excluding draw and screen sites),
--   math.random uses a seed derived from the battle and mod name, math.randomseed is
--   ignored, and pairs()/next() iterate keys in deterministic sorted order: numbers,
--   strings, false, true, then table/function/userdata keys in unspecified order
--   (logged as lua_battle_pairs_unordered).
-- - During a battle replay, coe5.gameData and coe5.playerData hold their pre-battle
--   snapshot and restore when the replay ends.
-- - In a LAN game, content scripts share a deterministic math.random seed per load phase,
--   and string keys iterate in the same order on every machine for identical inserts.
--
-- ================================================================================================
-- 4. Refusals and errors
-- ================================================================================================
-- Refusals return false, nil, or coe5.message.notAdded (noted per function) and write
-- api_refused or lua_refused with reason=<word> to coe5mod.log. Search for "_refused".
--
-- Errors (wrong argument type, unknown name, read-only write, step limit) log lua_error
-- and stop the script until the game restarts, the mod is toggled off and on, a LAN game
-- starts, the next LAN turn/month rebuilds a stopped script, or lan_rehearsal reloads it.
-- Stopped wraps pass calls on to next (so other mods' wraps still run; a wrap that errors
-- returns whatever next() returned). An active outer call finishes without adding callbacks
-- (reason=script_stopped), and setupAutomated is cleared. Wrap risky code in pcall (only
-- step_limit, memory_cap, and memory_total cannot be caught). Tracebacks report
-- <ModName>.lua:<line> (or subfolder/file.lua:<line>), never full disk paths.
--
-- Buttons, Game Menu lines, and wraps grow dynamically with no fixed cap.
--
-- ================================================================================================
-- 5. Cheat sheet
-- ================================================================================================
--
-- -- Files, settings, and logging --
--   Require a file in mod folder   local safety = require("gatherer.safety")   (gatherer/safety.lua)
--   Log a line                     coe5.log("hello")
--   Read a .c5se setting           coe5.setting("welcome")   (on/off: bool; choice: 0-based index; key: key code)
--   Translate text                 coe5.translate("Rolled %d of %d")   (uses language blocks; falls back to input)
--   Read / write mod INI file      coe5.writeIni("last", {Player1 = {Class = 3}}), coe5.readIni("last").Player1.Class
--   Copy text to clipboard         coe5.copyText("newweapon \"Laser\"\n")
--   Save text as a new mod         coe5.saveNewMod("My Laser", "newweapon \"Laser\"\ndmg 12\n")
--   Measure elapsed time (ms)      local t0 = coe5.get("ticksMs") ... coe5.get("ticksMs") - t0
--   Filter engine trace lines      coe5.set("traceLevel", 2); coe5.keepTraceLines({"ailog", "aiship"})
--
-- -- Hooking, game values, and LAN state --
--   Wrap a game call               coe5.wrap("PLAY_GAME_NEW_WORLD", function(next, mode) return next(mode) end)
--   Wrap outside other scripts     coe5.wrap("PLAY_GAME_NEW_WORLD", function(next, mode) return next(mode) end, 100)
--   Check which wraps changed it   coe5.wrap("AI_CONQUER", function(next, c, comstr) local s, x, y = next(c, comstr) for _, ch in ipairs(coe5.changedBy()) do coe5.log(ch.mod) end return s, x, y end, 2000000000)
--   Observe a game call            coe5.observe("MONTH_START", nil, function(result, kind, text) coe5.log(coe5.turn()) end)
--   Read / write a game value      coe5.get("society"), coe5.set("society", 3)
--   Read a player slot value       coe5.get("playerName", 0)
--   Add 10 Gold to a player        coe5.set("playerResource", p, 0, coe5.get("playerResource", p, 0) + 10)
--   Named game constants           coe5.controller.human, coe5.gameOverMode.plain, ... (tables at bottom of file)
--   Check LAN role and active seat coe5.netRole() == coe5.net.client; coe5.localPlayer()   (nil while waiting)
--   Persistent game state          coe5.gameData.count = (coe5.gameData.count or 0) + 1   (saved in saves and sent on LAN turns)
--   Per-player persistent state    coe5.playerData[coe5.localPlayer()].asked = true   (LAN client writes reach host)
--
-- -- Menus, messages, hotkeys, and UI --
--   Add a main menu button         coe5.addMainMenuButton("My Mod", openScreen)
--   Add a button after Mods        coe5.addMainMenuButton("My Mod", openScreen, coe5.mainMenu.mods)
--   Hide the Read Tip button       coe5.hideMainMenuButton(coe5.mainMenu.readTip, function() return true end)
--   Add a Game Menu line           coe5.addGameMenuLine{label = "Resign now", onClick = function() return coe5.gameMenu.resign end}
--   Add a line under Settings      coe5.addGameMenuLine{label = "My Options", onClick = openOptions, section = coe5.gameMenuSection.settings}
--   Add a line in a custom section coe5.addGameMenuLine{label = "Resources", onClick = openResources, section = "Cheats"}
--   Restart into a new game        coe5.restartGame()   (inside a Game Menu line's onClick)
--   Pick and load a save           coe5.loadGame()      (inside a Game Menu line's onClick)
--   Add a Mod Settings screen      coe5.addScreen("Open My Mod", openScreen)
--   Rebind a vanilla key           coe5.rebindKey(coe5.keyAction.mapEndTurn, string.byte("k"))
--   Send a Messages entry          coe5.sendMessage(coe5.message.everyHuman, "Hello", "Welcome back.")
--   A message with a Go to         coe5.sendMessage(p, "A Hero", "Squint has risen.", {unit = u})   (selects a commander's)
--   Show a blocking message box    coe5.showMessageBox("Sealed Gate", "The gate is sealed.")   (human turn only)
--   Post World News                coe5.worldNews("The Ring has been found")
--   Prompt for a map square        local x, y = coe5.chooseLocation("Where shall the Ring go?", p, x0, y0, 0)
--   Prompt yes / no                if coe5.confirm("Destroy the Ring?") then ... end
--   Prompt for a number            local n = coe5.numberBox("Tribute", "How much gold?")
--   Open a vanilla screen          coe5.openPlaneInfo(4), coe5.openRitualScreen(unit)
--   Build a custom screen          coe5.ui.screenBegin("My Mod") ... coe5.ui.wait()
--   Draw a table row               coe5.ui.rowBegin(30, 400, 0.6, 0.5, 0.2, 0.4) coe5.ui.text(30, "Hermann") coe5.ui.columnX(200) coe5.ui.text(30, "275 (+54)") coe5.ui.rowEnd()
--   Draw a button                  coe5.ui.button(32, "Generate", 300, 28, 0.16, 0.38, 0.66)
--   Draw a resource icon in a row  coe5.ui.rowBegin(31, 400) coe5.ui.resourceIcon(31, 0) coe5.ui.tooltip("Gold") coe5.ui.text(31, "275") coe5.ui.rowEnd()
--   Draw a monster sprite row      coe5.ui.monsterRow(20, coe5.findMonster("Spearman"), "Spearman", "Recruit")
--   Read text input on a screen    coe5.ui.setTextFocus(true) each frame, then coe5.ui.takeTextEvents().events
--   Add a commander order line     coe5.addCommanderLine{label = "Rally", icon = 0x75, onClick = function() local c = coe5.get("selectedCommander") end}
--   Add a map editor toolbox line  local id = coe5.addMapEditorLine{label = "Show grid", onClick = function() on = true end}
--   Draw over the map editor       coe5.wrap("MAP_EDITOR_DRAW", function(next) local v = coe5.mapView() coe5.ui.drawLine(v.originX, 0, v.originX, 600, 1, 1, 1, 0.3) return next() end)
--   Draw over the world map        coe5.wrap("MAP_DRAW", function(next) coe5.ui.drawRect(10, 400, 200, 60, 0, 0, 0, 0.6) coe5.ui.drawText(16, 440, 12, "Hermann", 1, 1, 1, 1) return next() end)   (Windows)
--   Draw a resource icon on map    coe5.wrap("MAP_DRAW", function(next) coe5.ui.drawResourceIcon(16, 420, 12, 5) coe5.ui.drawText(30, 420, 12, "31", 1, 1, 1, 1) return next() end)   (Windows)
--   Read hovered map square        coe5.get("mapPointerX"), coe5.get("mapPointerY")   (in MAP_SCREEN_WAIT after next)
--   Consume a map click            coe5.set("mouseButtons", 0)   (in MAP_SCREEN_WAIT after next)
--   Click custom map overlay text  coe5.wrap("MAP_SCREEN_WAIT", function(next, b, m) local id = next(b, m) local x, y = coe5.get("uiPointerX"), coe5.get("uiPointerY") if coe5.get("mouseButtons") == 1 and x >= 10 and x < 240 and y >= 300 and y < 336 then coe5.set("mouseButtons", 0) return 0 end return id end)   (Windows)
--   End a turn from a script       coe5.wrap("MAP_SCREEN_WAIT", function(next, b, m) local id = next(b, m) if skipping then return coe5.mapAction.endTurn end return id end)
--   Close the Messages screen      coe5.wrap("MESSAGES_WAIT", function(next, b, m) if skipping then return coe5.box.messagesOk end return next(b, m) end)
--   Dismiss End Turn Warning       coe5.wrap("MESSAGE_BOX_WAIT", function(next, b, m) if skipping and coe5.get("messageBoxText"):find("AP left") then return coe5.box.firstButton end return next(b, m) end)   (Windows)
--   Override top bar income text   coe5.wrap("TOP_BAR_INCOME", function(next, p, r, income) next(p, r, income) return coe5.get("playerAiIncome", p, r) end)   (Windows)
--   Wrap square panel icons        coe5.wrap("SQUARE_PANEL_WRAP", function(next, right, limit, line) next(right, limit, line) return line < 3 and 1 or 0 end)
--
-- -- World, planes, portals, and events --
--   Edit squares during world gen  coe5.wrap("PLAY_GAME_NEW_WORLD", function(next, mode) local r = next(mode) coe5.set("squareTerrain", x, y, t) coe5.set("squareName", x, y, "Old Tower") return r end)
--   Skip random terrain decoration coe5.wrap("DECORATE_SQUARE", function(next, x, y) if coe5.get("squareTerrain", x, y) ~= t then next(x, y) end end)
--   Check square plane and portals coe5.get("squarePlane", x, y), coe5.get("squarePortal", x, y), coe5.portalExits(x, y)
--   Block a portal crossing        coe5.onPortalCrossing(function(c) if c.portal == 1234 then return false end end)   (humans, AI, and routes)
--   Same spot on another plane     coe5.planeSameSpot(2, x, y)
--   Tint a plane's map             coe5.setPlaneTint("Inferno", 1, 0.5, 0, 0.2), coe5.clearPlaneTint("Inferno"); local r, g, b, s = coe5.get("planeTint", 4)
--   Reveal the map for a turn      coe5.revealMap(coe5.get("activePlayer"))   (one plane: coe5.revealMap(p, coe5.planeIndex("Inferno")))
--   Center the camera on a square  coe5.centerMap(x, y)   (switches plane too)
--   Disable random world events    coe5.setRandomEventsOff{"meteor", "drought"}; local off = coe5.randomEventsOff()
--   Queue a world event at turn end coe5.queueGameEvent("drought")   (coe5.gameEvents() lists names)
--   Run a mod event immediately    local e = coe5.modEvents()[1]; coe5.runModEvent(1)   (squareevent: coe5.runModEvent(i, x, y))
--   Keep a Seven Seal from breaking coe5.wrap("SEAL_BREAK", function(next, slot, seal) if seal ~= 3 then return next(slot, seal) end coe5.set("eventVar", 300, 1) end)
--   Replace a seal's monthly event coe5.wrap("SEAL_EVENT", function(next, slot, seal) if seal ~= 3 then return next(slot, seal) end coe5.set("eventVar", 300, 1) end)
--   Configure custom terrain looks coe5.setTerrainLooks{terrain = "Dungeon Wall", ground = "Cave Wall", picture = "./wall.tga"}   (at load)
--   Give forests a mod's trees     coe5.setSpritePack("Fangorn", "./fangorn/"); coe5.setTerrainScatter("Forest", {{pack = "Fangorn", count = 40, size = 50}})   (at load)
--   Make a terrain a forest        coe5.setTerrainForest(593, {fullforest = true})   (at load)
--   Add a battle map for a terrain coe5.addBattleMap{file = "./maps/moria_halls.coem", terrains = {"Moria Deeps"}, weight = 20}   (at load)
--   Set terrain battle scatter     coe5.setBattleScatter{terrain = "Moria Deeps", objects = {{"Mithril Vein", 20}}}   (at load)
--   Add a decked vehicle           coe5.addTransport{monster = "War Wagon", battlemap = "./maps/wagon.coem", heart = "Wagon Driver Seat"}   (at load)
--   Add a site-carrying vehicle    coe5.addTransport{monster = "Sky Galleon", terrain = "Port"}   (at load)
--   Add a land/air carrier         coe5.addTransport{monster = "Sky Galleon", land = true, air = true}   (at load)
--   Add an independent faction     coe5.addFaction{name = "Ash Court", like = "Inferno", home = "Ash Plane", color = {0.8, 0.3, 0}}   (at load; coe5.factionPlayer("Ash Court") in play)
--
-- -- Units, commanders, rituals, and ceremonies --
--   Spawn a unit on a square       coe5.spawnUnit(p, coe5.monsterVariant(coe5.findMonster("Spearman")), x, y)
--   Teleport a commander's army    coe5.teleportCommander(c, x, y)   (coe5.mayEnterSquare(c, x, y) checks walkability)
--   Apply a custom affliction      coe5.giveAffliction(unit, "Chestburster Embryo")   (newaffliction in .c5m)
--   Get a commander's square       local u = coe5.get("commanderUnit", c); coe5.get("unitX", u), coe5.get("unitY", u)
--   Get a unit's commander         coe5.get("unitCommander", u)   (-1 for none)
--   Walk every unit on a square    for _, u in ipairs(coe5.squareUnits(x, y)) do local s = coe5.get("unitBattleStrength", coe5.get("unitOwner", u), u) end
--   Check unit visibility          coe5.get("unitVisibleTo", u, p)   (stealth, invisibility, spiritsight; squareSeen for fog)
--   List armies visible to player  for _, a in ipairs(coe5.armies(p)) do coe5.log(a.x .. "," .. a.y .. " " .. a.owner .. " " .. a.units) end
--   Check flying / sea movement    coe5.get("monsterMoveFlags", coe5.get("unitMonster", u)) & 1 ~= 0; coe5.get("commanderMoveFlags", c) & 192 ~= 0   (unitMoveFlags adds items)
--   Read ability by mod command    coe5.get("monsterAbility", m, "aihold"), coe5.get("unitAbility", u, "mr")
--   Read class recruits            coe5.get("classRecruitName", coe5.get("playerClass", p), 0)   (nil past last)
--   Read a commander's rituals     coe5.get("commanderRitual", c, 0), coe5.get("ritualString", r, 0)   (nil past last)
--   Read ritual costs and effects  coe5.get("ritualCostType", r, 0), coe5.get("ritualTerrainName", r), coe5.get("ritualEffect", r, 0)
--   Check if caster can pay cost   coe5.get("commanderCostOnHand", c, coe5.get("ritualCostType", r, 0)) >= amount
--   Read caster ritual level       coe5.get("commanderRitualLevel", c)   (school: commanderRitpow)
--   Check terrain income / match   coe5.get("terrainIncome", coe5.get("squareTerrain", x, y), 3); coe5.terrainMatch(coe5.get("ritualTerrain", r), x, y)
--   Modify square library level    coe5.get("squareLibraryLevel", x, y); coe5.wrap("SQUARE_LIBRARY_LEVEL", function(next, p, x, y) return next(p, x, y) + 1 end)
--   Treat a square as water        coe5.wrap("SQUARE_WATERLIKE", function(next, x, y) return next(x, y) or (x == 12 and y == 30) end)
--   Check if commander pressed Space coe5.get("commanderDone", c)
--   Block or extend a ceremony     coe5.wrap("CEREMONY_PERFORM", function(next, ceremony, c, target) if coe5.get("ceremonyName", ceremony) ~= "Call the Inquisition" then next(ceremony, c, target) end end)
--   Change a ceremony's cost       coe5.wrap("CEREMONY_COSTS", function(next, c) next(c) coe5.set("ceremonyCost", n, 30) end)   (match n via ceremonyName)
--
-- -- Battles and AI --
--   Read unit battle coordinates   coe5.get("unitBattleX", unit), coe5.get("unitBattleY", unit)
--   Aim an attack at a battle cell coe5.wrap("BATTLE_TARGET_SQUARE", function(next, u, spell, weapon, t, v) if weapon == dart then return 1, x, y end end)
--   Strike sooner                  coe5.wrap("ATTACK_INIT", function(next, unit, slot, weapon, init) return next(unit, slot, weapon, init) + 2 end)
--   Summon a unit mid-battle       coe5.battleSummon(coe5.findMonster("Wolf"), player, x, y)   (5th arg true: stays after battle)
--   Modify a unit's armor          coe5.wrap("UNIT_ARMOR", function(next, unit) return next(unit) + 5 end)
--   Check AI route cost            coe5.route(c, x, y)   (nil for no route; inside an AI site wrap only)
--   Redirect AI exploration        coe5.wrap("AI_EXPLORE", function(next, c, comstr) local s, x, y = next(c, comstr) return s, x, y end)
--   Veto an AI attack              coe5.wrap("AI_CONQUER", function(next, c, comstr) local s, x, y = next(c, comstr) if risky(x, y) then return -999 end return s, x, y end)
--   Move a resting AI commander    coe5.wrap("AI_RALLY", function(next, c, comstr) local s, x, y = next(c, comstr) if s <= 0 then return 1, hx, hy end return s, x, y end)
--   Inspect AI conquest choice     local score, x, y = coe5.aiConquerStep(c, strength)   (Windows only for now)
--   Veto an AI ritual plan         coe5.wrap("AI_RITUAL_CANDIDATE", function(next, c, ritual) if blocked(c, ritual) then return false end return next(c, ritual) end)
--   Redirect an AI ritual target   coe5.wrap("AI_RITUAL_TARGET", function(next, c, ritual) local found, x, y = next(c, ritual) if found == 1 then x, y = better(c, ritual, x, y) end return found, x, y end)
--   Plan / scrap an AI ritual      coe5.planRitual(c, coe5.get("commanderRitual", c, 0)); coe5.scrapRitualPlan(c)   (inside AI sites; planRitual requires no current plan)
--   Release / transfer AI follower coe5.releaseFollower(u); coe5.transferFollower(u, c)   (inside AI sites)
--   Control AI troop gathering     coe5.wrap("AI_GATHER_TROOPS", function(next, c) if not leader(c) then next(c) end end)
--   Block AI loner pickup          coe5.wrap("AI_PICK_UP_LONER", function(next, c, unit) if busy(unit) then return 0 end return next(c, unit) end)
--   Change AI army split threshold coe5.wrap("AI_SPLIT_ARMY", function(next, c, minUnits) next(c, 30) end)
--   Adjust AI routing strength     coe5.wrap("AI_MOVE_COMSTR", function(next, c) return next(c) + helpers(c) end)
--   Allow AI across frozen water   coe5.wrap("AI_DANGEROUS_STEP", function(next, c, dx, dy) if frozenAhead(c, dx, dy) then return 0 end return next(c, dx, dy) end)
--   Change AI guard count          coe5.wrap("AI_MAKE_GUARDS", function(next, p, x, y, n) next(p, x, y, n * 2) end)
--   Hold independent armies        coe5.wrap("MOVE_INDEPENDENT_ARMY", function(next, p, x, y) if x < 100 then return end return next(p, x, y) end)
--   Pick a random enemy (AI way)   coe5.aiRandomEnemy(p)   (-1 for none)
--   Read next-turn & AI income     coe5.get("playerIncome", p, 3); coe5.get("playerAiIncome", p, 0) + coe5.get("playerAiTrade", p, 0)
--   Run vanilla AI after human     coe5.observe("HUMAN_TURN", nil, function(_, p) coe5.runAiTurn(p) end)
--   Read recruitment offers        for _, o in ipairs(coe5.shopOffers(p)) do coe5.log(o.name .. " " .. o.gold) end
--   Override AI recruiting         coe5.wrap("AI_RECRUIT", function(next, p) local sq = coe5.recruitSquares(p, offer)[1] if sq then coe5.aiBuyOffer(p, offer, sq.x, sq.y) end end)   (skipping next skips vanilla buying)
--   Buy from Recruit screen        coe5.wrap("RECRUIT_WAIT", function(next, b, m) local r = coe5.recruitScreen() ... coe5.aiBuyOffer(r.player, offer, r.x, r.y) return 0 end)
--   Modify / add shop offers       for _, o in ipairs(coe5.builtinOffers()) do if o.name == "Sage" then coe5.replaceBuiltinOffer(o.id, {gold = 20}) end end; coe5.addShopOffer{name = "Spearman", count = 10, gold = 60, chance = function(p) return coe5.guildCount(p, "Mercenaries Guild") * 10 end}
--   Inspect AI buy history         for _, a in ipairs(coe5.aiActions(p)) do coe5.log(a.mod .. " " .. a.call) end
--   React to Horror spawns         coe5.observe("POP_HORRORS", function(chance) coe5.log("chance " .. chance) end)
--
-- -- Content records and custom commands --
--   Read a content record field    coe5.get("monster", m, "hp"), coe5.get("terrain", t, "citadel"), coe5.get("class", c, "startplane")
--   Edit a content record          coe5.set("weapon", w, "dmg", 12)   (saved in saves)
--   Edit records when mods load    coe5.observe("MODS_READ", nil, function() coe5.set("monster", m, "hp", 30) end)
--   Check which mod added a monster coe5.get("monsterMod", m)   (nil for vanilla)
--   Add a custom .c5m/.c5se word   coe5.addModCommand("mymod_gold", function(args, mod, level) ... end)   (_commands.lua only)
--   Alter a vanilla command        coe5.alterCommand("hp", function(t) if t.args == "0" then return "hp 1" end end)   (_commands.lua only)
--   Override an event trigger      coe5.alterEventTrigger("minunitshere", function(t) return myCount(t.x, t.y, t.args) end)   (_commands.lua only)
--
-- -- Test field and preview world --
--   Run a standalone test battle   coe5.testBattle(coe5.findMonster("Old Wizard"), -1, coe5.findMonster("Spearman"), 40)
--   Open a test field with armies  local ok, units = coe5.openTestField{commander = wizard, enemies = {{spearman, 5}, {wolf, 4}}}
--   Open an empty editor field     local ok, units = coe5.openTestField{emptyEditor = true, terrain = 2}   (Forest; coe5.testFieldState())
--   Build a preview world          coe5.startPreviewWorld{plane = "Agartha", seed = 7, mapSize = coe5.mapSize.medium, society = 0, seats = {0, 0}}   (main menu; Windows)
--   Dock UI panels over preview    coe5.setPreviewDocks({{x = 0, y = 0, w = 220, h = 574}})   (Windows; uiWheel, uiPointerX, mouseButtons in MAP_SCREEN_WAIT; textWidth in MAP_DRAW; setPreviewKeyField / takePreviewFieldKeys for input)
--   Show army battlefield view     coe5.viewArmy(x, y)   (Windows)
--   Switch player sight view       coe5.showSight(seat, p) ... coe5.restoreSight(seat)   (human seat; Windows)
--   Route battle reports to seat   coe5.reportBattlesTo(seat)   (with Battle reports on; Windows)

-- ================================================================================================
-- 6. Game words and numbers
-- ================================================================================================
-- The notes below use the game's words. Each is the game's or its manual's meaning:
--   ability           a numbered trait a class, a monster or an item carries: each .c5m ability word stands for one, 0 to 1501
--   affliction        a state of a unit, one bit of its state bits (listed below)
--   benefit           a boon on a unit, one bit of a second word: what a bentarg ritual or a makeben event gives (the manual's
--                     Special Benefits table)
--   battle strength   the strength the game sums for a unit, a square's units or a commander's army, which the AI and battles
--                     compare. It is not a unit's Strength (unitStr)
--   blessing          up to 24 pairs of an ability and a value on a player, which its blessed units add to their abilities
--   ceremony          a rite of a Voice of El priest, on the Ceremonies screen: Convert Settlement, Break Seal, Increase Divine
--                     Favor, Bolster Devotion, Start Crusade, Call the Inquisition, and the Ceremonies of Mastery
--   Common Cause      the Advanced Options checkbox: a player only loses when no one on its team has a citadel or a commander
--   Divine Favor      a Voice of El player's level, 0 to 4, which sets how likely El's help is once a seal is broken
--   enchantment       what a ritual's squareench puts on a square. The enchant... values list the active ones. enchantType is
--                     the enchantment's number, and its ritual names it: coe5.get("ritualName", coe5.get("enchantRitual", e))
--   freespawn         free units from El for a Voice of El player at its turn start, once a seal is broken
--   guard             a unit the AI holds on a square, which never leaves it
--   holdit            the event command that makes the independent units on a square hold
--   Horror mark       an affliction that draws Horrors to a unit
--   loner             a unit with the loner move word, which moves by itself and never in a group
--   magic path        a school of magic a spell belongs to (numbered below)
--   mastery           a commander's level in its ritual school. A ritual of mastery (levelup) turns the commander into the
--                     monster its mastery word names
--   Mirror Cloaking   a Vanara leader's ability, which makes the units on its square look like apes
--   overrun           a mover that far outnumbers the enemy on a contested square kills it without a battle
--   ritpow            a ritual school. ritualRitpow and commanderRitpow hold the manual's school number plus 500: 501 necro,
--                     505 druid
--   seals             the Seven Seals, broken one at a time by the Break Seal ceremony, an item, or the game at a month's end
--   society           Random Map Settings' Society: 0 Random, 1 Dark Ages, 2 Agricultural, 3 Empire, 4 Fallen Empire,
--                     5 Monarchy, 6 Dawn of a New Empire, 7 to 11 the sub-societies
--   special site      a terrain that does something when a commander uses it: a Stonehenge, the Nexus, a Crystal Globe
--   stars             the astrology readings the game rolls for the whole world (the stars... values)
--   stupid            the move word of a unit its player cannot control, which moves like an independent unit
--   tagalong          a unit that changed sides in a battle. The game finds it a commander when the fight ends
--   tainted magic     a world event of some months (magicTaintedMonths), while a ritual is likely to draw Horrors
--   terrain units     a battlefield's trees, scenery and ship parts: units of player 31, made for the battle
--   wanderer          a commander with the wander word, which follows its objective by itself
--
-- Players: 0 to 23 are players. The independent factions are 24 Independents, 25 Special monsters, 26 Horrors, 27 Hades,
-- 28 Inferno, 29 Celestial and 30 Kingdom/Empire. 31 holds a battle's terrain units. 32 to 47 are mods' factions.
--
-- Magic paths: 0 none, 1 Infernal Magic, 2 Elemental Magic, 3 Pyromancy, 4 Hydromancy, 5 Storm Magic, 6 Geomancy, 7 Hedge Magic,
-- 8 Unlife, 9 Dark Magic, 10 Foul Magic, 11 Blood Magic, 12 Crystal Sorcery, 13 Garnet Sorcery, 14 Jade Sorcery, 15 Onyx Sorcery,
-- 16 White Magic, 17 Spiritism, 18 Fortune Magic, 19 Solar Magic, 20 Mysticism, 21 Serpent Magic, 22 Dawn Magic, 23 Iron Arcana,
-- 24 Kuro Do, 25 Prayers, 26 Enchantment, 27 Frost Magic, 28 Dark Prayers, 29 Wizardry, 30 Shamanism, 31 War and the Sun,
-- 32 Night, 33 Rain, 34 Bloody Mother, 35 Nahualli Magic, 36 Command, 37 Black Magic, 38 Necromancy, 39 Druidism, 40 Witchery,
-- 41 Troll Magic, 42 Void Magic, 43 Illusionism, 44 Sage Magic, 45 Dwarf Magic, 46 Maladies, 47 Beast Wards, 48 Warrior Wards,
-- 49 Paladin Magic, 50 Famine Magic, 51 High Arcana, 52 Moon Magic, 53 Silver Arcana, 54 Forest Magic, 55 Alchemy, 56 Gold Arcana,
-- 57 Metal Arcana, 58 Sky, 59 Death, 60 Tree Magic, 61 Astrology, 62 Defilement, 63 Bile Magic, 64 Divine Magic, 65 Sidhe Magic,
-- 66 Akashic Magic, 67 Destruction, 68 Maya Manipulation, 69 Apothecary. 70 to 469 are mods' paths (newspellpath).
--
-- A unit's state bits (unitStateFlags, UNIT_STATE_FLAGS), each as 2 to the power of its number: 0 Diseased, 1 Cursed, 2 Decaying,
-- 3 Asleep, 4 Paralyzed, 5 Burning, 6 Stunned, 7 Profuse bleeding, 8 Earth gripped, 9 Entangled, 10 Imprisoned in ice,
-- 11 Weakened, 12 Tree form, 13 Insane, 14 Horror marked, 15 the strong Horror mark, 17 a temporary unit of a battle (removed
-- after it), 18 Confused, 19 Turned into gold, 20 and 21 Lost an eye, 22 Chest wound, 23 Never healing wound, 24 Battle fright,
-- 25 Feebleminded, 26 Blind, 27 Temporary insanity, 28 Death wish, 29 Debilitating wound, 30 Festering wound, 31 Afraid,
-- 32 Stuck in net, 33 a tagalong, 34 made stationary by an event (makestationary), 35 Exhausted, 36 Petrified, 37 flying over
-- water (the unit stats screen's Fatigued), 38 Slimed, 39 a guard, 40 Shackled, 41 Panicked, 42 a copy removed after the
-- battle, 43 Numbness, 44 reached by a ship's collapse, 45 an AI sailor, 46 has attacked. The game uses none of 47 to 63.
--
-- A ritual's flag bits (ritualFlags), by .c5m word: 1 free, 2 nostart, 4 nodemon, 8 rarestart, 16 notforpoor, 32 start,
-- 64 nexttoo, 128 noundead, 256 nofemale, 512 afterprev, 1024 gathergems.
--
-- A terrain's flag bits (terrainFlags), by .c5m word and bit number: 0 citadel, 1 burnable, 2 water, 3 desert, 4 cave,
-- 5 nostdrec, 6 ownable, 7 smoke, 8 goldboost, 9 ironboost, 10 tradeboost, 11 harvest, 12 invisible, 13 forest, 15 deepspawn,
-- 16 mountain, 17 melts, 18 nostart, 19 woodencitadel, 20 forestcitadel, 21 useable, 22 rare, 23 mine, 24 farsight, 25 nosight,
-- 26 temple, 27 void, 28 unique, 29 swamp, 30 nodrown, 31 el, 32 settlement, 33 voidret, 34 library1, 35 library2, 36 hoburg,
-- 37 snowok, 38 desertok, 39 invseason, 40 spreadcold, 45 tempimmune, 46 cityname, 48 town1, 49 town2, 51 nosnowpen, 52 farvis,
-- 53 ug, 54 nevercold, 57 hardfly, 58 port, 59 realport, 60 misty, 61 anchored, 62 seepast. coe5.get("terrain", t, word) reads
-- one by its word.

---Runs `<name>.lua` from your mod's folder, the one holding <ModName>.lua, and returns what it
---returns (true when it returns nothing). Each dot is a subfolder: require("gatherer.safety") runs
---gatherer/safety.lua. A file runs once, in your script's Lua state (its globals, step budget, and
---memory), and gets its name as `...`; a later require of the same name returns the first one's
---value. It works at load time and later. A name is 1 to 200 bytes of letters, digits, and _, a
---single dot between folders, in the case the files are stored in (on Windows too). Refused, with
---lua_refused call=require and an error to the caller: any other name (empty_name, name_too_long,
---bad_character, bad_name, reserved_name for a Windows device name such as con), a file or folder
---in another case (wrong_case), a link or junction on the way (link), and the mod's main <ModName>
---(main_script). A missing file and a loop (a file that requires itself through others) are errors
---too, and a file of bytecode doesn't load. There is no package table.
---@param name string
---@return any
function require(name) end

-- ---- Sites (wrap and observe) -----------------------------------------------------------------

-- Reading a site's name. A site is one call the game makes, and its name says which:
--   <SCREEN>_WAIT              one frame of a screen while it waits for a click or a key; next returns the id clicked.
--                              It gets (block, mode), both to pass on. block 1 holds until something is clicked; block 0
--                              returns after one frame, with 0 for no click. mode is what the game draws behind the screen:
--                              0 as before, 1 the map, 2 a battle, 3 Transfer troops, 6 the army view, 9 the Game Over
--                              fog, 10 the menus and setup screens
--   <THING>_DRAW               one frame's drawing of a screen's map or of one piece of it, where coe5.ui.drawText and drawRect draw
--   <SCREEN>_ROW               one row of a screen as the game lays it out
--   <NAME>_SCREEN, _WINDOW, _MENU   a screen, a window or a menu opens
--   UNIT_<STAT>                a number the game works out for a unit (UNIT_ARMOR, UNIT_MORALE): return another to change it
--   SQUARE_<...>, COMMANDER_<...>   as UNIT_<STAT>, for a square or a commander's army
--   PLAYER_<RESOURCE>_INCOME   what a player gets of one resource at its next turn start
--   AI_<STEP>                  one step or choice of an AI player's turn
--   BATTLE_<STEP>              one step of a battle
--   ORDER_<ORDER>              a commander's order on the map, a player's or the AI's
--   POPULATE_<PLANE>, CREATE_<PLANE>_PLANE, LOAD_<PLANE>_PLANE, MAPGEN_<STEP>, GEN_<STEP>   a step of world generation
-- A name whose note says "Named after the game's debug log line" (ADDPLANE, SET_HOLDIT, MOVE_PLAYER_STUPIDS) is
-- the game's word for that step.
-- Names that sit close:
--   DAMAGE_UNIT is a hit's damage, with the death it causes. SUBTRACT_UNIT_HP is Hit Points lost with none of a
--   hit's checks. KILL_UNIT is a death in an overrun or an elimination. REMOVE_UNIT is a unit leaving the game
--   without dying.
--   AI_MAKE_GUARDS turns units on one square into guards. AI_MAKE_DEFENDERS is the AI's step that leaves them
--   after its moves.
--   BATTLE_SETUP builds the battlefield. BATTLE_START_SETUP is the start of the fight on it.
-- A site in coe5.Site is wrapped. A site in coe5.ObservedSite is watched with coe5.observe, and wrapped where
-- its note says what a wrap may change.
-- coe5.observe(site, fn) runs fn before each call, with the call's arguments. coe5.observe(site, nil, fn) runs it
-- after, with the result first and then the arguments. A name that is only in coe5.Site is an error there, which
-- stops the script; DOATTACK and BATTLE_ADVANCE_UNIT take both.
-- A "slot" in a ritual site (RITUAL_CAN_CAST, RITUAL_COST_PERCENT) is the ritual's number, the one ritualName takes.
-- UNIT_FLAGS and UNIT_RESISTANCES are named after the game's functions: UNIT_FLAGS is how a unit moves (the value
-- unitMoveFlags reads), UNIT_RESISTANCES what it is (the value unitFlags reads).

---The game calls a script can run code around with coe5.wrap. Each note gives the function you
---pass and what `next` returns. A "frame" site runs once per frame of its screen, and its `next`
---returns the id clicked in that frame (0 for none). Lines a script draws with coe5.ui.text inside
---a wrap go onto the game's screen. A site marked "Windows only" isn't found on the Mac build
---yet: coe5.wrap returns false there.
---@alias coe5.Site
---| "MAIN_MENU_WAIT"            # fn(next, block, mode): each frame of the main menu. next returns the clicked id (coe5.mainMenu).
---| "MAIN_MENU_PLAY_GAME"       # fn(next, server): Start New Game or Load Game (server 0 in single player). next returns after the Game Over screen, or at once if the player cancels setup. Not called when the player backs out of a loaded save whose spells changed.
---| "PLAY_GAME_NEW_WORLD"       # fn(next, mode): a new game's setup and world. Mode coe5.newGame.normal, .network, or .skipParticipants. next returns 0 on Cancel. Not called for a loaded game.
---| "NEW_WORLD_MAP_SCREEN"      # fn(next): Random Map Settings. next returns coe5.newGame.mapScreenOk on Ok, 0 on Cancel.
---| "NEW_WORLD_INIT_PARTICIPANTS"  # fn(next, arg): the default single-player slots, set before Setup Participants draws; after next returns, the slot values are what the screen opens with. Pass arg on unchanged. next returns nothing.
---| "NEW_WORLD_CREATING_WORLD"  # fn(next, kind, text): "Creating World", before the classes are rolled. next(kind, text) shows the text; other text is shown as it is (a % in it isn't read as a format). next returns nothing.
---| "MAP_SETTINGS_WAIT"         # fn(next, block, mode): each frame of Random Map Settings. next returns the clicked id; coe5.newGame.mapSettingsOk is Ok.
---| "PARTICIPANTS_WAIT"         # fn(next, block, mode): each frame of Setup Participants. next returns the clicked id; coe5.newGame.participantsOk is Ok.
---| "PARTICIPANTS_DIFFICULTY_WAIT"  # fn(next, block, mode): each frame of the Difficulty Level popup, which opens from an AI player's difficulty on Setup Participants. The game passes block 1 and mode 0. next returns the clicked id: 101 to 110 pick difficulty 1 to 10 (the levels aiLevelName names), 1 is Cancel. Return 100 + a level to pick that level. "newWorldUniqueRandom" is nil here (the popup is a separate call).
---| "PARTICIPANTS_BUTTON_ROW"   # fn(next, style): starts Setup Participants' Ok row. Lines drawn before next go above the row; lines drawn after it go into the row, above Ok. next returns an integer. On Mac CoE5SE makes the row itself (the game has no call for it there), and next works the same.
---| "GAME_MENU_SAVING_SECTION_END"  # fn(next): closes the Game Menu's "Saving & Exiting" section. Lines drawn before next go at the end of the section. next returns an integer.
---| "GAME_MENU_WAIT"            # fn(next, block, mode): each frame of the in-game Game Menu (F10). next returns the clicked id; coe5.gameMenu.resign resigns. CoE5SE's wrapper, which runs the Game Menu lines, runs first.
---| "RITUAL_SCREEN_ROW"         # fn(next, id, a, b, name, costType1, cost1, costType2, cost2, costType3, cost3, costType4, cost4, hover, apCost, castable, mirrorMonster, mirrorText): one row of the ritual screen. id is the ritual's number (the slot, which ritualName takes) + coe5.ritual.rowCastable, or + coe5.ritual.rowGreyed when greyed out; hover is the reason text, and the costs are already scaled for the caster; a is the x where the cost columns end and b the number of cost columns the screen keeps free this frame, both to pass on unchanged. A costType is -1 for none, 0 to 13 a resource (resourceName), 14 humanoid corpses, 15 gems, 16 expendable units, 17 corpses, 18 goblins (incomeName names all 19); apCost is the ritual's action point cost; mirrorMonster and mirrorText are the monster a mirrortarg ritual needs and its text. A ritual in a ritualmenu reaches the wrap as a separate row; CoE5SE, inside every wrap, then draws its menu's line or nothing in its place. Each frame of the menu's picker calls this site again for each of its rituals with the game's arguments, so a wrap changes each row once a draw. hover and mirrorText may be nil; name may not. In next, a mirrorMonster past the last monster or a costType above 50 is refused (lua_refused reason=out_of_range) and the game's value kept. next returns nothing the game uses (nil).
---| "RITUAL_SCREEN_WAIT"        # fn(next, block, mode): the ritual screen waiting for a click, once a frame. The game passes block 0: the screen lays its rows out again every frame, so RITUAL_SCREEN_ROW runs every frame too. next returns the clicked id, 0 when nothing was clicked, which the screen then acts on: coe5.ritual.rowCastable + a slot casts that ritual as a click on its row does (the action point and cost checks, then the cast and its target pick), 1 is Exit, and a row's id made negative is a right click on that row (the ritual's description); any other id goes back to the frame's start. Return another id and the screen acts on it. A wrap may draw a custom screen (coe5.ui) before it returns: the ritual screen comes back scrolled as it was. A right click whose ritual is past the ritual table becomes 0 (lua_refused reason=ritual_past_table). A click on a ritualmenu line is coe5.ritual.rowMenu + n; CoE5SE, outside every wrap, then opens that line's picker and gives the screen the picked ritual's id, or 0.
---| "MAP_SCREEN_WAIT"           # fn(next, block, mode): each frame of the map screen, after the game has bound its shortcuts. next returns the clicked id; coe5.mapAction.gameMenu opens the Game Menu. A commander's button is 20000 + its index (negated too); an id for a commander past the game's last becomes 0 (lua_refused, reason=commander_past_table). After next, "mapPointerX" and "mapPointerY" hold the square under the pointer and "mouseButtons" the frame's buttons (set it to 0 to keep a click from the map). A line drawn here isn't shown: the map lays out its lines before this wait.
---| "MAP_MENU_BUTTON_TEXT"      # fn(next, text): the map's "(F10) Menu" button text. next(text) draws it; pass other text to draw that. next returns nothing the game uses (nil).
---| "SQUARE_INFO_SCREEN"        # fn(next, player, x, y, mode): the whole square info window, "(i) terrain info", for square (x, y), from its opening to Ok: the i key on the map. player is whose view it shows, and mode is 0 from the map, to pass on. Skip next to keep the window shut, or pass another square. To add lines, keep the square here, draw them in SQUARE_INFO_BUTTON_ROW before its next, and take their clicks in SQUARE_INFO_WAIT: a line drawn around this next doesn't reach the window. In next, a player that isn't a slot (0-31) is refused (lua_refused reason=out_of_range) and the vanilla value kept. next returns nothing the game uses (nil).
---| "SQUARE_INFO_BUTTON_ROW"    # fn(next, style): starts the square info window's Ok row. Lines drawn before next go above the row. next returns an integer. On Mac CoE5SE makes the row itself (the game has no call for it there), and next works the same.
---| "SQUARE_INFO_WAIT"          # fn(next, block, mode): each frame of the square info window. next returns the clicked id; the window uses coe5.screen.squareInfoOk and 2.
---| "LOAD_GAME_BROWSER"         # fn(next, title): Load Game's save list (the game's file browser, which other screens use too). next returns 1 when a save was picked, 0 on Cancel. The browser's other arguments go on unchanged.
---| "FILE_BROWSER_ROW_DATE"     # fn(next, id, text): one row's date column in the file browser. Row n's id is coe5.screen.fileBrowserRowFirst + n; the row is 450 wide. next(id, text) draws it; draw more after it with coe5.ui.columnX and coe5.ui.text. next returns an integer.
---| "FILE_BROWSER_WAIT"         # fn(next, block, mode): each frame of the file browser. Returning coe5.screen.fileBrowserDelete deletes the row the "hoveredId" value names (the browser reads it right after the wait), after the game's question. A row id past the browser's last listed row becomes 0, no click (lua_refused, reason=row_past_list or hovered_row_past_list).
---| "FILE_BROWSER_HOVERED"      # Windows only until CoE5 5.41, which inlined it as Mac always had: on neither build now, so coe5.wrap returns false. The browser reads "hoveredId" right after its wait; set it in a FILE_BROWSER_WAIT wrap to pick the row to delete.
---| "MOD_BANNER_DRAW"           # fn(next, path, x, y, w, h, alpha): one enabled mod's banner in the column on the right of the menu screens. x is the screen width - 210, y the screen height - 60 - 60 * row, w 200. Skip next to hide the banner. Pass path to next as it came: another path is refused (lua_refused reason=path_changed) and the game's banner drawn. next returns nothing.
---| "GAME_OVER_WAIT"            # fn(next, block, mode): each frame of the Game Over screen. The game passes coe5.gameOverMode.fog (fog and falling particles; mode 8 draws them too); any other mode clears to black, and coe5.gameOverMode.plain draws nothing behind the screen. next returns the clicked id.
---| "ARMY_WAIT"                 # fn(next, block, mode): each frame of Transfer troops. next returns the clicked id.
---| "BATTLE_WAIT"               # fn(next, block, mode): each frame of a battle's real-time loop. next returns the clicked id.
---| "MAP_EDITOR_WAIT"           # fn(next, block, mode): each frame of the map editor. next returns the clicked id (coe5.mapEditor, or a coe5.addMapEditorLine id). A key bound with coe5.ui.bindKey before next counts for that frame, toolbox hidden or not.
---| "MAP_EDITOR"                # fn(next): the whole map editor, opened from the Mods screen's Start Map Editor or the --mapeditor launch option; next returns when the player quits it. With --mapeditor the editor opens before the main menu: the scripts load just before it, and the game exits when it closes.
---| "MAP_EDITOR_DRAW"           # fn(next): once per frame of the map editor's map, after the map's squares and before the toolbox, the plane buttons, and the hover box. Draw with coe5.ui.drawLine, drawText and drawRect: before next lands over the map and under the toolbox, after next over everything but the hover box. coe5.mapView says where each square is. next returns nothing.
---| "MAP_DRAW"                  # fn(next): once per frame of the world map in a player's turn, after the squares and the units and before the top bar, the commander panel, and the hover box. Draw with coe5.ui.drawLine, drawText and drawRect: before next lands over the map and under the game's panels, after next over everything but the hover box. coe5.mapView says where each square is. next returns nothing. Windows only for now: on Mac coe5.wrap returns false.
---| "BATTLE_DRAW"               # fn(next): once per frame of a battle's screen, after the battlefield and its units and before the battle's GUI. Draw with coe5.ui.drawLine, drawText and drawRect here; after next, over everything. Take a click on what you drew in a BATTLE_WAIT wrap, after next, with uiPointerX, uiPointerY and mouseButtons.
---| "MESSAGES_WAIT"             # fn(next, block, mode): each frame of the Messages screen, as a human turn starts (View messages shares it). next returns the clicked id; return coe5.box.messagesOk to close it, as the player's Ok does.
---| "WARN_WAIT"                 # fn(next, block, mode): each frame of the game's warning box ("<name> has been eliminated" and the like). next returns the clicked id; return coe5.box.warnOk to press its Ok.
---| "MESSAGE_BOX_WAIT"          # fn(next, block, mode): each frame of the game's message box (the End Turn Warning, and any question the game asks with one or two buttons). next returns the clicked id, 0 for none; return coe5.box.firstButton, secondButton or cancel to answer it as the player would. Its text reads as messageBoxText meanwhile. Windows only for now: on Mac coe5.wrap returns false.
---| "TOP_BAR_INCOME"            # fn(next, player, resource, income): the "+income" the map's top bar draws after one of the player's resources (0 Gold to 13 Lifeforce, as playerResource; not the gems or the trade points). Return the number to draw; next returns the game's. Only the drawn number changes: the income the game pays, which the AI reads, stays the game's. Windows only for now: on Mac coe5.wrap returns false.
---| "SQUARE_PANEL_WRAP"         # fn(next, right, limit, line): an icon of the resource row in the square's panel at the bottom of the map (one icon for each unit of the square's income) would end at right, past limit, the screen's width, in the game's screen units, on line number line of the row (1 the first). Return 1 to start a new line with it, 0 to keep it on the line; next returns the game's 0. The panel grows up a line for each.
---| "UNIT_ARMOR"                # fn(next, unit): the unit's armor as the game works it out for this call (its unit stats screen, a weapon's hit, the battle's setup and every strength sum read it). Return the armor the game uses; next returns the game's. Another unit in next is refused (unit_changed).
---| "UNIT_RETURNS"              # fn(next, unit): whether the unit comes back from death (immortal, reforming), as the AI's conquest and gather and the damage step ask it. Return true or false (or 1 or 0); next returns the game's 1 or 0. Another unit in next is refused (unit_changed).
---| "AI_COUNT_CITADELS"         # fn(next, player): the citadels the AI counts for a player, for its gather, its move (desperate play, conquest) and the month's and the game's statistics. Return the count the game uses; next returns the game's. Another player in next is refused (player_changed).
---| "MOVE_INDEPENDENT_ARMY"     # fn(next, player, x, y): one independent army's move, at most one step a turn. Don't call next and the army stays where it is this turn, as a hold or a stationary army does. Another player or square in next is refused (player_changed, square_changed). The result is nil.
---| "SQUARE_CITADEL"            # fn(next, player, x, y): whether the square is a citadel for the player (-1 for any class), as the game's citadel counts, the AI's recruiting and its conquest ask. next returns the game's answer, true or false; return true or false for the game to use. On Mac a wrap takes the uses that pick a random square for a mark and for a ritual; the game's other uses keep its answer.
---| "SQUARE_FORT_PARTS"         # fn(next, x, y): the fortification of a square: its terrain's, as some enchantments change it. The low byte is the battle map's number (batmap), 512 the walls, and the gates follow: 1024 woodengate, 2048 irongate, 4096 enchantedgate, 8192 bramblegate, 16384 barricade, 32768 earthbarricade, 65536 cloudgate. The game counts no walls as fort level 0, walls with an iron, enchanted or cloud gate as 2, and other walls as 1. The battlefield, the square's battle strength, the overrun check, the square info window and the AI's troop and recruit choices ask it; a ritual's fortreq and nofortreq don't. next returns the game's bits; return the bits the game then uses.
---| "SQUARE_WATERLIKE"          # fn(next, x, y): whether the square counts as water for world generation, the AI's routes and the battles (true off the map). next returns the game's answer, true or false; return true or false for the game to use.
---| "SQUARE_OPEN_LAND"          # fn(next, x, y): whether the square is open land, as world generation picks squares for its sites. next returns the game's answer, true or false; return true or false for the game to use.
---| "SQUARE_LIBRARY_LEVEL"      # fn(next, player, x, y): the library level at the square for the player: its terrain's library bits plus the player's library enchantments there; 4 and up halves the library's gold. next returns the game's level; return the level the game then uses.
---| "BATTLE_EDITOR_WAIT"        # fn(next, block, mode): each frame of the battle map editor, which opens only with the --batmapeditor launch option. The game passes block 0 and mode 2. next returns the clicked id, 0 for none: 1 quit, 3 home, 4 select monster, 5 clear all, 6 center line, 8 load, 9 save, 20 to 23 scroll, 24 the shortcut list, 25 ground edit mode. Return another id to act on it.
---| "RECRUIT_WAIT"              # fn(next, block, mode): each frame of Recruit. next returns the clicked id.
---| "VIEW_ARMY_WAIT"            # fn(next, block, mode): each frame of the army view, the battlefield of one square's army with both sides deployed: a right click on an army, coe5.viewArmy, and a LAN client's waiting screen. No battle follows. The game passes block 0 and mode 6. next returns the clicked id, 0 for none: 1 leaves (Enter, Esc), 3 the shortcut list, 4 highlights the units with magic items.
---| "UNIT_LIST_WAIT"            # fn(next, block, mode): each frame of a battle's unit list (F1). next returns the clicked id. A unit's row is 100000 + its unit index (negated too); an id for a unit past the game's last becomes 0 (lua_refused, reason=unit_past_table).
---| "KEYBOARD_SHORTCUTS_WAIT"   # fn(next, block, mode): the Keyboard Shortcuts screen waiting for a click: the Game Menu's, and the ? key's on each screen that has one. The game passes block 1 and mode 0, so it runs once a click. next returns the clicked id: 1 closes the screen, any other draws it again.
---| "TOOLTIP_LINE_DRAW"         # fn(next, text, index, r, g, b, alpha): one line of the hover text box at the bottom of the screen, on every frame it shows. text is the line as the game cut the hover text to the box's width, index its place (0 the top line; the game draws them from the last up), and r, g, b, alpha its colour, 0 to 1 (the game's white, 1 1 1 1). next(text, index, r, g, b, alpha) draws it in the colour you pass. Pass text and index as they came: other ones are refused (lua_refused reason=text_changed or index_changed) and the line's kept. next returns nothing.
---| "AI_EXPLORE"                # fn(next, commander, comstr): an AI player's exploration target for one commander, during its moves; comstr is the commander's strength. next(commander, comstr) returns the game's choice: score, x, y (x and y coe5.aiMove.noSquare for none). Return score, x, y to send the commander to any square of the world, on any plane (the game routes it, through portals too); return score alone to keep next's square; return nothing to keep next's choice. The game runs this only while the commander's earlier steps (rally, friendly move, conquest) scored below coe5.aiMove.exploreBelow, and keeps the square only when the score is above their best (commanderAiMoveKind is coe5.aiMove.target when one of them picked a square, at commanderAiTargetX and Y); a score of coe5.aiMove.farExploreBelow or more also skips the game's far exploration. A square off the world or on no plane, a score above 0 with no square, or values that aren't whole numbers are refused (lua_refused reason=off_map, no_plane, no_square, not_a_score, not_a_square, out_of_range) and next's choice kept; another commander in next is refused (commander_changed).
---| "AI_RALLY"                  # fn(next, commander, comstr): the AI's rally step for one commander, the first of its steps: a square of the player's where its other commanders' armies could join this one. Takes and returns what AI_EXPLORE does, with the same refusals; the game keeps the square when the score is above 0, and the move is then coe5.aiMove.target.
---| "AI_FRIENDLY_MOVE"          # fn(next, commander, comstr): the AI's move to a square a teammate owns, after AI_RALLY; the game drops a square where an enemy army near it is much stronger than comstr. Kept when the score is above the rally's; as AI_RALLY otherwise.
---| "AI_CONQUER"                # fn(next, commander, comstr): the AI's conquest step, after AI_FRIENDLY_MOVE: a square to attack or take, among those the player has seen; -999 for none. Kept when the score is above the earlier steps' best; as AI_RALLY otherwise.
---| "AI_FAR_EXPLORE"            # fn(next, commander, comstr): the AI's far exploration, after AI_EXPLORE, run only while the best score is below coe5.aiMove.farExploreBelow: an unseen square anywhere. Kept when the score is above the best, and the move is then coe5.aiMove.explore. As AI_RALLY otherwise.
---| "AI_RITUAL_CANDIDATE"       # fn(next, commander, ritual): one candidate of the AI's ritual plan choice for a commander with no plan (commanderRitualPlan 0), after the game's income, "rare", "never plan this" and other data reasons passed it. next(commander, ritual) runs the game's pointless check of it (the corpses and resources the ritual needs at the caster's square), and CoE5SE's (a ritual with raisecorpses or a far raise whose raises find no corpse there, as an AI's raise takes them), and returns true when the ritual isn't pointless, false when it is. Return false to veto the candidate (the game drops it as "pointless" and draws the next); return true or nothing to keep it, which leaves the decision to the game: a kept candidate always gets next's answer, called for you if the wrap didn't, so a wrap can veto but never plan what the game drops. A kept candidate is not yet planned: after the wrap the game still checks it is not bad for the AI, can be cast, has the ritual's terrain seen near enough, and meets its time and location rules, and drops it if any fails. Another value is refused (lua_refused reason=not_a_boolean) and keeps; another commander or ritual in next is refused (commander_changed, ritual_changed). A wrap that errors keeps. coe5.scrapRitualPlan works inside it.
---| "AI_RECRUIT"               # fn(next, player): an AI player's recruiting step, once a turn after AI_TURN and before its commanders move. next(player) runs the game's buying: from the player's offers (coe5.shopOffers) at its citadels, keeping gold for the game's save target (10 turns of gold income, at most 250). Skip next and the game buys nothing this turn. coe5.aiBuyOffer buys inside it, before or after next. next returns nothing.
---| "AI_GATHER_TROOPS"         # fn(next, commander): the AI's troop pick-up for one commander, before each of its move steps: next(commander) takes the free troops on its square that may join it, leaving a share there (20% on a citadel, 60% with under 2 citadels), or, with none free, leaves some of its ranged troops behind. Don't call next to skip it: the commander takes and leaves nothing this step, and the troops stay free for another commander there. Another commander in next is refused (commander_changed). A wrap that errors runs it. coe5.scrapRitualPlan works inside it.
---| "AI_PICK_UP_LONER"          # fn(next, commander, unit): the AI's pick-up of one lone commander (unit is its unit) into the army of commander, a caster of a higher ritual level on its square, after the game's checks passed it: from then on the lone commander and its followers follow commander and never move independently again. next(commander, unit) joins them and returns how many units joined; don't call it (return 0) to keep the lone commander free. Another commander or unit in next is refused (commander_changed, unit_changed). A wrap that errors runs it. coe5.scrapRitualPlan works inside it.
---| "AI_SPLIT_ARMY"             # fn(next, commander, minUnits): the AI's split of a commander's army, at the end of its routine before its moves, in 25% of the calls: with minUnits units or more following it (the vanilla value is 240), the best commander that follows it leaves with some of its troops (each troop of strength up to 100 to 299 with a 10% chance, in two passes, until the larger of a fifth and 40 to 99 units went). minUnits is the game's minimum for this call; next(commander, minUnits) runs the split with the minimum given, 1 or more, for this call only (lua_refused reason=out_of_range keeps the game's). Don't call next to skip it. No commander follows another unless the AI picked it up (AI_PICK_UP_LONER), so with none nothing splits. Another commander in next is refused (commander_changed). A wrap that errors runs it. coe5.scrapRitualPlan works inside it.
---| "AI_MOVE_COMSTR"            # fn(next, commander): the strength the AI commander's route takes for one move step, right after AI_GATHER_TROOPS and before the game routes it to the square its steps kept. next(commander) returns the game's, the strength of the commander's army. The route won't end on a square whose army is stronger than the strength allows (times 100, divided by aiAttackDivOther, aiAttackDivAi or aiAttackDivHuman by that army's owner, less 15, aiAttackMargin's value), so return a larger one, such as several commanders' strength together, to let it route onto a stronger army; the square itself was picked by the steps. Returns a whole number from -1 to 21474836, as coe5.route's comstr; another value is refused (lua_refused reason=not_a_number or out_of_range) and next's kept. Return nothing for next's. Another commander in next is refused (commander_changed). coe5.route works inside it. A wrap that errors gives the game's. Named after the game's AI debug log line "comstr".
---| "AI_DANGEROUS_STEP"         # fn(next, commander, dx, dy): whether an AI commander's step from its square by (dx, dy) is refused ("aborted dangerous move"), in the AI's move loop and in the independents' wandering moves: the Void off its plane, frozen water in Winter or Spring with 2 or fewer turns of the season left, a burning army (burnforest 100 or more) into burnable terrain while its fire resistance is under 100, Dispersing Clouds without flying, the sky or water while Fatigued. It reads the commander's unit only. next(commander, dx, dy) returns the game's answer, 1 to refuse and 0 to let the step go; return either. Another commander or step in next is refused (commander_changed, step_changed). A wrap that errors gives the game's.
---| "AI_MAKE_GUARDS"            # fn(next, player, x, y, n): n of the player's units on square (x, y) become guards, which never leave it; x and y are -1 for the player's units anywhere: at game start for each player's army (-1, -1, n 4), and in the AI's turn on each square it defends with an Enchanted Gate, Mirror Wall, Wall of Torment or Murderous Mist (n 1). next(player, x, y, n) makes them; pass another n (1 or more) for this call, or don't call it for none. Another player or square in next is refused (player_changed, square_changed), and an n below 1 (out_of_range) keeps the game's. A wrap that errors runs it.
---| "AI_RITUAL_TARGET"          # fn(next, commander, ritual): the square an AI commander casts its planned ritual at, from up to 25,000 random squares of Elysium, the Sky and Agartha: a seen square with an enemy army for an aitarg 1 ritual, a seen square with 5 corpses or more for aitarg 2, the first square for any other. next(commander, ritual) returns 1, x, y for a square found, 0 for none (the plan waits). Return 1, x, y for a square on a plane, or 0 for none; 1 with no square, a square off the map or off every plane, or another value is refused (lua_refused reason=no_square, off_map, no_plane, out_of_range, not_a_score, not_a_square) and next's kept. Return nothing for next's. Another commander or ritual in next is refused (commander_changed, ritual_changed). A wrap that errors gives the game's.
---| "DECORATE_SQUARE"           # fn(next, x, y): one square's random special terrain by its terrain (villages, farms, mines, ports; towers, castles and ancient ruins only on Elysium): in world building, a map file's random squares, and the map editor's Create Fancy Terrain. next(x, y) places them; don't call it to keep the square as it is. Another square in next is refused (square_changed). A wrap that errors runs it.
---| "WEAPON_HIT"                # fn(next, attacker, target, weapon, damage, value, dmgtype): one hit's damage in a battle, after the game's MR checks and its weapon flags, before its double-damage rules. value is the spell's or monster attack's dmg (999 on the kill spells; an affliction's mask, low 32 bits); dmgtype is the weapon's damage type, and a type the game doesn't have takes its plain damage. Return the damage the hit does: 0 makes the hit and its effect do nothing. next returns CoE5SE's answer (killres, polymorphres, petrifyres, doesntaffect and the rest). Runs on every hit: keep it quick.
---| "BATTLE_TARGET_CHECK"       # fn(next, unit, type, weapon, value, target): the battle AI asking whether a target is worth a spell or a mind weapon (a weapon with the mind flag, charm and enslave among them), each battle round and in a replay. unit is the caster or attacker, weapon its weapon and target the unit aimed at; type and value are the attack's, to pass on. target is below 0 when the game draws the target itself inside the check: the wrap still runs, and can't tell which unit was drawn. next returns the game's and CoE5SE's answer: coe5.target.useful (1); -2 for a target already affected, immune, too brave for fear or too healthy for a heal; coe5.target.notAffected (-3) when the weapon wouldn't affect it; -10 for no target. Return coe5.target.notAffected to make casters pass the target over.
---| "BATTLE_TARGET_SQUARE"      # fn(next, unit, spell, weapon, type, value): the battle cell an attack or spell aims at, for every unit of every side: each weapon attack (up to 5 tries), each spell (up to 50 spell picks), each later shot of a weapon or spell, and a strike sent to cell 999, 999. spell is -1 for a weapon's attack and weapon -1 for a spell's; type and value are the attack's (value its low 32 bits). next(unit, spell, weapon, type, value) returns the game's pick, useful, x, y: its trgrank, range and area, then a blind caster's miss and scatter, with x, y -1, -1 for no cell. Return useful, x, y to aim there: useful above 0 (coe5.target.useful) and the attack goes to the cell; 0 or below and the attack looks again, then gives up ("unable to attack", "casts no spell"). useful alone keeps next's cell. Return nothing for next's. A cell off the field, useful above 0 with no cell, or another value is refused (lua_refused reason=off_field, no_square, out_of_range, not_a_number, not_a_square) and next's kept. Another unit, spell, weapon, type or value in next is refused (unit_changed and the rest). Draw no random numbers in the wrap: a LAN client fights the battle again and must pick the same cells. A wrap that errors gives the game's.
---| "BATTLE_SPELL_PICK"         # fn(next, unit, path): the spell a caster's spell attack casts in a battle, picked at random among its memorized spells of that path (path is a magic path's number, listed under Game words and numbers); the attack asks again, up to 50 times, until the spell has a target. Return another spell (any spell the table holds, known or not) or -1 for none; nothing for next's. Another number is refused (not_a_spell, not_a_number) and next's kept; another unit or path in next is refused (unit_changed, path_changed). A wrap that errors gives the game's.
---| "CEREMONY_PERFORM"          # fn(next, ceremony, commander, target): one of the Voice of El's ceremonies, a player's (the Ceremonies screen, a map order) or the AI's. ceremony is the game's number for it: match it by coe5.get("ceremonyName", ceremony), never by the number. target is the player a Start Crusade goes against, -1 for every other ceremony. next(ceremony, commander, target) is the game's: it checks the rank, limits and cost (and does nothing when one fails), charges the cost and action points, and does the ceremony. Skip next and none of it happens: no cost, no action points, no effect, and no message. Code after next adds more effects. Another ceremony, commander or target in next is refused (ceremony_changed, commander_changed, target_changed) and the game's kept. A wrap that errors runs it.
---| "CEREMONY_COSTS"            # fn(next, commander): the game works out every ceremony's cost for a priest of El: at the top of the Ceremonies screen, before a map order's ceremony, and before the AI's. After next(commander), coe5.get("ceremonyCost", ceremony) holds each one and coe5.set("ceremonyCost", ceremony, value) changes it: the screen shows that cost, the ceremony checks and charges it, and the AI weighs it, until the next time the game works them out. The game multiplies some: Break Seal's by the seal's number (1 for the first), Bolster Devotion's by one more than the player's earlier Bolster Devotions, Increase Divine Favor's by the level it raises the favor to, Convert Settlement's by a fifth of the square's gold income (its action points too). Each is gold, or relics for Break Seal, Bolster Devotion and Increase Divine Favor. Another commander in next is refused (commander_changed).
---| "SEAL_BREAK"                # fn(next, slot, seal): one of the Seven Seals breaking, by a player's Break Seal ceremony or an item, slot that player; seal is the seal it breaks, 1 to 7. Inside the wrap the seals broken are still seal - 1; next(slot, seal) breaks it, as the game does. Skip next and the seal stays unbroken, so none of what follows from it happens (its event, SEAL_EVENT; El's minions for the Voice of El players get no stronger), and the next Break Seal ceremony breaks the same seal again. The ceremony's cost and action points are spent either way. To break the seal but replace its event, wrap SEAL_EVENT instead. The seals the game breaks by itself at a month's end ("Cardinal Intervenes", "Heavenly Surge of Power") don't come here. Another slot or seal in next is refused (slot_changed, seal_changed). A wrap that errors breaks it.
---| "SEAL_EVENT"                # fn(next, slot, seal): a broken seal's event, at a month's end: the month after it broke, one seal a month in order when several broke, whoever broke it (a ceremony, an item, or the game at a month's end), once per seal. seal is 1 to 7; the game's events: none for the first two, an Apocalyptic Rider for the third to the sixth ("The apocalypse is coming closer"), the Apocalypse for the seventh. slot is always -1: the game keeps no record of who broke a seal. next(slot, seal) lets the game run its event, which it does once every wrap has returned (so code after next runs before the event); next returns nothing. Skip next to replace the event with the script's: the seal stays broken, its event counts as done and never comes back, and the next seal can be broken as before. What the wrap returns isn't read. Another slot or seal in next is refused (slot_changed, seal_changed). A wrap that errors runs it.
---| "UNIT_BATTLE_STRENGTH"     # fn(next, unit, monster, hp): the battle strength of one unit as the game sums it, 0 to 1000: unit -1 counts the bare monster (no armor, items or afflictions), monster -1 the unit's. Every unit's, square's and commander's battle strength adds it up, the AI's too. next returns the game's; return another whole number for the game to use. A unit or monster outside the tables is refused (lua_refused reason=out_of_range) and the game's kept. The reads squareBattleStrength and commanderBattleStrength show a wrap's numbers, unitBattleStrength (CoE5SE's sum) doesn't; a call of the site from the wrap's code goes to the game unwrapped.
---| "SQUARE_BATTLE_STRENGTH"   # fn(next, team, x, y): the battle strength of the units on a square as player team counts them (-1: every unit), raised by the fort when over a tenth of them are ranged: what the AI's conquest, routes and strength memory ask. next returns the game's; return another whole number for the game to use. A team, x or y outside the game's tables is refused (lua_refused reason=out_of_range) and the game's kept. The sums of unit battle strength inside it take UNIT_BATTLE_STRENGTH's answers. The AI's strength memory (squareRememberedBattleStrength) stores a wrap's number; the squareBattleStrength read is vanilla and doesn't show it. A call from the wrap's code goes to the game unwrapped.
---| "COMMANDER_BATTLE_STRENGTH" # fn(next, commander, flags): the battle strength of the commander's unit plus each unit following it, as the game asks it for the AI's plans, ritual and army checks: every call but the move loop's route, which AI_MOVE_COMSTR keeps. flags is the game's second argument on Windows (0 counts every follower, else only those whose move flags hold every bit); Mac's copy takes the commander alone. next returns the game's; return another whole number for the game to use. A call from the wrap's code goes to the game unwrapped.
---| "PLAYER_ABILITY"           # fn(next, player, ability, sum): a player's value of an ability as the game reckons it. An ability is a numbered trait a class, a monster or an item carries, and ability is the number of its .c5m word, 0 to 1501 (the value playerAbility and playerAbilitySum read). The class's value counts first, then the largest (sum 0) or the total (sum 1) over its living units. The income functions, the AI, the shops and the recruit screen ask it. next returns the game's; return another whole number for the game to use. A player outside the slots is refused (lua_refused reason=out_of_range) and the game's kept.
---| "UNIT_STATE_FLAGS"         # fn(next, unit): the unit's state bits with its items' (the value unitStateFlags reads; every bit is listed under Game words and numbers), as the game tests them for armor, moves, battles and the AI. next returns the game's; return another whole number for the game to use. A unit outside the table is refused (lua_refused reason=out_of_range) and the game's kept.
---| "ARMY_MOVE_FLAGS"          # fn(next, commander): the move flags of a commander's army as the AI's explore, conquest and routes test them (the value commanderMoveFlags reads). next returns the game's; return another whole number for the game to use. A commander outside the table is refused (lua_refused reason=out_of_range) and the game's kept.
---| "UNIT_PATH_LEVEL"          # fn(next, unit, path): the best level of the unit's spell attacks of a path (-1: any path) with the path's boosts, as the game's library and memorized spells reckon it (the value commanderSpellLevel reads): 0 for a unit with no spell attack of the path, since boosts never give a path. A spell attack in battle asks UNIT_PATH_BONUS, not this. next returns the game's; return another whole number for the game to use. A unit outside the table is refused (lua_refused reason=out_of_range) and the game's kept.
---| "UNIT_VISIBLE_TO"          # fn(next, player, unit, x, y): whether player sees the unit standing on square (x, y): 1 for one of the player's units or one without stealth, else by scrying and spiritsight, as the map's army draw, the army screens, the spy catch and the AI decide it (the value unitVisibleTo reads). next returns the game's, 1 or 0; return another whole number for the game to use. A player, unit or square outside the game's tables is refused (lua_refused reason=out_of_range) and the game's kept.
---| "RESOURCE_AVAILABLE"       # fn(next, player, costType, x, y): what a player has of a cost type where the game's screens ask (the top bar, the Recruit screen, can-afford), cost types 0 to 18 as commanderCostOnHand reads them. A ritual's cost check (ritual cost checks, ADR 0007) stays the game's and a cast pays what it checked. next returns the game's; return another whole number for the screens to use. A player, cost type or square outside the game's tables is refused (lua_refused reason=out_of_range) and the game's kept. On Mac a wrap takes only the Recruit screen's checks of expendable units and goblins (cost types 16 and 18) when an offer is bought; the game's other uses read the stock directly and keep its answer.
---| "UNIT_RESISTANCES"         # fn(next, unit): what the unit is. Despite the name, this is the unit's flag words (the value unitFlags reads: human, undead, stealth, the shields), and how a unit moves is UNIT_FLAGS. The flag words are: its monster's with its items' and benefits' bits, the bits resrestrict and resreq test. next returns the game's; return another whole number for the game to use. Windows and Mac hook the function itself when a script first wraps it, so a wrap runs for every call of it that isn't inlined into its caller: keep it short. A unit outside the table is refused (lua_refused reason=out_of_range) and the game's kept. A shield, large shield or magic shield (2^27, 2^28, 2^31) a wrap adds also counts in the battle's deploy score, as the monster's own does; one the game already gives the unit (an item's) stays the game's.
---| "UNIT_FLAGS"               # fn(next, unit): how the unit moves. Despite the name, this is the unit's move word (the value unitMoveFlags reads: flying 1, fast 16, stationary 2^35), and the flag words unitFlags reads are UNIT_RESISTANCES. The move word is: its monster's with its items' and some abilities' bits. next returns the game's; return another whole number for the game to use. Windows and Mac hook the function itself when a script first wraps it, so a wrap runs for every call of it that isn't inlined into its caller: keep it short. A unit outside the table is refused (lua_refused reason=out_of_range) and the game's kept. Fast (16) or battlefast (2^33) a wrap adds gives a second move each battle round, and teleport (2^39) the battle's blink, as the monster's own words do; one the game already gives the unit (an item's) stays the game's.
---| "RITUAL_CAN_CAST"          # fn(next, commander, slot): every cast check of a ritual, where slot is the ritual's number: the ritual screen's for a row, the AI's for its plans, a repeated ritual's. next returns the game's answer with castermaxcast: 1, or a reason at 0 or below (coe5.ritual.reasonMirror needs a mirror monster, reasonMaxcast cast as often as maxcast allows, reasonImmortal an immortal caster, reasonMastery no more rituals at this mastery level, reasonPlane the wrong plane). Return 1 to let the commander cast, a reason to hold it back; the ritual screen shows the reason's text. A commander or slot off the tables in next is refused (out_of_range) and the vanilla value goes on.
---| "RITUAL_LOCATION_CHECK"    # fn(next, commander, slot, x, y): every location check of a ritual, where slot is the ritual's number (its terrain, the units and enchantments its rules count, its trade and guild rules): the ritual screen's, the AI's, a repeated ritual's, the castable-anywhere test's. x and y are the square asked about, -999 and -999 for the caster's square. next returns the game's answer: 1, or a reason at 0 or below (-2 needs a certain type of unit, -3 more trade points than guilds, -4 no guild here). Return 1 to let the ritual be cast there, 0 or below to hold it back. The ritual screen tests the ritual's terrain itself (ritualTerrainAt), so a row it greys for the terrain is opened at RITUAL_SCREEN_ROW. A square that is neither -999, -999 nor on the map in next is refused (out_of_range).
---| "CEREMONY_SCREEN_ROW"       # fn(next, id, a, b, name, costType1, cost1, costType2, cost2, costType3, cost3, costType4, cost4, hover, apCost, castable, mirrorMonster, mirrorText): one row of the Voice of El's Ceremonies screen, with RITUAL_SCREEN_ROW's arguments (the game draws both screens' rows alike). id is the ceremony + coe5.ceremony.rowUsable when the priest may perform it (the number CEREMONY_PERFORM gets, which ceremonyName names), or + coe5.ceremony.rowGreyed when greyed out (the game gives a greyed row no id; CoE5SE numbers it while a script wraps here, so coe5.get("hoveredId") tells the rows apart). hover is a greyed row's reason, the game's words, and "" on most rows the priest may perform; each cost is gold (costType 0) or relics (12), as the game charges it; pass a and b on unchanged. On Windows the game draws five of the rows with a separate copy of the row code, over which CoE5SE writes the call, so a wrap sees every row on both builds. In next, a mirrorMonster past the last monster or a costType above 50 is refused (lua_refused reason=out_of_range) and the game's value kept. next returns nothing the game uses (nil). a and b are the end and the number of the cost columns, as on a ritual's row.
---| "CEREMONY_SCREEN_WAIT"      # fn(next, block, mode): the Voice of El's Ceremonies screen waiting for a click. The game passes block 1: the screen keeps showing its rows as they were laid out until a click, so CEREMONY_SCREEN_ROW runs only then. next(0, mode) returns 0 at the end of each frame with no click, and the screen lays its rows out again for the next frame, as the ritual screen does every frame. next returns the clicked id, which the screen then acts on.
---| "UNIT_DAMAGE_BONUS"         # fn(next, unit): what the unit's items, afflictions and benefits add to its monster's str, added to each hit's damage unless the weapon has nostr; its stats screen shows the two together as Strength (unitStr). Diseased, Weakened, Chest wound, Exhausted, Numbness, Debilitating wound and Death wish change it inside the game's function. next returns the game's; return another whole number for the game to use. The unit stays the game's (unit_changed).
---| "UNIT_MORALE"               # fn(next, unit): the unit's morale, as each hit, the battle's fear check and the battle AI read it. Battle fright and Feebleminded change it inside the game's function. next returns the game's; return another whole number for the game to use. The unit stays the game's (unit_changed).
---| "UNIT_MR"                   # fn(next, unit): the unit's magic resistance, as each spell hit, confusion's recovery and lifeforce drain read it. Feebleminded changes it inside the game's function. next returns the game's; return another whole number for the game to use. On Mac the primal animal test sums a separate copy, which keeps the game's answer. The unit stays the game's (unit_changed).
---| "UNIT_BATTLE_RANK"          # fn(next, unit): the unit's rank in battle, 1 front, 0 middle, -1 back, as the battle's setup and its target picks read it. Death wish and Battle fright change it inside the game's function. next returns the game's; return another whole number for the game to use. The unit stays the game's (unit_changed).
---| "UNIT_MAX_HP"               # fn(next, unit): the unit's most hit points, as hits, healing, the turn's upkeep, the AI and the battle token read it. Chest wound and Never healing wound change it inside the game's function. next returns the game's; return another whole number for the game to use. Windows and Mac hook the function itself when it is first wrapped, so a wrap runs for every call of it. The unit stays the game's (unit_changed).
---| "UNIT_PATH_BONUS"           # fn(next, unit, path): what the unit's boosts add to its level in a magic path: its magicboost and the path's pathboost, from its monster and items, less 1 when it is Feebleminded. A spell attack in battle asks it at each cast, and so do the unit stats screen and UNIT_PATH_LEVEL (the library, memorized spells, the AI), so a wrap here changes both. next returns the game's; return another whole number for the game to use. The unit and path stay the game's (unit_changed, path_changed).
---| "UNIT_BLINDNESS"            # fn(next, unit): 0 when the unit sees, 1 with one eye left, 2 blind, as each hit, each target pick and sight read it. Lost an eye and Blind change it inside the game's function. next returns the game's; return another whole number for the game to use. The unit stays the game's (unit_changed).
---| "UNIT_INCAPACITATED"        # fn(next, unit): whether the unit can't move, attack or weapon-attack this round (asleep, paralyzed, stunned, held, tree form, turned to gold, petrified, shackled), as each round and the battle token ask it. next returns true or false; return true, false or a whole number (0 false). The unit stays the game's (unit_changed).
---| "UNIT_FIRE_RESISTANCE"      # fn(next, unit): the unit's fire resistance, as fire hits, burning and the AI's routes read it (the value coe5.unitFireResistance reads). Tree form changes it inside the game's function. next returns the game's; return another whole number for the game to use. The unit stays the game's (unit_changed).
---| "DOATTACK"                  # fn(next, unit, slot, spell): one unit's attack in a battle round: an attack slot with spell -1, or slot -1 with the spell an item casts. A wrap that doesn't call next skips the attack, as a Confused or Slimed unit's is skipped. The unit, slot and spell stay the game's (unit_changed, slot_changed, spell_changed). coe5.observe still watches every call. Named after the game's debug log line "doattack".
---| "UNIT_BANISHABLE"           # fn(next, unit, weapon, attacker): what a banish hit (dmgtype 23) does to the unit, as the hit, the battle AI's target picks and the unit's Banishable line ask it. weapon and attacker are the hit's, or a target pick's weapon and caster; -1 for the unit stats screen. next returns the game's: 1 for an undead or demon off its home plane with banishsurv, 2 without, 0 for any other. Return coe5.banish.noEffect, .banished or .damage; anything else is refused (out_of_range) and the game's kept. The unit, weapon and attacker stay the game's.
---| "BANISH_DESTINATION"        # fn(next, unit, weapon, attacker): where a unit banished in a battle lands after it, by dmgtype 23 or coe5.battleBanishUnit. weapon and attacker are the hit's that banished it, -1 for none. next returns the game's plane, x and y: the plane coe5.battleBanishUnit named, else the unit's, and its square, else a random fitting one. Return a plane, 0 to the last, and a square of it, or a plane alone for a random fitting square; a square off the map or on another plane is refused (off_map, wrong_plane) and the game's kept. Runs on the host in a LAN game.
---| "IMMORTAL_REFORM"           # fn(next, unit, plane, applied): where, as what and for whom a dead immortal reforms. applied is true at the turn's reform, false when the AI asks where its returning units would reform as it plans its moves; give both the same answer. plane is the plane of the unit's square (at the reform, the square it died on), -1 for none. next returns the game's x, y, monster and owner: x and y -1 when it stays dead, monster and owner -1 when it keeps its current value. Return x and y, then a monster and an owner, each -1 or left out to keep the unit's; -1, -1 keeps it dead. A square off the map or on no plane, a monster the game makes no units of, or an owner that isn't a player slot is refused (off_map, no_plane, bad_monster, bad_player) and next's answer kept. The unit, plane and applied stay the game's. Runs on the host in a LAN game.
---| "UNIT_HEAL_RATE"            # fn(next, unit): how the unit heals between turns, as its player's turn start asks it of each unit that doesn't regenerate and isn't on Hades' no-heal path. next returns the game's rate: coe5.healRate.never for noheal without falselife, .fast for fastheal or an army heal spell (heallivingarmy, healhumanarmy, heal<trait>army), else .usual. Return coe5.healRate.never, .usual or .fast; anything else is refused (out_of_range) and the game's kept. The game works out the hit points from the rate; the unit stats screen and regeneration keep the game's. The unit stays the game's (unit_changed). Runs on the host in a LAN game.
---| "ATTACK_INIT"               # fn(next, unit, slot, weapon, init): the initiative of the unit's attack in slot (its place in the unit's weapon list). A high initiative strikes before attacks with a lower one. weapon is -1 for a spell attack. next returns the game's: the weapon's init, 1 for a spell, 8 for a Combat Caster's. Each battle round asks it of every attack and strikes from 10 down to 0, so an answer above 10 strikes at 10 and one below 0 at 0; the unit stats screen's Init column shows the answer as it is. The unit, slot and weapon stay the game's (unit_changed, slot_changed, weapon_changed).
---| "BATTLE_ADVANCE_UNIT"       # fn(next, unit, ambush): one unit's move in a battle round. A wrap that doesn't call next keeps the unit where it stands this time. The unit and ambush stay the game's (unit_changed, ambush_changed). coe5.observe still watches every call.

---The game calls a script can watch with coe5.observe, and change with coe5.wrap. With coe5.observe,
---before(...) gets a copy of the arguments before the call, and after(result, ...) the call's result
---and the arguments after it (result nil where the call has none); what they return doesn't reach
---the game. With coe5.wrap, fn(next, ...) gets the same arguments: next may take other units,
---commanders, players, squares, monsters, items, rituals, planes, weapons and spells, each inside its
---table as it stands now (else lua_refused reason=out_of_range and the vanilla value goes on), and other
---amounts and switches; a file path, a text the game may keep, and an argument whose meaning isn't
---known stay the game's (reason=argument_kept). The wrap's result reaches the game where the line
---says what the result is; NEW_UNIT and RITUAL_FAR_SUMMON_UNITS take a unit or -1, FIGHT_THE_FIGHT a
---player, -1 or -2, and a result the line doesn't explain stays the game's (reason=result_kept). A
---wrap that skips next keeps the call from running, and its observers don't see it.
---@alias coe5.ObservedSite
---| "RITUAL_SCREEN_LOCATION_CHECK"  # (commander, slot, a, b): the ritual screen's location check for one row, right before the row is drawn. slot is the ritual's number, and a and b are -999, the caster's square. The result is 1, or a reason at 0 or below as RITUAL_LOCATION_CHECK gives it, after that site's wraps: wrap RITUAL_LOCATION_CHECK to change the answer.
---| "RITUAL_SCREEN_CAN_CAST"    # (commander, slot): the ritual screen's cast check for one row, right before the row is drawn. slot is the ritual's number. The result is 1, or a reason at 0 or below (coe5.ritual.reasonMirror, ...) as RITUAL_CAN_CAST gives it, after that site's wraps: wrap RITUAL_CAN_CAST, which is every cast check, to change the answer.
---| "GAME_START"                # (a, b): once before the first turn of a new or loaded game; both 0 in single player. After a load, "turn" holds the saved month. No result.
---| "MONTH_START"               # (kind, text): the "New Month" text, after the game's monthly update; "turn" already holds the new month. The result is nil.
---| "PLAYER_TURN_START"         # (player, quick): one player's start-of-turn step, humans, AI and independents (slots 24-30) alike; quick is 1 for AI and independents. No result.
---| "HUMAN_TURN"                # (player): a human player's turn; after runs when they end it. The result is nil.
---| "CLIENT_TURN"               # (player): a LAN client's turn, once its turn file has loaded; after runs when they end it. The result is nil.
---| "MODS_READ"                 # (): the game has read the mods (startup, leaving the Mods screen, a load, joining a LAN game). Content writes made here are made alike on every machine, and run again at each reading. No result.
---| "AI_TURN"                   # (player): an AI player's moves. No result.
---| "PLAYER_TURN_END"           # (player): the first step after a player has moved, where the game deletes that player's messages: once for each slot that moved, humans, AI players and the independents, in slot order. MOVE_WANDERING_MONSTERS, the loners, MOVE_PLAYER_STUPIDS, the repeated rituals and the battles follow it.
---| "GAME_OVER"                 # (winner): the game over screens (not in autorun games). No result.
---| "GAME_SAVE"                 # (path, name): the Game Menu's Save and both autosaves. A result above 0 means the game was saved; after can write the mod's data next to it.
---| "GAME_LOAD"                 # (path, a): Load Game, and a load from the command line. GAME_START follows.
---| "AFTER_GAME_TURN"           # (): the month's world update at its end, before MONTH_START: the season changes, the world's events run, units drown or crash-land, and the squares are cleaned up. No result.
---| "DO_ALL_FIGHTS"             # (): every battle on the map is fought; three times after each round of players' moves, and twice in the month's update. No result.
---| "MOVE_ALL_INDEPENDENTS"     # (player): an independent slot's armies move (24 to 30); for the Inferno, Hades and the Celestial after their steps. No result.
---| "CONQUER_SQUARE"            # (x, y, player): a player takes the square: an army that took it (world population included), the month's update, a ritual, or a unit's step. The result is nil.
---| "FIGHT_THE_FIGHT"           # (x, y): one battle on the square, its rounds to a winner; the field is built and both sides deployed before it. The result is the winner's player, -1 for none, -2 when no side is left. Named after the game's debug log line "--- fight the fight".
---| "BATTLE_ROUND"              # (): a battle round starts (battleRound holds it), before starvation, movement and attacks; a replay's rounds too (battleReplay). No result.
---| "DAMAGE_UNIT"               # (unit, damage): damage comes off a unit's hp (below 0 heals); its death runs inside when the hp falls to 0 or below. In battle and out of it; a replay's too (battleReplay); not a meteor's kills. The result is nil.
---| "DOATTACK"                  # (unit, slot, spell): one unit's attack in a battle round: an attack slot with spell -1, or slot -1 with the spell an item casts. Damage it deals reaches DAMAGE_UNIT inside it. The result is nil. A wrap may also skip it (coe5.Site); the observers see every call. Named after the game's debug log line "doattack".
---| "EXECUTE_RITUAL"            # (unit, ritual, x, y, percent, pay): a ritual is cast: the Rituals screen, the AI, repeated rituals, a cast by name, world building on Elysium. unit is the caster's unit, not its commander number. x and y are the target square, which only a ritual with chooseloc, chooseterrloc or choosefxtarg reads: any other works at the caster's square, and x -999 lets the caster pick. percent is the cost percent, 100 normally and the Sacrifice Level for a varcost ritual. pay is nonzero when the cast pays the costs, 0 when the caller paid first, as every AI cast does. The result is 1 for a cast, -2 when it found no good target, -3 for no dig direction, -1 when it was refused.
---| "NEW_UNIT"                  # (x, y, player, commander, monster): a unit is made on square (x, y) for the player, under the commander (-1 none): recruits, summons, every populate's units, battle summons and splits. The result is the new unit, -1 when the unit table is full.
---| "REMOVE_UNIT"               # (unit): a unit leaves the game without dying: the event killunit at 9999 and above, killsquare, a mirror image's end, units the game clears. A death by damage is DAMAGE_UNIT's. The result is nil.
---| "TURN_UNIT_UPKEEP"          # (player): a player's units' step near the end of its turn start: damage on terrain, disease, ability spawns, sinners, colonies, terraforming, promotions. The result is nil.
---| "SQUARE_UPKEEP"             # (player, -1, x, y): what a square does to the army on it: fire (burning ground, Magma, Flames) hits units without full fire resistance, a Thunder Cloud shocks, poison fog poisons, the Void damages and drives insane without voidship, and units that heat or cool their square change its temperature. It runs at a player's turn start for every square holding an army of its team, before the unit upkeep, and after a commander's step onto a square. player is the player whose turn it is and the second value is always -1. The result is nil.
---| "RECRUIT_BUY"               # (player, offer, x, y): a human player's buy of shop offer `offer` at square (x, y): the Recruit screen and the auto-recruit. Its units reach NEW_UNIT inside it. The result is 1.
---| "PROMOTE_UNIT"              # (owner, unit, monster, n): a unit becomes `monster`, and with n nonzero a commander: the event actions promote and promoteunits, a ritual's promotion, the month's growth, and coe5.promoteUnit. The result is nil.
---| "MASTERY_TRANSFORM"         # (unit), the commander's unit: A commander changes into its next form by mastery: a ritual of mastery (levelup) or a library's mastery turns the unit into the monster its mastery word names, and it learns 2 spells of each spell level gained. unit is the commander's unit. The result is nil.
---| "BATTLE_SETUP"              # (x, y, mode): a battlefield is built for square (x, y): each battle and replay (mode 0), and the backdrop of the army screens, rituals, summons and the battle map editor. The result is nil.
---| "BATTLE_SETUP_SIDE"         # (player, x, y, side, filter): one side's units are placed on the battlefield, by rank in columns from the middle: the defender (side 1) first, then the attacker (side 0). filter is -1 for every unit of the side, else a player, for only the units that player sees. It runs for every battle and replay, and for the backdrop of Transfer troops, the army view and the battle map editor; an ambush runs BATTLE_SETUP_AMBUSH. The result is nil.
---| "BATTLE_SETUP_AMBUSH"       # (player, x, y, side, filter): one side's units are placed for an ambush, in place of BATTLE_SETUP_SIDE and with its arguments: the ambushed (side 1) in a block with its commanders first, then each ambusher (side 0) beside the nearest defender. The result is nil.
---| "BATTLE_ADVANCE_UNIT"       # (unit, ambush): one unit's move in a battle round: fleeing, confused, burrowed, flying, holding by rank, or a step toward the enemy; a fast unit moves twice. The result is nil. A wrap may also skip it (coe5.Site); the observers see every call.
---| "BATTLE_TURN_EFFECTS"       # (unit, aftermath): a battle round's effects on a unit: regeneration, decay, damage over time; `aftermath` is 0 in the rounds and nonzero after the fight. The result is nil.
---| "BATTLE_TRANSFORM"          # (unit, monster, attacker): a hit turns `unit` into `monster` on the attacker's side: mutate, replicate, ghoulify. The result is nil.
---| "FOUND_ITEM"                # (x, y, item): a commander on square (x, y) finds `item`: a battle's loot, a conquered square's, a dead unit's. The result is nil.
---| "REMOVE_DEAD_PLAYERS"       # (): the game takes out every player with no citadel or commander left ("has been eliminated"): after each round of moves, in the month's update, after battles, when a square is taken. Which players went shows in the slots before and after. No result.
---| "CREATE_CITADEL"            # (player, x, y): a player's citadel is made at square (x, y) as the game places the player. The result is nil.
---| "INQUISITION_SPAWN"         # (player): the Inquisition's army arrives for the player (Call the Inquisition). The result is nil.
---| "ANIMAL_SUMMONING"          # (commander, value): a druid ritual with specpow summons animals by the terrain of its caster's square. commander is the caster's commander number, and value is the ritual's specpow: 1 Minor Animal Summoning, 2 Major Animal Summoning. The result is nil.
---| "ORDER_SLAVE_HUNT"          # (commander): a commander's slave hunt on its square: the map's order, the AI's, and the repeated rituals'. The result is nil.
---| "ORDER_EAT_VILLAGE"         # (commander): a commander eats the village on its square: the map's order and the AI's. The result is nil.
---| "UPDATE_VOID"                # (): The month's redraw of the Void: each of its squares goes unseen by every player and gets its terrain drawn again. The result is nil.
---| "VOID_SHUFFLE"               # (): The month's shuffle of the Void: fifteen swaps of two random squares. The result is nil.
---| "PLANESHIFT_UNIT"            # (unit, plane, noSight): A unit moves to the same spot on another plane. Windows only for now. The result is nil.
---| "NEW_PORTAL_PAIR"            # (x1, y1, x2, y2, twoWay): Two squares are linked with a new portal number: startplane's home portal, the Strange Portals, and the rituals that open a portal. Windows only for now. The result is nil.
---| "PLANE_DISCOVERED_PASS"      # (): The month's check of which vanilla planes a human player has seen enough of, which sets planeFlags bit 0. The result is nil.
---| "MOVE_INFERNO_PLAYER"        # (): The Inferno's move each month, slot 28's armies included. The result is nil.
---| "MOVE_HADES_PLAYER"          # (): Hades' move each month, slot 27's armies included. The result is nil.
---| "MOVE_CELESTIAL_PLAYER"      # (): The Celestial Plane's move each month, slot 29's armies included. The result is nil.
---| "POPULATE_CELESTIAL"         # (): World generation places the Celestial Plane's independents. The result is nil.
---| "POPULATE_SKY"               # (): World generation places the Sky's independents. The result is nil.
---| "POPULATE_AGARTHA"           # (): World generation places Agartha's independents. The result is nil.
---| "POPULATE_INFERNO"           # (): World generation places the Inferno's independents. The result is nil.
---| "POPULATE_HADES"             # (): World generation places Hades' independents. The result is nil.
---| "POPULATE_AZTLAN"            # (): World generation places Aztlan's independents. The result is nil.
---| "POPULATE_PRIMAL"            # (): World generation places the Primal Plane's independents. The result is nil.
---| "POPULATE_ELEMENTAL"         # (): World generation places the Elemental Planes' independents. The result is nil.
---| "PLANE_RISE_TO_ELYSIUM"         # (plane, percent, n, monster): A plane sends units of a monster up to Elysium, as Hades sends its Dispossessed Spirits in its move each turn. percent is the chance for each square of the plane, n the chance for each living unit of the monster on a chosen square, and monster is the monster's name, a string ("Dispossessed Spirit"). On Mac the plane stays the game's. The result is nil.
---| "ENLARGE_PLANE"              # (plane, w, h, dx, dy, terrain): World generation grows the last plane by 4 squares each way, its squares moved 2 east and 2 south and the new frame filled with terrain: twice for a random map, once for the Inferno. w and h are 4, dx and dy 2, and the game reads none of the four. plane must be the last plane: with any other the game stops with a fatal error. The result is nil.
---| "GEN_FLOOD_PLANE"            # (plane): World generation floods a plane from its wet squares, Agartha's among them; the result is how many squares it flooded.
---| "ADDPLANE"                   # (name, w, h): A plane is placed beside the map, from world generation or a map file; the result is its number. Named after the game's debug log line "addplane".
---| "LOAD_MAP_PLANE"             # (path, name, plane): A plane's lines are read from a map file; the result is 0 when the file can't be opened.
---| "UNIT_ADD_XP"                # (unit, xp): A unit gains experience. Windows only for now. The result is nil.
---| "USE_SPECIAL_SITE"           # (x, y, commander): A commander uses the special site on its square: the o key on the map ("Use location's special power"), and the AI's commanders at a Stonehenge, Ship Stones, Temple Pyramid, Colossus or Crystal Globe. A special site is a terrain that does something when used, those and the Nexus, the Mirror Pool, the Sealed Chamber and the Floating Cube among them. Nothing happens when the commander may not act. The result is nil.
---| "LONER_STEP"                 # (unit): One step of a loner, a unit with the loner move word, which moves by itself and never in a group. Each loner takes three steps in its player's end-of-turn steps, each toward a terrain it loves or likes, never onto an army of its team and onto another only at up to 3 times its strength. The result is nil.
---| "REMOVE_TEMP_MEN"            # (): The units a battle made temporary are removed. The result is nil. Named after the game's debug log line "removetempmen".
---| "MOVE_WANDERING_MONSTERS"    # (): The active player's wandering monsters move, right after PLAYER_TURN_END. A wandering monster is a commander with the wander word, which follows its objective by itself (walk randomly, a crusade, visit citadels, a dragon's raids); the independent factions' move by indwander, infwander, hadeswander and celwander. The result is nil.
---| "MOVE_PLAYER_STUPIDS"        # (): The active player's stupid units move, after its wandering monsters and loners and before its repeated rituals. stupid is the game's move word for a unit its player cannot control, which moves like an independent unit. The result is nil. Named after the game's debug log line "moveplayerstupids".
---| "OVERRUN_CHECK"              # (player, x, y): The check for an overrun after a move onto a contested square: a mover with as many units and 10 times the Hit Points kills every enemy unit there without a battle, when none is a commander, each has battle strength 17 or less, and their battle strengths plus 2 each add to 30 or less. Their items drop on the square. The result is how many units the overrun killed, 0 for none.
---| "KILL_UNIT"                  # (unit): A unit dies of 999 damage in an overrun or an elimination. The result is nil.
---| "SUBTRACT_UNIT_HP"           # (unit, damage): A unit loses Hit Points with none of a hit's checks: the damage DAMAGE_UNIT doesn't see, a meteor's kills among it. Windows only for now. The result is nil.
---| "CONTINUE_MOVE_ORDERS"       # (player, commander): A player's stored move orders carry on, or one commander's; the result is how many moved.
---| "HUMAN_MULTI_MOVE"           # (commander, x, y, mode): A human player's commander walks a route to square (x, y), one GROUP_MOVE a square: mode 0 for a click on the end of a shown route, 1 for a stored move order carrying on. The walk stops when a step fails, the commander dies or goes insane, or an enemy is seen ahead. MOVE_ORDER_ROUTE is the order that made the route.
---| "GROUP_MOVE"                 # (group, x, y): The selected commanders take one step together to square (x, y), which is beside them or a portal's far end: the map's arrow keys and Enter Gateway, and each square of HUMAN_MULTI_MOVE. Human players only. group is the order group the commanders share, the value selectedCommanderGroup holds. The result is 0 or more when they moved, -1 for no group, -4 insane, -5 engaged in combat, -6 can't enter the square, -7 immobile, and -9 when one of several couldn't move, so none did.
---| "MOVE_ORDER_ROUTE"           # (commander, x, y): A human player's move order to a square: the game finds the route, stores the order and shows it, and no step is taken. HUMAN_MULTI_MOVE then walks it. The result is 1 with a route, -1 without one.
---| "UNIT_REMOVE_ITEM"            # (unit, item): An item is taken from a unit: the AI handing items on, a soul contract whose master is gone, an item given to an ally's unit, and the network's item moves; the result is -1 when the unit didn't hold it. Windows only for now.
---| "MEMORIZE_SPELL"             # (unit, spell): A unit memorizes a spell. The result is nil.
---| "LEARN_RANDOM_SPELL"         # (unit, path, level): A unit learns one random spell it doesn't know, then picks its memorized spells again: a ritual's newspell1 to newspell3, a summoning ritual that teaches, and the AI's library use. path is a magic path's number, -999 for the unit's first path. level is a spell level, -999 for a random one from 1 to its level in the path. The result is the spell learned, -1 when none was found.
---| "GET_START_RITUALS"          # (commander): A commander learns the rituals its schools start it with: a new commander or a promotion. The result is nil.
---| "COMMANDER_LEARN_RITUAL"     # (commander, ritual): A commander learns a ritual. Windows only for now. The result is nil.
---| "PLAYER_RANDOM_BLESSING"      # (player, kind, show): A player gets a random blessing effect of a kind, new or a rise of one the player has: Bolster Devotion and world creation (kind 0), gaindarkbless (1), tattoos (2 to 9). Each call adds 1 to playerBlessingRolls. The result is nil.
---| "ADD_PLAYER_BLESSING"         # (player, ability, add, cap): A blessing of an ability is added to a player, or raised by add up to cap when the player has it. Every try of PLAYER_RANDOM_BLESSING runs it; coe5.addPlayerBlessing does not. The result is nil.
---| "REMOVE_ENCHANT"             # (caster, type, x, y): The game ends the enchantments that match a player, a type and a square: a guild upgrade's remsquareench, purgeguilds, and a LAN client's request. Despite its name, caster is the player who owns the enchantment, -1 for any player. type is the enchantment's number (enchantType), below 0 for any type and -2 for the guilds only. The result is the last one removed, -1 when none matched. A wrap may pass another square.
---| "RITUAL_PAY_COSTS"           # (player, commander, ritual, percent, show): A ritual's costs are paid. The result is nil.
---| "RITUAL_FAR_SUMMON_UNITS"    # (caster, monster, count, player, x, y): A ritual summons units on a far square; the result is the first unit.
---| "PRIEST_FREESPAWN"           # (player): A Voice of El player's chance of free units by the seals broken. The result is nil.
---| "EVENT_KILL_SQUARE"          # (x, y): The event word killsquare runs. The result is nil.
---| "GIVE_ITEM_BY_NAME"          # (unit, item): An item is given by string: the event word newitem and rituals. The result is nil.
---| "TAKE_ITEM_BY_NAME"          # (unit, item): The event word removeitem runs. The result is nil.
---| "GIVE_RANDOM_ITEM"           # (unit, kind, percent, level): A random item is given at a percent chance: the event words randitem and randmisc and more. Windows only for now. The result is nil.
---| "SQUAREEVENT_START"          # (): A squareevent starts: every square is marked and the event's player set. Windows only for now. The result is nil. Named after the .c5m word squareevent.
---| "TERRAIN_PRESENT_RESCAN"     # (): The game rescans which terrains are present after a terrain change. The result is nil.
---| "SHOP_OFFER_MESSAGE"         # (player): A player gets the message for the newest shop entry: a Recruitment Offer or a Magic Item for Sale. The result is nil.
---| "WORLD_FIRE_UPDATE"          # (): The month's update of burning squares. The result is nil.
---| "EVENT_VAR_CHANGED"          # (variable): A ritual effect wrote an event variable. The result is nil.
---| "CONSCRIPTION"               # (player): The Baron's conscription, the first month of Summer. The result is nil.
---| "GAME_STATISTICS"            # (): Each player's row of the score graphs is written for the turn. The result is nil.
---| "ALTER_TERRAIN"              # (player, x, y, from, from2, to): A square's terrain changes: every way a square changes through the game's alterterrain.
---| "RUIN_SQUARE"                # (player, x, y): A square's terrain becomes its ruined form. The result is nil.
---| "DESTROY_GATEWAY"            # (player, x, y, flag): A Gateway is destroyed. Windows only for now.
---| "DIG_TUNNEL_SQUARE"          # (player, x, y): A Cave Wall is dug out; the result is 1 when it was. On Mac the result is nil.
---| "MONTH_SQUARE_RESET"         # (): The month's reset of every square's recruited mark. The result is nil.
---| "ROLL_CLASS"                 # (slot, unique): A random class is rolled for a slot. The result is nil.
---| "RESET_INCOME_CACHES"        # (): Every player's income is marked to be counted again. The result is nil.
---| "TEAM_START_LOCATIONS"       # (): World generation picks the teams' start squares ("Creating World"). The result is nil.
---| "TRIM_TEAM_LOCATIONS"        # (): World generation moves the teams' start positions further apart ("Trimming World"), after TEAM_START_LOCATIONS: it tries random moves and keeps a layout that scores better. It changes no terrain. Skip next to keep the positions the setup picked. The result is nil.
---| "SWAP_TEAM_LOCATIONS"        # (): World generation swaps start squares for a better score. The result is nil.
---| "CREATE_CITADELS_STEP"       # (): World generation places the citadels ("Creating Citadels"). Windows only for now. The result is nil.
---| "FIX_PORTALS"                # (): Each portal is marked active or not. On Mac the result is nil.
---| "VOID_FLUCTUATIONS"          # (): The Void's border shifts, at a 50% roll each month's end: on the ring two squares in from the Void's edge, Void squares turn to Deep Sea and Deep Sea back to Void. UPDATE_VOID runs every month and changes no terrain. The result is nil.
---| "PLANESHIFT_COMMANDER"       # (commander, plane): A commander's army planeshifts; the units that can't go are removed. The result is nil.
---| "MIST_SPREAD"                # (): The month's spread of mist. On Mac the result is nil.
---| "TEMPERATURE_CHANGE"          # (): Squares change terrain by their temperature, then every square's temperature offset fades by a third: each month, and once before the first turn. On Mac the result is nil.
---| "ELEMENTAL_ANOMALY_SPAWN"     # (): The Elemental Anomaly's elementals appear near gem producing squares, once a month while the anomaly lasts. The result is nil.
---| "UNIT_SWALLOW"               # (swallower, victim): A unit swallows another. The result is nil.
---| "UNIT_UNSWALLOW"             # (swallower): Swallowed units come out, the swallower's or, with -1, everyone's. The result is nil.
---| "LIFEFORCE_DRAIN_UNIT"       # (attacker, unit, amount): A unit is drained of lifeforce; the result is what it gave.
---| "UNIT_CATCH_FIRE"            # (a, b): Fire spreads to a battlefield cell: the unit in it and the terrain unit there each catch fire when not burning already and with fire resistance below 0, at that resistance in percent. a and b are the cell's column and row. The result is how many caught fire, 0 to 2.
---| "UNIT_CRUSH"                 # (unit): A unit 3 cells tall or more crushes what stands in its cells: each unit and terrain unit there takes 100 damage, walls excepted. unit is the crusher. It runs for every unit each battle round, and a shorter unit crushes nothing. The result is nil.
---| "REMOVE_BAD_FOLLOWERS"       # (): Followers whose commander can't lead them go free. The result is nil.
---| "OVERPOP_CULL"               # (): The game counts its units against its limit, and past 97,000 kills free units to make room. On Mac the result is nil.
---| "MONKEY_CLOAK"               # (player): Mirror Cloaking is renewed at the end of a player's turn: the disguise is cleared from the player's units, then each unit with Mirror Cloaking disguises itself and as many others on its square as its Mirror Cloaking value. Windows only for now. The result is nil.
---| "RECYCLE_UNITS"              # (): Dead unit records are freed for reuse. On Mac the result is nil.
---| "RANDOM_TELEPORT"            # (unit, terrain, followers): A unit teleports to a random square of a terrain code. On Mac the result is nil.
---| "POP_HORRORS"                # (chance): Horrors come for the units with a Horror mark, at each month's end: each marked unit draws Horrors onto its square at chance percent, and a strong mark multiplies the chance by 10. The game passes 1, and 25 in a second call while magic is tainted. A wrap may pass another chance. The result is nil.
---| "BATTLE_REMOVE_TERRAIN_UNITS"         # (): After a battle the game removes its terrain units: the trees, boulders, walls and ship parts, which are units of player 31. Skip next and they stay on the square. The result is nil.
---| "BATTLE_REMOVE_SUNKEN_SHIPS"          # (x, y): A sea battle's sunken ships are removed. The result is nil.
---| "CREATE_BOATS"               # (): A sea battle's boats are made. Windows only for now. The result is nil.
---| "BATTLE_TRAPS"               # (): A siege's traps fire, each battle round: a unit on a trapped cell sets the trap off, takes its effect (gas, fire, cold, shock or decay), and the trap is used up. The defender's side and flying or floating units set off none. BATTLE_ENCHANT_TRAPS laid them at the battle's setup. The result is nil.
---| "BATTLE_ENCHANT_TRAPS"       # (): At a siege's setup the game lays the fort's traps, as many as the square's trap enchantments give, on random free cells before the field's middle. BATTLE_TRAPS fires them each round. Skip next and the battle has no traps. Windows only for now. The result is nil.
---| "BATTLE_SUMMON_STRING"       # (): A battle spell summons by string. The result is the number of units summoned.
---| "PAY_SPELL_LIFEFORCE"        # (): A battle spell pays its lifeforce. Windows only for now. The result is not 0 when the spell goes on to be cast, else 0.
---| "WORLD_NEWS"                 # (text): A World News line goes out to every human player. Windows only for now. The result is nil.
---| "SEASON_NEWS"                # (season): The season changes, and World News says so. The result is nil.
---| "CHECK_GAME_WON"             # (): The turn's check for a winner. The result is nil.
---| "SPY_CAUGHT"                 # (): A player's units on a square find and kill the enemy units there that fight no battle, its spies, when they can see them: at each turn start, and after an army's step onto a square it then holds. A script gets no arguments. Skip next and no spy dies there. The result is nil.
---| "TOGGLE_AUTO_RECRUIT"        # (player, x, y, monster): A player's auto recruit of a monster on a square switches. Windows only for now.
---| "UNITS_TURN_TO_HADES"             # (x, y, monster, fromOwner): Units on a square turn to slot 27, Hades.
---| "ALTER_TERRAIN_RECT"         # (x, y): Part of Elysium unravels into the Void, at a month's end: from square (x, y) the game walks at random over Void squares and turns the first other square it reaches into Void, once for each square it changes, 10 the first time and 1 to 4 after. The result is nil.
---| "RESPAWN_VOID"               # (): The Void's monsters come back. The result is nil.
---| "BATTLE_SIEGE_UNIT"          # (unit): A unit acts against a siege's walls. The result is nil.
---| "GATHER_SHIPS"               # (): In a battle with ships, each ship part steps one row toward the enemy's ships, so the fleets line up for boarding: on odd rounds, for ten rounds after the ships stop advancing. The result is nil.
---| "ADVANCE_SHIPS"              # (): A sea battle's ships advance. The result is nil.
---| "MOVE_ENCHANTS"              # (x1, y1, x2, y2): Active ritual effects move from one square to another; the result is how many. On Mac the result is nil.
---| "MOVE_GEMS"                  # (x1, y1, x2, y2): A square's gems move to another. The result is nil.
---| "TAINTED_RITUAL_EFFECT"      # (commander, taint): After every ritual and ceremony: while magic is tainted and the ritual has a cost, the caster's unit gets the Horror mark at 50%. Despite its name, taint is the ritual's number, -2 for a ceremony. Skip next and no mark is given. The result is nil.
---| "LOCATION_SWAP"              # (x1, y1, x2, y2): The units on two squares swap places. On Mac the result is nil.
---| "APE_POSSESSION"             # (a, b): The ritual effect apeposs: Bandar and Vanara spirits possess the corpses on the caster's square, which rise as Possessed Longdead. a is the casting commander and b the most bodies raised. The result is how many rose, 0 for no corpses; on Mac it is nil.
---| "RITUAL_ASK_HEMISPHERE"      # (): The box "Target ritual where?", which asks a human player which way a ritual moves its caster. The result is the button: 10 up a plane, 11 down a plane, 20 north, 21 south, 22 west, 23 east, -1 Cancel; a result below 1 aborts the cast. The result stays the game's, and a wrap that skips next aborts the cast.
---| "GET_TITHES"                 # (): Once a year, at the end of Summer's first month, the Tithe of El is shared out: the gold in tithePool goes to the players with Voice of El priests. Skip next and the pool carries over. The result is nil.
---| "ELIMINATE_PLAYER"           # (player): A player is removed from the game. The result is nil.
---| "FIND_START_SQUARE"          # (player): World generation picks a player's start square. The result is nil.
---| "TROLL_FOREST_SPAWN"         # (x, y): The troll forest spawns on a square. The result is nil.
---| "AI_MAKE_DEFENDERS"          # (player): An AI player leaves guards after its moves. The result is nil.
---| "AI_REDISTRIBUTE_ITEMS"      # (player, x, y): An AI player hands items around a square. The result is nil.
---| "AI_SHIPS"                   # (commander): The AI's ship handling for one commander on a port, before its move: a commander whose army fits the free ships takes ships and sailors, then drops the crew it doesn't need. Skip next and the commander takes no ships this move. Windows only for now.
---| "AI_SHIP_DROP_CREW"          # (commander): An AI commander drops one ship's crew member. Windows only for now.
---| "AI_SHIP_TAKE_BOAT"          # (commander): An AI commander links a free ship. Windows only for now.
---| "AI_SHIP_TAKE_CREW"          # (commander): An AI commander links a free sailor. Windows only for now.
---| "CHECK_FOLLOWER_LINKS"       # (commander): The followers of a dead commander go free. The result is nil.
---| "STEP_BURN_FOREST"           # (player, x, y): A burning unit's step sets its square alight. Windows only for now. The result is nil.
---| "ORDER_BURN_FOREST"          # (unused, commander): A commander burns the forest on its square. The result is nil.
---| "ORDER_CHOP_FOREST"          # (unused, commander): A commander chops the forest on its square. The result is nil.
---| "LIFEFORCE_DRAIN_SQUARE"     # (player, x, y, amount): A square's lifeforce is drained. The result is nil.
---| "SPEND_ALL_AP_AT"            # (player, x, y): A square's units spend their action points. The result is nil.
---| "SWAP_SQUARES"               # (x1, y1, x2, y2): Two squares trade records, as the Void's shuffle does. On Mac the result is nil.
---| "TELEPORT_UNIT"              # (unit, x, y): One unit teleports to a square. The result is nil.
---| "UNIT_CLEAR_BENEFITS"        # (unit): The game clears a unit's battle afflictions and battle benefits (sleep, paralysis, burning and fear; luck, the resistances and the shields), keeping the lasting ones: when the unit dies, after each fight, and at its player's turn start. Skip next and the unit keeps them. The result is nil.
---| "GAME_MENU"                  # (): The Game Menu opens. Windows only for now. The result is nil.
---| "ARMY_COMMANDS_MENU"         # (commander): The army screen's menu opens. The result is nil.
---| "CEREMONY_SCREEN"            # (commander): The Ceremonies screen opens for a priest. Windows only for now. The result is nil.
---| "INCOME_DETAIL_SCREEN"       # (player, resource): The map's income popup opens for a resource. Windows only for now. The result is nil.
---| "BATTLE_CREATE_TREES"        # (x, y): A battle's field gets its trees, bushes and boulders by the square. Windows only for now. The result is nil.
---| "BATTLE_DEPLOY"              # (filter, x, y): The units of a fight are placed on the battlefield, the defender's side first, or both by the ambush layout. filter is -1 to place every unit, or a player, to place only the units that player sees. x and y are the battle's square. It also runs where no battle is fought: Transfer troops, the army view, a replay, and rituals that summon or promote. The result is nil.
---| "BATTLE_FIGHT_MOVE"          # (unit, x, y, mode, mark): A unit moves to a cell of the battlefield, pushing aside the units in its way: an advance, a siege engine's move, a tunneller's surfacing, a blink and a flier's landing. x and y are the cell's column and row. mode is the move's rule, 1 for an ordinary move and 3 for one that may push into a cell an enemy holds. mark is 1 to draw the move as a slide, 0 for the unit to appear there. A wrap may pass another unit; skip next and the unit stays where it stands. The result is nil.
---| "BATTLE_FLY_MOVE"            # (unit): A flier jumps on the field. The result is nil.
---| "BATTLE_FLEE"                # (unit): A unit flees the battlefield. The result is nil.
---| "BATTLE_FEAR_CHECK"          # (): Each round's fear check of every unit on the field. The result is nil.
---| "BATTLE_CLOUD_STEP"          # (unit): The clouds a unit leaves after its advance. The result is nil.
---| "BATTLE_ENSNARE_CHECK"       # (unit): A held unit's grip, net and vines before an attack. The result is nil.
---| "BATTLE_ENSNARE_RELEASE"     # (unit): The holds on a unit wear off after an attack. The result is nil.
---| "BATTLE_WALL_ENCHANTS"       # (): A siege's wall enchantments are placed on the wall cells. The result is nil.
---| "BATTLE_WALL_EFFECTS"        # (): Each round's wall and field effects: Wall of Fire, Lightning and more. The result is nil.
---| "BATTLE_START_SETUP"         # (): The start of a fight: mirror images, sleepers on snow and more. The result is nil.
---| "BATTLE_TUNNEL_MOVE"         # (unit): A unit burrows with tunnelmove. The result is nil.
---| "BATTLE_BANISH"              # (unit): A unit is banished from the field. The result is nil.
---| "BATTLE_DEPLOY_UNIT"         # (unit, x, y): A unit is put on the battlefield. The result is nil.
---| "BATTLE_AFTER_FIGHT_UPKEEP"  # (x, y): After a fight on a square: tagalong commanders, survivors' experience, charge refills. The result is nil.
---| "BATTLE_AFTERMATH_RETURN"    # (x, y, winner): Burrowed and fled units come back on the square for the winner's team. The result is nil.
---| "BATTLE_MR_ROLL"             # (unit): A unit's magic resistance roll against a weapon with the mr flag: the unit resists when the roll is below its magic resistance. A script reads no result (nil), but a wrap that returns a whole number gives it to the game: 1 resists, 0 doesn't. Windows only for now.
---| "BATTLE_SUMMON_WEAPON"       # (): A weapon that summons by its name runs. Windows only for now. The result is nil.
---| "BATTLE_GATE_SWAP"           # (): At a siege's setup, each Wooden Gate the battle map placed becomes the gate the square's terrain names (an Iron Gate for irongate, an Enchanted Gate for enchantedgate). A script gets no arguments. Skip next and the map's Wooden Gates stay. The result is nil.
---| "BATTLE_SPREAD_COLLAPSE"     # (): A ship sinks part by part: when a ship part that holds others up dies in a battle, the parts of the same ship on the eight cells around it are marked to sink. A script gets no arguments. Skip next and those parts stay up. The result is nil.
---| "UNIT_CURE_AFFLICTION"       # (unit): One of a unit's afflictions is cured. The result is nil.
---| "UNIT_HP_SHAPE_CHANGE"        # (unit): A unit grows into the previous monster at its growhp or shrinks into the next at its shrinkhp, checked after each damage. The result is nil.
---| "BERSERK_END"                # (unit): A unit stops being berserk. Windows only for now. The result is nil.
---| "UNIT_RANDOM_AFFLICTION"     # (unit): A unit gets a random affliction. The result is nil.
---| "REVEAL_TERRAIN_ON_PLANE"    # (player, plane, terrain): A player sees every square of a terrain code on a plane.
---| "NEAR_TERRAIN_SQUARE"        # (plane, terrain, x, y): The game looks for a square of a terrain code near a square. The result is nil.
---| "PLAY_SAMPLE"                # (sample, change): The game plays a sound. Windows only for now. The result is nil.
---| "GROVE_REVERSION"            # (x, y): The month's step for one square's forest: a grove enchantment of the square's owner spawns a Forest Shepherd at 33%, the square's other spawns run (revels, wild animals, storms, the troll forest), and a Sacred Grove, Grove of Gaia or Primal Forest whose owner has no grove power reverts to Ancient Forest at 10%. It runs for every square at each month's end. The result is nil.
---| "PUT_RARE_SQUARE"            # (x, y): World generation puts a rare terrain on a square. The result is nil.
---| "RANDOM_ITEM_PICK"           # (a, b, rarePercent, c, d): The game picks a random item: a shop's offers, start equipment, a random item gift, ground items and grave robbing. a is the unit that will hold it, -1 for none. b is the item type wanted (itemSlotType's numbers), -1 for any. rarePercent is the chance the pick may take a rare item. c is an ability the item must have, 0 for none, and d the value that ability must have. The result is the item, -1 when none was found; it stays the game's. Don't skip next: the game then takes item 0. Windows only for now.
---| "CREATE_RANDOM_MAP"          # (): World generation builds the random map. The result is nil.
---| "WORLD_PART_TERRAIN"         # (): World generation's polar and tropical pass, when the north or south part is above 0: a cold Plain becomes Tundra, a hot Plain Savanna, a hot Forest or Ancient Forest Jungle. Skip next for no tundra or jungle by temperature. Windows only for now. The result is nil.
---| "MAPGEN_REMOVE_SMALL_ISLANDS" # (): World generation fills in small islands of sea. The result is nil.
---| "MAPGEN_RIVER_CROSSINGS"     # (): World generation places the fords and bridges. On Mac the result is nil.
---| "MAPGEN_ANCIENT_FOREST"      # (): World generation places one Ancient Forest. On Mac the result is nil.
---| "MAPGEN_CLEAR_AREA"          # (x, y): World generation clears the area around the Capital. On Mac a call with x or y below 0, which clears nothing, isn't seen. The result is nil.
---| "MAPGEN_ADJUST_TEMPERATURE"  # (): World generation adjusts each square's temperature by what is near it. The result is nil.
---| "MARK_RIVER"                 # (x1, y1, x2, y2, mark, mode): World generation finds a river's course, the cheapest path from (x1, y1) to (x2, y2) or to the first water on the way, and marks each square of it with mark; its caller then lays River or lava on the marked squares. mode is the cost rule: 0 plain, 1 the Inferno's, 2 with walls dear. A wrap may move both ends. The result is 1.
---| "GEN_DIAGONAL_FIX"           # (plane): World generation fixes a plane's diagonal cave gaps. On Mac a wrap's plane stays the game's, and the result is nil.
---| "GEN_UPDATE_MOIST_WALLS"     # (plane): World generation updates a plane's moist cave walls. The result is nil.
---| "GEN_TERRAIN_BLOB"           # (terrain, x, y, n, onto, maxDistance): World generation lays a patch of a terrain: n random walks from (x, y), each laying terrain on the first square it finds that may take it. onto is the terrain it may replace, -1 for any, and maxDistance how far from the start a walk goes. A wrap may change the start and n. The result is how many squares were laid.
---| "CREATE_CELESTIAL_PLANE"     # (): World generation builds the Celestial Plane. The result is nil.
---| "CREATE_SKY_PLANE"           # (): World generation builds the Sky. The result is nil.
---| "CREATE_AGARTHA_PLANE"       # (): World generation builds Agartha. The result is nil.
---| "CREATE_INFERNO_PLANE"       # (): World generation builds the Inferno. The result is nil.
---| "CREATE_HADES_PLANE"         # (): World generation builds Hades. The result is nil.
---| "CREATE_ELEMENTAL_PLANE"     # (): World generation builds the Elemental Planes. The result is nil.
---| "LOAD_AZTLAN_PLANE"          # (): World generation loads Aztlan. The result is nil.
---| "LOAD_PRIMAL_PLANE"          # (): World generation loads the Primal Plane. The result is nil.
---| "AUTO_RECRUIT"               # (): The auto recruit of the player whose turn it is runs. The result is nil.
---| "USE_LIBRARY"                # (unit), the commander's unit: A commander uses a library. The result is nil.
---| "ADD_GROUND_ITEMS"           # (x, y): Items are added to a square's ground. Windows only for now. The result is nil.
---| "VOID_INSANITY"              # (unit, x, y): A commander set down on the Void goes mad: on the Void plane or Void terrain, a unit without voidsanity gains insanity. It runs when a commander uses a Crystal Globe and after a ritual moves it there. unit is the commander's unit, and x and y the square it arrived on. Skip next for no insanity. The result is nil.
---| "WANDERER_PICK_TARGET"       # (commander, objective): A wanderer chooses where to go by its objective, the value of its wander word: a crusade's target, a dragon's home, a rest. It runs in the wanderers' moves at every player's turn. A script reads no result (nil) and doesn't see the square chosen; a wrap may pass another commander. Don't return a number from a wrap: the game takes it as its answer, 1 found, 0 none, -1 the crusade is over, -2 no move.
---| "WANDERER_PICK_UP"           # (commander): Free units on a wanderer's square join it. Windows only for now.
---| "SET_HOLDIT"                 # (player, x, y): Units on a square are told to hold. On Mac the result is nil. Named after the game's debug log line "setholdit".
---| "LIBRARY_SCREEN"             # (unit), the commander's unit: The library screen opens. The result is nil.
---| "CHOP_BURN_SCREEN"           # (): The Chop & Burn window opens. The result is nil.
---| "SLAVE_HUNT_SCREEN"          # (): The Hunt for Slaves screen opens. The result is nil.
---| "UNIT_STATS_SCREEN"          # (unit): The unit stats screen opens. The result is nil.
---| "ITEM_INFO_SCREEN"           # (item): An item's info screen opens. Windows only for now. The result is nil.
---| "WEAPON_INFO_WINDOW"         # (weapon): A weapon's info window opens. Windows only for now. The result is nil.
---| "PLANE_INFO_SCREEN"          # (plane): A plane's info screen opens. The result is nil.
---| "BATTLE_INTRO_SCREEN"        # (attacker, defender, x, y): The Fight box before a shown battle. Windows only for now. The result is nil.
---| "COMMANDER_SET_DONE"         # (): The selected commander is marked done for this turn. Windows only for now. The result is nil.
---| "SELECT_NEXT_COMMANDER"      # (): The map selects the next commander. The result is nil.
---| "SELECT_PREV_COMMANDER"      # (): The map selects the previous commander. Windows only for now. The result is nil.
---| "RUN_HATREDS"                # (): The month's hatreds run. The result is nil.
---| "SANDSTORM_CLOUD_UPDATE"     # (): The month's desert clouds. The result is nil.
---| "TAINTED_MAGIC_PASS"         # (a, b): The month's mark of tainted magic: on each void gateway square, at 50%, one living unit there that is not a Horror gets the Horror mark, or the strong mark at 10% when it has one. It runs at each month's end while magic is tainted. a and b are fixed numbers the game doesn't read. Skip next for no marks that month. The result is nil.
---| "HERO_ARRIVAL"               # (player): A class's hero may arrive for a player. The result is nil.
---| "CLEAN_SQUARE"               # (x, y): The game tidies a square's ownership: it frees the square when its owner can no longer own it, works out the square's army owner again from the units there, and clears holdit on them. It runs for every square at each month's end, and when a ritual promotes a unit on a square. The result is nil.
---| "ADJUST_FOR_SNOW"            # (x, y, w, h): Ground turns to snow by the temperature. The result is nil.
---| "START_TUNE"                 # (tune, force): The game starts a music tune. The result is nil.
---| "SQUARE_SET_OWNER"           # (x, y, player): A square's owner is written, and nothing else: no income, sight, event or message follows. player is the new owner, -1 for none. It runs inside a conquest (CONQUER_SQUARE, the whole taking of a square), when a square's terrain changes in play, and on a LAN client for an ally's move; other places write the owner without it. Windows only for now. The result is nil.
---| "TEAMLESS_OWN_TEAM"          # (): Each slot with no team gets a team no other slot shares. The result is nil.
---| "ADD_BENEFIT"                # (unit): A unit gains benefits: a hit by a weapon with a benefit, and its turn upkeep. When they raise its maximum Hit Points, its Hit Points rise by the same. A script doesn't get the benefit bits. Skip next for no benefit. Windows only for now. The result is nil.
---| "SOCIETY_PRESETS"            # (society): The society's world constants are set. Windows only for now. The result is nil.
---| "MOVE_STEP_AFTER"            # (): After an army's step onto a square on the map. On a contested square it is the overrun check or an ambush, else the army loses its action points there. On a square the army now holds it is the conquest check, the spy hunt (SPY_CAUGHT) and new sight. It runs after every step of a human's and an AI's move. A script gets no arguments. Skip next and all of that is skipped. The result is nil.
---| "NUMBER_BOX"                 # (title, prompt): The game's number entry box opens.
---| "ENCHANTS_DESTROYED_AT"      # (player, x, y): Active effects on a square end as a player takes it. The result is nil.
---| "STORM_SPAWN"                # (x, y): A square may spawn Storm Elementals. Windows only for now. The result is nil.
---| "WILD_SPAWN"                 # (x, y): A square may spawn its wild animals. Windows only for now. The result is nil.
---| "REVELS_SPAWN"               # (x, y): A square may spawn its revelers. Windows only for now. The result is nil.
---| "HORROR_LORD_PICK"           # (x, y): One of the seven Horror lords, chosen at random, is put on square (x, y) for the Horrors (player 26): a Haunted City's pool, a gold mine's attack and the Void's spawns among its callers. A wrap may pass another square, or skip next for no lord. Windows only for now. The result is nil.
---| "PLAYER_GOLD_INCOME"            # (player): the gold (resource 0) a player gets at its next turn start. Every call of the game's income function: the turn start's payment, the top bar, the AI's plans, the game's statistics, and playerIncome, which reads through a wrap. The result is the income before the AI difficulty's percent (aiLevelBonus), which the turn start then applies; a wrap's result is the income the game uses.
---| "PLAYER_IRON_INCOME"            # (player): the iron (1) a player gets at its next turn start. As PLAYER_GOLD_INCOME, for this resource.
---| "PLAYER_HERBS_INCOME"           # (player): the herbs (2) a player gets at its next turn start. As PLAYER_GOLD_INCOME, for this resource.
---| "PLAYER_FUNGUS_INCOME"          # (player): the fungus (3) a player gets at its next turn start. As PLAYER_GOLD_INCOME, for this resource.
---| "PLAYER_SACRIFICES_INCOME"      # (player): the sacrifices (4) a player gets at its next turn start. As PLAYER_GOLD_INCOME, for this resource.
---| "PLAYER_HANDS_INCOME"           # (player): the hands of glory (5) a player gets at its next turn start. As PLAYER_GOLD_INCOME, for this resource.
---| "PLAYER_WEED_INCOME"            # (player): the enchanted weed (6) a player gets at its next turn start. As PLAYER_GOLD_INCOME, for this resource.
---| "PLAYER_TRADE_INCOME"           # (player): the trade points (11) a player gets at its next turn start. As PLAYER_GOLD_INCOME, for this resource.
---| "PLAYER_RELICS_INCOME"          # (player): the relics (12) a player gets at its next turn start. As PLAYER_GOLD_INCOME, for this resource.
---| "PLAYER_LIFEFORCE_INCOME"       # (player, drain): the lifeforce (13) a player gets at its next turn start; with drain 1, as the turn start asks, the game also drains the squares. As PLAYER_GOLD_INCOME, for this resource.
---| "PLAYER_GEM_INCOME"             # (kind, player): the gems of one kind a player gets at its next turn start, kind 0 to 3 for resources 7 to 10. As PLAYER_GOLD_INCOME; a wrap's kind stays 0 to 3.
---| "TRADE_INCOME_APPLY"            # (player): the trade step of the player's turn start, after the incomes: its trade type (playerTradeType) buys a resource with gold or sells one for gold, with that turn start's trade points (playerAiTrade). A wrap that skips next trades nothing; playerResource changes the stocks. The result is nil.
---| "RITUAL_COST_PERCENT"           # (commander, slot): the percent of its costs the ritual charges the commander, where slot is the ritual's number: 100 is the costs as listed, 50 half, 0 free. The game works it out after its discounts and rises (rebatelvl, rebatefx25 and rebatefx50, rebateterr20 and rebateterr50, the caster's classcost, the terrain's ritrebate, the rise with each cast): 100 charges the costs as listed, 50 half, 200 double, 0 nothing. Every call of the game's cost percent: the ritual screen's costs, the cast check before a cast and the payment, the AI's casts among them. A wrap's result is the percent all three use; the Sacrifice Level of a varcost ritual multiplies after it. A result below 0 is refused (out_of_range) and the vanilla value kept, 0 when the wrap skipped next. The AI's planning weighs the costs as listed, as it does the game's discounts.

-- Reading a value's name. The first word says what the first argument is:
--   player...      a player: 0 to 31, 32 to 47 for a mod's factions, or a faction's name (the notes say "per slot")
--   unit...        a unit, 0 to unitLastIndex
--   commander...   a commander, 0 to commanderLastIndex; commanderUnit gives its unit
--   square...      a square: x, then y. x runs across the whole world, where the planes lie side by side
--                  (planeX is a plane's first x, squarePlane the plane a square is on)
--   monster..., item..., class..., terrain..., ritual...   one record of that table, 0 to its ...LastIndex
--   weapon...      a weapon
--   enchant...     an active ritual effect, 0 to enchantLastIndex
--   plane...       a plane, 0 to planeLastIndex
--   battle...      the battle being fought, or the last one
--   gen...         a number of the society's world generation: a ...Chance or a ...Share is per mille unless its
--                  note says percent. The society's presets write them all at each try at a random world, so
--                  set one in a CREATE_RANDOM_MAP wrap or its before observer
--   cli...         a command line option
--   ai..., route...   a number the game's AI and its routes use
-- A value takes an argument when its note starts with "per slot" or "index", and none otherwise.
-- A few take the player first: unitBattleStrength (player, unit), and squareSeen, squareCitadel, squareBattleStrength
-- and squareRememberedBattleStrength (player, x, y). unitVisibleTo takes (unit, player); its site UNIT_VISIBLE_TO
-- takes (player, unit, x, y).
-- ...LastIndex is the highest number in use, for a loop from 0. ...Flags is a word of bits, which its note lists.
-- monster, weapon, item, class, terrain and ritual, alone, read or write one field of a record by its .c5m word:
-- coe5.get("monster", m, "hp").
-- A value the game works out (unitArmor, squareCitadel, playerAbility) is read-only, and its note names the site
-- to wrap to change what the game uses (UNIT_ARMOR).
-- Names that sit close:
--   mapW and mapH are the map's size as Random Map Settings made it. worldW and worldH are plane 0's size now.
--   mapDepth is the width of the whole world, every plane counted.
--   unitStr is the Strength a unit's screen shows. unitBattleStrength is the battle strength the AI and battles
--   compare.

---The game values coe5.get and coe5.set reach.
---@alias coe5.Variable
---| "turn"                  # integer: 0 on the first turn, -1 during setup. Writable, -1 to 250.
---| "networkGame"           # boolean. Read-only.
---| "loadedGame"            # integer: nonzero for a game loaded from a save. Setting it nonzero logs lua_loaded_game_set with your mod.
---| "loadedMapName"         # string: the map file the game started on, empty for a random map. Read-only.
---| "society"               # integer: 0 Random, 1 Dark Ages, 2 Agricultural, 3 Empire, 4 Fallen Empire, 5 Monarchy, 6 Dawn of a New Empire, 7-11 sub-societies; nothing else (the game's society table). Changes the moment the player clicks it.
---| "genAncientRuinChance"  # integer: the society's ancient ruin chance per mille (Elysium's ruins; the Sky's Ancient Temples and Magic Libraries), set when the world is built. Writable.
---| "genLibraryChance"      # integer: the society's Magic Library chance per mille, set when the world is built. Writable.
---| "genBattlefieldChance"  # integer: the society's chance per mille of a battlefield site (an Old Battlefield or a graveyard), tried on each Plain, Tundra and Savanna square of Elysium and each Clouds square of the Sky as the world is decorated. Writable.
---| "genTowerChance"        # integer: the society's tower chance per mille (Elysium's towers, the Sky's Cloud Towers), set when the world is built. Writable.
---| "genVillageChance"      # integer: the society's settlement chance per mille (the Sky's Cloud Cities, Villages and Hamlets), set when the world is built. Writable.
---| "genTownShare"          # integer: the society's town share per mille, the settlements that are towns (the Sky's Cloud Cities), set when the world is built. Writable.
---| "commonCause"           # integer: the Advanced Options checkbox. Changes the moment the player clicks it. Read during play and saved, so a change in a game counts. 0 off, 1 on: with it a player only loses when no one on its team has a citadel or a commander.
---| "scoreGraphs"           # integer: the Advanced Options checkbox. Changes the moment the player clicks it. Read during play and saved, so a change in a game counts.
---| "battleReports"         # integer: the Advanced Options checkbox. Changes the moment the player clicks it. Read during play and saved, so a change in a game counts.
---| "mapW"                  # integer: the map's width in squares, written at Ok (with a random ±1). At most 1000, the game's square arrays.
---| "mapH"                  # integer: the map's height in squares, written at Ok (with a random ±1). At most 160, the game's square arrays.
---| "startFlags"            # integer: start positions, written at Ok: 8 clustered, 16 west/east, 0 random. It is what world building reads; startDefault is what Random Map Settings opens with.
---| "wilder"                # integer: Wilder, written at Ok: 2 on, 1 off, 0 before the first game. 0 to 2, what the game stores.
---| "mapDepth"              # integer: despite the name, the width of the whole world: the map's width, raised as planes are added beside the map. At most 1000.
---| "northPart"             # integer: Random Map Settings' North/South box, the north part in percent; -1 is the default, 25. -1 to 100, what the game stores. Not saved. The north part is the percent of Elysium's height, from the top, that is the snowy north.
---| "southPart"             # integer: the south part in percent; -1 is the default, 35. -1 to 100. Not saved. The south part is the percent of Elysium's height, from the bottom, that is the arid south.
---| "playerController"        # per slot, integer: coe5.controller.human, .ai, or .removed; .independent once the world is built.
---| "playerClass"             # per slot, integer: the class's `newclass` number (1 Baron, 2 Necromancer, ...). Below 1 is Random until the classes are rolled. At most the class table's last record: 249 on Windows, 199 on Mac.
---| "playerAi"                # per slot, integer: AI difficulty, 2 (Jester) by default. Read at each turn start and saved, so a change in a game counts. At most 10: the game stops on a level outside 1 to 10 at an AI's turn.
---| "playerTeam"              # per slot, integer: below 0 is no team.
---| "playerName"              # per slot, string, up to 31 characters. Humans' names are filled in after PLAY_GAME_NEW_WORLD's next.
---| "playerColorRed"          # per slot, number 0 to 1: the red of the player's colour, the square on Setup Participants. Each new game's setup starts every slot on the palette's colour (playercolor). Takes what the game stores: a finite number from -8421505 to 8421504 (playercolor's whole numbers over 255); NaN or an infinity is refused (reason=not_finite).
---| "playerColorGreen"        # per slot, number 0 to 1: the colour's green.
---| "playerColorBlue"         # per slot, number 0 to 1: the colour's blue.
---| "playerResource"          # per slot and resource, integer: how much of resource 0-13 (resourceName's order, 0 Gold) the player has. coe5.get("playerResource", slot, resource), coe5.set("playerResource", slot, resource, value).
---| "playerAiIncome"          # per slot and resource 0-13, integer: playerIncome as the game pays an AI of the player's AI level, for a human too: a human a script plays as an AI (coe5.runAiTurn) gets a human's 100% meanwhile, and this reads what its AI would get. 0 for a slot that is neither a human nor an AI. Read-only; a wrap of PLAYER_GOLD_INCOME or another income site changes what the game pays, and this reads through them. For an AI player it equals playerIncome. For a human it is what the game would pay that player as an AI of its playerAi level.
---| "playerAiTrade"           # per slot and resource 0-13, integer: how much of the resource the trade step of the player's next turn start adds, or takes when negative. Only Gold, the traded resource and Trade (11) ever change, so the gold a player has after its next turn start is its gold now + playerAiIncome(p, 0) + playerAiTrade(p, 0). The step is as the game runs it after paying the incomes (playerAiIncome): the player's trade type (playerTradeType) buys a resource with gold or sells one for gold, with that turn start's trade points. Negative for what it pays (the gold of a buy, the resource of a sell, the trade points), positive for what it gets. Read with the stocks of now plus those incomes; the step's random draw can give one unit more or less, and the more likely is taken; type 23's gem is that draw's, so no gem is listed. 0 with no trade type. Gold's gain at that turn start is playerAiIncome plus this, if nothing changes first. The trade points are that turn start's trade income alone: a Trade stock left over is overwritten, not added. The game fights a player's battles after its turn, before the next player's turn start, so a read in an AI_TURN `after` observer comes before them; the next player's PLAYER_TURN_START `before` observer comes after. Read-only; a wrap of TRADE_INCOME_APPLY changes the step (Windows only for now), and the income sites change the trade points.
---| "playerIncome"            # per slot and resource 0-13, integer: what the player gets of the resource at its next turn start, as the game works it out then: each special resource (all but Gold, Iron and Trade) only while the player has a commander who gathers it, then an AI's difficulty percent (a human's is 100; an AI's 100 plus its level's bonus, a quarter of a positive bonus more for the special resources, and that quarter less for Gold, Iron and Trade while the AI has any special income). Trade is the trade points. 0 for a slot that is neither a human nor an AI. Read-only; a wrap of PLAYER_GOLD_INCOME or another income site changes what the game pays, and this reads through them.
---| "playerTradeType"         # per slot, integer 0-24: what the player's trade points turn into at its next turn start: 0 nothing, 1-22 a resource bought (odd) or sold (even: 1-2 iron, 3-4 sacrifices, 5-6 hands, 7-8 herbs, 9-10 fungus, 11-12 weed, 13-20 the four gems, 21-22 relics), 23 a random gem bought, 24 gems sold in turn. The game's AI picks it at the end of its move. Writable with 0 to 24; any other value is refused (out_of_range).
---| "playerTradeMode"         # per slot, integer: how much the player's trade step trades at its next turn start: 1 normal, any value above 1 doubled. The game's AI writes it with playerTradeType at the end of its move, 2 when it picks a buy with gold above 500. playerTradeType is what the trade points turn into, and this is how much. Writable.
---| "newWorldUniqueRandom"  # integer: Setup Participants' "unique random players" box, a local of the new game's setup call. Nil, and not writable, outside a wrap of a NEW_WORLD_* or PARTICIPANTS_* site, and inside PARTICIPANTS_DIFFICULTY_WAIT.
---| "cliMapW"               # integer: the command line's --mapw, below 1 when not given. Random Map Settings opens on exactly cliMapW x cliMapH when either is set. At most 1000.
---| "cliMapH"               # integer: the command line's --maph, below 1 when not given. At most 160.
---| "cliSociety"            # integer: the command line's --society, below 0 when not given. Writable.
---| "subSociety"            # boolean: the command line's --subsoc, which lets the player pick a sub-society. Writable.
---| "startDefault"          # integer: the start positions Random Map Settings opens with: 0 Random, 1 Clustered, 2 West / East; nothing else. It is what the screen opens with; startFlags is what the world is built with.
---| "wilderDefault"         # boolean: whether Random Map Settings opens with Wilder on.
---| "setupNetwork"          # boolean: true while a network game is set up. Read-only.
---| "aiLevelName"           # index 0-10, string: the AI difficulty names that playerAi indexes. Writable, cut to 99 bytes: the name the game shows from then on, until the game closes. On Mac the list that opens from a player's AI level keeps the game's names.
---| "screenW"               # integer: the game's UI width. Writable; the game works it out again each frame.
---| "screenH"               # integer: the game's UI height. UI y counts up from the bottom. Writable; the game works it out again each frame.
---| "hoveredId"             # integer: the id of the line under the pointer, as the file browser reads it right after its wait. A row id past the browser's last listed file doesn't reach its Delete (see FILE_BROWSER_WAIT). An id is the number a screen gives each line it draws, the number its _WAIT site returns on a click. The game works it out again when the pointer moves, so a write holds until then.
---| "bannerFade"            # number: the mod banner column's opacity, 0 to 1. The game lowers it every frame and draws the banners only while it is above 0.
---| "activePlayer"          # integer: the slot whose turn it is, -1 outside a game. Writable, -1 to 31, or 47 once a mod adds factions.
---| "season"                # integer: 0 Summer, 1 Autumn, 2 Winter, 3 Spring. Writable, 0 to 3.
---| "seasonName"            # index 0-3, string: the game's name for a season ("Winter"). Writable, as aiLevelName.
---| "date"                  # string: the date as the map shows it, "Mid Summer of year 0". Writable, up to 99 characters. The game builds it again each time it shows the date.
---| "worldW"                # integer: the world's width in squares. Writable, 0 to 1000, as long as plane 0's planeX plus it is at most 1000. It is plane 0's planeWidth.
---| "worldH"                # integer: the world's height in squares. Writable, 0 to 160, as long as plane 0's planeY plus it is at most 160. It is plane 0's planeHeight.
---| "uniqueRandomDefault"   # boolean: whether Setup Participants' "unique random players" box starts ticked.
---| "playerFixedClass"         # per slot, integer: the class a loaded map locks the slot to. Writable, -32768 to the class table's last record, as playerClass: the game copies the lock into the slot.
---| "playerFixedController"    # per slot, integer: the controller a loaded map locks. Writable. -1 is no lock; any other value is a coe5.controller value, from a map file's fixedplayer line.
---| "playerFixedAi"            # per slot, integer: the AI difficulty a loaded map locks. Writable, -32768 to 10. -1 is no lock. A lock of 0, or below 0 other than -1, stops the game at that AI's turn.
---| "playerFixedTeam"          # per slot, integer: the team a loaded map locks. Writable. -1 is no lock.
---| "eventVar"              # index 0-9999, integer: the .c5m event variables that setvar and +varequal use.
---| "modLastIndex"          # integer: the last index in the game's mod list. Read-only.
---| "modName"               # index 0 to modLastIndex, string: the mod's .c5m name. Read-only.
---| "modFolder"             # index 0 to modLastIndex, string: the mod's folder. Read-only.
---| "modEnabled"            # index 0 to modLastIndex, integer: nonzero when enabled on the Mods screen. Read-only.
---| "monsterName"           # index 0 to monsterLastIndex, string (empty for an unused index). Writable, any mod's or the game's, cut to 47 bytes: coe5.findMonster and the game's lookups by name then find it by the new name, as after the .c5m word name.
---| "monsterMod"            # index 0 to monsterLastIndex, string: the name of the mod whose newmonster made the monster, nil for vanilla; a later mod's selectmonster doesn't change it. Read-only.
---| "monsterLastIndex"      # integer. Read-only.
---| "monsterSprite"         # index 0 to monsterLastIndex, integer: the monster's sprite, for coe5.ui.nameBox. Writable, 0-32699.
---| "itemName"              # index 0 to itemLastIndex, string. Writable, cut to 47 bytes.
---| "itemLastIndex"         # integer. Read-only.
---| "className"             # index 0 to classLastIndex, string; 0 is "no class". Writable, cut to 63 bytes.
---| "classLeader"           # index 0 to classLastIndex, string: the class leader's monster name. Writable, cut to 47 bytes.
---| "classLastIndex"        # integer. Read-only.
---| "terrainName"           # index 0 to terrainLastIndex, string. Writable, cut to 47 bytes.
---| "fortPartWord"          # index 0 to 31, a fort part bit, string: the terrain word that sets it ("irongate" for 11, the bit of 2048), from the game's terrain reader; nil for a bit none sets. Read-only.
---| "monster"               # index: a monster 0-5999, then a word of a .c5m monster block, the parser's spelling: a field (hp, str, armor, mor, rank, mr) is an integer, a flag or move word (undead, flying, wateronly) a boolean, an ability word ("aihold", "mastery") its value, 0 for none. An unknown word is an error. coe5.set writes any mod's monster or the game's: a field within its width, a flag word true or false, an ability word as its .c5m line sets it (refused, reason=no_room, when the list is full).
---| "weapon"                # index: a weapon (weaponDmgType's), then a word of a .c5m weapon block: a field (dmg, dmgtype, aoe, range, init, trgrank, nextdmg, nextalwayswep, look, sound, flymode, flylook, flysound, sndvol, reloadsnd) an integer, a special word (drain, sweep, ...) a boolean. nextalwayswep holds the chained weapon as the game keeps it: the weapon's number for nextwep, its negative for nextalwayswep, -1 for none. coe5.set writes any mod's weapon or the game's, as a monster's; nextalwayswep within the weapons.
---| "item"                  # index: an item 0-999, then a word of a .c5m magic item block: a field (type, rarity, itemwep) an integer, a flag or move word a boolean, an ability word (armor, protection, hp, and the monster ability words) its value. coe5.set writes it as a monster's; type 0-8, itemwep -1 to the last weapon.
---| "class"                 # index: a class 0-199, then a word of a .c5m class block: hometerr an integer, an ability word (startplane, aiclass, stdtroops, goldbonus, likesnorth, ...) its value, 0 for none. coe5.set writes it as a monster's; hometerr up to 1999.
---| "terrain"               # index: a terrain 0-999, then a word of a .c5m terrain block: a field (gold, iron, trade, herbs, fungus, weed, sacr, dead, hands, pop, lifeforce, gems, apcost, batmap) an integer, a flag or fort word (citadel, cave, walls, irongate) a boolean, ritrebate, bonusrelics, destroyto, mistrare their values. coe5.set writes it as a monster's.
---| "ritual"                # index: a ritual (as ritualName), then a word of a .c5m ritual block: level and terr integers, a flag word (free, nostart, rarestart, ...) a boolean, an effect word ("maxcast", "nomonreq") its value, 0 for none. coe5.set writes it as a monster's; terr up to 1999; an effect word sets the ritual's first effect of it, else adds one.
---| "terrainLastIndex"      # integer. Read-only.
---| "terrainIncome"         # index: a terrain (as terrainName), then a resource 0-13, integer: the terrain's income of the resource per month, as its .c5m gives it (gold, iron, trade, herbs, fungus, weed, sacr, hands), before seasons, class bonuses and enchantments; nil for Gems (7-10, a square's squareGems), Relics and Lifeforce, which no terrain field gives. Writable, for a resource with a field (any other is refused, reason=no_field).
---| "ritualName"            # index 0 to ritualLastIndex, string. Writable, cut to 47 bytes.
---| "ritualLastIndex"       # integer. Read-only.
---| "resourceName"          # index 0-13, string: Gold, Iron, ... Lifeforce. Writable, as aiLevelName.
---| "incomeName"            # index 0-18, string: the game's income names, "Gold Income" ... "Lifeforce Income", then the cost types past 13: "Humanoid Corpses", "Gem Income", "Expendable Units Income", "Corpses", "Goblin Income". Writable, as aiLevelName.
---| "classRecruitName"      # index: a class (0 to classLastIndex), then an offer 0-159, string: the monster name of the class's recruitment offer (addunitrec, addmercrec, addcomrec); nil at and past the list's end. Writable, cut to 47 bytes; a write at the list's end adds an offer.
---| "classRecruitCount"     # index: a class, then an offer 0-159, integer: units per offer, -1 for a commander (addcomrec); nil at and past the list's end. Writable.
---| "classStartArmy"        # index: a class, then a line 0-15, string: a start army line, "6 Spearman" (addstartunits) or "High Lord" for a commander (addstartcom); nil at and past the last line. Writable, cut to 63 bytes; a write at the list's end adds a line.
---| "classStandardTroops"   # index: a class, boolean: the class has stdtroops, so it also recruits Spearman, Swordsman, Archer, Crossbowman, Heavy Infantry, and Catapult. The game holds those six names in its code, not in a table, so a script lists them itself. Writable.
---| "classAbility"          # index: a class, then an ability id (coe5.classAbility), integer: the class's value of it, through the game's lookup (the first match), 0 when the class has none. The game's random class roll reads aiClass, randomSkip, aiLevelMin, and aiLevelMax. Writable: the class's value, added when it has none (refused, reason=no_room, when its 16 are taken).
---| "messageBoxText"        # string: the game's message box's text, as it shows it. Nil outside a wrap of MESSAGE_BOX_WAIT. Writable in that wrap, cut to 10000 bytes. Windows only for now.
---| "mapPointerX"          # integer: the map square under the pointer in this map frame, -1 when none (it is a square of the world, planes side by side). Nil outside a wrap of MAP_SCREEN_WAIT; read it after next. Writable, -1 to 999, in that wrap.
---| "mapPointerY"           # integer: that square's y, -1 when none. Nil outside a wrap of MAP_SCREEN_WAIT. Writable, -1 to 159, in that wrap.
---| "uiPointerX"            # number: the pointer's x in UI units, from the left, the space coe5.ui.drawText and drawRect take (the UI is 914 wide at 1152x756 and wider in a wider window: coe5.mapView's screenW). Updated each frame of any screen. Writable, 0 to screenW; the game sets it again each frame.
---| "uiPointerY"            # number: the pointer's y in UI units, up from the bottom, as uiPointerX. With mouseButtons in a MAP_SCREEN_WAIT wrap, after next, a script takes a click on what it drew: test the pointer against its text's rectangle, set mouseButtons to 0 while the button is down, and return 0 for next's id (a click over the game's buttons gives their id). Writable, 0 to screenH; the game sets it again each frame.
---| "uiWheel"               # integer: the mouse wheel's running count while a preview world runs, nil outside one. A panel over the map scrolls by how far it moved since the last frame; the wheel over a coe5.setPreviewDocks rectangle doesn't zoom the map. Writable: a write of n adds n scroll steps for the next frame.
---| "mouseButtons"          # integer: this frame's mouse buttons, 1 left, 2 right, 4 middle, added; the first frame of a press, then each frame it is held. In a wrap of MAP_SCREEN_WAIT, after next, set it to 0 to keep the map from acting on the click (a left click moves the selected commander), and keep setting it to 0 while the button stays down. 0 to 7.
---| "keyHeld"               # index: a key, as coe5.key names it or a character's byte (1-399, no ctrl or alt), boolean: the key is held down now. The key itself, so a key rebindKey moved reads as the key the player presses. Every key reads false again when the game window gains or loses focus, as the game clears its key state then. Writable: true holds the key down and false lets go of it, for the game and for key actions, as a press does, until the key's next press or release on the keyboard or the next focus change.
---| "windowMode"            # integer: the window's mode now: 0 a window, 1 fullscreen, 2 a borderless fullscreen window. Writable; it records the window and doesn't change it.
---| "fullscreen"            # integer: the Preferences' Fullscreen, 0 a window, 1 fullscreen, 2 borderless; the game saves it and uses it at the next start (windowMode is the window now). Writable, 0 to 2.
---| "battleGrid"            # integer: the battle's grid, 1 shown, as its Hide/show grid (key g) sets it. Writable, 0 or 1.
---| "frameMs"               # integer: the milliseconds the last frame took, what the map's sway and zoom glide advance by. Writable; the game sets it every frame.
---| "frameCount"            # integer: the frames the game's screens have drawn since it started. Writable.
---| "mapEditorOpen"         # boolean: the map editor is open. Writable; it doesn't open or close the editor.
---| "squareSeen"            # index: a slot, then x 0-999, then y 0-159, integer: what that player sees of the square: 1 darkened, 2 seen, 3 scried, 0 none of these. The map acts on a click only on a square above 1. Writable.
---| "squareArmy"            # index: x 0-999, then y 0-159, integer: the square's army size, 0 for none, -1 while two players at war share it. Writable. The game recounts it from the units there as they come and go.
---| "squareArmyOwner"       # index: x 0-999, then y 0-159, integer: the player whose army stands on the square (the one with the most units), -1 for none or a square outside the world. Writable, -1 to the last player slot (31, or 47 with a mod's factions); the game counts it again at the square's next change.
---| "squareOwner"           # index: x 0-999, then y 0-159, integer: the player who owns the square, -1 for none. Writable, -1 to 31, or 47 once a mod adds factions. coe5.setSquareOwner writes it as the game's conquest does.
---| "squareTerrain"         # index: x 0-999, then y 0-159, integer: the square's terrain type (terrainName takes it). Writable only while the world is built (see squareName): 0 to the last terrain, through the game's terrain change, which also sets the square's gems, its owner's bookkeeping and its ground (set squareGround after it).
---| "squarePortal"          # index: x 0-999, then y 0-159, integer: the square's portal number, 0 for none. Above 0 it is an entrance, and coe5.portalExits lists where it leads; below 0 only an exit. Squares with the same number (either sign) link, on any plane, as a map file's portal x y n makes them. Writable only while the world is built (see squareName): -32768 to 32767.
---| "squareGround"          # index: x 0-999, then y 0-159, integer: the square's ground, what a map file's groundrow writes; the game sets it from the terrain and the plane. Writable only while the world is built (see squareName): 0 to 28. The ground is the texture the map draws under the terrain: 0 grass, 1 shade land, 2 jungle, 3 savanna, 4 sand, 5 swamp, 6 coast, 7 waste, 8 ash, 9 ash waste, 10 snow, 11 water, 12 deep water, 13 river, 14 frozen, 15 cave, 16 moist cave, 17 lava, 18 stone, 19 thunder, 20 and 21 cloud, 22 flames, 23 void, 24 black sea, 25 sky, 26 ford, 27 nothing, 28 field. Each month the game turns grass, savanna and swamp into snow on a cold square and back on a warm one.
---| "squareTemperature"     # index: x 0-999, then y 0-159, integer: the square's temperature, what a map file's temprow writes; below 1 is cold (snow ground and the game's cold rules), above 49 hot; at 25 or less, the game's temperature step and a plane's map file also give the square the cold flag, which draws conifers in its forest. Writable only while the world is built (see squareName): -32768 to 32767.
---| "squareName"            # index: x 0-999, then y 0-159, string: the square's name, what a map file's squarename writes, up to 31 characters (a longer one is cut, as squarename cuts it). The square values are writable only while the world is built: inside a wrap of PLAY_GAME_NEW_WORLD (the whole world), or a plane's generator (only that plane's squares). Elsewhere coe5.set is refused (not_world_building, or outside_plane for a square outside the area being built), as is a value the game never stores there (out_of_range).
---| "squarePlane"           # index: x 0-999, then y 0-159, integer: the plane the square lies on (planeName takes it), -1 for none, as the game finds it. Read-only.
---| "commanderLastIndex"    # integer: the last commander record in use or once used; a loop runs 0 to it. Writable, -1 to 9989.
---| "selectedCommander"     # integer: the commander selected on the map, whose orders the commander panel shows; -1 for none. Writable, -1 to 9989.
---| "commanderUnit"         # index 0-9989, integer: the commander's unit (unitOwner, unitHp, unitMonster take it), below 0 for an empty record. Writable, -1 to 98999.
---| "commanderRitual"       # index: a commander 0-9989, then 0-63, integer: a ritual the commander knows (ritualName, ritualSummons take it); nil at and past the first slot below 1, the list's end. Writable, 0 to the last ritual slot. Writing the list's first free place adds a ritual and ends the list after it.
---| "commanderCostOnHand" # index: a commander 0-9989, then a cost type 0-18 (as ritualCostType), integer: what the owner of the commander's unit has of it where the unit stands, as the game counts it when a ritual is cast: 0-13 the stock, 14 humanoid corpses on the square, 15 gems, 16 the player's expendable units there, 17 all corpses there, 18 the player's goblins there; a cost is paid when this is at least its amount. Nil for a commander with no unit or a unit no player owns. Read-only; wrap RESOURCE_AVAILABLE to change what the game uses.
---| "commanderAiTargetX"    # index 0-9989, integer: x of the square an AI commander's move heads for, coe5.aiMove.noSquare for none. Writable, -1 to 999.
---| "commanderAiTargetY"    # index 0-9989, integer: that square's y. Writable, -1 to 159.
---| "commanderAiMoveKind"   # index 0-9989, integer: what the move is: coe5.aiMove.none, .target (from the rally, friendly move, or conquest step), or .explore. Writable.
---| "commanderAiMoves"      # index 0-9989, integer: the moves an AI commander made this turn; its move loop stops at 4 and clears them for the next turn. Writable.
---| "commanderAiFromX"      # index 0-9989, integer: x of the square an AI commander stood on before its last step. Writable.
---| "commanderAiFromY"      # index 0-9989, integer: that square's y. Writable.
---| "commanderAiLastTargetX" # index 0-9989, integer: x of the target an AI commander's move loop ended on this turn. Writable. A record only: nothing in the game's AI reads it, and the next move loop overwrites it. commanderAiTargetX is the target the AI's moves head for.
---| "commanderAiLastTargetY" # index 0-9989, integer: that target's y. Writable. A record only, as commanderAiLastTargetX.
---| "commanderRitualPlan"   # index 0-9989, integer: the ritual an AI commander plans to cast (ritualName takes it), as the game's AI keeps it from turn to turn until it casts or scraps it; 0 for none, -1 a spawnbooster (a commander the AI keeps on its square to raise a spawn, which never moves). coe5.scrapRitualPlan drops a plan. Writable, -32768 to the last ritual slot.
---| "commanderDone"         # index 0-9989, boolean: true once the player gave the commander the map's "Commander is done for this turn" (Space; the game also marks the other commanders of its group), which the map's next-commander key then skips and End Turn's "commanders have AP left" warning leaves out; no action points spent. The game clears it at its player's next turn start, for a new commander, and on load (saves don't hold it). Writable.
---| "playerLeader"            # per slot, integer: the player's leader, a commander (commanderUnit takes it), -1 once the leader is dead. Writable, -1 to 9989.
---| "unitLastIndex"         # integer: the last unit record; a loop runs 0 to it. Writable, -1 to 98999.
---| "unitOwner"             # index 0-98999, integer: the unit's player (slot). Writable, -1 to 31, or 47 once a mod adds factions.
---| "unitHp"                # index 0-98999, integer: the unit's hit points; 0 or below is a dead unit or an empty record. Writable. A write runs no death handling; coe5.killUnit does.
---| "unitMonster"           # index 0-98999, integer: the unit's monster (monsterName takes it), below 0 for an empty record. Writable, -1 to the last monster slot.
---| "unitCommander"         # index 0-98999, integer: the commander the unit follows (commanderUnit takes it), -1 for none: a commander's unit too, unless it follows another. commanderBattleStrength adds the commander's unit and every unit following it. The AI's troop pick-up and its pick-up of lone commanders set it. Writable, -1 to 9989.
---| "unitStationary"        # index 0-98999, boolean: true when the unit never moves on the world map ("Cannot move on world map", the monster word stationary or an item's or ability's), as the game's AI checks it before it plans a commander's move; false for an empty record. Read-only; wrap UNIT_FLAGS to change what the game uses.
---| "ritualSummons"         # index 0 to ritualLastIndex, boolean: the ritual has the summoning effect: it makes one of its ritualString results, at random. Writable: true adds the summoning effect, false takes it out.
---| "ritualString"          # index: a ritual, then 0-47, string: one of its addstring results, as the .c5m wrote it ("c*Mound King", "1d3*Shade", "2*Cannon Car & 1*Mortar Car", "(167)Anakite"); nil at and past the first empty one. Writable, cut to 255 bytes; a write at the list's end adds a result.
---| "ritualRitpow"          # index 0 to ritualLastIndex, integer: the ritual's school (ritpow). Writable. The number is 500 plus the school's number in the manual's Ritual Schools table: necro, 1 there, is 501.
---| "ritualLevel"           # index 0 to ritualLastIndex, integer: its level, the ritual screen's "Level" group. Writable.
---| "ritualCostType"        # index: a ritual, then 0-3, integer: a cost's resource (resourceName takes 0-13); nil at and past the first type below 0, the list's end. Writable, up to 50 (the game's last); a write at the list's end adds a cost.
---| "ritualCost"            # index: a ritual, then 0-3, integer: that cost's base amount, before the caster's discounts (RITUAL_SCREEN_ROW's costs are the charged ones); nil where ritualCostType is. Writable.
---| "ritualTerrain"         # index 0 to ritualLastIndex, integer: the terrain it needs: a terrain type (terrainName takes it), a group below -1, or -1 for none. Writable, up to 1999.
---| "ritualEffect"          # index: a ritual, then 0-13, integer: an effect id (effectName gives its mod command); nil at and past the first id below 1, the list's end. Writable, up to 1501; a write at the list's end adds an effect.
---| "ritualEffectValue"     # index: a ritual, then 0-13, integer: that effect's value; nil where ritualEffect is. Writable.
---| "ritualFlags"           # index 0 to ritualLastIndex, integer: the ritual's 64 flag bits. Writable. Game words and numbers lists the bits; coe5.get("ritual", r, "nostart") reads one by its word.
---| "ritualTerrainName"     # index 0 to ritualLastIndex, string: the game's name for ritualTerrain, a group's included ("forest or jungle"). Read-only: write the code, ritualTerrain.
---| "ritualTerrainAt"       # index: a ritual, then x 0-999, then y 0-159, boolean: the square counts for the ritual's terrain, as the ritual screen checks it (true when it needs none); nil for a terrain code the game can't check. Read-only: wrap RITUAL_LOCATION_CHECK to change whether the ritual can be cast there.
---| "ritualEffectMet"       # index: a ritual, then an effect 0-13 (as ritualEffect), then a commander 0-9989, boolean: the effect's rule holds for the commander where it stands, as the game's checks decide it: nearby1req to nearby99req and nonearby1req to nonearby99req (the terrain within 1, 3, 5, 7 squares or on the plane, or none there); the unit rules minmonreq, monworldreq, monplayerreq, nomonreq, fewmonreq, nomonhomereq, nomonworldreq, nomonplayerreq, nomonelysiumreq, nomonmasteryreq (the ritual's "(&)", "(-)", "(+)", "(|)" strings, counted in order); promotion (an owned unit here, the caster too, named by the first line of one of the ritual's string pairs; a lone "(-)" line is skipped); levelreq, resrestrict, resreq, benrestrict, fxreq, nofxherereq, enchherereq, fortreq, nofortreq, hasportalreq, noportalreq, noplanereq, seasonreq, eventvarreq, noeventvarreq, homereq, homecitreq. An effect repeated after the first of its word is true (the game reads only the first). Nil for any other effect, or a commander with no unit, no owner, or (for the terrain rules) no square on the map. Read-only: wrap RITUAL_CAN_CAST or RITUAL_LOCATION_CHECK to change the check's answer.
---| "ritualEffectTerrainName" # index: a ritual, then an effect 0-13, string: the game's name for a nearby or nonearby effect's terrain, a group's included ("forest or jungle"); nil for any other effect. Read-only.
---| "effectName"            # index 0-1501, string: the mod command of a ritual effect id (ritualEffect), "maxcast" for 1057; nil for an id no mod command sets. Read-only.
---| "playerRitualCasts"       # per slot, then a ritual, integer: how many times that player has cast the ritual (what maxcast counts). Writable, 0 to 32000.
---| "ritualCasterMaxCast"      # index 0 to ritualLastIndex, integer: the ritual's castermaxcast, how many times each commander can cast it; 0 for no limit. Writable, 0-32000.
---| "unitRitualCasts"       # index: a unit, then a ritual, integer: how many times the commander whose unit it is has cast the ritual (what castermaxcast counts; only rituals with a castermaxcast are counted). Writable, 0 to 32000; 0 clears it.
---| "unitX"                 # index 0-98999, integer: the unit's square's x (ritualTerrainAt takes it). Writable, -1 to 999. A write moves no army record; coe5.teleportCommander and coe5.teleportUnit move a unit whole.
---| "unitY"                 # index 0-98999, integer: the unit's square's y. Writable, -1 to 159. See unitX for moving a unit whole.
---| "unitBattleX"           # index 0-98999, integer: the battle cell of the unit's bottom-left corner (its token's lowest x and y), during a battle or on an open test field; nil off the field. Writable, 0 to the field's width less 1: moves the unit's token there as the game's fight does, pushing aside any units under it (16 at most).
---| "unitBattleY"           # index 0-98999, integer: its y, 0 to 19. Writable, 0 to 19: the same move.
---| "unitApLeft"            # index 0-98999, integer: the unit's action points left this turn, as the game reckons them (3, 4 for fast, 2 for slow, less those used; 0 for an empty record). The ritual screen casts while it is above 0. Writable: sets the points used so this many are left (unitApUsed).
---| "unitApUsed"            # index 0-98999, integer: the unit's action points used this turn, the number unitApLeft subtracts; moving and the game's other actions add to it. Set it to 0 to give the unit every action point back.
---| "unitAbility"           # index: a unit, then an ability 0-1501 or its mod command ("aihold"; an unknown mod command is an error), integer: the unit's total of that ability, its monster's, items', and temporary ones, as the game adds them. A ritual's ritualRitpow is the ability that gives the caster's mastery level in its school. Writable for a living unit: coe5.set("unitAbility", u, "mr", 12) stores a temporary value of the ability on the unit (one of its 4, unitTempAbilityId) so that this total reads 12, and takes the unit's temporary value off when its total is 12 already. Refused when the unit's monster or an item gives more (below_own; so is any value below 0 for an ability the unit has none of: the game counts a temporary value only when it is the larger) or its 4 temporary places hold other abilities (no_room). Where the game adds an ability up instead of taking the largest, it adds the stored value to the monster's and the items'. Once any script is loaded, a temporary affres that brings the monster's own to 100 or more also stops Cursed and Blind, as the monster's own affres 100 does; an item's affres does not, as in the game.
---| "tooltipLineHeight"     # number: the hover box's line height in UI units (14). The box's bottom sits at 16 less 2 a line, so a text past about 8 lines starts below the screen. Writable.
---| "planeName"             # index 0-18, string: a plane of this world ("Elysium"), by the number a ritual's planereq names (squarePlane gives it); mods' planes from 11 (coe5.addPlane); nil past planeLastIndex. Writable, up to 31 characters.
---| "planeX"                  # index 0-18, integer: the plane's first x on the world map, so a square's x on the plane is its world x less this; nil past planeLastIndex. Writable, 0 to 1000 less the plane's planeWidth.
---| "planeY"                  # index 0-18, integer: the plane's first y, 0; nil past planeLastIndex. Writable, 0 to 160 less the plane's planeHeight.
---| "planeWidth"              # index 0-18, integer: the plane's width in squares; nil past planeLastIndex. Writable, 0 to 1000 less the plane's planeX.
---| "planeHeight"             # index 0-18, integer: the plane's height in squares; nil past planeLastIndex. Writable, 0 to 160 less the plane's planeY.
---| "planeFlags"              # index 0-18, integer: the plane's flags; bit 0 means a human player has seen enough of it, which Reveal Map at Game Over goes by; nil past planeLastIndex. Writable.
---| "infernoActive"           # boolean: the Inferno's wanderers on its plane may move; they wait while it is false. Writable.
---| "celestialActive"         # boolean: the Celestial Plane's wanderers on its plane may move; they wait while it is false. Writable.
---| "primalPortalsOpen"       # boolean: a portal to the Primal Plane stands open, so its gods may move. Writable.
---| "infernoCentreX"          # integer: the x of the Inferno's centre, where the Citadel of Abaddon stands. Writable.
---| "infernoCentreY"          # integer: its y. Writable.
---| "mapFilePlane"            # integer: the number of the map file's last plane line read. Writable, 0 to 9. It is the plane number the map file's last plane line named, 0 to 9, and it matters only while a map file is read. mapPlaneIndex is the plane record the square lines are written into.
---| "battleUnderground"       # boolean: the battle is on Agartha or the Inferno, where some spells are refused. Writable.
---| "battleSpoilsGold"        # integer: the gold the battle's dead have dropped so far, by their goldcarrier. Writable.
---| "battleSpoilsIron"        # integer: the iron the battle's dead have dropped so far, by their ironcarrier. Writable.
---| "worldNorthPart"          # integer: the north part world generation goes by: northPart, or 25 for the default. Writable, 0 to 100. The society presets write it at each try at a random world, so a write counts in a CREATE_RANDOM_MAP wrap.
---| "worldSouthPart"          # integer: the south part world generation goes by: southPart, or 35 for the default. Writable, 0 to 100. The society presets write it at each try at a random world, so a write counts in a CREATE_RANDOM_MAP wrap.
---| "genRuinTempleShare"      # integer: the society's share of ruins that are temples, per mille. Writable.
---| "genDeadForestChance"     # integer: the society's chance of a dead forest, per mille. Writable.
---| "genAncientForestRate"    # integer: the society's rate of ancient forests. Writable. The generator places rate x world width x world height / 15000 Ancient Forests, plus 1: the default 25 is about one for each 600 squares.
---| "genSwampChance"          # integer: the society's chance of a swamp, per mille. Writable.
---| "genBattlefieldShare"     # integer: the society's share of battlefields, per mille. Writable. It is the share per mille of genBattlefieldChance's hits on Elysium that become an Old Battlefield; the rest become a Gallows or a graveyard.
---| "genCastleChance"         # integer: the society's chance of a castle, per mille. Writable.
---| "genFordChance"           # integer: the society's chance that a river crossing is a ford, in percent. Writable.
---| "genSkyFeatureChance"     # integer: the society's chance of the Sky's rainbows and beanstalks, per mille. Writable.
---| "genRareChance"           # integer: the society's chance of rare sites, portals, Coral Towers and the Maelstrom, per mille. Writable.
---| "genMineChance"           # integer: the society's chance of a mine, per mille. Writable.
---| "droughtMonths"           # integer: the months of drought left on Elysium, 0 for none. Writable.
---| "eventState"              # integer: where the mod pass stands in an event: 1 inside a playerevent, 2 inside a squareevent, -2 after endevent, -1 after a failed line, 0 outside any. Writable. A script inside an event stops the rest of it by writing -1: below 1, every later line of the event returns at once.
---| "eventFile"               # string: the file whose events the game is running, a map's event file or <folder>/<name>.c5m. Writable, up to 2499 characters.
---| "loneHuman"               # integer: the game's one human slot, -1 for a game set up with none or several. Writable, -1 to 31, or 47 once a mod adds factions.
---| "routeMaxSkies"           # integer: the longest run of Sky or Dispersing Clouds squares a route may hold before the game routes it another way. Writable.
---| "pathLength"              # integer: how many squares the game's last route filled, at most 256. Writable, 0 to 256.
---| "battleSpeedSetting"      # integer: the Preferences' battle speed. Writable. 3 Normal, 4 Fast, 5 Faster; a lower number is slower and a higher one faster than Preferences offers.
---| "pathX"                   # index 0-255, integer: the x of a square of the game's last route, from the far end back; pathLength says how many hold one. Writable, 0 to 999.
---| "pathY"                   # index 0-255, integer: its y. Writable, 0 to 159.
---| "commanderRepeatPercent"  # index 0-9989, integer: the power percent a commander's repeat ritual is paid and cast at. Writable. It is the Sacrifice Level the ritual screen's slider set when the player ticked Repeat ritual: 100 is a normal cast, and 0 means no repeat.
---| "gameWinner"              # integer: the winner the game has settled on, -1 while the game goes on. Writable, -1 to 31, or 47 once a mod adds factions.
---| "evilTimesMonths"         # integer: the months of Evil Times left. Writable.
---| "tithePool"               # integer: the gold in the Tithe of El's pool. Writable. It is counted in gold and paid out once a year.
---| "spawnCarry"              # integer: a percent the game adds to the size of its next site or mover spawn, then clears. Writable. The game sets it before most site spawns, to the largest sitepopboost of the units on the site, so a write is overwritten before those.
---| "rulershipSlot"           # integer: the slot the rulership check found, -1 for none. Writable, -1 to 31, or 47 once a mod adds factions. Every count of living units sets it again, so a write lasts until the next count.
---| "messageLastIndex"        # integer: the last message in the game's list, -1 when empty. Writable, -1 to 999. The list holds the messages each player reads on the Messages screen, 1000 at most.
---| "autosaveOn"              # integer: nonzero while autosave is on. Writable.
---| "endTurnWarning"          # integer: the Preferences' End turn warning checkbox. Writable.
---| "nextCommanderMode"       # integer: the Preferences' Next Commander Selection. Writable. 0 Manual, 1 Select first commander automatically, 2 Always select next commander when out of AP.
---| "graphicsDetail"          # integer: the Preferences' decoration detail: below 10 a quarter of the pieces, 10 half. Writable. Preferences' Graphics Quality: 9 Low, 10 Medium, 11 High, 12 Highest, 13 Ultra.
---| "selectedCommanderGroup"  # integer: the order group of the selected commander when the map took it. Writable. An order group is the number commanders that move together share; each commander is made with one no other holds, and Sentry sets it to -2. The map's arrow keys and Enter Gateway move every commander of this group.
---| "routeBoundOther"         # integer: the strength the current route may not run into, for an independent army. Writable. The game sets it at the start of each route, to the moving army's battle strength x 100 / aiAttackDivOther less aiAttackMargin, and only that route reads it, so a write between routes changes nothing. routeBoundAi and routeBoundHuman are the same bound for an AI player's and a human player's army.
---| "routeBoundAi"            # integer: the same for an AI player's army. Writable. As routeBoundOther, from aiAttackDivAi.
---| "routeBoundHuman"         # integer: the same for a human player's army. Writable. As routeBoundOther, from aiAttackDivHuman.
---| "routeExemptX"            # integer: the x of the square a route is never refused for strength: a player's move order's target. Writable.
---| "routeExemptY"            # integer: its y. Writable.
---| "routeMaxSeas"            # integer: the longest run of sea squares a route may hold before the game routes it another way. Writable.
---| "pathNoSea"               # integer: 1 while the current route refuses sea. Writable.
---| "pathNoSky"               # integer: 1 while the current route refuses Sky. Writable.
---| "stepTargetX"             # integer: the x of the square the current move step is going to. Writable.
---| "stepTargetY"             # integer: its y. Writable.
---| "freeTroopsCount"         # integer: the free troops on the square the AI is gathering from. Writable.
---| "battleDelay"             # integer: the battle's current per-step delay; the speed keys change it. Writable.
---| "battleOver"              # integer: nonzero when a battle is told to stop early. Writable.
---| "battleNotASiege"         # integer: nonzero for a battle with no siege setup, where assassin weapons do not fire. Writable.
---| "mapVersion"              # integer: the version of the map file last read. Writable.
---| "mapPlaneIndex"           # integer: the plane a map file's lines write into, -1 before its mapsize. Writable, -1 to the plane table's last record (10, or 18 with mods' planes). mapFilePlane is the number the file's plane line named; this is the plane record its mapsize line made.
---| "sampleHighest"           # integer: the highest sound coe5.playSample can play, -1 at reset. Writable, -1 to 248.
---| "recruitLocationLast"     # integer: the Recruit screen's last location, -1 for none. Writable, -1 to 255.
---| "genRealmWord"            # string: "Kingdom" or "Empire", the word the society gives the realm. Writable, up to 47 characters.
---| "battleAttackerRankCount" # index 0-2, integer: the attacker's mobile units this round: of rank -1, of ranks -1 and 0, and all. Windows only for now. Writable.
---| "battleDefenderRankCount" # index 0-2, integer: the same for the defender. Windows only for now. Writable.
---| "battleSummons"           # index a monster 0-5999, integer: the summons of the monster in this battle so far, -1 after a failed one. Writable.
---| "autoNameLast"            # index a name list 0-61, integer: the list's last name: the game draws a name from 0 to this. -1 is an empty list. The game counts every list at its first draw of a name, so a write before that is overwritten. Writable, -1 to 1499.
---| "mapFileHasPlane"         # index a plane 0-10, integer: 1 for each plane line the map file holds. Writable.
---| "aiLevelBonus"            # index an AI level 0-10, integer: the percent that AI level adds to its income. Writable.
---| "recruitLocationX"        # index 0-255, integer: the x of a Recruit screen location. Writable, 0 to 999.
---| "recruitLocationY"        # index 0-255, integer: its y. Writable, 0 to 159.
---| "unitHomeX"               # index a unit, integer: the x of the unit's home square, as homereq compares it. Writable.
---| "unitHomeY"               # index a unit, integer: its y. Writable.
---| "itemSlotType"            # index an item, integer: the kind of equipment position the item fits. Writable, 0-8. The value an item's type word sets: 0 none, 1 weapon, 2 missile weapon, 3 helmet, 4 body armor, 5 gauntlets, 6 boots, 7 misc item, 8 a bad item type.
---| "planeLastIndex"        # integer: the world's last plane, as the game counts them, 10 without a mod's plane and up to 18 with them; planeName runs 0 to it. Writable, -1 to the plane table's last record (10, or 18 with mods' planes).
---| "planeTint"             # index 0-18, four numbers r, g, b, strength, each 0 to 1: the colour the map lays over the screen while the plane is in view, as the plane's record holds it (red at 0.15 on Elysium and the Sky while a Blood Sun is up); strength 0 while none is laid; nil past planeLastIndex. Writable: coe5.set("planeTint", plane, r, g, b, strength), as coe5.setPlaneTint.
---| "ticksMs"               # integer: the game's millisecond clock. Only the difference between two reads means anything, and it can jump back when it wraps. Read-only.
---| "traceLevel"            # integer: the game's debug trace level, 0 in a player's game (each -d on the command line adds 1). At 1 the game writes its trace (log.txt in the game folder on Windows, standard output on Mac), at 2 and 3 more of it; above 0 the game also shows a few debug details (a unit of no one named with its number). coe5.keepTraceLines picks the lines. 0 to 3.
---| "bugChecksOff"          # boolean: true skips the game's bug checks, as its --nochecks switch does. At trace level 2 and 3 the game runs them at many places a turn (each over every unit), and they also fix some of what they find.
---| "commanderName"         # index 0-9989, string: the commander's name ("Agrippa"); the trace names it "<name> the <monster>". Writable.
---| "commanderBattleStrength" # index 0-9989, integer: the battle strength of the commander's army as the game counts it for the AI's moves (the comstr its AI sites get): the commander's unit and every unit following it. Nil for a commander with no living unit. Read-only; wrap UNIT_BATTLE_STRENGTH or COMMANDER_BATTLE_STRENGTH to change what the game uses.
---| "commanderGarrisonLeft" # index 0-9989, integer: the battle strength that stays on the commander's square when the commander leaves as the AI moves it: its army, the free troops the AI's gather adds to it there, less the units the gather leaves behind, by the game's rule, measured as squareBattleStrength measures the owner's units (the fort and ranged raise worked out on the staying units). The gather's one random drop (a follower with unit flag 8 that is a tenth to a quarter of the army, 50%) counts as leaving. Nil for a commander with no living unit on a square. Read-only; wrap AI_GATHER_TROOPS to change what the gather takes and leaves.
---| "playerHomeX"             # per slot, integer: the player's home citadel's x, -1 for none. Writable, -1 to 999.
---| "playerHomeY"             # per slot, integer: the player's home citadel's y. Writable, -1 to 159.
---| "squareGems"            # index: x 0-999, then y 0-159, then a gem type 0-3, integer: the square's gem income of that type; the game lets a player with a gem gatherer own a square with any above 0. Writable.
---| "squareCorpses"         # index: x 0-999, then y 0-159, then a corpse kind 0-7 (coe5.corpseKind), integer: the square's corpses of that kind, what Raise Dead, Raise Animals, reanimate, the ritual corpse costs and the corpse event triggers use. Set 0 to 32767.
---| "squareBattleStrength"  # index: a slot (or -1 for every player), then x 0-999, then y 0-159, integer: the battle strength of the units on the square of that player and its teammates, now, as the game measures it for the AI: each living unit's battle strength as that player sees it, the sum raised by (100 + 60 x the square's fort level)% when over a tenth of the units are ranged. Sees through fog. Read-only; wrap UNIT_BATTLE_STRENGTH or SQUARE_BATTLE_STRENGTH to change what the game uses.
---| "squareArmyStationary"  # index: x 0-999, then y 0-159, boolean: true when the game never moves the army on the square on the world map: every living unit there is stationary ("Cannot move on world map"), as the independents' moves check it. Also true for a square with no living unit. Sees through fog. Read-only; coe5.wrap("MOVE_INDEPENDENT_ARMY") without next holds an army.
---| "squareArmyHolds"       # index: x 0-999, then y 0-159, boolean: true when the independents' moves hold the army on the square, by the game's two hold tests before its stationary one: the square's hold flag (the event command holdit 1, a gold mine's attackers) or the hold flag on its top unit, the unit the move finds for the army's owner. False for a square with no army. Sees through fog. Writable: true holds the army (the hold flag), false takes both hold flags off.
---| "squareCitadel"         # index: a slot (or -1 for any class), then x 0-999, then y 0-159, boolean: true when the square is a citadel for that player, as the game counts a player's citadels and its AI's conquest scores them: the terrain's citadel flag, or a citadel only the player's class has (its forest, wooden or mine citadels, a class's citadel terrain or enchantment there). Whoever owns it. Sees through fog. Read-only; wrap SQUARE_CITADEL to change what the game counts.
---| "squareRememberedBattleStrength" # index: a slot, then x 0-999, then y 0-159, integer: the battle strength that player's AI remembers on the square, what the game's conquest and threat checks read: every unit's battle strength there (as squareBattleStrength with -1) when the player's AI turn last saw it, or, once forgotten (5% a turn on a square out of view), the battle strength there now, which the read doesn't store; 0 off the world. Writable, -1 up; the AI's next look at the square replaces it.
---| "unitBattleStrength"    # index: a slot (or -1 for no player), then a unit 0-98999, integer: the unit's battle strength as that player counts it, the per-unit battle strength squareBattleStrength and commanderBattleStrength add up: the player's unit with its armor, items and afflictions (pass unitOwner for the battle strength its owner sees), any other unit, allies' too, as the bare monster it shows, without armor or items. 0 for a unit that isn't alive. Not its Strength stat, which is unitStr. Read-only; wrap UNIT_BATTLE_STRENGTH to change what the game uses.
---| "unitArmor"             # index 0-98999, integer: the unit's armor as its unit screen shows it: its monster's or items' armor, protection, and the states that add to it. Nil for a unit that isn't alive. Read-only; coe5.wrap("UNIT_ARMOR") changes what the game uses.
---| "unitStr"               # index 0-98999, integer: the unit's Strength as its unit screen shows it: its monster's str with what its items, afflictions and benefits add, worked out at each read. Nil for a unit that isn't alive. Read-only; coe5.wrap("UNIT_DAMAGE_BONUS") changes what the game uses, and this read with it.
---| "unitVisibleTo"         # index: a unit 0-98999, then a slot, boolean: whether that player sees the unit, as the game decides what the map shows: its unit and any unit without stealth on any square (squareSeen tells the fog); a stealthy one only scried, or in view with a unit of that player next to it with spiritsight (acutesenses against plain stealth). False for a unit that isn't alive. Read-only; wrap UNIT_VISIBLE_TO to change what the game uses.
---| "monsterAttackType"     # index: a monster 0-5999, then a row 0-15, integer: the kind of the monster's attack in that row, by its mod command: 1 meleeweapon, 2 rangedweapon, 3 spellweapon, 4 meleeweaponbonus, 5 rangedweaponbonus, 6 spellweaponbonus, 7 rangedweapon25, 8 meleeweaponspec, 10 prebatweapon, 11 rangedweapon50, 12 assassinweapon, 13 siegeweapon, 14 spellweapon50, 15 spellweaponsingle, 17 meleeweapon50s, 18 rangedweapon50s, 19 spellweapon50s, 21 rangedweapon50x; nil at and past the list's end (the first row whose type is below 1 or weapon below 0, as the game ends it). Writable; a write at the list's end adds a row.
---| "monsterAttackWeapon"   # index: a monster, then a row 0-15, integer: the row's weapon (weaponDmgType takes it), or for a spell attack (types 3, 6, 14, 15, 19) its spell path; nil where monsterAttackType is. Writable, -1 to the last weapon.
---| "monsterAttackValue"    # index: a monster, then a row 0-15, integer: the row's number as the .c5m wrote it: the damage value, a special damage weapon's kind (a bitmask: 262144 Confusion, 4294967296 a Web's Stuck in Net), or a spell attack's level; nil where monsterAttackType is. Writable.
---| "weaponDmgType"         # index 0-4999, integer: the weapon's damage type (dmgtype): 1 slash, 2 blunt, 3 pierce, 4 fire, 5 cold, 6 shock, 7 magic, 8 fear, 9 poison, 10 acid, 11 phantasm, 12 special damage (its kind in the attack row's value), 13 special benefit, 14 charm, 15 polymorph, 16 sleep, 17 paralyze, 18 summon, 19 enslave, 20 swallow, 21 mirror image, 22 corrupt, 23 banish, 24 petrify, 25 animate, 26 heal. Writable.
---| "ceremonyName"          # index 0 to ceremonyLastIndex, string: the game's name for a Voice of El ceremony number, its label on the Ceremonies screen: "Convert Settlement", "Break Seal", "Bolster Devotion", "Start Crusade", "Call the Inquisition", "Ceremony of Mastery" (three numbers, one per rank it raises a priest to), "Increase Divine Favor"; nil for a number with none. Match ceremonies by it, never by their numbers. Read-only.
---| "ceremonyLastIndex"     # integer: the last ceremony number the game takes; ceremonyName runs 0 to it. Read-only.
---| "unitXp"                # index 0-98999, integer: the unit's experience (xp). Writable. coe5.unitAddXp gives experience as the game does.
---| "unitKills"             # index 0-98999, integer: the unit's kills. Writable.
---| "unitItem"              # index: a unit, then an equipment place 0-19: integer, the item there (itemName takes it), -1 for an empty place. 0-7 are the game's; 8-19 only for a monster whose itemslots lines give it more than 8. A place keeps its number when an earlier one is empty. Writable, -1 to the last item: an item goes in as the game gives one (the place's kind must fit it, and its equip effects run).
---| "unitKnownSpell"        # index: a unit, then 0-59: integer, a spell the unit knows. Nil at and past the first value below 0, the list's end. Writable, -1 to the last spell slot. Writing the list's first free place adds a spell and ends the list after it.
---| "unitMemorizedSpell"    # index: a unit, then 0-29: integer, a spell the unit memorized for battle. Nil at and past the list's end. Writable, -1 to the last spell slot. Writing the list's first free place adds a spell and ends the list after it.
---| "squareItem"            # index: x, y, then a place 0-5: integer, an item lying on the square (itemName takes it), -1 for an empty place. Writable, -1 to the item table's last number (999, or 1999 with the bigger table).
---| "commanderRepeatRitual" # index 0-9989, integer: the ritual the commander repeats each turn (ritualName takes it), -1 a slave hunt. Writable, -1 to the last ritual slot. 0 is no repeat. Each turn the game pays for the ritual and casts it at commanderRepeatPercent, and a cast that can't go ahead clears the repeat.
---| "playerBlessingRolls"         # index 0-31, integer: the random blessing effects the player has had (PLAYER_RANDOM_BLESSING: Bolster Devotion, gaindarkbless, tattoos; the Celestial Plane's player starts at 7). Bolster Devotion's cost grows with it. Writable.
---| "playerDivineFavor"       # index 0-31, integer: the player's Divine Favor, 0 to 4. Writable. It is the Voice of El's level, which the Increase Divine Favor ceremony raises and which sets the chance of El's freespawn.
---| "sealsBroken"           # integer: the seven seals broken, 0 to 7, one count for every player. Writable.
---| "sealEventsDone"        # integer: the seals whose event has happened; the game catches up one seal a turn. Writable.
---| "starsReliability"      # integer: the stars' reading for summoning control, -2 to 2 (2: "The stars are perfect for reliable summonings."). Writable. One reading for every player, rolled again each time starsMonths runs out; it changes whether a summoning obeys.
---| "starsPower"            # integer: the stars' reading for summoning size, -2 to 2 (2: "Summonings would be very powerful now."). Writable. Rolled with starsReliability; it changes how many units a summoning brings.
---| "starsMonths"           # integer: the months the stars' readings still hold. Writable.
---| "starsFlags"            # integer: 1 when the stars can't be read, 2 the water star. Writable. A word of bits: 1 the stars can't be read now, 2 the water star, under which a summoning by the stars on water may bring a Kraken.
---| "apocalypseMonths"      # integer: 0 until the seventh seal's event, then the months of the apocalypse so far. Writable.
---| "magicTaintedMonths"    # integer: the months of tainted magic left ("Horrors are drawing closer to our world and magic has become tainted"), 0 for none; the game's AI casts no ritual meanwhile. Writable.
---| "mapZoom"               # number: the map view's UI units per square: 48 at the start, Page Up and Down glide it between 8 (12 outside the map editor) and 100. Writable, 8 to 100.
---| "mapCameraX"            # number: the world x, in squares, at the map view's centre column. Writable, 0 to 1000; coe5.centerMap moves the view whole.
---| "mapCameraY"            # number: the y, in squares; the square at the centre row is mapCameraY + 1. Writable, 0 to 160.
---| "mapViewPlane"          # integer: the plane the map view shows (planeName takes it). Writable, 0 to the last plane; coe5.viewPlane also swaps in the plane's camera and zoom.
---| "enchantLastIndex"      # integer: the last record of the active ritual effects in use, at most 999. Writable, -1 to 999.
---| "enchantType"           # index 0-999, integer: the effect's type, 0 for a free record. Writable. The number is the one a ritual's squareench gives, so its ritual names it: coe5.get("ritualName", coe5.get("enchantRitual", e)). The game makes 29 for an illusion, which shows the square as another terrain to other teams, and 31 to 51 for the guilds.
---| "enchantCaster"         # index 0-999, integer: the unit that cast the effect. Writable, -1 to 98999.
---| "enchantOwner"          # index 0-999, integer: the caster's player. Writable, -1 to 31, or 47 once a mod adds factions.
---| "enchantRitual"         # index 0-999, integer: the ritual that made the effect (ritualName takes it), 0 or below for none. Writable, up to the ritual table's last record (2999, or 10750 with the Ritual Expansion).
---| "enchantX"              # index 0-999, integer: the effect's square. Writable, up to 999.
---| "enchantY"              # index 0-999, integer: its y. Writable, up to 159.
---| "battleSeed"            # integer: the random seed of the battle being fought, or of the last one. Writable.
---| "battleX"               # integer: that battle's square. Writable, -1 to 999.
---| "battleY"               # integer: its y. Writable, -1 to 159.
---| "battleAttacker"        # integer: that battle's attacking player. Writable, -1 to 31, or 47 once a mod adds factions.
---| "battleDefender"        # integer: that battle's defending player. Writable, -1 to 31, or 47 once a mod adds factions.
---| "battleWidth"           # integer: that battle's battlefield columns. Writable, 28 to 1536.
---| "battleRound"           # integer: that battle's round: -15 first, movement from 0. Writable.
---| "battleWinner"          # integer: the winning side's player once the fight ends, -1 for none. Writable, -1 to 31, or 47 once a mod adds factions.
---| "battleStarveRound"     # integer: the round past which every unit takes (round - it) damage each round. Writable.
---| "battleChargeRound"     # integer: the round from which ranks 0 and -1 stop holding. Writable.
---| "battleAmbush"          # boolean: that battle is an ambush. Writable.
---| "battleAtSea"           # boolean: that battle is at sea. Writable.
---| "battleSiege"           # boolean: that battle is a siege. Writable.
---| "battleReplay"          # boolean: a battle replay is refighting a battle (its rounds and damage reach BATTLE_ROUND and DAMAGE_UNIT). Writable.
---| "eventPlayer"           # integer: the player of the event being run. Writable, up to the last player slot (31, or 47 with a mod's factions).
---| "eventTargetX"          # integer: that event's target square, -1 for none. Writable, up to 999. A playerevent starts on the player's home citadel, a squareevent on each marked square in turn, and randloc and setloc move it. eventSquareX is the square a squareevent's trigger is being tested on.
---| "eventTargetY"          # integer: its y. Writable, up to 159. As eventTargetX.
---| "eventSquareX"          # integer: the square an event's trigger is being checked on. Writable, up to 999. A squareevent tests each trigger on each marked square in turn, and this is the square under test. eventTargetX is where the event's actions act.
---| "eventSquareY"          # integer: its y. Writable, up to 159. As eventSquareX.
---| "unitLastSpawned"       # integer: the last unit a populate or a commander spawn made. Writable, -1 to 98999. Read it right after coe5.newUnits to find the last unit made; coe5.spawnUnit and coe5.spawnCommander return theirs. The next populate overwrites it.
---| "commanderLastSpawned"  # integer: the last commander a populate or a commander spawn made. Writable, -1 to 9989. The next populate of a commander overwrites it; its unit is in unitLastSpawned.
---| "unitTempAbilityId"     # index: a unit, then 0-3: integer, a temporary ability's id (effectName and unitAbility take it): 0 at the list's end, -1 for a free place. Writable, -1 to 1501.
---| "unitTempAbilityValue"  # index: a unit, then 0-3: integer, that temporary ability's value. Writable.
---| "playerBlessingListId"        # index: a slot 0-31, then 0-23: integer, a blessing the player has. Nil at and past the first id 0, the list's end. Writable, 0 to 1501. coe5.addPlayerBlessing adds one the game's way.
---| "playerBlessingListValue"     # index: a slot, then 0-23: integer, that blessing's value. Nil at and past the list's end. Writable.
---| "playerAiMostCitadels"        # index 0-31, integer: the most citadels the player's AI has counted at the start of its moves, 0 for a player no AI has moved; the game's AI plays desperate above 2 with under 2 now. Writable.
---| "playerCitadels"          # index 0-31, integer: the citadels the player holds now, as the AI's turn counts them for its desperate play and its conquest. Read-only; coe5.wrap("AI_COUNT_CITADELS") changes what the AI counts.
---| "aiMoveDelay"           # integer: the Preferences' Delay for AI moves, in milliseconds; 0 shows no AI move. Writable, 0 to 10000.
---| "aiGathersHands"        # integer: what the moving AI player gathers of hands, as its commanders' routine sets it before they move and its conquest step scores squares by it; the last AI's until the next AI moves. Writable. 1 when the AI player moving now gathers Hands of Glory, else 0; its conquest then weighs a square's hands. The game's AI sets it before its commanders move and it holds that AI's value until the next AI moves, so a write counts in a wrap of that AI's moves.
---| "aiGathersHerbs"        # integer: as aiGathersHands, for herbs. Writable. 1 or 0, as aiGathersHands.
---| "aiGathersFungus"       # integer: as aiGathersHands, for fungus, the player's fungus ability added. Writable. A weight, not a flag: 0 when the AI player doesn't gather fungus, else 100 plus its class's fungus weight.
---| "aiGathersSacrifices"   # integer: as aiGathersHands, for sacrifices. Writable. 1 or 0, as aiGathersHands.
---| "aiGathersWeed"         # integer: as aiGathersHands, for weed. Writable. 1 or 0, as aiGathersHands.
---| "aiGathersGems"         # integer: as aiGathersHands, for gems. Writable. 1 or 0, as aiGathersHands.
---| "aiGathersLifeforce"    # integer: as aiGathersHands, for lifeforce. Writable. 1 or 0, as aiGathersHands.
---| "aiWeighsIron"          # boolean: the AI's conquest weighs iron, set once any AI player's iron ability passed 40 and never cleared. Writable.
---| "aiSaveResource"        # index: a cost type 0-18 (ritualCostType's), integer: nonzero when a failed ritual payment flagged it, so the AI's plan choice spares it; its commanders' routine clears them each turn. Writable.
---| "playerAbility"           # index: a slot 0-31, then an ability 0-1501 or its mod command: integer, the player's value of it as the game reckons it: its class's value, then the best among its living units. Read-only; wrap PLAYER_ABILITY to change what the game uses.
---| "playerAbilitySum"        # index: as playerAbility: integer, the class's value, then the sum over its living units. Read-only; wrap PLAYER_ABILITY to change what the game uses.
---| "unitStateFlags"        # index 0-98999, integer: the unit's state bits with its items' (afflictions, insane 2^13, Fatigued 2^37). Nil for a unit that isn't alive. Read-only; wrap UNIT_STATE_FLAGS to change what the game uses. Game words and numbers lists every bit. coe5.addAffliction, coe5.removeAffliction and coe5.hasAffliction change and test the unit's bits.
---| "unitReturns"           # index 0-98999, boolean: the unit comes back from death (immortal, reforming), as the game decides it. Nil for an empty record. Read-only; coe5.wrap("UNIT_RETURNS") changes what the game uses.
---| "ceremonyCost"          # index 0 to ceremonyLastIndex, integer: a ceremony's cost as the game worked it out for the commander of the running CEREMONY_COSTS wrap (see there for the ceremonies the game multiplies it for), after that wrap's next (before it, the last costs the game worked out); nil outside the wrap, and for a number with no cost. Writable in it, 0 to 100000: the screen shows, the ceremony charges, and the AI weighs the new one (lua_refused reason=no_cost for a number with none, out_of_range past the bounds).
---| "weaponAoe"             # index 0-4999, integer: the weapon's area (aoe): 0 a single target, n a burst of n, -n n strikes, 1000s beams and cones. Writable.
---| "weaponName"            # index 0-6998, string: the weapon's name, "" for an empty slot. Writable, cut to 63 bytes.
---| "unitMoveFlags"         # index 0-98999, integer: the unit's move flags, the game's sum of its monster's move words and its items' (boots of flying): flying 1, immobile 4, slow 8, fast 16, swamp 32, water 64 (swims or sails), float 128, mountain 256, noland 512, passwall 2^32, stationary 2^35 (also for a unit the game holds in place), teleport 2^39, shipmove 2^40, tunnel 2^42, and the terrain words (forest1 2^23, snow 2^37 and the rest). Test a bit with &: coe5.get("unitMoveFlags", u) & 1 ~= 0 flies. It shows another player's unit with its items, which the game's AI never weighs: for what the AI may know, read monsterFlags or monsterMoveFlags of its unitMonster. Nil for an empty record. Read-only; wrap UNIT_FLAGS to change what the game uses.
---| "unitFlags"             # index 0-98999, integer: the unit's flag words, its monster's and items' added as the game adds them (resrestrict's test): human 2^3, undead 2^4, spiritsight 2^5, stealth 2^17, invisible 2^22, ethereal 2^26, demonic 2^33, foreststealth 2^44, desertstealth 2^41, snowstealth 2^56, and the rest of the monster flag words. Shows another player's items, as unitMoveFlags does. Nil for an empty record. Read-only; wrap UNIT_RESISTANCES to change what the game uses.
---| "monsterFlags"          # index 0-5999, integer: the monster's flag words, as unitFlags' bits, as the .c5m set them: what anyone who knows the monster knows. Writable.
---| "monsterMoveFlags"      # index 0-5999, integer: the monster's move words, as unitMoveFlags' bits. Writable.
---| "monsterHp"             # index 0-5999, integer: the monster's hit points, a new unit's most before its items. Writable.
---| "monsterAbility"        # index: a monster 0-5999, then an ability 0-1501 or its mod command ("mastery"), integer: the monster's value of that ability as the game's lookup reads it (its first match), 0 for none; no items or temporary ones (unitAbility adds those for a living unit). Reads a monster no unit is yet: what a Ritual of Mastery makes (mastery 0x32b, an offset to the next form; montag 0x320), a school's level (ritualRitpow's ability), libmastery 0x32d. Writable: the monster's value, added when it has none.
---| "commanderMoveFlags"    # index 0-9989, integer: the move flags of the commander's army as the AI's routes test them: its unit's move flags, widened (flying or float adds water 64), with each living unit following it (a bit stays only when every unit has it, noland and slow when any has). Either bit of 192 (water 64, float 128) lets the AI's conquest take a sea square; either bit of 129 (flying, float) lets an army into the Sky. Shows the army's items. Nil for a commander with no living unit. Read-only; wrap ARMY_MOVE_FLAGS to change what the game uses.
---| "commanderRitpow"       # index 0-9989, integer: the commander's ritual school, the first ritpow ability (501 up, as ritualRitpow names a school) its unit has; 0 for none. The AI ranks casters by its level (commanderRitualLevel): the pick-up of lone commanders, a split's new leader. Nil for a commander with no living unit. Read-only. 0 for a commander with no school. A commander casts a ritual when this equals the ritual's ritualRitpow and commanderRitualLevel is at least the ritual's level.
---| "commanderRitualLevel"  # index 0-9989, integer: the commander's level in that school (unitAbility of it), 0 for none; the AI's apprentice spam runs for level 1. Nil for a commander with no living unit. Read-only.
---| "commanderSpellLevel"   # index 0-9989, integer: the best level of the commander's spell attacks, with its magic and path boosts, any path, as the game's library and memorized spells reckon it; 0 for none. Shows the boosts of another player's items. Nil for a commander with no living unit. Read-only; wrap UNIT_PATH_LEVEL to change what the game uses.
---| "terrainFlags"          # index: a terrain (as terrainName), integer: its 64 flag bits, the ones the AI reads: 1 a citadel, 2 burnable (the AI won't step a burnforest army onto it), 4 water (sea), 2^17 frozen water that thaws (the AI won't step onto it from other terrain in Winter or Spring with seasonTurnsLeft 2 or less), 2^21, 2^23 and 2^26 weigh in conquest's score (2^23 with a mine citadel class), 2^34 and 2^35 a library of level 1 and 2. Writable. Game words and numbers lists each bit by its .c5m word.
---| "squareHasFort"         # index: x 0-999, then y 0-159, boolean: the square has a fort, as the game's AI buy asks it for an aihold monster. False off the world. Read-only; wrap SQUARE_FORT_PARTS to change it.
---| "squareWaterlike"       # index: x 0-999, then y 0-159, boolean: the square counts as water where world generation and battles ask: sea terrain and the crossings (bridges, ports, fords, the Ferry, Frozen Sea, Coral Towers and the like), not lakes or rivers. True off the map. Read-only; wrap SQUARE_WATERLIKE to change it.
---| "squareOpenLand"        # index: x 0-999, then y 0-159, boolean: the square is open land, as world generation picks squares for its sites: not waterlike, not sea or flooded, and its terrain has no nostart. False off the map. Read-only; wrap SQUARE_OPEN_LAND to change it.
---| "squareLibraryLevel"    # index: x 0-999, then y 0-159, integer: the square's library level as the AI's conquest scores it (100 a level): its terrain's library bits and the square owner's library enchantments there. 0 off the world. Read-only; wrap SQUARE_LIBRARY_LEVEL to change it.
---| "seasonTurnsLeft"       # integer: the months left in this season, this one included (+seasonleft): 3 in its Early month, 2 in Mid, 1 in Late; in Summer, Autumn and Winter the weather may add one more month. Writable.
---| "aiAttackDivOther"      # integer: the moving AI's attack divisor for an army of an independent or no one, which its turn sets before it recruits: 200 (175 while its team is over 4 times its enemies' strength), 50 less when desperate (fewer than 2 citadels after more than 2), 25 less on its attack turns (10%). Its conquest attacks a square whose remembered strength is at most comstr * 100 / divisor - 15 (aiAttackMargin), and its routes won't end on a stronger army (AI_MOVE_COMSTR). Writable, 1 or more.
---| "aiAttackDivAi"         # integer: the same for an AI's army: 200, 125 while over 4 times, else 150 when 2 or more enemies and the team at most 2 times them. Writable, 1 or more.
---| "aiAttackDivHuman"      # integer: the same for a human's army: 175, 125, or 150 as aiAttackDivAi. Writable, 1 or more.
---| "aiAttackMargin"        # integer: 15, taken off after the division. The game's conquest and routes hold it in their code. Writable, 0 to 127: a write changes it there, for every AI player, until the game closes; refused on a game build CoE5SE doesn't know (game_code_differs, unknown_game_build).

-- ---- The coe5 table ---------------------------------------------------------------------------

---Every call, site and game value is reached through this table. Its calls are declared in coe5_calls.lua.
---@class coe5
coe5 = {}

-- ---- Game numbers ------------------------------------------------------------------------------
-- The game's numbers that a script passes to CoE5SE or gets back, by name. The values are the
-- game's, and the same as the C names in jazzy_api.h (given with each table).

---The game's main menu buttons, for coe5.addMainMenuButton's afterId (C: MAIN_MENU_*). The ids
---the table skips (5, 7, 8) have no button.
---@class coe5.mainMenu
coe5.mainMenu = {
  startNewGame = 1,
  loadGame = 2,
  preferences = 3,
  network = 4,
  credits = 6,
  quit = 9,
  mods = 10,
  readTip = 11,
}

---What a Game Menu line's onClick returns (C: GAME_MENU_*).
---@class coe5.gameMenu
coe5.gameMenu = {
  stay = 0,    -- the menu stays open
  close = 1,   -- the menu closes, as Ok does
  resign = 6,  -- the player resigns, with no question asked
  save = 7,         -- the save dialog; the menu stays open
  saveAndQuit = 2,  -- the save dialog, then the game exits
}

---Random Map Settings' start positions for allied players, for coe5.startPreviewWorld's startPositions (C: START_POSITIONS_*).
---@class coe5.startPositions
coe5.startPositions = {
  random = 0,
  clustered = 1,
  westEast = 2,
}

---Random Map Settings' five map sizes, for coe5.startPreviewWorld's mapSize (C: MAP_SIZE_*).
---@class coe5.mapSize
coe5.mapSize = {
  small = 0,
  medium = 1,
  large = 2,
  huge = 3,
  enormous = 4,
}

---Which section of the Game Menu a line goes in, addGameMenuLine's `section` (C: GAME_MENU_SECTION_*).
---A name instead makes a section for the mod.
---@class coe5.gameMenuSection
coe5.gameMenuSection = {
  saving = 0,    -- the end of Saving & Exiting (the default)
  settings = 1,  -- the end of Settings, after the game's lines
}

---The map editor's ids, as MAP_EDITOR_WAIT's next returns them (C: MAP_EDITOR_*).
---@class coe5.mapEditor
coe5.mapEditor = {
  hideToolbox = 2,  -- "Hide Toolbox" and Tab: the editor shows or hides its toolbox
}

---Map screen actions (C: MAP_ACTION_*).
---@class coe5.mapAction
coe5.mapAction = {
  specialPower = 5,  -- Use Special Power: a caster's ritual screen
  gameMenu = 6,      -- the Game Menu (F10)
  squareInfo = 96,   -- the square info window for the square under the pointer
  endTurn = 7,       -- End Turn (y): MAP_SCREEN_WAIT returning it ends the human turn
}

---The answers to the boxes that stop a turn, as their waits return them (C: BOX_*).
---@class coe5.box
coe5.box = {
  messagesOk = 1,     -- the Messages screen's Ok (MESSAGES_WAIT)
  warnOk = 1,         -- the warning box's Ok (WARN_WAIT): an elimination's box and the like
  firstButton = 11,   -- the message box's first button (MESSAGE_BOX_WAIT): the End Turn Warning's Yes
  secondButton = 12,  -- its second button
  cancel = 2,         -- its Cancel, or Esc
}

---Key values: the game's key codes, the same on Windows and Mac (C: KEY_*). A letter, digit, or
---punctuation key is its character's byte, string.byte("y"); these are the other keys. Add ctrl,
---alt, or ctrlShift (with a lowercase letter) for a combination: coe5.key.ctrl + string.byte("i")
---is ctrl+i. The map screen already binds F1 to F6 and F9 to F12; F7 and F8 are free.
---@class coe5.key
coe5.key = {
  none = 0,         -- no key
  backspace = 8,
  tab = 9,
  enter = 13,
  esc = 27,
  space = 32,
  delete = 127,
  f1 = 328,
  f2 = 329,
  f3 = 330,
  f4 = 331,
  f5 = 332,
  f6 = 333,
  f7 = 334,
  f8 = 335,
  f9 = 336,
  f10 = 337,
  f11 = 338,
  f12 = 339,
  pageDown = 340,
  pageUp = 341,
  endKey = 342,     -- End
  home = 343,
  up = 344,
  down = 345,
  left = 346,
  right = 347,
  ctrl = 100000,    -- added to a key: ctrl+key
  alt = 200000,     -- added to a key: alt+key
  ctrlShift = 300000,  -- added to a lowercase letter: ctrl+shift+letter
}

---coe5.netRole's answer (C: NET_ROLE_*).
---@class coe5.net
coe5.net = {
  single = 0,  -- single player and hotseat, and any menu before a game
  host = 1,    -- runs the game server: events, AI, battles, months
  client = 2,  -- connected to a game server: plays one player's turn
}

---coe5.sendMessage's player and result (C: MESSAGE_*).
---@class coe5.message
coe5.message = {
  everyHuman = -2,  -- every human player, as a .c5m event's `message -2`
  notAdded = -1,    -- the result when no message was added
}

---A player slot's controller, the playerController value (C: CONTROLLER_*).
---@class coe5.controller
coe5.controller = {
  removed = -1,     -- an empty slot
  human = 0,
  ai = 1,
  independent = 2,  -- the independents, slots 24-30, once the world is built
}

---A class ability id, classAbility's second index (C: CLASS_ABILITY_ID_*). The game's random class
---roll reads the first four.
---@class coe5.classAbility
coe5.classAbility = {
  aiClass = 203,          -- aiclass: who may play the class (coe5.aiClassValue)
  randomSkip = 204,       -- a percent: the random roll skips the class that often
  aiLevelMin = 210,       -- an AI below this difficulty level never rolls the class; 0 no bound
  aiLevelMax = 211,       -- an AI above this difficulty level never rolls the class; 0 no bound
  standardTroops = 222,   -- stdtroops: 1 when the class also recruits the standard troops
}

---An aiclass value, what classAbility gives for coe5.classAbility.aiClass (C: AI_CLASS_*): who may play
---the class.
---@class coe5.aiClassValue
coe5.aiClassValue = {
  humanOnly = -1,   -- an AI's random roll never gets the class
  any = 0,
  aiOnly = 1,       -- only an AI's random roll gets it, and Choose Class doesn't list it
  neither = 2,      -- no player gets it, by roll or by Choose Class
}

---A corpse kind, squareCorpses' third index (C: CORPSE_KIND_*), in the game's order. The humanoid kinds
---(Raise Dead's) are small, human, giant, vanara and bandar; the animal kinds (Raise Animals') smallAnimal,
---largeAnimal and markata.
---@class coe5.corpseKind
coe5.corpseKind = {
  small = 0,        -- a humanoid with small (Goblins, Hoburgs, Bakemono)
  human = 1,        -- any other humanoid of battle size 1
  giant = 2,        -- a humanoid of battle size 2 or 3
  smallAnimal = 3,  -- an animal with under 20 hp
  largeAnimal = 4,  -- an animal with 20 hp or more
  markata = 5,      -- ape 1
  vanara = 6,       -- ape 2
  bandar = 7,       -- ape 3
}

---GAME_OVER_WAIT's mode (C: GAME_OVER_MODE_*).
---@class coe5.gameOverMode
coe5.gameOverMode = {
  plain = 4,  -- nothing drawn behind the screen: plain black
  fog = 9,    -- what the game passes: fog and falling particles
}

---A new game's setup: PLAY_GAME_NEW_WORLD's mode, and the setup screens' results and ids (C:
---NEW_WORLD_*, MAP_SCREEN_OK, MAP_SETTINGS_*, PARTICIPANTS_OK).
---@class coe5.newGame
coe5.newGame = {
  normal = 0,
  network = 1,
  skipParticipants = 2,  -- any mode but 0 and 1 skips Setup Participants
  mapScreenOk = 1,       -- what NEW_WORLD_MAP_SCREEN returns on Ok (0 is Cancel)
  mapSettingsOk = 9,     -- Random Map Settings' Ok, as MAP_SETTINGS_WAIT returns it
  mapSizeFirst = 100,    -- its first map size button (Small); size n is 100 + n, to 104
  societyFirst = 200,    -- its first society button (Random); society n is 200 + n
  participantsOk = 1,    -- Setup Participants' Ok
}

---The ritual screen: why a ritual can't be cast, and its rows' ids (C: RITUAL_REASON_*, RITUAL_ROW_*),
---for RITUAL_SCREEN_ROW, RITUAL_SCREEN_CAN_CAST, and RITUAL_SCREEN_LOCATION_CHECK.
---@class coe5.ritual
coe5.ritual = {
  reasonMirror = -2,    -- needs a mirror monster
  reasonMaxcast = -3,   -- cast as often as its maxcast allows
  reasonImmortal = -4,
  reasonMastery = -6,
  reasonPlane = -7,
  rowCastable = 1000,   -- a castable ritual's row id is its slot + 1000
  rowGreyed = 12288,    -- a greyed-out one's is its slot + 12288 (0x3000)
  rowMenu = 900,        -- a ritualmenu line's is 900 + its place among the screen's lines, from 0 (at most 32)
}

---The Voice of El's Ceremonies screen: its rows' ids (C: CEREMONY_ROW_*), for CEREMONY_SCREEN_ROW. A row's ceremony is its
---id less rowUsable or rowGreyed: the number CEREMONY_PERFORM gets, which ceremonyName names.
---@class coe5.ceremony
coe5.ceremony = {
  rowUsable = 10,       -- a ceremony the priest may perform: its row id is the ceremony + 10, the game's id
  rowGreyed = 12288,    -- a greyed-out one's is the ceremony + 12288 (0x3000), CoE5SE's
}

---A battle target check's answers (C: TARGET_USEFUL, TARGET_NOT_AFFECTED), for BATTLE_TARGET_CHECK.
---@class coe5.target
coe5.target = {
  useful = 1,         -- the target is worth the spell or mind weapon
  notAffected = -3,   -- the weapon wouldn't affect it: casters pass it over
}

---A banish hit's answers, for UNIT_BANISHABLE, and the ability whose total names a unit's plane for a banish
---(C: BANISH_NO_EFFECT, BANISH_AWAY, BANISH_DAMAGE, BANISH_PLANE_ABILITY).
---@class coe5.banish
coe5.banish = {
  noEffect = 0,       -- the hit does nothing
  banished = 1,       -- the unit leaves the field, and after the battle stands on another plane
  damage = 2,         -- the unit takes the hit's damage instead
  planeAbility = 94,  -- coe5.get("unitAbility", unit, coe5.banish.planeAbility): the plane its monster or items name, 0 for none
}

---How a unit heals between turns, for UNIT_HEAL_RATE (C: HEAL_RATE_NEVER, HEAL_RATE_USUAL, HEAL_RATE_FAST).
---@class coe5.healRate
coe5.healRate = {
  never = 0,          -- heals nothing, as Never Heals (noheal)
  usual = 1,          -- a tenth of its hit points, half with an army heal spell; nothing while diseased
  fast = 2,           -- half its hit points, as Fast Heal (fastheal); a quarter and 3 while diseased
}

---The game's file browser (Load Game's list) and the square info window (C: FILE_BROWSER_*,
---SQUARE_INFO_OK), for FILE_BROWSER_ROW_DATE, FILE_BROWSER_WAIT, and SQUARE_INFO_WAIT.
---@class coe5.screen
coe5.screen = {
  fileBrowserRowFirst = 10000,  -- row n's id is 10000 + n
  fileBrowserDelete = 7,        -- what the browser's Delete key returns
  squareInfoOk = 1,             -- the square info window's Ok
}

---An AI commander's move (C: AI_MOVE_*, AI_NO_SQUARE, AI_EXPLORE_BELOW, AI_FAR_EXPLORE_BELOW), for
---the AI sites (AI_RALLY, AI_FRIENDLY_MOVE, AI_CONQUER, AI_EXPLORE, AI_FAR_EXPLORE) and
---commanderAiMoveKind.
---@class coe5.aiMove
coe5.aiMove = {
  none = 0,              -- no move planned: the commander rests
  target = 1,            -- a square from the rally, friendly move, or conquest step
  explore = 2,           -- an exploration, which the game goes on with next turn while the square stays unseen
  noSquare = -1,         -- x and y of no square
  exploreBelow = 50,     -- AI_EXPLORE runs only while the best score so far is below this
  farExploreBelow = 10,  -- the game's far exploration runs only while the best score is below this
}

-- BEGIN key actions (generated from key_actions.h: change that list, not this block)
---The key actions (C: KEY_ACTION_* in key_actions.h): one for each key of each line of the game's
---Keyboard Shortcuts list, for coe5.rebindKey, coe5.addActionKey, and coe5.actionKey. The
---comment gives the screen, the game's key, and the line.
---@class coe5.keyAction
coe5.keyAction = {
  mapShortcutsList = 1,          -- Map, ?: List keyboard shortcuts
  mapRecruit = 2,                -- Map, r: Recruit
  mapEndTurn = 3,                -- Map, y: End turn
  mapMessages = 4,               -- Map, m: View messages
  mapNextCommander = 5,          -- Map, n: Next commander
  mapPreviousCommander = 6,      -- Map, j: Previous commander
  mapCommanderDone = 7,          -- Map, Space: Commander is done for this turn
  mapTransfer = 8,               -- Map, t: Transfer troops
  mapSpecialPower = 9,           -- Map, p: Use commander's special power
  mapUnitOverview = 10,          -- Map, F1: Unit overview
  mapPlayerOverview = 11,        -- Map, F2: Player overview
  mapClassDescription = 12,      -- Map, F3: Class description
  mapScoreGraphs = 13,           -- Map, F5: View score graphs
  mapActiveMods = 14,            -- Map, F6: View active mods
  mapGameMenu = 15,              -- Map, F10: Settings, save, quit (the Game Menu)
  mapSquareInfo = 16,            -- Map, i: Square terrain info
  mapCommanderStats = 17,        -- Map, v: View commander stats
  mapTrade = 18,                 -- Map, l: Trade administration
  mapSentry = 19,                -- Map, s: Sentry commander
  mapLocationPower = 20,         -- Map, o: Use location's special power
  mapBurnForestScreen = 21,      -- Map, b: Burn forest screen
  mapBurnForestNow = 22,         -- Map, B: Burn forest immediately
  mapHuntSlaves = 23,            -- Map, h: Hunt for slaves
  mapPlaneShift = 24,            -- Map, u: Plane shift
  mapContinue = 25,              -- Map, ,: Continue moving
  mapContinueAll = 26,           -- Map, .: Continue moving for all commanders
  mapSquareDetails = 27,         -- Map, ctrl+i: Detailed square info
  mapAstrology = 28,             -- Map, ctrl+a: Astrology reading
  mapRepeatOrders = 29,          -- Map, ctrl+r: Execute all repeat orders now
  mapScry = 30,                  -- Map, alt+s: Scry
  mapCenter = 31,                -- Map, Home: Center map on starting citadel
  mapHideIcons = 32,             -- Map, Tab: Hide map icons (hold)
  mapZoomIn = 33,                -- Map, PgUp: Zoom in
  mapZoomOut = 34,               -- Map, PgDn: Zoom out
  mapScrollUp = 35,              -- Map, Up: Scroll map up
  mapScrollDown = 36,            -- Map, Down: Scroll map down
  mapScrollLeft = 37,            -- Map, Left: Scroll map left
  mapScrollRight = 38,           -- Map, Right: Scroll map right
  mapMoveUpLeft = 39,            -- Map, q: Move commander up-left
  mapMoveUp = 40,                -- Map, w: Move commander up
  mapMoveUpRight = 41,           -- Map, e: Move commander up-right
  mapMoveLeft = 42,              -- Map, a: Move commander left
  mapMoveRight = 43,             -- Map, d: Move commander right
  mapMoveDownLeft = 44,          -- Map, z: Move commander down-left
  mapMoveDown = 45,              -- Map, x: Move commander down
  mapMoveDownRight = 46,         -- Map, c: Move commander down-right
  mapLookUpLeft = 47,            -- Map, Q: Info on the square up-left
  mapLookUp = 48,                -- Map, W: Info on the square up
  mapLookUpRight = 49,           -- Map, E: Info on the square up-right
  mapLookLeft = 50,              -- Map, A: Info on the square to the left
  mapLookRight = 51,             -- Map, D: Info on the square to the right
  mapLookDownLeft = 52,          -- Map, Z: Info on the square down-left
  mapLookDown = 53,              -- Map, X: Info on the square down
  mapLookDownRight = 54,         -- Map, C: Info on the square down-right
  mapLookHere = 55,              -- Map, S: Info on the commander's square
  armyShortcutsList = 56,        -- Transfer troops, ?: List keyboard shortcuts
  armySelectAll = 57,            -- Transfer troops, a: Select/deselect all units
  armySelectAfflicted = 58,      -- Transfer troops, b: Select units with battle afflictions
  armyDeselectSlowAp = 59,       -- Transfer troops, c: Deselect units with fewer AP left
  armySelectFast = 60,           -- Transfer troops, f: Select all fast units
  armySelectHorror = 61,         -- Transfer troops, h: Select horror marked units
  armyShowItems = 62,            -- Transfer troops, i: Highlight units with magic items
  armyDeselectLandlubbers = 63,  -- Transfer troops, l: Deselect all landlubbers
  armySelectType = 64,           -- Transfer troops, m: Select the type under the pointer
  armySelectSlow = 65,           -- Transfer troops, s: Select all slow units
  armySelectWounded = 66,        -- Transfer troops, w: Select all wounded units
  armySelectTen = 67,            -- Transfer troops, +: Select 10 of the type under the pointer
  armyDeselectTen = 68,          -- Transfer troops, -: Deselect 10 of the type under the pointer
  armyScrollUp = 69,             -- Transfer troops, Up: Scroll battlefield up
  armyScrollDown = 70,           -- Transfer troops, Down: Scroll battlefield down
  armyScrollLeft = 71,           -- Transfer troops, Left: Scroll battlefield left
  armyScrollRight = 72,          -- Transfer troops, Right: Scroll battlefield right
  armySelectMenu = 73,           -- Transfer troops, F2: Select units menu
  armyExit = 74,                 -- Transfer troops, Esc: Exit screen
  battleShortcutsList = 75,      -- Battle, ?: List keyboard shortcuts
  battleScrollLeft = 76,         -- Battle, Left: Scroll battlefield left
  battleScrollRight = 77,        -- Battle, Right: Scroll battlefield right
  battleMessagesUp = 78,         -- Battle, -: Scroll battle messages back
  battleMessagesDown = 79,       -- Battle, +: Scroll battle messages forward
  battleMessagesPageUp = 80,     -- Battle, PgUp: Scroll battle messages a page back
  battleMessagesPageDown = 81,   -- Battle, PgDn: Scroll battle messages a page forward
  battlePause = 82,              -- Battle, Space: Pause battle
  battleTerrainOnly = 83,        -- Battle, Tab: View terrain only (hold)
  battleSlowMotion = 84,         -- Battle, z: Slow motion
  battleNormalSpeed = 85,        -- Battle, n: Normal speed
  battleFastForward = 86,        -- Battle, f: Fast forward
  battleSpeedUp = 87,            -- Battle, s: Speed up one notch
  battleSkip = 88,               -- Battle, q: Skip entire battle
  battleLog = 89,                -- Battle, l: Hide/show battle log
  battleGrid = 90,               -- Battle, g: Hide/show grid
  battleTerrain = 91,            -- Battle, t: Hide/show terrain
  battleUnitList = 92,           -- Battle, F1: View unit list
  mapEditorShortcutsList = 93,   -- Map editor, ?: List keyboard shortcuts
  mapEditorMenu = 94,            -- Map editor, Tab: Hide/show menu
  mapEditorPencil1 = 95,         -- Map editor, 1: Pencil size 1
  mapEditorPencil2 = 96,         -- Map editor, 2: Pencil size 2
  mapEditorPencil3 = 97,         -- Map editor, 3: Pencil size 3
  mapEditorClearSquare = 98,     -- Map editor, c: Clear special attributes for square
  mapEditorNameSquare = 99,      -- Map editor, n: Name square
  mapEditorGateway = 100,        -- Map editor, g: Create gateway
  mapEditorOverlay = 101,        -- Map editor, o: Toggle overlay on/off
  mapEditorQuit = 102,           -- Map editor, q: Quit map editor
  mapEditorRandomWorld = 103,    -- Map editor, r: Recreate random world
  mapEditorTerrainList = 104,    -- Map editor, t: Select terrain from a list
  mapEditorUnflood = 105,        -- Map editor, u: Unflood all squares
  mapEditorFixedTemp = 106,      -- Map editor, E: Set fixed temp on entire map
  mapEditorRemoveFancy = 107,    -- Map editor, F: Remove all fancy terrains
  mapEditorLoadOverlay = 108,    -- Map editor, O: Load overlay image
  mapEditorFixTemps = 109,       -- Map editor, T: Fix temperatures
  mapEditorCircularTemp = 110,   -- Map editor, ctrl+u: Circular temperature on map
  mapEditorLoad = 111,           -- Map editor, ctrl+l: Load map
  mapEditorDrawTemp = 112,       -- Map editor, ctrl+e: Enter temperature to draw with
  mapEditorResize = 113,         -- Map editor, ctrl+r: Resize map (scale)
  mapEditorSave = 114,           -- Map editor, ctrl+s: Save map
  mapEditorRemoveCold = 115,     -- Map editor, ctrl+t: Remove all cold temperatures
  mapEditorReduceSquare = 116,   -- Map editor, alt+r: Reduce square
  mapEditorDestroySquare = 117,  -- Map editor, alt+d: Destroy square
  mapEditorMoveUp = 118,         -- Map editor, U: Move entire map up
  mapEditorMoveLeft = 119,       -- Map editor, H: Move entire map left
  mapEditorMoveDown = 120,       -- Map editor, J: Move entire map down
  mapEditorMoveRight = 121,      -- Map editor, K: Move entire map right
  mapEditorZoomIn = 122,         -- Map editor, PgUp: Zoom in
  mapEditorZoomOut = 123,        -- Map editor, PgDn: Zoom out
  mapEditorScrollUp = 124,       -- Map editor, Up: Scroll map up
  mapEditorScrollDown = 125,     -- Map editor, Down: Scroll map down
  mapEditorScrollLeft = 126,     -- Map editor, Left: Scroll map left
  mapEditorScrollRight = 127,    -- Map editor, Right: Scroll map right
  mapEditorPlaneAbove = 128,     -- Map editor, <: Plane above
  mapEditorPlaneBelow = 129,     -- Map editor, >: Plane below
  mapEditorRefresh = 130,        -- Map editor, F5: Refresh map
  battleEditorShortcutsList = 131, -- Battle map editor, ?: List keyboard shortcuts
  battleEditorGroundMode = 132,  -- Battle map editor, Tab: Switch to/from ground edit mode
  battleEditorCenterLine = 133,  -- Battle map editor, ctrl+w: Show center line
  battleEditorReplacer = 134,    -- Battle map editor, r: Replacer
  battleEditorDraw = 135,        -- Battle map editor, q: Draw normally
  battleEditorLoad = 136,        -- Battle map editor, ctrl+l: Load
  battleEditorSave = 137,        -- Battle map editor, ctrl+s: Save
  battleEditorMonster = 138,     -- Battle map editor, m: Select monster
  battleEditorClear = 139,       -- Battle map editor, C: Clear all
  battleEditorScrollUp = 140,    -- Battle map editor, u: Scroll terrain up
  battleEditorScrollLeft = 141,  -- Battle map editor, h: Scroll terrain left
  battleEditorScrollDown = 142,  -- Battle map editor, j: Scroll terrain down
  battleEditorScrollRight = 143, -- Battle map editor, k: Scroll terrain right
  battleEditorNoDeploy = 144,    -- Battle map editor, alt+b: No deploy mark
  battleEditorCatapult = 145,    -- Battle map editor, alt+c: Catapult mark
  battleEditorEffigy = 146,      -- Battle map editor, alt+e: Effigy mark
  battleEditorMage = 147,        -- Battle map editor, alt+m: Mage mark
  battleEditorMelee = 148,       -- Battle map editor, alt+s: Melee soldier mark
  battleEditorDemonHand = 149,   -- Battle map editor, alt+d: Demon hand mark
  battleEditorDemonHead = 150,   -- Battle map editor, alt+h: Demon head mark
  battleEditorFirewall = 151,    -- Battle map editor, alt+v: Firewall mark
  battleEditorNoTrees = 152,     -- Battle map editor, alt+t: No random trees/bushes here
  battleEditorClearNoTrees = 153, -- Battle map editor, alt+y: Clear no random trees/bushes mark
  recruitClearHere = 154,        -- Recruitment, c: Clear auto-recruitment at this location
  recruitClearAll = 155,         -- Recruitment, v: Clear auto-recruitment everywhere
  recruitLocation1 = 156,        -- Recruitment, 1: Recruitment location 1
  recruitLocation2 = 157,        -- Recruitment, 2: Recruitment location 2
  recruitLocation3 = 158,        -- Recruitment, 3: Recruitment location 3
  recruitLocation4 = 159,        -- Recruitment, 4: Recruitment location 4
  recruitLocation5 = 160,        -- Recruitment, 5: Recruitment location 5
  recruitLocation6 = 161,        -- Recruitment, 6: Recruitment location 6
  recruitLocation7 = 162,        -- Recruitment, 7: Recruitment location 7
  recruitLocation8 = 163,        -- Recruitment, 8: Recruitment location 8
  recruitLocation9 = 164,        -- Recruitment, 9: Recruitment location 9
  recruitPrevious = 165,         -- Recruitment, Left: Previous recruitment location
  recruitNext = 166,             -- Recruitment, Right: Next recruitment location
  recruitShortcutsList = 167,    -- Recruitment, ?: List keyboard shortcuts
  recruitExit = 168,             -- Recruitment, Esc: Exit recruitment
  recruitExitEnter = 169,        -- Recruitment, Enter: Exit recruitment, second key
  participantsAdd = 170,         -- Setup participants, n: Add new player
  participantsColors = 171,      -- Setup participants, r: Randomize player colors by team
  participantsAllHuman = 172,    -- Setup participants, H: Set all players to human controlled
  participantsAllAi = 173,       -- Setup participants, A: Set all players to AI controlled
  participantsAiUp = 174,        -- Setup participants, +: Increase all AI levels by 1
  participantsAiDown = 175,      -- Setup participants, -: Decrease all AI levels by 1
  participantsRemove = 176,      -- Setup participants, Backspace: Remove last player
  participantsOk = 177,          -- Setup participants, Enter: Ok
  participantsCancel = 178,      -- Setup participants, Esc: Cancel
  viewArmyShowItems = 179,       -- Army on the battlefield, i: Highlight units with magic items
  viewArmyShortcutsList = 180,   -- Army on the battlefield, ?: List keyboard shortcuts
  viewArmyScrollLeft = 181,      -- Army on the battlefield, Left: Scroll battlefield left
  viewArmyScrollRight = 182,     -- Army on the battlefield, Right: Scroll battlefield right
  viewArmyTerrainOnly = 183,     -- Army on the battlefield, Tab: View terrain only (hold)
  viewArmyExit = 184,            -- Army on the battlefield, Enter: Exit screen
  viewArmyExitEsc = 185,         -- Army on the battlefield, Esc: Exit screen, second key
  unitListCommanders = 186,      -- Battle unit list, c: Only show commanders
  unitListShowAll = 187,         -- Battle unit list, A: Show all (terrain units too)
  unitListShortcutsList = 188,   -- Battle unit list, ?: List keyboard shortcuts
  unitListExit = 189,            -- Battle unit list, Enter: Exit screen
  unitListExitEsc = 190,         -- Battle unit list, Esc: Exit screen, second key
  fileBrowserDelete = 191,       -- Load game, Delete: Delete save file under pointer
  fileBrowserDeleteCtrl = 192,   -- Load game, ctrl+d: Delete save file, second key
  fileBrowserSortTime = 193,     -- Load game, ctrl+t: Sort by time
  fileBrowserSortName = 194,     -- Load game, ctrl+n: Sort by name
  fileBrowserShortcutsList = 195, -- Load game, ?: List keyboard shortcuts
  fileBrowserCancel = 196,       -- Load game, Esc: Cancel
}
-- END key actions

-- ---- Archived site and value names (kept for older mods) ---------------------------------------
-- A renamed call, site or game value keeps its earlier name as an archived alias, so a released mod that uses it still
-- works. Each alias is the same function, site or value as its name now: the same arguments, the same answers, and the same
-- rules in a LAN game. A refused call is logged under its name now (lua_refused call=coe5.<name now>). New scripts use the
-- name now.

---Archived site names, kept for older mods. coe5.wrap and coe5.observe take each as its name now.
---@alias coe5.ArchivedSite
---| "UNIT_STRENGTH"          # now UNIT_BATTLE_STRENGTH
---| "SQUARE_STRENGTH"        # now SQUARE_BATTLE_STRENGTH
---| "COMMANDER_STRENGTH"     # now COMMANDER_BATTLE_STRENGTH
---| "KILL_UNIT_999"          # now KILL_UNIT
---| "DAMAGE_UNIT_PLAIN"      # now SUBTRACT_UNIT_HP
---| "BOLSTER_BLESSING"       # now PLAYER_RANDOM_BLESSING
---| "SPECIAL_SITE_POPULATE"  # now ELEMENTAL_ANOMALY_SPAWN
---| "KILL_TERRAIN_UNITS"     # now BATTLE_REMOVE_TERRAIN_UNITS
---| "KILL_SUNKEN_SHIPS"      # now BATTLE_REMOVE_SUNKEN_SHIPS
---| "CHANGE_LOYALTY"         # now UNITS_TURN_TO_HADES

---Archived game value names, kept for older mods. coe5.get and coe5.set take each as its name now.
---@alias coe5.ArchivedVariable
---| "commanderStrength"         # now commanderBattleStrength
---| "squareStrength"            # now squareBattleStrength
---| "squareRememberedStrength"  # now squareRememberedBattleStrength
---| "unitStrength"              # now unitBattleStrength
---| "enchantKind"               # now enchantType
---| "slotController"            # now playerController
---| "slotClass"                 # now playerClass
---| "slotAi"                    # now playerAi
---| "slotTeam"                  # now playerTeam
---| "slotName"                  # now playerName
---| "slotColorRed"              # now playerColorRed
---| "slotColorGreen"            # now playerColorGreen
---| "slotColorBlue"             # now playerColorBlue
---| "slotResource"              # now playerResource
---| "slotAiIncome"              # now playerAiIncome
---| "slotAiTrade"               # now playerAiTrade
---| "slotIncome"                # now playerIncome
---| "slotTradeType"             # now playerTradeType
---| "slotTradeMode"             # now playerTradeMode
---| "slotLeader"                # now playerLeader
---| "slotRitualCasts"           # now playerRitualCasts
---| "slotHomeX"                 # now playerHomeX
---| "slotHomeY"                 # now playerHomeY
---| "slotBolsterDevotions"      # now playerBlessingRolls
---| "slotDivineFavor"           # now playerDivineFavor
---| "slotCitadels"              # now playerCitadels
---| "slotAbility"               # now playerAbility
---| "slotAbilitySum"            # now playerAbilitySum
---| "slotLockClass"             # now playerFixedClass
---| "slotLockController"        # now playerFixedController
---| "slotLockAi"                # now playerFixedAi
---| "slotLockTeam"              # now playerFixedTeam
---| "slotMostCitadels"          # now playerAiMostCitadels
---| "slotBlessingId"            # now playerBlessingListId
---| "slotBlessingValue"         # now playerBlessingListValue
---| "commanderCostAvailable"    # now commanderCostOnHand

-- The archived calls are at the end of coe5_calls.lua.
