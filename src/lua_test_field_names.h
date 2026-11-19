#pragma once
#include "test_field_name_arg.h"
#include "fanpatch/fanpatch_bridge.h"

/* Ordinary units have no native personal-name string in 5.43. Intercept the
 * game's actual formatter for one owned constructor generation. Commanders
 * instead retain their native name record and native "name the type" format. */
typedef struct {
    uint64_t generation, session;
    int commander, custom, changedCommander;
    char name[TEST_FIELD_NAME_BYTES + 1], original[COMMANDER_NAME_SIZE];
    unsigned reads, combatReads;
} TestFieldName;
static TestFieldName **testFieldNames;
static const char *(*testFieldOriginalShownName)(int);
static const char *(*testFieldOriginalCommanderName)(int);
static int testFieldNameHooks;

_Static_assert(TEST_FIELD_NAME_BYTES + 1 == COMMANDER_NAME_SIZE, "bounded native personal name");
static unsigned testFieldNameTypeDepth;

static int testFieldNameCommander(int unit) {
    int last = G32(RVA_LAST_COMMANDER);
    if (last < -1 || last >= COMMANDER_TABLE_MAX) return -2;
    for (int c = 0; c <= last; c++)
        if (G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE) == unit) return c;
    return -1;
}
static TestFieldName *testFieldNameOf(int unit) {
    if (!testField.open || !testFieldLife.active || !testFieldNames ||
        unit < 0 || unit >= UNIT_TABLE_MAX || !isTestFieldActor(unit) ||
        GetCurrentThreadId() != testFieldLife.thread) return NULL;
    TestFieldName *n = testFieldNames[unit];
    return n && n->session == testFieldLife.session &&
           n->generation == testFieldLife.generations[unit] ? n : NULL;
}
static __attribute__((noinline)) const char *testFieldShownNameHook(int unit) {
    TestFieldName *n = testFieldNameOf(unit);
    /* The native card separately adds the commander's name before this one
     * verified type-name call (CALL1d4a3b, return1d4a40). The canonical
     * commander formatter also needs the original type, not "name the name".
     * Other unit-oriented callers, including battle messages, need the name. */
    int typeOnly = n && n->commander >= 0 && (testFieldNameTypeDepth ||
                   __builtin_return_address(0) == gameBase + RVA_INSPECT_CARD_UNIT_TYPE_CALL + 5);
    if (n && n->custom && !typeOnly) {
        n->reads++; if (testField.fighting) n->combatReads++;
        return n->name;
    }
    return testFieldOriginalShownName(unit);
}
static const char *testFieldCommanderNameHook(int commander) {
    int unit = commander >= 0 && commander < COMMANDER_TABLE_MAX && commander <= G32(RVA_LAST_COMMANDER) ?
               G32(RVA_COMMANDERS + (size_t)commander * COMMANDER_SIZE) : -1;
    TestFieldName *n = testFieldNameOf(unit);
    if (!n || !n->custom || n->commander != commander) return testFieldOriginalCommanderName(commander);
    testFieldNameTypeDepth++;
    const char *name = testFieldOriginalCommanderName(commander);
    testFieldNameTypeDepth--;
    return name;
}

/* Clear only this feature's state, before the owned native records are killed.
 * A commander record reused by a different generation is never overwritten. */
