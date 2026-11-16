#ifndef GUARD_BATTLE_HUD_REFERENCE_H
#define GUARD_BATTLE_HUD_REFERENCE_H

// Screenshot2 skin. Existing status colors12..15 remain owned by the engine.
static const u16 sReferenceHudPalette[12] =
{
    0, 0, 0x7FFF, 0x5294, 0x1084, 0, 0x7FFF, 0x0B3D,
    0x03E0, 0x7FE0, 0x4E7F, 0x7F28,
};

static inline u32 ReferenceHudFramePixel(bool32 player, bool32 single, u32 x, u32 y, u32 original)
{
    u32 color = 0;
    // Thin white ribbon, with a raised, pointed end toward the battlefield.
    if (player)
    {
        if (y == 14 && x == 6) color = 1;
        if (y == 15 && x >= 5 && x <= 7) color = x == 6 ? 2 : 1;
        if (y >= 16 && y <= 21)
        {
            u32 left = y < 20 ? 20 - y : y - 20;
            if (x >= left) color = y == 16 || x == left ? 1 : y == 21 ? 3 : 2;
        }
        if (y >= 22 && y <= 28 && x >= 2)
            color = y == 28 || x == 2 ? 1 : 4;
    }
    else
    {
        if (y == 14 && x == 117) color = 1;
        if (y == 15 && x >= 116 && x <= 118) color = x == 117 ? 2 : 1;
        if (y >= 16 && y <= 21)
        {
            u32 right = y < 20 ? 119 + y - 16 : 123 - (y - 20);
            if (x <= right) color = y == 16 || x == right ? 1 : y == 21 ? 3 : 2;
        }
        if (y >= 22 && y <= 25 && x <= 120)
            color = y == 25 || x == 120 ? 1 : 4;
    }
    // Retain the established fonts: native Lv and HP glyphs, new skin colors.
    u32 lv = player ? 80 : 88;
    u32 hp = player ? 40 : 32;
    if (y >= 8 && y <= 15 && x >= lv && x < lv + 8 && original)
        color = original;
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
