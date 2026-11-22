#pragma once

#define TRANSLATOR_FOLDER       "coe5se_translations"
#define TRANSLATOR_REPORT_FILE  "report.txt"
#define TRANSLATOR_TEXT_MAX     (TRANSLATED_SIZE - 1)
#define TRANSLATOR_NAME_LIMIT   (TRANSLATED_ARG_SIZE - 1)
#define TRANSLATOR_SCRIPT_MAX   (8 * 1024 * 1024)
#define TRANSLATOR_QUOTE_SHOWN  100
#define TRANSLATOR_FILE_MAX     72

typedef enum { TRANSLATOR_HASH_IN_TEXT = 1, TRANSLATOR_QUOTE_IN_TEXT, TRANSLATOR_BREAK_IN_TEXT, TRANSLATOR_CARET_IN_TEXT } TranslatorProblemKind;
static int languageWordTakesCaret(const char *word);

typedef struct {
    char *text;
    int mod, line, limit;
    char file[TRANSLATOR_FILE_MAX + 1];
} TranslatorText;

typedef struct {
    char *from, *to;
    int mod, line, rank, taken;
    char file[TRANSLATOR_FILE_MAX + 1];
} TranslatorLine;

typedef struct {
    char *text;
    int mod, line, kind;
    char file[TRANSLATOR_FILE_MAX + 1];
} TranslatorProblem;

typedef struct { char name[MOD_NAME_MAX], folder[MAX_PATH]; } TranslatorMod;

typedef struct { int *slots, capacity, count; } TranslatorIndex;

static struct {
    int collecting;
    TranslatorMod *mods;
    int modCount, modCapacity;
    TranslatorText *texts;
    int textCount, textCapacity;
    TranslatorLine *lines;
    int lineCount, lineCapacity;
    TranslatorProblem *problems;
    int problemCount, problemCapacity;
    TranslatorIndex textIndex;
    TranslatorIndex finalIndex;
    int terrainOpen;
    unsigned int terrainFile;
} translators;

// ---- The index ---------------------------------------------------------------------------------
static unsigned int translatorHash(const char *text, int tag) {
    unsigned int h = 2166136261u ^ (unsigned int)tag;
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) h = (h ^ *c) * 16777619u;
    return h;
}

static const char *translatorKeyText(const TranslatorIndex *index, int item) {
    return index == &translators.textIndex ? translators.texts[item].text : translators.lines[item].from;
}
static int translatorKeyTag(const TranslatorIndex *index, int item) {
    return index == &translators.textIndex ? translators.texts[item].mod : -1;
}

static int translatorSlot(const TranslatorIndex *index, const char *text, int tag) {
    unsigned int mask = (unsigned int)index->capacity - 1, slot = translatorHash(text, tag) & mask;
    for (;; slot = (slot + 1) & mask) {
        int item = index->slots[slot] - 1;
        if (item < 0 || (translatorKeyTag(index, item) == tag && !strcmp(translatorKeyText(index, item), text))) return (int)slot;
    }
}

static int translatorFind(const TranslatorIndex *index, const char *text, int tag) {
    if (!index->capacity) return -1;
    return index->slots[translatorSlot(index, text, tag)] - 1;
}

static int translatorIndexPut(TranslatorIndex *index, int item) {
    if ((index->count + 1) * 2 > index->capacity) {
        int capacity = index->capacity ? index->capacity * 2 : 256;
        int *slots = calloc((size_t)capacity, sizeof *slots), *old = index->slots, oldCapacity = index->capacity;
        if (!slots) return 0;
        index->slots = slots, index->capacity = capacity;
        for (int s = 0; s < oldCapacity; s++)
            if (old[s]) slots[translatorSlot(index, translatorKeyText(index, old[s] - 1), translatorKeyTag(index, old[s] - 1))] = old[s];
        free(old);
    }
    int slot = translatorSlot(index, translatorKeyText(index, item), translatorKeyTag(index, item));
    if (!index->slots[slot]) index->count++;
    index->slots[slot] = item + 1;
    return 1;
}

// ---- What a pass noted -------------------------------------------------------------------------
static void clearTranslators(void) {
    for (int i = 0; i < translators.textCount; i++) free(translators.texts[i].text);
    for (int i = 0; i < translators.lineCount; i++) free(translators.lines[i].from), free(translators.lines[i].to);
    for (int i = 0; i < translators.problemCount; i++) free(translators.problems[i].text);
    free(translators.textIndex.slots), free(translators.finalIndex.slots);
    memset(&translators.textIndex, 0, sizeof translators.textIndex);
    memset(&translators.finalIndex, 0, sizeof translators.finalIndex);
    translators.modCount = translators.textCount = translators.lineCount = translators.problemCount = 0;
}

static void addTranslatorText(int mod, const char *file, int line, const char *text, int limit);

static int translatorMod(const char *name, const char *folder) {
    for (int i = 0; i < translators.modCount; i++)
        if (!strcmp(translators.mods[i].name, name)) return i;
    if (!growList((void **)&translators.mods, &translators.modCapacity, translators.modCount + 1, sizeof *translators.mods)) return -1;
    TranslatorMod *m = &translators.mods[translators.modCount];
    snprintf(m->name, sizeof m->name, "%s", name);
    snprintf(m->folder, sizeof m->folder, "%s", folder);
    char file[TRANSLATOR_FILE_MAX + 1];
    snprintf(file, sizeof file, "%.*s.c5m", TRANSLATOR_FILE_MAX - 4, name);
    addTranslatorText(translators.modCount, file, 0, name, TRANSLATOR_NAME_LIMIT);
    return translators.modCount++;
}

