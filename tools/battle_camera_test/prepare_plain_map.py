"""Extend Plain's text map with existing illustrated tiles; no new tiles/palette.
Run from the SoulGold root. Original scene rows0..13 are preserved exactly.
The operation is idempotent. --check verifies without modifying the map.
"""
from pathlib import Path
import struct
import sys

path = Path("graphics/battle_environment/plain/map.bin")
original = path.read_bytes()
assert len(original) == 4096
entries = list(struct.unpack("<2048H", original))

def index(x, y):
    return (x // 32) * 1024 + y * 32 + x % 32

for x in range(64):
    for y in range(14):
        assert entries[index(x, y)] >> 12 == 2, "Unexpected source art palette bank"
    # Continue with textured central grass. Alternate mirrored two-row strips,
    # avoiding repeated trees/rocks. This lower guard is normally behind UI.
    for y in range(14, 30):
        phase = (y - 14) % 4
        source_y = (13, 12, 12, 13)[phase]
        source_x = x if y < 16 else (x // 32) * 32 + 4 + x % 16
        # The first16 lower guard pixels reflect the complete original edge,
        # including the cut-off corner rock; deeper UI-covered rows use turf.
        entry = entries[index(source_x, source_y)]
        entries[index(x, y)] = entry ^ (0x800 if phase < 2 else 0)
    # Negative Y wraps into rows30/31 deliberately, with edge-continuous art.
    entries[index(x, 30)] = entries[index(x, 1)] ^ 0x800
    entries[index(x, 31)] = entries[index(x, 0)] ^ 0x800

result = struct.pack("<2048H", *entries)
if "--check" in sys.argv:
    assert result == original, "Plain map guards are not prepared"
else:
    path.write_bytes(result)
for x in range(64):
    for y in list(range(22)) + [30, 31]:
        assert entries[index(x, y)] >> 12 == 2
assert max(e & 1023 for e in entries) < 512
print("PASS: Plain original112 rows + illustrated lower/negative-Y guards, 4096 bytes, no new tiles")
