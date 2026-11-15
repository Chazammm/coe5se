#pragma once

/* Main-menu test-field text focus. SDL2's existing PollEvent hook calls this
 * before native key translation; committed UTF-8 TEXTINPUT is used, never a
 * guessed keyboard layout or an operating-system keyboard poll. */
#define TEST_FIELD_TEXT_EVENTS 64
#define TEST_FIELD_TEXT_BYTES 32 /* SDL2 SDL_TextInputEvent.text, including NUL */
#define TEST_FIELD_TEXT_SCANCODES 512
#define TFT_TEXT_EDITING 0x302
#define TFT_TEXT_INPUT 0x303
#define TFT_EVENT_SCANCODE 16
#define TFT_EVENT_SYM 20
#define TFT_EVENT_MOD 24
#define TFT_EVENT_REPEAT 13
#define TFT_EVENT_TEXT 12
#define TFT_MOD_CTRL 0x00c0
#define TFT_MOD_ALT 0x0300
#define TFT_MOD_GUI 0x0c00
#define TFT_KEY_F4 0x4000003d
#define TFT_KEY_KP_ENTER 0x40000058

enum { TFT_EDIT_TEXT, TFT_EDIT_BACKSPACE, TFT_EDIT_CLEAR, TFT_EDIT_SUBMIT, TFT_EDIT_CANCEL, TFT_EDIT_BLUR };
typedef struct { int type; char text[TEST_FIELD_TEXT_BYTES]; } TestFieldTextEvent;
typedef struct {
    unsigned keyDown, keyUp, textInput, backspace, clear, submit, cancel;
    int controlSeen, controlSym, controlScan;
    unsigned controlMod, controlRepeat;
} TestFieldTextDiagnostics;
static struct {
    int focused, count, overflow, priorTextInput;
    unsigned char held[TEST_FIELD_TEXT_SCANCODES];
    LuaScript *script;
    char owner[MOD_NAME_MAX];
    TestFieldTextEvent events[TEST_FIELD_TEXT_EVENTS];
    TestFieldTextDiagnostics diagnostics;
    void (*start)(void);
    void (*stop)(void);
    int (*active)(void);
} testFieldText;

static int testFieldTextResolve(void) {
#ifdef __APPLE__
    return 0;
#else
    if (testFieldText.start && testFieldText.stop && testFieldText.active) return 1;
    /* The PollEvent import may already point at the extender's dispatcher.
     * Resolve only the game's already loaded SDL2 module; never load a DLL. */
    HMODULE sdl = GetModuleHandleW(L"SDL2.dll");
    if (!sdl) return 0;
    testFieldText.start = (void (*)(void))(void *)GetProcAddress(sdl, "SDL_StartTextInput");
    testFieldText.stop = (void (*)(void))(void *)GetProcAddress(sdl, "SDL_StopTextInput");
    testFieldText.active = (int (*)(void))(void *)GetProcAddress(sdl, "SDL_IsTextInputActive");
    return testFieldText.start && testFieldText.stop && testFieldText.active;
#endif
}

static void testFieldTextQueue(int type, const char *text) {
    if (testFieldText.count >= TEST_FIELD_TEXT_EVENTS) { testFieldText.overflow = 1; return; }
    TestFieldTextDiagnostics *d = &testFieldText.diagnostics;
    unsigned *counter = type == TFT_EDIT_BACKSPACE ? &d->backspace : type == TFT_EDIT_CLEAR ? &d->clear :
                        type == TFT_EDIT_SUBMIT ? &d->submit : type == TFT_EDIT_CANCEL ? &d->cancel : NULL;
    if (counter && *counter < UINT_MAX) (*counter)++;
    TestFieldTextEvent *event = &testFieldText.events[testFieldText.count++];
    event->type = type;
    snprintf(event->text, sizeof event->text, "%s", text ? text : "");
}

static void testFieldTextBlur(int queueEvent) {
    if (!testFieldText.focused) return;
    testFieldText.focused = 0;
    if (testFieldText.active && testFieldText.start && testFieldText.stop) {
        int active = testFieldText.active() != 0;
        if (testFieldText.priorTextInput && !active) testFieldText.start();
        else if (!testFieldText.priorTextInput && active) testFieldText.stop();
    }
    /* Exactly one bounded numeric summary per focus session. Never log text,
     * printable key symbols, or a history of typed letters. This distinguishes
     * missing injected SDL key events from editor-event handling failures. */
    const TestFieldTextDiagnostics *d = &testFieldText.diagnostics;
    writeLog("test_field_text_focus", "keydown=%u keyup=%u textinput=%u queued_backspace=%u queued_clear=%u queued_submit=%u queued_cancel=%u control_seen=%d control_sym=%d control_scan=%d control_mod=%u control_repeat=%u overflow=%d",
             d->keyDown, d->keyUp, d->textInput, d->backspace, d->clear, d->submit, d->cancel,
             d->controlSeen, d->controlSym, d->controlScan, d->controlMod, d->controlRepeat, testFieldText.overflow);
    if (queueEvent) testFieldTextQueue(TFT_EDIT_BLUR, NULL);
}

