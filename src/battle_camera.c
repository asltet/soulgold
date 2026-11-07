#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_anim_scripts.h"
#include "constants/battle_anim.h"
#include "battle_main.h"
#include "battle_camera.h"
#include "move.h"
#include "main.h"
#include "palette.h"
#include "scanline_effect.h"
#include "sprite.h"
#include "task.h"
#include "constants/battle.h"

// Art-space camera coordinates and zoom have eight fractional bits.
struct BattleCamera
{
    s16 x, y, zoom;
    s16 targetX, targetY, targetZoom;
    s16 startX, startY, startZoom;
    u16 frame;
    u8 track, leg;
    bool8 active;
};

struct CameraKey
{
    u16 duration;
    u16 x, y, zoom;
};

struct CameraTrack
{
    const struct CameraKey *keys;
    u8 count;
};

#include "data/battle_camera_tracks.h"

struct CameraSpriteBackup
{
    struct Sprite sprite;
    u8 id;
    u8 matrix;
};

static EWRAM_DATA struct BattleCamera sCamera = {0};
static EWRAM_DATA struct CameraSpriteBackup sBackups[BATTLE_CAMERA_MAX_SCENE_SPRITES] = {0};
static EWRAM_DATA u8 sBackupCount = 0;
static EWRAM_DATA u16 sIdleFrames = 0;
static EWRAM_DATA u16 sWaitTicks = 0;
static EWRAM_DATA bool8 sMenuReturning = FALSE;
static EWRAM_DATA u16 sMenuReturnTick = 0;
static EWRAM_DATA u16 sMoveBlend = 0;
static EWRAM_DATA u16 sHandoffStartBlend = 0;
static EWRAM_DATA u16 sHandoffTick = 0;
static EWRAM_DATA u8 sLastTrack = 0;
static EWRAM_DATA u32 sVisualRandom = 0;
static EWRAM_DATA bool8 sSceneReady = FALSE;
static EWRAM_DATA u16 sSceneDepthStrength = 0;
static EWRAM_DATA s16 sShakeX = 0, sShakeY = 0;
struct CameraRowKey
{
    s16 x, y, zoom;
    u16 blend, depth;
};
static EWRAM_DATA struct CameraRowKey sRowKey = {0};
static EWRAM_DATA bool8 sRowsValid = FALSE;
static EWRAM_DATA bool8 sOwnsScanlines = FALSE;
static EWRAM_DATA u32 sScanlineGeneration = 0;
// Include the transfer after the final visible row. Main-loop scratch only;
// the completed table is published into the engine's DMA buffer in VBlank.
static EWRAM_DATA u32 ALIGNED(4) sRows[DISPLAY_HEIGHT + 1] = {0};

enum MoveCameraProfileId
{
    MOVE_CAMERA_MELEE,
    MOVE_CAMERA_PHYSICAL,
    MOVE_CAMERA_SPECIAL,
    MOVE_CAMERA_SELF,
    MOVE_CAMERA_ALLY,
    MOVE_CAMERA_GROUP,
    MOVE_CAMERA_STATUS,
    MOVE_CAMERA_EMBER,
};

enum Bw2MoveFraming
{
    BW2_FRAME_AUTO,
    BW2_FRAME_TARGET,
    BW2_FRAME_USER,
    BW2_FRAME_TRAVEL,
    BW2_FRAME_GROUP,
    BW2_FRAME_STILL,
};
#include "data/battle_camera_bw2_framing.h"

enum MoveCameraTargetClass
{
    CAMERA_TARGET_NONE,
    CAMERA_TARGET_OPPONENT,
    CAMERA_TARGET_SELF,
    CAMERA_TARGET_ALLY,
    CAMERA_TARGET_GROUP,
    CAMERA_TARGET_FIELD,
};

struct MoveCameraProfile
{
    u8 start, travel, impact, restore;
    u16 pan, zoom;
    u8 easing;
    u8 shakeX, shakeY, shakeTicks;
};
struct MoveCameraOverride
{
    u16 move;
    u8 profile;
    const u8 *script;
};
struct MoveCameraState
{
    bool8 active, blocked, impactSeen;
    bool8 generic, suspended;
    bool8 visualSeen, targetAcquired;
    u8 framing, bgLayer[MAX_BATTLERS_COUNT];
    u16 move;
    u8 targetType;
    const u8 *script;
    u8 shakeTick, easing;
    u8 halfWidth[MAX_BATTLERS_COUNT], halfHeight[MAX_BATTLERS_COUNT];
    bool8 affineBounds;
    u8 attacker, target, targetMask, actorMask;
    u8 profile, targetClass, phase, battlerCount;
    u8 actorSprite[MAX_BATTLERS_COUNT];
    const struct SpriteFrameImage *actorImages[MAX_BATTLERS_COUNT];
    u32 effects[2];
    u16 age, elapsed, duration;
};

#include "data/battle_camera_move_profiles.h"
static EWRAM_DATA struct MoveCameraState sMoveCamera = {0};

enum MoveCameraEvent
{
    CAMERA_EVENT_NONE,
    CAMERA_EVENT_START,
    CAMERA_EVENT_END,
    CAMERA_EVENT_CANCEL,
    CAMERA_EVENT_SUSPEND,
    CAMERA_EVENT_START_SUSPENDED,
};
struct MoveCameraRequest
{
    u16 move;
    u8 event, attacker, target, targetType;
    const u8 *script;
};
static EWRAM_DATA struct MoveCameraRequest sMoveRequest = {0};

static void StartMoveNow(u16 move, u8 attacker, u8 target, u8 targetType, const u8 *script);
static void EndMoveNow(void);
static void SuspendMoveNow(void);
static bool32 UpdateMoveCamera(void);
static void SetMoveGoal(u8 focus, u16 duration, u16 zoom);

static bool32 HasScanlineLease(void)
{
    return sOwnsScanlines && sScanlineGeneration == gScanlineEffectGeneration
        && gScanlineEffect.state == 1
        && gScanlineEffect.dmaDest == (void *)REG_ADDR_BG3HOFS
        && gScanlineEffect.dmaControl == SCANLINE_EFFECT_DMACNT_32BIT
        && gScanlineEffect.waveTaskId == TASK_NONE;
}

void BattleCamera_Cancel(void)
{
    // Never stop another effect which has replaced our scanline owner.
    if (HasScanlineLease())
        ScanlineEffect_Stop();
    sOwnsScanlines = FALSE;
    sRowsValid = FALSE;
    sShakeX = sShakeY = 0;
    sMoveRequest.event = CAMERA_EVENT_NONE;
    memset(&sMoveCamera, 0, sizeof(sMoveCamera));
    memset(&sCamera, 0, sizeof(sCamera));
    sCamera.zoom = sCamera.targetZoom = 256;
    sIdleFrames = 0;
    sWaitTicks = BATTLE_CAMERA_WAIT_TICKS;
    sMenuReturning = FALSE;
    sMenuReturnTick = 0;
    sMoveBlend = 256;
    sHandoffStartBlend = 256;
    sHandoffTick = BATTLE_CAMERA_HANDOFF_TICKS;
}

