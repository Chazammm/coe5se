#pragma once

typedef struct {
    int unit, monster, owner, x, y, width, height;
    unsigned long long generation;
} TestFieldInitialActor;
typedef struct { int unit, otherUnit, x, y; } TestFieldInitialOverlap;
static struct {
    unsigned long long nextToken, token;
    int captured, invalidated, consumed, eligible, sourceHealthy, completeRoster;
    int lastCommander, monsterRecords, actorCount, overlapCount;
    int battlePlayerA, battlePlayerB, slotTeamA, slotTeamB, rawInBattle;
    const char *reason;
    TestFieldDeploymentSnapshot context;
    size_t unitsBytes, commandersBytes, monstersBytes, generationsBytes, actorsBytes, totalBytes;
    unsigned char *units, *commanders, *monsters, *generations, *actors;
    TestFieldInitialActor *roster;
    TestFieldInitialOverlap *overlaps;
} testFieldInitialDeployment;

static void testFieldInitialDeploymentReset(void) {
    unsigned long long next=testFieldInitialDeployment.nextToken;
    free(testFieldInitialDeployment.context.grid);
    free(testFieldInitialDeployment.units);free(testFieldInitialDeployment.commanders);
    free(testFieldInitialDeployment.monsters);free(testFieldInitialDeployment.generations);
    free(testFieldInitialDeployment.actors);free(testFieldInitialDeployment.roster);
    free(testFieldInitialDeployment.overlaps);
    memset(&testFieldInitialDeployment,0,sizeof testFieldInitialDeployment);
    testFieldInitialDeployment.nextToken=next;
    testFieldInitialDeployment.reason="no_initial_receipt";
}

static int testFieldInitialDeploymentContextSame(const LuaScript *script) {
    TestFieldDeploymentSnapshot *s=&testFieldInitialDeployment.context;
    return testFieldDeploymentContextSame(script,s) &&
        G32(RVA_BATTLE_PLAYER_A)==testFieldInitialDeployment.battlePlayerA &&
        G32(RVA_BATTLE_PLAYER_B)==testFieldInitialDeployment.battlePlayerB &&
        PSLOT16(RVA_SLOT_TEAM, 0, PLAYER_BLOCK_SIZE)==testFieldInitialDeployment.slotTeamA &&
        PSLOT16(RVA_SLOT_TEAM, TEST_BATTLE_ENEMY_PLAYER, PLAYER_BLOCK_SIZE)==testFieldInitialDeployment.slotTeamB &&
        *(const unsigned char *)GAME_PTR(RVA_IN_BATTLE)==testFieldInitialDeployment.rawInBattle;
}

static int testFieldInitialDeploymentSurroundingSame(const LuaScript *script) {
    return testFieldInitialDeployment.captured && testFieldInitialDeploymentContextSame(script) &&
        G32(RVA_LAST_COMMANDER)==testFieldInitialDeployment.lastCommander &&
        tableRecords(TABLE_MONSTER)==testFieldInitialDeployment.monsterRecords &&
        !memcmp(testFieldInitialDeployment.units,GAME_PTR(RVA_UNITS),testFieldInitialDeployment.unitsBytes) &&
        !memcmp(testFieldInitialDeployment.commanders,GAME_PTR(RVA_COMMANDERS),testFieldInitialDeployment.commandersBytes) &&
        !memcmp(testFieldInitialDeployment.monsters,tableAddress(TABLE_MONSTER),testFieldInitialDeployment.monstersBytes) &&
        !memcmp(testFieldInitialDeployment.generations,testFieldLife.generations,testFieldInitialDeployment.generationsBytes) &&
        !memcmp(testFieldInitialDeployment.actors,testFieldLife.actors,testFieldInitialDeployment.actorsBytes);
}

static int testFieldInitialDeploymentFresh(const LuaScript *script) {
    if(!testFieldInitialDeployment.captured || testFieldInitialDeployment.invalidated || testFieldInitialDeployment.consumed)return 0;
    TestFieldDeploymentSnapshot *s=&testFieldInitialDeployment.context;
    if(!testFieldInitialDeploymentSurroundingSame(script) ||
        memcmp(s->grid,GAME_PTR(RVA_BATTLE_GRID),(size_t)s->width*BATTLE_ROWS*BATTLE_CELL_SIZE)) {
        testFieldInitialDeployment.invalidated=1;
        testFieldInitialDeployment.reason="initial_source_changed";
        return 0;
    }
    return 1;
}

