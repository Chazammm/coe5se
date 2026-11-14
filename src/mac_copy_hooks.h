// @platform-split copy_hooks_mac: Mac hooks the copies of three sites' functions (Windows calls the functions)
#pragma once
#ifdef __APPLE__

enum { COPY_COUNT, COPY_LIBRARY, COPY_TEST };
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
};
#define COPY_HOOK_COUNT ((int)(sizeof COPY_HOOKS_MAC / sizeof COPY_HOOKS_MAC[0]))
#define COPY_EXITS_MAX 24

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
    } else {
        exit = ((int (*)(int, int))head)(a, (int)ctx->x[m->args[1]]) != 0;
    }
    ctx->pc = h->exit[exit];
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

static int putCopyHookMac(CopyHookState *h, const CopyHookMac *m) {
    const char *name = SITES[m->site].name;
    h->row = m;
    h->step.handler = copyHookRun;
    h->step.site = m->site;
    h->step.resume = 0;
    for (int i = 0; i < (m->kind == COPY_TEST ? 2 : 1); i++) {
        h->exit[i] = copyExitJumpMac(m->exits[i]);
        if (!h->exit[i]) {
            writeLog("copy_hook_skipped", "site=%s reason=no_padding address=%p", name, (void *)(gameBase + m->start));
            return 0;
        }
    }
    unsigned char *stub = takePadding(sizeof(uint32_t) * STEP_STUB_WORDS);
    if (!stub) {
        writeLog("copy_hook_skipped", "site=%s reason=no_padding address=%p", name, (void *)(gameBase + m->start));
        return 0;
    }
    unsigned char *at = gameBase + m->start;
    uint32_t code[STEP_STUB_WORDS], branch = 0x14000000u | ((uint32_t)((stub - at) >> 2) & 0x3ffffffu);
    writeStepStubMac(code, (uint64_t)(uintptr_t)&h->step, (uint64_t)(uintptr_t)coe5seStepHookCall);
    if (!writeGameCode(stub, code, sizeof code) || !writeGameCode(at, &branch, 4)) {
        writeLogError("site_failed", "site=%s reason=write address=%p", name, (void *)at);
        return 0;
    }
    return 1;
}

static int installCopyHooksMac(CallSite site) {
    if (!gameCodeMatches) {
        writeLog("site_skipped", "site=%s reason=not_the_expected_code", SITES[site].name);
        return 0;
    }
    *siteHead(site) = siteGameFunction(site);
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
