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
    {30, "Baron's Castle", UINT64_C(0x1000001), 2821, "greatwall"},
    {124, "King's Castle", UINT64_C(0x400040101), 2849, "kingscastle"},
    {154, "Outpost", UINT64_C(0x1), 1547, "outpost"},
    {199, "Motte and Bailey", UINT64_C(0x100081040), 1546, "palisaden"},
    {235, "Repaired Old Castle", UINT64_C(0x1001001), 1537, "lowwall"},
    {329, "Bronze Castle", UINT64_C(0x1000001), 2853, "widebronzewall"},
    {210, "Granite Citadel", UINT64_C(0x1000001), 2821, "greatwall"},
    {211, "Granite Citadel", UINT64_C(0x1000001), 2821, "greatwall"},
    {212, "Fire Citadel", UINT64_C(0x200001000001), 2597, "widebronzewall"},
    {213, "Fire Citadel", UINT64_C(0x200001000001), 2597, "widebronzewall"},
    {214, "Fire Citadel", UINT64_C(0x200001000001), 2597, "widebronzewall"},
    {215, "Cloud Citadel", UINT64_C(0x4001001001), 66069, "cloudcastle"},
    {216, "Cloud Citadel", UINT64_C(0x4001001001), 66069, "cloudcastle"},
    {217, "Cloud Citadel", UINT64_C(0x4001001001), 66069, "cloudcastle"},
    {218, "Coral Citadel", UINT64_C(0x1000005), 2565, "greatwall"},
    {219, "Coral Citadel", UINT64_C(0x1000005), 2565, "greatwall"},
    {220, "Coral Citadel", UINT64_C(0x1000005), 2565, "greatwall"},
    {223, "Nexus", UINT64_C(0x2000004c01200001), 135941, "greatwall"},
    {78, "Coastal Hamlet", UINT64_C(0x100009000), 43, "coastville"},
    {27, "Sea", UINT64_C(0x4000000000001004), 0, NULL},
    {68, "Deep Sea", UINT64_C(0x4000000000001004), 0, NULL},
};


#define TEST_FIELD_TERRAIN_POLICY_COUNT (sizeof testFieldTerrainPolicies / sizeof testFieldTerrainPolicies[0])

static const TestFieldTerrainPolicy *findTestFieldTerrainPolicy(int id) {
    for (size_t i = 0; i < TEST_FIELD_TERRAIN_POLICY_COUNT; i++)
        if (testFieldTerrainPolicies[i].id == id) return &testFieldTerrainPolicies[i];
    return NULL;
}

static int testFieldTerrainPolicyTower(const TestFieldTerrainPolicy *policy) {
    unsigned selector = policy->battleFlags & 0xffu;
    return selector == 12 || selector == 13;
}

static int testFieldTerrainPolicyWater(const TestFieldTerrainPolicy *policy) {
    return policy && ((policy->requiredFlags & UINT64_C(4)) || (policy->battleFlags & 0xffu) == 43);
}

static int testFieldTerrainPolicyMatches(const TestFieldTerrainPolicy *policy, const char *name, uint64_t flags, unsigned int parts) {
    return policy && name && !strcmp(name, policy->name) && flags == policy->requiredFlags && parts == policy->battleFlags;
}

typedef struct {
    short terrain, ground, temperature;
} TestFieldPlanarTile;

typedef struct {
    int terrain, plane;
    const char *planeName;
    TestFieldPlanarTile squares[9];
} TestFieldPlanarPolicy;

