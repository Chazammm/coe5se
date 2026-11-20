#pragma once

typedef enum { SCENERY_OTHER, SCENERY_WALL, SCENERY_PARAPET, SCENERY_STAIRS } TestFieldSceneryRole;
typedef struct {
    int first, last;
    const char *name;
    TestFieldSceneryRole role;
    unsigned long long flags;
    int cover, rangeBonus;
} TestFieldSceneryType;

#define SCENERY_STONE_WALL_FLAGS     0x0c002800030401c7ULL
#define SCENERY_WOOD_WALL_FLAGS      0x0c002800028001c6ULL
#define SCENERY_METAL_WALL_FLAGS     0x0c002800038481c7ULL
#define SCENERY_CLOUD_WALL_FLAGS     0x0c002800038481c6ULL
#define SCENERY_STONE_SURFACE_FLAGS  0x4c002800030401c7ULL
#define SCENERY_WOOD_SURFACE_FLAGS   0x4c002800028001c6ULL
#define SCENERY_METAL_SURFACE_FLAGS  0x4c002800038481c7ULL
#define SCENERY_CLOUD_SURFACE_FLAGS  0x4c002800038481c6ULL

static const TestFieldSceneryType testFieldSceneryTypes[] = {
    {1393,1393,"Stone Parapet (Light Cover)",SCENERY_PARAPET,SCENERY_STONE_SURFACE_FLAGS,1,1},
    {1394,1394,"Stone Parapet (Medium Cover)",SCENERY_PARAPET,SCENERY_STONE_SURFACE_FLAGS,2,1},
    {1395,1395,"Stone Parapet (Heavy Cover)",SCENERY_PARAPET,SCENERY_STONE_SURFACE_FLAGS,3,2},
    {1396,1409,"Stone Wall",SCENERY_WALL,SCENERY_STONE_WALL_FLAGS,0,0},
    {1410,1417,"Cliff",SCENERY_WALL,SCENERY_STONE_WALL_FLAGS,0,0},
    {1418,1418,"Cliff Parapet (Light Cover)",SCENERY_PARAPET,SCENERY_STONE_SURFACE_FLAGS,1,1},
    {1419,1419,"Cliff Parapet (Medium Cover)",SCENERY_PARAPET,SCENERY_STONE_SURFACE_FLAGS,2,1},
    {1420,1423,"Cliff",SCENERY_WALL,SCENERY_STONE_WALL_FLAGS,0,0},
    {1424,1424,"Bramble Parapet (Light Cover)",SCENERY_PARAPET,SCENERY_WOOD_SURFACE_FLAGS,1,1},
    {1425,1425,"Bramble Parapet (Medium Cover)",SCENERY_PARAPET,SCENERY_WOOD_SURFACE_FLAGS,2,1},
    {1426,1430,"Bramble Wall",SCENERY_WALL,SCENERY_WOOD_WALL_FLAGS,0,0},
    {1432,1432,"Cliff Parapet (Light Cover)",SCENERY_PARAPET,SCENERY_STONE_SURFACE_FLAGS,1,1},
    {1433,1433,"Cliff Parapet (Medium Cover)",SCENERY_PARAPET,SCENERY_STONE_SURFACE_FLAGS,2,1},
    {1434,1435,"Cliff",SCENERY_WALL,SCENERY_STONE_WALL_FLAGS,0,0},
    {1446,1446,"Palisade Parapet",SCENERY_PARAPET,SCENERY_WOOD_SURFACE_FLAGS,1,1},
    {1447,1447,"Palisade Parapet (High)",SCENERY_PARAPET,SCENERY_WOOD_SURFACE_FLAGS,1,2},
    {1448,1455,"Palisade Wall",SCENERY_WALL,SCENERY_WOOD_WALL_FLAGS,0,0},
    {1462,1462,"Wooden Stairs",SCENERY_STAIRS,SCENERY_WOOD_SURFACE_FLAGS,0,0},
    {1463,1463,"Bronze Parapet (Medium Cover)",SCENERY_PARAPET,SCENERY_METAL_SURFACE_FLAGS,2,1},
    {1464,1477,"Bronze Wall",SCENERY_WALL,SCENERY_METAL_WALL_FLAGS,0,0},
    {1478,1478,"Bronze Stairs",SCENERY_STAIRS,SCENERY_METAL_SURFACE_FLAGS,0,0},
    {1479,1479,"Silver Parapet (Medium Cover)",SCENERY_PARAPET,SCENERY_METAL_SURFACE_FLAGS,2,1},
    {1480,1493,"Silver Wall",SCENERY_WALL,SCENERY_METAL_WALL_FLAGS,0,0},
    {1494,1494,"Silver Stairs",SCENERY_STAIRS,SCENERY_METAL_SURFACE_FLAGS,0,0},
    {1495,1495,"Golden Parapet (Heavy Cover)",SCENERY_PARAPET,SCENERY_METAL_SURFACE_FLAGS,3,2},
    {1496,1509,"Golden Wall",SCENERY_WALL,SCENERY_METAL_WALL_FLAGS,0,0},
    {1510,1510,"Golden Stairs",SCENERY_STAIRS,SCENERY_METAL_SURFACE_FLAGS,0,0},
    {1511,1516,"Stone Stairs",SCENERY_STAIRS,SCENERY_STONE_SURFACE_FLAGS,0,0},
    {1523,1523,"Cloud Parapet",SCENERY_PARAPET,SCENERY_CLOUD_SURFACE_FLAGS,2,1},
    {1524,1531,"Cloud Wall",SCENERY_WALL,SCENERY_CLOUD_WALL_FLAGS,0,0},
    {1532,1532,"Cloud Parapet",SCENERY_PARAPET,SCENERY_CLOUD_SURFACE_FLAGS,2,1},
    {1534,1541,"Earth Wall",SCENERY_WALL,SCENERY_STONE_WALL_FLAGS,0,0},
    {1542,1550,"Earth Rampart (Light Cover)",SCENERY_PARAPET,SCENERY_STONE_SURFACE_FLAGS,1,1},
    {1590,1597,"Stone Wall",SCENERY_WALL,SCENERY_STONE_WALL_FLAGS,0,0},
    {1634,1634,"Cabbage",SCENERY_OTHER,0,0,0},
};

