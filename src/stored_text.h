#pragma once

#define STORED_SLOTS     2048
#define STORED_TURN      1024
#define STORED_ARGS_MAX  16
#define STORED_SHOWN_SIZE WRAPPED_TEXT_SIZE

typedef struct {
    char *given;
    char *format;
    int slots, passed;
    int stringArg[STORED_ARGS_MAX];
    u64 args[STORED_ARGS_MAX];
} StoredText;

static struct {
    uint64_t hash[2][STORED_SLOTS];
    StoredText *text[2][STORED_SLOTS];
    int count[2], current;
    StoredText *pending;
    const char *pendingBuffer;
    const char *givenWhole;
} storedTexts;

static void freeStoredText(StoredText *t) {
    if (!t) return;
    for (int k = 0; k < t->slots; k++)
        if (t->stringArg[k]) free((void *)(uintptr_t)t->args[k]);
    free(t->given);
    free(t->format);
    free(t);
}

static char *copyStoredString(const char *s) {
    size_t n = strlen(s) + 1;
    char *c = malloc(n);
    if (c) memcpy(c, s, n);
    return c;
}

static StoredText *newStoredText(const char *format, const u64 *args, const int *stringArg, int slots, const char *given) {
    StoredText *t = calloc(1, sizeof *t);
    if (!t) return NULL;
    t->given = copyStoredString(given);
    t->format = format ? copyStoredString(format) : NULL;
    int ok = t->given && (!format || t->format);
    for (int k = 0; k < slots && ok; k++) {
        t->stringArg[k] = stringArg[k] && args[k];
        t->args[k] = t->stringArg[k] ? (u64)(uintptr_t)copyStoredString((const char *)(uintptr_t)args[k]) : args[k];
        t->slots = k + 1;
        ok = !t->stringArg[k] || t->args[k];
    }
    if (ok) return t;
    freeStoredText(t);
    return NULL;
}

static StoredText *copyStoredText(const StoredText *from) {
    return newStoredText(from->format, from->args, from->stringArg, from->slots, from->given);
}

static int storedSlot(int g, uint64_t h) {
    unsigned int at = (unsigned int)(h ^ (h >> 29)) & (STORED_SLOTS - 1);
    while (storedTexts.hash[g][at] && storedTexts.hash[g][at] != h) at = (at + 1) & (STORED_SLOTS - 1);
    return (int)at;
}

static void forgetStoredGeneration(int g) {
    for (int i = 0; i < STORED_SLOTS; i++) freeStoredText(storedTexts.text[g][i]);
    memset(storedTexts.hash[g], 0, sizeof storedTexts.hash[g]);
    memset(storedTexts.text[g], 0, sizeof storedTexts.text[g]);
    storedTexts.count[g] = 0;
}

static void noteStoredText(const char *stored, StoredText *t) {
    if (!t) return;
    if (!stored || !stored[0]) {
        freeStoredText(t);
        return;
    }
    uint64_t h = drawnTextHash(stored, (size_t)-1);
    int g = storedTexts.current, at = storedSlot(g, h);
    if (!storedTexts.hash[g][at] && storedTexts.count[g] >= STORED_TURN) {
        g = storedTexts.current ^= 1;
        forgetStoredGeneration(g);
        at = storedSlot(g, h);
    }
    if (!storedTexts.hash[g][at]) storedTexts.count[g]++;
    freeStoredText(storedTexts.text[g][at]);
    storedTexts.hash[g][at] = h;
    storedTexts.text[g][at] = t;
}

static StoredText *storedTextOf(const char *text) {
    if (!text || !text[0]) return NULL;
    uint64_t h = drawnTextHash(text, (size_t)-1);
    int g = storedTexts.current, at = storedSlot(g, h);
    if (storedTexts.text[g][at]) return storedTexts.text[g][at];
    int before = g ^ 1, was = storedSlot(before, h);
    StoredText *t = storedTexts.text[before][was];
    if (!t) return NULL;
    storedTexts.text[before][was] = NULL;
    noteStoredText(text, t);
    return t;
}

// ---- What a stored text was made from -----------------------------------------------------------
static void noteStoredFormat(const char *buf, const char *fmt, const u64 *args) {
    freeStoredText(storedTexts.pending);
    storedTexts.pending = NULL;
    if (!drawnText.installed || !buf || !fmt || !args) return;
    int stringArg[MAX_FORMAT_ARGS];
    int slots = scanFormatArgs(fmt, stringArg);
    if (slots < 0 || slots > STORED_ARGS_MAX) return;
    StoredText *known = slots == 1 && stringArg[0] && !strcmp(fmt, "%s") ? storedTextOf((const char *)(uintptr_t)args[0]) : NULL;
    storedTexts.pending = known ? copyStoredText(known)
                                : newStoredText(fmt, args, stringArg, slots, storedTexts.givenWhole ? storedTexts.givenWhole : buf);
    storedTexts.pendingBuffer = buf;
}

