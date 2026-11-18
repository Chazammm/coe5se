// @platform-split font_fallback: Windows takes SDL_ttf and SDL from their DLLs' exports and turns the call [rip] into a call to a stub; Mac calls SDL_ttf linked into the game and points the bl at a trampoline
#pragma once

#include "font_fallback_layout.h"

#define FALLBACK_TEXT_BYTES   4096
#define FALLBACK_FONT_SLOTS   32
#define FALLBACK_FILES_MAX    16
#define FALLBACK_CHAIN_MAX    (3 + FALLBACK_FILES_MAX)
#define FALLBACK_CHECK_PIXELS 20
#define FALLBACKFONT_WORD     "fallbackfont"
#define SCRIPT_UNKNOWN        "Zzzz"
#define FALLBACK_LOG_DRAWS    20
#define SDL_FORMAT_ARGB8888   0x16362004u
#define SDL_FORMAT_BITS_AT    0x10

typedef struct {
    uint32_t flags;
    unsigned char *format;
    int w, h, pitch;
    unsigned char *pixels;
} FallbackSurface;

typedef struct {
    void *(*openFont)(const char *path, int pixels);
    void (*closeFont)(void *font);
    void (*setStyle)(void *font, int style);
    void (*setHinting)(void *font, int hinting);
    int (*hasLetter)(void *font, uint32_t code);
    int (*letterMetrics)(void *font, uint32_t code, int *minX, int *maxX, int *minY, int *maxY, int *advance);
    int (*fontHeight)(const void *font);
    int (*fontAscent)(const void *font);
    FallbackSurface *(*render)(void *font, const char *text, uint32_t color);
    FallbackSurface *(*createSurface)(uint32_t flags, int width, int height, int depth, uint32_t format);
    void (*freeSurface)(FallbackSurface *surface);
    int (*setScript)(void *font, const char *script);
} FallbackFunctions;
static FallbackFunctions fallbackCalls;

typedef struct {
    char path[FONT_CACHE_PATH_BYTES];
    int pixels, flags, used;
    unsigned int use;
    void *font;
} FallbackFont;
static FallbackFont fallbackFonts[FALLBACK_FONT_SLOTS];
static int fallbackFontNext, fallbackDrawsLogged;
static unsigned int fallbackFontUse = 1;
static volatile int fallbackBusy;

