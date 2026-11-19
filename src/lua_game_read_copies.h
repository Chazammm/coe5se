#pragma once

#if defined(__APPLE__) || defined(COE5SE_COPY_SELFCHECK)
#define READ_COPY_UNIT_ITEMS 8
#define READ_COPY_TEMP_ABILITIES 4
#define READ_COPY_ABILITY_BLESS_A 0x19b
#define READ_COPY_ABILITY_BLESS_B 0x4cb
#define READ_COPY_ABILITY_FALSELIFE 0x1e9
#define READ_COPY_ABILITY_GUARD 0x34
#define READ_COPY_ABILITY_TRANSPORT 0x1cf
#define READ_COPY_ABILITY_FEAR 0x6a
#define READ_COPY_ABILITY_LOCAL_LEADERSHIP 9
#define READ_COPY_RITUAL_FX_LEVELUP 0x3b0
#define READ_COPY_MONSTER_INANIMATE (1ULL << 45)
#define READ_COPY_UNIT_NONSHIP (1ULL << 45)
#define READ_COPY_PORT_TERRAIN 0x400000000000001ULL

typedef unsigned long long (*ReadCopyFlagsFn)(int);

static size_t readCopyUnitAt(int u) { return (size_t)u * UNIT_SIZE; }
static int readCopyUnitPlayer(int u) { return (short)G16(RVA_UNIT_PLAYER + readCopyUnitAt(u)); }
static unsigned char *readCopyRitual(int slot) { return GAME_PTR(ritualTableRva() + (size_t)slot * RITUAL_SIZE); }

// ---- Ability sums -----------------------------------------------------------------------------------

static int readCopyAbilitySum(int u, int ability, int add) {
    if (u < 0 || u > G32(RVA_UNIT_COUNT) || ability < 1) return 0;
    size_t at = readCopyUnitAt(u);
    int monster = (short)G16(RVA_UNIT_MONSTER + at);
    if (monster < 0) return 0;
    int (*itemNth)(int, int, int) = GAME_FN(int (*)(int, int, int), RVA_ITEM_ABILITY_NTH);
    int total = monsterAbility(monster, ability), last = -1;
    for (int p = 0; p < READ_COPY_UNIT_ITEMS; p++) {
        int item = (short)G16(RVA_UNIT_ITEMS + at + (size_t)p * 2);
        if (item < 0 || item == last) continue;
        last = item;
        int value = itemNth(item, ability, 0);
        if (!value) continue;
        total = add ? total + value : total && value < total ? total : value;
    }
    int owner = (short)G16(RVA_UNIT_PLAYER + at);
    if (owner >= 0) {
        unsigned long long resistances = GAME_FN(ReadCopyFlagsFn, RVA_UNIT_RESISTANCES)(u), benefits = GAME_FN(ReadCopyFlagsFn, RVA_UNIT_BENEFITS)(u);
        if ((resistances >> 14 & 1) || (benefits >> 16 & 1) || monsterAbility(monster, READ_COPY_ABILITY_BLESS_A) > 0 ||
            monsterAbility(monster, READ_COPY_ABILITY_BLESS_B) > 0)
            total += slotBlessingOfCopy(owner, ability);
    }
    int temporary = 0;
    for (int i = 0; i < READ_COPY_TEMP_ABILITIES; i++) {
        int id = (short)G16(RVA_UNIT_TEMP_ABILITY_IDS + at + (size_t)i * 2);
        if (!id) break;
        if (id == ability) {
            temporary = G32(RVA_UNIT_TEMP_ABILITY_VALUES + at + (size_t)i * 4);
            break;
        }
    }
    if (add) return total + temporary;
    return temporary && temporary >= total ? temporary : total;
}

static int unitAbilitySumCopy(int u, int ability, int add) { return readCopyAbilitySum(u, ability, add); }
static int unitNoLandCopy(int u) { return (int)(GAME_FN(ReadCopyFlagsFn, RVA_UNIT_FLAGS)(u) >> 9 & 1); }
static int unitLifelessCopy(int u) {
    if (u < 0) return 0;
    int monster = (short)G16(RVA_UNIT_MONSTER + readCopyUnitAt(u));
    if (monster < 0 || !(copyMonsterWord(monster, COPY_MONSTER_FLAGS) & READ_COPY_MONSTER_INANIMATE)) return 0;
    if (u > G32(RVA_UNIT_COUNT)) return 1;
    return readCopyAbilitySum(u, READ_COPY_ABILITY_FALSELIFE, 1) <= 0;
}
static int itemAbilityCopy(int item, int ability) { return GAME_FN(int (*)(int, int, int), RVA_ITEM_ABILITY_NTH)(item, ability, 0); }

