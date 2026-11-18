#pragma once

// ---- The cause: the hit or target pick running now ------------------------------------------------
#define BANISH_CAUSE_MAX 64
static struct { int weapon, attacker; } banishCauses[BANISH_CAUSE_MAX];
static int banishCauseDepth;
static void pushBanishCause(int weapon, int attacker) {
    if (banishCauseDepth < BANISH_CAUSE_MAX) banishCauses[banishCauseDepth].weapon = weapon, banishCauses[banishCauseDepth].attacker = attacker;
    banishCauseDepth++;
}
static void popBanishCause(void) { banishCauseDepth -= banishCauseDepth > 0; }
static void banishCause(int *weapon, int *attacker) {
    int known = banishCauseDepth > 0 && banishCauseDepth <= BANISH_CAUSE_MAX;
    *weapon = known ? banishCauses[banishCauseDepth - 1].weapon : -1;
    *attacker = known ? banishCauses[banishCauseDepth - 1].attacker : -1;
}

typedef unsigned long long (*WeaponHitFn)(unsigned long long, unsigned long long, unsigned long long, unsigned long long,
                                          unsigned long long, unsigned long long, unsigned long long, unsigned long long);
static WeaponHitFn gameWeaponHit;
static unsigned long long weaponHitCauseHook(unsigned long long attacker, unsigned long long target, unsigned long long weapon,
                                             unsigned long long a4, unsigned long long a5, unsigned long long a6,
                                             unsigned long long a7, unsigned long long a8) {
    pushBanishCause((int)weapon, (int)attacker);
    unsigned long long result = gameWeaponHit(attacker, target, weapon, a4, a5, a6, a7, a8);
    popBanishCause();
    return result;
}

// ---- Each banish in the fights, and its cause -----------------------------------------------------
typedef struct { int unit, weapon, attacker, plane, x, y, used; } BanishRecord;
static struct { BanishRecord *list; int count, capacity; } banishRecords;
static int banishFightsDepth;
static int battleBanishState;
static int banishAftermathIn;

static int noteBanish(int unit, int plane, int x, int y) {
    if (!growList((void **)&banishRecords.list, &banishRecords.capacity, banishRecords.count + 1, sizeof(BanishRecord))) return 0;
    BanishRecord *r = &banishRecords.list[banishRecords.count++];
    *r = (BanishRecord){unit, -1, -1, plane, x, y, 0};
    banishCause(&r->weapon, &r->attacker);
    return 1;
}

static BanishRecord *banishRecordOf(int unit) {
    for (int i = banishRecords.count - 1; i >= 0; i--)
        if (banishRecords.list[i].unit == unit && !banishRecords.list[i].used) return &banishRecords.list[i];
    return NULL;
}

static void battleBanishFightsBegin(const CallInfo *call) { (void)call, banishFightsDepth++; }

static void battleBanishFightsEnd(const CallInfo *call) {
    (void)call;
    banishFightsDepth -= banishFightsDepth > 0;
    if (banishFightsDepth || !banishRecords.count) return;
    int used = 0;
    for (int i = 0; i < banishRecords.count; i++) used += banishRecords.list[i].used;
    writeLog("battle_banish_records_cleared", "records=%d used=%d", banishRecords.count, used);
    banishRecords.count = 0;
}

static void battleBanishObserved(const CallInfo *call) {
    int unit = (int)call->args[0];
    if (!banishFightsDepth || unit < 0 || unit >= UNIT_TABLE_MAX) return;
    if (*(const unsigned long long *)GAME_PTR(RVA_UNITS + (size_t)unit * UNIT_SIZE) & UNIT_STATE_BATTLE_SUMMON) return;
    if (!noteBanish(unit, -1, -1, -1)) writeLogError("battle_banish_not_kept", "unit=%d reason=out_of_memory", unit);
}

// ---- BANISH_DESTINATION ---------------------------------------------------------------------------
static struct { int plane, x, y; } banishGameChoice;

static void drawBanishSquare(int plane, int *x, int *y) {
    GAME_FN(int (*)(int, int, int, int, int, int, int *, int *), RVA_RANDOM_RITUAL_SQUARE)(plane, 1, 0, 0, -1, -1, x, y);
}

static int banishDestinationAsGame(int unit, int weapon, int attacker, int *x, int *y) {
    (void)unit, (void)weapon, (void)attacker;
    if (banishGameChoice.x >= 0) *x = banishGameChoice.x, *y = banishGameChoice.y;
    else drawBanishSquare(banishGameChoice.plane, x, y);
    return banishGameChoice.plane;
}

