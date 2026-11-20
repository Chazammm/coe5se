#pragma once

typedef enum { FIX_COMMAND, FIX_TRIGGER, FIX_ACTION, FIX_AFTER, FIX_KINDS } CommandFixKind;
static const char *const COMMAND_FIX_CALLS[FIX_KINDS] = {"coe5.fixCommand", "coe5.fixTrigger", "coe5.fixAction", "coe5.fixAfter"};

typedef struct {
    char word[64];
    CommandFixKind kind;
    int lua;
    int badResultLogged;
} CommandFix;
static CommandFix *commandFixes;
static int commandFixCount, commandFixCapacity;

static const char *const PARSER_LEVEL1_WORDS[] = {"icon", "description", "coeversion", "modversion", "modprio"};
static const char *const PARSER_LEVEL4_WORDS[] = {"spr1", "spr2", "spr", "sample"};

static int wordInList(const char *const *list, size_t count, const char *word) {
    for (size_t i = 0; i < count; i++)
        if (!strcmp(list[i], word)) return 1;
    return 0;
}
#define WORD_IN(list, word) wordInList(list, sizeof list / sizeof list[0], word)

static const char *readFixWord(const char *line, int event, char word[64], int *sign) {
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

static int commandFixed(CommandFixKind kind, const char *word) {
    for (int i = 0; i < commandFixCount; i++)
        if (commandFixes[i].kind == kind && !strcmp(commandFixes[i].word, word)) return 1;
    return 0;
}

// ---- Registering a fix --------------------------------------------------------------------------
static const char *commandFixNameProblem(CommandFixKind kind, const char *name, char word[64]) {
    if (name[0] == '+' || name[0] == '-') name++;
    size_t n = strlen(name);
    if (!n || n >= 64) return "not_a_game_command";
    for (size_t i = 0; i <= n; i++) word[i] = name[i] >= 'A' && name[i] <= 'Z' ? (char)(name[i] - 'A' + 'a') : name[i];
    if (!WORD_IN(GAME_C5M_WORDS, word)) return "not_a_game_command";
    if (kind == FIX_TRIGGER && !WORD_IN(GAME_EVENT_TRIGGERS, word)) return "not_a_trigger";
    if (kind == FIX_ACTION && !WORD_IN(GAME_EVENT_ACTIONS, word)) return "not_an_action";
    if (kind == FIX_AFTER && !WORD_IN(GAME_PARSER_WORDS, word)) return "not_a_content_command";
    return NULL;
}

static int addCommandFix(lua_State *L, CommandFixKind kind) {
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    const char *call = COMMAND_FIX_CALLS[kind];
    if (refuseFromDeadLuaScript(L, call)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    char word[64] = "";
    const char *reason = !luaCommandsLoading ? "not_in_commands_file" : commandFixNameProblem(kind, name, word);
    int callback = -1;
    if (!reason && (callback = newLuaCallback(L, 2, 0)) < 0) reason = "out_of_memory";
    if (!reason && !growList((void **)&commandFixes, &commandFixCapacity, commandFixCount + 1, sizeof commandFixes[0])) reason = "out_of_memory";
    if (reason) {
        refuseLuaCall(runningLuaMod(), call, reason, "command=%.60s", name);
        lua_pushboolean(L, 0);
        return 1;
    }
    CommandFix *f = &commandFixes[commandFixCount++];
    memset(f, 0, sizeof *f);
    snprintf(f->word, sizeof f->word, "%s", word);
    f->kind = kind;
    f->lua = callback;
    writeLog("command_fix_added", "mod=%s call=%s command=%s", runningLuaMod() ? runningLuaMod() : "?", call, word);
    lua_pushboolean(L, 1);
    return 1;
}
static int lFixCommand(lua_State *L) { return addCommandFix(L, FIX_COMMAND); }
static int lFixTrigger(lua_State *L) { return addCommandFix(L, FIX_TRIGGER); }
static int lFixAction(lua_State *L) { return addCommandFix(L, FIX_ACTION); }
static int lFixAfter(lua_State *L) { return addCommandFix(L, FIX_AFTER); }

static void dropCommandFixesOf(const LuaScript *s) {
    int kept = 0;
    for (int i = 0; i < commandFixCount; i++) {
        int lua = commandFixes[i].lua;
        if (lua >= 0 && lua < luaCallbackCount && luaCallbacks[lua].s == s) continue;
        commandFixes[kept++] = commandFixes[i];
    }
    commandFixCount = kept;
}

// ---- Running a fix ------------------------------------------------------------------------------
typedef struct {
    int ref;
    const char *word, *line, *args, *mod;
    int level, event, sign, player, x, y, squareEvent, after;
} CommandFixCall;

static void setFixField(lua_State *L, const char *key, lua_Integer value) {
    lua_pushinteger(L, value);
    lua_setfield(L, -2, key);
}
static int pushCommandFix(lua_State *L, void *ctx) {
    const CommandFixCall *c = ctx;
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
    setFixField(L, "level", c->level);
    if (c->event) {
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "event");
        lua_pushboolean(L, c->squareEvent);
        lua_setfield(L, -2, "squareEvent");
        setFixField(L, "player", c->player);
        setFixField(L, "x", c->x);
        setFixField(L, "y", c->y);
        if (c->sign) setFixField(L, "sign", c->sign);
    }
    if (c->after) {
        setFixField(L, "monster", G32(RVA_MONSTER_CURRENT));
        setFixField(L, "weapon", G32(RVA_WEAPON_CURRENT));
        setFixField(L, "item", G32(RVA_ITEM_CURRENT));
        setFixField(L, "class", G32(RVA_CLASS_CURRENT));
        setFixField(L, "terrain", G32(RVA_TERRAIN_CURRENT));
        setFixField(L, "ritual", G32(RVA_RITUAL_CURRENT));
    }
    return 1;
}

static int runCommandFix(CommandFix *f, CommandFixCall *c, int *boolean, char **text, int *integer) {
    if (f->lua < 0 || f->lua >= luaCallbackCount) return -1;
    LuaCallback cb = luaCallbacks[f->lua];
    if (!cb.s || cb.s->dead) return -1;
    NetRole role = netRoleNow();
    if (role != NET_ROLE_SINGLE && !commandFixOpenFor(role, modClassNow(cb.s->mod))) return -1;
    c->ref = cb.ref;
    LuaLoadPhase outer = c->event ? luaLoadPhase : beginLuaLoadPhase(LUA_LOAD_PASS);
    int ran = callLuaProtected(cb.s, pushCommandFix, c, 1);
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

static void refuseCommandFixResult(CommandFix *f, int type) {
    if (f->badResultLogged) return;
    f->badResultLogged = 1;
    LuaScript *s = f->lua >= 0 && f->lua < luaCallbackCount ? luaCallbacks[f->lua].s : NULL;
    refuseLuaCall(s ? s->mod : "?", COMMAND_FIX_CALLS[f->kind], "bad_result", "command=%s result=%s", f->word,
                  s && s->L ? lua_typename(s->L, type) : "?");
}

#define COMMAND_FIX_LINE_MAX 50000

static const char *runFixCommands(const char *line, int event, const char *mod, int level, char **fixed) {
    for (int i = 0; i < commandFixCount; i++) {
        CommandFix *f = &commandFixes[i];
        if (f->kind != FIX_COMMAND) continue;
        char word[64];
        int sign;
        const char *args = readFixWord(line, event, word, &sign);
        if (strcmp(word, f->word)) continue;
        CommandFixCall c = {0};
        c.word = word, c.line = line, c.args = args, c.mod = mod, c.level = level, c.event = event, c.sign = sign;
        if (event) {
            c.player = G32(RVA_EVENT_PLAYER), c.squareEvent = G32(RVA_EVENT_STATE) == SQUARE_EVENT;
            c.x = G32(RVA_EVENT_TARGET_X), c.y = G32(RVA_EVENT_TARGET_Y);
        }
        int boolean = 1;
        char *text = NULL;
        int type = runCommandFix(f, &c, &boolean, &text, NULL);
        if (type == -1 || type == LUA_TNIL || (type == LUA_TBOOLEAN && boolean)) continue;
        if (type == LUA_TBOOLEAN) return NULL;
        if (type != LUA_TSTRING) {
            refuseCommandFixResult(f, type);
            continue;
        }
        if (!text) {
            refuseLuaCall(luaCallbacks[f->lua].s->mod, COMMAND_FIX_CALLS[f->kind], "out_of_memory", "command=%s", f->word);
            continue;
        }
        if (!text[0]) {
            free(text);
            return NULL;
        }
        size_t n = strlen(text);
        if (n >= COMMAND_FIX_LINE_MAX || strcspn(text, "\r\n") != n) {
            refuseLuaCall(luaCallbacks[f->lua].s->mod, COMMAND_FIX_CALLS[f->kind], n >= COMMAND_FIX_LINE_MAX ? "line_too_long" : "two_lines",
                          "command=%s length=%zu", f->word, n);
            free(text);
            continue;
        }
        free(*fixed);
        *fixed = text;
        line = text;
    }
    return line;
}

static int contentFixDue(const char *word, int level) {
    if (level == 2) return 1;
    if (level == 1) return WORD_IN(PARSER_LEVEL1_WORDS, word);
    if (level == 4) return WORD_IN(PARSER_LEVEL4_WORDS, word);
    return 0;
}

static const char *fixContentCommandLine(const char *line, int mod, int level, char **fixed) {
    if (!commandFixCount) return line;
    char word[64];
    int sign;
    readFixWord(line, 0, word, &sign);
    if (!contentFixDue(word, level) || !commandFixed(FIX_COMMAND, word)) return line;
    return runFixCommands(line, 0, mod >= 0 && mod <= G32(RVA_MOD_LAST_INDEX) ? modName(mod) : NULL, level, fixed);
}

static void fixAfterContentLine(const char *line, int mod, int level) {
    if (!commandFixCount) return;
    char word[64];
    int sign;
    const char *args = readFixWord(line, 0, word, &sign);
    if (!contentFixDue(word, level)) return;
    for (int i = 0; i < commandFixCount; i++) {
        CommandFix *f = &commandFixes[i];
        if (f->kind != FIX_AFTER || strcmp(f->word, word)) continue;
        CommandFixCall c = {0};
        c.word = word, c.line = line, c.args = args, c.mod = mod >= 0 && mod <= G32(RVA_MOD_LAST_INDEX) ? modName(mod) : NULL;
        c.level = level, c.after = 1;
        int boolean;
        runCommandFix(f, &c, &boolean, NULL, NULL);
    }
}

static const char *fixEventCommandLine(const char *line, int level, const char *mod, char **fixed) {
    if (!commandFixCount || level != 3) return line;
    char word[64];
    int sign;
    readFixWord(line, 1, word, &sign);
    if (G32(RVA_EVENT_STATE) < 1 && !WORD_IN(GAME_EVENT_STARTS, word)) return line;
    if (!commandFixed(FIX_COMMAND, word)) return line;
    return runFixCommands(line, 1, mod, level, fixed);
}

static const char *const UNIT_TRIGGERS[] = {"hascom", "hascomhere", "hasunit", "hasunithere", "hasaffunithere"};

static int answerCommandFixesAt(CommandFixKind kind, const char *word, CommandFixCall *c, int *unit) {
    *unit = -1;
    for (int i = 0; i < commandFixCount; i++) {
        CommandFix *f = &commandFixes[i];
        if (f->kind != kind || strcmp(f->word, word)) continue;
        int boolean = 0, integer = 0;
        int type = runCommandFix(f, c, &boolean, NULL, &integer);
        if (type == -1 || type == LUA_TNIL) continue;
        if (kind == FIX_TRIGGER && integer && boolean >= 0 && boolean < UNIT_TABLE_MAX && WORD_IN(UNIT_TRIGGERS, word)) {
            *unit = boolean;
            return 1;
        }
        if (type != LUA_TBOOLEAN) {
            refuseCommandFixResult(f, type);
            continue;
        }
        if (kind == FIX_TRIGGER) return boolean;
        if (boolean) return 1;
    }
    return -1;
}

typedef struct { int square; unsigned char mark; } FixedSquare;

static void runFixedEventLine(const char *line, int level, const char *mod) {
    int state = G32(RVA_EVENT_STATE);
    if (!commandFixCount || level != 3 || (state != PLAYER_EVENT && state != SQUARE_EVENT)) {
        nextEventLine(line, level);
        return;
    }
    char word[64];
    int sign;
    const char *args = readFixWord(line, 1, word, &sign);
    CommandFixKind kind = sign ? FIX_TRIGGER : FIX_ACTION;
    if (!commandFixed(kind, word)) {
        nextEventLine(line, level);
        return;
    }
    CommandFixCall c = {0};
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
        int answer = answerCommandFixesAt(kind, word, &c, &unit);
        if (answer < 0) nextEventLine(line, level);
        else if (kind == FIX_TRIGGER && answer != (sign > 0)) G32(RVA_EVENT_STATE) = EVENT_FAILED;
        if (unit >= 0) targetUnits[target] = unit;
        return;
    }
    unsigned char *marks = gameBase + RVA_SQUARE_EVENT_A;
    FixedSquare *kept = NULL;
    int keptCount = 0, keptCapacity = 0, left = 0;
    int width = G32(RVA_MAP_DEPTH), height = G32(RVA_MAP_H);
    if (width > MAP_COLUMNS_MAX) width = MAP_COLUMNS_MAX;
    if (height > MAP_COLUMN_SQUARES) height = MAP_COLUMN_SQUARES;
    for (int i = 0; i < SQUARE_EVENT_A_SIZE; i++) {
        if (!marks[i] || i / MAP_COLUMN_SQUARES >= width || i % MAP_COLUMN_SQUARES >= height) continue;
        int square = target >= 0 ? target : i;
        c.x = square / MAP_COLUMN_SQUARES, c.y = square % MAP_COLUMN_SQUARES;
        int unit;
        int answer = answerCommandFixesAt(kind, word, &c, &unit);
        if (unit >= 0) targetUnits[square] = unit;
        if (answer < 0) {
            left++;
            continue;
        }
        if (kind == FIX_TRIGGER && answer != (sign > 0)) {
            marks[i] = 0;
            continue;
        }
        if (!growList((void **)&kept, &keptCapacity, keptCount + 1, sizeof kept[0])) {
            refuseLuaCall("-", COMMAND_FIX_CALLS[kind], "out_of_memory", "command=%s", word);
            left++;
            continue;
        }
        kept[keptCount++] = (FixedSquare){i, marks[i]};
    }
    if (left) {
        for (int k = 0; k < keptCount; k++) marks[kept[k].square] = 0;
        nextEventLine(line, level);
        if (keptCount && G32(RVA_EVENT_STATE) == EVENT_FAILED) {
            memset(marks, 0, SQUARE_EVENT_A_SIZE);
            G32(RVA_EVENT_STATE) = SQUARE_EVENT;
        }
        for (int k = 0; k < keptCount; k++) marks[kept[k].square] = kept[k].mark;
    } else if (kind == FIX_TRIGGER && !keptCount) {
        G32(RVA_EVENT_STATE) = EVENT_FAILED;
    }
    free(kept);
}