void BattleCamera_Reset(void)
{
    BattleCamera_Cancel();
    sSceneReady = FALSE;
    sSceneDepthStrength = 0;
    sLastTrack = 0xFF;
    sVisualRandom = gMain.vblankCounter1 ^ 0x6D2B79F5;
}

void BattleCamera_SetEnvironment(u16 environment)
{
    BattleCamera_Cancel();
    // Called by the actual loader after environment overrides are resolved.
    sSceneReady = environment < BATTLE_ENVIRONMENT_COUNT;
    // Shared coverage does not imply that every illustration has Plain's depth.
    sSceneDepthStrength = environment == BATTLE_ENVIRONMENT_PLAIN ? 256 : BATTLE_CAMERA_OTHER_ENV_DEPTH;
}

// Monotonic Q12 quintic samples; linear interpolation avoids cancellation jitter.
static const u16 sSmootherWeights[] = {
    0, 1, 9, 29, 66, 122, 200, 300, 424, 570, 737, 924, 1127, 1345, 1573, 1809, 2048, 2287, 2523, 2751, 2969, 3172, 3359, 3526, 3672, 3796, 3896, 3974, 4030, 4067, 4087, 4095, 4096
};

static s32 EasingWeight(u16 frame, u16 duration, u8 easing)
{
    s32 t = min(frame, duration) * 256 / max(1, duration);
    s32 t2 = t * t / 256;
    s32 weight;
    switch (easing)
    {
    case BATTLE_CAMERA_EASING_IN_CUBIC:
        return t2 * t / 256;
    case BATTLE_CAMERA_EASING_OUT_CUBIC:
        t = 256 - t;
        return 256 - t * t / 256 * t / 256;
    case BATTLE_CAMERA_EASING_SMOOTHSTEP:
        return t * t * (768 - 2 * t) / 65536;
    case BATTLE_CAMERA_EASING_SMOOTHERSTEP:
        t = min(frame, duration) * 4096 / max(1, duration);
        if (t == 4096)
            return 256;
        weight = sSmootherWeights[t / 128];
        weight += (sSmootherWeights[t / 128 + 1] - weight) * (t % 128) / 128;
        return weight / 16;
    default:
        return t;
    }
}

static s16 Ease(s16 destination, u16 frame, u16 duration)
{
    return destination * EasingWeight(frame, duration, BATTLE_CAMERA_EASING) / 256;
}

// Private cosmetic PRNG: never consume battle/encounter RNG for camera shots.
static u8 PickTrack(void)
{
    u32 value = sVisualRandom ? sVisualRandom : 0x6D2B79F5;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    sVisualRandom = value;
    if (sLastTrack >= ARRAY_COUNT(sIdleTracks))
        return value % ARRAY_COUNT(sIdleTracks);
    return (sLastTrack + 1 + value % (ARRAY_COUNT(sIdleTracks) - 1)) % ARRAY_COUNT(sIdleTracks);
}

static void BeginLeg(void)
{
    const struct CameraKey *key = &sIdleTracks[sCamera.track].keys[sCamera.leg];
    u16 panLimit = gBattlersCount > 2 ? BATTLE_CAMERA_IDLE_DOUBLES_PAN_X : BATTLE_CAMERA_MAX_PAN_X;
    sCamera.startX = sCamera.x;
    sCamera.startY = sCamera.y;
    sCamera.startZoom = sCamera.zoom;
    sCamera.targetX = panLimit * key->x;
    sCamera.targetY = -BATTLE_CAMERA_IDLE_PAN_Y * key->y;
    sCamera.targetZoom = 256 + (BATTLE_CAMERA_MAX_ZOOM - 256) * key->zoom / 256;
    sCamera.frame = 0;
}

static bool32 AdvanceTrack(void)
{
    const struct CameraTrack *track = &sIdleTracks[sCamera.track];
    if (sCamera.frame == track->keys[sCamera.leg].duration)
    {
        if (++sCamera.leg == track->count)
        {
            BattleCamera_Cancel();
            // BW2 stays in PLAY after the initial wait: no fresh periodic delay.
            sWaitTicks = 0;
            return FALSE;
        }
        BeginLeg();
    }
    sCamera.frame++;
    sCamera.x = sCamera.startX + Ease(sCamera.targetX - sCamera.startX, sCamera.frame, track->keys[sCamera.leg].duration);
    sCamera.y = sCamera.startY + Ease(sCamera.targetY - sCamera.startY, sCamera.frame, track->keys[sCamera.leg].duration);
    sCamera.zoom = sCamera.startZoom + Ease(sCamera.targetZoom - sCamera.startZoom, sCamera.frame, track->keys[sCamera.leg].duration);
    return TRUE;
}

// Artwork-space depth, not a recovered BW2 world coordinate. The same anchors
// drive parallax, image scale, floor forward mapping and inverse BG sampling.
static s32 PlaneWeight(s32 sourceY, s32 far, s32 middle, s32 near)
{
    if (sourceY <= BATTLE_CAMERA_HORIZON)
        return far;
    if (sourceY < BATTLE_CAMERA_MIDGROUND_Y)
        return far + (middle - far) * (sourceY - BATTLE_CAMERA_HORIZON)
                   / (BATTLE_CAMERA_MIDGROUND_Y - BATTLE_CAMERA_HORIZON);
    if (sourceY < BATTLE_CAMERA_NEAR_CONTACT)
        return middle + (near - middle) * (sourceY - BATTLE_CAMERA_MIDGROUND_Y)
                      / (BATTLE_CAMERA_NEAR_CONTACT - BATTLE_CAMERA_MIDGROUND_Y);
    return near;
}

static s32 EnvironmentWeight(s32 weight)
{
    return 256 - (256 - weight) * sSceneDepthStrength / 256;
}

static s32 DepthWeight(s32 sourceY)
{
    s32 plane = PlaneWeight(sourceY, BATTLE_CAMERA_BACKGROUND_MOTION,
                           BATTLE_CAMERA_MIDGROUND_MOTION, BATTLE_CAMERA_FOREGROUND_MOTION);
    return EnvironmentWeight(256 - BATTLE_CAMERA_PARALLAX + plane * BATTLE_CAMERA_PARALLAX / 256);
}

static s32 MoveBlend(s32 idle, s32 move)
{
    return sMoveCamera.active ? idle + (move - idle) * sMoveBlend / 256 : idle;
}

