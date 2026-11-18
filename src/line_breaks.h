#pragma once

#include "font_fallback_layout.h"

#define WRAP_LINES_MAX   1000
#define WRAP_SHORT_BYTES 200
#define WRAP_TEXT_BYTES  500

typedef enum {
    LB_AL,
    LB_ID,
    LB_OP,
    LB_CL,
    LB_NS,
    LB_EX,
    LB_IN,
    LB_QU,
    LB_PR,
    LB_PO,
    LB_BA,
    LB_ZW,
    LB_ZWJ,
    LB_CM,
} LineBreakClass;

static LineBreakClass lineBreakClass(uint32_t code) {
    static const struct { uint32_t first, last; unsigned char kind; } RANGES[] = {
        {0x0021, 0x0021, LB_EX}, {0x0022, 0x0022, LB_QU}, {0x0024, 0x0024, LB_PR}, {0x0025, 0x0025, LB_PO},
        {0x0027, 0x0027, LB_QU}, {0x0028, 0x0028, LB_OP}, {0x0029, 0x0029, LB_CL}, {0x002c, 0x002c, LB_CL},
        {0x002d, 0x002d, LB_NS}, {0x002e, 0x002e, LB_CL}, {0x003a, 0x003b, LB_CL}, {0x003f, 0x003f, LB_EX},
        {0x005b, 0x005b, LB_OP}, {0x005d, 0x005d, LB_CL}, {0x007b, 0x007b, LB_OP}, {0x007d, 0x007d, LB_CL},
        {0x00a2, 0x00a2, LB_PO}, {0x00a3, 0x00a5, LB_PR}, {0x00ab, 0x00ab, LB_QU}, {0x00b0, 0x00b0, LB_PO},
        {0x00bb, 0x00bb, LB_QU}, {0x2010, 0x2010, LB_NS}, {0x2013, 0x2013, LB_NS}, {0x2018, 0x2019, LB_QU},
        {0x201c, 0x201d, LB_QU}, {0x2025, 0x2026, LB_IN}, {0x2030, 0x2033, LB_PO}, {0x203c, 0x203d, LB_NS},
        {0x2047, 0x2049, LB_NS}, {0x20ac, 0x20ac, LB_PR}, {0x2103, 0x2103, LB_PO},
        {0x2e80, 0x2fff, LB_ID},
        {0x3000, 0x3000, LB_BA}, {0x3001, 0x3002, LB_CL}, {0x3003, 0x3004, LB_ID}, {0x3005, 0x3005, LB_NS},
        {0x3006, 0x3007, LB_ID}, {0x3008, 0x3008, LB_OP}, {0x3009, 0x3009, LB_CL}, {0x300a, 0x300a, LB_OP},
        {0x300b, 0x300b, LB_CL}, {0x300c, 0x300c, LB_OP}, {0x300d, 0x300d, LB_CL}, {0x300e, 0x300e, LB_OP},
        {0x300f, 0x300f, LB_CL}, {0x3010, 0x3010, LB_OP}, {0x3011, 0x3011, LB_CL}, {0x3012, 0x3013, LB_ID},
        {0x3014, 0x3014, LB_OP}, {0x3015, 0x3015, LB_CL}, {0x3016, 0x3016, LB_OP}, {0x3017, 0x3017, LB_CL},
        {0x3018, 0x3018, LB_OP}, {0x3019, 0x3019, LB_CL}, {0x301a, 0x301a, LB_OP}, {0x301b, 0x301b, LB_CL},
        {0x301c, 0x301c, LB_NS}, {0x301d, 0x301d, LB_OP}, {0x301e, 0x301f, LB_CL}, {0x3020, 0x303a, LB_ID},
        {0x303b, 0x303b, LB_NS}, {0x303c, 0x3040, LB_ID}, {0x3041, 0x3041, LB_NS}, {0x3042, 0x3042, LB_ID},
        {0x3043, 0x3043, LB_NS}, {0x3044, 0x3044, LB_ID}, {0x3045, 0x3045, LB_NS}, {0x3046, 0x3046, LB_ID},
        {0x3047, 0x3047, LB_NS}, {0x3048, 0x3048, LB_ID}, {0x3049, 0x3049, LB_NS}, {0x304a, 0x3062, LB_ID},
        {0x3063, 0x3063, LB_NS}, {0x3064, 0x3082, LB_ID}, {0x3083, 0x3083, LB_NS}, {0x3084, 0x3084, LB_ID},
        {0x3085, 0x3085, LB_NS}, {0x3086, 0x3086, LB_ID}, {0x3087, 0x3087, LB_NS}, {0x3088, 0x308d, LB_ID},
        {0x308e, 0x308e, LB_NS}, {0x308f, 0x3094, LB_ID}, {0x3095, 0x3096, LB_NS}, {0x3097, 0x3098, LB_ID},
        {0x3099, 0x309a, LB_CM}, {0x309b, 0x309e, LB_NS}, {0x309f, 0x309f, LB_ID}, {0x30a0, 0x30a1, LB_NS},
        {0x30a2, 0x30a2, LB_ID}, {0x30a3, 0x30a3, LB_NS}, {0x30a4, 0x30a4, LB_ID}, {0x30a5, 0x30a5, LB_NS},
        {0x30a6, 0x30a6, LB_ID}, {0x30a7, 0x30a7, LB_NS}, {0x30a8, 0x30a8, LB_ID}, {0x30a9, 0x30a9, LB_NS},
        {0x30aa, 0x30c2, LB_ID}, {0x30c3, 0x30c3, LB_NS}, {0x30c4, 0x30e2, LB_ID}, {0x30e3, 0x30e3, LB_NS},
        {0x30e4, 0x30e4, LB_ID}, {0x30e5, 0x30e5, LB_NS}, {0x30e6, 0x30e6, LB_ID}, {0x30e7, 0x30e7, LB_NS},
        {0x30e8, 0x30ed, LB_ID}, {0x30ee, 0x30ee, LB_NS}, {0x30ef, 0x30f4, LB_ID}, {0x30f5, 0x30f6, LB_NS},
        {0x30f7, 0x30fa, LB_ID}, {0x30fb, 0x30fe, LB_NS}, {0x30ff, 0x312f, LB_ID},
        {0x3190, 0x31ef, LB_ID}, {0x31f0, 0x31ff, LB_NS}, {0x3200, 0x4dbf, LB_ID}, {0x4e00, 0x9fff, LB_ID},
        {0xa000, 0xa4cf, LB_ID}, {0xf900, 0xfaff, LB_ID}, {0xfe10, 0xfe10, LB_CL}, {0xfe11, 0xfe12, LB_CL},
        {0xfe13, 0xfe14, LB_CL}, {0xfe15, 0xfe16, LB_EX}, {0xfe17, 0xfe17, LB_OP}, {0xfe18, 0xfe18, LB_CL},
        {0xfe19, 0xfe19, LB_IN}, {0xfe30, 0xfe34, LB_ID}, {0xfe35, 0xfe35, LB_OP}, {0xfe36, 0xfe36, LB_CL},
        {0xfe37, 0xfe37, LB_OP}, {0xfe38, 0xfe38, LB_CL}, {0xfe39, 0xfe39, LB_OP}, {0xfe3a, 0xfe3a, LB_CL},
        {0xfe3b, 0xfe3b, LB_OP}, {0xfe3c, 0xfe3c, LB_CL}, {0xfe3d, 0xfe3d, LB_OP}, {0xfe3e, 0xfe3e, LB_CL},
        {0xfe3f, 0xfe3f, LB_OP}, {0xfe40, 0xfe40, LB_CL}, {0xfe41, 0xfe41, LB_OP}, {0xfe42, 0xfe42, LB_CL},
        {0xfe43, 0xfe43, LB_OP}, {0xfe44, 0xfe44, LB_CL}, {0xfe47, 0xfe47, LB_OP}, {0xfe48, 0xfe48, LB_CL},
        {0xfe50, 0xfe50, LB_CL}, {0xfe52, 0xfe52, LB_CL}, {0xfe54, 0xfe55, LB_NS}, {0xfe56, 0xfe57, LB_EX},
        {0xfe59, 0xfe59, LB_OP}, {0xfe5a, 0xfe5a, LB_CL}, {0xfe5b, 0xfe5b, LB_OP}, {0xfe5c, 0xfe5c, LB_CL},
        {0xfe5d, 0xfe5d, LB_OP}, {0xfe5e, 0xfe5e, LB_CL}, {0xff01, 0xff01, LB_EX}, {0xff02, 0xff07, LB_ID},
        {0xff08, 0xff08, LB_OP}, {0xff09, 0xff09, LB_CL}, {0xff0a, 0xff0b, LB_ID}, {0xff0c, 0xff0c, LB_CL},
        {0xff0d, 0xff0d, LB_ID}, {0xff0e, 0xff0e, LB_CL}, {0xff0f, 0xff19, LB_ID}, {0xff1a, 0xff1b, LB_NS},
        {0xff1c, 0xff1e, LB_ID}, {0xff1f, 0xff1f, LB_EX}, {0xff20, 0xff3a, LB_ID}, {0xff3b, 0xff3b, LB_OP},
        {0xff3c, 0xff3c, LB_ID}, {0xff3d, 0xff3d, LB_CL}, {0xff3e, 0xff5a, LB_ID}, {0xff5b, 0xff5b, LB_OP},
        {0xff5c, 0xff5c, LB_ID}, {0xff5d, 0xff5d, LB_CL}, {0xff5e, 0xff5e, LB_ID}, {0xff5f, 0xff5f, LB_OP},
        {0xff60, 0xff61, LB_CL}, {0xff62, 0xff62, LB_OP}, {0xff63, 0xff64, LB_CL}, {0xff65, 0xff65, LB_NS},
        {0xff66, 0xff66, LB_ID}, {0xff67, 0xff70, LB_NS}, {0xff71, 0xff9d, LB_ID}, {0xff9e, 0xff9f, LB_NS},
        {0xffe0, 0xffe0, LB_PO}, {0xffe1, 0xffe1, LB_PR}, {0xffe2, 0xffe4, LB_ID}, {0xffe5, 0xffe6, LB_PR},
        {0x1b000, 0x1b16f, LB_ID}, {0x20000, 0x3fffd, LB_ID},
    };
    if (code == 0x200b) return LB_ZW;
    if (code == 0x200d) return LB_ZWJ;
    if (fallbackJoinsPrevious(code)) return LB_CM;
    size_t lo = 0, hi = sizeof RANGES / sizeof RANGES[0];
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (code < RANGES[mid].first) hi = mid;
        else if (code > RANGES[mid].last) lo = mid + 1;
        else return (LineBreakClass)RANGES[mid].kind;
    }
    return LB_AL;
}

