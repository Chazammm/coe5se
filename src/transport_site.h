#pragma once

#define CARRIED_SITES_MAX 256
#define CARRIED_SQUARE_FLAGS (RVA_SQUARE_FLAGS - RVA_SQUARE_RECORDS)
#define CARRIED_OWN (-1)

typedef struct {
    int x, y, terrain;
    int own;
    short realTerrain;
    signed char realOwner;
    int realFlags;
    int in;
} CarriedSquare;

typedef struct {
    int set, offer, x, y, source;
} CarriedPick;

typedef struct {
    int unit, terrain, square;
} CarriedVehicle;

static struct {
    int depth, player, count, full;
    CarriedSquare squares[CARRIED_SITES_MAX];
    CarriedVehicle *vehicles;
    int vehicleCount, vehicleCapacity;
    short orders[AUTO_RECRUIT_ORDERS_MAX][3];
    int screen, waitSeen;
    CarriedPick pick;
} carriedSites;
static int carriedSiteHooks;
static int (*carriedOwnPlaceForTests)(int player, int x, int y);
static int (*carriedTerrainMatchForTests)(int code, int x, int y);
static int (*carriedOfferAvailableForTests)(int player, int offer, int x, int y);

typedef struct {
    int unit, turn, monster;
} CarriedMark;
static CarriedMark *carriedMarks;
static int carriedMarkCount, carriedMarkCapacity;

typedef struct {
    int player, order, unit, monster;
} CarriedOrder;
static CarriedOrder *carriedOrders;
static int carriedOrderCount, carriedOrderCapacity;

static int carriedGrow(void **items, int count, int *capacity, size_t size, const char *what) {
    if (count < *capacity) return 1;
    int grown = *capacity ? *capacity * 2 : 16;
    void *bigger = realloc(*items, (size_t)grown * size);
    if (!bigger) {
        writeLogError("carried_sites_refused", "reason=out_of_memory list=%s", what);
        return 0;
    }
    *items = bigger, *capacity = grown;
    return 1;
}

static unsigned char *carriedSquareRecord(int x, int y) {
    return GAME_PTR(RVA_SQUARE_RECORDS) + ((size_t)x * MAP_COLUMN_SQUARES + (size_t)y) * SQUARE_SIZE;
}

static int carriedSquareOnMap(int x, int y) {
    return x >= 0 && y >= 0 && x < G32(RVA_MAP_DEPTH) && x < G32(RVA_MAP_W) && y < G32(RVA_MAP_H) && y < MAP_COLUMN_SQUARES;
}

static int carriedTerrainOf(const TransportEntry *e) {
    if (!e || !e->terrain[0]) return -1;
    if (isOfficialGame() || (isNetworkGame() && !e->fromWords && modClassNow(e->mod) != LUA_CLASS_CONTENT)) return -1;
    int last = G32(RVA_LAST_TERRAIN);
    if (e->terrainIsNumber) {
        long long n = atoll(e->terrain);
        return n >= 0 && n <= last && n < TERRAIN_TABLE_MAX ? (int)n : -1;
    }
    for (int t = 0; t <= last && t < TERRAIN_TABLE_MAX; t++)
        if (recordNameIs(GAME_PTR(RVA_TERRAIN_TABLE) + (size_t)t * TERRAIN_SIZE, TERRAIN_NAME_SIZE, e->terrain)) return t;
    return -1;
}

static int carriedUnitTerrain(int unit, int player) {
    if (unit < 0 || unit >= UNIT_TABLE_MAX || unit > G32(RVA_UNIT_COUNT)) return -1;
    size_t at = (size_t)unit * UNIT_SIZE;
    int monster = (short)G16(RVA_UNIT_MONSTER + at);
    if (monster < 0 || (short)G16(RVA_UNIT_HP + at) <= 0 || (short)G16(RVA_UNIT_PLAYER + at) != player) return -1;
    return carriedTerrainOf(transportEntryForMonster(monster));
}

static int carriedSquareIsOwnPlace(int player, int x, int y) {
    if (carriedOwnPlaceForTests) return carriedOwnPlaceForTests(player, x, y);
    return (signed char)carriedSquareRecord(x, y)[SQUARE_OWNER] == player && squareCitadelFor(player, x, y);
}

