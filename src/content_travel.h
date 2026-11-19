#pragma once

#define CONTENT_WRITES_VERSION     1
#define CONTENT_WRITE_WIDTH_MAX    4096
#define FNV_OFFSET_64              0xcbf29ce484222325ull
#define FNV_PRIME_64               0x100000001b3ull

// ---- The pass window, and MODS_READ ---------------------------------------------------------------
static int contentPassDepth;

static int contentModsReading(void) { return contentPassDepth > 0; }

static void modsReadAsGame(void) {}

static void beginContentPass(void) { contentPassDepth++; }

static void endContentPass(void) {
    if (contentPassDepth <= 0) return;
    if (contentPassDepth == 1 && startupFinished) ((void (*)(void))siteEntry(SITE_MODS_READ, (void *)modsReadAsGame))();
    if (--contentPassDepth == 0) contentWrites.passCount = contentWrites.count;
}

// ---- The mods' fingerprint ------------------------------------------------------------------------
static u64 fnvAdd(u64 hash, const void *data, size_t size) {
    const unsigned char *p = data;
    for (size_t i = 0; i < size; i++) hash = (hash ^ p[i]) * FNV_PRIME_64;
    return hash;
}

static u64 contentModsFingerprint(void) {
    u64 hash = FNV_OFFSET_64;
    for (int m = 0; m <= G32(RVA_MOD_LAST_INDEX); m++) {
        if (fileLeavesModOut(m)) continue;
        const ModContentNote *note = findModContentNote(modName(m));
        if (note && note->contentFree) continue;
        u64 content = note && note->hashed ? note->hash : 0;
        hash = fnvAdd(hash, modName(m), strlen(modName(m)) + 1);
        hash = fnvAdd(hash, &content, sizeof content);
    }
    return hash;
}

// ---- Writing: CWRT --------------------------------------------------------------------------------
static int contentWriteKeyOrder(const ContentWrite *a, const ContentWrite *b) {
    int ownStore = a->kind == CONTENT_CASTER_MAX_CAST || a->kind == CONTENT_NOEXPLODE || a->kind == CONTENT_WEAPON_TRAITS;
    if (a->kind != b->kind) return a->kind < b->kind ? -1 : 1;
    if (a->index != b->index) return a->index < b->index ? -1 : 1;
    if (a->offset != b->offset) return a->offset < b->offset ? -1 : 1;
    if (!ownStore && a->width != b->width) return a->width < b->width ? -1 : 1;
    return 0;
}
static int compareContentWriteAt(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    int order = contentWriteKeyOrder(&contentWrites.writes[a], &contentWrites.writes[b]);
    return order ? order : (a > b) - (a < b);
}

static int contentWritesChunkEntries;

static void addContentWritesChunk(SaveList *list) {
    contentWritesChunkEntries = 0;
    if (saveTailPlayer >= 0 || netRoleNow() == NET_ROLE_OFFICIAL) return;
    int first = contentWrites.passCount, n = contentWrites.count - first;
    if (n <= 0) return;
    int *order = malloc((size_t)n * sizeof *order);
    unsigned char *kept = calloc((size_t)n, 1);
    if (!order || !kept) {
        free(order), free(kept);
        writeLogError("content_writes_dropped", "file=save reason=out_of_memory entries=%d", n);
        return;
    }
    for (int i = 0; i < n; i++) order[i] = first + i;
    qsort(order, (size_t)n, sizeof *order, compareContentWriteAt);
    for (int i = 0; i < n; i++)
        if (i == n - 1 || contentWriteKeyOrder(&contentWrites.writes[order[i]], &contentWrites.writes[order[i + 1]]))
            kept[order[i] - first] = 1;
    free(order);
    SaveBuffer chunk = {0};
    u64 mods = contentModsFingerprint();
    int count = 0;
    for (int i = 0; i < n; i++) count += kept[i];
    saveBufferInt(&chunk, CONTENT_WRITES_VERSION);
    saveBufferInt(&chunk, (int)(mods & 0xffffffffu));
    saveBufferInt(&chunk, (int)(mods >> 32));
    saveBufferInt(&chunk, count);
    for (int i = 0; i < n; i++) {
        if (!kept[i]) continue;
        const ContentWrite *w = &contentWrites.writes[first + i];
        saveBufferInt(&chunk, w->kind);
        saveBufferInt(&chunk, w->index);
        saveBufferInt(&chunk, w->offset);
        saveBufferInt(&chunk, w->width);
        saveBufferAdd(&chunk, w->bytes, (size_t)w->width);
    }
    free(kept);
    if (!chunk.failed) contentWritesChunkEntries = count;
    writeLog("content_writes_written", "entries=%d listed=%d bytes=%zu", count, n, chunk.size);
    saveListAdd(list, SAVE_TAG_CWRT, &chunk);
}

