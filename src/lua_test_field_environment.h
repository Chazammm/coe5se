#pragma once

static int testFieldTemplateAvailable(const TestFieldTerrainPolicy *policy) {
    if (!policy || !policy->nativeTemplate) return 1;
    char path[MAX_PATH]; int header[5] = {0};
    if (!buildPath(path, sizeof path, __func__, "%s/%s.coem", (const char *)GAME_PTR(RVA_BATMAPS_DIR), policy->nativeTemplate)) return 0;
    FILE *file = fopen(path, "rb");
    size_t read = file ? fread(header, 1, sizeof header, file) : 0;
    if (file) fclose(file);
    return read == sizeof header && (unsigned)header[0] == BATTLE_MAP_MAGIC && header[1] >= 2 &&
           header[3] >= 2 && header[3] <= BATTLE_COLUMNS_MAX && header[4] == BATTLE_ROWS;
}

static const char *testFieldEnvironmentProblem(void) {
    if (!testField.environmentBorrowed) return NULL;
    const TestFieldTerrainPolicy *policy = findTestFieldTerrainPolicy(testField.terrain);
    const char *reason = testFieldTerrainPolicyWater(policy) ? "water_context_mismatch" : "field_context_mismatch";
    if (!policy || !testField.setupComplete || !testField.nativeTemplateObserved || testField.customMapReplaced ||
        strcmp(testField.nativeTemplate, policy->nativeTemplate ? policy->nativeTemplate : "") ||
        !!G32(RVA_BATTLE_IS_SIEGE) != !!(policy->battleFlags & 0x200) ||
        G32(RVA_BATTLE_IS_TOWER) != testFieldTerrainPolicyTower(policy) ||
        G32(RVA_BATTLE_AT_SEA) != (int)!!(policy->requiredFlags & UINT64_C(4)) ||
        G32(RVA_BATTLE_PLANE_3_OR_4) ||
        (unsigned)GAME_FN(int (*)(int,int), RVA_SQUARE_FORT_PARTS)(testField.x,testField.y) != policy->battleFlags ||
        GAME_FN(int (*)(int,int), RVA_PLANE_AT)(testField.x,testField.y) != 0 ||
        *(short *)(testFieldSquare() + (RVA_SQUARES - RVA_SQUARE_RECORDS)) != testField.terrain ||
        *(short *)(testFieldSquare() + SQUARE_TEMPERATURE) != 25) return reason;
    if (testFieldTerrainPolicyWater(policy) &&
        (G32(RVA_BATTLE_SHIPS_A) != 0 || G32(RVA_BATTLE_SHIPS_D) != 0)) return reason;
    return NULL;
}

static int lTestFieldEnvironmentState(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldEnvironmentState");
    const char *reason = testFieldReadProblem(checkLuaScript(L));
    if (!reason && !testField.environmentBorrowed) reason = "unverified_context";
    if (reason) return refuseTestFieldRead(L, "coe5.testFieldEnvironmentState", reason);
    lua_newtable(L);
    setIntField(L, "version", 1); setIntField(L, "session", testFieldLife.session);
    setIntField(L, "terrain", testField.terrain); setIntField(L, "plane", GAME_FN(int (*)(int,int), RVA_PLANE_AT)(testField.x,testField.y));
    setIntField(L, "ground", G16(RVA_SQUARE_RECORDS + ((size_t)testField.x * MAP_COLUMN_SQUARES + testField.y) * SQUARE_SIZE + SQUARE_GROUND));
    setIntField(L, "temperature", (short)G16(RVA_SQUARE_RECORDS + ((size_t)testField.x * MAP_COLUMN_SQUARES + testField.y) * SQUARE_SIZE + SQUARE_TEMPERATURE));
    setIntField(L, "width", G32(RVA_BATTLE_WIDTH)); setIntField(L, "height", BATTLE_ROWS);
    setIntField(L, "fortParts", GAME_FN(int (*)(int,int), RVA_SQUARE_FORT_PARTS)(testField.x,testField.y));
    setBoolField(L, "atSea", G32(RVA_BATTLE_AT_SEA) != 0);
    setIntField(L,"boatCountA",G32(RVA_BATTLE_SHIPS_A));setIntField(L,"boatCountB",G32(RVA_BATTLE_SHIPS_D));
    setBoolField(L, "siege", G32(RVA_BATTLE_IS_SIEGE) != 0);
    setBoolField(L, "tower", G32(RVA_BATTLE_IS_TOWER) != 0);
    setBoolField(L, "underground", G32(RVA_BATTLE_PLANE_3_OR_4) != 0);
    setBoolField(L, "setupReturned", testField.setupComplete);
    setBoolField(L, "nativeTemplateObserved", testField.nativeTemplateObserved);
    setBoolField(L, "customMapReplaced", testField.customMapReplaced);
    lua_pushstring(L, testField.nativeTemplate); lua_setfield(L, -2, "nativeTemplate");
    lua_pushliteral(L, "native selection before load; returned setup and current field context, not full environment state"); lua_setfield(L, -2, "scope");
    return 1;
}

