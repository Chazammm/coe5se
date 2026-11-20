#pragma once

_Static_assert(UNIT_KNOWN_SPELLS_MAX == 60 && UNIT_MEMORIZED_SPELLS_MAX == 30, "the unit record's two spell lists hold 60 and 30 entries");

static const char *testFieldUnitEditProblem(const LuaScript *script, lua_Integer unit) {
#ifndef __APPLE__
    if (testFieldDeploymentBusy) return "field_transaction_active";
#endif
    if (testFieldLife.retiring) return "retirement_in_progress";
    if (!gameCodeMatches) return "game_code_differs";
    if (!testField.open) return "not_open";
    if (!testField.editor) return "not_editor_field";
    if (_stricmp(script->mod, testField.owner)) return "not_owner";
    if (testField.fighting) return "in_fight";
    if (testFieldFinalViewActive()) return "final_read_only";
    if (testFieldLife.pending || testFieldLife.failed) return "cleanup_unverified";
    if (unit < 0 || unit >= UNIT_TABLE_MAX || unit > G32(RVA_UNIT_COUNT)) return "bad_unit";
    if (!isTestFieldActor((int)unit)) return "not_on_field";
    size_t at = (size_t)unit * UNIT_SIZE;
    if (G16(RVA_UNIT_HP + at) <= 0 || G16(RVA_UNIT_MONSTER + at) < 0) return "dead_unit";
    if (G32(RVA_UNIT_X + at) != testField.x || G32(RVA_UNIT_X + at + 4) != testField.y) return "not_on_field";
    int player = G16(RVA_UNIT_PLAYER + at);
    return player == 0 || player == TEST_BATTLE_ENEMY_PLAYER ? NULL : "not_on_field";
}

static int pushTestFieldEditResult(lua_State *L, const char *call, lua_Integer unit, const char *reason) {
    if (reason) refuseLuaCall(runningLuaMod(), call, reason, "unit=%lld", (long long)unit);
    lua_pushboolean(L, reason == NULL);
    if (reason) lua_pushstring(L, reason);
    else lua_pushnil(L);
    return 2;
}

static int readTestFieldInteger(lua_State *L, int index, lua_Integer *value) {
    int exact = 0;
    if (lua_type(L, index) != LUA_TNUMBER) return 0;
    *value = lua_tointegerx(L, index, &exact);
    return exact;
}

static const char *readTestFieldSpellList(lua_State *L, int state, const char *key, short *out, int capacity, int *count) {
    for (int i = 0; i < capacity; i++) out[i] = -1;
    lua_pushstring(L, key);
    lua_rawget(L, state);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return "bad_spell_list";
    }
    int list = lua_gettop(L);
    size_t n = lua_rawlen(L, list), entries = 0;
    if (n > (size_t)capacity) {
        lua_pop(L, 1);
        return "too_many_spells";
    }
    lua_pushnil(L);
    while (lua_next(L, list)) {
        lua_Integer index;
        if (!readTestFieldInteger(L, -2, &index) || index < 1 || (lua_Unsigned)index > n) {
            lua_pop(L, 3);
            return "bad_spell_list";
        }
        entries++;
        lua_pop(L, 1);
    }
    if (entries != n) {
        lua_pop(L, 1);
        return "bad_spell_list";
    }
    for (size_t i = 0; i < n; i++) {
        lua_Integer spell;
        lua_rawgeti(L, list, (lua_Integer)i + 1);
        int exact = readTestFieldInteger(L, -1, &spell);
        lua_pop(L, 1);
        if (!exact || spell < 0 || spell > SHRT_MAX || spell >= spellTableRecords()) {
            lua_pop(L, 1);
            return "bad_spell";
        }
        const SpellRecord *record = spellSlot((int)spell);
        if (!record->name || !record->name[0] || record->path < 0 || record->path >= spellPathCount()) {
            lua_pop(L, 1);
            return "bad_spell";
        }
        for (size_t j = 0; j < i; j++)
            if (out[j] == spell) {
                lua_pop(L, 1);
                return "duplicate_spell";
            }
        out[i] = (short)spell;
    }
    lua_pop(L, 1);
    *count = (int)n;
    return NULL;
}

