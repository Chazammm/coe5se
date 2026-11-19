#pragma once

#define TEST_FIELD_TEXT_EVENTS 64

enum { TEXT_EDIT_TEXT, TEXT_EDIT_BACKSPACE, TEXT_EDIT_CLEAR, TEXT_EDIT_SUBMIT, TEXT_EDIT_CANCEL, TEXT_EDIT_BLUR };

typedef struct {
    int type;
    char text[SDL_TEXT_BYTES];
} TestFieldTextEvent;

typedef struct {
    unsigned keyDown, keyUp, textInput, backspace, clear, submit, cancel;
    int controlSeen, controlSym, controlScan;
    unsigned controlMod, controlRepeat;
} TestFieldTextCounts;

static struct {
    int focused, screen, count, overflow, priorTextInput;
    unsigned char held[SDL_SCANCODE_COUNT];
    LuaScript *script;
    char owner[MOD_NAME_MAX];
    TestFieldTextEvent events[TEST_FIELD_TEXT_EVENTS];
    TestFieldTextCounts counts;
    void (*start)(void);
    void (*stop)(void);
    int (*active)(void);
} testFieldText;

static int pushTextEvents(lua_State *L);

static int findSdlTextInput(void) {
    if (testFieldText.start && testFieldText.stop && testFieldText.active) return 1;
    testFieldText.start = (void (*)(void))SDL_FUNCTION("SDL_StartTextInput", RVA_SDL_START_TEXT_INPUT_SLOT);
    testFieldText.stop = (void (*)(void))SDL_FUNCTION("SDL_StopTextInput", RVA_SDL_STOP_TEXT_INPUT_SLOT);
    testFieldText.active = (int (*)(void))SDL_FUNCTION("SDL_IsTextInputActive", RVA_SDL_IS_TEXT_INPUT_ACTIVE_SLOT);
    return testFieldText.start && testFieldText.stop && testFieldText.active;
}

static void queueTestFieldTextEvent(int type, const char *text) {
    if (testFieldText.count >= TEST_FIELD_TEXT_EVENTS) {
        testFieldText.overflow = 1;
        return;
    }
    TestFieldTextCounts *c = &testFieldText.counts;
    unsigned *counter = type == TEXT_EDIT_BACKSPACE ? &c->backspace
                      : type == TEXT_EDIT_CLEAR     ? &c->clear
                      : type == TEXT_EDIT_SUBMIT    ? &c->submit
                      : type == TEXT_EDIT_CANCEL    ? &c->cancel
                                                    : NULL;
    if (counter && *counter < UINT_MAX) (*counter)++;
    TestFieldTextEvent *event = &testFieldText.events[testFieldText.count++];
    event->type = type;
    snprintf(event->text, sizeof event->text, "%s", text ? text : "");
}

static void blurTestFieldText(int queueEvent) {
    if (!testFieldText.focused) return;
    testFieldText.focused = 0;
    if (testFieldText.active && testFieldText.start && testFieldText.stop) {
        int active = testFieldText.active() != 0;
        if (testFieldText.priorTextInput && !active) testFieldText.start();
        else if (!testFieldText.priorTextInput && active) testFieldText.stop();
    }
    const TestFieldTextCounts *c = &testFieldText.counts;
    writeLog("test_field_text_focus",
             "keydown=%u keyup=%u textinput=%u queued_backspace=%u queued_clear=%u queued_submit=%u queued_cancel=%u control_seen=%d control_sym=%d "
             "control_scan=%d control_mod=%u control_repeat=%u overflow=%d",
             c->keyDown, c->keyUp, c->textInput, c->backspace, c->clear, c->submit, c->cancel, c->controlSeen, c->controlSym, c->controlScan, c->controlMod,
             c->controlRepeat, testFieldText.overflow);
    if (queueEvent) queueTestFieldTextEvent(TEXT_EDIT_BLUR, NULL);
}

static void resetTestFieldText(void) {
    blurTestFieldText(0);
    testFieldText.count = testFieldText.overflow = testFieldText.screen = 0;
    testFieldText.script = NULL;
    testFieldText.owner[0] = 0;
}

static int testFieldTextOwnerAlive(void) {
    for (int i = 0; i < luaScriptCount; i++)
        if (luaScripts[i] == testFieldText.script) return !luaScripts[i]->dead && !luaScripts[i]->retired && luaScripts[i]->L;
    return 0;
}

