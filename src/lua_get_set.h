#pragma once

// ---- Move flags, flag words, ritual and spell levels, library levels ----------------------------
static int unitFlagsOf(int unit, int words, unsigned long long *out) {
    if (G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE) < 0) return -1;
    *out = GAME_FN(unsigned long long (*)(int), words ? RVA_UNIT_RESISTANCES : RVA_UNIT_FLAGS)(unit);
    return 0;
}

static int livingCommanderUnit(int commander) {
    int unit = G32(RVA_COMMANDERS + (size_t)commander * COMMANDER_SIZE);
    if (unit < 0 || unit >= UNIT_TABLE_MAX || G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE) <= 0) return -1;
    return unit;
}

static unsigned long long commanderMoveFlags(int commander) {
    return GAME_FN(unsigned long long (*)(int), RVA_ARMY_MOVE_FLAGS)(commander);
}

static int commanderRitpowOf(int commander, int *level) {
    *level = 0;
    int unit = livingCommanderUnit(commander);
    if (unit < 0 || (*(const unsigned long long *)GAME_PTR(RVA_UNITS + (size_t)unit * UNIT_SIZE + 8) >> 28) & 1) return 0;
    int last = G32(RVA_RITPOW_COUNT);
    for (int school = 501; school <= last && school < RITUAL_EFFECT_ID_COUNT; school++) {
        int total = unitAbilityTotal(unit, school, 1);
        if (total > 0) {
            *level = total;
            return school;
        }
    }
    return 0;
}

static int squareThrough2(CallSite site, CallSite alsoRunning, int (*own)(int, int), int x, int y) {
    if (!siteInstalled[site] || luaSiteDepth[site] || (alsoRunning != SITE_COUNT && luaSiteDepth[alsoRunning])) return own(x, y);
    return ((int (*)(int, int))*siteHeadSlot(site))(x, y);
}
static int squareThrough3(CallSite site, int (*own)(int, int, int), int a, int x, int y) {
    if (!siteInstalled[site] || luaSiteDepth[site]) return own(a, x, y);
    return ((int (*)(int, int, int))*siteHeadSlot(site))(a, x, y);
}

#define LIBRARY_ENCHANT_KIND 0x484
static int libraryLevelOf(int owner, int x, int y) {
    size_t square = ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE;
    int terrain = (short)G16(RVA_SQUARE_RECORDS + square + SQUARE_TERRAIN_TYPE);
    if (terrain < 0 || terrain >= TERRAIN_TABLE_MAX) return 0;
    unsigned long long flags = *(const unsigned long long *)GAME_PTR(RVA_TERRAIN_TABLE + (size_t)terrain * TERRAIN_SIZE + TERRAIN_FLAGS);
    int base = (int)((flags >> 34) & 1);
    int enchant = GAME_FN(int (*)(int, int, int, int), RVA_SQUARE_ENCHANT_VALUE)(owner, x, y, LIBRARY_ENCHANT_KIND);
    return enchant + ((flags >> 35) & 1 ? base + 2 : base);
}
static int squareLibraryLevel(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_DEPTH) || y >= G32(RVA_MAP_H) || x >= G32(RVA_MAP_W)) return 0;
    size_t square = ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE;
    int terrain = (short)G16(RVA_SQUARE_RECORDS + square + SQUARE_TERRAIN_TYPE);
    if (terrain < 0 || terrain >= TERRAIN_TABLE_MAX) return 0;
    int owner = (signed char)G8(RVA_SQUARE_RECORDS + square + SQUARE_OWNER);
    return squareThrough3(SITE_SQUARE_LIBRARY_LEVEL, libraryLevelOf, owner, x, y);
}

#define FORT_PART_FORT 0x200
#define FORT_PARTS_LEVEL_2 0x11800
static int fortPartsOf(int x, int y) { return (int)GAME_FN(unsigned (*)(int, int), RVA_SQUARE_FORT_PARTS)(x, y); }
static unsigned squareFortParts(int x, int y) { return (unsigned)squareThrough2(SITE_SQUARE_FORT_PARTS, SITE_COUNT, fortPartsOf, x, y); }
static int squareHasFort(int x, int y) {
    if (x >= G32(RVA_MAP_DEPTH) || y >= G32(RVA_MAP_H)) return 0;
    return (squareFortParts(x, y) & FORT_PART_FORT) != 0;
}

static int waterlikeOf(int x, int y) { return GAME_FN(int (*)(int, int), RVA_SQUARE_WATERLIKE)(x, y); }
static int squareWaterlike(int x, int y) { return squareThrough2(SITE_SQUARE_WATERLIKE, SITE_COUNT, waterlikeOf, x, y) != 0; }

#define TERRAIN_FLAG_WATER     0x4
#define TERRAIN_FLAG_NOSTART   (1ULL << 18)
static int openLandOf(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_W) || y >= G32(RVA_MAP_H) || x >= MAP_COLUMNS_MAX || y >= MAP_COLUMN_SQUARES) return 0;
    size_t square = ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE;
    int terrain = (short)G16(RVA_SQUARE_RECORDS + square + SQUARE_TERRAIN_TYPE);
    if (terrain < 0 || terrain >= TERRAIN_TABLE_MAX || squareWaterlike(x, y)) return 0;
    unsigned long long flags = *(const unsigned long long *)GAME_PTR(RVA_TERRAIN_TABLE + (size_t)terrain * TERRAIN_SIZE + TERRAIN_FLAGS);
    if (flags & (TERRAIN_FLAG_WATER | TERRAIN_FLAG_NOSTART)) return 0;
    return !(G32(RVA_SQUARE_FLAGS + square) & SQUARE_FLAG_FLOODED);
}
static int squareOpenLand(int x, int y) { return squareThrough2(SITE_SQUARE_OPEN_LAND, SITE_SQUARE_WATERLIKE, openLandOf, x, y) != 0; }

// ---- Incomes: slotIncome and terrainIncome ---------------------------------------------------------
static int terrainIncomeField(int resource) {
    static const int FIELDS[RESOURCE_COUNT] = {TERRAIN_GOLD,  TERRAIN_IRON, TERRAIN_HERBS, TERRAIN_FUNGUS, TERRAIN_SACRIFICES,
                                               TERRAIN_HANDS, TERRAIN_WEED, -1, -1, -1, -1, TERRAIN_TRADE, -1, -1};
    return resource >= 0 && resource < RESOURCE_COUNT ? FIELDS[resource] : -1;
}

static int incomePercent(int controller, int bonus, int resource, int anySpecial) {
    if (controller == CONTROLLER_HUMAN) return 100;
    if (controller != CONTROLLER_AI) return 0;
    int quarter = bonus > 0 ? bonus / 4 : 0;
    if (resource != 0 && resource != 1 && resource != 11) return 100 + bonus + quarter;
    return 100 + bonus - (anySpecial ? quarter : 0);
}

static int scaledIncome(int raw, int percent) {
    return (int)(((long long)raw * percent + 50) / 100);
}

static int anyCommanderGathers(int slot, unsigned int check) {
    int last = G32(RVA_LAST_COMMANDER);
    for (int c = 0; c <= last && c < COMMANDER_TABLE_MAX; c++) {
        int unit = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE);
        int owner = unit >= 0 && unit < UNIT_TABLE_MAX ? (short)G16(RVA_UNIT_PLAYER + (size_t)unit * UNIT_SIZE) : -1;
        if (owner == slot && GAME_FN(int (*)(int), check)(c)) return 1;
    }
    return 0;
}
static int playerGathersSpecial(int slot, int resource) {
    switch (resource) {
    case 2: return GAME_FN(int (*)(int), RVA_PLAYER_GATHERS_HERBS)(slot) != 0;
    case 3: return GAME_FN(int (*)(int), RVA_PLAYER_GATHERS_FUNGUS)(slot) != 0;
    case 4: return anyCommanderGathers(slot, RVA_COMMANDER_GATHERS_SACRIFICES);
    case 5: return GAME_FN(int (*)(int), RVA_PLAYER_GATHERS_HANDS)(slot) != 0;
    case 6: return GAME_FN(int (*)(int), RVA_PLAYER_GATHERS_WEED)(slot) != 0;
    case 7: case 8: case 9: case 10:
        return anyCommanderGathers(slot, RVA_COMMANDER_GATHERS_GEMS) || GAME_FN(int (*)(int), RVA_PLAYER_GATHERS_ANY_GEMS)(slot) != 0;
    case 12: return GAME_FN(int (*)(int), RVA_PLAYER_GATHERS_RELICS)(slot) != 0;
    case 13: return GAME_FN(int (*)(int), RVA_PLAYER_GATHERS_LIFEFORCE)(slot) != 0;
    default: return 1;
    }
}
static int rawIncome(int slot, int resource) {
    switch (resource) {
    case 0: return GAME_FN(int (*)(int, int *, int), RVA_PLAYER_GOLD_INCOME)(slot, NULL, 0);
    case 1: return GAME_FN(int (*)(int), RVA_PLAYER_IRON_INCOME)(slot);
    case 2: return GAME_FN(int (*)(int), RVA_PLAYER_HERBS_INCOME)(slot);
    case 3: return GAME_FN(int (*)(int), RVA_PLAYER_FUNGUS_INCOME)(slot);
    case 4: return GAME_FN(int (*)(int), RVA_PLAYER_SACRIFICES_INCOME)(slot);
    case 5: return GAME_FN(int (*)(int), RVA_PLAYER_HANDS_INCOME)(slot);
    case 6: return GAME_FN(int (*)(int), RVA_PLAYER_WEED_INCOME)(slot);
    case 11: return GAME_FN(int (*)(int), RVA_PLAYER_TRADE_INCOME)(slot);
    case 12: return GAME_FN(int (*)(int), RVA_PLAYER_RELICS_INCOME)(slot);
    case 13: return GAME_FN(int (*)(int, int), RVA_PLAYER_LIFEFORCE_INCOME)(slot, 0);
    default: return GAME_FN(int (*)(int, int), RVA_PLAYER_GEM_INCOME)(resource - 7, slot);
    }
}
static int slotIncomeAs(int slot, int resource, int asAi) {
    int controller = (*(short *)playerSlotAddress(RVA_SLOT_CONTROLLER, slot, 2));
    if (asAi && controller == CONTROLLER_HUMAN) controller = CONTROLLER_AI;
    int level = (*(short *)playerSlotAddress(RVA_SLOT_AI, slot, PLAYER_BLOCK_SIZE));
    int bonus = level >= 0 && level < AI_LEVEL_TABLE_MAX ? G32(RVA_AI_LEVEL_BONUS + (size_t)level * 4) : 0;
    int percent = incomePercent(controller, bonus, resource, 0);
    if (percent == 0 || !playerGathersSpecial(slot, resource)) return 0;
    if (controller == CONTROLLER_AI && (resource == 0 || resource == 1 || resource == 11)) {
        static const int SPECIAL_CHECKS[] = {3, 2, 6, 13, 4, 12, 5, 7};
        int anySpecial = 0;
        for (size_t i = 0; i < sizeof SPECIAL_CHECKS / sizeof SPECIAL_CHECKS[0] && !anySpecial; i++)
            anySpecial = playerGathersSpecial(slot, SPECIAL_CHECKS[i]);
        percent = incomePercent(controller, bonus, resource, anySpecial);
    }
    return scaledIncome(rawIncome(slot, resource), percent);
}
static int slotIncome(int slot, int resource) { return slotIncomeAs(slot, resource, 0); }

// ---- The trade step of the turn start: slotAiTrade ----------------------------------------------
#define TRADE_TYPE_RANDOM_GEM 23
#define TRADE_TYPE_SELL_GEMS  24
static const struct { signed char resource, rate; } TRADE_GOODS[TRADE_TYPE_SELL_GEMS + 1] = {
    {-1, 1}, {1, 10}, {1, 10}, {4, 20}, {4, 20}, {5, 30}, {5, 30}, {2, 5}, {2, 5}, {3, 5}, {3, 5}, {6, 20}, {6, 20},
    {7, 20}, {7, 20}, {8, 20}, {8, 20}, {9, 20}, {9, 20}, {10, 20}, {10, 20}, {12, 30}, {12, 30}, {-1, 20}, {-1, 20},
};

static int tradeStepChange(int type, int mode, int points, const int stocks[RESOURCE_COUNT], int faction,
                           int change[RESOURCE_COUNT], int *randomGem) {
    memset(change, 0, RESOURCE_COUNT * sizeof change[0]);
    *randomGem = 0;
    if (points <= 0 || type < 1 || type > TRADE_TYPE_SELL_GEMS) return 0;
    int rate = TRADE_GOODS[type].rate, odd = type & 1;
    if (mode > 1) points = odd ? points * 2 : points * 4;
    int left = rate > 1 ? points * 10 % rate : 0, drawAdds = left != 0;
    int units = points * 10 / (rate < 1 ? 1 : rate) + (2 * left > rate);
    if (mode > 1) rate = odd ? rate * 2 : rate / 2;
    if (faction && type <= 5) {
        if (type == 5) change[11] = -(units < stocks[0] * 10 / rate ? units : stocks[0] * 10 / rate);
        return drawAdds;
    }
    if (type == TRADE_TYPE_SELL_GEMS) {
        for (int gem = 7; gem <= 10 && units > 0; gem++) {
            int n = units < stocks[gem] ? units : stocks[gem];
            if (n < 0) n = 0;
            change[0] += n * rate / 10, change[gem] -= n, change[11] -= n, units -= n;
        }
        return drawAdds;
    }
    int good = TRADE_GOODS[type].resource, n;
    if (odd) {
        int afford = stocks[0] * 10 / rate;
        n = units < afford ? units : afford;
        change[0] = -(n * rate / 10);
        if (good >= 0) change[good] = n;
        else *randomGem = n;
    } else {
        n = units < stocks[good] ? units : stocks[good];
        if (n < 0) n = 0;
        change[0] = n * rate / 10;
        change[good] = -n;
    }
    change[11] = -n;
    return drawAdds;
}

static int slotAiTrade(int slot, int resource) {
    if (resource < 0 || resource >= RESOURCE_COUNT) return 0;
    int type = (*(short *)playerSlotAddress(RVA_PLAYER_TRADE_TYPE, slot, PLAYER_BLOCK_SIZE));
    int mode = (*(short *)playerSlotAddress(RVA_PLAYER_TRADE_MODE, slot, PLAYER_BLOCK_SIZE));
    if (type < 1 || type > TRADE_TYPE_SELL_GEMS) return 0;
    int stocks[RESOURCE_COUNT] = {0}, change[RESOURCE_COUNT], randomGem;
    int good = TRADE_GOODS[type].resource;
    for (int r = 0; r < RESOURCE_COUNT; r++) {
        if (r != 0 && r != good && !(type == TRADE_TYPE_SELL_GEMS && r >= 7 && r <= 10)) continue;
        int stock = *(int *)playerSlotAddress(RVA_PLAYER_INTS + (size_t)r * 4, slot, PLAYER_BLOCK_SIZE) + slotIncomeAs(slot, r, 1);
        stocks[r] = stock > 0 ? stock : 0;
    }
    tradeStepChange(type, mode, slotIncomeAs(slot, 11, 1), stocks, slot >= PLAYER_SLOTS, change, &randomGem);
    return change[resource];
}

// ---- What stays home: commanderGarrisonLeft --------------------------------------------------------
enum { GARRISON_GAME, GARRISON_JOINED, GARRISON_DROPPED };
static unsigned char garrisonState[UNIT_TABLE_MAX];
static unsigned long long garrisonUnitFlags[UNIT_TABLE_MAX];

