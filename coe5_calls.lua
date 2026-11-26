---@meta
-- CoE5SE Lua API definitions, the second of two files: every coe5.* call, with the tables they take
-- and return. coe5.lua has how scripts run, the cheat sheet, the game's words and numbers, the sites
-- and the game values. CoE5SE never executes either file. Place both beside your script.

-- ---- Table types ------------------------------------------------------------------------------

---@class coe5.GameMenuLine
---@field label string cut to 63 characters (logged as api_cut)
---@field tooltip? string cut to 255 characters
---@field visible? fun(): boolean Shows the line while it returns true. In Lua, 0 counts as true: return a boolean.
---@field onClick fun(): integer? coe5.gameMenu.stay (or nothing, or false) keeps the menu open, coe5.gameMenu.close (or true) closes it, coe5.gameMenu.resign resigns. To end the game and start a new one, call coe5.restartGame() here instead of resigning.
---@field section? integer|string coe5.gameMenuSection.saving (the default), coe5.gameMenuSection.settings, or a section name, cut to 63 characters

---A line of the map editor's toolbox, for coe5.addMapEditorLine.
---@class coe5.MapEditorLine
---@field label string cut to 63 characters (logged as api_cut)
---@field tooltip? string cut to 255 characters
---@field visible? fun(): boolean Shows the line while it returns true, checked every frame. In Lua, 0 counts as true: return a boolean.
---@field onClick fun() runs when the line, or a key bound to its id, is clicked

