#pragma once

#define TRANSPORT_ENTRIES_MAX       1000
#define TRANSPORT_FROM_EARLIER_PASS 1
#define TRANSPORT_TEXT_SIZE         MAX_PATH

typedef struct {
    char mod[MOD_NAME_MAX];
    char file[MAX_PATH];
    int line, from;
    char monsterName[MONSTER_NAME_SIZE];
    int monster;
    char battlemap[TRANSPORT_TEXT_SIZE];
    char heart[TRANSPORT_TEXT_SIZE];
    char terrain[TRANSPORT_TEXT_SIZE];
    int heartIsNumber, terrainIsNumber;
    int media;
    char folder[MAX_PATH];
    char mapFolder[MAX_PATH];
    int words;
    int fromWords;
} TransportEntry;
#define CARRY_LAND 1
#define CARRY_AIR  2
static TransportEntry *transportEntries;
static int transportEntryCount, transportEntryCapacity;

#define TRANSPORT_WORD_MAP     1
#define TRANSPORT_WORD_HEART   2
#define TRANSPORT_WORD_TERRAIN 4
static const char *const TRANSPORT_WORD_NAMES[3] = {"transportmap", "transportheart", "transportterrain"};
static const char *const TRANSPORT_WORD_FIELDS[3] = {"battlemap", "heart", "terrain"};

typedef struct {
    char mod[MOD_NAME_MAX];
    char file[MAX_PATH];
    int line;
} TransportWordLine;

typedef struct {
    int monster, has;
    TransportWordLine at[3];
    char mapFolder[MAX_PATH];
    char battlemap[TRANSPORT_TEXT_SIZE], heart[TRANSPORT_TEXT_SIZE], terrain[TRANSPORT_TEXT_SIZE];
    int heartIsNumber, terrainIsNumber;
} TransportWords;
static TransportWords *transportWords;
static int transportWordCount, transportWordCapacity;

static const char *transportDeckRefusal(const TransportEntry *e, const char *folder);
static void decksOnTransportAdded(const TransportEntry *e);
static void dropTransportDeck(const char *monsterName, int fromWords);
static void transportDecksAfterPass(void);
static const char *transportSiteRefusal(const TransportEntry *e);
static void carriersOnTransportAdded(void);

static int transportMonsterNamed(int monster, const char *name) {
    const unsigned char *record = monster >= 0 && monster <= G32(RVA_MONSTER_COUNT) ? monsterRecord(monster) : NULL;
    return record && recordNameIs(record, MONSTER_NAME_SIZE, name);
}

static void refindTransportMonster(TransportEntry *e) {
    if (transportMonsterNamed(e->monster, e->monsterName)) return;
    int now = monsterByName(e->monsterName);
    if (now != e->monster) writeLog("transport_monster_moved", "mod=%s monster=%s from=%d to=%d", e->mod, e->monsterName, e->monster, now);
    e->monster = now;
}

static int transportEntryLive(const TransportEntry *e) { return e->fromWords || gameDataScriptOf(e->mod) != NULL; }

static TransportEntry *transportEntryForMonster(int monsterId) {
    if (monsterId < 0) return NULL;
    TransportEntry *words = NULL;
    for (int i = 0; i < transportEntryCount; i++) {
        TransportEntry *e = &transportEntries[i];
        refindTransportMonster(e);
        if (e->monster != monsterId || e->from == TRANSPORT_FROM_EARLIER_PASS || !transportEntryLive(e)) continue;
        if (!e->fromWords) return e;
        words = e;
    }
    return words;
}

static void markScriptTransportsRestarting(const char *mod) {
    for (int i = 0; i < transportEntryCount; i++)
        if (!transportEntries[i].fromWords && !strcmp(transportEntries[i].mod, mod)) transportEntries[i].from = TRANSPORT_FROM_EARLIER_PASS;
}

// ---- The .c5m words: transportmap, transportheart, transportterrain -------------------------------
static TransportWords *transportWordsOf(int monster) {
    for (int i = 0; i < transportWordCount; i++)
        if (transportWords[i].monster == monster) return &transportWords[i];
    return NULL;
}

static void clearTransportWords(int monster) {
    TransportWords *w = transportWordsOf(monster);
    if (w) *w = transportWords[--transportWordCount];
}

static void copyTransportWords(int source, int target) {
    if (source == target) return;
    clearTransportWords(target);
    const TransportWords *from = transportWordsOf(source);
    if (!from) return;
    TransportWords copy = *from;
    if (!growList((void **)&transportWords, &transportWordCapacity, transportWordCount + 1, sizeof *transportWords)) {
        writeLogError("transport_words_refused", "reason=out_of_memory call=copystats source=%d target=%d", source, target);
        return;
    }
    copy.monster = target;
    transportWords[transportWordCount++] = copy;
}

