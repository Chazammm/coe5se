#pragma once

#define FOREST_FROM_WORDS     1
#define FOREST_EARLIER_PASS   2
#define FOREST_TABLE_SIZE     2048
#define FOREST_TABLE_SHIFT    11
#define TERRAIN_SAVANNA       0x77
#define TERRAIN_ANCIENT_FOREST 4
#define FOREST_STUB_BYTES     64

enum { FOREST_HEARTCLAIM, FOREST_WILDSPAWN, FOREST_HOTCONVERT, FOREST_FULLFOREST, FOREST_WORDS };
static const char *const FOREST_WORD_NAMES[FOREST_WORDS] = {"heartclaim", "wildspawn", "hotconvert", "fullforest"};
enum { FOREST_TEST_CLAIM, FOREST_TEST_WILD, FOREST_TEST_HEAT, FOREST_TESTS };
static const char *const FOREST_TEST_SITES[FOREST_TESTS] = {"forest_claim", "forest_wild_spawn", "forest_heat"};
static const unsigned char FOREST_WORD_TESTS[FOREST_WORDS] = {1 << FOREST_TEST_CLAIM, 1 << FOREST_TEST_WILD, 1 << FOREST_TEST_HEAT, 7};
static const int FOREST_CALL_ORDER[FOREST_WORDS] = {FOREST_FULLFOREST, FOREST_HEARTCLAIM, FOREST_WILDSPAWN, FOREST_HOTCONVERT};

typedef struct {
    char mod[MOD_NAME_MAX], file[MAX_PATH];
    int line, script, from, order;
    int rank;
    int terrain, word, target;
} ForestWord;

static ForestWord *forestWords;
static int forestWordCount, forestWordCapacity, forestWordOrder;
static unsigned char terrainHeartClaims[FOREST_TABLE_SIZE], terrainWildSpawns[FOREST_TABLE_SIZE];
static int terrainHotConverts[FOREST_TABLE_SIZE];
static int forestTestHooks[FOREST_TESTS];
static int destroytoHooks;

static int forestWordLater(const ForestWord *a, const ForestWord *b) { return a->rank != b->rank ? a->rank > b->rank : a->order > b->order; }

static unsigned char *forestTerrainRecord(int terrain) {
    return terrain >= 0 && terrain < TERRAIN_TABLE_MAX ? GAME_PTR(RVA_TERRAIN_TABLE) + (size_t)terrain * TERRAIN_SIZE : NULL;
}

static void rebuildForestWords(void) {
    static int hotFrom[FOREST_TABLE_SIZE];
    memset(terrainHeartClaims, 0, sizeof terrainHeartClaims);
    memset(terrainWildSpawns, 0, sizeof terrainWildSpawns);
    memset(hotFrom, 0, sizeof hotFrom);
    for (int i = 0; i < forestWordCount; i++) {
        const ForestWord *w = &forestWords[i];
        if (w->from == FOREST_EARLIER_PASS || w->terrain < 0 || w->terrain >= TERRAIN_TABLE_MAX) continue;
        if (w->word == FOREST_HEARTCLAIM || w->word == FOREST_FULLFOREST) terrainHeartClaims[w->terrain] = 1;
        if (w->word == FOREST_WILDSPAWN || w->word == FOREST_FULLFOREST) terrainWildSpawns[w->terrain] = 1;
        if ((w->word == FOREST_HOTCONVERT || w->word == FOREST_FULLFOREST) && (!hotFrom[w->terrain] || forestWordLater(w, &forestWords[hotFrom[w->terrain] - 1])))
            hotFrom[w->terrain] = i + 1;
        unsigned char *record = w->word == FOREST_FULLFOREST ? forestTerrainRecord(w->terrain) : NULL;
        if (record) {
            unsigned long long flags;
            memcpy(&flags, record + TERRAIN_FLAGS, sizeof flags);
            flags |= TERRAIN_FLAG_FOREST;
            memcpy(record + TERRAIN_FLAGS, &flags, sizeof flags);
        }
    }
    for (int t = 0; t < FOREST_TABLE_SIZE; t++) terrainHotConverts[t] = hotFrom[t] ? forestWords[hotFrom[t] - 1].target : -1;
}

