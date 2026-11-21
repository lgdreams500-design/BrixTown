---
doc_id: WASTED-SPEC-WORLD-MAP
title: Wasted — World map as data, the starting city and the playable slice
version: 0.5.3
status: draft
last_updated: 2026-11-22
parent: docs/specs/GAME_SPEC.md        # extends §4.6 CITY and §4.7 WLD; same ID rules
related: docs/specs/domains/LIVING_CITY.md
data: World/                           # the city itself
tools: Tools/worldgen/                 # generator, loader, checks, bundler
playable: Prototype/play/index.html    # open in Edge or Chrome
scope_source: docs/backlog/feature_choices.json
---

# World map

The city is a set of data files. Nothing that draws or simulates the city knows what is in it: the
loader, the living-city rules and the playable slice all read the same files. The city grows by adding
or editing files, never by changing code.

## 1. What exists today

| Thing | Where | State |
|---|---|---|
| Starting city, 8×8 blocks | `World/city_01/` | Generated, checked, 0 problems |
| Shared rules (roles, choices, calendar, plants, walking speeds) | `World/rules/` | Moved out of the one-block prototype |
| Generator for the starting city | `Tools/worldgen/make_city_01.py` | Deterministic from one seed |
| Loader and checks | `Tools/worldgen/world.py` | `load()`, `validate()`, `simulation()` |
| Tests | `Tools/worldgen/test_world.py` | 13 pass; the 13 one-block tests in `Tools/citysim/` still pass |
| Playable slice | `Prototype/play/` | Runs in a browser from the same files |

The starting city, **Old Town**: 64 blocks, 503 lots, 1,134 residents, 589 trees.

| Kind | Count | Notes |
|---|---|---|
| Homes | 340 | Row houses on the outskirts, courts in Old Town, 2 residential towers |
| Shops | 35 | Includes Lantern Arcade (the mall) and the tower kiosks |
| Cafés | 38 | |
| Workplaces | 42 | Includes Town Hall, Guild House and 3 office towers of 16 to 22 floors |
| Clinics and hospital | 9 | Mill Street Hospital plus 8 clinics |
| Gardens and parks | 28 | 4 parks, 24 courtyard gardens |
| Market Square | 1 | 8 stalls, staffed on market days |
| Tram stops | 6 | |
| Utilities | 4 | 2 depots you can enter, 2 substations you cannot |

466 of the 468 buildings can be entered (the two substations cannot).

## 2. File layout

```
World/
  rules/                 shared by every city
    roles.json           how each role spends a weekday, weekend, market day
    choices.json         what "lunch", "leisure" … can mean, and what people use where
    calendar.json        seasons, week, sunrise and sunset, weather odds, tram timetable, clock pace
    vegetation.json      growth and leaf dates per species, flower dates
    signatures.json      walking speed per movement signature (CHARACTER_MOTION §3)
    traffic.json         rules of the road and residents' cars (LIVING_CITY §3.4)
  city_01/
    city.json            name, seed, grid, street names, spawn point, list of block files
    residents.json       every resident: home, role, workplace, nearest amenities
    roads.json           junctions and their control, streets with speed limits, parking bays, crossings, town gates (CITY-043, CITY-068)
    blocks/b_<i>_<j>.json   one file per block
```

Units are metres. `x` runs east, `z` runs south, `y` is up. Block `(i, j)` has its north-west corner
at `(i·96 + 8, j·96 + 8)` and is 80 m square; streets are 16 m wide and their centre lines sit at
multiples of 96 m.

### 2.1 A block file
| Field | Meaning |
|---|---|
| `id`, `grid`, `origin`, `size` | Which cell it fills and where that is |
| `template`, `district` | What kind of block it is and which part of town |
| `lots[]` | Buildings and open places (below) |
| `paths[]` | Footpath segments `[[x, z], [x, z]]`. Segments must meet at shared end points and reach a street centre line |
| `trees[]` | `id`, `species`, `pos`, `planted` (day, may be negative), `seed` |
| `props[]` | `kind` (`lamp`, `bench`, `fountain`, `flowerbed`, `stall`, `shelter`, `watertower`, `arch`, `lantern`), `pos`, and a few extras (`arch`: `span`, `width`, `thick`; `lantern`: `height`, `power` in watts, `out`) |

