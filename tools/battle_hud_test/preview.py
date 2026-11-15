"""Analytical layout preview from source fonts/art; not an emulator capture.

Represents ordinary light-theme singles, sample labels/values, no status/caught
or gimmick icon. Actual engine control codes/palette options are not emulated.
"""
from pathlib import Path
import argparse
import re
import sys
sys.path.insert(0, str(Path('tools/battle_camera_test').resolve()))
from preview_backgrounds import read_png, write_png, render
from font_bounds import load, fit, measure, ink

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
mapping,fonts=load()
constants={m[1]:int(m[2]) for m in re.finditer(r'#define\s+(COMPACT_HUD_\w+)\s+(\d+)\b',
            Path('include/battle_hud_layout.h').read_text())}


def card(player,compact):
    name='healthbox_singles_player' if player else 'healthbox_singles_opponent'
    w,h,p,pal=read_png(Path('graphics/battle_interface')/(name+'.png'))
    height=constants['COMPACT_HUD_PLAYER_HEIGHT' if player else 'COMPACT_HUD_OTHER_HEIGHT'] if compact else h
    width=constants['COMPACT_HUD_WIDTH'] if compact else w
    image=[[None]*width for _ in range(height)]
    if compact:
        for y in range(constants['COMPACT_HUD_PLATE_TOP'],height):
            left=20-(y-constants['COMPACT_HUD_PLATE_TOP'])//3
            for x in range(left,width-left):
                image[y][x]=pal[4 if y in (constants['COMPACT_HUD_PLATE_TOP'],height-1) or x in (left,width-left-1) else 2]
    else:
        for y in range(h):
            for x in range(w):
                if p[y][x]:image[y][x]=pal[p[y][x]]
    def label(text,x,y,limit,header=False):
        font=fit(mapping,fonts,text,limit)
        for px,py,color in ink(mapping,font,text,x,y):
            if 0<=px<width and 0<=py<height:image[py][px]=pal[(2 if color==1 else 1) if header else (1 if color==1 else 3)]
    nickname='CHARIZARD' if player else 'GENGAR'
    label(nickname,constants['COMPACT_HUD_NAME_X'] if compact else 16 if player else 8,0 if compact else 3,
          constants['COMPACT_HUD_NAME_WIDTH'] if compact else 55,header=compact)
    if compact:label('\u2642',constants['COMPACT_HUD_GENDER_X'],0,8,header=True)
    level='\x01'+'100'
    right=constants['COMPACT_HUD_LEVEL_RIGHT'] if compact else 96 if player else 88
    label(level,right-measure(mapping,fonts[0],level),constants['COMPACT_HUD_LEVEL_Y'] if compact else 3,24,header=compact)
    _,_,bar,bpal=read_png(Path('graphics/battle_interface/hpbar.png'))
    slots=[1,2]+[3+max(0,min(8,31-i*8)) for i in range(6)]
    bx=constants['COMPACT_HUD_BAR_X'] if compact else 32 if player else 24
    for i,tile in enumerate(slots):
        for y in range(8):
            for x in range(8):
                color=bar[y][tile*8+x]
                if color and bx+i*8+x<width:image[(constants['COMPACT_HUD_BAR_Y'] if compact else 16)+y][bx+i*8+x]=bpal[color]
    if player:
        text='63/100' if compact else '  63/100'
        limit=constants['COMPACT_HUD_HP_TEXT_WIDTH'] if compact else 56
        font=fit(mapping,fonts,text,limit)
        tx=(width-measure(mapping,font,text))//2 if compact else 96-measure(mapping,font,text)
        label(text,tx,constants['COMPACT_HUD_HP_TEXT_Y'] if compact else 21,limit)
        _,_,exp,epal=read_png(Path('graphics/battle_interface/expbar.png'))
        for i in range(8):
            fill=max(0,min(8,37-i*8))
            rows=(2,3,4,5) if compact else range(8)
            for y,sy in enumerate(rows):
                for x in range(8):
                    color=exp[sy][fill*8+x]
                    if color:image[32+y][(24 if compact else 32)+i*8+x]=epal[color]
    return image


images=[]
for compact in (False,True):
    base=Path('graphics/battle_environment/plain')
    image=render(base,base/'map.bin',width=240,height=112)+[[(32,40,48)]*240 for _ in range(48)]
    for player in (False,True):
        cx,cy=(160,92) if compact and player else (34,20) if compact else (168,88) if player else (34,30)
        panel=card(player,compact)
        for y,row in enumerate(panel):
            for x,color in enumerate(row):
                px,py=cx-32+x,cy-16+y
                if color is not None and 0<=px<240 and 0<=py<160:image[py][px]=color
    write_png(args.output/('compact.png' if compact else 'original.png'),image)
    images.append(image)
write_png(args.output/'comparison.png',[a+b for a,b in zip(*images)])
