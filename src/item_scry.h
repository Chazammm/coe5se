// ---- randitemloc and randnewitemloc: a ritual's target on a square holding a matching item ----------------------
#pragma once

#define ITEM_SCRY_MARK        0x4953ULL
#define ITEM_SCRY_SETS_MAX    0xffff
#define ITEM_SCRY_WORD_SIZE   32
#define ITEM_SCRY_SEEN        2
#define ITEM_SCRY_PLAYERS     24

enum { ITEM_SCRY_TAG = 1, ITEM_SCRY_NAME, ITEM_SCRY_COMMAND };

typedef struct {
    int ritual, unseenFirst;
} ItemScrySet;

typedef struct {
    int set, kind, tag, line;
    char name[ITEM_NAME_SIZE];
    unsigned short flagOffset;
    unsigned long long flagMask;
    int ability;
    int kept;
    unsigned long long weaponFlags;
    char mod[64];
} ItemScryRule;

static struct {
    ItemScrySet *sets;
    int setCount, setCapacity;
    ItemScryRule *rules;
    int ruleCount, ruleCapacity;
    int state;
    int casterPlayer;
    unsigned char *picked;
} itemScry = {.casterPlayer = -1};

static int (*itemScryRandom)(int n);
static int (*itemScryPlaneAt)(int x, int y);
static int (*itemScrySameTeam)(int a, int b);
static int (*itemScrySeenAt)(int player, int x, int y);

static unsigned long long itemScryValue(int set) { return ITEM_SCRY_MARK << 48 | (unsigned long long)set << 32 | 1; }

static int itemScrySetOf(long long value) {
    unsigned long long v = (unsigned long long)value;
    if (v >> 48 != ITEM_SCRY_MARK || (unsigned)v != 1) return -1;
    int set = (int)(v >> 32 & ITEM_SCRY_SETS_MAX);
    return set < itemScry.setCount ? set : -1;
}

// ---- Matching an item ------------------------------------------------------------------------------------------

static const unsigned char *itemScryItem(int item) {
    return item >= 0 && item < tableRecords(TABLE_ITEM) ? tableAddress(TABLE_ITEM) + (size_t)item * ITEM_SIZE : NULL;
}

static int itemScryAbility(const unsigned char *record, int id) {
    const int *pairs = (const int *)(record + ITEM_ABILITIES);
    for (int i = 0; i < ITEM_ABILITY_PAIRS && pairs[2 * i]; i++)
        if (pairs[2 * i] == id) return pairs[2 * i + 1];
    return 0;
}

static int itemScryRuleMatches(const ItemScryRule *r, int item) {
    const unsigned char *record = itemScryItem(item);
    if (!record) return 0;
    if (r->kind == ITEM_SCRY_TAG) return itemHasTag(item, r->tag);
    if (r->kind == ITEM_SCRY_NAME) return recordNameIs(record, ITEM_NAME_SIZE, r->name);
    if (r->kept && item < keptItemsSize && keptItems[item]) return 1;
    if (r->flagOffset) {
        unsigned long long flags;
        memcpy(&flags, record + r->flagOffset, sizeof flags);
        if ((flags & r->flagMask) == r->flagMask) return 1;
    }
    if (r->ability && itemScryAbility(record, r->ability) > 0) return 1;
    if (r->weaponFlags) {
        int weapon;
        memcpy(&weapon, record + ITEM_WEAPON, sizeof weapon);
        if (weapon < 0 || weapon >= tableRecords(TABLE_WEAPON)) return 0;
        unsigned long long flags;
        memcpy(&flags, tableAddress(TABLE_WEAPON) + (size_t)weapon * WEAPON_SIZE + WEAPON_FLAGS, sizeof flags);
        return (flags & r->weaponFlags) == r->weaponFlags;
    }
    return 0;
}

static int itemScrySetMatches(int set, int item) {
    for (int i = 0; i < itemScry.ruleCount; i++)
        if (itemScry.rules[i].set == set && itemScryRuleMatches(&itemScry.rules[i], item)) return 1;
    return 0;
}

// ---- The pick ----------------------------------------------------------------------------------------------------

static int itemScryOnPlane(int plane, int x, int y) { return plane < 0 || itemScryPlaneAt(x, y) == plane; }

