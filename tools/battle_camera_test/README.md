# Battle camera prototype checks

Run from the SoulGold root after the documented toolchain setup.

```sh
python tools/battle_camera_test/check.py
gcc tools/battle_camera_test/test.c -iquote include -DMODERN=1 -DTESTING=0 -DEMERALD -O2 -w -o build/battle-camera-host-test
./build/battle-camera-host-test
make -j4
```

On Windows the native executable can use a `.exe` suffix. `check.py` requires
`arm-none-eabi-gcc` on PATH or DEVKITARM, the built `tools/preproc` executable,
and generated `include/constants/region_map_sections.h`. The normal make setup
creates prerequisites. If only that header is missing, its existing rule is:

```sh
tools/jsonproc/jsonproc src/data/region_map/region_map_sections.json src/data/region_map/region_map_sections.constants.json.txt include/constants/region_map_sections.h
```

The ARM check runs CPP -> project text preprocessor -> ARM GCC for the eight
relevant translation units. It permits the existing unused-const warning in
battle_main.c, matching the project's default unused-warning policy.
The host harness includes the real camera implementation and project headers;
it removes only the ARM target attribute and stubs hardware-facing sprite
helpers. `-w` suppresses host/header portability warnings, not compile errors.
It checks state restoration and arithmetic, not GBA ABI, GPU output, timing,
ROM linkage, animation compatibility, or visual quality. Run the emulator
checklist in `reports/faux3d-prototype-testing.md` in the parent workspace.

## Iteration8.1 Plain guards and row warp

Before rebuilding the map, verify its prepared margin:

```sh
python tools/battle_camera_test/prepare_plain_map.py --check
```

To regenerate the checked-in map from its unchanged original top14 tile rows,
run the same command without `--check`. This reuses existing tiles/palette,
preserves the original illustrated112 rows, and changes only lower/negative-Y
guard rows. Regenerate the compressed map through the usual make dependency.

The native test now checks row sampling/contact consistency, stronger settings,
neutral fallback for unprepared terrain, whole-shot rollback under partial
matrix exhaustion, exclusive scanline admission, different/same-parameter
ownership replacement, and interrupted VBlank publication. DMA registers,
actual scanline timing, rendering seams and clipping still need emulator tests.
Renderer limits live in `include/battle_camera.h`; current choreography lives in
`src/data/battle_camera_tracks.h`.

## Wild/trainer coverage correction

The Plain-only gate has been removed. All loaded environments and battle flags,
including wild encounters and doubles, are admitted under the existing menu/
resource safeguards. Check all active persistent maps with:

```sh
python tools/battle_camera_test/prepare_background_maps.py --check
```

Without `--check`, the tool regenerates edge guards in the actual INCBIN map
paths (including special environments using `tiles.bin`). It preserves original
upper112 pixels, tile IDs, palette banks and map size. Plain keeps its existing
specialized turf extension; other environments reflect their own ground edge.
The native fixture now covers every environment ID, wild/trainer/link/Frontier/
recorded flags, two/four battlers, and retry after an initially busy scanline
owner releases its resource. Visual platform/seam checks remain necessary.

## BW2-style idle scheduling (current)

See the parent workspace reports `bw2-idle-camera-behavior.md` (verified DS
source) and `soulgold-bw2-idle-policy.md` (GBA implementation).

`BATTLE_CAMERA_WAIT_TICKS` in `include/battle_camera.h` now defaults to900,
matching the normal BW2 initial idle threshold. Wait900 eligible menu updates;
selection can start on the next one. There is no invented90-tick repeat pause.
Once ready, completed tracks allow another nonconsecutive choice. A busy
scanline owner delays admission without restarting the elapsed wait.

Six authored GBA tracks live in `src/data/battle_camera_tracks.h`. Keys contain
explicit update duration and0..256 fractions of the existing renderer envelope.
They approximate the referenced DS scripts' choreography, not their world poses,
braking algorithm or exact runtime timing. Private cosmetic RNG does not consume
battle RNG. Menu exit/incompatible presentation cancels and rearms the initial
wait; cancellation preserves the last track so it is not immediately repeated.

The native fixture now covers the initial threshold, complete paths and exact
neutral endpoints of all six tracks, nonrepeating selection, READY after busy
ownership, next-track continuation without a new delay, restart after
interruption, affine/row/resource invariants, and all environment/type/doubles
cases. Emulator visual and performance acceptance remain separate.

## Executed-move camera extension (2026-11-07)

Moving profiles are source-admitted only for canonical Headbutt, Peck, Ember,
Splash, Withdraw and Helping Hand scripts. Growl, Tail Whip and Rain Dance use
stable neutral group/field framing. All other or replaced scripts remain neutral.
These entries have not yet been observed with the extension in an emulator.

```sh
python tools/battle_camera_test/audit_move_scripts.py
```

The compile checker now covers eight relevant C units. The native fixture tests
queued snapshot-safe lifecycle events,20 repeated actions from both sides,
resolved self/ally/spread/field/redirection classes, effect registration and
slot reuse, projectile endpoints, local affine state, hidden helpers, matrix
exhaustion, interruption/rebinding/faint/exit, watchdog and neutral restoration.
It compares mid-focus state under1x/4x logical-update grouping; it does not run
actual accelerated VM/game frames. Full idle/background checks remain in place.

Move tuning/bindings live in `src/data/battle_camera_move_profiles.h`; snapshot
capacity, watchdog and focus calibration are in `include/battle_camera.h`.
Script pointer matching is intentional: do not add a move ID without auditing
its actual animation and C callbacks. The static script checker does not replace
that C audit or emulator observation. No camera tasks or animation waits are
added; all move events commit in BeginFrame inside the existing OAM transaction.

In the parent workspace read faux3d-move-camera-implementation.md,
faux3d-move-camera-testing.md and faux3d-move-camera-compatibility.md. Rebuild a
ROM in the working tool environment, enable battle animations, and test multiple
successive executed moves. With battle scenes/headless animations disabled,
no move camera is forced over an animation the engine skips.
