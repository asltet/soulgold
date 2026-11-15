// Native tests of helpers actually used by battle_interface.c. Hardware/render
// and text-printer behavior require the engine build and emulator checklist.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define TRUE 1
#define FALSE 0
typedef uint32_t u32;
typedef uint32_t bool32;
typedef int16_t s16;
#include "battle_hud_layout.h"

int main(int argc, char **argv)
{
    u32 player, doubles, right, i, x, filled;
    s16 cx, cy;
    u32 source[9][8];
    FILE *file;
    assert(argc == 2);
    file = fopen(argv[1], "rb");
    assert(file != NULL && fread(source, sizeof(source), 1, file) == 1);
    fclose(file);
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
    for (i = COMPACT_HUD_PLATE_TOP; i < COMPACT_HUD_PLAYER_HEIGHT; i++)
    {
        assert(CompactHudPlateLeft(i) > 0 && CompactHudPlateLeft(i) < COMPACT_HUD_HP_FILL_X);
        if (i > COMPACT_HUD_PLATE_TOP) assert(CompactHudPlateLeft(i) <= CompactHudPlateLeft(i - 1));
    }
    for (i = 0; i < 8; i++)
    {
        u32 offset = CompactHudExpTileOffset(i);
        u32 half = offset >= 64 ? 64 : 0;
        assert(offset < 128 && (offset - half) / 8 == 4);
        for (x = 0; x < i; x++) assert(offset != CompactHudExpTileOffset(x));
    }
    for (i = 0; i <= 8; i++)
    {
        struct {u32 before, tile[8], after;} guarded = {0xDEADBEEF, {0}, 0xCAFECAFE};
        CompactHudMakeExpTile(guarded.tile, source[i]);
        assert(guarded.before == 0xDEADBEEF && guarded.after == 0xCAFECAFE);
        assert(guarded.tile[0] == source[i][2] && guarded.tile[3] == source[i][5]);
        for (x = 4; x < 8; x++) assert(guarded.tile[x] == 0);
        filled = 0;
        assert((source[0][3] & 15) != (source[8][3] & 15));
        for (x = 0; x < 8; x++)
            if (((guarded.tile[1] >> (4*x)) & 15) == (source[8][3] & 15)) filled++;
        assert(filled == i); // Actual source graphics keep all0..8 fill steps.
    }
    puts("PASS: panel bounds/doubles separation, text lanes, EXP addressing/fill0..8 and guarded packing");
    return 0;
}
