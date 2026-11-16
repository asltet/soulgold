// Native tests of helpers actually used by battle_interface.c. Hardware/render
// and text-printer behavior require the engine build and emulator checklist.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define TRUE 1
#define FALSE 0
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint32_t bool32;
typedef int16_t s16;
#include "battle_hud_layout.h"
#include "../../src/data/bw2_hud_assets.h"

int main(void)
{
    u32 player, doubles, right, i, x, filled;
    s16 cx, cy;
    for (player = 0; player <= 1; player++)
        for (doubles = 0; doubles <= 1; doubles++)
            for (right = 0; right <= 1; right++)
            {
                u32 height = CompactHudHeight(player, doubles);
                CompactHudGetCoords(player, doubles, right, &cx, &cy);
                assert(cx - 32 >= 0 && cx - 32 + COMPACT_HUD_WIDTH <= 240);
                assert(cy - 16 >= 0 && cy - 16 + height <= 112);
                if (doubles && right)
                {
                    s16 lx, ly;
                    CompactHudGetCoords(player, doubles, FALSE, &lx, &ly);
                    assert(ly + (s16)height <= cy);
                }
            }
    assert(COMPACT_HUD_NAME_X + COMPACT_HUD_NAME_WIDTH <= COMPACT_HUD_GENDER_X);
    assert(COMPACT_HUD_GENDER_X + COMPACT_HUD_GENDER_WIDTH == 80);
    assert(COMPACT_HUD_BAR_X + 16 == COMPACT_HUD_HP_FILL_X);
    assert(COMPACT_HUD_HP_FILL_X + 24 == 80);
    assert(COMPACT_HUD_HP_TEXT_X + COMPACT_HUD_HP_TEXT_WIDTH / 2 == 84);
    for (i = 0; i < 8; i++)
    {
        u32 offset = CompactHudExpTileOffset(i);
        u32 half = offset >= 64 ? 64 : 0;
        assert(offset < 128 && (offset - half) / 8 == 3);
        for (x = 0; x < i; x++) assert(offset != CompactHudExpTileOffset(x));
    }
    for (i = 0; i < 7; i++) assert(CompactHudHpDigitX(3, 3, i) == 56 + i * 8);
    for (i = 0; i < 9; i++) assert(CompactHudHpDigitX(4, 4, i) >= 48 && CompactHudHpDigitX(4, 4, i) + 8 <= 120);
    assert(sizeof(sBw2HudFrames) == 4096 && sizeof(sBw2HudDigits) == 528);
    for (i = 0; i <= 8; i++)
    {
        for (u32 color = 0; color < 3; color++)
        {
            const u32 *tile = sBw2HudHpTiles[color][i];
            filled = 0;
            for (x = 0; x < 8; x++)
            {
                u32 pixel = (tile[5] >> (4*x)) & 15;
                if (pixel >= 10 && pixel <= 15) filled++;
            }
            assert(filled == i && tile[3] == 0x88888888 && tile[7] == tile[3]);
            for (x = 0; x < 3; x++) assert(tile[x] == 0);
        }
        filled = 0;
        for (x = 0; x < 8; x++)
        {
            u32 pixel = (sBw2HudExpTiles[i][5] >> (4*x)) & 15;
            if (pixel == 9 || pixel == 11) filled++;
        }
        assert(filled == i && sBw2HudExpTiles[i][4] == 0x11111111 && sBw2HudExpTiles[i][7] == 0x11111111);
        for (x = 0; x < 4; x++) assert(sBw2HudExpTiles[i][x] == 0);
    }
    // Reproduce the actual full-tile EXP overwrite, then exercise the exact
    // subtile blitter used by the engine. Preserve existing HP ink and guards.
    for (u32 fill = 0; fill <= 8; fill++)
    {
        struct {u32 before, tile[8], after;} guarded;
        guarded.before = 0xDEADBEEF; guarded.after = 0xCAFEBABE;
        for (i = 0; i < 8; i++) guarded.tile[i] = 0x55555555 + i;
        // Previous32-byte copy erased all upper words with transparent zeros.
        for (i = 0; i < 8; i++) guarded.tile[i] = sBw2HudExpTiles[fill][i];
        for (i = 0; i < 4; i++) assert(guarded.tile[i] == 0);
        for (i = 0; i < 8; i++) guarded.tile[i] = 0x55555555 + i;
        CompactHudBlitExpTile(guarded.tile, sBw2HudExpTiles[fill]);
        for (i = 0; i < 4; i++) assert(guarded.tile[i] == 0x55555555 + i);
        for (i = 4; i < 8; i++) assert(guarded.tile[i] == sBw2HudExpTiles[fill][i]);
        assert(guarded.before == 0xDEADBEEF && guarded.after == 0xCAFEBABE);
    }
    assert(COMPACT_HUD_BAR_Y - COMPACT_HUD_HP_TILE_INSET + 7 < COMPACT_HUD_HP_TEXT_Y);
    puts("PASS: panel bounds/doubles separation, text lanes, EXP addressing, native BW2 HP/EXP fill0..8 and zero padding");
    return 0;
}
