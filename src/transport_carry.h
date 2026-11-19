#pragma once

#define CARRIER_FLY_OR_FLOAT_WIDEN   0x8000000081ULL
#define CARRIER_WIDEN_TERRAINS       0x1300002000000d22ULL
#define CARRIER_HUGE                 0x2ULL
#define CARRIER_HUGE_TERRAINS        0xc00ULL
#define CARRIER_SHIPMOVE_TERRAINS    0x800ULL
#define CARRIER_AP_MIN               (-90)
#define CARRIER_AP_MAX               90
#define CARRIER_CRASH_CALLS          5
#define SLOT_CONTROLLER_AI_LAST      4

typedef struct {
    int need;
    int carriers, riders, places;
    unsigned long long flags;
    int ap;
    int commanderCarries;
    int allotment, spent;
} CarriedArmy;

typedef struct {
    int unit, turn, monster, extra;
} RiderSpent;
static RiderSpent *riderSpent;
static int riderSpentCount, riderSpentCapacity;

static unsigned long long (*nextCarrierMoveFlags)(int commander);
static int (*gameCarrierArmyApLeft)(int commander);
static int (*gameCarrierCommanderApLeft)(int commander);
static void (*gameCarrierAddApUsed)(int commander, int wholeArmy, int n);
static void (*gameCarrierShipmoveSpendAp)(int commander, int wholeArmy, int n);
static int carriersInstalled;
static int (*carrierSkyForTests)(int x, int y);
static void carriersOnTransportAdded(void);

// ---- The words: transportland and transportair ----------------------------------------------------
static unsigned char *monsterCarryWords;
static int monsterCarryWordCount, carryWordsAny;

static int carrierMediaOf(int monster) {
    int media = monsterCarryWords && monster >= 0 && monster < monsterCarryWordCount ? monsterCarryWords[monster] : 0;
    const TransportEntry *e = transportEntryForMonster(monster);
    return media | (e && transportEntryLive(e) ? e->media : 0);
}

static int carriersDeclared(void) { return transportEntryCount || carryWordsAny; }

static int setMonsterCarryWord(int monster, int bit, int on) {
    if (!monsterCarryWords) {
        if (!on) return 1;
        int records = tableRecords(TABLE_MONSTER);
        if (!(monsterCarryWords = calloc((size_t)records, 1))) return 0;
        monsterCarryWordCount = records;
    }
    if (monster < 0 || monster >= monsterCarryWordCount) return 0;
    monsterCarryWords[monster] = (unsigned char)(on ? monsterCarryWords[monster] | bit : monsterCarryWords[monster] & ~bit);
    if (on) carryWordsAny = 1;
    return 1;
}

static void copyMonsterCarryWords(int source, int target) {
    if (!monsterCarryWords || target < 0 || target >= monsterCarryWordCount) return;
    monsterCarryWords[target] = source >= 0 && source < monsterCarryWordCount ? monsterCarryWords[source] : 0;
}

static void clearMonsterCarryWords(int monster) {
    if (!monsterCarryWords || monster < 0 || monster >= monsterCarryWordCount) return;
    monsterCarryWords[monster] = 0;
}

static void carryWordsBeforePass(int level) {
    if (level != 2) return;
    if (monsterCarryWords) memset(monsterCarryWords, 0, (size_t)monsterCarryWordCount);
    carryWordsAny = 0;
}

