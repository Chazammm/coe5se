#pragma once

#if defined(__APPLE__) || defined(COE5SE_COPY_SELFCHECK)
#define COPY_MONSTER_FLAGS (RVA_MONSTER_FLAGS - RVA_MONSTER_TABLE)
#define COPY_BATTLE_CELL 24
#define COPY_BATTLE_COLUMN (BATTLE_ROWS * COPY_BATTLE_CELL)

static int copyUnitMonster(int u) { return (short)G16(RVA_UNIT_MONSTER + (size_t)u * UNIT_SIZE); }
static unsigned long long copyMonsterWord(int monster, int at) {
    const unsigned char *m = monsterRecord(monster);
    unsigned long long v = 0;
    if (m) memcpy(&v, m + at, sizeof v);
    return v;
}
static int copySquareTerrain(int x, int y) {
    return (short)G16(RVA_SQUARE_RECORDS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE + SQUARE_TERRAIN_TYPE);
}
static unsigned long long copyTerrainFlags(int terrain) {
    return *(const unsigned long long *)GAME_PTR(RVA_TERRAIN_TABLE + (size_t)terrain * TERRAIN_SIZE + TERRAIN_FLAGS);
}
static int copyNameMatches(const char *a, const char *b) {
    for (int i = 0; i < 500000; i++) {
        int ca = (unsigned char)(a[i] - 'A') < 26 ? a[i] + 32 : a[i], cb = (unsigned char)(b[i] - 'A') < 26 ? b[i] + 32 : b[i];
        if (!a[i] && !b[i]) return 1;
        if (ca != cb) return 0;
    }
    return 1;
}

// ---- Planes ---------------------------------------------------------------------------------------

static int planesAlikeCopy(int a, int b) {
    return a >= 0 && b >= 0 && (a == b || ((a & ~3) == 0 && a != 1 && (b & ~3) == 0 && b != 1));
}
static int planeOrderStepCopy(int plane, int dir) {
    int at = plane < 0 ? 0 : plane < 10 ? plane : 10, step = dir < 1 ? dir : 1;
    if (!step) return at;
    if (plane == 0) {
        if (step == -1) return 3;
        if (step == 1) return 2;
        return at;
    } else if (plane == 1) {
        if (step == 1) return 1;
    } else if (plane == 9) {
        if (step == -1) return 9;
    } else if (plane == 2) {
        if (step == -1) return 0;
    } else if (plane == 3 && step == 1) {
        return 0;
    }
    at -= step;
    return at < 0 ? 0 : at < 10 ? at : 10;
}
static int planeIsReachableCopy(int plane, int x, int y) {
    int last = G32(RVA_LAST_PLANE);
    for (int p = 0; p <= last && p < planeTableRecords(); p++) {
        int px = planeField(p, PLANE_X), py = planeField(p, PLANE_Y);
        if (px > x || x >= planeField(p, PLANE_W) + px || py > y || y >= py + planeField(p, PLANE_H)) continue;
        if (plane == p) return 1;
        int cx = planeField(plane, PLANE_W) / 2 + planeField(plane, PLANE_X), cy = planeField(plane, PLANE_H) / 2 + planeField(plane, PLANE_Y);
        GameMonsterPath path = GAME_FN(GameMonsterPath, RVA_MONSTER_PATH);
        void (*trace)(const char *, ...) = GAME_FN(void (*)(const char *, ...), RVA_TRACE);
        int r = path(SKY_PATH_MODE, x, y, cx, cy, SKY_PATH_AP_MAX, 0, 0);
        trace("planeisreachable %d,%d -> pln %d = %d\n", x, y, plane, r);
        if (r >= 0) return 1;
        r = path(SKY_PATH_MODE, x, y, cx - SKY_PATH_SHIFT, cy, SKY_PATH_AP_MAX, 0, 0);
        trace("planeisreachable %d,%d -> pln %d = %d\n", x - SKY_PATH_SHIFT, y, plane, r);
        if (r >= 0) return 1;
        r = path(SKY_PATH_MODE, x, y, cx + SKY_PATH_SHIFT, cy, SKY_PATH_AP_MAX, 0, 0);
        trace("planeisreachable %d,%d -> pln %d = %d\n", x + SKY_PATH_SHIFT, y, plane, r);
        return r >= 0;
    }
    return 0;
}
static int countTerrainCodeCopy(int plane, int code) {
    int (*match)(int, int, int) = GAME_FN(int (*)(int, int, int), RVA_TERRAIN_MATCH);
    int count = 0, x0 = planeField(plane, PLANE_X), y0 = planeField(plane, PLANE_Y);
    for (int x = x0; x < planeField(plane, PLANE_W) + x0; x++)
        for (int y = y0; y < planeField(plane, PLANE_H) + y0; y++)
            if (match(code, x, y)) count++;
    return count;
}

// ---- Units ----------------------------------------------------------------------------------------

static int unitPlaneCopy(int u) {
    if (u < 0) return -1;
    return GAME_FN(int (*)(int, int), RVA_PLANE_AT)(G32(RVA_UNIT_X + (size_t)u * UNIT_SIZE), G32(RVA_UNIT_X + 4 + (size_t)u * UNIT_SIZE));
}
#define COPY_ABILITY_ACIDRES 0x82
static int unitAcidResistanceCopy(int u) { return unitAbilityTotal(u, COPY_ABILITY_ACIDRES, 1); }
static int unitBattleSlownessCopy(int u) {
    if (u < 0 || copyUnitMonster(u) < 0) return 0;
    unsigned long long move = copyMonsterWord(copyUnitMonster(u), MONSTER_MOVE_FLAGS);
    return (move >> 54) & 1 ? 2 : (int)((move >> 53) & 1);
}
static int unitBattleFastCopy(int u) {
    if (u < 0 || copyUnitMonster(u) < 0) return 0;
    unsigned long long move = copyMonsterWord(copyUnitMonster(u), MONSTER_MOVE_FLAGS);
    if (move & (1ULL << 33) || move & 0x10) return 1;
    return (int)((GAME_FN(unsigned long long (*)(int), RVA_UNIT_BENEFITS)(u) >> 30) & 1);
}
#define COPY_INCAPACITATED_BITS (0x8ULL | 0x10 | 0x40 | 0x100 | 0x200 | 0x400 | 0x1000 | 0x80000 | 0x100000000ULL | 0x1000000000ULL)
static int unitIncapacitatedCopy(int u) {
    if (u < 0 || u > G32(RVA_UNIT_COUNT)) return 0;
    size_t at = (size_t)u * UNIT_SIZE;
    int x = G32(RVA_UNIT_X + at);
    unsigned long long state = *(const unsigned long long *)GAME_PTR(RVA_UNITS + at);
    if ((short)G16(RVA_UNIT_HP + at) > 0 && x > -9 && x + 6 > 4 && !(state & COPY_INCAPACITATED_BITS)) return (int)((state >> 40) & 1);
    return 1;
}
static int unitAfflictionCountCopy(int u) {
    if (u < 0) return 0;
    return __builtin_popcountll((GAME_FN(unsigned long long (*)(int), RVA_UNIT_STATE_FLAGS)(u) >> 20) & 0x7ff);
}
static int unitMonsterFlagCopy(int u, int bit) {
    if (u < 0 || copyUnitMonster(u) < 0) return 0;
    return (int)((copyMonsterWord(copyUnitMonster(u), COPY_MONSTER_FLAGS) >> bit) & 1);
}
static int unitIsUndeadCopy(int u) { return unitMonsterFlagCopy(u, 4); }
static int unitIsDemonCopy(int u) { return unitMonsterFlagCopy(u, 33); }
static int squareHasSeenUnitCopy(int player, int x, int y) {
    if (x < 0 || y < 0) return 0;
    int (*visible)(int, int, int, int) = GAME_FN(int (*)(int, int, int, int), RVA_UNIT_VISIBLE_TO);
    for (int u = 0; u <= G32(RVA_UNIT_COUNT) && u < UNIT_TABLE_MAX; u++) {
        size_t at = (size_t)u * UNIT_SIZE;
        int ux = G32(RVA_UNIT_X + at), uy = G32(RVA_UNIT_X + 4 + at);
        if (ux == x && uy == y && (short)G16(RVA_UNIT_HP + at) > 0 && visible(player, u, ux, uy) > 0) return 1;
    }
    return 0;
}

// ---- Spells and rituals ---------------------------------------------------------------------------

static int playerBlessingValueCopy(int player, int ability) {
    if (player < 0 || ability <= 0) return 0;
    for (int i = 0; i < 24; i++) {
        int id = PSLOT32(RVA_PLAYER_BLESSINGS + (size_t)i * 8, player, PLAYER_BLOCK_SIZE);
        if (!id) return 0;
        if (id == ability) return PSLOT32(RVA_PLAYER_BLESSINGS + (size_t)i * 8 + 4, player, PLAYER_BLOCK_SIZE);
    }
    return 0;
}
static int spellPathWeightCopy(int path) {
    unsigned p = (unsigned)path;
    if (path < 0) return 0;
    if (p == 0x19 || p - 0xc <= 3) return 75;
    if (p == 0x12) return 50;
    if (p == 0x14 || (p <= 0x3b && (0x800000580000000ULL >> (p & 0x3f)) & 1) || p == 0x21 || p == 0x3a) return 75;
    return p == 0x37 ? 75 : 100;
}
static UNUSED int monsterBattleSizeCopy(int monster) {
    if (monster < 0) return 1;
    if (battlesizeWidth(monster)) return battlesizeWidth(monster);
    unsigned long long flags = copyMonsterWord(monster, COPY_MONSTER_FLAGS);
    if (flags & (1ULL << 52)) return 1;
    if (flags & (1ULL << 53)) return 2;
    int huge = (copyMonsterWord(monster, MONSTER_MOVE_FLAGS) & 2) != 0, width = 0;
    GAME_FN(void (*)(int, int *, int *, int *), RVA_MONSTER_SPRITE_SIZE)(*(const int *)(monsterRecord(monster) + (RVA_MONSTER_SPRITES - RVA_MONSTER_TABLE)), &width, NULL, NULL);
    return width >= 151 || huge ? 3 : width >= 101 ? 2 : 1;
}
static int monsterHasRangedCopy(int monster) {
    const unsigned char *m = monster < 0 ? NULL : monsterRecord(monster);
    if (!m || *(const short *)(m + MONSTER_RANK) == 1) return 0;
    for (int i = 0; i < MONSTER_ATTACK_MAX; i++) {
        int type = *(const int *)(m + MONSTER_ATTACKS + (size_t)i * MONSTER_ATTACK_SIZE);
        if (type <= 0) return 0;
        if (type <= 22 && (0x6cc8ecu >> type) & 1u) return 1;
    }
    return 0;
}
static int memorizeAllowanceCopy(int u, int path) {
    int level = GAME_FN(int (*)(int, int), RVA_UNIT_PATH_LEVEL)(u, path);
    return level < 1 ? 0 : level + 1;
}
static int countMemorizedOfTypeCopy(int u, int path, int type) {
    if (u < 0) return 0;
    int count = 0;
    for (int i = 0; i < 30; i++) {
        int spell = (short)G16(RVA_UNIT_MEMORIZED_SPELLS + (size_t)u * UNIT_SIZE + (size_t)i * 2);
        if (spell < 0) return count;
        const SpellRecord *r = spellSlot(spell);
        if ((path == -1 || r->path == path) &&
            (type == -1 || (r->weapon > 0 && *(const int *)(tableAddress(TABLE_WEAPON) + (size_t)r->weapon * WEAPON_SIZE + WEAPON_DMGTYPE) == type)))
            count++;
    }
    return count;
}
static int countSpellsCopy(int path, int level) {
    int count = 0;
    for (int s = 0; spellSlot(s)->path != SPELL_END_PATH; s++)
        if ((path == -1 || spellSlot(s)->path == path) && (level == -1 || spellSlot(s)->level == level)) count++;
    return count;
}
static int isSpellAttackCopy(int type) { return (unsigned)type <= 0x13 && (0x8c048u >> type) & 1; }
static int findSpellCopy(const char *name) {
    for (int s = 0; spellSlot(s)->path != SPELL_END_PATH; s++)
        if (spellSlot(s)->name && copyNameMatches(spellSlot(s)->name, name)) return s;
    GAME_FN(void (*)(const char *, ...), RVA_TRACE)("*** No spell called %s\n", name);
    return -1;
}
static int findRitualCopy(const char *name, int n) {
    int found = -1;
    for (int slot = 0; slot <= G32(RVA_RITUAL_COUNT) && slot < ritualSlotCount(); slot++)
        if (copyNameMatches((const char *)GAME_PTR(ritualTableRva() + (size_t)slot * RITUAL_SIZE), name) && ++found == n) return slot;
    return -1;
}
static int randomRitualCopy(int ritpow, int level) {
    if (ritualExpansionActive) return randomRitual(ritpow, level);
    static int matches[VANILLA_RITUAL_SLOTS];
    int count = 0;
    for (int slot = 1; slot < VANILLA_RITUAL_SLOTS; slot++) {
        const unsigned char *r = GAME_PTR(RVA_RITUAL_TABLE + (size_t)slot * RITUAL_SIZE);
        if ((*(const int *)(r + RITUAL_RITPOW) == ritpow || ritpow == -1) && (*(const int *)(r + RITUAL_LEVEL) == level || level == -1))
            matches[count++] = slot;
    }
    if (count < 1) return 0;
    return matches[count > 1 ? GAME_FN(int (*)(int), RVA_RANDOM)(count) : 0];
}
static int commanderKnowsRitualCopy(int c, int slot) {
    if (c < 0) return 0;
    for (int i = 0; i < 64; i++) {
        int known = (short)G16(RVA_COMMANDER_KNOWN_RITUALS + (size_t)c * COMMANDER_SIZE + (size_t)i * 2);
        if (known < 1) return 0;
        if (known == slot) return 1;
    }
    return 0;
}

