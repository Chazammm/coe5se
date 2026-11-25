#pragma once

typedef struct { CallSite site; int skip; } AfflictionStat;
#define AFFLICTION_SITE_STATS   (AFFLICTION_STAT_COUNT - 1)
#define AFFLICTION_STAT_ABILITY (AFFLICTION_STAT_COUNT - 1)
#define AFFLICTION_ABILITY_NAME "UNIT_ABILITY"
#define AFFLICTION_ABILITY_RUNS_AS SITE_UNIT_RESISTANCES
static const AfflictionStat AFFLICTION_STATS[AFFLICTION_SITE_STATS] = {
    {SITE_UNIT_DAMAGE_BONUS, 0}, {SITE_UNIT_MORALE, 0}, {SITE_UNIT_MR, 0}, {SITE_UNIT_BATTLE_RANK, 0}, {SITE_UNIT_MAX_HP, 0},
    {SITE_UNIT_PATH_BONUS, 0}, {SITE_UNIT_BLINDNESS, 0}, {SITE_UNIT_INCAPACITATED, 0}, {SITE_UNIT_FIRE_RESISTANCE, 0},
    {SITE_UNIT_ARMOR, 0}, {SITE_UNIT_RESISTANCES, 0}, {SITE_UNIT_FLAGS, 0}, {SITE_UNIT_BATTLE_STRENGTH, 0}, {SITE_UNIT_STATE_FLAGS, 0},
    {SITE_DOATTACK, 1}, {SITE_BATTLE_ADVANCE_UNIT, 1}, {SITE_ATTACK_INIT, 0},
};

typedef struct {
    LuaScript *s;
    int ref;
    int kind, stat;
    int next;
} AfflictionEffect;
static AfflictionEffect *afflictionEffects;
static int afflictionEffectCount, afflictionEffectCapacity;
static int afflictionEffectFirst[CUSTOM_AFFLICTIONS_MAX][AFFLICTION_STAT_COUNT];
static int afflictionStatDepth[AFFLICTION_STAT_COUNT];
static int afflictionStatInstalled[AFFLICTION_STAT_COUNT];
typedef u64 (*AfflictionStatFn)(u64, u64, u64, u64, u64, u64, u64, u64);
static AfflictionStatFn nextAfflictionStat[AFFLICTION_SITE_STATS];

static int afflictionStatNamed(const char *site) {
    if (!strcmp(site, AFFLICTION_ABILITY_NAME)) return AFFLICTION_STAT_ABILITY;
    const LuaSite *ls = luaSiteOfName(site);
    for (int i = 0; ls && i < AFFLICTION_SITE_STATS; i++)
        if (ls->site == AFFLICTION_STATS[i].site) return i;
    return -1;
}

static const char *afflictionStatName(int stat) {
    return stat == AFFLICTION_STAT_ABILITY ? "unit_ability" : SITES[AFFLICTION_STATS[stat].site].name;
}

static const LuaSite *afflictionStatSite(int stat) {
    if (stat < 0 || stat >= AFFLICTION_SITE_STATS) return NULL;
    for (int r = 0; r < LUA_SITE_COUNT; r++)
        if (luaSites[r].site == AFFLICTION_STATS[stat].site) return &luaSites[r];
    return NULL;
}

typedef struct {
    const AfflictionEffect *e;
    const LuaSite *ls;
    int unit;
    u64 value;
    const u64 *args;
} AfflictionEffectCall;

static int pushAfflictionEffect(lua_State *L, void *ctx) {
    const AfflictionEffectCall *c = ctx;
    const char *args = siteArgs(c->ls);
    luaL_checkstack(L, 3 + LUA_CALL_ARGS, "an affliction effect's arguments");
    lua_rawgeti(L, LUA_REGISTRYINDEX, c->e->ref);
    lua_pushinteger(L, c->unit);
    char result = siteResult(c->ls);
    if (AFFLICTION_STATS[c->e->stat].skip || c->ls->kind == K_TEST) lua_pushboolean(L, c->value != 0);
    else pushLuaValue(L, result, c->value);
    int n = 2;
    for (int i = 1; args[i] && i < LUA_CALL_ARGS; i++, n++) pushLuaValue(L, args[i], c->args[i]);
    return n;
}

