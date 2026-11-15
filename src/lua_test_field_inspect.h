#pragma once

/* The genuine native unit card, restricted to an owned living TestField actor.
 * Include after lua_test_field_input.h and the strict caster-state helper.
 * Native 5.43 showstats(unit, viewer, drawOnly, mode, ciok): viewer=-1 blocks
 * owner-only rename/debug actions; ciok=0 disables spell/item changes AND the
 * chooseMemorized finalizer on Exit. No editable mode is exposed here. */
static int testFieldInspectActive;
static const char *testFieldSpellSnapshotProblem(int unit);

#ifndef __APPLE__
typedef const unsigned char *(*TestFieldInspectKeyboardState)(int *);

static void testFieldInspectClearInput(TestFieldInspectKeyboardState keyboard) {
    /* Inspect owns this modal interaction. Clear its native pending events,
     * then retain physical SDL scancodes until KeyUp in the existing input
     * hook, so closing Enter/Escape/repeats cannot become battle shortcuts. */
    memset(keysHeld, 0, sizeof keysHeld);
    memset(keyStateWritten, 0, sizeof keyStateWritten);
    memset(GAME_PTR(RVA_KEY_STATE), 0, KEY_STATE_SIZE);
    G32(RVA_KEY_BUFFER_LAST) = -1;
    G32(RVA_BUTTONS_HELD) = G32(RVA_PRESSED) = G32(RVA_MOUSE_BUTTONS) = G32(RVA_WHEEL) = 0;
    int count = 0;
    const unsigned char *state = keyboard(&count);
    if (state && count > 0) {
        if (count > TEST_FIELD_TEXT_SCANCODES) count = TEST_FIELD_TEXT_SCANCODES;
        for (int i = 0; i < count; i++) if (state[i]) testFieldText.held[i] = 1;
    }
}
#endif

static const char *testFieldInspectSpellProblem(int unit, int final) {
    if (!final) return testFieldSpellSnapshotProblem(unit);
    const char *reason = testFieldCasterReadProblem(unit);
    if (reason) return reason;
    /* Combat can legitimately change levels/selections. RO viewing requires
     * safe IDs, not a prepare-phase memorization quota. The existing fanpatch
     * may still normalize inline arrays; the full readback below restores them. */
    const short *known = (const short *)GAME_PTR(RVA_UNIT_KNOWN_SPELLS + (size_t)unit * UNIT_SIZE);
    const short *mem = (const short *)GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + (size_t)unit * UNIT_SIZE);
    for (int i = 0; i < UNIT_KNOWN_SPELLS_MAX && known[i] >= 0; i++) if (!testFieldCasterSpell(known[i])) return "bad_spell";
    for (int i = 0; i < UNIT_MEMORIZED_SPELLS_MAX && mem[i] >= 0; i++) if (!testFieldCasterSpell(mem[i])) return "bad_spell";
    return NULL;
}