static int commanderUseItemsCopy(int c) {
    if (c < 0 || c > G32(RVA_LAST_COMMANDER)) return 0;
    int u = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE), used = 0;
    if (u < 0) return 0;
    int (*rate)(int, int) = GAME_FN(int (*)(int, int), RVA_AI_ITEM_USE_RATING);
    void (*use)(int, int) = GAME_FN(void (*)(int, int), RVA_ITEM_USE);
    for (int i = 0; i < ITEM_POSITION_MAX; i++) {
        int item = unitItemAt(u, i);
        if (item >= 0 && rate(u, item) >= 2) use(u, item), used++;
    }
    return used;
}

// ---- Factions -------------------------------------------------------------------------------------

static int playerHasCommanderCopy(int player) {
    for (int c = 0; c <= G32(RVA_LAST_COMMANDER) && c < COMMANDER_TABLE_MAX; c++) {
        int u = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE);
        if (u < 0 || u > G32(RVA_UNIT_COUNT)) continue;
        if ((short)G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE) > 0 && (short)G16(RVA_UNIT_PLAYER + (size_t)u * UNIT_SIZE) == player) return 1;
    }
    return 0;
}
static int playerClassAbilityCopy(int player, int ability) {
    if (player < 0) return 0;
    int cls = PSLOT16(RVA_SLOT_CLASS, player, 2);
    if (cls < 0 || cls > G32(RVA_CLASS_COUNT) || cls >= tableRecords(TABLE_CLASS)) return 0;
    const int *pairs = (const int *)(tableAddress(TABLE_CLASS) + (size_t)cls * CLASS_SIZE + CLASS_ABILITIES);
    for (int i = 0; i <= 0x1e && pairs[i]; i += 2)
        if (pairs[i] == ability) return pairs[i + 1];
    return 0;
}
static int copyIncomeBonusPercent(int player, int ability, int fx, unsigned long long terrainBit) {
    if (player < 0) return 0;
    int total = GAME_FN(int (*)(int, int, int), RVA_PLAYER_ABILITY)(player, ability, 0);
    int mapW = G32(RVA_MAP_W), mapH = G32(RVA_MAP_H);
    long long effects = 0;
    for (int e = 0; e <= G32(RVA_LAST_ENCHANT); e++) {
        const unsigned char *rec = GAME_PTR(RVA_ENCHANTS + (size_t)e * ENCHANT_SIZE);
        int ritual = *(const int *)(rec + ENCHANT_RITUAL), x = *(const int *)(rec + ENCHANT_X), y = *(const int *)(rec + ENCHANT_Y);
        if (!*(const int *)(rec + ENCHANT_KIND) || ritual <= 0 || *(const int *)(rec + ENCHANT_OWNER) != player) continue;
        int owner = x < 0 || y < 0 || x >= mapW || y >= mapH ? -1
                                                             : *(const signed char *)GAME_PTR(RVA_SQUARE_RECORDS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE + SQUARE_OWNER);
        if (!playerSameTeam(owner, player)) continue;
        const unsigned char *effect = GAME_PTR(ritualTableRva() + (size_t)ritual * RITUAL_SIZE + RITUAL_FX);
        for (int i = 0; i < 20; i++) {
            if (*(const int *)(effect + (size_t)i * RITUAL_FX_SIZE) < 1) break;
            if (*(const int *)(effect + (size_t)i * RITUAL_FX_SIZE) == fx) {
                effects += *(const long long *)(effect + (size_t)i * RITUAL_FX_SIZE + 8);
                break;
            }
        }
    }
    int fromEffects = (int)effects < 100 ? (int)effects : 100;
    total += fromEffects;
    for (int x = 0; x < G32(RVA_MAP_DEPTH); x++)
        for (int y = 0; y < mapH; y++) {
            if (x >= mapW) continue;
            size_t square = ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE;
            if (*(const signed char *)GAME_PTR(RVA_SQUARE_RECORDS + square + SQUARE_OWNER) != player) continue;
            int terrain = copySquareTerrain(x, y);
            if (terrain >= 0 && (unsigned)copyTerrainFlags(terrain) & terrainBit) total += 25;
        }
    return total;
}
#define COPY_ABILITY_IRONBONUS 0xc9
#define COPY_RITUAL_FX_IRONBONUS 0x489
#define COPY_TERRAIN_IRON_BONUS (1ULL << 9)
static int ironBonusPercentCopy(int player) { return copyIncomeBonusPercent(player, COPY_ABILITY_IRONBONUS, COPY_RITUAL_FX_IRONBONUS, COPY_TERRAIN_IRON_BONUS); }
static int countSlotsWithControllerCopy(int controller) {
    int count = 0;
    for (int p = 0; p < PLAYER_SLOTS_GAME; p++)
        if (PSLOT16(RVA_SLOT_CONTROLLER, p, 2) == controller) count++;
    return count;
}

// ---- Squares --------------------------------------------------------------------------------------

static int terrainBurnableAtCopy(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_W) || y >= G32(RVA_MAP_H) || copySquareTerrain(x, y) < 0) return 0;
    return (int)((copyTerrainFlags(copySquareTerrain(x, y)) >> 1) & 1);
}
static int squareIsNorthCopy(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_W) || y >= G32(RVA_MAP_H)) return 0;
    return (short)G16(RVA_SQUARE_RECORDS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE + SQUARE_TEMPERATURE) < 1;
}
static int squareIsWetCopy(int x, int y) {
    if (x < 0 || x >= G32(RVA_MAP_W) || y < 0 || y >= G32(RVA_MAP_H)) return 0;
    if (G32(RVA_SQUARE_FLAGS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE) & SQUARE_FLAG_FLOODED) return 1;
    int terrain = copySquareTerrain(x, y);
    if (terrain == 10 || terrain == 11 || terrain == 0x127 || terrain == 0x158 || terrain < 0) return 0;
    return (copyTerrainFlags(terrain) & 4) != 0;
}
static int armyAtForPlayerCopy(int player, int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_DEPTH) || y >= G32(RVA_MAP_H)) return 0;
    size_t at = (size_t)x * MAP_COLUMN_SQUARES + (size_t)y;
    short army = G16(RVA_SQUARE_ARMY + at * 2);
    if (!army) return 0;
    if (player < 0 || army < 0) return 1;
    return playerSameTeam((short)G16(RVA_SQUARE_RECORDS + at * SQUARE_SIZE + SQUARE_ARMY_OWNER), player);
}
static int weaponExtraCopy(int weapon, int id) {
    if (weapon <= 0) return 0;
    const int *pairs = (const int *)(tableAddress(TABLE_WEAPON) + (size_t)weapon * WEAPON_SIZE + WEAPON_EXTRAS);
    for (int i = 0; i < 4 && pairs[i]; i += 2)
        if (pairs[i] == id) return pairs[i + 1];
    return 0;
}

// ---- The AI ---------------------------------------------------------------------------------------

static int aiCountAllyTargetsNearCopy(int player, int x, int y, int commander, int range) {
    int count = 0;
    for (int c = 0; c <= G32(RVA_LAST_COMMANDER) && c < COMMANDER_TABLE_MAX; c++) {
        int u = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE);
        if (u < 0 || (short)G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE) <= 0) continue;
        int owner = (short)G16(RVA_UNIT_PLAYER + (size_t)u * UNIT_SIZE);
        int dx = (short)G16(RVA_COMMANDER_AI_TARGET_X + (size_t)c * 2) - x, dy = (short)G16(RVA_COMMANDER_AI_TARGET_Y + (size_t)c * 2) - y;
        if (playerSameTeam(owner, player) && abs(dx) <= range && abs(dy) <= range && c != commander) count++;
    }
    return count;
}

// ---- Battle ---------------------------------------------------------------------------------------

#define COPY_BATTLE_LIST_MAX 0x7800
#define COPY_UNIMPORTANT (1ULL << 59)
static int battleWinnerCopy(void) {
    static int list[COPY_BATTLE_LIST_MAX];
    int n = 0, width = G32(RVA_BATTLE_WIDTH);
    for (int row = 0; row < BATTLE_ROWS; row++)
        for (int col = 0; col < width; col++) {
            int u = G32(RVA_BATTLE_GRID + (size_t)col * COPY_BATTLE_COLUMN + (size_t)row * COPY_BATTLE_CELL);
            if (u >= 0 && n < COPY_BATTLE_LIST_MAX) list[n++] = u;
        }
    for (int u = 0; u <= G32(RVA_UNIT_COUNT) && u < UNIT_TABLE_MAX; u++) {
        if ((unsigned)(G32(RVA_UNIT_X + (size_t)u * UNIT_SIZE) + 8) >= 2) continue;
        int listed = 0;
        for (int i = 0; i < n && !listed; i++) listed = list[i] == u;
        if (!listed && n < COPY_BATTLE_LIST_MAX) list[n++] = u;
    }
    void (*trace)(const char *, ...) = GAME_FN(void (*)(const char *, ...), RVA_TRACE);
    int winner = -2;
    while (n > 0) {
        int u = list[--n];
        if ((short)G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE) <= 0 || GAME_FN(unsigned long long (*)(int), RVA_UNIT_RESISTANCES)(u) & COPY_UNIMPORTANT) continue;
        int owner = u > G32(RVA_UNIT_COUNT) ? -1 : (short)G16(RVA_UNIT_PLAYER + (size_t)u * UNIT_SIZE);
        if (winner == -2) {
            winner = owner;
        } else if (!playerSameTeam(winner, owner)) {
            trace("No fightwinner %d %d\n", winner, owner);
            return -1;
        }
    }
    trace("Fightwinner: %d\n", winner);
    return winner;
}
static void battleEnemyExtentCopy(int player, int *low, int *high) {
    int lo = 9999, hi = -1;
    int (*unitAt)(int, int) = GAME_FN(int (*)(int, int), RVA_BATTLE_UNIT_AT);
    for (int row = 0; row < BATTLE_ROWS; row++)
        for (int col = 0; col < G32(RVA_BATTLE_WIDTH); col++) {
            int u = unitAt(col, row);
            if (u < 0) continue;
            if (u <= G32(RVA_UNIT_COUNT) && playerSameTeam((short)G16(RVA_UNIT_PLAYER + (size_t)u * UNIT_SIZE), player)) continue;
            if (col < lo) lo = col;
            if (col > hi) hi = col;
        }
    *low = lo, *high = hi;
}
// ---- Teleports ------------------------------------------------------------------------------------

