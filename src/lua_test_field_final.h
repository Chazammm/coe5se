#pragma once

typedef struct {
    int unit, object, flags, ground, hit;
} TestFieldFinalCell;

typedef struct {
    int unit, actor, readable, hp, monster, owner, team, worldX, worldY, width, height, sprite;
    int anchors, occupied, anchorX, anchorY, layer, createdDuringFight, createdMonster, createdOwner;
    unsigned long long generation;
} TestFieldFinalUnit;

typedef struct {
    const char *code;
    int unit, x, y;
} TestFieldFinalIssue;

#define TEST_FIELD_FINAL_ISSUES 256

static struct {
    int armed, finished, captured, complete, captures, width, round, winner, count, issueCount, issueTotal;
    const char *unavailable;
    unsigned long thread;
    unsigned long long session;
    unsigned long long *initial;
    int *map;
    TestFieldFinalUnit *units;
    TestFieldFinalCell *grid;
    TestFieldFinalIssue issues[TEST_FIELD_FINAL_ISSUES];
} testFieldFinal;

static void noteTestFieldFinalIssue(const char *code, int unit, int x, int y) {
    testFieldFinal.complete = 0;
    testFieldFinal.issueTotal++;
    if (testFieldFinal.issueCount < TEST_FIELD_FINAL_ISSUES) testFieldFinal.issues[testFieldFinal.issueCount++] = (TestFieldFinalIssue){code, unit, x, y};
}

static void resetTestFieldFinal(void) {
    free(testFieldFinal.initial);
    free(testFieldFinal.map);
    free(testFieldFinal.units);
    free(testFieldFinal.grid);
    memset(&testFieldFinal, 0, sizeof testFieldFinal);
}

static int testFieldFinalViewActive(void) {
    return testField.open && testFieldFinal.finished && testFieldFinal.session == testFieldLife.session;
}

static const char *testFieldFinalStatus(const TestFieldFinalUnit *u) {
    return !u->readable ? "unverified" : u->hp <= 0 ? "dead" : u->anchors ? "on_field" : "off_field";
}

static int testFieldFinalIndex(int unit) {
    return unit >= 0 && unit < UNIT_TABLE_MAX && testFieldFinal.map ? testFieldFinal.map[unit] : -1;
}

