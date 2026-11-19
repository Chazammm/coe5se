#pragma once

#define GAME_EVENT_NEEDS_MAX   4
#define GAME_EVENT_PATCHES_MAX 12
#define GAME_EVENT_QUEUE_MAX   16
#define GAME_EVENT_OWNER       "game_events"
#define GAME_EVENT_STUB_BYTES  0x20
#define SEALS_LAST             7

typedef enum { GAME_EVENT_STUB_NONE, GAME_EVENT_STUB_GOLD_MINE_ONCE, GAME_EVENT_STUB_POPULATE_ONCE, GAME_EVENT_STUB_COUNT } GameEventStub;

typedef struct {
    unsigned rva;
    CodeToggleKind kind;
    unsigned target;
    GameEventStub stub;
} GameEventPatch;

#include "game_event_patches.h"

typedef enum { GAME_EXTRA_NONE, GAME_EXTRA_EVIL, GAME_EXTRA_SEAL, GAME_EXTRA_SEAL_BREAK, GAME_EXTRA_ONCE } GameEventExtra;

typedef struct {
    const char *name, *label;
    GameEventExtra extra;
    int seal;
    const GameEventPatch *patch;
    int patches;
    const char *needs[GAME_EVENT_NEEDS_MAX + 1];
} GameEvent;

#define PATCHES(e) PATCHES_##e, (int)(sizeof PATCHES_##e / sizeof PATCHES_##e[0])
#define NO_PATCHES NULL, 0
#define NEEDS(...) {__VA_ARGS__, NULL}
#define NO_NEEDS {NULL}