static int itemScryMarkSquares(int set, int player, int plane, int *unseen) {
    int w = G32(RVA_MAP_W), h = G32(RVA_MAP_H), squares = 0;
    if (w > MAP_COLUMNS_MAX) w = MAP_COLUMNS_MAX;
    if (h > MAP_COLUMN_SQUARES) h = MAP_COLUMN_SQUARES;
    *unseen = 0;
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++) {
            const short *ground = groundItemsAt(x, y);
            int found = 0;
            for (int i = 0; i < GROUND_ITEMS_PER_SQUARE && ground[i] >= 0 && !found; i++) found = itemScrySetMatches(set, ground[i]);
            if (found && itemScryOnPlane(plane, x, y)) itemScry.picked[(size_t)x * MAP_COLUMN_SQUARES + (size_t)y] = 1;
        }
    int last = G32(RVA_LAST_COMMANDER);
    for (int c = 0; c <= last && c < COMMANDER_TABLE_MAX; c++) {
        int unit = G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE);
        if (unit < 0 || unit >= UNIT_TABLE_MAX) continue;
        size_t at = (size_t)unit * UNIT_SIZE;
        int x = G32(RVA_UNIT_X + at), y = G32(RVA_UNIT_X + 4 + at), owner = (short)G16(RVA_UNIT_PLAYER + at);
        if ((short)G16(RVA_UNIT_HP + at) <= 0 || x < 0 || y < 0 || x >= w || y >= h || owner < 0 || owner >= ITEM_SCRY_PLAYERS ||
            itemScrySameTeam(owner, player) || !itemScryOnPlane(plane, x, y))
            continue;
        int positions = unitExtraItemCount(unit) ? ITEM_POSITION_MAX : UNIT_ITEM_SLOTS;
        for (int p = 0; p < positions; p++)
            if (itemScrySetMatches(set, unitItemAt(unit, p))) {
                itemScry.picked[(size_t)x * MAP_COLUMN_SQUARES + (size_t)y] = 1;
                break;
            }
    }
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++)
            if (itemScry.picked[(size_t)x * MAP_COLUMN_SQUARES + (size_t)y]) {
                squares++;
                if (itemScrySeenAt(player, x, y) < ITEM_SCRY_SEEN) (*unseen)++;
            }
    return squares;
}

static void itemScryTakeSquare(int n, int unseenOnly, int player, int *x, int *y) {
    int w = G32(RVA_MAP_W), h = G32(RVA_MAP_H);
    if (w > MAP_COLUMNS_MAX) w = MAP_COLUMNS_MAX;
    if (h > MAP_COLUMN_SQUARES) h = MAP_COLUMN_SQUARES;
    for (int sx = 0; sx < w; sx++)
        for (int sy = 0; sy < h; sy++) {
            unsigned char *mark = &itemScry.picked[(size_t)sx * MAP_COLUMN_SQUARES + (size_t)sy];
            if (!*mark) continue;
            *mark = 0;
            if (unseenOnly && itemScrySeenAt(player, sx, sy) >= ITEM_SCRY_SEEN) continue;
            if (n-- == 0) *x = sx, *y = sy;
        }
}

static __attribute__((unused)) int itemScryAtEffect(int ritual, int effect, int plane, int *x, int *y) {
    if (ritual < 0 || ritual >= ritualSlotCount() || effect < 0 || effect >= RITUAL_FX_MAX) return 0;
    const long long *fx = ritualWordEffect(ritualWordRecord(ritual), effect);
    int set = fx[0] == RITUAL_FX_RANDOMLOC ? itemScrySetOf(fx[1]) : -1;
    if (set < 0) return 0;
    int player = itemScry.casterPlayer, unseen = 0;
    *x = *y = -1;
    if (!itemScry.picked) itemScry.picked = calloc((size_t)MAP_COLUMNS_MAX * MAP_COLUMN_SQUARES, 1);
    if (!itemScry.picked) {
        writeLogWarning("item_scry_target", "ritual=%d reason=out_of_memory", ritual);
        return 1;
    }
    int squares = itemScryMarkSquares(set, player, plane, &unseen);
    int unseenOnly = itemScry.sets[set].unseenFirst && unseen > 0, count = unseenOnly ? unseen : squares;
    itemScryTakeSquare(count ? itemScryRandom(count) : -1, unseenOnly, player, x, y);
    writeLog("item_scry_target", "ritual=%d player=%d plane=%d squares=%d unseen=%d x=%d y=%d", ritual, player, plane, squares, unseen, *x, *y);
    return 1;
}