#include "lua_test_field_caster.h"

static int lRemoveTestFieldUnit(lua_State *L) {
    checkLuaMainThread(L, "coe5.removeTestFieldUnit");
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = -1;
    const char *reason = readTestFieldInteger(L, 1, &unit) ? testFieldUnitEditProblem(script, unit) : "bad_unit";
    if (!reason && !testField.emptyEditor && unit == testField.unit) reason = "primary_commander";
    int width = reason ? 0 : G32(RVA_BATTLE_WIDTH), tokens = 0, oldX = -1, oldY = -1;
    if (!reason && (width < 1 || width > BATTLE_COLUMNS_MAX)) reason = "bad_field";
    for (int x = 0; !reason && x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++)
            if (*(const int *)battleCell(x, y) == unit) {
                tokens++;
                oldX = x;
                oldY = y;
            }
    if (!reason && tokens > 1) reason = "ambiguous_token";
    if (reason) return pushTestFieldEditResult(L, "coe5.removeTestFieldUnit", unit, reason);
#ifndef __APPLE__
    if (!retireTestFieldUnit((int)unit)) {
        testFieldLife.failed = testFieldLife.pending = 1;
        reason = "retirement_unverified";
    }
    (void)oldX;
    (void)oldY;
#else
    short *hp = (short *)GAME_PTR(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE), oldHp = *hp;
    if (tokens) *(int *)battleCell(oldX, oldY) = -1;
    *hp = 0;
    if (*hp != 0 || (tokens && *(const int *)battleCell(oldX, oldY) != -1)) {
        *hp = oldHp;
        if (tokens) *(int *)battleCell(oldX, oldY) = (int)unit;
        reason = "remove_failed";
    }
#endif
    writeLog("test_field_unit_removed", "mod=%s unit=%lld removed=%d", script->mod, (long long)unit, reason == NULL);
    return pushTestFieldEditResult(L, "coe5.removeTestFieldUnit", unit, reason);
}

typedef struct {
    int unit, object, hit;
    unsigned int flags;
    short ground;
} TestFieldObservedCell;

typedef struct {
    int unit, hp, monster, owner, worldX, worldY, readable, alive, width, height, anchorCount, safeSize;
} TestFieldObservedUnit;

static const char *testFieldReadProblem(const LuaScript *script) {
    if (!gameCodeMatches) return "game_code_differs";
    if (!testField.open) return "not_open";
    if (_stricmp(script->mod, testField.owner)) return "not_owner";
    if (testField.activePlayer >= 0) return "not_main_menu";
    return NULL;
}

static int refuseTestFieldRead(lua_State *L, const char *call, const char *reason) {
    refuseLuaCall(runningLuaMod(), call, reason, "field_snapshot=unavailable");
    lua_pushnil(L);
    lua_pushstring(L, reason);
    return 2;
}

typedef unsigned int (*SdlMouseStateFn)(int *x, int *y);
static SdlMouseStateFn sdlMouseState(void) {
    return (SdlMouseStateFn)SDL_FUNCTION("SDL_GetMouseState", RVA_SDL_GET_MOUSE_STATE_SLOT);
}

static int lTestFieldPointerButtons(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldPointerButtons");
    const char *reason = testFieldReadProblem(checkLuaScript(L));
    if (!reason && battleWaiting <= 0 && luaSiteDepth[SITE_BATTLE_WAIT] <= 0) reason = "not_battle_frame";
    SdlMouseStateFn read = reason ? NULL : sdlMouseState();
    if (!reason && !read) reason = "sdl_mouse_unavailable";
    if (reason) return refuseTestFieldRead(L, "coe5.testFieldPointerButtons", reason);
    unsigned int raw = read(NULL, NULL);
    lua_pushinteger(L, (raw & 1u) | ((raw & 4u) >> 1) | ((raw & 2u) << 1));
    return 1;
}

#ifndef __APPLE__
static int testFieldSceneryLookupValid(void);
#endif