static void teleportUnitCopy(int unit, int x, int y) {
    GAME_FN(void (*)(int, int, int, int, int, int, int), RVA_TELEPORT_ARMY)(unit, 0, -1, -1, x, y, 0);
}
static void teleportSquareCopy(int fromX, int fromY, int toX, int toY) {
    GAME_FN(void (*)(int, int, int, int, int, int, int), RVA_TELEPORT_ARMY)(-1, 0, fromX, fromY, toX, toY, 0);
}
// ---- Commanders and units -------------------------------------------------------------------------

static int commanderLearnRitualCopy(int c, int slot) {
    if (c < 0) return 0;
    for (int i = 0; i < 64; i++) {
        size_t at = RVA_COMMANDER_KNOWN_RITUALS + (size_t)c * COMMANDER_SIZE + (size_t)i * 2;
        if ((short)G16(at) > 0) continue;
        G16(at) = (unsigned short)slot;
        if (i + 1 < 64) G16(at + 2) = 0;
        return 1;
    }
    return 0;
}
static void unitAddXpCopy(int u, int xp) {
    if (u < 0 || GAME_FN(int (*)(int), RVA_UNIT_MINDLESS)(u)) return;
    int (*maxHp)(int) = GAME_FN(int (*)(int), RVA_UNIT_MAX_HP);
    int before = maxHp(u);
    short sum = (short)(unsigned short)((unsigned short)G16(RVA_UNIT_XP + (size_t)u * UNIT_SIZE) + (unsigned short)xp);
    if (sum <= 0) sum = 0;
    if (sum > 9999) sum = 9999;
    G16(RVA_UNIT_XP + (size_t)u * UNIT_SIZE) = (unsigned short)sum;
    int gain = maxHp(u) - before;
    short hp = (short)G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE);
    if (gain > 0 && hp > 0) G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE) = (unsigned short)(hp + gain);
}
static void setSquareOwnerCopy(int x, int y, int player) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_W) || y >= G32(RVA_MAP_H) || player >= 32) return;
    G8(RVA_SQUARE_RECORDS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE + SQUARE_OWNER) = (unsigned char)player;
}
static int unitRemoveTempAbilityCopy(int u, int ability) {
    if (u < 0) return 0;
    for (int i = 0; i < 4; i++) {
        size_t id = RVA_UNIT_TEMP_ABILITY_IDS + (size_t)u * UNIT_SIZE + (size_t)i * 2;
        int has = (short)G16(id);
        if (!has) return 0;
        if (has != ability) continue;
        G16(id) = -1;
        G32(RVA_UNIT_TEMP_ABILITY_VALUES + (size_t)u * UNIT_SIZE + (size_t)i * 4) = 0;
        return 1;
    }
    return 0;
}
static void addBenefitCopy(int u, unsigned long long bits) {
    if (u < 0 || u > G32(RVA_UNIT_COUNT)) return;
    if ((bits & 0x80000000ULL) && !GAME_FN(int (*)(int), RVA_UNIT_PRIMABLE)(u)) bits -= 0x80000000ULL;
    unsigned long long *have = (unsigned long long *)GAME_PTR(RVA_UNIT_BENEFIT_BITS + (size_t)u * UNIT_SIZE);
    if ((*have & bits) == bits) return;
    int (*maxHp)(int) = GAME_FN(int (*)(int), RVA_UNIT_MAX_HP);
    int before = maxHp(u);
    *have |= bits;
    int gain = maxHp(u) - before;
    short hp = (short)G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE);
    if (gain > 0 && hp > 0) G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE) = (unsigned short)(hp + gain);
}
static int squareHoldCopy(int player, int x, int y) {
    int count = 0;
    for (int u = 0; u <= G32(RVA_UNIT_COUNT); u++) {
        size_t at = (size_t)u * UNIT_SIZE;
        if (G32(RVA_UNIT_X + at) != x || G32(RVA_UNIT_X + 4 + at) != y || (short)G16(RVA_UNIT_HP + at) <= 0) continue;
        if ((short)G16(RVA_UNIT_PLAYER + at) != player) continue;
        *(unsigned long long *)GAME_PTR(RVA_UNIT_BENEFIT_BITS + at) |= 1ULL << UNIT_FLAG2_HOLDIT;
        count++;
    }
    return count;
}
static int copyCommanderPlayer(int c) {
    if (c > G32(RVA_LAST_COMMANDER)) return -1;
    int unit = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE);
    return unit < 0 ? -1 : (short)G16(RVA_UNIT_PLAYER + (size_t)unit * UNIT_SIZE);
}
static void selectedCommanderDoneCopy(void) {
    int c = G32(RVA_SELECTED_COMMANDER);
    if (c < 0) return;
    G8(RVA_COMMANDER_DONE + (size_t)c * COMMANDER_SIZE) = 1;
    int player = copyCommanderPlayer(c), group = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE + COMMANDER_ORDER_GROUP);
    if (group <= 0) return;
    for (int i = 0; i <= G32(RVA_LAST_COMMANDER); i++)
        if (G32(RVA_COMMANDERS + (size_t)i * COMMANDER_SIZE + COMMANDER_ORDER_GROUP) == group && copyCommanderPlayer(i) == player)
            G8(RVA_COMMANDER_DONE + (size_t)i * COMMANDER_SIZE) = 1;
}
static int toggleAutoRecruitCopy(int player, int x, int y, int monster) {
    GAME_FN(void (*)(const char *, ...), RVA_TRACE)("toggleautorec p%d %d,%d mnr %d\n", player, x, y, monster);
    if (player < 0 || x < 0 || y < 0 || monster <= 0) return -1;
    short *squares = (short *)playerSlotAddress(RVA_AUTO_RECRUIT_SQUARES, player, PLAYER_BLOCK_SIZE);
    short *monsters = (short *)playerSlotAddress(RVA_AUTO_RECRUIT_MONSTERS, player, PLAYER_BLOCK_SIZE);
    for (int i = 0; i < 64; i++)
        if (squares[2 * i] == x && squares[2 * i + 1] == y && monsters[i] == monster) {
            squares[2 * i] = 0, squares[2 * i + 1] = 0, monsters[i] = 0;
            return 0;
        }
    for (int i = 0; i < 64; i++)
        if (!squares[2 * i] && !squares[2 * i + 1]) {
            squares[2 * i] = (short)x, squares[2 * i + 1] = (short)y, monsters[i] = (short)monster;
            return 1;
        }
    return -1;
}
static int unmemorizeSpellCopy(int u, int spell) {
    if (u < 0 || spell < 0) return -1;
    GAME_FN(void (*)(const char *, ...), RVA_TRACE_C)("%s unmemorized %s\n", GAME_FN(const char *(*)(int), RVA_UNIT_SHOWN_NAME)(u),
                                                      spellSlot(spell)->name);
    size_t list = RVA_UNIT_MEMORIZED_SPELLS + (size_t)u * UNIT_SIZE;
    for (int i = 0; i < 30; i++) {
        int at = (short)G16(list + (size_t)i * 2);
        if (at == spell) {
            if (i < 29) memmove(GAME_PTR(list + (size_t)i * 2), GAME_PTR(list + (size_t)(i + 1) * 2), (size_t)(29 - i) * 2);
            G16(list + 29 * 2) = -1;
            return i;
        }
        if (at < 0) return -1;
    }
    return -1;
}
static void moveGemsCopy(int fromX, int fromY, int toX, int toY) {
    if (fromX < 0 || fromY < 0 || fromX >= G32(RVA_MAP_DEPTH) || fromY >= G32(RVA_MAP_H)) return;
    if (toX < 0 || toY < 0 || toX >= G32(RVA_MAP_DEPTH) || toY >= G32(RVA_MAP_H)) return;
    GAME_FN(void (*)(const char *, ...), RVA_TRACE)("movegems %d,%d -> %d,%d\n", fromX, fromY, toX, toY);
    unsigned char *from = GAME_PTR(RVA_SQUARE_RECORDS + ((size_t)fromX * MAP_COLUMN_SQUARES + (size_t)fromY) * SQUARE_SIZE + SQUARE_GEMS);
    unsigned char *to = GAME_PTR(RVA_SQUARE_RECORDS + ((size_t)toX * MAP_COLUMN_SQUARES + (size_t)toY) * SQUARE_SIZE + SQUARE_GEMS);
    unsigned char next = from[0];
    for (int i = 0; i < 4; i++) {
        to[i] = next;
        if (i < 3) next = from[i + 1];
        from[i] = 0;
    }
}
static void removeBadFollowersCopy(void) {
    GAME_FN(void (*)(const char *, ...), RVA_TRACE)("removeallbadfollowers\n");
    for (int c = 0; c <= G32(RVA_LAST_COMMANDER); c++) {
        if (G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE) >= 0) {
            GAME_FN(void (*)(int), RVA_CHECK_FOLLOWER_LINKS)(c);
            continue;
        }
        for (int u = 0; u <= G32(RVA_UNIT_COUNT); u++)
            if (G32(RVA_UNIT_LEADER + (size_t)u * UNIT_SIZE) == c) G32(RVA_UNIT_LEADER + (size_t)u * UNIT_SIZE) = -1;
    }
}
static int moveEnchantsCopy(int fromX, int fromY, int toX, int toY) {
    void (*trace)(const char *, ...) = GAME_FN(void (*)(const char *, ...), RVA_TRACE);
    int moved = 0;
    for (int i = 0; i <= G32(RVA_LAST_ENCHANT); i++) {
        size_t e = RVA_ENCHANTS + (size_t)i * ENCHANT_SIZE;
        int kind = G32(e + ENCHANT_KIND);
        trace("moveench: %d = type %d  at %d,%d\n", i, kind, G32(e + ENCHANT_X), G32(e + ENCHANT_Y));
        if (!kind || G32(e + ENCHANT_X) != fromX || G32(e + ENCHANT_Y) != fromY) continue;
        trace("moveench: type %d  %d,%d -> %d,%d\n", kind, fromX, fromY, toX, toY);
        moved++;
        G32(e + ENCHANT_X) = toX, G32(e + ENCHANT_Y) = toY;
    }
    if (moved <= 0) return moved;
    for (int x = 0; x <= G32(RVA_MAP_DEPTH); x++)
        for (int y = 0; y <= G32(RVA_MAP_H); y++) G32(RVA_SQUARE_FLAGS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE) &= ~4;
    for (int i = 0; i <= G32(RVA_LAST_ENCHANT); i++) {
        size_t e = RVA_ENCHANTS + (size_t)i * ENCHANT_SIZE;
        int x = G32(e + ENCHANT_X), y = G32(e + ENCHANT_Y);
        if (G32(e + ENCHANT_KIND) && x >= 0 && y >= 0) G32(RVA_SQUARE_FLAGS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE) |= 4;
    }
    return moved;
}
static void subtractUnitHpCopy(int u, int damage) {
    if (u < 0) return;
    size_t at = (size_t)u * UNIT_SIZE;
    int x = G32(RVA_UNIT_X + at), y = G32(RVA_UNIT_X + 4 + at);
    G16(RVA_UNIT_HP + at) = (short)((unsigned short)G16(RVA_UNIT_HP + at) - (unsigned short)damage);
    if ((short)G16(RVA_UNIT_HP + at) > 0) {
        rebuildSquareArmyHook(x, y);
        return;
    }
    if (damage != 9999) GAME_FN(void (*)(int), RVA_UNIT_LEAVE_CORPSE)(u);
    for (int c = 0; c <= G32(RVA_LAST_COMMANDER); c++) {
        if (G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE) != u) continue;
        for (int p = 0; p < playerSlotCount(); p++)
            if (PSLOT16(RVA_SLOT_CONTROLLER, p, 2) >= 0 && PSLOT32(RVA_PLAYER_LEADER, p, PLAYER_BLOCK_SIZE) == c) {
                PSLOT32(RVA_PLAYER_LEADER, p, PLAYER_BLOCK_SIZE) = -1;
                break;
            }
        G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE) = -1;
        break;
    }
    for (int f = 0; f <= G32(RVA_UNIT_COUNT); f++) {
        size_t link = RVA_UNIT_LEADER + (size_t)f * UNIT_SIZE;
        int leader = G32(link);
        if (leader < 0 || leader >= 9990) continue;
        int unit = leader > G32(RVA_LAST_COMMANDER) ? -1 : G32(RVA_COMMANDERS + (size_t)leader * COMMANDER_SIZE);
        if (unit < 0 || (short)G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE) <= 0 ||
            G16(RVA_UNIT_PLAYER + (size_t)unit * UNIT_SIZE) != G16(RVA_UNIT_PLAYER + (size_t)f * UNIT_SIZE))
            G32(link) = -1;
    }
    rebuildSquareArmyHook(x, y);
}
#define COPY_PORTAL_ACTIVE 0xcc
#define COPY_PORTAL_INACTIVE 0x141
static size_t copySquareAt(int x, int y) { return RVA_SQUARE_RECORDS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE; }
static int fixPortalsCopy(void) {
    void (*trace)(const char *, ...) = GAME_FN(void (*)(const char *, ...), RVA_TRACE);
    const char *(*text)(int, int) = GAME_FN(const char *(*)(int, int), RVA_SQUARE_TEXT);
    int count = 0;
    for (int y = 0; y < G32(RVA_MAP_H); y++)
        for (int x = 0; x < G32(RVA_MAP_DEPTH); x++) {
            int portal = (short)G16(copySquareAt(x, y) + SQUARE_PORTAL);
            if (portal <= 0) continue;
            int onMap = x < G32(RVA_MAP_W) && y < G32(RVA_MAP_H);
            int terrain = onMap ? (short)G16(copySquareAt(x, y) + SQUARE_TERRAIN_TYPE) : -1;
            int same = 0;
            for (int x2 = 0; x2 < G32(RVA_MAP_DEPTH); x2++)
                for (int y2 = 0; y2 < G32(RVA_MAP_H); y2++) {
                    int other = (short)G16(copySquareAt(x2, y2) + SQUARE_PORTAL);
                    if (other > 0 && other == portal) same++;
                }
            int from = same > 1 ? COPY_PORTAL_INACTIVE : COPY_PORTAL_ACTIVE, to = same > 1 ? COPY_PORTAL_ACTIVE : COPY_PORTAL_INACTIVE;
            if (terrain == from) {
                trace(same > 1 ? "fixportals: Portal %d at %s -> active\n" : "fixportals: Portal %d at %s -> inactive\n", portal, text(x, y));
                if (onMap && G32(RVA_LAST_TERRAIN) >= to) G16(copySquareAt(x, y) + SQUARE_TERRAIN_TYPE) = (short)to;
            }
            count++;
        }
    return count;
}
static void planeShiftUnitCopy(int u, int plane, int noSight) {
    if (u < 0 || plane < 0) return;
    int x = G32(RVA_UNIT_X + (size_t)u * UNIT_SIZE), y = G32(RVA_UNIT_X + 4 + (size_t)u * UNIT_SIZE);
    GAME_FN(void (*)(int, int *, int *), RVA_PLANE_SAME_SPOT)(plane, &x, &y);
    int c = -1;
    for (int i = 0; i <= G32(RVA_LAST_COMMANDER); i++)
        if (G32(RVA_COMMANDERS + (size_t)i * COMMANDER_SIZE) == u) { c = i; break; }
    const char *name = c >= 0 ? commanderTheName(c) : GAME_FN(const char *(*)(int), RVA_UNIT_SHOWN_NAME)(u);
    GAME_FN(void (*)(const char *, ...), RVA_TRACE)("planeshiftunit %s to %d (%d,%d)\n", name, plane, x, y);
    GAME_FN(void (*)(int, int, int, int, int, int, int), RVA_TELEPORT_ARMY)(u, 0, -1, -1, x, y, noSight);
}
static void planeRiseToElysiumCopy(int plane, int percent, int each, const char *name) {
    int (*random)(int) = GAME_FN(int (*)(int), RVA_RANDOM);
    int (*planeAt)(int, int) = GAME_FN(int (*)(int, int), RVA_PLANE_AT);
    for (int y = 0; y < planeField(plane, PLANE_H); y++)
        for (int x = planeField(plane, PLANE_X); x < planeField(plane, PLANE_X) + planeField(plane, PLANE_W); x++) {
            if (random(100) >= percent || x < 0 || x >= G32(RVA_MAP_DEPTH) || y >= G32(RVA_MAP_H) ||
                !G16(RVA_SQUARE_ARMY + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * 2))
                continue;
            int moved = 0, owner = 24, lastX = -1, lastY = -1;
            for (int u = 0; u <= G32(RVA_UNIT_COUNT) && u < UNIT_TABLE_MAX; u++) {
                size_t at = (size_t)u * UNIT_SIZE;
                int ux = G32(RVA_UNIT_X + at), uy = G32(RVA_UNIT_X + 4 + at);
                if (planeAt(ux, uy) != plane || (short)G16(RVA_UNIT_HP + at) <= 0 || ux != x || uy != y) continue;
                const unsigned char *monster = monsterRecord((short)G16(RVA_UNIT_MONSTER + at));
                if (!monster || strcmp((const char *)monster, name) || random(100) >= each) continue;
                planeShiftUnitCopy(u, 0, 1);
                owner = (short)G16(RVA_UNIT_PLAYER + at), lastX = G32(RVA_UNIT_X + at), lastY = G32(RVA_UNIT_X + 4 + at);
                moved++;
            }
            int again = moved > 0 ? (moved * 100 + 50) / 100 : 0;
            if (again > 0) GAME_FN(int (*)(int, int, const char *, int, int, int), RVA_POPULATE)(lastX, lastY, name, again, owner, -1);
        }
}
static int destroyGatewayCopy(int player, int x, int y, int flag);
static int destroyGatewayLinksCopy(int player, int number) {
    int sum = 0;
    if (!number) return 0;
    for (int x = 0; x < G32(RVA_MAP_DEPTH); x++)
        for (int y = 0; y < G32(RVA_MAP_H); y++)
            if ((short)G16(copySquareAt(x, y) + SQUARE_PORTAL) == number) sum += destroyGatewayCopy(player, x, y, 0);
    return sum;
}
static int destroyGatewayCopy(int player, int x, int y, int flag) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_W) || y >= G32(RVA_MAP_H)) return 0;
    int terrain = (short)G16(copySquareAt(x, y) + SQUARE_TERRAIN_TYPE), changed = 0;
    if (terrain < 0) return 0;
    if (terrain >= 201 && terrain <= 204) {
        changed = GAME_FN(int (*)(int, int, int, int, int, int), RVA_ALTER_TERRAIN)(player, x, y, terrain, -1, 200);
        if (!changed) return 0;
    }
    int number = (short)G16(copySquareAt(x, y) + SQUARE_PORTAL);
    if (number) {
        G16(copySquareAt(x, y) + SQUARE_PORTAL) = 0;
        if (flag) destroyGatewayLinksCopy(player, number), destroyGatewayLinksCopy(player, -number);
        changed = 1;
    }
    if (changed) GAME_FN(void (*)(int, int, int), RVA_NET_SEND_SQUARE)(player, x, y);
    if (terrain == 206)
        GAME_FN(void (*)(int, int, int, int, int, int, const char *, const char *, ...), RVA_ADD_MESSAGE)(
            -1, -2, x, y, 4, 0, "Wonderful News", "The Gateway to Inferno has been destroyed by %s.",
            (const char *)playerSlotAddress(RVA_SLOT_NAME, player, PLAYER_BLOCK_SIZE));
    return changed;
}

