#pragma once

#define AFFLICTION_STAND_IN_MARK   (0xc4ULL << 55)
#define AFFLICTION_STAND_IN_MASK   (0xffULL << 55)
#define AFFLICTION_CARD_ICON       0x12
#define AFFLICTION_HOLDERS_MAX     10000
#define AFFLICTION_WORD(unit)      (*(unsigned long long *)(gameBase + RVA_UNITS + (size_t)(unit) * UNIT_SIZE))
#define AFFRES_CHANCE_MAX          1000

static int customAfflictionsState;
static int installCustomAfflictions(void);

// ---- newaffliction and its block -------------------------------------------------------------------
static struct {
    int open, kind, mod, line;
    unsigned fileId;
    int again;
    int refused;
} afflictionBlock;

static void finishAfflictionBlock(void) { afflictionBlock.open = 0; }

static void refuseAfflictionBlock(int mod) {
    afflictionBlock.open = 1, afflictionBlock.refused = 1, afflictionBlock.kind = -1, afflictionBlock.mod = mod;
    afflictionBlock.line = G32(RVA_MOD_LINE), afflictionBlock.fileId = modFileNow(), afflictionBlock.again = 0;
}

static void newafflictionLine(const char *args, int mod, int level) {
    if (level != 2) return;
    finishSpellBlock();
    finishAfflictionBlock();
    clearParserCurrentRecords();
    const char *p = args;
    char name[CUSTOM_AFFLICTION_NAME_SIZE];
    int length = readSpellQuoted(&p, name, sizeof name);
    if (length <= 0) {
        refuseC5mLine("newaffliction", spellModName(mod), "no_name", "line=%d text=%.60s", G32(RVA_MOD_LINE), args);
        refuseAfflictionBlock(mod);
        return;
    }
    if (length >= CUSTOM_AFFLICTION_NAME_SIZE) {
        refuseC5mLine("newaffliction", spellModName(mod), "name_too_long", "line=%d max=%d", G32(RVA_MOD_LINE), CUSTOM_AFFLICTION_NAME_SIZE - 1);
        refuseAfflictionBlock(mod);
        return;
    }
    if (!isLineEnd(p)) {
        refuseC5mLine("newaffliction", spellModName(mod), "bad_value", "line=%d text=%.60s", G32(RVA_MOD_LINE), args);
        refuseAfflictionBlock(mod);
        return;
    }
    int k = findCustomAffliction(name), again = customAfflictionDeclared(k);
    if (k < 0 && (k = addCustomAffliction(name)) < 0) {
        refuseC5mLine("newaffliction", spellModName(mod), "too_many_afflictions", "line=%d name=%.63s max=%d", G32(RVA_MOD_LINE), name,
                      CUSTOM_AFFLICTIONS_MAX);
        refuseAfflictionBlock(mod);
        return;
    }
    CustomAffliction *a = &customAfflictions[k];
    if (!again) {
        a->declared = 1, a->battleaff = 0, a->descr[0] = '\0', a->icon = a->picture = -1;
        snprintf(a->mod, sizeof a->mod, "%s", spellModName(mod));
        if (traitDescriptions) traitDescriptions[TRAIT_DESCRIPTION_SLOTS + k] = a->descr;
    }
    afflictionBlock.open = 1, afflictionBlock.refused = 0, afflictionBlock.kind = k, afflictionBlock.mod = mod, afflictionBlock.line = G32(RVA_MOD_LINE);
    afflictionBlock.fileId = modFileNow(), afflictionBlock.again = again;
    installCustomAfflictions();
}

static void afflictionDescrLine(const char *args, int mod) {
    CustomAffliction *a = &customAfflictions[afflictionBlock.kind];
    const char *p = args;
    char text[CUSTOM_AFFLICTION_DESCR_SIZE];
    int length = readSpellQuoted(&p, text, sizeof text);
    const char *reason = length < 0 || !isLineEnd(p) ? "no_text" : length >= CUSTOM_AFFLICTION_DESCR_SIZE ? "text_too_long"
                       : afflictionBlock.again && strcmp(a->descr, text) ? "other_declaration" : NULL;
    if (reason) {
        refuseC5mLine("descr", spellModName(mod), reason, "line=%d name=%.63s max=%d first_mod=%.63s", G32(RVA_MOD_LINE), a->name,
                      CUSTOM_AFFLICTION_DESCR_SIZE - 1, a->mod);
        return;
    }
    snprintf(a->descr, sizeof a->descr, "%s", text);
}

static int afflictionWithText(const char *name) {
    int k = findCustomAffliction(name);
    return customAfflictionDeclared(k) ? k : -1;
}

static const char *afflictionText(int k) {
    return customAfflictionDeclared(k) && customAfflictions[k].descr[0] ? customAfflictions[k].descr : NULL;
}

