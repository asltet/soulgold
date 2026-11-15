# Compact battle HUD checks

Run from the SoulGold root:

```sh
python tools/battle_hud_test/run.py --cc /path/to/native/gcc
python tools/battle_camera_test/check.py
python tools/battle_camera_test/check.py --classic-hud
python tools/battle_hud_test/preview.py --output ../reports/assets/bw2-native-hud
```

The native suite exercises actual coordinate/HP-packing/EXP helpers. Source-font checks cover floating names, gender and level; asset checks cover copied source hashes, original native silhouettes, generated frame bytes and11,112 HP strings using BW2 digit ink. The preview is analytical, not a capture. Normal build/tests need no BW2 checkout. Only reimport needs it:

```sh
python tools/battle_hud_test/import_bw2.py ../pokebw2
```

Original outline pixels are retained unscaled. Baked labels are cleared and colors mapped to existing GBA theme slots. Five-row HP, six-row original digit ink and four-row EXP fit the15px player plate. HP remains48 steps; EXP64 and singles-only. No extra sprites, affine matrices, palettes or sheets. Set B_COMPACT_BATTLE_HUD FALSE for the stock layout.

Manual emulator tests: singles/doubles/Safari, long names/gender/levels, HP0/1/four digits/damage/healing, EXP/levels, statuses/cure, dark/shiny/gimmick/caught icons, numeric toggle, party/bag restoration, hide/slide/shake, switching/fainting, normal/accelerated speed and camera HP-action hold. Inspect native plate ends and compact text/bar rows. Native/compile success does not imply runtime acceptance.

The latest importer also ports all27 original HP fill tiles and nine striped EXP tiles. Native/offline checks validate source-word equality, fill0..8, borders and end caps. Existing GBA palettes and48/64px bar lengths remain. The old HP label remains local artwork.

Latest layout preserves original HP/EXP/Lv labels in the complete native cell. Side-specific header/HP lanes and fixed-slash numbers replace the earlier centered layout. EXP uses four-row inset4 in row3 at X40/Y28. Compact HP label OBJ slots are blank; body artwork supplies HP letters. Tests verify original colored label pixels and native three-digit/four-digit adapted number positions.
