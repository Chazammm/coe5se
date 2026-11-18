// @platform-split ai_steps_mac: Mac's copied AI steps run their Lua wraps split at next() (Windows calls them)
#pragma once
#ifdef __APPLE__

#define SPLIT_ARGS_MAX 6
#define SPLIT_LEVELS_MAX 64
enum { SPLIT_PASS, SPLIT_WAITING, SPLIT_DONE };

typedef struct {
    LuaWrap *w;
    lua_State *co;
    int coRef;
    int state;
    LuaFrame f;
    LuaArgs in;
    LuaArgs out;
    int refusedTwice;
} SplitLevel;

typedef struct SplitCall {
    struct SplitCall *up;
    CallSite site;
    int luaSite;
    int count;
    int skipped;
    u64 result;
    int squareX, squareY;
    uint64_t frameSp;
    int row;
    int observed;
    u64 kept[SPLIT_ARGS_MAX];
    u64 passed[SPLIT_ARGS_MAX];
    LuaFrame *framesBelow;
    SplitLevel levels[SPLIT_LEVELS_MAX];
} SplitCall;
static SplitCall *splitChainCalls;

static u64 splitStepGame(void) {
    writeLog("split_step_called", "reason=unexpected");
    return 0;
}

static int lNextSplit(lua_State *L) {
    SplitLevel *level = lua_touserdata(L, lua_upvalueindex(1));
    lua_Integer serial = lua_tointeger(L, lua_upvalueindex(2));
    int found = 0;
    for (SplitCall *c = splitChainCalls; c && !found; c = c->up)
        found = level >= c->levels && level < c->levels + c->count;
    if (!found || level->f.serial != serial || level->co != L) return luaL_error(L, "next() called after its wrap returned");
    const LuaSite *ls = &luaSites[level->w->site];
    if (level->f.nextCalled) {
        if (!level->refusedTwice) refuseLuaCall(level->w->s->mod, ls->name, "next_once", "site=%s", ls->name);
        level->refusedTwice = 1;
        return pushLuaResult(L, ls, &level->out, level->f.result);
    }
    LuaArgs a = level->in;
    a.changed = NULL;
    readNextArgs(L, ls, &a);
    checkNextArgs(ls, &a, &level->in, level->w->s->mod);
    level->out = a;
    level->f.nextCalled = 1;
    return lua_yield(L, 0);
}

static int makeSplitThread(lua_State *L) {
    SplitLevel *level = lua_touserdata(L, 1);
    lua_State *co = lua_newthread(L);
    luaL_checkstack(co, 2 + LUA_CALL_ARGS, "a wrap's arguments");
    lua_rawgeti(co, LUA_REGISTRYINDEX, level->w->ref);
    lua_pushlightuserdata(co, level);
    lua_pushinteger(co, level->f.serial);
    lua_pushcclosure(co, lNextSplit, 2);
    pushLuaArgs(co, &luaSites[level->w->site], &level->in);
    return 1;
}
static int pushMakeSplitThread(lua_State *L, void *level) {
    lua_pushcfunction(L, makeSplitThread);
    lua_pushlightuserdata(L, level);
    return 1;
}

static void stopSplitScript(LuaScript *s, lua_State *co) {
    char line[600];
    int type = lua_type(co, -1);
    if (type == LUA_TSTRING) snprintf(line, sizeof line, "%s", lua_tostring(co, -1));
    else snprintf(line, sizeof line, "(error object is a %s value)", lua_typename(co, type));
    writeLogError("lua_error", "mod=%s error=%s", s->mod, line);
    previewScriptNote(s->mod, "", 0, "", "", PREVIEW_SCRIPT_ERROR, line);
    lua_settop(co, 0);
    s->dead = 1;
    if (!s->stopLogged) writeLogError("lua_script_stopped", "mod=%s reason=script_error", s->mod);
    s->stopLogged = 1;
}

static int resumeSplitLevel(SplitLevel *level, int nargs, int *nresults) {
    LuaScript *s = level->w->s;
    if (s->stepsFrame != gameFrames) startLuaSteps(s);
    LuaScript *outer = luaRunning;
    luaRunning = s;
    luaCodeRunning++;
    int rc = lua_resume(level->co, NULL, nargs, nresults);
    luaCodeRunning--;
    luaRunning = outer;
    return rc;
}

