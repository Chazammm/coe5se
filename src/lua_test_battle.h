#pragma once

#define TEST_BATTLE_X 1
#define TEST_BATTLE_Y 1
#define TEST_BATTLE_TERRAIN 0
#define TEST_BATTLE_ENEMY_PLAYER 24
#define TEST_FIELD_FIRE_RANGE 30
#define TEST_BATTLE_DELAY 400000
#define TEST_FIELD_MIN_WIDTH 32
#define TEST_FIELD_MAX_WIDTH 1536
#include "test_field_terrain_arg.h"
#include "test_field_terrain_policy.h"

static void beginTestFieldFinal(void);
static void finishTestFieldFinal(void);
static void resetTestFieldFinal(void);
static void turnOffTestFieldFinal(void);
static int testFieldFinalViewActive(void);
static void turnOffTestFieldTraining(void);
static int installBattleMapHooks(void);

static void clearTestBattleSquare(int x, int y) {
    int last = G32(RVA_UNIT_COUNT);
    for (int u = 0; u <= last && u < UNIT_TABLE_MAX; u++) {
        unsigned char *at = GAME_PTR(RVA_UNIT_X + (size_t)u * UNIT_SIZE);
        if (*(int *)at == x && *(int *)(at + 4) == y) *(short *)GAME_PTR(RVA_UNIT_HP + (size_t)u * UNIT_SIZE) = 0;
    }
}

static const char *testFieldTerrainProblem(lua_State *L, int terrain) {
    const unsigned char *record = GAME_PTR(RVA_TERRAIN_TABLE + (size_t)terrain * TERRAIN_SIZE);
    if (!*record) return "bad_terrain";
    const TestFieldTerrainPolicy *policy = findTestFieldTerrainPolicy(terrain);
    if (!policy) return "unsupported_terrain";
    pushGameTerrainName(L, terrain);
    uint64_t flags;
    unsigned int fortParts;
    memcpy(&flags, record + TERRAIN_FLAGS, sizeof flags);
    memcpy(&fortParts, record + TERRAIN_FORT_PARTS, sizeof fortParts);
    int matches = testFieldTerrainPolicyMatches(policy, lua_tostring(L, -1), flags, fortParts);
    lua_pop(L, 1);
    return matches ? NULL : "unsupported_terrain";
}

static int lTestFieldTerrains(lua_State *L) {
    lua_newtable(L);
    int n = 0;
    for (size_t i = 0; i < TEST_FIELD_TERRAIN_POLICY_COUNT; i++) {
        int terrain = testFieldTerrainPolicies[i].id;
        if (terrain <= G32(RVA_LAST_TERRAIN) && !testFieldTerrainProblem(L, terrain)) {
            lua_pushinteger(L, terrain);
            lua_rawseti(L, -2, ++n);
        }
    }
    return 1;
}

static unsigned hashTestFieldNeighbours(int x, int y) {
    unsigned hash = 2166136261u;
    for (int cx = x - 1; cx <= x + 1; cx++)
        for (int cy = y - 1; cy <= y + 1; cy++) {
            if (cx == x && cy == y) continue;
            const unsigned char *s = GAME_PTR(RVA_SQUARE_RECORDS + ((size_t)cx * MAP_COLUMN_SQUARES + cy) * SQUARE_SIZE);
            for (int i = 0; i < SQUARE_SIZE; i++) hash = (hash ^ s[i]) * 16777619u;
        }
    return hash;
}

static struct {
    int open, editor, unit, commander, spell, weapon, standIn, x, y, terrain, allowMultipleUnique, emptyEditor;
    unsigned neighbours;
    unsigned char square[SQUARE_SIZE];
    unsigned char plane[PLANE_SIZE];
    int planeBorrowed, lastPlane;
    char owner[MOD_NAME_MAX];
    int fighting;
    int minimumWidth, sizing;
    short army, teamA, teamB;
    int mapW, mapH, activePlayer, delay, showFlag;
    unsigned char *spellWeaponRecord, spellWeaponSaved[WEAPON_SIZE];
} testField;

#include "test_field_lifecycle.h"

// Applies an owned field's floor before native map allocation and placement.
static int applyTestFieldMinimumWidth(int width) {
    if (!testField.open || !testField.editor || !testField.sizing || !testFieldLife.active ||
        GetCurrentThreadId() != testFieldLife.thread || G32(RVA_BATTLE_X) != testField.x ||
        G32(RVA_BATTLE_Y) != testField.y || width < 1 || width >= testField.minimumWidth) return width;
    *(int *)GAME_PTR(RVA_BATTLE_WIDTH) = testField.minimumWidth;
    return testField.minimumWidth;
}

static unsigned long long testFieldDrawSession;
static const char *testFieldOpenFailure;

static unsigned char *testFieldSquare(void) {
    return GAME_PTR(RVA_SQUARE_RECORDS + ((size_t)testField.x * MAP_COLUMN_SQUARES + testField.y) * SQUARE_SIZE);
}

static int testFieldShowFlag(void) {
#ifdef __APPLE__
    return *GAME_PTR(RVA_TEST_GAME_FLAG);
#else
    return G32(RVA_TEST_GAME_FLAG);
#endif
}
static void setTestFieldShowFlag(int on) {
#ifdef __APPLE__
    *GAME_PTR(RVA_TEST_GAME_FLAG) = (unsigned char)on;
#else
    *(int *)GAME_PTR(RVA_TEST_GAME_FLAG) = on;
#endif
}

static void setTestFieldBattleOver(int on) {
#ifdef __APPLE__
    *GAME_PTR(RVA_BATTLE_OVER) = (unsigned char)on;
#else
    *(int *)GAME_PTR(RVA_BATTLE_OVER) = on;
#endif
}

static void restoreTestFieldPlane(void) {
    if (!testField.planeBorrowed) return;
    memcpy(planeRecord(0), testField.plane, PLANE_SIZE);
    *(int *)GAME_PTR(RVA_LAST_PLANE) = testField.lastPlane;
}

