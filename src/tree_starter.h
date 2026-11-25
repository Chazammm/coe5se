#define TREES_DUMP_FOLDER        "coe5se_trees"
#define TREES_DUMP_SWITCH        "--dumptrees"
#define TREES_STARTER_MOD        "TreeStarter"
#define TREES_SHEET_FILE         "trees.tga"

enum { TREE_FOREST, TREE_COLD, TREE_SNOW, TREE_SNOW_COLD, TREE_DEAD, TREE_VERSIONS };
static const char *const TREE_VERSION_KEYS[TREE_VERSIONS] = {"forest", "cold", "snow", "snowcold", "dead"};
static const short TREE_VERSION_FIRST[TREE_VERSIONS] = {16, 22, 96, 102, 38};
static const short TREE_VERSION_LAST[TREE_VERSIONS] = {20, 27, 101, 107, 39};
static const short TREE_VERSION_SCATTER[TREE_VERSIONS][4] = {{40, 50, 100, 50}, {35, 75, 100, 50}, {40, 50, 100, 50}, {35, 75, 100, 50}, {10, 70, 0, 50}};
static const int TREE_VERSION_CONDITION[TREE_VERSIONS] = {SCATTER_ANY, SCATTER_COLD, SCATTER_SNOW, SCATTER_SNOW_COLD, SCATTER_ANY};
static const char *const TREE_SHEET_TITLES[TREE_VERSIONS] = {
    "Forest and Enchanted Forest: the line with no word",
    "Forest on a cold square: the line marked cold",
    "Forest on snow ground (winter): the line marked snow",
    "Snow ground on a cold square: the line marked snowcold",
    "Dead Forest: its own terrain's line",
};
static const char *const TREE_SHEET_RULES[TREE_VERSIONS] = {
    "terrainscatter 40 50 100 50: 40 a square, height 50% of a square, sway 100%, shade 50%",
    "terrainscatter 35 75 100 50 cold: 35 a square, height 75%",
    "terrainscatter 40 50 100 50 snow: 40 a square, height 50%",
    "terrainscatter 35 75 100 50 snowcold: 35 a square, height 75%",
    "terrainscatter 10 70 0 50: 10 a square, height 70%, no sway",
};

static void trsPixel(const unsigned char *at, int alpha, unsigned char out[4]) {
    unsigned word = (unsigned)at[0] << 8 | at[1];
    unsigned char r = at[0] & 0xf8, g = (unsigned char)((word >> 3) & 0xfc), b = (unsigned char)(at[1] << 3);
    if (alpha >= 0) out[0] = r, out[1] = g, out[2] = b, out[3] = (unsigned char)alpha;
    else if (!r && !g && !b) out[0] = out[1] = out[2] = out[3] = 0;
    else if (r > 0xf5 && !g && b > 0xf5) out[0] = out[1] = out[2] = 0, out[3] = 0x80;
    else out[0] = r, out[1] = g, out[2] = b, out[3] = 0xff;
    if (out[3] == 0xff && out[0] < 2 && out[1] < 2 && out[2] < 2) out[0] = out[1] = out[2] = out[3] = 0;
}

static int trsCount(const unsigned char *data, size_t size, size_t *pos, unsigned *out) {
    if (*pos >= size) return 0;
    if (data[*pos] != 0xff) {
        *out = data[(*pos)++];
        return 1;
    }
    if (*pos + 4 > size) return 0;
    *out = (unsigned)data[*pos + 1] << 16 | (unsigned)data[*pos + 2] << 8 | data[*pos + 3];
    *pos += 4;
    return 1;
}

