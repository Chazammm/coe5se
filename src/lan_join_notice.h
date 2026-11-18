#pragma once

#define LAN_SEED_PREFIX       "CoE5SE seed:"
#define LAN_BUILD_PREFIX      "CoE5SE build:"
#define LAN_JOIN_TITLE        "CoE5SE: Mods with scripts"
#define LAN_PLATFORMS         2

// ---- A site on each build ---------------------------------------------------------------------------
typedef enum { SITE_ON_NONE, SITE_ON_PART, SITE_ON_ALL } SiteCoverage;
static const struct { CallSite site; int platform; SiteCoverage coverage; } SITE_BUILD_EXCEPTIONS[] = {
    {SITE_SQUARE_LIBRARY_LEVEL, 1, SITE_ON_ALL},
    {SITE_SQUARE_OPEN_LAND, 1, SITE_ON_PART},
    {SITE_COMMANDER_BATTLE_STRENGTH, 1, SITE_ON_PART},
    {SITE_TOP_BAR_INCOME, 1, SITE_ON_NONE},
};

static SiteCoverage siteCoverageOn(CallSite site, int platform) {
    if ((unsigned)site >= SITE_COUNT) return SITE_ON_NONE;
    for (size_t i = 0; i < sizeof SITE_BUILD_EXCEPTIONS / sizeof SITE_BUILD_EXCEPTIONS[0]; i++)
        if (SITE_BUILD_EXCEPTIONS[i].site == site && SITE_BUILD_EXCEPTIONS[i].platform == platform) return SITE_BUILD_EXCEPTIONS[i].coverage;
    int here = SITES[site].rvas[0] || SITES[site].targetRva, other = SITES[site].onOtherBuild;
    if (!here && !other) return SITE_ON_ALL;
    return (platform == SAVE_PLATFORM ? here : other) ? SITE_ON_ALL : SITE_ON_NONE;
}

// ---- The texts --------------------------------------------------------------------------------------
static signed char lanSlotBuilds[PLAYER_SLOTS_MOVED];
static int lanSlotBuildsReady;
static unsigned lanNoticeEpoch = 1;

static void readyLanSlotBuilds(void) {
    if (lanSlotBuildsReady) return;
    memset(lanSlotBuilds, -1, sizeof lanSlotBuilds);
    lanSlotBuildsReady = 1;
}

static void sendLanScriptSeed(void) {
    char message[NET_LOBBY_MESSAGE_MAX];
    snprintf(message, sizeof message, LAN_SEED_PREFIX "%08x", lanScriptSeedNow());
    sendNetworkCheckText(message);
}

static void sendLanBuild(u64 slots) {
    char message[NET_LOBBY_MESSAGE_MAX];
    snprintf(message, sizeof message, LAN_BUILD_PREFIX "%016llx:%d", (unsigned long long)slots, SAVE_PLATFORM);
    sendNetworkCheckText(message);
}

static int lanScriptSeedFromSave;
static void takeHostScriptSeed(unsigned seed) {
    if (netRoleNow() != NET_ROLE_CLIENT) return;
    if (!lanScriptSeedKnown || lanScriptSeed != seed) {
        lanScriptSeed = seed;
        lanScriptSeedKnown = 1;
        lanNoticeEpoch++;
        writeLog("lan_script_seed_read", "seed=%08x from=save", lanScriptSeed);
    }
    lanScriptSeedFromSave = 1;
}

static void readPersonalModList(const char *body);
static int readLanLobbyMessage(const char *message, int lists) {
    const char *body = networkCheckBody(message, LAN_SEED_PREFIX);
    if (body) {
        char *end = NULL;
        unsigned long seed = strtoul(body, &end, 16);
        if (end != body + 8 || *end) {
            writeLogWarning("lan_script_seed_dropped", "reason=bad_text");
            return 1;
        }
        if (netRoleNow() != NET_ROLE_CLIENT || (lanScriptSeedKnown && lanScriptSeed == (unsigned)seed)) return 1;
        if (lanScriptSeedKnown) {
            static int dropsLogged;
            if (dropsLogged < 8) {
                dropsLogged++;
                writeLogWarning("lan_script_seed_dropped", "reason=%s seed=%08lx held=%08x", lanScriptSeedFromSave ? "not_the_saves" : "not_the_first", seed, lanScriptSeed);
            }
            return 1;
        }
        lanScriptSeed = (unsigned)seed;
        lanScriptSeedKnown = 1;
        lanNoticeEpoch++;
        writeLog("lan_script_seed_read", "seed=%08x from=lobby", lanScriptSeed);
        return 1;
    }
    body = networkCheckBody(message, LAN_BUILD_PREFIX);
    if (body) {
        char *end = NULL;
        u64 slots = strtoull(body, &end, 16);
        int platform = end == body + 16 && *end == ':' ? atoi(end + 1) : -1;
        if (platform < 0 || platform >= LAN_PLATFORMS) {
            writeLogWarning("lan_build_dropped", "reason=bad_text");
            return 1;
        }
        readyLanSlotBuilds();
        for (int i = 0; i < PLAYER_SLOTS_MOVED && i < 64; i++) {
            if (!(slots >> i & 1) || lanSlotBuilds[i] == platform) continue;
            lanSlotBuilds[i] = (signed char)platform;
            lanNoticeEpoch++;
            writeLog("lan_build_read", "slot=%d build=%s", i, savePlatformName(platform));
        }
        return 1;
    }
    body = lists ? networkCheckBody(message, PERSONAL_LIST_PREFIX) : NULL;
    if (body) {
        readPersonalModList(body);
        lanNoticeEpoch++;
        return 1;
    }
    return 0;
}