#define SCENERY_FIRST_TEMPLATE 1393
#define SCENERY_LAST_TEMPLATE 1634
static struct {
    int observed;
    unsigned char record[MONSTER_SIZE];
} testFieldSceneryBaseline[SCENERY_LAST_TEMPLATE - SCENERY_FIRST_TEMPLATE + 1];

static void captureTestFieldSceneryTemplates(int level) {
    if (level != 2 || !gameCodeMatches || !TEST_FIELD_VERIFIED_WINDOWS_ABI) return;
    for (size_t n = 0; n < sizeof testFieldSceneryTypes / sizeof *testFieldSceneryTypes; n++) {
        for (int id = testFieldSceneryTypes[n].first; id <= testFieldSceneryTypes[n].last; id++) {
            const unsigned char *record = monsterRecord(id);
            int index = id - SCENERY_FIRST_TEMPLATE;
            if (!testFieldSceneryBaseline[index].observed && record && isUsedMonster(id)) {
                memcpy(testFieldSceneryBaseline[index].record, record, MONSTER_SIZE);
                testFieldSceneryBaseline[index].observed = 1;
            }
        }
    }
}

static int testFieldSceneryStandable(const TestFieldSceneryType *type) {
    return type->role == SCENERY_PARAPET || type->role == SCENERY_STAIRS;
}

static int testFieldSceneryRequest = -1;
static const char *testFieldSceneryRequestName;

static int testFieldSceneryLookup(const char *name, int mode) {
    if (testFieldSceneryRequest >= 0 && name == testFieldSceneryRequestName && mode == 1 &&
        testFieldLife.active && GetCurrentThreadId() == testFieldLife.thread) return testFieldSceneryRequest;
    return GAME_FN(int (*)(const char *, int), RVA_FIND_MONSTER)(name, mode);
}

