// @platform-split battle_replays: Windows redirects the eof crc and replay fight calls through stubs in a near page, Mac through trampolines
#pragma once

#define REPLAY_END_VERSION 1
#define REPLAY_NOTICE_MODS_MAX 6

// ---- The fight's end --------------------------------------------------------------------------------
typedef struct { int crc, winner, scriptCalls; } BattleEnd;

static int battleEofCrc, battleEofCrcSeen;
static int battleScriptCalls;
static char battleScriptMods[REPLAY_NOTICE_MODS_MAX][MOD_NAME_MAX];
static int battleScriptModCount, battleScriptModsLeft;

static void noteBattleScriptCall(const LuaScript *s) {
    if (!luaBattleCodeNow() || !s) return;
    if (netRoleNow() != NET_ROLE_SINGLE && modClassNow(s->mod) != LUA_CLASS_CONTENT) return;
    battleScriptCalls++;
    for (int i = 0; i < battleScriptModCount; i++)
        if (!strcmp(battleScriptMods[i], s->mod)) return;
    if (battleScriptModCount == REPLAY_NOTICE_MODS_MAX) {
        battleScriptModsLeft++;
        return;
    }
    snprintf(battleScriptMods[battleScriptModCount++], MOD_NAME_MAX, "%s", s->mod);
}

static void startBattleRecord(int seed, int x, int y) {
    startLuaBattle(seed, x, y);
    battleEofCrcSeen = 0;
    battleScriptCalls = 0;
    battleScriptModCount = battleScriptModsLeft = 0;
}

static void buildBattleEnd(SaveBuffer *b, const BattleEnd *end) {
    saveBufferInt(b, REPLAY_END_VERSION);
    saveBufferInt(b, end->crc);
    saveBufferInt(b, end->winner);
    saveBufferInt(b, end->scriptCalls);
}

static int readBattleEnd(const unsigned char *payload, int size, BattleEnd *end) {
    SaveCursor c = {payload, payload + size, 0};
    int version = saveCursorInt(&c);
    end->crc = saveCursorInt(&c), end->winner = saveCursorInt(&c), end->scriptCalls = saveCursorInt(&c);
    return !c.failed && version == REPLAY_END_VERSION;
}

static int battleEndsDiffer(const BattleEnd *host, const BattleEnd *replay) {
    return host->crc != replay->crc || host->winner != replay->winner;
}

// ---- The trailer the host's fight adds ----------------------------------------------------------------
static char battleReplayWrittenPath[2600];

static void noteBattleReplayWritten(const char *path) {
    if (strlen(path) >= sizeof battleReplayWrittenPath) {
        writeLogWarning("replay_trailer_written", "ok=0 reason=path_too_long");
        battleReplayWrittenPath[0] = '\0';
        return;
    }
    snprintf(battleReplayWrittenPath, sizeof battleReplayWrittenPath, "%s", path);
}

static int replayCarriesScripts(void) {
    for (int i = 0; i < luaScriptCount; i++)
        if (!luaScripts[i]->retired && !luaScripts[i]->dead) return 1;
    return 0;
}

static void writeReplayScriptStore(FILE *file) {
    SaveBuffer payload = {0};
    int refused = 0, mods = writeGameDataPayload(&payload, -1, &refused);
    if (writeReplayChunk(file, REPLAY_STORE_TAG, &payload)) writeLog("replay_store_written", "mods=%d bytes=%zu refused=%d", mods, payload.size, refused);
    free(payload.bytes);
}

static void appendBattleEnd(int winner) {
    if (!battleReplayWrittenPath[0]) return;
    if (!battleEofCrcSeen) {
        writeLogWarning("replay_end_written", "ok=0 reason=no_end_crc");
        battleReplayWrittenPath[0] = '\0';
        return;
    }
    BattleEnd end = {battleEofCrc, winner, battleScriptCalls};
    FILE *file = fopen(battleReplayWrittenPath, "ab");
    battleReplayWrittenPath[0] = '\0';
    if (!file) {
        writeLogError("replay_end_written", "ok=0 reason=no_file");
        return;
    }
    SaveBuffer payload = {0};
    buildBattleEnd(&payload, &end);
    if (writeReplayChunk(file, REPLAY_END_TAG, &payload))
        writeLog("replay_end_written", "crc=%d winner=%d script_calls=%d", end.crc, end.winner, end.scriptCalls);
    free(payload.bytes);
    fclose(file);
}

// ---- The box ----------------------------------------------------------------------------------------
static int replayDiffersDue, replayDiffersShown;
static char replayDiffersText[POPUP_TEXT_SIZE];
#define REPLAY_DIFFERS_TITLE "CoE5SE: Replay differs"

static void replayDiffersMessage(char *text, size_t size, const char (*mods)[MOD_NAME_MAX], int count, int more) {
    size_t n = 0;
    text[0] = '\0';
    appendText(text, size, &n, "Battle replay differs from the fight.\n\nBattle scripts: ");
    for (int i = 0; i < count; i++) {
        char name[MOD_NAME_MAX];
        luaNoticeName(mods[i], name, sizeof name);
        appendText(text, size, &n, "%s%.60s", i ? ", " : "", name);
    }
    if (!count) appendText(text, size, &n, "none here");
    if (more) appendText(text, size, &n, ", +%d", more);
}

