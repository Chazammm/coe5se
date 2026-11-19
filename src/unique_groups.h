// @platform-split unique_groups: Windows redirects RVA_MONSTER_UNIT_FIRST's callers and patches four inlined counts in place; Mac step-hooks the five inlined searches and the Void's two name compares
#pragma once

#define UNIQUE_GROUP_WORD      "uniquegroup"
#define UNIQUE_GROUP_NAME_SIZE 32

static struct {
    char (*names)[UNIQUE_GROUP_NAME_SIZE];
    int nameCount, nameCapacity;
    unsigned short *of;
    int ofSize;
    int searches, searchCount;
    int newUnitCall;
} uniqueGroups;

static int uniqueGroupOf(int monster) {
    return monster >= 0 && monster < uniqueGroups.ofSize ? uniqueGroups.of[monster] : 0;
}

static int uniqueGroupStandIn(int monster) {
    int group = uniqueGroupOf(monster);
    if (!group || monsterAbility(monster, MONSTER_ABILITY_UNIQUE) <= 0) return monster;
    for (int m = 0; m < monster; m++)
        if (uniqueGroups.of[m] == group && monsterAbility(m, MONSTER_ABILITY_UNIQUE) > 0) return m;
    return monster;
}

static int sameUniqueRecord(int other, int monster, int group) {
    if (other == monster) return 1;
    return group && uniqueGroupOf(other) == group && monsterAbility(other, MONSTER_ABILITY_UNIQUE) > 0;
}

static int uniqueGroupSearch(int monster, int skip) {
    int last = G32(RVA_UNIT_COUNT), group = uniqueGroupOf(monster), first = -1;
    for (int unit = 0; unit <= last && unit < UNIT_TABLE_MAX; unit++) {
        size_t at = (size_t)unit * UNIT_SIZE;
        int other = G16(RVA_UNIT_MONSTER + at);
        if (!sameUniqueRecord(other, monster, group) || (unit == skip && other != monster)) continue;
        if (G16(RVA_UNIT_HP + at) > 0) return unit;
        if (first < 0) first = unit;
    }
    return first;
}

static int uniqueGroupLivingCount(int monster) {
    int last = G32(RVA_UNIT_COUNT), group = uniqueGroupOf(monster), count = 0;
    G32(RVA_RULERSHIP_SLOT) = -1;
    for (int unit = 0; unit <= last && unit < UNIT_TABLE_MAX; unit++) {
        size_t at = (size_t)unit * UNIT_SIZE;
        int other = G16(RVA_UNIT_MONSTER + at);
        if (G16(RVA_UNIT_HP + at) <= 0 || other < 0 || !sameUniqueRecord(other, monster, group)) continue;
        count++;
        if (G32(RVA_RULERSHIP_SLOT) == -1) G32(RVA_RULERSHIP_SLOT) = G16(RVA_UNIT_PLAYER + at);
    }
    return count;
}

static int ritualEffectRetargets(long long id) {
    return id == RITUAL_FX_SUMMONING || id == RITUAL_FX_MIRRORTARG || id == RITUAL_FX_CHOOSEFXTARG || id == RITUAL_FX_PROMOTION ||
           id == RITUAL_FX_TARGET_1035 || id == RITUAL_FX_TARGET_1039 || id == RITUAL_FX_MONTARG;
}

static int ritualTransformsCaster(int ritual) {
    if (ritual < 0) return 0;
    unsigned char *record = ritualWordRecord(ritual);
    int found = 0, retargeted = 0;
    for (int s = 0; s < RITUAL_FX_MAX && ritualWordEffect(record, s)[0]; s++) {
        long long id = ritualWordEffect(record, s)[0];
        if (id == RITUAL_FX_SUMMONING || id == RITUAL_FX_FARSUMMON || id == RITUAL_FX_PROMOTION) return 0;
        if (id == RITUAL_FX_TRANSFORMTARG) {
            if (retargeted) return 0;
            found = 1;
        }
        if (id == RITUAL_FX_LEVELUPMON) found = 1;
        if (id == RITUAL_FX_CASTERTARG) retargeted = 0;
        if (ritualEffectRetargets(id)) retargeted = 1;
    }
    return found;
}

static int uniqueGroupPickUnit(int monster, int caster, int ritual) {
    return uniqueGroupSearch(monster, ritualTransformsCaster(ritual) ? caster : -1);
}

// ---- The Void's uniques, counted by name ----------------------------------------------------------

