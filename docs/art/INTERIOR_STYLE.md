---
doc_id: WASTED-ART-INTERIOR-STYLE
title: Wasted — Interior and furniture style
version: 1.0.0
status: accepted          # owner chose all four lines from furniture look test 17 on 2026-11-22 (ADR-0026); §4 the outlet on 2026-11-22 (ADR-0027)
last_updated: 2026-11-22
relates_to: [FURN-002, FURN-003, FURN-005, FURN-008, FURN-020, WLD-050]
extends: docs/art/ART_DIRECTION.md     # rules W1-W8 and the palette §2.3 still apply indoors
decision: docs/decisions/ADR-0026-interior-and-furniture-look.md
---

# Interior and furniture style

> **Status: accepted** (ADR-0026). The owner chose **all four lines** from the renders of
> `Tools/blender/furniture_kit_17.py` (in `Art/StyleTests/furniture_kit_17/`). Rules I1–I9 are in force from
> ART_DIRECTION §2.6, which wins if the two differ; references `Art/Reference/furniture_lines.png` and
> `Art/Reference/interior_room_daylight.png`. §4, the outlet, was approved when the owner asked for it to be built like `showroom.png` (ADR-0027); it is in the slice (`Prototype/play/outlet.js`).

## 1. In one sentence

Inside is the same warm, sun-worn old town: chunky, soft-edged, matte furniture in worn wood, dull
cloth and painted panels, in rooms lit by low sun through deep windows and warm pools of lamplight.

## 2. Rules

| # | Rule | Check |
|---|---|---|
| I1 | **Matte and mottled, like the streets** (W1). Wood, cloth, paint and tile use the two-tone `painted()` material. No gloss, no chrome, no glass that shines; metal is dull dark iron or painted. | Roughness about 0.9; no highlights on any piece. |
| I2 | **Chunky and soft** (W3). Tops at least 4 cm thick, every edge bevelled at least 1 cm, fat cushions, block or turned legs. Nothing spindly or knife-thin. | No leg under 4 cm across; no edge reads sharp from 3 m. |
| I3 | **True size, readable details.** Pieces are true size for people 1.58–1.80 m tall (seats about 45 cm, tables about 75 cm, beds about 50 cm), but handles, knobs, buttons and seams are about 1.3× real so they read at third-person distance, like the larger heads and eyes of option B. | A character sits with feet on the floor and elbows on the table. |
| I4 | **The world palette indoors** (§2.3). Woods from Brown, Ochre and Cream; cloth from the accents (Teal, Terracotta, Dusty blue, Olive, Plum, Cream, Indigo); walls in Plaster and Plaster warm; floors in boards, stone flags or tiles. Colour comes from cloth and painted doors, never from saturated walls or bright plastic. | Every colour in the catalogue is a palette entry or a mix of two. |
| I5 | **Light from outside and from lamps** (W5, W6). Low sun through deep windows lays shafts across the floor; every room has at least one lamp in the Lantern colour, and at dusk the lamps carry the room. | A room reads at dusk with the sun gone. |
| I6 | **One kit, many lines.** Every style line uses the same kit of parts and the same materials (FURN-002); lines differ by silhouette, proportions and accents, not by new materials. | Two lines side by side look like the same town. |
| I7 | **Silhouette first.** A sofa, a bed, a wardrobe and a dresser are told apart by outline from 6 m; colour variants of one piece are clearly different from 6 m. | Contact sheet in grey still reads. |
| I8 | **Wear shows** (FURN-008): used pieces have paler edges and stronger mottling, worn ones a sagging cushion and a chipped corner. Never dirty or broken-looking unless damaged. | Three stages side by side. |
| I9 | **Boxes belong too.** Flat-pack boxes are mottled cardboard (Cream ↔ Ochre) with the company's painted band and a simple drawing of the piece; size follows the piece. | A stack of boxes reads as one company's. |

## 3. Style lines (all four chosen, ADR-0026)

| Line `[ASSUMPTION: names]` | Silhouette | Woods and cloth | Sold by (FURN-030) |
|---|---|---|---|
| **Plain** | Flat slabs, square legs, simple box cushions | Pale wood (Cream ↔ Ochre), one accent cloth | Budget flat-pack chain |
| **Old Town** | Turned legs, panelled doors, rolled arms, buttoned backs | Mid and dark wood (Brown), Terracotta, Olive, Plum | Mid-range chain and the design house |
| **Workshop** | Thick dark tops on dull iron frames, crates and trunks | Dark wood (Brown ↔ Charcoal), iron, Cream canvas | Mid-range chain |
| **Curve** | Rounded corners, round tables, tub chairs, arched headboards (matches the soft-box cars, V1) | Ochre and Cream wood, Teal and Dusty blue cloth | The design house |

