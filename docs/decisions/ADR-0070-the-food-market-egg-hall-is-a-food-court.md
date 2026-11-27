---
adr: ADR-0070
title: Eastgate's Food Market egg hall is a food court
status: accepted          # the owner in chat, 2026-11-27: "needs tables and chair, even long tables with more chairs like in the mall. we can have addition food outlets skirting the round wall, like we have in many malls", then "structure is really good. Food stall at skirting the wall are really nice and matches my vision", then "Approve it"
date: 2026-11-27
relates_to: [ADR-0037, ADR-0038, ADR-0066, ADR-0069, WLD-059]
---

## Context
Each of Eastgate Mall's four eggs stands over a round hall that is a corner store (WLD-059, ADR-0038). The south-west
hall is the Food Market: in the plan and the game it has six produce stalls round a tree, and its round wall is bare.
Making its reference (ADR-0066: reference before build), the owner asked for tables and chairs, long tables among them,
and food outlets along the round wall, as in many malls' food courts.

## Decision
- The Food Market's hall is a food court, the reference `MallsEastgateFoodCourt` (`Art/Reference/Malls/`, made by
  `Tools/blender/hours_50.py` `mall_egg_hall()`, `egg_food_court()`, `egg_seating()`, its numbers in `MALL_LOOK`).
- The hall's shapes are the game's: the round hall, its drum walls, the walk in from the court and its balconies, the
  roof's eye and the egg, the six stalls and the tree, all from `World/city_01/blocks/b_16_5.json`.
- On top of those shapes it adds:
  - food outlets skirting the drum on the ground floor, about 15 of them, one between each two stone piers. Each has an
    open counter, a sign in its own colour with a backlit name, lit menu boards and its kitchen behind;
  - seating: long tables for ten in a ring between the stalls and the outlets, round tables for four between them and
    near the tree, with the ways to the doorway kept clear.
- The look is the court's (ADR-0069), taken warmer:
  - honed stone drums with a line of light round each floor;
  - timber fins round the upper drum;
  - warm polished stone bands in the floor;
  - the egg's lattice in gold;
  - uplit piers, warm kitchens and pendants over the long tables.
- It is lit by the town's one light, and its lamps are of the kinds in `World/rules/lights.json`.

## Consequences
- The plan has no units for the food outlets and no seating. Building the hall in the game to this reference needs them
  added to the block's plan, in `Tools/worldgen/mall_inside_01.py`. Until then it is noted in `Game/README.md` under
  "Not yet".
- The other three eggs keep their stores. One of them may become an adventure arcade: `F-BLD-ADVENTURE` is proposed in
  `docs/backlog/features.json`, and its level and egg are the owner's to pick.