// ---- The month's spawns ---------------------------------------------------------------------------
static int copySquareOwner(int x, int y) { return (signed char)G8(copySquareAt(x, y) + SQUARE_OWNER); }
static int copyOnMapW(int x, int y) { return x >= 0 && y >= 0 && x < G32(RVA_MAP_W) && y < G32(RVA_MAP_H); }
static int copyPopulate(int x, int y, const char *name, int count, int player) {
    return GAME_FN(int (*)(int, int, const char *, int, int, int), RVA_POPULATE)(x, y, name, count, player, -1);
}
static int copyEnchantCount(int player, int kind, int x, int y) {
    return GAME_FN(int (*)(int, int, int, int), RVA_ENCHANT_COUNT)(player, kind, x, y);
}
static int copyRandom(int n) { return GAME_FN(int (*)(int), RVA_RANDOM)(n); }
static void aiRedistributeItemsCopy(int player, int x, int y);
static int copyOpenDie(int n) { return GAME_FN(int (*)(int), RVA_RANDOM_OPEN_DIE)(n); }

static void forestShepherdSpawnCopy(int x, int y) {
    if (!copyOnMapW(x, y)) return;
    int owner = copySquareOwner(x, y);
    if (owner < 0 || !copyEnchantCount(owner, 2, x, y) || copyRandom(100) >= 33) return;
    copyPopulate(x, y, "Forest Shepherd", 1, owner);
}
static int terrainSpawnsWild(int terrain);
static void wildSpawnCopy(int x, int y) {
    if (!copyOnMapW(x, y)) return;
    int owner = copySquareOwner(x, y);
    if (owner < 0 || !copyEnchantCount(owner, 3, x, y) || copyRandom(100) < 50) return;
    int w = G32(RVA_MAP_W), h = G32(RVA_MAP_H);
    for (int tries = 0; tries < 10; tries++) {
        int tx = copyRandom(7) + x - 3, ty = copyRandom(7) + y - 3;
        if (tx < 0 || ty < 0 || tx >= w || ty >= h) continue;
        if (!terrainSpawnsWild((short)G16(copySquareAt(tx, ty) + SQUARE_TERRAIN_TYPE))) continue;
        if (copySquareOwner(x, y) != owner) continue;
        int pick = copyRandom(8), n;
        const char *name;
        if (pick < 2) n = copyOpenDie(4), name = "Wolf";
        else if (pick < 4) n = copyOpenDie(4), name = "Deer";
        else if (pick == 4) n = copyOpenDie(4), name = "Snake";
        else if (pick == 5) n = copyOpenDie(2), name = "Moose";
        else if (pick == 6) n = copyOpenDie(2), name = "Bear";
        else n = copyOpenDie(3), name = "Boar";
        copyPopulate(tx, ty, name, n, owner);
        return;
    }
}
static void revelsSpawnCopy(int x, int y) {
    if (!copyOnMapW(x, y)) return;
    int owner = copySquareOwner(x, y);
    if (owner < 0 || !copyEnchantCount(owner, 1, x, y)) return;
    if (copyRandom(100) < 40) {
        int pick = copyRandom(2);
        if (pick == 0) copyPopulate(x, y, "Maenad", copyOpenDie(6), owner);
        else if (pick == 1) copyPopulate(x, y, "Satyr Reveler", copyOpenDie(3), owner);
    }
    if (copyRandom(100) < 5 && !GAME_FN(int (*)(int, int, const char *), RVA_COUNT_MONSTERS_AT)(x, y, "Hamadryad"))
        copyPopulate(x, y, "Hamadryad", 1, owner);
}
static void stormSpawnCopy(int x, int y) {
    if (!copyOnMapW(x, y)) return;
    int owner = copySquareOwner(x, y);
    if (owner < 0 || !copyEnchantCount(owner, 14, x, y)) return;
    for (int i = 0; i < 2; i++) {
        int tx = copyRandom(3) - 1 + x, ty = y - 1 + copyRandom(3);
        if (!tx && !ty) continue;
        if (!GAME_FN(int (*)(int, int, int), RVA_TERRAIN_MATCH)(-21, tx, ty)) continue;
        if (!copyRandom(5)) {
            if (tx >= 0) copyPopulate(tx, ty, "Storm Elemental", 1, owner);
        } else {
            int n = copyOpenDie(2);
            if (n && tx >= 0) copyPopulate(tx, ty, "Storm Cloud", n, owner);
        }
    }
    int last = G32(RVA_UNIT_COUNT);
    static const char *const gone[2] = {"Storm Elemental", "Storm Cloud"};
    for (int k = 0; k < 2; k++) {
        int removed = 0;
        for (int u = 0; u <= last && u < UNIT_TABLE_MAX; u++) {
            size_t at = (size_t)u * UNIT_SIZE;
            if ((short)G16(RVA_UNIT_HP + at) <= 0 || G32(RVA_UNIT_X + at) != x || G32(RVA_UNIT_X + 4 + at) != y) continue;
            const unsigned char *monster = monsterRecord((short)G16(RVA_UNIT_MONSTER + at));
            if (!monster || strcmp((const char *)monster, gone[k])) continue;
            GAME_FN(void (*)(int), RVA_REMOVE_UNIT)(u);
            if (++removed > 999) break;
        }
    }
}
static void trollForestSpawnCopy(int x, int y) {
    if (!copyOnMapW(x, y)) return;
    size_t sq = copySquareAt(x, y);
    int terrain = (unsigned short)G16(sq + SQUARE_TERRAIN_TYPE), owner = copySquareOwner(x, y);
    if ((unsigned short)(terrain - 130) > 1) return;
    int forest = owner >= 0 && PSLOT16(RVA_SLOT_CLASS, owner, 2) == 14;
    if (!forest && owner < 24) forest = GAME_FN(int (*)(int, int, int), RVA_PLAYER_ABILITY)(owner, 0x20a, 1) > 0;
    if (!forest) {
        if (copyRandom(100) >= 10 || !copyOnMapW(x, y)) return;
        if (terrain == 130 && G32(RVA_LAST_TERRAIN) >= 2) G16(sq + SQUARE_TERRAIN_TYPE) = 2;
        if (terrain == 131 && G32(RVA_LAST_TERRAIN) >= 4) G16(sq + SQUARE_TERRAIN_TYPE) = 4;
        return;
    }
    short *corpses = (short *)GAME_PTR(sq + SQUARE_CORPSES);
    int a = copyRandom(100), b = copyRandom(100);
    if (a < 50) {
        if (b < corpses[1] * 10) {
            if (owner >= 0) copyPopulate(x, y, "Carrion Manikin", 1, owner);
            corpses[1]--;
        }
        int have = corpses[3];
        if (copyRandom(100) < have * 10) {
            if (owner >= 0) copyPopulate(x, y, "Carrion Stalker", 1, owner);
            corpses[3]--;
        }
        if (copyRandom(100) < 7) {
            if (owner >= 0) copyPopulate(x, y, "Carrion Stalker", 1, owner);
            corpses[3]--;
        }
        have = corpses[4];
        if (copyRandom(100) < have * 10) {
            if (owner >= 0) copyPopulate(x, y, "Carrion Beast", 1, owner);
            corpses[4]--;
        }
        return;
    }
    if (b >= 50) return;
    if (!GAME_FN(int (*)(int, int, const char *), RVA_COUNT_MONSTERS_AT)(x, y, "Shadow Tree") && copyRandom(100) < 15 && owner >= 0)
        copyPopulate(x, y, "Shadow Tree", 1, owner);
    if (copyRandom(100) < 2 && owner >= 0) copyPopulate(x, y, "Shadow Tree", 1, owner);
    if (copyRandom(100) < 5) {
        int n = copyRandom(3) + 1;
        if (owner >= 0) copyPopulate(x, y, "Venus Trap", n, owner);
    }
    int onWorld = x < G32(RVA_MAP_DEPTH) && y < G32(RVA_MAP_H), sum = 0;
    if (onWorld)
        for (int k = 0; k < SQUARE_CORPSE_KINDS; k++) sum += corpses[k];
    if (copyRandom(100) < (sum - 3) * 3) {
        if (owner >= 0) copyPopulate(x, y, "Death Fungus", 1, owner);
        if (x < G32(RVA_MAP_DEPTH) && y < G32(RVA_MAP_H))
            for (int k = 0; k < SQUARE_CORPSE_KINDS; k++)
                if (corpses[k] > 0) {
                    corpses[k]--;
                    break;
                }
    }
    if (copyRandom(100) < 7) {
        int snow = copyOnMapW(x, y) && (short)G16(sq + SQUARE_TEMPERATURE) <= 0, n = copyRandom(2) + 1;
        if (owner >= 0) copyPopulate(x, y, snow ? "Snow Fungus" : "Devil Fungus", n, owner);
    }
    int blood = GAME_FN(int (*)(int, int, const char *), RVA_COUNT_MONSTERS_AT)(x, y, "Blood Fungus");
    if (blood > 0) {
        if (copyRandom(100) < 5 && owner >= 0) copyPopulate(x, y, "Blood Fungus", 1, owner);
    } else {
        int chance = (10 - blood) * 2;
        if (chance < 5) chance = 5;
        if (copyRandom(100) < chance && owner >= 0) copyPopulate(x, y, "Blood Fungus", 1, owner);
    }
}
static void groveReversionCopy(int x, int y) {
    if (x < 0 || y < 0) return;
    size_t sq = copySquareAt(x, y);
    if ((G32(RVA_SQUARE_FLAGS + (size_t)x * MAP_COLUMN_SQUARES * SQUARE_SIZE + (size_t)y * SQUARE_SIZE) & 4) && G32(RVA_LAST_ENCHANT) >= 0) {
        int count = 0;
        for (int i = 0; i <= G32(RVA_LAST_ENCHANT); i++) {
            size_t e = RVA_ENCHANTS + (size_t)i * ENCHANT_SIZE;
            if (G32(e + ENCHANT_KIND) && G32(e + ENCHANT_X) == x && G32(e + ENCHANT_Y) == y) count++;
        }
        if (count > 0) {
            forestShepherdSpawnCopy(x, y);
            revelsSpawnCopy(x, y);
            wildSpawnCopy(x, y);
            stormSpawnCopy(x, y);
        }
    }
    trollForestSpawnCopy(x, y);
    int onMap = x < G32(RVA_MAP_W) && y < G32(RVA_MAP_H);
    int terrain = onMap ? (short)G16(sq + SQUARE_TERRAIN_TYPE) : -1, owner = -1;
    if (onMap) owner = copySquareOwner(x, y);
    if (!(onMap && owner >= 0 && PSLOT16(RVA_SLOT_CLASS, owner, 2) == 24) && (unsigned)(terrain - 236) <= 2 &&
        copyRandom(100) < 10 && GAME_FN(int (*)(int, int), RVA_PLANE_AT)(x, y) != 7 &&
        GAME_FN(int (*)(int, int, int), RVA_PLAYER_ABILITY)(owner, 0x223, 1) <= 0 && copyOnMapW(x, y) && G32(RVA_LAST_TERRAIN) >= 4)
        G16(sq + SQUARE_TERRAIN_TYPE) = 4;
    if (terrain == 314 && copyRandom(100) < 20 && copyOnMapW(x, y) && G32(RVA_LAST_TERRAIN) >= 4) G16(sq + SQUARE_TERRAIN_TYPE) = 4;
}