static int testFieldInitialDeploymentActorIndex(int unit) {
    for(int i=0;i<testFieldInitialDeployment.actorCount;i++)
        if(testFieldInitialDeployment.roster[i].unit==unit)return i;
    return -1;
}

static void testFieldInitialDeploymentCapture(const LuaScript *script) {
    testFieldInitialDeploymentReset();
    testFieldDeploymentBusy = 1;
    const char *reason=testFieldPolicyProblem(script);
    int *anchorCounts=NULL,*anchorX=NULL,*anchorY=NULL,*objectCounts=NULL,*cover=NULL;
    TestFieldActorBody *bodies=NULL;
    unsigned char *present=NULL;
    TestFieldDeploymentSnapshot *s=&testFieldInitialDeployment.context;
    if(!reason && (battleDrawing || !testField.setupComplete || testField.activePlayer>=0))reason="not_initial_editor";
    if(!reason)reason=testFieldEnvironmentProblem();
    if(reason)goto done;
    s->session=testFieldLife.session;s->serial=testFieldLife.serial;s->created=testFieldLife.createdCount;
    s->width=G32(RVA_BATTLE_WIDTH);s->last=G32(RVA_UNIT_COUNT);s->setup=testField.setupComplete;
    s->activePlayer=G32(RVA_ACTIVE_PLAYER);s->x=testField.x;s->y=testField.y;
    s->sea=G32(RVA_BATTLE_AT_SEA);s->siege=G32(RVA_BATTLE_IS_SIEGE);
    s->tower=G32(RVA_BATTLE_IS_TOWER);s->plane=G32(RVA_BATTLE_PLANE_3_OR_4);
    memcpy(s->square,testFieldSquare(),SQUARE_SIZE);
    testFieldInitialDeployment.battlePlayerA=G32(RVA_BATTLE_PLAYER_A);
    testFieldInitialDeployment.battlePlayerB=G32(RVA_BATTLE_PLAYER_B);
    testFieldInitialDeployment.slotTeamA=PSLOT16(RVA_SLOT_TEAM, 0, PLAYER_BLOCK_SIZE);
    testFieldInitialDeployment.slotTeamB=PSLOT16(RVA_SLOT_TEAM, TEST_BATTLE_ENEMY_PLAYER, PLAYER_BLOCK_SIZE);
    testFieldInitialDeployment.rawInBattle=*(const unsigned char *)GAME_PTR(RVA_IN_BATTLE);
    testFieldInitialDeployment.lastCommander=G32(RVA_LAST_COMMANDER);
    int records=testFieldInitialDeployment.monsterRecords=tableRecords(TABLE_MONSTER);
    if(s->width<1 || s->width>BATTLE_COLUMNS_MAX || s->last < -1 || s->last>=UNIT_TABLE_MAX || s->activePlayer!=0 ||
        testFieldInitialDeployment.lastCommander < -1 || testFieldInitialDeployment.lastCommander>=COMMANDER_TABLE_MAX ||
        records<1 || records>MONSTER_RECORDS_MOVED) {reason="initial_snapshot_bounds";goto done;}
    size_t cells=(size_t)s->width*BATTLE_ROWS,capacity=(size_t)(s->last+1);if(!capacity)capacity=1;
    testFieldInitialDeployment.unitsBytes=(size_t)UNIT_TABLE_MAX*UNIT_SIZE;
    testFieldInitialDeployment.commandersBytes=(size_t)COMMANDER_TABLE_MAX*COMMANDER_SIZE;
    testFieldInitialDeployment.monstersBytes=(size_t)records*MONSTER_SIZE;
    testFieldInitialDeployment.generationsBytes=(size_t)UNIT_TABLE_MAX*sizeof *testFieldLife.generations;
    testFieldInitialDeployment.actorsBytes=(size_t)UNIT_TABLE_MAX*sizeof *testFieldLife.actors;
    size_t total=testFieldInitialDeployment.unitsBytes+testFieldInitialDeployment.commandersBytes+
        testFieldInitialDeployment.monstersBytes+testFieldInitialDeployment.generationsBytes+testFieldInitialDeployment.actorsBytes+
        cells*BATTLE_CELL_SIZE+capacity*(sizeof(TestFieldInitialActor)+sizeof(TestFieldActorBody)+5*sizeof(int)+1)+
        cells*(sizeof(int)+9*sizeof(TestFieldInitialOverlap));
    if(total>128u*1024u*1024u){reason="initial_snapshot_too_large";goto done;}
    testFieldInitialDeployment.totalBytes=total;
    s->grid=malloc(cells*BATTLE_CELL_SIZE);
    testFieldInitialDeployment.units=malloc(testFieldInitialDeployment.unitsBytes);
    testFieldInitialDeployment.commanders=malloc(testFieldInitialDeployment.commandersBytes);
    testFieldInitialDeployment.monsters=malloc(testFieldInitialDeployment.monstersBytes);
    testFieldInitialDeployment.generations=malloc(testFieldInitialDeployment.generationsBytes);
    testFieldInitialDeployment.actors=malloc(testFieldInitialDeployment.actorsBytes);
    testFieldInitialDeployment.roster=calloc(capacity,sizeof *testFieldInitialDeployment.roster);
    testFieldInitialDeployment.overlaps=calloc(cells*9,sizeof *testFieldInitialDeployment.overlaps);
    anchorCounts=calloc(capacity,sizeof *anchorCounts);anchorX=malloc(capacity*sizeof *anchorX);anchorY=malloc(capacity*sizeof *anchorY);
    objectCounts=calloc(capacity,sizeof *objectCounts);cover=malloc(cells*sizeof *cover);
    bodies=calloc(capacity,sizeof *bodies);present=calloc(capacity,1);
    if(!s->grid || !testFieldInitialDeployment.units || !testFieldInitialDeployment.commanders || !testFieldInitialDeployment.monsters ||
        !testFieldInitialDeployment.generations || !testFieldInitialDeployment.actors || !testFieldInitialDeployment.roster ||
        !testFieldInitialDeployment.overlaps || !anchorCounts || !anchorX || !anchorY || !objectCounts || !cover || !bodies || !present) {
        reason="out_of_memory";goto done;
    }
    memcpy(s->grid,GAME_PTR(RVA_BATTLE_GRID),cells*BATTLE_CELL_SIZE);
    memcpy(testFieldInitialDeployment.units,GAME_PTR(RVA_UNITS),testFieldInitialDeployment.unitsBytes);
    memcpy(testFieldInitialDeployment.commanders,GAME_PTR(RVA_COMMANDERS),testFieldInitialDeployment.commandersBytes);
    memcpy(testFieldInitialDeployment.monsters,tableAddress(TABLE_MONSTER),testFieldInitialDeployment.monstersBytes);
    memcpy(testFieldInitialDeployment.generations,testFieldLife.generations,testFieldInitialDeployment.generationsBytes);
    memcpy(testFieldInitialDeployment.actors,testFieldLife.actors,testFieldInitialDeployment.actorsBytes);
    testFieldInitialDeployment.captured=1;
    for(size_t cell=0;cell<cells;cell++)cover[cell]=-1;
    for(size_t unit=0;unit<capacity;unit++)anchorX[unit]=anchorY[unit]=-1;
    for(int unit=0;unit<UNIT_TABLE_MAX;unit++) {
        size_t at=(size_t)unit*UNIT_SIZE;
        TestFieldMadeUnit *origin=&testFieldLife.actors[unit];
        if(G16(RVA_UNIT_HP+at)<=0) {
            if(origin->generation && (origin->actor || (origin->source == TEST_FIELD_GATE))) {reason="initial_dead_roster_actor";goto done;}
            continue;
        }
        int owner=G16(RVA_UNIT_PLAYER+at),onField=G32(RVA_UNIT_X+at)==s->x && G32(RVA_UNIT_X+at+4)==s->y;
        if(onField && (!isTestFieldUnit(unit) || origin->source<=0)){reason="initial_unexplained_on_field_unit";goto done;}
        if((origin->generation && (origin->actor || (origin->source == TEST_FIELD_GATE))) || (onField && (owner==0 || owner==TEST_BATTLE_ENEMY_PLAYER))) {
            if(unit>s->last || !isTestFieldUnit(unit) || (!origin->actor && !(origin->source == TEST_FIELD_GATE))) {reason="initial_unexplained_actor";goto done;}
        }
        if(!origin->generation || !isTestFieldUnit(unit))continue;
        if(unit>s->last || origin->source<=0 || !testFieldActorBodyRead(unit,&bodies[unit])){reason="initial_invalid_owned_body";goto done;}
        present[unit]=1;
        TestFieldActorBody *b=&bodies[unit];
        if(origin->actor || (origin->source == TEST_FIELD_GATE)) {
            if(b->x!=s->x || b->y!=s->y || (b->owner!=0 && b->owner!=TEST_BATTLE_ENEMY_PLAYER) ||
                b->monster!=origin->created.monster || b->owner!=origin->created.owner ||
                (origin->actor && ((origin->source == TEST_FIELD_GATE) || origin->source!=1)) ||
                ((origin->source == TEST_FIELD_GATE) && (origin->actor || origin->source!=4 || b->monster!=origin->created.monster || b->owner!=origin->created.owner))) {
                reason="initial_provenance_unverified";goto done;
            }
        }
    }
    for(int x=0;x<s->width;x++)for(int y=0;y<BATTLE_ROWS;y++) {
        const unsigned char *raw=s->grid+((size_t)x*BATTLE_ROWS+y)*BATTLE_CELL_SIZE;
        int unit,object;memcpy(&unit,raw,4);memcpy(&object,raw+4,4);
        if(unit < -1 || object < -1 || unit>s->last || object>s->last){reason="initial_invalid_grid_handle";goto done;}
        if(unit>=0) {
            if(!present[unit] || (!bodies[unit].actor && !bodies[unit].structure) || ++anchorCounts[unit]!=1){reason="initial_ambiguous_actor_anchor";goto done;}
            anchorX[unit]=x;anchorY[unit]=y;
        }
        if(object>=0) {
            TestFieldActorBody *b=&bodies[object];
            if(!present[object] || b->actor || b->structure || (b->source!=2 && b->source!=3) || b->owner!=31 ||
                b->x!=s->x || b->y!=s->y || ++objectCounts[object]!=1){reason="initial_unverified_object";goto done;}
        }
    }
    for(int unit=0;unit<=s->last;unit++)if(present[unit] && (bodies[unit].actor || bodies[unit].structure)) {
        if(anchorCounts[unit]!=1){reason="initial_missing_actor_anchor";goto done;}
        if(!testFieldInitialDeploymentContextSame(script) || !testFieldActorBodySame(&bodies[unit])){reason="initial_source_changed";goto done;}
        int w=GAME_FN(int (*)(int),RVA_UNIT_BATTLE_SIZE_2)(unit);
        if(!testFieldInitialDeploymentContextSame(script) || !testFieldActorBodySame(&bodies[unit])){reason="initial_source_changed";goto done;}
        int h=GAME_FN(int (*)(int),RVA_UNIT_BATTLE_SIZE)(unit);
        if(!testFieldInitialDeploymentContextSame(script) || !testFieldActorBodySame(&bodies[unit])){reason="initial_source_changed";goto done;}
        int x=anchorX[unit],y=anchorY[unit];
        if(w<1 || w>battleTokenReach() || h<1 || h>battleTokenReach() || x+w>s->width || y+h>BATTLE_ROWS){reason="initial_footprint_outside";goto done;}
        if(bodies[unit].actor) {
            TestFieldInitialActor *r=&testFieldInitialDeployment.roster[testFieldInitialDeployment.actorCount++];
            *r=(TestFieldInitialActor){.unit=unit,.monster=bodies[unit].monster,.owner=bodies[unit].owner,
                .generation=bodies[unit].generation,.x=x,.y=y,.width=w,.height=h};
        }
        for(int dx=0;dx<w;dx++)for(int dy=0;dy<h;dy++) {
            size_t cell=(size_t)(x+dx)*BATTLE_ROWS+y+dy;
            int other=cover[cell];
            if(other>=0 && (!bodies[unit].actor || !bodies[other].actor)){reason="initial_structure_overlap";goto done;}
            if(other<0)cover[cell]=unit;
        }
    }
    for(int i=0;i<testFieldInitialDeployment.actorCount;i++) {
        TestFieldInitialActor *r=&testFieldInitialDeployment.roster[i];
        if(!testFieldInitialDeploymentContextSame(script) || !testFieldActorBodySame(&bodies[r->unit])){reason="initial_source_changed";goto done;}
        int stand0=GAME_FN(int (*)(int,int,int,int),RVA_BATTLE_CAN_STAND)(r->unit,r->x,r->y,0);
        if(!testFieldInitialDeploymentContextSame(script) || !testFieldActorBodySame(&bodies[r->unit])){reason="initial_source_changed";goto done;}
        if(!stand0){reason="initial_native_stand_rejected";goto done;}
        int stand4=GAME_FN(int (*)(int,int,int,int),RVA_BATTLE_CAN_STAND)(r->unit,r->x,r->y,4);
        if(!testFieldInitialDeploymentContextSame(script) || !testFieldActorBodySame(&bodies[r->unit])){reason="initial_source_changed";goto done;}
        if(!stand4){reason="initial_native_terrain_rejected";goto done;}
    }
    for(int x=0;x<s->width;x++)for(int y=0;y<BATTLE_ROWS;y++) {
        if(!testFieldInitialDeploymentContextSame(script)){reason="initial_source_changed";goto done;}
        int hit=GAME_FN(int (*)(int,int),RVA_BATTLE_UNIT_AT)(x,y),expected=cover[(size_t)x*BATTLE_ROWS+y];
        if(!testFieldInitialDeploymentContextSame(script)){reason="initial_source_changed";goto done;}
        if(hit==expected)continue;
        int i=testFieldInitialDeploymentActorIndex(hit);
        if(expected<0 || i<0 || !bodies[expected].actor){reason="initial_unexplained_hit";goto done;}
        TestFieldInitialActor *r=&testFieldInitialDeployment.roster[i];
        if(x<r->x || x>=r->x+r->width || y<r->y || y>=r->y+r->height){reason="initial_unexplained_hit";goto done;}
    }
    for(int i=0;i<testFieldInitialDeployment.actorCount;i++) {
        TestFieldInitialActor *r=&testFieldInitialDeployment.roster[i];
        for(int x=r->x;x<r->x+r->width;x++)for(int y=r->y;y<r->y+r->height;y++) {
            if(!testFieldInitialDeploymentContextSame(script)){reason="initial_source_changed";goto done;}
            int hit=GAME_FN(int (*)(int,int),RVA_BATTLE_UNIT_AT)(x,y);
            if(!testFieldInitialDeploymentContextSame(script)){reason="initial_source_changed";goto done;}
            if(hit==r->unit)continue;
            int other=testFieldInitialDeploymentActorIndex(hit);
            if(other<0){reason="initial_unexplained_hit";goto done;}
            TestFieldInitialActor *o=&testFieldInitialDeployment.roster[other];
            if(x<o->x || x>=o->x+o->width || y<o->y || y>=o->y+o->height ||
                testFieldInitialDeployment.overlapCount>=(int)(cells*9)){reason="initial_overlap_unverified";goto done;}
            testFieldInitialDeployment.overlaps[testFieldInitialDeployment.overlapCount++]=
                (TestFieldInitialOverlap){.unit=r->unit,.otherUnit=hit,.x=x,.y=y};
        }
    }
    if(!testFieldInitialDeploymentFresh(script)){reason="initial_source_changed";goto done;}
    testFieldInitialDeployment.completeRoster=1;
    testFieldInitialDeployment.sourceHealthy=testFieldInitialDeployment.overlapCount==0;
    testFieldInitialDeployment.eligible=!testFieldInitialDeployment.sourceHealthy;
    if(++testFieldInitialDeployment.nextToken>LLONG_MAX || !testFieldInitialDeployment.nextToken) {
        reason="initial_receipt_exhausted";testFieldInitialDeployment.eligible=0;goto done;
    }
    testFieldInitialDeployment.token=testFieldInitialDeployment.nextToken;
    reason=testFieldInitialDeployment.sourceHealthy?"initial_repair_not_required":NULL;
done:
    testFieldDeploymentBusy = 0;
    testFieldInitialDeployment.reason=reason;
    if(reason && !testFieldInitialDeployment.sourceHealthy)testFieldInitialDeployment.eligible=0;
    free(anchorCounts);free(anchorX);free(anchorY);free(objectCounts);free(cover);free(bodies);free(present);
    writeLog("test_field_initial_deployment","mod=%s session=%llu captured=%d eligible=%d source_healthy=%d actors=%d overlaps=%d snapshot_bytes=%llu reason=%s",
        script->mod,testFieldLife.session,testFieldInitialDeployment.captured,testFieldInitialDeployment.eligible,
        testFieldInitialDeployment.sourceHealthy,testFieldInitialDeployment.actorCount,testFieldInitialDeployment.overlapCount,
        (unsigned long long)testFieldInitialDeployment.totalBytes,reason?reason:"none");
}

