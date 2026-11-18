#pragma once

#define LANGUAGE_TEXT_MAX    10000

#define BLOCK_RECORD_AFFLICTION (RECORD_RITUAL + 1)
typedef struct { const char *word; int kind; } BlockOpener;
static const BlockOpener BLOCK_OPENERS[] = {
    {"selectmonster", RECORD_MONSTER}, {"selectitem", RECORD_ITEM}, {"selectclass", RECORD_CLASS},
    {"selectterr", RECORD_TERRAIN}, {"selectritual", RECORD_RITUAL}, {"selectaffliction", BLOCK_RECORD_AFFLICTION},
};

static const char *blockOpenerWord(int kind) {
    for (size_t i = 0; i < sizeof BLOCK_OPENERS / sizeof BLOCK_OPENERS[0]; i++)
        if (BLOCK_OPENERS[i].kind == kind) return BLOCK_OPENERS[i].word;
    return "-";
}

typedef struct { const char *word; int kinds; int field, caret; const char *tableKey; } BlockTextWord;
static const BlockTextWord BLOCK_TEXT_WORDS[] = {
    {"descr", 1 << RECORD_MONSTER | 1 << RECORD_ITEM | 1 << RECORD_RITUAL | 1 << BLOCK_RECORD_AFFLICTION, 0, 1, NULL},
    {"classdescr", 1 << RECORD_CLASS, CLASS_DESCR, 1, "class"},
    {"classabdescr", 1 << RECORD_CLASS, CLASS_ABILITY_DESCR, 1, "abilities"},
    {"townbonusdescr", 1 << RECORD_CLASS, CLASS_TOWN_BONUS_DESCR, 0, NULL},
    {"libbonusdescr", 1 << RECORD_CLASS, CLASS_LIB_BONUS_DESCR, 0, NULL},
    {"templebonusdescr", 1 << RECORD_CLASS, CLASS_TEMPLE_BONUS_DESCR, 0, NULL},
    {"description", 0, 0, 0, NULL},
};

static int monsterByName(const char *text);
static int ritualByName(const char *name, int nth);
static int classByName(const char *name);
static int itemSlotsItemNamed(const char *name);
static unsigned char *monsterRecord(int monster);
static unsigned char *tableAddress(int table);
static int tableRecords(int table);
static unsigned int ritualTableRva(void);
static int afflictionWithText(const char *name);
static const char *afflictionText(int kind);
static void noteTranslatorRecordPair(int mod, int in, int line, const char *from, const char *to, int rank, int taken);

static int gameRitualDescription(int ritual) { return GAME_FN(int (*)(int), RVA_RITUAL_DESCRIPTION)(ritual); }
static void gameAddTranslation(const char *from, const char *to) { GAME_FN(void (*)(const char *, const char *), RVA_ADD_TRANSLATION)(from, to); }
static int (*ritualDescriptionLine)(int ritual) = gameRitualDescription;
static void (*addLanguagePair)(const char *from, const char *to) = gameAddTranslation;

// ---- The game's descriptions --------------------------------------------------------------------
static const char *descriptionLine(int i) {
    if (i < 0 || i >= DESCRIPTION_LINES_MAX) return NULL;
    return ((const char *const *)GAME_PTR(RVA_ABILITY_DESCRIPTIONS))[i];
}

static int descriptionIndex(const char *key) {
    for (int i = 0; i < DESCRIPTION_LINES_MAX; i++) {
        const char *line = descriptionLine(i);
        if (!line || !strcmp(line, ":end")) return 0;
        if (line[0] != ':' || _stricmp(line + 1, key)) continue;
        for (int t = i + 1; t < DESCRIPTION_LINES_MAX; t++) {
            const char *text = descriptionLine(t);
            if (!text || !strcmp(text, ":end")) return 0;
            if (text[0] != ':') return t;
        }
        return 0;
    }
    return 0;
}

