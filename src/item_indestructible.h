// ---- indestructible: items the game never destroys --------------------------------------------------------------
#pragma once

static unsigned char *keptItems;
static int keptItemsSize;
static int indestructibleState;

static int itemKept(int item) {
    return item >= 0 && item < keptItemsSize && keptItems[item] && !*(const unsigned char *)GAME_PTR(RVA_BATTLE_IS_REPLAY);
}

static int growKeptItems(void) {
    int records = tableRecords(TABLE_ITEM);
    if (keptItemsSize >= records) return 1;
    unsigned char *grown = realloc(keptItems, (size_t)records);
    if (!grown) return 0;
    memset(grown + keptItemsSize, 0, (size_t)(records - keptItemsSize));
    keptItems = grown;
    keptItemsSize = records;
    return 1;
}

static void indestructibleBeforePass(int level) {
    if (level == 2 && keptItems) memset(keptItems, 0, (size_t)keptItemsSize);
}

static void indestructibleCopy(int source, int target) {
    if (target < 0 || !growKeptItems() || target >= keptItemsSize) return;
    keptItems[target] = (unsigned char)itemKept(source);
}

static void indestructibleClear(int item) {
    if (item >= 0 && item < keptItemsSize) keptItems[item] = 0;
}

// ---- The ground ----------------------------------------------------------------------------------------------

static short *groundItemsAt(int x, int y) {
    return (short *)(gameBase + RVA_GROUND_ITEMS) + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * GROUND_ITEMS_PER_SQUARE;
}

static int squareOnMap(int x, int y) {
    return x >= 0 && y >= 0 && x < G32(RVA_MAP_W) && y < G32(RVA_MAP_H) && y < MAP_COLUMN_SQUARES;
}

static int putGroundItem(int x, int y, int item) {
    short *slots = groundItemsAt(x, y);
    for (int i = 0; i < GROUND_ITEMS_PER_SQUARE; i++) {
        if (slots[i] >= 0) continue;
        slots[i] = (short)item;
        if (i + 1 < GROUND_ITEMS_PER_SQUARE) slots[i + 1] = -1;
        return 1;
    }
    return 0;
}

#define INDESTRUCTIBLE_RINGS 40

typedef int (*PlaneAtFn)(int x, int y);
static PlaneAtFn indestructiblePlaneAt;

static void noteClientGround(int holder, int x, int y);

static int dropKeptItem(int x, int y, int item, int holder, const char *why) {
    if (!squareOnMap(x, y)) {
        writeLogWarning("indestructible_item_lost", "item=%d x=%d y=%d why=%s reason=no_square", item, x, y, why);
        return 0;
    }
    int plane = indestructiblePlaneAt(x, y);
    for (int r = 0; r <= INDESTRUCTIBLE_RINGS; r++) {
        for (int dx = -r; dx <= r; dx++) {
            for (int dy = -r; dy <= r; dy++) {
                if (dx > -r && dx < r && dy > -r && dy < r) continue;
                int sx = x + dx, sy = y + dy;
                if (!squareOnMap(sx, sy) || (r && indestructiblePlaneAt(sx, sy) != plane)) continue;
                if (!putGroundItem(sx, sy, item)) continue;
                writeLog("indestructible_item_grounded", "item=%d x=%d y=%d why=%s ring=%d", item, sx, sy, why, r);
                noteClientGround(holder, sx, sy);
                return 1;
            }
        }
    }
    writeLogWarning("indestructible_item_lost", "item=%d x=%d y=%d why=%s reason=no_room rings=%d", item, x, y, why, INDESTRUCTIBLE_RINGS);
    return 0;
}

#define ITEM_HELD_BY_BATTLE_X (-2)

static int battleAftermath;

static int unitGroundSquare(int unit, int *x, int *y) {
    size_t at = (size_t)unit * UNIT_SIZE;
    int ux = G32(RVA_UNIT_X + at), uy = G32(RVA_UNIT_X + 4 + at);
    if (ux >= 0 && uy >= 0) {
        *x = ux, *y = uy;
        return 1;
    }
    if (ux <= ITEM_HELD_BY_BATTLE_X && (G32(RVA_IN_BATTLE) || battleAftermath)) {
        *x = G32(RVA_BATTLE_X), *y = G32(RVA_BATTLE_Y);
        return 1;
    }
    int lx = (short)G16(RVA_UNIT_LAST_X + at), ly = (short)G16(RVA_UNIT_LAST_X + 2 + at);
    if (ux == ITEM_LOOT_IN_FIGHT_X && squareOnMap(lx, ly)) {
        *x = lx, *y = ly;
        return 1;
    }
    int hx = (short)G16(RVA_UNIT_HOME_X + at), hy = (short)G16(RVA_UNIT_HOME_X + 2 + at);
    if (squareOnMap(hx, hy)) {
        *x = hx, *y = hy;
        return 1;
    }
    return 0;
}