static int closeTestFieldNow(void) {
    if (testFieldLife.retiring) return 0;
    resetTestFieldText();
    resetTestFieldFinal();
    turnOffTestFieldTraining();
    turnOffTestFieldFinal();
    if (!testField.open) {
        int done = !testFieldLife.pending || removeTestFieldUnits();
        turnOffTestFieldLifecycle();
        return done;
    }
    int x = testField.x, y = testField.y, cleaned = 1;
    if (testField.spellWeaponRecord) memcpy(testField.spellWeaponRecord, testField.spellWeaponSaved, WEAPON_SIZE);
    if (testField.editor) cleaned = removeTestFieldUnits();
    else clearTestBattleSquare(x, y);
    short *army = (short *)GAME_PTR(RVA_SQUARE_ARMY + ((size_t)x * MAP_COLUMN_SQUARES + y) * 2);
    short *teamA = (short *)GAME_PTR(RVA_SLOT_TEAM), *teamB = (short *)GAME_PTR(RVA_SLOT_TEAM + (size_t)TEST_BATTLE_ENEMY_PLAYER * PLAYER_BLOCK_SIZE);
    memcpy(testFieldSquare(), testField.square, SQUARE_SIZE);
    *army = testField.army;
    *teamA = testField.teamA;
    *teamB = testField.teamB;
    *(int *)GAME_PTR(RVA_ACTIVE_PLAYER) = testField.activePlayer;
    *(int *)GAME_PTR(RVA_BATTLE_DELAY) = testField.delay;
    *(int *)GAME_PTR(RVA_MAP_W) = testField.mapW;
    *(int *)GAME_PTR(RVA_MAP_H) = testField.mapH;
    restoreTestFieldPlane();
    setTestFieldShowFlag(testField.showFlag);
    testField.open = 0;
    testField.fighting = 0;
    if (!testField.editor) {
        writeLog("test_field_closed", "x=%d y=%d", x, y);
        return 1;
    }
    int restored = !memcmp(testFieldSquare(), testField.square, SQUARE_SIZE) && *army == testField.army && G32(RVA_ACTIVE_PLAYER) == testField.activePlayer &&
                   G32(RVA_BATTLE_DELAY) == testField.delay && G32(RVA_MAP_W) == testField.mapW && G32(RVA_MAP_H) == testField.mapH &&
                   testFieldShowFlag() == testField.showFlag &&
                   (!testField.spellWeaponRecord || !memcmp(testField.spellWeaponRecord, testField.spellWeaponSaved, WEAPON_SIZE)) && *teamA == testField.teamA &&
                   *teamB == testField.teamB &&
                   (!testField.planeBorrowed || (!memcmp(planeRecord(0), testField.plane, PLANE_SIZE) && G32(RVA_LAST_PLANE) == testField.lastPlane));
    writeLog("test_field_closed", "x=%d y=%d borrowed_restored=%d neighbours_unchanged=%d actors_cleaned=%d session=%llu", x, y, restored,
             hashTestFieldNeighbours(x, y) == testField.neighbours, cleaned, testFieldLife.session);
    if (!restored) testFieldLife.failed = testFieldLife.pending = 1;
    turnOffTestFieldLifecycle();
    return cleaned && restored;
}

typedef struct {
    int monster;
    long long count;
} TestFieldGroup;

typedef struct {
    TestFieldGroup *groups;
    int groupCount;
    long long units;
} TestFieldSide;

typedef struct {
    lua_Integer monster, spell, standIn, terrain;
    int terrainSpecified, allowMultipleUnique, emptyEditor, minimumWidth;
    TestFieldSide friends, enemies;
} TestFieldSpec;

static const char *testFieldUniqueProblem(int commander, const TestFieldSide *friends, const TestFieldSide *enemies, int *monster, int *existing,
                                          long long *requested) {
    *monster = *existing = -1;
    *requested = 0;
    long long *counts = calloc(MONSTER_TABLE_MAX, sizeof *counts);
    if (!counts) return "out_of_memory";
    if (commander >= 0 && commander < MONSTER_TABLE_MAX && monsterAbility(commander, MONSTER_ABILITY_UNIQUE) > 0) counts[uniqueGroupStandIn(commander)] = 1;
    const TestFieldSide *sides[2] = {friends, enemies};
    for (int side = 0; side < 2; side++) {
        if (!sides[side]) continue;
        for (int g = 0; g < sides[side]->groupCount; g++) {
            int m = sides[side]->groups[g].monster;
            long long n = sides[side]->groups[g].count;
            if (m < 0 || m >= MONSTER_TABLE_MAX || monsterAbility(m, MONSTER_ABILITY_UNIQUE) <= 0) continue;
            m = uniqueGroupStandIn(m);
            counts[m] = counts[m] > LLONG_MAX - n ? LLONG_MAX : counts[m] + n;
        }
    }
    const char *reason = NULL;
    for (int m = 0; m < MONSTER_TABLE_MAX; m++)
        if (counts[m] > 1) {
            *monster = m;
            *requested = counts[m];
            reason = "unique_count";
            break;
        }
    int last = G32(RVA_UNIT_COUNT);
    if (!reason && (last < -1 || last >= UNIT_TABLE_MAX)) reason = "bad_unit_table";
    for (int unit = 0; !reason && unit < UNIT_TABLE_MAX; unit++) {
        size_t at = (size_t)unit * UNIT_SIZE;
        int m = G16(RVA_UNIT_MONSTER + at);
        int key = m >= 0 && m < MONSTER_TABLE_MAX && G16(RVA_UNIT_HP + at) > 0 ? uniqueGroupStandIn(m) : -1;
        if (key >= 0 && counts[key]) {
            *monster = m;
            *existing = unit;
            *requested = counts[key];
            reason = "unique_exists";
        }
    }
    free(counts);
    return reason;
}

static void freeTestFieldSide(TestFieldSide *side) {
    free(side->groups);
    side->groups = NULL;
    side->groupCount = 0;
    side->units = 0;
}

static const char *readTestFieldGroups(lua_State *L, int index, TestFieldSide *side, const char *badMonster) {
    freeTestFieldSide(side);
    if (!lua_istable(L, index)) return "bad_list";
    size_t n = lua_rawlen(L, index);
    if (n > (size_t)INT_MAX) return "bad_list";
    side->groups = calloc(n ? n : 1, sizeof *side->groups);
    if (!side->groups) return "out_of_memory";
    for (int i = 1; i <= (int)n; i++) {
        lua_rawgeti(L, index, i);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            return "bad_list";
        }
        lua_rawgeti(L, -1, 1);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, -1, "monster");
        }
        lua_rawgeti(L, -2, 2);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, -2, "count");
        }
        int isMonster, isCount;
        lua_Integer monster = lua_tointegerx(L, -2, &isMonster);
        lua_Integer count = lua_isnil(L, -1) ? (isCount = 1, 1) : lua_tointegerx(L, -1, &isCount);
        lua_pop(L, 3);
        if (!isMonster || !isUsedMonster(monster)) return badMonster;
        if (!isCount || count < 1) return "bad_count";
        side->groups[side->groupCount].monster = (int)monster;
        side->groups[side->groupCount].count = count;
        side->groupCount++;
        side->units = side->units > LLONG_MAX - count ? LLONG_MAX : side->units + count;
    }
    return NULL;
}