static int decodeTrsSprite(const unsigned char *data, size_t size, int sprite, int *w, int *h, unsigned char **rgba) {
    if (size < 12 || memcmp(data, "TCS", 3)) return 0;
    int count = data[4] << 8 | data[5], format = data[6] << 8 | data[7];
    if (sprite >= count || format < 4) return 0;
    size_t row = 12 + (size_t)sprite * 12;
    if (row + 12 > size) return 0;
    *w = data[row] ? data[row] : 256, *h = data[row + 1] ? data[row + 1] : 256;
    size_t raw = (size_t)data[row + 4] << 24 | (size_t)data[row + 5] << 16 | (size_t)data[row + 6] << 8 | data[row + 7];
    size_t coded = (size_t)data[row + 8] << 24 | (size_t)data[row + 9] << 16 | (size_t)data[row + 10] << 8 | data[row + 11];
    size_t pixels = (size_t)*w * (size_t)*h;
    *rgba = calloc(pixels, 4);
    if (!*rgba) return 0;
    if (raw) {
        if (raw + pixels * 2 > size) return free(*rgba), 0;
        for (size_t i = 0; i < pixels; i++) trsPixel(data + raw + 2 * i, -1, *rgba + 4 * i);
        return 1;
    }
    if (coded + 4 > size) return free(*rgba), 0;
    int mode = data[coded] << 8 | data[coded + 1];
    unsigned runs = (unsigned)data[coded + 2] << 8 | data[coded + 3];
    size_t pos = coded + 4, index = 0, step = mode == 1 ? 3 : 2;
    for (unsigned r = 0; r < runs; r++) {
        unsigned skip, length;
        if (!trsCount(data, size, &pos, &skip) || !trsCount(data, size, &pos, &length) || pos + (size_t)length * step > size) return free(*rgba), 0;
        index += skip;
        for (unsigned k = 0; k < length; k++, index++, pos += step)
            if (index < pixels) trsPixel(data + pos, mode == 1 ? data[pos + 2] : -1, *rgba + 4 * index);
    }
    return 1;
}

static int writeTreeTga(const char *path, int w, int h, const unsigned char *rgba) { return writeScatterTgaRows(path, w, h, rgba, 0); }

static int fileExists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) fclose(f);
    return f != NULL;
}

