static int ceremonyOfGreyedRow(const char *hover) {
    static const struct {
        unsigned rva;
        int ceremony;
    } WHY[] = {
        {RVA_STR_CEREMONY_WHY_WORSHIPPED, CEREMONY_CONVERT},
        {RVA_STR_CEREMONY_WHY_NOT_SETTLEMENT, CEREMONY_CONVERT},
        {RVA_STR_CEREMONY_WHY_SEALS, CEREMONY_BREAK_SEAL},
        {RVA_STR_CEREMONY_WHY_FAVOR, CEREMONY_INCREASE_FAVOR},
        {RVA_STR_CEREMONY_WHY_CRUSADE, CEREMONY_START_CRUSADE},
        {RVA_STR_CEREMONY_WHY_INQUISITION, CEREMONY_CALL_INQUISITION},
        {RVA_STR_CEREMONY_WHY_CARDINALS, CEREMONY_MASTERY_CARDINAL},
        {RVA_STR_CEREMONY_WHY_PONTIFF, CEREMONY_MASTERY_PONTIFF},
    };
    for (size_t i = 0; i < sizeof WHY / sizeof WHY[0]; i++)
        if (hover == (const char *)GAME_PTR(WHY[i].rva)) return WHY[i].ceremony;
    return 0;
}

static void numberGreyedCeremonyRow(u64 *args) {
    if ((int)args[0] != 0 || (int)args[14] != 0) return;
    int ceremony = ceremonyOfGreyedRow((const char *)(uintptr_t)args[12]);
    if (ceremony) args[0] = (u64)(CEREMONY_ROW_GREYED + ceremony);
}

#ifndef __aarch64__
#define CEREMONY_COPIES          5
#define CEREMONY_COPY_EMPTY      0x000
#define CEREMONY_COPY_STUBS      0x010
#define CEREMONY_COPY_STUB_MAX   0x100
#define CEREMONY_COPY_FUNCTION   0x600
#define CEREMONY_COPY_UNWIND     0x610
#define CEREMONY_COPY_UNWIND_CODES_MAX 64
#define CEREMONY_COST_TYPE_RELICS 12

enum { CEREMONY_COST_CONVERT, CEREMONY_COST_SEAL, CEREMONY_COST_NONE, CEREMONY_COST_FAVOR };
typedef struct {
    unsigned at, end;
    int id, costType, cost, castable;
    unsigned label;
    unsigned hover;
} CeremonyRowCopy;
static const CeremonyRowCopy CEREMONY_ROW_COPIES[CEREMONY_COPIES] = {
    {RVA_CEREMONY_ROW_WORSHIPPED, RVA_CEREMONY_ROWS_CONVERT_END, 0, 0, CEREMONY_COST_CONVERT, 0, RVA_STR_CEREMONY_CONVERT,
     RVA_STR_CEREMONY_WHY_WORSHIPPED},
    {RVA_CEREMONY_ROW_NOT_SETTLEMENT, RVA_CEREMONY_ROWS_CONVERT_END, 0, 0, CEREMONY_COST_CONVERT, 0, RVA_STR_CEREMONY_CONVERT,
     RVA_STR_CEREMONY_WHY_NOT_SETTLEMENT},
    {RVA_CEREMONY_ROW_SEAL, RVA_CEREMONY_ROWS_SEAL_END, CEREMONY_ROW_USABLE + CEREMONY_BREAK_SEAL,
     CEREMONY_COST_TYPE_RELICS, CEREMONY_COST_SEAL, 1, 0, 0},
    {RVA_CEREMONY_ROW_SEALS_BROKEN, RVA_CEREMONY_ROWS_SEAL_END, 0, CEREMONY_COST_TYPE_RELICS, CEREMONY_COST_NONE, 0,
     RVA_STR_CEREMONY_BREAK_SEAL, RVA_STR_CEREMONY_WHY_SEALS},
    {RVA_CEREMONY_ROW_FAVOR, RVA_CEREMONY_ROWS_FAVOR_END, CEREMONY_ROW_USABLE + CEREMONY_INCREASE_FAVOR,
     CEREMONY_COST_TYPE_RELICS, CEREMONY_COST_FAVOR, 1, 0, 0},
};
static unsigned char *ceremonyRowPage, *ceremonyRowCalls[CEREMONY_COPIES];

