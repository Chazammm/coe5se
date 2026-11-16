#pragma once

/* Experimental Windows test-field editor operations, not campaign mutations.
 * The existing Lua getters/save code and native add-known/memorize routines use
 * these same bounded, inline signed-short arrays (60 known / 30 memorized).
 * No unit/commander record is copied, and no learn/memorize/death hook is called.
 * Include after lua_test_battle.h; testField.owner/fighting are lifecycle guards. */
#define TEST_FIELD_EDITOR_MAX_COLUMNS 0x600 /* native/battle_maps.h grid capacity */
_Static_assert(UNIT_KNOWN_SPELLS_MAX == 60 && UNIT_MEMORIZED_SPELLS_MAX == 30,
               "test-field spell restore requires the verified inline array sizes");

static const char *testFieldUnitEditorProblem(const LuaScript *script, lua_Integer unit) {
#ifdef __APPLE__
    (void)script; (void)unit;
    return "windows_only";
#else
    if (!gameCodeMatches) return "game_code_differs";
    if (!testField.open) return "not_open";
    if (_stricmp(script->mod, testField.owner)) return "not_owner";
    if (testField.fighting) return "in_fight";
    if (testFieldIsFinalView()) return "final_read_only";
    if (unit < 0 || unit >= UNIT_TABLE_MAX || unit > G32(RVA_UNIT_COUNT)) return "bad_unit";
    if (!testFieldLifeActorOwned((int)unit)) return "not_on_field";
    size_t at = (size_t)unit * UNIT_SIZE;
    if (G16(RVA_UNIT_HP + at) <= 0 || G16(RVA_UNIT_MONSTER + at) < 0) return "dead_unit";
    if (G32(RVA_UNIT_X + at) != testField.x || G32(RVA_UNIT_X + at + 4) != testField.y)
        return "not_on_field";
    int player = G16(RVA_UNIT_PLAYER + at);
    return player == 0 || player == TEST_BATTLE_ENEMY_PLAYER ? NULL : "not_on_field";
#endif
}

static int testFieldEditorResult(lua_State *L, const char *call, lua_Integer unit, const char *reason) {
    if (reason) refuseLuaCall(runningLuaMod(), call, reason, "unit=%lld", (long long)unit);
    lua_pushboolean(L, reason == NULL);
    if (reason) lua_pushstring(L, reason);
    else lua_pushnil(L);
    return 2;
}

static int testFieldEditorInteger(lua_State *L, int index, lua_Integer *value) {
    int exact = 0;
    if (lua_type(L, index) != LUA_TNUMBER) return 0;
    *value = lua_tointegerx(L, index, &exact);
    return exact;
}

/* Only a dense array of unique, used spell IDs is accepted; raw access avoids
 * invoking a table's __index callback partway through validation. */
static const char *readTestFieldSpellList(lua_State *L, int state, const char *key, short *out, int capacity, int *count) {
    for (int i = 0; i < capacity; i++) out[i] = -1;
    lua_pushstring(L, key);
    lua_rawget(L, state);
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return "bad_spell_list"; }
    int list = lua_gettop(L);
    size_t n = lua_rawlen(L, list), entries = 0;
    if (n > (size_t)capacity) { lua_pop(L, 1); return "too_many_spells"; }
    lua_pushnil(L);
    while (lua_next(L, list)) {
        lua_Integer index;
        if (!testFieldEditorInteger(L, -2, &index) || index < 1 || (lua_Unsigned)index > n) {
            lua_pop(L, 3); return "bad_spell_list";
        }
        entries++;
        lua_pop(L, 1);
    }
    if (entries != n) { lua_pop(L, 1); return "bad_spell_list"; }
    for (size_t i = 0; i < n; i++) {
        lua_Integer spell;
        lua_rawgeti(L, list, (lua_Integer)i + 1);
        int exact = testFieldEditorInteger(L, -1, &spell);
        lua_pop(L, 1);
        if (!exact || spell < 0 || spell > SHRT_MAX || spell >= spellTableRecords()) {
            lua_pop(L, 1); return "bad_spell";
        }
        const SpellRecord *record = spellSlot((int)spell);
        if (!record->name || !record->name[0] || record->path < 0 || record->path >= spellPathCount()) {
            lua_pop(L, 1); return "bad_spell";
        }
        for (size_t j = 0; j < i; j++)
            if (out[j] == spell) { lua_pop(L, 1); return "duplicate_spell"; }
        out[i] = (short)spell;
    }
    lua_pop(L, 1);
    *count = (int)n;
    return NULL;
}