static int carriedSitesHuman(int player) {
    if (player < 0 || player >= PLAYER_SLOTS) return 0;
    short controller;
    memcpy(&controller, playerSlotAddress(RVA_SLOT_CONTROLLER, player, 2), 2);
    return controller == 0 || controller == CONTROLLER_REMOTE_HUMAN;
}

// ---- A vehicle's month ---------------------------------------------------------------------------
static int carriedVehicleMarked(int unit) {
    int turn = G32(RVA_TURN), monster = (short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE);
    for (int i = 0; i < carriedMarkCount; i++)
        if (carriedMarks[i].unit == unit && carriedMarks[i].turn == turn && carriedMarks[i].monster == monster) return 1;
    return 0;
}

static int carriedSetMark(int unit) {
    int turn = G32(RVA_TURN), monster = (short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE);
    CarriedMark *slot = NULL;
    for (int i = 0; i < carriedMarkCount && !slot; i++)
        if (carriedMarks[i].unit == unit || carriedMarks[i].turn != turn) slot = &carriedMarks[i];
    if (!slot) {
        if (!carriedGrow((void **)&carriedMarks, carriedMarkCount, &carriedMarkCapacity, sizeof *carriedMarks, "marks")) return 0;
        slot = &carriedMarks[carriedMarkCount++];
    }
    *slot = (CarriedMark){unit, turn, monster};
    return 1;
}

static void carriedMarkVehicle(int unit) {
    if (carriedSetMark(unit)) writeLog("carried_site_bought", "vehicle=%d turn=%d", unit, G32(RVA_TURN));
}

// ---- The sources of a place ----------------------------------------------------------------------
static int carriedSquareIndex(int x, int y) {
    if (!carriedSites.depth) return -1;
    for (int i = 0; i < carriedSites.count; i++)
        if (carriedSites.squares[i].x == x && carriedSites.squares[i].y == y) return i;
    return -1;
}

static int carriedSourceTerrain(const CarriedSquare *c, int source) {
    return source == CARRIED_OWN ? c->realTerrain : carriedSites.vehicles[source].terrain;
}

static int carriedSourceUsed(const CarriedSquare *c, int source) {
    return source == CARRIED_OWN ? (c->realFlags & SQUARE_FLAG_RECRUITED) != 0 : carriedVehicleMarked(carriedSites.vehicles[source].unit);
}

static int carriedGameTerrainMatch(int code, int x, int y) {
    if (carriedTerrainMatchForTests) return carriedTerrainMatchForTests(code, x, y);
    return GAME_FN(int (*)(int, int, int), RVA_TERRAIN_MATCH)(code, x, y);
}

static int carriedMatchAs(int code, int x, int y, int terrain) {
    unsigned char *r = carriedSquareRecord(x, y);
    short now, as = (short)terrain;
    memcpy(&now, r + SQUARE_TERRAIN_TYPE, 2);
    memcpy(r + SQUARE_TERRAIN_TYPE, &as, 2);
    int match = carriedGameTerrainMatch(code, x, y);
    memcpy(r + SQUARE_TERRAIN_TYPE, &now, 2);
    return match;
}

static int carriedSourceOffers(const CarriedSquare *c, int source, int player, int offer) {
    int code = PSLOT32(RVA_SHOP_OFFER_TERRAIN + (size_t)offer * 4, player, PLAYER_BLOCK_SIZE);
    return !code || carriedMatchAs(code, c->x, c->y, carriedSourceTerrain(c, source));
}

static int carriedNextSource(int square, int source) {
    const CarriedSquare *c = &carriedSites.squares[square];
    if (source == -2 && c->own) return CARRIED_OWN;
    for (int v = source < 0 ? 0 : source + 1; v < carriedSites.vehicleCount; v++)
        if (carriedSites.vehicles[v].square == square) return v;
    return -2;
}