// ---- The sheet: each version's trees in a row, with their numbers and the game's rule ------------
static const unsigned char TREE_SHEET_FONT[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5f, 0x00, 0x00}, {0x00, 0x07, 0x00, 0x07, 0x00}, {0x14, 0x7f, 0x14, 0x7f, 0x14},
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62}, {0x36, 0x49, 0x56, 0x20, 0x50}, {0x00, 0x08, 0x07, 0x03, 0x00},
    {0x00, 0x1c, 0x22, 0x41, 0x00}, {0x00, 0x41, 0x22, 0x1c, 0x00}, {0x2a, 0x1c, 0x7f, 0x1c, 0x2a}, {0x08, 0x08, 0x3e, 0x08, 0x08},
    {0x00, 0x80, 0x70, 0x30, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08}, {0x00, 0x00, 0x60, 0x60, 0x00}, {0x20, 0x10, 0x08, 0x04, 0x02},
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0x00, 0x42, 0x7f, 0x40, 0x00}, {0x72, 0x49, 0x49, 0x49, 0x46}, {0x21, 0x41, 0x49, 0x4d, 0x33},
    {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39}, {0x3c, 0x4a, 0x49, 0x49, 0x31}, {0x41, 0x21, 0x11, 0x09, 0x07},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x46, 0x49, 0x49, 0x29, 0x1e}, {0x00, 0x00, 0x14, 0x00, 0x00}, {0x00, 0x40, 0x34, 0x00, 0x00},
    {0x00, 0x08, 0x14, 0x22, 0x41}, {0x14, 0x14, 0x14, 0x14, 0x14}, {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x59, 0x09, 0x06},
    {0x3e, 0x41, 0x5d, 0x59, 0x4e}, {0x7c, 0x12, 0x11, 0x12, 0x7c}, {0x7f, 0x49, 0x49, 0x49, 0x36}, {0x3e, 0x41, 0x41, 0x41, 0x22},
    {0x7f, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x49, 0x49, 0x49, 0x41}, {0x7f, 0x09, 0x09, 0x09, 0x01}, {0x3e, 0x41, 0x41, 0x51, 0x73},
    {0x7f, 0x08, 0x08, 0x08, 0x7f}, {0x00, 0x41, 0x7f, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3f, 0x01}, {0x7f, 0x08, 0x14, 0x22, 0x41},
    {0x7f, 0x40, 0x40, 0x40, 0x40}, {0x7f, 0x02, 0x1c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f}, {0x3e, 0x41, 0x41, 0x41, 0x3e},
    {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e}, {0x7f, 0x09, 0x19, 0x29, 0x46}, {0x26, 0x49, 0x49, 0x49, 0x32},
    {0x03, 0x01, 0x7f, 0x01, 0x03}, {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x03, 0x04, 0x78, 0x04, 0x03}, {0x61, 0x59, 0x49, 0x4d, 0x43}, {0x00, 0x7f, 0x41, 0x41, 0x41},
    {0x02, 0x04, 0x08, 0x10, 0x20}, {0x00, 0x41, 0x41, 0x41, 0x7f}, {0x04, 0x02, 0x01, 0x02, 0x04}, {0x40, 0x40, 0x40, 0x40, 0x40},
    {0x00, 0x03, 0x07, 0x08, 0x00}, {0x20, 0x54, 0x54, 0x78, 0x40}, {0x7f, 0x28, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x28},
    {0x38, 0x44, 0x44, 0x28, 0x7f}, {0x38, 0x54, 0x54, 0x54, 0x18}, {0x00, 0x08, 0x7e, 0x09, 0x02}, {0x18, 0xa4, 0xa4, 0x9c, 0x78},
    {0x7f, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7d, 0x40, 0x00}, {0x20, 0x40, 0x40, 0x3d, 0x00}, {0x7f, 0x10, 0x28, 0x44, 0x00},
    {0x00, 0x41, 0x7f, 0x40, 0x00}, {0x7c, 0x04, 0x78, 0x04, 0x78}, {0x7c, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38},
    {0xfc, 0x18, 0x24, 0x24, 0x18}, {0x18, 0x24, 0x24, 0x18, 0xfc}, {0x7c, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x24},
    {0x04, 0x04, 0x3f, 0x44, 0x24}, {0x3c, 0x40, 0x40, 0x20, 0x7c}, {0x1c, 0x20, 0x40, 0x20, 0x1c}, {0x3c, 0x40, 0x30, 0x40, 0x3c},
    {0x44, 0x28, 0x10, 0x28, 0x44}, {0x4c, 0x90, 0x90, 0x90, 0x7c}, {0x44, 0x64, 0x54, 0x4c, 0x44}, {0x00, 0x08, 0x36, 0x41, 0x00},
    {0x00, 0x00, 0x77, 0x00, 0x00}, {0x00, 0x41, 0x36, 0x08, 0x00}, {0x02, 0x01, 0x02, 0x04, 0x02},
};
enum { SHEET_SCALE = 2, SHEET_ADVANCE = 6 * SHEET_SCALE, SHEET_MARGIN = 16, SHEET_CELL = 128, SHEET_GAP = 8, SHEET_ROW = 24 + 22 + SHEET_CELL + 6 + 18 + 14 };

typedef struct {
    unsigned char *rgba;
    int w, h;
} TreeSheet;

static void sheetText(TreeSheet *s, int x, int y, const char *text, const unsigned char colour[3]) {
    for (const char *c = text; *c; c++, x += SHEET_ADVANCE) {
        int ch = *c < 32 || *c > 126 ? '?' : *c;
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 8; row++) {
                if (!(TREE_SHEET_FONT[ch - 32][col] >> row & 1)) continue;
                for (int dy = 0; dy < SHEET_SCALE; dy++)
                    for (int dx = 0; dx < SHEET_SCALE; dx++) {
                        int px = x + col * SHEET_SCALE + dx, py = y + row * SHEET_SCALE + dy;
                        if (px < 0 || py < 0 || px >= s->w || py >= s->h) continue;
                        unsigned char *q = s->rgba + ((size_t)py * (size_t)s->w + (size_t)px) * 4;
                        q[0] = colour[0], q[1] = colour[1], q[2] = colour[2];
                    }
            }
    }
}

