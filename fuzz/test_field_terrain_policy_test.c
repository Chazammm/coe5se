#include <assert.h>
#include <stdio.h>
#include "../src/test_field_terrain_policy.h"

int main(void) {
    unsigned checks = 0;
#define CHECK(expression) do { assert(expression); checks++; } while (0)
    CHECK(TEST_FIELD_TERRAIN_POLICY_COUNT == 5);
    CHECK(!testFieldTerrainPolicyFind(-1));
    CHECK(!testFieldTerrainPolicyFind(1));
    CHECK(!testFieldTerrainPolicyFind(10)); /* Lake is not a land candidate. */
    CHECK(!testFieldTerrainPolicyFind(27)); /* Sea remains unsupported. */
    for (size_t i = 0; i < TEST_FIELD_TERRAIN_POLICY_COUNT; i++) {
        const TestFieldTerrainPolicy *p = &testFieldTerrainPolicies[i];
        CHECK(testFieldTerrainPolicyFind(p->id) == p);
        CHECK(!testFieldTerrainPolicyMatches(p, NULL, p->requiredFlags, 0));
        CHECK(!testFieldTerrainPolicyMatches(p, "changed terrain", p->requiredFlags, 0));
        if (i < 2) {
            CHECK(testFieldTerrainPolicyEnabled(p));
            CHECK(testFieldTerrainPolicyMatches(p, p->name, 0, 0));
        } else {
            CHECK(p->verified == 1 && testFieldTerrainPolicyEnabled(p));
            CHECK(testFieldTerrainPolicyMatches(p, p->name, p->requiredFlags, 0));
            CHECK(!testFieldTerrainPolicyMatches(p, p->name, 0, 0));
            CHECK(!testFieldTerrainPolicyMatches(p, p->name, p->requiredFlags | UINT64_C(4), 0));
            CHECK(!testFieldTerrainPolicyMatches(p, p->name, p->requiredFlags | UINT64_C(0x10), 0));
            CHECK(!testFieldTerrainPolicyMatches(p, p->name, p->requiredFlags | UINT64_C(0x20000000000000), 0));
            CHECK(!testFieldTerrainPolicyMatches(p, p->name, p->requiredFlags | UINT64_C(1), 0));
            CHECK(!testFieldTerrainPolicyMatches(p, p->name, p->requiredFlags, 10));
            CHECK(!testFieldTerrainPolicyMatches(p, p->name, p->requiredFlags, 0x200));
        }
    }
    printf("%u terrain policy assertions passed; candidate build = %d; no game was simulated\n", checks, !!COE5SE_TEST_FIELD_TERRAIN_CANDIDATES);
    return 0;
}