#if defined(__APPLE__) || defined(COE5SE_COPY_SELFCHECK)
static int terrainSpawnsWild(int terrain) {
    return terrain == TERRAIN_FOREST || terrain == TERRAIN_ANCIENT_FOREST || (terrain >= 0 && terrain < FOREST_TABLE_SIZE && terrainWildSpawns[terrain]);
}
#endif

static void markScriptForestRestarting(const char *mod) {
    for (int i = 0; i < forestWordCount; i++)
        if (forestWords[i].script && !forestWords[i].from && !strcmp(forestWords[i].mod, mod)) forestWords[i].from = FOREST_EARLIER_PASS;
}

// ---- The patched tests: Windows ---------------------------------------------------------------------
#ifndef __APPLE__
#define CLAIM_TEST_SIZE  10
#define WILD_TEST_SIZE   12
#define HEAT_TEST_SIZE   5

static unsigned char *putForestRel(unsigned char *p, const unsigned char *out, const unsigned char *stub, const unsigned char *target) {
    int rel = (int)(target - (stub + (p - out) + 4));
    memcpy(p, &rel, 4);
    return p + 4;
}

static size_t writeClaimStubWin(unsigned char *out, const unsigned char *stub, const unsigned char *at, uintptr_t table) {
    unsigned char *p = out;
    *p++ = 0x46; *p++ = 0x0f; *p++ = 0xb7; *p++ = 0x9c; *p++ = 0x28;
    memcpy(p, at + 5, 4);
    p += 4;
    *p++ = 0x41; *p++ = 0x81; *p++ = 0xfb;
    int max = TERRAIN_TABLE_MAX;
    memcpy(p, &max, 4);
    p += 4;
    *p++ = 0x73; *p++ = 0x11;
    *p++ = 0x49; *p++ = 0xba;
    memcpy(p, &table, 8);
    p += 8;
    *p++ = 0x43; *p++ = 0x80; *p++ = 0x3c; *p++ = 0x1a; *p++ = 0x00;
    *p++ = 0x75; *p++ = (unsigned char)(CLAIM_TEST_SIZE + 5);
    memcpy(p, at, CLAIM_TEST_SIZE);
    p += CLAIM_TEST_SIZE;
    *p++ = 0xe9;
    p = putForestRel(p, out, stub, at + CLAIM_TEST_SIZE);
    *p++ = 0x45; *p++ = 0x39; *p++ = 0xdb;
    *p++ = 0xe9;
    p = putForestRel(p, out, stub, at + CLAIM_TEST_SIZE);
    return (size_t)(p - out);
}

static size_t writeWildStubWin(unsigned char *out, const unsigned char *stub, const unsigned char *at, uintptr_t table) {
    unsigned char *p = out;
    *p++ = 0x81; *p++ = 0xf9;
    int max = TERRAIN_TABLE_MAX;
    memcpy(p, &max, 4);
    p += 4;
    *p++ = 0x73; *p++ = 0x10;
    *p++ = 0x48; *p++ = 0xb8;
    memcpy(p, &table, 8);
    p += 8;
    *p++ = 0x80; *p++ = 0x3c; *p++ = 0x08; *p++ = 0x00;
    *p++ = 0x75; *p++ = (unsigned char)(WILD_TEST_SIZE + 5);
    memcpy(p, at, WILD_TEST_SIZE);
    p += WILD_TEST_SIZE;
    *p++ = 0xe9;
    p = putForestRel(p, out, stub, at + WILD_TEST_SIZE);
    *p++ = 0x31; *p++ = 0xc0;
    *p++ = 0xe9;
    p = putForestRel(p, out, stub, at + WILD_TEST_SIZE);
    return (size_t)(p - out);
}

