// @platform-split attack_init: Windows jumps to stubs that call CoE5SE at the game's rsp; Mac uses step hooks
#pragma once

static int attackInitHooksIn;

static int attackInitAsGame(int unit, int slot, int weapon, int init) { return (void)unit, (void)slot, (void)weapon, init; }

static int attackInitOf(int unit, int slot, int weapon, int init) {
    typedef int (*InitFn)(int, int, int, int);
    return ((InitFn)siteEntry(SITE_ATTACK_INIT, (void *)attackInitAsGame))(unit, slot, weapon, init);
}

static int attackInitIsSpell(int type) {
#ifdef __APPLE__
    return isSpellAttackCopy(type);
#else
    return GAME_FN(int (*)(int), RVA_IS_SPELL_ATTACK)(type);
#endif
}

static int attackInitPlanned(int unit, int slot, int weapon, int type, int init) {
    return attackInitOf(unit, slot, attackInitIsSpell(type) ? -1 : weapon, init);
}

static const char *attackInitSpellText(int unit, const char *text, int slot) {
    static char shown[4][12];
    static unsigned next;
    int game = text ? (int)strtol(text, NULL, 10) : 0;
    int init = attackInitOf(unit, slot, -1, game);
    if (init == game && text) return text;
    char *out = shown[next++ % 4];
    snprintf(out, sizeof shown[0], "%d", init);
    return out;
}

#ifndef __APPLE__
// ---- Windows: three stubs in a page near the game ---------------------------------------------------------
enum { ATTACK_INIT_PLAN, ATTACK_INIT_CARD, ATTACK_INIT_SPELL, ATTACK_INIT_SITES };
#define ATTACK_INIT_SLOTS      (ATTACK_INIT_SITES * ATTACK_INIT_STUB_SIZE)
#define ATTACK_INIT_FUNCTIONS  (ATTACK_INIT_SLOTS + ATTACK_INIT_SITES * 8)
#define ATTACK_INIT_UNWIND     (ATTACK_INIT_FUNCTIONS + ATTACK_INIT_SITES * (int)sizeof(RUNTIME_FUNCTION))
#define ATTACK_INIT_CODE_BYTES (ATTACK_INIT_UNWIND + ATTACK_INIT_SITES * ITEM_STAT_UNWIND_SIZE)
#define ATTACK_INIT_DATA       0x1000
_Static_assert(ATTACK_INIT_CODE_BYTES <= ATTACK_INIT_DATA, "the initiative's code page");

static int attackInitPlanHook(int unit, int slot, int weapon, int type, int init) { return attackInitPlanned(unit, slot, weapon, type, init); }
static int attackInitCardHook(int unit, int slot, int weapon, int init) { return attackInitOf(unit, slot, weapon, init); }

