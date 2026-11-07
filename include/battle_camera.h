#ifndef GUARD_BATTLE_CAMERA_H
#define GUARD_BATTLE_CAMERA_H

// Renderer limits; zoom/strength values have eight fractional bits.
// Authored GBA track shapes/durations: src/data/battle_camera_tracks.h.
#define BATTLE_CAMERA_DEMO TRUE
#define BATTLE_CAMERA_MAX_SCENE_SPRITES 24
#define BATTLE_CAMERA_MOVE_MAX_TICKS 1200
#define BATTLE_CAMERA_RECOVERY_WAIT_TICKS 60
#define BATTLE_CAMERA_RECOVERY_FOCUS_TICKS 12
#define BATTLE_CAMERA_FIELD_PAN_X 4
#define BATTLE_CAMERA_MOVE_ZOOM_DELTA 24 // Up to1.094x; framing may reduce.
#define BATTLE_CAMERA_HANDOFF_TICKS 16
#define BATTLE_CAMERA_FOCUS_LEFT_X 72
#define BATTLE_CAMERA_FOCUS_SPAN_X 128
#define BATTLE_CAMERA_MAX_ZOOM 288          // 1.125x near visible scale.
#define BATTLE_CAMERA_MAX_PAN_X 16          // Idle/source envelope, pixels.
#define BATTLE_CAMERA_MOVE_PAN_X 16         // Stronger moves, within prepared artwork.
#define BATTLE_CAMERA_IDLE_DOUBLES_PAN_X 12 // Preserve the near edge in doubles.
#define BATTLE_CAMERA_IDLE_PAN_Y 2          // Less vertical scenery deformation.
#define BATTLE_CAMERA_MAX_PAN_Y 4           // Upward camera pan, pixels.
#define BATTLE_CAMERA_PARALLAX 160          // Far pan attenuation (0 = uniform).
#define BATTLE_CAMERA_EASING_LINEAR 0
#define BATTLE_CAMERA_EASING_SMOOTHSTEP 1
#define BATTLE_CAMERA_EASING_IN_CUBIC 2
#define BATTLE_CAMERA_EASING_OUT_CUBIC 3
#define BATTLE_CAMERA_EASING_SMOOTHERSTEP 4
#define BATTLE_CAMERA_RETURN_EASING BATTLE_CAMERA_EASING_SMOOTHERSTEP
#define BATTLE_CAMERA_FRAMING_MARGIN 2
#define BATTLE_CAMERA_MAX_SHAKE_X 2
#define BATTLE_CAMERA_MAX_SHAKE_Y 1
#define BATTLE_CAMERA_MAX_SHAKE_TICKS 12
#define BATTLE_CAMERA_EASING BATTLE_CAMERA_EASING_SMOOTHSTEP // 0 = linear.
#define BATTLE_CAMERA_WAIT_TICKS 900        // BW2 normal initial idle threshold.

// Q8 depth planes. Select 0 for restrained, 1 for cinematic (default).
#ifndef BATTLE_CAMERA_DEPTH_PRESET
#define BATTLE_CAMERA_DEPTH_PRESET 1
#endif
#define BATTLE_CAMERA_MIDGROUND_Y 72
#define BATTLE_CAMERA_OTHER_ENV_DEPTH 128 // Restrained until artwork-specific audit.
#if BATTLE_CAMERA_DEPTH_PRESET == 0
#define BATTLE_CAMERA_BACKGROUND_MOTION 64
#define BATTLE_CAMERA_MIDGROUND_MOTION 160
#define BATTLE_CAMERA_FOREGROUND_MOTION 256
#define BATTLE_CAMERA_PERSPECTIVE 128
#define BATTLE_CAMERA_BACKGROUND_ZOOM 128
#define BATTLE_CAMERA_MIDGROUND_ZOOM 192
#elif BATTLE_CAMERA_DEPTH_PRESET == 1
#define BATTLE_CAMERA_BACKGROUND_MOTION 0
#define BATTLE_CAMERA_MIDGROUND_MOTION 64
#define BATTLE_CAMERA_FOREGROUND_MOTION 256
#define BATTLE_CAMERA_PERSPECTIVE 256
#define BATTLE_CAMERA_BACKGROUND_ZOOM 32
#define BATTLE_CAMERA_MIDGROUND_ZOOM 128
#else
#error Unknown camera depth preset
#endif
// Contacts share projection; size responses may be reduced, never amplified.
#define BATTLE_CAMERA_BATTLER_DEPTH_SCALE 256
#define BATTLE_CAMERA_SHADOW_DEPTH_SCALE 256
#define BATTLE_CAMERA_PLATFORM_DEPTH_SCALE 256 // Baked floor: must remain 256.
#define BATTLE_CAMERA_PITCH_APPROXIMATION 0    // Unsupported: no extra pitch.