static int carriedBuySource(int square, int player, int offer, int *used) {
    const CarriedSquare *c = &carriedSites.squares[square];
    int first = -2, any = -2;
    for (int s = carriedNextSource(square, -2); s != -2; s = carriedNextSource(square, s)) {
        if (any == -2) any = s;
        if (offer < 0 || offer >= SHOP_OFFERS_MAX || !carriedSourceOffers(c, s, player, offer)) continue;
        if (first == -2) first = s;
        if (!carriedSourceUsed(c, s)) return *used = 0, s;
    }
    int s = first != -2 ? first : any;
    *used = s == -2 ? 0 : carriedSourceUsed(c, s);
    return s;
}

// ---- The reads ----------------------------------------------------------------------------------
static void carriedReadAs(int square, int source) {
    CarriedSquare *c = &carriedSites.squares[square];
    unsigned char *r = carriedSquareRecord(c->x, c->y);
    short t = (short)(source == -2 ? c->terrain : carriedSourceTerrain(c, source));
    int used = 1;
    if (source != -2) used = carriedSourceUsed(c, source);
    else
        for (int s = carriedNextSource(square, -2); s != -2 && used; s = carriedNextSource(square, s)) used = carriedSourceUsed(c, s);
    int flags;
    memcpy(&flags, r + CARRIED_SQUARE_FLAGS, 4);
    flags = (flags & ~SQUARE_FLAG_RECRUITED) | (used ? SQUARE_FLAG_RECRUITED : 0);
    memcpy(r + SQUARE_TERRAIN_TYPE, &t, 2);
    r[SQUARE_OWNER] = (unsigned char)(signed char)carriedSites.player;
    memcpy(r + CARRIED_SQUARE_FLAGS, &flags, 4);
}

static void carriedSitesIn(void) {
    for (int i = 0; i < carriedSites.count; i++) {
        CarriedSquare *c = &carriedSites.squares[i];
        if (c->in) continue;
        unsigned char *r = carriedSquareRecord(c->x, c->y);
        memcpy(&c->realTerrain, r + SQUARE_TERRAIN_TYPE, 2);
        c->realOwner = (signed char)r[SQUARE_OWNER];
        memcpy(&c->realFlags, r + CARRIED_SQUARE_FLAGS, 4);
        if (c->own) c->terrain = c->realTerrain;
        carriedReadAs(i, -2);
        c->in = 1;
    }
}

static void carriedSitesOut(void) {
    for (int i = carriedSites.count - 1; i >= 0; i--) {
        CarriedSquare *c = &carriedSites.squares[i];
        if (!c->in) continue;
        unsigned char *r = carriedSquareRecord(c->x, c->y);
        memcpy(r + SQUARE_TERRAIN_TYPE, &c->realTerrain, 2);
        r[SQUARE_OWNER] = (unsigned char)c->realOwner;
        int flags;
        memcpy(&flags, r + CARRIED_SQUARE_FLAGS, 4);
        flags = (flags & ~SQUARE_FLAG_RECRUITED) | (c->realFlags & SQUARE_FLAG_RECRUITED);
        memcpy(r + CARRIED_SQUARE_FLAGS, &flags, 4);
        c->in = 0;
    }
}

// ---- Orders that follow their vehicle -----------------------------------------------------------
static short *carriedOrderField(int player, int i, int field) {
    if (field < 2) return &PSLOT16(RVA_AUTO_RECRUIT_SQUARES + (size_t)i * 4 + (size_t)field * 2, player, PLAYER_BLOCK_SIZE);
    return &PSLOT16(RVA_AUTO_RECRUIT_MONSTERS + (size_t)i * 2, player, PLAYER_BLOCK_SIZE);
}

static void carriedDropOrder(int player, int i) {
    for (int k = 0; k < carriedOrderCount; k++)
        if (carriedOrders[k].player == player && carriedOrders[k].order == i) {
            carriedOrders[k] = carriedOrders[--carriedOrderCount];
            return;
        }
}

static int carriedOfferOf(int player, int monster) {
    int last = PSLOT32(RVA_SHOP_LAST, player, PLAYER_BLOCK_SIZE);
    for (int e = 0; e <= last && e < SHOP_OFFERS_MAX; e++)
        if (PSLOT32(RVA_SHOP_OFFERS + (size_t)e * 4, player, PLAYER_BLOCK_SIZE) == monster && PSLOT32(RVA_SHOP_OFFER_COUNTS + (size_t)e * 4, player, PLAYER_BLOCK_SIZE) > 0) return e;
    return -1;
}

