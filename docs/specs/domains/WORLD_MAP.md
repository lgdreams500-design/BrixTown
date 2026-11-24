---
doc_id: WASTED-SPEC-WORLD-MAP
title: Brix Town — World map as data, the starting city and the playable slice
version: 0.5.22
status: draft
last_updated: 2026-11-24
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
| Furniture outlet | 1 | Outside the walls, beyond the East Gate: block `b_8_4`, the first outside the 8×8 grid (FURN-020, ADR-0027; `Tools/worldgen/add_outlet_01.py`) |

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
    transit.json         bus stops and lines: stops in order, how often, first and last (CITY-091)
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
| optional | `residents` (how many live here), `tower`, `loop` (strolling loop of a garden), `tags`, `staff`, `desks`, `seats`, `plan` (a mall's floors: `malls_01.py`; Eastgate's: `mall_inside_01.py`, WLD-058), `shell` (a mall's sculpted outside round its hall: WLD-057, `Tools/worldgen/mall_shell_01.py`) |

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
| WLD-027 | The starting city shall have a bank, a car showroom (`dealer`, on a through street), a library, a museum and a cinema, plus a second hospital and a second mall; each staffed, each the nearest of its kind for every resident, and the library, museum, cinema and bank visited in a day. Lot kinds `bank`, `dealer`, `library`, `museum`, `cinema`; the hospital and mall are tagged `hospital` and `mall`. Made from existing lots by `Tools/worldgen/add_places_01.py`. | F-BLD-CIVIC | 1 | P1 | `test_WLD_027_bank_showroom_library_museum_cinema_and_second_hospital_and_mall` |
| WLD-028 | Street dressing shall be data: props in the block files marked `"set": "dress"` (kinds `playground`, `swing`, `slide`, `seesaw`, `sandpit`, `climber`, `bandstand`, `noticeboard`, `sundial`, `well`, `pergola`, `statue`, `kiosk`, `bunting`, `planter`, `bin`, `bikerack`, `postbox`, `phonebox`, `bollard`, `timetable`, `advert`, `hoarding`), placed by `Tools/worldgen/add_dressing_01.py`; each within its block or the pavements round it and none in a doorway. | F-MAP-DRESS | 1 | P1 | `test_WLD_028_dressing_is_sound` |
| WLD-029 | Each of the four big parks shall have a playground (swings, slide, seesaws, sandpit, climbing frame) with places to play, and a notice board; Linden Park a bandstand; each court garden a centrepiece of its own; the Town Hall a statue before it; Market Square a grand fountain, a news kiosk and a notice board. | F-MAP-DRESS | 1 | P1 | `test_WLD_029_parks_courts_and_squares` |
| WLD-030 | Buildings shall carry painted adverts on blank street walls, timber hoardings on works and depots, and window boxes on about half the homes. | F-MAP-DRESS | 1 | P1 | `test_WLD_030_buildings` |
| WLD-031 | Pavements shall have bins, planters, bicycle racks, post boxes, telephone boxes and corner bollards, and every tram stop a timetable. | F-MAP-DRESS | 1 | P1 | `test_WLD_031_streets` |
| WLD-032 | Children (role `child`, the students who live nearest each park) shall take the tram to school on weekdays and play on the swings, slide, seesaws, sandpit and climbing frame after school and at weekends, staying in when it rains; the slice moves them with what they play on. | F-MAP-DRESS | 2 | P1 | `test_WLD_032_children_play_after_school` |
| WLD-033 | Residents shall stop to read the notice boards and buy a paper at the kiosk, which has its own keeper. | F-MAP-DRESS | 2 | P1 | `test_WLD_033_notices_and_the_kiosk` |
| WLD-034 | What the dressing shows shall be worked out from the day and `World/rules/dressing.json`, never stored: each advert board changes every few days, out of step with the others; the cinema shows the film of the day; bunting is up on market days only. | F-MAP-DRESS | 2 | P1 | `test_WLD_034_what_changes_with_the_day` |
| WLD-035 | By day the sky shall be blue overhead and hazy at the horizon, with soft clouds lit from the sun's side that take the colour of the hour; its numbers shall be data in `World/rules/sky.json`. | F-SKY | 1 | P1 | `test_WLD_035_sky_rules_are_sound`; slice at morning, midday, dusk and night |
| WLD-036 | Where the clouds are, their shape and how much sky they cover shall be worked out from the world clock, the day's weather and the data, never stored or ticked; every kind of weather shall have a cover. Cover follows the weather only from F-SKY level 2. | F-SKY | 1 | P1 | `test_WLD_036_every_kind_of_weather_has_a_sky`, `test_WLD_036_the_slice_gets_the_sky` |
| WLD-037 | Cloud, rain and fog shall be seen, not only named: each kind of weather shall change the sunlight, the thickness of the air and the clouds by numbers in `World/rules/sky.json`; rain shall fall and wet the paving, which dries after it stops; clear weather shall leave the approved light untouched. All of it shall be worked out from the world clock and the day's weather. | F-WEATHER | 1 | P1 | `test_WLD_037_every_kind_of_weather_changes_the_light`, `test_WLD_037_broken_weather_is_caught`; slice in rain, fog and cloud, by day and at night |
| WLD-038 | Which stone, brick or paving a surface uses shall be data: patterns and first-match rules in `World/rules/surfaces.json` for a building's walls, a block's pavements and an open lot's ground, with an optional `surface` field on any lot or block that beats the rules; the data is checked (known types and sizes, every named pattern exists and suits its slot, every slot's rules end with a catch-all). | ART-001 | 1 | P1 | `test_WLD_038_surfaces_are_sound` |
| WLD-039 | The town shall mix its surfaces by district (ADR-0017): rubble in the old quarter, brick terraces (some brick to the eaves), rusticated civic buildings; flagstones, slabs, herringbone, basket weave and setts on the ground; the same building always gets the same pattern. | ART-001 | 1 | P1 | `test_WLD_039_surfaces_mix_by_district` |
| WLD-040 | Every park and court garden shall be planted from data in its block file (`Tools/worldgen/add_garden_01.py`): a low clipped box hedge along each side, open at the corners, the middle of each side and wherever a seat needs the room; a border of two kinds of flower in front of it; flowering shrubs (lilac, hydrangea, rose) in the corners and on the lawn; bulbs coming up under the trees and asters on the lawn; in the four park blocks, groups of shrubs with bulbs on the outer lawn. What each kind looks like on a day (in leaf, in flower, dry heads, died back) comes from `World/rules/vegetation.json` "plants" through citysim (CITY-017). Hedges and shrubs block the way; nothing is planted on a path or a seat. | F-GARDEN | 1 | P1 | `test_WLD_040_every_garden_is_planted`, `garden_problems()` |
| WLD-041 | The player shall be able to play on the playgrounds as the children do: sit on a swing seat and swing, higher as they keep at it; sit on a seesaw end and rock; climb the slide's ladder and slide down, getting off at the bottom; sit in the sandpit; stand on the climbing frame's deck. E or moving gets off, beside the piece. A swing seat, seesaw end or sandpit place a child is on is not offered to the player (one person per place); the slide takes turns and the frame has room for all. Timings in `World/rules/dressing.json` "motion". | F-OBJ-POINTS | 1 | P1 | slice (`dressing.js` playerAt) |
| WLD-042 | At night the sky shall show stars and the Milky Way, turning round the celestial pole with the hour and the day of the year, and a moon that changes its phase over the days, lights the street by its phase and height, and fades the stars when bright. The weather decides how much of it shows: clear nights show the most, cloud hides what is behind it, rain and fog hide the stars, and the town glows under the clouds. Lanterns light only what faces them at night and glow less in clear air, so stone, brick and paving keep their relief. All of it is worked out from the world clock and the day's weather; the numbers are in `World/rules/sky.json` "night" (ADR-0023). | F-SKY | 2 | P1 | `test_WLD_042_night_sky_rules_are_sound`, `test_WLD_042_a_broken_night_sky_is_caught`; slice at night in each kind of weather (`Art/StyleTests/night_sky_15/`) |
| WLD-043 | The sun and the moon shall cross the sky on paths seen from the town's latitude. Each season the sun shall rise and set at the calendar's times, north of east and west in summer and south of them in winter, higher at noon in summer than in winter. The moon shall be later than the sun by its phase and swing north and south with its orbit. The sun shall be drawn as a disc that stands out from its glare, and the moon shall show by day as a pale shape in its phase. All of it is worked out from the world clock and `World/rules/sky.json` (night.latitude, night.paths; ADR-0024). | F-TIME-DAY | 1 | P1 | `test_WLD_043_the_sun_climbs_higher_in_summer`, `test_WLD_043_broken_paths_are_caught`; slice in summer and winter (`Art/StyleTests/sun_paths_16/`) |
| WLD-050 | Every building shall be enterable, and every storey of one with two or more shall be reachable from inside: a switchback stair in a stairwell against a side wall (two flights a storey, a spine wall between them, a landing at each end, a rail where a floor stops at the opening), placed in the block file (`stair`) by `Tools/worldgen/add_storeys_01.py` from `World/rules/storeys.json`; the data is checked (`world.storey_problems`). | F-BLD-INSIDE | 3 | P1 | `test_WLD_050_storeys_are_sound` |
| WLD-051 | Buildings of `lift_from_floors` storeys or more (the towers) shall also have a lift that goes up or down a floor, to the ground, to the top floor and to the roof (owner, Q-23: "Stairs and a lift"). | F-BLD-INSIDE | 3 | P1 | `test_WLD_051_towers_have_a_lift` |
| WLD-052 | Inside shall be the size seen from outside: the ground floor at 0.2 m with its ceiling at 3.2 m, floor f at 3f + 0.4 m under a 0.2 m slab, the top ceiling meeting the roof at floors × 3 + 0.4 m, behind the same windows; each floor furnished for what `storeys` says it is (bedrooms, flats, offices, storerooms, wards, galleries, stacks, plant rooms). | F-BLD-INSIDE | 3 | P1 | `test_WLD_052_inside_is_the_size_of_outside` |
| WLD-053 | Residents shall use the upper floors: beds in houses and flats are upstairs (`levels`), and half the desks in offices; no two floors have a place at the same spot on the plan. | F-BLD-INSIDE | 3 | P1 | `test_WLD_053_residents_sleep_upstairs` |
| WLD-054 | Each mall shall have a small car park, about a quarter of the cars it had (40, 40 and 50 instead of 160, 160 and 200), a paved forecourt at its door, trees where the rest of the car park was, and a bus stop beside it (CITY-091). Owner, 2026-11-23: "Malls to ~1/4, bus stop"; done by `Tools/worldgen/shrink_carparks_01.py`. | F-MAP-CITY | 1 | P1 | `test_WLD_054_small_car_parks_with_a_bus_stop` |
| WLD-055 | The town wall shall be gone (owner, 2026-11-24: "remove city wall"): no wall, gate towers or gate names; Old Town's paving ends at a low stone kerb on the line where the wall stood (city.json `wall`, still the edge of Old Town's street grid), with openings where the four gate roads and the tram rails run on across the open ground. The street names West Wall, North Wall, East Wall and South Wall stay. ADR-0034. | F-MAP-CITY | 1 | P1 | `test_WLD_055_the_town_wall_is_gone` |
| WLD-056 | Every grid cell outside Old Town shall be part of a block, and the ground outside Old Town shall look finished with no new buildings (owner, 2026-11-24: "use vegetation other props etc, to fill in ... roads, parks, plots for sales, even dense woods ... but no building please"): 17 blocks of open ground with no lots, made by `Tools/worldgen/add_green_01.py`. Woods (pine, oak and birch close together, a trail, a clearing with picnic tables, undergrowth, logs, rocks), commons (a path round and across, a pond, groups of big trees, benches, bins, lamps), orchards (apple and plum rows in a box hedge), fields (crop strips and pasture between hawthorn hedgerows, a farm track, hay bales), building plots for sale beside the West and East Gate roads (fenced, pegged, mown, a "FOR SALE" board each, a gravel track in; set dressing only, since F-MAP-PLOTS is at level 0 for the first slice) and small meadows. Each block facing Old Town carries the Wall Walk: a footpath with an avenue of limes and lamps. Nothing stands on a road. Tracks and paths are walked, not driven. Spacings and counts `[ASSUMPTION]`. | F-MAP-DRESS | 2 | P1 | `test_WLD_056_*` (`TheOpenGround`); `test_WLD_056_pines_stay_green_and_woods_trees_leaf` |
| WLD-057 | Eastgate Mall shall have a sculpted shell in place of its plain box (owner, 2026-11-24, with a photograph of a mall of flowing white bands and golden pierced domes: "would you be able design one of these like attached pic?"): a skin of plaster ribbons standing round the hall on a closed line, its top rising and falling between about 19 and 28 m, dipping round an ochre pierced egg over each of the four corner stores (the tallest 42 m) and lifted off the ground as a round arch on each side, the grand one over the door; a cream pierced half egg over the open court as its roof light. The joints between the ribbons and the eggs' openings are dark by day and lit at night. The hall's `rect`, `door` and floors do not change (its `plan` was replaced the same day: WLD-058 to WLD-061). The shape is data (`shell` on the lot: `skin`, `domes`, `roof_light`, `arches`), worked out by `Tools/worldgen/mall_shell_01.py`; the slice draws it in the town's own materials (`Prototype/play/shell.js`; ART_DIRECTION W1, W3, W4, W6). The skin cannot be walked through; under an arch the player walks in to the hall's entrance (WLD-058). Sizes `[ASSUMPTION]`. ADR-0037. | F-MAP-CITY | 1 | P1 | `test_WLD_057_*` (`TheMallShell`) |
| WLD-058 | Eastgate Mall shall be entered on foot through an opening in the hall's wall under each of the shell's four arches (owner, 2026-11-24: "there are no entries to the mall"), the south one being the lot's `door`; from each a concourse runs straight to the court. The inside is designed afresh for the shell (owner, 2026-11-24: "ignore the old mall and use new interiors ... start interiors fresh as this is new design ... feel free to come with ideas that fit this new design language"): the lot's `plan` (concept `court`) is worked out by `Tools/worldgen/mall_inside_01.py` and replaces the `loop` plan of `malls_01.py`; the slice draws it and makes it walkable (`Prototype/play/court.js`) in room materials. Built at the owner's word; F-BLD-MALL has no level recorded yet and the level is theirs to say. ADR-0038. Sizes `[ASSUMPTION]`. | F-BLD-MALL | 2 | P1 | `test_WLD_058_*` (`TheMallInside`) |
| WLD-059 | Under each egg of the shell there shall be a round hall, open from its floor up into the egg: the big store of that corner (Food Market, Cinema, Department Store, Fashion House), reached by a slanting walk from the court that on the upper floors ends in a balcony looking into the hall. From inside, an egg and the roof light show the sky through their openings by day and are dark at night. The hall's six staff stand at stalls in it. | F-BLD-MALL | 2 | P1 | `test_WLD_059_*` |
| WLD-060 | The middle of the mall shall be one oval court under the roof light, open through all three floors, with a gallery round it on each upper floor whose edge wanders and stands further back on the upper floor; a plaster ribbon parapet with a line of light under it along every edge where a floor stops; in each half of the court a bridge on each upper floor, and moving stairs (walked up, no steeper than 30 degrees) from the court floor to the lower bridge and on to the upper; on the court floor a fountain, four gardens with trees, benches the player can sit on, and kiosks. The upper floors are stood on, at 6 m and 12 m. | F-BLD-MALL | 2 | P1 | `test_WLD_060_*` |
| WLD-061 | Every stretch of wall along the court, the concourses and the slanting walks shall be a shop front where a shop fits (61 to 64 a floor). The ground floor shall keep the jobs it had (four big stores of 3,600 m2, 58 shops and two cafes with a full staff: 206 posts in the same order, `careers.posts`), so that no resident's work changes; each such unit says where its staff stand (`stand`, used by `add_careers_01.py`). A shop whose staff are in is drawn open, with a counter and the staff behind it; the others have lit windows. Shops are not gone into or bought from one by one yet, and no resident visits the mall as a customer (the crowds of F-BLD-MALL level 2 need F-NPC-POP at 2; it is at 1). | F-BLD-MALL | 2 | P1 | `test_WLD_061_*` |
| WLD-062 | The mall's moving stairs shall move: each is two flights side by side, and whoever stands on one is carried, up on the right-hand flight (seen from its foot) and down on the left. A lift shall stand in each side concourse, a round shaft through every floor with its door towards the court, and take the player up or down a floor, or between the ground and the top (`plan.lifts`). Indoors the court and the egg halls are brighter by day than the walks back under the galleries (owner, 2026-11-24: "complete the mall"). | F-BLD-MALL | 2 | P1 | `test_WLD_062_*` |
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
- Traffic (LIVING_CITY 3.4): 198 residents' cars parked in their bays and driven by their owners on the day's plan
  (commuters out through the town gates and back), 6 delivery vans on rounds from the depots, and cars passing through
  between the gates, busiest at 08:00 and 17:00: 720 trips on day 16. The cars are the chosen "Soft box" look (ADR-0015,
  ART_DIRECTION 2.5): a small car, an estate (often with a case, a bag and rails on the roof), a pickup with crates and
  the delivery van, simple shapes with generously rounded edges, round lamps and wheels, a cream roof; drawn from meshes
  exported by `Tools/blender/car_export_11.py` into `car_models.js`, about 3,200 triangles near and 900 far. Cabins are open
  frames, so people show: a resident drives their own car, vans and cars passing through have a driver from out of town
  (sometimes with a passenger), and the player sits in the car they drive. The tram has open windows and bench seats,
  and the commuters who boarded it ride in it until it leaves the map, and on the way back from the edge to their stop.
  The player cannot walk through a car.
