#pragma once

typedef struct {
    int unit, monster, count;
    unsigned long long generation, session;
    int path[MONSTER_ATTACK_MAX], level[MONSTER_ATTACK_MAX];
} TestFieldTraining;

#define TEST_FIELD_TRAINING_MAX 2000
static TestFieldTraining testFieldTraining[TEST_FIELD_TRAINING_MAX];
static int testFieldTrainingIndex[UNIT_TABLE_MAX];
static int testFieldTrainingCount;
static unsigned testFieldTrainingCombatReads;
static int (*gameUnitSpellWeaponLevel)(int unit, int path);
static int (*gameUnitPathLevel)(int unit, int path);
static struct {
    int spellLevel, pathLevel, attackRow;
#ifdef __APPLE__
    int (*pathExpansionLevel)(int, int);
#endif
} testFieldTrainingHooks;

static int isTestFieldTrainingLive(const TestFieldTraining *t) {
    return testField.open && t->unit >= 0 && t->unit < UNIT_TABLE_MAX && isTestFieldActor(t->unit) &&
           t->generation == testFieldLife.actors[t->unit].generation && t->session == testFieldLife.session &&
           G16(RVA_UNIT_HP + (size_t)t->unit * UNIT_SIZE) > 0 && t->monster == (short)G16(RVA_UNIT_MONSTER + (size_t)t->unit * UNIT_SIZE);
}

static TestFieldTraining *testFieldTrainingOf(int unit) {
    if (!testField.open || unit < 0 || unit >= UNIT_TABLE_MAX) return NULL;
    int index = testFieldTrainingIndex[unit] - 1;
    if (index >= 0 && index < testFieldTrainingCount && testFieldTraining[index].unit == unit && isTestFieldTrainingLive(&testFieldTraining[index]))
        return &testFieldTraining[index];
    return NULL;
}

static int testFieldTrainingLevel(const TestFieldTraining *t, int path) {
    for (int i = 0; t && i < t->count; i++)
        if (t->path[i] == path) return t->level[i];
    return -1;
}

static int testFieldSpellWeaponLevelHook(int unit, int path) {
    int level = gameUnitSpellWeaponLevel(unit, path);
    const TestFieldTraining *t = testFieldTrainingOf(unit);
    if (path >= 0) {
        int set = testFieldTrainingLevel(t, path);
        return set >= 0 ? set : level;
    }
    if (path == -1 && t) {
        level = 0;
        for (int i = 0; i < t->count; i++)
            if (t->level[i] > level) level = t->level[i];
    }
    return level;
}

static int testFieldPathLevelHook(int unit, int path) {
    const TestFieldTraining *t = testFieldTrainingOf(unit);
    int level = testFieldTrainingLevel(t, path);
    if (level >= 0) {
        level += GAME_FN(int (*)(int, int), RVA_UNIT_PATH_BONUS)(unit, path);
        return level > 0 ? level : 0;
    }
    if (path == -1 && t && t->count > 0) {
        int highest = 0;
        for (int i = 0; i < t->count; i++)
            if (t->level[i] > highest) highest = t->level[i];
        highest += GAME_FN(int (*)(int, int), RVA_UNIT_PATH_BONUS)(unit, -1);
        return highest > 0 ? highest : 0;
    }
    return gameUnitPathLevel(unit, path);
}

static void adjustTestFieldTrainingRow(int unit, int type, int weapon, long long *value) {
    if (type < 0 || type >= 20 || !((0x8c048u >> type) & 1u)) return;
    int level = testFieldTrainingLevel(testFieldTrainingOf(unit), weapon);
    if (level < 0) return;
    *value = (*value & ~0xffffffffLL) | (unsigned)level;
    if (testField.fighting) testFieldTrainingCombatReads++;
}

