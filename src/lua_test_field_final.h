#pragma once

/* Windows 5.43. The sole callsite is AFTER all 75 aftermath ticks and BEFORE
 * afterFightUpkeep, removeTempMen and the final native grid clear. Capture is
 * read-only; the original native upkeep ALWAYS runs. This is an exact bounded
 * unit/cell observation at that boundary, NOT a retained complete render scene.
 * Include after unit_state/preview/input and before lua_test_field_inspect.h. */
typedef struct { int unit, object, flags, ground, hit; } TestFieldFinalCell;
typedef struct {
    int unit, actor, readable, hp, monster, owner, team, worldX, worldY, width, height, sprite;
    int anchors, occupied, anchorX, anchorY, layer, createdDuringFight, createdMonster, createdOwner;
    unsigned long long generation;
} TestFieldFinalUnit;
typedef struct { const char *code; int unit, x, y; } TestFieldFinalIssue;
#define TEST_FIELD_FINAL_ISSUES 256
static struct {
    int armed, finished, captured, complete, captures, width, round, winner, count, issueCount, issueTotal;
    unsigned long thread;
    unsigned long long session;
    unsigned long long *initial;
    int *map;
    TestFieldFinalUnit *units;
    TestFieldFinalCell *grid;
    TestFieldFinalIssue issues[TEST_FIELD_FINAL_ISSUES];
} testFieldFinal;
static int testFieldFinalInstalled;
static const char *testFieldFinalUnavailable;

static void testFieldFinalIssue(const char *code, int unit, int x, int y) {
    testFieldFinal.complete = 0;
    testFieldFinal.issueTotal++;
    if (testFieldFinal.issueCount < TEST_FIELD_FINAL_ISSUES)
        testFieldFinal.issues[testFieldFinal.issueCount++] = (TestFieldFinalIssue){code, unit, x, y};
}