static TestFieldCall testFieldSceneryLookupCall = {
    RVA_PLACE_TERRAIN_FIND_MONSTER_CALL, RVA_FIND_MONSTER, (void *)testFieldSceneryLookup,
    "owned_scenery_exact_lookup", NULL, 0,
};

static int testFieldSceneryLookupValid(void) {
    const unsigned char *call = gameBase + testFieldSceneryLookupCall.callRva;
    int relative;
    memcpy(&relative, call + 1, sizeof relative);
    return call[0] == 0xe8 && call + 5 + relative == gameBase + testFieldSceneryLookupCall.targetRva;
}

static int restoreTestFieldSceneryLookup(void) {
    TestFieldCall *site = &testFieldSceneryLookupCall;
    if (testFieldSceneryLookupValid()) {
        site->on = 0;
        return 1;
    }
    const unsigned char *call = gameBase + site->callRva;
    int relative;
    memcpy(&relative, call + 1, sizeof relative);
    return site->on && call[0] == 0xe8 && call + 5 + relative == site->stub &&
        restoreTestFieldCall(site) && testFieldSceneryLookupValid();
}

static int testFieldSceneryTypeMatches(const TestFieldSceneryType *type, int id) {
    if (!isUsedMonster(id) || !isMonsterSpriteValid(id) || monsterMadeByName(id) ||
        monsterTokenWidth(id) != 1 || monsterTokenHeight(id) != 1) return 0;
    const unsigned char *record = monsterRecord(id);
    int index = id - SCENERY_FIRST_TEMPLATE;
    if (!testFieldSceneryBaseline[index].observed ||
        memcmp(record, testFieldSceneryBaseline[index].record, MONSTER_SIZE)) return 0;
    unsigned long long flags;
    memcpy(&flags, record + (RVA_MONSTER_FLAGS - RVA_MONSTER_TABLE), sizeof flags);
    return !strncmp((const char *)record, type->name, MONSTER_NAME_SIZE) &&
        (!type->flags || flags == type->flags) &&
        monsterAbility(id, MONSTER_ABILITY_WALL_COVER) == type->cover &&
        monsterAbility(id, MONSTER_ABILITY_WALL_RANGE_BONUS) == type->rangeBonus;
}

static const TestFieldSceneryType *testFieldSceneryType(int id) {
    for (size_t i = 0; i < sizeof testFieldSceneryTypes / sizeof *testFieldSceneryTypes; i++) {
        const TestFieldSceneryType *type = &testFieldSceneryTypes[i];
        if (id >= type->first && id <= type->last)
            return testFieldSceneryTypeMatches(type, id) ? type : NULL;
    }
    return NULL;
}

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

static int lTestFieldSceneryTypes(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldSceneryTypes");
    lua_createtable(L, 0, 2);
    int version = TEST_FIELD_VERIFIED_WINDOWS_ABI && gameCodeMatches &&
        testFieldProvenanceCallsValid() && testFieldSceneryLookupValid();
    setIntField(L, "version", version);
    lua_newtable(L);
    int count = 0;
    for (size_t i = 0; version && i < sizeof testFieldSceneryTypes / sizeof *testFieldSceneryTypes; i++) {
        const TestFieldSceneryType *type = &testFieldSceneryTypes[i];
        static const char *const roles[] = {"other","wall","parapet","stairs"};
        for (int id = type->first; id <= type->last; id++) {
            if (!testFieldSceneryTypeMatches(type, id)) continue;
            lua_createtable(L, 0, 7);
            setIntField(L, "monster", id);
            setIntField(L, "owner", 31);
            setIntField(L, "width", 1);
            setIntField(L, "height", 1);
            setBoolField(L, "standable", testFieldSceneryStandable(type));
            lua_pushstring(L, roles[type->role]);
            lua_setfield(L, -2, "role");
            lua_pushliteral(L, "object");
            lua_setfield(L, -2, "layer");
            lua_rawseti(L, -2, ++count);
        }
    }
    lua_setfield(L, -2, "types");
    return 1;
}

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

static int readTestFieldSceneryAt(int unit, unsigned long long generation, int x, int y, TestFieldUnitSnapshot *snapshot) {
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
    if (snapshot) *snapshot = row;
    return 1;
}

