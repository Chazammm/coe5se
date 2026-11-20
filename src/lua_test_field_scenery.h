#pragma once

// The exact stock templates verified for this Windows object's constructor.
typedef struct { int monster; const char *name; int standable; } TestFieldSceneryType;
static const TestFieldSceneryType testFieldSceneryTypes[] = {
    {1394, "Stone Parapet (Medium Cover)", 1}, {1395, "Stone Parapet (Heavy Cover)", 1},
    {1404, "Stone Wall", 0}, {1446, "Palisade Parapet", 1}, {1448, "Palisade Wall", 0},
    {1462, "Wooden Stairs", 1}, {1511, "Stone Stairs", 1}, {1634, "Cabbage", 0},
};
static int testFieldSceneryRequest = -1;
static const char *testFieldSceneryRequestName;

// Resolves an exact identity at the terrain constructor's sole lookup site.
static int testFieldSceneryLookup(const char *name, int mode) {
    if (testFieldSceneryRequest >= 0 && name == testFieldSceneryRequestName && mode == 1 &&
        testFieldLife.active && GetCurrentThreadId() == testFieldLife.thread) return testFieldSceneryRequest;
    return GAME_FN(int (*)(const char *, int), RVA_FIND_MONSTER)(name, mode);
}

// The Windows constructor's first call resolves the requested template.
static TestFieldCall testFieldSceneryLookupCall = {
    RVA_BATTLE_PLACE_TERRAIN_UNIT + 0x21, RVA_FIND_MONSTER, (void *)testFieldSceneryLookup,
    "owned_scenery_exact_lookup", NULL, 0,
};

// Returns the verified constructor argument for this loaded definition.
static const TestFieldSceneryType *testFieldSceneryType(int id) {
    for (size_t i = 0; i < sizeof testFieldSceneryTypes / sizeof *testFieldSceneryTypes; i++) {
        if (testFieldSceneryTypes[i].monster != id) continue;
        const char *name = testFieldSceneryTypes[i].name;
        if (!isUsedMonster(id) || !isMonsterSpriteValid(id) || monsterMadeByName(id) ||
            strncmp((const char *)monsterRecord(id), name, MONSTER_NAME_SIZE) ||
            monsterTokenWidth(id) != 1 || monsterTokenHeight(id) != 1) return NULL;
        return &testFieldSceneryTypes[i];
    }
    return NULL;
}

// Checks ownership and preparation boundaries before mutable scenery operations.
static const char *testFieldSceneryProblem(lua_State *L, const char *call, int edit) {
    checkLuaMainThread(L, call);
    const char *reason = testFieldReadProblem(checkLuaScript(L));
    if (reason) return reason;
    if (!TEST_FIELD_VERIFIED_WINDOWS_ABI) return "unsupported_compiler_abi";
    if (!testField.editor || !testField.emptyEditor || !testFieldLife.active) return "not_editor_field";
    if (testField.fighting || testFieldFinalViewActive()) return "not_editor_preparation";
    if (testFieldLife.pending || testFieldLife.failed || testFieldLife.retiring) return "cleanup_unverified";
    if (edit && (testFieldDeploymentBusy || testFieldLife.depth || battleDrawing || luaSiteDepth[SITE_BATTLE_DRAW]))
        return "field_transaction_active";
    if (testFieldTerrainPolicyWater(findTestFieldTerrainPolicy(testField.terrain))) return "water_scenery_unverified";
    return testFieldEnvironmentProblem();
}

// Returns the stock identities available to an editor for this loaded game.
static int lTestFieldSceneryTypes(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldSceneryTypes");
    lua_createtable(L, 0, 2);
    int version = TEST_FIELD_VERIFIED_WINDOWS_ABI && gameCodeMatches && testFieldProvenanceCallsValid();
    setIntField(L, "version", version);
    lua_newtable(L);
    int count = 0;
    for (size_t i = 0; version && i < sizeof testFieldSceneryTypes / sizeof *testFieldSceneryTypes; i++) {
        int id = testFieldSceneryTypes[i].monster;
        const TestFieldSceneryType *type = testFieldSceneryType(id);
        if (!type) continue;
        lua_createtable(L, 0, 6);
        setIntField(L, "monster", id);
        setIntField(L, "owner", 31);
        setIntField(L, "width", 1);
        setIntField(L, "height", 1);
        setBoolField(L, "standable", type->standable);
        lua_pushliteral(L, "object");
        lua_setfield(L, -2, "layer");
        lua_rawseti(L, -2, ++count);
    }
    lua_setfield(L, -2, "types");
    return 1;
}

