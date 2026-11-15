"""Decode BW2 HUD cell/OAM resources read-only and export layout evidence.

Run from SoulGold: python tools/battle_camera_test/audit_bw2_hud.py ../pokebw2
  --output ../reports/assets/bw2-hud
No native executable/DS runtime is inspected. PNG rendering is an analytical
resource reconstruction, not an emulator capture or a copy into GBA assets.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
from preview_backgrounds import read_png, write_png

DIMENSIONS = (((8,8),(16,16),(32,32),(64,64)),
              ((16,8),(32,8),(32,16),(64,32)),
              ((8,16),(8,32),(16,32),(32,64)))
RESOURCES = {'opponent-name': (435,436), 'player-name': (438,439),
             'hp': (447,448), 'exp': (453,454), 'hp-numbers': (456,457)}


def decode(path):
    data = path.read_bytes()
    assert data[:4] == b'RECN'
    start = data.index(b'KBEC')
    base = start+8
    count, flags, offset, mapping = struct.unpack_from('<HHII', data, base)
    assert count == 1 and flags == 0, (path, count, flags)
    cell = base+offset
    objects, attrs, rel = struct.unpack_from('<HHI', data, cell)
    oam = cell+count*8+rel
    entries = []
    for i in range(objects):
        at = oam+i*6
        a0,a1,a2 = struct.unpack_from('<HHH', data, at)
        shape,size = a0>>14,a1>>14
        assert shape < 3 and not (a0 & 0x100), 'Decoder admits ordinary nonaffine cells only'
        width,height = DIMENSIONS[shape][size]
        x,y = a1&511,a0&255
        if x >= 256:x-=512
        if y >= 128:y-=256
        entries.append({'offset':at,'attributes':[a0,a1,a2], 'x':x,'y':y,
                        'width':width,'height':height,'tile':a2&1023})
    bounds = [min(e['x'] for e in entries), min(e['y'] for e in entries),
              max(e['x']+e['width'] for e in entries), max(e['y']+e['height'] for e in entries)]
    return {'path':str(path),'sha256':hashlib.sha256(data).hexdigest(),
            'cell_offset':cell,'oam_offset':oam,'mapping':mapping,'entries':entries,'bounds':bounds}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bw2',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    evidence={}
    for name,(png,cell) in RESOURCES.items():
        directory=args.bw2/'res/graphics/btlv'
        record=decode(directory/f'{cell}.ncer')
        animation_path=directory/f'{cell+1}.nanr'
        animation=animation_path.read_bytes()
        assert animation[:4]==b'RNAN'
        bank=animation.index(b'KNBA')+8
        sequences,frames,seq_offset,frame_offset,data_offset=struct.unpack_from('<HHIII',animation,bank)
        assert sequences==1 and frames==1
        seq=bank+seq_offset
        num_frames,loop,kind,mode,frame_rel=struct.unpack_from('<HHIII',animation,seq)
        payload,duration,_=struct.unpack_from('<IHH',animation,bank+frame_offset+frame_rel)
        record['animation']={'path':str(animation_path),'sha256':hashlib.sha256(animation).hexdigest(),
                             'sequences':sequences,'frames':num_frames,'loop_start':loop,
                             'type_raw':kind,'mode_raw':mode,'duration':duration,
                             'cell_payload':animation[bank+data_offset+payload:bank+data_offset+payload+4].hex()}
        width,height,pixels,palette=read_png(directory/f'{png}.png')
        # Archive cells specify 1D mapping value1, i.e.64-byte OBJ boundaries.
        assert record['mapping']==1
        indices=[]
        for ty in range(0,height,8):
            for tx in range(0,width,8):
                indices += [pixels[ty+y][tx+x]&15 for y in range(8) for x in range(8)]
        left,top,right,bottom=record['bounds']
        image=[[(192,192,192)]*(right-left) for _ in range(bottom-top)]
        occupied=[]
        for e in record['entries']:
            for y in range(e['height']):
                for x in range(e['width']):
                    sx=e['width']-1-x if e['attributes'][1]&4096 else x
                    sy=e['height']-1-y if e['attributes'][1]&8192 else y
                    offset=e['tile']*128+((sy//8)*(e['width']//8)+sx//8)*64+(sy%8)*8+sx%8
                    assert offset < len(indices)
                    color=indices[offset]
                    if color:
                        px,py=e['x']+x,e['y']+y
                        image[py-top][px-left]=palette[color]
                        occupied.append((px,py))
        record['painted_bounds']=[min(x for x,y in occupied),min(y for x,y in occupied),
                                  max(x for x,y in occupied)+1,max(y for x,y in occupied)+1]
        record['character_path']=str(directory/f'{png}.png')
        record['character_sha256']=hashlib.sha256((directory/f'{png}.png').read_bytes()).hexdigest()
        evidence[name]=record
        write_png(args.output/f'{name}.png',image)
        print(name,'cell',record['bounds'],'painted',record['painted_bounds'])
    (args.output/'layout-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n',newline='\n')


if __name__=='__main__':main()