static void addTranslatorText(int mod, const char *file, int line, const char *text, int limit) {
    if (!text[0] || strlen(text) > TRANSLATOR_TEXT_MAX) return;
    int known = translatorFind(&translators.textIndex, text, mod);
    if (known >= 0) {
        if (limit < translators.texts[known].limit) translators.texts[known].limit = limit;
        return;
    }
    char *copy = strdup(text);
    if (!copy || !growList((void **)&translators.texts, &translators.textCapacity, translators.textCount + 1, sizeof *translators.texts)) {
        free(copy);
        return;
    }
    TranslatorText *t = &translators.texts[translators.textCount];
    *t = (TranslatorText){copy, mod, line, limit, ""};
    snprintf(t->file, sizeof t->file, "%.*s", TRANSLATOR_FILE_MAX, file);
    if (!translatorIndexPut(&translators.textIndex, translators.textCount)) {
        free(copy);
        return;
    }
    translators.textCount++;
}

static void addTranslatorProblem(int mod, const char *file, int line, int kind, const char *text) {
    char *copy = strdup(text);
    if (!copy || !growList((void **)&translators.problems, &translators.problemCapacity, translators.problemCount + 1, sizeof *translators.problems)) {
        free(copy);
        return;
    }
    TranslatorProblem *p = &translators.problems[translators.problemCount++];
    *p = (TranslatorProblem){copy, mod, line, kind, ""};
    snprintf(p->file, sizeof p->file, "%.*s", TRANSLATOR_FILE_MAX, file);
}

// ---- Lines of a mod's .c5m, .c5se and included files -------------------------------------------
static int translatorQuotedTexts(const char *line, char texts[][TRANSLATED_SIZE], int max, int *hashInText, int keepHash) {
    int count = 0;
    *hashInText = 0;
    for (const char *p = line; *p && *p != '#' && *p != '\r' && *p != '\n'; p++) {
        if (*p != '"') continue;
        const char *start = ++p;
        size_t n = strcspn(start, keepHash ? "\"\r\n" : "\"#\r\n");
        if (start[n] == '#') {
            *hashInText = 1;
            if (count >= max) return count;
            snprintf(texts[count], TRANSLATED_SIZE, "%.*s", (int)(n < TRANSLATED_SIZE - 1 ? n : TRANSLATED_SIZE - 1), start);
            return count + 1;
        }
        if (start[n] != '"') return count;
        if (count < max) snprintf(texts[count], TRANSLATED_SIZE, "%.*s", (int)(n < TRANSLATED_SIZE - 1 ? n : TRANSLATED_SIZE - 1), start);
        count++;
        p = start + n;
    }
    return count;
}

static int translatorWordLimit(const char *word, int *numbered) {
    size_t n = strlen(word);
    *numbered = 0;
    if (!strcmp(word, "message")) return *numbered = 1, TRANSLATOR_TEXT_MAX;
    if (!strcmp(word, "comname") || !strcmp(word, "namesquare") || !strcmp(word, "addname")) return TRANSLATOR_NAME_LIMIT;
    if (!strcmp(word, "label")) return CHECKBOX_TEXT_SIZE - 1;
    if (!strcmp(word, "tip")) return TOOLTIP_TEXT_SIZE - 1;
    if (!strcmp(word, "option")) return *numbered = 1, RADIO_OPTION_TEXT_SIZE - 1;
    if (!strcmp(word, "optiontip")) return *numbered = 1, TOOLTIP_TEXT_SIZE - 1;
    if (!strcmp(word, "description") || !strcmp(word, "newsettingheader")) return TRANSLATOR_TEXT_MAX;
    if (!strcmp(word, "name") || !strcmp(word, "setname") || !strcmp(word, "setclassname")) return TRANSLATOR_NAME_LIMIT;
    if (!strncmp(word, "new", 3) && strncmp(word, "newsetting", 10)) return TRANSLATOR_NAME_LIMIT;
    if (n >= 5 && !strcmp(word + n - 5, "descr")) return TRANSLATOR_TEXT_MAX;
    return 0;
}

static const char *terrainNameArticle(const char *name) {
    return name[0] == 'A' || name[0] == 'E' || name[0] == 'I' || name[0] == 'O' ? "an" : "a";
}

static void readTranslatorSourceLine(int mod, const char *file, int line, const char *word, const char *args) {
    int numbered, limit = translatorWordLimit(word, &numbered);
    if (!limit) return;
    const char *p = args;
    if (numbered) {
        while (*p == ' ' || *p == '\t') p++;
        while ((*p >= '0' && *p <= '9') || *p == '-') p++;
    }
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return;
    static char texts[2][TRANSLATED_SIZE];
    int hash, count = translatorQuotedTexts(p, texts, 2, &hash, 0);
    if (!count) return;
    if (hash) {
        addTranslatorProblem(mod, file, line, TRANSLATOR_HASH_IN_TEXT, texts[0]);
        return;
    }
    if (count > 1) addTranslatorProblem(mod, file, line, TRANSLATOR_QUOTE_IN_TEXT, texts[0]);
    if (strchr(texts[0], '^') && !languageWordTakesCaret(word)) {
        addTranslatorProblem(mod, file, line, TRANSLATOR_CARET_IN_TEXT, texts[0]);
        return;
    }
    const char *text = texts[0];
    if (!strcmp(word, "namesquare") && text[0] == '!' && text[1] && text[2]) text += 2;
    addTranslatorText(mod, file, line, text, limit);
    if (strcmp(word, "name") || !translators.terrainOpen || translators.terrainFile != modFileNow()) return;
    char article[TRANSLATED_SIZE];
    snprintf(article, sizeof article, "%s %.*s", terrainNameArticle(text), TRANSLATED_SIZE - 4, text);
    addTranslatorText(mod, file, line, article, limit);
}