#ifdef __APPLE__
static const char *findFallbackCalls(void) {
    fallbackCalls = (FallbackFunctions){
        .openFont = GAME_FN(void *(*)(const char *, int), RVA_TTF_OPEN_FONT),
        .closeFont = GAME_FN(void (*)(void *), RVA_TTF_CLOSE_FONT),
        .setStyle = GAME_FN(void (*)(void *, int), RVA_TTF_SET_FONT_STYLE),
        .setHinting = GAME_FN(void (*)(void *, int), RVA_TTF_SET_FONT_HINTING),
        .hasLetter = GAME_FN(int (*)(void *, uint32_t), RVA_TTF_GLYPH_IS_PROVIDED32),
        .letterMetrics = GAME_FN(int (*)(void *, uint32_t, int *, int *, int *, int *, int *), RVA_TTF_GLYPH_METRICS32),
        .fontHeight = GAME_FN(int (*)(const void *), RVA_TTF_FONT_HEIGHT),
        .fontAscent = GAME_FN(int (*)(const void *), RVA_TTF_FONT_ASCENT),
        .render = GAME_FN(FallbackSurface * (*)(void *, const char *, uint32_t), RVA_TTF_RENDER_UTF8_BLENDED),
        .createSurface = (FallbackSurface * (*)(uint32_t, int, int, int, uint32_t))
            SDL_FUNCTION("SDL_CreateRGBSurfaceWithFormat", RVA_SDL_CREATE_SURFACE_FMT_SLOT),
        .freeSurface = GAME_FN(void (*)(FallbackSurface *), RVA_SDL_FREE_SURFACE),
        .setScript = GAME_FN(int (*)(void *, const char *), RVA_TTF_SET_FONT_SCRIPT_NAME),
    };
    return fallbackCalls.createSurface ? NULL : "no_sdl_create_surface";
}
#else
static const char *findFallbackCalls(void) {
    HMODULE ttf = GetModuleHandleA("SDL2_ttf.dll");
    if (!ttf) return "no_sdl2_ttf_dll";
#define TTF_EXPORT(name) (void *)GetProcAddress(ttf, name)
    fallbackCalls = (FallbackFunctions){
        .openFont = TTF_EXPORT("TTF_OpenFont"),
        .closeFont = TTF_EXPORT("TTF_CloseFont"),
        .setStyle = TTF_EXPORT("TTF_SetFontStyle"),
        .setHinting = TTF_EXPORT("TTF_SetFontHinting"),
        .hasLetter = TTF_EXPORT("TTF_GlyphIsProvided32"),
        .letterMetrics = TTF_EXPORT("TTF_GlyphMetrics32"),
        .fontHeight = TTF_EXPORT("TTF_FontHeight"),
        .fontAscent = TTF_EXPORT("TTF_FontAscent"),
        .render = TTF_EXPORT("TTF_RenderUTF8_Blended"),
        .createSurface = SDL_FUNCTION("SDL_CreateRGBSurfaceWithFormat", 0),
        .freeSurface = SDL_FUNCTION("SDL_FreeSurface", 0),
        .setScript = TTF_EXPORT("TTF_SetFontScriptName"),
    };
#undef TTF_EXPORT
    void *const all[] = {fallbackCalls.openFont, fallbackCalls.closeFont, fallbackCalls.setStyle, fallbackCalls.setHinting,
                         fallbackCalls.hasLetter, fallbackCalls.letterMetrics, fallbackCalls.fontHeight, fallbackCalls.fontAscent,
                         fallbackCalls.render, fallbackCalls.createSurface, fallbackCalls.freeSurface};
    for (size_t i = 0; i < sizeof all / sizeof all[0]; i++)
        if (!all[i]) return "missing_export";
    return NULL;
}
#endif

static void *fallbackFont(const char *path, int pixels, int flags) {
    size_t length = strnlen(path, FONT_CACHE_PATH_BYTES);
    if (length == FONT_CACHE_PATH_BYTES) return NULL;
    for (int i = 0; i < FALLBACK_FONT_SLOTS; i++)
        if (fallbackFonts[i].used && fallbackFonts[i].pixels == pixels && fallbackFonts[i].flags == flags &&
            !strcmp(fallbackFonts[i].path, path)) {
            fallbackFonts[i].use = fallbackFontUse;
            return fallbackFonts[i].font;
        }
    for (int tries = 0; tries < FALLBACK_FONT_SLOTS && fallbackFonts[fallbackFontNext].used && fallbackFonts[fallbackFontNext].use == fallbackFontUse; tries++)
        fallbackFontNext = (fallbackFontNext + 1) % FALLBACK_FONT_SLOTS;
    FallbackFont *slot = &fallbackFonts[fallbackFontNext];
    fallbackFontNext = (fallbackFontNext + 1) % FALLBACK_FONT_SLOTS;
    if (slot->font) fallbackCalls.closeFont(slot->font);
    slot->use = fallbackFontUse;
    memcpy(slot->path, path, length + 1);
    slot->pixels = pixels;
    slot->flags = flags;
    slot->used = 1;
    slot->font = fallbackCalls.openFont(path, pixels);
    if (!slot->font) {
        writeLogError("font_fallback_font_failed", "reason=open_failed path=%.200s pixels=%d", path, pixels);
        return NULL;
    }
    fallbackCalls.setHinting(slot->font, 1);
    if (flags & 3) fallbackCalls.setStyle(slot->font, flags & 3);
    return slot->font;
}

#define GAME_FONT_STANDARD 0
#define GAME_FONT_FANCY    1
#define GAME_FONT_TEXTY    2
static const char *gameFontPath(int which) {
    static const unsigned RVAS[3] = {RVA_STR_GUIFONT, RVA_STR_GUIFONT_FANCY, RVA_STR_GUIFONT_TEXTY};
    return (const char *)GAME_PTR(RVAS[which]);
}

