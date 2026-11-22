---
adr: ADR-0027
title: The furniture outlet stands outside the East Gate; the map's first block beyond the walls
status: accepted          # owner in chat on 2026-11-22: a large outlet "which looks like showroom.png", and for its place "Outside East Gate"
date: 2026-11-22
relates_to: [FURN-020, FURN-021, Q-30, CITY-068, WLD-050, ADR-0021, ADR-0026]
---

## Context
The owner asked for a large furniture outlet where players see furniture before buying it, looking like
`Art/StyleTests/furniture_kit_17/showroom.png`. Old Town is 8×8 blocks of 80 m inside its wall and every block is in
use; a hall with a car park needs about a whole block. Where it goes was open (Q-30).

## Options considered
| Option | Pros | Cons |
|---|---|---|
| **A new block outside the East Gate** | Keeps every home and park; a retail park on the road out of town is where such a shed stands | The map grows past its 8×8 grid for the first time; the wall needs a real gate |
| Replace North Park (b_3_0) | Cheapest | The town loses a park |
| Replace a row-house block | Inside the walls | About 27 residents and their homes' owners would have to move |

## Decision
Outside the East Gate (owner's choice). As data, made by `Tools/worldgen/add_outlet_01.py` (re-runnable):
- Block `b_8_4` in the grid cell east of East Wall Street, district "Outside the walls". The town wall crosses the west
  edge of that cell, so the outlet stands behind it: lot `b_8_4_l0`, kind `furniture`, 66 m × 40 m, one hall 8.5 m high,
  door on the East Gate road, a car park of 14 cars to the south. Every size is `[ASSUMPTION]`.
- Inside: four room sets, one per style line of ADR-0026, open to a painted walking route; sales desks between them;
  tills and a café by the door; three rows of warehouse racks of flat-pack boxes behind. Places for staff (10), customers
  (24) and café seats (8) are in the lot's `objects`, so residents use them like any other building.
- Ten commuters (who worked out of town) became its staff. Every resident knows it as their nearest `furniture`, and
  "browse the furniture outlet" is one more leisure outing, indoors on rainy days too (`World/rules/choices.json`): about
  40 to 100 visits a game day, near the museum's share.
- The East Gate road is 200 m instead of 80 so it runs past the outlet (`World/city_01/roads.json`, edited by the script).
- New kind `furniture` in `estate.json` (one showroom unit) and `storeys.json`.

In the slice (`Prototype/play/outlet.js`, new; small hooks in `play.js`, `cars.js`, `index.html`, `build_play.py`):
- The wall opens where a gate road leads to a block outside the walls: an opening as wide as the street, a stone band
  with the gate's name, a tower each side, and the road laid on through the fields. Only the East Gate has one today;
  cars at the other gates still leave through the wall, as before.
- The hall as in the render: stone base and plaster, tall arched windows, the name on a painted band, trusses, skylights,
  hanging lanterns, the sets in their lines with price tags and name boards. The car park uses the Soft box car models.
- Indoor light: a room may say how much more of the day it sees (`skylight`, 2.2 for the outlet `[ASSUMPTION]`), so a
  hall under skylights is lit like one. This stands in for sun through the skylights (INTERIOR_STYLE §6, R2), which the
  slice's indoor light still does not do.

## Consequences
- The outlet's look (INTERIOR_STYLE §4) is approved by the owner's request, as built.
- `test_world` allows blocks outside the walls beside the 64 inside them; FURN-020 and FURN-021 have tests.
- Not done: buying (FURN-022 onward), the other two companies and their outlets (FURN-030), the walkway round the
  outlet's block still runs along East Wall Street's carriageway on its west side, and the town map does not show the
  outlet's block (it draws only inside the walls).
- Regenerating `roads.json` with `make_roads_01.py` would put the East Gate road back to 80 m: run `add_outlet_01.py` again
  after it.
