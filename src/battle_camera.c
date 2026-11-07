#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_main.h"
#include "battle_camera.h"
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
static EWRAM_DATA struct CameraSpriteBackup sBackups[MAX_BATTLERS_COUNT * 3] = {0};
static EWRAM_DATA u8 sBackupCount = 0;
static EWRAM_DATA u16 sIdleFrames = 0;
static EWRAM_DATA u16 sWaitTicks = 0;
static EWRAM_DATA u8 sLastTrack = 0;
static EWRAM_DATA u32 sVisualRandom = 0;
static EWRAM_DATA bool8 sSceneReady = FALSE;
static EWRAM_DATA bool8 sOwnsScanlines = FALSE;
static EWRAM_DATA u32 sScanlineGeneration = 0;
// Include the transfer after the final visible row. Main-loop scratch only;
// the completed table is published into the engine's DMA buffer in VBlank.
static EWRAM_DATA u32 ALIGNED(4) sRows[DISPLAY_HEIGHT + 1] = {0};

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
    memset(&sCamera, 0, sizeof(sCamera));
    sCamera.zoom = sCamera.targetZoom = 256;
    sIdleFrames = 0;
    sWaitTicks = BATTLE_CAMERA_WAIT_TICKS;
}

void BattleCamera_Reset(void)
{
    BattleCamera_Cancel();
    sSceneReady = FALSE;
    sLastTrack = 0xFF;
    sVisualRandom = gMain.vblankCounter1 ^ 0x6D2B79F5;
}

void BattleCamera_SetEnvironment(u16 environment)
{
    BattleCamera_Cancel();
    // Called by the actual loader after environment overrides are resolved.
    sSceneReady = environment < BATTLE_ENVIRONMENT_COUNT;
}

static s16 Ease(s16 destination, u16 frame, u16 duration)
{
    s32 t = frame * 256 / duration;
    s32 weight = t;
    if (BATTLE_CAMERA_EASING == BATTLE_CAMERA_EASING_SMOOTHSTEP)
        weight = t * t * (768 - 2 * t) / 65536;
    return destination * weight / 256;
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
    sCamera.startX = sCamera.x;
    sCamera.startY = sCamera.y;
    sCamera.startZoom = sCamera.zoom;
    sCamera.targetX = BATTLE_CAMERA_MAX_PAN_X * key->x;
    sCamera.targetY = -BATTLE_CAMERA_MAX_PAN_Y * key->y;
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

static s32 DepthWeight(s32 sourceY)
{
    s32 depth = (sourceY - BATTLE_CAMERA_HORIZON) * 256
              / (BATTLE_CAMERA_NEAR_CONTACT - BATTLE_CAMERA_HORIZON);
    depth = max(0, min(256, depth));
    return 256 - BATTLE_CAMERA_PARALLAX + depth * BATTLE_CAMERA_PARALLAX / 256;
}

static s32 PanAt(s32 sourceY)
{
    return sCamera.x * DepthWeight(sourceY) / 65536;
}

static s32 GroundY(s32 sourceY)
{
    return BATTLE_CAMERA_HORIZON
         + ((sourceY - BATTLE_CAMERA_HORIZON) * 256 - sCamera.y) * sCamera.zoom / 65536;
}

static bool32 BuildRows(void)
{
    u32 row;
    s32 inverseZoom = 16777216 / sCamera.zoom;
    for (row = 0; row <= DISPLAY_HEIGHT; row++)
    {
        s32 sourceY = BATTLE_CAMERA_HORIZON
                    + ((((s32)row - BATTLE_CAMERA_HORIZON) * inverseZoom / 256
                    + sCamera.y) / 256);
        s32 sourceX = PanAt(sourceY);
        // Validate unwrapped coordinates before encoding GBA register bits.
        // Negative Y samples address the explicitly mirrored map rows30/31.
        if (sourceY < BATTLE_CAMERA_SOURCE_MIN_Y || sourceY > BATTLE_CAMERA_SOURCE_MAX_Y
            || sourceX < 0 || sourceX + DISPLAY_WIDTH > 256)
            return FALSE;
        sRows[row] = (sourceX & 0x1FF) | ((u32)((sourceY - row) & 0x1FF) << 16);
    }
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
    if (!sprite->inUse || sprite->invisible)
        return TRUE;
    if (sprite->subspriteMode != SUBSPRITES_OFF || sBackupCount >= ARRAY_COUNT(sBackups))
        return FALSE;
    backup = &sBackups[sBackupCount++];
    backup->sprite = *sprite;
    backup->id = id;
    backup->matrix = 0xFF;

    // Image, split shadows, and floor use the same ground-contact transform.
    sprite->x = anchorX - PanAt(anchorY)
              + (sprite->x + sprite->x2 - anchorX) * scale / 256 - sprite->x2;
    sprite->y = GroundY(anchorY)
              + (sprite->y + sprite->y2 - anchorY) * scale / 256 - sprite->y2;
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

    if (!BATTLE_CAMERA_DEMO || !sSceneReady || !gMain.inBattle || !gBattleSpritesDataPtr
        || (gBattlersCount < 2 || gBattlersCount > MAX_BATTLERS_COUNT) || !InBattleChoosingMoves() || gPaletteFade.active
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
        scale = 256 + (sCamera.zoom - 256)
              * (256 - BATTLE_CAMERA_PERSPECTIVE + DepthWeight(anchorY) * BATTLE_CAMERA_PERSPECTIVE / 256) / 256;
        if (!TransformSprite(id, anchorX, anchorY, scale))
            goto cancel;
        if (GetBattlerSide(battler) == B_SIDE_OPPONENT)
        {
            if (!TransformSprite(gBattleSpritesDataPtr->healthBoxesData[battler].shadowSpriteIdPrimary, anchorX, anchorY, scale))
                goto cancel;
            if (B_ENEMY_MON_SHADOW_STYLE >= GEN_4 && P_GBA_STYLE_SPECIES_GFX == FALSE
                && !TransformSprite(gBattleSpritesDataPtr->healthBoxesData[battler].shadowSpriteIdSecondary, anchorX, anchorY, scale))
                goto cancel;
        }
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