#include "lua_test_field_caster.h"

static int lSetTestFieldSpells(lua_State *L) {
    checkLuaMainThread(L, "coe5.setTestFieldSpells");
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = -1;
    const char *reason = testFieldEditorInteger(L, 1, &unit) ? testFieldUnitEditorProblem(script, unit) : "bad_unit";
    short known[UNIT_KNOWN_SPELLS_MAX], memorized[UNIT_MEMORIZED_SPELLS_MAX];
    TestFieldCasterDetail detail = {.stage = "validate", .path = -1, .spell = -1, .level = -1};
    int knownCount = 0, memorizedCount = 0;
    if (!reason && !lua_istable(L, 2)) reason = "bad_state";
    if (!reason) reason = readTestFieldSpellList(L, 2, "known", known, UNIT_KNOWN_SPELLS_MAX, &knownCount);
    if (!reason) reason = readTestFieldSpellList(L, 2, "memorized", memorized, UNIT_MEMORIZED_SPELLS_MAX, &memorizedCount);
    for (int i = 0; !reason && i < memorizedCount; i++) {
        int found = 0;
        for (int k = 0; k < knownCount; k++) if (known[k] == memorized[i]) found = 1;
        if (!found) reason = "memorized_not_known";
    }
    if (!reason) {
        TestFieldCasterSnapshot candidate = {.knownCount = knownCount, .memorizedCount = memorizedCount, .complete = 1,
                                             .detail = {.stage = "validate", .path = -1, .spell = -1, .level = -1}};
        memcpy(candidate.known, known, sizeof known);
        memcpy(candidate.memorized, memorized, sizeof memorized);
        reason = testFieldCasterValidate((int)unit, &candidate);
        detail = candidate.detail;
    }
    /* Recheck the live membership after all validation, before the first write. */
    if (!reason) reason = testFieldUnitEditorProblem(script, unit);
    if (reason) return testFieldCasterResult(L, "coe5.setTestFieldSpells", (int)unit, reason, detail);

    short *knownAt = (short *)GAME_PTR(RVA_UNIT_KNOWN_SPELLS + (size_t)unit * UNIT_SIZE);
    short *memorizedAt = (short *)GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + (size_t)unit * UNIT_SIZE);
    short oldKnown[UNIT_KNOWN_SPELLS_MAX], oldMemorized[UNIT_MEMORIZED_SPELLS_MAX];
    memcpy(oldKnown, knownAt, sizeof oldKnown);
    memcpy(oldMemorized, memorizedAt, sizeof oldMemorized);
    memcpy(knownAt, known, sizeof known);
    memcpy(memorizedAt, memorized, sizeof memorized);
    if (memcmp(knownAt, known, sizeof known) || memcmp(memorizedAt, memorized, sizeof memorized)) {
        memcpy(knownAt, oldKnown, sizeof oldKnown);
        memcpy(memorizedAt, oldMemorized, sizeof oldMemorized);
        reason = "restore_failed";
        detail.stage = "readback";
    }
    if (!reason) detail.stage = "committed";
    writeLog("test_field_spells_restored", "mod=%s unit=%lld known=%d memorized=%d restored=%d",
             script->mod, (long long)unit, knownCount, memorizedCount, reason == NULL);
    return testFieldCasterResult(L, "coe5.setTestFieldSpells", (int)unit, reason, detail);
}