static int garrisonLeader(int c, int u) {
    if (garrisonState[u] == GARRISON_JOINED) return c;
    if (garrisonState[u] == GARRISON_DROPPED) return -1;
    return G32(RVA_UNIT_LEADER + (size_t)u * UNIT_SIZE);
}
static unsigned long long garrisonUnitFlagsOf(int u) { return GAME_FN(unsigned long long (*)(int), RVA_UNIT_FLAGS)(u); }

static int garrisonUnitStrength(int u) {
    if (u < 0 || u >= UNIT_TABLE_MAX) return 0;
    return GAME_FN(int (*)(int, int, int), RVA_UNIT_STRENGTH_CORE)(u, -1, (short)G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE));
}
static int garrisonUnitX(int u) { return u < 0 || u >= UNIT_TABLE_MAX ? -1 : G32(RVA_UNIT_X + (size_t)u * UNIT_SIZE); }
static int garrisonUnitY(int u) { return u < 0 || u >= UNIT_TABLE_MAX ? -1 : G32(RVA_UNIT_X + 4 + (size_t)u * UNIT_SIZE); }
static int garrisonUnitHp(int u) { return u < 0 || u >= UNIT_TABLE_MAX ? 0 : (short)G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE); }
static int garrisonUnitOwner(int u) {
    if (u < 0 || u >= UNIT_TABLE_MAX || u > G32(RVA_UNIT_COUNT)) return -1;
    return (short)G16(RVA_UNIT_PLAYER + (size_t)u * UNIT_SIZE);
}
static int garrisonCommanderOf(int u) {
    int last = G32(RVA_LAST_COMMANDER);
    for (int c = 0; c <= last && c < COMMANDER_TABLE_MAX; c++)
        if (G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE) == u) return c;
    return -1;
}
static int garrisonSameTeam(int a, int b) { return playerSameTeam(a, b); }
static int garrisonSquareTerrain(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_W) || y >= G32(RVA_MAP_H) || x >= MAP_COLUMNS_MAX || y >= MAP_COLUMN_SQUARES) return -1;
    return (short)G16(RVA_SQUARE_RECORDS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE + SQUARE_TERRAIN_TYPE);
}
static int garrisonFortLevel(int x, int y) {
    if (!squareHasFort(x, y)) return 0;
    return (squareFortParts(x, y) & FORT_PARTS_LEVEL_2) ? 2 : 1;
}

static int garrisonArmyStrength(int c, int last, unsigned long long flags) {
    int sum = 0;
    for (int u = 0; u <= last; u++)
        if (garrisonLeader(c, u) == c && (garrisonUnitFlagsOf(u) & flags) == flags) sum += garrisonUnitStrength(u);
    int unit = c <= G32(RVA_LAST_COMMANDER) ? G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE) : -1;
    return sum + garrisonUnitStrength(unit);
}

static int garrisonReformsHere(int player, int x, int y, int last) {
    int randomIndex = G32(RVA_RANDOM_INDEX), randomShift = G32(RVA_RANDOM_SHIFT), randomXor = G32(RVA_RANDOM_XOR);
    for (int u = 0; u <= last; u++) garrisonUnitFlags[u] = *(unsigned long long *)(gameBase + RVA_UNITS + (size_t)u * UNIT_SIZE + 8);
    int here = GAME_FN(int (*)(int, int, int), RVA_AI_REFORMS_HERE)(player, x, y);
    for (int u = 0; u <= last; u++) *(unsigned long long *)(gameBase + RVA_UNITS + (size_t)u * UNIT_SIZE + 8) = garrisonUnitFlags[u];
    G32(RVA_RANDOM_INDEX) = randomIndex, G32(RVA_RANDOM_SHIFT) = randomShift, G32(RVA_RANDOM_XOR) = randomXor;
    return here;
}

static int garrisonMayJoin(int c, int u, int player, int x, int y, int kind) {
    if (garrisonUnitX(u) != x || garrisonUnitY(u) != y) return 0;
    if (garrisonUnitHp(u) <= 0 || garrisonLeader(c, u) != -1) return 0;
    if (garrisonUnitOwner(u) != player || garrisonCommanderOf(u) != -1) return 0;
    return GAME_FN(int (*)(int, int), RVA_AI_UNIT_JOIN_KIND)(c, u) >= kind;
}

static void garrisonDropFollowers(int c, int last, int (*drop)(int u)) {
    for (int u = 0; u <= last; u++)
        if (garrisonLeader(c, u) == c && drop(u)) garrisonState[u] = GARRISON_DROPPED;
}
static int garrisonHolds(int u) { return unitAbilityTotal(u, 0x34, 0) == 1; }
static int garrisonLacks10(int u) { return !(garrisonUnitFlagsOf(u) & 0x10); }
static int garrisonLacks2(int u) { return !(garrisonUnitFlagsOf(u) & 2); }
static int garrisonHas8(int u) { return (garrisonUnitFlagsOf(u) & 8) != 0; }

static void garrisonGather(int c, int unit, int player, int x, int y, int last) {
    if ((garrisonUnitFlagsOf(unit) >> MONSTER_FLAG_NO_COMMANDER) & 1) return;
    if ((GAME_FN(unsigned long long (*)(int), RVA_UNIT_RESISTANCES)(unit) >> UNIT_RESISTANCE_NOLEADER) & 1) return;
    if (*(unsigned long long *)(gameBase + RVA_UNITS + (size_t)unit * UNIT_SIZE) & UNIT_AI_SAILOR) return;
    int terrain = garrisonSquareTerrain(x, y);
    if (terrain < 0 || terrain >= TERRAIN_TABLE_MAX) return;
    int gold = G16(RVA_TERRAIN_TABLE + (size_t)terrain * TERRAIN_SIZE + TERRAIN_GOLD);
    int citadel = squareCitadelFor(player, x, y) != 0;
    int keep = 0;
    if (citadel) keep = GAME_FN(int (*)(int), RVA_AI_COUNT_CITADELS)(player) <= 1 ? 60 : 20;
    if (gold >= 4 && keep < 10) keep = 10;
    if (garrisonReformsHere(player, x, y, last) && keep < 50) keep = 50;
    int own = garrisonUnitStrength(unit) + GAME_FN(int (*)(int), RVA_COMMANDER_CASTER_BONUS)(c);
    int savedCount = G32(RVA_FREE_TROOPS_COUNT);
    int free = GAME_FN(int (*)(int, int), RVA_FREE_TROOPS_STRENGTH)(x, y);
    int count = G32(RVA_FREE_TROOPS_COUNT);
    G32(RVA_FREE_TROOPS_COUNT) = savedCount;
    int army = garrisonArmyStrength(c, last, 0);
    if (free > keep && count > 1) {
        int take = (int)((unsigned)(100 - keep) * (unsigned)free) / 100;
        if (take > 0) {
            for (int u = 0; u <= last; u++)
                if (garrisonMayJoin(c, u, player, x, y, 2)) garrisonState[u] = GARRISON_JOINED, take -= garrisonUnitStrength(u), count--;
            for (int r = 0; r <= 3 && r <= last; r++)
                for (int u = r; u <= last && take > 0 && (count > 1 || keep == 0) && count > 0; u += 4)
                    if (garrisonMayJoin(c, u, player, x, y, 1)) garrisonState[u] = GARRISON_JOINED, take -= garrisonUnitStrength(u), count--;
        }
    } else if (count == 0 && army > own && keep) {
        int t = (int)((unsigned)army * (unsigned)keep) / 100;
        for (int start = 0; start <= 1; start++)
            for (int u = start; u <= last && t > own; u += 2) {
                if (garrisonLeader(c, u) != c || garrisonUnitHp(u) <= 0) continue;
                int strength = garrisonUnitStrength(u);
                if (t - strength < own || !GAME_FN(int (*)(int), RVA_AI_UNIT_MAY_GUARD)(u)) continue;
                garrisonState[u] = GARRISON_DROPPED;
                t -= strength;
            }
    }
    if (squareHasFort(x, y)) garrisonDropFollowers(c, last, garrisonHolds);
    if (gold <= 1 && !citadel) return;
    unsigned long long f = garrisonUnitFlagsOf(unit);
    if (f & 0x10) {
        army = garrisonArmyStrength(c, last, 0);
        if (army - garrisonArmyStrength(c, last, 0x10) < army / 2) garrisonDropFollowers(c, last, garrisonLacks10);
    }
    if (f & 2) {
        army = garrisonArmyStrength(c, last, 0);
        if (army - garrisonArmyStrength(c, last, 2) < army / 4) garrisonDropFollowers(c, last, garrisonLacks2);
    }
    if (!(f & 8)) {
        army = garrisonArmyStrength(c, last, 0);
        if (garrisonArmyStrength(c, last, 8) < army / 10) garrisonDropFollowers(c, last, garrisonHas8);
    }
}

static int garrisonLeaves(int c, int cUnit, int u) {
    if (u == cUnit) return 1;
    int leader = garrisonLeader(c, u);
    for (int depth = 0; depth < 8 && leader >= 0 && leader < COMMANDER_TABLE_MAX; depth++) {
        if (leader == c) return 1;
        int leaderUnit = G32(RVA_COMMANDERS + (size_t)leader * COMMANDER_SIZE);
        if (leaderUnit < 0 || leaderUnit >= UNIT_TABLE_MAX) return 0;
        leader = garrisonLeader(c, leaderUnit);
    }
    return 0;
}

static int commanderGarrisonLeft(int c) {
    int unit = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE);
    if (unit < 0 || unit >= UNIT_TABLE_MAX || G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE) <= 0) return -1;
    int player = garrisonUnitOwner(unit);
    int x = garrisonUnitX(unit), y = garrisonUnitY(unit);
    if (player < 0 || player >= playerSlotCount() || x < 0 || y < 0) return -1;
    int last = G32(RVA_UNIT_COUNT);
    if (last >= UNIT_TABLE_MAX) last = UNIT_TABLE_MAX - 1;
    memset(garrisonState, GARRISON_GAME, sizeof garrisonState);
    garrisonGather(c, unit, player, x, y, last);
    long long sum = 0;
    int count = 0, ranged = 0;
    for (int u = 0; u <= last; u++) {
        if (!unitAliveAt(u, x, y)) continue;
        if (!garrisonSameTeam(player, garrisonUnitOwner(u))) continue;
        if (garrisonLeaves(c, unit, u)) continue;
        sum += unitStrengthFor(player, u);
        count++;
        ranged += GAME_FN(int (*)(int, int, int), RVA_UNIT_HAS_RANGED)(u, 3, 1) != 0;
    }
    if (ranged > count / 10) sum = sum * (100 + 60 * garrisonFortLevel(x, y)) / 100;
    return sum < INT_MAX ? (int)sum : INT_MAX;
}

static int rememberedStrength(const int *memo, int x, int y, int (*measure)(int, int, int)) {
    if (x >= G32(RVA_MAP_DEPTH) || y >= G32(RVA_MAP_H)) return 0;
    return *memo >= 0 ? *memo : measure(-1, x, y);
}

typedef struct {
    const unsigned char *record;
    int commander, unit, player, x, y;
} RitualCaster;

static int ritualFxValue(const unsigned char *record, int i) {
    return (int)*(const long long *)(record + RITUAL_FX + (size_t)i * RITUAL_FX_SIZE + RITUAL_FX_VALUE);
}
static int ritualFxId(const unsigned char *record, int i) { return *(const int *)(record + RITUAL_FX + (size_t)i * RITUAL_FX_SIZE); }

static int ritualFirstFx(const unsigned char *record, int id, int none) {
    for (int i = 0; i < RITUAL_FX_MAX && ritualFxId(record, i) >= 1; i++)
        if (ritualFxId(record, i) == id) return ritualFxValue(record, i);
    return none;
}

static void ritualStringDecode(const char *line, char *name, size_t size, int *kind) {
    char text[RITUAL_STRING_SIZE + 1];
    size_t len = strnlen(line, RITUAL_STRING_SIZE);
    memcpy(text, line, len);
    text[len] = 0;
    *kind = 0;
    if (text[0] == '|') return;
    if (text[0] == '(') {
        static const char MARKS[] = "-+!&|";
        const char *mark = text[1] ? strchr(MARKS, text[1]) : NULL;
        if (mark && text[2] == ')') *kind = (int)(mark - MARKS) + 1;
        char *close = strchr(text, ')');
        if (close) memmove(text, close + 1, strlen(close + 1) + 1);
    }
    char *amp = strstr(text, " & ");
    if (amp) *amp = 0;
    const char *star = strchr(text, '*');
    snprintf(name, size, "%.*s", (int)(size - 1), star ? star + 1 : text);
}

static int monstersHomeAt(int x, int y, const char *name) {
    int count = 0, last = G32(RVA_UNIT_COUNT);
    for (int u = 0; u <= last && u < UNIT_TABLE_MAX; u++) {
        int monster = G16(RVA_UNIT_MONSTER + (size_t)u * UNIT_SIZE);
        if (G16(RVA_UNIT_HOME_X + (size_t)u * UNIT_SIZE) != x || G16(RVA_UNIT_HOME_X + 2 + (size_t)u * UNIT_SIZE) != y) continue;
        if (G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE) < 1 || !monsterRecord(monster)) continue;
        count += !strncmp((const char *)monsterRecord(monster), name, MONSTER_NAME_SIZE);
    }
    return count;
}

enum { COUNT_HERE, COUNT_HOME_HERE, COUNT_WORLD, COUNT_PLAYER, COUNT_ELYSIUM };
static int ritualMonsterCount(const RitualCaster *c, int how, const char *name) {
    int (*at)(int, int, const char *) = GAME_FN(int (*)(int, int, const char *), RVA_COUNT_MONSTERS_AT);
    int (*plane)(int, const char *) = GAME_FN(int (*)(int, const char *), RVA_COUNT_MONSTERS_ON_PLANE);
    switch (how) {
    case COUNT_HERE: return at(c->x, c->y, name);
    case COUNT_HOME_HERE: return monstersHomeAt(c->x, c->y, name);
    case COUNT_WORLD: return at(-1, -1, name);
    case COUNT_PLAYER: return GAME_FN(int (*)(int, const char *), RVA_COUNT_PLAYER_MONSTERS)(c->player, name);
    default: return plane(0, name) + plane(2, name) + plane(3, name);
    }
}

