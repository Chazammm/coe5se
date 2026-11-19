#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#pragma GCC diagnostic push
#ifdef __clang__
#pragma GCC diagnostic ignored "-W#warnings"
#else
#pragma GCC diagnostic ignored "-Wcpp"
#endif
#include <winsock2.h>
#pragma GCC diagnostic pop
typedef SOCKET RelaySocket;
typedef int RelaySockLen;
#define RELAY_NO_SOCKET INVALID_SOCKET
#define closeRelaySocket closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int RelaySocket;
typedef socklen_t RelaySockLen;
#define RELAY_NO_SOCKET (-1)
#define closeRelaySocket close
#endif

#ifndef PRINTF_FORMAT
#define PRINTF_FORMAT(fmtIndex, firstArg) __attribute__((format(printf, fmtIndex, firstArg)))
#endif

#define RELAY_PROTOCOL       1
#define RELAY_SECRET_LEN     8
#define RELAY_MAX_STREAMS    32
#define RELAY_MAX_REFUSED    16
#define RELAY_HEADER         5
#define RELAY_CHUNK          65536
#define RELAY_FRAME_MAX      (RELAY_HEADER + RELAY_CHUNK)
#define RELAY_CODE_MAX       64
#define STEAM_ID_DIGITS      17

enum { RELAY_HELLO = 1, RELAY_WELCOME = 2, RELAY_DATA = 3, RELAY_CLOSE = 4, RELAY_REFUSE = 5 };
enum { RELAY_REFUSED_SECRET = 1, RELAY_REFUSED_PROTOCOL = 2, RELAY_REFUSED_FULL = 3, RELAY_REFUSED_GAME_PORT = 4 };
enum { RELAY_OFF = 0, RELAY_HOST = 1, RELAY_CLIENT = 2 };

typedef struct RelaySteam {
    void *ctx;
    int (*send)(void *ctx, uint64_t to, const void *data, uint32_t size);
    int (*packetSize)(void *ctx, uint32_t *size);
    int (*read)(void *ctx, void *out, uint32_t cap, uint32_t *size, uint64_t *from);
    void (*closeSession)(void *ctx, uint64_t peer);
} RelaySteam;

typedef struct RelayStream {
    uint64_t peer;
    uint32_t id;
    RelaySocket sock;
} RelayStream;

typedef struct Relay {
    int role;
    RelaySteam steam;
    char secret[RELAY_SECRET_LEN + 1];
    uint64_t self, host;
    unsigned short gamePort;
    RelaySocket listener;
    unsigned short loopbackPort;
    uint32_t nextId;
    RelayStream streams[RELAY_MAX_STREAMS];
    int streamCount;
    uint64_t refused[RELAY_MAX_REFUSED];
    volatile int refusedCount;
    int hostRefusal;
    unsigned long long toSteam, fromSteam;
    unsigned char frame[RELAY_FRAME_MAX];
    unsigned char incoming[RELAY_FRAME_MAX];
} Relay;

static const char RELAY_ALPHABET[] = "0123456789abcdefghjkmnpqrstvwxyz";

// ---- The connect code ---------------------------------------------------------------------------

static void makeRelaySecret(char out[RELAY_SECRET_LEN + 1], const unsigned char random[RELAY_SECRET_LEN]) {
    for (int i = 0; i < RELAY_SECRET_LEN; i++) out[i] = RELAY_ALPHABET[random[i] & 31];
    out[RELAY_SECRET_LEN] = '\0';
}

static int isPersonSteamId(uint64_t id) { return (id >> 32) == 0x01100001u && (uint32_t)id != 0; }

static int formatSteamCode(char *out, size_t size, uint64_t id, const char *secret) {
    int n = snprintf(out, size, "%llu-%s", (unsigned long long)id, secret);
    return n > 0 && (size_t)n < size;
}

static int parseSteamCode(const char *text, uint64_t *id, char secret[RELAY_SECRET_LEN + 1]) {
    const char *p = text;
    while (*p == ' ') p++;
    uint64_t value = 0;
    int digits = 0;
    for (; *p >= '0' && *p <= '9'; p++, digits++) {
        if (digits >= STEAM_ID_DIGITS) return 0;
        value = value * 10 + (uint64_t)(*p - '0');
    }
    if (digits != STEAM_ID_DIGITS || *p++ != '-' || !isPersonSteamId(value)) return 0;
    char got[RELAY_SECRET_LEN + 1];
    for (int i = 0; i < RELAY_SECRET_LEN; i++, p++) {
        char c = *p >= 'A' && *p <= 'Z' ? (char)(*p - 'A' + 'a') : *p;
        if (!c || !strchr(RELAY_ALPHABET, c)) return 0;
        got[i] = c;
    }
    got[RELAY_SECRET_LEN] = '\0';
    while (*p == ' ') p++;
    if (*p) return 0;
    *id = value;
    memcpy(secret, got, sizeof got);
    return 1;
}