// ---- Commanders and the player ----------------------------------------------------------------------

static int countFreeCommandersCopy(int player, int free) {
    int count = 0;
    for (int c = 0; c <= G32(RVA_LAST_COMMANDER) && c < COMMANDER_TABLE_MAX; c++) {
        int u = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE);
        if (u < 0) continue;
        if (readCopyUnitPlayer(u) == player && (!free || G32(RVA_UNIT_LEADER + readCopyUnitAt(u)) < 0)) count++;
    }
    return count;
}
static long long playerRitualFlagsCopy(int player) {
    unsigned long long flags = 0;
    for (int c = 0; c <= G32(RVA_LAST_COMMANDER) && c < COMMANDER_TABLE_MAX; c++) {
        int u = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE);
        if (u < 0 || (short)G16(RVA_UNIT_HP + readCopyUnitAt(u)) <= 0 || readCopyUnitPlayer(u) != player) continue;
        for (int i = 0; i < 64; i++) {
            int slot = (short)G16(RVA_COMMANDER_KNOWN_RITUALS + (size_t)c * COMMANDER_SIZE + (size_t)i * 2);
            if (slot < 1) break;
            flags |= *(const unsigned long long *)(readCopyRitual(slot) + RITUAL_FLAGS);
        }
    }
    return (long long)flags;
}
#define READ_COPY_ABILITY_GOLDBONUS 0xc8
#define READ_COPY_RITUAL_FX_GOLDBONUS 0x488
#define READ_COPY_ABILITY_TRADEBONUS 0xca
#define READ_COPY_RITUAL_FX_TRADEBONUS 0x48a
static int goldBonusPercentCopy(int player) { return copyIncomeBonusPercent(player, READ_COPY_ABILITY_GOLDBONUS, READ_COPY_RITUAL_FX_GOLDBONUS, 1ULL << 8); }
static int tradeBonusPercentCopy(int player) { return copyIncomeBonusPercent(player, READ_COPY_ABILITY_TRADEBONUS, READ_COPY_RITUAL_FX_TRADEBONUS, 1ULL << 10); }

// ---- Rituals and spells -----------------------------------------------------------------------------

static int ritualEffectFirstCopy(int ritual, int fx) { return ritual < 1 ? 0 : ritualFirstFx(readCopyRitual(ritual), fx, 0); }
static int ritualEffectOrCopy(int ritual, int fx, int none) { return ritual < 1 ? 0 : ritualFirstFx(readCopyRitual(ritual), fx, none); }
static void repickMemorizedCopy(int u) {
    G16(RVA_UNIT_MEMORIZED_SPELLS + readCopyUnitAt(u)) = -1;
    void (*choose)(int) = GAME_FN(void (*)(int), RVA_CHOOSE_MEMORIZED);
#ifdef __APPLE__
    if (macRedirectedHook(RVA_CHOOSE_MEMORIZED)) choose = (void (*)(int))macRedirectedHook(RVA_CHOOSE_MEMORIZED);
#endif
    choose(u);
}

// ---- Squares ----------------------------------------------------------------------------------------

static int squareIsSouthCopy(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_W) || y >= G32(RVA_MAP_H)) return 0;
    return (short)G16(copySquareAt(x, y) + SQUARE_TEMPERATURE) > 49;
}
static int squareGoldIncomeCopy(int player, int x, int y) { return GAME_FN(int (*)(int, int, int, int), RVA_SQUARE_INCOME)(player, x, y, 0); }