static void putRareSquareCopy(int x, int y) {
    int last = G32(RVA_LAST_TERRAIN), terrain;
    for (int tries = 1;; tries++) {
        if (tries > 100000) {
            GAME_FN(void (*)(const char *, ...), RVA_TRACE)("*** putraresquare failed\n");
            last = G32(RVA_LAST_TERRAIN), terrain = 0;
            break;
        }
        terrain = copyRandom(last);
        unsigned long long flags = copyTerrainFlags(terrain);
        if (!(flags & (1ULL << 22))) continue;
        if ((flags & (1ULL << 28)) &&
            GAME_FN(int (*)(int, int, int, int, int, int), RVA_MAPGEN_NEAREST_TERRAIN)(x, y, terrain, -1, -1, -1) < 1000)
            continue;
        break;
    }
    if (copyOnMapW(x, y) && terrain >= 0 && terrain <= last) G16(copySquareAt(x, y) + SQUARE_TERRAIN_TYPE) = (short)terrain;
    GAME_FN(void (*)(int, int), RVA_SQUARE_NEW_TERRAIN_SETUP)(x, y);
}

// ---- Rituals ---------------------------------------------------------------------------------------

static void copyRevealDisc(int player, int x, int y, float radius, int value, int flag) {
    void (*reveal)(int, int, int, float, int, int) = GAME_FN(void (*)(int, int, int, float, int, int), RVA_REVEAL_DISC);
#ifdef __APPLE__
    if (macRedirectedHook(RVA_REVEAL_DISC)) reveal = (void (*)(int, int, int, float, int, int))macRedirectedHook(RVA_REVEAL_DISC);
#endif
    reveal(player, x, y, radius, value, flag);
}
static int revealTerrainOnPlaneCopy(int player, int plane, int code) {
    int count = 0, lastX = -1, lastY = -1;
    for (int x = 0; x < G32(RVA_MAP_DEPTH); x++) {
        int at = -1;
        for (int i = 0; i <= G32(RVA_LAST_PLANE); i++) {
            int px = planeField(i, PLANE_X), py = planeField(i, PLANE_Y);
            if (x >= px && x < px + planeField(i, PLANE_W) && py <= 1 && py + planeField(i, PLANE_H) > 1) {
                at = i;
                break;
            }
        }
        if (at != plane) continue;
        for (int y = 0; y < G32(RVA_MAP_H); y++) {
            if (!GAME_FN(int (*)(int, int, int), RVA_TERRAIN_MATCH)(code, x, y)) continue;
            copyRevealDisc(player, x, y, 0.5f, 1, -1);
            count++, lastX = x, lastY = y;
        }
    }
    if (count > 0) copyRevealDisc(player, lastX, lastY, 0.5f, 1, 1);
    return count;
}

// ---- The AI's turn ----------------------------------------------------------------------------------

static void aiMakeDefendersCopy(int player) {
    if (player < 0) return;
    void (*trace)(const char *, ...) = GAME_FN(void (*)(const char *, ...), RVA_TRACE);
    trace("ailog: makedefenders\n");
    for (int y = 0; y < G32(RVA_MAP_H); y++) {
#ifdef __APPLE__
        int mode = !G8(RVA_AI_PROGRESS_MODE);
#else
        int mode = G32(RVA_AI_PROGRESS_MODE) != 0;
#endif
        GAME_FN(unsigned long long (*)(int, const char *, ...), RVA_PROGRESS_TEXT)(mode ? -1 : -2, "");
        for (int x = 0; x < G32(RVA_MAP_DEPTH); x++) {
            size_t sq = copySquareAt(x, y), cell = (size_t)x * MAP_COLUMN_SQUARES + (size_t)y;
            if (!(G32(RVA_SQUARE_FLAGS + cell * SQUARE_SIZE) & 4) || (signed char)G8(sq + SQUARE_SEEN + (size_t)player) <= 0 || G16(RVA_SQUARE_ARMY + cell * 2) <= 0) continue;
            int armyOwner = (short)G16(sq + SQUARE_ARMY_OWNER);
            if (armyOwner >= 0 && armyOwner != player) continue;
            int guards = 0;
            for (int u = 0; u <= G32(RVA_UNIT_COUNT); u++) {
                size_t at = (size_t)u * UNIT_SIZE;
                if ((short)G16(RVA_UNIT_PLAYER + at) == player && (short)G16(RVA_UNIT_HP + at) > 0 && G32(RVA_UNIT_X + at) == x &&
                    G32(RVA_UNIT_X + 4 + at) == y && (*(unsigned long long *)GAME_PTR(RVA_UNITS + at) & UNIT_AI_GUARD))
                    guards++;
            }
            if (guards > 0) continue;
            if (copyEnchantCount(player, 11, x, y) + copyEnchantCount(player, 10, x, y) + copyEnchantCount(player, 9, x, y) +
                    copyEnchantCount(player, 6, x, y) <= 0)
                continue;
            trace("ailog:   creating aiguard at %s\n", GAME_FN(const char *(*)(int, int), RVA_SQUARE_TEXT)(x, y));
            GAME_FN(void (*)(int, int, int, int), RVA_AI_MAKE_GUARDS_AT)(player, x, y, 1);
        }
    }
}