static void addTranslatorLine(int mod, const char *file, int line, char *from, char *to, int rank, int taken) {
    if (!from || !to || !growList((void **)&translators.lines, &translators.lineCapacity, translators.lineCount + 1, sizeof *translators.lines)) {
        free(from), free(to);
        return;
    }
    TranslatorLine *l = &translators.lines[translators.lineCount];
    *l = (TranslatorLine){from, to, mod, line, rank, taken, ""};
    snprintf(l->file, sizeof l->file, "%.*s", TRANSLATOR_FILE_MAX, file);
    int shown = taken && (rank > 0 || !strchr(from, '^'));
    if (shown && !translatorIndexPut(&translators.finalIndex, translators.lineCount)) {
        free(from), free(to);
        return;
    }
    translators.lineCount++;
}

static void readTranslatorLine(int mod, const char *file, int line, const char *args, int rank, int taken) {
    static char texts[3][TRANSLATED_SIZE];
    int hash, count = translatorQuotedTexts(args, texts, 3, &hash, rank > 0);
    if (hash) {
        if (count) addTranslatorProblem(mod, file, line, TRANSLATOR_HASH_IN_TEXT, texts[count - 1]);
        return;
    }
    if (count < 2) return;
    if (count > 2) addTranslatorProblem(mod, file, line, TRANSLATOR_QUOTE_IN_TEXT, texts[1]);
    char *from = strdup(texts[0]), *to = strdup(texts[1]);
    addTranslatorLine(mod, file, line, from, to, rank, taken);
}

static void noteTranslatorRecordPair(int mod, int in, int line, const char *fromText, const char *toText, int rank, int taken) {
    if (!translators.collecting || mod < 0 || mod > G32(RVA_MOD_LAST_INDEX)) return;
    int m = translatorMod(modName(mod), modFolder(mod));
    if (m < 0) return;
    char file[TRANSLATOR_FILE_MAX + 1];
    if (in >= SETTINGS_IN_INCLUDED) snprintf(file, sizeof file, "%.*s", TRANSLATOR_FILE_MAX, includeLabelText(in - SETTINGS_IN_INCLUDED) + 1);
    else snprintf(file, sizeof file, "%.60s%s", modName(mod), in == SETTINGS_IN_C5M ? MOD_FILE_SUFFIX : SIDECAR_SUFFIX);
    char *from = strdup(fromText), *to = strdup(toText);
    for (char *c = from; c && *c; c++) *c = *c == '\n' ? '^' : *c;
    for (char *c = to; c && *c; c++) *c = *c == '\n' ? '^' : *c;
    addTranslatorLine(m, file, line, from, to, rank, taken);
}

// ---- Texts in a mod's scripts ------------------------------------------------------------------
static const struct { const char *call; int arg; const char *field; int limit; } TRANSLATOR_LUA_TEXTS[] = {
    {"coe5.translate", 1, NULL, TRANSLATOR_TEXT_MAX},
    {"coe5.ui.screenBegin", 1, NULL, SCREEN_TITLE_TEXT_SIZE - 1},
    {"coe5.ui.sectionBegin", 1, NULL, TRANSLATOR_TEXT_MAX},
    {"coe5.ui.sectionHeader", 1, NULL, TRANSLATOR_TEXT_MAX},
    {"coe5.ui.text", 2, NULL, TEXT_BUTTON_TEXT_SIZE - 1},
    {"coe5.ui.checkbox", 3, NULL, CHECKBOX_TEXT_SIZE - 1},
    {"coe5.ui.choice", 3, NULL, RADIO_OPTION_TEXT_SIZE - 1},
    {"coe5.ui.tooltip", 1, NULL, TOOLTIP_TEXT_SIZE - 1},
    {"coe5.ui.listRow", 2, NULL, TEXT_BUTTON_TEXT_SIZE - 1},
    {"coe5.ui.button", 2, NULL, UI_BUTTON_TEXT - 1},
    {"coe5.ui.button", 8, NULL, UI_BUTTON_TEXT - 1},
    {"coe5.ui.drawText", 4, NULL, DRAW_TEXT_SIZE - 1},
    {"coe5.ui.nameBox", 1, NULL, SCREEN_TITLE_TEXT_SIZE - 1},
    {"coe5.ui.nameBox", 2, NULL, NAME_BOX_PROMPT_SIZE - 1},
    {"coe5.ui.monsterRow", 3, NULL, TEXT_BUTTON_TEXT_SIZE - 1},
    {"coe5.ui.monsterRow", 4, NULL, TEXT_BUTTON_TEXT_SIZE - 1},
    {"coe5.addGameMenuLine", 1, "label", TEXT_BUTTON_TEXT_SIZE - 1},
    {"coe5.addGameMenuLine", 1, "tooltip", TOOLTIP_TEXT_SIZE - 1},
    {"coe5.addMapEditorLine", 1, "label", TEXT_BUTTON_TEXT_SIZE - 1},
    {"coe5.addMapEditorLine", 1, "tooltip", TOOLTIP_TEXT_SIZE - 1},
    {"coe5.addCommanderLine", 1, "label", TRANSLATOR_TEXT_MAX},
    {"coe5.addCommanderLine", 1, "tooltip", TOOLTIP_TEXT_SIZE - 1},
};
#define TRANSLATOR_LUA_TEXT_ROWS ((int)(sizeof TRANSLATOR_LUA_TEXTS / sizeof TRANSLATOR_LUA_TEXTS[0]))

typedef enum { LUA_TOKEN_END, LUA_TOKEN_NAME, LUA_TOKEN_STRING, LUA_TOKEN_PUNCT, LUA_TOKEN_OTHER } LuaTokenType;

typedef struct {
    int type, line, cut;
    const char *start;
    size_t length;
    char value[TRANSLATED_SIZE];
    size_t valueLength;
} LuaToken;

typedef struct { const char *s; size_t n, at; int line; } LuaSource;

static void luaTokenByte(LuaToken *t, unsigned char c) {
    if (t->valueLength + 1 >= sizeof t->value) t->cut = 1;
    else t->value[t->valueLength++] = (char)c;
}

