// @platform-split battle_trait_reads: Windows redirects Fast's two calls and stubs the teleport and shield reads; Mac step-hooks the four inlined loads
#pragma once

#define BATTLE_FAST_BITS   (UNIT_FLAG_FAST | UNIT_FLAG_BATTLEFAST)
#define DEPLOY_SHIELD_BITS ((1ULL << MONSTER_FLAG_SHIELD) | (1ULL << MONSTER_FLAG_LARGESHIELD) | (1ULL << MONSTER_FLAG_MAGICSHIELD))
#define AFFRES_STOPS_BITS  ((1ULL << AFFLICTION_BIT_CURSED) | (1ULL << AFFLICTION_BIT_BLIND))
#define AFFRES_IMMUNE_AT   100

static unsigned long long scriptAddedBits(unsigned long long wrapped, unsigned long long game, unsigned long long mask) {
    return wrapped & ~game & mask;
}

static unsigned long long scriptAddedUnitFlags(int unit, unsigned long long mask) {
    if (!gameUnitFlagsWord) return 0;
    return scriptAddedBits(unitFlagsHook(unit), gameUnitFlagsWord(unit), mask);
}
static unsigned long long scriptAddedUnitResistances(int unit, unsigned long long mask) {
    if (!gameUnitResistancesWord) return 0;
    return scriptAddedBits(unitResistancesHook(unit), gameUnitResistancesWord(unit), mask);
}

// ---- Affliction Resistance 100: Cursed and Blind -----------------------------------------------------------------
typedef int (*AfflictionImmuneFn)(int unit, unsigned long long bits);
static AfflictionImmuneFn gameAfflictionImmune;

static unsigned long long affresStoppedBits(int total, unsigned long long bits) {
    return total >= AFFRES_IMMUNE_AT ? bits & AFFRES_STOPS_BITS : 0;
}

static int contentScriptRuns(void);

static int affresForImmunity(int unit) {
    if (unit < 0 || unit >= UNIT_TABLE_MAX) return 0;
    int own = itemStatMonsterPair((short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE), ABILITY_AFFRES);
    if (!contentScriptRuns()) return own;
    return own + GAME_FN(int (*)(int, int), RVA_GET_TEMP_ABILITY)(unit, ABILITY_AFFRES);
}

static int afflictionImmuneWithScripts(int unit, unsigned long long bits) {
    return gameAfflictionImmune(unit, bits & ~affresStoppedBits(bits & AFFRES_STOPS_BITS ? affresForImmunity(unit) : 0, bits));
}

static void installAffresImmunity(void) {
    static int done;
    if (done) return;
    done = 1;
    unsigned char *target = GAME_PTR(RVA_AFFLICTION_IMMUNE);
    int ok = gameCodeMatches && fanpatchHookCreate(target, (void *)afflictionImmuneWithScripts, (void **)&gameAfflictionImmune) &&
             fanpatchHookEnable(target, 1);
    writeInstallLogFields("battle_trait_immunity_installed", ok, 1, "%s", gameCodeMatches ? "" : "reason=game_code_differs");
}

// ---- Fast, Teleport and the shields ------------------------------------------------------------------------------
static unsigned long long deployShieldWord(int unit, unsigned long long word) {
    return word | scriptAddedUnitResistances(unit, DEPLOY_SHIELD_BITS);
}

#ifndef __APPLE__
// ---- Windows: Fast's two calls redirected, two stubs in a page near the game ---------------------------------------------
enum { TRAIT_STUB_FAST_A, TRAIT_STUB_FAST_B, TRAIT_STUB_TELEPORT, TRAIT_STUB_SHIELD, TRAIT_STUBS };
#define TRAIT_REDIRECT_SIZE 0x20
#define TRAIT_STUB_AT(i)    ((i) < TRAIT_STUB_TELEPORT ? (i) * TRAIT_REDIRECT_SIZE \
                                                       : 2 * TRAIT_REDIRECT_SIZE + ((i) - TRAIT_STUB_TELEPORT) * TRAIT_READ_STUB_SIZE)
#define TRAIT_SLOTS         TRAIT_STUB_AT(TRAIT_STUBS)
#define TRAIT_FUNCTIONS     (TRAIT_SLOTS + 2 * 8)
#define TRAIT_UNWIND        (TRAIT_FUNCTIONS + 2 * (int)sizeof(RUNTIME_FUNCTION))
#define TRAIT_CODE_BYTES    (TRAIT_UNWIND + 2 * ITEM_STAT_UNWIND_SIZE)
#define TRAIT_DATA          0x1000
_Static_assert(TRAIT_CODE_BYTES <= TRAIT_DATA, "the trait reads' code page");
static unsigned char *traitPage;

static int unitBattleFastWithScripts(int unit) {
    return GAME_FN(int (*)(int), RVA_UNIT_BATTLE_FAST)(unit) || scriptAddedUnitFlags(unit, BATTLE_FAST_BITS);
}
static int teleportAddedByScripts(int unit) { return scriptAddedUnitFlags(unit, UNIT_FLAG_TELEPORT) != 0; }

