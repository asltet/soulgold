"""Prepare camera guards for all actually declared persistent backgrounds.
Run from SoulGold root, --check for verification only. No new tiles/palettes.
Plain retains its separately prepared/art-directed guards.
"""
from pathlib import Path
import re
import struct
import sys
def index(x, y):
    return (x // 32) * 1024 + y * 32 + x % 32

import subprocess
subprocess.run([sys.executable, "tools/battle_camera_test/prepare_plain_map.py", *(["--check"] if "--check" in sys.argv else [])], check=True)

source = Path("src/data/graphics/battle_environment.h").read_text()
paths = re.findall(r'gBattleEnvironmentTilemap_\w+\[\].*?INCBIN_U32\("([^"\n]+)\.smolTM"', source)
for name in dict.fromkeys(paths):
    path = Path(name)
    original = path.read_bytes()
    assert len(original) == 4096, name
    entries = list(struct.unpack("<2048H", original))
    if path.parent.name == "plain":
        continue
    for x in range(64):
        for y in range(14):
            assert entries[index(x,y)] >> 12 == 2, name
        # Reflect the existing lower edge and repeat only the ground-edge strip.
        for y in range(14,30):
            phase = (y-14) % 4
            entries[index(x,y)] = entries[index(x,(13,12,12,13)[phase])] ^ (0x800 if phase < 2 else 0)
        entries[index(x,30)] = entries[index(x,1)] ^ 0x800
        entries[index(x,31)] = entries[index(x,0)] ^ 0x800
    result = struct.pack("<2048H", *entries)
    assert all(result[b*2048:b*2048+896] == original[b*2048:b*2048+896] for b in range(2))
    if "--check" in sys.argv:
        assert result == original, name
    else:
        path.write_bytes(result)
    assert all(e >> 12 == 2 for e in entries), name
print(f"PASS: guards for all {len(set(paths))} active persistent background maps")
