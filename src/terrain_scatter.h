#define SPRITE_PACK_WORD           "newspritepack"
#define SPRITE_WORD                "spr"
#define SCATTER_WORD               "terrainscatter"
#define SCATTER_CENTER_WORD        "terraincenter"
#define SCATTER_CLEAR_WORD         "clearcenter"
#define SCATTER_NAME_MAX           64
#define SPRITE_PACK_PICTURES_MAX  16
#define SCATTER_LINES_MAX          64
#define SCATTER_SQUARE_PIECES_MAX  100
#define SCATTER_COUNT_MAX          (SPRINKLE_POINTS < SCATTER_SQUARE_PIECES_MAX ? SPRINKLE_POINTS : SCATTER_SQUARE_PIECES_MAX)
#define SCATTER_SIZE_MAX           200
#define SCATTER_SWAY_MAX           500
#define SCATTER_SHADE_MAX          100
#define SCATTER_CLEAR_MAX          100
#define SCATTER_CLEAR_Y_OFFSET     (-0.4f)
#define SCATTER_CENTER_Y_OFFSET    0.25f
#define SCATTER_SNOW_GROUND        10
#define SCATTER_FROM_WORDS         1
#define SCATTER_EARLIER_PASS       2
#define SCATTER_PICTURE_SMALL      64
#define SCATTER_PICTURE_LARGE      128
#define SCATTER_PICTURES_CACHE     "scatter_pictures"

enum { SCATTER_ANY, SCATTER_COLD, SCATTER_SNOW, SCATTER_SNOW_COLD, SCATTER_CONDITIONS };
static const char *const SCATTER_CONDITION_WORDS[SCATTER_CONDITIONS] = {"", "cold", "snow", "snowcold"};
static const signed char SCATTER_STATE_TRIES[SCATTER_CONDITIONS][4] = {
    {SCATTER_ANY, -1, -1, -1}, {SCATTER_COLD, SCATTER_ANY, -1, -1}, {SCATTER_SNOW, SCATTER_ANY, -1, -1}, {SCATTER_SNOW_COLD, SCATTER_COLD, SCATTER_SNOW, SCATTER_ANY}};

static int scatterVersionFor(int versions, int state) {
    for (int i = 0; i < 4 && SCATTER_STATE_TRIES[state][i] > SCATTER_ANY; i++)
        if (versions & 1 << SCATTER_STATE_TRIES[state][i]) return SCATTER_STATE_TRIES[state][i];
    return SCATTER_ANY;
}

typedef struct {
    char mod[MOD_NAME_MAX], name[SCATTER_NAME_MAX];
    int script, from;
    int pictureCount, picture[SPRITE_PACK_PICTURES_MAX];
} SpritePack;

typedef struct {
    char mod[MOD_NAME_MAX], pack[SCATTER_NAME_MAX], file[MAX_PATH];
    int line, script, from, order;
    int rank;
    int terrain, condition, count, size, sway, shade;
    int clear;
    int center;
    int versionOf;
    int versions;
    int pictureCount, picture[SPRITE_PACK_PICTURES_MAX];
} ScatterLine;

static SpritePack *spritePacks;
static int spritePackCount, spritePackCapacity;
static ScatterLine *scatterLines;
static int scatterLineCount, scatterLineCapacity, scatterOrder;
static int *scatterDrawn, scatterDrawnCapacity;
static int scatterFirst[TERRAIN_TABLE_MAX];
static short scatterCount[TERRAIN_TABLE_MAX];
typedef void (*DecorScatterFn)(int x, int y, int kind, int sprite, int variants, int count, float size, float sway, float shade);
typedef void (*DecorSingleFn)(int x, int y, int kind, int sprite, int variants, int count, float size, float sway, float shade, float spread,
                             float widthJitter, float heightJitter, float keepout, float yOffset, float alpha);
typedef void (*DecorCenteredFn)(int x, int y, int kind, int sprite, float size, long long unread1, float yOffset, long long unread2, float alpha);
static DecorScatterFn scatterForTests;
static DecorSingleFn scatterSingleForTests;
static DecorCenteredFn scatterCenteredForTests;
static unsigned char *(*scatterImageReadForTests)(const char *path, int *w, int *h, int mode);

static const SpritePack *findSpritePack(const char *mod, const char *name) {
    for (int i = 0; i < spritePackCount; i++)
        if (spritePacks[i].from != SCATTER_EARLIER_PASS && !strcmp(spritePacks[i].mod, mod) && !strcmp(spritePacks[i].name, name)) return &spritePacks[i];
    return NULL;
}

static int scatterModRank(const char *mod) {
    for (int i = 0; i <= G32(RVA_MOD_LAST_INDEX) && i < 4096; i++)
        if (!strcmp(modName(i), mod)) return (int)(short)G16(RVA_MOD_NAMES + (size_t)i * MOD_RECORD_SIZE + MOD_RECORD_PRIO) * 4096 + i;
    return -1;
}

static int scatterLineLater(const ScatterLine *a, const ScatterLine *b) { return a->rank != b->rank ? a->rank > b->rank : a->order > b->order; }

static int lastScatterPiece(const char *mod, int terrain, int script) {
    for (int i = scatterLineCount - 1; i >= 0; i--) {
        const ScatterLine *s = &scatterLines[i];
        if (s->from != SCATTER_EARLIER_PASS && s->condition == SCATTER_ANY && s->terrain == terrain && s->script == script && !strcmp(s->mod, mod)) return i;
    }
    return -1;
}

static int scatterLinesOn(const char *mod, int terrain) {
    int lines = 0;
    for (int i = 0; i < scatterLineCount; i++)
        lines += scatterLines[i].from != SCATTER_EARLIER_PASS && scatterLines[i].terrain == terrain && !strcmp(scatterLines[i].mod, mod);
    return lines;
}

