#pragma once

typedef unsigned long long (*UnitWordFn)(int unit);
static UnitWordFn gameUnitResistancesWord, gameUnitFlagsWord;

static unsigned long long unitResistancesAsGame(int unit) { return gameUnitResistancesWord(unit); }
static unsigned long long unitFlagsAsGame(int unit) { return gameUnitFlagsWord(unit); }

static unsigned long long unitResistancesHook(int unit) {
    return ((UnitWordFn)siteEntry(SITE_UNIT_RESISTANCES, (void *)unitResistancesAsGame))(unit);
}
static unsigned long long unitFlagsHook(int unit) {
    return ((UnitWordFn)siteEntry(SITE_UNIT_FLAGS, (void *)unitFlagsAsGame))(unit);
}

static int installComputedHook(CallSite site) {
    static int done[SITE_COUNT];
    if (site != SITE_UNIT_RESISTANCES && site != SITE_UNIT_FLAGS) return 1;
    if (done[site]) return done[site] > 0;
    unsigned char *target = gameBase + (site == SITE_UNIT_RESISTANCES ? RVA_UNIT_RESISTANCES : RVA_UNIT_FLAGS);
    void *detour = site == SITE_UNIT_RESISTANCES ? (void *)unitResistancesHook : (void *)unitFlagsHook;
    void **original = site == SITE_UNIT_RESISTANCES ? (void **)&gameUnitResistancesWord : (void **)&gameUnitFlagsWord;
    int ok = fanpatchHookCreate(target, detour, original) && fanpatchHookEnable(target, 1);
    done[site] = ok ? 1 : -1;
    writeInstallLogFields("computed_hook_installed", ok, 1, "site=%s", SITES[site].name);
    return ok;
}