static void carriedLinkOrder(int player, int i, int square, int monster) {
    const CarriedSquare *c = &carriedSites.squares[square];
    int offer = carriedOfferOf(player, monster), vehicle = -2;
    for (int s = carriedNextSource(square, -2); s != -2 && vehicle == -2; s = carriedNextSource(square, s)) {
        if (offer >= 0 && !carriedSourceOffers(c, s, player, offer)) continue;
        if (s == CARRIED_OWN) return;
        vehicle = s;
    }
    if (vehicle == -2) return;
    if (!carriedGrow((void **)&carriedOrders, carriedOrderCount, &carriedOrderCapacity, sizeof *carriedOrders, "orders")) return;
    carriedOrders[carriedOrderCount++] = (CarriedOrder){player, i, carriedSites.vehicles[vehicle].unit, monster};
    writeLog("carried_order_linked", "player=%d order=%d vehicle=%d monster=%d", player, i, carriedSites.vehicles[vehicle].unit, monster);
}

static void carriedOrdersSeen(void) {
    int p = carriedSites.player;
    if (!carriedSites.depth || p < 0 || p >= PLAYER_SLOTS) return;
    for (int i = 0; i < AUTO_RECRUIT_ORDERS_MAX; i++) {
        short x = *carriedOrderField(p, i, 0), y = *carriedOrderField(p, i, 1), m = *carriedOrderField(p, i, 2);
        short *seen = carriedSites.orders[i];
        if (seen[0] == x && seen[1] == y && seen[2] == m) continue;
        seen[0] = x, seen[1] = y, seen[2] = m;
        carriedDropOrder(p, i);
        int square = x || y ? carriedSquareIndex(x, y) : -1;
        if (square >= 0 && m > 0) carriedLinkOrder(p, i, square, m);
    }
}

static void carriedOrdersFollow(int player) {
    int moved = 0;
    for (int k = carriedOrderCount - 1; k >= 0; k--) {
        CarriedOrder *o = &carriedOrders[k];
        if (o->player != player) continue;
        short *x = carriedOrderField(player, o->order, 0), *y = carriedOrderField(player, o->order, 1), m = *carriedOrderField(player, o->order, 2);
        size_t at = (size_t)o->unit * UNIT_SIZE;
        if (m != o->monster || (!*x && !*y) || carriedUnitTerrain(o->unit, player) < 0) {
            writeLog("carried_order_unlinked", "player=%d order=%d vehicle=%d", player, o->order, o->unit);
            carriedOrders[k] = carriedOrders[--carriedOrderCount];
            continue;
        }
        int vx = (int)G32(RVA_UNIT_X + at), vy = (int)G32(RVA_UNIT_X + 4 + at);
        if (!carriedSquareOnMap(vx, vy) || (*x == vx && *y == vy)) continue;
        writeLog("carried_order_moved", "player=%d order=%d vehicle=%d from=%d,%d to=%d,%d", player, o->order, o->unit, *x, *y, vx, vy);
        *x = (short)vx, *y = (short)vy;
        moved = 1;
    }
    if (moved) GAME_FN(void (*)(int), RVA_NET_SEND_AUTO_RECRUIT)(player);
}

// ---- The step ------------------------------------------------------------------------------------
static int carriedAddVehicle(int unit, int terrain, int x, int y) {
    int square = -1;
    for (int i = 0; i < carriedSites.count && square < 0; i++)
        if (carriedSites.squares[i].x == x && carriedSites.squares[i].y == y) square = i;
    if (square < 0) {
        if (carriedSites.count == CARRIED_SITES_MAX) return 0;
        square = carriedSites.count++;
        carriedSites.squares[square] = (CarriedSquare){.x = x, .y = y, .terrain = terrain, .own = carriedSquareIsOwnPlace(carriedSites.player, x, y)};
    }
    if (!carriedGrow((void **)&carriedSites.vehicles, carriedSites.vehicleCount, &carriedSites.vehicleCapacity, sizeof *carriedSites.vehicles, "vehicles"))
        return 1;
    carriedSites.vehicles[carriedSites.vehicleCount++] = (CarriedVehicle){unit, terrain, square};
    return 1;
}

