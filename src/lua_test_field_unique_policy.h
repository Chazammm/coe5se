#pragma once

static const char *testFieldUniquePolicyConflict(int *group, int *first, int *second) {
    int records = tableRecords(TABLE_MONSTER);
    if (records < 1 || records > MONSTER_RECORDS_MOVED) return "bad_monster_table";
    int *seen = calloc((size_t)records, sizeof *seen);
    if (!seen) return "out_of_memory";
    const char *reason = NULL;
    for (int unit = 0; unit < UNIT_TABLE_MAX && !reason; unit++) {
        if (!isTestFieldActor(unit)) continue;
        size_t at = (size_t)unit * UNIT_SIZE;
        int monster = G16(RVA_UNIT_MONSTER + at);
        if (G16(RVA_UNIT_HP + at) <= 0 || !isUsedMonster(monster) || monsterAbility(monster, MONSTER_ABILITY_UNIQUE) <= 0) continue;
        int id = uniqueGroupStandIn(monster);
        if (id < 0 || id >= records) reason = "bad_unique_group";
        else seen[id] = -1;
    }
    for (int unit = 0; unit < UNIT_TABLE_MAX && !reason; unit++) {
        size_t at = (size_t)unit * UNIT_SIZE;
        int monster = G16(RVA_UNIT_MONSTER + at);
        if (G16(RVA_UNIT_HP + at) <= 0 || !isUsedMonster(monster) || monsterAbility(monster, MONSTER_ABILITY_UNIQUE) <= 0) continue;
        int id = uniqueGroupStandIn(monster);
        if (id < 0 || id >= records) {
            reason = "bad_unique_group";
        } else if (seen[id] < 0) {
            seen[id] = unit + 1;
        } else if (seen[id]) {
            *group = id;
            *first = seen[id] - 1;
            *second = unit;
            reason = "unique_count";
        }
    }
    free(seen);
    return reason;
}

static int lSetTestFieldMultipleUnique(lua_State *L) {
    const char *call = "coe5.setTestFieldMultipleUnique";
    checkLuaMainThread(L, call);
    LuaScript *script = checkLuaScript(L);
    int enabled = lua_toboolean(L, 1), previous = testField.allowMultipleUnique;
    const char *reason = lua_isboolean(L, 1) ? testFieldPolicyProblem(script) : "bad_allow_multiple_unique";
    int teamA, teamB, group = -1, first = -1, second = -1;
    if (!reason) {
        reason = testFieldFightProblem(script, &teamA, &teamB);
        if (reason && (!strcmp(reason, "missing_team_a") || !strcmp(reason, "missing_team_b"))) reason = NULL;
    }
    if (!reason && !enabled) reason = testFieldUniquePolicyConflict(&group, &first, &second);
    if (!reason && enabled && !uniqueGroups.newUnitCall && !pointTestFieldCall(&testFieldUniqueCall)) reason = "unique_hook_unavailable";
    if (reason) {
        return refuseTestFieldCall(L, call, reason, 0, "requested=%d previous=%d group=%d first_unit=%d second_unit=%d",
                                   enabled, previous, group, first, second);
    }
    if (enabled != previous) testFieldInitialDeploymentReset();
    testField.allowMultipleUnique = testFieldLife.allowMultiple = enabled;
    lua_pushboolean(L, 1);
    return 1;
}
