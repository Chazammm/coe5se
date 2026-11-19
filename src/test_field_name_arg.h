#pragma once
#include <stddef.h>

// The commander's native 48-byte field includes its terminator.
#define TEST_FIELD_NAME_BYTES 47
static const char *testFieldNameProblem(const unsigned char *text, size_t length) {
    if (length > TEST_FIELD_NAME_BYTES) return "name_too_long";
    size_t at = 0; int visible = 0;
    while (at < length) {
        unsigned cp = text[at++], minimum = 0; int tail = 0;
        if (cp < 0x80) {
            if (cp < 0x20 || cp == 0x7f) return "name_control";
            if (cp == '%' || cp == ',') return "name_format_character";
        } else {
            if (cp >= 0xc2 && cp <= 0xdf) cp &= 0x1f, minimum = 0x80, tail = 1;
            else if (cp >= 0xe0 && cp <= 0xef) cp &= 0xf, minimum = 0x800, tail = 2;
            else if (cp >= 0xf0 && cp <= 0xf4) cp &= 7, minimum = 0x10000, tail = 3;
            else return "name_utf8";
            if (length - at < (size_t)tail) return "name_utf8";
            for (int i = 0; i < tail; i++) {
                if ((text[at] & 0xc0) != 0x80) return "name_utf8";
                cp = (cp << 6) | (text[at++] & 0x3f);
            }
            if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return "name_utf8";
            if ((cp >= 0x80 && cp < 0xa0) || cp == 0x2028 || cp == 0x2029 ||
                (cp >= 0x202a && cp <= 0x202e) || (cp >= 0x2066 && cp <= 0x2069)) return "name_control";
        }
        if (cp != ' ' && cp != 0xa0) visible = 1;
    }
    return length && !visible ? "name_whitespace" : NULL;
}