static int mostScatterPieces(const char *mod, int terrain, const ScatterLine *added, int addedCount, int *state) {
    static int sums[SCATTER_LINES_MAX][SCATTER_CONDITIONS], versions[SCATTER_LINES_MAX];
    int pieces = 0, last[2] = {-1, -1}, most = 0;
    *state = SCATTER_ANY;
    for (int i = 0; i < scatterLineCount + addedCount; i++) {
        const ScatterLine *s = i < scatterLineCount ? &scatterLines[i] : &added[i - scatterLineCount];
        if (i < scatterLineCount && (s->from == SCATTER_EARLIER_PASS || s->terrain != terrain || strcmp(s->mod, mod))) continue;
        int *piece = &last[i >= scatterLineCount || s->script ? 1 : 0];
        if (s->condition == SCATTER_ANY) {
            *piece = pieces < SCATTER_LINES_MAX ? pieces++ : -1;
            if (*piece < 0) continue;
            memset(sums[*piece], 0, sizeof sums[*piece]);
            versions[*piece] = 0, sums[*piece][SCATTER_ANY] = s->count;
        } else if (*piece >= 0) versions[*piece] |= 1 << s->condition, sums[*piece][s->condition] += s->count;
    }
    for (int c = 0; c < SCATTER_CONDITIONS; c++) {
        int total = 0;
        for (int p = 0; p < pieces; p++) total += sums[p][scatterVersionFor(versions[p], c)];
        if (total > most) most = total, *state = c;
    }
    return most;
}

static void writeScatterPiecesWhat(char *out, size_t size, int most, int state) {
    if (state == SCATTER_ANY) snprintf(out, size, "%d", most);
    else snprintf(out, size, "%d on %s squares", most, SCATTER_CONDITION_WORDS[state]);
}

static void rebuildTerrainScatter(void) {
    static int owner[TERRAIN_TABLE_MAX];
    static int lastPiece[TERRAIN_TABLE_MAX][2];
    memset(owner, 0, sizeof owner);
    memset(scatterCount, 0, sizeof scatterCount);
    memset(lastPiece, 0xff, sizeof lastPiece);
    for (int i = 0; i < scatterLineCount; i++) {
        ScatterLine *s = &scatterLines[i];
        s->versionOf = -1, s->versions = 0;
        if (s->from == SCATTER_EARLIER_PASS) continue;
        if (!owner[s->terrain] || scatterLineLater(s, &scatterLines[owner[s->terrain] - 1])) owner[s->terrain] = i + 1;
    }
    int drawn = 0;
    for (int i = 0; i < scatterLineCount; i++) {
        const ScatterLine *s = &scatterLines[i];
        if (s->from != SCATTER_EARLIER_PASS && owner[s->terrain] && !strcmp(scatterLines[owner[s->terrain] - 1].mod, s->mod)) scatterCount[s->terrain]++, drawn++;
    }
    if (drawn > scatterDrawnCapacity && !growList((void **)&scatterDrawn, &scatterDrawnCapacity, drawn, sizeof *scatterDrawn)) {
        memset(scatterCount, 0, sizeof scatterCount);
        writeLogError("terrain_scatter_not_drawn", "reason=out_of_memory lines=%d", drawn);
        return;
    }
    for (int t = 0, at = 0; t < TERRAIN_TABLE_MAX; t++) scatterFirst[t] = at, at += scatterCount[t], scatterCount[t] = 0;
    for (int i = 0; i < scatterLineCount; i++) {
        ScatterLine *s = &scatterLines[i];
        if (s->from == SCATTER_EARLIER_PASS || !owner[s->terrain] || strcmp(scatterLines[owner[s->terrain] - 1].mod, s->mod)) continue;
        scatterDrawn[scatterFirst[s->terrain] + scatterCount[s->terrain]++] = i;
        int *piece = &lastPiece[s->terrain][s->script ? 1 : 0];
        if (s->condition == SCATTER_ANY) *piece = i;
        else if ((s->versionOf = *piece) >= 0) scatterLines[*piece].versions |= 1 << s->condition;
    }
}

static int terrainHasScatter(int terrain) { return terrain >= 0 && terrain < TERRAIN_TABLE_MAX && scatterCount[terrain] > 0; }

static void markScriptScatterRestarting(const char *mod) {
    for (int i = 0; i < spritePackCount; i++)
        if (spritePacks[i].script && !spritePacks[i].from && !strcmp(spritePacks[i].mod, mod)) spritePacks[i].from = SCATTER_EARLIER_PASS;
    for (int i = 0; i < scatterLineCount; i++)
        if (scatterLines[i].script && !scatterLines[i].from && !strcmp(scatterLines[i].mod, mod)) scatterLines[i].from = SCATTER_EARLIER_PASS;
}

// ---- Pictures: a file or a folder, each fitted to a square ----------------------------------------
static const char *addScatterPicture(SpritePack *pack, const char *folder, const char *spec) {
    int picture;
    const char *reason = findTerrainPicture(folder, spec, TERRAIN_PICTURE_FITTED, &picture);
    if (reason) return reason;
    for (int k = 0; k < pack->pictureCount; k++)
        if (pack->picture[k] == picture) return NULL;
    if (pack->pictureCount >= SPRITE_PACK_PICTURES_MAX) return "too_many_pictures";
    pack->picture[pack->pictureCount++] = picture;
    return NULL;
}

