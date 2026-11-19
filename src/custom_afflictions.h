#pragma once

#define CUSTOM_AFFLICTIONS_MAX       200
#define CUSTOM_AFFLICTION_NAME_SIZE  64
#define CUSTOM_AFFLICTION_DESCR_SIZE (TRAIT_LINE_TEXT_MAX + 1)
#define CUSTOM_AFFLICTIONS_VERSION   1
#define REPLAY_AFFLICTIONS_VERSION   1
#define AFFRES_IMMUNE                100
#define AFFLICTION_CHUNKS_KEPT_AFTER 8
#define AFFLICTION_UNIT_BYTES_MAX    (8 + CUSTOM_AFFLICTIONS_MAX * 9)
#define AFFLICTION_STAT_COUNT        16
_Static_assert(CUSTOM_AFFLICTIONS_MAX == AFFLICTION_DESCRIPTION_SLOTS, "one card description slot per kind");
_Static_assert(CUSTOM_AFFLICTIONS_MAX <= 255, "a kind is one byte in AFFS and C5RA");

typedef struct {
    char name[CUSTOM_AFFLICTION_NAME_SIZE];
    char descr[CUSTOM_AFFLICTION_DESCR_SIZE];
    int declared;
    int battleaff;
    char mod[64];
    unsigned hash;
    int holders;
    unsigned effectStats;
} CustomAffliction;

static CustomAffliction customAfflictions[CUSTOM_AFFLICTIONS_MAX];
static int customAfflictionCount;

typedef struct {
    unsigned char kind;
    int turn;
    int value;
    int holderAt;
} AfflictionEntry;

typedef struct {
    AfflictionEntry *entries;
    int count, capacity;
} AfflictionList;

typedef struct {
    AfflictionEntry *entries;
    int count, capacity;
    unsigned round;
} ParkedAfflictions;

static AfflictionList *unitAfflictionLists;
static ParkedAfflictions *parkedAfflictionLists;
static int afflictedUnitCount;
static int *afflictionHolders[CUSTOM_AFFLICTIONS_MAX];
static int afflictionHolderCapacity[CUSTOM_AFFLICTIONS_MAX];
static unsigned afflictionRound = 1;
static int replayChangedCount, replayBeforeCount;
static int afflictionStatHolders[AFFLICTION_STAT_COUNT];

static void countAfflictionStatHolders(int k, int delta) {
    for (unsigned bits = customAfflictions[k].effectStats; bits; bits &= bits - 1) afflictionStatHolders[__builtin_ctz(bits)] += delta;
}

static unsigned char *changedAfflictionUnitBits;
static int *changedAfflictionUnits, changedAfflictionUnitCount, changedAfflictionUnitCapacity;

static int gameAfflictionRandom(int n) { return GAME_FN(int (*)(int), RVA_RANDOM)(n); }
static int (*afflictionRandom)(int n) = gameAfflictionRandom;

static int anyCustomAfflictions(void) { return afflictedUnitCount > 0; }

// ---- Kinds --------------------------------------------------------------------------------------
static unsigned afflictionNameHash(const char *name) {
    unsigned h = 2166136261u;
    for (; *name; name++) h = (h ^ (unsigned char)(*name >= 'A' && *name <= 'Z' ? *name - 'A' + 'a' : *name)) * 16777619u;
    return h;
}

static int findCustomAffliction(const char *name) {
    for (int k = 0; k < customAfflictionCount; k++)
        if (!strcasecmp(customAfflictions[k].name, name)) return k;
    return -1;
}

static int findCustomAfflictionByHash(unsigned hash) {
    for (int k = 0; k < customAfflictionCount; k++)
        if (customAfflictions[k].hash == hash) return k;
    return -1;
}