typedef struct {
    char name[MONSTER_NAME_SIZE];
    int *groups, count, capacity;
} UniqueNameGroups;
static UniqueNameGroups *uniqueNamesKept;
static int uniqueNamesKeptCount, uniqueNamesKeptCapacity;

static int uniqueGroupsNamed(const char *name, const int **groups) {
    for (int i = 0; i < uniqueNamesKeptCount; i++)
        if (!strncmp(uniqueNamesKept[i].name, name, MONSTER_NAME_SIZE)) {
            *groups = uniqueNamesKept[i].groups;
            return uniqueNamesKept[i].count;
        }
    int had = uniqueNamesKeptCapacity;
    if (!growList((void **)&uniqueNamesKept, &uniqueNamesKeptCapacity, uniqueNamesKeptCount + 1, sizeof *uniqueNamesKept)) return -1;
    if (uniqueNamesKeptCapacity > had) memset(uniqueNamesKept + had, 0, (size_t)(uniqueNamesKeptCapacity - had) * sizeof *uniqueNamesKept);
    UniqueNameGroups *kept = &uniqueNamesKept[uniqueNamesKeptCount];
    snprintf(kept->name, MONSTER_NAME_SIZE, "%.*s", MONSTER_NAME_SIZE - 1, name);
    kept->count = 0;
    for (int m = 0; m < uniqueGroups.ofSize; m++) {
        int g = uniqueGroups.of[m], seen = 0;
        if (!g || strcmp((const char *)monsterRecord(m), name) || monsterAbility(m, MONSTER_ABILITY_UNIQUE) <= 0) continue;
        for (int k = 0; k < kept->count; k++) seen |= kept->groups[k] == g;
        if (seen) continue;
        if (!growList((void **)&kept->groups, &kept->capacity, kept->count + 1, sizeof *kept->groups)) return -1;
        kept->groups[kept->count++] = g;
    }
    uniqueNamesKeptCount++;
    *groups = kept->groups;
    return kept->count;
}

static int countsAsNamedUnique(int monster, const unsigned char *record, const char *name) {
    if (record && !strcmp((const char *)record, name)) return 1;
    int group = uniqueGroupOf(monster);
    if (!group || monsterAbility(monster, MONSTER_ABILITY_UNIQUE) <= 0) return 0;
    const int *groups = NULL;
    int n = uniqueGroupsNamed(name, &groups);
    for (int i = 0; i < n; i++)
        if (groups[i] == group) return 1;
    return 0;
}

static UNUSED int uniqueGroupLivingNamed(const char *name) {
    int last = G32(RVA_UNIT_COUNT), count = 0;
    G32(RVA_RULERSHIP_SLOT) = -1;
    for (int unit = 0; unit <= last && unit < UNIT_TABLE_MAX; unit++) {
        size_t at = (size_t)unit * UNIT_SIZE;
        int monster = G16(RVA_UNIT_MONSTER + at);
        if (G16(RVA_UNIT_HP + at) <= 0 || monster < 0 || !countsAsNamedUnique(monster, monsterRecord(monster), name)) continue;
        count++;
        if (G32(RVA_RULERSHIP_SLOT) == -1) G32(RVA_RULERSHIP_SLOT) = G16(RVA_UNIT_PLAYER + at);
    }
    return count;
}

// ---- The word ----------------------------------------------------------------------------------------

static int uniqueGroupId(const char *name) {
    for (int i = 0; i < uniqueGroups.nameCount; i++)
        if (!strcmp(uniqueGroups.names[i], name)) return i + 1;
    if (!growList((void **)&uniqueGroups.names, &uniqueGroups.nameCapacity, uniqueGroups.nameCount + 1, sizeof *uniqueGroups.names))
        return -1;
    snprintf(uniqueGroups.names[uniqueGroups.nameCount], UNIQUE_GROUP_NAME_SIZE, "%s", name);
    return ++uniqueGroups.nameCount;
}

static int growUniqueGroups(void) {
    int records = tableRecords(TABLE_MONSTER);
    if (uniqueGroups.ofSize >= records) return 1;
    unsigned short *grown = realloc(uniqueGroups.of, (size_t)records * sizeof *grown);
    if (!grown) return 0;
    memset(grown + uniqueGroups.ofSize, 0, (size_t)(records - uniqueGroups.ofSize) * sizeof *grown);
    uniqueGroups.of = grown;
    uniqueGroups.ofSize = records;
    return 1;
}

