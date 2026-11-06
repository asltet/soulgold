#ifndef GUARD_BATTLE_CAMERA_H
#define GUARD_BATTLE_CAMERA_H

// Presentation-only prototype. Disable to remove the automatic first-menu demo.
#define BATTLE_CAMERA_DEMO TRUE

void BattleCamera_Reset(void);
void BattleCamera_Cancel(void);
void BattleCamera_BeginFrame(void);
void BattleCamera_EndFrame(void);
s16 BattleCamera_GetBgX(void);
s16 BattleCamera_GetBgY(void);

#endif
