#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __APPLE__
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
#else
// Stock Windows surface contexts with verified loaded terrain and map-selection metadata.
typedef struct {
    int id;
    const char *name;
    uint64_t requiredFlags;
    unsigned battleFlags;
    const char *nativeTemplate;
} TestFieldTerrainPolicy;

static const TestFieldTerrainPolicy testFieldTerrainPolicies[] = {
    {0, "Plain", UINT64_C(0x1000), 0, NULL},
    {2, "Forest", UINT64_C(0x2003002), 0, NULL},
    {12, "Mountains", UINT64_C(0x11000), 0, NULL},
    {107, "Swamp", UINT64_C(0x22001000), 0, NULL},
    {117, "Desert", UINT64_C(0x8000000001008), 0, NULL},
    {115, "Jungle", UINT64_C(0x800002001002), 0, NULL},
    {5, "Farm", UINT64_C(0x880), 49, "farm"},
    {18, "Village", UINT64_C(0x100001000), 40, "village"},
    {70, "Hoburg Village", UINT64_C(0x1000001800), 46, "hoburgh"},
    {109, "Tent Village", UINT64_C(0x2100001000), 45, "tentvillage"},
    {98, "Oasis", UINT64_C(0x8), 52, "oasis"},
    {1, "Fortified Hoburg", UINT64_C(0x1000000001), 1546, "palisaden"},
    {20, "Guard tower", UINT64_C(0x1001001), 1548, "stonetower"},
    {72, "Watch Tower", UINT64_C(0x4001080040), 1549, "woodentower"},
    {22, "Castle", UINT64_C(0x1001001), 2821, "greatwall"},
    {23, "Citadel", UINT64_C(0x1000001), 2568, "citadel"},
    {17, "Town", UINT64_C(0x1400100001000), 1574, "town"},
    {9, "City", UINT64_C(0x2400500000001), 1831, "city"},
    {78, "Coastal Hamlet", UINT64_C(0x100009000), 43, "coastville"},
    {27, "Sea", UINT64_C(0x4000000000001004), 0, NULL},
    {68, "Deep Sea", UINT64_C(0x4000000000001004), 0, NULL},
};


#define TEST_FIELD_TERRAIN_POLICY_COUNT (sizeof testFieldTerrainPolicies / sizeof testFieldTerrainPolicies[0])

// Resolve the policy for a loaded terrain ID.
static const TestFieldTerrainPolicy *findTestFieldTerrainPolicy(int id) {
    for (size_t i = 0; i < TEST_FIELD_TERRAIN_POLICY_COUNT; i++)
        if (testFieldTerrainPolicies[i].id == id) return &testFieldTerrainPolicies[i];
    return NULL;
}

// Identify native tower and water contexts using their verified selector.
static int testFieldTerrainPolicyTower(const TestFieldTerrainPolicy *policy) {
    unsigned selector = policy->battleFlags & 0xffu;
    return selector == 12 || selector == 13;
}

static int testFieldTerrainPolicyWater(const TestFieldTerrainPolicy *policy) {
    return policy && ((policy->requiredFlags & UINT64_C(4)) || (policy->battleFlags & 0xffu) == 43);
}

// Only advertise stock definition combinations that the owner can verify on open.
static int testFieldTerrainPolicyMatches(const TestFieldTerrainPolicy *policy, const char *name, uint64_t flags, unsigned int parts) {
    return policy && name && !strcmp(name, policy->name) && flags == policy->requiredFlags && parts == policy->battleFlags;
}
#endif