static int testFieldOptionScanChanged;

static int testFieldHasEditorOption(lua_State *L) {
    static const char *const KEYS[] = {"terrain", "allowMultipleUnique", "emptyEditor", "minimumWidth"};
    testFieldOptionScanChanged = 0;
    if (!lua_istable(L, 1)) return 0;
    int wasOpen = testField.open, wasPlayer = G32(RVA_ACTIVE_PLAYER);
    unsigned long long wasSession = testFieldLife.session;
    int present = 0;
    for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0] && !present; i++) {
        lua_getfield(L, 1, KEYS[i]);
        present = !lua_isnil(L, -1);
        lua_pop(L, 1);
    }
    testFieldOptionScanChanged = testField.open != wasOpen || G32(RVA_ACTIVE_PLAYER) != wasPlayer || testFieldLife.session != wasSession;
    return present;
}

static int readTestFieldSwitch(lua_State *L, const char *key, int raw, int *value) {
    lua_pushstring(L, key);
    if (raw) lua_rawget(L, 1);
    else lua_gettable(L, 1);
    int ok = lua_isnil(L, -1) || lua_isboolean(L, -1);
    *value = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return ok;
}

static const char *readTestFieldSpec(lua_State *L, TestFieldSpec *spec) {
    spec->terrain = TEST_BATTLE_TERRAIN;
    spec->monster = spec->spell = spec->standIn = -1;
    if (!lua_istable(L, 1)) {
        spec->monster = luaL_checkinteger(L, 1);
        spec->spell = luaL_optinteger(L, 2, -1);
        lua_Integer enemy = luaL_checkinteger(L, 3), count = luaL_optinteger(L, 4, 40);
        spec->standIn = luaL_optinteger(L, 5, -1);
        if (!isUsedMonster(spec->monster)) return "bad_monster";
        if (!isUsedMonster(enemy)) return "bad_enemy";
        if (count < 1) return "bad_count";
        spec->enemies.groups = calloc(1, sizeof *spec->enemies.groups);
        if (!spec->enemies.groups) return "out_of_memory";
        spec->enemies.groups[0].monster = (int)enemy;
        spec->enemies.groups[0].count = count;
        spec->enemies.groupCount = 1;
        spec->enemies.units = count;
        return NULL;
    }
    const char *reason = readTestFieldTerrainArg(L, 1, G32(RVA_LAST_TERRAIN), TERRAIN_TABLE_MAX, &spec->terrain, &spec->terrainSpecified);
    if (reason) return reason;
    if (!readTestFieldSwitch(L, "allowMultipleUnique", 0, &spec->allowMultipleUnique)) return "bad_allow_multiple_unique";
    if (!readTestFieldSwitch(L, "emptyEditor", 1, &spec->emptyEditor)) return "bad_empty_editor";
    lua_pushliteral(L, "minimumWidth");
    lua_rawget(L, 1);
    if (!lua_isnil(L, -1)) {
        lua_Integer width = lua_tointeger(L, -1);
        int valid = lua_isinteger(L, -1) && width >= TEST_FIELD_MIN_WIDTH && width <= TEST_FIELD_MAX_WIDTH;
        lua_pop(L, 1);
        if (!valid) return "bad_minimum_width";
        spec->minimumWidth = (int)width;
    } else lua_pop(L, 1);
    if (spec->terrainSpecified) {
        reason = testFieldTerrainProblem(L, (int)spec->terrain);
        if (reason) return reason;
    }
    int isNumber = 0;
    lua_getfield(L, 1, "commander");
    int commanderGiven = !lua_isnil(L, -1);
    if (!spec->emptyEditor) spec->monster = lua_tointegerx(L, -1, &isNumber);
    lua_pop(L, 1);
    if (spec->emptyEditor && commanderGiven) return "empty_editor_commander";
    if (!spec->emptyEditor && (!isNumber || !isUsedMonster(spec->monster))) return "bad_monster";
    lua_getfield(L, 1, "spell");
    spec->spell = lua_isnil(L, -1) ? (isNumber = 1, -1) : lua_tointegerx(L, -1, &isNumber);
    lua_pop(L, 1);
    if (!isNumber) return "bad_spell";
    lua_getfield(L, 1, "standIn");
    spec->standIn = lua_isnil(L, -1) ? (isNumber = 1, -1) : lua_tointegerx(L, -1, &isNumber);
    lua_pop(L, 1);
    if (!isNumber) return "bad_weapon";
    if (spec->emptyEditor && (spec->spell != -1 || spec->standIn != -1)) return "empty_editor_primary_attack";
    lua_getfield(L, 1, "friends");
    if (!lua_isnil(L, -1)) reason = readTestFieldGroups(L, lua_gettop(L), &spec->friends, "bad_friend");
    lua_pop(L, 1);
    if (reason) return reason;
    lua_getfield(L, 1, "enemies");
    if (!spec->emptyEditor || !lua_isnil(L, -1)) reason = readTestFieldGroups(L, lua_gettop(L), &spec->enemies, "bad_enemy");
    lua_pop(L, 1);
    if (reason) return reason;
    return spec->emptyEditor || spec->enemies.units ? NULL : "no_enemies";
}

static int makeTestFieldGroups(int player, int commander, const TestFieldSide *side, int *made) {
    int n = 0;
    for (int g = 0; g < side->groupCount; g++)
        for (long long i = 0; i < side->groups[g].count; i++) {
            int unit = n < UNIT_TABLE_MAX ? GAME_FN(int (*)(int, int, int, int, int), RVA_NEW_UNIT)(testField.x, testField.y, player, commander,
                                                                                                      side->groups[g].monster)
                                          : -1;
            if (unit < 0) {
                for (int k = 0; k < n; k++) *(short *)GAME_PTR(RVA_UNIT_HP + (size_t)made[k] * UNIT_SIZE) = 0;
                return -1;
            }
            made[n++] = unit;
        }
    return n;
}

static void pushTestFieldUnits(lua_State *L, const int *units, int n) {
    lua_createtable(L, n, 0);
    for (int i = 0; i < n; i++) {
        lua_pushinteger(L, units[i]);
        lua_rawseti(L, -2, i + 1);
    }
}

static int testFieldFriendUnits[UNIT_TABLE_MAX], testFieldEnemyUnits[UNIT_TABLE_MAX];
static int testFieldFriendMade, testFieldEnemyMade;

