#pragma once

static int lLoadedContentOrigin(lua_State *L) {
    checkLuaMainThread(L, "coe5.loadedContentOrigin");
    const char *kind = luaL_checkstring(L, 1);
    lua_Integer id = luaL_checkinteger(L, 2);
    int valid = !strcmp(kind, "item")        ? id >= 0 && id < tableRecords(TABLE_ITEM)
              : !strcmp(kind, "spell")       ? id >= 0 && id < spellTableRecords()
              : !strcmp(kind, "spellschool") ? id >= 0 && id < spellPathCount()
                                             : 0;
    if (!gameCodeMatches || !valid) {
        const char *reason = !gameCodeMatches ? "game_code_differs" : "bad_content";
        refuseLuaCall(runningLuaMod(), "coe5.loadedContentOrigin", reason, "kind=%.20s id=%lld", kind, (long long)id);
        lua_pushnil(L);
        lua_pushstring(L, reason);
        return 2;
    }
    int creator = 0, writer = 0;
    int known = readContentOrigin(kind, (int)id, &creator, &writer);
    lua_newtable(L);
    setIntField(L, "id", id);
    lua_pushstring(L, known ? (creator ? "created_by_mod" : writer ? "modified_by_mod" : "baseline") : "unknown");
    lua_setfield(L, -2, "kind");
    setBoolField(L, "modded", known && (creator || writer));
    setBoolField(L, "observed", known);
    if (known && creator > 0 && creator <= G32(RVA_MOD_LAST_INDEX) + 1) {
        lua_pushstring(L, modName(creator - 1));
        lua_setfield(L, -2, "definedBy");
    }
    if (known && writer > 0 && writer <= G32(RVA_MOD_LAST_INDEX) + 1) {
        lua_pushstring(L, modName(writer - 1));
        lua_setfield(L, -2, "lastModifiedBy");
    }
    setBoolField(L, "historyComplete", 0);
    lua_pushliteral(L, "observed accepted C5M level-2 writes; exact current record binding; unobserved writes remain unknown");
    lua_setfield(L, -2, "scope");
    return 1;
}