static int lTestFieldEditorCapabilities(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldEditorCapabilities");
    int ready = gameCodeMatches != 0, lifecycle = ready && testFieldLife.installed >= 0;
    int multipleUnique = lifecycle;
    lua_createtable(L, 0, 13);
    setIntField(L, "uniqueGuard", ready);
    setIntField(L, "state", ready);
    setIntField(L, "pointerButtons", ready && sdlMouseState());
    setIntField(L, "lifecycle", lifecycle);
    setIntField(L, "multipleUnique", multipleUnique);
    setIntField(L, "emptyEditor", lifecycle);
    setIntField(L, "spellRecords", ready ? spellTableRecords() : 0);
    setIntField(L, "configuration", ready);
    setIntField(L, "cursedRemoval", ready);
    setIntField(L, "schoolLevelReduction", ready);
    setIntField(L, "restoreHigherKnown", ready);
    setIntField(L, "equipmentPositions", ITEM_POSITION_MAX);
    setIntField(L, "maxBaseSchoolLevel", 3);
#ifndef __APPLE__
    setIntField(L, "unitRetirement", lifecycle);
    setIntField(L, "clearHeldItems", lifecycle);
    setIntField(L, "battleLogVisibility", lifecycle);
    setIntField(L, "battleZoom", lifecycle && TEST_FIELD_VERIFIED_WINDOWS_ABI);
    setIntField(L, "battleViewport", lifecycle && TEST_FIELD_VERIFIED_WINDOWS_ABI);
    setIntField(L, "battleCenter", lifecycle && TEST_FIELD_VERIFIED_WINDOWS_ABI);
    setIntField(L, "nativeMonsterInspectVersion", lifecycle && TEST_FIELD_VERIFIED_WINDOWS_ABI && testFieldTextPollReady);
    int provenance = lifecycle && testFieldProvenanceCallsValid();
    setIntField(L, "sceneryVersion", provenance && TEST_FIELD_VERIFIED_WINDOWS_ABI && testFieldSceneryLookupValid());
    setIntField(L, "structureProvenance", provenance);
    setIntField(L, "exactDeploymentVersion", provenance);
    setIntField(L, "initialDeploymentVersion", provenance);
    setIntField(L, "multipleUniquePolicy", provenance);
    setIntField(L, "individualNameVersion", lifecycle);
    setIntField(L, "individualNameMaxBytes", COMMANDER_NAME_SIZE - 1);
    setIntField(L, "nativeLogLineBytes", 63);
    setIntField(L, "minimumWidthVersion", lifecycle);
    setIntField(L, "minimumWidthMin", TEST_FIELD_MIN_WIDTH);
    setIntField(L, "minimumWidthMax", TEST_FIELD_MAX_WIDTH);
#endif
    return 1;
}

static void pushTestFieldMadeUnit(lua_State *L, int unit, const TestFieldMadeUnit *made, TestFieldUnitSnapshot s) {
    static const char *const REASONS[] = {"active", "cleaned", "already_dead", "identity_replaced", "cleanup_failed"};
    lua_createtable(L, 0, 15);
    setIntField(L, "unit", unit);
    setIntField(L, "generation", (lua_Integer)made->generation);
    setIntField(L, "createdMonster", made->created.monster);
    setIntField(L, "createdOwner", made->created.owner);
    setIntField(L, "createdWorldX", made->created.x);
    setIntField(L, "createdWorldY", made->created.y);
    lua_pushstring(L, testFieldUnitKind(made));
    lua_setfield(L, -2, "kind");
    lua_pushstring(L, testFieldCreationSource(made->source));
    lua_setfield(L, -2, "creationSource");
    setBoolField(L, "provenanceVerified", made->source != TEST_FIELD_UNVERIFIED);
    setBoolField(L, "identityMatches", s.valid);
    setIntField(L, "hp", s.hp);
    setIntField(L, "monster", s.monster);
    setIntField(L, "owner", s.owner);
    setIntField(L, "worldX", s.x);
    setIntField(L, "worldY", s.y);
    lua_pushstring(L, REASONS[made->cleanup >= 0 && made->cleanup <= 4 ? made->cleanup : 4]);
    lua_setfield(L, -2, "reason");
}