static s32 RenderX(void)
{
    return max(0, min(BATTLE_CAMERA_MAX_PAN_X * 256, sCamera.x + sShakeX));
}

static s32 RenderY(void)
{
    return max(-BATTLE_CAMERA_MAX_PAN_Y * 256, min(0, sCamera.y + sShakeY));
}

static s32 PanAt(s32 sourceY)
{
    return RenderX() * MoveBlend(DepthWeight(sourceY), 256) / 65536;
}

static s32 SceneScale(s32 sourceY, s32 response)
{
    s32 weight = PlaneWeight(sourceY, BATTLE_CAMERA_BACKGROUND_ZOOM,
                            BATTLE_CAMERA_MIDGROUND_ZOOM, 256);
    // PERSPECTIVE=0 preserves equal image zoom; 256 uses full plane separation.
    weight = EnvironmentWeight(256 - BATTLE_CAMERA_PERSPECTIVE + weight * BATTLE_CAMERA_PERSPECTIVE / 256);
    return MoveBlend(256 + (sCamera.zoom - 256) * weight * response / 65536, sCamera.zoom);
}

static s32 FloorSlope(s32 weight)
{
    return MoveBlend(256 + (sCamera.zoom - 256) * EnvironmentWeight(weight) / 256, sCamera.zoom);
}

static s32 HorizonQ8(void)
{
    s32 translation = -RenderY() * sCamera.zoom / 256;
    return BATTLE_CAMERA_HORIZON * 256 + MoveBlend(translation * DepthWeight(BATTLE_CAMERA_HORIZON) / 256, translation);
}

static s32 GroundY(s32 sourceY)
{
    s32 result = HorizonQ8();
    if (sourceY < BATTLE_CAMERA_HORIZON)
        result += (sourceY - BATTLE_CAMERA_HORIZON) * FloorSlope(BATTLE_CAMERA_BACKGROUND_ZOOM);
    else if (sourceY < BATTLE_CAMERA_MIDGROUND_Y)
        result += (sourceY - BATTLE_CAMERA_HORIZON) * FloorSlope(BATTLE_CAMERA_MIDGROUND_ZOOM);
    else
        result += (BATTLE_CAMERA_MIDGROUND_Y - BATTLE_CAMERA_HORIZON) * FloorSlope(BATTLE_CAMERA_MIDGROUND_ZOOM)
                + (sourceY - BATTLE_CAMERA_MIDGROUND_Y) * FloorSlope(256);
    return result / 256;
}

static bool32 BuildRows(void)
{
    u32 row;
    struct CameraRowKey key = {RenderX(), RenderY(), sCamera.zoom,
                              sMoveCamera.active ? sMoveBlend : 0, sSceneDepthStrength};
    s32 horizon, middle, inverseFar, inverseMiddle, inverseNear;
    if (sRowsValid && key.x == sRowKey.x && key.y == sRowKey.y && key.zoom == sRowKey.zoom
        && key.blend == sRowKey.blend && key.depth == sRowKey.depth)
        return TRUE;
    sRowsValid = FALSE;
    horizon = HorizonQ8();
    middle = horizon + (BATTLE_CAMERA_MIDGROUND_Y - BATTLE_CAMERA_HORIZON) * FloorSlope(BATTLE_CAMERA_MIDGROUND_ZOOM);
    // Three reciprocals per table, no per-row perspective division. Q16
    // reciprocal products are preshifted to stay within signed 32-bit range.
    inverseFar = 16777216 / FloorSlope(BATTLE_CAMERA_BACKGROUND_ZOOM);
    inverseMiddle = 16777216 / FloorSlope(BATTLE_CAMERA_MIDGROUND_ZOOM);
    inverseNear = 16777216 / FloorSlope(256);
    for (row = 0; row <= DISPLAY_HEIGHT; row++)
    {
        s32 screen = row * 256;
        s32 origin = screen < middle ? horizon : middle;
        s32 anchor = screen < middle ? BATTLE_CAMERA_HORIZON : BATTLE_CAMERA_MIDGROUND_Y;
        s32 inverse = screen < horizon ? inverseFar : screen < middle ? inverseMiddle : inverseNear;
        s32 sourceY = anchor + ((screen - origin) / 16) * inverse / 1048576;
        s32 sourceX = PanAt(sourceY);
        // Reject unwrapped samples before encoding hardware register bits.
        if (sourceY < BATTLE_CAMERA_SOURCE_MIN_Y || sourceY > BATTLE_CAMERA_SOURCE_MAX_Y
            || sourceX < 0 || sourceX + DISPLAY_WIDTH > 256)
            return FALSE;
        sRows[row] = (sourceX & 0x1FF) | ((u32)((sourceY - row) & 0x1FF) << 16);
    }
    sRowKey = key;
    sRowsValid = TRUE;
    return TRUE;
}

static bool32 AcquireScanlines(void)
{
    struct ScanlineEffectParams params =
    {
        .dmaDest = (void *)REG_ADDR_BG3HOFS,
        .dmaControl = SCANLINE_EFFECT_DMACNT_32BIT,
        .initState = 1,
    };
    if (gScanlineEffect.state != 0 || gScanlineEffect.waveTaskId != TASK_NONE)
        return FALSE;
    // Both buffers start complete, including a possible VBlank during setup.
    memcpy(gScanlineEffectRegBuffers[0], sRows, sizeof(sRows));
    memcpy(gScanlineEffectRegBuffers[1], sRows, sizeof(sRows));
    ScanlineEffect_SetParams(params);
    sScanlineGeneration = gScanlineEffectGeneration;
    sOwnsScanlines = TRUE;
    return TRUE;
}