### 2.2 A lot
| Field | Meaning |
|---|---|
| `id` | Unique in the city. Residents refer to lots by this |
| `kind` | `home`, `shop`, `food`, `work`, `health`, `utility` (buildings); `garden`, `market`, `stop` (open places) |
| `name` | Shown on signs, the map and the HUD |
| `rect` | Footprint `[x0, z0, x1, z1]` |
| `door` | `[x, z]`, on a wall of the footprint |
| `floors` | Storeys. Height is 3 m a floor |
| `enterable` | Whether the ground floor is open |
| `objects` | Places to be, by type: `bed`, `room`, `work`, `seat`, `customer`, `chair`, `bench`, `stall`, `wait`, `flowerbed`, `rim`. Each is a list of `[x, z]` |
| optional | `residents` (how many live here), `tower`, `loop` (strolling loop of a garden), `tags`, `staff`, `desks`, `seats` |

### 2.3 A resident
`id`, `name`, `role`, `home`, `work`, `offset` (minutes early or late by habit), `near` (their own
café, grocer, garden, market, stop and clinic), and for couriers and utility workers `rounds`.

## 3. Requirements

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| WLD-020 | The map shall be data: a city file that lists block files. Code that reads the map shall hold no knowledge of what is in it. | F-MAP-CITY | 1 | P0 | Slice and rules both run from `World/` unchanged |
| WLD-021 | Adding a block shall need only a new block file and one entry in `city.json`. | F-MAP-CITY | 1 | P0 | §4 |
| WLD-022 | Every lot shall have a stable id, a kind, a name, a footprint and a door; residents and saved state refer to lots by id. | F-MAP-LOTS | 1 | P0 | `validate()` |
| WLD-023 | The data shall be checked before use: no overlapping buildings, doors on walls, places inside their building, every reference resolves, every door reachable on foot. | F-MAP-LOTS | 1 | P0 | `test_data_is_sound`, `test_a_block_added_by_hand_is_checked` |
| WLD-024 | The starting city shall be 8×8 blocks with homes, shops, cafés, workplaces, a mall, a hospital and clinics, a town hall, a market square, parks, utilities, tram stops and at least five towers. | F-MAP-CITY | 1 | P0 | `test_key_amenities_exist` |
| WLD-025 | Rules shared by all cities (roles, choices, calendar, plants) shall live outside any one city. | F-MAP-CITY | 1 | P1 | `World/rules/` |
| WLD-026 | The clock pace shall be data, and people shall walk at their true speed whatever the pace. | F-NPC-ROUTINE | 1 | P0 | `test_nobody_walks_faster_than_their_own_pace` |
| CITY-050 | At least 95% of buildings shall be enterable through an open door, with the places inside taken from the lot's `objects`. | F-BLD-INSIDE | 2 | P0 | `test_nearly_every_building_can_be_entered` |
| CITY-051 | Residents shall use the same doors and the same places inside as the player. | F-BLD-INSIDE | 2 | P0 | Slice: residents sit, work and sleep at the listed places |
| CITY-052 | A resident shall leave early enough to arrive on time; one who is late skips the stop they have missed and walks on. Nobody speeds up. | F-NPC-ROUTINE | 1 | P0 | `test_most_walks_arrive_on_time`, `test_the_day_has_no_gaps…` |
| CITY-053 | Each resident shall use the amenities nearest their home unless their role says otherwise. | F-NPC-ROUTINE | 1 | P1 | `near` in `residents.json` |
| CITY-054 | Only residents near a player shall have bodies; the rest stay data and still show on the map. | F-NPC-LOAD | 1 | P0 | `test_only_nearby_residents_get_bodies`; slice HUD count |
| CITY-055 | The player shall walk the city in third or first person, enter buildings, and sit, lie down and ride lifts where the data offers a place. | F-BLD-INSIDE | 2 | P0 | Walking test, §5.3 |

Levels follow the owner's request for this slice ("most houses and buildings accessible"). The picks
in `docs/backlog/feature_choices.json` are still empty and remain the owner's to set.

## 4. How to grow the city

**Change something.** Edit a block file (move a door, rename a shop, add a bench), then:
```
cd Tools/worldgen
python -m unittest test_world        # refuses broken data and says what is wrong
python build_play.py                 # refresh the playable slice
```

**Add a block by hand.** Copy a block file, give it a new `id` and `grid` cell and matching `origin`,
give every lot a new id, add `{id, grid, file}` to `blocks` in `city.json`, and run the two commands
above. To grow past 8×8, raise `columns` or `rows` in `city.json`.

