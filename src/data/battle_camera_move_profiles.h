// Source-guided acquire/hold/return; incompatible effects remain neutral.
// Durations are authored logical ticks, not claimed BW2 move timings.
static const struct MoveCameraProfile sMoveProfiles[] = {
    // acquire, event travel, impact, return: logical updates, not retail frames.
    {24, 25, 25, 28, 256, BATTLE_CAMERA_MOVE_ZOOM_DELTA, BATTLE_CAMERA_EASING_SMOOTHERSTEP, 0, 0, 0},
    {25, 25, 25, 28, 256, BATTLE_CAMERA_MOVE_ZOOM_DELTA, BATTLE_CAMERA_EASING_SMOOTHERSTEP, 0, 0, 0},
    {25, 30, 25, 28, 256, BATTLE_CAMERA_MOVE_ZOOM_DELTA, BATTLE_CAMERA_EASING_SMOOTHERSTEP, 0, 0, 0},
    {25, 25, 25, 28, 256, BATTLE_CAMERA_MOVE_ZOOM_DELTA, BATTLE_CAMERA_EASING_SMOOTHERSTEP, 0, 0, 0},
    {25, 25, 25, 28, 256, BATTLE_CAMERA_MOVE_ZOOM_DELTA, BATTLE_CAMERA_EASING_SMOOTHERSTEP, 0, 0, 0},
    {32, 32, 25, 32, 96, 0, BATTLE_CAMERA_EASING_SMOOTHERSTEP, 0, 0, 0},
    {25, 25, 25, 28, 256, BATTLE_CAMERA_MOVE_ZOOM_DELTA, BATTLE_CAMERA_EASING_SMOOTHERSTEP, 0, 0, 0},
    // Ember is user-approved: preserve its exact preceding profile/phase policy.
    {8, 24, 6, 10, 256, BATTLE_CAMERA_MOVE_ZOOM_DELTA, BATTLE_CAMERA_EASING_OUT_CUBIC, 0, 0, 0},
};
static const struct MoveCameraOverride sMoveOverrides[] = {
    {MOVE_HEADBUTT, MOVE_CAMERA_MELEE, gBattleAnimMove_Headbutt},
    {MOVE_PECK, MOVE_CAMERA_PHYSICAL, gBattleAnimMove_Peck},
    {MOVE_EMBER, MOVE_CAMERA_EMBER, gBattleAnimMove_Ember},
    {MOVE_SPLASH, MOVE_CAMERA_SELF, gBattleAnimMove_Splash},
    {MOVE_WITHDRAW, MOVE_CAMERA_SELF, gBattleAnimMove_Withdraw},
    {MOVE_HELPING_HAND, MOVE_CAMERA_ALLY, gBattleAnimMove_HelpingHand},
    {MOVE_GROWL, MOVE_CAMERA_GROUP, gBattleAnimMove_Growl},
    {MOVE_TAIL_WHIP, MOVE_CAMERA_GROUP, gBattleAnimMove_TailWhip},
    {MOVE_RAIN_DANCE, MOVE_CAMERA_GROUP, gBattleAnimMove_RainDance},
};
