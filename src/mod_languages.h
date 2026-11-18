#pragma once

#define LANGUAGE_WORD        "language"
#define LANGUAGE_TAG_MAX     35
#define LANGUAGE_SUBTAG_MAX  8
#define LANGUAGE_NAME_MAX    48
#define LANGUAGES_MAX        200
#define LANGUAGE_PICK_KEY    "language"
#define LANGUAGE_RANK_SLOTS  32768
#define LANGUAGE_RANKS_MAX   20000
#define ID_LANGUAGE          1100
#define LANGUAGE_LETTER_SLOTS 16384
#define LANGUAGE_LETTERS_MAX 8192

#include "font_fallback_layout.h"
static unsigned int modFileNow(void);
static int pickLettersWithoutFont(void);
static void noteTranslatorLine(const char *word, const char *args, int mod, int level, int inBlock, int rank, int taken);
static int isLanguageRecordWord(const char *word);
static int languageRecordLine(const char *word, const char *args, int mod);
static int addBlockTranslation(const char *args);
static void forgetLanguageRecordLines(void);
#define BLOCK_RECORD_NONE (-1)
static struct { int kind, index; unsigned int file; } blockRecord = {BLOCK_RECORD_NONE, -1, 0};

typedef struct { char tag[LANGUAGE_TAG_MAX + 1], name[LANGUAGE_NAME_MAX + 1]; } DeclaredLanguage;

static struct {
    char pick[LANGUAGE_TAG_MAX + 1];
    int pickRead;
    int pickChanged;
    int open;
    unsigned int file;
    int rank;
    char tag[LANGUAGE_TAG_MAX + 1];
    DeclaredLanguage *declared;
    int declaredCount, declaredCapacity;
    unsigned int rankHash[LANGUAGE_RANK_SLOTS];
    unsigned char rankOf[LANGUAGE_RANK_SLOTS];
    int rankCount;
} languages;

static struct {
    uint32_t codes[LANGUAGE_LETTER_SLOTS];
    int count;
    unsigned int generation;
} pickLetters;

// ---- Tags --------------------------------------------------------------------------------------
static int isLanguageTag(const char *tag) {
    size_t n = strlen(tag);
    if (!n || n > LANGUAGE_TAG_MAX) return 0;
    int subtag = 0;
    size_t len = 0;
    for (size_t i = 0; i <= n; i++) {
        char c = tag[i];
        if (c == '-' || !c) {
            if (!len || len > LANGUAGE_SUBTAG_MAX) return 0;
            if (!subtag && len == 1 && !strchr("xXiI", tag[i - 1])) return 0;
            subtag++, len = 0;
            continue;
        }
        int letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'), digit = c >= '0' && c <= '9';
        if (!letter && !(digit && subtag)) return 0;
        len++;
    }
    return 1;
}

static int languageSubtags(const char *tag) {
    int n = 1;
    for (; *tag; tag++) n += *tag == '-';
    return n;
}

static int languageRank(const char *tag, const char *pick) {
    char range[LANGUAGE_TAG_MAX + 1];
    if (!tag[0] || !pick[0] || strlen(pick) > LANGUAGE_TAG_MAX) return 0;
    snprintf(range, sizeof range, "%.*s", LANGUAGE_TAG_MAX, pick);
    for (;;) {
        if (!_stricmp(range, tag)) return languageSubtags(range);
        char *dash = strrchr(range, '-');
        if (!dash) return 0;
        *dash = '\0';
        dash = strrchr(range, '-');
        if (dash && !dash[2]) *dash = '\0';
    }
}

static int languageTagMatches(const char *tag, const char *pick) {
    size_t t = strlen(tag), p = strlen(pick), n = t < p ? t : p;
    if (!t || !p || _strnicmp(tag, pick, n)) return 0;
    return t == p || (t < p ? pick[n] : tag[n]) == '-';
}

// ---- The pick ----------------------------------------------------------------------------------
static const char *languagePick(void) {
    if (languages.pickRead) return languages.pick;
    char text[64];
    languages.pickRead = 1;
    languages.pick[0] = '\0';
    if (!savedPickText(COE5SE_MOD, LANGUAGE_PICK_KEY, text, sizeof text)) return languages.pick;
    if (!isLanguageTag(text)) {
        writeLogWarning("language_pick_ignored", "reason=bad_language_tag pick=%.60s", text);
        return languages.pick;
    }
    snprintf(languages.pick, sizeof languages.pick, "%.*s", LANGUAGE_TAG_MAX, text);
    return languages.pick;
}

