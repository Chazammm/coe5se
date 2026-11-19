#pragma once

#define TEST_FIELD_ZOOM_MIN 0.1
#define TEST_FIELD_ZOOM_MAX 2.0

typedef struct {
    int installed, active, failed;
    unsigned long long session;
    float zoom, savedCell, savedX, savedY, viewportW, viewportH;
    int viewportSet;
    float left, bottom, right, top, currentCell, currentX, currentY;
    const char *unavailable;
} TestFieldZoomState;
static TestFieldZoomState testFieldZoom;

static struct {
    unsigned long long session;
    int queued, armed, applied, failed, completed, width, height;
    float requestedZoom, cell, x, y, screenW, screenH;
    const char *reason;
} testFieldBattleCenter;

static void (*testFieldZoomNative)(int reset);

static int testFieldZoomOwned(void) {
    return gameCodeMatches && testFieldZoom.active && !testFieldZoom.failed && !testFieldInspectActive &&
        testField.open && !testField.fighting && !testFieldFinalViewActive() && testField.activePlayer < 0 && testFieldLife.active &&
        !testFieldLife.retiring && !testFieldLife.failed && !testFieldLife.pending &&
        testFieldZoom.session == testFieldLife.session &&
        GetCurrentThreadId() == testFieldLife.thread &&
        !_stricmp(testField.owner, testFieldLife.owner) &&
        G32(RVA_ACTIVE_PLAYER) == 0 &&
        G32(RVA_BATTLE_X) == testField.x && G32(RVA_BATTLE_Y) == testField.y;
}

static float testFieldZoomViewportW(void) { return (float)G32(RVA_SCREEN_W); }
static float testFieldZoomViewportH(void) { return (float)G32(RVA_SCREEN_H); }

static int testFieldBattleCenterOwned(void) {
    return gameCodeMatches && testField.open && testField.fighting &&
        testField.activePlayer<0 && testFieldLife.active && !testFieldLife.retiring &&
        !testFieldLife.failed && !testFieldLife.pending &&
        testFieldBattleCenter.session==testFieldLife.session &&
        GetCurrentThreadId()==testFieldLife.thread &&
        !_stricmp(testField.owner,testFieldLife.owner) && G32(RVA_ACTIVE_PLAYER)==0 &&
        G32(RVA_BATTLE_X)==testField.x && G32(RVA_BATTLE_Y)==testField.y;
}

static void testFieldBattleCenterBegin(void) {
    if (!testFieldBattleCenter.queued) return;
    testFieldBattleCenter.queued=0;
    if (testFieldBattleCenter.session!=testFieldLife.session) {
        testFieldBattleCenter.failed=1;testFieldBattleCenter.reason="center_session_changed";
    }else testFieldBattleCenter.armed=1;
}

static void testFieldBattleCenterFinish(void) {
    if (testFieldBattleCenter.armed) {
        testFieldBattleCenter.armed=0;testFieldBattleCenter.reason="native_center_frame_not_observed";
    }
    if (testFieldBattleCenter.session==testFieldLife.session) testFieldBattleCenter.completed=1;
}

static void testFieldBattleCenterApply(void) {
    if (!testFieldBattleCenter.armed) return;
    if (!testFieldBattleCenterOwned()) {
        testFieldBattleCenter.armed=0;testFieldBattleCenter.failed=1;
        testFieldBattleCenter.reason="center_context_changed";return;
    }
    int columns=G32(RVA_BATTLE_WIDTH);
    float width=testFieldZoomViewportW(),height=testFieldZoomViewportH();
    float beforeCell=GF(RVA_BATTLE_CELL_PX);
    float beforeX=*(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X);
    float beforeY=*(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y);
    double cell=beforeCell;
    double x=((double)width-columns*cell)*0.5,y=((double)height-BATTLE_ROWS*cell)*0.5;
    testFieldBattleCenter.armed=0;
    if (columns<1 || columns>BATTLE_COLUMNS_MAX || !isfinite(width) || width<=0 ||
        !isfinite(height) || height<=0 || !isfinite(beforeCell) || beforeCell<=0 ||
        !isfinite(beforeX) || !isfinite(beforeY) || !isfinite(cell) || cell<=0 || cell>FLT_MAX ||
        !isfinite(x) || fabs(x)>FLT_MAX || !isfinite(y) || fabs(y)>FLT_MAX) {
        testFieldBattleCenter.failed=1;testFieldBattleCenter.reason="invalid_battle_view";return;
    }
    if (!testFieldZoom.active) {
        testFieldZoom.active=1;testFieldZoom.session=testFieldLife.session;
        testFieldZoom.savedCell=beforeCell;testFieldZoom.savedX=beforeX;testFieldZoom.savedY=beforeY;
    }
    *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X)=(float)x;
    *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y)=(float)y;
    if (GF(RVA_BATTLE_CELL_PX)!=(float)cell ||
        *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X)!=(float)x ||
        *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y)!=(float)y) {
        *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X)=beforeX;
        *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y)=beforeY;
        int restored=GF(RVA_BATTLE_CELL_PX)==beforeCell &&
            *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X)==beforeX &&
            *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y)==beforeY;
        testFieldBattleCenter.failed=1;
        testFieldBattleCenter.reason=restored?"center_verification_failed":"center_rollback_failed";
        if (!restored) testFieldZoom.failed=1;
    }else {
        testFieldBattleCenter.applied=1;testFieldBattleCenter.width=columns;testFieldBattleCenter.height=BATTLE_ROWS;
        testFieldBattleCenter.cell=(float)cell;testFieldBattleCenter.x=(float)x;testFieldBattleCenter.y=(float)y;
        testFieldBattleCenter.screenW=width;testFieldBattleCenter.screenH=height;
    }
    writeLog("test_field_battle_center","session=%llu applied=%d columns=%d setup_zoom=%.9g native_zoom=%.9g cell=%.9g x=%.9g y=%.9g reason=%s",
        testFieldLife.session,testFieldBattleCenter.applied,columns,testFieldBattleCenter.requestedZoom,
        cell/(height*0.05f),cell,x,y,
        testFieldBattleCenter.reason?testFieldBattleCenter.reason:"none");
}

typedef struct {
    double x, y, width, height, minX, maxX, minY, maxY;
    int fitsX, fitsY;
} TestFieldViewportBounds;