static int ritualUnitRuleMet(const RitualCaster *c, int id, int value) {
    const char *strings = (const char *)c->record + RITUAL_STRINGS;
    char name[100] = "";
    int how, count = 0;
    int casterMonster = G16(RVA_UNIT_MONSTER + (size_t)c->unit * UNIT_SIZE);
    const char *casterName = (const char *)monsterRecord(casterMonster);
    switch (id) {
    case RITUAL_FX_NOMONREQ: case RITUAL_FX_FEWMONREQ: {
        int limit = id == RITUAL_FX_NOMONREQ ? value - 1 : value;
        if (id == RITUAL_FX_FEWMONREQ && ritualFirstFx(c->record, RITUAL_FX_NOMONREQ, 0) != 0) return 1;
        if (value == 0 || limit < 0) return 1;
        for (int i = 0; i < RITUAL_STRINGS_MAX && strings[(size_t)i * RITUAL_STRING_SIZE]; i++) {
            int kind;
            ritualStringDecode(strings + (size_t)i * RITUAL_STRING_SIZE, name, sizeof name, &kind);
            if (kind != 1 && kind != 2) continue;
            count += ritualMonsterCount(c, COUNT_HERE, name);
            if (count > limit) return kind == 2;
        }
        return 1;
    }
    case RITUAL_FX_MINMONREQ: case RITUAL_FX_MONWORLDREQ: case RITUAL_FX_MONPLAYERREQ:
        how = id == RITUAL_FX_MINMONREQ ? COUNT_HERE : id == RITUAL_FX_MONWORLDREQ ? COUNT_WORLD : COUNT_PLAYER;
        if (value < 1) return 1;
        for (int i = 0; i < RITUAL_STRINGS_MAX && strings[(size_t)i * RITUAL_STRING_SIZE]; i++) {
            int kind;
            ritualStringDecode(strings + (size_t)i * RITUAL_STRING_SIZE, name, sizeof name, &kind);
            if (kind == 4 && (count += ritualMonsterCount(c, how, name)) >= value) return 1;
        }
        return 0;
    case RITUAL_FX_NOMONMASTERYREQ:
        if (value < 1 || unitAbilityTotal(c->unit, *(const int *)(c->record + RITUAL_RITPOW), 0) > *(const int *)(c->record + RITUAL_LEVEL))
            return 1;
        for (int i = 0; i < RITUAL_STRINGS_MAX && strings[(size_t)i * RITUAL_STRING_SIZE]; i++) {
            int kind;
            ritualStringDecode(strings + (size_t)i * RITUAL_STRING_SIZE, name, sizeof name, &kind);
            if (kind == 5 && casterName && !GAME_FN(int (*)(const char *, const char *), RVA_NAME_COMPARE)(casterName, name)) return 1;
            if ((kind == 1 || kind == 2) && ritualMonsterCount(c, COUNT_PLAYER, name) > 0) return kind == 2;
        }
        return 1;
    default:
        how = id == RITUAL_FX_NOMONHOMEREQ ? COUNT_HOME_HERE : id == RITUAL_FX_NOMONWORLDREQ ? COUNT_WORLD
            : id == RITUAL_FX_NOMONPLAYERREQ ? COUNT_PLAYER : COUNT_ELYSIUM;
        if (value < 1) return 1;
        for (int i = 0; i < RITUAL_STRINGS_MAX && strings[(size_t)i * RITUAL_STRING_SIZE]; i++) {
            int kind;
            ritualStringDecode(strings + (size_t)i * RITUAL_STRING_SIZE, name, sizeof name, &kind);
            if (how == COUNT_ELYSIUM) count += ritualMonsterCount(c, how, name);
            if (kind == 5 && casterName && !GAME_FN(int (*)(const char *, const char *), RVA_NAME_COMPARE)(casterName, name)) return 1;
            if (kind != 1 && kind != 2) continue;
            if (how != COUNT_ELYSIUM) count += ritualMonsterCount(c, how, name);
            if (count >= value) return kind == 2;
        }
        return 1;
    }
}

static int unitWithAbilityAt(int x, int y, int ability) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_DEPTH) || y >= G32(RVA_MAP_H) || x >= MAP_COLUMNS_MAX || y >= MAP_COLUMN_SQUARES) return 0;
    if (!G16(RVA_SQUARE_ARMY + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * 2)) return 0;
    int last = G32(RVA_UNIT_COUNT);
    for (int u = 0; u <= last && u < UNIT_TABLE_MAX; u++) {
        if (G32(RVA_UNIT_X + (size_t)u * UNIT_SIZE) != x || G32(RVA_UNIT_X + 4 + (size_t)u * UNIT_SIZE) != y) continue;
        if (G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE) < 1) continue;
        int total = unitAbilityTotal(u, ability, 1);
        if (total >= 1 && total <= 999999) return 1;
    }
    return 0;
}

static int portalAt(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_MAP_DEPTH) || y >= G32(RVA_MAP_H) || x >= MAP_COLUMNS_MAX || y >= MAP_COLUMN_SQUARES) return 0;
    return G16(RVA_SQUARE_RECORDS + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE + SQUARE_PORTAL);
}

static int ritualCasterRuleMet(const RitualCaster *c, int id, int v) {
    switch (id) {
    case RITUAL_FX_LEVELREQ: {
        if (v < 1) return 1;
        if (unitAbilityTotal(c->unit, *(const int *)(c->record + RITUAL_RITPOW), 0) != v) return 0;
        int last = G32(RVA_RITPOW_COUNT);
        for (int a = 501; a <= last && a < RITUAL_EFFECT_ID_COUNT; a++)
            if (unitAbilityTotal(c->unit, a, 0) > v) return 0;
        return 1;
    }
    case RITUAL_FX_RESRESTRICT: case RITUAL_FX_RESREQ: case RITUAL_FX_BENRESTRICT: {
        if (v == 0) return 1;
        unsigned long long bits = GAME_FN(unsigned long long (*)(int), id == RITUAL_FX_BENRESTRICT ? RVA_UNIT_BENEFITS : RVA_UNIT_RESISTANCES)(c->unit);
        unsigned long long want = (unsigned long long)(long long)v;
        return id == RITUAL_FX_RESREQ ? (want & ~bits) == 0 : (bits & want) == 0;
    }
    case RITUAL_FX_FXREQ:
        return v == 0 || GAME_FN(int (*)(int, int, int), RVA_COMMANDER_ABILITY_TOTAL)(c->commander, v, 1) > 0;
    case RITUAL_FX_NOPLANEREQ: return v == 999 || GAME_FN(int (*)(int, int), RVA_PLANE_AT)(c->x, c->y) != v;
    case RITUAL_FX_SEASONREQ: return v < 0 || G16(RVA_SEASON) == v;
    case RITUAL_FX_EVENTVARREQ: return v < 1 || v >= EVENT_VAR_COUNT || G32(RVA_EVENT_VARS + (size_t)v * 4) >= 1;
    case RITUAL_FX_NOEVENTVARREQ: return v < 1 || v >= EVENT_VAR_COUNT || G32(RVA_EVENT_VARS + (size_t)v * 4) <= 0;
    case RITUAL_FX_HASPORTALREQ: return v == 0 || GAME_FN(int (*)(int, int, int), RVA_COUNT_PLAYER_PORTALS)(c->player, -1, 2) > 0;
    case RITUAL_FX_NOPORTALREQ: return v < 1 || portalAt(c->x, c->y) <= 0;
    case RITUAL_FX_ENCHHEREREQ: {
        if (v < 1) return 1;
        int onMap = c->x >= 0 && c->y >= 0 && c->x < MAP_COLUMNS_MAX && c->y < MAP_COLUMN_SQUARES;
        int terrain = onMap ? G16(RVA_SQUARE_RECORDS + ((size_t)c->x * MAP_COLUMN_SQUARES + (size_t)c->y) * SQUARE_SIZE + SQUARE_TERRAIN_TYPE) : -1;
        return (v == 19 && terrain == 282) || GAME_FN(int (*)(int, int, int, int), RVA_ENCHANT_COUNT)(c->player, v, c->x, c->y) > 0;
    }
    case RITUAL_FX_NOFXHEREREQ: return v < 1 || !unitWithAbilityAt(c->x, c->y, v);
    case RITUAL_FX_FORTREQ: case RITUAL_FX_NOFORTREQ: {
        if (v == 0) return 1;
        unsigned parts = GAME_FN(unsigned (*)(int, int), RVA_SQUARE_FORT_PARTS)(c->x, c->y);
        return id == RITUAL_FX_FORTREQ ? (parts & (unsigned)v) == (unsigned)v : (parts & (unsigned)v) == 0;
    }
    case RITUAL_FX_HOMEREQ: {
        int home = G16(RVA_UNIT_HOME_X + (size_t)c->unit * UNIT_SIZE) == c->x && G16(RVA_UNIT_HOME_X + 2 + (size_t)c->unit * UNIT_SIZE) == c->y;
        return v == 0 || (v > 0 ? home : !home);
    }
    case RITUAL_FX_HOMECITREQ: {
        if (v == 0 || c->player < 0 || c->player >= playerSlotCount()) return 1;
        int hx = (*(short *)playerSlotAddress(RVA_PLAYER_HOME_X, c->player, 2)), hy = (*(short *)playerSlotAddress(RVA_PLAYER_HOME_Y, c->player, 2));
        return v < 0 ? !(c->x == hx && c->y == hy) : c->x == hx && c->y != hy;
    }
    case RITUAL_FX_NOMONREQ: case RITUAL_FX_FEWMONREQ: case RITUAL_FX_MINMONREQ: case RITUAL_FX_MONWORLDREQ:
    case RITUAL_FX_MONPLAYERREQ: case RITUAL_FX_NOMONMASTERYREQ: case RITUAL_FX_NOMONHOMEREQ:
    case RITUAL_FX_NOMONWORLDREQ: case RITUAL_FX_NOMONPLAYERREQ: case RITUAL_FX_NOMONELYSIUMREQ:
        return ritualUnitRuleMet(c, id, v);
    case RITUAL_FX_PROMOTION: {
        if (v == 0) return 1;
        const char *strings = (const char *)c->record + RITUAL_STRINGS;
        int (*named)(int, int, int, const char *) = GAME_FN(int (*)(int, int, int, const char *), RVA_COUNT_UNITS_NAMED);
        char line[RITUAL_STRING_SIZE + 1];
        for (int i = 0; i < RITUAL_STRINGS_MAX && strings[(size_t)i * RITUAL_STRING_SIZE]; i++) {
            const char *at = strings + (size_t)i * RITUAL_STRING_SIZE;
            if (!strncmp(at, "(-)", 3)) continue;
            snprintf(line, sizeof line, "%.*s", RITUAL_STRING_SIZE, at);
            if (named(c->player, c->x, c->y, line) > 0) return 1;
            i++;
        }
        return 0;
    }
    default: return -1;
    }
}

static int ritualEffectMetAt(const unsigned char *record, int i, int commander) {
    static const int RADII[] = {1, 3, 5, 7, 99};
    int unit = G32(RVA_COMMANDERS + (size_t)commander * COMMANDER_SIZE);
    if (unit < 0 || unit >= UNIT_TABLE_MAX) return -1;
    RitualCaster c = {record, commander, unit, G16(RVA_UNIT_PLAYER + (size_t)unit * UNIT_SIZE),
                      G32(RVA_UNIT_X + (size_t)unit * UNIT_SIZE), G32(RVA_UNIT_X + 4 + (size_t)unit * UNIT_SIZE)};
    if (c.player < 0) return -1;
    int id = ritualFxId(record, i);
    for (int j = 0; j < i; j++)
        if (ritualFxId(record, j) == id) return 1;
    int code, rule = ritualNearbyRule(record + RITUAL_FX + (size_t)i * RITUAL_FX_SIZE, &code);
    if (rule < 0) return ritualCasterRuleMet(&c, id, ritualFxValue(record, i));
    if (c.x < 0 || c.x >= MAP_COLUMNS_MAX || c.y < 0 || c.y >= MAP_COLUMN_SQUARES) return -1;
    int count = GAME_FN(int (*)(int, int, int, int), RVA_TERRAIN_NEAR_COUNT)(c.x, c.y, code, RADII[rule % 5]);
    return rule < 5 ? count == 0 : count > 0;
}

static const char *fortPartWords[FORT_PART_BITS];
static int fortPartWordsRead;

static int singleBitIndex(uint32_t value) {
    if (!value || (value & (value - 1))) return -1;
    int n = 0;
    while (!(value & 1)) value >>= 1, n++;
    return n;
}

static __attribute__((unused)) int readFortWordsX64(const unsigned char *code, size_t size, uintptr_t at, uintptr_t match, const char **words) {
    int found = 0;
    for (size_t i = 20; i + 5 <= size; i++) {
        if (code[i] != 0xe8 || code[i - 20] != 0xba || memcmp(code + i - 15, "\x48\x8d\x0d", 3) || code[i - 8] != 0x41 || code[i - 7] != 0xb8)
            continue;
        int32_t rel, disp;
        uint32_t bit;
        memcpy(&rel, code + i + 1, 4);
        if (at + i + 5 + (intptr_t)rel != match) continue;
        memcpy(&bit, code + i - 19, 4);
        memcpy(&disp, code + i - 12, 4);
        int n = singleBitIndex(bit);
        if (n < 0) continue;
        words[n] = (const char *)(at + i - 8 + (intptr_t)disp);
        found++;
    }
    return found;
}

static __attribute__((unused)) int readFortWordsArm64(const unsigned char *code, size_t size, uintptr_t at, uintptr_t match, const char **words) {
    int found = 0;
    for (size_t i = 12; i + 4 <= size; i += 4) {
        uint32_t adrp, add, mov, bl;
        memcpy(&adrp, code + i - 12, 4);
        memcpy(&add, code + i - 8, 4);
        memcpy(&mov, code + i - 4, 4);
        memcpy(&bl, code + i, 4);
        if ((bl & 0xfc000000) != 0x94000000 || (adrp & 0x9f00001f) != 0x90000000 || (add & 0xffc003ff) != 0x91000000 ||
            (mov & 0xff80001f) != 0x52800001) continue;
        int32_t offset = (int32_t)(bl << 6) >> 4;
        if (at + i + (intptr_t)offset != match) continue;
        int64_t pageImm = (int64_t)(((uint64_t)((adrp >> 5) & 0x7ffff) << 2 | ((adrp >> 29) & 3)) << 43) >> 31;
        uintptr_t page = ((at + i - 12) & ~(uintptr_t)0xfff) + (intptr_t)pageImm;
        int n = singleBitIndex(((mov >> 5) & 0xffff) << (16 * ((mov >> 21) & 3)));
        if (n < 0) continue;
        words[n] = (const char *)(page + ((add >> 10) & 0xfff));
        found++;
    }
    return found;
}

static const char *fortPartWord(int bit) {
    if (!fortPartWordsRead) {
        fortPartWordsRead = 1;
        const unsigned char *code = GAME_PTR(RVA_FORT_WORD_CALLS);
#ifdef __APPLE__
        int found = readFortWordsArm64(code, FORT_WORD_CALLS_SIZE, (uintptr_t)code, (uintptr_t)GAME_PTR(RVA_FORT_WORD_MATCH), fortPartWords);
#else
        int found = readFortWordsX64(code, FORT_WORD_CALLS_SIZE, (uintptr_t)code, (uintptr_t)GAME_PTR(RVA_FORT_WORD_MATCH), fortPartWords);
#endif
        writeLog("fort_part_words_read", "found=%d", found);
    }
    return bit >= 0 && bit < FORT_PART_BITS ? fortPartWords[bit] : NULL;
}

static unsigned int ceremonyNameRva(int ceremony) {
    switch (ceremony) {
    case CEREMONY_CONVERT: return RVA_STR_CEREMONY_CONVERT;
    case CEREMONY_BREAK_SEAL: return RVA_STR_CEREMONY_BREAK_SEAL;
    case CEREMONY_BOLSTER_DEVOTION: return RVA_STR_CEREMONY_BOLSTER;
    case CEREMONY_START_CRUSADE: return RVA_STR_CEREMONY_CRUSADE;
    case CEREMONY_CALL_INQUISITION: return RVA_STR_CEREMONY_INQUISITION;
    case CEREMONY_MASTERY_ARCH_BISHOP:
    case CEREMONY_MASTERY_CARDINAL:
    case CEREMONY_MASTERY_PONTIFF: return RVA_STR_CEREMONY_MASTERY;
    case CEREMONY_INCREASE_FAVOR: return RVA_STR_CEREMONY_FAVOR;
    default: return 0;
    }
}