static u64 takeSplitResult(SplitCall *c, SplitLevel *level, int nresults) {
    const LuaSite *ls = &luaSites[level->w->site];
    LuaScript *s = level->w->s;
    lua_State *co = level->co;
    u64 result;
    int base = lua_gettop(co) - nresults;
    if (ls->kind == K_TARGET_SQUARE) {
        lua_settop(co, base + 3);
        result = takeTargetSquare(co, ls, &level->in, &level->f, s->mod);
    } else {
        result = nresults && lua_isinteger(co, base + 1) ? (u64)lua_tointeger(co, base + 1) : level->f.result;
        if (ls->kind == K_OBSERVE) result = takeObservedResult(ls, &level->f, result, s->mod);
    }
    lua_settop(co, 0);
    if (!level->f.nextCalled || (siteResult(ls) == 'i' ? (int)result != (int)level->f.result : result != level->f.result))
        siteResultMod[c->site] = s->mod;
    if (!level->f.nextCalled && !level->w->noNextLogged) {
        level->w->noNextLogged = 1;
        writeLog("lua_wrap_no_next", "mod=%s site=%s priority=%d result=%lld", s->mod, ls->name, level->w->priority, (long long)result);
    }
    return result;
}

static u64 finishSplitLevel(SplitCall *c, int i, u64 result) {
    SplitLevel *level = &c->levels[i];
    if (level->state != SPLIT_WAITING) return result;
    LuaScript *s = level->w->s;
    level->state = SPLIT_DONE;
    level->f.result = result;
    if (s->dead) return result;
    const LuaSite *ls = &luaSites[level->w->site];
    int pushed = pushLuaResult(level->co, ls, &level->out, result), nresults = 0;
    luaFrames = &level->f;
    int rc = resumeSplitLevel(level, pushed, &nresults);
    luaFrames = level->f.up;
    if (rc == LUA_OK) return takeSplitResult(c, level, nresults);
    if (rc == LUA_YIELD) lua_pushstring(level->co, "a wrap can't yield");
    stopSplitScript(s, level->co);
    return result;
}

static void releaseSplitCall(SplitCall *c) {
    for (int i = 0; i < c->count; i++) {
        SplitLevel *level = &c->levels[i];
        if (level->co) {
            LuaScript *s = level->w->s;
            if (s->L) luaL_unref(s->L, LUA_REGISTRYINDEX, level->coRef);
            s->depth--;
            closeDeadLuaScript(s);
        }
    }
    luaSiteDepth[c->site]--;
    luaFrames = c->framesBelow;
    splitChainCalls = c->up;
    free(c);
}

static void notifySplitObservers(SplitCall *c, const u64 *args, int after, u64 result) {
    CallInfo call = {{args[0], args[1], args[2], args[3], args[4], args[5]}, after ? result : 0};
    notifyObservers(c->site, &call, after);
}

