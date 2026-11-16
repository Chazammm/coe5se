#pragma once

/* Local experiment, Windows CoE5 5.43 only. No monster/template mutation.
 * Item storage and anatomical compatibility come from upstream 3.4.0.
 * Base-school overrides are owned by one test-field actor generation and are
 * consumed by both native level readers AND the native combat attack reader. */
#ifndef __APPLE__
typedef struct {
    int unit, monster, count;
    uint64_t generation, session;
    int path[MONSTER_ATTACK_MAX], level[MONSTER_ATTACK_MAX];
} TestFieldTraining;
#define TEST_FIELD_TRAINING_MAX 2000
static TestFieldTraining testFieldTraining[TEST_FIELD_TRAINING_MAX];
static int testFieldTrainingIndex[UNIT_TABLE_MAX]; /* entry index + 1; zero absent */
static int testFieldTrainingCount, testFieldTrainingHooks;
static unsigned testFieldTrainingCombatReads;
static int (*testFieldOriginalBase)(int,int);
static int (*testFieldOriginalLevel)(int,int);

static int testFieldTrainingValid(const TestFieldTraining *t) {
    return testField.open && t->unit >= 0 && t->unit < UNIT_TABLE_MAX &&
           testFieldLifeActorOwned(t->unit) && t->generation == testFieldLife.actors[t->unit].generation &&
           t->session == testFieldLife.session && G16(RVA_UNIT_HP+(size_t)t->unit*UNIT_SIZE)>0 &&
           t->monster == (short)G16(RVA_UNIT_MONSTER + (size_t)t->unit * UNIT_SIZE);
}
static TestFieldTraining *testFieldTrainingOf(int unit) {
    if(!testField.open || unit<0 || unit>=UNIT_TABLE_MAX)return NULL;
    int index=testFieldTrainingIndex[unit]-1;
    if(index>=0 && index<testFieldTrainingCount && testFieldTraining[index].unit==unit && testFieldTrainingValid(&testFieldTraining[index]))return &testFieldTraining[index];
    return NULL;
}
static int testFieldTrainingLevel(const TestFieldTraining *t,int path) {
    for (int i=0;t && i<t->count;i++) if (t->path[i]==path) return t->level[i];
    return -1;
}
static int testFieldTrainingBaseHook(int unit,int path) {
    int native=testFieldOriginalBase(unit,path);
    const TestFieldTraining *t=testFieldTrainingOf(unit);
    if (path>=0) { int level=testFieldTrainingLevel(t,path);return level>=0?level:native; }
    if (path==-1 && t) {
        native=0;
        for(int i=0;i<t->count;i++)if(t->level[i]>native)native=t->level[i];
    }
    return native;
}
static int testFieldTrainingPathHook(int unit,int path) {
    const TestFieldTraining *t=testFieldTrainingOf(unit);
    int level=testFieldTrainingLevel(t,path);
    if(level>=0){level+=GAME_FN(int (*)(int,int),RVA_UNIT_PATH_BONUS)(unit,path);return level>0?level:0;}
    if(path==-1 && t && t->count>0) {
        int highest=0;for(int i=0;i<t->count;i++)if(t->level[i]>highest)highest=t->level[i];
        highest+=GAME_FN(int (*)(int,int),RVA_UNIT_PATH_BONUS)(unit,-1);
        return highest>0?highest:0;
    }
    return testFieldOriginalLevel(unit,path);
}
static void testFieldTrainingAttackAdjust(int unit,int type,int weapon,long long *value) {
    if(type>=0 && type<20 && ((0x8c048u>>type)&1u)) {
        int level=testFieldTrainingLevel(testFieldTrainingOf(unit),weapon);
        if(level>=0){*value=(*value & ~0xffffffffLL)|(unsigned)level;if(testField.fighting)testFieldTrainingCombatReads++;}
    }
}
static int testFieldTrainingInstall(void) {
    if(testFieldTrainingHooks)return testFieldTrainingHooks>0;
    struct {size_t rva;void *hook;void **original;} hooks[]={
        {RVA_UNIT_SPELL_WEAPON_LEVEL,(void*)testFieldTrainingBaseHook,(void**)&testFieldOriginalBase},
        {RVA_UNIT_PATH_LEVEL,(void*)testFieldTrainingPathHook,(void**)&testFieldOriginalLevel}};
    int created=0,enabled=0;
    for(int i=0;i<2;i++)if(fanpatchHookCreate(gameBase+hooks[i].rva,hooks[i].hook,hooks[i].original))created++;
    if(created==2 && installItemSlotWeapons())for(int i=0;i<2;i++)if(fanpatchHookEnable(gameBase+hooks[i].rva,1))enabled++;
    if(enabled!=2)for(int i=0;i<2;i++)if(*hooks[i].original)fanpatchHookEnable(gameBase+hooks[i].rva,0);
    testFieldTrainingHooks=enabled==2?1:-1;
    if(enabled==2)testFieldAttackAdjustment=testFieldTrainingAttackAdjust;
    writeLog("test_field_training_hooks","created=%d enabled=%d",created,enabled);
    return testFieldTrainingHooks>0;
}
static const char *testFieldLoadoutGuard(LuaScript *script,int unit) {
    const char *reason=testFieldUnitEditorProblem(script,unit);
    if(!reason && testField.activePlayer>=0)reason="not_main_menu";
    if(!reason && !testFieldTrainingInstall())reason="training_hooks_unavailable";
    return reason;
}
static int testFieldLoadoutItem(int id) {
    return id>=0 && id<tableRecords(TABLE_ITEM) &&
        ((const char*)(tableAddress(TABLE_ITEM)+(size_t)id*ITEM_SIZE))[0];
}
static int testFieldLoadoutImplant(int id) {
    return id>=0 && (itemFirstAbility(id,ITEM_ABILITY_EYE)>0 || itemFirstAbility(id,ITEM_ABILITY_HEART)>0);
}
static unsigned testFieldOrganInjuries(int unit) {
    unsigned long long flags;memcpy(&flags,GAME_PTR(RVA_UNITS+(size_t)unit*UNIT_SIZE),sizeof flags);
    return (unsigned)(flags&0x700000ULL);
}
static int testFieldSameImplants(const short *before,const short *after) {
    for(int p=0;p<ITEM_POSITION_MAX;p++) {
        int ids[2]={before[p],after[p]};
        for(int k=0;k<2;k++)if(ids[k]>=0) {
            if(!testFieldLoadoutItem(ids[k]))return 0;
            if(testFieldLoadoutImplant(ids[k])) {
                int a=0,b=0;for(int q=0;q<ITEM_POSITION_MAX;q++){a+=before[q]==ids[k];b+=after[q]==ids[k];}
                if(a!=b)return 0;
            }
        }
    }
    return 1;
}
static void testFieldLoadoutItems(int unit,short *items) {
    for(int p=0;p<ITEM_POSITION_MAX;p++)items[p]=(short)unitItemAt(unit,p);
}
static int testFieldLoadoutSameItems(int unit,const short *items) {
    for(int p=0;p<ITEM_POSITION_MAX;p++)if(unitItemAt(unit,p)!=items[p])return 0;
    return 1;
}
static void testFieldLoadoutWriteItems(int unit,const short *items) {
    for(int p=0;p<ITEM_POSITION_MAX;p++)setUnitItemAt(unit,p,items[p]);
    flushUnitItemCaches(unit);
}
static int testFieldLoadoutResult(lua_State *L,const char *reason,int rollback) {
    lua_pushboolean(L,reason==NULL);
    if(reason)lua_pushstring(L,reason);else lua_pushnil(L);
    lua_newtable(L);lua_pushboolean(L,rollback);lua_setfield(L,-2,"rollbackVerified");
    lua_pushstring(L,reason?"validate":"committed");lua_setfield(L,-2,"stage");
    testFieldCasterInt(L,"combatAttackReads",testFieldTrainingCombatReads);
    return 3;
}
static const char *testFieldLoadoutReadItems(lua_State *L,int state,short *items) {
    lua_pushliteral(L,"items");lua_rawget(L,state);
    if(!lua_istable(L,-1) || lua_rawlen(L,-1)!=ITEM_POSITION_MAX){lua_pop(L,1);return "bad_item_positions";}
    for(int p=0;p<ITEM_POSITION_MAX;p++) {
        lua_Integer id;lua_rawgeti(L,-1,p+1);
        int good=testFieldEditorInteger(L,-1,&id);lua_pop(L,1);
        if(!good || id < -1 || id>SHRT_MAX || (id>=0 && !testFieldLoadoutItem((int)id))){lua_pop(L,1);return "bad_item";}
        items[p]=(short)id;
    }
    lua_pop(L,1);return NULL;
}
static const char *testFieldLoadoutReadSchools(lua_State *L,int state,int unit,TestFieldTraining *wanted) {
    lua_pushliteral(L,"schools");lua_rawget(L,state);
    if(!lua_istable(L,-1) || lua_rawlen(L,-1)>MONSTER_ATTACK_MAX){lua_pop(L,1);return "bad_schools";}
    wanted->count=(int)lua_rawlen(L,-1);
    for(int i=0;i<wanted->count;i++) {
        lua_rawgeti(L,-1,i+1);
        if(!lua_istable(L,-1)){lua_pop(L,2);return "bad_schools";}
        lua_Integer path,level;lua_pushliteral(L,"path");lua_rawget(L,-2);int good=testFieldEditorInteger(L,-1,&path);lua_pop(L,1);
        lua_pushliteral(L,"level");lua_rawget(L,-2);good=good && testFieldEditorInteger(L,-1,&level);lua_pop(L,2);
        if(!good || path<0 || path>=spellPathCount() || level<1 || level>INT_MAX){lua_pop(L,1);return "bad_school_level";}
        int native=testFieldOriginalBase(unit,(int)path);
        if(native<=0){lua_pop(L,1);return "school_not_existing";}
        if(level>3 && level!=native){lua_pop(L,1);return "unsupported_school_level";}
        for(int j=0;j<i;j++)if(wanted->path[j]==path){lua_pop(L,1);return "duplicate_school";}
        wanted->path[i]=(int)path;wanted->level[i]=(int)level;
    }
    int nativeCount=0;
    for(int path=0;path<spellPathCount();path++)if(testFieldOriginalBase(unit,path)>0)nativeCount++;
    if(nativeCount!=wanted->count){lua_pop(L,1);return "missing_school_configuration";}
    lua_pop(L,1);return NULL;
}

