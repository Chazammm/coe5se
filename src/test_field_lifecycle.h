#pragma once

/* Windows 5.43 only. The NEW_UNIT entry detour observes all callers, including
 * direct extender calls. Sidecar generations never live in a game record.
 * The sole unique override is NEW_UNIT's verified call to findUnique; ordinary
 * game calls retain their original behavior outside our owned creation scope.
 * Include after the testField struct, before its open/close implementation. */
typedef struct {
    int hp, monster, owner, x, y, valid;
} TestFieldLifeSnapshot;
typedef struct {
    unsigned long long generation;
    int actor, beforeTaken, afterTaken, cleanup;
    TestFieldLifeSnapshot created, before, after;
} TestFieldLifeActor;
static struct {
    int installed, active, depth, overrideDepth, allowMultiple, failed, pending, closed;
    unsigned long long session, serial, createdCount;
    unsigned long thread;
    char owner[MOD_NAME_MAX];
    const char *unavailable;
    unsigned long long *generations;
    TestFieldLifeActor *actors;
} testFieldLife;


static int testFieldLifeOwns(int unit) {
    return unit >= 0 && unit < UNIT_TABLE_MAX && testFieldLife.actors && testFieldLife.generations &&
           testFieldLife.actors[unit].generation &&
           testFieldLife.actors[unit].generation == testFieldLife.generations[unit];
}

static int testFieldLifeActorOwned(int unit) {
    return testFieldLifeOwns(unit) && testFieldLife.actors[unit].actor;
}

static TestFieldLifeSnapshot testFieldLifeRead(int unit) {
    TestFieldLifeSnapshot s = {.monster = -1, .owner = -1, .x = -1, .y = -1};
    if (!testFieldLifeOwns(unit)) return s;
    size_t at = (size_t)unit * UNIT_SIZE;
    s.hp = G16(RVA_UNIT_HP + at); s.monster = G16(RVA_UNIT_MONSTER + at); s.owner = G16(RVA_UNIT_PLAYER + at);
    s.x = G32(RVA_UNIT_X + at); s.y = G32(RVA_UNIT_X + at + 4); s.valid = 1;
    return s;
}

#ifndef __APPLE__
static int (*testFieldLifeNewUnit)(int, int, int, int, int);

static int testFieldLifeFindUnique(int monster) {
    if (testFieldLife.active && testFieldLife.overrideDepth > 0 && testFieldLife.allowMultiple &&
        GetCurrentThreadId() == testFieldLife.thread) return -1;
    return ((int (*)(int))(void *)(GAME_PTR(RVA_MONSTER_UNIT_FIRST)))(monster);
}

static int testFieldLifeNewUnitHook(int x, int y, int player, int commander, int monster) {
    const char *mod = runningLuaMod();
    int scope = testFieldLife.active && testFieldLife.depth > 0 && GetCurrentThreadId() == testFieldLife.thread &&
                (!mod || !_stricmp(mod, testFieldLife.owner));
    int fieldPlayer = player == 0 || player == TEST_BATTLE_ENEMY_PLAYER;
    int local = scope && (fieldPlayer || player == 31) &&
                ((x == testField.x && y == testField.y) || (x == -1 && y == -1));
    int previous = testFieldLife.overrideDepth;
    testFieldLife.overrideDepth = local && testFieldLife.allowMultiple ? previous + 1 : 0;
    int unit = testFieldLifeNewUnit(x, y, player, commander, monster);
    testFieldLife.overrideDepth = previous;
    if (unit >= 0 && unit < UNIT_TABLE_MAX) {
        unsigned long long generation = ++testFieldLife.serial;
        if (!generation || generation > LLONG_MAX) testFieldLife.failed = 1;
        testFieldLife.generations[unit] = generation;
        if (local) {
            size_t at = (size_t)unit * UNIT_SIZE;
            if (G32(RVA_UNIT_X + at) != x || G32(RVA_UNIT_X + at + 4) != y || G16(RVA_UNIT_PLAYER + at) != player) {
                /* An external callback changed the constructor result. Do not
                 * claim or clean up an actor whose provenance cannot be proved. */
                testFieldLife.failed = 1;
                return unit;
            }
            TestFieldLifeActor *a = &testFieldLife.actors[unit];
            *a = (TestFieldLifeActor){.generation = generation, .actor = player == 0 || player == TEST_BATTLE_ENEMY_PLAYER};
            a->created = testFieldLifeRead(unit);
            testFieldLife.createdCount++;
        }
    }
    return unit;
}

