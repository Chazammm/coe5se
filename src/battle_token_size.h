#pragma once

#define BATTLE_SIZE_WORD       "battlesize"
#define BATTLE_TOKEN_SIZE_MAX  12
#define BATTLE_TOKEN_GAME_MAX  3
#define BATTLE_TOKEN_HUGE_AREA 9
#define BATTLE_COLUMN_SIZE     (BATTLE_ROWS * BATTLE_CELL_SIZE)
#define BATTLE_CELL_SECOND     4
#define MONSTER_FLAG_SIZE1X1   (1ULL << 52)
#define MONSTER_FLAG_SIZE2X2   (1ULL << 53)
#define MONSTER_FLAG_LOOKBACK_SKIP (1ULL << 62)
#define MONSTER_MOVE_HUGE      0x2ULL
#define SPRITE_PX_SIZE_3       151
#define SPRITE_PX_SIZE_2       101

enum { BATTLE_AXIS_WIDTH, BATTLE_AXIS_HEIGHT };

static struct {
    unsigned char *sizes;
    int capacity;
    int count;
    int largest;
} battleTokens;

static int battleTokenSizeOf(int monster, int axis) {
    if (monster < 0 || monster >= battleTokens.capacity || !battleTokens.sizes) return 0;
    return battleTokens.sizes[2 * monster + axis];
}

static int battlesizeWidth(int monster) { return battleTokenSizeOf(monster, BATTLE_AXIS_WIDTH); }
static int battlesizeHeight(int monster) { return battleTokenSizeOf(monster, BATTLE_AXIS_HEIGHT); }

static int battleTokenReach(void) { return battleTokens.largest > BATTLE_TOKEN_GAME_MAX ? battleTokens.largest : BATTLE_TOKEN_GAME_MAX; }

static int setBattleTokenSize(int monster, int width, int height) {
    if (monster >= battleTokens.capacity) {
        if (!width) return 1;
        int capacity = battleTokens.capacity ? battleTokens.capacity : 1024;
        while (capacity <= monster) capacity *= 2;
        unsigned char *grown = realloc(battleTokens.sizes, (size_t)capacity * 2);
        if (!grown) return 0;
        memset(grown + (size_t)battleTokens.capacity * 2, 0, (size_t)(capacity - battleTokens.capacity) * 2);
        battleTokens.sizes = grown;
        battleTokens.capacity = capacity;
    }
    unsigned char *at = battleTokens.sizes + 2 * monster;
    battleTokens.count += (width != 0) - (at[0] != 0);
    at[0] = (unsigned char)width;
    at[1] = (unsigned char)height;
    if (width > battleTokens.largest) battleTokens.largest = width;
    if (height > battleTokens.largest) battleTokens.largest = height;
    return 1;
}

static int readBattleTokenSize(const char *args, int *width, int *height) {
    const char *p = args;
    skipSpaces(&p);
    if (!readInt(&p, width)) return 0;
    if (*width == 0 && isLineEnd(p)) return *height = 0, 1;
    if (!readInt(&p, height) || !isLineEnd(p)) return 0;
    return *width >= 1 && *width <= BATTLE_TOKEN_SIZE_MAX && *height >= 1 && *height <= BATTLE_TOKEN_SIZE_MAX;
}

