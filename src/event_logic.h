#pragma once

#define EVENT_FAILED     -1
#define PLAYER_EVENT     1
#define SQUARE_EVENT     2

static struct {
    int active;
    int kind;
    int inActions;
    int hadOr;
    int anyHeld;
    int dropTriggers;
    int branchTaken;
    int afterElse;
    int afterIf;
    int branched;
    int restFailed;
    int targetX, targetY, player;
    int startUnitsCleared;
    int unitsDirty;
    int groupWroteHeld;
    int groupTargetWrote;
    int targetWritten, writtenTargetX, writtenTargetY;
} eventLogic;
static unsigned char eventStartMarks[SQUARE_EVENT_A_SIZE];
static unsigned char eventHeldMarks[SQUARE_EVENT_A_SIZE];
static unsigned char eventTakenMarks[SQUARE_EVENT_A_SIZE];
static int eventStartUnits[SQUARE_EVENT_A_SIZE];
static int eventHeldUnits[SQUARE_EVENT_A_SIZE];
typedef struct { int square, viaTarget; } EventUnitWrite;
static EventUnitWrite *eventGroupWrites;
static int eventGroupWriteCount, eventGroupWriteCapacity;
static int *eventBranchWrites;
static int eventBranchWriteCount, eventBranchWriteCapacity;
static const char *const EVENT_UNIT_WRITERS[] = {"hascom", "hascomhere", "hasunit", "hasunithere", "hasaffunithere", "hasitem", "hasitemhere"};
typedef struct { int square, unit; } EventStartUnit;
static EventStartUnit *eventStartUnitList;
static int eventStartUnitCount, eventStartUnitCapacity;

static unsigned char *eventMarks(void) { return gameBase + RVA_SQUARE_EVENT_A; }
static int *eventUnits(void) { return (int *)GAME_PTR(RVA_SQUARE_EVENT_B); }

static void putBackStartUnits(void) {
    int *units = eventUnits();
    if (eventLogic.kind == PLAYER_EVENT)
        for (int i = 0; i < eventStartUnitCount; i++) units[eventStartUnitList[i].square] = eventStartUnitList[i].unit;
    else if (eventLogic.startUnitsCleared) memset(units, 0xff, sizeof eventStartUnits);
    else memcpy(units, eventStartUnits, sizeof eventStartUnits);
    eventLogic.unitsDirty = 0;
}

static int eventWordIs(const char *line, const char *word) {
    size_t len = strlen(word);
    return !strncmp(line, word, len) && (line[len] == '\0' || line[len] == ' ' || line[len] == '\t' || line[len] == '\r' || line[len] == '\n');
}

static int isEventLogicWord(const char *line) {
    return eventWordIs(line, "if") || eventWordIs(line, "or") || eventWordIs(line, "elseif") || eventWordIs(line, "else");
}

static int eventTriggerWordIn(const char *line, const char *const *words, size_t count) {
    for (size_t i = 0; i < count; i++)
        if (eventWordIs(line + 1, words[i])) return 1;
    return 0;
}

static void noteEventUnitWrite(size_t square) {
    if (!eventLogic.active || eventLogic.kind != SQUARE_EVENT) return;
    eventLogic.unitsDirty = 1;
    if (!growList((void **)&eventGroupWrites, &eventGroupWriteCapacity, eventGroupWriteCount + 1, sizeof eventGroupWrites[0])) {
        refuseC5mLine("squareevent", "-", "out_of_memory", "line=%d file=%.200s", G32(RVA_MOD_LINE), (const char *)(gameBase + RVA_EVENT_FILE));
        return;
    }
    eventGroupWrites[eventGroupWriteCount++] = (EventUnitWrite){(int)square, G32(RVA_EVENT_TARGET_X) >= 0 && G32(RVA_EVENT_TARGET_Y) >= 0};
}

