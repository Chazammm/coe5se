#pragma once

// ---- Units: coe5.spawnUnit, coe5.spawnCommander, coe5.monsterVariant ----------------------------
#define SPAWN_SIGHT_RADIUS 3

static int isUsedMonster(lua_Integer monster) {
    return monster >= 0 && monster <= G32(RVA_MONSTER_COUNT) && monsterRecord((int)monster) && monsterRecord((int)monster)[0];
}

static const char *spawnProblem(lua_Integer player, lua_Integer monster, lua_Integer x, lua_Integer y) {
    if (G32(RVA_ACTIVE_PLAYER) < 0) return "no_game";
    if (player < 0 || player >= playerSlotCount()) return "bad_player";
    if (!isUsedMonster(monster)) return "bad_monster";
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_DEPTH) || y >= G32(RVA_MAP_H) || x >= MAP_COLUMNS_MAX || y >= MAP_COLUMN_SQUARES)
        return "off_map";
    return NULL;
}

static void settleSpawnedUnit(int player, int unit, int x, int y) {
    GAME_FN(void (*)(int), RVA_UNIT_FIT_TERRAIN)(unit);
    rebuildSquareArmyHook(x, y);
    GAME_FN(int (*)(int, int, int), RVA_CONQUER_CHECK)(x, y, 0);
    updateSightHook(player, x - SPAWN_SIGHT_RADIUS, y - SPAWN_SIGHT_RADIUS, x + SPAWN_SIGHT_RADIUS, y + SPAWN_SIGHT_RADIUS);
    GAME_FN(void (*)(void), RVA_RESET_SQUARE_CACHE)();
}

static int luaSpawn(lua_State *L, int commander) {
    const char *call = commander ? "coe5.spawnCommander" : "coe5.spawnUnit";
    lua_Integer player = luaCheckPlayer(L, 1), monster = luaL_checkinteger(L, 2);
    lua_Integer x = luaL_checkinteger(L, 3), y = luaL_checkinteger(L, 4);
    const char *problem = spawnProblem(player, monster, x, y);
    if (problem) {
        refuseLuaCall(runningLuaMod(), call, problem, "player=%lld monster=%lld x=%lld y=%lld", (long long)player,
                      (long long)monster, (long long)x, (long long)y);
        lua_pushnil(L);
        return 1;
    }
    int result, unit;
    if (commander) {
        result = GAME_FN(int (*)(int, int, int, int), RVA_NEW_COMMANDER)((int)x, (int)y, (int)player, (int)monster);
        unit = result >= 0 && result <= G32(RVA_LAST_COMMANDER) ? G32(RVA_COMMANDERS + (size_t)result * COMMANDER_SIZE) : -1;
    } else {
        result = unit = GAME_FN(int (*)(int, int, int, int, int), RVA_NEW_UNIT)((int)x, (int)y, (int)player, -1, (int)monster);
    }
    if (result < 0 || unit < 0) {
        refuseLuaCall(runningLuaMod(), call, "no_free_slot", "player=%lld monster=%lld x=%lld y=%lld", (long long)player,
                      (long long)monster, (long long)x, (long long)y);
        lua_pushnil(L);
        return 1;
    }
    settleSpawnedUnit((int)player, unit, (int)x, (int)y);
    lua_pushinteger(L, result);
    return 1;
}
static int lSpawnUnit(lua_State *L) { return luaSpawn(L, 0); }
static int lSpawnCommander(lua_State *L) { return luaSpawn(L, 1); }

// ---- Promotion: coe5.promoteUnit -----------------------------------------------------------------
typedef void (*PromoteUnitFn)(int owner, int unit, int monster, int commanderize);
static const char *promoteProblem(lua_Integer unit, lua_Integer monster) {
    if (G32(RVA_ACTIVE_PLAYER) < 0) return "no_game";
    if (unit < 0 || unit > G32(RVA_UNIT_COUNT) || unit >= UNIT_TABLE_MAX || (short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE) < 0 ||
        (short)G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE) <= 0)
        return "no_unit";
    if (!isUsedMonster(monster)) return "bad_monster";
    return NULL;
}

