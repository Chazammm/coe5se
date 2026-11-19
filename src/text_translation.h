#pragma once

// ---- Translating what goes into the game's text --------------------------------------------
#ifdef __APPLE__
typedef int (*TextFormatFn)(char *buf, size_t size, const char *fmt, va_list args);
#else
typedef int (*TextFormatFn)(u64 options, char *buf, size_t size, const char *fmt, void *locale, va_list args);
#endif
static TextFormatFn nextTextFormat;

#define MAX_FORMAT_ARGS    32
#define MAX_TRANSLATED_ARGS 8
#define TRANSLATED_ARG_SIZE TRANSLATED_SIZE

static int scanFormatArgs(const char *fmt, int stringArg[MAX_FORMAT_ARGS]) {
    int slots = 0;
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') continue;
        if (*++p == '%') continue;
        while (*p && strchr("-+ #0", *p)) p++;
        if (*p == '*') { if (slots >= MAX_FORMAT_ARGS) return -1; stringArg[slots++] = 0; p++; }
        else while (*p >= '0' && *p <= '9') p++;
        if (*p == '.') {
            p++;
            if (*p == '*') { if (slots >= MAX_FORMAT_ARGS) return -1; stringArg[slots++] = 0; p++; }
            else while (*p >= '0' && *p <= '9') p++;
        }
        int wide = 0;
        if (!strncmp(p, "I64", 3) || !strncmp(p, "I32", 3)) p += 3;
        else if (*p == 'l' || *p == 'w') { wide = 1; p++; if (*p == 'l') p++; }
        else if (*p == 'h') { p++; if (*p == 'h') p++; }
        else if (*p && strchr("LIjzt", *p)) p++;
        if (!*p || !strchr("cCdiouxXeEfFgGaApsSZ", *p) || slots >= MAX_FORMAT_ARGS) return -1;
        stringArg[slots++] = *p == 's' && !wide;
    }
    return slots;
}

static uint64_t drawnTextHash(const char *text, size_t size);
static const char *storedTextShown(const char *text);

static struct { const char *at; uint64_t hash; } argLookedUp;

static int argWasLookedUp(const char *s) { return s == argLookedUp.at && drawnTextHash(s, (size_t)-1) == argLookedUp.hash; }

static int translateStringArgs(const int *stringArg, int slots, u64 *args,
                               char translated[MAX_TRANSLATED_ARGS][TRANSLATED_ARG_SIZE]) {
    int all = 1;
    for (int k = 0, used = 0; k < slots; k++) {
        const char *s = (const char *)args[k];
        if (!stringArg[k] || !s || argWasLookedUp(s)) continue;
        if (used >= MAX_TRANSLATED_ARGS || strlen(s) >= TRANSLATED_ARG_SIZE - 1) {
            all = 0;
            continue;
        }
        const char *stored = storedTextShown(s);
        if (stored) {
            snprintf(translated[used], TRANSLATED_ARG_SIZE, "%s", stored);
            endAtWholeLetter(translated[used], TRANSLATED_ARG_SIZE);
            args[k] = (u64)translated[used++];
            continue;
        }
        translateText(s, translated[used], TRANSLATED_ARG_SIZE);
        if (strcmp(translated[used], s)) args[k] = (u64)translated[used++];
    }
    return all;
}

static void noteTextLookedUp(const char *text, size_t size);
static int textWasHandedBack(const char *fmt);

static int noteFormattedText(int n, char *buf, size_t size, const char *fmt, int argsLookedUp) {
    if (buf && size) endAtWholeLetter(buf, size);
    if (argsLookedUp && buf && size && !textWasHandedBack(fmt)) noteTextLookedUp(buf, size);
    return n;
}

static char translatedArgs[MAX_TRANSLATED_ARGS][TRANSLATED_ARG_SIZE];
static int busy;

