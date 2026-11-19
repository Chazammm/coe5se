#pragma once

typedef enum { LUA_CLASS_CONTENT, LUA_CLASS_PERSONAL, LUA_CLASS_OFF } LuaClass;

// ---- The sites a personal script may reach: one table, every site a row ---------------------------
#define PERSONAL_SITES(X) \
    X(SITE_ARMY_WAIT) X(SITE_BATTLE_DRAW) X(SITE_BATTLE_WAIT) X(SITE_FILE_BROWSER_HOVERED) X(SITE_FILE_BROWSER_WAIT) \
    X(SITE_FILE_BROWSER_ROW_DATE) X(SITE_GAME_MENU_SAVING_SECTION_END) X(SITE_GAME_MENU_WAIT) X(SITE_GAME_OVER_WAIT) \
    X(SITE_KEYBOARD_SHORTCUTS_WAIT) X(SITE_MAP_DRAW) X(SITE_MAP_MENU_BUTTON_TEXT) X(SITE_MAP_SCREEN_WAIT) X(SITE_MESSAGES_WAIT) \
    X(SITE_MESSAGE_BOX_WAIT) X(SITE_MOD_BANNER_DRAW) X(SITE_RECRUIT_WAIT) X(SITE_RITUAL_SCREEN_ROW) X(SITE_SQUARE_INFO_BUTTON_ROW) \
    X(SITE_SQUARE_INFO_SCREEN) X(SITE_SQUARE_INFO_WAIT) X(SITE_TOOLTIP_LINE_DRAW) X(SITE_TOP_BAR_INCOME) X(SITE_SQUARE_PANEL_WRAP) X(SITE_UNIT_LIST_WAIT) \
    X(SITE_VIEW_ARMY_WAIT) X(SITE_WARN_WAIT) X(SITE_CEREMONY_SCREEN_ROW) X(SITE_CEREMONY_SCREEN_WAIT)

static int isPersonalSite(CallSite site) {
#define PERSONAL_CASE(s) case s:
    switch (site) {
    PERSONAL_SITES(PERSONAL_CASE)
        return 1;
    default:
        return 0;
    }
#undef PERSONAL_CASE
}

#define PERSONAL_OBSERVE_SITES(X) \
    X(SITE_MODS_READ, "", '-') X(SITE_RITUAL_SCREEN_LOCATION_CHECK, "c---", '-') X(SITE_RITUAL_SCREEN_CAN_CAST, "c-", '-') X(SITE_CLIENT_TURN, "-", '-') \
    X(SITE_GAME_SAVE, "--", '-') X(SITE_CONQUER_SQUARE, "xy-", '-') X(SITE_BATTLE_ROUND, "", '-') X(SITE_DAMAGE_UNIT, "u-", '-') \
    X(SITE_DOATTACK, "u--", '-') X(SITE_EXECUTE_RITUAL, "u-xy--", '-') X(SITE_NEW_UNIT, "xy-c-", 'u') \
    X(SITE_REMOVE_UNIT, "u", '-') X(SITE_SQUARE_UPKEEP, "--xy", '-') X(SITE_RECRUIT_BUY, "--xy", '-') \
    X(SITE_PROMOTE_UNIT, "-u--", '-') X(SITE_MASTERY_TRANSFORM, "u", '-') X(SITE_BATTLE_SETUP, "xy-", '-') \
    X(SITE_BATTLE_SETUP_SIDE, "-xy--", '-') X(SITE_BATTLE_SETUP_AMBUSH, "-xy--", '-') X(SITE_BATTLE_ADVANCE_UNIT, "u-", '-') \
    X(SITE_BATTLE_TURN_EFFECTS, "u-", '-') X(SITE_BATTLE_TRANSFORM, "u--", '-') X(SITE_FOUND_ITEM, "xy-", '-') \
    X(SITE_REMOVE_DEAD_PLAYERS, "", '-') X(SITE_INQUISITION_SPAWN, "-", '-') X(SITE_ANIMAL_SUMMONING, "c-", '-') \
    X(SITE_ORDER_SLAVE_HUNT, "c", '-') X(SITE_ORDER_EAT_VILLAGE, "c", '-') X(SITE_PLANESHIFT_UNIT, "u--", '-') \
    X(SITE_NEW_PORTAL_PAIR, "xyxy-", '-') X(SITE_GEN_FLOOD_PLANE, "-", '-') X(SITE_UNIT_ADD_XP, "u-", '-') \
    X(SITE_USE_SPECIAL_SITE, "xyc", '-') X(SITE_REMOVE_TEMP_MEN, "", '-') X(SITE_OVERRUN_CHECK, "-xy", '-') \
    X(SITE_KILL_UNIT_999, "u", '-') X(SITE_CONTINUE_MOVE_ORDERS, "-c", '-') X(SITE_HUMAN_MULTI_MOVE, "cxy-", '-') \
    X(SITE_GROUP_MOVE, "-xy", '-') X(SITE_MOVE_ORDER_ROUTE, "cxy", '-') X(SITE_UNIT_REMOVE_ITEM, "u-", '-') \
    X(SITE_MEMORIZE_SPELL, "u-", '-') X(SITE_LEARN_RANDOM_SPELL, "u--", '-') X(SITE_GET_START_RITUALS, "c", '-') \
    X(SITE_COMMANDER_LEARN_RITUAL, "c-", '-') X(SITE_BOLSTER_BLESSING, "---", '-') X(SITE_ADD_PLAYER_BLESSING, "----", '-') \
    X(SITE_REMOVE_ENCHANT, "--xy", '-') X(SITE_RITUAL_PAY_COSTS, "-c---", '-') X(SITE_RITUAL_FAR_SUMMON_UNITS, "----xy", 'u') \
    X(SITE_EVENT_KILL_SQUARE, "xy", '-') X(SITE_GIVE_ITEM_BY_NAME, "u-", '-') X(SITE_TAKE_ITEM_BY_NAME, "u-", '-') \
    X(SITE_GIVE_RANDOM_ITEM, "u---", '-') X(SITE_SQUAREEVENT_START, "", '-') X(SITE_TERRAIN_PRESENT_RESCAN, "", '-') \
    X(SITE_EVENT_VAR_CHANGED, "-", '-') X(SITE_ALTER_TERRAIN, "-xy---", '-') X(SITE_RUIN_SQUARE, "-xy", '-') \
    X(SITE_DESTROY_GATEWAY, "-xy-", '-') X(SITE_DIG_TUNNEL_SQUARE, "-xy", '-') X(SITE_RESET_INCOME_CACHES, "", '-') \
    X(SITE_PLANESHIFT_COMMANDER, "c-", '-') X(SITE_UNIT_SWALLOW, "uu", '-') X(SITE_UNIT_UNSWALLOW, "u", '-') \
    X(SITE_LIFEFORCE_DRAIN_UNIT, "uu-", '-') X(SITE_UNIT_CRUSH, "u", '-') X(SITE_REMOVE_BAD_FOLLOWERS, "", '-') \
    X(SITE_RANDOM_TELEPORT, "u--", '-') X(SITE_KILL_SUNKEN_SHIPS, "xy", '-') X(SITE_CREATE_BOATS, "", '-') \
    X(SITE_BATTLE_TRAPS, "", '-') X(SITE_BATTLE_ENCHANT_TRAPS, "", '-') X(SITE_BATTLE_SUMMON_STRING, "", '-') \
    X(SITE_PAY_SPELL_LIFEFORCE, "", '-') X(SITE_CHECK_GAME_WON, "", '-') X(SITE_SPY_CAUGHT, "", '-') \
    X(SITE_TOGGLE_AUTO_RECRUIT, "-xy-", '-') X(SITE_BATTLE_SIEGE_UNIT, "u", '-') X(SITE_GATHER_SHIPS, "", '-') \
    X(SITE_ADVANCE_SHIPS, "", '-') X(SITE_TAINTED_RITUAL_EFFECT, "c-", '-') X(SITE_LOCATION_SWAP, "xyxy", '-') \
    X(SITE_RITUAL_ASK_HEMISPHERE, "", '-') X(SITE_ELIMINATE_PLAYER, "-", '-') X(SITE_CHECK_FOLLOWER_LINKS, "c", '-') \
    X(SITE_STEP_BURN_FOREST, "-xy", '-') X(SITE_ORDER_BURN_FOREST, "-c", '-') X(SITE_ORDER_CHOP_FOREST, "-c", '-') \
    X(SITE_LIFEFORCE_DRAIN_SQUARE, "-xy-", '-') X(SITE_SPEND_ALL_AP_AT, "-xy", '-') X(SITE_TELEPORT_UNIT, "uxy", '-') \
    X(SITE_UNIT_CLEAR_BENEFITS, "u", '-') X(SITE_GAME_MENU, "", '-') X(SITE_ARMY_COMMANDS_MENU, "c", '-') \
    X(SITE_CEREMONY_SCREEN, "c", '-') X(SITE_INCOME_DETAIL_SCREEN, "--", '-') X(SITE_BATTLE_CREATE_TREES, "xy", '-') \
    X(SITE_BATTLE_DEPLOY, "---", '-') X(SITE_BATTLE_FIGHT_MOVE, "u----", '-') X(SITE_BATTLE_FLY_MOVE, "u", '-') \
    X(SITE_BATTLE_FLEE, "u", '-') X(SITE_BATTLE_FEAR_CHECK, "", '-') X(SITE_BATTLE_CLOUD_STEP, "u", '-') \
    X(SITE_BATTLE_ENSNARE_CHECK, "u", '-') X(SITE_BATTLE_ENSNARE_RELEASE, "u", '-') X(SITE_BATTLE_WALL_ENCHANTS, "", '-') \
    X(SITE_BATTLE_WALL_EFFECTS, "", '-') X(SITE_BATTLE_START_SETUP, "", '-') X(SITE_BATTLE_TUNNEL_MOVE, "u", '-') \
    X(SITE_BATTLE_BANISH, "u", '-') X(SITE_BATTLE_DEPLOY_UNIT, "u--", '-') X(SITE_BATTLE_AFTER_FIGHT_UPKEEP, "xy", '-') \
    X(SITE_BATTLE_MR_ROLL, "u", '-') X(SITE_BATTLE_SUMMON_WEAPON, "", '-') X(SITE_BATTLE_GATE_SWAP, "", '-') \
    X(SITE_BATTLE_SPREAD_COLLAPSE, "", '-') X(SITE_UNIT_CURE_AFFLICTION, "u", '-') X(SITE_UNIT_HP_SHAPE_CHANGE, "u", '-') \
    X(SITE_BERSERK_END, "u", '-') X(SITE_UNIT_RANDOM_AFFLICTION, "u", '-') X(SITE_REVEAL_TERRAIN_ON_PLANE, "---", '-') \
    X(SITE_PLAY_SAMPLE, "--", '-') X(SITE_RANDOM_ITEM_PICK, "-----", '-') X(SITE_GEN_UPDATE_MOIST_WALLS, "-", '-') \
    X(SITE_USE_LIBRARY, "u", '-') X(SITE_VOID_INSANITY, "uxy", '-') X(SITE_LIBRARY_SCREEN, "u", '-') \
    X(SITE_CHOP_BURN_SCREEN, "", '-') X(SITE_SLAVE_HUNT_SCREEN, "", '-') X(SITE_UNIT_STATS_SCREEN, "u", '-') \
    X(SITE_ITEM_INFO_SCREEN, "-", '-') X(SITE_WEAPON_INFO_WINDOW, "-", '-') X(SITE_PLANE_INFO_SCREEN, "-", '-') \
    X(SITE_COMMANDER_SET_DONE, "", '-') X(SITE_SELECT_NEXT_COMMANDER, "", '-') X(SITE_SELECT_PREV_COMMANDER, "", '-') \
    X(SITE_CLEAN_SQUARE, "xy", '-') X(SITE_ADJUST_FOR_SNOW, "xy--", '-') X(SITE_START_TUNE, "--", '-') \
    X(SITE_SQUARE_SET_OWNER, "xy-", '-') X(SITE_ADD_BENEFIT, "u", '-') X(SITE_MOVE_STEP_AFTER, "", '-') \
    X(SITE_NUMBER_BOX, "--", '-') X(SITE_ENCHANTS_DESTROYED_AT, "-xy", '-')