static int lRemoveTestFieldUnit(lua_State *L) {
    checkLuaMainThread(L, "coe5.removeTestFieldUnit");
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = -1;
    const char *reason = testFieldEditorInteger(L, 1, &unit) ? testFieldUnitEditorProblem(script, unit) : "bad_unit";
    if (!reason && !testField.emptyEditor && unit == testField.unit) reason = "primary_commander";
    int width = reason ? 0 : G32(RVA_BATTLE_WIDTH), tokens = 0, oldX = -1, oldY = -1;
    if (!reason && (width < 1 || width > TEST_FIELD_EDITOR_MAX_COLUMNS)) reason = "bad_field";
    for (int x = 0; !reason && x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++)
            if (*(const int *)battleCell(x, y) == unit) { tokens++;oldX = x;oldY = y; }
    if (!reason && tokens > 1) reason = "ambiguous_token";
    if (reason) return testFieldEditorResult(L, "coe5.removeTestFieldUnit", unit, reason);

    /* The same tombstone used by closeTestField, applied to one non-primary
     * temporary unit. No kill/death/event function and no commander unlinking. */
    short *hp = (short *)GAME_PTR(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE), oldHp = *hp;
    if (tokens) *(int *)battleCell(oldX, oldY) = -1;
    *hp = 0;
    if (*hp != 0 || (tokens && *(const int *)battleCell(oldX, oldY) != -1)) {
        *hp = oldHp;
        if (tokens) *(int *)battleCell(oldX, oldY) = (int)unit;
        reason = "remove_failed";
    }
    writeLog("test_field_unit_removed", "mod=%s unit=%lld removed=%d", script->mod, (long long)unit, reason == NULL);
    return testFieldEditorResult(L, "coe5.removeTestFieldUnit", unit, reason);
}

/* Snapshot only the calling mod's main-menu field. Native BattleUnitAt reads
 * dimensions through raw grid handles, so validate every unit-layer reference
 * before calling it. A corrupt field is still returned with explicit issues;
 * no raw unit record, address, unrelated army, or writable memory is exposed. */
typedef struct {
    int unit, object, hit;
    unsigned int flags;
    short ground;
} TestFieldObservedCell;

typedef struct {
    int unit, hp, monster, owner, worldX, worldY, readable, alive, width, height, anchorCount, safeSize;
} TestFieldObservedUnit;

static const char *testFieldReadProblem(const LuaScript *script) {
#ifdef __APPLE__
    (void)script;
    return "windows_only";
#else
    if (!gameCodeMatches) return "game_code_differs";
    if (!testField.open) return "not_open";
    if (_stricmp(script->mod, testField.owner)) return "not_owner";
    if (testField.activePlayer >= 0) return "not_main_menu";
    return NULL;
#endif
}

static int testFieldReadFailure(lua_State *L, const char *call, const char *reason) {
    refuseLuaCall(runningLuaMod(), call, reason, "field_snapshot=unavailable");
    lua_pushnil(L);
    lua_pushstring(L, reason);
    return 2;
}

#ifndef __APPLE__
typedef unsigned int (*TestFieldMouseStateFn)(int *, int *);
static TestFieldMouseStateFn testFieldMouseStateReader(void) {
    HMODULE sdl = GetModuleHandleA("SDL2.dll");
    return sdl ? (TestFieldMouseStateFn)(void *)GetProcAddress(sdl, "SDL_GetMouseState") : NULL;
}
#endif

static int lTestFieldPointerButtons(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldPointerButtons");
    const char *reason = testFieldReadProblem(checkLuaScript(L));
    if (!reason && battleWaiting <= 0 && luaSiteDepth[SITE_BATTLE_WAIT] <= 0) reason = "not_battle_frame";
#ifndef __APPLE__
    TestFieldMouseStateFn read = reason ? NULL : testFieldMouseStateReader();
    if (!reason && !read) reason = "sdl_mouse_unavailable";
    if (!reason) {
        unsigned int raw = read(NULL, NULL);
        /* SDL L/M/R = 1/2/4; the game's frame mask L/R/M = 1/2/4. */
        lua_pushinteger(L, (raw & 1u) | ((raw & 4u) >> 1) | ((raw & 2u) << 1));
        return 1;
    }
#endif
    return testFieldReadFailure(L, "coe5.testFieldPointerButtons", reason);
}

