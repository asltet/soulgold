"""Native checks using the real compact HUD helpers and source EXP graphics.

Run from SoulGold root. Requires a native GCC-compatible compiler; pass --cc
when it is not on PATH. Does not build a ROM or observe hardware rendering.
"""
from pathlib import Path
import argparse
import shutil
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc', default=shutil.which('gcc'))
args = parser.parse_args()
if not args.cc:
    parser.error('Supply --cc with the native GCC executable')
sys.path.insert(0, str(Path('tools/battle_camera_test').resolve()))
from preview_backgrounds import read_png

Path('build').mkdir(exist_ok=True)
executable=Path('build/compact-hud-host-test.exe' if sys.platform=='win32' else 'build/compact-hud-host-test')
subprocess.run([args.cc,'tools/battle_hud_test/test.c','-iquote','include','-std=c99',
                '-Wall','-Wextra','-Werror','-O2','-o',str(executable)],check=True)
subprocess.run([str(executable.resolve())],check=True)
from font_bounds import check
check()

from asset_bounds import check as check_assets
check_assets()