static void battleSizeLine(const char *args, int mod, int level) {
    if (level != 2) return;
    int width, height;
    if (!readBattleTokenSize(args, &width, &height)) {
        refuseC5mLine(BATTLE_SIZE_WORD, spellModName(mod), "bad_value", "line=%d text=%.40s min=1 max=%d", G32(RVA_MOD_LINE), args,
                      BATTLE_TOKEN_SIZE_MAX);
        return;
    }
    int monster = G32(RVA_MONSTER_CURRENT);
    if (!monsterRecord(monster)) {
        refuseC5mLine(BATTLE_SIZE_WORD, spellModName(mod), "no_monster", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    if (!setBattleTokenSize(monster, width, height))
        refuseC5mLine(BATTLE_SIZE_WORD, spellModName(mod), "out_of_memory", "line=%d", G32(RVA_MOD_LINE));
}

static int copystatsSource(const char *args) {
    const char *p = args;
    skipSpaces(&p);
    if (*p == '"') {
        const char *end = strchr(p + 1, '"');
        char name[OFFSET_NAME_TEXT_MAX];
        if (!end || (size_t)(end - p - 1) >= sizeof name) return -1;
        snprintf(name, sizeof name, "%.*s", (int)(end - p - 1), p + 1);
        return monsterByName(name);
    }
    int n;
    return readInt(&p, &n) && isLineEnd(p) && monsterRecord(n) ? n : -1;
}

static void battleTokenLineHook(const char *line, size_t len, int level) {
    if (level != 2 || !battleTokens.count) return;
    int monster = G32(RVA_MONSTER_CURRENT);
    if (!monsterRecord(monster)) return;
    if (len == sizeof "clearspec" - 1 && !strncmp(line, "clearspec", len)) setBattleTokenSize(monster, 0, 0);
    else if (len == sizeof "copystats" - 1 && !strncmp(line, "copystats", len)) {
        int from = copystatsSource(line + len);
        if (from >= 0) setBattleTokenSize(monster, battleTokenSizeOf(from, BATTLE_AXIS_WIDTH), battleTokenSizeOf(from, BATTLE_AXIS_HEIGHT));
    }
}

static void battleTokenSizesBeforePass(int level) {
    if (level != 2) return;
    if (battleTokens.sizes) memset(battleTokens.sizes, 0, (size_t)battleTokens.capacity * 2);
    battleTokens.count = 0;
    battleTokens.largest = 0;
}

// ---- The game's size reads, in C -----------------------------------------------------------------
static void (*battleTokenSpriteSize)(int sprite, int *width, int *height);

static void gameSpriteSize(int sprite, int *width, int *height) {
    GAME_FN(void (*)(int, int *, int *, int *), RVA_MONSTER_SPRITE_SIZE)(sprite, width, height, NULL);
}

static int monsterBattleTokenSize(int monster, int axis) {
    int own = battleTokenSizeOf(monster, axis);
    if (own) return own;
    const unsigned char *record = monsterRecord(monster);
    if (!record) return 1;
    unsigned long long flags = *(const unsigned long long *)(record + (RVA_MONSTER_FLAGS - RVA_MONSTER_TABLE));
    if (flags & MONSTER_FLAG_SIZE1X1) return 1;
    if (flags & MONSTER_FLAG_SIZE2X2) return 2;
    if (*(const unsigned long long *)(record + MONSTER_MOVE_FLAGS) & MONSTER_MOVE_HUGE) return 3;
    int width = 0, height = 0;
    (battleTokenSpriteSize ? battleTokenSpriteSize : gameSpriteSize)(*(const int *)(record + (RVA_MONSTER_SPRITES - RVA_MONSTER_TABLE)),
                                                                     &width, &height);
    int px = axis == BATTLE_AXIS_WIDTH ? width : height;
    return px >= SPRITE_PX_SIZE_3 ? 3 : px >= SPRITE_PX_SIZE_2 ? 2 : 1;
}

static int battleUnitMonster(int unit) {
    return unit >= 0 && unit < UNIT_TABLE_MAX ? (short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE) : -1;
}

static int unitBattleTokenSize(int unit, int axis) {
    int monster = battleUnitMonster(unit);
    return monster < 0 ? 1 : monsterBattleTokenSize(monster, axis);
}

// ---- Who covers a square -------------------------------------------------------------------------
static int (*gameBattleUnitAt2)(int x, int y, int mode);

static int battleCellUnit(int x, int y, int layer) {
    return *(const int *)GAME_PTR(RVA_BATTLE_GRID + (size_t)x * BATTLE_COLUMN_SIZE + (size_t)y * BATTLE_CELL_SIZE + layer);
}

static int battleUnitCovering(int x, int y, int layer, int gameReach, int mode) {
    if (x < 0 || y < 0 || x >= G32(RVA_BATTLE_WIDTH) || y >= BATTLE_ROWS) return -1;
    int reach = battleTokens.largest > gameReach ? battleTokens.largest : gameReach;
    for (int dy = 0; dy < reach && y - dy >= 0; dy++)
        for (int dx = 0; dx < reach && x - dx >= 0; dx++) {
            int unit = battleCellUnit(x - dx, y - dy, layer);
            if (unit < 0) continue;
            int monster = battleUnitMonster(unit);
            if (monster < 0 && layer == BATTLE_CELL_SECOND && gameBattleUnitAt2) return gameBattleUnitAt2(x, y, mode);
            if (mode && monsterRecord(monster) &&
                (*(const unsigned long long *)(monsterRecord(monster) + (RVA_MONSTER_FLAGS - RVA_MONSTER_TABLE)) & MONSTER_FLAG_LOOKBACK_SKIP))
                continue;
            if ((dx >= gameReach || dy >= gameReach) && !battleTokenSizeOf(monster, BATTLE_AXIS_WIDTH)) continue;
            if (unitBattleTokenSize(unit, BATTLE_AXIS_WIDTH) > dx && unitBattleTokenSize(unit, BATTLE_AXIS_HEIGHT) > dy) return unit;
        }
    return -1;
}

static int battleUnitAt(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_BATTLE_WIDTH) || y >= BATTLE_ROWS) return -1;
    int unit = battleCellUnit(x, y, 0);
    return unit >= 0 ? unit : battleUnitCovering(x, y, 0, 3, 0);
}

static int battleUnitAt2(int x, int y, int mode) {
    return battleUnitCovering(x, y, BATTLE_CELL_SECOND, 2, mode);
}

// ---- The game's side: entry hooks, on only while a monster has a battlesize ----------------------
// @platform-split battle_size_hooks: Mac hooks the area, the two look-backs, the landing and the move, and patches 91 inlined size reads
#ifndef __APPLE__
#define BATTLE_TOKEN_HOOKS 8
static int (*gameUnitBattleHeight)(int unit), (*gameUnitBattleWidth)(int unit), (*gameMonsterBattleWidth)(int monster);
#else
#define BATTLE_TOKEN_HOOKS 5
#endif
static int (*gameUnitBattleArea)(int unit), (*gameBattleUnitAt)(int x, int y);
static int battleTokenHooksMade, battleTokenHooksOn;