static int lPromoteUnit(lua_State *L) {
    lua_Integer unit = luaL_checkinteger(L, 1), monster = luaL_checkinteger(L, 2);
    int commanderize = lua_toboolean(L, 3);
    const char *problem = promoteProblem(unit, monster);
    if (problem) {
        refuseLuaCall(runningLuaMod(), "coe5.promoteUnit", problem, "unit=%lld monster=%lld", (long long)unit, (long long)monster);
        lua_pushnil(L);
        return 1;
    }
    size_t at = (size_t)unit * UNIT_SIZE;
    int owner = (short)G16(RVA_UNIT_PLAYER + at), leader = G32(RVA_UNITS + at + UNIT_COMMANDER), was = unitCommanderIndex((int)unit);
    ((PromoteUnitFn)siteEntry(SITE_PROMOTE_UNIT, GAME_PTR(RVA_PROMOTE_UNIT)))(owner, (int)unit, (int)monster, commanderize);
    int commander = commanderize ? unitCommanderIndex((int)unit) : -1;
    if (commanderize && commander < 0) {
        refuseLuaCall(runningLuaMod(), "coe5.promoteUnit", "no_free_slot", "unit=%lld monster=%lld", (long long)unit, (long long)monster);
        lua_pushnil(L);
        return 1;
    }
    if (commanderize && was < 0) {
        if (leader >= 0 && leader < COMMANDER_TABLE_MAX) flushCommanderCache(leader);
        int x = G32(RVA_UNIT_X + at), y = G32(RVA_UNIT_X + 4 + at);
        if (owner >= 0 && x >= 0) updateSightHook(owner, x - SPAWN_SIGHT_RADIUS, y - SPAWN_SIGHT_RADIUS, x + SPAWN_SIGHT_RADIUS, y + SPAWN_SIGHT_RADIUS);
    }
    writeLog("lua_unit_promoted", "mod=%s unit=%lld monster=%lld commander=%d leader=%d", runningLuaMod(), (long long)unit, (long long)monster,
             commander, leader);
    lua_pushinteger(L, commanderize ? commander : (lua_Integer)unit);
    return 1;
}

// ---- Renaming: coe5.renameCommander -------------------------------------------------------------
#define RENAME_COMMANDER_TITLE "Rename Commander"

static int commanderLeadsAPlayer(int commander) {
    for (int p = 0; p < playerSlotCount(); p++)
        if (PSLOT16(RVA_SLOT_CONTROLLER, p, 2) >= 0 && PSLOT32(RVA_PLAYER_LEADER, p, PLAYER_BLOCK_SIZE) == commander) return 1;
    return 0;
}

static void cleanTypedCommanderName(char *buffer) {
    size_t length = 0;
    while (length < COMMANDER_NAME_SIZE && buffer[length]) {
        if (buffer[length] == '%') buffer[length] = '_';
        length++;
    }
    if (length >= COMMANDER_NAME_SIZE) buffer[COMMANDER_NAME_SIZE - 1] = 0;
    for (char *c = buffer; *c; c++)
        if (*c == ',') *c = '.';
}

static int lRenameCommander(lua_State *L) {
    lua_Integer commander = luaL_checkinteger(L, 1);
    const char *problem = NULL;
    if (G32(RVA_ACTIVE_PLAYER) < 0) problem = "no_game";
    else if (commander < 0 || commander >= COMMANDER_TABLE_MAX || livingCommanderUnit((int)commander) < 0) problem = "dead_commander";
    else if (commanderLeadsAPlayer((int)commander)) problem = "main_commander";
    if (problem) {
        refuseLuaCall(runningLuaMod(), "coe5.renameCommander", problem, "commander=%lld", (long long)commander);
        lua_pushnil(L);
        return 1;
    }
    char *name = (char *)GAME_PTR(RVA_COMMANDER_NAMES + (size_t)commander * COMMANDER_SIZE);
    char buffer[COMMANDER_NAME_SIZE];
    memcpy(buffer, name, sizeof buffer);
    buffer[sizeof buffer - 1] = 0;
    int ok = GAME_FN(int (*)(const char *, char *, int, int), RVA_TEXT_ENTRY_BOX)(RENAME_COMMANDER_TITLE, buffer, COMMANDER_NAME_SIZE, 1);
    if (ok <= 0) {
        writeLog("lua_commander_rename_cancelled", "mod=%s commander=%lld", runningLuaMod(), (long long)commander);
        lua_pushnil(L);
        return 1;
    }
    cleanTypedCommanderName(buffer);
    memcpy(name, buffer, sizeof buffer);
    writeLog("lua_commander_renamed", "mod=%s commander=%lld length=%zu", runningLuaMod(), (long long)commander, strlen(buffer));
    lua_pushstring(L, buffer);
    return 1;
}