static void showReplayDiffersNotice(void) {
    if (!replayDiffersDue || replayDiffersShown || luaCodeRunning) return;
    NetRole role = netRoleNow();
    if (role == NET_ROLE_HOST || role == NET_ROLE_OFFICIAL || (role == NET_ROLE_CLIENT && !netClientInTurn)) return;
    replayDiffersShown = 1;
    writeLog("replay_differs_shown", "role=%d", role);
    okBox(REPLAY_DIFFERS_TITLE, replayDiffersText);
}

// ---- The hooks ----------------------------------------------------------------------------------------
static int battleEofCrcHook(int x, int y) {
    int crc = GAME_FN(int (*)(int, int), RVA_BATTLE_CRC)(x, y);
    battleEofCrc = crc;
    battleEofCrcSeen = 1;
    return crc;
}

static u64 (*nextFightTheFight)(u64, u64);
static u64 fightTheFightHook(u64 x, u64 y) {
    startBattleRecord(G32(RVA_BATTLE_SEED), (int)x, (int)y);
    u64 winner = nextFightTheFight(x, y);
    appendBattleEnd((int)winner);
    return winner;
}

static int replayFightHook(int x, int y) {
    unsigned char *file = NULL;
    size_t size = 0;
    readReplayFileBytes("battle_replay_read", &file, &size);
    SaveCursor header = {file, file + (file ? size : 0), 0};
    saveCursorInt(&header), saveCursorInt(&header);
    int seed = saveCursorInt(&header);
    startBattleRecord(header.failed ? G32(RVA_BATTLE_SEED) : seed, x, y);
    const unsigned char *payload = NULL;
    int payloadSize = 0, swapped = -1;
    if (file && findReplayChunk(file, size, REPLAY_STORE_TAG, &payload, &payloadSize) > 0) swapped = useReplayGameData(payload, (size_t)payloadSize);
    int winner = GAME_FN(int (*)(int, int), RVA_FIGHT_THE_FIGHT)(x, y);
    restoreReplayGameData();
    BattleEnd host = {0}, replay = {battleEofCrcSeen ? battleEofCrc : 0, winner, battleScriptCalls};
    int recorded = file && findReplayChunk(file, size, REPLAY_END_TAG, &payload, &payloadSize) > 0 && readBattleEnd(payload, payloadSize, &host);
    int differs = recorded && battleEofCrcSeen && battleEndsDiffer(&host, &replay);
    const char *result = !recorded ? "no_record" : !battleEofCrcSeen ? "no_end_crc" : "match", *store = swapped < 0 ? "none" : "used";
    if (differs)
        writeLogWarning("battle_replay_compared", "result=differs reason=ends_differ host_crc=%d replay_crc=%d host_winner=%d replay_winner=%d host_script_calls=%d replay_script_calls=%d store=%s",
                        host.crc, replay.crc, host.winner, replay.winner, host.scriptCalls, replay.scriptCalls, store);
    else
        writeLog("battle_replay_compared", "result=%s host_crc=%d replay_crc=%d host_winner=%d replay_winner=%d host_script_calls=%d replay_script_calls=%d store=%s", result,
                 host.crc, replay.crc, host.winner, replay.winner, host.scriptCalls, replay.scriptCalls, store);
    if (differs && (host.scriptCalls || replay.scriptCalls) && !replayDiffersDue) {
        replayDiffersDue = 1;
        replayDiffersMessage(replayDiffersText, sizeof replayDiffersText, (const char (*)[MOD_NAME_MAX])battleScriptMods, battleScriptModCount, battleScriptModsLeft);
    }
    free(file);
    return winner;
}

static int redirectBattleCall(unsigned int rva, unsigned int targetRva, void *hook, const char *label) {
#ifdef __APPLE__
    if (blTarget(gameBase + rva) != gameBase + targetRva) {
        writeLog("hook_skipped", "site=%s reason=not_the_expected_call", label);
        return 0;
    }
    unsigned char *trampoline = newTrampoline(hook);
    uint32_t word = trampoline ? blTo(gameBase + rva, trampoline) : 0;
    if (trampoline && writeGameCode(gameBase + rva, &word, 4)) return 1;
    writeLog("hook_skipped", "site=%s reason=%s", label, trampoline ? "code_unwritable" : "no_padding");
    return 0;
#else
    unsigned char *stub = allocNear(gameBase + rva);
    if (!stub) {
        writeLog("hook_skipped", "site=%s reason=no_near_page", label);
        return 0;
    }
    makePagesReadExecute(stub, 0x1000);
    return redirectCall(rva, targetRva, stub, hook, label, NULL);
#endif
}

static int battleReplayScriptsState;
static void installBattleReplayScripts(void) {
    if (battleReplayScriptsState) return;
    char owner[64];
    moduleFileName((void *)fightTheFightHook, owner, sizeof owner);
    int ok = installItemSlotReplay() +
             insertWrap(SITE_FIGHT_THE_FIGHT, (void *)fightTheFightHook, (void **)&nextFightTheFight, (WrapPlace){.layer = WRAP_LAYER_OUTERMOST}, owner, NULL) +
             redirectBattleCall(RVA_BATTLE_EOF_CRC_CALL, RVA_BATTLE_CRC, (void *)battleEofCrcHook, "battle_eof_crc") +
             redirectBattleCall(RVA_REPLAY_FIGHT_CALL, RVA_FIGHT_THE_FIGHT, (void *)replayFightHook, "replay_fight");
    battleReplayScriptsState = ok == 4 ? 1 : -1;
    writeInstallLog("battle_replay_scripts_installed", ok, 4);
}
