#pragma once

#define DECK_PART             2
#define DECK_PART_LONE        1
#define DECK_SINK_DAMAGE      10
#define DECK_TAG_BASE         10000000
#define DECK_ATTACKER_FLAGS   0x2000000000ULL
#define DECK_DEFENDER_FLAGS   0x4000000000ULL
#define DECK_FIRST_ROW        1
#define DECK_MIDDLE_ROW       10
#define DECK_FRONT_GAP        3
#define DECK_SWIMS            0xc0ULL
#define DECK_UNIT_PLAYER_MAX  31

typedef struct {
    short dx, dy;
    int monster;
} DeckObject;

typedef struct {
    char mod[MOD_NAME_MAX];
    char path[MAX_PATH];
    char monsterName[MONSTER_NAME_SIZE], heartName[MONSTER_NAME_SIZE];
    int monster, heart;
    int fromWords;
    int width, height, count, hearts;
    DeckObject *objects;
} TransportDeck;
static TransportDeck *transportDecks;
static int transportDeckCount, transportDeckCapacity;
static TransportDeck transportDeckStaged;

typedef struct {
    int deck, vehicle, side, tag;
    int left, bottom;
    int parts, hearts, offField;
    int keptOff;
} PlacedDeck;
static struct {
    int active, fighting, x, y, count, capacity, sea;
    PlacedDeck *decks;
} deckBattle;
static int transportDeckHooks;

static void (*deckPlaceForTests)(const char *name, int x, int y, int side, unsigned long long flags, int a, int b);
static void (*deckRemoveForTests)(int unit);
static void (*deckMoveForTests)(int unit, int x, int y);
static int (*deckRankForTests)(int unit);
static unsigned long long (*deckUnitFlagsForTests)(int unit);
static int (*deckTokenForTests)(int unit, int *width, int *height);

// ---- The deck's file -----------------------------------------------------------------------------
static TransportDeck *transportDeckFor(const char *monsterName, int fromWords) {
    for (int i = 0; i < transportDeckCount; i++)
        if (transportDecks[i].fromWords == fromWords && !strcmp(transportDecks[i].monsterName, monsterName)) return &transportDecks[i];
    return NULL;
}

static void dropTransportDeck(const char *monsterName, int fromWords) {
    TransportDeck *d = transportDeckFor(monsterName, fromWords);
    if (!d) return;
    free(d->objects);
    *d = transportDecks[--transportDeckCount];
    deckBattle.active = deckBattle.count = 0;
}

static const char *readTransportDeck(const char *path, TransportDeck *deck) {
    BattleMapFile m;
    const char *names = NULL;
    if (!readBattleMapFile(path, &m)) {
        free(m.data);
        return "not_a_battle_map";
    }
    int hasNames = battleMapNames(&m, &names), tree = battleTreeNow(), minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    deck->objects = calloc((size_t)(m.count ? m.count : 1), sizeof *deck->objects);
    if (!deck->objects) {
        free(m.data);
        return "out_of_memory";
    }
    for (int i = 0; i < m.count; i++) {
        short at[3];
        memcpy(at, m.data + m.objects + (size_t)i * 6, 6);
        int monster = at[2] - m.ctm + tree;
        const char *name = hasNames ? names + (size_t)i * MONSTER_NAME_SIZE : "";
        if (name[0]) {
            char text[MONSTER_NAME_SIZE];
            snprintf(text, sizeof text, "%.*s", MONSTER_NAME_SIZE - 1, name);
            monster = monsterByName(text);
            if (monster < 0) {
                writeLogWarning("battle_map_object_missing", "reason=no_such_object path=%s name=%s", path, text);
                continue;
            }
        }
        if (!monsterRecord(monster) || !monsterRecord(monster)[0]) continue;
        DeckObject *o = &deck->objects[deck->count++];
        o->dx = at[0], o->dy = at[1], o->monster = monster;
        minX = at[0] < minX ? at[0] : minX, maxX = at[0] > maxX ? at[0] : maxX;
        minY = at[1] < minY ? at[1] : minY, maxY = at[1] > maxY ? at[1] : maxY;
    }
    free(m.data);
    if (!deck->count) return "no_deck_objects";
    for (int i = 0; i < deck->count; i++) deck->objects[i].dx = (short)(deck->objects[i].dx - minX), deck->objects[i].dy = (short)(deck->objects[i].dy - minY);
    deck->width = maxX - minX + 1, deck->height = maxY - minY + 1;
    return NULL;
}