static void carryWordLine(const char *word, int bit, const char *args, int mod) {
    const char *p = args;
    long long on = 1;
    skipSpaces(&p);
    if (!isLineEnd(p) && (!readSpellNumber(&p, &on) || (skipSpaces(&p), !isLineEnd(p)) || on < 0 || on > 1)) {
        refuseC5mLine(word, spellModName(mod), "bad_value", "line=%d text=%.40s min=0 max=1", G32(RVA_MOD_LINE), args);
        return;
    }
    int monster = G32(RVA_MONSTER_CURRENT);
    if (!monsterRecord(monster)) {
        refuseC5mLine(word, spellModName(mod), "no_monster", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    if (!setMonsterCarryWord(monster, bit, (int)on)) {
        refuseC5mLine(word, spellModName(mod), "out_of_memory", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    if (on) carriersOnTransportAdded();
}
static void transportlandLine(const char *args, int mod, int level) {
    if (level == 2) carryWordLine("transportland", CARRY_LAND, args, mod);
}
static void transportairLine(const char *args, int mod, int level) {
    if (level == 2) carryWordLine("transportair", CARRY_AIR, args, mod);
}

static void carrierWordsInit(void) {
    int land = apiAddModCommand("transportland", transportlandLine), air = apiAddModCommand("transportair", transportairLine);
    writeLog("carrier_words_ready", "transportland=%d transportair=%d", land, air);
}

static int carrierMediumAt(int x, int y) {
    int sky = carrierSkyForTests ? carrierSkyForTests(x, y) : GAME_FN(int (*)(int, int), RVA_SQUARE_IS_SKY)(x, y);
    return sky > 0 || squareIsWater(x, y) ? CARRY_AIR : CARRY_LAND;
}

static unsigned long long carrierUnitFlags(int unit) { return GAME_FN(unsigned long long (*)(int), RVA_UNIT_FLAGS)(unit); }

static unsigned long long carrierWidenedFlags(unsigned long long f) {
    if (f & CARRIER_FLY_OR_FLOAT_WIDEN) {
        f |= CARRIER_WIDEN_TERRAINS;
        if (f & UNIT_FLAG_FLY_OR_FLOAT) f |= UNIT_FLAG_WATER_MOVE;
    }
    if (f & CARRIER_HUGE) f |= CARRIER_HUGE_TERRAINS;
    if (f & UNIT_FLAG_SHIPMOVE) f |= CARRIER_SHIPMOVE_TERRAINS;
    return f;
}

// ---- A rider's spent points ------------------------------------------------------------------------
static RiderSpent *riderSpentEntry(int unit, int add) {
    int turn = G32(RVA_TURN), monster = (short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE);
    RiderSpent *slot = NULL;
    for (int i = 0; i < riderSpentCount; i++) {
        RiderSpent *r = &riderSpent[i];
        if (r->turn != turn) {
            if (!slot) slot = r;
            continue;
        }
        if (r->unit == unit) {
            if (r->monster == monster) return r;
            if (!add) return NULL;
            r->monster = monster, r->extra = 0;
            return r;
        }
    }
    if (!add) return NULL;
    if (!slot) {
        if (riderSpentCount == riderSpentCapacity) {
            int capacity = riderSpentCapacity ? riderSpentCapacity * 2 : 64;
            RiderSpent *grown = realloc(riderSpent, (size_t)capacity * sizeof *grown);
            if (!grown) {
                writeLogError("rider_spent_refused", "reason=out_of_memory unit=%d", unit);
                return NULL;
            }
            riderSpent = grown, riderSpentCapacity = capacity;
        }
        slot = &riderSpent[riderSpentCount++];
    }
    *slot = (RiderSpent){unit, turn, monster, 0};
    return slot;
}

static int unitSpentThisTurn(int unit) {
    const RiderSpent *r = riderSpentEntry(unit, 0);
    return (short)G16(RVA_UNITS + (size_t)unit * UNIT_SIZE + UNIT_AP_USED) + (r ? r->extra : 0);
}

static int carrierRulesFor(int player) {
    if (player < 0 || player >= playerSlotCount()) return 0;
    short controller = *(const short *)playerSlotAddress(RVA_SLOT_CONTROLLER, player, 2);
    return controller < CONTROLLER_AI || controller > SLOT_CONTROLLER_AI_LAST;
}

static int carrierPlaces(int unit, int need) {
    int monster = (short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE);
    if (!(carrierMediaOf(monster) & need)) return 0;
    int places = unitAbilityTotal(unit, MONSTER_ABILITY_SHIP, 1);
    return places > 0 ? places : 0;
}

static int addCarriedUnit(CarriedArmy *a, int unit, int isCommander, int *anyNoland, int *anySlow, int *anyShip) {
    int places = carrierPlaces(unit, a->need), spent = unitSpentThisTurn(unit);
    unsigned long long own = carrierUnitFlags(unit);
    if (spent > a->spent) a->spent = spent;
    if (own & UNIT_FLAG_MT_NOLAND) *anyNoland = 1;
    if (!places) {
        a->riders++;
        return GAME_FN(int (*)(int), RVA_UNIT_BOAT_SIZE)(unit) == 1;
    }
    if (!(carrierMediaOf((short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE)) & CARRY_AIR)) own &= ~UNIT_FLAG_FLY_OR_FLOAT;
    unsigned long long widened = carrierWidenedFlags(own);
    a->flags = a->carriers ? a->flags & widened : widened;
    if (own & UNIT_FLAG_SLOW) *anySlow = 1;
    if (own & UNIT_FLAG_SHIPMOVE) *anyShip = 1;
    int speed = unitApLeft(unit) + (short)G16(RVA_UNITS + (size_t)unit * UNIT_SIZE + UNIT_AP_USED);
    if (!a->carriers || speed < a->allotment) a->allotment = speed;
    a->carriers++;
    a->places += places;
    if (isCommander) a->commanderCarries = 1;
    return 1;
}

static int carriedArmy(int commander, CarriedArmy *a) {
    memset(a, 0, sizeof *a);
    if (!carriersDeclared() || commander < 0 || commander >= COMMANDER_TABLE_MAX || commander > G32(RVA_LAST_COMMANDER)) return 0;
    int unit = G32(RVA_COMMANDERS + (size_t)commander * COMMANDER_SIZE), last = G32(RVA_UNIT_COUNT);
    if (unit < 0 || unit > last || unit >= UNIT_TABLE_MAX) return 0;
    size_t at = (size_t)unit * UNIT_SIZE;
    if ((short)G16(RVA_UNIT_MONSTER + at) < 0 || (short)G16(RVA_UNIT_HP + at) <= 0) return 0;
    if (!carrierRulesFor((short)G16(RVA_UNIT_PLAYER + at))) return 0;
    a->need = carrierMediumAt((int)G32(RVA_UNIT_X + at), (int)G32(RVA_UNIT_X + 4 + at));
    int anyNoland = 0, anySlow = 0, anyShip = 0, fits = addCarriedUnit(a, unit, 1, &anyNoland, &anySlow, &anyShip);
    for (int u = 0; fits && u <= last && u < UNIT_TABLE_MAX; u++) {
        at = (size_t)u * UNIT_SIZE;
        if (u == unit || (int)G32(RVA_UNIT_LEADER + at) != commander || (short)G16(RVA_UNIT_HP + at) < 1 || (short)G16(RVA_UNIT_MONSTER + at) < 0) continue;
        fits = addCarriedUnit(a, u, 0, &anyNoland, &anySlow, &anyShip);
    }
    if (!fits || !a->carriers || !a->riders || a->riders > a->places) return 0;
    if (anyNoland) a->flags |= UNIT_FLAG_MT_NOLAND;
    if (anySlow) a->flags |= UNIT_FLAG_SLOW;
    if (anyShip) a->flags |= UNIT_FLAG_SHIPMOVE | UNIT_FLAG_WATER_MOVE;
    a->ap = a->allotment - a->spent;
    a->ap = a->ap < CARRIER_AP_MIN ? CARRIER_AP_MIN : a->ap > CARRIER_AP_MAX ? CARRIER_AP_MAX : a->ap;
    return 1;
}

// ---- The hooks ------------------------------------------------------------------------------------
static unsigned long long carrierMoveFlags(int commander) {
    CarriedArmy a;
    return carriedArmy(commander, &a) ? a.flags : nextCarrierMoveFlags(commander);
}

static int carrierArmyApLeft(int commander) {
    int game = gameCarrierArmyApLeft(commander);
    CarriedArmy a;
    return carriedArmy(commander, &a) ? a.ap : game;
}

static int carrierCommanderApLeft(int commander) {
    int game = gameCarrierCommanderApLeft(commander);
    CarriedArmy a;
    return carriedArmy(commander, &a) && !a.commanderCarries ? a.ap : game;
}

static void carriedArmyCharge(void (*game)(int, int, int), int commander, int wholeArmy, int n) {
    CarriedArmy a;
    if (!wholeArmy || !carriedArmy(commander, &a)) {
        game(commander, wholeArmy, n);
        return;
    }
    int last = G32(RVA_UNIT_COUNT), unit = G32(RVA_COMMANDERS + (size_t)commander * COMMANDER_SIZE), count = 0;
    struct { int unit; short before; } *riders = malloc((size_t)a.riders * sizeof *riders);
    if (!riders) {
        writeLogError("rider_spent_refused", "reason=out_of_memory commander=%d", commander);
        game(commander, wholeArmy, n);
        return;
    }
    for (int u = 0; u <= last && u < UNIT_TABLE_MAX && count < a.riders; u++) {
        size_t at = (size_t)u * UNIT_SIZE;
        if (u != unit && ((int)G32(RVA_UNIT_LEADER + at) != commander || (short)G16(RVA_UNIT_HP + at) < 1)) continue;
        if ((short)G16(RVA_UNIT_MONSTER + at) < 0 || carrierPlaces(u, a.need)) continue;
        riders[count].unit = u, riders[count++].before = (short)G16(RVA_UNITS + at + UNIT_AP_USED);
    }
    game(commander, wholeArmy, n);
    for (int i = 0; i < count; i++) {
        int u = riders[i].unit;
        size_t at = (size_t)u * UNIT_SIZE;
        int now = (short)G16(RVA_UNITS + at + UNIT_AP_USED), speed = unitApLeft(u) + now;
        int cap = riders[i].before > speed ? riders[i].before : speed;
        if (now <= cap) continue;
        RiderSpent *r = riderSpentEntry(u, 1);
        if (!r) continue;
        r->extra += now - cap;
        G16(RVA_UNITS + at + UNIT_AP_USED) = (short)cap;
        GAME_FN(void (*)(int), RVA_UNIT_FLUSH_CACHES)(u);
    }
    free(riders);
}
static void carrierAddApUsed(int commander, int wholeArmy, int n) { carriedArmyCharge(gameCarrierAddApUsed, commander, wholeArmy, n); }
static void carrierShipmoveSpendAp(int commander, int wholeArmy, int n) { carriedArmyCharge(gameCarrierShipmoveSpendAp, commander, wholeArmy, n); }

static int carrierCommanderOf(int unit) {
    int last = G32(RVA_LAST_COMMANDER);
    for (int c = 0; c <= last && c < COMMANDER_TABLE_MAX; c++)
        if (G32(RVA_COMMANDERS + (size_t)c * COMMANDER_SIZE) == unit) return c;
    return -1;
}

static unsigned long long carrierCrashUnitFlags(int unit) {
    unsigned long long f = carrierUnitFlags(unit);
    if (!carriersDeclared() || unit < 0 || unit >= UNIT_TABLE_MAX) return f;
    int leader = G32(RVA_UNIT_LEADER + (size_t)unit * UNIT_SIZE), commander = leader >= 0 ? leader : carrierCommanderOf(unit);
    CarriedArmy a;
    if (commander < 0 || !carriedArmy(commander, &a) || carrierPlaces(unit, a.need)) return f;
    return f | (a.flags & UNIT_FLAG_FLY_OR_FLOAT);
}

// ---- Install --------------------------------------------------------------------------------------
static int pointCarrierCrashCalls(void) {
    static const struct {
        unsigned int rva;
        const char *name;
    } CALLS[CARRIER_CRASH_CALLS] = {
        {RVA_CRASH_FLY_TEST_CALL, "crash_fly_test"}, {RVA_CRASH_FLY_RETEST_CALL, "crash_fly_retest"},
        {RVA_CRASH_FLOAT_TEST_CALL, "crash_float_test"}, {RVA_CRASH_COMMANDER_TEST_CALL, "crash_commander_test"},
        {RVA_CRASH_UNIT_TEST_CALL, "crash_unit_test"},
    };
    int ok = 0;
#ifdef __APPLE__
    unsigned char *trampoline = newTrampoline((void *)carrierCrashUnitFlags);
    for (int i = 0; trampoline && i < CARRIER_CRASH_CALLS; i++) {
        unsigned char *at = gameBase + CALLS[i].rva;
        if (blTarget(at) != gameBase + RVA_UNIT_FLAGS) {
            writeLog("hook_skipped", "site=%s reason=not_the_expected_call address=%p", CALLS[i].name, (void *)at);
            continue;
        }
        uint32_t word = blTo(at, trampoline);
        ok += writeGameCode(at, &word, 4);
    }
    if (!trampoline) writeLog("hook_skipped", "site=crash_calls reason=no_padding");
#else
    unsigned char *stubs = allocNear(gameBase + RVA_TEXT_SECTION);
    if (!stubs) {
        writeLog("hook_skipped", "site=crash_calls reason=no_near_page");
        return 0;
    }
    makePagesReadExecute(stubs, 0x1000);
    for (int i = 0; i < CARRIER_CRASH_CALLS; i++)
        ok += redirectCall(CALLS[i].rva, RVA_UNIT_FLAGS, stubs + 0x20 * i, (void *)carrierCrashUnitFlags, CALLS[i].name, NULL);
#endif
    return ok;
}

static void carriersOnTransportAdded(void) {
    if (carriersInstalled) return;
    carriersInstalled = -1;
    if (!checkGameFunction("carriers_check", "")) {
        writeLogError("carriers_installed", "ok=0 of 4 reason=%s", gameCodeReason());
        return;
    }
    unsigned char *armyAp = gameBase + RVA_COMMANDER_ARMY_AP_LEFT, *commanderAp = gameBase + RVA_COMMANDER_AP_LEFT;
    unsigned char *addAp = gameBase + RVA_COMMANDER_ADD_AP_USED, *shipAp = gameBase + RVA_SHIPMOVE_SPEND_AP;
    int army = fanpatchHookCreate(armyAp, (void *)carrierArmyApLeft, (void **)&gameCarrierArmyApLeft) && fanpatchHookEnable(armyAp, 1);
    int own = fanpatchHookCreate(commanderAp, (void *)carrierCommanderApLeft, (void **)&gameCarrierCommanderApLeft) && fanpatchHookEnable(commanderAp, 1);
    int charge = fanpatchHookCreate(addAp, (void *)carrierAddApUsed, (void **)&gameCarrierAddApUsed) && fanpatchHookEnable(addAp, 1) &&
                 fanpatchHookCreate(shipAp, (void *)carrierShipmoveSpendAp, (void **)&gameCarrierShipmoveSpendAp) && fanpatchHookEnable(shipAp, 1);
    int crash = pointCarrierCrashCalls();
    int flags = army && own && charge && crash == CARRIER_CRASH_CALLS &&
                insertWrap(SITE_ARMY_MOVE_FLAGS, (void *)carrierMoveFlags, (void **)&nextCarrierMoveFlags, (WrapPlace){.priority = INT_MIN}, "coe5se", NULL);
    int ok = flags;
    carriersInstalled = ok ? 1 : -1;
    writeResultLog(ok, "carriers_installed", "flags=%d army_ap=%d commander_ap=%d charge=%d crash=%d of %d", flags, army, own, charge, crash,
                   CARRIER_CRASH_CALLS);
}

// ---- In saves: VHCL -------------------------------------------------------------------------------
#define VEHICLES_CHUNK_VERSION 1

static void saveBufferUnitMonster(SaveBuffer *b, int unit) {
    int monster = unit >= 0 && unit < UNIT_TABLE_MAX ? (short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE) : -1;
    char name[MONSTER_NAME_SIZE + 1] = "";
    if (monster >= 0 && monster <= G32(RVA_MONSTER_COUNT)) snprintf(name, sizeof name, "%.*s", MONSTER_NAME_SIZE, (const char *)monsterRecord(monster));
    saveBufferString(b, name);
}

static int loadedUnitMonster(int unit, const char *name) {
    if (unit < 0 || unit >= UNIT_TABLE_MAX || unit > G32(RVA_UNIT_COUNT)) return -1;
    int monster = (short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE);
    return name[0] && transportMonsterNamed(monster, name) ? monster : -1;
}

static void addVehiclesChunk(SaveList *list) {
    int turn = saveTailPlayer >= 0, orders = 0;
    if (turn && netRoleNow() != NET_ROLE_CLIENT) return;
    for (int i = 0; i < carriedOrderCount; i++) orders += !turn || carriedOrders[i].player == saveTailPlayer;
    if (turn ? !orders : !orders && !carriedMarkCount && !riderSpentCount) return;
    SaveBuffer chunk = {0};
    saveBufferInt(&chunk, VEHICLES_CHUNK_VERSION);
    int now = G32(RVA_TURN), riders = 0, marks = 0;
    for (int i = 0; !turn && i < riderSpentCount; i++) riders += riderSpent[i].turn == now;
    for (int i = 0; !turn && i < carriedMarkCount; i++) marks += carriedMarks[i].turn == now;
    saveBufferInt(&chunk, riders);
    for (int i = 0; !turn && i < riderSpentCount; i++) {
        if (riderSpent[i].turn != now) continue;
        saveBufferInt(&chunk, riderSpent[i].unit), saveBufferInt(&chunk, riderSpent[i].turn), saveBufferInt(&chunk, riderSpent[i].extra);
        saveBufferUnitMonster(&chunk, riderSpent[i].unit);
    }
    saveBufferInt(&chunk, marks);
    for (int i = 0; !turn && i < carriedMarkCount; i++) {
        if (carriedMarks[i].turn != now) continue;
        saveBufferInt(&chunk, carriedMarks[i].unit), saveBufferInt(&chunk, carriedMarks[i].turn);
        saveBufferUnitMonster(&chunk, carriedMarks[i].unit);
    }
    saveBufferInt(&chunk, orders);
    for (int i = 0; i < carriedOrderCount; i++) {
        if (turn && carriedOrders[i].player != saveTailPlayer) continue;
        saveBufferInt(&chunk, carriedOrders[i].player), saveBufferInt(&chunk, carriedOrders[i].order), saveBufferInt(&chunk, carriedOrders[i].unit);
        saveBufferUnitMonster(&chunk, carriedOrders[i].unit);
    }
    writeLog("vehicles_written", "file=%s riders=%d marks=%d orders=%d bytes=%zu", turn ? "turn" : "save", riders, marks, orders, chunk.size);
    saveListAdd(list, SAVE_TAG_VHCL, &chunk);
}

typedef struct {
    int kind, a, b, c;
    char name[MONSTER_NAME_SIZE + 1];
} VehicleChunkEntry;
#define VEHICLE_ENTRY_RIDER 0
#define VEHICLE_ENTRY_MARK  1
#define VEHICLE_ENTRY_LINK  2

static const char *readVehiclesChunk(const SaveChunk *chunk, VehicleChunkEntry **out, int *count, int *version) {
    SaveCursor c = {chunk->payload, chunk->payload + chunk->size, 0};
    *out = NULL, *count = 0;
    *version = saveCursorInt(&c);
    if (c.failed) return "bad_chunk";
    if (*version != VEHICLES_CHUNK_VERSION) return "other_version";
    int capacity = chunk->size / 8 + 1;
    VehicleChunkEntry *e = calloc((size_t)capacity, sizeof *e);
    if (!e) return "out_of_memory";
    for (int kind = VEHICLE_ENTRY_RIDER; kind <= VEHICLE_ENTRY_LINK && !c.failed; kind++) {
        int n = saveCursorInt(&c);
        if (c.failed || n < 0 || *count + n > capacity) {
            c.failed = 1;
            break;
        }
        for (int i = 0; i < n && !c.failed; i++) {
            VehicleChunkEntry *v = &e[(*count)++];
            v->kind = kind;
            v->a = saveCursorInt(&c), v->b = saveCursorInt(&c);
            if (kind != VEHICLE_ENTRY_MARK) v->c = saveCursorInt(&c);
            saveCursorString(&c, v->name, sizeof v->name);
        }
    }
    if (c.failed) {
        free(e);
        *count = 0;
        return "bad_chunk";
    }
    *out = e;
    return NULL;
}

static void useVehiclesChunk(const SaveChunk *chunk, SaveFile kind) {
    int turn = kind == SAVE_FILE_TURN, player = turn ? turnFileReadPlayer() : -1;
    if (turn && !chunk) return;
    if (turn && (netRoleNow() != NET_ROLE_HOST || player < 0)) {
        writeLogWarning("vehicles_refused", "reason=not_the_host_read");
        return;
    }
    if (!turn) riderSpentCount = carriedMarkCount = carriedOrderCount = 0;
    if (!chunk) return;
    VehicleChunkEntry *entries;
    int count, version;
    const char *bad = readVehiclesChunk(chunk, &entries, &count, &version);
    if (bad) {
        writeLogWarning("vehicles_read", "file=%s reason=%s version=%d", SAVE_FILE_NAMES[kind], bad, version);
        return;
    }
    if (turn)
        for (int i = carriedOrderCount - 1; i >= 0; i--)
            if (carriedOrders[i].player == player) carriedOrders[i] = carriedOrders[--carriedOrderCount];
    int read = 0, skipped = 0, now = G32(RVA_TURN);
    for (int i = 0; i < count; i++) {
        const VehicleChunkEntry *v = &entries[i];
        int ok = 0;
        if (v->kind == VEHICLE_ENTRY_RIDER) {
            RiderSpent *r = !turn && loadedUnitMonster(v->a, v->name) >= 0 && v->b == now && v->c > 0 ? riderSpentEntry(v->a, 1) : NULL;
            if (r) r->extra = v->c, ok = 1;
        } else if (v->kind == VEHICLE_ENTRY_MARK) {
            ok = !turn && loadedUnitMonster(v->a, v->name) >= 0 && v->b == now && carriedSetMark(v->a);
        } else {
            int owner = v->a, order = v->b, unit = v->c;
            ok = owner >= 0 && owner < PLAYER_SLOTS && order >= 0 && order < AUTO_RECRUIT_ORDERS_MAX && (!turn || owner == player) &&
                 loadedUnitMonster(unit, v->name) >= 0 && *carriedOrderField(owner, order, 2) > 0 &&
                 carriedGrow((void **)&carriedOrders, carriedOrderCount, &carriedOrderCapacity, sizeof *carriedOrders, "orders");
            if (ok) carriedOrders[carriedOrderCount++] = (CarriedOrder){owner, order, unit, *carriedOrderField(owner, order, 2)};
        }
        if (ok) read++;
        else skipped++;
    }
    free(entries);
    if (skipped) writeLogWarning("vehicles_read", "file=%s reason=bad_entries read=%d skipped=%d player=%d", SAVE_FILE_NAMES[kind], read, skipped, player);
    else writeLog("vehicles_read", "file=%s read=%d player=%d", SAVE_FILE_NAMES[kind], read, player);
}