static void sheetTree(TreeSheet *s, int x, int y, const unsigned char *rgba, int w, int h) {
    for (int py = 0; py < SHEET_CELL; py++)
        for (int px = 0; px < SHEET_CELL; px++) {
            unsigned char *q = s->rgba + ((size_t)(y + py) * (size_t)s->w + (size_t)(x + px)) * 4;
            q[0] = q[1] = q[2] = (px / 8 + py / 8) % 2 ? 90 : 120;
        }
    int zoom = w * 2 <= SHEET_CELL && h * 2 <= SHEET_CELL ? 2 : 1;
    int left = (SHEET_CELL - w * zoom) / 2, top = SHEET_CELL - h * zoom;
    for (int py = 0; py < h * zoom; py++)
        for (int px = 0; px < w * zoom; px++) {
            if (left + px < 0 || left + px >= SHEET_CELL || top + py < 0) continue;
            const unsigned char *p = rgba + ((size_t)(py / zoom) * (size_t)w + (size_t)(px / zoom)) * 4;
            unsigned char *q = s->rgba + ((size_t)(y + top + py) * (size_t)s->w + (size_t)(x + left + px)) * 4;
            for (int c = 0; c < 3; c++) q[c] = (unsigned char)((p[c] * p[3] + q[c] * (255 - p[3])) / 255);
        }
}

static int writeTreeSheet(const char *path, const unsigned char *data, size_t size) {
    static const unsigned char WHITE[3] = {240, 240, 240}, GREY[3] = {170, 170, 170};
    TreeSheet s = {NULL, 0, TREE_VERSIONS * SHEET_ROW + SHEET_MARGIN};
    int most = 0;
    for (int v = 0; v < TREE_VERSIONS; v++) {
        int cells = TREE_VERSION_LAST[v] - TREE_VERSION_FIRST[v] + 1;
        int wide = cells * (SHEET_CELL + SHEET_GAP), title = (int)strlen(TREE_SHEET_TITLES[v]) * SHEET_ADVANCE, rule = (int)strlen(TREE_SHEET_RULES[v]) * SHEET_ADVANCE;
        most = wide > most ? wide : most, most = title > most ? title : most, most = rule > most ? rule : most;
    }
    s.w = most + 2 * SHEET_MARGIN;
    s.rgba = malloc((size_t)s.w * (size_t)s.h * 4);
    if (!s.rgba) return 0;
    for (size_t i = 0; i < (size_t)s.w * (size_t)s.h; i++) s.rgba[4 * i] = s.rgba[4 * i + 1] = s.rgba[4 * i + 2] = 36, s.rgba[4 * i + 3] = 255;
    for (int v = 0; v < TREE_VERSIONS; v++) {
        int y = SHEET_MARGIN + v * SHEET_ROW;
        sheetText(&s, SHEET_MARGIN, y, TREE_SHEET_TITLES[v], WHITE);
        sheetText(&s, SHEET_MARGIN, y + 24, TREE_SHEET_RULES[v], GREY);
        for (int sprite = TREE_VERSION_FIRST[v]; sprite <= TREE_VERSION_LAST[v]; sprite++) {
            int x = SHEET_MARGIN + (sprite - TREE_VERSION_FIRST[v]) * (SHEET_CELL + SHEET_GAP), w, h;
            unsigned char *rgba = NULL;
            if (decodeTrsSprite(data, size, sprite, &w, &h, &rgba)) sheetTree(&s, x, y + 46, rgba, w, h);
            free(rgba);
            char number[16];
            snprintf(number, sizeof number, "%d", sprite);
            sheetText(&s, x, y + 46 + SHEET_CELL + 6, number, WHITE);
        }
    }
    int ok = writeTreeTga(path, s.w, s.h, s.rgba);
    free(s.rgba);
    return ok;
}