static void noteStoredMessage(const char *text) {
    StoredText *t = storedTexts.pending;
    storedTexts.pending = NULL;
    if (!t) return;
    if (text != storedTexts.pendingBuffer) {
        freeStoredText(t);
        return;
    }
    t->passed = 1;
    noteStoredText(text, t);
}

#ifdef __APPLE__
#define SLOTS_AS_VA_LIST(slots) ((va_list)(void *)(slots))
#else
#define SLOTS_AS_VA_LIST(slots) ((va_list)(slots))
#endif

static const char *formatStoredAgain(const StoredText *t, char *shown, size_t size) {
    static char format[TRANSLATED_SIZE];
    static char argText[MAX_TRANSLATED_ARGS][TRANSLATED_SIZE];
    if (!t->format || strlen(t->format) >= sizeof format - 1) return NULL;
    translateText(t->format, format, sizeof format);
    int formatTranslated = strcmp(format, t->format) != 0, changed = formatTranslated;
    u64 args[STORED_ARGS_MAX];
    memcpy(args, t->args, sizeof args);
    for (int k = 0, used = 0; k < t->slots && used < MAX_TRANSLATED_ARGS; k++) {
        const char *s = (const char *)(uintptr_t)args[k];
        if (!t->stringArg[k] || strlen(s) >= sizeof argText[0] - 1) continue;
        translateText(s, argText[used], sizeof argText[0]);
        if (strcmp(argText[used], s)) args[k] = (u64)(uintptr_t)argText[used++], changed = 1;
    }
    if (!changed) return NULL;
    vsnprintf(shown, size, format, SLOTS_AS_VA_LIST(args));
    if (!formatTranslated && t->passed && gameEnglishPass) gameEnglishPass(shown, (int)size);
    endAtWholeLetter(shown, size);
    return shown;
}

static const char *storedTextShown(const char *text) {
    static char shown[STORED_SHOWN_SIZE];
    if (!drawnText.installed || G32(RVA_TRANSLATION_COUNT) < 0) return NULL;
    StoredText *t = storedTextOf(text);
    if (!t || strlen(text) >= sizeof shown - 1) return NULL;
    translateText(text, shown, sizeof shown);
    if (strcmp(shown, text)) return shown;
    if (strlen(t->given) < sizeof shown - 1) {
        translateText(t->given, shown, sizeof shown);
        if (strcmp(shown, t->given)) return shown;
    }
    return formatStoredAgain(t, shown, sizeof shown);
}

// ---- The hooks ----------------------------------------------------------------------------------
static TextFormatFn nextStoredFormat;
#ifdef __APPLE__
static int storedFormatHook(char *buf, size_t size, const char *fmt, va_list args) {
    int n = nextStoredFormat(buf, size, fmt, args);
    noteStoredFormat(buf, fmt, (const u64 *)(const void *)args);
    return n;
}
#else
static int storedFormatHook(u64 options, char *buf, size_t size, const char *fmt, void *locale, va_list args) {
    int n = nextStoredFormat(options, buf, size, fmt, locale, args);
    noteStoredFormat(buf, fmt, (const u64 *)(const void *)args);
    return n;
}
#endif

static void noteBuiltText(const char *buf, const char *fmt, const u64 *args, int argCount) {
    if (!drawnText.installed || !buf || !fmt) return;
    int stringArg[MAX_FORMAT_ARGS];
    int slots = scanFormatArgs(fmt, stringArg);
    if (slots < 0 || slots > argCount) return;
    noteStoredText(buf, newStoredText(fmt, args, stringArg, slots, buf));
}
#define BUILT_ARGS_MAX 5
static TextBuildFn nextEliminatedText;
#ifdef __APPLE__
static int eliminatedTextHook(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    u64 args[BUILT_ARGS_MAX];
    memcpy(args, (const void *)ap, sizeof args);
    va_end(ap);
    int n = nextEliminatedText(buf, size, fmt, args[0], args[1], args[2], args[3], args[4]);
    noteBuiltText(buf, fmt, args, BUILT_ARGS_MAX);
    return n;
}
#else
static int eliminatedTextHook(char *buf, u64 size, const char *fmt, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    int n = nextEliminatedText(buf, size, fmt, a1, a2, a3, a4, a5);
    u64 args[BUILT_ARGS_MAX] = {a1, a2, a3, a4, a5};
    noteBuiltText(buf, fmt, args, BUILT_ARGS_MAX);
    return n;
}
#endif

static void noteSendMessageText(const char *text) { storedTexts.givenWhole = text; }

static void storedTextInit(void) {
    int format = wrapSite(SITE_STORED_FORMAT, (void *)storedFormatHook, (void **)&nextStoredFormat);
    int eliminated = wrapSite(SITE_ELIMINATED_TEXT, (void *)eliminatedTextHook, (void **)&nextEliminatedText);
    writeResultLog(format && eliminated, "stored_text_installed", "stored_format=%d eliminated_text=%d", format, eliminated);
}
