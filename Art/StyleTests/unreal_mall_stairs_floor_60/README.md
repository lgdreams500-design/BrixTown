# Unreal: Eastgate's moving stairs as steps, and the court's floor as polished stone (2026-11-28, WLD-058)

The owner, 2026-11-28, of Eastgate Mall in the game: "there are no steps pattern of escalator visible, floor is square
titled instead of smoot", then "fix the escalator steps and the floor".

The place and its references (`python Tools/reference_catalog.py --for MallsEastgateEscalators`): the moving stairs,
`MallsEastgateEscalators`; the Great Court, `MallsEastgateCourt`. Both approved by the owner, in Blender. The game's
picture is the references' mirror image, and the stairs' stop is a scouted one on the stairs' own line (the catalog says
why), so the pictures do not line up shape for shape.

## What was changed (`ABrixMall`, `M_MallRoom`; nothing in `BrixWorld/BrixMall`, no light)
- **The stairs.** Over the slice's plain slope, the thirty steps a walker's feet already climb: dark treads ribbed along
  the way they run, a brass nose on each, a darker riser; a steel floor plate with a brass edge and a comb at the foot
  and the head; steel skirts with a line of light along the steps; a brass handrail on each parapet, turned down at its
  ends. Numbers in `DefaultGame.ini` and `BrixMallActor.h` (`StairSteps`, `StairRibs`, `StairRib`, `StairNose`,
  `StairPlate`, `StairBrass`, `StairSteel`, `StairPlateColour`).
- **The floor.** A court mall's floor carries no tile pattern: it is polished stone with hidden joints, the floor's own
  colour toned (`FloorStone`), softly clouded, and smooth enough to mirror the lights (`FloorRough` 0.1, where every
  other face is 0.85). `M_MallRoom` was remade by `Tools/unreal/make_mall_materials.py` for this (the asset was locked
  first). The furniture outlet's hall keeps its tiles.

## The pictures
| File | What it shows |
|---|---|
| `stairs_beside_reference.jpg` | The stairs: the reference at Midday, Dusk and NightLamps; the game before; the game now, at 12:00, 20:00 and 22:30 from the foot (`cam:1683.4,660,1.5,96,3`) |
| `court_beside_reference.jpg` | The Great Court, the same three rows, from the reference's own camera (`cam:1668,636,1.7,79,5.7`) |
| `stairs_close.jpg` | Close on the steps, from the head, from the side; the player standing on each flight; and the first try, kept as a failure (the floor with its tiles off and nothing in their place was one flat white, the ribs loud pale stripes) |
| `stairs_marked_before.jpg`, `stairs_marked_now.jpg` | The reference page's Midday picture of the stairs before and now, with what was wrong numbered on it: green mended, red still wrong |
| `food_court_and_shop.jpg` | The food court and the shop walk, before and now: their floors are the same stone |

Shots: `Tools\unreal\light_tour.ps1`, 1066 x 666, High. Frame rates now: the foot of the stairs 42 to 45, the court 35 to
42, the food court 43, the shop walk 44 (before, on the reference page: the stairs 45 to 49, the court 39 to 42).

## The look gate (G1 to G10), the whole frame
| # | The stairs | The court |
|---|---|---|
| G1 shade and depth | Yes on the steps: risers darker than treads, the skirts shaded. No under the stairs' sides: flat dark brown | Partly: the floor now carries the galleries' dark reflections; undersides are still one dark brown |
| G2 shadows | No: nothing casts a shadow on the steps or the floor; the faces are lit by the hall's even light | No, as before |
| G3 surfaces | Treads ribbed, brass and steel read as such. The parapets and the stairs' sides are still plain fills | The floor is no longer a matte checker: polished, clouded, mirroring the lights, trees and shop fronts. It is polished on purpose, as its reference is. Undersides still plain |
| G4 colour | Warm and muted; the brass is a dull ochre, less golden than the reference's | Yes |
| G5 whites and blacks | The skirts' lines of light and the gallery lines are burnt white, as before | The gallery lines are burnt, as before |
| G6 people | The player on the steps is lit as before (the hall's light for people) | Nobody is in the court in these shots |
| G7 lamps | The stairs read at dusk and at night; no warm pools | As before |
| G8 whole, right way round | Yes: nothing inside out, the steps meet the floor at the foot and the head. The handrails end in flat square drops, not the reference's rounded curl | Yes |
| G9 the hours | 12:00, 20:00 and 22:30 are nearly one picture; at night the floor is paler than by day, as before | The same |
| G10 nothing over the picture | Yes | Yes |

`python Tools/unreal/look_check.py` on the thirteen shots: 13 pictures, 0 fail.

## Still not as the references (also in `Game/README.md`, Not yet)
- The stairs have no glass sides with round newels; the parapets are the slice's plain cream slabs.
- The handrail is a square brass bar that ends in a flat drop: the reference's is round and curls over at the foot.
- No timber cladding on the stairs' sides and underside: plain dark brown.
- The slope is the plan's 12 degrees, a moving walk's, so there are thirty long shallow steps; the reference's stairs
  are steeper with short steps. The steps do not move: the walker is carried, the drawn steps stand still.
- The court: no timber slats under the galleries, no glass balustrades, columns, chandeliers or banners; the roof
  light is a patch of dots. Nothing casts a shadow on the floor.
- The floor's reflections were looked at from these stops only; how they hold up from other angles, and whether
  things off the screen are mirrored, was not checked.