static int testFieldSceneryIdentity(int unit, unsigned long long generation, int x, int y) {
    if (!readTestFieldSceneryAt(unit, generation, x, y, NULL)) return 0;
    int width = G32(RVA_BATTLE_WIDTH);
    for (int bx = 0; bx < width; bx++)
        for (int by = 0; by < BATTLE_ROWS; by++) {
            const unsigned char *cell = battleCell(bx, by);
            if (*(const int *)cell == unit || ((bx != x || by != y) && *(const int *)(cell + BATTLE_CELL_OBJECT) == unit))
                return 0;
        }
    return 1;
}

static int testFieldSceneryAnchorsValid(void) {
    int width = G32(RVA_BATTLE_WIDTH);
    if (width < 1 || width > BATTLE_COLUMNS_MAX) return 0;
    for (int x = 0; x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            const unsigned char *cell = battleCell(x, y);
            int actor = *(const int *)cell, object = *(const int *)(cell + BATTLE_CELL_OBJECT);
            if (actor >= 0 && actor < UNIT_TABLE_MAX && testFieldLife.actors[actor].source == TEST_FIELD_OWNED_SCENERY &&
                !testFieldLife.actors[actor].cleanup)
                return 0;
            if (object < 0 || object >= UNIT_TABLE_MAX) continue;
            const TestFieldMadeUnit *made = &testFieldLife.actors[object];
            if (made->source == TEST_FIELD_OWNED_SCENERY && !made->cleanup && (made->sceneryX != x || made->sceneryY != y))
                return 0;
        }
    return 1;
}

