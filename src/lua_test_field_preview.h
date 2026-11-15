#pragma once

/* Read-only geometry/occupancy preview for the owned Windows 5.43 TestField.
 * Include after lua_test_unit_state.h and lua_battle_view.h. No temporary
 * actors, scenario writes, RNG calls or drawing operations are used. Native
 * sprite metadata may populate its ordinary dimension cache on first access. */
typedef struct {
    int monster, unit, width, height;
} TestFieldPreviewFootprint;

static int testFieldPreviewSpriteValid(int monster) {
    const unsigned char *record = monsterRecord(monster);
    uint64_t flags;
    memcpy(&flags, record + (RVA_MONSTER_FLAGS - RVA_MONSTER_TABLE), sizeof flags);
    if (flags & ((1ULL << 52) | (1ULL << 53))) return 1;
    int sprite = *(const int *)(record + (RVA_MONSTER_SPRITES - RVA_MONSTER_TABLE));
    return sprite >= 0 && sprite < SPRITES_PER_KIND;
}

static int testFieldPreviewFailure(lua_State *L, const char *call, const char *reason) {
    refuseLuaCall(runningLuaMod(), call, reason, "preview=unavailable");
    lua_pushnil(L); lua_pushstring(L, reason); return 2;
}

static const char *testFieldPreviewFootprint(LuaScript *script, lua_Integer monster, lua_Integer unit,
                                            TestFieldPreviewFootprint *out) {
    const char *reason = testFieldReadProblem(script);
    if (reason) return reason;
    if (testField.fighting) return "in_fight";
    if (!isUsedMonster(monster)) return "bad_monster";
    int width = 0, height = 0;
#ifndef __APPLE__
    if (unit >= 0) {
        reason = testFieldUnitEditorProblem(script, unit);
        if (reason) return reason;
        monster = G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE);
        if (!isUsedMonster(monster)) return "bad_monster";
        if (!testFieldPreviewSpriteValid((int)monster)) return "bad_sprite";
        width = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)((int)unit);
        height = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)((int)unit);
    } else {
        /* Width is the native monster counterpart of unit width. Height has
         * no exported template counterpart: this is the exact observed unit
         * height rule with the current monster substituted, including explicit
         * size1x1/size2x2 flags and giant/size3 flag BEFORE the frame fallback.
         * Native unit width/height inspect no HP, items or instance abilities;
         * a transformation is reflected by the unit's changed monster ID. */
        const unsigned char *record = monsterRecord((int)monster);
        uint64_t flags, moveFlags;
        memcpy(&flags, record + (RVA_MONSTER_FLAGS - RVA_MONSTER_TABLE), sizeof flags);
        memcpy(&moveFlags, record + MONSTER_MOVE_FLAGS, sizeof moveFlags);
        if (!testFieldPreviewSpriteValid((int)monster)) return "bad_sprite";
        width = GAME_FN(int (*)(int), RVA_MONSTER_BATTLE_SIZE)((int)monster);
        if (flags & (1ULL << 52)) height = 1;
        else if (flags & (1ULL << 53)) height = 2;
        else if (moveFlags & 2) height = 3;
        else {
            int sprite = *(const int *)(record + (RVA_MONSTER_SPRITES - RVA_MONSTER_TABLE)), pixels = 0;
            if (sprite < 0 || sprite >= SPRITES_PER_KIND) return "bad_sprite";
            GAME_FN(void (*)(int, int *, int *, int *), RVA_MONSTER_SPRITE_SIZE)(sprite, NULL, &pixels, NULL);
            if (pixels <= 0) return "bad_sprite_size";
            height = pixels > 150 ? 3 : pixels > 100 ? 2 : 1;
        }
    }
#endif
    if (width < 1 || width > 3 || height < 1 || height > 3) return "bad_footprint";
    *out = (TestFieldPreviewFootprint){(int)monster, (int)unit, width, height};
    return NULL;
}

static void pushTestFieldPreviewFootprint(lua_State *L, const TestFieldPreviewFootprint *f) {
    testFieldStateInt(L, "monster", f->monster); testFieldStateInt(L, "unit", f->unit);
    testFieldStateInt(L, "width", f->width); testFieldStateInt(L, "height", f->height);
    testFieldStateBool(L, "exact", 1); testFieldStateBool(L, "instance", f->unit >= 0);
    lua_pushstring(L, f->unit >= 0 ? "native_unit" : "native_monster_rules"); lua_setfield(L, -2, "source");
    lua_pushliteral(L, "positive_xy"); lua_setfield(L, -2, "anchor");
    /* The template geometry is exact for this monster now; no constructor is
     * called to discover a possible random spawn variant or future shape. */
    testFieldStateBool(L, "spawnVariantVerified", f->unit >= 0);
}