#define CEREMONY_COST_ENTRY(name) (int)RVA_CEREMONY_COST_##name
#ifdef __APPLE__
#define CEREMONY_MASTERY_ENTRY(name) (int)RVA_CEREMONY_COST_##name##_SET
#else
#define CEREMONY_MASTERY_ENTRY(name) (int)RVA_CEREMONY_COST_##name
#endif
static int ceremonyCostAt(int ceremony, int *also) {
    *also = -1;
    switch (ceremony) {
    case CEREMONY_CONVERT: return CEREMONY_COST_ENTRY(CONVERT);
    case CEREMONY_BREAK_SEAL: *also = CEREMONY_COST_ENTRY(SEAL_BASE); return CEREMONY_COST_ENTRY(SEAL_STEP);
    case CEREMONY_BOLSTER_DEVOTION: *also = CEREMONY_COST_ENTRY(BOLSTER_BASE); return CEREMONY_COST_ENTRY(BOLSTER_STEP);
    case CEREMONY_START_CRUSADE: return CEREMONY_COST_ENTRY(CRUSADE);
    case CEREMONY_CALL_INQUISITION: return CEREMONY_COST_ENTRY(INQUISITION);
    case CEREMONY_MASTERY_ARCH_BISHOP: return CEREMONY_MASTERY_ENTRY(ARCH_BISHOP);
    case CEREMONY_MASTERY_CARDINAL: return CEREMONY_MASTERY_ENTRY(CARDINAL);
    case CEREMONY_MASTERY_PONTIFF: return CEREMONY_MASTERY_ENTRY(PONTIFF);
    case CEREMONY_INCREASE_FAVOR: return CEREMONY_COST_ENTRY(FAVOR_STEP);
    default: return -1;
    }
}
#define CEREMONY_COST_MAX 100000

static int setCeremonyCost(lua_State *L, unsigned char *table, int arg) {
    lua_Integer ceremony = lua_tointeger(L, 2), value = luaL_checkinteger(L, arg);
    int also, at = ceremonyCostAt((int)ceremony, &also);
    LuaScript *s = luaScriptOf(L);
    if (at < 0) {
        refuseLuaCall(s ? s->mod : "?", "coe5.set", "no_cost", "name=ceremonyCost ceremony=%lld", (long long)ceremony);
        return 0;
    }
    if (value < 0 || value > CEREMONY_COST_MAX) {
        refuseLuaCall(s ? s->mod : "?", "coe5.set", "out_of_range", "name=ceremonyCost ceremony=%lld value=%lld min=0 max=%d",
                      (long long)ceremony, (long long)value, CEREMONY_COST_MAX);
        return 0;
    }
    *(int *)(table + at) = (int)value;
    if (also >= 0) *(int *)(table + also) = (int)value;
    return 0;
}

// ---- Content records by their .c5m words: coe5.get("monster", m, "hp") -----------------------------
static const RecordWord *recordWordRow(int record, const char *word) {
    for (size_t i = 0; i < sizeof RECORD_WORDS / sizeof RECORD_WORDS[0]; i++)
        if (RECORD_WORDS[i].record == record && !strcmp(RECORD_WORDS[i].word, word)) return &RECORD_WORDS[i];
    return NULL;
}

static long long abilityPairValue(const unsigned char *pairs, int count, int width, int id) {
    for (int i = 0; i < count; i++) {
        const unsigned char *pair = pairs + (size_t)i * 2 * width;
        long long ability = width == 8 ? *(const long long *)pair : *(const int *)pair;
        if (!ability) break;
        if (ability == id) return width == 8 ? *(const long long *)(pair + 8) : *(const int *)(pair + 4);
    }
    return 0;
}

static void pushRecordWord(lua_State *L, const char *name, int kind, const unsigned char *record, const char *word) {
    const RecordWord *row = recordWordRow(kind, word);
    long long ability = row && row->kind == WORD_ABILITY ? (long long)row->value : -1;
    if (!row && (kind == RECORD_MONSTER || kind == RECORD_ITEM || kind == RECORD_RITUAL)) ability = abilityIdOfWord(word);
    if (!row && ability < 0) { luaL_error(L, "%s: unknown word '%s'", name, word); return; }
    if (row && row->kind == WORD_FIELD) {
        const unsigned char *at = record + row->offset;
        lua_pushinteger(L, row->size == 1 ? *at : row->size == 2 ? *(const short *)at : row->size == 4 ? *(const int *)at
                                                                                                    : *(const long long *)at);
        return;
    }
    if (row && row->kind == WORD_FLAG) {
        unsigned long long bits = row->size == 4 ? *(const unsigned int *)(record + row->offset) : *(const unsigned long long *)(record + row->offset);
        lua_pushboolean(L, (bits & row->value) == row->value);
        return;
    }
    switch (kind) {
    case RECORD_MONSTER: lua_pushinteger(L, abilityPairValue(record + MONSTER_ABILITIES, MONSTER_ABILITY_PAIRS, 4, (int)ability)); break;
    case RECORD_ITEM: lua_pushinteger(L, abilityPairValue(record + ITEM_ABILITIES, ITEM_ABILITY_PAIRS, 4, (int)ability)); break;
    case RECORD_CLASS: lua_pushinteger(L, abilityPairValue(record + CLASS_ABILITIES, CLASS_ABILITY_PAIRS, 4, (int)ability)); break;
    case RECORD_TERRAIN: lua_pushinteger(L, abilityPairValue(record + TERRAIN_ABILITIES, TERRAIN_ABILITY_PAIRS, 8, (int)ability)); break;
    default: lua_pushinteger(L, ritualFirstFx(record, (int)ability, 0));
    }
}

// ---- Record writes: coe5.set("monster", m, "hp", 30) on any mod's record or the game's -----------
static int weaponSoundPairKind(const char *word) {
    return !strcmp(word, "sndvol") ? 1 : !strcmp(word, "reloadsnd") ? 6 : 0;
}

static int weaponSoundPairValue(const unsigned char *record, int kind) {
    const int *pairs = (const int *)(record + WEAPON_SOUND_PAIRS);
    for (int i = 0; i < WEAPON_SOUND_PAIRS_MAX && pairs[2 * i]; i++)
        if (pairs[2 * i] == kind) return pairs[2 * i + 1];
    return 0;
}

static int weaponRuleOfWord(const char *word) {
    for (int rule = 0; rule < 3; rule++)
        if (!strcmp(word, WEAPON_RULE_WORDS[rule])) return rule;
    return -1;
}

static void pushWeaponRuleTraits(lua_State *L, int weapon, int rule) {
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    int first = 1;
    for (int i = 0; i < weaponTraits.count; i++) {
        const WeaponTraitRule *r = &weaponTraits.rules[i];
        if (r->weapon != weapon || r->rule != rule) continue;
        if (!first) luaL_addchar(&b, ' ');
        luaL_addstring(&b, r->word);
        first = 0;
    }
    luaL_pushresult(&b);
}

static const char *setWeaponRuleTraits(int weapon, int rule, const char *traits, char *bad, size_t badSize) {
    const char *p = traits;
    char trait[WEAPON_TRAIT_WORD_SIZE];
    int kind;
    unsigned long long value;
    for (int len; (len = readWord(&p, trait, sizeof trait)) > 0;) {
        if ((size_t)len >= sizeof trait || !findWeaponTrait(trait, &kind, &value)) {
            snprintf(bad, badSize, "%.40s", trait);
            return "unknown_trait";
        }
    }
    int kept = 0;
    for (int i = 0; i < weaponTraits.count; i++)
        if (weaponTraits.rules[i].weapon != weapon || weaponTraits.rules[i].rule != rule) weaponTraits.rules[kept++] = weaponTraits.rules[i];
    weaponTraits.count = kept;
    p = traits;
    while (readWord(&p, trait, sizeof trait) > 0) {
        findWeaponTrait(trait, &kind, &value);
        if (!addWeaponTraitRule(weapon, rule, kind, value, trait)) return "out_of_memory";
    }
    return NULL;
}

static int lastWeaponNumber(void) { return tableRecords(TABLE_WEAPON) - 1; }
static int lastTerrainCode(void) { return TERRAIN_TABLE_MAX - 1; }

#define ITEM_TYPE_LAST 8
static void recordWordRange(int kind, const RecordWord *row, lua_Integer *min, lua_Integer *max) {
    *min = row->size == 1 ? SCHAR_MIN : row->size == 2 ? SHRT_MIN : row->size == 4 ? INT_MIN : LUA_MININTEGER;
    *max = row->size == 1 ? UCHAR_MAX : row->size == 2 ? USHRT_MAX : row->size == 4 ? UINT_MAX : LUA_MAXINTEGER;
    if (kind == RECORD_ITEM && !strcmp(row->word, "itemwep")) *min = -1, *max = lastWeaponNumber();
    else if (kind == RECORD_WEAPON && !strcmp(row->word, "nextalwayswep")) *min = -(lastWeaponNumber() + 1), *max = lastWeaponNumber() + 1;
    else if (kind == RECORD_ITEM && !strcmp(row->word, "type")) *min = 0, *max = ITEM_TYPE_LAST;
    else if ((kind == RECORD_CLASS && !strcmp(row->word, "hometerr")) || (kind == RECORD_RITUAL && !strcmp(row->word, "terr"))) *max = lastTerrainCode();
}

typedef struct { int offset, count, stride, valueAt, valueWidth, idWidth; } AbilityPairs;
static AbilityPairs abilityPairsOf(int kind) {
    switch (kind) {
    case RECORD_MONSTER: return (AbilityPairs){MONSTER_ABILITIES, MONSTER_ABILITY_PAIRS, 8, 4, 4, 4};
    case RECORD_ITEM: return (AbilityPairs){ITEM_ABILITIES, ITEM_ABILITY_PAIRS, 8, 4, 4, 4};
    case RECORD_CLASS: return (AbilityPairs){CLASS_ABILITIES, CLASS_ABILITY_PAIRS, 8, 4, 4, 4};
    case RECORD_TERRAIN: return (AbilityPairs){TERRAIN_ABILITIES, TERRAIN_ABILITY_PAIRS, 16, 8, 8, 8};
    default: return (AbilityPairs){RITUAL_FX, RITUAL_FX_MAX, RITUAL_FX_SIZE, RITUAL_FX_VALUE, 8, 4};
    }
}

static const char *setRecordAbility(int kind, int index, long long id, lua_Integer value) {
    AbilityPairs a = abilityPairsOf(kind);
    if (a.valueWidth == 4 && (value < INT_MIN || value > INT_MAX)) return "out_of_range";
    const unsigned char *record = contentRecord(kind, index);
    if (!record) return "bad_record";
    int at = -1, end = -1;
    for (int i = 0; i < a.count && at < 0 && end < 0; i++) {
        const unsigned char *pair = record + a.offset + (size_t)i * a.stride;
        long long have = a.idWidth == 8 ? *(const long long *)pair : *(const int *)pair;
        if (have < 1) end = i;
        else if (have == id) at = i;
    }
    if (at < 0 && end < 0) return "no_room";
    int slot = at >= 0 ? at : end;
    unsigned char pair[RITUAL_FX_SIZE];
    memcpy(pair, record + a.offset + (size_t)slot * a.stride, (size_t)a.stride);
    if (a.idWidth == 8) *(long long *)pair = id;
    else *(int *)pair = (int)id;
    if (a.valueWidth == 8) *(long long *)(pair + a.valueAt) = value;
    else *(int *)(pair + a.valueAt) = (int)value;
    const char *reason = writeContent(kind, index, a.offset + slot * a.stride, pair, a.stride);
    if (reason || at >= 0 || slot + 1 >= a.count) return reason;
    unsigned char ended[8] = {0};
    return writeContent(kind, index, a.offset + (slot + 1) * a.stride, ended, a.idWidth);
}

static int refuseRecordWrite(lua_State *L, const char *reason, const char *name, int index, const char *word) {
    LuaScript *s = luaScriptOf(L);
    refuseLuaCall(s ? s->mod : "?", "coe5.set", reason, "name=%s index=%d word=%s", name, index, word);
    return 0;
}

static int setRecordWord(lua_State *L, const GameVar *v, int arg) {
    int kind = v->size, index = (int)lua_tointeger(L, 2);
    const char *word = luaL_checkstring(L, arg);
    const unsigned char *record = contentRecord(kind, index);
    if (!record) return luaL_error(L, "%s: index %d out of range", v->name, index);
    if (kind == RECORD_WEAPON) {
        int rule = weaponRuleOfWord(word);
        if (rule >= 0) {
            const char *traits = luaL_checkstring(L, arg + 1);
            char bad[48] = "";
            int kind2, len;
            const char *p = traits;
            char trait[WEAPON_TRAIT_WORD_SIZE];
            unsigned long long value;
            while ((len = readWord(&p, trait, sizeof trait)) > 0)
                if ((size_t)len >= sizeof trait || !findWeaponTrait(trait, &kind2, &value)) {
                    snprintf(bad, sizeof bad, "%.40s", trait);
                    LuaScript *s = luaScriptOf(L);
                    refuseLuaCall(s ? s->mod : "?", "coe5.set", "unknown_trait", "name=weapon weapon=%d word=%s trait=%s", index, word, bad);
                    return 0;
                }
            const char *reason = writeContent(CONTENT_WEAPON_TRAITS, index, rule, traits, (int)strlen(traits) + 1);
            return reason ? refuseRecordWrite(L, reason, v->name, index, word) : 0;
        }
        if (!strcmp(word, "noexplode")) {
            if (lua_type(L, arg + 1) != LUA_TBOOLEAN) return refuseRecordWrite(L, "needs_boolean", v->name, index, word);
            unsigned char on = (unsigned char)lua_toboolean(L, arg + 1);
            const char *reason = writeContent(CONTENT_NOEXPLODE, index, 0, &on, 1);
            return reason ? refuseRecordWrite(L, reason, v->name, index, word) : 0;
        }
        int pairKind = weaponSoundPairKind(word);
        if (pairKind) {
            lua_Integer value = luaL_checkinteger(L, arg + 1);
            if (value < INT_MIN || value > INT_MAX) return refuseRecordWrite(L, "out_of_range", v->name, index, word);
            int pairs[2 * WEAPON_SOUND_PAIRS_MAX];
            memcpy(pairs, record + WEAPON_SOUND_PAIRS, sizeof pairs);
            int i = 0;
            while (i < WEAPON_SOUND_PAIRS_MAX && pairs[2 * i] && pairs[2 * i] != pairKind) i++;
            if (i == WEAPON_SOUND_PAIRS_MAX) return refuseRecordWrite(L, "no_room", v->name, index, word);
            if (!pairs[2 * i] && i + 1 < WEAPON_SOUND_PAIRS_MAX) pairs[2 * (i + 1)] = 0;
            pairs[2 * i] = pairKind;
            pairs[2 * i + 1] = (int)value;
            const char *reason = writeContent(CONTENT_WEAPON, index, WEAPON_SOUND_PAIRS, pairs, (int)sizeof pairs);
            return reason ? refuseRecordWrite(L, reason, v->name, index, word) : 0;
        }
    }
    const RecordWord *row = recordWordRow(kind, word);
    long long ability = row && row->kind == WORD_ABILITY ? (long long)row->value : -1;
    if (!row && (kind == RECORD_MONSTER || kind == RECORD_ITEM || kind == RECORD_RITUAL)) ability = abilityIdOfWord(word);
    if (!row && ability < 0) return luaL_error(L, "%s: unknown word '%s'", v->name, word);
    if (row && row->kind == WORD_FLAG) {
        if (lua_type(L, arg + 1) != LUA_TBOOLEAN) return refuseRecordWrite(L, "needs_boolean", v->name, index, word);
        unsigned long long bits = row->size == 4 ? *(const unsigned int *)(record + row->offset) : *(const unsigned long long *)(record + row->offset);
        bits = lua_toboolean(L, arg + 1) ? bits | row->value : bits & ~row->value;
        unsigned int low = (unsigned int)bits;
        const char *reason = writeContent(kind, index, row->offset, row->size == 4 ? (const void *)&low : (const void *)&bits, row->size == 4 ? 4 : 8);
        return reason ? refuseRecordWrite(L, reason, v->name, index, word) : 0;
    }
    lua_Integer value = luaL_checkinteger(L, arg + 1), min, max;
    if (row && row->kind == WORD_FIELD) {
        recordWordRange(kind, row, &min, &max);
        if (value < min || value > max) {
            LuaScript *s = luaScriptOf(L);
            refuseLuaCall(s ? s->mod : "?", "coe5.set", "out_of_range", "name=%s index=%d word=%s value=%lld min=%lld max=%lld", v->name, index, word,
                          (long long)value, (long long)min, (long long)max);
            return 0;
        }
        unsigned char bytes[8];
        if (row->size == 1) bytes[0] = (unsigned char)value;
        else if (row->size == 2) *(short *)bytes = (short)value;
        else if (row->size == 4) *(int *)bytes = (int)value;
        else *(long long *)bytes = value;
        const char *reason = writeContent(kind, index, row->offset, bytes, row->size);
        return reason ? refuseRecordWrite(L, reason, v->name, index, word) : 0;
    }
    const char *reason = setRecordAbility(kind, index, ability, value);
    return reason ? refuseRecordWrite(L, reason, v->name, index, word) : 0;
}