static PRINTF_FORMAT(1, 2) int descriptionIndexOf(const char *fmt, ...) {
    char key[DESCRIPTION_TEXT_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(key, sizeof key, fmt, ap);
    va_end(ap);
    return descriptionIndex(key);
}

static int monsterDescriptionIndex(int m) {
    int i = descriptionIndexOf("mon%d", m);
    const unsigned char *record = monsterRecord(m);
    if (i > 0 || !record) return i;
    char name[MONSTER_NAME_SIZE + 1];
    snprintf(name, sizeof name, "%.*s", (int)strnlen((const char *)record, MONSTER_NAME_SIZE), (const char *)record);
    if ((i = descriptionIndexOf("mon %s", name)) > 0) return i;
    int before = 0;
    for (int k = m - 1; k >= 0 && monsterRecord(k) && !strncmp((const char *)monsterRecord(k), name, MONSTER_NAME_SIZE); k--) before++;
    if ((i = descriptionIndexOf("+%d %s", before, name)) > 0) return i;
    return descriptionIndexOf("%s", name);
}

static const char *blockRecordText(const BlockTextWord *w, int mod) {
    int line = 0;
    if (!w->kinds) {
        const char *d = *(const char *const *)(GAME_PTR(RVA_MOD_NAMES) + (size_t)mod * MOD_RECORD_SIZE + MOD_RECORD_DESCRIPTION);
        return d && d[0] ? d : NULL;
    }
    if (blockRecord.kind == RECORD_MONSTER) line = monsterDescriptionIndex(blockRecord.index);
    if (blockRecord.kind == RECORD_ITEM)
        line = descriptionIndexOf("%.*s", ITEM_NAME_SIZE, (const char *)(tableAddress(TABLE_ITEM) + (size_t)blockRecord.index * ITEM_SIZE));
    if (blockRecord.kind == RECORD_RITUAL) line = ritualDescriptionLine(blockRecord.index);
    if (blockRecord.kind == BLOCK_RECORD_AFFLICTION) return afflictionText(blockRecord.index);
    if (blockRecord.kind == RECORD_CLASS) {
        const char *record = (const char *)(tableAddress(TABLE_CLASS) + (size_t)blockRecord.index * CLASS_SIZE);
        if (record[w->field]) return record + w->field;
        if (!w->tableKey) return NULL;
        line = descriptionIndexOf("%s %.*s", w->tableKey, CLASS_NAME_SIZE, record);
    }
    const char *text = line > 0 ? descriptionLine(line) : NULL;
    return text && text[0] ? text : NULL;
}

// ---- The lines ----------------------------------------------------------------------------------
typedef struct {
    int word;
    int kind;
    int number;
    int offset;
    char *text;
    int mod, line, rank, in;
} KeptRecordLine;
static struct { KeptRecordLine *lines; int count, capacity; } keptRecordLines;

static void forgetLanguageRecordLines(void) {
    for (int i = 0; i < keptRecordLines.count; i++) free(keptRecordLines.lines[i].text);
    keptRecordLines.count = 0;
}

static int recordLineFileNow(void) {
    if (!readingSidecarNow()) return SETTINGS_IN_C5M;
    int label = includedLabelNow();
    return label >= 0 ? SETTINGS_IN_INCLUDED + label : SETTINGS_IN_C5SE;
}

static int keepRecordLine(KeptRecordLine line, const char *text, const char *word) {
    char *copy = strdup(text);
    if (!copy || !growList((void **)&keptRecordLines.lines, &keptRecordLines.capacity, keptRecordLines.count + 1, sizeof *keptRecordLines.lines)) {
        free(copy);
        refuseC5mLine(word, modName(line.mod), "out_of_memory", "line=%d", line.line);
        return 0;
    }
    line.text = copy;
    keptRecordLines.lines[keptRecordLines.count++] = line;
    return 1;
}

static void openBlockRecord(const char *word, int kind, const char *line, int mod) {
    char name[CLASS_NAME_SIZE + 1], args[512];
    snprintf(args, sizeof args, "%.*s", (int)languageLineLength(line), line);
    const char *p = args;
    int number = -1, offset = 0, lineNo = G32(RVA_MOD_LINE) + 1;
    blockRecord.kind = BLOCK_RECORD_NONE;
    blockRecord.file = modFileNow();
    skipSpaces(&p);
    name[0] = '\0';
    if (kind == RECORD_TERRAIN || (kind == RECORD_CLASS && *p != '"')) {
        int last = kind == RECORD_TERRAIN ? TERRAIN_SELECT_MAX : G32(RVA_CLASS_COUNT);
        if (!readInt(&p, &number) || number < (kind == RECORD_CLASS) || number > last) {
            refuseC5mLine(word, modName(mod), "bad_record_number", "line=%d text=%.60s", lineNo, args + strspn(args, " \t"));
            return;
        }
    } else {
        int n = readLanguageQuoted(&p, name, sizeof name);
        if (n <= 0 || n >= (int)sizeof name) {
            refuseC5mLine(word, modName(mod), "no_record_name", "line=%d text=%.60s", lineNo, args + strspn(args, " \t"));
            return;
        }
        skipSpaces(&p);
        if (!isLineEnd(p) && (kind == RECORD_MONSTER || kind == RECORD_RITUAL) && (!readInt(&p, &offset) || offset < 0)) offset = 0;
    }
    KeptRecordLine kept = {-1, kind, number, offset, NULL, mod, lineNo, languages.rank, recordLineFileNow()};
    if (!keepRecordLine(kept, name, word)) return;
    blockRecord.kind = kind;
    blockRecord.index = keptRecordLines.count - 1;
}

static void keepBlockRecordText(int wordIndex, const char *args, int mod) {
    const BlockTextWord *w = &BLOCK_TEXT_WORDS[wordIndex];
    static char text[LANGUAGE_TEXT_MAX], line[LANGUAGE_TEXT_MAX + 64];
    snprintf(line, sizeof line, "%.*s", (int)languageLineLength(args), args);
    const char *p = line;
    int lineNo = G32(RVA_MOD_LINE) + 1, own = !w->kinds;
    if (!own && (blockRecord.kind == BLOCK_RECORD_NONE || blockRecord.file != modFileNow())) {
        refuseC5mLine(w->word, modName(mod), "no_record", "line=%d", lineNo);
        return;
    }
    if (!own && !(w->kinds & 1 << blockRecord.kind)) {
        refuseC5mLine(w->word, modName(mod), "not_for_this_record", "line=%d", lineNo);
        return;
    }
    int n = readLanguageQuoted(&p, text, sizeof text);
    if (n < 0 || n >= (int)sizeof text) {
        refuseC5mLine(w->word, modName(mod), n < 0 ? "no_text" : "text_too_long", "line=%d max=%d", lineNo, LANGUAGE_TEXT_MAX - 1);
        return;
    }
    KeptRecordLine kept = {wordIndex, own ? 0 : blockRecord.kind, -1, 0, NULL, mod, lineNo, languages.rank, recordLineFileNow()};
    keepRecordLine(kept, text, w->word);
}

static int findKeptRecord(const KeptRecordLine *l) {
    if (l->number >= 0) return l->number;
    int index = -1;
    if (l->kind == RECORD_MONSTER) {
        index = monsterByName(l->text);
        if (index >= 0) index = index + l->offset <= G32(RVA_MONSTER_COUNT) ? index + l->offset : G32(RVA_MONSTER_COUNT);
    }
    if (l->kind == RECORD_ITEM) index = itemSlotsItemNamed(l->text);
    if (l->kind == RECORD_CLASS) index = classByName(l->text);
    if (l->kind == RECORD_RITUAL) index = ritualByName(l->text, l->offset);
    if (l->kind == BLOCK_RECORD_AFFLICTION) index = afflictionWithText(l->text);
    if (index < 0)
        refuseSidecarLine(l->in, blockOpenerWord(l->kind), modName(l->mod), "no_such_record", "line=%d name=%.60s", l->line, l->text);
    return index;
}

static void languageRecordsAfterPass(int level) {
    if (level != 2) return;
    int savedRank = languages.rank, opened = 0;
    for (int i = 0; i < keptRecordLines.count; i++) {
        KeptRecordLine *l = &keptRecordLines.lines[i];
        if (l->word < 0) {
            blockRecord.kind = l->kind;
            blockRecord.index = findKeptRecord(l);
            opened = blockRecord.index >= 0;
            continue;
        }
        const BlockTextWord *w = &BLOCK_TEXT_WORDS[l->word];
        if (w->kinds && !opened) continue;
        if (w->kinds) blockRecord.kind = l->kind;
        const char *from = blockRecordText(w, l->mod);
        if (!from) {
            refuseSidecarLine(l->in, w->word, modName(l->mod), "no_description", "line=%d", l->line);
            continue;
        }
        if (w->caret && l->kind != BLOCK_RECORD_AFFLICTION) caretsToLineBreaks(l->text);
        languages.rank = l->rank;
        int taken = takesLanguageKey(from);
        noteTranslatorRecordPair(l->mod, l->in, l->line, from, l->text, l->rank, taken);
        if (!taken) continue;
        addLanguagePair(from, l->text);
        notePickLetterText(l->text, strlen(l->text));
    }
    languages.rank = savedRank;
    blockRecord.kind = BLOCK_RECORD_NONE;
    forgetLanguageRecordLines();
}

static int languageRecordLine(const char *word, const char *args, int mod) {
    for (size_t i = 0; i < sizeof BLOCK_OPENERS / sizeof BLOCK_OPENERS[0]; i++)
        if (!strcmp(word, BLOCK_OPENERS[i].word)) {
            openBlockRecord(word, BLOCK_OPENERS[i].kind, args, mod);
            return 1;
        }
    for (size_t i = 0; i < sizeof BLOCK_TEXT_WORDS / sizeof BLOCK_TEXT_WORDS[0]; i++)
        if (!strcmp(word, BLOCK_TEXT_WORDS[i].word)) {
            keepBlockRecordText((int)i, args, mod);
            return 1;
        }
    return 0;
}

static int isLanguageRecordWord(const char *word) {
    for (size_t i = 0; i < sizeof BLOCK_OPENERS / sizeof BLOCK_OPENERS[0]; i++)
        if (!strcmp(word, BLOCK_OPENERS[i].word)) return 1;
    for (size_t i = 0; i < sizeof BLOCK_TEXT_WORDS / sizeof BLOCK_TEXT_WORDS[0]; i++)
        if (!strcmp(word, BLOCK_TEXT_WORDS[i].word)) return 1;
    return 0;
}

// ---- A translation line CoE5SE adds itself ------------------------------------------------------
static int addBlockTranslation(const char *args) {
    static char line[LANGUAGE_TEXT_MAX * 2 + 64], from[LANGUAGE_TEXT_MAX], to[LANGUAGE_TEXT_MAX];
    size_t length = languageLineLength(args);
    if (!languageLineKeepsHash(args) && !memchr(args, '^', length)) return 0;
    snprintf(line, sizeof line, "%.*s", (int)length, args);
    const char *p = line;
    int n = readLanguageQuoted(&p, from, sizeof from), m = n < 0 ? -1 : readLanguageQuoted(&p, to, sizeof to);
    if (n < 0 || m < 0 || n >= (int)sizeof from || m >= (int)sizeof to) return 0;
    caretsToLineBreaks(from);
    caretsToLineBreaks(to);
    addLanguagePair(from, to);
    return 1;
}

static int languageWordTakesCaret(const char *word) {
    for (size_t i = 0; i < sizeof BLOCK_TEXT_WORDS / sizeof BLOCK_TEXT_WORDS[0]; i++)
        if (!strcmp(word, BLOCK_TEXT_WORDS[i].word)) return BLOCK_TEXT_WORDS[i].caret;
    return 0;
}

// ---- Installed mods' descriptions ---------------------------------------------------------------
typedef struct { uint64_t hash; char *text; } ModDescriptionPair;
static struct {
    ModDescriptionPair *pairs;
    int count, capacity;
    uint64_t readFor;
} modDescriptions;

typedef struct { char *text; size_t size; int rank; } ModBlockText;

static void readDescriptionFile(int mod, const char *path, int kind, ModBlockText *description, ModBlockText *name) {
    FILE *f = fopen(path, "rb");
    if (!f) return;
    static char line[MOD_LINE_MAX + 2];
    char word[32], tag[LANGUAGE_TAG_MAX + 2];
    int rank = -1;
    while (fgets(line, sizeof line, f)) {
        const char *own = kind == SCAN_C5M ? sidecarLineText(line) : line, *text = own ? own : line;
        const char *at = text + strspn(text, " \t\r\n");
        size_t n = strcspn(at, " \t\r\n\"#");
        snprintf(word, sizeof word, "%.*s", (int)(n < sizeof word ? n : sizeof word - 1), at);
        if (!strcmp(word, LANGUAGE_WORD)) {
            const char *p = at + n;
            int tagLength = own ? readLanguageQuoted(&p, tag, sizeof tag) : -1;
            if (!own) rank = -1;
            else rank = tagLength >= 0 && tagLength <= LANGUAGE_TAG_MAX && isLanguageTag(tag) ? languageRank(tag, languagePick()) : 0;
            continue;
        }
        char included[MAX_PATH], file[INCLUDE_FILE_MAX + 2];
        if (rank < 0 && own && kind != SCAN_INCLUDED && readIncludeLine(text, file, sizeof file) > 0) {
            if (!findIncludedFile(mod, file, included, sizeof included)) readDescriptionFile(mod, included, SCAN_INCLUDED, description, name);
            continue;
        }
        int isDescription = !strcmp(word, "description");
        ModBlockText *best = isDescription ? description : !strcmp(word, "translation") ? name : NULL;
        if (rank <= 0 || !best || rank < best->rank) continue;
        static char cut[MOD_LINE_MAX + 2], from[LANGUAGE_TEXT_MAX], quoted[LANGUAGE_TEXT_MAX];
        snprintf(cut, sizeof cut, "%.*s", (int)languageLineLength(at + n), at + n);
        const char *p = cut;
        if (!isDescription && (readLanguageQuoted(&p, from, sizeof from) < 0 || strcmp(from, modName(mod)))) continue;
        int got = readLanguageQuoted(&p, quoted, sizeof quoted);
        if (got < 0 || got >= (int)sizeof quoted || got >= (int)best->size) continue;
        snprintf(best->text, best->size, "%s", quoted);
        best->rank = rank;
    }
    fclose(f);
}

static void readModDescription(int mod, ModBlockText *description, ModBlockText *name) {
    char path[MAX_PATH];
    description->text[0] = name->text[0] = '\0';
    description->rank = name->rank = 1;
    if (buildPath(path, sizeof path, __func__, "%s" PATH_SEP "%s" MOD_FILE_SUFFIX, modFolder(mod), modName(mod)))
        readDescriptionFile(mod, path, SCAN_C5M, description, name);
    if (buildPath(path, sizeof path, __func__, "%s" PATH_SEP "%s" SIDECAR_SUFFIX, modFolder(mod), modName(mod)))
        readDescriptionFile(mod, path, SCAN_SIDECAR, description, name);
}

static void forgetModDescriptions(void) {
    for (int i = 0; i < modDescriptions.count; i++) free(modDescriptions.pairs[i].text);
    modDescriptions.count = 0;
    modDescriptions.readFor = 0;
}

static int keepModDescriptionPair(int mod, const char *from, const char *to) {
    char *copy = strdup(to);
    if (!copy || !growList((void **)&modDescriptions.pairs, &modDescriptions.capacity, modDescriptions.count + 1, sizeof *modDescriptions.pairs)) {
        free(copy);
        writeLogError("mod_descriptions_not_read", "reason=out_of_memory mod=%s", modName(mod));
        return 0;
    }
    modDescriptions.pairs[modDescriptions.count++] = (ModDescriptionPair){drawnTextHash(from, (size_t)-1), copy};
    return 1;
}

static void modDescriptionsAfterPass(int level) {
    if (level != 2) return;
    languageRecordsAfterPass(level);
    const char *pick = languagePick();
    uint64_t key = drawnTextHash(pick, (size_t)-1);
    for (int m = 0; pick[0] && m <= G32(RVA_MOD_LAST_INDEX) && m < MOD_TABLE_MAX; m++)
        if (!modEnabled(m)) key = (key ^ drawnTextHash(modName(m), (size_t)-1) ^ (uint64_t)m) * 1099511628211ull;
    if (!pick[0]) {
        forgetModDescriptions();
        return;
    }
    if (key == modDescriptions.readFor) return;
    forgetModDescriptions();
    static char descriptionText[LANGUAGE_TEXT_MAX], nameText[LANGUAGE_TEXT_MAX];
    ModBlockText description = {descriptionText, sizeof descriptionText, 1}, name = {nameText, sizeof nameText, 1};
    int read = 0, names = 0, kept = 1;
    for (int m = 0; kept && m <= G32(RVA_MOD_LAST_INDEX) && m < MOD_TABLE_MAX; m++) {
        if (modEnabled(m) || !modName(m)[0]) continue;
        const char *from = *(const char *const *)(GAME_PTR(RVA_MOD_NAMES) + (size_t)m * MOD_RECORD_SIZE + MOD_RECORD_DESCRIPTION);
        read++;
        readModDescription(m, &description, &name);
        if (from && from[0] && description.text[0]) kept = keepModDescriptionPair(m, from, description.text);
        if (kept && name.text[0]) kept = keepModDescriptionPair(m, modName(m), name.text), names += kept;
    }
    modDescriptions.readFor = key;
    writeLog("mod_descriptions_read", "pick=%s mods=%d translated=%d names=%d", pick, read, modDescriptions.count - names, names);
}

static const char *modDescriptionTranslation(const char *text) {
    if (!modDescriptions.count) return NULL;
    uint64_t h = drawnTextHash(text, (size_t)-1);
    for (int i = 0; i < modDescriptions.count; i++)
        if (modDescriptions.pairs[i].hash == h) return modDescriptions.pairs[i].text;
    return NULL;
}
