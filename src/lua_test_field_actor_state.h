#pragma once

// Native affliction-list code reads these eleven injury bits from the unit status word.
#define TEST_FIELD_INJURY_MASK UINT64_C(0x7ff00000)
typedef struct {
    int hp, xp, kills, maxHp, monster, width, height;
    uint64_t injuryFlags;
} TestFieldActorState;

// Read bounded, understood scalar/flag fields only; this is not a serialized unit record.
static void readTestFieldActorState(int unit, TestFieldActorState *out) {
    size_t at = (size_t)unit * UNIT_SIZE;
    uint64_t flags; memcpy(&flags, GAME_PTR(RVA_UNITS + at), sizeof flags);
    out->injuryFlags = flags & TEST_FIELD_INJURY_MASK;
    out->hp = (short)G16(RVA_UNIT_HP + at);
    out->xp = (short)G16(RVA_UNIT_XP + at);
    out->kills = (short)G16(RVA_UNIT_KILLS + at);
    out->monster = (short)G16(RVA_UNIT_MONSTER + at);
    out->maxHp = GAME_FN(int (*)(int), RVA_UNIT_MAX_HP)(unit);
    out->width = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(unit);
    out->height = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(unit);
}

// Export the supported subset and make the missing complete-record coverage explicit.
static int lTestFieldActorState(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldActorState");
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = luaL_checkinteger(L, 1);
    const char *reason = testFieldUnitEditProblem(script, unit);
    if (reason) { lua_pushnil(L); lua_pushstring(L, reason); return 2; }
    TestFieldActorState state; readTestFieldActorState((int)unit, &state);
    lua_newtable(L);
    setIntField(L, "version", 1); setIntField(L, "unit", unit);
    setIntField(L, "generation", testFieldLife.actors[unit].generation);
    setIntField(L, "hp", state.hp); setIntField(L, "maxHp", state.maxHp);
    setIntField(L, "xp", state.xp); setIntField(L, "kills", state.kills);
    setIntField(L, "monster", state.monster); setIntField(L, "width", state.width);
    setIntField(L, "height", state.height); setIntField(L, "injuryFlags", state.injuryFlags);
    lua_pushboolean(L, 1); lua_setfield(L, -2, "complete");
    lua_pushboolean(L, 0); lua_setfield(L, -2, "fullState");
    lua_pushliteral(L, "HP, XP, kills, native injury bits 20-30; identity/footprint/max-HP checks"); lua_setfield(L, -2, "scope");
    lua_pushliteral(L, "Other status flags, poison/bleeding counters, benefits, commander/opaque state and native effects are not serialized");
    lua_setfield(L, -2, "limitations");
    return 1;
}

// Require actual integer fields using raw reads, without invoking caller metamethods.
static int testFieldActorInteger(lua_State *L, const char *key, lua_Integer low, lua_Integer high, int *out) {
    lua_pushstring(L, key); lua_rawget(L, 2);
    int valid = lua_isinteger(L, -1) && lua_tointeger(L, -1) >= low && lua_tointeger(L, -1) <= high;
    if (valid) *out = (int)lua_tointeger(L, -1);
    lua_pop(L, 1); return valid;
}