static void beginCarriedSites(int player) {
    if (carriedSites.depth++) return;
    carriedSites.count = carriedSites.full = carriedSites.vehicleCount = carriedSites.pick.set = 0;
    carriedSites.player = player;
    if (!transportEntryCount || player < 0 || player >= PLAYER_SLOTS) return;
    carriedOrdersFollow(player);
    int last = G32(RVA_UNIT_COUNT);
    for (int u = 0; u <= last && u < UNIT_TABLE_MAX; u++) {
        int terrain = carriedUnitTerrain(u, player);
        size_t at = (size_t)u * UNIT_SIZE;
        int x = terrain < 0 ? -1 : (int)G32(RVA_UNIT_X + at), y = terrain < 0 ? -1 : (int)G32(RVA_UNIT_X + 4 + at);
        if (terrain < 0 || !carriedSquareOnMap(x, y)) continue;
        if (!carriedAddVehicle(u, terrain, x, y)) {
            carriedSites.full = 1;
            break;
        }
    }
    if (carriedSites.full) writeLogWarning("carried_sites_cut", "reason=too_many player=%d max=%d", player, CARRIED_SITES_MAX);
    for (int i = 0; i < AUTO_RECRUIT_ORDERS_MAX; i++)
        for (int f = 0; f < 3; f++) carriedSites.orders[i][f] = *carriedOrderField(player, i, f);
    carriedSitesIn();
}

static void endCarriedSites(void) {
    if (carriedSites.depth <= 0) return;
    if (carriedSites.depth == 1) carriedOrdersSeen();
    if (--carriedSites.depth) return;
    carriedSitesOut();
    carriedSites.count = carriedSites.vehicleCount = carriedSites.pick.set = 0;
}

// ---- The hooks -----------------------------------------------------------------------------------
static void (*nextCarriedAutoRecruit)(void);
static int (*nextCarriedRecruitWait)(int a, int b);
static int (*nextCarriedRecruitBuy)(int player, int offer, int x, int y);

static void carriedRecruitScreenHook(int player, int x, int y) {
    beginCarriedSites(player);
    int screen = carriedSites.screen, seen = carriedSites.waitSeen;
    carriedSites.screen = 1, carriedSites.waitSeen = 0;
    GAME_FN(void (*)(int, int, int), RVA_RECRUIT_SCREEN)(player, x, y);
    carriedSites.screen = screen, carriedSites.waitSeen = seen;
    endCarriedSites();
}

static int carriedRecruitWaitHook(int a, int b) {
    carriedOrdersSeen();
    carriedSitesOut();
    carriedSites.pick.set = 0;
    int r = nextCarriedRecruitWait(a, b);
    carriedSites.waitSeen = carriedSites.screen;
    if (carriedSites.depth) carriedSitesIn();
    return r;
}

static int carriedOfferTest(int square, int source, int player, int offer, int x, int y) {
    if (square >= 0) {
        carriedReadAs(square, source);
        carriedSites.pick = (CarriedPick){1, offer, x, y, source};
    }
    int r = carriedOfferAvailableForTests ? carriedOfferAvailableForTests(player, offer, x, y)
                                          : GAME_FN(int (*)(int, int, int, int), RVA_SHOP_OFFER_AVAILABLE)(player, offer, x, y);
    if (square >= 0) carriedReadAs(square, -2);
    return r;
}

static int carriedScreenOfferHook(int player, int offer, int x, int y) {
    int square = player == carriedSites.player ? carriedSquareIndex(x, y) : -1, used = 0;
    int source = square >= 0 ? carriedBuySource(square, player, offer, &used) : -2;
    if (source == -2) return carriedOfferTest(-1, 0, player, offer, x, y);
    if (carriedSites.waitSeen) *(int *)(siteCallerSp(SITE_RECRUIT_WAIT) + FRAME_RECRUIT_SCREEN_USED) = used;
    return carriedOfferTest(square, source, player, offer, x, y);
}