// ---- The words ---------------------------------------------------------------------------------------------------

static int readItemScryCommand(const char *word, ItemScryRule *r) {
    size_t len = strlen(word);
    if (!strcmp(word, "indestructible")) r->kept = 1;
    for (size_t i = 0; i < sizeof RECORD_WORDS / sizeof RECORD_WORDS[0]; i++) {
        const RecordWord *row = &RECORD_WORDS[i];
        if (strcmp(row->word, word)) continue;
        if (row->record == RECORD_ITEM && row->kind == WORD_FLAG && !r->flagOffset && !r->ability) {
            r->flagOffset = row->offset;
            r->flagMask = row->value;
        } else if (row->record == RECORD_ITEM && row->kind == WORD_ABILITY && !r->flagOffset && !r->ability) {
            r->ability = (int)row->value;
        } else if (row->record == RECORD_WEAPON && row->kind == WORD_FLAG && !r->weaponFlags) {
            r->weaponFlags = row->value;
        }
    }
    int own = itemBlockWord(word, len);
    if (own >= ITEM_WORD_USEABLE && own < ITEM_WORD_INDESTRUCTIBLE && !r->flagOffset && !r->ability)
        r->ability = ITEM_WORD_ABILITIES[own - ITEM_WORD_USEABLE];
    for (size_t i = 0; !r->flagOffset && !r->ability && i < sizeof RITUAL_EFFECT_NAMES / sizeof RITUAL_EFFECT_NAMES[0]; i++)
        for (const char *name = RITUAL_EFFECT_NAMES[i].name; name; name = strchr(name, '/') ? strchr(name, '/') + 1 : NULL)
            if (strcspn(name, "/") == len && !strncmp(name, word, len)) {
                r->ability = RITUAL_EFFECT_NAMES[i].id;
                break;
            }
    return r->kept || r->flagOffset || r->ability || r->weaponFlags;
}

static int installItemScry(void);

static void itemScryLine(const char *word, int unseenFirst, const char *args, int mod, int level) {
    if (level != 2) return;
    int ritual = G32(RVA_RITUAL_CURRENT), line = G32(RVA_MOD_LINE);
    const char *p = args, *reason = NULL;
    ItemScryRule r = {.line = line};
    snprintf(r.mod, sizeof r.mod, "%s", spellModName(mod));
    skipSpaces(&p);
    if (!strncmp(p, "tag", 3) && (p[3] == ' ' || p[3] == '\t' || isLineEnd(p + 3))) {
        p += 3;
        char name[MONSTER_TAG_NAME_SIZE];
        int len = readTagName(&p, name, sizeof name);
        skipSpaces(&p);
        if (len < 0) reason = "too_long";
        else if (!len || !isLineEnd(p)) reason = "no_name";
        else if ((r.tag = monsterTagId(name, 1)) < 0) reason = "out_of_memory";
        r.kind = ITEM_SCRY_TAG;
        snprintf(r.name, sizeof r.name, "%s", name);
    } else if (*p == '"') {
        int len = readSpellQuoted(&p, r.name, sizeof r.name);
        skipSpaces(&p);
        if (len <= 0 || !isLineEnd(p)) reason = "no_name";
        else if (len >= ITEM_NAME_SIZE) reason = "too_long";
        r.kind = ITEM_SCRY_NAME;
    } else {
        char command[ITEM_SCRY_WORD_SIZE];
        size_t len = strcspn(p, " \t\r\n");
        snprintf(command, sizeof command, "%.*s", (int)(len < sizeof command ? len : sizeof command - 1), p);
        p += len;
        skipSpaces(&p);
        long long number;
        if (!isLineEnd(p) && readSpellNumber(&p, &number)) skipSpaces(&p);
        r.kind = ITEM_SCRY_COMMAND;
        snprintf(r.name, sizeof r.name, "%s", command);
        if (!len) reason = "no_command";
        else if (len >= sizeof command || !readItemScryCommand(command, &r)) reason = "unknown_command";
        else if (!isLineEnd(p)) reason = "bad_value";
    }
    if (!reason && (ritual < 0 || ritual >= ritualSlotCount())) reason = "no_ritual";
    if (!reason && !installItemScry()) reason = "not_installed";
    if (reason) {
        refuseC5mLine(word, r.mod, reason, "line=%d text=%.40s", line, args);
        return;
    }
    const char *ritualName = (const char *)ritualWordRecord(ritual);
    int max = 0;
    if (itemScry.setCount >= ITEM_SCRY_SETS_MAX) reason = "too_many_lines", max = ITEM_SCRY_SETS_MAX;
    else if (!growList((void **)&itemScry.sets, &itemScry.setCapacity, itemScry.setCount + 1, sizeof *itemScry.sets) ||
             !growList((void **)&itemScry.rules, &itemScry.ruleCapacity, itemScry.ruleCount + 1, sizeof *itemScry.rules))
        reason = "out_of_memory";
    else if (!addRitualWordEffect(ritual, RITUAL_FX_RANDOMLOC, (long long)itemScryValue(itemScry.setCount)))
        reason = "too_many_effects", max = RITUAL_FX_MAX;
    if (reason) {
        refuseC5mLine(word, r.mod, reason, "line=%d ritual=%.48s max=%d", line, ritualName, max);
        return;
    }
    r.set = itemScry.setCount++;
    itemScry.sets[r.set] = (ItemScrySet){ritual, unseenFirst};
    itemScry.rules[itemScry.ruleCount++] = r;
}