static int installTestFieldLifecycle(void) {
    if (testFieldLife.installed) return testFieldLife.installed > 0;
    if (!gameCodeMatches) return 0;
    testFieldLife.installed = -1; /* One transaction; failed installs never retry allocations. */
    testFieldLife.unavailable = "install_incomplete";
    /* Complete instructions, no PC-relative operands, preceding sub rsp,48. */
    static const unsigned char PROLOG[15] = {0x89,0x54,0x24,0x10,0x89,0x4c,0x24,0x08,0x55,0x41,0x55,0x41,0x56,0x41,0x57};
    unsigned char *entry = GAME_PTR(RVA_NEW_UNIT), *call = GAME_PTR(RVA_TEST_FIELD_NEW_UNIT_UNIQUE_CALL);
    int relative;
    memcpy(&relative, call + 1, 4);
    if (memcmp(entry, PROLOG, sizeof PROLOG) || call[0] != 0xe8 || call + 5 + relative != GAME_PTR(RVA_MONSTER_UNIT_FIRST)) {
        writeLogError("test_field_lifecycle_unavailable", "reason=code_differs");
        testFieldLife.unavailable = "code_differs";
        return 0;
    }
    testFieldLife.generations = calloc(UNIT_TABLE_MAX, sizeof *testFieldLife.generations);
    testFieldLife.actors = calloc(UNIT_TABLE_MAX, sizeof *testFieldLife.actors);
    unsigned char *page = allocNear(entry);
    if (!testFieldLife.generations || !testFieldLife.actors || !page) {
        writeLogError("test_field_lifecycle_unavailable", "reason=out_of_memory");
        testFieldLife.unavailable = "out_of_memory";
        return 0;
    }
    unsigned char slot[ENTRY_HOOK_SLOT];
    if (!buildEntryHookSlot(slot, page, entry, PROLOG, sizeof PROLOG, NULL, 0, (void *)testFieldLifeNewUnitHook)) {
        testFieldLife.unavailable = "trampoline_out_of_reach"; return 0;
    }
    memcpy(page, slot, sizeof slot);
    unsigned char *find = page + ENTRY_HOOK_SLOT;
    static const unsigned char JMP_RIP[6] = {0xff,0x25,0,0,0,0};
    memcpy(find, JMP_RIP, sizeof JMP_RIP);
    void *target = (void *)testFieldLifeFindUnique;
    memcpy(find + 6, &target, 8);
    if (!makePagesReadExecute(page, 0x1000)) { testFieldLife.unavailable = "trampoline_protection"; return 0; }
    unsigned char oldCall[5], newCall[5], jump[sizeof PROLOG];
    memcpy(oldCall, call, sizeof oldCall);
    memcpy(newCall, oldCall, sizeof newCall);
    long long distance = find - (call + 5);
    if (distance < INT32_MIN || distance > INT32_MAX) { testFieldLife.unavailable = "call_out_of_reach"; return 0; }
    relative = (int)distance;
    memcpy(newCall + 1, &relative, 4);
    buildEntryHookJump(jump, entry, page + ENTRY_HOOK_STUB, sizeof PROLOG);
    testFieldLifeNewUnit = (int (*)(int,int,int,int,int))(void *)page;
    if (!writeGameCode(call, newCall, sizeof newCall)) { testFieldLife.unavailable = "call_unwritable"; return 0; }
    if (!writeGameCode(entry, jump, sizeof jump)) {
        int rolledBack = writeGameCode(call, oldCall, sizeof oldCall) && !memcmp(call, oldCall, sizeof oldCall);
        testFieldLife.unavailable = rolledBack ? "entry_unwritable" : "install_rollback_failed";
        writeLogError("test_field_lifecycle_unavailable", "reason=%s rollback=%d", testFieldLife.unavailable, rolledBack);
        return 0;
    }
    testFieldLife.installed = 1;
    testFieldLife.unavailable = NULL;
    writeLog("test_field_lifecycle_installed", "generation_tracking=1 scoped_multiple_unique=1");
    return 1;
}
#else
static int installTestFieldLifecycle(void) { return 0; }
#endif