static void afflictionIconLine(const char *args, int mod) {
    CustomAffliction *a = &customAfflictions[afflictionBlock.kind];
    const char *p = args, *reason = NULL;
    int icon = -1, picture = -1;
    if (*p == '"') {
        char file[MAX_PATH], path[MAX_PATH];
        int length = readSpellQuoted(&p, file, sizeof file);
        reason = length <= 0 || !isLineEnd(p) ? "bad_value" : findModImage(modFolder(mod), file, path, sizeof path);
        if (!reason && (picture = addTraitPicture(path)) < 0) reason = "too_many_pictures";
    } else {
        long long n;
        reason = !readSpellNumber(&p, &n) || !isLineEnd(p) || n < 0 || n >= RES_TRS_SPRITES ? "bad_icon" : NULL;
        if (!reason) icon = (int)n;
    }
    if (!reason && afflictionBlock.again && (a->icon != icon || a->picture != picture)) reason = "other_declaration";
    if (reason) {
        refuseC5mLine("icon", spellModName(mod), reason, "line=%d name=%.63s text=%.60s first_mod=%.63s", G32(RVA_MOD_LINE), a->name, args, a->mod);
        return;
    }
    a->icon = icon, a->picture = picture;
}

static int afflictionCardIcon(int k) {
    const CustomAffliction *a = &customAfflictions[k];
    if (a->picture >= 0) {
        int sprite = traitPictureSprite(a->picture);
        return sprite < 0 ? AFFLICTION_CARD_ICON : sprite;
    }
    return a->icon >= 0 ? a->icon : AFFLICTION_CARD_ICON;
}

static void afflictionBattleaffLine(const char *args, int mod) {
    CustomAffliction *a = &customAfflictions[afflictionBlock.kind];
    if (!isLineEnd(args) || (afflictionBlock.again && !a->battleaff)) {
        refuseC5mLine("battleaff", spellModName(mod), isLineEnd(args) ? "other_declaration" : "bad_value", "line=%d name=%.63s first_mod=%.63s",
                      G32(RVA_MOD_LINE), a->name, a->mod);
        return;
    }
    a->battleaff = 1;
}

static void battleaffLine(const char *args, int mod, int level) {
    (void)args;
    if (level != 2) return;
    refuseC5mLine("battleaff", spellModName(mod), "no_affliction_block", "line=%d", G32(RVA_MOD_LINE));
}

// ---- affres, afftarg and cureoneaff with a name ----------------------------------------------------------
typedef struct { int ritual, number, cure, kind, chance; } AfflictionRitualLine;
static AfflictionRitualLine *afflictionRitualLines;
static int afflictionRitualLineCount, afflictionRitualLineCapacity;

static int readAfflictionName(const char **p, const char *word, const char *modName, const char *text) {
    char name[CUSTOM_AFFLICTION_NAME_SIZE];
    int length = readSpellQuoted(p, name, sizeof name);
    if (length <= 0 || length >= CUSTOM_AFFLICTION_NAME_SIZE) {
        refuseC5mLine(word, modName, "no_name", "line=%d text=%.60s", G32(RVA_MOD_LINE), text);
        return -1;
    }
    int k = findCustomAffliction(name);
    if (!customAfflictionDeclared(k)) {
        refuseC5mLine(word, modName, "unknown_affliction", "line=%d name=%.63s", G32(RVA_MOD_LINE), name);
        return -1;
    }
    return k;
}

