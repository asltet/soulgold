"""Render indexed PNG tiles + GBA text maps for read-only scenery inspection.

Run from repository root; output is an analytical preview, not an emulator capture.
Uses only Python's standard library. No source assets are modified.
"""
from pathlib import Path
import argparse
import re
import struct
import zlib


def read_png(path):
    data = path.read_bytes()
    pos, packed, palette = 8, b"", []
    while pos < len(data):
        length = struct.unpack_from(">I", data, pos)[0]
        tag, chunk = data[pos+4:pos+8], data[pos+8:pos+8+length]
        if tag == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
            assert color == 3 and depth in (4, 8) and interlace == 0, path
        elif tag == b"PLTE":
            palette = list(zip(chunk[0::3], chunk[1::3], chunk[2::3]))
        elif tag == b"IDAT":
            packed += chunk
        pos += length + 12
    raw = zlib.decompress(packed)
    stride = (width * depth + 7) // 8
    previous = [0] * stride
    pixels = []
    for y in range(height):
        at = y * (stride + 1)
        mode, row = raw[at], list(raw[at+1:at+1+stride])
        for x in range(stride):
            a, b, c = row[x-1] if x else 0, previous[x], previous[x-1] if x else 0
            p = a+b-c
            pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
            pred = a if pa <= pb and pa <= pc else b if pb <= pc else c
            row[x] = (row[x] + (0, a, b, (a+b)//2, pred)[mode]) & 255
        previous = row
        pixels.append(row if depth == 8 else [v for byte in row for v in (byte >> 4, byte & 15)][:width])
    return width, height, pixels, palette


def write_png(path, rows):
    height, width = len(rows), len(rows[0])
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag+data))
    raw = b"".join(b"\0" + bytes(v for rgb in row for v in rgb) for row in rows)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def render(directory, map_path, width=256, height=112, tile_source=None):
    tw, th, pixels, palette = tile_source or read_png(directory / "tiles.png")
    entries = struct.unpack("<2048H", map_path.read_bytes())
    result = []
    for y in range(height):
        row = []
        for x in range(width):
            tx, ty = x//8, (y % 256)//8
            entry = entries[(tx//32)*1024 + ty*32 + tx % 32]
            tile = entry & 1023
            px, py = x % 8, y % 8
            if entry & 1024: px = 7-px
            if entry & 2048: py = 7-py
            sx, sy = (tile % (tw//8))*8+px, (tile//(tw//8))*8+py
            assert sy < th, (directory, tile)
            color = pixels[sy][sx] & 15
            row.append(palette[color])
        result.append(row)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = Path("src/data/graphics/battle_environment.h").read_text()
    assets = re.findall(r'gBattleEnvironmentTilemap_(\w+)\[\].*?"([^"\n]+)\.smolTM"', source)
    previews = []
    for index, (name, path) in enumerate(assets):
        path = Path(path)
        rows = render(path.parent, path)
        write_png(args.output / f"{index:02d}-{name}.png", rows)
        previews.append(rows)
        print(index, name, path)
    sheet = [[(32, 32, 32)]*(256*3) for _ in range(((len(previews)+2)//3)*120)]
    for index, rows in enumerate(previews):
        for y, row in enumerate(rows):
            sheet[(index//3)*120+y][(index%3)*256:(index%3+1)*256] = row
    write_png(args.output / "contact-sheet.png", sheet)


if __name__ == "__main__":
    main()
