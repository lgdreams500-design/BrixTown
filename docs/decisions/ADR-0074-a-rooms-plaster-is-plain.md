---
adr: ADR-0074
title: A room's plaster is plain in the game: one even tone, no mottle
status: accepted          # the owner in chat, 2026-11-28, on the golden room's first shots: "they are good but can we remove the patches on the walls and make them smoother?", then "what happens if we don't paint at all? would it look bad?", then, shown it three ways, "no paint is looking good to me"
date: 2026-11-28
relates_to: [ADR-0004, ADR-0017, ADR-0026, ADR-0058, ADR-0073, ART-001]
---

## Context
The golden room (ADR-0073) was built in Unreal and shot beside its Blender references. Its walls and ceilings carried
large patches: a dark bluish tone blotched against a warm highlight. They were not the engine's light. They were the
paint of the town's one material (`M_Greybox`, `Tools/unreal/greybox_surface.hlsl`, the slice's `paint()` ported for
the street's walls, ADR-0017): its dark tone is 0.69 of the colour and its highlight 1.12, where the approved rooms'
plaster in Blender is two close tones. Rule W1 of ART_DIRECTION asks for surfaces "matte and mottled ... never a flat
fill", and I1 carries it indoors.

The owner was shown the room and the hallway three ways, at 07:36 and at 22:30
(`Art/StyleTests/unreal_golden_room_58/walls_three_ways_home.jpg`, `walls_three_ways_hall.jpg`): as it was, nearly smooth
(0.8 of the way to one tone) and with no mottle at all.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| The street's mottle on a room's walls, as it was | W1 as written | Reads as stains on a wall seen from two metres; the owner asked for it gone |
| Nearly smooth (0.8) | Keeps W1 in the letter | At a shot's size it cannot be told from none; the owner chose none |
| **No mottle: one even tone** | What the owner chose by eye; the walls still shade into the corners and take the lamp's pool | An exception to W1 and I1; the look check's flat-fill limit does not fit a room's plain walls |

## Decision
- **A room's plaster in the game is one even tone**: its walls, its ceiling and the painted wood on them. Light gives
  it its shade; the paint does not.
- Painted plaster in the surface's code has a number for it: how smooth, 0 the street's mottle to 1 one even tone of
  the same mean colour (the pattern's second number when its type is plaster). The street's walls stay at 0.
- The rooms have their own copy of the material, `M_Room`, made by `Tools/unreal/make_greybox_material.py` with
  `BRIX_MATERIAL=M_Room` from the same code. `M_Greybox`, the kit's meshes and everything out of doors are untouched.
- The golden room uses it: `WallSmooth=1.0` in `DefaultGame.ini` (`ABrixLightRoom`).
- ART_DIRECTION 1.14.0: I1 says so. Floors, cloth, wood furniture and everything out of doors keep W1.

## Consequences
- **Only the golden room has it.** The rooms of the town (`ABrixRooms`) and the inner faces of the buildings' own walls
  (`ABrixTownActor`) still draw the street's mottle. Carrying it over is a change to something every room shares, so
  it goes with the look tour, once the golden room's pictures are approved. Written in `Game/README.md`, Not yet.
- **The look check's flat-fill limit fails the hallway by day**: 55 % of the picture is plain where the limit is 45 %
  (it was 44 % with the mottle). The limit was set to catch a place with no shade at all. It is not changed here: what
  it should be for a room is to settle with the owner, and until then the hallway's "flat" failure is this decision,
  not a defect.
- No frame rate was gained or lost: the three ways ran within 2 to 3 frames a second of each other, which is what
  the same shot varies by from run to run. The mottle is still worked out and then dropped; a material with none of
  that code would save a little more, too little to measure here.
- Blender's room references keep their faint mottle. They say what a room is and its mood (ADR-0073), and the owner
  judged the plain walls beside them.
