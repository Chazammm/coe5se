#pragma once

#define MONSTER_FLAG_SIZE1X1 (1ULL << 52)
#define MONSTER_FLAG_SIZE2X2 (1ULL << 53)
#define MONSTER_MOVE_FLAG_HUGE 2

typedef struct {
    int monster, unit, width, height;
} TestFieldFootprint;

static int isMonsterSpriteValid(int monster) {
    const unsigned char *record = monsterRecord(monster);
    uint64_t flags;
    memcpy(&flags, record + (RVA_MONSTER_FLAGS - RVA_MONSTER_TABLE), sizeof flags);
    if (flags & (MONSTER_FLAG_SIZE1X1 | MONSTER_FLAG_SIZE2X2)) return 1;
    int sprite = *(const int *)(record + (RVA_MONSTER_SPRITES - RVA_MONSTER_TABLE));
    return sprite >= 0 && sprite < SPRITES_PER_KIND;
}

static int refuseTestFieldPreview(lua_State *L, const char *call, const char *reason) {
    refuseLuaCall(runningLuaMod(), call, reason, "preview=unavailable");
    lua_pushnil(L);
    lua_pushstring(L, reason);
    return 2;
}

#ifdef __APPLE__
static int monsterBattleSizeCopy(int monster);
#endif

// @platform-split test_field_copies: Mac's compiler copied the memorize allowance and the monster token width into their callers, so CoE5SE's copies run there (lua_game_copies.h)
static int monsterTokenWidth(int monster) {
    if (battlesizeWidth(monster)) return battlesizeWidth(monster);
#ifdef __APPLE__
    return monsterBattleSizeCopy(monster);
#else
    return GAME_FN(int (*)(int), RVA_MONSTER_BATTLE_SIZE)(monster);
#endif
}

static int monsterTokenHeight(int monster) {
    if (battlesizeHeight(monster)) return battlesizeHeight(monster);
    const unsigned char *record = monsterRecord(monster);
    uint64_t flags, moveFlags;
    memcpy(&flags, record + (RVA_MONSTER_FLAGS - RVA_MONSTER_TABLE), sizeof flags);
    memcpy(&moveFlags, record + MONSTER_MOVE_FLAGS, sizeof moveFlags);
    if (flags & MONSTER_FLAG_SIZE1X1) return 1;
    if (flags & MONSTER_FLAG_SIZE2X2) return 2;
    if (moveFlags & MONSTER_MOVE_FLAG_HUGE) return 3;
    int pixels = 0;
    GAME_FN(void (*)(int, int *, int *, int *), RVA_MONSTER_SPRITE_SIZE)(*(const int *)(record + (RVA_MONSTER_SPRITES - RVA_MONSTER_TABLE)), NULL, &pixels, NULL);
    return pixels <= 0 ? 0 : pixels > 150 ? 3 : pixels > 100 ? 2 : 1;
}

static const char *readTestFieldFootprint(LuaScript *script, lua_Integer monster, lua_Integer unit, TestFieldFootprint *out) {
    const char *reason = testFieldReadProblem(script);
    if (reason) return reason;
    if (testField.fighting) return "in_fight";
    if (!isUsedMonster(monster)) return "bad_monster";
    int width = 0, height = 0;
    if (unit >= 0) {
        reason = testFieldUnitEditProblem(script, unit);
        if (reason) return reason;
        monster = G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE);
        if (!isUsedMonster(monster)) return "bad_monster";
        if (!isMonsterSpriteValid((int)monster)) return "bad_sprite";
        width = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)((int)unit);
        height = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)((int)unit);
    } else {
        if (!isMonsterSpriteValid((int)monster)) return "bad_sprite";
        width = monsterTokenWidth((int)monster);
        height = monsterTokenHeight((int)monster);
        if (!height) return "bad_sprite_size";
    }
    if (width < 1 || width > battleTokenReach() || height < 1 || height > battleTokenReach()) return "bad_footprint";
    *out = (TestFieldFootprint){(int)monster, (int)unit, width, height};
    return NULL;
}

static void pushTestFieldFootprint(lua_State *L, const TestFieldFootprint *f) {
    setIntField(L, "monster", f->monster);
    setIntField(L, "unit", f->unit);
    setIntField(L, "width", f->width);
    setIntField(L, "height", f->height);
    setBoolField(L, "exact", 1);
    setBoolField(L, "instance", f->unit >= 0);
    lua_pushstring(L, f->unit >= 0 ? "native_unit" : "native_monster_rules");
    lua_setfield(L, -2, "source");
    lua_pushliteral(L, "positive_xy");
    lua_setfield(L, -2, "anchor");
    setBoolField(L, "spawnVariantVerified", f->unit >= 0);
}

static int lTestFieldFootprint(lua_State *L) {
    const char *call = "coe5.testFieldFootprint";
    checkLuaMainThread(L, call);
    LuaScript *script = checkLuaScript(L);
    lua_Integer monster = luaL_checkinteger(L, 1), unit = luaL_optinteger(L, 2, -1);
    TestFieldFootprint f;
    const char *reason = unit < -1 ? "bad_unit" : readTestFieldFootprint(script, monster, unit, &f);
    if (reason) return refuseTestFieldPreview(L, call, reason);
    lua_createtable(L, 0, 9);
    pushTestFieldFootprint(L, &f);
    return 1;
}