static void emitCeremonyBytes(unsigned char **p, const void *bytes, size_t n) {
    memcpy(*p, bytes, n);
    *p += n;
}
static void emitCeremonyInt(unsigned char **p, int value) { emitCeremonyBytes(p, &value, 4); }
static void emitCeremonyRel32(unsigned char **p, const unsigned char *target) { emitCeremonyInt(p, (int)(target - (*p + 4))); }
static void emitCeremonyStackInt(unsigned char **p, int at, int value) {
    if (at < 0x80) {
        const unsigned char op[] = {0xc7, 0x44, 0x24, (unsigned char)at};
        emitCeremonyBytes(p, op, sizeof op);
    } else {
        const unsigned char op[] = {0xc7, 0x84, 0x24};
        emitCeremonyBytes(p, op, sizeof op);
        emitCeremonyInt(p, at);
    }
    emitCeremonyInt(p, value);
}
static void emitCeremonyStackString(unsigned char **p, int at, const unsigned char *string) {
    const unsigned char lea[] = {0x48, 0x8d, 0x05};
    emitCeremonyBytes(p, lea, sizeof lea);
    emitCeremonyRel32(p, string);
    if (at < 0x80) {
        const unsigned char op[] = {0x48, 0x89, 0x44, 0x24, (unsigned char)at};
        emitCeremonyBytes(p, op, sizeof op);
    } else {
        const unsigned char op[] = {0x48, 0x89, 0x84, 0x24};
        emitCeremonyBytes(p, op, sizeof op);
        emitCeremonyInt(p, at);
    }
}
static void emitCeremonyGameInt(unsigned char **p, const unsigned char *op, size_t n, unsigned rva) {
    emitCeremonyBytes(p, op, n);
    emitCeremonyRel32(p, GAME_PTR(rva));
}