// Finds an unoccupied one-cell target on either battlefield layer.
static const char *testFieldSceneryTarget(lua_State *L, int arg, int *x, int *y, int *occupant) {
    lua_Integer cx, cy;
    int width = G32(RVA_BATTLE_WIDTH);
    if (width < 1 || width > BATTLE_COLUMNS_MAX ||
        !readTestFieldInteger(L, arg, &cx) || !readTestFieldInteger(L, arg + 1, &cy) ||
        cx < 0 || cx >= width || cy < 0 || cy >= BATTLE_ROWS) return "off_field";
    *x = (int)cx;
    *y = (int)cy;
    if (testFieldTokenCovering(*x, *y, BATTLE_CELL_OBJECT, -1, 1) >= 0) return "blocked";
    int actor = testFieldTokenCovering(*x, *y, 0, -1, 1);
    if (actor >= 0 && (!occupant || !isTestFieldActor(actor))) return "blocked";
    if (occupant) *occupant = actor;
    return NULL;
}

// Confirms one owned generation and exactly one object-layer anchor.
static int testFieldSceneryIdentity(int unit, unsigned long long generation, int x, int y) {
    if (!isTestFieldUnit(unit) || testFieldLife.actors[unit].generation != generation) return 0;
    const TestFieldMadeUnit *made = &testFieldLife.actors[unit];
    TestFieldUnitSnapshot row = readTestFieldUnitSnapshot(unit);
    int width = G32(RVA_BATTLE_WIDTH);
    if (!row.valid || row.owner != 31 || row.hp <= 0 || row.monster != made->created.monster ||
        row.x != testField.x || row.y != testField.y ||
        width < 1 || width > BATTLE_COLUMNS_MAX || x < 0 || x >= width || y < 0 || y >= BATTLE_ROWS ||
        GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(unit) != 1 ||
        GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(unit) != 1) return 0;
    if (*(int *)(battleCell(x, y) + BATTLE_CELL_OBJECT) != unit) return 0;
    for (int bx = 0; bx < width; bx++)
        for (int by = 0; by < BATTLE_ROWS; by++) {
            const unsigned char *cell = battleCell(bx, by);
            if (*(const int *)cell == unit || ((bx != x || by != y) && *(const int *)(cell + BATTLE_CELL_OBJECT) == unit))
                return 0;
        }
    return 1;
}

// Returns all authored scenery rows with their session and generation.
static int lTestFieldSceneryState(lua_State *L) {
    const char *call = "coe5.testFieldSceneryState";
    const char *reason = testFieldSceneryProblem(L, call, 0);
    if (reason) return refuseTestFieldRead(L, call, reason);
    lua_createtable(L, 0, 3);
    setIntField(L, "version", 1);
    setIntField(L, "session", (lua_Integer)testFieldLife.session);
    lua_newtable(L);
    int count = 0;
    for (int unit = 0; unit < UNIT_TABLE_MAX; unit++) {
        const TestFieldMadeUnit *made = &testFieldLife.actors[unit];
        if (made->source != TEST_FIELD_OWNED_SCENERY) continue;
        if (!testFieldSceneryIdentity(unit, made->generation, made->sceneryX, made->sceneryY))
            return refuseTestFieldRead(L, call, "scenery_identity_unverified");
        TestFieldUnitSnapshot row = readTestFieldUnitSnapshot(unit);
        lua_createtable(L, 0, 13);
        setIntField(L, "unit", unit);
        setIntField(L, "generation", (lua_Integer)made->generation);
        setIntField(L, "monster", row.monster);
        setIntField(L, "owner", row.owner);
        setIntField(L, "hp", row.hp);
        setIntField(L, "x", made->sceneryX);
        setIntField(L, "y", made->sceneryY);
        setIntField(L, "width", 1);
        setIntField(L, "height", 1);
        setBoolField(L, "standable", made->sceneryStandable);
        lua_pushliteral(L, "object");
        lua_setfield(L, -2, "anchorLayer");
        lua_pushliteral(L, "owned_scenery_placement");
        lua_setfield(L, -2, "creationSource");
        lua_rawseti(L, -2, ++count);
    }
    lua_setfield(L, -2, "objects");
    return 1;
}