// Restore this subset atomically, preserving every other status bit and native record field.
static int lSetTestFieldActorState(lua_State *L) {
    checkLuaMainThread(L, "coe5.setTestFieldActorState");
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = luaL_checkinteger(L, 1);
    const char *reason = testFieldUnitEditProblem(script, unit);
    uint64_t session = testFieldLife.session, generation = reason ? 0 : testFieldLife.actors[unit].generation;
    TestFieldActorState wanted = {0}, before;
    int version = 0, injuryFlags = 0;
    if (!reason && (!lua_istable(L, 2) ||
        !testFieldActorInteger(L, "version", 1, 1, &version) ||
        !testFieldActorInteger(L, "hp", 1, 32767, &wanted.hp) ||
        !testFieldActorInteger(L, "xp", 0, 32767, &wanted.xp) ||
        !testFieldActorInteger(L, "kills", 0, 32767, &wanted.kills) ||
        !testFieldActorInteger(L, "monster", 0, tableRecords(TABLE_MONSTER) - 1, &wanted.monster) ||
        !testFieldActorInteger(L, "width", 1, 3, &wanted.width) ||
        !testFieldActorInteger(L, "height", 1, 3, &wanted.height) ||
        !testFieldActorInteger(L, "maxHp", 1, INT_MAX, &wanted.maxHp) ||
        !testFieldActorInteger(L, "injuryFlags", 0, (int)TEST_FIELD_INJURY_MASK, &injuryFlags))) reason = "bad_actor_state";
    wanted.injuryFlags = (unsigned)injuryFlags;
    if (!reason && (wanted.injuryFlags & ~TEST_FIELD_INJURY_MASK)) reason = "bad_injury_flags";
    if (!reason && (testFieldUnitEditProblem(script, unit) || session != testFieldLife.session ||
        testFieldLife.actors[unit].generation != generation)) reason = "context_changed";
    int rollback = 1;
    int written = 0;
    if (!reason) {
        readTestFieldActorState((int)unit, &before);
        if (wanted.monster != before.monster || wanted.width != before.width || wanted.height != before.height) reason = "actor_identity_changed";
    }
    if (!reason) {
        size_t at = (size_t)unit * UNIT_SIZE;
        uint64_t oldFlags; memcpy(&oldFlags, GAME_PTR(RVA_UNITS + at), sizeof oldFlags);
        uint64_t flags = (oldFlags & ~TEST_FIELD_INJURY_MASK) | wanted.injuryFlags;
        written = 1;
        memcpy(GAME_PTR(RVA_UNITS + at), &flags, sizeof flags);
        *(short *)GAME_PTR(RVA_UNIT_XP + at) = (short)wanted.xp;
        *(short *)GAME_PTR(RVA_UNIT_HP + at) = (short)wanted.hp;
        *(short *)GAME_PTR(RVA_UNIT_KILLS + at) = (short)wanted.kills;
        int maximum = GAME_FN(int (*)(int), RVA_UNIT_MAX_HP)((int)unit);
        if (testFieldUnitEditProblem(script, unit) || session != testFieldLife.session ||
            generation != testFieldLife.actors[unit].generation) reason = "actor_context_changed";
        else if (maximum != wanted.maxHp) reason = "native_max_hp_changed";
        if (!reason) {
            *(short *)GAME_PTR(RVA_UNIT_HP + at) = (short)wanted.hp;
            *(short *)GAME_PTR(RVA_UNIT_XP + at) = (short)wanted.xp;
            *(short *)GAME_PTR(RVA_UNIT_KILLS + at) = (short)wanted.kills;
            TestFieldActorState after; readTestFieldActorState((int)unit, &after);
            if (after.hp != wanted.hp || after.xp != wanted.xp || after.kills != wanted.kills ||
                after.injuryFlags != wanted.injuryFlags || after.maxHp != wanted.maxHp) reason = "actor_state_readback_failed";
        }
        if (reason && (testFieldUnitEditProblem(script, unit) || session != testFieldLife.session ||
                       generation != testFieldLife.actors[unit].generation)) {
            rollback = 0;
            reason = "actor_context_changed";
        } else if (reason) {
            memcpy(GAME_PTR(RVA_UNITS + at), &oldFlags, sizeof oldFlags);
            *(short *)GAME_PTR(RVA_UNIT_HP + at) = (short)before.hp;
            *(short *)GAME_PTR(RVA_UNIT_XP + at) = (short)before.xp;
            *(short *)GAME_PTR(RVA_UNIT_KILLS + at) = (short)before.kills;
            TestFieldActorState restored; readTestFieldActorState((int)unit, &restored);
            rollback = restored.hp == before.hp && restored.xp == before.xp && restored.kills == before.kills &&
                restored.injuryFlags == before.injuryFlags && restored.maxHp == before.maxHp;
            if (!rollback) { reason = "restore_failed"; testFieldLife.failed = testFieldLife.pending = 1; }
        }
    }
    if (written && !rollback && session == testFieldLife.session) testFieldLife.failed = testFieldLife.pending = 1;
    if (reason) refuseLuaCall(script->mod, "coe5.setTestFieldActorState", reason, "unit=%lld rollback=%d", (long long)unit, rollback);
    lua_pushboolean(L, !reason);
    if (reason) lua_pushstring(L, reason); else lua_pushnil(L);
    lua_newtable(L); lua_pushboolean(L, rollback); lua_setfield(L, -2, "rollbackVerified");
    lua_pushliteral(L, "supported_actor_subset"); lua_setfield(L, -2, "scope");
    return 3;
}
