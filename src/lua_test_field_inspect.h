#pragma once

static int testFieldInspectActive;

typedef const unsigned char *(*SdlKeyboardStateFn)(int *count);
static SdlKeyboardStateFn sdlKeyboardState(void) {
    return (SdlKeyboardStateFn)SDL_FUNCTION("SDL_GetKeyboardState", RVA_SDL_GET_KEYBOARD_STATE_SLOT);
}

static void clearTestFieldInspectInput(SdlKeyboardStateFn keyboard) {
    memset(keysHeld, 0, sizeof keysHeld);
    memset(keyStateWritten, 0, sizeof keyStateWritten);
    memset(GAME_PTR(RVA_KEY_STATE), 0, KEY_STATE_SIZE);
    G32(RVA_KEY_BUFFER_LAST) = -1;
    G32(RVA_BUTTONS_HELD) = G32(RVA_PRESSED) = G32(RVA_MOUSE_BUTTONS) = G32(RVA_WHEEL) = 0;
    int count = 0;
    const unsigned char *state = keyboard(&count);
    if (!state || count <= 0) return;
    if (count > SDL_SCANCODE_COUNT) count = SDL_SCANCODE_COUNT;
    for (int i = 0; i < count; i++)
        if (state[i]) testFieldText.held[i] = 1;
}

static const char *testFieldInspectSpellProblem(int unit, int final) {
    if (!final) return testFieldCasterProblem(unit);
    const char *reason = testFieldCasterReadProblem(unit);
    if (reason) return reason;
    const short *known = (const short *)GAME_PTR(RVA_UNIT_KNOWN_SPELLS + (size_t)unit * UNIT_SIZE);
    const short *memorized = (const short *)GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + (size_t)unit * UNIT_SIZE);
    for (int i = 0; i < UNIT_KNOWN_SPELLS_MAX && known[i] >= 0; i++)
        if (!isTestFieldSpell(known[i])) return "bad_spell";
    for (int i = 0; i < UNIT_MEMORIZED_SPELLS_MAX && memorized[i] >= 0; i++)
        if (!isTestFieldSpell(memorized[i])) return "bad_spell";
    return NULL;
}