- Driving: E beside any parked car or van gets you in (it pulls out of its bay onto the road), W/S to drive and brake,
  A/D to steer, Space for the handbrake, E to get out. Walls and other cars stop you. A car you have moved stays where
  you left it and leaves its owner's day (a stand-in for owning a car, CITY-060 to CITY-063).
- Crash damage (F-VEH-CRASH level 2, CITY-072 to CITY-076): at any speed the player's car breaks lamps, benches, bins,
  planters, bollards, post and phone boxes, boards, racks, flowerbeds, stalls, crates, café tables and trees (2,596 in
  all), and chips building corners, even with a gentle nudge. Tall things topple the way they were hit, low things burst
  into pieces in their own colours that settle as rubble, a corner shows its rough core. Lamps go out and broken benches
  can't be sat on. Damage is saved in the browser (`wasted.damage.v1`; the server in the game) and stays until a repair
  crew from a depot has worked at it (07:00 to 19:00); a label says when they come. Residents within 18 m shout.
- Talking to residents (CITY-007, F-NPC-TALK level 1): walk up to someone and press E. Conversations are
  data in `World/rules/dialogue.json`, picked by the resident's role and what they are doing (at the counter,
  at the clinic, in passing, an elder, a chat), with replies that branch, loop back and end. While talking,
  the resident stops and turns to you; afterwards their day carries on from where they stood, running late
  by the time spent, until they next stand still. Replies can test and set flags kept per resident, ready
  for quests; `residents` in the file gives one person their own conversation.