// Calls the native terrain constructor and publishes only a verified exact object.
static int lAddTestFieldScenery(lua_State *L) {
    const char *call = "coe5.addTestFieldScenery";
    const char *reason = testFieldSceneryProblem(L, call, 1);
    lua_Integer monster = -1;
    const TestFieldSceneryType *type = NULL;
    int x = -1, y = -1, occupant = -1, actorX = -1, actorY = -1;
    if (!reason && (!readTestFieldInteger(L, 1, &monster) || monster < 0 || monster >= MONSTER_TABLE_MAX ||
        !(type = testFieldSceneryType((int)monster)))) reason = "unsupported_scenery";
    if (!reason) reason = testFieldSceneryTarget(L, 2, &x, &y, type->standable ? &occupant : NULL);
    if (!reason && occupant >= 0 && !unitBattleCell(occupant, &actorX, &actorY)) reason = "unverified_occupant";
    if (!reason && !pointTestFieldCall(&testFieldSceneryLookupCall)) reason = "scenery_lookup_unavailable";
    if (reason) return refuseTestFieldRead(L, call, reason);

    unsigned char *cell = battleCell(x, y);
    unsigned char old[BATTLE_CELL_SIZE], expected[BATTLE_CELL_SIZE];
    unsigned char actorBody[UNIT_SIZE];
    const unsigned char *actorRecord = occupant >= 0 ? GAME_PTR(RVA_UNITS + (size_t)occupant * UNIT_SIZE) : NULL;
    unsigned char *actorAnchor = occupant >= 0 ? battleCell(actorX, actorY) : NULL;
    unsigned long long actorGeneration = occupant >= 0 ? testFieldLife.generations[occupant] : 0;
    if (actorRecord) memcpy(actorBody, actorRecord, sizeof actorBody);
    memcpy(old, cell, sizeof old);
    const unsigned long long serial = testFieldLife.serial;
    if (actorAnchor && *(int *)actorAnchor != occupant) return refuseTestFieldRead(L, call, "unverified_occupant");
    enterTestFieldCode();
    testFieldSceneryRequest = (int)monster;
    testFieldSceneryRequestName = type->name;
    // The terrain constructor expects a unit-free target. Stage the one owned
    // anchor while it runs and restore it before checking native passability.
    if (actorAnchor) *(int *)actorAnchor = -1;
    int unit = GAME_FN(int (*)(const char *, int, int, int, unsigned long long, int, int),
                       RVA_BATTLE_PLACE_TERRAIN_UNIT)(type->name, x, y, -1, 0, 0, 0);
    testFieldSceneryRequest = -1;
    testFieldSceneryRequestName = NULL;
    leaveTestFieldCode();
    if (actorAnchor && *(int *)actorAnchor == -1) *(int *)actorAnchor = occupant;
    if (actorRecord && (!isTestFieldActor(occupant) || actorGeneration != testFieldLife.generations[occupant] ||
        *(int *)actorAnchor != occupant || memcmp(actorBody, actorRecord, sizeof actorBody))) {
        testFieldLife.pending = testFieldLife.failed = 1;
        return refuseTestFieldRead(L, call, "scenery_occupant_changed");
    }
    if (unit < 0 && serial == testFieldLife.serial && !memcmp(old, cell, sizeof old) && !testFieldLife.failed)
        return refuseTestFieldRead(L, call, "native_scenery_refused");
    if (unit < 0 && testFieldLife.serial == serial + 1) {
        for (int id = 0; id < UNIT_TABLE_MAX; id++)
            if (isTestFieldUnit(id) && testFieldLife.actors[id].generation == serial + 1) {
                unit = id;
                break;
            }
    }
    if (testFieldLife.failed || testFieldLife.serial != serial + 1 || !isTestFieldUnit(unit) ||
        testFieldLife.actors[unit].generation != serial + 1 || testFieldLife.actors[unit].source != TEST_FIELD_SCATTER) {
        testFieldLife.pending = testFieldLife.failed = 1;
        return refuseTestFieldRead(L, call, "scenery_creation_unverified");
    }
    TestFieldMadeUnit *made = &testFieldLife.actors[unit];
    memcpy(expected, old, sizeof expected);
    memcpy(expected + BATTLE_CELL_OBJECT, &unit, sizeof unit);
    memset(expected + BATTLE_CELL_OBJECT_SLIDE, 0, sizeof(int));
    int (*canStand)(int, int, int, int) = GAME_FN(int (*)(int, int, int, int), RVA_BATTLE_CAN_STAND);
    if (made->actor || made->created.monster != monster || memcmp(expected, cell, sizeof expected) ||
        !testFieldSceneryIdentity(unit, made->generation, x, y) ||
        (occupant >= 0 && (!canStand(occupant, actorX, actorY, 0) || !canStand(occupant, actorX, actorY, 4)))) {
        if (retireTestFieldUnit(unit)) {
            memcpy(cell, old, sizeof old);
            return refuseTestFieldRead(L, call, "native_scenery_rejected");
        }
        testFieldLife.pending = testFieldLife.failed = 1;
        return refuseTestFieldRead(L, call, "cleanup_unverified");
    }
    made->source = TEST_FIELD_OWNED_SCENERY;
    made->sceneryX = x;
    made->sceneryY = y;
    made->sceneryStandable = type->standable;
    memcpy(&made->scenerySlide, old + BATTLE_CELL_OBJECT_SLIDE, sizeof made->scenerySlide);
    testFieldInitialDeploymentReset();
    lua_pushinteger(L, unit);
    return 1;
}