static int addCustomAffliction(const char *name) {
    int k = findCustomAffliction(name);
    if (k >= 0 || customAfflictionCount >= CUSTOM_AFFLICTIONS_MAX) return k;
    CustomAffliction *a = &customAfflictions[customAfflictionCount];
    memset(a, 0, sizeof *a);
    snprintf(a->name, sizeof a->name, "%s", name);
    a->hash = afflictionNameHash(a->name);
    return customAfflictionCount++;
}

static int customAfflictionDeclared(int k) {
    return k >= 0 && k < customAfflictionCount && customAfflictions[k].declared;
}

// ---- The store ----------------------------------------------------------------------------------
static int ensureAfflictionStore(void) {
    if (unitAfflictionLists) return 1;
    AfflictionList *lists = calloc(UNIT_TABLE_MAX, sizeof *lists);
    ParkedAfflictions *parked = calloc(UNIT_TABLE_MAX, sizeof *parked);
    unsigned char *bits = calloc(UNIT_TABLE_MAX / 8 + 1, 1);
    if (!lists || !parked || !bits) {
        free(lists), free(parked), free(bits);
        writeLogError("custom_afflictions_store_failed", "reason=out_of_memory units=%d", UNIT_TABLE_MAX);
        return 0;
    }
    unitAfflictionLists = lists, parkedAfflictionLists = parked, changedAfflictionUnitBits = bits;
    return 1;
}

static int afflictionUnitInRange(int unit) { return unitAfflictionLists && unit >= 0 && unit < UNIT_TABLE_MAX; }

static int unitAfflictionCount(int unit) { return afflictionUnitInRange(unit) ? unitAfflictionLists[unit].count : 0; }

static AfflictionEntry *unitAfflictionEntry(int unit, int k) {
    if (!unitAfflictionCount(unit)) return NULL;
    AfflictionList *l = &unitAfflictionLists[unit];
    for (int i = 0; i < l->count; i++)
        if (l->entries[i].kind == k) return &l->entries[i];
    return NULL;
}

static int afflictionUnitInUse(int unit) {
    if (unit < 0 || unit >= UNIT_TABLE_MAX || unit > G32(RVA_UNIT_COUNT)) return 0;
    size_t at = (size_t)unit * UNIT_SIZE;
    if ((short)G16(RVA_UNIT_MONSTER + at) < 0) return 0;
    return !((short)G16(RVA_UNIT_XP + at) == -1 && (short)G16(RVA_UNIT_PLAYER + at) < 0 && (short)G16(RVA_UNIT_HP + at) < 1);
}

static void noteAfflictionUnit(int unit) {
    if (!afflictionUnitInRange(unit) || changedAfflictionUnitBits[unit / 8] & 1 << unit % 8) return;
    if (!growList((void **)&changedAfflictionUnits, &changedAfflictionUnitCapacity, changedAfflictionUnitCount + 1, sizeof *changedAfflictionUnits))
        return;
    changedAfflictionUnitBits[unit / 8] |= (unsigned char)(1 << unit % 8);
    changedAfflictionUnits[changedAfflictionUnitCount++] = unit;
}

static void forgetChangedAfflictionUnits(void) {
    for (int i = 0; i < changedAfflictionUnitCount; i++) changedAfflictionUnitBits[changedAfflictionUnits[i] / 8] = 0;
    changedAfflictionUnitCount = 0;
}

static int addAfflictionEntry(int unit, int k, int turn, int value) {
    if (!afflictionUnitInRange(unit) || k < 0 || k >= customAfflictionCount) return 0;
    AfflictionEntry *have = unitAfflictionEntry(unit, k);
    if (have) {
        have->turn = turn, have->value = value;
        return 1;
    }
    AfflictionList *l = &unitAfflictionLists[unit];
    CustomAffliction *a = &customAfflictions[k];
    if (!growList((void **)&l->entries, &l->capacity, l->count + 1, sizeof l->entries[0]) ||
        !growList((void **)&afflictionHolders[k], &afflictionHolderCapacity[k], a->holders + 1, sizeof(int))) {
        writeLogError("custom_affliction_dropped", "reason=out_of_memory unit=%d name=%.63s", unit, a->name);
        return 0;
    }
    l->entries[l->count++] = (AfflictionEntry){(unsigned char)k, turn, value, a->holders};
    afflictionHolders[k][a->holders++] = unit;
    countAfflictionStatHolders(k, 1);
    if (l->count == 1) afflictedUnitCount++;
    return 1;
}

