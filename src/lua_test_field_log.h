#pragma once

// Owned Windows battle-log overlay state. The native layout and message production are untouched.
// @platform-split test_field_log: Windows mappings only.
static struct {
    int active, failed, saved;
    unsigned long long session;
} testFieldLog;

static const char *testFieldLogProblem(const LuaScript *script) {
    const char *reason=testFieldReadProblem(script);
    if (reason) return reason;
    if (!testField.editor) return "not_editor_field";
    if (testFieldDeploymentBusy) return "field_transaction_active";
    if (!testFieldLife.active || testFieldLife.failed || testFieldLife.pending)
        return "cleanup_unverified";
    if (GetCurrentThreadId()!=testFieldLife.thread) return "wrong_thread";
    if (G32(RVA_ACTIVE_PLAYER)!=0 || G32(RVA_BATTLE_X)!=testField.x || G32(RVA_BATTLE_Y)!=testField.y)
        return "battle_context_changed";
    if (testFieldLog.active && testFieldLog.session!=testFieldLife.session) return "log_session_unverified";
    if (testFieldLog.failed) return "log_state_unverified";
    int visible=G32(RVA_TEST_FIELD_LOG_VISIBLE);
    return visible==0 || visible==1 ? NULL : "invalid_log_visibility";
}

static int restoreTestFieldLog(void) {
    if (!testFieldLog.active) return !testFieldLog.failed;
    if (!gameCodeMatches || !testField.open || !testFieldLife.active ||
        testFieldLog.session!=testFieldLife.session || GetCurrentThreadId()!=testFieldLife.thread) {
        testFieldLog.failed=1;return 0;
    }
    *(int *)GAME_PTR(RVA_TEST_FIELD_LOG_VISIBLE)=testFieldLog.saved;
    int restored=G32(RVA_TEST_FIELD_LOG_VISIBLE)==testFieldLog.saved;
    if (restored) testFieldLog.active=testFieldLog.failed=0;
    else testFieldLog.failed=1;
    writeLog("test_field_log_restore","session=%llu restored=%d visible=%d",testFieldLog.session,restored,testFieldLog.saved);
    return restored;
}

static int lTestFieldLogState(lua_State *L) {
    const char *call="coe5.testFieldLogState";
    checkLuaMainThread(L,call);
    const char *reason=testFieldLogProblem(checkLuaScript(L));
    if (reason) return refuseTestFieldRead(L,call,reason);
    lua_newtable(L);
    setIntField(L,"version",1);setIntField(L,"session",(lua_Integer)testFieldLife.session);
    setBoolField(L,"visible",G32(RVA_TEST_FIELD_LOG_VISIBLE)!=0);
    setBoolField(L,"overlay",1);setBoolField(L,"reservesMapSpace",0);
    setIntField(L,"toggleAction",12);
    lua_pushstring(L,testField.fighting ? "fight" : testFieldFinalViewActive() ? "after" : "prepare");lua_setfield(L,-2,"phase");
    float x=GF(RVA_TEST_FIELD_LOG_X),y=GF(RVA_TEST_FIELD_LOG_Y),w=GF(RVA_TEST_FIELD_LOG_WIDTH),h=GF(RVA_TEST_FIELD_LOG_HEIGHT);
    int screenW=G32(RVA_SCREEN_W),screenH=G32(RVA_SCREEN_H);
    int valid=isfinite(x) && isfinite(y) && isfinite(w) && isfinite(h) && x>=0 && y>=0 &&
        x<=screenW && y<=screenH && w>0 && h>0 && x+w<=screenW+1 && y+h<=screenH+1;
    setBoolField(L,"rectValid",valid);
    if (valid) {
        /* The actual native background/scissor truncates X/Y to int, but
         * passes float width/height. Preserve that exact readback contract. */
        lua_newtable(L);setIntField(L,"x",(int)x);setIntField(L,"y",(int)y);
        lua_pushnumber(L,w);lua_setfield(L,-2,"width");lua_pushnumber(L,h);lua_setfield(L,-2,"height");lua_setfield(L,-2,"rect");
    }
    int last=G32(RVA_TEST_FIELD_LOG_LAST_LINE);
    setBoolField(L,"lineCountValid",last>=-1 && last<2500);
    if (last>=-1 && last<2500) setIntField(L,"lineCount",last+1);
    setIntField(L,"scrollOffset",G32(RVA_TEST_FIELD_LOG_SCROLL));
    lua_pushliteral(L,"native overlay visibility/rectangle only; no reserved map width, log contents or complete scene");lua_setfield(L,-2,"scope");
    return 1;
}

static int lSetTestFieldLogVisible(lua_State *L) {
    const char *call="coe5.setTestFieldLogVisible";
    checkLuaMainThread(L,call);
    const char *reason=lua_type(L,1)!=LUA_TBOOLEAN ? "bad_type" : testFieldLogProblem(checkLuaScript(L));
    if (!reason && (battleDrawing>0 || luaSiteDepth[SITE_BATTLE_DRAW]>0)) reason="draw_read_only";
    int visible=lua_toboolean(L,1),previous=0;
    if (!reason) {
        previous=G32(RVA_TEST_FIELD_LOG_VISIBLE);
        if (!testFieldLog.active) {
            testFieldLog.active=1;testFieldLog.saved=previous;testFieldLog.session=testFieldLife.session;
        }
        *(int *)GAME_PTR(RVA_TEST_FIELD_LOG_VISIBLE)=visible;
        if (G32(RVA_TEST_FIELD_LOG_VISIBLE)!=visible) {
            *(int *)GAME_PTR(RVA_TEST_FIELD_LOG_VISIBLE)=previous;
            int restored=G32(RVA_TEST_FIELD_LOG_VISIBLE)==previous;
            reason=restored ? "log_verification_failed" : "log_rollback_failed";
            if (!restored) { testFieldLog.failed=1;testFieldLife.failed=testFieldLife.pending=1; }
        }
        writeLog("test_field_log_visibility","session=%llu visible=%d previous=%d ok=%d phase=%s",testFieldLife.session,visible,previous,reason==NULL,
                 testField.fighting ? "fight" : testFieldFinalViewActive() ? "after" : "prepare");
    }
    if (reason) refuseLuaCall(runningLuaMod(),call,reason,"presentation_only=1");
    lua_pushboolean(L,reason==NULL);if(reason)lua_pushstring(L,reason);else lua_pushnil(L);
    return 2;
}