static bool32 TransformSprite(u8 id, s16 anchorX, s16 anchorY, s16 scale)
{
    struct Sprite *sprite;
    struct CameraSpriteBackup *backup;
    u8 matrix;
    u32 i;

    if (id >= MAX_SPRITES)
        return TRUE;
    for (i = 0; i < sBackupCount; i++)
        if (sBackups[i].id == id)
            return TRUE;
    sprite = &gSprites[id];
    if (!sprite->inUse || sprite->invisible || sprite->oam.affineMode == ST_OAM_AFFINE_ERASE)
        return TRUE;
    if (sprite->x + sprite->x2 < -512 || sprite->x + sprite->x2 > 1024
        || sprite->y + sprite->y2 < -512 || sprite->y + sprite->y2 > 1024)
        return FALSE; // Bound all projection products before applying fixed-point math.
    if (sprite->subspriteMode != SUBSPRITES_OFF || sBackupCount >= ARRAY_COUNT(sBackups))
        return FALSE;
    backup = &sBackups[sBackupCount++];
    backup->sprite = *sprite;
    backup->id = id;
    backup->matrix = 0xFF;

    // Image, split shadows, and floor use the same ground-contact transform.
    sprite->x = anchorX - PanAt(anchorY)
              + (sprite->x + sprite->x2 - anchorX) * scale / 256 - sprite->x2;
    {
        s32 originalY = sprite->y + sprite->y2;
        s32 projectedY;
        if (sMoveCamera.active && sMoveBlend == 256)
            projectedY = GroundY(originalY);
        else
        {
            projectedY = GroundY(anchorY) + (originalY - anchorY) * scale / 256;
            if (sMoveCamera.active)
                projectedY += (GroundY(originalY) - projectedY) * sMoveBlend / 256;
        }
        sprite->y = projectedY - sprite->y2;
    }
    if (scale == 256)
        return TRUE;
    matrix = AllocOamMatrix();
    if (matrix == 0xFF)
        return FALSE; // Roll back the entire shot, including the floor zoom.
    backup->matrix = matrix;
    if (sprite->oam.affineMode & ST_OAM_AFFINE_ON_MASK)
    {
        struct OamMatrix original = gOamMatrices[sprite->oam.matrixNum];
        SetOamMatrix(matrix, original.a * 256 / scale, original.b * 256 / scale,
                     original.c * 256 / scale, original.d * 256 / scale);
    }
    else
    {
        s16 inverse = 65536 / scale;
        SetOamMatrix(matrix, (sprite->oam.matrixNum & 8) ? -inverse : inverse, 0, 0,
                     (sprite->oam.matrixNum & 16) ? -inverse : inverse);
    }
    sprite->oam.affineMode = ST_OAM_AFFINE_DOUBLE;
    sprite->oam.matrixNum = matrix;
    CalcCenterToCornerVec(sprite, sprite->oam.shape, sprite->oam.size, sprite->oam.affineMode);
    return TRUE;
}

void BattleCamera_BeginFrame(void)
{
    u32 battler;
    u8 event = sMoveRequest.event;

    // VM/controller events publish only inside the existing OAM transaction.
    sMoveRequest.event = CAMERA_EVENT_NONE;
    if (event == CAMERA_EVENT_START || event == CAMERA_EVENT_START_SUSPENDED)
    {
        StartMoveNow(sMoveRequest.move, sMoveRequest.attacker, sMoveRequest.target,
                     sMoveRequest.targetType, sMoveRequest.script);
        if (event == CAMERA_EVENT_START_SUSPENDED)
            SuspendMoveNow();
    }
    else if (event == CAMERA_EVENT_CANCEL)
        BattleCamera_Cancel();
    else if (event == CAMERA_EVENT_SUSPEND)
        SuspendMoveNow();
    else if (event == CAMERA_EVENT_END)
        EndMoveNow();

    if (sMoveCamera.active)
    {
        if (!UpdateMoveCamera())
            return;
        goto compose;
    }

    if (!BATTLE_CAMERA_DEMO || !sSceneReady || !gMain.inBattle || !gBattleSpritesDataPtr
        || (gBattlersCount < 2 || gBattlersCount > MAX_BATTLERS_COUNT) || gPaletteFade.active
        || gAnimScriptActive || gBattle_BG3_X || gBattle_BG3_Y
        || gSpriteCoordOffsetX || gSpriteCoordOffsetY)
    {
        BattleCamera_Cancel();
        return;
    }
    if (sCamera.active && !HasScanlineLease())
    {
        BattleCamera_Cancel();
        return;
    }
    if (sMenuReturning || (!InBattleChoosingMoves() && sCamera.active))
    {
        if (!sMenuReturning)
        {
            sMenuReturning = TRUE;
            sMenuReturnTick = 0;
            sCamera.startX = sCamera.x;
            sCamera.startY = sCamera.y;
            sCamera.startZoom = sCamera.zoom;
        }
        sMenuReturnTick++;
        {
            s32 weight = EasingWeight(sMenuReturnTick, BATTLE_CAMERA_HANDOFF_TICKS, BATTLE_CAMERA_RETURN_EASING);
            sCamera.x = sCamera.startX - sCamera.startX * weight / 256;
            sCamera.y = sCamera.startY - sCamera.startY * weight / 256;
            sCamera.zoom = sCamera.startZoom + (256 - sCamera.startZoom) * weight / 256;
        }
        if (sMenuReturnTick == BATTLE_CAMERA_HANDOFF_TICKS)
        {
            BattleCamera_Cancel();
            return;
        }
        goto compose;
    }
    if (!InBattleChoosingMoves())
    {
        BattleCamera_Cancel();
        return;
    }
    if (!sCamera.active)
    {
        // Match BW2's threshold comparison: N waits, then try on update N+1.
        if (sIdleFrames < sWaitTicks)
        {
            sIdleFrames++;
            return;
        }
        // Keep READY while another effect owns the renderer. Do not re-wait.
        if (gScanlineEffect.state != 0 || gScanlineEffect.waveTaskId != TASK_NONE)
            return;
        if (!BuildRows() || !AcquireScanlines())
            return;
        sCamera.track = PickTrack();
        sLastTrack = sCamera.track;
        sCamera.leg = 0;
        sCamera.active = TRUE;
        BeginLeg();
    }
    if (!AdvanceTrack())
        return;

compose:
    if (!BuildRows())
        goto cancel;
    for (battler = 0; battler < gBattlersCount; battler++)
    {
        u8 id = gBattlerSpriteIds[battler];
        s16 anchorX, anchorY, scale;
        if (id >= MAX_SPRITES || !gSprites[id].inUse)
            continue;
        anchorX = gSprites[id].x + gSprites[id].x2;
        // Engine nominal ground baseline excludes species padding/elevation;
        // their existing offsets remain in the image relative to this anchor.
        anchorY = GetBattlerSpriteCoord(battler, BATTLER_COORD_Y) + MON_PIC_HEIGHT / 2;
        if (GetBattlerSide(battler) == B_SIDE_PLAYER)
            anchorY += 8; // Matches GetBattlerSpriteFinal_Y's player adjustment.
        scale = SceneScale(anchorY, BATTLE_CAMERA_BATTLER_DEPTH_SCALE);
        if (!TransformSprite(id, anchorX, anchorY, scale))
            goto cancel;
        if (GetBattlerSide(battler) == B_SIDE_OPPONENT)
        {
            s16 shadowScale = SceneScale(anchorY, BATTLE_CAMERA_SHADOW_DEPTH_SCALE);
            if (!TransformSprite(gBattleSpritesDataPtr->healthBoxesData[battler].shadowSpriteIdPrimary, anchorX, anchorY, shadowScale))
                goto cancel;
            if (B_ENEMY_MON_SHADOW_STYLE >= GEN_4 && P_GBA_STYLE_SPECIES_GFX == FALSE
                && !TransformSprite(gBattleSpritesDataPtr->healthBoxesData[battler].shadowSpriteIdSecondary, anchorX, anchorY, shadowScale))
                goto cancel;
        }
    }
    if (sMoveCamera.active)
    {
        u32 id;
        for (id = 0; id < MAX_SPRITES; id++)
            if ((sMoveCamera.effects[id / 32] & (1u << (id % 32))) && gSprites[id].inUse)
                if (!TransformSprite(id, gSprites[id].x + gSprites[id].x2,
                                      gSprites[id].y + gSprites[id].y2, sCamera.zoom))
                    goto cancel;
    }

    return;

cancel:
    BattleCamera_EndFrame();
    BattleCamera_Cancel();
}

