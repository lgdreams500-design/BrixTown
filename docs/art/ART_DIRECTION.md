---
doc_id: WASTED-ART-DIRECTION
title: Brix Town — Art direction (locked look)
version: 1.13.0
status: accepted          # owner approved the world look and character option B on 2026-11-21, the kit daylight on 2026-11-22, interiors and furniture on 2026-11-22, the menu's look on 2026-11-23, the house colourways and the house plan with its furnishing levels on 2026-11-23, every other building's colourways and the row houses and flats inside on 2026-11-24
last_updated: 2026-11-28
relates_to: [FURN-003, ART-001, ART-002, ART-003, ART-004, ART-005, ART-006, CHR-007, Q-10, Q-15]
decision: docs/decisions/ADR-0004-art-direction.md, docs/decisions/ADR-0007-warmer-kit-light.md, docs/decisions/ADR-0013-kit-light-measured.md, docs/decisions/ADR-0026-interior-and-furniture-look.md, docs/decisions/ADR-0033-menu-look-painted-signs.md, docs/decisions/ADR-0032-house-colourways.md, docs/decisions/ADR-0035-house-plan-and-furnishing-levels.md, docs/decisions/ADR-0036-building-colourways-and-home-interiors.md
---

# Brix Town — Art direction

This is the visual target for the game. Everything here was approved by the owner from Blender
renders. When a new asset, light setup or shader is made, compare it against the reference images in
`Art/Reference/` and the rules below. If the two disagree, the reference image wins and this file
gets corrected.

> **What these renders are and are not.** They are *look targets* built from simple shapes by script
> and rendered offline in Blender (Cycles). They are not game assets. Production models, materials
> and Unreal lighting still have to be made to match them (Q-15).

## 1. Reference images (the source of truth)

Every approved reference is in `Art/Reference/<Category>/`, named `<Category><Subject>[_<Variant>].<ext>`, and listed in
**`Art/Reference/catalog.json`**: what each locks, its approval and decision, the script that makes it, the game models
built from it, issues seen in it, and its old names (ADR-0061). The owner reviews them in `Art/Reference/index.html`,
which `python Tools/reference_catalog.py` writes from the catalog after checking it. Categories: World, Buildings,
Indoor, Shops, Malls, Furniture, Ornaments (light fittings first), Vehicles, People, Interface. References taken out of use are in `Art/Retired/`; look
tests and every earlier experiment stay in `Art/StyleTests/`.

Rejected directions, kept only so they are not tried again: the first 2D sketches, the painterly
filter (`style_test_02_painted.png`), and the three "playful" looks (`look_test_03_*`).

## 2. The world

### 2.1 In one sentence
A warm, sun-worn old town of sandstone blocks and patchy plaster, seen in low light through a soft
haze, with small pools of lantern light in the shade.

### 2.2 Rules
| # | Rule | Check |
|---|---|---|
| W1 | Surfaces are **matte and mottled**: two close tones of one colour blotched together, never a flat fill and never glossy. | Roughness about 0.9; no visible highlights on walls or cloth. |
| W2 | Stone is laid in **visible large blocks** with dark joints; plaster above it is smooth and patchy. | Block pattern readable from 20 m. |
| W3 | Shapes are **chunky and slightly soft**: bevelled edges, thick walls, deep window reveals, round arches. | No knife-sharp edges; no thin planes. |
| W4 | Palette is **muted and warm**. Colour comes from a few dull accents (shutters, awnings, cloth), never from saturated walls. | See §2.3. |
| W5 | Light is **low and directional**, warm sun against cool sky fill, with **haze** that lightens distance and shows light shafts. | Far end of a street is visibly paler than the near end. |
| W6 | **Lanterns and lit windows** give warm pools in shade and carry the scene at dusk. | At dusk the street is still readable without the sun. |
| W7 | The same place must read clearly at **morning, midday and dusk**; only light and haze change. | `Art/Reference/World/WorldAlley_*.jpg`, `WorldMarketSquare_*.jpg` (every hour and weather). |
| W8 | **No painterly or outline post-process.** The image is the plain render. | Owner chose the raw render over the filtered one. |

