# Battle camera prototype checks

Run from the SoulGold root after the documented toolchain setup.

```sh
python tools/battle_camera_test/check.py
gcc tools/battle_camera_test/test.c -iquote include -DMODERN=1 -DTESTING=0 -DEMERALD -O2 -w -o build/battle-camera-host-test
./build/battle-camera-host-test
make -j4
```

On Windows the native executable can use a `.exe` suffix. `check.py` requires
`arm-none-eabi-gcc` on PATH or DEVKITARM, the built `tools/preproc` executable,
and generated `include/constants/region_map_sections.h`. The normal make setup
creates prerequisites. If only that header is missing, its existing rule is:

```sh
tools/jsonproc/jsonproc src/data/region_map/region_map_sections.json src/data/region_map/region_map_sections.constants.json.txt include/constants/region_map_sections.h
```

The ARM check runs CPP -> project text preprocessor -> ARM GCC for the eight
relevant translation units. It permits the existing unused-const warning in
battle_main.c, matching the project's default unused-warning policy.
The host harness includes the real camera implementation and project headers;
it removes only the ARM target attribute and stubs hardware-facing sprite
helpers. `-w` suppresses host/header portability warnings, not compile errors.
It checks state restoration and arithmetic, not GBA ABI, GPU output, timing,
ROM linkage, animation compatibility, or visual quality. Run the emulator
checklist in `reports/faux3d-prototype-testing.md` in the parent workspace.

## Iteration8.1 Plain guards and row warp

Before rebuilding the map, verify its prepared margin:

```sh
python tools/battle_camera_test/prepare_plain_map.py --check
```

To regenerate the checked-in map from its unchanged original top14 tile rows,
run the same command without `--check`. This reuses existing tiles/palette,
preserves the original illustrated112 rows, and changes only lower/negative-Y
guard rows. Regenerate the compressed map through the usual make dependency.

The native test now checks row sampling/contact consistency, stronger settings,
neutral fallback for unprepared terrain, whole-shot rollback under partial
matrix exhaustion, exclusive scanline admission, different/same-parameter
ownership replacement, and interrupted VBlank publication. DMA registers,
actual scanline timing, rendering seams and clipping still need emulator tests.
Renderer limits live in `include/battle_camera.h`; current choreography lives in
`src/data/battle_camera_tracks.h`.

## Wild/trainer coverage correction

The Plain-only gate has been removed. All loaded environments and battle flags,
including wild encounters and doubles, are admitted under the existing menu/
resource safeguards. Check all active persistent maps with:

```sh
python tools/battle_camera_test/prepare_background_maps.py --check
```

Without `--check`, the tool regenerates edge guards in the actual INCBIN map
paths (including special environments using `tiles.bin`). It preserves original
upper112 pixels, tile IDs, palette banks and map size. Plain keeps its existing
specialized turf extension; other environments reflect their own ground edge.
The native fixture now covers every environment ID, wild/trainer/link/Frontier/
recorded flags, two/four battlers, and retry after an initially busy scanline
owner releases its resource. Visual platform/seam checks remain necessary.

## BW2-style idle scheduling (current)

See the parent workspace reports `bw2-idle-camera-behavior.md` (verified DS
source) and `soulgold-bw2-idle-policy.md` (GBA implementation).

`BATTLE_CAMERA_WAIT_TICKS` in `include/battle_camera.h` now defaults to900,
matching the normal BW2 initial idle threshold. Wait900 eligible menu updates;
selection can start on the next one. There is no invented90-tick repeat pause.
Once ready, completed tracks allow another nonconsecutive choice. A busy
scanline owner delays admission without restarting the elapsed wait.

Six authored GBA tracks live in `src/data/battle_camera_tracks.h`. Keys contain
explicit update duration and0..256 fractions of the existing renderer envelope.
They approximate the referenced DS scripts' choreography, not their world poses,
braking algorithm or exact runtime timing. Private cosmetic RNG does not consume
battle RNG. Menu exit/incompatible presentation cancels and rearms the initial
wait; cancellation preserves the last track so it is not immediately repeated.

The native fixture now covers the initial threshold, complete paths and exact
neutral endpoints of all six tracks, nonrepeating selection, READY after busy
ownership, next-track continuation without a new delay, restart after
interruption, affine/row/resource invariants, and all environment/type/doubles
cases. Emulator visual and performance acceptance remain separate.

## Executed-move camera extension (2026-11-07)

