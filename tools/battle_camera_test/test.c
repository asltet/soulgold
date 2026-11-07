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
    return (battler & 1) ? 40 : 80;
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

static struct Sprite sOriginal[MAX_SPRITES + 1];

static void Tick(u32 allocated)
{
    BattleCamera_BeginFrame();
    assert(sCamera.zoom >= 256 && sCamera.zoom <= BATTLE_CAMERA_MAX_ZOOM);
    assert(sCamera.x >= 0 && sCamera.x <= BATTLE_CAMERA_MAX_PAN_X * 256);
    assert(sCamera.y <= 0 && sCamera.y >= -BATTLE_CAMERA_MAX_PAN_Y * 256);
    assert(memcmp(&sOriginal[4], &gSprites[4], sizeof(struct Sprite)) == 0);
    if (sCamera.active)
    {
        CheckRows();
        BattleCamera_PublishScanlines();
        assert(memcmp(gScanlineEffectRegBuffers[gScanlineEffect.srcBuffer], sRows, sizeof(sRows)) == 0);
        gScanlineEffect.srcBuffer ^= 1;
    }
    BattleCamera_EndFrame();
    assert(memcmp(sOriginal, gSprites, sizeof(sOriginal)) == 0);
    assert(sAllocated == allocated && sBackupCount == 0);
}

static void ForceTrack(u8 track)
{
    Ready();
    assert(BuildRows() && AcquireScanlines());
    sCamera.track = track;
    sCamera.leg = 0;
    sCamera.active = TRUE;
    sLastTrack = track;
    BeginLeg();
}

