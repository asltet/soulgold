"""Offline checks of copied original resources, native outlines and digit lanes."""
from pathlib import Path
import hashlib,json,re
from import_bw2 import convert,cell,bars


def check():
    directory=Path('graphics/battle_interface/bw2_original')
    for name,digest in json.loads((directory/'provenance.json').read_text()).items():
        assert hashlib.sha256((directory/name).read_bytes()).hexdigest()==digest,name
    frames,glyphs=convert(directory)
    for player,number in enumerate((435,438)):
        original=cell(directory,number)
        for y in range(32):
            expected=original[y+4] if 13<=y<(24 if player==0 else 28) else [0]*128
            assert [bool(v) for v in frames[player][y]]==[bool(v) for v in expected]
    # Verify generated engine arrays still contain those exact converted pixels.
    header=Path('src/data/bw2_hud_assets.h').read_text().split('static const u8 sBw2HudDigits')[0]
    packed=bytes(int(v,16) for v in re.findall(r'0x([0-9A-F]{2})',header))
    expected=bytes(row[x]|row[x+1]<<4 for frame in frames for row in frame for x in range(0,128,2))
    assert packed==expected
    assert all(any(v for row in glyph for v in row) for glyph in glyphs)
    assert all(not v for frame in frames for row in frame[:13] for v in row)
    hp,exp=bars(directory)
    header=Path('src/data/bw2_hud_assets.h').read_text()
    for symbol,data in [('sBw2HudHpTiles',[tile for color in hp for tile in color]),('sBw2HudExpTiles',exp)]:
        body=header.split('static const u32 '+symbol)[1].split('};')[0]
        assert [int(v,16) for v in re.findall(r'0x([0-9A-F]{8})',body)] == [word for tile in data for word in tile]
    for color in hp:
        for fill,tile in enumerate(color):
            assert sum(10 <= ((tile[2]>>(x*4))&15) <= 15 for x in range(8)) == fill
            assert tile[0]==tile[4]==0x88888888 and tile[5:]==[0,0,0]
    for fill,tile in enumerate(exp):
        assert sum(((tile[1]>>(x*4))&15) in (9,11) for x in range(8)) == fill
        assert tile[0]==tile[3]==0x11111111 and tile[4:]==[0,0,0,0]
    cases=0
    for maximum in (1,9,99,999,9999):
        for current in range(maximum+1):
            text=f'{current}/{maximum}'
            width=(len(text)-1)*7+8
            left=(128-width)//2
            assert 32<=left and left+width<=96
            for i,char in enumerate(text):
                for y,row in enumerate(glyphs[10 if char=='/' else int(char)]):
                    for x,ink in enumerate(row):
                        if ink:assert frames[1][18+y][left+i*7+x]
            cases+=1
    print(f'PASS: copied resource hashes, native player/opponent silhouettes, generated bar tiles/fill0..8, pixels and {cases} HP strings')


if __name__=='__main__':check()