static void removeAfflictionEntryAt(int unit, int i) {
    AfflictionList *l = &unitAfflictionLists[unit];
    AfflictionEntry gone = l->entries[i];
    CustomAffliction *a = &customAfflictions[gone.kind];
    int last = afflictionHolders[gone.kind][--a->holders];
    countAfflictionStatHolders(gone.kind, -1);
    afflictionHolders[gone.kind][gone.holderAt] = last;
    if (last != unit) {
        AfflictionEntry *moved = unitAfflictionEntry(last, gone.kind);
        if (moved) moved->holderAt = gone.holderAt;
    }
    l->entries[i] = l->entries[--l->count];
    if (!l->count) afflictedUnitCount--;
}

static int removeAfflictionEntry(int unit, int k) {
    if (!unitAfflictionCount(unit)) return 0;
    AfflictionList *l = &unitAfflictionLists[unit];
    for (int i = 0; i < l->count; i++)
        if (l->entries[i].kind == k) {
            removeAfflictionEntryAt(unit, i);
            return 1;
        }
    return 0;
}

static void clearUnitAfflictions(int unit) {
    while (unitAfflictionCount(unit)) removeAfflictionEntryAt(unit, unitAfflictionLists[unit].count - 1);
}

static void moveUnitAfflictions(int from, int to) {
    if (!afflictionUnitInRange(from) || !afflictionUnitInRange(to) || from == to) return;
    clearUnitAfflictions(to);
    AfflictionList *l = &unitAfflictionLists[from];
    for (int i = 0; i < l->count; i++) addAfflictionEntry(to, l->entries[i].kind, l->entries[i].turn, l->entries[i].value);
    clearUnitAfflictions(from);
    noteAfflictionUnit(from), noteAfflictionUnit(to);
}

static void clearAllCustomAfflictions(void) {
    if (!unitAfflictionLists) return;
    for (int k = 0; k < customAfflictionCount; k++)
        while (customAfflictions[k].holders) clearUnitAfflictions(afflictionHolders[k][0]);
    forgetChangedAfflictionUnits();
    replayChangedCount = replayBeforeCount = 0;
}

// ---- affres: a monster's or item's chance of not receiving a kind -------------------------------
typedef struct {
    int item;
    int record;
    int kind, chance;
} AffresRule;

static AffresRule *affresRules;
static int affresRuleCount, affresRuleCapacity;

static int recordAffres(int item, int record, int k) {
    int chance = 0;
    for (int i = 0; i < affresRuleCount; i++)
        if (affresRules[i].item == item && affresRules[i].record == record && affresRules[i].kind == k) chance += affresRules[i].chance;
    return chance;
}

static int (*afflictionAbilityTotal)(int, int, int, char *, int, int *);

static int afflictionResistance(int unit, int k) {
    int chance = customAfflictions[k].battleaff && afflictionAbilityTotal ? afflictionAbilityTotal(unit, ABILITY_AFFRES, 1, NULL, 0, NULL) : 0;
    if (!affresRuleCount) return chance;
    chance += recordAffres(0, (short)G16(RVA_UNIT_MONSTER + (size_t)unit * UNIT_SIZE), k);
    for (int p = 0; p < ITEM_POSITION_MAX; p++) {
        int item = unitItemAt(unit, p);
        if (item >= 0) chance += recordAffres(1, item, k);
    }
    return chance;
}

