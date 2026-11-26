---
adr: ADR-0061
title: A room fitted out by its design; a named person with a look of their own
status: proposed        # built on 2026-11-26 to bring ADR-0059's approved looks into the game; the owner has not seen it in Unreal
date: 2026-11-26
relates_to: [ADR-0004, ADR-0026, ADR-0047, ADR-0056, ADR-0058, ADR-0059, CITY-013, QST-014, ART-004]
---

## Context
The owner approved two looks in Blender on 2026-11-26 (ADR-0059): the Stone Oven's inside and Noor. Neither was in the
game. Unreal furnishes every building with no plan as the slice does (play.js furnish(), ported as BrixFurnish), by the
building's kind, so the Stone Oven, a shop, was fitted as a grocer's; and every resident wears their job's stock clothes
(people.js look(), ported as BrixLookOf), so there was no way to give one named person her own clothes or her age.

## Decision
- **A fit-out is data.** `World/rules/fitouts.json` holds a design's pieces, placed in the door's frame (along, in), with
  where each faces, its size, lit boxes (an oven's fire) and pendants. A lot names its fit-out in its block file
  (`"fitout": "stone_oven"`). On that floor Unreal places the fit-out alone: none of the slice's places' furniture or
  dressing. Other floors are furnished as before. `world.fitout_problems()` checks every piece against its export and that
  it stands inside the walls and out of the stairwell.
- **The pieces are the approved design's own**, not re-modelled: `Tools/blender/unreal_bakery_50.py` runs baker look 47's
  piece functions and exports each with unreal_furniture_37's own colouring and export; `import_furniture_kit.py` imports
  them into `/Game/Furniture` beside the furniture kit.
- **A named person's look is data.** `World/rules/cast_looks.json`, keyed by resident, holds the designer's values and the
  ones that say age, copied from the Blender design the owner approved (Noor: N of baker_look_47.py). Unreal applies it
  over the role's look (`BrixApplyCastLooks`); the walk and seed stay the role's.
- **Age and a dress in Unreal's people**, drawn only for a look that has them, so everyone else stays exactly as the slice
  draws them (ART004 unchanged): garment `dress` (one form from the hem to the chest, short sleeves, bare legs), `age`
  (creases at the brow, the eyes and from nose to mouth in the person's own skin, paler lips, locks along the hairline in
  two close tones over the hair shell's cut edge, after rule C9).

## Options considered
| Option | For | Against |
|---|---|---|
| A new kind `bakery` in the slice's furnishing code | One path for every bakery later | The boxes would be the slice's, not the approved look; the slice is frozen |
| **A fit-out named by the lot, from the design's meshes** | The approved pieces themselves; data, not code; one lot or many | A second way to furnish a room |
| Rebuild Unreal's people on the Blender elders' model | Age and hair exactly as approved | A new body for everyone; breaks the port's check against the slice |
| **Age and a dress as extra parts on the same body, for a cast look only** | Small; nobody else changes | Not the Blender elders' full set (no stoop, glasses, cane, shawl, beard); the hair is still a shell under the locks |

## Consequences
- The Stone Oven's ground floor is the approved bakery's pieces in a shop of 38 by 16 m: the design was a room of 7.6 by
  4.8 m, so the pieces stand round the four staff's work places, the racks along the back wall, the counters before the
  staff; the room is larger and emptier in front than the design.
- Noor wears her own look wherever she is drawn. The stoop, the Blender brow value and the elders' other values are not
  carried; the hair under the locks is still the shell (ADR-0059's weak point, only covered at the hairline).
- Neither has passed the look gate beside its reference until the shots are taken and kept (`Art/StyleTests/unreal_bakery_50/`).