static size_t writeHeatStubWin(unsigned char *out, const unsigned char *stub, const unsigned char *at, uintptr_t table) {
    unsigned char *p = out;
    *p++ = 0x41; *p++ = 0x81; *p++ = 0xf8;
    int max = TERRAIN_TABLE_MAX;
    memcpy(p, &max, 4);
    p += 4;
    *p++ = 0x73; *p++ = 0x19;
    *p++ = 0x48; *p++ = 0xb8;
    memcpy(p, &table, 8);
    p += 8;
    *p++ = 0x42; *p++ = 0x8b; *p++ = 0x04; *p++ = 0x80;
    *p++ = 0x85; *p++ = 0xc0;
    *p++ = 0x78; *p++ = 0x07;
    *p++ = 0x8b; *p++ = 0xd8;
    *p++ = 0xe9;
    p = putForestRel(p, out, stub, GAME_PTR(RVA_HEAT_TERRAIN_CHOSEN));
    *p++ = 0x45; *p++ = 0x85; *p++ = 0xc0;
    *p++ = 0x0f; *p++ = 0x85;
    p = putForestRel(p, out, stub, at + HEAT_TEST_SIZE + (signed char)at[HEAT_TEST_SIZE - 1]);
    *p++ = 0xe9;
    p = putForestRel(p, out, stub, at + HEAT_TEST_SIZE);
    return (size_t)(p - out);
}

static int patchForestTest(int test) {
    static unsigned char *page;
    static size_t used;
    const char *site = FOREST_TEST_SITES[test];
    if (!gameCodeMatches) {
        writeLog("hook_skipped", "site=%s reason=game_code_differs", site);
        return 0;
    }
    if (!page) {
        if (!(page = allocNear(gameBase + RVA_TEXT_SECTION))) {
            writeLog("hook_skipped", "site=%s reason=no_near_page", site);
            return 0;
        }
        makePagesReadExecute(page, 0x1000);
    }
    if (used + FOREST_STUB_BYTES > 0x1000) {
        writeLog("hook_skipped", "site=%s reason=no_stub_room", site);
        return 0;
    }
    unsigned char *stub = page + used, code[FOREST_STUB_BYTES];
    unsigned char *at = GAME_PTR(test == FOREST_TEST_CLAIM ? RVA_CLAIM_FOREST_TEST : test == FOREST_TEST_WILD ? RVA_WILD_SPAWN_TERRAIN_TEST : RVA_HEAT_FOREST_TEST);
    size_t size = test == FOREST_TEST_CLAIM ? CLAIM_TEST_SIZE : test == FOREST_TEST_WILD ? WILD_TEST_SIZE : HEAT_TEST_SIZE;
    size_t length = test == FOREST_TEST_CLAIM ? writeClaimStubWin(code, stub, at, (uintptr_t)terrainHeartClaims)
                  : test == FOREST_TEST_WILD  ? writeWildStubWin(code, stub, at, (uintptr_t)terrainWildSpawns)
                                              : writeHeatStubWin(code, stub, at, (uintptr_t)terrainHotConverts);
    unsigned char jump[WILD_TEST_SIZE];
    memset(jump, 0x90, sizeof jump);
    jump[0] = 0xe9;
    int rel = (int)(stub - (at + 5));
    memcpy(jump + 1, &rel, 4);
    if (!writeGameCode(stub, code, length) || !writeGameCode(at, jump, size)) {
        writeLog("hook_skipped", "site=%s reason=code_unwritable", site);
        return 0;
    }
    used += FOREST_STUB_BYTES;
    return 1;
}
#else
// ---- The patched tests: Mac -------------------------------------------------------------------------
#define FOREST_TEST_STUB_WORDS 14
#define HEAT_STUB_WORDS        16
#define HEAT_TARGET_REGISTER   24
#define MAC_STR_X10_BELOW      0xf8170feau
#define MAC_LDR_X10_BACK       0xf84907eau