static int lTestFieldLoadoutState(lua_State *L) {
    checkLuaMainThread(L,"coe5.testFieldLoadoutState");LuaScript *script=checkLuaScript(L);
    lua_Integer unit=-1;
    const char *reason=testFieldEditorInteger(L,1,&unit) && unit>=0 && unit<UNIT_TABLE_MAX ?
        testFieldLoadoutGuard(script,(int)unit):"bad_unit";
    if(reason){lua_pushnil(L);lua_pushstring(L,reason);return 2;}
    TestFieldCasterSnapshot caster;testFieldCasterRead((int)unit,&caster);
    lua_newtable(L);int result=lua_gettop(L);
    testFieldCasterInt(L,"unit",unit);testFieldCasterInt(L,"generation",testFieldLife.actors[unit].generation);
    testFieldCasterInt(L,"combatAttackReads",testFieldTrainingCombatReads);
    testFieldCasterInt(L,"organInjuryFlags",testFieldOrganInjuries((int)unit));
    lua_pushboolean(L,caster.complete);lua_setfield(L,result,"complete");
    testFieldCasterInt(L,"maxBase",testFieldTrainingBaseHook((int)unit,-1));
    testFieldCasterInt(L,"maxEffective",testFieldTrainingPathHook((int)unit,-1));
    lua_pushboolean(L,caster.complete && !caster.detail.reason);lua_setfield(L,result,"valid");
    lua_createtable(L,ITEM_POSITION_MAX,0);
    for(int p=0;p<ITEM_POSITION_MAX;p++) {
        int item=unitItemAt((int)unit,p),kind=itemSlotKind((int)unit,p);
        lua_newtable(L);testFieldCasterInt(L,"position",p);testFieldCasterInt(L,"id",item);testFieldCasterInt(L,"slotType",kind);
        if(item>=0 && testFieldLoadoutItem(item)) {
            lua_pushstring(L,(const char*)(tableAddress(TABLE_ITEM)+(size_t)item*ITEM_SIZE));lua_setfield(L,-2,"name");
            testFieldCasterInt(L,"itemType",itemTypeOf(item));
            lua_pushboolean(L,(itemFlagsOf(item)&ITEM_FLAG_CURSED)!=0);lua_setfield(L,-2,"cursed");
            lua_pushboolean(L,testFieldLoadoutImplant(item));lua_setfield(L,-2,"permanentEffects");
        } else if(item>=0) {lua_pushboolean(L,0);lua_setfield(L,result,"complete");}
        lua_rawseti(L,-2,p+1);
    }
    lua_setfield(L,result,"items");
    lua_newtable(L);int n=0;
    for(int path=0;path<spellPathCount();path++) {
        int base=testFieldOriginalBase((int)unit,path);if(base<=0)continue;
        const char *name=spellPathNames()[path];
        lua_newtable(L);testFieldCasterInt(L,"path",path);testFieldCasterInt(L,"templateBase",base);
        testFieldCasterInt(L,"base",testFieldTrainingBaseHook((int)unit,path));
        testFieldCasterInt(L,"level",caster.level[path]);testFieldCasterInt(L,"bonus",caster.bonus[path]);
        testFieldCasterInt(L,"allowance",caster.allowance[path]);
        lua_pushstring(L,name?name:"Unnamed school");lua_setfield(L,-2,"name");
        lua_rawseti(L,-2,++n);
    }
    lua_setfield(L,result,"schools");
    lua_newtable(L);for(int i=0;i<caster.knownCount;i++){lua_pushinteger(L,caster.known[i]);lua_rawseti(L,-2,i+1);}lua_setfield(L,result,"known");
    lua_newtable(L);for(int i=0;i<caster.memorizedCount;i++){lua_pushinteger(L,caster.memorized[i]);lua_rawseti(L,-2,i+1);}lua_setfield(L,result,"memorized");
    return 1;
}

