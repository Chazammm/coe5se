#pragma once

#define TERRAIN_RECRUITS_END (-999)

enum { TR_ADD_UNIT, TR_ADD_MERC, TR_ADD_COM, TR_CLEAR, TR_XCOST, TR_TEMPLE, TR_LIBRARY, TR_ATAVI, TR_LIMITER, TR_TERRAIN, TR_WORDS };

static const char *const TERRAIN_RECRUIT_WORDS[TR_WORDS] = {
    "addunitrec", "addmercrec", "addcomrec", "clearrec", "recxcost", "templerec", "libraryrec", "atavirec", "reclimiter", "recterr",
};

static struct {
    unsigned char vanilla[SPECIAL_LOCATION_RECRUITS_MAX * CLASS_RECRUIT_SIZE];
    int vanillaCount;
    int count;
    int last, lastMod, lastTerrain, limiters;
} terrainRecruits = {.vanillaCount = 0, .last = -1};

static int terrainRecruitWordOf(const char *line, size_t len) {
    int word = 0;
    while (word < TR_WORDS && (strlen(TERRAIN_RECRUIT_WORDS[word]) != len || strncmp(TERRAIN_RECRUIT_WORDS[word], line, len))) word++;
    return word;
}

static unsigned char *terrainRecruitAt(int i) {
    return gameBase + RVA_SPECIAL_LOCATION_RECRUITS + (size_t)i * CLASS_RECRUIT_SIZE;
}

static int *terrainRecruitInt(int i, int field) {
    return (int *)(terrainRecruitAt(i) + field);
}

static int readVanillaTerrainRecruits(void) {
    if (terrainRecruits.vanillaCount) return terrainRecruits.vanillaCount > 0;
    int end = 0;
    while (end < SPECIAL_LOCATION_RECRUITS_MAX && *terrainRecruitInt(end, RECRUIT_TERRAIN) != TERRAIN_RECRUITS_END) end++;
    if (end == SPECIAL_LOCATION_RECRUITS_MAX) {
        terrainRecruits.vanillaCount = -1;
        writeLogWarning("terrain_recruits_unexpected", "reason=no_end_record");
        return 0;
    }
    memcpy(terrainRecruits.vanilla, terrainRecruitAt(0), (size_t)(end + 1) * CLASS_RECRUIT_SIZE);
    terrainRecruits.vanillaCount = end;
    return 1;
}

static void terrainRecruitsBeforePass(int level) {
    if (level != 2) return;
    terrainRecruits.last = -1;
    if (!readVanillaTerrainRecruits()) return;
    int n = terrainRecruits.vanillaCount;
    memcpy(terrainRecruitAt(0), terrainRecruits.vanilla, (size_t)(n + 1) * CLASS_RECRUIT_SIZE);
    memset(terrainRecruitAt(n + 1), 0, (size_t)(SPECIAL_LOCATION_RECRUITS_MAX - n - 1) * CLASS_RECRUIT_SIZE);
    terrainRecruits.count = n;
}

static int terrainRecruitIntArg(const char *line, int n) {
    int quoted = 0, outside = 1, start = 0, found = -1;
    for (int i = 0;; i++) {
        char c = line[i];
        if (c == '"') quoted = !quoted;
        if (!quoted) {
            if ((c >= '0' && c <= '9') || c == '-') {
                if (outside) start = i;
                outside = 0;
            } else if (!outside) {
                if (++found == n) return atoi(line + start);
                outside = 1;
            }
        }
        if (!c) return -1;
    }
}

static int terrainRecruitTextArg(const char *line, char *out, size_t size) {
    const char *open = strchr(line, '"'), *close = open ? strchr(open + 1, '"') : NULL;
    if (!close) return 0;
    snprintf(out, size, "%.*s", (int)(close - open - 1), open + 1);
    return 1;
}

static void writeTerrainRecruitEnd(int i) {
    memcpy(terrainRecruitAt(i), terrainRecruits.vanilla + (size_t)terrainRecruits.vanillaCount * CLASS_RECRUIT_SIZE, CLASS_RECRUIT_SIZE);
}

