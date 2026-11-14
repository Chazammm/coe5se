#pragma once

// ---- The battle ---------------------------------------------------------------------------------
static unsigned luaBattleSerial;
static int luaBattleSeed, luaBattleX, luaBattleY;

static void startLuaBattle(int seed, int x, int y) {
    luaBattleSerial++;
    if (!luaBattleSerial) luaBattleSerial = 1;
    luaBattleSeed = seed, luaBattleX = x, luaBattleY = y;
}

static int luaBattleCodeNow(void) {
    return luaBattleSerial && *(const unsigned char *)GAME_PTR(RVA_IN_BATTLE) && luaRunningSite != SITE_COUNT && !isPersonalSite(luaRunningSite);
}

// ---- Loading in a LAN game ----------------------------------------------------------------------
typedef enum { LUA_LOAD_NONE, LUA_LOAD_MAIN, LUA_LOAD_COMMANDS, LUA_LOAD_PASS } LuaLoadPhase;
static LuaLoadPhase luaLoadPhase;
static unsigned luaLoadPass = 1;

static unsigned lanScriptSeed;
static int lanScriptSeedKnown;
static unsigned lanScriptSeedNow(void) {
    if (lanScriptSeedKnown) return lanScriptSeed;
    if (netRoleNow() == NET_ROLE_HOST) {
        fillLuaRandom(&lanScriptSeed, sizeof lanScriptSeed);
        lanScriptSeedKnown = 1;
        writeLog("lan_script_seed_made", "seed=%08x", lanScriptSeed);
        return lanScriptSeed;
    }
    static int logged;
    if (!logged++) writeLogWarning("lan_script_seed_missing", "reason=no_seed_from_host role=%d", netRoleNow());
    return 0;
}

static uint64_t luaNameHash(const char *text) {
    uint32_t h = 2166136261u;
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) h = (h ^ *c) * 16777619u;
    return h;
}

static int luaStateSharedInLan(const LuaScript *s) {
    NetRole role = netRoleNow();
    return (role == NET_ROLE_HOST || role == NET_ROLE_CLIENT) && modClassNow(s->mod) == LUA_CLASS_CONTENT;
}

static unsigned luaStateSeedFor(const LuaScript *s) {
    if (!s || !luaStateSharedInLan(s)) return coe5seMakeLuaSeed();
    return (unsigned)mixLuaHash(((uint64_t)lanScriptSeedNow() << 32) ^ luaNameHash(s->mod) ^ 0x5eedull);
}

// ---- The streams ----------------------------------------------------------------------------------
static void seedLuaRandomState(uint64_t words[4], uint64_t seed) {
    for (int i = 0; i < 4; i++) {
        seed += 0x9e3779b97f4a7c15ull;
        words[i] = mixLuaHash(seed);
    }
}

static uint64_t luaBattleStreamSeed(const char *mod) {
    return ((uint64_t)(uint32_t)luaBattleSeed << 32 | (uint32_t)(luaBattleX * 1000 + luaBattleY)) ^ (luaNameHash(mod) << 16) ^ 0xba771eull;
}
static uint64_t luaLoadStreamSeed(const char *mod, LuaLoadPhase phase) {
    return ((uint64_t)lanScriptSeedNow() << 32 | luaNameHash(mod)) ^ ((uint64_t)phase << 8) ^ 0x10adull;
}

static int luaRandomStreamFor(const LuaScript *s) {
    if (!s) return 1;
    if (luaBattleCodeNow()) return 2;
    if (luaLoadPhase != LUA_LOAD_NONE && luaStateSharedInLan(s)) return 3;
    return 1;
}

static void seedLuaStreamIfDue(lua_State *L, LuaScript *s, int stream) {
    uint64_t seed;
    if (stream == 2) {
        if (s->battleRandomSerial == luaBattleSerial) return;
        s->battleRandomSerial = luaBattleSerial;
        seed = luaBattleStreamSeed(s->mod);
    } else {
        if (s->loadRandomPhase == (int)luaLoadPhase && (luaLoadPhase != LUA_LOAD_PASS || s->loadRandomPass == luaLoadPass)) return;
        s->loadRandomPhase = (int)luaLoadPhase;
        s->loadRandomPass = luaLoadPass;
        seed = luaLoadStreamSeed(s->mod, luaLoadPhase);
    }
    lua_getupvalue(L, lua_upvalueindex(stream), 1);
    uint64_t *words = lua_touserdata(L, -1);
    if (words) seedLuaRandomState(words, seed);
    lua_pop(L, 1);
}

