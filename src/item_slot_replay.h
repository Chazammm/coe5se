// @platform-split item_slot_replay: Windows hooks RVA_SAVE_BATTLE_REPLAY and RVA_LOAD_BATTLE_REPLAY at their entries; Mac inlines both, so the writer's fclose call is redirected and the replay is read from tmp_shom.vcr
#pragma once

#define REPLAY_ITEMS_TAG      SAVE_TAG('C', '5', 'R', 'I')
#define REPLAY_STORE_TAG      SAVE_TAG('C', '5', 'L', 'S')
#define REPLAY_END_TAG        SAVE_TAG('C', '5', 'R', 'E')
#define REPLAY_ITEMS_VERSION  1
#define REPLAY_ITEMS_TRAILER  8
#define REPLAY_HEADER_BYTES   22
#define REPLAY_FILE_MAX       (64 << 20)
#define REPLAY_VIEW_FILE      "tmp_shom.vcr"
#define REPLAY_WRITE_FILE     "tmp_cbat.vcr"
static void noteBattleReplayWritten(const char *path);
static int replayCarriesScripts(void);
static void writeReplayScriptStore(FILE *file);

typedef struct { int unit, position, item; } ReplayItem;
static ReplayItem *replayItems;
static int replayItemCount;
static int replayItemsRead;
static int *replayGivenUnits;
static int replayGivenCount, replayGivenCapacity;
#ifndef __APPLE__
static const char *replayLoadPath;
#endif

static int replayWritesUnit(int unit) {
    size_t at = (size_t)unit * UNIT_SIZE;
    return G32(RVA_UNIT_X + at) == G32(RVA_BATTLE_X) && G32(RVA_UNIT_X + 4 + at) == G32(RVA_BATTLE_Y) && (short)G16(RVA_UNIT_HP + at) > 0;
}

static int buildReplayItems(SaveBuffer *out) {
    if (!anyExtraItems()) return 0;
    int entries = 0, last = G32(RVA_UNIT_COUNT);
    for (int u = 0; u <= last && u < UNIT_TABLE_MAX; u++)
        if (unitExtraItemCount(u) && replayWritesUnit(u)) entries += unitExtraItemCount(u);
    if (!entries) return 0;
    SaveBuffer payload = {0};
    saveBufferInt(&payload, REPLAY_ITEMS_VERSION);
    saveBufferInt(&payload, entries);
    for (int u = 0; u <= last && u < UNIT_TABLE_MAX; u++) {
        if (!unitExtraItemCount(u) || !replayWritesUnit(u)) continue;
        for (int p = UNIT_ITEM_SLOTS; p < ITEM_POSITION_MAX; p++) {
            int item = unitItemAt(u, p);
            if (item < 0) continue;
            saveBufferInt(&payload, u);
            saveBufferInt(&payload, p);
            saveBufferString(&payload, (const char *)(tableAddress(TABLE_ITEM) + (size_t)item * ITEM_SIZE));
        }
    }
    saveBufferAdd(out, payload.bytes, payload.size);
    saveBufferInt(out, (int)payload.size);
    saveBufferInt(out, REPLAY_ITEMS_TAG);
    int failed = payload.failed || out->failed;
    free(payload.bytes);
    if (failed) {
        writeLogError("item_slots_replay_written", "ok=0 reason=out_of_memory_or_too_big entries=%d", entries);
        return 0;
    }
    writeLog("item_slots_replay_written", "entries=%d bytes=%zu", entries, out->size);
    return entries;
}

static int writeReplayItems(FILE *file) {
    SaveBuffer bytes = {0};
    int entries = buildReplayItems(&bytes);
    if (entries && fwrite(bytes.bytes, 1, bytes.size, file) != bytes.size) {
        writeLogError("item_slots_replay_written", "ok=0 reason=write_failed entries=%d", entries);
        entries = 0;
    }
    free(bytes.bytes);
    return entries;
}

static void forgetReplayItems(void) {
    free(replayItems);
    replayItems = NULL;
    replayItemCount = 0;
}

static int findReplayChunk(const unsigned char *file, size_t size, int tag, const unsigned char **payload, int *payloadSize) {
    size_t end = size;
    while (end >= REPLAY_HEADER_BYTES + REPLAY_ITEMS_TRAILER) {
        SaveCursor c = {file + end - REPLAY_ITEMS_TRAILER, file + end, 0};
        int bytes = saveCursorInt(&c), found = saveCursorInt(&c);
        if (found != REPLAY_ITEMS_TAG && found != REPLAY_STORE_TAG && found != REPLAY_END_TAG) return 0;
        if (bytes < 0 || (size_t)bytes > end - REPLAY_HEADER_BYTES - REPLAY_ITEMS_TRAILER) {
            writeLogWarning("replay_trailer_read", "reason=bad_size bytes=%d file=%zu", bytes, size);
            return -1;
        }
        end -= REPLAY_ITEMS_TRAILER;
        if (found == tag) {
            *payload = file + end - (size_t)bytes;
            *payloadSize = bytes;
            return 1;
        }
        end -= (size_t)bytes;
    }
    return 0;
}

