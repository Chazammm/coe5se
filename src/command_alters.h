#pragma once

typedef enum { ALTER_COMMAND, ALTER_EVENT_TRIGGER, ALTER_EVENT_ACTION, ALTER_COMMAND_AFTER, ALTER_KINDS } CommandAlterKind;
static const char *const COMMAND_ALTER_CALLS[ALTER_KINDS] = {"coe5.alterCommand", "coe5.alterEventTrigger", "coe5.alterEventAction", "coe5.alterCommandAfter"};

typedef struct {
    char word[64];
    CommandAlterKind kind;
    int lua;
    int badResultLogged;
} CommandAlter;
static CommandAlter *commandAlters;
static int commandAlterCount, commandAlterCapacity;

static const char *const PARSER_LEVEL1_WORDS[] = {"icon", "description", "coeversion", "modversion", "modprio"};
static const char *const PARSER_LEVEL4_WORDS[] = {"spr1", "spr2", "spr", "sample"};

static int wordInList(const char *const *list, size_t count, const char *word) {
    for (size_t i = 0; i < count; i++)
        if (!strcmp(list[i], word)) return 1;
    return 0;
}
#define WORD_IN(list, word) wordInList(list, sizeof list / sizeof list[0], word)

static const char *readAlterWord(const char *line, int event, char word[64], int *sign) {
    while (*line == ' ' || *line == '\t') line++;
    *sign = 0;
    if (event && (*line == '+' || *line == '-')) *sign = *line++ == '+' ? 1 : -1;
    size_t n = strcspn(line, " \t\r\n");
    if (n >= 64) n = 0;
    for (size_t i = 0; i < n; i++) word[i] = line[i] >= 'A' && line[i] <= 'Z' ? (char)(line[i] - 'A' + 'a') : line[i];
    word[n] = '\0';
    line += strcspn(line, " \t\r\n");
    while (*line == ' ' || *line == '\t') line++;
    return line;
}

static int commandAltered(CommandAlterKind kind, const char *word) {
    for (int i = 0; i < commandAlterCount; i++)
        if (commandAlters[i].kind == kind && !strcmp(commandAlters[i].word, word)) return 1;
    return 0;
}

// ---- Registering an alter ------------------------------------------------------------------------
static const char *commandAlterNameProblem(CommandAlterKind kind, const char *name, char word[64]) {
    if (name[0] == '+' || name[0] == '-') name++;
    size_t n = strlen(name);
    if (!n || n >= 64) return "not_a_game_command";
    for (size_t i = 0; i <= n; i++) word[i] = name[i] >= 'A' && name[i] <= 'Z' ? (char)(name[i] - 'A' + 'a') : name[i];
    if (!WORD_IN(GAME_C5M_WORDS, word)) return "not_a_game_command";
    if (kind == ALTER_EVENT_TRIGGER && !WORD_IN(GAME_EVENT_TRIGGERS, word)) return "not_a_trigger";
    if (kind == ALTER_EVENT_ACTION && !WORD_IN(GAME_EVENT_ACTIONS, word)) return "not_an_action";
    if (kind == ALTER_COMMAND_AFTER && !WORD_IN(GAME_PARSER_WORDS, word)) return "not_a_content_command";
    return NULL;
}