static int lTestFieldFootprint(lua_State *L) {
    const char *call = "coe5.testFieldFootprint";
    checkLuaMainThread(L, call);
    LuaScript *script = checkLuaScript(L);
    lua_Integer monster = luaL_checkinteger(L, 1), unit = luaL_optinteger(L, 2, -1);
    TestFieldPreviewFootprint f;
    const char *reason = unit < -1 ? "bad_unit" : testFieldPreviewFootprint(script, monster, unit, &f);
    if (reason) return testFieldPreviewFailure(L, call, reason);
    lua_createtable(L, 0, 9); pushTestFieldPreviewFootprint(L, &f); return 1;
}

/* Same positive-X/positive-Y coverage as native BattleUnitAt/ObjectAt. Validate
 * handles BEFORE the native dimension getter could dereference them. A bad or
 * dead nearby handle produces unknown, never an optimistic clear cell. */
static int testFieldPreviewCovering(int x, int y, int layer, int ignore, int *uncertain) {
    int found = -1, last = G32(RVA_UNIT_COUNT);
    if (last < -1 || last >= UNIT_TABLE_MAX) { *uncertain = 1; return -1; }
    for (int ax = x - 2; ax <= x; ax++)
        for (int ay = y - 2; ay <= y; ay++) {
            if (ax < 0 || ay < 0) continue;
            int unit = *(const int *)(battleCell(ax, ay) + layer);
            if (unit == -1 || unit == ignore) continue;
            if (unit < 0 || unit >= UNIT_TABLE_MAX || unit > last) { *uncertain = 1; continue; }
            size_t at = (size_t)unit * UNIT_SIZE;
            int monster = G16(RVA_UNIT_MONSTER + at);
            if (G16(RVA_UNIT_HP + at) <= 0 || !isUsedMonster(monster) || !testFieldPreviewSpriteValid(monster)) { *uncertain = 1; continue; }
            int w = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(unit);
            int h = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(unit);
            if (w < 1 || w > 3 || h < 1 || h > 3) { *uncertain = 1; continue; }
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
    TestFieldPreviewFootprint f;
    const char *reason = unit < -1 ? "bad_unit" : !lua_isnoneornil(L, 5) && lua_type(L, 5) != LUA_TBOOLEAN ? "bad_type" :
                         ignoreSelf && unit < 0 ? "bad_unit" : testFieldPreviewFootprint(script, monster, unit, &f);
    if (reason) return testFieldPreviewFailure(L, call, reason);
    TestFieldBattleView view;
    if (!currentTestFieldBattleView(L, call, &view)) { lua_pushnil(L); lua_pushliteral(L, "not_battle_frame"); return 2; }
    /* Small adjacent out-of-field anchors are useful for red edge previews;
     * bound integers before sums, casts and projection. */
    if (x < -3 || y < -3 || x > view.width + 3 || y > view.height + 3)
        return testFieldPreviewFailure(L, call, "bad_anchor");
    lua_createtable(L, 0, 15); int result = lua_gettop(L);
    pushTestFieldPreviewFootprint(L, &f);
    testFieldStateBool(L, "partial", 1);
    lua_pushliteral(L, "geometry_and_occupancy_only"); lua_setfield(L, result, "reason");
    lua_createtable(L, f.width * f.height, 0); int cells = lua_gettop(L), n = 0, valid = 1;
    for (int dx = 0; dx < f.width; dx++)
        for (int dy = 0; dy < f.height; dy++) {
            int cx = (int)x + dx, cy = (int)y + dy, actor = -1, object = -1, uncertain = 0;
            const char *status = "out_of_bounds";
            if (cx >= 0 && cy >= 0 && cx < view.width && cy < view.height) {
                actor = testFieldPreviewCovering(cx, cy, 0, ignoreSelf ? (int)unit : -1, &uncertain);
                object = testFieldPreviewCovering(cx, cy, 4, ignoreSelf ? (int)unit : -1, &uncertain);
                status = uncertain ? "unknown" : actor >= 0 ? "occupied" : object >= 0 ? "terrain" : "clear";
            }
            if (strcmp(status, "clear")) valid = 0;
            lua_createtable(L, 0, 8);
            testFieldStateInt(L, "x", cx); testFieldStateInt(L, "y", cy);
            testFieldStateInt(L, "unit", actor); testFieldStateInt(L, "object", object);
            lua_pushstring(L, status); lua_setfield(L, -2, "status");
            lua_pushnumber(L, ((float)cx + 0.5f) * view.cellSize + view.baseX + view.cameraX); lua_setfield(L, -2, "screenX");
            lua_pushnumber(L, ((float)cy + 0.5f) * view.cellSize + view.baseY + view.cameraY); lua_setfield(L, -2, "screenY");
            lua_rawseti(L, cells, ++n);
        }
    lua_setfield(L, result, "cells"); testFieldStateBool(L, "valid", valid);
    return 1;
}
