// @platform-split copy_hooks_mac: Mac hooks the copies of five sites' functions (Windows calls the functions)
#pragma once
#ifdef __APPLE__

enum { COPY_COUNT, COPY_LIBRARY, COPY_TEST, COPY_UNIT_TEST };
typedef struct {
    CallSite site;
    int kind;
    unsigned start;
    unsigned exits[2];
    unsigned char args[3];
    unsigned char result;
} CopyHookMac;

static const CopyHookMac COPY_HOOKS_MAC[] = {
    {SITE_AI_COUNT_CITADELS, COPY_COUNT, RVA_CITADELS_MOVE_COPY_START, {RVA_CITADELS_MOVE_COPY_END}, {19}, 26},
    {SITE_AI_COUNT_CITADELS, COPY_COUNT, RVA_CITADELS_MONTHLY_COPY_START, {RVA_CITADELS_MONTHLY_COPY_END}, {20}, 28},
    {SITE_AI_COUNT_CITADELS, COPY_COUNT, RVA_CITADELS_STATS_COPY_START, {RVA_CITADELS_STATS_COPY_END}, {19}, 22},
    {SITE_SQUARE_LIBRARY_LEVEL, COPY_LIBRARY, RVA_LIBRARY_USE_COPY_CALL, {RVA_LIBRARY_USE_COPY_CALL + 8}, {0, 1, 2}, 0},
    {SITE_SQUARE_LIBRARY_LEVEL, COPY_LIBRARY, RVA_LIBRARY_SCREEN_COPY_CALL, {RVA_LIBRARY_SCREEN_COPY_CALL + 8}, {0, 1, 2}, 0},
    {SITE_SQUARE_LIBRARY_LEVEL, COPY_LIBRARY, RVA_LIBRARY_PANEL_COPY_CALL, {RVA_LIBRARY_PANEL_COPY_CALL + 8}, {0, 1, 2}, 0},
    {SITE_SQUARE_LIBRARY_LEVEL, COPY_LIBRARY, RVA_LIBRARY_MAP_COPY_CALL, {RVA_LIBRARY_MAP_COPY_CALL + 8}, {0, 1, 2}, 0},
    {SITE_SQUARE_LIBRARY_LEVEL, COPY_LIBRARY, RVA_LIBRARY_TOTAL_COPY_CALL, {RVA_LIBRARY_TOTAL_COPY_CALL + 8}, {0, 1, 2}, 0},
    {SITE_SQUARE_LIBRARY_LEVEL, COPY_LIBRARY, RVA_LIBRARY_INFO_COPY_CALL, {RVA_LIBRARY_INFO_COPY_CALL + 8}, {0, 1, 2}, 0},
    {SITE_SQUARE_LIBRARY_LEVEL, COPY_LIBRARY, RVA_LIBRARY_AI_COPY_CALL, {RVA_LIBRARY_AI_COPY_CALL + 8}, {0, 1, 2}, 0},
    {SITE_SQUARE_OPEN_LAND, COPY_TEST, RVA_OPEN_LAND_MARK_START, {RVA_OPEN_LAND_MARK_CLOSED, RVA_OPEN_LAND_MARK_OPEN}, {23, 24}, 0},
    {SITE_SQUARE_OPEN_LAND, COPY_TEST, RVA_OPEN_LAND_RITUAL_1_START, {RVA_OPEN_LAND_RITUAL_1_CLOSED, RVA_OPEN_LAND_RITUAL_1_OPEN}, {19, 28}, 0},
    {SITE_SQUARE_OPEN_LAND, COPY_TEST, RVA_OPEN_LAND_RITUAL_2_START, {RVA_OPEN_LAND_RITUAL_2_CLOSED, RVA_OPEN_LAND_RITUAL_2_OPEN}, {27, 20}, 0},
    {SITE_SQUARE_OPEN_LAND, COPY_TEST, RVA_OPEN_LAND_RITUAL_3_START, {RVA_OPEN_LAND_RITUAL_3_CLOSED, RVA_OPEN_LAND_RITUAL_3_OPEN}, {19, 20}, 0},
    {SITE_UNIT_INCAPACITATED, COPY_UNIT_TEST, RVA_INCAP_ADVANCE_TEST, {RVA_INCAP_ADVANCE_ACTS, RVA_INCAP_ADVANCE_HELD}, {19}, 0},
    {SITE_UNIT_INCAPACITATED, COPY_UNIT_TEST, RVA_INCAP_DOATTACK_TEST, {RVA_INCAP_DOATTACK_ACTS, RVA_INCAP_DOATTACK_HELD}, {23}, 0},
    {SITE_UNIT_INCAPACITATED, COPY_UNIT_TEST, RVA_INCAP_WEAPON_ATTACK_A_TEST, {RVA_INCAP_WEAPON_ATTACK_A_ACTS, RVA_INCAP_WEAPON_ATTACK_A_HELD}, {19}, 0},
    {SITE_UNIT_INCAPACITATED, COPY_UNIT_TEST, RVA_INCAP_WEAPON_ATTACK_B_TEST, {RVA_INCAP_WEAPON_ATTACK_B_ACTS, RVA_INCAP_WEAPON_ATTACK_B_HELD}, {19}, 0},
    {SITE_UNIT_INCAPACITATED, COPY_UNIT_TEST, RVA_INCAP_WEAPON_HIT_TEST, {RVA_INCAP_WEAPON_HIT_RESUME}, {20}, 8},
    {SITE_UNIT_INCAPACITATED, COPY_UNIT_TEST, RVA_INCAP_TOKEN_DRAW, {RVA_INCAP_TOKEN_DRAW_ACTS, RVA_INCAP_TOKEN_DRAW_HELD}, {9}, 0},
};
static const unsigned FIRE_COPY_CALLS_MAC[] = {
    RVA_FIRE_COPY_1_CALL, RVA_FIRE_COPY_2_CALL, RVA_FIRE_COPY_3_CALL, RVA_FIRE_COPY_4_CALL, RVA_FIRE_COPY_5_CALL, RVA_FIRE_COPY_6_CALL,
    RVA_FIRE_COPY_7_CALL, RVA_FIRE_COPY_8_CALL, RVA_FIRE_COPY_9_CALL, RVA_FIRE_COPY_10_CALL, RVA_FIRE_COPY_11_CALL, RVA_FIRE_COPY_12_CALL,
    RVA_FIRE_COPY_13_CALL,
};
#define FIRE_COPY_CALL_COUNT ((int)(sizeof FIRE_COPY_CALLS_MAC / sizeof FIRE_COPY_CALLS_MAC[0]))
#define COPY_HOOK_COUNT ((int)(sizeof COPY_HOOKS_MAC / sizeof COPY_HOOKS_MAC[0]))
#define COPY_EXITS_MAX 40