static int testFieldViewportBoundsFor(const TestFieldZoomState *state, double cell,
                                      double screenW, double screenH, TestFieldViewportBounds *out) {
    int columns = G32(RVA_BATTLE_WIDTH);
    if (!state->viewportSet || columns < 1 || columns > BATTLE_COLUMNS_MAX ||
        !isfinite(cell) || cell <= 0 || cell > FLT_MAX || !isfinite(screenW) || !isfinite(screenH)) return 0;
    TestFieldViewportBounds bounds = {.x=state->left, .y=state->bottom,
        .width=screenW-state->left-state->right, .height=screenH-state->bottom-state->top};
    if (bounds.x < 0 || bounds.y < 0 || bounds.width <= 4 || bounds.height <= 4) return 0;
    double left=bounds.x+2, bottom=bounds.y+2, right=bounds.x+bounds.width-2, top=bounds.y+bounds.height-2;
    double mapW=columns*cell, mapH=BATTLE_ROWS*cell;
    if (!isfinite(mapW) || !isfinite(mapH) || mapW > FLT_MAX || mapH > FLT_MAX) return 0;
    bounds.fitsX = mapW <= right-left; bounds.fitsY = mapH <= top-bottom;
    if (bounds.fitsX) bounds.minX=bounds.maxX=(left+right-mapW)*0.5;
    else { bounds.minX=right-mapW; bounds.maxX=left; }
    if (bounds.fitsY) bounds.minY=bounds.maxY=(bottom+top-mapH)*0.5;
    else { bounds.minY=top-mapH; bounds.maxY=bottom; }
    *out=bounds;
    return 1;
}

static int testFieldZoomCameraFor(const TestFieldZoomState *state, double cell, double baseX, double baseY,
                                  double *x, double *y) {
    if (state->viewportSet) {
        TestFieldViewportBounds bounds;
        if (!isfinite(baseX) || !isfinite(baseY) || !isfinite(*x) || !isfinite(*y) ||
            !testFieldViewportBoundsFor(state,cell,testFieldZoomViewportW(),testFieldZoomViewportH(),&bounds)) return 0;
        double originX=*x+baseX, originY=*y+baseY;
        if (originX < bounds.minX) originX=bounds.minX;
        if (originX > bounds.maxX) originX=bounds.maxX;
        if (originY < bounds.minY) originY=bounds.minY;
        if (originY > bounds.maxY) originY=bounds.maxY;
        *x=originX-baseX; *y=originY-baseY;
        return fabs(*x)<=FLT_MAX && fabs(*y)<=FLT_MAX;
    }
    int width = G32(RVA_BATTLE_WIDTH);
    double height = testFieldZoomViewportH();
    if (width < 1 || width > BATTLE_COLUMNS_MAX || !isfinite(cell) || cell <= 0 ||
        cell > FLT_MAX || !isfinite(height) || height <= 0 || !isfinite(*x) || !isfinite(*y)) return 0;
    double limitX = width * cell, minimumY = height - BATTLE_ROWS * cell;
    if (!isfinite(limitX) || limitX > FLT_MAX || !isfinite(minimumY)) return 0;
    if (minimumY > 0) minimumY = 0;
    if (*x < -limitX) *x = -limitX;
    if (*x > limitX) *x = limitX;
    if (*y < minimumY) *y = minimumY;
    if (*y > 0) *y = 0;
    return fabs(*x) <= FLT_MAX && fabs(*y) <= FLT_MAX;
}

static int testFieldZoomCamera(double cell, double *x, double *y) {
    return testFieldZoomCameraFor(&testFieldZoom,cell,
        *(const float *)GAME_PTR(RVA_BATTLE_VIEW_ORIGIN_X),
        *(const float *)GAME_PTR(RVA_BATTLE_VIEW_ORIGIN_Y),x,y);
}

TEST_FIELD_PRESERVE_ALL static void testFieldZoomNativeHook(int reset) {
    float beforeX = *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X);
    float beforeY = *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y);
    const void *caller=__builtin_return_address(0);
    testFieldZoomNative(reset);
    if (caller!=(const void *)(GAME_PTR(RVA_BATTLE_CELL_SIZE_CALL_A)+5) &&
        caller!=(const void *)(GAME_PTR(RVA_BATTLE_CELL_SIZE_CALL_B)+5) &&
        caller!=(const void *)(GAME_PTR(RVA_BATTLE_CELL_SIZE_CALL_C)+5) &&
        caller!=(const void *)(GAME_PTR(RVA_BATTLE_CELL_SIZE_CALL_D)+5)) return;
    testFieldBattleCenterApply();
    if (!testFieldZoomOwned()) return;
    float width = testFieldZoomViewportW(), height = testFieldZoomViewportH();
    float oldCell = GF(RVA_BATTLE_CELL_PX);
    double cell = (double)(height * 0.05f) * testFieldZoom.zoom;
    double x = testFieldZoom.viewportSet ? testFieldZoom.currentX : *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X);
    double y = testFieldZoom.viewportSet ? testFieldZoom.currentY : *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y);
    int recentered = reset || width != testFieldZoom.viewportW || height != testFieldZoom.viewportH ||
                     x != beforeX || y != beforeY;
    if (!isfinite(width) || width <= 0 || !isfinite(height) || height <= 0 ||
        !isfinite(oldCell) || oldCell <= 0) {
        writeLog("test_field_zoom_unverified", "stage=viewport caller=%llx reset=%d w=%g h=%g old_cell=%g",
                 (unsigned long long)((const unsigned char *)caller-gameBase), reset, width, height, oldCell);
        testFieldZoom.failed = 1; return;
    }
    if (testFieldZoom.viewportSet) {
        if (width!=testFieldZoom.viewportW || height!=testFieldZoom.viewportH) {
            TestFieldViewportBounds oldBounds,newBounds;
            if (!testFieldViewportBoundsFor(&testFieldZoom,testFieldZoom.currentCell,
                                            testFieldZoom.viewportW,testFieldZoom.viewportH,&oldBounds) ||
                !testFieldViewportBoundsFor(&testFieldZoom,cell,width,height,&newBounds) || testFieldZoom.currentCell<=0) {
                testFieldZoom.failed=1; return;
            }
            double oldCX=oldBounds.x+oldBounds.width*0.5, oldCY=oldBounds.y+oldBounds.height*0.5;
            x=newBounds.x+newBounds.width*0.5-((oldCX-x)/testFieldZoom.currentCell)*cell;
            y=newBounds.y+newBounds.height*0.5-((oldCY-y)/testFieldZoom.currentCell)*cell;
        }
    } else if (recentered) {
        x = width * 0.5 - ((width * 0.5 - x) / oldCell) * cell;
        y = height * 0.5 - ((height * 0.5 - y) / oldCell) * cell;
    }
    if (!testFieldZoomCameraFor(&testFieldZoom,cell,0,0,&x,&y)) {
        writeLog("test_field_zoom_unverified", "stage=camera caller=%llx reset=%d w=%g h=%g cell=%g x=%g y=%g columns=%d",
                 (unsigned long long)((const unsigned char *)caller-gameBase), reset, width, height, cell, x, y, G32(RVA_BATTLE_WIDTH));
        testFieldZoom.failed = 1; return;
    }
    GF(RVA_BATTLE_CELL_PX) = (float)cell;
    *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X) = (float)x;
    *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y) = (float)y;
    testFieldZoom.viewportW = width; testFieldZoom.viewportH = height;
    testFieldZoom.currentCell=(float)cell; testFieldZoom.currentX=(float)x; testFieldZoom.currentY=(float)y;
}

