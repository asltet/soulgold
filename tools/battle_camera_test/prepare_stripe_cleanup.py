"""Prepare exact, guarded tile-word substitutions for five striped backdrops.

Run from repository root. Does not edit artwork, maps or palettes. Emits C data
for a load-time graphics filter and analytical previews of its output.
--check verifies the checked-in C table against the source PNG tile indices.
"""
from pathlib import Path
import argparse
import struct
from collections import deque
from preview_backgrounds import read_png, render, write_png

ASSETS = {
    "Water": ("water", 0, {1, 2, 3, 4, 5}),
    "Building": ("building", 1, {2, 3, 4, 5, 6}),
    "Stadium": ("stadium", 2, {4, 5, 7, 11, 12}),
    "Sky": ("sky", 3, {4, 6, 7, 10, 12}),
    "Snow": ("snow", 1, {3, 7, 10, 12}),
}


def prepare(directory, stripe, ground):
    width, height, original, palette = read_png(directory / "tiles.png")
    pixels = [row[:] for row in original]
    # Classify in assembled scene space: tile-local vertical runs miss stripe
    # ends where the next row is a curved platform outline. Flood only from
    # the unobstructed top edge; side/bottom seeds could enter clipped platforms.
    scene_width, scene_height = 512, 112
    entries = struct.unpack('<2048H', (directory/'map.bin').read_bytes())
    coordinates, scene = [], []
    for y in range(scene_height):
        for x in range(scene_width):
            tx, ty = x//8, y//8
            entry = entries[(tx//32)*1024 + ty*32 + tx % 32]
            tile = entry & 1023
            px, py = x % 8, y % 8
            if entry & 1024: px = 7-px
            if entry & 2048: py = 7-py
            sx, sy = (tile % (width//8))*8+px, (tile//(width//8))*8+py
            coordinates.append((sx, sy))
            scene.append(original[sy][sx])
    allowed = ground | {stripe}
    reachable = set(i for i in range(scene_width) if scene[i] in allowed)
    queue = deque(reachable)
    while queue:
        i = queue.popleft()
        x, y = i % scene_width, i // scene_width
        for nx, ny in ((x-1,y), (x+1,y), (x,y-1), (x,y+1)):
            ni = ny*scene_width+nx
            if 0 <= nx < scene_width and 0 <= ny < scene_height and ni not in reachable and scene[ni] in allowed:
                reachable.add(ni)
                queue.append(ni)
    protected = {coordinates[i] for i in range(len(scene)) if i not in reachable}
    replacements = {}
    for i in sorted(reachable):
        sx, sy = coordinates[i]
        if scene[i] != stripe or sy % 8 not in (0, 4):
            continue
        assert (sx, sy) not in protected, ('Tile reused inside platform', directory, sx, sy)
        x, y = i % scene_width, i // scene_width
        candidates = []
        for radius in range(1, 17):
            for dx in range(-radius, radius+1):
                dy = radius-abs(dx)
                for ny in {y-dy, y+dy}:
                    nx = x+dx
                    ni = ny*scene_width+nx
                    if 0 <= nx < scene_width and 0 <= ny < scene_height and ni in reachable and scene[ni] in ground:
                        candidates.append(scene[ni])
            if candidates:
                break
        assert candidates, ('No adjacent background tone', directory, x, y)
        # A nearest connected background tone extends to the outside of the
        # silhouette even where the adjacent vertical pixel is outline/art.
        color = max(set(candidates), key=lambda c: (candidates.count(c), -c))
        if (sx, sy) in replacements:
            assert replacements[sx, sy] == color, ('Inconsistent tile reuse', directory, sx, sy)
        replacements[sx, sy] = color
    for (x, y), color in replacements.items():
        pixels[y][x] = color
    assert all(pixels[y][x] == original[y][x] for x, y in protected)
    assert all(pixels[coordinates[i][1]][coordinates[i][0]] != stripe
               for i in reachable if scene[i] == stripe and coordinates[i][1] % 8 in (0, 4))
    def pack(rows):
        data = bytearray()
        for ty in range(0, height, 8):
            for tx in range(0, width, 8):
                for y in range(8):
                    for x in range(0, 8, 2):
                        data.append((rows[ty+y][tx+x] & 15) | ((rows[ty+y][tx+x+1] & 15) << 4))
        return bytes(data)
    before, after = pack(original), pack(pixels)
    assert len(before) == len(after)
    changes = [(at//2, struct.unpack_from('<H', before, at)[0], struct.unpack_from('<H', after, at)[0])
               for at in range(0, len(before), 2) if before[at:at+2] != after[at:at+2]]
    # Model the guarded VRAM writes and prove they exactly match the preview.
    simulated = bytearray(before)
    for offset, expected, replacement in changes:
        assert struct.unpack_from('<H', simulated, offset*2)[0] == expected
        struct.pack_into('<H', simulated, offset*2, replacement)
    assert simulated == after
    return changes, (width, height, pixels, palette)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--preview', type=Path)
    args = parser.parse_args()
    text = ('// Generated by tools/battle_camera_test/prepare_stripe_cleanup.py.\n'
            '// Offsets are 16-bit words in the decompressed 4bpp tileset.\n'
            'struct BattleBackgroundStripePatch\n{\n    u16 offset, expected, replacement;\n};\n\n')
    for name, (folder, stripe, colors) in ASSETS.items():
        directory = Path('graphics/battle_environment') / folder
        changes, tiles = prepare(directory, stripe, colors)
        text += f'static const struct BattleBackgroundStripePatch sStripePatches{name}[] = {{\n'
        text += ''.join(f'    {{{offset}, 0x{old:04X}, 0x{new:04X}}},\n' for offset, old, new in changes) + '};\n\n'
        if args.preview:
            write_png(args.preview / f'{name}.png', render(directory, directory/'map.bin', tile_source=tiles))
        print(f'PASS: {name}: {len(changes)} guarded tile-word changes; maps/palettes/tile count unchanged')
    target = Path('src/data/battle_background_stripes.h')
    if args.check:
        assert target.read_text() == text, 'Stripe table does not match source tiles'
    else:
        target.write_text(text, newline='\n')


if __name__ == '__main__':
    main()