static int compareScatterFileNames(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

static const char *addScatterPictures(SpritePack *pack, const char *folder, const char *spec, char *what, size_t whatSize) {
    snprintf(what, whatSize, "%s", spec);
    size_t length = strlen(spec);
    if (!length || spec[length - 1] != '/') return addScatterPicture(pack, folder, spec);
    static const char *const ENDINGS[2] = {"", ""};
    static const char *const REASONS[4] = {"bad_folder", "folder_not_found", "folder_wrong_case", "folder_link"};
    char sub[PLANE_ICON_SPEC_MAX + 1], dir[MAX_PATH], pattern[MAX_PATH];
    snprintf(sub, sizeof sub, "%.*s", (int)(length - 1), spec);
    const char *reason = length > PLANE_ICON_SPEC_MAX ? "bad_folder" : findModFile(folder, sub, ENDINGS, REASONS, dir, sizeof dir);
    if (reason) return reason;
    if (!buildPath(pattern, sizeof pattern, __func__, "%s" PATH_SEP "*", dir)) return "path_too_long";
    static char names[SPRITE_PACK_PICTURES_MAX + 1][MAX_PATH];
    int count = 0;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            size_t n = strlen(fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY || n < 5 || (_stricmp(fd.cFileName + n - 4, ".tga") && _stricmp(fd.cFileName + n - 4, ".png"))) continue;
            if (count <= SPRITE_PACK_PICTURES_MAX) snprintf(names[count], sizeof names[count], "%s", fd.cFileName);
            count++;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    if (!count) return "empty_folder";
    if (count > SPRITE_PACK_PICTURES_MAX) return "too_many_pictures";
    qsort(names, (size_t)count, sizeof names[0], compareScatterFileNames);
    SpritePack tried = *pack;
    for (int i = 0; i < count; i++) {
        char file[PLANE_ICON_SPEC_MAX + MAX_PATH + 2];
        snprintf(file, sizeof file, "%s%.*s", spec, (int)sizeof names[i] - 1, names[i]);
        if ((reason = addScatterPicture(&tried, folder, file))) {
            if (strcmp(reason, "too_many_pictures")) snprintf(what, whatSize, "%s", file);
            return reason;
        }
    }
    *pack = tried;
    return NULL;
}

static int scatterProblemText(const char *reason, const char *what, char *out, size_t size) {
    if (!strcmp(reason, "icon_not_found") || !strcmp(reason, "icon_wrong_case")) snprintf(out, size, "No picture '%.70s'", what);
    else if (!strcmp(reason, "folder_not_found") || !strcmp(reason, "folder_wrong_case")) snprintf(out, size, "No folder '%.70s'", what);
    else if (!strcmp(reason, "bad_icon") || !strcmp(reason, "bad_folder")) snprintf(out, size, "Bad picture name '%.70s'", what);
    else if (!strcmp(reason, "bad_picture")) snprintf(out, size, "Not a .tga or .png picture: '%.60s'", what);
    else if (!strcmp(reason, "empty_folder")) snprintf(out, size, "No .tga or .png pictures in '%.60s'", what);
    else if (!strcmp(reason, "too_many_pictures")) snprintf(out, size, "More than 16 pictures in a sprite pack: '%.50s'", what);
    else if (!strcmp(reason, "pack_taken")) snprintf(out, size, "Sprite pack '%.40s' declared twice", what);
    else if (!strcmp(reason, "no_pictures")) snprintf(out, size, "Sprite pack '%.40s' has no pictures", what);
    else if (!strcmp(reason, "no_name")) snprintf(out, size, "newspritepack needs a name");
    else if (!strcmp(reason, "no_pack")) snprintf(out, size, "No sprite pack called '%.40s'", what);
    else if (!strcmp(reason, "no_terrain_block")) snprintf(out, size, "%.20s only after selectterr", what);
    else if (!strcmp(reason, "bad_numbers")) snprintf(out, size, "terrainscatter: count 1-%d, size 1-200, sway 0-500, shade 0-100, clearcenter 0-100", SCATTER_COUNT_MAX);
    else if (!strcmp(reason, "too_many_lines")) snprintf(out, size, "More than %d scatter lines on one terrain", SCATTER_LINES_MAX);
    else if (!strcmp(reason, "too_many_pieces")) snprintf(out, size, "More than %d pieces a square: %.40s", SCATTER_SQUARE_PIECES_MAX, what);
    else if (!strcmp(reason, "bad_center_size")) snprintf(out, size, "terraincenter: size 1-200");
    else if (!strcmp(reason, "zero_count")) snprintf(out, size, "A count of 0 only on a cold, snow or snowcold line");
    else if (!strcmp(reason, "no_piece_above")) snprintf(out, size, "A %.10s line needs a line with no state word above it", what);
    else if (!strcmp(reason, "bad_condition")) snprintf(out, size, "No scatter condition called '%.30s': cold, snow or snowcold", what);
    else return 0;
    return 1;
}

// ---- Declarations: the words' and the Lua calls' ---------------------------------------------------
static const char *declareSpritePack(const char *mod, const SpritePack *given, int script) {
    if (!given->pictureCount) return "no_pictures";
    if (findSpritePack(mod, given->name)) return "pack_taken";
    if (!growList((void **)&spritePacks, &spritePackCapacity, spritePackCount + 1, sizeof *spritePacks)) return "out_of_memory";
    SpritePack *p = &spritePacks[spritePackCount++];
    *p = *given;
    snprintf(p->mod, sizeof p->mod, "%s", mod);
    p->script = script, p->from = script ? 0 : SCATTER_FROM_WORDS;
    writeLog("sprite_pack_declared", "mod=%s name=%s pictures=%d script=%d", mod, p->name, p->pictureCount, script);
    return NULL;
}

static const char *scatterNumbersReason(const ScatterLine *s) {
    if (s->center) return s->size < 1 || s->size > SCATTER_SIZE_MAX ? "bad_center_size" : NULL;
    if (s->count < 0 || s->count > SCATTER_COUNT_MAX || s->size < 1 || s->size > SCATTER_SIZE_MAX || s->sway < 0 || s->sway > SCATTER_SWAY_MAX || s->shade < 0 ||
        s->shade > SCATTER_SHADE_MAX || s->clear < -1 || s->clear > SCATTER_CLEAR_MAX)
        return "bad_numbers";
    return !s->count && s->condition == SCATTER_ANY ? "zero_count" : NULL;
}

static const char *declareScatterLine(const char *mod, const ScatterLine *given, int script, const char *file, int line) {
    const SpritePack *pack = findSpritePack(mod, given->pack);
    if (!pack) return "no_pack";
    if (given->terrain < 0 || given->terrain >= TERRAIN_TABLE_MAX) return "unknown_terrain";
    if (given->condition < SCATTER_ANY || given->condition >= SCATTER_CONDITIONS) return "bad_condition";
    const char *reason = scatterNumbersReason(given);
    if (reason) return reason;
    if (scatterLinesOn(mod, given->terrain) >= SCATTER_LINES_MAX) return "too_many_lines";
    int piece = given->condition == SCATTER_ANY ? -1 : lastScatterPiece(mod, given->terrain, script);
    if (given->condition != SCATTER_ANY && piece < 0) return "no_piece_above";
    if (!installTerrainDecorHook()) return "hook_not_installed";
    if (!growList((void **)&scatterLines, &scatterLineCapacity, scatterLineCount + 1, sizeof *scatterLines)) return "out_of_memory";
    ScatterLine *s = &scatterLines[scatterLineCount++];
    *s = *given;
    snprintf(s->mod, sizeof s->mod, "%s", mod);
    snprintf(s->file, sizeof s->file, "%s", file);
    s->line = line, s->script = script, s->from = script ? 0 : SCATTER_FROM_WORDS, s->order = ++scatterOrder, s->rank = scatterModRank(mod);
    s->pictureCount = pack->pictureCount;
    memcpy(s->picture, pack->picture, sizeof s->picture);
    const ScatterLine *of = piece >= 0 ? &scatterLines[piece] : NULL;
    writeLog("scatter_line_declared", "mod=%s word=%s terrain=%d pack=%s count=%d size=%d sway=%d shade=%d clearcenter=%d condition=%s version_of=%s version_of_line=%d script=%d", mod,
             s->center ? SCATTER_CENTER_WORD : SCATTER_WORD, s->terrain, s->pack, s->count, s->size, s->sway, s->shade, s->clear,
             s->condition ? SCATTER_CONDITION_WORDS[s->condition] : "any", of ? of->pack : "-", of ? of->line : 0, script);
    rebuildTerrainScatter();
    return NULL;
}

// ---- Fitting a picture: a 64 or 128 square, its bottom row on the ground ------------------------------
static unsigned char *fitScatterPixels(const unsigned char *rgba, int fullW, int fullH, int *side, int *fw, int *fh) {
    int bx0 = fullW, by0 = fullH, bx1 = -1, by1 = -1;
    for (int y = 0; y < fullH; y++)
        for (int x = 0; x < fullW; x++)
            if (rgba[((size_t)y * (size_t)fullW + (size_t)x) * 4 + 3]) {
                if (x < bx0) bx0 = x;
                if (x > bx1) bx1 = x;
                if (y < by0) by0 = y;
                if (y > by1) by1 = y;
            }
    if (bx1 < 0) bx0 = by0 = 0, bx1 = fullW - 1, by1 = fullH - 1;
    int w = bx1 - bx0 + 1, h = by1 - by0 + 1;
    const unsigned char *origin = rgba + ((size_t)by0 * (size_t)fullW + (size_t)bx0) * 4;
    int c = w > SCATTER_PICTURE_SMALL || h > SCATTER_PICTURE_SMALL ? SCATTER_PICTURE_LARGE : SCATTER_PICTURE_SMALL;
    int nw = w, nh = h;
    if (w > c || h > c) {
        if (w >= h) nw = c, nh = (int)(((long long)h * c + w / 2) / w);
        else nh = c, nw = (int)(((long long)w * c + h / 2) / h);
        if (nw < 1) nw = 1;
        if (nh < 1) nh = 1;
    }
    unsigned char *out = calloc((size_t)c * (size_t)c, 4);
    if (!out) return NULL;
    int left = (c - nw) / 2;
    for (int y = 0; y < nh; y++) {
        int y0 = (int)((long long)y * h / nh), y1 = (int)((long long)(y + 1) * h / nh);
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < nw; x++) {
            int x0 = (int)((long long)x * w / nw), x1 = (int)((long long)(x + 1) * w / nw);
            if (x1 <= x0) x1 = x0 + 1;
            unsigned long long r = 0, g = 0, b = 0, a = 0, n = 0;
            for (int sy = y0; sy < y1; sy++)
                for (int sx = x0; sx < x1; sx++) {
                    const unsigned char *p = origin + ((size_t)sy * (size_t)fullW + (size_t)sx) * 4;
                    r += (unsigned long long)p[0] * p[3], g += (unsigned long long)p[1] * p[3], b += (unsigned long long)p[2] * p[3], a += p[3], n++;
                }
            unsigned char *q = out + ((size_t)y * (size_t)c + (size_t)(left + x)) * 4;
            if (a) q[0] = (unsigned char)(r / a), q[1] = (unsigned char)(g / a), q[2] = (unsigned char)(b / a), q[3] = (unsigned char)(a / n);
        }
    }
    *side = c, *fw = nw, *fh = nh;
    return out;
}

