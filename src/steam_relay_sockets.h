#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "steam_relay_core.h"

#define STEAM_SOCKETS_VERSION     "SteamNetworkingSockets012"
#define STEAM_UTILS_VERSION       "SteamNetworkingUtils004"

enum {
    SOCKETS_SLOT_CREATE_LISTEN_P2P = 2,
    SOCKETS_SLOT_CONNECT_P2P = 3,
    SOCKETS_SLOT_ACCEPT = 4,
    SOCKETS_SLOT_CLOSE = 5,
    SOCKETS_SLOT_CLOSE_LISTEN = 6,
    SOCKETS_SLOT_SEND = 11,
    SOCKETS_SLOT_RECEIVE = 14,
    SOCKETS_SLOT_RUN_CALLBACKS = 41,
    SOCKETS_SLOT_COUNT = 47,
};
enum { UTILS_SLOT_CHECK_PING = 7, UTILS_SLOT_SET_CONFIG = 16, UTILS_SLOT_GET_CONFIG = 17, UTILS_SLOT_COUNT = 25 };

enum {
    STEAM_CONFIG_SEND_BUFFER = 9,
    STEAM_CONFIG_SEND_RATE_MIN = 10,
    STEAM_CONFIG_SEND_RATE_MAX = 11,
    STEAM_CONFIG_ICE_ENABLE = 104,
    STEAM_CONFIG_STATUS_CALLBACK = 201,
};
enum { STEAM_CONFIG_SCOPE_GLOBAL = 1, STEAM_CONFIG_SCOPE_LISTEN = 3, STEAM_CONFIG_SCOPE_CONNECTION = 4 };
enum { STEAM_CONFIG_INT32 = 1, STEAM_CONFIG_PTR = 5 };
enum { STEAM_GET_CONFIG_OK = 1, STEAM_GET_CONFIG_INHERITED = 2 };
#define STEAM_ICE_DISABLE        0
#define STEAM_SEND_RELIABLE      8
#define STEAM_RESULT_OK          1
#define STEAM_RESULT_LIMIT       25
#define STEAM_IDENTITY_STEAM_ID  16
#define STEAM_INFO_RELAYED       16
#define STEAM_END_APP            1000
#define RELAY_VIRTUAL_PORT       5
#define RELAY_SEND_BUFFER        (16 << 20)
#define RELAY_SEND_RATE          (1 << 20)
#define RELAY_SEND_WAIT_MS       10000
#define RELAY_EVENT_MAX          64

enum {
    STEAM_CONN_NONE = 0,
    STEAM_CONN_CONNECTING = 1,
    STEAM_CONN_FINDING_ROUTE = 2,
    STEAM_CONN_CONNECTED = 3,
    STEAM_CONN_CLOSED_BY_PEER = 4,
    STEAM_CONN_PROBLEM = 5,
};

typedef struct SteamIdentity {
    int32_t type;
    int32_t size;
    unsigned char data[128];
} SteamIdentity;

typedef struct SteamConnectionInfo {
    SteamIdentity remote;
    int64_t userData;
    uint32_t listenSocket;
    unsigned char addressRemote[18];
    uint16_t pad1;
    uint32_t popRemote, popRelay;
    int32_t state;
    int32_t endReason;
    char endDebug[128];
    char description[128];
    int32_t flags;
    uint32_t reserved[63];
} SteamConnectionInfo;

typedef struct SteamConnectionStatus {
    uint32_t conn;
    SteamConnectionInfo info;
    int32_t oldState;
} SteamConnectionStatus;

typedef struct SteamMessage SteamMessage;
struct SteamMessage {
    void *data;
    int32_t size;
    uint32_t conn;
    SteamIdentity peer;
    int64_t connUserData;
    int64_t timeReceived;
    int64_t number;
    void (*freeData)(SteamMessage *);
    void (*release)(SteamMessage *);
    int32_t channel;
    int32_t flags;
    int64_t userData;
    uint16_t lane;
    uint16_t pad1;
};

typedef struct SteamConfigValue {
    int32_t value;
    int32_t dataType;
    union {
        int32_t i32;
        int64_t i64;
        void *ptr;
    } as;
} SteamConfigValue;

