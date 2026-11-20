#pragma once

#define RITUAL_MENU_WORD       "ritualmenu"
#define RITUAL_MENU_NAME_MAX   (RITUAL_NAME_SIZE - 1)
#define RITUAL_MENU_ARROW      " \xe2\x96\xb8"
#define RITUAL_MENU_ROWS_MAX   COMMANDER_KNOWN_RITUALS_MAX
#define RITUAL_MENU_LINES_MAX  (RITUAL_MENU_ROWS_MAX / 2)
#define RITUAL_MENU_REDOS_MAX  2
#define RITUAL_MENU_CANCEL     1
#define RITUAL_SCREEN_UNIT_ID  9
#define RITUAL_MENU_GAP        4.0f
#define RITUAL_ROW_ARGS        17
#define RITUAL_MENU_SCROLL_SLOT 99
#define LAYOUT_WINDOW_TOP_UNSET (-999.0f)
#define RITUAL_FX_NEWRIT       904
_Static_assert(RITUAL_ROW_MENU + RITUAL_MENU_LINES_MAX < RITUAL_ROW_CASTABLE, "the lines' ids are ones the screen's click test ignores");

// ---- The word -----------------------------------------------------------------------------------
static unsigned short *ritualMenuOfSlot;
static char (*ritualMenuNames)[RITUAL_NAME_SIZE];
static int ritualMenuNameCount, ritualMenuNameCapacity;

static int ritualMenuOf(int ritual) {
    if (!ritualMenuOfSlot || ritual < 0 || ritual >= RITUAL_SLOT_CAP) return -1;
    return (int)ritualMenuOfSlot[ritual] - 1;
}

static int ritualMenuIndex(const char *name) {
    for (int i = 0; i < ritualMenuNameCount; i++)
        if (!strcmp(ritualMenuNames[i], name)) return i;
    if (ritualMenuNameCount >= RITUAL_SLOT_CAP ||
        !growList((void **)&ritualMenuNames, &ritualMenuNameCapacity, ritualMenuNameCount + 1, sizeof ritualMenuNames[0]))
        return -1;
    snprintf(ritualMenuNames[ritualMenuNameCount], sizeof ritualMenuNames[0], "%.*s", RITUAL_MENU_NAME_MAX, name);
    return ritualMenuNameCount++;
}