void BattleCamera_EndFrame(void)
{
    while (sBackupCount)
    {
        struct CameraSpriteBackup *backup = &sBackups[--sBackupCount];
        gSprites[backup->id] = backup->sprite;
        if (backup->matrix != 0xFF)
            FreeOamMatrix(backup->matrix);
    }
}

void BattleCamera_PublishScanlines(void)
{
    if (!HasScanlineLease())
        return;
    if (!gMain.oamLoadDisabled)
        memcpy(gScanlineEffectRegBuffers[gScanlineEffect.srcBuffer], sRows, sizeof(sRows));
    else
        // Reuse the previous complete table if VBlank interrupted construction.
        // The existing InitHBlankDmaTransfer then toggles back to the next slot.
        gScanlineEffect.srcBuffer ^= 1;
}

// Match row0 in the buffered GPU register writes as well as the DMA first-row
// copy: the outer VBlank GPU flush may occur after the battle callback.
s16 BattleCamera_GetBgX(void) { return HasScanlineLease() ? sRows[0] & 0x1FF : 0; }
s16 BattleCamera_GetBgY(void) { return HasScanlineLease() ? (sRows[0] >> 16) & 0x1FF : 0; }

// Actual executed targets enter here, before any animation-only target rewrite.
static bool32 VisibleMoveBattler(u8 battler)
{
    u8 sprite;
    if (battler >= gBattlersCount || (gAbsentBattlerFlags & (1u << battler)) || !gBattleMons[battler].hp)
        return FALSE;
    sprite = gBattlerSpriteIds[battler];
    return sprite < MAX_SPRITES && gSprites[sprite].inUse
        && (!gSprites[sprite].invisible || (sMoveCamera.active && sMoveCamera.bgLayer[battler]))
        && gSprites[sprite].oam.affineMode != ST_OAM_AFFINE_ERASE;
}

static void ResolveMoveTargets(u8 type)
{
    u32 i;
    u8 attacker = sMoveCamera.attacker;
    u8 target = sMoveCamera.target;
    sMoveCamera.targetClass = CAMERA_TARGET_NONE;
    switch (type)
    {
    case TARGET_NONE:
    case TARGET_FIELD:
    case TARGET_OPPONENTS_FIELD:
        sMoveCamera.targetClass = CAMERA_TARGET_FIELD;
        return;
    case TARGET_BOTH:
    case TARGET_FOES_AND_ALLY:
    case TARGET_ALL_BATTLERS:
    case TARGET_USER_AND_ALLY:
        sMoveCamera.targetClass = CAMERA_TARGET_GROUP;
        for (i = 0; i < gBattlersCount; i++)
            if (VisibleMoveBattler(i)
                && (type == TARGET_ALL_BATTLERS
                    || (type == TARGET_BOTH && GetBattlerSide(i) != GetBattlerSide(attacker))
                    || (type == TARGET_FOES_AND_ALLY && i != attacker)
                    || (type == TARGET_USER_AND_ALLY && GetBattlerSide(i) == GetBattlerSide(attacker))))
                sMoveCamera.targetMask |= 1u << i;
        return;
    case TARGET_USER:
        target = attacker;
        sMoveCamera.target = attacker;
        break;
    default:
        break;
    }
    if (!VisibleMoveBattler(target))
        return;
    if ((type == TARGET_ALLY || type == TARGET_USER_OR_ALLY)
        && GetBattlerSide(target) != GetBattlerSide(attacker))
        return;
    sMoveCamera.targetMask = 1u << target;
    sMoveCamera.targetClass = target == attacker ? CAMERA_TARGET_SELF
                           : GetBattlerSide(target) == GetBattlerSide(attacker) ? CAMERA_TARGET_ALLY
                           : CAMERA_TARGET_OPPONENT;
}

static bool32 HasUncertainBounds(const struct Sprite *sprite)
{
    const struct OamMatrix *matrix;
    if (!(sprite->oam.affineMode & ST_OAM_AFFINE_ON_MASK))
        return FALSE;
    matrix = &gOamMatrices[sprite->oam.matrixNum];
    // Ordinary battlers start affine with an identity (possibly flipped) matrix.
    return matrix->b != 0 || matrix->c != 0
        || (matrix->a != 256 && matrix->a != -256)
        || (matrix->d != 256 && matrix->d != -256);
}

