#pragma once

#define GROWTH_TIME_ABILITY   0x83
#define GROWTH_OFFS_ABILITY   0x84
#define GROWTH_COUNT_ABILITY  0x85
#define GROWTH_TERR_ABILITY   0x86
#define GROWTH_KILLS_START    0x7f01
#define GROWTH_XP_START       0x7f02
#define GROWTH_OFFS_WORD      "growoffs"

typedef struct {
    int monster, kills, xp;
} GrowthNeed;

typedef struct {
    int monster, target, chance;
} GrowthTarget;

static struct {
    GrowthNeed *needs;
    int needCount, needCapacity;
    GrowthTarget *targets;
    int targetCount, targetCapacity;
    int dueUnit;
    int dueBy;
    int pickedUnit;
    int picked;
} growth = {.dueUnit = -1, .pickedUnit = -1};

enum { GROWTH_BY_TIME, GROWTH_BY_KILLS, GROWTH_BY_XP };
static const char *const GROWTH_BY_NAMES[] = {"time", "kills", "xp"};

static int (*growthAbilityTotal)(int unit, int ability, int add, char *out, int outIf, int *last);
static int (*growthTerrainMatch)(int code, int x, int y);
static int (*growthRandom)(int n);

static GrowthNeed *growthNeedOf(int monster, int add) {
    for (int i = 0; i < growth.needCount; i++)
        if (growth.needs[i].monster == monster) return &growth.needs[i];
    if (!add) return NULL;
    if (!growList((void **)&growth.needs, &growth.needCapacity, growth.needCount + 1, sizeof *growth.needs)) return NULL;
    growth.needs[growth.needCount] = (GrowthNeed){monster, 0, 0};
    return &growth.needs[growth.needCount++];
}

static int monsterGrowsByDeeds(int monster) {
    const GrowthNeed *need = growthNeedOf(monster, 0);
    return need && (need->kills > 0 || need->xp > 0);
}

static long long growthChanceTotal(int monster) {
    long long total = 0;
    for (int i = 0; i < growth.targetCount; i++)
        if (growth.targets[i].monster == monster) total += growth.targets[i].chance;
    return total;
}

static void clearGrowthTargets(int monster) {
    int n = 0;
    for (int i = 0; i < growth.targetCount; i++)
        if (growth.targets[i].monster != monster) growth.targets[n++] = growth.targets[i];
    growth.targetCount = n;
}

static void clearMonsterGrowth(int monster) {
    GrowthNeed *need = growthNeedOf(monster, 0);
    if (need) need->kills = need->xp = 0;
    clearGrowthTargets(monster);
}

static int copyMonsterGrowth(int source, int target) {
    clearMonsterGrowth(target);
    const GrowthNeed *from = growthNeedOf(source, 0);
    if (from && (from->kills || from->xp)) {
        GrowthNeed copy = *from;
        GrowthNeed *to = growthNeedOf(target, 1);
        if (!to) return 0;
        to->kills = copy.kills, to->xp = copy.xp;
    }
    int count = growth.targetCount;
    for (int i = 0; i < count; i++) {
        if (growth.targets[i].monster != source) continue;
        if (!growList((void **)&growth.targets, &growth.targetCapacity, growth.targetCount + 1, sizeof *growth.targets)) return 0;
        GrowthTarget t = growth.targets[i];
        t.monster = target;
        growth.targets[growth.targetCount++] = t;
    }
    return 1;
}

static void growthWordsBeforePass(int level) {
    if (level != 2) return;
    growth.needCount = 0;
    growth.targetCount = 0;
}

