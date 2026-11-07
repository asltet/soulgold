// Specialized profiles override metadata defaults; complex effects defer safely.
// Durations are authored logical ticks, not claimed BW2 move timings.
static const struct MoveCameraProfile sMoveProfiles[] = {
    // start, travel, impact, return, pan(Q8), zoom delta(Q8), easing, shake X/Y pixels/ticks
    {6, 10, 4, 10, 256, BATTLE_CAMERA_MOVE_ZOOM_DELTA, BATTLE_CAMERA_EASING_SMOOTHSTEP, 2, 1, 8}, // melee user -> target
    {8, 8, 4, 10, 256, BATTLE_CAMERA_MOVE_ZOOM_DELTA, BATTLE_CAMERA_EASING_SMOOTHSTEP, 1, 0, 6}, // defender emphasis
    {8, 24, 6, 10, 256, BATTLE_CAMERA_MOVE_ZOOM_DELTA, BATTLE_CAMERA_EASING_OUT_CUBIC, 0, 0, 0}, // ranged endpoints
    {10, 16, 4, 12, 224, BATTLE_CAMERA_MOVE_ZOOM_DELTA, BATTLE_CAMERA_EASING_SMOOTHERSTEP, 0, 0, 0}, // self
    {8, 12, 4, 10, 256, BATTLE_CAMERA_MOVE_ZOOM_DELTA, BATTLE_CAMERA_EASING_SMOOTHSTEP, 0, 0, 0}, // ally
    {10, 18, 4, 12, 96, 0, BATTLE_CAMERA_EASING_SMOOTHERSTEP, 0, 0, 0}, // wide spread/field
    {10, 16, 4, 12, 224, 16, BATTLE_CAMERA_EASING_SMOOTHERSTEP, 0, 0, 0}, // opponent status
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
