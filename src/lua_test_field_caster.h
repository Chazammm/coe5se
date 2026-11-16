#pragma once

/* Native 5.43: allowance is TOTAL capacity, max spell-attack path level plus
 * current unitPathBonus, then +1 when positive. It does not depend on either
 * spell list. memorizeSpell is only an unchecked array insertion primitive.
 * Never call chooseMemorized here: it randomly fills/removes/replaces spells. */
typedef struct {
    const char *reason, *stage;
    int path, spell, actual, allowed, level;
} TestFieldCasterDetail;
typedef struct {
    short known[UNIT_KNOWN_SPELLS_MAX], memorized[UNIT_MEMORIZED_SPELLS_MAX];
    int knownCount, memorizedCount, complete;
    int level[SPELL_PATH_COUNT], bonus[SPELL_PATH_COUNT], allowance[SPELL_PATH_COUNT];
    int knownByPath[SPELL_PATH_COUNT], memorizedByPath[SPELL_PATH_COUNT];
    TestFieldCasterDetail detail;
} TestFieldCasterSnapshot;

static void testFieldCasterProblem(TestFieldCasterSnapshot *s, const char *reason, int path, int spell, int actual, int allowed) {
    if (!s->detail.reason) s->detail = (TestFieldCasterDetail){reason, "validate", path, spell, actual, allowed,
                                                           path >= 0 && path < SPELL_PATH_COUNT ? s->level[path] : -1};
}

static const char *testFieldCasterReadProblem(int unit) {
    if (unit < 0 || unit >= UNIT_TABLE_MAX || unit > G32(RVA_UNIT_COUNT)) return "bad_unit";
    int monster = G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE);
    if (G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE) <= 0) return "dead_unit";
    if (monster < 0 || monster >= MONSTER_TABLE_MAX || !isUsedMonster(monster)) return "bad_monster";
    /* The level/bonus routines read these item records before returning. */
    const short *items = (const short *)GAME_PTR(RVA_UNITS + (size_t)unit * UNIT_SIZE + 0xd4);
    for (int i = 0; i < 8; i++) if (items[i] >= tableRecords(TABLE_ITEM)) return "bad_item";
    return NULL;
}

static int testFieldCasterSpell(int id) {
    return id >= 0 && id < spellTableRecords() && spellSlot(id)->name && spellSlot(id)->name[0] &&
           spellSlot(id)->path >= 0 && spellSlot(id)->path < spellPathCount();
}

static const char *testFieldCasterValidate(int unit, TestFieldCasterSnapshot *s) {
    const char *guard = testFieldCasterReadProblem(unit);
    if (guard) { s->complete = 0; testFieldCasterProblem(s, guard, -1, -1, 0, 0); return s->detail.reason; }
    int paths = spellPathCount();
    if (paths < 1 || paths > SPELL_PATH_COUNT) {
        s->complete = 0; testFieldCasterProblem(s, "bad_path_table", -1, -1, paths, SPELL_PATH_COUNT);
        return s->detail.reason;
    }
    memset(s->knownByPath, 0, sizeof s->knownByPath);
    memset(s->memorizedByPath, 0, sizeof s->memorizedByPath);
#ifndef __APPLE__
    for (int path = 0; path < paths; path++) {
        s->level[path] = GAME_FN(int (*)(int,int), RVA_UNIT_PATH_LEVEL)(unit, path);
        s->bonus[path] = GAME_FN(int (*)(int,int), RVA_UNIT_PATH_BONUS)(unit, path);
        s->allowance[path] = GAME_FN(int (*)(int,int), RVA_MEMORIZE_ALLOWANCE)(unit, path);
    }
#endif
    for (int i = 0; i < s->knownCount; i++) {
        int spell = s->known[i];
        if (!testFieldCasterSpell(spell)) {
            s->complete = 0; testFieldCasterProblem(s, "bad_spell", -1, spell, 0, 0); continue;
        }
        int path = spellSlot(spell)->path;
        s->knownByPath[path]++;
        for (int j = 0; j < i; j++) if (s->known[j] == spell)
            testFieldCasterProblem(s, "duplicate_spell", path, spell, 0, 0);
        /* Known higher-level spells can survive a temporary level debuff. */
    }
    for (int i = 0; i < s->memorizedCount; i++) {
        int spell = s->memorized[i];
        if (!testFieldCasterSpell(spell)) {
            s->complete = 0; testFieldCasterProblem(s, "bad_spell", -1, spell, 0, 0); continue;
        }
        int path = spellSlot(spell)->path, found = 0;
        s->memorizedByPath[path]++;
        for (int j = 0; j < i; j++) if (s->memorized[j] == spell)
            testFieldCasterProblem(s, "duplicate_spell", path, spell, 0, 0);
        for (int j = 0; j < s->knownCount; j++) if (s->known[j] == spell) found = 1;
        if (!found) testFieldCasterProblem(s, "memorized_not_known", path, spell, 0, 0);
        if (spellSlot(spell)->level > s->level[path])
            testFieldCasterProblem(s, "spell_level", path, spell, spellSlot(spell)->level, s->level[path]);
    }
    for (int path = 0; path < paths; path++)
        if (s->memorizedByPath[path] > s->allowance[path])
            testFieldCasterProblem(s, "memorize_limit", path, -1, s->memorizedByPath[path], s->allowance[path]);
    return s->detail.reason;
}