static int setUniqueGroup(int monster, int group) {
    if (!group && monster >= uniqueGroups.ofSize) return 1;
    if (!growUniqueGroups() || monster < 0 || monster >= uniqueGroups.ofSize) return 0;
    uniqueGroups.of[monster] = (unsigned short)group;
    return 1;
}

static void uniqueGroupLine(const char *args, int mod, int level) {
    if (level != 2) return;
    const char *p = args;
    char name[UNIQUE_GROUP_NAME_SIZE];
    int len = readTagName(&p, name, sizeof name), monster = G32(RVA_MONSTER_CURRENT);
    unsigned char *record = monsterRecord(monster);
    skipSpaces(&p);
    if (!record)
        refuseC5mLine(UNIQUE_GROUP_WORD, spellModName(mod), "no_monster", "line=%d", G32(RVA_MOD_LINE));
    else if (len < 0)
        refuseC5mLine(UNIQUE_GROUP_WORD, spellModName(mod), "too_long", "line=%d max=%d", G32(RVA_MOD_LINE), UNIQUE_GROUP_NAME_SIZE - 1);
    else if (!len)
        refuseC5mLine(UNIQUE_GROUP_WORD, spellModName(mod), "no_name", "line=%d text=%.40s", G32(RVA_MOD_LINE), args);
    else if (!isLineEnd(p))
        refuseC5mLine(UNIQUE_GROUP_WORD, spellModName(mod), "bad_value", "line=%d text=%.40s", G32(RVA_MOD_LINE), args);
    else if (uniqueGroups.searches < uniqueGroups.searchCount)
        refuseC5mLine(UNIQUE_GROUP_WORD, spellModName(mod), "searches_not_installed", "line=%d installed=%d of %d", G32(RVA_MOD_LINE),
                      uniqueGroups.searches, uniqueGroups.searchCount);
    else {
        int group = uniqueGroupId(name), value = monsterAbility(monster, MONSTER_ABILITY_UNIQUE);
        if (group < 0 || !setUniqueGroup(monster, group)) {
            refuseC5mLine(UNIQUE_GROUP_WORD, spellModName(mod), "out_of_memory", "line=%d", G32(RVA_MOD_LINE));
            return;
        }
        if (!setAbilityPair((int *)(record + MONSTER_ABILITIES), MONSTER_ABILITY_PAIRS, MONSTER_ABILITY_UNIQUE, value > 0 ? value : 1, 1)) {
            setUniqueGroup(monster, 0);
            refuseC5mLine(UNIQUE_GROUP_WORD, spellModName(mod), "too_many_abilities", "line=%d monster=%.48s max=%d", G32(RVA_MOD_LINE),
                          (const char *)record, MONSTER_ABILITY_PAIRS);
        }
    }
}

static void copyMonsterUniqueGroup(int source, int target) {
    setUniqueGroup(target, uniqueGroupOf(source));
}

static void clearMonsterUniqueGroup(int monster) {
    setUniqueGroup(monster, 0);
}

static void uniqueGroupsBeforePass(int level) {
    if (level != 2) return;
    uniqueGroups.nameCount = 0;
    uniqueNamesKeptCount = 0;
    if (uniqueGroups.of) memset(uniqueGroups.of, 0, (size_t)uniqueGroups.ofSize * sizeof *uniqueGroups.of);
}

// ---- The game's searches -----------------------------------------------------------------------------

static int testFieldAllowsSecondUnique(void);

#ifndef __APPLE__
#define UNIQUE_GROUP_STUB     0x20
#define UNIQUE_GROUP_SLOTS    0x100

static int uniqueGroupUnitFirst(int monster) {
    if (!uniqueGroupOf(monster)) return GAME_FN(int (*)(int), RVA_MONSTER_UNIT_FIRST)(monster);
    return uniqueGroupSearch(monster, -1);
}

// The test-field preview scope also serves the stock owned-field fallback hook.
static int testFieldBypassPreviewUnique(int monster);
static unsigned char *uniqueGroupNewUnitStub;

static int newUnitUniqueHook(int monster) {
    if (testFieldBypassPreviewUnique(monster) || testFieldAllowsSecondUnique()) return -1;
    return uniqueGroupUnitFirst(monster);
}