static const GameEvent GAME_EVENTS[] = {
    {"longsummer", "Summer hotter and longer than usual", GAME_EXTRA_NONE, 0, PATCHES(LONGSUMMER), NEEDS("A Stonehenge, turned on")},
    {"longautumn", "Autumn longer than usual", GAME_EXTRA_NONE, 0, PATCHES(LONGAUTUMN), NEEDS("Ship Stones, turned on")},
    {"longwinter", "Winter harsher and longer than usual", GAME_EXTRA_NONE, 0, PATCHES(LONGWINTER), NEEDS("A Temple Pyramid, turned on")},
    {"hadesshift", "Hades shifted close to the realm of the living", GAME_EXTRA_NONE, 0, PATCHES(HADESSHIFT), NO_NEEDS},
    {"monument", "Monument of Hades started", GAME_EXTRA_NONE, 0, PATCHES(MONUMENT),
     NEEDS("An Old Castle Ruin in Elysium", "Owned by the independents", "A commander there")},
    {"foretelling", "A foretelling greater than all previous foretellings", GAME_EXTRA_NONE, 0, PATCHES(FORETELLING),
     NEEDS("An Academy of Higher Magic in Elysium", "Owned by the independents", "A High Astrologer of theirs there")},
    {"taintedmagic", "Magic has become tainted", GAME_EXTRA_NONE, 0, PATCHES(TAINTEDMAGIC), NO_NEEDS},
    {"elementalanomaly", "Elemental Anomaly", GAME_EXTRA_NONE, 0, PATCHES(ELEMENTALANOMALY), NEEDS("No Nexus turned on")},
    {"heatwave", "A heat wave and forest fires", GAME_EXTRA_NONE, 0, PATCHES(HEATWAVE), NO_NEEDS},
    {"bumperharvest", "Bumper Harvest", GAME_EXTRA_NONE, 0, PATCHES(BUMPERHARVEST), NO_NEEDS},
    {"fungusyear", "Fungus Year", GAME_EXTRA_NONE, 0, PATCHES(FUNGUSYEAR), NO_NEEDS},
    {"meteor", "A meteor crashed", GAME_EXTRA_NONE, 0, PATCHES(METEOR), NO_NEEDS},
    {"insanefisherman", "Insane Fisherman", GAME_EXTRA_NONE, 0, PATCHES(INSANEFISHERMAN), NEEDS("A Crater in Elysium", "No army there")},
    {"fishermancube", "The most perfect cube floating over a crater", GAME_EXTRA_NONE, 0, PATCHES(FISHERMANCUBE),
     NEEDS("An Insane Fisherman in Elysium", "On a Crater")},
    {"drought", "A drought has struck Elysium", GAME_EXTRA_NONE, 0, PATCHES(DROUGHT), NO_NEEDS},
    {"strangemist", "A strange mist has appeared", GAME_EXTRA_NONE, 0, PATCHES(STRANGEMIST), NO_NEEDS},
    {"eviltimes", "Evil Times", GAME_EXTRA_NONE, 0, PATCHES(EVILTIMES), NO_NEEDS},
    {"banditking", "A Brigand proclaimed King of all Elysium", GAME_EXTRA_EVIL, 0, PATCHES(BANDITKING),
     NEEDS("An Old Castle Ruin in Elysium", "Owned by the independents or no one", "No army there but Brigands or Bandits")},
    {"brigandlair", "A new brigand lair", GAME_EXTRA_EVIL, 0, PATCHES(BRIGANDLAIR), NEEDS("A Forest in Elysium", "No army there")},
    {"piratehaven", "A haven for pirates", GAME_EXTRA_EVIL, 0, PATCHES(PIRATEHAVEN),
     NEEDS("A Port in Elysium", "Owned by the independents or no one", "No army there but the independents'")},
    {"infernogate", "An old gateway to Inferno opened", GAME_EXTRA_NONE, 0, PATCHES(INFERNOGATE),
     NEEDS("A Cultist of the independents or the cities", "In Elysium")},
    {"primalgate", "A gateway to the Primal Plane opened", GAME_EXTRA_NONE, 0, PATCHES(PRIMALGATE),
     NEEDS("A Green Enchantress of the independents or the cities", "In Elysium", "On an Ancient Forest")},
    {"cardinal", "Cardinal Intervenes", GAME_EXTRA_SEAL_BREAK, 0, PATCHES(CARDINAL), NEEDS("Fewer than 7 seals broken", "A Cardinal or Pontiff of the cities")},
    {"seal6", "The sixth of the seven seals broken", GAME_EXTRA_SEAL_BREAK, 6, PATCHES(SEAL6),
     NEEDS("Fewer than 7 seals broken", "A gateway from Elysium or Agartha to the Inferno")},
    {"seal7", "The last of the seven seals broken", GAME_EXTRA_SEAL_BREAK, 7, PATCHES(SEAL7),
     NEEDS("Fewer than 7 seals broken", "A gateway from Elysium or Agartha to the Inferno")},
    {"celestialgate", "A gate to the Celestial Plane appeared", GAME_EXTRA_NONE, 0, PATCHES(CELESTIALGATE), NO_NEEDS},
    {"rider_plague", "Plague has come to Elysium", GAME_EXTRA_SEAL, 3, NO_PATCHES, NEEDS("Its rider alive")},
    {"rider_war", "War has come to Elysium", GAME_EXTRA_SEAL, 4, NO_PATCHES, NEEDS("Its rider alive")},
    {"rider_famine", "Famine has come to Elysium", GAME_EXTRA_SEAL, 5, NO_PATCHES, NEEDS("Its rider alive")},
    {"rider_death", "Death has come to Elysium", GAME_EXTRA_SEAL, 6, NO_PATCHES, NEEDS("Its rider alive")},
    {"apocalypse", "The apocalypse is here", GAME_EXTRA_SEAL, 7, NO_PATCHES, NO_NEEDS},
    {"bloodsun", "Blood Sun", GAME_EXTRA_NONE, 0, PATCHES(BLOODSUN), NEEDS("A God in Elysium, the Sky or Agartha")},
    {"rephaites", "Rephaites", GAME_EXTRA_NONE, 0, PATCHES(REPHAITES), NEEDS("A God in Elysium, the Sky or Agartha")},
    {"fairycircle", "Fairy Circle", GAME_EXTRA_NONE, 0, PATCHES(FAIRYCIRCLE), NEEDS("An army in a Fairy Circle")},
    {"goldmine", "Gold mine attacked", GAME_EXTRA_ONCE, 0, PATCHES(GOLDMINE), NEEDS("A Gold Mine in Elysium")},
    {"youngdvala", "Young Dvala", GAME_EXTRA_ONCE, 0, PATCHES(YOUNGDVALA),
     NEEDS("A greater mine of the independents", "Two Dwarves and a Dwarf Commander there", "No Dvala, Beholder or Daughter of Dvalin there")},
    {"merchant", "A wealthy merchant comes ashore", GAME_EXTRA_NONE, 0, PATCHES(MERCHANT), NEEDS("A port in Elysium")},
    {"palacemoved", "Palace Moved", GAME_EXTRA_NONE, 0, PATCHES(PALACEMOVED), NEEDS("A Palace of Dreams, turned on", "A Mountains square")},
};
#undef PATCHES
#undef NO_PATCHES
#undef NEEDS
#undef NO_NEEDS
#define GAME_EVENT_COUNT ((int)(sizeof GAME_EVENTS / sizeof GAME_EVENTS[0]))
_Static_assert(GAME_EVENT_COUNT == 38, "notes/world/windows.md's world events but newseason, foretelling_done, normalsun, voidcube, miasma, sealedchamber");

