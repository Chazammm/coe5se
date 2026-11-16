#pragma once

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

static void noteTestFieldCasterProblem(TestFieldCasterSnapshot *s, const char *reason, int path, int spell, int actual, int allowed) {
    if (!s->detail.reason)
        s->detail = (TestFieldCasterDetail){reason, "validate", path, spell, actual, allowed, path >= 0 && path < SPELL_PATH_COUNT ? s->level[path] : -1};
}

static const char *testFieldCasterReadProblem(int unit) {
    if (unit < 0 || unit >= UNIT_TABLE_MAX || unit > G32(RVA_UNIT_COUNT)) return "bad_unit";
    int monster = G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE);
    if (G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE) <= 0) return "dead_unit";
    if (monster < 0 || monster >= MONSTER_TABLE_MAX || !isUsedMonster(monster)) return "bad_monster";
    const short *items = (const short *)GAME_PTR(RVA_UNIT_ITEMS + (size_t)unit * UNIT_SIZE);
    for (int i = 0; i < UNIT_ITEM_SLOTS; i++)
        if (items[i] >= tableRecords(TABLE_ITEM)) return "bad_item";
    return NULL;
}

static int isTestFieldSpell(int id) {
    return id >= 0 && id < spellTableRecords() && spellSlot(id)->name && spellSlot(id)->name[0] && spellSlot(id)->path >= 0 &&
           spellSlot(id)->path < spellPathCount();
}

#ifdef __APPLE__
static int memorizeAllowanceCopy(int u, int path);
#endif

// @platform-split test_field_copies: Mac's compiler copied the memorize allowance and the monster token width into their callers, so CoE5SE's copies run there (lua_game_copies.h)
static int unitMemorizeAllowance(int unit, int path) {
#ifdef __APPLE__
    return memorizeAllowanceCopy(unit, path);
#else
    return GAME_FN(int (*)(int, int), RVA_MEMORIZE_ALLOWANCE)(unit, path);
#endif
}

static const char *checkTestFieldCaster(int unit, TestFieldCasterSnapshot *s) {
    const char *guard = testFieldCasterReadProblem(unit);
    if (guard) {
        s->complete = 0;
        noteTestFieldCasterProblem(s, guard, -1, -1, 0, 0);
        return s->detail.reason;
    }
    int paths = spellPathCount();
    if (paths < 1 || paths > SPELL_PATH_COUNT) {
        s->complete = 0;
        noteTestFieldCasterProblem(s, "bad_path_table", -1, -1, paths, SPELL_PATH_COUNT);
        return s->detail.reason;
    }
    memset(s->knownByPath, 0, sizeof s->knownByPath);
    memset(s->memorizedByPath, 0, sizeof s->memorizedByPath);
    for (int path = 0; path < paths; path++) {
        s->level[path] = GAME_FN(int (*)(int, int), RVA_UNIT_PATH_LEVEL)(unit, path);
        s->bonus[path] = GAME_FN(int (*)(int, int), RVA_UNIT_PATH_BONUS)(unit, path);
        s->allowance[path] = unitMemorizeAllowance(unit, path);
    }
    for (int i = 0; i < s->knownCount; i++) {
        int spell = s->known[i];
        if (!isTestFieldSpell(spell)) {
            s->complete = 0;
            noteTestFieldCasterProblem(s, "bad_spell", -1, spell, 0, 0);
            continue;
        }
        int path = spellSlot(spell)->path;
        s->knownByPath[path]++;
        for (int j = 0; j < i; j++)
            if (s->known[j] == spell) noteTestFieldCasterProblem(s, "duplicate_spell", path, spell, 0, 0);
    }
    for (int i = 0; i < s->memorizedCount; i++) {
        int spell = s->memorized[i];
        if (!isTestFieldSpell(spell)) {
            s->complete = 0;
            noteTestFieldCasterProblem(s, "bad_spell", -1, spell, 0, 0);
            continue;
        }
        int path = spellSlot(spell)->path, found = 0;
        s->memorizedByPath[path]++;
        for (int j = 0; j < i; j++)
            if (s->memorized[j] == spell) noteTestFieldCasterProblem(s, "duplicate_spell", path, spell, 0, 0);
        for (int j = 0; j < s->knownCount; j++)
            if (s->known[j] == spell) found = 1;
        if (!found) noteTestFieldCasterProblem(s, "memorized_not_known", path, spell, 0, 0);
        if (spellSlot(spell)->level > s->level[path]) noteTestFieldCasterProblem(s, "spell_level", path, spell, spellSlot(spell)->level, s->level[path]);
    }
    for (int path = 0; path < paths; path++)
        if (s->memorizedByPath[path] > s->allowance[path])
            noteTestFieldCasterProblem(s, "memorize_limit", path, -1, s->memorizedByPath[path], s->allowance[path]);
    return s->detail.reason;
}