// ---- The starter mod ---------------------------------------------------------------------------
static void writeTreeStarterScatter(FILE *f) {
    for (int v = 0; v < TREE_VERSIONS; v++) fprintf(f, "#!@ newspritepack \"Starter %s\"\n#!@ spr \"./%s/\"\n", TREE_VERSION_KEYS[v], TREE_VERSION_KEYS[v]);
    static const int TERRAINS[3] = {TERRAIN_FOREST, TERRAIN_ENCHANTED_FOREST, TERRAIN_DEAD_FOREST};
    for (int t = 0; t < 3; t++) {
        fprintf(f, "\nselectterr %d\n", TERRAINS[t]);
        for (int v = 0; v < TREE_VERSIONS; v++) {
            if ((v == TREE_DEAD) != (TERRAINS[t] == TERRAIN_DEAD_FOREST)) continue;
            const short *n = TREE_VERSION_SCATTER[v];
            int c = TREE_VERSION_CONDITION[v];
            fprintf(f, "#!@ terrainscatter \"Starter %s\" %d %d %d %d%s%s\n", TREE_VERSION_KEYS[v], n[0], n[1], n[2], n[3], c ? " " : "", SCATTER_CONDITION_WORDS[c]);
        }
    }
}

static int writeTreeStarterC5m(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fprintf(f, "icon \"./banner.tga\"\ndescription \"The game's own Forest and Dead Forest trees as sprite packs, written by CoE5SE's --dumptrees. Paint over the pictures in its folders to make your own.\"\nneeds_script_extender\n\n");
    writeTreeStarterScatter(f);
    return fclose(f) == 0;
}

static int writeTreeStarterBanner(const char *path, const unsigned char *data, size_t size) {
    enum { BANNER_W = 256, BANNER_H = 64 };
    unsigned char *banner = malloc((size_t)BANNER_W * BANNER_H * 4);
    if (!banner) return 0;
    for (int i = 0; i < BANNER_W * BANNER_H; i++) banner[4 * i] = 24, banner[4 * i + 1] = 48, banner[4 * i + 2] = 28, banner[4 * i + 3] = 255;
    for (int s = TREE_VERSION_FIRST[TREE_FOREST]; s <= TREE_VERSION_LAST[TREE_FOREST]; s++) {
        int w, h, left = 8 + (s - TREE_VERSION_FIRST[TREE_FOREST]) * 50;
        unsigned char *rgba = NULL;
        if (!decodeTrsSprite(data, size, s, &w, &h, &rgba)) continue;
        for (int y = 0; y < h && y < BANNER_H; y++)
            for (int x = 0; x < w && left + x < BANNER_W; x++) {
                const unsigned char *p = rgba + 4 * ((size_t)(h - 1 - y) * (size_t)w + (size_t)x);
                unsigned char *q = banner + 4 * ((size_t)(BANNER_H - 1 - y) * BANNER_W + (size_t)(left + x));
                for (int c = 0; c < 3; c++) q[c] = (unsigned char)((p[c] * p[3] + q[c] * (255 - p[3])) / 255);
            }
        free(rgba);
    }
    int ok = writeTreeTga(path, BANNER_W, BANNER_H, banner);
    free(banner);
    return ok;
}