#ifndef __APPLE__
static int unitBattleHeightHook(int unit) {
    int own = battleTokenSizeOf(battleUnitMonster(unit), BATTLE_AXIS_HEIGHT);
    return own ? own : gameUnitBattleHeight(unit);
}

static int unitBattleWidthHook(int unit) {
    int own = battleTokenSizeOf(battleUnitMonster(unit), BATTLE_AXIS_WIDTH);
    return own ? own : gameUnitBattleWidth(unit);
}

static int monsterBattleWidthHook(int monster) {
    int own = battleTokenSizeOf(monster, BATTLE_AXIS_WIDTH);
    return own ? own : gameMonsterBattleWidth(monster);
}
#endif

static int unitBattleAreaHook(int unit) {
    int monster = battleUnitMonster(unit), width = battleTokenSizeOf(monster, BATTLE_AXIS_WIDTH);
    return width ? width * battleTokenSizeOf(monster, BATTLE_AXIS_HEIGHT) : gameUnitBattleArea(unit);
}

#define BATTLE_TOKEN_AREA_MAX     (BATTLE_TOKEN_SIZE_MAX * BATTLE_TOKEN_SIZE_MAX)
static struct {
    int active, mover, count;
    int unit[BATTLE_TOKEN_AREA_MAX];
} battleCrowd;

static int battleUnitAtPushScan(int x, int y);
#ifndef __APPLE__
#define BATTLE_PUSH_SCAN_RETURN 5
#else
#define BATTLE_PUSH_SCAN_RETURN 4
#endif

static __attribute__((noinline)) int battleUnitAtHook(int x, int y) {
    if (!battleTokens.count) return gameBattleUnitAt(x, y);
    if (battleCrowd.active &&
        __builtin_return_address(0) == (void *)(gameBase + RVA_FIGHT_MOVE_PUSH_SCAN_CALL + BATTLE_PUSH_SCAN_RETURN))
        return battleUnitAtPushScan(x, y);
    return battleUnitAt(x, y);
}

static int battleUnitAt2Hook(int x, int y, int mode) {
    return battleTokens.count ? battleUnitAt2(x, y, mode) : gameBattleUnitAt2(x, y, mode);
}

// ---- A big token on a crowded field ---------------------------------------------------------------
#define BATTLE_GAME_DISPLACED_MAX 16
#define BATTLE_CELL_SLIDE         0xe
#define BATTLE_DEPLOY_PUSHED      6
#define BATTLE_DEPLOY_FIGHT_MOVE  3

typedef struct {
    int unit, x, y;
} BattleCovered;

static int battleUnitsUnder(int self, int x, int y, int width, int height, BattleCovered out[BATTLE_TOKEN_AREA_MAX]) {
    int n = 0;
    for (int cx = x; cx < x + width; cx++)
        for (int cy = y; cy < y + height; cy++) {
            int unit = battleUnitAt(cx, cy), seen = 0;
            if (unit < 0 || unit == self) continue;
            for (int i = 0; i < n && !seen; i++) seen = out[i].unit == unit;
            if (!seen && n < BATTLE_TOKEN_AREA_MAX) out[n++] = (BattleCovered){unit, cx, cy};
        }
    return n;
}

static int battleUnitAnchor(int unit, int *x, int *y) {
    for (int cx = 0; cx < G32(RVA_BATTLE_WIDTH); cx++)
        for (int cy = 0; cy < BATTLE_ROWS; cy++)
            if (battleCellUnit(cx, cy, 0) == unit) {
                if (x) *x = cx;
                if (y) *y = cy;
                return 1;
            }
    return 0;
}

static void (*battleRemoveFromFieldFn)(int unit), (*battleDeployUnitFn)(int unit, int x, int y, int mode);
static int (*battleCanStandFn)(int unit, int x, int y, int mode);

static void battleRemoveFromField(int unit) {
    (battleRemoveFromFieldFn ? battleRemoveFromFieldFn : GAME_FN(void (*)(int), RVA_BATTLE_REMOVE_FROM_FIELD))(unit);
}

static void battleDeployUnit(int unit, int x, int y, int mode) {
    (battleDeployUnitFn ? battleDeployUnitFn : GAME_FN(void (*)(int, int, int, int), RVA_BATTLE_DEPLOY_UNIT))(unit, x, y, mode);
}

static int battleCanStand(int unit, int x, int y, int mode) {
    return (battleCanStandFn ? battleCanStandFn : GAME_FN(int (*)(int, int, int, int), RVA_BATTLE_CAN_STAND))(unit, x, y, mode);
}

static void (*gameBattlePlaceUnit)(int unit, int x, int y);
static int (*gameBattleFightMove)(int unit, int x, int y, int mode, int mark);

static int battleTokenCovers(int unit, int x, int y, BattleCovered under[BATTLE_TOKEN_AREA_MAX]) {
    int monster = battleUnitMonster(unit), width = battleTokenSizeOf(monster, BATTLE_AXIS_WIDTH);
    if (!width || x < 0 || y < 0 || x >= G32(RVA_BATTLE_WIDTH) || y >= BATTLE_ROWS) return 0;
    return battleUnitsUnder(unit, x, y, width, battleTokenSizeOf(monster, BATTLE_AXIS_HEIGHT), under);
}