static int installTestFieldZoom(void) {
    if (testFieldZoom.installed) return testFieldZoom.installed > 0;
    testFieldZoom.installed = -1;
    testFieldZoom.unavailable = "zoom_hook_unavailable";
    if (!TEST_FIELD_VERIFIED_WINDOWS_ABI) { testFieldZoom.unavailable = "unsupported_compiler_abi"; return 0; }
    if (!gameCodeMatches) { testFieldZoom.unavailable = "game_code_differs"; return 0; }
    unsigned char *entry = GAME_PTR(RVA_BATTLE_CELL_SIZE_UPDATE);
    static const unsigned sceneCalls[]={RVA_BATTLE_CELL_SIZE_CALL_A,RVA_BATTLE_CELL_SIZE_CALL_B,RVA_BATTLE_CELL_SIZE_CALL_C,RVA_BATTLE_CELL_SIZE_CALL_D};
    for(size_t i=0;i<sizeof sceneCalls/sizeof sceneCalls[0];i++){
        unsigned char *sceneCall=GAME_PTR(sceneCalls[i]);int sceneRelative=0;memcpy(&sceneRelative,sceneCall+1,4);
        if(sceneCall[0]!=0xe8 || sceneCall+5+sceneRelative!=entry){testFieldZoom.unavailable="zoom_scene_code_differs";return 0;}
    }
    unsigned char saved[16];
    memcpy(saved, entry, sizeof saved);
    unsigned char *page = allocNear(entry);
    if (!page) { testFieldZoom.unavailable = "out_of_memory"; return 0; }
    int load0, load8;
    memcpy(&load0, saved + 4, sizeof load0); memcpy(&load8, saved + 12, sizeof load8);
    CodeFixup fixups[2] = {{4, entry + 8 + load0}, {12, entry + 16 + load8}};
    unsigned char slot[ENTRY_HOOK_SLOT], jump[sizeof saved];
    if (!buildEntryHookSlot(slot, page, entry, saved, sizeof saved, fixups, 2, (void *)testFieldZoomNativeHook)) {
        testFieldZoom.unavailable = "zoom_trampoline_out_of_reach"; return 0;
    }
    memcpy(page, slot, sizeof slot);
    if (!makePagesReadExecute(page, 0x1000)) { testFieldZoom.unavailable = "zoom_trampoline_protection"; return 0; }
    testFieldZoomNative = (void (*)(int))(void *)page;
    buildEntryHookJump(jump, entry, page + ENTRY_HOOK_STUB, sizeof saved);
    if (!writeGameCode(entry, jump, sizeof jump)) { testFieldZoom.unavailable = "zoom_code_unwritable"; return 0; }
    if (memcmp(entry, jump, sizeof jump)) {
        int restored = writeGameCode(entry, saved, sizeof saved) && !memcmp(entry, saved, sizeof saved);
        testFieldZoom.unavailable = restored ? "zoom_hook_unverified" : "zoom_hook_rollback_failed";
        if (!restored) testFieldLife.failed = testFieldLife.pending = 1;
        return 0;
    }
    testFieldZoom.installed = 1; testFieldZoom.unavailable = NULL;
    writeLog("test_field_zoom_hook", "installed=1 native_projection=1 candidate_minimum=0.1");
    return 1;
}

static int testFieldViewportPanInstalled;
static const char *testFieldViewportPanUnavailable;

TEST_FIELD_PRESERVE_ALL static int testFieldViewportPanClamp(void) {
    if (!testFieldZoomOwned() || !testFieldZoom.viewportSet) return 0;
    double cell=GF(RVA_BATTLE_CELL_PX);
    double x=*(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X);
    double y=*(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y);
    if (!testFieldZoomCamera(cell,&x,&y)) { testFieldZoom.failed=1; return 0; }
    *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X)=(float)x;
    *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y)=(float)y;
    testFieldZoom.currentCell=(float)cell; testFieldZoom.currentX=(float)x; testFieldZoom.currentY=(float)y;
    return 1;
}

static int testFieldViewportRip(unsigned char **p, unsigned char *code, const unsigned char *stub,
                                 const unsigned char *opcode, int length, const unsigned char *target) {
    long long distance=target-(stub+(*p-code)+length+4);
    if (distance<INT32_MIN || distance>INT32_MAX) return 0;
    int relative=(int)distance;
    memcpy(*p,opcode,(size_t)length); *p+=length;
    memcpy(*p,&relative,4); *p+=4;
    return 1;
}
static int testFieldViewportJump(unsigned char **p, unsigned char *code, const unsigned char *stub,
                                  const unsigned char *target) {
    static const unsigned char jump=0xe9;
    return testFieldViewportRip(p,code,stub,&jump,1,target);
}

