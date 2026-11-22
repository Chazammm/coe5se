#pragma once
#define PLAYER_SLOTS_GAME      PLAYER_SLOTS
#define FACTION_SLOT_TARGET    16
#define PLAYER_SLOTS_MOVED     (PLAYER_SLOTS_GAME + FACTION_SLOT_TARGET)
#define PLAYER_SEATS           24

static unsigned char *playerTableAddress(unsigned int rva);
static int playerSlotCount(void);
static unsigned char *playerSlotAddress(unsigned int rva0, int player, int entry);
#define PG8(rva)  (*(signed char *)playerTableAddress(rva))
#define PG16(rva) (*(short *)playerTableAddress(rva))
#define PG32(rva) (*(int *)playerTableAddress(rva))
#define PGF(rva)  (*(float *)playerTableAddress(rva))
#define PSLOT8(rva0, player, entry)  (*(signed char *)playerSlotAddress(rva0, player, entry))
#define PSLOT16(rva0, player, entry) (*(short *)playerSlotAddress(rva0, player, entry))
#define PSLOT32(rva0, player, entry) (*(int *)playerSlotAddress(rva0, player, entry))
#define PSLOTF(rva0, player, entry)  (*(float *)playerSlotAddress(rva0, player, entry))