#ifdef __APPLE__
_Static_assert(sizeof(va_list) == sizeof(void *), "Apple arm64's va_list points at the 8-byte argument slots");
static int textFormatHook(char *buf, size_t size, const char *fmt, va_list args) {
    int stringArg[MAX_FORMAT_ARGS];
    int slots = fmt && !busy && !textArgsLookedUp ? scanFormatArgs(fmt, stringArg) : -1;
    if (slots <= 0) return noteFormattedText(nextTextFormat(buf, size, fmt, args), buf, size, fmt, slots == 0 || textArgsLookedUp);
    u64 copy[MAX_FORMAT_ARGS];
    memcpy(copy, (const void *)args, slots * sizeof copy[0]);
    busy = 1;
    int all = translateStringArgs(stringArg, slots, copy, translatedArgs);
    busy = 0;
    return noteFormattedText(nextTextFormat(buf, size, fmt, (va_list)(void *)copy), buf, size, fmt, all);
}
#else
static int textFormatHook(u64 options, char *buf, size_t size, const char *fmt, void *locale, va_list args) {
    int stringArg[MAX_FORMAT_ARGS];
    int slots = fmt && !busy && !textArgsLookedUp ? scanFormatArgs(fmt, stringArg) : -1;
    if (slots <= 0) return noteFormattedText(nextTextFormat(options, buf, size, fmt, locale, args), buf, size, fmt, slots == 0 || textArgsLookedUp);
    u64 copy[MAX_FORMAT_ARGS];
    memcpy(copy, args, slots * sizeof copy[0]);
    busy = 1;
    int all = translateStringArgs(stringArg, slots, copy, translatedArgs);
    busy = 0;
    return noteFormattedText(nextTextFormat(options, buf, size, fmt, locale, (va_list)copy), buf, size, fmt, all);
}
#endif

#define MAX_BUILD_ARGS 5
static int textLookedUp(const char *text);

static void translateBuildFormat(const char *fmt, char *out, int size) {
    if (textLookedUp(fmt)) snprintf(out, (size_t)size, "%s", fmt);
    else translateText(fmt, out, size);
}
#ifdef __APPLE__
typedef int (*TextBuildFn)(char *buf, size_t size, const char *fmt, ...);
static TextBuildFn nextTextBuild;

static int textBuildHook(char *buf, size_t size, const char *fmt, ...) {
    static char translatedFormat[TRANSLATED_ARG_SIZE];
    int stringArg[MAX_FORMAT_ARGS];
    va_list ap;
    va_start(ap, fmt);
    int slots = fmt && !busy ? scanFormatArgs(fmt, stringArg) : -1;
    if (slots < 0 || slots > MAX_BUILD_ARGS || strlen(fmt) >= TRANSLATED_ARG_SIZE - 1) {
        int n = vsnprintf(buf, size, fmt, ap);
        va_end(ap);
        return n;
    }
    u64 args[MAX_BUILD_ARGS];
    memcpy(args, (const void *)ap, sizeof args);
    va_end(ap);
    busy = 1;
    translateBuildFormat(fmt, translatedFormat, sizeof translatedFormat);
    int all = translateStringArgs(stringArg, slots, args, translatedArgs);
    busy = 0;
    return noteFormattedText(nextTextBuild(buf, size, translatedFormat, args[0], args[1], args[2], args[3], args[4]), buf, size, translatedFormat, all);
}
#else
typedef int (*TextBuildFn)(char *buf, u64 size, const char *fmt, u64, u64, u64, u64, u64);
static TextBuildFn nextTextBuild;

static int textBuildHook(char *buf, u64 size, const char *fmt, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    static char translatedFormat[TRANSLATED_ARG_SIZE];
    int stringArg[MAX_FORMAT_ARGS];
    int slots = fmt && !busy ? scanFormatArgs(fmt, stringArg) : -1;
    if (slots < 0 || slots > MAX_BUILD_ARGS || strlen(fmt) >= TRANSLATED_ARG_SIZE - 1)
        return nextTextBuild(buf, size, fmt, a1, a2, a3, a4, a5);
    u64 args[MAX_BUILD_ARGS] = {a1, a2, a3, a4, a5};
    busy = 1;
    translateBuildFormat(fmt, translatedFormat, sizeof translatedFormat);
    int all = translateStringArgs(stringArg, slots, args, translatedArgs);
    busy = 0;
    return noteFormattedText(nextTextBuild(buf, size, translatedFormat, args[0], args[1], args[2], args[3], args[4]), buf, size, translatedFormat, all);
}
#endif