static void eventLogicAfterGameLine(const char *line, int level, int eachSquare) {
    if (level != 3 || !eventLogic.active || eventLogic.kind != SQUARE_EVENT || (line[0] != '+' && line[0] != '-')) return;
    int writer = eventTriggerWordIn(line, EVENT_UNIT_WRITERS, sizeof EVENT_UNIT_WRITERS / sizeof EVENT_UNIT_WRITERS[0]);
    if (writer) eventLogic.unitsDirty = 1;
    if (line[0] != '+' || G32(RVA_EVENT_STATE) < 1) return;
    if (eventWordIs(line + 1, "ownsloctarg")) eventLogic.groupTargetWrote = 1;
    if (!writer) return;
    int tx = G32(RVA_EVENT_TARGET_X), ty = G32(RVA_EVENT_TARGET_Y);
    if (tx >= 0 && ty >= 0) {
        if (tx < MAP_COLUMNS_MAX && ty < MAP_COLUMN_SQUARES) noteEventUnitWrite((size_t)tx * MAP_COLUMN_SQUARES + (size_t)ty);
        return;
    }
    if (!eachSquare) {
        eventLogic.groupWroteHeld = 1;
        return;
    }
    const unsigned char *marks = eventMarks();
    for (size_t i = 0; i < SQUARE_EVENT_A_SIZE; i++)
        if (marks[i]) noteEventUnitWrite(i);
}

static void keepHeldUnitWrite(size_t square, int unit) {
    eventHeldUnits[square] = unit;
    if (!growList((void **)&eventBranchWrites, &eventBranchWriteCapacity, eventBranchWriteCount + 1, sizeof eventBranchWrites[0])) {
        refuseC5mLine("or", "-", "out_of_memory", "line=%d file=%.200s", G32(RVA_MOD_LINE), (const char *)(gameBase + RVA_EVENT_FILE));
        return;
    }
    eventBranchWrites[eventBranchWriteCount++] = (int)square;
}

static void restartEvent(const unsigned char *marks) {
    if (eventLogic.restFailed) {
        G32(RVA_EVENT_STATE) = EVENT_FAILED;
        return;
    }
    G32(RVA_EVENT_TARGET_X) = eventLogic.targetX;
    G32(RVA_EVENT_TARGET_Y) = eventLogic.targetY;
    G32(RVA_EVENT_PLAYER) = eventLogic.player;
    if (eventLogic.kind == PLAYER_EVENT || eventLogic.unitsDirty) putBackStartUnits();
    if (eventLogic.kind == PLAYER_EVENT) {
        G32(RVA_EVENT_STATE) = PLAYER_EVENT;
        return;
    }
    int any = 0;
    for (size_t i = 0; i < SQUARE_EVENT_A_SIZE; i++) any |= (eventMarks()[i] = marks[i]);
    G32(RVA_EVENT_STATE) = any ? SQUARE_EVENT : EVENT_FAILED;
}

static void endTriggerGroup(void) {
    int held = G32(RVA_EVENT_STATE) >= 1;
    eventLogic.anyHeld |= held;
    if (eventLogic.kind == SQUARE_EVENT && held) {
        const unsigned char *marks = eventMarks();
        const int *units = eventUnits();
        for (size_t i = 0; i < SQUARE_EVENT_A_SIZE; i++) {
            if (!marks[i]) continue;
            eventHeldMarks[i] |= marks[i];
            if (eventLogic.groupWroteHeld) keepHeldUnitWrite(i, units[i]);
        }
        for (int w = 0; w < eventGroupWriteCount; w++) {
            size_t square = (size_t)eventGroupWrites[w].square;
            if (eventGroupWrites[w].viaTarget || marks[square]) keepHeldUnitWrite(square, units[square]);
        }
        if (eventLogic.groupTargetWrote)
            eventLogic.targetWritten = 1, eventLogic.writtenTargetX = G32(RVA_EVENT_TARGET_X), eventLogic.writtenTargetY = G32(RVA_EVENT_TARGET_Y);
    }
    eventGroupWriteCount = eventLogic.groupWroteHeld = eventLogic.groupTargetWrote = 0;
}