// ---- A battle replay's changes, undone at its end ------------------------------------------------
typedef struct { int unit, kind, turn, value; } ReplayAffliction;
static int *replayChangedUnits, replayChangedCapacity;
static ReplayAffliction *replayBeforeEntries;
static int replayBeforeCapacity;

static int keepReplayUnitAfflictions(int unit) {
    for (int i = 0; i < replayChangedCount; i++)
        if (replayChangedUnits[i] == unit) return 1;
    int n = unitAfflictionCount(unit);
    if (!growList((void **)&replayChangedUnits, &replayChangedCapacity, replayChangedCount + 1, sizeof *replayChangedUnits) ||
        !growList((void **)&replayBeforeEntries, &replayBeforeCapacity, replayBeforeCount + n + 1, sizeof *replayBeforeEntries)) {
        writeLogError("custom_afflictions_replay_kept", "reason=out_of_memory unit=%d", unit);
        return 0;
    }
    replayChangedUnits[replayChangedCount++] = unit;
    for (int i = 0; i < n; i++) {
        const AfflictionEntry *e = &unitAfflictionLists[unit].entries[i];
        replayBeforeEntries[replayBeforeCount++] = (ReplayAffliction){unit, e->kind, e->turn, e->value};
    }
    return 1;
}

static void undoReplayAfflictionChanges(void) {
    for (int i = 0; i < replayChangedCount; i++) {
        int unit = replayChangedUnits[i];
        clearUnitAfflictions(unit);
        for (int j = 0; j < replayBeforeCount; j++)
            if (replayBeforeEntries[j].unit == unit)
                addAfflictionEntry(unit, replayBeforeEntries[j].kind, replayBeforeEntries[j].turn, replayBeforeEntries[j].value);
    }
    if (replayChangedCount) writeLog("custom_afflictions_replay_undone", "units=%d", replayChangedCount);
    replayChangedCount = replayBeforeCount = 0;
}

// ---- Giving and curing ----------------------------------------------------------------------------
static int giveCustomAffliction(int unit, int k) {
    if (unitAfflictionEntry(unit, k)) return 1;
    int chance = afflictionResistance(unit, k);
    if (chance >= AFFRES_IMMUNE) return 0;
    if (chance > 0 && afflictionRandom(100) < chance) return 0;
    int replay = replayLoadingUnits();
    if (replay && !keepReplayUnitAfflictions(unit)) return 1;
    if (!addAfflictionEntry(unit, k, G32(RVA_TURN), 0)) return 0;
    if (!replay) noteAfflictionUnit(unit);
    writeLog("custom_affliction_given", "unit=%d name=%.63s turn=%d replay=%d", unit, customAfflictions[k].name, G32(RVA_TURN), replay);
    return 1;
}

static int cureCustomAffliction(int unit, int k) {
    if (!unitAfflictionEntry(unit, k)) return 0;
    int replay = replayLoadingUnits();
    if (replay && !keepReplayUnitAfflictions(unit)) return 1;
    removeAfflictionEntry(unit, k);
    if (!replay) noteAfflictionUnit(unit);
    writeLog("custom_affliction_cured", "unit=%d name=%.63s replay=%d", unit, customAfflictions[k].name, replay);
    return 1;
}

static int firstBattleAffliction(int unit) {
    if (!unitAfflictionCount(unit)) return -1;
    for (int k = 0; k < customAfflictionCount; k++)
        if (customAfflictions[k].battleaff && customAfflictionDeclared(k) && unitAfflictionEntry(unit, k)) return k;
    return -1;
}

// ---- A unit's slot: new units, a fight's parked units ---------------------------------------------
static void lendAfflictionSlot(int unit) {
    if (!unitAfflictionCount(unit)) return;
    clearUnitAfflictions(unit);
}