static void luaTokenLetter(LuaToken *t, uint32_t c) {
    char bytes[4];
    int n = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
    if (n == 1) bytes[0] = (char)c;
    else {
        for (int i = n - 1; i > 0; i--, c >>= 6) bytes[i] = (char)(0x80 | (c & 0x3f));
        bytes[0] = (char)((0xf00 >> n) | c);
    }
    for (int i = 0; i < n; i++) luaTokenByte(t, (unsigned char)bytes[i]);
}

static int luaLongBracket(const LuaSource *src, size_t at) {
    if (at >= src->n || src->s[at] != '[') return -1;
    size_t i = at + 1;
    while (i < src->n && src->s[i] == '=') i++;
    return i < src->n && src->s[i] == '[' ? (int)(i - at - 1) : -1;
}

static void luaSkipLong(LuaSource *src, int level, LuaToken *t) {
    src->at += (size_t)level + 2;
    if (src->at < src->n && src->s[src->at] == '\r') src->at++;
    if (src->at < src->n && src->s[src->at] == '\n') src->at++;
    for (; src->at < src->n; src->at++) {
        char c = src->s[src->at];
        if (c == ']') {
            size_t i = src->at + 1;
            while (i < src->n && src->s[i] == '=') i++;
            if (i < src->n && src->s[i] == ']' && (int)(i - src->at - 1) == level) {
                src->at = i + 1;
                return;
            }
        }
        if (c == '\n') src->line++;
        if (t) luaTokenByte(t, (unsigned char)c);
    }
}

static int luaReadQuoted(LuaSource *src, char quote, LuaToken *t) {
    while (src->at < src->n) {
        char c = src->s[src->at++];
        if (c == quote) return 1;
        if (c == '\n' || c == '\r') return 0;
        if (c != '\\') {
            luaTokenByte(t, (unsigned char)c);
            continue;
        }
        if (src->at >= src->n) return 0;
        c = src->s[src->at++];
        const char *simple = strchr("abfnrtv\\\"'", c);
        static const char VALUES[] = "\a\b\f\n\r\t\v\\\"'";
        if (simple && c) luaTokenByte(t, (unsigned char)VALUES[simple - "abfnrtv\\\"'"]);
        else if (c == '\n' || c == '\r') {
            src->line++;
            luaTokenByte(t, '\n');
            if (src->at < src->n && (src->s[src->at] == '\n' || src->s[src->at] == '\r') && src->s[src->at] != c) src->at++;
        } else if (c == 'z') {
            while (src->at < src->n && strchr(" \t\r\n\f\v", src->s[src->at])) src->line += src->s[src->at++] == '\n';
        } else if (c == 'x') {
            unsigned int v = 0;
            for (int i = 0; i < 2 && src->at < src->n && isxdigit((unsigned char)src->s[src->at]); i++)
                v = v * 16 + (unsigned int)(isdigit((unsigned char)src->s[src->at]) ? src->s[src->at] - '0' : (tolower((unsigned char)src->s[src->at]) - 'a' + 10)), src->at++;
            luaTokenByte(t, (unsigned char)v);
        } else if (c == 'u') {
            uint32_t v = 0;
            if (src->at < src->n && src->s[src->at] == '{') src->at++;
            while (src->at < src->n && isxdigit((unsigned char)src->s[src->at]) && v < 0x110000)
                v = v * 16 + (uint32_t)(isdigit((unsigned char)src->s[src->at]) ? src->s[src->at] - '0' : (tolower((unsigned char)src->s[src->at]) - 'a' + 10)), src->at++;
            if (src->at < src->n && src->s[src->at] == '}') src->at++;
            if (v < 0x110000) luaTokenLetter(t, v);
        } else if (isdigit((unsigned char)c)) {
            unsigned int v = (unsigned int)(c - '0');
            for (int i = 0; i < 2 && src->at < src->n && isdigit((unsigned char)src->s[src->at]); i++) v = v * 10 + (unsigned int)(src->s[src->at++] - '0');
            luaTokenByte(t, (unsigned char)v);
        } else return 0;
    }
    return 0;
}

static void luaNextToken(LuaSource *src, LuaToken *t) {
    for (;;) {
        while (src->at < src->n && strchr(" \t\r\n\f\v", src->s[src->at])) src->line += src->s[src->at++] == '\n';
        if (src->at + 1 < src->n && src->s[src->at] == '-' && src->s[src->at + 1] == '-') {
            src->at += 2;
            int level = luaLongBracket(src, src->at);
            if (level >= 0) luaSkipLong(src, level, NULL);
            else
                while (src->at < src->n && src->s[src->at] != '\n') src->at++;
            continue;
        }
        break;
    }
    t->line = src->line, t->cut = 0, t->valueLength = 0, t->start = src->s + src->at, t->length = 1;
    if (src->at >= src->n) {
        t->type = LUA_TOKEN_END;
        return;
    }
    char c = src->s[src->at];
    int level = luaLongBracket(src, src->at);
    if (level >= 0) {
        luaSkipLong(src, level, t);
        t->type = LUA_TOKEN_STRING;
    } else if (c == '"' || c == '\'') {
        src->at++;
        t->type = luaReadQuoted(src, c, t) ? LUA_TOKEN_STRING : LUA_TOKEN_OTHER;
    } else if (isalpha((unsigned char)c) || c == '_') {
        while (src->at < src->n && (isalnum((unsigned char)src->s[src->at]) || src->s[src->at] == '_')) src->at++;
        t->type = LUA_TOKEN_NAME;
    } else if (isdigit((unsigned char)c) || (c == '.' && src->at + 1 < src->n && isdigit((unsigned char)src->s[src->at + 1]))) {
        while (src->at < src->n && (isalnum((unsigned char)src->s[src->at]) || src->s[src->at] == '.' ||
                                    ((src->s[src->at] == '+' || src->s[src->at] == '-') && strchr("eEpP", src->s[src->at - 1]))))
            src->at++;
        t->type = LUA_TOKEN_OTHER;
    } else if (c == '.' && src->at + 1 < src->n && src->s[src->at + 1] == '.') {
        src->at += 2;
        if (src->at < src->n && src->s[src->at] == '.') src->at++;
        t->type = LUA_TOKEN_OTHER;
    } else if (strchr("=<>~", c) && src->at + 1 < src->n && src->s[src->at + 1] == '=') {
        src->at += 2;
        t->type = LUA_TOKEN_OTHER;
    } else {
        src->at++;
        t->type = strchr("(){}[],;.=", c) ? LUA_TOKEN_PUNCT : LUA_TOKEN_OTHER;
    }
    t->length = (size_t)(src->s + src->at - t->start);
    t->value[t->valueLength] = '\0';
}

