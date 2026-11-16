#pragma once
#include <float.h>
#include <math.h>

typedef struct {
    float cellSize, baseX, baseY, cameraX, cameraY;
    int width, height, screenW, screenH, windowW, windowH;
} BattleView;

static int readBattleView(lua_State *L, const char *call, BattleView *out) {
    checkLuaMainThread(L, call);
    if (!testFieldIsOpen(L, call)) return 0;
    const char *reason = NULL;
    BattleView v = {0};
    if (!gameCodeMatches) reason = "game_code_differs";
    else if (battleDrawing <= 0 && battleWaiting <= 0 && luaSiteDepth[SITE_BATTLE_DRAW] <= 0 && luaSiteDepth[SITE_BATTLE_WAIT] <= 0) reason = "not_battle_frame";
    else {
        v = (BattleView){
            .cellSize = GF(RVA_BATTLE_CELL_PX),
            .baseX = GF(RVA_BATTLE_VIEW_ORIGIN_X),
            .baseY = GF(RVA_BATTLE_VIEW_ORIGIN_Y),
            .cameraX = GF(RVA_BATTLE_VIEW_SCROLL_X),
            .cameraY = GF(RVA_BATTLE_VIEW_SCROLL_Y),
            .width = G32(RVA_BATTLE_WIDTH),
            .height = BATTLE_ROWS,
            .screenW = G32(RVA_SCREEN_W),
            .screenH = G32(RVA_SCREEN_H),
            .windowW = G32(RVA_WINDOW_W),
            .windowH = G32(RVA_WINDOW_H),
        };
        if (!(v.cellSize > 0) || !isfinite(v.cellSize) || !isfinite(v.baseX) || !isfinite(v.baseY) || !isfinite(v.cameraX) || !isfinite(v.cameraY) ||
            v.width < 1 || v.width > BATTLE_COLUMNS_MAX || v.screenW < 1 || v.screenH < 1 || v.windowW < 1 || v.windowH < 1)
            reason = "invalid_battle_view";
    }
    if (reason) {
        refuseLuaCall(runningLuaMod(), call, reason, "field=open");
        return 0;
    }
    *out = v;
    return 1;
}

static int lBattleView(lua_State *L) {
    BattleView v;
    if (!readBattleView(L, "coe5.battleView", &v)) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 13);
    const struct {
        const char *name;
        double value;
    } numbers[] = {
        {"cellSize", v.cellSize}, {"originX", (float)(v.baseX + v.cameraX)}, {"originY", (float)(v.baseY + v.cameraY)}, {"baseX", v.baseX},
        {"baseY", v.baseY},       {"cameraX", v.cameraX},                    {"cameraY", v.cameraY},
    };
    for (size_t i = 0; i < sizeof numbers / sizeof numbers[0]; i++) {
        lua_pushnumber(L, numbers[i].value);
        lua_setfield(L, -2, numbers[i].name);
    }
    const struct {
        const char *name;
        int value;
    } ints[] = {
        {"width", v.width}, {"height", v.height}, {"screenW", v.screenW}, {"screenH", v.screenH}, {"windowW", v.windowW}, {"windowH", v.windowH},
    };
    for (size_t i = 0; i < sizeof ints / sizeof ints[0]; i++) setIntField(L, ints[i].name, ints[i].value);
    return 1;
}

// @platform-split battle_cell_at_point: Windows calls the game's function; Mac's compiler copied it into the two click tests, so CoE5SE's copy runs there
static void battleCellAtPoint(const BattleView *v, float x, float y, int *cellX, int *cellY) {
#ifdef __APPLE__
    *cellX = *cellY = -1;
    for (int row = 0; row < BATTLE_ROWS; row++) {
        float bottom = v->cameraY + fmaf(v->cellSize, (float)row, v->baseY);
        if (!(bottom <= y && v->cellSize + bottom > y)) continue;
        for (int column = 0; column < v->width; column++) {
            float left = v->cameraX + fmaf(v->cellSize, (float)column, v->baseX);
            if (left <= x && v->cellSize + left > x) {
                *cellX = column;
                *cellY = row;
                return;
            }
        }
    }
#else
    (void)v;
    GAME_FN(void (*)(float, float, int *, int *), RVA_BATTLE_CELL_AT_POINT)(x, y, cellX, cellY);
#endif
}

static int lBattleScreenToCell(lua_State *L) {
    double x = luaL_checknumber(L, 1), y = luaL_checknumber(L, 2);
    BattleView v;
    if (!readBattleView(L, "coe5.battleScreenToCell", &v)) {
        lua_pushnil(L);
        return 1;
    }
    int cellX = -1, cellY = -1;
    if (isfinite(x) && isfinite(y) && fabs(x) <= FLT_MAX && fabs(y) <= FLT_MAX && x >= 0 && y >= 0 && x < v.screenW && y < v.screenH)
        battleCellAtPoint(&v, (float)x, (float)y, &cellX, &cellY);
    if (cellX < 0 || cellY < 0 || cellX >= v.width || cellY >= v.height) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, cellX);
    lua_pushinteger(L, cellY);
    return 2;
}

static int lBattleCellToScreen(lua_State *L) {
    lua_Integer x = luaL_checkinteger(L, 1), y = luaL_checkinteger(L, 2);
    BattleView v;
    if (!readBattleView(L, "coe5.battleCellToScreen", &v) || x < 0 || y < 0 || x >= v.width || y >= v.height) {
        lua_pushnil(L);
        return 1;
    }
    float sx = ((float)x * v.cellSize + v.baseX) + v.cameraX;
    float sy = ((float)y * v.cellSize + v.baseY) + v.cameraY;
    lua_pushnumber(L, sx + v.cellSize * 0.5f);
    lua_pushnumber(L, sy + v.cellSize * 0.5f);
    return 2;
}
