# Compact battle HUD checks

From the SoulGold root:

```sh
python tools/battle_hud_test/run.py --cc /path/to/native/gcc
python tools/battle_camera_test/check.py
python tools/battle_camera_test/check.py --classic-hud
```

The native harness exercises the exact coordinate/EXP helpers used by the engine with actual source EXP tiles: panel bounds, nonoverlapping doubles, separate nickname/level/HP lanes, split player-sheet tile addressing, all0..8 fill levels and no buffer overwrite. A companion model checks actual source-font glyph bounds, including11,112 HP strings, long Latin names/genders and the real LV extra-symbol glyph. It does not execute the engine text printer or prove hardware presentation. The ARM checker now includes battle_interface.c and battle_gimmick.c in addition to the previous eight units. Relevant graphics INCBIN prerequisites must be built first through normal asset rules (ability_pop_up uses its existing8×4 metatile conversion).

`B_COMPACT_BATTLE_HUD` defaults TRUE in include/battle_hud_layout.h; set FALSE for the stock layout. HP remains48 logical pixels; EXP remains64 and keeps existing gain/level-up/max-level math, displayed as a four-pixel-high strip in player singles. Doubles keep their stock no-EXP policy. No affine matrices or extra sprites/sheets/palettes are required. Safari player panels retain their original layout.

Manual emulator checklist: wild/trainer singles and doubles; names/gender at maximum nickname length; level1/99/100; HP0/1/full and four digits; damage/healing and green/yellow/red; all statuses and cure; caught marker; shiny/dark UI; gimmick icon/level alignment; number/bar toggle; entrance/selection/hide/shake; switching, fainting, party/bag return; EXP0/partial/full, multiple level-ups and max level; normal/accelerated speed; camera focus/contact/HP-action hold. Check no overlap with text at Y112 and no unexpected original-frame redraw. No runtime success is implied by the native/ARM checks.