static int installTransportDeckHooks(void);

static int transportDeckHeart(const TransportEntry *e) {
    if (!e->heart[0]) return -1;
    if (e->heartIsNumber) return isUsedMonster(atoll(e->heart)) ? (int)atoll(e->heart) : -2;
    int monster = strlen(e->heart) < MONSTER_NAME_SIZE ? monsterByName(e->heart) : -1;
    return monster < 0 ? -2 : monster;
}

static const char *readTransportDeckHearts(TransportDeck *d) {
    const char *reason = readTransportDeck(d->path, d);
    if (!reason && d->height > BATTLE_ROWS - DECK_FIRST_ROW) reason = "deck_too_tall";
    d->hearts = 0;
    for (int i = 0; !reason && i < d->count; i++) d->hearts += d->heart >= 0 && d->objects[i].monster == d->heart;
    return reason;
}

static const char *transportDeckRefusal(const TransportEntry *e, const char *folder) {
    free(transportDeckStaged.objects);
    memset(&transportDeckStaged, 0, sizeof transportDeckStaged);
    int heart = transportDeckHeart(e);
    if (heart == -2) return "unknown_heart";
    if (!e->battlemap[0]) return heart >= 0 ? "heart_without_battlemap" : NULL;
    if (monsterAbility(e->monster, ABILITY_SHIP_SIZE) > 0) return "game_ship";
    TransportDeck *deck = &transportDeckStaged;
    deck->heart = heart;
    const char *reason = findBattleMapFile(folder, e->battlemap, deck->path, sizeof deck->path);
    if (!reason) reason = readTransportDeckHearts(deck);
    if (!reason && heart >= 0 && !deck->hearts) reason = "heart_not_on_deck";
    if (!reason && !installTransportDeckHooks()) reason = "hook_not_installed";
    if (!reason && !transportDeckFor(e->monsterName, e->fromWords) && !growList((void **)&transportDecks, &transportDeckCapacity, transportDeckCount + 1, sizeof *transportDecks))
        reason = "out_of_memory";
    if (reason) {
        free(deck->objects);
        memset(deck, 0, sizeof *deck);
        return reason;
    }
    snprintf(deck->mod, sizeof deck->mod, "%s", e->mod);
    snprintf(deck->monsterName, sizeof deck->monsterName, "%s", e->monsterName);
    if (heart >= 0) snprintf(deck->heartName, sizeof deck->heartName, "%s", (const char *)monsterRecord(heart));
    deck->monster = e->monster, deck->fromWords = e->fromWords;
    return NULL;
}

static void decksOnTransportAdded(const TransportEntry *e) {
    dropTransportDeck(e->monsterName, e->fromWords);
    if (!transportDeckStaged.objects || !e->battlemap[0]) {
        free(transportDeckStaged.objects);
        memset(&transportDeckStaged, 0, sizeof transportDeckStaged);
        return;
    }
    TransportDeck *deck = &transportDecks[transportDeckCount++];
    *deck = transportDeckStaged;
    memset(&transportDeckStaged, 0, sizeof transportDeckStaged);
    writeLog("transport_deck_declared", "mod=%s monster=%d path=%s objects=%d width=%d height=%d heart=%d hearts=%d line=%d", e->mod, e->monster, deck->path,
             deck->count, deck->width, deck->height, deck->heart, deck->hearts, e->line);
}

static void transportDecksAfterPass(void) {
    deckBattle.active = deckBattle.count = 0;
    for (int i = 0; i < transportDeckCount; i++) {
        TransportDeck *d = &transportDecks[i];
        free(d->objects);
        d->objects = NULL;
        d->count = 0;
        d->monster = monsterByName(d->monsterName);
        d->heart = d->heartName[0] ? monsterByName(d->heartName) : -1;
        const char *reason = readTransportDeckHearts(d);
        if (!reason && d->heartName[0] && !d->hearts) reason = "heart_not_on_deck";
        if (!reason) continue;
        writeLogWarning("transport_deck_dropped", "reason=%s mod=%s monster=%s path=%s", reason, d->mod, d->monsterName, d->path);
        free(d->objects);
        *d = transportDecks[--transportDeckCount];
        i--;
    }
}

