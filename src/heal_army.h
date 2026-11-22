#pragma once

#define HEAL_ARMY_WORD      "healarmy"
#define HEAL_ARMY_BYPASS_WORD "bypassnoheal"
#define HEAL_ARMY_DISEASED  1ULL
#define RESIST_NOHEAL_BIT   (1ULL << 8)
#define RESIST_FASTHEAL_BIT (1ULL << 9)
#define ABILITY_FALSELIFE   0x1e9

enum { HEAL_ARMY_SPELL, HEAL_ARMY_MONSTER, HEAL_ARMY_ITEM };
static const char *const HEAL_ARMY_SOURCE_WORDS[] = {"spell", "monster", "item"};

typedef struct {
    int source;
    int slot;
    int bypassNoheal;
    WeaponTraitRule trait;
} HealArmyRule;

static struct {
    HealArmyRule *rules;
    int count, capacity;
} healArmy;

static int (*healArmyPathLevel)(int unit, int path);
static unsigned long long (*healArmyUnitState)(int unit);
static int (*healArmyMaxHp)(int unit);
static int (*healArmyFalselife)(int unit);

static void dropHealArmyRules(int source, int record) {
    int n = 0;
    for (int i = 0; i < healArmy.count; i++)
        if (healArmy.rules[i].source != source || (record != -1 && healArmy.rules[i].slot != record)) healArmy.rules[n++] = healArmy.rules[i];
    healArmy.count = n;
}

static void healArmyReset(void) { dropHealArmyRules(HEAL_ARMY_SPELL, -1); }

static void healArmyBeforePass(void) {
    dropHealArmyRules(HEAL_ARMY_MONSTER, -1);
    dropHealArmyRules(HEAL_ARMY_ITEM, -1);
}

static void healArmyBlockStored(int slot) {
    for (int i = 0; i < healArmy.count; i++)
        if (healArmy.rules[i].source == HEAL_ARMY_SPELL && !healArmy.rules[i].slot) healArmy.rules[i].slot = slot;
}

static void healArmyDropPending(void) { dropHealArmyRules(HEAL_ARMY_SPELL, 0); }

static void clearHealArmyRules(int item, int record) {
    if (record < 0) return;
    dropHealArmyRules(item ? HEAL_ARMY_ITEM : HEAL_ARMY_MONSTER, record);
}

static void copyHealArmyRules(int item, int source, int target) {
    int kind = item ? HEAL_ARMY_ITEM : HEAL_ARMY_MONSTER;
    dropHealArmyRules(kind, target);
    int count = healArmy.count;
    for (int i = 0; i < count; i++)
        if (healArmy.rules[i].source == kind && healArmy.rules[i].slot == source &&
            growList((void **)&healArmy.rules, &healArmy.capacity, healArmy.count + 1, sizeof *healArmy.rules)) {
            healArmy.rules[healArmy.count] = healArmy.rules[i];
            healArmy.rules[healArmy.count++].slot = target;
        }
}

static int addHealArmyRule(int source, int slot, int kind, unsigned long long value, const char *word, int bypassNoheal) {
    for (int i = 0; i < healArmy.count; i++) {
        HealArmyRule *r = &healArmy.rules[i];
        if (r->source == source && r->slot == slot && r->trait.kind == kind && r->trait.value == value) {
            r->bypassNoheal |= bypassNoheal;
            return 1;
        }
    }
    if (!growList((void **)&healArmy.rules, &healArmy.capacity, healArmy.count + 1, sizeof *healArmy.rules)) return 0;
    HealArmyRule *r = &healArmy.rules[healArmy.count++];
    memset(r, 0, sizeof *r);
    r->source = source;
    r->slot = slot;
    r->bypassNoheal = bypassNoheal;
    r->trait.kind = kind;
    r->trait.value = value;
    snprintf(r->trait.word, sizeof r->trait.word, "%s", word);
    return 1;
}

static int healArmyWordTrait(const char *line, size_t len, char *trait, size_t size) {
    static const char PRE[] = "heal", POST[] = "army";
    size_t pre = sizeof PRE - 1, post = sizeof POST - 1;
    if (len <= pre + post || strncmp(line, PRE, pre) || strncmp(line + len - post, POST, post)) return 0;
    if ((len == sizeof "heallivingarmy" - 1 && !strncmp(line, "heallivingarmy", len)) ||
        (len == sizeof "healhumanarmy" - 1 && !strncmp(line, "healhumanarmy", len)))
        return 0;
    size_t n = len - pre - post;
    if (n >= size) n = size - 1;
    memcpy(trait, line + pre, n);
    trait[n] = 0;
    return 1;
}