**Add a new kind of block.** Add a template to `Block.build()` in `make_city_01.py`, or write a new
generator that emits the same fields. Generators are a convenience; the files are the map.

**Add residents.** Add rows to `residents.json` (the generator fills homes, staffs workplaces nearest
first, and gives everyone their nearest amenities).

Re-running `make_city_01.py` rewrites `World/city_01/` from the seed, so hand edits to that city are
lost. Once hand editing starts, stop re-running it for that city `[OPEN: Q-22]`.

## 5. The playable slice

`Prototype/play/index.html`, opened in Edge or Chrome. It is a stand-in for the Unreal build: plain
WebGL, every shape built by code, no art assets. It exists to walk the city and to prove the data, the
rules and, since 0.2.0, the look and the movement set in the art documents (§5.5).

### 5.1 What is real
- The whole city is built at load time from the block files: streets, pavements, every building with a
  hollow ground floor and an open doorway, furniture from `objects`, roofs, windows, trees at their
  calculated size and leaf colour, lamps, benches, fountains, stalls.
- One day of all 1,134 residents (day 16, a summer Wednesday and market day), exported from the
  living-city rules. Residents near the player get bodies; all of them show as dots on the map.
- Third and first person, collision with walls and furniture, jumping, stepping onto low things.
- Sit on stools and benches, lie on beds, ride the lift to the roof of the five towers.
- Day and night from the calendar's sunrise and sunset; lamps and lit windows at night.
- A tram on Tram Street, on the timetable the residents use.
- Talking to residents (CITY-007, F-NPC-TALK level 1): walk up to someone and press E. Conversations are
  data in `World/rules/dialogue.json`, picked by the resident's role and what they are doing (at the counter,
  at the clinic, in passing, an elder, a chat), with replies that branch, loop back and end. While talking,
  the resident stops and turns to you; afterwards their day carries on from where they stood, running late
  by the time spent, until they next stand still. Replies can test and set flags kept per resident, ready
  for quests; `residents` in the file gives one person their own conversation.

### 5.2 What is not there yet
- Upper floors: only ground floors are open, plus tower roofs.
- Interiors are one room per building.
- Residents do not avoid the player or each other, and do not talk among themselves. Conversations are
  samples, the same for everyone in a role; flags last only until the page is reloaded; no outcomes yet.
- No rain or fog, no vehicles other than the tram, no trams on the other stops' streets.
- The same day repeats at midnight.
- No needs, money, jobs or saving.
- From the art documents, not yet: bevelled edges, light shafts, speech and expressions, the state
  layers (tired, happy, cold), cloth that swings. See §5.5.

### 5.3 Checked
Driven in headless Chrome: walks in through a café door and out again, is stopped by the wall beside
the door, sits on a bench, lies on a bed, rides a lift up and down, is held by the roof parapet, jumps
and lands. `Prototype/play/preview.png` shows the start screen, the market, a café front at dusk and a
street at night. It has not yet been timed on the owner's laptop: the frame rate is on the HUD. The
picture costs more since 0.2.0 (shadows, lantern light); Q, or a low frame rate, turns shadows off. `?quality=1` (or `0.5`) holds the picture so frame times can be compared.

### 5.4 Numbers
| | |
|---|---|
| Static geometry | About 1.7 million vertices in 129 pieces (one per block, the trees of each block square, and the fields and town wall), plus 8.3 million of building relief in 64 block pieces drawn within 115 m. Each frame draws only the pieces inside the view, and for shadows only those inside the sun's box: about 0.7 to 1.1 million vertices in view. Depth is laid first so the lighting runs once per pixel. On an Intel laptop GPU at 1536 × 1152 with shadows: 10 to 14 ms of GPU work a frame, down from 21 to 27 ms in 0.5.0 (measured with GPU timers, same views, same picture) |
| Things you can bump into | 6,236 |
| Places you can use | 3,266 |
| Bodies on screen | 10 to 60 of 1,134, within 85 m |
| Data bundle | 2.3 MB |

### 5.5 How the slice follows the art documents
Checked against the images in `Art/Reference/`. "Partly" means the rule is met in a cheaper way than the renders.