// Resolves a mutation handle only when its session and generation still match.
static TestFieldMadeUnit *testFieldSceneryHandle(lua_State *L, int *unit) {
    lua_Integer id, session, generation;
    if (!readTestFieldInteger(L, 1, &id) || id < 0 || id >= UNIT_TABLE_MAX ||
        !readTestFieldInteger(L, 2, &session) || session < 1 || (unsigned long long)session != testFieldLife.session ||
        !readTestFieldInteger(L, 3, &generation) || generation < 1 || !isTestFieldUnit((int)id)) return NULL;
    TestFieldMadeUnit *made = &testFieldLife.actors[id];
    if (made->source != TEST_FIELD_OWNED_SCENERY ||
        !testFieldSceneryIdentity((int)id, (unsigned long long)generation, made->sceneryX, made->sceneryY)) return NULL;
    *unit = (int)id;
    return made;
}

// Moves a scenery anchor without recreating the unit or changing its health.
static int lMoveTestFieldScenery(lua_State *L) {
    const char *call = "coe5.moveTestFieldScenery";
    const char *reason = testFieldSceneryProblem(L, call, 1);
    int unit, x, y;
    TestFieldMadeUnit *made = reason ? NULL : testFieldSceneryHandle(L, &unit);
    if (!reason && !made) reason = "stale_scenery";
    if (!reason && testFieldTokenCovering(made->sceneryX, made->sceneryY, 0, -1, 1) >= 0) reason = "scenery_occupied";
    if (!reason) reason = testFieldSceneryTarget(L, 4, &x, &y, NULL);
    if (reason) return refuseTestFieldRead(L, call, reason);
    unsigned char *source = battleCell(made->sceneryX, made->sceneryY), *dest = battleCell(x, y);
    unsigned char beforeFrom[BATTLE_CELL_SIZE], beforeTo[BATTLE_CELL_SIZE];
    memcpy(beforeFrom, source, sizeof beforeFrom);
    memcpy(beforeTo, dest, sizeof beforeTo);
    *(int *)(source + BATTLE_CELL_OBJECT) = -1;
    memcpy(source + BATTLE_CELL_OBJECT_SLIDE, &made->scenerySlide, sizeof made->scenerySlide);
    *(int *)(dest + BATTLE_CELL_OBJECT) = unit;
    memset(dest + BATTLE_CELL_OBJECT_SLIDE, 0, sizeof(int));
    if (!testFieldSceneryIdentity(unit, made->generation, x, y)) {
        memcpy(source, beforeFrom, sizeof beforeFrom);
        memcpy(dest, beforeTo, sizeof beforeTo);
        if (!testFieldSceneryIdentity(unit, made->generation, made->sceneryX, made->sceneryY)) {
            testFieldLife.pending = testFieldLife.failed = 1;
            return refuseTestFieldRead(L, call, "cleanup_unverified");
        }
        return refuseTestFieldRead(L, call, "native_scenery_rejected");
    }
    made->sceneryX = x;
    made->sceneryY = y;
    memcpy(&made->scenerySlide, beforeTo + BATTLE_CELL_OBJECT_SLIDE, sizeof made->scenerySlide);
    testFieldInitialDeploymentReset();
    lua_pushboolean(L, 1);
    return 1;
}