static uint32_t forestBranch(const unsigned char *from, const unsigned char *to) { return 0x14000000u | ((uint32_t)((to - from) >> 2) & 0x3ffffffu); }

static void writeForestTestStubMac(uint32_t code[FOREST_TEST_STUB_WORDS], const unsigned char *stub, const unsigned char *at, uint64_t table) {
    uint32_t game;
    memcpy(&game, at, 4);
    uint32_t reg = (game >> 5) & 31u;
    code[0] = MAC_STR_X10_BELOW;
    code[1] = 0x53007c0au | ((uint32_t)FOREST_TABLE_SHIFT << 16) | (reg << 5);
    code[2] = 0x3500000au | (7u << 5);
    code[3] = 0x5800000au | (9u << 5);
    code[4] = 0x3860494au | (reg << 16);
    code[5] = 0x3400000au | (4u << 5);
    code[6] = MAC_LDR_X10_BACK;
    code[7] = 0x6b1f03ffu;
    code[8] = forestBranch(stub + 32, at + 4);
    code[9] = MAC_LDR_X10_BACK;
    code[10] = game;
    code[11] = forestBranch(stub + 44, at + 4);
    memcpy(&code[12], &table, 8);
}

static void writeHeatStubMac(uint32_t code[HEAT_STUB_WORDS], const unsigned char *stub, const unsigned char *at, const unsigned char *chosen, uint64_t table) {
    uint32_t game, next;
    memcpy(&game, at, 4);
    memcpy(&next, at + 4, 4);
    uint32_t reg = (game >> 5) & 31u;
    code[0] = MAC_STR_X10_BELOW;
    code[1] = 0x53007c0au | ((uint32_t)FOREST_TABLE_SHIFT << 16) | (reg << 5);
    code[2] = 0x3500000au | (8u << 5);
    code[3] = 0x5800000au | (11u << 5);
    code[4] = 0xb860594au | (reg << 16);
    code[5] = 0x37f8000au | (5u << 5);
    code[6] = 0x2a0a03e0u | HEAT_TARGET_REGISTER;
    code[7] = MAC_LDR_X10_BACK;
    code[8] = next;
    code[9] = forestBranch(stub + 36, chosen);
    code[10] = MAC_LDR_X10_BACK;
    code[11] = game;
    code[12] = forestBranch(stub + 48, at + 4);
    code[13] = 0xd503201fu;
    memcpy(&code[14], &table, 8);
}

static int branchToForestStub(unsigned char *at, unsigned char *stub, const void *code, size_t size) {
    uint32_t branch = forestBranch(at, stub);
    return writeGameCode(stub, code, size) && writeGameCode(at, &branch, 4);
}

