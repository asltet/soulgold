#ifndef GUARD_BATTLE_HUD_LAYOUT_H
#define GUARD_BATTLE_HUD_LAYOUT_H

// Presentation only. FALSE retains the original battle-interface layout.
#ifndef B_COMPACT_BATTLE_HUD
#define B_COMPACT_BATTLE_HUD TRUE
#endif

//1 retains the imported BW2 skin;2 selects screenshots/2.png-inspired HUD.
#ifndef B_COMPACT_BATTLE_HUD_STYLE
#define B_COMPACT_BATTLE_HUD_STYLE 2
#endif
#define REFERENCE_BATTLE_HUD (B_COMPACT_BATTLE_HUD_STYLE == 2)

#define COMPACT_HUD_WIDTH 128
#define COMPACT_HUD_PLAYER_HEIGHT 32
#define COMPACT_HUD_DOUBLE_HEIGHT 32
#define COMPACT_HUD_OTHER_HEIGHT 28
#define COMPACT_HUD_NAME_X 10
#define COMPACT_HUD_NAME_Y 3
#define COMPACT_HUD_NAME_WIDTH 62
#define COMPACT_HUD_GENDER_X 72
#define COMPACT_HUD_GENDER_WIDTH 8
#define COMPACT_HUD_LEVEL_RIGHT 112
#define COMPACT_HUD_LEVEL_Y 4
#define COMPACT_HUD_BAR_X 40
#define COMPACT_HUD_BAR_Y 17
#define COMPACT_HUD_HP_FILL_X 56
#define COMPACT_HUD_HP_FILL_WIDTH 48
#define COMPACT_HUD_HP_TILE_INSET 3
#define COMPACT_HUD_BLACK_INDEX 5
#define COMPACT_HUD_HP_GRAY_INDEX 8
#define COMPACT_HUD_HP_TEXT_X 48
#define COMPACT_HUD_HP_TEXT_WIDTH 72
#define COMPACT_HUD_HP_TEXT_Y 22
#define COMPACT_HUD_PLATE_TOP 17
#define COMPACT_HUD_STATUS_TILE 16 // Separate badge at X0/Y16.

static inline u32 CompactHudHeight(bool32 player, bool32 doubles)
{
    return player ? (doubles ? COMPACT_HUD_DOUBLE_HEIGHT : (REFERENCE_BATTLE_HUD ? 36 : COMPACT_HUD_PLAYER_HEIGHT)) : COMPACT_HUD_OTHER_HEIGHT;
}

static inline void CompactHudGetCoords(bool32 player, bool32 doubles, bool32 right, s16 *x, s16 *y)
{
    *x = player ? 144 : (REFERENCE_BATTLE_HUD ? 32 : 34);
    *y = player ? (doubles ? (right ? 96 : 64) : (REFERENCE_BATTLE_HUD ? 92 : 96)) : (doubles && right ? 48 : 20);
}

// Existing player-single sheets contain two64x64 halves, not a linear128px
// tile row. Use row3 for the centered64px EXP lane at logical X40/Y28 (four-row inset within tile row3).
static inline u32 CompactHudExpTileOffset(u32 tile)
{
    return tile < 3 ? 29 + tile : 88 + tile - 3;
}

// Side-specific local offsets recovered from BW2's text writes and cell data.
static inline u32 CompactHudSideOffset(bool32 player) { return player ? 0 : 8; }
static inline u32 CompactHudHpFillX(bool32 player) { return COMPACT_HUD_HP_FILL_X - CompactHudSideOffset(player); }
static inline u32 CompactHudGenderX(bool32 player) { return COMPACT_HUD_GENDER_X + CompactHudSideOffset(player); }
static inline u32 CompactHudLevelX(bool32 player) { return 88 + CompactHudSideOffset(player); }
static inline u32 CompactHudNameX(bool32 player, u32 width) { return (width > 48 ? 10 : 16) + CompactHudSideOffset(player); }

// BW2 keeps the slash in a fixed tile, current HP right-aligned before it,
// and max HP left-aligned after it. Compact only four-digit GBA values.
static inline u32 CompactHudHpDigitX(u32 currentCount, u32 maxCount, u32 index)
{
    if (index < currentCount)
    {
        u32 step = currentCount > 3 ? 7 : 8;
        return 80 - currentCount * step + index * step;
    }
    if (index == currentCount)
        return 80;
    return 88 + (index - currentCount - 1) * (maxCount > 3 ? 7 : 8);
}

// EXP shares tile row3 with HP-number ink. Replace only its bottom four
// words; zero padding in the ROM tile is not transparency to a VRAM write.
static inline void CompactHudBlitExpTile(volatile u32 *dest, const u32 *source)
{
    for (u32 row = 4; row < 8; row++)
        dest[row] = source[row];
}

#endif
