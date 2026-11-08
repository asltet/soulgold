// Native arithmetic/lifecycle test for the actual prototype implementation.
// Hardware/OAM submission still requires emulator verification.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#undef ARM_FUNC
#define ARM_FUNC
#include "../../src/battle_camera.c"

const struct OamDimensions gOamDimensions[3][4] = {
    {{8,8},{16,16},{32,32},{64,64}},
    {{16,8},{32,8},{32,16},{64,32}},
    {{8,16},{8,32},{16,32},{32,64}},
};
struct Main gMain;
struct Sprite gSprites[MAX_SPRITES + 1];
struct OamMatrix gOamMatrices[OAM_MATRIX_COUNT];
struct PaletteFadeControl gPaletteFade;
struct BattleSpriteData *gBattleSpritesDataPtr;
u8 gBattlersCount;
u8 gBattlerPositions[MAX_BATTLERS_COUNT];
u8 gBattlerSpriteIds[MAX_BATTLERS_COUNT];
u32 gBattleTypeFlags;
u8 gAbsentBattlerFlags;
u32 gBattleControllerExecFlags;
struct BattlePokemon gBattleMons[MAX_BATTLERS_COUNT];
const u8 gBattleAnimMove_Headbutt[] = {1};
const u8 gBattleAnimMove_Peck[] = {2};
const u8 gBattleAnimMove_Ember[] = {3};
const u8 gBattleAnimMove_Splash[] = {4};
const u8 gBattleAnimMove_Withdraw[] = {5};
const u8 gBattleAnimMove_HelpingHand[] = {6};
const u8 gBattleAnimMove_Growl[] = {7};
const u8 gBattleAnimMove_TailWhip[] = {8};
const u8 gBattleAnimMove_RainDance[] = {9};

static const u8 sGenericMoveScript[] = {99};
static const struct AdditionalEffect sFreezeEffect[] = {{.moveEffect = MOVE_EFFECT_FREEZE}};
const struct MoveInfo gMovesInfo[MOVES_COUNT_ALL] = {
    [1 ... MOVES_COUNT_ALL - 1] = {.category = DAMAGE_CATEGORY_PHYSICAL, .battleAnimScript = sGenericMoveScript},
    [MOVE_ICE_BEAM] = {.category = DAMAGE_CATEGORY_SPECIAL, .battleAnimScript = sGenericMoveScript,
                       .additionalEffects = sFreezeEffect, .numAdditionalEffects = 1},
    [MOVE_HEADBUTT] = {.category = DAMAGE_CATEGORY_PHYSICAL, .makesContact = TRUE, .battleAnimScript = gBattleAnimMove_Headbutt},
    [MOVE_PECK] = {.category = DAMAGE_CATEGORY_PHYSICAL, .makesContact = TRUE, .battleAnimScript = gBattleAnimMove_Peck},
    [MOVE_EMBER] = {.category = DAMAGE_CATEGORY_SPECIAL, .makesContact = FALSE, .battleAnimScript = gBattleAnimMove_Ember},
    [MOVE_SPLASH] = {.category = DAMAGE_CATEGORY_STATUS, .makesContact = FALSE, .battleAnimScript = gBattleAnimMove_Splash},
    [MOVE_WITHDRAW] = {.category = DAMAGE_CATEGORY_STATUS, .makesContact = FALSE, .battleAnimScript = gBattleAnimMove_Withdraw},
    [MOVE_HELPING_HAND] = {.category = DAMAGE_CATEGORY_STATUS, .makesContact = FALSE, .battleAnimScript = gBattleAnimMove_HelpingHand},
    [MOVE_GROWL] = {.category = DAMAGE_CATEGORY_STATUS, .makesContact = FALSE, .battleAnimScript = gBattleAnimMove_Growl},
    [MOVE_TAIL_WHIP] = {.category = DAMAGE_CATEGORY_STATUS, .makesContact = FALSE, .battleAnimScript = gBattleAnimMove_TailWhip},
    [MOVE_RAIN_DANCE] = {.category = DAMAGE_CATEGORY_STATUS, .makesContact = FALSE, .battleAnimScript = gBattleAnimMove_RainDance},
};

u16 gBattle_BG3_X, gBattle_BG3_Y;
s16 gSpriteCoordOffsetX, gSpriteCoordOffsetY;
bool8 gAnimScriptActive;
struct ScanlineEffect gScanlineEffect;
u32 gScanlineEffectGeneration;
u16 ALIGNED(4) gScanlineEffectRegBuffers[2][0x3C0];
static bool32 sChoosing = TRUE;
static bool32 sDoubleCoords = FALSE;