static void endTriggers(void) {
    if (eventLogic.hadOr && !eventLogic.dropTriggers) {
        endTriggerGroup();
        if (eventLogic.kind == SQUARE_EVENT) {
            memcpy(eventMarks(), eventHeldMarks, SQUARE_EVENT_A_SIZE);
            if (eventLogic.unitsDirty) putBackStartUnits();
            int *units = eventUnits();
            for (int w = 0; w < eventBranchWriteCount; w++) units[eventBranchWrites[w]] = eventHeldUnits[eventBranchWrites[w]];
            if (eventBranchWriteCount) eventLogic.unitsDirty = 1;
            G32(RVA_EVENT_TARGET_X) = eventLogic.targetWritten ? eventLogic.writtenTargetX : eventLogic.targetX;
            G32(RVA_EVENT_TARGET_Y) = eventLogic.targetWritten ? eventLogic.writtenTargetY : eventLogic.targetY;
            G32(RVA_EVENT_STATE) = eventLogic.anyHeld ? SQUARE_EVENT : EVENT_FAILED;
        } else if (!eventLogic.anyHeld) {
            G32(RVA_EVENT_STATE) = EVENT_FAILED;
        }
    }
}

static void endBranchTriggers(void) {
    if (eventLogic.inActions) return;
    eventLogic.inActions = 1;
    endTriggers();
    if (G32(RVA_EVENT_STATE) >= 1) {
        if (eventLogic.kind == PLAYER_EVENT) eventLogic.branchTaken = 1;
        else
            for (size_t i = 0; i < SQUARE_EVENT_A_SIZE; i++) eventTakenMarks[i] |= eventMarks()[i];
    }
}

static void startBranchTriggers(void) {
    eventLogic.inActions = eventLogic.hadOr = eventLogic.anyHeld = eventLogic.dropTriggers = eventLogic.targetWritten = 0;
    eventLogic.groupWroteHeld = eventLogic.groupTargetWrote = eventGroupWriteCount = eventBranchWriteCount = 0;
    memset(eventHeldMarks, 0, sizeof eventHeldMarks);
}

static void keepEventStartPlace(void) {
    eventLogic.targetX = G32(RVA_EVENT_TARGET_X);
    eventLogic.targetY = G32(RVA_EVENT_TARGET_Y);
    eventLogic.player = G32(RVA_EVENT_PLAYER);
    eventStartUnitCount = 0;
    if (eventLogic.kind == SQUARE_EVENT) memcpy(eventStartMarks, eventMarks(), SQUARE_EVENT_A_SIZE);
}

static void keepEventStart(void) {
    keepEventStartPlace();
    if (eventLogic.kind != SQUARE_EVENT || !eventLogic.unitsDirty) return;
    memcpy(eventStartUnits, eventUnits(), sizeof eventStartUnits);
    eventLogic.startUnitsCleared = eventLogic.unitsDirty = 0;
}

static void failRestOfEvent(void) {
    eventLogic.restFailed = 1;
    if (eventLogic.kind == SQUARE_EVENT) memset(eventMarks(), 0, SQUARE_EVENT_A_SIZE);
    G32(RVA_EVENT_STATE) = EVENT_FAILED;
}

static void refuseEventLogicLine(const char *line, const char *reason) {
    char word[32];
    snprintf(word, sizeof word, "%.*s", (int)strcspn(line, " \t\r\n"), line);
    refuseC5mLine(word, "-", reason, "line=%d file=%.200s", G32(RVA_MOD_LINE), (const char *)(gameBase + RVA_EVENT_FILE));
}