// ---- The units and objects the decks read --------------------------------------------------------
static int deckUnitHp(int unit) { return (short)G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE); }
static int deckUnitPlayer(int unit) { return (short)G16(RVA_UNIT_PLAYER + (size_t)unit * UNIT_SIZE); }

static int deckTempAbility(int unit, int ability) {
    for (int i = 0; i < UNIT_TEMP_ABILITIES; i++) {
        int id = (short)G16(RVA_UNIT_TEMP_ABILITY_IDS + (size_t)unit * UNIT_SIZE + (size_t)i * 2);
        if (!id) break;
        if (id == ability) return G32(RVA_UNIT_TEMP_ABILITY_VALUES + (size_t)unit * UNIT_SIZE + (size_t)i * 4);
    }
    return 0;
}

static void deckRemoveTempAbility(int unit, int ability) {
    for (int i = 0; i < UNIT_TEMP_ABILITIES; i++) {
        size_t id = RVA_UNIT_TEMP_ABILITY_IDS + (size_t)unit * UNIT_SIZE + (size_t)i * 2;
        if (!(short)G16(id)) break;
        if ((short)G16(id) != ability) continue;
        G16(id) = (short)-1;
        G32(RVA_UNIT_TEMP_ABILITY_VALUES + (size_t)unit * UNIT_SIZE + (size_t)i * 4) = 0;
        return;
    }
}

static int deckObjectAt(int x, int y) {
    if (x < 0 || y < 0 || x >= G32(RVA_BATTLE_WIDTH) || x >= BATTLE_COLUMNS_MAX || y >= BATTLE_ROWS) return -1;
    int object;
    memcpy(&object, battleCell(x, y) + BATTLE_CELL_OBJECT, 4);
    return object;
}

static int deckPartValue(int unit) {
    if (unit < 0 || unit >= UNIT_TABLE_MAX) return 0;
    return monsterAbility(unitMonsterNumber(unit), ABILITY_SHIP_PART) + deckTempAbility(unit, ABILITY_SHIP_PART);
}

static int deckCellStandable(int x, int y, int tag) {
    int object = deckObjectAt(x, y);
    if (object < 0 || object >= UNIT_TABLE_MAX || deckUnitHp(object) <= 0 || deckTempAbility(object, ABILITY_SHIP_TAG) != tag) return 0;
    const unsigned char *monster = monsterRecord(unitMonsterNumber(object));
    unsigned long long flags = 0;
    if (monster) memcpy(&flags, monster + (RVA_MONSTER_FLAGS - RVA_MONSTER_TABLE), sizeof flags);
    return (flags >> MONSTER_FLAG_STANDABLE_OBJECT) & 1;
}

// ---- Placing the decks ---------------------------------------------------------------------------
static int deckRectFree(int left, int bottom, int w, int h, int width) {
    if (left < 0 || left + w > width || bottom < DECK_FIRST_ROW || bottom + h > BATTLE_ROWS) return 0;
    for (int x = left; x < left + w; x++)
        for (int y = bottom; y < bottom + h; y++)
            if (deckPartValue(deckObjectAt(x, y)) > 0) return 0;
    return 1;
}

static int findDeckPlace(const TransportDeck *d, int side, int width, int *left, int *bottom) {
    int front = side ? width / 2 + DECK_FRONT_GAP : width / 2 - DECK_FRONT_GAP;
    int middle = DECK_MIDDLE_ROW - d->height / 2;
    for (;;) {
        int l = side ? front : front - d->width + 1;
        if (l < 0 || l + d->width > width) return 0;
        for (int k = 0; k < 2 * BATTLE_ROWS; k++) {
            int row = middle + (k % 2 ? -(k + 1) / 2 : k / 2);
            if (deckRectFree(l, row, d->width, d->height, width)) {
                *left = l, *bottom = row;
                return 1;
            }
        }
        front += side ? d->width + 1 : -(d->width + 1);
    }
}