#define PERSONAL_CLOSED_SITES(X) \
    X(SITE_MAIN_MENU_WAIT) X(SITE_MAIN_MENU_PLAY_GAME) X(SITE_PLAY_GAME_NEW_WORLD) X(SITE_NEW_WORLD_MAP_SCREEN) \
    X(SITE_NEW_WORLD_INIT_PARTICIPANTS) X(SITE_NEW_WORLD_CREATING_WORLD) X(SITE_MAP_SETTINGS_WAIT) X(SITE_PARTICIPANTS_WAIT) \
    X(SITE_PARTICIPANTS_DIFFICULTY_WAIT) X(SITE_PARTICIPANTS_BUTTON_ROW) X(SITE_LOAD_GAME_BROWSER) X(SITE_MAP_EDITOR_WAIT) \
    X(SITE_BATTLE_EDITOR_WAIT) X(SITE_AI_EXPLORE) X(SITE_AI_RALLY) X(SITE_AI_FRIENDLY_MOVE) X(SITE_AI_CONQUER) \
    X(SITE_AI_FAR_EXPLORE) X(SITE_AI_RITUAL_CANDIDATE) X(SITE_AI_RECRUIT) X(SITE_AI_GATHER_TROOPS) X(SITE_AI_PICK_UP_LONER) \
    X(SITE_AI_SPLIT_ARMY) X(SITE_AI_MOVE_COMSTR) X(SITE_AI_DANGEROUS_STEP) X(SITE_AI_MAKE_GUARDS) X(SITE_AI_RITUAL_TARGET) \
    X(SITE_DECORATE_SQUARE) X(SITE_CEREMONY_PERFORM) X(SITE_CEREMONY_COSTS) X(SITE_SEAL_BREAK) X(SITE_SEAL_EVENT) \
    X(SITE_MAP_EDITOR) X(SITE_MAP_EDITOR_DRAW) X(SITE_UNIT_ARMOR) X(SITE_UNIT_RETURNS) X(SITE_AI_COUNT_CITADELS) \
    X(SITE_MOVE_INDEPENDENT_ARMY) X(SITE_SQUARE_CITADEL) X(SITE_SQUARE_FORT_PARTS) X(SITE_SQUARE_WATERLIKE) \
    X(SITE_SQUARE_OPEN_LAND) X(SITE_SQUARE_LIBRARY_LEVEL) X(SITE_GAME_START) X(SITE_MONTH_START) X(SITE_PLAYER_TURN_START) \
    X(SITE_HUMAN_TURN) X(SITE_AI_TURN) X(SITE_PLAYER_TURN_END) X(SITE_GAME_OVER) X(SITE_GAME_LOAD) X(SITE_AFTER_GAME_TURN) \
    X(SITE_DO_ALL_FIGHTS) X(SITE_MOVE_ALL_INDEPENDENTS) X(SITE_FIGHT_THE_FIGHT) X(SITE_BATTLE_SPELL_PICK) X(SITE_WEAPON_HIT) \
    X(SITE_BATTLE_TARGET_CHECK) X(SITE_TURN_UNIT_UPKEEP) X(SITE_CREATE_CITADEL) X(SITE_UPDATE_VOID) X(SITE_VOID_SHUFFLE) \
    X(SITE_PLANE_DISCOVERED_PASS) X(SITE_MOVE_INFERNO_PLAYER) X(SITE_MOVE_HADES_PLAYER) X(SITE_MOVE_CELESTIAL_PLAYER) \
    X(SITE_POPULATE_CELESTIAL) X(SITE_POPULATE_SKY) X(SITE_POPULATE_AGARTHA) X(SITE_POPULATE_INFERNO) X(SITE_POPULATE_HADES) \
    X(SITE_POPULATE_AZTLAN) X(SITE_POPULATE_PRIMAL) X(SITE_POPULATE_ELEMENTAL) X(SITE_PLANE_RISE_TO_ELYSIUM) X(SITE_ENLARGE_PLANE) \
    X(SITE_ADDPLANE) X(SITE_LOAD_MAP_PLANE) X(SITE_LONER_STEP) X(SITE_MOVE_WANDERING_MONSTERS) X(SITE_MOVE_PLAYER_STUPIDS) \
    X(SITE_DAMAGE_UNIT_PLAIN) X(SITE_PRIEST_FREESPAWN) X(SITE_SHOP_OFFER_MESSAGE) X(SITE_WORLD_FIRE_UPDATE) X(SITE_CONSCRIPTION) \
    X(SITE_GAME_STATISTICS) X(SITE_MONTH_SQUARE_RESET) X(SITE_ROLL_CLASS) X(SITE_TEAM_START_LOCATIONS) X(SITE_TRIM_TEAM_LOCATIONS) \
    X(SITE_SWAP_TEAM_LOCATIONS) X(SITE_CREATE_CITADELS_STEP) X(SITE_FIX_PORTALS) X(SITE_VOID_FLUCTUATIONS) X(SITE_MIST_SPREAD) \
    X(SITE_TEMPERATURE_CHANGE) X(SITE_SPECIAL_SITE_POPULATE) X(SITE_UNIT_CATCH_FIRE) X(SITE_OVERPOP_CULL) X(SITE_MONKEY_CLOAK) \
    X(SITE_RECYCLE_UNITS) X(SITE_POP_HORRORS) X(SITE_KILL_TERRAIN_UNITS) X(SITE_WORLD_NEWS) X(SITE_SEASON_NEWS) \
    X(SITE_CHANGE_LOYALTY) X(SITE_ALTER_TERRAIN_RECT) X(SITE_RESPAWN_VOID) X(SITE_MOVE_ENCHANTS) X(SITE_MOVE_GEMS) \
    X(SITE_APE_POSSESSION) X(SITE_GET_TITHES) X(SITE_FIND_START_SQUARE) X(SITE_TROLL_FOREST_SPAWN) X(SITE_AI_MAKE_DEFENDERS) \
    X(SITE_AI_REDISTRIBUTE_ITEMS) X(SITE_AI_SHIPS) X(SITE_AI_SHIP_DROP_CREW) X(SITE_AI_SHIP_TAKE_BOAT) X(SITE_AI_SHIP_TAKE_CREW) \
    X(SITE_SWAP_SQUARES) X(SITE_BATTLE_AFTERMATH_RETURN) X(SITE_NEAR_TERRAIN_SQUARE) X(SITE_GROVE_REVERSION) \
    X(SITE_PUT_RARE_SQUARE) X(SITE_CREATE_RANDOM_MAP) X(SITE_WORLD_PART_TERRAIN) X(SITE_MAPGEN_REMOVE_SMALL_ISLANDS) \
    X(SITE_MAPGEN_RIVER_CROSSINGS) X(SITE_MAPGEN_ANCIENT_FOREST) X(SITE_MAPGEN_CLEAR_AREA) X(SITE_MAPGEN_ADJUST_TEMPERATURE) \
    X(SITE_MARK_RIVER) X(SITE_GEN_DIAGONAL_FIX) X(SITE_GEN_TERRAIN_BLOB) X(SITE_CREATE_CELESTIAL_PLANE) X(SITE_CREATE_SKY_PLANE) \
    X(SITE_CREATE_AGARTHA_PLANE) X(SITE_CREATE_INFERNO_PLANE) X(SITE_CREATE_HADES_PLANE) X(SITE_CREATE_ELEMENTAL_PLANE) \
    X(SITE_LOAD_AZTLAN_PLANE) X(SITE_LOAD_PRIMAL_PLANE) X(SITE_AUTO_RECRUIT) X(SITE_ADD_GROUND_ITEMS) X(SITE_WANDERER_PICK_TARGET) \
    X(SITE_WANDERER_PICK_UP) X(SITE_SET_HOLDIT) X(SITE_BATTLE_INTRO_SCREEN) X(SITE_RUN_HATREDS) X(SITE_SANDSTORM_CLOUD_UPDATE) \
    X(SITE_TAINTED_MAGIC_PASS) X(SITE_HERO_ARRIVAL) X(SITE_TEAMLESS_OWN_TEAM) X(SITE_SOCIETY_PRESETS) X(SITE_STORM_SPAWN) \
    X(SITE_WILD_SPAWN) X(SITE_REVELS_SPAWN) X(SITE_HORROR_LORD_PICK) X(SITE_UNIT_BATTLE_STRENGTH) X(SITE_SQUARE_BATTLE_STRENGTH) \
    X(SITE_COMMANDER_BATTLE_STRENGTH) X(SITE_PLAYER_ABILITY) X(SITE_UNIT_STATE_FLAGS) X(SITE_ARMY_MOVE_FLAGS) X(SITE_UNIT_PATH_LEVEL) \
    X(SITE_UNIT_VISIBLE_TO) X(SITE_RESOURCE_AVAILABLE) X(SITE_UNIT_RESISTANCES) X(SITE_UNIT_FLAGS) \
    X(SITE_RITUAL_CAN_CAST) X(SITE_RITUAL_LOCATION_CHECK) X(SITE_PLAYER_GOLD_INCOME) X(SITE_PLAYER_IRON_INCOME) \
    X(SITE_PLAYER_HERBS_INCOME) X(SITE_PLAYER_FUNGUS_INCOME) X(SITE_PLAYER_SACRIFICES_INCOME) X(SITE_PLAYER_HANDS_INCOME) \
    X(SITE_PLAYER_WEED_INCOME) X(SITE_PLAYER_TRADE_INCOME) X(SITE_PLAYER_RELICS_INCOME) X(SITE_PLAYER_LIFEFORCE_INCOME) \
    X(SITE_PLAYER_GEM_INCOME) X(SITE_TRADE_INCOME_APPLY) X(SITE_UNIT_DAMAGE_BONUS) X(SITE_UNIT_MORALE) X(SITE_UNIT_MR) \
    X(SITE_UNIT_BATTLE_RANK) X(SITE_UNIT_MAX_HP) X(SITE_UNIT_PATH_BONUS) X(SITE_UNIT_BLINDNESS) X(SITE_UNIT_INCAPACITATED) \
    X(SITE_UNIT_FIRE_RESISTANCE) X(SITE_UNIT_BANISHABLE) X(SITE_BANISH_DESTINATION) X(SITE_IMMORTAL_REFORM) X(SITE_UNIT_HEAL_RATE) X(SITE_BATTLE_TARGET_SQUARE) X(SITE_RITUAL_COST_PERCENT)