static void addTerrainRecruit(int word, const char *line, int terrain, int mod) {
    char name[RECRUIT_LIMITER_SIZE * 2] = "";
    if (!terrainRecruitTextArg(line, name, sizeof name) || monsterByName(name) < 0) {
        refuseC5mLine(TERRAIN_RECRUIT_WORDS[word], spellModName(mod), "unknown_monster", "line=%d monster=%.47s", G32(RVA_MOD_LINE), name);
        return;
    }
    terrainRecruits.last = -1;
    if (terrainRecruits.count >= SPECIAL_LOCATION_RECRUITS_MAX - 1) {
        refuseC5mLine(TERRAIN_RECRUIT_WORDS[word], spellModName(mod), "list_full", "line=%d max=%d", G32(RVA_MOD_LINE), SPECIAL_LOCATION_RECRUITS_MAX - 1);
        return;
    }
    int i = terrainRecruits.count, com = word == TR_ADD_COM, chance = terrainRecruitIntArg(line, 0);
    writeTerrainRecruitEnd(i + 1);
    unsigned char *r = terrainRecruitAt(i);
    memset(r, 0, CLASS_RECRUIT_SIZE);
    snprintf((char *)r + RECRUIT_MONSTER_NAME, RECRUIT_LIMITER_SIZE, "%s", name);
    *terrainRecruitInt(i, RECRUIT_CHANCE) = chance;
    *terrainRecruitInt(i, RECRUIT_COUNT) = com ? -1 : terrainRecruitIntArg(line, 1);
    *terrainRecruitInt(i, RECRUIT_GOLD) = terrainRecruitIntArg(line, com ? 1 : 2);
    *terrainRecruitInt(i, RECRUIT_GOLD_DIE) = terrainRecruitIntArg(line, com ? 2 : 3);
    *terrainRecruitInt(i, RECRUIT_IRON) = terrainRecruitIntArg(line, com ? 3 : 4);
    *terrainRecruitInt(i, RECRUIT_FLAGS) = com ? 5 : (chance != 100) + (word == TR_ADD_MERC ? 4 : 0);
    short *costs = (short *)(r + RECRUIT_EXTRA_COSTS);
    costs[0] = costs[2] = -1;
    *terrainRecruitInt(i, RECRUIT_TERRAIN) = terrain;
    terrainRecruits.count++;
    terrainRecruits.last = i, terrainRecruits.limiters = 0;
}

static void clearTerrainRecruits(int terrain) {
    int kept = 0;
    for (int i = 0; i < terrainRecruits.count; i++) {
        if (*terrainRecruitInt(i, RECRUIT_TERRAIN) == terrain) {
            moveTerrainListRecBonuses(i, -1);
            continue;
        }
        if (kept != i) {
            memcpy(terrainRecruitAt(kept), terrainRecruitAt(i), CLASS_RECRUIT_SIZE);
            moveTerrainListRecBonuses(i, kept);
        }
        kept++;
    }
    writeTerrainRecruitEnd(kept);
    memset(terrainRecruitAt(kept + 1), 0, (size_t)(terrainRecruits.count - kept) * CLASS_RECRUIT_SIZE);
    terrainRecruits.count = kept;
    terrainRecruits.last = -1;
}

static void setTerrainRecruitCost(int i, const char *line) {
    short *costs = (short *)(terrainRecruitAt(i) + RECRUIT_EXTRA_COSTS);
    int at = costs[0] == -1 ? 0 : 2;
    costs[at] = (short)terrainRecruitIntArg(line, 0), costs[at + 1] = (short)terrainRecruitIntArg(line, 1);
    costs[at + 2] = -1;
}

