---
adr: ADR-0035
title: The house plan at the town's size and four furnishing levels
status: accepted          # owner in chat on 2026-11-23, shown house interiors look test 21: "Approve as rendered"
date: 2026-11-23
relates_to: [F-HOME-FURNISH, F-BLD-INSIDE, F-FURN-ITEMS, FURN-001, FURN-003, FURN-005, FURN-034, WLD-050, ADR-0026, ADR-0032]
---

## Context
The four sample houses (`Tools/blender/house_samples_19.py`) were drawn at 10 x 8 m, while every designed house in
World/city_01 is 12 x 9 m (`Tools/worldgen/add_houses_03.py`), and every house was furnished the same way, with rooms too
small for some sets (the dining table against the sideboard, the hall crowded by the stair). The owner asked for each
furniture set to be neatly placed in the right rooms, allowed the house and rooms to be resized, and asked for variety
"from bare essentials to fully flushed house".

`Tools/blender/furniture_sizes_21.py` measured every home piece of furniture kits 17 and 18 in each line.
`Tools/blender/house_interiors_21_plan.py` laid out the house at 12 x 9 m from those sizes and checked it (every piece in
its room, none overlapping, doorways, the front door and the stair foot clear, nothing tall in front of a window);
`house_interiors_21.py` rendered each line at each level, both storeys
(`Art/StyleTests/house_interiors_21/sheet_ground.png`, `sheet_upstairs.png`).

## Decision
The plan and furnishing of look test 21, as rendered:
- **The house is 12 x 9 m**, as in the town. The hall runs between x = -1.0 and 1.5 (the living room and kitchen 20 cm
  wider than before, so a whole kitchen run fits the back wall); the stair rises against the hall's right wall from
  y = 1.55 to the back. Ground floor: living room, kitchen, hall, dining room, bathroom. Upstairs: main bedroom, study,
  landing, children's room, guest room.
- **Four furnishing levels**, each adding to the one before: **1 Bare** (11 pieces: beds, sofa, a table to eat at,
  kitchen and bathroom), **2 Basic** (35: storage and comfort), **3 Homely** (60: rugs, shelves, television, pictures,
  hall furniture), **4 Full** (94: every piece of every set and the decoration).
- The layout is data: Art/StyleTests/house_interiors_21/layout.json now, moved into `World/rules/home_plans.json`
  for the game (FURN-060). Each line furnishes its own house.
- Which level a house shows follows its household (F-HOME-FURNISH level 3, the owner's pick of 2026-11-23): worked out
  from the household's money and the world clock, never ticked or stored.
- New references: `Art/Reference/house_interior_ground.png` and `house_interior_upstairs.png` (the two sheets).

## Consequences
- The town's houses get the new plan: room rects in their lots (`add_houses_03.py`), walls, stair and furniture in
  `Prototype/play/houses.js`, from the same data the Blender scripts read.
- Curtains (2.6 m wide) fit only single windows; rooms with two front windows have none. The main bedroom has no
  cheval mirror (no wall free of windows). A narrower curtain piece would close both.
- Row houses, court flats and towers get their own plans and the same four levels (owner, 2026-11-23: "Colourways and
  interiors"), each with its own look test before it goes into the slice.