static uintptr_t *uniqueGroupPickRsp;
static int stringPickUniqueHook(int monster) {
    if (!uniqueGroupOf(monster)) return GAME_FN(int (*)(int), RVA_MONSTER_UNIT_FIRST)(monster);
    uintptr_t rsp = *uniqueGroupPickRsp + 8;
    return uniqueGroupPickUnit(monster, *(const int *)(rsp + FRAME_STRING_PICK_CASTER), *(const int *)(rsp + FRAME_STRING_PICK_RITUAL));
}

static int writeUniqueCountCall(unsigned int rva, unsigned int endRva, const void *slot) {
    unsigned char code[UNIQUE_COUNT_CALL_SIZE], *at = gameBase + rva;
    size_t size = buildUniqueCountCall(code, at, slot, gameBase + endRva);
    return writeGameCode(at, code, size);
}

static int writeVoidCountCall(unsigned int rva, unsigned int endRva, unsigned int nameRva, const void *slot) {
    unsigned char code[VOID_COUNT_CALL_SIZE], *at = gameBase + rva;
    size_t size = buildVoidCountCall(code, at, gameBase + nameRva, slot, gameBase + endRva);
    return writeGameCode(at, code, size);
}

static int installUniqueGroupSearches(void) {
    uniqueGroups.searchCount = 8;
    unsigned char *page = allocNear(gameBase + RVA_TEXT_SECTION), *data = page ? allocNear(page) : NULL;
    if (!data) {
        writeLog("hook_skipped", "site=unique_groups reason=no_near_page");
        return 0;
    }
    void **slots = (void **)(page + UNIQUE_GROUP_SLOTS);
    slots[0] = slots[1] = (void *)uniqueGroupLivingCount;
    slots[2] = slots[3] = (void *)uniqueGroupLivingNamed;
    makePagesReadExecute(page, 0x1000);
    uniqueGroupPickRsp = (uintptr_t *)data;
    uniqueGroups.newUnitCall = redirectCall(RVA_NEW_UNIT_UNIQUE_CALL, RVA_MONSTER_UNIT_FIRST, page, (void *)newUnitUniqueHook, "unique_new_unit", NULL);
    if (uniqueGroups.newUnitCall) uniqueGroupNewUnitStub = page;
    int ok = uniqueGroups.newUnitCall +
             redirectCall(RVA_SUMMON_TO_UNIT_UNIQUE_CALL, RVA_MONSTER_UNIT_FIRST, page + UNIQUE_GROUP_STUB, (void *)uniqueGroupUnitFirst,
                          "unique_summon", NULL) +
             redirectCall(RVA_POPULATE_COMMANDER_UNIQUE_CALL, RVA_MONSTER_UNIT_FIRST, page + 2 * UNIQUE_GROUP_STUB,
                          (void *)uniqueGroupUnitFirst, "unique_populate_commander", NULL) +
             redirectCall(RVA_STRING_PICK_UNIQUE_CALL, RVA_MONSTER_UNIT_FIRST, page + 3 * UNIQUE_GROUP_STUB, (void *)stringPickUniqueHook,
                          "unique_string_pick", uniqueGroupPickRsp);
    ok += writeUniqueCountCall(RVA_POPULATE_UNIQUE_COUNT, RVA_POPULATE_UNIQUE_COUNT_END, &slots[0]);
    ok += writeUniqueCountCall(RVA_POPULATECOM_UNIQUE_COUNT, RVA_POPULATECOM_UNIQUE_COUNT_END, &slots[1]);
    ok += writeVoidCountCall(RVA_POPULATE_VOID_KING_COUNT, RVA_POPULATE_VOID_KING_COUNT_END, RVA_POPULATE_VOID_KING_NAME, &slots[2]);
    ok += writeVoidCountCall(RVA_POPULATE_VOID_VASTNESS_COUNT, RVA_POPULATE_VOID_VASTNESS_END, RVA_POPULATE_VOID_VASTNESS_NAME, &slots[3]);
    return ok;
}
#else
typedef struct {
    unsigned int at, found, none;
    int monsterReg, unitReg, pick;
    const char *name;
} UniqueSearchMac;

typedef struct {
    StepHook hook, foundAt, noneAt;
} UniqueSearchHooksMac;