### 2.3 Palette
Values are linear RGB as used in the scripts (0–1). Convert when authoring textures.

| Name | Linear RGB | Used for |
|---|---|---|
| Stone | (0.27, 0.26, 0.30) ↔ (0.52, 0.48, 0.43) | Block walls |
| Stone dark | (0.18, 0.18, 0.22) ↔ (0.37, 0.34, 0.32) | Ledges, frames, sills |
| Plaster | (0.46, 0.43, 0.40) ↔ (0.68, 0.63, 0.55) | Upper walls |
| Plaster warm | (0.50, 0.40, 0.33) ↔ (0.70, 0.58, 0.46) | Variation between houses |
| Paving | (0.22, 0.21, 0.25) ↔ (0.46, 0.42, 0.37) | Streets |
| Ochre | (0.52, 0.40, 0.20) | Accent |
| Terracotta | (0.44, 0.24, 0.18) | Accent |
| Teal | (0.20, 0.36, 0.37) | Accent |
| Cream | (0.66, 0.61, 0.50) | Accent, cloth |
| Dusty blue | (0.30, 0.38, 0.52) | Accent, cloth |
| Brown / Olive / Plum | (0.24, 0.16, 0.11) / (0.30, 0.33, 0.20) / (0.30, 0.24, 0.34) | Wood, cloth |
| Charcoal / Indigo | (0.09, 0.09, 0.11) / (0.10, 0.14, 0.24) | Dark cloth |
| Lantern | (1.0, 0.55, 0.22) emissive | Lamps, lit windows |

"A ↔ B" means the surface is a blotchy mix of the two tones (the `painted()` material).

### 2.4 Light recipes
From the scripts; use as starting values when building the Unreal sky and sun for each hour.

| Time | Sun direction (towards sun) | Sun colour | Sun strength | Sky colour | Sky strength | Haze density | Exposure |
|---|---|---|---|---|---|---|---|
| **Daylight, kit (ADR-0007, the default)** | low and raking along the facades, e.g. (0.78, −0.40, 0.48) | (1.0, 0.86, 0.68) | 8.5 | (0.42, 0.55, 0.82) | 0.35 | 0.006 (street scale) | −0.15 |
| Morning (the alley image; earlier default) | (0.446, 0.594, 0.669) | (1.0, 0.86, 0.68) | 14.0 | (0.42, 0.55, 0.82) | 0.35 | 0.022 | +0.3 |
| Dusk | (0.42, 0.83, 0.36) | (1.0, 0.42, 0.22) | 2.5 | (0.06, 0.10, 0.30) | 0.6 | 0.014 | +0.2 |
| Character sheets | (−0.45, −0.50, 0.74) | (1.0, 0.82, 0.62) | 3.2 | (0.42, 0.55, 0.82) | 0.25 | 0.012 or none | −0.3 |

In the playable slice the kit daylight is the reference recipe scaled towards the colours measured in
`Art/Reference/World/WorldDaylight.png` (ADR-0013): sun ×0.7 and slightly golden, sky fill ×1.25 by day, a mostly grey
bounce ×1.2, haze ×0.55 with a light veil over sky and distance, exposure ×0.74 (`LIGHTS.kit` in
`Prototype/play/play.js`). The exact measured match was too pale for the owner; it is kept as `?light=measured`. In shade, bounced
light comes only from sunlit ground nearby and things stand in contact shade (ADR-0018), so alleys go dark and warm as in
the alley image. Its cost: the light shafts in narrow alleys are fainter than in the alley image.

Common to all: AgX view transform with the "Medium High Contrast" look; haze scatters forward
(anisotropy 0.6); at dusk lanterns are doubled in strength and about half the windows are lit.

### 2.5 Vehicles — "Soft box" (ADR-0015, replacing "Classic" of ADR-0011)

