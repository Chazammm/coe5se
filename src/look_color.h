#pragma once

#define LOOK_COLOR_WORD       "lookcolor"
#define FLY_LOOK_COLOR_WORD   "flylookcolor"
#define LOOK_COLOR_SMOKE_WORD "smoke"
#define LOOK_COLOR_SLOTS      8192
#define VISUAL_FX_TRACER      222
#define PARTICLE_FLAG_SMOKE   0x1

enum { LOOK_COLOR_MAIN = 1, LOOK_COLOR_SMOKE = 2 };
typedef struct {
    unsigned char set;
    unsigned char main[3], smoke[3];
} LookColor;
static struct {
    LookColor *look, *fly;
    int size;
} lookColors;

static void lookColorBeforePass(int level) {
    if (level != 2 || !lookColors.look) return;
    memset(lookColors.look, 0, (size_t)lookColors.size * sizeof(LookColor));
    memset(lookColors.fly, 0, (size_t)lookColors.size * sizeof(LookColor));
}

static const LookColor *lookColorOf(int weapon, int fly) {
    if (weapon < 0 || weapon >= lookColors.size) return NULL;
    const LookColor *c = fly ? &lookColors.fly[weapon] : &lookColors.look[weapon];
    return c->set ? c : NULL;
}

static int lookColorsGrow(int records) {
    if (lookColors.size >= records) return 1;
    LookColor *look = realloc(lookColors.look, (size_t)records * sizeof(LookColor));
    if (look) lookColors.look = look;
    LookColor *fly = look ? realloc(lookColors.fly, (size_t)records * sizeof(LookColor)) : NULL;
    if (!fly) return 0;
    lookColors.fly = fly;
    memset(look + lookColors.size, 0, (size_t)(records - lookColors.size) * sizeof(LookColor));
    memset(fly + lookColors.size, 0, (size_t)(records - lookColors.size) * sizeof(LookColor));
    lookColors.size = records;
    return 1;
}