static int readHealArmyBypass(const char *p) {
    char word[16];
    skipSpaces(&p);
    if (isLineEnd(p)) return 0;
    int len = readWord(&p, word, sizeof word);
    skipSpaces(&p);
    return len > 0 && (size_t)len < sizeof word && !strcmp(word, HEAL_ARMY_BYPASS_WORD) && isLineEnd(p) ? 1 : -1;
}

#define HEAL_ARMY_PROBLEM(source, mod, line, ...) \
    do { \
        if ((source) == HEAL_ARMY_SPELL) noteSpellProblem(mod, line, __VA_ARGS__); \
    } while (0)

static int healArmyLineSource(const char *word, int mod, int level, int *slot) {
    if (spellBlock.open) {
        *slot = spellBlock.slot;
        return spellBlockTakes(word, mod, level) ? HEAL_ARMY_SPELL : -1;
    }
    int monster = G32(RVA_MONSTER_CURRENT), item = G32(RVA_ITEM_CURRENT);
    if (monsterRecord(monster)) return *slot = monster, HEAL_ARMY_MONSTER;
    if (item >= 0 && item < tableRecords(TABLE_ITEM)) return *slot = item, HEAL_ARMY_ITEM;
    refuseC5mLine(word, spellModName(mod), "no_block", "line=%d", G32(RVA_MOD_LINE));
    noteSpellProblem(mod, G32(RVA_MOD_LINE), "%s outside a newspell, monster or item block", word);
    return -1;
}

static void healArmyRuleLine(const char *word, const char *trait, const char *p, int mod, int level) {
    if (level != 2) return;
    int slot = 0, source = healArmyLineSource(word, mod, level, &slot);
    if (source < 0) return;
    int kind, line = G32(RVA_MOD_LINE);
    unsigned long long value;
    char name[MONSTER_TAG_NAME_SIZE];
    const char *wordShown = trait;
    if (!trait[0]) {
        refuseC5mLine(word, spellModName(mod), "no_trait", "line=%d", line);
        HEAL_ARMY_PROBLEM(source, mod, line, "%s needs a trait", word);
        return;
    }
    if (!strcmp(trait, "tag")) {
        int len = readTagName(&p, name, sizeof name), tag = len > 0 ? monsterTagId(name, 1) : 0;
        if (len <= 0 || tag < 0) {
            refuseC5mLine(word, spellModName(mod), len < 0 ? "too_long" : tag < 0 ? "out_of_memory" : "no_name", "line=%d", line);
            HEAL_ARMY_PROBLEM(source, mod, line, "%s tag needs a name in quotes", word);
            return;
        }
        kind = WEAPON_TRAIT_TAG;
        value = (unsigned long long)tag;
        wordShown = name;
    } else if (!findWeaponTrait(trait, &kind, &value)) {
        refuseC5mLine(word, spellModName(mod), "unknown_trait", "line=%d trait=%.40s", line, trait);
        HEAL_ARMY_PROBLEM(source, mod, line, "%.40s isn't a monster trait", trait);
        return;
    }
    int bypassNoheal = readHealArmyBypass(p);
    if (bypassNoheal < 0) {
        refuseC5mLine(word, spellModName(mod), "bad_value", "line=%d text=%.40s", line, p);
        HEAL_ARMY_PROBLEM(source, mod, line, "only " HEAL_ARMY_BYPASS_WORD " can follow %s", word);
        return;
    }
    if (!addHealArmyRule(source, slot, kind, value, wordShown, bypassNoheal)) {
        refuseC5mLine(word, spellModName(mod), "out_of_memory", "line=%d", line);
        return;
    }
    writeLog("heal_army_rule", "mod=%s source=%s slot=%d trait=%s bypassnoheal=%d line=%d", spellModName(mod), HEAL_ARMY_SOURCE_WORDS[source], slot,
             wordShown, bypassNoheal, line);
}

static void healarmyLine(const char *args, int mod, int level) {
    char trait[WEAPON_TRAIT_WORD_SIZE];
    const char *p = args;
    int len = readWord(&p, trait, sizeof trait);
    if (len < 0 || (size_t)len >= sizeof trait) trait[0] = 'x', trait[1] = 0;
    healArmyRuleLine(HEAL_ARMY_WORD, len > 0 ? trait : "", p, mod, level);
}