static int testFieldUnitCovering(int x, int y, int layer, int ignore, int *uncertain) {
    int found = -1, last = G32(RVA_UNIT_COUNT);
    if (last < -1 || last >= UNIT_TABLE_MAX) {
        *uncertain = 1;
        return -1;
    }
    int reach = battleTokenReach();
    for (int ax = x - reach + 1; ax <= x; ax++)
        for (int ay = y - reach + 1; ay <= y; ay++) {
            if (ax < 0 || ay < 0) continue;
            int unit = *(const int *)(battleCell(ax, ay) + layer);
            if (unit == -1 || unit == ignore) continue;
            if (unit < 0 || unit >= UNIT_TABLE_MAX || unit > last) {
                *uncertain = 1;
                continue;
            }
            size_t at = (size_t)unit * UNIT_SIZE;
            int monster = G16(RVA_UNIT_MONSTER + at);
            if (G16(RVA_UNIT_HP + at) <= 0 || !isUsedMonster(monster) || !isMonsterSpriteValid(monster)) {
                *uncertain = 1;
                continue;
            }
            int w = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(unit);
            int h = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(unit);
            if (w < 1 || w > reach || h < 1 || h > reach) {
                *uncertain = 1;
                continue;
            }
            if (ax + w > x && ay + h > y) {
                if (found >= 0 && found != unit) *uncertain = 1;
                found = unit;
            }
        }
    return found;
}

static int lTestFieldPlacementPreflight(lua_State *L) {
    const char *call = "coe5.testFieldPlacementPreflight";
    checkLuaMainThread(L, call);
    LuaScript *script = checkLuaScript(L);
    lua_Integer monster = luaL_checkinteger(L, 1), x = luaL_checkinteger(L, 2), y = luaL_checkinteger(L, 3);
    lua_Integer unit = luaL_optinteger(L, 4, -1);
    int ignoreSelf = lua_toboolean(L, 5);
    TestFieldFootprint f;
    const char *reason = unit < -1                                                  ? "bad_unit"
                       : !lua_isnoneornil(L, 5) && lua_type(L, 5) != LUA_TBOOLEAN ? "bad_type"
                       : ignoreSelf && unit < 0                                     ? "bad_unit"
                                                                                    : readTestFieldFootprint(script, monster, unit, &f);
    if (reason) return refuseTestFieldPreview(L, call, reason);
    BattleView view;
    if (!readBattleView(L, call, &view)) {
        lua_pushnil(L);
        lua_pushliteral(L, "not_battle_frame");
        return 2;
    }
    if (x < -3 || y < -3 || x > view.width + 3 || y > view.height + 3) return refuseTestFieldPreview(L, call, "bad_anchor");
    lua_createtable(L, 0, 15);
    int result = lua_gettop(L);
    pushTestFieldFootprint(L, &f);
#ifndef __APPLE__
    setIntField(L, "version", 2);
#endif
    setBoolField(L, "partial", 1);
    lua_pushliteral(L, "geometry_and_occupancy_only");
    lua_setfield(L, result, "reason");
    lua_createtable(L, f.width * f.height, 0);
    int cells = lua_gettop(L), n = 0, valid = 1;
    for (int dx = 0; dx < f.width; dx++)
        for (int dy = 0; dy < f.height; dy++) {
            int cx = (int)x + dx, cy = (int)y + dy, actor = -1, object = -1, uncertain = 0;
            const char *status = "out_of_bounds", *decision = NULL;
            if (cx >= 0 && cy >= 0 && cx < view.width && cy < view.height) {
                actor = testFieldUnitCovering(cx, cy, 0, ignoreSelf ? (int)unit : -1, &uncertain);
                object = testFieldUnitCovering(cx, cy, BATTLE_CELL_OBJECT, ignoreSelf ? (int)unit : -1, &uncertain);
                status = uncertain ? "unknown" : actor >= 0 ? "occupied" : object >= 0 ? "terrain" : "clear";
#ifndef __APPLE__
                if (!uncertain && actor < 0 && object >= 0 && testFieldSiegeObjectCandidate(object)) {
                    int fits = x >= 0 && y >= 0 && x <= view.width - f.width && y <= view.height - f.height;
                    if (unit < 0) {
                        status = "unknown";
                        decision = "requires_instance";
                    } else if (fits && testFieldSiegeObjectPassable((int)unit, (int)x, (int)y, object)) {
                        status = "clear";
                        decision = "native_instance_accepted";
                    } else decision = "native_move_rejected";
                }
#endif
            }
            if (strcmp(status, "clear")) valid = 0;
            lua_createtable(L, 0, 8);
            setIntField(L, "x", cx);
            setIntField(L, "y", cy);
            setIntField(L, "unit", actor);
            setIntField(L, "object", object);
            lua_pushstring(L, status);
            lua_setfield(L, -2, "status");
            if (decision) {
                lua_pushstring(L, decision);
                lua_setfield(L, -2, "objectDecision");
            }
            lua_pushnumber(L, ((float)cx + 0.5f) * view.cellSize + view.baseX + view.cameraX);
            lua_setfield(L, -2, "screenX");
            lua_pushnumber(L, ((float)cy + 0.5f) * view.cellSize + view.baseY + view.cameraY);
            lua_setfield(L, -2, "screenY");
            lua_rawseti(L, cells, ++n);
        }
    lua_setfield(L, result, "cells");
    setBoolField(L, "valid", valid);
    return 1;
}