static int luaTokenIs(const LuaToken *t, const char *text) {
    return t->length == strlen(text) && !memcmp(t->start, text, t->length);
}

static int translatorLuaCallKnown(const char *call) {
    for (int i = 0; i < TRANSLATOR_LUA_TEXT_ROWS; i++)
        if (!strcmp(TRANSLATOR_LUA_TEXTS[i].call, call)) return 1;
    return 0;
}

static void takeTranslatorLuaText(int mod, const char *file, const char *call, int arg, const char *field, const LuaToken *t) {
    for (int i = 0; i < TRANSLATOR_LUA_TEXT_ROWS; i++) {
        if (strcmp(TRANSLATOR_LUA_TEXTS[i].call, call) || TRANSLATOR_LUA_TEXTS[i].arg != arg) continue;
        if (!TRANSLATOR_LUA_TEXTS[i].field != !field || (field && strcmp(TRANSLATOR_LUA_TEXTS[i].field, field))) continue;
        if (t->cut || strlen(t->value) != t->valueLength) return;
        if (strchr(t->value, '"')) addTranslatorProblem(mod, file, t->line, TRANSLATOR_QUOTE_IN_TEXT, t->value);
        else if (strchr(t->value, '\r')) addTranslatorProblem(mod, file, t->line, TRANSLATOR_BREAK_IN_TEXT, t->value);
        else if (strchr(t->value, '^')) addTranslatorProblem(mod, file, t->line, TRANSLATOR_CARET_IN_TEXT, t->value);
        else {
            static char caret[TRANSLATED_SIZE];
            snprintf(caret, sizeof caret, "%s", t->value);
            for (char *c = caret; *c; c++)
                if (*c == '\n') *c = '^';
            addTranslatorText(mod, file, t->line, caret, TRANSLATOR_LUA_TEXTS[i].limit);
        }
        return;
    }
}

#define LUA_CALL_FRAMES 16
#define LUA_CALL_NAME   48

typedef struct {
    char call[LUA_CALL_NAME];
    int argDepth, fieldDepth, tableArg, arg, expecting;
    char key[32];
    LuaToken *pending;
} LuaCallFrame;

static void readTranslatorScript(int mod, const char *file, const char *text, size_t length) {
    static LuaToken token, pending[LUA_CALL_FRAMES];
    LuaSource src = {text, length, 0, 1};
    if (length >= 3 && !memcmp(text, "\xef\xbb\xbf", 3)) src.at = 3;
    LuaCallFrame frames[LUA_CALL_FRAMES];
    int frameCount = 0, depth = 0;
    char chain[LUA_CALL_NAME] = "";
    int chainOpen = 0, lastWasDot = 0, beforeName = 0;
    for (;;) {
        luaNextToken(&src, &token);
        if (token.type == LUA_TOKEN_END) return;
        for (int f = 0; f < frameCount; f++) {
            LuaCallFrame *fr = &frames[f];
            int atArgs = depth == fr->argDepth && fr->fieldDepth < 0, atFields = depth == fr->fieldDepth;
            if (!atArgs && !atFields) continue;
            int ends = luaTokenIs(&token, ",") || luaTokenIs(&token, ";") || luaTokenIs(&token, ")") || luaTokenIs(&token, "}") || luaTokenIs(&token, "]");
            if (ends && fr->pending) takeTranslatorLuaText(mod, file, fr->call, atFields ? fr->tableArg : fr->arg, atFields && fr->key[0] ? fr->key : NULL, fr->pending);
            if (fr->pending && !ends) fr->pending = NULL;
            if (ends) {
                fr->pending = NULL;
                if (luaTokenIs(&token, ",") || luaTokenIs(&token, ";")) {
                    fr->expecting = 1;
                    if (atArgs) fr->arg++;
                    else fr->key[0] = '\0';
                }
                continue;
            }
            if (token.type == LUA_TOKEN_STRING && fr->expecting) {
                pending[f] = token;
                fr->pending = &pending[f];
            } else if (atFields && token.type == LUA_TOKEN_NAME && fr->expecting) {
                snprintf(fr->key, sizeof fr->key, "%.*s", (int)(token.length < sizeof fr->key ? token.length : sizeof fr->key - 1), token.start);
                fr->expecting = 0;
                continue;
            } else if (atFields && luaTokenIs(&token, "=") && fr->key[0]) {
                fr->expecting = 1;
                continue;
            } else if (atArgs && luaTokenIs(&token, "{") && fr->expecting) {
                fr->fieldDepth = depth + 1, fr->tableArg = fr->arg, fr->key[0] = '\0';
                continue;
            }
            fr->expecting = 0;
        }
        int opens = luaTokenIs(&token, "(") || luaTokenIs(&token, "{");
        if (chainOpen && !lastWasDot && (opens || token.type == LUA_TOKEN_STRING) && translatorLuaCallKnown(chain)) {
            if (token.type == LUA_TOKEN_STRING) takeTranslatorLuaText(mod, file, chain, 1, NULL, &token);
            else if (frameCount < LUA_CALL_FRAMES) {
                LuaCallFrame *fr = &frames[frameCount++];
                snprintf(fr->call, sizeof fr->call, "%s", chain);
                fr->argDepth = depth + 1, fr->arg = 1, fr->expecting = 1, fr->key[0] = '\0', fr->pending = NULL;
                fr->fieldDepth = luaTokenIs(&token, "{") ? depth + 1 : -1, fr->tableArg = 1;
            }
        }
        if (token.type == LUA_TOKEN_NAME && !lastWasDot && luaTokenIs(&token, "coe5") && !beforeName) {
            snprintf(chain, sizeof chain, "coe5");
            chainOpen = 1;
        } else if (chainOpen && token.type == LUA_TOKEN_NAME && lastWasDot && strlen(chain) + 1 + token.length < sizeof chain) {
            size_t at = strlen(chain);
            snprintf(chain + at, sizeof chain - at, ".%.*s", (int)token.length, token.start);
        } else if (!(chainOpen && luaTokenIs(&token, ".") && !lastWasDot)) {
            chainOpen = 0;
        }
        beforeName = luaTokenIs(&token, ".") || luaTokenIs(&token, ":");
        lastWasDot = luaTokenIs(&token, ".");
        if (opens || luaTokenIs(&token, "[")) depth++;
        else if (luaTokenIs(&token, ")") || luaTokenIs(&token, "}") || luaTokenIs(&token, "]")) {
            depth--;
            while (frameCount && depth < frames[frameCount - 1].argDepth) frameCount--;
            for (int f = 0; f < frameCount; f++)
                if (frames[f].fieldDepth > depth) frames[f].fieldDepth = -1, frames[f].expecting = 0;
        }
    }
}