static u64 runAfflictionEffect(const AfflictionEffect *e, int unit, u64 value, const u64 *args) {
    LuaScript *s = e->s;
    CallSite site = AFFLICTION_STATS[e->stat].site;
    if (!s || s->dead || !s->L || !luaSiteOpenNow(s, site)) return value;
    const LuaSite *ls = afflictionStatSite(e->stat);
    if (!ls) return value;
    AfflictionEffectCall c = {e, ls, unit, value, args};
    CallSite outerSite = luaRunningSite;
    luaRunningSite = site;
    noteBattleScriptCall(s);
    int ok = callLuaProtected(s, pushAfflictionEffect, &c, 1);
    luaRunningSite = outerSite;
    if (!ok) return value;
    lua_State *L = s->L;
    lua_Integer n = 0;
    int skipOrTest = AFFLICTION_STATS[e->stat].skip || ls->kind == K_TEST;
    if (lua_isnil(L, -1)) {
    } else if (skipOrTest && lua_isboolean(L, -1)) {
        value = (u64)lua_toboolean(L, -1);
    } else if (luaWholeNumber(L, -1, &n)) {
        value = skipOrTest ? (u64)(n != 0) : siteResult(ls) == 'i' ? (u64)(long long)(int)n : (u64)n;
    } else {
        refuseLuaCall(s->mod, "coe5.afflictionEffect", "not_a_number", "site=%s name=%.63s type=%s", ls->name, customAfflictions[e->kind].name,
                      luaL_typename(L, -1));
    }
    lua_pop(L, 1);
    closeDeadLuaScript(s);
    return value;
}

static int afflictionKindsWithEffect(int unit, int stat, int *kinds) {
    const AfflictionList *l = &unitAfflictionLists[unit];
    int n = 0;
    for (int i = 0; i < l->count; i++) {
        int k = l->entries[i].kind;
        if (!(customAfflictions[k].effectStats >> stat & 1) || !customAfflictionDeclared(k)) continue;
        int at = n++;
        while (at > 0 && kinds[at - 1] > k) kinds[at] = kinds[at - 1], at--;
        kinds[at] = k;
    }
    return n;
}

static u64 applyAfflictionEffects(int stat, const u64 *args, u64 value) {
    int unit = (int)args[0];
    if (!afflictionStatHolders[stat] || afflictionStatDepth[stat] || !unitAfflictionCount(unit)) return value;
    int kinds[CUSTOM_AFFLICTIONS_MAX], n = afflictionKindsWithEffect(unit, stat, kinds);
    if (!n) return value;
    afflictionStatDepth[stat]++;
    int effectUnitWas = afflictionEffectUnit;
    afflictionEffectUnit = unit;
    for (int i = 0; i < n; i++)
        for (int r = afflictionEffectFirst[kinds[i]][stat]; r; r = afflictionEffects[r - 1].next) {
            value = runAfflictionEffect(&afflictionEffects[r - 1], unit, value, args);
            if (AFFLICTION_STATS[stat].skip && !value) break;
        }
    afflictionEffectUnit = effectUnitWas;
    afflictionStatDepth[stat]--;
    return value;
}

// ---- UNIT_ABILITY: an ability's amount, folded into the game's totals --------------------------------------
typedef struct {
    const AfflictionEffect *e;
    int unit, ability;
    lua_Integer value;
} AfflictionAbilityCall;

static int pushAfflictionAbility(lua_State *L, void *ctx) {
    const AfflictionAbilityCall *c = ctx;
    luaL_checkstack(L, 4, "an affliction effect's arguments");
    lua_rawgeti(L, LUA_REGISTRYINDEX, c->e->ref);
    lua_pushinteger(L, c->unit);
    lua_pushinteger(L, c->value);
    lua_pushinteger(L, c->ability);
    return 3;
}