static void saveLanguagePick(const char *tag) {
    char path[MAX_PATH];
    noteSettingsIniNow();
    if (!settingsWritePath(COE5SE_MOD, path, sizeof path)) {
        noteOwnSettingsWrite(0);
        return;
    }
    int saved = WritePrivateProfileStringA(COE5SE_MOD, LANGUAGE_PICK_KEY, tag[0] ? tag : NULL, path) != 0;
    noteOwnSettingsWrite(saved);
    if (!saved) {
        writeLogError("language_pick_not_saved", "reason=write_failed pick=%s path=%s", tag, path);
        return;
    }
    snprintf(languages.pick, sizeof languages.pick, "%.*s", LANGUAGE_TAG_MAX, tag);
    languages.pickRead = 1;
    languages.pickChanged = 1;
    writeLog("language_picked", "pick=%s", tag[0] ? tag : "default");
}

// ---- The pass ----------------------------------------------------------------------------------
static void languagesBeforePass(int level) {
    if (level != 2) return;
    languages.pickRead = 0;
    languagePick();
    languages.open = 0;
    languages.declaredCount = 0;
    memset(languages.rankHash, 0, sizeof languages.rankHash);
    languages.rankCount = 0;
    forgetLanguageRecordLines();
    memset(pickLetters.codes, 0, sizeof pickLetters.codes);
    pickLetters.count = 0;
    pickLetters.generation++;
}

static int readLanguageQuoted(const char **p, char *out, size_t size);

static void notePickLetterText(const char *text, size_t n) {
    const unsigned char *s = (const unsigned char *)text;
    for (size_t at = 0; at < n;) {
        uint32_t code;
        at += (size_t)fallbackReadLetter(s + at, n - at, &code);
        if (code < 0x80 || pickLetters.count >= LANGUAGE_LETTERS_MAX) continue;
        unsigned int slot = (code * 2654435761u) & (LANGUAGE_LETTER_SLOTS - 1);
        while (pickLetters.codes[slot] && pickLetters.codes[slot] != code) slot = (slot + 1) & (LANGUAGE_LETTER_SLOTS - 1);
        if (!pickLetters.codes[slot]) pickLetters.codes[slot] = code, pickLetters.count++;
    }
}

static void notePickLetters(const char *args) {
    const char *p = args;
    char first[2];
    if (readLanguageQuoted(&p, first, sizeof first) < 0) return;
    skipSpaces(&p);
    if (*p != '"') return;
    notePickLetterText(p + 1, strcspn(p + 1, "\""));
}

static void declareLanguage(int mod, const char *tag, const char *name) {
    for (int i = 0; i < languages.declaredCount; i++) {
        DeclaredLanguage *d = &languages.declared[i];
        if (_stricmp(d->tag, tag)) continue;
        if (!d->name[0] && name[0]) snprintf(d->name, sizeof d->name, "%.*s", LANGUAGE_NAME_MAX, name);
        return;
    }
    if (languages.declaredCount >= LANGUAGES_MAX) {
        refuseC5mLine(LANGUAGE_WORD, modName(mod), "too_many_languages", "line=%d tag=%s max=%d", G32(RVA_MOD_LINE) + 1, tag, LANGUAGES_MAX);
        return;
    }
    if (!growList((void **)&languages.declared, &languages.declaredCapacity, languages.declaredCount + 1, sizeof *languages.declared)) {
        refuseC5mLine(LANGUAGE_WORD, modName(mod), "out_of_memory", "line=%d tag=%s", G32(RVA_MOD_LINE) + 1, tag);
        return;
    }
    DeclaredLanguage *d = &languages.declared[languages.declaredCount++];
    snprintf(d->tag, sizeof d->tag, "%.*s", LANGUAGE_TAG_MAX, tag);
    snprintf(d->name, sizeof d->name, "%.*s", LANGUAGE_NAME_MAX, name);
}

static size_t languageLineLength(const char *line) {
    size_t end = strcspn(line, "\r\n");
    int quoted = 0;
    for (size_t i = 0; i < end; i++) {
        if (line[i] == '"') quoted = !quoted;
        else if (line[i] == '#' && !quoted) return i;
    }
    return quoted ? strcspn(line, "#\r\n") : end;
}

static int languageLineKeepsHash(const char *line) { return languageLineLength(line) > strcspn(line, "#\r\n"); }