static int prepareTraitPage(void) {
    static int state;
    if (state) return state > 0;
    state = -1;
    unsigned char *page = allocNearBytes(gameBase + RVA_TEXT_SECTION, 2 * TRAIT_DATA);
    if (!page) {
        writeLogError("hook_skipped", "site=battle_trait_reads reason=no_near_page");
        return 0;
    }
    static const struct { unsigned rva; void *handler; } STUBS[2] = {
        {RVA_ADVANCE_TELEPORT_TEST, (void *)teleportAddedByScripts},
        {RVA_SETUP_SIDE_SHIELD_READ, (void *)deployShieldWord},
    };
    RUNTIME_FUNCTION *functions = (RUNTIME_FUNCTION *)(page + TRAIT_FUNCTIONS);
    for (int i = 0; i < 2; i++) {
        unsigned char *at = GAME_PTR(STUBS[i].rva), *stub = page + TRAIT_STUB_AT(TRAIT_STUB_TELEPORT + i);
        unsigned char *slot = page + TRAIT_SLOTS + i * 8, *unwind = page + TRAIT_UNWIND + i * ITEM_STAT_UNWIND_SIZE;
        unsigned char *saved = page + TRAIT_DATA + i * 0x40;
        memcpy(slot, &STUBS[i].handler, 8);
        size_t n = i == 0 ? buildTeleportTestStub(stub, stub, slot, saved, at, GAME_PTR(RVA_ADVANCE_TELEPORT_BLINK),
                                                  GAME_PTR(RVA_ADVANCE_TELEPORT_NONE))
                          : buildShieldReadStub(stub, stub, slot, saved, at, at + TRAIT_READ_MOVED);
        if (!n || !copyItemStatUnwind(at, unwind)) {
            writeLogError("hook_skipped", "site=battle_trait_reads_0x%x reason=stub_not_built", STUBS[i].rva);
            return 0;
        }
        functions[i].BeginAddress = (DWORD)(stub - page);
        functions[i].EndAddress = (DWORD)(stub - page + n);
        functions[i].UnwindData = (DWORD)(unwind - page);
    }
    if (!RtlAddFunctionTable(functions, 2, (DWORD64)(uintptr_t)page)) {
        writeLogError("hook_skipped", "site=battle_trait_reads reason=unwind_not_added");
        return 0;
    }
    makePagesReadExecute(page, TRAIT_DATA);
    makePagesReadWrite(page + TRAIT_DATA, TRAIT_DATA);
    traitPage = page;
    state = 1;
    return 1;
}

static int putTraitJump(int stub, unsigned rva, int size) {
    unsigned char *at = GAME_PTR(rva), jump[16];
    buildEntryHookJump(jump, at, traitPage + TRAIT_STUB_AT(stub), size);
    return writeGameCode(at, jump, (size_t)size);
}

static int putTraitReads(CallSite site) {
    if (!prepareTraitPage()) return 0;
    if (site == SITE_UNIT_RESISTANCES) return putTraitJump(TRAIT_STUB_SHIELD, RVA_SETUP_SIDE_SHIELD_READ, TRAIT_READ_MOVED);
    int ok = redirectCall(RVA_FIGHT_BATTLE_FAST_A, RVA_UNIT_BATTLE_FAST, traitPage + TRAIT_STUB_AT(TRAIT_STUB_FAST_A),
                          (void *)unitBattleFastWithScripts, "battle_fast_a", NULL);
    ok += redirectCall(RVA_FIGHT_BATTLE_FAST_B, RVA_UNIT_BATTLE_FAST, traitPage + TRAIT_STUB_AT(TRAIT_STUB_FAST_B),
                       (void *)unitBattleFastWithScripts, "battle_fast_b", NULL);
    return ok + putTraitJump(TRAIT_STUB_TELEPORT, RVA_ADVANCE_TELEPORT_TEST, TRAIT_READ_MOVED + 2);
}
#else
// ---- Mac: a step hook on each inlined load, which it replaces ------------------------------------------------------------
static void battleFastReadMac(StepContext *ctx, const StepHook *hook) {
    (void)hook;
    ctx->x[8] = *(const uint64_t *)(uintptr_t)(ctx->x[8] + MONSTER_MOVE_FLAGS) | scriptAddedUnitFlags((int)ctx->x[19], BATTLE_FAST_BITS);
}
static void teleportReadMac(StepContext *ctx, const StepHook *hook) {
    (void)hook;
    ctx->x[8] = *(const uint64_t *)(uintptr_t)(ctx->x[8] + MONSTER_MOVE_FLAGS) | scriptAddedUnitFlags((int)ctx->x[19], UNIT_FLAG_TELEPORT);
}
static void shieldReadMac(StepContext *ctx, const StepHook *hook) {
    (void)hook;
    ctx->x[9] = deployShieldWord((int)ctx->x[24], *(const uint64_t *)(uintptr_t)ctx->x[19]);
}

static int putTraitReads(CallSite site) {
    static StepHook fastA, fastB, teleport, shield;
    if (site == SITE_UNIT_RESISTANCES) {
        shield.handler = shieldReadMac;
        return putStepHookMac(RVA_SETUP_SIDE_SHIELD_READ, &shield, "battle_shield_read", 1);
    }
    fastA.handler = fastB.handler = battleFastReadMac;
    teleport.handler = teleportReadMac;
    int ok = putStepHookMac(RVA_FIGHT_BATTLE_FAST_A, &fastA, "battle_fast_a", 1);
    ok += putStepHookMac(RVA_FIGHT_BATTLE_FAST_B, &fastB, "battle_fast_b", 1);
    return ok + putStepHookMac(RVA_ADVANCE_TELEPORT_TEST, &teleport, "battle_teleport_read", 1);
}
#endif

static void installBattleTraitReads(CallSite site) {
    static int done[SITE_COUNT];
    if ((site != SITE_UNIT_FLAGS && site != SITE_UNIT_RESISTANCES) || done[site]) return;
    done[site] = 1;
    int parts = site == SITE_UNIT_FLAGS ? 3 : 1;
    if (!gameCodeMatches) {
        writeLogError("battle_trait_reads_installed", "ok=0 of %d site=%s reason=game_code_differs", parts, SITES[site].name);
        return;
    }
    writeInstallLogFields("battle_trait_reads_installed", putTraitReads(site), parts, "site=%s", SITES[site].name);
}