| Rule | In the slice | State |
|---|---|---|
| W1 matte, mottled | Every surface uses the `painted()` recipe: dark tone, mid tone and warm highlight blotched by noise | Done |
| W2 large stone blocks, dark joints | Blocks of 1 × 0.5 m with thin dark joints and a shaded lower edge; in the old quarters stone runs one storey (3.55 m, as in `building_kit_07.py`; it was 5 m) with a string course and piers every 5 m, plaster above on all but about one in seven houses | Done |
| W3 chunky and soft | Round-arched doorways ringed with arch stones and a keystone, stone arches over every alley, a covered gateway with a room over it; windows with a standing sill, jambs, lintel and keystone, louvred shutters, flower boxes; corner quoins, plinth, cornices, tiled roofs on corbels with ridge and chimney, drainpipes, striped awnings; shops, cafés and clinics carry their lot's `name` on a board over the door and on both faces of a hanging sign (from `Tools/blender/building_kit_07.py`). The small relief is one mesh per block, drawn within 150 m | Partly: edges are not bevelled and window reveals are drawn, not cut |
| W4 muted, warm, few accents | Only the §2.3 palette; colour comes from shutters, awnings, cloth across streets, crates and clothes | Done |
| W5 low directional light and haze | The script's sun, sky, exposure and AgX tone curve; soft shadows; light bounced off sunlit stone estimated from how much sky each spot sees; forward-scattering haze with light shafts | Partly: bounced light is an estimate, not traced |
| W6 lanterns and lit windows | Street, door and alley lanterns light paving, walls and people (the 12 nearest); wall lanterns stay lit in the shade by day and double at night; half the home windows are lit | Done |
| W7 reads at morning, midday, dusk | `preview.png`. Night is added and is not yet in the art document | Done |
| W8 no post-process | None | Done |
| C1 whole garments, §3.2 proportions | Tee, long top, tunic, coat and skirt as one soft form; head 1.22, legs 0.95, limbs 1.12 | Done |
| C2, C3 palette cloth, matte skin | Clothes in the world palette with blotches; eight skin tones | Done |
| C4 solid hair | Short, side, bob, long, bun, afro | Done |
| C5 silhouette per role | Apron for shop and café staff, cap and bag for couriers, long coat for office workers, scarf and bun for elders | Done |
| C6 faces | Round eyes with a large iris low in the eye, highlight, brows, small nose, mouth at rest; blink every 2 to 6 s | Partly: no expressions or speech shapes yet |
| C7 one body for all | Player and residents are one function with designer values (`People.look`, `People.player`) | Done |
| M1 movement signatures | Springy, Easy, Hurried and Careful with the numbers of CHARACTER_MOTION §3, ported from `motion_lib.py`; step rate follows leg length | Done |
| M5 feet first | Feet are placed, legs are solved to reach them; phase comes from distance walked | Done |
| M3, M4 layers; eyes lead, head follows | A resident within 5 m turns eyes and head to the player while walking or standing; the player's head follows the camera | Partly: no spring or overshoot, no chest turn |
| M6 soft and weighted | Hip bounce, sway, roll and counter-turn of the chest from the signature | Done |
| M2 state shows in the body | Not used: nothing sets tired, happy or cold yet | Not yet |

**Where the numbers come from (0.3.0).** Light, haze, materials and tone are taken from
`Tools/blender/style_test_02.py`, the script behind the reference image: sun 14 W/m² in (1, 0.86, 0.68),
sky (0.42, 0.55, 0.82) at 0.35, haze anisotropy 0.6, exposure +0.3, AgX "Medium High Contrast", the
`painted()` colour ramp, lantern emission 6 and a 120 W lantern light, camera lens 24 mm. One value differs
on purpose: haze density is 0.009 to 0.014 instead of 0.022, because the city is 800 m across and the
reference scene is 80 m. The slice starts in an alley at 07:20 looking into the sun, the view closest to
the reference. Colours measured in the same spots (sRGB):

| Spot | Reference image | Slice |
|---|---|---|
| Shaded wall | (105, 77, 57) left, (66, 49, 41) right | (97, 72, 58) |
| Shaded paving | (84, 66, 56) | (73, 60, 51) |
| Haze through the arch | (177, 165, 156) | (164, 159, 154) |

Still short of the renders: every edge there has a 4 cm bevel, light bounces properly, paint has brush
relief, and people are smooth sculpted forms. A path-traced render cannot be matched exactly in a browser
with code-built shapes; the Unreal build is where it should be. `wasted.dials` in the browser console
(sun, sky, bounce, lanterns, haze, exposure, warm) scales each part of the light for tuning.

