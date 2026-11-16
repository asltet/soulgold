#ifndef GUARD_BATTLE_HUD_REFERENCE_H
#define GUARD_BATTLE_HUD_REFERENCE_H

// Screenshot2 skin. Existing status colors12..15 remain owned by the engine.
static const u16 sReferenceHudPalette[12] =
{
    0, 0x2108, 0x7FFF, 0x5294, 0x1084, 0, 0x7FFF, 0x0B3D,
    0x03E0, 0x7FE0, 0x4E7F, 0x7F28,
};

// Healthbox/shadow tags alias the same palette bank. Preserve colors0/1,
// especially shadow texel1, and leave dynamic status colors12..15 untouched.
static inline void ReferenceHudLoadPalette(u32 offset, void (*writePalette)(const void *, u32, u32))
{
    writePalette(sReferenceHudPalette + 2, offset + 2, sizeof(sReferenceHudPalette) - 2 * sizeof(u16));
}

static inline u32 ReferenceHudFramePixel(bool32 player, bool32 single, u32 x, u32 y, u32 original)
{
    u32 color = 0;
    // Thin white ribbon, with a raised, pointed end toward the battlefield.
    if (player)
    {
        if (y == 13 && x == 6) color = 5;
        if (y == 14 && x >= 5 && x <= 7) color = x == 6 ? 2 : 5;
        if (y == 15 && x >= 4 && x <= 8) color = x > 4 && x < 8 ? 2 : 5;
        if (y >= 16 && y <= 21)
        {
            u32 left = y < 20 ? 20 - y : y - 20;
            if (x >= left) color = x == left || (y == 16 && x >= 9) ? 5 : y == 21 ? 3 : 2;
        }
        if (y >= 22 && y <= 28)
        {
            u32 left = 30 - y; // Diagonal undercut: inset8px below ribbon to2px at base.
            if (x >= left) color = y == 28 || x == left ? 5 : 4;
        }
    }
    else
    {
        if (y == 13 && x == 117) color = 5;
        if (y == 14 && x >= 116 && x <= 118) color = x == 117 ? 2 : 5;
        if (y == 15 && x >= 115 && x <= 119) color = x > 115 && x < 119 ? 2 : 5;
        if (y >= 16 && y <= 21)
        {
            u32 right = y < 20 ? 119 + y - 16 : 123 - (y - 20);
            if (x <= right) color = x == right || (y == 16 && x <= 114) ? 5 : y == 21 ? 3 : 2;
        }
        if (y >= 22 && y <= 25)
        {
            u32 right = 115 + y - 22;
            if (x <= right) color = y == 25 || x == right ? 5 : 4;
        }
    }
    // Retain the established fonts: native Lv and HP glyphs, new skin colors.
    u32 lv = player ? 80 : 88;
    u32 hp = player ? 40 : 32;
    if (y >= 8 && y <= 15 && x >= lv && x < lv + 8 && original)
        color = original == 1 ? 5 : original;
    if (y >= 16 && y <= 22 && x >= hp - 1 && x <= hp + 15 && (original == 5 || original == 8))
        color = original;
    // Separate EXP capsule:80px fill, white empty interior, cyan progression.
    if (player && single && y >= 32 && y <= 35 && x >= 39 && x <= 120)
        color = y == 32 || y == 35 ? (x == 39 || x == 120 ? 0 : 5) : x == 39 || x == 120 ? 5 : 2;
    return color;
}

static inline u32 ReferenceHudExpPixels(u32 filled64)
{
    return ((filled64 > 64 ? 64 : filled64) * 80 + 32) / 64;
}

static inline u32 ReferenceHudExpTileOffset(u32 tile)
{
    return tile < 3 ? 37 + tile : 96 + tile - 3;
}

static inline void ReferenceHudBlitExpTile(volatile u32 *dest, u32 fill)
{
    u32 pixels = 0;
    for (u32 x = 0; x < 8; x++)
        pixels |= (x < fill ? 9u : 2u) << (x * 4);
    // Preserve the capsule outline and all unrelated rows in the same tile.
    dest[1] = dest[2] = pixels;
}

#endif
