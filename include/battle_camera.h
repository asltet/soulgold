#ifndef GUARD_BATTLE_CAMERA_H
#define GUARD_BATTLE_CAMERA_H

// Authored demo settings; zoom/strength values have eight fractional bits.
#define BATTLE_CAMERA_DEMO TRUE
#define BATTLE_CAMERA_MAX_ZOOM 288          // 1.125x near visible scale.
#define BATTLE_CAMERA_MAX_PAN_X 12          // Rightward camera pan, pixels.
#define BATTLE_CAMERA_MAX_PAN_Y 4           // Upward camera pan, pixels.
#define BATTLE_CAMERA_PERSPECTIVE 64        // Far image-scale attenuation.
#define BATTLE_CAMERA_PARALLAX 128          // Far pan attenuation (0 = uniform).
#define BATTLE_CAMERA_EASING_SMOOTHSTEP 1
#define BATTLE_CAMERA_EASING BATTLE_CAMERA_EASING_SMOOTHSTEP // 0 = linear.
#define BATTLE_CAMERA_MOVE_TICKS 48
#define BATTLE_CAMERA_HOLD_TICKS 24
#define BATTLE_CAMERA_WAIT_TICKS 30

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
#if BATTLE_CAMERA_PARALLAX < 0 || BATTLE_CAMERA_PARALLAX > 256 || BATTLE_CAMERA_PERSPECTIVE < 0 || BATTLE_CAMERA_PERSPECTIVE > 256
#error Camera strength must be in the range 0..256
#endif
#if BATTLE_CAMERA_MOVE_TICKS < 1 || BATTLE_CAMERA_MOVE_TICKS > 600 || BATTLE_CAMERA_HOLD_TICKS < 0 || BATTLE_CAMERA_HOLD_TICKS > 600 || BATTLE_CAMERA_WAIT_TICKS < 1 || BATTLE_CAMERA_WAIT_TICKS > 600
#error Invalid camera duration
#endif
#if BATTLE_CAMERA_NEAR_CONTACT <= BATTLE_CAMERA_HORIZON
#error Invalid camera ground depth interval
#endif
#if BATTLE_CAMERA_EASING != 0 && BATTLE_CAMERA_EASING != BATTLE_CAMERA_EASING_SMOOTHSTEP
#error Unsupported camera easing
#endif

void BattleCamera_Reset(void);
void BattleCamera_Cancel(void);
void BattleCamera_SetEnvironment(u16 environment);
void BattleCamera_BeginFrame(void);
void BattleCamera_EndFrame(void);
void BattleCamera_PublishScanlines(void);
s16 BattleCamera_GetBgX(void);
s16 BattleCamera_GetBgY(void);

#endif
