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

#ifdef __APPLE__
// ---- The item card on Mac: the one inside the unit card --------------------------------------------
// @platform-split test_field_item_card: Windows calls the game's item card (RVA_ITEM_INFO_SCREEN); Mac's is copied into its unit card, which CoE5SE opens with unit -1 and step hooks take into the item card and out again
#define UNIT_CARD_FRAME_READY_WORD 0xb9020ff3u
#define UNIT_CARD_ITEM_CARD_EXIT_WORD 0xf007d91cu
static struct {
    StepHook entry, start, exit, done;
    uint32_t entryGame, entryBranch, exitGame, exitBranch;
    int state;
    int item;
    uint64_t sp;
} macItemCard = {.item = -1};

static void macItemCardEntry(StepContext *ctx, const StepHook *hook) {
    (void)hook;
    if (macItemCard.item < 0) return;
    uint64_t d15;
    memcpy(&d15, GAME_PTR(RVA_MENU_FLOAT4), sizeof d15);
    ctx->x[28] = (uint64_t)(unsigned)macItemCard.item;
    ctx->x[13] = 1;
    ctx->x[24] = (uint64_t)(uintptr_t)GAME_PTR(RVA_LAYOUT_ELEMENTS);
    ctx->q[15] = d15;
    macItemCard.sp = ctx->sp;
    macItemCard.item = -1;
    ctx->pc = macItemCard.start.resume;
}

static void macItemCardExit(StepContext *ctx, const StepHook *hook) {
    (void)hook;
    if (!macItemCard.sp || ctx->sp != macItemCard.sp) return;
    macItemCard.sp = 0;
    ctx->pc = macItemCard.done.resume;
}

static int putMacItemCard(void) {
    if (macItemCard.state) return macItemCard.state > 0;
    macItemCard.state = -1;
    if (!gameCodeMatches) {
        writeLogError("hook_skipped", "site=item_card_mac reason=game_code_differs");
        return 0;
    }
    memcpy(&macItemCard.entryGame, gameBase + RVA_UNIT_CARD_FRAME_READY, 4);
    memcpy(&macItemCard.exitGame, gameBase + RVA_UNIT_CARD_ITEM_CARD_EXIT, 4);
    if (macItemCard.entryGame != UNIT_CARD_FRAME_READY_WORD || macItemCard.exitGame != UNIT_CARD_ITEM_CARD_EXIT_WORD) {
        writeLogError("hook_skipped", "site=item_card_mac reason=not_the_expected_code");
        return 0;
    }
    macItemCard.entry.handler = macItemCardEntry;
    macItemCard.exit.handler = macItemCardExit;
    int ok = putStepResumeMac(RVA_UNIT_CARD_ITEM_CARD_START, &macItemCard.start, "item_card_mac_start") &&
             putStepResumeMac(RVA_UNIT_CARD_RETURN, &macItemCard.done, "item_card_mac_return") &&
             putStepHookMac(RVA_UNIT_CARD_FRAME_READY, &macItemCard.entry, "item_card_mac_entry", 0) &&
             putStepHookMac(RVA_UNIT_CARD_ITEM_CARD_EXIT, &macItemCard.exit, "item_card_mac_exit", 0);
    if (ok) {
        memcpy(&macItemCard.entryBranch, gameBase + RVA_UNIT_CARD_FRAME_READY, 4);
        memcpy(&macItemCard.exitBranch, gameBase + RVA_UNIT_CARD_ITEM_CARD_EXIT, 4);
        ok = writeGameCode(gameBase + RVA_UNIT_CARD_FRAME_READY, &macItemCard.entryGame, 4) &&
             writeGameCode(gameBase + RVA_UNIT_CARD_ITEM_CARD_EXIT, &macItemCard.exitGame, 4);
    }
    macItemCard.state = ok ? 1 : -1;
    writeResultLog(ok, "item_card_mac_installed", "padding_left=%ld", paddingLeft());
    return ok;
}

static int showItemCardMac(int item) {
    unsigned char *entry = gameBase + RVA_UNIT_CARD_FRAME_READY, *exit = gameBase + RVA_UNIT_CARD_ITEM_CARD_EXIT;
    if (macItemCard.state <= 0 || !writeGameCode(entry, &macItemCard.entryBranch, 4) || !writeGameCode(exit, &macItemCard.exitBranch, 4)) {
        writeGameCode(entry, &macItemCard.entryGame, 4);
        writeLogError("item_card_mac_failed", "item=%d reason=hook", item);
        return 0;
    }
    macItemCard.item = item;
    GAME_FN(unsigned long long (*)(int, int, int, int, int), RVA_UNIT_CARD)(-1, -1, 0, 0, 0);
    int shown = macItemCard.item < 0;
    macItemCard.item = -1;
    macItemCard.sp = 0;
    writeGameCode(entry, &macItemCard.entryGame, 4);
    writeGameCode(exit, &macItemCard.exitGame, 4);
    if (!shown) writeLogError("item_card_mac_failed", "item=%d reason=not_reached", item);
    return shown;
}

static long long macItemInfoScreen(long long item, long long b, long long c, long long d, long long e, long long f, long long g, long long h) {
    (void)b, (void)c, (void)d, (void)e, (void)f, (void)g, (void)h;
    showItemCardMac((int)item);
    return 0;
}
#endif

// @platform-split test_field_item_card: Windows calls the game's item card (RVA_ITEM_INFO_SCREEN); Mac's is copied into its unit card, which CoE5SE opens with unit -1 and step hooks take into the item card and out again
static int lInspectTestFieldItem(lua_State *L) {
    const char *call = "coe5.inspectTestFieldItem";
    checkLuaMainThread(L, call);
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
#ifdef __APPLE__
    if (!reason && !putMacItemCard()) reason = "item_card_hook_unavailable";
#endif
    if (reason) return refuseTestFieldCall(L, call, reason, 0, "item=%lld", (long long)item);
    unsigned long long session = testFieldLife.session;
    int randomIndex = G32(RVA_RANDOM_INDEX), randomShift = G32(RVA_RANDOM_SHIFT), randomXor = G32(RVA_RANDOM_XOR);
    resetTestFieldText();
    clearTestFieldInspectInput(keyboard);
    testFieldInspectActive = 1;
    luaCodeRunning--;
#ifdef __APPLE__
    int shown = showItemCardMac((int)item);
#else
    GAME_FN(void (*)(int), RVA_ITEM_INFO_SCREEN)((int)item);
#endif
    luaCodeRunning++;
    testFieldInspectActive = 0;
    clearTestFieldInspectInput(keyboard);
    resetLuaSteps(L);
    G32(RVA_RANDOM_INDEX) = randomIndex;
    G32(RVA_RANDOM_SHIFT) = randomShift;
    G32(RVA_RANDOM_XOR) = randomXor;
#ifdef __APPLE__
    if (!shown) return refuseTestFieldCall(L, call, "item_card_hook_unavailable", 0, "item=%lld", (long long)item);
#endif
    if (!testField.open || session != testFieldLife.session || _stricmp(script->mod, testField.owner))
        return refuseTestFieldCall(L, call, "field_changed", 0, "item=%lld", (long long)item);
    lua_pushboolean(L, 1);
    lua_newtable(L);
    lua_pushliteral(L, "native_item_card");
    lua_setfield(L, -2, "source");
    setBoolField(L, "readOnly", 1);
    setIntField(L, "item", item);
    return 2;
}