static void parkUnitAfflictions(int unit) {
    if (!afflictionUnitInRange(unit)) return;
    ParkedAfflictions *p = &parkedAfflictionLists[unit];
    const AfflictionList *l = &unitAfflictionLists[unit];
    if (l->count && !growList((void **)&p->entries, &p->capacity, l->count, sizeof p->entries[0])) {
        writeLogError("custom_afflictions_park_failed", "reason=out_of_memory unit=%d", unit);
        p->round = 0;
        return;
    }
    if (l->count) memcpy(p->entries, l->entries, (size_t)l->count * sizeof l->entries[0]);
    p->count = l->count;
    p->round = afflictionRound;
}

static void returnAfflictionSlot(int unit) {
    if (!afflictionUnitInRange(unit) || parkedAfflictionLists[unit].round != afflictionRound) return;
    ParkedAfflictions *p = &parkedAfflictionLists[unit];
    clearUnitAfflictions(unit);
    for (int i = 0; i < p->count; i++) addAfflictionEntry(unit, p->entries[i].kind, p->entries[i].turn, p->entries[i].value);
    p->round = 0;
}

static void endAfflictionRound(void) {
    afflictionRound++;
    if (!afflictionRound) afflictionRound = 1;
}

// ---- LAN unit records: entries 20..25 (item_slot_net.h) -------------------------------------------
static struct { int unit; unsigned hash, turn, value; } afflictionNetEntry = {-1, 0, 0, 0};

static void clearUnitAfflictionsForRecord(int unit) {
    afflictionNetEntry.unit = -1;
    clearUnitAfflictions(unit);
}

static int putUnitAfflictionEntries(int unit) {
    int n = unitAfflictionCount(unit);
    if (!n) return 0;
    const AfflictionList *l = &unitAfflictionLists[unit];
    for (int i = 0; i < n; i++) {
        unsigned fields[3] = {customAfflictions[l->entries[i].kind].hash, (unsigned)l->entries[i].turn, (unsigned)l->entries[i].value};
        for (int f = 0; f < 6; f++) {
            unsigned half = fields[f / 2] >> (f % 2 ? 16 : 0) & 0xffff;
            itemNetPutByte(AFFLICTION_NET_FIRST_INDEX + f);
            itemNetPutByte((int)(half & 0xff));
            itemNetPutByte((int)(half >> 8));
        }
    }
    writeLog("custom_afflictions_net_sent", "unit=%d afflictions=%d", unit, n);
    return n * 6;
}

static void getUnitAfflictionEntry(int unit, int index, unsigned half) {
    int field = index - AFFLICTION_NET_FIRST_INDEX;
    if (field == 0) afflictionNetEntry.unit = unit, afflictionNetEntry.hash = afflictionNetEntry.turn = afflictionNetEntry.value = 0;
    if (afflictionNetEntry.unit != unit || field < 0 || field > 5) return;
    unsigned *to = field < 2 ? &afflictionNetEntry.hash : field < 4 ? &afflictionNetEntry.turn : &afflictionNetEntry.value;
    *to |= half << (field % 2 ? 16 : 0);
    if (field < 5 || unit < 0) return;
    afflictionNetEntry.unit = -1;
    int k = findCustomAfflictionByHash(afflictionNetEntry.hash);
    if (k < 0) {
        writeLogWarning("custom_afflictions_net_read", "reason=unknown_affliction unit=%d hash=%08x", unit, afflictionNetEntry.hash);
        return;
    }
    if (!ensureAfflictionStore()) return;
    if (addAfflictionEntry(unit, k, (int)afflictionNetEntry.turn, (int)afflictionNetEntry.value))
        writeLog("custom_afflictions_net_read", "unit=%d name=%.63s", unit, customAfflictions[k].name);
}

// ---- Saves and turn files: AFFS -------------------------------------------------------------------
static int turnFileReadPlayer(void);

static int afflictionUnitWritten(int unit, int turnPlayer) {
    if (turnPlayer < 0) return unitAfflictionCount(unit) && afflictionUnitInUse(unit);
    if (changedAfflictionUnitBits[unit / 8] & 1 << unit % 8) return 1;
    return unitAfflictionCount(unit) && (short)G16(RVA_UNIT_PLAYER + (size_t)unit * UNIT_SIZE) == turnPlayer;
}