static int fallbackChainPaths(const char *textPath, const char *paths[2], const char **name) {
    static const int AFTER[4][2] = {{GAME_FONT_TEXTY, -1}, {GAME_FONT_TEXTY, GAME_FONT_STANDARD}, {GAME_FONT_STANDARD, -1},
                                    {GAME_FONT_STANDARD, GAME_FONT_TEXTY}};
    static const char *const NAMES[4] = {"standard", "fancy", "long_texts", "other"};
    int which = 3;
    for (int f = 0; f < 3 && which == 3; f++)
        if (!strncmp(textPath, gameFontPath(f), FONT_CACHE_PATH_BYTES)) which = f;
    *name = NAMES[which];
    int count = 0;
    for (int k = 0; k < 2 && AFTER[which][k] >= 0; k++) {
        const char *path = gameFontPath(AFTER[which][k]);
        if (strncmp(path, textPath, FONT_CACHE_PATH_BYTES)) paths[count++] = path;
    }
    return count;
}

// ---- Mods' fonts: fallbackfont ---------------------------------------------------------------------
typedef struct {
    char path[MAX_PATH];
    char tag[LANGUAGE_TAG_MAX + 1];
    int mod, inBlock;
    int forPick;
    int failed;
} FallbackFontFile;
static struct {
    FallbackFontFile files[FALLBACK_FILES_MAX];
    int count;
    unsigned int hash;
    unsigned int drawn;
} fallbackFiles = {.hash = 2166136261u, .drawn = 2166136261u};

static int fallbackFileOrder(int order[FALLBACK_FILES_MAX]) {
    int n = 0;
    for (int group = 0; group < 3; group++)
        for (int i = 0; i < fallbackFiles.count; i++) {
            const FallbackFontFile *f = &fallbackFiles.files[i];
            if ((f->forPick ? 0 : f->inBlock ? 1 : 2) == group) order[n++] = i;
        }
    return n;
}

static void hashFallbackFiles(void) {
    int order[FALLBACK_FILES_MAX], n = fallbackFileOrder(order);
    unsigned int h = 2166136261u;
    for (int k = 0; k < n; k++)
        for (const char *c = fallbackFiles.files[order[k]].path; *c; c++) h = (h ^ (unsigned char)*c) * 16777619u;
    fallbackFiles.hash = n ? h ^ (unsigned int)n : 2166136261u;
}

static void fallbackFontsBeforePass(int level) {
    if (level != 2) return;
    for (int i = 0; i < FALLBACK_FONT_SLOTS; i++)
        if (fallbackFonts[i].used && !fallbackFonts[i].font) fallbackFonts[i].used = 0;
    fallbackFiles.count = 0;
    hashFallbackFiles();
}