Reference: `Art/Reference/vehicles_soft_box_2.png` (option A of `Tools/blender/car_kit_09.py`; ADR-0057 opened its cabins
down to their floor, where `Art/Retired/vehicles_soft_box.png` had a board over the seats). The owner first chose C
"Classic" (`Art/Retired/vehicles_classic.png`, kept for the record), then after playing with it in the slice preferred A.

| # | Rule | Check |
|---|---|---|
| V1 | Cars are **small and friendly soft boxes**: simple body shapes with **generously rounded edges and corners** everywhere, a soft waist above the body, round lamps, rounded bumpers, round tyres with pale hubs. No sharp edges, no flat slab sides (W3). | No corner reads as sharp from 5 m. |
| V2 | **Matte painted** like everything else (W1), one accent of the palette for the body and a **cream roof**, sometimes a cream belt stripe; vans carry a painted band on a rounded load box. No chrome shine, no glossy paint. | No highlights on the body. |
| V3 | Cabins are **open frames on round posts** with a rounded roof, so the people inside can be seen; seats and a steering wheel inside, the seats on the floor of a **cabin well** cut out of the body (nothing over the lap), under a roof the tallest rider's head clears (ADR-0057). | A seated driver shows from the chest up from the side, and to the knees from above; no head touches the roof. |
| V4 | Character comes from extras, not from new shapes: roof racks with cases and bags, crates in a pickup bed. | Three cars parked together look like a family, not copies. |

### 2.6 Interiors and furniture (ADR-0026)

References: `Art/Reference/Furniture/FurnitureLines.png`, and for a home's rooms `Art/Reference/Indoor/IndoorHomeRoom_*` and
`IndoorHallway_*` (every hour; `IndoorRooms` and `interior_room_daylight.png` are archived in `Art/Retired/`, the owner, 2026-11-26). Style lines, the outlet and
how the renders get their look (and what the slice and Unreal must do to match it): `docs/art/INTERIOR_STYLE.md`.

| # | Rule | Check |
|---|---|---|
| I1 | **Matte and mottled, like the streets** (W1): wood, cloth, paint and tile use the `painted()` material. No gloss, no chrome, no shiny glass; metal is dull dark iron or painted. | Roughness about 0.9; no highlights on any piece. |
| I2 | **Chunky and soft** (W3): tops at least 4 cm thick, every edge bevelled at least 1 cm, fat cushions, block or turned legs. | No leg under 4 cm across; no edge reads sharp from 3 m. |
| I3 | **True size, readable details**: pieces true size for people 1.58–1.80 m (seats about 45 cm, tables 75 cm, beds 50 cm); handles, knobs, buttons and seams about 1.3× real. | A character sits with feet on the floor. |
| I4 | **The world palette indoors** (§2.3): woods from Brown, Ochre and Cream, cloth from the accents, walls in Plaster, floors in boards, flags or tiles. | Every catalogue colour is a palette entry or a mix of two. |
| I5 | **Light from outside and from lamps** (W5, W6): low sun through deep windows lays a patch on the floor; every room has a lamp in the Lantern colour that carries it at dusk. | A room reads at dusk with the sun gone. |
| I6 | **One kit, four lines**: Plain, Old Town, Workshop and Curve share the kit of parts and the materials and differ by silhouette, proportion and accent only. | Two lines side by side look like the same town. |
| I7 | **Silhouette first**: a sofa, bed, wardrobe and dresser are told apart by outline from 6 m; colour variants of one piece clearly differ from 6 m. | The sheet still reads in grey. |
| I8 | **Wear shows**: used pieces have paler edges and stronger mottling, worn ones a sagging cushion and a chipped corner. | Three stages side by side (not yet rendered). |
| I9 | **Boxes belong too**: flat-pack boxes are mottled cardboard (Cream ↔ Ochre) with the company's painted band. | A stack reads as one company's. |

### 2.7 The interface — "Painted signs" (ADR-0033)