static int refuseTestFieldOpen(const LuaScript *s, const char *call, TestFieldSpec *spec, const char *reason, const char *fmt, ...) {
    char fields[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(fields, sizeof fields, fmt, ap);
    va_end(ap);
    testFieldOpenFailure = reason;
    if (spec) {
        freeTestFieldSide(&spec->friends);
        freeTestFieldSide(&spec->enemies);
        free(spec);
    }
    refuseLuaCall(s->mod, call, reason, "%s", fields);
    return 0;
}

static void undoTestFieldOpen(int editor) {
    if (editor) {
        removeTestFieldUnits();
        turnOffTestFieldLifecycle();
    } else clearTestBattleSquare(testField.x, testField.y);
    memcpy(testFieldSquare(), testField.square, SQUARE_SIZE);
    *(short *)GAME_PTR(RVA_SQUARE_ARMY + ((size_t)testField.x * MAP_COLUMN_SQUARES + testField.y) * 2) = testField.army;
    *(int *)GAME_PTR(RVA_MAP_W) = testField.mapW;
    *(int *)GAME_PTR(RVA_MAP_H) = testField.mapH;
    restoreTestFieldPlane();
}

static int openTestFieldFrom(lua_State *L, const char *call, int editor) {
    if (editor) checkLuaMainThread(L, call);
    testFieldOpenFailure = NULL;
    LuaScript *s = checkLuaScript(L);
    TestFieldSpec *spec = calloc(1, sizeof *spec);
    if (!spec) return luaL_error(L, "out of memory");
    unsigned long long expectedSession = testFieldLife.session;
    const char *reason = editor && !gameCodeMatches   ? "game_code_differs"
                       : editor && testFieldOptionScanChanged ? "context_changed"
                       : G32(RVA_ACTIVE_PLAYER) >= 0 ? "in_game"
                       : testField.open              ? "already_open"
                                                     : readTestFieldSpec(L, spec);
    if (editor && !reason && (testField.open || testFieldLife.session != expectedSession || G32(RVA_ACTIVE_PLAYER) >= 0)) reason = "context_changed";
    if (!editor && !reason && (spec->terrainSpecified || spec->allowMultipleUnique || spec->emptyEditor || spec->minimumWidth)) reason = "context_changed";
    lua_Integer monster = spec->monster, spell = spec->spell, standIn = spec->standIn;
    int spellWeapon = spell >= 0 && spell < spellTableRecords() ? spellSlot((int)spell)->weapon : -1;
    if (!reason)
        reason = spell < -1 || spell >= spellTableRecords()                     ? "bad_spell"
               : standIn >= 0 && (spellWeapon < 0 || spellWeapon >= tableRecords(TABLE_WEAPON)) ? "no_spell_weapon"
               : standIn < -1 || standIn >= tableRecords(TABLE_WEAPON) ? "bad_weapon"
                              : NULL;
    if (!editor && reason)
        return refuseTestFieldOpen(s, call, spec, reason, "monster=%lld spell=%lld friends=%d enemies=%d", (long long)monster, (long long)spell,
                                   spec->friends.groupCount, spec->enemies.groupCount);
    int uniqueMonster = -1, existingUnique = -1;
    long long uniqueRequested = 0;
    if (editor && !reason && (testFieldLife.pending || testFieldLife.failed)) reason = "cleanup_unverified";
    if (editor && !reason && !spec->allowMultipleUnique)
        reason = testFieldUniqueProblem((int)monster, &spec->friends, &spec->enemies, &uniqueMonster, &existingUnique, &uniqueRequested);
    if (editor && !reason && (G32(RVA_UNIT_COUNT) < -1 || G32(RVA_UNIT_COUNT) >= UNIT_TABLE_MAX)) reason = "bad_unit_table";
    for (int u = 0; editor && !reason && u < UNIT_TABLE_MAX; u++)
        if (G16(RVA_UNIT_HP + (size_t)u * UNIT_SIZE) > 0 && G32(RVA_UNIT_X + (size_t)u * UNIT_SIZE) == TEST_BATTLE_X &&
            G32(RVA_UNIT_X + (size_t)u * UNIT_SIZE + 4) == TEST_BATTLE_Y)
            reason = "temporary_square_busy";
    if (!reason && spec->minimumWidth) {
#ifdef __APPLE__
        reason = "minimum_width_unavailable";
#else
        if (!installBattleMapHooks()) reason = "minimum_width_unavailable";
#endif
    }
    if (reason)
        return refuseTestFieldOpen(s, call, spec, reason, "monster=%lld spell=%lld friends=%d enemies=%d unique_monster=%d existing_unit=%d requested=%lld",
                                   (long long)monster, (long long)spell, spec->friends.groupCount, spec->enemies.groupCount, uniqueMonster, existingUnique,
                                   uniqueRequested);
    if (editor && !beginTestFieldLife(s->mod, spec->allowMultipleUnique)) return refuseTestFieldOpen(s, call, spec, "lifecycle_unavailable", "field=unchanged");
    if (editor) resetTestFieldFinal();
    int terrain = (int)spec->terrain, explicitTerrain = spec->terrainSpecified;
    int x = testField.x = TEST_BATTLE_X, y = testField.y = TEST_BATTLE_Y;
    testField.allowMultipleUnique = spec->allowMultipleUnique;
    testField.emptyEditor = spec->emptyEditor;
    testField.terrain = terrain;
    testField.minimumWidth = spec->minimumWidth;
    testField.neighbours = hashTestFieldNeighbours(x, y);
    testField.mapW = G32(RVA_MAP_W);
    testField.mapH = G32(RVA_MAP_H);
    testField.planeBorrowed = explicitTerrain;
    if (explicitTerrain) {
        memcpy(testField.plane, planeRecord(0), PLANE_SIZE);
        testField.lastPlane = G32(RVA_LAST_PLANE);
    }
    short *army = (short *)GAME_PTR(RVA_SQUARE_ARMY + ((size_t)x * MAP_COLUMN_SQUARES + y) * 2);
    short *teamA = (short *)GAME_PTR(RVA_SLOT_TEAM), *teamB = (short *)GAME_PTR(RVA_SLOT_TEAM + (size_t)TEST_BATTLE_ENEMY_PLAYER * PLAYER_BLOCK_SIZE);
    testField.army = *army;
    testField.teamA = *teamA;
    testField.teamB = *teamB;
    testField.activePlayer = G32(RVA_ACTIVE_PLAYER);
    testField.delay = G32(RVA_BATTLE_DELAY);
    testField.showFlag = testFieldShowFlag();
    unsigned char *square = testFieldSquare();
    memcpy(testField.square, square, SQUARE_SIZE);
    memset(square, 0, SQUARE_SIZE);
    *(short *)(square + (RVA_SQUARES - RVA_SQUARE_RECORDS)) = (short)terrain;
    if (!editor) clearTestBattleSquare(x, y);
    if (explicitTerrain) {
        if (testField.mapW <= x + 1) *(int *)GAME_PTR(RVA_MAP_W) = x + 2;
        if (testField.mapH <= y + 1) *(int *)GAME_PTR(RVA_MAP_H) = y + 2;
        setPlaneField(0, PLANE_X, 0);
        setPlaneField(0, PLANE_Y, 0);
        setPlaneField(0, PLANE_W, x + 2);
        setPlaneField(0, PLANE_H, y + 2);
        if (testField.lastPlane < 0) *(int *)GAME_PTR(RVA_LAST_PLANE) = 0;
        int plane = GAME_FN(int (*)(int, int), RVA_PLANE_AT)(x, y);
        if (plane == 0) {
            GAME_FN(void (*)(int, int, int, int), RVA_GEN_SET_FIXED_TEMPERATURE)(x, y, 1, 1);
            GAME_FN(void (*)(int, int, int, int, int), RVA_SET_GROUND_RECT)(x, y, 1, 1, -1);
        }
        short ground = *(short *)(square + SQUARE_GROUND);
        if (plane != 0 || ground < 0 || ground > SQUARE_GROUND_LAST) {
            undoTestFieldOpen(editor);
            return refuseTestFieldOpen(s, call, spec, "terrain_init_failed", "terrain=%d ground=%d plane=%d", terrain, ground, plane);
        }
    }
    enterTestFieldCode();
    int commander = -1, unit = -1;
    if (!spec->emptyEditor) {
        commander = GAME_FN(int (*)(int, int, int, int), RVA_NEW_COMMANDER)(x, y, 0, (int)monster);
        unit = commander >= 0 && commander <= G32(RVA_LAST_COMMANDER) ? G32(RVA_COMMANDERS + (size_t)commander * COMMANDER_SIZE) : -1;
    }
    testFieldFriendMade = !spec->emptyEditor && unit < 0 ? -1 : makeTestFieldGroups(0, commander, &spec->friends, testFieldFriendUnits);
    testFieldEnemyMade = testFieldFriendMade < 0 ? -1 : makeTestFieldGroups(TEST_BATTLE_ENEMY_PLAYER, -1, &spec->enemies, testFieldEnemyUnits);
    leaveTestFieldCode();
    int friendGroups = spec->friends.groupCount, enemyGroups = spec->enemies.groupCount;
    if (testFieldEnemyMade < 0 || (!testField.emptyEditor && testFieldEnemyMade < 1)) {
        undoTestFieldOpen(editor);
        return refuseTestFieldOpen(s, call, spec, "no_free_slot", "monster=%lld friends=%d enemies=%d", (long long)monster, friendGroups, enemyGroups);
    }
    freeTestFieldSide(&spec->friends);
    freeTestFieldSide(&spec->enemies);
    free(spec);
    testField.open = 1;
    testField.editor = editor;
    snprintf(testField.owner, sizeof testField.owner, "%s", s->mod);
    testField.fighting = 0;
    testField.unit = unit;
    testField.commander = commander;
    testField.spell = (int)spell;
    testField.weapon = testField.emptyEditor ? -1
                     : spell >= 0            ? spellWeapon
                                             : *(const int *)(monsterRecord((int)monster) + MONSTER_ATTACKS + MONSTER_ATTACK_WEAPON);
    *teamA = 100;
    *teamB = 100 + TEST_BATTLE_ENEMY_PLAYER;
    *(short *)(square + SQUARE_ARMY_OWNER) = 0;
    *(short *)(square + SQUARE_HOLDER) = TEST_BATTLE_ENEMY_PLAYER;
    *army = -1;
    if (spell >= 0) {
        GAME_FN(int (*)(int, int), RVA_ADD_KNOWN_SPELL)(unit, (int)spell);
        short *memorized = (short *)GAME_PTR(RVA_UNIT_MEMORIZED_SPELLS + (size_t)unit * UNIT_SIZE);
        memorized[0] = (short)spell;
        memorized[1] = -1;
    }
    if (testField.mapW <= x + 1) *(int *)GAME_PTR(RVA_MAP_W) = x + 2;
    if (testField.mapH <= y + 1) *(int *)GAME_PTR(RVA_MAP_H) = y + 2;
    setTestFieldShowFlag(1);
    *(int *)GAME_PTR(RVA_ACTIVE_PLAYER) = 0;
    *(int *)GAME_PTR(RVA_BATTLE_DELAY) = TEST_BATTLE_DELAY;
    *(int *)GAME_PTR(RVA_BATTLE_X) = x;
    *(int *)GAME_PTR(RVA_BATTLE_Y) = y;
    *(int *)GAME_PTR(RVA_BATTLE_PLAYER_A) = 0;
    *(int *)GAME_PTR(RVA_BATTLE_PLAYER_B) = TEST_BATTLE_ENEMY_PLAYER;
    testField.spellWeaponRecord = NULL;
    testField.standIn = (int)standIn;
    if (standIn >= 0) {
        testField.spellWeaponRecord = tableAddress(TABLE_WEAPON) + (size_t)spellWeapon * WEAPON_SIZE;
        memcpy(testField.spellWeaponSaved, testField.spellWeaponRecord, WEAPON_SIZE);
        memcpy(testField.spellWeaponRecord + WEAPON_NAME_MAX + 1, tableAddress(TABLE_WEAPON) + (size_t)standIn * WEAPON_SIZE + WEAPON_NAME_MAX + 1,
               WEAPON_SIZE - WEAPON_NAME_MAX - 1);
    }
    writeLog("test_field_opened", "mod=%s monster=%lld spell=%lld friends=%d enemies=%d terrain=%d explicit_terrain=%d ground=%d empty_editor=%d editor=%d", s->mod,
             (long long)monster, (long long)spell, testFieldFriendMade, testFieldEnemyMade, terrain, explicitTerrain, *(short *)(square + SQUARE_GROUND),
             testField.emptyEditor, editor);
    enterTestFieldCode();
    testField.sizing = testField.minimumWidth > 0;
    GAME_FN(void (*)(int, int, int), RVA_BATTLE_SETUP)(x, y, 0);
    testField.sizing = 0;
    int readyWidth = G32(RVA_BATTLE_WIDTH);
    if (testField.minimumWidth && (readyWidth < testField.minimumWidth || readyWidth > TEST_FIELD_MAX_WIDTH)) {
        leaveTestFieldCode();
        int cleaned = closeTestFieldNow();
        return refuseTestFieldOpen(s, call, NULL, cleaned ? "minimum_width_not_applied" : "cleanup_unverified", "width=%d", readyWidth);
    }
    GAME_FN(void (*)(int, int, int), RVA_BATTLE_DEPLOY)(-1, x, y);
    leaveTestFieldCode();
    return 1;
}

static int lOpenTestField(lua_State *L) {
    int editor = testFieldHasEditorOption(L);
    if (!openTestFieldFrom(L, "coe5.openTestField", editor)) {
        lua_pushboolean(L, 0);
        if (!editor) return 1;
        lua_pushstring(L, testFieldOpenFailure ? testFieldOpenFailure : "open_failed");
        return 2;
    }
    lua_pushboolean(L, 1);
    lua_createtable(L, 0, 4);
    if (editor) setBoolField(L, "emptyEditor", testField.emptyEditor);
    lua_pushinteger(L, testField.unit);
    lua_setfield(L, -2, "commander");
    pushTestFieldUnits(L, testFieldFriendUnits, testFieldFriendMade);
    lua_setfield(L, -2, "friends");
    pushTestFieldUnits(L, testFieldEnemyUnits, testFieldEnemyMade);
    lua_setfield(L, -2, "enemies");
    return 2;
}

static int testFieldCallIsEditor(lua_State *L) {
    if (testField.open) return testField.editor;
    return testField.editor && !_stricmp(checkLuaScript(L)->mod, testFieldLife.owner);
}

static int testFieldIsOpen(lua_State *L, const char *call) {
    if (testField.open && !testFieldLife.retiring) return 1;
    LuaScript *s = checkLuaScript(L);
    refuseLuaCall(s->mod, call, testFieldLife.retiring ? "retirement_in_progress" : "not_open", "available=0");
    return 0;
}

static int refuseTestFieldCall(lua_State *L, const char *call, const char *reason, int nilResult, const char *fmt, ...) {
    char fields[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(fields, sizeof fields, fmt, ap);
    va_end(ap);
    refuseLuaCall(checkLuaScript(L)->mod, call, reason, "%s", fields);
    if (nilResult) lua_pushnil(L);
    else lua_pushboolean(L, 0);
    lua_pushstring(L, reason);
    return 2;
}

static const char *testFieldFightProblem(const LuaScript *script, int *teamA, int *teamB) {
    *teamA = *teamB = 0;
    if (!gameCodeMatches) return "game_code_differs";
    if (!testField.open) return "not_open";
    if (_stricmp(script->mod, testField.owner)) return "not_owner";
    if (testField.fighting) return "in_fight";
    if (testFieldLife.retiring) return "retirement_in_progress";
    if (testFieldFinalViewActive()) return "final_read_only";
    if (testFieldLife.pending || testFieldLife.failed || !testFieldLife.active) return "cleanup_unverified";
    int width = G32(RVA_BATTLE_WIDTH), last = G32(RVA_UNIT_COUNT);
    if (width < 1 || width > BATTLE_COLUMNS_MAX) return "bad_field";
    if (last < -1 || last >= UNIT_TABLE_MAX) return "bad_unit_table";
    unsigned char *seen = calloc(UNIT_TABLE_MAX, 1);
    if (!seen) return "out_of_memory";
    const char *reason = NULL;
    for (int x = 0; !reason && x < width; x++)
        for (int y = 0; !reason && y < BATTLE_ROWS; y++) {
            int unit = *(const int *)battleCell(x, y);
            if (unit == -1) continue;
            if (unit < 0 || unit > last || unit >= UNIT_TABLE_MAX || !isTestFieldActor(unit)) {
                reason = "not_on_field";
                break;
            }
            if (seen[unit]) {
                reason = "ambiguous_token";
                break;
            }
            seen[unit] = 1;
            size_t at = (size_t)unit * UNIT_SIZE;
            int player = G16(RVA_UNIT_PLAYER + at), monster = G16(RVA_UNIT_MONSTER + at);
            if (G16(RVA_UNIT_HP + at) <= 0) reason = "dead_unit";
            else if (!isUsedMonster(monster)) reason = "bad_monster";
            else if (G32(RVA_UNIT_X + at) != testField.x || G32(RVA_UNIT_X + at + 4) != testField.y) reason = "not_on_field";
            else if (player == 0) (*teamA)++;
            else if (player == TEST_BATTLE_ENEMY_PLAYER) (*teamB)++;
            else reason = "not_on_field";
        }
    for (int unit = 0; !reason && unit < UNIT_TABLE_MAX; unit++)
        if (isTestFieldActor(unit) && G16(RVA_UNIT_HP + (size_t)unit * UNIT_SIZE) > 0 && !seen[unit]) reason = "not_on_field";
    free(seen);
    return reason ? reason : !*teamA ? "missing_team_a" : !*teamB ? "missing_team_b" : NULL;
}

static int lDrawTestField(lua_State *L) {
    if (!testFieldIsOpen(L, "coe5.drawTestField")) return 0;
    *(int *)GAME_PTR(RVA_BATTLE_SCREEN_CARRY_US) = G32(RVA_FRAME_DELAY_MS) * 1000;
    GAME_FN(void (*)(int), RVA_BATTLE_SCREEN)(0);
    testFieldDrawSession = testFieldLife.session;
    return 0;
}

static void fightTestFieldNow(void) {
    resetTestFieldText();
    testField.fighting = 1;
    enterTestFieldCode();
    GAME_FN(void (*)(int, int), RVA_FIGHT_THE_FIGHT)(testField.x, testField.y);
    leaveTestFieldCode();
    testField.fighting = 0;
}

static void fireTestFieldNow(void) {
    resetTestFieldText();
    testField.fighting = 1;
    enterTestFieldCode();
    int *range = NULL, oldRange = 0;
    if (testField.weapon >= 0 && testField.weapon < tableRecords(TABLE_WEAPON)) {
        range = (int *)(tableAddress(TABLE_WEAPON) + (size_t)testField.weapon * WEAPON_SIZE + WEAPON_RANGE);
        oldRange = *range;
        if (*range > 1 && *range < TEST_FIELD_FIRE_RANGE) *range = TEST_FIELD_FIRE_RANGE;
    }
    if (testField.spellWeaponRecord && testField.standIn >= 0)
        memcpy(testField.spellWeaponRecord + WEAPON_NAME_MAX + 1, tableAddress(TABLE_WEAPON) + (size_t)testField.standIn * WEAPON_SIZE + WEAPON_NAME_MAX + 1,
               WEAPON_SIZE - WEAPON_NAME_MAX - 1);
    if (testField.spell >= 0) GAME_FN(void (*)(int, int, int), RVA_DOATTACK)(testField.unit, -1, testField.spell);
    else GAME_FN(void (*)(int, int, int), RVA_DOATTACK)(testField.unit, 0, -1);
    GAME_FN(int (*)(int), RVA_FLY_WEAPONS_STEP)(-1);
    if (range) *range = oldRange;
    leaveTestFieldCode();
    testField.fighting = 0;
}

static int lFireTestField(lua_State *L) {
    if (!testFieldCallIsEditor(L)) {
        if (!testFieldIsOpen(L, "coe5.fireTestField")) return 0;
        fireTestFieldNow();
        return 0;
    }
    checkLuaMainThread(L, "coe5.fireTestField");
    LuaScript *script = checkLuaScript(L);
    const char *reason = !gameCodeMatches                        ? "game_code_differs"
                       : !testField.open                         ? "not_open"
                       : _stricmp(script->mod, testField.owner) ? "not_owner"
                       : testField.fighting                      ? "in_fight"
                       : testFieldFinalViewActive()              ? "final_read_only"
                       : testField.emptyEditor                   ? "no_primary_unit"
                                                                 : NULL;
    if (reason) return refuseTestFieldCall(L, "coe5.fireTestField", reason, 0, "field=unchanged");
    fireTestFieldNow();
    return 0;
}

static int lFightTestField(lua_State *L) {
    if (!testFieldCallIsEditor(L)) {
        if (!testFieldIsOpen(L, "coe5.fightTestField")) return 0;
        fightTestFieldNow();
    } else {
        checkLuaMainThread(L, "coe5.fightTestField");
        int teamA = 0, teamB = 0;
        const char *reason = testFieldFightProblem(checkLuaScript(L), &teamA, &teamB);
        if (reason) return refuseTestFieldCall(L, "coe5.fightTestField", reason, 0, "team_a=%d team_b=%d field=unchanged", teamA, teamB);
        beginTestFieldFinal();
        fightTestFieldNow();
        finishTestFieldFinal();
    }
    *(int *)GAME_PTR(RVA_BATTLE_DELAY) = TEST_BATTLE_DELAY;
    setTestFieldShowFlag(1);
    setTestFieldBattleOver(0);
    return 0;
}

static int lStopTestFight(lua_State *L) {
    if (!testFieldIsOpen(L, "coe5.stopTestFight")) return 0;
    *(int *)GAME_PTR(RVA_BATTLE_DELAY) = 0;
    setTestFieldShowFlag(0);
    setTestFieldBattleOver(1);
    return 0;
}

static int lCloseTestField(lua_State *L) {
    if (!testFieldCallIsEditor(L)) {
        closeTestFieldNow();
        return 0;
    }
    checkLuaMainThread(L, "coe5.closeTestField");
    LuaScript *script = checkLuaScript(L);
    const char *reason = testFieldLife.session && _stricmp(script->mod, testFieldLife.owner) ? "not_owner" :
                         testFieldLife.retiring ? "retirement_in_progress" : NULL;
    int ok = !reason && closeTestFieldNow();
    if (!ok && !reason) reason = "cleanup_unverified";
    if (reason) refuseLuaCall(script->mod, "coe5.closeTestField", reason, "session=%llu", testFieldLife.session);
    lua_pushboolean(L, ok);
    if (reason) lua_pushstring(L, reason);
    else lua_pushnil(L);
    return 2;
}

static int lAddTestFieldUnits(lua_State *L) {
    int editor = testFieldCallIsEditor(L);
    if (editor) checkLuaMainThread(L, "coe5.addTestFieldUnits");
    LuaScript *s = checkLuaScript(L);
    unsigned long long expectedSession = testFieldLife.session;
    const char *side = luaL_checkstring(L, 1);
    int friends = !strcmp(side, "friends"), enemies = !strcmp(side, "enemies");
    int player = friends ? 0 : TEST_BATTLE_ENEMY_PLAYER;
    TestFieldSide groups = {0};
    const char *reason = testFieldLife.retiring                       ? "retirement_in_progress"
                       : editor && !gameCodeMatches                   ? "game_code_differs"
                       : !testField.open                              ? "not_open"
                       : editor && _stricmp(s->mod, testField.owner) ? "not_owner"
                       : editor && testField.fighting                 ? "in_fight"
                       : editor && testFieldFinalViewActive()         ? "final_read_only"
                       : !friends && !enemies                         ? "bad_side"
                                                                      : readTestFieldGroups(L, 2, &groups, friends ? "bad_friend" : "bad_enemy");
    if (editor && !reason &&
        (!testField.open || testFieldLife.session != expectedSession || _stricmp(s->mod, testField.owner) || testField.fighting || testFieldFinalViewActive()))
        reason = "context_changed";
    if (!reason && !groups.units) reason = "bad_count";
    int uniqueMonster = -1, existingUnique = -1;
    long long uniqueRequested = 0;
    if (editor && !reason && (testFieldLife.pending || testFieldLife.failed)) reason = "cleanup_unverified";
    if (editor && !reason && !testField.allowMultipleUnique)
        reason = testFieldUniqueProblem(-1, &groups, NULL, &uniqueMonster, &existingUnique, &uniqueRequested);
    int *made = friends ? testFieldFriendUnits : testFieldEnemyUnits;
    enterTestFieldCode();
    int n = reason ? 0 : makeTestFieldGroups(player, friends ? testField.commander : -1, &groups, made);
    leaveTestFieldCode();
    if (!reason && n < 0) reason = "no_free_slot";
    int groupCount = groups.groupCount;
    long long units = groups.units;
    freeTestFieldSide(&groups);
    if (reason && !editor) {
        refuseLuaCall(s->mod, "coe5.addTestFieldUnits", reason, "side=%.20s groups=%d units=%lld", side, groupCount, units);
        lua_pushnil(L);
        return 1;
    }
    if (reason)
        return refuseTestFieldCall(L, "coe5.addTestFieldUnits", reason, 1, "side=%.20s groups=%d units=%lld unique_monster=%d existing_unit=%d requested=%lld",
                                   side, groupCount, units, uniqueMonster, existingUnique, uniqueRequested);
    if (friends) testFieldFriendMade = n;
    else testFieldEnemyMade = n;
    enterTestFieldCode();
    ((void (*)(int, int, int, int, int))siteEntry(SITE_BATTLE_SETUP_SIDE, GAME_PTR(RVA_BATTLE_SETUP_SIDE)))(player, testField.x, testField.y,
                                                                                                          friends ? 0 : 1, -1);
    leaveTestFieldCode();
    writeLog("test_field_units_added", "mod=%s side=%s units=%d", s->mod, side, n);
    pushTestFieldUnits(L, made, n);
    return 1;
}

static int testFieldTokenCovering(int x, int y, int layer, int self, int strict) {
    for (int ax = x - 2; ax <= x; ax++)
        for (int ay = y - 2; ay <= y; ay++) {
            if (ax < 0 || ay < 0) continue;
            int u = *(const int *)(battleCell(ax, ay) + layer);
            if (u == -1 || u == self) continue;
            if (!strict && (u < 0 || u >= UNIT_TABLE_MAX)) continue;
            if (strict) {
                if (u < -1 || u >= UNIT_TABLE_MAX || u > G32(RVA_UNIT_COUNT)) return INT_MAX;
                int monster = G16(RVA_UNIT_MONSTER + (size_t)u * UNIT_SIZE);
                if (monster < 0 || monster >= MONSTER_TABLE_MAX || !isUsedMonster(monster)) return INT_MAX;
            }
            int w = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)(u), h = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)(u);
            if (ax + w > x && ay + h > y) return u;
        }
    return -1;
}

