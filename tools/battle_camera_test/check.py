"""Compile touched ARM units through the project's text preprocessor.
Run from the SoulGold root: python tools/battle_camera_test/check.py
Requires generated region_map_sections.h and built tools/preproc/preproc.
This is a compile check, not a replacement for make or a ROM test.
"""
import os
from pathlib import Path
import shutil
import subprocess

suffix = ".exe" if os.name == "nt" else ""
gcc = shutil.which("arm-none-eabi-gcc")
if not gcc and os.environ.get("DEVKITARM"):
    gcc = str(Path(os.environ["DEVKITARM"]) / "bin" / ("arm-none-eabi-gcc" + suffix))
if not gcc:
    raise SystemExit("Put arm-none-eabi-gcc on PATH or set DEVKITARM")
flags = ["-mthumb", "-mthumb-interwork", "-O2", "-mabi=apcs-gnu", "-mtune=arm7tdmi",
         "-march=armv4t", "-std=gnu17", "-Werror", "-Wall", "-Wno-pointer-to-int-cast",
         "-Wno-strict-aliasing", "-Wno-attribute-alias", "-Woverride-init", "-Wnonnull",
         "-Wenum-conversion", "-Wno-error=unused-const-variable"]
Path("build").mkdir(exist_ok=True)
for name in ("battle_camera", "battle_main", "reshow_battle_screen", "battle_bg", "scanline_effect", "battle_anim", "battle_controllers", "sprite"):
    src = f"src/{name}.c"
    cpp = subprocess.run([gcc, "-E", "-iquote", "include", "-DMODERN=1", "-DTESTING=0",
                          "-DEMERALD", "-std=gnu17", src], capture_output=True, check=True)
    pre = subprocess.run(["tools/preproc/preproc" + suffix, "-i", src, "charmap.txt"],
                         input=cpp.stdout, capture_output=True, check=True)
    output = Path("build") / f"{name}-prototype.i"
    output.write_bytes(pre.stdout)
    subprocess.run([gcc, "-c", "-fpreprocessed", "-x", "c", str(output), "-o",
                    f"build/{name}-prototype.o", *flags], check=True)
    print(f"PASS: ARM compile {src}")