static void battlePlaceUnitHook(int unit, int x, int y) {
    BattleCovered under[BATTLE_TOKEN_AREA_MAX];
    int n = battleTokenCovers(unit, x, y, under);
    if (n <= BATTLE_GAME_DISPLACED_MAX) {
        gameBattlePlaceUnit(unit, x, y);
        return;
    }
    if (battleUnitAnchor(unit, NULL, NULL)) return;
    for (int i = 0; i < n; i++) battleRemoveFromField(under[i].unit);
    unsigned char *cell = battleCell(x, y);
    *(int *)cell = unit;
    memset(cell + BATTLE_CELL_SLIDE, 0, 4);
    for (int i = 0; i < n; i++) battleDeployUnit(under[i].unit, under[i].x, under[i].y, BATTLE_DEPLOY_PUSHED);
    writeLog("battle_size_crowd_placed", "unit=%d x=%d y=%d pushed=%d", unit, x, y, n);
}

static int battleCrowdHides(int unit) {
    if (unit == battleCrowd.mover) return 1;
    for (int i = 0; i < battleCrowd.count; i++)
        if (battleCrowd.unit[i] == unit) return 1;
    return 0;
}

static int battleUnitAtPushScan(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_BATTLE_WIDTH) || y >= BATTLE_ROWS) return -1;
    int reach = battleTokens.largest > BATTLE_TOKEN_GAME_MAX ? battleTokens.largest : BATTLE_TOKEN_GAME_MAX;
    for (int dy = 0; dy < reach && y - dy >= 0; dy++)
        for (int dx = 0; dx < reach && x - dx >= 0; dx++) {
            int unit = battleCellUnit(x - dx, y - dy, 0);
            if (unit < 0 || battleCrowdHides(unit)) continue;
            if ((dx >= BATTLE_TOKEN_GAME_MAX || dy >= BATTLE_TOKEN_GAME_MAX) && !battleTokenSizeOf(battleUnitMonster(unit), BATTLE_AXIS_WIDTH)) continue;
            if (unitBattleTokenSize(unit, BATTLE_AXIS_WIDTH) > dx && unitBattleTokenSize(unit, BATTLE_AXIS_HEIGHT) > dy) return unit;
        }
    return -1;
}

static int battleTokensMeet(int unit, int x, int y, int width, int height) {
    int ux, uy;
    if (!battleUnitAnchor(unit, &ux, &uy)) return 0;
    return ux < x + width && x < ux + unitBattleTokenSize(unit, BATTLE_AXIS_WIDTH) && uy < y + height &&
           y < uy + unitBattleTokenSize(unit, BATTLE_AXIS_HEIGHT);
}

static int battleFightMoveHook(int unit, int x, int y, int mode, int mark) {
    BattleCovered under[BATTLE_TOKEN_AREA_MAX];
    int n = battleTokenCovers(unit, x, y, under);
    if (n <= BATTLE_GAME_DISPLACED_MAX || battleCrowd.active || !battleCanStand(unit, x, y, mode))
        return gameBattleFightMove(unit, x, y, mode, mark);
    battleCrowd.active = 1;
    battleCrowd.mover = unit;
    battleCrowd.count = 0;
    for (int i = BATTLE_GAME_DISPLACED_MAX; i < n; i++) battleCrowd.unit[battleCrowd.count++] = under[i].unit;
    int fromX = -1, fromY = -1;
    if (!battleUnitAnchor(unit, &fromX, &fromY)) fromX = -1;
    int moved = gameBattleFightMove(unit, x, y, mode, mark), passes = 1;
    unsigned char *cell = battleCell(x, y);
    while (moved && battleCrowd.count > 0 && passes <= BATTLE_TOKEN_AREA_MAX) {
        int next = battleCrowd.count < BATTLE_GAME_DISPLACED_MAX ? battleCrowd.count : BATTLE_GAME_DISPLACED_MAX;
        battleCrowd.count -= next;
        memmove(battleCrowd.unit, battleCrowd.unit + next, (size_t)battleCrowd.count * sizeof battleCrowd.unit[0]);
        unsigned char slide[4];
        int delay = G32(RVA_BATTLE_DELAY);
        memcpy(slide, cell + BATTLE_CELL_SLIDE, 4);
        *(int *)cell = -1;
        if (fromX >= 0 && battleCellUnit(fromX, fromY, 0) < 0) *(int *)battleCell(fromX, fromY) = unit;
        G32(RVA_BATTLE_DELAY) = 0;
        gameBattleFightMove(unit, x, y, mode, 0);
        G32(RVA_BATTLE_DELAY) = delay;
        memcpy(cell + BATTLE_CELL_SLIDE, slide, 4);
        passes++;
    }
    battleCrowd.active = 0;
    int left = 0;
    if (moved) {
        int monster = battleUnitMonster(unit);
        for (int i = BATTLE_GAME_DISPLACED_MAX; i < n; i++)
            if (battleTokensMeet(under[i].unit, x, y, battleTokenSizeOf(monster, BATTLE_AXIS_WIDTH), battleTokenSizeOf(monster, BATTLE_AXIS_HEIGHT))) {
                battleRemoveFromField(under[i].unit);
                battleDeployUnit(under[i].unit, under[i].x, under[i].y, BATTLE_DEPLOY_FIGHT_MOVE);
                left++;
            }
    }
    writeLog("battle_size_crowd_moved", "unit=%d x=%d y=%d pushed=%d moves=%d moved=%d left=%d", unit, x, y, n, passes, moved, left);
    return moved;
}