static void captureTestFieldFinal(void) {
    testFieldFinal.captures++;
    if (testFieldFinal.captures != 1) {
        noteTestFieldFinalIssue("multiple_capture_boundaries", -1, -1, -1);
        return;
    }
    testFieldFinal.captured = 1;
    testFieldFinal.complete = 1;
    testFieldFinal.width = G32(RVA_BATTLE_WIDTH);
    testFieldFinal.round = G32(RVA_BATTLE_ROUND);
    testFieldFinal.winner = G32(RVA_BATTLE_WINNER_PLAYER);
    int width = testFieldFinal.width, last = G32(RVA_UNIT_COUNT), safeHit = 1;
    if (width < 1 || width > BATTLE_COLUMNS_MAX || last < -1 || last >= UNIT_TABLE_MAX) {
        noteTestFieldFinalIssue("bad_native_bounds", -1, -1, -1);
        return;
    }
    if (testFieldLife.failed || testFieldLife.pending) noteTestFieldFinalIssue("tracking_unverified", -1, -1, -1);
    testFieldFinal.grid = calloc((size_t)width * BATTLE_ROWS, sizeof *testFieldFinal.grid);
    if (!testFieldFinal.grid) {
        noteTestFieldFinalIssue("out_of_memory", -1, -1, -1);
        return;
    }
    for (int id = 0; id < UNIT_TABLE_MAX; id++) {
        TestFieldMadeUnit *made = &testFieldLife.actors[id];
        if (!made->generation) continue;
        TestFieldFinalUnit *u = &testFieldFinal.units[testFieldFinal.count];
        *u = (TestFieldFinalUnit){.unit = id,
                                  .actor = made->actor,
                                  .generation = made->generation,
                                  .monster = -1,
                                  .owner = -1,
                                  .sprite = -1,
                                  .anchorX = -1,
                                  .anchorY = -1,
                                  .worldX = -1,
                                  .worldY = -1,
                                  .createdMonster = made->created.monster,
                                  .createdOwner = made->created.owner,
                                  .createdDuringFight = testFieldFinal.initial[id] != made->generation};
        testFieldFinal.map[id] = testFieldFinal.count++;
        if (!isTestFieldUnit(id) || id > last) {
            noteTestFieldFinalIssue("identity_unverified", id, -1, -1);
            continue;
        }
        size_t at = (size_t)id * UNIT_SIZE;
        u->readable = 1;
        u->hp = G16(RVA_UNIT_HP + at);
        u->monster = G16(RVA_UNIT_MONSTER + at);
        u->owner = G16(RVA_UNIT_PLAYER + at);
        u->worldX = G32(RVA_UNIT_X + at);
        u->worldY = G32(RVA_UNIT_X + at + 4);
        u->team = u->owner == G32(RVA_BATTLE_PLAYER_A) ? 1 : u->owner == G32(RVA_BATTLE_PLAYER_B) ? 2 : 0;
        if (u->monster < 0 || u->monster >= MONSTER_TABLE_MAX || !isUsedMonster(u->monster) || !isMonsterSpriteValid(u->monster)) {
            u->readable = 0;
            noteTestFieldFinalIssue("bad_monster_or_sprite", id, -1, -1);
            continue;
        }
        u->width = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(id);
        u->height = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(id);
        u->sprite = *(const int *)(monsterRecord(u->monster) + (RVA_MONSTER_SPRITES - RVA_MONSTER_TABLE));
        if (u->width < 1 || u->width > 3 || u->height < 1 || u->height > 3) {
            u->readable = 0;
            noteTestFieldFinalIssue("bad_footprint", id, -1, -1);
        }
    }
    for (int x = 0; x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            TestFieldFinalCell *c = &testFieldFinal.grid[x * BATTLE_ROWS + y];
            const unsigned char *at = battleCell(x, y);
            short ground;
            memcpy(&c->unit, at, 4);
            memcpy(&c->object, at + BATTLE_CELL_OBJECT, 4);
            memcpy(&c->flags, at + BATTLE_CELL_FLAGS, 4);
            memcpy(&ground, at + BATTLE_CELL_GROUND, 2);
            c->ground = ground;
            c->hit = -1;
            int ids[2] = {c->unit, c->object};
            for (int layer = 0; layer < 2; layer++) {
                int id = ids[layer];
                if (id == -1) continue;
                int index = testFieldFinalIndex(id);
                if (index < 0 || !testFieldFinal.units[index].readable) {
                    safeHit = 0;
                    noteTestFieldFinalIssue("unverified_grid_identity", id, x, y);
                    continue;
                }
                TestFieldFinalUnit *u = &testFieldFinal.units[index];
                u->anchors++;
                u->anchorX = x;
                u->anchorY = y;
                u->layer = layer;
            }
        }
    if (!safeHit) noteTestFieldFinalIssue("hit_test_unavailable", -1, -1, -1);
    for (int x = 0; safeHit && x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            TestFieldFinalCell *c = &testFieldFinal.grid[x * BATTLE_ROWS + y];
            c->hit = GAME_FN(int (*)(int, int), RVA_BATTLE_UNIT_AT)(x, y);
            if (c->hit < 0) continue;
            int i = testFieldFinalIndex(c->hit);
            if (i < 0) noteTestFieldFinalIssue("unexpected_hit", c->hit, x, y);
            else testFieldFinal.units[i].occupied++;
        }
    for (int i = 0; i < testFieldFinal.count; i++) {
        TestFieldFinalUnit *u = &testFieldFinal.units[i];
        if (!u->readable || u->hp <= 0 || !u->anchors) continue;
        if (u->anchors != 1) {
            noteTestFieldFinalIssue("duplicate_anchor", u->unit, u->anchorX, u->anchorY);
            continue;
        }
        if (u->anchorX + u->width > width || u->anchorY + u->height > BATTLE_ROWS) {
            noteTestFieldFinalIssue("footprint_outside", u->unit, u->anchorX, u->anchorY);
            continue;
        }
        if (u->actor && u->layer == 0 && safeHit) {
            if (u->occupied != u->width * u->height) noteTestFieldFinalIssue("footprint_hit_count", u->unit, u->anchorX, u->anchorY);
            for (int dx = 0; dx < u->width; dx++)
                for (int dy = 0; dy < u->height; dy++)
                    if (testFieldFinal.grid[(u->anchorX + dx) * BATTLE_ROWS + u->anchorY + dy].hit != u->unit)
                        noteTestFieldFinalIssue("footprint_hit_mismatch", u->unit, u->anchorX + dx, u->anchorY + dy);
        } else if (u->actor && u->layer != 0) noteTestFieldFinalIssue("actor_on_object_layer", u->unit, u->anchorX, u->anchorY);
    }
    writeLog("test_field_final_captured",
             "session=%llu source=post_aftermath_before_upkeep units=%d cells=%d round=%d winner=%d complete=%d scene_complete=0 issues=%d",
             testFieldFinal.session, testFieldFinal.count, width * BATTLE_ROWS, testFieldFinal.round, testFieldFinal.winner, testFieldFinal.complete,
             testFieldFinal.issueTotal);
}