static void testFieldFinalReset(void) {
    free(testFieldFinal.initial); free(testFieldFinal.map); free(testFieldFinal.units); free(testFieldFinal.grid);
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

static void testFieldFinalCapture(void) {
    testFieldFinal.captures++;
    if (testFieldFinal.captures != 1) { testFieldFinalIssue("multiple_capture_boundaries", -1, -1, -1); return; }
    testFieldFinal.captured = 1;
    testFieldFinal.complete = 1;
    testFieldFinal.width = G32(RVA_BATTLE_WIDTH);
    testFieldFinal.round = G32(RVA_BATTLE_ROUND);
    testFieldFinal.winner = G32(RVA_BATTLE_WINNER_PLAYER);
    int width = testFieldFinal.width, last = G32(RVA_UNIT_COUNT), safeHit = 1;
    if (width < 1 || width > TEST_FIELD_EDITOR_MAX_COLUMNS || last < -1 || last >= UNIT_TABLE_MAX) {
        testFieldFinalIssue("bad_native_bounds", -1, -1, -1); return;
    }
    if (testFieldLife.failed || testFieldLife.pending) testFieldFinalIssue("tracking_unverified", -1, -1, -1);
    testFieldFinal.grid = calloc((size_t)width * BATTLE_ROWS, sizeof *testFieldFinal.grid);
    if (!testFieldFinal.grid) { testFieldFinalIssue("out_of_memory", -1, -1, -1); return; }
    for (int id = 0; id < UNIT_TABLE_MAX; id++) {
        TestFieldLifeActor *a = &testFieldLife.actors[id];
        if (!a->generation) continue;
        TestFieldFinalUnit *u = &testFieldFinal.units[testFieldFinal.count];
        *u = (TestFieldFinalUnit){.unit=id, .actor=a->actor, .generation=a->generation,
            .monster=-1, .owner=-1, .sprite=-1, .anchorX=-1, .anchorY=-1, .worldX=-1, .worldY=-1,
            .createdMonster=a->created.monster, .createdOwner=a->created.owner,
            .createdDuringFight=testFieldFinal.initial[id] != a->generation};
        testFieldFinal.map[id] = testFieldFinal.count++;
        if (!testFieldLifeOwns(id) || id > last) { testFieldFinalIssue("identity_unverified", id, -1, -1); continue; }
        size_t at = (size_t)id * UNIT_SIZE;
        u->readable=1; u->hp=G16(RVA_UNIT_HP+at); u->monster=G16(RVA_UNIT_MONSTER+at); u->owner=G16(RVA_UNIT_PLAYER+at);
        u->worldX=G32(RVA_UNIT_X+at); u->worldY=G32(RVA_UNIT_X+at+4);
        u->team=u->owner == G32(RVA_BATTLE_PLAYER_A) ? 1 : u->owner == G32(RVA_BATTLE_PLAYER_B) ? 2 : 0;
        if (u->monster < 0 || u->monster >= MONSTER_TABLE_MAX || !isUsedMonster(u->monster) || !testFieldPreviewSpriteValid(u->monster)) {
            u->readable=0; testFieldFinalIssue("bad_monster_or_sprite", id, -1, -1); continue;
        }
#ifndef __APPLE__
        u->width=GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(id);
        u->height=GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(id);
#endif
        u->sprite=*(const int *)(monsterRecord(u->monster)+(RVA_MONSTER_SPRITES-RVA_MONSTER_TABLE));
        if (u->width < 1 || u->width > 3 || u->height < 1 || u->height > 3) {
            u->readable=0; testFieldFinalIssue("bad_footprint", id, -1, -1);
        }
    }
    for (int x=0; x<width; x++) for (int y=0; y<BATTLE_ROWS; y++) {
        TestFieldFinalCell *c=&testFieldFinal.grid[x*BATTLE_ROWS+y]; const unsigned char *at=battleCell(x,y);
        memcpy(&c->unit,at,4); memcpy(&c->object,at+4,4); memcpy(&c->flags,at+8,4);
        short ground; memcpy(&ground,at+12,2); c->ground=ground; c->hit=-1;
        int ids[2]={c->unit,c->object};
        for (int layer=0; layer<2; layer++) {
            int id=ids[layer]; if (id == -1) continue;
            int index=testFieldFinalIndex(id);
            if (index < 0 || !testFieldFinal.units[index].readable) {
                safeHit=0; testFieldFinalIssue("unverified_grid_identity",id,x,y); continue;
            }
            TestFieldFinalUnit *u=&testFieldFinal.units[index];
            u->anchors++; u->anchorX=x; u->anchorY=y; u->layer=layer;
        }
    }
    if (safeHit) {
        for (int x=0; x<width; x++) for (int y=0; y<BATTLE_ROWS; y++) {
            TestFieldFinalCell *c=&testFieldFinal.grid[x*BATTLE_ROWS+y];
#ifndef __APPLE__
            c->hit=GAME_FN(int (*)(int,int),RVA_BATTLE_UNIT_AT)(x,y);
#endif
            if (c->hit >= 0) {
                int i=testFieldFinalIndex(c->hit);
                if (i < 0) testFieldFinalIssue("unexpected_hit",c->hit,x,y);
                else testFieldFinal.units[i].occupied++;
            }
        }
    } else testFieldFinalIssue("hit_test_unavailable",-1,-1,-1);
    for (int i=0; i<testFieldFinal.count; i++) {
        TestFieldFinalUnit *u=&testFieldFinal.units[i];
        if (!u->readable || u->hp <= 0 || !u->anchors) continue;
        if (u->anchors != 1) { testFieldFinalIssue("duplicate_anchor",u->unit,u->anchorX,u->anchorY); continue; }
        if (u->anchorX+u->width > width || u->anchorY+u->height > BATTLE_ROWS) {
            testFieldFinalIssue("footprint_outside",u->unit,u->anchorX,u->anchorY); continue;
        }
        if (u->actor && u->layer == 0 && safeHit) {
            if (u->occupied != u->width*u->height) testFieldFinalIssue("footprint_hit_count",u->unit,u->anchorX,u->anchorY);
            for (int dx=0; dx<u->width; dx++) for (int dy=0; dy<u->height; dy++)
                if (testFieldFinal.grid[(u->anchorX+dx)*BATTLE_ROWS+u->anchorY+dy].hit != u->unit)
                    testFieldFinalIssue("footprint_hit_mismatch",u->unit,u->anchorX+dx,u->anchorY+dy);
        } else if (u->actor && u->layer != 0) testFieldFinalIssue("actor_on_object_layer",u->unit,u->anchorX,u->anchorY);
    }
    writeLog("test_field_final_captured","session=%llu source=post_aftermath_before_upkeep units=%d cells=%d round=%d winner=%d complete=%d scene_complete=0 issues=%d",
             testFieldFinal.session,testFieldFinal.count,width*BATTLE_ROWS,testFieldFinal.round,testFieldFinal.winner,testFieldFinal.complete,testFieldFinal.issueTotal);
}

#ifndef __APPLE__
static void (*nextTestFieldFinalUpkeep)(int, int);
static void testFieldFinalUpkeep(int x,int y) {
    const char *mod=runningLuaMod();
    if (testFieldFinal.armed && testField.open && testField.fighting && testFieldLife.active && testFieldLife.depth>0 &&
        testFieldFinal.session==testFieldLife.session && GetCurrentThreadId()==testFieldFinal.thread &&
        x==testField.x && y==testField.y && (!mod || !_stricmp(mod,testField.owner))) testFieldFinalCapture();
    nextTestFieldFinalUpkeep(x, y);
}
#endif

static int installTestFieldFinal(void) {
#ifdef __APPLE__
    testFieldFinalUnavailable="windows_only"; return 0;
#else
    if (testFieldFinalInstalled) return testFieldFinalInstalled>0;
    if (!gameCodeMatches) return 0;
    testFieldFinalInstalled=-1; testFieldFinalUnavailable="install_incomplete";
    if (!wrapSite(SITE_BATTLE_AFTER_FIGHT_UPKEEP, (void *)testFieldFinalUpkeep,
                  (void **)&nextTestFieldFinalUpkeep)) {
        testFieldFinalUnavailable = "upkeep_hook_unavailable";
        return 0;
    }
    testFieldFinalInstalled=1;testFieldFinalUnavailable=NULL;
    writeLog("test_field_final_installed","source=post_aftermath_before_upkeep native_upkeep_unchanged=1");return 1;
#endif
}

static int testFieldFinalBegin(void) {
    testFieldFinalReset();
    testFieldFinal.session=testFieldLife.session;
    if (!installTestFieldFinal() || !testField.open || testField.fighting) return 0;
    testFieldFinal.initial=calloc(UNIT_TABLE_MAX,sizeof *testFieldFinal.initial);
    testFieldFinal.map=malloc(UNIT_TABLE_MAX*sizeof *testFieldFinal.map);
    testFieldFinal.units=calloc(UNIT_TABLE_MAX,sizeof *testFieldFinal.units);
    if (!testFieldFinal.initial || !testFieldFinal.map || !testFieldFinal.units) { testFieldFinalIssue("out_of_memory",-1,-1,-1);return 0; }
    for (int id=0;id<UNIT_TABLE_MAX;id++) {
        testFieldFinal.map[id]=-1;
        if (testFieldLifeOwns(id)) testFieldFinal.initial[id]=testFieldLife.actors[id].generation;
    }
#ifndef __APPLE__
    testFieldFinal.thread=GetCurrentThreadId();
#endif
    testFieldFinal.armed=1;return 1;
}

static void testFieldFinalFinish(void) {
    testFieldFinal.armed=0;testFieldFinal.finished=1;
    if (!testFieldFinal.captured) testFieldFinalIssue(testFieldFinalUnavailable ? testFieldFinalUnavailable : "capture_boundary_not_reached",-1,-1,-1);
}

static const char *testFieldFinalReadProblem(const LuaScript *script) {
#ifdef __APPLE__
    (void)script; return "windows_only";
#else
    if (!gameCodeMatches) return "game_code_differs";
    if (!testField.open) return "not_open";
    if (_stricmp(script->mod,testField.owner)) return "not_owner";
    if (testField.fighting) return "in_fight";
    if (!testFieldFinalViewActive()) return "no_final_state";
    return NULL;
#endif
}

static const char *testFieldFinalInspectProblem(const LuaScript *script,lua_Integer id) {
    const char *reason=testFieldFinalReadProblem(script);if (reason) return reason;
    if (id<0 || id>=UNIT_TABLE_MAX) return "bad_unit";
    int i=testFieldFinalIndex((int)id);if (i<0) return "not_in_final_state";
    TestFieldFinalUnit *u=&testFieldFinal.units[i];
    if (!u->readable || !u->actor || u->hp<=0 || u->anchors!=1) return "not_a_visible_survivor";
    if (!testFieldLifeActorOwned((int)id) || testFieldLife.generations[id]!=u->generation) return "identity_changed";
    if (id>G32(RVA_UNIT_COUNT) || G16(RVA_UNIT_HP+(size_t)id*UNIT_SIZE)<=0) return "removed_after_capture";
    /* A strict editor quota is intentionally NOT required after combat. The
     * card wrapper still validates readable IDs and restores its inline lists. */
    return testFieldCasterReadProblem((int)id);
}

static int lTestFieldFinalState(lua_State *L) {
    checkLuaMainThread(L,"coe5.testFieldFinalState");LuaScript *script=checkLuaScript(L);
    const char *reason=testFieldFinalReadProblem(script);
    if (reason) return testFieldReadFailure(L,"coe5.testFieldFinalState",reason);
    lua_createtable(L,0,14);int state=lua_gettop(L);
    lua_pushliteral(L,"post_aftermath_before_upkeep");lua_setfield(L,state,"source");
    lua_pushstring(L,testFieldFinal.captured ? "captured" : "unavailable");lua_setfield(L,state,"phase");
    testFieldStateBool(L,"complete",testFieldFinal.complete && testFieldFinal.captured);
    testFieldStateBool(L,"sceneComplete",0);testFieldStateBool(L,"captured",testFieldFinal.captured);
    lua_pushliteral(L,"current owned native generations and every battlefield cell; not transition history");lua_setfield(L,state,"coverage");
    lua_pushinteger(L,(lua_Integer)testFieldFinal.session);lua_setfield(L,state,"session");
    testFieldStateInt(L,"width",testFieldFinal.width);testFieldStateInt(L,"height",BATTLE_ROWS);
    testFieldStateInt(L,"round",testFieldFinal.round);testFieldStateInt(L,"winner",testFieldFinal.winner);
    testFieldStateInt(L,"issueCount",testFieldFinal.issueTotal);testFieldStateBool(L,"issuesTruncated",testFieldFinal.issueTotal>testFieldFinal.issueCount);
    lua_createtable(L,testFieldFinal.count,0);int rows=lua_gettop(L);
    for (int i=0;i<testFieldFinal.count;i++) {
        TestFieldFinalUnit *u=&testFieldFinal.units[i];size_t at=(size_t)u->unit*UNIT_SIZE;
        int identity=testFieldLifeOwns(u->unit) && testFieldLife.generations[u->unit]==u->generation;
        int hp=identity ? G16(RVA_UNIT_HP+at) : -1;
        const char *inspect=testFieldFinalInspectProblem(script,u->unit);
        lua_createtable(L,0,32);
        testFieldStateInt(L,"unit",u->unit);lua_pushinteger(L,(lua_Integer)u->generation);lua_setfield(L,-2,"generation");
        testFieldStateInt(L,"hp",u->hp);testFieldStateBool(L,"alive",u->readable && u->hp>0);testFieldStateInt(L,"monster",u->monster);
        testFieldStateBool(L,"readable",u->readable);
        testFieldStateInt(L,"owner",u->owner);testFieldStateInt(L,"team",u->team);testFieldStateInt(L,"sprite",u->sprite);
        testFieldStateInt(L,"worldX",u->worldX);testFieldStateInt(L,"worldY",u->worldY);
        testFieldStateInt(L,"width",u->width);testFieldStateInt(L,"height",u->height);testFieldStateInt(L,"anchorCount",u->anchors);
        testFieldStateInt(L,"createdMonster",u->createdMonster);testFieldStateInt(L,"createdOwner",u->createdOwner);
        testFieldStateBool(L,"createdDuringFight",u->createdDuringFight);testFieldStateBool(L,"ownerChanged",u->owner!=u->createdOwner);
        testFieldStateBool(L,"monsterChanged",u->monster!=u->createdMonster);testFieldStateBool(L,"identityMatches",identity);
        testFieldStateBool(L,"currentAlive",identity && hp>0);testFieldStateInt(L,"currentHP",hp);
        testFieldStateInt(L,"currentMonster",identity ? G16(RVA_UNIT_MONSTER+at) : -1);testFieldStateInt(L,"currentOwner",identity ? G16(RVA_UNIT_PLAYER+at) : -1);
        testFieldStateBool(L,"inspectable",!inspect);if (inspect) { lua_pushstring(L,inspect);lua_setfield(L,-2,"inspectReason"); }
        lua_pushstring(L,u->actor ? "actor" : "terrain");lua_setfield(L,-2,"kind");
        lua_pushstring(L,testFieldFinalStatus(u));lua_setfield(L,-2,"status");
        /* flee() writes worldX=-2, but the snapshot does not observe the call.
         * Preserve the marker without claiming a complete route classification. */
        testFieldStateBool(L,"nativeFleeMarker",u->readable && u->hp>0 && !u->anchors && u->worldX==-2);
        lua_pushliteral(L,"not_classified");lua_setfield(L,-2,"routeStatus");
        lua_pushstring(L,u->layer ? "object" : "unit");lua_setfield(L,-2,"anchorLayer");
        lua_pushstring(L,u->actor && !u->layer ? "native_unit_hit_test" : "object_layer_has_no_unit_hit_footprint");lua_setfield(L,-2,"occupiedSource");
        lua_pushliteral(L,"native_new_unit_owned_scope");lua_setfield(L,-2,"provenance");
        lua_newtable(L);lua_setfield(L,-2,"anchors");lua_newtable(L);lua_setfield(L,-2,"occupied");
        lua_rawseti(L,rows,i+1);
    }
    lua_createtable(L,testFieldFinal.grid ? testFieldFinal.width*BATTLE_ROWS : 0,0);int grid=lua_gettop(L);
    if (testFieldFinal.grid) for (int x=0;x<testFieldFinal.width;x++) for (int y=0;y<BATTLE_ROWS;y++) {
        TestFieldFinalCell *c=&testFieldFinal.grid[x*BATTLE_ROWS+y];lua_createtable(L,0,7);
        testFieldStateInt(L,"x",x);testFieldStateInt(L,"y",y);testFieldStateInt(L,"unit",c->unit);testFieldStateInt(L,"object",c->object);
        testFieldStateInt(L,"flags",c->flags);testFieldStateInt(L,"ground",c->ground);testFieldStateInt(L,"hit",c->hit);lua_rawseti(L,grid,x*BATTLE_ROWS+y+1);
        int ids[2]={c->unit,c->object};for (int layer=0;layer<2;layer++) {
            int i=testFieldFinalIndex(ids[layer]);if (i>=0) testFieldStateCoord(L,rows,i,"anchors",x,y);
        }
        int i=testFieldFinalIndex(c->hit);if(i>=0) testFieldStateCoord(L,rows,i,"occupied",x,y);
    }
    /* Terrain's object layer has no unit-hit-test footprint. Record the native
     * anchor and exact native dimensions separately; no invented occupied cells. */
    lua_pushvalue(L,rows);lua_setfield(L,state,"units");lua_pushvalue(L,grid);lua_setfield(L,state,"grid");
    lua_createtable(L,testFieldFinal.issueCount,0);
    for (int i=0;i<testFieldFinal.issueCount;i++) {
        TestFieldFinalIssue *p=&testFieldFinal.issues[i];lua_createtable(L,0,4);
        lua_pushstring(L,p->code);lua_setfield(L,-2,"code");testFieldStateInt(L,"unit",p->unit);testFieldStateInt(L,"x",p->x);testFieldStateInt(L,"y",p->y);lua_rawseti(L,-2,i+1);
    }
    lua_setfield(L,state,"issues");lua_settop(L,state);return 1;
}