_Static_assert(sizeof(SteamIdentity) == 136, "SteamNetworkingIdentity");
_Static_assert(sizeof(SteamConnectionInfo) == 696, "SteamNetConnectionInfo_t");
_Static_assert(offsetof(SteamConnectionInfo, state) == 176 && offsetof(SteamConnectionInfo, flags) == 440, "SteamNetConnectionInfo_t");
_Static_assert(offsetof(SteamConnectionStatus, info) == 8 && offsetof(SteamConnectionStatus, oldState) == 704, "status callback");
_Static_assert(sizeof(SteamMessage) == 216 && offsetof(SteamMessage, release) == 184, "SteamNetworkingMessage_t");
_Static_assert(sizeof(SteamConfigValue) == 16, "SteamNetworkingConfigValue_t");

typedef void (*SteamStatusCallback)(SteamConnectionStatus *);

typedef uint32_t (*SocketsCreateListenP2P)(void *self, int virtualPort, int count, const SteamConfigValue *options);
typedef uint32_t (*SocketsConnectP2P)(void *self, const SteamIdentity *remote, int virtualPort, int count, const SteamConfigValue *options);
typedef int (*SocketsAccept)(void *self, uint32_t conn);
typedef _Bool (*SocketsClose)(void *self, uint32_t conn, int reason, const char *debug, _Bool linger);
typedef _Bool (*SocketsCloseListen)(void *self, uint32_t listenSocket);
typedef int (*SocketsSend)(void *self, uint32_t conn, const void *data, uint32_t size, int flags, int64_t *number);
typedef int (*SocketsReceive)(void *self, uint32_t conn, SteamMessage **out, int max);
typedef void (*SocketsRunCallbacks)(void *self);
typedef _Bool (*UtilsCheckPing)(void *self, float maxAgeSeconds);
typedef _Bool (*UtilsSetConfig)(void *self, int value, int scope, intptr_t object, int dataType, const void *arg);
typedef int (*UtilsGetConfig)(void *self, int value, int scope, intptr_t object, int *dataType, void *out, size_t *size);

#define STEAM_SLOT(object, slot) ((*(void *const *const *)(object))[slot])
#define SOCKETS_CALL(type, rs, slot) ((type)STEAM_SLOT((rs)->sockets, slot))
#define UTILS_CALL(type, rs, slot) ((type)STEAM_SLOT((rs)->utils, slot))

typedef struct RelayPeer {
    uint64_t id;
    uint32_t conn;
    int connected;
} RelayPeer;

typedef struct RelayEvent {
    uint32_t conn, listenSocket;
    int state, oldState, endReason, flags;
    uint64_t peer;
} RelayEvent;

typedef struct RelaySockets {
    void *sockets, *utils;
    Relay *relay;
    uint32_t listenSocket;
    RelayPeer peers[RELAY_MAX_STREAMS];
    int peerCount;
    int nextPeer;
    SteamMessage *held;
    uint64_t heldFrom;
    void (*sleepMs)(int ms);
} RelaySockets;

static RelayEvent relayEvents[RELAY_EVENT_MAX];
static int relayEventCount, relayEventsLost;
static volatile int relayEventLock;

static void lockRelayEvents(void) {
    while (__atomic_test_and_set(&relayEventLock, __ATOMIC_ACQUIRE)) {
    }
}
static void unlockRelayEvents(void) { __atomic_clear(&relayEventLock, __ATOMIC_RELEASE); }

static SteamIdentity steamIdentity(uint64_t id) {
    SteamIdentity s;
    memset(&s, 0, sizeof s);
    s.type = STEAM_IDENTITY_STEAM_ID;
    s.size = sizeof id;
    memcpy(s.data, &id, sizeof id);
    return s;
}

static uint64_t identitySteamId(const SteamIdentity *s) {
    uint64_t id = 0;
    if (s->type == STEAM_IDENTITY_STEAM_ID) memcpy(&id, s->data, sizeof id);
    return id;
}

static void relayConnectionChanged(SteamConnectionStatus *status) {
    RelayEvent e = {status->conn, status->info.listenSocket, status->info.state, status->oldState, status->info.endReason,
                    status->info.flags, identitySteamId(&status->info.remote)};
    lockRelayEvents();
    if (relayEventCount < RELAY_EVENT_MAX) relayEvents[relayEventCount++] = e;
    else relayEventsLost++;
    unlockRelayEvents();
}