static const char *testFieldCasterRead(int unit, TestFieldCasterSnapshot *s) {
    memset(s, 0, sizeof *s);
    s->complete = 1;
    s->detail.path = s->detail.spell = s->detail.level = -1;
    const char *guard = testFieldCasterReadProblem(unit);
    if (guard) { s->complete = 0; testFieldCasterProblem(s, guard, -1, -1, 0, 0); return s->detail.reason; }
    memcpy(s->known, GAME_PTR(RVA_UNIT_KNOWN_SPELLS + (size_t)unit * UNIT_SIZE), sizeof s->known);
    memcpy(s->memorized, GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + (size_t)unit * UNIT_SIZE), sizeof s->memorized);
    /* Native construction initializes the first terminator, not every unused
     * slot. Bytes after the first negative entry are not active spells. */
    while (s->knownCount < UNIT_KNOWN_SPELLS_MAX && s->known[s->knownCount] >= 0) s->knownCount++;
    while (s->memorizedCount < UNIT_MEMORIZED_SPELLS_MAX && s->memorized[s->memorizedCount] >= 0) s->memorizedCount++;
    return testFieldCasterValidate(unit, s);
}


static void testFieldCasterInt(lua_State *L, const char *key, lua_Integer value) {
    lua_pushinteger(L, value); lua_setfield(L, -2, key);
}
static void testFieldCasterPushDetail(lua_State *L, TestFieldCasterDetail d) {
    lua_createtable(L, 0, 7);
    lua_pushstring(L, d.stage ? d.stage : "validate"); lua_setfield(L, -2, "stage");
    if (d.reason) { lua_pushstring(L, d.reason); lua_setfield(L, -2, "code"); }
    testFieldCasterInt(L, "path", d.path); testFieldCasterInt(L, "spell", d.spell);
    testFieldCasterInt(L, "actual", d.actual); testFieldCasterInt(L, "allowed", d.allowed);
    testFieldCasterInt(L, "allowance", d.allowed); testFieldCasterInt(L, "level", d.level);
}
static int testFieldCasterResult(lua_State *L, const char *call, int unit, const char *reason, TestFieldCasterDetail detail) {
    if (reason) refuseLuaCall(runningLuaMod(), call, reason, "unit=%d path=%d spell=%d actual=%d allowed=%d stage=%s",
                              unit, detail.path, detail.spell, detail.actual, detail.allowed, detail.stage ? detail.stage : "validate");
    lua_pushboolean(L, !reason);
    if (reason) lua_pushstring(L, reason); else lua_pushnil(L);
    testFieldCasterPushDetail(L, detail);
    return 3;
}

static int lTestFieldSpellState(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldSpellState");
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = -1;
    const char *reason = !gameCodeMatches ? "game_code_differs" : !testField.open ? "not_open"
                       : _stricmp(script->mod, testField.owner) ? "not_owner"
                       : !testFieldEditorInteger(L, 1, &unit) || unit < 0 || unit >= UNIT_TABLE_MAX ? "bad_unit"
                       : !testFieldLifeActorOwned((int)unit) ? "not_on_field" : NULL;
#ifdef __APPLE__
    reason = "windows_only";
#endif
    if (!reason) reason = testFieldCasterReadProblem((int)unit);
    if (reason) {
        refuseLuaCall(script->mod, "coe5.testFieldSpellState", reason, "unit=%lld", (long long)unit);
        lua_pushnil(L); lua_pushstring(L, reason); return 2;
    }
    TestFieldCasterSnapshot s;
    testFieldCasterRead((int)unit, &s);
    lua_createtable(L, 0, 9);
    int result = lua_gettop(L);
    testFieldCasterInt(L, "unit", unit);
    testFieldCasterInt(L, "generation", (lua_Integer)testFieldLife.actors[unit].generation);
    lua_pushboolean(L, s.complete); lua_setfield(L, result, "complete");
    lua_pushboolean(L, !s.detail.reason && s.complete); lua_setfield(L, result, "valid");
    lua_createtable(L, s.knownCount, 0);
    for (int i = 0; i < s.knownCount; i++) { lua_pushinteger(L, s.known[i]); lua_rawseti(L, -2, i + 1); }
    lua_setfield(L, result, "known");
    lua_createtable(L, s.memorizedCount, 0);
    for (int i = 0; i < s.memorizedCount; i++) { lua_pushinteger(L, s.memorized[i]); lua_rawseti(L, -2, i + 1); }
    lua_setfield(L, result, "memorized");
    lua_newtable(L); int rows = lua_gettop(L), n = 0;
    for (int path = 0; path < spellPathCount() && path < SPELL_PATH_COUNT; path++) {
        if (!s.level[path] && !s.knownByPath[path] && !s.memorizedByPath[path]) continue;
        const char *name = spellPathsExpanded ? spellPathCopy[path] : ((const char *const *)GAME_PTR(RVA_SPELL_PATH_NAMES))[path];
        char fallback[32]; snprintf(fallback, sizeof fallback, "Path %d", path);
        lua_createtable(L, 0, 7);
        testFieldCasterInt(L, "path", path);
        lua_pushstring(L, name && name[0] ? name : fallback); lua_setfield(L, -2, "name");
        testFieldCasterInt(L, "level", s.level[path]); testFieldCasterInt(L, "bonus", s.bonus[path]);
        testFieldCasterInt(L, "allowance", s.allowance[path]);
        testFieldCasterInt(L, "knownCount", s.knownByPath[path]); testFieldCasterInt(L, "memorizedCount", s.memorizedByPath[path]);
        lua_rawseti(L, rows, ++n);
    }
    lua_setfield(L, result, "schools");
    lua_newtable(L);
    if (s.detail.reason) { testFieldCasterPushDetail(L, s.detail); lua_rawseti(L, -2, 1); }
    lua_setfield(L, result, "issues");
    return 1;
}