static int isCommittedUtf8(const unsigned char *text) {
    const unsigned char *end = memchr(text, 0, SDL_TEXT_BYTES);
    if (!end) return 0;
    while (text < end) {
        unsigned cp = *text++, minimum = 0;
        int tail = 0;
        if (cp < 0x80) {
            if (cp < 0x20 || cp == 0x7f) return 0;
            continue;
        }
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

static int isWindowKey(int sym, unsigned mod) {
    return ((mod & SDL_KMOD_ALT) && (sym == KEY_TAB || sym == KEY_ENTER || sym == SDLK_F4)) || ((mod & SDL_KMOD_GUI) && sym == SDLK_M);
}

static int testFieldTextAnyHeld(void) {
    for (int i = 0; i < SDL_SCANCODE_COUNT; i++)
        if (testFieldText.held[i]) return 1;
    return 0;
}

static int testFieldTextTakesEvent(const void *raw) {
    if (!raw) return 0;
    if (testFieldText.screen && !testFieldText.focused && uiWaitScript != testFieldText.script) resetTestFieldText();
    if (testFieldText.focused && (testFieldText.screen ? uiWaitScript != testFieldText.script || !testFieldTextOwnerAlive()
                                                       : !testField.open || testField.fighting || strcmp(testFieldText.owner, testField.owner) || !testFieldTextOwnerAlive()))
        resetTestFieldText();
    const unsigned char *event = raw;
    unsigned type;
    memcpy(&type, event, sizeof type);
    if (type == SDL_WINDOW_EVENT) {
        if (event[SDL_EVENT_WINDOW_EVENT] == SDL_FOCUS_LOST) {
            blurTestFieldText(1);
            memset(testFieldText.held, 0, sizeof testFieldText.held);
        } else if (event[SDL_EVENT_WINDOW_EVENT] == SDL_FOCUS_GAINED) memset(testFieldText.held, 0, sizeof testFieldText.held);
        return 0;
    }
    if (type == SDL_TEXT_INPUT) {
        if (!testFieldText.focused) return testFieldTextAnyHeld();
        if (testFieldText.counts.textInput < UINT_MAX) testFieldText.counts.textInput++;
        if (isCommittedUtf8(event + SDL_EVENT_TEXT)) queueTestFieldTextEvent(TEXT_EDIT_TEXT, (const char *)event + SDL_EVENT_TEXT);
        else testFieldText.overflow = 1;
        return 1;
    }
    if (type == SDL_TEXT_EDITING) return testFieldText.focused;
    if (type != SDL_KEY_DOWN && type != SDL_KEY_UP) return 0;
    int scan, sym;
    unsigned short mod;
    memcpy(&scan, event + SDL_EVENT_SCANCODE, sizeof scan);
    memcpy(&sym, event + SDL_EVENT_SYM, sizeof sym);
    memcpy(&mod, event + SDL_EVENT_MOD, sizeof mod);
    if (testFieldText.focused) {
        TestFieldTextCounts *c = &testFieldText.counts;
        unsigned *counter = type == SDL_KEY_DOWN ? &c->keyDown : &c->keyUp;
        if (*counter < UINT_MAX) (*counter)++;
        if (type == SDL_KEY_DOWN && (sym < 0x20 || sym == 0x7f || sym >= 0x40000000)) {
            c->controlSeen = 1;
            c->controlSym = sym;
            c->controlScan = scan;
            c->controlMod = mod;
            c->controlRepeat = event[SDL_EVENT_REPEAT];
        }
    }
    int slot = scan >= 0 && scan < SDL_SCANCODE_COUNT;
    if (type == SDL_KEY_UP) {
        int took = slot && testFieldText.held[scan];
        if (slot) testFieldText.held[scan] = 0;
        return took;
    }
    if (!testFieldText.focused) return slot && testFieldText.held[scan];
    if (isWindowKey(sym, mod)) return 0;
    if (slot) testFieldText.held[scan] = 1;
    if ((mod & SDL_KMOD_CTRL) && !(mod & (SDL_KMOD_ALT | SDL_KMOD_GUI)) && (sym == KEY_BACKSPACE || sym == 'u')) queueTestFieldTextEvent(TEXT_EDIT_CLEAR, NULL);
    else if (sym == KEY_BACKSPACE) queueTestFieldTextEvent(TEXT_EDIT_BACKSPACE, NULL);
    else if (!event[SDL_EVENT_REPEAT] && (sym == KEY_ENTER || sym == SDLK_KP_ENTER)) {
        queueTestFieldTextEvent(TEXT_EDIT_SUBMIT, NULL);
        blurTestFieldText(0);
    } else if (!event[SDL_EVENT_REPEAT] && sym == KEY_ESC) {
        queueTestFieldTextEvent(TEXT_EDIT_CANCEL, NULL);
        blurTestFieldText(0);
    }
    return 1;
}

static void takeTextFocus(LuaScript *script, int screen) {
    testFieldText.count = testFieldText.overflow = 0;
    memset(&testFieldText.counts, 0, sizeof testFieldText.counts);
    testFieldText.script = script;
    testFieldText.screen = screen;
    snprintf(testFieldText.owner, sizeof testFieldText.owner, "%s", script->mod);
    testFieldText.priorTextInput = testFieldText.active() != 0;
    testFieldText.start();
    testFieldText.focused = 1;
    memset(keysHeld, 0, sizeof keysHeld);
    memset(keyStateWritten, 0, sizeof keyStateWritten);
    memset(GAME_PTR(RVA_KEY_STATE), 0, KEY_STATE_SIZE);
}

static int lSetTestFieldTextFocus(lua_State *L) {
    checkLuaMainThread(L, "coe5.setTestFieldTextFocus");
    LuaScript *script = checkLuaScript(L);
    int on = lua_toboolean(L, 1);
    const char *reason = lua_type(L, 1) != LUA_TBOOLEAN ? "bad_type" : NULL;
    if (!reason && testFieldText.owner[0] && strcmp(testFieldText.owner, script->mod)) reason = "not_owner";
    if (!reason && on)
        reason = !gameCodeMatches                        ? "game_code_differs"
               : !testFieldTextPollReady                 ? "text_input_hook_unavailable"
               : !testField.open                         ? "not_open"
               : strcmp(testField.owner, script->mod)   ? "not_owner"
               : testField.fighting                      ? "in_fight"
               : !findSdlTextInput()                     ? "text_input_unavailable"
                                                         : NULL;
    if (reason) return refuseTestFieldCall(L, "coe5.setTestFieldTextFocus", reason, 0, "on=%d", on);
    if (on && testFieldText.focused && testFieldText.screen) resetTestFieldText();
    if (on && !testFieldText.focused) takeTextFocus(script, 0);
    else if (!on) blurTestFieldText(0);
    lua_pushboolean(L, 1);
    lua_pushnil(L);
    return 2;
}

static int lTakeTestFieldTextEvents(lua_State *L) {
    checkLuaMainThread(L, "coe5.takeTestFieldTextEvents");
    LuaScript *script = checkLuaScript(L);
    const char *reason = !testField.open ? "not_open" : strcmp(testField.owner, script->mod) ? "not_owner" : NULL;
    if (reason) return refuseTestFieldCall(L, "coe5.takeTestFieldTextEvents", reason, 1, "count=%d", testFieldText.count);
    return pushTextEvents(L);
}

static int pushTextEvents(lua_State *L) {
    static const char *const NAMES[] = {"text", "backspace", "clear", "submit", "cancel", "blur"};
    lua_createtable(L, 0, 3);
    setBoolField(L, "focused", testFieldText.focused);
    setBoolField(L, "overflow", testFieldText.overflow);
    lua_createtable(L, testFieldText.count, 0);
    for (int i = 0; i < testFieldText.count; i++) {
        const TestFieldTextEvent *event = &testFieldText.events[i];
        lua_createtable(L, 0, 2);
        lua_pushstring(L, NAMES[event->type]);
        lua_setfield(L, -2, "type");
        if (event->type == TEXT_EDIT_TEXT) {
            lua_pushstring(L, event->text);
            lua_setfield(L, -2, "text");
        }
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "events");
    testFieldText.count = testFieldText.overflow = 0;
    return 1;
}

static int screenTextFocusOf(const LuaScript *script) {
    return script && testFieldText.focused && testFieldText.screen && testFieldText.script == script;
}

static int lSetUiTextFocus(lua_State *L) {
    checkLuaMainThread(L, "coe5.ui.setTextFocus");
    LuaScript *script = checkLuaScript(L);
    int on = lua_toboolean(L, 1);
    const char *reason = lua_type(L, 1) != LUA_TBOOLEAN ? "bad_type" : NULL;
    if (!reason && testFieldText.owner[0] && strcmp(testFieldText.owner, script->mod)) reason = "not_owner";
    if (!reason && on)
        reason = !gameCodeMatches          ? "game_code_differs"
               : !testFieldTextPollReady   ? "text_input_hook_unavailable"
               : uiFrameScript != script   ? "no_screen"
               : !findSdlTextInput()       ? "text_input_unavailable"
                                           : NULL;
    if (reason) return refuseTestFieldCall(L, "coe5.ui.setTextFocus", reason, 0, "on=%d", on);
    if (on && testFieldText.focused && !testFieldText.screen) resetTestFieldText();
    if (on && !testFieldText.focused) takeTextFocus(script, 1);
    else if (!on && testFieldText.screen) resetTestFieldText();
    lua_pushboolean(L, 1);
    lua_pushnil(L);
    return 2;
}

static int lTakeUiTextEvents(lua_State *L) {
    checkLuaMainThread(L, "coe5.ui.takeTextEvents");
    LuaScript *script = checkLuaScript(L);
    if (testFieldText.owner[0] && strcmp(testFieldText.owner, script->mod))
        return refuseTestFieldCall(L, "coe5.ui.takeTextEvents", "not_owner", 1, "count=%d", testFieldText.count);
    if (!testFieldText.screen || testFieldText.script != script) {
        lua_createtable(L, 0, 3);
        setBoolField(L, "focused", 0);
        setBoolField(L, "overflow", 0);
        lua_newtable(L);
        lua_setfield(L, -2, "events");
        return 1;
    }
    return pushTextEvents(L);
}
