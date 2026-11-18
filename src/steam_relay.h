// @platform-split steam_relay: Windows only; the Mac game never loads Steam (its steamwrap_osx.so is x86_64 only)
#pragma once

#ifdef __APPLE__
static void steamRelayInit(void) { writeLog("steam_relay_unavailable", "reason=no_steam_library platform=mac"); }
static void drawSteamCodeCursor(void) {}
#else
#include "steam_relay_sockets.h"

#define STEAM_RELAY_WAIT_MS            10
#define STEAM_RELAY_STOP_WAIT_MS       (RELAY_SEND_WAIT_MS + 5000)
#define STEAM_RELAY_STUB_SIZE          0x20
#define STEAM_RELAY_HOOKS              8
#define STEAM_CONNECT_TITLE            "CoE5SE: Steam connect"
#define STEAM_SERVER_TITLE             "CoE5SE: Game server"
#define STEAM_SERVER_TEXT              "LAN: players connect by IP address, with the port open.\nSteam: players connect by Steam code, with no port open."
#define STEAM_SERVER_LAN               "LAN (open port)"
#define STEAM_SERVER_STEAM             "Steam"
#define STEAM_SERVER_NO_STEAM          "Steam not available.\nA Steam game server needs the game started through Steam."
#define STEAM_SERVER_FAILED            "Steam game server failed"
#define STEAM_JOIN_TITLE               "CoE5SE: Connect"
#define STEAM_JOIN_TEXT                "IP address: the server's address and port.\nSteam code: the code on the server's screen."
#define STEAM_JOIN_IP                  "IP address"
#define STEAM_JOIN_CODE                "Steam code"
#define STEAM_CODE_BOX_TITLE           "CoE5SE: Steam code"
#define STEAM_CODE_NOT_A_CODE          "Not a Steam code"
#define STEAM_CODE_BOX_WIDTH           400.0f
#define STEAM_CODE_BOX_OK              1
#define NET_ADDRESS_RESOLVED_SIZE      50
#define NET_ADDRESS_SIZE               200
#define STEAM_CODE_MASK                "\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2"
#define STEAM_CODE_LINE                "CoE5SE Steam code: %s"
#define STEAM_CODE_REVEAL              "Reveal"
#define STEAM_CODE_HIDE                "Hide"
#define STEAM_CODE_COPY                "Copy"
#define STEAM_CODE_COPIED              "Copied"
#define STEAM_CODE_REVEAL_ID           9310
#define STEAM_CODE_COPY_ID             9311
#define SDLK_V_KEY                     0x76
#define PASTE_MAX                      256
#define STEAM_CODE_FIELD_LOOK          0
#define STEAM_CODE_BOX_GAP             3.0f
#define STEAM_CODE_BOX_CANCEL          2
#define STEAM_CODE_CURSOR_WIDTH        1.0f
#define STEAM_CODE_CURSOR_BOTTOM       0.06f
#define STEAM_CODE_CURSOR_HEIGHT       0.72f

static int copyTextToClipboard(const char *text);
static void drawUiRect(float x, float y, float w, float h, float r, float g, float b, float alpha);
static double measureUiText(const char *text, float size);

static struct SteamApi {
    void *user, *sockets, *utils;
    uint64_t self;
    int (*getUser)(void);
    int (*getPipe)(void);
    void *(*client)(void);
    void *(*clientUser)(void *client, int user, int pipe, const char *version);
    void *(*clientGeneric)(void *client, int user, int pipe, const char *version);
    unsigned char (*loggedOn)(void *user);
    uint64_t (*steamId)(void *user);
} steamApi;

static Relay steamRelay;
static RelaySockets steamSockets;
static HANDLE steamRelayThread;
static volatile LONG steamRelayStopping;
static char steamHostCode[RELAY_CODE_MAX];
static int steamHostCodeRevealed;
static int steamHostCodeCopied;
static int steamServerChosen;
static int steamClientPort;

static void sleepRelayMs(int ms) { Sleep((DWORD)ms); }