static int groundUnitKeptItemsAt(int unit, int x, int y, int have, const char *why) {
    if (unit < 0 || unit >= UNIT_TABLE_MAX) return 0;
    int moved = 0;
    for (int p = 0; p < ITEM_POSITION_MAX; p++) {
        int item = unitItemAt(unit, p);
        if (!itemKept(item)) continue;
        if (have < 0) have = unitGroundSquare(unit, &x, &y);
        if (!have) {
            writeLogWarning("indestructible_item_lost", "item=%d unit=%d why=%s reason=no_square", item, unit, why);
            continue;
        }
        if (!dropKeptItem(x, y, item, unit, why)) continue;
        setUnitItemAt(unit, p, -1);
        moved++;
    }
    if (moved) flushUnitItemCaches(unit);
    return moved;
}

static int groundUnitKeptItems(int unit, const char *why) { return groundUnitKeptItemsAt(unit, -1, -1, -1, why); }

static void keptItemsBefore(int unit, short kept[ITEM_POSITION_MAX]) {
    for (int p = 0; p < ITEM_POSITION_MAX; p++) {
        int item = indestructibleState > 0 ? unitItemAt(unit, p) : -1;
        kept[p] = (short)(itemKept(item) ? item : -1);
    }
}

static void keptItemsAfter(int unit, const short kept[ITEM_POSITION_MAX]) {
    int x = -1, y = -1, have = -1;
    for (int p = 0; p < ITEM_POSITION_MAX; p++) {
        if (kept[p] < 0 || unitItemAt(unit, p) == kept[p]) continue;
        if (have < 0) have = unitGroundSquare(unit, &x, &y);
        if (have) dropKeptItem(x, y, kept[p], unit, "no_longer_fits");
        else writeLogWarning("indestructible_item_lost", "item=%d unit=%d why=no_longer_fits reason=no_square", kept[p], unit);
    }
}

// ---- The catches ---------------------------------------------------------------------------------------------

typedef int (*FoundItemFn)(int x, int y, int item, int kind, int message);
static FoundItemFn gameFoundItemFn;

static __attribute__((unused)) int indestructibleFoundItemHook(int x, int y, int item, int kind, int message) {
    int took = gameFoundItemFn(x, y, item, kind, message);
    if (took <= 0 && itemKept(item)) dropKeptItem(x, y, item, -1, "no_taker");
    return took;
}

static int groundLootKeptItems(int x, int y) {
    int last = G32(RVA_UNIT_COUNT), moved = 0;
    for (int u = 0; u <= last && u < UNIT_TABLE_MAX; u++) {
        size_t at = (size_t)u * UNIT_SIZE;
        int ux = G32(RVA_UNIT_X + at), uy = G32(RVA_UNIT_X + 4 + at), hp = (short)G16(RVA_UNIT_HP + at);
        int dead = ux == x && uy == y && hp <= 0;
        if (dead || ux == ITEM_LOOT_IN_FIGHT_X || (ux <= ITEM_HELD_BY_BATTLE_X && hp <= 0)) moved += groundUnitKeptItems(u, "loot");
    }
    return moved;
}

typedef void (*RebuildArmyFn)(int x, int y);
static __attribute__((unused)) void lootRebuildHook(int x, int y) {
    GAME_FN(RebuildArmyFn, RVA_REBUILD_SQUARE_ARMY)(x, y);
    groundLootKeptItems(x, y);
    battleAftermath = 0;
}

static uint64_t (*nextIndestructibleFight)(uint64_t x, uint64_t y);
static __attribute__((unused)) uint64_t indestructibleFightHook(uint64_t x, uint64_t y) {
    battleAftermath = 0;
    uint64_t winner = nextIndestructibleFight(x, y);
    battleAftermath = !*(const unsigned char *)GAME_PTR(RVA_BATTLE_IS_REPLAY);
    return winner;
}