static SplitCall *startSplitChain(CallSite site, const LuaArgs *in, LuaArgs *out, unsigned kept) {
    if (netShutsGameCalls()) return NULL;
    int luaSite = -1;
    for (int i = 0; i < LUA_SITE_COUNT && luaSite < 0; i++)
        if (luaSites[i].site == site) luaSite = i;
    if (luaSite < 0) return NULL;
    void *g = *siteHead(site), *inner = siteGameFunction(site), *observer = observeWrapperFor(site);
    if (g == inner) return NULL;
    SplitCall *c = calloc(1, sizeof *c);
    if (!c) {
        writeLogError("split_chain_skipped", "site=%s reason=out_of_memory", SITES[site].name);
        return NULL;
    }
    c->site = site, c->luaSite = luaSite, c->up = splitChainCalls, c->framesBelow = luaFrames;
    c->squareX = c->squareY = AI_NO_SQUARE;
    splitChainCalls = c;
    luaSiteDepth[site]++;
    LuaArgs args = *in;
    if (luaSites[luaSite].kind == K_TARGET_SQUARE) {
        args.a[2] = (u64)(uintptr_t)&c->squareX;
        args.a[3] = (u64)(uintptr_t)&c->squareY;
    }
    for (LuaWrap *w = NULL; g != inner; g = w ? w->next : (void *)nextObserved[site]) {
        w = NULL;
        if (observer && g == observer) {
            c->observed = 1;
            continue;
        }
        w = luaWrapOfGate(g);
        if (!w) {
            writeLogError("split_chain_skipped", "site=%s reason=not_a_lua_wrap", SITES[site].name);
            c->count = 0;
            releaseSplitCall(c);
            return NULL;
        }
        if (c->count == SPLIT_LEVELS_MAX) {
            refuseLuaCall(w->s ? w->s->mod : "?", luaSites[luaSite].name, "split_chain_full", "max=%d", SPLIT_LEVELS_MAX);
            break;
        }
        SplitLevel *level = &c->levels[c->count++];
        level->w = w;
        level->in = args;
        level->f = (LuaFrame){luaFrames, w, &level->in, 0, 0, ++luaFrameSerial, 0, 0, 0, luaChangeCount, 0};
        LuaScript *s = w->s;
        if (!s || s->dead || !luaSiteOpenNow(s, site)) continue;
        if (!s->depth++) startLuaSteps(s);
        if (!callLuaProtected(s, pushMakeSplitThread, level, 1)) {
            s->depth--;
            closeDeadLuaScript(s);
            continue;
        }
        level->co = lua_tothread(s->L, -1);
        level->coRef = luaL_ref(s->L, LUA_REGISTRYINDEX);
        luaFrames = &level->f;
        int nresults = 0, rc = resumeSplitLevel(level, lua_gettop(level->co) - 1, &nresults);
        if (rc != LUA_YIELD) luaFrames = level->f.up;
        if (rc == LUA_YIELD) {
            level->state = SPLIT_WAITING;
            args = level->out;
            continue;
        }
        if (rc == LUA_OK) {
            level->state = SPLIT_DONE;
            c->skipped = 1;
            c->result = takeSplitResult(c, level, nresults);
            for (int i = c->count - 2; i >= 0; i--) c->result = finishSplitLevel(c, i, c->result);
            break;
        }
        stopSplitScript(s, level->co);
    }
    for (int k = 0; k < SPLIT_ARGS_MAX && !c->skipped; k++) {
        if (!(kept >> k & 1) || args.a[k] == in->a[k]) continue;
        for (int i = 0; i < c->count; i++) {
            const SplitLevel *level = &c->levels[i];
            if (level->state == SPLIT_WAITING && level->out.a[k] != level->in.a[k])
                refuseLuaCall(level->w->s->mod, luaSites[luaSite].name, "argument_kept", "arg=%d value=%lld own=%lld", k + 1,
                              (long long)(int64_t)level->out.a[k], (long long)(int64_t)level->in.a[k]);
        }
        args.a[k] = in->a[k];
    }
    *out = args;
    if (c->observed && !c->skipped) notifySplitObservers(c, out->a, 0, 0);
    return c;
}

static u64 endSplitChain(SplitCall *c, u64 result) {
    if (c->observed) notifySplitObservers(c, c->passed, 1, result);
    for (int i = c->count - 1; i >= 0; i--) result = finishSplitLevel(c, i, result);
    return result;
}