static void growthNeedLine(const char *word, int byXp, const char *args, int mod, int level) {
    if (level != 2) return;
    int monster = G32(RVA_MONSTER_CURRENT);
    if (!monsterRecord(monster)) {
        refuseC5mLine(word, spellModName(mod), "no_monster", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    const char *p = args;
    long long n;
    if (!readSpellNumber(&p, &n) || n < 0 || n > INT_MAX) {
        refuseC5mLine(word, spellModName(mod), "bad_value", "line=%d text=%.40s", G32(RVA_MOD_LINE), args);
        return;
    }
    skipSpaces(&p);
    if (*p && *p != '\r' && *p != '\n') {
        refuseC5mLine(word, spellModName(mod), "bad_value", "line=%d text=%.40s", G32(RVA_MOD_LINE), args);
        return;
    }
    GrowthNeed *need = growthNeedOf(monster, n > 0);
    if (!need) {
        if (n > 0) refuseC5mLine(word, spellModName(mod), "out_of_memory", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    if (byXp) need->xp = (int)n;
    else need->kills = (int)n;
}

static void growkillsLine(const char *args, int mod, int level) { growthNeedLine("growkills", 0, args, mod, level); }
static void growxpLine(const char *args, int mod, int level) { growthNeedLine("growxp", 1, args, mod, level); }

static const char *growthLineHook(const char *line, size_t len, int mod, int level) {
    if (level != 2 || !spellWordIs(line, len, GROWTH_OFFS_WORD)) return line;
    int monster = G32(RVA_MONSTER_CURRENT);
    if (!monsterRecord(monster)) return line;
    const char *p = line + len;
    long long offset, chance;
    if (!readSpellNumber(&p, &offset)) return line;
    const char *afterOffset = p;
    if (!readSpellNumber(&p, &chance)) {
        skipSpaces(&afterOffset);
        if (*afterOffset == '"' || (*afterOffset == '-' || isdigit((unsigned char)*afterOffset))) {
            refuseC5mLine(GROWTH_OFFS_WORD, spellModName(mod), "bad_value", "line=%d text=%.40s", G32(RVA_MOD_LINE), line + len);
            return NULL;
        }
        clearGrowthTargets(monster);
        return line;
    }
    skipSpaces(&p);
    long long records = tableRecords(TABLE_MONSTER);
    long long target = offset < -records || offset > records ? -1 : monster + (offset ? offset : 1);
    const char *problem = chance < 1 || (*p && *p != '\r' && *p != '\n') ? "bad_value"
                        : target < 0 || target >= tableRecords(TABLE_MONSTER) || !monsterRecord((int)target) ? "unknown_monster"
                        : target == monster ? "same_monster"
                        : NULL;
    if (problem) {
        refuseC5mLine(GROWTH_OFFS_WORD, spellModName(mod), problem, "line=%d text=%.40s", G32(RVA_MOD_LINE), line + len);
        return NULL;
    }
    for (int i = 0; i < growth.targetCount; i++)
        if (growth.targets[i].monster == monster && growth.targets[i].target == target) {
            long long others = growthChanceTotal(monster) - growth.targets[i].chance;
            if (others + chance > INT_MAX) break;
            growth.targets[i].chance = (int)chance;
            return line;
        }
    if (growthChanceTotal(monster) + chance > INT_MAX) {
        refuseC5mLine(GROWTH_OFFS_WORD, spellModName(mod), "bad_value", "line=%d text=%.40s total_over=%d", G32(RVA_MOD_LINE),
                      line + len, INT_MAX);
        return NULL;
    }
    if (!growList((void **)&growth.targets, &growth.targetCapacity, growth.targetCount + 1, sizeof *growth.targets)) {
        refuseC5mLine(GROWTH_OFFS_WORD, spellModName(mod), "out_of_memory", "line=%d", G32(RVA_MOD_LINE));
        return NULL;
    }
    growth.targets[growth.targetCount++] = (GrowthTarget){monster, (int)target, (int)chance};
    return line;
}

static int growthStartValue(int unit, int ability) {
    for (int i = 0; i < UNIT_TEMP_ABILITIES; i++) {
        int id = (short)G16(RVA_UNIT_TEMP_ABILITY_IDS + (size_t)unit * UNIT_SIZE + (size_t)i * 2);
        if (!id) return 0;
        if (id == ability) return G32(RVA_UNIT_TEMP_ABILITY_VALUES + (size_t)unit * UNIT_SIZE + (size_t)i * 4);
    }
    return 0;
}

static void unitGrowthStart(int unit, int *kills, int *xp) {
    *kills = growthStartValue(unit, GROWTH_KILLS_START);
    *xp = growthStartValue(unit, GROWTH_XP_START);
}

static void clearUnitGrowthStart(int unit) {
    for (int i = 0; i < UNIT_TEMP_ABILITIES; i++) {
        size_t id = RVA_UNIT_TEMP_ABILITY_IDS + (size_t)unit * UNIT_SIZE + (size_t)i * 2;
        int has = (short)G16(id);
        if (!has) return;
        if (has != GROWTH_KILLS_START && has != GROWTH_XP_START) continue;
        G16(id) = -1;
        G32(RVA_UNIT_TEMP_ABILITY_VALUES + (size_t)unit * UNIT_SIZE + (size_t)i * 4) = 0;
    }
}

static void freeEmptyGrowthCount(int unit) {
    for (int i = 0; i < UNIT_TEMP_ABILITIES; i++) {
        size_t id = RVA_UNIT_TEMP_ABILITY_IDS + (size_t)unit * UNIT_SIZE + (size_t)i * 2;
        int has = (short)G16(id);
        if (!has) return;
        if (has != GROWTH_COUNT_ABILITY) continue;
        if (!G32(RVA_UNIT_TEMP_ABILITY_VALUES + (size_t)unit * UNIT_SIZE + (size_t)i * 4)) G16(id) = -1;
        return;
    }
}

static int unitGrowthDue(int unit) {
    const GrowthNeed *need = growthNeedOf(unitMonsterNumber(unit), 0);
    if (!need || (need->kills <= 0 && need->xp <= 0)) return GROWTH_BY_TIME;
    int kills, xp;
    unitGrowthStart(unit, &kills, &xp);
    if (need->kills > 0 && (short)G16(RVA_UNIT_KILLS + (size_t)unit * UNIT_SIZE) - kills >= need->kills) return GROWTH_BY_KILLS;
    if (need->xp > 0 && (short)G16(RVA_UNIT_XP + (size_t)unit * UNIT_SIZE) - xp >= need->xp) return GROWTH_BY_XP;
    return GROWTH_BY_TIME;
}

static int growthTimeAnswer(int unit, int game) {
    growth.dueUnit = growth.pickedUnit = -1;
    if (!growth.needCount) return game;
    if (game <= 0) freeEmptyGrowthCount(unit);
    int by = unitGrowthDue(unit);
    if (by == GROWTH_BY_TIME) return game;
    int code = growthAbilityTotal(unit, GROWTH_TERR_ABILITY, 0, NULL, 0, NULL);
    int x = G32(RVA_UNIT_X + (size_t)unit * UNIT_SIZE), y = G32(RVA_UNIT_X + 4 + (size_t)unit * UNIT_SIZE);
    if (code && !growthTerrainMatch(code, x, y)) return game;
    int answer = game > 0 ? game : 1;
    if (!setTempAbility(unit, GROWTH_COUNT_ABILITY, answer + 1)) {
        writeLogWarning("growth_waits", "unit=%d monster=%d by=%s reason=temporary_places_full", unit, unitMonsterNumber(unit), GROWTH_BY_NAMES[by]);
        return game;
    }
    growth.dueUnit = unit, growth.dueBy = by;
    return answer;
}

static int growthOffsetAnswer(int unit, int game) {
    growth.pickedUnit = unit;
    growth.picked = 0;
    int monster = unitMonsterNumber(unit);
    long long total = growth.targetCount ? growthChanceTotal(monster) : 0;
    if (total < 1) return game;
    int roll = growthRandom((int)total);
    for (int i = 0; i < growth.targetCount; i++) {
        const GrowthTarget *t = &growth.targets[i];
        if (t->monster != monster) continue;
        if (roll < t->chance) {
            growth.picked = 1;
            return t->target - monster;
        }
        roll -= t->chance;
    }
    return game;
}

static void noteUnitGrew(int unit, int from) {
    int to = unitMonsterNumber(unit), kills = (short)G16(RVA_UNIT_KILLS + (size_t)unit * UNIT_SIZE);
    int xp = (short)G16(RVA_UNIT_XP + (size_t)unit * UNIT_SIZE);
    int by = unit == growth.dueUnit ? growth.dueBy : GROWTH_BY_TIME, kept = 0;
    clearUnitGrowthStart(unit);
    const GrowthNeed *need = growthNeedOf(to, 0);
    if (need && (need->kills > 0 || need->xp > 0)) {
        kept = (need->kills <= 0 || setTempAbility(unit, GROWTH_KILLS_START, kills)) && (need->xp <= 0 || setTempAbility(unit, GROWTH_XP_START, xp));
        if (!kept) writeLogWarning("growth_start_dropped", "unit=%d monster=%d reason=temporary_places_full", unit, to);
    }
    if (by != GROWTH_BY_TIME || growth.picked || kept)
        writeLog("unit_grew", "unit=%d from=%d to=%d by=%s picked=%d kills=%d xp=%d", unit, from, to, GROWTH_BY_NAMES[by], growth.picked, kills, xp);
    growth.dueUnit = growth.pickedUnit = -1;
}

// ---- The hooks ----------------------------------------------------------------------------------
static int gameGrowthAbilityTotal(int unit, int ability, int add, char *out, int outIf, int *last) {
    return GAME_FN(int (*)(int, int, int, char *, int, int *), RVA_UNIT_ABILITY_TOTAL)(unit, ability, add, out, outIf, last);
}
static int gameGrowthTerrainMatch(int code, int x, int y) { return GAME_FN(int (*)(int, int, int), RVA_TERRAIN_MATCH)(code, x, y); }
static int gameGrowthRandom(int n) { return GAME_FN(int (*)(int), RVA_RANDOM)(n); }

static int growthTimeHook(int unit, int ability, int add, char *out, int outIf, int *last) {
    return growthTimeAnswer(unit, growthAbilityTotal(unit, ability, add, out, outIf, last));
}

static int growthOffsetHook(int unit, int ability, int add, char *out, int outIf, int *last) {
    return growthOffsetAnswer(unit, growthAbilityTotal(unit, ability, add, out, outIf, last));
}

static long long (*nextGrowthPromote)(int, int, int, int);
static long long growthPromoteHook(int owner, int unit, int monster, int n) {
    int from = unitMonsterNumber(unit);
    long long result = nextGrowthPromote(owner, unit, monster, n);
    if (unit >= 0 && unit == growth.pickedUnit) noteUnitGrew(unit, from);
    else if (unit >= 0 && growth.needCount && !monsterGrowsByDeeds(unitMonsterNumber(unit))) clearUnitGrowthStart(unit);
    return result;
}

static void growthInit(void) {
    growthAbilityTotal = gameGrowthAbilityTotal;
    growthTerrainMatch = gameGrowthTerrainMatch;
    growthRandom = gameGrowthRandom;
    int words = apiAddModCommand("growkills", growkillsLine) + apiAddModCommand("growxp", growxpLine);
    int calls = redirectBattleCall(RVA_UPKEEP_GROWTIME_CALL, RVA_UNIT_ABILITY_TOTAL, (void *)growthTimeHook, "upkeep_growtime") +
                redirectBattleCall(RVA_UPKEEP_GROWOFFS_CALL, RVA_UNIT_ABILITY_TOTAL, (void *)growthOffsetHook, "upkeep_growoffs");
    int sites = wrapSite(SITE_PROMOTE_UNIT, (void *)growthPromoteHook, (void **)&nextGrowthPromote);
    writeInstallLogFields("growth_installed", words + calls + sites, 5, "words=%d calls=%d sites=%d", words, calls, sites);
}