static size_t copyTestFieldPanUnwind(unsigned char *out) {
    DWORD64 imageBase = 0;
    PRUNTIME_FUNCTION f = RtlLookupFunctionEntry((DWORD64)(uintptr_t)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_CLAMP), &imageBase, NULL);
    if (!f || imageBase != (DWORD64)(uintptr_t)gameBase) return 0;
    const unsigned char *info = gameBase + f->UnwindData, *parts[2] = {info, NULL};
    if ((info[0] & 7) != 1) return 0;
    if ((info[0] >> 3) & 4) {
        const RUNTIME_FUNCTION *chained = (const RUNTIME_FUNCTION *)(info + 4 + (size_t)((info[2] + 1) & ~1) * 2);
        parts[1] = gameBase + chained->UnwindData;
        if ((parts[1][0] & 7) != 1 || (parts[1][0] >> 3) & 4) return 0;
    } else if ((info[0] >> 3) & 3) return 0;
    int frame = info[3] ? info[3] : parts[1] ? parts[1][3] : 0, count = 2;
    unsigned char *codes = out + 4;
    codes[0] = 6; codes[1] = (unsigned char)(2 | ((0x28 / 8 - 1) << 4));
    codes[2] = 2; codes[3] = (unsigned char)(0 | (11 << 4));
    for (int k = 0; k < 2 && parts[k]; k++) {
        const unsigned char *from = parts[k] + 4;
        for (int i = 0; i < parts[k][2];) {
            int op = from[i * 2 + 1] & 15, opInfo = from[i * 2 + 1] >> 4;
            int slots = op == 0 || op == 2 || op == 3 || op == 10 ? 1 : op == 4 || op == 8 ? 2 : op == 5 || op == 9 ? 3 :
                        op == 1 ? (opInfo ? 3 : 2) : 0;
            if (!slots || i + slots > parts[k][2] || count + slots > 64) return 0;
            memcpy(codes + count * 2, from + i * 2, (size_t)slots * 2);
            codes[count * 2] = 0;
            count += slots; i += slots;
        }
    }
    out[0] = 1; out[1] = 6; out[2] = (unsigned char)count; out[3] = (unsigned char)frame;
    return 4 + (size_t)((count + 1) & ~1) * 2;
}

static int installTestFieldViewportPan(void) {
    if (testFieldViewportPanInstalled) return testFieldViewportPanInstalled>0;
    testFieldViewportPanInstalled=-1; testFieldViewportPanUnavailable="viewport_pan_unavailable";
    if (!gameCodeMatches) { testFieldViewportPanUnavailable="game_code_differs"; return 0; }
    unsigned char *entry=GAME_PTR(RVA_BATTLE_VIEW_SCROLL_CLAMP), *resume=GAME_PTR(RVA_BATTLE_VIEW_SCROLL_CLAMP_END);
    unsigned char saved[16]; memcpy(saved,entry,sizeof saved);
    unsigned char *page=allocNear(entry);
    if (!page) { testFieldViewportPanUnavailable="out_of_memory"; return 0; }
    unsigned char original[ENTRY_HOOK_SLOT], code[256], *p=code, *stub=page+0x100;
    CodeFixup fixups[2]={{4,GAME_PTR(RVA_SCREEN_H)},{12,GAME_PTR(RVA_BATTLE_CELL_PX)}};
    if (!buildEntryHookSlot(original,page,entry,saved,sizeof saved,fixups,2,(void *)testFieldViewportPanClamp)) {
        testFieldViewportPanUnavailable="viewport_pan_out_of_reach"; return 0;
    }
    static const unsigned char CALL_HEAD[]={0x41,0x53,0x48,0x83,0xec,0x28,0x49,0xbb};
    static const unsigned char CALL_TAIL[]={0x41,0xff,0xd3,0x48,0x83,0xc4,0x28,0x41,0x5b,0x85,0xc0,0x0f,0x84};
    memcpy(p,CALL_HEAD,sizeof CALL_HEAD); p+=sizeof CALL_HEAD;
    const void *observer=(void *)testFieldViewportPanClamp; memcpy(p,&observer,8); p+=8;
    memcpy(p,CALL_TAIL,sizeof CALL_TAIL); p+=sizeof CALL_TAIL;
    unsigned char *branch=p; p+=4;
    static const unsigned char LOAD_X[]={0xf3,0x0f,0x10,0x25}, LOAD_Y[]={0xf3,0x0f,0x10,0x2d};
    static const unsigned char LOAD_H[]={0x66,0x0f,0x6e,0x1d}, LOAD_CELL[]={0xf3,0x0f,0x10,0x15};
    static const unsigned char LOAD_W[]={0x8b,0x0d}, CONVERT_H[]={0x0f,0x5b,0xdb};
    int ok=testFieldViewportRip(&p,code,stub,LOAD_X,sizeof LOAD_X,GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X)) &&
        testFieldViewportRip(&p,code,stub,LOAD_Y,sizeof LOAD_Y,GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y)) &&
        testFieldViewportRip(&p,code,stub,LOAD_H,sizeof LOAD_H,GAME_PTR(RVA_SCREEN_H));
    memcpy(p,CONVERT_H,sizeof CONVERT_H); p+=sizeof CONVERT_H;
    ok=ok && testFieldViewportRip(&p,code,stub,LOAD_CELL,sizeof LOAD_CELL,GAME_PTR(RVA_BATTLE_CELL_PX)) &&
        testFieldViewportRip(&p,code,stub,LOAD_W,sizeof LOAD_W,GAME_PTR(RVA_BATTLE_WIDTH));
    memcpy(p,entry+0x4b,4); p+=4;
    memcpy(p,entry+0x6a,14); p+=14;
    ok=ok && testFieldViewportJump(&p,code,stub,resume);
    int relative=(int)(p-(branch+4)); memcpy(branch,&relative,4);
    ok=ok && testFieldViewportJump(&p,code,stub,page);
    if (!ok) { testFieldViewportPanUnavailable="viewport_pan_out_of_reach"; return 0; }
    memcpy(page,original,sizeof original); memcpy(stub,code,(size_t)(p-code));
    RUNTIME_FUNCTION *function=(RUNTIME_FUNCTION *)(page+0x200);
    if (!copyTestFieldPanUnwind(page+0x210)) { testFieldViewportPanUnavailable="viewport_pan_unwind"; return 0; }
    function->BeginAddress=0x100; function->EndAddress=(DWORD)(0x100+(p-code)); function->UnwindData=0x210;
    if (!makePagesReadExecute(page,0x1000)) { testFieldViewportPanUnavailable="viewport_pan_protection"; return 0; }
    if (!RtlAddFunctionTable(function,1,(DWORD64)(uintptr_t)page)) { testFieldViewportPanUnavailable="viewport_pan_unwind"; return 0; }
    unsigned char jump[16]; buildEntryHookJump(jump,entry,stub,sizeof jump);
    if (!writeGameCode(entry,jump,sizeof jump)) { testFieldViewportPanUnavailable="viewport_pan_unwritable"; return 0; }
    if (memcmp(entry,jump,sizeof jump)) {
        int restored=writeGameCode(entry,saved,sizeof jump) && !memcmp(entry,saved,sizeof jump);
        testFieldViewportPanUnavailable=restored ? "viewport_pan_unverified" : "viewport_pan_rollback_failed";
        if (!restored) testFieldLife.failed=testFieldLife.pending=1;
        return 0;
    }
    testFieldViewportPanInstalled=1; testFieldViewportPanUnavailable=NULL;
    writeLog("test_field_viewport_pan_hook","installed=1 native_clamp_boundary=1 delta_translation_retained=1");
    return 1;
}