// Retires an authored object and restores the cell underneath its anchor.
static int lRemoveTestFieldScenery(lua_State *L) {
    const char *call = "coe5.removeTestFieldScenery";
    const char *reason = testFieldSceneryProblem(L, call, 1);
    int unit;
    TestFieldMadeUnit *made = reason ? NULL : testFieldSceneryHandle(L, &unit);
    if (!reason && !made) reason = "stale_scenery";
    if (!reason && testFieldTokenCovering(made->sceneryX, made->sceneryY, 0, -1, 1) >= 0) reason = "scenery_occupied";
    if (reason) return refuseTestFieldRead(L, call, reason);
    unsigned char *cell = battleCell(made->sceneryX, made->sceneryY);
    unsigned char underlying[BATTLE_CELL_SIZE];
    memcpy(underlying, cell, sizeof underlying);
    memset(underlying + BATTLE_CELL_OBJECT, 0xff, sizeof(int));
    memcpy(underlying + BATTLE_CELL_OBJECT_SLIDE, &made->scenerySlide, sizeof made->scenerySlide);
    if (!retireTestFieldUnit(unit)) {
        testFieldLife.pending = testFieldLife.failed = 1;
        return refuseTestFieldRead(L, call, "cleanup_unverified");
    }
    memcpy(cell, underlying, sizeof underlying);
    made->cleanup = 1;
    made->source = TEST_FIELD_SCATTER;
    testFieldInitialDeploymentReset();
    lua_pushboolean(L, 1);
    return 1;
}