#define RELAY_OPTION_COUNT 5
static void relayConnectionOptions(SteamConfigValue out[RELAY_OPTION_COUNT]) {
    memset(out, 0, sizeof(SteamConfigValue) * RELAY_OPTION_COUNT);
    out[0] = (SteamConfigValue){STEAM_CONFIG_ICE_ENABLE, STEAM_CONFIG_INT32, {.i32 = STEAM_ICE_DISABLE}};
    out[1] = (SteamConfigValue){STEAM_CONFIG_STATUS_CALLBACK, STEAM_CONFIG_PTR, {.ptr = (void *)relayConnectionChanged}};
    out[2] = (SteamConfigValue){STEAM_CONFIG_SEND_BUFFER, STEAM_CONFIG_INT32, {.i32 = RELAY_SEND_BUFFER}};
    out[3] = (SteamConfigValue){STEAM_CONFIG_SEND_RATE_MIN, STEAM_CONFIG_INT32, {.i32 = RELAY_SEND_RATE}};
    out[4] = (SteamConfigValue){STEAM_CONFIG_SEND_RATE_MAX, STEAM_CONFIG_INT32, {.i32 = RELAY_SEND_RATE}};
}

static int relayConnectionIce(RelaySockets *rs, uint32_t conn) {
    int32_t value = -2;
    int type = 0;
    size_t size = sizeof value;
    int got = UTILS_CALL(UtilsGetConfig, rs, UTILS_SLOT_GET_CONFIG)(rs->utils, STEAM_CONFIG_ICE_ENABLE, STEAM_CONFIG_SCOPE_CONNECTION,
                                                                    (intptr_t)conn, &type, &value, &size);
    if ((got != STEAM_GET_CONFIG_OK && got != STEAM_GET_CONFIG_INHERITED) || type != STEAM_CONFIG_INT32) return -2;
    return value;
}

static RelayPeer *findRelayPeer(RelaySockets *rs, uint64_t id) {
    for (int i = 0; i < rs->peerCount; i++)
        if (rs->peers[i].id == id) return &rs->peers[i];
    return NULL;
}
static RelayPeer *findRelayConn(RelaySockets *rs, uint32_t conn) {
    for (int i = 0; i < rs->peerCount; i++)
        if (rs->peers[i].conn == conn) return &rs->peers[i];
    return NULL;
}

static void closeRelayConn(RelaySockets *rs, uint32_t conn, const char *debug, int linger) {
    SOCKETS_CALL(SocketsClose, rs, SOCKETS_SLOT_CLOSE)(rs->sockets, conn, STEAM_END_APP, debug, linger != 0);
    RelayPeer *p = findRelayConn(rs, conn);
    if (!p) return;
    if (rs->held && rs->heldFrom == p->id) {
        rs->held->release(rs->held);
        rs->held = NULL;
    }
    *p = rs->peers[--rs->peerCount];
}

// ---- RelaySteam over the connections ------------------------------------------------------------

static int socketsSend(void *ctx, uint64_t to, const void *data, uint32_t size) {
    RelaySockets *rs = (RelaySockets *)ctx;
    RelayPeer *p = findRelayPeer(rs, to);
    if (!p) return 0;
    int result = 0;
    for (int waited = 0; waited <= RELAY_SEND_WAIT_MS; waited += 10) {
        result = SOCKETS_CALL(SocketsSend, rs, SOCKETS_SLOT_SEND)(rs->sockets, p->conn, data, size, STEAM_SEND_RELIABLE, NULL);
        if (result != STEAM_RESULT_LIMIT || !rs->sleepMs) break;
        rs->sleepMs(10);
    }
    if (result == STEAM_RESULT_OK) return 1;
    writeLogWarning("steam_relay_send_failed", "reason=%s peer=%llu bytes=%u result=%d", result == STEAM_RESULT_LIMIT ? "send_buffer_full" : "steam_refused", (unsigned long long)to, size, result);
    return 0;
}

