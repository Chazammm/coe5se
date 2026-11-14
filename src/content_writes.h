#pragma once

enum {
    CONTENT_MONSTER = RECORD_MONSTER,
    CONTENT_WEAPON = RECORD_WEAPON,
    CONTENT_ITEM = RECORD_ITEM,
    CONTENT_CLASS = RECORD_CLASS,
    CONTENT_TERRAIN = RECORD_TERRAIN,
    CONTENT_RITUAL = RECORD_RITUAL,
    CONTENT_CLASS_RECRUITS,
    CONTENT_CASTER_MAX_CAST,
    CONTENT_NOEXPLODE,
    CONTENT_WEAPON_TRAITS,
    CONTENT_KIND_COUNT
};

typedef struct {
    unsigned char kind;
    int index, offset, width;
    unsigned char *bytes, *old;
} ContentWrite;

static struct {
    ContentWrite *writes;
    int count, capacity, passCount;
} contentWrites;

static const char *setWeaponRuleTraits(int weapon, int rule, const char *traits, char *bad, size_t badSize);

static unsigned char *contentRecord(int kind, int index) {
    if (index < 0) return NULL;
    switch (kind) {
    case CONTENT_MONSTER: return monsterRecord(index);
    case CONTENT_WEAPON: return index < tableRecords(TABLE_WEAPON) ? tableAddress(TABLE_WEAPON) + (size_t)index * WEAPON_SIZE : NULL;
    case CONTENT_ITEM: return index < tableRecords(TABLE_ITEM) ? tableAddress(TABLE_ITEM) + (size_t)index * ITEM_SIZE : NULL;
    case CONTENT_CLASS: return index < tableRecords(TABLE_CLASS) ? tableAddress(TABLE_CLASS) + (size_t)index * CLASS_SIZE : NULL;
    case CONTENT_CLASS_RECRUITS:
        return index < tableRecords(TABLE_CLASS_RECRUITS) ? tableAddress(TABLE_CLASS_RECRUITS) + (size_t)index * CLASS_RECRUITS_SIZE : NULL;
    case CONTENT_TERRAIN: return index < TERRAIN_TABLE_MAX ? GAME_PTR(RVA_TERRAIN_TABLE) + (size_t)index * TERRAIN_SIZE : NULL;
    case CONTENT_RITUAL: return index < ritualSlotCount() ? gameBase + ritualTableRva() + (size_t)index * RITUAL_SIZE : NULL;
    default: return NULL;
    }
}

static int contentRecordSize(int kind) {
    switch (kind) {
    case CONTENT_MONSTER: return MONSTER_SIZE;
    case CONTENT_WEAPON: return WEAPON_SIZE;
    case CONTENT_ITEM: return ITEM_SIZE;
    case CONTENT_CLASS: return CLASS_SIZE;
    case CONTENT_CLASS_RECRUITS: return CLASS_RECRUITS_SIZE;
    case CONTENT_TERRAIN: return TERRAIN_SIZE;
    case CONTENT_RITUAL: return RITUAL_SIZE;
    default: return 0;
    }
}

static int weaponRuleTraitsText(int weapon, int rule, char *out, int size) {
    int n = 0;
    for (int i = 0; i < weaponTraits.count; i++) {
        const WeaponTraitRule *r = &weaponTraits.rules[i];
        if (r->weapon != weapon || r->rule != rule) continue;
        int len = (int)strlen(r->word);
        if (out && n + (n > 0) + len < size) {
            if (n) out[n] = ' ';
            memcpy(out + n + (n > 0), r->word, (size_t)len);
        }
        n += (n > 0) + len;
    }
    if (out && size > 0) out[n < size ? n : size - 1] = 0;
    return n;
}