static unsigned char *buildCeremonyRowStub(const CeremonyRowCopy *copy, unsigned char *stub, unsigned char **end) {
    static const unsigned char MOV_EAX[] = {0x8b, 0x05}, IMUL_EAX[] = {0x0f, 0xaf, 0x05}, ADD_EAX[] = {0x03, 0x05};
    unsigned char *p = stub, *call;
    switch (copy->cost) {
    case CEREMONY_COST_CONVERT:
        emitCeremonyGameInt(&p, MOV_EAX, sizeof MOV_EAX, RVA_CEREMONY_COST_CONVERT);
        emitCeremonyBytes(&p, (const unsigned char[]){0x0f, 0xaf, 0xc2}, 3);
        break;
    case CEREMONY_COST_SEAL:
        emitCeremonyGameInt(&p, MOV_EAX, sizeof MOV_EAX, RVA_CEREMONY_COST_SEAL_STEP);
        emitCeremonyGameInt(&p, IMUL_EAX, sizeof IMUL_EAX, RVA_SEALS_BROKEN);
        emitCeremonyGameInt(&p, ADD_EAX, sizeof ADD_EAX, RVA_CEREMONY_COST_SEAL_BASE);
        break;
    case CEREMONY_COST_FAVOR: {
        unsigned long long cell = (unsigned long long)(uintptr_t)playerBlocksNowCell();
        emitCeremonyBytes(&p, (const unsigned char[]){0x48, 0xb8}, 2);
        emitCeremonyBytes(&p, &cell, 8);
        emitCeremonyBytes(&p, (const unsigned char[]){0x48, 0x8b, 0x00}, 3);
        emitCeremonyBytes(&p, (const unsigned char[]){0x42, 0x8b, 0x84, 0x38}, 4);
        emitCeremonyInt(&p, (int)(RVA_PLAYER_DIVINE_FAVOR - RVA_SLOT_NAME));
        emitCeremonyBytes(&p, (const unsigned char[]){0xff, 0xc0}, 2);
        emitCeremonyGameInt(&p, IMUL_EAX, sizeof IMUL_EAX, RVA_CEREMONY_COST_FAVOR_STEP);
        break;
    }
    default:
        emitCeremonyBytes(&p, (const unsigned char[]){0x31, 0xc0}, 2);
        break;
    }
    emitCeremonyBytes(&p, (const unsigned char[]){0x89, 0x44, 0x24, 0x28}, 4);
    emitCeremonyBytes(&p, (const unsigned char[]){0xb9}, 1);
    emitCeremonyInt(&p, copy->id);
    emitCeremonyBytes(&p, (const unsigned char[]){0x8b, 0x55, (unsigned char)CEREMONY_SCREEN_WIDTH_RBP}, 3);
    emitCeremonyBytes(&p, (const unsigned char[]){0x41, 0xb8}, 2);
    emitCeremonyInt(&p, 2);
    if (copy->label) {
        emitCeremonyBytes(&p, (const unsigned char[]){0x4c, 0x8d, 0x0d}, 3);
        emitCeremonyRel32(&p, GAME_PTR(copy->label));
    } else {
        emitCeremonyBytes(&p, (const unsigned char[]){0x4c, 0x8d, 0x8d}, 3);
        emitCeremonyInt(&p, CEREMONY_SCREEN_LABEL_RBP);
    }
    emitCeremonyStackInt(&p, 0x20, copy->costType);
    emitCeremonyStackInt(&p, 0x30, 0);
    emitCeremonyStackInt(&p, 0x38, 0);
    emitCeremonyStackInt(&p, 0x40, -1);
    emitCeremonyStackInt(&p, 0x48, 0);
    emitCeremonyStackInt(&p, 0x50, -1);
    emitCeremonyStackInt(&p, 0x58, 0);
    emitCeremonyStackString(&p, 0x60, copy->hover ? GAME_PTR(copy->hover) : ceremonyRowPage + CEREMONY_COPY_EMPTY);
    emitCeremonyStackInt(&p, 0x68, 1);
    emitCeremonyStackInt(&p, 0x70, copy->castable);
    emitCeremonyStackInt(&p, 0x78, -1);
    emitCeremonyStackString(&p, 0x80, ceremonyRowPage + CEREMONY_COPY_EMPTY);
    call = p;
    emitCeremonyBytes(&p, (const unsigned char[]){0xe8}, 1);
    emitCeremonyRel32(&p, GAME_PTR(RVA_RITUAL_ROW));
    emitCeremonyBytes(&p, (const unsigned char[]){0x31, 0xdb, 0x49, 0xbe}, 4);
    emitCeremonyBytes(&p, &gameBase, 8);
    emitCeremonyBytes(&p, (const unsigned char[]){0xe9}, 1);
    emitCeremonyRel32(&p, GAME_PTR(copy->end));
    *end = p;
    return call;
}

static size_t copyCeremonyScreenUnwind(unsigned char *out) {
    DWORD64 imageBase = 0;
    PRUNTIME_FUNCTION f = RtlLookupFunctionEntry((DWORD64)(uintptr_t)GAME_PTR(RVA_CEREMONY_ROW_WORSHIPPED), &imageBase, NULL);
    if (!f || imageBase != (DWORD64)(uintptr_t)gameBase) return 0;
    const unsigned char *info = gameBase + f->UnwindData;
    int codes = 0;
    for (int depth = 0; depth < 8; depth++) {
        int n = info[2];
        if ((info[0] & 7) != 1 || (info[3] & 0x0f) || codes + n > CEREMONY_COPY_UNWIND_CODES_MAX) return 0;
        memcpy(out + 4 + (size_t)codes * 2, info + 4, (size_t)n * 2);
        codes += n;
        if (!((info[0] >> 3) & 4)) break;
        const RUNTIME_FUNCTION *parent = (const RUNTIME_FUNCTION *)(info + 4 + (size_t)((n + 1) & ~1) * 2);
        info = gameBase + parent->UnwindData;
        if (depth == 7) return 0;
    }
    out[0] = 1;
    out[1] = 0;
    out[2] = (unsigned char)codes;
    out[3] = 0;
    return 4 + (size_t)((codes + 1) & ~1) * 2;
}

