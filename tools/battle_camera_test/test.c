// Native arithmetic/lifecycle test for the actual prototype implementation.
// Hardware/OAM submission still requires emulator verification.
#include <assert.h>
#include <stdio.h>
#include "global.h"
#undef ARM_FUNC
#define ARM_FUNC
#include "../../src/battle_camera.c"

struct Main gMain;
struct Sprite gSprites[MAX_SPRITES + 1];
struct OamMatrix gOamMatrices[OAM_MATRIX_COUNT];
struct PaletteFadeControl gPaletteFade;
struct BattleSpriteData *gBattleSpritesDataPtr;
u8 gBattlersCount;
u8 gBattlerPositions[MAX_BATTLERS_COUNT];
u8 gBattlerSpriteIds[MAX_BATTLERS_COUNT];
u32 gBattleTypeFlags;
static bool32 sChoosing = TRUE;
static u32 sAllocated;

bool32 InBattleChoosingMoves(void) { return sChoosing; }
u8 AllocOamMatrix(void)
{
    u32 i;
    for (i = 0; i < 32; i++)
        if (!(sAllocated & (1u << i)))
        {
            sAllocated |= 1u << i;
            return i;
        }
    return 0xFF;
}
void SetOamMatrix(u8 i, u16 a, u16 b, u16 c, u16 d)
{
    gOamMatrices[i] = (struct OamMatrix){a, b, c, d};
}
void FreeOamMatrix(u8 i)
{
    sAllocated &= ~(1u << i);
    SetOamMatrix(i, 256, 0, 0, 256);
}
void CalcCenterToCornerVec(struct Sprite *s, u8 shape, u8 size, u8 mode)
{
    // Fixtures use square 64px sprites only.
    assert(shape == 0 && size == 3);
    s->centerToCornerVecX = s->centerToCornerVecY = mode == ST_OAM_AFFINE_DOUBLE ? -64 : -32;
}

int main(void)
{
    static struct BattleSpriteData data;
    static struct BattleHealthboxInfo boxes[MAX_BATTLERS_COUNT];
    struct Sprite original[MAX_SPRITES + 1];
    u32 i;
    data.healthBoxesData = boxes;
    gBattleSpritesDataPtr = &data;
    gMain.inBattle = TRUE;
    gBattlersCount = 2;
    gBattlerPositions[1] = B_SIDE_OPPONENT;
    gBattlerSpriteIds[1] = 1;
    boxes[1].shadowSpriteIdPrimary = 2;
    boxes[1].shadowSpriteIdSecondary = 3;
    for (i = 0; i < 4; i++)
    {
        gSprites[i].inUse = TRUE;
        gSprites[i].oam.size = 3;
        gSprites[i].x = 72 + i * 16;
        gSprites[i].y = 80;
        gSprites[i].centerToCornerVecX = -32;
        gSprites[i].centerToCornerVecY = -32;
    }
    // Unregistered healthbox/effect fixture must remain unchanged.
    gSprites[4].inUse = TRUE;
    gSprites[4].x = 90;
    memcpy(original, gSprites, sizeof(original));
    BattleCamera_Reset();
    for (i = 0; i < 150; i++)
    {
        BattleCamera_BeginFrame();
        assert(sCamera.zoom >= 256 && sCamera.zoom <= 268);
        assert(BattleCamera_GetBgX() >= 0 && BattleCamera_GetBgX() <= 4);
        assert(BattleCamera_GetBgY() <= 0 && BattleCamera_GetBgY() >= -2);
        assert(memcmp(&original[4], &gSprites[4], sizeof(struct Sprite)) == 0);
        if (i == 77)
        {
            assert(sCamera.zoom == 268 && sCamera.x == 1024 && sCamera.y == -512);
            assert(gOamMatrices[gSprites[0].oam.matrixNum].a == 244);
        }
        BattleCamera_EndFrame();
        assert(memcmp(original, gSprites, sizeof(original)) == 0);
        assert(sAllocated == 0 && sBackupCount == 0);
    }
    assert(!sCamera.active && sCamera.zoom == 256 && sBgX == 0 && sBgY == 0);
    // Exhausted matrices: safe translation fallback, no existing matrix writes.
    BattleCamera_Reset();
    sAllocated = 0xFFFFFFFF;
    for (i = 0; i < 80; i++)
    {
        BattleCamera_BeginFrame();
        assert(gSprites[0].oam.affineMode == ST_OAM_AFFINE_OFF);
        BattleCamera_EndFrame();
        assert(sAllocated == 0xFFFFFFFF);
        assert(memcmp(original, gSprites, sizeof(original)) == 0);
    }
    // Existing rotated matrices are composed into a temporary slot, never edited.
    sAllocated = 1u << 7;
    gSprites[0].oam.affineMode = ST_OAM_AFFINE_NORMAL;
    gSprites[0].oam.matrixNum = 7;
    SetOamMatrix(7, 0, -256, 256, 0);
    gSprites[1].oam.matrixNum = 8; // Non-affine horizontal flip bit.
    memcpy(original, gSprites, sizeof(original));
    BattleCamera_Reset();
    for (i = 0; i < 78; i++)
    {
        BattleCamera_BeginFrame();
        if (i == 77)
        {
            struct OamMatrix composed = gOamMatrices[gSprites[0].oam.matrixNum];
            assert(composed.a == 0 && composed.b == -244 && composed.c == 244 && composed.d == 0);
            assert(gOamMatrices[gSprites[1].oam.matrixNum].a == -244);
        }
        BattleCamera_EndFrame();
        assert(sAllocated == (1u << 7));
        assert(gOamMatrices[7].b == -256 && gOamMatrices[7].c == 256);
        assert(memcmp(original, gSprites, sizeof(original)) == 0);
    }
    // Leaving the menu cancels and does not replay on subsequent menu entry.
    sChoosing = FALSE;
    BattleCamera_BeginFrame();
    BattleCamera_EndFrame();
    assert(!sCamera.active && sBgX == 0 && sBgY == 0);
    sChoosing = TRUE;
    for (i = 0; i < 80; i++)
    {
        BattleCamera_BeginFrame();
        BattleCamera_EndFrame();
        assert(!sCamera.active);
    }
    puts("PASS: bounds, endpoints, sprite restoration, matrix exhaustion, affine composition, flips, UI exclusion, cancellation");
    return 0;
}