static int lMemorizeTestFieldSpell(lua_State *L) {
    checkLuaMainThread(L, "coe5.memorizeTestFieldSpell");
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = -1, spell = -1;
    TestFieldCasterDetail detail = {.stage = "validate", .path = -1, .spell = -1, .level = -1};
    const char *reason = testFieldEditorInteger(L, 1, &unit) ? testFieldUnitEditorProblem(script, unit) : "bad_unit";
    if (!reason && (!testFieldEditorInteger(L, 2, &spell) || spell < 0 || spell > SHRT_MAX || !testFieldCasterSpell((int)spell))) reason = "bad_spell";
    if (!reason && !lua_isboolean(L, 3)) reason = "bad_state";
    int on = lua_toboolean(L, 3);
    if (reason) return testFieldCasterResult(L, "coe5.memorizeTestFieldSpell", (int)unit, reason, detail);
    TestFieldCasterSnapshot before;
    const char *currentProblem = testFieldCasterRead((int)unit, &before);
    int slot = -1, known = 0, path = spellSlot((int)spell)->path;
    for (int i = 0; i < before.memorizedCount; i++) if (before.memorized[i] == spell) slot = i;
    for (int i = 0; i < before.knownCount; i++) if (before.known[i] == spell) known = 1;
    detail.path = path; detail.spell = (int)spell; detail.level = before.level[path];
    detail.actual = before.memorizedByPath[path]; detail.allowed = before.allowance[path];
    if (on && currentProblem) { reason = currentProblem; detail = before.detail; }
    if (on && slot < 0 && !reason) {
        if (!known) reason = "memorized_not_known";
        else if (spellSlot((int)spell)->level > before.level[path]) reason = "spell_level";
        else if (before.memorizedByPath[path] >= before.allowance[path]) { reason = "memorize_limit"; detail.actual++; }
        else if (before.memorizedCount >= UNIT_MEMORIZED_SPELLS_MAX) reason = "too_many_spells";
    }
    if (!on && currentProblem && strcmp(currentProblem, "memorize_limit") && strcmp(currentProblem, "spell_level")) reason = currentProblem;
    if (reason) return testFieldCasterResult(L, "coe5.memorizeTestFieldSpell", (int)unit, reason, detail);
    short wanted[UNIT_MEMORIZED_SPELLS_MAX]; memcpy(wanted, before.memorized, sizeof wanted);
    if (on && slot < 0) {
        wanted[before.memorizedCount] = (short)spell;
        if (before.memorizedCount + 1 < UNIT_MEMORIZED_SPELLS_MAX) wanted[before.memorizedCount + 1] = -1;
    }
    else if (!on && slot >= 0) {
        memmove(wanted + slot, wanted + slot + 1, (UNIT_MEMORIZED_SPELLS_MAX - slot - 1) * sizeof wanted[0]);
        wanted[UNIT_MEMORIZED_SPELLS_MAX - 1] = -1;
    }
    reason = testFieldUnitEditorProblem(script, unit);
    if (reason) return testFieldCasterResult(L, "coe5.memorizeTestFieldSpell", (int)unit, reason, detail);
    short *at = (short *)GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + (size_t)unit * UNIT_SIZE);
    memcpy(at, wanted, sizeof wanted);
    if (memcmp(at, wanted, sizeof wanted) || memcmp(before.known, GAME_PTR(RVA_UNIT_KNOWN_SPELLS + (size_t)unit * UNIT_SIZE), sizeof before.known)) {
        memcpy(at, before.memorized, sizeof before.memorized); reason = "restore_failed";
    }
    detail.stage = reason ? "readback" : "committed";
    return testFieldCasterResult(L, "coe5.memorizeTestFieldSpell", (int)unit, reason, detail);
}
