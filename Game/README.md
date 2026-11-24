# Game

The Unreal project of Brix Town (ADR-0025): Unreal Engine 5.8 (TECH-001), C++ (TECH-002). It was set up by hand, not
from a template (the installed engine has no templates folder), and it has no map or asset files of its own yet: it
starts on the engine's empty map and the game mode builds the town from `World/` at load time (WLD-020).

| Path | What it is |
|---|---|
| `BrixTown.uproject` | The project |
| `Source/BrixWorld/` | The map as data: reads `World/<city>/city.json` and its block files into plain structs, and checks them (WLD-022). Needs only Core and Json (TECH-003, TECH-004). Its tests are here too |
| `Source/BrixTown/` | The game: `BrixGameMode` (load the city, the light, the player's start), `BrixTownActor` (the town as instanced shapes), `BrixCharacter` (the player in third person) |
| `Config/DefaultGame.ini` | Every tunable number and colour (TECH-005) |
| `Config/DefaultEngine.ini` | The startup map, Lumen and virtual shadow maps |

## What it shows so far (grey-box)
The starting city at the slice's sizes: a box for each building (its footprint, turned by its `angle`; `floors` × 3 m +
0.4 m high, at least 3.6 m), a slab for each block (green for parks and open ground), a trunk and crown for each tree,
the sun low in the east, sky, haze, Lumen. You walk it from the city's spawn with the slice's keys: W A S D, mouse,
Shift to run, Space to jump. No residents, cars, interiors, money or sound yet.

## Buildings (ADR-0046)
Each building is laid as the Blender building kit lays it: a core set back by the wall's depth (0.32 m), front walls
between real openings, bays of 3.2 m on every facade, the lot's own door, a stone ground storey in Old Town and the
Centre, all-stone clinics and workplaces, a tiled gable roof on homes and shops (a parapet on the rest). The windows,
doors and shutters are the kit's own pieces: `Tools/blender/unreal_kit_36.py` makes them (Blender, headless) into
`Art/Export/unreal_kit_36/`, and `Tools/unreal/import_building_kit.py` imports them into `/Game/Kit` with Nanite. Run
both again after changing a piece, and `Tools/unreal/make_greybox_material.py` before them after changing the material.
The whole town is 15 groups of instances, one per mesh, each instance carrying its own colour, all through Nanite: on
the owner's laptop at High it runs at 63-66 fps, as fast as the grey-box (ADR-0046). The binaries are lockable in LFS
(ADR-0002): `git lfs lock <file>` before running the scripts that remake them.

## Shopfronts, cafe fronts and balconies
On the street front of every shop of two storeys or more, the kit's shopfront(): one opening across the ground storey,
teal pilasters on stone plinths about every 2.2 m, lit display panes over a stall riser, a glazed door in the middle, a
fascia with the shop's name, a striped awning and crates of produce on the pavement. On every old-town cafe of two
storeys or more, cafe_front(): a striped awning over the arches, a blade sign at the corner, tables and chairs on the
pavement, and a French window on a balcony over the middle bay (balcony(); the kit puts balconies on cafes only). The
pieces are units the game stretches to each front (a pane, the door bay, a metre of fascia, an awning strip); the
numbers are in DefaultGame.ini.

## Stone, brick, paving and plaster (ADR-0017)
Every wall, pavement, street and open lot is drawn with the pattern World/rules/surfaces.json gives it: coursed rubble
in the old quarter, brick in the terraces (about half to the eaves), rusticated stone on civic buildings, ashlar
elsewhere; flagstones, slabs, herringbone and basket weave underfoot; setts in the streets; mottled two-tone plaster on
the upper storeys. BrixWorld picks the patterns as `world.surfaces()` does; the test `WLD038_surfaces_match_the_rules`
checks every lot and block against `Tests/values/surfaces_city_01.json` (run `python unreal_test_values.py` in
`Tools/worldgen` after changing the rules). M_Greybox draws them with the slice's own code, ported to
`Tools/unreal/greybox_surface.hlsl`; each box carries its pattern in its custom data.

## Light and speed (measured 2026-11-24 on the Intel integrated graphics of the owner's laptop, 1066x666)
Lumen (bounced light, the sky's fill in shade) needs shader model 6 (`Config/DefaultEngine.ini`); without it the shade is
black. At Epic quality the town runs at about 36 frames a second, at High about 63 with the same shade, at Medium about 86
but without Lumen. On this laptop use High (`scalability 2` in the console, or `-ExecCmds="scalability 2"`). The light
follows the kit daylight (ADR-0007): a shaded wall on a wide street measures (161, 151, 135) against (159, 151, 146) in
`Art/Reference/world_street_kit_light.png`. The first run after a change of shader settings compiles shaders for minutes
and runs at a few frames a second; that is not the game's speed.

## Build, run, test
Paths below are for this machine (`docs/setup/DEV_ENVIRONMENT.md`); run from the repository root.

```
"C:\Users\leena\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" BrixTownEditor Win64 Development -Project="%CD%\Game\BrixTown.uproject" -WaitMutex
"C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%CD%\Game\BrixTown.uproject" -game -windowed -ResX=1600 -ResY=900
"C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject" -ExecCmds="Automation RunTests BrixTown.World; Quit" -unattended -nullrhi -nosplash
```
The first is the build, the second plays it, the third runs the tests. Without `-game` the second opens the editor; press
Play there. To check the picture without anyone at the keys, add `-BrixShot=C:/full/path/shot.png` (a screenshot after
12 s, then quit) and optionally `-BrixCam=x,z,height,yaw,pitch` (metres and degrees, the data's frame, yaw 0 = east).

## Tests
| Test | Requirement |
|---|---|
| `BrixTown.World.WLD020_city_loads_from_data` | WLD-020 |
| `BrixTown.World.WLD022_every_lot_is_sound` | WLD-022 |
| `BrixTown.World.WLD022_broken_lots_are_caught` | WLD-022 |
| `BrixTown.World.WLD038_surfaces_match_the_rules` | WLD-038, WLD-039 |