static void caretsToLineBreaks(char *text) {
    for (; *text; text++)
        if (*text == '^') *text = '\n';
}

static int readLanguageQuoted(const char **p, char *out, size_t size) {
    skipSpaces(p);
    if (**p != '"') return -1;
    const char *end = strchr(++*p, '"');
    if (!end) return -1;
    size_t n = (size_t)(end - *p);
    snprintf(out, size, "%.*s", (int)(n < size ? n : size - 1), *p);
    *p = end + 1;
    return (int)n;
}

static void openLanguageBlock(const char *args, int mod, int level) {
    char line[LANGUAGE_TAG_MAX + LANGUAGE_NAME_MAX + 64], tag[LANGUAGE_TAG_MAX + 2], name[LANGUAGE_NAME_MAX + 2];
    snprintf(line, sizeof line, "%.*s", (int)languageLineLength(args), args);
    const char *p = line;
    int lineNo = G32(RVA_MOD_LINE) + 1, report = level == 2 && mod >= 0 && mod <= G32(RVA_MOD_LAST_INDEX);
    const char *problem = NULL;
    int tagLength = readLanguageQuoted(&p, tag, sizeof tag);
    if (tagLength < 0) problem = "no_language_tag";
    else if (tagLength > LANGUAGE_TAG_MAX || !isLanguageTag(tag)) problem = "bad_language_tag";
    languages.open = 1;
    languages.file = modFileNow();
    blockRecord.kind = BLOCK_RECORD_NONE;
    snprintf(languages.tag, sizeof languages.tag, "%.*s", LANGUAGE_TAG_MAX, problem ? "-" : tag);
    languages.rank = problem ? 0 : languageRank(tag, languagePick());
    if (problem) {
        if (report) refuseC5mLine(LANGUAGE_WORD, modName(mod), problem, "line=%d text=%.60s", lineNo, line);
        return;
    }
    name[0] = '\0';
    skipSpaces(&p);
    if (*p == '"') {
        int nameLength = readLanguageQuoted(&p, name, sizeof name);
        if (nameLength <= 0 || nameLength > LANGUAGE_NAME_MAX) {
            if (report) refuseC5mLine(LANGUAGE_WORD, modName(mod), "bad_language_name", "line=%d tag=%s max=%d", lineNo, tag, LANGUAGE_NAME_MAX);
            name[0] = '\0';
            if (nameLength < 0) p += strlen(p);
        }
    }
    if (report && !isLineEnd(p)) refuseC5mLine(LANGUAGE_WORD, modName(mod), "text_after_language", "line=%d tag=%s text=%.40s", lineNo, tag, p);
    if (report) declareLanguage(mod, tag, name);
}

static int takesLanguageKey(const char *key);
static int takesLanguageLine(const char *word, const char *args) {
    char key[TRANSLATED_SIZE];
    const char *p = args;
    if (!strcmp(word, "fontfile")) {
        int slot;
        if (!readInt(&p, &slot)) return 1;
        snprintf(key, sizeof key, "\x01" "fontfile %d", slot);
    } else if (readLanguageQuoted(&p, key, sizeof key) < 0) {
        return 1;
    } else {
        caretsToLineBreaks(key);
    }
    return takesLanguageKey(key);
}

static int takesLanguageKey(const char *key) {
    unsigned int h = 2166136261u;
    size_t n = strlen(key);
    while (n && (key[n - 1] == '\n' || key[n - 1] == '\r')) n--;
    for (size_t i = 0; i < n; i++) h = (h ^ (unsigned char)key[i]) * 16777619u;
    if (!h) h = 1;
    unsigned int slot = h & (LANGUAGE_RANK_SLOTS - 1);
    while (languages.rankHash[slot] && languages.rankHash[slot] != h) slot = (slot + 1) & (LANGUAGE_RANK_SLOTS - 1);
    if (languages.rankHash[slot]) {
        if (languages.rankOf[slot] > languages.rank) return 0;
        languages.rankOf[slot] = (unsigned char)languages.rank;
        return 1;
    }
    if (languages.rankCount >= LANGUAGE_RANKS_MAX) return 1;
    languages.rankHash[slot] = h;
    languages.rankOf[slot] = (unsigned char)languages.rank;
    languages.rankCount++;
    return 1;
}

