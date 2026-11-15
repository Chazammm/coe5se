#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Identity comes from the loaded vanilla terrain catalog on the examined 5.43
 * build, not from a guessed enum. The three additional neutral land rows passed
 * the complete e9c934 real-game matrix on 2026-11-14 (two natural fights each,
 * placement, Save/Load, exact Reset/Rematch and verified cleanup). The final
 * ordinary build is tested separately. Future rows must remain disabled until
 * their matrix passes; private probe builds can enable candidates beforehand. */
#ifndef COE5SE_TEST_FIELD_TERRAIN_CANDIDATES
#define COE5SE_TEST_FIELD_TERRAIN_CANDIDATES 0
#endif

typedef struct {
    int id;
    const char *name;
    uint64_t requiredFlags;
    int verified;
} TestFieldTerrainPolicy;

static const TestFieldTerrainPolicy testFieldTerrainPolicies[] = {
    {0, "Plain", UINT64_C(0), 1},
    {2, "Forest", UINT64_C(0x2000), 1},
    {12, "Mountains", UINT64_C(0x10000), 1},
    {107, "Swamp", UINT64_C(0x20000000), 1},
    {117, "Desert", UINT64_C(0x8), 1},
};

#define TEST_FIELD_TERRAIN_POLICY_COUNT (sizeof testFieldTerrainPolicies / sizeof testFieldTerrainPolicies[0])

static const TestFieldTerrainPolicy *testFieldTerrainPolicyFind(int id) {
    for (size_t i = 0; i < TEST_FIELD_TERRAIN_POLICY_COUNT; i++)
        if (testFieldTerrainPolicies[i].id == id) return &testFieldTerrainPolicies[i];
    return NULL;
}

static int testFieldTerrainPolicyEnabled(const TestFieldTerrainPolicy *policy) {
    return policy && (policy->verified || COE5SE_TEST_FIELD_TERRAIN_CANDIDATES);
}

static int testFieldTerrainPolicyMatches(const TestFieldTerrainPolicy *policy, const char *name,
                                         uint64_t flags, unsigned int battleFlags) {
    if (!testFieldTerrainPolicyEnabled(policy) || !name || strcmp(name, policy->name)) return 0;
    /* Plain/Forest retain their existing exact-ID/name behavior. Additional
     * candidates must also retain their characteristic vanilla land flag,
     * native general battle map, and lack water/cave/fort/other-plane tags.
     * These guards do not claim full campaign-equivalent mod detection. */
    if (policy->id == 0 || policy->id == 2) return 1;
    const uint64_t forbidden = UINT64_C(0x1) | UINT64_C(0x4) | UINT64_C(0x10) |
        UINT64_C(0x100000) | UINT64_C(0x8000000) | UINT64_C(0x20000000000000);
    return (flags & policy->requiredFlags) == policy->requiredFlags && !(flags & forbidden) && battleFlags == 0;
}