static int writeScatterTgaRows(const char *path, int w, int h, const unsigned char *rgba, int bottomFirst) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    unsigned char head[18] = {0, 0, 2};
    head[12] = (unsigned char)w, head[13] = (unsigned char)(w >> 8), head[14] = (unsigned char)h, head[15] = (unsigned char)(h >> 8), head[16] = 32, head[17] = 8;
    int ok = fwrite(head, 1, sizeof head, f) == sizeof head;
    unsigned char *row = malloc((size_t)w * 4);
    ok = ok && row;
    for (int k = 0; ok && k < h; k++) {
        const unsigned char *p = rgba + (size_t)(bottomFirst ? k : h - 1 - k) * (size_t)w * 4;
        for (int x = 0; x < w; x++) row[4 * x] = p[4 * x + 2], row[4 * x + 1] = p[4 * x + 1], row[4 * x + 2] = p[4 * x], row[4 * x + 3] = p[4 * x + 3];
        ok = fwrite(row, 4, (size_t)w, f) == (size_t)w;
    }
    free(row);
    return fclose(f) == 0 && ok;
}

static int scatterPictureCachePath(const TerrainPicture *t, char *out, size_t size) {
    unsigned hash = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)t->path; *p; p++) hash = (hash ^ *p) * 16777619u;
    return buildPath(out, size, __func__, "%s" USER_DIR_SEP COE5SE_USER_DIR USER_DIR_SEP SCATTER_PICTURES_CACHE USER_DIR_SEP "%08x.tga", userDir(), hash);
}

