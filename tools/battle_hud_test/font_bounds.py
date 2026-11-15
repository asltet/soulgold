"""Source-font layout model, not the hardware text-printer implementation."""
from pathlib import Path
import re
import sys
sys.path.insert(0, str(Path('tools/battle_camera_test').resolve()))
from preview_backgrounds import read_png


def load():
    mapping={m[1]:int(m[2],16) for m in re.finditer(r"^'([^']+)'\s*=\s*([0-9A-F]{2})\s*$",
              Path('charmap.txt').read_text(encoding='utf-8'),re.M)}
    mapping['\x01']=0x105 # CHAR_EXTRA_SYMBOL / CHAR_LV_2 in the real header.
    source=Path('src/fonts.c').read_text()
    fonts=[]
    for symbol,file in [('Small','latin_small'),('SmallNarrow','latin_small_narrow'),('SmallNarrower','latin_small_narrower')]:
        body=re.search(rf'gFont{symbol}LatinGlyphWidths\[\]\s*=\s*\{{(.*?)\}};',source,re.S)[1]
        widths=list(map(int,re.findall(r'\b\d+\b',re.sub(r'//[^\n]*','',body))))
        _,_,pixels,_=read_png(Path('graphics/fonts')/(file+'.png'))
        points={}
        for char,index in mapping.items():
            gx,gy=index%16*16,index//16*16
            points[char]=[(x,y,pixels[gy+y][gx+x]) for y in range(13) for x in range(widths[index])
                          if pixels[gy+y][gx+x] in (1,2)]
        fonts.append((widths,points))
    return mapping,fonts


def measure(mapping,font,text):
    return sum(font[0][mapping[c]] for c in text)


def fit(mapping,fonts,text,limit):
    return next((font for font in fonts if measure(mapping,font,text)<=limit),fonts[-1])


def ink(mapping,font,text,x,y):
    for char in text:
        for px,py,color in font[1][char]:yield x+px,y+py,color
        x+=font[0][mapping[char]]


def check():
    mapping,fonts=load()
    h=Path('include/battle_hud_layout.h').read_text()
    c={m[1]:int(m[2]) for m in re.finditer(r'#define\s+(COMPACT_HUD_\w+)\s+(\d+)\b',h)}
    for side in (0,8):
        for word in ['CHARIZARD','gypqjgypsy','WWWWWWWWWW','iiiiiiiiii']:
            font=fit(mapping,fonts,word,c['COMPACT_HUD_NAME_WIDTH'])
            width=measure(mapping,font,word)
            assert width<=c['COMPACT_HUD_NAME_WIDTH']
            left=(10 if width>48 else 16)+side
            for x,y,_ in ink(mapping,font,word,left,c['COMPACT_HUD_NAME_Y']):assert x<72+side and y<16
        for word in ['\u2642','\u2640']:
            assert measure(mapping,fonts[0],word)<=c['COMPACT_HUD_GENDER_WIDTH']
            for x,y,_ in ink(mapping,fonts[0],word,72+side,c['COMPACT_HUD_NAME_Y']):assert 72+side<=x<80+side and y<16
        for level in (1,9,10,99,100,255):
            text=str(level)
            assert measure(mapping,fonts[0],text)<=24
            for x,y,_ in ink(mapping,fonts[0],text,88+side,c['COMPACT_HUD_LEVEL_Y']):assert 88+side<=x<112+side and y<16
    print('PASS: floating name, separate gender and level source glyphs fit their lanes')


if __name__=='__main__':check()