static int socketsPacketSize(void *ctx, uint32_t *size) {
    RelaySockets *rs = (RelaySockets *)ctx;
    for (int n = 0; !rs->held && n < rs->peerCount; n++) {
        RelayPeer *p = &rs->peers[(rs->nextPeer + n) % rs->peerCount];
        SteamMessage *m = NULL;
        if (SOCKETS_CALL(SocketsReceive, rs, SOCKETS_SLOT_RECEIVE)(rs->sockets, p->conn, &m, 1) == 1 && m) {
            rs->held = m;
            rs->heldFrom = p->id;
            rs->nextPeer = (rs->nextPeer + n + 1) % rs->peerCount;
        }
    }
    if (!rs->held) return 0;
    *size = (uint32_t)rs->held->size;
    return 1;
}

static int socketsRead(void *ctx, void *out, uint32_t cap, uint32_t *size, uint64_t *from) {
    RelaySockets *rs = (RelaySockets *)ctx;
    if (!rs->held) return 0;
    uint32_t n = (uint32_t)rs->held->size;
    if (n > cap) n = cap;
    memcpy(out, rs->held->data, n);
    *size = n;
    *from = rs->heldFrom;
    rs->held->release(rs->held);
    rs->held = NULL;
    return 1;
}

static void socketsCloseSession(void *ctx, uint64_t peer) {
    RelaySockets *rs = (RelaySockets *)ctx;
    RelayPeer *p = findRelayPeer(rs, peer);
    if (p) closeRelayConn(rs, p->conn, "refused", 0);
}

static RelaySteam relaySocketsSteam(RelaySockets *rs) { return (RelaySteam){rs, socketsSend, socketsPacketSize, socketsRead, socketsCloseSession}; }

// ---- The two ends -------------------------------------------------------------------------------

static void resetRelaySockets(RelaySockets *rs, void *sockets, void *utils, Relay *relay, void (*sleepMs)(int)) {
    memset(rs, 0, sizeof *rs);
    rs->sockets = sockets;
    rs->utils = utils;
    rs->relay = relay;
    rs->sleepMs = sleepMs;
    lockRelayEvents();
    relayEventCount = relayEventsLost = 0;
    unlockRelayEvents();
    UTILS_CALL(UtilsCheckPing, rs, UTILS_SLOT_CHECK_PING)(rs->utils, 1e10f);
}

static int listenRelaySockets(RelaySockets *rs, void *sockets, void *utils, Relay *relay, void (*sleepMs)(int)) {
    resetRelaySockets(rs, sockets, utils, relay, sleepMs);
    SteamConfigValue options[RELAY_OPTION_COUNT];
    relayConnectionOptions(options);
    rs->listenSocket = SOCKETS_CALL(SocketsCreateListenP2P, rs, SOCKETS_SLOT_CREATE_LISTEN_P2P)(rs->sockets, RELAY_VIRTUAL_PORT, RELAY_OPTION_COUNT, options);
    if (rs->listenSocket) return 1;
    writeLogError("steam_relay_unavailable", "role=host reason=no_listen_socket");
    return 0;
}

static int connectRelaySockets(RelaySockets *rs, void *sockets, void *utils, Relay *relay, uint64_t host, void (*sleepMs)(int)) {
    resetRelaySockets(rs, sockets, utils, relay, sleepMs);
    SteamConfigValue options[RELAY_OPTION_COUNT];
    relayConnectionOptions(options);
    SteamIdentity remote = steamIdentity(host);
    uint32_t conn = SOCKETS_CALL(SocketsConnectP2P, rs, SOCKETS_SLOT_CONNECT_P2P)(rs->sockets, &remote, RELAY_VIRTUAL_PORT, RELAY_OPTION_COUNT, options);
    if (!conn) {
        writeLogError("steam_relay_unavailable", "role=client reason=no_connection host=%llu", (unsigned long long)host);
        return 0;
    }
    int ice = relayConnectionIce(rs, conn);
    if (ice != STEAM_ICE_DISABLE) {
        SOCKETS_CALL(SocketsClose, rs, SOCKETS_SLOT_CLOSE)(rs->sockets, conn, STEAM_END_APP, "ice", 0);
        writeLogError("steam_relay_unavailable", "role=client reason=ice_not_off ice=%d", ice);
        return 0;
    }
    rs->peers[rs->peerCount++] = (RelayPeer){host, conn, 0};
    writeLog("steam_relay_connection", "role=client peer=%llu conn=%u ice=%d", (unsigned long long)host, conn, ice);
    return 1;
}