static int addCommandAlter(lua_State *L, CommandAlterKind kind) {
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    const char *call = COMMAND_ALTER_CALLS[kind];
    if (refuseFromDeadLuaScript(L, call)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    char word[64] = "";
    const char *reason = !luaCommandsLoading ? "not_in_commands_file" : commandAlterNameProblem(kind, name, word);
    int callback = -1;
    if (!reason && (callback = newLuaCallback(L, 2, 0)) < 0) reason = "out_of_memory";
    if (!reason && !growList((void **)&commandAlters, &commandAlterCapacity, commandAlterCount + 1, sizeof commandAlters[0])) reason = "out_of_memory";
    if (reason) {
        refuseLuaCall(runningLuaMod(), call, reason, "command=%.60s", name);
        lua_pushboolean(L, 0);
        return 1;
    }
    CommandAlter *f = &commandAlters[commandAlterCount++];
    memset(f, 0, sizeof *f);
    snprintf(f->word, sizeof f->word, "%s", word);
    f->kind = kind;
    f->lua = callback;
    writeLog("command_alter_added", "mod=%s call=%s command=%s", runningLuaMod() ? runningLuaMod() : "?", call, word);
    lua_pushboolean(L, 1);
    return 1;
}
static int lAlterCommand(lua_State *L) { return addCommandAlter(L, ALTER_COMMAND); }
static int lAlterEventTrigger(lua_State *L) { return addCommandAlter(L, ALTER_EVENT_TRIGGER); }
static int lAlterEventAction(lua_State *L) { return addCommandAlter(L, ALTER_EVENT_ACTION); }
static int lAlterCommandAfter(lua_State *L) { return addCommandAlter(L, ALTER_COMMAND_AFTER); }

static void dropCommandAltersOf(const LuaScript *s) {
    int kept = 0;
    for (int i = 0; i < commandAlterCount; i++) {
        int lua = commandAlters[i].lua;
        if (lua >= 0 && lua < luaCallbackCount && luaCallbacks[lua].s == s) continue;
        commandAlters[kept++] = commandAlters[i];
    }
    commandAlterCount = kept;
}

// ---- Running an alter ----------------------------------------------------------------------------
typedef struct {
    int ref;
    const char *word, *line, *args, *mod;
    int level, event, sign, player, x, y, squareEvent, after;
} CommandAlterCall;

static void setAlterField(lua_State *L, const char *key, lua_Integer value) {
    lua_pushinteger(L, value);
    lua_setfield(L, -2, key);
}
static int pushCommandAlter(lua_State *L, void *ctx) {
    const CommandAlterCall *c = ctx;
    lua_rawgeti(L, LUA_REGISTRYINDEX, c->ref);
    lua_createtable(L, 0, 16);
    lua_pushstring(L, c->word);
    lua_setfield(L, -2, "command");
    lua_pushstring(L, c->line);
    lua_setfield(L, -2, "line");
    lua_pushstring(L, c->args);
    lua_setfield(L, -2, "args");
    if (c->mod && c->mod[0]) {
        lua_pushstring(L, c->mod);
        lua_setfield(L, -2, "mod");
    }
    setAlterField(L, "level", c->level);
    if (c->event) {
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "event");
        lua_pushboolean(L, c->squareEvent);
        lua_setfield(L, -2, "squareEvent");
        setAlterField(L, "player", c->player);
        setAlterField(L, "x", c->x);
        setAlterField(L, "y", c->y);
        if (c->sign) setAlterField(L, "sign", c->sign);
    }
    if (c->after) {
        setAlterField(L, "monster", G32(RVA_MONSTER_CURRENT));
        setAlterField(L, "weapon", G32(RVA_WEAPON_CURRENT));
        setAlterField(L, "item", G32(RVA_ITEM_CURRENT));
        setAlterField(L, "class", G32(RVA_CLASS_CURRENT));
        setAlterField(L, "terrain", G32(RVA_TERRAIN_CURRENT));
        setAlterField(L, "ritual", G32(RVA_RITUAL_CURRENT));
    }
    return 1;
}

static int runCommandAlter(CommandAlter *f, CommandAlterCall *c, int *boolean, char **text, int *integer) {
    if (f->lua < 0 || f->lua >= luaCallbackCount) return -1;
    LuaCallback cb = luaCallbacks[f->lua];
    if (!cb.s || cb.s->dead) return -1;
    NetRole role = netRoleNow();
    if (role != NET_ROLE_SINGLE && !commandAlterOpenFor(role, modClassNow(cb.s->mod))) return -1;
    c->ref = cb.ref;
    LuaLoadPhase outer = c->event ? luaLoadPhase : beginLuaLoadPhase(LUA_LOAD_PASS);
    int ran = callLuaProtected(cb.s, pushCommandAlter, c, 1);
    if (!c->event) endLuaLoadPhase(outer);
    if (!ran) return -1;
    lua_State *L = cb.s->L;
    int type = lua_type(L, -1);
    if (type == LUA_TBOOLEAN) *boolean = lua_toboolean(L, -1);
    if (integer) *integer = 0;
    if (type == LUA_TNUMBER && integer && lua_isinteger(L, -1) && lua_tointeger(L, -1) >= INT_MIN && lua_tointeger(L, -1) <= INT_MAX) {
        *boolean = (int)lua_tointeger(L, -1);
        *integer = 1;
    }
    if (type == LUA_TSTRING && text) {
        size_t n;
        const char *s = lua_tolstring(L, -1, &n);
        *text = malloc(n + 1);
        if (*text) memcpy(*text, s, n + 1);
    }
    lua_pop(L, 1);
    closeDeadLuaScript(cb.s);
    return type;
}

static void refuseCommandAlterResult(CommandAlter *f, int type) {
    if (f->badResultLogged) return;
    f->badResultLogged = 1;
    LuaScript *s = f->lua >= 0 && f->lua < luaCallbackCount ? luaCallbacks[f->lua].s : NULL;
    refuseLuaCall(s ? s->mod : "?", COMMAND_ALTER_CALLS[f->kind], "bad_result", "command=%s result=%s", f->word,
                  s && s->L ? lua_typename(s->L, type) : "?");
}

#define COMMAND_ALTER_LINE_MAX 50000