static void randitemlocLine(const char *args, int mod, int level) { itemScryLine("randitemloc", 0, args, mod, level); }
static void randnewitemlocLine(const char *args, int mod, int level) { itemScryLine("randnewitemloc", 1, args, mod, level); }

static int copyItemScryRitual(int source, int target) {
    (void)source;
    if (target < 0 || target >= ritualSlotCount()) return 1;
    unsigned char *r = ritualWordRecord(target);
    for (int s = 0; s < RITUAL_FX_MAX && ritualWordEffect(r, s)[0]; s++) {
        long long *fx = ritualWordEffect(r, s);
        int from = fx[0] == RITUAL_FX_RANDOMLOC ? itemScrySetOf(fx[1]) : -1;
        if (from < 0 || itemScry.sets[from].ritual == target) continue;
        if (itemScry.setCount >= ITEM_SCRY_SETS_MAX ||
            !growList((void **)&itemScry.sets, &itemScry.setCapacity, itemScry.setCount + 1, sizeof *itemScry.sets))
            return 0;
        int to = itemScry.setCount++;
        itemScry.sets[to] = (ItemScrySet){target, itemScry.sets[from].unseenFirst};
        fx[1] = (long long)itemScryValue(to);
        int count = itemScry.ruleCount;
        for (int i = 0; i < count; i++) {
            if (itemScry.rules[i].set != from) continue;
            if (!growList((void **)&itemScry.rules, &itemScry.ruleCapacity, itemScry.ruleCount + 1, sizeof *itemScry.rules)) return 0;
            itemScry.rules[itemScry.ruleCount] = itemScry.rules[i];
            itemScry.rules[itemScry.ruleCount++].set = to;
        }
    }
    return 1;
}

static void itemScryBeforePass(int level) {
    if (level != 2) return;
    itemScry.setCount = itemScry.ruleCount = 0;
}

static void itemScryAfterPass(int level) {
    if (level != 2) return;
    int kept = 0, last = G32(RVA_ITEM_COUNT);
    for (int i = 0; i < itemScry.ruleCount; i++) {
        ItemScryRule *r = &itemScry.rules[i];
        int found = r->kind != ITEM_SCRY_NAME;
        for (int item = 0; !found && item <= last && item < tableRecords(TABLE_ITEM); item++) found = recordNameIs(itemScryItem(item), ITEM_NAME_SIZE, r->name);
        if (found) itemScry.rules[kept++] = *r;
        else refuseC5mLine(itemScry.sets[r->set].unseenFirst ? "randnewitemloc" : "randitemloc", r->mod, "unknown_item", "line=%d item=%.47s", r->line, r->name);
    }
    itemScry.ruleCount = kept;
}