static const char *applyContentBytes(int kind, int index, int offset, const unsigned char *bytes, int width) {
    if (kind == CONTENT_CASTER_MAX_CAST) {
        if (index < 0 || index >= RITUAL_SLOT_CAP || offset || width != (int)sizeof(short)) return "bad_record";
        if (!ritualCasterMaxCasts && !(ritualCasterMaxCasts = calloc(RITUAL_SLOT_CAP, sizeof ritualCasterMaxCasts[0]))) return "out_of_memory";
        memcpy(&ritualCasterMaxCasts[index], bytes, sizeof(short));
        return NULL;
    }
    if (kind == CONTENT_NOEXPLODE) {
        if (index < 0 || index >= tableRecords(TABLE_WEAPON) || offset || width != 1) return "bad_record";
        if (bytes[0]) return addNoExplodeWeapon(index) ? NULL : "out_of_memory";
        if (index < noExplode.weaponCount) noExplode.weapons[index] = 0;
        return NULL;
    }
    if (kind == CONTENT_WEAPON_TRAITS) {
        if (index < 0 || index >= tableRecords(TABLE_WEAPON) || offset < 0 || offset > 2 || width < 1 || bytes[width - 1]) return "bad_record";
        char bad[48];
        return setWeaponRuleTraits(index, offset, (const char *)bytes, bad, sizeof bad);
    }
    unsigned char *record = contentRecord(kind, index);
    if (!record || offset < 0 || width < 1 || offset + width > contentRecordSize(kind)) return "bad_record";
    memcpy(record + offset, bytes, (size_t)width);
    if (kind == CONTENT_CLASS) memcpy(tableAddress(TABLE_CLASS_COPY) + (size_t)index * CLASS_SIZE + offset, bytes, (size_t)width);
    return NULL;
}

static void readContentBytes(int kind, int index, int offset, unsigned char *out, int width) {
    memset(out, 0, (size_t)width);
    if (kind == CONTENT_CASTER_MAX_CAST) {
        short n = (short)ritualCasterMaxCast(index);
        memcpy(out, &n, sizeof n < (size_t)width ? sizeof n : (size_t)width);
    } else if (kind == CONTENT_NOEXPLODE) {
        out[0] = index >= 0 && index < noExplode.weaponCount && noExplode.weapons[index];
    } else if (kind == CONTENT_WEAPON_TRAITS) {
        weaponRuleTraitsText(index, offset, (char *)out, width);
    } else {
        const unsigned char *record = contentRecord(kind, index);
        if (record && offset >= 0 && offset + width <= contentRecordSize(kind)) memcpy(out, record + offset, (size_t)width);
    }
}

static const char *writeContent(int kind, int index, int offset, const void *bytes, int width) {
    int oldWidth = kind == CONTENT_WEAPON_TRAITS ? weaponRuleTraitsText(index, offset, NULL, 0) + 1 : width;
    int size = oldWidth > width ? oldWidth : width;
    if (kind < 0 || kind >= CONTENT_KIND_COUNT || width < 1) return "bad_record";
    if (contentWrites.count == contentWrites.capacity) {
        int capacity = contentWrites.capacity ? contentWrites.capacity * 2 : 64;
        ContentWrite *grown = realloc(contentWrites.writes, (size_t)capacity * sizeof *grown);
        if (!grown) return "out_of_memory";
        contentWrites.writes = grown;
        contentWrites.capacity = capacity;
    }
    unsigned char *both = calloc(2, (size_t)size);
    if (!both) return "out_of_memory";
    memcpy(both, bytes, (size_t)width);
    readContentBytes(kind, index, offset, both + size, size);
    const char *reason = applyContentBytes(kind, index, offset, both, kind == CONTENT_WEAPON_TRAITS ? size : width);
    if (reason) {
        free(both);
        return reason;
    }
    contentWrites.writes[contentWrites.count++] = (ContentWrite){(unsigned char)kind, index, offset, size, both, both + size};
    return NULL;
}

// ---- Which mod made a monster ------------------------------------------------------------------
static short monsterMadeBy[MONSTER_RECORDS_MOVED];

static void noteMonsterMade(int mod) {
    int monster = G32(RVA_MONSTER_CURRENT);
    if (monster >= 0 && monster < MONSTER_RECORDS_MOVED) monsterMadeBy[monster] = (short)(mod + 1);
}

static const char *monsterMadeByName(int monster) {
    int maker = monster >= 0 && monster < MONSTER_RECORDS_MOVED ? monsterMadeBy[monster] - 1 : -1;
    return maker < 0 || maker > G32(RVA_MOD_LAST_INDEX) ? NULL : modName(maker);
}

static void contentWritesBeforePass(int level) {
    if (level != 2) return;
    for (int i = 0; i < contentWrites.count; i++) free(contentWrites.writes[i].bytes);
    contentWrites.count = contentWrites.passCount = 0;
    memset(monsterMadeBy, 0, sizeof monsterMadeBy);
}