typedef struct {
    StepHook step;
    const CopyHookMac *row;
    uint64_t exit[2];
} CopyHookState;
static CopyHookState copyHooksMac[COPY_HOOK_COUNT];
static struct { unsigned rva; unsigned char *jump; } copyExitsMac[COPY_EXITS_MAX];
static int copyExitCountMac;

static int libraryEnchantOf(int owner, int x, int y) {
    return GAME_FN(int (*)(int, int, int, int), RVA_SQUARE_ENCHANT_VALUE)(owner, x, y, LIBRARY_ENCHANT_KIND);
}

static void copyHookRun(StepContext *ctx, const StepHook *hook) {
    const CopyHookState *h = (const CopyHookState *)hook;
    const CopyHookMac *m = h->row;
    StubData *data = &coe5seStubData[m->site];
    data->sp = (uintptr_t)ctx->sp;
    memcpy(&data->d8, &ctx->q[8], 8);
    memcpy(&data->d9, &ctx->q[9], 8);
    void *head = *siteHead(m->site);
    int a = (int)ctx->x[m->args[0]];
    int exit = 0;
    if (m->kind == COPY_COUNT) {
        ctx->x[m->result] = (uint32_t)((int (*)(int))head)(a);
    } else if (m->kind == COPY_LIBRARY) {
        int b = (int)ctx->x[m->args[1]], c = (int)ctx->x[m->args[2]];
        uint32_t enchant = (uint32_t)libraryEnchantOf(a, b, c), own = (uint32_t)libraryLevelOf(a, b, c);
        uint32_t level = (uint32_t)((int (*)(int, int, int))head)(a, b, c);
        ctx->x[0] = (uint32_t)(enchant + (level - own));
    } else if (m->kind == COPY_UNIT_TEST) {
        int held = ((int (*)(int))head)(a) != 0;
        if (m->result) ctx->x[m->result] = (uint64_t)held;
        else exit = held;
    } else {
        exit = ((int (*)(int, int))head)(a, (int)ctx->x[m->args[1]]) != 0;
    }
    ctx->pc = h->exit[exit];
}

#define INCAPACITATED_STATE_MASK 0x11100081758ULL
static int unitIncapacitatedStateMac(int unit) {
    if (unit < 0 || unit >= UNIT_TABLE_MAX) return 1;
    unsigned long long state = *(const unsigned long long *)GAME_PTR(RVA_UNITS + (size_t)unit * UNIT_SIZE);
    return (state & INCAPACITATED_STATE_MASK) != 0;
}