// ---- The cast ----------------------------------------------------------------------------------------------------

static long long (*nextItemScryExecute)(int, int, int, int, int, int);
static __attribute__((unused)) long long itemScryExecuteRitual(int unit, int ritual, int x, int y, int percent, int pay) {
    int outer = itemScry.casterPlayer;
    itemScry.casterPlayer = unit >= 0 && unit < UNIT_TABLE_MAX ? (short)G16(RVA_UNIT_PLAYER + (size_t)unit * UNIT_SIZE) : -1;
    long long result = nextItemScryExecute(unit, ritual, x, y, percent, pay);
    itemScry.casterPlayer = outer;
    return result;
}

#ifndef __APPLE__
typedef int (*RandomRitualSquareFn)(int plane, int mode, int empty, int size, int terrainA, int terrainB, int *x, int *y);
static uintptr_t *itemScryRspSlot;

static int itemScryRandomSquareHook(int plane, int mode, int empty, int size, int terrainA, int terrainB, int *x, int *y) {
    const unsigned char *rsp = (const unsigned char *)(*itemScryRspSlot + 8);
    int ritual = *(const int *)(rsp + FRAME_EXECUTE_RITUAL_NUMBER), slot = *(const int *)(rsp + FRAME_EXECUTE_RITUAL_FX_SLOT);
    if (itemScryAtEffect(ritual, slot / 2, plane, x, y)) return 1;
    return GAME_FN(RandomRitualSquareFn, RVA_RANDOM_RITUAL_SQUARE)(plane, mode, empty, size, terrainA, terrainB, x, y);
}
static int gameItemScryRandom(int n) { return GAME_FN(int (*)(int), RVA_RANDOM)(n); }
static int gameItemScryPlaneAt(int x, int y) { return GAME_FN(int (*)(int, int), RVA_PLANE_AT)(x, y); }
static int gameItemScrySameTeam(int a, int b) { return GAME_FN(int (*)(int, int), RVA_SAME_TEAM)(a, b); }
static int gameItemScrySeenAt(int player, int x, int y) { return GAME_FN(int (*)(int, int, int), RVA_SQUARE_SEEN_AT)(player, x, y); }
#endif

// @platform-split item_scry: Windows only for now; Mac keeps no effect index in randomloc's frame
static int installItemScry(void) {
    if (itemScry.state) return itemScry.state > 0;
#ifdef __APPLE__
    itemScry.state = -1;
    writeLogWarning("item_scry_installed", "ok=0 reason=windows_first");
    return 0;
#else
    itemScryRandom = gameItemScryRandom;
    itemScryPlaneAt = gameItemScryPlaneAt;
    itemScrySameTeam = gameItemScrySameTeam;
    itemScrySeenAt = gameItemScrySeenAt;
    unsigned char *stub = allocNear(gameBase + RVA_RANDOMLOC_SQUARE_CALL), *slot = stub ? allocNear(stub) : NULL;
    int calls = 0, sites = 0;
    if (slot) {
        makePagesReadExecute(stub, 0x1000);
        itemScryRspSlot = (uintptr_t *)slot;
        calls = redirectCall(RVA_RANDOMLOC_SQUARE_CALL, RVA_RANDOM_RITUAL_SQUARE, stub, (void *)itemScryRandomSquareHook, "randomloc_square",
                             itemScryRspSlot);
        sites = calls && wrapSite(SITE_EXECUTE_RITUAL, (void *)itemScryExecuteRitual, (void **)&nextItemScryExecute);
    } else writeLog("hook_skipped", "site=randomloc_square reason=no_near_page");
    itemScry.state = calls + sites == 2 ? 1 : -1;
    writeInstallLogFields("item_scry_installed", calls + sites, 2, "calls=%d sites=%d", calls, sites);
    return itemScry.state > 0;
#endif
}

static void itemScryInit(void) {
    int words = apiAddModCommand("randitemloc", randitemlocLine) + apiAddModCommand("randnewitemloc", randnewitemlocLine);
    writeResultLog(words == 2, "item_scry_ready", "words=%d of 2", words);
}
