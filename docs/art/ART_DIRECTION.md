---
doc_id: WASTED-ART-DIRECTION
title: Brix Town — Art direction (locked look)
version: 1.5.0
status: accepted          # owner approved the world look and character option B on 2026-11-21, the kit daylight on 2026-11-22, interiors and furniture on 2026-11-22
last_updated: 2026-11-22
relates_to: [FURN-003, ART-001, ART-002, ART-003, ART-004, ART-005, ART-006, CHR-007, Q-10, Q-15]
decision: docs/decisions/ADR-0004-art-direction.md, docs/decisions/ADR-0007-warmer-kit-light.md, docs/decisions/ADR-0013-kit-light-measured.md, docs/decisions/ADR-0026-interior-and-furniture-look.md
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

All in `Art/Reference/`. Originals and every earlier experiment stay in `Art/StyleTests/`.

| File | What it locks | Made by |
|---|---|---|
| `world_alley_morning_raw.png` | **The look.** Chosen by the owner as "style test 02, raw". Old-town alley, low sun through haze. | `Tools/blender/style_test_02.py` |
| `world_overview.png` | The same alley at morning and dusk plus a market square at midday: one place, readable at every hour. | `Tools/blender/old_town_04.py` |
| `world_street_kit_light.png` | **The daylight** (ADR-0007): a street of real lots built by the building kit, in the softer, warmer kit light. Forms and materials still follow the alley image. | `Tools/blender/building_kit_07.py` |
| `character_options.png` | Character options A, B, C side by side. **B "Soft" is the chosen one.** | `Tools/blender/old_town_04.py` |
| `character_lineup.png` | Six people in option B: two player examples and four NPC roles. | `Tools/blender/character_face_05.py` |
| `face_expressions.png` | Six expressions on one face. | `Tools/blender/character_face_05.py` |
| `face_mouth_shapes.png` | The eight speech mouth shapes. | `Tools/blender/character_face_05.py` |
| `furniture_lines.png` | **Furniture**: every piece of the four style lines Plain, Old Town, Workshop and Curve (ADR-0026). | `Tools/blender/furniture_kit_17.py` (`sheet`) |
| `interior_room_daylight.png` | **Interiors**: an Old Town living room with a bed corner, low sun through a side window, two warm lamps. | `Tools/blender/furniture_kit_17.py` (`room_oldtown`) |
| `face_talk.mp4` / `.gif` | A spoken line with lip sync, blink and brow movement. | `Tools/blender/character_face_05.py` |

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
| W7 | The same place must read clearly at **morning, midday and dusk**; only light and haze change. | `world_overview.png`. |
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
`world_street_kit_light.png` (ADR-0013): sun ×0.7 and slightly golden, sky fill ×1.25 by day, a mostly grey
bounce ×1.2, haze ×0.55 with a light veil over sky and distance, exposure ×0.74 (`LIGHTS.kit` in
`Prototype/play/play.js`). The exact measured match was too pale for the owner; it is kept as `?light=measured`. In shade, bounced
light comes only from sunlit ground nearby and things stand in contact shade (ADR-0018), so alleys go dark and warm as in
the alley image. Its cost: the light shafts in narrow alleys are fainter than in the alley image.

Common to all: AgX view transform with the "Medium High Contrast" look; haze scatters forward
(anisotropy 0.6); at dusk lanterns are doubled in strength and about half the windows are lit.

### 2.5 Vehicles — "Soft box" (ADR-0015, replacing "Classic" of ADR-0011)

Reference: `Art/Reference/vehicles_soft_box.png` (option A of `Tools/blender/car_kit_09.py`). The owner first chose C
"Classic" (`vehicles_classic.png`, kept for the record), then after playing with it in the slice preferred A.

| # | Rule | Check |
|---|---|---|
| V1 | Cars are **small and friendly soft boxes**: simple body shapes with **generously rounded edges and corners** everywhere, a soft waist above the body, round lamps, rounded bumpers, round tyres with pale hubs. No sharp edges, no flat slab sides (W3). | No corner reads as sharp from 5 m. |
| V2 | **Matte painted** like everything else (W1), one accent of the palette for the body and a **cream roof**, sometimes a cream belt stripe; vans carry a painted band on a rounded load box. No chrome shine, no glossy paint. | No highlights on the body. |
| V3 | Cabins are **open frames on round posts** with a rounded roof, so the people inside can be seen; seats and a steering wheel inside. | A seated driver shows from the chest up. |
| V4 | Character comes from extras, not from new shapes: roof racks with cases and bags, crates in a pickup bed. | Three cars parked together look like a family, not copies. |

### 2.6 Interiors and furniture (ADR-0026)

References: `Art/Reference/furniture_lines.png` and `Art/Reference/interior_room_daylight.png`. Style lines, the outlet and
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
| Vegetation, UI style | not started (vehicles: §2.5, interiors and furniture: §2.6) |
| More furniture kits and decoration (kitchen, bathroom, dining, study, children, hallway, garden, shop, café, bedroom, living, decoration) | not approved; renders of `Tools/blender/furniture_kit_18.py` in `Art/StyleTests/furniture_kit_18/` for the owner to choose from |
| Wear stages (I8) | not yet rendered. (The furniture outlet's look, `Art/StyleTests/furniture_kit_17/showroom.png`, was approved by the owner's request to build it, ADR-0027.) |
| Night (after dusk) and bad-weather looks | night: approved in the slice (ADR-0023, owner 2026-11-22), no reference image yet; pictures in `Art/StyleTests/night_sky_15/`. Rain, fog and overcast: approved in the slice (ADR-0016), no rule or reference image yet; must follow W5–W7 |
| The sky: blue overhead with drifting clouds | approved in the slice (ADR-0014); not in the reference renders, still needs a rule and a reference image |
| Whether the iris should be smaller than in `face_expressions.png` | owner to say |
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