static void StartMoveNow(u16 move, u8 attacker, u8 target, u8 targetType, const u8 *script)
{
    u32 i;
    s16 oldX = sCamera.x, oldY = sCamera.y, oldZoom = sCamera.zoom;
    u16 oldBlend = sMoveCamera.active ? sMoveBlend : 0;
    bool32 inherit = sCamera.active && HasScanlineLease()
                  && (oldX != 0 || oldY != 0 || oldZoom != 256);
    BattleCamera_Cancel();
    if (!BATTLE_CAMERA_DEMO || !sSceneReady || !gMain.inBattle || !gBattleSpritesDataPtr
        || !gBattleSpritesDataPtr->battlerData || !gBattleSpritesDataPtr->healthBoxesData
        || gBattlersCount < 2 || gBattlersCount > MAX_BATTLERS_COUNT || !VisibleMoveBattler(attacker))
        return;
    sMoveCamera.active = TRUE;
    sMoveCamera.blocked = TRUE;
    sMoveCamera.battlerCount = gBattlersCount;
    sMoveCamera.attacker = attacker;
    sMoveCamera.target = target;
    sMoveCamera.phase = CAMERA_PHASE_MOVE_START;
    sMoveCamera.move = move;
    sMoveCamera.targetType = targetType;
    sMoveCamera.script = script;
    if (move == MOVE_NONE || move >= MOVES_COUNT_ALL || script == NULL)
        return;
    ResolveMoveTargets(targetType);
    if (sMoveCamera.targetClass == CAMERA_TARGET_NONE)
        return;
    sMoveCamera.generic = TRUE;
    sMoveCamera.blocked = FALSE;
    sMoveCamera.profile = gMovesInfo[move].category == DAMAGE_CATEGORY_SPECIAL ? MOVE_CAMERA_SPECIAL
                        : gMovesInfo[move].category == DAMAGE_CATEGORY_STATUS ? MOVE_CAMERA_STATUS
                        : gMovesInfo[move].makesContact ? MOVE_CAMERA_MELEE : MOVE_CAMERA_PHYSICAL;
    for (i = 0; i < ARRAY_COUNT(sMoveOverrides); i++)
        if (sMoveOverrides[i].move == move && sMoveOverrides[i].script == script)
        {
            sMoveCamera.profile = sMoveOverrides[i].profile;
            sMoveCamera.generic = FALSE;
            break;
        }
    // Noncanonical weather/charge variants stay neutral unless explicitly adapted.
    if (sMoveCamera.generic && script != gMovesInfo[move].battleAnimScript)
    {
        sMoveCamera.blocked = TRUE;
        sMoveCamera.suspended = TRUE;
    }
    if (sMoveCamera.targetClass == CAMERA_TARGET_SELF)
        sMoveCamera.profile = MOVE_CAMERA_SELF;
    else if (sMoveCamera.targetClass == CAMERA_TARGET_ALLY)
        sMoveCamera.profile = MOVE_CAMERA_ALLY;
    else if (sMoveCamera.targetClass == CAMERA_TARGET_GROUP || sMoveCamera.targetClass == CAMERA_TARGET_FIELD)
        sMoveCamera.profile = MOVE_CAMERA_GROUP;
    sMoveCamera.framing = sBw2MoveFraming[move];
    if (sMoveCamera.framing == BW2_FRAME_AUTO)
        sMoveCamera.framing = sMoveCamera.profile == MOVE_CAMERA_MELEE || sMoveCamera.profile == MOVE_CAMERA_SPECIAL
                           ? BW2_FRAME_TRAVEL : BW2_FRAME_TARGET;
    if (move == MOVE_HEADBUTT)
        sMoveCamera.framing = BW2_FRAME_TRAVEL; // First actual wind-up barrier releases target acquisition.
    if (sMoveCamera.targetClass == CAMERA_TARGET_SELF)
        sMoveCamera.framing = BW2_FRAME_USER;
    else if (sMoveCamera.targetClass == CAMERA_TARGET_ALLY)
        sMoveCamera.framing = BW2_FRAME_TARGET;
    else if (sMoveCamera.profile == MOVE_CAMERA_GROUP)
        sMoveCamera.framing = BW2_FRAME_GROUP;
    if (sMoveCamera.framing == BW2_FRAME_GROUP)
        sMoveCamera.profile = MOVE_CAMERA_GROUP;
    if (sMoveCamera.framing == BW2_FRAME_STILL)
        sMoveCamera.blocked = TRUE;
    for (i = 0; i < gBattlersCount; i++)
    {
        u8 id = gBattlerSpriteIds[i];
        sMoveCamera.actorSprite[i] = id;
        if (id < MAX_SPRITES)
            sMoveCamera.actorImages[i] = gSprites[id].images;
        if (VisibleMoveBattler(i))
        {
            sMoveCamera.actorMask |= 1u << i;
            if (gSprites[id].oam.shape >= 3)
                sMoveCamera.blocked = TRUE;
            else
            {
                sMoveCamera.halfWidth[i] = gOamDimensions[gSprites[id].oam.shape][gSprites[id].oam.size].width / 2;
                sMoveCamera.halfHeight[i] = gOamDimensions[gSprites[id].oam.shape][gSprites[id].oam.size].height / 2;
                if (HasUncertainBounds(&gSprites[id]))
                    sMoveCamera.affineBounds = TRUE;
            }
            if (gBattleSpritesDataPtr->battlerData[i].behindSubstitute)
                sMoveCamera.blocked = TRUE;
        }
    }
    if (sMoveCamera.blocked)
        return;
    if (inherit && !sMoveCamera.blocked)
    {
        sCamera.x = oldX;
        sCamera.y = oldY;
        sCamera.zoom = oldZoom;
        sMoveBlend = sHandoffStartBlend = oldBlend;
        sHandoffTick = 0;
    }
    SetMoveGoal((sMoveCamera.profile == MOVE_CAMERA_EMBER
                 || sMoveCamera.framing == BW2_FRAME_USER || sMoveCamera.framing == BW2_FRAME_TRAVEL)
                ? attacker : sMoveCamera.target, sMoveProfiles[sMoveCamera.profile].start,
                sMoveCamera.generic ? 0 : sMoveProfiles[sMoveCamera.profile].zoom);
}

static void ClampMoveFraming(void)
{
    u32 i;
    s32 zoom;
    s32 margin = BATTLE_CAMERA_FRAMING_MARGIN;
    // Rotated/scaled image extents are unknown: do not add camera zoom.
    if (sMoveCamera.affineBounds)
        sCamera.targetZoom = 256;
    for (i = 0; i < gBattlersCount; i++)
        if (sMoveCamera.actorMask & (1u << i))
        {
            const struct Sprite *sprite = &gSprites[sMoveCamera.actorSprite[i]];
            s32 room;
            if (sprite->x + sprite->x2 < -512 || sprite->x + sprite->x2 > 1024
                || sprite->y + sprite->y2 < -512 || sprite->y + sprite->y2 > 1024)
            {
                sCamera.targetZoom = 256;
                sCamera.targetX = 0;
                return; // Main projection will reject the malformed coordinates.
            }
            if (HasUncertainBounds(sprite))
                sCamera.targetZoom = 256;
            room = max(0, sprite->x + sprite->x2 - sMoveCamera.halfWidth[i] - margin);
            sCamera.targetX = min(sCamera.targetX, room * 256);
        }
    // At most33 candidates *4 actors, only when a goal changes, not per frame.
    for (zoom = sCamera.targetZoom; zoom > 256; zoom--)
    {
        bool32 fits = TRUE;
        for (i = 0; i < gBattlersCount; i++)
            if (sMoveCamera.actorMask & (1u << i))
            {
                const struct Sprite *sprite = &gSprites[sMoveCamera.actorSprite[i]];
                s32 x = sprite->x + sprite->x2 - sCamera.targetX / 256;
                s32 y = BATTLE_CAMERA_HORIZON
                      + ((sprite->y + sprite->y2 - BATTLE_CAMERA_HORIZON) * 256 - sCamera.targetY) * zoom / 65536;
                s32 halfWidth = (sMoveCamera.halfWidth[i] * zoom + 255) / 256;
                s32 halfHeight = (sMoveCamera.halfHeight[i] * zoom + 255) / 256;
                if (x - halfWidth < margin || x + halfWidth >= DISPLAY_WIDTH - margin
                    || y - halfHeight < margin || y + halfHeight >= DISPLAY_HEIGHT - margin)
                    fits = FALSE;
            }
        if (fits)
            break;
    }
    sCamera.targetZoom = zoom;
}