static const char *personalObserveKinds(CallSite site, char *result) {
#define PERSONAL_OBSERVE_CASE(s, kinds, res) \
    case s:                                 \
        if (result) *result = res;          \
        return kinds;
    switch (site) {
    PERSONAL_OBSERVE_SITES(PERSONAL_OBSERVE_CASE)
    default:
        return NULL;
    }
#undef PERSONAL_OBSERVE_CASE
}

// ---- The calls a personal script may make ---------------------------------------------------------
static const char *const PERSONAL_CALLS[] = {
    "coe5.actionKey", "coe5.addActionKey", "coe5.addCommanderLine", "coe5.addGameMenuLine", "coe5.addMainMenuButton",
    "coe5.addModCommand", "coe5.addScreen", "coe5.addTraitLine", "coe5.afflictionImmune", "coe5.aiActions",
    "coe5.aiCommanderBoatPlaces", "coe5.aiCountAllyTargetsNear", "coe5.aiCountPlanners", "coe5.aiCountTroops",
    "coe5.aiGroupBoatTotal", "coe5.aiItemUseRating", "coe5.aiItemValue", "coe5.aiNearestTerrain",
    "coe5.aiPlanSquareBonus", "coe5.aiPortsBoatTotals", "coe5.aiRitualBad", "coe5.aiRitualBadTime",
    "coe5.aiRitualMaster", "coe5.aiShipsAt", "coe5.aiThreatNear", "coe5.aiUnitStays", "coe5.armies",
    "coe5.armyAbilityTotal", "coe5.armyAtForPlayer", "coe5.armyBurnValue", "coe5.battleAdvanceRank",
    "coe5.battleCanStand", "coe5.battleCountRank", "coe5.battleEnemyExtent", "coe5.battleFearAt", "coe5.battleInRange",
    "coe5.battleKillerMove", "coe5.battleLineOfSight", "coe5.battleLocalLeadership", "coe5.battleNearestEnemy",
    "coe5.battleObjectAt", "coe5.battlePathfind", "coe5.battleSquareAbility", "coe5.battleUnitAt",
    "coe5.battleUnitDistance", "coe5.battleWallCover", "coe5.battleWinner", "coe5.builtinOfferChance",
    "coe5.builtinOffers", "coe5.canOwnSquare", "coe5.centerMap", "coe5.changedBy", "coe5.chooseLocation",
    "coe5.classAbilityNth", "coe5.classPicker", "coe5.classRecruits", "coe5.commanderApLeft",
    "coe5.commanderKnowsRitual", "coe5.commanderLedCount", "coe5.commanderMayAct", "coe5.commanderTheName",
    "coe5.confirm", "coe5.copyText", "coe5.countCommandersWithAp", "coe5.countFreeCommanders", "coe5.countKnownSpells",
    "coe5.countLandNear", "coe5.countMemorizedOfType", "coe5.countMemorizedSpells", "coe5.countPlayerPortals",
    "coe5.countSideUnits", "coe5.countSlotsWithController", "coe5.countSpells", "coe5.countTerrainCode",
    "coe5.countTerrainOnPlane", "coe5.countUnits", "coe5.countUnitsAt", "coe5.countWaterNear", "coe5.dataFolder",
    "coe5.deleteIni", "coe5.factionSlot", "coe5.findAfflictedUnitAt", "coe5.findClass", "coe5.findItemHolderAt",
    "coe5.findMonster", "coe5.findRitual", "coe5.findSpell", "coe5.findUnitAt", "coe5.get", "coe5.goldBonusPercent",
    "coe5.groundFromNeighbours", "coe5.guildCount", "coe5.hideMainMenuButton", "coe5.illusionTerrain",
    "coe5.ironBonusPercent", "coe5.isSpellAttack", "coe5.itemAbility", "coe5.itemAbilityNth", "coe5.itemCastableSpell",
    "coe5.listIni", "coe5.loadedContentOrigin", "coe5.localPlayer", "coe5.log", "coe5.loneHumanNow", "coe5.lookName", "coe5.magicPathWeight", "coe5.mapView",
    "coe5.mayEnterSquare", "coe5.memorizeAllowance", "coe5.memorizedSlot", "coe5.monsterHasRanged",
    "coe5.monsterNameType", "coe5.monsterPicker", "coe5.monsterVariant", "coe5.nearTerrainSquare",
    "coe5.nearestTerrainDistance", "coe5.nearestUnitAbilityIs", "coe5.netRole", "coe5.numberBox", "coe5.observe",
    "coe5.openArmyMenu", "coe5.openCeremonies", "coe5.openChopBurn", "coe5.openIncomeDetail", "coe5.openItemInfo",
    "coe5.openLibrary", "coe5.openPlaneInfo", "coe5.openRitualScreen", "coe5.openSlaveHunt", "coe5.openSquareInfo",
    "coe5.openWeaponInfo", "coe5.planeAbilitySum", "coe5.planeIndex", "coe5.planeIsReachable", "coe5.planeOrderStep",
    "coe5.planeSameSpot", "coe5.planesAlike", "coe5.playSample", "coe5.playerClassAbility", "coe5.playerHasCitadel",
    "coe5.playerHasCommander", "coe5.playerIncomeByType", "coe5.playerRitualFlags", "coe5.portalExits",
    "coe5.portalsLinkedTo", "coe5.randomEventsOff", "coe5.readIni", "coe5.rebindKey", "coe5.recruitSquares", "coe5.resetIncomeCaches",
    "coe5.ritualCastableAnywhere", "coe5.ritualEffectFirst", "coe5.ritualEffectOr", "coe5.ritualMayLearn",
    "coe5.ritualPointless", "coe5.route", "coe5.selectNextCommander", "coe5.selectPrevCommander", "coe5.set",
    "coe5.setting", "coe5.shopOffers", "coe5.showMessageBox", "coe5.slotBlessingOf", "coe5.soullessForm",
    "coe5.spellInfo", "coe5.spellPathWeight", "coe5.spellUseful", "coe5.squareDefaultGround",
    "coe5.squareEffectiveTemp", "coe5.squareGoldIncome", "coe5.squareHasSeenUnit", "coe5.squareIncome",
    "coe5.squareIsCoast", "coe5.squareIsNorth", "coe5.squareIsSky", "coe5.squareIsSouth", "coe5.squareIsWet",
    "coe5.squareLifeforce", "coe5.squareMoveCost", "coe5.squareShownName", "coe5.squareSideBattleStrength", "coe5.squareSideStrength",
    "coe5.squareSideWater", "coe5.squareTeamAbilitySum", "coe5.squareUnits", "coe5.squareUnitsAbility",
    "coe5.startLikesFilter", "coe5.startPlacementScore", "coe5.startTune", "coe5.terrainBurnableAt",
    "coe5.terrainMatch", "coe5.terrainRuinForm", "coe5.tradeBonusPercent", "coe5.translate", "coe5.turn", "coe5.ui.bindEscape",
    "coe5.ui.bindKey", "coe5.ui.button", "coe5.ui.buttonRow", "coe5.ui.checkbox", "coe5.ui.choice", "coe5.ui.columnX",
    "coe5.ui.drawItemSprite", "coe5.ui.drawLine", "coe5.ui.drawMonsterSprite", "coe5.ui.drawRect", "coe5.ui.drawResourceIcon", "coe5.ui.drawText", "coe5.ui.listBegin",
    "coe5.ui.listEnd", "coe5.ui.listRow", "coe5.ui.monsterRow", "coe5.ui.nameBox", "coe5.ui.resourceIcon",
    "coe5.ui.rowBegin", "coe5.ui.rowEnd", "coe5.ui.screenBegin", "coe5.ui.screenMusic", "coe5.ui.screenWidth",
    "coe5.ui.sectionBegin", "coe5.ui.sectionEnd", "coe5.ui.sectionHeader", "coe5.ui.setupScreenWidth", "coe5.ui.text",
    "coe5.ui.textWidth", "coe5.ui.tooltip", "coe5.ui.wait", "coe5.unitAbilitySum", "coe5.unitAcidResistance",
    "coe5.unitAfflictionCount", "coe5.unitBattleFast", "coe5.unitBattleRank", "coe5.unitBattleSlowness",
    "coe5.unitColdResistance", "coe5.unitFireResistance", "coe5.unitFirstMagicPath", "coe5.unitFirstSpellPath", "coe5.unitFlightKind", "coe5.unitFlightType",
    "coe5.unitHasSpellAttack", "coe5.unitHasWaterMove", "coe5.unitHatesSquare", "coe5.unitInContact",
    "coe5.unitIncapacitated", "coe5.unitIsDemon", "coe5.unitIsHuge", "coe5.unitIsUndead", "coe5.unitLifeless",
    "coe5.unitLikesSquare", "coe5.unitLovesSquare", "coe5.unitMagicResistance", "coe5.unitMayBeOnLand",
    "coe5.unitMindless", "coe5.unitMorale", "coe5.unitNoLand", "coe5.unitPathBonus", "coe5.unitPlane",
    "coe5.unitPoisonResistance", "coe5.unitPrimable", "coe5.unitRegenPercent", "coe5.unitShockResistance",
    "coe5.unitSpeedClass", "coe5.unitSpellWeaponLevel", "coe5.viewArmy", "coe5.viewPlane", "coe5.wandererAt",
    "coe5.weaponExtra", "coe5.weaponRangeFor", "coe5.wetNeighbours", "coe5.wrap", "coe5.writeIni", "require",
};
#define PERSONAL_CALL_COUNT ((int)(sizeof PERSONAL_CALLS / sizeof PERSONAL_CALLS[0]))
static const char *const PERSONAL_CLOSED_CALLS[] = {
    "coe5.addAffliction", "coe5.addBattleMap", "coe5.addBenefit", "coe5.addFaction", "coe5.addMapEditorLine",
    "coe5.addPlane", "coe5.addPlayerBlessing", "coe5.addShopOffer", "coe5.addTestFieldUnits", "coe5.addTransport", "coe5.afflictedUnits", "coe5.afflictionEffect",
    "coe5.afflictionTurn", "coe5.afflictionValue", "coe5.aiBuyOffer", "coe5.aiConquerStep", "coe5.aiExploreStep",
    "coe5.aiFarExploreStep", "coe5.aiFriendlyMoveStep", "coe5.aiMakeDefenders", "coe5.aiMakeGuardsAt", "coe5.aiRallyStep",
    "coe5.aiRandomEnemy", "coe5.aiRedistributeItems", "coe5.aiShipDropCrew", "coe5.aiShipTakeBoat", "coe5.aiShipTakeCrew",
    "coe5.aiShips", "coe5.aiUpdateBattleStrengthMemory", "coe5.aiUpdateStrengthMemory", "coe5.alterTerrain", "coe5.autoName", "coe5.battleBanishUnit", "coe5.battleCellToScreen",
    "coe5.battleScreenToCell", "coe5.battleSummon", "coe5.battleView", "coe5.bolsterBlessing", "coe5.changeLoyalty", "coe5.checkFollowerLinks",
    "coe5.chooseMemorized", "coe5.cleanSquare", "coe5.clearPlaneTint", "coe5.closeTestField", "coe5.commanderLearnRitual",
    "coe5.commanderUseItems", "coe5.conscription", "coe5.cureAffliction", "coe5.damageUnitPlain", "coe5.declaredPlanes",
    "coe5.destroyEnchantsAt", "coe5.destroyGateway", "coe5.digTunnelSquare", "coe5.drawTestField", "coe5.endPreviewWorld",
    "coe5.fightTestField", "coe5.fireTestField", "coe5.fixPortals", "coe5.forestShepherdSpawn", "coe5.gen.addGroundItems",
    "coe5.gen.decorateSquare", "coe5.gen.drawRiver", "coe5.gen.findCaveSpot", "coe5.gen.findSpotByNeighbours",
    "coe5.gen.fixDiagonals", "coe5.gen.floodPlane", "coe5.gen.heatBlob", "coe5.gen.linkNewPortal", "coe5.gen.markAt",
    "coe5.gen.markBlob", "coe5.gen.markNearTerrain", "coe5.gen.markRect", "coe5.gen.markRiver", "coe5.gen.markVoidNear",
    "coe5.gen.markWalk", "coe5.gen.marksToTerrain", "coe5.gen.marksToTerrainChance", "coe5.gen.random",
    "coe5.gen.randomMarkedSquare", "coe5.gen.randomSquareOfTerrain", "coe5.gen.reachesElysium", "coe5.gen.refuseSmallPlane",
    "coe5.gen.setFixedTemperature", "coe5.gen.setGround", "coe5.gen.setMark", "coe5.gen.setPortalPair",
    "coe5.gen.setTemperatures", "coe5.gen.setTerrain", "coe5.gen.specBlob", "coe5.gen.spreadTerrain", "coe5.gen.stampPiece",
    "coe5.gen.terrainBlob", "coe5.gen.terrainFlags", "coe5.gen.terrainNear", "coe5.gen.updateMoistWalls",
    "coe5.getStartRituals", "coe5.giveAffliction", "coe5.giveItem", "coe5.giveRandomItem", "coe5.groveReversion",
    "coe5.hasAffliction", "coe5.heroArrival", "coe5.initSquareCorpses", "coe5.initSquareGems", "coe5.inquisitionSpawn",
    "coe5.inspectTestFieldFinalUnit", "coe5.inspectTestFieldItem", "coe5.inspectTestFieldUnit", "coe5.keepTraceLines",
    "coe5.killSquare", "coe5.killUnit", "coe5.learnLevelRituals", "coe5.learnRandomSpell", "coe5.learnStartSpells",
    "coe5.loadGame", "coe5.memorizeSpell", "coe5.memorizeTestFieldSpell", "coe5.modEvents", "coe5.moveEnchants",
    "coe5.moveGems", "coe5.moveTestFieldUnit", "coe5.newPortalPair", "coe5.newUnits", "coe5.onPortalCrossing",
    "coe5.openTestField", "coe5.orderBurnForest", "coe5.orderChopForest", "coe5.orderEatVillage", "coe5.orderSlaveHunt",
    "coe5.planRitual", "coe5.planeRiseToElysium", "coe5.planeShiftUnit", "coe5.popHorrors", "coe5.previewArmySource",
    "coe5.previewEventResults", "coe5.previewLineIndex", "coe5.previewLineResults", "coe5.previewScriptResults",
    "coe5.previewSquareSource", "coe5.previewWorldActive", "coe5.priestFreespawn", "coe5.putRareSquare",
    "coe5.randomMemorizedSpell", "coe5.randomRitual", "coe5.randomSpell", "coe5.randomSpellNotKnown",
    "coe5.randomSquareOfTerrain", "coe5.randomTeleport", "coe5.recruitScreen", "coe5.releaseFollower",
    "coe5.removeAffliction", "coe5.removeBadFollowers", "coe5.removeEnchant", "coe5.removeTestFieldUnit",
    "coe5.repickMemorized", "coe5.replaceBuiltinOffer", "coe5.reportBattlesTo", "coe5.restartGame", "coe5.restartPhase",
    "coe5.restoreSight", "coe5.revealDisc", "coe5.revealMap", "coe5.revealPortalExits", "coe5.revealTerrainOnPlane",
    "coe5.revelsSpawn", "coe5.ruinSquare", "coe5.runAiTurn", "coe5.runHatreds", "coe5.runModEvent",
    "coe5.sandstormCloudUpdate", "coe5.saveNewMod", "coe5.scrapRitualPlan", "coe5.selectedCommanderDone",
    "coe5.sendMessage", "coe5.setAfflictionTurn", "coe5.setAfflictionValue", "coe5.setBattleScatter", "coe5.setPlaneShape",
    "coe5.setPlaneTint", "coe5.setPreviewDocks", "coe5.setPreviewHoverText", "coe5.setPreviewKeyField",
    "coe5.setPreviewLineEdits", "coe5.setRandomEventsOff", "coe5.setSeenDisc", "coe5.setSetupAutomated",
    "coe5.setSquareOwner", "coe5.setTerrainLooks", "coe5.setTestFieldConfiguration", "coe5.setTestFieldTextFocus",
    "coe5.setupAutomated", "coe5.showSight", "coe5.spawnCommander", "coe5.spawnUnit", "coe5.spendAllApAt",
    "coe5.squareHold", "coe5.startPreviewWorld", "coe5.stopTestFight", "coe5.stormSpawn", "coe5.swapLocations",
    "coe5.swapSquares", "coe5.takeItem", "coe5.takePreviewFieldKeys", "coe5.takeTestFieldTextEvents",
    "coe5.teleportCommander", "coe5.teleportSquare", "coe5.teleportUnit", "coe5.testBattle",
    "coe5.testFieldEditorCapabilities", "coe5.testFieldFinalState", "coe5.testFieldFootprint", "coe5.testFieldLifecycle",
    "coe5.testFieldLoadoutState", "coe5.testFieldPlacementPreflight", "coe5.testFieldPointerButtons",
    "coe5.testFieldSpellState", "coe5.testFieldState", "coe5.testFieldTerrains", "coe5.testFieldUnitState",
    "coe5.toggleAutoRecruit", "coe5.transferFollower", "coe5.trollForestSpawn", "coe5.unitAddXp", "coe5.unitAfflictions",
    "coe5.unitRandomAffliction", "coe5.unitRemoveItem", "coe5.unitRemoveTempAbility", "coe5.unmemorizeSpell",
    "coe5.useSpecialSite", "coe5.wildSpawn", "coe5.worldNews",
};
#define PERSONAL_CLOSED_CALL_COUNT ((int)(sizeof PERSONAL_CLOSED_CALLS / sizeof PERSONAL_CLOSED_CALLS[0]))

