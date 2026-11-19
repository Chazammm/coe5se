#pragma once

#if TEST_FIELD_VERIFIED_WINDOWS_ABI
__attribute__((preserve_all,noinline)) static int testFieldMonsterInspectAllocate(int player) {
    if (!testFieldMonsterPreview.constructing)
        return GAME_FN(int (*)(int),RVA_ALLOC_TEMP_UNIT)(player);
    if (testFieldMonsterPreview.thread!=GetCurrentThreadId() || player!=testFieldMonsterPreview.player ||
        testFieldMonsterPreview.allocations || testFieldMonsterPreview.slot<0 || testFieldMonsterPreview.slot>=UNIT_TABLE_MAX) {
        testFieldMonsterPreview.violation=1;
        return -1;
    }
    testFieldMonsterPreview.allocations=1;
    return testFieldMonsterPreview.slot;
}

static TestFieldCall testFieldPreviewAllocator = {
    .callRva = RVA_NEW_UNIT_ALLOC_TEMP_CALL, .targetRva = RVA_ALLOC_TEMP_UNIT,
    .hook = (void *)testFieldMonsterInspectAllocate, .name = "test_field_preview_allocator"
};

static int testFieldPreviewCallMatches(unsigned int at, const unsigned char *stub, void *hook) {
    if (!stub) return 0;
    const unsigned char *call = GAME_PTR(at);
    int relative;
    memcpy(&relative, call + 1, sizeof relative);
    if (*call != 0xe8 || call + 5 + relative != stub) return 0;
    unsigned char expected[0x20];
    size_t size = buildRedirectCallStub(expected, (unsigned char *)stub, hook, NULL);
    return !memcmp(stub, expected, size);
}
#endif

static int testFieldMonsterInspectCallsMatch(void) {
#if TEST_FIELD_VERIFIED_WINDOWS_ABI
    const unsigned char *unique = uniqueGroups.newUnitCall ? uniqueGroupNewUnitStub : testFieldUniqueCall.stub;
    void *lookup = uniqueGroups.newUnitCall ? (void *)newUnitUniqueHook : testFieldUniqueCall.hook;
    return testFieldLife.installed == 1 && testFieldLife.tracking && gameNewUnit &&
        testFieldPreviewCallMatches(RVA_NEW_UNIT_UNIQUE_CALL, unique, lookup) &&
        testFieldPreviewCallMatches(testFieldPreviewAllocator.callRva, testFieldPreviewAllocator.stub, testFieldPreviewAllocator.hook);
#else
    return 0;
#endif
}

static const char *installTestFieldMonsterInspect(int *temporaryUnique) {
#if TEST_FIELD_VERIFIED_WINDOWS_ABI
    if (!testFieldLife.tracking || !gameNewUnit) return "lifecycle_unavailable";
    if (!uniqueGroups.newUnitCall && !testFieldUniqueCall.on) {
        if (!pointTestFieldCall(&testFieldUniqueCall)) return "preview_unique_hook_unavailable";
        *temporaryUnique = 1;
    }
    if (!pointTestFieldCall(&testFieldPreviewAllocator)) return "preview_allocator_hook_unavailable";
    return testFieldMonsterInspectCallsMatch() ? NULL : "preview_hooks_changed";
#else
    (void)temporaryUnique;
    return "unsupported_compiler_abi";
#endif
}

static int stopTestFieldMonsterInspect(int temporaryUnique) {
#if TEST_FIELD_VERIFIED_WINDOWS_ABI
    const unsigned char *allocatorCall = GAME_PTR(testFieldPreviewAllocator.callRva);
    const unsigned char *uniqueCall = GAME_PTR(testFieldUniqueCall.callRva);
    int allocatorWasOn = testFieldPreviewAllocator.on;
    int allocator = restoreTestFieldCall(&testFieldPreviewAllocator);
    int unique = !temporaryUnique || restoreTestFieldCall(&testFieldUniqueCall);
    int relative;
    if (allocatorWasOn) {
        memcpy(&relative, allocatorCall + 1, sizeof relative);
        allocator = allocator && allocatorCall[0] == 0xe8 &&
            allocatorCall + 5 + relative == GAME_PTR(testFieldPreviewAllocator.targetRva);
    }
    if (temporaryUnique) {
        memcpy(&relative, uniqueCall + 1, sizeof relative);
        unique = unique && uniqueCall[0] == 0xe8 && uniqueCall + 5 + relative == GAME_PTR(testFieldUniqueCall.targetRva);
    }
    return allocator && unique;
#else
    (void)temporaryUnique;
    return 1;
#endif
}

