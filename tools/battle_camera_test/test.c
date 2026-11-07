// Native arithmetic/lifecycle test for the actual prototype implementation.
// Hardware/OAM submission still requires emulator verification.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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
u16 gBattle_BG3_X, gBattle_BG3_Y;
s16 gSpriteCoordOffsetX, gSpriteCoordOffsetY;
bool8 gAnimScriptActive;
struct ScanlineEffect gScanlineEffect;
u32 gScanlineEffectGeneration;
u16 ALIGNED(4) gScanlineEffectRegBuffers[2][0x3C0];
static bool32 sChoosing = TRUE;

u8 GetBattlerSpriteCoord(enum BattlerId battler, u8 coord)
{
    assert(coord == BATTLER_COORD_Y);
    return battler == 0 ? 80 : 40;
}
void ScanlineEffect_Stop(void) { gScanlineEffectGeneration++; gScanlineEffect.state = 0; }
void ScanlineEffect_SetParams(struct ScanlineEffectParams params)
{
    gScanlineEffectGeneration++;
    gScanlineEffect.dmaDest = params.dmaDest;
    gScanlineEffect.dmaControl = params.dmaControl;
    gScanlineEffect.state = params.initState;
}

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

static void Ready(void)
{
    BattleCamera_Reset();
    gScanlineEffect.state = 0;
    gScanlineEffect.waveTaskId = TASK_NONE;
    sAllocated = 0;
    sChoosing = TRUE;
    BattleCamera_SetEnvironment(BATTLE_ENVIRONMENT_PLAIN);
}