static int copyAbilityTotal(int u, int ability, int add) {
    return GAME_FN(int (*)(int, int, int, char *, int, int *), RVA_UNIT_ABILITY_TOTAL)(u, ability, add, NULL, 0, NULL);
}
#define COPY_ABILITY_TRANSPORT 0x1cf
#define COPY_UNIT_FLAG_SAILOR 0x200000000000ULL
static unsigned long long copyUnitFlags(int u, const char *fatal) {
    if ((short)G16(RVA_UNIT_MONSTER + (size_t)u * UNIT_SIZE) < 0) GAME_FN(void (*)(const char *), RVA_EXIT_GAME)(fatal);
    return GAME_FN(unsigned long long (*)(int), RVA_UNIT_FLAGS)(u);
}
static int copyUnitAiSkip(int u) {
    if (u < 0) return 0;
    unsigned long long f = copyUnitFlags(u, "iss");
    return (f & (1ULL << 25)) || (f & (1ULL << 29)) ? 1 : (int)(f >> 62 & 1);
}
static int copyUnitNoCommander(int u) { return u < 0 ? 0 : (int)(copyUnitFlags(u, "isst") >> 35 & 1); }
static int copyShipUnit(int c, int *player) {
    if (c < 0 || c > G32(RVA_LAST_COMMANDER)) return -1;
    int unit = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE);
    if (unit >= 0) *player = unit > G32(RVA_UNIT_COUNT) ? -1 : (short)G16(RVA_UNIT_PLAYER + (size_t)unit * UNIT_SIZE);
    return unit;
}
static int aiShipDropCrewCopy(int c) {
    int player = -1, unit = copyShipUnit(c, &player);
    if (unit < 0 || G32(RVA_UNIT_X + (size_t)unit * UNIT_SIZE) < 0 || G32(RVA_UNIT_X + 4 + (size_t)unit * UNIT_SIZE) < 0) return 0;
    for (int pass = 0; pass < 2; pass++)
        for (int v = 0; v <= G32(RVA_UNIT_COUNT); v++) {
            size_t at = (size_t)v * UNIT_SIZE;
            if ((short)G16(RVA_UNIT_PLAYER + at) != player || G32(RVA_UNIT_LEADER + at) != c) continue;
            if (!pass && !(*(unsigned long long *)GAME_PTR(RVA_UNITS + at) & COPY_UNIT_FLAG_SAILOR)) continue;
            if ((short)G16(RVA_UNIT_MONSTER + at) >= 0 && copyAbilityTotal(v, COPY_ABILITY_TRANSPORT, 1) >= 1) continue;
            G32(RVA_UNIT_LEADER + at) = -1;
            return 1;
        }
    return 0;
}
static int copyShipFree(int v, int pass, int player, int unit, int x, int y, int guards) {
    size_t at = (size_t)v * UNIT_SIZE;
    unsigned long long flags = *(unsigned long long *)GAME_PTR(RVA_UNITS + at);
    if ((short)G16(RVA_UNIT_PLAYER + at) != player || G32(RVA_UNIT_LEADER + at) >= 0 || (!pass && !(flags & COPY_UNIT_FLAG_SAILOR))) return 0;
    if (!guards && (flags & UNIT_AI_GUARD)) return 0;
    if ((short)G16(RVA_UNIT_HP + at) <= 0 || G32(RVA_UNIT_X + at) != x || G32(RVA_UNIT_X + 4 + at) != y) return 0;
    return !copyUnitAiSkip(v) && !copyUnitNoCommander(v) && v != unit;
}
static int aiShipTakeBoatCopy(int c) {
    int player = -1, unit = copyShipUnit(c, &player);
    if (unit < 0) return 0;
    int x = G32(RVA_UNIT_X + (size_t)unit * UNIT_SIZE), y = G32(RVA_UNIT_X + 4 + (size_t)unit * UNIT_SIZE);
    if (x < 0 || y < 0) return 0;
    for (int pass = 0; pass < 2; pass++)
        for (int v = 0; v <= G32(RVA_UNIT_COUNT); v++) {
            if (!copyShipFree(v, pass, player, unit, x, y, 1) || copyAbilityTotal(v, COPY_ABILITY_TRANSPORT, 1) <= 0) continue;
            G32(RVA_UNIT_LEADER + (size_t)v * UNIT_SIZE) = c;
            return 1;
        }
    return 0;
}
static int aiShipTakeCrewCopy(int c) {
    int player = -1, unit = copyShipUnit(c, &player);
    if (unit < 0) return 0;
    int x = G32(RVA_UNIT_X + (size_t)unit * UNIT_SIZE), y = G32(RVA_UNIT_X + 4 + (size_t)unit * UNIT_SIZE);
    if (x < 0 || y < 0) return 0;
    for (int pass = 0; pass < 2; pass++)
        for (int v = 0; v <= G32(RVA_UNIT_COUNT); v++) {
            if (!copyShipFree(v, pass, player, unit, x, y, 0)) continue;
            if (GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(v) * GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(v) >= 2) continue;
            if (copyAbilityTotal(v, COPY_ABILITY_TRANSPORT, 1) >= 1) continue;
            G32(RVA_UNIT_LEADER + (size_t)v * UNIT_SIZE) = c;
            return 1;
        }
    return 0;
}

static int copyShipGroup(int c, int placesOnly) {
    int player = -1, unit = copyShipUnit(c, &player);
    if (unit < 0 || G32(RVA_UNIT_X + (size_t)unit * UNIT_SIZE) < 0 || G32(RVA_UNIT_X + 4 + (size_t)unit * UNIT_SIZE) < 0) return 0;
    int places = 0, men = 0;
    for (int v = 0; v <= G32(RVA_UNIT_COUNT); v++) {
        size_t at = (size_t)v * UNIT_SIZE;
        if ((short)G16(RVA_UNIT_PLAYER + at) != player || (G32(RVA_UNIT_LEADER + at) != c && v != unit)) continue;
        int t = (short)G16(RVA_UNIT_MONSTER + at) < 0 ? 0 : copyAbilityTotal(v, COPY_ABILITY_TRANSPORT, 1);
        if (placesOnly && (short)G16(RVA_UNIT_MONSTER + at) < 0) continue;
        if (t > 0) places += t;
        else men++;
    }
    return placesOnly ? places : places - men;
}
static int aiShipsCopy(int c) {
    int player = -1, unit = copyShipUnit(c, &player);
    if (unit < 0) return 0;
    size_t uat = (size_t)unit * UNIT_SIZE;
    int x = G32(RVA_UNIT_X + uat), y = G32(RVA_UNIT_X + 4 + uat);
    if (x < 0 || y < 0) return 0;
    int terrain = copyOnMapW(x, y) ? (short)G16(copySquareAt(x, y) + SQUARE_TERRAIN_TYPE) : -1;
    if (terrain < 0 || !(copyTerrainFlags(terrain) & TERRAIN_FLAG_PORT)) return 0;
    void (*trace)(const char *, ...) = GAME_FN(void (*)(const char *, ...), RVA_TRACE);
    trace("aiship: %s is in port, maybecaptain\n", commanderTheName(c));
    unsigned long long *flags = (unsigned long long *)GAME_PTR(RVA_UNITS + uat);
    int boats = 1000, free = 1000;
    if (!(*flags & COPY_UNIT_FLAG_SAILOR)) {
        int men = 0;
        boats = 0, free = 0;
        for (int v = 0; v <= G32(RVA_UNIT_COUNT); v++) {
            size_t at = (size_t)v * UNIT_SIZE;
            if ((short)G16(RVA_UNIT_PLAYER + at) != player) continue;
            if ((short)G16(RVA_UNIT_HP + at) <= 0 || G32(RVA_UNIT_X + at) != x || G32(RVA_UNIT_X + 4 + at) != y) continue;
            int leader = G32(RVA_UNIT_LEADER + at);
            if (leader != -1 && leader != c) continue;
            int t = (short)G16(RVA_UNIT_MONSTER + at) < 0 ? 0 : copyAbilityTotal(v, COPY_ABILITY_TRANSPORT, 1);
            if (leader == c || v == unit) {
                men++;
                if (!t && GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(v) * GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(v) > 1) return 0;
            }
            if (t > 0) boats += t;
            else free++;
        }
        if (boats <= 0) return 0;
        if (men > boats) {
            trace("aiship: %s  men %d, boats %d, no good\n", commanderTheName(c), men, boats);
            return 0;
        }
        if (free < boats - 2) return 0;
    }
    trace("aiship: %s is a captain, boats %d, sailors %d\n", commanderTheName(c), boats, free);
    *flags |= COPY_UNIT_FLAG_SAILOR;
    int places = copyShipGroup(c, 0);
    for (int i = 0; i < 100; i++) {
        if (places <= 2) {
            if (aiShipTakeBoatCopy(c) <= 0) break;
            places = copyShipGroup(c, 0);
            trace("aiship: +boat %d\n", places);
        } else {
            if (aiShipTakeCrewCopy(c) <= 0) break;
            places--;
            trace("aiship: +sailor %d\n", places);
        }
    }
    places = copyShipGroup(c, 0);
    if (places < 0 && copyShipGroup(c, 1) > 0) {
        trace("aiship: too many sailors %d\n", places);
        for (int i = 0; i < 100; i++) {
            if (places >= 0) continue;
            if (aiShipDropCrewCopy(c) <= 0) break;
            places++;
            trace("aiship: -sailor %d\n", places);
        }
    }
    return 1;
}

// ---- The month -------------------------------------------------------------------------------------