// ---- Reading --------------------------------------------------------------------------------------
static int undoPlayContentWrites(void) {
    int undone = 0;
    for (int i = contentWrites.count - 1; i >= contentWrites.passCount && i >= 0; i--) {
        ContentWrite *w = &contentWrites.writes[i];
        const char *reason = applyContentBytes(w->kind, w->index, w->offset, w->old, w->width);
        if (reason) writeLogWarning("content_write_not_undone", "reason=%s kind=%d index=%d offset=%d", reason, w->kind, w->index, w->offset);
        else undone++;
        free(w->bytes);
    }
    if (contentWrites.count > contentWrites.passCount) contentWrites.count = contentWrites.passCount;
    return undone;
}

typedef struct { int kind, index, offset, width; const unsigned char *bytes; } ContentWriteRead;

static const char *readContentWritesPayload(const SaveChunk *chunk, u64 *mods, ContentWriteRead **out, int *count) {
    SaveCursor c = {chunk->payload, chunk->payload + chunk->size, 0};
    *out = NULL, *count = 0;
    int version = saveCursorInt(&c);
    if (c.failed) return "bad_chunk";
    if (version != CONTENT_WRITES_VERSION) return "other_version";
    unsigned low = (unsigned)saveCursorInt(&c), high = (unsigned)saveCursorInt(&c);
    int n = saveCursorInt(&c);
    if (c.failed || n < 0 || n > chunk->size / 16) return "bad_chunk";
    *mods = (u64)low | (u64)high << 32;
    ContentWriteRead *e = n ? malloc((size_t)n * sizeof *e) : NULL;
    if (n && !e) return "out_of_memory";
    for (int i = 0; i < n; i++) {
        e[i].kind = saveCursorInt(&c), e[i].index = saveCursorInt(&c), e[i].offset = saveCursorInt(&c), e[i].width = saveCursorInt(&c);
        if (c.failed || e[i].kind < 0 || e[i].kind >= CONTENT_KIND_COUNT || e[i].offset < 0 || e[i].width < 1 ||
            e[i].width > CONTENT_WRITE_WIDTH_MAX || c.end - c.at < e[i].width) {
            free(e);
            return "bad_chunk";
        }
        e[i].bytes = c.at;
        c.at += e[i].width;
    }
    if (c.at != c.end) {
        free(e);
        return "bad_chunk";
    }
    *out = e, *count = n;
    return NULL;
}

// ---- Where an entry may land ----------------------------------------------------------------------
static uintptr_t contentTableRva(int kind) {
    switch (kind) {
    case CONTENT_MONSTER: return RVA_MONSTER_TABLE;
    case CONTENT_WEAPON: return RVA_WEAPON_TABLE;
    case CONTENT_ITEM: return RVA_ITEM_TABLE;
    case CONTENT_CLASS: return RVA_CLASS_TABLE;
    case CONTENT_CLASS_RECRUITS: return RVA_CLASS_SIDE_A;
    case CONTENT_TERRAIN: return RVA_TERRAIN_TABLE;
    case CONTENT_RITUAL: return RVA_RITUAL_TABLE;
    default: return 0;
    }
}