static int restoreTestFieldZoom(void) {
    memset(&testFieldBattleCenter, 0, sizeof testFieldBattleCenter);
    if (!testFieldZoom.active) return 1;
    if (!gameCodeMatches || !testField.open || testFieldZoom.session != testFieldLife.session ||
        GetCurrentThreadId() != testFieldLife.thread || _stricmp(testField.owner, testFieldLife.owner)) return 0;
    GF(RVA_BATTLE_CELL_PX) = testFieldZoom.savedCell;
    *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X) = testFieldZoom.savedX;
    *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y) = testFieldZoom.savedY;
    int restored = GF(RVA_BATTLE_CELL_PX) == testFieldZoom.savedCell &&
        *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X) == testFieldZoom.savedX &&
        *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y) == testFieldZoom.savedY;
    if (restored) { testFieldZoom.active = testFieldZoom.failed = 0; testFieldZoom.session = 0; }
    if (restored) testFieldZoom.viewportSet=0;
    writeLog("test_field_zoom_restore", "restored=%d", restored);
    return restored;
}

static const char *testFieldViewProblem(const LuaScript *script) {
    const char *reason = testFieldReadProblem(script);
    if (reason) return reason;
    if (!testField.editor) return "not_editor_field";
    if (testField.fighting) return "in_fight";
    if (testFieldFinalViewActive()) return "final_read_only";
    if (!testFieldLife.active || testFieldLife.retiring || testFieldLife.failed || testFieldLife.pending)
        return "cleanup_unverified";
    if (testFieldDeploymentBusy || testFieldLife.depth || testFieldLife.overrideDepth)
        return "field_transaction_active";
    if (testFieldInspectActive) return "inspect_active";
    if (GetCurrentThreadId() != testFieldLife.thread) return "wrong_thread";
    if (G32(RVA_ACTIVE_PLAYER) != 0 || G32(RVA_BATTLE_X) != testField.x || G32(RVA_BATTLE_Y) != testField.y)
        return "battle_context_changed";
    return NULL;
}

static const char *testFieldZoomView(lua_State *L, BattleView *view) {
    const char *reason = testFieldViewProblem(checkLuaScript(L));
    if (reason) return reason;
    if (testFieldZoom.installed < 0) return testFieldZoom.unavailable;
    if (testFieldZoom.active && testFieldZoom.session != testFieldLife.session) return "zoom_session_unverified";
    if (testFieldZoom.failed) return "zoom_unverified";
    BattleView current = {
        .cellSize = GF(RVA_BATTLE_CELL_PX),
        .baseX = *(const float *)GAME_PTR(RVA_BATTLE_VIEW_ORIGIN_X),
        .baseY = *(const float *)GAME_PTR(RVA_BATTLE_VIEW_ORIGIN_Y),
        .cameraX = *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X),
        .cameraY = *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y),
        .width = G32(RVA_BATTLE_WIDTH), .height = BATTLE_ROWS,
        .screenW = G32(RVA_SCREEN_W), .screenH = G32(RVA_SCREEN_H),
        .windowW = G32(RVA_WINDOW_W), .windowH = G32(RVA_WINDOW_H),
    };
    if (!isfinite(current.cellSize) || current.cellSize <= 0 ||
        !isfinite(current.baseX) || !isfinite(current.baseY) ||
        !isfinite(current.cameraX) || !isfinite(current.cameraY) ||
        current.width < 1 || current.width > BATTLE_COLUMNS_MAX ||
        current.screenW < 1 || current.screenH < 1 || current.windowW < 1 || current.windowH < 1)
        return "invalid_battle_view";
    float height = testFieldZoomViewportH(), width = testFieldZoomViewportW();
    if (!isfinite(height) || height <= 0 || !isfinite(width) || width <= 0 ||
        !isfinite(height * 0.05f) || height * 0.05f <= 0) return "invalid_battle_view";
    double ratio = current.cellSize / (double)(height * 0.05f);
    if (!isfinite(ratio) || ratio <= 0) return "invalid_battle_view";
    *view = current;
    return NULL;
}

static int lTestFieldZoom(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldZoom");
    BattleView view;
    const char *reason = testFieldZoomView(L, &view);
    if (reason) return refuseTestFieldRead(L, "coe5.testFieldZoom", reason);
    lua_createtable(L, 0, 5);
    lua_pushinteger(L, 1); lua_setfield(L, -2, "version");
    double ratio=view.cellSize/(double)(testFieldZoomViewportH()*0.05f);
    if (fabs(ratio-TEST_FIELD_ZOOM_MIN)<1e-7) ratio=TEST_FIELD_ZOOM_MIN;
    if (fabs(ratio-TEST_FIELD_ZOOM_MAX)<1e-7) ratio=TEST_FIELD_ZOOM_MAX;
    lua_pushnumber(L, ratio); lua_setfield(L, -2, "zoom");
    lua_pushnumber(L, TEST_FIELD_ZOOM_MIN); lua_setfield(L, -2, "minZoom");
    lua_pushnumber(L, TEST_FIELD_ZOOM_MAX); lua_setfield(L, -2, "maxZoom");
    lua_pushnumber(L, view.cellSize); lua_setfield(L, -2, "cellSize");
    return 1;
}

static int lTestFieldPointerWheel(lua_State *L) {
    checkLuaMainThread(L, "coe5.testFieldPointerWheel");
    const char *reason=testFieldViewProblem(checkLuaScript(L));
    if(!reason && battleWaiting<=0 && luaSiteDepth[SITE_BATTLE_WAIT]<=0)reason="not_battle_frame";
    if(reason)return refuseTestFieldRead(L,"coe5.testFieldPointerWheel",reason);
    lua_pushinteger(L,(unsigned int)G32(RVA_WHEEL_FRAME));
    return 1;
}