static int lTestFieldLifecycle(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldLifecycle");
    LuaScript *script = checkLuaScript(L);
    const char *reason = !gameCodeMatches                               ? "game_code_differs"
                       : testFieldLife.installed <= 0                   ? "lifecycle_unavailable"
                       : !testFieldLife.session                         ? "no_session"
                       : _stricmp(script->mod, testFieldLife.owner)    ? "not_owner"
                                                                        : NULL;
    if (reason) return refuseTestFieldRead(L, "coe5.testFieldLifecycle", reason);
    lua_createtable(L, 0, 11);
    int result = lua_gettop(L);
    setIntField(L, "session", (lua_Integer)testFieldLife.session);
    setIntField(L, "creationDepth", testFieldLife.depth);
    setIntField(L, "uniqueOverrideDepth", testFieldLife.overrideDepth);
    setIntField(L, "createdCount", (lua_Integer)testFieldLife.createdCount);
    setBoolField(L, "open", testField.open);
    setBoolField(L, "allowMultipleUnique", testFieldLife.allowMultiple);
    setBoolField(L, "complete", !testFieldLife.failed && !testFieldLife.pending);
    const char *keys[3] = {"created", "beforeCleanup", "afterCleanup"};
    for (int phase = 0; phase < 3; phase++) {
        lua_newtable(L);
        int rows = lua_gettop(L), n = 0;
        for (int unit = 0; unit < UNIT_TABLE_MAX; unit++) {
            TestFieldMadeUnit *made = &testFieldLife.actors[unit];
            if (!made->generation || (phase == 1 && !made->beforeTaken) || (phase == 2 && !made->afterTaken)) continue;
            pushTestFieldMadeUnit(L, unit, made, phase == 0 ? made->created : phase == 1 ? made->before : made->after);
            lua_rawseti(L, rows, ++n);
        }
        lua_setfield(L, result, keys[phase]);
    }
    lua_newtable(L);
    if (testFieldLife.failed || testFieldLife.pending) {
        lua_createtable(L, 0, 1);
        lua_pushstring(L, testFieldLife.failed ? "tracking_unverified" : "cleanup_unverified");
        lua_setfield(L, -2, "code");
        lua_rawseti(L, -2, 1);
    }
    lua_setfield(L, result, "issues");
    return 1;
}

static void addTestFieldIssue(lua_State *L, int issues, int *count, const char *code, int unit, int x, int y) {
    lua_createtable(L, 0, 4);
    lua_pushstring(L, code);
    lua_setfield(L, -2, "code");
    if (unit != -1) setIntField(L, "unit", unit);
    if (x >= 0) setIntField(L, "x", x);
    if (y >= 0) setIntField(L, "y", y);
    lua_rawseti(L, issues, ++*count);
}

static int compareUnitIds(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int findObservedUnit(const TestFieldObservedUnit *units, int count, int unit) {
    int lo = 0, hi = count;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (units[mid].unit < unit) lo = mid + 1;
        else hi = mid;
    }
    return lo < count && units[lo].unit == unit ? lo : -1;
}

static void addTestFieldUnitCell(lua_State *L, int rows, int row, const char *list, int x, int y) {
    lua_rawgeti(L, rows, row + 1);
    lua_getfield(L, -1, list);
    lua_createtable(L, 0, 2);
    setIntField(L, "x", x);
    setIntField(L, "y", y);
    lua_rawseti(L, -2, (lua_Integer)lua_rawlen(L, -2) + 1);
    lua_pop(L, 2);
}