static int fitScatterPictureFile(TerrainPicture *t, char *out, size_t size) {
    if (!scatterPictureCachePath(t, out, size)) return 0;
    if (t->fitted) return t->fitted > 0;
    t->fitted = -1;
    int w = 0, h = 0, side = 0, fw = 0, fh = 0;
    unsigned char *rgba = scatterImageReadForTests ? scatterImageReadForTests(t->path, &w, &h, SPRITE_MODE_MAP)
                                                   : GAME_FN(unsigned char *(*)(const char *, int *, int *, int), RVA_IMAGE_READ)(t->path, &w, &h, SPRITE_MODE_MAP);
    unsigned char *fit = rgba && w > 0 && h > 0 ? fitScatterPixels(rgba, w, h, &side, &fw, &fh) : NULL;
    if (rgba && scatterImageReadForTests) free(rgba);
    else if (rgba) GAME_FN(void (*)(void *), RVA_FREE)(rgba);
    char folder[MAX_PATH];
    if (buildPath(folder, sizeof folder, __func__, "%s" USER_DIR_SEP COE5SE_USER_DIR, userDir())) CreateDirectoryA(folder, NULL);
    if (buildPath(folder, sizeof folder, __func__, "%s" USER_DIR_SEP COE5SE_USER_DIR USER_DIR_SEP SCATTER_PICTURES_CACHE, userDir())) CreateDirectoryA(folder, NULL);
    int ok = fit && writeScatterTgaRows(out, side, side, fit, 1);
    free(fit);
    if (!ok) {
        writeLogWarning("scatter_picture_refused", "reason=%s path=%s", !rgba ? "not_decoded" : !fit ? "out_of_memory" : "write_failed", t->path);
        return 0;
    }
    t->fitted = 1;
    if (fw != w || fh != h) writeLog("scatter_picture_resized", "from=%dx%d to=%dx%d side=%d path=%s", w, h, fw, fh, side, t->path);
    return 1;
}

// ---- The decorations ---------------------------------------------------------------------------
static unsigned scatterPieceHash(int plane, int x, int y, int k, int n) {
    unsigned h = (unsigned)plane * 0x9e3779b1u ^ (unsigned)x * 0x85ebca6bu ^ (unsigned)y * 0xc2b2ae35u ^ (unsigned)n * 0x27d4eb2fu ^ (unsigned)k * 0x165667b1u;
    h ^= h >> 15, h *= 0x2c1b3c6du, h ^= h >> 12;
    return h;
}

static int scatterSquareState(int x, int y) {
    if (x < 0 || x >= MAP_COLUMNS_MAX || y < 0 || y >= MAP_COLUMN_SQUARES) return SCATTER_ANY;
    size_t square = ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE;
    short ground = (short)G16(RVA_SQUARE_RECORDS + square + SQUARE_GROUND);
    return (ground == SCATTER_SNOW_GROUND ? SCATTER_SNOW : 0) | (G32(RVA_SQUARE_FLAGS + square) & SQUARE_FLAG_COLD ? SCATTER_COLD : 0);
}

static void drawScatterLine(const ScatterLine *s, int k, int plane, int x, int y) {
    int sprite = terrainPictureSprite(s->picture[0]);
    if (sprite < 0) return;
    float zoom;
    memcpy(&zoom, GAME_PTR(RVA_MAP_ZOOM), 4);
    int first = G32(RVA_DECOR_QUEUE_LAST) + 1;
    float size = zoom * (float)s->size / 100.0f, sway = (float)s->sway / 100.0f, shade = (float)s->shade / 100.0f;
    if (s->center) {
        DecorCenteredFn centered = scatterCenteredForTests ? scatterCenteredForTests : GAME_FN(DecorCenteredFn, RVA_DECOR_CENTERED);
        centered(x, y, SPRITE_KIND_TREE, sprite, size, 0, SCATTER_CENTER_Y_OFFSET, 0, 1.0f);
    } else if (s->clear >= 0) {
        DecorSingleFn single = scatterSingleForTests ? scatterSingleForTests : GAME_FN(DecorSingleFn, RVA_DECOR_SINGLE);
        single(x, y, SPRITE_KIND_TREE, sprite, 1, s->count, size, sway, shade, s->count > 1 ? 1.0f : 0.0f, 0, 0, (float)s->clear / 100.0f, SCATTER_CLEAR_Y_OFFSET, 1.0f);
    } else {
        DecorScatterFn scatter = scatterForTests ? scatterForTests : GAME_FN(DecorScatterFn, RVA_DECOR_SCATTER);
        scatter(x, y, SPRITE_KIND_TREE, sprite, 1, s->count, size, sway, shade);
    }
    unsigned char *queue = GAME_PTR(RVA_DECOR_QUEUE);
    for (int i = first; i <= G32(RVA_DECOR_QUEUE_LAST); i++) {
        int pic = s->picture[s->pictureCount > 1 ? scatterPieceHash(plane, x, y, k, i - first) % (unsigned)s->pictureCount : 0];
        int to = pic == s->picture[0] ? sprite : terrainPictureSprite(pic);
        if (to >= 0) pointDecorEntry(queue + (size_t)i * DECOR_QUEUE_ENTRY_SIZE, to);
    }
}

