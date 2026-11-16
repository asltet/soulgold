"""Analytical layout preview from source fonts/art; not an emulator capture.

Represents ordinary light-theme singles, sample labels/values, no status/caught
or gimmick icon. Actual engine control codes/palette options are not emulated.
"""
from pathlib import Path
import argparse
import re
import sys
import struct
sys.path.insert(0, str(Path('tools/battle_camera_test').resolve()))
from preview_backgrounds import read_png, write_png, render
from font_bounds import load, fit, measure, ink
from import_bw2 import convert,bars
frames,digit_ink=convert(Path("graphics/battle_interface/bw2_original"))
hp_tiles,exp_tiles=bars(Path("graphics/battle_interface/bw2_original"))

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
mapping,fonts=load()
reference=False
native_frames=Path('build/reference-hud-frames.bin').read_bytes()
native_palette=struct.unpack('<12H',Path('build/reference-hud-palette.bin').read_bytes())
reference_palette=[tuple(((word>>shift)&31)*255//31 for shift in (0,5,10)) for word in native_palette]
constants={m[1]:int(m[2]) for m in re.finditer(r'#define\s+(COMPACT_HUD_\w+)\s+(\d+)\b',
            Path('include/battle_hud_layout.h').read_text())}


def card(player,compact,doubles=False):
    name='healthbox_singles_player' if player else 'healthbox_singles_opponent'
    w,h,p,pal=read_png(Path('graphics/battle_interface')/(name+'.png'))
    height=(36 if player and reference and not doubles else constants['COMPACT_HUD_PLAYER_HEIGHT' if player else 'COMPACT_HUD_OTHER_HEIGHT']) if compact else h
    width=constants['COMPACT_HUD_WIDTH'] if compact else w
    if compact:
        pal[5]=(0,0,0)
        pal[8]=tuple(map(int,Path("graphics/battle_interface/healthbar.pal").read_text().splitlines()[6].split()))
        if reference:pal[:12]=reference_palette
    image=[[None]*width for _ in range(height)]
    if compact:
        scene=2 if player and doubles else 1 if player else 0
        source_frames=([list(native_frames[(scene*40+y)*128:(scene*40+y+1)*128]) for y in range(40)] if reference else frames[player])
        for y,row in enumerate(source_frames[:height]):
            for x,value in enumerate(row):
                if value:image[y][x]=pal[value]
    else:
        for y in range(h):
            for x in range(w):
                if p[y][x]:image[y][x]=pal[p[y][x]]
    def label(text,x,y,limit,header=False):
        font=fit(mapping,fonts,text,limit)
        for px,py,color in ink(mapping,font,text,x,y):
            if 0<=px<width and 0<=py<height:image[py][px]=pal[(2 if color==1 else (5 if reference else 1)) if header else (1 if color==1 else 3)]
    nickname='CHARIZARD' if player else 'GENGAR'
    side=0 if player else 8
    if compact:
        font=fit(mapping,fonts,nickname,constants['COMPACT_HUD_NAME_WIDTH'])
        nx=(10 if measure(mapping,font,nickname)>48 else 16)+side
        label(nickname,nx,constants['COMPACT_HUD_NAME_Y'],constants['COMPACT_HUD_NAME_WIDTH'],header=True)
        label('\u2642',72+side,constants['COMPACT_HUD_NAME_Y'],8,header=True)
        label('100',88+side,constants['COMPACT_HUD_LEVEL_Y'],24,header=True)
    else:
        label(nickname,16 if player else 8,3,55)
        level='\x01'+'100'
        label(level,(96 if player else 88)-measure(mapping,fonts[0],level),3,24)
    _,_,bar,bpal=read_png(Path('graphics/battle_interface/hpbar.png'))
    slots=[1,2]+[3+max(0,min(8,31-i*8)) for i in range(6)]
    bx=constants['COMPACT_HUD_BAR_X']-side if compact else 32 if player else 24
    for i,tile in enumerate(slots):
        if compact and i<2:continue
        rows=([1,2,3,4,5] if i<2 else [1,3,4,5,7]) if compact else range(8)
        for y,sy in enumerate(rows):
            for x in range(8):
                color=((hp_tiles[0][max(0,min(8,31-(i-2)*8))][y+3]>>(4*x))&15) if compact and i>=2 else bar[sy][tile*8+x]
                if color and bx+i*8+x<width:image[(constants['COMPACT_HUD_BAR_Y'] if compact else 16)+y][bx+i*8+x]=bpal[color]
    if player:
        if compact:
            text='63/100'
            for i,c in enumerate(text):
                left=64+i*8
                for y,row in enumerate(digit_ink[10 if c=='/' else int(c)]):
                    for x,value in enumerate(row):
                        if value:image[constants['COMPACT_HUD_HP_TEXT_Y']+y][left+x]=pal[(2 if reference else 8) if value==1 else 5]
        else:
            text='  63/100'
            font=fit(mapping,fonts,text,56)
            label(text,96-measure(mapping,font,text),21,56)
        if reference and compact and not doubles:
            filled=(37*80+32)//64
            for y in (33,34):
                for x in range(80):image[y][40+x]=pal[9 if x<filled else 2]
        elif not doubles:
            _,_,exp,epal=read_png(Path('graphics/battle_interface/expbar.png'))
            for i in range(8):
                fill=max(0,min(8,37-i*8))
                rows=(2,3,4,5) if compact else range(8)
                for y,sy in enumerate(rows):
                    for x in range(8):
                        color=((exp_tiles[fill][y+4]>>(4*x))&15) if compact else exp[sy][fill*8+x]
                        if color:image[(28 if compact else 32)+y][(40 if compact else 32)+i*8+x]=epal[color]
    if compact:
        for x in (55-side,104-side):
            for y in range(17,22):image[y][x]=pal[5 if reference else 1]
        if player and not reference and not doubles:
            for x in (39,104):
                for y in range(28,32):image[y][x]=pal[1]
    return image


images={}
for mode in ('original','legacy','reference'):
    reference=mode=='reference'
    compact=mode!='original'
    base=Path('graphics/battle_environment/plain')
    frame=render(base,base/'map.bin',width=240,height=112)+[[(32,40,48)]*240 for _ in range(48)]
    for player in (False,True):
        cx,cy=((144,92) if reference else (144,96)) if compact and player else ((32,20) if reference else (34,20)) if compact else (168,88) if player else (34,30)
        panel=card(player,compact)
        for y,row in enumerate(panel):
            for x,color in enumerate(row):
                px,py=cx-32+x,cy-16+y
                if color is not None and 0<=px<240 and 0<=py<160:frame[py][px]=color
    write_png(args.output/(mode+'.png'),frame)
    images[mode]=frame
write_png(args.output/'comparison.png',[a+b for a,b in zip(images['legacy'],images['reference'])])
reference=True
base=Path('graphics/battle_environment/plain')
frame=render(base,base/'map.bin',width=240,height=112)+[[(32,40,48)]*240 for _ in range(48)]
for player in (False,True):
    for slot in (0,1):
        cx,cy=(144,64+slot*32) if player else (32,20+slot*28)
        panel=card(player,True,True)
        for y,row in enumerate(panel):
            for x,color in enumerate(row):
                px,py=cx-32+x,cy-16+y
                if color is not None and 0<=px<240 and 0<=py<160:frame[py][px]=color
write_png(args.output/'reference-doubles.png',frame)
