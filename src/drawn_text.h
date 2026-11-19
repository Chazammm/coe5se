#pragma once

#include "line_breaks.h"

#define LOOKED_UP_SLOTS     8192
#define LOOKED_UP_TURN      4096
#define DRAWN_TEXT_SIZE     1024
#define WRAPPED_TEXT_SIZE   10000
#define LONG_TRANSLATIONS_LOGGED 20

static struct {
    uint64_t slots[2][LOOKED_UP_SLOTS];
    int count[2];
    int current;
    int wrapping;
    int installed;
    const char *handedBack;
    uint64_t handedBackHash;
    int longLogged;
} drawnText;

static uint64_t drawnTextHash(const char *text, size_t size) {
    uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < size && text[i]; i++) h = (h ^ (unsigned char)text[i]) * 1099511628211ull;
    return h ? h : 1;
}

static int lookedUpSlot(int g, uint64_t h) {
    unsigned int at = (unsigned int)(h ^ (h >> 29)) & (LOOKED_UP_SLOTS - 1);
    while (drawnText.slots[g][at] && drawnText.slots[g][at] != h) at = (at + 1) & (LOOKED_UP_SLOTS - 1);
    return (int)at;
}

static void noteLookedUpHash(uint64_t h) {
    int g = drawnText.current, at = lookedUpSlot(g, h);
    if (drawnText.slots[g][at]) return;
    if (drawnText.count[g] >= LOOKED_UP_TURN) {
        g = drawnText.current ^= 1;
        memset(drawnText.slots[g], 0, sizeof drawnText.slots[g]);
        drawnText.count[g] = 0;
        at = lookedUpSlot(g, h);
    }
    drawnText.slots[g][at] = h;
    drawnText.count[g]++;
}

static void noteTextLookedUp(const char *text, size_t size) {
    if (text && text[0]) noteLookedUpHash(drawnTextHash(text, size));
}

static int textLookedUp(const char *text) {
    uint64_t h = drawnTextHash(text, (size_t)-1);
    if (drawnText.slots[drawnText.current][lookedUpSlot(drawnText.current, h)]) return 1;
    int before = drawnText.current ^ 1;
    if (!drawnText.slots[before][lookedUpSlot(before, h)]) return 0;
    noteLookedUpHash(h);
    return 1;
}

static void forgetLookedUpTexts(void) {
    memset(drawnText.slots, 0, sizeof drawnText.slots);
    drawnText.count[0] = drawnText.count[1] = 0;
}

static void drawnTextAroundPass(int level) {
    if (level == 2) forgetLookedUpTexts();
}

static int drawnTextHasLetter(const char *text) {
    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
        if ((unsigned int)((*p | 0x20) - 'a') < 26u || *p >= 0x80) return 1;
    return 0;
}

static const char *modDescriptionTranslation(const char *text);
static const char *storedTextShown(const char *text);
static void noteStoredMessage(const char *text);

static const char *lookUpDrawnText(const char *text, char *out, int size) {
    if (!drawnText.installed || drawnText.wrapping || !text || !text[0]) return text;
    const char *description = modDescriptionTranslation(text);
    if (description) {
        noteTextLookedUp(description, (size_t)-1);
        return description;
    }
    if (G32(RVA_TRANSLATION_COUNT) < 0 || !drawnTextHasLetter(text)) return text;
    const char *stored = storedTextShown(text);
    if (stored) {
        noteTextLookedUp(stored, (size_t)-1);
        return stored;
    }
    if (textLookedUp(text)) return text;
    translateText(text, out, size);
    noteTextLookedUp(out, (size_t)size);
    return strcmp(out, text) ? out : text;
}

static const char *lookUpDrawnLine(const char *text, char *out) {
    const char *line = lookUpDrawnText(text, out, DRAWN_TEXT_SIZE);
    if (!line || strnlen(line, WRAP_TEXT_BYTES) < WRAP_TEXT_BYTES) return line;
    size_t n = textCutLength(line, WRAP_TEXT_BYTES - 1);
    if (line != out) memcpy(out, line, n);
    out[n] = '\0';
    return out;
}

// ---- The hooks ----------------------------------------------------------------------------------
typedef void (*TextDrawLineFn)(int index, float x, float y, const char *text);
static TextDrawLineFn gameTextDrawLine;
static void textDrawLineHook(int index, float x, float y, const char *text) {
    char out[DRAWN_TEXT_SIZE];
    gameTextDrawLine(index, x, y, lookUpDrawnLine(text, out));
}

#ifdef __APPLE__
typedef void (*TextDrawScaledFn)(int index, float x, float y, float size, const char *text);
static TextDrawScaledFn gameTextDrawScaled;
static void textDrawScaledHook(int index, float x, float y, float size, const char *text) {
    char out[DRAWN_TEXT_SIZE];
    gameTextDrawScaled(index, x, y, size, lookUpDrawnLine(text, out));
}
#else
typedef void (*TextDrawScaledFn)(int index, float x, float y, u64 unused, float size, const char *text);
static TextDrawScaledFn gameTextDrawScaled;
static void textDrawScaledHook(int index, float x, float y, u64 unused, float size, const char *text) {
    char out[DRAWN_TEXT_SIZE];
    gameTextDrawScaled(index, x, y, unused, size, lookUpDrawnLine(text, out));
}
#endif

typedef void (*TextMeasureFn)(int index, const char *text, int *w, int *h);
static TextMeasureFn gameTextMeasure;
static void textMeasureHook(int index, const char *text, int *w, int *h) {
    char out[DRAWN_TEXT_SIZE];
    gameTextMeasure(index, lookUpDrawnLine(text, out), w, h);
}

