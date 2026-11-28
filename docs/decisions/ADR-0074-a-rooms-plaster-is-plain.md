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
- **At first only the golden room had it.** Carried to the town the same day, at the owner's word on the reference page's
  bakery and shop floor ("you have not used 0 paint like you did in the home rooms else where interiors"): the rooms of
  the town (`ABrixRooms`) and the inside faces of the buildings' own walls, their ceilings, a stair's spine wall and a
  lift's shaft (`ABrixTownActor`, in a group of their own so the street faces keep their mottle) use `M_Room` at
  `WallSmooth=1.0` (`[/Script/BrixTown.BrixRooms]`). Before and after, indoors and out: `Art/StyleTests/unreal_plain_plaster_59`.
  Eastgate Mall and the Furniture Outlet are not done; `Game/README.md`, Not yet, says what else is not.
- **The look check's flat-fill limit fails the hallway by day**: 55 % of the picture is plain where the limit is 45 %
  (it was 44 % with the mottle). The limit was set to catch a place with no shade at all. It is not changed here: what
  it should be for a room is to settle with the owner, and until then the hallway's "flat" failure is this decision,
  not a defect.
- No frame rate was gained or lost: the three ways ran within 2 to 3 frames a second of each other, which is what
  the same shot varies by from run to run. The mottle is still worked out and then dropped; a material with none of
  that code would save a little more, too little to measure here.
- Blender's room references keep their faint mottle. They say what a room is and its mood (ADR-0073), and the owner
  judged the plain walls beside them.
