#pragma once

#define FACTION_REF_PLACEHOLDER 2000000
#define FACTION_LINE_MAX        PLANE_LINE_MAX

enum { FACTION_REF_RITUAL, FACTION_REF_MONSTER };

static const struct { const char *word; int kind, id; } FACTION_PLAYER_WORDS[] = {
    {"setplayer", FACTION_REF_RITUAL, RITUAL_FX_SETPLAYER},   {"failplayer", FACTION_REF_RITUAL, RITUAL_FX_FAILPLAYER},
    {"promplayer", FACTION_REF_RITUAL, RITUAL_FX_PROMPLAYER}, {"wanderattack", FACTION_REF_MONSTER, ABILITY_WANDERATTACK},
    {"revertowner", FACTION_REF_MONSTER, ABILITY_REVERTOWNER},
};

static const struct { const char *word; int texts; } FACTION_EVENT_CONDITIONS[] = {
    {"squareowner", 0}, {"armyowner", 0}, {"player", 0},     {"class", 1},     {"humanplayer", 0},    {"aiplayer", 0},
    {"hascom", 1},      {"hascomhere", 1}, {"ownsloc", 0},   {"ownsloctarg", 0}, {"hasunit", 1},      {"hasunithere", 1},
    {"hasitem", 1},     {"hasitemhere", 1}, {"hasaffunithere", 0}, {"minunits", 1}, {"minunitshere", 1},
};
static const struct { const char *word; int texts; } FACTION_EVENT_COMMANDS[] = {
    {"playercolor", 0}, {"addresources", 0}, {"message", 1}, {"newunits", 1}, {"promoteunits", 2}, {"reveal", 0}, {"changeowner", 0},
};

typedef struct {
    int kind, record, id;
    int slot;
    int line;
    char name[FACTION_NAME_SIZE], mod[MOD_NAME_MAX], word[16];
} FactionNameRef;

static FactionNameRef *factionRefs;
static int factionRefCount, factionRefCapacity;
static char factionNameLine[FACTION_LINE_MAX];
static char factionEventLine[FACTION_LINE_MAX];

static int playerNamed(const char *name) {
    const Faction *f = findFaction(name);
    if (f && f->fromPass != FACTION_EARLIER_PASS) return factionSlotOf(f);
    return factionPresetNamed(name);
}

static int putPlayerNumber(const char *line, const char *at, const char *rest, long long number, char *out, const char *word, const char *mod) {
    int n = snprintf(out, FACTION_LINE_MAX, "%.*s%lld%s", (int)(at - line), line, number, rest);
    if (n >= 0 && n < FACTION_LINE_MAX) return 1;
    refuseC5mLine(word, mod, "line_too_long", "line=%d", G32(RVA_MOD_LINE));
    return 0;
}

static const char *factionNumberWordLine(const char *line, size_t len, const char *args, int mod, int kind, int id) {
    char word[16], name[FACTION_NAME_SIZE + 2];
    snprintf(word, sizeof word, "%.*s", (int)(len < sizeof word ? len : sizeof word - 1), line);
    const char *rest = args;
    int nameLen = readSpellQuoted(&rest, name, sizeof name);
    int player = nameLen > 0 && nameLen < FACTION_NAME_SIZE ? playerNamed(name) : -1;
    if (player < 0) {
        refuseC5mLine(word, spellModName(mod), nameLen <= 0 ? "no_name" : "unknown_faction", "line=%d faction=%.40s", G32(RVA_MOD_LINE), name);
        return NULL;
    }
    int record = G32(kind == FACTION_REF_RITUAL ? RVA_RITUAL_CURRENT : RVA_MONSTER_CURRENT);
    long long number = player;
    if (player >= PLAYER_SLOTS_GAME && record >= 0 && growList((void **)&factionRefs, &factionRefCapacity, factionRefCount + 1, sizeof factionRefs[0])) {
        FactionNameRef *r = &factionRefs[factionRefCount];
        memset(r, 0, sizeof *r);
        r->kind = kind, r->record = record, r->id = id, r->slot = -1, r->line = G32(RVA_MOD_LINE);
        snprintf(r->name, sizeof r->name, "%.*s", FACTION_NAME_SIZE - 1, name);
        snprintf(r->mod, sizeof r->mod, "%s", spellModName(mod));
        snprintf(r->word, sizeof r->word, "%s", word);
        number = FACTION_REF_PLACEHOLDER + factionRefCount++;
    }
    return putPlayerNumber(line, args, rest, number, factionNameLine, word, spellModName(mod)) ? factionNameLine : NULL;
}