static int putAttackInitHooks(void) {
    static const struct { unsigned int rva; unsigned char moved; } SITES[ATTACK_INIT_SITES] = {
        {RVA_AI_ZAP_INIT_TEST, 6}, {RVA_UNIT_CARD_INIT_READ, 8}, {RVA_UNIT_CARD_SPELL_INIT, 6},
    };
    void *handlers[ATTACK_INIT_SITES] = {(void *)attackInitPlanHook, (void *)attackInitCardHook, (void *)attackInitSpellText};
    const unsigned char cardArgs[] = {0x8b, 0x4c, 0x24, (unsigned char)FRAME_STATS_SCREEN_UNIT, 0x44, 0x89, 0xea, 0x45, 0x89, 0xc1,
                                      0x41, 0x89, 0xd8};
    static const unsigned char CARD_RESULT[] = {0x41, 0x89, 0xc0};
    static const unsigned char SPELL_ARGS[] = {0x44, 0x89, 0xe1, 0x45, 0x89, 0xe8}, SPELL_RESULT[] = {0x48, 0x89, 0xc2, 0x33, 0xc9};
    _Static_assert(FRAME_STATS_SCREEN_UNIT < 0x80, "an imm8 rsp offset");
    if (!gameCodeMatches) {
        writeLogError("hook_skipped", "site=attack_init reason=game_code_differs");
        return 0;
    }
    unsigned char *page = allocNearBytes(gameBase + RVA_TEXT_SECTION, 2 * ATTACK_INIT_DATA);
    if (!page) {
        writeLogError("hook_skipped", "site=attack_init reason=no_near_page");
        return 0;
    }
    RUNTIME_FUNCTION *functions = (RUNTIME_FUNCTION *)(page + ATTACK_INIT_FUNCTIONS);
    for (int i = 0; i < ATTACK_INIT_SITES; i++) {
        unsigned char *at = GAME_PTR(SITES[i].rva), *stub = page + (size_t)i * ATTACK_INIT_STUB_SIZE, *slot = page + ATTACK_INIT_SLOTS + (size_t)i * 8;
        unsigned char *unwind = page + ATTACK_INIT_UNWIND + (size_t)i * ITEM_STAT_UNWIND_SIZE;
        memcpy(slot, &handlers[i], 8);
        size_t n = 0;
        if (i == ATTACK_INIT_PLAN)
            n = buildAttackInitPlanStub(stub, stub, slot, page + ATTACK_INIT_DATA, GAME_PTR(RVA_AI_ZAP_INIT_CLAMP), GAME_PTR(RVA_AI_ZAP_INIT_NONE));
        else if (i == ATTACK_INIT_CARD)
            n = buildAttackInitCardStub(stub, stub, slot, at, SITES[i].moved, cardArgs, sizeof cardArgs, CARD_RESULT, sizeof CARD_RESULT,
                                        at + SITES[i].moved);
        else
            n = buildAttackInitCardStub(stub, stub, slot, at, SITES[i].moved, SPELL_ARGS, sizeof SPELL_ARGS, SPELL_RESULT, sizeof SPELL_RESULT,
                                        GAME_PTR(RVA_UNIT_CARD_SPELL_INIT_CALL));
        if (!n || !copyItemStatUnwind(at, unwind)) {
            writeLogError("hook_skipped", "site=attack_init_0x%x reason=stub_not_built", SITES[i].rva);
            return 0;
        }
        functions[i].BeginAddress = (DWORD)(stub - page);
        functions[i].EndAddress = (DWORD)(stub - page + n);
        functions[i].UnwindData = (DWORD)(unwind - page);
    }
    if (!RtlAddFunctionTable(functions, ATTACK_INIT_SITES, (DWORD64)(uintptr_t)page)) {
        writeLogError("hook_skipped", "site=attack_init reason=unwind_not_added");
        return 0;
    }
    makePagesReadExecute(page, ATTACK_INIT_DATA);
    makePagesReadWrite(page + ATTACK_INIT_DATA, ATTACK_INIT_DATA);
    for (int i = 0; i < ATTACK_INIT_SITES; i++) {
        unsigned char *at = GAME_PTR(SITES[i].rva), jump[ITEM_CARD_MOVED_MAX];
        buildEntryHookJump(jump, at, page + (size_t)i * ATTACK_INIT_STUB_SIZE, SITES[i].moved);
        if (!writeGameCode(at, jump, SITES[i].moved)) return 0;
    }
    return 1;
}
#else
// ---- Mac: step hooks ------------------------------------------------------------------------------------------
static void attackInitPlanMac(StepContext *ctx, const StepHook *hook) {
    (void)hook;
    const unsigned char *sp = (const unsigned char *)(uintptr_t)ctx->sp;
    int unit = (int)*(const long long *)(sp + FRAME_FIGHT_PLAN_UNIT), type = *(const int *)(sp + FRAME_FIGHT_PLAN_TYPE);
    int weapon = *(const int *)(sp + FRAME_FIGHT_PLAN_WEAPON);
    ctx->x[10] = (uint32_t)attackInitPlanned(unit, (int)ctx->x[20], weapon, type, (int)ctx->x[10]);
}
static void attackInitCardMac(StepContext *ctx, const StepHook *hook) {
    (void)hook;
    ctx->x[8] = (uint32_t)attackInitOf((int)ctx->x[25], (int)ctx->x[28], (int)ctx->x[26], (int)ctx->x[8]);
}
static void attackInitSpellMac(StepContext *ctx, const StepHook *hook) {
    (void)hook;
    const char *text = *(const char *const *)((const unsigned char *)(uintptr_t)ctx->sp + FRAME_UNIT_CARD_SPELL_INIT);
    ctx->x[1] = (uint64_t)(uintptr_t)attackInitSpellText((int)ctx->x[25], text, (int)ctx->x[28]);
}

static int putAttackInitHooks(void) {
    static StepHook plan, card, spell;
    static const struct { StepHook *hook; void (*handler)(StepContext *, const StepHook *); unsigned int rva; int replace; const char *name; } HOOKS[] = {
        {&plan, attackInitPlanMac, RVA_AI_ZAP_INIT_TEST, 0, "attack_init_plan"},
        {&card, attackInitCardMac, RVA_UNIT_CARD_INIT_READ, 0, "attack_init_card"},
        {&spell, attackInitSpellMac, RVA_UNIT_CARD_SPELL_INIT, 1, "attack_init_spell"},
    };
    if (!gameCodeMatches) {
        writeLogError("hook_skipped", "site=attack_init reason=game_code_differs");
        return 0;
    }
    for (size_t i = 0; i < sizeof HOOKS / sizeof HOOKS[0]; i++) {
        HOOKS[i].hook->handler = HOOKS[i].handler;
        if (!putStepHookMac(HOOKS[i].rva, HOOKS[i].hook, HOOKS[i].name, HOOKS[i].replace)) return 0;
    }
    return 1;
}
#endif

static int installAttackInitHooks(void) {
    if (attackInitHooksIn) return attackInitHooksIn > 0;
    attackInitHooksIn = putAttackInitHooks() ? 1 : -1;
    writeResultLog(attackInitHooksIn > 0, "attack_init_hooks_installed", "sites=3");
    return attackInitHooksIn > 0;
}
