#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define FALLBACK_MAX_RUNS   64
#define FALLBACK_MAX_SIDE   16384
#define FALLBACK_BAD_BYTE   0xfffd

static int fallbackReadLetter(const unsigned char *s, size_t n, uint32_t *code) {
    unsigned char c = s[0];
    int length = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 0;
    *code = FALLBACK_BAD_BYTE;
    if (length == 1) *code = c;
    if (length <= 1 || (size_t)length > n) return 1;
    uint32_t value = c & (0x7f >> length);
    for (int i = 1; i < length; i++) {
        if ((s[i] & 0xc0) != 0x80) return 1;
        value = value << 6 | (s[i] & 0x3f);
    }
    static const uint32_t LEAST[5] = {0, 0, 0x80, 0x800, 0x10000};
    if (value < LEAST[length] || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return 1;
    *code = value;
    return length;
}

static int fallbackJoinsPrevious(uint32_t code) {
    return (code >= 0x300 && code <= 0x36f) || (code >= 0x1ab0 && code <= 0x1aff) || (code >= 0x1dc0 && code <= 0x1dff) ||
           (code >= 0x20d0 && code <= 0x20ff) || (code >= 0xfe20 && code <= 0xfe2f) || (code >= 0xfe00 && code <= 0xfe0f) ||
           code == 0x200d || (code >= 0x1f3fb && code <= 0x1f3ff) || (code >= 0xe0020 && code <= 0xe007f) ||
           (code >= 0xe0100 && code <= 0xe01ef);
}

static int fallbackDrawsNothing(uint32_t code) {
    return (code >= 0x200b && code <= 0x200f) || (code >= 0x2060 && code <= 0x2064) || (code >= 0xfe00 && code <= 0xfe0f) ||
           code == 0xfeff || (code >= 0xe0000 && code <= 0xe0fff);
}

static const char *fallbackShapedScript(uint32_t code) {
    static const struct { uint32_t first, last; char tag[5]; } SCRIPTS[] = {
        {0x0900, 0x097f, "Deva"}, {0x0980, 0x09ff, "Beng"}, {0x0a00, 0x0a7f, "Guru"}, {0x0a80, 0x0aff, "Gujr"},
        {0x0b00, 0x0b7f, "Orya"}, {0x0b80, 0x0bff, "Taml"}, {0x0c00, 0x0c7f, "Telu"}, {0x0c80, 0x0cff, "Knda"},
        {0x0d00, 0x0d7f, "Mlym"}, {0x0d80, 0x0dff, "Sinh"}, {0x0e00, 0x0e7f, "Thai"}, {0x0e80, 0x0eff, "Laoo"},
        {0x0f00, 0x0fff, "Tibt"}, {0x1000, 0x109f, "Mymr"}, {0x1100, 0x11ff, "Hang"}, {0x1780, 0x17ff, "Khmr"},
        {0x1b00, 0x1b7f, "Bali"}, {0xa8e0, 0xa8ff, "Deva"}, {0xa960, 0xa97f, "Hang"}, {0xa980, 0xa9df, "Java"},
        {0xd7b0, 0xd7ff, "Hang"},
    };
    for (size_t i = 0; i < sizeof SCRIPTS / sizeof SCRIPTS[0]; i++)
        if (code >= SCRIPTS[i].first && code <= SCRIPTS[i].last) return SCRIPTS[i].tag;
    return NULL;
}

static const char *fallbackRunScript(const char *text, int length) {
    const unsigned char *s = (const unsigned char *)text;
    for (int at = 0; at < length;) {
        uint32_t code;
        at += fallbackReadLetter(s + at, (size_t)(length - at), &code);
        const char *script = fallbackShapedScript(code);
        if (script) return script;
    }
    return NULL;
}

typedef int (*FallbackHasLetter)(void *context, int font, uint32_t code);

typedef struct {
    int start, length, font;
} FallbackRun;

#define FALLBACK_LEFT_OUT -1

static int fallbackSplitRuns(const char *text, int fonts, FallbackHasLetter has, void *context, FallbackRun *runs, int maxRuns) {
    const unsigned char *s = (const unsigned char *)text;
    size_t n = strlen(text);
    int count = 0;
    uint32_t before = 0;
    for (size_t at = 0; at < n;) {
        uint32_t code;
        int length = fallbackReadLetter(s + at, n - at, &code), font = 0, found = 0;
        const char *script = fallbackShapedScript(code), *scriptBefore = fallbackShapedScript(before);
        int joins = fallbackJoinsPrevious(code) || before == 0x200d || (script && scriptBefore && !strcmp(script, scriptBefore));
        if (count && joins && runs[count - 1].font >= 0 && has(context, runs[count - 1].font, code)) font = runs[count - 1].font, found = 1;
        for (int f = 0; f < fonts && !found; f++)
            if (has(context, f, code)) font = f, found = 1;
        if (!found && fallbackDrawsNothing(code)) font = FALLBACK_LEFT_OUT;
        if (count && runs[count - 1].font == font) runs[count - 1].length += length;
        else {
            if (count == maxRuns) return -1;
            runs[count++] = (FallbackRun){(int)at, length, font};
        }
        before = code;
        at += (size_t)length;
    }
    return count;
}

typedef struct {
    int width, height, rowsAbove, ascent, inkTop, inkBottom;
} FallbackPiece;

static int fallbackRowsAbove(int ascent, int tallestTop) { return tallestTop > ascent ? tallestTop - ascent : 0; }

static void fallbackInkRows(const unsigned char *pixels, int pitch, int width, int height, int *top, int *bottom) {
    *top = -1, *bottom = -2;
    for (int row = 0; row < height; row++) {
        const uint32_t *line = (const uint32_t *)(pixels + (size_t)row * (size_t)pitch);
        for (int col = 0; col < width; col++)
            if (line[col] >> 24) {
                if (*top < 0) *top = row;
                *bottom = row;
                break;
            }
    }
}

static int fallbackJoinLayout(int textAscent, int textHeight, const FallbackPiece *pieces, int count, int *x, int *y, int *width,
                              int *height) {
    int below = textHeight - textAscent, grow = 0;
    long long total = 0;
    for (int i = 0; i < count; i++) {
        if (pieces[i].inkTop > pieces[i].inkBottom) continue;
        int pieceBaseline = pieces[i].rowsAbove + pieces[i].ascent;
        int overAbove = pieceBaseline - pieces[i].inkTop - textAscent, overBelow = pieces[i].inkBottom + 1 - pieceBaseline - below;
        if (overAbove > grow) grow = overAbove;
        if (overBelow > grow) grow = overBelow;
    }
    int baseline = textAscent + grow, rows = textHeight + 2 * grow;
    for (int i = 0; i < count; i++) {
        x[i] = (int)total;
        y[i] = baseline - (pieces[i].rowsAbove + pieces[i].ascent);
        total += pieces[i].width;
    }
    if (total <= 0 || total > FALLBACK_MAX_SIDE || rows <= 0 || rows > FALLBACK_MAX_SIDE) return 0;
    *width = (int)total;
    *height = rows;
    return 1;
}

static uint32_t fallbackClearPixel(uint32_t color) {
    return (color & 0xff) << 16 | (color >> 8 & 0xff) << 8 | (color >> 16 & 0xff);
}

static void fallbackFill(unsigned char *pixels, int pitch, int width, int height, uint32_t pixel) {
    for (int row = 0; row < height; row++) {
        uint32_t *line = (uint32_t *)(pixels + (size_t)row * (size_t)pitch);
        for (int col = 0; col < width; col++) line[col] = pixel;
    }
}

static void fallbackPutPiece(unsigned char *dst, int dstPitch, int dstWidth, int dstHeight, int x, int y, const unsigned char *src,
                             int srcPitch, int srcWidth, int srcHeight) {
    for (int row = 0; row < srcHeight; row++) {
        int to = y + row;
        if (to < 0 || to >= dstHeight) continue;
        const uint32_t *from = (const uint32_t *)(src + (size_t)row * (size_t)srcPitch);
        uint32_t *line = (uint32_t *)(dst + (size_t)to * (size_t)dstPitch);
        for (int col = 0; col < srcWidth; col++) {
            int at = x + col;
            if (at >= 0 && at < dstWidth && from[col] >> 24 > line[at] >> 24) line[at] = from[col];
        }
    }
}