static int contentBytesInRange(const unsigned char *bytes, int width, lua_Integer min, lua_Integer max) {
    lua_Integer s, u;
    if (width == 1) s = (signed char)bytes[0], u = bytes[0];
    else if (width == 2) {
        short v;
        memcpy(&v, bytes, 2);
        s = v, u = (unsigned short)v;
    } else if (width == 4) {
        int v;
        memcpy(&v, bytes, 4);
        s = v, u = (unsigned int)v;
    } else if (width == 8) {
        long long v;
        memcpy(&v, bytes, 8);
        s = u = v;
    } else return 0;
    return (s >= min && s <= max) || (u >= min && u <= max);
}

static int contentBytesZero(const unsigned char *bytes, int width) {
    for (int i = 0; i < width; i++)
        if (bytes[i]) return 0;
    return 1;
}

enum { CONTENT_PLACE_NONE, CONTENT_PLACE_BROKEN, CONTENT_PLACE_TAKES };

static int contentValuePlace(const ContentWriteRead *e, int row) {
    const GameVar *v = findGameVar(CONTENT_VALUES[row].name);
    int kind = CONTENT_VALUES[row].kind;
    if (!v || kind == CONTENT_CASTER_MAX_CAST || kind != e->kind) return CONTENT_PLACE_NONE;
    long long base = (long long)(v->addr - contentTableRva(kind)), at = (long long)e->offset - base;
    int count2 = v->count2 > 1 ? v->count2 : 1, stride2 = v->count2 > 1 ? v->stride2 : 0;
    lua_Integer min, max;
    if (v->type == T_TERRAIN_INCOME) {
        for (int r = 0; r < count2; r++)
            if (terrainIncomeField(r) >= 0 && e->offset == terrainIncomeField(r) && e->width == 2) return CONTENT_PLACE_TAKES;
        return CONTENT_PLACE_NONE;
    }
    if (v->type == T_RITUAL_SUMMONS)
        return e->offset == RITUAL_FX && e->width == RITUAL_FX_MAX * RITUAL_FX_SIZE ? CONTENT_PLACE_TAKES : CONTENT_PLACE_NONE;
    if (v->type == T_CLASS_STDTROOPS || v->type == T_CLASS_ABILITY || v->type == T_MONSTER_ABILITY) return CONTENT_PLACE_NONE;
    if (at < 0 || (stride2 ? at % stride2 || at / stride2 >= count2 : at != 0)) return CONTENT_PLACE_NONE;
    if (v->type == T_STR) {
        int size = CONTENT_VALUES[row].nameSize;
        if (e->width == size) return e->bytes[size - 1] ? CONTENT_PLACE_BROKEN : CONTENT_PLACE_TAKES;
        return e->width == 1 && stride2 && at > 0 && gameVarShape(v).list == LIST_TO_EMPTY_NAME ? (e->bytes[0] ? CONTENT_PLACE_BROKEN : CONTENT_PLACE_TAKES)
                                                                                                : CONTENT_PLACE_NONE;
    }
    int width = v->type == T_MONSTER_ATTACK || v->type == T_I64 ? 8 : v->type == T_I16 ? 2 : 4;
    if (e->width != width) return CONTENT_PLACE_NONE;
    contentValueRange(v, &min, &max);
    if (contentBytesInRange(e->bytes, width, min, max)) return CONTENT_PLACE_TAKES;
    GameVarShape shape = gameVarShape(v);
    if (stride2 && at > 0 && !shape.nameBack && width == 4 &&
        ((shape.list == LIST_TO_BELOW_ZERO_I32 && contentBytesInRange(e->bytes, 4, -1, -1)) ||
         (shape.list == LIST_TO_BELOW_ONE_I32 && contentBytesZero(e->bytes, 4))))
        return CONTENT_PLACE_TAKES;
    return CONTENT_PLACE_BROKEN;
}