static void startAfflictionsChunk(SaveBuffer *chunk) {
    saveBufferInt(chunk, CUSTOM_AFFLICTIONS_VERSION);
    saveBufferInt(chunk, customAfflictionCount);
    for (int k = 0; k < customAfflictionCount; k++) saveBufferString(chunk, customAfflictions[k].name);
}

static void addCustomAfflictionsChunks(SaveList *list) {
    int turn = saveTailPlayer >= 0;
    if (!unitAfflictionLists || (turn && netRoleNow() != NET_ROLE_CLIENT)) return;
    if (!afflictedUnitCount && (!turn || !changedAfflictionUnitCount)) return;
    int room = SAVE_CHUNKS_MAX - list->count - AFFLICTION_CHUNKS_KEPT_AFTER, chunks = 0, units = 0, entries = 0, dropped = 0;
    if (room > SAVE_AFFS_CHUNKS_MAX) room = SAVE_AFFS_CHUNKS_MAX;
    SaveBuffer chunk = {0};
    int last = G32(RVA_UNIT_COUNT);
    for (int u = 0; u <= last && u < UNIT_TABLE_MAX; u++) {
        if (!afflictionUnitWritten(u, saveTailPlayer)) continue;
        if (chunk.size && chunk.size + AFFLICTION_UNIT_BYTES_MAX + 4 > SAVE_CHUNK_MAX) {
            saveBufferInt(&chunk, -1);
            saveListAdd(list, SAVE_TAG_AFFS, &chunk);
            chunks++;
        }
        if (!chunk.size && chunks >= room) {
            dropped++;
            continue;
        }
        if (!chunk.size) startAfflictionsChunk(&chunk);
        const AfflictionList *l = &unitAfflictionLists[u];
        saveBufferInt(&chunk, u);
        saveBufferInt(&chunk, l->count);
        for (int i = 0; i < l->count; i++) {
            unsigned char kind = l->entries[i].kind;
            saveBufferAdd(&chunk, &kind, 1);
            saveBufferInt(&chunk, l->entries[i].turn);
            saveBufferInt(&chunk, l->entries[i].value);
        }
        units++, entries += l->count;
    }
    if (chunk.size) {
        saveBufferInt(&chunk, -1);
        saveListAdd(list, SAVE_TAG_AFFS, &chunk);
        chunks++;
    }
    if (dropped) writeLogError("custom_afflictions_dropped", "reason=too_many_chunks file=%s units=%d max=%d", turn ? "turn" : "save", dropped, room);
    writeLog("custom_afflictions_written", "file=%s chunks=%d units=%d entries=%d changed=%d", turn ? "turn" : "save", chunks, units, entries,
             turn ? changedAfflictionUnitCount : 0);
    if (turn) forgetChangedAfflictionUnits();
}