static int indestructiblePromoting;
static uint64_t (*nextIndestructibleRemoveUnit)(uint64_t unit);
static __attribute__((unused)) uint64_t indestructibleRemoveUnitHook(uint64_t unit) {
    if (!indestructiblePromoting) groundUnitKeptItems((int)unit, "removed");
    return nextIndestructibleRemoveUnit(unit);
}

static uint64_t (*nextIndestructibleDamage)(uint64_t unit, uint64_t damage);
static __attribute__((unused)) uint64_t indestructibleDamageHook(uint64_t unit, uint64_t damage) {
    int u = (int)unit, held = u >= 0 && u < UNIT_TABLE_MAX ? G32(RVA_UNIT_X + (size_t)u * UNIT_SIZE) : 0;
    uint64_t result = nextIndestructibleDamage(unit, damage);
    size_t at = (size_t)u * UNIT_SIZE;
    if (held <= ITEM_HELD_BY_BATTLE_X && (short)G16(RVA_UNIT_HP + at) <= 0 && G32(RVA_UNIT_X + at) == -1 &&
        (G32(RVA_IN_BATTLE) || battleAftermath))
        groundUnitKeptItemsAt(u, G32(RVA_BATTLE_X), G32(RVA_BATTLE_Y), 1, "died_held_by_battle");
    return result;
}

static uint64_t (*nextIndestructiblePromote)(uint64_t owner, uint64_t unit, uint64_t monster, uint64_t commander);
static __attribute__((unused)) uint64_t indestructiblePromoteHook(uint64_t owner, uint64_t unit, uint64_t monster, uint64_t commander) {
    indestructiblePromoting++;
    uint64_t result = nextIndestructiblePromote(owner, unit, monster, commander);
    indestructiblePromoting--;
    return result;
}

#ifndef __APPLE__
static void indestructibleNewUnitBefore(const CallInfo *call) {
    int monster = (int)call->args[4];
    if (!keptItemsSize || monster < 0 || monster >= MONSTER_TABLE_MAX || monsterAbility(monster, MONSTER_ABILITY_UNIQUE) <= 0) return;
    int preview = testFieldMonsterPreview.constructing && GetCurrentThreadId() == testFieldMonsterPreview.thread &&
                  monster == testFieldMonsterPreview.monster;
    if (preview || testFieldAllowsSecondUnique()) return;
    int unit = uniqueGroupUnitFirst(monster);
    if (unit >= 0) groundUnitKeptItems(unit, "unique_remade");
}
#endif

static int groundDeadKeptItems(void) {
    int last = G32(RVA_UNIT_COUNT), moved = 0;
    for (int u = 0; u <= last && u < UNIT_TABLE_MAX; u++) {
        size_t at = (size_t)u * UNIT_SIZE;
        if ((short)G16(RVA_UNIT_HP + at) <= 0 && G32(RVA_UNIT_X + at) >= 0) moved += groundUnitKeptItems(u, "dead_at_month");
    }
    return moved;
}
static uint64_t (*nextIndestructibleRecycle)(void);
static __attribute__((unused)) uint64_t indestructibleRecycleHook(void) {
    groundDeadKeptItems();
    return nextIndestructibleRecycle();
}

#ifndef __APPLE__
typedef void (*ClearUnitRecordFn)(int unit);
static void clearRecordHook(int unit) {
    groundUnitKeptItems(unit, "record_cleared");
    GAME_FN(ClearUnitRecordFn, RVA_CLEAR_UNIT_RECORD)(unit);
}
#endif

// ---- A LAN client's ground -----------------------------------------------------------------------------------

#define CLIENT_GROUND_MAX 512

static struct { short x, y; int unit, turn, recordSent; } clientGround[CLIENT_GROUND_MAX];
static int clientGroundCount;

typedef void (*SendSquareFn)(int player, int x, int y);
static SendSquareFn indestructibleSendSquare;
typedef int (*TwoHIncludesFn)(int player, int unit);
static TwoHIncludesFn indestructibleTwoHIncludes;

