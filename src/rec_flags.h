#pragma once

enum { RF_HUMAN, RF_NO_GATHERER, RF_PRIEST, RF_TOWER, RF_WORDS };

static const char *const recFlagWords[RF_WORDS] = {"humanrec", "nogathererrec", "priestrec", "towerrec"};

#define REC_FLAG_GEMS (RECRUIT_FLAG_RUBY | RECRUIT_FLAG_SAPPHIRE | RECRUIT_FLAG_DIAMOND | RECRUIT_FLAG_EMERALD)
#define REC_FLAG_RUBY_LOCATIONS    (-81)
#define REC_FLAG_EMERALD_LOCATIONS (-84)
#define REC_FLAG_PRIEST_MIN 2
#define REC_FLAG_PRIEST_MAX 3

static int recFlagWordOf(const char *line, size_t len) {
    int word = 0;
    while (word < RF_WORDS && (strlen(recFlagWords[word]) != len || strncmp(recFlagWords[word], line, len))) word++;
    return word;
}

static int recFlagRestIsBlank(const char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++;
    return !*text;
}

static void setRecFlag(int word, const char *line, size_t len, unsigned char *record, int terrainList, int mod) {
    const char *name = recFlagWords[word], *args = line + len;
    int *flags = (int *)(record + RECRUIT_FLAGS), *terrain = (int *)(record + RECRUIT_TERRAIN);
    if (word == RF_PRIEST) {
        char *end = NULL;
        long level = strtol(args, &end, 10);
        if (end == args || !recFlagRestIsBlank(end) || level < REC_FLAG_PRIEST_MIN || level > REC_FLAG_PRIEST_MAX) {
            refuseC5mLine(name, spellModName(mod), "bad_value", "line=%d text=%.40s", G32(RVA_MOD_LINE), args);
            return;
        }
        *flags = (*flags & ~(RECRUIT_FLAG_PRIEST_2 | RECRUIT_FLAG_PRIEST_3)) | (level == 2 ? RECRUIT_FLAG_PRIEST_2 : RECRUIT_FLAG_PRIEST_3);
        return;
    }
    if (!recFlagRestIsBlank(args)) {
        refuseC5mLine(name, spellModName(mod), "bad_value", "line=%d text=%.40s", G32(RVA_MOD_LINE), args);
        return;
    }
    if (word == RF_HUMAN) *flags |= RECRUIT_FLAG_HUMAN;
    else if (word == RF_NO_GATHERER) *flags |= RECRUIT_FLAG_NO_GATHERER;
    else if (terrainList) refuseC5mLine(name, spellModName(mod), "terrain_list", "line=%d", G32(RVA_MOD_LINE));
    else if (*terrain <= REC_FLAG_RUBY_LOCATIONS && *terrain >= REC_FLAG_EMERALD_LOCATIONS) {
        *flags = (*flags & ~REC_FLAG_GEMS) | (RECRUIT_FLAG_RUBY << (REC_FLAG_RUBY_LOCATIONS - *terrain)) | RECRUIT_FLAG_AND_TOWERS;
        *terrain = -1;
    } else if (*terrain != -1 || !(*flags & REC_FLAG_GEMS) || !(*flags & RECRUIT_FLAG_AND_TOWERS))
        refuseC5mLine(name, spellModName(mod), "no_gem_location", "line=%d terrain=%d", G32(RVA_MOD_LINE), *terrain);
}

static int recFlagLineHook(const char *line, size_t len, int mod, int level) {
    int word = level == 2 ? recFlagWordOf(line, len) : RF_WORDS;
    if (word == RF_WORDS) return 0;
    int cls = G32(RVA_CLASS_CURRENT), offer = G32(RVA_CLASS_LAST_RECRUIT);
    if (cls < 0 || cls >= tableRecords(TABLE_CLASS_RECRUITS)) return 0;
    if (offer < 0 || offer >= CLASS_RECRUITS_MAX) return 1;
    setRecFlag(word, line, len, tableAddress(TABLE_CLASS_RECRUITS) + (size_t)cls * CLASS_RECRUITS_SIZE + (size_t)offer * CLASS_RECRUIT_SIZE, 0, mod);
    return 1;
}