static const char *runCommandAlters(const char *line, int event, const char *mod, int level, char **altered) {
    for (int i = 0; i < commandAlterCount; i++) {
        CommandAlter *f = &commandAlters[i];
        if (f->kind != ALTER_COMMAND) continue;
        char word[64];
        int sign;
        const char *args = readAlterWord(line, event, word, &sign);
        if (strcmp(word, f->word)) continue;
        CommandAlterCall c = {0};
        c.word = word, c.line = line, c.args = args, c.mod = mod, c.level = level, c.event = event, c.sign = sign;
        if (event) {
            c.player = G32(RVA_EVENT_PLAYER), c.squareEvent = G32(RVA_EVENT_STATE) == SQUARE_EVENT;
            c.x = G32(RVA_EVENT_TARGET_X), c.y = G32(RVA_EVENT_TARGET_Y);
        }
        int boolean = 1;
        char *text = NULL;
        int type = runCommandAlter(f, &c, &boolean, &text, NULL);
        if (type == -1 || type == LUA_TNIL || (type == LUA_TBOOLEAN && boolean)) continue;
        if (type == LUA_TBOOLEAN) return NULL;
        if (type != LUA_TSTRING) {
            refuseCommandAlterResult(f, type);
            continue;
        }
        if (!text) {
            refuseLuaCall(luaCallbacks[f->lua].s->mod, COMMAND_ALTER_CALLS[f->kind], "out_of_memory", "command=%s", f->word);
            continue;
        }
        if (!text[0]) {
            free(text);
            return NULL;
        }
        size_t n = strlen(text);
        if (n >= COMMAND_ALTER_LINE_MAX || strcspn(text, "\r\n") != n) {
            refuseLuaCall(luaCallbacks[f->lua].s->mod, COMMAND_ALTER_CALLS[f->kind], n >= COMMAND_ALTER_LINE_MAX ? "line_too_long" : "two_lines",
                          "command=%s length=%zu", f->word, n);
            free(text);
            continue;
        }
        free(*altered);
        *altered = text;
        line = text;
    }
    return line;
}

static int contentAlterDue(const char *word, int level) {
    if (level == 2) return 1;
    if (level == 1) return WORD_IN(PARSER_LEVEL1_WORDS, word);
    if (level == 4) return WORD_IN(PARSER_LEVEL4_WORDS, word);
    return 0;
}

static const char *alterContentCommandLine(const char *line, int mod, int level, char **altered) {
    if (!commandAlterCount) return line;
    char word[64];
    int sign;
    readAlterWord(line, 0, word, &sign);
    if (!contentAlterDue(word, level) || !commandAltered(ALTER_COMMAND, word)) return line;
    return runCommandAlters(line, 0, mod >= 0 && mod <= G32(RVA_MOD_LAST_INDEX) ? modName(mod) : NULL, level, altered);
}

static void alterCommandAfterLine(const char *line, int mod, int level) {
    if (!commandAlterCount) return;
    char word[64];
    int sign;
    const char *args = readAlterWord(line, 0, word, &sign);
    if (!contentAlterDue(word, level)) return;
    for (int i = 0; i < commandAlterCount; i++) {
        CommandAlter *f = &commandAlters[i];
        if (f->kind != ALTER_COMMAND_AFTER || strcmp(f->word, word)) continue;
        CommandAlterCall c = {0};
        c.word = word, c.line = line, c.args = args, c.mod = mod >= 0 && mod <= G32(RVA_MOD_LAST_INDEX) ? modName(mod) : NULL;
        c.level = level, c.after = 1;
        int boolean;
        runCommandAlter(f, &c, &boolean, NULL, NULL);
    }
}

static const char *alterEventCommandLine(const char *line, int level, const char *mod, char **altered) {
    if (!commandAlterCount || level != 3) return line;
    char word[64];
    int sign;
    readAlterWord(line, 1, word, &sign);
    if (G32(RVA_EVENT_STATE) < 1 && !WORD_IN(GAME_EVENT_STARTS, word)) return line;
    if (!commandAltered(ALTER_COMMAND, word)) return line;
    return runCommandAlters(line, 1, mod, level, altered);
}

static const char *const UNIT_TRIGGERS[] = {"hascom", "hascomhere", "hasunit", "hasunithere", "hasaffunithere"};