static int isPersonalCall(const char *name) {
    int low = 0, high = PERSONAL_CALL_COUNT - 1;
    while (low <= high) {
        int middle = (low + high) / 2, order = strcmp(name, PERSONAL_CALLS[middle]);
        if (!order) return 1;
        if (order < 0) high = middle - 1;
        else low = middle + 1;
    }
    return 0;
}

static int isPersonalSetValue(const char *name) {
    static const char *const PERSONAL_SET_VALUES[] = {"bannerFade", "battleGrid", "fullscreen", "hoveredId", "mouseButtons"};
    for (size_t i = 0; i < sizeof PERSONAL_SET_VALUES / sizeof PERSONAL_SET_VALUES[0]; i++)
        if (!strcmp(name, PERSONAL_SET_VALUES[i])) return 1;
    return 0;
}

// ---- The calls that hold the game until someone answers -------------------------------------------
typedef enum { LUA_BLOCKING_NONE, LUA_BLOCKING_NIL, LUA_BLOCKING_FALSE } LuaBlockingCall;

static LuaBlockingCall luaBlockingCallOf(const char *name) {
    static const char *const SCREENS[] = {
        "coe5.chooseLocation", "coe5.classPicker", "coe5.commanderUseItems", "coe5.monsterPicker", "coe5.numberBox", "coe5.openArmyMenu",
        "coe5.openCeremonies", "coe5.openChopBurn", "coe5.openIncomeDetail", "coe5.openItemInfo", "coe5.openLibrary", "coe5.openPlaneInfo",
        "coe5.openRitualScreen", "coe5.openSlaveHunt", "coe5.openSquareInfo", "coe5.openWeaponInfo", "coe5.recruitScreen", "coe5.viewArmy",
        "coe5.viewPlane",
    };
    if (!strcmp(name, "coe5.showMessageBox") || !strcmp(name, "coe5.confirm")) return LUA_BLOCKING_FALSE;
    if (!strncmp(name, "coe5.ui.", 8)) return LUA_BLOCKING_NIL;
    for (size_t i = 0; i < sizeof SCREENS / sizeof SCREENS[0]; i++)
        if (!strcmp(name, SCREENS[i])) return LUA_BLOCKING_NIL;
    return LUA_BLOCKING_NONE;
}