static int inspectTestFieldUnit(lua_State *L, int final) {
    const char *call = final ? "coe5.inspectTestFieldFinalUnit" : "coe5.inspectTestFieldUnit";
    checkLuaMainThread(L, call);
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = luaL_checkinteger(L, 1);
    const char *reason = final ? testFieldFinalInspectProblem(script, unit) : testFieldUnitEditorProblem(script, unit);
    if (!reason && testFieldInspectActive) reason = "inspect_active";
    if (!reason && testField.activePlayer >= 0) reason = "not_main_menu";
    if (!reason && !testFieldTextPollReady) reason = "text_input_hook_unavailable";
    if (!reason && battleWaiting <= 0 && luaSiteDepth[SITE_BATTLE_WAIT] <= 0 &&
        testFieldDrawSession != testFieldLife.session) reason = "not_battle_frame";
    if (!reason && !lua_isnoneornil(L, 2)) {
        if (!lua_istable(L, 2)) reason = "bad_options";
        else {
            lua_pushliteral(L, "readOnly"); lua_rawget(L, 2);
            if (!lua_isnil(L, -1) && (lua_type(L, -1) != LUA_TBOOLEAN || !lua_toboolean(L, -1))) reason = "read_only_required";
            lua_pop(L, 1);
        }
    }
    if (!reason) reason = testFieldInspectSpellProblem((int)unit, final);
#ifndef __APPLE__
    HMODULE sdl = GetModuleHandleW(L"SDL2.dll");
    TestFieldInspectKeyboardState keyboard = sdl ? (TestFieldInspectKeyboardState)(void *)GetProcAddress(sdl, "SDL_GetKeyboardState") : NULL;
    if (!reason && !keyboard) reason = "text_input_unavailable";
#endif
    if (reason) {
        refuseLuaCall(script->mod, call, reason, "unit=%lld read_only=1", (long long)unit);
        lua_pushboolean(L, 0); lua_pushstring(L, reason); return 2;
    }
#ifdef __APPLE__
    lua_pushboolean(L, 0); lua_pushliteral(L, "windows_only"); return 2;
#else
    size_t at = (size_t)unit * UNIT_SIZE;
    unsigned char before[UNIT_SIZE], after[UNIT_SIZE], compared[UNIT_SIZE];
    const size_t known = RVA_UNIT_KNOWN_SPELLS - RVA_UNITS, memorized = RVA_UNIT_MEMORIZED_SPELLS - RVA_UNITS;
    const size_t knownBytes = UNIT_KNOWN_SPELLS_MAX * sizeof(short), memorizedBytes = UNIT_MEMORIZED_SPELLS_MAX * sizeof(short);
    unsigned long long generation = testFieldLife.generations[unit];
    memcpy(before, GAME_PTR(RVA_UNITS + at), sizeof before);
    int showSpells = *(int *)(gameBase + RVA_TEST_FIELD_INSPECT_SHOW_SPELLS);
    testFieldTextReset();
    testFieldInspectClearInput(keyboard);
    testFieldInspectActive = 1;
    *(int *)(gameBase + RVA_TEST_FIELD_INSPECT_SHOW_SPELLS) = 1;
    luaCodeRunning--;
    GAME_FN(unsigned long long (*)(unsigned, int, int, int, int), RVA_UNIT_STATS_SCREEN)((unsigned)unit, -1, 0, 2, 0);
    luaCodeRunning++;
    *(int *)(gameBase + RVA_TEST_FIELD_INSPECT_SHOW_SPELLS) = showSpells;
    testFieldInspectActive = 0;
    testFieldInspectClearInput(keyboard);
    resetLuaSteps(L);
    int identity = testFieldLifeActorOwned((int)unit) && testFieldLife.generations[unit] == generation;
    int spellsChanged = 0, spellsRestored = 0, otherChanged = 0, itemsChanged = 0, hpChanged = 0;
    if (!identity) reason = "identity_changed";
    else {
        memcpy(after, GAME_PTR(RVA_UNITS + at), sizeof after);
        spellsChanged = memcmp(before + known, after + known, knownBytes) || memcmp(before + memorized, after + memorized, memorizedBytes);
        memcpy(compared, after, sizeof compared);
        memcpy(compared + known, before + known, knownBytes);
        memcpy(compared + memorized, before + memorized, memorizedBytes);
        otherChanged = memcmp(before, compared, sizeof before) != 0;
        itemsChanged = memcmp(before + (RVA_UNIT_ITEMS - RVA_UNITS), after + (RVA_UNIT_ITEMS - RVA_UNITS), 8 * sizeof(short)) != 0;
        hpChanged = memcmp(before + (RVA_UNIT_HP - RVA_UNITS), after + (RVA_UNIT_HP - RVA_UNITS), sizeof(short)) != 0;
        if (otherChanged) reason = "inspect_changed_unverified";
        else if (spellsChanged) {
            /* The existing Feeblemind fanpatch can normalize a caster at card
             * entry independently of native ciok. Enforce RO by restoring only
             * the two verified inline arrays, after unchanged identity and all
             * remaining bytes have been proved. Never copy the full record. */
            memcpy(GAME_PTR(RVA_UNIT_KNOWN_SPELLS + at), before + known, knownBytes);
            memcpy(GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + at), before + memorized, memorizedBytes);
            spellsRestored = !memcmp(GAME_PTR(RVA_UNIT_KNOWN_SPELLS + at), before + known, knownBytes) &&
                             !memcmp(GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + at), before + memorized, memorizedBytes);
            if (!spellsRestored) reason = "restore_failed";
        }
        if (!reason) reason = final ? testFieldFinalInspectProblem(script, unit) : testFieldUnitEditorProblem(script, unit);
        if (!reason) reason = testFieldInspectSpellProblem((int)unit, final);
    }
    writeLog("test_field_unit_inspected", "unit=%lld generation=%llu read_only=1 identity_matches=%d spells_changed=%d spells_restored=%d other_changed=%d items_changed=%d hp_changed=%d verified=%d",
             (long long)unit, generation, identity, spellsChanged, spellsRestored, otherChanged, itemsChanged, hpChanged, reason == NULL);
    if (reason) refuseLuaCall(script->mod, call, reason, "unit=%lld read_only=1", (long long)unit);
    lua_pushboolean(L, reason == NULL);
    if (reason) lua_pushstring(L, reason);
    lua_createtable(L, 0, 12);
    testFieldStateInt(L, "unit", (int)unit); testFieldStateBool(L, "readOnly", 1);
    testFieldStateBool(L, "changed", !identity || otherChanged || (spellsChanged && !spellsRestored));
    testFieldStateBool(L, "spellsChanged", spellsChanged); testFieldStateBool(L, "spellsRestored", spellsRestored);
    testFieldStateBool(L, "itemsChanged", itemsChanged); testFieldStateBool(L, "hpChanged", hpChanged);
    testFieldStateBool(L, "identityMatches", identity); testFieldStateBool(L, "verified", reason == NULL);
    testFieldStateBool(L, "nativeFinalizeDisabled", 1);
    testFieldStateBool(L, "finalView", final);
    testFieldStateBool(L, "currentNativeState", 1);
    lua_pushliteral(L, "native_unit_card"); lua_setfield(L, -2, "source");
    return reason ? 3 : 2;
