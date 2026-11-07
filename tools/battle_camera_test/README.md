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

The ARM check runs CPP -> project text preprocessor -> ARM GCC for the five
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
Runtime camera tuning lives only in `include/battle_camera.h`.