static int lMathRandom(lua_State *L) {
    LuaScript *s = luaScriptOf(L);
    int stream = luaRandomStreamFor(s);
    if (stream != 1) seedLuaStreamIfDue(L, s, stream);
    lua_pushvalue(L, lua_upvalueindex(stream));
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
    return lua_gettop(L);
}

static int lMathRandomSeed(lua_State *L) {
    if (luaRandomStreamFor(luaScriptOf(L)) != 1) return 0;
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
    return lua_gettop(L);
}

// ---- Sorted keys ----------------------------------------------------------------------------------
typedef struct { int rank; lua_Integer i; lua_Number n; int isInt; const char *s; size_t length; int at; } LuaSortKey;
enum { LUA_KEY_NUMBER, LUA_KEY_STRING, LUA_KEY_BOOLEAN, LUA_KEY_UNORDERED };

static void readLuaSortKey(lua_State *L, int index, LuaSortKey *k) {
    memset(k, 0, sizeof *k);
    switch (lua_type(L, index)) {
    case LUA_TNUMBER:
        k->rank = LUA_KEY_NUMBER;
        k->isInt = lua_isinteger(L, index);
        if (k->isInt) k->i = lua_tointeger(L, index);
        k->n = lua_tonumber(L, index);
        break;
    case LUA_TSTRING: k->rank = LUA_KEY_STRING, k->s = lua_tolstring(L, index, &k->length); break;
    case LUA_TBOOLEAN: k->rank = LUA_KEY_BOOLEAN, k->i = lua_toboolean(L, index); break;
    default: k->rank = LUA_KEY_UNORDERED; break;
    }
}

static int compareLuaSortKeys(const void *pa, const void *pb) {
    const LuaSortKey *a = pa, *b = pb;
    if (a->rank != b->rank) return a->rank < b->rank ? -1 : 1;
    switch (a->rank) {
    case LUA_KEY_NUMBER:
        if (a->isInt && b->isInt) return a->i < b->i ? -1 : a->i > b->i;
        if (a->n != b->n) return a->n < b->n ? -1 : 1;
        return a->isInt == b->isInt ? 0 : a->isInt ? -1 : 1;
    case LUA_KEY_STRING: {
        size_t n = a->length < b->length ? a->length : b->length;
        int order = memcmp(a->s, b->s, n);
        if (order) return order;
        return a->length < b->length ? -1 : a->length > b->length;
    }
    case LUA_KEY_BOOLEAN: return (int)(a->i - b->i);
    default: return a->at < b->at ? -1 : a->at > b->at;
    }
}

static void noteUnorderedLuaKey(lua_State *L, int index) {
    LuaScript *s = luaScriptOf(L);
    if (!s || s->unorderedKeysWarned == luaBattleSerial) return;
    s->unorderedKeysWarned = luaBattleSerial;
    writeLogWarning("lua_battle_pairs_unordered", "mod=%s type=%s", s->mod, lua_typename(L, lua_type(L, index)));
}

static void pushSortedLuaKeys(lua_State *L, int t) {
    t = lua_absindex(L, t);
    lua_newtable(L);
    int list = lua_gettop(L), count = 0;
    lua_pushnil(L);
    while (lua_next(L, t)) {
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        lua_rawseti(L, list, ++count);
    }
    coe5seChargeLuaSteps(L, (size_t)count + 1);
    if (count < 2) return;
    LuaSortKey *keys = lua_newuserdatauv(L, (size_t)count * sizeof *keys, 0);
    for (int i = 0; i < count; i++) {
        lua_rawgeti(L, list, i + 1);
        readLuaSortKey(L, -1, &keys[i]);
        keys[i].at = i + 1;
        if (keys[i].rank == LUA_KEY_UNORDERED) noteUnorderedLuaKey(L, -1);
        lua_pop(L, 1);
    }
    qsort(keys, (size_t)count, sizeof *keys, compareLuaSortKeys);
    lua_createtable(L, count, 0);
    for (int i = 0; i < count; i++) {
        lua_rawgeti(L, list, keys[i].at);
        lua_rawseti(L, -2, i + 1);
    }
    lua_replace(L, list);
    lua_pop(L, 1);
}