// ---- The game's % rule -------------------------------------------------------------------------
static int translationConversionsMatch(const char *from, const char *to) {
    size_t n = strlen(from);
    while (n && (from[n - 1] == '\n' || from[n - 1] == '\r')) n--;
    for (int k = 0;; k++) {
        int a = 0, b = 0, seen = -1;
        for (size_t i = 0; i < n; i++)
            if (from[i] == '%' && ++seen == k) {
                a = i + 1 < n ? (unsigned char)from[i + 1] : 0;
                break;
            }
        seen = -1;
        for (const char *c = to; *c; c++)
            if (*c == '%' && ++seen == k) {
                b = (unsigned char)c[1];
                break;
            }
        if (a != b) return 0;
        if (!a) return 1;
    }
}

// ---- Writing -----------------------------------------------------------------------------------
static int translatorFinal(const char *text) { return translatorFind(&translators.finalIndex, text, -1); }

static void writeTranslatorQuote(FILE *f, const char *text) {
    size_t n = textCutLength(text, TRANSLATOR_QUOTE_SHOWN);
    fputc('"', f);
    for (size_t i = 0; i < n; i++) fputc(text[i] == '\n' || text[i] == '\r' ? '^' : text[i], f);
    fprintf(f, "%s\"", text[n] ? "..." : "");
}

static const char *translatorLanguageName(const char *pick) {
    for (int i = 0; i < languages.declaredCount; i++)
        if (!_stricmp(languages.declared[i].tag, pick)) return languages.declared[i].name;
    return "";
}

static int translatorFileNameOk(const char *name) {
    if (!name[0]) return 0;
    for (const unsigned char *c = (const unsigned char *)name; *c; c++)
        if (*c < 0x20 || *c > 0x7e || strchr("\\/:*?\"<>|", *c)) return 0;
    return 1;
}

static void writeTranslatorTemplate(FILE *f, int mod, const char *pick, int reads) {
    const char *name = translators.mods[mod].name, *languageName = translatorLanguageName(pick);
    fprintf(f, "# CoE5SE: translation template for %s\n", name);
    fprintf(f, "# Every translatable string CoE5SE found in this mod, listed once each.\n");
    fprintf(f, "# Uncommented lines are currently active translations. Commented lines (#) still need translation:\n");
    fprintf(f, "# fill in the second quoted string and remove the leading #.\n");
    fprintf(f, "# Copy this file into your mod before editing (CoE5SE overwrites this template whenever mods reload).\n");
    if (pick[0] && languageName[0]) fprintf(f, "language \"%s\" \"%s\"\n", pick, languageName);
    else if (pick[0]) fprintf(f, "language \"%s\"\n", pick);
    else if (reads) fprintf(f, "# Language is set to Default in Mod Settings (each enabled translation mod uses its declared language).\n# Start your file with a language tag and display name:\n# language \"fr\" \"Fran\xc3\xa7" "ais\"\n");
    else fprintf(f, "# No language selected in Mod Settings. Start your file with a language tag and display name:\n# language \"fr\" \"Fran\xc3\xa7" "ais\"\n");
    const char *file = NULL;
    for (int i = 0; i < translators.textCount; i++) {
        const TranslatorText *t = &translators.texts[i];
        if (t->mod != mod) continue;
        if (!file || strcmp(file, t->file)) fprintf(f, "\n# %s\n", file = t->file);
        int final = reads ? translatorFinal(t->text) : -1;
        if (final >= 0) fprintf(f, "translation \"%s\" \"%s\"\n", t->text, translators.lines[final].to);
        else fprintf(f, "# translation \"%s\" \"\"\n", t->text);
    }
    for (int i = 0; i < translators.problemCount; i++) {
        const TranslatorProblem *p = &translators.problems[i];
        if (p->mod != mod || !p->text[0]) continue;
        static const char *const HOLDS[] = {"", "a #", "a \"", "a carriage return", "a ^"};
        fprintf(f, "\n# Not translatable as written: %s line %d holds %s inside a text\n", p->file, p->line, HOLDS[p->kind]);
        if (p->kind != TRANSLATOR_BREAK_IN_TEXT && !strchr(p->text, '\n')) fprintf(f, "# %s\n", p->text);
    }
}