static int pushTestFieldState(lua_State *L, int extraUnit) {
    int width = G32(RVA_BATTLE_WIDTH), last = G32(RVA_UNIT_COUNT);
    if (width < 1 || width > BATTLE_COLUMNS_MAX) {
        lua_createtable(L, 0, 7);
        setBoolField(L, "emptyEditor", testField.emptyEditor);
        setIntField(L, "width", width);
        setIntField(L, "height", BATTLE_ROWS);
        setBoolField(L, "allowMultipleUnique", testField.allowMultipleUnique);
        setBoolField(L, "complete", 0);
        lua_newtable(L);
        lua_setfield(L, -2, "units");
        lua_newtable(L);
        lua_setfield(L, -2, "grid");
        lua_newtable(L);
        int issues = lua_gettop(L), count = 0;
        addTestFieldIssue(L, issues, &count, "bad_field_width", -1, -1, -1);
        lua_setfield(L, -2, "issues");
        return 1;
    }
    int cellCount = width * BATTLE_ROWS;
    TestFieldObservedCell *cells = calloc((size_t)cellCount, sizeof *cells);
    int *ids = malloc(((size_t)UNIT_TABLE_MAX + (size_t)cellCount + 1) * sizeof *ids);
    if (!cells || !ids) {
        free(cells);
        free(ids);
        return 0;
    }
    int idCount = 0;
    for (int u = 0; u < UNIT_TABLE_MAX; u++)
        if (G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE) > 0 && isTestFieldBattler(u)) ids[idCount++] = u;
    if (extraUnit >= 0) ids[idCount++] = extraUnit;
    for (int x = 0; x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            TestFieldObservedCell *c = &cells[x * BATTLE_ROWS + y];
            const unsigned char *raw = battleCell(x, y);
            memcpy(&c->unit, raw, 4);
            memcpy(&c->object, raw + BATTLE_CELL_OBJECT, 4);
            memcpy(&c->flags, raw + BATTLE_CELL_FLAGS, 4);
            memcpy(&c->ground, raw + BATTLE_CELL_GROUND, 2);
            c->hit = -1;
            if (c->unit >= 0) ids[idCount++] = c->unit;
        }
    qsort(ids, (size_t)idCount, sizeof *ids, compareUnitIds);
    int unique = 0;
    for (int i = 0; i < idCount; i++)
        if (!unique || ids[i] != ids[unique - 1]) ids[unique++] = ids[i];
    TestFieldObservedUnit *units = calloc((size_t)(unique ? unique : 1), sizeof *units);
    if (!units) {
        free(cells);
        free(ids);
        return 0;
    }

    lua_createtable(L, 0, 7);
    int state = lua_gettop(L);
#ifndef __APPLE__
    setIntField(L, "classificationVersion", 1);
