#pragma once

#define TERRAIN_DUMP_SWITCH       "--dumpterrain"
#define TERRAIN_DUMP_FOLDER       "coe5se_terrain"
#define TERRAIN_DUMP_MAX          64
#define TERRAIN_DUMP_TOKEN_MAX    48
#define TERRAIN_DUMP_PATCH        8
#define TERRAIN_DUMP_RING         3
#define TERRAIN_DUMP_SHOWN        10
#define TERRAIN_DUMP_SQUARE_PX    48
#define TERRAIN_DUMP_STATES       4
#define TERRAIN_DUMP_SETTLE       3
#define TERRAIN_DUMP_WAIT_MAX     600
#define TERRAIN_DUMP_WARM         80
#define TERRAIN_DUMP_FROZEN       (-80)
#define TERRAIN_DUMP_MARGIN       16
#define TERRAIN_DUMP_LABEL        28
#define MAP_EDITOR_QUIT           1
#define GL_READ_VIEWPORT          0x0ba2
#define GL_READ_BACK              0x0405
#define GL_READ_RGBA              0x1908
#define GL_READ_UNSIGNED_BYTE     0x1401

static const char *const TERRAIN_DUMP_STATE_NAMES[TERRAIN_DUMP_STATES] = {"Plain", "Cold", "Snow", "Snow and cold"};
static const int TERRAIN_DUMP_STATE_COLD[TERRAIN_DUMP_STATES] = {0, 1, 0, 1};
static const int TERRAIN_DUMP_STATE_SNOW[TERRAIN_DUMP_STATES] = {0, 0, 1, 1};

enum { DUMP_LAYOUT, DUMP_SETTLE, DUMP_READ };
static struct {
    int listOpen;
    int count;
    char tokens[TERRAIN_DUMP_MAX][TERRAIN_DUMP_TOKEN_MAX];
    int started, running, quit;
    int terrains[TERRAIN_DUMP_MAX], terrainCount;
    int index, state, phase, frames, read;
    int x0, y0;
    int shotW, shotH;
    unsigned char *shots[TERRAIN_DUMP_STATES];
    unsigned long long startedMs;
    int written, failed;
} terrainDump;
static int terrainDumpSwitchHook;

static unsigned long long terrainDumpNowMs(void) {
#ifdef __APPLE__
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned long long)t.tv_sec * 1000u + (unsigned long long)t.tv_nsec / 1000000u;
#else
    return GetTickCount64();
#endif
}

// ---- The switch ----------------------------------------------------------------------------------
static int terrainDumpPrefixHook(const char *arg, const char *prefix, size_t n) {
    if (arg && terrainDump.listOpen && arg[0] != '-') return 0;
    if (arg) terrainDump.listOpen = !strcmp(arg, TERRAIN_DUMP_SWITCH);
    return strncmp(arg, prefix, n);
}

static int takeTerrainDumpArgument(const char *arg) {
    if (!arg) return 0;
    if (!strcmp(arg, TERRAIN_DUMP_SWITCH)) {
        writeLog("terrain_dump_asked", "switch=%s", arg);
        return 1;
    }
    if (arg[0] == '-' || !terrainDump.listOpen) return 0;
    if (terrainDump.count >= TERRAIN_DUMP_MAX || strlen(arg) >= TERRAIN_DUMP_TOKEN_MAX) {
        writeLogError("terrain_dump_refused", "reason=%s terrain=%.48s", terrainDump.count >= TERRAIN_DUMP_MAX ? "too_many" : "too_long", arg);
        return 1;
    }
    snprintf(terrainDump.tokens[terrainDump.count++], TERRAIN_DUMP_TOKEN_MAX, "%s", arg);
    return 1;
}

static void installTerrainDumpSwitch(void) {
    if (terrainDumpSwitchHook) return;
    int ok = pointCustomPlaneCall(RVA_CLI_SWITCH_PREFIX_CALL, RVA_STRNCMP, (void *)terrainDumpPrefixHook, "command_line_prefix");
    terrainDumpSwitchHook = ok ? 1 : -1;
    writeResultLog(ok, "terrain_dump_switch_installed", "ok=%d of 1", ok);
}

static int terrainDumpTerrain(const char *token) {
    int last = G32(RVA_LAST_TERRAIN);
    if (last >= TERRAIN_TABLE_MAX) last = TERRAIN_TABLE_MAX - 1;
    if (!token[0]) return -1;
    if (strspn(token, "0123456789") == strlen(token)) {
        long n = strtol(token, NULL, 10);
        return n <= last ? (int)n : -1;
    }
    for (int t = 0; t <= last; t++)
        if (!_stricmp((const char *)GAME_PTR(RVA_TERRAIN_TABLE) + (size_t)t * TERRAIN_SIZE, token)) return t;
    return -1;
}

