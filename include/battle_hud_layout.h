#ifndef GUARD_BATTLE_HUD_LAYOUT_H
#define GUARD_BATTLE_HUD_LAYOUT_H

// Presentation only. FALSE retains the original battle-interface layout.
#ifndef B_COMPACT_BATTLE_HUD
#define B_COMPACT_BATTLE_HUD TRUE
#endif

#define COMPACT_HUD_WIDTH 104
#define COMPACT_HUD_PLAYER_HEIGHT 28
#define COMPACT_HUD_OTHER_HEIGHT 24
#define COMPACT_HUD_NAME_X 28
#define COMPACT_HUD_NAME_WIDTH 48
#define COMPACT_HUD_LEVEL_RIGHT 100
#define COMPACT_HUD_LEVEL_Y 2
#define COMPACT_HUD_HP_TEXT_X 64
#define COMPACT_HUD_HP_TEXT_WIDTH 36
#define COMPACT_HUD_HP_TEXT_RIGHT 100
#define COMPACT_HUD_HP_TEXT_Y 12
#define COMPACT_HUD_STATUS_TILE 8 // Header row1, before the nickname.

static inline void CompactHudGetCoords(bool32 player, bool32 doubles, bool32 right, s16 *x, s16 *y)
{
    *x = player ? 168 : 34;
    *y = player ? (doubles ? (right ? 104 : 80) : 100) : (doubles && right ? 44 : 20);
}

// Existing player-single sheets contain two64x64 halves, not a linear128px
// tile row. Reuse their third row for the64px EXP lane at logical X24/Y24.
static inline u32 CompactHudExpTileOffset(u32 tile)
{
    return tile < 5 ? 27 + tile : 88 + tile - 5;
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