static void keepStartUnitAtTarget(const char *line) {
    int tx = G32(RVA_EVENT_TARGET_X), ty = G32(RVA_EVENT_TARGET_Y);
    if (tx < 0 || ty < 0 || tx >= MAP_COLUMNS_MAX || ty >= MAP_COLUMN_SQUARES) return;
    int square = tx * MAP_COLUMN_SQUARES + ty;
    for (int i = 0; i < eventStartUnitCount; i++)
        if (eventStartUnitList[i].square == square) return;
    if (!growList((void **)&eventStartUnitList, &eventStartUnitCapacity, eventStartUnitCount + 1, sizeof eventStartUnitList[0])) {
        refuseEventLogicLine(line, "out_of_memory");
        return;
    }
    eventStartUnitList[eventStartUnitCount++] = (EventStartUnit){square, eventUnits()[square]};
}

static int eventLogicLine(const char *line, int level) {
    int ours = isEventLogicWord(line);
    if (level != 3) return ours;
    if (eventWordIs(line, "endevent")) {
        eventLogic.active = 0;
        return 0;
    }
    if (!eventLogic.active) {
        if (ours) refuseEventLogicLine(line, "outside_event");
        return ours;
    }
    int trigger = line[0] == '+' || line[0] == '-';
    if (trigger && eventLogic.kind == PLAYER_EVENT && G32(RVA_EVENT_STATE) >= 1) keepStartUnitAtTarget(line);
    if (!ours) {
        if (trigger && !eventLogic.inActions) return eventLogic.dropTriggers;
        if (!trigger) {
            endBranchTriggers();
            eventLogic.unitsDirty = 1;
        }
        return 0;
    }
    if (eventWordIs(line, "if")) {
        if (eventLogic.afterIf || eventLogic.inActions || eventLogic.branched) {
            refuseEventLogicLine(line, eventLogic.afterIf ? "second_if" : "if_after_branch");
            failRestOfEvent();
            return 1;
        }
        eventLogic.afterIf = 1;
        endTriggers();
        if (G32(RVA_EVENT_STATE) >= 1) keepEventStart();
        else failRestOfEvent();
        startBranchTriggers();
        return 1;
    }
    if (eventWordIs(line, "or")) {
        if (eventLogic.inActions) {
            refuseEventLogicLine(line, "or_after_actions");
            return 1;
        }
        if (eventLogic.dropTriggers) return 1;
        eventLogic.hadOr = 1;
        endTriggerGroup();
        if (eventLogic.kind == PLAYER_EVENT && G32(RVA_EVENT_STATE) >= 1) eventLogic.dropTriggers = 1;
        else restartEvent(eventStartMarks);
        return 1;
    }
    if (eventLogic.afterElse) {
        refuseEventLogicLine(line, "after_else");
        failRestOfEvent();
        return 1;
    }
    endBranchTriggers();
    eventLogic.branched = 1;
    eventLogic.afterElse = eventWordIs(line, "else");
    startBranchTriggers();
    if (eventLogic.kind == PLAYER_EVENT) {
        if (eventLogic.branchTaken) G32(RVA_EVENT_STATE) = EVENT_FAILED;
        else restartEvent(NULL);
    } else {
        static unsigned char left[SQUARE_EVENT_A_SIZE];
        for (size_t i = 0; i < SQUARE_EVENT_A_SIZE; i++) left[i] = eventStartMarks[i] & (unsigned char)!eventTakenMarks[i];
        restartEvent(left);
    }
    return 1;
}

static void eventLogicAfterLine(const char *line, int level) {
    if (level != 3) return;
    if (!eventWordIs(line, "playerevent") && !eventWordIs(line, "squareevent")) return;
    int state = G32(RVA_EVENT_STATE);
    memset(&eventLogic, 0, sizeof eventLogic);
    if (state != PLAYER_EVENT && state != SQUARE_EVENT) return;
    eventLogic.active = 1;
    eventLogic.kind = state;
    keepEventStartPlace();
    eventLogic.startUnitsCleared = state == SQUARE_EVENT;
    eventGroupWriteCount = eventBranchWriteCount = 0;
    memset(eventHeldMarks, 0, sizeof eventHeldMarks);
    memset(eventTakenMarks, 0, sizeof eventTakenMarks);
}