static int lSetTestFieldConfiguration(lua_State *L) {
    checkLuaMainThread(L,"coe5.setTestFieldConfiguration");LuaScript *script=checkLuaScript(L);
    lua_Integer unit=-1;
    const char *reason=testFieldEditorInteger(L,1,&unit) && unit>=0 && unit<UNIT_TABLE_MAX ?
        testFieldLoadoutGuard(script,(int)unit):"bad_unit";
    uint64_t generation=reason?0:testFieldLife.actors[unit].generation,session=testFieldLife.session;
    TestFieldTraining wanted={0},oldTraining={0};short items[ITEM_POSITION_MAX],oldItems[ITEM_POSITION_MAX];
    int restoreKnown=0,cursedPosition=-1,cursedItem=-1;
    TestFieldCasterSnapshot oldCaster,candidate={.complete=1,.detail={.stage="validate",.path=-1,.spell=-1,.level=-1}};
    if(!reason && !lua_istable(L,2))reason="bad_state";
    if(!reason)reason=testFieldLoadoutReadItems(L,2,items);
    if(!reason)reason=testFieldLoadoutReadSchools(L,2,(int)unit,&wanted);
    if(!reason)reason=readTestFieldSpellList(L,2,"known",candidate.known,UNIT_KNOWN_SPELLS_MAX,&candidate.knownCount);
    if(!reason)reason=readTestFieldSpellList(L,2,"memorized",candidate.memorized,UNIT_MEMORIZED_SPELLS_MAX,&candidate.memorizedCount);
    if(!reason) {
        lua_pushliteral(L,"restoreKnown");lua_rawget(L,2);
        if(!lua_isnil(L,-1) && lua_type(L,-1)!=LUA_TBOOLEAN)reason="bad_restore_known";
        restoreKnown=lua_toboolean(L,-1);lua_pop(L,1);
    }
    if(!reason) {
        lua_pushliteral(L,"removeCursed");lua_rawget(L,2);
        if(!lua_isnil(L,-1)) {
            if(!lua_istable(L,-1))reason="bad_cursed_removal";
            else {
                lua_Integer position=-1,item=-1;
                lua_pushliteral(L,"position");lua_rawget(L,-2);
                if(!testFieldEditorInteger(L,-1,&position) || position<0 || position>=ITEM_POSITION_MAX)reason="bad_cursed_removal";
                lua_pop(L,1);lua_pushliteral(L,"item");lua_rawget(L,-2);
                if(!testFieldEditorInteger(L,-1,&item) || item<0 || item>=tableRecords(TABLE_ITEM))reason="bad_cursed_removal";
                lua_pop(L,1);
                if(!reason){cursedPosition=(int)position;cursedItem=(int)item;}
            }
        }
        lua_pop(L,1);
    }
    /* Parse raw table data, then pin the same owned generation before writes. */
    if(!reason)reason=testFieldLoadoutGuard(script,(int)unit);
    if(!reason && (session!=testFieldLife.session || generation!=testFieldLife.actors[unit].generation))reason="identity_changed";
    if(reason)return testFieldLoadoutResult(L,reason,1);
    testFieldCasterRead((int)unit,&oldCaster);
    if(!oldCaster.complete)return testFieldLoadoutResult(L,"incomplete_native_configuration",1);
    testFieldLoadoutItems((int)unit,oldItems);
    unsigned oldOrganInjuries=testFieldOrganInjuries((int)unit);
    int restoreImplantLayout=restoreKnown && testFieldSameImplants(oldItems,items);
    if(cursedPosition>=0) {
        if(oldItems[cursedPosition]!=cursedItem || !testFieldLoadoutItem(cursedItem) ||
            !(itemFlagsOf(cursedItem)&ITEM_FLAG_CURSED) || items[cursedPosition]!=-1)
            return testFieldLoadoutResult(L,"cursed_removal_changed",1);
        for(int p=0;p<ITEM_POSITION_MAX;p++)if(p!=cursedPosition && items[p]!=oldItems[p])
            return testFieldLoadoutResult(L,"cursed_removal_only",1);
        for(int i=0;i<wanted.count;i++)if(testFieldTrainingBaseHook((int)unit,wanted.path[i])!=wanted.level[i])
            return testFieldLoadoutResult(L,"cursed_removal_only",1);
        if(candidate.knownCount!=oldCaster.knownCount || candidate.memorizedCount!=oldCaster.memorizedCount ||
            memcmp(candidate.known,oldCaster.known,(size_t)candidate.knownCount*sizeof candidate.known[0]) ||
            memcmp(candidate.memorized,oldCaster.memorized,(size_t)candidate.memorizedCount*sizeof candidate.memorized[0]))
            return testFieldLoadoutResult(L,"cursed_removal_only",1);
    }
    for(int p=0;p<ITEM_POSITION_MAX;p++) {
        if(oldItems[p]>=0 && !testFieldLoadoutItem(oldItems[p]))return testFieldLoadoutResult(L,"bad_existing_item",1);
        if(oldItems[p]>=0 && oldItems[p]!=items[p] && (itemFlagsOf(oldItems[p])&ITEM_FLAG_CURSED) && p!=cursedPosition &&
            !(restoreImplantLayout && testFieldLoadoutImplant(oldItems[p])))
            return testFieldLoadoutResult(L,"cursed_item",1);
        if(items[p]!=oldItems[p] && (testFieldLoadoutImplant(items[p]) || testFieldLoadoutImplant(oldItems[p])) &&
            !(p==cursedPosition && items[p]==-1) && !restoreImplantLayout)
            return testFieldLoadoutResult(L,"permanent_anatomy_item_unsupported",1);
    }
    if(!ensureExtraItems())return testFieldLoadoutResult(L,"out_of_memory",1);
    TestFieldTraining *entry=testFieldTrainingOf((int)unit);
    if(!entry)for(int i=0;i<testFieldTrainingCount;i++)if(!testFieldTrainingValid(&testFieldTraining[i])){entry=&testFieldTraining[i];break;}
    if(!entry && testFieldTrainingCount<TEST_FIELD_TRAINING_MAX)entry=&testFieldTraining[testFieldTrainingCount++];
    if(!entry)return testFieldLoadoutResult(L,"too_many_configured_units",1);
    oldTraining=*entry;wanted.unit=(int)unit;wanted.monster=(short)G16(RVA_UNIT_MONSTER+(size_t)unit*UNIT_SIZE);
    wanted.generation=testFieldLife.actors[unit].generation;
    wanted.session=testFieldLife.session;
    testFieldTrainingIndex[unit]=(int)(entry-testFieldTraining)+1;
    *entry=wanted;
    short empty[ITEM_POSITION_MAX];memset(empty,0xff,sizeof empty);testFieldLoadoutWriteItems((int)unit,empty);
    for(int p=0;!reason && p<ITEM_POSITION_MAX;p++)if(items[p]>=0) {
        if(itemSlotKind((int)unit,p)!=itemTypeOf(items[p]))reason="incompatible_slot";
        else if(!testFieldLoadoutImplant(items[p]) && GAME_FN(CanEquipItemFn,RVA_CAN_EQUIP_ITEM)((int)unit,items[p],0)<0)reason="native_item_restriction";
        if(!reason){setUnitItemAt((int)unit,p,items[p]);flushUnitItemCaches((int)unit);}
    }
    for(int p=0;!reason && p<ITEM_POSITION_MAX;p++)if(items[p]>=0 && itemSlotKind((int)unit,p)!=itemTypeOf(items[p]))reason="equipment_changes_anatomy";
    if(!reason)reason=testFieldCasterValidate((int)unit,&candidate);
    for(int i=0;!reason && i<candidate.knownCount;i++) {
        int spell=candidate.known[i],known=0,path=spellSlot(spell)->path;
        for(int j=0;j<oldCaster.knownCount;j++)if(oldCaster.known[j]==spell)known=1;
        if(!known && (candidate.level[path]<=0 || spellSlot(spell)->level>candidate.level[path])) {
            int native=testFieldOriginalBase((int)unit,path),supported=native>3?native:3;
            if(!restoreKnown || native<=0 || spellSlot(spell)->level>supported)reason="new_spell_above_level";
        }
    }
    if(!reason)reason=testFieldUnitEditorProblem(script,unit);
    short *knownAt=(short*)GAME_PTR(RVA_UNIT_KNOWN_SPELLS+(size_t)unit*UNIT_SIZE);
    short *memorizedAt=(short*)GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS+(size_t)unit*UNIT_SIZE);
    if(!reason) {
        memcpy(knownAt,candidate.known,sizeof candidate.known);memcpy(memorizedAt,candidate.memorized,sizeof candidate.memorized);
        TestFieldCasterSnapshot actual;reason=testFieldCasterRead((int)unit,&actual);
        if(!reason && (!testFieldLoadoutSameItems((int)unit,items) || memcmp(knownAt,candidate.known,sizeof candidate.known) ||
            memcmp(memorizedAt,candidate.memorized,sizeof candidate.memorized)))reason="configuration_readback_failed";
        for(int i=0;!reason && i<wanted.count;i++)if(testFieldTrainingBaseHook((int)unit,wanted.path[i])!=wanted.level[i])reason="school_readback_failed";
    }
    if(!reason && testFieldOrganInjuries((int)unit)!=oldOrganInjuries)reason="permanent_effect_changed";
    int rollback=1;
    if(reason) {
        *entry=oldTraining;testFieldLoadoutWriteItems((int)unit,oldItems);
        memcpy(knownAt,oldCaster.known,sizeof oldCaster.known);memcpy(memorizedAt,oldCaster.memorized,sizeof oldCaster.memorized);
        rollback=testFieldLoadoutSameItems((int)unit,oldItems) && !memcmp(knownAt,oldCaster.known,sizeof oldCaster.known) &&
            !memcmp(memorizedAt,oldCaster.memorized,sizeof oldCaster.memorized);
        for(int path=0;rollback && path<spellPathCount();path++)if(GAME_FN(int (*)(int,int),RVA_UNIT_PATH_LEVEL)((int)unit,path)!=oldCaster.level[path])rollback=0;
        if(testFieldOrganInjuries((int)unit)!=oldOrganInjuries)rollback=0;
        if(!rollback)reason="rollback_failed";
    }
    writeLog("test_field_configuration","mod=%s unit=%lld result=%s rollback_verified=%d restore_known=%d remove_cursed_position=%d remove_cursed_item=%d",script->mod,(long long)unit,reason?reason:"committed",rollback,restoreKnown,cursedPosition,cursedItem);
    int result=testFieldLoadoutResult(L,reason,rollback);
    if(cursedPosition>=0){testFieldCasterInt(L,"cursedRemovalPosition",cursedPosition);testFieldCasterInt(L,"cursedRemovalItem",cursedItem);}
    lua_pushboolean(L,restoreKnown);lua_setfield(L,-2,"restoreKnown");
    lua_pushstring(L,reason?(rollback?"rejected_rollback_verified":"rollback_unverified"):"committed");lua_setfield(L,-2,"stage");
    if(candidate.detail.reason) {
        testFieldCasterInt(L,"path",candidate.detail.path);testFieldCasterInt(L,"spell",candidate.detail.spell);
        testFieldCasterInt(L,"actual",candidate.detail.actual);testFieldCasterInt(L,"allowed",candidate.detail.allowed);
        testFieldCasterInt(L,"allowance",candidate.detail.allowed);testFieldCasterInt(L,"level",candidate.detail.level);
    }
    return result;
}
#else
static int lTestFieldLoadoutState(lua_State *L){lua_pushnil(L);lua_pushliteral(L,"windows_only");return 2;}
static int lSetTestFieldConfiguration(lua_State *L){lua_pushboolean(L,0);lua_pushliteral(L,"windows_only");return 2;}
#endif
