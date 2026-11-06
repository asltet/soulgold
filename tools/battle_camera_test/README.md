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

The ARM check runs CPP -> project text preprocessor -> ARM GCC for the three
touched translation units. It permits the existing unused-const warning in
battle_main.c, matching the project's default unused-warning policy.
The host harness includes the real camera implementation and project headers;
it removes only the ARM target attribute and stubs hardware-facing sprite
helpers. `-w` suppresses host/header portability warnings, not compile errors.
It checks state restoration and arithmetic, not GBA ABI, GPU output, timing,
ROM linkage, animation compatibility, or visual quality. Run the emulator
checklist in `reports/faux3d-prototype-testing.md` in the parent workspace.