static void testFieldTextReset(void) {
    testFieldTextBlur(0);
    testFieldText.count = testFieldText.overflow = 0;
    testFieldText.script = NULL;
    testFieldText.owner[0] = 0;
    /* Keep swallowed scancodes until keyup, including after close/fight, so the
     * held tail of Enter/Escape or a letter cannot become a native shortcut. */
}

static int testFieldTextOwnerAlive(void) {
    for (int i = 0; i < luaScriptCount; i++)
        if (luaScripts[i] == testFieldText.script)
            return !luaScripts[i]->dead && !luaScripts[i]->retired && luaScripts[i]->L;
    return 0;
}

/* SDL2 guarantees valid committed UTF-8, but validate bounded event bytes before
 * presenting them to Lua. This admits German/other Unicode letters and IME
 * commits; control characters, overlong UTF-8 and surrogate values are refused. */
static int testFieldTextValidUtf8(const unsigned char *text) {
    const unsigned char *end = memchr(text, 0, TEST_FIELD_TEXT_BYTES);
    if (!end) return 0;
    while (text < end) {
        unsigned cp = *text++, minimum = 0;
        int tail = 0;
        if (cp < 0x80) { if (cp < 0x20 || cp == 0x7f) return 0; continue; }
        if (cp >= 0xc2 && cp <= 0xdf) cp &= 0x1f, minimum = 0x80, tail = 1;
        else if (cp >= 0xe0 && cp <= 0xef) cp &= 0x0f, minimum = 0x800, tail = 2;
        else if (cp >= 0xf0 && cp <= 0xf4) cp &= 7, minimum = 0x10000, tail = 3;
        else return 0;
        if (end - text < tail) return 0;
        for (int i = 0; i < tail; i++) {
            if ((*text & 0xc0) != 0x80) return 0;
            cp = (cp << 6) | (*text++ & 0x3f);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) || (cp >= 0x80 && cp < 0xa0)) return 0;
    }
    return 1;
}

static int testFieldTextWindowKey(int sym, unsigned mod) {
    return ((mod & TFT_MOD_ALT) && (sym == KEY_TAB || sym == KEY_ENTER || sym == TFT_KEY_F4)) ||
           ((mod & TFT_MOD_GUI) && sym == 'm');
}

static int testFieldTextAnyHeld(void) {
    for (int i = 0; i < TEST_FIELD_TEXT_SCANCODES; i++) if (testFieldText.held[i]) return 1;
    return 0;
}

static int testFieldTextTakesEvent(const void *raw) {
    if (!raw) return 0;
    if (testFieldText.focused && (!testField.open || testField.fighting ||
        strcmp(testFieldText.owner, testField.owner) || !testFieldTextOwnerAlive())) testFieldTextReset();
    const unsigned char *event = raw;
    unsigned type;
    memcpy(&type, event, sizeof type);
    if (type == SDL_WINDOW_EVENT) {
        if (event[SDL_EVENT_WINDOW_EVENT] == SDL_FOCUS_LOST) {
            testFieldTextBlur(1);
            memset(testFieldText.held, 0, sizeof testFieldText.held);
        } else if (event[SDL_EVENT_WINDOW_EVENT] == SDL_FOCUS_GAINED)
            memset(testFieldText.held, 0, sizeof testFieldText.held);
        return 0; /* native window/focus handling must still run */
    }
    if (type == TFT_TEXT_INPUT) {
        if (!testFieldText.focused) return testFieldTextAnyHeld();
        if (testFieldText.diagnostics.textInput < UINT_MAX) testFieldText.diagnostics.textInput++;
        if (testFieldTextValidUtf8(event + TFT_EVENT_TEXT))
            testFieldTextQueue(TFT_EDIT_TEXT, (const char *)event + TFT_EVENT_TEXT);
        else testFieldText.overflow = 1;
        return 1;
    }
    if (type == TFT_TEXT_EDITING) return testFieldText.focused; /* consume IME preedit, commit arrives as TEXTINPUT */
    if (type != SDL_KEY_DOWN && type != SDL_KEY_UP) return 0;
    int scan, sym;
    unsigned short mod;
    memcpy(&scan, event + TFT_EVENT_SCANCODE, sizeof scan);
    memcpy(&sym, event + TFT_EVENT_SYM, sizeof sym);
    memcpy(&mod, event + TFT_EVENT_MOD, sizeof mod);
    if (testFieldText.focused) {
        TestFieldTextDiagnostics *d = &testFieldText.diagnostics;
        unsigned *counter = type == SDL_KEY_DOWN ? &d->keyDown : &d->keyUp;
        if (*counter < UINT_MAX) (*counter)++;
        if (type == SDL_KEY_DOWN && (sym < 0x20 || sym == 0x7f || sym >= 0x40000000)) {
            d->controlSeen = 1; d->controlSym = sym; d->controlScan = scan;
            d->controlMod = mod; d->controlRepeat = event[TFT_EVENT_REPEAT];
        }
    }
    int slot = scan >= 0 && scan < TEST_FIELD_TEXT_SCANCODES;
    if (type == SDL_KEY_UP) {
        int took = slot && testFieldText.held[scan];
        if (slot) testFieldText.held[scan] = 0;
        return took;
    }
    if (!testFieldText.focused) return slot && testFieldText.held[scan];
    if (testFieldTextWindowKey(sym, mod)) return 0;
    if (slot) testFieldText.held[scan] = 1;
    if ((mod & TFT_MOD_CTRL) && !(mod & (TFT_MOD_ALT | TFT_MOD_GUI)) && (sym == KEY_BACKSPACE || sym == 'u'))
        testFieldTextQueue(TFT_EDIT_CLEAR, NULL);
    else if (sym == KEY_BACKSPACE) testFieldTextQueue(TFT_EDIT_BACKSPACE, NULL);
    else if (!event[TFT_EVENT_REPEAT] && (sym == KEY_ENTER || sym == TFT_KEY_KP_ENTER)) {
        testFieldTextQueue(TFT_EDIT_SUBMIT, NULL);testFieldTextBlur(0);
    } else if (!event[TFT_EVENT_REPEAT] && sym == KEY_ESC) {
        testFieldTextQueue(TFT_EDIT_CANCEL, NULL);testFieldTextBlur(0);
    }
    return 1;
}