static int patchForestTest(int test) {
    const char *site = FOREST_TEST_SITES[test];
    if (!gameCodeMatches) {
        writeLog("hook_skipped", "site=%s reason=game_code_differs", site);
        return 0;
    }
    int count = test == FOREST_TEST_WILD ? WILD_SPAWN_MAC_TESTS : 1;
    size_t words = test == FOREST_TEST_HEAT ? HEAT_STUB_WORDS : FOREST_TEST_STUB_WORDS;
    unsigned char *stubs = takePadding(sizeof(uint32_t) * words * (size_t)count);
    if (!stubs) {
        writeLog("hook_skipped", "site=%s reason=no_padding", site);
        return 0;
    }
    for (int k = 0; k < count; k++) {
        unsigned char *stub = stubs + sizeof(uint32_t) * words * (size_t)k;
        uint32_t code[HEAT_STUB_WORDS];
        unsigned char *at;
        if (test == FOREST_TEST_HEAT) {
            at = GAME_PTR(RVA_HEAT_FOREST_TEST);
            writeHeatStubMac(code, stub, at, GAME_PTR(RVA_HEAT_TERRAIN_CHOSEN), (uint64_t)(uintptr_t)terrainHotConverts);
        } else {
            at = test == FOREST_TEST_CLAIM ? GAME_PTR(RVA_CLAIM_FOREST_TEST)
               : k == 0                    ? GAME_PTR(RVA_WILD_SPAWN_TERRAIN_TEST)
                                           : GAME_PTR(RVA_WILD_SPAWN_TERRAIN_TEST_2) + (size_t)(k - 1) * WILD_SPAWN_MAC_TEST_STRIDE;
            writeForestTestStubMac(code, stub, at, (uint64_t)(uintptr_t)(test == FOREST_TEST_CLAIM ? terrainHeartClaims : terrainWildSpawns));
        }
        if (!branchToForestStub(at, stub, code, sizeof(uint32_t) * words)) {
            writeLog("hook_skipped", "site=%s reason=code_unwritable copy=%d", site, k);
            return 0;
        }
    }
    return 1;
}
#endif

// @platform-split forest_tests: Windows jumps to a stub in a near page; Mac branches to one in the header padding, the wild spawn's ten copies each
static int installForestTest(int test) {
    if (forestTestHooks[test]) return forestTestHooks[test] > 0;
    rebuildForestWords();
    forestTestHooks[test] = patchForestTest(test) ? 1 : -1;
    writeResultLog(forestTestHooks[test] > 0, "forest_test_installed", "site=%s ok=%d", FOREST_TEST_SITES[test], forestTestHooks[test] > 0);
    return forestTestHooks[test] > 0;
}

// ---- The destroyto fix ---------------------------------------------------------------------------
static int terrainDestroyTo(int terrain) {
    const unsigned char *record = terrain >= 0 && terrain <= G32(RVA_LAST_TERRAIN) ? forestTerrainRecord(terrain) : NULL;
    for (int i = 0; record && i < TERRAIN_ABILITY_PAIRS; i++) {
        long long key, value;
        memcpy(&key, record + TERRAIN_ABILITIES + 16 * i, 8);
        memcpy(&value, record + TERRAIN_ABILITIES + 16 * i + 8, 8);
        if (!key) break;
        if (key == TERRAIN_ABILITY_DESTROYTO) return value <= 0 ? -1 : value < G32(RVA_LAST_TERRAIN) ? (int)value : G32(RVA_LAST_TERRAIN);
    }
    return -1;
}

static int forestSquareTerrain(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_W) || y >= G32(RVA_MAP_H)) return -1;
    return (short)G16(RVA_SQUARE_RECORDS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE + SQUARE_TERRAIN_TYPE);
}

static int destroytoDrainHook(int x, int y) {
    int terrain = forestSquareTerrain(x, y), to = terrainDestroyTo(terrain);
    if (to < 0) return GAME_FN(int (*)(int, int), RVA_DRAINED_TERRAIN)(x, y);
    GAME_FN(void (*)(int, int, int), RVA_POPULATE_SET_HOLDIT)(x, y, 0);
    int taken = GAME_FN(int (*)(int, int, int, int), RVA_SQUARE_SACRIFICE_TAKEN)(-1, x, y, 1);
    if (taken > 0) return taken;
    GAME_FN(void (*)(int, int, int), RVA_CHANGE_SQUARE_TERRAIN)(x, y, to);
    writeLog("destroyto_applied", "by=drain x=%d y=%d terrain=%d to=%d", x, y, terrain, to);
    return 1;
}

static int destroytoWitherHook(int x, int y) {
    int terrain = forestSquareTerrain(x, y), to = terrainDestroyTo(terrain);
    if (to < 0) return GAME_FN(int (*)(int, int), RVA_HADES_WITHER_SQUARE)(x, y);
    GAME_FN(void (*)(int, int, int), RVA_CHANGE_SQUARE_TERRAIN)(x, y, to);
    writeLog("destroyto_applied", "by=wither x=%d y=%d terrain=%d to=%d", x, y, terrain, to);
    return 1;
}

