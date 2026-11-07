// GBA approximations of BW2 idle script choreography, not DS camera values.
// Key: duration in GBA updates, X/Y/zoom fractions of the admitted envelope.
// Y weights describe upward pan; all weights stay in 0..256 for prepared art.
// Equal consecutive poses are holds. Every track restores neutral framing.
static const struct CameraKey sIdle235[] = {
    {90, 256, 256, 224}, {500, 32, 64, 96},
    {30, 48, 64, 80}, {50, 0, 0, 0},
};
static const struct CameraKey sIdle24c[] = {
    {40, 256, 128, 160}, {25, 256, 128, 160},
    {450, 48, 64, 128}, {30, 48, 64, 128}, {50, 0, 0, 0},
};
static const struct CameraKey sIdle24d[] = {
    {100, 32, 192, 128}, {400, 224, 64, 128}, {50, 0, 0, 0},
};
static const struct CameraKey sIdle24e[] = {
    {45, 128, 256, 160}, {120, 128, 64, 256},
    {100, 128, 64, 256}, {50, 0, 0, 0},
};
static const struct CameraKey sIdle24f[] = {
    {30, 32, 64, 96}, {80, 192, 192, 160},
    {150, 256, 256, 192}, {50, 0, 0, 0},
};
static const struct CameraKey sIdle250[] = {
    {100, 256, 256, 224}, {50, 256, 256, 224},
    {50, 192, 96, 160}, {50, 192, 96, 160},
    {120, 32, 96, 160}, {120, 32, 96, 160},
    {150, 160, 128, 192}, {100, 192, 160, 160},
    {50, 192, 160, 160}, {100, 0, 0, 0},
};
static const struct CameraTrack sIdleTracks[] = {
    {sIdle235, ARRAY_COUNT(sIdle235)},
    {sIdle24c, ARRAY_COUNT(sIdle24c)},
    {sIdle24d, ARRAY_COUNT(sIdle24d)},
    {sIdle24e, ARRAY_COUNT(sIdle24e)},
    {sIdle24f, ARRAY_COUNT(sIdle24f)},
    {sIdle250, ARRAY_COUNT(sIdle250)},
};
