#pragma once

#include "line_breaks.h"

#define TEXT_WRAP_LOG_TEXTS 20
#define TEXT_WRAP_LOG_BYTES 80

typedef int (*TextWrapFn)(const char *font, float size, int flags, const char *text, float width);
static TextWrapFn gameTextWrap;
static int textWrapsLogged;

static int measureWrapText(void *context, const char *text) {
    int width = 0, height = 0;
    GAME_FN(void (*)(int, const char *, int *, int *), RVA_TEXT_MEASURE)(*(const int *)context, text, &width, &height);
    return width;
}

static int wrapTextLinesAsGame(const char *font, float size, int flags, const char *text, float width) {
    if (!text || !textWrapTakes(text)) return gameTextWrap(font, size, flags, text, width);
#ifndef __APPLE__
    flags = G32(RVA_TEXT_RENDER_FLAGS);
#endif
    int index = GAME_FN(int (*)(const char *, int, int), RVA_OPEN_GUI_FONT)(font, (int)size, flags);
    if (index < 0) return gameTextWrap(font, size, flags, text, width);
    float *widths = (float *)GAME_PTR(RVA_TOOLTIP_LINE_WIDTHS);
    int count = wrapTextLines(text, width, measureWrapText, &index, (char *)GAME_PTR(RVA_TOOLTIP_LINES), TOOLTIP_LINE_SIZE, widths,
                              WRAP_LINES_MAX);
    if (textWrapsLogged < TEXT_WRAP_LOG_TEXTS) {
        float widest = 0;
        for (int i = 0; i < count; i++)
            if (widths[i] > widest) widest = widths[i];
        int shown = (int)strnlen(text, TEXT_WRAP_LOG_BYTES + 1);
        if (shown > TEXT_WRAP_LOG_BYTES)
            for (shown = TEXT_WRAP_LOG_BYTES; shown > 0 && ((unsigned char)text[shown] & 0xc0) == 0x80; shown--) {
            }
        writeLog("text_wrapped", "lines=%d width=%d widest=%d pixels=%d text=\"%.*s\"%s", count, (int)width, (int)widest, (int)size, shown,
                 text, ++textWrapsLogged == TEXT_WRAP_LOG_TEXTS ? " last_logged=1" : "");
    }
    return count;
}

static int textWrapHook(const char *font, float size, int flags, const char *text, float width) {
    const char *shown = lookUpWrappedText(text, __builtin_return_address(0));
    drawnText.wrapping++;
    int count = wrapTextLinesAsGame(font, size, flags, shown, width);
    drawnText.wrapping--;
    noteWrappedLines(count);
    return count;
}

static int textWrapInit(void) {
    if (!checkGameFunction("text_wrap_check", "")) {
        writeLogError("text_wrap_installed", "ok=0 reason=%s", gameCodeReason());
        return 0;
    }
    void *target = gameBase + RVA_TEXT_WRAP;
    int ok = fanpatchHookCreate(target, (void *)textWrapHook, (void **)&gameTextWrap) && fanpatchHookEnable(target, 1);
    writeResultLog(ok, "text_wrap_installed", "ok=%d", ok);
    return ok;
}