static int translatorAnyModHas(const char *text) {
    for (int m = 0; m < translators.modCount; m++)
        if (translatorFind(&translators.textIndex, text, m) >= 0) return 1;
    return 0;
}

static int writeTranslatorLetters(FILE *f, const TranslatorLine *l, int (*hasLetter)(uint32_t code)) {
    const unsigned char *s = (const unsigned char *)l->to;
    size_t n = strlen(l->to);
    uint32_t reported[8];
    int reportedCount = 0;
    for (size_t at = 0; at < n;) {
        uint32_t code;
        int length = fallbackReadLetter(s + at, n - at, &code);
        int known = 0;
        for (int i = 0; i < reportedCount; i++) known |= reported[i] == code;
        if (!known && code >= 0x20) {
            int has = hasLetter(code);
            if (has < 0) return 0;
            if (!has && reportedCount < 8) {
                reported[reportedCount++] = code;
                fprintf(f, "No font has U+%04X \"%.*s\": ", (unsigned int)code, code == FALLBACK_BAD_BYTE ? 0 : length, (const char *)s + at);
                writeTranslatorQuote(f, l->from);
                fprintf(f, " (%s line %d)\n", l->file, l->line);
            }
        }
        at += (size_t)length;
    }
    return 1;
}

static void writeTranslatorReport(FILE *f, const char *pick, int reads, int fromDefault, int (*hasLetter)(uint32_t code)) {
    const char *languageName = translatorLanguageName(pick);
    fprintf(f, "CoE5SE: translation report\n");
    if (!reads) fprintf(f, "Language: Default (none picked on CoE5SE's page in Mod Settings): nothing to count\n");
    else if (!pick[0]) fprintf(f, "Language: Default on CoE5SE's page in Mod Settings, each enabled translation mod in its own language\n");
    else if (languageName[0]) fprintf(f, "Language: %s (%s)%s\n", languageName, pick, fromDefault ? ", the enabled translation mods' language (Default on CoE5SE's page in Mod Settings)" : "");
    else fprintf(f, "Language: %s%s\n", pick, fromDefault ? ", the enabled translation mods' language (Default on CoE5SE's page in Mod Settings)" : "");
    fprintf(f, "Mods: %d. Written at each mods pass while the game runs with --dumpstrings.\n", translators.modCount);
    fprintf(f, "Missing: no translation line. Not used: the game ignores the line, its %% codes differ from the text's.\n");
    fprintf(f, "Too long: the screen cuts the translation. Not in any enabled mod: vanilla text, or text removed from a mod.\n");
    int lettersChecked = 1;
    for (int m = 0; m < translators.modCount; m++) {
        int texts = 0, translated = 0;
        for (int i = 0; i < translators.textCount; i++) {
            const TranslatorText *t = &translators.texts[i];
            if (t->mod != m) continue;
            int final = reads ? translatorFinal(t->text) : -1;
            texts++;
            translated += final >= 0 && translationConversionsMatch(t->text, translators.lines[final].to);
        }
        if (reads) fprintf(f, "\n== %s: %d texts, %d translated, %d missing ==\n", translators.mods[m].name, texts, translated, texts - translated);
        else fprintf(f, "\n== %s: %d texts ==\n", translators.mods[m].name, texts);
        for (int i = 0; i < translators.textCount && reads; i++) {
            const TranslatorText *t = &translators.texts[i];
            if (t->mod != m) continue;
            int final = translatorFinal(t->text);
            if (final < 0) {
                fprintf(f, "Missing: ");
                writeTranslatorQuote(f, t->text);
                if (t->line) fprintf(f, " (%s line %d)\n", t->file, t->line);
                else fprintf(f, " (the mod's name)\n");
                continue;
            }
            const TranslatorLine *l = &translators.lines[final];
            if (!translationConversionsMatch(t->text, l->to)) {
                fprintf(f, "Not used, %% codes differ: ");
                writeTranslatorQuote(f, t->text);
                fprintf(f, " -> ");
                writeTranslatorQuote(f, l->to);
                fprintf(f, " (%s line %d)\n", l->file, l->line);
            }
            if (!l->to[0]) {
                fprintf(f, "Empty translation, the text shows blank: ");
                writeTranslatorQuote(f, t->text);
                fprintf(f, " (%s line %d)\n", l->file, l->line);
            }
            if ((int)strlen(l->to) > t->limit) {
                fprintf(f, "Too long, %d bytes where %d fit: ", (int)strlen(l->to), t->limit);
                writeTranslatorQuote(f, t->text);
                fprintf(f, " (%s line %d)\n", l->file, l->line);
            }
            if (lettersChecked) lettersChecked = writeTranslatorLetters(f, l, hasLetter);
        }
        for (int i = 0; i < translators.lineCount && reads; i++) {
            const TranslatorLine *l = &translators.lines[i];
            if (l->mod != m || l->rank <= 0 || translatorAnyModHas(l->from)) continue;
            int earlier = 0;
            for (int j = 0; j < i && !earlier; j++)
                earlier = translators.lines[j].mod == m && translators.lines[j].rank > 0 && !strcmp(translators.lines[j].from, l->from);
            if (earlier) continue;
            fprintf(f, "Not in any enabled mod: ");
            writeTranslatorQuote(f, l->from);
            fprintf(f, " (%s line %d)\n", l->file, l->line);
        }
        for (int i = 0; i < translators.problemCount; i++) {
            const TranslatorProblem *p = &translators.problems[i];
            if (p->mod != m) continue;
            static const char *const HOLDS[] = {"", "Holds a #, where the game cuts the line: ", "Holds a \", which no translation line can match: ",
                                                "Holds a carriage return, which no translation line can hold: ",
                                                "Holds a ^, which a language block reads as a line break: "};
            fprintf(f, "%s", HOLDS[p->kind]);
            writeTranslatorQuote(f, p->text);
            fprintf(f, " (%s line %d)\n", p->file, p->line);
        }
    }
    if (!lettersChecked) fprintf(f, "\nLetters not checked: the game's fonts did not open.\n");
}