static void drawTerrainScatter(int plane, int x, int y, int terrain) {
    int state = scatterSquareState(x, y);
    for (int k = 0; k < scatterCount[terrain]; k++) {
        const ScatterLine *s = &scatterLines[scatterDrawn[scatterFirst[terrain] + k]];
        const ScatterLine *piece = s->condition == SCATTER_ANY ? s : s->versionOf >= 0 ? &scatterLines[s->versionOf] : NULL;
        if (!piece || !s->count) continue;
        if (s->condition == scatterVersionFor(piece->versions, state)) drawScatterLine(s, k, plane, x, y);
    }
}

// ---- The words -----------------------------------------------------------------------------------
static void refuseScatterLine(const char *word, int mod, const char *reason, const char *what, const char *args) {
    const char *name = spellModName(mod), *file = (const char *)GAME_PTR(RVA_EVENT_FILE);
    int line = G32(RVA_MOD_LINE);
    refuseC5mLine(word, name, reason, "line=%d text=%.60s", line, args);
    char text[96];
    if (scatterProblemText(reason, what, text, sizeof text)) addPlaneProblem(0, name, file, line, "%s", text);
}

static struct {
    int open, refused, mod, line;
    unsigned int fileId;
    char file[MAX_PATH];
    SpritePack pack;
} spritePackBlock;

static void finishSpritePack(void) {
    if (!spritePackBlock.open) return;
    spritePackBlock.open = 0;
    if (spritePackBlock.refused) return;
    const char *mod = spellModName(spritePackBlock.mod), *reason = declareSpritePack(mod, &spritePackBlock.pack, 0);
    if (!reason) return;
    refuseC5mLine(SPRITE_PACK_WORD, mod, reason, "line=%d pack=%.60s", spritePackBlock.line, spritePackBlock.pack.name);
    char text[96];
    if (scatterProblemText(reason, spritePackBlock.pack.name, text, sizeof text)) addPlaneProblem(0, mod, spritePackBlock.file, spritePackBlock.line, "%s", text);
}

static void newspritepackLine(const char *args, int mod, int level) {
    if (level != 2) return;
    finishSpritePack();
    memset(&spritePackBlock, 0, sizeof spritePackBlock);
    spritePackBlock.open = 1, spritePackBlock.mod = mod, spritePackBlock.line = G32(RVA_MOD_LINE), spritePackBlock.fileId = modFileNow();
    snprintf(spritePackBlock.file, sizeof spritePackBlock.file, "%s", (const char *)GAME_PTR(RVA_EVENT_FILE));
    const char *p = args;
    const char *reason = mod < 0 || mod > G32(RVA_MOD_LAST_INDEX)                                          ? "not_in_a_mod"
                       : readSpellQuoted(&p, spritePackBlock.pack.name, sizeof spritePackBlock.pack.name) <= 0 ? "no_name"
                       : NULL;
    if (reason) spritePackBlock.refused = 1, refuseScatterLine(SPRITE_PACK_WORD, mod, reason, "", args);
}

static void packSprLine(const char *args, int mod) {
    if (spritePackBlock.refused) return;
    const char *p = args;
    char spec[PLANE_ICON_SPEC_MAX + 2] = "", what[PLANE_ICON_SPEC_MAX + MAX_PATH + 2] = "";
    const char *reason = readSpellQuoted(&p, spec, sizeof spec) <= 0 ? "bad_icon" : NULL;
    if (!reason) skipSpaces(&p);
    if (!reason && *p && *p != '\r' && *p != '\n' && *p != '#') reason = "bad_icon", snprintf(what, sizeof what, "%.60s", p);
    if (!reason) reason = addScatterPictures(&spritePackBlock.pack, modFolder(mod), spec, what, sizeof what);
    if (reason) refuseScatterLine(SPRITE_WORD, mod, reason, what[0] ? what : spec, args);
}

static const char *spritePackLineHook(const char *line, size_t len, int mod, int level) {
    if (level != 2 || !spritePackBlock.open) return line;
    if (spritePackBlock.mod != mod || spritePackBlock.fileId != modFileNow() || !spellWordIs(line, len, SPRITE_WORD)) {
        finishSpritePack();
        return line;
    }
    const char *args = line + len;
    while (*args == ' ' || *args == '\t') args++;
    packSprLine(args, mod);
    return NULL;
}

static int scatterConditionNamed(const char *word) {
    for (int c = SCATTER_COLD; c < SCATTER_CONDITIONS; c++)
        if (!strcmp(word, SCATTER_CONDITION_WORDS[c])) return c;
    return -1;
}

static size_t readScatterWord(const char **p, char *word, size_t size) {
    skipSpaces(p);
    size_t len = strcspn(*p, " \t\r\n#");
    snprintf(word, size, "%.*s", (int)(len < size ? len : size - 1), *p);
    *p += len;
    return len;
}

static void readScatterLine(const char *word, int center, const char *args, int mod, int level) {
    if (level != 2) return;
    ScatterLine s;
    memset(&s, 0, sizeof s);
    s.terrain = G32(RVA_TERRAIN_CURRENT), s.clear = -1, s.center = center;
    const char *p = args;
    long long n[4] = {1, -1, 0, 0};
    char state[32] = "";
    const char *reason = mod < 0 || mod > G32(RVA_MOD_LAST_INDEX)            ? "not_in_a_mod"
                       : s.terrain < 0 || G32(RVA_CLASS_CURRENT) >= 0        ? "no_terrain_block"
                       : readSpellQuoted(&p, s.pack, sizeof s.pack) <= 0     ? "no_pack"
                       : NULL;
    for (int i = center; i < (center ? 2 : 4) && !reason; i++)
        if (!readSpellNumber(&p, &n[i]) || n[i] < 0 || n[i] > 100000) reason = center ? "bad_center_size" : "bad_numbers";
    if (!reason) {
        size_t len = readScatterWord(&p, state, sizeof state);
        long long clear;
        if (len && !center && !strcmp(state, SCATTER_CLEAR_WORD)) {
            if (!readSpellNumber(&p, &clear) || clear < 0 || clear > SCATTER_CLEAR_MAX) reason = "bad_numbers";
            else s.clear = (int)clear, len = readScatterWord(&p, state, sizeof state);
        }
        s.condition = len ? scatterConditionNamed(state) : SCATTER_ANY;
        skipSpaces(&p);
        if (!reason && (s.condition < 0 || (*p && *p != '\r' && *p != '\n' && *p != '#'))) reason = "bad_condition";
    }
    s.count = (int)n[0], s.size = (int)n[1], s.sway = (int)n[2], s.shade = (int)n[3];
    if (!reason) reason = declareScatterLine(spellModName(mod), &s, 0, (const char *)GAME_PTR(RVA_EVENT_FILE), G32(RVA_MOD_LINE));
    if (reason)
        refuseScatterLine(word, mod, reason, !strcmp(reason, "bad_condition") ? state : !strcmp(reason, "no_terrain_block") ? word : !strcmp(reason, "no_piece_above") ? SCATTER_CONDITION_WORDS[s.condition] : s.pack, args);
}