u8 GetBattlerSpriteCoord(enum BattlerId battler, u8 coord)
{
    assert(coord == BATTLER_COORD_Y);
    if (sDoubleCoords)
    {
        static const u8 y[] = {80,40,84,32};
        return y[GetBattlerPosition(battler)];
    }
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
    assert(sAllocated & (1u << i));
    sAllocated &= ~(1u << i);
    SetOamMatrix(i, 256, 0, 0, 256);
}
void CalcCenterToCornerVec(struct Sprite *s, u8 shape, u8 size, u8 mode)
{
    assert(shape < 3 && size < 4);
    s->centerToCornerVecX = -gOamDimensions[shape][size].width / (mode == ST_OAM_AFFINE_DOUBLE ? 1 : 2);
    s->centerToCornerVecY = -gOamDimensions[shape][size].height / (mode == ST_OAM_AFFINE_DOUBLE ? 1 : 2);
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
    assert(abs(sCamera.y) <= BATTLE_CAMERA_MAX_PAN_Y * 256);
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

static void TestDepthProjection(void)
{
    u32 zoom, blend, environment;
    s32 y, source;
    Ready();
    sCamera.x = BATTLE_CAMERA_MAX_PAN_X * 256;
    sCamera.y = -512;
    sCamera.zoom = 288;
#if BATTLE_CAMERA_DEPTH_PRESET == 1
    // Independent golden Plain contacts: far/mid slopes 260/272, near288.
    assert(GroundY(32) == 32);
    assert(GroundY(72) == 75);
    assert(GroundY(120) == 129);
    assert(SceneScale(32, 256) == 260);
    assert(SceneScale(72, 256) == 272);
    assert(SceneScale(120, 256) == 288);
    assert(PanAt(32) == 6 && PanAt(72) == 8 && PanAt(120) == 16);
#endif
    assert(SceneScale(32, 256) < SceneScale(120, 256));
    assert(SceneScale(72, BATTLE_CAMERA_BATTLER_DEPTH_SCALE)
        == SceneScale(72, BATTLE_CAMERA_SHADOW_DEPTH_SCALE));
    for (zoom = 256; zoom <= BATTLE_CAMERA_MAX_ZOOM; zoom++)
        for (y = -BATTLE_CAMERA_MAX_PAN_Y * 256; y <= BATTLE_CAMERA_MAX_PAN_Y * 256; y += 256)
            for (blend = 0; blend <= 256; blend += 32)
            {
                sCamera.zoom = zoom;
                sCamera.y = y;
                sMoveCamera.active = TRUE;
                sMoveBlend = blend;
                assert(BuildRows());
                CheckRows();
                for (source = BATTLE_CAMERA_SOURCE_MIN_Y + 1; source <= BATTLE_CAMERA_SOURCE_MAX_Y; source++)
                    assert(GroundY(source) >= GroundY(source - 1));
                if (blend == 256)
                {
                    // Endpoint must remain the legacy uniform animation model.
                    for (source = 0; source <= 160; source++)
                        assert(abs(GroundY(source) - (32 + ((source - 32) * 256 - y) * (s32)zoom / 65536)) <= 1);
                    assert(SceneryPanAt(32) <= SceneryPanAt(72));
                    assert(SceneryPanAt(72) == PanAt(120));
                    sMoveCamera.translationOnly = TRUE;
                    assert(PanAt(32) == PanAt(120));
                    sMoveCamera.translationOnly = FALSE;
                }
            }
    Ready();
    assert(BuildRows());
    for (source = 0; source <= DISPLAY_HEIGHT; source++)
        assert(sRows[source] == 0); // Neutral must be bit-exact identity.
    BattleCamera_SetEnvironment(BATTLE_ENVIRONMENT_CAVE);
    assert(sSceneDepthStrength == BATTLE_CAMERA_OTHER_ENV_DEPTH);
    for (environment = 0; environment < BATTLE_ENVIRONMENT_COUNT; environment++)
    {
        BattleCamera_SetEnvironment(environment);
        sCamera.x = BATTLE_CAMERA_MAX_PAN_X * 256;
        sCamera.y = -BATTLE_CAMERA_MAX_PAN_Y * 256;
        sCamera.zoom = BATTLE_CAMERA_MAX_ZOOM;
        for (blend = 0; blend <= 256; blend += 128)
        {
            sMoveCamera.active = TRUE;
            sMoveBlend = blend;
            assert(BuildRows());
            CheckRows();
        }
    }
    BattleCamera_SetEnvironment(BATTLE_ENVIRONMENT_COUNT);
    assert(!sSceneReady);
    Ready();
    puts("PASS: depth contacts, scale planes, monotonic floor/inverse, full envelope/blend sweep, neutral identity, environment fallback");
}

static void TestCinematicMath(void)
{
    u32 curve, frame;
    s32 previous;
    for (curve = 0; curve <= BATTLE_CAMERA_EASING_SMOOTHERSTEP; curve++)
    {
        previous = 0;
        assert(EasingWeight(0, 1000, curve) == 0);
        assert(EasingWeight(1000, 1000, curve) == 256);
        assert(EasingWeight(2000, 1000, curve) == 256);
        for (frame = 0; frame <= 1000; frame++)
        {
            s32 weight = EasingWeight(frame, 1000, curve);
            assert(weight >= previous && weight <= 256);
            previous = weight;
        }
    }
    Ready();
    sCamera.zoom = 284;
    assert(BuildRows() && sRowsValid);
    {
        u32 original = sRows[100];
        // Sentinel confirms an exact cache hit without a second generation pass.
        sRows[100] = 0x1234;
        assert(BuildRows() && sRows[100] == 0x1234);
        sRows[100] = original;
        sShakeX = 256;
        assert(BuildRows() && sRowKey.x == 256);
        sShakeX = 0;
        sMoveCamera.active = TRUE;
        sMoveBlend = 128;
        assert(BuildRows() && sRowKey.blend == 128);
        sSceneDepthStrength = 128;
        assert(BuildRows() && sRowKey.depth == 128);
        sCamera.x = -10000;
        // RenderX clamps the shake/source envelope before sampling.
        assert(BuildRows() && sRowKey.x == 0);
    }
    {
        struct Sprite identity = {0};
        struct OamMatrix saved = gOamMatrices[7];
        identity.oam.affineMode = ST_OAM_AFFINE_NORMAL;
        identity.oam.matrixNum = 7;
        SetOamMatrix(7, 256, 0, 0, 256);
        assert(!HasUncertainBounds(&identity));
        SetOamMatrix(7, -256, 0, 0, 256);
        assert(!HasUncertainBounds(&identity));
        SetOamMatrix(7, 0, -256, 256, 0);
        assert(HasUncertainBounds(&identity));
        gOamMatrices[7] = saved;
    }
    Ready();
    {
        struct Sprite saved[MAX_SPRITES + 1];
        u32 i;
        memcpy(saved, gSprites, sizeof(saved));
        Ready();
        sMoveCamera.active = TRUE;
        sCamera.zoom = 280;
        sMoveBlend = 256;
        sAllocated = 0xFFFFFFFE;
        for (i = 0; i < 8; i++)
        {
            gSprites[i] = saved[0];
            gSprites[i].oam.affineMode = ST_OAM_AFFINE_OFF;
            gSprites[i].oam.matrixNum = 0;
            gSprites[i].invisible = FALSE;
            assert(TransformSprite(i, gSprites[i].x, 120, 280));
            assert(gSprites[i].oam.matrixNum == 0);
        }
        assert(sAllocated == 0xFFFFFFFF);
        BattleCamera_EndFrame();
        assert(sAllocated == 0xFFFFFFFE && sBackupCount == 0);
        memcpy(gSprites, saved, sizeof(saved));
        memcpy(sOriginal, saved, sizeof(saved));
        Ready();
    }
    puts("PASS: easing/cache, shared affine slots and resource cleanup");
}

static void TestMoveCamera(void);

int main(void)
{
    static struct BattleSpriteData data;
    static struct BattleHealthboxInfo boxes[MAX_BATTLERS_COUNT];
    static struct BattleSpriteInfo battlerData[MAX_BATTLERS_COUNT];
    u32 i, track;
    data.healthBoxesData = boxes;
    data.battlerData = battlerData;
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

    TestDepthProjection();
    TestCinematicMath();

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
        u16 minX = 256, maxX = 0;
        const struct CameraTrack *spec = &sIdleTracks[track];
        for (i = 0; i < spec->count; i++)
        {
            assert(spec->keys[i].duration > 0);
            assert(spec->keys[i].x <= 256 && spec->keys[i].y <= 256 && spec->keys[i].zoom <= 256);
            duration += spec->keys[i].duration;
            if (i + 1 < spec->count)
            {
                minX = min(minX, spec->keys[i].x);
                maxX = max(maxX, spec->keys[i].x);
            }
        }
        assert(maxX - minX >= 160); // Every idle family has a meaningful lateral sweep.
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

    ForceTrack(0);
    for (i = 0; i < 90; i++) Tick(0);
    assert(PanAt(BATTLE_CAMERA_NEAR_CONTACT) == 16);
    assert(PanAt(BATTLE_CAMERA_HORIZON) < PanAt(BATTLE_CAMERA_NEAR_CONTACT));
    assert(sCamera.y == -BATTLE_CAMERA_IDLE_PAN_Y * 256);

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
    {
        s16 beforeX = sCamera.x;
        s16 beforeZoom = sCamera.zoom;
        Tick(0);
        assert(sCamera.active && sMenuReturning && sCamera.zoom > 256);
        assert(sCamera.x > 0 && sCamera.x <= beforeX && sCamera.zoom <= beforeZoom);
        for (i = 1; i < BATTLE_CAMERA_HANDOFF_TICKS; i++) Tick(0);
    }
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
    assert(gOamMatrices[gSprites[1].oam.matrixNum].a == -(65536 / SceneScale(72, BATTLE_CAMERA_BATTLER_DEPTH_SCALE)));
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
    TestMoveCamera();
    puts("PASS: BW2-style initial WAIT/READY/PLAY, six bounded tracks, no repeat, neutral restore, busy resume, restart delay, all battle types/doubles, UI/affine/ownership safety");
    return 0;
}

static void StartTestMove(u16 move, u8 attacker, u8 target, u8 type, const u8 *script)
{
    gAnimScriptActive = TRUE;
    sChoosing = FALSE;
    BattleCamera_InterruptAnimation();
    BattleCamera_StartMove(move, attacker, target, type, script);
    Tick(0); // Consume the queued executed-animation event at the snapshot boundary.
}

static void FinishTestMove(void)
{
    u32 i;
    BattleCamera_EndMove();
    gAnimScriptActive = FALSE;
    BattleCamera_FinishAction();
    for (i = 0; i < BATTLE_CAMERA_RETURN_TICKS + 4; i++) Tick(0);
    assert(!sMoveCamera.active && !sCamera.active && !sOwnsScanlines);
    assert(sCamera.x == 0 && sCamera.y == 0 && sCamera.zoom == 256);
}

static void TestMoveCamera(void)
{
    u32 i, n;
    Ready();
    gBattlersCount = 4;
    for (i = 0; i < MAX_BATTLERS_COUNT; i++) gBattleMons[i].hp = 100;
    gAbsentBattlerFlags = 0;

    // Launch events cannot mutate the currently published idle pose/lease.
    ForceTrack(0);
    for (i = 0; i < 10; i++) Tick(0);
    {
        u32 generation = gScanlineEffectGeneration;
        s16 oldX = sCamera.x;
        gAnimScriptActive = TRUE;
        sChoosing = FALSE;
        BattleCamera_StartMove(MOVE_EMBER, 0, 1, TARGET_SELECTED, gBattleAnimMove_Ember);
        assert(!sMoveCamera.active && sCamera.x == oldX && gScanlineEffectGeneration == generation);
        Tick(0);
        assert(sMoveCamera.active && gScanlineEffectGeneration != generation);
        FinishTestMove();
    }
    // A rapid executed move inherits a fully displaced idle pose, not neutral.
    ForceTrack(0);
    for (i = 0; i < 90; i++) Tick(0);
    {
        s16 oldDrawX[2], oldDrawY[2];
        BattleCamera_BeginFrame();
        for (i = 0; i < 2; i++)
        {
            oldDrawX[i] = gSprites[gBattlerSpriteIds[i]].x;
            oldDrawY[i] = gSprites[gBattlerSpriteIds[i]].y;
        }
        BattleCamera_EndFrame();
        gAnimScriptActive = TRUE;
        sChoosing = FALSE;
        BattleCamera_InterruptAnimation();
        BattleCamera_StartMove(MOVE_EMBER, 0, 1, TARGET_SELECTED, gBattleAnimMove_Ember);
        BattleCamera_BeginFrame();
        assert(sMoveCamera.active && sCamera.x > 0 && sCamera.zoom > 256);
        assert(sMoveCamera.duration >= BATTLE_CAMERA_HANDOFF_TICKS);
        assert(sMoveBlend < 256);
        for (i = 0; i < 2; i++)
        {
            assert(abs(gSprites[gBattlerSpriteIds[i]].x - oldDrawX[i]) <= 1);
            assert(abs(gSprites[gBattlerSpriteIds[i]].y - oldDrawY[i]) <= 1);
        }
        BattleCamera_EndFrame();
        assert(memcmp(sOriginal, gSprites, sizeof(sOriginal)) == 0 && sAllocated == 0);
        for (i = 1; i < BATTLE_CAMERA_HANDOFF_TICKS; i++) Tick(0);
        assert(sMoveBlend == 256);
        FinishTestMove();
    }
    // First, second, third and subsequent executed moves, on both sides.
    for (i = 0; i < 20; i++)
    {
        u8 attacker = i & 1;
        StartTestMove(MOVE_PECK, attacker, attacker ^ 1, TARGET_SELECTED, gBattleAnimMove_Peck);
        assert(!sMoveCamera.blocked && sMoveCamera.target == (attacker ^ 1));
        for (n = 0; n < 24; n++) Tick(0);
        assert(sCamera.active && sMoveCamera.phase == CAMERA_PHASE_ATTACK_PRESENTATION);
        FinishTestMove();
    }
    // A new animation may supersede an unfinished return without a stuck lease.
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    for (i = 0; i < 12; i++) Tick(0);
    BattleCamera_EndMove();
    gAnimScriptActive = FALSE;
    BattleCamera_FinishAction();
    Tick(0);
    assert(sMoveCamera.phase == CAMERA_PHASE_RETURN);
    StartTestMove(MOVE_EMBER, 1, 0, TARGET_SELECTED, gBattleAnimMove_Ember);
    assert(sMoveCamera.attacker == 1 && sMoveCamera.target == 0 && !sMoveCamera.blocked);
    FinishTestMove();
    // Self support must not focus the foe, and an explicit ally remains the ally.
    StartTestMove(MOVE_WITHDRAW, 0, 1, TARGET_USER, gBattleAnimMove_Withdraw);
    assert(sMoveCamera.target == 0 && sMoveCamera.targetClass == CAMERA_TARGET_SELF);
    for (i = 0; i < 30; i++) Tick(0);
    assert(sCamera.targetX == 0);
    FinishTestMove();
    StartTestMove(MOVE_HELPING_HAND, 0, 2, TARGET_ALLY, gBattleAnimMove_HelpingHand);
    assert(sMoveCamera.targetClass == CAMERA_TARGET_ALLY && sMoveCamera.targetMask == 4);
    for (i = 0; i < 24; i++) Tick(0);
    FinishTestMove();

    // Redirection uses the resolved ID, never the opposite-slot assumption.
    StartTestMove(MOVE_EMBER, 0, 3, TARGET_SELECTED, gBattleAnimMove_Ember);
    assert(sMoveCamera.target == 3 && sMoveCamera.targetMask == 8);
    // Projectile is registered at the same logical coordinates as its endpoint.
    gSprites[9] = gSprites[gBattlerSpriteIds[3]];
    gSprites[9].oam.affineMode = ST_OAM_AFFINE_OFF;
    gSprites[9].x2 = gSprites[9].y2 = 0;
    BattleCamera_OnSpriteCreated(9, ANIM_TAG_SMALL_EMBER);
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    for (i = 0; i < 40; i++)
    {
        BattleCamera_BeginFrame();
        assert(gSprites[9].x == gSprites[gBattlerSpriteIds[3]].x);
        assert(gSprites[9].y == gSprites[gBattlerSpriteIds[3]].y);
        BattleCamera_EndFrame();
        assert(memcmp(sOriginal, gSprites, sizeof(sOriginal)) == 0 && sAllocated == 0);
    }
    // Above-horizon/elevated endpoints must use the same integer translation.
    gSprites[gBattlerSpriteIds[3]].y = gSprites[9].y = 18;
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    for (i = 0; i < 8; i++)
    {
        BattleCamera_BeginFrame();
        assert(gSprites[9].y == gSprites[gBattlerSpriteIds[3]].y);
        BattleCamera_EndFrame();
        assert(memcmp(sOriginal, gSprites, sizeof(sOriginal)) == 0);
    }
    gSprites[gBattlerSpriteIds[3]].y = gSprites[9].y = 69;
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    FinishTestMove();
    // Sprite-slot reuse outside animation must not retain effect membership.
    BattleCamera_OnSpriteCreated(9, 0);
    assert(!(sMoveCamera.effects[0] & (1u << 9)));

    // Hardware-hidden helper sprites must never become visible affine objects.
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    gSprites[10] = gSprites[0];
    gSprites[10].oam.affineMode = ST_OAM_AFFINE_ERASE;
    BattleCamera_OnSpriteCreated(10, 0);
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    for (i = 0; i < 12; i++)
    {
        BattleCamera_BeginFrame();
        assert(gSprites[10].oam.affineMode == ST_OAM_AFFINE_ERASE);
        BattleCamera_EndFrame();
        assert(memcmp(sOriginal, gSprites, sizeof(sOriginal)) == 0 && sAllocated == 0);
    }
    FinishTestMove();
    // Bounds-aware framing: retain a large actor at the edge, reduce extra zoom.
    {
        u8 id = gBattlerSpriteIds[1];
        s16 oldX = gSprites[id].x;
        gSprites[id].x = 207;
        memcpy(sOriginal, gSprites, sizeof(sOriginal));
        StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
        // Pan creates room on the right; changing to self/no pan is stricter.
        SetMoveGoal(0, 8, 12);
        assert(sCamera.targetZoom < 256 + BATTLE_CAMERA_TARGET_ZOOM_DELTA);
        FinishTestMove();
        gSprites[id].oam.size = 1; // Small16px art at the same location needs less room.
        gSprites[id].centerToCornerVecX = gSprites[id].centerToCornerVecY = -8;
        memcpy(sOriginal, gSprites, sizeof(sOriginal));
        StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
        SetMoveGoal(0, 8, 12);
        assert(sCamera.targetZoom > 256);
        for (i = 0; i < 10; i++) Tick(0);
        FinishTestMove();
        gSprites[id].oam.size = 3;
        gSprites[id].centerToCornerVecX = gSprites[id].centerToCornerVecY = -32;
        gSprites[id].x = oldX;
        memcpy(sOriginal, gSprites, sizeof(sOriginal));
    }

    // Melee opens on the user, then actual hitsplat creation requests impact focus.
    StartTestMove(MOVE_HEADBUTT, 0, 1, TARGET_SELECTED, gBattleAnimMove_Headbutt);
    assert(sCamera.targetX == 0);
    for (i = 0; i < 4; i++) Tick(0);
    BattleCamera_OnSpriteCreated(10, ANIM_TAG_IMPACT);
    assert(sMoveCamera.phase == CAMERA_PHASE_TARGET_FOCUS && sMoveCamera.targetAcquired && sCamera.targetX > 0);
    {
        u16 elapsed;
        s16 goal = sCamera.targetX;
        for (i = 0; i < 2; i++) Tick(0);
        elapsed = sMoveCamera.elapsed;
        BattleCamera_OnSpriteCreated(11, ANIM_TAG_IMPACT);
        assert(sMoveCamera.elapsed == elapsed && sCamera.targetX == goal);
        assert(sMoveCamera.impactSeen && sMoveCamera.shakeTick == 0);
        for (i = 0; i < 10; i++)
        {
            Tick(0);
            assert(abs(sShakeX) <= BATTLE_CAMERA_MAX_SHAKE_X * 256);
            assert(abs(sShakeY) <= BATTLE_CAMERA_MAX_SHAKE_Y * 256);
        }
        assert(sShakeX == 0 && sShakeY == 0);
    }
    FinishTestMove();
    assert(sShakeX == 0 && sShakeY == 0 && !sRowsValid);
    StartTestMove(MOVE_HEADBUTT, 1, 0, TARGET_SELECTED, gBattleAnimMove_Headbutt);
    assert(!sMoveCamera.impactSeen);
    BattleCamera_OnSpriteCreated(11, ANIM_TAG_IMPACT);
    assert(sMoveCamera.impactSeen && sMoveCamera.shakeTick == 0);
    Tick(0);
    BattleCamera_InterruptAnimation();
    Tick(0);
    assert(!sMoveCamera.active && sShakeX == 0 && sShakeY == 0);
    gAnimScriptActive = FALSE;

    // Spread and field are stable group/neutral framing, never first-target focus.
    StartTestMove(MOVE_GROWL, 0, 1, TARGET_BOTH, gBattleAnimMove_Growl);
    assert(sMoveCamera.targetClass == CAMERA_TARGET_GROUP && sMoveCamera.targetMask == 10);
    for (i = 0; i < 30; i++)
    {
        Tick(0);
        assert(sCamera.active && sCamera.zoom <= 256 + BATTLE_CAMERA_GROUP_ZOOM_DELTA && sCamera.x <= BATTLE_CAMERA_MOVE_PAN_X * 256);
    }
    FinishTestMove();
    StartTestMove(MOVE_RAIN_DANCE, 0, MAX_BATTLERS_COUNT, TARGET_FIELD, gBattleAnimMove_RainDance);
    assert(sMoveCamera.targetClass == CAMERA_TARGET_FIELD && sMoveCamera.targetMask == 0);
    for (i = 0; i < 24; i++) Tick(0);
    FinishTestMove();
    // Missing targets, unreviewed moves or a substituted script retain neutral.
    StartTestMove(MOVE_EMBER, 0, MAX_BATTLERS_COUNT, TARGET_SELECTED, gBattleAnimMove_Ember);
    assert(sMoveCamera.blocked);
    for (i = 0; i < 24; i++) Tick(0);
    FinishTestMove();
    StartTestMove(MOVE_POUND, 0, 1, TARGET_SELECTED, NULL);
    assert(sMoveCamera.blocked);
    FinishTestMove();
    StartTestMove(MOVE_EMBER, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    assert(sMoveCamera.blocked);
    FinishTestMove();

    // Disappearance, fainting, rebinding and battle exit invalidate presentation.
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    Tick(0);
    gSprites[gBattlerSpriteIds[1]].invisible = TRUE;
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    Tick(0);
    assert(!sMoveCamera.active);
    gSprites[gBattlerSpriteIds[1]].invisible = FALSE;
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    Tick(0);
    gBattleMons[1].hp = 0;
    for (i = 0; i < 20; i++) Tick(0);
    assert(sMoveCamera.active); // Visible KO target may still have a draining bar.
    gAnimScriptActive = FALSE;
    BattleCamera_FinishAction();
    Tick(0);
    assert(sMoveCamera.phase == CAMERA_PHASE_RETURN); // Zero HP alone must not snap the visible scene.
    for (i = 0; i < BATTLE_CAMERA_RETURN_TICKS; i++) Tick(0);
    assert(!sMoveCamera.active);
    gBattleMons[1].hp = 100;
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    Tick(0);
    BattleCamera_OnSpriteCreated(gBattlerSpriteIds[1], 0);
    Tick(0);
    assert(!sMoveCamera.active && !sOwnsScanlines);
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    Tick(0);
    gMain.inBattle = FALSE;
    Tick(0);
    assert(!sMoveCamera.active);
    gMain.inBattle = TRUE;
    Ready();
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    for (i = 0; i < 24; i++) Tick(0);
    assert(sCamera.active);
    FinishTestMove();

    // Interrupted scripts and unsupported BG commands cannot leave a stuck shot.
    StartTestMove(MOVE_EMBER, 0, 1, TARGET_SELECTED, gBattleAnimMove_Ember);
    for (i = 0; i < 12; i++) Tick(0);
    gAnimScriptActive = FALSE;
    BattleCamera_FinishAction();
    for (i = 0; i < BATTLE_CAMERA_RETURN_TICKS + 4; i++) Tick(0);
    assert(!sMoveCamera.active);
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    Tick(0);
    BattleCamera_BlockMove();
    BattleCamera_EndMove(); // End cannot override an unsupported-command cancellation.
    Tick(0);
    assert(sMoveCamera.active && sMoveCamera.blocked && sMoveCamera.suspended && !sOwnsScanlines);
    gAnimScriptActive = FALSE;
    for (i = 0; i < 40; i++) Tick(0);
    assert(!sMoveCamera.active);

    // Malformed/extreme animation coordinates fail closed before projection.
    StartTestMove(MOVE_EMBER, 0, 1, TARGET_SELECTED, gBattleAnimMove_Ember);
    gSprites[9].y = 2000;
    BattleCamera_OnSpriteCreated(9, ANIM_TAG_SMALL_EMBER);
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    Tick(0);
    assert(!sMoveCamera.active && !sOwnsScanlines);
    gSprites[9].y = 40;
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    gAnimScriptActive = FALSE;
    // Matrix exhaustion cancels the entire move, not only actor scaling.
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    sAllocated = 0xFFFFFFFE;
    BattleCamera_OnHitReaction(1, TRUE);
    for (i = 0; i < 12; i++) Tick(0xFFFFFFFE);
    assert(!sMoveCamera.active && !sOwnsScanlines);
    sAllocated = 0;
    gAnimScriptActive = FALSE;

    // A replacement scanline owner with identical parameters is never stopped.
    StartTestMove(MOVE_EMBER, 0, 1, TARGET_SELECTED, gBattleAnimMove_Ember);
    for (i = 0; i < 12; i++) Tick(0);
    ScanlineEffect_SetParams((struct ScanlineEffectParams){(void *)REG_ADDR_BG3HOFS, SCANLINE_EFFECT_DMACNT_32BIT, 1, 0});
    Tick(0);
    assert(sMoveCamera.active && sMoveCamera.blocked && !sOwnsScanlines && gScanlineEffect.state == 1);
    ScanlineEffect_Stop();
    gAnimScriptActive = FALSE;
    // Local move affine state is composed in temporary slots and never reset.
    gSprites[0].oam.affineMode = ST_OAM_AFFINE_NORMAL;
    gSprites[0].oam.matrixNum = 7;
    SetOamMatrix(7, 0, -256, 256, 0);
    sAllocated = 1u << 7;
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    gAnimScriptActive = TRUE;
    BattleCamera_StartMove(MOVE_WITHDRAW, 0, 0, TARGET_USER, gBattleAnimMove_Withdraw);
    for (i = 0; i < BATTLE_CAMERA_RETURN_TICKS; i++)
    {
        Tick(1u << 7);
        assert(gOamMatrices[7].b == -256 && gOamMatrices[7].c == 256);
    }
    BattleCamera_EndMove();
    gAnimScriptActive = FALSE;
    BattleCamera_FinishAction();
    for (i = 0; i < BATTLE_CAMERA_RETURN_TICKS + 4; i++) Tick(1u << 7);
    assert(!sMoveCamera.active && sAllocated == (1u << 7));
    gSprites[0].oam.affineMode = ST_OAM_AFFINE_OFF;
    gSprites[0].oam.matrixNum = 0;
    sAllocated = 0;
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    // Logical-update replay is invariant to grouping into 1x or4x frames.
    {
        struct BattleCamera reference;
        u8 referencePhase;
        u32 frame, tick;
        StartTestMove(MOVE_EMBER, 0, 1, TARGET_SELECTED, gBattleAnimMove_Ember);
        for (frame = 0; frame < 16; frame++) Tick(0);
        assert(sMoveCamera.phase == CAMERA_PHASE_TARGET_FOCUS);
        reference = sCamera;
        referencePhase = sMoveCamera.phase;
        FinishTestMove();
        StartTestMove(MOVE_EMBER, 0, 1, TARGET_SELECTED, gBattleAnimMove_Ember);
        for (frame = 0; frame < 4; frame++)
            for (tick = 0; tick < 4; tick++) Tick(0);
        assert(sCamera.x == reference.x && sCamera.y == reference.y && sCamera.zoom == reference.zoom);
        assert(sMoveCamera.phase == referencePhase);
        FinishTestMove();
    }
    // Replacement scripts/fainting/substitute transitions retain neutral safely.
    gBattleSpritesDataPtr->battlerData[1].behindSubstitute = TRUE;
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    assert(sMoveCamera.blocked && !sCamera.active);
    FinishTestMove();
    gBattleSpritesDataPtr->battlerData[1].behindSubstitute = FALSE;
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    Tick(0);
    BattleCamera_InterruptAnimation();
    Tick(0);
    assert(!sMoveCamera.active);
    gAnimScriptActive = FALSE;

    // Zero-update/skipped animation never forces a cinematic or a camera wait.
    gAnimScriptActive = TRUE;
    BattleCamera_InterruptAnimation();
    BattleCamera_StartMove(MOVE_EMBER, 0, 1, TARGET_SELECTED, gBattleAnimMove_Ember);
    BattleCamera_EndMove();
    gAnimScriptActive = FALSE;
    Tick(0);
    assert(!sMoveCamera.active && !sOwnsScanlines);
    // Watchdog bounds broken animation callbacks without touching their state.
    StartTestMove(MOVE_EMBER, 0, 1, TARGET_SELECTED, gBattleAnimMove_Ember);
    for (i = 0; i <= BATTLE_CAMERA_MOVE_MAX_TICKS; i++) Tick(0);
    assert(!sMoveCamera.active && gAnimScriptActive);
    gAnimScriptActive = FALSE;
    // Every valid move ID is eligible, without adding a per-move whitelist row.
    // Synthetic metadata proves dispatch breadth, not all real animation callbacks.
    {
        u32 move;
        for (move = 1; move < MOVES_COUNT_ALL; move++)
        {
            u8 type = move % 5 == 0 ? TARGET_FIELD : move % 5 == 1 ? TARGET_USER
                    : move % 5 == 2 ? TARGET_ALLY : move % 5 == 3 ? TARGET_BOTH : TARGET_SELECTED;
            u8 target = type == TARGET_FIELD ? MAX_BATTLERS_COUNT : type == TARGET_ALLY ? 2 : 1;
            StartTestMove(move, 0, target, type, gMovesInfo[move].battleAnimScript);
            assert(sMoveCamera.active && !sMoveCamera.blocked);
            for (n = 0; n < 4; n++) Tick(0);
            FinishTestMove();
        }
        printf("PASS: universal dispatcher for %u synthetic canonical move IDs\n", MOVES_COUNT_ALL - 1);
    }
    // Approved Ember retains exactly the previous timing/strength configuration.
    assert(sMoveProfiles[MOVE_CAMERA_EMBER].start == 8);
    assert(sMoveProfiles[MOVE_CAMERA_EMBER].travel == 24);
    assert(sMoveProfiles[MOVE_CAMERA_EMBER].impact == 6);
    assert(sMoveProfiles[MOVE_CAMERA_EMBER].restore == 10);
    assert(sMoveProfiles[MOVE_CAMERA_EMBER].pan == 256);
    assert(sMoveProfiles[MOVE_CAMERA_EMBER].zoom == BATTLE_CAMERA_MOVE_ZOOM_DELTA);

    // Target-only patterns do not restart acquisition on impact or return early.
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    for (i = 0; i < 12; i++) Tick(0);
    {
        u16 elapsed = sMoveCamera.elapsed;
        BattleCamera_OnSpriteCreated(10, ANIM_TAG_IMPACT);
        assert(sMoveCamera.elapsed == elapsed);
    }
    for (i = 0; i < 80; i++) Tick(0);
    assert(sMoveCamera.phase == CAMERA_PHASE_ATTACK_PRESENTATION && sCamera.x > 0);
    BattleCamera_EndMove();
    gAnimScriptActive = FALSE;
    BattleCamera_FinishAction();
    {
        s16 previous = sCamera.x;
        for (i = 0; i < BATTLE_CAMERA_RETURN_TICKS; i++)
        {
            Tick(0);
            assert(sCamera.x <= previous && previous - sCamera.x <= 512);
            previous = sCamera.x;
        }
        assert(!sMoveCamera.active && sCamera.x == 0);
    }

    // Effect creation at the attacker is not target contact; arrival unlocks zoom.
    StartTestMove(MOVE_VINE_WHIP, 0, 1, TARGET_SELECTED, sGenericMoveScript);
    assert(sMoveCamera.generic && sCamera.targetZoom > 256);
    for (i = 0; i < 40; i++) Tick(0);
    assert(sCamera.zoom == 256);
    {
        struct Sprite saved = gSprites[9];
        gSprites[9].inUse = TRUE;
        gSprites[9].invisible = FALSE;
        gSprites[9].x = gSprites[0].x; gSprites[9].y = gSprites[0].y;
        gSprites[9].x2 = gSprites[9].y2 = 0;
        memcpy(sOriginal, gSprites, sizeof(sOriginal));
        BattleCamera_OnSpriteCreated(9, 0);
        for (i = 0; i < 40; i++) Tick(0);
        assert(!sMoveCamera.contactSeen && sCamera.zoom == 256);
        gSprites[9].x = gSprites[1].x; gSprites[9].y = gSprites[1].y;
        memcpy(sOriginal, gSprites, sizeof(sOriginal));
        Tick(0);
        assert(sMoveCamera.contactSeen && sCamera.zoom <= 257);
        for (i = 0; i < 40; i++) Tick(0);
        assert(sCamera.zoom > 256 && sMoveCamera.zoomElapsed == sMoveCamera.duration);
        FinishTestMove();
        gSprites[9] = saved;
        memcpy(sOriginal, gSprites, sizeof(sOriginal));
    }
    // Same strong budget for self, ally recipient and incoming near-side hits.
    {
        static const u16 moves[] = {MOVE_WITHDRAW, MOVE_HELPING_HAND, MOVE_PECK};
        static const u8 attackers[] = {0, 0, 1};
        static const u8 recipients[] = {0, 2, 0};
        static const u8 types[] = {TARGET_USER, TARGET_ALLY, TARGET_SELECTED};
        const u8 *scripts[] = {gBattleAnimMove_Withdraw, gBattleAnimMove_HelpingHand, gBattleAnimMove_Peck};
        u32 n;
        for (n = 0; n < 3; n++)
        {
            StartTestMove(moves[n], attackers[n], recipients[n], types[n], scripts[n]);
            assert(sCamera.targetZoom > 256 && (sMoveCamera.targetMask & (1u << recipients[n])));
            for (i = 0; i < 40; i++) Tick(0);
            assert(!sMoveCamera.contactSeen && sCamera.zoom == 256);
            if (n == 2)
                BattleCamera_OnHitReaction(recipients[n], TRUE);
            else
                BattleCamera_OnAnimationLaunch(ANIM_TYPE_GENERAL, B_ANIM_STATS_CHANGE, recipients[n]);
            Tick(0);
            assert(sMoveCamera.contactSeen && sCamera.zoom <= 257);
            for (i = 0; i < 40; i++) Tick(0);
            assert(sCamera.zoom > 256);
            if (n == 2)
                BattleCamera_OnHitReaction(recipients[n], FALSE);
            FinishTestMove();
        }
    }
    // Possible freeze reactions reserve1x so later monbg status cannot reset the shot.
    StartTestMove(MOVE_ICE_BEAM, 0, 1, TARGET_SELECTED, sGenericMoveScript);
    assert(sMoveCamera.translationOnly && sCamera.targetZoom == 256 && sCamera.targetY != 0);
    BattleCamera_OnSpriteCreated(9, 0);
    for (i = 0; i < 40; i++) Tick(0);
    assert(sCamera.zoom == 256);
    BattleCamera_EndMove();
    gAnimScriptActive = FALSE;
    Tick(0);
    BattleCamera_OnAnimationLaunch(ANIM_TYPE_STATUS, B_ANIM_STATUS_FRZ, 1);
    gAnimScriptActive = TRUE;
    BattleCamera_AttachMonBg(1, 1);
    Tick(0);
    assert(sMoveCamera.active && !sMoveCamera.blocked && sCamera.zoom == 256);
    BattleCamera_ClearMonBg(1);
    FinishTestMove();

    // A travel profile holds user focus until actual visual completion, not age.
    StartTestMove(MOVE_WATER_GUN, 0, 1, TARGET_SELECTED, sGenericMoveScript);
    assert(sMoveCamera.framing == BW2_FRAME_TRAVEL && sCamera.targetX == 0);
    for (i = 0; i < 40; i++) Tick(0);
    assert(!sMoveCamera.targetAcquired && sCamera.targetX == 0);
    BattleCamera_OnVisualBarrier();
    assert(!sMoveCamera.targetAcquired); // No actual visuals yet.
    BattleCamera_OnSpriteCreated(9, 0);
    BattleCamera_OnVisualBarrier();
    assert(sMoveCamera.targetAcquired && sCamera.targetX > 0);
    Tick(0);
    {
        u16 elapsed = sMoveCamera.elapsed;
        BattleCamera_OnVisualBarrier();
        assert(sMoveCamera.elapsed == elapsed);
    }
    for (i = 0; i < 60; i++) Tick(0);
    assert(sMoveCamera.active && sCamera.x > 0 && gAnimScriptActive);
    FinishTestMove();

    // Mon-as-BG images follow the exact same1x translation while hidden in OBJ.
    StartTestMove(MOVE_TACKLE, 0, 1, TARGET_SELECTED, sGenericMoveScript);
    for (i = 0; i < 18; i++) Tick(0);
    BattleCamera_AttachMonBg(1, 1);
    assert(sMoveCamera.bgLayer[1] == 1 && !sMoveCamera.blocked);
    gSprites[gBattlerSpriteIds[1]].invisible = TRUE;
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    for (i = 0; i < 30; i++)
    {
        Tick(0);
        assert(sCamera.active && sCamera.zoom == 256);
        assert(BattleCamera_GetMonBgOffset(1, FALSE) == PanAt(72));
        assert(GroundY(72) - 72 == -BattleCamera_GetMonBgOffset(1, TRUE));
        assert(BattleCamera_GetMonBgOffset(2, FALSE) == 0);
    }
    BattleCamera_AttachMonBg(2, 2);
    gSprites[gBattlerSpriteIds[2]].invisible = TRUE;
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    for (i = 0; i < 4; i++)
    {
        Tick(0);
        assert(BattleCamera_GetMonBgOffset(1, FALSE) == BattleCamera_GetMonBgOffset(2, FALSE));
        assert(BattleCamera_GetMonBgOffset(1, TRUE) == BattleCamera_GetMonBgOffset(2, TRUE));
    }
    gSprites[gBattlerSpriteIds[2]].invisible = FALSE;
    BattleCamera_ClearMonBg(2);
    gSprites[gBattlerSpriteIds[1]].invisible = FALSE;
    BattleCamera_ClearMonBg(1);
    memcpy(sOriginal, gSprites, sizeof(sOriginal));
    assert(BattleCamera_GetMonBgOffset(1, FALSE) == 0);
    FinishTestMove();

    // BG replacement suspends until completion, with no late focus/zoom pulse.
    StartTestMove(MOVE_TACKLE, 0, 1, TARGET_SELECTED, sGenericMoveScript);
    assert(sMoveCamera.generic && sCamera.targetZoom > 256 && sCamera.zoom == 256);
    BattleCamera_BlockMove();
    Tick(0);
    assert(sMoveCamera.suspended && !sCamera.active);
    for (i = 0; i < 10; i++) Tick(0);
    gAnimScriptActive = FALSE;
    Tick(0);
    assert(!sMoveCamera.active && !sCamera.active); // Never invent a shot after an incompatible effect.
    assert(sCamera.targetZoom == 256);
    for (i = 0; i < 40; i++) Tick(0);
    assert(!sMoveCamera.active && !sOwnsScanlines);
    // Suspension in the launch transaction retains the executed action context.
    gAnimScriptActive = TRUE;
    BattleCamera_StartMove(MOVE_TACKLE, 0, 1, TARGET_SELECTED, sGenericMoveScript);
    BattleCamera_BlockMove();
    Tick(0);
    assert(sMoveCamera.move == MOVE_TACKLE && sMoveCamera.suspended);
    BattleCamera_InterruptAnimation();
    BattleCamera_BlockMove();
    Tick(0);
    assert(!sMoveCamera.active); // Hard cancellation always dominates suspension.
    gAnimScriptActive = FALSE;

    // A new action supersedes deferred framing; a busy owner is never stopped.
    StartTestMove(MOVE_TACKLE, 0, 1, TARGET_SELECTED, sGenericMoveScript);
    BattleCamera_BlockMove(); Tick(0);
    gAnimScriptActive = FALSE;
    ScanlineEffect_SetParams((struct ScanlineEffectParams){(void *)REG_ADDR_BG3HOFS, SCANLINE_EFFECT_DMACNT_32BIT, 1, 0});
    for (i = 0; i < 64; i++) Tick(0);
    assert(!sMoveCamera.active && gScanlineEffect.state == 1);
    ScanlineEffect_Stop();
    Ready();
    // Whole-action ownership: visual end is not stat/status/HP completion.
    StartTestMove(MOVE_GROWL, 0, 1, TARGET_BOTH, gBattleAnimMove_Growl);
    for (i = 0; i < 40; i++) Tick(0);
    BattleCamera_EndMove();
    gAnimScriptActive = FALSE;
    for (i = 0; i < BATTLE_CAMERA_RETURN_TICKS + 8; i++) Tick(0);
    assert(sMoveCamera.active && sMoveCamera.phase != CAMERA_PHASE_RETURN);
    {
        struct BattleCamera held = sCamera;
        gBattleControllerExecFlags = 2;
        BattleCamera_OnHitReaction(1, TRUE);
        for (i = 0; i < 32; i++)
        {
            gSprites[gBattlerSpriteIds[1]].invisible = (i / 4) & 1;
            memcpy(sOriginal, gSprites, sizeof(sOriginal));
            Tick(0);
            assert(sMoveCamera.active && sCamera.x == held.x);
        }
        gSprites[gBattlerSpriteIds[1]].invisible = FALSE;
        BattleCamera_OnHitReaction(1, FALSE);
        memcpy(sOriginal, gSprites, sizeof(sOriginal));
        gBattleControllerExecFlags = 0;
        held.zoom = sCamera.zoom; // Contact has now acquired the stronger recipient zoom.
        BattleCamera_OnAnimationLaunch(ANIM_TYPE_GENERAL, B_ANIM_STATS_CHANGE, 1);
        gAnimScriptActive = TRUE;
        for (i = 0; i < 30; i++) Tick(0);
        assert(sMoveCamera.active && sCamera.x == held.x && sCamera.y == held.y && sCamera.zoom == held.zoom);
        BattleCamera_EndMove();
        gAnimScriptActive = FALSE;
        BattleCamera_OnAnimationLaunch(ANIM_TYPE_STATUS, B_ANIM_STATUS_PRZ, 1);
        gAnimScriptActive = TRUE;
        for (i = 0; i < 30; i++) Tick(0);
        assert(sMoveCamera.active && sCamera.x == held.x && sCamera.zoom == held.zoom);
        BattleCamera_EndMove();
        gAnimScriptActive = FALSE;
        gBattleControllerExecFlags = 2; // Target HP bar still being animated.
        BattleCamera_OnHealthbar(1);
        BattleCamera_FinishAction(); // Early callers cannot release a busy controller.
        assert(!sMoveCamera.actionComplete);
        for (i = 0; i < BATTLE_CAMERA_RETURN_TICKS + 20; i++) Tick(0);
        assert(sMoveCamera.active && sCamera.x == held.x && sMoveCamera.phase != CAMERA_PHASE_RETURN);
        gBattleControllerExecFlags = 0;
        BattleCamera_FinishAction();
        Tick(0);
        assert(sMoveCamera.phase == CAMERA_PHASE_RETURN && sMoveCamera.duration >= BATTLE_CAMERA_RETURN_TICKS);
        for (i = 0; i < BATTLE_CAMERA_RETURN_TICKS; i++) Tick(0);
        assert(!sMoveCamera.active && !sOwnsScanlines);
    }
    StartTestMove(MOVE_PECK, 0, 1, TARGET_SELECTED, gBattleAnimMove_Peck);
    BattleCamera_OnAnimationLaunch(ANIM_TYPE_SPECIAL, 0, 1);
    Tick(0);
    assert(!sMoveCamera.active); // Switching/other special ownership still cancels.
    gAnimScriptActive = FALSE;
    puts("PASS: action hold through stats/status/HP controllers, gated finish, slower smooth return");

    // Target-side composition, real double coordinates, and VFX-aware context.
    {
        struct Sprite saved[MAX_SPRITES + 1];
        s16 farX, farY, farZoom, leftZoom, rightZoom;
        memcpy(saved, gSprites, sizeof(saved));
        gBattlersCount = 2;
        for (i = 0; i < 2; i++)
        {
            gSprites[gBattlerSpriteIds[i]].x = i ? 176 : 72;
            gSprites[gBattlerSpriteIds[i]].y = i ? 40 : 80;
            gSprites[gBattlerSpriteIds[i]].x2 = gSprites[gBattlerSpriteIds[i]].y2 = 0;
        }
        memcpy(sOriginal, gSprites, sizeof(sOriginal));
        StartTestMove(MOVE_GROWL, 0, 1, TARGET_BOTH, gBattleAnimMove_Growl);
        farX = sCamera.targetX; farY = sCamera.targetY; farZoom = sCamera.targetZoom;
        assert(sMoveCamera.focusMask == 2);
        for (i = 0; i < 40; i++) Tick(0);
        assert(sMoveCamera.contextWeight[0] > 0 && sMoveCamera.contextWeight[1] == 0);
        assert(ActorScale(0, sCamera.zoom) < ActorScale(1, sCamera.zoom));
        StartTestMove(MOVE_PECK, 1, 0, TARGET_SELECTED, gBattleAnimMove_Peck);
        assert(sMoveCamera.contextWeight[0] > 0); // Inherit quiet context; fade into the next target.
        FinishTestMove();
        StartTestMove(MOVE_GROWL, 0, 1, TARGET_BOTH, gBattleAnimMove_Growl);
        for (i = 0; i < 40; i++) Tick(0);
        gSprites[9].inUse = TRUE;
        gSprites[9].invisible = FALSE;
        gSprites[9].oam.affineMode = ST_OAM_AFFINE_OFF;
        gSprites[9].x = 72; gSprites[9].y = 80;
        gSprites[9].x2 = gSprites[9].y2 = 0;
        BattleCamera_OnSpriteCreated(9, 0);
        sMoveCamera.focusMask = 1; // A previously quiet actor becomes the effect's focus.
        memcpy(sOriginal, gSprites, sizeof(sOriginal));
        Tick(0);
        assert(sMoveCamera.contextWeight[0] == 0); // Nearby VFX restore common scale immediately.
        FinishTestMove();
        StartTestMove(MOVE_GROWL, 1, 0, TARGET_BOTH, gBattleAnimMove_Growl);
        assert(sMoveCamera.focusMask == 1 && sCamera.targetY > 0);
        assert(sCamera.targetX != farX || sCamera.targetY != farY || sCamera.targetZoom != farZoom);
        FinishTestMove();

        gBattlersCount = 4;
        sDoubleCoords = TRUE;
        for (i = 0; i < 4; i++)
        {
            static const u8 x[] = {32,200,90,152}, y[] = {80,40,84,32};
            u8 id = gBattlerSpriteIds[i];
            gSprites[id].x = x[i]; gSprites[id].y = y[i];
            gSprites[id].x2 = gSprites[id].y2 = 0;
            gSprites[id].oam.affineMode = ST_OAM_AFFINE_OFF;
            gSprites[id].invisible = FALSE;
        }
        memcpy(sOriginal, gSprites, sizeof(sOriginal));
        StartTestMove(MOVE_GROWL, 1, 0, TARGET_BOTH, gBattleAnimMove_Growl);
        assert(sMoveCamera.focusMask == 5 && sCamera.targetY > 0);
        FinishTestMove();
        StartTestMove(MOVE_GROWL, 0, 1, TARGET_BOTH, gBattleAnimMove_Growl);
        assert(sMoveCamera.focusMask == 10 && sCamera.targetY < 0);
        FinishTestMove();
        StartTestMove(MOVE_PECK, 1, 0, TARGET_SELECTED, gBattleAnimMove_Peck);
        assert(sMoveCamera.focusMask == 1);
        leftZoom = sCamera.targetZoom;
        for (i = 0; i < 40; i++) Tick(0);
        assert(sMoveCamera.contextWeight[0] == 0 && sMoveCamera.contextWeight[2] > 0);
        FinishTestMove();
        StartTestMove(MOVE_PECK, 1, 2, TARGET_SELECTED, gBattleAnimMove_Peck);
        assert(sMoveCamera.focusMask == 4);
        rightZoom = sCamera.targetZoom;
        assert(rightZoom >= leftZoom);
        for (i = 0; i < 40; i++) Tick(0);
        assert(sMoveCamera.contextWeight[2] == 0 && sMoveCamera.contextWeight[0] > 0);
        FinishTestMove();
        sDoubleCoords = FALSE;
        memcpy(gSprites, saved, sizeof(saved));
        memcpy(sOriginal, saved, sizeof(saved));
        Ready();
    }
    puts("PASS: Growl side/group composition, doubled target masks, quiet context and VFX protection");
    puts("PASS: repeated executed moves, both sides, resolved self/ally/spread/field targets, projectile alignment, impacts, invalidation, interruption, timeout, neutral reset");
}