static int lGet(lua_State *L) {
    const GameVar *v;
    int arg = 2;
    unsigned char *p = gameVarAddress(L, &v, &arg, 0);
    if (!p) { lua_pushnil(L); return 1; }
    switch (v->type) {
    case T_ARMY_OWNER: lua_pushinteger(L, armyOwnerAt((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3))); break;
    case T_RITUAL_SUMMONS: {
        int summons = 0;
        for (int i = 0; i < RITUAL_FX_MAX; i++) summons |= *(const int *)(p + (size_t)i * RITUAL_FX_SIZE) == RITUAL_FX_SUMMONING;
        lua_pushboolean(L, summons);
        break;
    }
    case T_I64: lua_pushinteger(L, (lua_Integer)*(const long long *)p); break;
    case T_TICKS_MS: lua_pushinteger(L, GAME_FN(int (*)(void), RVA_TICKS_MS)()); break;
    case T_EFFECT_NAME: {
        const char *word = ritualEffectWord((int)lua_tointeger(L, 2));
        if (word) lua_pushstring(L, word);
        else lua_pushnil(L);
        break;
    }
    case T_RITUAL_TERRAIN_NAME: pushGameTerrainName(L, *(const int *)p); break;
    case T_MONSTER_MOD: {
        const char *mod = monsterMadeByName((int)lua_tointeger(L, 2));
        if (mod) lua_pushstring(L, mod);
        else lua_pushnil(L);
        break;
    }
    case T_RITUAL_TERRAIN_AT: {
        int at = ritualTerrainAt(*(const int *)p, (int)lua_tointeger(L, 3), (int)lua_tointeger(L, 4));
        if (at < 0) lua_pushnil(L);
        else lua_pushboolean(L, at);
        break;
    }
    case T_RITUAL_EFFECT_MET: {
        int i = (int)lua_tointeger(L, 3);
        int met = ritualEffectMetAt(p - RITUAL_FX - (size_t)i * RITUAL_FX_SIZE, i, (int)lua_tointeger(L, 4));
        if (met < 0) lua_pushnil(L);
        else lua_pushboolean(L, met);
        break;
    }
    case T_RITUAL_EFFECT_TERRAIN_NAME: {
        int code;
        if (ritualNearbyRule(p, &code) < 0) lua_pushnil(L);
        else pushGameTerrainName(L, code);
        break;
    }
    case T_CAST_COUNT: lua_pushinteger(L, *(const short *)p); break;
    case T_RITUAL_CASTER_MAX_CAST: lua_pushinteger(L, ritualCasterMaxCast((int)lua_tointeger(L, 2))); break;
    case T_UNIT_RITUAL_CASTS: lua_pushinteger(L, unitRitualCasts((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3))); break;
    case T_UNIT_ITEM: lua_pushinteger(L, unitItemAt((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3))); break;
    case T_UNIT_AP_LEFT: lua_pushinteger(L, unitApLeft((int)lua_tointeger(L, 2))); break;
    case T_UNIT_STATIONARY: lua_pushboolean(L, unitStationary((int)lua_tointeger(L, 2))); break;
    case T_UNIT_BATTLE_X:
    case T_UNIT_BATTLE_Y: {
        int x, y;
        if (!unitBattleCell((int)lua_tointeger(L, 2), &x, &y)) lua_pushnil(L);
        else lua_pushinteger(L, v->type == T_UNIT_BATTLE_X ? x : y);
        break;
    }
    case T_COMMANDER_COST: {
        int have;
        if (commanderCostAvailable((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3), &have) < 0) lua_pushnil(L);
        else lua_pushinteger(L, have);
        break;
    }
    case T_SLOT_INCOME: lua_pushinteger(L, slotIncome((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3))); break;
    case T_SLOT_AI_INCOME: lua_pushinteger(L, slotIncomeAs((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3), 1)); break;
    case T_SLOT_AI_TRADE: lua_pushinteger(L, slotAiTrade((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3))); break;
    case T_TERRAIN_INCOME: {
        int field = terrainIncomeField((int)lua_tointeger(L, 3));
        if (field < 0) lua_pushnil(L);
        else lua_pushinteger(L, *(const short *)(p + field));
        break;
    }
    case T_UNIT_ABILITY: lua_pushinteger(L, unitAbilityTotal((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3), 0)); break;
    case T_MONSTER_ABILITY: lua_pushinteger(L, monsterAbility((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3))); break;
    case T_CLASS_STDTROOPS:
        lua_pushboolean(L, GAME_FN(int (*)(int, int, int), RVA_CLASS_ABILITY)((int)lua_tointeger(L, 2), CLASS_ABILITY_STDTROOPS, 0) > 0);
        break;
    case T_CLASS_ABILITY:
        lua_pushinteger(L, GAME_FN(int (*)(int, int, int), RVA_CLASS_ABILITY)((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3), 0));
        break;
    case T_U8: lua_pushinteger(L, *p); break;
    case T_I8: lua_pushinteger(L, *(const signed char *)p); break;
    case T_PLANE_AT:
        lua_pushinteger(L, GAME_FN(int (*)(int, int), RVA_PLANE_AT)((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3)));
        break;
    case T_COMMANDER_STRENGTH: {
        int unit = *(const int *)p;
        if (unit < 0 || unit >= UNIT_TABLE_MAX || G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE) <= 0) lua_pushnil(L);
        else lua_pushinteger(L, commanderStrength((int)lua_tointeger(L, 2)));
        break;
    }
    case T_SQUARE_STRENGTH:
        lua_pushinteger(L, squareStrength((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3), (int)lua_tointeger(L, 4)));
        break;
    case T_GARRISON_LEFT: {
        int left = commanderGarrisonLeft((int)lua_tointeger(L, 2));
        if (left < 0) lua_pushnil(L);
        else lua_pushinteger(L, left);
        break;
    }
    case T_ARMY_STATIONARY: lua_pushboolean(L, armyStationaryAt((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3)) != 0); break;
    case T_ARMY_HOLDS: lua_pushboolean(L, armyHoldsAt((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3)) != 0); break;
    case T_UNIT_STRENGTH: lua_pushinteger(L, unitStrengthFor((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3))); break;
    case T_UNIT_VISIBLE: lua_pushboolean(L, unitVisibleToNow((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3))); break;
    case T_UNIT_ARMOR: {
        int armor = unitArmorNow((int)lua_tointeger(L, 2));
        if (armor < 0) lua_pushnil(L);
        else lua_pushinteger(L, armor);
        break;
    }
    case T_MONSTER_ATTACK: {
        int row = (int)lua_tointeger(L, 3), field = (int)(v->addr - (RVA_MONSTER_TABLE + MONSTER_ATTACKS)), value;
        if (monsterAttackField(p - field - (size_t)row * MONSTER_ATTACK_SIZE, row, field, &value) < 0) lua_pushnil(L);
        else lua_pushinteger(L, value);
        break;
    }
    case T_SQUARE_CITADEL:
        lua_pushboolean(L, squareCitadelFor((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3), (int)lua_tointeger(L, 4)) != 0);
        break;
    case T_REMEMBERED_STRENGTH:
        lua_pushinteger(L, rememberedStrength((const int *)p, (int)lua_tointeger(L, 3), (int)lua_tointeger(L, 4), squareStrength));
        break;
    case T_WINDOWS_ONLY: lua_pushnil(L); break;
    case T_GAME_CHANGED: lua_pushnil(L); break;
    case T_CEREMONY_NAME: {
        unsigned int rva = ceremonyNameRva((int)lua_tointeger(L, 2));
        if (!rva) lua_pushnil(L);
        else lua_pushlstring(L, (const char *)GAME_PTR(rva), strnlen((const char *)GAME_PTR(rva), NAME_SIZE));
        break;
    }
    case T_CEREMONY_COST: {
        int also, at = ceremonyCostAt((int)lua_tointeger(L, 2), &also);
        if (at < 0) lua_pushnil(L);
        else lua_pushinteger(L, *(const int *)(p + at));
        break;
    }
    case T_CEREMONY_LAST: lua_pushinteger(L, CEREMONY_LAST); break;
    case T_FORT_PART_WORD: {
        const char *word = fortPartWord((int)lua_tointeger(L, 2));
        if (!word) lua_pushnil(L);
        else lua_pushstring(L, word);
        break;
    }
    case T_UNIT_MOVE_FLAGS:
    case T_UNIT_FLAG_WORDS: {
        unsigned long long flags;
        if (unitFlagsOf((int)lua_tointeger(L, 2), v->type == T_UNIT_FLAG_WORDS, &flags) < 0) lua_pushnil(L);
        else lua_pushinteger(L, (lua_Integer)flags);
        break;
    }
    case T_COMMANDER_MOVE_FLAGS:
    case T_COMMANDER_RITPOW:
    case T_COMMANDER_RITUAL_LEVEL:
    case T_COMMANDER_SPELL_LEVEL: {
        int commander = (int)lua_tointeger(L, 2), unit = livingCommanderUnit(commander), level;
        if (unit < 0) lua_pushnil(L);
        else if (v->type == T_COMMANDER_MOVE_FLAGS) lua_pushinteger(L, (lua_Integer)commanderMoveFlags(commander));
        else if (v->type == T_COMMANDER_SPELL_LEVEL) lua_pushinteger(L, GAME_FN(int (*)(int, int), RVA_UNIT_PATH_LEVEL)(unit, -1));
        else {
            int school = commanderRitpowOf(commander, &level);
            lua_pushinteger(L, v->type == T_COMMANDER_RITPOW ? school : level);
        }
        break;
    }
    case T_SQUARE_LIBRARY_LEVEL: lua_pushinteger(L, squareLibraryLevel((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3))); break;
    case T_SQUARE_HAS_FORT: lua_pushboolean(L, squareHasFort((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3))); break;
    case T_SQUARE_WATERLIKE: lua_pushboolean(L, squareWaterlike((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3))); break;
    case T_SQUARE_OPEN_LAND: lua_pushboolean(L, squareOpenLand((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3))); break;
    case T_KEY_HELD: lua_pushboolean(L, keyCodeHeld((int)lua_tointeger(L, 2))); break;
    case T_PREVIEW_WHEEL:
        if (previewWorldOn()) lua_pushinteger(L, G32(RVA_WHEEL));
        else lua_pushnil(L);
        break;
    case T_SLOT_CITADELS: lua_pushinteger(L, GAME_FN(int (*)(int), RVA_AI_COUNT_CITADELS)((int)lua_tointeger(L, 2))); break;
    case T_RECORD_WORD: {
        const char *word = luaL_checkstring(L, arg);
        int weapon = (int)lua_tointeger(L, 2);
        if (v->size == RECORD_WEAPON && weaponSoundPairKind(word)) lua_pushinteger(L, weaponSoundPairValue(p, weaponSoundPairKind(word)));
        else if (v->size == RECORD_WEAPON && weaponRuleOfWord(word) >= 0) pushWeaponRuleTraits(L, weapon, weaponRuleOfWord(word));
        else if (v->size == RECORD_WEAPON && !strcmp(word, "noexplode"))
            lua_pushboolean(L, weapon >= 0 && weapon < noExplode.weaponCount && noExplode.weapons[weapon]);
        else pushRecordWord(L, v->name, v->size, p, word);
        break;
    }
    case T_AI_ATTACK_MARGIN: lua_pushinteger(L, aiAttackMarginNow); break;
    case T_SLOT_ABILITY:
        lua_pushinteger(L, GAME_FN(int (*)(int, int, int), RVA_PLAYER_ABILITY)((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3), v->size));
        break;
    case T_UNIT_STATE_FLAGS: {
        unsigned long long flags = 0;
        if (!unitStateFlagsNow((int)lua_tointeger(L, 2), &flags)) lua_pushnil(L);
        else lua_pushinteger(L, (lua_Integer)flags);
        break;
    }
    case T_UNIT_RETURNS: {
        int returns = unitReturnsNow((int)lua_tointeger(L, 2));
        if (returns < 0) lua_pushnil(L);
        else lua_pushboolean(L, returns);
        break;
    }
    case T_I16: lua_pushinteger(L, *(short *)p); break;
    case T_FLAG_U8: lua_pushboolean(L, *p != 0); break;
    case T_FLAG_POSITIVE: lua_pushboolean(L, *(int *)p > 0); break;
    case T_FLAG_I32: lua_pushboolean(L, *(int *)p != 0); break;
    case T_FLAG_I16: lua_pushboolean(L, *(short *)p != 0); break;
    case T_FLAG_TWO: lua_pushboolean(L, *(int *)p == 2); break;
    case T_F32: lua_pushnumber(L, *(float *)p); break;
    case T_STR: case T_FRAME_STR: lua_pushlstring(L, (const char *)p, strnlen((const char *)p, v->size)); break;
    case T_PTR_STR: {
        const char *s = *(const char *const *)p;
        if (s) lua_pushlstring(L, s, strnlen(s, v->size));
        else lua_pushnil(L);
        break;
    }
    case T_PLANE_TINT: {
        float rgba[PLANE_TINT_CHANNELS];
        memcpy(rgba, p + PLANE_TINT, sizeof rgba);
        for (int c = 0; c < PLANE_TINT_CHANNELS; c++) lua_pushnumber(L, c == PLANE_TINT_CHANNELS - 1 && !p[PLANE_TINT_ON] ? 0.0 : rgba[c]);
        return PLANE_TINT_CHANNELS;
    }
    default: lua_pushinteger(L, *(int *)p);
    }
    return 1;
}

#define GAME_LAST_TURN 250
#define BATTLE_WIDTH_FIRST 28
#define BATTLE_WIDTH_MAX 0x600
#define SAMPLE_SLOT_LAST 248
#define MAP_ZOOM_MIN 8.0
#define MAP_ZOOM_MAX 100.0
#define MESSAGE_RECORDS 1000
#define PATH_ENTRIES 256
#define MONSTER_SPRITE_END 0x7fbc
#define RITUAL_COST_TYPE_LAST 0x32

