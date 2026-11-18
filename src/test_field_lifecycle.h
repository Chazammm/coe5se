#pragma once

// @platform-split test_field_calls: Windows redirects the call through a stub in a near page (redirectCall), Mac points the bl at a trampoline in the header padding
typedef struct {
    unsigned int callRva, targetRva;
    void *hook;
    const char *name;
    unsigned char *stub;
    int on;
} TestFieldCall;

#ifndef __APPLE__
#define TEST_FIELD_CALL_STUB 0x20
static unsigned char *testFieldCallPage;
static int testFieldCallStubs;
#endif

static int pointTestFieldCall(TestFieldCall *c) {
    if (c->on) return 1;
#ifdef __APPLE__
    unsigned char *call = gameBase + c->callRva;
    if (blTarget(call) != gameBase + c->targetRva) {
        writeLog("hook_skipped", "site=%s reason=not_the_expected_call", c->name);
        return 0;
    }
    if (!c->stub) c->stub = newTrampoline(c->hook);
    uint32_t word = c->stub ? blTo(call, c->stub) : 0;
    if (!c->stub || !writeGameCode(call, &word, 4)) {
        writeLog("hook_skipped", "site=%s reason=%s", c->name, c->stub ? "code_unwritable" : "no_padding");
        return 0;
    }
#else
    if (!testFieldCallPage && (testFieldCallPage = allocNear(gameBase + RVA_TEXT_SECTION))) makePagesReadExecute(testFieldCallPage, 0x1000);
    if (!testFieldCallPage) {
        writeLog("hook_skipped", "site=%s reason=no_near_page", c->name);
        return 0;
    }
    if (!c->stub) c->stub = testFieldCallPage + TEST_FIELD_CALL_STUB * testFieldCallStubs++;
    if (!redirectCall(c->callRva, c->targetRva, c->stub, c->hook, c->name, NULL)) return 0;
#endif
    c->on = 1;
    return 1;
}

static int restoreTestFieldCall(TestFieldCall *c) {
    if (!c->on) return 1;
    unsigned char *call = gameBase + c->callRva;
#ifdef __APPLE__
    uint32_t word = blTo(call, gameBase + c->targetRva);
    int ok = writeGameCode(call, &word, 4);
#else
    int rel = (int)((gameBase + c->targetRva) - (call + 5));
    int ok = writeGameCode(call + 1, &rel, 4);
#endif
    if (ok) c->on = 0;
    return ok;
}

typedef struct {
    int hp, monster, owner, x, y, valid;
} TestFieldUnitSnapshot;

typedef struct {
    unsigned long long generation;
    int actor, beforeTaken, afterTaken, cleanup;
    TestFieldUnitSnapshot created, before, after;
} TestFieldMadeUnit;

static struct {
    int installed, tracking, active, depth, overrideDepth, allowMultiple, failed, pending;
    unsigned long long session, serial, createdCount;
    unsigned long thread;
    char owner[MOD_NAME_MAX];
    unsigned long long *generations;
    TestFieldMadeUnit *actors;
} testFieldLife;

static int isTestFieldUnit(int unit) {
    return unit >= 0 && unit < UNIT_TABLE_MAX && testFieldLife.tracking && testFieldLife.actors[unit].generation &&
           testFieldLife.actors[unit].generation == testFieldLife.generations[unit];
}

static int isTestFieldActor(int unit) {
    return isTestFieldUnit(unit) && testFieldLife.actors[unit].actor;
}

static TestFieldUnitSnapshot readTestFieldUnitSnapshot(int unit) {
    TestFieldUnitSnapshot s = {.monster = -1, .owner = -1, .x = -1, .y = -1};
    if (!isTestFieldUnit(unit)) return s;
    size_t at = (size_t)unit * UNIT_SIZE;
    s.hp = G16(RVA_UNIT_HP + at);
    s.monster = G16(RVA_UNIT_MONSTER + at);
    s.owner = G16(RVA_UNIT_PLAYER + at);
    s.x = G32(RVA_UNIT_X + at);
    s.y = G32(RVA_UNIT_X + at + 4);
    s.valid = 1;
    return s;
}

static int (*gameNewUnit)(int x, int y, int player, int commander, int monster);

static int testFieldAllowsSecondUnique(void) {
    return testFieldLife.active && testFieldLife.overrideDepth > 0 && testFieldLife.allowMultiple && GetCurrentThreadId() == testFieldLife.thread;
}