// ---- The patches --------------------------------------------------------------------------------
static CodeToggle gameEventToggles[GAME_EVENT_COUNT][GAME_EVENT_PATCHES_MAX];
static CodeToggle *gameEventBorrowed[GAME_EVENT_COUNT][GAME_EVENT_PATCHES_MAX];
static int gameEventGateWasOn[GAME_EVENT_COUNT][GAME_EVENT_PATCHES_MAX];
static int gameEventsPrepared;

static CodeToggle *randomEventGateAt(unsigned rva) {
    for (int e = 0; e < RANDOM_EVENT_COUNT; e++)
        for (int s = 0; s < RANDOM_EVENTS[e].sites; s++)
            if (randomEventGates[e][s].available && randomEventGates[e][s].rva == rva) return &randomEventGates[e][s];
    return NULL;
}

static int gameEventsClaimed(unsigned rva, unsigned size) {
    for (int i = 0; i < gameCodeClaimCount; i++)
        if (gameCodeClaims[i].rva == rva && gameCodeClaims[i].size == size && !strcmp(gameCodeClaims[i].owner, GAME_EVENT_OWNER)) return 1;
    return 0;
}

static void takeGameEventPatchesOut(int e) {
    for (int p = GAME_EVENTS[e].patches - 1; p >= 0; p--) {
        switchCodeToggle(&gameEventToggles[e][p], 0);
        if (gameEventGateWasOn[e][p]) switchCodeToggle(gameEventBorrowed[e][p], 1);
        gameEventGateWasOn[e][p] = 0;
    }
}

// ---- The first square of a loop: the gold mine and Young Dvala ---------------------------------
static int gameEventOnceFired[GAME_EVENT_COUNT];

static void *gameEventCallTarget(int e, int p) {
    const CodeToggle *t = &gameEventToggles[e][p];
    unsigned char *at = GAME_PTR(t->rva);
#ifdef __APPLE__
    int32_t words = (int32_t)(codeWordAt(t->game) << 6) >> 6;
    return at + (int64_t)words * 4;
#else
    int32_t rel;
    memcpy(&rel, t->game + 1, 4);
    return at + 5 + rel;
#endif
}

static void gameEventOnceDone(int e) {
    gameEventOnceFired[e] = 1;
    takeGameEventPatchesOut(e);
    writeLog("game_event_fired", "event=%s first_square=1", GAME_EVENTS[e].name);
}