static int inspectTestFieldUnit(lua_State *L, int final) {
    const char *call = final ? "coe5.inspectTestFieldFinalUnit" : "coe5.inspectTestFieldUnit";
    checkLuaMainThread(L, call);
    LuaScript *script = checkLuaScript(L);
    lua_Integer unit = luaL_checkinteger(L, 1);
    const char *reason = final ? testFieldFinalInspectProblem(script, unit) : testFieldUnitEditProblem(script, unit);
    if (!reason && testFieldInspectActive) reason = "inspect_active";
    if (!reason && testField.activePlayer >= 0) reason = "not_main_menu";
    if (!reason && !testFieldTextPollReady) reason = "text_input_hook_unavailable";
    if (!reason && battleWaiting <= 0 && luaSiteDepth[SITE_BATTLE_WAIT] <= 0 && testFieldDrawSession != testFieldLife.session) reason = "not_battle_frame";
    if (!reason && !lua_isnoneornil(L, 2)) {
        if (!lua_istable(L, 2)) reason = "bad_options";
        else {
            lua_pushliteral(L, "readOnly");
            lua_rawget(L, 2);
            if (!lua_isnil(L, -1) && (lua_type(L, -1) != LUA_TBOOLEAN || !lua_toboolean(L, -1))) reason = "read_only_required";
            lua_pop(L, 1);
        }
    }
    if (!reason) reason = testFieldInspectSpellProblem((int)unit, final);
    SdlKeyboardStateFn keyboard = sdlKeyboardState();
    if (!reason && !keyboard) reason = "text_input_unavailable";
    if (reason) return refuseTestFieldCall(L, call, reason, 0, "unit=%lld read_only=1", (long long)unit);
    size_t at = (size_t)unit * UNIT_SIZE;
    unsigned char before[UNIT_SIZE], after[UNIT_SIZE], compared[UNIT_SIZE];
    const size_t known = RVA_UNIT_KNOWN_SPELLS - RVA_UNITS, memorized = RVA_UNIT_MEMORIZED_SPELLS - RVA_UNITS;
    const size_t knownBytes = UNIT_KNOWN_SPELLS_MAX * sizeof(short), memorizedBytes = UNIT_MEMORIZED_SPELLS_MAX * sizeof(short);
    unsigned long long generation = testFieldLife.generations[unit];
    int viewer = G16(RVA_UNIT_PLAYER + at);
    memcpy(before, GAME_PTR(RVA_UNITS + at), sizeof before);
    resetTestFieldText();
    clearTestFieldInspectInput(keyboard);
    testFieldInspectActive = 1;
    luaCodeRunning--;
    GAME_FN(unsigned long long (*)(unsigned, int, int, int, int), RVA_UNIT_STATS_SCREEN)((unsigned)unit, viewer, 0, 2, 0);
    luaCodeRunning++;
    testFieldInspectActive = 0;
    clearTestFieldInspectInput(keyboard);
    resetLuaSteps(L);
    int identity = isTestFieldActor((int)unit) && testFieldLife.generations[unit] == generation;
    int spellsChanged = 0, spellsRestored = 0, otherChanged = 0, itemsChanged = 0, hpChanged = 0;
    if (!identity) reason = "identity_changed";
    else {
        memcpy(after, GAME_PTR(RVA_UNITS + at), sizeof after);
        spellsChanged = memcmp(before + known, after + known, knownBytes) || memcmp(before + memorized, after + memorized, memorizedBytes);
        memcpy(compared, after, sizeof compared);
        memcpy(compared + known, before + known, knownBytes);
        memcpy(compared + memorized, before + memorized, memorizedBytes);
        otherChanged = memcmp(before, compared, sizeof before) != 0;
        itemsChanged = memcmp(before + (RVA_UNIT_ITEMS - RVA_UNITS), after + (RVA_UNIT_ITEMS - RVA_UNITS), UNIT_ITEM_SLOTS * sizeof(short)) != 0;
        hpChanged = memcmp(before + (RVA_UNIT_HP - RVA_UNITS), after + (RVA_UNIT_HP - RVA_UNITS), sizeof(short)) != 0;
        if (otherChanged) reason = "inspect_changed_unverified";
        else if (spellsChanged) {
            memcpy(GAME_PTR(RVA_UNIT_KNOWN_SPELLS + at), before + known, knownBytes);
            memcpy(GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + at), before + memorized, memorizedBytes);
            spellsRestored = !memcmp(GAME_PTR(RVA_UNIT_KNOWN_SPELLS + at), before + known, knownBytes) &&
                             !memcmp(GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + at), before + memorized, memorizedBytes);
            if (!spellsRestored) reason = "restore_failed";
        }
        if (!reason) reason = final ? testFieldFinalInspectProblem(script, unit) : testFieldUnitEditProblem(script, unit);
        if (!reason) reason = testFieldInspectSpellProblem((int)unit, final);
    }
    writeLog("test_field_unit_inspected",
             "unit=%lld generation=%llu read_only=1 identity_matches=%d spells_changed=%d spells_restored=%d other_changed=%d items_changed=%d hp_changed=%d "
             "verified=%d",
             (long long)unit, generation, identity, spellsChanged, spellsRestored, otherChanged, itemsChanged, hpChanged, reason == NULL);
    if (reason) refuseLuaCall(script->mod, call, reason, "unit=%lld read_only=1", (long long)unit);
    lua_pushboolean(L, reason == NULL);
    if (reason) lua_pushstring(L, reason);
    lua_createtable(L, 0, 12);
    setIntField(L, "unit", (int)unit);
    setBoolField(L, "readOnly", 1);
    setBoolField(L, "changed", !identity || otherChanged || (spellsChanged && !spellsRestored));
    setBoolField(L, "spellsChanged", spellsChanged);
    setBoolField(L, "spellsRestored", spellsRestored);
    setBoolField(L, "itemsChanged", itemsChanged);
    setBoolField(L, "hpChanged", hpChanged);
    setBoolField(L, "identityMatches", identity);
    setBoolField(L, "verified", reason == NULL);
    setBoolField(L, "nativeFinalizeDisabled", 1);
    setBoolField(L, "finalView", final);
    setBoolField(L, "currentNativeState", 1);
    lua_pushliteral(L, "native_unit_card");
    lua_setfield(L, -2, "source");
    return reason ? 3 : 2;
}