**Daylight since 0.5.0: the "kit" light (ADR-0007).** It is the softer, warmer light of the building kit renders: sun ×0.62,
sky ×0.9, bounce ×1.5, haze ×0.55, exposure ×0.7, a more golden sun. Sunlit plaster reads warm grey instead of nearly white;
the hazy light shafts of the reference alley are weaker. The earlier light is kept as the "reference" preset: L switches,
`?light=reference` starts with it. The colour measurements above were taken under the earlier light.

New and not approved: the running signature, and the standing, sitting and lying poses (CHARACTER_MOTION §6
lists them as missing). They are in `Prototype/play/people.js` and need the owner's eye.

## 6. A finding: the clock and the size of the city

People walk at their true speed, so the faster the clock, the longer a walk takes in game minutes.
Measured over the whole city for one day:

| Real seconds per game minute | Real length of a game day | Walks that arrive on time | 90% of walks are at most this late | Walking at 08:00 |
|---|---|---|---|---|
| 3 | 1.2 h | 45% | 8 h | 412 |
| 6 | 2.4 h | 58% | 88 min | 220 |
| **10 (set now)** | **4 h** | **85%** | **6 min** | **132** |
| 15 | 6 h | 95% | 0 min | 87 |
| 60 | 24 h | 98% | 0 min | 25 |

GAME_SPEC WLD-001 assumes a 60-minute day. At that pace residents of a city this spread out cannot
keep a timetable with several stops in it. Something has to give `[OPEN: Q-21]`.

## 7. Open questions

| # | Question | Suggested answer |
|---|---|---|
| Q-21 | How long is a game day? A 60-minute day (WLD-001) does not fit residents who walk at true speed between places 150 m apart. | A 4-hour day for now. To shorten it later, give residents fewer and longer stops, or let the tram carry them, rather than making them walk faster. |
| Q-22 | Is the starting city generated or hand-made from here on? | Generate once, then edit by hand and with a block editor; keep the generator for new districts. |
| Q-23 | How are upper floors reached? | Stairs in houses, lifts in towers, each floor its own set of places in the lot data. |
| Q-24 | Do streets carry people down the middle, or on pavements with crossings? | **DECIDED 2026-11-22:** pavements and crossings (CITY-043, accepted); the path data needs a lane per side. |

## 8. Change log
| Version | Date | Change |
|---|---|---|
| 0.1.0 | 2026-11-21 | First version: data format, starting city, checks, playable slice, clock finding. |
| 0.2.0 | 2026-11-21 | The slice now follows ART_DIRECTION, CHARACTER_FACE and CHARACTER_MOTION (§5.5). |
| 0.3.0 | 2026-11-21 | Light, haze, materials and tone taken from the reference Blender script and checked by measured colours; arched alleys and gateways in the courtyard blocks (new `arch` prop); old-quarter buildings built as in the reference. |
| 0.4.0 | 2026-11-21 | Buildings in the slice carry the relief of the building kit (`building_kit_07.py`, not yet approved): window trims, louvred shutters, arch stones, quoins, tiled roofs, awnings and signs (§5.5 W3); deep framed windows, arched shop and café fronts lit by day under a full-width awning, café tables, balconies; old-quarter stone storey lowered from 5 m to 3.55 m so upper floors are plaster with windows. |
| 0.5.0 | 2026-11-22 | Daylight is the "kit" light by default (ADR-0007); the earlier light stays as a preset (L). |
| 0.5.1 | 2026-11-22 | Faster slice, same picture: the town mesh is drawn in pieces culled to the view and to the sun's shadow box, a depth-only pass comes first so each pixel is lit once, painted names have their own small mesh and shader, and the shader skips the shadow lookup on faces turned from the sun and unused lantern slots. GPU time per frame roughly halved (§5.4). |
| 0.5.2 | 2026-11-22 | Q-24 decided: pavements and crossings, now that cars are in scope (CITY-043). |
| 0.5.3 | 2026-11-22 | Talking to residents in the slice (CITY-007; owner picked F-NPC-TALK level 1 for the first slice): `World/rules/dialogue.json`, checked by `world.dialogue_problems` and `test_world.TheDialogue` (every reply leads somewhere, every node is reachable, every talk can end), played by `Prototype/play/conversations.js` (§5.1). |