The look test renders each line as one furnished room set (living room and bedroom) and the whole
basic set as a grey contact sheet, in the kit daylight and at dusk.

## 4. The outlet (FURN-020) — approved by the owner's request to build it like `showroom.png` (ADR-0027)

The huge outlet keeps the town's look (W1–W4): a long sandstone and plaster shed with tall round-arched
windows, a timber-trussed roof with skylights, and a big painted sign and band in the company's accent.
Inside, the showroom is lit warmly and evenly from skylights and hanging lanterns; room sets are three
walls of plaster with a window and a lamp each, so furniture is seen as it would be at home. The
warehouse hall has tall timber racks of boxes. No fluorescent white light, no shiny floors.

## 5. Questions for the owner

| Question | Default |
|---|---|
| Are four lines right? | **Decided 2026-11-22:** all four (ADR-0026). |
| Should the handles and details be larger than real (I3)? | **Decided 2026-11-22** with the lines: 1.3×, as rendered (ADR-0026). |

## 5a. More kits (look test 18, not approved)

`Tools/blender/furniture_kit_18.py` builds on kit 17 and adds twelve kits, each piece in all four lines from the same
kit parts: kitchen, bathroom, dining, study, children, hallway, garden, shop, café, bedroom, living and decoration
(78 base pieces, 312 in the four lines). Renders in `Art/StyleTests/furniture_kit_18/`: a sheet per kit and one room
per kit in one line. New objects need a few new shared materials, used the same way by every line (I6): porcelain and
enamel (matte, Cream), a dull blue-grey mirror that never shines (I1), chalkboard, cork, wicker and terracotta pots.
They are for the owner to choose from; nothing here is in force until approved.

## 5b. Sample houses (look test 19, not approved)

`Tools/blender/house_samples_19.py` builds on kit 18 and furnishes one plan of a two-storey detached house (10 × 8 m,
in its own garden) once in each line: hall and stairs, living room, kitchen, dining room and bathroom downstairs; main
bedroom, children's room, study and guest room upstairs; garden pieces outside. Renders in
`Art/StyleTests/house_samples_19/`: the house outside, and each floor cut away at waist height. The outside is built from the
town's own kit (`building_kit_07.py`): barrel-tile roof with ridge tiles, eave corbels and a chimney with pots, stone
plinth, string course, stone window surrounds and louvred shutters. Each line picks from it: Plain is plaster with teal
shutters; Old Town warm plaster with dressed corner stones, a door hood and window boxes; Workshop dressed stone all
over with a darker roof; Curve plaster with round-arched ground-floor windows and door. Not in the map: if the owner
wants such houses in the town, they become lots in `World/` (WORLD_MAP).

## 6. How the renders get their look, and how the game must match it

The look test renders are not painted or filtered: the look comes from a handful of settings in the scripts
(`style_test_02.py` materials, `old_town_04.py` light, `furniture_kit_17.py` rooms). The game has to rebuild each of
them; if one is left out the furniture will not look like the renders. Values are the scripts' own.