#ifdef __APPLE__
// ---- Mac: the inlined size reads -------------------------------------------------------------------
enum { BATTLE_SIZE_N, BATTLE_SIZE_N_LESS_1, BATTLE_SIZE_MINUS_N, BATTLE_SIZE_64N };
#define BATTLE_SITE_NO_TBNZ 0xff
typedef struct {
    unsigned int site, join, below;
    unsigned char axis, encoding, threshold, monsterReg;
    signed char result, alsoReg, belowReg;
    signed char alsoFrom;
    unsigned char flagsReg;
    unsigned short alsoValue, belowValue;
    unsigned char saveScratch;
} BattleTokenSite;
#include "battle_token_sites_mac.h"

#define BATTLE_STUB_WORDS    84
#define BATTLE_STUB_FRAME    0x240
#define BATTLE_STUB_LITERALS 72
#define BATTLE_STUB_EXITS    4

static uint64_t battleTokenSiteValue(int monster, int axis, int encoding, int threshold) {
    int size = battleTokenSizeOf(monster, axis);
    if (!size) return 0;
    uint32_t value = encoding == BATTLE_SIZE_N_LESS_1 ? (uint32_t)(size - 1)
                   : encoding == BATTLE_SIZE_MINUS_N  ? (uint32_t)-size
                   : encoding == BATTLE_SIZE_64N      ? (uint32_t)(64 * size)
                                                      : (uint32_t)size;
    return 1ULL << 32 | (uint64_t)(size >= threshold) << 33 | value;
}

static uint32_t a64Stp(int a, int b, int offset) { return 0xa9000000u | (uint32_t)((offset / 8) & 0x7f) << 15 | (uint32_t)b << 10 | 31u << 5 | (uint32_t)a; }
static uint32_t a64Ldp(int a, int b, int offset) { return a64Stp(a, b, offset) | 0x00400000u; }
static uint32_t a64StpQ(int a, int b, int offset) { return 0xad000000u | (uint32_t)((offset / 16) & 0x7f) << 15 | (uint32_t)b << 10 | 31u << 5 | (uint32_t)a; }
static uint32_t a64LdpQ(int a, int b, int offset) { return a64StpQ(a, b, offset) | 0x00400000u; }
static uint32_t a64MovW(int to, int from) { return 0x2a0003e0u | (uint32_t)from << 16 | (uint32_t)to; }
static uint32_t a64Movz(int to, unsigned value) { return 0x52800000u | (value & 0xffff) << 5 | (uint32_t)to; }
static uint32_t a64LdrLiteral(int to, int fromWord, int literalWord) { return 0x58000000u | (uint32_t)((literalWord - fromWord) & 0x7ffff) << 5 | (uint32_t)to; }
static uint32_t a64Tbz(int reg, int bit, int fromWord, int toWord) {
    return 0x36000000u | (uint32_t)(bit >> 5) << 31 | (uint32_t)(bit & 31) << 19 | (uint32_t)((toWord - fromWord) & 0x3fff) << 5 | (uint32_t)reg;
}
static uint32_t a64B(const unsigned char *at, const unsigned char *to) { return 0x14000000u | ((uint32_t)((to - at) >> 2) & 0x3ffffffu); }

static int a64RunsAnywhere(uint32_t w) {
    return (w & 0x1f000000u) != 0x10000000u && (w & 0x7c000000u) != 0x14000000u && (w & 0xff000010u) != 0x54000000u &&
           (w & 0x7c000000u) != 0x34000000u && (w & 0x3b000000u) != 0x18000000u && (w & 0xfe1f0000u) != 0xd61f0000u;
}

static int emitBattleStubExit(uint32_t *code, int n, int exit) {
    code[n] = a64LdrLiteral(17, n, BATTLE_STUB_LITERALS + 2 + 2 * exit);
    code[n + 1] = 0xd61f0220u;
    return n + 2;
}