static void fallbackfontLine(const char *args, int mod, int level) {
    static const char *const ENDINGS[2] = {".ttf", ".otf"};
    static const char *const REASONS[4] = {"bad_font_file", "font_file_not_found", "font_file_wrong_case", "font_file_link"};
    if (level != 2 || mod < 0 || mod > G32(RVA_MOD_LAST_INDEX)) return;
    const char *name = modName(mod);
    int line = G32(RVA_MOD_LINE);
    if (!readingSidecarOf(mod) && !readingPrefixedLine()) {
        refuseC5mLine(FALLBACKFONT_WORD, name, "fallbackfont_in_c5m", "line=%d", line);
        return;
    }
    char file[INCLUDE_FILE_MAX + 2], spec[INCLUDE_FILE_MAX + 4], path[MAX_PATH];
    const char *p = args;
    int length = readLanguageQuoted(&p, file, sizeof file);
    if (length <= 0) {
        refuseC5mLine(FALLBACKFONT_WORD, name, "no_font_file", "line=%d text=%.60s", line, args);
        return;
    }
    if (length > INCLUDE_FILE_MAX) {
        refuseC5mLine(FALLBACKFONT_WORD, name, "bad_font_file", "line=%d file=%.60s max=%d", line, file, INCLUDE_FILE_MAX);
        return;
    }
    char tag[LANGUAGE_TAG_MAX + 2] = "";
    skipSpaces(&p);
    if (*p == '"') {
        const char *at = p;
        int tagLength = readLanguageQuoted(&p, tag, sizeof tag);
        if (tagLength < 0) p = at + strlen(at);
        if (tagLength < 0 || tagLength > LANGUAGE_TAG_MAX || !isLanguageTag(tag)) {
            refuseC5mLine(FALLBACKFONT_WORD, name, "bad_language_tag", "line=%d file=%s tag=%.40s", line, file, tagLength < 0 ? at : tag);
            tag[0] = '\0';
        }
        skipSpaces(&p);
    }
    if (*p != '#' && !isLineEnd(p)) refuseC5mLine(FALLBACKFONT_WORD, name, "text_after_fallbackfont", "line=%d text=%.40s", line, p);
    snprintf(spec, sizeof spec, "./%s", file);
    const char *reason = findModFile(modFolder(mod), spec, ENDINGS, REASONS, path, sizeof path);
    if (reason) {
        refuseC5mLine(FALLBACKFONT_WORD, name, reason, "line=%d file=%s", line, file);
        return;
    }
    int inBlock = languages.open && languages.file == modFileNow();
    for (int i = 0; i < fallbackFiles.count; i++)
        if (!strcmp(fallbackFiles.files[i].path, path) && fallbackFiles.files[i].inBlock == inBlock) {
            refuseC5mLine(FALLBACKFONT_WORD, name, "duplicate_fallbackfont", "line=%d file=%s", line, file);
            return;
        }
    if (fallbackFiles.count >= FALLBACK_FILES_MAX) {
        refuseC5mLine(FALLBACKFONT_WORD, name, "too_many_fallback_fonts", "line=%d file=%s max=%d", line, file, FALLBACK_FILES_MAX);
        return;
    }
    FallbackFontFile *f = &fallbackFiles.files[fallbackFiles.count++];
    snprintf(f->path, sizeof f->path, "%s", path);
    snprintf(f->tag, sizeof f->tag, "%s", tag);
    f->mod = mod, f->inBlock = inBlock, f->failed = 0;
    f->forPick = languageTagMatches(tag, languagePick());
    hashFallbackFiles();
    writeLog("fallbackfont_added", "mod=%s file=%s language=%s tag=%s for_pick=%d", name, file, inBlock ? languages.tag : "-",
             tag[0] ? tag : "-", f->forPick);
}

static void *fallbackFileFont(int i, int pixels, int flags) {
    FallbackFontFile *f = &fallbackFiles.files[i];
    if (f->failed) return NULL;
    void *font = fallbackFont(f->path, pixels, flags);
    if (!font) {
        f->failed = 1;
        writeLogError("fallbackfont_not_opened", "reason=open_failed mod=%s path=%.200s pixels=%d", modName(f->mod), f->path, pixels);
    }
    return font;
}

// ---- A text's chain ------------------------------------------------------------------------------
typedef struct {
    void *fonts[FALLBACK_CHAIN_MAX];
    const char *gamePaths[2];
    int gameCount, fileOrder[FALLBACK_FILES_MAX], fileCount;
    unsigned char asked[FALLBACK_CHAIN_MAX];
    int count, pixels, flags;
} FallbackChain;

static void *chainFont(FallbackChain *chain, int font) {
    if (font == 0 || chain->asked[font]) return chain->fonts[font];
    chain->asked[font] = 1;
    chain->fonts[font] = font <= chain->gameCount ? fallbackFont(chain->gamePaths[font - 1], chain->pixels, chain->flags)
                                                  : fallbackFileFont(chain->fileOrder[font - 1 - chain->gameCount], chain->pixels, chain->flags);
    return chain->fonts[font];
}

static int chainHasLetter(void *context, int font, uint32_t code) {
    void *f = chainFont(context, font);
    return f && fallbackCalls.hasLetter(f, code);
}

static void startFallbackChain(FallbackChain *chain, void *font, const char *textPath, int pixels, int flags, const char **fontName) {
    memset(chain, 0, sizeof *chain);
    chain->fonts[0] = font;
    chain->pixels = pixels, chain->flags = flags;
    chain->gameCount = fallbackChainPaths(textPath, chain->gamePaths, fontName);
    chain->fileCount = fallbackFileOrder(chain->fileOrder);
    chain->count = 1 + chain->gameCount + chain->fileCount;
}