static const struct { const char *name; lua_Integer min, max; } GAME_VAR_RANGES[] = {
    {"mapW", INT_MIN, MAP_COLUMNS_MAX}, {"mapH", INT_MIN, MAP_COLUMN_SQUARES}, {"mapDepth", INT_MIN, MAP_COLUMNS_MAX},
    {"cliMapW", INT_MIN, MAP_COLUMNS_MAX}, {"cliMapH", INT_MIN, MAP_COLUMN_SQUARES},
    {"society", 0, SOCIETY_TABLE_MAX - 1},
    {"slotClass", INT_MIN, LAST_MOD_CLASS_SLOT},
    {"slotAi", INT_MIN, AI_LEVEL_TABLE_MAX - 1},
    {"startDefault", 0, 2}, {"northPart", -1, 100}, {"southPart", -1, 100}, {"wilder", 0, 2},
    {"mouseButtons", 0, 7},
    {"fullscreen", 0, 2}, {"battleGrid", 0, 1},
    {"aiMoveDelay", 0, 10000},
    {"traceLevel", 0, 3},
    {"slotTradeType", 0, 24},
    {"squareCorpses", 0, SHRT_MAX},
    {"unitLastIndex", -1, UNIT_TABLE_MAX - 1}, {"unitLastSpawned", -1, UNIT_TABLE_MAX - 1},
    {"commanderLastIndex", -1, COMMANDER_TABLE_MAX - 1}, {"commanderLastSpawned", -1, COMMANDER_TABLE_MAX - 1},
    {"selectedCommander", -1, COMMANDER_TABLE_MAX - 1},
    {"unitCommander", -1, COMMANDER_TABLE_MAX - 1}, {"slotLeader", -1, COMMANDER_TABLE_MAX - 1},
    {"commanderUnit", -1, UNIT_TABLE_MAX - 1},
    {"unitX", -1, MAP_COLUMNS_MAX - 1}, {"unitY", -1, MAP_COLUMN_SQUARES - 1},
    {"slotHomeX", -1, MAP_COLUMNS_MAX - 1}, {"slotHomeY", -1, MAP_COLUMN_SQUARES - 1},
    {"commanderAiTargetX", -1, MAP_COLUMNS_MAX - 1}, {"commanderAiTargetY", -1, MAP_COLUMN_SQUARES - 1},
    {"recruitLocationLast", -1, 255}, {"recruitLocationX", 0, MAP_COLUMNS_MAX - 1}, {"recruitLocationY", 0, MAP_COLUMN_SQUARES - 1},
    {"unitOwner", -1, INT_MAX}, {"activePlayer", -1, INT_MAX}, {"rulershipSlot", -1, INT_MAX}, {"unitMonster", -1, INT_MAX},
    {"commanderRitual", 0, INT_MAX}, {"commanderRepeatRitual", -1, INT_MAX}, {"commanderRitualPlan", SHRT_MIN, INT_MAX},
    {"unitKnownSpell", -1, INT_MAX}, {"unitMemorizedSpell", -1, INT_MAX},
    {"unitTempAbilityId", -1, RITUAL_EFFECT_ID_COUNT - 1}, {"slotBlessingId", 0, RITUAL_EFFECT_ID_COUNT - 1},
    {"aiAttackDivOther", 1, INT_MAX}, {"aiAttackDivAi", 1, INT_MAX}, {"aiAttackDivHuman", 1, INT_MAX},
    {"turn", -1, GAME_LAST_TURN},
    {"battleX", -1, MAP_COLUMNS_MAX - 1}, {"battleY", -1, MAP_COLUMN_SQUARES - 1},
    {"mapPointerX", -1, MAP_COLUMNS_MAX - 1}, {"mapPointerY", -1, MAP_COLUMN_SQUARES - 1},
    {"battleAttacker", -1, PLAYER_SLOTS_MOVED - 1}, {"battleDefender", -1, PLAYER_SLOTS_MOVED - 1},
    {"battleWinner", -1, PLAYER_SLOTS_MOVED - 1}, {"loneHuman", -1, PLAYER_SLOTS_MOVED - 1},
    {"gameWinner", -1, PLAYER_SLOTS_MOVED - 1},
    {"battleWidth", BATTLE_WIDTH_FIRST, BATTLE_WIDTH_MAX},
    {"sampleHighest", -1, SAMPLE_SLOT_LAST},
    {"autoNameLast", -1, AUTO_NAME_LIST_SIZE - 1},
    {"slotLockClass", INT_MIN, LAST_MOD_CLASS_SLOT}, {"slotLockAi", INT_MIN, AI_LEVEL_TABLE_MAX - 1},
    {"mapViewPlane", 0, INT_MAX},
    {"squareOwner", -1, INT_MAX},
    {"squareItem", -1, INT_MAX},
    {"planeX", 0, MAP_COLUMNS_MAX}, {"planeY", 0, MAP_COLUMN_SQUARES}, {"planeWidth", 0, MAP_COLUMNS_MAX},
    {"planeHeight", 0, MAP_COLUMN_SQUARES}, {"worldW", 0, MAP_COLUMNS_MAX}, {"worldH", 0, MAP_COLUMN_SQUARES},
    {"planeLastIndex", -1, INT_MAX}, {"mapPlaneIndex", -1, INT_MAX},
    {"mapFilePlane", 0, 9},
    {"worldNorthPart", 0, 100}, {"worldSouthPart", 0, 100},
    {"season", 0, 3},
    {"messageLastIndex", -1, MESSAGE_RECORDS - 1},
    {"enchantLastIndex", -1, ENCHANTS_MAX - 1}, {"enchantCaster", -1, UNIT_TABLE_MAX - 1}, {"enchantOwner", -1, INT_MAX},
    {"enchantX", INT_MIN, MAP_COLUMNS_MAX - 1}, {"enchantY", INT_MIN, MAP_COLUMN_SQUARES - 1},
    {"eventTargetX", INT_MIN, MAP_COLUMNS_MAX - 1}, {"eventTargetY", INT_MIN, MAP_COLUMN_SQUARES - 1},
    {"eventSquareX", INT_MIN, MAP_COLUMNS_MAX - 1}, {"eventSquareY", INT_MIN, MAP_COLUMN_SQUARES - 1},
    {"pathLength", 0, PATH_ENTRIES}, {"pathX", 0, MAP_COLUMNS_MAX - 1}, {"pathY", 0, MAP_COLUMN_SQUARES - 1},
    {"squareArmyOwner", -1, INT_MAX}, {"squareRememberedStrength", -1, INT_MAX},
    {"slotRitualCasts", 0, COMMANDER_CAST_MAX}, {"unitRitualCasts", 0, COMMANDER_CAST_MAX},
    {"unitItem", -1, INT_MAX}, {"unitBattleX", 0, BATTLE_WIDTH_MAX - 1}, {"unitBattleY", 0, BATTLE_ROWS - 1}, {"uiWheel", INT_MIN, INT_MAX},
    {"monsterSprite", 0, MONSTER_SPRITE_END - 1}, {"itemSlotType", 0, ITEM_TYPE_LAST}, {"ritualTerrain", INT_MIN, TERRAIN_TABLE_MAX - 1},
    {"ritualCostType", INT_MIN, RITUAL_COST_TYPE_LAST}, {"ritualEffect", INT_MIN, RITUAL_EFFECT_ID_COUNT - 1},
    {"monsterAttackWeapon", -1, INT_MAX},
};

static const struct { const char *name; double min, max; } GAME_VAR_FLOAT_RANGES[] = {
    {"mapZoom", MAP_ZOOM_MIN, MAP_ZOOM_MAX},
    {"mapCameraX", 0, MAP_COLUMNS_MAX}, {"mapCameraY", 0, MAP_COLUMN_SQUARES},
};

static int gameVarFloatRange(const GameVar *v, double *min, double *max) {
    for (size_t i = 0; i < sizeof GAME_VAR_FLOAT_RANGES / sizeof GAME_VAR_FLOAT_RANGES[0]; i++) {
        if (strcmp(GAME_VAR_FLOAT_RANGES[i].name, v->name)) continue;
        *min = GAME_VAR_FLOAT_RANGES[i].min, *max = GAME_VAR_FLOAT_RANGES[i].max;
        return 1;
    }
    if (!strcmp(v->name, "uiPointerX")) *min = 0, *max = G32(RVA_SCREEN_W);
    else if (!strcmp(v->name, "uiPointerY")) *min = 0, *max = G32(RVA_SCREEN_H);
    else return 0;
    return 1;
}

#define SLOT_COLOUR_MIN -8421505.0f
#define SLOT_COLOUR_MAX 8421504.0f

static int isSlotColour(const GameVar *v) {
    return v->type == T_F32 && v->addr >= RVA_SLOT_COLOUR && v->addr <= RVA_SLOT_COLOUR + 8;
}

static int lastPlayerSlot(void) { return playerSlotCount() - 1; }
static int lastMonsterNumber(void) { return tableRecords(TABLE_MONSTER) - 1; }
static int lastItemNumber(void) { return tableRecords(TABLE_ITEM) - 1; }
static int lastClassNumber(void) { return tableRecords(TABLE_CLASS) - 1; }
static int lastPlaneRecord(void) { return planeTableRecords() - 1; }
static int lastRitualSlot(void) { return ritualSlotCount() - 1; }
static int lastBattleColumn(void) { return G32(RVA_BATTLE_WIDTH) - 1; }
static int lastSpellSlot(void) { return spellTableRecords() - 2; }
static int lastPlaneInUse(void) {
    int last = G32(RVA_LAST_PLANE);
    return last < lastPlaneRecord() ? last : lastPlaneRecord();
}
static const struct { const char *name; int (*last)(void); } GAME_VAR_LAST[] = {
    {"activePlayer", lastPlayerSlot}, {"battleAttacker", lastPlayerSlot}, {"battleDefender", lastPlayerSlot},
    {"battleWinner", lastPlayerSlot}, {"enchantOwner", lastPlayerSlot}, {"eventPlayer", lastPlayerSlot},
    {"gameWinner", lastPlayerSlot}, {"loneHuman", lastPlayerSlot}, {"rulershipSlot", lastPlayerSlot},
    {"squareOwner", lastPlayerSlot}, {"unitOwner", lastPlayerSlot},
    {"unitMonster", lastMonsterNumber}, {"squareItem", lastItemNumber},
    {"slotClass", lastClassNumber}, {"slotLockClass", lastClassNumber},
    {"planeLastIndex", lastPlaneRecord}, {"mapPlaneIndex", lastPlaneRecord}, {"mapViewPlane", lastPlaneInUse},
    {"commanderRitual", lastRitualSlot}, {"commanderRepeatRitual", lastRitualSlot}, {"commanderRitualPlan", lastRitualSlot},
    {"enchantRitual", lastRitualSlot}, {"unitKnownSpell", lastSpellSlot}, {"unitMemorizedSpell", lastSpellSlot},
    {"squareArmyOwner", lastPlayerSlot}, {"unitItem", lastItemNumber}, {"unitBattleX", lastBattleColumn},
    {"monsterAttackWeapon", lastWeaponNumber},
};

static void gameVarRange(const GameVar *v, lua_Integer *min, lua_Integer *max) {
    if (v->type == T_U8) *min = SCHAR_MIN, *max = UCHAR_MAX;
    else if (v->type == T_I8) *min = SCHAR_MIN, *max = SCHAR_MAX;
    else if (v->type == T_I16) *min = SHRT_MIN, *max = USHRT_MAX;
    else *min = INT_MIN, *max = UINT_MAX;
    for (size_t i = 0; i < sizeof GAME_VAR_RANGES / sizeof GAME_VAR_RANGES[0]; i++) {
        if (strcmp(GAME_VAR_RANGES[i].name, v->name)) continue;
        if (GAME_VAR_RANGES[i].min > *min) *min = GAME_VAR_RANGES[i].min;
        if (GAME_VAR_RANGES[i].max < *max) *max = GAME_VAR_RANGES[i].max;
    }
    for (size_t i = 0; i < sizeof GAME_VAR_LAST / sizeof GAME_VAR_LAST[0]; i++)
        if (!strcmp(GAME_VAR_LAST[i].name, v->name) && GAME_VAR_LAST[i].last() < *max) *max = GAME_VAR_LAST[i].last();
}

static void flushCommanderCache(int commander) {
    if (commander < 0 || commander >= COMMANDER_TABLE_MAX) return;
    int unit = G32(RVA_COMMANDERS + (size_t)commander * COMMANDER_SIZE);
    if (unit >= 0 && unit < UNIT_TABLE_MAX) {
        int leader = G32(RVA_UNITS + (size_t)unit * UNIT_SIZE + UNIT_COMMANDER);
        if (leader >= 0 && leader < COMMANDER_TABLE_MAX) {
            *(signed char *)GAME_PTR(RVA_COMMANDERS + (size_t)leader * COMMANDER_SIZE + COMMANDER_AP_CACHE) = -99;
            G32(RVA_COMMANDER_CACHE_INTS + (size_t)leader * 4) = -1;
        }
    }
    *(signed char *)GAME_PTR(RVA_COMMANDERS + (size_t)commander * COMMANDER_SIZE + COMMANDER_AP_CACHE) = -99;
    G32(RVA_COMMANDER_CACHE_INTS + (size_t)commander * 4) = -1;
}

static void flushArmyApCaches(int unit) {
    int last = G32(RVA_LAST_COMMANDER);
    for (int c = 0; c <= last && c < COMMANDER_TABLE_MAX; c++) {
        if (G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE) == unit) {
            flushCommanderCache(c);
            break;
        }
    }
    flushCommanderCache(G32(RVA_UNITS + (size_t)unit * UNIT_SIZE + UNIT_COMMANDER));
}

typedef struct { int x, y, w, h; } SquareRect;
static int worldBuildingDepth;
static SquareRect worldBuildingArea;

static __attribute__((unused)) SquareRect startWorldBuilding(SquareRect area) {
    SquareRect previous = worldBuildingArea;
    worldBuildingArea = area;
    worldBuildingDepth++;
    return previous;
}

static __attribute__((unused)) void stopWorldBuilding(SquareRect previous) {
    worldBuildingArea = previous;
    if (worldBuildingDepth > 0) worldBuildingDepth--;
}

static int worldBuildingAreaNow(SquareRect *area) {
    if (worldBuildingDepth > 0) {
        *area = worldBuildingArea;
        return 1;
    }
    if (!luaSiteDepth[SITE_PLAY_GAME_NEW_WORLD]) return 0;
    int w = G32(RVA_MAP_W), h = G32(RVA_MAP_H);
    *area = (SquareRect){0, 0, w < MAP_COLUMNS_MAX ? w : MAP_COLUMNS_MAX, h < MAP_COLUMN_SQUARES ? h : MAP_COLUMN_SQUARES};
    return 1;
}

static const char *squareWriteRefusal(int building, SquareRect area, lua_Integer x, lua_Integer y) {
    if (!building) return "not_world_building";
    if (x < area.x || y < area.y || x >= (lua_Integer)area.x + area.w || y >= (lua_Integer)area.y + area.h) return "outside_plane";
    return NULL;
}

static void squareValueRange(const char *name, int lastTerrain, lua_Integer *min, lua_Integer *max) {
    *min = SHRT_MIN, *max = SHRT_MAX;
    if (!strcmp(name, "squareTerrain")) *min = 0, *max = lastTerrain;
    else if (!strcmp(name, "squareGround")) *min = 0, *max = SQUARE_GROUND_LAST;
}

static int setSquareValue(lua_State *L, const GameVar *v, unsigned char *p, int arg) {
    LuaScript *s = luaScriptOf(L);
    const char *mod = s ? s->mod : "?";
    lua_Integer x = lua_tointeger(L, 2), y = lua_tointeger(L, 3);
    SquareRect area = {0, 0, 0, 0};
    int building = worldBuildingAreaNow(&area);
    const char *reason = squareWriteRefusal(building, area, x, y);
    if (reason) {
        refuseLuaCall(mod, "coe5.set", reason, "name=%s x=%lld y=%lld", v->name, (long long)x, (long long)y);
        return 0;
    }
    if (v->type == T_STR) {
        snprintf((char *)p, v->size, "%s", luaL_checkstring(L, arg));
        return 0;
    }
    lua_Integer value = luaL_checkinteger(L, arg), min, max;
    squareValueRange(v->name, G32(RVA_LAST_TERRAIN), &min, &max);
    if (value < min || value > max) {
        refuseLuaCall(mod, "coe5.set", "out_of_range", "name=%s value=%lld min=%lld max=%lld", v->name, (long long)value,
                      (long long)min, (long long)max);
        return 0;
    }
    if (!strcmp(v->name, "squareTerrain")) GAME_FN(void (*)(int, int, int), RVA_CHANGE_SQUARE_TERRAIN)((int)x, (int)y, (int)value);
    else *(short *)p = (short)value;
    return 0;
}