static lua_Integer runAfflictionAbility(const AfflictionEffect *e, int unit, int ability, lua_Integer value) {
    LuaScript *s = e->s;
    if (!s || s->dead || !s->L || !luaSiteOpenNow(s, AFFLICTION_ABILITY_RUNS_AS)) return value;
    AfflictionAbilityCall c = {e, unit, ability, value};
    CallSite outerSite = luaRunningSite;
    luaRunningSite = AFFLICTION_ABILITY_RUNS_AS;
    noteBattleScriptCall(s);
    int ok = callLuaProtected(s, pushAfflictionAbility, &c, 1);
    luaRunningSite = outerSite;
    if (!ok) return value;
    lua_State *L = s->L;
    lua_Integer n = 0;
    if (lua_isnil(L, -1)) {
    } else if (luaWholeNumber(L, -1, &n)) {
        value = n < INT_MIN ? INT_MIN : n > INT_MAX ? INT_MAX : n;
    } else {
        refuseLuaCall(s->mod, "coe5.afflictionEffect", "not_a_number", "site=%s name=%.63s type=%s", AFFLICTION_ABILITY_NAME,
                      customAfflictions[e->kind].name, luaL_typename(L, -1));
    }
    lua_pop(L, 1);
    closeDeadLuaScript(s);
    return value;
}

static int afflictionAbilityHolder(int unit) {
    if (!afflictionStatHolders[AFFLICTION_STAT_ABILITY] || !unitAfflictionCount(unit)) return 0;
    const AfflictionList *l = &unitAfflictionLists[unit];
    for (int i = 0; i < l->count; i++) {
        int k = l->entries[i].kind;
        if ((customAfflictions[k].effectStats >> AFFLICTION_STAT_ABILITY & 1) && customAfflictionDeclared(k)) return 1;
    }
    return 0;
}

static int afflictionAbilityAmount(int unit, int ability) {
    if (afflictionStatDepth[AFFLICTION_STAT_ABILITY] || ability <= 0) return 0;
    int kinds[CUSTOM_AFFLICTIONS_MAX], n = afflictionKindsWithEffect(unit, AFFLICTION_STAT_ABILITY, kinds);
    if (!n) return 0;
    afflictionStatDepth[AFFLICTION_STAT_ABILITY]++;
    int effectUnitWas = afflictionEffectUnit;
    afflictionEffectUnit = unit;
    lua_Integer value = 0;
    for (int i = 0; i < n; i++)
        for (int r = afflictionEffectFirst[kinds[i]][AFFLICTION_STAT_ABILITY]; r; r = afflictionEffects[r - 1].next)
            value = runAfflictionAbility(&afflictionEffects[r - 1], unit, ability, value);
    afflictionEffectUnit = effectUnitWas;
    afflictionStatDepth[AFFLICTION_STAT_ABILITY]--;
    return (int)value;
}

static u64 runAfflictionStat(int stat, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6, u64 a7, u64 a8) {
    u64 args[LUA_CALL_ARGS] = {a1, a2, a3, a4, a5, a6, a7, a8};
    if (AFFLICTION_STATS[stat].skip) {
        if (!applyAfflictionEffects(stat, args, 1)) return 0;
        return nextAfflictionStat[stat](a1, a2, a3, a4, a5, a6, a7, a8);
    }
    return applyAfflictionEffects(stat, args, nextAfflictionStat[stat](a1, a2, a3, a4, a5, a6, a7, a8));
}
#define AFFLICTION_STAT_HOOK(i) \
    static u64 afflictionStatHook##i(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6, u64 a7, u64 a8) { \
        return runAfflictionStat(i, a1, a2, a3, a4, a5, a6, a7, a8); \
    }
AFFLICTION_STAT_HOOK(0) AFFLICTION_STAT_HOOK(1) AFFLICTION_STAT_HOOK(2) AFFLICTION_STAT_HOOK(3) AFFLICTION_STAT_HOOK(4)
AFFLICTION_STAT_HOOK(5) AFFLICTION_STAT_HOOK(6) AFFLICTION_STAT_HOOK(7) AFFLICTION_STAT_HOOK(8) AFFLICTION_STAT_HOOK(9)
AFFLICTION_STAT_HOOK(10) AFFLICTION_STAT_HOOK(11) AFFLICTION_STAT_HOOK(12) AFFLICTION_STAT_HOOK(13) AFFLICTION_STAT_HOOK(14)
AFFLICTION_STAT_HOOK(15) AFFLICTION_STAT_HOOK(16)
#undef AFFLICTION_STAT_HOOK
static const AfflictionStatFn AFFLICTION_STAT_HOOKS[AFFLICTION_SITE_STATS] = {
    afflictionStatHook0, afflictionStatHook1, afflictionStatHook2, afflictionStatHook3, afflictionStatHook4, afflictionStatHook5,
    afflictionStatHook6, afflictionStatHook7, afflictionStatHook8, afflictionStatHook9, afflictionStatHook10, afflictionStatHook11,
    afflictionStatHook12, afflictionStatHook13, afflictionStatHook14, afflictionStatHook15, afflictionStatHook16,
};