static int gameEventStubPatch(int e, GameEventStub stub) {
    for (int p = 0; p < GAME_EVENTS[e].patches; p++)
        if (GAME_EVENTS[e].patch[p].stub == stub) return p;
    return -1;
}

static int gameEventIndex(const char *name);

// @platform-split gold_mine_attack: the Mac copy takes (x, y) and stores the Windows 25 itself
#ifdef __APPLE__
static void gameEventGoldMineOnce(int x, int y) {
    int e = gameEventIndex("goldmine");
    ((void (*)(int, int))gameEventCallTarget(e, gameEventStubPatch(e, GAME_EVENT_STUB_GOLD_MINE_ONCE)))(x, y);
    gameEventOnceDone(e);
}
#else
static void gameEventGoldMineOnce(int kind, int x, int y) {
    int e = gameEventIndex("goldmine");
    ((void (*)(int, int, int))gameEventCallTarget(e, gameEventStubPatch(e, GAME_EVENT_STUB_GOLD_MINE_ONCE)))(kind, x, y);
    gameEventOnceDone(e);
}
#endif

static int gameEventPopulateOnce(int x, int y, const char *name, int count, int player, int commander) {
    int e = gameEventIndex("youngdvala");
    int last = ((int (*)(int, int, const char *, int, int, int))gameEventCallTarget(e, gameEventStubPatch(e, GAME_EVENT_STUB_POPULATE_ONCE)))(
        x, y, name, count, player, commander);
    gameEventOnceDone(e);
    return last;
}

static const void *gameEventStubCode[GAME_EVENT_STUB_COUNT];

static int prepareGameEventStubs(void) {
    void *const fns[GAME_EVENT_STUB_COUNT] = {NULL, (void *)gameEventGoldMineOnce, (void *)gameEventPopulateOnce};
    int made = 0;
#ifdef __APPLE__
    for (int s = 1; s < GAME_EVENT_STUB_COUNT; s++) {
        if (!gameEventStubCode[s]) gameEventStubCode[s] = newTrampoline(fns[s]);
        made += gameEventStubCode[s] != NULL;
    }
#else
    static unsigned char *page;
    if (!page && !(page = allocNear(GAME_PTR(RVA_AFTER_GAME_TURN)))) {
        writeLogError("game_event_stubs_refused", "reason=no_memory_near_game");
        return 0;
    }
    for (int s = 1; s < GAME_EVENT_STUB_COUNT; s++) {
        unsigned char *at = page + (size_t)s * GAME_EVENT_STUB_BYTES;
        if (!gameEventStubCode[s]) buildRedirectCallStub(at, at, fns[s], NULL), gameEventStubCode[s] = at;
        made++;
    }
#endif
    if (made < GAME_EVENT_STUB_COUNT - 1) writeLogError("game_event_stubs_refused", "reason=no_room made=%d of %d", made, GAME_EVENT_STUB_COUNT - 1);
    return made == GAME_EVENT_STUB_COUNT - 1;
}

static void prepareGameEvents(void) {
    if (gameEventsPrepared) return;
    gameEventsPrepared = 1;
    int stubs = prepareGameEventStubs(), patches = 0, available = 0, borrowed = 0, events = 0;
    for (int e = 0; e < GAME_EVENT_COUNT; e++) {
        int all = 1;
        for (int p = 0; p < GAME_EVENTS[e].patches; p++) {
            const GameEventPatch *patch = &GAME_EVENTS[e].patch[p];
            CodeToggle *t = &gameEventToggles[e][p];
            *t = (CodeToggle){.owner = GAME_EVENTS[e].name, .rva = patch->rva, .target = patch->target, .kind = patch->kind,
                              .to = patch->stub ? gameEventStubCode[patch->stub] : NULL};
            unsigned size = (unsigned)codeToggleSize(t->kind);
            patches++;
            if (!gameCodeMatches || !size || (patch->stub && !t->to)) {
                writeLogError("code_toggle_refused", "owner=%s rva=0x%x reason=%s", t->owner, t->rva,
                              !gameCodeMatches ? "game_code_differs" : !size ? "bad_kind" : "no_stub");
                all = 0;
                continue;
            }
            gameEventBorrowed[e][p] = randomEventGateAt(t->rva);
            t->available = gameEventBorrowed[e][p] || gameEventsClaimed(t->rva, size) || claimGameCode(t->rva, size, GAME_EVENT_OWNER);
            borrowed += gameEventBorrowed[e][p] != NULL;
            available += t->available;
            all = all && t->available;
        }
        events += all;
    }
    writeResultLog(available == patches && stubs, "game_events_prepared", "events=%d of %d patches=%d available=%d borrowed=%d stubs=%d", events,
                   GAME_EVENT_COUNT, patches, available, borrowed, stubs);
}