static int copyCountSquaresNamed(int player, const char *name) {
    int count = 0;
    for (int x = 0; x < G32(RVA_MAP_DEPTH); x++)
        for (int y = 0; y < G32(RVA_MAP_H); y++) {
            int owner = copyOnMapW(x, y) ? copySquareOwner(x, y) : -1;
            if (owner != player) continue;
            int terrain = (short)G16(copySquareAt(x, y) + SQUARE_TERRAIN_TYPE);
            if (copyNameMatches((const char *)GAME_PTR(RVA_TERRAIN_TABLE + (ptrdiff_t)terrain * TERRAIN_SIZE), name)) count++;
        }
    return count;
}
static int copyUnitCommander(int unit) {
    if (unit < 0) return -1;
    for (int c = 0; c <= G32(RVA_LAST_COMMANDER); c++)
        if (G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE) == unit) return c;
    return -1;
}
static int copySquareTerrainOrNone(int x, int y) { return copyOnMapW(x, y) ? (short)G16(copySquareAt(x, y) + SQUARE_TERRAIN_TYPE) : -1; }
static int copyFriendlyLocation(int player, int t1, int t2, int t3, int t4, int *x, int *y) {
    return GAME_FN(int (*)(int, int, int, int, int, int, int, int, int *, int *), RVA_GET_FRIENDLY_LOCATION)(player, 0, 0, 0, t1, t2, t3, t4, x, y);
}
static void heroArrivalCopy(int player) {
    if (G32(RVA_TURN) < 2) return;
    int kind = copyRandom(500) < 9 ? 1 : copyRandom(500) < 3 ? 2 : copyRandom(500) < 1 ? 3 : 0;
    int cls = PSLOT16(RVA_SLOT_CLASS, player, 2);
    void (*exitGame)(const char *) = GAME_FN(void (*)(const char *), RVA_EXIT_GAME);
    if (cls < 0) exitGame("gch1");
    void (*message)(int, int, int, int, int, int, const char *, const char *, ...) =
        GAME_FN(void (*)(int, int, int, int, int, int, const char *, const char *, ...), RVA_ADD_MESSAGE);
    int (*populate)(int, int, const char *, int, int, int) = GAME_FN(int (*)(int, int, const char *, int, int, int), RVA_POPULATE);
    if (kind) {
        const unsigned char *heroes = tableAddress(TABLE_CLASS_HEROES) + (size_t)cls * CLASS_HEROES_SIZE;
#define COPY_HERO_OK(h) ((!(*(const int *)((h) + 0x74) & 1) || kind == 1) && (!(*(const int *)((h) + 0x74) & 2) || kind == 3))
        int n = 0;
        for (int i = 0; i < 4 && heroes[(size_t)i * CLASS_HERO_SIZE + CLASS_HERO_NAME]; i++)
            if (COPY_HERO_OK(heroes + (size_t)i * CLASS_HERO_SIZE)) n++;
        if (n > 0) {
            int left = copyRandom(n), pick = -1;
            for (int i = 0; i < 4; i++) {
                const unsigned char *h = heroes + (size_t)i * CLASS_HERO_SIZE;
                if (!h[CLASS_HERO_NAME]) break;
                if (COPY_HERO_OK(h) && --left < 0) {
                    pick = i;
                    break;
                }
            }
            if (pick < 0) exitGame("gch2");
            const unsigned char *h = heroes + (size_t)pick * CLASS_HERO_SIZE;
            const char *name = (const char *)(h + CLASS_HERO_NAME);
            int t1 = *(const int *)(h + 0x60), t2 = -1;
            if (t1 != -1 && *(const int *)(h + 0x64) != -1) t2 = *(const int *)(h + 0x64);
            int x = -1, y = -1;
            if (player >= 0) copyFriendlyLocation(player, t1, t2, t2, t2, &x, &y);
            if (t1 != -1) {
                int here = copySquareTerrainOrNone(x, y);
                if (here != t1 && here != t2) return;
            }
            GAME_FN(void (*)(const char *, ...), RVA_TRACE)("Hero %s (%d,%d): %d %d %d %d at %d\n", name, cls, pick, t1, t2, t2, t2,
                                                             copySquareTerrainOrNone(x, y));
            int c = copyUnitCommander(populate(x, y, name, -999, player, -1));
            message(-1, player, x, y, 1, c, "A Hero", "%s has arrived to help your empire to greatness.", commanderTheName(c));
            return;
        }
#undef COPY_HERO_OK
    }
    if (kind != 2 && kind != 3) return;
    int (*countSquares)(int, int, int, int) = GAME_FN(int (*)(int, int, int, int), RVA_COUNT_SQUARES_OF_TERRAIN);
    int first = kind == 2 ? 7 : 5, x = -1, y = -1;
    if (countSquares(player, first, first + 1, 0) <= 0 || player < 0) return;
    if (!copyFriendlyLocation(player, first, first + 1, -1, -1, &x, &y)) return;
    int c = copyUnitCommander(populate(x, y, kind == 2 ? "Unexpected Hero" : "Old Weapon Master", -999, player, -1));
    if (kind == 2) message(-1, player, x, y, 1, c, "Unexpected Event", "A hero has arrived in one of your villages.");
    else message(-1, player, x, y, 1, c, "Good News", "An old weapon master has arrived at one of your farms.");
}
static int copyTerrainField42(int terrain) { return *(const short *)GAME_PTR(RVA_TERRAIN_TABLE + (size_t)terrain * TERRAIN_SIZE + 0x42); }
static int copyCommanderUnitOf(int c) { return c < 0 || c > G32(RVA_LAST_COMMANDER) ? -1 : G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE); }
static void priestFreespawnCopy(int player) {
    if (player < 0 || PSLOT16(RVA_SLOT_CLASS, player, 2) != 21) return;
    void (*trace)(const char *, ...) = GAME_FN(void (*)(const char *, ...), RVA_TRACE);
    int seals = G32(RVA_SEALS_BROKEN);
    trace("priest_freespawn, seals %d\n", seals);
    if (seals <= 0) return;
    if (copyRandom(100) > (PSLOT32(RVA_PLAYER_DIVINE_FAVOR, player, PLAYER_BLOCK_SIZE) + 2) * 10) return;
    int x = -1, y = -1;
    if (!GAME_FN(int (*)(int, int, int, int, int, int, int, int, int *, int *), RVA_GET_FRIENDLY_LOCATION)(player, 0, 0, 0x4000, -1, -1, -1, -1, &x, &y))
        return;
    int terrain = copySquareTerrainOrNone(x, y), m = 0;
    if (terrain >= 0) {
        m = copyTerrainField42(terrain) / 5;
        if (m < 1) m = 1;
    }
    trace("Priest freespawn t=%d, m=%d\n", terrain, m);
    int kind = copyRandom(seals + 4);
    if (kind >= 6 && copyRandom(100) < 50) kind = 10;
    void (*message)(int, int, int, int, int, int, const char *, const char *, ...) =
        GAME_FN(void (*)(int, int, int, int, int, int, const char *, const char *, ...), RVA_ADD_MESSAGE);
    int (*commander)(const char *, int, int, int, const char *) = GAME_FN(int (*)(const char *, int, int, int, const char *), RVA_POPULATE_COMMANDER);
    int (*followers)(int, const char *, int) = GAME_FN(int (*)(int, const char *, int), RVA_POPULATE_FOLLOWERS);
    int (*dieOpenEnded)(int) = GAME_FN(int (*)(int), RVA_DIE_OPEN_ENDED);
    if (copyRandom(100) < 25 - seals * 10 || kind < 2) {
        int total = 0;
        for (int sx = 0; sx < G32(RVA_MAP_DEPTH); sx++)
            for (int sy = 0; sy < G32(RVA_MAP_H); sy++) {
                if (!copyOnMapW(sx, sy) || copySquareOwner(sx, sy) != player) continue;
                if (!(G32(RVA_SQUARE_FLAGS + ((size_t)sx * MAP_COLUMN_SQUARES + (size_t)sy) * SQUARE_SIZE) & 0x4000)) continue;
                int t = copySquareTerrainOrNone(sx, sy);
                if (t < 0) continue;
                int q = (copyRandom(5) + copyTerrainField42(t)) / 5;
                if (q <= 0) continue;
                int n = copyOpenDie(q + 2) - 1;
                if (n < 0) n = 0;
                if (n && sx >= 0) copyPopulate(sx, sy, "Flagellant", n, player);
                total += n;
            }
        if (total > 0)
            message(-1, player, -1, -1, 0, 0, "Religious Fervor",
                    "A religious fervor has caused %d Flagellants to appear in your converted settlements in order to spread the word of El.", total);
        return;
    }
    int n = 0;
    switch (kind) {
    case 2:
        if (copyRandom(6) < 1) {
            if (x >= 0) copyPopulate(x, y, "Pilgrim", 1, player);
            message(-1, player, x, y, 0, 0, "Pilgrim", "A young peasant has left his home and embarked on a pilgrimage.");
            return;
        }
        n = dieOpenEnded(3 * m + 1) + 1;
        if (n && x >= 0) copyPopulate(x, y, "Flagellant", n, player);
        message(-1, player, x, y, 0, 0, "Holy Cause", "%d Flagellants have come to aid your holy cause.", n);
        return;
    case 3:
        n = dieOpenEnded(3 * m + 1) + 3;
        commander("Saint", player, x, y, "");
        if (x >= 0) copyPopulate(x, y, "Heavenly Sword", 1, player);
        if (n && x >= 0) copyPopulate(x, y, "Flagellant", n, player);
        message(-1, player, x, y, 0, 0, "Holy Cause", "A Saint protected by a Heavenly Sword and %d Flagellants have come to aid your holy cause.", n);
        return;
    case 4: {
        const char *paladin = seals >= 5 && copyRandom(100) < 30 ? "2:Paladin" : seals >= 3 && copyRandom(100) < 40 ? "1:Paladin" : "0:Paladin";
        int c = commander(paladin, player, x, y, "");
        n = copyOpenDie(m + 1) + 1;
        followers(copyCommanderUnitOf(c), "Templar", n);
        message(-1, player, x, y, 0, 0, "Holy Cause", "A Paladin and %d Templars have come to aid your holy cause.", n);
        return;
    }
    case 5:
        commander("Angel", player, x, y, "");
        message(-1, player, x, y, 0, 0, "Holy Cause", "An Angel has descended from the heavens to aid your holy cause.");
        return;
    case 6:
        commander("Harbinger", player, x, y, "");
        if (x >= 0) copyPopulate(x, y, "Heavenly Sword", 2, player);
        message(-1, player, x, y, 0, 0, "Holy Cause", "A Harbinger has descended from the heavens to aid your holy cause.");
        return;
    case 7: {
        int warriors = copyRandom(6);
        int c = commander("Arch Angel", player, x, y, "");
        followers(copyCommanderUnitOf(c), "Angelic Warrior", warriors + 1);
        if (x >= 0) copyPopulate(x, y, "Heavenly Sword", 4, player);
        message(-1, player, x, y, 0, 0, "Holy Cause", "Angelic Warriors led by an Arch Angel have descended from the heavens to aid your holy cause.");
        return;
    }
    case 8: {
        int a = copyRandom(6), b = copyRandom(6);
        int c = commander("Seraph", player, x, y, "");
        followers(copyCommanderUnitOf(c), "Angelic Warrior", a + b + 2);
        if (x >= 0) copyPopulate(x, y, "Heavenly Sword", 6, player);
        message(-1, player, x, y, 0, 0, "Holy Cause", "A Seraph and a host of Angelic Warriors have descended from the heavens to aid your holy cause.");
        return;
    }
    case 9:
        commander("Ophan", player, x, y, "");
        message(-1, player, x, y, 0, 0, "Holy Cause", "An Ophan has descended from the heavens to aid your holy cause.");
        return;
    default:
        if (kind < 10) return;
        n = copyOpenDie(3);
        if (n && x >= 0) copyPopulate(x, y, "Celestial Lion", n, player);
        message(-1, player, x, y, 0, 0, "Holy Cause", "%d Celestial Lions have descended from the heavens to aid your holy cause.", n);
        return;
    }
}
static void conscriptionCopy(int player) {
    if (player < 0) return;
    int farms = copyCountSquaresNamed(player, "Farm");
    int hamlets = copyCountSquaresNamed(player, "Coastal Hamlet") + copyCountSquaresNamed(player, "Hamlet");
    int towns = copyCountSquaresNamed(player, "Motte and Bailey") + copyCountSquaresNamed(player, "Market Village") +
                copyCountSquaresNamed(player, "Village");
    int lion = copyCountSquaresNamed(player, "Lion Tribe Village"), hyena = copyCountSquaresNamed(player, "Hyena Tribe Village");
    int spider = copyCountSquaresNamed(player, "Spider Tribe Village"), serpent = copyCountSquaresNamed(player, "Serpent Tribe Village");
    int tent = copyCountSquaresNamed(player, "Tent Village"), hoburg = copyCountSquaresNamed(player, "Hoburg Village");
    int ancient = copyCountSquaresNamed(player, "Ancient Forest"), enchanted = copyCountSquaresNamed(player, "Enchanted Forest");
    int vassals = 0;
    int (*followers)(int, const char *, int) = GAME_FN(int (*)(int, const char *, int), RVA_POPULATE_FOLLOWERS);
    for (int c = 0; c <= G32(RVA_LAST_COMMANDER); c++) {
        int unit = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE);
        if (unit < 0) continue;
        size_t at = (size_t)unit * UNIT_SIZE;
        if ((short)G16(RVA_UNIT_HP + at) <= 0 || (short)G16(RVA_UNIT_PLAYER + at) != player) continue;
        if (unit > G32(RVA_UNIT_COUNT) || (short)G16(RVA_UNIT_MONSTER + at) < 0 || copyAbilityTotal(unit, 0x1d1, 1) <= 0) continue;
        vassals++;
        followers(unit, "Spearman", 2), followers(unit, "Archer", 2), followers(unit, "Knight", 1);
    }
    towns -= vassals;
    void (*trace)(const char *, ...) = GAME_FN(void (*)(const char *, ...), RVA_TRACE);
    trace("getconscription player %d (%d %d %d)\n", player, farms, hamlets, towns);
    int x = -1, y = -1, knights = 0, spearmen = 0;
    if (!farms && !hamlets && !towns) {
        trace("Player %d has no farms\n", player);
    } else {
        x = PSLOT16(RVA_PLAYER_HOME_X, player, 2), y = PSLOT16(RVA_PLAYER_HOME_Y, player, 2);
        int owner = x >= 0 && y >= 0 && copyOnMapW(x, y) ? copySquareOwner(x, y) : -1;
        if (x >= 0 && y >= 0 && copyOnMapW(x, y) && (unsigned)owner < (unsigned)playerSlotCount() && player < playerSlotCount() &&
            (owner == player || (PSLOT16(RVA_SLOT_TEAM, owner, PLAYER_BLOCK_SIZE) > 0 &&
                                 PSLOT16(RVA_SLOT_TEAM, player, PLAYER_BLOCK_SIZE) > 0 &&
                                 PSLOT16(RVA_SLOT_TEAM, owner, PLAYER_BLOCK_SIZE) == PSLOT16(RVA_SLOT_TEAM, player, PLAYER_BLOCK_SIZE)))) {
            spearmen = (towns + farms) * 2, knights = towns + hamlets;
            copyPopulate(x, y, "Phantasmal Knight", enchanted, player);
            copyPopulate(x, y, "Unicorn Knight", ancient, player);
            copyPopulate(x, y, "Knight", knights, player);
            copyPopulate(x, y, "Spearman", spearmen, player);
            copyPopulate(x, y, "Lion Tribe Warrior", lion * 2, player);
            copyPopulate(x, y, "Hyena Tribe Warrior", hyena * 2, player);
            copyPopulate(x, y, "Wolf Tribe Bowman", tent, player);
            copyPopulate(x, y, "Spider Tribe Warrior", spider * 2, player);
            copyPopulate(x, y, "Serpent Tribe Warrior", serpent * 2, player);
            copyPopulate(x, y, "Hoburg Soldier", hoburg * 2, player);
        }
    }
    int exotic = enchanted + (serpent + spider + hyena + hoburg + lion) * 2 + tent + ancient;
    void (*message)(int, int, int, int, int, int, const char *, const char *, ...) =
        GAME_FN(void (*)(int, int, int, int, int, int, const char *, const char *, ...), RVA_ADD_MESSAGE);
    if (exotic > 0)
        message(-1, player, x, y, 0, 0, "Conscription Report", "This years conscription resulted in %d Knights, %d spearmen and %d exotic troops.",
                knights, spearmen, exotic);
    else
        message(-1, player, x, y, 0, 0, "Conscription Report", "This years conscription resulted in %d Knights and %d spearmen.", knights, spearmen);
}