---A line of the commander panel, for coe5.addCommanderLine.
---@class coe5.CommanderLine
---@field label string cut to 63 characters (logged as api_cut)
---@field tooltip? string the hover text, cut to 255 characters
---@field icon? integer a small icon (the panel's icons: 0x75 Sentry to 0x85 Continue Moving), drawn left of the label as the game's lines have theirs; left out, none
---@field color? number[] the label's colour, {r, g, b} or {r, g, b, alpha}, each 0 to 1 (alpha 1 when left out); left out, the game's text colour (white). Any other count of parts, or a part outside 0 to 1, refuses the line (bad_color)
---@field visible? fun(): boolean Shows the line while it returns true, checked every frame with the commander selected (coe5.get("selectedCommander")). In Lua, 0 counts as true: return a boolean.
---@field onClick fun() runs when the line is clicked, with that commander still selected

---A mod's plane, for coe5.addPlane.
---@class coe5.Plane
---@field name string the plane's name, 1 to 31 characters, unlike every other plane's (a game plane's full or short name included)
---@field w? integer its width in squares, 1 to 999; leave w and h out with like "Sky", "Agartha", "Hades" or "Inferno" to take that plane's size in each world (Elysium's, the Inferno 4 more each way)
---@field h? integer its height in squares, 1 to 159
---@field icon? string its plane button's picture, a file in your mod's folder: "./plane.tga" or "./art/plane.png" (64 x 64 like the game's plane icons); without it, the icons of the plane it is `like`, else a coloured square
---@field fill? integer the terrain on every square before anything else (terrainName takes it); one past terrainLastIndex is laid as 0, logged. Needed without like and generate; 0 (Plain) when left out with them
---@field like? string build it with that game plane's recipe, CoE5SE's copy of the game's random map generator for it (planes/plane_recipes.lua): "Celestial", "Sky", "Agartha", "Inferno", "Hades", "Elemental" or "Void". A step the plane is too small for is skipped and logged (lua_refused reason=plane_too_small with its minimum); nothing is scaled down
---@field populate? string|false the game plane whose independents the game places on it ("Elysium" to "Void"), or false for none; like's plane by default, none without like. The game places independents by terrain number, so another plane's independents appear only on terrains both planes have
---@field swap? table<integer, integer> terrain = the terrain that replaces it, after the independents are placed (so they are placed by the first terrain); up to 64 pairs
---@field generate? fun(plane: coe5.PlaneBuild) runs at each world build after the recipe, with coe5.gen's tools and square writes open on the plane
---@field pieces? coe5.PlanePiece[] set pieces stamped after the recipe and generate: fixed squares first (center, x and y), then nearportal, then random, the largest first; never on another piece (plane_piece_skipped)
---@field zones? coe5.PlaneZone[] areas around the pieces with their swap and monsters
---@field units? coe5.PlaneUnits[] the plane's list: only these monsters are placed on it (outside the zones with a list or populate = false), never populate's

---A set piece of coe5.addPlane: a single-plane map file in your mod's folder, in the game's format.
---@class coe5.PlanePiece
---@field file string "./citadel.coem"; its mapsize is the piece's size, at most the plane's
---@field place? "center"|"centre"|"random"|"nearportal" the middle; count random spots on no portal, other piece or square kept for the gateway (100 draws each); or beside the plane's entrance
---@field count? integer copies for "random", 1 or more (1)
---@field x? integer with y and no place: its top left square, from the plane's top left (0, 0)
---@field y? integer

---A zone of coe5.addPlane: every square within radius of a copy of the piece `around`, by the game's square distance (the larger of the x and y distances). A square in two zones is the first one's.
---@class coe5.PlaneZone
---@field name string 1 to 31 bytes, unlike the plane's other zones
---@field around string a pieces entry's file
---@field radius integer 0 to 999 (0: the piece alone)
---@field swap? table<integer, integer> terrain = the terrain it becomes, in the zone, after the rare special terrain and before the independents; up to 32 pairs
---@field units? coe5.PlaneUnits[] only these monsters are placed in the zone
---@field populate? false no monsters in the zone; without it or units, the plane's

---One monster of a unit list: {name, count, chance}. On each open square of its area (no army, no player's, no apcost 99 wall; water only for a monster with the water movement command, land only without noland) it rolls chance percent for count independents, doubled with Wilder on as the game's dice are.
---@class coe5.PlaneUnits
---@field [1] string the monster's name, as the game names its independents (a name several monsters share places one of them at random)
---@field [2] integer how many with Wilder off, 1 to 1000
---@field [3] integer the chance per square, 1 to 100

---A mod's independent faction, for coe5.addFaction. Each field is the newfaction block word of its name (factionlike for like, factionmonumentevent for monumentEvent), takes what the word takes, and is the preset's when left out.
---@class coe5.Faction
---@field name string 1 to 31 characters, unlike every other faction's and the vanilla factions' names
---@field like? "Independents"|"Special Monsters"|"Horrors"|"Hades"|"Inferno"|"Celestial"|"Empire"|"Kingdom" the vanilla faction whose rules it takes; Independents when left out
---@field color? number[] {r, g, b}, each 0 to 1; the preset's colour when left out
---@field home? string its home plane's name, a game plane's or a mod's (a name no plane has is logged once every script has declared its planes, lua_refused reason=unknown_plane field=home, and is no home)
---@field active? "always"|"never"|integer when it is active, for its own-citadel spawns and its wanderers' gate: always, never, or once that many portals are open on its home (1 to 1000)
---@field populate? boolean whether its home's population is owned by the faction
---@field wander? "wander"|"indwander"|"infwander"|"hadeswander"|"celwander" the wander word its commanders take
---@field wanderGate? boolean
---@field tithe? boolean
---@field worldEvents? boolean
---@field conquest? "independent"|"normal" whether its squares are conquered as the Independents' are
---@field statue? boolean
---@field spawnSite? "owncitadel"|"monument"|"none" where its turn spawns
---@field spawnPlane? string with spawnSite "monument": the plane its monument stands on; Elysium when left out
---@field fuel? string the monster its spawns use up
---@field spawns? coe5.FactionSpawn[] its spawns, in order, replacing the preset's; {} or false for none
---@field monument? string|false its monument monster, false for none
---@field monumentRule? boolean whether its monument counts toward the game's rulership check, as the Monument of Hades does
---@field monumentEvent? coe5.FactionMonumentEvent|false its monument's founding event, false for none
---@field planeShift? coe5.FactionPlaneShift|false the units its monument shifts off its home, false for none
---@field loyalty? string|false the monster whose units on its monument's plane become owned by the faction, false for none
---@field gather? integer|false the ability number that makes its units carriers, 1 to 65535, false for none
---@field gateway? coe5.FactionGateway|false the gateway its carriers restore, false for none
---@field caravan? coe5.FactionCaravan|false its caravan event, false for none
---@field horrors? boolean whether it takes a share of the Horrors' spawns
---@field avoid? coe5.FactionAvoid[]|false factions whose armies the faction's armies step onto only at a chance, replacing the preset's; false or {} for none
---@field units? coe5.FactionUnits[] its units, placed at each world build after the game's population on free squares (nobody's or the Independents', no army, no wall); the squares become owned by the faction
---@field populateTerrain? coe5.FactionTerrain[] terrains whose Independents' squares and units become owned by the faction at each world build, before its units are placed

---One spawn of coe5.Faction: with at least min fuel units (and the terrain), a draw below chance makes group, using use of them.
---@class coe5.FactionSpawn
---@field min integer 0 to 10000
---@field use integer 0 to min
---@field chance integer 1 to 100
---@field group string the group, as sitepop names one
---@field terrain? "magma"|"ice"|"waste" only with spawnSite "owncitadel" (lua_refused reason=terrain_needs_owncitadel otherwise)

---@class coe5.FactionMonumentEvent
---@field turn integer the first month it may run, 0 to 100000
---@field chance integer in 1000 each month, 1 to 1000
---@field plane string the plane the monument is founded on
---@field terrain integer the terrain it is founded on, a number or a terrain group
---@field unfinished string the monster that grows into the monument
---@field news? string the news, with %s for the commander's name; none when left out

---@class coe5.FactionPlaneShift
---@field monster string
---@field percent integer 1 to 100
---@field each integer 1 to 100

---@class coe5.FactionGateway
---@field fuel integer the carriers' units it takes, 1 to 10000
---@field ruin integer the ruined gateway's terrain
---@field gate integer the restored gateway's terrain
---@field exit integer the terrain at the far end
---@field news? string the news, naming no one; none when left out

---@class coe5.FactionCaravan
---@field turn integer the first month it may run, 0 to 100000
---@field chance integer in 350 each month, 1 to 350
---@field plane string
---@field terrain integer a number or a terrain group
---@field group? string the caravan's group; the Merchants' crew when left out
---@field news? string the news, with %s for the commander's name; none when left out

---One monster of coe5.Faction's units: each free square of the terrain on the plane rolls chance percent for count of it, doubled with Wilder on as planeunits' are.
---@class coe5.FactionUnits
---@field monster string
---@field count integer 1 to 1000
---@field chance integer the chance per square, 1 to 100
---@field terrain? integer a terrain number or a terrain group; any terrain when left out
---@field plane? string a game plane's or a mod's plane's name; every plane when left out

---@class coe5.FactionTerrain
---@field terrain integer a terrain number or a terrain group
---@field plane? string every plane when left out

---@class coe5.FactionAvoid
---@field name string a mod's faction or a vanilla faction
---@field percent integer the chance to step onto its armies, 0 to 100

---The plane being built, as a recipe's steps and a generate function get it.
---@class coe5.PlaneBuild
---@field name string
---@field index integer its plane number in this world
---@field x integer its first column in world squares
---@field y integer its first row
---@field w integer
---@field h integer
---@field like? string the recipe's plane
---@field mod string the declaring mod
---@field elysiumW integer Elysium's width, for steps that read Elysium at the same offset
---@field elysiumH integer Elysium's height
---@field platform "windows"|"mac" the build, for a step whose random draws the two builds make in another order

---The map view, from coe5.mapView. UI units: x from the left, y up from the bottom.
---@class coe5.MapView
---@field zoom number UI units per square (8 to 100 in the map editor, 12 to 100 on the map)
---@field cameraX number the world x at the view's centre column, in squares
---@field cameraY number the y at the view's centre row, in squares (the centre shows row cameraY + 1)
---@field originX number the UI x of world square x = 0's left edge; square x's is originX + x * zoom
---@field originY number the UI y of row y = 0's top edge; row y's is originY - y * zoom
---@field screenW integer the UI's width
---@field screenH integer the UI's height
---@field plane integer the shown plane (planeName takes it)
---@field planeX integer the plane's first world x
---@field planeY integer the plane's first y
---@field planeW integer the plane's width in squares
---@field planeH integer the plane's height in squares
---@field firstX integer the first world x of the plane on screen
---@field lastX integer the last
---@field firstY integer the first y of the plane on screen
---@field lastY integer the last

---A recruitment offer, from coe5.shopOffers (a player's offers this turn) or coe5.classRecruits (a
---class's list). The fields marked shop or class come only from that one.
---@class coe5.Offer
---@field offer integer shop: the offer's number, for coe5.recruitSquares and coe5.aiBuyOffer
---@field monster? integer the monster (monsterName takes it); class: nil for a name no monster has
---@field name string the monster's name
---@field count integer units it buys; class: -1 for a commander (addcomrec)
---@field commander boolean the unit comes as a commander
---@field costs table<integer, integer> cost by type 0-18 (incomeName's order: 0 gold, 1 iron, ...)
---@field gold integer costs[0]; shop: rolled with the die when the offer came
---@field iron integer costs[1]
---@field goldDie? integer class: the open-ended die added to gold each time the offer comes
---@field chance? integer class: percent a month (before temples, libraries, the AI level, and the flags below)
---@field flags integer the offer's flag bits: 1 chance not 100, 4 mercenary or commander, 0x8 nogathererrec, 0x20 templerec, 0x40 libraryrec, 0x80 and 0x100 priestrec 2 and 3, 0x200 the AI's first turns, 0x400 to 0x10000 a terrain group (0x1000 to 0x8000 the gem locations), 0x20000 and towers (towerrec), 0x40000 atavirec, 0x80000 humanrec
---@field oncePerOffer? boolean shop: bought once, then gone
---@field extra? boolean shop: a buy leaves the square's recruitment for the turn unused (mercenaries, commanders)
---@field aiNever? boolean shop: the game's AI never buys it, and coe5.aiBuyOffer refuses it
---@field terrain integer the terrain it is recruited on (recterr), -1 or 0 for any; class: a record with a terrain group bit in flags gets its place from that bit, not from this -1
---@field limiter? string shop: its "=" limiter ("=Soulless": it upgrades those units)
---@field limiters? string[] class: its reclimiter lines ("-Baron", "+Goblin Chieftain", "=Soulless")
---@field moves integer the monster's action points a turn: 4 fast, 2 slow, else 3
---@field spellLevel integer the highest level of the monster's spell attacks, 0 for none (a caster's measure)

-- ---- Core API ---------------------------------------------------------------------------------


---Writes a lua_log line to coe5mod.log, while the player has CoE5SE's "Enable copious logs" on
---(Mod Settings, CoE5SE's page; off by default, so ask players to turn it on when you need
---their log). print works the same way. CoE5SE's lines about your script (lua_error,
---lua_refused, lua_script_loaded) are always written. The text stays on that one line: a line break
---or other control character in it is written escaped (\n, \r, \t, \xNN).
---@param text string|number
function coe5.log(text) end

---Adds a main menu button above Quit, or after the game's button `afterId`: coe5.mainMenu.startNewGame,
---.loadGame, .network, .preferences, .mods, or .credits. Buttons several mods put after the same
---game button follow it in the order the mods added them. When the column has no room for every
---mod's button, its last row is More, which lists the rest.
---@param label string cut to 47 characters (logged as api_cut)
---@param onClick fun()
---@param afterId? integer
---@return boolean added false when `label` is empty (logged as api_refused) or memory ran out (lua_refused)
function coe5.addMainMenuButton(label, onClick, afterId) end

---Adds a line to the in-game Game Menu: at the end of its Saving & Exiting section, or with
---`section = coe5.gameMenuSection.settings`, at the end of its Settings section. A name, such as
---`section = "Cheats"`, puts the line in a section of that name after Saving & Exiting. Every mod
---that gives the same name (in any case) shares that section, headed with the first line's
---spelling. Sections come in the order their names were first added, and a section shows only
---while one of its lines does.
---@param line coe5.GameMenuLine
---@return boolean added false when `label` is empty, `section` is an empty name, or `section` is a number that is neither (logged as api_refused)
function coe5.addGameMenuLine(line) end

---Reads a game value. A per-slot value needs its slot (0-31, a mod's faction 32-47) or a faction's name, and a table value its index;
---playerResource needs the slot, then the resource (0-13), and a value with more indexes takes each
---in the order its note gives (squareSeen: the slot, x, then y).
---@param name coe5.Variable|coe5.ArchivedVariable
---@param slot? integer|string slot or index; a faction's name for a slot (coe5.factionPlayer's names)
---@param index? integer|string playerResource's resource, or a second index (unitVisibleTo's player, or a faction's name); for unitAbility and monsterAbility an ability's mod command, for monster, weapon, item, class, terrain and ritual a word of that .c5m block
---@param index3? integer a third index (squareSeen's y)
---@return any value an integer, boolean, or string, as the name's note says; nil for a frame value outside its site's wrap, an index past a list's end, or a name the game's table leaves empty
function coe5.get(name, slot, index, index3) end

---Changes a game value. A per-slot value needs its slot (0-31, a mod's faction 32-47) or a faction's name, and a table value its index;
---playerResource needs the slot, then the resource (0-13). A
---read-only value is an error. A boolean value takes only true or false: anything else, a number or
---nil, changes nothing and is refused (lua_refused reason=needs_boolean), because in Lua 0 is true.
---An integer the game itself never holds there (past a table the game indexes by it, or outside what
---the game stores, as the name's note says, or too big for the game's field) changes nothing and is
---refused (reason=out_of_range). A slot's colour takes what the name's note says; bannerFade takes any
---number, and one too big for a float is stored as an infinity. A square's terrain, portal, ground,
---temperature, and name are written only while the world is built (squareName's note says when);
---elsewhere the write is refused (reason=not_world_building, or outside_plane for a square outside
---the area being built).
---A content record (monster, weapon, item, class, terrain, ritual, and their named values) takes a write
---of any mod's record or the game's. The game reading the mods again (a load, leaving the Mods screen)
---puts the records back as the mods make them; then MODS_READ runs, and a save's writes made after it
---go back on. A save from other mods keeps none of its writes (content_writes_dropped reason=mods_differ).
---In a LAN game the host's writes reach every client with each turn's save.
---In a LAN game a content script on a client writes only what reaches the host: its player's resources,
---AI level and trade type, and its units' action points, in its turn. Another value (an event
---variable, a game setting, a square's corpses) the host never gets, and it is refused
---(reason=client_write_lost), a content record's included but in MODS_READ and the script's loading, which every
---machine runs alike; another player's slot or unit is refused (not_own_player), and so is a
---write while the client waits for its turn (client_not_acting). Send the host what it should do
---through coe5.playerData instead. How the machine draws (hoveredId, bannerFade, fullscreen, ...)
---stays free.
---@param name coe5.Variable|coe5.ArchivedVariable
---@param value integer|string|boolean
---@overload fun(name: coe5.Variable|coe5.ArchivedVariable, slot: integer|string, value: integer|string|boolean)
---@overload fun(name: coe5.Variable|coe5.ArchivedVariable, slot: integer|string, index: integer, value: integer|string)
---@overload fun(name: coe5.Variable|coe5.ArchivedVariable, record: integer, word: string, value: integer|string|boolean)
function coe5.set(name, value) end

---Runs `fn` around a game call. `fn` gets `next` first, then the call's arguments; call `next` with
---them to go on to the game, and return what the call should return. Among the scripts' wraps, a
---higher `priority` runs first, outside the others, whatever order the mods load in; without one it
---is 0. Among the same priority, the wrap added last runs first. Every DLL's wrap runs outside every
---script's, whatever either's priority: a DLL sees each call before any script and gets back what
---the scripts returned. The log's wrap_order lines list each site's wrappers in the order they run, with
---"plugins:" before the DLLs' and "inner:" before the scripts'. Call `next` only while
---`fn` runs, and not from a coroutine; either is an error. A wrap that returns without calling
---`next` keeps the game's call from running; the first time it does, the log names it
---(lua_wrap_no_next). A coe5.ObservedSite takes a wrap too: what next may change there is that
---alias's note.
---@param site coe5.Site|coe5.ObservedSite|coe5.ArchivedSite
---@param fn fun(next: function, ...): ... the site's result (the AI sites: score, x, y)
---@param priority? integer higher runs first among the scripts' wraps; 0 when left out
---@return boolean wrapped false when the site isn't on this platform yet (logged as api_refused or lua_refused)
function coe5.wrap(site, fn, priority) end

---Watches a game call. `before` gets a copy of the call's arguments right before it, and `after` the
---call's result, then the arguments, right after it; either may be nil, not both. What they return
---doesn't reach the game. They run inside every script's wrap of the site: they see the call as the
---game makes it, after any wrap changed it, and the game's result, and a call a wrap skipped not
---at all. Every DLL's observers of the site run before any script's, in both `before` and `after`. A
---site that isn't a coe5.ObservedSite is an error. In a LAN
---game a personal mod's observers run only at the observe-only sites that fire on its player's
---machine (not UNIT_CATCH_FIRE, APE_POSSESSION, RITUAL_COST_PERCENT, or any of the 11 PLAYER_*_INCOME
---sites), and only for a call whose units, commanders and
---squares its player sees: a unit or commander as coe5.get would answer it, a square seen at least once.
---A replay on screen and the Game Over screen show everything.
---@param site coe5.ObservedSite|coe5.ArchivedSite
---@param before? fun(...)
---@param after? fun(result: any, ...)
---@return boolean added false when the site isn't on this platform yet (logged as api_refused)
function coe5.observe(site, before, after) end

---The turn: 0 on the first turn, -1 while a game is set up. The same as coe5.get("turn").
---@return integer
function coe5.turn() end

---This process's part in the game: coe5.net.single, coe5.net.host, or coe5.net.client.
---@return integer
function coe5.netRole() end

---The player this machine is playing now: on a LAN client the player whose turn is running, nil while it
---waits for a turn; in single player and hotseat the seat playing now; nil on a host.
---@return integer?
function coe5.localPlayer() end

---Your mod's table that lasts: kept in every save, and in a LAN game sent from the host to every client
---with each turn. Your Lua globals last only until the next load or LAN turn; put what must last here.
---Plain values only: booleans, numbers, strings, and tables of them with string, number or boolean
---keys. A function, a userdata, a table with a metatable, or a table that holds itself is left out when
---the game is saved (lua_refused, call=coe5.gameData, with at= its place). A load empties this table and
---fills it again in place, so keep coe5.gameData itself, never a table inside it, across a load. It is
---empty in a new game and in a save made before it existed: check for nil. A write while your files
---load (their top level) is undone and refused (lua_refused, reason=during_load): they run again at every
---load and LAN turn. Set first values in GAME_START or a later site. In a LAN game only the
---host's changes last; a client's are gone at its next turn (logged once a turn as game_data_not_sent):
---a client writes coe5.playerData instead.
---@type table
coe5.gameData = {}

---A table per player slot (0-47) that lasts as coe5.gameData does, empty until written. In a LAN game a
---client's turn file carries its playing player's table to the host, which takes it for that slot alone.
---That is how a client asks for what the game doesn't send (a square, an event variable, a message):
---write the request here in the client's turn, and act on it in a site the host runs, such as
---PLAYER_TURN_START or MONTH_START. Another slot's table written on a client stays on that client.
---@type table<integer, table>
coe5.playerData = {}

---Adds a message to a player's Messages window. Only human players get messages.
---With `where` the message carries a square and a unit, as the game's own messages do. Its Go to button
---centres the map on the square; for a commander's unit it also selects the commander, as the game's
---"A Hero" message does, when the commander is the reader's, alive, and not stupid or a loner. A unit alone
---gives its own square; a unit off the map (fled, waiting to reform) gives none, so no Go to.
---@param player integer|string a player number (0-31), a vanilla faction's name (coe5.factionPlayer's names; a mod's faction 32-47 is refused bad_player), or coe5.message.everyHuman
---@param title string cut to 99 characters
---@param text string cut to 2047 characters, the game's buffer for it
---@param where? {x?: integer, y?: integer, unit?: integer} a square (x below mapDepth, y below mapH; both or neither) and a unit
---@return integer index the message's index, or coe5.message.notAdded when no message was added. With coe5.message.everyHuman it is always coe5.message.notAdded, even when the message was sent. A bad slot or a LAN client is logged (api_refused or lua_refused), and so is a `where` that isn't a table of whole numbers (bad_where), a square off the map (off_map) or a unit not in the game's table (no_unit).
function coe5.sendMessage(player, title, text, where) end

---Shows the game's message box at once, with your title and text and one Ok button (Enter clicks
---it), and returns when the player closes it. Nothing is added to what you give. Use it when a
---human is at the screen, such as in an onPortalCrossing handler for a human's Enter Gateway or
---step. The box doesn't wrap: end each line with \n. The game fixes up the text's English, as on
---every box it shows: the word after a 1 and a space loses a last s that follows a
---small letter other than s or u ("1 gems" is drawn "1 gem", "21 gems" "21 gem", "option1 is"
---"option1 i"; "1 glass" stays).
---@param title string cut to 99 characters
---@param text string cut to 4999 characters
---@return boolean shown true once the player closed it; false when refused, with nothing shown: in a LAN game where no one can answer (lan_host, content_on_client, wait_screen), during the AI's or the independents' turn or inside an AI site (ai_turn), and in an onPortalCrossing handler for a route being planned (planning_route), each logged as lua_refused
function coe5.showMessageBox(title, text) end

---Reads one of your mod's settings. Declare settings with newsetting blocks in <ModName>.c5se
---(or #!@ lines in <ModName>.c5m; see Mod Settings in the docs). Scripts only read them. Names
---match ignoring case. Returns the player's current pick in Mod Settings on each call (or the
---default if a setting broke a rule). In a LAN game a content mod reads the host's pick and a
---personal mod reads the local player's pick. A setting read only once, when the script
---loads, takes effect after a restart: give it a restart line, and changing it offers one.
---@param name string the setting's name, as its newsetting line gives it
---@return boolean|integer|nil value on/off: true or false; a choice: the picked option's number from 0; a key: its key value (coe5.key.none for none); nil when your mod declares no such setting (logged as lua_refused)
function coe5.setting(name) end

---The text's translation, from the mods' translation lines the game has read (language blocks), or the text itself
---when no line has it. A translation is used only when its % codes match
---the text's, in order, so string.format of the result takes the same values. Read only; each player has a
---language pick, so store text as written and translate it where it is shown. Text you build with string.format
---translates only as string.format(coe5.translate(fmt), ...): the built text matches no translation line.
---@param text string the text as your mod writes it
---@return string text the translation, or the text itself
function coe5.translate(text) end

---Tells other mods that your script is filling in a new game's setup itself: while it is on, mods
---that change the setup screens leave them alone. Turn it on right
---before your code starts a new game's setup, and off once that setup returns. Every mod shares
---the one flag, so always turn it off again. A call that changes it logs setup_automated with your
---mod's name (by=<ModName>.lua).
---@param on boolean
function coe5.setSetupAutomated(on) end

---Whether a mod has said it is filling in a new game's setup itself (coe5.setSetupAutomated). A mod
---that changes the setup screens leaves them alone while it is true.
---@return boolean
function coe5.setupAutomated() end

---Hides the game's main menu button `id` on every frame where `hidden()` returns true. Only Read Tip,
---coe5.mainMenu.readTip, can be hidden; the buttons below it move up. Any number of mods may ask: the
---button is hidden while any of them says so.
---@param id integer coe5.mainMenu.readTip
---@param hidden fun(): boolean runs every main menu frame; keep it short
---@return boolean added false for any other button (logged as lua_refused)
function coe5.hideMainMenuButton(id, hidden) end

---Adds a line to your mod's page in Mod Settings, in a box titled "Screens" after your mod's
---settings. A click runs `open`, the mod's screen (a coe5.ui loop), then shows the page again. The
---tooltip comes last because it is optional; the C API's addScreen takes it second.
---@param label string 1 to 56 characters
---@param open fun()
---@param tooltip? string up to 255 characters
---@return boolean added false when the label or tooltip breaks a rule (the page lists it; logged as api_refused)
function coe5.addScreen(label, open, tooltip) end

---Sets key action `action`'s key (coe5.keyAction) to `key` (coe5.key, or a character's byte) on its
---screen, in the game's texts, and in the Keyboard Shortcuts list, in place of the game's key.
---coe5.key.none leaves the action with no key. The key lasts until the game closes or the next call;
---nothing is saved, so keep players' picks in key settings in your .c5se. Every mod shares one key per action and
---the last call wins: each change logs key_rebound with mod=, and key_rebound_over naming both mods
---when another mod set that key last.
---@param action integer
---@param key integer
---@return boolean changed false for an unknown action, a value that isn't a key, or on a LAN client a mod that isn't personal (not_personal_on_client; logged as api_refused)
function coe5.rebindKey(action, key) end

---Gives key action `action` a second key beside the primary key, which runs it too; the texts name both,
---"Esc/F10". coe5.key.none removes it.
---@param action integer
---@param key integer
---@return boolean changed false for an unknown action, a value that isn't a key, or on a LAN client a mod that isn't personal (not_personal_on_client; logged as api_refused)
function coe5.addActionKey(action, key) end

---Key action `action`'s key now.
---@param action integer
---@return integer|nil key nil for an unknown action (logged as api_refused), where the C API returns -1
function coe5.actionKey(action) end

-- ---- World, units, AI, and recruiting ---------------------------------------------------------

---The index of the first monster in the game's table with this name, ignoring case: a class
---leader's with coe5.get("classLeader", class). Its sprite is coe5.get("monsterSprite", index).
---@param name string
---@return integer|nil index nil when no monster has this name
function coe5.findMonster(name) end

---Makes one unit of `monster` for `player` on square (x, y), following no commander, the way the
---game's recruit purchase makes a troop: the game's newunit, then what the game does after it (the
---unit's land or water shape for the square, the square's army recount and conquest check, the
---player's sight around it). The game checks no terrain, owner, or sight here, and neither does
---CoE5SE: your script picks the square. A unique monster that already has a living unit moves that
---unit here, as the game's spawns do. For the variant the recruit screen would make, pass
---coe5.monsterVariant(monster).
---@param player integer|string a player number (0-31, a mod's faction 32-47) or a faction's name (coe5.factionPlayer's names)
---@param monster integer a monster's index (coe5.findMonster)
---@param x integer below mapDepth
---@param y integer below mapH
---@return integer|nil unit the unit's index, or nil: outside a game (reason=no_game), a player past the slots (bad_player), a monster index no monster uses (bad_monster), a square outside the world (off_map), or no free unit record (no_free_slot), each logged as lua_refused
function coe5.spawnUnit(player, monster, x, y) end

---Makes one commander of `monster` for `player` on square (x, y), with its unit and a name the game
---gives it, the way the recruit purchase makes a commander, then does what coe5.spawnUnit does after.
---@param player integer|string a player number (0-31, a mod's faction 32-47) or a faction's name (coe5.factionPlayer's names)
---@param monster integer a monster's index
---@param x integer below mapDepth
---@param y integer below mapH
---@return integer|nil commander the commander's index, or nil with the reasons coe5.spawnUnit gives (no_free_slot also when no commander record is free)
function coe5.spawnCommander(player, monster, x, y) end

---Promotes a unit, as the event action promote <commanderize> "monster" does: the unit becomes `monster` with full
---hit points, keeps its experience, kills, items and afflictions, and learns the new monster's start spells in place
---of its own. With commanderize true a plain unit also becomes a commander, with a name the game gives, the new
---monster's start rituals and no followers; it leaves the army it followed, and sees around it as a recruited
---commander does. A commander stays one and changes in place. A promotion into a unique monster follows the game:
---a living unit of it may be taken instead. In a LAN game, on the host or a client's own unit in its turn.
---@param unit integer a living unit
---@param monster integer a monster's index (coe5.findMonster); the unit's own for a promotion that keeps it
---@param commanderize boolean true to make the unit a commander
---@return integer|nil index the commander's index with commanderize, the unit's without; nil outside a game (reason=no_game), for a unit not in the table or dead (no_unit), a monster index no monster uses (bad_monster), or no free commander record or unit slot (no_free_slot: the monster changed, no commander was made), each logged as lua_refused
function coe5.promoteUnit(unit, monster, commanderize) end

---Runs the game's own commander rename for one commander, as key N on the unit stats screen does when the game is
---launched with --rename: the "Rename Commander" box, its field holding the commander's name, with Ok and Cancel. On Ok the
---name is written as the game writes it: each % becomes _, each comma a full stop, at most 47 characters, an
---empty name included. A player's leader can't be renamed, as in the game. The box waits for the player, so call
---it on the machine where that player plays, in their turn. In a LAN game, a client's rename of its own
---commander reaches the host with its turn.
---@param commander integer a commander with a living unit
---@return string|nil name the name written; nil on Cancel, outside a game (reason=no_game), for a commander with no living unit (dead_commander), or a player's leader (main_commander), each refusal logged as lua_refused
function coe5.renameCommander(commander) end

---What the event word newunits does: the units of a summon string on square (x, y), as rituals and events
---write it ("c*Captain & 2d6*Spearman": a commander, then 2d6 units following it), made for `player`
---by the game's reader. A monster name the game doesn't know is skipped, as in a ritual. As
---newunits, the player's sight isn't updated.
---@param player integer|string a player number (0-31, a mod's faction 32-47) or a faction's name (coe5.factionPlayer's names)
---@param x integer below mapDepth
---@param y integer below mapH
---@param monsters string a summon string, up to 1999 characters
---@return integer|nil made the units made, or nil: outside a game (reason=no_game), a player past the slots (bad_player), a square outside the world (off_map), or a string too long or holding a 0 byte (bad_string), each logged as lua_refused
function coe5.newUnits(player, x, y, monsters) end

---Moves a commander and every living unit following him on his square to (x, y), any plane, with
---the teleport ritual's call. It runs a walked step's checks: onto another player's army or
---independents at war it overruns weak defenders or sets up the battle, fought after the move with
---the others, and a hidden army there ambushes; a free square is conquered and its surroundings seen.
---A free square costs no action points; a square with an army at war spends them all, as walking in
---does. The commander's stored move order is cleared. Like the game's teleport, it checks nothing of
---the terrain: sea, a cave wall, or the Void take the army as any square does. coe5.mayEnterSquare
---says whether the army could walk there.
---@param commander integer 0-9989
---@param x integer
---@param y integer
---@return boolean teleported false when refused (lua_refused): outside a game (reason=no_game), a commander past the table (bad_commander), one with no living unit (dead_commander), a square outside the world (off_map) or on no plane (no_plane)
function coe5.teleportCommander(commander, x, y) end

---Whether the commander's army may walk onto (x, y), by the game's test for a move: the army's
---move flags against the square's terrain (sea, walls, the Void, mountains for a mountain-only army),
---whatever stands there now.
---@param commander integer 0-9989
---@param x integer
---@param y integer
---@return boolean may false also when refused, as coe5.teleportCommander refuses (lua_refused)
function coe5.mayEnterSquare(commander, x, y) end

---The monster the recruit purchase makes for an offer of `monster`: the game's pick, `monster` or
---one of the up to 9 records right after it with the same name, at random (a class's "Swordsman"
---with three looks).
---@param monster integer a monster's index
---@return integer|nil variant nil for a monster index no monster uses (logged as lua_refused, reason=bad_monster)
function coe5.monsterVariant(monster) end

---Where the portal on square (x, y) leads: the squares the game sends an army through it to. An
---entrance is a square whose squarePortal is above 0, and its exits are the squares with that
---portal number or its negative, the entrance left out, as the game finds them: the first 100 by
---x, then y. The game picks one of them at random each time, so the list is where it can lead.
---@param x integer
---@param y integer
---@return {x: integer, y: integer}[] exits empty for a square with no entrance or outside the world
function coe5.portalExits(x, y) end

---Calls `handler` before every crossing through a portal, so a mod can refuse it: a human's Enter
---Gateway (asked before the exit is drawn or seen), each step a human's route, a standing order, an
---AI commander or a wandering monster takes through a portal, an independent army's random exit,
---and every route the game plans, the AI's, a human's shown route and coe5.route's (with
---`planning` true). A handler that returns false refuses that one crossing, and the game goes on as
---for a portal it won't take itself: Enter Gateway does nothing (no exit is revealed, no action
---points are spent), a step is refused (the AI drops that move), a route goes another way or none,
---an independent army stays. Anything else, nil included, leaves the crossing to the game. Your mod
---decides everything else: which portals, one way or both, who may pass, and any message
---(coe5.showMessageBox, coe5.sendMessage, coe5.ui). Any number of mods may add handlers; each is asked in the order they
---were added, until one refuses. No handler means no call at all. On a LAN client none is asked, and on the host only a content mod's.
---A handler runs inside the game's move or route, so keep it quick: route planning asks many
---times a turn (a commander's every route that reaches the portal). coe5.route inside a handler
---returns nil (lua_refused reason=inside_portal_handler), and crossings a handler causes aren't asked.
---A handler that errors stops your script, and its crossings go through.
---@param handler fun(crossing: {x: integer, y: integer, toX: integer?, toY: integer?, portal: integer, player: integer?, commander: integer?, planning: boolean}): boolean?
---  x, y: the square the army leaves. toX, toY: where it arrives; nil for Enter Gateway, asked before
---  the game draws one of the portal's exits. portal: the leaving square's squarePortal, above 0.
---  player: the army's player (nil for none). commander: the moving commander (nil for an
---  independent army without one). planning: true for a route being planned, false for a move.
---@return boolean added false when refused: a game build CoE5SE wasn't made for (site_unavailable), each logged as lua_refused
function coe5.onPortalCrossing(handler) end

---The living units on square (x, y), the units the game counts there (squareBattleStrength, its square
---checks): hp above 0 and standing on the square, in unit order. A square's army is what stands on
---it; unitCommander tells which follow which commander. Sees through fog, as squareBattleStrength does.
---@param x integer
---@param y integer
---@return integer[] units unit indexes (unitHp, unitMonster, unitBattleStrength take them); empty for no living unit there or a square outside the arrays
function coe5.squareUnits(x, y) end

---The game's Reveal Map, the Game Over screen's button: every square of every plane in the world,
---the mods' planes included (Elysium2's copy aside), or of one plane, in view for `player`
---(squareSeen 2, a scried 3 included), then
---the game's sight update, which redraws the map for a human. Sight only rises within a turn, and
---the game lowers squares in view to seen before (1) at the player's next turn start, so they stay
---in view until then: terrain stays on the map after, armies show only where the player sees them.
---@param player integer|string a player number (0-31, a mod's faction 32-47) or a faction's name (coe5.factionPlayer's names)
---@param plane? integer a plane's index (coe5.planeIndex), 0 to the last plane; left out, every plane
---@return boolean revealed false when refused: outside a game (reason=no_game), a player past the slots (bad_player), a plane past the last (bad_plane), each logged as lua_refused
function coe5.revealMap(player, plane) end

---Every enabled mod's events, as the last full reading of the mods saw them (at startup, after the
---Mods screen, at a load, at a game's end), in the order the game runs them. Each entry is
---`{mod = , line = , kind = , message = , needs = , lines = }`: the mod's name, the line of its `playerevent`
---or `squareevent` in the mod's file, the kind (`"playerevent"` or `"squareevent"`), the text of its
---first `message` line ("" for none), each trigger line (`+` or `-`) as the mod writes it, and every
---line of the event as the game read it, the event word first and `endevent` last. A map file's
---events aren't listed. The index in this list is what coe5.runModEvent takes.
---@return {mod: string, line: integer, kind: string, message: string, needs: string[], lines: string[]}[] events
function coe5.modEvents() end

---Runs event `index` of coe5.modEvents() now, for the player whose turn it is, with every trigger
---line left out: its actions run through the game's event runner, as at a turn start, whatever
---the event needs. A playerevent's target is the player's home citadel, as the game sets it; a
---squareevent runs on square (x, y) alone. With its triggers gone, an `if`'s first branch runs. A
---bad action line stops the game with its Mod Error box, naming the event's file and line, as it
---would at a turn start.
---@param index integer the event's place in coe5.modEvents(), from 1
---@param x? integer a squareevent's square
---@param y? integer
---@return boolean ran false when refused: outside a game (reason=no_game), while the game reads a mod or runs an event (event_running), no such event (bad_event), a squareevent with no square (needs_square), a square off the map (off_map), each logged as lua_refused; on a LAN client, client_write_lost
function coe5.runModEvent(index, x, y) end

---Every army on the world map: each square of the world (x below mapDepth, y below mapH) with an
---army (squareArmy not 0), by x, then y. With viewer, only the squares that player sees now
---(squareSeen 2 or 3), which the game's AI refreshes its strength memory from; without it, every
---army, through fog. A viewer outside 0-31 is refused (lua_refused reason=bad_player) and the list
---is empty.
---@param viewer? integer a player slot 0-31
---@return {x: integer, y: integer, owner: integer, units: integer, commanders: integer[]}[] armies owner as squareArmyOwner, units as squareArmy (-1 while two players at war share the square), commanders the commanders whose unit stands there alive, in commander order, followers of another commander included (unitCommander tells)
function coe5.armies(viewer) end

---How far a commander's army has to go to reach square (x, y), by the game's pathfinder, the
---route the AI's moves take: from the commander's square, through portals too, with the whole map
---known (like the AI's routes, it ignores what the player has seen). comstr is the strength the
---route's end may face: a square held by an army stronger than it allows is closed. Left out, it is
---the commander's, the comstr the AI sites get. Only inside a wrap of an AI move site
---(AI_EXPLORE); anywhere else the call is refused (lua_refused reason=outside_ai_site) and returns
---nil. A route draws nothing from the game's random sequence that the game's later draws see.
---Each call, refused or not, counts 10,000 steps of the script's 20,000,000-step budget before the
---game routes, so about 2,000 routes fit in one call into the script (reason=step_limit past it).
---@param commander integer 0-9989
---@param x integer below mapDepth
---@param y integer below mapH
---@param comstr? integer -1 to 21474836; -1 routes as a player's move order does
---@return integer|nil length the route's cost in moves, about 1 a square on open ground, 0 on the commander's square; nil for no route, a commander with no living unit on a square, or a refused call (reason=outside_ai_site, bad_commander, off_map, out_of_range)
function coe5.route(commander, x, y, comstr) end

---Drops an AI commander's planned ritual (commanderRitualPlan) the way the game's "scrapping
---plan" does: the plan becomes 0, and the game's ritual step plans again when it next runs for the
---commander (in 80% of its runs), with each candidate passing AI_RITUAL_CANDIDATE. Only 0 is
---written: a script can't pick the plan. Only inside an AI site: a wrap of an AI move site, of
---AI_RITUAL_CANDIDATE, or of an army site (AI_GATHER_TROOPS, AI_PICK_UP_LONER, AI_SPLIT_ARMY), or
---an observer of AI_TURN (before the player's moves, so its commanders plan again that turn). Refused (lua_refused) and false: outside them (reason=outside_ai_site), a
---commander past the table (bad_commander), one with no living unit (no_unit), one whose player
---isn't an AI (not_ai), and a spawnbooster's plan, -1 (not_ritual). Each scrap logs
---ai_plan_scrapped with the commander, its player, and the ritual. Works on both builds where an AI
---site runs (Mac: AI_TURN).
---@param commander integer 0-9989
---@return boolean ok true when the commander has no plan now (scrapped, or it had none)
function coe5.scrapRitualPlan(commander) end

---Frees one unit of an AI player from the commander it follows (unitCommander becomes -1), as the
---game's unfollowtarg ritual effect does, and resets that commander's cached action points, since
---its army changed. The unit stays on its square, a free unit of its player, which the AI's troop
---pick-up can take again. Only inside an AI site, as coe5.scrapRitualPlan, and only for a unit of the
---player whose AI turn runs (coe5.get("activePlayer")). Refused (lua_refused) and false: outside an AI
---site (reason=outside_ai_site), on a LAN client (network_game), a unit past the table (bad_unit),
---one dead or of no player (no_unit), one whose player isn't an AI (not_ai), one of another player
---(not_moving_player). Each release logs ai_follower_released with the
---unit, its old commander, and its player.
---@param unit integer 0-98999
---@return boolean ok true when the unit follows no one now (released, or it followed no one)
function coe5.releaseFollower(unit) end

---Moves one unit of an AI player from the commander it follows to another commander of the same
---player on the same square, the game's way: the army screen's transfer, with its rules (no
---stationary unit, none that moves on its own, none led by a wandering commander, no insane
---commander's unit) and both
---commanders' cached action points reset. Only inside an AI site, for a unit of the player whose AI
---turn runs, as coe5.releaseFollower. Refused (lua_refused) and false: the reasons of
---coe5.releaseFollower, then a commander past the table (bad_commander), one with no living unit
---(no_commander), one of another player (other_player), the commander's unit (same_unit), a unit
---that follows no one (not_following), one that follows the commander already (already_following),
---the two on different squares (not_same_square), and one the game's transfer refuses
---(game_refused).
---Each transfer logs ai_follower_transferred.
---@param unit integer 0-98999
---@param commander integer 0-9989
---@return boolean ok true when the unit follows the commander now
function coe5.transferFollower(unit, commander) end

---A player's recruitment offers this turn, those with units to buy, in the shop's order: what
---Recruit lists, the other citadels' terrain-bound offers included. A magic item for sale and an
---offer already bought (once per offer) are left out. Refused (lua_refused reason=bad_player, an
---empty list) for a player outside 0-31.
---@param player integer|string a player number (0-31, a mod's faction 32-47) or a faction's name (coe5.factionPlayer's names)
---@return coe5.Offer[] offers
function coe5.shopOffers(player) end

---A class's recruitment list (addunitrec, addmercrec, addcomrec, in its order): every offer the
---class can get, before the month's chance rolls. The standard troops of stdtroops aren't in it
---(classStandardTroops). Refused (lua_refused reason=bad_class, an empty list) for a class outside
---0-249 (0-199 on Mac).
---@param class integer playerClass's number
---@return coe5.Offer[] offers
function coe5.classRecruits(class) end

---One of the 61 offers the game's monthly shop build adds from its code (coe5.builtinOffers):
---the standard troops, the mercenary bands, the Hero, the hired wizards, the Assassin, the guild
---offers, the ship guilds' offers and a magic item for sale. Its chance comes from
---coe5.builtinOfferChance. The fields a row takes (coe5.addShopOffer) have the same names.
---@class coe5.BuiltinOffer
---@field id integer its number from 1, in the build's order, for builtinOfferChance and replaceBuiltinOffer
---@field name string the monster ("Magic item" for the item for sale)
---@field count integer units it offers; -1 the magic item
---@field item boolean the magic item for sale: a random one each time
---@field commander boolean the unit comes as a commander, with a name
---@field gold integer base gold
---@field goldDie integer a die added to gold each time the offer comes
---@field goldDieExplodes boolean the die rolls again on its top face and adds the new roll less 1 (false only for the Captain)
---@field iron integer
---@field costs table<integer, integer> extra costs by cost type (the Void Ship: [15] = 100 gems)
---@field terrain integer where it is bought: 0 anywhere, a terrain group below -1 (-93 ports, -106 the sky plane)
---@field oncePerOffer boolean bought once, then gone
---@field extra boolean a buy leaves the square's recruitment for the turn unused
---@field aiImportant boolean the game's AI tries it first
---@field aiNever boolean the game's AI never buys it
---@field message boolean the month's Recruitment Offer message comes with it
---@field outOf integer what the chance is out of: 100, 150 (the Hero), 500 or 1000
---@field price string its price rule: "plain" (gold + die), "mercenary" (times mercpricemult), "hero" (5 less per Adventurers or Beastmasters guild, at most 5), "assassin", "shadow_stalker", or "item" (the item's extracost + 20 + two dice)

---The 61 offers the shop build adds from its code, in its order (coe5.BuiltinOffer). The class
---lists and the special locations' offers come from records instead (coe5.classRecruits).
---@return coe5.BuiltinOffer[] offers
function coe5.builtinOffers() end

---Built-in offer `id`'s chance this month for `player`, worked out as the game does (guilds, library
---levels, town points, the class's recwizchance, recherochance, recasschance and mercboost), and
---what it is out of. Offers the game adds every month once their need is met (the standard troops,
---the Ship, the Galleon) give outOf when it is met and -1 when it isn't. The AI difficulty adds
---nothing to these. Refused (lua_refused reason=bad_id or bad_player, nil).
---@param id integer 1-61
---@param player integer|string a player number (0-31, a mod's faction 32-47) or a faction's name (coe5.factionPlayer's names)
---@return integer? chance
---@return integer? outOf
function coe5.builtinOfferChance(id, player) end

---The player's guilds made by the guild ritual `guild` ("Apothecaries Guild", or a mod's guild
---ritual), counted as the shop counts them: on squares the player or a teammate holds, or with
---`ownSquares` only the player's (the mercenary bands' and the Hero's count). A guild in a town an
---enemy took doesn't count, and an upgraded guild counts under its upgrade's name. Refused
---(lua_refused reason=bad_player or not_a_guild, nil).
---@param player integer|string a player number (0-31, a mod's faction 32-47) or a faction's name (coe5.factionPlayer's names)
---@param guild string
---@param ownSquares? boolean
---@return integer? count
function coe5.guildCount(player, guild, ownSquares) end

---A monthly offer's fields, for coe5.addShopOffer and coe5.replaceBuiltinOffer. Every field is
---optional for replaceBuiltinOffer, which starts from the built-in offer's own.
---@class coe5.ShopRow
---@field name? string the monster (addShopOffer needs it)
---@field count? integer units, 1-1000 (1)
---@field commander? boolean the unit comes as a commander, with a name
---@field gold? integer base gold (0); setting gold or goldDie on a replaced offer drops its price rule
---@field goldDie? integer a die added to gold (0)
---@field goldDieExplodes? boolean (true)
---@field iron? integer
---@field costs? table<integer, integer> one extra cost: {[type] = amount}, type 2-18
---@field terrain? integer where it is bought: 0 anywhere (0), a terrain number, or a group below -1
---@field oncePerOffer? boolean
---@field extra? boolean
---@field aiImportant? boolean
---@field aiNever? boolean
---@field limiter? string a "=" line: the offer upgrades those units ("=Militia")
---@field chance? integer|fun(player: integer): integer each month's chance (0); a replaced offer keeps its rule without one
---@field outOf? integer what chance is out of (100)
---@field message? boolean the Recruitment Offer message when it comes (true)

---Adds an offer every player's shop rolls at each turn start, after the game's: with `chance` in
---`outOf`, its gold die rolled, through the game's add and message calls. Any number of rows, from
---any number of mods; a row of a script that stopped no longer rolls. On a LAN client no row rolls, and on the host a content mod's do. Refused
---(lua_refused, false): reason=no_name, no_monster, bad_field, bad_costs, out_of_memory,
---site_unavailable.
---@param row coe5.ShopRow
---@return integer|false row its number
function coe5.addShopOffer(row) end

---Turns built-in offer `id` off and adds a row in its place, built from the offer's fields, chance
---rule and price rule, with each field of `row` replacing that part. The game still rolls the offer,
---so no other offer's dice change; the offer it made is emptied and its line taken out of the month's Recruitment Offer message, and the row
---rolls after the game's offers. It counts toward the 50 rows. Two scripts replacing one offer get a
---row each. Refused (lua_refused, false): reason=bad_id and those of addShopOffer.
---@param id integer 1-61
---@param row? coe5.ShopRow
---@return integer|false row its number
function coe5.replaceBuiltinOffer(id, row) end

---The squares where the game's AI recruiting can buy for the player: its citadels (squares
---whose terrain is a citadel for its class), by y then x, at most 100, each {x, y, used}, used true
---when the square's recruitment is used this turn. With an offer (coe5.Offer.offer), only the
---squares where coe5.aiBuyOffer would buy it now. Refused (lua_refused, an empty list):
---reason=bad_player, bad_offer (past the player's last offer), windows_only on Mac.
---@param player integer|string a player number (0-31, a mod's faction 32-47) or a faction's name (coe5.factionPlayer's names)
---@param offer? integer
---@return {x: integer, y: integer, used: boolean}[] squares
function coe5.recruitSquares(player, offer) end

---Buys one of an AI player's offers on square (x, y) the way the game's recruiting step does:
---the same purchase (the units, or a commander, made there and every cost paid), then an offer that
---isn't extra uses the square's recruitment for the turn and a once-per-offer offer is gone. Only
---inside a wrap of AI_RECRUIT, for the player recruiting. The game's square rules hold: the
---offer must be buyable there (its terrain, costs, limiter), the square one of the player's
---recruit squares, unused this turn, and the monster's .c5m AI words met (aipowcom1-3, aimaxshop,
---aihold, a ship by the sea). Refused (lua_refused) and false: reason=outside_ai_recruit,
---bad_player, other_player, not_ai, bad_offer, off_map, not_recruit_square, item, sold_out,
---not_for_ai, cannot_pay, too_few_to_upgrade, not_here, square_used, aipowcom, aimaxshop, no_sea,
---aihold; windows_only on Mac; the second result is that reason word. False alone: a commander
---offer the game's buy couldn't make (no free commander, or no unit to upgrade), with the square
---and the offer used all the same, as the game's step leaves them. Each buy logs
---ai_offer_bought with the gold before and after.
---@param player integer|string a player number (0-31, a mod's faction 32-47) or a faction's name (coe5.factionPlayer's names), the player of the AI_RECRUIT call
---@param offer integer coe5.Offer.offer
---@param x integer
---@param y integer
---@return boolean bought
---@return string? reason the lua_refused reason when refused
function coe5.aiBuyOffer(player, offer, x, y) end

---The buys and ritual plans scripts made for `player` this turn, in order, each with the mod that
---made it: a call "aiBuyOffer" (each buy) with offer, monster, count, x, y and commander (the new
---commander; nil for troops), and "planRitual" or "scrapRitualPlan" with commander and ritual. The
---list starts again with the first call of a new turn; it holds 64 a turn, and the second result
---counts the ones past that. It only reads: no call changes. Empty for a player with no call this
---turn.
---@param player integer|string
---@return {mod: string, call: string, offer?: integer, monster?: integer, count?: integer, x?: integer, y?: integer, commander?: integer, ritual?: integer}[] actions
---@return integer cut
function coe5.aiActions(player) end
-- On a played AI's Recruit screen: inside a RECRUIT_WAIT wrap, when the screen is the active
-- player's and that player is human, coe5.aiBuyOffer also buys for that player, at the screen's
-- selected square only (else not_screen_square), through the game's AI buy, with the Recruit
-- screen's checks in place of the AI's rules (square_used, not_enough_gold, not_enough_iron,
-- not_enough_units, cannot_pay, wrong_plane, no_upgrade_units, sold_out, not_here, not_possible,
-- item, too_few_to_upgrade). Windows only.

---The game's AI moves for `player` in its turn, from where the player left its commanders: the
---way Play as AI hands a played AI back to the game. Only in a HUMAN_TURN `after` observer for that
---player's turn. The player is an AI while the game's ai_move runs, then gets back its controller.
---Nothing of a turn's start runs again: no income, no action points back, no upkeep.
---Refused (lua_refused) and false: reason=network_game, outside_human_turn_end, not_this_turn,
---resigning.
---@param player integer the player whose human turn just ended
---@return boolean ran
function coe5.runAiTurn(player) end

---The Recruit screen, inside a RECRUIT_WAIT wrap: its player, its selected square (x and y -1 with
---none), whether a ctrl key is down (a ctrl click toggles an offer's auto-recruit), and its click
---latch (1 once a frame had no mouse button; the screen acts on a click only then). nil, refused
---(lua_refused), outside a RECRUIT_WAIT wrap (outside_recruit_wait), on Mac (windows_only), or
---when the screen can't be read (no_frame).
---@return {player: integer, x: integer, y: integer, ctrl: boolean, latch: integer}|nil screen
function coe5.recruitScreen() end

---Inside a wrap, after its next(): the other scripts' wraps of the same site, inside that next(),
---that changed what their next() gave them, innermost first. `answer` is true when a wrap
---returned something else than its next() returned (or skipped next()); `arguments` when it passed
---other arguments to its next(). A wrap that passed everything on unchanged isn't listed. It only
---reads: no wrap's arguments or answer change. Wrap at 2000000000 to see every script's change;
---a plugin DLL's wrap isn't listed. An empty list (lua_refused) outside a wrap (outside_wrap) or
---before next() (next_not_called). On Mac the AI steps Mac's game copied into their callers list no
---changes, for now.
---@return {mod: string, priority: integer, answer: boolean, arguments: boolean}[] changes
function coe5.changedBy() end

---Centres the map view on world square (x, y), on the plane that holds it, as the game centres the
---map on a commander or a message's square: the view switches plane as a plane button would, then
---the camera moves (a glide on the same plane). Only the view changes: nothing a game turn decides
---reads the camera or the view plane. Refused (lua_refused) and false: reason=no_game, off_map (x
---or y outside the world: mapW and mapH), no_plane (a square on no plane).
---@param x integer a world square's x (planes side by side, as mapPointerX)
---@param y integer
---@return boolean centred
function coe5.centerMap(x, y) end

-- ---- Preview world ----------------------------------------------------------------------------

---Builds a preview world: every plane from a seed, the whole world in view for a viewing seat, and
---the map on `plane` at the first turn, before anyone moves. The same seed and the same mods give
---the same squares and units, and a preview world can't be played, saved or continued, so this is
---for a tool that shows a modder a plane as a player gets it. Only from the main menu: a
---main menu button's function, or a MAIN_MENU_WAIT wrap below priority 0. The world starts when that
---script code is done. Windows only for now: on Mac the function doesn't exist, so test for it with
---rawget(coe5, "startPreviewWorld"). Nothing it does reaches a real game; other mods' Game Menu lines stay
---hidden in the world, and only this script's show.
---@param world {plane: string, seed: integer, mapSize: integer, society: integer, wilder?: boolean, startPositions?: integer, uniqueRandom?: boolean, seats: integer[]} `plane`: a plane's name, a game plane or an enabled mod's. `mapSize`: coe5.mapSize. `society`: as the game value `society`, 0 Random; the seed rolls Random. `startPositions`: coe5.startPositions, clustered when left out. `uniqueRandom`: Setup Participants' "unique random players". `seats`: the players' class numbers, 1 to 24 of them, 0 for a Random class the seed rolls. The first is a human in slot 0, with a class, home and units as in a real game; the rest are AIs
---@return boolean started false when refused (lua_refused): not_in_main_menu, network_game, already_active, bad_plane, bad_seed, bad_map_size, bad_society, bad_start_positions, bad_seats, bad_seat_class, hooks_not_installed
function coe5.startPreviewWorld(world) end

---Ends the preview world at once, with no AI turn, and goes back to the main menu. The game re-reads
---every mod's files as a world ends, so a file saved before this call is what the next world builds.
---From a Game Menu line's onClick or a map frame.
---@return boolean ended false when refused (lua_refused): no_preview_world, already_ending, not_at_first_turn
function coe5.endPreviewWorld() end

---Whether a preview world is running, and how long the last one took to build.
---@return boolean active
---@return number? seconds from coe5.startPreviewWorld to the map's first frame, nil before the first world of this launch
function coe5.previewWorldActive() end

---Tells CoE5SE where a tool's panels sit on a preview world's map. While the pointer is over one, the
---wheel doesn't zoom the map, a click doesn't reach the map (read it with mouseButtons after next in a
---MAP_SCREEN_WAIT wrap), and the game's hover box shows coe5.setPreviewHoverText's text or nothing. Keys
---still work. A left press on the map keeps MAP_DRAW running, so panels stay on screen while the button is
---down; mouseButtons after next reads the press, and the game's left clicks act as in the vanilla game. An
---empty list clears them, and so does the world's end. Nothing here reaches a real game.
---@param docks {x: number, y: number, w: number, h: number}[] rectangles in UI units, y up from the bottom, as coe5.ui.drawRect takes
---@return boolean set false when refused (lua_refused): no_preview_world, too_many_docks, bad_dock
function coe5.setPreviewDocks(docks) end

---What the game's hover box says while the pointer is over a coe5.setPreviewDocks rectangle.
---@param text? string nil or left out for no box
---@return boolean set false when refused (lua_refused): no_preview_world
function coe5.setPreviewHoverText(text) end

---Whether a tool's text field takes the keyboard on a preview world's map. While on, no key press
---reaches the game: no shortcut, no key action, no held key moving the view. coe5.takePreviewFieldKeys
---reads them. alt+Tab, alt+Enter and alt+F4 still reach the window, and a screen opened over the map gets
---its keys. The world's end turns it off. Nothing here reaches a real game.
---@param on boolean
---@return boolean set false when refused (lua_refused): no_preview_world
function coe5.setPreviewKeyField(on) end

---The keys a coe5.setPreviewKeyField field took since the last call, oldest first: a string for typed text
---(ASCII and the game's 16 accented letters, as the game's text boxes take), or coe5.key.backspace,
---coe5.key.enter (keypad Enter too) or coe5.key.esc. Backspace repeats while held; Enter and Esc come once.
---@return (string|integer)[]? keys nil when refused (lua_refused): no_preview_world
function coe5.takePreviewFieldKeys() end

---What each line of a plane's newplane block did in this preview world, in file order. A game plane has no block, so an empty list.
---state is off (an edit switched it off), refused (the parser), failed (the build couldn't), partly, overridden, nothing, rolledNo, worked, or notMeasured (a word with no counter yet, never worked). edited: an edit ran in its place or added it; fileText: the file's line then.
---did and of are copies placed of copies asked, rolls won of rolls, or squares changed. reason is the refusal's or the build's word, by the earlier or later line it is refused or overridden because of, blames the lines placed before a piece that found no room.
---armies, squares and rects are what it holds now, in world squares: from the unit table, the terrain record, a piece's rectangles.
---@param plane string the plane's name
---@return {line: integer, file: string, mod: string, word: string, text: string, state: string, plane?: string, off?: boolean, edited?: boolean, fileText?: string, added?: boolean, row?: any, zone?: string, reason?: string, by?: integer, from?: integer, to?: integer, did: integer, of: integer, placed: integer, units: integer, w?: integer, h?: integer, blames?: integer[], order?: integer, orderOf?: integer, copies?: table[], close?: string, replaces?: string, sameNameTerrain?: integer, sameNameLeft?: integer, armies: table[], squares: table[], rects?: table[]}[]? rows nil when refused (lua_refused): no_preview_world
function coe5.previewLineResults(plane) end

---Each mod event of this preview world's passes and what it did: `passes` it was reached in, `ran` passes an action line ran in, `stopped` passes a trigger stopped it in with the last such line's `stopLine` and `stopText`, and `planes`, the indexes of the planes its targets stood on. Refused outside a preview world (lua_refused reason=no_preview_world). Windows only for now.
---actions and turns are its action lines and turn triggers, one per line; named is the plane indexes its +plane and randloc lines name; held is the triggers that held in the pass that ran or got furthest; changed is the plane indexes whose units or terrain it changed. stopLine is the furthest line any pass stopped at.
---With a plane index, each row also holds what it did there: did, one row per action line that changed something (units made or changed, left of those still standing as it left them, later of those a later line changed or that moved off the plane, killed, squares changed, squaresLeft still holding its terrain), and armies and squares, the world squares it changed there. Compared with the unit table and every plane's terrain after each action line, so it holds whatever the line's word.
---@param plane? integer a plane's index
---@return {mod: string, file: string, line: integer, text: string, actions: string, turns: string, held: string, named: integer[], passes: integer, ran: integer, stopped: integer, stopLine?: integer, stopText?: string, planes: integer[], changed: integer[], did?: {line: integer, text: string, units: integer, left: integer, later: integer, killed: integer, squares: integer, squaresLeft: integer}[], armies?: {x: integer, y: integer, units: integer}[], squares?: {x: integer, y: integer}[]}[]|nil
function coe5.previewEventResults(plane) end

---What scripts did in this preview world, one row per place, so a loop's writes are one row with their `count`. `kind` is call (a coe5 call or written value that changes the world), refused (`reason` says why), error (`text` is the message), or wrap (the wrap of site `call`, at `line`, changed an answer). Refused outside a preview world (lua_refused reason=no_preview_world). Windows only for now.
---@return {mod: string, file: string, line: integer, call: string, count: integer, kind: string, reason?: string, text?: string}[]|nil
function coe5.previewScriptResults() end

---What the pass at this preview world's end and the next world's lines take in place of a file's lines; nothing is written to a file. Replaces the list; an empty table empties it. A world that doesn't start at the menu's next frame empties it, and a real game's pass never reads one.
---An edit names its line by mod, block and text, and which of the equal lines of the block (occurrence, from 1), never by number. A line changed in the file finds no edit, so the file wins.
---off drops the line, changed runs in its place, added with changed runs after it. Results then say off and edited, with fileText.
---@param edits {mod: string, plane: string, text: string, occurrence?: integer, off?: boolean, changed?: string, added?: boolean}[]
---@return boolean set false when refused (lua_refused): no_preview_world, bad_edit, out_of_memory
function coe5.setPreviewLineEdits(edits) end

---Every line of every plane's block in this preview world, of any mod, which mod each new monster came from, by name, and which each new terrain came from, by its selectterr number.
---skipped is the words the last pass didn't know, scripts the mods with a script, changedScripts each script file changed since it loaded, which runs only after a relaunch.
---@return {lines: {plane: string, mod: string, file: string, line: integer, word: string, text: string, state: string, from?: integer, to?: integer}[], monsters: table<string, string>, terrains: table<integer, string>, skipped: {mod: string, file: string, line: integer, word: string, close?: string}[], scripts: string[], changedScripts: {mod: string, file: string}[]}|nil index nil when refused (lua_refused): no_preview_world
function coe5.previewLineIndex() end

---What placed the units on world square (x, y) in this preview world: one row per kind of unit and source. A row names its line, or the game step that made them (step: world, population with planeIndex, month, event, script, gateway). An event's action line names itself: eventLine, its event's first line, and actionLine, with text, file and mod; step event with no eventLine is what no line was caught doing.
---placedAs and changes say what event lines did to them since: the monster they were placed as, and each change first to last, with from, to and the line's fields.
---@param x integer
---@param y integer
---@return {monster: integer, units: integer, line?: integer, file?: string, mod?: string, word?: string, text?: string, plane?: string, step?: string, planeIndex?: integer, eventLine?: integer, actionLine?: integer, scriptLine?: integer, placedAs?: integer, changes?: {from: integer, to: integer, eventLine: integer, actionLine: integer, file: string, mod: string, text: string}[]}[]? rows nil when refused (lua_refused): no_preview_world, bad_square
function coe5.previewArmySource(x, y) end

---Every write of world square (x, y)'s terrain in this preview world, first to last, each with the terrain it left and its line or game step (as coe5.previewArmySource's rows).
---@param x integer
---@param y integer
---@return {terrain: integer, line?: integer, file?: string, mod?: string, word?: string, text?: string, plane?: string, step?: string, planeIndex?: integer, eventLine?: integer, actionLine?: integer}[]? rows nil when refused (lua_refused): no_preview_world, bad_square
function coe5.previewSquareSource(x, y) end

---Every plane the enabled mods declare, in load order. A plane that failed stays in the list with its
---reason: the word its refusal logged, a newplane block's or a coe5.addPlane call's, or no_room when the last world had no room for it.
---@return {name: string, mod: string, file: string, line: integer, lines: integer, w: integer, h: integer, failed?: string}[] planes `file` and `line`: where the plane's declaration starts. `lines`: how many plane lines it took. `w` and `h`: its size as declared, 0 for a plane refused
function coe5.declaredPlanes() end

---Shows the army on world square (x, y) in the game's army view, the screen a right click on an
---army opens: its battlefield, both sides deployed. It waits there until the player leaves it, then
---returns. The game's random sequence is put back after, so a view changes nothing a later turn
---draws. Refused (lua_refused) and false: reason=no_game, off_map (outside the world: mapW and
---mapH), no_army (no army on the square), windows_only (Mac, for now). Logs lua_army_viewed.
---@param x integer a world square's x (planes side by side, as mapPointerX)
---@param y integer
---@return boolean shown
function coe5.viewArmy(x, y) end

---Shows `player`'s sight on the human `seat`: the map then shows the seat what that player sees,
---fog and all, and the game shows the seat that player's moves. The seat's sight is kept the
---first time, and coe5.restoreSight puts it back. Nothing else changes: the player's sight and
---every other player's stay as they are. At the seat's next turn start the game ages the seat's
---sight as always, so the player's "in view now" squares become "seen before" ones. Refused
---(lua_refused) and false: reason=network_game, no_game, bad_player (outside 0-31), same_player,
---seat_not_human, out_of_memory; windows_only on Mac, for now.
---@param seat integer a human player
---@param player integer the player whose sight to show
---@return boolean shown
function coe5.showSight(seat, player) end

---Puts back the sight coe5.showSight kept for the seat, as it was at that first call. Refused
---(lua_refused) and false: reason=bad_player, no_game, not_shown (nothing kept in this game; a new
---or loaded game drops it), windows_only on Mac, for now.
---@param seat integer
---@return boolean restored
function coe5.restoreSight(seat) end

---Gives `seat` the game's Battle Report message for every battle an AI faction fights that the
---seat isn't in (a battle of independents or Horrors alone gives none), as if it saw the square: "You have observed a battle in ...", with the jump to the square, the replay and the
---stats, on its Messages screen at its next turn start. The seat gets no sight: what it and every
---other player see stays as it is. The game sends the reports only while its Battle reports option
---is on (coe5.get("battleReports")), and only while the seat is human. nil or -1 turns it off; a
---new or loaded game starts with none. Refused (lua_refused) and false: reason=network_game,
---bad_player (outside 0-31), not_installed; windows_only on Mac, for now.
---@param seat integer? a player 0-31, or nil or -1 for none
---@return boolean set
function coe5.reportBattlesTo(seat) end

---Adds a line to the map editor's toolbox, right under "Create Fancy Terrain". The editor's Tab
---(Hide Toolbox) hides it with the rest. CoE5SE gives the line an id the editor itself never uses
---(44 to 999, in the order lines are added); a click on the line runs onClick, and so does a key
---bound to the id with coe5.ui.bindKey in a MAP_EDITOR_WAIT wrap, toolbox hidden or not.
---@param line coe5.MapEditorLine
---@return integer|false id the line's id, or false when `label` is empty, all 956 ids are taken, or the site isn't on this build (logged as api_refused)
function coe5.addMapEditorLine(line) end

---Adds a line at the end of the selected commander's orders on the map (the column with Sentry,
---Transfer Units and Use Special Power), drawn as the game draws its own: the icon, the label beside
---it, the hover text. The column's frame grows to hold it. The map draws the column only while a
---commander is selected. Past 16 shown lines of all mods (fewer when the screen's bottom comes first),
---the mods' lines go on pages: a small box under the column says "Page 1 of 2" and has Previous and
---Next; the game's lines show on every page, and selecting another commander goes back to the
---first. A click on the line runs onClick and nothing else: the map doesn't take the click.
---@param line coe5.CommanderLine
---@return integer|false id the line's id (2000 to 9997, in the order lines are added), or false when `label` is empty, `icon` is below 0 or past the sprite numbers (bad_icon), `color` isn't 3 or 4 parts from 0 to 1 (bad_color), all 7998 ids are taken, or the site isn't on this build (logged as api_refused or lua_refused)
function coe5.addCommanderLine(line) end

---The map view now (the map screen's or the map editor's), in UI units: x from the left, y up from
---the bottom, screenW x screenH. World square (x, y) is `zoom` units wide, its top left corner at
---(originX + x * zoom, originY - y * zoom). firstX to lastX and firstY to lastY are the squares of
---the shown plane at least partly on the screen (first is above last when none is). A map file
---numbers a square x - planeX, y - planeY.
---@return coe5.MapView|nil view nil when the shown plane isn't one of the game's
function coe5.mapView() end

---Gives an AI commander with no plan (commanderRitualPlan 0) the plan of casting one of the rituals
---it knows (commanderRitual), as the game's planner writes a candidate it keeps. The game's
---ritual step then treats it as any plan it holds: casts it when the caster can, keeps it while it
---can't cast it right now or can't pay, and scraps it as the game scraps a plan ("cannot cast",
---"pointless", "aibad"). The game's reasons for drawing a candidate (airare, aialways, the
---income checks) don't apply, as they don't to a plan held from an earlier turn. Set from an
---AI_TURN observer, the commander's ritual step sees it that same turn. Only inside an AI site, as
---coe5.scrapRitualPlan. Refused (lua_refused) and false: outside them (reason=outside_ai_site), a
---commander past the table (bad_commander), one with no living unit (no_unit), one whose player isn't
---an AI (not_ai), a commander with a plan already, a spawnbooster's -1 included (has_plan: scrap it
---first), and a ritual the commander doesn't know (not_known). Each plan logs ai_plan_set with the
---commander, its player, and the ritual.
---@param commander integer 0-9989
---@param ritual integer a ritual the commander knows
---@return boolean ok true when the plan is set
function coe5.planRitual(commander, ritual) end

---The AI's "aibad" test of a ritual for a commander, as its ritual plan choice drops a candidate: the
---commander too strong for an aiweakonly ritual, the player's casts at aimaxcast, or an immobile caster off
---the ritual's terrain.
---Named after the game's AI debug log line "aibad".
---@param commander integer 0-9989, with a living unit
---@param ritual integer a ritual (1 up)
---@return boolean|nil bad true when the AI drops the ritual, nil when refused (lua_refused: no_game, dead_commander, bad_ritual)
function coe5.aiRitualBad(commander, ritual) end

---The AI's "bad time" test of a ritual for a commander at the caster's square or a target square: water or
---the Sky for a caster that can't stand there, the ritual's aiwhere, ainothere, ainotclose1-3 and
---ainotnearhome, and another team's enchantment there.
---Named after the game's AI debug log line "bad time for ritual right now".
---@param commander integer 0-9989, with a living unit
---@param ritual integer a ritual (1 up)
---@param x? integer a target square's x, or -1 (the default) for the caster's square
---@param y? integer its y, or -1
---@return boolean|nil bad true when the AI waits, nil when refused (lua_refused: no_game, dead_commander, bad_ritual, off_map)
function coe5.aiRitualBadTime(commander, ritual, x, y) end

---How much the AI values an item for a unit: its item redistribution hands each item on a square to the
---unit there that values it most.
---@param unit integer a living unit
---@param item integer an item (itemName's index)
---@return integer|nil value nil when refused (lua_refused: no_game, dead_unit, bad_item)
function coe5.aiItemValue(unit, item) end

---The AI's rating of an item for use by a unit: 0 not usable, 1 kept, above 1 used when its commanders
---use items.
---@param unit integer a living unit
---@param item integer an item (itemName's index)
---@return integer|nil rating nil when refused (lua_refused: no_game, dead_unit, bad_item)
function coe5.aiItemUseRating(unit, item) end

---The square at the same spot on another plane, as a planeshift finds it: the same offset, wrapped to that plane's size and kept off its edges when that plane is smaller. Nil for a square outside every plane.
---@param plane integer a plane 0-10
---@param x integer integer
---@param y integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.planeSameSpot(plane, x, y) end

---Whether two planes count as one place for the game's wanderers and for reforming immortals: the same plane, or both among Elysium, the Sky and Agartha.
---@param planeA integer a plane 0-10
---@param planeB integer a plane 0-10
---@return boolean|nil result nil when refused (lua_refused)
function coe5.planesAlike(planeA, planeB) end

---An ability's total over every living unit on a plane, their items, blessings and temporary abilities counted. Hades counts its monuments this way.
---@param plane integer a plane 0-10, -1 for any, -2 for Elysium, the Sky and Agartha
---@param ability integer|string an ability's word or id
---@return integer|nil result nil when refused (lua_refused)
function coe5.planeAbilitySum(plane, ability) end

---The portal entrances on a plane whose portal number a square of otherPlane holds too. The Inferno counts its ways out this way.
---@param plane integer a plane 0-10, -1 for any, -2 for Elysium, the Sky and Agartha
---@param otherPlane integer a plane 0-10, -1 for any
---@return integer|nil result nil when refused (lua_refused)
function coe5.portalsLinkedTo(plane, otherPlane) end

---The portal entrances a player owns on a plane. portal 0 counts every portal number, another counts only that one.
---@param player integer a player slot, -1 for any
---@param plane integer a plane 0-10, -1 for any
---@param portal integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.countPlayerPortals(player, plane, portal) end

---The squares of a terrain on a plane, as +minterrains counts them.
---@param terrain integer a terrain
---@param plane integer a plane 0-10, -1 for any, -2 for Elysium, the Sky and Agartha
---@return integer|nil result nil when refused (lua_refused)
function coe5.countTerrainOnPlane(terrain, plane) end

---The next plane in the order the top bar shows them, as the < and > keys step: direction 1 forward, -1 back.
---@param plane integer a plane 0-10
---@param direction integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.planeOrderStep(plane, direction) end

---Moves a unit to the same spot on another plane, as the game's planeshift does. A noSight other than 0 leaves sight as it is.
---@param unit integer a living unit
---@param plane integer a plane 0-10
---@param noSight integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.planeShiftUnit(unit, plane, noSight) end

---Links two squares with a portal number no other portal uses, as startplane's home portal and the Strange Portals do. twoWay 0 makes the second square an exit only.
---@param x1 integer a square's x
---@param y1 integer a square's y
---@param x2 integer a square's x
---@param y2 integer a square's y
---@param twoWay integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.newPortalPair(x1, y1, x2, y2, twoWay) end

---Shows a player every square of a portal number, and returns how many.
---@param player integer a player slot
---@param portal integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.revealPortalExits(player, portal) end

---Sends a monster up from a plane, as Hades sends its Dispossessed Spirits: on each square that rolls under percent, each of the monster's units, by string, has an n percent chance to planeshift to Elysium, and the game places as many again where they land.
---@param plane integer a plane 0-10
---@param percent integer integer
---@param n integer integer
---@param monster string a monster by string
---@return boolean|nil result nil when refused (lua_refused)
function coe5.planeRiseToElysium(plane, percent, n, monster) end

---Moves the map to a plane, as its top bar button does.
---@param plane integer a plane 0-10
---@return boolean|nil result nil when refused (lua_refused)
function coe5.viewPlane(plane) end

---The plane a unit stands on.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitPlane(unit) end

---A unit's fire resistance as a hit reads it: fire immunity counts 100, its fireres is added, and state bit 12 of unitStateFlags takes 50 off.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitFireResistance(unit) end

---A unit's cold resistance as a hit reads it: cold immunity counts 100, and its coldres is added.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitColdResistance(unit) end

---A unit's shock resistance as a hit reads it: shock immunity counts 100, and its shockres is added.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitShockResistance(unit) end

---A unit's poison resistance as a hit reads it: poison immunity counts 100, and its poisonres is added.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitPoisonResistance(unit) end

---A unit's acidres total with its items, as a hit reads it.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitAcidResistance(unit) end

---A unit's morale as a battle reads it, with experience and items; a mindless unit reads 99.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitMorale(unit) end

---Where a unit deploys in battle: 1 the front, 0 the middle, -1 the back, its items and afflictions counted.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitBattleRank(unit) end

---2 for battleslow2, which moves every fourth round, 1 for battleslow, which moves on odd rounds, else 0.
---Named after the .c5m words battleslow and battleslow2.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitBattleSlowness(unit) end

---Whether a unit is fast in battle: battlefast, fast, or Quickness.
---Named after the .c5m word battlefast.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitBattleFast(unit) end

---Whether a unit can't act: asleep, paralysed, petrified, webbed and the like.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitIncapacitated(unit) end

---A unit's move speed by its move flags: 4 fast, 3 normal, 2 slow.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitSpeedClass(unit) end

---2 for a unit with flying and float, 1 for flying, 0 for neither.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitFlightType(unit) end


---Whether a unit's move flags, its items' counted, let it move on water.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitHasWaterMove(unit) end

---Whether a unit is noland, its items counted.
---Named after the .c5m word noland.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitNoLand(unit) end

---Whether a unit's monster has morale 99, the game's mark of a mindless one.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitMindless(unit) end

---The percent of its Hit Points a unit regenerates each turn.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitRegenPercent(unit) end

---How many afflictions a unit has.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitAfflictionCount(unit) end

---Whether a unit's attacks hold a spell.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitHasSpellAttack(unit) end

---Whether a unit counts as undead, as noundead and the game's hits test it.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitIsUndead(unit) end

---Whether a unit counts as a demon, as nodemon and the game's hits test it.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitIsDemon(unit) end

---An ability's sum over a unit, its 8 items, blessings and temporary abilities. add is the game's switch; it passes 0 or 1.
---@param unit integer a living unit
---@param ability integer|string an ability's word or id
---@param add integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitAbilitySum(unit, ability, add) end

---Whether a monster has a ranged weapon or a spell attack, as the game decides it: never for a front-rank monster.
---At the main menu too.
---@param monster integer a monster
---@return boolean|nil result nil when refused (lua_refused)
function coe5.monsterHasRanged(monster) end

---A monster's nametype, the list of names its commanders draw from.
---Named after the .c5m word nametype.
---@param monster integer a monster
---@return integer|nil result nil when refused (lua_refused)
function coe5.monsterNameType(monster) end

---The living units following a commander.
---@param commander integer a commander with a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.commanderLedCount(commander) end

---The action points a commander has left this turn.
---@param commander integer a commander with a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.commanderApLeft(commander) end

---Whether a commander may take an order now, as the AI's turn tests it.
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.commanderMayAct(commander) end

---The largest burnforest in a commander's army.
---@param commander integer a commander with a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.armyBurnValue(commander) end

---An ability's total over a commander and the units following it. across and sources are the game's switches for what is counted.
---@param commander integer a commander with a living unit
---@param ability integer|string an ability's word or id
---@param across integer integer
---@param sources integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.armyAbilityTotal(commander, ability, across, sources) end

---An item's value of an ability, 0 for none.
---@param item integer an item
---@param ability integer|string an ability's word or id
---@return integer|nil result nil when refused (lua_refused)
function coe5.itemAbility(item, ability) end

---The value of an item's nth copy of an ability, from 0.
---@param item integer an item
---@param ability integer|string an ability's word or id
---@param n integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.itemAbilityNth(item, ability, n) end

---A player's commanders; a freeOnly other than 0 counts only those following no one.
---@param player integer a player slot
---@param freeOnly integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.countFreeCommanders(player, freeOnly) end

---Whether a player sees a living unit on a square.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.squareHasSeenUnit(player, x, y) end

---The battle strength of a team's living units on a square, or everyone's with -1. A fort other than 0 adds 10 per ranged unit and the square's fort bonuses.
---@param player integer a player slot, -1 for any
---@param x integer a square's x
---@param y integer a square's y
---@param fort integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.squareSideBattleStrength(player, x, y, fort) end


---What a square costs a commander to enter, 1000 to an action point and 99999 for refused. flag is the game's switch; it passes 0 for a plain route.
---@param commander integer a commander with a living unit
---@param x integer a square's x
---@param y integer a square's y
---@param flag integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.squareMoveCost(commander, x, y, flag) end

---A weapon's range for a unit, its spellrange and rangebonus counted.
---@param unit integer a living unit
---@param attackType integer integer
---@param weapon integer a weapon
---@return integer|nil result nil when refused (lua_refused)
function coe5.weaponRangeFor(unit, attackType, weapon) end

---Whether a square is a loner's first preference, as its wandering steps read it.
---@param unit integer a living unit
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitLovesSquare(unit, x, y) end

---Whether a square is a loner's second preference.
---@param unit integer a living unit
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitLikesSquare(unit, x, y) end

---Whether a loner refuses a square; a square off the map counts.
---@param unit integer a living unit
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitHatesSquare(unit, x, y) end

---Whether a wandering commander stands on a square.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.wandererAt(x, y) end

---Moves one unit alone to a square, as the game's teleportloc does.
---@param unit integer a living unit
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.teleportUnit(unit, x, y) end

---Gives a unit experience; a mindless unit gets none.
---@param unit integer a living unit
---@param xp integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitAddXp(unit, xp) end

---Takes an item from a unit by its number, emptying the slot that holds it, as the AI hands items on (coe5.takeItem
---takes one by name). -1 when the unit doesn't hold it.
---@param unit integer a living unit
---@param item integer an item
---@return integer|nil result nil when refused (lua_refused)
function coe5.takeItemNumber(unit, item) end

---Takes Hit Points off a unit with none of the checks and death effects of a hit, as a meteor's aftermath does.
---@param unit integer a living unit
---@param damage integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.subtractUnitHp(unit, damage) end

---Makes each living unit a player has on a square hold, and returns how many.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.squareHold(player, x, y) end

---Spends the action points of a player's units on a square, or everyone's with -1.
---@param player integer a player slot, -1 for any
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.spendAllApAt(player, x, y) end

---Uses each item of a commander's unit the AI rates worth using.
---@param commander integer a commander with a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.commanderUseItems(commander) end

---Kills a unit with 999 damage, twice, as an overrun does, with every death effect. A unit that comes back from death
---(coe5.get("unitReturns", unit)) reforms with its afflictions, as the game's reform does; cure yours first to
---keep them off it. In a LAN client's turn the kill reaches the host in the turn file; in a battle replay the
---refight's unit dies, and comes back when the replay ends.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.killUnit(unit) end

---Gives a unit an item by string, as the event word newitem does.
---@param unit integer a living unit
---@param item string a string
---@return boolean|nil result nil when refused (lua_refused)
function coe5.giveItem(unit, item) end

---Takes an item from a unit by string, as the event word removeitem does.
---@param unit integer a living unit
---@param item string a string
---@return boolean|nil result nil when refused (lua_refused)
function coe5.takeItem(unit, item) end

---At a percent chance gives a unit a random item, as the event words randitem (type -1) and randmisc (type 7) do.
---@param unit integer a living unit
---@param type integer the item type, as the manual's Magic Item Types
---@param percent integer integer
---@param level integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.giveRandomItem(unit, type, percent, level) end

---A ritual's first value of an effect, 0 for none, as the AI reads its ai words.
---@param ritual integer a ritual
---@param ability integer|string an ability's word or id
---@return integer|nil result nil when refused (lua_refused)
function coe5.ritualEffectFirst(ritual, ability) end

---A ritual's first value of an effect, or your default without one.
---@param ritual integer a ritual
---@param ability integer|string an ability's word or id
---@param default integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.ritualEffectOr(ritual, ability, default) end

---Whether the game's check calls a ritual pointless for a commander: the corpses and resources it needs aren't on the caster's square. It leaves out CoE5SE's raise commands, which the AI's check adds (see AI_RITUAL_CANDIDATE).
---Named after the game's AI debug log line "pointless ritual".
---@param commander integer a commander with a living unit
---@param ritual integer a ritual
---@return boolean|nil result nil when refused (lua_refused)
function coe5.ritualPointless(commander, ritual) end

---Whether a square the commander's player has seen would take the ritual's terrain.
---@param commander integer a commander with a living unit
---@param ritual integer a ritual
---@return boolean|nil result nil when refused (lua_refused)
function coe5.ritualCastableAnywhere(commander, ritual) end

---A player's blessing value of an ability, 0 for none.
---@param player integer a player slot
---@param ability integer|string an ability's word or id
---@return integer|nil result nil when refused (lua_refused)
function coe5.playerBlessingValue(player, ability) end

---What a spell attack of a magic path weighs in a unit's strength.
---@param path integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.magicPathWeight(path) end


---A unit's boost to a magic path: its magic boost and its path boosts.
---@param unit integer a living unit
---@param path integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitPathBonus(unit, path) end

---The highest level of a unit's spell attacks of a magic path, or of any with -1.
---@param unit integer a living unit
---@param path integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitSpellWeaponLevel(unit, path) end

---How many spells of a magic path a unit may memorize.
---@param unit integer a living unit
---@param path integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.memorizeAllowance(unit, path) end

---The spell if the unit has it memorized, else -1.
---@param unit integer a living unit
---@param spell integer a spell
---@return integer|nil result nil when refused (lua_refused)
function coe5.memorizedSlot(unit, spell) end

---A random spell of a magic path and level, -1 for none. path and level -1 for any; flags are the game's.
---@param path integer integer
---@param level integer integer
---@param flags integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.randomSpell(path, level, flags) end

---A random spell a unit doesn't know yet, from up to 1000 draws.
---@param unit integer a living unit
---@param path integer integer
---@param level integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.randomSpellNotKnown(unit, path, level) end

---The spells a unit knows of a magic path and level, -1 for any.
---@param unit integer a living unit
---@param path integer integer
---@param level integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.countKnownSpells(unit, path, level) end

---The spells a unit has memorized of a magic path, -1 for any.
---@param unit integer a living unit
---@param path integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.countMemorizedSpells(unit, path) end

---The memorized spells of a magic path whose weapon has a type, -1 for any.
---@param unit integer a living unit
---@param path integer integer
---@param weaponType integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.countMemorizedOfType(unit, path, weaponType) end

---The spells the game holds of a magic path and level.
---@param path integer integer
---@param level integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.countSpells(path, level) end

---The magic path of a monster's first spell attack, -1 for none; items and boosts don't count.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitFirstMagicPath(unit) end


---A random memorized spell of a magic path, -1 for none.
---@param unit integer a living unit
---@param path integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.randomMemorizedSpell(unit, path) end

---The spell of a unit's first item with combatspell 1, -1 for none.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.itemCastableSpell(unit) end

---Whether the game would call a spell worth casting for a unit now.
---@param unit integer a living unit
---@param spell integer a spell
---@return boolean|nil result nil when refused (lua_refused)
function coe5.spellUseful(unit, spell) end

---Whether a monster attack row's type is one of the five spell weapon types.
---@param attackType integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.isSpellAttack(attackType) end

---A spell by string, -1 for none.
---@param name string a string
---@return integer|nil result nil when refused (lua_refused)
function coe5.findSpell(name) end

---The nth ritual of a name, from 0, -1 for none.
---@param name string a string
---@param n integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.findRitual(name, n) end

---A random ritual of a school and level.
---@param ritpow integer integer
---@param level integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.randomRitual(ritpow, level) end

---Whether a commander may learn a ritual by the ritual's requirements.
---@param commander integer a commander with a living unit
---@param ritual integer a ritual
---@return boolean|nil result nil when refused (lua_refused)
function coe5.ritualMayLearn(commander, ritual) end

---Whether a ritual is in a commander's known list.
---@param commander integer a commander with a living unit
---@param ritual integer a ritual
---@return boolean|nil result nil when refused (lua_refused)
function coe5.commanderKnowsRitual(commander, ritual) end

---Memorizes a spell for a unit, as the game's stats screen does.
---@param unit integer a living unit
---@param spell integer a spell
---@return integer|nil result nil when refused (lua_refused)
function coe5.memorizeSpell(unit, spell) end

---Takes a spell out of a unit's memorized list.
---@param unit integer a living unit
---@param spell integer a spell
---@return integer|nil result nil when refused (lua_refused)
function coe5.unmemorizeSpell(unit, spell) end

---Fills a unit's battle spells the way the game picks them.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.chooseMemorized(unit) end

---Clears a unit's memorized spells, then picks them again.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.repickMemorized(unit) end

---Teaches a unit a random spell it doesn't know of a magic path and level, then picks its battle spells.
---@param unit integer a living unit
---@param path integer integer
---@param level integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.learnRandomSpell(unit, path, level) end

---Teaches a unit the spells its magic paths start it with.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.learnStartSpells(unit) end

---Puts a ritual in a commander's known list at the first free place.
---@param commander integer a commander with a living unit
---@param ritual integer a ritual
---@return integer|nil result nil when refused (lua_refused)
function coe5.commanderLearnRitual(commander, ritual) end

---Teaches a commander every free or starting ritual of a school at exactly that level.
---@param commander integer a commander with a living unit
---@param ritpow integer integer
---@param level integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.learnLevelRituals(commander, ritpow, level) end

---Teaches a commander the rituals its schools start it with, and returns how many.
---On Mac true, since its function there counts nothing.
---Named after the game's debug log line "getstartrituals".
---@param commander integer a commander with a living unit
---@return integer|boolean|nil result nil when refused (lua_refused)
function coe5.getStartRituals(commander) end

---Ends an active ritual effect by caster, type and square, the game's only way to free one.
---@param caster integer integer
---@param type integer the effect's type
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.removeEnchant(caster, type, x, y) end

---Adds a random blessing effect to a player, as Bolster Devotion does.
---@param player integer a player slot
---@param kind integer integer
---@param show integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.bolsterBlessing(player, kind, show) end

---Adds to a player's blessing of an ability, up to cap.
---@param player integer a player slot
---@param ability integer|string an ability's word or id
---@param add integer integer
---@param cap integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.addPlayerBlessing(player, ability, add, cap) end

---Removes every unit on a square, as the event word killsquare does.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.killSquare(x, y) end

---A square's name as messages give it to a player.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return string|nil result nil when refused (lua_refused)
function coe5.squareShownName(player, x, y) end

---A square's income of a resource for a player, in hundredths.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@param resource integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.squareIncome(player, x, y, resource) end

---A square's gold for a player, in hundredths.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.squareGoldIncome(player, x, y) end

---The lifeforce a draining square gives.
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.squareLifeforce(x, y) end

---A player's trade bonus in percent, their team's trade enchantments counted.
---@param player integer a player slot
---@return integer|nil result nil when refused (lua_refused)
function coe5.tradeBonusPercent(player) end

---1 and the terrain shown when another player's illusion covers a square for this player, else 0.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.illusionTerrain(player, x, y) end

---Turns a square's terrain into its ruined form.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.ruinSquare(player, x, y) end

---Digs out a Cave Wall or Moist Cave Wall, and returns 1 when it dug.
---On Mac true, since its function there returns nothing.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return integer|boolean|nil result nil when refused (lua_refused)
function coe5.digTunnelSquare(player, x, y) end

---Changes a square to terrain to when it is from or from2, -1 for any, as the event word alterterrain does.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@param from integer integer
---@param from2 integer integer
---@param to integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.alterTerrain(player, x, y, from, from2, to) end

---Turns a Gateway into a Destroyed Gateway. With a flag other than 0, the squares sharing its portal go too.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@param flag integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.destroyGateway(player, x, y, flag) end

---The Baron's conscription for a player, as the first month of Summer runs it.
---@param player integer a player slot
---@return boolean|nil result nil when refused (lua_refused)
function coe5.conscription(player) end

---A Voice of El player's chance of free units by the seals broken.
---@param player integer a player slot
---@return boolean|nil result nil when refused (lua_refused)
function coe5.priestFreespawn(player) end

---Calls the Inquisition's army for a player.
---@param player integer a player slot
---@return boolean|nil result nil when refused (lua_refused)
function coe5.inquisitionSpawn(player) end

---Makes the game count every player's income again the next time it is read.
---@return boolean|nil result nil when refused (lua_refused)
function coe5.resetIncomeCaches() end

---Marks each portal active or not, as the game does when a portal opens or closes.
---@return integer|nil result nil when refused (lua_refused)
function coe5.fixPortals() end

---Teleports a unit to a random square of a terrain code, as the game's random teleports do; followers other than 0 brings them along. False when 500,000 draws found none.
---On Mac true, since its function there returns nothing.
---@param unit integer a living unit
---@param terrain integer integer
---@param followers integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.randomTeleport(unit, terrain, followers) end

---Horrors come for the units marked by them, at a chance the game multiplies by ten for the most marked.
---@param chance integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.popHorrors(chance) end

---Frees every follower whose commander can't lead it.
---@return boolean|nil result nil when refused (lua_refused)
function coe5.removeBadFollowers() end

---Sends every human player a World News line, as the game's events do.
---@param text string a string
---@return boolean|nil result nil when refused (lua_refused)
function coe5.worldNews(text) end

---Switches a player's auto recruit of a monster on a square on or off.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@param monster integer a monster
---@return integer|nil result nil when refused (lua_refused)
function coe5.toggleAutoRecruit(player, x, y, monster) end

---The living units of a monster on a square, by string ("" for any), owned by fromOwner (-1 for any) become slot 27's, Hades', and the call returns how many.
---@param x integer a square's x
---@param y integer a square's y
---@param monster string a string
---@param fromOwner integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.changeLoyalty(x, y, monster, fromOwner) end

---Moves every active ritual effect on a square to another, as the game does when squares swap, and returns how many.
---@param x1 integer a square's x
---@param y1 integer a square's y
---@param x2 integer a square's x
---@param y2 integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.moveEnchants(x1, y1, x2, y2) end

---Swaps the units on two squares; the squares stay as they are.
---On Mac true, since its function there returns nothing.
---@param x1 integer a square's x
---@param y1 integer a square's y
---@param x2 integer a square's x
---@param y2 integer a square's y
---@return integer|boolean|nil result nil when refused (lua_refused)
function coe5.swapLocations(x1, y1, x2, y2) end

---Moves a square's gems to another square, which leaves the first with none.
---@param x1 integer a square's x
---@param y1 integer a square's y
---@param x2 integer a square's x
---@param y2 integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.moveGems(x1, y1, x2, y2) end

---Whether a player has a living commander; the game removes a player without one.
---@param player integer a player slot
---@return boolean|nil result nil when refused (lua_refused)
function coe5.playerHasCommander(player) end

---Whether a player still owns their home citadel.
---@param player integer a player slot
---@return boolean|nil result nil when refused (lua_refused)
function coe5.playerHasCitadel(player) end

---The first value of an ability in a player's class, 0 for none.
---@param player integer a player slot
---@param ability integer|string an ability's word or id
---@return integer|nil result nil when refused (lua_refused)
function coe5.playerClassAbility(player, ability) end

---Whether a player, or anyone with -1, may own a square.
---@param player integer a player slot, -1 for any
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.canOwnSquare(player, x, y) end

---What world generation scores a square as a player's start, by their class's likesnorth, likessouth and likescoast, then a second number the score's sum keeps.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.startPlacementScore(player, x, y) end

---Whether a square passes a player's likesnorth, likessouth and likescoast when world generation picks their start.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.startLikesFilter(player, x, y) end

---A player's income of a resource: gold, iron, or the rest summed by square.
---@param player integer a player slot
---@param resource integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.playerIncomeByType(player, resource) end

---A player's ironbonus in percent.
---@param player integer a player slot
---@return integer|nil result nil when refused (lua_refused)
function coe5.ironBonusPercent(player) end

---A player's goldbonus in percent.
---@param player integer a player slot
---@return integer|nil result nil when refused (lua_refused)
function coe5.goldBonusPercent(player) end

---An ability's sum over a player's team's living units on a square.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@param ability integer|string an ability's word or id
---@return integer|nil result nil when refused (lua_refused)
function coe5.squareTeamAbilitySum(player, x, y, ability) end

---An ability over the units with Hit Points on a square, a player's or, with -1, everyone's.
---@param player integer a player slot, -1 for any
---@param x integer a square's x
---@param y integer a square's y
---@param ability integer|string an ability's word or id
---@return integer|nil result nil when refused (lua_refused)
function coe5.squareUnitsAbility(player, x, y, ability) end

---The troll forest's spawns on a square, as the game's Troll Forest does.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.trollForestSpawn(x, y) end

---Whether a square is Sky, as the game's routes count Sky squares.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.squareIsSky(x, y) end

---Whether a square's terrain can burn.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.terrainBurnableAt(x, y) end

---The terrain a terrain ruins into, -1 for none.
---@param terrain integer a terrain
---@return integer|nil result nil when refused (lua_refused)
function coe5.terrainRuinForm(terrain) end

---Whether a square counts as south, as south1 and south2 test it.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.squareIsSouth(x, y) end

---Whether a square counts as north, as north1 tests it.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.squareIsNorth(x, y) end

---The squares within 1 of a square whose terrain is water.
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.countWaterNear(x, y) end

---The squares within 1 of a square whose terrain isn't water.
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.countLandNear(x, y) end

---The ground a battle on a square uses where no battle map cell sets one.
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.squareDefaultGround(x, y) end

---Whether a square is coast, as likescoast tests it.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.squareIsCoast(x, y) end

---The temperature the game uses for a square, the heat sources near it counted.
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.squareEffectiveTemperature(x, y) end

---Two squares trade their whole records and the units on them, as the Void's shuffle does. 1 when they swapped.
---On Mac true, since its function there returns nothing.
---@param x1 integer a square's x
---@param y1 integer a square's y
---@param x2 integer a square's x
---@param y2 integer a square's y
---@return integer|boolean|nil result nil when refused (lua_refused)
function coe5.swapSquares(x1, y1, x2, y2) end

---Moves every living unit on a square to another.
---@param x1 integer a square's x
---@param y1 integer a square's y
---@param x2 integer a square's x
---@param y2 integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.teleportSquare(x1, y1, x2, y2) end

---Whether a unit may stand on land, as the drown check tests it.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitMayBeOnLand(unit) end

---A unit's magic resistance total.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitMagicResistance(unit) end

---Whether a unit counts as huge for a weapon's huge.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitIsHuge(unit) end

---Whether a unit is Lifeless on its unit stats screen.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitLifeless(unit) end

---Whether a unit can be primed: an animal with magic resistance below 6.
---Named after the .c5m word primable.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitPrimable(unit) end

---Takes an ability off a unit's first matching temporary slot, true when it did.
---@param unit integer a living unit
---@param ability integer|string an ability's word or id
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitRemoveTempAbility(unit, ability) end

---Frees the followers of a commander whose unit is dead.
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.checkFollowerLinks(commander) end

---A commander burns the forest on its square, as the map's order does.
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.orderBurnForest(commander) end

---A commander chops the forest on its square, as the map's order does.
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.orderChopForest(commander) end

---A commander hunts slaves on its square, as the map's order does.
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.orderSlaveHunt(commander) end

---A commander eats the village on its square, as the map's order does.
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.orderEatVillage(commander) end

---A commander uses the special site on a square, as a human's commander does.
---@param x integer a square's x
---@param y integer a square's y
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.useSpecialSite(x, y, commander) end

---Whether an army the AI fears stands within 3 squares of a square.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@param strength integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.aiThreatNear(player, x, y, strength) end

---How many other commanders of a team head for a square's surroundings.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@param commander integer a commander with a living unit
---@param range integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiCountAllyTargetsNear(player, x, y, commander, range) end

---Refreshes what an AI player remembers of each seen square's battle strength.
---@param player integer a player slot
---@return boolean|nil result nil when refused (lua_refused)
function coe5.aiUpdateBattleStrengthMemory(player) end


---Makes n of a player's units on a square guards, which never leave it.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@param n integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.aiMakeGuardsAt(player, x, y, n) end

---Leaves one guard on each seen square the AI player holds with an army, as its turn does after its moves.
---@param player integer a player slot
---@return boolean|nil result nil when refused (lua_refused)
function coe5.aiMakeDefenders(player) end

---The commanders, melee units and ranged units of a player on a square, as the AI's recruiting counts them.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiCountTroops(player, x, y) end

---Whether a unit stays put for the AI: a guard, or an aihold unit on a fort.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.aiUnitStays(unit) end

---The nearest square matching a terrain code over the whole map, as the AI looks for a ritual's terrain: 1 and its x and y.
---@param terrain integer integer
---@param x integer a square's x
---@param y integer a square's y
---@param player integer a player slot, -1 for any
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiNearestTerrain(terrain, x, y, player) end

---A player's commander with the most of an ability.
---@param player integer a player slot
---@param ability integer|string an ability's word or id
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiRitualMaster(player, ability) end

---The ship capacity a commander's army has left.
---@param commander integer a commander with a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiCommanderBoatPlaces(commander) end

---Drops one follower from a commander's ship crew, true when it did.
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.aiShipDropCrew(commander) end

---Links one free ship on a commander's square, true when it did.
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.aiShipTakeBoat(commander) end

---Links one free sailor on a commander's square, true when it did.
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.aiShipTakeCrew(commander) end

---The ship capacity of a commander and its followers.
---@param commander integer a commander with a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiGroupBoatTotal(commander) end

---The ship capacity on a player's ports, then their ships.
---@param player integer a player slot
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiPortsBoatTotals(player) end

---A player's ships on a square.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiShipsAt(player, x, y) end

---A player's commanders planning a ritual, by the ritual's name.
---@param player integer a player slot
---@param ritual integer a ritual
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiCountPlanners(player, ritual) end

---What a square scores for a commander's planned ritual.
---@param commander integer a commander with a living unit
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiPlanSquareBonus(commander, x, y) end

---Hands each item on a square to the player's unit there that values it most.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.aiRedistributeItems(player, x, y) end

---The AI's ship handling for one commander.
---@param commander integer a commander with a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiShips(commander) end

---The AI's conquest step for a commander: the best score, -999 for none, then its square.
---Windows only for now.
---@param commander integer a commander with a living unit
---@param strength integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiConquerStep(commander, strength) end

---The AI's exploration step for a commander: the best score, then its square.
---Windows only for now.
---@param commander integer a commander with a living unit
---@param strength integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiExploreStep(commander, strength) end

---The AI's far exploration step for a commander: the best score, then its square.
---@param commander integer a commander with a living unit
---@param strength integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiFarExploreStep(commander, strength) end

---The AI's rally step for a commander: a score, then its square.
---Windows only for now.
---@param commander integer a commander with a living unit
---@param strength integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiRallyStep(commander, strength) end

---The AI's move to a friendly square for a commander: a score, then its square.
---Windows only for now.
---@param commander integer a commander with a living unit
---@param strength integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.aiFriendlyMoveStep(commander, strength) end

---The unit covering a battle cell, -1 for none.
---Only during a battle or on an open test field.
---@param x integer a battle cell's x
---@param y integer a battle cell's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleUnitAt(x, y) end

---The summed fear a player's unit feels on a battle cell.
---Only during a battle or on an open test field.
---@param x integer a battle cell's x
---@param y integer a battle cell's y
---@param player integer a player slot
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleFearAt(x, y, player) end

---The best localleadership of a player's unit near a battle cell.
---Only during a battle or on an open test field.
---@param x integer a battle cell's x
---@param y integer a battle cell's y
---@param player integer a player slot
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleLocalLeadership(x, y, player) end

---A team's living mobile units on the field of a rank range.
---Only during a battle or on an open test field.
---@param player integer a player slot
---@param lowRank integer integer
---@param highRank integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleCountRank(player, lowRank, highRank) end

---The one team left on the field, as that team's first player.
---Only during a battle or on an open test field.
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleWinner() end

---Whether two units on the field can reach each other past a siege wall.
---Only during a battle or on an open test field.
---@param unit integer a living unit
---@param other integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.battleLineOfSight(unit, other) end

---Whether a unit is in range of another, the line test counted.
---Only during a battle or on an open test field.
---@param unit integer a living unit
---@param other integer a living unit
---@param range integer integer
---@param flags integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.battleInRange(unit, other, range, flags) end

---Whether an enemy touches a unit on the field.
---Only during a battle or on an open test field.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitInContact(unit) end

---Whether a unit fits on a battle cell.
---Only during a battle or on an open test field.
---@param unit integer a living unit
---@param x integer a battle cell's x
---@param y integer a battle cell's y
---@param mode integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.battleCanStand(unit, x, y, mode) end

---The squared distance between two units' footprints, then the distance in cells.
---Only during a battle or on an open test field.
---@param unit integer a living unit
---@param other integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleUnitDistance(unit, other) end

---A unit's nearest enemy, then its squared distance.
---Only during a battle or on an open test field.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleNearestEnemy(unit) end

---The lowest and highest column holding a player's enemies.
---Only during a battle or on an open test field.
---@param player integer a player slot
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleEnemyExtent(player) end

---An ability of the battle cell under a unit, the unit's and a wall's or parapet's there added up; 0 for a unit on no cell.
---Only during a battle or on an open test field.
---@param unit integer a living unit
---@param ability integer|string an ability's word or id
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleSquareAbility(unit, ability) end

---The Wall Cover of the cell under a unit.
---Only during a battle or on an open test field.
---@param unit integer a living unit
---@param kind integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleWallCover(unit, kind) end

---The rank a unit advances by this round.
---Only during a battle or on an open test field.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleAdvanceRank(unit) end

---A random living unit of a monster on a square, by string, a player's or, with -1, anyone's. -1 for none.
---@param player integer a player slot, -1 for any
---@param x integer a square's x
---@param y integer a square's y
---@param monster string a string
---@return integer|nil result nil when refused (lua_refused)
function coe5.findUnitAt(player, x, y, monster) end

---A random living unit on a square holding an item, by string. -1 for none.
---@param player integer a player slot, -1 for any
---@param x integer a square's x
---@param y integer a square's y
---@param item string a string
---@return integer|nil result nil when refused (lua_refused)
function coe5.findItemHolderAt(player, x, y, item) end

---A random square of a plane and terrain code, as the game's rituals and world generation pick one: 1, then its x and y.
---@param plane integer a plane 0-10, -1 for any, -2 for Elysium, the Sky and Agartha
---@param terrain integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.randomSquareOfTerrain(plane, terrain) end

---The ground the game falls back on for a square, by its 3x3.
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.groundFromNeighbours(x, y) end

---Gives a unit one of the game's random afflictions.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.unitRandomAffliction(unit) end

---Gives a unit an affliction by its bit in unitStateFlags.
---Takes the game's raw bits; coe5.giveAffliction gives an affliction a mod declared, by its name.
---@param unit integer a living unit
---@param bit integer a mask of bits
---@return boolean|nil result nil when refused (lua_refused)
function coe5.addAffliction(unit, bit) end

---Shows a player every square of a terrain code on a plane, and returns how many.
---@param player integer a player slot
---@param plane integer a plane 0-10, -1 for any, -2 for Elysium, the Sky and Agartha
---@param terrain integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.revealTerrainOnPlane(player, plane, terrain) end

---A square of a terrain code near a square, the nearest more often than not: 1, then its x and y; 0 for none.
---On Mac the plane is 0-10, since its function there has no -1 or -2.
---@param plane integer a plane 0-10, -1 for any, -2 for Elysium, the Sky and Agartha
---@param terrain integer integer
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.nearTerrainSquare(plane, terrain, x, y) end

---Puts a random rare terrain on a square, as world generation does.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.putRareSquare(x, y) end

---A grove's monthly step on a square: its spawns and its spread.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.groveReversion(x, y) end

---Shows a player's team every square within a radius, on that square's plane.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@param radius integer a number
---@param value integer integer
---@param flag integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.revealDisc(player, x, y, radius, value, flag) end

---Sets what a player, or every player with -1, has seen within a radius.
---@param player integer a player slot, -1 for any
---@param x integer a square's x
---@param y integer a square's y
---@param radius integer a number
---@param level integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.setSeenDisc(player, x, y, radius, level) end

---Plays one of the game's sounds, change raising or lowering it from its volume.
---@param sample integer integer
---@param change integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.playSample(sample, change) end

---A random name from one of the game's name lists, as nametype draws one.
---@param nameType integer a name list 0-61
---@return string|nil result nil when refused (lua_refused)
function coe5.autoName(nameType) end

---Shows the game's Yes/Cancel box, and returns whether the player picked Yes.
---@param text string a string
---@return boolean|nil result nil when refused (lua_refused)
function coe5.confirm(text) end

---Whether any of a square's four sides is water, as world generation places river sources.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.squareSideWater(x, y) end

---Whether a square is flooded or its terrain is water, as a plane's flood reads it.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.squareIsWet(x, y) end

---The wet squares in the 3x3 around and on a square.
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.wetNeighbours(x, y) end

---The Manhattan distance to the nearest other square of any of four terrains.
---@param x integer a square's x
---@param y integer a square's y
---@param terrain1 integer integer
---@param terrain2 integer integer
---@param terrain3 integer integer
---@param terrain4 integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.nearestTerrainDistance(x, y, terrain1, terrain2, terrain3, terrain4) end

---The squares of a plane matching a terrain code.
---@param plane integer a plane 0-10
---@param terrain integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.countTerrainCode(plane, terrain) end

---Opens the game's location picker, as a ritual that asks where, and returns the square the player picked.
---@param title string the picker's title
---@param player integer a player slot
---@param x integer the square's x it starts on
---@param y integer the square's y it starts on
---@param multiplane integer 1 keeps the player on the plane in view, 0 lets them pick any plane they have seen
---@param range? integer how far from the first square they may pick; leave it out for any distance
---@return integer|nil x nil when the player cancelled or the call was refused (lua_refused)
---@return integer|nil y
function coe5.chooseLocation(title, player, x, y, multiplane, range) end

---Opens the map's income popup for a player and a resource, as a top bar button does.
---Windows only for now.
---@param player integer a player slot
---@param resource integer a resource 0-18
---@return boolean|nil result nil when refused (lua_refused)
function coe5.openIncomeDetail(player, resource) end

---Opens a plane's info screen, as a right click on its top bar button does.
---@param plane integer a plane 0-10
---@return boolean|nil result nil when refused (lua_refused)
function coe5.openPlaneInfo(plane) end

---Opens the Square terrain info window for a player and a square. mode is the game's switch.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@param mode integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.openSquareInfo(player, x, y, mode) end

---Opens the Ceremonies screen for a commander.
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.openCeremonies(commander) end

---Opens the Rituals screen for a commander.
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.openRitualScreen(commander) end

---Opens the library screen for a commander's unit.
---@param unit integer a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.openLibrary(unit) end

---Opens the Chop & Burn window for the selected commander.
---@return boolean|nil result nil when refused (lua_refused)
function coe5.openChopBurn() end

---Opens the Hunt for Slaves screen for the selected commander.
---@return boolean|nil result nil when refused (lua_refused)
function coe5.openSlaveHunt() end

---Opens the army screen's menu for a commander.
---@param commander integer a commander with a living unit
---@return boolean|nil result nil when refused (lua_refused)
function coe5.openArmyMenu(commander) end

---Opens a weapon's info window.
---Windows only for now.
---@param weapon integer a weapon
---@return boolean|nil result nil when refused (lua_refused)
function coe5.openWeaponInfo(weapon) end

---Opens an item's info screen.
---@param item integer an item
---@return boolean|nil result nil when refused (lua_refused)
function coe5.openItemInfo(item) end

---Opens the Select Monster screen on the monsters whose value of an ability lies in a range, and returns the monster picked.
---Windows only for now.
---@param low integer integer
---@param high integer integer
---@param ability integer|string an ability's word or id
---@return integer|nil result nil when refused (lua_refused)
function coe5.monsterPicker(low, high, ability) end

---Opens the game's class picker, and returns the class picked: -1 for its Random line. It needs no
---game running, so it works from the main menu.
---@return integer|nil result nil when refused (lua_refused)
function coe5.classPicker() end

---How many of a player's commanders have action points left, as End Turn's warning counts them.
---@param player integer a player slot
---@return integer|nil result nil when refused (lua_refused)
function coe5.countCommandersWithAp(player) end

---Marks the selected commander done for this turn, as Space does.
---@return boolean|nil result nil when refused (lua_refused)
function coe5.selectedCommanderDone() end

---Selects the next commander and centres the map on it.
---@return boolean|nil result nil when refused (lua_refused)
function coe5.selectNextCommander() end

---Selects the previous commander and centres the map on it.
---Windows only for now.
---@return boolean|nil result nil when refused (lua_refused)
function coe5.selectPrevCommander() end

---The living units of a monster on a square, by string, a player's or, with -1, anyone's.
---@param player integer a player slot, -1 for any
---@param x integer a square's x
---@param y integer a square's y
---@param monster string a string
---@return integer|nil result nil when refused (lua_refused)
function coe5.countUnitsAt(player, x, y, monster) end

---The living units of a monster on a plane, by string.
---@param player integer a player slot, -1 for any
---@param plane integer a plane 0-10, -1 for any, -2 for Elysium, the Sky and Agartha
---@param monster string a string
---@return integer|nil result nil when refused (lua_refused)
function coe5.countUnits(player, plane, monster) end

---The nearest living unit on a plane to a square, in squares across plus squares down, whose value of an ability lies from `low` to `high`, both included: the highest of its and its items' values, its blessings and temporary abilities counted. -1 for none.
---@param plane integer a plane 0-10, -1 for any, -2 for Elysium, the Sky and Agartha
---@param x integer a square's x
---@param y integer a square's y
---@param ability integer|string an ability's word or id
---@param low integer integer
---@param high integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.nearestUnitAbilityIs(plane, x, y, ability, low, high) end

---Whether a square holds an army, as a player's checks read it.
---@param player integer a player slot, -1 for any
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.armyAtForPlayer(player, x, y) end

---A commander's full name as messages give it: "Jazz the Dwarf Queen".
---@param commander integer a commander with a living unit
---@return string|nil result nil when refused (lua_refused)
function coe5.commanderFullName(commander) end

---The flags of every ritual a player knows, OR'd together.
---@param player integer a player slot
---@return integer|nil result nil when refused (lua_refused)
function coe5.playerRitualFlags(player) end

---How many player slots hold a controller.
---@param controller integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.countPlayersWithController(controller) end

---A class by string, -1 for none.
---@param name string a string
---@return integer|nil result nil when refused (lua_refused)
function coe5.findClass(name) end

---A weapon's value of an extra trait, 0 for none.
---@param weapon integer a weapon
---@param id integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.weaponExtra(weapon, id) end

---Whether a unit's immunities stop an affliction, by its bits in unitStateFlags.
---@param unit integer a living unit
---@param bits integer a mask of bits
---@return boolean|nil result nil when refused (lua_refused)
function coe5.afflictionImmune(unit, bits) end

---The monster a Soulless made from a unit becomes.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.soullessForm(unit) end

---Gives a unit a benefit, by its bits, as a weapon's special benefit does.
---Takes the game's raw bits; coe5.giveAffliction gives an affliction a mod declared, by its name.
---@param unit integer a living unit
---@param bits integer a mask of bits
---@return boolean|nil result nil when refused (lua_refused)
function coe5.addBenefit(unit, bits) end

---A player's class may send one of its heroes, as the game's turn does.
---@param player integer a player slot
---@return boolean|nil result nil when refused (lua_refused)
function coe5.heroArrival(player) end

---Clears what lies on a square, its hold included. flag is the game's switch; world generation passes 0.
---@param x integer a square's x
---@param y integer a square's y
---@param flag integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.cleanSquare(x, y, flag) end

---Gives a square its starting gems by its terrain.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.initSquareGems(x, y) end

---Gives a square its starting corpses by its terrain.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.initSquareCorpses(x, y) end

---Runs the month's hatreds: each hates unit may kill a hated unit of its team.
---@return boolean|nil result nil when refused (lua_refused)
function coe5.runHatreds() end

---Runs the month's desert clouds.
---Windows only for now.
---@return boolean|nil result nil when refused (lua_refused)
function coe5.sandstormCloudUpdate() end

---Starts one of the game's music tunes.
---Named after the game's debug log line "starttune".
---A top-level coe5 call runs the game's tune function directly; coe5.ui.screenMusic is CoE5SE's UI layer, which starts the menu screens' music.
---@param tune integer integer
---@param force integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.startTune(tune, force) end

---Shows the game's number entry box, and returns the number entered.
---A top-level coe5 call runs the game's number box directly; coe5.ui.nameBox is CoE5SE's UI layer, a box CoE5SE draws as the game does.
---@param title string a string
---@param prompt string a string
---@return integer|nil result nil when refused (lua_refused)
function coe5.numberBox(title, prompt) end

---The unit or terrain object on a battle cell's second layer: a tree, a boulder, or a second unit.
---Only during a battle or on an open test field.
---@param x integer a battle cell's x
---@param y integer a battle cell's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleObjectAt(x, y) end

---The cheapest step for a unit toward a battle cell: a result, then the step.
---Only during a battle or on an open test field.
---@param unit integer a living unit
---@param x integer a battle cell's x
---@param y integer a battle cell's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.battlePathfind(unit, x, y) end

---A free neighbouring cell touching an enemy: a result, then the step.
---Only during a battle or on an open test field.
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.battleKillerMove(unit) end

---Banishes a unit from the battle, as dmgtype 23 does a banishsurv monster: it leaves the field, and after the
---battle it is flung to `plane`, whatever its monster and items name, on square x, y or a random fitting one, keeping its owner,
---and fights any monsters there; a commander goes alone. With no plane, the game's choice: the plane its monster or items name,
---else Hades, or on Windows the Inferno for a demon. A BANISH_DESTINATION wrap has the last word. A unit summoned in the
---battle is destroyed. Only during a battle. nil for a plane past the last, a square with no plane, off the map or not on the
---plane named, or a unit off the field (a tree or a house too).
---@param unit integer a living unit on the field
---@param plane? integer a plane from 0 to the last, or none for the game's choice
---@param x? integer a square of that plane, or none for a random fitting one
---@param y? integer
---@return true|nil result nil when refused (lua_refused)
function coe5.battleBanishUnit(unit, plane, x, y) end

---Puts a new unit of `monster` on the battlefield for `player`, as a weapon named "Summon <monster>" (dmgtype 18) puts its
---summons there: on the battle's square, at cell x, y or the nearest cell it fits. Without `stays` it is a battle summons, and
---leaves when the battle ends; with it, it stays with the player's units after the battle, and may then have another number.
---Only during a battle. The monster's summon count (battleSummons) is the script's to keep. nil for an unused monster, a player
---on neither side, a cell off the field, a full unit table, or a field with no room for it.
---@param monster integer a monster
---@param player integer|string a player slot on one of the battle's sides, or a faction's name
---@param x integer a battle cell's x, below battleWidth
---@param y integer a battle cell's y, 0-19
---@param stays? boolean true for a unit that stays after the battle
---@return integer|nil unit nil when refused (lua_refused)
function coe5.battleSummon(monster, player, x, y, stays) end

---Whether a plane can be reached from a square, as the game's routes ask.
---@param plane integer a plane 0-10
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.planeIsReachable(plane, x, y) end

---Ends the other players' active effects on a square that can't stand against a player taking it.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.destroyEnchantsAt(player, x, y) end

---A square's owner, if their class allows it, gets Storm Elementals and Storm Clouds there.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.stormSpawn(x, y) end

---A square's owner, if their class allows it, has a chance of Snakes, Moose and the like there.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.wildSpawn(x, y) end

---Maenads and Satyr Revelers come to a square.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.revelsSpawn(x, y) end

---A square's owner, if their class allows it, has a chance of a Forest Shepherd there.
---@param x integer a square's x
---@param y integer a square's y
---@return boolean|nil result nil when refused (lua_refused)
function coe5.forestShepherdSpawn(x, y) end

---Writes a square's owner, as the game's conquest does; a mod that wants the rest of a conquest also calls what it needs.
---@param x integer a square's x
---@param y integer a square's y
---@param player integer a player slot
---@return boolean|nil result nil when refused (lua_refused)
function coe5.setSquareOwner(x, y, player) end

---A random living unit on a square with every bit of a state mask, the game's search for an afflicted unit. monster -1 for any. -1 for none.
---@param player integer a player slot, -1 for any
---@param x integer a square's x
---@param y integer a square's y
---@param monster integer integer
---@param stateMask integer a mask of bits
---@param resistanceMask integer a mask of bits
---@return integer|nil result nil when refused (lua_refused)
function coe5.findAfflictedUnitAt(player, x, y, monster, stateMask, resistanceMask) end

---The nth value of an ability in a class, from 0; 0 for none.
---@param class integer a class
---@param ability integer|string an ability's word or id
---@param n integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.classAbilityNth(class, ability, n) end

---A side's units on a square, as the game sizes a battlefield.
---@param player integer a player slot
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.countSideUnits(player, x, y) end

---The one slot with a human controller, -1 for none or several, worked out now.
---@return integer|nil result nil when refused (lua_refused)
function coe5.soleHumanPlayer() end

---The game's pick of a random enemy of a player, as the AI and a ritual cast draw one: a human or AI slot
---not on the player's team, from the game's random numbers.
---@param player integer a player slot
---@return integer|nil enemy a player slot, -1 for none found in 1000 tries; nil when refused (lua_refused: no_game, bad_player)
function coe5.aiRandomEnemy(player) end

---Whether square (x, y) counts for a terrain code, as the game checks a ritual's terrain
---(ritualTerrainAt): -1 any terrain, a terrain type (terrainName takes it) by its name, so another
---type of the same name counts, and a group (-2 to -1500, as ritualTerrain and the newlocplanes,
---randnewloc and nearby words give them) by the game's test.
---@param code integer
---@param x integer
---@param y integer
---@return boolean|nil counts false for a square off the world; nil for a code the game can't check (below -1500, or 2000 and up)
function coe5.terrainMatch(code, x, y) end

---Keeps only some of the game's debug trace lines (log.txt in the game folder on Windows, standard
---output on Mac, at "traceLevel" 1 to 3): from now on the game writes a line only when its format text, after its leading spaces,
---starts with one of the words, and drops the rest before it formats them. The words match the
---game's format text, not the finished line: "bt%d:" keeps the battle lines that read "bt3: ...".
---nil or an empty list keeps every line again. CoE5SE keeps one list for every mod: the last call
---wins. The list does nothing in a network game.
---@param words? string[] each a word a kept line starts with ("ailog", "removing player")
---@return boolean set false when refused (lua_refused reason=not_a_list, not_a_string, empty_word, leading_space, bad_character, out_of_memory; site_unavailable when the game's trace functions aren't the code CoE5SE knows, logged as trace_filter_installed)
function coe5.keepTraceLines(words) end

-- ---- Planes, factions, world events, and custom commands --------------------------------------

---Declares a plane of the mod's, at script load only. At each new world CoE5SE adds it beside the
---game planes (between the Void and Elysium2, plane 10), or, when the world's width (mapDepth, at
---most 1000 squares) is used up, under a smaller game plane (Celestial to the Void); a plane with no
---room is left out (plane_skipped, and a line on the CoE5SE page). It is filled with one terrain, then
---built by like's recipe (CoE5SE's copy of the game's random map generator for that plane) and
---generate, then its pieces are stamped. After the game places its rare special terrain, the recipe
---places its own, the zones' swaps run, and a plane with no portal to another plane gets an entrance:
---a gateway on a flat square of Elysium and one on a land square of the plane, joined both ways,
---never on a wall, a portal or a piece. With no such land square after 100 draws, the plane's gateway
---goes on any square of it an army can enter, water included; with none, it gets no gateway
---(plane_entrance_missing). When the game places independents, populate's plane places them (or only
---its units list), each zone with a units list only from it and each populate = false zone not at
---all, then swap applies. Its plane number is handed out at each new world and load, from 11, in the
---order the planes were declared: every mod's newplane blocks first, then the scripts' planes, in the
---order the scripts run; coe5.planeIndex reads it. Up to 8 planes, all mods together. The plane gets a
---button after the game's on the map's top bar, the map editor's plane bar and a chooseloc
---ritual's target location screen (its icon, greyed until the player has seen a square of it), and on
---the Game Over map view, never greyed. A save holding one loads only where every plane it holds is
---declared again; a game without CoE5SE refuses it. A .c5se's newplane block declares the same plane
---without a script (the reference guide's "New Planes").
---@param plane coe5.Plane
---@return boolean declared false when refused (lua_refused reason=not_at_load, bad_name, name_taken, bad_size, bad_terrain, unknown_plane, no_recipe, bad_swap, too_many_swaps, recipes_not_loaded, bad_icon, icon_not_found, icon_wrong_case, icon_link, path_too_long, too_many_planes, plane_table_not_moved, hooks_not_installed)
function coe5.addPlane(plane) end

---Gives a monster a shape on a mod's plane, at script load only: a unit of that monster turns into
---shapeMonster there, as elysiumshape..voidshape do on the game planes, and it turns back by
---shapeMonster's shapechanging commands. A monster with no shape on a mod's plane keeps its form
---there, as a monster with no agarthashape keeps its form on Agartha. A name finds a monster as the
---game's commands do, with an optional offset ("2:Hound" for the third); the names resolve at each new
---world and load, and one that doesn't is refused then (lua_refused reason=no_such_plane,
---no_such_monster, no_such_shape). A later call for the same monster and plane wins. A monster
---block's planeshape line in a .c5se declares the same.
---@param monster string
---@param planeName string a mod's plane (coe5.addPlane), from any mod
---@param shapeMonster? string nil for none
---@return boolean declared false when refused (lua_refused reason=not_at_load, bad_name, bad_plane, game_plane for a game plane by its full or short name ("Celestial Plane", "Celestial"), out_of_memory)
function coe5.setPlaneShape(monster, planeName, shapeMonster) end

---The plane number of the plane with that name in this world (a mod's plane or a game plane), or
---nil when the world has none; planeName and squarePlane use the same numbers.
---@param name string
---@return integer|nil index
function coe5.planeIndex(name) end

---The player number of the faction with that name in this game: a mod's faction (a .c5se newfaction block),
---or a vanilla independent faction by its name ("Independents", "Special Monsters", "Horrors", "Hades",
---"Inferno", "Celestial", "Empire"); nil when there is none. Every call that takes a player takes it, and
---takes the name itself too (coe5.get("playerName", "Ash Court"), coe5.spawnUnit("Ash Court", m, x, y)), as .c5m
---lines take the quoted name; a name no faction has is an error there. A mod's faction gets its number at each game's start, so ask in the game.
---@param name string
---@return integer|nil player
function coe5.factionPlayer(name) end

---Declares an independent faction of the mod's, at script load only, as a .c5se newfaction block does:
---the same player number, rules, save rules and checks. Each field goes through its block word's check,
---and a field refused refuses the whole call (lua_refused field= the field), so the faction is declared
---as asked or not at all. A plane name (home, spawnPlane, monumentEvent's, caravan's, units' and populateTerrain's
---plane) is looked up once every script has declared its planes: one no game plane and no enabled mod's plane
---has is logged then (lua_refused reason=unknown_plane field= the field) and shown in the main menu's "CoE5SE: Mod
---Error" pop-up, and the faction does without it. It stays for the session; up to 16 factions, all mods together.
---@param faction coe5.Faction
---@return boolean declared false when refused (lua_refused reason=not_at_load, no_name, name_too_long, unknown_field, bad_field, bad_number, bad_text, missing_value, too_long, bad_color, a block word's reason, game_plane, terrain_needs_owncitadel, name_taken, too_many_factions, faction_slots_not_ready, hooks_not_installed)
function coe5.addFaction(faction) end

---Lays a colour over the map while a plane is in view, as the game lays red at 0.15 over Elysium and
---the Sky while a Blood Sun is up: one rectangle over the whole screen, over the ground and under the
---units. Any plane, the game's included, by its number or name (a game plane's full or short name,
---"Elysium2", or a mod's plane in this world). It wins over the game's tint, and over the Blood
---Sun copy a plane like "Sky" gets, until coe5.clearPlaneTint, a load or the game's end; it isn't
---saved, so set it again from the mod's events. coe5.get("planeTint", plane) reads it back. Not in
---network games.
---@param plane integer|string
---@param r number 0 to 1
---@param g number 0 to 1
---@param b number 0 to 1
---@param strength number 0 to 1: how much of the colour covers the map (the Blood Sun's 0.15)
---@return boolean set false when refused (lua_refused reason=no_such_plane, out_of_range, hooks_not_installed)
function coe5.setPlaneTint(plane, r, g, b, strength) end

---Drops your tint on a plane: from the game's next tint update (each map screen, after each ritual)
---the plane has the game's tint again.
---@param plane integer|string
---@return boolean cleared false when refused (lua_refused reason=no_such_plane)
function coe5.clearPlaneTint(plane) end

---Switches off the game's dice-rolled events for the game being played: each listed event's roll is
---still thrown, and a roll that would start it does nothing, as a failed roll does. Every event not
---listed is on again, so {} turns them all back on. An event already under way runs to its end. The
---set is saved with the game and comes back with each load; a new game starts with every event on, so
---call it at "Creating World" (a NEW_WORLD_CREATING_WORLD wrap) or later. Names, in the game's
---monthly order: "hadesshift", "monument" (Monument of Hades), "foretelling", "taintedmagic",
---"elementalanomaly", "heatwave", "bumperharvest", "fungusyear", "meteor", "insanefisherman",
---"drought", "strangemist", "eviltimes", "infernogate", "primalgate", "goldmine" (a gold mine
---invaded), "youngdvala", "merchant", then "hero" (A Hero), "unexpectedhero", "weaponmaster" (Old
---Weapon Master; each of the three may bring a class hero) and "antspawn" (the Giant Ant Hills'
---monthly spawns). In a LAN game the host's set counts: a call on a client is refused
---(client_write_lost).
---@param names string[]
---@return boolean set false when a name was refused (lua_refused reason=not_a_table, unknown_event, not_a_string; the other names still go in) or an event's gate couldn't be switched on this game build (code_toggle_refused in coe5mod.log)
function coe5.setRandomEventsOff(names) end

---The events coe5.setRandomEventsOff switched off for this game, by name, in the game's order.
---@return string[]
function coe5.randomEventsOff() end

---The game's world events, in the game's monthly order: the long seasons, Hades, the Monument of
---Hades, the foretelling, tainted magic, the Elemental Anomaly, the heat wave, Bumper Harvest, Fungus
---Year, the meteor, the Insane Fisherman and his cube, drought, strange mist, Evil Times and its
---Bandit King, brigand lair and pirate haven, the Inferno and Primal gates, Cardinal Intervenes, the
---sixth and seventh seals, the Celestial gate, the four riders and the Apocalypse, Blood Sun, the
---Rephaites, the fairy circle, a gold mine attacked, Young Dvala, the merchant and the palace moved.
---Each entry is `{name = , label = , queued = , needs = }`: the name coe5.queueGameEvent takes, the
---event's headline (or what it does), true while it waits for the end of the turn, and what the
---game still looks for on the map before it fires the event.
---@return {name: string, label: string, queued: boolean, needs: string[]}[] events
function coe5.gameEvents() end

---Fires event `name` of coe5.gameEvents() at the end of this turn, in the game's month update,
---with the event's tests passed: its dice, turn, season and month, and its "already" tests.
---The game still finds the event's place and unit as it always does (an Ancient Forest's Green
---Enchantress for the Primal gate), and with none on the map the event doesn't happen. A gold mine's
---attack and Young Dvala happen at the first square the game finds. Cardinal Intervenes and the
---sixth and seventh seals need fewer than 7 seals broken, and the sixth and seventh leave the count
---at 6 and 7. Two events queued for one month that change the same code two ways, or that both set
---or break seals, wait: the second fires the month after. Only the host fires events in a LAN game.
---@param name string an event's name, from coe5.gameEvents()
---@return boolean queued false when refused: outside a game (reason=no_game), no such event (unknown_event), one already queued (already_queued), a seal breaker at 7 seals (seals_done), 16 events already queued (queue_full), a patch this game build can't take (game_code_differs, patch_unavailable), each logged as lua_refused; on a LAN client, client_write_lost
function coe5.queueGameEvent(name) end

---Draws a mod's terrain (a number past 355) on the world map with a game terrain's looks, at script
---load only, as a terrainlooks block does. `ground`: the game terrain whose ground it takes (its
---place in the blend order and its edges); `picture` ("./file.tga", sides powers of two) is drawn
---instead of that ground's picture on this terrain's squares only. `decor`: the game terrain whose
---decorations it takes, exactly; `sprites` = {[40] = "./pool.tga"} draws tree.trs sprite 40 of them
---as the picture (a .tga or .png), on this terrain only. Terrains by number or name.
---@param looks {terrain: integer|string, ground: integer|string|nil, picture: string|nil, decor: integer|string|nil, sprites: table<integer, string>|nil}
---The ground picture also shows in battle, on the cells of the battle square's ground.
---@return boolean set false when refused (lua_refused reason=not_at_load, no_terrain, unknown_terrain, not_a_mod_terrain, not_a_game_terrain, no_looks, no_decor, no_ground, looks_taken, bad_sprite, too_many_sprites, not_a_tga, size_not_power_of_two, icon_not_found, hook_not_installed)
function coe5.setTerrainLooks(looks) end

---Declares a sprite pack of the running mod, at script load only, as a newspritepack and its spr lines do: `pictures`
---is a "./file.tga" (or .png) or "./folder/" (every .tga and .png in it, in name order), or a list of them,
---at most 16 pictures. Each picture is drawn from a copy fitted to a 64 or 128 square, bottom-centred. A
---pack is its own mod's: another mod's lines can't name it.
---@param name string
---@param pictures string|string[]
---@return boolean set false when refused (lua_refused reason=not_at_load, no_name, no_pictures, bad_list, pack_taken, icon_not_found, icon_wrong_case, bad_icon, bad_picture, folder_not_found, folder_wrong_case, bad_folder, empty_folder, too_many_pictures, out_of_memory)
function coe5.setSpritePack(name, pictures) end

---Draws the running mod's sprite packs on a terrain (by number or name), at script load only, as a
---selectterr block's terrainscatter and terraincenter lines do: every line or none. Each line is the game's own
---scatter of `count` pieces (1 to 100) of `size` percent of a square (1 to 200), `sway` percent of Forest's (0 to
---500, 100 when left out) and `shade` percent (0 to 100, 50 when left out); `clearcenter` (0 to 100 percent of
---a square) keeps a circle in the middle clear, as the Brigand Lair's trees do, and with a count of 1
---stands one piece in the middle. `center = true` stands one still piece of `size` in the middle, as the game's
---towers stand, with no count, sway, shade or clearcenter. A line with no `condition` is a piece, drawn in every
---state; one with `condition` "snow", "cold" or "snowcold" is a version of the nearest piece above it (in this
---call or an earlier one on the terrain), drawn in its place on those squares, and a count of 0 there drops the
---piece. Snow on a cold square takes the piece's snowcold versions, else its cold ones, else its snow ones, else
---the piece. A terrain with lines draws only them, in place of the game's own pieces; the latest mod's lines on a
---terrain draw it. A mod has at most 64 lines on a terrain, and they draw at most 100 pieces on a square of each
---state, counting each piece's versions there in its place and a centred piece as one.
---@param terrain integer|string
---@param lines {pack: string, count: integer|nil, size: integer, sway: integer|nil, shade: integer|nil, clearcenter: integer|nil, center: boolean|nil, condition: "snow"|"cold"|"snowcold"|nil}[]
---@return boolean set false when refused (lua_refused reason=not_at_load, no_terrain, unknown_terrain, bad_list, too_many_lines, too_many_pieces, no_pack, bad_numbers, bad_center_size, zero_count, bad_condition, no_piece_above, hook_not_installed, out_of_memory)
function coe5.setTerrainScatter(terrain, lines) end

---Gives a terrain (by number or name) what the game ties to Forest, at script load only, as a selectterr block's
---words do: every word or none. `heartclaim`: Heart of the Forest claims it as it claims Forest. `wildspawn`: Call
---of the Wild's animals appear on it as on Forest. `hotconvert`: heat above 65 turns it into that terrain (by
---number or name), at Forest's chance, from turn 1 on. `fullforest`: the forest flag, heartclaim, wildspawn and
---hotconvert to Savanna; a `hotconvert` with it gives the terrain heat makes. A word left out, or false, is off;
---a flag word is on for a terrain any mod names, and the latest mod's
---hotconvert on a terrain is the one it takes.
---@param terrain integer|string
---@param words {fullforest: boolean|nil, heartclaim: boolean|nil, wildspawn: boolean|nil, hotconvert: integer|string|false|nil}
---@return boolean set false when refused (lua_refused reason=not_at_load, no_terrain, unknown_terrain, bad_list, hook_not_installed, out_of_memory)
function coe5.setTerrainForest(terrain, words) end

---Adds a battle map file of the game's format (made in the game's battle map editor, --batmapeditor),
---at script load only, as a battlemap block does. Battles on a square of one of `terrains` pick from
---the maps naming that terrain; with none, battles that would use one of the game's maps in `forts`
---("citadel", "village", "villageupgr": its file name in the game's data/batmaps) pick from the maps
---naming it and the game's, which `replace` = true leaves out. `weight` (1 to 1000, default 10,
---the game's map's) is the map's share of its pool; every word of `when` must hold: "siege",
---"open", "ambush", "sea", "land", "cold", "warm", "summer", "autumn", "winter", "spring"; `planes`
---names the planes it serves (any); `noscatter` = true keeps the map free of random trees and rocks;
---`grounds` = {[8] = "./ash.tga"} draws the map's cells of that ground (0 to 28) with the picture (a .tga,
---sides powers of two).
---The same square always gets the same map. Terrains by number or name.
---@param map {file: string, terrains: (integer|string)[]|nil, forts: string[]|nil, replace: boolean|nil, weight: integer|nil, when: string[]|nil, planes: string[]|nil, noscatter: boolean|nil, grounds: table<integer, string>|nil}
---@return boolean added false when refused (lua_refused reason=not_at_load, no_file, bad_file, file_not_found, file_wrong_case, file_link, path_too_long, not_a_battle_map, bad_weight, bad_list, bad_terrain, unknown_terrain, too_many_terrains, unknown_battle_map, too_many_forts, unknown_condition, no_name, too_many_planes, bad_ground, not_a_tga, size_not_power_of_two, icon_not_found, icon_wrong_case, no_place, replace_without_fort, conditions_never_met, hook_not_installed, out_of_memory)
function coe5.addBattleMap(map) end

---Gives a terrain its random plants and rocks in battle, in place of the game's trees, bushes,
---rocks and rock pillars, at script load only, as a battlescatter block does: `objects` lists
---{monster name, chance in 1000 cells} pairs, tried in order, one object a cell at most; none, or no
---`objects`, for no scatter. The same square always gets the same scatter. One per terrain across
---all mods. Terrain by number or name.
---@param scatter {terrain: integer|string, objects: {[1]: string, [2]: integer}[]|nil}
---@return boolean set false when refused (lua_refused reason=not_at_load, no_terrain, unknown_terrain, bad_terrain, bad_list, no_name, bad_chance, too_many_objects, scatter_taken, hook_not_installed, out_of_memory)
function coe5.setBattleScatter(scatter) end

---Makes `monster` a vehicle, at script load only, one entry per monster: the same mod's second call
---replaces its first, another mod's is refused. `monster` (a number or a name) is required; each other
---field turns on one part. `battlemap` ("./deck.coem", made in the game's battle map editor) is its deck:
---the map's objects, placed on the battle square's field (sea, land or Sky) where the game builds a
---ship's deck, front toward the middle, mirrored for the defenders. The deck moves as the game's ships do
---in the advance and gather rounds, riders along, and the vehicle's side boards it, front rank first.
---`heart`, a monster on the deck: the vehicle stays off the field and survives the fight only if a heart
---of its deck lives; without one it fights as a unit. Deck objects need standable to be stood on. A
---monster with the game's `ship` can't have a deck (the game builds its own). `terrain` (a name or a
---number): the vehicle carries a site of that terrain, and its owner recruits from it wherever the
---vehicle stands, an owned square or not, as from an owned square of that terrain: the terrain's recruit
---lists and the game's tests. On a square that already is a place for its owner to recruit, the one
---place lists both: the square's offers and the site's. Each vehicle buys once a month wherever it
---goes, apart from the square's recruiting, and an auto-recruit order made for its site follows it.
---The square under it keeps its terrain. `land` = true and `air` = true make a monster with the
---game's `transport` ability a carrier there (the .c5m words transportland and transportair do the
---same; either one turns a medium on): a human's army holding it and up to `transport` 1 x 1 riders (one
---place each) moves with its action points and its move flags. With `land` it carries on land squares,
---its riders on the ground; with `air` it carries in the Sky and over water while it flies, and its
---riders fly and float with it and stay in the Sky while it does. A bare `coe5.addTransport{monster}`
---carries nowhere. The sea stays the game's: `transport`, `shipmove` and the month's drown check. An army
---past its places moves as the game moves it, and so does every AI's; the AI doesn't recruit from
---carried sites. When the carrier dies its riders fare as the game's rules say: they drown at sea at
---the month's end, fall from the Sky unless they float, and nothing happens on land. The .c5m words
---transportmap, transportheart and transportterrain give a monster `battlemap`, `heart` and `terrain`
---without a script; a call for the same monster takes the ones it leaves out, and its win.
---@param transport {monster: integer|string, land: boolean|nil, air: boolean|nil, battlemap: string|nil, heart: integer|string|nil, terrain: integer|string|nil}
---@return boolean added false when refused (lua_refused reason=not_at_load, no_monster, bad_monster, unknown_monster, bad_battlemap, bad_heart, bad_terrain, bad_land, bad_air, bad_text, monster_taken, too_many, unknown_terrain, unknown_heart, heart_without_battlemap, game_ship, bad_file, file_not_found, file_wrong_case, file_link, path_too_long, not_a_battle_map, no_deck_objects, deck_too_tall, heart_not_on_deck, hook_not_installed, out_of_memory)
function coe5.addTransport(transport) end

---Adds a mod command, as a DLL's addModCommand does, and only from your <ModName>_commands.lua while
---it loads. A line whose first word is `name`, exactly, capitals included, goes to `handler` instead
---of to the game, which would stop with "mod command '<name>' did nothing": `args` is the rest of the
---line with its leading spaces and tabs trimmed, `mod` the name of the mod whose line it is (nil if
---CoE5SE can't name it), and `level` the mod pass's level: 1 mod info (every pass reads each mod at
---level 1 first), 2 content (startup, leaving the Mods screen, loading a game, a game's end; where
---most commands act), 4 art files (spr, sample; read for a mod's checksum when a game is saved or
---loaded). Level 3, the event passes, never reaches a mod command, and neither does a line inside an
---event. The handler normally runs from the startup pass on, before <ModName>.lua has loaded, so it
---uses only what the commands file defines until then. The command works in a mod's <ModName>.c5se
---too. A prefix such as "mymod_" keeps the name from clashing with another mod's (duplicate). In a
---LAN game the handler runs on the host for a content mod, and at level 2 on a client for a content or personal mod.
---@param name string under 48 characters, no space or tab, not one of the game's commands, ignoring case
---@param handler fun(args: string, mod: string|nil, level: integer)
---@return boolean added false when refused (lua_refused reason=not_in_commands_file, no_name, bad_name, duplicate, game_command, site_unavailable, out_of_memory, script_stopped)
function coe5.addModCommand(name, handler) end

---Alters one of the game's .c5m commands, from your <ModName>_commands.lua only, while it loads.
---`fn` gets every line whose first word is `command`, in every enabled mod's .c5m and .c5se and in a
---map's event file, before CoE5SE or the game reads it, and returns nil to leave the line as it is, a
---new line to read in its place, or false (or "") to drop it. A new line is read as if the mod had
---written it, CoE5SE's words included, and goes on to the next alterCommand of the same command: they
---run in load order. A content line comes at the level the game reads its command (t.level: 2, or 1 for
---icon, description, coeversion, modversion and modprio, or 4 for spr1, spr2, spr and sample); a line
---in an event only in an event pass (level 3), with t.event true, t.player, t.squareEvent, and t.x and
---t.y, the event's square (-1 in a squareevent). t.command is the command in lowercase (a trigger
---without its + or -, which is t.sign, 1 or -1), t.args the rest of the line, t.line the whole line,
---t.mod the mod's name (nil for a map's events). In a LAN game it runs for a content mod only.
---@param command string one of the game's commands, any case; a trigger with or without its sign
---@param fn fun(t: {command: string, args: string, line: string, mod: string|nil, level: integer, event: boolean|nil, sign: integer|nil, player: integer|nil, squareEvent: boolean|nil, x: integer|nil, y: integer|nil}): string|false|nil
---@return boolean added false when refused (lua_refused reason=not_in_commands_file, not_a_game_command, out_of_memory, script_stopped); a result that is not a string, false or nil is lua_refused reason=bad_result, once, and a new line over 49,999 bytes or with a line break is lua_refused reason=line_too_long or two_lines, and the line is read as it was
function coe5.alterCommand(command, fn) end

---Answers one of the game's event triggers in place of the game, from your <ModName>_commands.lua
---only, while it loads. `fn` runs where the game would check the trigger: in a playerevent once, on
---the event's square (not at all when it has none, as the game skips the line), and in a squareevent
---once for each square still marked, with t.x and t.y; after randloc, setloc or the like it is the
---event's square each time, as for the game. It returns whether the trigger holds there, as written
---with +; CoE5SE turns the answer around for a - line, as the game does. True keeps the square (the
---event goes on), false drops it (a playerevent fails, and a squareevent fails when no square is
---left), and nil leaves the square to the next alterEventTrigger or to the game. For hascom, hascomhere, hasunit,
---hasunithere and hasaffunithere, a unit holds too and becomes the unit the actions after act on
---(killunit, promote, makeaff, comname, gainxp, newitem), as the unit the game finds does. The fields
---of t are coe5.alterCommand's. In a LAN game it runs for a content mod only, on the host, where
---events run.
---@param trigger string one of the game's triggers, with or without its + or -
---@param fn fun(t: {command: string, args: string, line: string, mod: string|nil, level: integer, event: boolean, sign: integer, player: integer, squareEvent: boolean, x: integer, y: integer}): boolean|integer|nil
---@return boolean added false when refused (lua_refused reason=not_in_commands_file, not_a_game_command, not_a_trigger, out_of_memory, script_stopped); a result of another type, or a unit past 98998, is lua_refused reason=bad_result, once
function coe5.alterEventTrigger(trigger, fn) end

---Does one of the game's event actions in place of the game, from your <ModName>_commands.lua only,
---while it loads. `fn` runs where the game would act: in a playerevent once, on the event's square
---(not at all when it has none), and in a squareevent once for each marked square, with t.x and t.y
---(the event's square each time after randloc and the like). It returns true when it did the
---action there, so the game's action skips that square; anything else leaves the square to the next
---alterEventAction or to the game. The fields of t are coe5.alterCommand's. In a LAN game it runs for a content
---mod only, on the host, where events run.
---@param action string one of the game's event actions
---@param fn fun(t: {command: string, args: string, line: string, mod: string|nil, level: integer, event: boolean, player: integer, squareEvent: boolean, x: integer, y: integer}): boolean|nil
---@return boolean added false when refused (lua_refused reason=not_in_commands_file, not_a_game_command, not_an_action, out_of_memory, script_stopped)
function coe5.alterEventAction(action, fn) end

---Runs `fn` after the game reads each content line whose first word is `command`, from your
---<ModName>_commands.lua only, while it loads, to set right what the line did with coe5.set. t has
---coe5.alterCommand's fields and the records the line's block is on, -1 for none: t.monster, t.weapon,
---t.item, t.class, t.terrain, t.ritual (coe5.set("monster", t.monster, "hp", 30)). Its result is unused.
---In a LAN game it runs for a content mod only.
---@param command string one of the game's content commands
---@param fn fun(t: {command: string, args: string, line: string, mod: string|nil, level: integer, monster: integer, weapon: integer, item: integer, class: integer, terrain: integer, ritual: integer})
---@return boolean added false when refused (lua_refused reason=not_in_commands_file, not_a_game_command, not_a_content_command, out_of_memory, script_stopped)
function coe5.alterCommandAfter(command, fn) end

-- ---- Per-mod data files (INI) -----------------------------------------------------------------

---Names a subfolder of your mod's area for your data files. The area is coe5se_data\<your mod's
---name> in the game's user folder (%APPDATA%\coe5 on Windows, ~/.coe5 on Mac); without this call
---your files go in the area itself. The subfolder is always inside the area: no mod reaches
---another's. The area holds 10 MB in all, its subfolders included, or up to 100 MB when your
---mod's .c5se has a maxdata line (maxdata 50).
---@param name string up to 64 characters, no / \ : * ? " < > |, not starting or ending with a space or a dot
---@return boolean named false for a name that isn't one plain folder name (logged as lua_refused)
function coe5.dataFolder(name) end

---Reads data file `name`.ini from your data folder, as t[section][key] = value. Values are strings:
---use tonumber for numbers. Keys before the first [section] line are in t[""]. A line it can't read
---(a [ with no ], a line with no =) is skipped, and the rest still reads (logged as bad_lines).
---@param name string the file's name without .ini: the same rules as coe5.dataFolder's
---@return table<string, table<string, string>>|nil t nil when there is no such file, the file is over 1 MB, or the name breaks a rule (logged as lua_refused); in a LAN game nil for a content mod (lan_local_data)
function coe5.readIni(name) end

---Writes t[section][key] = value as data file `name`.ini in your data folder, in place of the old
---file, sections and keys in sorted order. Section names and keys are strings, each section a table,
---and values strings or numbers: a number or other key, a NUL byte in a section name or key, or a
---value of another type (problem=not_text) is refused. A section name, key or value over 1,024 bytes
---is refused (problem=too_long), and so is one that wouldn't read back the same: a line break, ] in a
---section name or key, an empty key, = in a key, a key starting with [ ; or #, a NUL byte in a value,
---or a space or tab at either end. One refused section name, key or value fails the whole write:
---nothing is written and the old file stays as it was. Split a long list over several keys.
---@param name string the file's name without .ini
---@param t table<string, table<string, string|number>>
---@return boolean written false when something is refused, the file would pass 1 MB, your mod's area would pass 10 MB or its .c5se's maxdata (data_quota), or it can't be written (logged as lua_refused)
function coe5.writeIni(name, t) end

---The names of the data files in your data folder, without .ini, sorted.
---@return string[] names empty in a LAN game for a content mod (lan_local_data)
function coe5.listIni() end

---Deletes data file `name`.ini from your data folder.
---@param name string
---@return boolean deleted false when there was no such file, or it can't be deleted (logged as lua_refused)
function coe5.deleteIni(name) end

---Shows one battle with no game, from the main menu: a commander of `monster` against `count` units of
---`enemy` (the independents), on a grass field with no fort, at the battle screen's default speed.
---With `spell` (a spell's number, -1 for none) the commander knows and has memorized only that spell.
---With `standIn`, any weapon, the spell casts with that weapon's values in place of its
---weapon's, for this battle only. Returns when the battle ends; the units are gone afterwards.
---It also takes coe5.openTestField's table in place of its arguments, for mixed groups.
---With an editor field's table, false and a reason when the fight is refused: coe5.fightTestField's editor field reasons.
---@param monster integer a monster's index (coe5.findMonster)
---@param spell integer|nil -1 or nil for none
---@param enemy integer a monster's index
---@param count? integer 1 or more; 40 when left out
---@param standIn? integer a weapon your mod made
---@overload fun(field: {commander?: integer, spell?: integer, standIn?: integer, friends?: {[1]: integer, [2]?: integer}[], enemies?: {[1]: integer, [2]?: integer}[], terrain?: integer, allowMultipleUnique?: boolean, emptyEditor?: boolean}): boolean, string|nil
---@return boolean shown false when refused (lua_refused): in_game, bad_monster, bad_enemy, bad_count, bad_spell, no_spell_weapon, bad_weapon, no_free_slot; with a table, also coe5.openTestField's table reasons
---@return string|nil reason with an editor field's table, the reason when the fight was refused
function coe5.testBattle(monster, spell, enemy, count, standIn) end

---Adds a line to the unit card's traits, after vanilla and CoE5SE's (Polymorph Immunity and the rest), on
---each unit `visible` says true for. Call it while your script loads.
---@param line {label: string, text?: string, icon?: integer|string, visible: fun(unit: integer, monster: integer): boolean}
---label up to 63 characters; icon a game icon number (0 to 667) or a picture in your mod's folder ("./icon.tga",
---32 x 32), 211 (Magic Shield) when left out; visible runs each time the card draws, on every machine of a LAN game that draws it.
---@return integer|false id false when refused (lua_refused): no_label, label_too_long, no_visible, bad_icon (plus findModImage's), too_many_pictures, out_of_memory
function coe5.addTraitLine(line) end

---Gives a unit an affliction your mod declared with newaffliction "<name>", as makeaff "<name>" does. The unit's
---affres "<name>" (its monster's and items') may stop it: 100 or more always does, below that one roll of the game's
---dice. A unit holding it already keeps it, with its first turn. In a battle replay the refight's unit holds it
---until the replay ends, as the game's afflictions in a refight.
---Takes a mod's affliction by its name; coe5.addAffliction and coe5.addBenefit take the game's raw bits.
---@param unit integer a unit
---@param name string the affliction's name, case ignored
---@return boolean|nil holds true when the unit holds it after, false when its affres stopped it, nil when refused (lua_refused): no_unit, unknown_affliction, out_of_memory
function coe5.giveAffliction(unit, name) end

---Cures one of your mod's afflictions on a unit, as cureoneaff "<name>" does. In a battle replay the refight's unit
---lacks it until the replay ends.
---@param unit integer a unit
---@param name string the affliction's name
---@return boolean|nil cured true when the unit held it, false when not, nil when refused (lua_refused): no_unit, unknown_affliction, out_of_memory
function coe5.cureAffliction(unit, name) end

---Whether a unit holds one of the mods' afflictions.
---@param unit integer a unit
---@param name string the affliction's name
---@return boolean|nil holds nil when refused (lua_refused): no_unit, unknown_affliction, out_of_memory
function coe5.hasAffliction(unit, name) end

---The names of the mods' afflictions a unit holds, in the order the mods declared them.
---@param unit integer a unit
---@return string[]|nil names nil when refused (lua_refused): no_unit
function coe5.unitAfflictions(unit) end

---The living units holding one of the mods' afflictions, lowest index first; no walk of the unit table.
---@param name string the affliction's name
---@return integer[]|nil units nil when refused (lua_refused): unknown_affliction, out_of_memory
function coe5.afflictedUnits(name) end

---The turn (coe5.turn()) a unit was given one of the mods' afflictions.
---@param unit integer a unit
---@param name string the affliction's name
---@return integer|nil turn nil when the unit doesn't hold it, or when refused (lua_refused): no_unit, unknown_affliction, out_of_memory
function coe5.afflictionTurn(unit, name) end

---The number a script keeps with a unit's affliction (coe5.setAfflictionValue), 0 until one is set.
---@param unit integer a unit
---@param name string the affliction's name
---@return integer|nil value nil when the unit doesn't hold it, or when refused (lua_refused): no_unit, unknown_affliction, out_of_memory
function coe5.afflictionValue(unit, name) end

---Sets the turn a unit's affliction counts as given. In a battle replay it lasts until the replay ends.
---@param unit integer a unit
---@param name string the affliction's name
---@param turn integer a turn
---@return boolean|nil set false when the unit doesn't hold it, nil when refused (lua_refused): no_unit, unknown_affliction, bad_value, out_of_memory
function coe5.setAfflictionTurn(unit, name, turn) end

---Keeps a number with a unit's affliction, such as the player whose monster gave it. It goes with the unit into
---saves, LAN turns and battle replays. In a battle replay it lasts until the replay ends.
---@param unit integer a unit
---@param name string the affliction's name
---@param value integer any 32-bit number
---@return boolean|nil set false when the unit doesn't hold it, nil when refused (lua_refused): no_unit, unknown_affliction, bad_value, out_of_memory
function coe5.setAfflictionValue(unit, name, value) end

---Clears the game's afflictions by their bits in unitStateFlags, as the game's cures clear them; the afflictions
---a unit's items give stay with the items. Any bit may be cleared, the game's states included: clearing bit 17
---keeps a unit summoned in a battle after the fight, and bit 42 a split copy.
---@param unit integer a unit
---@param bits integer a mask of bits
---@return boolean|nil cleared true, nil when refused (lua_refused): no_unit
function coe5.removeAffliction(unit, bits) end

---What one of your mod's afflictions does to a unit holding it: fn runs at each read of the site's stat for such a
---unit, as the game's afflictions change a stat inside the function that works it out. fn(unit, value, ...)
---gets the answer so far and the site's other arguments, and returns the new answer, or nil to keep it. For
---UNIT_INCAPACITATED return true or false; for DOATTACK and BATTLE_ADVANCE_UNIT value is true, and false skips the
---attack or the move. Sites: UNIT_DAMAGE_BONUS (Strength), UNIT_MORALE, UNIT_MR, UNIT_BATTLE_RANK, UNIT_MAX_HP,
---UNIT_PATH_BONUS, UNIT_BLINDNESS, UNIT_INCAPACITATED, UNIT_FIRE_RESISTANCE, UNIT_ARMOR, UNIT_RESISTANCES,
---UNIT_FLAGS, UNIT_BATTLE_STRENGTH, UNIT_STATE_FLAGS, DOATTACK, BATTLE_ADVANCE_UNIT, ATTACK_INIT, and UNIT_ABILITY, which is
---no site: fn(unit, value, ability) gives the unit an amount of an ability, value what the afflictions gave it so far (0 at
---first), and the game takes it in every total of the ability as one more item holding it: added where the game adds an
---ability's values, the larger kept where it takes the largest. No temporary ability slot is used. A unit holding none runs no
---Lua. Inside fn, coe5.afflictionValue, coe5.afflictionTurn and coe5.hasAffliction of `unit` always answer.
---Effects run inside every coe5.wrap of the site, in the order the mods declared the afflictions; nothing is kept
---between reads. A name no newaffliction declares yet waits for its mod. A shield, fast or teleport bit an effect adds on
---UNIT_RESISTANCES or UNIT_FLAGS reaches the battle as a wrap's does.
---@param name string the affliction's name, case ignored
---@param site coe5.Site|"UNIT_ABILITY" one of the sites above, or UNIT_ABILITY
---@param fn fun(unit: integer, value: integer|boolean, ...: integer): integer|boolean|nil
---@return boolean|nil added true, nil when refused (lua_refused): unknown_site, bad_value, too_many_afflictions, site_unavailable, out_of_memory
function coe5.afflictionEffect(name, site, fn) end

---The words of one of your mod's afflictions on the unit stats screen, worked out for each unit holding it: fn(unit, value)
---returns the line's label in place of the affliction's name, and the text its click shows in place of descr.
---value is the unit's coe5.afflictionValue. A label that isn't a string keeps the name and descr; a text that isn't
---a string keeps descr. The line keeps the affliction's icon. fn runs each time the card draws, on every machine
---of a LAN game that draws it. One function an affliction: another mod's is refused while the first's runs.
---A name no newaffliction declares yet waits for its mod.
---@param name string the affliction's name, case ignored
---@param fn fun(unit: integer, value: integer): string|nil, string|nil label up to 63 characters, text up to 511 (cut beyond)
---@return boolean|nil added true, nil when refused (lua_refused): bad_value, card_line_taken, too_many_afflictions, out_of_memory
function coe5.afflictionCardLine(name, fn) end

---One record of the game's spell table, mods' spells included: its name, path, level, the weapon that is
---its effect, and the damage value the weapon is used with.
---@param spell integer
---@return {name: string, path: integer, level: integer, weapon: integer, dmg: integer}|nil info nil past the table or for an empty slot
function coe5.spellInfo(spell) end

---Which mod made a loaded item, spell or magic path, and which mod last changed it, from the mod lines the game read.
---kind is created_by_mod, modified_by_mod, baseline (the game's, untouched) or unknown (a change came from where
---CoE5SE didn't see it). historyComplete is always false.
---@param kind "item"|"spell"|"spellschool"
---@param id integer an id in that table
---@return {id: integer, kind: string, modded: boolean, observed: boolean, definedBy: string|nil, lastModifiedBy: string|nil, historyComplete: false, scope: string}|nil origin nil when refused (lua_refused): game_code_differs, bad_content
---@return string|nil reason the reason, with that nil
function coe5.loadedContentOrigin(kind, id) end

---Opens a test field from the main menu, with no game: a commander of `monster` and `count` units of `enemy`
---(the independents) deployed on a grass field, as coe5.testBattle has them, and left standing. Draw it a
---frame at a time with coe5.drawTestField, attack with coe5.fireTestField, fight with coe5.fightTestField,
---and coe5.closeTestField when done. For mixed groups on both sides, pass one table instead:
---coe5.openTestField{commander = wizard, friends = {{archer, 3}}, enemies = {{spearman, 5}, {troll, 1}}}, each
---group a monster's index and a count (1 when left out). Friends follow the commander. No count is capped, and the
---enemies need at least one unit; spell and standIn are optional fields. The units come back as the second value,
---and coe5.closeTestField takes every one of them off.
---Any of terrain, allowMultipleUnique, emptyEditor or minimumWidth in the table opens an editor field, which the
---calls after coe5.closeTestField edit and read. It stands on terrain, a number from coe5.testFieldTerrains() (0, Plain,
---when left out). With emptyEditor true it has no commander (leave commander out, and spell and standIn too), either
---side may be empty, and the units' commander is -1. Without allowMultipleUnique it holds one unit of a unique monster
---at most, and none while one lives anywhere (unique_count, unique_exists). An editor field's refusal gives its reason
---as the second value. On Windows, minimumWidth is an integer from 32 to 1536. Native setup may make the field wider.
---Read the resulting width with coe5.testFieldState().width. Omit minimumWidth for the native default.
---On Windows, verified planar terrain profiles temporarily borrow the local plane and tiles in the main menu.
---A changed required neighbour can return planar_neighbour_changed; an occupied staging area can refuse the request.
---@overload fun(field: {commander?: integer, spell?: integer, standIn?: integer, friends?: {[1]: integer, [2]?: integer}[], enemies?: {[1]: integer, [2]?: integer}[], terrain?: integer, allowMultipleUnique?: boolean, emptyEditor?: boolean, minimumWidth?: integer}): boolean, {emptyEditor: boolean|nil, commander: integer, friends: integer[], enemies: integer[]}|string|nil
---@param monster integer
---@param spell integer|nil
---@param enemy integer
---@param count? integer
---@param standIn? integer a weapon your mod made
---@return boolean opened false when refused (lua_refused): coe5.testBattle's reasons, or already_open; with a table, also bad_friend, bad_list (a side or a group that isn't a table), no_enemies; on an editor field also game_code_differs, bad_terrain, unsupported_terrain, bad_allow_multiple_unique, bad_empty_editor, empty_editor_commander, empty_editor_primary_attack, cleanup_unverified, temporary_square_busy, bad_unit_table, unique_count, unique_exists, lifecycle_unavailable, terrain_init_failed, context_changed
---@return {emptyEditor: boolean|nil, commander: integer, friends: integer[], enemies: integer[]}|string|nil unitsOrReason the units made in group order; on an editor field, the reason when refused
function coe5.openTestField(monster, spell, enemy, count, standIn) end

---Adds units to one side of the open test field, each group a monster's index and a count, as
---coe5.openTestField's table takes them. The game's deploy places them, so the units already on the field
---keep their cells. On an editor field neither side follows a commander, and the whole request is checked before a unit
---is made.
---@param side "friends"|"enemies"
---@param groups {[1]: integer, [2]?: integer}[]
---@return integer[]|nil units the units made, nil when refused (lua_refused): not_open, bad_side, bad_friend, bad_enemy, bad_list, bad_count, no_free_slot (the game's unit table is full), out_of_memory; on an editor field also game_code_differs, not_owner, in_fight, final_read_only, context_changed, cleanup_unverified, unique_count, unique_exists, bad_unit_table
---@return string|nil reason on an editor field, the reason when refused
function coe5.addTestFieldUnits(side, groups) end

---Moves a unit on the open test field to battle cell (x, y), its token's lowest x and y there (its bottom-left corner on screen), to place a group exactly
---where an area attack lands. coe5.get("unitBattleX", unit) and "unitBattleY" read where a unit stands.
---@param unit integer
---@param x integer below battleWidth
---@param y integer 0 to 19
---@return boolean moved false when refused (lua_refused): not_open, not_on_field, off_field (the unit doesn't fit there), blocked (another unit, a tree or a boulder covers a cell of it); on an editor field also game_code_differs, not_owner, in_fight, final_read_only, bad_unit, bad_field, dead_unit, bad_monster
---@return string|nil reason on an editor field, the reason when refused
function coe5.moveTestFieldUnit(unit, x, y) end

---One frame of the battle screen on the open test field, its speed arrows working. BATTLE_DRAW and
---BATTLE_WAIT run in it.
function coe5.drawTestField() end

---The test field's commander makes one attack, as in a battle, then every projectile flies to its end: its
---first weapon, or its spell cast as an item casts it. The weapon reaches across the field for that attack.
---Units it hits take the damage.
---@return false|nil refused false on an editor field when refused (lua_refused): game_code_differs, not_open, not_owner, in_fight, final_read_only, no_primary_unit (an empty field has no commander)
---@return string|nil reason the reason, with that false
function coe5.fireTestField() end

---The battle on the test field, as the game fights one; returns when it ends. On an editor field, the field as the battle
---left it is kept for coe5.testFieldFinalState(), and the field can only be read until it closes.
---@return false|nil refused false on an editor field when refused (lua_refused): game_code_differs, not_open, not_owner, in_fight, final_read_only, cleanup_unverified, bad_field, bad_unit_table, out_of_memory, not_on_field, ambiguous_token, dead_unit, bad_monster, missing_team_a, missing_team_b (no living unit on a side)
---@return string|nil reason the reason, with that false
function coe5.fightTestField() end

---Ends the test field's running battle (coe5.fightTestField) as the battle screen's skip to the end does:
---nothing more drawn, no delay, the battle over. Call it from a BATTLE_WAIT wrap during the battle.
function coe5.stopTestFight() end

---Closes the test field: its units gone, and everything it borrowed put back.
---On an editor field only the units it made go, wherever they stand, and it returns true, or false and a reason.
---@return boolean|nil closed on an editor field: true, or false when refused (lua_refused): not_owner, cleanup_unverified (a unit of the field's still lives; the next editor field is refused until a close succeeds)
---@return string|nil reason the reason, with that false
function coe5.closeTestField() end

---The terrains a test field opens on, as loaded now: a list of terrain numbers among 0 Plain, 2 Forest, 12 Mountains,
---107 Swamp and 117 Desert.
---@return integer[] terrains
function coe5.testFieldTerrains() end

---The Windows stock contexts that can be verified on open. Does not load a field.
---Reports field bounds, minimumWidthVersion, context API versions and loaded terrain rows.
---@return table|nil info
---@return string|nil reason
function coe5.testFieldEnvironmentInfo() end

---Read the owner's explicit stock test-field context and native template selection.
---Windows 5.43. Reports the current plane, dimensions, siege/tower/sea flags,
---observed stock template name and whether a replacement was selected.
---@return table|nil state
---@return string|nil reason
function coe5.testFieldEnvironmentState() end

---Read a loaded terrain's native name, flag bits and complete fort parts. On Windows with matching
---game code. The low byte of fortParts is the native battle-template selector. available reports
---the owned-field terrain policy and required template checks. Opening can still refuse unsafe
---world state. When available, plane is the plane used by the owned field and nativeTemplate is
---the expected stock template, if it has one. The read makes no changes and needs no open field.
---@param id integer
---@return {id: integer, name: string, flags: integer, fortParts: integer, available: boolean, reason?: string, plane?: integer, nativeTemplate?: string}|nil info
---@return string|nil reason bad_terrain or game_code_differs if no record was returned
function coe5.testFieldTerrainInfo(id) end

---Verified stock scenery definitions for Windows 5.43, without opening a field.
---Version 0 means unavailable. Each type has an exact loaded monster ID, owner, layer, dimensions, standable flag and role.
---@return {version: integer, types: table[]} catalog
function coe5.testFieldSceneryTypes() end

---Read authored scenery in the owner's preparation field.
---Each object includes its generation, exact monster ID, anchor, owner, health and standable flag.
---@return {version: integer, session: integer, objects: table[]}|nil state
---@return string|nil reason
function coe5.testFieldSceneryState() end

---Place an exact verified scenery definition at a zero-based battlefield cell.
---The object layer must be free. Standable scenery may share a verified army footprint.
---@param monster integer
---@param x integer
---@param y integer
---@return integer|nil unit
---@return string|nil reason
function coe5.addTestFieldScenery(monster, x, y) end

---Move an authored scenery object while keeping its native generation and health.
---@param unit integer
---@param session integer
---@param generation integer
---@param x integer
---@param y integer
---@return true|nil moved
---@return string|nil reason
function coe5.moveTestFieldScenery(unit, session, generation, x, y) end

---Permanently remove the authored scenery with this session and generation.
---@param unit integer
---@param session integer
---@param generation integer
---@return true|nil removed
---@return string|nil reason
function coe5.removeTestFieldScenery(unit, session, generation) end

---Open the read-only native card for a verified feature at this zero-based cell.
---A final capture is inspectable only while its live identity and position still match.
---@param x integer
---@param y integer
---@param final? boolean
---@return true|nil shown
---@return string|nil reason
function coe5.inspectTestFieldFeature(x, y, final) end

---Windows 5.43. Read the visible state and rectangle of the owned field's
---native battle-log overlay. Reports the native toggle action and line count
---when available. The overlay has no reserved map space.
---@return table|nil state
---@return string|nil reason
function coe5.testFieldLogState() end

---Windows 5.43. Show or hide the owner's native battle-log overlay.
---Requires a safe update call, outside drawing. The original visibility
---is restored on close. Combat, message generation and reports continue.
---@param visible boolean
---@return boolean accepted
---@return string|nil reason
function coe5.setTestFieldLogVisible(visible) end

---Read the native water cover and drowning predicate for a living owned actor
---at a hypothetical anchor. Windows 5.43. Does not move, damage or board the actor.
---@param unit integer
---@param x integer
---@param y integer
---@return table|nil state fields include cellWater, willDrown, waterMobile and ship
---@return string|nil reason
function coe5.testFieldWaterState(unit,x,y) end

---@class TestFieldDeploymentTarget
---@field unit integer owned roster actor
---@field generation integer|nil expected construction generation, required with a receipt
---@field x integer exact anchor column
---@field y integer exact anchor row

---Windows 5.43. Apply a complete roster's exact anchors inside an owned field.
---Retains verified fixed structures and scenery. Checks the entire footprint,
---native passability, unchanged actor bodies and final anchor readback.
---Optional initialReceipt permits only evacuation of verified native initial
---actor overlap. Reports byteRollbackVerified separately from healthy rollback.
---If rollback cannot be verified, close the quarantined field and use the saved setup.
---@param targets TestFieldDeploymentTarget[]
---@param options? {initialReceipt: integer}
---@return boolean accepted
---@return string|nil reason
---@return table detail
function coe5.setTestFieldDeployment(targets, options) end

---Windows 5.43. Get one-use evidence captured by native opening for the current
---owner and generation. A receipt is eligible only for verified initial
---actor-only overlap and becomes invalid if the captured source changes.
---@return table receipt version, session, token, fresh, eligible, actors, overlaps and reason
function coe5.testFieldInitialDeployment() end

---Windows 5.43. Change the owned field's duplicate-unique constructor policy
---without rebuilding. Disabling refuses while a living duplicate of an owned
---unit's unique group exists. Changes affect subsequent constructor calls only.
---@param enabled boolean
---@return boolean accepted
---@return string|nil reason
function coe5.setTestFieldMultipleUnique(enabled) end

---@class TestFieldActorSubset
---@field version integer 1
---@field monster integer
---@field width integer
---@field height integer
---@field maxHp integer native derived maximum after restored XP and injuries
---@field hp integer positive native HP
---@field xp integer
---@field kills integer
---@field injuryFlags integer native injury bits 20 through 30

---Windows 5.43. Read supported condition fields of a living owned actor.
---The result includes its unit and generation and sets fullState to false.
---Other native effects, opaque state, poison and bleeding are outside the contract.
---@param unit integer
---@return TestFieldActorSubset|nil state
---@return string|nil reason
function coe5.testFieldActorState(unit) end

---Windows 5.43. Restore the bounded condition subset after loadout restoration.
---Checks monster, footprint and derived maximum health. Preserves other flags,
---items and spells, and reports any unverified rollback.
---@param unit integer
---@param state TestFieldActorSubset
---@return boolean accepted
---@return string|nil reason
---@return table detail
function coe5.setTestFieldActorState(unit, state) end

---What this CoE5SE's test field offers, for a script to check before it opens a field: uniqueGuard, state,
---pointerButtons, lifecycle, multipleUnique, emptyEditor, configuration, cursedRemoval, schoolLevelReduction.
---Windows-only optional capabilities include multipleUniquePolicy, battleLogVisibility and clearHeldItems.
---battleZoom, battleViewport, battleCenter and nativeMonsterInspectVersion advertise the Windows native presentation APIs.
---sceneryVersion advertises verified native scenery editing on supported Windows builds.
---and restoreHigherKnown, each 1 or 0; spellRecords, the spell table's size; equipmentPositions, 20; maxBaseSchoolLevel, 3.
---Windows adds minimumWidthVersion (1 for supported game code and lifecycle), minimumWidthMin and minimumWidthMax.
---@return {uniqueGuard: integer, state: integer, pointerButtons: integer, lifecycle: integer, multipleUnique: integer, emptyEditor: integer, spellRecords: integer, configuration: integer, cursedRemoval: integer, schoolLevelReduction: integer, restoreHigherKnown: integer, equipmentPositions: integer, maxBaseSchoolLevel: integer} capabilities
function coe5.testFieldEditorCapabilities() end

---Takes one owned unit off your editor field and empties its cells. Windows uses permanent native retirement so
---a removed regenerative unit cannot re-enter a later fight. Check capabilities.unitRetirement == 1 for this behavior.
---If retirement_unverified is returned, keep the saved setup and close the field before attempting another edit.
---@param unit integer a unit of the field
---@return boolean removed false when refused (lua_refused): game_code_differs, not_open, not_editor_field, not_owner, in_fight, final_read_only, bad_unit, not_on_field, dead_unit, primary_commander (the commander of a field with one), bad_field, ambiguous_token (the unit stands on two cells), remove_failed
---@return string|nil reason the reason, with that false
function coe5.removeTestFieldUnit(unit) end

---Windows 5.43 owned main-menu test field only. Read the actual personal-name
---source and the game's native formatted title for this session/generation.
---Ordinary actors: owned_native_formatter_override (no native per-unit string
---exists); commanders: native_commander_record, shown as "name the type".
---Works after fighting only while the original record remains identifiable.
---Counters show formatter use, not proof that every game surface was tested.
---@param unit integer
---@return table|nil state {version:1,unit:integer,generation:integer,commander:integer,custom:boolean,name:string,shown:string,storage:string,maxBytes:47,nativeLogLineBytes:63,formatterReads:integer,combatReads:integer}
---@return string|nil reason
function coe5.testFieldUnitName(unit) end

---Guarded individual rename for a living owned editor actor, outside simulation
---and final view. Strict UTF-8, at most47 bytes, NUL/control/bidi characters,
---percent/comma and whitespace-only nonempty strings refuse. No truncation or
---normalization. Empty restores the original commander record or ordinary
---type/effect formatter. Never writes monsterName or another unit/template.
---Native Inspect and combat use the real formatter; combat rows still truncate
---at63 bytes for the ENTIRE line. Fonts may lack glyphs. Not campaign-save or
---promotion/commander-conversion support. Generation/session bound; cleared on
---field close. Failure with rollbackVerified=false requires quarantine/recovery.
---Caps: individualNameVersion=1, individualNameMaxBytes=47, nativeLogLineBytes=63;
---these describe support, not installed hook or successful rendering evidence.
---@param unit integer
---@param name string empty clears
---@return boolean accepted
---@return string|nil reason
---@return table detail {version:1,maxBytes:47,rollbackVerified:boolean}
function coe5.setTestFieldUnitName(unit, name) end

---Every unit and cell of your editor field. units holds each living unit of both sides and each unit a cell holds, in
---unit order, with among others unit, hp, monster, owner, generation, width, height, anchors and occupied (its cells),
---and anchorX, anchorY when it stands on one cell. grid[x * 20 + y + 1] is cell (x, y). Each issue is a code and the
---unit, x and y it concerns; complete is false when a part couldn't be read.
---@return {width: integer, height: integer, emptyEditor: boolean, allowMultipleUnique: boolean, complete: boolean, units: table[], grid: {x: integer, y: integer, unit: integer, object: integer, flags: integer, ground: integer}[], issues: {code: string, unit?: integer, x?: integer, y?: integer}[]}|nil state nil when refused (lua_refused): game_code_differs, not_open, not_owner, not_editor_field, out_of_memory
---@return string|nil reason the reason, with that nil
function coe5.testFieldState() end

---One unit's row of coe5.testFieldState(), living or not, with the field's complete and issues.
---@param unit integer a unit of the field
---@return table|nil row nil when refused (lua_refused): coe5.testFieldState()'s reasons, bad_unit, not_on_field, bad_field
---@return string|nil reason the reason, with that nil
function coe5.testFieldUnitState(unit) end

---The mouse buttons held now, 1 left, 2 right, 4 middle, added, read from the system rather than the game's last frame,
---in a BATTLE_WAIT frame of your open test field.
---@return integer|nil buttons 0 to 7; nil when refused (lua_refused): game_code_differs, not_open, not_owner, not_battle_frame, sdl_mouse_unavailable
---@return string|nil reason the reason, with that nil
function coe5.testFieldPointerButtons() end

---A unit's spells: known and memorized (spell numbers), schools (each magic path it has a level or a spell in) and
---valid, false when the lists break a memorization rule, with issues[1] naming it.
---@param unit integer a unit of your editor field
---@return {unit: integer, generation: integer, complete: boolean, valid: boolean, known: integer[], memorized: integer[], schools: {path: integer, name: string, level: integer, bonus: integer, allowance: integer, knownCount: integer, memorizedCount: integer}[], issues: table[]}|nil spells nil when refused (lua_refused): game_code_differs, not_open, not_owner, not_editor_field, bad_unit, not_on_field, dead_unit, bad_monster, bad_item
---@return string|nil reason the reason, with that nil
function coe5.testFieldSpellState(unit) end

---Memorizes a spell the unit knows (true) or drops it from the memorized ones (false), as the game's memorization rules
---allow; nothing is picked at random. True when it already was so. The table says which check stopped it.
---@param unit integer a unit of your editor field
---@param spell integer a spell
---@param on boolean
---@return boolean done false when refused (lua_refused): game_code_differs, not_open, not_editor_field, not_owner, in_fight, final_read_only, bad_unit, not_on_field, dead_unit, bad_spell, bad_state, memorized_not_known, spell_level (above the unit's level in its magic path), memorize_limit (more than the level plus 1 in a path), too_many_spells (30 memorized), restore_failed
---@return string|nil reason the reason, with that false
---@return {stage: string, code: string, path: integer, spell: integer, actual: integer, allowed: integer, allowance: integer, level: integer}|nil detail
function coe5.memorizeTestFieldSpell(unit, spell, on) end

---The units the game made for your last editor field: created, beforeCleanup and afterCleanup, each row a unit, its
---generation, monster, owner and square, and a reason: active, cleaned, already_dead, identity_replaced or
---cleanup_failed. Readable after the field closes, until the next editor field opens.
---@return table|nil lifecycle nil when refused (lua_refused): game_code_differs, lifecycle_unavailable, no_session, not_owner
---@return string|nil reason the reason, with that nil
function coe5.testFieldLifecycle() end

---How many battle cells a monster's token takes, width and height (1 to 3, up to 12 with a battlesize), with no unit made; with a unit of your
---editor field, that unit as it is now.
---@param monster integer a monster
---@param unit? integer a unit of the field; -1 when left out
---@return {monster: integer, unit: integer, width: integer, height: integer, exact: boolean, instance: boolean, source: string}|nil footprint nil when refused (lua_refused): bad_unit, game_code_differs, not_open, not_owner, in_fight, bad_monster, bad_sprite, bad_sprite_size, bad_footprint, and with a unit coe5.removeTestFieldUnit's first nine
---@return string|nil reason the reason, with that nil
function coe5.testFieldFootprint(monster, unit) end

---For each cell a token at (x, y) would take, its token's lowest x and y there: whether it is on the field and what
---covers it (status clear, occupied, terrain, unknown or out_of_bounds), and where it is on screen; valid when every cell
---is clear. Size and cover only: the game's placement still decides. ignoreSelf true leaves unit's token out.
---@param monster integer a monster
---@param x integer
---@param y integer
---@param unit? integer a unit of the field; -1 when left out
---@param ignoreSelf? boolean
---@return table|nil preflight nil when refused (lua_refused): coe5.testFieldFootprint's reasons, bad_type, not_battle_frame, bad_anchor
---@return string|nil reason the reason, with that nil
function coe5.testFieldPlacementPreflight(monster, x, y, unit, ignoreSelf) end

---The game's card for a living unit of your editor field, as its owner sees it, with no change to its spells
---or items; returns when the player closes it. The table says what changed while it was up and what was put back.
---@param unit integer a unit of the field
---@param options? {readOnly?: true}
---@return boolean shown false when refused (lua_refused): game_code_differs, not_open, not_editor_field, not_owner, in_fight, final_read_only, bad_unit, not_on_field, dead_unit, inspect_active (a card is up), not_main_menu, text_input_hook_unavailable, not_battle_frame (the field not drawn yet), bad_options, read_only_required, text_input_unavailable, the memorization rules' reasons; after the card, identity_changed, inspect_changed_unverified, restore_failed
---@return table|string detailsOrReason
function coe5.inspectTestFieldUnit(unit, options) end

---Opens the real native unit card for a temporary instance of a loaded monster type.
---Available only on Windows 5.43 with the supported Clang 23 build and an owned idle main-menu
---editor field after its first draw. Invoke outside BATTLE_DRAW and close the native card normally.
---Construction uses a fresh off-field slot and preserves existing actors, terrain, unique groups
---and the game RNG. A successful return verifies cleanup, restores the slot and reports the type.
---Starting items are omitted. Randomized preview spells can differ from a later placed instance.
---An unverified cleanup quarantines the field. Check the returned detail before editing again.
---@param monster integer a loaded monster type
---@return boolean shown
---@return table|string detailOrReason verified preview detail on success; refusal reason on failure
---@return table|nil refusalDetail cleanup verification and quarantine status when refused
function coe5.inspectTestFieldMonster(monster) end

---The same card for a unit that outlived the field's last fight, as it is now.
---@param unit integer a unit of coe5.testFieldFinalState()
---@param options? {readOnly?: true}
---@return boolean shown false when refused (lua_refused): coe5.inspectTestFieldUnit's reasons, no_final_state, not_in_final_state, not_a_visible_survivor (dead, or not on one cell at the fight's end), removed_after_capture
---@return table|string detailsOrReason
function coe5.inspectTestFieldFinalUnit(unit, options) end

---The game's card for a magic item, on your open test field.
---@param item integer an item
---@return boolean shown false when refused (lua_refused): bad_item, game_code_differs, not_open, not_owner, in_fight, final_read_only, not_main_menu, inspect_active, text_input_unavailable, item_card_hook_unavailable (CoE5SE couldn't open the card), field_changed
---@return table|string detailsOrReason
function coe5.inspectTestFieldItem(item) end

---Your editor field as its last fight left it, read after the fight's end and before the game's upkeep after it:
---round, winner, units (with hp, alive, owner, team 1 or 2, status on_field, off_field, dead or unverified, and its
---cells), grid, complete, and issues (256 at most; issueCount counts all). sceneComplete is always false: no effects or
---projectiles are kept. When the fight ran without the capture, phase is unavailable with the issue capture_hook_unavailable.
---@return table|nil final nil when refused (lua_refused): game_code_differs, not_open, not_owner, in_fight, no_final_state
---@return string|nil reason the reason, with that nil
---Windows final snapshots include classificationVersion=1. Each generation's
---kind and creationSource distinguish roster actors, scenery and native gates.
---Native owner and team are reported separately, including a gate's original side.
function coe5.testFieldFinalState() end

---Takes the keyboard for a script's text field on the test field (true), or gives it back. While you have it, typed
---text goes to coe5.takeTestFieldTextEvents() and no key reaches the game but Alt+Tab, Alt+Enter, Alt+F4 and Win+M
---(Cmd+M on Mac). It ends on Enter, Esc, the window losing focus, a fight, the field's close, coe5.ui.wait and
---coe5.ui.nameBox.
---@param on boolean
---@return boolean done false when refused (lua_refused): bad_type, not_owner, game_code_differs, text_input_hook_unavailable, not_open, in_fight, text_input_unavailable
---@return string|nil reason the reason, with that false
function coe5.setTestFieldTextFocus(on) end

---What was typed since the last call, 64 events at most: each {type = "text", text} (UTF-8) or a type of backspace,
---clear (Ctrl+Backspace or Ctrl+U), submit (Enter), cancel (Esc) or blur (the window lost focus); overflow when an
---event was lost.
---@return {focused: boolean, overflow: boolean, events: {type: string, text?: string}[]}|nil typed nil when refused (lua_refused): not_open, not_owner
---@return string|nil reason the reason, with that nil
function coe5.takeTestFieldTextEvents() end

---Where the test field's battlefield is on screen now, in UI units (x from the left, y up from the bottom), in a
---BATTLE_DRAW or BATTLE_WAIT frame. originX and originY are the field's corner: baseX plus cameraX, baseY plus cameraY.
---@return {cellSize: number, originX: number, originY: number, baseX: number, baseY: number, cameraX: number, cameraY: number, width: integer, height: integer, screenW: number, screenH: number, windowW: integer, windowH: integer}|nil view nil when refused (lua_refused): not_open, game_code_differs, not_battle_frame, invalid_battle_view
function coe5.battleView() end

---The test field's battle cell under a point in UI units, found as the game finds the cell under the pointer.
---@param uiX number
---@param uiY number
---@return integer|nil x nil off the field, or when refused as coe5.battleView()
---@return integer|nil y
function coe5.battleScreenToCell(uiX, uiY) end

---The centre of a battle cell on screen, in UI units.
---@param x integer
---@param y integer
---@return number|nil uiX nil off the field, or when refused as coe5.battleView()
---@return number|nil uiY
function coe5.battleCellToScreen(x, y) end

---Windows 5.43 owned idle field view with the supported Clang 23 build. Getter reads validated native globals without
---installing a hook; values describe the current projection, not proof that pixels were redrawn.
---Zoom is relative to viewport-height / 20. Mutations belong after BATTLE_WAIT next() returns;
---BATTLE_DRAW is read-only. Wait for a fresh native DRAW before processing mapped input.
---@return {version:integer,zoom:number,minZoom:number,maxZoom:number,cellSize:number}|nil view
---@return string|nil reason
function coe5.testFieldZoom() end

---Changes actual native cell scale/camera, with optional UI-coordinate anchor. Finite zoom 0.1..2;
---default anchor is screen centre. Native pan bounds still apply. Retained within the same owned
---field; caller must reapply after rebuilding. Close restores the saved scale/camera. No campaign,
---combat or final-view mutation. Exact supported native code is required; errors are explicit.
---@param zoom number
---@param anchorX? number
---@param anchorY? number
---@return boolean accepted
---@return string|nil reason
function coe5.setTestFieldZoom(zoom, anchorX, anchorY) end

---Experimental Windows 5.43 owned idle editor viewport, in lower-left native UI coordinates.
---Margins are retained across window resizing. Width/height must be finite and >4, and the
---rectangle must lie inside the native screen. Native projection globals remain shared with
---rendering, pointer mapping, previews and Inspect. Set/Fit/Pan mutate only after native WAIT;
---DRAW is read-only. Wait for a fresh DRAW before handling mapped input. Close restores the
---original scale/camera. Refusal does not imply a fallback projection; unverified rollback
---disables the view contract and retains the close recovery snapshot. Mac is unavailable.
---@param rectangle {x:number,y:number,width:number,height:number}
---@return boolean accepted
---@return string|nil reason
function coe5.setTestFieldViewport(rectangle) end

---Read actual usable rectangle and projection, including reachable map origins. The two-unit
---inner padding is included in min/maxOrigin. fits requires both axes; it describes geometric
---fit, not a completed render or an unobscured OS window. No hook installation by this getter.
---@return {version:integer,x:number,y:number,width:number,height:number,cellSize:number,originX:number,originY:number,minOriginX:number,maxOriginX:number,minOriginY:number,maxOriginY:number,fitsX:boolean,fitsY:boolean,fits:boolean,fieldWidth:integer,fieldHeight:integer,margin:number}|nil viewport
---@return string|nil reason
function coe5.testFieldViewport() end

---Choose an undistorted uniform scale using actual native field columns/20 rows. Clamp to
---0.1..2; a huge field may still return fits=false and require native arrows/middle-drag or Pan.
---Requires a previously accepted usable rectangle. Read back actual zoom and viewport.
---@return boolean accepted
---@return string|nil reason
function coe5.fitTestFieldViewport() end

---Pan in native UI units, clamped to the accepted viewport's full reachable map extent.
---The scoped native pan clamp runs before native cursor/effect-cache delta translations.
---@param dx number finite
---@param dy number finite
---@return boolean accepted
---@return string|nil reason
function coe5.panTestFieldViewport(dx,dy) end

---Windows owned idle-field input, during BATTLE_WAIT only. Returns
---the native frame-latched cumulative wheel count as unsigned 32-bit (wraps).
---Read-only/non-consuming; compute successive deltas modulo 2^32, initializing
---a fresh baseline on entering the field. Discard deltas over menus, text focus,
---panels/modals/scrollable controls; never replay them on return to the map.
---This getter alone does not suppress native input or choose a scroll region.
---@return integer|nil count
---@return string|nil reason
function coe5.testFieldPointerWheel() end

---Queue a one-shot camera center for the next verified fight in this owned editor field.
---Apply on the first authenticated combat-map reset at native combat scale. The editor's last
---zoom is observational. Subsequent native battle panning stays available.
---@return boolean queued
---@return string|nil reason
function coe5.requestTestFieldBattleCenter() end

---Read the current owned session's camera request, including status and applied native geometry.
---A queued or armed request has no observed camera position. An applied request includes
---fieldWidth, fieldHeight, initialCellSize, initialOriginX/Y and initialScreenW/H.
---@return table|nil state
---@return string|nil reason
function coe5.testFieldBattleCenterState() end

---A unit's items by position (0 to 19: id, -1 for none, slotType, and an item's name, cursed and permanentEffects), its
---schools (each magic path its monster has: templateBase, its monster's level, base, the level set, level, bonus,
---allowance) and its known and memorized spells. The first call on a field puts in what lets a path's level be set; it
---comes out when the field closes.
---@param unit integer a unit of your editor field
---@return table|nil loadout nil when refused (lua_refused): game_code_differs, not_open, not_editor_field, not_owner, in_fight, final_read_only, bad_unit, not_on_field, dead_unit, not_main_menu, training_hooks_unavailable
---@return string|nil reason the reason, with that nil
function coe5.testFieldLoadoutState(unit) end

---Sets a unit's items, path levels and spells together, reads them back, and puts the unit back as it was when anything
---fails. Windows builds advertising clearHeldItems=1 accept clearHeldItems=true with twenty empty slots and the
---original schools and ordered spells. This mode clears cursed and implanted items, preserves existing organ
---injuries, and refuses if an active memorization depends on the removed equipment. It cannot be combined with
---restoreKnown or removeCursed. The table
---says how far it got: stage validate, committed, rejected_rollback_verified or rollback_unverified.
---@param unit integer a unit of your editor field
---@param state {items: integer[], schools: {[1]: integer, [2]: integer}[], known: integer[], memorized: integer[], restoreKnown?: boolean, removeCursed?: {[1]: integer, [2]: integer}, clearHeldItems?: boolean} items 20 item numbers (-1 for none); schools {path, level} for every magic path its monster has, level 1 to 3, or the monster's when higher; known and memorized 60 and 30 spells at most; removeCursed {position, item}
---@return boolean done false when refused (lua_refused): coe5.removeTestFieldUnit's first nine, not_main_menu, training_hooks_unavailable, bad_state, bad_item_positions, bad_item, bad_schools, bad_school_level, school_not_existing, unsupported_school_level, duplicate_school, missing_school_configuration, bad_spell_list, too_many_spells, bad_spell, duplicate_spell, bad_restore_known, bad_cursed_removal, identity_changed, incomplete_native_configuration, cursed_removal_changed, cursed_removal_only, bad_existing_item, cursed_item, permanent_anatomy_item_unsupported, out_of_memory, too_many_configured_units, incompatible_slot, native_item_restriction, equipment_changes_anatomy, the memorization rules' reasons, new_spell_above_level, configuration_readback_failed, school_readback_failed, permanent_effect_changed, rollback_failed
---@return string|nil reason the reason, with that false
---@return table detail
function coe5.setTestFieldConfiguration(unit, state) end

---A weapon look's name in the game's visual effect table, the numbers look and flylook take ("Fireball").
---@param look integer
---@return string|nil name nil past the table or for an unnamed row
function coe5.lookName(look) end

---Puts `text` on the system clipboard as plain text, for the player to paste.
---@param text string
---@return boolean copied false when the text holds a 0 byte (bad_string) or the system refused (clipboard_refused), logged as lua_refused
function coe5.copyText(text) end

---Makes a new mod in the player's mods folder: <name>/<name>.c5m, holding the icon and description
---lines and then `text`, beside a plain banner. It starts disabled, as every mod the game finds.
---It never writes into an existing mod: a name whose folder exists is refused.
---@param name string 1 to 40 letters, digits, spaces and _, no space first or last
---@param text string the .c5m lines after the icon and description
---@return boolean saved false when refused (lua_refused): no_name, name_too_long, bad_name, bad_string, path_too_long, exists, folder_failed, write_failed
function coe5.saveNewMod(name, text) end

---Call it in a Game Menu line's onClick, in a single player game. When onClick returns, CoE5SE
---ends the game the way Resign does, and after the Game Over screen it starts a new game's setup
---instead of going back to the main menu. Your wraps can fill that setup in while
---coe5.restartPhase() is "starting".
---@return boolean asked false outside a Game Menu line's onClick, in a network game, or unless exactly one human player is in the game (logged as api_refused)
function coe5.restartGame() end

---Call it in a Game Menu line's onClick, in a single player game. It shows the game's Load Game
---list, with every mod's wraps of LOAD_GAME_BROWSER, while the game goes on underneath. Cancel
---returns false and the game goes on. When a save is picked, it returns true, and when onClick
---returns, CoE5SE ends the game the way Resign does and then loads the save, as the main menu's
---Load Game does. coe5.restartPhase() is "ending" until then. CoE5SE clicks Ok and Exit on the
---screens the end shows, and Ok on the loaded turn's first Messages screen (View messages, key m,
---still shows them).
---@return boolean picked false on Cancel (logged as load_game phase=cancelled), outside a Game Menu line's onClick, in a network game, unless exactly one human player is in the game, or while a Load Game list from coe5.loadGame is already up, as from a wrap of its sites (logged as api_refused, reason=already_picking)
function coe5.loadGame() end

---Where a restart is: "ending" from the onClick that called coe5.restartGame or picked a save with
---coe5.loadGame until the old game's MAIN_MENU_PLAY_GAME returns, "starting" during the new game
---CoE5SE started (its setup and play), and nil otherwise. A game started from the main menu, or
---loaded with coe5.loadGame, is always nil. A MAIN_MENU_PLAY_GAME wrap that calls next() again
---cancels the restart or load: that second game is nil (logged as restart_game phase=cancelled
---reason=play_game_reentered).
---@return "ending"|"starting"|nil phase
function coe5.restartPhase() end

---The game's screen widgets. A screen is a loop. Each pass calls screenBegin, draws sections,
---then buttonRow and its lines, and calls wait, which returns the clicked id.
---
---    while true do
---        coe5.ui.screenBegin("My Mod")
---        coe5.ui.sectionBegin("Options")
---        coe5.ui.text(10, "Say hello")
---        coe5.ui.sectionEnd()
---        coe5.ui.buttonRow()
---        coe5.ui.text(9, "Back")
---        coe5.ui.bindEscape(9)
---        local id = coe5.ui.wait()
---        if id == 9 then return end
---        if id == 10 then coe5.log("hello") end
---    end
-- ---- Plane generator and custom UI ------------------------------------------------------------

---@class coe5.ui
coe5.ui = {}

---The game's random map generator steps, one per function, for a plane's recipe or generate function
---(coe5.addPlane). They work only while a mod's plane is being built, and only on it: a square off
---the plane is refused (lua_refused reason=outside_plane), a rectangle is cut to the plane, and
---outside a build every call is refused (not_world_building). A refused call returns nil.
---The map generator draws with marks: a walk, blob, rectangle or neighbour pass writes a mark number
---(1 to 32767) on squares, and a marks-to-terrain pass turns marked squares into a terrain. The marks
---are the plane's for the build: empty at its start, gone after it.
---@class coe5.gen
coe5.gen = {}

---The game's random draw, 0 to n - 1, as its map generator draws (0 for n below 2, with no draw).
---@param n integer
---@return integer
function coe5.gen.random(n) end

---A square's mark, 0 for none.
---@param x integer
---@param y integer
---@return integer
function coe5.gen.markAt(x, y) end

---Sets a square's mark (0 clears it).
---@param x integer
---@param y integer
---@param mark integer
function coe5.gen.setMark(x, y, mark) end

---The map generator's terrain write: the terrain only; the square's gems, its owner's citadel and
---income, and its ground stay as they were (setGround sets the ground after, as the generator does;
---coe5.set("squareTerrain", ...) is the in-game change).
---@param x integer
---@param y integer
---@param terrain integer 0 to terrainLastIndex
function coe5.gen.setTerrain(x, y, terrain) end

---A terrain's flag bits: 4 water, 0x2000 forest, 0x10000 mountain, among others. Works outside a build.
---@param terrain integer
---@return integer
function coe5.gen.terrainFlags(terrain) end

---The squares within radius of (x, y), on its plane, holding the terrain or in the terrain group.
---@param x integer
---@param y integer
---@param code integer a terrain number, or a terrain group (a negative terrain number)
---@param radius integer
---@return integer
function coe5.gen.terrainNear(x, y, code, radius) end

---Marks a rectangle, cut to the plane.
---@param x integer
---@param y integer
---@param w integer
---@param h integer
---@param mark integer
function coe5.gen.markRect(x, y, w, h, mark) end

---The map generator's random walk ("markmapcave") from a square of the plane, marking count new squares;
---with a terrain each step keeps to squares whose whole 3 x 3 is that terrain, with -1 it goes anywhere.
---Gives up after 1,000,000 steps.
---@param x integer
---@param y integer
---@param terrain integer a terrain, or -1
---@param count integer 1 to the plane's squares
---@param mark integer
function coe5.gen.markWalk(x, y, terrain, count, mark) end

---The map generator's round blob from a square of the plane, size squares.
---@param x integer
---@param y integer
---@param size integer 1 to the plane's squares
---@param mark integer
function coe5.gen.markBlob(x, y, size, mark) end

---Marks the cheapest path between two squares of the plane (the game's river path): mode 0 any
---ground, 1 through Cave Wall and Magma River, 2 around Cave Wall; it stops at water.
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param mark integer
---@param mode integer 0 to 2
---@return integer
function coe5.gen.markRiver(x1, y1, x2, y2, mark, mode) end

---Every square of the plane with the mark and terrain from becomes to ("mmct").
---@param mark integer
---@param from integer
---@param to integer
---@return integer changed
function coe5.gen.marksToTerrain(mark, from, to) end

---Every square of the plane with the mark becomes terrain at percent, whatever its terrain ("mm2t";
---one draw per marked square).
---@param mark integer
---@param terrain integer
---@param percent integer 0 to 100
---@return integer changed
function coe5.gen.marksToTerrainChance(mark, terrain, percent) end

---Marks every square of terrain with code next to it ("mmnbors").
---@param terrain integer
---@param code integer a terrain number or terrain group
---@param mark integer
---@return integer marked the plane's squares with that mark
function coe5.gen.markNearTerrain(terrain, code, mark) end

---Marks every Void square with code within radius (Celestial's clouds and flames).
---@param code integer
---@param mark integer
---@param radius integer
---@return integer marked the plane's squares with that mark
function coe5.gen.markVoidNear(code, mark, radius) end

---A random Cave Wall square 1 in from the plane's edge with at least need Cave Walls in its 3 x 3
---(its 5 x 5 above 9), 100,000 tries.
---@param need integer 0 to 25
---@return integer|nil x nil when none
---@return integer|nil y
function coe5.gen.findCaveSpot(need) end

---Temperature 25 on the rectangle ("setfixtemp").
---@param x integer
---@param y integer
---@param w integer
---@param h integer
function coe5.gen.setFixedTemperature(x, y, w, h) end

---The game's temperature step: 25, -10 on Frozen Waste, the heat of mountains, deserts and jungles, the cold flags.
---@param x integer
---@param y integer
---@param w integer
---@param h integer
function coe5.gen.setTemperatures(x, y, w, h) end

---The game's heat blob, a step of setTemperatures: a random walk from (x, y) adds amount to the temperature of each
---new square it reaches, going back to (x, y) after each, until squares are counted. Squares the walk reaches off
---the plane count but keep their temperature.
---@param x integer
---@param y integer
---@param squares integer 1 to the plane's square count
---@param amount integer -32768 to 32767
function coe5.gen.heatBlob(x, y, squares, amount) end

---The ground kinds of the rectangle from each square's terrain, only on squares of terrain unless -1.
---@param x integer
---@param y integer
---@param w integer
---@param h integer
---@param terrain integer a terrain, or -1
function coe5.gen.setGround(x, y, w, h, terrain) end

---One flooding pass: the plane's open squares joined to water flood (Agartha's sea).
---@return integer flooded
function coe5.gen.floodPlane() end

---Cave Wall next to water becomes Moist Cave Wall.
function coe5.gen.updateMoistWalls() end

---Rock Ground touching open ground only at a corner becomes Cave Wall, until none is left.
---@return integer
function coe5.gen.fixDiagonals() end

---tries times, a random neighbour holding terrain onto takes the square's terrain (the Sky's cloud forests).
---@param x integer
---@param y integer
---@param tries integer
---@param onto integer
---@return integer changed
function coe5.gen.spreadTerrain(x, y, tries, onto) end

---Elysium's terrain blob: n squares of t1 or t2 grown from (x, y), wx and wy weighting east-west and
---north-south growth (forests 40 to 99 each way, deserts 999 and 999). a, b, c are the game's last
---three numbers (forests 0, 0, 0; deserts 0, 0, 1). Squares grown off the plane are put back.
---@param x integer
---@param y integer
---@param wx integer
---@param wy integer
---@param n integer
---@param t1 integer
---@param t2 integer
---@param a? integer
---@param b? integer
---@param c? integer
function coe5.gen.specBlob(x, y, wx, wy, n, t1, t2, a, b, c) end

---Elysium's river from one square of the plane to another: River on the cheapest path.
---@param x integer
---@param y integer
---@param tx integer
---@param ty integer
function coe5.gen.drawRiver(x, y, tx, ty) end

---Elysium's random special terrain for one square by its terrain (villages, farms, mines, ports; towers, castles and
---ruins only on Elysium).
---@param x integer
---@param y integer
function coe5.gen.decorateSquare(x, y) end

---count random items lying on the square, at level (Agartha's statue: 2 to 6 at 25).
---@param x integer
---@param y integer
---@param count integer 1 to 50
---@param level integer
function coe5.gen.addGroundItems(x, y, count, level) end

---A random square of the plane holding the terrain or in the terrain group (randloc's search,
---500,000 tries).
---@param code integer
---@return integer|nil x nil when none
---@return integer|nil y
function coe5.gen.randomSquareOfTerrain(code) end

---A random square of the plane with the mark that holds the terrain or is in the terrain group, the search the
---Inferno's builder uses ("randommarkloc", 10,000,000 tries); with openLand, only open land
---(squareOpenLand). Draws with the C runtime's rand, as the game does, not coe5.gen.random.
---@param mark integer 1 to 32767
---@param code integer a terrain number or terrain group
---@param openLand? boolean
---@return integer|nil x nil when none
---@return integer|nil y
function coe5.gen.randomMarkedSquare(mark, code, openLand) end

---The map generator's terrain blob ("terrblob", the Sky's cloud banks): n times a random walk of up to 25 steps
---from (x, y), held within maxDist of it and on the plane, lays terrain on the first square holding onto (-1: any
---square not already that terrain). Squares reached off the plane are put back.
---@param terrain integer
---@param x integer
---@param y integer
---@param n integer 1 to the plane's squares
---@param onto integer a terrain, or -1
---@param maxDist integer
---@return integer laid
function coe5.gen.terrainBlob(terrain, x, y, n, onto, maxDist) end

---A random square of the plane matching code whose 3 x 3, itself included, holds min to max squares matching
---nearCode (the Sky's Castle Arcanum: Sky, Sky, 9, 100), 500,000 tries.
---@param code integer a terrain number or terrain group
---@param nearCode integer a terrain number or terrain group
---@param min integer 0 to 9
---@param max integer
---@return integer|nil x nil when none
---@return integer|nil y
function coe5.gen.findSpotByNeighbours(code, nearCode, min, max) end

---Whether a monster's path leads from the square to Elysium's centre, or 10 squares west or east of it, through
---the portals the world has so far: the Sky's test for a Cloud City.
---@param x integer
---@param y integer
---@return boolean
function coe5.gen.reachesElysium(x, y) end

---A portal from a square of the plane to a square of any plane: n on the first, n (two ways) or -n
---(one way) on the second. A portal pairs with every square holding its number.
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param twoway boolean
---@param n integer 1 to 32767
---@return integer n
function coe5.gen.setPortalPair(x1, y1, x2, y2, twoway, n) end

---setPortalPair with a number no square holds yet, picked as the game picks one (1000 to 10999).
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param twoway boolean
---@return integer n
function coe5.gen.linkNewPortal(x1, y1, x2, y2, twoway) end

---For a step with a minimum plane size: true when the plane being built is at least minW x minH; else false, and
---the log names the step and its minimum (lua_refused reason=plane_too_small).
---@param step string
---@param minW integer
---@param minH integer
---@return boolean
function coe5.gen.refuseSmallPlane(step, minW, minH) end

---Stamps a map file from your mod's folder ("./citadel.coem", the game's single-plane format) with
---its top left square at (x, y), through the game's map reader, as a newplane block's planepiece does:
---the plane's square names, portals and special square flags (squarespec) under it are cleared first. Its whole mapsize
---must be on the plane (outside_plane). The pieces coe5.addPlane declares, stamped after generate,
---never go on it.
---@param file string
---@param x integer
---@param y integer
---@return boolean|nil stamped nil when refused (lua_refused reason=outside_plane, not_world_building, no_plane, no_mod_folder, bad_file, file_not_found, file_wrong_case, file_link, path_too_long, no_mapsize, out_of_memory, hook_not_installed)
function coe5.gen.stampPiece(file, x, y) end

---Clears the screen and draws the title. Each text the coe5.ui calls draw is cut to the game's
---buffer for it, the most the game draws (logged as api_cut): 99 characters for a title, 4999 for
---a line, list row, or tooltip, 999 for a checkbox, 998 for a choice.
---@param title string
function coe5.ui.screenBegin(title) end

---Starts a framed section.
---@param header? string
function coe5.ui.sectionBegin(header) end

---Ends the section.
function coe5.ui.sectionEnd() end

---Draws one line. Id 0 makes it plain text; any other id makes it clickable.
---@param id integer
---@param text string
function coe5.ui.text(id, text) end

---Draws one checkbox line. A click on it makes wait return `id`; your script then flips its
---boolean value, as with choice.
---@param id integer
---@param checked boolean
---@param label string
function coe5.ui.checkbox(id, checked, label) end

---Draws one radio line. Your script tracks which one is selected.
---@param id integer
---@param selected boolean pass a boolean such as `id == picked`: in Lua, 0 counts as true
---@param label string
function coe5.ui.choice(id, selected, label) end

---Hover text for the line drawn just before. Right after coe5.ui.monsterRow it is the whole row's
---hover, and a second tooltip right after replaces the first across the whole row.
---@param text string
function coe5.ui.tooltip(text) end

---Starts the bottom row of buttons (Ok, Back, ...). Always last.
function coe5.ui.buttonRow() end

---Starts a list, Load Game's layout: call it after screenBegin, then listRow for each item, then
---listEnd. Use it to pick one of several things, each opening a custom screen.
function coe5.ui.listBegin() end

---Draws one striped row of the list, 450 wide. A click on it makes wait return `id`.
---@param id integer
---@param text string
function coe5.ui.listRow(id, text) end

---Ends the list, with the gap before the button row.
function coe5.ui.listEnd() end

---Makes the Esc key click line `id`.
---@param id integer
function coe5.ui.bindEscape(id) end

---Makes a key click line `id`: coe5.key.enter, coe5.key.esc (as bindEscape), or a lowercase letter's
---byte, string.byte("s"). Key 0 binds nothing. An id or key that doesn't fit the game's 32-bit int
---binds nothing either (logged as lua_refused, reason=out_of_range), as for bindEscape's id.
---@param id integer
---@param key integer
function coe5.ui.bindKey(id, key) end

---Takes the keyboard for a text field on your screen (true), or gives it back. Call it while drawing the
---screen, between screenBegin and wait. While you have it, typed text goes to coe5.ui.takeTextEvents(), and no
---key reaches the game but Alt+Tab, Alt+Enter, Alt+F4 and Win+M (Cmd+M on Mac); clicks still reach the screen. It
---lasts across your screen's waits and ends on Enter, Esc, the window losing focus, coe5.ui.nameBox, and any
---screen but yours, so take it again each frame while your field is open.
---@param on boolean
---@return boolean done false when refused (lua_refused): bad_type, not_owner (another mod has a text field open), game_code_differs, text_input_hook_unavailable, no_screen (not between screenBegin and wait), text_input_unavailable
---@return string|nil reason the reason, with that false
function coe5.ui.setTextFocus(on) end

---What was typed on your screen since the last call, 64 events at most: each {type = "text", text} (UTF-8) or a
---type of backspace, clear (Ctrl+Backspace or Ctrl+U), submit (Enter), cancel (Esc) or blur (the window lost
---focus); overflow when an event was lost. The Enter or Esc that ended the focus comes with focused false.
---@return {focused: boolean, overflow: boolean, events: {type: string, text?: string}[]}|nil typed nil when refused (lua_refused): not_owner
---@return string|nil reason the reason, with that nil
function coe5.ui.takeTextEvents() end

---Ends the frame: shows the screen and returns the clicked line's id.
---@return integer id the clicked line's id, 0 when nothing was clicked this frame
function coe5.ui.wait() end

---Draws a section's header: sectionBegin(header) is sectionBegin() then this.
---@param text string
function coe5.ui.sectionHeader(text) end

---Sets the screen's width. screenBegin uses Random Map Settings' width, coe5.ui.setupScreenWidth().
---NaN, an infinity, or a negative width changes nothing (lua_refused reason=not_finite or negative).
---@param width number
function coe5.ui.screenWidth(width) end

---The width Random Map Settings uses.
---@return number
function coe5.ui.setupScreenWidth() end

---Starts the menu screens' music, as the game does once before a screen's loop. Does nothing on Mac.
---Part of coe5.ui, CoE5SE's UI layer; coe5.startTune runs the game's tune function directly, with any tune.
function coe5.ui.screenMusic() end

---Moves where the next line of the row starts, `x` from the row's left edge: a new column
---in a row the game draws, such as the file browser's (FILE_BROWSER_ROW_DATE). NaN or an infinity
---changes nothing (lua_refused reason=not_finite).
---@param x number
function coe5.ui.columnX(x) end

---Opens a row `width` UI units wide, all of it clicked as `id` (0 for none), with a background in
---r, g, b (each 0 to 1) at `alpha` (none when left out or 0): a table's row, or its highlighted
---one. Draw the cells with coe5.ui.text at coe5.ui.columnX, then close it with coe5.ui.rowEnd.
---Refused (lua_refused), drawing nothing: a row already open (row_open), a width of 0 or less, an
---id past the game's 32-bit int, or a colour outside 0 to 1 (out_of_range), NaN or an infinity
---(not_finite). A row left open is closed at coe5.ui.wait or
---screenBegin (row_open).
---@param id integer
---@param width number
---@param r? number
---@param g? number
---@param b? number
---@param alpha? number
function coe5.ui.rowBegin(id, width, r, g, b, alpha) end

---Closes the row coe5.ui.rowBegin opened. With none open it does nothing (lua_refused reason=no_row).
function coe5.ui.rowEnd() end

---Draws a button with a colored face, `width` by `height` UI units, in r, g, b (each 0 to 1; a slate
---grey when left out): an edge, a lit top and a shaded bottom, lighter under the pointer, its label
---centered and `line2` in smaller text under it. A click on it makes wait return `id`. The screen lays
---it out as a row, with a small gap under it. Refused (lua_refused), drawing nothing: a row open
---(row_open), more than 64 on a screen (too_many), an id of 0 or past the game's 32-bit int, a width or
---height of 0 or less, or a colour outside 0 to 1 (out_of_range), NaN or an infinity (not_finite).
---@param id integer
---@param label string cut to 99 characters
---@param width number
---@param height number 28 fits one line, 42 fits line2 too
---@param r? number
---@param g? number
---@param b? number
---@param line2? string cut to 99 characters
function coe5.ui.button(id, label, width, height, r, g, b, line2) end

---The game's "Enter name" box, with a monster's picture. It runs a separate loop until Ok or Cancel.
---Enter clicks Ok, and Esc clicks Cancel. CoE5SE draws the box itself, as the game draws it, because
---the game's box binds no key to Esc and, on Mac, always says "Enter name", takes 25 characters,
---and has no Cancel.
---Part of coe5.ui, CoE5SE's UI layer; coe5.numberBox runs the game's number box directly.
---@param title string cut to 99 characters
---@param prompt string cut to 499 characters
---@param sprite integer a monster's picture: coe5.get("monsterSprite", monster)
---@param maxLength? integer 1 to 42; 32 when left out. The game's field takes two characters fewer (32 takes 30)
---@return string|nil text the typed text, or nil on Cancel, or at once for a sprite no monster has (lua_refused reason=bad_sprite)
function coe5.ui.nameBox(title, prompt, sprite, maxLength) end

---Draws one striped row: the monster's picture, then `text`, all clicked as `id`. With `text` only it
---is the row of the game's monster list ("Select Monster"), the picture as high as the line.
---With `line2` it is two lines under one stripe as wide as the screen's sections, the second one
---subtext (the recruit screen's smaller text, and fainter), and the picture is as high as both
---lines, beside them; a click anywhere on the row is `id`, and a coe5.ui.tooltip right after it is the
---whole row's hover. A monster index no monster uses draws nothing (lua_refused
---reason=bad_monster), and so does an id past the game's 32-bit int (out_of_range).
---@param id integer
---@param monster integer a monster's index (coe5.findMonster)
---@param text string
---@param line2? string a second line under it
function coe5.ui.monsterRow(id, monster, text, line2) end

---Draws one line from (x1, y1) to (x2, y2) at UI coordinates, in r, g, b (each 0 to 1) at `alpha`
---(0 clear, 1 solid), `width` screen pixels wide. A line straight across or straight up the screen
---is drawn on whole pixels, a pixel it covers in part at that part of `alpha`, so it moves by
---fractions of a pixel with the map while the zoom glides. Only inside a MAP_EDITOR_DRAW, MAP_DRAW or BATTLE_DRAW
---wrap; anywhere else it draws nothing (lua_refused reason=not_drawing). NaN or an infinity draws nothing
---(not_finite), and so does a width of 0 or less (out_of_range).
---@param x1 number
---@param y1 number
---@param x2 number
---@param y2 number
---@param r number
---@param g number
---@param b number
---@param alpha number
---@param width? number 1 when left out
function coe5.ui.drawLine(x1, y1, x2, y2, r, g, b, alpha, width) end

---Draws one line of text in the game's GUI font with its bottom left at (x, y), UI coordinates,
---`size` UI units high, in r, g, b at `alpha`. Only inside a MAP_EDITOR_DRAW, MAP_DRAW or BATTLE_DRAW wrap, as drawLine.
---The text is cut to 499 bytes (logged as api_cut).
---@param x number
---@param y number
---@param size number
---@param text string
---@param r number
---@param g number
---@param b number
---@param alpha number
function coe5.ui.drawText(x, y, size, text, r, g, b, alpha) end

---Draws a filled rectangle with its bottom left corner at (x, y), UI coordinates, `width` across
---and `height` up, in r, g, b at `alpha`: a panel behind drawn text, or a square of fog over the
---map. Only inside a MAP_EDITOR_DRAW, MAP_DRAW or BATTLE_DRAW wrap, as drawLine. A width or height of 0 or
---less draws nothing (out_of_range), and so does NaN or an infinity (not_finite).
---@param x number
---@param y number
---@param width number
---@param height number
---@param r number
---@param g number
---@param b number
---@param alpha number
function coe5.ui.drawRect(x, y, width, height, r, g, b, alpha) end

---Draws the game's icon for a resource as the next element of a screen's line: the icon the
---recruit screen shows beside a cost, as high as the text. In a row (coe5.ui.rowBegin) a
---coe5.ui.text after it sits beside it, and coe5.ui.tooltip after it is its hover text. `resource`
---is 0 Gold to 13 Lifeforce, the numbers playerResource takes. A resource outside 0 to 13 draws
---nothing (out_of_range), and so does a value that isn't a whole number (bad_type).
---@param id integer the click id, as coe5.ui.text's
---@param resource integer 0 Gold, 1 Iron, 2 Herbs, 3 Fungus, 4 Sacrifices, 5 Hand of Glory, 6 Enchanted Weed, 7 Rubies, 8 Sapphires, 9 Diamonds, 10 Emeralds, 11 Trade, 12 Relics, 13 Lifeforce
function coe5.ui.resourceIcon(id, resource) end

---Draws the game's icon for a resource with its bottom left corner at (x, y), UI coordinates,
---`size` UI units square. Only inside a MAP_EDITOR_DRAW, MAP_DRAW or BATTLE_DRAW wrap, as
---drawText. `resource` as coe5.ui.resourceIcon's. A size of 0 or less, or a resource outside 0
---to 13, draws nothing (out_of_range); so does NaN or an infinity (not_finite) and a resource
---that isn't a whole number (bad_type).
---@param x number
---@param y number
---@param size number
---@param resource integer 0 Gold to 13 Lifeforce
function coe5.ui.drawResourceIcon(x, y, size, resource) end

---A monster's sprite fitted into the rectangle with its bottom left at (x, y), its shape kept and centred. Only in a
---frame a script may draw in, as drawText.
---@param x number
---@param y number
---@param width number above 0, up to 512
---@param height number above 0, up to 512
---@param monster integer a monster
---@return boolean drawn false when refused (lua_refused): not_finite, out_of_range, not_drawing, bad_sprite, bad_sprite_size
---@return {sprite: integer, sourceWidth: integer, sourceHeight: integer, x: number, y: number, width: number, height: number}|string drawnOrReason what was drawn, or the reason
function coe5.ui.drawMonsterSprite(x, y, width, height, monster) end

---An item's sprite, as coe5.ui.drawMonsterSprite draws a monster's.
---@param x number
---@param y number
---@param width number above 0, up to 512
---@param height number above 0, up to 512
---@param item integer an item
---@return boolean drawn false when refused (lua_refused): not_finite, out_of_range, not_drawing, bad_sprite, bad_sprite_size
---@return {sprite: integer, sourceWidth: integer, sourceHeight: integer, x: number, y: number, width: number, height: number}|string drawnOrReason what was drawn, or the reason
function coe5.ui.drawItemSprite(x, y, width, height, item) end

---How wide coe5.ui.drawText draws `text` at `size`, in UI units, by the game's text measure, so
---a panel right aligns a number or cuts a long name exactly. Only in a preview world, inside a
---MAP_DRAW wrap. Windows only for now: on Mac the function doesn't exist.
---@param text string
---@param size number as coe5.ui.drawText's
---@return number|nil width nil when refused (lua_refused): no_preview_world, out_of_range, not_drawing, no_font
function coe5.ui.textWidth(text, size) end

-- ---- Archived aliases (kept for older mods) ------------------------------------------------------
-- Each archived call is the same function as its name now: the same arguments, the same answers,
-- and the same rules in a LAN game. New scripts use the name now.


---@deprecated Archived alias for coe5.unitFlightType. Kept for older mods.
---@see coe5.unitFlightType
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitFlightKind(unit) end

---@deprecated Archived alias for coe5.squareSideBattleStrength. Kept for older mods.
---@see coe5.squareSideBattleStrength
---@param player integer a player slot, -1 for any
---@param x integer a square's x
---@param y integer a square's y
---@param fort integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.squareSideStrength(player, x, y, fort) end

---@deprecated Archived alias for coe5.magicPathWeight. Kept for older mods.
---@see coe5.magicPathWeight
---@param path integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.spellPathWeight(path) end

---@deprecated Archived alias for coe5.unitFirstMagicPath. Kept for older mods.
---@see coe5.unitFirstMagicPath
---@param unit integer a living unit
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitFirstSpellPath(unit) end

---@deprecated Archived alias for coe5.aiUpdateBattleStrengthMemory. Kept for older mods.
---@see coe5.aiUpdateBattleStrengthMemory
---@param player integer a player slot
---@return boolean|nil result nil when refused (lua_refused)
function coe5.aiUpdateStrengthMemory(player) end

---@deprecated Archived alias for coe5.subtractUnitHp. Kept for older mods.
---@see coe5.subtractUnitHp
---@param unit integer a living unit
---@param damage integer integer
---@return boolean|nil result nil when refused (lua_refused)
function coe5.damageUnitPlain(unit, damage) end

---@deprecated Archived alias for coe5.commanderFullName. Kept for older mods.
---@see coe5.commanderFullName
---@param commander integer a commander with a living unit
---@return string|nil result nil when refused (lua_refused)
function coe5.commanderTheName(commander) end

---@deprecated Archived alias for coe5.takeItemNumber. Kept for older mods.
---@see coe5.takeItemNumber
---@param unit integer a living unit
---@param item integer an item
---@return integer|nil result nil when refused (lua_refused)
function coe5.unitRemoveItem(unit, item) end

---@deprecated Archived alias for coe5.playerBlessingValue. Kept for older mods.
---@see coe5.playerBlessingValue
---@param player integer a player slot
---@param ability integer|string an ability's word or id
---@return integer|nil result nil when refused (lua_refused)
function coe5.slotBlessingOf(player, ability) end

---@deprecated Archived alias for coe5.squareEffectiveTemperature. Kept for older mods.
---@see coe5.squareEffectiveTemperature
---@param x integer a square's x
---@param y integer a square's y
---@return integer|nil result nil when refused (lua_refused)
function coe5.squareEffectiveTemp(x, y) end

---@deprecated Archived alias for coe5.soleHumanPlayer. Kept for older mods.
---@see coe5.soleHumanPlayer
---@return integer|nil result nil when refused (lua_refused)
function coe5.loneHumanNow() end

---@deprecated Archived alias for coe5.soleHumanPlayer. Kept for older mods.
---@see coe5.soleHumanPlayer
---@return integer|nil result nil when refused (lua_refused)
function coe5.soleHumanSlot() end

---@deprecated Archived alias for coe5.factionPlayer. Kept for older mods.
---@see coe5.factionPlayer
---@param name string
---@return integer|nil player
function coe5.factionSlot(name) end

---@deprecated Archived alias for coe5.countPlayersWithController. Kept for older mods.
---@see coe5.countPlayersWithController
---@param controller integer integer
---@return integer|nil result nil when refused (lua_refused)
function coe5.countSlotsWithController(controller) end
