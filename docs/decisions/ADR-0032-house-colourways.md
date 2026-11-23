---
adr: ADR-0032
title: House colourways — three colour sets for each of the four house lines
status: accepted          # owner in chat on 2026-11-23, shown house styles look test 20: "ALL look good to me, so we can have all of these combinations available"
date: 2026-11-23
relates_to: [BUILD-001, BUILD-002, BUILD-003, FURN-005, ADR-0004, ADR-0007, ADR-0026]
---

## Context
The town's 469 independent houses are the four sample houses of `Tools/blender/house_samples_19.py`, one house line
each: **Plain**, **Old Town** (corner stones, door hood, window boxes), **Workshop** (dressed stone) and **Curve**
(arched ground floor). `Tools/worldgen/add_houses_03.py` gives every house its line, and each house keeps the trim
`add_houses_01.py` picked for it (shutter colour, window boxes, keystones, plaster and roof tone), stored in its lot.
Each line had one colour set only, and the colours the trim picks were never shown together on one house. The owner
wants to decide the colour sets each house type can use, to become `World/rules/house_styles.json`, and asked to see
variations within the art direction first.

`Tools/blender/house_styles_20.py` rendered each line in three colourways: "a", the line as house_samples_19 has it,
and "b" and "c", other sets. Every colour is a palette entry of ART_DIRECTION §2.3, a mix of two (the pale and the
ochre-washed plaster), or a roof tone of `World/rules/building_kit.json`. Each line keeps its own shape. Sheet:
`Art/StyleTests/house_styles_20/sheet.png`.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Keep one set per line | Nothing to change | 469 houses in four looks; streets of the same house |
| Pick one or two colourways per line | Fewer combinations to keep in step | The owner liked them all |
| **All twelve colourways** | Most variety from one palette; still one town, since every colour is the locked palette's | Twelve sets to carry into the slice and Unreal |
| New colours outside the palette | Walls could tell houses apart from afar | Breaks W4 and ADR-0004; needs new look tests |

## Decision
All twelve colourways are approved for the house lines. A colourway sets the walls (plaster pair, or the line's stone),
shutters, door, window frames and roof tiles; the line keeps its shape.

| Line | a | b | c |
|---|---|---|---|
| Plain | Teal on cool plaster, terracotta roof | Dusty blue on pale plaster, slate roof | Olive on warm plaster, brown roof |
| Old Town | Terracotta on warm plaster, brown frames | Ochre on ochre-washed plaster, brown door and frames | Plum on cool plaster, cream frames, dark roof |
| Workshop | Brown wood on stone, dark roof | Teal on stone, terracotta roof | Dusty blue on stone, brown door, slate roof |
| Curve | Dusty blue shutters, ochre door and frames, cool plaster | Cream shutters, terracotta door, warm plaster | Teal on pale plaster, brown roof |

The exact values are kept in `World/rules/house_styles.json`, which `house_styles_20.py` reads. New reference: `Art/Reference/house_colourways.png` (the
sheet). ART_DIRECTION gets §2.7 for houses.

## Consequences
- `World/rules/house_styles.json` holds these twelve sets and the lines' shapes, read by the slice's `houses.js`, by
  the Blender scripts and later by Unreal, so a colour is changed in one place.
- Which colourway a house gets is to be picked once, when the house is made, and stored in its lot, as the line and
  trim are now (`add_houses_03.py`); loading the game never changes it.
- In the renders every shutter of a house takes its colourway's colour. In the town kit about one shutter in twelve
  takes another accent (`building_kit_07.py` `own_shutter`); whether houses keep that mix is still to be decided.
- The wall plasters (cool, warm, pale, ochre-washed) differ only a little in daylight. Telling houses apart by wall
  colour from a distance would need stronger plasters, which is a change to the locked look: a new look test and the
  owner's approval.
- The furnished interiors of `house_interiors_21.py` (the house at 12 x 9 m, four furnishing levels) are not approved
  by this record.