// ---- The game's side ---------------------------------------------------------------------------
static int dumpStringsOn(void) { return ((const char *)GAME_PTR(RVA_DUMPSTRINGS_FILE))[0] != '\0'; }

static void translatorFileNow(int mod, char *out, size_t size) {
    if (sidecar.including) snprintf(out, size, "%.*s", TRANSLATOR_FILE_MAX, sidecar.includeFile);
    else snprintf(out, size, "%.60s%s", modName(mod), sidecar.reading ? SIDECAR_SUFFIX : MOD_FILE_SUFFIX);
}

static void noteTranslatorLine(const char *word, const char *args, int mod, int level, int inBlock, int rank, int taken) {
    if (!translators.collecting || level != 2 || mod < 0 || mod > G32(RVA_MOD_LAST_INDEX)) return;
    if (inBlock && (rank <= 0 || strcmp(word, "translation"))) return;
    int m = translatorMod(modName(mod), modFolder(mod));
    if (m < 0) return;
    char file[TRANSLATOR_FILE_MAX + 1];
    translatorFileNow(mod, file, sizeof file);
    int lineNo = G32(RVA_MOD_LINE) + 1;
    if (!strncmp(word, "select", 6) || !strncmp(word, "new", 3)) translators.terrainOpen = !strcmp(word, "selectterr"), translators.terrainFile = modFileNow();
    if (!strcmp(word, "translation")) readTranslatorLine(m, file, lineNo, args, inBlock ? rank : 0, taken);
    else readTranslatorSourceLine(m, file, lineNo, word, args);
}

static void translatorsBeforePass(int level) {
    if (level != 2 || !dumpStringsOn()) return;
    clearTranslators();
    translators.collecting = 1;
}

static void readTranslatorScripts(int m) {
    ModScriptList list = {0};
    listModScripts(translators.mods[m].folder, "", &list, 0);
    if (list.count) qsort(list.paths, (size_t)list.count, sizeof list.paths[0], compareModScriptPaths);
    for (int i = 0; i < list.count; i++) {
        char path[MAX_PATH];
        if (!buildPath(path, sizeof path, __func__, "%s" PATH_SEP "%s", translators.mods[m].folder, list.paths[i])) continue;
        FILE *f = fopen(path, "rb");
        long size = -1;
        if (f && !fseek(f, 0, SEEK_END)) size = ftell(f), fseek(f, 0, SEEK_SET);
        char *text = size >= 0 && size <= TRANSLATOR_SCRIPT_MAX ? malloc((size_t)size + 1) : NULL;
        if (!text || fread(text, 1, (size_t)size, f) != (size_t)size) {
            writeLogWarning("translation_script_not_read", "reason=%s mod=%s file=%s", size > TRANSLATOR_SCRIPT_MAX ? "too_big" : "not_read",
                            translators.mods[m].name, list.paths[i]);
            free(text);
            if (f) fclose(f);
            continue;
        }
        fclose(f);
        text[size] = '\0';
        readTranslatorScript(m, list.paths[i], text, (size_t)size);
        free(text);
    }
    free(list.paths);
}

static int translatorFontHasLetter(uint32_t code) { return fallbackChainHasLetter(code); }

static FILE *openTranslatorFile(const char *file) {
    char path[MAX_PATH];
    if (!buildPath(path, sizeof path, __func__, "%s" USER_DIR_SEP TRANSLATOR_FOLDER, userDir())) return NULL;
    CreateDirectoryA(path, NULL);
    if (!buildPath(path, sizeof path, __func__, "%s" USER_DIR_SEP TRANSLATOR_FOLDER USER_DIR_SEP "%s", userDir(), file)) return NULL;
    FILE *f = fopen(path, "wb");
    if (!f) writeLogError("translation_file_not_written", "reason=open_failed file=%s", path);
    return f;
}

static void translatorsAfterPass(int level) {
    if (level != 2 || !translators.collecting) return;
    translators.collecting = 0;
    const char *pick = languagePassTag();
    int reads = pick[0] || languageBlocksRead(), templates = 0;
    for (int m = 0; m < translators.modCount; m++) readTranslatorScripts(m);
    for (int m = 0; m < translators.modCount; m++) {
        const char *name = translators.mods[m].name;
        char file[MOD_NAME_MAX + 8];
        if (!translatorFileNameOk(name) || strlen(name) + sizeof SIDECAR_SUFFIX > sizeof file) {
            writeLogWarning("translation_template_refused", "reason=name_not_ascii mod=%.80s", name);
            continue;
        }
        snprintf(file, sizeof file, "%s" SIDECAR_SUFFIX, name);
        FILE *f = openTranslatorFile(file);
        if (!f) continue;
        writeTranslatorTemplate(f, m, pick, reads);
        if (fclose(f)) writeLogError("translation_file_not_written", "reason=write_failed file=%s", file);
        else templates++;
    }
    FILE *f = openTranslatorFile(TRANSLATOR_REPORT_FILE);
    int report = f != NULL;
    if (f) {
        writeTranslatorReport(f, pick, reads, languages.defaultReads && languages.translationMods, translatorFontHasLetter);
        if (fclose(f)) {
            writeLogError("translation_file_not_written", "reason=write_failed file=%s", TRANSLATOR_REPORT_FILE);
            report = 0;
        }
    }
    writeLog("translation_files_written", "pick=%s mods=%d templates=%d texts=%d lines=%d report=%d folder=%s" USER_DIR_SEP TRANSLATOR_FOLDER,
             pick[0] ? pick : reads ? "several" : "default", translators.modCount, templates, translators.textCount, translators.lineCount, report, userDir());
}