static int lInspectTestFieldUnit(lua_State *L) {
    return inspectTestFieldUnit(L, 0);
}

static int lInspectTestFieldFinalUnit(lua_State *L) {
    return inspectTestFieldUnit(L, 1);
}

// @platform-split test_field_item_card: Windows shows the game's item card; Mac's is copied into its unit card, with no function to call, so the call refuses (windows_only)
static int lInspectTestFieldItem(lua_State *L) {
    const char *call = "coe5.inspectTestFieldItem";
    checkLuaMainThread(L, call);
#ifdef __APPLE__
    return refuseTestFieldCall(L, call, "windows_only", 0, "item=%lld", (long long)luaL_optinteger(L, 1, -1));
#else
    LuaScript *script = checkLuaScript(L);
    lua_Integer item = -1;
    const char *reason = NULL;
    if (!readTestFieldInteger(L, 1, &item) || item < 0 || item >= tableRecords(TABLE_ITEM) || !*(const char *)(tableAddress(TABLE_ITEM) + (size_t)item * ITEM_SIZE))
        reason = "bad_item";
    else if (!gameCodeMatches) reason = "game_code_differs";
    else if (!testField.open) reason = "not_open";
    else if (_stricmp(script->mod, testField.owner)) reason = "not_owner";
    else if (testField.fighting) reason = "in_fight";
    else if (testFieldFinalViewActive()) reason = "final_read_only";
    else if (testField.activePlayer >= 0) reason = "not_main_menu";
    else if (testFieldInspectActive) reason = "inspect_active";
    SdlKeyboardStateFn keyboard = sdlKeyboardState();
    if (!reason && !keyboard) reason = "text_input_unavailable";
    if (reason) return refuseTestFieldCall(L, call, reason, 0, "item=%lld", (long long)item);
    unsigned long long session = testFieldLife.session;
    int randomIndex = G32(RVA_RANDOM_INDEX), randomShift = G32(RVA_RANDOM_SHIFT), randomXor = G32(RVA_RANDOM_XOR);
    resetTestFieldText();
    clearTestFieldInspectInput(keyboard);
    testFieldInspectActive = 1;
    luaCodeRunning--;
    GAME_FN(void (*)(int), RVA_ITEM_INFO_SCREEN)((int)item);
    luaCodeRunning++;
    testFieldInspectActive = 0;
    clearTestFieldInspectInput(keyboard);
    resetLuaSteps(L);
    G32(RVA_RANDOM_INDEX) = randomIndex;
    G32(RVA_RANDOM_SHIFT) = randomShift;
    G32(RVA_RANDOM_XOR) = randomXor;
    if (!testField.open || session != testFieldLife.session || _stricmp(script->mod, testField.owner))
        return refuseTestFieldCall(L, call, "field_changed", 0, "item=%lld", (long long)item);
    lua_pushboolean(L, 1);
    lua_newtable(L);
    lua_pushliteral(L, "native_item_card");
    lua_setfield(L, -2, "source");
    setBoolField(L, "readOnly", 1);
    setIntField(L, "item", item);
    return 2;
#endif
}