static void CheckRows(void)
{
    u32 row;
    for (row = 0; row <= DISPLAY_HEIGHT; row++)
    {
        s32 dx = sRows[row] & 511;
        s32 dy = (sRows[row] >> 16) & 511;
        s32 sourceY;
        if (dy >= 256)
            dy -= 512;
        sourceY = row + dy;
        assert(dx >= 0 && dx + DISPLAY_WIDTH <= 256);
        assert(sourceY >= BATTLE_CAMERA_SOURCE_MIN_Y && sourceY <= BATTLE_CAMERA_SOURCE_MAX_Y);
        // Forward-projected ground contact and inverse row lookup agree.
        assert(abs(GroundY(sourceY) - (s32)row) <= 2);
    }
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
        gSprites[i].x = i == 0 ? 72 : i == 1 ? 176 : i == 2 ? 160 : 192;
        gSprites[i].y = i == 0 ? 80 : i == 1 ? 40 : 69;
        gSprites[i].centerToCornerVecX = -32;
        gSprites[i].centerToCornerVecY = -32;
    }
    gSprites[4].inUse = TRUE; // Excluded healthbox/effect fixture.
    gSprites[4].x = 90;
    memcpy(original, gSprites, sizeof(original));
    Ready();
    for (i = 0; i < 150; i++)
    {
        BattleCamera_BeginFrame();
        assert(sCamera.zoom >= 256 && sCamera.zoom <= BATTLE_CAMERA_MAX_ZOOM);
        assert(memcmp(&original[4], &gSprites[4], sizeof(struct Sprite)) == 0);
        if (sCamera.active)
        {
            CheckRows();
            BattleCamera_PublishScanlines();
            assert(memcmp(gScanlineEffectRegBuffers[gScanlineEffect.srcBuffer], sRows, sizeof(sRows)) == 0);
            gScanlineEffect.srcBuffer ^= 1; // Emulate the engine VBlank swap.
        }
        if (i == 77)
        {
            assert(sCamera.zoom == 288 && sCamera.x == 3072 && sCamera.y == -1024);
            assert(PanAt(120) == 12 && PanAt(32) == 6);
            assert(gOamMatrices[gSprites[0].oam.matrixNum].a == 227);
            assert(gOamMatrices[gSprites[1].oam.matrixNum].a == 229);
            // Same projected contact for battler and shadow halves.
            assert(gSprites[1].x == 168 && gSprites[1].y == 46);
            assert(gSprites[2].x == 151 && gSprites[3].x == 185);
        }
        BattleCamera_EndFrame();
        assert(memcmp(original, gSprites, sizeof(original)) == 0);
        assert(sAllocated == 0 && sBackupCount == 0);
    }
    assert(!sCamera.active && sCamera.zoom == 256 && !sOwnsScanlines);
    // Not-yet-adapted scenery gets a neutral fallback, not unsupported scroll.
    Ready();
    BattleCamera_SetEnvironment(BATTLE_ENVIRONMENT_GRASS);
    for (i = 0; i < 160; i++)
    {
        BattleCamera_BeginFrame();
        BattleCamera_EndFrame();
        assert(!sCamera.active && sAllocated == 0 && gScanlineEffect.state == 0);
    }
    // Roll back all actors and floor if only one affine matrix is available.
    Ready();
    sAllocated = 0xFFFFFFFE;
    for (i = 0; i < 80; i++)
    {
        BattleCamera_BeginFrame();
        BattleCamera_EndFrame();
        assert(sAllocated == 0xFFFFFFFE);
        assert(memcmp(original, gSprites, sizeof(original)) == 0);
    }
    assert(!sCamera.active && !sOwnsScanlines);
    // Existing rotated matrices are composed in temporary slots only.
    Ready();
    sAllocated = 1u << 7;
    gSprites[0].oam.affineMode = ST_OAM_AFFINE_NORMAL;
    gSprites[0].oam.matrixNum = 7;
    SetOamMatrix(7, 0, -256, 256, 0);
    gSprites[1].oam.matrixNum = 8;
    memcpy(original, gSprites, sizeof(original));
    for (i = 0; i < 78; i++)
    {
        BattleCamera_BeginFrame();
        if (i == 77)
        {
            struct OamMatrix m = gOamMatrices[gSprites[0].oam.matrixNum];
            assert(m.a == 0 && m.b == -227 && m.c == 227 && m.d == 0);
            assert(gOamMatrices[gSprites[1].oam.matrixNum].a == -229);
            // Interrupted build reselects last complete DMA table, not scratch.
            gScanlineEffect.srcBuffer = 1;
            gMain.oamLoadDisabled = TRUE;
            BattleCamera_PublishScanlines();
            assert(gScanlineEffect.srcBuffer == 0);
            gMain.oamLoadDisabled = FALSE;
        }
        BattleCamera_EndFrame();
        assert(sAllocated == (1u << 7));
        assert(gOamMatrices[7].b == -256 && gOamMatrices[7].c == 256);
        assert(memcmp(original, gSprites, sizeof(original)) == 0);
    }
    sChoosing = FALSE;
    BattleCamera_BeginFrame();
    BattleCamera_EndFrame();
    assert(!sCamera.active && !sOwnsScanlines && gScanlineEffect.state == 0);
    sChoosing = TRUE;
    for (i = 0; i < 80; i++)
    {
        BattleCamera_BeginFrame();
        BattleCamera_EndFrame();
        assert(!sCamera.active);
    }
    // An existing scanline owner is never displaced or stopped.
    Ready();
    gScanlineEffect.state = 1;
    gScanlineEffect.dmaDest = (void *)REG_ADDR_BG1HOFS;
    for (i = 0; i < 80; i++)
    {
        BattleCamera_BeginFrame();
        BattleCamera_EndFrame();
        assert(!sCamera.active && gScanlineEffect.state == 1);
        assert(gScanlineEffect.dmaDest == (void *)REG_ADDR_BG1HOFS);
    }
    // Ownership taken after admission is also preserved on cancellation.
    Ready();
    for (i = 0; i < 60; i++)
    {
        BattleCamera_BeginFrame();
        BattleCamera_EndFrame();
    }
    assert(sCamera.active);
    gScanlineEffect.dmaDest = (void *)REG_ADDR_BG1HOFS;
    BattleCamera_BeginFrame();
    BattleCamera_EndFrame();
    assert(!sCamera.active && gScanlineEffect.state == 1 && !sOwnsScanlines);
    // Identical register/control parameters still belong to a new effect.
    Ready();
    for (i = 0; i < 60; i++)
    {
        BattleCamera_BeginFrame();
        BattleCamera_EndFrame();
    }
    assert(sCamera.active);
    ScanlineEffect_SetParams((struct ScanlineEffectParams){(void *)REG_ADDR_BG3HOFS, SCANLINE_EFFECT_DMACNT_32BIT, 1, 0});
    BattleCamera_BeginFrame();
    BattleCamera_EndFrame();
    assert(!sCamera.active && !sOwnsScanlines && gScanlineEffect.state == 1);
    // Sweep the full authored track, including rows which are normally behind UI.
    Ready();
    for (i = 0; i <= BATTLE_CAMERA_MOVE_TICKS; i++)
    {
        sCamera.x = Ease(BATTLE_CAMERA_MAX_PAN_X * 256, i);
        sCamera.y = Ease(-BATTLE_CAMERA_MAX_PAN_Y * 256, i);
        sCamera.zoom = 256 + Ease(BATTLE_CAMERA_MAX_ZOOM - 256, i);
        assert(BuildRows());
        CheckRows();
    }
    // Existing animation activity and non-neutral legacy scroll reject the shot.
    Ready();
    gAnimScriptActive = TRUE;
    for (i = 0; i < 80; i++)
    {
        BattleCamera_BeginFrame();
        BattleCamera_EndFrame();
        assert(!sCamera.active);
    }
    gAnimScriptActive = FALSE;
    gBattle_BG3_X = 1;
    for (i = 0; i < 80; i++)
    {
        BattleCamera_BeginFrame();
        BattleCamera_EndFrame();
        assert(!sCamera.active);
    }
    gBattle_BG3_X = 0;
    puts("PASS: stronger track, row coverage, contact mapping, restoration, matrix rollback, affine/flips, UI, neutral fallback, DMA ownership, interrupted publication");
    return 0;
}