// Local stock battle context observed on the native Elemental Planes.
// This supplies battle terrain, not the preview world's portal network.
static const TestFieldPlanarPolicy testFieldPlanarPolicies[] = {
{210, 8, "Elemental Planes", {{47, 18, 25}, {196, 18, 25}, {204, 18, 25}, {175, 18, 25}, {210, 18, 25}, {47, 18, 25}, {51, 15, 25}, {47, 18, 25}, {196, 18, 25}}},
    {211, 8, "Elemental Planes", {{204, 8, 25}, {51, 15, 25}, {51, 15, 25}, {51, 15, 25}, {211, 18, 25}, {47, 18, 25}, {51, 15, 25}, {51, 15, 25}, {51, 15, 25}}},
    {212, 8, "Elemental Planes", {{198, 22, 25}, {198, 22, 25}, {198, 22, 25}, {198, 22, 25}, {212, 22, 25}, {198, 22, 25}, {198, 22, 25}, {198, 22, 25}, {198, 22, 25}}},
    {213, 8, "Elemental Planes", {{50, 23, 25}, {50, 23, 25}, {198, 22, 25}, {198, 22, 25}, {213, 22, 25}, {198, 22, 25}, {50, 23, 25}, {198, 22, 25}, {198, 22, 25}}},
    {214, 8, "Elemental Planes", {{53, 17, 25}, {53, 17, 25}, {53, 17, 25}, {198, 22, 25}, {214, 17, 25}, {53, 17, 25}, {198, 22, 25}, {53, 17, 25}, {53, 17, 25}}},
    {215, 8, "Elemental Planes", {{194, 21, 25}, {307, 19, 25}, {194, 21, 25}, {194, 21, 25}, {215, 21, 25}, {194, 21, 25}, {194, 21, 25}, {194, 21, 25}, {194, 21, 25}}},
    {216, 8, "Elemental Planes", {{194, 21, 25}, {50, 23, 25}, {50, 23, 25}, {194, 21, 25}, {216, 21, 25}, {50, 23, 25}, {194, 21, 25}, {50, 23, 25}, {50, 23, 25}}},
    {217, 8, "Elemental Planes", {{208, 21, 25}, {194, 21, 25}, {194, 21, 25}, {194, 21, 25}, {217, 21, 25}, {194, 21, 25}, {194, 21, 25}, {194, 21, 25}, {194, 21, 25}}},
    {218, 8, "Elemental Planes", {{68, 12, 25}, {68, 12, 25}, {68, 12, 25}, {68, 12, 25}, {218, 12, 25}, {68, 12, 25}, {68, 12, 25}, {68, 12, 25}, {68, 12, 25}}},
    {219, 8, "Elemental Planes", {{68, 12, 25}, {68, 12, 25}, {68, 12, 25}, {68, 12, 25}, {219, 12, 25}, {68, 12, 25}, {68, 12, 25}, {68, 12, 25}, {54, 10, -10}}},
    {220, 8, "Elemental Planes", {{335, 12, 25}, {68, 12, 25}, {68, 12, 25}, {68, 12, 25}, {220, 12, 25}, {68, 12, 25}, {68, 12, 25}, {68, 12, 25}, {68, 12, 25}}},
    {223, 8, "Elemental Planes", {{204, 8, 25}, {56, 8, 25}, {204, 8, 25}, {56, 8, 25}, {223, 8, 25}, {56, 8, 25}, {204, 8, 25}, {56, 8, 25}, {204, 8, 25}}},
};

static const TestFieldPlanarPolicy *findTestFieldPlanarPolicy(int terrain) {
    for (size_t i = 0; i < sizeof testFieldPlanarPolicies / sizeof testFieldPlanarPolicies[0]; i++)
        if (testFieldPlanarPolicies[i].terrain == terrain) return &testFieldPlanarPolicies[i];
    return NULL;
}

static const TestFieldTerrainPolicy testFieldPlanarNeighbours[] = {
{47, "Rock Ground", UINT64_C(0x20004000001000), 0, NULL},
    {50, "Void", UINT64_C(0x200048001000), 0, NULL},
    {51, "Cave Wall", UINT64_C(0x20200000041000), 0, NULL},
    {53, "Magma", UINT64_C(0x200000041080), 0, NULL},
    {54, "Frozen Waste", UINT64_C(0x1000), 0, NULL},
    {56, "Ash Field", UINT64_C(0x4000001000), 0, NULL},
    {68, "Deep Sea", UINT64_C(0x4000000000001004), 0, NULL},
    {175, "Deep Earth Gem Deposit", UINT64_C(0x20000000040000), 0, NULL},
    {194, "Clouds", UINT64_C(0x4000001080), 0, NULL},
    {196, "Crystal Forest", UINT64_C(0x20002000001080), 0, NULL},
    {198, "Flames", UINT64_C(0x200000001080), 0, NULL},
    {204, "Gateway", UINT64_C(0x4000040000), 0, NULL},
    {208, "Cloud Forest", UINT64_C(0x1200004000001080), 0, NULL},
    {307, "Thunder Cloud", UINT64_C(0x200004000001080), 0, NULL},
    {335, "Enchanted Current", UINT64_C(0x4000000000040084), 0, NULL},
};

static const TestFieldTerrainPolicy *findTestFieldPlanarNeighbour(int terrain) {
    for (size_t i = 0; i < sizeof testFieldPlanarNeighbours / sizeof testFieldPlanarNeighbours[0]; i++)
        if (testFieldPlanarNeighbours[i].id == terrain) return &testFieldPlanarNeighbours[i];
    return NULL;
}
#endif