// ---- The hooks at each copied step's ends ---------------------------------------------------------
enum { PLACE_NONE, PLACE_REG, PLACE_SP, PLACE_X29, PLACE_CONST, PLACE_SP32 };
typedef struct { unsigned char in; int at; } SplitPlace;
enum {
    SKIP_NONE,
    SKIP_REG,
    SKIP_REG_GAME,
    SKIP_REG_SP64,
    SKIP_SP32_REG,
    SKIP_SP32_COMSTR,
    SKIP_SP64_RECORD,
    SKIP_X29_PAIR,
    SKIP_REG_SP32,
    SKIP_REG_SPADD,
};
typedef struct { unsigned char op, reg; short at; int64_t value; int stride; } SplitSkip;
enum { VALUE_MOV, VALUE_CMP };
typedef struct { unsigned int rva; unsigned char op, reg, arg; } SplitValueHook;
typedef struct { unsigned char arg; SplitPlace place; } SplitAlso;
typedef struct { unsigned char arg; SplitPlace place; unsigned int base; int stride; } SplitRecordPlace;
#define SPLIT_RECORDS_MAX 4
#define SPLIT_SKIPS_MAX 6
#define SPLIT_VALUES_MAX 2
#define SPLIT_ALSO_MAX 4
typedef struct {
    CallSite site;
    unsigned int startRva, endRva, noneRva;
    SplitPlace args[SPLIT_ARGS_MAX];
    SplitPlace score, x, y;
    SplitSkip skip[SPLIT_SKIPS_MAX];
    SplitValueHook values[SPLIT_VALUES_MAX];
    SplitPlace result;
    SplitAlso also[SPLIT_ALSO_MAX];
    SplitRecordPlace records[SPLIT_RECORDS_MAX];
    SplitAlso restore[SPLIT_ALSO_MAX];
} SplitStepMac;
#define MOVE_LOOP_ARGS {{PLACE_SP, AI_MOVE_MAC_COMMANDER_AT}, {PLACE_SP, AI_MOVE_MAC_COMSTR_AT}}
static const SplitStepMac SPLIT_STEPS_MAC[] = {
#ifdef SPLIT_ROWS_TEST
    SPLIT_ROWS_TEST
#endif
    {SITE_AI_RALLY, RVA_AI_RALLY_COPY_START, RVA_AI_RALLY_COPY_END, RVA_AI_RALLY_COPY_NONE + 8, MOVE_LOOP_ARGS, {PLACE_SP, 0x34},
     {PLACE_X29, -0xd8}, {PLACE_X29, -0xd0},
     {{SKIP_REG_GAME, 12, 0, RVA_UNITS & ~0xfffu, 0}, {SKIP_REG_SP64, 24, AI_MOVE_MAC_COMMANDER_AT, 0, 0}}, {{0}}, {0}, {{0}}, {{0}}, {{0}}},
    {SITE_AI_FRIENDLY_MOVE, RVA_AI_FRIENDLY_COPY_START, RVA_AI_FRIENDLY_COPY_END, 0, MOVE_LOOP_ARGS, {PLACE_SP, 0xd4}, {PLACE_SP, 0x60},
     {PLACE_SP, 0x64}, {{SKIP_SP32_COMSTR, 0, 0x30, 4, 0}, {SKIP_X29_PAIR, 0, -0x78, -1, 0}}, {{0}}, {0}, {{0}}, {{0}}, {{0}}},
    {SITE_AI_CONQUER, RVA_AI_CONQUER_COPY_START, RVA_AI_CONQUER_COPY_END, 0, MOVE_LOOP_ARGS, {PLACE_REG, 19}, {PLACE_REG, 22},
     {PLACE_REG, 21},
     {{SKIP_REG, 26, 0, 0xc580, 0}, {SKIP_X29_PAIR, 0, -0x78, -1, 0}, {SKIP_SP32_REG, 9, 0x34, 0, 0},
      {SKIP_REG_GAME, 20, 0, RVA_COMMANDER_NAMES, 0}, {SKIP_REG_SP64, 8, 0xb8, 0, 0}},
     {{0}}, {0}, {{0}}, {{0}}, {{0}}},
    {SITE_AI_EXPLORE, RVA_AI_EXPLORE_COPY_START, RVA_AI_EXPLORE_COPY_END, 0, MOVE_LOOP_ARGS, {PLACE_REG, 21}, {PLACE_REG, 24},
     {PLACE_REG, 23}, {{SKIP_REG, 26, 0, 0xc580, 0}, {SKIP_X29_PAIR, 0, -0x78, -1, 0}, {SKIP_SP32_REG, 9, 0x34, 0, 0}}, {{0}}, {0}, {{0}}, {{0}}, {{0}}},
    {SITE_AI_GATHER_TROOPS, RVA_AI_GATHER_COPY_START, RVA_AI_GATHER_COPY_END, 0, {{PLACE_SP, AI_MOVE_MAC_COMMANDER_AT}, {PLACE_NONE, 0}},
     {0}, {0}, {0}, {{0}}, {{0}}, {0}, {{0}}, {{0}}, {{0}}},
    {SITE_AI_SPLIT_ARMY, RVA_AI_SPLIT_COPY_START, RVA_AI_SPLIT_COPY_END, 0, {{PLACE_REG, 19}, {PLACE_CONST, AI_SPLIT_MIN_UNITS_MAC}},
     {0}, {0}, {0}, {{0}}, {{RVA_AI_SPLIT_MIN_UNITS_TRACE, VALUE_MOV, 9, 1}, {RVA_AI_SPLIT_MIN_UNITS_TEST, VALUE_CMP, 21, 1}}, {0}, {{0}}, {{0}}, {{0}}},
    {SITE_AI_RECRUIT, RVA_AI_RECRUIT_COPY_START, RVA_AI_RECRUIT_COPY_END, 0, {{PLACE_REG, 19}, {PLACE_NONE, 0}}, {0}, {0}, {0},
     {{SKIP_SP64_RECORD, 24, 0xf0, RVA_SLOT_NAME + 0x1930, PLAYER_BLOCK_SIZE}}, {{0}}, {0}, {{0}}, {{0}}, {{0}}},
#include "split_rows_world_mac.h"
#include "split_rows_battle_mac.h"
};
#define SPLIT_STEP_COUNT ((int)(sizeof SPLIT_STEPS_MAC / sizeof SPLIT_STEPS_MAC[0]))
static StepHook splitStartHooksMac[SPLIT_STEP_COUNT], splitEndHooksMac[SPLIT_STEP_COUNT], splitNoneHooksMac[SPLIT_STEP_COUNT],
    splitValueHooksMac[SPLIT_STEP_COUNT][SPLIT_VALUES_MAX];