static void buildBattleTokenStubMac(uint32_t code[BATTLE_STUB_WORDS], const BattleTokenSite *s, const uint64_t exits[BATTLE_STUB_EXITS],
                                    uint32_t replaced) {
    int n = 0;
    memset(code, 0, BATTLE_STUB_WORDS * sizeof *code);
    code[n++] = 0xd10003ffu | (uint32_t)BATTLE_STUB_FRAME << 10;
    for (int r = 0; r < 16; r += 2) code[n++] = a64Stp(r, r + 1, r * 8);
    code[n++] = a64Stp(18, 30, 0x80);
    code[n++] = 0xd53b4200u;
    code[n++] = 0xf9000000u | (0x90 / 8) << 10 | 31u << 5;
    for (int q = 0; q < 8; q += 2) code[n++] = a64StpQ(q, q + 1, 0xa0 + q * 16);
    for (int q = 16; q < 32; q += 2) code[n++] = a64StpQ(q, q + 1, 0x120 + (q - 16) * 16);
    code[n++] = a64MovW(0, s->monsterReg);
    code[n++] = a64Movz(1, s->axis);
    code[n++] = a64Movz(2, s->encoding);
    code[n++] = a64Movz(3, s->threshold);
    code[n] = a64LdrLiteral(16, n, BATTLE_STUB_LITERALS);
    n++;
    code[n++] = 0xd63f0200u;
    code[n++] = 0xaa0003f0u;
    code[n++] = 0xf9400000u | (0x90 / 8) << 10 | 31u << 5;
    code[n++] = 0xd51b4200u;
    for (int q = 0; q < 8; q += 2) code[n++] = a64LdpQ(q, q + 1, 0xa0 + q * 16);
    for (int q = 16; q < 32; q += 2) code[n++] = a64LdpQ(q, q + 1, 0x120 + (q - 16) * 16);
    for (int r = 0; r < 16; r += 2) code[n++] = a64Ldp(r, r + 1, r * 8);
    code[n++] = a64Ldp(18, 30, 0x80);
    code[n++] = 0x910003ffu | (uint32_t)BATTLE_STUB_FRAME << 10;
    int noSize = n++, below = s->threshold ? n++ : -1;
    if (s->alsoReg >= 0 && s->alsoFrom >= 0) code[n++] = 0xaa0003e0u | (uint32_t)s->alsoFrom << 16 | (uint32_t)s->alsoReg;
    if (s->result >= 0) code[n++] = a64MovW(s->result, 16);
    if (s->alsoReg >= 0 && s->alsoFrom < 0) code[n++] = a64Movz(s->alsoReg, s->alsoValue);
    n = emitBattleStubExit(code, n, 0);
    if (below >= 0) {
        code[below] = a64Tbz(16, 33, below, n);
        if (s->belowReg >= 0) code[n++] = a64Movz(s->belowReg, s->belowValue);
        n = emitBattleStubExit(code, n, 1);
    }
    code[noSize] = a64Tbz(16, 32, noSize, n);
    if (s->flagsReg != BATTLE_SITE_NO_TBNZ) {
        code[n] = a64Tbz(s->flagsReg, 52, n, n + 3);
        n = emitBattleStubExit(code, n + 1, 2);
    } else {
        code[n++] = replaced;
    }
    emitBattleStubExit(code, n, 3);
    memcpy(&code[BATTLE_STUB_LITERALS + 2], exits, BATTLE_STUB_EXITS * sizeof exits[0]);
    const void *callback = (const void *)battleTokenSiteValue;
    memcpy(&code[BATTLE_STUB_LITERALS], &callback, sizeof callback);
}

static void buildBattleTokenSharedJumpMac(uint32_t code[6], const uint64_t *table) {
    code[0] = 0x58000091u;
    code[1] = 0xf8707a31u;
    code[2] = 0xd61f0220u;
    code[3] = 0xd503201fu;
    memcpy(&code[4], &table, sizeof table);
}

static int buildBattleTokenEntryMac(uint32_t code[4], const unsigned char *at, int index, int saving, const unsigned char *shared) {
    int n = 0;
    if (saving) code[n++] = 0xa9bf47f0u;
    code[n++] = a64Movz(16, (unsigned)index);
    code[n] = a64B(at + 4 * n, shared);
    n++;
    if (saving) code[n++] = 0xd503201fu;
    return n * 4;
}

static void buildBattleTokenExitsMac(uint32_t code[8], const unsigned char *at, const uint64_t exits[BATTLE_STUB_EXITS]) {
    memset(code, 0, 8 * sizeof *code);
    for (int i = 0; i < BATTLE_STUB_EXITS; i++) {
        code[2 * i] = 0xa8c147f0u;
        code[2 * i + 1] = exits[i] ? a64B(at + 8 * i + 4, (const unsigned char *)(uintptr_t)exits[i]) : 0;
    }
}

static uint32_t battleTokenSiteSaved[BATTLE_TOKEN_MAC_SITE_COUNT], battleTokenSiteJump[BATTLE_TOKEN_MAC_SITE_COUNT];
static uint64_t battleTokenStubTable[BATTLE_TOKEN_MAC_SITE_COUNT];
static int battleTokenSitesBuilt;