static int luaCallWaitsForPlayer(const char *name) {
    if (!strncmp(name, "coe5.ui.", 8)) return !strcmp(name, "coe5.ui.screenBegin") || !strcmp(name, "coe5.ui.wait");
    return luaBlockingCallOf(name) != LUA_BLOCKING_NONE;
}

// ---- The decisions ------------------------------------------------------------------------------
static int luaStartsFor(NetRole role, LuaClass cls) {
    switch (role) {
    case NET_ROLE_SINGLE: return 1;
    case NET_ROLE_HOST:
    case NET_ROLE_CLIENT: return cls != LUA_CLASS_OFF;
    default: return 0;
    }
}

static int luaSiteOpenFor(NetRole role, LuaClass cls, CallSite site, int loading) {
    switch (role) {
    case NET_ROLE_SINGLE: return 1;
    case NET_ROLE_HOST: return cls == LUA_CLASS_PERSONAL ? isPersonalSite(site) : cls == LUA_CLASS_CONTENT;
    case NET_ROLE_CLIENT:
        if (loading) return 0;
        return cls == LUA_CLASS_PERSONAL ? isPersonalSite(site) : cls == LUA_CLASS_CONTENT;
    default: return 0;
    }
}

static int luaCallbackOpenFor(NetRole role, LuaClass cls, int loading) {
    switch (role) {
    case NET_ROLE_SINGLE: return 1;
    case NET_ROLE_HOST: return cls == LUA_CLASS_CONTENT || cls == LUA_CLASS_PERSONAL;
    case NET_ROLE_CLIENT: return !loading && (cls == LUA_CLASS_PERSONAL || cls == LUA_CLASS_CONTENT);
    default: return 0;
    }
}

