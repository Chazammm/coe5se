#pragma once

#ifndef __APPLE__
static const unsigned char FAR_SUMMON_MADE_COUNT[5] = {0x44, 0x8b, 0xed, 0x66, 0x90};
#else
static const uint32_t FAR_SUMMON_MADE_COUNT[1] = {0x2a1b03e8u};
#endif

static void farSummonCountInit(void) {
    if (!checkGameFunction("far_summon_count_check", "")) {
        writeLogError("far_summon_count_installed", "ok=0 of 1 reason=%s", gameCodeReason());
        return;
    }
    if (!writeGameCode(GAME_PTR(RVA_FAR_SUMMON_SETUP_COUNT), FAR_SUMMON_MADE_COUNT, sizeof FAR_SUMMON_MADE_COUNT)) {
        writeLogError("far_summon_count_installed", "ok=0 of 1 reason=code_unwritable");
        return;
    }
    writeLog("far_summon_count_installed", "ok=1 of 1");
}