static int buildBattleTokenSitesMac(void) {
    if (battleTokenSitesBuilt) return battleTokenSitesBuilt > 0;
    battleTokenSitesBuilt = -1;
    long need = 24, room = paddingLeft() + (helperLeft() > 0 ? helperLeft() : 0);
    for (int i = 0; i < BATTLE_TOKEN_MAC_SITE_COUNT; i++) {
        const BattleTokenSite *s = &BATTLE_TOKEN_MAC_SITES[i];
        uint32_t word;
        memcpy(&word, gameBase + s->site, 4);
        if (s->flagsReg != BATTLE_SITE_NO_TBNZ ? (word & 0xfff8001fu) != (0xb7a00000u | s->flagsReg) : !a64RunsAnywhere(word)) {
            writeLogError("battle_size_sites_built", "ok=0 reason=not_the_expected_code site=0x%x", s->site);
            return 0;
        }
        need += s->saveScratch ? 48 : 8;
    }
    if (room < need) {
        writeLogError("battle_size_sites_built", "ok=0 reason=no_padding need=%ld left=%ld", need, room);
        return 0;
    }
    size_t bytes = ((size_t)BATTLE_TOKEN_MAC_SITE_COUNT * BATTLE_STUB_WORDS * 4 + MAC_PAGE_MASK) & ~(size_t)MAC_PAGE_MASK;
    unsigned char *page = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (page == MAP_FAILED) {
        writeLogError("battle_size_sites_built", "ok=0 reason=no_memory");
        return 0;
    }
    uint32_t sharedCode[6];
    unsigned char *shared = takePadding(sizeof sharedCode);
    buildBattleTokenSharedJumpMac(sharedCode, battleTokenStubTable);
    if (!shared || !writeGameCode(shared, sharedCode, sizeof sharedCode)) {
        writeLogError("battle_size_sites_built", "ok=0 reason=no_padding site=shared");
        munmap(page, bytes);
        return 0;
    }
    for (int i = 0; i < BATTLE_TOKEN_MAC_SITE_COUNT; i++) {
        const BattleTokenSite *s = &BATTLE_TOKEN_MAC_SITES[i];
        unsigned char *site = gameBase + s->site, *stub = page + (size_t)i * BATTLE_STUB_WORDS * 4;
        uint32_t word, entry[4], out[8];
        memcpy(&word, site, 4);
        uint64_t game[BATTLE_STUB_EXITS] = {
            (uint64_t)(uintptr_t)(gameBase + s->join), s->below ? (uint64_t)(uintptr_t)(gameBase + s->below) : 0,
            s->flagsReg != BATTLE_SITE_NO_TBNZ ? (uint64_t)(uintptr_t)(site + ((int32_t)(word << 13) >> 18) * 4) : 0,
            (uint64_t)(uintptr_t)(site + 4)}, exits[BATTLE_STUB_EXITS];
        unsigned char *in = takePadding(s->saveScratch ? 16 : 8), *outAt = in && s->saveScratch ? takePadding(32) : NULL;
        if (!in || (s->saveScratch && !outAt)) {
            writeLogError("battle_size_sites_built", "ok=0 reason=no_padding site=0x%x", s->site);
            munmap(page, bytes);
            return 0;
        }
        int entryBytes = buildBattleTokenEntryMac(entry, in, i, s->saveScratch, shared);
        if (s->saveScratch) {
            buildBattleTokenExitsMac(out, outAt, game);
            for (int e = 0; e < BATTLE_STUB_EXITS; e++) exits[e] = (uint64_t)(uintptr_t)(outAt + 8 * e);
        } else {
            memcpy(exits, game, sizeof game);
        }
        if (!writeGameCode(in, entry, (size_t)entryBytes) || (s->saveScratch && !writeGameCode(outAt, out, sizeof out))) {
            writeLogError("battle_size_sites_built", "ok=0 reason=write site=0x%x", s->site);
            munmap(page, bytes);
            return 0;
        }
        buildBattleTokenStubMac((uint32_t *)stub, s, exits, word);
        battleTokenStubTable[i] = (uint64_t)(uintptr_t)stub;
        battleTokenSiteSaved[i] = word;
        battleTokenSiteJump[i] = a64B(site, in);
    }
    if (mprotect(page, bytes, PROT_READ | PROT_EXEC)) {
        writeLogError("battle_size_sites_built", "ok=0 reason=protect");
        munmap(page, bytes);
        return 0;
    }
    sys_icache_invalidate(page, bytes);
    battleTokenSitesBuilt = 1;
    writeLog("battle_size_sites_built", "ok=1 sites=%d padding_used=%ld", BATTLE_TOKEN_MAC_SITE_COUNT, need);
    return 1;
}

static int setBattleTokenSitesMac(int on) {
    if (on && !buildBattleTokenSitesMac()) return 0;
    if (battleTokenSitesBuilt <= 0) return 0;
    int set = 0;
    for (int i = 0; i < BATTLE_TOKEN_MAC_SITE_COUNT; i++)
        set += writeGameCode(gameBase + BATTLE_TOKEN_MAC_SITES[i].site, on ? &battleTokenSiteJump[i] : &battleTokenSiteSaved[i], 4);
    return set;
}
#endif

// ---- The random target's reach -------------------------------------------------------------------
#define BATTLE_TARGET_GAME_REACH 2
#ifndef __APPLE__
static const unsigned char BATTLE_REACH_AT[] = {0x02, 0x05, 0x11, 0x19};
#define BATTLE_REACH_BYTES 0x1a
#else
static const unsigned char BATTLE_REACH_AT[] = {0x00, 0x0c, 0x10, 0x2c, 0x34, 0x50};
#define BATTLE_REACH_BYTES 0x54
#endif
static int battleTargetReach = BATTLE_TARGET_GAME_REACH, battleTargetReachClaimed;