static int testFieldPreeofCrcHook(int x, int y) {
    const char *mod = runningLuaMod();
    if (testFieldFinal.armed && testField.open && testField.fighting && testFieldLife.active && testFieldLife.depth > 0 &&
        testFieldFinal.session == testFieldLife.session && GetCurrentThreadId() == testFieldFinal.thread && x == testField.x && y == testField.y &&
        (!mod || !_stricmp(mod, testField.owner)))
        captureTestFieldFinal();
    return GAME_FN(int (*)(int, int), RVA_BATTLE_CRC)(x, y);
}

static TestFieldCall testFieldPreeofCall = {RVA_BATTLE_PREEOF_CRC_CALL, RVA_BATTLE_CRC, (void *)testFieldPreeofCrcHook, "battle_preeof_crc", NULL, 0};

static void turnOffTestFieldFinal(void) {
    restoreTestFieldCall(&testFieldPreeofCall);
}

static void beginTestFieldFinal(void) {
    resetTestFieldFinal();
    testFieldFinal.session = testFieldLife.session;
    if (!gameCodeMatches || !pointTestFieldCall(&testFieldPreeofCall)) {
        testFieldFinal.unavailable = "capture_hook_unavailable";
        return;
    }
    testFieldFinal.initial = calloc(UNIT_TABLE_MAX, sizeof *testFieldFinal.initial);
    testFieldFinal.map = malloc(UNIT_TABLE_MAX * sizeof *testFieldFinal.map);
    testFieldFinal.units = calloc(UNIT_TABLE_MAX, sizeof *testFieldFinal.units);
    if (!testFieldFinal.initial || !testFieldFinal.map || !testFieldFinal.units) {
        testFieldFinal.unavailable = "out_of_memory";
        return;
    }
    for (int id = 0; id < UNIT_TABLE_MAX; id++) {
        testFieldFinal.map[id] = -1;
        if (isTestFieldUnit(id)) testFieldFinal.initial[id] = testFieldLife.actors[id].generation;
    }
    testFieldFinal.thread = GetCurrentThreadId();
    testFieldFinal.armed = 1;
}

static void finishTestFieldFinal(void) {
    testFieldFinal.armed = 0;
    testFieldFinal.finished = 1;
    if (!testFieldFinal.captured) noteTestFieldFinalIssue(testFieldFinal.unavailable ? testFieldFinal.unavailable : "capture_boundary_not_reached", -1, -1, -1);
}