static const char *readTestFieldCaster(int unit, TestFieldCasterSnapshot *s) {
    memset(s, 0, sizeof *s);
    s->complete = 1;
    s->detail.path = s->detail.spell = s->detail.level = -1;
    const char *guard = testFieldCasterReadProblem(unit);
    if (guard) {
        s->complete = 0;
        noteTestFieldCasterProblem(s, guard, -1, -1, 0, 0);
        return s->detail.reason;
    }
    memcpy(s->known, GAME_PTR(RVA_UNIT_KNOWN_SPELLS + (size_t)unit * UNIT_SIZE), sizeof s->known);
    memcpy(s->memorized, GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + (size_t)unit * UNIT_SIZE), sizeof s->memorized);
    while (s->knownCount < UNIT_KNOWN_SPELLS_MAX && s->known[s->knownCount] >= 0) s->knownCount++;
    while (s->memorizedCount < UNIT_MEMORIZED_SPELLS_MAX && s->memorized[s->memorizedCount] >= 0) s->memorizedCount++;
    return checkTestFieldCaster(unit, s);
}

static const char *testFieldCasterProblem(int unit) {
    TestFieldCasterSnapshot s;
    return readTestFieldCaster(unit, &s);
}

static void pushTestFieldCasterDetail(lua_State *L, TestFieldCasterDetail d) {
    lua_createtable(L, 0, 7);
    lua_pushstring(L, d.stage ? d.stage : "validate");
    lua_setfield(L, -2, "stage");
    if (d.reason) {
        lua_pushstring(L, d.reason);
        lua_setfield(L, -2, "code");
    }
    setIntField(L, "path", d.path);
    setIntField(L, "spell", d.spell);
    setIntField(L, "actual", d.actual);
    setIntField(L, "allowed", d.allowed);
    setIntField(L, "allowance", d.allowed);
    setIntField(L, "level", d.level);
}

static int pushTestFieldCasterResult(lua_State *L, const char *call, int unit, const char *reason, TestFieldCasterDetail detail) {
    if (reason)
        refuseLuaCall(runningLuaMod(), call, reason, "unit=%d path=%d spell=%d actual=%d allowed=%d stage=%s", unit, detail.path, detail.spell, detail.actual,
                      detail.allowed, detail.stage ? detail.stage : "validate");
    lua_pushboolean(L, !reason);
    if (reason) lua_pushstring(L, reason);
    else lua_pushnil(L);
    pushTestFieldCasterDetail(L, detail);
    return 3;
}