static int relayJoinChoiceShows(int textOnly, int netPort, int relayPort) {
    if (textOnly) return 0;
    return netPort <= 0 || (relayPort > 0 && netPort == relayPort);
}

// ---- Frames -------------------------------------------------------------------------------------

static size_t encodeRelayFrame(unsigned char *out, size_t cap, int kind, uint32_t id, const void *payload, size_t n) {
    if (cap < RELAY_HEADER || n > cap - RELAY_HEADER) return 0;
    out[0] = (unsigned char)kind;
    for (int i = 0; i < 4; i++) out[1 + i] = (unsigned char)(id >> (8 * i));
    if (n) memcpy(out + RELAY_HEADER, payload, n);
    return RELAY_HEADER + n;
}

static int decodeRelayFrame(const unsigned char *in, size_t n, int *kind, uint32_t *id, const unsigned char **payload, size_t *size) {
    if (n < RELAY_HEADER) return 0;
    *kind = in[0];
    *id = (uint32_t)in[1] | (uint32_t)in[2] << 8 | (uint32_t)in[3] << 16 | (uint32_t)in[4] << 24;
    *payload = in + RELAY_HEADER;
    *size = n - RELAY_HEADER;
    return 1;
}

static int relaySecretMatches(const unsigned char *got, size_t n, const char *secret) {
    unsigned char diff = n != RELAY_SECRET_LEN;
    for (size_t i = 0; i < RELAY_SECRET_LEN; i++) diff |= (unsigned char)((i < n ? got[i] : 0) ^ (unsigned char)secret[i]);
    return !diff;
}

// ---- Sockets ------------------------------------------------------------------------------------

static void tuneRelaySocket(RelaySocket s) {
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
#ifdef SO_NOSIGPIPE
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&one, sizeof one);
#endif
}

static struct sockaddr_in loopbackAddress(unsigned short port) {
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return a;
}

static int sendAllRelay(RelaySocket s, const unsigned char *p, size_t n) {
    while (n) {
        int sent = (int)send(s, (const char *)p, (int)(n > RELAY_CHUNK ? RELAY_CHUNK : n), 0);
        if (sent <= 0) return 0;
        p += sent;
        n -= (size_t)sent;
    }
    return 1;
}

// ---- Streams ------------------------------------------------------------------------------------

static RelayStream *findRelayStream(Relay *r, uint64_t peer, uint32_t id) {
    for (int i = 0; i < r->streamCount; i++)
        if (r->streams[i].peer == peer && r->streams[i].id == id) return &r->streams[i];
    return NULL;
}

static void dropRelayStream(Relay *r, RelayStream *s, int tell, const char *why) {
    unsigned char frame[RELAY_HEADER];
    if (tell) {
        size_t n = encodeRelayFrame(frame, sizeof frame, RELAY_CLOSE, s->id, NULL, 0);
        r->steam.send(r->steam.ctx, s->peer, frame, (uint32_t)n);
    }
    writeLog("steam_relay_stream_closed", "role=%s peer=%llu stream=%u why=%s", r->role == RELAY_HOST ? "host" : "client",
             (unsigned long long)s->peer, s->id, why);
    closeRelaySocket(s->sock);
    *s = r->streams[--r->streamCount];
}

static void sendRelayFrame(Relay *r, uint64_t peer, int kind, uint32_t id, const void *payload, size_t n) {
    size_t size = encodeRelayFrame(r->frame, sizeof r->frame, kind, id, payload, n);
    if (size) r->steam.send(r->steam.ctx, peer, r->frame, (uint32_t)size);
}

static int relayPeerRefused(const Relay *r, uint64_t peer) {
    int n = r->refusedCount;
    for (int i = 0; i < n && i < RELAY_MAX_REFUSED; i++)
        if (r->refused[i] == peer) return 1;
    return 0;
}