static int installDestroytoHooks(void) {
    if (destroytoHooks) return destroytoHooks > 0;
    int ok = pointCustomPlaneCall(RVA_DRAINED_TERRAIN_CALL, RVA_DRAINED_TERRAIN, (void *)destroytoDrainHook, "destroyto_drain") +
             pointCustomPlaneCall(RVA_CHOP_WITHER_CALL, RVA_HADES_WITHER_SQUARE, (void *)destroytoWitherHook, "destroyto_chop") +
             pointCustomPlaneCall(RVA_HADES_WITHER_CALL, RVA_HADES_WITHER_SQUARE, (void *)destroytoWitherHook, "destroyto_hades");
    destroytoHooks = ok == 3 ? 1 : -1;
    writeResultLog(ok == 3, "destroyto_hooks_installed", "ok=%d of 3", ok);
    return destroytoHooks > 0;
}

static void noteTerrainRecordWritten(int terrain) {
    if (!destroytoHooks && terrainDestroyTo(terrain) >= 0) installDestroytoHooks();
}

// ---- Declarations: the words' and the Lua call's ---------------------------------------------------
static int forestProblemText(const char *reason, const char *what, char *out, size_t size) {
    if (!strcmp(reason, "no_terrain_block")) snprintf(out, size, "%.20s only after selectterr", what);
    else if (!strcmp(reason, "bad_terrain")) snprintf(out, size, "hotconvert needs a terrain number");
    else if (!strcmp(reason, "unknown_terrain")) snprintf(out, size, "No terrain %.20s", what);
    else if (!strcmp(reason, "extra_text")) snprintf(out, size, "%.20s takes nothing after it", what);
    else if (!strcmp(reason, "bad_list")) snprintf(out, size, "coe5.setTerrainForest: fullforest, heartclaim, wildspawn or hotconvert");
    else return 0;
    return 1;
}

static const char *declareForestWord(const char *mod, int terrain, int word, int target, int script, const char *file, int line) {
    if (terrain < 0 || terrain >= TERRAIN_TABLE_MAX) return "unknown_terrain";
    if (word == FOREST_HOTCONVERT && (target < 0 || target >= TERRAIN_TABLE_MAX)) return "unknown_terrain";
    for (int test = 0; test < FOREST_TESTS; test++)
        if ((FOREST_WORD_TESTS[word] >> test & 1) && !installForestTest(test)) return "hook_not_installed";
    if (!growList((void **)&forestWords, &forestWordCapacity, forestWordCount + 1, sizeof *forestWords)) return "out_of_memory";
    ForestWord *w = &forestWords[forestWordCount++];
    memset(w, 0, sizeof *w);
    snprintf(w->mod, sizeof w->mod, "%s", mod);
    snprintf(w->file, sizeof w->file, "%s", file);
    w->line = line, w->script = script, w->from = script ? 0 : FOREST_FROM_WORDS, w->order = ++forestWordOrder, w->rank = scatterModRank(mod);
    w->terrain = terrain, w->word = word, w->target = word == FOREST_HOTCONVERT ? target : word == FOREST_FULLFOREST ? TERRAIN_SAVANNA : -1;
    writeLog("forest_word_declared", "mod=%s word=%s terrain=%d target=%d script=%d", mod, FOREST_WORD_NAMES[word], terrain, w->target, script);
    rebuildForestWords();
    return NULL;
}

// ---- The words -----------------------------------------------------------------------------------
static void refuseForestLine(const char *word, int mod, const char *reason, const char *what, const char *args) {
    const char *name = spellModName(mod), *file = (const char *)GAME_PTR(RVA_EVENT_FILE);
    int line = G32(RVA_MOD_LINE);
    refuseC5mLine(word, name, reason, "line=%d text=%.60s", line, args);
    char text[96];
    if (forestProblemText(reason, what, text, sizeof text)) addPlaneProblem(0, name, file, line, "%s", text);
}