static void ritualmenuLine(const char *args, int mod, int level) {
    if (level != 2) return;
    const char *p = args;
    char name[RITUAL_NAME_SIZE + 1];
    int len = readSpellQuoted(&p, name, sizeof name);
    if (len < 0 || (skipSpaces(&p), !isLineEnd(p))) {
        refuseC5mLine(RITUAL_MENU_WORD, spellModName(mod), "bad_value", "line=%d text=%.40s", G32(RVA_MOD_LINE), args);
        return;
    }
    if (len == 0) {
        refuseC5mLine(RITUAL_MENU_WORD, spellModName(mod), "no_name", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    if (len > RITUAL_MENU_NAME_MAX) {
        refuseC5mLine(RITUAL_MENU_WORD, spellModName(mod), "too_long", "line=%d length=%d max=%d", G32(RVA_MOD_LINE), len,
                      RITUAL_MENU_NAME_MAX);
        return;
    }
    int ritual = G32(RVA_RITUAL_CURRENT);
    if (ritual < 0 || ritual >= ritualSlotCount()) {
        refuseC5mLine(RITUAL_MENU_WORD, spellModName(mod), "no_ritual", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    if (!ritualMenuOfSlot && !(ritualMenuOfSlot = calloc(RITUAL_SLOT_CAP, sizeof ritualMenuOfSlot[0]))) {
        refuseC5mLine(RITUAL_MENU_WORD, spellModName(mod), "out_of_memory", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    int menu = ritualMenuIndex(name);
    if (menu < 0) {
        refuseC5mLine(RITUAL_MENU_WORD, spellModName(mod), "out_of_memory", "line=%d", G32(RVA_MOD_LINE));
        return;
    }
    ritualMenuOfSlot[ritual] = (unsigned short)(menu + 1);
}

static int copyRitualMenu(int source, int target) {
    if (!ritualMenuOfSlot || source < 0 || source >= RITUAL_SLOT_CAP || target < 0 || target >= RITUAL_SLOT_CAP) return 1;
    ritualMenuOfSlot[target] = ritualMenuOfSlot[source];
    return 1;
}

static void ritualMenusBeforePass(int level) {
    if (level != 2) return;
    if (ritualMenuOfSlot) memset(ritualMenuOfSlot, 0, RITUAL_SLOT_CAP * sizeof ritualMenuOfSlot[0]);
    ritualMenuNameCount = 0;
}

// ---- The ritual screen --------------------------------------------------------------------------
typedef u64 (*RitualMenuRowFn)(int, int, int, const char *, int, int, int, int, int, int, int, int, const char *, int, int, int,
                               const char *);
typedef struct {
    int slot, menu, castable;
    u64 v[RITUAL_ROW_ARGS];
    char *name, *hover, *mirrorText;
} RitualMenuRow;
typedef struct { int slot, menu, castable; } RitualMenuSeen;
typedef struct { int slot, pass, hidden; } RitualMenuListed;
typedef struct {
    RitualMenuRow rows[RITUAL_MENU_ROWS_MAX];
    int rowCount;
    RitualMenuSeen last[RITUAL_MENU_ROWS_MAX];
    int lastCount;
    RitualMenuListed listed[RITUAL_MENU_ROWS_MAX];
    RitualMenuListed lastListed[RITUAL_MENU_ROWS_MAX];
    int listedCount, lastListedCount, listedFull, lastListedFull;
    unsigned headersOut, lastHeadersOut;
    int lineMenu[RITUAL_MENU_LINES_MAX];
    int lineCount;
    int redos;
    int reopen;
    int reopened;
} RitualMenuScreen;
static RitualMenuScreen ritualMenuScreens[RITUAL_SCREEN_DEPTH_MAX];
static int ritualMenuWaitDepth;
static int ritualMenuPicking;
static RitualMenuRowFn nextRitualMenuRow, nextRitualMenuGameRow;
static int (*nextRitualMenuWait)(int, int);
static const u64 *ritualMenuGameArgs;

static int ritualMenuRowSlot(int id) {
    if (id < RITUAL_ROW_CASTABLE) return -1;
    long long slot = ritualRightClickSlot(id);
    return slot >= 0 && slot < ritualSlotCount() ? (int)slot : -1;
}

static int copyRitualMenuText(const char *text, char **out) {
    *out = NULL;
    if (!text) return 1;
    size_t n = strlen(text) + 1;
    if (!(*out = malloc(n))) return 0;
    memcpy(*out, text, n);
    return 1;
}

static int keepRitualMenuRow(RitualMenuScreen *s, int slot, int menu, const u64 *v, int castable) {
    static int fullLogged, memoryLogged;
    for (int i = 0; i < s->rowCount; i++)
        if (s->rows[i].slot == slot) return 1;
    if (s->rowCount >= RITUAL_MENU_ROWS_MAX) {
        if (!fullLogged++) writeLogWarning("ritual_menu_rows_cut", "reason=too_many_rows max=%d", RITUAL_MENU_ROWS_MAX);
        return 0;
    }
    RitualMenuRow *r = &s->rows[s->rowCount];
    r->slot = slot;
    r->menu = menu;
    r->castable = castable;
    memcpy(r->v, v, sizeof r->v);
    if (!copyRitualMenuText((const char *)(uintptr_t)v[3], &r->name) || !copyRitualMenuText((const char *)(uintptr_t)v[12], &r->hover) ||
        !copyRitualMenuText((const char *)(uintptr_t)v[16], &r->mirrorText)) {
        free(r->name);
        free(r->hover);
        free(r->mirrorText);
        if (!memoryLogged++) writeLogError("ritual_menu_rows_cut", "reason=out_of_memory");
        return 0;
    }
    r->v[3] = (u64)(uintptr_t)r->name;
    r->v[12] = (u64)(uintptr_t)r->hover;
    r->v[16] = (u64)(uintptr_t)r->mirrorText;
    s->rowCount++;
    return 1;
}

static int ritualMenuLastRows(const RitualMenuScreen *s, int menu, int *first, int *castable) {
    int n = 0;
    *first = -1;
    *castable = 0;
    for (int i = 0; i < s->lastCount; i++) {
        if (s->last[i].menu != menu) continue;
        if (!n++) *first = s->last[i].slot;
        *castable |= s->last[i].castable;
    }
    return n;
}

static u64 callRitualMenuRow(RitualMenuRowFn fn, const u64 *v) {
    return fn((int)v[0], (int)v[1], (int)v[2], (const char *)(uintptr_t)v[3], (int)v[4], (int)v[5], (int)v[6], (int)v[7], (int)v[8],
              (int)v[9], (int)v[10], (int)v[11], (const char *)(uintptr_t)v[12], (int)v[13], (int)v[14], (int)v[15],
              (const char *)(uintptr_t)v[16]);
}

static int ritualMenuPass(int slot) {
    return commanderCastEffect(slot, RITUAL_FX_NEWRIT) ? 0 : *(const int *)(ritualWordRecord(slot) + RITUAL_LEVEL);
}

static int *ritualScreenHeaderLevelInFrame(void) {
    return (int *)(siteCallerSp(SITE_RITUAL_SCREEN_ROW) + (uintptr_t)FRAME_RITUAL_SCREEN_HEADER_LEVEL);
}
static int *(*ritualScreenHeaderLevel)(void) = ritualScreenHeaderLevelInFrame;

static int ritualMenuPassHidden(const RitualMenuScreen *s, int pass) {
    int rows = 0;
    for (int i = 0; i < s->lastListedCount; i++) {
        if (s->lastListed[i].pass != pass) continue;
        if (!s->lastListed[i].hidden) return 0;
        rows++;
    }
    return rows > 0;
}

static void noteRitualMenuListed(RitualMenuScreen *s, int slot, int pass, int hidden) {
    static int fullLogged;
    if (s->listedCount >= RITUAL_MENU_ROWS_MAX) {
        s->listedFull = 1;
        if (!fullLogged++) writeLogWarning("ritual_menu_rows_cut", "reason=too_many_listed max=%d", RITUAL_MENU_ROWS_MAX);
        return;
    }
    int k = s->listedCount++;
    s->listed[k] = (RitualMenuListed){slot, pass, hidden};
    if (s->lastListedFull || k + 1 >= s->lastListedCount || s->lastListed[k].slot != slot) return;
    int next = s->lastListed[k + 1].pass;
    if (next <= 0 || next == pass || !ritualMenuPassHidden(s, next)) return;
    *ritualScreenHeaderLevel() = next;
    s->headersOut |= next < 32 ? 1u << next : 0;
}

static u64 ritualMenuRowAs(RitualMenuScreen *s, int slot, const u64 *v, int *hidden) {
    if (ritualMenuGameArgs && ritualMenuRowSlot((int)ritualMenuGameArgs[0]) != slot) return callRitualMenuRow(nextRitualMenuRow, v);
    int menu = ritualMenuOf(slot);
    if (menu < 0) return callRitualMenuRow(nextRitualMenuRow, v);
    if (!keepRitualMenuRow(s, slot, menu, ritualMenuGameArgs ? ritualMenuGameArgs : v, (int)v[14] != 0)) return callRitualMenuRow(nextRitualMenuRow, v);
    int first, anyCastable;
    if (ritualMenuLastRows(s, menu, &first, &anyCastable) < 2) return callRitualMenuRow(nextRitualMenuRow, v);
    *hidden = 1;
    if (slot != first) return 0;
    for (int i = 0; i < s->lineCount; i++)
        if (s->lineMenu[i] == menu) return 0;
    *hidden = 0;
    if (s->lineCount >= RITUAL_MENU_LINES_MAX) return callRitualMenuRow(nextRitualMenuRow, v);
    int line = s->lineCount++;
    s->lineMenu[line] = menu;
    static char translated[TRANSLATED_SIZE], text[TRANSLATED_SIZE + sizeof RITUAL_MENU_ARROW];
    snprintf(text, sizeof text, "%s" RITUAL_MENU_ARROW, translateText(ritualMenuNames[menu], translated, sizeof translated));
    return nextRitualMenuRow(RITUAL_ROW_MENU + line, (int)v[1], (int)v[2], text, -1, 0, -1, 0, -1, 0, -1, 0, "", 1, anyCastable, -1, "");
}

static u64 recordRitualMenuGameRow(int id, int a, int b, const char *name, int costType1, int cost1, int costType2, int cost2,
                                   int costType3, int cost3, int costType4, int cost4, const char *hover, int apCost, int castable,
                                   int mirrorMonster, const char *mirrorText) {
    u64 v[RITUAL_ROW_ARGS] = {(u64)id, (u64)a, (u64)b, (u64)(uintptr_t)name, (u64)costType1, (u64)cost1, (u64)costType2,
                              (u64)cost2, (u64)costType3, (u64)cost3, (u64)costType4, (u64)cost4, (u64)(uintptr_t)hover,
                              (u64)apCost, (u64)castable, (u64)mirrorMonster, (u64)(uintptr_t)mirrorText};
    const u64 *outer = ritualMenuGameArgs;
    ritualMenuGameArgs = v;
    u64 r = callRitualMenuRow(nextRitualMenuGameRow, v);
    ritualMenuGameArgs = outer;
    return r;
}

static u64 ritualMenuRow(int id, int a, int b, const char *name, int costType1, int cost1, int costType2, int cost2, int costType3,
                         int cost3, int costType4, int cost4, const char *hover, int apCost, int castable, int mirrorMonster,
                         const char *mirrorText) {
    u64 v[RITUAL_ROW_ARGS] = {(u64)id, (u64)a, (u64)b, (u64)(uintptr_t)name, (u64)costType1, (u64)cost1, (u64)costType2,
                              (u64)cost2, (u64)costType3, (u64)cost3, (u64)costType4, (u64)cost4, (u64)(uintptr_t)hover,
                              (u64)apCost, (u64)castable, (u64)mirrorMonster, (u64)(uintptr_t)mirrorText};
    if (!ritualMenuNameCount || ritualMenuPicking || ritualMenuWaitDepth >= RITUAL_SCREEN_DEPTH_MAX) return callRitualMenuRow(nextRitualMenuRow, v);
    int slot = ritualMenuRowSlot(id);
    if (slot < 0) return callRitualMenuRow(nextRitualMenuRow, v);
    RitualMenuScreen *s = &ritualMenuScreens[ritualMenuWaitDepth];
    int hidden = 0, gameSlot = ritualMenuGameArgs ? ritualMenuRowSlot((int)ritualMenuGameArgs[0]) : -1;
    u64 r = ritualMenuRowAs(s, slot, v, &hidden);
    noteRitualMenuListed(s, slot, ritualMenuPass(gameSlot >= 0 ? gameSlot : slot), hidden);
    return r;
}

static int ritualMenuRowsSame(const RitualMenuScreen *s) {
    if (s->rowCount != s->lastCount || s->listedCount != s->lastListedCount) return 0;
    for (int i = 0; i < s->listedCount; i++)
        if (s->listed[i].slot != s->lastListed[i].slot || s->listed[i].hidden != s->lastListed[i].hidden) return 0;
    for (int i = 0; i < s->rowCount; i++)
        if (s->last[i].slot != s->rows[i].slot || s->last[i].menu != s->rows[i].menu ||
            s->last[i].castable != s->rows[i].castable)
            return 0;
    return 1;
}

static void endRitualMenuFrame(RitualMenuScreen *s) {
    for (int i = 0; i < s->rowCount; i++) {
        RitualMenuRow *r = &s->rows[i];
        s->last[i] = (RitualMenuSeen){r->slot, r->menu, r->castable};
        free(r->name);
        free(r->hover);
        free(r->mirrorText);
    }
    s->lastCount = s->rowCount;
    s->rowCount = 0;
    s->lineCount = 0;
    memcpy(s->lastListed, s->listed, (size_t)s->listedCount * sizeof s->listed[0]);
    s->lastListedCount = s->listedCount;
    s->lastListedFull = s->listedFull;
    s->listedCount = s->listedFull = 0;
    if (s->headersOut != s->lastHeadersOut) writeLog("ritual_menu_headers_out", "levels=0x%x", s->headersOut);
    s->lastHeadersOut = s->headersOut;
    s->headersOut = 0;
}

static int ritualMenuPickerAnswer(RitualMenuScreen *s, int menu, int id, int *out) {
    *out = 0;
    if (id == RITUAL_MENU_CANCEL) return 1;
    if (id <= -RITUAL_ROW_CASTABLE) s->reopen = menu;
    else if (id != RITUAL_SCREEN_UNIT_ID && id != -RITUAL_SCREEN_UNIT_ID &&
             (ritualMenuRowSlot(id) < 0 || id >= (ritualExpansionActive ? RITUAL_ROW_GREYED_BASE : GAME_RITUAL_ROW_GREYED)))
        return 0;
    *out = id;
    return 1;
}

static RitualMenuRowFn ritualMenuRowEntryInGame(void) { return (RitualMenuRowFn)siteEntry(SITE_RITUAL_SCREEN_ROW, GAME_PTR(RVA_RITUAL_ROW)); }
static RitualMenuRowFn (*ritualMenuRowEntry)(void) = ritualMenuRowEntryInGame;

static void drawRitualMenuPicker(const RitualMenuScreen *s, int menu) {
    const RitualMenuRow *firstRow = NULL;
    for (int i = 0; i < s->rowCount && !firstRow; i++)
        if (s->rows[i].menu == menu) firstRow = &s->rows[i];
    screenBegin();
    if (firstRow) screenWidth((float)(int)firstRow->v[1]);
    screenTitle(ritualMenuNames[menu]);
    sectionBegin();
    sectionHeader("Rituals");
    RitualMenuRowFn row = ritualMenuRowEntry();
    ritualMenuPicking++;
    for (int i = 0; i < s->rowCount; i++)
        if (s->rows[i].menu == menu) callRitualMenuRow(row, s->rows[i].v);
    ritualMenuPicking--;
    sectionEnd();
    buttonRow();
    listGap(RITUAL_MENU_GAP);
    textButton(RITUAL_MENU_CANCEL, "Cancel");
    if (ESCAPE_KEY()) bindHotkey(RITUAL_MENU_CANCEL, ESCAPE_KEY());
}

static int waitRitualMenuPicker(void) {
    G32(RVA_SCROLL_SLOT) = RITUAL_MENU_SCROLL_SLOT;
    screenLayout();
    int id = GAME_FN(WaitForClickFn, RVA_WAIT_FOR_CLICK)(0, 0);
    G32(RVA_SCROLL_SAVED_POS + RITUAL_MENU_SCROLL_SLOT * 4) = G32(RVA_LAYOUT_WINDOWS + LAYOUT_WINDOW_SCROLL);
    GF(RVA_SCROLL_SAVED_TOP + RITUAL_MENU_SCROLL_SLOT * 4) = GF(RVA_LAYOUT_WINDOWS + LAYOUT_WINDOW_TOP);
    return id;
}

static int ritualMenuCloseAsked(void) {
#ifdef __APPLE__
    return G8(RVA_WINDOW_CLOSE_ASKED) != 0;
#else
    return G32(RVA_WINDOW_CLOSE_ASKED) != 0;
#endif
}

static int runRitualMenuPicker(RitualMenuScreen *s, int menu) {
    int scroll = G32(RVA_LAYOUT_WINDOWS + LAYOUT_WINDOW_SCROLL), resetA = G32(RVA_SCREEN_RESET_A);
    float top = GF(RVA_LAYOUT_WINDOWS + LAYOUT_WINDOW_TOP);
    int rows = 0, castable = 0;
    for (int i = 0; i < s->rowCount; i++)
        if (s->rows[i].menu == menu) rows++, castable += s->rows[i].castable;
    writeLog("ritual_menu_opened", "rows=%d castable=%d menu=%.47s", rows, castable, ritualMenuNames[menu]);
    if (!s->reopened) {
        GF(RVA_SCROLL_SAVED_TOP + RITUAL_MENU_SCROLL_SLOT * 4) = LAYOUT_WINDOW_TOP_UNSET;
        G32(RVA_SCROLL_SAVED_POS + RITUAL_MENU_SCROLL_SLOT * 4) = 0;
    }
    GF(RVA_LAYOUT_WINDOWS + LAYOUT_WINDOW_TOP) = LAYOUT_WINDOW_TOP_UNSET;
    G32(RVA_LAYOUT_WINDOWS + LAYOUT_WINDOW_SCROLL) = G32(RVA_SCROLL_SAVED_POS + RITUAL_MENU_SCROLL_SLOT * 4);
    int id = 0;
    for (;;) {
        drawRitualMenuPicker(s, menu);
        int clicked = waitRitualMenuPicker();
        if (ritualMenuCloseAsked()) {
            writeLog("ritual_menu_closed", "reason=window_close menu=%.47s", ritualMenuNames[menu]);
            break;
        }
        if (ritualMenuPickerAnswer(s, menu, clicked, &id)) {
            if (!id) writeLog("ritual_menu_closed", "reason=cancel menu=%.47s", ritualMenuNames[menu]);
            else if (id <= -RITUAL_ROW_CASTABLE) writeLog("ritual_menu_closed", "reason=description id=%d menu=%.47s", id, ritualMenuNames[menu]);
            else if (id == RITUAL_SCREEN_UNIT_ID || id == -RITUAL_SCREEN_UNIT_ID)
                writeLog("ritual_menu_closed", "reason=unit_screen id=%d menu=%.47s", id, ritualMenuNames[menu]);
            else writeLog("ritual_menu_picked", "id=%d slot=%d menu=%.47s", id, ritualMenuRowSlot(id), ritualMenuNames[menu]);
            break;
        }
    }
    G32(RVA_LAYOUT_WINDOWS + LAYOUT_WINDOW_SCROLL) = scroll;
    GF(RVA_LAYOUT_WINDOWS + LAYOUT_WINDOW_TOP) = top;
    G32(RVA_SCREEN_RESET_A) = resetA;
    return id;
}
static int (*ritualMenuPicker)(RitualMenuScreen *, int) = runRitualMenuPicker;

static int ritualMenuWait(int block, int mode) {
    if (!ritualMenuNameCount || ritualMenuWaitDepth >= RITUAL_SCREEN_DEPTH_MAX) return nextRitualMenuWait(block, mode);
    RitualMenuScreen *s = &ritualMenuScreens[ritualMenuWaitDepth];
    int id = 0;
    ritualMenuWaitDepth++;
    if (!ritualMenuRowsSame(s) && s->redos < RITUAL_MENU_REDOS_MAX) {
        s->redos++;
        writeLog("ritual_menu_frame_again", "rows=%d last=%d lines=%d", s->rowCount, s->lastCount, s->lineCount);
    } else {
        s->redos = 0;
        int reopen = s->reopen, line = -1;
        s->reopen = -1;
        for (int i = 0; reopen >= 0 && i < s->lineCount; i++)
            if (s->lineMenu[i] == reopen) line = i;
        id = line >= 0 ? RITUAL_ROW_MENU + line : nextRitualMenuWait(block, mode);
        int clicked = id - RITUAL_ROW_MENU;
        s->reopened = line >= 0;
        if (clicked >= 0 && clicked < s->lineCount) id = ritualMenuPicker(s, s->lineMenu[clicked]);
        s->reopened = 0;
    }
    endRitualMenuFrame(s);
    ritualMenuWaitDepth--;
    return id;
}

static void installRitualMenus(void) {
    for (int i = 0; i < RITUAL_SCREEN_DEPTH_MAX; i++) ritualMenuScreens[i].reopen = -1;
    int word = apiAddModCommand("ritualmenu", ritualmenuLine);
    int row = insertWrap(SITE_RITUAL_SCREEN_ROW, (void *)ritualMenuRow, (void **)&nextRitualMenuRow, (WrapPlace){.priority = INT_MIN},
                         "coe5se", NULL) &&
              insertWrap(SITE_RITUAL_SCREEN_ROW, (void *)recordRitualMenuGameRow, (void **)&nextRitualMenuGameRow,
                         (WrapPlace){.priority = WRAP_PRIORITY_DEFAULT, .layer = WRAP_LAYER_OUTERMOST}, "coe5se", NULL);
    int wait = insertWrap(SITE_RITUAL_SCREEN_WAIT, (void *)ritualMenuWait, (void **)&nextRitualMenuWait,
                          (WrapPlace){.priority = WRAP_PRIORITY_DEFAULT - 1, .layer = WRAP_LAYER_OUTERMOST}, "coe5se", NULL);
    writeResultLog(word && row && wait, "ritual_menus_installed", "word=%d row=%d wait=%d", word, row, wait);
}