static int lTestFieldInitialDeployment(lua_State *L) {
    checkLuaMainThread(L,"coe5.testFieldInitialDeployment");LuaScript *script=checkLuaScript(L);
    const char *reason=testFieldPolicyProblem(script);
    int fresh=!reason && testFieldInitialDeploymentFresh(script);
    lua_createtable(L,0,12);setIntField(L,"version",1);
    setIntField(L,"session",(lua_Integer)testFieldInitialDeployment.context.session);
    if(fresh)setIntField(L,"token",(lua_Integer)testFieldInitialDeployment.token);
    setBoolField(L,"fresh",fresh);setBoolField(L,"eligible",fresh && testFieldInitialDeployment.eligible);
    setBoolField(L,"sourceHealthy",fresh && testFieldInitialDeployment.sourceHealthy);
    setBoolField(L,"completeRoster",fresh && testFieldInitialDeployment.completeRoster);
    setBoolField(L,"actorOverlapOnly",fresh && testFieldInitialDeployment.eligible && testFieldInitialDeployment.overlapCount>0);
    if(!reason)reason=testFieldInitialDeployment.consumed?"initial_receipt_consumed":testFieldInitialDeployment.reason;
    if(reason){lua_pushstring(L,reason);lua_setfield(L,-2,"reason");}
    lua_createtable(L,fresh?testFieldInitialDeployment.actorCount:0,0);
    if(fresh)for(int i=0;i<testFieldInitialDeployment.actorCount;i++) {
        TestFieldInitialActor *r=&testFieldInitialDeployment.roster[i];lua_createtable(L,0,13);
        setIntField(L,"unit",r->unit);setIntField(L,"generation",(lua_Integer)r->generation);
        setIntField(L,"monster",r->monster);setIntField(L,"owner",r->owner);
        setIntField(L,"x",r->x);setIntField(L,"y",r->y);setIntField(L,"width",r->width);setIntField(L,"height",r->height);
        lua_pushliteral(L,"actor");lua_setfield(L,-2,"kind");lua_pushliteral(L,"owned_actor_creation");lua_setfield(L,-2,"creationSource");
        setBoolField(L,"provenanceVerified",1);lua_rawseti(L,-2,i+1);
    }
    lua_setfield(L,-2,"actors");lua_createtable(L,fresh?testFieldInitialDeployment.overlapCount:0,0);
    if(fresh)for(int i=0;i<testFieldInitialDeployment.overlapCount;i++) {
        TestFieldInitialOverlap *o=&testFieldInitialDeployment.overlaps[i];lua_createtable(L,0,4);
        setIntField(L,"unit",o->unit);setIntField(L,"otherUnit",o->otherUnit);setIntField(L,"x",o->x);setIntField(L,"y",o->y);
        lua_rawseti(L,-2,i+1);
    }
    lua_setfield(L,-2,"overlaps");return 1;
}
