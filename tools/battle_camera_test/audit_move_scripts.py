"""Static audit of the nine specialized bindings, not universal eligibility or runtime proof.
Run from SoulGold root. Traverses simple assembly call/goto bodies and rejects
BG ownership commands in non-neutral camera profiles. Also emits source lines.
"""
from pathlib import Path
import re

lines = Path("data/battle_anim_scripts.s").read_text().splitlines()
labels = {}
for i,line in enumerate(lines):
    m = re.match(r"^([A-Za-z_][A-Za-z_0-9]*):{1,2}\s*$",line)
    if m: labels[m[1]] = i
profiles = Path("src/data/battle_camera_move_profiles.h").read_text()
rows = re.findall(r"\{(MOVE_\w+), (MOVE_CAMERA_\w+), (gBattleAnimMove_\w+)\}", profiles)
blocked = {"monbg","monbg_static","fadetobg","fadetobgfromset","changebg","restorebg"}
for move,profile,label in rows:
    seen,work,tasks = set(),[label],set()
    while work:
        at = work.pop()
        if at in seen: continue
        seen.add(at)
        start = labels[at]
        for index in range(start+1,len(lines)):
            text = lines[index].split("@",1)[0].strip()
            if not text or text.startswith("#"): continue
            cmd = text.split()[0]
            assert cmd not in blocked or profile == "MOVE_CAMERA_GROUP", (move,index+1,cmd)
            if cmd in {"call","goto"}:
                work.append(text.split()[1].split(",")[0])
                if cmd == "goto": break
            if cmd == "createvisualtask": tasks.add(text.split()[1].split(",")[0])
            if cmd in {"end","return"}: break
            assert not text.endswith(":") and cmd != "gBattleAnimMove_", (move,index+1,"unexpected control flow")
    print(f"PASS: {move} ({profile}), data/battle_anim_scripts.s:{labels[label]+1}, helpers={sorted(seen)}, tasks={sorted(tasks)}")
assert len(rows) == 9
print("Source audit only: C callback behavior and renderer timing require separate tests.")
