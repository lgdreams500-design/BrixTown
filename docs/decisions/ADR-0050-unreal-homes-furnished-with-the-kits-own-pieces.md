---
adr: ADR-0050
title: Unreal's homes are furnished from their plans with the furniture kits' own meshes, near the camera
status: accepted
date: 2026-11-25
relates_to: [FURN-001, FURN-002, FURN-060, FURN-061, FURN-062, FURN-063, ADR-0026, ADR-0035, ADR-0036, ADR-0046, ADR-0048]
---

## Context
The slice furnishes its planned homes (homes.js: row houses and court flats; houses.js: the designed houses) from
World/rules/home_plans.json, drawing each piece as boxes: furniture.js's box versions of the Blender pieces of
Tools/blender/furniture_kit_17.py and furniture_kit_18.py. The approved look of the pieces is the Blender kits' own
(Art/Reference/house_interior_*.png, home_interior_*.png). Unreal's buildings can now be entered (ADR-0048).

## Options considered
| Option | Pros | Cons |
|---|---|---|
| **Export the kits' pieces from Blender, one mesh per piece and line; place them in Unreal from the plans** | The approved pieces themselves, rounded edges and all; one instance per piece | 272 more binary meshes (48 MB) in LFS |
| Port furniture.js's boxes to C++ | No new binaries; line for line with the slice | Boxes where the reference shows real furniture; dozens of boxes per piece |

## Decision
- `Tools/blender/unreal_furniture_37.py` builds every piece of kits 17 and 18 in each of the four lines (ADR-0026) at the
  kits' own sizes, bakes each material's colour into vertex colours, and writes one .glb each to
  `Art/Export/unreal_furniture_37/` with an `index.json`. `Tools/unreal/import_furniture_kit.py` imports the pieces the
  plans use into `/Game/Furniture/SM_<piece>_<line>` with Nanite, all with one material, `M_Furniture` (the vertex colour,
  matte). The .glb files are not kept in git (64 MB, remade in about 20 s); the imported meshes are (48 MB).
- `BrixWorld/BrixHomes` ports homes.js and houses.js: the plan mapped into the home (mapA), the inner walls cut at their
  doorways, the rooms (tiled where wet), the pieces at the household's furnishing level (FURN-063: the household's daily
  wages and the day, as furnishing.js works it out before a market exists) with a bed for every resident, the tall pieces
  in front of the building's real windows left out (dropped()), and where people lie and sit.
- `ABrixRooms` furnishes the homes within 30 m of the camera and rebuilds when that set changes (the slice's near rooms);
  the rooms of the home the camera is in have their lamps, as homes.js gives each room one. The light is the one the
  owner approved for entering a room (ADR-0048).

## Consequences
- 719 planned homes (469 designed houses, 250 row houses), 25,284 pieces on day 16; `FURN060_every_planned_home_is_furnished_in_its_rooms`.
- Only the ground floors are furnished until the storeys above are built with their stairs; court flats (on upper
  floors) wait for them too. Shops, cafes and civic buildings are furnished separately (play.js furnish()).
- A pieces' look change means running both scripts again; a piece the plans start using is imported by running the import.