static void takeRelayConnection(RelaySockets *rs, const RelayEvent *e) {
    if (!relayTakesSession(rs->relay, e->peer) || rs->peerCount >= RELAY_MAX_STREAMS) {
        SOCKETS_CALL(SocketsClose, rs, SOCKETS_SLOT_CLOSE)(rs->sockets, e->conn, STEAM_END_APP, "refused", 0);
        refuseRelayPeer(rs->relay->role != RELAY_HOST ? "not_hosting" : rs->peerCount >= RELAY_MAX_STREAMS ? "too_many_peers" : "refused_earlier",
                        "peer=%llu", (unsigned long long)e->peer);
        return;
    }
    int ice = relayConnectionIce(rs, e->conn);
    if (ice != STEAM_ICE_DISABLE) {
        SOCKETS_CALL(SocketsClose, rs, SOCKETS_SLOT_CLOSE)(rs->sockets, e->conn, STEAM_END_APP, "ice", 0);
        refuseRelayPeer("ice_not_off", "role=host peer=%llu ice=%d", (unsigned long long)e->peer, ice);
        return;
    }
    int result = SOCKETS_CALL(SocketsAccept, rs, SOCKETS_SLOT_ACCEPT)(rs->sockets, e->conn);
    writeLog("steam_relay_session_accepted", "peer=%llu conn=%u ok=%d ice=%d", (unsigned long long)e->peer, e->conn,
             result == STEAM_RESULT_OK, ice);
    if (result != STEAM_RESULT_OK) {
        SOCKETS_CALL(SocketsClose, rs, SOCKETS_SLOT_CLOSE)(rs->sockets, e->conn, STEAM_END_APP, "accept", 0);
        return;
    }
    rs->peers[rs->peerCount++] = (RelayPeer){e->peer, e->conn, 0};
}

static void handleRelayEvent(RelaySockets *rs, const RelayEvent *e) {
    RelayPeer *p = findRelayConn(rs, e->conn);
    switch (e->state) {
    case STEAM_CONN_CONNECTING:
        if (!p && e->listenSocket && e->listenSocket == rs->listenSocket) takeRelayConnection(rs, e);
        break;
    case STEAM_CONN_CONNECTED:
        if (p && !p->connected) {
            p->connected = 1;
            writeLog("steam_relay_session", "peer=%llu relayed=%d", (unsigned long long)p->id, (e->flags & STEAM_INFO_RELAYED) != 0);
        }
        break;
    case STEAM_CONN_CLOSED_BY_PEER:
    case STEAM_CONN_PROBLEM:
        if (!p) break;
        writeLogWarning("steam_relay_session_failed", "reason=%s peer=%llu state=%d end_reason=%d", e->state == STEAM_CONN_CLOSED_BY_PEER ? "closed_by_peer" : "connection_problem", (unsigned long long)p->id, e->state, e->endReason);
        relaySessionFailed(rs->relay, p->id);
        closeRelayConn(rs, e->conn, "closed", 0);
        break;
    default:
        break;
    }
}

static void pollRelaySockets(RelaySockets *rs) {
    SOCKETS_CALL(SocketsRunCallbacks, rs, SOCKETS_SLOT_RUN_CALLBACKS)(rs->sockets);
    RelayEvent events[RELAY_EVENT_MAX];
    lockRelayEvents();
    int n = relayEventCount, lost = relayEventsLost;
    memcpy(events, relayEvents, sizeof events[0] * (size_t)n);
    relayEventCount = relayEventsLost = 0;
    unlockRelayEvents();
    if (lost) writeLogWarning("steam_relay_events_lost", "reason=queue_full count=%d", lost);
    for (int i = 0; i < n; i++) handleRelayEvent(rs, &events[i]);
}

static void closeRelaySockets(RelaySockets *rs) {
    if (rs->held) {
        rs->held->release(rs->held);
        rs->held = NULL;
    }
    while (rs->peerCount) closeRelayConn(rs, rs->peers[rs->peerCount - 1].conn, "ended", 1);
    if (rs->listenSocket) SOCKETS_CALL(SocketsCloseListen, rs, SOCKETS_SLOT_CLOSE_LISTEN)(rs->sockets, rs->listenSocket);
    rs->listenSocket = 0;
}