static void runBanishDestination(int unit, int gamePlane, int *x, int *y) {
    BanishRecord *r = banishRecordOf(unit);
    int weapon = r ? r->weapon : -1, attacker = r ? r->attacker : -1, named = r && r->plane >= 0;
    banishGameChoice.plane = named ? r->plane : gamePlane;
    banishGameChoice.x = named ? r->x : -1;
    banishGameChoice.y = named ? r->y : -1;
    if (r) r->used = 1;
    *x = *y = -1;
    typedef int (*DestinationFn)(int, int, int, int *, int *);
    int plane = ((DestinationFn)siteEntry(SITE_BANISH_DESTINATION, (void *)banishDestinationAsGame))(unit, weapon, attacker, x, y);
    writeLog("battle_banish_destination", "unit=%d plane=%d x=%d y=%d weapon=%d attacker=%d game_plane=%d named=%d", unit, plane, *x, *y,
             weapon, attacker, gamePlane, named ? banishGameChoice.plane : -1);
}

#ifndef __aarch64__
#define BANISH_STUB          0x000
#define BANISH_FUNCTION      0x080
#define BANISH_UNWIND        0x090
#define BANISH_DRAW_SIZE     7

static unsigned long long banishSquareWin(int unit, int total, int fallback) {
    int x, y;
    runBanishDestination(unit, total > 0 ? total : fallback, &x, &y);
    return (unsigned long long)(unsigned)x | (unsigned long long)(unsigned)y << 32;
}

static int installBanishAftermath(void) {
    unsigned char *draw = GAME_PTR(RVA_BANISH_SQUARE_DRAW), *write = GAME_PTR(RVA_BANISH_SQUARE_WRITE);
    int rel;
    memcpy(&rel, draw + 3, 4);
    static const unsigned char WRITE_CODE[] = {0x48, 0x8b, 0x44, 0x24, 0x48};
    if (!gameCodeMatches || draw[0] != 0x44 || draw[1] != 0x8b || draw[2] != 0x05 || draw + BANISH_DRAW_SIZE + rel != GAME_PTR(RVA_MAP_DEPTH) ||
        memcmp(write, WRITE_CODE, sizeof WRITE_CODE)) {
        writeLogError("hook_skipped", "site=banish_aftermath reason=not_the_expected_code");
        return 0;
    }
    unsigned char *page = allocNear(draw);
    if (!page || !copyGameUnwind(draw, page + BANISH_UNWIND)) {
        writeLogError("hook_skipped", "site=banish_aftermath reason=%s", page ? "unwind_info" : "no_near_page");
        return 0;
    }
    unsigned char *stub = page + BANISH_STUB;
    size_t n = buildBanishSquareStub(stub, stub, (const void *)banishSquareWin, write);
    RUNTIME_FUNCTION *function = (RUNTIME_FUNCTION *)(page + BANISH_FUNCTION);
    function->BeginAddress = BANISH_STUB;
    function->EndAddress = (DWORD)(BANISH_STUB + n);
    function->UnwindData = BANISH_UNWIND;
    if (!RtlAddFunctionTable(function, 1, (DWORD64)(uintptr_t)page)) {
        writeLogError("hook_skipped", "site=banish_aftermath reason=unwind_not_added");
        return 0;
    }
    makePagesReadExecute(page, 0x1000);
    unsigned char code[BANISH_DRAW_SIZE] = {0xe9, 0, 0, 0, 0, 0x90, 0x90};
    rel = (int)(stub - (draw + 5));
    memcpy(code + 1, &rel, 4);
    if (!writeGameCode(draw, code, sizeof code)) {
        writeLogError("hook_skipped", "site=banish_aftermath reason=code_unwritable");
        return 0;
    }
    return 1;
}
#else
// @platform-split banish: Mac's aftermath calls RVA_UNIT_ABILITY_TOTAL and RVA_RANDOM_RITUAL_SQUARE, whose two bl's CoE5SE points at its own; Windows inlines both, so a stub takes the draw
static int banishPlaneUnitMac = -1;
static int banishPlaneTotalMac(int unit, int ability, int add, char *out, int flag, int *out2) {
    banishPlaneUnitMac = unit;
    return GAME_FN(int (*)(int, int, int, char *, int, int *), RVA_UNIT_ABILITY_TOTAL)(unit, ability, add, out, flag, out2);
}
static int banishSquareMac(int plane, int mode, int empty, int size, int terrainA, int terrainB, int *x, int *y) {
    (void)mode, (void)empty, (void)size, (void)terrainA, (void)terrainB;
    runBanishDestination(banishPlaneUnitMac, plane, x, y);
    return *x >= 0;
}
static int installBanishAftermath(void) {
    return pointWeaponTraitBranch(RVA_BANISH_PLANE_TOTAL_CALL, RVA_UNIT_ABILITY_TOTAL, (void *)banishPlaneTotalMac, "banish_plane_total") &&
           pointWeaponTraitBranch(RVA_BANISH_SQUARE_DRAW, RVA_RANDOM_RITUAL_SQUARE, (void *)banishSquareMac, "banish_square");
}
#endif