- Faces while talking (CHR-007 to CHR-009, ANIM-006, F-NPC-SPEECH level 2): the resident speaking has the
  full face control set of CHARACTER_FACE. Each line is turned into the eight mouth shapes from its text and
  played in time (12 a second, pauses at punctuation, ending on a smile or rest); the line's mood sets brows,
  lids, squint and mouth curve, blended over time; brows lift at the start of a line and before a question,
  and the head nods on long words. Rules and values in `World/rules/speech.json`. Only the person you talk
  to gets this face, so it costs nothing elsewhere. The camera comes in to 2.6 m while talking.

### 5.2 What is not there yet
- Upper floors: every storey is reachable by stairs, the towers by a lift as well (WLD-050 to WLD-053, ADR-0021).
- Interiors are one room per building.
- Residents do not avoid the player or each other, and do not talk among themselves. Conversations are
  samples, the same for everyone in a role; flags last only until the page is reloaded; no outcomes yet.
- Rain and its puddles show under arches and awnings too, and rain is not seen from indoors; no sound of rain (ADR-0016). No trams on the other stops' streets. No lay-bys: parked cars stand half on the pavement. People
  still walk in the road (no pavements and crossings yet, Q-24), so walkers and cars pass through each other. Cars do
  not stop for the player (CITY-049) or for the player's car; driving is arcade-simple; there is no buying a car yet
  (CITY-060 needs money).