static void terrainscatterLine(const char *args, int mod, int level) { readScatterLine(SCATTER_WORD, 0, args, mod, level); }

static void terraincenterLine(const char *args, int mod, int level) { readScatterLine(SCATTER_CENTER_WORD, 1, args, mod, level); }

// ---- The passes ----------------------------------------------------------------------------------
static void dumpTreesOnce(void);
static void installTreesDumpSwitch(void);

static void terrainScatterBeforePass(int level) {
    if (level != 2) return;
    dumpTreesOnce();
    finishSpritePack();
    for (int i = 0; i < spritePackCount; i++)
        if (spritePacks[i].from) spritePacks[i].from = SCATTER_EARLIER_PASS;
    for (int i = 0; i < scatterLineCount; i++)
        if (scatterLines[i].from) scatterLines[i].from = SCATTER_EARLIER_PASS;
}

static void refuseCrowdedScatterWords(void) {
    for (int i = 0; i < scatterLineCount; i++) {
        const ScatterLine *s = &scatterLines[i];
        int state, most;
        if (s->from != SCATTER_FROM_WORDS || (most = mostScatterPieces(s->mod, s->terrain, NULL, 0, &state)) <= SCATTER_SQUARE_PIECES_MAX) continue;
        char what[48], text[96];
        writeScatterPiecesWhat(what, sizeof what, most, state);
        refuseC5mLine(SCATTER_WORD, s->mod, "too_many_pieces", "line=%d terrain=%d pieces=%d state=%s", s->line, s->terrain, most, state ? SCATTER_CONDITION_WORDS[state] : "any");
        if (scatterProblemText("too_many_pieces", what, text, sizeof text)) addPlaneProblem(0, s->mod, s->file, s->line, "%s", text);
        for (int j = scatterLineCount - 1; j >= i; j--)
            if (scatterLines[j].from == SCATTER_FROM_WORDS && scatterLines[j].terrain == s->terrain && !strcmp(scatterLines[j].mod, s->mod)) scatterLines[j].from = SCATTER_EARLIER_PASS;
    }
}

static void terrainScatterAfterPass(int level) {
    if (level != 2) return;
    finishSpritePack();
    refuseCrowdedScatterWords();
    int kept = 0;
    for (int i = 0; i < spritePackCount; i++) {
        if (spritePacks[i].from == SCATTER_EARLIER_PASS) {
            writeLog("sprite_pack_dropped", "mod=%s name=%s reason=not_declared_this_pass", spritePacks[i].mod, spritePacks[i].name);
            continue;
        }
        if (kept != i) spritePacks[kept] = spritePacks[i];
        kept++;
    }
    spritePackCount = kept, kept = 0;
    for (int i = 0; i < scatterLineCount; i++) {
        if (scatterLines[i].from == SCATTER_EARLIER_PASS) continue;
        if (kept != i) scatterLines[kept] = scatterLines[i];
        kept++;
    }
    scatterLineCount = kept;
    rebuildTerrainScatter();
}

// ---- Lua ---------------------------------------------------------------------------------------
static const char *luaScatterPictures(lua_State *L, int index, const char *folder, SpritePack *pack, char *what, size_t whatSize) {
    if (lua_type(L, index) == LUA_TSTRING) return addScatterPictures(pack, folder, lua_tostring(L, index), what, whatSize);
    if (!lua_istable(L, index)) return "no_pictures";
    const char *reason = NULL;
    for (lua_Integer i = 1; !reason; i++) {
        lua_rawgeti(L, index, i);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            break;
        }
        reason = lua_type(L, -1) == LUA_TSTRING ? addScatterPictures(pack, folder, lua_tostring(L, -1), what, whatSize) : "bad_list";
        lua_pop(L, 1);
    }
    return reason;
}

static int luaCallerLine(lua_State *L) {
    lua_Debug caller;
    return lua_getstack(L, 1, &caller) && lua_getinfo(L, "l", &caller) ? caller.currentline : 0;
}

static void refuseScatterCall(lua_State *L, const char *call, const char *reason, const char *what) {
    const char *mod = runningLuaMod();
    refuseLuaCall(mod, call, reason, "what=%.80s", what[0] ? what : "-");
    char text[96];
    if (scatterProblemText(reason, what, text, sizeof text)) addPlaneProblem(1, mod ? mod : "?", luaRunning ? luaRunning->path : "?", luaCallerLine(L), "%s", text);
}

static int lSetSpritePack(lua_State *L) {
    SpritePack pack;
    memset(&pack, 0, sizeof pack);
    const char *mod = runningLuaMod(), *folder = luaRunning ? luaRunning->folder : "";
    char what[PLANE_ICON_SPEC_MAX + MAX_PATH + 2] = "";
    const char *reason = !luaScriptsLoading ? "not_at_load" : lua_type(L, 1) != LUA_TSTRING || !*lua_tostring(L, 1) ? "no_name" : NULL;
    if (!reason) snprintf(pack.name, sizeof pack.name, "%s", lua_tostring(L, 1));
    if (!reason) reason = luaScatterPictures(L, 2, folder, &pack, what, sizeof what);
    if (!reason) reason = declareSpritePack(mod ? mod : "?", &pack, 1);
    if (reason) refuseScatterCall(L, "coe5.setSpritePack", reason, !strcmp(reason, "pack_taken") || !strcmp(reason, "no_pictures") ? pack.name : what);
    lua_pushboolean(L, !reason);
    return 1;
}