static void installBattleBanish(void) {
    if (battleBanishState) return;
    int fights = apiObserve(SITE_DO_ALL_FIGHTS, battleBanishFightsBegin, battleBanishFightsEnd);
    int banish = apiObserve(SITE_BATTLE_BANISH, battleBanishObserved, NULL);
    unsigned char *hitAt = GAME_PTR(RVA_WEAPON_HIT);
    int hit = fanpatchHookCreate(hitAt, (void *)weaponHitCauseHook, (void **)&gameWeaponHit) && fanpatchHookEnable(hitAt, 1);
    banishAftermathIn = installBanishAftermath();
    battleBanishState = fights && banishAftermathIn ? 1 : -1;
    writeInstallLogFields("battle_banish_installed", fights + banish + hit + banishAftermathIn, 4, "fights=%d banish=%d hit=%d aftermath=%d", fights,
                          banish, hit, banishAftermathIn);
}

// ---- coe5.battleBanishUnit ------------------------------------------------------------------------
static const char *battleBanishProblem(lua_Integer unit, int named, lua_Integer plane, int square, lua_Integer x, lua_Integer y,
                                       int *unitIsSummons) {
    if (G32(RVA_ACTIVE_PLAYER) < 0) return "no_game";
    if (!*(const unsigned char *)GAME_PTR(RVA_IN_BATTLE)) return "no_battle";
    const char *reason = gameCallArgProblem('u', unit);
    if (reason) return reason;
    if (named && (plane < 0 || plane > G32(RVA_LAST_PLANE))) return "bad_plane";
    if (square && !named) return "no_plane";
    if (square && (x < 0 || y < 0 || x >= G32(RVA_MAP_DEPTH) || y >= G32(RVA_MAP_H) || x >= MAP_COLUMNS_MAX || y >= MAP_COLUMN_SQUARES))
        return "off_map";
    if (square && GAME_FN(int (*)(int, int), RVA_PLANE_AT)((int)x, (int)y) != plane) return "wrong_plane";
    int cellX, cellY;
    if (!unitBattleCell((int)unit, &cellX, &cellY)) return "not_on_field";
    *unitIsSummons = (*(const unsigned long long *)GAME_PTR(RVA_UNITS + (size_t)unit * UNIT_SIZE) & UNIT_STATE_BATTLE_SUMMON) != 0;
    if (named && !*unitIsSummons && battleBanishState < 0) return "site_unavailable";
    return NULL;
}

static int lBattleBanishUnit(lua_State *L) {
    lua_Integer unit = luaL_checkinteger(L, 1);
    int named = !lua_isnoneornil(L, 2), square = !lua_isnoneornil(L, 3), summons = 0;
    lua_Integer plane = named ? luaL_checkinteger(L, 2) : -1, x = square ? luaL_checkinteger(L, 3) : -1, y = square ? luaL_checkinteger(L, 4) : -1;
    const char *reason = battleBanishProblem(unit, named, plane, square, x, y, &summons);
    int kept = !reason && !summons && banishFightsDepth > 0;
    if (kept && !noteBanish((int)unit, (int)plane, (int)x, (int)y)) reason = "out_of_memory";
    if (reason) {
        refuseLuaCall(runningLuaMod(), "coe5.battleBanishUnit", reason, "unit=%lld plane=%lld x=%lld y=%lld", (long long)unit, (long long)plane,
                      (long long)x, (long long)y);
        return 0;
    }
    int weapon, attacker;
    banishCause(&weapon, &attacker);
    GAME_FN(void (*)(int), RVA_BATTLE_BANISH)((int)unit);
    writeLog("battle_banished", "unit=%lld plane=%lld x=%lld y=%lld weapon=%d attacker=%d summons=%d aftermath=%d", (long long)unit,
             (long long)plane, (long long)x, (long long)y, weapon, attacker, summons, kept);
    lua_pushboolean(L, 1);
    return 1;
}