Moving profiles are source-admitted only for canonical Headbutt, Peck, Ember,
Splash, Withdraw and Helping Hand scripts. Growl, Tail Whip and Rain Dance use
stable neutral group/field framing. All other or replaced scripts remain neutral.
These entries have not yet been observed with the extension in an emulator.

```sh
python tools/battle_camera_test/audit_move_scripts.py
```

The compile checker now covers eight relevant C units. The native fixture tests
queued snapshot-safe lifecycle events,20 repeated actions from both sides,
resolved self/ally/spread/field/redirection classes, effect registration and
slot reuse, projectile endpoints, local affine state, hidden helpers, matrix
exhaustion, interruption/rebinding/faint/exit, watchdog and neutral restoration.
It compares mid-focus state under1x/4x logical-update grouping; it does not run
actual accelerated VM/game frames. Full idle/background checks remain in place.

Move tuning/bindings live in `src/data/battle_camera_move_profiles.h`; snapshot
capacity, watchdog and focus calibration are in `include/battle_camera.h`.
Script pointer matching is intentional: do not add a move ID without auditing
its actual animation and C callbacks. The static script checker does not replace
that C audit or emulator observation. No camera tasks or animation waits are
added; all move events commit in BeginFrame inside the existing OAM transaction.

In the parent workspace read faux3d-move-camera-implementation.md,
faux3d-move-camera-testing.md and faux3d-move-camera-compatibility.md. Rebuild a
ROM in the working tool environment, enable battle animations, and test multiple
successive executed moves. With battle scenes/headless animations disabled,
no move camera is forced over an animation the engine skips.

## Smooth handoff and stronger idle framing (2026-11-07)

Menu exit now eases to neutral over16 logical updates. A compatible moving
profile starting sooner inherits the pose and blends rendering models. Safety
fallbacks for incompatible scripts/resources remain neutral. The native test
checks peak-idle exit, first-frame continuity, completed blend and cleanup.

Idle singles pan is16px, doubles12px; move pan remains12px. Idle vertical pan
is2px and parallax attenuation160/256. Zoom,900-update initial wait and track
durations are unchanged. All six tracks emphasize lateral movement. Settings
live in include/battle_camera.h; keys in src/data/battle_camera_tracks.h.
Read reports/faux3d-camera-handoff-and-idle-tuning.md in the parent workspace
for validation limits and the pending emulator checklist.

## Prompt9 depth bands (2026-11-07)

The camera now shares artwork-space depth weights across row parallax, ground
projection and battler/shadow scaling. Plain uses full depth strength; other
recognized environments use a restrained fallback. No additional BG/OBJ layer
or artwork was allocated. Fully blended move animations retain uniform scene
projection for their existing effects.

`BATTLE_CAMERA_DEPTH_PRESET` in include/battle_camera.h is1 cinematic by default,
0 conservative. To exercise both with a native GCC, compile this fixture normally
and again with `-DBATTLE_CAMERA_DEPTH_PRESET=0`. TestDepthProjection sweeps zoom,
vertical pan, handoff weights, source contacts and all environment admissions;
it also checks independent golden contacts and bit-exact neutral row identity.
The native fixture is not an emulator or a cycle benchmark.

Parent-workspace reports/faux3d-depth-implementation.md links the six Prompt9
reports, source map, full-build blocker and detailed pending emulator matrix.

## Prompt10 cinematic polish (2026-11-07)

Native tests now check monotonic easing (including lookup-assisted smootherstep),
row-cache reuse/invalidation, OAM rectangle-aware zoom, identity/flip versus
nonidentity affine bounds, first-impact deduplication, impulse decay and cleanup.
The same tests run in both depth presets. Real battle sprites are normally
affine; identity/flip matrices remain eligible for zoom. Unknown rotated/scaled
bounds suppress extra zoom rather than overwriting local affine state.

Profile shake is2/1px over8 updates for Headbutt,1/0px over6 for Peck, zero for
other admitted profiles. Normal/menu returns use smootherstep; inherited idle
legs retain smoothstep before profile-specific easing. No VM waits were added.
Rendering cache keys include effective shake X/Y, zoom, blend and environment.
See parent reports/faux3d-regression-tests.md for the full pending emulator matrix.

## Universal move profiles and stronger framing (2026-11-07)