static int carriedAutoOfferHook(int player, int offer, int x, int y) {
    int square = player == carriedSites.player ? carriedSquareIndex(x, y) : -1, used = 0;
    int source = square >= 0 ? carriedBuySource(square, player, offer, &used) : -2;
    if (source == -2) return carriedOfferTest(-1, 0, player, offer, x, y);
    int flags = PSLOT32(RVA_SHOP_OFFER_FLAGS + (size_t)offer * 4, player, PLAYER_BLOCK_SIZE);
    if (used && !(flags & SHOP_FLAG_EXTRA)) return -3;
    return carriedOfferTest(square, source, player, offer, x, y);
}

static int carriedTerrainMatchHook(int code, int x, int y) {
    int r = carriedGameTerrainMatch(code, x, y), square = r ? -1 : carriedSquareIndex(x, y);
    if (square < 0) return r;
    const CarriedSquare *c = &carriedSites.squares[square];
    for (int s = carriedNextSource(square, -2); s != -2 && !r; s = carriedNextSource(square, s)) r = carriedMatchAs(code, x, y, carriedSourceTerrain(c, s));
    return r;
}

static int carriedRecruitBuyHook(int player, int offer, int x, int y) {
    int square = player == carriedSites.player ? carriedSquareIndex(x, y) : -1, used = 0, source = -2;
    if (square >= 0) {
        source = carriedSites.pick.set && carriedSites.pick.offer == offer && carriedSites.pick.x == x && carriedSites.pick.y == y
                     ? carriedSites.pick.source
                     : carriedBuySource(square, player, offer, &used);
    }
    carriedSites.pick.set = 0;
    carriedSitesOut();
    int r;
    if (source >= 0 && offer >= 0 && offer < SHOP_OFFERS_MAX) {
        int *flags = &PSLOT32(RVA_SHOP_OFFER_FLAGS + (size_t)offer * 4, player, PLAYER_BLOCK_SIZE), was = *flags;
        int unit = carriedSites.vehicles[source].unit;
        *flags |= SHOP_FLAG_EXTRA;
        r = nextCarriedRecruitBuy(player, offer, x, y);
        *flags = (*flags & ~SHOP_FLAG_EXTRA) | (was & SHOP_FLAG_EXTRA);
        if (!(was & SHOP_FLAG_EXTRA)) carriedMarkVehicle(unit);
    } else {
        r = nextCarriedRecruitBuy(player, offer, x, y);
    }
    if (carriedSites.depth) carriedSitesIn();
    return r;
}

static void carriedAutoRecruitBegin(void) {
    int player = (int)G32(RVA_ACTIVE_PLAYER);
    beginCarriedSites(carriedSitesHuman(player) ? player : -1);
}
#ifndef __APPLE__
static void carriedAutoRecruitHook(void) {
    carriedAutoRecruitBegin();
    nextCarriedAutoRecruit();
    endCarriedSites();
}
#endif

static void carriedSitesShopBuild(int player, int begin) {
    if (!carriedSiteHooks) return;
    if (begin) beginCarriedSites(carriedSitesHuman(player) ? player : -1);
    else endCarriedSites();
}

#ifdef __APPLE__
// @platform-split carried_auto_recruit: Mac's auto-recruit is play_game's copy, so its split row's start and end hooks begin and end the step (Windows wraps the call)
static void carriedSplitPoint(CallSite site, int start) {
    if (site != SITE_AUTO_RECRUIT || carriedSiteHooks <= 0) return;
    if (start) carriedAutoRecruitBegin();
    else endCarriedSites();
}
#endif

static int installShopBuildCall(void);

