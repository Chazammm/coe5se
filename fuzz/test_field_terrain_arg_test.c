/* Standalone regression for the actual header used by openTestField/testBattle.
 * Uses upstream's bundled Lua; does not simulate a game or native initialization. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "../src/test_field_terrain_arg.h"
unsigned coe5seMakeLuaSeed(void) { return 17; }
unsigned coe5seHashLuaFloat(lua_Number n) { (void)n; return 0; }
void coe5seChargeLuaSteps(lua_State *L, size_t steps) { (void)L; (void)steps; }
static int cases;
static void check(lua_State *L, const char *expression, int last, int cap, const char *expected, int terrain, int specified) {
    lua_settop(L, 0);
    assert(luaL_dostring(L, expression) == LUA_OK);
    assert(lua_istable(L, 1));
    lua_Integer value = -99; int present = -99;
    const char *reason = readTestFieldTerrainArg(L, 1, last, cap, &value, &present);
    assert(lua_gettop(L) == 1);
    assert(expected ? reason && !strcmp(expected, reason) : reason == NULL);
    if (!expected) assert(value == terrain && present == specified);
    cases++;
}
int main(void) {
    lua_State *L=luaL_newstate(); assert(L);luaL_requiref(L,"_G",luaopen_base,1);lua_pop(L,1);
    check(L,"return {}",355,1000,NULL,0,0);
    check(L,"return {terrain=nil}",355,1000,NULL,0,0);
    check(L,"return {terrain=0}",355,1000,NULL,0,1);
    check(L,"return {terrain=2}",355,1000,NULL,2,1);
    check(L,"return {terrain=2.0}",355,1000,NULL,2,1);
    check(L,"return {terrain=355}",355,1000,NULL,355,1);
    const char *bad[]={"-1","356","1e100","9223372036854775807","0.5","'2'","true","false","{}","function()end","1/0","0/0"};
    for(size_t i=0;i<sizeof bad/sizeof bad[0];i++) {
        char expression[160];snprintf(expression,sizeof expression,"return {terrain=%s}",bad[i]);
        check(L,expression,355,1000,"bad_terrain",0,0);
    }
    check(L,"return {terrain=1000}",2000,1000,"bad_terrain",0,0);
    check(L,"return {terrain=32768}",40000,50000,"bad_terrain",0,0);
    lua_settop(L,0);lua_newtable(L);lua_pushlightuserdata(L,L);lua_setfield(L,1,"terrain");
    lua_Integer v;int present;assert(!strcmp(readTestFieldTerrainArg(L,1,355,1000,&v,&present),"bad_terrain"));cases++;
    lua_close(L);printf("PASS: %d actual terrain-parser cases (outside game)\n",cases);return 0;
}