static int readCopyPlaneHolding(int x, int y) {
    for (int p = 0; p <= G32(RVA_LAST_PLANE) && p < planeTableRecords(); p++) {
        int px = planeField(p, PLANE_X), py = planeField(p, PLANE_Y);
        if (px <= x && x < px + planeField(p, PLANE_W) && py <= y && y < py + planeField(p, PLANE_H)) return p;
    }
    return -1;
}
static int readCopyLifeforceAround(int x, int y, int range, int scale) {
    int plane = readCopyPlaneHolding(x, y), total = 0;
    if (plane < 0) return 0;
    for (int sx = x - range; sx <= x + range; sx++)
        for (int sy = y - range; sy <= y + range; sy++) {
            if (readCopyPlaneHolding(sx, sy) != plane) continue;
            int terrain = copySquareTerrain(sx, sy);
            if (terrain < 0) continue;
            int distance = abs(x - sx) + abs(y - sy), life = (short)G16(RVA_TERRAIN_TABLE + (size_t)terrain * TERRAIN_SIZE + TERRAIN_LIFEFORCE), give;
            if (distance < 2) distance = 2;
            if (life < 1) {
                int ground = (short)G16(copySquareAt(sx, sy) + SQUARE_GROUND);
                if (ground != 0 && ground != 3) continue;
                give = scale * 4;
            } else {
                give = life * scale * 2;
            }
            total += give / distance;
        }
    return (total + 5) / 10;
}
static int squareLifeforceCopy(int x, int y) {
    if (readCopyPlaneHolding(x, y) < 0 || x < 0 || y < 0 || x >= G32(RVA_MAP_W) || y >= G32(RVA_MAP_H)) return 0;
    int terrain = copySquareTerrain(x, y);
    if (terrain < 0) return 0;
    int life = (short)G16(RVA_TERRAIN_TABLE + (size_t)terrain * TERRAIN_SIZE + TERRAIN_LIFEFORCE);
    if (life >= 0) return 0;
    int drain = -life, around = readCopyLifeforceAround(x, y, drain, drain * 100);
    if (drain <= 1) return around + 100 < 1500 ? around + 100 : 1500;
    if (drain == 2) return around + 300 < 5000 ? around + 300 : 5000;
    return around + 1000 < 10000 ? around + 1000 : 10000;
}

// ---- The AI -----------------------------------------------------------------------------------------