static int readAfflictionsChunk(const SaveChunk *chunk, int apply, int player, int *units, int *entries, int *dropped) {
    SaveCursor c = {chunk->payload, chunk->payload + chunk->size, 0};
    int version = saveCursorInt(&c), kinds = saveCursorInt(&c);
    if (c.failed || version != CUSTOM_AFFLICTIONS_VERSION || kinds < 0 || kinds > CUSTOM_AFFLICTIONS_MAX) return 0;
    int index[CUSTOM_AFFLICTIONS_MAX];
    for (int i = 0; i < kinds; i++) {
        char name[SAVE_STRING_MAX + 1];
        saveCursorString(&c, name, sizeof name);
        if (c.failed || !name[0] || strlen(name) >= CUSTOM_AFFLICTION_NAME_SIZE) return 0;
        index[i] = -1;
        if (!apply) continue;
        if ((index[i] = addCustomAffliction(name)) < 0) {
            (*dropped)++;
            writeLogWarning("custom_afflictions_dropped", "reason=too_many_afflictions name=%.63s max=%d", name, CUSTOM_AFFLICTIONS_MAX);
        }
    }
    for (;;) {
        int unit = saveCursorInt(&c);
        if (c.failed) return 0;
        if (unit == -1) break;
        int count = saveCursorInt(&c);
        if (c.failed || unit < 0 || unit >= UNIT_TABLE_MAX || count < 0 || count > kinds) return 0;
        if (apply && player >= 0) clearUnitAfflictions(unit);
        for (int i = 0; i < count; i++) {
            unsigned char kind = 0;
            if (c.end - c.at < 1) return 0;
            kind = *c.at++;
            int turn = saveCursorInt(&c), value = saveCursorInt(&c);
            if (c.failed || kind >= kinds) return 0;
            if (apply && index[kind] >= 0 && addAfflictionEntry(unit, index[kind], turn, value)) (*entries)++;
        }
        if (apply) (*units)++;
    }
    return c.at == c.end;
}

static void useCustomAfflictionsChunks(const SaveChunk *const *chunks, int count, SaveFile kind) {
    int turn = kind == SAVE_FILE_TURN, player = turn ? turnFileReadPlayer() : -1;
    endAfflictionRound();
    if (turn && !count) return;
    if (turn && (netRoleNow() != NET_ROLE_HOST || player < 0)) {
        writeLogWarning("custom_afflictions_refused", "reason=not_the_host_read");
        return;
    }
    if (!turn) clearAllCustomAfflictions();
    if (!count) return;
    if (!ensureAfflictionStore()) return;
    int units = 0, entries = 0, dropped = 0, bad = 0;
    for (int i = 0; i < count; i++) {
        if (!readAfflictionsChunk(chunks[i], 0, player, &units, &entries, &dropped)) {
            bad++;
            writeLogWarning("custom_afflictions_read", "file=%s reason=bad_chunk chunk=%d bytes=%d", SAVE_FILE_NAMES[kind], i, chunks[i]->size);
            continue;
        }
        readAfflictionsChunk(chunks[i], 1, player, &units, &entries, &dropped);
    }
    if (!turn) forgetChangedAfflictionUnits();
    writeLog("custom_afflictions_read", "file=%s chunks=%d units=%d entries=%d dropped=%d bad=%d player=%d", SAVE_FILE_NAMES[kind], count, units,
             entries, dropped, bad, player);
}

// ---- Battle replays: C5RA after the replay's records (item_slot_replay.h) --------------------------------
static ReplayAffliction *replayAfflictions;
static int replayAfflictionCount, replayAfflictionCapacity;
static int *replayAfflictedUnits, replayAfflictedCount, replayAfflictedCapacity;

static int writeReplayAfflictions(FILE *file) {
    if (!anyCustomAfflictions()) return 0;
    SaveBuffer payload = {0};
    saveBufferInt(&payload, REPLAY_AFFLICTIONS_VERSION);
    saveBufferInt(&payload, customAfflictionCount);
    for (int k = 0; k < customAfflictionCount; k++) saveBufferString(&payload, customAfflictions[k].name);
    int units = 0, last = G32(RVA_UNIT_COUNT);
    for (int u = 0; u <= last && u < UNIT_TABLE_MAX; u++) {
        if (!unitAfflictionCount(u) || !replayWritesUnit(u)) continue;
        const AfflictionList *l = &unitAfflictionLists[u];
        saveBufferInt(&payload, u);
        saveBufferInt(&payload, l->count);
        for (int i = 0; i < l->count; i++) {
            unsigned char kind = l->entries[i].kind;
            saveBufferAdd(&payload, &kind, 1);
            saveBufferInt(&payload, l->entries[i].turn);
            saveBufferInt(&payload, l->entries[i].value);
        }
        units++;
    }
    saveBufferInt(&payload, -1);
    int ok = units && writeReplayChunk(file, REPLAY_AFFLICTIONS_TAG, &payload);
    if (units) writeLog("custom_afflictions_replay_written", "ok=%d units=%d bytes=%zu", ok, units, payload.size);
    free(payload.bytes);
    return ok;
}