static int placeDeckParts(const TransportDeck *d, PlacedDeck *p) {
    int dir = p->side ? -1 : 1;
    for (int i = 0; i < d->count; i++) {
        const DeckObject *o = &d->objects[i];
        int x = p->side ? p->left + d->width - 1 - o->dx : p->left + o->dx, y = p->bottom + o->dy;
        unsigned char *cell = battleCell(x, y);
        int before;
        memcpy(&before, cell + BATTLE_CELL_OBJECT, 4);
        if (before >= 0) {
            int none = -1;
            memcpy(cell + BATTLE_CELL_OBJECT, &none, 4);
            if (deckRemoveForTests) deckRemoveForTests(before);
            else GAME_FN(void (*)(int), RVA_REMOVE_UNIT)(before);
        }
        const char *name = (const char *)monsterRecord(o->monster);
        if (deckPlaceForTests) deckPlaceForTests(name, x, y, -dir, p->side ? DECK_DEFENDER_FLAGS : DECK_ATTACKER_FLAGS, ABILITY_SHIP_TAG, p->tag);
        else GAME_FN(PlaceTerrainUnitFn, RVA_BATTLE_PLACE_TERRAIN_UNIT)(name, x, y, -dir, p->side ? DECK_DEFENDER_FLAGS : DECK_ATTACKER_FLAGS, ABILITY_SHIP_TAG, p->tag);
        int object = deckObjectAt(x, y);
        if (object < 0 || object >= UNIT_TABLE_MAX) continue;
        if (unitMonsterNumber(object) != o->monster)
            writeLogWarning("transport_deck_object_differs", "reason=another_monster_by_name monster=%d placed=%d name=%s", o->monster, unitMonsterNumber(object), name);
        int isHeart = d->heart >= 0 && unitMonsterNumber(object) == d->heart;
        if (!monsterAbility(unitMonsterNumber(object), ABILITY_SHIP_PART)) setTempAbility(object, ABILITY_SHIP_PART, deckBattle.sea || isHeart ? DECK_PART : DECK_PART_LONE);
        p->parts++;
        p->hearts += isHeart;
    }
    return p->parts;
}

static void placeTransportDecks(int sx, int sy) {
    int width = G32(RVA_BATTLE_WIDTH), sea = *(const unsigned char *)GAME_PTR(RVA_BATTLE_AT_SEA) != 0;
    deckBattle.active = 1, deckBattle.count = 0, deckBattle.x = sx, deckBattle.y = sy, deckBattle.sea = sea;
    if (width > BATTLE_COLUMNS_MAX) width = BATTLE_COLUMNS_MAX;
    for (int u = 0; u <= G32(RVA_UNIT_COUNT) && u < UNIT_TABLE_MAX; u++) {
        if (deckUnitHp(u) <= 0 || G32(RVA_UNIT_X + (size_t)u * UNIT_SIZE) != sx || G32(RVA_UNIT_X + 4 + (size_t)u * UNIT_SIZE) != sy) continue;
        int monster = unitMonsterNumber(u);
        const TransportEntry *e = transportEntryForMonster(monster);
        const TransportDeck *d = e ? transportDeckFor(e->monsterName, e->fromWords) : NULL;
        if (!d) continue;
        int side = playerSameTeam(G32(RVA_BATTLE_PLAYER_A), deckUnitPlayer(u)) ? 0 : 1;
        PlacedDeck p = {.deck = (int)(d - transportDecks), .vehicle = u, .side = side, .tag = DECK_TAG_BASE + u};
        if (!findDeckPlace(d, side, width, &p.left, &p.bottom)) {
            writeLogWarning("transport_deck_not_placed", "reason=no_room vehicle=%d monster=%d width=%d height=%d field=%d", u, monster, d->width, d->height, width);
            continue;
        }
        if (!growList((void **)&deckBattle.decks, &deckBattle.capacity, deckBattle.count + 1, sizeof *deckBattle.decks)) {
            writeLogError("transport_deck_not_placed", "reason=out_of_memory vehicle=%d", u);
            continue;
        }
        if (!placeDeckParts(d, &p)) {
            writeLogWarning("transport_deck_not_placed", "reason=no_object_placed vehicle=%d monster=%d", u, monster);
            continue;
        }
        G32(side ? RVA_BATTLE_SHIPS_D : RVA_BATTLE_SHIPS_A) += 1;
        if (p.hearts) {
            p.offField = p.keptOff = setTempAbility(u, ABILITY_SHIP_SIZE, 1);
            if (!p.offField) writeLogWarning("transport_vehicle_on_field", "reason=temp_abilities_full vehicle=%d", u);
        } else if (d->heart >= 0) writeLogWarning("transport_heart_not_placed", "reason=no_cell vehicle=%d heart=%d", u, d->heart);
        deckBattle.decks[deckBattle.count++] = p;
        writeLog("transport_deck_placed", "vehicle=%d monster=%d side=%d x=%d y=%d left=%d bottom=%d parts=%d of %d hearts=%d sea=%d tag=%d", u, monster, side, sx, sy,
                 p.left, p.bottom, p.parts, d->count, p.hearts, sea, p.tag);
    }
}

