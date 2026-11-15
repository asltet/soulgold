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
    assert(COMPACT_HUD_GENDER_X + COMPACT_HUD_GENDER_WIDTH <= COMPACT_HUD_LEVEL_RIGHT - 24);
    assert(COMPACT_HUD_BAR_X + 16 == COMPACT_HUD_HP_FILL_X);
    assert(COMPACT_HUD_HP_FILL_X + 24 == COMPACT_HUD_WIDTH / 2);
    assert(COMPACT_HUD_HP_TEXT_X + COMPACT_HUD_HP_TEXT_WIDTH / 2 == COMPACT_HUD_WIDTH / 2);
    for (i = 0; i < 8; i++)
    {
        u32 offset = CompactHudExpTileOffset(i);
        u32 half = offset >= 64 ? 64 : 0;
        assert(offset < 128 && (offset - half) / 8 == 3);
        for (x = 0; x < i; x++) assert(offset != CompactHudExpTileOffset(x));
    }
    assert(sizeof(sBw2HudFrames) == 4096 && sizeof(sBw2HudDigits) == 528);
    for (i = 0; i <= 8; i++)
    {
        for (u32 color = 0; color < 3; color++)
        {
            const u32 *tile = sBw2HudHpTiles[color][i];
            filled = 0;
            for (x = 0; x < 8; x++)
            {
                u32 pixel = (tile[2] >> (4*x)) & 15;
                if (pixel >= 10 && pixel <= 15) filled++;
            }
            assert(filled == i && tile[0] == 0x88888888 && tile[4] == tile[0]);
            for (x = 5; x < 8; x++) assert(tile[x] == 0);
        }
        filled = 0;
        for (x = 0; x < 8; x++)
        {
            u32 pixel = (sBw2HudExpTiles[i][1] >> (4*x)) & 15;
            if (pixel == 9 || pixel == 11) filled++;
        }
        assert(filled == i && sBw2HudExpTiles[i][0] == 0x11111111 && sBw2HudExpTiles[i][3] == 0x11111111);
        for (x = 4; x < 8; x++) assert(sBw2HudExpTiles[i][x] == 0);
    }
    {
        u32 sourceRows[8] = {0,1,2,3,4,5,6,7}, output[8];
        CompactHudMakeHpTile(output, sourceRows);
        assert(output[0] == 1 && output[1] == 3 && output[2] == 4 && output[3] == 5 && output[4] == 7);
        assert(output[5] == 0 && output[6] == 0 && output[7] == 0);
    }
    puts("PASS: panel bounds/doubles separation, text lanes, EXP addressing, native BW2 HP/EXP fill0..8 and zero padding");
    return 0;
}