static int answerCommandAltersAt(CommandAlterKind kind, const char *word, CommandAlterCall *c, int *unit) {
    *unit = -1;
    for (int i = 0; i < commandAlterCount; i++) {
        CommandAlter *f = &commandAlters[i];
        if (f->kind != kind || strcmp(f->word, word)) continue;
        int boolean = 0, integer = 0;
        int type = runCommandAlter(f, c, &boolean, NULL, &integer);
        if (type == -1 || type == LUA_TNIL) continue;
        if (kind == ALTER_EVENT_TRIGGER && integer && boolean >= 0 && boolean < UNIT_TABLE_MAX && WORD_IN(UNIT_TRIGGERS, word)) {
            *unit = boolean;
            return 1;
        }
        if (type != LUA_TBOOLEAN) {
            refuseCommandAlterResult(f, type);
            continue;
        }
        if (kind == ALTER_EVENT_TRIGGER) return boolean;
        if (boolean) return 1;
    }
    return -1;
}

typedef struct { int square; unsigned char mark; } AlteredSquare;

static void runAlteredEventLine(const char *line, int level, const char *mod) {
    int state = G32(RVA_EVENT_STATE);
    if (!commandAlterCount || level != 3 || (state != PLAYER_EVENT && state != SQUARE_EVENT)) {
        nextEventLine(line, level);
        eventLogicAfterGameLine(line, level, 0);
        return;
    }
    char word[64];
    int sign;
    const char *args = readAlterWord(line, 1, word, &sign);
    CommandAlterKind kind = sign ? ALTER_EVENT_TRIGGER : ALTER_EVENT_ACTION;
    if (!commandAltered(kind, word)) {
        nextEventLine(line, level);
        eventLogicAfterGameLine(line, level, 0);
        return;
    }
    CommandAlterCall c = {0};
    c.word = word, c.line = line, c.args = args, c.mod = mod, c.level = level, c.event = 1, c.sign = sign;
    c.player = G32(RVA_EVENT_PLAYER), c.squareEvent = state == SQUARE_EVENT;
    int tx = G32(RVA_EVENT_TARGET_X), ty = G32(RVA_EVENT_TARGET_Y), target = tx >= 0 && ty >= 0 ? tx * MAP_COLUMN_SQUARES + ty : -1;
    int *targetUnits = (int *)GAME_PTR(RVA_SQUARE_EVENT_B);
    if (state == PLAYER_EVENT) {
        if (target < 0) {
            nextEventLine(line, level);
            return;
        }
        c.x = tx, c.y = ty;
        int unit;
        int answer = answerCommandAltersAt(kind, word, &c, &unit);
        if (answer < 0) nextEventLine(line, level);
        else if (kind == ALTER_EVENT_TRIGGER && answer != (sign > 0)) G32(RVA_EVENT_STATE) = EVENT_FAILED;
        if (unit >= 0) targetUnits[target] = unit;
        return;
    }
    unsigned char *marks = gameBase + RVA_SQUARE_EVENT_A;
    AlteredSquare *kept = NULL;
    int keptCount = 0, keptCapacity = 0, left = 0;
    int width = G32(RVA_MAP_DEPTH), height = G32(RVA_MAP_H);
    if (width > MAP_COLUMNS_MAX) width = MAP_COLUMNS_MAX;
    if (height > MAP_COLUMN_SQUARES) height = MAP_COLUMN_SQUARES;
    for (int i = 0; i < SQUARE_EVENT_A_SIZE; i++) {
        if (!marks[i] || i / MAP_COLUMN_SQUARES >= width || i % MAP_COLUMN_SQUARES >= height) continue;
        int square = target >= 0 ? target : i;
        c.x = square / MAP_COLUMN_SQUARES, c.y = square % MAP_COLUMN_SQUARES;
        int unit;
        int answer = answerCommandAltersAt(kind, word, &c, &unit);
        if (unit >= 0) targetUnits[square] = unit, noteEventUnitWrite((size_t)square);
        if (answer < 0) {
            left++;
            continue;
        }
        if (kind == ALTER_EVENT_TRIGGER && answer != (sign > 0)) {
            marks[i] = 0;
            continue;
        }
        if (!growList((void **)&kept, &keptCapacity, keptCount + 1, sizeof kept[0])) {
            refuseLuaCall("-", COMMAND_ALTER_CALLS[kind], "out_of_memory", "command=%s", word);
            left++;
            continue;
        }
        kept[keptCount++] = (AlteredSquare){i, marks[i]};
    }
    if (left) {
        for (int k = 0; k < keptCount; k++) marks[kept[k].square] = 0;
        nextEventLine(line, level);
        eventLogicAfterGameLine(line, level, 1);
        if (keptCount && G32(RVA_EVENT_STATE) == EVENT_FAILED) {
            memset(marks, 0, SQUARE_EVENT_A_SIZE);
            G32(RVA_EVENT_STATE) = SQUARE_EVENT;
        }
        for (int k = 0; k < keptCount; k++) marks[kept[k].square] = kept[k].mark;
    } else if (kind == ALTER_EVENT_TRIGGER && !keptCount) {
        G32(RVA_EVENT_STATE) = EVENT_FAILED;
    }
    free(kept);
}
