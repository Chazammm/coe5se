#pragma once

// Capture a body and its placement identity for guarded anchor transactions.
typedef struct {
    int unit, monster, hp, owner, x, y, owned, source, actor, structure;
    unsigned long long generation, rawStateBits, monsterFlags, monsterMoveFlags;
    int monsterSprite;
    /* Internal stability fingerprint only; never returned or restored. This
     * catches item/configuration writes from a wrapped native getter too. */
    unsigned char nativeRecord[UNIT_SIZE];
} TestFieldActorBody;

static int testFieldActorBodyRead(int unit, TestFieldActorBody *body) {
    int last=G32(RVA_UNIT_COUNT);
    if (last < -1 || last >= UNIT_TABLE_MAX || unit < 0 || unit > last || unit >= UNIT_TABLE_MAX) return 0;
    memset(body,0,sizeof *body);
    size_t at=(size_t)unit*UNIT_SIZE;
    memcpy(body->nativeRecord,GAME_PTR(RVA_UNITS+at),UNIT_SIZE);
    body->unit=unit;body->monster=G16(RVA_UNIT_MONSTER+at);body->hp=G16(RVA_UNIT_HP+at);
    body->owner=G16(RVA_UNIT_PLAYER+at);body->x=G32(RVA_UNIT_X+at);body->y=G32(RVA_UNIT_X+at+4);
    if (body->hp <= 0 || !isUsedMonster(body->monster) || !isMonsterSpriteValid(body->monster)) return 0;
    const unsigned char *monster=monsterRecord(body->monster);
    memcpy(&body->monsterMoveFlags,monster+MONSTER_MOVE_FLAGS,sizeof body->monsterMoveFlags);
    memcpy(&body->monsterFlags,monster+(RVA_MONSTER_FLAGS-RVA_MONSTER_TABLE),sizeof body->monsterFlags);
    memcpy(&body->monsterSprite,monster+(RVA_MONSTER_SPRITES-RVA_MONSTER_TABLE),sizeof body->monsterSprite);
    body->owned=isTestFieldUnit(unit);
    if (body->owned) {
        const TestFieldMadeUnit *a=&testFieldLife.actors[unit];
        body->generation=a->generation;body->source=a->source;body->actor=a->actor;body->structure=a->source == TEST_FIELD_GATE;
    }
    memcpy(&body->rawStateBits,GAME_PTR(RVA_UNIT_BENEFIT_BITS+at),sizeof body->rawStateBits);
    return 1;
}

static int testFieldActorBodySame(const TestFieldActorBody *body) {
    TestFieldActorBody current;
    return testFieldActorBodyRead(body->unit,&current) && !memcmp(body,&current,sizeof current);
}