static int writeReplayChunk(FILE *file, int tag, const SaveBuffer *payload) {
    SaveBuffer chunk = {0};
    saveBufferAdd(&chunk, payload->bytes, payload->size);
    saveBufferInt(&chunk, (int)payload->size);
    saveBufferInt(&chunk, tag);
    int ok = !chunk.failed && !payload->failed && fwrite(chunk.bytes, 1, chunk.size, file) == chunk.size;
    if (!ok) writeLogError("replay_trailer_written", "ok=0 reason=write_failed tag=%08x bytes=%zu", (unsigned)tag, chunk.size);
    free(chunk.bytes);
    return ok;
}

static int readReplayItems(const unsigned char *file, size_t size) {
    forgetReplayItems();
    const unsigned char *payloadAt = NULL;
    int payloadSize = 0;
    int found = findReplayChunk(file, size, REPLAY_ITEMS_TAG, &payloadAt, &payloadSize);
    if (found <= 0) return found;
    if (payloadSize < 8) {
        writeLogWarning("item_slots_replay_read", "reason=bad_size bytes=%d file=%zu", payloadSize, size);
        return -1;
    }
    SaveCursor c = {payloadAt, payloadAt + payloadSize, 0};
    int version = saveCursorInt(&c), entries = saveCursorInt(&c);
    if (version != REPLAY_ITEMS_VERSION || entries < 0 || entries > payloadSize / 9) {
        writeLogWarning("item_slots_replay_read", "reason=%s version=%d entries=%d", version != REPLAY_ITEMS_VERSION ? "other_version" : "bad_entries",
                        version, entries);
        return -1;
    }
    replayItems = entries ? malloc((size_t)entries * sizeof replayItems[0]) : NULL;
    if (entries && !replayItems) {
        writeLogError("item_slots_replay_read", "reason=out_of_memory entries=%d", entries);
        return -1;
    }
    int skipped = 0, missing = 0;
    for (int i = 0; i < entries; i++) {
        int unit = saveCursorInt(&c), position = saveCursorInt(&c);
        char name[SAVE_STRING_MAX + 1];
        saveCursorString(&c, name, sizeof name);
        if (c.failed) break;
        if (unit < 0 || unit >= UNIT_TABLE_MAX || position < UNIT_ITEM_SLOTS || position >= ITEM_POSITION_MAX) {
            skipped++;
            continue;
        }
        int item = itemSlotsItemNamed(name);
        if (item < 0) {
            missing++;
            continue;
        }
        replayItems[replayItemCount++] = (ReplayItem){unit, position, item};
    }
    if (c.failed) {
        writeLogWarning("item_slots_replay_read", "reason=bad_entries read=%d entries=%d", replayItemCount, entries);
        forgetReplayItems();
        return -1;
    }
    if (skipped || missing)
        writeLogWarning("item_slots_replay_read", "reason=bad_entries read=%d skipped=%d missing=%d", replayItemCount, skipped, missing);
    else writeLog("item_slots_replay_read", "read=%d", replayItemCount);
    return replayItemCount;
}

static int replayFilePath(char *out, size_t size) {
#ifndef __APPLE__
    if (replayLoadPath) return buildPath(out, size, __func__, "%s", replayLoadPath);
#endif
    return buildPath(out, size, __func__, "%s/" REPLAY_VIEW_FILE, (const char *)GAME_PTR(RVA_TEMP_DIR));
}

static int readReplayFileBytes(const char *event, unsigned char **bytes, size_t *size) {
    *bytes = NULL, *size = 0;
    char path[2600];
    if (!replayFilePath(path, sizeof path)) return 0;
    FILE *file = fopen(path, "rb");
    if (!file) {
        writeLogWarning(event, "reason=no_file");
        return 0;
    }
    long length = fseek(file, 0, SEEK_END) == 0 ? ftell(file) : -1;
    int ok = length >= 0 && length <= REPLAY_FILE_MAX && fseek(file, 0, SEEK_SET) == 0 && (*bytes = malloc((size_t)length + 1)) &&
             fread(*bytes, 1, (size_t)length, file) == (size_t)length;
    fclose(file);
    if (!ok) {
        writeLogWarning(event, "reason=unreadable bytes=%ld", length);
        free(*bytes);
        *bytes = NULL;
        return 0;
    }
    *size = (size_t)length;
    return 1;
}

static void readReplayFileItems(void) {
    replayItemsRead = 1;
    unsigned char *bytes;
    size_t size;
    if (!readReplayFileBytes("item_slots_replay_read", &bytes, &size)) return;
    readReplayItems(bytes, size);
    free(bytes);
}