#ifndef __APPLE__
// @platform-split test_field_unique: Windows answers newunit's own call of the unique search; Mac's newunit holds a copy of the search, whose count read a step hook answers
static int testFieldMonsterUnitFirstHook(int monster) {
    if (testFieldAllowsSecondUnique()) return -1;
    return uniqueGroupUnitFirst(monster);
}
static TestFieldCall testFieldUniqueCall = {RVA_NEW_UNIT_UNIQUE_CALL, RVA_MONSTER_UNIT_FIRST, (void *)testFieldMonsterUnitFirstHook, "new_unit_unique", NULL, 0};
#else
// @platform-split test_field_unique: Windows answers newunit's own call of the unique search; Mac's newunit holds a copy of the search, whose count read a step hook answers
#define NEW_UNIT_UNIQUE_COUNT_READ_WORD 0xb9487908u
#define UNIT_COUNT_PAGE_OFFSET 0x878
static const int32_t testFieldNoUnitCount = -1;
static struct {
    StepHook hook;
    uint32_t game, branch;
    int put, on;
} testFieldUniqueRead;

static void testFieldUniqueCountRead(StepContext *ctx, const StepHook *hook) {
    (void)hook;
    if (testFieldAllowsSecondUnique()) ctx->x[8] = (uint64_t)(uintptr_t)&testFieldNoUnitCount - UNIT_COUNT_PAGE_OFFSET;
}

static int pointTestFieldUniqueRead(void) {
    if (testFieldUniqueRead.on) return 1;
    unsigned char *at = gameBase + RVA_NEW_UNIT_UNIQUE_COUNT_READ;
    if (!testFieldUniqueRead.put) {
        memcpy(&testFieldUniqueRead.game, at, 4);
        if (testFieldUniqueRead.game != NEW_UNIT_UNIQUE_COUNT_READ_WORD) {
            writeLog("hook_skipped", "site=new_unit_unique reason=not_the_expected_code");
            return 0;
        }
        testFieldUniqueRead.hook.handler = testFieldUniqueCountRead;
        if (!putStepHookMac(RVA_NEW_UNIT_UNIQUE_COUNT_READ, &testFieldUniqueRead.hook, "new_unit_unique", 0)) return 0;
        memcpy(&testFieldUniqueRead.branch, at, 4);
        testFieldUniqueRead.put = testFieldUniqueRead.on = 1;
        return 1;
    }
    if (!writeGameCode(at, &testFieldUniqueRead.branch, 4)) return 0;
    testFieldUniqueRead.on = 1;
    return 1;
}

static int restoreTestFieldUniqueRead(void) {
    if (!testFieldUniqueRead.on) return 1;
    if (!writeGameCode(gameBase + RVA_NEW_UNIT_UNIQUE_COUNT_READ, &testFieldUniqueRead.game, 4)) return 0;
    testFieldUniqueRead.on = 0;
    return 1;
}
#endif

static int testFieldNewUnitHook(int x, int y, int player, int commander, int monster) {
    const char *mod = runningLuaMod();
    int scope = testFieldLife.active && testFieldLife.depth > 0 && GetCurrentThreadId() == testFieldLife.thread && (!mod || !_stricmp(mod, testFieldLife.owner));
    int fieldPlayer = player == 0 || player == TEST_BATTLE_ENEMY_PLAYER;
    int local = scope && (fieldPlayer || player == 31) && ((x == testField.x && y == testField.y) || (x == -1 && y == -1));
    int previous = testFieldLife.overrideDepth;
    testFieldLife.overrideDepth = local && testFieldLife.allowMultiple ? previous + 1 : 0;
    int unit = gameNewUnit(x, y, player, commander, monster);
    testFieldLife.overrideDepth = previous;
    if (unit < 0 || unit >= UNIT_TABLE_MAX) return unit;
    unsigned long long generation = ++testFieldLife.serial;
    if (!generation || generation > LLONG_MAX) testFieldLife.failed = 1;
    testFieldLife.generations[unit] = generation;
    if (!local) return unit;
    size_t at = (size_t)unit * UNIT_SIZE;
    if (G32(RVA_UNIT_X + at) != x || G32(RVA_UNIT_X + at + 4) != y || G16(RVA_UNIT_PLAYER + at) != player) {
        testFieldLife.failed = 1;
        return unit;
    }
    TestFieldMadeUnit *made = &testFieldLife.actors[unit];
    *made = (TestFieldMadeUnit){.generation = generation, .actor = fieldPlayer};
    made->created = readTestFieldUnitSnapshot(unit);
    testFieldLife.createdCount++;
    return unit;
}

static void turnOffTestFieldLifecycle(void) {
#ifndef __APPLE__
    restoreTestFieldCall(&testFieldUniqueCall);
#else
    restoreTestFieldUniqueRead();
#endif
    if (testFieldLife.pending || !testFieldLife.tracking) return;
    if (fanpatchHookEnable(gameBase + RVA_NEW_UNIT, 0)) testFieldLife.tracking = 0;
}