// ---- Boarding ------------------------------------------------------------------------------------
static void deckTokenSize(int unit, int *w, int *h) {
    if (deckTokenForTests && deckTokenForTests(unit, w, h)) return;
    *w = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(unit), *h = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(unit);
}

static int findDeckSeat(const PlacedDeck *p, int w, int h, int *sx, int *sy) {
    const TransportDeck *d = &transportDecks[p->deck];
    int middle = p->bottom + d->height / 2;
    for (int c = 0; c < d->width; c++) {
        int x = p->side ? p->left + c : p->left + d->width - 1 - c;
        for (int k = 0; k < 2 * d->height; k++) {
            int y = middle + (k % 2 ? -(k + 1) / 2 : k / 2), fits = 1;
            if (y < p->bottom || y >= p->bottom + d->height) continue;
            for (int tx = x; fits && tx < x + w; tx++)
                for (int ty = y; fits && ty < y + h; ty++) {
                    int unit;
                    fits = ty < BATTLE_ROWS && deckCellStandable(tx, ty, p->tag);
                    if (fits) memcpy(&unit, battleCell(tx, ty), 4), fits = unit < 0;
                }
            if (fits) {
                *sx = x, *sy = y;
                return 1;
            }
        }
    }
    return 0;
}

static void boardTransportDecks(int side) {
    int player = G32(side ? RVA_BATTLE_PLAYER_B : RVA_BATTLE_PLAYER_A), sea = *(const unsigned char *)GAME_PTR(RVA_BATTLE_AT_SEA) != 0;
    int width = G32(RVA_BATTLE_WIDTH), any = 0;
    if (width > BATTLE_COLUMNS_MAX) width = BATTLE_COLUMNS_MAX;
    for (int i = 0; i < deckBattle.count; i++) {
        PlacedDeck *p = &deckBattle.decks[i];
        if (p->side != side) continue;
        if (p->offField) deckRemoveTempAbility(p->vehicle, ABILITY_SHIP_SIZE), p->offField = 0;
        any = 1;
    }
    if (!any) return;
    unsigned char *seen = calloc(UNIT_TABLE_MAX, 1);
    int (*riders)[3] = calloc(UNIT_TABLE_MAX < BATTLE_COLUMNS_MAX * BATTLE_ROWS ? UNIT_TABLE_MAX : BATTLE_COLUMNS_MAX * BATTLE_ROWS, sizeof *riders);
    if (!seen || !riders) {
        free(seen), free(riders);
        writeLogError("transport_deck_not_boarded", "reason=out_of_memory side=%d", side);
        return;
    }
    int count = 0;
    for (int y = 0; y < BATTLE_ROWS; y++)
        for (int x = 0; x < width; x++) {
            int unit;
            memcpy(&unit, battleCell(x, y), 4);
            if (unit < 0 || unit >= UNIT_TABLE_MAX || seen[unit]) continue;
            seen[unit] = 1;
            if (deckUnitHp(unit) <= 0 || deckUnitPlayer(unit) == DECK_UNIT_PLAYER_MAX || !playerSameTeam(player, deckUnitPlayer(unit))) continue;
            if (deckPartValue(deckObjectAt(x, y)) > 0) continue;
            unsigned long long flags = deckUnitFlagsForTests ? deckUnitFlagsForTests(unit) : GAME_FN(unsigned long long (*)(int), RVA_UNIT_FLAGS)(unit);
            if (sea && (flags & DECK_SWIMS)) continue;
            riders[count][0] = unit, riders[count][1] = x, riders[count][2] = y;
            count++;
        }
    free(seen);
    int boarded = 0;
    for (int rank = 1; rank >= -1; rank--)
        for (int r = 0; r < count; r++) {
            int unit = riders[r][0];
            int unitRank = deckRankForTests ? deckRankForTests(unit) : GAME_FN(int (*)(int), RVA_UNIT_BATTLE_RANK)(unit);
            if (unitRank != rank && !(rank == -1 && (unitRank < -1 || unitRank > 1))) continue;
            int w, h, x = -1, y = -1;
            deckTokenSize(unit, &w, &h);
            int seated = 0;
            for (int i = 0; i < deckBattle.count && !seated; i++) seated = deckBattle.decks[i].side == side && findDeckSeat(&deckBattle.decks[i], w, h, &x, &y);
            if (!seated) continue;
            if (deckMoveForTests) deckMoveForTests(unit, x, y);
            else {
                GAME_FN(void (*)(int), RVA_BATTLE_REMOVE_FROM_FIELD)(unit);
                GAME_FN(void (*)(int, int, int), RVA_BATTLE_PLACE_UNIT)(unit, x, y);
            }
            boarded++;
        }
    free(riders);
    writeLog("transport_deck_boarded", "side=%d units=%d boarded=%d sea=%d", side, count, boarded, sea);
}