static int lTestFieldEditorCapabilities(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldEditorCapabilities");
    lua_createtable(L, 0, 6);
    lua_pushinteger(L, gameCodeMatches ? 1 : 0);
    lua_setfield(L, -2, "uniqueGuard");
#ifdef __APPLE__
    lua_pushinteger(L, 0);
#else
    lua_pushinteger(L, gameCodeMatches ? 1 : 0);
#endif
    lua_setfield(L, -2, "state");
#ifdef __APPLE__
    lua_pushinteger(L, 0);
#else
    lua_pushinteger(L, gameCodeMatches && testFieldMouseStateReader() ? 1 : 0);
#endif
    lua_setfield(L, -2, "pointerButtons");
    lua_pushinteger(L, gameCodeMatches && testFieldLife.installed >= 0 ? 1 : 0);
    lua_setfield(L, -2, "lifecycle");
    lua_pushinteger(L, gameCodeMatches && testFieldLife.installed >= 0 ? 1 : 0);
    lua_setfield(L, -2, "multipleUnique");
#ifdef __APPLE__
    lua_pushinteger(L, 0);
#else
    lua_pushinteger(L, gameCodeMatches && testFieldLife.installed >= 0 ? 1 : 0);
#endif
    lua_setfield(L, -2, "emptyEditor");
    lua_pushinteger(L, gameCodeMatches ? spellTableRecords() : 0);
    lua_setfield(L, -2, "spellRecords");
#ifdef __APPLE__
    lua_pushinteger(L, 0);
#else
    lua_pushinteger(L, gameCodeMatches && testFieldConfigurationEnabled ? 1 : 0);
#endif
    lua_setfield(L, -2, "configuration");
#ifdef __APPLE__
    lua_pushinteger(L,0);
#else
    lua_pushinteger(L, gameCodeMatches && testFieldConfigurationEnabled ? 1 : 0);
#endif
    lua_setfield(L, -2, "cursedRemoval");
#ifdef __APPLE__
    lua_pushinteger(L,0);
#else
    lua_pushinteger(L, gameCodeMatches && testFieldConfigurationEnabled ? 1 : 0);
#endif
    lua_setfield(L, -2, "schoolLevelReduction");
    lua_pushinteger(L, testFieldConfigurationEnabled); lua_setfield(L, -2, "restoreHigherKnown");
    lua_pushinteger(L, ITEM_POSITION_MAX);lua_setfield(L, -2, "equipmentPositions");
    lua_pushinteger(L, 3);lua_setfield(L, -2, "maxBaseSchoolLevel");
    return 1;
}

static void testFieldStateInt(lua_State *L, const char *key, lua_Integer value) {
    lua_pushinteger(L, value);
    lua_setfield(L, -2, key);
}

static void testFieldStateBool(lua_State *L, const char *key, int value) {
    lua_pushboolean(L, value);
    lua_setfield(L, -2, key);
}

static void testFieldLifePushRow(lua_State *L, int unit, const TestFieldLifeActor *a, TestFieldLifeSnapshot s) {
    static const char *const REASONS[] = {"active", "cleaned", "already_dead", "identity_replaced", "cleanup_failed"};
    lua_createtable(L, 0, 15);
    testFieldStateInt(L, "unit", unit); testFieldStateInt(L, "generation", (lua_Integer)a->generation);
    testFieldStateInt(L, "createdMonster", a->created.monster); testFieldStateInt(L, "createdOwner", a->created.owner);
    testFieldStateInt(L, "createdWorldX", a->created.x); testFieldStateInt(L, "createdWorldY", a->created.y);
    lua_pushstring(L, a->actor ? "actor" : "terrain"); lua_setfield(L, -2, "kind");
    testFieldStateBool(L, "identityMatches", s.valid);
    testFieldStateInt(L, "hp", s.hp); testFieldStateInt(L, "monster", s.monster); testFieldStateInt(L, "owner", s.owner);
    testFieldStateInt(L, "worldX", s.x); testFieldStateInt(L, "worldY", s.y);
    lua_pushstring(L, REASONS[a->cleanup >= 0 && a->cleanup <= 4 ? a->cleanup : 4]);
    lua_setfield(L, -2, "reason");
}

