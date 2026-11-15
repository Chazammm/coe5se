#pragma once

// Mac retains the upstream legacy field; the owned editor is not enabled.
static int lTestFieldEditorCapabilities(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldEditorCapabilities");
    static const char *const names[] = {"uniqueGuard", "state", "pointerButtons", "lifecycle",
        "multipleUnique", "emptyEditor", "configuration", "cursedRemoval", "schoolLevelReduction",
        "restoreHigherKnown", "spellRecords", "maxBaseSchoolLevel"};
    lua_newtable(L);
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        lua_pushinteger(L, 0); lua_setfield(L, -2, names[i]);
    }
    lua_pushinteger(L, ITEM_POSITION_MAX); lua_setfield(L, -2, "equipmentPositions");
    return 1;
}

static int lTestFieldTerrains(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldTerrains");
    lua_newtable(L);
    return 1;
}

static int testFieldMacHasEditorOptions(lua_State *L) {
    if (!lua_istable(L, 1)) return 0;
    static const char *const names[] = {"terrain", "emptyEditor", "allowMultipleUnique"};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        lua_pushstring(L, names[i]); lua_rawget(L, 1);
        int present = !lua_isnil(L, -1); lua_pop(L, 1);
        if (present) return 1;
    }
    return 0;
}

static int testFieldMacUnsupported(lua_State *L, const char *call) {
    checkLuaMainThread(L, call);
    refuseLuaCall(runningLuaMod(), call, "windows_only", "owned_editor_options=1");
    lua_pushboolean(L, 0); lua_pushliteral(L, "windows_only");
    return 2;
}

static int lOpenTestFieldMac(lua_State *L) {
    if (testFieldMacHasEditorOptions(L)) return testFieldMacUnsupported(L, "coe5.openTestField");
    return lOpenTestField(L);
}

static int lTestBattleMac(lua_State *L) {
    if (testFieldMacHasEditorOptions(L)) return testFieldMacUnsupported(L, "coe5.testBattle");
    return lTestBattle(L);
}