static int lineBreakSaLetter(uint32_t code) {
    return (code >= 0x0e00 && code <= 0x0eff) || (code >= 0x1000 && code <= 0x109f) || (code >= 0x1780 && code <= 0x17ff) ||
           (code >= 0x19e0 && code <= 0x19ff) || (code >= 0x1a20 && code <= 0x1aaf) || (code >= 0xa9e0 && code <= 0xa9ff) ||
           (code >= 0xaa60 && code <= 0xaadf);
}

static int lineBreakTakesText(uint32_t code) {
    if (lineBreakSaLetter(code)) return 1;
    if (code < 0x2e80) return 0;
    LineBreakClass kind = lineBreakClass(code);
    return kind != LB_AL && kind != LB_CM && kind != LB_ZW && kind != LB_ZWJ;
}

static int lineBreakAllowed(uint32_t before, uint32_t after) {
    LineBreakClass a = lineBreakClass(before), b = lineBreakClass(after);
    if (b == LB_CM || b == LB_ZWJ || a == LB_ZWJ || b == LB_ZW) return 0;
    if (a == LB_ZW) return 1;
    if (a == LB_QU || b == LB_QU || a == LB_OP) return 0;
    if (b == LB_CL || b == LB_NS || b == LB_EX || b == LB_IN || b == LB_BA) return 0;
    if ((a == LB_PR && b == LB_ID) || (a == LB_ID && b == LB_PO)) return 0;
    int wideBefore = a == LB_ID || a == LB_BA || ((a == LB_CL || a == LB_NS || a == LB_EX || a == LB_IN) && before >= 0x2e80);
    int wideAfter = b == LB_ID || (b == LB_OP && after >= 0x2e80);
    return wideBefore || wideAfter;
}

