---
adr: ADR-0048
title: Unreal's enterable buildings are hollow behind an open kit door, with clear windows on their hollow storeys
status: accepted
date: 2026-11-25
relates_to: [CITY-013, WLD-050, WLD-052, ADR-0004, ADR-0046]
---

## Context
The Unreal town laid every building as a solid core behind its kit walls (ADR-0046), so no door led anywhere. The browser
slice makes every enterable lot hollow on its ground floor (play.js building(): four walls with an open arched doorway at
the lot's door, a floor at 0.2 m, plaster inside, a ceiling), and the owner asked for the slice's interiors in Unreal. Doing
that raises three choices the slice does not settle: what the doorway looks like with the kit's doors, how the windows
look from inside (the kit's glass is a dark box made to sit in front of the solid core), and how a room is lit.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| **The kit's own doors standing open; the hollow storey's windows clear** | The same doors as outside (made by building_kit_07 `door()`, the leaf swung in); the room looks out at the street and the sun comes in, lit by Lumen | Four more kit pieces; a second group for each window piece |
| A doorway cut with no door | No new pieces | A different front from every closed building nearby |
| Keep the dark glass and give each window a glowing pane inside (the slice's "same window seen from inside: daylight beyond") | One material | Tried: blown-out white rectangles and no sunlight on the floor |

## Decision
- **Hollow ground storeys.** Every enterable lot but the furniture outlet's hall and the malls (`BrixIsHollow`) is built
  with its ground storey hollow: the kit walls as before, a floor in the storey's finish (boards or flags, BUILD-019),
  the walls plastered inside in the storey's colour, a ceiling 0.12 m under the next storey's line. Above it the core stays
  solid until the storeys are built (WLD-050 to WLD-053).
- **An open door at the lot's door.** `BrixDoorwayOf` (BrixWorld) puts the doorway where the residents' routes go in: at
  the lot's door on its nearest facade, a designed house's plan door, or the shopfront pane nearest the door. It is the
  kit's door of that building made open by `Tools/blender/unreal_kit_36.py` (`SM_Kit_DoorPlainOpen`, `DoorArchOpen`,
  `DoorCivicOpen`, `ShopDoorOpen`: the same frame, steps, hood and voussoirs, the leaf turned in against the reveal, an
  arch open to its crown). The other ground-storey windows of that facade that it would cut into are left out.
- **Windows are not ways in.** Every other opening of a hollow building has an unseen collision box in it.
- **Clear windows on a hollow storey.** Their glass (and a shop window's lit glass) is drawn with `/Game/Greybox/M_Clear`
  (masked to nothing; `Tools/unreal/make_clear_material.py`), so the room has the real view and the real sun. The upper
  storeys keep the dark glass in front of their core.
- **Little haze indoors.** The height fog is not shut out by walls; while the camera is in a hollow building the haze is
  turned down to `IndoorHaze` (0: the slice cuts its fog by 0.85 indoors, and anything above washed the room out).
- **The look.** The owner watched the test shots of walking into a row house, a shop and a designed house and said, in
  chat on 2026-11-25: "actually I like the look what i am seeing duing entring the room" and "if you retain same
  lighting and look and feel it should be good". That lighting is kept for the rooms that follow; the darker mood of
  `Art/Reference/interior_room_daylight.png` is not chased.

## Consequences
- 959 of the 965 enterable buildings can be walked into (`CITY013_every_enterable_building_has_a_doorway`: a doorway
  wider and higher than the player where they pass, at the lot's door, the stair's steps clear of it).
- About 72,000 more instances (floors, ceilings, linings, blockers) and five more groups; on the owner's laptop at High a
  street measured 46.7 to 48.6 fps against 45.8 to 49.5 with `-BrixShut` (the buildings solid as before): the same within
  the noise of other sessions running Unreal at the time.
- A test shot no longer takes the mouse and keys of whoever is at the machine (they swung the camera and once walked the
  player out of the room).