static const char *factionNameLineHook(const char *line, size_t len, int mod, int level) {
    if (level != 2) return line;
    const char *args = line + len;
    while (*args == ' ' || *args == '\t') args++;
    if (*args != '"') return line;
    for (size_t i = 0; i < sizeof FACTION_PLAYER_WORDS / sizeof FACTION_PLAYER_WORDS[0]; i++)
        if (spellWordIs(line, len, FACTION_PLAYER_WORDS[i].word))
            return factionNumberWordLine(line, len, args, mod, FACTION_PLAYER_WORDS[i].kind, FACTION_PLAYER_WORDS[i].id);
    return line;
}

static unsigned char *factionRefValueAt(const FactionNameRef *r, int slot, int *id) {
    if (r->kind == FACTION_REF_RITUAL) {
        if (r->record >= ritualSlotCount() || slot >= RITUAL_FX_MAX) return NULL;
        unsigned char *fx = ritualEffectAt(r->record, slot);
        memcpy(id, fx, 4);
        return fx + RITUAL_FX_VALUE;
    }
    unsigned char *record = slot < MONSTER_ABILITY_PAIRS ? monsterRecord(r->record) : NULL;
    if (!record) return NULL;
    int *pair = (int *)(record + MONSTER_ABILITIES) + 2 * slot;
    *id = pair[0];
    return (unsigned char *)(pair + 1);
}

static int copyFactionRefs(int kind, int source, int target) {
    int count = factionRefCount, ok = 1;
    for (int i = 0; i < count; i++) {
        if (factionRefs[i].kind != kind || factionRefs[i].record != source) continue;
        FactionNameRef copy = factionRefs[i];
        copy.record = target, copy.slot = -1;
        for (int s = 0;; s++) {
            int id;
            unsigned char *value = factionRefValueAt(&copy, s, &id);
            if (!value || id <= 0) break;
            long long held = 0, number = -1;
            if (kind == FACTION_REF_RITUAL) memcpy(&held, value, 8);
            else held = *(int *)value;
            if (id != copy.id || held != FACTION_REF_PLACEHOLDER + i) continue;
            if (growList((void **)&factionRefs, &factionRefCapacity, factionRefCount + 1, sizeof factionRefs[0])) {
                factionRefs[factionRefCount] = copy;
                number = FACTION_REF_PLACEHOLDER + factionRefCount++;
            } else ok = 0;
            if (kind == FACTION_REF_RITUAL) memcpy(value, &number, 8);
            else *(int *)value = (int)number;
            break;
        }
    }
    return ok;
}

static int copyMonsterFactionRefs(int source, int target) {
    return copyFactionRefs(FACTION_REF_MONSTER, source, target);
}

static void resolveFactionNames(int level) {
    if (level != 2) return;
    int resolved = 0;
    for (int i = 0; i < factionRefCount; i++) {
        FactionNameRef *r = &factionRefs[i];
        long long placeholder = FACTION_REF_PLACEHOLDER + i;
        for (int s = 0;; s++) {
            int id;
            unsigned char *value = factionRefValueAt(r, s, &id);
            if (!value || id <= 0) break;
            long long held = 0;
            if (r->kind == FACTION_REF_RITUAL) memcpy(&held, value, 8);
            else held = *(int *)value;
            if (id == r->id && held == placeholder) r->slot = s;
        }
        if (r->slot < 0) {
            r->slot = -2;
            continue;
        }
        int player = factionSlotNamed(r->name);
        if (player < 0) refuseC5mLine(r->word, r->mod, "faction_dropped", "line=%d faction=%s", r->line, r->name);
        else resolved++;
        int id;
        unsigned char *value = factionRefValueAt(r, r->slot, &id);
        if (r->kind == FACTION_REF_RITUAL) {
            long long wide = player;
            memcpy(value, &wide, 8);
        } else *(int *)value = player;
    }
    if (factionRefCount) writeLog("faction_names_resolved", "names=%d resolved=%d", factionRefCount, resolved);
    factionRefCount = 0;
}