static void noteClientGround(int holder, int x, int y) {
    if (holder < 0 || !isNetworkClient()) return;
    int turn = G32(RVA_TURN);
    for (int i = 0; i < clientGroundCount; i++)
        if (clientGround[i].unit == holder && clientGround[i].x == x && clientGround[i].y == y && clientGround[i].turn == turn) {
            clientGround[i].recordSent = 0;
            return;
        }
    if (clientGroundCount >= CLIENT_GROUND_MAX) {
        writeLogWarning("indestructible_square_not_sent", "unit=%d x=%d y=%d reason=list_full max=%d", holder, x, y, CLIENT_GROUND_MAX);
        return;
    }
    clientGround[clientGroundCount].x = (short)x, clientGround[clientGroundCount].y = (short)y;
    clientGround[clientGroundCount].unit = holder, clientGround[clientGroundCount].turn = turn, clientGround[clientGroundCount].recordSent = 0;
    clientGroundCount++;
}

static void clientGroundRecordSent(int unit) {
    for (int i = 0; i < clientGroundCount; i++)
        if (clientGround[i].unit == unit) clientGround[i].recordSent = 1;
}

static int sendClientGround(int player) {
    int turn = G32(RVA_TURN), sent = 0;
    for (int i = 0; i < clientGroundCount; i++) {
        if (clientGround[i].turn != turn) continue;
        int x = clientGround[i].x, y = clientGround[i].y, unit = clientGround[i].unit;
        if (!clientGround[i].recordSent && indestructibleTwoHIncludes(player, unit) <= 0) {
            writeLog("indestructible_square_not_sent", "unit=%d x=%d y=%d reason=host_keeps_holder", unit, x, y);
            continue;
        }
        const char *by = clientGround[i].recordSent ? "record" : "2h";
        for (int j = i + 1; j < clientGroundCount; j++)
            if (clientGround[j].x == x && clientGround[j].y == y) clientGround[j].turn = turn - 1;
        indestructibleSendSquare(player, x, y);
        sent++;
        writeLog("indestructible_square_sent", "player=%d x=%d y=%d unit=%d by=%s", player, x, y, unit, by);
    }
    clientGroundCount = 0;
    return sent;
}

static uint64_t (*gamePutFullUnitFn)(uint64_t unit);
static __attribute__((unused)) uint64_t indestructiblePutFullUnitHook(uint64_t unit) {
    if (isNetworkClient()) clientGroundRecordSent((int)unit);
    return gamePutFullUnitFn(unit);
}

static uint64_t (*gameSavePlayer2hFn)(uint64_t player, uint64_t path);
static __attribute__((unused)) uint64_t indestructibleSave2hHook(uint64_t player, uint64_t path) {
    if (isNetworkClient()) sendClientGround((int)player);
    return gameSavePlayer2hFn(player, path);
}

// ---- The square's info window --------------------------------------------------------------------------------

static int groundInfoX = -1, groundInfoY = -1;
static uint64_t (*nextGroundInfoScreen)(int player, int x, int y, int mode);
static __attribute__((unused)) uint64_t groundInfoScreen(int player, int x, int y, int mode) {
    groundInfoX = x, groundInfoY = y;
    uint64_t result = nextGroundInfoScreen(player, x, y, mode);
    groundInfoX = groundInfoY = -1;
    return result;
}

static uint64_t (*nextGroundInfoRow)(short style);
static __attribute__((unused)) uint64_t groundInfoRow(short style) {
    if (squareOnMap(groundInfoX, groundInfoY)) {
        const short *slots = groundItemsAt(groundInfoX, groundInfoY);
        for (int i = 0; i < GROUND_ITEMS_PER_SQUARE && slots[i] >= 0; i++) {
            if (slots[i] >= tableRecords(TABLE_ITEM)) break;
            char line[96];
            snprintf(line, sizeof line, "Magic item here: %.47s", (const char *)(tableAddress(TABLE_ITEM) + (size_t)slots[i] * ITEM_SIZE));
            textButtonAsIs(0, line);
        }
    }
    return nextGroundInfoRow(style);
}

// ---- Installing ----------------------------------------------------------------------------------------------

static int indestructiblePlaneAtGame(int x, int y) { return GAME_FN(PlaneAtFn, RVA_PLANE_AT)(x, y); }

