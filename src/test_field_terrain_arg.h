#pragma once

/* Shared with the standalone parser regression. Does not read or mutate game state.
 * lua_tointegerx accepts numeric strings: check the type first intentionally. */
static const char *readTestFieldTerrainArg(lua_State *L, int table, int last, int tableMax,
                                         lua_Integer *terrain, int *specified) {
    *terrain = 0;
    *specified = 0;
    lua_getfield(L, table, "terrain");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return NULL;
    }
    int integer = 0;
    lua_Integer value = lua_type(L, -1) == LUA_TNUMBER ? lua_tointegerx(L, -1, &integer) : 0;
    lua_pop(L, 1);
    if (!integer || value < 0 || value > last || value >= tableMax || value > SHRT_MAX) return "bad_terrain";
    *terrain = value;
    *specified = 1;
    return NULL;
}