static void addTerrainRecruitLimiter(int i, const char *line, size_t len, int mod) {
    int slot = terrainRecruits.limiters;
    if (slot < 0 || slot >= RECRUIT_LIMITERS_MAX) return;
    char text[RECRUIT_LIMITER_SIZE] = "";
    terrainRecruitTextArg(line, text, sizeof text);
    unsigned char *limiters = terrainRecruitAt(i) + RECRUIT_LIMITERS;
    memcpy(limiters + (size_t)slot * RECRUIT_LIMITER_SIZE, text, RECRUIT_LIMITER_SIZE);
    terrainRecruits.limiters = ++slot;
    if (slot < RECRUIT_LIMITERS_MAX) limiters[(size_t)slot * RECRUIT_LIMITER_SIZE] = 0;
    readRecBonusWord(REC_BONUS_UNITS, line + len, REC_BONUS_TERRAIN_LIST, i, slot - 1, mod);
}

static int terrainRecruitLineHook(const char *line, size_t len, int mod, int level) {
    if (level != 2) return 0;
    int terrain = G32(RVA_TERRAIN_CURRENT);
    if (terrain < 0 || G32(RVA_CLASS_CURRENT) >= 0) return 0;
    int word = terrainRecruitWordOf(line, len), flagWord = word == TR_WORDS ? recFlagWordOf(line, len) : RF_WORDS;
    if (word == TR_WORDS && flagWord == RF_WORDS) return 0;
    const char *name = word < TR_WORDS ? TERRAIN_RECRUIT_WORDS[word] : recFlagWords[flagWord];
    if (!featureOn(FEATURE_MOD_SETTINGS)) {
        refuseC5mLine(name, spellModName(mod), "site_unavailable", "line=%d", G32(RVA_MOD_LINE));
        return 1;
    }
    if (!readVanillaTerrainRecruits()) {
        refuseC5mLine(name, spellModName(mod), "list_unexpected", "line=%d", G32(RVA_MOD_LINE));
        return 1;
    }
    if (terrainRecruits.lastMod != mod || terrainRecruits.lastTerrain != terrain) terrainRecruits.last = -1;
    terrainRecruits.lastMod = mod, terrainRecruits.lastTerrain = terrain;
    if (word <= TR_ADD_COM) {
        addTerrainRecruit(word, line, terrain, mod);
        return 1;
    }
    if (word == TR_CLEAR) {
        clearTerrainRecruits(terrain);
        return 1;
    }
    int i = terrainRecruits.last;
    if (i < 0) return 1;
    if (flagWord < RF_WORDS) setRecFlag(flagWord, line, len, terrainRecruitAt(i), 1, mod);
    else if (word == TR_XCOST) setTerrainRecruitCost(i, line);
    else if (word == TR_TERRAIN) *terrainRecruitInt(i, RECRUIT_TERRAIN) = terrainRecruitIntArg(line, 0);
    else if (word == TR_LIMITER) addTerrainRecruitLimiter(i, line, len, mod);
    else {
        int kind = recBonusKindOf(line, len);
        *terrainRecruitInt(i, RECRUIT_FLAGS) |= REC_BONUS_WORDS[kind].flag;
        readRecBonusWord(kind, line + len, REC_BONUS_TERRAIN_LIST, i, -1, mod);
    }
    return 1;
}

static int declinedTerrainRecruitLine(const char *line, size_t len, int mod, int level) {
    static const char *const ENDERS[] = {"newmonster", "selectmonster", "newweapon", "selectweapon", "newritual", "selectritual",
                                          "newitem", "selectitem", "selectterrgroup", "selectclass", "newclass"};
    static int inTerrain, blockMod = -1, blockLevel = -1;
    if (mod != blockMod || level != blockLevel) inTerrain = 0;
    blockMod = mod, blockLevel = level;
    if (spellWordIs(line, len, "selectterr")) inTerrain = 1;
    for (size_t i = 0; i < sizeof ENDERS / sizeof ENDERS[0]; i++)
        if (spellWordIs(line, len, ENDERS[i])) inTerrain = 0;
    return inTerrain && level == 2 && terrainRecruitWordOf(line, len) < TR_WORDS;
}