#endif
}

static int lInspectTestFieldUnit(lua_State *L) { return inspectTestFieldUnit(L, 0); }
static int lInspectTestFieldFinalUnit(lua_State *L) { return inspectTestFieldUnit(L, 1); }

/* Native metadata-only item card. Unlike editable showstats, this function has
 * no unit/commander argument and no equip or memorization finalizer. */
static int lInspectTestFieldItem(lua_State *L) {
    checkLuaMainThread(L,"coe5.inspectTestFieldItem");LuaScript *script=checkLuaScript(L);
#ifdef __APPLE__
    (void)script;lua_pushboolean(L,0);lua_pushliteral(L,"windows_only");return 2;
#else
    lua_Integer item=-1;const char *reason=NULL;
    if(!testFieldEditorInteger(L,1,&item) || item<0 || item>=tableRecords(TABLE_ITEM) ||
       !*(const char*)(tableAddress(TABLE_ITEM)+(size_t)item*ITEM_SIZE))reason="bad_item";
    else if(!gameCodeMatches)reason="game_code_differs";
    else if(!testField.open)reason="not_open";
    else if(_stricmp(script->mod,testField.owner))reason="not_owner";
    else if(testField.fighting)reason="in_fight";
    else if(testFieldFinalViewActive())reason="final_read_only";
    else if(testField.activePlayer>=0)reason="not_main_menu";
    else if(testFieldInspectActive)reason="inspect_active";
    HMODULE sdl=GetModuleHandleW(L"SDL2.dll");
    TestFieldInspectKeyboardState keyboard=sdl?(TestFieldInspectKeyboardState)(void*)GetProcAddress(sdl,"SDL_GetKeyboardState"):NULL;
    if(!reason && !keyboard)reason="text_input_unavailable";
    if(reason){lua_pushboolean(L,0);lua_pushstring(L,reason);return 2;}
    uint64_t session=testFieldLife.session;
    int randomIndex=G32(RVA_RANDOM_INDEX),randomShift=G32(RVA_RANDOM_SHIFT),randomXor=G32(RVA_RANDOM_XOR);
    testFieldTextReset();testFieldInspectClearInput(keyboard);testFieldInspectActive=1;
    luaCodeRunning--;GAME_FN(void (*)(int),RVA_ITEM_INFO_SCREEN)((int)item);luaCodeRunning++;
    testFieldInspectActive=0;testFieldInspectClearInput(keyboard);resetLuaSteps(L);
    G32(RVA_RANDOM_INDEX)=randomIndex;G32(RVA_RANDOM_SHIFT)=randomShift;G32(RVA_RANDOM_XOR)=randomXor;
    if(!testField.open || session!=testFieldLife.session || _stricmp(script->mod,testField.owner)) {
        lua_pushboolean(L,0);lua_pushliteral(L,"field_changed");return 2;
    }
    lua_pushboolean(L,1);lua_newtable(L);
    lua_pushliteral(L,"native_item_card");lua_setfield(L,-2,"source");
    lua_pushboolean(L,1);lua_setfield(L,-2,"readOnly");
    lua_pushinteger(L,item);lua_setfield(L,-2,"item");
    return 2;
#endif
}
