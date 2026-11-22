---
adr: ADR-0029
title: The outlet's showroom has a room set for every furniture kit
status: proposed          # owner in chat on 2026-11-23: "expand furniture showroom to accomadate showcases for the more furniture" (kit 18, PR #4); the layout is for the owner to approve
date: 2026-11-23
relates_to: [FURN-020, FURN-021, FURN-003, FURN-005, ADR-0026, ADR-0027]
---

## Context
The outlet (ADR-0027) showed four room sets, kit 17's living room with a bed corner in each style line. Kit 18
(`Tools/blender/furniture_kit_18.py`, PR #4) added twelve kits (kitchen, bathroom, dining, study, children, hallway,
garden, shop, cafe, bedroom, living and decoration) and renders one furnished room per kit. The owner asked for the
showroom to show them. The hall was 66 m x 40 m with the sets in one row between the tills and the warehouse, and no room
for twelve more at the same size. Since the city remodel the outlet stands in block `b_14_8`, with its car park to the south.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| **A deeper hall, two rows of sets back to back** | One floor, as the render; every set full size (6.4 m x 4.4 m); still inside its block | The car park is 2 m shallower; the cafe moves away from the door |
| A showroom upstairs over the warehouse | As big outlets do it | A second storey, stairs and the upstairs light: much more work in the slice and in Unreal |
| Smaller sets in the old row | No change to the hall | Rooms too small to read as rooms; still not sixteen |

## Decision
A deeper hall with an island of sixteen sets, made as data by `Tools/worldgen/expand_outlet_01.py` (re-runnable; it finds
the outlet by its kind):
- The hall is 66 m x 46 m (6 m deeper). The car park moves 3 m south and is 20 m deep instead of 22 (same 14 cars).
- Sixteen sets in two rows of eight, back to back, 1 m between neighbouring walls. The north row opens north onto the
  first walk: kit 17's living room in Plain, Old Town, Workshop and Curve, then kit 18's living room, bedroom,
  children's room and study. The south row opens south onto the second walk: kitchen, dining room, bathroom, hallway,
  decoration, garden, cafe fittings and shop fittings. Each kit 18 room is in the line it was rendered in, furnished as the
  look test places it (its `ROOMS` table, read from the script and copied into the set's `pieces`).
- A painted route loops round the island (the lot's `route`). Two sales desks stand by each walk, the tills stay by the
  door, the cafe (8 seats) moves beside the warehouse, and the warehouse racks keep their three rows, now 6 m further south
  (the lot's `rack_rows`). What each staff place is (till, desk, cafe counter, packing table) is the lot's `fittings`.
- In the slice, `outlet.js` draws sets open either way, the kit 18 rooms through `furniture.js`, the route from data, and
  the fittings by kind. `furniture.js` gains box versions of the kit 18 pieces the rooms use, and which of them can be
  sat or lain on.

## Consequences
- 16 room sets and 71 customer places (four in front of and in each set, and seven by the racks) instead of 4 and 24;
  the same 10 staff.
- `test_FURN_020_showroom_shows_every_kit` checks every kit has its set, in its line, inside the hall and clear of the
  others, the walks and the fittings.
- `add_outlet_01.py` writes block `b_8_4`, which the remodel turned into homes; it now stops rather than overwrite them.
- Not done: a window in each set's side wall (the renders have one), tags that name each piece (FURN-022), and the box
  furniture's rounded edges and light (INTERIOR_STYLE §6).