static int splitStepIndex(CallSite site) {
    for (int i = 0; i < SPLIT_STEP_COUNT; i++)
        if (SPLIT_STEPS_MAC[i].site == site) return i;
    return -1;
}
static int isSplitSite(CallSite site) { return splitStepIndex(site) >= 0; }
static int splitRowOfHook(const StepHook *hook, const StepHook *table, size_t stride) {
    uintptr_t at = (uintptr_t)hook, first = (uintptr_t)table, size = SPLIT_STEP_COUNT * stride;
    if (at >= first && at < first + size) return (int)((at - first) / stride);
    return splitStepIndex(hook->site);
}

static uint32_t *splitPlace(StepContext *ctx, SplitPlace p) {
    if (p.in == PLACE_REG) return (uint32_t *)&ctx->x[p.at];
    return (uint32_t *)(uintptr_t)((p.in == PLACE_SP || p.in == PLACE_SP32 ? ctx->sp : ctx->x[29]) + (int64_t)p.at);
}
static u64 splitArg(StepContext *ctx, SplitPlace p) {
    if (p.in == PLACE_REG) return ctx->x[p.at];
    if (p.in == PLACE_SP) return *(u64 *)(uintptr_t)(ctx->sp + (int64_t)p.at);
    if (p.in == PLACE_CONST) return (u64)(int64_t)p.at;
    if (p.in == PLACE_SP32 || p.in == PLACE_X29) return (u64)(int64_t)(int32_t)*splitPlace(ctx, p);
    return 0;
}
static void putSplitArg(StepContext *ctx, SplitPlace p, u64 value) {
    if (p.in == PLACE_REG) ctx->x[p.at] = value;
    else if (p.in == PLACE_SP) *(u64 *)(uintptr_t)(ctx->sp + (int64_t)p.at) = value;
    else if (p.in == PLACE_SP32 || p.in == PLACE_X29) *splitPlace(ctx, p) = (uint32_t)value;
}
static void putSplitArgAll(StepContext *ctx, const SplitStepMac *m, int k, u64 value) {
    putSplitArg(ctx, m->args[k], value);
    for (int j = 0; j < SPLIT_ALSO_MAX && m->also[j].place.in != PLACE_NONE; j++)
        if (m->also[j].arg == k) putSplitArg(ctx, m->also[j].place, value);
    for (int j = 0; j < SPLIT_RECORDS_MAX && m->records[j].place.in != PLACE_NONE; j++) {
        const SplitRecordPlace *r = &m->records[j];
        if (r->arg == k) putSplitArg(ctx, r->place, (u64)(uintptr_t)(gameBase + r->base) + (u64)((int64_t)(int)value * r->stride));
    }
}
static void putSplitResult(StepContext *ctx, const SplitStepMac *m, u64 score, int x, int y) {
    if (m->score.in == PLACE_NONE) {
        if (m->result.in == PLACE_REG) ctx->x[m->result.at] = (uint32_t)score;
        else if (m->result.in != PLACE_NONE) *splitPlace(ctx, m->result) = (uint32_t)score;
        return;
    }
    const SplitPlace places[3] = {m->score, m->x, m->y};
    const uint32_t values[3] = {(uint32_t)score, (uint32_t)x, (uint32_t)y};
    for (int k = 0; k < 3; k++) {
        if (places[k].in == PLACE_REG) ctx->x[places[k].at] = values[k];
        else *splitPlace(ctx, places[k]) = values[k];
    }
}
static SplitCall *splitCallHere(StepContext *ctx, int row) {
    SplitCall *c = splitChainCalls;
    return c && c->row == row && c->site == SPLIT_STEPS_MAC[row].site && c->frameSp == ctx->sp ? c : NULL;
}

