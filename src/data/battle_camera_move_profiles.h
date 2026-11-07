// First audited scripts only. Unknown/replaced scripts retain neutral framing.
// Durations are authored logical ticks, not claimed BW2 move timings.
static const struct MoveCameraProfile sMoveProfiles[] = {
    // start, travel, impact, return, pan(Q8), zoom delta(Q8), easing
    {6, 10, 4, 8, 256, 8, BATTLE_CAMERA_EASING_SMOOTHSTEP}, // melee user -> target
    {8, 8, 4, 8, 256, 8, BATTLE_CAMERA_EASING_SMOOTHSTEP}, // defender emphasis
    {8, 24, 6, 10, 192, 0, BATTLE_CAMERA_EASING_SMOOTHSTEP}, // ranged endpoints
    {10, 16, 4, 12, 160, 12, BATTLE_CAMERA_EASING_SMOOTHSTEP}, // self
    {8, 12, 4, 10, 192, 0, BATTLE_CAMERA_EASING_SMOOTHSTEP}, // ally
    {1, 1, 1, 1, 0, 0, BATTLE_CAMERA_EASING_SMOOTHSTEP}, // stable spread/field
};
static const struct MoveCameraOverride sMoveOverrides[] = {
    {MOVE_HEADBUTT, MOVE_CAMERA_MELEE, gBattleAnimMove_Headbutt},
    {MOVE_PECK, MOVE_CAMERA_PHYSICAL, gBattleAnimMove_Peck},
    {MOVE_EMBER, MOVE_CAMERA_SPECIAL, gBattleAnimMove_Ember},
    {MOVE_SPLASH, MOVE_CAMERA_SELF, gBattleAnimMove_Splash},
    {MOVE_WITHDRAW, MOVE_CAMERA_SELF, gBattleAnimMove_Withdraw},
    {MOVE_HELPING_HAND, MOVE_CAMERA_ALLY, gBattleAnimMove_HelpingHand},
    {MOVE_GROWL, MOVE_CAMERA_GROUP, gBattleAnimMove_Growl},
    {MOVE_TAIL_WHIP, MOVE_CAMERA_GROUP, gBattleAnimMove_TailWhip},
    {MOVE_RAIN_DANCE, MOVE_CAMERA_GROUP, gBattleAnimMove_RainDance},
};