static const UniqueSearchMac uniqueSearchesMac[] = {
    {RVA_NEW_UNIT_UNIQUE_SEARCH, RVA_NEW_UNIT_UNIQUE_FOUND, RVA_NEW_UNIT_UNIQUE_NONE, 20, 19, 0, "unique_new_unit"},
    {RVA_SUMMON_TO_UNIT_UNIQUE_SEARCH, RVA_SUMMON_TO_UNIT_UNIQUE_FOUND, RVA_SUMMON_TO_UNIT_UNIQUE_NONE, 26, 23, 0, "unique_summon"},
    {RVA_POPULATE_COMMANDER_UNIQUE_SEARCH, RVA_POPULATE_COMMANDER_UNIQUE_FOUND, RVA_POPULATE_COMMANDER_UNIQUE_NONE, 3, 8, 0,
     "unique_populate_commander"},
    {RVA_STRING_PICK_UNIQUE_SEARCH, RVA_STRING_PICK_UNIQUE_FOUND, RVA_STRING_PICK_UNIQUE_NONE, 23, 1, 1, "unique_string_pick"},
    {RVA_POPULATE_UNIQUE_COUNT, RVA_POPULATE_UNIQUE_COUNT_END, 0, 3, 9, 0, "unique_populate"},
};
#define UNIQUE_SEARCHES_MAC (int)(sizeof uniqueSearchesMac / sizeof uniqueSearchesMac[0])
static UniqueSearchHooksMac uniqueSearchHooksMac[UNIQUE_SEARCHES_MAC];

static void uniqueSearchStepMac(StepContext *ctx, const StepHook *hook) {
    const UniqueSearchHooksMac *h = (const UniqueSearchHooksMac *)((const unsigned char *)hook - offsetof(UniqueSearchHooksMac, hook));
    const UniqueSearchMac *s = &uniqueSearchesMac[h - uniqueSearchHooksMac];
    int monster = (int)(uint32_t)ctx->x[s->monsterReg];
    if (!uniqueGroupOf(monster)) return;
    if (!s->none) {
        ctx->x[s->unitReg] = (uint32_t)uniqueGroupLivingCount(monster);
        ctx->pc = h->foundAt.resume;
        return;
    }
    int unit = s->pick ? uniqueGroupPickUnit(monster, *(const int *)(uintptr_t)(ctx->sp + FRAME_STRING_PICK_CASTER),
                                             *(const int *)(uintptr_t)(ctx->sp + FRAME_STRING_PICK_RITUAL))
                       : uniqueGroupSearch(monster, -1);
    if (unit < 0) {
        ctx->pc = h->noneAt.resume;
        return;
    }
    ctx->x[s->unitReg] = (uint32_t)unit;
    ctx->pc = h->foundAt.resume;
}

static StepHook voidNameHooksMac[2];
static void voidNameCompareStepMac(StepContext *ctx, const StepHook *hook) {
    (void)hook;
    int monster = (int)(uint16_t)ctx->x[8];
    ctx->x[0] = countsAsNamedUnique(monster, (const unsigned char *)(uintptr_t)ctx->x[0], (const char *)(uintptr_t)ctx->x[1]) ? 0 : 1;
}

static int installUniqueGroupSearches(void) {
    int ok = 0;
    uniqueGroups.searchCount = UNIQUE_SEARCHES_MAC + 2;
    voidNameHooksMac[0].handler = voidNameHooksMac[1].handler = voidNameCompareStepMac;
    ok += putStepHookMac(RVA_POPULATE_VOID_KING_COUNT, &voidNameHooksMac[0], "unique_void_king", 1);
    ok += putStepHookMac(RVA_POPULATE_VOID_VASTNESS_COUNT, &voidNameHooksMac[1], "unique_void_vastness", 1);
    for (int i = 0; i < UNIQUE_SEARCHES_MAC; i++) {
        const UniqueSearchMac *s = &uniqueSearchesMac[i];
        UniqueSearchHooksMac *h = &uniqueSearchHooksMac[i];
        h->hook.handler = uniqueSearchStepMac;
        if (!putStepResumeMac(s->found, &h->foundAt, s->name) || (s->none && !putStepResumeMac(s->none, &h->noneAt, s->name)) ||
            !putStepHookMac(s->at, &h->hook, s->name, 0))
            continue;
        ok++;
    }
    return ok;
}
#endif

static void uniqueGroupsInit(void) {
    if (!checkGameFunction("unique_groups_check", "")) {
        uniqueGroups.searchCount = 1;
        writeLogError("unique_groups_installed", "ok=0 of 1 reason=%s", gameCodeReason());
    } else {
        uniqueGroups.searches = installUniqueGroupSearches();
        writeInstallLog("unique_groups_installed", uniqueGroups.searches, uniqueGroups.searchCount);
    }
    apiAddModCommand("uniquegroup", uniqueGroupLine);
}