static void dumpTreesInto(const char *folder, const unsigned char *data, size_t size) {
    char path[MAX_PATH];
    int written = 0, kept = 0, failed = 0;
    for (int v = 0; v < TREE_VERSIONS; v++) {
        if (!buildPath(path, sizeof path, __func__, "%s" USER_DIR_SEP "%s", folder, TREE_VERSION_KEYS[v])) continue;
        CreateDirectoryA(path, NULL);
        for (int s = TREE_VERSION_FIRST[v]; s <= TREE_VERSION_LAST[v]; s++) {
            if (!buildPath(path, sizeof path, __func__, "%s" USER_DIR_SEP "%s" USER_DIR_SEP "%d.tga", folder, TREE_VERSION_KEYS[v], s)) continue;
            if (fileExists(path)) {
                kept++;
                continue;
            }
            int w, h;
            unsigned char *rgba = NULL;
            if (decodeTrsSprite(data, size, s, &w, &h, &rgba) && writeTreeTga(path, w, h, rgba)) written++;
            else failed++, writeLogError("tree_not_dumped", "reason=decode_or_write_failed sprite=%d path=%s", s, path);
            free(rgba);
        }
    }
    static const char *const STARTER_FILES[3] = {TREES_STARTER_MOD ".c5m", "banner.tga", TREES_SHEET_FILE};
    for (int k = 0; k < 3; k++) {
        if (!buildPath(path, sizeof path, __func__, "%s" USER_DIR_SEP "%s", folder, STARTER_FILES[k])) continue;
        if (fileExists(path)) kept++;
        else if (k == 0 ? writeTreeStarterC5m(path) : k == 1 ? writeTreeStarterBanner(path, data, size) : writeTreeSheet(path, data, size)) written++;
        else failed++, writeLogError("tree_not_dumped", "reason=write_failed path=%s", path);
    }
    writeResultLog(!failed, "trees_dumped", "written=%d kept=%d failed=%d folder=%s", written, kept, failed, folder);
}

// ---- The switch ----------------------------------------------------------------------------------
static int treesDumpAsked;
static int treesDumpSwitchHook;
typedef void (*CommandLineSwitchFn)(const char *arg);

static int takeTerrainDumpArgument(const char *arg);
static void commandLineSwitchHook(const char *arg) {
    if (takeTerrainDumpArgument(arg)) return;
    if (arg && !strcmp(arg, TREES_DUMP_SWITCH)) {
        treesDumpAsked = 1;
        writeLog("trees_dump_asked", "switch=%s", arg);
        return;
    }
    GAME_FN(CommandLineSwitchFn, RVA_COMMAND_LINE_SWITCH)(arg);
}

static void installTreesDumpSwitch(void) {
    if (treesDumpSwitchHook) return;
    int ok = pointCustomPlaneCall(RVA_CLI_SWITCH_WHOLE_CALL, RVA_COMMAND_LINE_SWITCH, (void *)commandLineSwitchHook, "command_line_switch");
    treesDumpSwitchHook = ok ? 1 : -1;
    writeResultLog(ok, "trees_dump_switch_installed", "ok=%d of 1", ok);
}

static void dumpTreesOnce(void) {
    static int done;
    if (done) return;
    done = 1;
    if (!treesDumpAsked) return;
    char path[MAX_PATH], folder[MAX_PATH];
    if (!buildPath(path, sizeof path, __func__, "%s/tree.trs", (const char *)GAME_PTR(RVA_DATA_DIR))) return;
    FILE *in = fopen(path, "rb");
    unsigned char *data = NULL;
    long size = -1;
    if (in && !fseek(in, 0, SEEK_END) && (size = ftell(in)) > 0 && !fseek(in, 0, SEEK_SET) && (data = malloc((size_t)size)) && fread(data, 1, (size_t)size, in) != (size_t)size) size = -1;
    if (in) fclose(in);
    if (!data || size <= 0) {
        free(data);
        writeLogError("trees_not_dumped", "reason=no_tree_trs path=%s", path);
        return;
    }
    if (buildPath(folder, sizeof folder, __func__, "%s" USER_DIR_SEP TREES_DUMP_FOLDER, userDir())) CreateDirectoryA(folder, NULL);
    if (buildPath(folder, sizeof folder, __func__, "%s" USER_DIR_SEP TREES_DUMP_FOLDER USER_DIR_SEP TREES_STARTER_MOD, userDir())) {
        CreateDirectoryA(folder, NULL);
        dumpTreesInto(folder, data, (size_t)size);
    }
    free(data);
}
