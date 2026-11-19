#pragma once

typedef struct {
    int unit, x, y, width, height, fromX, fromY, object, objectX, objectY;
    unsigned long long generation;
} TestFieldDeploymentRow;

typedef struct {
    unsigned long long session, serial, created;
    int width, last, setup, activePlayer, x, y, sea, siege, tower, plane;
    unsigned char square[SQUARE_SIZE];
    unsigned char *grid;
    int *anchors, *bodyIds, *unitAnchors, *objectAnchors, *byUnit;
    unsigned char *present;
    TestFieldActorBody *bodies;
    int bodyCount;
} TestFieldDeploymentSnapshot;

static int testFieldDeploymentInteger(lua_State *L, int table, const char *key, lua_Integer *value, int optional) {
    lua_pushstring(L,key);lua_rawget(L,table);
    int valid=optional && lua_isnil(L,-1);
    if(valid)*value=0;
    else {valid=readTestFieldInteger(L,-1,value);if(optional && valid && *value<=0)valid=0;}
    lua_pop(L,1);return valid;
}

static int testFieldDeploymentDense(lua_State *L, int table, size_t count) {
    size_t seen=0;lua_pushnil(L);
    while(lua_next(L,table)) {
        lua_Integer key=0;
        int valid=readTestFieldInteger(L,-2,&key) && key>=1 && (unsigned long long)key<=count;
        lua_pop(L,1);
        if(!valid){lua_pop(L,1);return 0;}
        seen++;
    }
    return seen==count;
}

static const char *testFieldPolicyProblem(const LuaScript *script) {
    const char *reason = testFieldReadProblem(script);
    if (reason) return reason;
    if (!testField.editor || !testField.environmentBorrowed) return "unverified_context";
    if (testField.fighting) return "in_fight";
    if (testFieldFinalViewActive()) return "final_read_only";
    if (!testFieldLife.active || testFieldLife.retiring || testFieldLife.failed || testFieldLife.pending)
        return "cleanup_unverified";
    if (GetCurrentThreadId() != testFieldLife.thread) return "wrong_thread";
    if (testFieldLife.depth || testFieldLife.overrideDepth) return "creation_in_progress";
    return NULL;
}

static int isTestFieldStructure(int unit) {
    return isTestFieldUnit(unit) && testFieldLife.actors[unit].source == TEST_FIELD_GATE;
}

static int testFieldDeploymentContextSame(const LuaScript *script,const TestFieldDeploymentSnapshot *s) {
    return !testFieldPolicyProblem(script) && !battleDrawing &&
        testFieldLife.session==s->session && testFieldLife.serial==s->serial && testFieldLife.createdCount==s->created &&
        G32(RVA_BATTLE_WIDTH)==s->width && G32(RVA_UNIT_COUNT)==s->last && testField.setupComplete==s->setup &&
        G32(RVA_ACTIVE_PLAYER)==s->activePlayer && testField.x==s->x && testField.y==s->y &&
        G32(RVA_BATTLE_X)==s->x && G32(RVA_BATTLE_Y)==s->y &&
        G32(RVA_BATTLE_AT_SEA)==s->sea && G32(RVA_BATTLE_IS_SIEGE)==s->siege &&
        G32(RVA_BATTLE_IS_TOWER)==s->tower && G32(RVA_BATTLE_PLANE_3_OR_4)==s->plane &&
        !memcmp(testFieldSquare(),s->square,SQUARE_SIZE);
}

static int testFieldDeploymentGridSame(const TestFieldDeploymentSnapshot *s,int planned) {
    for(int x=0;x<s->width;x++)for(int y=0;y<BATTLE_ROWS;y++) {
        size_t cell=(size_t)x*BATTLE_ROWS+y;
        const unsigned char *current=battleCell(x,y),*old=s->grid+cell*BATTLE_CELL_SIZE;
        int unit;memcpy(&unit,current,sizeof unit);
        if(unit!=(planned?s->anchors[cell]:*(const int *)old) ||
            memcmp(current+sizeof unit,old+sizeof unit,BATTLE_CELL_SIZE-sizeof unit))return 0;
    }
    return 1;
}