static int lSetTestFieldZoom(lua_State *L) {
    checkLuaMainThread(L, "coe5.setTestFieldZoom");
    double zoom = lua_type(L, 1) == LUA_TNUMBER ? lua_tonumber(L, 1) : NAN;
    const char *reason = !isfinite(zoom) || zoom < TEST_FIELD_ZOOM_MIN || zoom > TEST_FIELD_ZOOM_MAX ? "bad_zoom" : NULL;
    BattleView view;
    if (!reason) reason = testFieldZoomView(L, &view);
    if (!reason && (battleDrawing > 0 || luaSiteDepth[SITE_BATTLE_DRAW] > 0)) reason = "draw_read_only";
    if (!reason && battleWaiting <= 0 && luaSiteDepth[SITE_BATTLE_WAIT] <= 0) reason = "not_battle_frame";
    int anchorGiven = !lua_isnoneornil(L, 2) || !lua_isnoneornil(L, 3);
    double anchorX = 0, anchorY = 0;
    if (!reason) {
        anchorX = anchorGiven ? lua_type(L, 2) == LUA_TNUMBER ? lua_tonumber(L, 2) : NAN : view.screenW * 0.5;
        anchorY = anchorGiven ? lua_type(L, 3) == LUA_TNUMBER ? lua_tonumber(L, 3) : NAN : view.screenH * 0.5;
        if (!anchorGiven && testFieldZoom.viewportSet) {
            TestFieldViewportBounds bounds;
            if (!testFieldViewportBoundsFor(&testFieldZoom,view.cellSize,view.screenW,view.screenH,&bounds)) reason="invalid_viewport";
            else { anchorX=bounds.x+bounds.width*0.5; anchorY=bounds.y+bounds.height*0.5; }
        }
        if (!isfinite(anchorX) || !isfinite(anchorY) || anchorX < 0 || anchorY < 0 ||
            anchorX >= view.screenW || anchorY >= view.screenH) reason = "bad_zoom_anchor";
    }
    if (!reason) {
        double cell = (double)(testFieldZoomViewportH() * 0.05f) * zoom;
        double x = anchorX - view.baseX - ((anchorX - view.baseX - view.cameraX) / view.cellSize) * cell;
        double y = anchorY - view.baseY - ((anchorY - view.baseY - view.cameraY) / view.cellSize) * cell;
        if (!testFieldZoomCamera(cell, &x, &y)) reason = "invalid_battle_view";
        if (!reason && !installTestFieldZoom()) reason = testFieldZoom.unavailable;
        if (!reason) {
            TestFieldZoomState before = testFieldZoom;
            if (!testFieldZoom.active) {
                testFieldZoom.savedCell = view.cellSize; testFieldZoom.savedX = view.cameraX; testFieldZoom.savedY = view.cameraY;
                testFieldZoom.session = testFieldLife.session; testFieldZoom.active = 1;
            }
            testFieldZoom.zoom = (float)zoom;
            testFieldZoom.viewportW = testFieldZoomViewportW(); testFieldZoom.viewportH = testFieldZoomViewportH();
            testFieldZoom.currentCell=(float)cell; testFieldZoom.currentX=(float)x; testFieldZoom.currentY=(float)y;
            GF(RVA_BATTLE_CELL_PX) = (float)cell;
            *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X) = (float)x;
            *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y) = (float)y;
            if (GF(RVA_BATTLE_CELL_PX) != (float)cell ||
                *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X) != (float)x ||
                *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y) != (float)y) {
                GF(RVA_BATTLE_CELL_PX) = view.cellSize;
                *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X) = view.cameraX;
                *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y) = view.cameraY;
                int restored = GF(RVA_BATTLE_CELL_PX) == view.cellSize &&
                    *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X) == view.cameraX &&
                    *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y) == view.cameraY;
                if (restored) testFieldZoom = before;
                else testFieldZoom.failed = 1;
                reason = restored ? "zoom_verification_failed" : "zoom_rollback_failed";
            }
            writeLog("test_field_zoom", "session=%llu zoom=%.6f cell=%.6f camera_x=%.6f camera_y=%.6f ok=%d",
                     testFieldLife.session, zoom, cell, x, y, reason == NULL);
        }
    }
    if (reason) refuseLuaCall(runningLuaMod(), "coe5.setTestFieldZoom", reason, "view=kept_or_unverified");
    lua_pushboolean(L, reason == NULL);
    if (reason) lua_pushstring(L, reason); else lua_pushnil(L);
    return 2;
}

static const char *testFieldViewportMutationView(lua_State *L, BattleView *view) {
    const char *reason=testFieldZoomView(L,view);
    if (!reason && battleDrawing>0) reason="draw_read_only";
    if (!reason && battleWaiting<=0 && luaSiteDepth[SITE_BATTLE_WAIT]<=0 &&
        luaSiteDepth[SITE_BATTLE_DRAW]<=0) reason="not_battle_frame";
    return reason;
}
static int testFieldViewportResult(lua_State *L, const char *call, const char *reason) {
    if (reason) refuseLuaCall(runningLuaMod(),call,reason,"owned_view=kept_or_unverified");
    lua_pushboolean(L,reason==NULL);
    if (reason) lua_pushstring(L,reason); else lua_pushnil(L);
    return 2;
}
static int testFieldViewportNumber(lua_State *L, const char *field, double *value) {
    lua_pushstring(L,field); lua_rawget(L,1);
    int valid=lua_type(L,-1)==LUA_TNUMBER;
    *value=valid ? lua_tonumber(L,-1) : NAN;
    lua_pop(L,1);
    return valid && isfinite(*value);
}

static const char *testFieldViewportCommit(const BattleView *view, TestFieldZoomState candidate,
                                           double zoom, double cell, double x, double y, const char *operation) {
    TestFieldZoomState before=testFieldZoom;
    candidate.installed=before.installed; candidate.unavailable=before.unavailable;
    if (!before.active) {
        candidate.savedCell=view->cellSize; candidate.savedX=view->cameraX; candidate.savedY=view->cameraY;
    }
    candidate.active=1; candidate.failed=0; candidate.session=testFieldLife.session;
    candidate.zoom=(float)zoom; candidate.viewportW=testFieldZoomViewportW(); candidate.viewportH=testFieldZoomViewportH();
    candidate.currentCell=(float)cell; candidate.currentX=(float)x; candidate.currentY=(float)y;
    testFieldZoom=candidate;
    GF(RVA_BATTLE_CELL_PX)=(float)cell;
    *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X)=(float)x;
    *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y)=(float)y;
    const char *reason=NULL;
    if (GF(RVA_BATTLE_CELL_PX)!=(float)cell ||
        *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X)!=(float)x ||
        *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y)!=(float)y) {
        GF(RVA_BATTLE_CELL_PX)=view->cellSize;
        *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X)=view->cameraX;
        *(float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y)=view->cameraY;
        int restored=GF(RVA_BATTLE_CELL_PX)==view->cellSize &&
            *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_X)==view->cameraX &&
            *(const float *)GAME_PTR(RVA_BATTLE_VIEW_SCROLL_Y)==view->cameraY;
        if (restored) testFieldZoom=before; else testFieldZoom.failed=1;
        reason=restored ? "viewport_verification_failed" : "viewport_rollback_failed";
    }
    writeLog("test_field_viewport","operation=%s session=%llu zoom=%.9g cell=%.9g camera_x=%.9g camera_y=%.9g ok=%d",
             operation,testFieldLife.session,zoom,cell,x,y,reason==NULL);
    return reason;
}