#endif
    setIntField(L, "session", (lua_Integer)testFieldLife.session);
    setIntField(L, "width", width);
    setIntField(L, "height", BATTLE_ROWS);
    setBoolField(L, "emptyEditor", testField.emptyEditor);
    setBoolField(L, "allowMultipleUnique", testField.allowMultipleUnique);
    lua_newtable(L);
    int issues = lua_gettop(L), issueCount = 0;
    int complete = !testFieldLife.failed && !testFieldLife.pending, safeHit = 1;
    if (!complete) addTestFieldIssue(L, issues, &issueCount, "cleanup_unverified", -1, -1, -1);
    if (last < -1 || last >= UNIT_TABLE_MAX) {
        complete = safeHit = 0;
        addTestFieldIssue(L, issues, &issueCount, "bad_unit_table", -1, -1, -1);
    }
    for (int i = 0; i < unique; i++) {
        TestFieldObservedUnit *u = &units[i];
        *u = (TestFieldObservedUnit){.unit = ids[i], .monster = -1, .owner = -1, .worldX = -1, .worldY = -1};
        if (u->unit < 0 || u->unit >= UNIT_TABLE_MAX) {
            complete = safeHit = 0;
            addTestFieldIssue(L, issues, &issueCount, "invalid_unit", u->unit, -1, -1);
            continue;
        }
        if (!isTestFieldBattler(u->unit)) {
            complete = safeHit = 0;
            addTestFieldIssue(L, issues, &issueCount, "foreign_unit", u->unit, -1, -1);
            continue;
        }
        size_t at = (size_t)u->unit * UNIT_SIZE;
        u->readable = 1;
        u->hp = G16(RVA_UNIT_HP + at);
        u->monster = G16(RVA_UNIT_MONSTER + at);
        u->owner = G16(RVA_UNIT_PLAYER + at);
        u->worldX = G32(RVA_UNIT_X + at);
        u->worldY = G32(RVA_UNIT_X + at + 4);
        u->alive = u->hp > 0;
        if (u->unit > last) {
            complete = safeHit = 0;
            addTestFieldIssue(L, issues, &issueCount, "past_last_unit", u->unit, -1, -1);
        } else if (u->monster < 0 || u->monster >= MONSTER_TABLE_MAX || !isUsedMonster(u->monster)) {
            complete = safeHit = 0;
            addTestFieldIssue(L, issues, &issueCount, "bad_monster", u->unit, -1, -1);
        } else {
            u->width = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(u->unit);
            u->height = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(u->unit);
            u->safeSize = u->width >= 1 && u->width <= 3 && u->height >= 1 && u->height <= 3;
            if (!u->safeSize) {
                complete = safeHit = 0;
                addTestFieldIssue(L, issues, &issueCount, "bad_footprint", u->unit, -1, -1);
            }
        }
    }
    free(ids);
    for (int x = 0; x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            TestFieldObservedCell *c = &cells[x * BATTLE_ROWS + y];
            if (c->unit < -1) {
                complete = safeHit = 0;
                addTestFieldIssue(L, issues, &issueCount, "invalid_grid_unit", c->unit, x, y);
            }
            if (c->object < -1 || c->object >= UNIT_TABLE_MAX || c->object > last) {
                complete = 0;
                addTestFieldIssue(L, issues, &issueCount, "invalid_grid_object", c->object, x, y);
            }
            if (c->unit >= 0) {
                int i = findObservedUnit(units, unique, c->unit);
                if (i >= 0) units[i].anchorCount++;
            }
        }
    lua_createtable(L, unique, 0);
    int rows = lua_gettop(L);
    for (int i = 0; i < unique; i++) {
        TestFieldObservedUnit *u = &units[i];
        lua_createtable(L, 0, 15);
        setIntField(L, "unit", u->unit);
        setIntField(L, "hp", u->hp);
        setIntField(L, "monster", u->monster);
        setIntField(L, "owner", u->owner);
        setIntField(L, "worldX", u->worldX);
        setIntField(L, "worldY", u->worldY);
        setBoolField(L, "readable", u->readable);
        setBoolField(L, "alive", u->alive);
        setBoolField(L, "owned", u->readable);
        if (u->readable) {
            TestFieldMadeUnit *made = &testFieldLife.actors[u->unit];
            setIntField(L, "generation", (lua_Integer)made->generation);
            setIntField(L, "createdMonster", made->created.monster);
            setIntField(L, "createdOwner", made->created.owner);
            lua_pushstring(L, testFieldUnitKind(made));
            lua_setfield(L, -2, "kind");
            lua_pushstring(L, testFieldCreationSource(made->source));
            lua_setfield(L, -2, "creationSource");
            lua_pushliteral(L, "unit");
            lua_setfield(L, -2, "anchorLayer");
            setBoolField(L, "provenanceVerified", made->source != TEST_FIELD_UNVERIFIED);
        }
        setIntField(L, "width", u->width);
        setIntField(L, "height", u->height);
        setIntField(L, "anchorCount", u->anchorCount);
        lua_newtable(L);
        lua_setfield(L, -2, "anchors");
        lua_newtable(L);
        lua_setfield(L, -2, "occupied");
        lua_rawseti(L, rows, i + 1);
        if (u->anchorCount > 1) addTestFieldIssue(L, issues, &issueCount, "duplicate_anchor", u->unit, -1, -1);
        if (u->readable && !u->alive && u->anchorCount) addTestFieldIssue(L, issues, &issueCount, "dead_anchor", u->unit, -1, -1);
        if (u->alive && !u->anchorCount) addTestFieldIssue(L, issues, &issueCount, "missing_anchor", u->unit, -1, -1);
    }
    if (!safeHit) addTestFieldIssue(L, issues, &issueCount, "hit_test_unavailable", -1, -1, -1);
    lua_createtable(L, cellCount, 0);
    int grid = lua_gettop(L);
    for (int x = 0; x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            TestFieldObservedCell *c = &cells[x * BATTLE_ROWS + y];
            lua_createtable(L, 0, 6);
            setIntField(L, "x", x);
            setIntField(L, "y", y);
            setIntField(L, "unit", c->unit);
            setIntField(L, "object", c->object);
            setIntField(L, "flags", c->flags);
            setIntField(L, "ground", c->ground);
            lua_rawseti(L, grid, x * BATTLE_ROWS + y + 1);
            if (c->unit >= 0) {
                int i = findObservedUnit(units, unique, c->unit);
                if (i >= 0) {
                    addTestFieldUnitCell(L, rows, i, "anchors", x, y);
                    if (units[i].anchorCount == 1) {
                        lua_rawgeti(L, rows, i + 1);
                        setIntField(L, "anchorX", x);
                        setIntField(L, "anchorY", y);
                        lua_pop(L, 1);
                    }
                }
            }
            if (!safeHit) continue;
            c->hit = GAME_FN(int (*)(int, int), RVA_BATTLE_UNIT_AT)(x, y);
            if (c->hit < 0) continue;
            int i = findObservedUnit(units, unique, c->hit);
            if (i >= 0) {
                addTestFieldUnitCell(L, rows, i, "occupied", x, y);
            } else {
                complete = 0;
                addTestFieldIssue(L, issues, &issueCount, "unexpected_hit", c->hit, x, y);
            }
        }
    for (int x = 0; x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            int id = cells[x * BATTLE_ROWS + y].unit;
            int i = id >= 0 ? findObservedUnit(units, unique, id) : -1;
            if (i < 0 || !units[i].safeSize) continue;
            TestFieldObservedUnit *u = &units[i];
            if (x + u->width > width || y + u->height > BATTLE_ROWS) {
                addTestFieldIssue(L, issues, &issueCount, "footprint_outside", id, x, y);
                continue;
            }
            if (!safeHit) continue;
            for (int dx = 0; dx < u->width; dx++)
                for (int dy = 0; dy < u->height; dy++)
                    if (cells[(x + dx) * BATTLE_ROWS + y + dy].hit != id) addTestFieldIssue(L, issues, &issueCount, "overlapping_footprint", id, x + dx, y + dy);
        }
    free(units);
    free(cells);
    lua_pushvalue(L, grid);
    lua_setfield(L, state, "grid");
    lua_pushvalue(L, rows);
    lua_setfield(L, state, "units");
    lua_pushvalue(L, issues);
    lua_setfield(L, state, "issues");
    lua_settop(L, state);
    setBoolField(L, "complete", complete);
    return 1;
}