static int installAfflictionStat(int stat) {
    if (afflictionStatInstalled[stat]) return afflictionStatInstalled[stat] > 0;
    if (stat == AFFLICTION_STAT_ABILITY) {
        scriptAbilityHolder = afflictionAbilityHolder, scriptAbilityAmount = afflictionAbilityAmount;
        int ok = featureOn(FEATURE_ITEM_SLOT_STATS) && installItemSlotStats();
        if (ok) openItemStatSitesToScripts();
        afflictionStatInstalled[stat] = ok ? 1 : -1;
        writeInstallLogFields("affliction_effect_site_installed", ok, 1, "site=%s", afflictionStatName(stat));
        return ok;
    }
    CallSite site = AFFLICTION_STATS[stat].site;
    int ok = wrapInnermost(site, (void *)AFFLICTION_STAT_HOOKS[stat], (void **)&nextAfflictionStat[stat]);
    afflictionStatInstalled[stat] = ok ? 1 : -1;
    writeInstallLogFields("affliction_effect_site_installed", ok, 1, "site=%s", SITES[site].name);
    return ok;
}

static void linkAfflictionEffect(int r) {
    AfflictionEffect *e = &afflictionEffects[r];
    e->next = 0;
    int *at = &afflictionEffectFirst[e->kind][e->stat];
    while (*at) at = &afflictionEffects[*at - 1].next;
    *at = r + 1;
    if (!(customAfflictions[e->kind].effectStats >> e->stat & 1)) {
        customAfflictions[e->kind].effectStats |= 1u << e->stat;
        afflictionStatHolders[e->stat] += customAfflictions[e->kind].holders;
    }
}

static int lAfflictionEffect(lua_State *L) {
    const char *name = luaL_checkstring(L, 1), *siteName = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    if (refuseFromDeadLuaScript(L, "coe5.afflictionEffect")) return lua_pushnil(L), 1;
    LuaScript *s = luaScriptOf(L);
    const char *mod = s ? s->mod : "?";
    int stat = afflictionStatNamed(siteName);
    const char *reason = stat < 0 ? "unknown_site" : !name[0] || strlen(name) >= CUSTOM_AFFLICTION_NAME_SIZE ? "bad_value"
                       : !ensureAfflictionStore() ? "out_of_memory" : NULL;
    int k = reason ? -1 : findCustomAffliction(name);
    if (!reason && k < 0 && (k = addCustomAffliction(name)) < 0) reason = "too_many_afflictions";
    if (!reason && !installAfflictionStat(stat)) reason = "site_unavailable";
    if (!reason && !growList((void **)&afflictionEffects, &afflictionEffectCapacity, afflictionEffectCount + 1, sizeof afflictionEffects[0]))
        reason = "out_of_memory";
    if (reason) {
        refuseLuaCall(mod, "coe5.afflictionEffect", reason, "name=%.63s site=%.63s", name, siteName);
        return lua_pushnil(L), 1;
    }
    lua_pushvalue(L, 3);
    int r = afflictionEffectCount++;
    afflictionEffects[r] = (AfflictionEffect){s, luaL_ref(L, LUA_REGISTRYINDEX), k, stat, 0};
    linkAfflictionEffect(r);
    writeLog("affliction_effect_added", "mod=%s name=%.63s site=%s declared=%d", mod, customAfflictions[k].name, afflictionStatName(stat),
             customAfflictionDeclared(k));
    lua_pushboolean(L, 1);
    return 1;
}

static void dropAfflictionEffects(const LuaScript *s) {
    int kept = 0;
    for (int r = 0; r < afflictionEffectCount; r++)
        if (afflictionEffects[r].s != s) afflictionEffects[kept++] = afflictionEffects[r];
    if (kept == afflictionEffectCount) return;
    afflictionEffectCount = kept;
    memset(afflictionEffectFirst, 0, sizeof afflictionEffectFirst);
    memset(afflictionStatHolders, 0, sizeof afflictionStatHolders);
    for (int k = 0; k < customAfflictionCount; k++) customAfflictions[k].effectStats = 0;
    for (int r = 0; r < afflictionEffectCount; r++) linkAfflictionEffect(r);
}