static int testFieldLifeBegin(const char *owner, int multiple) {
    if (!installTestFieldLifecycle() || testFieldLife.pending || testFieldLife.failed) return 0;
    memset(testFieldLife.actors, 0, UNIT_TABLE_MAX * sizeof *testFieldLife.actors);
    testFieldLife.session++; testFieldLife.createdCount = 0;
    testFieldLife.active = 1; testFieldLife.depth = testFieldLife.overrideDepth = 0;
    testFieldLife.allowMultiple = multiple; testFieldLife.closed = 0;
#ifndef __APPLE__
    testFieldLife.thread = GetCurrentThreadId();
#endif
    snprintf(testFieldLife.owner, sizeof testFieldLife.owner, "%s", owner);
    return 1;
}

static void testFieldLifeEnter(void) { if (testFieldLife.active) testFieldLife.depth++; }
static void testFieldLifeLeave(void) { if (testFieldLife.depth > 0) testFieldLife.depth--; }

static void testFieldLifeClearAnchors(int unit) {
    int width = G32(RVA_BATTLE_WIDTH);
    if (width < 1 || width > 0x600) return;
    for (int x = 0; x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            unsigned char *cell = GAME_PTR(RVA_BATTLE_GRID) + ((size_t)x * BATTLE_ROWS + y) * 24;
            if (*(int *)cell == unit) {
                *(int *)cell = -1;
            }
            if (*(int *)(cell + 4) == unit) {
                *(int *)(cell + 4) = -1;
                *(unsigned int *)(cell + 8) &= ~0x101u;
            }
        }
}

static int testFieldLifeCleanup(void) {
    if (!testFieldLife.actors) return 0;
    int complete = !testFieldLife.failed;
    for (int unit = 0; unit < UNIT_TABLE_MAX; unit++) {
        TestFieldLifeActor *a = &testFieldLife.actors[unit];
        if (!a->generation) continue;
        a->before = testFieldLifeRead(unit); a->beforeTaken = 1;
        if (!a->before.valid) {
            a->cleanup = 3; /* A later constructor replaced this identity. */
        } else {
            if (a->before.hp > 0) G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE) = 0;
            testFieldLifeClearAnchors(unit);
            a->cleanup = a->before.hp > 0 ? 1 : 2;
        }
        a->after = testFieldLifeRead(unit); a->afterTaken = 1;
        if (a->after.valid && a->after.hp > 0) { a->cleanup = 4; complete = 0; }
        writeLog("test_field_actor_cleanup", "session=%llu unit=%d generation=%llu matched=%d before_hp=%d before_monster=%d before_owner=%d before_x=%d before_y=%d after_hp=%d result=%d",
                 testFieldLife.session, unit, a->generation, a->before.valid, a->before.hp, a->before.monster,
                 a->before.owner, a->before.x, a->before.y, a->after.hp, a->cleanup);
    }
    testFieldLife.active = 0; testFieldLife.depth = testFieldLife.overrideDepth = 0;
    testFieldLife.pending = !complete; testFieldLife.closed = 1;
    return complete;
}