static int lSortedPairsStep(lua_State *L) {
    lua_Integer at = lua_tointeger(L, lua_upvalueindex(3));
    for (;;) {
        if (lua_rawgeti(L, lua_upvalueindex(2), at) == LUA_TNIL) return 1;
        lua_pushinteger(L, ++at);
        lua_replace(L, lua_upvalueindex(3));
        lua_pushvalue(L, -1);
        if (lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL) return 2;
        lua_pop(L, 2);
    }
}

static int lPairs(lua_State *L) {
    if (!luaBattleCodeNow() || lua_type(L, 1) != LUA_TTABLE || luaL_getmetafield(L, 1, "__pairs") != LUA_TNIL) {
        lua_settop(L, 1);
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_insert(L, 1);
        lua_call(L, 1, LUA_MULTRET);
        return lua_gettop(L);
    }
    lua_settop(L, 1);
    lua_pushvalue(L, 1);
    pushSortedLuaKeys(L, 1);
    lua_pushinteger(L, 1);
    lua_pushcclosure(L, lSortedPairsStep, 3);
    lua_pushvalue(L, 1);
    lua_pushnil(L);
    lua_pushnil(L);
    return 4;
}

static int lNext(lua_State *L) {
    if (!luaBattleCodeNow()) {
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_insert(L, 1);
        lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
        return lua_gettop(L);
    }
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 2);
    lua_pushnil(L);
    LuaSortKey after = {.rank = -1}, best = {0}, k;
    if (!lua_isnil(L, 2)) {
        readLuaSortKey(L, 2, &after);
        lua_pushvalue(L, 2);
        if (lua_rawget(L, 1) == LUA_TNIL) return luaL_error(L, "invalid key to 'next'");
        lua_pop(L, 1);
    }
    int count = 0, found = 0;
    if (after.rank != LUA_KEY_UNORDERED) {
        lua_pushnil(L);
        while (lua_next(L, 1)) {
            lua_pop(L, 1);
            count++;
            readLuaSortKey(L, -1, &k);
            if (k.rank == LUA_KEY_UNORDERED) {
                noteUnorderedLuaKey(L, -1);
                continue;
            }
            if ((after.rank >= 0 && compareLuaSortKeys(&k, &after) <= 0) || (found && compareLuaSortKeys(&k, &best) >= 0)) continue;
            best = k;
            found = 1;
            lua_pushvalue(L, -1);
            lua_replace(L, 3);
        }
    }
    if (!found) {
        if (after.rank == LUA_KEY_UNORDERED) lua_pushvalue(L, 2);
        else lua_pushnil(L);
        while (lua_next(L, 1)) {
            lua_pop(L, 1);
            count++;
            readLuaSortKey(L, -1, &k);
            if (k.rank != LUA_KEY_UNORDERED) continue;
            lua_replace(L, 3);
            found = 1;
            break;
        }
    }
    coe5seChargeLuaSteps(L, (size_t)count + 1);
    if (!found) {
        lua_pushnil(L);
        return 1;
    }
    lua_settop(L, 3);
    lua_pushvalue(L, 3);
    lua_rawget(L, 1);
    return 2;
}

static void wrapLuaDeterminism(lua_State *L) {
    lua_getglobal(L, LUA_MATHLIBNAME);
    lua_getfield(L, -1, "random");
    lua_CFunction random = lua_tocfunction(L, -1);
    lua_getupvalue(L, -1, 1);
    size_t stateSize = lua_rawlen(L, -1);
    lua_pop(L, 1);
    for (int stream = 0; stream < 2; stream++) {
        void *state = lua_newuserdatauv(L, stateSize, 0);
        memset(state, 0, stateSize);
        lua_pushcclosure(L, random, 1);
    }
    lua_pushcclosure(L, lMathRandom, 3);
    lua_setfield(L, -2, "random");
    lua_getfield(L, -1, "randomseed");
    lua_pushcclosure(L, lMathRandomSeed, 1);
    lua_setfield(L, -2, "randomseed");
    lua_pop(L, 1);
    lua_getglobal(L, "pairs");
    lua_pushcclosure(L, lPairs, 1);
    lua_setglobal(L, "pairs");
    lua_getglobal(L, "next");
    lua_pushcclosure(L, lNext, 1);
    lua_setglobal(L, "next");
}

static LuaLoadPhase beginLuaLoadPhase(LuaLoadPhase phase) {
    LuaLoadPhase outer = luaLoadPhase;
    luaLoadPhase = phase;
    return outer;
}
static void endLuaLoadPhase(LuaLoadPhase outer) { luaLoadPhase = outer; }