static void terrainDumpFileName(int terrain, const char *name, char *out, size_t size) {
    int at = snprintf(out, size, "%d_", terrain);
    for (const char *c = name; *c && at > 0 && (size_t)at + 5 < size; c++)
        out[at++] = isalnum((unsigned char)*c) ? *c : '_';
    snprintf(out + at, size - (size_t)at, ".tga");
}

// ---- The main menu -------------------------------------------------------------------------------
static int terrainDumpDue(void) {
    if (terrainDump.started || !terrainDump.count) return 0;
    terrainDump.started = 1;
    terrainDump.terrainCount = 0;
    for (int i = 0; i < terrainDump.count; i++) {
        int t = terrainDumpTerrain(terrainDump.tokens[i]);
        if (t < 0) writeLogError("terrain_dump_refused", "reason=no_such_terrain terrain=%s", terrainDump.tokens[i]);
        else terrainDump.terrains[terrainDump.terrainCount++] = t;
    }
    if (!terrainDump.terrainCount) return 0;
    terrainDump.running = 1, terrainDump.index = terrainDump.state = 0, terrainDump.phase = DUMP_LAYOUT;
    terrainDump.startedMs = terrainDumpNowMs();
    writeLog("terrain_dump_started", "terrains=%d", terrainDump.terrainCount);
    return 1;
}

// ---- The editor ----------------------------------------------------------------------------------
static unsigned char *terrainDumpSquare(int x, int y) { return GAME_PTR(RVA_SQUARE_RECORDS) + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE; }

static int layTerrainDumpPatch(int terrain, int state) {
    const unsigned char *plane = planeRecord(0);
    int px = *(const int *)(plane + PLANE_X), py = *(const int *)(plane + PLANE_Y), pw = *(const int *)(plane + PLANE_W), ph = *(const int *)(plane + PLANE_H);
    int side = TERRAIN_DUMP_PATCH + 2 * TERRAIN_DUMP_RING;
    if (pw < side || ph < side || px + pw > G32(RVA_MAP_W) || py + ph > G32(RVA_MAP_H) || px + pw > MAP_COLUMNS_MAX || py + ph > MAP_COLUMN_SQUARES) return 0;
    terrainDump.x0 = px + (pw - TERRAIN_DUMP_PATCH) / 2, terrainDump.y0 = py + (ph - TERRAIN_DUMP_PATCH) / 2;
    int left = terrainDump.x0 - TERRAIN_DUMP_RING, top = terrainDump.y0 - TERRAIN_DUMP_RING;
    short temperature = (short)(TERRAIN_DUMP_STATE_SNOW[state] ? TERRAIN_DUMP_FROZEN : TERRAIN_DUMP_WARM);
    for (int x = left; x < left + side; x++)
        for (int y = top; y < top + side; y++) {
            unsigned char *square = terrainDumpSquare(x, y);
            int inside = x >= terrainDump.x0 && x < terrainDump.x0 + TERRAIN_DUMP_PATCH && y >= terrainDump.y0 && y < terrainDump.y0 + TERRAIN_DUMP_PATCH;
            short t = (short)(inside ? terrain : 0);
            memcpy(square + SQUARE_TERRAIN_TYPE, &t, 2);
            memcpy(square + SQUARE_TEMPERATURE, &temperature, 2);
            size_t flags = RVA_SQUARE_FLAGS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE;
            if (TERRAIN_DUMP_STATE_COLD[state]) G32(flags) |= SQUARE_FLAG_COLD;
            else G32(flags) &= ~SQUARE_FLAG_COLD;
        }
    GAME_FN(void (*)(int, int, int, int, int), RVA_SET_GROUND_RECT)(left, top, side, side, -1);
    GAME_FN(void (*)(int, int, int, int), RVA_ADJUST_FOR_SNOW)(left, top, side, side);
    G32(RVA_MAP_BG_DIRTY) = 1;
    return 1;
}

static void holdTerrainDumpView(void) {
    G32(RVA_MAP_VIEW_PLANE) = 0;
    GF(RVA_MAP_ZOOM) = (float)TERRAIN_DUMP_SQUARE_PX / uiScaleX();
    GF(RVA_MAP_ZOOM_SPEED) = 0;
    GF(RVA_MAP_CAMERA_X) = (float)(terrainDump.x0 + 4);
    GF(RVA_MAP_CAMERA_Y) = (float)(terrainDump.y0 + 3);
}