// ---- Letters a chain has --------------------------------------------------------------------------
static int fallbackChainHasLetter(uint32_t code) {
    if (!fallbackCalls.openFont || !fallbackCalls.hasLetter || fallbackBusy) return -1;
    fallbackFontUse++;
    int opened = 0, order[FALLBACK_FILES_MAX], files = fallbackFileOrder(order);
    for (int which = GAME_FONT_STANDARD; which <= GAME_FONT_TEXTY; which++) {
        void *font = fallbackFont(gameFontPath(which), FALLBACK_CHECK_PIXELS, 0);
        if (!font) continue;
        opened = 1;
        if (fallbackCalls.hasLetter(font, code)) return 1;
    }
    for (int k = 0; k < files; k++) {
        void *font = fallbackFileFont(order[k], FALLBACK_CHECK_PIXELS, 0);
        if (font && fallbackCalls.hasLetter(font, code)) return 1;
    }
    return opened ? 0 : -1;
}

static int pickLettersWithoutFont(void) {
    static unsigned int countedLetters, countedFonts;
    static int missing = -1;
    if (missing >= 0 && countedLetters == pickLetters.generation && countedFonts == fallbackFiles.hash) return missing;
    int count = 0;
    for (int i = 0; i < LANGUAGE_LETTER_SLOTS; i++) {
        if (!pickLetters.codes[i]) continue;
        int has = fallbackChainHasLetter(pickLetters.codes[i]);
        if (has < 0) return -1;
        count += !has;
    }
    missing = count, countedLetters = pickLetters.generation, countedFonts = fallbackFiles.hash;
    writeLog("language_letters_checked", "pick=%s letters=%d without_font=%d fonts=%d", languagePick()[0] ? languagePick() : "default",
             pickLetters.count, missing, fallbackFiles.count);
    return missing;
}

// ---- The game's text cache -------------------------------------------------------------------------
static int clearGameTextCache(void) {
    void (*deleteTextures)(int, const unsigned int *) = GL_IMPORT(void (*)(int, const unsigned int *), RVA_FPS_GL_DELETE_TEXTURES_IAT);
    unsigned long long *keys = (unsigned long long *)GAME_PTR(RVA_TEXT_CACHE_KEYS);
    unsigned int *textures = (unsigned int *)GAME_PTR(RVA_TEXT_CACHE_TEXTURES);
    int dropped = 0;
    for (int i = 0; i < TEXT_CACHE_ENTRIES; i++) {
        if (!textures[i]) continue;
        deleteTextures(1, &textures[i]);
        textures[i] = 0;
        keys[i] = 0;
        dropped++;
    }
    return dropped;
}

static int fontHasEveryLetter(void *font, const char *text) {
    const unsigned char *s = (const unsigned char *)text;
    size_t n = strlen(text);
    for (size_t at = 0; at < n;) {
        uint32_t code;
        at += (size_t)fallbackReadLetter(s + at, n - at, &code);
        if (!fallbackCalls.hasLetter(font, code)) return 0;
    }
    return 1;
}

static FallbackSurface *renderFallbackRun(void *font, const char *text, const char *script, uint32_t color, FallbackPiece *piece) {
    if (script && fallbackCalls.setScript) fallbackCalls.setScript(font, script);
    FallbackSurface *surface = fallbackCalls.render(font, text, color);
    if (script && fallbackCalls.setScript) fallbackCalls.setScript(font, SCRIPT_UNKNOWN);
    if (!surface) return NULL;
    int ascent = fallbackCalls.fontAscent(font), tallestTop = 0;
    const unsigned char *s = (const unsigned char *)text;
    size_t n = strlen(text);
    for (size_t at = 0; at < n;) {
        uint32_t code;
        int top = 0;
        at += (size_t)fallbackReadLetter(s + at, n - at, &code);
        if (!fallbackCalls.letterMetrics(font, code, NULL, NULL, NULL, &top, NULL) && top > tallestTop) tallestTop = top;
    }
    *piece = (FallbackPiece){surface->w, surface->h, fallbackRowsAbove(ascent, tallestTop), ascent, -1, -2};
    return surface;
}

static int fallbackSurfaceUsable(const FallbackSurface *surface) {
    return surface && surface->pixels && surface->format && surface->format[SDL_FORMAT_BITS_AT] == 32 && surface->pitch >= surface->w * 4;
}