static int lTestFieldLifecycle(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldLifecycle");
    LuaScript *script = checkLuaScript(L);
    const char *reason = !gameCodeMatches ? "game_code_differs"
                       : testFieldLife.installed <= 0 ? "lifecycle_unavailable"
                       : !testFieldLife.session ? "no_session"
                       : _stricmp(script->mod, testFieldLife.owner) ? "not_owner" : NULL;
    if (reason) return testFieldReadFailure(L, "coe5.testFieldLifecycle", reason);
    lua_createtable(L, 0, 11);
    int result = lua_gettop(L);
    testFieldStateInt(L, "session", (lua_Integer)testFieldLife.session);
    testFieldStateInt(L, "creationDepth", testFieldLife.depth);
    testFieldStateInt(L, "uniqueOverrideDepth", testFieldLife.overrideDepth);
    testFieldStateInt(L, "createdCount", (lua_Integer)testFieldLife.createdCount);
    testFieldStateBool(L, "open", testField.open);
    testFieldStateBool(L, "allowMultipleUnique", testFieldLife.allowMultiple);
    testFieldStateBool(L, "complete", !testFieldLife.failed && !testFieldLife.pending);
    const char *keys[3] = {"created", "beforeCleanup", "afterCleanup"};
    for (int phase = 0; phase < 3; phase++) {
        lua_newtable(L); int rows = lua_gettop(L), n = 0;
        for (int unit = 0; unit < UNIT_TABLE_MAX; unit++) {
            TestFieldLifeActor *a = &testFieldLife.actors[unit];
            if (!a->generation || (phase == 1 && !a->beforeTaken) || (phase == 2 && !a->afterTaken)) continue;
            TestFieldLifeSnapshot s = phase == 0 ? a->created : phase == 1 ? a->before : a->after;
            testFieldLifePushRow(L, unit, a, s); lua_rawseti(L, rows, ++n);
        }
        lua_setfield(L, result, keys[phase]);
    }
    lua_newtable(L);
    if (testFieldLife.failed || testFieldLife.pending) {
        lua_createtable(L, 0, 1);
        lua_pushstring(L, testFieldLife.failed ? "tracking_unverified" : "cleanup_unverified");
        lua_setfield(L, -2, "code"); lua_rawseti(L, -2, 1);
    }
    lua_setfield(L, result, "issues");
    return 1;
}

static void testFieldStateIssue(lua_State *L, int issues, int *count, const char *code, int unit, int x, int y) {
    lua_createtable(L, 0, 4);
    lua_pushstring(L, code);
    lua_setfield(L, -2, "code");
    if (unit != -1) testFieldStateInt(L, "unit", unit);
    if (x >= 0) testFieldStateInt(L, "x", x);
    if (y >= 0) testFieldStateInt(L, "y", y);
    lua_rawseti(L, issues, ++*count);
}

static int testFieldStateIdCompare(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int testFieldStateUnitIndex(const TestFieldObservedUnit *units, int count, int unit) {
    int lo = 0, hi = count;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (units[mid].unit < unit) lo = mid + 1;
        else hi = mid;
    }
    return lo < count && units[lo].unit == unit ? lo : -1;
}

static int testFieldStateMembership(int unit) {
    return testFieldLifeActorOwned(unit);
}

static void testFieldStateCoord(lua_State *L, int rows, int row, const char *list, int x, int y) {
    lua_rawgeti(L, rows, row + 1);
    lua_getfield(L, -1, list);
    lua_createtable(L, 0, 2);
    testFieldStateInt(L, "x", x);
    testFieldStateInt(L, "y", y);
    lua_rawseti(L, -2, (lua_Integer)lua_rawlen(L, -2) + 1);
    lua_pop(L, 2);
}