static void SetMoveGoal(u8 focus, u16 duration, u16 zoom)
{
    const struct MoveCameraProfile *profile = &sMoveProfiles[sMoveCamera.profile];
    sCamera.startX = sCamera.x;
    sCamera.startY = sCamera.y;
    sCamera.startZoom = sCamera.zoom;
    sCamera.targetX = sCamera.targetY = 0;
    sCamera.targetZoom = 256;
    if (sMoveCamera.profile == MOVE_CAMERA_GROUP && sMoveCamera.phase != CAMERA_PHASE_RETURN)
    {
        // A small common pan keeps all targets together; no extra zoom.
        sCamera.targetX = BATTLE_CAMERA_FIELD_PAN_X * 256;
        sCamera.targetY = -256;
        ClampMoveFraming();
    }
    else if (focus < gBattlersCount && VisibleMoveBattler(focus))
    {
        s32 x = gSprites[gBattlerSpriteIds[focus]].x;
        s32 y = GetBattlerSpriteCoord(focus, BATTLER_COORD_Y);
        sCamera.targetX = max(0, min(BATTLE_CAMERA_MOVE_PAN_X * 256,
                            (x - BATTLE_CAMERA_FOCUS_LEFT_X) * BATTLE_CAMERA_MOVE_PAN_X * 256 / BATTLE_CAMERA_FOCUS_SPAN_X)) * min(profile->pan, 256) / 256;
        sCamera.targetY = -max(0, min(BATTLE_CAMERA_MAX_PAN_Y * 256,
                            (BATTLE_CAMERA_NEAR_CONTACT - y) * BATTLE_CAMERA_MAX_PAN_Y * 256
                            / (BATTLE_CAMERA_NEAR_CONTACT - BATTLE_CAMERA_HORIZON)));
        sCamera.targetZoom = 256 + min(zoom, BATTLE_CAMERA_MAX_ZOOM - 256);
        ClampMoveFraming();
    }
    sMoveCamera.easing = sMoveCamera.phase == CAMERA_PHASE_RETURN ? BATTLE_CAMERA_RETURN_EASING
                       : sHandoffTick < BATTLE_CAMERA_HANDOFF_TICKS ? BATTLE_CAMERA_EASING_SMOOTHSTEP : profile->easing;
    sMoveCamera.elapsed = 0;
    sMoveCamera.duration = max(max(1, duration), BATTLE_CAMERA_HANDOFF_TICKS - sHandoffTick);
}

static void EndMoveNow(void)
{
    if (!sMoveCamera.active)
        return;
    if (sMoveCamera.blocked)
    {
        BattleCamera_Cancel();
        return;
    }
    if (sMoveCamera.phase != CAMERA_PHASE_RETURN)
    {
        sMoveCamera.phase = CAMERA_PHASE_RETURN;
        SetMoveGoal(MAX_BATTLERS_COUNT, sMoveProfiles[sMoveCamera.profile].restore, 0);
    }
}

void BattleCamera_StartMove(u16 move, u8 attacker, u8 target, u8 targetType, const u8 *script)
{
    sMoveRequest.move = move;
    sMoveRequest.attacker = attacker;
    sMoveRequest.target = target;
    sMoveRequest.targetType = targetType;
    sMoveRequest.script = script;
    sMoveRequest.event = CAMERA_EVENT_START;
}

void BattleCamera_InterruptAnimation(void)
{
    sMoveRequest.event = CAMERA_EVENT_CANCEL;
}

void BattleCamera_EndMove(void)
{
    if (sMoveRequest.event == CAMERA_EVENT_CANCEL || sMoveRequest.event == CAMERA_EVENT_SUSPEND)
        return;
    // A zero-update animation needs neutral cleanup, not an artificial shot.
    sMoveRequest.event = sMoveRequest.event == CAMERA_EVENT_START || sMoveRequest.event == CAMERA_EVENT_START_SUSPENDED
                       ? CAMERA_EVENT_CANCEL : CAMERA_EVENT_END;
}

void BattleCamera_BlockMove(void)
{
    // No partial cancellation between VM commands and the OAM snapshot boundary.
    if (sMoveRequest.event == CAMERA_EVENT_CANCEL)
        return;
    if (sMoveRequest.event == CAMERA_EVENT_START || sMoveRequest.event == CAMERA_EVENT_START_SUSPENDED)
        sMoveRequest.event = CAMERA_EVENT_START_SUSPENDED;
    else
        sMoveRequest.event = CAMERA_EVENT_SUSPEND;
}

static void SuspendMoveNow(void)
{
    if (!sMoveCamera.active)
        return;
    if (HasScanlineLease())
        ScanlineEffect_Stop();
    sOwnsScanlines = FALSE;
    sRowsValid = FALSE;
    memset(&sCamera, 0, sizeof(sCamera));
    sCamera.zoom = sCamera.targetZoom = 256;
    sShakeX = sShakeY = 0;
    sMoveCamera.blocked = sMoveCamera.suspended = TRUE;
    sMoveCamera.effects[0] = sMoveCamera.effects[1] = 0;
}

void BattleCamera_OnSpriteCreated(u8 id, u16 tileTag)
{
    u32 i;
    if (id >= MAX_SPRITES)
        return;
    sMoveCamera.effects[id / 32] &= ~(1u << (id % 32));
    if (!sMoveCamera.active)
        return;
    for (i = 0; i < MAX_BATTLERS_COUNT; i++)
        if ((sMoveCamera.actorMask & (1u << i)) && sMoveCamera.actorSprite[i] == id)
        {
            BattleCamera_InterruptAnimation(); // Battler replacement invalidates the snapshot binding.
            return;
        }
    if (sMoveCamera.blocked || !gAnimScriptActive)
        return;
    sMoveCamera.effects[id / 32] |= 1u << (id % 32);
    sMoveCamera.visualSeen = TRUE;
    if (tileTag == ANIM_TAG_IMPACT && !sMoveCamera.impactSeen && sMoveCamera.phase != CAMERA_PHASE_RETURN)
    {
        sMoveCamera.impactSeen = TRUE;
        if (sMoveCamera.profile == MOVE_CAMERA_EMBER)
        {
            sMoveCamera.shakeTick = 0;
            sMoveCamera.phase = CAMERA_PHASE_IMPACT;
            SetMoveGoal(sMoveCamera.target, sMoveProfiles[sMoveCamera.profile].impact,
                        sMoveProfiles[sMoveCamera.profile].zoom);
        }
        else
            BattleCamera_OnVisualBarrier();
    }
}