static void *terrainDumpGl(const char *name) {
#ifdef __APPLE__
    return dlsym(RTLD_DEFAULT, name);
#else
    HMODULE gl = GetModuleHandleA("opengl32.dll");
    return gl ? (void *)GetProcAddress(gl, name) : NULL;
#endif
}

static int readTerrainDumpShot(void) {
    MapView v;
    if (!currentMapView(&v)) return 0;
    float sx = uiScaleX(), sy = uiScaleY();
    double left = v.originX + (terrainDump.x0 - 1) * v.zoom, bottom = v.originY - (terrainDump.y0 + TERRAIN_DUMP_SHOWN - 1) * v.zoom;
    int x = (int)lround(left * sx), y = (int)lround(bottom * sy);
    int w = (int)lround(TERRAIN_DUMP_SHOWN * v.zoom * sx), h = (int)lround(TERRAIN_DUMP_SHOWN * v.zoom * sy);
    void (*getIntegerv)(unsigned, int *) = (void (*)(unsigned, int *))terrainDumpGl("glGetIntegerv");
    void (*readBuffer)(unsigned) = (void (*)(unsigned))terrainDumpGl("glReadBuffer");
    void (*readPixels)(int, int, int, int, unsigned, unsigned, void *) = (void (*)(int, int, int, int, unsigned, unsigned, void *))terrainDumpGl("glReadPixels");
    int vp[4] = {0};
    if (getIntegerv) getIntegerv(GL_READ_VIEWPORT, vp);
    const char *reason = !getIntegerv || !readBuffer || !readPixels                     ? "no_gl"
                       : w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > vp[2] || y + h > vp[3] ? "outside_frame"
                       : terrainDump.shotW && (w != terrainDump.shotW || h != terrainDump.shotH) ? "size_changed"
                       : NULL;
    unsigned char *pixels = reason ? NULL : malloc((size_t)w * (size_t)h * 4);
    if (!reason && !pixels) reason = "out_of_memory";
    if (reason) {
        writeLogError("terrain_dump_read_failed", "reason=%s terrain=%d state=%d frame=%dx%d rect=%d,%d,%dx%d", reason, terrainDump.terrains[terrainDump.index],
                      terrainDump.state, vp[2], vp[3], x, y, w, h);
        return 0;
    }
    readBuffer(GL_READ_BACK);
    readPixels(vp[0] + x, vp[1] + y, w, h, GL_READ_RGBA, GL_READ_UNSIGNED_BYTE, pixels);
    free(terrainDump.shots[terrainDump.state]);
    terrainDump.shots[terrainDump.state] = pixels, terrainDump.shotW = w, terrainDump.shotH = h;
    return 1;
}

static void terrainDumpBeforeToolbox(void) {
    if (!terrainDump.running || terrainDump.phase != DUMP_READ || terrainDump.read) return;
    terrainDump.read = readTerrainDumpShot();
}

static int writeTerrainDumpSheet(int terrain) {
    static const unsigned char WHITE[3] = {240, 240, 240}, GREY[3] = {170, 170, 170};
    const char *name = (const char *)GAME_PTR(RVA_TERRAIN_TABLE) + (size_t)terrain * TERRAIN_SIZE;
    int w = terrainDump.shotW, h = terrainDump.shotH;
    TreeSheet s = {NULL, 2 * w + 3 * TERRAIN_DUMP_MARGIN, TERRAIN_DUMP_LABEL + 2 * (TERRAIN_DUMP_LABEL + h) + 3 * TERRAIN_DUMP_MARGIN};
    if (!w || !(s.rgba = calloc((size_t)s.w * (size_t)s.h, 4))) return 0;
    for (size_t i = 0; i < (size_t)s.w * (size_t)s.h; i++) s.rgba[4 * i] = s.rgba[4 * i + 1] = s.rgba[4 * i + 2] = 40, s.rgba[4 * i + 3] = 255;
    char title[TERRAIN_NAME_SIZE + 16];
    snprintf(title, sizeof title, "%d %.*s", terrain, TERRAIN_NAME_SIZE, name);
    sheetText(&s, TERRAIN_DUMP_MARGIN, TERRAIN_DUMP_MARGIN, title, WHITE);
    for (int k = 0; k < TERRAIN_DUMP_STATES; k++) {
        int left = TERRAIN_DUMP_MARGIN + (k % 2) * (w + TERRAIN_DUMP_MARGIN);
        int top = TERRAIN_DUMP_MARGIN + TERRAIN_DUMP_LABEL + (k / 2) * (TERRAIN_DUMP_LABEL + h + TERRAIN_DUMP_MARGIN);
        sheetText(&s, left, top + 6, TERRAIN_DUMP_STATE_NAMES[k], GREY);
        const unsigned char *shot = terrainDump.shots[k];
        for (int y = 0; shot && y < h; y++)
            for (int x = 0; x < w; x++) {
                const unsigned char *p = shot + ((size_t)(h - 1 - y) * (size_t)w + (size_t)x) * 4;
                unsigned char *q = s.rgba + ((size_t)(top + TERRAIN_DUMP_LABEL + y) * (size_t)s.w + (size_t)(left + x)) * 4;
                q[0] = p[0], q[1] = p[1], q[2] = p[2], q[3] = 255;
            }
    }
    char folder[MAX_PATH], file[96], path[MAX_PATH];
    int ok = buildPath(folder, sizeof folder, __func__, "%s" USER_DIR_SEP TERRAIN_DUMP_FOLDER, userDir());
    if (ok) CreateDirectoryA(folder, NULL);
    terrainDumpFileName(terrain, name, file, sizeof file);
    ok = ok && buildPath(path, sizeof path, __func__, "%s" USER_DIR_SEP "%s", folder, file) && writeScatterTgaRows(path, s.w, s.h, s.rgba, 0);
    if (ok) writeLog("terrain_dumped", "terrain=%d name=%.*s file=%s size=%dx%d", terrain, TERRAIN_NAME_SIZE, name, path, s.w, s.h);
    free(s.rgba);
    return ok;
}