static const char *gameFontCachePath(int index) { return (const char *)GAME_PTR(RVA_FONT_CACHE_PATHS) + (size_t)index * FONT_CACHE_PATH_BYTES; }

static FallbackSurface *joinFallbackRuns(void *font, const char *text, uint32_t color) {
    if (!text || !text[0] || strlen(text) >= FALLBACK_TEXT_BYTES || fontHasEveryLetter(font, text)) return NULL;
    int index = -1;
    void *const *handles = (void *const *)GAME_PTR(RVA_FONT_HANDLES);
    for (int i = 0; i < FONT_CACHE_ENTRIES && index < 0; i++)
        if (handles[i] == font) index = i;
    if (index < 0) return NULL;
    int pixels = ((const int *)GAME_PTR(RVA_FONT_CACHE_SIZES))[index], flags = ((const int *)GAME_PTR(RVA_FONT_CACHE_FLAGS))[index];
    const char *fontName;
    FallbackChain chain;
    fallbackFontUse++;
    startFallbackChain(&chain, font, gameFontCachePath(index), pixels, flags, &fontName);

    FallbackRun runs[FALLBACK_MAX_RUNS];
    int count = fallbackSplitRuns(text, chain.count, chainHasLetter, &chain, runs, FALLBACK_MAX_RUNS);
    if (count < 1 || (count == 1 && runs[0].font == 0)) return NULL;

    FallbackSurface *pieces[FALLBACK_MAX_RUNS] = {0}, *joined = NULL;
    FallbackPiece sizes[FALLBACK_MAX_RUNS];
    int x[FALLBACK_MAX_RUNS], y[FALLBACK_MAX_RUNS], width = 0, height = 0, made = 0;
    const char *why = NULL;
    char part[FALLBACK_TEXT_BYTES];
    for (int r = 0; r < count && !why; r++) {
        if (runs[r].font == FALLBACK_LEFT_OUT) continue;
        memcpy(part, text + runs[r].start, (size_t)runs[r].length);
        part[runs[r].length] = '\0';
        pieces[made] = renderFallbackRun(chain.fonts[runs[r].font], part, fallbackRunScript(part, runs[r].length), color, &sizes[made]);
        if (!fallbackSurfaceUsable(pieces[made])) why = "run_not_drawn";
        else fallbackInkRows(pieces[made]->pixels, pieces[made]->pitch, pieces[made]->w, pieces[made]->h, &sizes[made].inkTop, &sizes[made].inkBottom);
        made++;
    }
    if (!why && !fallbackJoinLayout(fallbackCalls.fontAscent(font), fallbackCalls.fontHeight(font), sizes, made, x, y, &width, &height))
        why = "bad_size";
    if (!why) {
        joined = fallbackCalls.createSurface(0, width, height, 32, SDL_FORMAT_ARGB8888);
        if (!fallbackSurfaceUsable(joined)) why = "no_surface";
    }
    if (!why) {
        fallbackFill(joined->pixels, joined->pitch, width, height, fallbackClearPixel(color));
        for (int i = 0; i < made; i++)
            fallbackPutPiece(joined->pixels, joined->pitch, width, height, x[i], y[i], pieces[i]->pixels, pieces[i]->pitch, pieces[i]->w,
                             pieces[i]->h);
    }
    for (int i = 0; i < made; i++)
        if (pieces[i]) fallbackCalls.freeSurface(pieces[i]);
    if (why) {
        if (joined) fallbackCalls.freeSurface(joined);
        writeLogError("font_fallback_not_drawn", "reason=%s font=%s pixels=%d runs=%d text=\"%.80s\"", why, fontName, pixels, count, text);
        return NULL;
    }
    if (fallbackDrawsLogged < FALLBACK_LOG_DRAWS) {
        char fonts[FALLBACK_MAX_RUNS * 3 + 1];
        size_t n = 0;
        for (int i = 0; i < count && n + 3 < sizeof fonts; i++) n += (size_t)snprintf(fonts + n, sizeof fonts - n, "%s%d", i ? "," : "", runs[i].font);
        writeLog("font_fallback_drawn", "font=%s pixels=%d runs=%d chain=%s width=%d height=%d text=\"%.80s\"%s", fontName, pixels, count, fonts,
                 width, height, text, ++fallbackDrawsLogged == FALLBACK_LOG_DRAWS ? " last_logged=1" : "");
    }
    return joined;
}