static void turnOffTestFieldTraining(void) {
    if (testFieldTrainingHooks.pathLevel && fanpatchHookEnable(gameBase + RVA_UNIT_PATH_LEVEL, 0)) testFieldTrainingHooks.pathLevel = 0;
#ifdef __APPLE__
    if (testFieldTrainingHooks.spellLevel) {
        unitSpellWeaponLevel = testFieldTrainingHooks.pathExpansionLevel;
        if (macRestoreCalls(RVA_UNIT_SPELL_WEAPON_LEVEL, (void *)testFieldSpellWeaponLevelHook, "unit_spell_weapon_level")) testFieldTrainingHooks.spellLevel = 0;
    }
#else
    if (testFieldTrainingHooks.spellLevel && fanpatchHookEnable(gameBase + RVA_UNIT_SPELL_WEAPON_LEVEL, 0)) testFieldTrainingHooks.spellLevel = 0;
#endif
    if (testFieldTrainingHooks.attackRow) {
        adjustTestFieldAttackRow = NULL;
        if (returnUnitAttackRowHook()) testFieldTrainingHooks.attackRow = 0;
    }
    testFieldTrainingCount = 0;
}

// @platform-split test_field_spell_level: Windows hooks the spell level reader's entry; Mac points its callers at the hook (macRedirectCalls), and spell_path_expansion.h's own reads through its function pointer
static int turnOnTestFieldTraining(void) {
    if (testFieldTrainingHooks.spellLevel && testFieldTrainingHooks.pathLevel && testFieldTrainingHooks.attackRow) return 1;
    void *pathLevel = gameBase + RVA_UNIT_PATH_LEVEL;
    if (!testFieldTrainingHooks.attackRow) testFieldTrainingHooks.attackRow = lendUnitAttackRowHook();
    if (!testFieldTrainingHooks.pathLevel)
        testFieldTrainingHooks.pathLevel = fanpatchHookCreate(pathLevel, (void *)testFieldPathLevelHook, (void **)&gameUnitPathLevel) && fanpatchHookEnable(pathLevel, 1);
#ifdef __APPLE__
    if (!testFieldTrainingHooks.spellLevel) {
        gameUnitSpellWeaponLevel = GAME_FN(int (*)(int, int), RVA_UNIT_SPELL_WEAPON_LEVEL);
        testFieldTrainingHooks.spellLevel = macRedirectCalls(RVA_UNIT_SPELL_WEAPON_LEVEL, (void *)testFieldSpellWeaponLevelHook, "unit_spell_weapon_level") > 0;
        if (testFieldTrainingHooks.spellLevel) {
            testFieldTrainingHooks.pathExpansionLevel = unitSpellWeaponLevel;
            unitSpellWeaponLevel = testFieldSpellWeaponLevelHook;
        }
    }
#else
    void *spellLevel = gameBase + RVA_UNIT_SPELL_WEAPON_LEVEL;
    if (!testFieldTrainingHooks.spellLevel)
        testFieldTrainingHooks.spellLevel = fanpatchHookCreate(spellLevel, (void *)testFieldSpellWeaponLevelHook, (void **)&gameUnitSpellWeaponLevel) && fanpatchHookEnable(spellLevel, 1);
#endif
    int ok = testFieldTrainingHooks.spellLevel && testFieldTrainingHooks.pathLevel && testFieldTrainingHooks.attackRow;
    writeResultLog(ok, "test_field_training_hooks", "spell_level=%d path_level=%d attack_row=%d", testFieldTrainingHooks.spellLevel, testFieldTrainingHooks.pathLevel,
                   testFieldTrainingHooks.attackRow);
    if (ok) adjustTestFieldAttackRow = adjustTestFieldTrainingRow;
    else turnOffTestFieldTraining();
    return ok;
}

static const char *testFieldLoadoutProblem(LuaScript *script, int unit) {
    const char *reason = testFieldUnitEditProblem(script, unit);
    if (!reason && testField.activePlayer >= 0) reason = "not_main_menu";
    if (!reason && !turnOnTestFieldTraining()) reason = "training_hooks_unavailable";
    return reason;
}

static int isLoadedItem(int id) {
    return id >= 0 && id < tableRecords(TABLE_ITEM) && ((const char *)(tableAddress(TABLE_ITEM) + (size_t)id * ITEM_SIZE))[0];
}

static int isImplantItem(int id) {
    return id >= 0 && (itemFirstAbility(id, ITEM_ABILITY_EYE) > 0 || itemFirstAbility(id, ITEM_ABILITY_HEART) > 0);
}

static unsigned testFieldOrganInjuries(int unit) {
    unsigned long long flags;
    memcpy(&flags, GAME_PTR(RVA_UNITS + (size_t)unit * UNIT_SIZE), sizeof flags);
    return (unsigned)(flags & 0x700000ULL);
}