static PRINTF_FORMAT(2, 3) void refuseRelayPeer(const char *reason, const char *fmt, ...) {
    char fields[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(fields, sizeof fields, fmt, ap);
    va_end(ap);
    writeLogWarning("steam_relay_refused", "call=steam_relay reason=%s %s", reason, fields);
}

static void refuseRelayStream(Relay *r, uint64_t peer, uint32_t id, int reason, const char *word) {
    unsigned char why = (unsigned char)reason;
    sendRelayFrame(r, peer, RELAY_REFUSE, id, &why, 1);
    refuseRelayPeer(word, "role=host peer=%llu stream=%u", (unsigned long long)peer, id);
    if (reason == RELAY_REFUSED_SECRET || reason == RELAY_REFUSED_PROTOCOL) {
        int n = r->refusedCount;
        if (n < RELAY_MAX_REFUSED) {
            r->refused[n] = peer;
            r->refusedCount = n + 1;
        }
        r->steam.closeSession(r->steam.ctx, peer);
    }
}

static void openHostStream(Relay *r, uint64_t peer, uint32_t id, const unsigned char *payload, size_t n) {
    if (n < 1 || payload[0] != RELAY_PROTOCOL) {
        refuseRelayStream(r, peer, id, RELAY_REFUSED_PROTOCOL, "other_protocol");
        return;
    }
    if (!relaySecretMatches(payload + 1, n - 1, r->secret)) {
        refuseRelayStream(r, peer, id, RELAY_REFUSED_SECRET, "wrong_secret");
        return;
    }
    if (findRelayStream(r, peer, id)) return;
    if (r->streamCount >= RELAY_MAX_STREAMS) {
        refuseRelayStream(r, peer, id, RELAY_REFUSED_FULL, "too_many_streams");
        return;
    }
    RelaySocket s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in to = loopbackAddress(r->gamePort);
    if (s == RELAY_NO_SOCKET || connect(s, (struct sockaddr *)&to, sizeof to) != 0) {
        if (s != RELAY_NO_SOCKET) closeRelaySocket(s);
        refuseRelayStream(r, peer, id, RELAY_REFUSED_GAME_PORT, "game_port_closed");
        return;
    }
    tuneRelaySocket(s);
    r->streams[r->streamCount++] = (RelayStream){peer, id, s};
    sendRelayFrame(r, peer, RELAY_WELCOME, id, NULL, 0);
    writeLog("steam_relay_stream_open", "role=host peer=%llu stream=%u game_port=%u", (unsigned long long)peer, id, r->gamePort);
}

static void takeRelayMessage(Relay *r, uint64_t from, const unsigned char *msg, size_t n) {
    int kind;
    uint32_t id;
    const unsigned char *payload;
    size_t size;
    if (!decodeRelayFrame(msg, n, &kind, &id, &payload, &size)) {
        refuseRelayPeer("short_message", "peer=%llu bytes=%zu", (unsigned long long)from, n);
        return;
    }
    if (r->role == RELAY_CLIENT && from != r->host) {
        refuseRelayPeer("not_the_host", "role=client peer=%llu", (unsigned long long)from);
        return;
    }
    if (r->role == RELAY_HOST && relayPeerRefused(r, from)) return;
    RelayStream *s = findRelayStream(r, from, id);
    switch (kind) {
    case RELAY_HELLO:
        if (r->role == RELAY_HOST) openHostStream(r, from, id, payload, size);
        break;
    case RELAY_WELCOME:
        if (r->role == RELAY_CLIENT && s) writeLog("steam_relay_stream_open", "role=client peer=%llu stream=%u", (unsigned long long)from, id);
        break;
    case RELAY_DATA:
        if (!s) {
            if (r->role == RELAY_HOST) sendRelayFrame(r, from, RELAY_CLOSE, id, NULL, 0);
            break;
        }
        r->fromSteam += size;
        if (!sendAllRelay(s->sock, payload, size)) dropRelayStream(r, s, 1, "game_closed");
        break;
    case RELAY_CLOSE:
        if (s) dropRelayStream(r, s, 0, "other_end_closed");
        break;
    case RELAY_REFUSE:
        if (r->role == RELAY_CLIENT) {
            r->hostRefusal = size ? payload[0] : -1;
            refuseRelayPeer("host_refused", "role=client peer=%llu stream=%u why=%d", (unsigned long long)from, id, r->hostRefusal);
            if (s) dropRelayStream(r, s, 0, "host_refused");
        }
        break;
    default:
        refuseRelayPeer("unknown_kind", "peer=%llu kind=%d", (unsigned long long)from, kind);
        break;
    }
}

static void acceptClientStream(Relay *r) {
    RelaySocket s = accept(r->listener, NULL, NULL);
    if (s == RELAY_NO_SOCKET) return;
    if (r->streamCount >= RELAY_MAX_STREAMS) {
        closeRelaySocket(s);
        refuseRelayPeer("too_many_streams", "role=client streams=%d", r->streamCount);
        return;
    }
    tuneRelaySocket(s);
    uint32_t id = ++r->nextId;
    r->streams[r->streamCount++] = (RelayStream){r->host, id, s};
    unsigned char hello[1 + RELAY_SECRET_LEN];
    hello[0] = RELAY_PROTOCOL;
    memcpy(hello + 1, r->secret, RELAY_SECRET_LEN);
    sendRelayFrame(r, r->host, RELAY_HELLO, id, hello, sizeof hello);
    writeLog("steam_relay_stream_asked", "role=client peer=%llu stream=%u", (unsigned long long)r->host, id);
}

// ---- The two ends -------------------------------------------------------------------------------

static void startRelayHost(Relay *r, RelaySteam steam, uint64_t self, unsigned short gamePort, const char *secret) {
    memset(r, 0, sizeof *r);
    r->role = RELAY_HOST;
    r->steam = steam;
    r->self = self;
    r->gamePort = gamePort;
    r->listener = RELAY_NO_SOCKET;
    snprintf(r->secret, sizeof r->secret, "%s", secret);
}

static int startRelayClient(Relay *r, RelaySteam steam, uint64_t host, const char *secret) {
    memset(r, 0, sizeof *r);
    r->steam = steam;
    r->host = host;
    r->listener = RELAY_NO_SOCKET;
    snprintf(r->secret, sizeof r->secret, "%s", secret);
    RelaySocket s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in at = loopbackAddress(0);
    RelaySockLen len = sizeof at;
    if (s == RELAY_NO_SOCKET || bind(s, (struct sockaddr *)&at, sizeof at) != 0 || listen(s, 4) != 0 ||
        getsockname(s, (struct sockaddr *)&at, &len) != 0) {
        if (s != RELAY_NO_SOCKET) closeRelaySocket(s);
        writeLogError("steam_relay_unavailable", "role=client reason=no_loopback_listener");
        return 0;
    }
    r->listener = s;
    r->loopbackPort = ntohs(at.sin_port);
    r->role = RELAY_CLIENT;
    return 1;
}

static int relayTakesSession(const Relay *r, uint64_t peer) { return r->role == RELAY_HOST && !relayPeerRefused(r, peer); }

static void relaySessionFailed(Relay *r, uint64_t peer) {
    for (int i = r->streamCount - 1; i >= 0; i--)
        if (r->streams[i].peer == peer) dropRelayStream(r, &r->streams[i], 0, "session_failed");
}

static void pumpRelay(Relay *r, int waitMs) {
    uint32_t size;
    while (r->steam.packetSize(r->steam.ctx, &size)) {
        uint64_t from = 0;
        uint32_t got = 0;
        if (size > sizeof r->frame) {
            static unsigned char big[1 << 20];
            if (!r->steam.read(r->steam.ctx, big, sizeof big, &got, &from)) break;
            refuseRelayPeer("long_message", "peer=%llu bytes=%u", (unsigned long long)from, got);
            continue;
        }
        if (!r->steam.read(r->steam.ctx, r->incoming, sizeof r->incoming, &got, &from)) break;
        takeRelayMessage(r, from, r->incoming, got);
    }
    fd_set readable;
    FD_ZERO(&readable);
    RelaySocket top = 0;
    int listening = r->role == RELAY_CLIENT && r->listener != RELAY_NO_SOCKET;
    if (listening) {
        FD_SET(r->listener, &readable);
        top = r->listener;
    }
    for (int i = 0; i < r->streamCount; i++) {
        FD_SET(r->streams[i].sock, &readable);
        if (r->streams[i].sock > top) top = r->streams[i].sock;
    }
    struct timeval wait = {waitMs / 1000, (waitMs % 1000) * 1000};
    if (!listening && !r->streamCount) {
#ifdef _WIN32
        Sleep((DWORD)waitMs);
#else
        select(0, NULL, NULL, NULL, &wait);
#endif
        return;
    }
    if (select((int)top + 1, &readable, NULL, NULL, &wait) <= 0) return;
    if (listening && FD_ISSET(r->listener, &readable)) acceptClientStream(r);
    for (int i = r->streamCount - 1; i >= 0; i--) {
        RelayStream *s = &r->streams[i];
        if (!FD_ISSET(s->sock, &readable)) continue;
        int n = (int)recv(s->sock, (char *)r->frame + RELAY_HEADER, RELAY_CHUNK, 0);
        if (n <= 0) {
            dropRelayStream(r, s, 1, "game_closed");
            continue;
        }
        encodeRelayFrame(r->frame, sizeof r->frame, RELAY_DATA, s->id, NULL, 0);
        r->toSteam += (unsigned long long)n;
        r->steam.send(r->steam.ctx, s->peer, r->frame, (uint32_t)(RELAY_HEADER + n));
    }
}

static void stopRelay(Relay *r, const char *why) {
    if (r->role == RELAY_OFF) return;
    for (int i = r->streamCount - 1; i >= 0; i--) dropRelayStream(r, &r->streams[i], 1, why);
    if (r->listener != RELAY_NO_SOCKET) closeRelaySocket(r->listener);
    writeLog("steam_relay_ended", "role=%s why=%s to_steam=%llu from_steam=%llu", r->role == RELAY_HOST ? "host" : "client", why,
             r->toSteam, r->fromSteam);
    r->listener = RELAY_NO_SOCKET;
    r->role = RELAY_OFF;
}