static void lookColorWordLine(const char *word, int fly, const char *args, int mod) {
    const char *p = args;
    char part[16];
    int weapon = G32(RVA_WEAPON_CURRENT), records = tableRecords(TABLE_WEAPON), smoke = 0, bad = 0;
    long rgb[3];
    skipSpaces(&p);
    const char *afterPart = p;
    int len = readWord(&afterPart, part, sizeof part);
    if (len > 0 && (size_t)len < sizeof part && !strcmp(part, LOOK_COLOR_SMOKE_WORD)) smoke = 1, p = afterPart;
    for (int i = 0; i < 3; i++) {
        char value[16], *end = NULL;
        skipSpaces(&p);
        int n = readWord(&p, value, sizeof value);
        rgb[i] = n > 0 && (size_t)n < sizeof value ? strtol(value, &end, 10) : -1;
        if (!end || *end || rgb[i] < 0 || rgb[i] > 255) bad = 1;
    }
    skipSpaces(&p);
    if (weapon < 0 || weapon >= records) {
        refuseC5mLine(word, spellModName(mod), "no_weapon", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    if (bad || !isLineEnd(p)) {
        refuseC5mLine(word, spellModName(mod), "bad_value", "line=%d text=%.40s", G32(RVA_MOD_LINE), args);
        return;
    }
    if (!lookColorsGrow(records)) {
        refuseC5mLine(word, spellModName(mod), "out_of_memory", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    LookColor *c = fly ? &lookColors.fly[weapon] : &lookColors.look[weapon];
    unsigned char *to = smoke ? c->smoke : c->main;
    for (int i = 0; i < 3; i++) to[i] = (unsigned char)rgb[i];
    c->set |= smoke ? LOOK_COLOR_SMOKE : LOOK_COLOR_MAIN;
    writeLog("look_color_set", "mod=%s word=%s weapon=%d part=%s red=%ld green=%ld blue=%ld line=%d", spellModName(mod), word, weapon,
             smoke ? "smoke" : "all", rgb[0], rgb[1], rgb[2], G32(RVA_MOD_LINE));
}
static void lookcolorLine(const char *args, int mod, int level) {
    if (level == 2) lookColorWordLine(LOOK_COLOR_WORD, 0, args, mod);
}
static void flylookcolorLine(const char *args, int mod, int level) {
    if (level == 2) lookColorWordLine(FLY_LOOK_COLOR_WORD, 1, args, mod);
}

static int lookParticlesMax(int row) {
    int total = 0;
    while (row > 0 && row < VISUAL_FX_ROWS) {
        const unsigned char *r = GAME_PTR(RVA_VISUAL_FX_TABLE) + (size_t)row * VISUAL_FX_SIZE;
        int flags = *(const int *)(r + VISUAL_FX_FLAGS), count = *(const int *)(r + VISUAL_FX_COUNT), each = 1;
        switch (flags & 0xff) {
        case 12: case 16: case 19: each = 128; break;
        case 17: each = 256; break;
        case 18: case 22: each = 16; break;
        case 23: each = 32; break;
        case 20: each = 2; break;
        }
        if (count > 0) total += count * each;
        if (total >= LOOK_COLOR_SLOTS || !(flags & 0x2000)) break;
        row++;
    }
    return total < LOOK_COLOR_SLOTS ? total : LOOK_COLOR_SLOTS;
}

static int lookFreeSlots(int most, int *slots) {
    int at = G32(RVA_PARTICLE_FREE_HINT), last = G32(RVA_PARTICLES_LAST), n = 0;
    if (at < 0) at = 0;
    const unsigned char *particles = GAME_PTR(RVA_PARTICLES);
    for (; n < most && at < PARTICLES_MAX; at++)
        if (at > last || *(const unsigned short *)(particles + (size_t)at * PARTICLE_SIZE + PARTICLE_FLAGS) == 0xffff) slots[n++] = at;
    return n;
}

static void lookColorPaint(const LookColor *c, const int *slots, int n) {
    unsigned char *particles = GAME_PTR(RVA_PARTICLES);
    for (int i = 0; i < n; i++) {
        unsigned char *p = particles + (size_t)slots[i] * PARTICLE_SIZE;
        unsigned short flags = *(unsigned short *)(p + PARTICLE_FLAGS);
        if (flags == 0xffff) continue;
        const unsigned char *rgb = (flags & PARTICLE_FLAG_SMOKE) && (c->set & LOOK_COLOR_SMOKE) ? c->smoke
                                   : (c->set & LOOK_COLOR_MAIN)                                ? c->main
                                                                                               : NULL;
        if (!rgb) continue;
        for (int k = 0; k < 3; k++) ((float *)(p + PARTICLE_R))[k] = rgb[k] / 255.0f;
    }
}

static void lookColorDraw(const LookColor *c, int most, void (*draw)(void *), void *arg) {
    static int slots[LOOK_COLOR_SLOTS];
    int n = c && most > 0 ? lookFreeSlots(most, slots) : 0;
    draw(arg);
    if (n) lookColorPaint(c, slots, n);
}

static const unsigned char *lookWeaponRecord(int weapon) {
    return weapon >= 0 && weapon < tableRecords(TABLE_WEAPON) ? tableAddress(TABLE_WEAPON) + (size_t)weapon * WEAPON_SIZE : NULL;
}

typedef struct {
    void (*draw)(int index);
    int index;
} LookFlyCall;
static void lookFlyCall(void *arg) { ((LookFlyCall *)arg)->draw(((LookFlyCall *)arg)->index); }
static void lookColorFlyDraw(int index, void (*draw)(int index)) {
    int weapon = index >= 0 && index < FLY_WEAPONS_MAX ? *(const int *)(GAME_PTR(RVA_FLY_WEAPONS) + (size_t)index * FLY_WEAPON_SIZE + FLY_WEAPON_WEAPON) : -1;
    const LookColor *c = lookColorOf(weapon, 1);
    if (!c) {
        draw(index);
        return;
    }
    const unsigned char *w = lookWeaponRecord(weapon);
    int flyLook = w ? *(const int *)(w + WEAPON_FLYLOOK) : 0, most = lookParticlesMax(VISUAL_FX_TRACER), own = lookParticlesMax(flyLook);
    most = (own > most ? own : most) * beamFlyDraws(index);
    LookFlyCall call = {draw, index};
    lookColorDraw(c, most < LOOK_COLOR_SLOTS ? most : LOOK_COLOR_SLOTS, lookFlyCall, &call);
}

// ---- The game's side -----------------------------------------------------------------------------

typedef struct {
    int look;
    float x, y;
} LookStrikeCall;
#ifndef __aarch64__
static void lookStrikeGame(void *arg) {
    const LookStrikeCall *s = arg;
    GAME_FN(void (*)(int, int, int), RVA_PLACE_WEAPON_LOOK)(s->look, (int)s->x, (int)s->y);
}
#else
static void lookStrikeGame(void *arg) {
    const LookStrikeCall *s = arg;
    GAME_FN(void (*)(int, float, float), RVA_DRAW_VISUAL_FX)(s->look, s->x, s->y);
}
#endif
static void lookColorStrike(int look, int weapon, float x, float y) {
    LookStrikeCall call = {look, x, y};
    const LookColor *c = lookColorOf(weapon, 0);
    lookColorDraw(c, c ? lookParticlesMax(look) : 0, lookStrikeGame, &call);
}

#ifndef __aarch64__
static uintptr_t *lookRspSlot;
static void lookStrikeHook(int look, int x, int y) {
    lookColorStrike(look, *(const int *)(*lookRspSlot + 8 + FRAME_WEAPON_ATTACK_WEAPON), (float)x, (float)y);
}
static int installLookColorCall(void) {
    unsigned char *stub = allocNear(gameBase + RVA_TEXT_SECTION), *slot = stub ? allocNear(stub) : NULL;
    if (!slot) {
        writeLog("hook_skipped", "site=look_color reason=no_near_page");
        return 0;
    }
    makePagesReadExecute(stub, 0x1000);
    lookRspSlot = (uintptr_t *)slot;
    return redirectCall(RVA_PLACE_WEAPON_LOOK_CALL, RVA_PLACE_WEAPON_LOOK, stub, (void *)lookStrikeHook, "place_weapon_look", lookRspSlot);
}
#else
// @platform-split look_color: Mac's game inlines the placing function, so its hook is the inlined copy's draw call, the weapon read in asm
void coe5seLookColorStrike(int look, int weapon, float x, float y);
__attribute__((used)) void coe5seLookColorStrike(int look, int weapon, float x, float y) { lookColorStrike(look, weapon, x, y); }
__asm__(".text\n.p2align 2\n_coe5seLookColorStrikeStub:\n"
        "  ldr w1, [sp, #0x168]\n"
        "  b _coe5seLookColorStrike\n");
extern const unsigned char coe5seLookColorStrikeStub[] __asm__("_coe5seLookColorStrikeStub");
_Static_assert(FRAME_WEAPON_ATTACK_WEAPON == 0x168, "the stub's immediate is the frame entry's Mac column");

static int installLookColorCall(void) {
    if (blTarget(gameBase + RVA_PLACE_WEAPON_LOOK_CALL) != gameBase + RVA_DRAW_VISUAL_FX) {
        writeLog("hook_skipped", "site=place_weapon_look reason=not_the_expected_call va=0x%llx", RVA_PLACE_WEAPON_LOOK_CALL + 0x100000000ULL);
        return 0;
    }
    unsigned char *trampoline = newTrampoline((void *)coe5seLookColorStrikeStub);
    uint32_t word = trampoline ? blTo(gameBase + RVA_PLACE_WEAPON_LOOK_CALL, trampoline) : 0;
    if (trampoline && writeGameCode(gameBase + RVA_PLACE_WEAPON_LOOK_CALL, &word, 4)) return 1;
    writeLog("hook_skipped", "site=place_weapon_look reason=%s", trampoline ? "code_unwritable" : "no_padding");
    return 0;
}
#endif

static void lookColorInit(void) {
    int words = apiAddModCommand("lookcolor", lookcolorLine) + apiAddModCommand("flylookcolor", flylookcolorLine);
    int calls = installLookColorCall();
    writeResultLog(words == 2 && calls == 1, "look_color_installed", "words=%d of 2 calls=%d of 1", words, calls);
}