static void affresNamedLine(const char *args, int mod) {
    const char *p = args, *m = spellModName(mod);
    int monster = G32(RVA_MONSTER_CURRENT), item = G32(RVA_ITEM_CURRENT);
    int isMonster = monsterRecord(monster) != NULL, isItem = !isMonster && item >= 0 && item < tableRecords(TABLE_ITEM);
    if (!isMonster && !isItem) {
        refuseC5mLine("affres", m, "no_monster_or_item", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    int k = readAfflictionName(&p, "affres", m, args);
    if (k < 0) return;
    long long chance = 0;
    if (!readSpellNumber(&p, &chance) || !isLineEnd(p) || chance < 0 || chance > AFFRES_CHANCE_MAX) {
        refuseC5mLine("affres", m, "bad_value", "line=%d text=%.60s min=0 max=%d", G32(RVA_MOD_LINE), args, AFFRES_CHANCE_MAX);
        return;
    }
    int record = isItem ? item : monster;
    for (int i = 0; i < affresRuleCount; i++)
        if (affresRules[i].item == isItem && affresRules[i].record == record && affresRules[i].kind == k) {
            affresRules[i].chance = (int)chance;
            return;
        }
    if (!growList((void **)&affresRules, &affresRuleCapacity, affresRuleCount + 1, sizeof affresRules[0])) {
        refuseC5mLine("affres", m, "out_of_memory", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    affresRules[affresRuleCount++] = (AffresRule){isItem, record, k, (int)chance};
}

static const char *ritualAfflictionLine(const char *word, const char *args, int cure, int mod) {
    static char standIn[64];
    const char *p = args, *m = spellModName(mod);
    int ritual = G32(RVA_RITUAL_CURRENT);
    if (ritual < 0 || ritual >= ritualSlotCount()) {
        refuseC5mLine(word, m, "no_ritual", "line=%d", G32(RVA_MOD_LINE));
        return NULL;
    }
    int k = readAfflictionName(&p, word, m, args);
    if (k < 0) return NULL;
    long long chance = 100;
    if (cure && !readSpellNumber(&p, &chance)) chance = -1;
    if (!isLineEnd(p) || chance < 0 || chance > 100) {
        refuseC5mLine(word, m, "bad_value", "line=%d text=%.60s min=0 max=100", G32(RVA_MOD_LINE), args);
        return NULL;
    }
    int number = 0;
    for (int i = 0; i < afflictionRitualLineCount; i++) number += afflictionRitualLines[i].ritual == ritual;
    if (number >= RITUAL_FX_MAX) {
        refuseC5mLine(word, m, "too_many_effects", "line=%d max=%d", G32(RVA_MOD_LINE), RITUAL_FX_MAX);
        return NULL;
    }
    if (!growList((void **)&afflictionRitualLines, &afflictionRitualLineCapacity, afflictionRitualLineCount + 1, sizeof afflictionRitualLines[0])) {
        refuseC5mLine(word, m, "out_of_memory", "line=%d", G32(RVA_MOD_LINE));
        return NULL;
    }
    afflictionRitualLines[afflictionRitualLineCount++] = (AfflictionRitualLine){ritual, number, cure, k, (int)chance};
    snprintf(standIn, sizeof standIn, "afftarg %llu", AFFLICTION_STAND_IN_MARK | (unsigned long long)number << 47);
    return standIn;
}

static int namedAfflictionWord(const char *line, size_t len, const char *word) {
    if (!spellWordIs(line, len, word)) return 0;
    const char *p = line + len;
    skipSpaces(&p);
    return *p == '"';
}

static int afflictionIconOutsideContent(const char *line, size_t len, int mod, int level) {
    static int inBlock, blockMod = -1, blockLevel = -1;
    static unsigned int blockFile;
    if (mod != blockMod || level != blockLevel || modFileNow() != blockFile) inBlock = 0;
    blockMod = mod, blockLevel = level, blockFile = modFileNow();
    if (spellWordIs(line, len, "newaffliction")) return inBlock = 1, 0;
    if (!inBlock) return 0;
    if (spellWordIs(line, len, "icon")) return 1;
    if (!spellWordIs(line, len, "descr") && !spellWordIs(line, len, "battleaff")) inBlock = 0;
    return 0;
}

static const char *afflictionWordLineHook(const char *line, size_t len, int mod, int level) {
    if (level != 2) return afflictionIconOutsideContent(line, len, mod, level) ? NULL : line;
    const char *args = line + len;
    skipSpaces(&args);
    if (afflictionBlock.open && (afflictionBlock.mod != mod || afflictionBlock.fileId != modFileNow())) finishAfflictionBlock();
    if (afflictionBlock.open && afflictionBlock.refused) {
        int descr = spellWordIs(line, len, "descr"), icon = spellWordIs(line, len, "icon");
        if (descr || icon || spellWordIs(line, len, "battleaff")) {
            refuseC5mLine(descr ? "descr" : icon ? "icon" : "battleaff", spellModName(mod), "no_affliction_block", "line=%d declaration_line=%d",
                          G32(RVA_MOD_LINE), afflictionBlock.line);
            return NULL;
        }
        if (spellWordIs(line, len, "end")) return finishAfflictionBlock(), NULL;
        finishAfflictionBlock();
    }
    if (afflictionBlock.open) {
        if (spellWordIs(line, len, "descr")) return afflictionDescrLine(args, mod), NULL;
        if (spellWordIs(line, len, "battleaff")) return afflictionBattleaffLine(args, mod), NULL;
        if (spellWordIs(line, len, "icon")) return afflictionIconLine(args, mod), NULL;
        if (spellWordIs(line, len, "end")) return finishAfflictionBlock(), NULL;
        finishAfflictionBlock();
    }
    if (namedAfflictionWord(line, len, "affres")) return affresNamedLine(args, mod), NULL;
    int item = G32(RVA_MONSTER_CURRENT) < 0 && G32(RVA_ITEM_CURRENT) >= 0;
    if (namedAfflictionWord(line, len, "afftarg")) return ritualAfflictionLine("afftarg", args, 0, mod);
    if (namedAfflictionWord(line, len, "cureoneaff")) {
        if (!item) return ritualAfflictionLine("cureoneaff", args, 1, mod);
        refuseC5mLine("cureoneaff", spellModName(mod), "not_in_ritual", "line=%d text=%.60s", G32(RVA_MOD_LINE), args);
        return NULL;
    }
    if (namedAfflictionWord(line, len, "makeaff")) {
        refuseC5mLine("makeaff", spellModName(mod), "not_in_event", "line=%d text=%.60s", G32(RVA_MOD_LINE), args);
        return NULL;
    }
    return line;
}

static int declinedAfflictionLine(const char *line, size_t len, int mod, int level) {
    static int inBlock, blockMod = -1, blockLevel = -1;
    if (mod != blockMod || level != blockLevel) inBlock = 0;
    blockMod = mod, blockLevel = level;
    if (spellWordIs(line, len, "newaffliction")) return inBlock = 1;
    if (inBlock && (spellWordIs(line, len, "descr") || spellWordIs(line, len, "battleaff") || spellWordIs(line, len, "icon") ||
                    spellWordIs(line, len, "end")))
        return 1;
    inBlock = 0;
    return namedAfflictionWord(line, len, "affres") || namedAfflictionWord(line, len, "afftarg") || namedAfflictionWord(line, len, "cureoneaff") ||
           namedAfflictionWord(line, len, "makeaff");
}

static int isNamedAfflictionEventLine(const char *line) {
    size_t len = strcspn(line, " \t");
    return namedAfflictionWord(line, len, "makeaff") || namedAfflictionWord(line, len, "cureoneaff") ||
           (len == 15 && !strncmp(line, "+hasaffunithere", 15) && strchr(line, '"'));
}

static void afflictionWordsBeforePass(int level) {
    if (level != 2) return;
    for (int k = 0; k < customAfflictionCount; k++) customAfflictions[k].declared = 0;
    affresRuleCount = afflictionRitualLineCount = 0;
    finishAfflictionBlock();
}

static void copyAffresRules(int item, int source, int target) {
    for (int i = 0; i < affresRuleCount; i++)
        if (affresRules[i].item == item && affresRules[i].record == target) affresRules[i--] = affresRules[--affresRuleCount];
    int count = affresRuleCount;
    for (int i = 0; i < count; i++)
        if (affresRules[i].item == item && affresRules[i].record == source &&
            growList((void **)&affresRules, &affresRuleCapacity, affresRuleCount + 1, sizeof affresRules[0])) {
            affresRules[affresRuleCount] = affresRules[i];
            affresRules[affresRuleCount++].record = target;
        }
}

static void clearAffresRules(int item, int record) {
    for (int i = 0; i < affresRuleCount; i++)
        if (affresRules[i].item == item && affresRules[i].record == record) affresRules[i--] = affresRules[--affresRuleCount];
}

// ---- Events: makeaff, cureoneaff and +hasaffunithere with a name ------------------------------------------
static int eventTargetUnit(int square) {
    int unit = ((const int *)GAME_PTR(RVA_SQUARE_EVENT_B))[square];
    return unit >= 0 && unit < UNIT_TABLE_MAX ? unit : -1;
}

static int gameEventPlayerArg(int player) { return GAME_FN(int (*)(int), RVA_EVENT_PLAYER_ARG)(player); }
static int (*afflictionEventPlayer)(int player) = gameEventPlayerArg;

static int pickAfflictedUnitAt(int player, int x, int y, int k) {
    if (!finderArmyHere(player, x, y)) return -1;
    int ok, first = finderSquareFirst(x, y, &ok), n = 0;
    if (!ok) return -1;
    for (int u = first; u != FINDER_NO_UNIT && n < AFFLICTION_HOLDERS_MAX; u = finderNext[u])
        if ((player < 0 || (short)G16(RVA_UNIT_PLAYER + (size_t)u * UNIT_SIZE) == player) && unitAfflictionEntry(u, k)) n++;
    if (!n) return -1;
    int pick = finderRandom(n);
    for (int u = first; u != FINDER_NO_UNIT; u = finderNext[u])
        if ((player < 0 || (short)G16(RVA_UNIT_PLAYER + (size_t)u * UNIT_SIZE) == player) && unitAfflictionEntry(u, k) && pick-- == 0) return u;
    return -1;
}

static int afflictionEventSquare(int word, int square, int player, int k) {
    if (word == 0 || word == 1) {
        int unit = eventTargetUnit(square);
        if (unit >= 0 && afflictionUnitInUse(unit) && ensureAfflictionStore()) word ? cureCustomAffliction(unit, k) : giveCustomAffliction(unit, k);
        return 1;
    }
    if (!G16(RVA_SQUARE_ARMY + (size_t)square * 2)) return 0;
    int unit = pickAfflictedUnitAt(player, square / MAP_COLUMN_SQUARES, square % MAP_COLUMN_SQUARES, k);
    if (unit < 0) return 0;
    ((int *)GAME_PTR(RVA_SQUARE_EVENT_B))[square] = unit;
    noteEventUnitWrite((size_t)square);
    return 1;
}

static int afflictionEventLine(const char *line, int level, const char *mod) {
    size_t len = strcspn(line, " \t");
    int word = namedAfflictionWord(line, len, "makeaff") ? 0 : namedAfflictionWord(line, len, "cureoneaff") ? 1
             : len == 15 && !strncmp(line, "+hasaffunithere", 15) && strchr(line, '"') ? 2 : -1;
    if (word < 0) return 0;
    if (level != 3 || G32(RVA_EVENT_STATE) < 1) return 1;
    const char *p = line + len, *name = word == 2 ? "+hasaffunithere" : word ? "cureoneaff" : "makeaff";
    long long player = -1;
    if (word == 2 && !readSpellNumber(&p, &player)) {
        refuseC5mLine(name, mod, "bad_value", "line=%d file=%.200s text=%.60s", G32(RVA_MOD_LINE), (const char *)(gameBase + RVA_EVENT_FILE), line);
        G32(RVA_EVENT_STATE) = EVENT_FAILED;
        return 1;
    }
    int k = readAfflictionName(&p, name, mod, line);
    if (k >= 0 && !isLineEnd(p)) refuseC5mLine(name, mod, "bad_value", "line=%d file=%.200s text=%.60s", G32(RVA_MOD_LINE), (const char *)(gameBase + RVA_EVENT_FILE), line), k = -1;
    if (k < 0) {
        if (word == 2) G32(RVA_EVENT_STATE) = EVENT_FAILED;
        return 1;
    }
    int who = word == 2 ? afflictionEventPlayer((int)player) : -1;
    int tx = G32(RVA_EVENT_TARGET_X), ty = G32(RVA_EVENT_TARGET_Y), target = tx >= 0 && ty >= 0 ? tx * MAP_COLUMN_SQUARES + ty : -1;
    if (word == 2) eventFindersLineBegin();
    if (G32(RVA_EVENT_STATE) == PLAYER_EVENT) {
        if (target >= 0 && !afflictionEventSquare(word, target, who, k)) G32(RVA_EVENT_STATE) = EVENT_FAILED;
    } else {
        unsigned char *marks = gameBase + RVA_SQUARE_EVENT_A;
        int any = 0;
        for (size_t i = 0; i < SQUARE_EVENT_A_SIZE; i++) {
            if (!marks[i]) continue;
            if (afflictionEventSquare(word, target >= 0 ? target : (int)i, who, k)) any = 1;
            else marks[i] = 0;
        }
        if (word == 2 && !any) G32(RVA_EVENT_STATE) = EVENT_FAILED;
    }
    if (word == 2) eventFindersLineEnd();
    return 1;
}

// ---- Rituals: the stand-in afftarg at its cast --------------------------------------------------------------
#define AFFLICTION_CASTS_MAX 8
static int afflictionCastRituals[AFFLICTION_CASTS_MAX], afflictionCastDepth, afftargUnit = -1;
typedef struct { int unit; unsigned long long bits; } AfftargStandIn;
static AfftargStandIn afftargStandIns[RITUAL_FX_MAX * AFFLICTION_CASTS_MAX];
static int afftargStandInCount;

static long long (*nextAfflictionExecute)(int, int, int, int, int, int);
static long long afflictionExecuteRitual(int unit, int ritual, int x, int y, int percent, int pay) {
    if (afflictionCastDepth < AFFLICTION_CASTS_MAX) afflictionCastRituals[afflictionCastDepth] = ritual;
    afflictionCastDepth++;
    long long result = nextAfflictionExecute(unit, ritual, x, y, percent, pay);
    afflictionCastDepth--;
    for (int i = 0; i < afftargStandInCount; i++) AFFLICTION_WORD(afftargStandIns[i].unit) &= ~afftargStandIns[i].bits;
    afftargStandInCount = 0;
    return result;
}

static const char *gameAfftargTraceName(int unit) { return GAME_FN(const char *(*)(int), RVA_UNIT_TRACE_NAME)(unit); }
static void gameAfftargTrace(const char *format, const char *name, unsigned high, unsigned low) {
    GAME_FN(void (*)(const char *, ...), RVA_TRACE)(format, name, high, low);
}
static unsigned gameAfflictionList(int unit, unsigned long long flags) {
    return GAME_FN(unsigned (*)(int, unsigned long long), RVA_UNIT_AFFLICTION_LIST)(unit, flags);
}
static int gameAfflictionLine(int icon, const char *name) { return GAME_FN(AbilityLineFn, RVA_ABILITY_LIST)(icon, "%s", name); }
static const char *(*afflictionTraceName)(int unit) = gameAfftargTraceName;
static void (*afflictionTrace)(const char *format, const char *name, unsigned high, unsigned low) = gameAfftargTrace;
static unsigned (*afflictionGameList)(int unit, unsigned long long flags) = gameAfflictionList;
static int (*afflictionCardLine)(int icon, const char *name) = gameAfflictionLine;

static const char *afftargTraceNameHook(int unit) {
    afftargUnit = unit;
    return afflictionTraceName(unit);
}

static void runAfftargStandIn(unsigned long long value) {
    int unit = afftargUnit, ritual = afflictionCastDepth > 0 && afflictionCastDepth <= AFFLICTION_CASTS_MAX ? afflictionCastRituals[afflictionCastDepth - 1] : -1;
    afftargUnit = -1;
    if (unit < 0 || unit >= UNIT_TABLE_MAX) return;
    unsigned long long word = AFFLICTION_WORD(unit);
    if (afftargStandInCount < (int)(sizeof afftargStandIns / sizeof afftargStandIns[0]))
        afftargStandIns[afftargStandInCount++] = (AfftargStandIn){unit, value & ~word};
    int number = (int)(value >> 47 & 0xff);
    for (int i = 0; i < afflictionRitualLineCount; i++) {
        const AfflictionRitualLine *r = &afflictionRitualLines[i];
        if (r->ritual != ritual || r->number != number || !customAfflictionDeclared(r->kind) || !afflictionUnitInUse(unit) || !ensureAfflictionStore()) continue;
        if (!r->cure) giveCustomAffliction(unit, r->kind);
        else if (afflictionRandom(100) < r->chance) cureCustomAffliction(unit, r->kind);
        return;
    }
}

static void afftargTraceHook(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    const char *name = va_arg(ap, const char *);
    unsigned high = va_arg(ap, unsigned), low = va_arg(ap, unsigned);
    va_end(ap);
    unsigned long long value = (unsigned long long)high << 32 | low;
    if ((value & AFFLICTION_STAND_IN_MASK) == AFFLICTION_STAND_IN_MARK && !(value & ((1ULL << 47) - 1))) runAfftargStandIn(value);
    afflictionTrace(format, name, high, low);
}

// ---- The game's cures, the AI's Elixir, the immortal merge, the Legion of Gods ----------------------------
static u64 (*nextUnitCureAffliction)(u64);
static u64 unitCureAfflictionHook(u64 unit) {
    u64 cured = nextUnitCureAffliction(unit);
    if ((int)cured || !anyCustomAfflictions()) return cured;
    int k = firstBattleAffliction((int)unit);
    return k >= 0 && cureCustomAffliction((int)unit, k) ? 1 : cured;
}

static int (*gameAiItemUseRatingFn)(int, int);
static int aiItemUseRatingHook(int unit, int item) {
    int rating = gameAiItemUseRatingFn(unit, item);
    if (rating != 1 || !anyCustomAfflictions() || item < 0 || item >= tableRecords(TABLE_ITEM)) return rating;
    return itemFirstAbility(item, ITEM_ABILITY_CURE_AFFLICTION) > 0 && firstBattleAffliction(unit) >= 0 ? 2 : rating;
}

static u64 (*nextAfflictionRemoveUnit)(u64);
static u64 afflictionRemoveUnitHook(u64 unit) {
    int host = (int)unit, to = reformingUnit;
    if (to >= 0 && host != to && host >= 0 && host < UNIT_TABLE_MAX && to < UNIT_TABLE_MAX &&
        AFFLICTION_WORD(host) == AFFLICTION_WORD(to) && (unitAfflictionCount(host) || unitAfflictionCount(to))) {
        writeLog("custom_afflictions_merged", "from=%d to=%d afflictions=%d", host, to, unitAfflictionCount(host));
        moveUnitAfflictions(host, to);
    }
    return nextAfflictionRemoveUnit(unit);
}

#define LEGION_OF_GODS_NAME "Legion of Gods"
static u64 (*nextAfflictionDamageUnit)(u64, u64);
static u64 afflictionDamageUnitHook(u64 unit, u64 damage) {
    int u = (int)unit, before = unitAfflictionCount(u) ? unitMonsterNumber(u) : -1;
    u64 result = nextAfflictionDamageUnit(unit, damage);
    if (before < 0 || !unitAfflictionCount(u)) return result;
    int after = unitMonsterNumber(u);
    const unsigned char *m = after != before ? monsterRecord(after) : NULL;
    if (m && !AFFLICTION_WORD(u) && !strcmp((const char *)m, LEGION_OF_GODS_NAME)) {
        writeLog("custom_afflictions_cleared", "unit=%d reason=legion_of_gods afflictions=%d", u, unitAfflictionCount(u));
        clearUnitAfflictions(u);
        noteAfflictionUnit(u);
    }
    return result;
}

// ---- The unit card ------------------------------------------------------------------------------
typedef struct { int ref, unit, value; } AfflictionCardCall;
static int pushAfflictionCardCall(lua_State *L, void *arg) {
    const AfflictionCardCall *c = arg;
    lua_rawgeti(L, LUA_REGISTRYINDEX, c->ref);
    lua_pushinteger(L, c->unit);
    lua_pushinteger(L, c->value);
    return 2;
}

static int afflictionCardWords(int k, int unit, int value) {
    CustomAffliction *a = &customAfflictions[k];
    a->cardLabel[0] = a->cardText[0] = '\0';
    if (traitDescriptions) traitDescriptions[TRAIT_DESCRIPTION_SLOTS + k] = a->descr;
    int lua = a->cardLua;
    if (lua < 0 || lua >= luaCallbackCount) return 0;
    LuaCallback c = luaCallbacks[lua];
    if (!c.s || c.s->dead || c.s->retired || !luaCallbackOpenNow(c.s)) return 0;
    AfflictionCardCall call = {c.ref, unit, value};
    if (!callLuaProtected(c.s, pushAfflictionCardCall, &call, 2)) return 0;
    lua_State *L = c.s->L;
    if (lua_type(L, -2) == LUA_TSTRING) {
        const char *label = lua_tostring(L, -2);
        size_t n = textCutLength(label, sizeof a->cardLabel - 1);
        memcpy(a->cardLabel, label, n), a->cardLabel[n] = '\0';
        if (lua_type(L, -1) == LUA_TSTRING) {
            const char *text = lua_tostring(L, -1);
            n = textCutLength(text, sizeof a->cardText - 1);
            memcpy(a->cardText, text, n), a->cardText[n] = '\0';
            if (traitDescriptions) traitDescriptions[TRAIT_DESCRIPTION_SLOTS + k] = a->cardText;
        }
    }
    lua_pop(L, 2);
    closeDeadLuaScript(c.s);
    return a->cardLabel[0] != '\0';
}

static unsigned afflictionListHook(int unit, unsigned long long flags) {
    unsigned count = afflictionGameList(unit, flags);
    if (!unitAfflictionCount(unit)) return count;
    for (int k = 0; k < customAfflictionCount; k++) {
        const AfflictionEntry *e = customAfflictionDeclared(k) ? unitAfflictionEntry(unit, k) : NULL;
        if (!e) continue;
        const char *label = afflictionCardWords(k, unit, e->value) ? customAfflictions[k].cardLabel : customAfflictions[k].name;
        if (afflictionCardLine(afflictionCardIcon(k), label)) count++;
    }
    return count;
}

static int afflictionDescriptionId(const char *text) {
    if (!customAfflictionCount || !traitDescriptions || !text) return 0;
    for (int k = 0; k < customAfflictionCount; k++)
        if (customAfflictionDeclared(k) && (!strcmp(customAfflictions[k].name, text) || (customAfflictions[k].cardLabel[0] && !strcmp(customAfflictions[k].cardLabel, text))))
            return (int)(((uintptr_t)&traitDescriptions[TRAIT_DESCRIPTION_SLOTS + k] - (uintptr_t)GAME_PTR(RVA_ABILITY_DESCRIPTIONS)) / sizeof(char *));
    return 0;
}

// ---- Installing ---------------------------------------------------------------------------------
static int installCustomAfflictions(void) {
    if (customAfflictionsState) return customAfflictionsState > 0;
    customAfflictionsState = -1;
    if (!ensureAfflictionStore()) return 0;
    afflictionAbilityTotal = GAME_FN(int (*)(int, int, int, char *, int, int *), RVA_UNIT_ABILITY_TOTAL);
    int life = installUnitLifeHooks(), net = installItemSlotNet(), replay = installItemSlotReplay();
    int entries = 0;
    struct { unsigned int rva; void *hook; void **game; } hooks[] = {
        {RVA_AI_ITEM_USE_RATING, (void *)aiItemUseRatingHook, (void **)&gameAiItemUseRatingFn},
    };
    for (size_t i = 0; i < sizeof hooks / sizeof hooks[0]; i++) {
        void *target = gameBase + hooks[i].rva;
        entries += fanpatchHookCreate(target, hooks[i].hook, hooks[i].game) && fanpatchHookEnable(target, 1);
    }
    entries += installComputedHook(SITE_IMMORTAL_REFORM);
    int sites = wrapSite(SITE_UNIT_CURE_AFFLICTION, (void *)unitCureAfflictionHook, (void **)&nextUnitCureAffliction) +
                wrapSite(SITE_REMOVE_UNIT, (void *)afflictionRemoveUnitHook, (void **)&nextAfflictionRemoveUnit) +
                wrapSite(SITE_DAMAGE_UNIT, (void *)afflictionDamageUnitHook, (void **)&nextAfflictionDamageUnit) +
                wrapSite(SITE_EXECUTE_RITUAL, (void *)afflictionExecuteRitual, (void **)&nextAfflictionExecute);
    int calls = redirectBattleCall(RVA_UNIT_AFFLICTION_LIST_CALL, RVA_UNIT_AFFLICTION_LIST, (void *)afflictionListHook, "affliction_card_list") +
                redirectBattleCall(RVA_RITUAL_AFFTARG_NAME_CALL, RVA_UNIT_TRACE_NAME, (void *)afftargTraceNameHook, "afftarg_trace_name") +
                redirectBattleCall(RVA_RITUAL_AFFTARG_TRACE_CALL, RVA_TRACE, (void *)afftargTraceHook, "afftarg_trace");
    int ok = life + net + replay + entries + sites + calls;
    customAfflictionsState = ok == 12 ? 1 : -1;
    writeInstallLogFields("custom_afflictions_installed", ok, 12, "life=%d net=%d replay=%d entries=%d sites=%d calls=%d", life, net, replay,
                          entries, sites, calls);
    return customAfflictionsState > 0;
}

static void customAfflictionsInit(void) {
    int words = apiAddModCommand("newaffliction", newafflictionLine) + apiAddModCommand("battleaff", battleaffLine);
    writeResultLog(words == 2, "custom_afflictions_ready", "words=%d of 2", words);
}

// ---- Lua: coe5.giveAffliction and the rest ---------------------------------------------------------------
static int luaAfflictionArgs(lua_State *L, const char *call, int reads, int *unit, int *kind) {
    lua_Integer u = luaL_checkinteger(L, 1);
    const char *name = luaL_checkstring(L, 2);
    int k = findCustomAffliction(name);
    int inUse = afflictionUnitInUse((int)u) || (reads && u == afflictionEffectUnit);
    const char *reason = refuseFromDeadLuaScript(L, call) ? "dead_script" : !inUse ? "no_unit"
                       : !customAfflictionDeclared(k) ? "unknown_affliction" : !ensureAfflictionStore() ? "out_of_memory" : NULL;
    if (reason) {
        if (strcmp(reason, "dead_script")) refuseLuaCall(runningLuaMod(), call, reason, "unit=%lld name=%.63s", (long long)u, name);
        return 0;
    }
    *unit = (int)u, *kind = k;
    return 1;
}

static int lAfflictionCardLine(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (refuseFromDeadLuaScript(L, "coe5.afflictionCardLine")) return lua_pushnil(L), 1;
    const char *mod = runningLuaMod();
    const char *reason = !name[0] || strlen(name) >= CUSTOM_AFFLICTION_NAME_SIZE ? "bad_value" : !ensureAfflictionStore() ? "out_of_memory" : NULL;
    int k = reason ? -1 : findCustomAffliction(name);
    if (!reason && k < 0 && (k = addCustomAffliction(name)) < 0) reason = "too_many_afflictions";
    int old = k >= 0 ? customAfflictions[k].cardLua : -1;
    if (!reason && old >= 0 && old < luaCallbackCount && !luaCallbackRetired(old) && luaCallbacks[old].s && !luaCallbacks[old].s->dead)
        reason = "card_line_taken";
    int lua = reason ? -1 : newLuaCallback(L, 2, 0);
    if (!reason && lua < 0) reason = "out_of_memory";
    if (reason) {
        refuseLuaCall(mod, "coe5.afflictionCardLine", reason, "name=%.63s", name);
        return lua_pushnil(L), 1;
    }
    customAfflictions[k].cardLua = lua;
    writeLog("affliction_card_line_added", "mod=%s name=%.63s declared=%d", mod, customAfflictions[k].name, customAfflictionDeclared(k));
    lua_pushboolean(L, 1);
    return 1;
}

static int lGiveAffliction(lua_State *L) {
    int unit, k;
    if (!luaAfflictionArgs(L, "coe5.giveAffliction", 0, &unit, &k)) return lua_pushnil(L), 1;
    lua_pushboolean(L, giveCustomAffliction(unit, k));
    return 1;
}

static int lCureAffliction(lua_State *L) {
    int unit, k;
    if (!luaAfflictionArgs(L, "coe5.cureAffliction", 0, &unit, &k)) return lua_pushnil(L), 1;
    lua_pushboolean(L, cureCustomAffliction(unit, k));
    return 1;
}

static int lHasAffliction(lua_State *L) {
    int unit, k;
    if (!luaAfflictionArgs(L, "coe5.hasAffliction", 1, &unit, &k)) return lua_pushnil(L), 1;
    lua_pushboolean(L, unitAfflictionEntry(unit, k) != NULL);
    return 1;
}

static int luaAfflictionField(lua_State *L, const char *call, int value) {
    int unit, k;
    if (!luaAfflictionArgs(L, call, 1, &unit, &k)) return lua_pushnil(L), 1;
    const AfflictionEntry *e = unitAfflictionEntry(unit, k);
    if (e) lua_pushinteger(L, value ? e->value : e->turn);
    else lua_pushnil(L);
    return 1;
}
static int lAfflictionTurn(lua_State *L) { return luaAfflictionField(L, "coe5.afflictionTurn", 0); }
static int lAfflictionValue(lua_State *L) { return luaAfflictionField(L, "coe5.afflictionValue", 1); }

static int luaSetAfflictionField(lua_State *L, const char *call, int value) {
    int unit, k;
    lua_Integer n = luaL_checkinteger(L, 3);
    if (!luaAfflictionArgs(L, call, 0, &unit, &k)) return lua_pushnil(L), 1;
    if (n < INT_MIN || n > INT_MAX) {
        refuseLuaCall(runningLuaMod(), call, "bad_value", "value=%lld", (long long)n);
        return lua_pushnil(L), 1;
    }
    AfflictionEntry *e = unitAfflictionEntry(unit, k);
    int replay = replayLoadingUnits();
    if (e && (!replay || keepReplayUnitAfflictions(unit))) {
        *(value ? &e->value : &e->turn) = (int)n;
        if (!replay) noteAfflictionUnit(unit);
    }
    lua_pushboolean(L, e != NULL);
    return 1;
}
static int lSetAfflictionTurn(lua_State *L) { return luaSetAfflictionField(L, "coe5.setAfflictionTurn", 0); }
static int lSetAfflictionValue(lua_State *L) { return luaSetAfflictionField(L, "coe5.setAfflictionValue", 1); }

static int lUnitAfflictions(lua_State *L) {
    lua_Integer u = luaL_checkinteger(L, 1);
    if (refuseFromDeadLuaScript(L, "coe5.unitAfflictions")) return lua_pushnil(L), 1;
    if (!afflictionUnitInUse((int)u)) {
        refuseLuaCall(runningLuaMod(), "coe5.unitAfflictions", "no_unit", "unit=%lld", (long long)u);
        return lua_pushnil(L), 1;
    }
    lua_newtable(L);
    int n = 0;
    for (int k = 0; k < customAfflictionCount; k++)
        if (customAfflictionDeclared(k) && unitAfflictionEntry((int)u, k)) {
            lua_pushstring(L, customAfflictions[k].name);
            lua_rawseti(L, -2, ++n);
        }
    return 1;
}

static int compareUnitIndexes(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

static int lAfflictedUnits(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    int k = findCustomAffliction(name);
    if (refuseFromDeadLuaScript(L, "coe5.afflictedUnits")) return lua_pushnil(L), 1;
    if (!customAfflictionDeclared(k)) {
        refuseLuaCall(runningLuaMod(), "coe5.afflictedUnits", "unknown_affliction", "name=%.63s", name);
        return lua_pushnil(L), 1;
    }
    int holders = customAfflictions[k].holders, *units = holders ? malloc((size_t)holders * sizeof *units) : NULL, n = 0;
    if (holders && !units) {
        refuseLuaCall(runningLuaMod(), "coe5.afflictedUnits", "out_of_memory", "holders=%d", holders);
        return lua_pushnil(L), 1;
    }
    for (int i = 0; i < holders; i++) {
        int u = afflictionHolders[k][i];
        if (afflictionUnitInUse(u) && (short)G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE) > 0) units[n++] = u;
    }
    if (n > 1) qsort(units, (size_t)n, sizeof *units, compareUnitIndexes);
    lua_createtable(L, n, 0);
    for (int i = 0; i < n; i++) lua_pushinteger(L, units[i]), lua_rawseti(L, -2, i + 1);
    free(units);
    return 1;
}

static int lRemoveAffliction(lua_State *L) {
    lua_Integer u = luaL_checkinteger(L, 1), bits = luaL_checkinteger(L, 2);
    const char *reason = refuseFromDeadLuaScript(L, "coe5.removeAffliction") ? "dead_script" : !afflictionUnitInUse((int)u) ? "no_unit" : NULL;
    if (reason) {
        if (strcmp(reason, "dead_script"))
            refuseLuaCall(runningLuaMod(), "coe5.removeAffliction", reason, "unit=%lld bits=0x%llx", (long long)u, (unsigned long long)bits);
        return lua_pushnil(L), 1;
    }
    AFFLICTION_WORD(u) &= ~(unsigned long long)bits;
    lua_pushboolean(L, 1);
    return 1;
}