static void readForestLine(int word, const char *args, int mod, int level) {
    if (level != 2) return;
    const char *name = FOREST_WORD_NAMES[word], *p = args, *what = name;
    int terrain = G32(RVA_TERRAIN_CURRENT);
    long long target = -1;
    char number[24] = "";
    const char *reason = mod < 0 || mod > G32(RVA_MOD_LAST_INDEX)                                  ? "not_in_a_mod"
                       : terrain < 0 || G32(RVA_CLASS_CURRENT) >= 0                                ? "no_terrain_block"
                       : word == FOREST_HOTCONVERT && (!readSpellNumber(&p, &target) || target < 0) ? "bad_terrain"
                       : NULL;
    if (!reason) {
        skipSpaces(&p);
        if (*p && *p != '\r' && *p != '\n' && *p != '#') reason = word == FOREST_HOTCONVERT ? "bad_terrain" : "extra_text";
    }
    if (!reason && word == FOREST_HOTCONVERT && target >= TERRAIN_TABLE_MAX)
        reason = "unknown_terrain", snprintf(number, sizeof number, "%lld", target), what = number;
    if (!reason) reason = declareForestWord(spellModName(mod), terrain, word, (int)target, 0, (const char *)GAME_PTR(RVA_EVENT_FILE), G32(RVA_MOD_LINE));
    if (reason) refuseForestLine(name, mod, reason, what, args);
}

static void heartclaimLine(const char *args, int mod, int level) { readForestLine(FOREST_HEARTCLAIM, args, mod, level); }
static void wildspawnLine(const char *args, int mod, int level) { readForestLine(FOREST_WILDSPAWN, args, mod, level); }
static void hotconvertLine(const char *args, int mod, int level) { readForestLine(FOREST_HOTCONVERT, args, mod, level); }
static void fullforestLine(const char *args, int mod, int level) { readForestLine(FOREST_FULLFOREST, args, mod, level); }

// ---- The passes ----------------------------------------------------------------------------------
static void forestWordsBeforePass(int level) {
    if (level != 2) return;
    for (int i = 0; i < forestWordCount; i++)
        if (forestWords[i].from) forestWords[i].from = FOREST_EARLIER_PASS;
}

static void forestWordsAfterPass(int level) {
    if (level != 2) return;
    int kept = 0;
    for (int i = 0; i < forestWordCount; i++) {
        ForestWord *w = &forestWords[i];
        if (w->from == FOREST_FROM_WORDS && w->word == FOREST_HOTCONVERT && w->target > G32(RVA_LAST_TERRAIN)) {
            char what[24], text[96];
            snprintf(what, sizeof what, "%d", w->target);
            refuseC5mLine(FOREST_WORD_NAMES[w->word], w->mod, "unknown_terrain", "line=%d terrain=%d target=%d", w->line, w->terrain, w->target);
            if (forestProblemText("unknown_terrain", what, text, sizeof text)) addPlaneProblem(0, w->mod, w->file, w->line, "%s", text);
            continue;
        }
        if (w->from == FOREST_EARLIER_PASS) continue;
        if (kept != i) forestWords[kept] = *w;
        kept++;
    }
    forestWordCount = kept;
    rebuildForestWords();
    for (int t = 0; !destroytoHooks && t <= G32(RVA_LAST_TERRAIN) && t < TERRAIN_TABLE_MAX; t++) noteTerrainRecordWritten(t);
}

// ---- Lua ---------------------------------------------------------------------------------------
static void refuseForestCall(lua_State *L, const char *reason, const char *what) {
    const char *mod = runningLuaMod();
    refuseLuaCall(mod, "coe5.setTerrainForest", reason, "what=%.80s", what[0] ? what : "-");
    char text[96];
    if (forestProblemText(reason, what, text, sizeof text)) addPlaneProblem(1, mod ? mod : "?", luaRunning ? luaRunning->path : "?", luaCallerLine(L), "%s", text);
}

