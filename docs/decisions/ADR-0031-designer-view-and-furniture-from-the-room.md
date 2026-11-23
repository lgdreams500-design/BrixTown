---
adr: ADR-0031
title: The building designer looks from above, cuts the storey open, and finds the town's furniture in the room as drawn
status: accepted          # owner in chat on 2026-11-23: an isometric view of the house with a camera to move, and placing and arranging furniture ("Arrange + add, priced")
date: 2026-11-23
relates_to: [BUILD-005, BUILD-020, FURN-051, FURN-052, FURN-055, ADR-0018, ADR-0028]
---

## Context
The remodel designer in the slice (BUILDER.md §2.5) showed changes on the building, but through the player's own camera at
the door: the owner expected, on pressing E, a view of the whole house from above that they can turn and move, and to place
and arrange furniture. To see furniture the storey has to be opened up. And the furniture already in a home is not data:
`play.js` `furnish()` draws it straight into a room mesh from each building's places (beds, chairs, desks) and its kind.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| **The camera above the building; a cut in the designer's own shaders; the town's pieces found as groups of touching boxes** | No change to how the town is drawn or furnished; every kind of room works at once; the town's shaders keep the early depth test | Piece names come from what furnish() was drawing (a tag), not from data; a piece is a group of boxes, so two pieces that touch are one |
| Hide the building's roof and upper meshes instead of a cut | No shader change | The roof, the upper storeys and the outer walls are in the same town and detail meshes as the neighbours; they would have to be split per building |
| Rewrite furnish() so every room is a list of catalogue pieces | Furniture becomes data everywhere | A rewrite of every room of every kind of building; the town would no longer look as it does (BUILD-002) |

## Decision
- **View.** While the designer is open, `designCam` replaces the player's camera: a target over the building, 35° down
  (the angle of an isometric drawing), a 0.6 rad lens, framed beside the panel. Drag turns and tilts it, the wheel brings it
  nearer, W A S D slide it, Q and E turn it.
- **Cut.** On the Inside and Furnish tabs, inside the lot's rectangle (0.6 m wider), fragments above the storey's ceiling
  are discarded, and the walls (outer, and inner: material 17) above 1.25 m over the floor. The cut lives in `#define CUT`
  copies of the lit, depth-only and shadow programs, made the first time the designer cuts, so the town's shaders keep the
  early depth test that the painted signs alone give up (a discard costs every pixel).
- **The town's pieces.** For a storey being furnished, `furnish()` runs once more into a recording mesh that keeps each
  top-level box apart. Boxes that touch (6 mm) are one piece; flat boxes on the floor make rugs; a group that starts more
  than 1 m above the floor and touches nothing below (pictures, hanging lamps) stays where it is. `furnish()` sets a tag
  for what it is drawing (bed, wardrobe, armchair...), and a piece is named from its tags. The groups are the same every
  time, so a saved layout names them by their order (`t0`, `t1`...).
- **Layouts.** An arranged storey is a layout (`Tools/citysim/furnish.py`): the town's pieces moved and turned, and pieces
  from `World/rules/furnishing.json` drawn by `furniture.js`. Its room mesh is built from the layout; once kept, the load's
  collisions for that storey are switched off and each piece gets one, and its places to sit and lie move with it.

## Consequences
- Unreal will furnish rooms from data; there the layout is already the unit's furniture (FURN-051) and no grouping is
  needed. The rules (`furnish.py`) carry over as they are.
- Pieces that touch in the town's layout move together (a desk and the chair pushed under it would); that is acceptable
  for the slice.
- Inner walls between rooms are not in the check yet; residents who live in an arranged home still use the town's places.