typedef void (*TranslateFormatFn)(const char *text, char *out, int size);
static TranslateFormatFn gameTranslateFormat;
static void translateFormatHook(const char *text, char *out, int size) {
    if (text && out && size > 0 && !textLookupOwn && drawnText.installed && textLookedUp(text)) {
        snprintf(out, (size_t)size, "%s", text);
        endAtWholeLetter(out, (size_t)size);
        return;
    }
    gameTranslateFormat(text, out, size);
    if (!out || size <= 0) return;
    if (text && size > 1 && strnlen(out, (size_t)size) == (size_t)size - 1 && strncmp(out, text, (size_t)size - 1)) {
        int handBack = !textLookupOwn && !strchr(text, '%') && strlen(text) < (size_t)size;
        if (drawnText.longLogged < LONG_TRANSLATIONS_LOGGED) {
            drawnText.longLogged++;
            writeLog("translation_too_long_for_call", "size=%d way=%s text=%.40s", size, handBack ? "looked_up_where_drawn" : "cut_at_letter", text);
        }
        if (handBack) {
            memcpy(out, text, strlen(text) + 1);
            drawnText.handedBack = out;
            drawnText.handedBackHash = drawnTextHash(out, (size_t)-1);
            return;
        }
        endAtWholeLetter(out, (size_t)size);
    }
    noteTextLookedUp(out, (size_t)size);
}

static int textWasHandedBack(const char *fmt) {
    return fmt && fmt == drawnText.handedBack && drawnTextHash(fmt, (size_t)-1) == drawnText.handedBackHash;
}

// ---- The message box's English pass ------------------------------------------------------------
typedef void (*EnglishPassFn)(char *text, int size);
static EnglishPassFn gameEnglishPass;

static char englishPassTranslation[WRAPPED_TEXT_SIZE];
static int translateBeforeEnglishPass(char *text, int size) {
    if (!drawnText.installed || !text || size <= 0 || G32(RVA_TRANSLATION_COUNT) < 0) return 0;
    if (!drawnTextHasLetter(text)) return 0;
    if (textLookedUp(text)) {
        argLookedUp.at = text;
        argLookedUp.hash = drawnTextHash(text, (size_t)-1);
        return 1;
    }
    translateText(text, englishPassTranslation, sizeof englishPassTranslation);
    if (!strcmp(englishPassTranslation, text)) return 0;
    snprintf(text, (size_t)size, "%s", englishPassTranslation);
    argLookedUp.at = text;
    argLookedUp.hash = drawnTextHash(text, (size_t)-1);
    noteTextLookedUp(text, (size_t)size);
    return 1;
}

static void englishPassHook(char *text, int size) {
    void *from = __builtin_return_address(0);
    if (from == (void *)(gameBase + RVA_MESSAGE_BOX_PASS_RETURN) && translateBeforeEnglishPass(text, size)) return;
    gameEnglishPass(text, size);
    if (from == (void *)(gameBase + RVA_ADD_MESSAGE_PASS_RETURN)) noteStoredMessage(text);
}

// ---- Wrapped text -------------------------------------------------------------------------------
static char wrappedTranslation[WRAPPED_TEXT_SIZE];

static const char *lookUpWrappedText(const char *text, const void *caller) {
    if (caller == (const void *)(gameBase + RVA_TEXT_FIELD_WRAP_RETURN)) return text;
    return lookUpDrawnText(text, wrappedTranslation, sizeof wrappedTranslation);
}

static void noteWrappedLines(int count) {
    if (!drawnText.installed) return;
    const char *lines = (const char *)GAME_PTR(RVA_TOOLTIP_LINES);
    for (int i = 0; i < count && i < WRAP_LINES_MAX; i++) noteTextLookedUp(lines + (size_t)i * TOOLTIP_LINE_SIZE, TOOLTIP_LINE_SIZE);
}

// ---- Install ------------------------------------------------------------------------------------
static void drawnTextInit(int textWrapIn) {
    if (!textWrapIn) {
        writeLogError("drawn_text_installed", "ok=0 reason=no_text_wrap_hook");
        return;
    }
    if (!checkGameFunction("drawn_text_check", "")) {
        writeLogError("drawn_text_installed", "ok=0 reason=%s", gameCodeReason());
        return;
    }
    struct { unsigned int rva; void *hook; void **game; } hooks[] = {
        {RVA_TRANSLATE_FORMAT, (void *)translateFormatHook, (void **)&gameTranslateFormat},
        {RVA_TEXT_DRAW_LINE, (void *)textDrawLineHook, (void **)&gameTextDrawLine},
        {RVA_TEXT_DRAW_LINE_SCALED, (void *)textDrawScaledHook, (void **)&gameTextDrawScaled},
        {RVA_TEXT_MEASURE, (void *)textMeasureHook, (void **)&gameTextMeasure},
        {RVA_ENGLISH_TEXT_PASS, (void *)englishPassHook, (void **)&gameEnglishPass},
    };
    int ok = 1;
    for (size_t i = 0; i < sizeof hooks / sizeof hooks[0] && ok; i++)
        ok = fanpatchHookCreate(gameBase + hooks[i].rva, hooks[i].hook, hooks[i].game);
    for (size_t i = 0; i < sizeof hooks / sizeof hooks[0] && ok; i++)
        ok = fanpatchHookEnable(gameBase + hooks[i].rva, 1);
    if (!ok)
        for (size_t i = 0; i < sizeof hooks / sizeof hooks[0]; i++) fanpatchHookEnable(gameBase + hooks[i].rva, 0);
    drawnText.installed = ok;
    if (ok) textAlreadyLookedUp = textLookedUp;
    writeResultLog(ok, "drawn_text_installed", "ok=%d", ok);
}