static int aiUnitStaysCopy(int u) {
    if (u < 0) return 0;
    if (GAME_FN(ReadCopyFlagsFn, RVA_UNIT_STATE_FLAGS)(u) >> 39 & 1) return 1;
    size_t at = readCopyUnitAt(u);
    int x = G32(RVA_UNIT_X + at), y = G32(RVA_UNIT_X + 4 + at);
    if (x < 0 || u > G32(RVA_UNIT_COUNT) || (short)G16(RVA_UNIT_MONSTER + at) < 0) return 0;
    return readCopyAbilitySum(u, READ_COPY_ABILITY_GUARD, 0) == 1 && GAME_FN(unsigned (*)(int, int), RVA_SQUARE_FORT_PARTS)(x, y) >> 9 & 1;
}
static int aiRitualMasterCopy(int player, int ability) {
    int best = 0, master = -1;
    for (int c = 0; c <= G32(RVA_LAST_COMMANDER) && c < COMMANDER_TABLE_MAX; c++) {
        int u = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE);
        if (u < 0 || readCopyUnitPlayer(u) != player || (short)G16(RVA_UNIT_HP + readCopyUnitAt(u)) <= 0) continue;
        int total = readCopyAbilitySum(u, ability, 1);
        int scaled = (int)((unsigned)total * 100u);
        if (total > 0 && scaled > best) best = scaled, master = c;
    }
    return master;
}
static int readCopyInGroup(int u, int c, int lead, int player) {
    return readCopyUnitPlayer(u) == player && (G32(RVA_UNIT_LEADER + readCopyUnitAt(u)) == c || u == lead);
}
static int readCopyGroupOf(int c, int *lead, int *player) {
    if (c < 0 || c > G32(RVA_LAST_COMMANDER) || c >= COMMANDER_TABLE_MAX) return 0;
    *lead = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE);
    if (*lead < 0) return 0;
    *player = *lead > G32(RVA_UNIT_COUNT) ? -1 : readCopyUnitPlayer(*lead);
    return G32(RVA_UNIT_X + readCopyUnitAt(*lead)) >= 0 && G32(RVA_UNIT_X + 4 + readCopyUnitAt(*lead)) >= 0;
}
static int aiCommanderBoatPlacesCopy(int c) {
    int lead, player, places = 0, without = 0;
    if (!readCopyGroupOf(c, &lead, &player)) return 0;
    for (int u = 0; u <= G32(RVA_UNIT_COUNT) && u < UNIT_TABLE_MAX; u++) {
        if (!readCopyInGroup(u, c, lead, player)) continue;
        int transport = readCopyAbilitySum(u, READ_COPY_ABILITY_TRANSPORT, 1);
        if (transport > 0) places += transport;
        else without++;
    }
    return places - without;
}
static int aiGroupBoatTotalCopy(int c) {
    int lead, player, places = 0;
    if (!readCopyGroupOf(c, &lead, &player)) return 0;
    for (int u = 0; u <= G32(RVA_UNIT_COUNT) && u < UNIT_TABLE_MAX; u++) {
        if (!readCopyInGroup(u, c, lead, player)) continue;
        int transport = readCopyAbilitySum(u, READ_COPY_ABILITY_TRANSPORT, 1);
        if (transport > 0) places += transport;
    }
    return places;
}
static int readCopyUnitHere(int u, int player, int x, int y) {
    size_t at = readCopyUnitAt(u);
    return readCopyUnitPlayer(u) == player && x >= 0 && (short)G16(RVA_UNIT_HP + at) > 0 && G32(RVA_UNIT_X + at) == x && G32(RVA_UNIT_X + 4 + at) == y;
}
static int aiShipsAtCopy(int player, int x, int y) {
    int ships = 0;
    for (int u = 0; u <= G32(RVA_UNIT_COUNT) && u < UNIT_TABLE_MAX; u++)
        if (readCopyUnitHere(u, player, x, y) && readCopyAbilitySum(u, READ_COPY_ABILITY_TRANSPORT, 1) > 0) ships++;
    return ships;
}
static int readCopyPortFreePlaces(int player, int x, int y) {
    int places = 0, others = 0;
    for (int u = 0; u <= G32(RVA_UNIT_COUNT) && u < UNIT_TABLE_MAX; u++) {
        if (!readCopyUnitHere(u, player, x, y)) continue;
        int transport = readCopyAbilitySum(u, READ_COPY_ABILITY_TRANSPORT, 1);
        if (transport > 0) {
            places += transport;
            continue;
        }
        if (*(const unsigned long long *)GAME_PTR(RVA_UNITS + readCopyUnitAt(u)) & READ_COPY_UNIT_NONSHIP) others++;
    }
    return places - others;
}
static int aiPortsBoatTotalsCopy(int player, int *ships) {
    int places = 0, shipCount = 0, depth = G32(RVA_MAP_DEPTH), width = G32(RVA_MAP_W), height = G32(RVA_MAP_H);
    for (int y = 0; y < height; y++)
        for (int x = 0; x < depth; x++) {
            int owner = x >= width ? -1 : *(const signed char *)GAME_PTR(copySquareAt(x, y) + SQUARE_OWNER);
            if (owner != player || (short)G16(RVA_SQUARE_ARMY + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * 2) <= 0) continue;
            int terrain = x < width ? copySquareTerrain(x, y) : -1;
            unsigned long long flags = terrain >= 0 ? copyTerrainFlags(terrain) : 0;
            if ((flags & READ_COPY_PORT_TERRAIN) != READ_COPY_PORT_TERRAIN) continue;
            int free = readCopyPortFreePlaces(player, x, y);
            places += free > 0 ? free : 0;
            shipCount += aiShipsAtCopy(player, x, y);
        }
    if (ships) *ships = shipCount;
    return places;
}
static int aiCountPlannersCopy(int player, int ritual) {
    int count = 0;
    for (int c = 0; c <= G32(RVA_LAST_COMMANDER) && c < COMMANDER_TABLE_MAX; c++) {
        int u = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE), plan = (short)G16(RVA_COMMANDER_AI_PLAN + (size_t)c * COMMANDER_SIZE);
        if (u < 0 || (short)G16(RVA_UNIT_HP + readCopyUnitAt(u)) <= 0 || readCopyUnitPlayer(u) != player || plan <= 0) continue;
        if (!strcmp((const char *)readCopyRitual(plan), (const char *)readCopyRitual(ritual))) count++;
    }
    return count;
}
static int aiPlanSquareBonusCopy(int c, int x, int y) {
    if (c < 0 || x < 0 || y < 0) return 0;
    int owner = c > G32(RVA_LAST_COMMANDER) || G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE) < 0 ? -1 : readCopyUnitPlayer(G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE));
    if (owner < 0) return 0;
    int plan = (short)G16(RVA_COMMANDER_AI_PLAN + (size_t)c * COMMANDER_SIZE);
    if (plan <= 0) return 0;
    const unsigned char *r = readCopyRitual(plan);
    int terrain = *(const int *)(r + RITUAL_TERRAIN);
    if (terrain == -1 || !GAME_FN(int (*)(int, int, int), RVA_TERRAIN_MATCH)(terrain, x, y)) return 0;
    if (GAME_FN(int (*)(int, int, int, int, int), RVA_RITUAL_CHECK_COSTS)(owner, c, plan, 100, 0) < 1) return 5;
    int level = *(const int *)(r + RITUAL_LEVEL), levelup = ritualFirstFx(r, READ_COPY_RITUAL_FX_LEVELUP, 0);
    return (level < levelup ? levelup : level) * 250;
}
static int readCopyRememberedStrength(int player, int x, int y) {
    if (x < 0 || y < 0) return 0;
    int (*strength)(int, int, int) = GAME_FN(int (*)(int, int, int), RVA_SQUARE_STRENGTH);
    if (player < 0) return strength(-1, x, y);
    int *memory = (int *)GAME_PTR(copySquareAt(x, y) + SQUARE_AI_STRENGTH + (size_t)player * 4);
    if (*memory < 0) *memory = strength(-1, x, y);
    return *memory;
}
static int readCopyTeam(int p) { return *(const short *)playerSlotAddress(RVA_SLOT_TEAM, p, PLAYER_BLOCK_SIZE); }
static int aiThreatNearCopy(int player, int x, int y, int comstr) {
    for (int sx = x - 3; sx <= x + 3; sx++)
        for (int sy = y - 3; sy <= y + 3; sy++) {
            if (sx < 0 || sy < 0 || sx >= G32(RVA_MAP_DEPTH) || sy >= G32(RVA_MAP_H)) continue;
            int owner = (short)G16(copySquareAt(sx, sy) + SQUARE_ARMY_OWNER);
            if (owner < 0) continue;
            int enemy = player < 0 || owner > 31 || player > 31 ||
                        (owner != player && (readCopyTeam(owner) < 1 || readCopyTeam(player) < 1 || readCopyTeam(owner) != readCopyTeam(player)));
            if (enemy && owner < 24 && comstr * 15 < readCopyRememberedStrength(player, sx, sy) * 10) return 1;
        }
    return 0;
}