static void forgetReplayAfflictions(void) { replayAfflictionCount = 0; }

static void readReplayAfflictions(const unsigned char *file, size_t size) {
    forgetReplayAfflictions();
    const unsigned char *at = NULL;
    int bytes = 0;
    if (findReplayChunk(file, size, REPLAY_AFFLICTIONS_TAG, &at, &bytes) <= 0) return;
    SaveCursor c = {at, at + bytes, 0};
    int version = saveCursorInt(&c), kinds = saveCursorInt(&c), index[CUSTOM_AFFLICTIONS_MAX], missing = 0;
    if (c.failed || version != REPLAY_AFFLICTIONS_VERSION || kinds < 0 || kinds > CUSTOM_AFFLICTIONS_MAX) {
        writeLogWarning("custom_afflictions_replay_read", "reason=bad_chunk version=%d", version);
        return;
    }
    for (int i = 0; i < kinds; i++) {
        char name[SAVE_STRING_MAX + 1];
        saveCursorString(&c, name, sizeof name);
        index[i] = c.failed ? -1 : findCustomAffliction(name);
    }
    for (;;) {
        int unit = saveCursorInt(&c);
        if (c.failed || unit == -1) break;
        int count = saveCursorInt(&c);
        if (c.failed || unit < 0 || unit >= UNIT_TABLE_MAX || count < 0 || count > kinds) {
            c.failed = 1;
            break;
        }
        for (int i = 0; i < count && !c.failed; i++) {
            unsigned kind = c.at < c.end ? *c.at++ : (c.failed = 1, 0u);
            int turn = saveCursorInt(&c), value = saveCursorInt(&c);
            if (c.failed || kind >= (unsigned)kinds) {
                c.failed = 1;
                break;
            }
            if (index[kind] < 0) {
                missing++;
                continue;
            }
            if (!growList((void **)&replayAfflictions, &replayAfflictionCapacity, replayAfflictionCount + 1, sizeof replayAfflictions[0])) {
                c.failed = 1;
                break;
            }
            replayAfflictions[replayAfflictionCount++] = (ReplayAffliction){unit, index[kind], turn, value};
        }
    }
    if (c.failed) {
        writeLogWarning("custom_afflictions_replay_read", "reason=bad_entries read=%d", replayAfflictionCount);
        forgetReplayAfflictions();
        return;
    }
    writeLog("custom_afflictions_replay_read", "read=%d missing=%d", replayAfflictionCount, missing);
}

static void giveReplayUnitAfflictions(int unit) {
    int given = 0;
    for (int i = 0; i < replayAfflictionCount; i++)
        if (replayAfflictions[i].unit == unit && ensureAfflictionStore())
            given += addAfflictionEntry(unit, replayAfflictions[i].kind, replayAfflictions[i].turn, replayAfflictions[i].value);
    if (!given) return;
    if (!growList((void **)&replayAfflictedUnits, &replayAfflictedCapacity, replayAfflictedCount + 1, sizeof *replayAfflictedUnits)) {
        writeLogError("custom_afflictions_replay_given", "reason=out_of_memory unit=%d", unit);
        clearUnitAfflictions(unit);
        return;
    }
    replayAfflictedUnits[replayAfflictedCount++] = unit;
}

static void endReplayAfflictions(void) {
    undoReplayAfflictionChanges();
    for (int i = 0; i < replayAfflictedCount; i++) clearUnitAfflictions(replayAfflictedUnits[i]);
    if (replayAfflictedCount) writeLog("custom_afflictions_replay_ended", "units=%d", replayAfflictedCount);
    replayAfflictedCount = 0;
    forgetReplayAfflictions();
}