static int gameEventAvailable(int e) {
    for (int p = 0; p < GAME_EVENTS[e].patches; p++)
        if (!gameEventToggles[e][p].available) return 0;
    return 1;
}

// ---- The queue ----------------------------------------------------------------------------------
typedef struct {
    int event, turn;
    char by[MOD_NAME_MAX];
} QueuedGameEvent;
static QueuedGameEvent gameEventQueue[GAME_EVENT_QUEUE_MAX];
static int gameEventQueueCount;
static int gameEventFiring[GAME_EVENT_QUEUE_MAX];
static int gameEventSavedSeals[2];

static int gameEventIndex(const char *name) {
    for (int e = 0; e < GAME_EVENT_COUNT; e++)
        if (!strcmp(GAME_EVENTS[e].name, name)) return e;
    return -1;
}

static int gameEventQueued(int e) {
    for (int i = 0; i < gameEventQueueCount; i++)
        if (gameEventQueue[i].event == e) return i;
    return -1;
}

static const char *gameEventClash(int e) {
    const GameEvent *a = &GAME_EVENTS[e];
    int aSeals = a->extra == GAME_EXTRA_SEAL || a->extra == GAME_EXTRA_SEAL_BREAK;
    for (int i = 0; i < gameEventQueueCount; i++) {
        if (!gameEventFiring[i]) continue;
        const GameEvent *b = &GAME_EVENTS[gameEventQueue[i].event];
        if (aSeals && (b->extra == GAME_EXTRA_SEAL || b->extra == GAME_EXTRA_SEAL_BREAK)) return b->name;
        for (int p = 0; p < a->patches; p++)
            for (int r = 0; r < b->patches; r++)
                if (a->patch[p].rva == b->patch[r].rva &&
                    (a->patch[p].kind != b->patch[r].kind || a->patch[p].target != b->patch[r].target || a->patch[p].stub != b->patch[r].stub))
                    return b->name;
    }
    return NULL;
}

static int gameEventPatchShared(int firing, int e, int p) {
    for (int i = 0; i < firing; i++) {
        if (!gameEventFiring[i]) continue;
        const GameEvent *b = &GAME_EVENTS[gameEventQueue[i].event];
        for (int r = 0; r < b->patches; r++)
            if (b->patch[r].rva == GAME_EVENTS[e].patch[p].rva) return 1;
    }
    return 0;
}

static int putGameEventIn(int i) {
    int e = gameEventQueue[i].event;
    const GameEvent *ev = &GAME_EVENTS[e];
    if (ev->extra == GAME_EXTRA_SEAL || ev->extra == GAME_EXTRA_SEAL_BREAK)
        gameEventSavedSeals[0] = G32(RVA_SEAL_EVENTS_DONE), gameEventSavedSeals[1] = G32(RVA_SEALS_BROKEN);
    if (ev->extra == GAME_EXTRA_SEAL) G32(RVA_SEAL_EVENTS_DONE) = ev->seal - 1, G32(RVA_SEALS_BROKEN) = ev->seal;
    gameEventOnceFired[e] = 0;
    int refused = 0;
    for (int p = 0; p < ev->patches; p++) {
        if (gameEventPatchShared(i, e, p)) continue;
        CodeToggle *gate = gameEventBorrowed[e][p];
        gameEventGateWasOn[e][p] = gate && gate->applied;
        if (gameEventGateWasOn[e][p] && switchCodeToggle(gate, 0)) refused++;
        else if (switchCodeToggle(&gameEventToggles[e][p], 1)) refused++;
    }
    return refused;
}