// Opens the native battlefield right-click card for a verified live feature.
static int lInspectTestFieldFeature(lua_State *L) {
    const char *call = "coe5.inspectTestFieldFeature";
    checkLuaMainThread(L, call);
    LuaScript *script = checkLuaScript(L);
    int final = lua_toboolean(L, 3);
    const char *reason = final ? testFieldFinalReadProblem(script) : testFieldReadProblem(script);
    if (!reason && (!TEST_FIELD_VERIFIED_WINDOWS_ABI || !testField.editor || !testFieldLife.active ||
        testFieldLife.failed || testFieldLife.pending || testField.fighting ||
        (!final && testFieldFinalViewActive()))) reason = "feature_inspect_unavailable";
    if (!reason && (testFieldInspectActive || battleDrawing || !testFieldTextPollReady ||
        (battleWaiting <= 0 && luaSiteDepth[SITE_BATTLE_WAIT] <= 0 && testFieldDrawSession != testFieldLife.session)))
        reason = "feature_card_not_ready";
    lua_Integer x = -1, y = -1;
    int width = G32(RVA_BATTLE_WIDTH);
    if (!reason && (width < 1 || width > BATTLE_COLUMNS_MAX ||
        !readTestFieldInteger(L, 1, &x) || !readTestFieldInteger(L, 2, &y) ||
        x < 0 || x >= width || y < 0 || y >= BATTLE_ROWS)) reason = "off_field";

    int unit = -1, objectLayer = 0;
    const TestFieldFinalUnit *captured = NULL;
    if (!reason && final) {
        if (!testFieldFinal.captured || !testFieldFinal.grid || testFieldFinal.width != width)
            reason = "final_feature_unavailable";
        else {
            const TestFieldFinalCell *cell = &testFieldFinal.grid[x * BATTLE_ROWS + y];
            unit = cell->hit;
            if (unit < 0) { unit = cell->object; objectLayer = 1; }
            int index = testFieldFinalIndex(unit);
            if (index >= 0) captured = &testFieldFinal.units[index];
            if (!captured || !captured->readable || captured->hp <= 0 || captured->anchors != 1 ||
                captured->layer != objectLayer) reason = "final_feature_unavailable";
        }
    } else if (!reason) {
        unit = GAME_FN(int (*)(int, int), RVA_BATTLE_UNIT_AT)((int)x, (int)y);
        if (unit < 0) {
            unit = GAME_FN(int (*)(int, int, int), RVA_BATTLE_UNIT_AT_2)((int)x, (int)y, 0);
            objectLayer = 1;
        }
    }
    if (!reason && !isTestFieldUnit(unit)) reason = "feature_identity_unverified";
    TestFieldUnitSnapshot row = {0};
    if (!reason) {
        const TestFieldMadeUnit *made = &testFieldLife.actors[unit];
        row = readTestFieldUnitSnapshot(unit);
        int scenery = made->source == TEST_FIELD_OWNED_SCENERY || made->source == TEST_FIELD_MAP_OBJECT ||
                      made->source == TEST_FIELD_SCATTER;
        if (!row.valid || row.hp <= 0 || !isUsedMonster(row.monster) || !isMonsterSpriteValid(row.monster) ||
            row.monster != made->created.monster || row.owner != made->created.owner ||
            (!objectLayer && (made->source != TEST_FIELD_GATE || (row.owner != 0 && row.owner != TEST_BATTLE_ENEMY_PLAYER))) ||
            (objectLayer && (!scenery || row.owner != 31))) reason = "not_a_verified_scenery_or_structure";
        if (captured && (captured->generation != made->generation || row.hp != captured->hp ||
            row.monster != captured->monster || row.owner != captured->owner ||
            row.x != captured->worldX || row.y != captured->worldY)) reason = "final_feature_changed_or_removed";
    }
    SdlKeyboardStateFn keyboard = sdlKeyboardState();
    if (!reason && !keyboard) reason = "text_input_unavailable";
    if (reason) return refuseTestFieldRead(L, call, reason);

    unsigned long long session = testFieldLife.session, generation = testFieldLife.generations[unit];
    size_t at = (size_t)unit * UNIT_SIZE;
    unsigned char actor[UNIT_SIZE];
    size_t boardBytes = (size_t)width * BATTLE_ROWS * BATTLE_CELL_SIZE;
    unsigned char *board = malloc(boardBytes);
    if (!board) return refuseTestFieldRead(L, call, "out_of_memory");
    memcpy(actor, GAME_PTR(RVA_UNITS + at), sizeof actor);
    memcpy(board, battleCell(0, 0), boardBytes);
    resetTestFieldText();
    clearTestFieldInspectInput(keyboard);
    testFieldInspectActive = 1;
    luaCodeRunning--;
    GAME_FN(unsigned long long (*)(unsigned, int, int, int, int), RVA_UNIT_STATS_SCREEN)((unsigned)unit, -1, 0, 0, -1);
    luaCodeRunning++;
    testFieldInspectActive = 0;
    clearTestFieldInspectInput(keyboard);
    resetLuaSteps(L);
    int unchanged = session == testFieldLife.session && isTestFieldUnit(unit) &&
        generation == testFieldLife.generations[unit] && width == G32(RVA_BATTLE_WIDTH) &&
        !memcmp(actor, GAME_PTR(RVA_UNITS + at), sizeof actor) && !memcmp(board, battleCell(0, 0), boardBytes);
    free(board);
    writeLog("test_field_feature_inspected", "unit=%d generation=%llu object_layer=%d final=%d verified=%d",
        unit, generation, objectLayer, final, unchanged);
    if (!unchanged) {
        testFieldLife.pending = testFieldLife.failed = 1;
        return refuseTestFieldRead(L, call, "feature_inspect_changed_unverified");
    }
    lua_pushboolean(L, 1);
    return 1;
}