static int setBattleTargetReach(int reach) {
    if (reach == battleTargetReach) return 1;
    if (!gameCodeMatches) return 0;
    if (!battleTargetReachClaimed) {
        if (!claimGameCode(RVA_BATTLE_RANDOM_TARGET_REACH, BATTLE_REACH_BYTES, "battlesize")) return 0;
        battleTargetReachClaimed = 1;
    }
    int ok = 0;
    for (size_t i = 0; i < sizeof BATTLE_REACH_AT; i++) {
        unsigned char *at = gameBase + RVA_BATTLE_RANDOM_TARGET_REACH + BATTLE_REACH_AT[i];
#ifndef __APPLE__
        unsigned char byte = (unsigned char)reach;
        ok += writeGameCode(at, &byte, 1);
#else
        uint32_t word;
        memcpy(&word, at, 4);
        word = (word & 0xff800000u) == 0x52800000u ? (word & ~(0xffffu << 5)) | (uint32_t)reach << 5
                                                   : (word & ~(0xfffu << 10)) | (uint32_t)reach << 10;
        ok += writeGameCode(at, &word, 4);
#endif
    }
    writeResultLog(ok == (int)sizeof BATTLE_REACH_AT, "battle_size_reach_set", "reach=%d writes=%d of %d", reach, ok, (int)sizeof BATTLE_REACH_AT);
    if (ok == (int)sizeof BATTLE_REACH_AT) battleTargetReach = reach;
    return ok == (int)sizeof BATTLE_REACH_AT;
}

static int setBattleTokenHooks(void) {
    static const struct { unsigned int rva; void *hook; void **game; } HOOKS[BATTLE_TOKEN_HOOKS] = {
#ifndef __APPLE__
        {RVA_UNIT_BATTLE_SIZE, (void *)unitBattleHeightHook, (void **)&gameUnitBattleHeight},
        {RVA_UNIT_BATTLE_SIZE_2, (void *)unitBattleWidthHook, (void **)&gameUnitBattleWidth},
        {RVA_MONSTER_BATTLE_SIZE, (void *)monsterBattleWidthHook, (void **)&gameMonsterBattleWidth},
#endif
        {RVA_UNIT_BOAT_SIZE, (void *)unitBattleAreaHook, (void **)&gameUnitBattleArea},
        {RVA_BATTLE_UNIT_AT, (void *)battleUnitAtHook, (void **)&gameBattleUnitAt},
        {RVA_BATTLE_UNIT_AT_2, (void *)battleUnitAt2Hook, (void **)&gameBattleUnitAt2},
        {RVA_BATTLE_PLACE_UNIT, (void *)battlePlaceUnitHook, (void **)&gameBattlePlaceUnit},
        {RVA_BATTLE_FIGHT_MOVE, (void *)battleFightMoveHook, (void **)&gameBattleFightMove},
    };
    int on = battleTokens.count > 0;
    if (on == battleTokenHooksOn) return battleTokenHooksOn ? BATTLE_TOKEN_HOOKS : 0;
    int ok = 0;
    for (int i = 0; i < BATTLE_TOKEN_HOOKS; i++) {
        void *target = gameBase + HOOKS[i].rva;
        if (!(battleTokenHooksMade & (1 << i)) && on) {
            if (!fanpatchHookCreate(target, HOOKS[i].hook, HOOKS[i].game)) continue;
            battleTokenHooksMade |= 1 << i;
        }
        if (battleTokenHooksMade & (1 << i)) ok += fanpatchHookEnable(target, on);
    }
    battleTokenHooksOn = on;
    int sites = 0, sitesWanted = 0;
#ifdef __APPLE__
    sites = setBattleTokenSitesMac(on);
    sitesWanted = on || battleTokenSitesBuilt > 0 ? BATTLE_TOKEN_MAC_SITE_COUNT : 0;
#endif
    writeResultLog(ok == (on ? BATTLE_TOKEN_HOOKS : __builtin_popcount((unsigned)battleTokenHooksMade)) && sites == sitesWanted,
                   "battle_size_hooks_set", "on=%d hooks=%d sites=%d monsters=%d largest=%d", on, ok, sites, battleTokens.count,
                   battleTokens.largest);
    return on ? ok : 0;
}

static int setBattleTokensHuge(void) {
    int monsters = 0, given = 0;
    for (int monster = 0; monster < battleTokens.capacity && battleTokens.count; monster++) {
        if (battleTokenSizeOf(monster, BATTLE_AXIS_WIDTH) * battleTokenSizeOf(monster, BATTLE_AXIS_HEIGHT) < BATTLE_TOKEN_HUGE_AREA)
            continue;
        unsigned char *record = monsterRecord(monster);
        if (!record) continue;
        unsigned long long moves;
        memcpy(&moves, record + MONSTER_MOVE_FLAGS, sizeof moves);
        monsters++;
        if (moves & MONSTER_MOVE_HUGE) continue;
        moves |= MONSTER_MOVE_HUGE;
        memcpy(record + MONSTER_MOVE_FLAGS, &moves, sizeof moves);
        given++;
    }
    if (monsters) writeLog("battle_size_huge_set", "monsters=%d given=%d", monsters, given);
    return given;
}

static void battleTokenSizesAfterPass(int level) {
    if (level != 2) return;
    setBattleTokensHuge();
    setBattleTokenHooks();
    setBattleTargetReach(battleTokens.largest > BATTLE_TOKEN_GAME_MAX ? battleTokens.largest - 1 : BATTLE_TARGET_GAME_REACH);
}

static void battleTokenWordsInit(void) {
    int words = apiAddModCommand("battlesize", battleSizeLine);
    writeResultLog(words == 1, "battle_size_word_installed", "words=%d of 1", words);
}