static int installCarriedSiteHooks(void) {
    if (carriedSiteHooks) return carriedSiteHooks > 0;
    static const unsigned int SCREEN_CALLS[] = {RVA_RECRUIT_SCREEN_CALL_KEY, RVA_RECRUIT_SCREEN_CALL_INFO, RVA_RECRUIT_SCREEN_CALL_PANEL};
    static const char *const SCREEN_NAMES[] = {"recruit_screen_key", "recruit_screen_info", "recruit_screen_panel"};
    static const struct {
        unsigned int call, target;
        void *hook;
        const char *name;
    } TESTS[] = {
        {RVA_RECRUIT_ROW_OFFER_TERRAIN, RVA_TERRAIN_MATCH, (void *)carriedTerrainMatchHook, "recruit_row_offer_terrain"},
        {RVA_RECRUIT_ROW_CLASS_TERRAIN, RVA_TERRAIN_MATCH, (void *)carriedTerrainMatchHook, "recruit_row_class_terrain"},
        {RVA_RECRUIT_OFFER_TEST_CALL, RVA_SHOP_OFFER_AVAILABLE, (void *)carriedScreenOfferHook, "recruit_offer_test"},
        {RVA_AUTO_RECRUIT_OFFER_TEST_CALL, RVA_SHOP_OFFER_AVAILABLE, (void *)carriedAutoOfferHook, "auto_recruit_offer_test"},
        {RVA_SHOP_CITADEL_MATCH_CALL, RVA_TERRAIN_MATCH, (void *)carriedTerrainMatchHook, "shop_citadel_match"},
#ifdef __APPLE__
        // @platform-split shop_citadel_match: Mac inlined RVA_PLAYER_HAS_TERRAIN_CITADEL into the shop build three times, so each copy's RVA_TERRAIN_MATCH call is pointed at the hook (Windows: the function's one call)
        {RVA_SHOP_CITADEL_MATCH_CALL_2, RVA_TERRAIN_MATCH, (void *)carriedTerrainMatchHook, "shop_citadel_match_2"},
        {RVA_SHOP_CITADEL_MATCH_CALL_3, RVA_TERRAIN_MATCH, (void *)carriedTerrainMatchHook, "shop_citadel_match_3"},
#endif
    };
    int ok = 0, wanted = 0;
    for (int i = 0; i < 3; i++, wanted++)
        ok += pointCustomPlaneCall(SCREEN_CALLS[i], RVA_RECRUIT_SCREEN, (void *)carriedRecruitScreenHook, SCREEN_NAMES[i]);
    for (size_t i = 0; i < sizeof TESTS / sizeof TESTS[0]; i++, wanted++)
        ok += pointCustomPlaneCall(TESTS[i].call, TESTS[i].target, TESTS[i].hook, TESTS[i].name);
    WrapPlace outermost = {.layer = WRAP_LAYER_OUTERMOST, .gated = GATE_NONE};
    ok += insertWrap(SITE_RECRUIT_WAIT, (void *)carriedRecruitWaitHook, (void **)&nextCarriedRecruitWait, outermost, "coe5se_carried_sites", NULL);
    ok += insertWrap(SITE_RECRUIT_BUY, (void *)carriedRecruitBuyHook, (void **)&nextCarriedRecruitBuy, outermost, "coe5se_carried_sites", NULL);
    wanted += 2;
#ifndef __APPLE__
    ok += insertWrap(SITE_AUTO_RECRUIT, (void *)carriedAutoRecruitHook, (void **)&nextCarriedAutoRecruit, outermost, "coe5se_carried_sites", NULL);
#else
    ok += siteInstalled[SITE_AUTO_RECRUIT] || installSite(SITE_AUTO_RECRUIT);
#endif
    ok += installShopBuildCall();
    wanted += 2;
    carriedSiteHooks = ok == wanted ? 1 : -1;
    writeInstallLog("carried_site_hooks_installed", ok, wanted);
    return carriedSiteHooks > 0;
}

static const char *transportSiteRefusal(const TransportEntry *f) {
    if (!f->terrain[0]) return NULL;
    int last = G32(RVA_LAST_TERRAIN);
    if (f->terrainIsNumber) {
        long long n = atoll(f->terrain);
        if (n < 0 || n > last || n >= TERRAIN_TABLE_MAX) return "bad_terrain";
    } else if (strlen(f->terrain) >= TERRAIN_NAME_SIZE) {
        return "bad_text";
    } else {
        int found = 0;
        for (int t = 0; t <= last && t < TERRAIN_TABLE_MAX && !found; t++)
            found = recordNameIs(GAME_PTR(RVA_TERRAIN_TABLE) + (size_t)t * TERRAIN_SIZE, TERRAIN_NAME_SIZE, f->terrain);
        if (!found) return "unknown_terrain";
    }
    return installCarriedSiteHooks() ? NULL : "hook_not_installed";
}