// Prepared battle-background artwork profile: unwrapped X 0..255, Y -16..175.
#define BATTLE_CAMERA_HORIZON 32
#define BATTLE_CAMERA_NEAR_CONTACT 120
#define BATTLE_CAMERA_SOURCE_MIN_Y (-16)
#define BATTLE_CAMERA_SOURCE_MAX_Y 175

#if BATTLE_CAMERA_MAX_ZOOM < 256 || BATTLE_CAMERA_MAX_ZOOM > 294
#error Camera zoom exceeds the audited Plain prototype envelope
#endif
#if BATTLE_CAMERA_MAX_PAN_X < 0 || BATTLE_CAMERA_MAX_PAN_X > 16 || BATTLE_CAMERA_MAX_PAN_Y < 0 || BATTLE_CAMERA_MAX_PAN_Y > 16
#error Camera pan exceeds the prepared Plain artwork margins
#endif
#if BATTLE_CAMERA_IDLE_DOUBLES_PAN_X < 0 || BATTLE_CAMERA_IDLE_DOUBLES_PAN_X > BATTLE_CAMERA_MAX_PAN_X
#error Invalid doubles idle pan
#endif
#if BATTLE_CAMERA_MOVE_PAN_X < 0 || BATTLE_CAMERA_MOVE_PAN_X > BATTLE_CAMERA_MAX_PAN_X || BATTLE_CAMERA_IDLE_PAN_Y < 0 || BATTLE_CAMERA_IDLE_PAN_Y > BATTLE_CAMERA_MAX_PAN_Y
#error Idle/move tuning exceeds the prepared camera envelope
#endif
#if BATTLE_CAMERA_PARALLAX < 0 || BATTLE_CAMERA_PARALLAX > 256 || BATTLE_CAMERA_PERSPECTIVE < 0 || BATTLE_CAMERA_PERSPECTIVE > 256
#error Camera strength must be in the range 0..256
#endif
#if BATTLE_CAMERA_HANDOFF_TICKS < 1 || BATTLE_CAMERA_HANDOFF_TICKS > 120
#error Invalid camera handoff duration
#endif
#if BATTLE_CAMERA_MAX_SCENE_SPRITES < 12 || BATTLE_CAMERA_MAX_SCENE_SPRITES > 64
#error Invalid camera snapshot capacity
#endif
#if BATTLE_CAMERA_MOVE_MAX_TICKS < 1 || BATTLE_CAMERA_MOVE_MAX_TICKS > 3600 || BATTLE_CAMERA_FOCUS_SPAN_X < 1
#error Invalid move camera watchdog or focus calibration
#endif
#if BATTLE_CAMERA_WAIT_TICKS < 1 || BATTLE_CAMERA_WAIT_TICKS > 3600
#error Invalid camera initial idle delay
#endif
#if BATTLE_CAMERA_NEAR_CONTACT <= BATTLE_CAMERA_HORIZON
#error Invalid camera ground depth interval
#endif
#if BATTLE_CAMERA_EASING < 0 || BATTLE_CAMERA_EASING > BATTLE_CAMERA_EASING_SMOOTHERSTEP || BATTLE_CAMERA_RETURN_EASING < 0 || BATTLE_CAMERA_RETURN_EASING > BATTLE_CAMERA_EASING_SMOOTHERSTEP
#error Unsupported camera easing
#endif