static int healArmyLineHook(const char *line, size_t len, const char *args, int mod, int level) {
    char trait[WEAPON_TRAIT_WORD_SIZE + 1], word[64];
    if (!healArmyWordTrait(line, len, trait, sizeof trait)) return 0;
    snprintf(word, sizeof word, "%.*s", (int)(len < sizeof word ? len : sizeof word - 1), line);
    if (strlen(trait) >= WEAPON_TRAIT_WORD_SIZE) trait[0] = 'x', trait[1] = 0;
    healArmyRuleLine(word, trait, args, mod, level);
    return 1;
}

static int healArmyCasterHas(int caster, int slot) {
    const short *known = (const short *)GAME_PTR(RVA_UNIT_KNOWN_SPELLS + (size_t)caster * UNIT_SIZE);
    for (int k = 0; k < UNIT_KNOWN_SPELLS_MAX && known[k] >= 0; k++) {
        if (known[k] != slot) continue;
        const SpellRecord *s = spellSlot(slot);
        return s->path >= 0 && healArmyPathLevel(caster, s->path) >= s->level;
    }
    return 0;
}

static int healArmyUnitHolds(int u, int item) {
    int positions = unitExtraItemCount(u) ? ITEM_POSITION_MAX : UNIT_ITEM_SLOTS;
    for (int i = 0; i < positions; i++)
        if (unitItemAt(u, i) == item) return 1;
    return 0;
}

static int healArmyGiverHas(int giver, const HealArmyRule *r) {
    if (r->source == HEAL_ARMY_MONSTER) return G16(RVA_UNIT_MONSTER + (size_t)giver * UNIT_SIZE) == r->slot;
    if (r->source == HEAL_ARMY_ITEM) return healArmyUnitHolds(giver, r->slot);
    return healArmyCasterHas(giver, r->slot);
}

static int healArmyHealsFast(int unit, int rate) {
    if (!healArmy.count || rate >= HEAL_RATE_FAST || unit < 0 || unit >= UNIT_TABLE_MAX) return 0;
    size_t at = (size_t)unit * UNIT_SIZE;
    if ((short)G16(RVA_UNIT_HP + at) >= healArmyMaxHp(unit) || (healArmyUnitState(unit) & HEAL_ARMY_DISEASED)) return 0;
    int match = 0;
    for (int i = 0; i < healArmy.count && !match; i++)
        if ((healArmy.rules[i].slot || healArmy.rules[i].source != HEAL_ARMY_SPELL) && (rate >= 1 || healArmy.rules[i].bypassNoheal) &&
            unitHasWeaponTrait(unit, &healArmy.rules[i].trait))
            match = 1;
    if (!match) return 0;
    int x = G32(RVA_UNIT_X + at), y = G32(RVA_UNIT_X + 4 + at), player = (short)G16(RVA_UNIT_PLAYER + at), last = G32(RVA_UNIT_COUNT);
    for (int c = 0; c <= last && c < UNIT_TABLE_MAX; c++) {
        size_t cat = (size_t)c * UNIT_SIZE;
        if (G32(RVA_UNIT_X + cat) != x || G32(RVA_UNIT_X + 4 + cat) != y || (short)G16(RVA_UNIT_HP + cat) <= 0) continue;
        if (!playerSameTeam(player, (short)G16(RVA_UNIT_PLAYER + cat))) continue;
        for (int i = 0; i < healArmy.count; i++) {
            const HealArmyRule *r = &healArmy.rules[i];
            if ((!r->slot && r->source == HEAL_ARMY_SPELL) || (rate < 1 && !r->bypassNoheal) || !unitHasWeaponTrait(unit, &r->trait) ||
                !healArmyGiverHas(c, r))
                continue;
            writeLog("heal_army_heals", "unit=%d caster=%d %s=%d trait=%s rate=%d", unit, c, HEAL_ARMY_SOURCE_WORDS[r->source], r->slot, r->trait.word, rate);
            return 1;
        }
    }
    return 0;
}

static void healArmyInfoLines(int spell) {
    char line[160];
    for (int i = 0; i < healArmy.count; i++) {
        const HealArmyRule *r = &healArmy.rules[i];
        if (r->source != HEAL_ARMY_SPELL || r->slot != spell || !spell) continue;
        snprintf(line, sizeof line, "Knowing this spell grants increased healing rate for all %s beings in the army%s.", r->trait.word,
                 r->bypassNoheal ? ", even never healing ones" : "");
        textButton(0, line);
        listGap(4.0f);
    }
}

static int healRateHooksIn;