static const char *bindSteam(void) {
    if (!G32(RVA_STEAM_CONNECTED)) return "steam_not_connected";
    HMODULE dll = GetModuleHandleA("steam_api64.dll");
    if (!dll) return "no_steam_api";
#define STEAM_CALL(field, name) steamApi.field = (__typeof__(steamApi.field))(void *)GetProcAddress(dll, name)
    STEAM_CALL(getUser, "SteamAPI_GetHSteamUser");
    STEAM_CALL(getPipe, "SteamAPI_GetHSteamPipe");
    STEAM_CALL(client, "SteamClient");
    STEAM_CALL(clientUser, "SteamAPI_ISteamClient_GetISteamUser");
    STEAM_CALL(clientGeneric, "SteamAPI_ISteamClient_GetISteamGenericInterface");
    STEAM_CALL(loggedOn, "SteamAPI_ISteamUser_BLoggedOn");
    STEAM_CALL(steamId, "SteamAPI_ISteamUser_GetSteamID");
#undef STEAM_CALL
    if (!steamApi.getUser || !steamApi.getPipe || !steamApi.client || !steamApi.clientUser || !steamApi.clientGeneric ||
        !steamApi.loggedOn || !steamApi.steamId)
        return "missing_steam_call";
    void *client = steamApi.client();
    int user = steamApi.getUser(), pipe = steamApi.getPipe();
    if (!client || !user || !pipe) return "no_steam_pipe";
    steamApi.user = steamApi.clientUser(client, user, pipe, "SteamUser019");
    steamApi.sockets = steamApi.clientGeneric(client, user, pipe, STEAM_SOCKETS_VERSION);
    steamApi.utils = steamApi.clientGeneric(client, 0, pipe, STEAM_UTILS_VERSION);
    if (!steamApi.utils) steamApi.utils = steamApi.clientGeneric(client, user, pipe, STEAM_UTILS_VERSION);
    if (!steamApi.user) return "no_steam_user";
    if (!steamApi.sockets || !steamApi.utils) {
        writeLogError("steam_relay_no_interface", "reason=old_steam_api sockets=%d utils=%d", steamApi.sockets != NULL, steamApi.utils != NULL);
        return "no_steam_interface";
    }
    if (!steamApi.loggedOn(steamApi.user)) return "steam_offline";
    steamApi.self = steamApi.steamId(steamApi.user);
    if (!isPersonSteamId(steamApi.self)) return "no_steam_id";
    static int winsockStarted;
    WSADATA wsa;
    if (!winsockStarted && WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return "no_winsock";
    winsockStarted = 1;
    return NULL;
}

static const char *steamRelayGameEnded(void) {
    if (isOfficialGame()) return "official_game";
    if (steamRelay.role == RELAY_HOST && !G32(RVA_NET_SERVER)) return "server_closed";
    if (steamRelay.role == RELAY_CLIENT && G32(RVA_NET_CLIENT) <= 0) return "client_closed";
    return NULL;
}

static DWORD WINAPI steamRelayLoop(LPVOID unused) {
    (void)unused;
    const char *why = "stopped";
    while (!steamRelayStopping) {
        const char *ended = steamRelayGameEnded();
        if (ended) {
            why = ended;
            break;
        }
        pollRelaySockets(&steamSockets);
        pumpRelay(&steamRelay, STEAM_RELAY_WAIT_MS);
    }
    stopRelay(&steamRelay, why);
    closeRelaySockets(&steamSockets);
    return 0;
}

static void stopSteamRelay(void) {
    steamHostCode[0] = '\0';
    steamHostCodeRevealed = 0;
    steamHostCodeCopied = 0;
    if (!steamRelayThread) return;
    InterlockedExchange(&steamRelayStopping, 1);
    DWORD waited = WaitForSingleObject(steamRelayThread, STEAM_RELAY_STOP_WAIT_MS);
    CloseHandle(steamRelayThread);
    steamRelayThread = NULL;
    if (waited != WAIT_OBJECT_0) {
        writeLogWarning("steam_relay_stop_timeout", "reason=thread_still_running waited_ms=%d", STEAM_RELAY_STOP_WAIT_MS);
        return;
    }
    InterlockedExchange(&steamRelayStopping, 0);
}

static int startSteamRelayThread(const char *role) {
    steamRelayStopping = 0;
    steamRelayThread = CreateThread(NULL, 0, steamRelayLoop, NULL, 0, NULL);
    if (steamRelayThread) return 1;
    writeLogError("steam_relay_unavailable", "role=%s reason=no_thread error=%lu", role, GetLastError());
    stopRelay(&steamRelay, "no_thread");
    closeRelaySockets(&steamSockets);
    return 0;
}

static int openSteamRelayHost(int port) {
    if (isOfficialGame()) {
        writeLog("steam_relay_unavailable", "role=host reason=official_game");
        return 0;
    }
    const char *reason = bindSteam();
    if (reason) {
        writeLog("steam_relay_unavailable", "role=host reason=%s", reason);
        return 0;
    }
    stopSteamRelay();
    unsigned char random[RELAY_SECRET_LEN];
    if (!BCRYPT_SUCCESS(BCryptGenRandom(NULL, random, sizeof random, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
        writeLogError("steam_relay_unavailable", "role=host reason=no_random_bytes");
        return 0;
    }
    char secret[RELAY_SECRET_LEN + 1];
    makeRelaySecret(secret, random);
    SecureZeroMemory(random, sizeof random);
    startRelayHost(&steamRelay, relaySocketsSteam(&steamSockets), steamApi.self, (unsigned short)port, secret);
    if (!listenRelaySockets(&steamSockets, steamApi.sockets, steamApi.utils, &steamRelay, sleepRelayMs)) {
        stopRelay(&steamRelay, "no_listen_socket");
        return 0;
    }
    if (!startSteamRelayThread("host")) return 0;
    formatSteamCode(steamHostCode, sizeof steamHostCode, steamApi.self, secret);
    SecureZeroMemory(secret, sizeof secret);
    writeLog("steam_relay_host_open", "steam_id=%llu game_port=%d", (unsigned long long)steamApi.self, port);
    return 1;
}

static int openSteamRelayClient(uint64_t host, const char *secret) {
    const char *reason = isOfficialGame() ? "official_game" : bindSteam();
    if (!reason && host == steamApi.self) reason = "own_code";
    if (reason) {
        writeLog("steam_relay_unavailable", "role=client reason=%s", reason);
        okBox(STEAM_CONNECT_TITLE, !strcmp(reason, "own_code") ? "Connect code of this Steam account" :
                                   "Steam not available.\nA connect code needs the game started through Steam.");
        return 0;
    }
    stopSteamRelay();
    if (!startRelayClient(&steamRelay, relaySocketsSteam(&steamSockets), host, secret)) {
        okBox(STEAM_CONNECT_TITLE, "Steam connect failed");
        return 0;
    }
    if (!connectRelaySockets(&steamSockets, steamApi.sockets, steamApi.utils, &steamRelay, host, sleepRelayMs)) {
        stopRelay(&steamRelay, "no_connection");
        okBox(STEAM_CONNECT_TITLE, "Steam connect failed");
        return 0;
    }
    if (!startSteamRelayThread("client")) {
        okBox(STEAM_CONNECT_TITLE, "Steam connect failed");
        return 0;
    }
    writeLog("steam_relay_client_open", "steam_id=%llu host=%llu loopback_port=%u", (unsigned long long)steamApi.self,
             (unsigned long long)host, steamRelay.loopbackPort);
    return 1;
}

static void exitDisconnectHook(void) {
    if (steamRelayThread) writeLog("steam_relay_exit", "role=%s", steamRelay.role == RELAY_HOST ? "host" : "client");
    stopSteamRelay();
    GAME_FN(void (*)(void), RVA_DISCONNECT)();
}

// ---- The host: Start Game Server's choice, the code's box, the server screen ---------------------

static int freeServerPort(void) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;
    struct sockaddr_in at;
    memset(&at, 0, sizeof at);
    at.sin_family = AF_INET;
    at.sin_addr.s_addr = htonl(INADDR_ANY);
    int len = sizeof at, port = 0;
    if (bind(s, (struct sockaddr *)&at, sizeof at) == 0 && getsockname(s, (struct sockaddr *)&at, &len) == 0) port = ntohs(at.sin_port);
    closesocket(s);
    return port;
}

static int (*nextSteamNumberBox)(const char *title, const char *prompt, unsigned long long value);

static int setupServerPortBoxHook(const char *title, const char *prompt, unsigned long long value) {
    uintptr_t returnTo = *(const uintptr_t *)(siteCallerSp(SITE_NUMBER_BOX) - sizeof(uintptr_t));
    if (returnTo != (uintptr_t)(gameBase + RVA_SETUP_SERVER_PORT_BOX_CALL + 5)) return nextSteamNumberBox(title, prompt, value);
    steamServerChosen = 0;
    for (;;) {
        int pick = messageBox(STEAM_SERVER_TITLE, STEAM_SERVER_TEXT, STEAM_SERVER_LAN, STEAM_SERVER_STEAM, 1, 0, 0);
        if (pick == 0) return -1;
        if (pick == 1) {
            writeLog("steam_relay_server_choice", "choice=lan");
            return nextSteamNumberBox(title, prompt, value);
        }
        const char *reason = isOfficialGame() ? "official_game" : bindSteam();
        int freePort = reason ? 0 : freeServerPort();
        if (!reason && !freePort) reason = "no_free_port";
        if (!reason) {
            steamServerChosen = 1;
            writeLog("steam_relay_server_choice", "choice=steam port=%d", freePort);
            return freePort;
        }
        writeLog("steam_relay_unavailable", "role=host reason=%s", reason);
        okBox(STEAM_SERVER_TITLE, strcmp(reason, "no_free_port") ? STEAM_SERVER_NO_STEAM : STEAM_SERVER_FAILED);
    }
}

static void drawSteamCode(void) {
    GAME_FN(void (*)(int, const char *, ...), RVA_TEXT_BUTTON)(0, STEAM_CODE_LINE, steamHostCodeRevealed ? steamHostCode : STEAM_CODE_MASK);
    textButton(STEAM_CODE_REVEAL_ID, steamHostCodeRevealed ? STEAM_CODE_HIDE : STEAM_CODE_REVEAL);
    textButton(STEAM_CODE_COPY_ID, steamHostCodeCopied ? STEAM_CODE_COPIED : STEAM_CODE_COPY);
}

static int takeSteamCodeClick(int id) {
    if (id != STEAM_CODE_REVEAL_ID && id != STEAM_CODE_COPY_ID) return 0;
    if (id == STEAM_CODE_REVEAL_ID) {
        steamHostCodeRevealed = !steamHostCodeRevealed;
        writeLog("steam_relay_code_revealed", "shown=%d", steamHostCodeRevealed);
        return 1;
    }
    int ok = steamHostCode[0] && copyTextToClipboard(steamHostCode);
    steamHostCodeCopied = ok;
    if (ok) writeLog("steam_relay_code_copied", "ok=1");
    else writeLogWarning("steam_relay_code_copied", "ok=0 reason=clipboard_refused");
    return 1;
}

static void showSteamCodeBox(void) {
    screenMusic();
    for (;;) {
        screenBegin();
        screenWidth(STEAM_CODE_BOX_WIDTH);
        screenTitle(STEAM_SERVER_TITLE);
        drawSteamCode();
        listGap(NAME_BOX_BUTTON_GAP);
        textButton(STEAM_CODE_BOX_OK, "Ok");
        bindHotkey(STEAM_CODE_BOX_OK, UI_KEY_ENTER);
        int id = screenWait();
        if (id == STEAM_CODE_BOX_OK) return;
        takeSteamCodeClick(id);
    }
}

static int setupServerSocketHook(int port, int listen) {
    int ok = GAME_FN(int (*)(int, int), RVA_NET_SOCKET_INIT)(port, listen);
    int steam = steamServerChosen;
    steamServerChosen = 0;
    if (!ok || !listen || !steam) return ok;
    if (openSteamRelayHost(port)) showSteamCodeBox();
    else okBox(STEAM_SERVER_TITLE, STEAM_SERVER_FAILED);
    return ok;
}

static void serverConnectionsLineHook(int id, const char *format, int connections) {
    GAME_FN(void (*)(int, const char *, ...), RVA_TEXT_BUTTON)(id, format, connections);
    if (!steamHostCode[0] || steamRelay.role != RELAY_HOST) return;
    drawSteamCode();
}

static int serverScreenWaitHook(int until, int mode) {
    int id = GAME_FN(WaitForClickFn, RVA_WAIT_FOR_CLICK)(until, mode);
    return takeSteamCodeClick(id) ? 0 : id;
}

// ---- The joining player's address box ------------------------------------------------------------

static struct {
    int open;
    char *field;
    int cap;
    char code[RELAY_CODE_MAX];
    int lookup;
} addressBox;

static void moveAddressCursorToEnd(void) { G32(RVA_TEXT_FIELD_CURSOR) = (int)strlen(addressBox.field); }

static void holdAddressCode(const char *code) {
    snprintf(addressBox.code, sizeof addressBox.code, "%s", code);
    snprintf(addressBox.field, (size_t)addressBox.cap, "%s", STEAM_CODE_MASK);
    moveAddressCursorToEnd();
}

static void dropAddressCode(void) { SecureZeroMemory(addressBox.code, sizeof addressBox.code); }

static void syncAddressField(void) {
    uint64_t host;
    char secret[RELAY_SECRET_LEN + 1];
    size_t maskLen = sizeof STEAM_CODE_MASK - 1;
    if (addressBox.code[0]) {
        if (!strcmp(addressBox.field, STEAM_CODE_MASK)) return;
        dropAddressCode();
        if (!strncmp(addressBox.field, STEAM_CODE_MASK, maskLen))
            memmove(addressBox.field, addressBox.field + maskLen, strlen(addressBox.field + maskLen) + 1);
        else addressBox.field[0] = '\0';
        moveAddressCursorToEnd();
        return;
    }
    if (parseSteamCode(addressBox.field, &host, secret)) holdAddressCode(addressBox.field);
    SecureZeroMemory(secret, sizeof secret);
}

static int readClipboardText(char *out, size_t size) {
    out[0] = '\0';
    if (!OpenClipboard(NULL)) return 0;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    const WCHAR *wide = data ? (const WCHAR *)GlobalLock(data) : NULL;
    int n = wide ? WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, (int)size, NULL, NULL) : 0;
    if (wide) GlobalUnlock(data);
    CloseClipboard();
    if (n <= 0) out[size - 1] = '\0';
    return n > 0;
}

static void pasteIntoAddressBox(void) {
    char text[PASTE_MAX];
    if (!readClipboardText(text, sizeof text)) {
        writeLog("steam_relay_paste", "code=0 bytes=0");
        return;
    }
    char *start = text;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
    size_t n = strlen(start);
    while (n && (start[n - 1] == ' ' || start[n - 1] == '\t' || start[n - 1] == '\r' || start[n - 1] == '\n')) start[--n] = '\0';
    uint64_t host;
    char secret[RELAY_SECRET_LEN + 1];
    int code = parseSteamCode(start, &host, secret);
    SecureZeroMemory(secret, sizeof secret);
    if (code) holdAddressCode(start);
    else {
        if (addressBox.code[0]) {
            dropAddressCode();
            addressBox.field[0] = '\0';
        }
        size_t at = strlen(addressBox.field), room = addressBox.cap > 2 ? (size_t)addressBox.cap - 2 : 0;
        for (const char *c = start; *c && at < room; c++)
            if (*c >= 32 && *c <= 126) addressBox.field[at++] = *c;
        addressBox.field[at] = '\0';
        moveAddressCursorToEnd();
    }
    writeLog("steam_relay_paste", "code=%d bytes=%zu", code, n);
    SecureZeroMemory(text, sizeof text);
}

static int (*nextSteamPoll)(void *event);

static int steamPollHook(void *event) {
    int got;
    for (;;) {
        got = nextSteamPoll(event);
        if (!addressBox.open || !got || !event) break;
        const unsigned char *e = event;
        unsigned int type;
        int sym;
        unsigned short mod;
        memcpy(&type, e, sizeof type);
        memcpy(&sym, e + SDL_EVENT_SYM, sizeof sym);
        memcpy(&mod, e + SDL_EVENT_MOD, sizeof mod);
        if (type != SDL_KEY_DOWN || sym != SDLK_V_KEY || !(mod & SDL_KMOD_CTRL)) break;
        pasteIntoAddressBox();
    }
    if (addressBox.open) syncAddressField();
    return got;
}

static int gameAddressBox(const char *title, char *address, int maxLen, int allowCancel) {
    return GAME_FN(int (*)(const char *, char *, int, int), RVA_TEXT_ENTRY_BOX)(title, address, maxLen, allowCancel);
}

static int runSteamCodeBox(char *field, int maxLen) {
    textInputStart();
    for (;;) {
        screenBegin();
        CALL_WITH_TEXT_LOOKED_UP(GAME_FN(void (*)(FMT_ARGS), RVA_SCREEN_TITLE)("%s", STEAM_CODE_BOX_TITLE));
        GAME_FN(int (*)(char *, int, int, int, FMT_ARGS), RVA_TEXT_FIELD)(field, maxLen, 1, STEAM_CODE_FIELD_LOOK, "%s", "");
        listGap(STEAM_CODE_BOX_GAP);
        GAME_FN(void (*)(int, const char *), RVA_TEXT_BUTTON)(STEAM_CODE_BOX_OK, "Ok");
        GAME_FN(void (*)(int, const char *), RVA_TEXT_BUTTON)(STEAM_CODE_BOX_CANCEL, "Cancel");
        bindHotkey(STEAM_CODE_BOX_CANCEL, ESCAPE_KEY());
        screenLayout();
        int id = GAME_FN(WaitForClickFn, RVA_WAIT_FOR_CLICK)(WAIT_UNTIL_CLICK, 0);
        if (id != STEAM_CODE_BOX_OK && id != STEAM_CODE_BOX_CANCEL) continue;
        if (id == STEAM_CODE_BOX_CANCEL) field[0] = '\0';
        textInputStop();
        return id == STEAM_CODE_BOX_OK;
    }
}

static void drawSteamCodeCursor(void) {
    if (!addressBox.open) return;
    int slot = G32(RVA_TEXT_FIELD_FOCUS);
    if (slot < 0 || slot > G32(RVA_GUI_LAST_SLOT)) return;
    const unsigned char *e = (const unsigned char *)GAME_PTR(RVA_GUI_TABLE) + (size_t)slot * GUI_STRIDE;
    if (*(const short *)(e + GUI_ELEMENT_KIND) != GUI_KIND_TEXT_FIELD || *(char *const *)(e + GUI_ELEMENT_VALUE) != addressBox.field)
        return;
    const float *at = (const float *)e;
    float size = at[GUI_ELEMENT_HEIGHT / 4];
    size_t len = strlen(addressBox.field);
    int cursor = G32(RVA_TEXT_FIELD_CURSOR);
    size_t before = cursor < 0 ? 0 : (size_t)cursor > len ? len : (size_t)cursor;
    char text[NET_ADDRESS_SIZE];
    snprintf(text, sizeof text, "%.*s", (int)before, addressBox.field);
    double width = before ? measureUiText(text, size) : 0;
    if (width < 0) return;
    const float *colour = at + GUI_ELEMENT_COLOUR / 4;
    int hovered = G32(RVA_HOVER_SLOT) == slot && !(e[GUI_ELEMENT_NO_HOVER] & 1);
    drawUiRect(at[GUI_ELEMENT_X / 4] + at[GUI_FIELD_LABEL_WIDTH / 4] + TEXT_FIELD_TEXT_INSET + (float)width,
               at[GUI_ELEMENT_Y / 4] + size * STEAM_CODE_CURSOR_BOTTOM, STEAM_CODE_CURSOR_WIDTH, size * STEAM_CODE_CURSOR_HEIGHT,
               hovered ? 1.0f : colour[0], hovered ? 0.0f : colour[1], hovered ? 0.0f : colour[2], colour[3]);
}

static int steamCodeBox(int maxLen) {
    static char field[NET_ADDRESS_SIZE];
    field[0] = '\0';
    int cap = maxLen < TEXT_ENTRY_FIELD_MAX ? maxLen : TEXT_ENTRY_FIELD_MAX;
    for (;;) {
        addressBox.open = 1;
        addressBox.field = field;
        addressBox.cap = cap;
        dropAddressCode();
        syncAddressField();
        int ok = runSteamCodeBox(field, cap);
        addressBox.open = 0;
        char code[RELAY_CODE_MAX];
        snprintf(code, sizeof code, "%s", addressBox.code);
        dropAddressCode();
        uint64_t host;
        char secret[RELAY_SECRET_LEN + 1];
        int isCode = code[0] && parseSteamCode(code, &host, secret);
        SecureZeroMemory(code, sizeof code);
        if (!ok) {
            SecureZeroMemory(secret, sizeof secret);
            field[0] = '\0';
            return -1;
        }
        if (!isCode) {
            writeLog("steam_relay_unavailable", "role=client reason=not_a_code");
            okBox(STEAM_CODE_BOX_TITLE, STEAM_CODE_NOT_A_CODE);
            continue;
        }
        field[0] = '\0';
        int opened = openSteamRelayClient(host, secret);
        SecureZeroMemory(secret, sizeof secret);
        return opened;
    }
}

static int addressBoxHook(const char *title, char *address, int maxLen, int allowCancel) {
    int shows = relayJoinChoiceShows(G32(RVA_TEXT_ONLY), G32(RVA_NET_PORT), steamClientPort) && !isOfficialGame();
    if (shows) G32(RVA_NET_PORT) = 0;
    steamClientPort = 0;
    addressBox.lookup = 0;
    if (!shows) return gameAddressBox(title, address, maxLen, allowCancel);
    for (;;) {
        int pick = messageBox(STEAM_JOIN_TITLE, STEAM_JOIN_TEXT, STEAM_JOIN_IP, STEAM_JOIN_CODE, 1, 0, 0);
        if (pick == 0) {
            address[0] = '\0';
            return 0;
        }
        if (pick == 1) {
            writeLog("steam_relay_join_choice", "choice=ip");
            return gameAddressBox(title, address, maxLen, allowCancel);
        }
        writeLog("steam_relay_join_choice", "choice=steam_code");
        int opened = steamCodeBox(maxLen);
        if (opened < 0) continue;
        if (!opened) return 0;
        address[0] = '\0';
        G32(RVA_NET_PORT) = steamClientPort = steamRelay.loopbackPort;
        addressBox.lookup = 1;
        return 1;
    }
}

static int namelookupHook(void *unused, char *out) {
    if (!addressBox.lookup || steamRelay.role != RELAY_CLIENT)
        return GAME_FN(int (*)(void *, char *), RVA_NET_NAMELOOKUP)(unused, out);
    addressBox.lookup = 0;
    snprintf(out, NET_ADDRESS_RESOLVED_SIZE, "%s", "127.0.0.1");
    snprintf((char *)GAME_PTR(RVA_NET_ADDRESS), NET_ADDRESS_SIZE, "%s", STEAM_CODE_MASK);
    return 1;
}

static void steamRelayInit(void) {
    unsigned char *stubs = allocNear(gameBase + RVA_TEXT_SECTION);
    if (!stubs) {
        writeLogError("steam_relay_installed", "ok=0 of %d reason=no_near_page", STEAM_RELAY_HOOKS);
        return;
    }
    makePagesReadExecute(stubs, 0x1000);
    int ok = redirectCall(RVA_EXIT_DISCONNECT_CALL, RVA_DISCONNECT, stubs + 5 * STEAM_RELAY_STUB_SIZE, (void *)exitDisconnectHook,
                          "exit_disconnect", NULL);
    if (!ok) {
        writeInstallLog("steam_relay_installed", ok, STEAM_RELAY_HOOKS);
        return;
    }
    int choice = wrapSite(SITE_NUMBER_BOX, (void *)setupServerPortBoxHook, (void **)&nextSteamNumberBox) != 0;
    ok += choice;
    int host = choice && redirectCall(RVA_SETUP_SERVER_SOCKET_CALL, RVA_NET_SOCKET_INIT, stubs, (void *)setupServerSocketHook,
                                      "setup_server_socket", NULL);
    ok += host;
    if (host) {
        ok += redirectCall(RVA_SERVER_CONNECTIONS_LINE_CALL, RVA_TEXT_BUTTON, stubs + STEAM_RELAY_STUB_SIZE,
                           (void *)serverConnectionsLineHook, "server_connections_line", NULL);
        ok += redirectCall(RVA_SERVER_SCREEN_WAIT_CALL, RVA_WAIT_FOR_CLICK, stubs + 2 * STEAM_RELAY_STUB_SIZE, (void *)serverScreenWaitHook,
                           "server_screen_wait", NULL);
    }
    int lookup = redirectCall(RVA_NET_NAMELOOKUP_CALL, RVA_NET_NAMELOOKUP, stubs + 3 * STEAM_RELAY_STUB_SIZE, (void *)namelookupHook,
                              "net_namelookup", NULL);
    ok += lookup;
    if (lookup) {
        int box = redirectCall(RVA_NET_ADDRESS_BOX_CALL, RVA_TEXT_ENTRY_BOX, stubs + 4 * STEAM_RELAY_STUB_SIZE, (void *)addressBoxHook,
                               "net_address_box", NULL);
        ok += box;
        if (box) ok += wrapSite(SITE_POLL_EVENT, (void *)steamPollHook, (void **)&nextSteamPoll) != 0;
    }
    writeInstallLog("steam_relay_installed", ok, STEAM_RELAY_HOOKS);
}
#endif