static int luaScatterNumber(lua_State *L, const char *key, int fallback, int *out) {
    lua_getfield(L, -1, key);
    int missing = lua_isnil(L, -1), whole = lua_isinteger(L, -1);
    lua_Integer v = whole ? lua_tointeger(L, -1) : -1;
    lua_pop(L, 1);
    *out = missing ? fallback : v < 0 || v > 100000 ? INT_MIN : (int)v;
    return missing ? fallback != -1 : whole;
}

static int lSetTerrainScatter(lua_State *L) {
    const char *mod = runningLuaMod();
    char what[SCATTER_NAME_MAX + 8] = "";
    int terrain = luaTerrainArg(L, 1);
    const char *reason = !luaScriptsLoading ? "not_at_load" : terrain == -1 ? "no_terrain" : terrain < 0 || terrain >= TERRAIN_TABLE_MAX ? "unknown_terrain" : !lua_istable(L, 2) ? "bad_list" : NULL;
    static ScatterLine lines[SCATTER_LINES_MAX];
    int count = 0, pieces = 0;
    for (lua_Integer i = 1; !reason; i++) {
        lua_rawgeti(L, 2, i);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            break;
        }
        if (count >= (int)(sizeof lines / sizeof lines[0])) {
            reason = "too_many_lines";
            lua_pop(L, 1);
            break;
        }
        ScatterLine *s = &lines[count];
        memset(s, 0, sizeof *s);
        s->terrain = terrain;
        if (!lua_istable(L, -1)) reason = "bad_list";
        if (!reason) {
            lua_getfield(L, -1, "pack");
            if (lua_type(L, -1) == LUA_TSTRING) snprintf(s->pack, sizeof s->pack, "%s", lua_tostring(L, -1)), snprintf(what, sizeof what, "%s", s->pack);
            else reason = "no_pack";
            lua_pop(L, 1);
        }
        if (!reason) {
            lua_getfield(L, -1, "center");
            s->center = lua_toboolean(L, -1);
            if (!lua_isnil(L, -1) && !lua_isboolean(L, -1)) reason = "bad_list";
            lua_pop(L, 1);
        }
        if (!reason && s->center) {
            int none = 0;
            if (!luaScatterNumber(L, "size", -1, &s->size)) reason = "bad_center_size";
            else if (!luaScatterNumber(L, "count", -2, &none) || none != -2 || !luaScatterNumber(L, "sway", -2, &none) || none != -2 || !luaScatterNumber(L, "shade", -2, &none) ||
                     none != -2 || !luaScatterNumber(L, "clearcenter", -2, &none) || none != -2)
                reason = "bad_numbers";
            s->count = 1, s->sway = s->shade = 0, s->clear = -1;
        } else if (!reason && (!luaScatterNumber(L, "count", -1, &s->count) || !luaScatterNumber(L, "size", -1, &s->size) || !luaScatterNumber(L, "sway", 100, &s->sway) ||
                               !luaScatterNumber(L, "shade", 50, &s->shade) || !luaScatterNumber(L, "clearcenter", -2, &s->clear)))
            reason = "bad_numbers";
        if (!reason && s->clear == -2) s->clear = -1;
        if (!reason) {
            lua_getfield(L, -1, "condition");
            s->condition = lua_isnil(L, -1) ? SCATTER_ANY : lua_type(L, -1) == LUA_TSTRING ? scatterConditionNamed(lua_tostring(L, -1)) : -1;
            if (s->condition < 0) reason = "bad_condition", snprintf(what, sizeof what, "%s", lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "?");
            lua_pop(L, 1);
        }
        if (!reason && !findSpritePack(mod ? mod : "?", s->pack)) reason = "no_pack";
        if (!reason) reason = scatterNumbersReason(s);
        if (!reason && s->condition != SCATTER_ANY && !pieces && lastScatterPiece(mod ? mod : "?", terrain, 1) < 0)
            reason = "no_piece_above", snprintf(what, sizeof what, "%s", SCATTER_CONDITION_WORDS[s->condition]);
        pieces += s->condition == SCATTER_ANY;
        lua_pop(L, 1);
        if (!reason) count++;
    }
    if (!reason && !count) reason = "bad_list";
    int state, most;
    if (!reason && scatterLinesOn(mod ? mod : "?", terrain) + count > SCATTER_LINES_MAX) reason = "too_many_lines";
    if (!reason && (most = mostScatterPieces(mod ? mod : "?", terrain, lines, count, &state)) > SCATTER_SQUARE_PIECES_MAX)
        reason = "too_many_pieces", writeScatterPiecesWhat(what, sizeof what, most, state);
    int line = luaCallerLine(L);
    if (!reason && !installTerrainDecorHook()) reason = "hook_not_installed";
    for (int i = 0; i < count && !reason; i++)
        if ((reason = declareScatterLine(mod ? mod : "?", &lines[i], 1, luaRunning ? luaRunning->path : "?", line))) snprintf(what, sizeof what, "%s", lines[i].pack);
    if (reason) refuseScatterCall(L, "coe5.setTerrainScatter", reason, what);
    lua_pushboolean(L, !reason);
    return 1;
}

static void terrainScatterWordsInit(void) {
    int words = apiAddModCommand("newspritepack", newspritepackLine) + apiAddModCommand("terrainscatter", terrainscatterLine) + apiAddModCommand("terraincenter", terraincenterLine);
    writeResultLog(words == 3, "terrain_scatter_words_installed", "words=%d of 3", words);
    installTreesDumpSwitch();
}