// ---- The game's side -----------------------------------------------------------------------------
static int gameHealArmyPathLevel(int unit, int path) { return GAME_FN(int (*)(int, int), RVA_UNIT_PATH_LEVEL)(unit, path); }
static unsigned long long gameHealArmyUnitState(int unit) { return GAME_FN(unsigned long long (*)(int), RVA_UNIT_STATE_FLAGS)(unit); }
static int gameHealArmyMaxHp(int unit) { return GAME_FN(int (*)(int), RVA_UNIT_MAX_HP)(unit); }
static int gameHealArmyFalselife(int unit) {
    return GAME_FN(int (*)(int, int, int, char *, int, int *), RVA_UNIT_ABILITY_TOTAL)(unit, ABILITY_FALSELIFE, 1, NULL, 0, NULL);
}

static void healArmyFlagLinesHook(int spell) {
    GAME_FN(void (*)(int), RVA_SPELL_FLAG_LINES)(spell);
    healArmyInfoLines(spell);
}

// ---- UNIT_HEAL_RATE: how a unit heals between turns, as a site -------------------------------------
static struct { int unit, game, rate; } healRateAnswer = {-1, -1, 0};
static int (*healRateOfGame)(int unit);

static int unitHealRateAsGame(int unit) {
    int rate = unit == healRateAnswer.unit && healRateAnswer.game >= 0 ? healRateAnswer.game : healRateOfGame(unit);
    return healArmyHealsFast(unit, rate) ? HEAL_RATE_FAST : rate;
}

static int unitHealRateFirst(int unit, int game) {
    healRateAnswer.unit = unit, healRateAnswer.game = game;
    int rate = ((int (*)(int))siteEntry(SITE_UNIT_HEAL_RATE, (void *)unitHealRateAsGame))(unit);
    rate = rate < HEAL_RATE_NEVER ? HEAL_RATE_NEVER : rate > HEAL_RATE_FAST ? HEAL_RATE_FAST : rate;
    healRateAnswer.rate = rate;
    return rate;
}

#ifndef __APPLE__
static int gameHealRate(int unit) { return GAME_FN(int (*)(int), RVA_UNIT_HEAL_RATE)(unit); }

static int healRateFirstHook(int unit) { return unitHealRateFirst(unit, -1); }

static int healRateSecondHook(int unit) { return unit == healRateAnswer.unit ? healRateAnswer.rate : gameHealRate(unit); }

#define HEAL_ARMY_STUB_SIZE 0x20
static int installHealArmyCalls(void) {
    static const struct {
        unsigned int callRva, targetRva;
        void *hook;
        const char *name;
    } CALLS[] = {
        {RVA_UPKEEP_HEAL_RATE_CALL, RVA_UNIT_HEAL_RATE, (void *)healRateFirstHook, "upkeep_heal_rate"},
        {RVA_UPKEEP_HEAL_RATE_CALL_2, RVA_UNIT_HEAL_RATE, (void *)healRateSecondHook, "upkeep_heal_rate_2"},
        {RVA_SPELL_FLAG_LINES_CALL_A, RVA_SPELL_FLAG_LINES, (void *)healArmyFlagLinesHook, "spell_flag_lines_a"},
        {RVA_SPELL_FLAG_LINES_CALL_B, RVA_SPELL_FLAG_LINES, (void *)healArmyFlagLinesHook, "spell_flag_lines_b"},
        {RVA_SPELL_FLAG_LINES_CALL_C, RVA_SPELL_FLAG_LINES, (void *)healArmyFlagLinesHook, "spell_flag_lines_c"},
    };
    healRateOfGame = gameHealRate;
    unsigned char *stubs = allocNear(gameBase + RVA_TEXT_SECTION);
    if (!stubs) {
        writeLog("hook_skipped", "site=heal_army reason=no_near_page");
        return 0;
    }
    makePagesReadExecute(stubs, 0x1000);
    int ok = 0, rate = 0, n = (int)(sizeof CALLS / sizeof CALLS[0]);
    for (int i = 0; i < n; i++) {
        int in = redirectCall(CALLS[i].callRva, CALLS[i].targetRva, stubs + HEAL_ARMY_STUB_SIZE * i, CALLS[i].hook, CALLS[i].name, NULL);
        ok += in;
        rate += i < 2 && in;
    }
    healRateHooksIn = rate == 2;
    return ok == n;
}
#else
static int healRateOfWord(unsigned long long res, int falselife) {
    return (res & RESIST_NOHEAL_BIT) && falselife < 1 ? HEAL_RATE_NEVER : (res & RESIST_FASTHEAL_BIT) ? HEAL_RATE_FAST : HEAL_RATE_USUAL;
}

static unsigned long long healRateFirstWord(unsigned long long res, int game, int rate) {
    if (rate == game) return res;
    return rate >= HEAL_RATE_FAST ? (res & ~RESIST_NOHEAL_BIT) | RESIST_FASTHEAL_BIT : res & ~RESIST_FASTHEAL_BIT;
}

