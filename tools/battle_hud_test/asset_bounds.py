"""Offline checks of copied original resources, native outlines and digit lanes."""
from pathlib import Path
import hashlib,json,re
from import_bw2 import convert,cell,bars,read_png


def check():
    for name in ('enemy_mon_shadow','enemy_mon_shadows_sized'):
        _,_,pixels,_=read_png(Path('graphics/battle_interface')/(name+'.png'))
        assert {v for row in pixels for v in row if v} == {1}
    directory=Path('graphics/battle_interface/bw2_original')
    for name,digest in json.loads((directory/'provenance.json').read_text()).items():
        assert hashlib.sha256((directory/name).read_bytes()).hexdigest()==digest,name
    frames,glyphs=convert(directory)
    for player,number in enumerate((435,438)):
        original=cell(directory,number)
        for y in range(32):
            expected=original[y]
            assert [bool(v) for v in frames[player][y]]==[bool(v) for v in expected]
    assert sum(v in (9,11) for row in frames[1][25:30] for v in row[18:40]) > 0
    assert any(v in (7,8) for row in frames[1][9:16] for v in row[80:88])
    for player,number in enumerate((435,438)):
        original=cell(directory,number)
        hp_left=40 if player else 32
        lv_left=80 if player else 88
        assert any(frames[player][y][x] == 5 for y in range(16,23) for x in range(hp_left-1,hp_left+16))
        for y in range(16,23):
            for x in range(hp_left-1,hp_left+16):
                if original[y][x] == 2:assert frames[player][y][x] == 5
                if original[y][x] in (1,14):assert frames[player][y][x] == 8
        for y in range(8,17):
            for x in range(lv_left,lv_left+8):
                if original[y][x] == 2:assert frames[player][y][x] == 1
    # Verify generated engine arrays still contain those exact converted pixels.
    header=Path('src/data/bw2_hud_assets.h').read_text().split('static const u8 sBw2HudDigits')[0]
    packed=bytes(int(v,16) for v in re.findall(r'0x([0-9A-F]{2})',header))
    expected=bytes(row[x]|row[x+1]<<4 for frame in frames for row in frame for x in range(0,128,2))
    assert packed==expected
    assert all(any(v for row in glyph for v in row) for glyph in glyphs)
    assert all(not v for frame in frames for row in frame[:9] for v in row)
    hp,exp=bars(directory)
    header=Path('src/data/bw2_hud_assets.h').read_text()
    for symbol,data in [('sBw2HudHpTiles',[tile for color in hp for tile in color]),('sBw2HudExpTiles',exp)]:
        body=header.split('static const u32 '+symbol)[1].split('};')[0]
        assert [int(v,16) for v in re.findall(r'0x([0-9A-F]{8})',body)] == [word for tile in data for word in tile]
    for color in hp:
        for fill,tile in enumerate(color):
            assert sum(10 <= ((tile[5]>>(x*4))&15) <= 15 for x in range(8)) == fill
            assert tile[3]==tile[7]==0x88888888 and tile[:3]==[0,0,0]
    for fill,tile in enumerate(exp):
        assert sum(((tile[5]>>(x*4))&15) in (9,11) for x in range(8)) == fill
        assert tile[4]==tile[7]==0x11111111 and tile[:4]==[0,0,0,0]
    cases=0
    for maximum in (1,9,99,999,9999):
        for current in range(maximum+1):
            text=f'{current}/{maximum}'
            current_count=len(str(current));max_count=len(str(maximum))
            for i,char in enumerate(text):
                if i<current_count:left=80-current_count*(7 if current_count>3 else 8)+i*(7 if current_count>3 else 8)
                elif i==current_count:left=80
                else:left=88+(i-current_count-1)*(7 if max_count>3 else 8)
                assert 48<=left and left+8<=120
                for y,row in enumerate(glyphs[10 if char=='/' else int(char)]):
                    for x,ink in enumerate(row):
                        if ink:assert frames[1][22+y][left+x]
            cases+=1
    print(f'PASS: copied resource hashes, native player/opponent silhouettes, generated bar tiles/fill0..8, pixels and {cases} HP strings')


if __name__=='__main__':check()