// ---- Losing parts ---------------------------------------------------------------------------------
static PlacedDeck *placedDeckOfTag(int tag) {
    for (int i = 0; i < deckBattle.count; i++)
        if (deckBattle.decks[i].tag == tag) return &deckBattle.decks[i];
    return NULL;
}

static PlacedDeck *deckOfObject(int unit) {
    if (!deckBattle.active || unit < 0 || unit >= UNIT_TABLE_MAX || deckUnitPlayer(unit) != DECK_UNIT_PLAYER_MAX) return NULL;
    return placedDeckOfTag(deckTempAbility(unit, ABILITY_SHIP_TAG));
}

static int deckFieldWidth(void) {
    int width = G32(RVA_BATTLE_WIDTH);
    return width > BATTLE_COLUMNS_MAX ? BATTLE_COLUMNS_MAX : width;
}

static int deckMarkSinking(const PlacedDeck *p, int unit) {
    if (unit < 0 || unit >= UNIT_TABLE_MAX || deckUnitHp(unit) <= 0 || deckTempAbility(unit, ABILITY_SHIP_TAG) != p->tag) return 0;
    unsigned long long *state = (unsigned long long *)GAME_PTR(RVA_UNITS + (size_t)unit * UNIT_SIZE);
    if (*state & UNIT_STATE_SPREAD_COLLAPSE) return 0;
    *state |= UNIT_STATE_SPREAD_COLLAPSE;
    return 1;
}

static void deckObjectLost(const PlacedDeck *p, int unit, int monster, int part, int x, int y) {
    int marked = 0, width = deckFieldWidth();
    if (deckBattle.sea) {
        if (part < DECK_PART) return;
        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++)
                if ((dx || dy) && x + dx >= 0 && x + dx < width && y + dy >= 0 && y + dy < BATTLE_ROWS) marked += deckMarkSinking(p, deckObjectAt(x + dx, y + dy));
        writeLog("transport_deck_collapse", "vehicle=%d x=%d y=%d marked=%d sea=1", p->vehicle, x, y, marked);
        return;
    }
    if (transportDecks[p->deck].heart < 0 || monster != transportDecks[p->deck].heart) {
        writeLog("transport_deck_part_alone", "vehicle=%d x=%d y=%d unit=%d monster=%d sea=0", p->vehicle, x, y, unit, monster);
        return;
    }
    for (int cx = 0; cx < width; cx++)
        for (int cy = 0; cy < BATTLE_ROWS; cy++) marked += deckMarkSinking(p, deckObjectAt(cx, cy));
    writeLog("transport_deck_sinking", "vehicle=%d heart=%d marked=%d sea=0", p->vehicle, unit, marked);
}

static int deckObjectCell(int unit, int *x, int *y) {
    int width = deckFieldWidth();
    for (int cx = 0; cx < width; cx++)
        for (int cy = 0; cy < BATTLE_ROWS; cy++)
            if (deckObjectAt(cx, cy) == unit) return *x = cx, *y = cy, 1;
    return 0;
}

// ---- After the fight -----------------------------------------------------------------------------
static int deckHeartLives(const PlacedDeck *p) {
    int heart = transportDecks[p->deck].heart;
    for (int u = 0; u <= G32(RVA_UNIT_COUNT) && u < UNIT_TABLE_MAX; u++)
        if (deckUnitHp(u) > 0 && deckUnitPlayer(u) == DECK_UNIT_PLAYER_MAX && unitMonsterNumber(u) == heart && deckTempAbility(u, ABILITY_SHIP_TAG) == p->tag) return 1;
    return 0;
}