static int testFieldSameImplants(const short *before, const short *after) {
    for (int p = 0; p < ITEM_POSITION_MAX; p++) {
        int ids[2] = {before[p], after[p]};
        for (int k = 0; k < 2; k++)
            if (ids[k] >= 0) {
                if (!isLoadedItem(ids[k])) return 0;
                if (isImplantItem(ids[k])) {
                    int a = 0, b = 0;
                    for (int q = 0; q < ITEM_POSITION_MAX; q++) {
                        a += before[q] == ids[k];
                        b += after[q] == ids[k];
                    }
                    if (a != b) return 0;
                }
            }
    }
    return 1;
}

static void readTestFieldItems(int unit, short *items) {
    for (int p = 0; p < ITEM_POSITION_MAX; p++) items[p] = (short)unitItemAt(unit, p);
}
static int testFieldItemsAre(int unit, const short *items) {
    for (int p = 0; p < ITEM_POSITION_MAX; p++)
        if (unitItemAt(unit, p) != items[p]) return 0;
    return 1;
}
static void writeTestFieldItems(int unit, const short *items) {
    for (int p = 0; p < ITEM_POSITION_MAX; p++) setUnitItemAt(unit, p, items[p]);
    flushUnitItemCaches(unit);
}

static int pushTestFieldLoadoutResult(lua_State *L, lua_Integer unit, const char *reason, int rollback) {
    if (reason) refuseLuaCall(runningLuaMod(), "coe5.setTestFieldConfiguration", reason, "unit=%lld rollback_verified=%d", (long long)unit, rollback);
    lua_pushboolean(L, reason == NULL);
    if (reason) lua_pushstring(L, reason);
    else lua_pushnil(L);
    lua_newtable(L);
    setBoolField(L, "rollbackVerified", rollback);
    lua_pushstring(L, reason ? "validate" : "committed");
    lua_setfield(L, -2, "stage");
    setIntField(L, "combatAttackReads", testFieldTrainingCombatReads);
    return 3;
}

static const char *readTestFieldItemList(lua_State *L, int state, short *items) {
    lua_pushliteral(L, "items");
    lua_rawget(L, state);
    if (!lua_istable(L, -1) || lua_rawlen(L, -1) != ITEM_POSITION_MAX) {
        lua_pop(L, 1);
        return "bad_item_positions";
    }
    for (int p = 0; p < ITEM_POSITION_MAX; p++) {
        lua_Integer id;
        lua_rawgeti(L, -1, p + 1);
        int good = readTestFieldInteger(L, -1, &id);
        lua_pop(L, 1);
        if (!good || id < -1 || id > SHRT_MAX || (id >= 0 && !isLoadedItem((int)id))) {
            lua_pop(L, 1);
            return "bad_item";
        }
        items[p] = (short)id;
    }
    lua_pop(L, 1);
    return NULL;
}

static const char *readTestFieldSchools(lua_State *L, int state, int unit, TestFieldTraining *wanted) {
    lua_pushliteral(L, "schools");
    lua_rawget(L, state);
    if (!lua_istable(L, -1) || lua_rawlen(L, -1) > MONSTER_ATTACK_MAX) {
        lua_pop(L, 1);
        return "bad_schools";
    }
    wanted->count = (int)lua_rawlen(L, -1);
    for (int i = 0; i < wanted->count; i++) {
        lua_rawgeti(L, -1, i + 1);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 2);
            return "bad_schools";
        }
        lua_Integer path, level;
        lua_pushliteral(L, "path");
        lua_rawget(L, -2);
        int good = readTestFieldInteger(L, -1, &path);
        lua_pop(L, 1);
        lua_pushliteral(L, "level");
        lua_rawget(L, -2);
        good = good && readTestFieldInteger(L, -1, &level);
        lua_pop(L, 2);
        if (!good || path < 0 || path >= spellPathCount() || level < 1 || level > INT_MAX) {
            lua_pop(L, 1);
            return "bad_school_level";
        }
        int native = gameUnitSpellWeaponLevel(unit, (int)path);
        if (native <= 0) {
            lua_pop(L, 1);
            return "school_not_existing";
        }
        if (level > 3 && level != native) {
            lua_pop(L, 1);
            return "unsupported_school_level";
        }
        for (int j = 0; j < i; j++)
            if (wanted->path[j] == path) {
                lua_pop(L, 1);
                return "duplicate_school";
            }
        wanted->path[i] = (int)path;
        wanted->level[i] = (int)level;
    }
    int nativeCount = 0;
    for (int path = 0; path < spellPathCount(); path++)
        if (gameUnitSpellWeaponLevel(unit, path) > 0) nativeCount++;
    if (nativeCount != wanted->count) {
        lua_pop(L, 1);
        return "missing_school_configuration";
    }
    lua_pop(L, 1);
    return NULL;
}