- The same day repeats at midnight.
- No needs, money, jobs or saving.
- From the art documents, not yet: bevelled edges, light shafts, expressions outside conversations, the state
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
| Static geometry | About 10.6 million vertices in 466 pieces (one per block, the trees of each block square, and the fields, kerb and rails; measured 2026-11-24 after WLD-056; trees in the woods are drawn with fewer facets), plus 8.3 million of building relief in 64 block pieces drawn within 115 m. Each frame draws only the pieces inside the view, and for shadows only those inside the sun's box: about 0.7 to 1.1 million vertices in view. Depth is laid first so the lighting runs once per pixel. On an Intel laptop GPU at 1536 × 1152 with shadows: 10 to 14 ms of GPU work a frame, down from 21 to 27 ms in 0.5.0 (measured with GPU timers, same views, same picture) |
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
| C4 solid hair | Short, side, bob, long, bun, afro; a fringe frames the face for short, side, bob and long hair | Done |
| C5 silhouette per role | Apron for shop and café staff, cap and bag for couriers, long coat for office workers, scarf and bun for elders | Done |
| C6 faces | Round eyes with a large iris low in the eye, highlight, brows, small nose, mouth at rest; blink every 2 to 6 s | Done in conversations (speech shapes, expressions, lip sync timed from text); elsewhere the face stays at rest |
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