static int turnOnTestFieldLifecycle(int multiple) {
    if (!gameCodeMatches || testFieldLife.installed < 0) return 0;
    if (!testFieldLife.generations) testFieldLife.generations = calloc(UNIT_TABLE_MAX, sizeof *testFieldLife.generations);
    if (!testFieldLife.actors) testFieldLife.actors = calloc(UNIT_TABLE_MAX, sizeof *testFieldLife.actors);
    if (!testFieldLife.generations || !testFieldLife.actors) {
        writeLogError("test_field_lifecycle_unavailable", "reason=out_of_memory");
        return 0;
    }
    void *target = gameBase + RVA_NEW_UNIT;
    if (!testFieldLife.tracking) {
        if (!fanpatchHookCreate(target, (void *)testFieldNewUnitHook, (void **)&gameNewUnit) || !fanpatchHookEnable(target, 1)) {
            testFieldLife.installed = -1;
            writeLogError("test_field_lifecycle_unavailable", "reason=no_entry_hook");
            return 0;
        }
        testFieldLife.tracking = 1;
    }
#ifndef __APPLE__
    if (multiple && !uniqueGroups.newUnitCall && !pointTestFieldCall(&testFieldUniqueCall)) {
#else
    if (multiple && !pointTestFieldUniqueRead()) {
#endif
        turnOffTestFieldLifecycle();
        return 0;
    }
    testFieldLife.installed = 1;
    return 1;
}

static int beginTestFieldLife(const char *owner, int multiple) {
    if (testFieldLife.pending || testFieldLife.failed || !turnOnTestFieldLifecycle(multiple)) return 0;
    memset(testFieldLife.actors, 0, UNIT_TABLE_MAX * sizeof *testFieldLife.actors);
    testFieldLife.session++;
    testFieldLife.createdCount = 0;
    testFieldLife.active = 1;
    testFieldLife.depth = testFieldLife.overrideDepth = 0;
    testFieldLife.allowMultiple = multiple;
    testFieldLife.thread = GetCurrentThreadId();
    snprintf(testFieldLife.owner, sizeof testFieldLife.owner, "%s", owner);
    return 1;
}

static void enterTestFieldCode(void) {
    if (testFieldLife.active) testFieldLife.depth++;
}
static void leaveTestFieldCode(void) {
    if (testFieldLife.depth > 0) testFieldLife.depth--;
}

static void clearTestFieldUnitCells(int unit) {
    int width = G32(RVA_BATTLE_WIDTH);
    if (width < 1 || width > BATTLE_COLUMNS_MAX) return;
    for (int x = 0; x < width; x++)
        for (int y = 0; y < BATTLE_ROWS; y++) {
            unsigned char *cell = battleCell(x, y);
            if (*(int *)cell == unit) *(int *)cell = -1;
            if (*(int *)(cell + BATTLE_CELL_OBJECT) == unit) {
                *(int *)(cell + BATTLE_CELL_OBJECT) = -1;
                *(unsigned int *)(cell + BATTLE_CELL_FLAGS) &= ~0x101u;
            }
        }
}

static int removeTestFieldUnits(void) {
    if (!testFieldLife.actors) return 0;
    int complete = !testFieldLife.failed;
    for (int unit = 0; unit < UNIT_TABLE_MAX; unit++) {
        TestFieldMadeUnit *made = &testFieldLife.actors[unit];
        if (!made->generation) continue;
        made->before = readTestFieldUnitSnapshot(unit);
        made->beforeTaken = 1;
        if (!made->before.valid) {
            made->cleanup = 3;
        } else {
            if (made->before.hp > 0) G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE) = 0;
            clearTestFieldUnitCells(unit);
            made->cleanup = made->before.hp > 0 ? 1 : 2;
        }
        made->after = readTestFieldUnitSnapshot(unit);
        made->afterTaken = 1;
        if (made->after.valid && made->after.hp > 0) {
            made->cleanup = 4;
            complete = 0;
        }
        writeLog("test_field_actor_cleanup",
                 "session=%llu unit=%d generation=%llu matched=%d before_hp=%d before_monster=%d before_owner=%d before_x=%d before_y=%d after_hp=%d result=%d",
                 testFieldLife.session, unit, made->generation, made->before.valid, made->before.hp, made->before.monster, made->before.owner, made->before.x,
                 made->before.y, made->after.hp, made->cleanup);
    }
    testFieldLife.active = 0;
    testFieldLife.depth = testFieldLife.overrideDepth = 0;
    testFieldLife.pending = !complete;
    return complete;
}