typedef struct {
    unsigned long long session,serial,created;
    int last,slot,lastCommander,width,monsterRecords,activePlayer,x,y,playerA,playerB,sea,boatsA,boatsB,depth,overrideDepth,multiple;
    int randomIndex,randomShift,randomXor,churn[PREVIEW_CHURN_INTS],showSpells,selectedCommander;
    unsigned crt;
    unsigned char randomTable[500*sizeof(int)],slotBefore[UNIT_SIZE];
    float view[5];
    TestFieldZoomState zoom;
    unsigned char battleCenter[sizeof testFieldBattleCenter];
    size_t unitsBytes,commandersBytes,gridBytes,monstersBytes,generationsBytes,actorsBytes;
    unsigned char *units,*commanders,*grid,*monsters,*generations,*actors;
} TestFieldMonsterInspectSnapshot;

static void testFieldMonsterInspectFree(TestFieldMonsterInspectSnapshot *s) {
    free(s->units);free(s->commanders);free(s->grid);free(s->monsters);free(s->generations);free(s->actors);
}

static int testFieldMonsterInspectContext(const LuaScript *script,const TestFieldMonsterInspectSnapshot *s) {
    return gameCodeMatches && testField.open && !testField.fighting && !testFieldFinalViewActive() && testField.activePlayer<0 &&
        !_stricmp(script->mod,testField.owner) && testFieldLife.active && !testFieldLife.retiring &&
        !testFieldLife.failed && !testFieldLife.pending && GetCurrentThreadId()==testFieldLife.thread &&
        testFieldLife.session==s->session && G32(RVA_ACTIVE_PLAYER)==s->activePlayer &&
        G32(RVA_BATTLE_X)==s->x && G32(RVA_BATTLE_Y)==s->y && G32(RVA_BATTLE_WIDTH)==s->width &&
        G32(RVA_BATTLE_PLAYER_A)==s->playerA && G32(RVA_BATTLE_PLAYER_B)==s->playerB &&
        G32(RVA_BATTLE_AT_SEA)==s->sea && G32(RVA_BATTLE_SHIPS_A)==s->boatsA && G32(RVA_BATTLE_SHIPS_D)==s->boatsB;
}

static int testFieldMonsterInspectExistingSame(const TestFieldMonsterInspectSnapshot *s) {
    return G32(RVA_LAST_COMMANDER)==s->lastCommander && testFieldLife.serial==s->serial && testFieldLife.createdCount==s->created &&
        testFieldLife.depth==s->depth && testFieldLife.overrideDepth==s->overrideDepth && testFieldLife.allowMultiple==s->multiple &&
        (!s->unitsBytes || !memcmp(s->units,GAME_PTR(RVA_UNITS),s->unitsBytes)) &&
        (!s->commandersBytes || !memcmp(s->commanders,GAME_PTR(RVA_COMMANDERS),s->commandersBytes)) &&
        !memcmp(s->grid,GAME_PTR(RVA_BATTLE_GRID),s->gridBytes) &&
        !memcmp(s->generations,testFieldLife.generations,s->generationsBytes) &&
        !memcmp(s->actors,testFieldLife.actors,s->actorsBytes) &&
        tableRecords(TABLE_MONSTER)==s->monsterRecords && !memcmp(s->monsters,tableAddress(TABLE_MONSTER),s->monstersBytes);
}

static int testFieldMonsterInspectRefusal(lua_State *L,const char *reason) {
    int unchanged=!testFieldLife.failed && !testFieldLife.pending;
    refuseLuaCall(runningLuaMod(), "coe5.inspectTestFieldMonster", reason, "preview_allocation=0");
    lua_pushboolean(L,0);lua_pushstring(L,reason);lua_createtable(L,0,8);
    setBoolField(L,"verified",unchanged);setBoolField(L,"cleanupVerified",unchanged);
    setBoolField(L,"instanceCreated",0);setBoolField(L,"changed",!unchanged);
    setBoolField(L,"quarantined",!unchanged);
    lua_pushliteral(L,"native_preview_instance");lua_setfield(L,-2,"scope");
    lua_pushliteral(L,"Refused before preview allocation; verification concerns this operation's lack of mutations, not a complete world snapshot");lua_setfield(L,-2,"verificationScope");
    return 3;
}