void BattleCamera_OnVisualBarrier(void)
{
    if (!sMoveCamera.active || sMoveCamera.blocked || !gAnimScriptActive
        || sMoveCamera.profile == MOVE_CAMERA_EMBER || !sMoveCamera.visualSeen
        || sMoveCamera.framing != BW2_FRAME_TRAVEL || sMoveCamera.targetAcquired
        || sMoveCamera.phase == CAMERA_PHASE_RETURN)
        return;
    sMoveCamera.targetAcquired = TRUE;
    sMoveCamera.phase = CAMERA_PHASE_TARGET_FOCUS;
    SetMoveGoal(sMoveCamera.target, sMoveProfiles[sMoveCamera.profile].travel,
                sMoveCamera.generic ? 0 : sMoveProfiles[sMoveCamera.profile].zoom);
}

void BattleCamera_AttachMonBg(u8 battler, u8 bg)
{
    if (battler >= MAX_BATTLERS_COUNT || (bg != 1 && bg != 2))
        return;
    if (!sMoveCamera.active || sMoveCamera.blocked || sCamera.zoom != 256 || sMoveBlend != 256)
    {
        BattleCamera_BlockMove();
        return; // Cannot affine-scale the mon's text BG or inherit a zoomed pose safely.
    }
    sMoveCamera.bgLayer[battler] = bg;
    sMoveCamera.generic = TRUE; // Shared1x translation, including all effect sprites.
    sCamera.startZoom = sCamera.targetZoom = 256;
}

void BattleCamera_ClearMonBg(u8 bg)
{
    u32 i;
    for (i = 0; i < MAX_BATTLERS_COUNT; i++)
        if (sMoveCamera.bgLayer[i] == bg)
            sMoveCamera.bgLayer[i] = 0;
}

s16 BattleCamera_GetMonBgOffset(u8 bg, bool8 vertical)
{
    u32 i;
    if (!HasScanlineLease() || sMoveCamera.blocked)
        return 0;
    for (i = 0; i < MAX_BATTLERS_COUNT; i++)
        if (sMoveCamera.bgLayer[i] == bg)
        {
            s16 value = vertical ? BattleCamera_GetBgY() : BattleCamera_GetBgX();
            return value >= 256 ? value - 512 : value;
        }
    return 0;
}

static void UpdateImpactShake(void)
{
    const struct MoveCameraProfile *profile = &sMoveProfiles[sMoveCamera.profile];
    u16 ticks = min(profile->shakeTicks, BATTLE_CAMERA_MAX_SHAKE_TICKS);
    sShakeX = sShakeY = 0;
    if (!sMoveCamera.generic && sMoveCamera.impactSeen && sMoveCamera.shakeTick < ticks)
    {
        s32 decay = (ticks - sMoveCamera.shakeTick) * 256 / ticks;
        s32 sign = (sMoveCamera.shakeTick / 2) & 1 ? -1 : 1;
        sShakeX = sign * min(profile->shakeX, BATTLE_CAMERA_MAX_SHAKE_X) * decay;
        sShakeY = -sign * min(profile->shakeY, BATTLE_CAMERA_MAX_SHAKE_Y) * decay;
        sMoveCamera.shakeTick++;
    }
}

static bool32 UpdateMoveCamera(void)
{
    u32 i;
    const struct MoveCameraProfile *profile = &sMoveProfiles[sMoveCamera.profile];
    if (!gMain.inBattle || !sSceneReady || !gBattleSpritesDataPtr
        || !gBattleSpritesDataPtr->battlerData || !gBattleSpritesDataPtr->healthBoxesData
        || gBattlersCount != sMoveCamera.battlerCount
        || ++sMoveCamera.age > BATTLE_CAMERA_MOVE_MAX_TICKS)
        goto cancel;
    if (gPaletteFade.active || gBattle_BG3_X || gBattle_BG3_Y || gSpriteCoordOffsetX || gSpriteCoordOffsetY
        || (sOwnsScanlines && !HasScanlineLease()))
        SuspendMoveNow();
    if (sMoveCamera.blocked)
    {
        if (!gAnimScriptActive)
            goto cancel;
        return FALSE;
    }
    profile = &sMoveProfiles[sMoveCamera.profile];
    for (i = 0; i < gBattlersCount; i++)
    {
        if (gBattlerSpriteIds[i] != sMoveCamera.actorSprite[i])
            goto cancel;
        if (sMoveCamera.actorMask & (1u << i))
        {
            if (!VisibleMoveBattler(i) || gSprites[sMoveCamera.actorSprite[i]].images != sMoveCamera.actorImages[i])
                goto cancel;
        }
        else if (VisibleMoveBattler(i))
            goto cancel; // A new battler joined an otherwise unchanged slot.
    }
    if (!gAnimScriptActive)
        EndMoveNow();
    if (!sMoveCamera.active)
        return FALSE;
    if (!HasScanlineLease())
    {
        if (sOwnsScanlines || !BuildRows() || !AcquireScanlines())
        {
            SuspendMoveNow();
            return FALSE;
        }
        sCamera.active = TRUE;
    }
    if (sHandoffTick < BATTLE_CAMERA_HANDOFF_TICKS)
    {
        u16 weight = Ease(256, ++sHandoffTick, BATTLE_CAMERA_HANDOFF_TICKS);
        sMoveBlend = sHandoffStartBlend + (256 - sHandoffStartBlend) * weight / 256;
    }
    if (sMoveCamera.elapsed < sMoveCamera.duration)
    {
        s32 weight = EasingWeight(++sMoveCamera.elapsed, sMoveCamera.duration,
                                sMoveCamera.easing);
        sCamera.x = sCamera.startX + (sCamera.targetX - sCamera.startX) * weight / 256;
        sCamera.y = sCamera.startY + (sCamera.targetY - sCamera.startY) * weight / 256;
        sCamera.zoom = sCamera.startZoom + (sCamera.targetZoom - sCamera.startZoom) * weight / 256;
    }
    UpdateImpactShake();
    if (sMoveCamera.elapsed == sMoveCamera.duration)
    {
        switch (sMoveCamera.phase)
        {
        case CAMERA_PHASE_MOVE_START:
            if (sMoveCamera.profile == MOVE_CAMERA_EMBER)
            {
                sMoveCamera.phase = CAMERA_PHASE_TARGET_FOCUS;
                SetMoveGoal(sMoveCamera.target, profile->travel, profile->zoom);
            }
            else
                sMoveCamera.phase = CAMERA_PHASE_ATTACK_PRESENTATION;
            break;
        case CAMERA_PHASE_TARGET_FOCUS:
        case CAMERA_PHASE_IMPACT:
            sMoveCamera.phase = CAMERA_PHASE_ATTACK_PRESENTATION;
            break;
        case CAMERA_PHASE_RETURN:
            goto cancel;
        }
    }
    return TRUE;

cancel:
    BattleCamera_Cancel();
    return FALSE;
}