static int lTestFieldSceneryState(lua_State *L) {
    const char *call = "coe5.testFieldSceneryState";
    const char *reason = testFieldSceneryProblem(L, call, 0);
    if (reason) return refuseTestFieldRead(L, call, reason);
    if (!testFieldSceneryAnchorsValid()) return refuseTestFieldRead(L, call, "scenery_identity_unverified");
    lua_createtable(L, 0, 3);
    setIntField(L, "version", 1);
    setIntField(L, "session", (lua_Integer)testFieldLife.session);
    lua_newtable(L);
    int count = 0;
    for (int unit = 0; unit < UNIT_TABLE_MAX; unit++) {
        const TestFieldMadeUnit *made = &testFieldLife.actors[unit];
        if (made->source != TEST_FIELD_OWNED_SCENERY || made->cleanup) continue;
        TestFieldUnitSnapshot row;
        if (!readTestFieldSceneryAt(unit, made->generation, made->sceneryX, made->sceneryY, &row))
            return refuseTestFieldRead(L, call, "scenery_identity_unverified");
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

static int refuseUnverifiedScenery(lua_State *L, const char *call, const char *reason) {
    testFieldLife.pending = testFieldLife.failed = 1;
    return refuseTestFieldRead(L, call, reason);
}

static int testFieldSceneryBoardSame(const unsigned char *before, const unsigned char *cell, size_t boardBytes) {
    const unsigned char *board = battleCell(0, 0);
    size_t offset = (size_t)(cell - board);
    return !memcmp(before, board, offset) &&
        !memcmp(before + offset + BATTLE_CELL_SIZE, cell + BATTLE_CELL_SIZE, boardBytes - offset - BATTLE_CELL_SIZE);
}

static int testFieldSceneryRetiredCell(const unsigned char *before, const unsigned char *after) {
    unsigned char expected[BATTLE_CELL_SIZE];
    const int empty = -1;
    unsigned int flags;
    memcpy(expected, before, sizeof expected);
    memcpy(expected + BATTLE_CELL_OBJECT, &empty, sizeof empty);
    if (!memcmp(expected, after, sizeof expected)) return 1;
    memcpy(&flags, expected + BATTLE_CELL_FLAGS, sizeof flags);
    flags &= ~0x101u;
    memcpy(expected + BATTLE_CELL_FLAGS, &flags, sizeof flags);
    return !memcmp(expected, after, sizeof expected);
}

static int lAddTestFieldScenery(lua_State *L) {
    const char *call = "coe5.addTestFieldScenery";
    const char *reason = testFieldSceneryProblem(L, call, 1);
    lua_Integer monster = -1;
    const TestFieldSceneryType *type = NULL;
    int x = -1, y = -1, occupant = -1, actorX = -1, actorY = -1;
    if (!reason && (!readTestFieldInteger(L, 1, &monster) || monster < 0 || monster >= MONSTER_TABLE_MAX ||
        !(type = testFieldSceneryType((int)monster)))) reason = "unsupported_scenery";
    if (!reason) reason = testFieldSceneryTarget(L, 2, &x, &y, testFieldSceneryStandable(type) ? &occupant : NULL);
    if (!reason && occupant >= 0 && !unitBattleCell(occupant, &actorX, &actorY)) reason = "unverified_occupant";
    if (!reason && occupant >= 0 && testFieldTokenCovering(x, y, 0, occupant, 1) != -1) reason = "blocked";
    if (!reason && !testFieldSceneryLookupValid()) reason = "scenery_lookup_unavailable";
    if (reason) return refuseTestFieldRead(L, call, reason);

    unsigned char *cell = battleCell(x, y);
    unsigned char old[BATTLE_CELL_SIZE], expected[BATTLE_CELL_SIZE];
    unsigned char actorBody[UNIT_SIZE];
    const unsigned char *actorRecord = occupant >= 0 ? GAME_PTR(RVA_UNITS + (size_t)occupant * UNIT_SIZE) : NULL;
    unsigned char *actorAnchor = occupant >= 0 ? battleCell(actorX, actorY) : NULL;
    unsigned long long actorGeneration = occupant >= 0 ? testFieldLife.generations[occupant] : 0;
    memcpy(old, cell, sizeof old);
    const unsigned long long serial = testFieldLife.serial, session = testFieldLife.session;
    const int width = G32(RVA_BATTLE_WIDTH);
    size_t boardBytes = (size_t)width * BATTLE_ROWS * BATTLE_CELL_SIZE;
    if (actorAnchor && *(int *)actorAnchor != occupant) return refuseTestFieldRead(L, call, "unverified_occupant");
    unsigned char *board = malloc(boardBytes);
    if (!board) return refuseTestFieldRead(L, call, "out_of_memory");
    memcpy(board, battleCell(0, 0), boardBytes);
    if (actorRecord) memcpy(actorBody, actorRecord, sizeof actorBody);
    if (!pointTestFieldCall(&testFieldSceneryLookupCall)) {
        free(board);
        return refuseTestFieldRead(L, call, "scenery_lookup_unavailable");
    }
    enterTestFieldCode();
    testFieldSceneryRequest = (int)monster;
    testFieldSceneryRequestName = type->name;
    if (actorAnchor) *(int *)actorAnchor = -1;
    int unit = GAME_FN(int (*)(const char *, int, int, int, unsigned long long, int, int),
                       RVA_BATTLE_PLACE_TERRAIN_UNIT)(type->name, x, y, -1, 0, 0, 0);
    testFieldSceneryRequest = -1;
    testFieldSceneryRequestName = NULL;
    leaveTestFieldCode();
    int lookupRestored = restoreTestFieldSceneryLookup();
    if (session != testFieldLife.session || width != G32(RVA_BATTLE_WIDTH)) {
        free(board);
        return refuseUnverifiedScenery(L, call, "scenery_creation_unverified");
    }
    if (actorAnchor) {
        if (!isTestFieldActor(occupant) || actorGeneration != testFieldLife.generations[occupant] ||
            memcmp(actorBody, actorRecord, sizeof actorBody)) {
            free(board);
            return refuseUnverifiedScenery(L, call, "scenery_occupant_changed");
        }
        if (*(int *)actorAnchor == -1) *(int *)actorAnchor = occupant;
        if (*(int *)actorAnchor != occupant) {
            free(board);
            return refuseUnverifiedScenery(L, call, "scenery_occupant_changed");
        }
    }
    if (!lookupRestored || !testFieldSceneryBoardSame(board, cell, boardBytes)) {
        free(board);
        return refuseUnverifiedScenery(L, call, actorAnchor && lookupRestored ? "scenery_occupant_changed" : "scenery_creation_unverified");
    }
    if (unit < 0 && serial == testFieldLife.serial && !memcmp(old, cell, sizeof old) && !testFieldLife.failed) {
        free(board);
        return refuseTestFieldRead(L, call, "native_scenery_refused");
    }
    int nativeRefused = unit < 0;
    if (nativeRefused && testFieldLife.serial == serial + 1) {
        for (int id = 0; id < UNIT_TABLE_MAX; id++)
            if (isTestFieldUnit(id) && testFieldLife.actors[id].generation == serial + 1) {
                unit = id;
                break;
            }
    }
    if (testFieldLife.failed || testFieldLife.serial != serial + 1 || !isTestFieldUnit(unit) ||
        testFieldLife.actors[unit].generation != serial + 1 || testFieldLife.actors[unit].source != TEST_FIELD_SCATTER) {
        free(board);
        return refuseUnverifiedScenery(L, call, "scenery_creation_unverified");
    }
    TestFieldMadeUnit *made = &testFieldLife.actors[unit];
    memcpy(expected, old, sizeof expected);
    memcpy(expected + BATTLE_CELL_OBJECT, &unit, sizeof unit);
    memset(expected + BATTLE_CELL_OBJECT_SLIDE, 0, sizeof(int));
    if (made->actor || made->created.monster != monster || memcmp(expected, cell, sizeof expected) ||
        !testFieldSceneryIdentity(unit, made->generation, x, y)) {
        free(board);
        return refuseUnverifiedScenery(L, call, "scenery_creation_unverified");
    }
    int (*canStand)(int, int, int, int) = GAME_FN(int (*)(int, int, int, int), RVA_BATTLE_CAN_STAND);
    int stood = !nativeRefused && (occupant < 0 || (canStand(occupant, actorX, actorY, 0) && canStand(occupant, actorX, actorY, 4)));
    if (actorAnchor && (session != testFieldLife.session || width != G32(RVA_BATTLE_WIDTH) ||
        !isTestFieldActor(occupant) || actorGeneration != testFieldLife.generations[occupant] ||
        memcmp(actorBody, actorRecord, sizeof actorBody) || memcmp(expected, cell, sizeof expected) ||
        !testFieldSceneryBoardSame(board, cell, boardBytes))) {
        free(board);
        return refuseUnverifiedScenery(L, call, "scenery_occupant_changed");
    }
    if (!stood) {
        if (!retireTestFieldUnit(unit) || session != testFieldLife.session || width != G32(RVA_BATTLE_WIDTH) ||
            (actorAnchor && (!isTestFieldActor(occupant) || actorGeneration != testFieldLife.generations[occupant] ||
                memcmp(actorBody, actorRecord, sizeof actorBody))) ||
            !testFieldSceneryBoardSame(board, cell, boardBytes) || !testFieldSceneryRetiredCell(expected, cell)) {
            free(board);
            return refuseUnverifiedScenery(L, call, "cleanup_unverified");
        }
        memcpy(cell, old, sizeof old);
        free(board);
        return refuseTestFieldRead(L, call, nativeRefused ? "native_scenery_refused" : "native_scenery_rejected");
    }
    free(board);
    made->source = TEST_FIELD_OWNED_SCENERY;
    made->sceneryX = x;
    made->sceneryY = y;
    made->sceneryStandable = testFieldSceneryStandable(type);
    memcpy(&made->scenerySlide, old + BATTLE_CELL_OBJECT_SLIDE, sizeof made->scenerySlide);
    testFieldInitialDeploymentReset();
    lua_pushinteger(L, unit);
    return 1;
}

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

static int lRemoveTestFieldScenery(lua_State *L) {
    const char *call = "coe5.removeTestFieldScenery";
    const char *reason = testFieldSceneryProblem(L, call, 1);
    int unit;
    TestFieldMadeUnit *made = reason ? NULL : testFieldSceneryHandle(L, &unit);
    if (!reason && !made) reason = "stale_scenery";
    if (!reason && testFieldTokenCovering(made->sceneryX, made->sceneryY, 0, -1, 1) >= 0) reason = "scenery_occupied";
    if (reason) return refuseTestFieldRead(L, call, reason);
    unsigned char *cell = battleCell(made->sceneryX, made->sceneryY);
    const int width = G32(RVA_BATTLE_WIDTH);
    const unsigned long long session = testFieldLife.session;
    size_t boardBytes = (size_t)width * BATTLE_ROWS * BATTLE_CELL_SIZE;
    unsigned char *board = malloc(boardBytes);
    if (!board) return refuseTestFieldRead(L, call, "out_of_memory");
    const unsigned char *before = board + (cell - battleCell(0, 0));
    memcpy(board, battleCell(0, 0), boardBytes);
    if (!retireTestFieldUnit(unit) || session != testFieldLife.session || width != G32(RVA_BATTLE_WIDTH) ||
        !testFieldSceneryBoardSame(board, cell, boardBytes) || !testFieldSceneryRetiredCell(before, cell)) {
        free(board);
        return refuseUnverifiedScenery(L, call, "cleanup_unverified");
    }
    const int empty = -1;
    memcpy(cell, before, BATTLE_CELL_SIZE);
    memcpy(cell + BATTLE_CELL_OBJECT, &empty, sizeof empty);
    memcpy(cell + BATTLE_CELL_OBJECT_SLIDE, &made->scenerySlide, sizeof made->scenerySlide);
    free(board);
    made->cleanup = 1;
    testFieldInitialDeploymentReset();
    lua_pushboolean(L, 1);
    return 1;
}

static int testFieldFeatureAt(int x, int y, int *objectLayer) {
    int unit = GAME_FN(int (*)(int, int), RVA_BATTLE_UNIT_AT)(x, y);
    *objectLayer = unit < 0;
    return unit < 0 ? GAME_FN(int (*)(int, int, int), RVA_BATTLE_UNIT_AT_2)(x, y, 0) : unit;
}

static int lInspectTestFieldFeature(lua_State *L) {
    const char *call = "coe5.inspectTestFieldFeature";
    checkLuaMainThread(L, call);
    LuaScript *script = checkLuaScript(L);
    int final = lua_isboolean(L, 3) && lua_toboolean(L, 3);
    const char *reason = final ? testFieldFinalReadProblem(script) : testFieldReadProblem(script);
    if (!reason && !lua_isnoneornil(L, 3) && !lua_isboolean(L, 3)) reason = "bad_final";
    if (!reason && (!TEST_FIELD_VERIFIED_WINDOWS_ABI || !testField.editor || !testFieldLife.active ||
        testFieldLife.failed || testFieldLife.pending || testFieldLife.retiring || testField.fighting ||
        testField.activePlayer >= 0 ||
        (!final && testFieldFinalViewActive()))) reason = "feature_inspect_unavailable";
    if (!reason && (testFieldDeploymentBusy || testFieldLife.depth)) reason = "field_transaction_active";
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
                captured->layer != objectLayer || captured->anchorX < 0 || captured->anchorX >= width ||
                captured->anchorY < 0 || captured->anchorY >= BATTLE_ROWS) reason = "final_feature_unavailable";
            else {
                int liveLayer;
                int live = testFieldFeatureAt((int)x, (int)y, &liveLayer);
                const unsigned char *anchor = battleCell(captured->anchorX, captured->anchorY);
                if (live != unit || liveLayer != objectLayer ||
                    *(const int *)(anchor + (objectLayer ? BATTLE_CELL_OBJECT : 0)) != unit)
                    reason = "final_feature_changed_or_removed";
            }
        }
    } else if (!reason) {
        unit = testFieldFeatureAt((int)x, (int)y, &objectLayer);
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
        if (captured && (captured->generation != made->generation || captured->creationSource != made->source ||
            row.hp != captured->hp ||
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
    testFieldDeploymentBusy = 1;
    luaCodeRunning--;
    GAME_FN(unsigned long long (*)(unsigned, int, int, int, int), RVA_UNIT_STATS_SCREEN)((unsigned)unit, -1, 0, 0, -1);
    luaCodeRunning++;
    testFieldInspectActive = 0;
    testFieldDeploymentBusy = 0;
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