static void takeGameEventOut(int i) {
    int e = gameEventQueue[i].event;
    const GameEvent *ev = &GAME_EVENTS[e];
    takeGameEventPatchesOut(e);
    if (ev->extra == GAME_EXTRA_EVIL && G32(RVA_EVIL_TIMES_MONTHS) < 0) G32(RVA_EVIL_TIMES_MONTHS) = 0;
    if (ev->extra == GAME_EXTRA_SEAL) G32(RVA_SEAL_EVENTS_DONE) = gameEventSavedSeals[0], G32(RVA_SEALS_BROKEN) = gameEventSavedSeals[1];
    int sealBroke = ev->extra == GAME_EXTRA_SEAL_BREAK && G32(RVA_SEALS_BROKEN) != gameEventSavedSeals[1];
    if (sealBroke && ev->seal) G32(RVA_SEALS_BROKEN) = ev->seal;
    if ((ev->extra == GAME_EXTRA_ONCE && !gameEventOnceFired[e]) || (ev->extra == GAME_EXTRA_SEAL_BREAK && !sealBroke))
        writeLog("game_event_not_fired", "event=%s by=%s reason=no_place_found", ev->name, gameEventQueue[i].by);
    else writeLog("game_event_done", "event=%s by=%s seals=%d", ev->name, gameEventQueue[i].by, G32(RVA_SEALS_BROKEN));
}

static void gameEventsBeforeTurn(const CallInfo *call) {
    (void)call;
    int turn = G32(RVA_TURN);
    for (int i = 0; i < gameEventQueueCount; i++) {
        QueuedGameEvent *q = &gameEventQueue[i];
        const GameEvent *ev = &GAME_EVENTS[q->event];
        gameEventFiring[i] = 0;
        if (q->turn != turn && q->turn + 1 != turn) {
            writeLogWarning("game_event_dropped", "event=%s by=%s turn=%d queued_turn=%d reason=other_game", ev->name, q->by, turn, q->turn);
            q->event = -1;
            continue;
        }
        if (ev->extra == GAME_EXTRA_SEAL_BREAK && G32(RVA_SEALS_BROKEN) >= SEALS_LAST) {
            writeLog("game_event_not_fired", "event=%s by=%s reason=seals_done seals=%d", ev->name, q->by, G32(RVA_SEALS_BROKEN));
            q->event = -1;
            continue;
        }
        const char *clash = gameEventClash(q->event);
        if (clash) {
            writeLog("game_event_waits", "event=%s by=%s turn=%d reason=clashes_with clashes_with=%s", ev->name, q->by, turn, clash);
            q->turn = turn;
            continue;
        }
        gameEventFiring[i] = 1;
        randomEventsHeld = 1;
        int refused = putGameEventIn(i);
        writeResultLog(!refused, "game_event_forced", "event=%s by=%s turn=%d patches=%d refused=%d", ev->name, q->by, turn, ev->patches, refused);
        if (refused) {
            takeGameEventPatchesOut(q->event);
            if (ev->extra == GAME_EXTRA_SEAL) G32(RVA_SEAL_EVENTS_DONE) = gameEventSavedSeals[0], G32(RVA_SEALS_BROKEN) = gameEventSavedSeals[1];
            writeLogWarning("game_event_not_fired", "event=%s by=%s reason=patch_refused", ev->name, q->by);
            gameEventFiring[i] = 0;
            q->event = -1;
        }
    }
}