static int listEndMark(const char *name, short *mark) {
    if (!strcmp(name, "unitKnownSpell") || !strcmp(name, "unitMemorizedSpell")) { *mark = -1; return 1; }
    if (!strcmp(name, "commanderRitual")) { *mark = 0; return 1; }
    return 0;
}

static void narrowPlaneRange(const GameVar *v, const unsigned char *p, lua_Integer *max) {
    int across = !strcmp(v->name, "planeX") || !strcmp(v->name, "planeWidth") || !strcmp(v->name, "worldW");
    int down = !strcmp(v->name, "planeY") || !strcmp(v->name, "planeHeight") || !strcmp(v->name, "worldH");
    if (!across && !down) return;
    ptrdiff_t start = across ? PLANE_X : PLANE_Y, size = across ? PLANE_W : PLANE_H;
    ptrdiff_t here = !strcmp(v->name, "planeX") || !strcmp(v->name, "planeY") ? start : size;
    int other = *(const int *)(p + (here == start ? size : start) - here);
    lua_Integer limit = (across ? MAP_COLUMNS_MAX : MAP_COLUMN_SQUARES) - (lua_Integer)other;
    if (limit < *max) *max = limit;
}

static int refuseSetValue(lua_State *L, const char *reason, const GameVar *v, lua_Integer value, lua_Integer min, lua_Integer max) {
    LuaScript *s = luaScriptOf(L);
    refuseLuaCall(s ? s->mod : "?", "coe5.set", reason, "name=%s value=%lld min=%lld max=%lld", v->name, (long long)value, (long long)min,
                  (long long)max);
    return 0;
}

static int refusedSetValue(lua_State *L, const char *reason, const GameVar *v, lua_Integer value, lua_Integer min, lua_Integer max) {
    refuseSetValue(L, reason, v, value, min, max);
    return 1;
}

static int setPlaneTintValue(lua_State *L, int arg) {
    double rgba[PLANE_TINT_CHANNELS];
    for (int c = 0; c < PLANE_TINT_CHANNELS; c++) rgba[c] = luaL_checknumber(L, arg + c);
    lua_Integer plane = lua_tointeger(L, 2);
    const char *reason = setLuaPlaneTint(plane < 0 || plane > INT_MAX ? -1 : (int)plane, rgba);
    if (reason) {
        LuaScript *s = luaScriptOf(L);
        refuseLuaCall(s ? s->mod : "?", "coe5.set", reason, "name=planeTint plane=%lld r=%g g=%g b=%g strength=%g min=0 max=1", (long long)plane,
                      rgba[0], rgba[1], rgba[2], rgba[3]);
    }
    return 0;
}

#define WRITTEN_NAME_MAX 64
static struct { const char **entry; char text[NAME_SIZE]; } writtenNames[WRITTEN_NAME_MAX];
static int writtenNameCount;
static int setPointerName(lua_State *L, const GameVar *v, unsigned char *p, int arg) {
    const char *text = luaL_checkstring(L, arg);
    const char **entry = (const char **)p;
    int k = 0;
    while (k < writtenNameCount && writtenNames[k].entry != entry) k++;
    const char *reason = k == WRITTEN_NAME_MAX ? "no_room" : !makeGameDataWritable(entry, sizeof *entry) ? "table_not_writable" : NULL;
    if (reason) {
        LuaScript *s = luaScriptOf(L);
        refuseLuaCall(s ? s->mod : "?", "coe5.set", reason, "name=%s index=%lld", v->name, (long long)lua_tointeger(L, 2));
        return 0;
    }
    if (k == writtenNameCount) writtenNames[writtenNameCount++].entry = entry;
    snprintf(writtenNames[k].text, sizeof writtenNames[k].text, "%s", text);
    *entry = writtenNames[k].text;
    return 0;
}

static int setTempAbility(int unit, int ability, int value) {
    if (unit < 0 || ability < 1) return 0;
    for (int i = 0; i < UNIT_TEMP_ABILITIES; i++) {
        size_t id = RVA_UNIT_TEMP_ABILITY_IDS + (size_t)unit * UNIT_SIZE + (size_t)i * 2;
        int has = (short)G16(id);
        if (has && has != ability && has != -1) continue;
        G16(id) = (short)ability;
        G32(RVA_UNIT_TEMP_ABILITY_VALUES + (size_t)unit * UNIT_SIZE + (size_t)i * 4) = value;
        if (!has && i + 1 < UNIT_TEMP_ABILITIES) G16(id + 2) = 0;
        return 1;
    }
    return 0;
}

static const char *writeUnitAbility(int unit, int ability, int value, lua_Integer *own) {
    *own = INT_MIN;
    if (!unitAliveInTable(unit)) return "no_unit";
    if (ability < 1) return "bad_ability";
    unsigned char *ids = GAME_PTR(RVA_UNIT_TEMP_ABILITY_IDS + (size_t)unit * UNIT_SIZE);
    unsigned char *values = GAME_PTR(RVA_UNIT_TEMP_ABILITY_VALUES + (size_t)unit * UNIT_SIZE);
    short oldIds[UNIT_TEMP_ABILITIES];
    int oldValues[UNIT_TEMP_ABILITIES];
    memcpy(oldIds, ids, sizeof oldIds), memcpy(oldValues, values, sizeof oldValues);
    for (int i = 0; i < UNIT_TEMP_ABILITIES && oldIds[i]; i++)
        if (oldIds[i] == ability) {
            short freed = -1;
            int none = 0;
            memcpy(ids + (size_t)i * 2, &freed, 2), memcpy(values + (size_t)i * 4, &none, 4);
        }
    int total = unitAbilityTotal(unit, ability, 0);
    *own = total;
    if (total == value) return NULL;
    const char *reason = NULL;
    if (!setTempAbility(unit, ability, value)) reason = "no_room";
    else if ((total = unitAbilityTotal(unit, ability, 0)) != value) {
        long long lowered = (long long)value - ((long long)total - value);
        if (lowered >= INT_MIN && lowered <= INT_MAX && setTempAbility(unit, ability, (int)lowered)) total = unitAbilityTotal(unit, ability, 0);
        if (total != value) reason = "below_own";
    }
    if (reason) memcpy(ids, oldIds, sizeof oldIds), memcpy(values, oldValues, sizeof oldValues);
    return reason;
}

#define AI_ATTACK_MARGIN_MAX 127
#define MARGIN_IMM12 (0xfffu << 10)
#define MARGIN_IMM16 (0xffffu << 5)
static const char *writeAiAttackMargin(int margin) {
    if (!gameCodeMatches) return gameCodeReason();
#ifdef __APPLE__
    const struct { uintptr_t at; uint32_t mask; int shift; } words[] = {
        {RVA_AI_CONQUER_MARGIN, MARGIN_IMM12, 10},
        {RVA_WAYTOGO_MARGIN_OTHER - 0xc, MARGIN_IMM16, 5},
        {RVA_WAYTOGO_MARGIN_OTHER - 8, MARGIN_IMM12, 10}, {RVA_WAYTOGO_MARGIN_OTHER, MARGIN_IMM12, 10},
        {RVA_WAYTOGO_MARGIN_AI - 8, MARGIN_IMM12, 10},    {RVA_WAYTOGO_MARGIN_AI, MARGIN_IMM12, 10},
        {RVA_WAYTOGO_MARGIN_HUMAN - 8, MARGIN_IMM12, 10}, {RVA_WAYTOGO_MARGIN_HUMAN, MARGIN_IMM12, 10},
    };
    for (size_t i = 0; i < sizeof words / sizeof words[0]; i++) {
        uint32_t live;
        memcpy(&live, GAME_PTR(words[i].at), sizeof live);
        uint32_t word = patchedWord(live, words[i].mask, (uint32_t)margin << words[i].shift);
        if (!writeGameCode(GAME_PTR(words[i].at), &word, sizeof word)) return "code_unwritable";
    }
#else
    const struct { uintptr_t at; int sign; } bytes[] = {
        {RVA_AI_CONQUER_MARGIN, 1}, {RVA_WAYTOGO_MARGIN_OTHER, -1}, {RVA_WAYTOGO_MARGIN_AI, -1}, {RVA_WAYTOGO_MARGIN_HUMAN, -1},
    };
    for (size_t i = 0; i < sizeof bytes / sizeof bytes[0]; i++) {
        signed char immediate = (signed char)(bytes[i].sign * margin);
        if (!writeGameCode(GAME_PTR(bytes[i].at + 2), &immediate, 1)) return "code_unwritable";
    }
#endif
    aiAttackMarginNow = margin;
    return NULL;
}

static int setKeyHeld(lua_State *L, const GameVar *v, int arg) {
    LuaScript *s = luaScriptOf(L);
    lua_Integer code = lua_tointeger(L, 2);
    if (lua_type(L, arg) != LUA_TBOOLEAN) {
        refuseLuaCall(s ? s->mod : "?", "coe5.set", "needs_boolean", "name=%s value=%s", v->name, luaL_typename(L, arg));
        return 1;
    }
    if (code < 1) {
        refuseLuaCall(s ? s->mod : "?", "coe5.set", "out_of_range", "name=%s key=%lld min=1 max=%d", v->name, (long long)code, KEY_STATE_SIZE - 1);
        return 1;
    }
    unsigned char held = lua_toboolean(L, arg) ? 1 : 0;
    keysHeld[code] = held;
    *(unsigned char *)GAME_PTR(RVA_KEY_STATE + (size_t)code) = held;
    return 1;
}

static int setComputedValue(lua_State *L, const GameVar *v, unsigned char *p, int arg) {
    int integer = v->type == T_ARMY_OWNER || v->type == T_UNIT_AP_LEFT || v->type == T_UNIT_BATTLE_X || v->type == T_UNIT_BATTLE_Y ||
                  v->type == T_REMEMBERED_STRENGTH || v->type == T_CAST_COUNT || v->type == T_UNIT_RITUAL_CASTS || v->type == T_UNIT_ITEM ||
                  v->type == T_PREVIEW_WHEEL || v->type == T_UNIT_ABILITY || v->type == T_AI_ATTACK_MARGIN;
    if (v->type == T_KEY_HELD) return setKeyHeld(L, v, arg);
    if (v->type == T_ARMY_HOLDS) {
        if (lua_type(L, arg) != LUA_TBOOLEAN) {
            LuaScript *s = luaScriptOf(L);
            refuseLuaCall(s ? s->mod : "?", "coe5.set", "needs_boolean", "name=%s value=%s", v->name, luaL_typename(L, arg));
            return 1;
        }
        int x = (int)lua_tointeger(L, 2), y = (int)lua_tointeger(L, 3);
        size_t square = ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE;
        if (lua_toboolean(L, arg)) {
            G32(RVA_SQUARE_FLAGS + square) |= SQUARE_FLAG_HOLDIT;
            return 1;
        }
        G32(RVA_SQUARE_FLAGS + square) &= ~SQUARE_FLAG_HOLDIT;
        for (int u = 0, last = G32(RVA_UNIT_COUNT); u <= last && u < UNIT_TABLE_MAX; u++)
            if (unitAliveAt(u, x, y)) *(unsigned long long *)GAME_PTR(RVA_UNITS + (size_t)u * UNIT_SIZE + 8) &= ~(1ull << UNIT_FLAG2_HOLDIT);
        return 1;
    }
    if (!integer) return 0;
    lua_Integer value = luaL_checkinteger(L, arg), min, max;
    gameVarRange(v, &min, &max);
    if (v->type == T_UNIT_AP_LEFT) {
        int unit = (int)lua_tointeger(L, 2), used = (short)G16(RVA_UNITS + (size_t)unit * UNIT_SIZE + UNIT_AP_USED), most = unitApLeft(unit) + used;
        min = (lua_Integer)most - SHRT_MAX, max = (lua_Integer)most - SHRT_MIN;
        if (value < min || value > max) return refusedSetValue(L, "out_of_range", v, value, min, max);
        G16(RVA_UNITS + (size_t)unit * UNIT_SIZE + UNIT_AP_USED) = (short)(most - value);
        flushArmyApCaches(unit);
        return 1;
    }
    if (v->type == T_AI_ATTACK_MARGIN) {
        min = 0, max = AI_ATTACK_MARGIN_MAX;
        if (value < min || value > max) return refusedSetValue(L, "out_of_range", v, value, min, max);
        const char *reason = writeAiAttackMargin((int)value);
        if (reason) return refusedSetValue(L, reason, v, value, min, max);
        return 1;
    }
    if (v->type == T_UNIT_ABILITY) {
        min = INT_MIN, max = INT_MAX;
        if (value < min || value > max) return refusedSetValue(L, "out_of_range", v, value, min, max);
        const char *reason = writeUnitAbility((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3), (int)value, &min);
        if (reason) return refusedSetValue(L, reason, v, value, min, max);
        return 1;
    }
    if (value < min || value > max) return refusedSetValue(L, "out_of_range", v, value, min, max);
    switch (v->type) {
    case T_ARMY_OWNER: *(short *)p = (short)value; break;
    case T_REMEMBERED_STRENGTH: *(int *)p = (int)value; break;
    case T_CAST_COUNT: *(short *)p = (short)value; break;
    case T_PREVIEW_WHEEL: G32(RVA_WHEEL) = (int)((unsigned)G32(RVA_WHEEL) + (unsigned)value); break;
    case T_UNIT_RITUAL_CASTS:
        if (!setUnitRitualCasts((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3), (int)value)) return refusedSetValue(L, "out_of_memory", v, value, min, max);
        break;
    case T_UNIT_BATTLE_X:
    case T_UNIT_BATTLE_Y: {
        int unit = (int)lua_tointeger(L, 2), x = 0, y = 0;
        if (!unitBattleCell(unit, &x, &y)) return refusedSetValue(L, "not_on_field", v, value, min, max);
        const char *reason = moveUnitBattleCell(unit, v->type == T_UNIT_BATTLE_X ? (int)value : x, v->type == T_UNIT_BATTLE_Y ? (int)value : y);
        if (reason) return refusedSetValue(L, reason, v, value, min, max);
        break;
    }
    case T_UNIT_ITEM: {
        const char *reason = writeUnitItem((int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3), (int)value);
        if (reason) return refusedSetValue(L, reason, v, value, min, max);
        break;
    }
    default: break;
    }
    return 1;
}

static const struct { const char *name; int kind, nameSize; } CONTENT_VALUES[] = {
    {"monsterName", CONTENT_MONSTER, MONSTER_NAME_SIZE}, {"monsterSprite", CONTENT_MONSTER, 0}, {"monsterHp", CONTENT_MONSTER, 0},
    {"monsterFlags", CONTENT_MONSTER, 0}, {"monsterMoveFlags", CONTENT_MONSTER, 0}, {"monsterAttackType", CONTENT_MONSTER, 0},
    {"monsterAttackWeapon", CONTENT_MONSTER, 0}, {"monsterAttackValue", CONTENT_MONSTER, 0}, {"monsterAbility", CONTENT_MONSTER, 0},
    {"itemName", CONTENT_ITEM, ITEM_NAME_SIZE}, {"itemSlotType", CONTENT_ITEM, 0},
    {"className", CONTENT_CLASS, CLASS_NAME_SIZE}, {"classLeader", CONTENT_CLASS, MONSTER_NAME_SIZE},
    {"classStartArmy", CONTENT_CLASS, CLASS_START_UNIT_SIZE}, {"classStandardTroops", CONTENT_CLASS, 0}, {"classAbility", CONTENT_CLASS, 0},
    {"classRecruitName", CONTENT_CLASS_RECRUITS, MONSTER_NAME_SIZE}, {"classRecruitCount", CONTENT_CLASS_RECRUITS, 0},
    {"terrainName", CONTENT_TERRAIN, TERRAIN_GOLD}, {"terrainFlags", CONTENT_TERRAIN, 0}, {"terrainIncome", CONTENT_TERRAIN, 0},
    {"ritualName", CONTENT_RITUAL, RITUAL_NAME_SIZE}, {"ritualString", CONTENT_RITUAL, RITUAL_STRING_SIZE}, {"ritualRitpow", CONTENT_RITUAL, 0},
    {"ritualLevel", CONTENT_RITUAL, 0}, {"ritualCostType", CONTENT_RITUAL, 0}, {"ritualCost", CONTENT_RITUAL, 0},
    {"ritualTerrain", CONTENT_RITUAL, 0}, {"ritualEffect", CONTENT_RITUAL, 0}, {"ritualEffectValue", CONTENT_RITUAL, 0},
    {"ritualFlags", CONTENT_RITUAL, 0}, {"ritualSummons", CONTENT_RITUAL, 0}, {"ritualCasterMaxCast", CONTENT_CASTER_MAX_CAST, 0},
    {"weaponName", CONTENT_WEAPON, WEAPON_TRGRANK}, {"weaponDmgType", CONTENT_WEAPON, 0}, {"weaponAoe", CONTENT_WEAPON, 0},
};