static const char *contentWriteEntryRefusal(const ContentWriteRead *e) {
    if (e->kind == CONTENT_CASTER_MAX_CAST)
        return e->offset || e->width != 2 ? "no_place" : contentBytesInRange(e->bytes, 2, 0, COMMANDER_CAST_MAX) ? NULL : "out_of_range";
    if (e->kind == CONTENT_NOEXPLODE) return e->offset || e->width != 1 ? "no_place" : e->bytes[0] > 1 ? "out_of_range" : NULL;
    if (e->kind == CONTENT_WEAPON_TRAITS) return NULL;
    int broken = 0;
    for (size_t i = 0; e->kind <= CONTENT_RITUAL && i < sizeof RECORD_WORDS / sizeof RECORD_WORDS[0]; i++) {
        const RecordWord *row = &RECORD_WORDS[i];
        if (row->record != e->kind || row->offset != e->offset || row->kind == WORD_ABILITY) continue;
        if (row->kind == WORD_FLAG) {
            if (e->width == (row->size == 4 ? 4 : 8)) return NULL;
            continue;
        }
        if (e->width != row->size) continue;
        lua_Integer min, max;
        recordWordRange(e->kind, row, &min, &max);
        if (contentBytesInRange(e->bytes, e->width, min, max)) return NULL;
        broken = 1;
    }
    if (e->kind == CONTENT_MONSTER || e->kind == CONTENT_ITEM || e->kind == CONTENT_CLASS || e->kind == CONTENT_TERRAIN || e->kind == CONTENT_RITUAL) {
        AbilityPairs a = abilityPairsOf(e->kind);
        long long at = (long long)e->offset - a.offset;
        if (at >= 0 && at < (long long)a.count * a.stride && at % a.stride == 0) {
            long long id = 0;
            if (e->width == a.stride) memcpy(&id, e->bytes, (size_t)a.idWidth);
            if (e->width == a.stride && (a.idWidth == 8 ? id : (int)id) >= 1) {
                if (a.valueWidth == 8 || contentBytesInRange(e->bytes + a.valueAt, 4, INT_MIN, INT_MAX)) return NULL;
            } else if (e->width == a.idWidth && contentBytesZero(e->bytes, e->width)) return NULL;
            broken = 1;
        }
    }
    if (e->kind == CONTENT_WEAPON && e->offset == WEAPON_SOUND_PAIRS && e->width == (int)(2 * WEAPON_SOUND_PAIRS_MAX * sizeof(int))) return NULL;
    for (int row = 0; row < (int)(sizeof CONTENT_VALUES / sizeof CONTENT_VALUES[0]); row++) {
        int fares = contentValuePlace(e, row);
        if (fares == CONTENT_PLACE_TAKES) return NULL;
        broken = broken || fares == CONTENT_PLACE_BROKEN;
    }
    return broken ? "out_of_range" : "no_place";
}

#define CONTENT_WRITES_DROPS_LOGGED 8

static void useContentWritesChunk(const SaveChunk *chunk, SaveFile kind) {
    if (kind != SAVE_FILE_SAVE || netRoleNow() == NET_ROLE_OFFICIAL) return;
    int undone = undoPlayContentWrites(), applied = 0, dropped = 0, n = 0;
    const char *reason = NULL;
    ContentWriteRead *e = NULL;
    u64 mods = 0;
    if (chunk) reason = readContentWritesPayload(chunk, &mods, &e, &n);
    if (chunk && !reason && n && mods != contentModsFingerprint()) {
        writeLogWarning("content_writes_dropped", "file=save reason=mods_differ entries=%d", n);
        dropped = n, n = 0;
    }
    if (reason) writeLogWarning("content_writes_dropped", "file=save reason=%s", reason);
    for (int i = 0; i < n; i++) {
        const char *why = contentWriteEntryRefusal(&e[i]);
        if (!why) why = writeContent(e[i].kind, e[i].index, e[i].offset, e[i].bytes, e[i].width);
        if (why) {
            if (dropped < CONTENT_WRITES_DROPS_LOGGED)
                writeLogWarning("content_writes_dropped", "file=save reason=%s at=%d kind=%d index=%d offset=%d width=%d", why, i, e[i].kind, e[i].index,
                                e[i].offset, e[i].width);
            dropped++;
        } else applied++;
    }
    free(e);
    if (chunk || undone) writeLog("content_writes_read", "undone=%d applied=%d dropped=%d role=%d", undone, applied, dropped, netRoleNow());
}