static int luaObserverOpenFor(NetRole role, LuaClass cls, CallSite site, int loading) {
    if ((role == NET_ROLE_CLIENT || role == NET_ROLE_HOST) && cls == LUA_CLASS_PERSONAL && !personalObserveKinds(site, NULL)) return 0;
    return luaCallbackOpenFor(role, cls, loading);
}

static int luaGameLogicOpenFor(NetRole role, LuaClass cls, int loading, int inTurn) {
    return role == NET_ROLE_SINGLE || (role == NET_ROLE_HOST && cls == LUA_CLASS_CONTENT) ||
           (role == NET_ROLE_CLIENT && cls == LUA_CLASS_CONTENT && !loading && inTurn);
}

static int luaModsReadOpenFor(NetRole role, LuaClass cls) { return luaStartsFor(role, cls); }

static int isDataFileRead(const char *name) { return !strcmp(name, "coe5.readIni") || !strcmp(name, "coe5.listIni"); }

static int luaModCommandOpenFor(NetRole role, LuaClass cls, int level) {
    switch (role) {
    case NET_ROLE_SINGLE: return 1;
    case NET_ROLE_HOST: return cls == LUA_CLASS_CONTENT;
    case NET_ROLE_CLIENT: return level == 2 && cls != LUA_CLASS_OFF;
    default: return 0;
    }
}