static int languageLineHook(const char *text, int own, int mod, int level) {
    const char *at = text;
    while (*at == ' ' || *at == '\t' || *at == '\r' || *at == '\n') at++;
    char word[32];
    size_t n = strcspn(at, " \t\r\n\"#");
    snprintf(word, sizeof word, "%.*s", (int)(n < sizeof word ? n : sizeof word - 1), at);
    if (!word[0]) return 0;
    if (!strcmp(word, LANGUAGE_WORD)) {
        if (!own) {
            languages.open = 0;
            return 0;
        }
        openLanguageBlock(at + n, mod, level);
        return 1;
    }
    if (!languages.open || languages.file != modFileNow()) {
        noteTranslatorLine(word, at + n, mod, level, 0, 0, 1);
        return 0;
    }
    if (level != 2) return 1;
    if (!strcmp(word, "translation") || !strcmp(word, "fontfile")) {
        int taken = languages.rank && takesLanguageLine(word, at + n);
        noteTranslatorLine(word, at + n, mod, level, 1, languages.rank, taken);
        if (taken && word[0] == 't') notePickLetters(at + n);
        if (taken && word[0] == 't' && addBlockTranslation(at + n)) return 1;
        return !taken;
    }
    if (!strcmp(word, "fallbackfont")) return !languages.rank;
    if (isLanguageRecordWord(word)) {
        if (languages.rank && mod >= 0 && mod <= G32(RVA_MOD_LAST_INDEX)) languageRecordLine(word, at + n, mod);
        return 1;
    }
    if (mod >= 0 && mod <= G32(RVA_MOD_LAST_INDEX))
        refuseC5mLine(word, modName(mod), "not_display_line", "line=%d language=%s", G32(RVA_MOD_LINE) + 1, languages.tag);
    return 1;
}

static void languageWordLine(const char *args, int mod, int level) {
    if (level != 2 || mod < 0 || mod > G32(RVA_MOD_LAST_INDEX)) return;
    refuseC5mLine(LANGUAGE_WORD, modName(mod), "language_not_here", "line=%d text=%.60s", G32(RVA_MOD_LINE), args);
}

// ---- Content-free lines (ADR 0017) ------------------------------------------------------------
static int scanLanguageLine(const char *word, int mayDeclare, int *open) {
    if (!strcmp(word, LANGUAGE_WORD)) {
        *open = mayDeclare;
        return 1;
    }
    return *open;
}

// ---- The page's line ---------------------------------------------------------------------------
static int pickedLanguageIndex(void) {
    int best = -1, bestRank = 0;
    for (int i = 0; i < languages.declaredCount; i++) {
        int rank = languageRank(languages.declared[i].tag, languagePick());
        if (rank > bestRank) best = i, bestRank = rank;
    }
    return best;
}

static void drawLanguagePick(void) {
    if (!languages.declaredCount) return;
    int picked = pickedLanguageIndex();
    sectionBegin();
    sectionHeader("Language");
    radioOption(ID_LANGUAGE, picked < 0, "Default");
    tooltip("Mods' text as written");
    for (int i = 0; i < languages.declaredCount; i++) {
        const DeclaredLanguage *d = &languages.declared[i];
        radioOption(ID_LANGUAGE + 1 + i, picked == i, d->name[0] ? d->name : d->tag);
    }
    int missing = picked >= 0 && pickLetters.count ? pickLettersWithoutFont() : 0;
    if (missing > 0) {
        char label[64];
        snprintf(label, sizeof label, "No font for %d character%s", missing, missing == 1 ? "" : "s");
        textButton(0, label);
        tooltip("Drawn as boxes. A font pack mod for this language adds the font.");
    }
    sectionEnd();
}

static void clickLanguagePick(int id) {
    if (id < ID_LANGUAGE || id > ID_LANGUAGE + languages.declaredCount) return;
    const char *tag = id == ID_LANGUAGE ? "" : languages.declared[id - ID_LANGUAGE - 1].tag;
    if (!strcmp(tag, languagePick())) return;
    saveLanguagePick(tag);
}

static void rereadEnabledModsAsGame(void);
static void (*rereadForLanguage)(void) = rereadEnabledModsAsGame;

static void applyLanguagePick(void) {
    if (!languages.pickChanged) return;
    languages.pickChanged = 0;
    writeLog("language_pass", "pick=%s", languagePick()[0] ? languagePick() : "default");
    rereadForLanguage();
}

static int languageInit(void) {
    return apiAddModCommand("language", languageWordLine);
}