static void transportWordsBeforePass(int level) {
    if (level == 2) transportWordCount = 0;
}

static void transportWordLine(int i, const char *args, int mod) {
    static const char *const BAD[3] = {"bad_battlemap", "bad_heart", "bad_terrain"};
    const char *p = args, *reason = NULL;
    char text[TRANSPORT_TEXT_SIZE];
    long long n = 0;
    int isNumber = 0;
    skipSpaces(&p);
    if (*p == '"') {
        int len = readSpellQuoted(&p, text, sizeof text);
        reason = len < 0 ? BAD[i] : !len || len >= (int)sizeof text ? "bad_text" : NULL;
    } else if (i > 0 && readSpellNumber(&p, &n)) {
        snprintf(text, sizeof text, "%lld", n);
        isNumber = 1;
    } else {
        reason = BAD[i];
    }
    if (!reason && (skipSpaces(&p), !isLineEnd(p))) reason = BAD[i];
    if (reason) {
        refuseC5mLine(TRANSPORT_WORD_NAMES[i], spellModName(mod), reason, "line=%d text=%.40s", G32(RVA_MOD_LINE), args);
        return;
    }
    int monster = G32(RVA_MONSTER_CURRENT);
    if (!monsterRecord(monster)) {
        refuseC5mLine(TRANSPORT_WORD_NAMES[i], spellModName(mod), "no_monster", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    TransportWords *w = transportWordsOf(monster);
    if (!w) {
        if (!growList((void **)&transportWords, &transportWordCapacity, transportWordCount + 1, sizeof *transportWords)) {
            refuseC5mLine(TRANSPORT_WORD_NAMES[i], spellModName(mod), "out_of_memory", "line=%d", G32(RVA_MOD_LINE));
            return;
        }
        w = &transportWords[transportWordCount++];
        memset(w, 0, sizeof *w);
        w->monster = monster;
    }
    char *field = i == 0 ? w->battlemap : i == 1 ? w->heart : w->terrain;
    snprintf(field, TRANSPORT_TEXT_SIZE, "%s", text);
    if (i == 1) w->heartIsNumber = isNumber;
    if (i == 2) w->terrainIsNumber = isNumber;
    if (i == 0) snprintf(w->mapFolder, sizeof w->mapFolder, "%s", modFolder(mod));
    w->has |= 1 << i;
    TransportWordLine *at = &w->at[i];
    snprintf(at->mod, sizeof at->mod, "%s", spellModName(mod));
    snprintf(at->file, sizeof at->file, "%s", (const char *)GAME_PTR(RVA_EVENT_FILE));
    at->line = G32(RVA_MOD_LINE);
}
static void transportmapLine(const char *args, int mod, int level) {
    if (level == 2) transportWordLine(0, args, mod);
}
static void transportheartLine(const char *args, int mod, int level) {
    if (level == 2) transportWordLine(1, args, mod);
}
static void transportterrainLine(const char *args, int mod, int level) {
    if (level == 2) transportWordLine(2, args, mod);
}

static void transportWordsInit(void) {
    int map = apiAddModCommand("transportmap", transportmapLine), heart = apiAddModCommand("transportheart", transportheartLine),
        terrain = apiAddModCommand("transportterrain", transportterrainLine);
    writeLog("transport_words_ready", "transportmap=%d transportheart=%d transportterrain=%d", map, heart, terrain);
}

static void refuseTransportWord(TransportWords *w, int i, const char *reason, const char *monsterName) {
    const TransportWordLine *at = &w->at[i];
    w->has &= ~(1 << i);
    refuseC5mLine(TRANSPORT_WORD_NAMES[i], at->mod, reason, "line=%d monster=%.60s", at->line, monsterName);
    if (!strcmp(reason, "unknown_terrain")) addPlaneProblem(0, at->mod, at->file, at->line, "No terrain called '%.60s'", w->terrain);
    else if (!strcmp(reason, "unknown_heart")) addPlaneProblem(0, at->mod, at->file, at->line, "No monster called '%.60s'", w->heart);
    else if (!strcmp(reason, "file_not_found") || !strcmp(reason, "file_wrong_case"))
        addPlaneProblem(0, at->mod, at->file, at->line, "No file '%.60s'", w->battlemap);
}

static int isTransportHeartRefusal(const char *reason) {
    return !strcmp(reason, "unknown_heart") || !strcmp(reason, "heart_without_battlemap") || !strcmp(reason, "heart_not_on_deck");
}

static void putTransportWords(TransportEntry *e, const TransportWords *w, int mask) {
    if (mask & TRANSPORT_WORD_MAP) snprintf(e->battlemap, sizeof e->battlemap, "%s", w->battlemap), snprintf(e->mapFolder, sizeof e->mapFolder, "%s", w->mapFolder);
    if (mask & TRANSPORT_WORD_HEART) snprintf(e->heart, sizeof e->heart, "%s", w->heart), e->heartIsNumber = w->heartIsNumber;
    if (mask & TRANSPORT_WORD_TERRAIN) snprintf(e->terrain, sizeof e->terrain, "%s", w->terrain), e->terrainIsNumber = w->terrainIsNumber;
}

static void stripTransportWords(TransportEntry *e, int mask) {
    mask &= e->words;
    if (mask & TRANSPORT_WORD_MAP) e->battlemap[0] = '\0', snprintf(e->mapFolder, sizeof e->mapFolder, "%s", e->folder);
    if (mask & TRANSPORT_WORD_HEART) e->heart[0] = '\0', e->heartIsNumber = 0;
    if (mask & TRANSPORT_WORD_TERRAIN) e->terrain[0] = '\0', e->terrainIsNumber = 0;
    e->words &= ~mask;
}

static void mergeTransportWords(TransportEntry *e) {
    const TransportWords *w = transportWordsOf(e->monster);
    if (!w) return;
    for (int i = 0; i < 3; i++) {
        int bit = 1 << i;
        if (!(w->has & bit)) continue;
        const char *own = i == 0 ? e->battlemap : i == 1 ? e->heart : e->terrain;
        if (own[0]) {
            writeLog("transport_word_overridden", "mod=%s monster=%s field=%s word=%s word_mod=%s word_line=%d", e->mod, e->monsterName,
                     TRANSPORT_WORD_FIELDS[i], TRANSPORT_WORD_NAMES[i], w->at[i].mod, w->at[i].line);
            continue;
        }
        putTransportWords(e, w, bit);
        e->words |= bit;
    }
}

static void declareWordTransports(void) {
    int made = 0;
    for (int k = 0; k < transportWordCount; k++) {
        TransportWords *w = &transportWords[k];
        const unsigned char *record = monsterRecord(w->monster);
        if (!record || !record[0]) continue;
        TransportEntry e;
        memset(&e, 0, sizeof e);
        e.fromWords = 1, e.monster = w->monster;
        snprintf(e.monsterName, sizeof e.monsterName, "%s", (const char *)record);
        int first = w->has & TRANSPORT_WORD_MAP ? 0 : w->has & TRANSPORT_WORD_HEART ? 1 : 2;
        snprintf(e.mod, sizeof e.mod, "%s", w->at[first].mod);
        snprintf(e.file, sizeof e.file, "%s", w->at[first].file);
        e.line = w->at[first].line;
        const char *reason = NULL;
        if (w->has & TRANSPORT_WORD_TERRAIN) {
            putTransportWords(&e, w, TRANSPORT_WORD_TERRAIN);
            if ((reason = transportSiteRefusal(&e))) refuseTransportWord(w, 2, reason, e.monsterName), e.terrain[0] = '\0', e.terrainIsNumber = 0;
        }
        putTransportWords(&e, w, w->has & (TRANSPORT_WORD_MAP | TRANSPORT_WORD_HEART));
        while ((e.battlemap[0] || e.heart[0]) && (reason = transportDeckRefusal(&e, e.mapFolder))) {
            int heart = isTransportHeartRefusal(reason) && e.heart[0];
            refuseTransportWord(w, heart ? 1 : 0, reason, e.monsterName);
            if (heart) e.heart[0] = '\0', e.heartIsNumber = 0;
            else e.battlemap[0] = '\0';
        }
        if (!w->has) continue;
        if (made >= TRANSPORT_ENTRIES_MAX || !growList((void **)&transportEntries, &transportEntryCapacity, transportEntryCount + 1, sizeof *transportEntries)) {
            refuseTransportWord(w, first, made >= TRANSPORT_ENTRIES_MAX ? "too_many" : "out_of_memory", e.monsterName);
            continue;
        }
        made++;
        TransportEntry *added = &transportEntries[transportEntryCount++];
        *added = e;
        writeLog("transport_words_declared", "mod=%s monster=%d name=%s battlemap=%s heart=%s terrain=%s line=%d", e.mod, e.monster, e.monsterName,
                 e.battlemap[0] ? e.battlemap : "-", e.heart[0] ? e.heart : "-", e.terrain[0] ? e.terrain : "-", e.line);
        decksOnTransportAdded(added);
    }
}

static void remergeTransportWords(TransportEntry *e) {
    char map[TRANSPORT_TEXT_SIZE], heart[TRANSPORT_TEXT_SIZE], folder[MAX_PATH];
    snprintf(map, sizeof map, "%s", e->battlemap), snprintf(heart, sizeof heart, "%s", e->heart), snprintf(folder, sizeof folder, "%s", e->mapFolder);
    stripTransportWords(e, TRANSPORT_WORD_MAP | TRANSPORT_WORD_HEART | TRANSPORT_WORD_TERRAIN);
    mergeTransportWords(e);
    if (!strcmp(map, e->battlemap) && !strcmp(heart, e->heart) && !strcmp(folder, e->mapFolder)) return;
    dropTransportDeck(e->monsterName, 0);
    const char *reason = e->battlemap[0] || e->heart[0] ? transportDeckRefusal(e, e->mapFolder) : NULL;
    if (reason && e->words & (TRANSPORT_WORD_MAP | TRANSPORT_WORD_HEART)) {
        writeLogWarning("transport_words_not_merged", "mod=%s monster=%s reason=%s", e->mod, e->monsterName, reason);
        stripTransportWords(e, TRANSPORT_WORD_MAP | TRANSPORT_WORD_HEART);
        reason = e->battlemap[0] || e->heart[0] ? transportDeckRefusal(e, e->mapFolder) : NULL;
    }
    if (reason) writeLogWarning("transport_deck_dropped", "reason=%s mod=%s monster=%s path=%s", reason, e->mod, e->monsterName, e->battlemap);
    else decksOnTransportAdded(e);
}

static void transportsAfterPass(int level) {
    if (level != 2) return;
    int kept = 0;
    for (int i = 0; i < transportEntryCount; i++) {
        TransportEntry *e = &transportEntries[i];
        if (e->fromWords) {
            dropTransportDeck(e->monsterName, 1);
            continue;
        }
        if (e->from == TRANSPORT_FROM_EARLIER_PASS) {
            writeLog("transport_dropped", "mod=%s monster=%s reason=not_declared_again", e->mod, e->monsterName);
            dropTransportDeck(e->monsterName, 0);
            continue;
        }
        refindTransportMonster(e);
        if (kept != i) transportEntries[kept] = *e;
        kept++;
    }
    transportEntryCount = kept;
    int calls = transportEntryCount;
    declareWordTransports();
    for (int i = 0; i < calls; i++) remergeTransportWords(&transportEntries[i]);
    transportDecksAfterPass();
}

static int luaTransportFlag(lua_State *L, const char *field) {
    lua_getfield(L, 1, field);
    int type = lua_type(L, -1), on = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return type == LUA_TNIL ? 0 : type == LUA_TBOOLEAN ? on : -1;
}

static const char *luaTransportText(lua_State *L, const char *field, int numberToo, const char *badType, char *out, size_t size, int *isNumber) {
    lua_getfield(L, 1, field);
    const char *reason = NULL;
    if (lua_type(L, -1) == LUA_TSTRING) {
        size_t len;
        const char *s = lua_tolstring(L, -1, &len);
        if (!len || len >= size || strlen(s) != len) reason = "bad_text";
        else snprintf(out, size, "%s", s);
    } else if (numberToo && lua_isinteger(L, -1)) {
        snprintf(out, size, "%lld", (long long)lua_tointeger(L, -1));
        if (isNumber) *isNumber = 1;
    } else if (!lua_isnil(L, -1)) {
        reason = badType;
    }
    lua_pop(L, 1);
    return reason;
}

static int lAddTransport(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);
    TransportEntry e;
    memset(&e, 0, sizeof e);
    e.monster = -1;
    const char *mod = runningLuaMod();
    snprintf(e.mod, sizeof e.mod, "%s", mod ? mod : "?");
    snprintf(e.file, sizeof e.file, "%s", luaRunning ? luaRunning->path : "?");
    snprintf(e.folder, sizeof e.folder, "%s", luaRunning ? luaRunning->folder : "");
    snprintf(e.mapFolder, sizeof e.mapFolder, "%s", e.folder);
    lua_Debug caller;
    e.line = lua_getstack(L, 1, &caller) && lua_getinfo(L, "l", &caller) ? caller.currentline : 0;
    lua_getfield(L, 1, "monster");
    const char *reason = !luaScriptsLoading ? "not_at_load" : lua_isnil(L, 2) ? "no_monster" : NULL;
    if (!reason && lua_isinteger(L, 2)) {
        lua_Integer m = lua_tointeger(L, 2);
        if (isUsedMonster(m)) e.monster = (int)m;
        else reason = "unknown_monster";
    } else if (!reason && lua_type(L, 2) == LUA_TSTRING) {
        const char *name = lua_tostring(L, 2);
        e.monster = strlen(name) < MONSTER_NAME_SIZE ? monsterByName(name) : -1;
        if (e.monster < 0) reason = "unknown_monster";
    } else if (!reason) {
        reason = "bad_monster";
    }
    if (!reason) snprintf(e.monsterName, sizeof e.monsterName, "%s", (const char *)monsterRecord(e.monster));
    if (!reason) reason = luaTransportText(L, "battlemap", 0, "bad_battlemap", e.battlemap, sizeof e.battlemap, NULL);
    if (!reason) reason = luaTransportText(L, "heart", 1, "bad_heart", e.heart, sizeof e.heart, &e.heartIsNumber);
    if (!reason) reason = luaTransportText(L, "terrain", 1, "bad_terrain", e.terrain, sizeof e.terrain, &e.terrainIsNumber);
    int land = reason ? 0 : luaTransportFlag(L, "land"), air = reason ? 0 : luaTransportFlag(L, "air");
    if (!reason && land < 0) reason = "bad_land";
    if (!reason && air < 0) reason = "bad_air";
    e.media = (land > 0 ? CARRY_LAND : 0) | (air > 0 ? CARRY_AIR : 0);
    int at = -1;
    for (int i = 0; !reason && i < transportEntryCount; i++) {
        TransportEntry *old = &transportEntries[i];
        refindTransportMonster(old);
        if (old->monster != e.monster || old->fromWords) continue;
        if (strcmp(old->mod, e.mod) && old->from != TRANSPORT_FROM_EARLIER_PASS && transportEntryLive(old)) reason = "monster_taken";
        else at = i;
    }
    if (!reason && at < 0) {
        if (transportEntryCount >= TRANSPORT_ENTRIES_MAX) reason = "too_many";
        else if (!growList((void **)&transportEntries, &transportEntryCapacity, transportEntryCount + 1, sizeof *transportEntries)) reason = "out_of_memory";
    }
    TransportEntry own;
    if (!reason) {
        own = e;
        mergeTransportWords(&e);
        reason = transportSiteRefusal(&e);
        if (!reason) reason = transportDeckRefusal(&e, e.mapFolder);
        if (reason && e.words) {
            const char *ownReason = transportSiteRefusal(&own);
            if (!ownReason) ownReason = transportDeckRefusal(&own, own.mapFolder);
            if (!ownReason) writeLogWarning("transport_words_not_merged", "mod=%s monster=%s reason=%s", e.mod, e.monsterName, reason), e = own;
            reason = ownReason;
        }
    }
    if (reason) {
        refuseLuaCall(mod, "coe5.addTransport", reason, "monster=%.60s", luaL_tolstring(L, 2, NULL));
        if (!strcmp(reason, "unknown_monster")) addPlaneProblem(1, e.mod, e.file, e.line, "No monster called '%.60s'", lua_tostring(L, -1));
        if (!strcmp(reason, "unknown_terrain")) addPlaneProblem(1, e.mod, e.file, e.line, "No terrain called '%.60s'", e.terrain);
        if (!strcmp(reason, "file_not_found") || !strcmp(reason, "file_wrong_case")) addPlaneProblem(1, e.mod, e.file, e.line, "No file '%.60s'", e.battlemap);
        lua_pop(L, 1);
        lua_pushboolean(L, 0);
        return 1;
    }
    if (at < 0) at = transportEntryCount++;
    transportEntries[at] = e;
    writeLog("transport_declared", "mod=%s monster=%d name=%s land=%d air=%d battlemap=%s heart=%s terrain=%s line=%d words=%d", e.mod, e.monster,
             e.monsterName, (e.media & CARRY_LAND) != 0, (e.media & CARRY_AIR) != 0, e.battlemap[0] ? e.battlemap : "-", e.heart[0] ? e.heart : "-",
             e.terrain[0] ? e.terrain : "-", e.line, e.words);
    decksOnTransportAdded(&transportEntries[at]);
    if (e.media) carriersOnTransportAdded();
    lua_pushboolean(L, 1);
    return 1;
}