static int splitStartRow(StepContext *ctx, int i) {
    const SplitStepMac *m = &SPLIT_STEPS_MAC[i];
    for (SplitCall *stale; (stale = splitCallHere(ctx, i));) {
        writeLogWarning("split_call_dropped", "site=%s reason=no_end", SITES[m->site].name);
        releaseSplitCall(stale);
    }
    LuaArgs in = {{0}, {0}, NULL, NULL}, out;
    unsigned kept = 0;
    for (int k = 0; k < SPLIT_ARGS_MAX; k++) {
        in.a[k] = splitArg(ctx, m->args[k]);
        int hooked = 0;
        for (int j = 0; j < SPLIT_VALUES_MAX && m->values[j].rva; j++) hooked |= m->values[j].arg == k;
        if (m->args[k].in == PLACE_CONST && !hooked) kept |= 1u << k;
    }
    SplitCall *c = startSplitChain(m->site, &in, &out, kept);
    if (!c) return 0;
    c->frameSp = ctx->sp;
    c->row = i;
    for (int k = 0; k < SPLIT_ARGS_MAX; k++) c->kept[k] = in.a[k], c->passed[k] = out.a[k];
    if (!c->skipped) {
        for (int k = 0; k < SPLIT_ARGS_MAX; k++)
            if (out.a[k] != in.a[k]) putSplitArgAll(ctx, m, k, out.a[k]);
        return 0;
    }
    unsigned char *sp = (unsigned char *)(uintptr_t)ctx->sp;
    for (int k = 0; k < SPLIT_SKIPS_MAX && m->skip[k].op != SKIP_NONE; k++) {
        const SplitSkip *s = &m->skip[k];
        switch (s->op) {
        case SKIP_REG: ctx->x[s->reg] = (uint64_t)s->value; break;
        case SKIP_REG_GAME: ctx->x[s->reg] = (uint64_t)(uintptr_t)(gameBase + s->value); break;
        case SKIP_REG_SP64: ctx->x[s->reg] = *(uint64_t *)(sp + s->at); break;
        case SKIP_SP32_REG: *(uint32_t *)(sp + s->at) = (uint32_t)ctx->x[s->reg]; break;
        case SKIP_SP32_COMSTR: *(uint32_t *)(sp + s->at) = (uint32_t)in.a[1] << s->value; break;
        case SKIP_SP64_RECORD: *(uint64_t *)(sp + s->at) = (uint64_t)(uintptr_t)(gameBase + s->value) + ctx->x[s->reg] * (uint64_t)s->stride; break;
        case SKIP_REG_SP32: ctx->x[s->reg] = *(uint32_t *)(sp + s->at); break;
        case SKIP_REG_SPADD: ctx->x[s->reg] = (uint64_t)(uintptr_t)(sp + s->at); break;
        case SKIP_X29_PAIR: {
            int *pair = (int *)(uintptr_t)(ctx->x[29] + (int64_t)s->at);
            pair[0] = pair[1] = (int)s->value;
            break;
        }
        default: break;
        }
    }
    putSplitResult(ctx, m, c->result, c->squareX, c->squareY);
    ctx->pc = splitEndHooksMac[i].resume;
    releaseSplitCall(c);
    return 1;
}