static int lSetTestFieldTextFocus(lua_State *L) {
    checkLuaMainThread(L, "coe5.setTestFieldTextFocus");
    LuaScript *script = checkLuaScript(L);
    int on = lua_toboolean(L, 1);
    const char *reason = lua_type(L, 1) != LUA_TBOOLEAN ? "bad_type" : NULL;
    if (!reason && testFieldText.owner[0] && strcmp(testFieldText.owner, script->mod)) reason = "not_owner";
    if (!reason && on) {
        reason = !gameCodeMatches ? "game_code_differs" : !testFieldTextPollReady ? "text_input_hook_unavailable" : !testField.open ? "not_open" :
                 strcmp(testField.owner, script->mod) ? "not_owner" : testField.fighting ? "in_fight" :
                 !testFieldTextResolve() ? "text_input_unavailable" : NULL;
    }
    if (reason) {
        refuseLuaCall(script->mod, "coe5.setTestFieldTextFocus", reason, "on=%d", on);
        lua_pushboolean(L, 0);lua_pushstring(L, reason);return 2;
    }
    if (on && !testFieldText.focused) {
        testFieldText.count = testFieldText.overflow = 0;
        memset(&testFieldText.diagnostics, 0, sizeof testFieldText.diagnostics);
        testFieldText.script = script;
        snprintf(testFieldText.owner, sizeof testFieldText.owner, "%s", script->mod);
        testFieldText.priorTextInput = testFieldText.active() != 0;
        testFieldText.start();
        testFieldText.focused = 1;
        memset(keysHeld, 0, sizeof keysHeld);
        memset(keyStateWritten, 0, sizeof keyStateWritten);
        memset(GAME_PTR(RVA_KEY_STATE), 0, KEY_STATE_SIZE);
    } else if (!on) testFieldTextBlur(0);
    lua_pushboolean(L, 1);lua_pushnil(L);return 2;
}

static int lTakeTestFieldTextEvents(lua_State *L) {
    checkLuaMainThread(L, "coe5.takeTestFieldTextEvents");
    LuaScript *script = checkLuaScript(L);
    const char *reason = !testField.open ? "not_open" : strcmp(testField.owner, script->mod) ? "not_owner" : NULL;
    if (reason) {
        refuseLuaCall(script->mod, "coe5.takeTestFieldTextEvents", reason, "count=%d", testFieldText.count);
        lua_pushnil(L);lua_pushstring(L, reason);return 2;
    }
    static const char *const names[] = {"text", "backspace", "clear", "submit", "cancel", "blur"};
    lua_createtable(L, 0, 3);
    lua_pushboolean(L, testFieldText.focused);lua_setfield(L, -2, "focused");
    lua_pushboolean(L, testFieldText.overflow);lua_setfield(L, -2, "overflow");
    lua_createtable(L, testFieldText.count, 0);
    for (int i = 0; i < testFieldText.count; i++) {
        const TestFieldTextEvent *event = &testFieldText.events[i];
        lua_createtable(L, 0, 2);
        lua_pushstring(L, names[event->type]);lua_setfield(L, -2, "type");
        if (event->type == TFT_EDIT_TEXT) { lua_pushstring(L, event->text);lua_setfield(L, -2, "text"); }
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "events");
    testFieldText.count = testFieldText.overflow = 0;
    return 1;
}
