"""Source inventory/guide generator. Run from SoulGold with BW2 source path.
Lexical first-preset classification is guidance, not branch/runtime validation.
Writes generated target header inside SoulGold and a supplied evidence JSON.
"""
from pathlib import Path
import argparse,json,re,hashlib
from collections import Counter
p=argparse.ArgumentParser()
p.add_argument("bw2",type=Path)
p.add_argument("--evidence",type=Path,required=True)
p.add_argument("--check",action="store_true")
a=p.parse_args()
known=set(re.findall(r"\bMOVE_[A-Z0-9_]+\b",Path("include/constants/moves.h").read_text()))
records=[]; rows=[]; counts=Counter()
for path in sorted((a.bw2/"res/battle/animations/moves").glob("*.s")):
 text=path.read_text(); move=re.search(r"// (MOVE_[A-Z0-9_]+)",text)
 commands=[]
 for n,line in enumerate(text.splitlines(),1):
  if re.search(r"\b(Camera|WaitFrames|Wait EFFVM)",line):commands.append({"line":n,"text":line.strip()})
 presets=[list(map(int,m.groups())) for m in re.finditer(r"CameraMovePreset\s+(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)",text)]
 first=presets[0][1] if presets else None; counts[str(first)]+=1
 ids=[c[1] for c in presets]
 if first==11:style="BW2_FRAME_TARGET"
 elif first in (9,20) and 11 in ids[1:]:style="BW2_FRAME_TRAVEL"
 elif first==9:style="BW2_FRAME_USER"
 elif first==14:style="BW2_FRAME_GROUP"
 elif first==8 and set(ids)=={8}:style="BW2_FRAME_STILL"
 else:style="BW2_FRAME_AUTO"
 if move and move[1] in known and style!="BW2_FRAME_AUTO":rows.append((move[1],style))
 records.append({"path":str(path.relative_to(a.bw2)),"sha256":hashlib.sha256(path.read_bytes()).hexdigest(),"move":move[1] if move else None,"style":style,"commands":commands,"branch_confirmation_required":True})
header="// Generated source guide; lexical presets, not recovered GBA timings.\nstatic const u8 sBw2MoveFraming[MOVES_COUNT_ALL] = {\n"
header+="".join(f"    [{move}] = {style},\n" for move,style in rows)+"};\n"
evidence=json.dumps({"files":len(records),"mapped":len(rows),"first_presets":counts,"records":records},indent=2)+"\n"
target=Path("src/data/battle_camera_bw2_framing.h")
if a.check:
 assert target.read_text()==header,"source-guide header needs regeneration"
 assert a.evidence.read_text()==evidence,"evidence inventory needs regeneration"
else:
 target.write_text(header,newline="\n")
 a.evidence.write_text(evidence,newline="\n")
print(f"PASS: inventoried {len(records)} DS scripts, {len(rows)} matching source-guided defaults")