typedef int (*AbilityTotalFn)(int unit, int ability, int add, char *out, int outIf, int *last);
static int fireCopyAbilityTotalMac(int unit, int ability, int add, char *out, int outIf, int *last) {
    int total = GAME_FN(AbilityTotalFn, RVA_UNIT_ABILITY_TOTAL)(unit, ability, add, out, outIf, last);
    void *head = *siteHead(SITE_UNIT_FIRE_RESISTANCE);
    if (head == siteGameFunction(SITE_UNIT_FIRE_RESISTANCE)) return total;
    int game = GAME_FN(int (*)(int), RVA_UNIT_RESISTANCE_1D1280)(unit);
    return total + ((int (*)(int))head)(unit) - game;
}

static int installFireCopiesMac(void) {
    static int done;
    if (done) return done > 0;
    done = -1;
    for (int i = 0; i < FIRE_COPY_CALL_COUNT; i++)
        if (blTarget(gameBase + FIRE_COPY_CALLS_MAC[i]) != gameBase + RVA_UNIT_ABILITY_TOTAL) {
            writeLog("copy_hook_skipped", "site=unit_fire_resistance reason=not_the_expected_call address=%p", (void *)(gameBase + FIRE_COPY_CALLS_MAC[i]));
            return 0;
        }
    unsigned char *trampoline = newTrampoline((void *)fireCopyAbilityTotalMac);
    if (!trampoline) {
        writeLog("copy_hook_skipped", "site=unit_fire_resistance reason=no_padding");
        return 0;
    }
    for (int i = 0; i < FIRE_COPY_CALL_COUNT; i++) {
        uint32_t word = blTo(gameBase + FIRE_COPY_CALLS_MAC[i], trampoline);
        if (!writeGameCode(gameBase + FIRE_COPY_CALLS_MAC[i], &word, 4)) {
            writeLogError("site_failed", "site=unit_fire_resistance reason=code_unwritable address=%p", (void *)(gameBase + FIRE_COPY_CALLS_MAC[i]));
            return 0;
        }
    }
    done = 1;
    return 1;
}

static uint64_t copyExitJumpMac(unsigned rva) {
    for (int i = 0; i < copyExitCountMac; i++)
        if (copyExitsMac[i].rva == rva) return (uint64_t)(uintptr_t)copyExitsMac[i].jump;
    unsigned char *jump = copyExitCountMac < COPY_EXITS_MAX ? takePadding(8) : NULL;
    if (!jump) return 0;
    uint32_t code[2] = {STEP_LOAD_X16, 0x14000000u | ((uint32_t)(((gameBase + rva) - (jump + 4)) >> 2) & 0x3ffffffu)};
    if (!writeGameCode(jump, code, sizeof code)) return 0;
    copyExitsMac[copyExitCountMac].rva = rva, copyExitsMac[copyExitCountMac].jump = jump, copyExitCountMac++;
    return (uint64_t)(uintptr_t)jump;
}

static int copyHookExitCountMac(const CopyHookMac *m) {
    return m->kind == COPY_TEST || (m->kind == COPY_UNIT_TEST && !m->result) ? 2 : 1;
}

static int putCopyHookMac(CopyHookState *h, const CopyHookMac *m) {
    const char *name = SITES[m->site].name;
    h->row = m;
    h->step.handler = copyHookRun;
    h->step.site = m->site;
    h->step.resume = 0;
    for (int i = 0; i < copyHookExitCountMac(m); i++) {
        h->exit[i] = copyExitJumpMac(m->exits[i]);
        if (!h->exit[i]) {
            writeLog("copy_hook_skipped", "site=%s reason=no_padding address=%p", name, (void *)(gameBase + m->start));
            return 0;
        }
    }
    unsigned char *entry = takePadding(sizeof(uint32_t) * STEP_ENTRY_WORDS);
    if (!entry) {
        writeLog("copy_hook_skipped", "site=%s reason=no_padding address=%p", name, (void *)(gameBase + m->start));
        return 0;
    }
    return putStepEntryMac(gameBase + m->start, entry, &h->step, name);
}

static int installCopyHooksMac(CallSite site) {
    if (!gameCodeMatches) {
        writeLog("site_skipped", "site=%s reason=not_the_expected_code", SITES[site].name);
        return 0;
    }
    *siteHead(site) = siteGameFunction(site);
    if (site == SITE_UNIT_FIRE_RESISTANCE) {
        int ok = installFireCopiesMac();
        if (ok) writeLog("copy_hooks_installed", "site=%s hooks=%d", SITES[site].name, FIRE_COPY_CALL_COUNT);
        return ok;
    }
    int made = 0;
    for (int i = 0; i < COPY_HOOK_COUNT; i++) {
        if (COPY_HOOKS_MAC[i].site != site) continue;
        if (!putCopyHookMac(&copyHooksMac[i], &COPY_HOOKS_MAC[i])) return 0;
        made++;
    }
    writeLog("copy_hooks_installed", "site=%s hooks=%d", SITES[site].name, made);
    return made > 0;
}
#endif