static const char *testFieldFinalReadProblem(const LuaScript *script) {
    if (!gameCodeMatches) return "game_code_differs";
    if (!testField.open) return "not_open";
    if (_stricmp(script->mod, testField.owner)) return "not_owner";
    if (testField.fighting) return "in_fight";
    if (!testFieldFinalViewActive()) return "no_final_state";
    return NULL;
}

static const char *testFieldFinalInspectProblem(const LuaScript *script, lua_Integer id) {
    const char *reason = testFieldFinalReadProblem(script);
    if (reason) return reason;
    if (id < 0 || id >= UNIT_TABLE_MAX) return "bad_unit";
    int i = testFieldFinalIndex((int)id);
    if (i < 0) return "not_in_final_state";
    TestFieldFinalUnit *u = &testFieldFinal.units[i];
    if (!u->readable || !u->actor || u->hp <= 0 || u->anchors != 1) return "not_a_visible_survivor";
    if (!isTestFieldActor((int)id) || testFieldLife.generations[id] != u->generation) return "identity_changed";
    if (id > G32(RVA_UNIT_COUNT) || G16(RVA_UNIT_HP + (size_t)id * UNIT_SIZE) <= 0) return "removed_after_capture";
    return testFieldCasterReadProblem((int)id);
}

static int lTestFieldFinalState(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldFinalState");
    LuaScript *script = checkLuaScript(L);
    const char *reason = testFieldFinalReadProblem(script);
    if (reason) return refuseTestFieldRead(L, "coe5.testFieldFinalState", reason);
    lua_createtable(L, 0, 14);
    int state = lua_gettop(L);
    lua_pushliteral(L, "post_aftermath_before_upkeep");
    lua_setfield(L, state, "source");
    lua_pushstring(L, testFieldFinal.captured ? "captured" : "unavailable");
    lua_setfield(L, state, "phase");
    setBoolField(L, "complete", testFieldFinal.complete && testFieldFinal.captured);
    setBoolField(L, "sceneComplete", 0);
    setBoolField(L, "captured", testFieldFinal.captured);
    lua_pushliteral(L, "current owned native generations and every battlefield cell; not transition history");
    lua_setfield(L, state, "coverage");
    setIntField(L, "session", (lua_Integer)testFieldFinal.session);
    setIntField(L, "width", testFieldFinal.width);
    setIntField(L, "height", BATTLE_ROWS);
    setIntField(L, "round", testFieldFinal.round);
    setIntField(L, "winner", testFieldFinal.winner);
    setIntField(L, "issueCount", testFieldFinal.issueTotal);
    setBoolField(L, "issuesTruncated", testFieldFinal.issueTotal > testFieldFinal.issueCount);
    lua_createtable(L, testFieldFinal.count, 0);
    int rows = lua_gettop(L);
    for (int i = 0; i < testFieldFinal.count; i++) {
        TestFieldFinalUnit *u = &testFieldFinal.units[i];
        size_t at = (size_t)u->unit * UNIT_SIZE;
        int identity = isTestFieldUnit(u->unit) && testFieldLife.generations[u->unit] == u->generation;
        int hp = identity ? G16(RVA_UNIT_HP + at) : -1;
        const char *inspect = testFieldFinalInspectProblem(script, u->unit);
        lua_createtable(L, 0, 32);
        setIntField(L, "unit", u->unit);
        setIntField(L, "generation", (lua_Integer)u->generation);
        setIntField(L, "hp", u->hp);
        setBoolField(L, "alive", u->readable && u->hp > 0);
        setIntField(L, "monster", u->monster);
        setBoolField(L, "readable", u->readable);
        setIntField(L, "owner", u->owner);
        setIntField(L, "team", u->team);
        setIntField(L, "sprite", u->sprite);
        setIntField(L, "worldX", u->worldX);
        setIntField(L, "worldY", u->worldY);
        setIntField(L, "width", u->width);
        setIntField(L, "height", u->height);
        setIntField(L, "anchorCount", u->anchors);
        setIntField(L, "createdMonster", u->createdMonster);
        setIntField(L, "createdOwner", u->createdOwner);
        setBoolField(L, "createdDuringFight", u->createdDuringFight);
        setBoolField(L, "ownerChanged", u->owner != u->createdOwner);
        setBoolField(L, "monsterChanged", u->monster != u->createdMonster);
        setBoolField(L, "identityMatches", identity);
        setBoolField(L, "currentAlive", identity && hp > 0);
        setIntField(L, "currentHP", hp);
        setIntField(L, "currentMonster", identity ? G16(RVA_UNIT_MONSTER + at) : -1);
        setIntField(L, "currentOwner", identity ? G16(RVA_UNIT_PLAYER + at) : -1);
        setBoolField(L, "inspectable", !inspect);
        if (inspect) {
            lua_pushstring(L, inspect);
            lua_setfield(L, -2, "inspectReason");
        }
        lua_pushstring(L, u->actor ? "actor" : "terrain");
        lua_setfield(L, -2, "kind");
        lua_pushstring(L, testFieldFinalStatus(u));
        lua_setfield(L, -2, "status");
        setBoolField(L, "nativeFleeMarker", u->readable && u->hp > 0 && !u->anchors && u->worldX == -2);
        lua_pushliteral(L, "not_classified");
        lua_setfield(L, -2, "routeStatus");
        lua_pushstring(L, u->layer ? "object" : "unit");
        lua_setfield(L, -2, "anchorLayer");
        lua_pushstring(L, u->actor && !u->layer ? "native_unit_hit_test" : "object_layer_has_no_unit_hit_footprint");
        lua_setfield(L, -2, "occupiedSource");
        lua_pushliteral(L, "native_new_unit_owned_scope");
        lua_setfield(L, -2, "provenance");
        lua_newtable(L);
        lua_setfield(L, -2, "anchors");
        lua_newtable(L);
        lua_setfield(L, -2, "occupied");
        lua_rawseti(L, rows, i + 1);
    }
    lua_createtable(L, testFieldFinal.grid ? testFieldFinal.width * BATTLE_ROWS : 0, 0);
    int grid = lua_gettop(L);
    for (int x = 0; testFieldFinal.grid && x < testFieldFinal.width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            TestFieldFinalCell *c = &testFieldFinal.grid[x * BATTLE_ROWS + y];
            lua_createtable(L, 0, 7);
            setIntField(L, "x", x);
            setIntField(L, "y", y);
            setIntField(L, "unit", c->unit);
            setIntField(L, "object", c->object);
            setIntField(L, "flags", c->flags);
            setIntField(L, "ground", c->ground);
            setIntField(L, "hit", c->hit);
            lua_rawseti(L, grid, x * BATTLE_ROWS + y + 1);
            int ids[2] = {c->unit, c->object};
            for (int layer = 0; layer < 2; layer++) {
                int i = testFieldFinalIndex(ids[layer]);
                if (i >= 0) addTestFieldUnitCell(L, rows, i, "anchors", x, y);
            }
            int i = testFieldFinalIndex(c->hit);
            if (i >= 0) addTestFieldUnitCell(L, rows, i, "occupied", x, y);
        }
    lua_pushvalue(L, rows);
    lua_setfield(L, state, "units");
    lua_pushvalue(L, grid);
    lua_setfield(L, state, "grid");
    lua_createtable(L, testFieldFinal.issueCount, 0);
    for (int i = 0; i < testFieldFinal.issueCount; i++) {
        TestFieldFinalIssue *issue = &testFieldFinal.issues[i];
        lua_createtable(L, 0, 4);
        lua_pushstring(L, issue->code);
        lua_setfield(L, -2, "code");
        setIntField(L, "unit", issue->unit);
        setIntField(L, "x", issue->x);
        setIntField(L, "y", issue->y);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, state, "issues");
    lua_settop(L, state);
    return 1;
}