static void runHatredsCopy(void) {
    GAME_FN(void (*)(const char *, int), RVA_SPELL_BUG_CHECK)("runhatreds", G32(RVA_DEBUG_LEVEL));
    for (int u = 0; u <= G32(RVA_UNIT_COUNT); u++) {
        size_t at = (size_t)u * UNIT_SIZE;
        if ((short)G16(RVA_UNIT_HP + at) <= 0) continue;
        int x = G32(RVA_UNIT_X + at), y = G32(RVA_UNIT_X + 4 + at), plane = -1;
        for (int i = 0; i <= G32(RVA_LAST_PLANE); i++) {
            int px = planeField(i, PLANE_X), py = planeField(i, PLANE_Y);
            if (x >= px && x < px + planeField(i, PLANE_W) && y >= py && y < py + planeField(i, PLANE_H)) {
                plane = i;
                break;
            }
        }
        if (plane < 0 || (short)G16(RVA_UNIT_MONSTER + at) < 0) continue;
        int hate = copyAbilityTotal(u, 0x9b, 0);
        if (hate <= 0 || copyRandom(100) >= 15) continue;
        int target = GAME_FN(int (*)(int, int, int, int, int, int, int, int), RVA_UNIT_WITH_ABILITY_AT)(-1, -1, x, y, 0x9c, hate, hate, 1);
        if (target < 0) continue;
        int last = G32(RVA_UNIT_COUNT);
        int attackerPlayer = u > last ? -1 : (short)G16(RVA_UNIT_PLAYER + at);
        int targetPlayer = target > last ? -1 : (short)G16(RVA_UNIT_PLAYER + (size_t)target * UNIT_SIZE);
        if (!playerSameTeam(targetPlayer, attackerPlayer)) continue;
        const char *(*shownName)(int) = GAME_FN(const char *(*)(int), RVA_UNIT_SHOWN_NAME);
        const char *angry = shownName(u), *killed = shownName(target);
        size_t tat = (size_t)target * UNIT_SIZE;
        GAME_FN(void (*)(int, int, int, int, int, int, const char *, const char *, ...), RVA_ADD_MESSAGE)(
            -1, targetPlayer, G32(RVA_UNIT_X + tat), G32(RVA_UNIT_X + 4 + tat), 4, 0, "Internal Strife", "A %s got killed by an angry %s", killed, angry);
        GAME_FN(void (*)(int, int), RVA_DAMAGE_UNIT)(target, 999);
    }
}
#endif

// ---- Copies both builds run -----------------------------------------------------------------------

static int nearestUnitAbilityIsCopy(int plane, int x, int y, int ability, int low, int high) {
    int (*planeAt)(int, int) = GAME_FN(int (*)(int, int), RVA_PLANE_AT);
    int best = -1, bestDistance = 999999;
    for (int u = 0; u <= G32(RVA_UNIT_COUNT) && u < UNIT_TABLE_MAX; u++) {
        size_t at = (size_t)u * UNIT_SIZE;
        if ((short)G16(RVA_UNIT_HP + at) <= 0) continue;
        int ux = G32(RVA_UNIT_X + at), uy = G32(RVA_UNIT_X + 4 + at);
        if (plane != -1) {
            int on = planeAt(ux, uy);
            if (plane == -2 ? (on & ~3) != 0 || on == 1 : on != plane) continue;
        }
        if ((short)G16(RVA_UNIT_MONSTER + at) < 0 || ux < 0) continue;
        int total = unitAbilityTotal(u, ability, 0);
        if (total < low || total > high) continue;
        int distance = abs(ux - x) + abs(uy - y);
        if (distance < bestDistance) best = u, bestDistance = distance;
    }
    return best;
}

// @platform-split kill_unit_copy: Windows calls the game's RVA_KILL_UNIT_999; Mac makes its two RVA_DAMAGE_UNIT calls itself
#ifdef __APPLE__
static void killUnitDamage(int u) {
    typedef void (*DamageFn)(int, int);
    DamageFn damage = siteInstalled[SITE_DAMAGE_UNIT] ? (DamageFn)*siteHead(SITE_DAMAGE_UNIT) : GAME_FN(DamageFn, RVA_DAMAGE_UNIT);
    damage(u, 999);
}
#endif
static void noteClientUnitKill(int unit, int monster);
static int turnFileReadPlayer(void);
static void killUnitCopy(int u) {
    if (u < 0 || u > G32(RVA_UNIT_COUNT) || u >= UNIT_TABLE_MAX) return;
    size_t at = (size_t)u * UNIT_SIZE;
    if ((short)G16(RVA_UNIT_HP + at) <= 0) return;
    int monster = (short)G16(RVA_UNIT_MONSTER + at);
#ifdef __APPLE__
    int x = G32(RVA_UNIT_X + at), y = G32(RVA_UNIT_X + 4 + at);
    killUnitDamage(u);
    if ((short)G16(RVA_UNIT_HP + at) > 0 && x >= 0 && y >= 0 && G32(RVA_UNIT_X + at) == x && G32(RVA_UNIT_X + 4 + at) == y) killUnitDamage(u);
    GAME_FN(void (*)(int), RVA_UNIT_FLUSH_CACHES)(u);
#else
    GAME_FN(void (*)(int), RVA_KILL_UNIT_999)(u);
#endif
    if ((short)G16(RVA_UNIT_HP + at) <= 0 && netRoleNow() == NET_ROLE_CLIENT && netClientInTurn && !netReplayRunning()) noteClientUnitKill(u, monster);
}

// ---- A LAN client's kills in its turn file: UKIL ----------------------------------------------------
#define UNIT_KILLS_VERSION 1
#define UNIT_KILLS_MAX     10000
typedef struct { int unit, monster; } ClientUnitKill;
static ClientUnitKill *clientUnitKills;
static int clientUnitKillCount, clientUnitKillCapacity, clientUnitKillsDropped;

static void noteClientUnitKill(int unit, int monster) {
    if (clientUnitKillCount >= UNIT_KILLS_MAX ||
        !growList((void **)&clientUnitKills, &clientUnitKillCapacity, clientUnitKillCount + 1, sizeof *clientUnitKills)) {
        clientUnitKillsDropped++;
        return;
    }
    clientUnitKills[clientUnitKillCount++] = (ClientUnitKill){unit, monster};
}

static void addUnitKillsChunk(SaveList *list) {
    if (saveTailPlayer < 0 || netRoleNow() != NET_ROLE_CLIENT) return;
    if (clientUnitKillsDropped) writeLogError("unit_kills_dropped", "reason=too_many_kills kills=%d max=%d", clientUnitKillsDropped, UNIT_KILLS_MAX);
    if (!clientUnitKillCount) {
        clientUnitKillsDropped = 0;
        return;
    }
    SaveBuffer chunk = {0};
    saveBufferInt(&chunk, UNIT_KILLS_VERSION);
    saveBufferInt(&chunk, clientUnitKillCount);
    for (int i = 0; i < clientUnitKillCount; i++) saveBufferInt(&chunk, clientUnitKills[i].unit), saveBufferInt(&chunk, clientUnitKills[i].monster);
    writeLog("unit_kills_written", "kills=%d", clientUnitKillCount);
    clientUnitKillCount = clientUnitKillsDropped = 0;
    saveListAdd(list, SAVE_TAG_UKIL, &chunk);
}

static void useUnitKillsChunk(const SaveChunk *chunk, SaveFile kind) {
    if (kind != SAVE_FILE_TURN || !chunk) return;
    if (netRoleNow() != NET_ROLE_HOST || turnFileReadPlayer() < 0) {
        writeLogWarning("unit_kills_refused", "reason=not_the_host_read");
        return;
    }
    SaveCursor c = {chunk->payload, chunk->payload + chunk->size, 0};
    int version = saveCursorInt(&c), count = saveCursorInt(&c);
    if (c.failed || version != UNIT_KILLS_VERSION || count < 0 || count > UNIT_KILLS_MAX || (size_t)(c.end - c.at) != (size_t)count * 8) {
        writeLogWarning("unit_kills_read", "reason=bad_chunk version=%d bytes=%d", version, chunk->size);
        return;
    }
    int killed = 0, left = 0;
    for (int i = 0; i < count; i++) {
        int unit = saveCursorInt(&c), monster = saveCursorInt(&c);
        if (unit < 0 || unit > G32(RVA_UNIT_COUNT) || unit >= UNIT_TABLE_MAX || (short)G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE) <= 0 ||
            (short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE) != monster) {
            left++;
            continue;
        }
        killUnitCopy(unit);
        killed++;
    }
    writeLog("unit_kills_read", "player=%d killed=%d left=%d", turnFileReadPlayer(), killed, left);
}
