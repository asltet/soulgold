#ifndef GUARD_BATTLE_HUD_LAYOUT_H
#define GUARD_BATTLE_HUD_LAYOUT_H

// Presentation only. FALSE retains the original battle-interface layout.
#ifndef B_COMPACT_BATTLE_HUD
#define B_COMPACT_BATTLE_HUD TRUE
#endif

#define COMPACT_HUD_WIDTH 128
#define COMPACT_HUD_PLAYER_HEIGHT 28
#define COMPACT_HUD_DOUBLE_HEIGHT 28
#define COMPACT_HUD_OTHER_HEIGHT 24
#define COMPACT_HUD_NAME_X 4
#define COMPACT_HUD_NAME_Y 0
#define COMPACT_HUD_NAME_WIDTH 80
#define COMPACT_HUD_GENDER_X 92
#define COMPACT_HUD_GENDER_WIDTH 8
#define COMPACT_HUD_LEVEL_RIGHT 124
#define COMPACT_HUD_LEVEL_Y 0
#define COMPACT_HUD_BAR_X 24
#define COMPACT_HUD_BAR_Y 13
#define COMPACT_HUD_HP_FILL_X 40
#define COMPACT_HUD_HP_FILL_WIDTH 48
#define COMPACT_HUD_HP_TEXT_X 32
#define COMPACT_HUD_HP_TEXT_WIDTH 64
#define COMPACT_HUD_HP_TEXT_Y 18
#define COMPACT_HUD_PLATE_TOP 13
#define COMPACT_HUD_STATUS_TILE 16 // Separate badge at X0/Y16.

static inline u32 CompactHudHeight(bool32 player, bool32 doubles)
{
    return player ? (doubles ? COMPACT_HUD_DOUBLE_HEIGHT : COMPACT_HUD_PLAYER_HEIGHT) : COMPACT_HUD_OTHER_HEIGHT;
}

static inline void CompactHudGetCoords(bool32 player, bool32 doubles, bool32 right, s16 *x, s16 *y)
{
    *x = player ? 144 : 34;
    *y = player ? (doubles ? (right ? 100 : 72) : 100) : (doubles && right ? 44 : 20);
}

// Existing player-single sheets contain two64x64 halves, not a linear128px
// tile row. Use row3 for the centered64px EXP lane at logical X32/Y24.
static inline u32 CompactHudExpTileOffset(u32 tile)
{
    return tile < 4 ? 28 + tile : 88 + tile - 4;
}

// Keep the hardware bar sprite eight pixels high, but use five visible rows
// so its native-size BW2 plate can fit HP, compact digits and EXP vertically.
static inline void CompactHudMakeHpTile(u32 *dest, const u32 *source)
{
    dest[0] = source[1];
    dest[1] = source[3];
    dest[2] = source[4];
    dest[3] = source[5];
    dest[4] = source[7];
    dest[5] = dest[6] = dest[7] = 0;
}

#endif