Every valid executed move now selects a metadata/target default, retaining the
nine specialized script overrides. Live generic effects use uniform translation;
extra zoom is a short after-effect cue. BG/scanline/global-offset owners suspend
camera rendering and may receive the cue after animation completion/resources
release. Recovery times out after60 logical updates and never delays gameplay.
Move pan cap16px; specialized/post-effect zoom requests24/256 extra; field pan4px.

The native fixture dispatches all935 nonzero move-ID slots with synthetic
metadata, not935 actual animation scripts. It also tests deferred launch/end,
cancel priority, busy-owner expiry and repeated cleanup. Read the parent report
reports/faux3d-all-move-camera.md before treating earlier six-move lists as current.

## BW2 research and animation-aligned correction (2026-11-07)

Latest policy supersedes the generic after-effect cue above. Acquire and hold
through actual animation completion, then return; no late zoom shot. Most
acquisition/return profiles use24-32/28-32 logical updates. User-approved Ember
retains its dedicated8/24/6/10 profile and old phase policy.

The checked-in src/data/battle_camera_bw2_framing.h contains475 source-guided
hints from601 BW2 script files. The offline generator reads the reference source;
it is not part of the ROM build. To verify it from the SoulGold root:

```sh
python tools/battle_camera_test/research_bw2_moves.py ../pokebw2 --evidence ../reports/bw2-move-camera-script-inventory.json --check
```

The inventory is lexical, not proof of branch/video timing. Unknown moves keep
conservative target/category defaults. Travel-style handoffs use actual visual
completion or impact once. Mon-as-BG uses tracked BG1/BG2 uniform1x offsets;
unsafe zoom inheritance/BG replacement still suspends, with no postlude.
Native tests cover target hold, event-only acquisition, no reset on repeat,
BG1/BG2 hidden actors, cleared bindings, smooth return and Ember preservation.
Read reports/bw2-move-camera-resynchronization.md in the parent workspace for
current source references, evidence and pending emulator validation.

## Target-dependent prototype composition (2026-11-07)

Spread/group focus now derives from the actual target mask/centroid, not a fixed
field pan. Near/player framing admits positive vertical movement within guards;
far/opponent framing remains distinct. Single and group zoom budgets differ.
Quiet uninvolved images/shadows receive eased reduced extra zoom around their
shared contact; nearby VFX/local affine/movement protect the common transform.
Mon-as-BG and Ember keep the shared model. Compatible handoffs retain context
weights for the same actors instead of resetting them every move.

Native cases include both directions of Growl, actual doubles positions/masks,
individual near slots, selective context scaling/VFX restoration and positive-Y
source coverage. Both presets pass. Bounds can still reduce matching camera
goals, especially near-left64px sprites. Parent report:
reports/faux3d-target-composition.md. Emulator appearance remains unverified.

## Action completion, target reactions and HP bars (2026-11-07)

Visual script end no longer releases the shot. Normal battle action FINISHED,
all controller flags clear and animation idle release it with a minimum44-update
smootherstep return. Known stat/status reactions retain the original camera;
known hit blink permits temporary authored invisibility, and a tracked0-HP
image can remain while its bar finishes. Switching/disappearance/unsafe owners
still cancel. Healthbar/reaction progress resets camera-only watchdog age.

Native callers now use BattleCamera_FinishAction after simulated action/controller
completion. New tests cover blink, stat/status, extended busy HP bar, ignored
early finish, slower return, zero-HP hold and special-animation cancellation.
Direct debug users outside the normal turn loop must notify action completion.
See parent reports/faux3d-action-completion.md.44 is logical updates, not an
emulator-measured wall-clock duration; no gameplay waits/tasks were added.

## Consistent move strength

Generic moves now request full profile zoom after visual creation, with a separate eased clock to avoid catch-up snaps. Monbg and possible freeze/frostbite reaction paths retain compatible1? translation. Native regressions cover delayed visual activation,935 synthetic dispatches, action-completion holds and shared affine matrices with a single remaining hardware slot. These checks do not establish visual support for every move. See the workspace report `reports/faux3d-consistent-move-strength.md`; playtest both sides, doubles, monbg, status reactions and particle-heavy moves.

## Recipient contact zoom

Upward zoom now waits for a registered visible effect near the resolved recipient, an actual hit reaction, or recipient stat/status reaction start, for specialized and generic profiles alike. Tests cover travel before contact, arrival, self/ally/incoming cues and smooth action hold. Proximity is an approximation; visual onset for unusual VFX requires emulator capture and possibly explicit animation hooks. See workspace `reports/faux3d-recipient-contact-zoom.md`.