static int lTestFieldState(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldState");
    const char *reason = testFieldReadProblem(checkLuaScript(L));
    if (!reason && !testField.editor) reason = "not_editor_field";
    if (reason) return refuseTestFieldRead(L, "coe5.testFieldState", reason);
    if (!pushTestFieldState(L, -1)) return refuseTestFieldRead(L, "coe5.testFieldState", "out_of_memory");
    return 1;
}

static int lTestFieldUnitState(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldUnitState");
    lua_Integer id = -1;
    const char *reason = testFieldReadProblem(checkLuaScript(L));
    if (!reason && !testField.editor) reason = "not_editor_field";
    if (!reason && (!readTestFieldInteger(L, 1, &id) || id < 0 || id >= UNIT_TABLE_MAX)) reason = "bad_unit";
    if (!reason && !isTestFieldActor((int)id)) reason = "not_on_field";
    if (reason) return refuseTestFieldRead(L, "coe5.testFieldUnitState", reason);
    if (!pushTestFieldState(L, (int)id)) return refuseTestFieldRead(L, "coe5.testFieldUnitState", "out_of_memory");
    int state = lua_gettop(L);
    lua_getfield(L, state, "units");
    int rows = lua_gettop(L);
    for (lua_Integer i = 1, n = (lua_Integer)lua_rawlen(L, rows); i <= n; i++) {
        lua_rawgeti(L, rows, i);
        lua_getfield(L, -1, "unit");
        int match = lua_tointeger(L, -1) == id;
        lua_pop(L, 1);
        if (match) {
            lua_getfield(L, state, "complete");
            lua_setfield(L, -2, "complete");
            lua_getfield(L, state, "issues");
            lua_setfield(L, -2, "issues");
            return 1;
        }
        lua_pop(L, 1);
    }
    return refuseTestFieldRead(L, "coe5.testFieldUnitState", "bad_field");
}
