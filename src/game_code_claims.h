#pragma once

#define GAME_CODE_CLAIMS_MAX 192
#ifdef __APPLE__
#define SITE_CALL_BYTES  4
#define SEAL_SWITCH_BYTES 4
#else
#define SITE_CALL_BYTES  5
#define SEAL_SWITCH_BYTES 9
#endif

typedef struct {
    unsigned rva, size;
    const char *owner;
} GameCodeClaim;
static GameCodeClaim gameCodeClaims[GAME_CODE_CLAIMS_MAX];
static int gameCodeClaimCount;

static int codeRangesOverlap(unsigned a, unsigned an, unsigned b, unsigned bn) { return a < b + bn && b < a + an; }

static const char *gameCodeRangeOwner(unsigned rva, unsigned size) {
    for (int i = 0; i < gameCodeClaimCount; i++)
        if (codeRangesOverlap(rva, size, gameCodeClaims[i].rva, gameCodeClaims[i].size)) return gameCodeClaims[i].owner;
    for (int s = 0; s < SITE_COUNT; s++)
        for (int k = 0; k < MAX_SITE_CALLS && SITES[s].rvas[k]; k++)
            if (codeRangesOverlap(rva, size, SITES[s].rvas[k], SITE_CALL_BYTES)) return SITES[s].name;
    if (codeRangesOverlap(rva, size, RVA_SEAL_EVENT_SWITCH, SEAL_SWITCH_BYTES)) return "seal_events";
#ifndef __APPLE__
    static const unsigned CEREMONY_ROW_JUMPS[] = {RVA_CEREMONY_ROW_WORSHIPPED, RVA_CEREMONY_ROW_NOT_SETTLEMENT, RVA_CEREMONY_ROW_SEAL,
                                                  RVA_CEREMONY_ROW_SEALS_BROKEN, RVA_CEREMONY_ROW_FAVOR};
    for (size_t i = 0; i < sizeof CEREMONY_ROW_JUMPS / sizeof CEREMONY_ROW_JUMPS[0]; i++)
        if (codeRangesOverlap(rva, size, CEREMONY_ROW_JUMPS[i], 5)) return "ceremony_rows";
#endif
    return NULL;
}

static int claimGameCode(unsigned rva, unsigned size, const char *owner) {
    const char *taken = gameCodeRangeOwner(rva, size);
    if (taken || gameCodeClaimCount >= GAME_CODE_CLAIMS_MAX) {
        writeLogError("game_code_claim_refused", "owner=%s rva=0x%x bytes=%u reason=%s taken_by=%s", owner, rva, size,
                      taken ? "overlap" : "too_many_claims", taken ? taken : "-");
        return 0;
    }
    gameCodeClaims[gameCodeClaimCount++] = (GameCodeClaim){rva, size, owner};
    return 1;
}