static int lMoveTestFieldUnit(lua_State *L) {
    int editor = testFieldCallIsEditor(L);
    if (editor) checkLuaMainThread(L, "coe5.moveTestFieldUnit");
    LuaScript *s = checkLuaScript(L);
    lua_Integer unit = luaL_checkinteger(L, 1), x = luaL_checkinteger(L, 2), y = luaL_checkinteger(L, 3);
    int fromX = -1, fromY = -1;
    const char *reason = NULL;
    if (testFieldLife.retiring) return refuseTestFieldCall(L, "coe5.moveTestFieldUnit", "retirement_in_progress", 0, "field=unchanged");
    if (!editor) {
        reason = !testField.open ? "not_open" : unit < 0 || unit >= UNIT_TABLE_MAX || !unitBattleCell((int)unit, &fromX, &fromY) ? "not_on_field" : NULL;
    } else {
        reason = !gameCodeMatches                                                ? "game_code_differs"
               : !testField.open                                                 ? "not_open"
               : _stricmp(s->mod, testField.owner)                              ? "not_owner"
               : testField.fighting                                              ? "in_fight"
               : testFieldFinalViewActive()                                      ? "final_read_only"
               : unit < 0 || unit >= UNIT_TABLE_MAX || unit > G32(RVA_UNIT_COUNT) ? "bad_unit"
                                                                                 : NULL;
        if (!reason && !isTestFieldActor((int)unit)) reason = "not_on_field";
        if (!reason && (G32(RVA_BATTLE_WIDTH) < 1 || G32(RVA_BATTLE_WIDTH) > BATTLE_COLUMNS_MAX)) reason = "bad_field";
        if (!reason) {
            size_t at = (size_t)unit * UNIT_SIZE;
            int player = G16(RVA_UNIT_PLAYER + at), monster = G16(RVA_UNIT_MONSTER + at);
            if (G16(RVA_UNIT_HP + at) <= 0) reason = "dead_unit";
            else if (!isUsedMonster(monster)) reason = "bad_monster";
            else if (G32(RVA_UNIT_X + at) != testField.x || G32(RVA_UNIT_X + at + 4) != testField.y || (player != 0 && player != TEST_BATTLE_ENEMY_PLAYER) ||
                     !unitBattleCell((int)unit, &fromX, &fromY))
                reason = "not_on_field";
        }
    }
    int w = 1, h = 1;
    if (!reason) {
        w = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE_2)((int)unit);
        h = GAME_FN(int (*)(int), RVA_UNIT_BATTLE_SIZE)((int)unit);
        if (x < 0 || y < 0 || x + w > G32(RVA_BATTLE_WIDTH) || y + h > BATTLE_ROWS) reason = "off_field";
    }
    for (int cx = (int)x; !reason && cx < x + w; cx++)
        for (int cy = (int)y; !reason && cy < y + h; cy++)
            if (testFieldTokenCovering(cx, cy, 0, (int)unit, editor) >= 0 || testFieldTokenCovering(cx, cy, BATTLE_CELL_OBJECT, (int)unit, editor) >= 0)
                reason = "blocked";
    if (reason && !editor) {
        refuseLuaCall(s->mod, "coe5.moveTestFieldUnit", reason, "unit=%lld x=%lld y=%lld", (long long)unit, (long long)x, (long long)y);
        lua_pushboolean(L, 0);
        return 1;
    }
    if (reason) return refuseTestFieldCall(L, "coe5.moveTestFieldUnit", reason, 0, "unit=%lld x=%lld y=%lld", (long long)unit, (long long)x, (long long)y);
    *(int *)battleCell(fromX, fromY) = -1;
    *(int *)battleCell((int)x, (int)y) = (int)unit;
    lua_pushboolean(L, 1);
    return 1;
}