static int lineBreakJoinsPrevious(uint32_t before, uint32_t code) {
    static const uint32_t VIRAMAS[] = {0x094d, 0x09cd, 0x0a4d, 0x0acd, 0x0b4d, 0x0bcd, 0x0c4d, 0x0ccd, 0x0d4d, 0x0dca, 0x1039, 0x17d2};
    if (before == 0x200d || fallbackJoinsPrevious(code)) return 1;
    for (size_t i = 0; i < sizeof VIRAMAS / sizeof VIRAMAS[0]; i++)
        if (before == VIRAMAS[i]) return 1;
    if (code >= 0x0900 && code <= 0x0dff) {
        uint32_t at = code & 0x7f;
        return at <= 0x03 || at == 0x3c || (at >= 0x3e && at <= 0x4d) || (at >= 0x55 && at <= 0x57) || at == 0x62 || at == 0x63 ||
               (code >= 0x0dca && code <= 0x0ddf);
    }
    return code == 0x0e31 || (code >= 0x0e33 && code <= 0x0e3a) || (code >= 0x0e47 && code <= 0x0e4e) || code == 0x0eb1 ||
           (code >= 0x0eb3 && code <= 0x0ebc) || (code >= 0x0ec8 && code <= 0x0ece) || (code >= 0x102b && code <= 0x103e) ||
           (code >= 0x1056 && code <= 0x1059) || (code >= 0x105e && code <= 0x1060) || (code >= 0x1062 && code <= 0x1064) ||
           (code >= 0x1067 && code <= 0x106d) || (code >= 0x1071 && code <= 0x1074) || (code >= 0x1082 && code <= 0x108d) ||
           code == 0x108f || (code >= 0x109a && code <= 0x109d) || (code >= 0x17b4 && code <= 0x17d3) || code == 0x17dd ||
           (code >= 0x1160 && code <= 0x11ff) || code == 0x3099 || code == 0x309a;
}

