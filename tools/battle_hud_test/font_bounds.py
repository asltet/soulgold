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
    cases=0
    for maximum in (1,9,99,999,9999):
        for current in range(maximum+1):
            text=f'{current}/{maximum}'
            font=fit(mapping,fonts,text,c['COMPACT_HUD_HP_TEXT_WIDTH'])
            width=measure(mapping,font,text)
            assert width<=c['COMPACT_HUD_HP_TEXT_WIDTH']
            for x,y,_ in ink(mapping,font,text,(c['COMPACT_HUD_WIDTH']-width)//2,c['COMPACT_HUD_HP_TEXT_Y']):
                assert 32<=x<80 and 21<=y<30, (text,x,y)
            cases+=1
    for word in ['CHARIZARD','gypqjgypsy','WWWWWWWWWW','iiiiiiiiii']:
        font=fit(mapping,fonts,word,c['COMPACT_HUD_NAME_WIDTH'])
        assert measure(mapping,font,word)<=c['COMPACT_HUD_NAME_WIDTH']
        for x,y,_ in ink(mapping,font,word,c['COMPACT_HUD_NAME_X'],0):assert x<68 and y<13
    for word in ['\u2642','\u2640']:
        assert measure(mapping,fonts[0],word)<=c['COMPACT_HUD_GENDER_WIDTH']
        for x,y,_ in ink(mapping,fonts[0],word,c['COMPACT_HUD_GENDER_X'],0):assert 76<=x<84 and y<13
    for level in (1,9,10,99,100,255):
        text='\x01'+str(level)
        width=measure(mapping,fonts[0],text)
        assert width<=24
        for x,y,_ in ink(mapping,fonts[0],text,c['COMPACT_HUD_LEVEL_RIGHT']-width,c['COMPACT_HUD_LEVEL_Y']):
            assert x>=84 and y<13
    print(f'PASS: {cases} HP strings and long-name/gender source glyphs fit without EXP/header collision')


if __name__=='__main__':check()