// ---- The box ----------------------------------------------------------------------------------------
static void lanJoinNoticeText(char *text, size_t size, const char *contentMods, const char *const *personal, int players, const char *missing) {
    size_t n = 0;
    text[0] = '\0';
    if (!contentMods[0] && !players) return;
    appendText(text, size, &n, "All players: %s", contentMods[0] ? contentMods : "none");
    for (int i = 0; i < players; i++) appendText(text, size, &n, "\n%s", personal[i]);
    if (missing[0]) appendText(text, size, &n, "\n\n%s", missing);
}

static void appendLanNoticeName(char *out, size_t size, size_t *n, const char *mod) {
    char name[MOD_NAME_MAX];
    luaNoticeName(mod, name, sizeof name);
    appendText(out, size, n, "%s%.60s", *n ? ", " : "", name);
}

static int appendMissingSitesOf(const LuaScript *s, int platform, char *out, size_t size, size_t *n) {
    int count = 0;
    for (int i = 0; i < LUA_SITE_COUNT; i++) {
        const LuaSiteState *st = &luaSiteState[i];
        int uses = 0;
        for (int j = 0; j < st->count && !uses; j++) uses = st->wraps[j]->s == s;
        for (int j = 0; j < st->observerCount && !uses; j++) uses = st->observers[j].s == s;
        SiteCoverage coverage = uses ? siteCoverageOn(luaSites[i].site, platform) : SITE_ON_ALL;
        if (coverage == SITE_ON_ALL) continue;
        if (!count) {
            char name[MOD_NAME_MAX];
            luaNoticeName(s->mod, name, sizeof name);
            appendText(out, size, n, "%s%.60s (", *n ? "; " : "", name);
        }
        appendText(out, size, n, "%s%s%s", count ? ", " : "", luaSites[i].name, coverage == SITE_ON_PART ? " in part" : "");
        count++;
    }
    if (count) appendText(out, size, n, ")");
    return count;
}

static void buildLanJoinNotice(char *text, size_t size) {
    static char content[1200], missing[1600], lines[PLAYER_SLOTS_MOVED][PERSONAL_LIST_SIZE + 48];
    const char *personal[PLAYER_SLOTS_MOVED];
    size_t n = 0;
    content[0] = '\0';
    for (int i = 0; i < luaScriptCount; i++) {
        const LuaScript *s = luaScripts[i];
        if (!s->retired && !s->dead && modClassNow(s->mod) == LUA_CLASS_CONTENT) appendLanNoticeName(content, sizeof content, &n, s->mod);
    }
    int players = 0, builds[LAN_PLATFORMS] = {0};
    builds[SAVE_PLATFORM] = 1;
    if (loadedSaveWrittenOn >= 0 && loadedSaveWrittenOn < LAN_PLATFORMS) builds[loadedSaveWrittenOn] = 1;
    readyLanSlotBuilds();
    buildPersonalModList(ownPersonalModList, sizeof ownPersonalModList);
    for (int i = 0; i < playerSlotCount() && i < PLAYER_SLOTS_MOVED; i++) {
        short controller = *(short *)playerSlotAddress(RVA_SLOT_CONTROLLER, i, 2);
        if (controller != CONTROLLER_ME && controller != CONTROLLER_ANOTHER_PLAYER) continue;
        if (lanSlotBuilds[i] >= 0) builds[(int)lanSlotBuilds[i]] = 1;
        const char *list = controller == CONTROLLER_ME ? ownPersonalModList : personalModLists[i];
        if (!list[0]) continue;
        char name[33];
        shownSlotName(i, name, sizeof name);
        snprintf(lines[players], sizeof lines[players], "%s: %.*s", name, PERSONAL_LIST_SIZE - 1, list);
        personal[players] = lines[players];
        players++;
    }
    n = 0;
    missing[0] = '\0';
    for (int platform = 0; platform < LAN_PLATFORMS; platform++) {
        if (!builds[platform]) continue;
        char sites[1200] = "";
        size_t at = 0;
        for (int i = 0; i < luaScriptCount; i++) {
            const LuaScript *s = luaScripts[i];
            if (!s->retired && !s->dead && modClassNow(s->mod) == LUA_CLASS_CONTENT) appendMissingSitesOf(s, platform, sites, sizeof sites, &at);
        }
        if (sites[0]) appendText(missing, sizeof missing, &n, "%sNot on %s: %s", n ? "\n" : "", platform ? "Mac" : "Windows", sites);
    }
    lanJoinNoticeText(text, size, content, personal, players, missing);
}

static unsigned long long lanJoinShownHash;
static unsigned lanJoinBuiltEpoch, lanJoinBuiltLoads;
static void showLanJoinNotice(void) {
    if (netRoleNow() != NET_ROLE_CLIENT || !netClientInTurn || luaCodeRunning) return;
    if (lanJoinBuiltEpoch == lanNoticeEpoch && lanJoinBuiltLoads == netClientLoads) return;
    lanJoinBuiltEpoch = lanNoticeEpoch, lanJoinBuiltLoads = netClientLoads;
    static char text[POPUP_TEXT_SIZE];
    buildLanJoinNotice(text, sizeof text);
    unsigned long long hash = 1469598103934665603ull;
    for (const char *c = text; *c; c++) hash = (hash ^ (unsigned char)*c) * 1099511628211ull;
    if (!text[0] || hash == lanJoinShownHash) return;
    lanJoinShownHash = hash;
    writeLog("lan_join_notice_shown", "bytes=%zu", strlen(text));
    okBox(LAN_JOIN_TITLE, text);
}

static void showLanNotices(void) {
    showLanJoinNotice();
    showReplayDiffersNotice();
}