static int textWrapTakes(const char *text) {
    const unsigned char *s = (const unsigned char *)text;
    size_t n = strlen(text);
    for (size_t at = 0; at < n;) {
        if (s[at] < 0x80) {
            at++;
            continue;
        }
        uint32_t code;
        at += (size_t)fallbackReadLetter(s + at, n - at, &code);
        if (lineBreakTakesText(code)) return 1;
    }
    return 0;
}

typedef int (*WrapMeasure)(void *context, const char *text);

typedef struct {
    int bytes, resume;
} WrapPoint;

#define WRAP_ENDS_AT_NEWLINE 1
#define WRAP_ENDS_TEXT       2
static int readWrapLine(const char *text, int textLength, int at, int keepLeadingSpace, char *line, int room, int *from,
                        WrapPoint *points, int *bytes, int *ended) {
    const unsigned char *s = (const unsigned char *)text;
    int n = 0, count = 0, p = at, letterBefore = 0;
    uint32_t before = 0;
    *ended = 0;
    if (!keepLeadingSpace)
        while (s[p] == ' ' || s[p] == '\r') p++;
    for (;;) {
        unsigned char c = s[p];
        if (c == 0 || c == '\n') {
            points[count++] = (WrapPoint){n, c ? p + 1 : p};
            *ended = c ? WRAP_ENDS_AT_NEWLINE : WRAP_ENDS_TEXT;
            break;
        }
        if (c == ' ' || c == '\r') {
            int q = p;
            while (s[q] == ' ' || s[q] == '\r') q++;
            if (s[q] == 0 || s[q] == '\n') {
                p = q;
                continue;
            }
            if (n > 0) points[count++] = (WrapPoint){n, q};
            if (n + 1 > room) break;
            from[n] = p;
            line[n++] = ' ';
            letterBefore = 0;
            p = q;
            continue;
        }
        uint32_t code;
        int length = fallbackReadLetter(s + p, (size_t)(textLength - p), &code);
        if (letterBefore && lineBreakAllowed(before, code)) points[count++] = (WrapPoint){n, p};
        if (n + length > room) break;
        for (int k = 0; k < length; k++) from[n + k] = p + k, line[n + k] = (char)s[p + k];
        n += length;
        p += length;
        before = code;
        letterBefore = 1;
    }
    from[n] = p;
    line[n] = '\0';
    *bytes = n;
    return count;
}