static void factionNamesBeforePass(int level) {
    if (level == 2) factionRefCount = 0;
}

static int factionEventWordKind(const char *line, size_t len, int *texts) {
    if (line[0] == '+' || line[0] == '-') {
        for (size_t i = 0; i < sizeof FACTION_EVENT_CONDITIONS / sizeof FACTION_EVENT_CONDITIONS[0]; i++)
            if (spellWordIs(line + 1, len - 1, FACTION_EVENT_CONDITIONS[i].word)) return *texts = FACTION_EVENT_CONDITIONS[i].texts, 1;
        return 0;
    }
    for (size_t i = 0; i < sizeof FACTION_EVENT_COMMANDS / sizeof FACTION_EVENT_COMMANDS[0]; i++)
        if (spellWordIs(line, len, FACTION_EVENT_COMMANDS[i].word)) return *texts = FACTION_EVENT_COMMANDS[i].texts, 2;
    return 0;
}

static const char *factionEventNameAt(const char *line, size_t len, int *kind) {
    int texts = 0;
    *kind = factionEventWordKind(line, len, &texts);
    if (!*kind) return NULL;
    const char *at = line + len;
    while (*at == ' ' || *at == '\t') at++;
    return *at == '"' && gameQuotedTexts(at) > texts ? at : NULL;
}

static const char *factionNameEventLine(const char *line, int level, const char *mod) {
    size_t len = strcspn(line, " \t");
    int kind;
    const char *at = factionEventNameAt(line, len, &kind);
    if (!at || level != 3 || G32(RVA_EVENT_STATE) < 1) return line;
    char word[20], name[FACTION_NAME_SIZE + 2];
    snprintf(word, sizeof word, "%.*s", (int)(len < sizeof word ? len : sizeof word - 1), line);
    const char *rest = at;
    int nameLen = readSpellQuoted(&rest, name, sizeof name);
    int player = nameLen > 0 && nameLen < FACTION_NAME_SIZE ? playerNamed(name) : -1;
    if (player < 0) {
        refuseC5mLine(word, mod[0] ? mod : "-", nameLen <= 0 ? "no_name" : "unknown_faction", "faction=%.40s line=%d", name, G32(RVA_MOD_LINE));
        if (kind == 1 && line[0] == '+') G32(RVA_EVENT_STATE) = EVENT_FAILED;
        return NULL;
    }
    return putPlayerNumber(line, at, rest, player, factionEventLine, word, mod[0] ? mod : "-") ? factionEventLine : NULL;
}

static int declinedFactionNameLine(const char *line, size_t len) {
    const char *args = line + len;
    while (*args == ' ' || *args == '\t') args++;
    if (*args != '"') return 0;
    for (size_t i = 0; i < sizeof FACTION_PLAYER_WORDS / sizeof FACTION_PLAYER_WORDS[0]; i++)
        if (spellWordIs(line, len, FACTION_PLAYER_WORDS[i].word)) return 1;
    int kind;
    return factionEventNameAt(line, len, &kind) != NULL;
}

static int playerOfFactionName(const char *name) {
    const Faction *f = findFaction(name);
    return f ? factionSlotOf(f) : factionPresetNamed(name);
}

static int lFactionSlot(lua_State *L) {
    int slot = playerOfFactionName(luaL_checkstring(L, 1));
    if (slot < 0) lua_pushnil(L);
    else lua_pushinteger(L, slot);
    return 1;
}