**In the Unreal build the interface is "Lamplight" (ADR-0049, accepted by the owner 2026-11-25: "accept lamplight").**
Reference: look test 36, `Art/StyleTests/lamplight_menu_36/` (the menu's pages and a shop's counter, before and after). It
covers the game menu, the counters and the buy menu; the conversation box, the purse, the tracker and the toasts are not
brought to it yet. Painted signs, below, stays the look of the frozen browser slice (ADR-0045).

| # | Rule | Check |
|---|---|---|
| L1 | **Cards of warm dark stone**, corners rounded 10 to 18 px, a darker lower edge 4 to 5 px deep so each reads as a thick slab (W3); a hairline of warm light round the edge; no gloss, no gradient that reads as shine (W1). | No sharp corner and no highlight on the screen. |
| L2 | **The town stays in sight**: behind the menu it is dimmed and warmed by a brown veil, not blurred or hidden (W6). | The menu reads the same at noon and at night. |
| L3 | **Page colours from the palette** (§2.3, U1's colours) as the lintel over a page's card, the band of its header and its mark in the rail; the open page is lit in its colour. | Two pages are told apart by colour alone. |
| L4 | **One main action**, filled lantern orange with dark letters on a darker edge; the rest outlined in cream (U6). | Never two filled buttons side by side. |
| L5 | **Prices on paper tags**, dark letters on paper; a price the purse cannot meet on a red tag. Keys shown as small paper keycaps. | Every price on the screen is a tag. |
| L6 | **Goods on plinths in lantern light**: in a buy menu each thing stands on a dark plinth in a pool of lantern light, the chosen one ringed in lantern (ADR-0044's layouts unchanged). | No thing floats on a flat grey. |
| L7 | **Markers are brass tokens** (ADR-0051, look test 37): a coin with the kind's sign stamped dark in it, ringed in the kind's colour, in a warm halo; one sign per kind of thing to do, so they read without colour; a spark far off, a caption with the key near; new things pulse. Numbers, colours and signs in `World/rules/markers.json`. | No marker is words alone; no two kinds share a shape. |

The colours are data, `World/rules/settings.json` `menu.lamplight` (stone `#2b2723`, its edge `#1e1b18`, cream `#efe4cc`,
lantern `#e0964e`, paper `#e9dcbe` and the rest), checked by world.settings_problems().

The rules below are Painted signs, the slice's.

Reference: sample C of look test 19, `Art/StyleTests/menu_look_19/index.html` (a page, not a render: open it in a browser),
as built in the slice's menu. It covers the game menu; the counters, the conversation box and the building designer have
not been brought to it yet.

| # | Rule | Check |
|---|---|---|
| U1 | **Boards, not windows**: each part of the player's life is a painted board in one accent of the palette (§2.3) as it looks in shade, with cream lettering (dark on Lantern). A page keeps its board's colour in its header and its main buttons. | Two pages are told apart by colour alone. |
| U2 | **Matte and mottled** (W1): boards and cards carry the same blotchy two-tone paint as the walls. No gloss, no gradients that read as shine, no glow. | No highlight on any board. |
| U3 | **Chunky and soft** (W3): corners rounded 9 to 18 px, a darker lower edge for thickness, no hairline frames. | No sharp corner on the screen. |
| U4 | **Dusk behind**: the game is veiled dark and blurred, the boards are the colour (W4, W6). Body cards are dark stone with cream text. | The menu reads the same over morning, midday, dusk and night. |
| U5 | **Two voices**: names and headings in the serif (Constantia), figures and controls in the sans (Bahnschrift); money in tabular figures. | No third typeface. |
| U6 | **One main action**: in a row one button is filled in the page's colour, the others outlined; what cannot be undone is outlined in red. | Never two filled buttons side by side. |

Board colours (sRGB, board then lettering), in `World/rules/settings.json` `menu.colours`: Lantern `#e0964e`/`#1c1b21`,
Ochre `#a8843f`, Terracotta `#a35a44`, Teal `#3f7273`, Dusty blue `#55698c`, Plum `#6c5578`, Olive `#6b7245`,
Brown `#77583f`, Stone `#6a6871` (all with `#fbf3e0`), and the quiet board `#3b3c4a`/`#e8dfc9`.

### 2.8 Houses (ADR-0032)

Reference: `Art/Reference/Buildings/BuildingsHouseLines.png`. Each of the four house lines comes in three colourways; a colourway
sets the walls (a plaster pair from §2.3, or the line's stone), the shutters, door, window frames and roof tiles, and
the line keeps its shape (Old Town's corner stones, door hood and window boxes; Workshop's dressed stone; Curve's
arched ground floor). The values are kept in `World/rules/house_styles.json`.

| Line | a | b | c |
|---|---|---|---|
| Plain | Teal, cool plaster, terracotta roof | Dusty blue, pale plaster, slate roof | Olive, warm plaster, brown roof |
| Old Town | Terracotta, warm plaster | Ochre, ochre-washed plaster | Plum, cool plaster, dark roof |
| Workshop | Brown wood, stone, dark roof | Teal, stone, terracotta roof | Dusty blue, stone, slate roof |
| Curve | Dusty blue and ochre, cool plaster | Cream and terracotta, warm plaster | Teal and cream, pale plaster, brown roof |

Inside (ADR-0035; references `Art/Reference/Indoor/IndoorHouse12x9_Ground.png` and `Art/Reference/Indoor/IndoorHouse12x9_Upstairs.png`), each line furnishes its own
house, from the plan in `World/rules/home_plans.json`, at one of four levels that add to each other: Bare (beds, sofa, a
table to eat at, kitchen, bathroom), Basic (storage and comfort), Homely (rugs, shelves, pictures, the hall) and Full (every
piece and the decoration). Rules I1-I9 hold at every level.

### 2.9 Every other building (ADR-0036)

References: `Art/Reference/Buildings/BuildingsTypes_1.png` to `_3.png`, `Art/Reference/Indoor/IndoorRowHouse<plan>.png` and
`Art/Reference/Indoor/IndoorFlat<area>.png`. Each build type that
is not a designed house belongs to one of 17 groups of the town kit, and each group comes in three colourways (walls,
shutters, door, frames, roof, and for shop and café fronts an awning); the values are kept in
`World/rules/building_styles.json`. Row houses and court flats are furnished from their plans in
`World/rules/home_plans.json` at the four levels of 2.8.

## 3. Characters — option B "Soft"

### 3.1 In one sentence
Soft, slightly toy-like people about 5.6 heads tall, with a larger head and big readable eyes,
dressed in whole garments made of the same mottled matte material as the town.

### 3.2 Proportions (relative to a natural adult = 1.0)
| Parameter | Value | Meaning |
|---|---|---|
| Head scale | 1.22 | Larger head so faces read at third-person distance |
| Leg length | 0.95 | Slightly shorter legs |
| Torso length | 0.98 | |
| Limb thickness | 1.12 | Chunkier arms, legs, hands, feet |
| Body width | 1.04 | |
| Eye size | 1.15 | Full eyes: white, iris, pupil, highlight |
| Nose size | 0.85 | Small, simple |
| Height range in the cast | 1.58 m – 1.80 m | Elder to tall player |

### 3.3 Rules
| # | Rule |
|---|---|
| C1 | **Whole garments**: a tee, tunic, coat or dress is one soft form. No visible doll joints, no separate chest/waist blocks. |
| C2 | Cloth uses the **same mottled paint** as walls, in the world palette (§2.3). People belong to the town. |
| C3 | Skin is **matte**, one tone per person, with a wide range of tones across the population. |
| C4 | Hair is a **solid sculpted shape** (cap, bob, bun, afro, long locks), not strands. |
| C5 | Silhouette says who someone is: apron for a shopkeeper, cap and bag for a courier, long coat for an office worker, scarf and bun for an elder. |
| C6 | Faces are built for animation first: see `docs/art/CHARACTER_FACE.md`. |
| C7 | Player and NPC use the **same body and head**; the character designer changes values, not the model. |
| C9 | **Age shows** (ADR-0059): lines at the brow, the eyes and from nose to mouth are soft creases a little darker than the person's own skin, never black; lips pale with age; a stoop from the hips; thin hair hugs the head. **Hair sits on the skull**: a curved hairline, locks, no shell standing off the head, because the shade under a shell's edge reads as a cap. Checked close up by `Tools/blender/look_check.py`. |
| C8 | **Rosy cheeks** on every face (ADR-0010): a soft oval below and outside each eye, a redder version of the person's own skin tone. Taken from option C; everything else stays option B. |

### 3.4 The designer values already in use
Every person in the tests is one function called with a small set of values. This is the seed of the
character designer's data (CHR-002).

`height`, `build`, `skin`, `hair` (short, side, long, bob, bun, afro, bald), `hair_color`, `eyes`
(iris colour), `eye_size`, `eye_open`, `face_w`, `nose`, `brow`, `lips`, `garment` (tee, long, tunic,
dress), `top`, `bottom`, `bottom_type` (trousers, shorts), `coat` + `coat_len`, `dress_len`, `scarf`,
`shoes`, `extras` (apron, moustache, cap, bag).

The six reference people (`CAST` in `old_town_04.py`): PlayerA, PlayerB, Shopkeeper, Courier, Elder,
Office.

## 4. How to reproduce a reference render

1. Open Blender (5.3 was used), open the Python console (`Shift+F4`).
2. Run: `exec(compile(open("C:/Projects/Wasted/Tools/blender/<script>.py").read(), "<script>.py", "exec"))`
3. Blender is busy until the script ends. Progress is written to `Art/StyleTests/<script>_log.txt`;
   images go to `Art/StyleTests/`, the `.blend` to `Art/Source/StyleTests/`.

Scripts build on each other by loading the helper part of an earlier script, so keep all of
`Tools/blender/` together:
`style_test_02.py` → `character_test_02.py` → `old_town_04.py` → `character_face_05.py`; and
`old_town_04.py` → `building_kit_07.py` → `car_kit_09.py` → `furniture_kit_17.py` → `furniture_kit_18.py`.

## 5. Not decided yet

| Item | Where tracked |
|---|---|
| How production character models are made | Q-15 |
| Vegetation | not started (vehicles: §2.5, interiors and furniture: §2.6, the interface: §2.7) |
| The interface outside the menu (conversation box, building designer, the lettering over the game) | still the earlier dark-box look; the counters and the buy menu are Lamplight in the Unreal build (§2.7, ADR-0049); whether the rest follow is for the owner to say |
| More furniture kits and decoration (kitchen, bathroom, dining, study, children, hallway, garden, shop, café, bedroom, living, decoration) | the twelve rooms of `Tools/blender/furniture_kit_18.py` were a reference from 2026-11-26 and are archived the same day (`Art/Retired/IndoorRooms_<Kind>.png`, the owner: "they are bad"); the pieces in them are not yet approved one by one |
| Wear stages (I8) | not yet rendered. (The furniture outlet's look, `Art/Reference/Shops/ShopsFurnitureOutlet.png`, was approved by the owner's request to build it, ADR-0027.) |
| Night (after dusk) and bad-weather looks | references at every hour and weather since 2026-11-26: `WorldAlley`, `WorldMarketSquare`, `WorldStreet` (night with the lamps on and off under a full and a new moon; cloudy, rain, fog, snow), made by `Tools/blender/hours_50.py`; rules W5–W7 |
| The sky: blue overhead with drifting clouds | reference `WorldSky` at every hour and weather (2026-11-26; no planet, ADR-0064) |
| Big modern buildings outside Old Town (Eastgate Mall's ribbon walls and pierced eggs) | built in the slice at the owner's request, in the locked materials: mottled plaster, ochre and cream, lantern light (ADR-0037). No rule or reference image yet for the outside. Eastgate's Great Court inside has a reference and a look of its own (`MallsEastgateCourt`, ADR-0069): timber, cream, glass, brass and polished stone; that look is the court's, not the town's. Its shop signs are coloured glass, raked back and lit, an exception to W1 and U2 for Eastgate's signs only (`MallsEastgateShop`, ADR-0071) |
| Whether the iris should be smaller than in `Art/Reference/People/PeopleFaceExpressions.png` | owner to say |
| How the look is achieved in Unreal on modest hardware (haze, soft light) | PH1 task |

## 6. Change log
| Version | Date | Change |
|---|---|---|
| 1.0.0 | 2026-11-21 | First version: world look and character option B recorded from the approved renders. |
| 1.1.0 | 2026-11-22 | Daylight changed to the kit light (ADR-0007): new reference `world_street_kit_light.png` and light recipe row. |
| 1.2.0 | 2026-11-22 | Rule C8: option B gets rosy cheeks (ADR-0010). The B reference images predate it and show none. |
| 1.3.0 | 2026-11-22 | Vehicles: the owner chose "Classic" from car look test 09 (ADR-0011): §2.5 rules V1-V4, reference `vehicles_classic.png`. |
| 1.3.1 | 2026-11-22 | §2.4: the slice's kit light retuned towards the measured colours of the reference street (ADR-0013). No rule or reference changed. |
| 1.4.0 | 2026-11-22 | Vehicles: the owner switched to A "Soft box" after playing with the Classic cars (ADR-0015): §2.5 rewritten, new reference `vehicles_soft_box.png`. |
| 1.5.0 | 2026-11-22 | Interiors and furniture: the owner chose all four style lines from furniture look test 17 (ADR-0026): §2.6 rules I1-I9, references `furniture_lines.png` and `interior_room_daylight.png`. |
| 1.6.0 | 2026-11-23 | The interface: the owner chose "C, Painted signs" from menu look test 19 (ADR-0033): §2.7 rules U1-U6. |
| 1.7.0 | 2026-11-23 | Houses: the owner approved all twelve colourways of house styles look test 20 (ADR-0032): §2.8, reference `house_colourways.png`. |
| 1.8.0 | 2026-11-23 | Houses inside: the owner approved house interiors look test 21 (ADR-0035): §2.8, references `house_interior_ground.png` and `house_interior_upstairs.png`. |
| 1.9.0 | 2026-11-24 | Every other building's colourways and the row houses and flats inside: the owner approved look tests building_styles_22, home_interiors_22 and house_outside_23 (ADR-0036): §2.9 and nine references. |
| 1.10.0 | 2026-11-26 | Old age, Noor and the Stone Oven inside (ADR-0059): rule C9, references `character_elders.png`, `character_noor.png`, `bakery_inside_day.png`, `bakery_inside_four.png`. |
| 1.9.1 | 2026-11-24 | §5: Eastgate Mall's sculpted shell noted as built in the locked materials, with no rule or reference image yet (ADR-0037). No rule or reference changed. |
| 1.12.0 | 2026-11-26 | Eastgate's Great Court has a look of its own, approved by the owner (`MallsEastgateCourt`, ADR-0069). §5: night, weather and the sky now have references at every setting. |
| 1.13.0 | 2026-11-28 | Eastgate's shop signs are coloured glass, raked back, lit from a brass trough, their names backlit: an exception to W1 and U2 for the mall's signs only, approved by the owner (`MallsEastgateShop`, ADR-0071). |
| 1.11.2 | 2026-11-26 | `IndoorRooms` archived (the owner: "they are bad"); a home's rooms are `IndoorHomeRoom` and `IndoorHallway`. |
| 1.11.1 | 2026-11-26 | W7's check is the alley and Market Square at every hour and weather (`WorldAlley`, `WorldMarketSquare`); `WorldHours.png` retired. |
| 1.11.0 | 2026-11-26 | References organised (ADR-0061): a folder per category, every file named `<Category><Subject>[_<Variant>]`, one list in `Art/Reference/catalog.json` and a page to review them (`Art/Reference/index.html`, written by `Tools/reference_catalog.py`); §1's table replaced by it. The kit-18 rooms are references (`IndoorRooms`); `interior_room_daylight.png`, `vehicles_soft_box.png` and `vehicles_classic.png` retired to `Art/Retired/`. |