#if BATTLE_CAMERA_MIDGROUND_Y <= BATTLE_CAMERA_HORIZON || BATTLE_CAMERA_MIDGROUND_Y >= BATTLE_CAMERA_NEAR_CONTACT
#error Invalid scene depth anchors
#endif
#if BATTLE_CAMERA_BACKGROUND_MOTION < 0 || BATTLE_CAMERA_BACKGROUND_MOTION > BATTLE_CAMERA_MIDGROUND_MOTION || BATTLE_CAMERA_MIDGROUND_MOTION > BATTLE_CAMERA_FOREGROUND_MOTION || BATTLE_CAMERA_FOREGROUND_MOTION > 256
#error Invalid depth motion weights
#endif
#if BATTLE_CAMERA_BACKGROUND_ZOOM < 0 || BATTLE_CAMERA_BACKGROUND_ZOOM > BATTLE_CAMERA_MIDGROUND_ZOOM || BATTLE_CAMERA_MIDGROUND_ZOOM > 256
#error Invalid depth zoom weights
#endif
#if BATTLE_CAMERA_BATTLER_DEPTH_SCALE < 0 || BATTLE_CAMERA_BATTLER_DEPTH_SCALE > 256 || BATTLE_CAMERA_SHADOW_DEPTH_SCALE < 0 || BATTLE_CAMERA_SHADOW_DEPTH_SCALE > 256
#error Invalid scene image response
#endif
#if BATTLE_CAMERA_OTHER_ENV_DEPTH < 0 || BATTLE_CAMERA_OTHER_ENV_DEPTH > 256
#error Invalid fallback environment strength
#endif
#if BATTLE_CAMERA_PLATFORM_DEPTH_SCALE != 256 || BATTLE_CAMERA_PITCH_APPROXIMATION != 0
#error Baked platforms require shared floor projection; pitch is not supported
#endif

#if BATTLE_CAMERA_FRAMING_MARGIN < 0 || BATTLE_CAMERA_FRAMING_MARGIN > 8 || BATTLE_CAMERA_MAX_SHAKE_X < 0 || BATTLE_CAMERA_MAX_SHAKE_X > 2 || BATTLE_CAMERA_MAX_SHAKE_Y < 0 || BATTLE_CAMERA_MAX_SHAKE_Y > 1 || BATTLE_CAMERA_MAX_SHAKE_TICKS < 1 || BATTLE_CAMERA_MAX_SHAKE_TICKS > 32
#error Invalid cinematic framing/shake envelope
#endif

#if BATTLE_CAMERA_MOVE_ZOOM_DELTA < 0 || BATTLE_CAMERA_MOVE_ZOOM_DELTA > BATTLE_CAMERA_MAX_ZOOM - 256 || BATTLE_CAMERA_FIELD_PAN_X < 0 || BATTLE_CAMERA_FIELD_PAN_X > BATTLE_CAMERA_MOVE_PAN_X
#error Universal camera cue exceeds artwork envelope
#endif
#if BATTLE_CAMERA_RECOVERY_WAIT_TICKS < 1 || BATTLE_CAMERA_RECOVERY_WAIT_TICKS > 300 || BATTLE_CAMERA_RECOVERY_FOCUS_TICKS < 1 || BATTLE_CAMERA_RECOVERY_FOCUS_TICKS > 60
#error Invalid deferred camera cue timing
#endif

enum BattleCameraMovePhase
{
    CAMERA_PHASE_NEUTRAL,
    CAMERA_PHASE_MOVE_START,
    CAMERA_PHASE_TARGET_FOCUS,
    CAMERA_PHASE_ATTACK_PRESENTATION,
    CAMERA_PHASE_IMPACT,
    CAMERA_PHASE_RETURN,
};

void BattleCamera_StartMove(u16 move, u8 attacker, u8 target, u8 targetType, const u8 *script);
void BattleCamera_EndMove(void);
void BattleCamera_InterruptAnimation(void);
void BattleCamera_OnSpriteCreated(u8 spriteId, u16 tileTag);
void BattleCamera_BlockMove(void);
void BattleCamera_Reset(void);
void BattleCamera_Cancel(void);
void BattleCamera_SetEnvironment(u16 environment);
void BattleCamera_BeginFrame(void);
void BattleCamera_EndFrame(void);
void BattleCamera_PublishScanlines(void);
s16 BattleCamera_GetBgX(void);
s16 BattleCamera_GetBgY(void);

#endif