static int lTestFieldLoadoutState(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldLoadoutState");
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = -1;
    const char *reason = readTestFieldInteger(L, 1, &unit) && unit >= 0 && unit < UNIT_TABLE_MAX ? testFieldLoadoutProblem(script, (int)unit) : "bad_unit";
    if (reason) return refuseTestFieldCall(L, "coe5.testFieldLoadoutState", reason, 1, "unit=%lld", (long long)unit);
    TestFieldCasterSnapshot caster;
    readTestFieldCaster((int)unit, &caster);
    lua_newtable(L);
    int result = lua_gettop(L);
    setIntField(L, "unit", unit);
    setIntField(L, "generation", testFieldLife.actors[unit].generation);
    setIntField(L, "combatAttackReads", testFieldTrainingCombatReads);
    setIntField(L, "organInjuryFlags", testFieldOrganInjuries((int)unit));
    setBoolField(L, "complete", caster.complete);
    setIntField(L, "maxBase", testFieldSpellWeaponLevelHook((int)unit, -1));
    setIntField(L, "maxEffective", testFieldPathLevelHook((int)unit, -1));
    setBoolField(L, "valid", caster.complete && !caster.detail.reason);
    lua_createtable(L, ITEM_POSITION_MAX, 0);
    for (int p = 0; p < ITEM_POSITION_MAX; p++) {
        int item = unitItemAt((int)unit, p), kind = itemSlotKind((int)unit, p);
        lua_newtable(L);
        setIntField(L, "position", p);
        setIntField(L, "id", item);
        setIntField(L, "slotType", kind);
        if (item >= 0 && isLoadedItem(item)) {
            lua_pushstring(L, (const char *)(tableAddress(TABLE_ITEM) + (size_t)item * ITEM_SIZE));
            lua_setfield(L, -2, "name");
            setIntField(L, "itemType", itemTypeOf(item));
            setBoolField(L, "cursed", (itemFlagsOf(item) & ITEM_FLAG_CURSED) != 0);
            setBoolField(L, "permanentEffects", isImplantItem(item));
        } else if (item >= 0) {
            lua_pushboolean(L, 0);
            lua_setfield(L, result, "complete");
        }
        lua_rawseti(L, -2, p + 1);
    }
    lua_setfield(L, result, "items");
    lua_newtable(L);
    int n = 0;
    for (int path = 0; path < spellPathCount(); path++) {
        int base = gameUnitSpellWeaponLevel((int)unit, path);
        if (base <= 0) continue;
        const char *name = spellPathNames()[path];
        lua_newtable(L);
        setIntField(L, "path", path);
        setIntField(L, "templateBase", base);
        setIntField(L, "base", testFieldSpellWeaponLevelHook((int)unit, path));
        setIntField(L, "level", caster.level[path]);
        setIntField(L, "bonus", caster.bonus[path]);
        setIntField(L, "allowance", caster.allowance[path]);
        lua_pushstring(L, name ? name : "Unnamed school");
        lua_setfield(L, -2, "name");
        lua_rawseti(L, -2, ++n);
    }
    lua_setfield(L, result, "schools");
    lua_newtable(L);
    for (int i = 0; i < caster.knownCount; i++) {
        lua_pushinteger(L, caster.known[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, result, "known");
    lua_newtable(L);
    for (int i = 0; i < caster.memorizedCount; i++) {
        lua_pushinteger(L, caster.memorized[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, result, "memorized");
    return 1;
}

static int lSetTestFieldConfiguration(lua_State *L) {
    checkLuaMainThread(L, "coe5.setTestFieldConfiguration");
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = -1;
    const char *reason = readTestFieldInteger(L, 1, &unit) && unit >= 0 && unit < UNIT_TABLE_MAX ? testFieldLoadoutProblem(script, (int)unit) : "bad_unit";
    unsigned long long generation = reason ? 0 : testFieldLife.actors[unit].generation, session = testFieldLife.session;
    TestFieldTraining wanted = {0}, oldTraining = {0};
    short items[ITEM_POSITION_MAX], oldItems[ITEM_POSITION_MAX];
    int restoreKnown = 0, clearHeldItems = 0, cursedPosition = -1, cursedItem = -1;
    TestFieldCasterSnapshot oldCaster, candidate = {.complete = 1, .detail = {.stage = "validate", .path = -1, .spell = -1, .level = -1}};
    if (!reason && !lua_istable(L, 2)) reason = "bad_state";
    if (!reason) reason = readTestFieldItemList(L, 2, items);
    if (!reason) reason = readTestFieldSchools(L, 2, (int)unit, &wanted);
    if (!reason) reason = readTestFieldSpellList(L, 2, "known", candidate.known, UNIT_KNOWN_SPELLS_MAX, &candidate.knownCount);
    if (!reason) reason = readTestFieldSpellList(L, 2, "memorized", candidate.memorized, UNIT_MEMORIZED_SPELLS_MAX, &candidate.memorizedCount);
    if (!reason) {
        lua_pushliteral(L, "restoreKnown");
        lua_rawget(L, 2);
        if (!lua_isnil(L, -1) && lua_type(L, -1) != LUA_TBOOLEAN) reason = "bad_restore_known";
        restoreKnown = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }
    if (!reason) {
        lua_pushliteral(L, "clearHeldItems");
        lua_rawget(L, 2);
        if (!lua_isnil(L, -1) && lua_type(L, -1) != LUA_TBOOLEAN) reason = "bad_clear_held_items";
        clearHeldItems = lua_toboolean(L, -1);
        lua_pop(L, 1);
#ifdef __APPLE__
        if (!reason && clearHeldItems) reason = "windows_only";
#endif
    }
    if (!reason) {
        lua_pushliteral(L, "removeCursed");
        lua_rawget(L, 2);
        if (!lua_isnil(L, -1)) {
            if (!lua_istable(L, -1)) reason = "bad_cursed_removal";
            else {
                lua_Integer position = -1, item = -1;
                lua_pushliteral(L, "position");
                lua_rawget(L, -2);
                if (!readTestFieldInteger(L, -1, &position) || position < 0 || position >= ITEM_POSITION_MAX) reason = "bad_cursed_removal";
                lua_pop(L, 1);
                lua_pushliteral(L, "item");
                lua_rawget(L, -2);
                if (!readTestFieldInteger(L, -1, &item) || item < 0 || item >= tableRecords(TABLE_ITEM)) reason = "bad_cursed_removal";
                lua_pop(L, 1);
                if (!reason) {
                    cursedPosition = (int)position;
                    cursedItem = (int)item;
                }
            }
        }
        lua_pop(L, 1);
    }
    if (!reason) reason = testFieldLoadoutProblem(script, (int)unit);
    if (!reason && (session != testFieldLife.session || generation != testFieldLife.actors[unit].generation)) reason = "identity_changed";
    if (reason) return pushTestFieldLoadoutResult(L, unit, reason, 1);
    readTestFieldCaster((int)unit, &oldCaster);
    if (!oldCaster.complete) return pushTestFieldLoadoutResult(L, unit, "incomplete_native_configuration", 1);
    readTestFieldItems((int)unit, oldItems);
    unsigned oldOrganInjuries = testFieldOrganInjuries((int)unit);
    int restoreImplantLayout = restoreKnown && testFieldSameImplants(oldItems, items);
    if (clearHeldItems) {
        if (restoreKnown || cursedPosition >= 0) return pushTestFieldLoadoutResult(L, unit, "clear_items_only", 1);
        for (int p = 0; p < ITEM_POSITION_MAX; p++)
            if (items[p] != -1) return pushTestFieldLoadoutResult(L, unit, "clear_items_only", 1);
    }
    if (cursedPosition >= 0) {
        if (oldItems[cursedPosition] != cursedItem || !isLoadedItem(cursedItem) || !(itemFlagsOf(cursedItem) & ITEM_FLAG_CURSED) ||
            items[cursedPosition] != -1)
            return pushTestFieldLoadoutResult(L, unit, "cursed_removal_changed", 1);
        for (int p = 0; p < ITEM_POSITION_MAX; p++)
            if (p != cursedPosition && items[p] != oldItems[p]) return pushTestFieldLoadoutResult(L, unit, "cursed_removal_only", 1);
    }
    if (cursedPosition >= 0 || clearHeldItems) {
        const char *only = clearHeldItems ? "clear_items_only" : "cursed_removal_only";
        for (int i = 0; i < wanted.count; i++)
            if (testFieldSpellWeaponLevelHook((int)unit, wanted.path[i]) != wanted.level[i])
                return pushTestFieldLoadoutResult(L, unit, only, 1);
        if (candidate.knownCount != oldCaster.knownCount || candidate.memorizedCount != oldCaster.memorizedCount ||
            memcmp(candidate.known, oldCaster.known, (size_t)candidate.knownCount * sizeof candidate.known[0]) ||
            memcmp(candidate.memorized, oldCaster.memorized, (size_t)candidate.memorizedCount * sizeof candidate.memorized[0]))
            return pushTestFieldLoadoutResult(L, unit, only, 1);
    }
    for (int p = 0; p < ITEM_POSITION_MAX; p++) {
        if (oldItems[p] >= 0 && !isLoadedItem(oldItems[p])) return pushTestFieldLoadoutResult(L, unit, "bad_existing_item", 1);
        if (oldItems[p] >= 0 && oldItems[p] != items[p] && (itemFlagsOf(oldItems[p]) & ITEM_FLAG_CURSED) && p != cursedPosition && !clearHeldItems &&
            !(restoreImplantLayout && isImplantItem(oldItems[p])))
            return pushTestFieldLoadoutResult(L, unit, "cursed_item", 1);
        if (items[p] != oldItems[p] && (isImplantItem(items[p]) || isImplantItem(oldItems[p])) &&
            !((p == cursedPosition || clearHeldItems) && items[p] == -1) && !restoreImplantLayout)
            return pushTestFieldLoadoutResult(L, unit, "permanent_anatomy_item_unsupported", 1);
    }
    if (!ensureExtraItems()) return pushTestFieldLoadoutResult(L, unit, "out_of_memory", 1);
    TestFieldTraining *entry = testFieldTrainingOf((int)unit);
    if (!entry)
        for (int i = 0; i < testFieldTrainingCount; i++)
            if (!isTestFieldTrainingLive(&testFieldTraining[i])) {
                entry = &testFieldTraining[i];
                break;
            }
    if (!entry && testFieldTrainingCount < TEST_FIELD_TRAINING_MAX) entry = &testFieldTraining[testFieldTrainingCount++];
    if (!entry) return pushTestFieldLoadoutResult(L, unit, "too_many_configured_units", 1);
    oldTraining = *entry;
    wanted.unit = (int)unit;
    wanted.monster = (short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE);
    wanted.generation = testFieldLife.actors[unit].generation;
    wanted.session = testFieldLife.session;
    testFieldTrainingIndex[unit] = (int)(entry - testFieldTraining) + 1;
    *entry = wanted;
    short empty[ITEM_POSITION_MAX];
    memset(empty, 0xff, sizeof empty);
    writeTestFieldItems((int)unit, empty);
    for (int p = 0; !reason && p < ITEM_POSITION_MAX; p++)
        if (items[p] >= 0) {
            if (itemSlotKind((int)unit, p) != itemTypeOf(items[p])) reason = "incompatible_slot";
            else if (!isImplantItem(items[p]) && GAME_FN(CanEquipItemFn, RVA_CAN_EQUIP_ITEM)((int)unit, items[p], 0) < 0)
                reason = "native_item_restriction";
            if (!reason) {
                setUnitItemAt((int)unit, p, items[p]);
                flushUnitItemCaches((int)unit);
            }
        }
    for (int p = 0; !reason && p < ITEM_POSITION_MAX; p++)
        if (items[p] >= 0 && itemSlotKind((int)unit, p) != itemTypeOf(items[p])) reason = "equipment_changes_anatomy";
    if (!reason) reason = checkTestFieldCaster((int)unit, &candidate);
    for (int i = 0; !reason && i < candidate.knownCount; i++) {
        int spell = candidate.known[i], known = 0, path = spellSlot(spell)->path;
        for (int j = 0; j < oldCaster.knownCount; j++)
            if (oldCaster.known[j] == spell) known = 1;
        if (!known && (candidate.level[path] <= 0 || spellSlot(spell)->level > candidate.level[path])) {
            int native = gameUnitSpellWeaponLevel((int)unit, path), supported = native > 3 ? native : 3;
            if (!restoreKnown || native <= 0 || spellSlot(spell)->level > supported) reason = "new_spell_above_level";
        }
    }
    if (!reason) reason = testFieldUnitEditProblem(script, unit);
    short *knownAt = (short *)GAME_PTR(RVA_UNIT_KNOWN_SPELLS + (size_t)unit * UNIT_SIZE);
    short *memorizedAt = (short *)GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + (size_t)unit * UNIT_SIZE);
    if (!reason) {
        memcpy(knownAt, candidate.known, sizeof candidate.known);
        memcpy(memorizedAt, candidate.memorized, sizeof candidate.memorized);
        TestFieldCasterSnapshot actual;
        reason = readTestFieldCaster((int)unit, &actual);
        if (!reason && (!testFieldItemsAre((int)unit, items) || memcmp(knownAt, candidate.known, sizeof candidate.known) ||
                        memcmp(memorizedAt, candidate.memorized, sizeof candidate.memorized)))
            reason = "configuration_readback_failed";
        for (int i = 0; !reason && i < wanted.count; i++)
            if (testFieldSpellWeaponLevelHook((int)unit, wanted.path[i]) != wanted.level[i]) reason = "school_readback_failed";
    }
    if (!reason && testFieldOrganInjuries((int)unit) != oldOrganInjuries) reason = "permanent_effect_changed";
    int rollback = 1;
    if (reason) {
        *entry = oldTraining;
        writeTestFieldItems((int)unit, oldItems);
        memcpy(knownAt, oldCaster.known, sizeof oldCaster.known);
        memcpy(memorizedAt, oldCaster.memorized, sizeof oldCaster.memorized);
        rollback = testFieldItemsAre((int)unit, oldItems) && !memcmp(knownAt, oldCaster.known, sizeof oldCaster.known) &&
                   !memcmp(memorizedAt, oldCaster.memorized, sizeof oldCaster.memorized);
        for (int path = 0; rollback && path < spellPathCount(); path++)
            if (GAME_FN(int (*)(int, int), RVA_UNIT_PATH_LEVEL)((int)unit, path) != oldCaster.level[path]) rollback = 0;
        if (testFieldOrganInjuries((int)unit) != oldOrganInjuries) rollback = 0;
        if (!rollback) reason = "rollback_failed";
    }
    writeLog("test_field_configuration", "mod=%s unit=%lld result=%s rollback_verified=%d restore_known=%d remove_cursed_position=%d remove_cursed_item=%d clear_held_items=%d",
             script->mod, (long long)unit, reason ? reason : "committed", rollback, restoreKnown, cursedPosition, cursedItem, clearHeldItems);
    int result = pushTestFieldLoadoutResult(L, unit, reason, rollback);
    setBoolField(L, "clearHeldItems", clearHeldItems);
    if (cursedPosition >= 0) {
        setIntField(L, "cursedRemovalPosition", cursedPosition);
        setIntField(L, "cursedRemovalItem", cursedItem);
    }
    setBoolField(L, "restoreKnown", restoreKnown);
    lua_pushstring(L, reason ? (rollback ? "rejected_rollback_verified" : "rollback_unverified") : "committed");
    lua_setfield(L, -2, "stage");
    if (candidate.detail.reason) {
        setIntField(L, "path", candidate.detail.path);
        setIntField(L, "spell", candidate.detail.spell);
        setIntField(L, "actual", candidate.detail.actual);
        setIntField(L, "allowed", candidate.detail.allowed);
        setIntField(L, "allowance", candidate.detail.allowed);
        setIntField(L, "level", candidate.detail.level);
    }
    return result;
}