static int finishSplitStep(StepContext *ctx, int i, u64 score, int x, int y) {
    SplitCall *c = i < 0 ? NULL : splitCallHere(ctx, i);
    if (!c) return 0;
    const SplitStepMac *m = &SPLIT_STEPS_MAC[i];
    for (int k = 0; k < SPLIT_ARGS_MAX; k++) {
        if (c->passed[k] == c->kept[k]) continue;
        putSplitArgAll(ctx, m, k, c->kept[k]);
        for (int j = 0; j < SPLIT_ALSO_MAX && m->restore[j].place.in != PLACE_NONE; j++)
            if (m->restore[j].arg == k) putSplitArg(ctx, m->restore[j].place, c->kept[k]);
    }
    c->squareX = x;
    c->squareY = y;
    u64 result = endSplitChain(c, score);
    putSplitResult(ctx, m, result, c->squareX, c->squareY);
    releaseSplitCall(c);
    return 1;
}

static void splitEndRow(StepContext *ctx, int i) {
    const SplitStepMac *m = &SPLIT_STEPS_MAC[i];
    if (m->score.in == PLACE_NONE) {
        u64 result = m->result.in == PLACE_NONE ? 0 : (u64)(int64_t)(int32_t)*splitPlace(ctx, m->result);
        finishSplitStep(ctx, i, result, AI_NO_SQUARE, AI_NO_SQUARE);
        return;
    }
    finishSplitStep(ctx, i, (u64)(int64_t)(int)*splitPlace(ctx, m->score), (int)*splitPlace(ctx, m->x), (int)*splitPlace(ctx, m->y));
}


static int splitNoneRow(StepContext *ctx, int i) {
    if (!finishSplitStep(ctx, i, 0, AI_NO_SQUARE, AI_NO_SQUARE)) return 0;
    ctx->pc = splitEndHooksMac[i].resume;
    return 1;
}

static void splitRunPoint(StepContext *ctx, unsigned rva) {
    for (int hops = 0; rva && hops <= SPLIT_STEP_COUNT; hops++) {
        unsigned next = 0;
        for (int ended = 1; ended && !next;) {
            ended = 0;
            for (int j = 0; j < SPLIT_STEP_COUNT && !ended && !next; j++) {
                const SplitStepMac *r = &SPLIT_STEPS_MAC[j];
                if (r->noneRva == rva && splitNoneRow(ctx, j)) next = r->endRva;
                else if (r->endRva == rva && splitCallHere(ctx, j)) {
                    splitEndRow(ctx, j);
                    ended = 1;
                }
            }
        }
        for (int j = 0; j < SPLIT_STEP_COUNT && !next; j++)
            if (SPLIT_STEPS_MAC[j].startRva == rva && splitStartRow(ctx, j)) next = SPLIT_STEPS_MAC[j].endRva;
        rva = next;
    }
}
static void splitStepStart(StepContext *ctx, const StepHook *hook) {
    int i = splitRowOfHook(hook, splitStartHooksMac, sizeof(StepHook));
    if (i >= 0) splitRunPoint(ctx, SPLIT_STEPS_MAC[i].startRva);
}
static void splitStepEnd(StepContext *ctx, const StepHook *hook) {
    int i = splitRowOfHook(hook, splitEndHooksMac, sizeof(StepHook));
    if (i >= 0) splitRunPoint(ctx, SPLIT_STEPS_MAC[i].endRva);
}
static void splitStepNone(StepContext *ctx, const StepHook *hook) {
    int i = splitRowOfHook(hook, splitNoneHooksMac, sizeof(StepHook));
    if (i >= 0) splitRunPoint(ctx, SPLIT_STEPS_MAC[i].noneRva);
}