static void judgeTransportDecks(void) {
    int width = G32(RVA_BATTLE_WIDTH);
    if (width > BATTLE_COLUMNS_MAX) width = BATTLE_COLUMNS_MAX;
    for (int i = 0; i < deckBattle.count; i++) {
        const PlacedDeck *p = &deckBattle.decks[i];
        int front = -1, left = 0;
        for (int x = 0; x < width; x++)
            for (int y = 0; y < BATTLE_ROWS; y++) {
                int object = deckObjectAt(x, y);
                if (object < 0 || object >= UNIT_TABLE_MAX || deckUnitHp(object) <= 0 || deckTempAbility(object, ABILITY_SHIP_TAG) != p->tag) continue;
                left++;
                if (front < 0 || (p->side ? x < front : x > front)) front = x;
            }
        const TransportDeck *d = &transportDecks[p->deck];
        writeLog("transport_deck_fought", "vehicle=%d front_from=%d front_to=%d parts=%d of %d", p->vehicle, p->side ? p->left : p->left + d->width - 1, front, left, p->parts);
        if (!p->hearts || deckUnitHp(p->vehicle) <= 0) continue;
        if (deckHeartLives(p)) {
            writeLog("transport_heart_found", "vehicle=%d tag=%d", p->vehicle, p->tag);
            continue;
        }
        writeLog("transport_vehicle_removed", "vehicle=%d monster=%d tag=%d x=%d y=%d", p->vehicle, unitMonsterNumber(p->vehicle), p->tag, deckBattle.x, deckBattle.y);
        if (deckRemoveForTests) deckRemoveForTests(p->vehicle);
        else GAME_FN(void (*)(int), RVA_REMOVE_UNIT)(p->vehicle);
    }
    deckBattle.active = deckBattle.count = 0;
}

// ---- The hooks -----------------------------------------------------------------------------------
static int deckFieldNow(void) {
    return transportDeckCount && G32(RVA_BATTLE_ARMY_VIEW) == -1 && !*(const unsigned char *)GAME_PTR(RVA_BATTLE_EDITOR_ACTIVE);
}

static u64 (*nextDeckSetupSide)(u64, u64, u64, u64, u64);
static u64 deckSetupSideHook(u64 player, u64 x, u64 y, u64 side, u64 filter) {
    int on = deckFieldNow();
    if (on && ((int)side == 1 || !deckBattle.active)) placeTransportDecks((int)x, (int)y);
    u64 result = nextDeckSetupSide(player, x, y, side, filter);
    if (on && deckBattle.active && ((int)side == 0 || (int)side == 1)) boardTransportDecks((int)side);
    return result;
}

static u64 (*nextDeckDamageUnit)(u64, u64);
static int (*deckInBattleForTests)(void);
static u64 deckDamageUnitHook(u64 unit, u64 damage) {
    int u = (int)unit, x = -1, y = -1, monster = -1, part = 0;
    int inBattle = deckInBattleForTests ? deckInBattleForTests() : *(const unsigned char *)GAME_PTR(RVA_IN_BATTLE) != 0;
    PlacedDeck *p = inBattle && deckUnitHp(u) > 0 ? deckOfObject(u) : NULL;
    if (p && !deckObjectCell(u, &x, &y)) p = NULL;
    if (p) monster = unitMonsterNumber(u), part = deckPartValue(u);
    u64 result = nextDeckDamageUnit(unit, damage);
    if (p && (deckUnitHp(u) <= 0 || unitMonsterNumber(u) != monster)) deckObjectLost(p, u, monster, part, x, y);
    return result;
}

static u64 (*nextDeckTurnEffects)(u64, u64);
static u64 deckTurnEffectsHook(u64 unit, u64 b) {
    u64 result = nextDeckTurnEffects(unit, b);
    int u = (int)unit;
    const PlacedDeck *p = deckOfObject(u);
    if (!p || deckUnitHp(u) <= 0 || !(*(const unsigned long long *)GAME_PTR(RVA_UNITS + (size_t)u * UNIT_SIZE) & UNIT_STATE_SPREAD_COLLAPSE) ||
        monsterAbility(unitMonsterNumber(u), ABILITY_SHIP_PART) > 0)
        return result;
    GAME_FN(void (*)(int, int, int, const char *, ...), RVA_BATTLE_LOG)(-1, u, 1, (const char *)GAME_PTR(RVA_STR_SINKING),
                                                                         GAME_FN(const char *(*)(int), RVA_UNIT_SHOWN_NAME)(u));
    typedef u64 (*DamageFn)(u64, u64);
    DamageFn damage = siteInstalled[SITE_DAMAGE_UNIT] ? (DamageFn)*siteHeadSlot(SITE_DAMAGE_UNIT) : GAME_FN(DamageFn, RVA_DAMAGE_UNIT);
    damage(unit, DECK_SINK_DAMAGE);
    return result;
}