**Kit light retuned by measurement (0.5.6, ADR-0013).** Against `world_street_kit_light.png` the slice was two to
four times too saturated (sunlit paving 0.21 against 0.09) and its sky was darker than its walls. The exact match
(the script's own sun colour, sky fill ×1.5, a grey bounce ×1.2, a pale haze veil, exposure ×0.78) measured sunlit paving
(167, 156, 151) against the reference's (171, 160, 156) and shaded paving (108, 102, 101) against (112, 106, 106), but the
owner found it too pale; it is kept as `?light=measured`. The default is a setting between it and the old one (values in
ADR-0013). Materials are unchanged: softer paint blotches and a slab
carriageway were tried and taken back at the owner's word. The old values are kept as
`?light=golden`. Still
different from the reference: round cobbles on footpaths, faceted trees, unbevelled edges, shop windows dark by day.

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
| Q-23 | How are upper floors reached? | Decided 2026-11-22 by the owner: stairs in every building of two or more storeys, stairs and a lift in the towers; each floor's places are in the lot data (`levels`). ADR-0021. |
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
| 0.5.4 | 2026-11-22 | Faces while talking (owner picked F-NPC-SPEECH level 2 for the first slice; CHR-008, CHR-009, ANIM-006 to PH1; Q-14 text first): lip sync and expressions from `World/rules/speech.json`, checked by `world.speech_problems` and `test_world.TheFaces`; drawn in `people.js`, timed in `conversations.js` (§5.1). The reference line of CHARACTER_FACE §4 comes out at 53 frames against the reference's 55. |
| 0.5.5 | 2026-11-22 | People brought closer to `character_lineup.png`: rosy cheeks (C8, ADR-0010); cloth mottle blends across each form instead of showing as patches; smoother near bodies (16 sides); a flat bib apron in place of a round blob; arms hang closer when standing, with rounded sleeve tops; chunkier limbs and rounded shoes; a fringe; a larger iris low in the eye; coat and tunic hems wide enough to hide striding legs. |
| 0.5.6 | 2026-11-22 | Kit light retuned towards the measured colours of `world_street_kit_light.png` (ADR-0013): greyer shade, paler sky. §5.5. |
| 0.5.7 | 2026-11-22 | Sky and clouds in the slice (WLD-035, WLD-036; feature F-SKY; ADR-0014): blue overhead, hazy horizon, clouds placed by the clock, numbers in `World/rules/sky.json`. Cloud cover follows the day's weather (owner picked F-SKY level 2 and F-WEATHER level 1 for the first slice); rain and fog themselves are not drawn yet. |
| 0.5.8 | 2026-11-22 | Rain, fog and overcast in the slice (WLD-037; ADR-0016): the light changes with each kind of weather, rain falls and wets the paving, the HUD names the weather. §5.2. |
| 0.5.9 | 2026-11-22 | Two keys for looking, at the owner's request: K tries each kind of weather, Y loads the next season (daylight hours, that season's weather, trees and flowers from `city_seasons.js`; routines, cars and boards stay those of the built day). Neither is a level pick: F-SEASONS is still undecided. |
| 0.5.10 | 2026-11-22 | Rain redone after the owner found it bad (WLD-037, ADR-0016): drops fall in the world round the camera, the sky closes over, paving fills with puddles and drop rings, lamplight shows in the wet at night. |
| 0.5.11 | 2026-11-22 | A bench in the start alley, as in the reference alley (`b_3_5.json`). New default daylight (ADR-0018, approved by the owner after a trial): bounce only near sunlit ground, contact shade under street furniture; G switches it off to compare. Comparison pictures in `Art/StyleTests/light_compare_12/`. |
| 0.5.12 | 2026-11-22 | People rebuilt part for part from the script behind `character_lineup.png` (`character_face_05.py`): head at 1.22 (it was 1.09), limbs at the reference thickness, a yoke across the shoulders, hips under the top, one-piece arms and legs, oval hands and shoes, scarf tail, bib apron, bag strap (ART-004, C1). Cloth has the reference's brushed two-tone felt (`cloth()` in `old_town_04.py`), drawn in the shader from coordinates that stay on the garment; heads are egg-shaped with a jaw and ears; hair and hats are shells cut away round the face (`hair_cap()`), long hair has a lock each side, eyes have a lash line; hems turn under in a clean line. `index.html?lineup=1` stands the six reference people in a row on Linden Street; the comparison is in `Art/StyleTests/people_compare_13/`. |
| 0.5.13 | 2026-11-22 | People's light matched on one standing figure (ART-004). `index.html?studio=1` rebuilds the light and camera of `character_lineup.png` (sun 3.2 from the front left at 48 degrees, sky 0.25, two lanterns, no haze, exposure -0.3, a long lens) round one person; `?studio=2` to `6` show the others. Under it the slice's figure now measures as the reference's: people take more of the blue sky they can see, bounce in proportion to the sun that is actually shining, and a trace of sheen; forms have true surface normals. At the owner's word the face is a little longer than the reference's (head 0.14 tall, not 0.13), the eyes sit further apart (0.042), the mouth is about a third bigger; near people shade themselves: each point of the body is tested against the body's own big shapes (head, hair or hat as a shell with the face cut away, torso, arms, legs) for the sun and for how open it is to the sky, so the fringe shades the forehead, the chin the neck and an arm the body, softly and following the sun (five rays about nine degrees apart and a finely divided head, so a shadow's edge is a gradient); the nose is the reference's oval, a little smaller and higher at the owner's word, and casts its own shadow; the one face nearest the camera has its hair's, nose's, hat brim's and locks' shadows worked out for every pixel in the shader (`heroShade`), with soft edges, the others for every point; the inside of a hair shell or hat is dark. Long hair is one shell that hangs on as a hood, further down the back than at the sides (as `hair_cap()` makes it), with a lock each side of the face, so what shows beside the face and neck is the dark inside of the hood. The upper lid is `pose()`'s: a skin shape that comes down over the eye with the lash line on its edge, and a blink runs it down and up. The resting eyes and brows are built as `pose()` in `character_face_05.py` builds them (a round white set into the face and turned with the head, iris and pupil lying on it, a dark lash shape behind, tapered brows), finely divided so no edge is rough; the lash line is a thin arc over the top of the white, a closed eye is a skin lid with the lash line along its lower edge (`&blink=1` holds the eyes shut to look), and the fringe's edge sits clear of the brows. `&face=1` brings the studio camera up to the head. Comparison in `Art/StyleTests/people_standing_14/`. |
| 0.5.14 | 2026-11-22 | Gardens planted (WLD-040; F-GARDEN level 1, the owner's pick: "1 Living garden"): hedges, flower borders, shrubs and drifts of bulbs and asters in every park and court garden, and shrub groups on the park blocks' outer lawns, placed by `add_garden_01.py`; how each kind looks through the year from `vegetation.json` "plants" (CITY-017). Lawns given patches, mowing stripes, clover, daisies, edge grass, tufts and mulch rings under trees (ART-001). |
| 0.5.15 | 2026-11-22 | The player can play on the playgrounds (WLD-041, owner's request; F-OBJ-POINTS level 1, shared places): swings, seesaws, slides, sandpits and climbing frames, a place a child is on not offered. The second seesaw in each park now rocks under children too (its plank only listened to the first one's places). |
| 0.5.16 | 2026-11-22 | Night sky and night light (WLD-042, ADR-0023), at the owner's request and approved by the owner: stars and the Milky Way that turn with the clock, a moon with phases and its own light, weather deciding how much shows; lanterns no longer flatten the street at night. |
| 0.5.17 | 2026-11-22 | Sun and moon paths by season (WLD-043, ADR-0024), at the owner's request: the sun rises north-east and climbs to 75° in summer, rises south-east and stays at 25° in winter, always at the calendar's times; the moon follows its own path. |
| 0.5.18 | 2026-11-23 | Mall car parks shrunk to a quarter with a forecourt, trees and a bus stop (WLD-054). Bus lines and stops in `World/city_01/transit.json` (CITY-091); the slice draws the buses, their riders and a shelter at every stop, and the player can ride one. |
| 0.5.19 | 2026-11-24 | The town wall taken down (WLD-055, ADR-0034) and the bare fields outside Old Town filled (WLD-056): woods, commons, orchards, farmland, plots for sale and meadows, no buildings, at the owner's request. New trees pine, oak, birch and apple, and wild plants holly, bracken and hawthorn (`vegetation.json`; pines are evergreen). The slice's sign lettering holds 256 names (it held 128 and the town already needed 144). Plan in `Art/StyleTests/outskirts_plan_23.png`. |
| 0.5.19 | 2026-11-23 | The communities' lanes widened from 7 m to 9 m (owner: "make the roads wider near the independent houses"), by `Tools/worldgen/widen_lanes_01.py`; the gardens (`add_houses_03.py`) and the street dressing (`add_lanes_dressing_01.py`) run again from the new edges. Every house keeps its 2 m verge and a garden of 150 m2 or more. |
| 0.5.20 | 2026-11-24 | Eastgate Mall is no longer a plain box (WLD-057, ADR-0037; the owner, from a photograph): a skin of plaster ribbons round the hall, an ochre pierced egg over each corner store, a cream pierced roof light over the court, a round arch on each side with the grand one over the door; lit along its joints and through its openings at night. The shape is data on the lot (`shell`, by `mall_shell_01.py`); the slice draws it (`shell.js`, two new materials: 25 pierced shell, 26 ribbon wall; curved surfaces have their own normals). About 105,000 vertices where the box had 2,000. Westgate and Northgate are as they were, and no mall can be entered yet (F-BLD-MALL has no level picked). |
| 0.5.21 | 2026-11-24 | Eastgate Mall can be gone into, by an entrance under each of its four arches, and has a new inside designed for its shell (WLD-058 to WLD-061, ADR-0038; the owner: "start interiors fresh as this is new design"): an oval court under the roof light, open through three floors, with wandering ribbon galleries; a concourse from each entrance; a round hall under each egg, open into it; bridges and moving stairs in the court; a fountain, gardens, benches and kiosks; shop fronts on every walk. The plan is data (`mall_inside_01.py`); the slice draws it and makes it walkable (`court.js`; material 27, a pierced shell seen from inside). The ground floor keeps its 206 posts, so no resident's work changed; staff stand at their counters (`stand`). Still to come: going into single shops, visitors, lifts. |
| 0.5.22 | 2026-11-24 | Eastgate Mall finished for its level, and what showed wrong inside it put right (WLD-062, ADR-0038 addendum; the owner: "complete the mall ... there are lot of artifacts inside, fix them"; F-BLD-MALL picked at level 2 for the first slice). New: moving stairs that carry people both ways (material 28), a lift in each side concourse (`plan.lifts`), the mall's name over each entrance, daylight in the court and the egg halls. Put right: an orange veil over everything (the indoor lanterns' glow in the air), dark arcs under the galleries and gaps at the parapets' joints (faces with no back), plaster in steps and blocks (noise far from the map's origin, and floors drawn as very long strips), holes between the windows of shut shop fronts, a blotched cap in the roof light (now an open eye in a plain ring), ragged balcony edges in the egg halls. Still to come: visitors (needs F-NPC-POP at 2), buying in single shops. |