| # | What the render does | Where in the scripts | Browser slice today | Unreal (PH1) |
|---|---|---|---|---|
| R1 | **Light bounces.** Cycles path-traces with 5 diffuse bounces, so a room is lit mostly by light off the sunlit floor patch and the walls; corners, the gap under a sofa and the foot of a wardrobe go dark on their own, and colour bleeds (a terracotta sofa warms the floor in front of it). | `configure()`: `diffuse_bounces` 5, `max_bounces` 8 | Stood in for by `roomLight()` in `play.js`: a sky/ground ambient, the room's lamps, and contact shade from a floor map (ADR-0018). No real bounce, no colour bleeding. In the outlet hall the extra daylight comes from the band of skylights (`skyFF()`): each surface gets it by how much of the band it faces and how near it is, so the back of the hall, walls high under the roof and faces turned away from the openings fall into shade. The warehouse racks are shaded from their outlines: inside, darker deeper in, just under each shelf and at the foot of each box; outside, the wall and floor facing a rack. The hall's corners are shaded too: its walls darken into the corners where two meet and up under the roof, the roof darkens towards the walls and where it meets each truss, a truss's sides darken up into the roof, and a wall darkens where a truss meets it. People in the hall are lit like people outdoors under the sky: brightest towards the nearest part of the skylights, darker on what faces down (under the hair, the arms, the chin), and they keep most of the daylight on top even under the solid roof; the player and the 7 nearest people shade the floor at their feet. Everywhere, indoors and out, corners, creases and gaps also darken through ambient occlusion worked out from the screen's depth (ADR-0028): the nearest stand-in for the renders' own darkening of corners. | Lumen global illumination (software tracing for modest hardware), with furniture in the distance fields. This is the single biggest part of the look. |
| R2 | **Sun through the window.** A sun of strength 8.5, colour (1.0, 0.86, 0.68), with a 2.5° disc, so it lays a bright patch with soft edges across the floor, and that patch lights the room. | `sun_and_sky(...)`, `angle=2.5` | From reading the shader, surfaces indoors take `roomLight()` only: no sun patch on the floor through windows. **A gap to close** before furniture goes in. Closed for halls under skylights (the outlet, FURN-020): `roomLight()` adds the sun with the shadow map there, so the skylights lay patches broken by the trusses, and the sets, racks, furniture and people cast shadows; still open for ordinary rooms' windows. | Directional light, Source Angle 2.5°, colour and intensity from the kit recipe (ART_DIRECTION §2.4); shadowed through the window openings. |
| R3 | **Cool sky against warm sun.** The sky is one flat colour (0.42, 0.55, 0.82) at strength 0.25 to 0.35, not a photo, so shade is slightly blue and sunlit spots warm. | `sun_and_sky` | `uSky` / `uGround` in the kit light (ADR-0013). | Sky Light from a flat-colour cubemap of the same colour, not an HDRI. |
| R4 | **Warm lamps with soft shadows.** Point lights of 35 W (bedside) and 80 W (floor lamp), colour (1.0, 0.6, 0.3), 8 cm across; shades glow at 1.6. They light a pool, cast soft shadows and carry the room at dusk. | `light()`, `M["shade"]` | Up to 16 lamps per room, distance and facing, **no shadows**. | Point lights with inverse-square fall-off, Source Radius 8 cm, shadow casting, through Lumen; emissive shades. |
| R5 | **Tone mapping.** AgX view with the "Medium High Contrast" look: bright spots roll off softly instead of clipping, strong colours are calmed in the highlights. | `configure()`: `view_transform` AgX, `look` | Already matched: `TONE` in `play.js` is a fitted AgX Medium High Contrast curve. | Unreal has no AgX tonemapper: use OpenColorIO with Blender's AgX config, or a 3D LUT made from it, with the built-in tonemapper set neutral. Check side by side. |
| R6 | **Exposure differs indoors.** The renders use −0.15 outdoors and +1.1 indoors (+1.2 at dusk): rooms are about 2.5 times dimmer than the street and are brightened for viewing. | `render(... 1.1 ...)` | One exposure for the frame. | Auto-exposure clamped to that range (about −0.2 to +1.2 EV around the street value), or post-process volumes per room. Never full auto: the look depends on dark corners staying dark. |
| R7 | **Matte, mottled materials.** Every surface is `painted()`: two close tones mixed by noise (detail 8), a four-stop ramp, roughness 0.92, specular 0.25, a small bump from the same noise, and grime that darkens the bottom 30% of each object. No image textures. | `painted()`, `flat()` | Material 0 and the `paint()` shader function do the same mottling. | One master material with these parameters (noise in object space so pieces do not swim), roughness 0.9, specular about 0.25, a bottom-darkening gradient. |
| R8 | **Rounded edges catch light.** Every part has a bevel (1 to 5 cm, 2 to 4 segments) and smooth shading, so each edge shows a thin bright line in the sun and nothing reads knife-sharp (rule I2). Cushions and Curve pieces are subdivided. | `bevelled()`, `subsurf()` in `car_kit_09.py` | Box faces with no bevels: edges are hard. Needs bevelled meshes or a bevel in the shader near edges. | Bevels exported as real geometry with smooth normals (ASSET-004 export script), not normal maps. |
| R9 | **Clean but not noisy.** 96 samples with adaptive sampling and the denoiser; small things like book spines and knobs stay crisp. | `configure()` | n/a | Lumen's temporal filtering; keep knobs and handles as real geometry (rule I3) so they survive it. |

**How to check the game against the renders.** As for the streets (ADR-0013, ADR-0018): build the chosen room in the
slice (and later in Unreal) at the same time of day, then measure brightness and saturation at the same spots: the sunlit
floor patch, the shaded wall behind the sofa, the wall lit by the floor lamp, the floor under the bed. Each should be
within about 0.03 in brightness of the render. Keep the comparison pictures in `Art/StyleTests/`.
