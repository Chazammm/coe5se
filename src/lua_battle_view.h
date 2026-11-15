#pragma once
#include <float.h>
#include <math.h>

/* Experimental read-only view of an open main-menu TestField, Windows 5.43.
 * Include after lua_test_battle.h. These private RVAs were checked against the
 * supported executable; gameCodeMatches must hold before they are accessed.
 * Native screen-to-cell takes UI coordinates (Y up) and uses half-open cells.
 * Camera updates occur before BATTLE_WAIT returns: query after next for clicks,
 * and during BATTLE_DRAW for an overlay matching the frame being drawn.
 */
#ifndef __APPLE__
#endif
#define TEST_FIELD_VIEW_COLUMNS_MAX     0x600

typedef struct {
    float cellSize, baseX, baseY, cameraX, cameraY;
    int width, height, screenW, screenH, windowW, windowH;
} TestFieldBattleView;

static int currentTestFieldBattleView(lua_State *L, const char *call, TestFieldBattleView *out) {
    checkLuaMainThread(L, call);
    if (!testFieldIsOpen(L, call)) return 0;
    const char *reason = NULL;
#ifdef __APPLE__
    (void)out;
    reason = "windows_only";
#else
    if (!gameCodeMatches) reason = "game_code_differs";
    /* markBattleFrame's counter ends inside next(). Lua's site depth spans the
     * complete wrap, including its post-next input handling and fresh camera. */
    else if (battleDrawing <= 0 && battleWaiting <= 0 &&
             luaSiteDepth[SITE_BATTLE_DRAW] <= 0 && luaSiteDepth[SITE_BATTLE_WAIT] <= 0) reason = "not_battle_frame";
    else {
        TestFieldBattleView v = {
            .cellSize = GF(RVA_BATTLE_CELL_PX),
            .baseX = *(const float *)(gameBase + RVA_TEST_FIELD_VIEW_BASE_X),
            .baseY = *(const float *)(gameBase + RVA_TEST_FIELD_VIEW_BASE_Y),
            .cameraX = *(const float *)(gameBase + RVA_TEST_FIELD_VIEW_CAMERA_X),
            .cameraY = *(const float *)(gameBase + RVA_TEST_FIELD_VIEW_CAMERA_Y),
            .width = G32(RVA_BATTLE_WIDTH), .height = BATTLE_ROWS,
            .screenW = G32(RVA_SCREEN_W), .screenH = G32(RVA_SCREEN_H),
            .windowW = G32(RVA_WINDOW_W), .windowH = G32(RVA_WINDOW_H),
        };
        if (!(v.cellSize > 0) || !isfinite(v.cellSize) || !isfinite(v.baseX) || !isfinite(v.baseY) ||
            !isfinite(v.cameraX) || !isfinite(v.cameraY) || v.width < 1 || v.width > TEST_FIELD_VIEW_COLUMNS_MAX ||
            v.screenW < 1 || v.screenH < 1 || v.windowW < 1 || v.windowH < 1) reason = "invalid_battle_view";
        else {
            *out = v;
            return 1;
        }
    }
#endif
    refuseLuaCall(runningLuaMod(), call, reason, "field=open");
    return 0;
}

/* battleView() -> table or nil. origin is the lower-left field edge in UI
 * units; native battle Y increases upwards. Camera fields are native offsets
 * in UI units, not world-cell positions. Read-only, including while fighting.
 */
static int lBattleView(lua_State *L) {
    TestFieldBattleView v;
    if (!currentTestFieldBattleView(L, "coe5.battleView", &v)) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 13);
    const struct { const char *name; double value; } numbers[] = {
        {"cellSize", v.cellSize}, {"originX", (float)(v.baseX + v.cameraX)},
        {"originY", (float)(v.baseY + v.cameraY)}, {"baseX", v.baseX}, {"baseY", v.baseY},
        {"cameraX", v.cameraX}, {"cameraY", v.cameraY},
    };
    for (size_t i = 0; i < sizeof numbers / sizeof numbers[0]; i++) {
        lua_pushnumber(L, numbers[i].value);
        lua_setfield(L, -2, numbers[i].name);
    }
    const struct { const char *name; int value; } ints[] = {
        {"width", v.width}, {"height", v.height}, {"screenW", v.screenW}, {"screenH", v.screenH},
        {"windowW", v.windowW}, {"windowH", v.windowH},
    };
    for (size_t i = 0; i < sizeof ints / sizeof ints[0]; i++) {
        lua_pushinteger(L, ints[i].value);
        lua_setfield(L, -2, ints[i].name);
    }
    return 1;
}

/* battleScreenToCell(uiX, uiY) -> cellX, cellY or nil. Accepts the same UI
 * units as uiPointerX/Y, not desktop/window pixels. GUI occlusion is the
 * caller's responsibility: this identifies a field cell, not a free placement.
 */
static int lBattleScreenToCell(lua_State *L) {
    double x = luaL_checknumber(L, 1), y = luaL_checknumber(L, 2);
    TestFieldBattleView v;
    if (!currentTestFieldBattleView(L, "coe5.battleScreenToCell", &v)) {
        lua_pushnil(L);
        return 1;
    }
    if (!isfinite(x) || !isfinite(y) || fabs(x) > FLT_MAX || fabs(y) > FLT_MAX ||
        x < 0 || y < 0 || x >= v.screenW || y >= v.screenH) {
        lua_pushnil(L);
        return 1;
    }
    int cellX = -1, cellY = -1;
#ifndef __APPLE__
    ((void (*)(float, float, int *, int *))(void *)(gameBase + RVA_TEST_FIELD_SCREEN_TO_CELL))(
        (float)x, (float)y, &cellX, &cellY);
#endif
    if (cellX < 0 || cellY < 0 || cellX >= v.width || cellY >= v.height) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, cellX);
    lua_pushinteger(L, cellY);
    return 2;
}

/* battleCellToScreen(cellX, cellY) -> UI centreX, centreY or nil. A valid
 * field cell may project outside the visible screen when the camera scrolls.
 */
static int lBattleCellToScreen(lua_State *L) {
    lua_Integer x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2);
    TestFieldBattleView v;
    if (!currentTestFieldBattleView(L, "coe5.battleCellToScreen", &v) ||
        x < 0 || y < 0 || x >= v.width || y >= v.height) {
        lua_pushnil(L);
        return 1;
    }
    float sx = ((float)x * v.cellSize + v.baseX) + v.cameraX;
    float sy = ((float)y * v.cellSize + v.baseY) + v.cameraY;
    lua_pushnumber(L, sx + v.cellSize * 0.5f);
    lua_pushnumber(L, sy + v.cellSize * 0.5f);
    return 2;
}