// ---- Battle -----------------------------------------------------------------------------------------

static int readCopyCellUnit(int x, int y, int layer) { return G32(RVA_BATTLE_GRID + (size_t)x * COPY_BATTLE_COLUMN + (size_t)y * COPY_BATTLE_CELL + (size_t)layer); }
static int readCopyUnitCell(int u, int layer, int *x, int *y) {
    for (int row = 0; row < BATTLE_ROWS; row++)
        for (int col = 0; col < G32(RVA_BATTLE_WIDTH); col++)
            if (readCopyCellUnit(col, row, layer) == u) return *x = col, *y = row, 1;
    return 0;
}
static int readCopyUnitCellDistance(int u, int x, int y) {
    int ax = -1, ay = -1;
    if (u >= 0 && !readCopyUnitCell(u, 0, &ax, &ay)) readCopyUnitCell(u, 4, &ax, &ay);
    if (ax < 0 || ay < 0) return -1;
    int width = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(u), height = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(u), best = 99999;
    for (int cy = ay; cy < ay + height; cy++)
        for (int cx = ax; cx < ax + width; cx++) {
            int d = (cx - x) * (cx - x) + (cy - y) * (cy - y);
            if (d < best) best = d;
        }
    return best;
}
typedef int (*ReadCopyDistanceFn)(int, int, int *);
static int battleFearAtCopy(int x, int y, int player) {
    int target = readCopyCellUnit(x, y, 0), fear = 0, count = G32(RVA_UNIT_COUNT);
    if (target < 0) return 0;
    ReadCopyDistanceFn distance = GAME_FN(ReadCopyDistanceFn, RVA_BATTLE_UNIT_DISTANCE);
    int (*sight)(int, int) = GAME_FN(int (*)(int, int), RVA_BATTLE_LINE_OF_SIGHT);
    for (int cy = y - 6; cy <= y + 6; cy++)
        for (int cx = x - 6; cx <= x + 6; cx++) {
            if (cx < 0 || cy < 0 || cx >= G32(RVA_BATTLE_WIDTH) || cy >= BATTLE_ROWS) continue;
            unsigned char flags = *GAME_PTR(RVA_BATTLE_GRID + (size_t)cx * COPY_BATTLE_COLUMN + (size_t)cy * COPY_BATTLE_CELL + 8);
            if (flags & 0x80 && !playerSameTeam(target > count ? -1 : readCopyUnitPlayer(target), G32(RVA_BATTLE_PLAYER_B))) {
                if (readCopyUnitCellDistance(target, cx, cy) < 3) fear++;
                continue;
            }
            int u = readCopyCellUnit(cx, cy, 0);
            if (u < 0 || playerSameTeam(u > count ? -1 : readCopyUnitPlayer(u), player)) continue;
            int f = readCopyAbilitySum(u, READ_COPY_ABILITY_FEAR, 0);
            if (!f && GAME_FN(ReadCopyFlagsFn, RVA_UNIT_RESISTANCES)(u) >> 29 & 1) f = 1;
            if (distance(u, target, NULL) <= (f + 1) * f && sight(u, target) > 0) fear += f;
        }
    return fear;
}
static int battleLocalLeadershipCopy(int x, int y, int player) {
    if (x < 0 || y < 0 || x >= G32(RVA_BATTLE_WIDTH) || y >= BATTLE_ROWS) return 0;
    int target = readCopyCellUnit(x, y, 0), best = 0, count = G32(RVA_UNIT_COUNT);
    if (target < 0) return 0;
    ReadCopyDistanceFn distance = GAME_FN(ReadCopyDistanceFn, RVA_BATTLE_UNIT_DISTANCE);
    int (*sight)(int, int) = GAME_FN(int (*)(int, int), RVA_BATTLE_LINE_OF_SIGHT);
    for (int cy = y - 4; cy <= y + 4; cy++)
        for (int cx = x - 4; cx <= x + 4; cx++) {
            if (cx < 0 || cy < 0 || cx >= G32(RVA_BATTLE_WIDTH) || cy >= BATTLE_ROWS || (cx == x && cy == y)) continue;
            int u = readCopyCellUnit(cx, cy, 0);
            if (u < 0 || u > count || readCopyUnitPlayer(u) != player || (short)G16(RVA_UNIT_MONSTER + readCopyUnitAt(u)) < 0) continue;
            int lead = readCopyAbilitySum(u, READ_COPY_ABILITY_LOCAL_LEADERSHIP, 1);
            if (lead && distance(u, target, NULL) < 7 && sight(u, target) > 0 && lead >= best) best = lead;
        }
    return best;
}
static int battleSquareAbilityCopy(int x, int y, int ability) {
    if (x < 0 || y < 0 || x >= G32(RVA_BATTLE_WIDTH) || y >= BATTLE_ROWS) return 0;
    int total = readCopyAbilitySum(GAME_FN(int (*)(int, int), RVA_BATTLE_UNIT_AT)(x, y), ability, 1);
    int second = readCopyCellUnit(x, y, 4);
    return second >= 0 ? total + readCopyAbilitySum(second, ability, 1) : total;
}
static int battleInRangeCopy(int a, int b, int range, long long flags) {
    if (GAME_FN(ReadCopyDistanceFn, RVA_BATTLE_UNIT_DISTANCE)(a, b, NULL) > (range + 1) * range) return 0;
    int seen = GAME_FN(int (*)(int, int), RVA_BATTLE_LINE_OF_SIGHT)(a, b);
    if (seen >= 1) return 2;
    return (flags >> 33 & 1) ? 1 : 0;
}
#define READ_COPY_ABILITY_CELL_SAFE 0x4f6
static int readCopyCellTest(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_BATTLE_WIDTH) || y >= BATTLE_ROWS) return 0;
    unsigned short ground = (unsigned short)G16(RVA_BATTLE_GRID + (size_t)x * COPY_BATTLE_COLUMN + (size_t)y * COPY_BATTLE_CELL + 0xc);
    if ((unsigned short)(ground - 0xb) > 2) return 0;
    int u = readCopyCellUnit(x, y, 4);
    if (u < 0 || (short)G16(RVA_UNIT_HP + readCopyUnitAt(u)) < 1) return 1;
    return readCopyAbilitySum(u, READ_COPY_ABILITY_CELL_SAFE, 1) > 0;
}
static int readCopyTokenSafe(int u, int x, int y) {
    int x2 = x, y2 = y;
    if (x >= 0) x2 = x - 1 + GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(u), y2 = y - 1 + GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(u);
    return readCopyCellTest(x, y) && readCopyCellTest(x2, y2) && !(GAME_FN(ReadCopyFlagsFn, RVA_UNIT_FLAGS)(u) & 0xc1);
}
static int battleKillerMoveCopy(int u, int *dx, int *dy) {
    *dx = 0, *dy = 0;
    int ax = -1, ay = -1;
    if (u < 0 || (!readCopyUnitCell(u, 0, &ax, &ay) && !readCopyUnitCell(u, 4, &ax, &ay))) return 0;
    int (*stand)(int, int, int, int) = GAME_FN(int (*)(int, int, int, int), RVA_BATTLE_CAN_STAND);
    int (*unitAt)(int, int) = GAME_FN(int (*)(int, int), RVA_BATTLE_UNIT_AT);
    int width = G32(RVA_BATTLE_WIDTH), count = G32(RVA_UNIT_COUNT);
    for (int cx = ax - 1; cx <= ax + 1; cx++)
        for (int cy = ay - 1; cy <= ay + 1; cy++) {
            if (cx < 0 || cy < 0 || cx >= width || cy >= BATTLE_ROWS || readCopyCellUnit(cx, cy, 0) >= 0 || !stand(u, cx, cy, 0)) continue;
            int enemies = 0, w = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(u), h = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(u);
            int owner = u > count ? -1 : readCopyUnitPlayer(u);
            for (int ey = cy - 1; ey <= h + cy; ey++)
                for (int ex = cx - 1; ex <= w + cx; ex++) {
                    if (ex < 0 || ey < 0 || ex >= G32(RVA_BATTLE_WIDTH) || ey >= BATTLE_ROWS) continue;
                    int other = unitAt(ex, ey);
                    if (other < 0) continue;
                    if (!playerSameTeam(other > G32(RVA_UNIT_COUNT) ? -1 : readCopyUnitPlayer(other), owner)) enemies++;
                }
            if (enemies <= 0) continue;
            if (readCopyTokenSafe(u, cx, cy) && (G32(RVA_BATTLE_ROUND) <= 19 || GAME_FN(int (*)(int), RVA_RANDOM)(100) <= 89)) continue;
            *dx = cx - ax, *dy = cy - ay;
            int cell = readCopyCellTest(cx, cy);
            const char *name = GAME_FN(const char *(*)(int), RVA_UNIT_SHOWN_NAME)(u);
            GAME_FN(void (*)(const char *, ...), RVA_TRACE_C)("fightkillermove for %s  %d,%d -> %d,%d(ws%d)\n", name, *dx, *dy, cx, cy, cell);
            return 1;
        }
    return 0;
}
#define READ_COPY_ABILITY_SIDE_GATE 0x58
#define READ_COPY_ABILITY_SIDE_EXTRA 0x53
static int countSideUnitsCopy(int team, int x, int y) {
    int total = 0;
    for (int u = 0; u <= G32(RVA_UNIT_COUNT) && u < UNIT_TABLE_MAX; u++) {
        size_t at = readCopyUnitAt(u);
        if (G32(RVA_UNIT_X + at) != x || G32(RVA_UNIT_X + 4 + at) != y || (short)G16(RVA_UNIT_HP + at) <= 0) continue;
        if (team != -1 && !playerSameTeam(readCopyUnitPlayer(u), team)) continue;
        int w = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(u), h = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(u);
        total += w * h;
        if ((short)G16(RVA_UNIT_MONSTER + at) >= 0 && readCopyAbilitySum(u, READ_COPY_ABILITY_SIDE_GATE, 0)) {
            int extra = readCopyAbilitySum(u, READ_COPY_ABILITY_SIDE_EXTRA, 1);
            total += (extra > 4 ? 5 : extra) * 2;
        }
        if ((short)G16(RVA_UNIT_MONSTER + at) >= 0 && GAME_FN(ReadCopyFlagsFn, RVA_UNIT_FLAGS)(u) & 0x40000000000004ULL)
            total += GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(u) * GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(u) * 2 + 1;
        if (w * h > 8) total += 3;
    }
    return total;
}