static const char *luaCallRefusalFor(NetRole role, LuaClass cls, const char *name, int inTurn) {
    if (role == NET_ROLE_SINGLE) return NULL;
    if (role == NET_ROLE_OFFICIAL) return "official_game";
    if (cls == LUA_CLASS_OFF) return "nomultiplayer";
    if (cls == LUA_CLASS_PERSONAL) {
        if (!isPersonalCall(name)) return "personal_mod";
        if (role == NET_ROLE_HOST) return luaCallWaitsForPlayer(name) ? "lan_host" : NULL;
        return !inTurn && luaCallWaitsForPlayer(name) ? "wait_screen" : NULL;
    }
    if (isDataFileRead(name)) return "lan_local_data";
    if (luaBlockingCallOf(name) == LUA_BLOCKING_NONE) return NULL;
    if (role == NET_ROLE_HOST) return "lan_host";
    return inTurn ? NULL : "content_on_client";
}

// ---- What a content script's write on a client reaches ------------------------------------------
typedef enum { CLIENT_WRITE_STAYS, CLIENT_WRITE_TRAVELS, CLIENT_WRITE_LOST } ClientWriteKind;
typedef enum { WRITE_OWNER_NONE, WRITE_OWNER_PLAYER, WRITE_OWNER_UNIT, WRITE_OWNER_COMMANDER } ClientWriteOwner;
typedef struct { const char *name; ClientWriteKind kind; ClientWriteOwner owner; int arg; } ClientWrite;

