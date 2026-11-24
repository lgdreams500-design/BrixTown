---
adr: ADR-0046
title: Unreal buildings are laid from the building kit's pieces, at load time, from the data
status: proposed
date: 2026-11-24
relates_to: [ART-001, ART-002, WLD-020, TECH-005, TECH-008, ADR-0004, ADR-0045]
---

## Context
The Unreal town (Game/) was grey-box: one box per building. The owner asked to bring over the buildings' detail. The
browser slice draws every building in code (play.js `building()`), so it has no model files to import, and its shaders
cannot run in Unreal. The approved look of the buildings comes from the Blender building kit
(`Tools/blender/building_kit_07.py`, `_08.py`), which builds each lot from World/ with real stone blocks: right for a
render, far too heavy to export lot by lot for 965 buildings.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Export every building from the Blender kit | Exactly the kit's look | Millions of triangles and hundreds of large binary files; any data change means re-exporting; the city stops being data at load time |
| Port the slice's `building()` and shaders to C++ and Unreal materials | Matches the slice line for line | The largest job; the slice is itself an approximation of the kit |
| **Modular kit: the kit's own builders make each piece once; Unreal lays the pieces from the data** | Reuses the approved kit shapes; a dozen small files; the town is still built from World/ at load; copies of a piece draw together | Walls are simple boxes until a stone and plaster material exists; every new kind of piece needs an export |

## Decision
Modular kit. `Tools/blender/unreal_kit_36.py` runs the kit's `window()`, `door()`, `voussoirs()` and `louvres()` once per
piece (windows of each kind, doors, open shutters, a roof wedge) and exports them; `Tools/unreal/import_building_kit.py`
brings them into `/Game/Kit` with Nanite. `ABrixTownActor` lays each building as the kit does: a core set back by the
wall's depth, front walls between real openings, bays of 3.2 m on every facade, the lot's own door, a stone ground
storey in the old districts, a tiled gable roof or a parapet. The kit's numbers and the palette are in DefaultGame.ini.

## Consequences
- The look stays the kit's (ADR-0004). Walls, pavements, streets and open ground take their stone, brick and paving from
  World/rules/surfaces.json as the slice does (ADR-0017): BrixWorld picks each surface's pattern as world.surfaces()
  does (test WLD038 checks every lot and block against the Python), and M_Greybox draws it with the slice's own pattern
  and painted-plaster code ported to HLSL (Tools/unreal/greybox_surface.hlsl). About 5 fps of cost (61 at High).
- Cost on the owner's laptop, High quality: 63 fps grey-box; the first version with the pieces ran at about 22 fps
  (170,000 instances in 383 groups, one per piece and colour). Half the screen resolution gained almost nothing, so the
  cost was the number of objects, not pixels. Now each mesh is one group whose instances carry their own colour
  (M_Greybox reads it from per-instance custom data), walls, cores, roofs and trees use Nanite copies of the engine's
  shapes, and a wall piece that runs unchanged through several bands is one taller piece: 15 groups, 165,000
  instances, 63-66 fps, the same picture.
- Shopfronts, cafe fronts and balconies follow the kit's shopfront(), cafe_front() and balcony() the same way: units
  made by the kit's builders and stretched to each front in Unreal (about 172,000 instances in 25 groups; 60-65 fps at
  High on the owner's laptop).
- Turned designed houses (ADR-0032, ADR-0035) use the same generic layout for now, not their own plans and colourways.