int main(void)
{
    static struct BattleSpriteData data;
    static struct BattleHealthboxInfo boxes[MAX_BATTLERS_COUNT];
    u32 i, track;
    data.healthBoxesData = boxes;
    gBattleSpritesDataPtr = &data;
    gMain.inBattle = TRUE;
    gBattlersCount = 2;
    for (i = 0; i < MAX_BATTLERS_COUNT; i++)
        gBattlerPositions[i] = i;
    gBattlerSpriteIds[0] = 0;
    gBattlerSpriteIds[1] = 1;
    gBattlerSpriteIds[2] = 5;
    gBattlerSpriteIds[3] = 6;
    boxes[1].shadowSpriteIdPrimary = 2;
    boxes[1].shadowSpriteIdSecondary = 3;
    boxes[3].shadowSpriteIdPrimary = 7;
    boxes[3].shadowSpriteIdSecondary = 8;
    for (i = 0; i < 9; i++)
    {
        gSprites[i].inUse = TRUE;
        gSprites[i].oam.size = 3;
        gSprites[i].x = i == 0 ? 72 : i == 1 ? 176 : i == 2 ? 160 : i == 3 ? 192 : 120;
        gSprites[i].y = i == 0 ? 80 : i == 1 ? 40 : 69;
        gSprites[i].centerToCornerVecX = -32;
        gSprites[i].centerToCornerVecY = -32;
    }
    memcpy(sOriginal, gSprites, sizeof(sOriginal));

    // BW2 normal threshold: 900 WAIT updates, then admission on the next one.
    Ready();
    for (i = 0; i < BATTLE_CAMERA_WAIT_TICKS; i++)
    {
        Tick(0);
        assert(!sCamera.active && !sOwnsScanlines);
    }
    Tick(0);
    assert(sCamera.active && sIdleFrames == BATTLE_CAMERA_WAIT_TICKS);

    // Exhaust every leg of every authored track: bounds, contacts, exact restore.
    for (track = 0; track < ARRAY_COUNT(sIdleTracks); track++)
    {
        u32 duration = 0, n;
        const struct CameraTrack *spec = &sIdleTracks[track];
        for (i = 0; i < spec->count; i++)
        {
            assert(spec->keys[i].duration > 0);
            assert(spec->keys[i].x <= 256 && spec->keys[i].y <= 256 && spec->keys[i].zoom <= 256);
            duration += spec->keys[i].duration;
        }
        ForceTrack(track);
        for (n = 0; n < duration; n++)
        {
            Tick(0);
            assert(sCamera.active);
        }
        assert(sCamera.x == 0 && sCamera.y == 0 && sCamera.zoom == 256);
        Tick(0);
        assert(!sCamera.active && !sOwnsScanlines && sWaitTicks == 0);
        Tick(0); // No new 900-update wait between completed idle scripts.
        assert(sCamera.active && sCamera.track != track);
    }

    // Cosmetic selection has six outcomes and forbids consecutive repeats.
    Ready();
    {
        u32 seen = 0;
        for (i = 0; i < 256; i++)
        {
            u8 previous = sLastTrack;
            u8 pick = PickTrack();
            assert(pick < ARRAY_COUNT(sIdleTracks) && pick != previous);
            seen |= 1u << pick;
            sLastTrack = pick;
        }
        assert(seen == 63);
    }

    // Menu exit/interruption rearms the initial wait but preserves last choice.
    ForceTrack(0);
    for (i = 0; i < 100; i++) Tick(0);
    sChoosing = FALSE;
    Tick(0);
    assert(!sCamera.active && !sOwnsScanlines && sLastTrack == 0);
    sChoosing = TRUE;
    for (i = 0; i < BATTLE_CAMERA_WAIT_TICKS; i++)
    {
        Tick(0);
        assert(!sCamera.active);
    }
    Tick(0);
    assert(sCamera.active && sCamera.track != 0);

    // Busy renderer after the threshold stays READY; release starts immediately.
    Ready();
    gScanlineEffect.state = 1;
    gScanlineEffect.dmaDest = (void *)REG_ADDR_BG1HOFS;
    for (i = 0; i < BATTLE_CAMERA_WAIT_TICKS + 100; i++)
    {
        Tick(0);
        assert(!sCamera.active && gScanlineEffect.state == 1);
    }
    gScanlineEffect.state = 0;
    Tick(0);
    assert(sCamera.active);

    // Partial affine exhaustion rolls back sprites AND the floor.
    ForceTrack(0);
    sAllocated = 0xFFFFFFFE;
    for (i = 0; i < 50; i++) Tick(0xFFFFFFFE);
    assert(!sCamera.active && !sOwnsScanlines);

    // Original affine rotation and non-affine flip survive the slower path.
    ForceTrack(0);
    sAllocated = 1u << 7;
    gSprites[0].oam.affineMode = ST_OAM_AFFINE_NORMAL;
    gSprites[0].oam.matrixNum = 7;
    SetOamMatrix(7, 0, -256, 256, 0);
    gSprites[1].oam.matrixNum = 8;
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    for (i = 0; i < 89; i++) Tick(1u << 7);
    BattleCamera_BeginFrame();
    assert(sCamera.zoom == 284);
    assert(gOamMatrices[gSprites[0].oam.matrixNum].b == -230);
    assert(gOamMatrices[gSprites[0].oam.matrixNum].c == 230);
    assert(gOamMatrices[gSprites[1].oam.matrixNum].a == -232);
    gMain.oamLoadDisabled = TRUE;
    {
        u8 previous = gScanlineEffect.srcBuffer;
        BattleCamera_PublishScanlines();
        assert(gScanlineEffect.srcBuffer == (previous ^ 1));
    }
    gMain.oamLoadDisabled = FALSE;
    BattleCamera_EndFrame();
    assert(memcmp(sOriginal, gSprites, sizeof(sOriginal)) == 0 && sAllocated == (1u << 7));
    assert(gOamMatrices[7].b == -256 && gOamMatrices[7].c == 256);

    // Same-parameter ownership replacement must still be preserved.
    ScanlineEffect_SetParams((struct ScanlineEffectParams){(void *)REG_ADDR_BG3HOFS, SCANLINE_EFFECT_DMACNT_32BIT, 1, 0});
    Tick(1u << 7);
    assert(!sCamera.active && !sOwnsScanlines && gScanlineEffect.state == 1);
    gSprites[0].oam.affineMode = ST_OAM_AFFINE_OFF;
    gSprites[0].oam.matrixNum = gSprites[1].oam.matrixNum = 0;
    memcpy(sOriginal, gSprites, sizeof(sOriginal));

    // Existing local animation/global offsets suppress camera admission.
    Ready();
    gAnimScriptActive = TRUE;
    for (i = 0; i < BATTLE_CAMERA_WAIT_TICKS + 1; i++) Tick(0);
    assert(!sCamera.active);
    gAnimScriptActive = FALSE;
    gBattle_BG3_X = 1;
    for (i = 0; i < BATTLE_CAMERA_WAIT_TICKS + 1; i++) Tick(0);
    assert(!sCamera.active);
    gBattle_BG3_X = 0;

    // All environment IDs, battle flags, and doubles remain admitted.
    {
        u32 env, type, count, n;
        const u32 types[] = {0, BATTLE_TYPE_TRAINER, BATTLE_TYPE_LINK,
                            BATTLE_TYPE_FRONTIER, BATTLE_TYPE_RECORDED};
        for (env = 0; env < BATTLE_ENVIRONMENT_COUNT; env++)
        for (type = 0; type < ARRAY_COUNT(types); type++)
        for (count = 2; count <= 4; count += 2)
        {
            Ready();
            gBattlersCount = count;
            gBattleTypeFlags = types[type];
            BattleCamera_SetEnvironment(env);
            for (n = 0; n <= BATTLE_CAMERA_WAIT_TICKS + 100; n++) Tick(0);
            assert(sCamera.active);
        }
    }
    puts("PASS: BW2-style initial WAIT/READY/PLAY, six bounded tracks, no repeat, neutral restore, busy resume, restart delay, all battle types/doubles, UI/affine/ownership safety");
    return 0;
}