static int contentValueRow(const GameVar *v) {
    for (int i = 0; i < (int)(sizeof CONTENT_VALUES / sizeof CONTENT_VALUES[0]); i++)
        if (!strcmp(CONTENT_VALUES[i].name, v->name)) return i;
    return -1;
}

static int monsterAttackEnd(const unsigned char *record) {
    for (int k = 0; k < MONSTER_ATTACK_MAX; k++) {
        const unsigned char *r = record + MONSTER_ATTACKS + (size_t)k * MONSTER_ATTACK_SIZE;
        if (*(const int *)r < 1 || *(const int *)(r + MONSTER_ATTACK_WEAPON) < 0) return k;
    }
    return MONSTER_ATTACK_MAX;
}

static int refuseContentWrite(lua_State *L, const char *reason, const GameVar *v, lua_Integer index, lua_Integer value) {
    LuaScript *s = luaScriptOf(L);
    refuseLuaCall(s ? s->mod : "?", "coe5.set", reason, "name=%s index=%lld value=%lld", v->name, (long long)index, (long long)value);
    return 0;
}

static int setContentValue(lua_State *L, const GameVar *v, unsigned char *p, int arg) {
    int row = contentValueRow(v), kind = CONTENT_VALUES[row].kind, index = (int)lua_tointeger(L, 2);
    int recordKind = kind == CONTENT_CASTER_MAX_CAST ? CONTENT_RITUAL : kind;
    GameVarShape shape = gameVarShape(v);
    unsigned char *record = contentRecord(recordKind, index);
    int at2 = v->count2 > 1 ? (int)lua_tointeger(L, 3) : 0;
    const char *reason = NULL;
    lua_Integer value = 0, min = 0, max = 0;
    if (v->type != T_STR && v->type != T_CLASS_STDTROOPS && v->type != T_RITUAL_SUMMONS) {
        value = luaL_checkinteger(L, arg);
        gameVarRange(v, &min, &max);
        if (v->type == T_I64) min = LUA_MININTEGER, max = LUA_MAXINTEGER;
        if (v->type == T_TERRAIN_INCOME) min = SHRT_MIN, max = USHRT_MAX;
        if (v->type == T_RITUAL_CASTER_MAX_CAST) min = 0, max = COMMANDER_CAST_MAX;
        if (v->type == T_CLASS_ABILITY || v->type == T_MONSTER_ABILITY || v->type == T_MONSTER_ATTACK) {
            if (v->type != T_MONSTER_ATTACK || v->addr != RVA_MONSTER_TABLE + MONSTER_ATTACKS + MONSTER_ATTACK_WEAPON) min = INT_MIN, max = INT_MAX;
        }
        if (value < min || value > max) return refuseSetValue(L, "out_of_range", v, value, min, max);
    }
    if (!p) return shape.list != LIST_NONE ? luaL_error(L, "%s: the list ends before that index", v->name)
                                           : luaL_error(L, "%s: index %d out of range", v->name, index);
    if (!record) return luaL_error(L, "%s: index %d out of range", v->name, index);
    int offset = (int)(p - record);
    if (v->type == T_STR) {
        const char *text = luaL_checkstring(L, arg);
        int size = CONTENT_VALUES[row].nameSize;
        char field[RITUAL_STRING_SIZE] = {0};
        snprintf(field, (size_t)size, "%s", text);
        int appends = shape.list == LIST_TO_EMPTY_NAME && !*p && field[0] && at2 + 1 < v->count2;
        reason = writeContent(kind, index, offset, field, size);
        if (!reason && appends && p[v->stride2]) {
            char ended = 0;
            reason = writeContent(kind, index, offset + v->stride2, &ended, 1);
        }
        return reason ? refuseContentWrite(L, reason, v, index, 0) : 0;
    }
    if (v->type == T_CLASS_STDTROOPS || v->type == T_RITUAL_SUMMONS) {
        if (lua_type(L, arg) != LUA_TBOOLEAN) {
            LuaScript *s = luaScriptOf(L);
            refuseLuaCall(s ? s->mod : "?", "coe5.set", "needs_boolean", "name=%s value=%s", v->name, luaL_typename(L, arg));
            return 0;
        }
        int on = lua_toboolean(L, arg);
        if (v->type == T_CLASS_STDTROOPS) reason = setRecordAbility(RECORD_CLASS, index, CLASS_ABILITY_STDTROOPS, on);
        else if (on) reason = ritualFirstFx(record, RITUAL_FX_SUMMONING, -1) != -1 ? NULL : setRecordAbility(RECORD_RITUAL, index, RITUAL_FX_SUMMONING, 1);
        else {
            unsigned char fx[RITUAL_FX_MAX * RITUAL_FX_SIZE] = {0};
            int kept = 0;
            for (int i = 0; i < RITUAL_FX_MAX && ritualFxId(record, i) >= 1; i++)
                if (ritualFxId(record, i) != RITUAL_FX_SUMMONING) memcpy(fx + (size_t)kept++ * RITUAL_FX_SIZE, record + RITUAL_FX + (size_t)i * RITUAL_FX_SIZE, RITUAL_FX_SIZE);
            reason = writeContent(CONTENT_RITUAL, index, RITUAL_FX, fx, (int)sizeof fx);
        }
        return reason ? refuseContentWrite(L, reason, v, index, on) : 0;
    }
    switch (v->type) {
    case T_TERRAIN_INCOME: {
        int field = terrainIncomeField(at2);
        short n = (short)value;
        if (field < 0) return refuseContentWrite(L, "no_field", v, index, value);
        reason = writeContent(kind, index, field, &n, 2);
        break;
    }
    case T_CLASS_ABILITY:
    case T_MONSTER_ABILITY:
        if (at2 < 1) return refuseContentWrite(L, "bad_ability", v, index, value);
        reason = setRecordAbility(v->type == T_CLASS_ABILITY ? RECORD_CLASS : RECORD_MONSTER, index, at2, value);
        break;
    case T_RITUAL_CASTER_MAX_CAST: {
        short n = (short)value;
        reason = writeContent(CONTENT_CASTER_MAX_CAST, index, 0, &n, 2);
        break;
    }
    case T_MONSTER_ATTACK: {
        int end = monsterAttackEnd(record), field = (int)(v->addr - (RVA_MONSTER_TABLE + MONSTER_ATTACKS));
        if (at2 > end) return luaL_error(L, "%s: the list ends before that index", v->name);
        long long wide = value;
        reason = writeContent(kind, index, MONSTER_ATTACKS + at2 * MONSTER_ATTACK_SIZE + field, &wide, field == MONSTER_ATTACK_VALUE ? 8 : 4);
        if (!reason && at2 == end && monsterAttackEnd(record) > end && end + 1 < MONSTER_ATTACK_MAX && monsterAttackEnd(record) > end + 1) {
            int ended = 0;
            reason = writeContent(kind, index, MONSTER_ATTACKS + (end + 1) * MONSTER_ATTACK_SIZE, &ended, 4);
        }
        break;
    }
    default: {
        int width = v->type == T_I64 ? 8 : v->type == T_I16 ? 2 : 4;
        long long wide = value;
        int endsBefore = shape.list == LIST_TO_BELOW_ZERO_I32 ? *(const int *)p < 0 : shape.list == LIST_TO_BELOW_ONE_I32 ? *(const int *)p < 1 : 0;
        int keyAppends = !shape.nameBack && endsBefore && (shape.list == LIST_TO_BELOW_ZERO_I32 ? value >= 0 : value >= 1) && at2 + 1 < v->count2;
        reason = writeContent(kind, index, offset, &wide, width);
        if (!reason && keyAppends) {
            int ended = shape.list == LIST_TO_BELOW_ZERO_I32 ? -1 : 0;
            if (shape.list == LIST_TO_BELOW_ZERO_I32 ? *(const int *)(p + v->stride2) >= 0 : *(const int *)(p + v->stride2) >= 1)
                reason = writeContent(kind, index, offset + v->stride2, &ended, 4);
        }
    }
    }
    return reason ? refuseContentWrite(L, reason, v, index, value) : 0;
}

static int lSet(lua_State *L) {
    const GameVar *v;
    int arg = 2;
    unsigned char *p = gameVarAddress(L, &v, &arg, 1);
    if (v->type == T_RECORD_WORD && p) return setRecordWord(L, v, arg);
    if (!v->writable) return luaL_error(L, "game variable '%s' is read-only", v->name);
    if (contentValueRow(v) >= 0) return setContentValue(L, v, p, arg);
    if (v->type == T_PLANE_TINT) return setPlaneTintValue(L, arg);
    int pastListEnd = !p && gameVarShape(v).list != LIST_NONE;
    if (!p && !pastListEnd) return luaL_error(L, "%s can only be set while a wrap of its site runs", v->name);
    if (v->writable == WRITABLE_WHILE_BUILDING) return setSquareValue(L, v, p, arg);
    if (v->type == T_WINDOWS_ONLY) {
        LuaScript *s = luaScriptOf(L);
        refuseLuaCall(s ? s->mod : "?", "coe5.set", "windows_only", "name=%s", v->name);
        return 0;
    }
    if (v->type == T_GAME_CHANGED) {
        LuaScript *s = luaScriptOf(L);
        refuseLuaCall(s ? s->mod : "?", "coe5.set", "game_changed", "name=%s", v->name);
        return 0;
    }
    if (v->type == T_CEREMONY_COST) return setCeremonyCost(L, p, arg);
    if (v->type == T_PTR_STR) return setPointerName(L, v, p, arg);
    if (setComputedValue(L, v, p, arg)) return 0;
    if (v->type == T_U8 || v->type == T_I8 || v->type == T_I16 || v->type == T_I32 || v->type == T_FRAME_I32 || v->type == T_SITE_I32) {
        lua_Integer value = luaL_checkinteger(L, arg), min, max;
        gameVarRange(v, &min, &max);
        if (p) narrowPlaneRange(v, p, &max);
        if (value < min || value > max) {
            LuaScript *s = luaScriptOf(L);
            refuseLuaCall(s ? s->mod : "?", "coe5.set", "out_of_range", "name=%s value=%lld min=%lld max=%lld", v->name,
                          (long long)value, (long long)min, (long long)max);
            return 0;
        }
    }
    int isFlag = v->type == T_FLAG_U8 || v->type == T_FLAG_I32 || v->type == T_FLAG_I16 || v->type == T_FLAG_TWO;
    if (isFlag && lua_type(L, arg) != LUA_TBOOLEAN) {
        LuaScript *s = luaScriptOf(L);
        refuseLuaCall(s ? s->mod : "?", "coe5.set", "needs_boolean", "name=%s value=%s", v->name, luaL_typename(L, arg));
        return 0;
    }
    if (isSlotColour(v)) {
        lua_Number value = luaL_checknumber(L, arg);
        const char *reason = !isfinite(value) ? "not_finite"
                           : value < SLOT_COLOUR_MIN - 1.0 || value > SLOT_COLOUR_MAX + 1.0 || (float)value < SLOT_COLOUR_MIN ||
                                     (float)value > SLOT_COLOUR_MAX
                               ? "out_of_range"
                               : NULL;
        if (reason) {
            LuaScript *s = luaScriptOf(L);
            refuseLuaCall(s ? s->mod : "?", "coe5.set", reason, "name=%s value=%.14g min=%.14g max=%.14g", v->name, (double)value,
                          (double)SLOT_COLOUR_MIN, (double)SLOT_COLOUR_MAX);
            return 0;
        }
    }
    if (pastListEnd) return luaL_error(L, "%s: the list ends before that index", v->name);
    int oldLeader = !strcmp(v->name, "unitCommander") ? *(int *)p : -1;
    short endMark = 0;
    int appends = listEndMark(v->name, &endMark) && *(short *)p <= endMark;
    double floatMin, floatMax;
    if (v->type == T_F32 && gameVarFloatRange(v, &floatMin, &floatMax)) {
        lua_Number value = luaL_checknumber(L, arg);
        const char *reason = !isfinite(value) ? "not_finite" : value < floatMin || value > floatMax ? "out_of_range" : NULL;
        if (reason) {
            LuaScript *s = luaScriptOf(L);
            refuseLuaCall(s ? s->mod : "?", "coe5.set", reason, "name=%s value=%.14g min=%.14g max=%.14g", v->name, (double)value,
                          floatMin, floatMax);
            return 0;
        }
    }
    switch (v->type) {
    case T_U8: *p = (unsigned char)luaL_checkinteger(L, arg); break;
    case T_I8: *(signed char *)p = (signed char)luaL_checkinteger(L, arg); break;
    case T_I16: *(short *)p = (short)luaL_checkinteger(L, arg); break;
    case T_FLAG_U8: *p = lua_toboolean(L, arg) ? 1 : 0; break;
    case T_FLAG_I32: *(int *)p = lua_toboolean(L, arg) ? 1 : 0; break;
    case T_FLAG_I16: *(short *)p = lua_toboolean(L, arg) ? 1 : 0; break;
    case T_FLAG_TWO: *(int *)p = lua_toboolean(L, arg) ? 2 : 1; break;
    case T_F32: {
        lua_Number value = luaL_checknumber(L, arg);
        *(float *)p = value > FLT_MAX ? INFINITY : value < -FLT_MAX ? -INFINITY : (float)value;
        break;
    }
    case T_STR:
    case T_FRAME_STR: snprintf((char *)p, v->size, "%s", luaL_checkstring(L, arg)); break;
    case T_I32:
    case T_SITE_I32:
    case T_FRAME_I32: *(int *)p = (int)luaL_checkinteger(L, arg); break;
    default: {
        LuaScript *s = luaScriptOf(L);
        refuseLuaCall(s ? s->mod : "?", "coe5.set", "not_writable", "name=%s", v->name);
    }
    }
    if (appends && *(short *)p > endMark && lua_tointeger(L, arg - 1) + 1 < v->count2) *(short *)(p + v->stride2) = endMark;
    if (!strcmp(v->name, "unitCommander")) flushCommanderCache(oldLeader), flushCommanderCache(*(int *)p);
    if (!strcmp(v->name, "unitApUsed")) flushArmyApCaches((int)((p - GAME_PTR(v->addr)) / UNIT_SIZE));
    if (!strcmp(v->name, "loadedGame") && lua_tointeger(L, arg)) {
        LuaScript *s = luaScriptOf(L);
        writeLog("lua_loaded_game_set", "mod=%s value=%lld", s ? s->mod : "?", (long long)lua_tointeger(L, arg));
    }
    return 0;
}
