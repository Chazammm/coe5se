#pragma once

/* Read-only provenance, obtained from actual accepted parser writes, never ID
 * ranges. Full current-record comparison detects later unobserved mutations.
 * This records creator/last observed writer, not a complete dependency graph. */
typedef struct { int creator,writer;unsigned char expected[ITEM_SIZE]; } ContentItemOrigin;
typedef struct { int creator,writer;SpellRecord expected; } ContentSpellOrigin;
typedef struct { int creator,writer,known;char expected[64]; } ContentPathOrigin;
static ContentItemOrigin *contentItems;static int contentItemCount;
static ContentSpellOrigin *contentSpells;static int contentSpellCount;
static ContentPathOrigin contentPaths[SPELL_PATH_COUNT];
static uint64_t contentOriginEpoch;
typedef struct ContentOriginToken {
    struct ContentOriginToken *parent;
    unsigned char *allItems;unsigned char item[ITEM_SIZE];
    int index,mod,nested,count;uint64_t epoch;
} ContentOriginToken;
static ContentOriginToken *contentOriginTop;

static void contentOriginsBeforePass(int level) {
    if(level!=2)return;
    free(contentItems);contentItems=NULL;contentItemCount=0;
    free(contentSpells);contentSpells=NULL;contentSpellCount=0;
    memset(contentPaths,0,sizeof contentPaths);contentOriginEpoch++;
}
static int contentOriginsEnsure(void) {
    if(!gameCodeMatches)return 0;
    if(!contentItems || contentItemCount!=tableRecords(TABLE_ITEM)) {
        int n=tableRecords(TABLE_ITEM);
        if(n<=0 || n>32768 || !*tableAddress(TABLE_ITEM))return 0;
        ContentItemOrigin *items=calloc((size_t)n,sizeof *items);if(!items)return 0;
        for(int i=0;i<n;i++) {
            if(contentItems && i<contentItemCount)items[i]=contentItems[i];
            else memcpy(items[i].expected,tableAddress(TABLE_ITEM)+(size_t)i*ITEM_SIZE,ITEM_SIZE);
        }
        free(contentItems);contentItems=items;contentItemCount=n;
    }
    if(!contentSpells || contentSpellCount!=spellTableRecords()) {
        int n=spellTableRecords();if(n<=0 || n>32768)return 0;
        ContentSpellOrigin *spells=calloc((size_t)n,sizeof *spells);if(!spells)return 0;
        for(int i=0;i<n;i++) {if(contentSpells && i<contentSpellCount)spells[i]=contentSpells[i];else spells[i].expected=*spellSlot(i);}
        free(contentSpells);contentSpells=spells;contentSpellCount=n;
    }
    return contentItemCount==tableRecords(TABLE_ITEM) && contentSpellCount==spellTableRecords();
}
static void *contentOriginBeginLine(const char *line,int mod,int level) {
    if(level!=2 || mod<0 || mod>G32(RVA_MOD_LAST_INDEX) || !contentOriginsEnsure())return NULL;
    ContentOriginToken *t=calloc(1,sizeof *t);if(!t)return NULL;
    t->mod=mod;t->epoch=contentOriginEpoch;t->index=G32(RVA_ITEM_CURRENT);t->count=contentItemCount;
    size_t len=strcspn(line," \t");
    if(len==7 && !_strnicmp(line,"newitem",7)) {
        t->allItems=malloc((size_t)contentItemCount*ITEM_SIZE);
        if(t->allItems)memcpy(t->allItems,tableAddress(TABLE_ITEM),(size_t)contentItemCount*ITEM_SIZE);
        else t->index=-1; /* no before-image: never attribute a failed snapshot */
    } else if(t->index>=0 && t->index<contentItemCount)
        memcpy(t->item,tableAddress(TABLE_ITEM)+(size_t)t->index*ITEM_SIZE,ITEM_SIZE);
    t->parent=contentOriginTop;if(t->parent)t->parent->nested=1;contentOriginTop=t;
    return t;
}
static void contentOriginEndLine(void *token) {
    ContentOriginToken *t=token;if(!t)return;
    contentOriginTop=t->parent;
    int at=G32(RVA_ITEM_CURRENT);
    if(!t->nested && t->epoch==contentOriginEpoch && contentItems && at>=0 && at<contentItemCount && contentItemCount==tableRecords(TABLE_ITEM)) {
        const unsigned char *before=t->allItems?(at<t->count?t->allItems+(size_t)at*ITEM_SIZE:NULL):at==t->index?t->item:NULL;
        const unsigned char *after=tableAddress(TABLE_ITEM)+(size_t)at*ITEM_SIZE;
        if(before && *after && memcmp(before,after,ITEM_SIZE)) {
            ContentItemOrigin *o=&contentItems[at];
            if(t->allItems && !*before)o->creator=t->mod+1;
            o->writer=t->mod+1;memcpy(o->expected,after,ITEM_SIZE);
        }
    }
    free(t->allItems);free(t);
}
static void contentOriginsResetPaths(void) {
    memset(contentPaths,0,sizeof contentPaths);
    for(int p=0;p<spellPathCount();p++)if(spellPathNames()[p]) {
        ContentPathOrigin *o=&contentPaths[p];o->known=p<VANILLA_SPELL_PATH_COUNT;
        snprintf(o->expected,sizeof o->expected,"%s",spellPathNames()[p]);
    }
}
static void contentOriginNotePath(int path,int mod,int created) {
    if(path<0 || path>=spellPathCount() || mod<0 || mod>G32(RVA_MOD_LAST_INDEX))return;
    ContentPathOrigin *o=&contentPaths[path];if(created)o->creator=mod+1;
    o->writer=mod+1;o->known=1;snprintf(o->expected,sizeof o->expected,"%s",spellPathNames()[path]);
}
static void contentOriginNoteSpell(int spell,int mod,int created) {
    if(!contentOriginsEnsure() || spell<0 || spell>=contentSpellCount || mod<0 || mod>G32(RVA_MOD_LAST_INDEX))return;
    ContentSpellOrigin *o=&contentSpells[spell];if(created)o->creator=mod+1;
    o->writer=mod+1;o->expected=*spellSlot(spell);
}
static int contentOriginKnown(const char *kind,int id,int *creator,int *writer) {
    *creator=*writer=0;
    if(!strcmp(kind,"item")) {
        if(!contentItems || id<0 || id>=contentItemCount || contentItemCount!=tableRecords(TABLE_ITEM))return 0;
        ContentItemOrigin *o=&contentItems[id];
        if(memcmp(o->expected,tableAddress(TABLE_ITEM)+(size_t)id*ITEM_SIZE,ITEM_SIZE))return 0;
        *creator=o->creator;*writer=o->writer;return 1;
    }
    if(!strcmp(kind,"spell")) {
        if(!contentSpells || id<0 || id>=contentSpellCount || contentSpellCount!=spellTableRecords())return 0;
        ContentSpellOrigin *o=&contentSpells[id];
        if(memcmp(&o->expected,spellSlot(id),sizeof o->expected))return 0;
        *creator=o->creator;*writer=o->writer;return 1;
    }
    if(!strcmp(kind,"spellschool") && id>=0 && id<spellPathCount()) {
        ContentPathOrigin *o=&contentPaths[id];const char *name=spellPathNames()[id];
        if(!o->known || !name || strcmp(o->expected,name))return 0;
        *creator=o->creator;*writer=o->writer;return 1;
    }
    return 0;
}
