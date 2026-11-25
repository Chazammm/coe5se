#pragma once

typedef unsigned long long (*UnitWordFn)(int unit);
static UnitWordFn gameUnitResistancesWord, gameUnitFlagsWord;

static unsigned long long unitResistancesAsGame(int unit) { return gameUnitResistancesWord(unit); }
static unsigned long long unitFlagsAsGame(int unit) { return gameUnitFlagsWord(unit); }
typedef int (*UnitIntFn)(int unit);
static UnitIntFn gameUnitMaxHp;
static int unitMaxHpAsGame(int unit) { return gameUnitMaxHp(unit); }
static UnitIntFn gameUnitBanishable;
static int unitBanishableAsGame(int unit, int weapon, int attacker) { return (void)weapon, (void)attacker, gameUnitBanishable(unit); }
static void banishCause(int *weapon, int *attacker);
typedef void (*ReformPlaceFn)(int unit, int plane, int *x, int *y, int *monster, int *owner, int apply);
static ReformPlaceFn gameReformPlace;
static int reformingUnit = -1;
static void immortalReformAsGame(int unit, int plane, int apply, int *x, int *y, int *monster, int *owner) {
    *x = *y = *monster = *owner = -1;
    int outer = reformingUnit;
    reformingUnit = apply ? unit : -1;
    gameReformPlace(unit, plane, x, y, monster, owner, apply);
    reformingUnit = outer;
}

static unsigned long long unitResistancesHook(int unit) {
    return ((UnitWordFn)siteEntry(SITE_UNIT_RESISTANCES, (void *)unitResistancesAsGame))(unit);
}
static unsigned long long unitFlagsHook(int unit) {
    return ((UnitWordFn)siteEntry(SITE_UNIT_FLAGS, (void *)unitFlagsAsGame))(unit);
}
static int unitMaxHpHook(int unit) {
    return ((UnitIntFn)siteEntry(SITE_UNIT_MAX_HP, (void *)unitMaxHpAsGame))(unit);
}
static int unitBanishableHook(int unit) {
    int weapon, attacker;
    banishCause(&weapon, &attacker);
    return ((int (*)(int, int, int))siteEntry(SITE_UNIT_BANISHABLE, (void *)unitBanishableAsGame))(unit, weapon, attacker);
}
static void immortalReformHook(int unit, int plane, int *x, int *y, int *monster, int *owner, int apply) {
    ((void (*)(int, int, int, int *, int *, int *, int *))siteEntry(SITE_IMMORTAL_REFORM, (void *)immortalReformAsGame))(
        unit, plane, apply, x, y, monster, owner);
}

static void installBattleTraitReads(CallSite site);
static int installComputedHook(CallSite site) {
    static int done[SITE_COUNT];
    static const struct { CallSite site; unsigned int rva; void *detour; void **original; } HOOKS[] = {
        {SITE_UNIT_RESISTANCES, RVA_UNIT_RESISTANCES, (void *)unitResistancesHook, (void **)&gameUnitResistancesWord},
        {SITE_UNIT_FLAGS, RVA_UNIT_FLAGS, (void *)unitFlagsHook, (void **)&gameUnitFlagsWord},
        {SITE_UNIT_MAX_HP, RVA_UNIT_MAX_HP, (void *)unitMaxHpHook, (void **)&gameUnitMaxHp},
        {SITE_UNIT_BANISHABLE, RVA_UNIT_BANISHABLE, (void *)unitBanishableHook, (void **)&gameUnitBanishable},
        {SITE_IMMORTAL_REFORM, RVA_IMMORTAL_REFORM_PLACE, (void *)immortalReformHook, (void **)&gameReformPlace},
    };
    int row = -1;
    for (int i = 0; i < (int)(sizeof HOOKS / sizeof HOOKS[0]); i++)
        if (HOOKS[i].site == site) row = i;
    if (row < 0) return 1;
    if (done[site]) return done[site] > 0;
#ifdef __APPLE__
    // @platform-split banish: Mac points RVA_UNIT_BANISHABLE's five calls at the hook; Windows hooks its entry
    if (site == SITE_UNIT_BANISHABLE) {
        gameUnitBanishable = (UnitIntFn)(void *)GAME_PTR(RVA_UNIT_BANISHABLE);
        int ok = macRedirectCalls(RVA_UNIT_BANISHABLE, (void *)unitBanishableHook, "unit_banishable") == 5;
        done[site] = ok ? 1 : -1;
        writeInstallLogFields("computed_hook_installed", ok, 1, "site=%s", SITES[site].name);
        return ok;
    }
#endif
    unsigned char *target = gameBase + HOOKS[row].rva;
    void *detour = HOOKS[row].detour;
    void **original = HOOKS[row].original;
    int ok = fanpatchHookCreate(target, detour, original) && fanpatchHookEnable(target, 1);
    done[site] = ok ? 1 : -1;
    writeInstallLogFields("computed_hook_installed", ok, 1, "site=%s", SITES[site].name);
    if (ok) installBattleTraitReads(site);
    return ok;
}