static int lTestFieldSpellState(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldSpellState");
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = -1;
    const char *reason = !gameCodeMatches                                                               ? "game_code_differs"
                       : !testField.open                                                                ? "not_open"
                       : _stricmp(script->mod, testField.owner)                                        ? "not_owner"
                       : !testField.editor                                                              ? "not_editor_field"
                       : !readTestFieldInteger(L, 1, &unit) || unit < 0 || unit >= UNIT_TABLE_MAX      ? "bad_unit"
                       : !isTestFieldActor((int)unit)                                                   ? "not_on_field"
                                                                                                        : NULL;
    if (!reason) reason = testFieldCasterReadProblem((int)unit);
    if (reason) return refuseTestFieldCall(L, "coe5.testFieldSpellState", reason, 1, "unit=%lld", (long long)unit);
    TestFieldCasterSnapshot s;
    readTestFieldCaster((int)unit, &s);
    lua_createtable(L, 0, 9);
    int result = lua_gettop(L);
    setIntField(L, "unit", unit);
    setIntField(L, "generation", (lua_Integer)testFieldLife.actors[unit].generation);
    setBoolField(L, "complete", s.complete);
    setBoolField(L, "valid", !s.detail.reason && s.complete);
    lua_createtable(L, s.knownCount, 0);
    for (int i = 0; i < s.knownCount; i++) {
        lua_pushinteger(L, s.known[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, result, "known");
    lua_createtable(L, s.memorizedCount, 0);
    for (int i = 0; i < s.memorizedCount; i++) {
        lua_pushinteger(L, s.memorized[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, result, "memorized");
    lua_newtable(L);
    int rows = lua_gettop(L), n = 0;
    for (int path = 0; path < spellPathCount() && path < SPELL_PATH_COUNT; path++) {
        if (!s.level[path] && !s.knownByPath[path] && !s.memorizedByPath[path]) continue;
        const char *name = spellPathNames()[path];
        char fallback[32];
        snprintf(fallback, sizeof fallback, "Path %d", path);
        lua_createtable(L, 0, 7);
        setIntField(L, "path", path);
        lua_pushstring(L, name && name[0] ? name : fallback);
        lua_setfield(L, -2, "name");
        setIntField(L, "level", s.level[path]);
        setIntField(L, "bonus", s.bonus[path]);
        setIntField(L, "allowance", s.allowance[path]);
        setIntField(L, "knownCount", s.knownByPath[path]);
        setIntField(L, "memorizedCount", s.memorizedByPath[path]);
        lua_rawseti(L, rows, ++n);
    }
    lua_setfield(L, result, "schools");
    lua_newtable(L);
    if (s.detail.reason) {
        pushTestFieldCasterDetail(L, s.detail);
        lua_rawseti(L, -2, 1);
    }
    lua_setfield(L, result, "issues");
    return 1;
}

static int lMemorizeTestFieldSpell(lua_State *L) {
    checkLuaMainThread(L, "coe5.memorizeTestFieldSpell");
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = -1, spell = -1;
    TestFieldCasterDetail detail = {.stage = "validate", .path = -1, .spell = -1, .level = -1};
    const char *reason = readTestFieldInteger(L, 1, &unit) ? testFieldUnitEditProblem(script, unit) : "bad_unit";
    if (!reason && (!readTestFieldInteger(L, 2, &spell) || spell < 0 || spell > SHRT_MAX || !isTestFieldSpell((int)spell))) reason = "bad_spell";
    if (!reason && !lua_isboolean(L, 3)) reason = "bad_state";
    int on = lua_toboolean(L, 3);
    if (reason) return pushTestFieldCasterResult(L, "coe5.memorizeTestFieldSpell", (int)unit, reason, detail);
    TestFieldCasterSnapshot before;
    const char *currentProblem = readTestFieldCaster((int)unit, &before);
    int slot = -1, known = 0, path = spellSlot((int)spell)->path;
    for (int i = 0; i < before.memorizedCount; i++)
        if (before.memorized[i] == spell) slot = i;
    for (int i = 0; i < before.knownCount; i++)
        if (before.known[i] == spell) known = 1;
    detail.path = path;
    detail.spell = (int)spell;
    detail.level = before.level[path];
    detail.actual = before.memorizedByPath[path];
    detail.allowed = before.allowance[path];
    if (on && currentProblem) {
        reason = currentProblem;
        detail = before.detail;
    }
    if (on && slot < 0 && !reason) {
        if (!known) reason = "memorized_not_known";
        else if (spellSlot((int)spell)->level > before.level[path]) reason = "spell_level";
        else if (before.memorizedByPath[path] >= before.allowance[path]) {
            reason = "memorize_limit";
            detail.actual++;
        } else if (before.memorizedCount >= UNIT_MEMORIZED_SPELLS_MAX) reason = "too_many_spells";
    }
    if (!on && currentProblem && strcmp(currentProblem, "memorize_limit") && strcmp(currentProblem, "spell_level")) reason = currentProblem;
    if (reason) return pushTestFieldCasterResult(L, "coe5.memorizeTestFieldSpell", (int)unit, reason, detail);
    short wanted[UNIT_MEMORIZED_SPELLS_MAX];
    memcpy(wanted, before.memorized, sizeof wanted);
    if (on && slot < 0) {
        wanted[before.memorizedCount] = (short)spell;
        if (before.memorizedCount + 1 < UNIT_MEMORIZED_SPELLS_MAX) wanted[before.memorizedCount + 1] = -1;
    } else if (!on && slot >= 0) {
        memmove(wanted + slot, wanted + slot + 1, (UNIT_MEMORIZED_SPELLS_MAX - slot - 1) * sizeof wanted[0]);
        wanted[UNIT_MEMORIZED_SPELLS_MAX - 1] = -1;
    }
    reason = testFieldUnitEditProblem(script, unit);
    if (reason) return pushTestFieldCasterResult(L, "coe5.memorizeTestFieldSpell", (int)unit, reason, detail);
    short *at = (short *)GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + (size_t)unit * UNIT_SIZE);
    memcpy(at, wanted, sizeof wanted);
    if (memcmp(at, wanted, sizeof wanted) || memcmp(before.known, GAME_PTR(RVA_UNIT_KNOWN_SPELLS + (size_t)unit * UNIT_SIZE), sizeof before.known)) {
        memcpy(at, before.memorized, sizeof before.memorized);
        reason = "restore_failed";
    }
    detail.stage = reason ? "readback" : "committed";
    return pushTestFieldCasterResult(L, "coe5.memorizeTestFieldSpell", (int)unit, reason, detail);
}
