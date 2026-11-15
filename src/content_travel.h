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
        if (c.failed || e[i].kind < 0 || e[i].kind >= CONTENT_KIND_COUNT || e[i].width < 1 || e[i].width > CONTENT_WRITE_WIDTH_MAX ||
            c.end - c.at < e[i].width) {
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
        const char *why = writeContent(e[i].kind, e[i].index, e[i].offset, e[i].bytes, e[i].width);
        if (why) {
            writeLogWarning("content_writes_dropped", "file=save reason=%s at=%d kind=%d index=%d offset=%d", why, i, e[i].kind, e[i].index, e[i].offset);
            dropped++;
        } else applied++;
    }
    free(e);
    if (chunk || undone) writeLog("content_writes_read", "undone=%d applied=%d dropped=%d role=%d", undone, applied, dropped, netRoleNow());
}