// ---- World building -----------------------------------------------------------------------------------

static int readCopySquareIsSea(int x, int y) {
    if (x < 0 || x >= G32(RVA_MAP_W) || y < 0 || y >= G32(RVA_MAP_H)) return -1;
    int terrain = copySquareTerrain(x, y);
    if (terrain >= 0 && copyTerrainFlags(terrain) & 4) return 1;
    return G32(RVA_SQUARE_FLAGS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE) >> 13 & 1;
}
static int readCopySquareDry(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_W) || y >= G32(RVA_MAP_H)) return 0;
    int terrain = copySquareTerrain(x, y);
    return terrain >= 0 && !(copyTerrainFlags(terrain) & 4) && terrain != 0x32;
}
static int readCopySideWater(int x, int y, int sx, int sy) {
    if (x <= 0 || x >= G32(RVA_MAP_W) - 1 || y <= 0 || y >= G32(RVA_MAP_H) - 1 || !readCopySquareIsSea(x, y)) return 0;
    unsigned terrain = (unsigned)copySquareTerrain(x, y);
    if (terrain - 10u <= 1 || terrain - 0x84u <= 1 || !readCopySquareDry(x + sx, y + sy)) return 0;
    return !GAME_FN(int (*)(int, int), RVA_SQUARE_WATERLIKE)(x + sx, y + sy);
}
static int squareSideWaterCopy(int x, int y) {
    return readCopySideWater(x, y, 0, -1) || readCopySideWater(x, y, 0, 1) || readCopySideWater(x, y, 1, 0) || readCopySideWater(x, y, -1, 0);
}