static int measureWrapPrefix(char *line, int bytes, WrapMeasure measure, void *context) {
    char kept = line[bytes];
    line[bytes] = '\0';
    int width = measure(context, line);
    line[bytes] = kept;
    return width;
}

static int fitWrapClusters(char *line, int limit, float width, WrapMeasure measure, void *context, int *measured) {
    const unsigned char *u = (const unsigned char *)line;
    int ends[WRAP_TEXT_BYTES], cuts = 0;
    uint32_t before = 0;
    for (int k = 0; k < limit;) {
        uint32_t code;
        int length = fallbackReadLetter(u + k, (size_t)(limit - k), &code);
        if (k > 0 && !lineBreakJoinsPrevious(before, code)) ends[cuts++] = k;
        before = code;
        k += length;
    }
    ends[cuts++] = limit;
    int pick = 0;
    *measured = measureWrapPrefix(line, ends[0], measure, context);
    for (int lo = 1, hi = cuts - 1; lo <= hi;) {
        int mid = (lo + hi) / 2, w = measureWrapPrefix(line, ends[mid], measure, context);
        if ((float)w <= width) pick = mid, *measured = w, lo = mid + 1;
        else hi = mid - 1;
    }
    return ends[pick];
}

static int wrapTextLines(const char *text, float width, WrapMeasure measure, void *context, char *lines, int lineSize, float *widths,
                         int maxLines) {
    lines[0] = '\0';
    widths[0] = 0;
    if (!text || !text[0]) return 0;
    int textLength = (int)strlen(text);
    if (textLength <= WRAP_SHORT_BYTES && !strchr(text, '\n')) {
        int whole = measure(context, text);
        if ((float)whole <= width) {
            snprintf(lines, (size_t)lineSize, "%s", text);
            widths[0] = (float)whole;
            return 1;
        }
    }
    int room = lineSize - 1 < WRAP_TEXT_BYTES - 1 ? lineSize - 1 : WRAP_TEXT_BYTES - 1;
    char line[WRAP_TEXT_BYTES];
    int from[WRAP_TEXT_BYTES];
    WrapPoint points[WRAP_TEXT_BYTES + 1];
    int count = 0, at = 0;
    while (count < maxLines) {
        int bytes, ended, total = readWrapLine(text, textLength, at, at == 0, line, room, from, points, &bytes, &ended);
        int best = -1, lineWidth = 0;
        for (int lo = 0, hi = total - 1; lo <= hi;) {
            int mid = (lo + hi) / 2, w = measureWrapPrefix(line, points[mid].bytes, measure, context);
            if ((float)w <= width) best = mid, lineWidth = w, lo = mid + 1;
            else hi = mid - 1;
        }
        int lineBytes, next, endsText;
        if (best >= 0) {
            lineBytes = points[best].bytes, next = points[best].resume;
            endsText = ended == WRAP_ENDS_TEXT && best == total - 1;
        } else {
            int limit = total ? points[0].bytes : bytes;
            lineBytes = fitWrapClusters(line, limit, width, measure, context, &lineWidth);
            int atPoint = total && lineBytes == points[0].bytes;
            next = atPoint ? points[0].resume : from[lineBytes];
            endsText = atPoint && ended == WRAP_ENDS_TEXT && total == 1;
        }
        char *out = lines + (size_t)count * (size_t)lineSize;
        memcpy(out, line, (size_t)lineBytes);
        out[lineBytes] = '\0';
        widths[count++] = (float)lineWidth;
        if (endsText || next <= at) break;
        at = next;
    }
    return count;
}