static int patchCeremonyRowCopies(void) {
    if (ceremonyRowPage) return 1;
    if (!checkGameFunction("ceremony_rows_check", "")) return 0;
    unsigned char *page = allocNear(GAME_PTR(RVA_CEREMONY_ROW_WORSHIPPED));
    if (!page) {
        writeLogError("ceremony_rows_patched", "ok=0 reason=no_near_page");
        return 0;
    }
    if (!copyCeremonyScreenUnwind(page + CEREMONY_COPY_UNWIND)) {
        VirtualFree(page, 0, MEM_RELEASE);
        writeLogError("ceremony_rows_patched", "ok=0 reason=unwind_info");
        return 0;
    }
    page[CEREMONY_COPY_EMPTY] = 0;
    ceremonyRowPage = page;
    unsigned char *calls[CEREMONY_COPIES], *end = page + CEREMONY_COPY_STUBS;
    for (int i = 0; i < CEREMONY_COPIES; i++) {
        unsigned char *stub = page + CEREMONY_COPY_STUBS + i * CEREMONY_COPY_STUB_MAX;
        calls[i] = buildCeremonyRowStub(&CEREMONY_ROW_COPIES[i], stub, &end);
        if (end > stub + CEREMONY_COPY_STUB_MAX) {
            writeLogError("ceremony_rows_patched", "ok=0 reason=stub_too_big copy=%d", i);
            ceremonyRowPage = NULL;
            VirtualFree(page, 0, MEM_RELEASE);
            return 0;
        }
    }
    RUNTIME_FUNCTION *function = (RUNTIME_FUNCTION *)(page + CEREMONY_COPY_FUNCTION);
    function->BeginAddress = CEREMONY_COPY_STUBS;
    function->EndAddress = (DWORD)(end - page);
    function->UnwindData = CEREMONY_COPY_UNWIND;
    if (!RtlAddFunctionTable(function, 1, (DWORD64)(uintptr_t)page)) {
        ceremonyRowPage = NULL;
        VirtualFree(page, 0, MEM_RELEASE);
        writeLogError("ceremony_rows_patched", "ok=0 reason=unwind_not_added");
        return 0;
    }
    makePagesReadExecute(page, 0x1000);
    unsigned char saved[CEREMONY_COPIES][5];
    for (int i = 0; i < CEREMONY_COPIES; i++) {
        unsigned char *at = GAME_PTR(CEREMONY_ROW_COPIES[i].at), code[5] = {0xe9};
        int rel = (int)(page + CEREMONY_COPY_STUBS + i * CEREMONY_COPY_STUB_MAX - (at + 5));
        memcpy(code + 1, &rel, 4);
        memcpy(saved[i], at, 5);
        if (writeGameCode(at, code, sizeof code)) continue;
        while (--i >= 0) writeGameCode(GAME_PTR(CEREMONY_ROW_COPIES[i].at), saved[i], 5);
        writeLogError("ceremony_rows_patched", "ok=0 reason=code_unwritable");
        return 0;
    }
    memcpy(ceremonyRowCalls, calls, sizeof calls);
    writeLog("ceremony_rows_patched", "ok=1 copies=%d page=%p", CEREMONY_COPIES, (void *)page);
    return 1;
}

static unsigned char *writtenSiteCall(CallSite site, int i) {
    if (site != SITE_CEREMONY_SCREEN_ROW || i < 0 || i >= CEREMONY_COPIES) return NULL;
    return ceremonyRowCalls[i];
}
#endif