static void gameEventsAfterTurn(const CallInfo *call) {
    (void)call;
    for (int i = gameEventQueueCount - 1; i >= 0; i--)
        if (gameEventFiring[i]) takeGameEventOut(i);
    int kept = 0;
    for (int i = 0; i < gameEventQueueCount; i++)
        if (!gameEventFiring[i] && gameEventQueue[i].event >= 0) gameEventQueue[kept++] = gameEventQueue[i];
    gameEventQueueCount = kept;
    memset(gameEventFiring, 0, sizeof gameEventFiring);
    randomEventsHeld = 0;
    if (randomEventsHeldSet) {
        randomEventsHeldSet = 0;
        setRandomEventsOff(randomEventsHeldMask, randomEventsHeldBy);
    }
}

static void dropGameEvents(const char *why) {
    if (gameEventQueueCount) writeLogWarning("game_events_dropped", "count=%d reason=%s", gameEventQueueCount, why);
    gameEventQueueCount = 0;
}

// ---- Lua ----------------------------------------------------------------------------------------
static int lGameEvents(lua_State *L) {
    lua_createtable(L, GAME_EVENT_COUNT, 0);
    for (int e = 0; e < GAME_EVENT_COUNT; e++) {
        const GameEvent *ev = &GAME_EVENTS[e];
        lua_createtable(L, 0, 4);
        lua_pushstring(L, ev->name);
        lua_setfield(L, -2, "name");
        lua_pushstring(L, ev->label);
        lua_setfield(L, -2, "label");
        lua_pushboolean(L, gameEventQueued(e) >= 0);
        lua_setfield(L, -2, "queued");
        lua_createtable(L, GAME_EVENT_NEEDS_MAX, 0);
        for (int n = 0; ev->needs[n]; n++) {
            lua_pushstring(L, ev->needs[n]);
            lua_rawseti(L, -2, n + 1);
        }
        lua_setfield(L, -2, "needs");
        lua_rawseti(L, -2, e + 1);
    }
    return 1;
}

static int gameEventsObserved;

static const char *gameEventQueueRefusal(int e) {
    if (G32(RVA_ACTIVE_PLAYER) < 0) return "no_game";
    if (e < 0) return "unknown_event";
    if (gameEventQueued(e) >= 0) return "already_queued";
    if (GAME_EVENTS[e].extra == GAME_EXTRA_SEAL_BREAK && G32(RVA_SEALS_BROKEN) >= SEALS_LAST) return "seals_done";
    if (gameEventQueueCount >= GAME_EVENT_QUEUE_MAX) return "queue_full";
    prepareGameEvents();
    if (!gameEventAvailable(e)) return gameCodeMatches ? "patch_unavailable" : "game_code_differs";
    if (!gameEventsObserved && !(gameEventsObserved = apiObserve(SITE_AFTER_GAME_TURN, gameEventsBeforeTurn, gameEventsAfterTurn)))
        return "site_unavailable";
    return NULL;
}

static int lQueueGameEvent(lua_State *L) {
    const char *caller = runningLuaMod() ? runningLuaMod() : "-";
    const char *name = luaL_checkstring(L, 1);
    int e = gameEventIndex(name);
    const char *problem = gameEventQueueRefusal(e);
    if (problem) {
        refuseLuaCall(caller, "coe5.queueGameEvent", problem, "event=%.40s", name);
        lua_pushboolean(L, 0);
        return 1;
    }
    dropGameEventQueue = dropGameEvents;
    QueuedGameEvent *q = &gameEventQueue[gameEventQueueCount++];
    *q = (QueuedGameEvent){.event = e, .turn = G32(RVA_TURN)};
    snprintf(q->by, sizeof q->by, "%s", caller);
    writeLog("game_event_queued", "event=%s by=%s turn=%d queued=%d", name, caller, q->turn, gameEventQueueCount);
    lua_pushboolean(L, 1);
    return 1;
}