#define SUMMON_STRING_MAX 1999
static int lNewUnits(lua_State *L) {
    lua_Integer player = luaCheckPlayer(L, 1), x = luaL_checkinteger(L, 2), y = luaL_checkinteger(L, 3);
    size_t len;
    const char *monsters = luaL_checklstring(L, 4, &len);
    const char *problem = G32(RVA_ACTIVE_PLAYER) < 0                           ? "no_game"
                          : player < 0 || player >= playerSlotCount()          ? "bad_player"
                          : x < 0 || y < 0 || x >= G32(RVA_MAP_DEPTH) || y >= G32(RVA_MAP_H) || x >= MAP_COLUMNS_MAX || y >= MAP_COLUMN_SQUARES ? "off_map"
                          : len > SUMMON_STRING_MAX || strlen(monsters) != len ? "bad_string"
                                                                               : NULL;
    if (problem) {
        refuseLuaCall(runningLuaMod(), "coe5.newUnits", problem, "player=%lld x=%lld y=%lld length=%zu", (long long)player, (long long)x,
                      (long long)y, len);
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, GAME_FN(int (*)(int, int, int, const char *, int), RVA_SUMMON_STRING)((int)player, (int)x, (int)y, monsters, 100));
    return 1;
}

static int lMonsterVariant(lua_State *L) {
    lua_Integer monster = luaL_checkinteger(L, 1);
    if (!isUsedMonster(monster)) {
        refuseLuaCall(runningLuaMod(), "coe5.monsterVariant", "bad_monster", "monster=%lld", (long long)monster);
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, GAME_FN(int (*)(int), RVA_MONSTER_VARIANT)((int)monster));
    return 1;
}

// ---- Moving armies: coe5.teleportCommander, coe5.mayEnterSquare --------------------------------
#define MOVE_ORDER_NONE -1

static const char *commanderMoveProblem(lua_Integer commander, lua_Integer x, lua_Integer y) {
    if (G32(RVA_ACTIVE_PLAYER) < 0) return "no_game";
    if (commander < 0 || commander > G32(RVA_LAST_COMMANDER) || commander >= COMMANDER_TABLE_MAX) return "bad_commander";
    int unit = G32(RVA_COMMANDERS + (size_t)commander * COMMANDER_SIZE);
    if (unit < 0 || unit > G32(RVA_UNIT_COUNT) || unit >= UNIT_TABLE_MAX || G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE) <= 0)
        return "dead_commander";
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_DEPTH) || y >= G32(RVA_MAP_H) || x >= MAP_COLUMNS_MAX || y >= MAP_COLUMN_SQUARES)
        return "off_map";
    if (GAME_FN(int (*)(int, int), RVA_PLANE_AT)((int)x, (int)y) < 0) return "no_plane";
    return NULL;
}

static int lTeleportCommander(lua_State *L) {
    lua_Integer commander = luaL_checkinteger(L, 1), x = luaL_checkinteger(L, 2), y = luaL_checkinteger(L, 3);
    const char *problem = commanderMoveProblem(commander, x, y);
    if (problem) {
        refuseLuaCall(runningLuaMod(), "coe5.teleportCommander", problem, "commander=%lld x=%lld y=%lld", (long long)commander,
                      (long long)x, (long long)y);
        lua_pushboolean(L, 0);
        return 1;
    }
    GAME_FN(void (*)(int, int, int), RVA_TELEPORT_COMMANDER)((int)commander, (int)x, (int)y);
    unsigned char *record = GAME_PTR(RVA_COMMANDERS + (size_t)commander * COMMANDER_SIZE);
    *(int *)(record + COMMANDER_MOVE_ORDER_X) = MOVE_ORDER_NONE;
    *(int *)(record + COMMANDER_MOVE_ORDER_Y) = MOVE_ORDER_NONE;
    writeLog("lua_commander_teleported", "mod=%s commander=%lld x=%lld y=%lld", runningLuaMod(), (long long)commander, (long long)x,
             (long long)y);
    lua_pushboolean(L, 1);
    return 1;
}

static int lMayEnterSquare(lua_State *L) {
    lua_Integer commander = luaL_checkinteger(L, 1), x = luaL_checkinteger(L, 2), y = luaL_checkinteger(L, 3);
    const char *problem = commanderMoveProblem(commander, x, y);
    if (problem) {
        refuseLuaCall(runningLuaMod(), "coe5.mayEnterSquare", problem, "commander=%lld x=%lld y=%lld", (long long)commander,
                      (long long)x, (long long)y);
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, GAME_FN(int (*)(int, int, int), RVA_MAY_ENTER_SQUARE)((int)commander, (int)x, (int)y) != 0);
    return 1;
}
