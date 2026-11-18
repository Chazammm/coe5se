#pragma once

#define BATTLE_SUMMON_DEPLOY_MODE 3
#define PARKED_UNITS_FILE "tmp_np"
#define PARKED_UNITS_MAGIC 0x309

static void moveUnitAfflictions(int from, int to);
static void lendAfflictionSlot(int unit);
static int unitBattleCell(int unit, int *x, int *y);

// ---- Units that stay, through the fight's end --------------------------------------------------
static struct { int *list; int count, capacity; } stayingSummons;
static int summonFightsDepth;
static int battleSummonState;
static unsigned char *parkedSlots;

static int readParkedSlots(int *highest) {
    *highest = -1;
    if (!parkedSlots && !(parkedSlots = malloc(UNIT_TABLE_MAX))) return 0;
    memset(parkedSlots, 0, UNIT_TABLE_MAX);
    char path[2600];
    if (!buildPath(path, sizeof path, __func__, "%s/" PARKED_UNITS_FILE, (const char *)GAME_PTR(RVA_TEMP_DIR))) return 0;
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    int value = 0, whole = fread(&value, 4, 1, file) == 1 && value == PARKED_UNITS_MAGIC;
    while (whole) {
        if (fread(&value, 4, 1, file) != 1 || value >= UNIT_TABLE_MAX) whole = 0;
        else if (value < 0) break;
        else if (fseek(file, SAVED_UNIT_SIZE, SEEK_CUR)) whole = 0;
        else parkedSlots[value] = 1, *highest = value > *highest ? value : *highest;
    }
    fclose(file);
    return whole;
}

static int unitSlotEmpty(int unit) {
    size_t at = (size_t)unit * UNIT_SIZE;
    return G16(RVA_UNIT_PLAYER + at) < 0 && G16(RVA_UNIT_HP + at) <= 0 && G16(RVA_UNIT_XP + at) == -1;
}

static int unparkedSlot(int highest) {
    int last = G32(RVA_UNIT_COUNT) > highest ? G32(RVA_UNIT_COUNT) : highest;
    for (int slot = 0; slot <= last && slot < UNIT_TABLE_MAX; slot++)
        if (!parkedSlots[slot] && unitSlotEmpty(slot)) return slot;
    return last + 1 < UNIT_TABLE_MAX ? last + 1 : -1;
}

static void moveUnitSlot(int from, int to) {
    unsigned char *a = GAME_PTR(RVA_UNITS + (size_t)from * UNIT_SIZE), *b = GAME_PTR(RVA_UNITS + (size_t)to * UNIT_SIZE);
    lendUnitSlot(to), lendAfflictionSlot(to);
    memcpy(b, a, UNIT_SIZE);
    for (int p = UNIT_ITEM_SLOTS; p < ITEM_POSITION_MAX && unitExtraItemCount(from); p++)
        if (unitItemAt(from, p) >= 0) setUnitItemAt(to, p, unitItemAt(from, p)), setUnitItemAt(from, p, -1);
    moveUnitAfflictions(from, to);
    G16(RVA_UNIT_PLAYER + (size_t)from * UNIT_SIZE) = -1, G16(RVA_UNIT_HP + (size_t)from * UNIT_SIZE) = 0;
    G16(RVA_UNIT_XP + (size_t)from * UNIT_SIZE) = -1;
    if (to > G32(RVA_UNIT_COUNT)) G32(RVA_UNIT_COUNT) = to;
}

static void battleSummonFightsBegin(const CallInfo *call) { (void)call, summonFightsDepth++; }
static void battleSummonFightsEnd(const CallInfo *call) { (void)call, summonFightsDepth -= summonFightsDepth > 0; }
static void battleSummonFightBegin(const CallInfo *call) { (void)call, stayingSummons.count = 0; }

static void battleSummonFightEnd(const CallInfo *call) {
    (void)call;
    int count = stayingSummons.count, highest;
    stayingSummons.count = 0;
    if (!count || !summonFightsDepth) return;
    if (!readParkedSlots(&highest)) {
        writeLogError("battle_summon_not_moved", "units=%d reason=parked_units_unread", count);
        return;
    }
    for (int i = 0; i < count; i++) {
        int unit = stayingSummons.list[i];
        size_t at = (size_t)unit * UNIT_SIZE;
        if (!parkedSlots[unit] || G16(RVA_UNIT_HP + at) <= 0 || G16(RVA_UNIT_PLAYER + at) < 0) continue;
        int to = unparkedSlot(highest);
        if (to < 0) {
            writeLogError("battle_summon_not_moved", "unit=%d reason=no_free_slot", unit);
            continue;
        }
        moveUnitSlot(unit, to);
        parkedSlots[to] = 1;
        writeLog("battle_summon_moved", "unit=%d to=%d monster=%d player=%d", unit, to, G16(RVA_UNIT_MONSTER + (size_t)to * UNIT_SIZE),
                 G16(RVA_UNIT_PLAYER + (size_t)to * UNIT_SIZE));
    }
}

