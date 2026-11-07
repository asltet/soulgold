// GBA approximations of BW2 idle script choreography, not DS camera values.
// Key: duration in GBA updates, X/Y/zoom fractions of the admitted envelope.
// Y weights describe upward pan; all weights stay in 0..256 for prepared art.
// Equal consecutive poses are holds. Every track restores neutral framing.
// Lateral sweeps are emphasized; the24e family no longer only tilts/zooms.
static const struct CameraKey sIdle235[] = {
    {90, 256, 256, 224}, {500, 0, 64, 96},
    {30, 32, 64, 80}, {50, 0, 0, 0},
};
static const struct CameraKey sIdle24c[] = {
    {40, 256, 128, 160}, {25, 256, 128, 160},
    {450, 0, 64, 128}, {30, 0, 64, 128}, {50, 0, 0, 0},
};
static const struct CameraKey sIdle24d[] = {
    {100, 0, 192, 128}, {400, 256, 64, 128}, {50, 0, 0, 0},
};
static const struct CameraKey sIdle24e[] = {
    {45, 48, 256, 160}, {120, 224, 64, 256},
    {100, 224, 64, 256}, {50, 0, 0, 0},
};
static const struct CameraKey sIdle24f[] = {
    {30, 0, 64, 96}, {80, 208, 192, 160},
    {150, 256, 256, 192}, {50, 0, 0, 0},
};
static const struct CameraKey sIdle250[] = {
    {100, 256, 256, 224}, {50, 256, 256, 224},
    {50, 128, 96, 160}, {50, 128, 96, 160},
    {120, 0, 96, 160}, {120, 0, 96, 160},
    {150, 160, 128, 192}, {100, 224, 160, 160},
    {50, 224, 160, 160}, {100, 0, 0, 0},
};
static const struct CameraTrack sIdleTracks[] = {
    {sIdle235, ARRAY_COUNT(sIdle235)},
    {sIdle24c, ARRAY_COUNT(sIdle24c)},
    {sIdle24d, ARRAY_COUNT(sIdle24d)},
    {sIdle24e, ARRAY_COUNT(sIdle24e)},
    {sIdle24f, ARRAY_COUNT(sIdle24f)},
    {sIdle250, ARRAY_COUNT(sIdle250)},
};