static void finishTerrainDump(void) {
    terrainDump.running = 0, terrainDump.quit = 1;
    for (int k = 0; k < TERRAIN_DUMP_STATES; k++) free(terrainDump.shots[k]), terrainDump.shots[k] = NULL;
    writeResultLog(!terrainDump.failed, "terrain_dump_finished", "written=%d failed=%d ms=%llu folder=" TERRAIN_DUMP_FOLDER, terrainDump.written, terrainDump.failed,
                   terrainDumpNowMs() - terrainDump.startedMs);
}

static void terrainDumpBeforeEditorFrame(void) {
    if (!terrainDump.running) return;
    if (terrainDump.phase == DUMP_LAYOUT) {
        if (!layTerrainDumpPatch(terrainDump.terrains[terrainDump.index], terrainDump.state)) {
            writeLogError("terrain_dump_refused", "reason=plane_too_small terrain=%d", terrainDump.terrains[terrainDump.index]);
            terrainDump.failed++;
            finishTerrainDump();
            return;
        }
        terrainDump.phase = DUMP_SETTLE, terrainDump.frames = 0, terrainDump.read = 0;
    } else if (terrainDump.phase == DUMP_SETTLE) {
        int busy = *(volatile unsigned char *)GAME_PTR(RVA_EDITOR_BG_THREAD_BUSY);
        if (++terrainDump.frames > TERRAIN_DUMP_WAIT_MAX) writeLogError("terrain_dump_late", "reason=ground_not_built terrain=%d state=%d", terrainDump.terrains[terrainDump.index], terrainDump.state);
        if ((terrainDump.frames >= TERRAIN_DUMP_SETTLE && !busy) || terrainDump.frames > TERRAIN_DUMP_WAIT_MAX) terrainDump.phase = DUMP_READ, terrainDump.frames = 0;
    }
    holdTerrainDumpView();
}

static int terrainDumpAfterEditorFrame(int id) {
    if (terrainDump.quit) {
        terrainDump.quit = 0;
        return MAP_EDITOR_QUIT;
    }
    if (!terrainDump.running) return id;
    if (terrainDump.phase == DUMP_READ && (terrainDump.read || ++terrainDump.frames > TERRAIN_DUMP_SETTLE)) {
        if (!terrainDump.read) terrainDump.failed++;
        terrainDump.phase = DUMP_LAYOUT;
        if (++terrainDump.state == TERRAIN_DUMP_STATES) {
            int terrain = terrainDump.terrains[terrainDump.index];
            if (writeTerrainDumpSheet(terrain)) terrainDump.written++;
            else terrainDump.failed++, writeLogError("terrain_dump_refused", "reason=sheet_not_written terrain=%d", terrain);
            for (int k = 0; k < TERRAIN_DUMP_STATES; k++) free(terrainDump.shots[k]), terrainDump.shots[k] = NULL;
            terrainDump.shotW = terrainDump.shotH = 0, terrainDump.state = 0;
            if (++terrainDump.index == terrainDump.terrainCount) {
                finishTerrainDump();
                terrainDump.quit = 0;
                return MAP_EDITOR_QUIT;
            }
        }
    }
    return MAP_EDITOR_FREE_ID_FIRST;
}

static void terrainDumpInit(void) { installTerrainDumpSwitch(); }