static void clearTextCacheIfFontsChanged(void) {
    if (fallbackFiles.drawn == fallbackFiles.hash || !fallbackCalls.render) return;
    fallbackFiles.drawn = fallbackFiles.hash;
    writeLog("text_cache_cleared", "reason=fallback_fonts_changed fonts=%d texts=%d", fallbackFiles.count, clearGameTextCache());
}

static void fallbackFontsAfterPass(int level) {
    if (level == 2) clearTextCacheIfFontsChanged();
}

static FallbackSurface *fontFallbackRender(void *font, const char *text, uint32_t color) {
    while (__sync_lock_test_and_set(&fallbackBusy, 1)) {
    }
    FallbackSurface *joined = joinFallbackRuns(font, text, color);
    __sync_lock_release(&fallbackBusy);
    if (joined) return joined;
    const char *script = text && fallbackCalls.setScript ? fallbackRunScript(text, (int)strnlen(text, FALLBACK_TEXT_BYTES)) : NULL;
    if (!script) return fallbackCalls.render(font, text, color);
    fallbackCalls.setScript(font, script);
    FallbackSurface *surface = fallbackCalls.render(font, text, color);
    fallbackCalls.setScript(font, SCRIPT_UNKNOWN);
    return surface;
}

#ifdef __APPLE__
static const char *pointRenderCall(void) {
    unsigned char *call = GAME_PTR(RVA_PUT_TEXT_RENDER_CALL);
    if (blTarget(call) != GAME_PTR(RVA_TTF_RENDER_UTF8_BLENDED)) return "not_the_expected_call";
    if (!claimGameCode(RVA_PUT_TEXT_RENDER_CALL, 4, "font_fallback")) return "code_claimed";
    unsigned char *trampoline = newTrampoline((void *)fontFallbackRender);
    if (!trampoline) return "no_padding";
    uint32_t word = blTo(call, trampoline);
    return writeGameCode(call, &word, 4) ? NULL : "code_unwritable";
}
#else
#define FONT_FALLBACK_STUB_SIZE 0x20
static const char *pointRenderCall(void) {
    unsigned char *call = GAME_PTR(RVA_PUT_TEXT_RENDER_CALL);
    int rel;
    memcpy(&rel, call + 2, 4);
    if (call[0] != 0xff || call[1] != 0x15 || call + 6 + rel != GAME_PTR(RVA_TTF_RENDER_UTF8_BLENDED)) return "not_the_expected_call";
    if (!claimGameCode(RVA_PUT_TEXT_RENDER_CALL, 6, "font_fallback")) return "code_claimed";
    unsigned char *stub = allocNear(call);
    if (!stub) return "no_near_page";
    unsigned char code[FONT_FALLBACK_STUB_SIZE], patch[6] = {0xe8, 0, 0, 0, 0, 0x90};
    size_t size = buildRedirectCallStub(code, stub, (void *)fontFallbackRender, NULL);
    memcpy(stub, code, size);
    makePagesReadExecute(stub, 0x1000);
    rel = (int)(stub - (call + 5));
    memcpy(patch + 1, &rel, 4);
    return writeGameCode(call, patch, sizeof patch) ? NULL : "code_unwritable";
}
#endif

static void fontFallbackInit(void) {
    if (!checkGameFunction("font_fallback_check", "")) {
        writeLogError("font_fallback_installed", "ok=0 reason=%s", gameCodeReason());
        return;
    }
    const char *why = findFallbackCalls();
    if (!why) why = pointRenderCall();
    if (why) writeLogError("font_fallback_installed", "ok=0 reason=%s", why);
    else writeLog("font_fallback_installed", "ok=1 shaping=%d", fallbackCalls.setScript != NULL);
}

static void fallbackFontWordInit(void) {
    int ok = apiAddModCommand("fallbackfont", fallbackfontLine);
    writeResultLog(ok, "fallbackfont_word_installed", "ok=%d", ok);
}