static int testFieldNameClose(void) {
    int complete = 1;
    for (int unit = 0; testFieldNames && unit < UNIT_TABLE_MAX; unit++) {
        TestFieldName *n = testFieldNames[unit]; if (!n) continue;
        if (n->changedCommander && testFieldNameOf(unit) == n && n->commander >= 0 &&
            G32(RVA_COMMANDERS + (size_t)n->commander * COMMANDER_SIZE) == unit) {
            char *record = (char *)GAME_PTR(RVA_COMMANDER_NAMES + (size_t)n->commander * COMMANDER_SIZE);
            if (memcmp(record, n->name, sizeof n->name)) complete = 0;
            else {
                memcpy(record, n->original, sizeof n->original);
                if (memcmp(record, n->original, sizeof n->original)) complete = 0;
            }
        }
        free(n); testFieldNames[unit] = NULL;
    }
    if (testFieldNameHooks > 0) {
        int shown = fanpatchHookRemove(gameBase + RVA_UNIT_SHOWN_NAME);
        int commander = fanpatchHookRemove(gameBase + RVA_COMMANDER_THE_NAME);
        if (!shown || !commander) complete = 0;
        testFieldNameHooks = shown && commander ? 0 : -1;
    }
    if (!complete) testFieldLife.failed = testFieldLife.pending = 1;
    return complete;
}
static int testFieldNameInstall(void) {
    if (testFieldNameHooks) return testFieldNameHooks > 0;
    if (!testFieldNames && !(testFieldNames = calloc(UNIT_TABLE_MAX, sizeof *testFieldNames))) return 0;
    void *shown = gameBase + RVA_UNIT_SHOWN_NAME, *commander = gameBase + RVA_COMMANDER_THE_NAME;
    int madeShown = fanpatchHookCreate(shown, (void *)testFieldShownNameHook, (void **)&testFieldOriginalShownName);
    int madeCommander = fanpatchHookCreate(commander, (void *)testFieldCommanderNameHook, (void **)&testFieldOriginalCommanderName);
    int enabled = madeShown && madeCommander && fanpatchHookEnable(shown, 1) && fanpatchHookEnable(commander, 1);
    if (!enabled) {
        int shownRestored = !madeShown || fanpatchHookRemove(shown);
        int commanderRestored = !madeCommander || fanpatchHookRemove(commander);
        if (!shownRestored || !commanderRestored) testFieldLife.failed = testFieldLife.pending = 1;
    }
    testFieldNameHooks = enabled ? 1 : -1;
    writeLog("test_field_name_hook", "created=%d enabled=%d", madeShown + madeCommander, enabled);
    return enabled;
}
static const char *testFieldNameReadProblem(LuaScript *script, lua_Integer unit) {
    if (!gameCodeMatches) return "game_code_differs";
    if (!testField.open || !testField.editor) return "not_open";
    if (testFieldLife.retiring || testFieldLife.failed || testFieldLife.pending) return "cleanup_unverified";
    if (_stricmp(script->mod, testField.owner)) return "not_owner";
    if (unit < 0 || unit >= UNIT_TABLE_MAX || !isTestFieldActor((int)unit)) return "not_on_field";
    int monster = (short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE);
    return monster < 0 || monster >= MONSTER_TABLE_MAX ? "identity_unavailable" : NULL;
}
static int lTestFieldUnitName(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldUnitName");
    LuaScript *script = checkLuaScript(L); lua_Integer unit = luaL_checkinteger(L, 1);
    const char *reason = testFieldNameReadProblem(script, unit);
    int commander = reason ? -1 : testFieldNameCommander((int)unit);
    if (!reason && commander == -2) reason = "bad_commander_table";
    if (reason) { lua_pushnil(L); lua_pushstring(L, reason); return 2; }
    TestFieldName *n = testFieldNameOf((int)unit);
    if (n && n->commander != commander) { lua_pushnil(L); lua_pushliteral(L, "commander_identity_changed"); return 2; }
    const char *personal = commander >= 0 ? (const char *)GAME_PTR(RVA_COMMANDER_NAMES + (size_t)commander * COMMANDER_SIZE) :
                           n && n->custom ? n->name : "";
    size_t length = strnlen(personal, COMMANDER_NAME_SIZE);
    if (length >= COMMANDER_NAME_SIZE) { lua_pushnil(L); lua_pushliteral(L, "unterminated_name"); return 2; }
    lua_newtable(L);
    setIntField(L, "version", 1); setIntField(L, "unit", unit);
    setIntField(L, "generation", testFieldLife.generations[unit]);
    setIntField(L, "commander", commander); setIntField(L, "maxBytes", TEST_FIELD_NAME_BYTES);
    setIntField(L, "nativeLogLineBytes", 63);
    lua_pushboolean(L, n && n->custom); lua_setfield(L, -2, "custom");
    lua_pushlstring(L, personal, length); lua_setfield(L, -2, "name");
    lua_pushstring(L, commander >= 0 ? "native_commander_record" : "owned_native_formatter_override"); lua_setfield(L, -2, "storage");
    const char *shown = commander >= 0 ? GAME_FN(const char *(*)(int), RVA_COMMANDER_THE_NAME)(commander) :
                                       GAME_FN(const char *(*)(int), RVA_UNIT_SHOWN_NAME)((int)unit);
    lua_pushstring(L, shown); lua_setfield(L, -2, "shown");
    setIntField(L, "formatterReads", n ? n->reads : 0);
    setIntField(L, "combatReads", n ? n->combatReads : 0);
    return 1;
}
static int lSetTestFieldUnitName(lua_State *L) {
    checkLuaMainThread(L, "coe5.setTestFieldUnitName");
    LuaScript *script = checkLuaScript(L); lua_Integer unit = luaL_checkinteger(L, 1);
    const char *reason = testFieldUnitEditProblem(script, unit);
    size_t length = 0; const char *name = NULL;
    if (lua_type(L, 2) != LUA_TSTRING) reason = reason ? reason : "bad_name";
    else { name = lua_tolstring(L, 2, &length); if (!reason) reason = testFieldNameProblem((const unsigned char *)name, length); }
    uint64_t session = testFieldLife.session, generation = reason ? 0 : testFieldLife.generations[unit];
    int commander = reason ? -1 : testFieldNameCommander((int)unit);
    if (!reason && commander == -2) reason = "bad_commander_table";
    if (!reason && !testFieldNameInstall()) reason = "name_hook_unavailable";
    TestFieldName *n = reason ? NULL : testFieldNameOf((int)unit);
    if (!reason && n && n->commander != commander) reason = "commander_identity_changed";
    if (!reason && !n) {
        if (testFieldNames[unit]) { free(testFieldNames[unit]); testFieldNames[unit] = NULL; }
        n = calloc(1, sizeof *n);
        if (!n) reason = "out_of_memory";
        else {
            n->session = session; n->generation = generation; n->commander = commander;
            if (commander >= 0) {
                memcpy(n->original, GAME_PTR(RVA_COMMANDER_NAMES + (size_t)commander * COMMANDER_SIZE), sizeof n->original);
                if (!memchr(n->original, 0, sizeof n->original)) { free(n); n = NULL; reason = "unterminated_name"; }
            }
            if (n) testFieldNames[unit] = n;
        }
    }
    int rollback = 1;
    if (!reason && (testFieldUnitEditProblem(script, unit) || session != testFieldLife.session ||
        generation != testFieldLife.generations[unit] || commander != testFieldNameCommander((int)unit))) reason = "context_changed";
    if (!reason) {
        TestFieldName before = *n; char recordBefore[COMMANDER_NAME_SIZE];
        char *record = commander >= 0 ? (char *)GAME_PTR(RVA_COMMANDER_NAMES + (size_t)commander * COMMANDER_SIZE) : NULL;
        if (record) memcpy(recordBefore, record, sizeof recordBefore);
        memset(n->name, 0, sizeof n->name); memcpy(n->name, name, length); n->custom = length != 0;
        if (record) { memcpy(record, length ? n->name : n->original, sizeof n->original); n->changedCommander = length != 0; }
        const char *readback = record ? record : testFieldShownNameHook((int)unit);
        if ((length && strcmp(readback, name)) || (record && !length && memcmp(record, n->original, sizeof n->original))) {
            *n = before; if (record) memcpy(record, recordBefore, sizeof recordBefore);
            rollback = !memcmp(n, &before, sizeof before) && (!record || !memcmp(record, recordBefore, sizeof recordBefore));
            reason = rollback ? "name_readback_failed" : "restore_failed";
            if (!rollback) testFieldLife.failed = testFieldLife.pending = 1;
        }
        if (!reason) writeLog("test_field_unit_named", "unit=%lld generation=%llu bytes=%zu commander=%d storage=%s", (long long)unit,
                             (unsigned long long)generation, length, commander, record ? "native_record" : "native_formatter");
    }
    if (reason) refuseLuaCall(script->mod, "coe5.setTestFieldUnitName", reason, "unit=%lld", (long long)unit);
    lua_pushboolean(L, !reason); if (reason) lua_pushstring(L, reason); else lua_pushnil(L);
    lua_newtable(L); lua_pushboolean(L, rollback); lua_setfield(L, -2, "rollbackVerified");
    setIntField(L, "version", 1); setIntField(L, "maxBytes", TEST_FIELD_NAME_BYTES);
    return 3;
}