static int lSetTestFieldViewport(lua_State *L) {
    const char *call="coe5.setTestFieldViewport";
    checkLuaMainThread(L,call);
    BattleView view;
    const char *reason=!lua_istable(L,1) ? "bad_viewport" : testFieldViewportMutationView(L,&view);
    unsigned long long session=testFieldLife.session;
    double x=0,y=0,width=0,height=0;
    if (!reason && (!testFieldViewportNumber(L,"x",&x) || !testFieldViewportNumber(L,"y",&y) ||
        !testFieldViewportNumber(L,"width",&width) || !testFieldViewportNumber(L,"height",&height) ||
        x<0 || y<0 || width<=4 || height<=4 || x+width>view.screenW || y+height>view.screenH)) reason="bad_viewport";
    if (!reason) {
        TestFieldZoomState candidate=testFieldZoom;
        candidate.viewportSet=1; candidate.left=(float)x; candidate.bottom=(float)y;
        candidate.right=(float)(view.screenW-x-width); candidate.top=(float)(view.screenH-y-height);
        double cameraX=view.cameraX,cameraY=view.cameraY;
        double zoom=view.cellSize/(double)(testFieldZoomViewportH()*0.05f);
        if (zoom<TEST_FIELD_ZOOM_MIN-1e-6 || zoom>TEST_FIELD_ZOOM_MAX+1e-6 ||
            !testFieldZoomCameraFor(&candidate,view.cellSize,view.baseX,view.baseY,&cameraX,&cameraY)) reason="invalid_battle_view";
        if (!reason && !installTestFieldZoom()) reason=testFieldZoom.unavailable;
        if (!reason && !installTestFieldViewportPan()) reason=testFieldViewportPanUnavailable;
        if (!reason && (testFieldLife.session!=session || testFieldViewProblem(checkLuaScript(L)))) reason="context_changed";
        if (!reason) reason=testFieldViewportCommit(&view,candidate,zoom,view.cellSize,cameraX,cameraY,"set");
    }
    return testFieldViewportResult(L,call,reason);
}

static int lTestFieldViewport(lua_State *L) {
    const char *call="coe5.testFieldViewport";
    checkLuaMainThread(L,call);
    BattleView view;
    const char *reason=testFieldZoomView(L,&view);
    if (!reason && !testFieldZoom.viewportSet) reason="viewport_not_set";
    TestFieldViewportBounds bounds;
    if (!reason && testFieldViewportPanInstalled<=0) reason=testFieldViewportPanUnavailable ? testFieldViewportPanUnavailable : "viewport_pan_unavailable";
    if (!reason && !testFieldViewportBoundsFor(&testFieldZoom,view.cellSize,view.screenW,view.screenH,&bounds)) reason="invalid_viewport";
    if (reason) return refuseTestFieldRead(L,call,reason);
    lua_createtable(L,0,20);
    lua_pushinteger(L,1); lua_setfield(L,-2,"version");
    const struct { const char *name; double value; } values[]={
        {"x",bounds.x},{"y",bounds.y},{"width",bounds.width},{"height",bounds.height},
        {"cellSize",view.cellSize},{"originX",view.baseX+view.cameraX},{"originY",view.baseY+view.cameraY},
        {"minOriginX",bounds.minX},{"maxOriginX",bounds.maxX},{"minOriginY",bounds.minY},{"maxOriginY",bounds.maxY},
        {"zoom",view.cellSize/(double)(testFieldZoomViewportH()*0.05f)},
        {"minZoom",TEST_FIELD_ZOOM_MIN},{"maxZoom",TEST_FIELD_ZOOM_MAX},
        {"fieldWidth",view.width},{"fieldHeight",BATTLE_ROWS},{"margin",2}
    };
    for(size_t i=0;i<sizeof values/sizeof values[0];i++) { lua_pushnumber(L,values[i].value); lua_setfield(L,-2,values[i].name); }
    lua_pushboolean(L,bounds.fitsX); lua_setfield(L,-2,"fitsX");
    lua_pushboolean(L,bounds.fitsY); lua_setfield(L,-2,"fitsY");
    lua_pushboolean(L,bounds.fitsX && bounds.fitsY); lua_setfield(L,-2,"fits");
    return 1;
}

static int lFitTestFieldViewport(lua_State *L) {
    const char *call="coe5.fitTestFieldViewport";
    checkLuaMainThread(L,call);
    BattleView view;
    const char *reason=testFieldViewportMutationView(L,&view);
    if (!reason && !testFieldZoom.viewportSet) reason="viewport_not_set";
    TestFieldViewportBounds bounds;
    if (!reason && !testFieldViewportBoundsFor(&testFieldZoom,view.cellSize,view.screenW,view.screenH,&bounds)) reason="invalid_viewport";
    if (!reason) {
        double required=fmin((bounds.width-4)/view.width,(bounds.height-4)/BATTLE_ROWS);
        double base=testFieldZoomViewportH()*0.05f;
        double requested=required/base;
        if (requested<TEST_FIELD_ZOOM_MIN) requested=TEST_FIELD_ZOOM_MIN;
        if (requested>TEST_FIELD_ZOOM_MAX) requested=TEST_FIELD_ZOOM_MAX;
        float zoom=(float)requested;
        if (requested>TEST_FIELD_ZOOM_MIN && requested<TEST_FIELD_ZOOM_MAX && (float)(base*zoom)>required)
            zoom=nextafterf(zoom,0);
        double cell=(float)(base*zoom);
        double cameraX=bounds.x+(bounds.width-view.width*cell)*0.5-view.baseX;
        double cameraY=bounds.y+(bounds.height-BATTLE_ROWS*cell)*0.5-view.baseY;
        if (!testFieldZoomCamera(cell,&cameraX,&cameraY)) reason="invalid_battle_view";
        if (!reason) reason=testFieldViewportCommit(&view,testFieldZoom,zoom,cell,cameraX,cameraY,"fit");
    }
    return testFieldViewportResult(L,call,reason);
}