static int replayLoadingUnits(void) { return *(const unsigned char *)GAME_PTR(RVA_BATTLE_IS_REPLAY) != 0; }

static void giveReplayUnitItems(int unit) {
    if (!replayItemsRead) readReplayFileItems();
    int given = 0;
    for (int i = 0; i < replayItemCount; i++)
        if (replayItems[i].unit == unit) given += setUnitItemAt(unit, replayItems[i].position, replayItems[i].item);
    if (!given) return;
    if (replayGivenCount == replayGivenCapacity) {
        int capacity = replayGivenCapacity ? replayGivenCapacity * 2 : 64;
        int *units = realloc(replayGivenUnits, (size_t)capacity * sizeof *units);
        if (!units) {
            writeLogError("item_slots_replay_given", "reason=out_of_memory unit=%d", unit);
            clearUnitExtraItems(unit);
            return;
        }
        replayGivenUnits = units;
        replayGivenCapacity = capacity;
    }
    replayGivenUnits[replayGivenCount++] = unit;
}

static void endReplayItems(void) {
    for (int i = 0; i < replayGivenCount; i++) clearUnitExtraItems(replayGivenUnits[i]);
    if (replayGivenCount) writeLog("item_slots_replay_ended", "units=%d", replayGivenCount);
    replayGivenCount = 0;
    replayItemsRead = 0;
    forgetReplayItems();
}

static int itemSlotReplayState;

#ifndef __APPLE__
// ---- Windows: the writer and the loader are functions ---------------------------------------------------------
typedef void (*BattleReplayFileFn)(const char *path);
static BattleReplayFileFn gameSaveBattleReplay, gameLoadBattleReplay;

static void saveBattleReplayHook(const char *path) {
    gameSaveBattleReplay(path);
    noteBattleReplayWritten(path);
    int items = anyExtraItems(), store = replayCarriesScripts();
    if (!items && !store) return;
    FILE *file = fopen(path, "ab");
    if (!file) {
        writeLogError("item_slots_replay_written", "ok=0 reason=no_file");
        return;
    }
    if (items) writeReplayItems(file);
    if (store) writeReplayScriptStore(file);
    fclose(file);
}

static void loadBattleReplayHook(const char *path) {
    replayLoadPath = path;
    replayItemsRead = 0;
    gameLoadBattleReplay(path);
    replayLoadPath = NULL;
}

static int installItemSlotReplaySites(void) {
    struct { unsigned int rva; void *hook; void **game; } hooks[] = {
        {RVA_SAVE_BATTLE_REPLAY, (void *)saveBattleReplayHook, (void **)&gameSaveBattleReplay},
        {RVA_LOAD_BATTLE_REPLAY, (void *)loadBattleReplayHook, (void **)&gameLoadBattleReplay},
    };
    for (size_t i = 0; i < sizeof hooks / sizeof hooks[0]; i++) {
        void *target = gameBase + hooks[i].rva;
        if (!fanpatchHookCreate(target, hooks[i].hook, hooks[i].game) || !fanpatchHookEnable(target, 1)) return 0;
    }
    return 1;
}
#else
// ---- Mac: the writer is inline in RVA_DO_ALL_FIGHTS ------------------------------------------------------------
static int closeBattleReplayWritten(FILE *file) {
    writeReplayItems(file);
    if (replayCarriesScripts()) writeReplayScriptStore(file);
    int closed = fclose(file);
    char path[2600];
    if (buildPath(path, sizeof path, __func__, "%s/" REPLAY_WRITE_FILE, (const char *)GAME_PTR(RVA_TEMP_DIR))) noteBattleReplayWritten(path);
    return closed;
}

static int installItemSlotReplaySites(void) {
    unsigned char *at = gameBase + RVA_REPLAY_WRITE_FCLOSE_CALL;
    if (blTarget(at) != gameBase + RVA_FCLOSE) {
        writeLogError("hook_skipped", "site=item_slot_replay_write reason=not_the_expected_call");
        return 0;
    }
    unsigned char *trampoline = newTrampoline((void *)closeBattleReplayWritten);
    if (!trampoline) {
        writeLogError("hook_skipped", "site=item_slot_replay_write reason=no_padding");
        return 0;
    }
    uint32_t word = blTo(at, trampoline);
    return writeGameCode(at, &word, 4);
}
#endif

static int installItemSlotReplay(void) {
    if (itemSlotReplayState) return itemSlotReplayState > 0;
    itemSlotReplayState = installItemSlotReplaySites() ? 1 : -1;
    writeResultLog(itemSlotReplayState > 0, "item_slot_replay_installed", "sites=%d", itemSlotReplayState > 0 ? 2 : 0);
    return itemSlotReplayState > 0;
}