static int testFieldDeploymentBodiesSame(const TestFieldDeploymentSnapshot *s) {
    for(int i=0;i<s->bodyCount;i++)if(!testFieldActorBodySame(&s->bodies[s->bodyIds[i]]))return 0;
    return 1;
}

static int testFieldDeploymentRemember(TestFieldDeploymentSnapshot *s,int unit) {
    if(unit<0 || unit>s->last || unit>=UNIT_TABLE_MAX)return 0;
    if(s->present[unit])return 1;
    if(!testFieldActorBodyRead(unit,&s->bodies[unit]) || !s->bodies[unit].owned || s->bodies[unit].source<=0)return 0;
    s->present[unit]=1;s->bodyIds[s->bodyCount++]=unit;return 1;
}

#include "lua_test_field_initial_deployment.h"

static int testFieldDeploymentGuard(const LuaScript *script,const TestFieldDeploymentSnapshot *s,int initialRepair) {
    return testFieldDeploymentContextSame(script,s) &&
        (!initialRepair || testFieldInitialDeploymentContextSame(script));
}

static int lSetTestFieldDeployment(lua_State *L) {
    const char *call="coe5.setTestFieldDeployment";checkLuaMainThread(L,call);
    LuaScript *script=checkLuaScript(L);
    if (testFieldDeploymentBusy) return refuseTestFieldCall(L, call, "field_transaction_active", 0, "field=unchanged");
    const char *reason=testFieldPolicyProblem(script),*phase="arguments",*kind="none";
    testFieldDeploymentBusy = 1;
    TestFieldDeploymentSnapshot snap={0};TestFieldDeploymentRow *rows=NULL;
    int *cover=NULL,*objects=NULL,*widths=NULL,*heights=NULL;
    int count=0,blockedUnit=-1,blockedObject=-1,blockedX=-1,blockedY=-1,rollbackVerified=1,wrote=0;
    int initialRepair=0,sourceHealthy=-1,byteRollbackVerified=1;
    unsigned long long initialSession=0;
    int standMode=-1,stand0=-1,stand4=-1,anchorsRestored=0;const char *rollbackResult="not_needed";
    if(!reason && battleDrawing)reason="draw_read_only";
    if(!reason && (!testField.setupComplete || testField.activePlayer>=0))reason="not_main_menu";
    if(!reason)reason=testFieldEnvironmentProblem();
    if(!reason)reason=testFieldPolicyProblem(script);
    if(reason)goto done;
    if(!lua_isnoneornil(L,2)) {
        lua_Integer token=0;
        if(!lua_istable(L,2) || !testFieldDeploymentInteger(L,2,"initialReceipt",&token,0) || token<=0) {reason="bad_initial_receipt";goto done;}
        lua_pushnil(L);
        while(lua_next(L,2)) {
            const char *key=lua_type(L,-2)==LUA_TSTRING?lua_tostring(L,-2):NULL;
            int valid=key && !strcmp(key,"initialReceipt");lua_pop(L,1);
            if(!valid){lua_pop(L,1);reason="bad_initial_receipt";goto done;}
        }
        if((unsigned long long)token!=testFieldInitialDeployment.token || !testFieldInitialDeploymentFresh(script) ||
            !testFieldInitialDeployment.eligible || !testFieldInitialDeployment.completeRoster || testFieldInitialDeployment.sourceHealthy) {
            reason="initial_receipt_unavailable";goto done;
        }
        initialRepair=1;sourceHealthy=0;initialSession=testFieldInitialDeployment.context.session;
        testFieldInitialDeployment.consumed=1;
    }
    if(!lua_istable(L,1)){reason="bad_deployment";goto done;}
    size_t length=lua_rawlen(L,1);
    if(length>UNIT_TABLE_MAX || !testFieldDeploymentDense(L,1,length)){reason="bad_deployment";goto done;}
    count=(int)length;rows=calloc(length?length:1,sizeof *rows);
    if(!rows){reason="out_of_memory";goto done;}
    for(int i=0;i<count;i++) {
        lua_rawgeti(L,1,i+1);int at=lua_gettop(L);lua_Integer unit=0,x=0,y=0,generation=0;
        int valid=lua_istable(L,at) && testFieldDeploymentInteger(L,at,"unit",&unit,0) &&
            testFieldDeploymentInteger(L,at,"x",&x,0) && testFieldDeploymentInteger(L,at,"y",&y,0) &&
            testFieldDeploymentInteger(L,at,"generation",&generation,1);
        if(valid) {
            lua_pushnil(L);
            while(lua_next(L,at)) {
                const char *key=lua_type(L,-2)==LUA_TSTRING?lua_tostring(L,-2):NULL;
                int known=key && (!strcmp(key,"unit") || !strcmp(key,"x") || !strcmp(key,"y") || !strcmp(key,"generation"));
                lua_pop(L,1);if(!known){lua_pop(L,1);valid=0;break;}
            }
        }
        lua_pop(L,1);
        if(!valid || unit<0 || unit>=UNIT_TABLE_MAX || x<0 || x>=BATTLE_COLUMNS_MAX || y<0 || y>=BATTLE_ROWS || generation<0) {
            reason="bad_deployment_row";goto done;
        }
        rows[i]=(TestFieldDeploymentRow){.unit=(int)unit,.x=(int)x,.y=(int)y,.generation=(unsigned long long)generation,.fromX=-1,.fromY=-1,.object=-1,.objectX=-1,.objectY=-1};
    }
    phase="snapshot";
    if((reason=testFieldPolicyProblem(script)))goto done;
    snap.session=testFieldLife.session;snap.serial=testFieldLife.serial;snap.created=testFieldLife.createdCount;
    snap.width=G32(RVA_BATTLE_WIDTH);snap.last=G32(RVA_UNIT_COUNT);snap.setup=testField.setupComplete;
    snap.activePlayer=G32(RVA_ACTIVE_PLAYER);snap.x=testField.x;snap.y=testField.y;
    snap.sea=G32(RVA_BATTLE_AT_SEA);snap.siege=G32(RVA_BATTLE_IS_SIEGE);
    snap.tower=G32(RVA_BATTLE_IS_TOWER);snap.plane=G32(RVA_BATTLE_PLANE_3_OR_4);
    memcpy(snap.square,testFieldSquare(),SQUARE_SIZE);
    if(snap.width<1 || snap.width>BATTLE_COLUMNS_MAX || snap.last < -1 || snap.last>=UNIT_TABLE_MAX || snap.activePlayer!=0) {
        reason="bad_field";goto done;
    }
    size_t cells=(size_t)snap.width*BATTLE_ROWS,capacity=(size_t)(snap.last+1);if(!capacity)capacity=1;
    snap.grid=malloc(cells*BATTLE_CELL_SIZE);snap.anchors=malloc(cells*sizeof *snap.anchors);
    snap.bodyIds=malloc(capacity*sizeof *snap.bodyIds);snap.bodies=calloc(capacity,sizeof *snap.bodies);snap.present=calloc(capacity,1);
    snap.byUnit=calloc(capacity,sizeof *snap.byUnit);snap.unitAnchors=calloc(capacity,sizeof *snap.unitAnchors);snap.objectAnchors=calloc(capacity,sizeof *snap.objectAnchors);
    widths=calloc(capacity,sizeof *widths);heights=calloc(capacity,sizeof *heights);
    cover=malloc(cells*sizeof *cover);objects=malloc(cells*sizeof *objects);
    if(!snap.grid || !snap.anchors || !snap.bodyIds || !snap.bodies || !snap.present || !snap.byUnit || !snap.unitAnchors || !snap.objectAnchors || !cover || !objects || !widths || !heights) {
        reason="out_of_memory";goto done;
    }
    for(size_t i=0;i<cells;i++)cover[i]=objects[i]=-1;
    for(int i=0;i<count;i++) {
        int unit=rows[i].unit;blockedUnit=unit;
        if(unit>snap.last || !isTestFieldActor(unit) || !testFieldDeploymentRemember(&snap,unit)){reason="not_actor";goto done;}
        if(snap.byUnit[unit]){reason="duplicate_unit";goto done;}
        snap.byUnit[unit]=i+1;
        TestFieldActorBody *b=&snap.bodies[unit];
        if(b->structure || !b->actor || b->source!=1 || (b->owner!=0 && b->owner!=TEST_BATTLE_ENEMY_PLAYER) || b->x!=snap.x || b->y!=snap.y) {reason="not_on_field";goto done;}
        if((initialRepair && !rows[i].generation) || (rows[i].generation && rows[i].generation!=b->generation)){reason="generation_changed";goto done;}
        rows[i].generation=b->generation;
    }
    for(int unit=0;unit<UNIT_TABLE_MAX;unit++) {
        size_t at=(size_t)unit*UNIT_SIZE;
        if(G16(RVA_UNIT_HP+at)<=0)continue;
        TestFieldMadeUnit *a=&testFieldLife.actors[unit];
        int owner=G16(RVA_UNIT_PLAYER+at),onField=G32(RVA_UNIT_X+at)==snap.x && G32(RVA_UNIT_X+at+4)==snap.y;
        if((a->generation && a->actor) || (onField && (owner==0 || owner==TEST_BATTLE_ENEMY_PLAYER) && !isTestFieldStructure(unit))) {
            if(unit>snap.last || !isTestFieldActor(unit) || !snap.byUnit[unit]){blockedUnit=unit;reason="incomplete_actor_roster";goto done;}
        }
        if(a->generation && isTestFieldUnit(unit) && !testFieldDeploymentRemember(&snap,unit)){blockedUnit=unit;reason="invalid_owned_body";goto done;}
    }
    for(int x=0;x<snap.width;x++)for(int y=0;y<BATTLE_ROWS;y++) {
        size_t cell=(size_t)x*BATTLE_ROWS+y;unsigned char *raw=snap.grid+cell*BATTLE_CELL_SIZE;
        memcpy(raw,battleCell(x,y),BATTLE_CELL_SIZE);
        int unit,object;memcpy(&unit,raw,4);memcpy(&object,raw+4,4);snap.anchors[cell]=unit;
        if(unit < -1 || object < -1){blockedX=x;blockedY=y;reason="invalid_grid_handle";goto done;}
        if(unit>=0) {
            blockedUnit=unit;blockedX=x;blockedY=y;
            if(!testFieldDeploymentRemember(&snap,unit) || (!snap.byUnit[unit] && !isTestFieldStructure(unit))){reason="unexplained_actor";goto done;}
            if(!snap.byUnit[unit]) {
                TestFieldActorBody *body=&snap.bodies[unit];TestFieldMadeUnit *origin=&testFieldLife.actors[unit];
                if(!body->structure || body->actor || body->source!=4 || body->monster!=origin->created.monster ||
                    body->owner!=origin->created.owner || (body->owner!=0 && body->owner!=TEST_BATTLE_ENEMY_PLAYER) ||
                    body->x!=snap.x || body->y!=snap.y){reason="unverified_structure";goto done;}
            }
            if(++snap.unitAnchors[unit]!=1){reason="ambiguous_anchor";goto done;}
            if(snap.byUnit[unit]){TestFieldDeploymentRow *row=&rows[snap.byUnit[unit]-1];row->fromX=x;row->fromY=y;snap.anchors[cell]=-1;}
        }
        if(object>=0) {
            blockedObject=object;blockedX=x;blockedY=y;
            if(!testFieldDeploymentRemember(&snap,object)){reason="unverified_object";goto done;}
            TestFieldActorBody *b=&snap.bodies[object];
            if(b->actor || b->structure || (b->source!=2 && b->source!=3) || b->owner!=31 || b->x!=snap.x || b->y!=snap.y) {reason="unverified_object";goto done;}
            if(++snap.objectAnchors[object]!=1){reason="ambiguous_object_anchor";goto done;}
        }
    }
    phase="preflight";
    for(int n=0;n<snap.bodyCount;n++) {
        int unit=snap.bodyIds[n];
        if(!snap.unitAnchors[unit] && !snap.objectAnchors[unit])continue;
        if(!testFieldDeploymentGuard(script,&snap,initialRepair) || !testFieldActorBodySame(&snap.bodies[unit])){reason="context_changed";rollbackVerified=0;goto done;}
        widths[unit]=GAME_FN(int (*)(int),RVA_UNIT_BATTLE_SIZE_2)(unit);
        if(!testFieldDeploymentGuard(script,&snap,initialRepair) || !testFieldActorBodySame(&snap.bodies[unit])){reason="context_changed";rollbackVerified=0;goto done;}
        heights[unit]=GAME_FN(int (*)(int),RVA_UNIT_BATTLE_SIZE)(unit);
        if(!testFieldDeploymentGuard(script,&snap,initialRepair) || !testFieldActorBodySame(&snap.bodies[unit])){reason="context_changed";rollbackVerified=0;goto done;}
        if(widths[unit]<1 || widths[unit]>battleTokenReach() || heights[unit]<1 || heights[unit]>battleTokenReach()){blockedUnit=unit;reason="bad_footprint";goto done;}
    }
    if(!testFieldDeploymentGridSame(&snap,0) || !testFieldDeploymentBodiesSame(&snap)){reason="context_changed";rollbackVerified=0;goto done;}
    for(int x=0;x<snap.width;x++)for(int y=0;y<BATTLE_ROWS;y++) {
        size_t cell=(size_t)x*BATTLE_ROWS+y;int handles[2];memcpy(handles,snap.grid+cell*BATTLE_CELL_SIZE,sizeof handles);
        for(int layer=0;layer<2;layer++) {
            int unit=handles[layer];if(unit<0)continue;
            int w=widths[unit],h=heights[unit];
            if(!layer && (x+w>snap.width || y+h>BATTLE_ROWS)){blockedUnit=unit;blockedX=x;blockedY=y;reason="current_footprint_outside";goto done;}
            for(int dx=0;dx<w;dx++)for(int dy=0;dy<h;dy++) {
                if(x+dx>=snap.width || y+dy>=BATTLE_ROWS)continue;
                size_t target=(size_t)(x+dx)*BATTLE_ROWS+y+dy;int *map=layer?objects:cover;
                if(!layer && map[target]>=0 && map[target]!=unit) {
                    if(!initialRepair || !snap.byUnit[unit] || !snap.byUnit[map[target]]) {
                        blockedUnit=unit;blockedX=x+dx;blockedY=y+dy;reason="current_actor_overlap";goto done;
                    }
                }
                if(!layer || map[target]<0)map[target]=unit;
            }
        }
    }
    for(size_t cell=0;cell<cells;cell++)if(cover[cell]>=0 && snap.byUnit[cover[cell]])cover[cell]=-1;
    for(int i=0;i<count;i++) {
        TestFieldDeploymentRow *row=&rows[i];blockedUnit=row->unit;blockedObject=-1;kind="actor";
        if(snap.unitAnchors[row->unit]!=1 || row->fromX<0){reason="missing_anchor";goto done;}
        row->width=widths[row->unit];row->height=heights[row->unit];
        if(row->x+row->width>snap.width || row->y+row->height>BATTLE_ROWS){blockedX=row->x;blockedY=row->y;kind="out_of_bounds";reason="off_field";goto done;}
        for(int dx=0;dx<row->width;dx++)for(int dy=0;dy<row->height;dy++) {
            int x=row->x+dx,y=row->y+dy;size_t cell=(size_t)x*BATTLE_ROWS+y;
            blockedX=x;blockedY=y;
            if(cover[cell]>=0){blockedObject=cover[cell];kind=snap.byUnit[cover[cell]]?"target_overlap":"structure";reason="blocked";goto done;}
            int object=objects[cell];
            if(object>=0 && row->object<0){row->object=object;row->objectX=x;row->objectY=y;}
            cover[cell]=row->unit;
        }
        snap.anchors[(size_t)row->x*BATTLE_ROWS+row->y]=row->unit;
    }
    if(!testFieldDeploymentGuard(script,&snap,initialRepair) || !testFieldDeploymentGridSame(&snap,0) || !testFieldDeploymentBodiesSame(&snap)){reason="context_changed";rollbackVerified=0;goto done;}
    if(initialRepair && !testFieldInitialDeploymentSurroundingSame(script)){reason="context_changed";rollbackVerified=0;goto done;}
    blockedUnit=blockedObject=blockedX=blockedY=-1;kind="none";phase="anchor_stage";wrote=1;
    for(int x=0;x<snap.width;x++)for(int y=0;y<BATTLE_ROWS;y++)memcpy(battleCell(x,y),&snap.anchors[(size_t)x*BATTLE_ROWS+y],sizeof(int));
    if(!testFieldDeploymentGuard(script,&snap,initialRepair) || !testFieldDeploymentGridSame(&snap,1) || !testFieldDeploymentBodiesSame(&snap)){reason="deployment_verification_failed";goto done;}
    phase="native_verification";
    for(int i=0;i<count;i++) {
        TestFieldDeploymentRow *row=&rows[i];blockedUnit=row->unit;blockedX=row->x;blockedY=row->y;
        blockedObject=row->object;kind=row->object>=0?"terrain":"native_rules";stand4=-1;standMode=0;
        if(!testFieldDeploymentGuard(script,&snap,initialRepair) || !testFieldDeploymentGridSame(&snap,1) || !testFieldDeploymentBodiesSame(&snap)){reason="context_changed";goto done;}
        stand0=GAME_FN(int (*)(int,int,int,int),RVA_BATTLE_CAN_STAND)(row->unit,row->x,row->y,0);
        if(!testFieldDeploymentGuard(script,&snap,initialRepair) || !testFieldDeploymentGridSame(&snap,1) || !testFieldDeploymentBodiesSame(&snap)){reason="context_changed";goto done;}
        if(!stand0){reason="native_stand_rejected";goto done;}
        if(row->object>=0) {
            standMode=4;
            stand4=GAME_FN(int (*)(int,int,int,int),RVA_BATTLE_CAN_STAND)(row->unit,row->x,row->y,4);
            if(!testFieldDeploymentGuard(script,&snap,initialRepair) || !testFieldDeploymentGridSame(&snap,1) || !testFieldDeploymentBodiesSame(&snap)){reason="context_changed";goto done;}
            if(!stand4){reason="native_terrain_rejected";goto done;}
        }
    }
    if(!testFieldDeploymentGuard(script,&snap,initialRepair) || !testFieldDeploymentGridSame(&snap,1) || !testFieldDeploymentBodiesSame(&snap)){reason="deployment_verification_failed";goto done;}
    if(initialRepair) {
        phase="hit_verification";
        for(int x=0;x<snap.width;x++)for(int y=0;y<BATTLE_ROWS;y++) {
            blockedX=x;blockedY=y;blockedUnit=cover[(size_t)x*BATTLE_ROWS+y];
            if(!testFieldDeploymentGuard(script,&snap,initialRepair) || !testFieldDeploymentGridSame(&snap,1) || !testFieldDeploymentBodiesSame(&snap)){reason="context_changed";goto done;}
            int hit=GAME_FN(int (*)(int,int),RVA_BATTLE_UNIT_AT)(x,y);
            if(!testFieldDeploymentGuard(script,&snap,initialRepair) || !testFieldDeploymentGridSame(&snap,1) || !testFieldDeploymentBodiesSame(&snap)){reason="context_changed";goto done;}
            if(hit!=blockedUnit){reason="native_hit_verification_failed";goto done;}
        }
    }
    if(initialRepair && !testFieldInitialDeploymentSurroundingSame(script)){reason="context_changed";goto done;}
    blockedUnit=blockedObject=blockedX=blockedY=-1;kind="none";standMode=stand0=stand4=-1;phase="committed";
done:
    if(reason && wrote) {
        rollbackVerified=0;
        if(testFieldDeploymentGuard(script,&snap,initialRepair)) {
            for(int x=0;x<snap.width;x++)for(int y=0;y<BATTLE_ROWS;y++)memcpy(battleCell(x,y),snap.grid+((size_t)x*BATTLE_ROWS+y)*BATTLE_CELL_SIZE,sizeof(int));
            anchorsRestored=testFieldDeploymentGuard(script,&snap,initialRepair) && testFieldDeploymentGridSame(&snap,0);
            rollbackVerified=anchorsRestored && strcmp(reason,"context_changed") && testFieldDeploymentBodiesSame(&snap);
        }
        rollbackResult=rollbackVerified?"verified":"failed";
        if(!rollbackVerified && testFieldLife.session==snap.session)testFieldLife.failed=testFieldLife.pending=1;
    }
    byteRollbackVerified=rollbackVerified;
    if(initialRepair && reason) {
        byteRollbackVerified=testFieldInitialDeploymentSurroundingSame(script) &&
            !memcmp(testFieldInitialDeployment.context.grid,GAME_PTR(RVA_BATTLE_GRID),
                (size_t)testFieldInitialDeployment.context.width*BATTLE_ROWS*BATTLE_CELL_SIZE);
        rollbackVerified=0;
        rollbackResult=byteRollbackVerified?(wrote?"invalid_source_restored":"invalid_source_unchanged"):"failed";
        if(testFieldLife.session==initialSession) {
            testFieldLife.pending=1;
            if(!byteRollbackVerified)testFieldLife.failed=1;
        }
    }
    if(reason)refuseLuaCall(script->mod,call,reason,"phase=%s unit=%d x=%d y=%d object=%d rollback_verified=%d",phase,blockedUnit,blockedX,blockedY,blockedObject,rollbackVerified);
    else writeLog("test_field_deployment","mod=%s session=%llu units=%d exact=1 actor_bytes_unchanged=1",script->mod,testFieldLife.session,count);
    lua_pushboolean(L,reason==NULL);if(reason)lua_pushstring(L,reason);else lua_pushnil(L);
    lua_createtable(L,0,16);setIntField(L,"version",1);setIntField(L,"units",count);
    setBoolField(L,"rollbackVerified",rollbackVerified);setBoolField(L,"wroteAnchors",wrote);
    setBoolField(L,"byteRollbackVerified",byteRollbackVerified);
    setBoolField(L,"initialReceiptUsed",initialRepair);
    if(initialRepair)setIntField(L,"initialSession",(lua_Integer)initialSession);
    if(sourceHealthy>=0)setBoolField(L,"sourceHealthy",sourceHealthy);
    setBoolField(L,"finalHealthy",reason==NULL);
    setBoolField(L,"anchorsRestored",anchorsRestored);
    lua_pushstring(L,rollbackResult);lua_setfield(L,-2,"rollbackResult");
    if(standMode>=0)setIntField(L,"passabilityMode",standMode);
    if(stand0>=0)setIntField(L,"nativeStand0",stand0);
    if(stand4>=0)setIntField(L,"nativeStand4",stand4);
    lua_pushstring(L,phase);lua_setfield(L,-2,"phase");lua_pushstring(L,kind);lua_setfield(L,-2,"blockerKind");
    setIntField(L,"unit",blockedUnit);setIntField(L,"x",blockedX);setIntField(L,"y",blockedY);setIntField(L,"object",blockedObject);
    if(blockedObject>=0 && !strcmp(kind,"terrain")) {
        lua_pushliteral(L,"First intersecting verified object only; native passability covers the complete actor footprint and does not identify a sole object/cell cause.");
        lua_setfield(L,-2,"objectScope");
    }
    if(blockedObject>=0 && blockedObject<=snap.last && snap.present && snap.present[blockedObject]) {
        TestFieldActorBody *b=&snap.bodies[blockedObject];setIntField(L,"objectGeneration",(lua_Integer)b->generation);
        setIntField(L,"objectMonster",b->monster);setIntField(L,"objectOwner",b->owner);
        TestFieldMadeUnit origin={.source=b->source};lua_pushstring(L,testFieldCreationSource(origin.source));lua_setfield(L,-2,"objectSource");
    }
    free(rows);free(snap.grid);free(snap.anchors);free(snap.bodyIds);free(snap.bodies);free(snap.present);
    free(snap.byUnit);free(snap.unitAnchors);free(snap.objectAnchors);free(cover);free(objects);free(widths);free(heights);
    testFieldDeploymentBusy = 0;
    return 3;
}