static int lPanTestFieldViewport(lua_State *L) {
    const char *call="coe5.panTestFieldViewport";
    checkLuaMainThread(L,call);
    double dx=lua_type(L,1)==LUA_TNUMBER ? lua_tonumber(L,1) : NAN;
    double dy=lua_type(L,2)==LUA_TNUMBER ? lua_tonumber(L,2) : NAN;
    BattleView view;
    const char *reason=!isfinite(dx) || !isfinite(dy) ? "bad_pan" : testFieldViewportMutationView(L,&view);
    if (!reason && !testFieldZoom.viewportSet) reason="viewport_not_set";
    if (!reason) {
        double x=view.cameraX+dx,y=view.cameraY+dy;
        double zoom=view.cellSize/(double)(testFieldZoomViewportH()*0.05f);
        if (!testFieldZoomCamera(view.cellSize,&x,&y)) reason="invalid_battle_view";
        if (!reason) reason=testFieldViewportCommit(&view,testFieldZoom,zoom,view.cellSize,x,y,"pan");
    }
    return testFieldViewportResult(L,call,reason);
}

static int lRequestTestFieldBattleCenter(lua_State *L) {
    const char *call="coe5.requestTestFieldBattleCenter";
    checkLuaMainThread(L,call);
    BattleView view;
    const char *reason=testFieldZoomView(L,&view);
    if (!reason && battleDrawing>0) reason="draw_read_only";
    double zoom=0;
    if (!reason) {
        zoom=view.cellSize/(double)(testFieldZoomViewportH()*0.05f);
        if (!isfinite(zoom) || zoom<TEST_FIELD_ZOOM_MIN-1e-6 || zoom>TEST_FIELD_ZOOM_MAX+1e-6)
            reason="invalid_battle_view";
        if (!reason && !installTestFieldZoom()) reason=testFieldZoom.unavailable;
    }
    if (!reason) {
        if (zoom<TEST_FIELD_ZOOM_MIN) zoom=TEST_FIELD_ZOOM_MIN;
        if (zoom>TEST_FIELD_ZOOM_MAX) zoom=TEST_FIELD_ZOOM_MAX;
        memset(&testFieldBattleCenter,0,sizeof testFieldBattleCenter);
        testFieldBattleCenter.session=testFieldLife.session;
        testFieldBattleCenter.queued=1;testFieldBattleCenter.requestedZoom=(float)zoom;
        writeLog("test_field_battle_center_request","session=%llu queued=1 zoom=%.9g",testFieldLife.session,zoom);
    }
    return testFieldViewportResult(L,call,reason);
}

static int lTestFieldBattleCenterState(lua_State *L) {
    const char *call="coe5.testFieldBattleCenterState";
    checkLuaMainThread(L,call);
    const char *reason=testFieldReadProblem(checkLuaScript(L));
    if (!reason && !testField.editor) reason="not_editor_field";
    if (!reason && (!testFieldLife.active || testFieldLife.retiring || testFieldLife.failed || testFieldLife.pending))
        reason="cleanup_unverified";
    if (!reason && GetCurrentThreadId()!=testFieldLife.thread) reason="wrong_thread";
    if (!reason && (G32(RVA_ACTIVE_PLAYER)!=0 || G32(RVA_BATTLE_X)!=testField.x || G32(RVA_BATTLE_Y)!=testField.y))
        reason="battle_context_changed";
    if (reason) return refuseTestFieldRead(L,call,reason);
    int current=testFieldBattleCenter.session==testFieldLife.session;
    const char *status=!current ? "none" : testFieldBattleCenter.failed ? "failed" :
        testFieldBattleCenter.applied ? "applied" : testFieldBattleCenter.armed ? "armed" :
        testFieldBattleCenter.queued ? "queued" : testFieldBattleCenter.completed ? "not_observed" : "none";
    lua_createtable(L,0,18);
    setIntField(L,"version",1);setIntField(L,"session",(lua_Integer)testFieldLife.session);
    lua_pushstring(L,status);lua_setfield(L,-2,"status");
    setBoolField(L,"queued",current && testFieldBattleCenter.queued);
    setBoolField(L,"armed",current && testFieldBattleCenter.armed);
    setBoolField(L,"applied",current && testFieldBattleCenter.applied);
    setBoolField(L,"failed",current && testFieldBattleCenter.failed);
    setBoolField(L,"completed",current && testFieldBattleCenter.completed);
    if (current) {
        lua_pushnumber(L,testFieldBattleCenter.requestedZoom);lua_setfield(L,-2,"requestedZoom");
        lua_pushnumber(L,testFieldBattleCenter.requestedZoom);lua_setfield(L,-2,"setupZoom");
        if (testFieldBattleCenter.reason) {lua_pushstring(L,testFieldBattleCenter.reason);lua_setfield(L,-2,"reason");}
        if (testFieldBattleCenter.applied) {
            setIntField(L,"fieldWidth",testFieldBattleCenter.width);setIntField(L,"fieldHeight",testFieldBattleCenter.height);
            const struct {const char *name;float value;} values[]={
                {"initialCellSize",testFieldBattleCenter.cell},{"initialOriginX",testFieldBattleCenter.x},
                {"initialOriginY",testFieldBattleCenter.y},{"initialScreenW",testFieldBattleCenter.screenW},
                {"initialScreenH",testFieldBattleCenter.screenH},
                {"initialZoom",testFieldBattleCenter.cell/(testFieldBattleCenter.screenH*0.05f)}
            };
            for(size_t i=0;i<sizeof values/sizeof values[0];i++){lua_pushnumber(L,values[i].value);lua_setfield(L,-2,values[i].name);}
        }
    }
    lua_pushliteral(L,"Camera-only centering at native combat scale on the first authenticated full map reset; setupZoom/requestedZoom are prior-editor observations, not applied battle scale. Later native panning and scale remain native. No actor, terrain, deployment or report changes.");
    lua_setfield(L,-2,"scope");
    return 1;
}
