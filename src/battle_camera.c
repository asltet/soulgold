#include "global.h"
#include "battle.h"
#include "battle_main.h"
#include "battle_camera.h"
#include "main.h"
#include "palette.h"
#include "sprite.h"
#include "constants/battle.h"

// All camera coordinates and zoom use 8 fractional bits. Coordinates are
// screen-space art units, not the DS renderer's world coordinate system.
struct BattleCamera
{
    s16 x, y, zoom;
    s16 targetX, targetY, targetZoom;
    u16 frame;
    bool8 active;
};

struct CameraSpriteBackup
{
    struct Sprite sprite;
    u8 id;
    u8 matrix;
};

static EWRAM_DATA struct BattleCamera sCamera = {0};
static EWRAM_DATA struct CameraSpriteBackup sBackups[6] = {0};
static EWRAM_DATA u8 sBackupCount = 0;
static EWRAM_DATA s16 sBgX = 0;
static EWRAM_DATA s16 sBgY = 0;
static EWRAM_DATA bool8 sDemoPlayed = FALSE;
static EWRAM_DATA u8 sIdleFrames = 0;

void BattleCamera_Cancel(void)
{
    memset(&sCamera, 0, sizeof(sCamera));
    sCamera.zoom = sCamera.targetZoom = 256;
    sBgX = sBgY = 0;
    sIdleFrames = 0;
}

void BattleCamera_Reset(void)
{
    BattleCamera_Cancel();
    sDemoPlayed = FALSE;
    sIdleFrames = 0;
}

// Integer smoothstep: exact endpoints and no accumulating interpolation error.
static s16 Ease(s16 destination, u16 frame)
{
    s32 t = frame * 256 / 48;
    s32 weight = t * t * (768 - 2 * t) / 65536;
    return destination * weight / 256;
}

static void TransformSprite(u8 id, bool32 shadow, s16 anchorX)
{
    struct Sprite *sprite;
    struct CameraSpriteBackup *backup;
    u8 matrix;
    s32 scale;
    s32 foot;
    u32 i;

    if (id >= MAX_SPRITES || sBackupCount >= ARRAY_COUNT(sBackups))
        return;
    for (i = 0; i < sBackupCount; i++)
        if (sBackups[i].id == id)
            return;
    sprite = &gSprites[id];
    if (!sprite->inUse || sprite->invisible || sprite->subspriteMode != SUBSPRITES_OFF)
        return;
    backup = &sBackups[sBackupCount++];
    backup->sprite = *sprite;
    backup->id = id;
    backup->matrix = 0xFF;

    // Shared contact-plane translation keeps painted BG3 platforms aligned.
    sprite->x -= sBgX;
    sprite->y -= sBgY;
    if (sCamera.zoom == 256)
        return;
    matrix = AllocOamMatrix();
    if (matrix == 0xFF)
        return; // Translation remains usable under affine resource pressure.
    backup->matrix = matrix;
    scale = sCamera.zoom;
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
    // Scale around the bottom of the original OBJ bounding box (battler),
    // or its centre (shadow). Species-specific foot calibration is future work.
    if (shadow)
        sprite->x += (backup->sprite.x + sprite->x2 - anchorX) * (scale - 256) / 256;
    foot = shadow ? 0 : -sprite->centerToCornerVecY;
    if (sprite->oam.affineMode == ST_OAM_AFFINE_DOUBLE)
        foot /= 2;
    sprite->y -= foot * (scale - 256) / 256;
    sprite->oam.affineMode = ST_OAM_AFFINE_DOUBLE;
    sprite->oam.matrixNum = matrix;
    CalcCenterToCornerVec(sprite, sprite->oam.shape, sprite->oam.size, sprite->oam.affineMode);
}

void BattleCamera_BeginFrame(void)
{
    u32 battler;
    u16 frame;

    // Menu-only demo: never compete with send-out, attacks, fades, or link IO.
    if (!BATTLE_CAMERA_DEMO || !gMain.inBattle || !gBattleSpritesDataPtr
        || gBattlersCount != 2 || !InBattleChoosingMoves() || gPaletteFade.active
        || (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_FRONTIER)))
    {
        BattleCamera_Cancel();
        return;
    }
    if (!sCamera.active && !sDemoPlayed && ++sIdleFrames >= 30)
    {
        BattleCamera_Cancel();
        sDemoPlayed = TRUE;
        sCamera.active = TRUE;
        // Opponent lies to the right and above neutral framing.
        sCamera.targetX = 4 * 256;
        sCamera.targetY = -2 * 256;
        sCamera.targetZoom = 268; // 1.046875x visible scale.
    }
    if (!sCamera.active)
        return;
    frame = sCamera.frame++;
    if (frame >= 120)
    {
        BattleCamera_Cancel();
        return;
    }
    // 48 ticks out, 24 ticks hold, 48 ticks back. Menu runs at 1x.
    frame = frame < 48 ? frame : frame < 72 ? 48 : 120 - frame;
    sCamera.x = Ease(sCamera.targetX, frame);
    sCamera.y = Ease(sCamera.targetY, frame);
    sCamera.zoom = 256 + Ease(sCamera.targetZoom - 256, frame);
    sBgX = sCamera.x / 256;
    sBgY = sCamera.y / 256;
    for (battler = 0; battler < gBattlersCount; battler++)
    {
        u8 id = gBattlerSpriteIds[battler];
        s16 anchorX;
        if (id >= MAX_SPRITES || !gSprites[id].inUse)
            continue;
        anchorX = gSprites[id].x + gSprites[id].x2;
        TransformSprite(id, FALSE, anchorX);
        if (GetBattlerSide(battler) == B_SIDE_OPPONENT)
        {
            TransformSprite(gBattleSpritesDataPtr->healthBoxesData[battler].shadowSpriteIdPrimary, TRUE, anchorX);
            if (B_ENEMY_MON_SHADOW_STYLE >= GEN_4 && P_GBA_STYLE_SPECIES_GFX == FALSE)
                TransformSprite(gBattleSpritesDataPtr->healthBoxesData[battler].shadowSpriteIdSecondary, TRUE, anchorX);
        }
    }
}

void BattleCamera_EndFrame(void)
{
    // BuildOamBuffer has copied the presentation. Restore before any task runs.
    while (sBackupCount)
    {
        struct CameraSpriteBackup *backup = &sBackups[--sBackupCount];
        gSprites[backup->id] = backup->sprite;
        if (backup->matrix != 0xFF)
            FreeOamMatrix(backup->matrix);
    }
}

s16 BattleCamera_GetBgX(void) { return sBgX; }
s16 BattleCamera_GetBgY(void) { return sBgY; }