// @platform-split indestructible: Windows only for now; Mac inlines the record clears and hooks the rebuild through its callers
static int installIndestructible(void) {
    if (indestructibleState) return indestructibleState > 0;
    if (!indestructiblePlaneAt) indestructiblePlaneAt = indestructiblePlaneAtGame;
#ifdef __APPLE__
    indestructibleState = -1;
    writeLogWarning("indestructible_not_installed", "reason=windows_first");
    return 0;
#else
    char owner[64];
    moduleFileName((void *)groundInfoRow, owner, sizeof owner);
    void *found = gameBase + RVA_FOUND_ITEM;
    int entries = fanpatchHookCreate(found, (void *)indestructibleFoundItemHook, (void **)&gameFoundItemFn) && fanpatchHookEnable(found, 1);
    entries += installEnforceItemsHook();
    indestructibleSendSquare = GAME_FN(SendSquareFn, RVA_NET_SEND_SQUARE);
    indestructibleTwoHIncludes = GAME_FN(TwoHIncludesFn, RVA_2H_UNIT_INCLUDED);
    void *putUnit = gameBase + RVA_NET_PUT_FULL_UNIT, *save2h = gameBase + RVA_SAVE_PLAYER_2H;
    entries += fanpatchHookCreate(putUnit, (void *)indestructiblePutFullUnitHook, (void **)&gamePutFullUnitFn) && fanpatchHookEnable(putUnit, 1);
    entries += fanpatchHookCreate(save2h, (void *)indestructibleSave2hHook, (void **)&gameSavePlayer2hFn) && fanpatchHookEnable(save2h, 1);
    int sites = wrapSite(SITE_REMOVE_UNIT, (void *)indestructibleRemoveUnitHook, (void **)&nextIndestructibleRemoveUnit) +
                wrapSite(SITE_PROMOTE_UNIT, (void *)indestructiblePromoteHook, (void **)&nextIndestructiblePromote) +
                wrapSite(SITE_FIGHT_THE_FIGHT, (void *)indestructibleFightHook, (void **)&nextIndestructibleFight) +
                wrapSite(SITE_DAMAGE_UNIT, (void *)indestructibleDamageHook, (void **)&nextIndestructibleDamage) +
                wrapSite(SITE_RECYCLE_UNITS, (void *)indestructibleRecycleHook, (void **)&nextIndestructibleRecycle) +
                apiObserve(SITE_NEW_UNIT, indestructibleNewUnitBefore, NULL) +
                wrapSite(SITE_SQUARE_INFO_SCREEN, (void *)groundInfoScreen, (void **)&nextGroundInfoScreen) +
                wrapSite(SITE_SQUARE_INFO_BUTTON_ROW, (void *)groundInfoRow, (void **)&nextGroundInfoRow);
    static const struct { unsigned int rva, target; const char *label; } calls[] = {
        {RVA_LOOT_REBUILD_ARMY_CALL, RVA_REBUILD_SQUARE_ARMY, "indestructible_loot"},
        {RVA_RECYCLE_CLEAR_RECORD_CALL, RVA_CLEAR_UNIT_RECORD, "indestructible_recycle"},
        {RVA_ELIMINATE_CLEAR_RECORD_A, RVA_CLEAR_UNIT_RECORD, "indestructible_eliminate_a"},
        {RVA_ELIMINATE_CLEAR_RECORD_B, RVA_CLEAR_UNIT_RECORD, "indestructible_eliminate_b"},
        {RVA_OVERPOP_CLEAR_RECORD_A, RVA_CLEAR_UNIT_RECORD, "indestructible_overpop_a"},
        {RVA_OVERPOP_CLEAR_RECORD_B, RVA_CLEAR_UNIT_RECORD, "indestructible_overpop_b"},
    };
    int redirects = 0, callCount = (int)(sizeof calls / sizeof calls[0]);
    for (int i = 0; i < callCount; i++)
        redirects += redirectBattleCall(calls[i].rva, calls[i].target, i ? (void *)clearRecordHook : (void *)lootRebuildHook, calls[i].label);
    int ok = entries + sites + redirects, parts = 4 + 8 + callCount;
    indestructibleState = ok == parts ? 1 : -1;
    writeInstallLogFields("indestructible_installed", ok, parts, "entries=%d sites=%d calls=%d owner=%s", entries, sites, redirects, owner);
    return indestructibleState > 0;
#endif
}

static int indestructibleLine(int item, int on, int mod) {
    if (!growKeptItems()) {
        refuseC5mLine("indestructible", spellModName(mod), "out_of_memory", "line=%d", G32(RVA_MOD_LINE));
        return 1;
    }
    keptItems[item] = (unsigned char)(on != 0);
    if (on) installIndestructible();
    writeLog("item_word_added", "word=indestructible value=%d item=%d mod=%d", on != 0, item, mod);
    return 1;
}