static void testFieldMonsterInspectView(float values[5],int write) {
    const uintptr_t rva[5]={RVA_BATTLE_CELL_PX,RVA_BATTLE_VIEW_ORIGIN_X,RVA_BATTLE_VIEW_ORIGIN_Y,RVA_BATTLE_VIEW_SCROLL_X,RVA_BATTLE_VIEW_SCROLL_Y};
    for(int i=0;i<5;i++) if(write) memcpy(GAME_PTR(rva[i]),values+i,sizeof(float));else memcpy(values+i,GAME_PTR(rva[i]),sizeof(float));
}

static int lInspectTestFieldMonster(lua_State *L) {
    const char *call="coe5.inspectTestFieldMonster";
    checkLuaMainThread(L,call);LuaScript *script=checkLuaScript(L);lua_Integer monster=-1;
    const char *reason=!readTestFieldInteger(L,1,&monster) || !isUsedMonster(monster) ? "bad_monster" : testFieldReadProblem(script);
    if(!reason && (!testField.editor || testField.fighting || testFieldFinalViewActive())) reason="not_editor_preparation";
    if(!reason && (battleDrawing > 0 || luaSiteDepth[SITE_BATTLE_DRAW] > 0)) reason="draw_read_only";
    if(!reason && testFieldDeploymentBusy) reason="field_transaction_active";
    if(!reason && (testFieldInspectActive || testFieldMonsterPreview.constructing)) reason="inspect_active";
    if(!reason && (testFieldLife.failed || testFieldLife.pending || !testFieldLife.active || testFieldLife.retiring)) reason="cleanup_unverified";
    if(!reason && (testFieldLife.depth || testFieldLife.overrideDepth)) reason="native_creation_active";
    if(!reason && (isNetworkGame() || G32(RVA_ACTIVE_PLAYER)!=0)) reason="unsupported_game_context";
    if(!reason && !testFieldTextPollReady) reason="text_input_hook_unavailable";
    if(!reason && battleWaiting<=0 && luaSiteDepth[SITE_BATTLE_WAIT]<=0 && testFieldDrawSession!=testFieldLife.session) reason="not_battle_frame";
    if(!reason && !lua_isnoneornil(L,2)) reason="unexpected_options";
    SdlKeyboardStateFn keyboard=sdlKeyboardState();
    if(!reason && !keyboard) reason="text_input_unavailable";
    if(reason) return testFieldMonsterInspectRefusal(L,reason);
    TestFieldMonsterInspectSnapshot s={0};
    s.last=G32(RVA_UNIT_COUNT);s.lastCommander=G32(RVA_LAST_COMMANDER);s.slot=s.last+1;s.width=G32(RVA_BATTLE_WIDTH);
    int records=s.monsterRecords=tableRecords(TABLE_MONSTER);
    if(s.last< -1 || s.slot>=UNIT_TABLE_MAX || s.lastCommander< -1 || s.lastCommander>=COMMANDER_TABLE_MAX ||
        s.width<1 || s.width>BATTLE_COLUMNS_MAX || (records!=MONSTER_TABLE_MAX && records!=MONSTER_RECORDS_MOVED)) {
        reason="preview_capacity_unavailable";
        writeLog("test_field_monster_preview_capacity","last=%d slot=%d last_commander=%d width=%d monster_records=%d max_units=%d max_commanders=%d max_columns=%d native_monsters=%d moved_monsters=%d",s.last,s.slot,s.lastCommander,s.width,records,UNIT_TABLE_MAX,COMMANDER_TABLE_MAX,BATTLE_COLUMNS_MAX,MONSTER_TABLE_MAX,MONSTER_RECORDS_MOVED);
    }
    if(!reason && (testFieldLife.generations[s.slot] || testFieldLife.actors[s.slot].generation ||
        G16(RVA_UNIT_HP+(size_t)s.slot*UNIT_SIZE)>0)) reason="scratch_slot_not_fresh";
    if(!reason) for(int c=0;c<=s.lastCommander;c++) if(G32(RVA_COMMANDERS+(size_t)c*COMMANDER_SIZE)==s.slot) {reason="scratch_slot_referenced";break;}
    if(reason) return testFieldMonsterInspectRefusal(L,reason);
    s.unitsBytes=(size_t)(s.last+1)*UNIT_SIZE;s.commandersBytes=(size_t)(s.lastCommander+1)*COMMANDER_SIZE;
    s.gridBytes=(size_t)s.width*BATTLE_ROWS*BATTLE_CELL_SIZE;s.monstersBytes=(size_t)records*MONSTER_SIZE;
    s.generationsBytes=UNIT_TABLE_MAX*sizeof *testFieldLife.generations;s.actorsBytes=UNIT_TABLE_MAX*sizeof *testFieldLife.actors;
    if(s.unitsBytes+s.commandersBytes+s.gridBytes+s.monstersBytes+s.generationsBytes+s.actorsBytes>128u*1024u*1024u)
        return testFieldMonsterInspectRefusal(L,"preview_snapshot_too_large");
    s.units=malloc(s.unitsBytes ? s.unitsBytes : 1);s.commanders=malloc(s.commandersBytes ? s.commandersBytes : 1);s.grid=malloc(s.gridBytes);
    s.monsters=malloc(s.monstersBytes);s.generations=malloc(s.generationsBytes);s.actors=malloc(s.actorsBytes);
    if(!s.units || !s.commanders || !s.grid || !s.monsters || !s.generations || !s.actors) {testFieldMonsterInspectFree(&s);return testFieldMonsterInspectRefusal(L,"out_of_memory");}
    if(s.unitsBytes) memcpy(s.units,GAME_PTR(RVA_UNITS),s.unitsBytes);
    if(s.commandersBytes) memcpy(s.commanders,GAME_PTR(RVA_COMMANDERS),s.commandersBytes);
    memcpy(s.grid,GAME_PTR(RVA_BATTLE_GRID),s.gridBytes);memcpy(s.slotBefore,GAME_PTR(RVA_UNITS+(size_t)s.slot*UNIT_SIZE),UNIT_SIZE);
    for(size_t cell=0;cell<s.gridBytes/BATTLE_CELL_SIZE;cell++) {
        int actor,object;memcpy(&actor,s.grid+cell*BATTLE_CELL_SIZE,4);memcpy(&object,s.grid+cell*BATTLE_CELL_SIZE+4,4);
        if(actor==s.slot || object==s.slot) reason="scratch_slot_referenced";
    }
    if(reason) {testFieldMonsterInspectFree(&s);return testFieldMonsterInspectRefusal(L,reason);}
    s.session=testFieldLife.session;s.serial=testFieldLife.serial;s.created=testFieldLife.createdCount;
    s.depth=testFieldLife.depth;s.overrideDepth=testFieldLife.overrideDepth;s.multiple=testFieldLife.allowMultiple;
    memcpy(s.generations,testFieldLife.generations,s.generationsBytes);
    memcpy(s.actors,testFieldLife.actors,s.actorsBytes);
    memcpy(s.monsters,tableAddress(TABLE_MONSTER),s.monstersBytes);
    s.activePlayer=G32(RVA_ACTIVE_PLAYER);s.x=G32(RVA_BATTLE_X);s.y=G32(RVA_BATTLE_Y);
    s.playerA=G32(RVA_BATTLE_PLAYER_A);s.playerB=G32(RVA_BATTLE_PLAYER_B);s.sea=G32(RVA_BATTLE_AT_SEA);
    s.boatsA=G32(RVA_BATTLE_SHIPS_A);s.boatsB=G32(RVA_BATTLE_SHIPS_D);
    s.randomIndex=G32(RVA_RANDOM_INDEX);s.randomShift=G32(RVA_RANDOM_SHIFT);s.randomXor=G32(RVA_RANDOM_XOR);
    memcpy(s.randomTable,GAME_PTR(RVA_RANDOM_TABLE),sizeof s.randomTable);s.crt=previewCrtRandState();
    for(int k=0;k<PREVIEW_CHURN_INTS;k++) s.churn[k]=G32(PREVIEW_CHURN_RVAS[k]);
    s.showSpells=G32(RVA_DEBUG_SHOW_HIDDEN);s.selectedCommander=G32(RVA_SELECTED_COMMANDER);
    s.zoom=testFieldZoom;memcpy(s.battleCenter,&testFieldBattleCenter,sizeof s.battleCenter);testFieldMonsterInspectView(s.view,0);
    int temporaryUnique=0;
    reason=installTestFieldMonsterInspect(&temporaryUnique);
    if(reason) {
        if(!stopTestFieldMonsterInspect(temporaryUnique)) {
            testFieldLife.failed=testFieldLife.pending=1;
            reason="preview_hook_cleanup_unverified";
        }
        testFieldMonsterInspectFree(&s);
        return testFieldMonsterInspectRefusal(L,reason);
    }
    int uniqueBefore=uniqueGroupUnitFirst((int)monster);
    int scratch=-1,opened=0,retired=0,verified=0,quarantined=0;
    testFieldInspectActive=1;
    testFieldDeploymentBusy=1;
    testFieldMonsterPreview=(TestFieldMonsterPreviewScope){.constructing=1,.monster=(int)monster,.slot=s.slot,.player=0,.thread=GetCurrentThreadId()};
    scratch=gameNewUnit(-1,-1,0,-2,(int)monster);
    testFieldMonsterPreview.constructing=0;
    int hooksIntact=testFieldMonsterInspectCallsMatch();
    int hooksRestored=stopTestFieldMonsterInspect(temporaryUnique);
    int ours=scratch==s.slot && testFieldMonsterPreview.allocations==1 && !testFieldLife.generations[s.slot] &&
        G16(RVA_UNIT_PLAYER+(size_t)s.slot*UNIT_SIZE)==0 && G32(RVA_UNIT_X+(size_t)s.slot*UNIT_SIZE)==-1 &&
        G32(RVA_UNIT_X+(size_t)s.slot*UNIT_SIZE+4)==-1 && (G32(RVA_UNITS+(size_t)s.slot*UNIT_SIZE)&0x20000);
    if(!ours || testFieldMonsterPreview.violation) reason="preview_constructor_unverified";
    else if(G16(RVA_UNIT_HP+(size_t)s.slot*UNIT_SIZE)<=0 || G16(RVA_UNIT_MONSTER+(size_t)s.slot*UNIT_SIZE)!=(int)monster) reason="preview_type_changed";
    else if(!testFieldMonsterInspectContext(script,&s) || !testFieldMonsterInspectExistingSame(&s) || !hooksIntact || !hooksRestored) reason="preview_changed_existing_state";
    if(!reason) {
        resetTestFieldText();clearTestFieldInspectInput(keyboard);G32(RVA_DEBUG_SHOW_HIDDEN)=1;
        luaCodeRunning--;
        GAME_FN(unsigned long long (*)(unsigned,int,int,int,int),RVA_UNIT_STATS_SCREEN)((unsigned)scratch,-1,0,2,0);
        luaCodeRunning++;
        opened=1;
        resetLuaSteps(L);
    }
    ours=scratch==s.slot && testFieldMonsterPreview.allocations==1 && !testFieldLife.generations[s.slot] &&
        G16(RVA_UNIT_PLAYER+(size_t)s.slot*UNIT_SIZE)==0 && G32(RVA_UNIT_X+(size_t)s.slot*UNIT_SIZE)==-1 &&
        G32(RVA_UNIT_X+(size_t)s.slot*UNIT_SIZE+4)==-1 && (G32(RVA_UNITS+(size_t)s.slot*UNIT_SIZE)&0x20000);
    if(ours && testFieldMonsterInspectContext(script,&s) && G32(RVA_LAST_COMMANDER)==s.lastCommander) {
        for(int c=0;c<=s.lastCommander;c++) if(G32(RVA_COMMANDERS+(size_t)c*COMMANDER_SIZE)==s.slot) ours=0;
    } else ours=0;
    if(ours) {
        GAME_FN(void (*)(int),RVA_REMOVE_UNIT)(s.slot);
        retired=G16(RVA_UNIT_HP+(size_t)s.slot*UNIT_SIZE)==-9999 && G16(RVA_UNIT_PLAYER+(size_t)s.slot*UNIT_SIZE)==-1 &&
            G32(RVA_UNIT_X+(size_t)s.slot*UNIT_SIZE)==-1 && G32(RVA_UNIT_X+(size_t)s.slot*UNIT_SIZE+4)==-1;
    }
    int context=testFieldMonsterInspectContext(script,&s),existing=context && testFieldMonsterInspectExistingSame(&s);
    int noAllocation=scratch<0 && !testFieldMonsterPreview.allocations && G32(RVA_UNIT_COUNT)==s.last &&
        !memcmp(s.slotBefore,GAME_PTR(RVA_UNITS+(size_t)s.slot*UNIT_SIZE),UNIT_SIZE);
    if(context && existing && ((retired && G32(RVA_UNIT_COUNT)==s.slot) || noAllocation)) {
        if(retired) {memcpy(GAME_PTR(RVA_UNITS+(size_t)s.slot*UNIT_SIZE),s.slotBefore,UNIT_SIZE);G32(RVA_UNIT_COUNT)=s.last;}
        G32(RVA_RANDOM_INDEX)=s.randomIndex;G32(RVA_RANDOM_SHIFT)=s.randomShift;G32(RVA_RANDOM_XOR)=s.randomXor;
        GAME_FN(void (*)(unsigned),RVA_CRT_SRAND)(s.crt);
        for(int k=0;k<PREVIEW_CHURN_INTS;k++) G32(PREVIEW_CHURN_RVAS[k])=s.churn[k];
        G32(RVA_DEBUG_SHOW_HIDDEN)=s.showSpells;G32(RVA_SELECTED_COMMANDER)=s.selectedCommander;
        testFieldZoom=s.zoom;memcpy(&testFieldBattleCenter,s.battleCenter,sizeof s.battleCenter);testFieldMonsterInspectView(s.view,1);
        float restoredView[5];
        testFieldMonsterInspectView(restoredView,0);
        verified=testFieldMonsterInspectExistingSame(&s) && G32(RVA_UNIT_COUNT)==s.last &&
            !memcmp(s.view,restoredView,sizeof s.view) &&
            !memcmp(s.slotBefore,GAME_PTR(RVA_UNITS+(size_t)s.slot*UNIT_SIZE),UNIT_SIZE) &&
            G32(RVA_RANDOM_INDEX)==s.randomIndex && G32(RVA_RANDOM_SHIFT)==s.randomShift && G32(RVA_RANDOM_XOR)==s.randomXor &&
            !memcmp(GAME_PTR(RVA_RANDOM_TABLE),s.randomTable,sizeof s.randomTable) && previewCrtRandState()==s.crt &&
            uniqueGroupUnitFirst((int)monster)==uniqueBefore && hooksRestored;
        for(int k=0;k<PREVIEW_CHURN_INTS;k++) if(G32(PREVIEW_CHURN_RVAS[k])!=s.churn[k]) verified=0;
    }
    if(!verified) {reason="preview_restore_unverified";testFieldLife.failed=testFieldLife.pending=1;quarantined=1;}
    testFieldDeploymentBusy=0;
    testFieldInspectActive=0;testFieldMonsterPreview.constructing=0;clearTestFieldInspectInput(keyboard);
    int ok=opened && verified && !reason;
    if(!ok && !reason) reason="preview_not_opened";
    writeLog("test_field_monster_inspected","monster=%lld scratch=%d opened=%d retired=%d existing_unchanged=%d verified=%d quarantine=%d",(long long)monster,scratch,opened,retired,existing,verified,quarantined);
    testFieldMonsterInspectFree(&s);
    lua_pushboolean(L,ok);if(!ok) lua_pushstring(L,reason);
    lua_createtable(L,0,16);
    setIntField(L,"monster",monster);setBoolField(L,"opened",opened);setBoolField(L,"verified",verified);
    setBoolField(L,"existingStateUnchanged",existing);setBoolField(L,"quarantined",quarantined);
    setBoolField(L,"temporaryInstance",1);setBoolField(L,"instanceCreated",testFieldMonsterPreview.allocations==1);
    setBoolField(L,"instanceRetained",!verified && testFieldMonsterPreview.allocations==1);setBoolField(L,"readOnly",1);
    setBoolField(L,"cleanupVerified",verified);setBoolField(L,"changed",!verified);
    setIntField(L,"uniqueLookupBypasses",testFieldMonsterPreview.uniqueBypasses);
    setBoolField(L,"startingItemsIncluded",0);setBoolField(L,"futureInstanceSpellsVerified",0);
    lua_pushliteral(L,"native_unit_card");lua_setfield(L,-2,"source");
    lua_pushliteral(L,"native_preview_instance");lua_setfield(L,-2,"scope");
    lua_pushliteral(L,"Real native read-only unit card for an isolated temporary default instance. Random spells are a preview example; starting items omitted; no future placed-instance configuration is promised.");lua_setfield(L,-2,"limitations");
    return ok ? 2 : 3;
}