static void installBattleSummon(void) {
    if (battleSummonState) return;
    int fights = apiObserve(SITE_DO_ALL_FIGHTS, battleSummonFightsBegin, battleSummonFightsEnd);
    int fight = apiObserve(SITE_FIGHT_THE_FIGHT, battleSummonFightBegin, battleSummonFightEnd);
    battleSummonState = fights && fight ? 1 : -1;
    writeInstallLogFields("battle_summon_installed", fights + fight, 2, "fights=%d fight=%d", fights, fight);
}

// ---- coe5.battleSummon --------------------------------------------------------------------------
static const char *battleSummonProblem(lua_Integer monster, lua_Integer player, lua_Integer x, lua_Integer y, int stays) {
    if (G32(RVA_ACTIVE_PLAYER) < 0) return "no_game";
    if (!*(const unsigned char *)GAME_PTR(RVA_IN_BATTLE)) return "no_battle";
    if (!isUsedMonster(monster)) return "bad_monster";
    if (player < 0 || player >= playerSlotCount()) return "bad_player";
    if (!playerSameTeam((int)player, G32(RVA_BATTLE_PLAYER_A)) && !playerSameTeam((int)player, G32(RVA_BATTLE_PLAYER_B))) return "not_a_side";
    if (x < 0 || y < 0 || x >= G32(RVA_BATTLE_WIDTH) || x >= BATTLE_COLUMNS_MAX || y >= BATTLE_ROWS) return "off_field";
    if (stays && summonFightsDepth && battleSummonState < 0) return "site_unavailable";
    return NULL;
}

static int lBattleSummon(lua_State *L) {
    lua_Integer monster = luaL_checkinteger(L, 1), player = luaCheckPlayer(L, 2);
    lua_Integer x = luaL_checkinteger(L, 3), y = luaL_checkinteger(L, 4);
    int stays = lua_toboolean(L, 5);
    const char *reason = battleSummonProblem(monster, player, x, y, stays);
    int unit = -1, kept = stays && summonFightsDepth > 0;
    if (!reason) {
        unit = GAME_FN(int (*)(int, int, int, int, int), RVA_NEW_UNIT)(G32(RVA_BATTLE_X), G32(RVA_BATTLE_Y), (int)player, -1, (int)monster);
        if (unit < 0) reason = "no_free_slot";
    }
    if (!reason && kept && !growList((void **)&stayingSummons.list, &stayingSummons.capacity, stayingSummons.count + 1, sizeof(int)))
        reason = "out_of_memory";
    if (!reason) {
        if (!stays) *(unsigned long long *)GAME_PTR(RVA_UNITS + (size_t)unit * UNIT_SIZE) |= UNIT_STATE_BATTLE_SUMMON;
        ((void (*)(int, int, int, int))siteEntry(SITE_BATTLE_DEPLOY_UNIT, GAME_PTR(RVA_BATTLE_DEPLOY_UNIT)))(unit, (int)x, (int)y,
                                                                                                          BATTLE_SUMMON_DEPLOY_MODE);
        int cellX, cellY;
        if (!unitBattleCell(unit, &cellX, &cellY)) reason = "no_room";
    }
    if (reason) {
        if (unit >= 0) GAME_FN(void (*)(int), RVA_REMOVE_UNIT)(unit);
        refuseLuaCall(runningLuaMod(), "coe5.battleSummon", reason, "monster=%lld player=%lld x=%lld y=%lld stays=%d", (long long)monster,
                      (long long)player, (long long)x, (long long)y, stays);
        return 0;
    }
    if (kept) stayingSummons.list[stayingSummons.count++] = unit;
    writeLog("battle_summoned", "unit=%d monster=%lld player=%lld x=%lld y=%lld stays=%d replay=%d", unit, (long long)monster,
             (long long)player, (long long)x, (long long)y, stays, *(const unsigned char *)GAME_PTR(RVA_BATTLE_IS_REPLAY) != 0);
    lua_pushinteger(L, unit);
    return 1;
}
