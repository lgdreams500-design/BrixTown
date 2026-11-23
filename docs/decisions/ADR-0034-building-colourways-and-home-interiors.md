---
adr: ADR-0034
title: Colourways for every other building, and the row houses and flats furnished at four levels
status: accepted          # owner in chat on 2026-11-24, shown look tests building_styles_22, home_interiors_22 and house_outside_23: "Approve all"
date: 2026-11-24
relates_to: [BUILD-001, BUILD-002, BUILD-003, F-HOME-FURNISH, FURN-060, FURN-061, FURN-062, FURN-063, ADR-0004, ADR-0032, ADR-0033]
---

## Context
After approving the house colourways (ADR-0032) and the house plan with its furnishing levels (ADR-0033), the owner asked
for the same for the other building types, "Colourways and interiors", and to review them in Blender as for the houses.
Three look tests were made:
- `Tools/blender/building_styles_22.py`: every build type that is not a designed house (44 types in 17 groups: row house,
  court flats, residential tower, shop front, café front, plaza kiosk, workshop and works, office tower, clinic,
  hospital, civic stone building, showroom, Town Hall, Guild House, arcade and mall, depot hall, substation), each
  group in three colourways; sheets `Art/StyleTests/building_styles_22/sheet_1..3.png`.
- `Tools/blender/home_interiors_22.py` with `home_interiors_22_plan.py`: plans for the three row-house types
  (14 x 12 m on 2 and 3 storeys, 12 x 16 m on 2) and the 80 m2 and 160 m2 court flats, furnished in each line at the
  four levels of ADR-0033 and checked as the house was (every piece in its room, none overlapping, doorways, front door,
  stair and its foot and top clear, nothing tall in front of a window, every room reached, the largest household's beds
  at Bare); sheets `Art/StyleTests/home_interiors_22/sheet_<type>.png`.
- `Tools/blender/house_outside_23.py`: the twelve house colourways on the house at 12 x 9 m with the plan's windows.

## Decision
All of it, as rendered:
- **Colourways a, b and c for every group** (`World/rules/building_styles.json`, which names each build type's group).
  "a" is the group as the kit drew it; b and c use palette colours of ART_DIRECTION 2.3, mixes of two, the kit's own
  wood and sign tones, the four house roof pairs and two stone mixes (sandstone, pale stone). Shop and café fronts
  also take an awning (a pair of stripes).
- **The five home plans and their furnishing levels** (`World/rules/home_plans.json` "row_14x12_2", "row_14x12_3",
  "row_16x12_2", "flat_80", "flat_160"; written by `home_interiors_22_plan.py` when its checks pass). Pieces by level
  (each includes the one before):

  | Plan | 1 Bare | 2 Basic | 3 Homely | 4 Full |
  |---|---|---|---|---|
  | row_14x12_2 | 14 | 48 | 73 | 104 |
  | row_14x12_3 | 19 | 69 | 107 | 145 |
  | row_16x12_2 | 14 | 46 | 72 | 103 |
  | flat_80 | 11 | 27 | 31 | 38 |
  | flat_160 | 12 | 35 | 45 | 57 |

- **The house at 12 x 9 m outside**, in the twelve colourways of ADR-0032.
- New references: `Art/Reference/building_colourways_1.png` to `_3.png`, `home_interior_<plan>.png` for the five plans,
  and `house_colourways_12x9.png`.

## Consequences
- Which colourway a building gets, and which plan a home is furnished from, are picked once when it is made and kept in
  its lot, as for the houses (ADR-0032, ADR-0033); its furnishing level follows its household (FURN-063).
- The slice does not draw these yet: the town's buildings still take their colours from the kit by a hash of the lot
  (`play.js` `building()`), and row houses and flats are furnished by `play.js` `furnish()` and `layout()`. Moving them
  to `building_styles.json` and `home_plans.json` is the next step.
- Some designs are picks from kit parts where the kit had none of its own: the plaza kiosk, the civic stone building
  (bank, library, museum, police, schools) and the substation, which reads like a cottage; the fire station is drawn
  as the tram depot. They are approved as rendered; a design of their own needs a new look test.
- Not yet planned: the residential towers' floors (80 m2 tower flats are 5.78 x 13.85 m, another proportion) and the
  320 and 640 m2 flats. In the narrow flat rooms the main bedroom has no chest or rug and the single bedroom no desk;
  the 80 m2 flat's television stands on a side wall.