static void splitStepValue(StepContext *ctx, const StepHook *hook) {
    int i = splitRowOfHook(hook, &splitValueHooksMac[0][0], sizeof(StepHook) * SPLIT_VALUES_MAX);
    if (i < 0) return;
    const SplitStepMac *m = &SPLIT_STEPS_MAC[i];
    const SplitValueHook *v = &m->values[hook - splitValueHooksMac[i]];
    SplitCall *c = splitCallHere(ctx, i);
    uint32_t value = (uint32_t)(c ? c->passed[v->arg] : splitArg(ctx, m->args[v->arg]));
    if (v->op == VALUE_MOV) {
        ctx->x[v->reg] = value;
        return;
    }
    uint32_t a = (uint32_t)ctx->x[v->reg], r = a - value;
    uint64_t n = r >> 31, z = r == 0, carry = a >= value, overflow = ((a ^ value) & (a ^ r)) >> 31;
    ctx->nzcv = (n << 31) | (z << 30) | (carry << 29) | (overflow << 28);
}

static const StepHook *splitHookAt(unsigned rva) {
    for (int j = 0; j < SPLIT_STEP_COUNT; j++) {
        const SplitStepMac *r = &SPLIT_STEPS_MAC[j];
        if (r->startRva == rva && splitStartHooksMac[j].resume) return &splitStartHooksMac[j];
        if (r->endRva == rva && splitEndHooksMac[j].resume) return &splitEndHooksMac[j];
        if (r->noneRva == rva && splitNoneHooksMac[j].resume) return &splitNoneHooksMac[j];
    }
    return NULL;
}
static int putSplitPointMac(unsigned rva, StepHook *hook, const char *name) {
    const StepHook *there = splitHookAt(rva);
    if (there) {
        hook->resume = there->resume;
        return 1;
    }
    return putStepHookMac(rva, hook, name, 0);
}

static int installSplitRowMac(int i) {
    const SplitStepMac *m = &SPLIT_STEPS_MAC[i];
    CallSite site = m->site;
    const char *name = SITES[site].name;
    splitStartHooksMac[i] = (StepHook){0, splitStepStart, site};
    splitEndHooksMac[i] = (StepHook){0, splitStepEnd, site};
    splitNoneHooksMac[i] = (StepHook){0, splitStepNone, site};
    if (!putSplitPointMac(m->endRva, &splitEndHooksMac[i], name)) return 0;
    if (m->noneRva && !putSplitPointMac(m->noneRva, &splitNoneHooksMac[i], name)) return 0;
    for (int k = 0; k < SPLIT_VALUES_MAX && m->values[k].rva; k++) {
        splitValueHooksMac[i][k] = (StepHook){0, splitStepValue, site};
        if (!putStepHookMac(m->values[k].rva, &splitValueHooksMac[i][k], name, 1)) return 0;
    }
    return putSplitPointMac(m->startRva, &splitStartHooksMac[i], name);
}

static int installSplitSiteMac(CallSite site) {
    if (splitStepIndex(site) < 0) {
        writeLog("site_skipped", "site=%s reason=not_on_this_build", SITES[site].name);
        return 0;
    }
    if (!gameCodeMatches) {
        writeLog("site_skipped", "site=%s reason=not_the_expected_code", SITES[site].name);
        return 0;
    }
    for (int i = 0; i < SPLIT_STEP_COUNT; i++) {
        const SplitStepMac *m = &SPLIT_STEPS_MAC[i];
        if (m->site != site) continue;
        unsigned rvas[3] = {m->startRva, m->endRva, m->noneRva};
        for (int k = 0; k < 3; k++) {
            uint32_t insn;
            if (!rvas[k] || splitHookAt(rvas[k])) continue;
            memcpy(&insn, gameBase + rvas[k], 4);
            if (!stepInstructionMovable(insn)) {
                writeLog("site_skipped", "site=%s reason=not_the_expected_code address=%p", SITES[site].name, (void *)(gameBase + rvas[k]));
                return 0;
            }
        }
    }
    *siteHead(site) = siteGameFunction(site);
    for (int i = 0; i < SPLIT_STEP_COUNT; i++)
        if (SPLIT_STEPS_MAC[i].site == site && !installSplitRowMac(i)) return 0;
    siteInstalled[site] = 1;
    return 1;
}
#endif