static int pushTestFieldState(lua_State *L, int extraUnit) {
    int width = G32(RVA_BATTLE_WIDTH), last = G32(RVA_UNIT_COUNT);
    if (width < 1 || width > TEST_FIELD_EDITOR_MAX_COLUMNS) {
        /* There is no safe grid rectangle to read. Keep the unavailable portion
         * explicit, rather than treating an empty snapshot as a healthy field. */
        lua_createtable(L, 0, 7);
        testFieldStateBool(L, "emptyEditor", testField.emptyEditor);
        testFieldStateInt(L, "width", width);
        testFieldStateInt(L, "height", BATTLE_ROWS);
        testFieldStateBool(L, "allowMultipleUnique", testField.allowMultipleUnique);
        testFieldStateBool(L, "complete", 0);
        lua_newtable(L); lua_setfield(L, -2, "units");
        lua_newtable(L); lua_setfield(L, -2, "grid");
        lua_newtable(L);
        int issues = lua_gettop(L), count = 0;
        testFieldStateIssue(L, issues, &count, "bad_field_width", -1, -1, -1);
        lua_setfield(L, -2, "issues");
        return 1;
    }
    int cellCount = width * BATTLE_ROWS;
    TestFieldObservedCell *cells = calloc((size_t)cellCount, sizeof *cells);
    int *ids = malloc(((size_t)UNIT_TABLE_MAX + (size_t)cellCount + 1) * sizeof *ids);
    if (!cells || !ids) { free(cells); free(ids); return 0; }
    int idCount = 0;
    /* Scan the physical table capacity, not only lastUnit: an inconsistent
     * high-water mark must not hide an otherwise readable living field unit. */
    for (int u = 0; u < UNIT_TABLE_MAX; u++)
        if (G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE) > 0 && testFieldStateMembership(u)) ids[idCount++] = u;
    if (extraUnit >= 0) ids[idCount++] = extraUnit;
    for (int x = 0; x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            TestFieldObservedCell *c = &cells[x * BATTLE_ROWS + y];
            const unsigned char *raw = battleCell(x, y);
            memcpy(&c->unit, raw, 4); memcpy(&c->object, raw + 4, 4);
            memcpy(&c->flags, raw + 8, 4); memcpy(&c->ground, raw + 12, 2);
            c->hit = -1;
            if (c->unit >= 0) ids[idCount++] = c->unit;
        }
    qsort(ids, (size_t)idCount, sizeof *ids, testFieldStateIdCompare);
    int unique = 0;
    for (int i = 0; i < idCount; i++)
        if (!unique || ids[i] != ids[unique - 1]) ids[unique++] = ids[i];
    TestFieldObservedUnit *units = calloc((size_t)(unique ? unique : 1), sizeof *units);
    if (!units) { free(cells); free(ids); return 0; }

    lua_createtable(L, 0, 7);
    int state = lua_gettop(L);
    testFieldStateInt(L, "width", width); testFieldStateInt(L, "height", BATTLE_ROWS);
    testFieldStateBool(L, "emptyEditor", testField.emptyEditor);
    testFieldStateBool(L, "allowMultipleUnique", testField.allowMultipleUnique);
    lua_newtable(L); int issues = lua_gettop(L), issueCount = 0;
    int complete = 1, safeHit = 1;
    if (last < -1 || last >= UNIT_TABLE_MAX) {
        complete = safeHit = 0;
        testFieldStateIssue(L, issues, &issueCount, "bad_unit_table", -1, -1, -1);
    }
    for (int i = 0; i < unique; i++) {
        TestFieldObservedUnit *u = &units[i];
        *u = (TestFieldObservedUnit){.unit = ids[i], .monster = -1, .owner = -1, .worldX = -1, .worldY = -1};
        if (u->unit < 0 || u->unit >= UNIT_TABLE_MAX) {
            complete = safeHit = 0;
            testFieldStateIssue(L, issues, &issueCount, "invalid_unit", u->unit, -1, -1);
            continue;
        }
        if (!testFieldStateMembership(u->unit)) {
            complete = safeHit = 0;
            testFieldStateIssue(L, issues, &issueCount, "foreign_unit", u->unit, -1, -1);
            continue;
        }
        size_t at = (size_t)u->unit * UNIT_SIZE;
        u->readable = 1;
        u->hp = G16(RVA_UNIT_HP + at); u->monster = G16(RVA_UNIT_MONSTER + at); u->owner = G16(RVA_UNIT_PLAYER + at);
        u->worldX = G32(RVA_UNIT_X + at); u->worldY = G32(RVA_UNIT_X + at + 4); u->alive = u->hp > 0;
        if (u->unit > last) {
            complete = safeHit = 0;
            testFieldStateIssue(L, issues, &issueCount, "past_last_unit", u->unit, -1, -1);
        }
        else if (u->monster < 0 || u->monster >= MONSTER_TABLE_MAX || !isUsedMonster(u->monster)) {
            complete = safeHit = 0;
            testFieldStateIssue(L, issues, &issueCount, "bad_monster", u->unit, -1, -1);
        }
        else {
            u->width = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(u->unit);
            u->height = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(u->unit);
            u->safeSize = u->width >= 1 && u->width <= 3 && u->height >= 1 && u->height <= 3;
            if (!u->safeSize) {
                complete = safeHit = 0;
                testFieldStateIssue(L, issues, &issueCount, "bad_footprint", u->unit, -1, -1);
            }
        }
    }
    free(ids);
    for (int x = 0; x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            TestFieldObservedCell *c = &cells[x * BATTLE_ROWS + y];
            if (c->unit < -1) {
                complete = safeHit = 0;
                testFieldStateIssue(L, issues, &issueCount, "invalid_grid_unit", c->unit, x, y);
            }
            if (c->object < -1 || c->object >= UNIT_TABLE_MAX || c->object > last) {
                complete = 0;
                testFieldStateIssue(L, issues, &issueCount, "invalid_grid_object", c->object, x, y);
            }
            if (c->unit >= 0) {
                int i = testFieldStateUnitIndex(units, unique, c->unit);
                if (i >= 0) units[i].anchorCount++;
            }
        }
    lua_createtable(L, unique, 0); int rows = lua_gettop(L);
    for (int i = 0; i < unique; i++) {
        TestFieldObservedUnit *u = &units[i];
        lua_createtable(L, 0, 15);
        testFieldStateInt(L, "unit", u->unit); testFieldStateInt(L, "hp", u->hp);
        testFieldStateInt(L, "monster", u->monster); testFieldStateInt(L, "owner", u->owner);
        testFieldStateInt(L, "worldX", u->worldX); testFieldStateInt(L, "worldY", u->worldY);
        testFieldStateBool(L, "readable", u->readable); testFieldStateBool(L, "alive", u->alive);
        testFieldStateBool(L, "owned", u->readable);
        if (u->readable) {
            TestFieldLifeActor *a = &testFieldLife.actors[u->unit];
            testFieldStateInt(L, "generation", (lua_Integer)a->generation);
            testFieldStateInt(L, "createdMonster", a->created.monster); testFieldStateInt(L, "createdOwner", a->created.owner);
        }
        testFieldStateInt(L, "width", u->width); testFieldStateInt(L, "height", u->height);
        testFieldStateInt(L, "anchorCount", u->anchorCount);
        lua_newtable(L); lua_setfield(L, -2, "anchors");
        lua_newtable(L); lua_setfield(L, -2, "occupied");
        lua_rawseti(L, rows, i + 1);
        if (u->anchorCount > 1) testFieldStateIssue(L, issues, &issueCount, "duplicate_anchor", u->unit, -1, -1);
        if (u->readable && !u->alive && u->anchorCount)
            testFieldStateIssue(L, issues, &issueCount, "dead_anchor", u->unit, -1, -1);
        if (u->alive && !u->anchorCount)
            testFieldStateIssue(L, issues, &issueCount, "missing_anchor", u->unit, -1, -1);
    }
    if (!safeHit) testFieldStateIssue(L, issues, &issueCount, "hit_test_unavailable", -1, -1, -1);
    lua_createtable(L, cellCount, 0); int grid = lua_gettop(L);
    for (int x = 0; x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            TestFieldObservedCell *c = &cells[x * BATTLE_ROWS + y];
            lua_createtable(L, 0, 6);
            testFieldStateInt(L, "x", x); testFieldStateInt(L, "y", y);
            testFieldStateInt(L, "unit", c->unit); testFieldStateInt(L, "object", c->object);
            testFieldStateInt(L, "flags", c->flags); testFieldStateInt(L, "ground", c->ground);
            lua_rawseti(L, grid, x * BATTLE_ROWS + y + 1);
            if (c->unit >= 0) {
                int i = testFieldStateUnitIndex(units, unique, c->unit);
                if (i >= 0) {
                    testFieldStateCoord(L, rows, i, "anchors", x, y);
                    if (units[i].anchorCount == 1) {
                        lua_rawgeti(L, rows, i + 1);
                        testFieldStateInt(L, "anchorX", x); testFieldStateInt(L, "anchorY", y);
                        lua_pop(L, 1);
                    }
                }
            }
            if (safeHit) {
                c->hit = GAME_FN(int (*)(int, int), RVA_BATTLE_UNIT_AT)(x, y);
                if (c->hit >= 0) {
                    int i = testFieldStateUnitIndex(units, unique, c->hit);
                    if (i >= 0) testFieldStateCoord(L, rows, i, "occupied", x, y);
                    else {
                        complete = 0;
                        testFieldStateIssue(L, issues, &issueCount, "unexpected_hit", c->hit, x, y);
                    }
                }
            }
        }
    /* Overlap is distinct from an incomplete observation: BattleUnitAt's direct
     * anchor takes precedence over another token's derived footprint. */
    for (int x = 0; x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            int id = cells[x * BATTLE_ROWS + y].unit;
            int i = id >= 0 ? testFieldStateUnitIndex(units, unique, id) : -1;
            if (i < 0 || !units[i].safeSize) continue;
            TestFieldObservedUnit *u = &units[i];
            if (x + u->width > width || y + u->height > BATTLE_ROWS) {
                testFieldStateIssue(L, issues, &issueCount, "footprint_outside", id, x, y);
                continue;
            }
            if (safeHit)
                for (int dx = 0; dx < u->width; dx++)
                    for (int dy = 0; dy < u->height; dy++)
                        if (cells[(x + dx) * BATTLE_ROWS + y + dy].hit != id)
                            testFieldStateIssue(L, issues, &issueCount, "overlapping_footprint", id, x + dx, y + dy);
        }
    free(units); free(cells);
    lua_pushvalue(L, grid); lua_setfield(L, state, "grid");
    lua_pushvalue(L, rows); lua_setfield(L, state, "units");
    lua_pushvalue(L, issues); lua_setfield(L, state, "issues");
    lua_settop(L, state);
    testFieldStateBool(L, "complete", complete);
    return 1;
}