static int deckSpreadRefused(int tag) { return deckBattle.active && !deckBattle.sea && placedDeckOfTag(tag); }
#ifdef __APPLE__
static u64 (*nextDeckSpread)(u64, u64, u64, u64, u64);
static u64 deckSpreadHook(u64 x, u64 y, u64 monster, u64 ability, u64 tag) {
    return deckSpreadRefused((int)tag) ? 0 : nextDeckSpread(x, y, monster, ability, tag);
}
#else
static u64 (*nextDeckSpread)(u64, u64, u64, u64, u64, u64, u64, u64);
static u64 deckSpreadHook(u64 x, u64 y, u64 a3, u64 a4, u64 a5, u64 monster, u64 ability, u64 tag) {
    return deckSpreadRefused((int)tag) ? 0 : nextDeckSpread(x, y, a3, a4, a5, monster, ability, tag);
}
#endif

static u64 (*nextDeckFight)(u64, u64);
static u64 deckFightHook(u64 x, u64 y) {
    int count = 0, *off = deckBattle.active && deckBattle.count ? malloc((size_t)deckBattle.count * sizeof *off) : NULL;
    if (deckBattle.active && deckBattle.count && !off) writeLogError("transport_vehicles_unjudged", "reason=out_of_memory decks=%d", deckBattle.count);
    for (int i = 0; off && i < deckBattle.count; i++)
        if (deckBattle.decks[i].keptOff) off[count++] = deckBattle.decks[i].vehicle;
    deckBattle.fighting = 1;
    u64 winner = nextDeckFight(x, y);
    deckBattle.fighting = 0;
    int w = (int)winner;
    for (int i = 0; i < count && w >= 0 && w < playerSlotCount(); i++) {
        int v = off[i];
        if (deckUnitHp(v) <= 0 || playerSameTeam(w, deckUnitPlayer(v))) continue;
        writeLog("transport_vehicle_removed", "vehicle=%d monster=%d reason=side_lost winner=%d x=%d y=%d", v, unitMonsterNumber(v), w, (int)x, (int)y);
        if (deckRemoveForTests) deckRemoveForTests(v);
        else GAME_FN(void (*)(int), RVA_REMOVE_UNIT)(v);
    }
    free(off);
    return winner;
}

static u64 (*nextDeckRemoveTempMen)(void);
static u64 deckRemoveTempMenHook(void) {
    if (deckBattle.fighting && deckBattle.active && !*(const unsigned char *)GAME_PTR(RVA_IN_BATTLE)) judgeTransportDecks();
    return nextDeckRemoveTempMen();
}

static int installTransportDeckHooks(void) {
    if (transportDeckHooks) return transportDeckHooks > 0;
    char owner[64];
    moduleFileName((void *)deckSetupSideHook, owner, sizeof owner);
    WrapPlace place = {.layer = WRAP_LAYER_OUTERMOST};
    int ok = insertWrap(SITE_BATTLE_SETUP_SIDE, (void *)deckSetupSideHook, (void **)&nextDeckSetupSide, place, owner, NULL) +
             insertWrap(SITE_FIGHT_THE_FIGHT, (void *)deckFightHook, (void **)&nextDeckFight, place, owner, NULL) +
             insertWrap(SITE_REMOVE_TEMP_MEN, (void *)deckRemoveTempMenHook, (void **)&nextDeckRemoveTempMen, place, owner, NULL) +
             insertWrap(SITE_DAMAGE_UNIT, (void *)deckDamageUnitHook, (void **)&nextDeckDamageUnit, place, owner, NULL) +
             insertWrap(SITE_BATTLE_TURN_EFFECTS, (void *)deckTurnEffectsHook, (void **)&nextDeckTurnEffects, place, owner, NULL) +
             insertWrap(SITE_BATTLE_SPREAD_COLLAPSE, (void *)deckSpreadHook, (void **)&nextDeckSpread, place, owner, NULL);
    transportDeckHooks = ok == 6 ? 1 : -1;
    writeInstallLog("transport_deck_hooks_installed", ok, 6);
    return transportDeckHooks > 0;
}