#define WRITE_TRAVELS(name, owner, arg) {name, CLIENT_WRITE_TRAVELS, WRITE_OWNER_##owner, arg}
#define WRITE_LOST(name) {name, CLIENT_WRITE_LOST, WRITE_OWNER_NONE, 0}
#define WRITE_STAYS(name) {name, CLIENT_WRITE_STAYS, WRITE_OWNER_NONE, 0}
static const ClientWrite CLIENT_WRITE_CALLS[] = {
    WRITE_TRAVELS("coe5.addAffliction", UNIT, 1), WRITE_TRAVELS("coe5.addBenefit", UNIT, 1), WRITE_LOST("coe5.addPlayerBlessing"),
    WRITE_TRAVELS("coe5.alterTerrain", NONE, 0), WRITE_TRAVELS("coe5.battleBanishUnit", UNIT, 1), WRITE_TRAVELS("coe5.battleSummon", PLAYER, 2), WRITE_LOST("coe5.bolsterBlessing"), WRITE_LOST("coe5.changeLoyalty"),
    WRITE_TRAVELS("coe5.checkFollowerLinks", COMMANDER, 1), WRITE_TRAVELS("coe5.chooseMemorized", UNIT, 1), WRITE_LOST("coe5.cleanSquare"),
    WRITE_TRAVELS("coe5.commanderLearnRitual", COMMANDER, 1), WRITE_TRAVELS("coe5.commanderUseItems", COMMANDER, 1),
    WRITE_TRAVELS("coe5.cureAffliction", NONE, 0), WRITE_TRAVELS("coe5.damageUnitPlain", UNIT, 1), WRITE_LOST("coe5.destroyEnchantsAt"),
    WRITE_TRAVELS("coe5.destroyGateway", NONE, 0), WRITE_TRAVELS("coe5.digTunnelSquare", NONE, 0), WRITE_LOST("coe5.fixPortals"),
    WRITE_TRAVELS("coe5.getStartRituals", COMMANDER, 1), WRITE_TRAVELS("coe5.giveAffliction", NONE, 0),
    WRITE_TRAVELS("coe5.giveItem", UNIT, 1), WRITE_TRAVELS("coe5.giveRandomItem", UNIT, 1), WRITE_LOST("coe5.initSquareCorpses"),
    WRITE_LOST("coe5.initSquareGems"), WRITE_LOST("coe5.killSquare"), WRITE_TRAVELS("coe5.killUnit", NONE, 0),
    WRITE_TRAVELS("coe5.learnLevelRituals", COMMANDER, 1), WRITE_TRAVELS("coe5.learnRandomSpell", UNIT, 1),
    WRITE_TRAVELS("coe5.learnStartSpells", UNIT, 1), WRITE_TRAVELS("coe5.memorizeSpell", UNIT, 1), WRITE_LOST("coe5.moveEnchants"),
    WRITE_LOST("coe5.moveGems"), WRITE_LOST("coe5.newPortalPair"), WRITE_TRAVELS("coe5.newUnits", PLAYER, 1),
    WRITE_TRAVELS("coe5.orderBurnForest", COMMANDER, 1), WRITE_TRAVELS("coe5.orderChopForest", COMMANDER, 1),
    WRITE_TRAVELS("coe5.orderEatVillage", COMMANDER, 1), WRITE_TRAVELS("coe5.orderSlaveHunt", COMMANDER, 1),
    WRITE_TRAVELS("coe5.planeShiftUnit", UNIT, 1), WRITE_LOST("coe5.putRareSquare"), WRITE_TRAVELS("coe5.randomTeleport", UNIT, 1),
    WRITE_TRAVELS("coe5.removeAffliction", UNIT, 1), WRITE_TRAVELS("coe5.removeBadFollowers", NONE, 0),
    WRITE_TRAVELS("coe5.removeEnchant", NONE, 0), WRITE_TRAVELS("coe5.repickMemorized", UNIT, 1), WRITE_TRAVELS("coe5.revealDisc", NONE, 0),
    WRITE_LOST("coe5.revealMap"), WRITE_LOST("coe5.revealPortalExits"), WRITE_TRAVELS("coe5.revealTerrainOnPlane", NONE, 0),
    WRITE_TRAVELS("coe5.ruinSquare", NONE, 0), WRITE_LOST("coe5.runModEvent"), WRITE_TRAVELS("coe5.selectedCommanderDone", NONE, 0),
    WRITE_LOST("coe5.sendMessage"), WRITE_TRAVELS("coe5.setAfflictionTurn", NONE, 0), WRITE_TRAVELS("coe5.setAfflictionValue", NONE, 0),
    WRITE_LOST("coe5.setRandomEventsOff"), WRITE_LOST("coe5.setSeenDisc"), WRITE_LOST("coe5.setSquareOwner"),
    WRITE_TRAVELS("coe5.spawnCommander", PLAYER, 1), WRITE_TRAVELS("coe5.spawnUnit", PLAYER, 1),
    WRITE_TRAVELS("coe5.spendAllApAt", PLAYER, 1), WRITE_TRAVELS("coe5.squareHold", PLAYER, 1),
    WRITE_TRAVELS("coe5.swapLocations", NONE, 0), WRITE_LOST("coe5.swapSquares"), WRITE_TRAVELS("coe5.takeItem", UNIT, 1),
    WRITE_TRAVELS("coe5.teleportCommander", COMMANDER, 1), WRITE_TRAVELS("coe5.teleportSquare", NONE, 0),
    WRITE_TRAVELS("coe5.teleportUnit", UNIT, 1), WRITE_TRAVELS("coe5.unitAddXp", UNIT, 1),
    WRITE_TRAVELS("coe5.unitRandomAffliction", UNIT, 1), WRITE_TRAVELS("coe5.unitRemoveItem", UNIT, 1),
    WRITE_TRAVELS("coe5.unitRemoveTempAbility", UNIT, 1), WRITE_TRAVELS("coe5.unmemorizeSpell", UNIT, 1),
    WRITE_TRAVELS("coe5.useSpecialSite", COMMANDER, 3), WRITE_LOST("coe5.worldNews"),
};
static const ClientWrite CLIENT_SET_VALUES[] = {
    WRITE_STAYS("aiLevelName"), WRITE_STAYS("aiMoveDelay"), WRITE_STAYS("bannerFade"), WRITE_STAYS("battleGrid"), WRITE_STAYS("bugChecksOff"),
    WRITE_STAYS("ceremonyCost"), WRITE_STAYS("cliMapH"), WRITE_STAYS("cliMapW"), WRITE_STAYS("fullscreen"), WRITE_STAYS("hoveredId"),
    WRITE_STAYS("incomeName"), WRITE_STAYS("keyHeld"), WRITE_STAYS("mouseButtons"), WRITE_STAYS("newWorldUniqueRandom"),
    WRITE_STAYS("resourceName"), WRITE_STAYS("seasonName"), WRITE_TRAVELS("slotAi", PLAYER, 2), WRITE_TRAVELS("slotResource", PLAYER, 2),
    WRITE_TRAVELS("slotTradeType", PLAYER, 2), WRITE_STAYS("startDefault"), WRITE_STAYS("traceLevel"), WRITE_STAYS("uniqueRandomDefault"),
    WRITE_TRAVELS("unitApUsed", UNIT, 2), WRITE_STAYS("wilderDefault"),
};
#undef WRITE_TRAVELS
#undef WRITE_LOST
#undef WRITE_STAYS
#define CLIENT_WRITE_CALL_COUNT ((int)(sizeof CLIENT_WRITE_CALLS / sizeof CLIENT_WRITE_CALLS[0]))
#define CLIENT_SET_VALUE_COUNT ((int)(sizeof CLIENT_SET_VALUES / sizeof CLIENT_SET_VALUES[0]))

static const ClientWrite *findClientWrite(const ClientWrite *rows, int count, const char *name) {
    int low = 0, high = count - 1;
    while (low <= high) {
        int middle = (low + high) / 2, order = strcmp(name, rows[middle].name);
        if (!order) return &rows[middle];
        if (order < 0) high = middle - 1;
        else low = middle + 1;
    }
    return NULL;
}

static ClientWrite clientWriteOf(const char *name, const char *setValue) {
    if (!strcmp(name, "coe5.set")) {
        const ClientWrite *w = findClientWrite(CLIENT_SET_VALUES, CLIENT_SET_VALUE_COUNT, setValue ? setValue : "");
        return w ? *w : (ClientWrite){setValue, CLIENT_WRITE_LOST, WRITE_OWNER_NONE, 0};
    }
    const ClientWrite *w = findClientWrite(CLIENT_WRITE_CALLS, CLIENT_WRITE_CALL_COUNT, name);
    return w ? *w : (ClientWrite){name, CLIENT_WRITE_STAYS, WRITE_OWNER_NONE, 0};
}

static const char *const REPLAY_UNDONE_WRITES[] = {"coe5.battleBanishUnit", "coe5.battleSummon", "coe5.cureAffliction", "coe5.giveAffliction", "coe5.setAfflictionTurn",
                                                   "coe5.setAfflictionValue"};
#define REPLAY_UNDONE_WRITE_COUNT ((int)(sizeof REPLAY_UNDONE_WRITES / sizeof REPLAY_UNDONE_WRITES[0]))

static int writeUndoneInReplay(const char *name) {
    for (int i = 0; i < REPLAY_UNDONE_WRITE_COUNT; i++)
        if (!strcmp(name, REPLAY_UNDONE_WRITES[i])) return 1;
    return 0;
}

static const char *clientWriteRefusalFor(ClientWriteKind kind, int inTurn, int own, int replay) {
    if (kind == CLIENT_WRITE_STAYS) return NULL;
    if (replay) return "replay";
    if (kind == CLIENT_WRITE_LOST) return "client_write_lost";
    if (!inTurn) return "client_not_acting";
    return own == 0 ? "not_own_player" : NULL;
}

static const char *clientContentWriteRefusalFor(int modsReading) { return modsReading ? NULL : "client_write_lost"; }

static int keyPickCountsFor(NetRole role, int fromPersonal) {
    return role == NET_ROLE_SINGLE || (role == NET_ROLE_CLIENT && fromPersonal);
}

// ---- Now ----------------------------------------------------------------------------------------
static int modIsPersonal(const char *modName);
static int modIsNoMultiplayer(const char *modName);

static LuaClass modClassNow(const char *name) {
    return modIsNoMultiplayer(name) ? LUA_CLASS_OFF : modIsPersonal(name) ? LUA_CLASS_PERSONAL : LUA_CLASS_CONTENT;
}

static int netClientLoading;
static unsigned netClientLoads;
static int netClientInTurn;
static int netClientGameOverSeen;

static void clearNetServerFlag(void) {
#ifdef __APPLE__
    G8(RVA_NET_SERVER) = 0;
#else
    G32(RVA_NET_SERVER) = 0;
#endif
}

static int netShutsGameCalls(void) {
    NetRole role = netRoleNow();
    return role == NET_ROLE_CLIENT || role == NET_ROLE_OFFICIAL;
}