static unsigned long long healRateSecondWord(unsigned long long res, int game, int rate) {
    if (rate == game) return res;
    return rate <= HEAL_RATE_NEVER ? res | RESIST_NOHEAL_BIT : res & ~RESIST_NOHEAL_BIT;
}

static int gameHealRate(int unit) {
    unsigned long long res = GAME_FN(unsigned long long (*)(int), RVA_UNIT_RESISTANCES)(unit);
    return healRateOfWord(res, res & RESIST_NOHEAL_BIT ? healArmyFalselife(unit) : 0);
}

// @platform-split heal_army: Mac's upkeep inlines RVA_UNIT_HEAL_RATE, so its RVA_UNIT_RESISTANCES and falselife calls answer instead
static unsigned long long healRateFirstResistHook(int unit) {
    unsigned long long res = GAME_FN(unsigned long long (*)(int), RVA_UNIT_RESISTANCES)(unit);
    int game = healRateOfWord(res, res & RESIST_NOHEAL_BIT ? healArmyFalselife(unit) : 0);
    return healRateFirstWord(res, game, unitHealRateFirst(unit, game));
}
static unsigned long long healRateSecondResistHook(int unit) {
    unsigned long long res = GAME_FN(unsigned long long (*)(int), RVA_UNIT_RESISTANCES)(unit);
    return unit == healRateAnswer.unit ? healRateSecondWord(res, healRateAnswer.game, healRateAnswer.rate) : res;
}
static int healRateFalselifeHook(int unit, int ability, int add, char *out, int outIf, int *last) {
    if (unit == healRateAnswer.unit && healRateAnswer.rate != healRateAnswer.game) return healRateAnswer.rate > HEAL_RATE_NEVER;
    return GAME_FN(int (*)(int, int, int, char *, int, int *), RVA_UNIT_ABILITY_TOTAL)(unit, ability, add, out, outIf, last);
}

static int installHealArmyCalls(void) {
    static const struct {
        unsigned int callRva, targetRva;
        void *hook;
        const char *name;
    } CALLS[] = {
        {RVA_UPKEEP_HEAL_RESIST_CALL, RVA_UNIT_RESISTANCES, (void *)healRateFirstResistHook, "upkeep_heal_rate"},
        {RVA_UPKEEP_HEAL_RESIST_CALL_2, RVA_UNIT_RESISTANCES, (void *)healRateSecondResistHook, "upkeep_heal_rate_2"},
        {RVA_UPKEEP_HEAL_FALSELIFE_CALL, RVA_UNIT_ABILITY_TOTAL, (void *)healRateFalselifeHook, "upkeep_heal_falselife"},
    };
    healRateOfGame = gameHealRate;
    int n = (int)(sizeof CALLS / sizeof CALLS[0]);
    for (int i = 0; i < n; i++)
        if (blTarget(gameBase + CALLS[i].callRva) != gameBase + CALLS[i].targetRva) {
            writeLog("hook_skipped", "site=%s reason=not_the_expected_call va=0x%llx", CALLS[i].name, CALLS[i].callRva + 0x100000000ULL);
            return 0;
        }
    for (int i = 0; i < n; i++) {
        unsigned char *call = gameBase + CALLS[i].callRva, *trampoline = newTrampoline(CALLS[i].hook);
        uint32_t word = trampoline ? blTo(call, trampoline) : 0;
        if (!trampoline || !writeGameCode(call, &word, 4)) {
            writeLog("hook_skipped", "site=%s reason=%s", CALLS[i].name, trampoline ? "code_unwritable" : "no_padding");
            return 0;
        }
    }
    healRateHooksIn = 1;
    return macRedirectCalls(RVA_SPELL_FLAG_LINES, (void *)healArmyFlagLinesHook, "spell_flag_lines") > 0;
}
#endif

static void healArmyInit(void) {
    healArmyPathLevel = gameHealArmyPathLevel;
    healArmyUnitState = gameHealArmyUnitState;
    healArmyMaxHp = gameHealArmyMaxHp;
    healArmyFalselife = gameHealArmyFalselife;
    if (!weaponTraitUnitFlags) {
        weaponTraitUnitFlags = gameUnitResistances;
        weaponTraitUnitMoves = gameUnitMoveFlags;
        weaponTraitUnitAbility = gameUnitAbility;
    }
    int words = apiAddModCommand("healarmy", healarmyLine);
    int calls = installHealArmyCalls();
    writeResultLog(words == 1 && calls, "heal_army_installed", "words=%d of 1 calls=%d", words, calls);
}
