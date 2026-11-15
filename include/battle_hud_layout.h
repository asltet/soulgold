#ifndef GUARD_BATTLE_HUD_LAYOUT_H
#define GUARD_BATTLE_HUD_LAYOUT_H

// Presentation only. FALSE retains the original battle-interface layout.
#ifndef B_COMPACT_BATTLE_HUD
#define B_COMPACT_BATTLE_HUD TRUE
#endif

#define COMPACT_HUD_WIDTH 112
#define COMPACT_HUD_PLAYER_HEIGHT 36
#define COMPACT_HUD_DOUBLE_HEIGHT 31
#define COMPACT_HUD_OTHER_HEIGHT 22
#define COMPACT_HUD_NAME_X 4
#define COMPACT_HUD_NAME_Y 0
#define COMPACT_HUD_NAME_WIDTH 64
#define COMPACT_HUD_GENDER_X 76
#define COMPACT_HUD_GENDER_WIDTH 8
#define COMPACT_HUD_LEVEL_RIGHT 108
#define COMPACT_HUD_LEVEL_Y 0
#define COMPACT_HUD_BAR_X 16
#define COMPACT_HUD_BAR_Y 13
#define COMPACT_HUD_HP_FILL_X 32
#define COMPACT_HUD_HP_TEXT_X 32
#define COMPACT_HUD_HP_TEXT_WIDTH 48
#define COMPACT_HUD_HP_TEXT_Y 18
#define COMPACT_HUD_PLATE_TOP 13
#define COMPACT_HUD_STATUS_TILE 16 // Separate badge at X0/Y16.

static inline u32 CompactHudHeight(bool32 player, bool32 doubles)
{
    return player ? (doubles ? COMPACT_HUD_DOUBLE_HEIGHT : COMPACT_HUD_PLAYER_HEIGHT) : COMPACT_HUD_OTHER_HEIGHT;
}

static inline u32 CompactHudPlateLeft(u32 y)
{
    return 20 - (y - COMPACT_HUD_PLATE_TOP) / 3;
}

static inline void CompactHudGetCoords(bool32 player, bool32 doubles, bool32 right, s16 *x, s16 *y)
{
    *x = player ? 160 : 34;
    *y = player ? (doubles ? (right ? 97 : 66) : 92) : (doubles && right ? 42 : 20);
}

// Existing player-single sheets contain two64x64 halves, not a linear128px
// tile row. Use row4 for the centered64px EXP lane at logical X24/Y32.
static inline u32 CompactHudExpTileOffset(u32 tile)
{
    return tile < 5 ? 35 + tile : 96 + tile - 5;
}

static inline void CompactHudMakeExpTile(u32 *dest, const u32 *source)
{
    dest[0] = source[2];
    dest[1] = source[3];
    dest[2] = source[4];
    dest[3] = source[5];
    dest[4] = dest[5] = dest[6] = dest[7] = 0;
}

#endif
