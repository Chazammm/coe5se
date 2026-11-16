#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    int id;
    const char *name;
    uint64_t requiredFlags;
} TestFieldTerrainPolicy;

static const TestFieldTerrainPolicy testFieldTerrainPolicies[] = {
    {0, "Plain", UINT64_C(0)}, {2, "Forest", UINT64_C(0x2000)}, {12, "Mountains", UINT64_C(0x10000)}, {107, "Swamp", UINT64_C(0x20000000)}, {117, "Desert", UINT64_C(0x8)},
};

#define TEST_FIELD_TERRAIN_POLICY_COUNT (sizeof testFieldTerrainPolicies / sizeof testFieldTerrainPolicies[0])

static const TestFieldTerrainPolicy *findTestFieldTerrainPolicy(int id) {
    for (size_t i = 0; i < TEST_FIELD_TERRAIN_POLICY_COUNT; i++)
        if (testFieldTerrainPolicies[i].id == id) return &testFieldTerrainPolicies[i];
    return NULL;
}

static int testFieldTerrainPolicyMatches(const TestFieldTerrainPolicy *policy, const char *name, uint64_t flags, unsigned int battleFlags) {
    if (!policy || !name || strcmp(name, policy->name)) return 0;
    if (policy->id == 0 || policy->id == 2) return 1;
    const uint64_t forbidden =
        UINT64_C(0x1) | UINT64_C(0x4) | UINT64_C(0x10) | UINT64_C(0x100000) | UINT64_C(0x8000000) | UINT64_C(0x20000000000000);
    return (flags & policy->requiredFlags) == policy->requiredFlags && !(flags & forbidden) && battleFlags == 0;
}