// ---- Chop and burn ------------------------------------------------------------------------------------

static unsigned long long readCopyUnitSpellFlags(int u) {
    unsigned long long flags = 0;
    if (u < 0) return 0;
    int (*pathLevel)(int, int) = GAME_FN(int (*)(int, int), RVA_UNIT_PATH_LEVEL);
    for (int i = 0; i < 60; i++) {
        int spell = (short)G16(RVA_UNIT_KNOWN_SPELLS + readCopyUnitAt(u) + (size_t)i * 2);
        if (spell < 0) return flags;
        const SpellRecord *r = spellSlot(spell);
        if (r->path >= 0 && r->level <= pathLevel(u, r->path)) flags |= (unsigned long long)(long long)(int)r->flags;
    }
    return flags;
}
#define READ_COPY_ABILITY_BURNFOREST 0x1b6
static int armyBurnValueCopy(int c) {
    if (c < 0 || c > G32(RVA_LAST_COMMANDER) || c >= COMMANDER_TABLE_MAX) return 0;
    int lead = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE), best = 0;
    if (lead < 0) return 0;
    for (int u = 0; u <= G32(RVA_UNIT_COUNT) && u < UNIT_TABLE_MAX; u++) {
        size_t at = readCopyUnitAt(u);
        if ((short)G16(RVA_UNIT_HP + at) <= 0 || (u != lead && G32(RVA_UNIT_LEADER + at) != c)) continue;
        int burn = readCopyAbilitySum(u, READ_COPY_ABILITY_BURNFOREST, 1);
        if (burn < best) burn = best;
        best = burn;
        if (readCopyUnitSpellFlags(u) & 1 && best < 1) best = 1;
    }
    return best;
}
#endif

// ---- Rows both builds run --------------------------------------------------------------------------

static int unitBattleCell(int unit, int *x, int *y);
// @platform-split battle_square_ability: Windows calls RVA_BATTLE_SQUARE_ABILITY for the unit's cell, Mac runs CoE5SE's copy of it
static int battleSquareAbilityOfUnit(int unit, int ability) {
    int x, y;
    if (!unitBattleCell(unit, &x, &y)) return 0;
#ifdef __APPLE__
    return battleSquareAbilityCopy(x, y, ability);
#else
    return GAME_FN(int (*)(int, int, int), RVA_BATTLE_SQUARE_ABILITY)(x, y, ability);
#endif
}