static int lTestBattle(lua_State *L) {
    int editor = testFieldHasEditorOption(L);
    int opened = openTestFieldFrom(L, "coe5.testBattle", editor);
    if (!editor) {
        if (opened) {
            fightTestFieldNow();
            closeTestFieldNow();
        }
        lua_pushboolean(L, opened);
        return 1;
    }
    const char *reason = NULL;
    if (opened) {
        int teamA = 0, teamB = 0;
        reason = testFieldFightProblem(checkLuaScript(L), &teamA, &teamB);
        if (reason) refuseLuaCall(runningLuaMod(), "coe5.testBattle", reason, "team_a=%d team_b=%d", teamA, teamB);
        else {
            beginTestFieldFinal();
            fightTestFieldNow();
            finishTestFieldFinal();
        }
        closeTestFieldNow();
    }
    lua_pushboolean(L, opened && !reason);
    if (!reason) return 1;
    lua_pushstring(L, reason);
    return 2;
}

static int lSpellInfo(lua_State *L) {
    lua_Integer spell = luaL_checkinteger(L, 1);
    if (spell < 0 || spell >= spellTableRecords()) {
        lua_pushnil(L);
        return 1;
    }
    const SpellRecord *r = spellSlot((int)spell);
    if (!r->name || !r->name[0] || r->path < 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 5);
    lua_pushstring(L, r->name);
    lua_setfield(L, -2, "name");
    lua_pushinteger(L, r->path);
    lua_setfield(L, -2, "path");
    lua_pushinteger(L, r->level);
    lua_setfield(L, -2, "level");
    lua_pushinteger(L, r->weapon);
    lua_setfield(L, -2, "weapon");
    lua_pushinteger(L, r->dmg);
    lua_setfield(L, -2, "dmg");
    return 1;
}

static int lLookName(lua_State *L) {
    lua_Integer look = luaL_checkinteger(L, 1);
    if (look < 0 || look >= VISUAL_FX_ROWS) {
        lua_pushnil(L);
        return 1;
    }
    const char *name = *(const char *const *)GAME_PTR(RVA_VISUAL_FX_TABLE + (size_t)look * VISUAL_FX_SIZE);
    if (name && name[0]) lua_pushstring(L, name);
    else lua_pushnil(L);
    return 1;
}