static void observeTestFieldNativeMap(const char *path, int replaced) {
    if (!testField.settingUp || !testField.open || !testFieldLife.active ||
        GetCurrentThreadId() != testFieldLife.thread || G32(RVA_BATTLE_X) != testField.x || G32(RVA_BATTLE_Y) != testField.y) return;
    if (replaced) {
        testField.customMapReplaced = 1;
    } else if (path) {
        testField.nativeTemplateObserved = 1;
        gameBattleMapName(path, testField.nativeTemplate, sizeof testField.nativeTemplate);
    }
}

static int lTestFieldEnvironmentInfo(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldEnvironmentInfo");
    if (!gameCodeMatches) return refuseTestFieldRead(L, "coe5.testFieldEnvironmentInfo", "game_code_differs");
    lua_newtable(L);
    setIntField(L, "rows", BATTLE_ROWS);
    setIntField(L, "minimumColumns", TEST_FIELD_MIN_WIDTH);
    setIntField(L, "maximumColumns", TEST_FIELD_MAX_WIDTH);
    setIntField(L, "minimumWidthVersion", 1);
    setIntField(L, "actorStateVersion", 1);
    setIntField(L, "environmentStateVersion", 1);
    setIntField(L, "templateContextVersion", 1);
    setIntField(L, "towerContextVersion", 1);
    setIntField(L, "waterContextVersion", 1);
    lua_newtable(L);
    int count = 0;
    for (size_t i = 0; i < TEST_FIELD_TERRAIN_POLICY_COUNT; i++) {
        const TestFieldTerrainPolicy *policy = &testFieldTerrainPolicies[i];
        if (policy->id > G32(RVA_LAST_TERRAIN) || testFieldTerrainProblem(L, policy->id)) continue;
        lua_newtable(L);
        setIntField(L, "id", policy->id);
        lua_pushstring(L, policy->name); lua_setfield(L, -2, "name");
        setIntField(L, "battleMapFlags", policy->battleFlags);
        setBoolField(L, "verifiedNeutralField", policy->nativeTemplate == NULL);
        setBoolField(L, "verifiedTemplateField", policy->nativeTemplate != NULL);
        if (policy->nativeTemplate) {
            lua_pushstring(L, policy->nativeTemplate);
            lua_setfield(L, -2, "nativeTemplate");
        }
        lua_rawseti(L, -2, ++count);
    }
    lua_setfield(L, -2, "terrains");
    return 1;
}

static int lTestFieldWaterState(lua_State *L) {
    const char *call="coe5.testFieldWaterState";
    checkLuaMainThread(L,call);
    lua_Integer unit=luaL_checkinteger(L,1),x=luaL_checkinteger(L,2),y=luaL_checkinteger(L,3);
    const char *reason=testFieldUnitEditProblem(checkLuaScript(L),unit);
    if (!reason && (!testFieldLife.active || testFieldLife.pending || testFieldLife.failed)) reason="cleanup_unverified";
    if (!reason && !isTestFieldActor((int)unit)) reason="not_actor";
    TestFieldFootprint footprint;
    if (!reason) reason=readTestFieldFootprint(checkLuaScript(L),G16(RVA_UNIT_MONSTER+(size_t)unit*UNIT_SIZE),unit,&footprint);
    if (!reason && (x<0 || y<0 || x>G32(RVA_BATTLE_WIDTH)-footprint.width || y>BATTLE_ROWS-footprint.height)) reason="bad_anchor";
    if (reason) return refuseTestFieldRead(L,call,reason);
    uint64_t session=testFieldLife.session,generation=testFieldLife.actors[unit].generation;
    unsigned long long flags=GAME_FN(unsigned long long (*)(int),RVA_UNIT_FLAGS)((int)unit);
    if (testFieldUnitEditProblem(checkLuaScript(L),unit) || testFieldLife.pending || testFieldLife.failed || session!=testFieldLife.session ||
        generation!=testFieldLife.actors[unit].generation || footprint.monster!=G16(RVA_UNIT_MONSTER+(size_t)unit*UNIT_SIZE))
        return refuseTestFieldRead(L,call,"context_changed");
    int water=GAME_FN(int (*)(int,int),RVA_BATTLE_CELL_TEST)((int)x,(int)y);
    int drown=GAME_FN(int (*)(int,int,int),RVA_BATTLE_TOKEN_SAFE)((int)unit,(int)x,(int)y);
    int ship=testFieldWaterTransportUnit((int)unit);
    if (testFieldUnitEditProblem(checkLuaScript(L),unit) || testFieldLife.pending || testFieldLife.failed || session!=testFieldLife.session ||
        generation!=testFieldLife.actors[unit].generation || footprint.monster!=G16(RVA_UNIT_MONSTER+(size_t)unit*UNIT_SIZE))
        return refuseTestFieldRead(L,call,"context_changed");
    lua_newtable(L);pushTestFieldFootprint(L,&footprint);
    setIntField(L,"version",1);setIntField(L,"x",x);setIntField(L,"y",y);
    lua_pushinteger(L,(lua_Integer)flags);lua_setfield(L,-2,"moveFlags");
    setBoolField(L,"cellWater",water);
    setBoolField(L,"willDrown",drown);
    setBoolField(L,"waterMobile",(flags & 0xc1u)!=0);
    setBoolField(L,"ship",ship);
    lua_pushliteral(L,"current native cell cover and footprint drowning predicate; not transport, campaign entry or a full environment snapshot");lua_setfield(L,-2,"scope");
    return 1;
}