static int lTestFieldState(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldState");
    const char *reason = testFieldReadProblem(checkLuaScript(L));
    if (reason) return testFieldReadFailure(L, "coe5.testFieldState", reason);
    if (!pushTestFieldState(L, -1)) return testFieldReadFailure(L, "coe5.testFieldState", "out_of_memory");
    return 1;
}

static int lTestFieldUnitState(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldUnitState");
    lua_Integer id = -1;
    const char *reason = testFieldReadProblem(checkLuaScript(L));
    if (!reason && (!testFieldEditorInteger(L, 1, &id) || id < 0 || id >= UNIT_TABLE_MAX)) reason = "bad_unit";
    if (!reason && !testFieldStateMembership((int)id)) reason = "not_on_field";
    if (reason) return testFieldReadFailure(L, "coe5.testFieldUnitState", reason);
    if (!pushTestFieldState(L, (int)id)) return testFieldReadFailure(L, "coe5.testFieldUnitState", "out_of_memory");
    int state = lua_gettop(L);
    lua_getfield(L, state, "units");
    int rows = lua_gettop(L);
    for (lua_Integer i = 1, n = (lua_Integer)lua_rawlen(L, rows); i <= n; i++) {
        lua_rawgeti(L, rows, i); lua_getfield(L, -1, "unit");
        int match = lua_tointeger(L, -1) == id;
        lua_pop(L, 1);
        if (match) {
            lua_getfield(L, state, "complete"); lua_setfield(L, -2, "complete");
            lua_getfield(L, state, "issues"); lua_setfield(L, -2, "issues");
            return 1;
        }
        lua_pop(L, 1);
    }
    return testFieldReadFailure(L, "coe5.testFieldUnitState", "bad_field");
}