static int lSetTerrainForest(lua_State *L) {
    const char *mod = runningLuaMod();
    char what[48] = "";
    int terrain = luaTerrainArg(L, 1), on[FOREST_WORDS] = {0}, target = -1, words = 0;
    const char *reason = !luaScriptsLoading ? "not_at_load"
                       : terrain == -1 ? "no_terrain"
                       : terrain < 0 || terrain >= TERRAIN_TABLE_MAX ? "unknown_terrain"
                       : !lua_istable(L, 2) ? "bad_list"
                       : NULL;
    if (!reason) {
        lua_pushnil(L);
        while (!reason && lua_next(L, 2)) {
            int word = -1;
            for (int k = 0; lua_type(L, -2) == LUA_TSTRING && k < FOREST_WORDS; k++)
                if (!strcmp(lua_tostring(L, -2), FOREST_WORD_NAMES[k])) word = k;
            if (word < 0) reason = "bad_list", snprintf(what, sizeof what, "%s", lua_type(L, -2) == LUA_TSTRING ? lua_tostring(L, -2) : "?");
            else if (word == FOREST_HOTCONVERT && lua_isboolean(L, -1) && !lua_toboolean(L, -1)) on[word] = 0;
            else if (word == FOREST_HOTCONVERT && !lua_isinteger(L, -1) && lua_type(L, -1) != LUA_TSTRING)
                reason = "bad_list", snprintf(what, sizeof what, "%s", FOREST_WORD_NAMES[word]);
            else if (word == FOREST_HOTCONVERT) {
                target = luaTerrainArg(L, lua_gettop(L));
                if (target < 0 || target >= TERRAIN_TABLE_MAX || target > G32(RVA_LAST_TERRAIN)) reason = "unknown_terrain", snprintf(what, sizeof what, "%s", luaL_tolstring(L, -1, NULL)), lua_pop(L, 1);
                else on[word] = 1;
            } else if (!lua_isboolean(L, -1)) reason = "bad_list", snprintf(what, sizeof what, "%s", FOREST_WORD_NAMES[word]);
            else on[word] = lua_toboolean(L, -1);
            lua_pop(L, 1);
        }
        if (reason) lua_pop(L, 1);
    }
    for (int k = 0; k < FOREST_WORDS; k++) words += on[k];
    if (!reason && !words) reason = "bad_list";
    for (int test = 0; !reason && test < FOREST_TESTS; test++)
        for (int k = 0; k < FOREST_WORDS; k++)
            if (on[k] && (FOREST_WORD_TESTS[k] >> test & 1) && !installForestTest(test)) reason = "hook_not_installed";
    int line = luaCallerLine(L);
    for (int i = 0; i < FOREST_WORDS && !reason; i++) {
        int word = FOREST_CALL_ORDER[i];
        if (on[word] && (reason = declareForestWord(mod ? mod : "?", terrain, word, target, 1, luaRunning ? luaRunning->path : "?", line)))
            snprintf(what, sizeof what, "%s", FOREST_WORD_NAMES[word]);
    }
    if (reason) refuseForestCall(L, reason, what);
    lua_pushboolean(L, !reason);
    return 1;
}

static void forestWordsInit(void) {
    int words = apiAddModCommand("heartclaim", heartclaimLine) + apiAddModCommand("wildspawn", wildspawnLine) + apiAddModCommand("hotconvert", hotconvertLine) +
                apiAddModCommand("fullforest", fullforestLine);
    for (int t = 0; t < FOREST_TABLE_SIZE; t++) terrainHotConverts[t] = -1;
    writeResultLog(words == 4, "forest_words_installed", "words=%d of 4", words);
}
