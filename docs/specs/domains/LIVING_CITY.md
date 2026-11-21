---
doc_id: WASTED-SPEC-LIVING-CITY
title: Wasted — Living city (map, residents, things to use, vehicles, seasons and plants)
version: 0.3.2
status: draft
last_updated: 2026-11-22
parent: docs/specs/GAME_SPEC.md        # extends §4.6 CITY and §4.7 WLD; same ID rules
prototype: Tools/citysim/               # rules and tests with no engine
scope_source: docs/backlog/feature_choices.json
---

# Living city

How the city stays alive: where people are, what they use, what drives past, how plants change. This
file extends `CITY-` and `WLD-` in the main spec (new numbers start at 010) and follows the same rules.

## 0. How to read scope here

Every requirement names a **feature** and a **level** from `docs/backlog/features.json`.

> A requirement is in scope for a phase only if the owner's pick for that feature at that phase's
> milestone is at or above the requirement's level (`docs/backlog/feature_choices.json`).
> Milestones: first slice = PH1, life depth = PH3, full game = PH6. Undecided means ask.

So there is no Phase column: the owner's picks decide when. Priority says how much it matters once in scope.

## 1. The rule everything follows

**The city lives as data; bodies appear only near players.**

| Kind of state | How it is known | Example |
|---|---|---|
| **Calculated** | A pure function of world time, a seed and data. No history, nothing stored, same answer on every machine. | Where a level 1 resident is at 15:20. How wide a tree is. Today's weather. Where the tram is. |
| **Stored** | Changed by players or by level 2 behaviour. Kept on the server, saved, replicated when relevant. | Who owns a lot. A shop's stock. A resident's needs at level 2. A tree a player planted. |
| **Presented** | Bodies, meshes, sounds. Exists only near a player, built from the two above, thrown away freely. | A resident's body. Leaves on a tree. |

Anything that can be **calculated** must not be ticked or stored (TECH-004). This is what the
prototype proves: `Tools/citysim/citysim.py` answers "where is everyone at minute *t*" for any *t*,
in any order, with tests.

## 2. Controllers

One controller per job. Each is engine-light logic with a data file, a headless test, and a thin
Unreal layer that only presents its answers.

| Controller | Job | Reads | Answers | Prototype |
|---|---|---|---|---|
| **World clock** | Calendar: day, weekday, season, daylight | world time, calendar data | date, market day, rest day, light level | `Clock` |
| **Weather** | Shared weather in spells | time, season odds, seed | clear, cloud, rain, fog | `weather()` |
| **Lot registry** | What every plot is | lot data, ledger | kind, door, hours, owner, occupants | `City.lots` |
| **Path network** | Where people can walk | streets, doors | shortest routes | `Network` |
| **Interaction points** | Places to sit, work, sleep, buy | lot objects | who holds which place, fallbacks | `City.plan()` |
| **Routines** | What each resident does today | role templates, calendar, weather | a timeline of walks and stays | `City.blocks()`, `_timeline()` |
| **Population** | Who needs a body now | everyone's position, viewer positions | load and unload lists | `City.at()`, `City.loaded()` |
| **Vegetation** | Size and leaves of every plant | planting day, species data, calendar | size, leaf, blossom, fallen, replanted | `Vegetation` |
| **Vehicles** | Trams and vans on routes | timetables, routes | positions, stopped or moving | `City.vehicles()` |
| **Road traffic** | Residents' cars: route, queue, park | road network, bays, residents' trips | car positions and states at any time, bay use | not built (CITY-043 to CITY-049) |
| **Player cars** | Cars players own and drive | vehicle records, handling data, player input | live car on the server, parked cars saved | engine only (CITY-060 to CITY-067) |

In Unreal each controller becomes a subsystem in its own module (TECH-003). Which built-in systems to
build on (crowd, shared interaction points, streaming, vegetation tools) is to be checked against
what is production-ready in the pinned engine version before PH1 work starts `[OPEN: Q-18]`.

## 3. Requirements

Status of all rows: `proposed`, except CITY-043 to CITY-049 and CITY-060 to CITY-067, which the owner
accepted on 2026-11-22, and CITY-068 and CITY-069, which follow from the owner's answers the same day (Q-29; "both" for busier streets). "Proven by" names the test in `Tools/citysim/test_citysim.py`, or for cars in `Tools/citysim/test_traffic.py`.

### 3.1 Map, lots and buildings

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| CITY-010 | The city shall be made of blocks of lots laid out from data; a block loads and unloads with player proximity (TECH-006). | F-MAP-CITY | 1 | P0 | |
| CITY-011 | Every lot shall have a record: id, kind, name, footprint, door, owner, opening days and hours, and who lives or works there. Signs, routines and shop hours shall read from that record. | F-MAP-LOTS | 1 | P0 | data file |
| CITY-012 | A lot's use and owner shall be changeable at run time through the ledger, saved, and seen by all players. | F-MAP-LOTS | 2 | P1 | |
| CITY-013 | A lot marked enterable shall have an interior with its own interaction points; residents shown inside are at those points. | F-BLD-INSIDE | 1 | P1 | |
| CITY-014 | An arcade shall hold at least four shops under one roof with shared seating. | F-BLD-MALL | 1 | P2 | |
| CITY-015 | A character shall be able to restore health at a clinic for a fee through the ledger; how many can be treated at once is set by the clinic's places. | F-BLD-HOSPITAL | 1 | P1 | |
| CITY-016 | Each occupied lot shall be charged utilities every cycle (PROP-006), and utility workers shall visit lots on rounds. | F-BLD-UTILITY | 1 | P2 | rounds in data |
| CITY-017 | A garden shall offer benches and paths used by residents and players, planting that follows the seasons, and a gardener who tends it. | F-GARDEN | 1 | P1 | `test_one_person_per_place` |

### 3.2 Residents

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| CITY-020 | Each block shall have 20 to 30 `[ASSUMPTION]` named residents, each with a home lot, a role, a look and a movement signature, all from data. | F-NPC-POP | 1 | P0 | data file |
| CITY-021 | Each resident shall follow a daily routine set by role: timed activities at places, varied a little each day, walking between places along the path network at their signature's speed, never jumping. | F-NPC-ROUTINE | 1 | P0 | `test_nobody_teleports`, `test_at_work_mid_morning`, `test_asleep_at_home_at_night` |
| CITY-022 | A level 1 resident's position and activity at any time shall be calculable from the resident, the world time and data alone, with no stepping and no stored history; any two machines shall give the same answer. | F-NPC-ROUTINE | 1 | P0 | `test_same_answer_every_time` |
| CITY-023 | Routines shall differ between working days, rest days and market days. | F-NPC-WEEK | 1 | P1 | `test_weekend_differs`, `test_market_only_on_market_days` |
| CITY-024 | A resident shall have a body only while within the load distance (40 m `[ASSUMPTION]`) of at least one player. On loading, the body is placed at the position and activity given by CITY-022. Unloading shall lose nothing. | F-NPC-LOAD | 1 | P0 | `test_loading_is_by_distance` |
| CITY-025 | With many players, loading shall use distance tiers (full, simple, data only) and stay within a server budget (CITY-006, MP-006). | F-NPC-LOAD | 2 | P0 | |
| CITY-026 | Rain shall move leisure indoors; street lamps shall follow daylight and windows shall light where someone is home and awake. | F-NPC-REACT | 1 | P2 | choices in data |
| CITY-027 | Ambient animals shall appear by time and place and shall not affect play. | F-ANIMALS | 1 | P2 | |
| CITY-028 | At level 2 a resident shall choose the next activity from their own needs, money and mood. Their state is then *stored* (§1) and advanced in batches, not every frame. | F-NPC-ROUTINE | 2 | P1 | |

### 3.3 Things to use

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| CITY-030 | Interaction points shall be data: type, position, number of places and who may use them. Players and residents shall use the same points through the one interaction framework (CITY-001). | F-OBJ-POINTS | 1 | P0 | data file |
| CITY-031 | One place at an interaction point shall never be held by two characters at once. When no place is free a resident shall take a fallback: another kind of point or a spot nearby. | F-OBJ-POINTS | 1 | P0 | `test_one_person_per_place` |
| CITY-032 | Residents shall carry objects between places as part of their routines (deliveries, shopping). | F-OBJ-CARRY | 1 | P2 | |
| CITY-033 | Objects shall hold a state (stock, dirt, damage) that residents notice and act on. | F-OBJ-POINTS | 2 | P1 | |
| CITY-034 | A player asking for a place that a resident holds shall get it within a short, data-set time `[OPEN: Q-19]`. | F-OBJ-POINTS | 1 | P1 | |

### 3.4 Vehicles

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| CITY-040 | Vehicles shall follow fixed routes on a timetable; their position shall be calculated from time, and they shall stop where their route says. | F-VEH-TRAFFIC | 1 | P1 | `test_vehicles_keep_to_their_routes`; in the city: delivery vans on rounds from the depots, `test_vans_make_their_rounds` |
| CITY-041 | A tram line shall run on a timetable; residents who work elsewhere shall leave and return by tram; players shall be able to ride it. | F-VEH-TRANSIT | 1 | P1 | `test_at_work_mid_morning` |
| CITY-042 | Players shall be able to own and ride a light vehicle. | F-VEH-DRIVE | 1 | P2 | |

#### Streets and resident traffic

Residents' cars follow the rule in §1: a resident car's day is **calculated** like the resident's own,
queues included, and gets a body only near a player. Only cars that players drive are live. ADR-0009
has the reasoning.

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| CITY-043 | Streets shall carry a road network as data: one lane each way, junctions with a control (give way or signals) and a speed limit, parking bays along the kerb, and crossings where footpaths meet a road. People walk on pavements and cross at crossings (the suggested answer to Q-24). The data checks shall refuse a lane that does not connect and a lot whose door has no bay within 150 m `[ASSUMPTION]`. | F-VEH-TRAFFIC | 2 | P0 | `test_road_network_is_sound`, `test_every_door_has_a_bay_within_reach` |
| CITY-044 | Residents whose data gives them a car (a share of households, 30 % `[ASSUMPTION]`) shall own one: a vehicle record with id, owner, model and home bay. A trip in their routine longer than 250 m `[ASSUMPTION]` shall be driven: walk to the car, drive, park in a free bay near the destination, walk to the door. Shorter trips stay on foot. | F-VEH-TRAFFIC | 2 | P0 | `test_drivers_park_and_walk_to_the_door`, `test_short_tours_are_walked`, `test_about_the_right_share_of_households_has_a_car` |
| CITY-045 | A resident car's position, speed and state (parked, moving, waiting) at any time shall be calculable from world time, seed and data, with no stepping and no stored history; any two machines shall give the same answer. Waits at junctions and behind other cars shall be worked out when the day's plan is made, not live. | F-VEH-TRAFFIC | 2 | P0 | `test_traffic_same_answer_every_time` |
| CITY-046 | Resident cars shall keep to their lane, keep to the speed limit, give way at junctions and at crossings with someone on them, stop at red signals, and never share a lane space with another car. Cars shall choose the quickest route by the network's speed limits. | F-VEH-TRAFFIC | 2 | P0 | `test_cars_never_overlap_in_a_lane`, `test_cars_keep_to_the_speed_limit`, `test_cars_give_way_at_junctions`, `test_cars_choose_the_quickest_route`, the `test_busy_day_` tests; `test_cars_wait_for_walkers_at_crossings` is skipped until people walk on pavements |
| CITY-047 | A parking bay shall hold one car at a time. When no bay is free near the destination, the driver shall take the next nearest free one (as CITY-031 does for places). | F-VEH-TRAFFIC | 2 | P1 | `test_one_car_per_bay` |
| CITY-048 | A resident car shall have a body only within the car load distance (120 m `[ASSUMPTION]`, wider than for people because cars are faster) of a player. On loading it is placed where CITY-045 says. Parked cars beyond that distance may show as simple scenery. | F-VEH-TRAFFIC | 2 | P0 | `test_cars_load_by_distance` |
| CITY-049 | Near a car or person that a player controls, resident cars shall slow, wait or stop instead of colliding; once clear, each shall rejoin its calculated timeline without speeding up past the limit. A delayed car catches up only by skipping its next stop; one more than 10 minutes `[ASSUMPTION]` behind is unloaded and placed back on its timeline out of sight (Q-25). | F-VEH-TRAFFIC | 2 | P0 | |
| CITY-068 | A car owner who works out of town shall drive out through a town gate instead of taking the tram, leaving early enough to be through the gate when the tram would have left; while away the car is off the map; it comes back through the same gate when the tram would have brought them back, and drives on to their next place (Q-29). Commuters without a car keep the tram (CITY-041). | F-VEH-TRAFFIC | 2 | P1 | `test_commuters_with_a_car_drive_out_and_back` |
| CITY-069 | Cars from elsewhere shall pass through town from one gate to another, a number each hour set in data, routed and queued like residents' cars and off the map before and after. | F-VEH-TRAFFIC | 2 | P1 | `test_through_traffic_crosses_town` |

#### Cars players drive

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| CITY-060 | A character shall be able to buy a car at a car dealer, paid through the ledger (ECO-002), with ownership recorded on the server (PROP-001), and sell it again. Models and prices shall be data `[ASSUMPTION]`. Dealers are a new lot kind, `dealer`, with a forecourt of bays; the starting city has one on a through street (Q-26). | F-VEH-DRIVE | 2 | P0 | |
| CITY-061 | The owner, and players the owner allows, shall get in at a door, drive in third person with keyboard or gamepad, and get out. Top speed, acceleration, braking and steering shall be data per model. Getting in, sitting at the wheel and getting out shall be motions added to CHARACTER_MOTION. | F-VEH-DRIVE | 2 | P0 | |
| CITY-062 | Driving shall be server-authoritative: the client sends inputs only, the server moves the car and corrects the client, and a car moving faster or further than its model allows is put back. | F-VEH-DRIVE | 2 | P0 | Engine test `CITY-062` |
| CITY-063 | A player may park in any free bay or on their own lot. A parked car shall stay where it was left, be saved, and survive a server restart. A bay taken by a player's car is unavailable to residents, who take the next nearest (CITY-047). | F-VEH-DRIVE | 2 | P0 | Engine test `CITY-063` |
| CITY-064 | Cars shall collide with walls, props and other cars. A car that hits a person stops and the person is pushed aside; nobody is hurt, and cars take no damage (Q-27). | F-VEH-DRIVE | 2 | P0 | |
| CITY-065 | Other players shall be able to ride as passengers in a car with free seats (from PH2). | F-VEH-DRIVE | 2 | P1 | |
| CITY-066 | Cars shall need no fuel and shall cost a small weekly upkeep `[ASSUMPTION]`, charged through the ledger from PH4 with the other recurring costs (PROP-006; Q-28). | F-VEH-DRIVE | 2 | P2 | |
| CITY-067 | Every car model shall match the art direction (ART-001): a look test in `Tools/blender/` comes before any production model. | F-VEH-DRIVE | 2 | P1 | Look test |
| CITY-070 | A resident on foot outdoors who has a moving car a player drives heading at them, due to reach them within a reaction time set in data, shall get out of its path: after a short startle, a hurried scramble (quick running steps and a small hop) sideways to the side with room, far enough to clear the car, then carry on with their day. This is presented state only (§1): an offset on top of where CITY-010 puts them, near players, never stored; their calculated timeline is unchanged. All times and distances are in `World/rules/dodge.json` `[ASSUMPTION]`. | F-NPC-DODGE | 1 | P1 | `test_dodge_rules` (data); slice |
| CITY-071 | Once clear, the resident shall turn to the car and tell the driver off: a raised fist or both arms flung up (an arm gesture layer, CHARACTER_MOTION §4) and a short shout shown as a tag over them (a line from data; no words spoken, Q-14), then go back to where their day has them and what they were doing. | F-NPC-DODGE | 2 | P1 | `test_dodge_rules` (data); slice |

### 3.5 Seasons, weather and plants

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| WLD-010 | The year shall have four seasons of equal length (12 game days each `[ASSUMPTION]`); daylight hours, weather odds and plants shall read the season. | F-SEASONS | 1 | P1 | `Clock` |
| WLD-011 | A plant's size shall be a function of its age and species data only (refines WLD-005). | F-VEG-GROW | 1 | P1 | `test_trees_grow_from_age` |
| WLD-012 | Each species shall have its own dates for bud, full leaf, blossom, autumn colour and bare branches; fallen leaves shall appear under trees and clear away. | F-VEG-LEAF | 1 | P1 | `test_leaf_cycle` |
| WLD-013 | Trees shall have a lifespan. At its end a tree falls, the site stands empty, and a sapling is planted; all dates come from the tree's seed. | F-VEG-LIFE | 1 | P2 | `test_trees_fall_and_are_replanted` |
| WLD-014 | Weather shall be calculated from time and a world seed, come in spells, and be the same for every player (refines WLD-003). | F-WEATHER | 1 | P1 | `test_weather_is_shared_and_seasonal` |

## 4. Numbers used by the prototype

All `[ASSUMPTION]`; all live in `Tools/citysim/data/block_01.json`, none in code (TECH-005).

| Number | Value | Note |
|---|---|---|
| Block | 160 m square: a ring of buildings round a courtyard garden | Six homes, grocer, café, workshop, office, clinic, depot, market, tram stop |
| Residents | 27 in 11 roles | Speeds come from the movement signatures in CHARACTER_MOTION |
| Year | 48 game days, 12 a season | With WLD-001's 60 real minutes a day, a year is 48 hours of play |
| Week | 7 days; market Wednesday and Saturday; rest Saturday and Sunday | |
| Load distance | 40 m | |
| Tram | every 20 minutes, 05:40 to 23:00 | |
| Rain chance by season | 30 %, 16 %, 36 %, 26 % of six-hour slots | |

Numbers for cars, also `[ASSUMPTION]`, in `World/rules/traffic.json`:

| Number | Value | Note |
|---|---|---|
| Households with a car | 60 % | CITY-044. Was 30 %; raised for busier streets (owner, 2026-11-22) |
| Shortest trip driven | 0 m | Car owners drive every errand (owner, 2026-11-22). Was 600, then 250 m (§5.1) |
| Vans | 6, from the two depots, 3 rounds of 4 shops from 07:30, 6 min at each stop, 20 at the depot | CITY-040 |
| Cars passing through | 0 to 8 an hour, most at 08:00 and 17:00 | CITY-069 |
| Speed limit | 30 km/h in the old quarters, 40 km/h on through streets | CITY-046 |
| Bay within reach of every door | 150 m | CITY-043 |
| Car load distance | 120 m | CITY-048 |

## 5. What the prototype does not cover

Interiors, the mall, animals, carried objects, object state, needs-driven choices, several blocks,
many players, saving. Cars (CITY-043 to CITY-049) are in `traffic.py` and `test_traffic.py`, run over the whole starting city; what
they leave out is in §5.1.
Residents do not avoid each other or queue. Stays that end while someone is
still walking shorten the walk rather than re-plan. These are levels or requirements above; none is
built.

### 5.1 Cars: what the prototype shows and leaves out

`traffic.py` works out a day of car trips once, in time order, on the lanes of `World/city_01/roads.json`, and
then answers any car's position by lookup (`render_traffic.py` draws it). On day 16 with the rules as set,
198 households have a car (60 %); with 6 vans and 81 cars passing through there are 720 trips: 441 errands, 54
commuters out through a gate and back (CITY-068), 90 van legs, 81 through. Something is on the road in 822 of the 960
minutes from 06:00 to 22:00 (198 before vans, through traffic and more cars); at most 29 at once. Through traffic keeps
to Market and Baker Streets, so back streets stay quieter. With every household driving every trip: 326 cars, 950 trips,
up to 42 on the road at once, no overlap, no crossing movements in a box, nobody through a red light.

- **Finding: residents' lives are short-range.** The generator gives everyone the café, shop, garden and clinic
  nearest their home and staffs workplaces nearest first, and commuters leave by tram. No car owner has a trip
  longer than 432 m, so at the accepted 600 m nobody drove. The threshold is now 250 m `[ASSUMPTION]`. Since
  Q-29, car-owning commuters drive out through a town gate (CITY-068); errands still go to the nearest place.
- **Traffic is light.** Even the busy run peaks at 42 cars on the road at once; queues are short. Junction and
  queue rules are proven to hold, not yet proven to look right at rush hour.
- **Left out for now:** walkers on pavements and crossings (so cars do not yet wait for people;
  `test_cars_wait_for_walkers_at_crossings` is skipped), the tram sharing Tram Street, acceleration (cars go
  straight to the speed limit), lane changes, players' cars (CITY-049 and CITY-060 onward are engine work),
  and cars stopping for the player. `world.simulation(w, traffic=True)` turns cars on; `build_play.py` uses it, so the
  playable slice shows residents' cars (WORLD_MAP §5.1).

## 6. Open questions

| ID | Question | Default if unanswered |
|---|---|---|
| Q-18 | Which built-in Unreal systems carry the controllers (crowds, shared interaction points, streaming, vegetation)? | Check the pinned version's production-ready list at the start of PH1; prefer production-ready, wrap experimental ones behind our own controller interface. |
| Q-19 | How long may a resident keep a player waiting for a bench, counter or bed? | Residents give way at once to a player unless they are working at that point. |
| Q-20 | Length of the game year and week? | 48-day year, 7-day week, as in the prototype. |
| Q-25 | When a player's car or body blocks a resident car, how far may it drift from its calculated timeline, and how does it catch up? | **DECIDED 2026-11-22:** it waits; it catches up only by skipping its next stop (as CITY-052 does for walkers), never by speeding. A car more than 10 minutes behind `[ASSUMPTION]` is unloaded and placed back on its timeline out of sight. |
| Q-26 | Where are cars bought, and is a car dealer a new lot kind in the city data? | **DECIDED 2026-11-22:** a new lot kind `dealer`, one in the starting city on a through street, with a forecourt of bays. |
| Q-27 | What happens when a car hits a person, and do cars take damage? | **DECIDED 2026-11-22:** nobody is hurt: people are pushed aside and the car stops. Cars take no damage until a later level. |
| Q-29 | Should commuters who own a car drive out of town instead of taking the tram, and should some errands go beyond the nearest shop or café? | **DECIDED 2026-11-22** (owner: "drive out"), now CITY-068: car-owning commuters drive out through a town gate on the wall streets; the tram stays for everyone else. Errands stay nearest. |
| Q-28 | Do cars need fuel, and what do they cost to keep? | **DECIDED 2026-11-22:** no fuel; a small weekly upkeep through the ledger, from PH4 with the other recurring costs (PROP-006). |

## 7. Change log
| Version | Date | Change |
|---|---|---|
| 0.1.0 | 2026-11-21 | First version, with the city-in-a-box prototype and its tests. |
| 0.2.0 | 2026-11-22 | Cars and real traffic drafted after the owner's picks (`F-VEH-DRIVE` 2, `F-VEH-TRAFFIC` 2 from the first slice): CITY-043 to CITY-049 for streets and residents' cars, CITY-060 to CITY-067 for cars players drive, car numbers in §4, Q-25 to Q-28, ADR-0009. All proposed. |
| 0.2.1 | 2026-11-22 | Owner accepted CITY-043 to CITY-049, CITY-060 to CITY-067 and ADR-0009, and the suggested answers to Q-25 to Q-28, now written into CITY-049, CITY-060, CITY-064 and CITY-066. |
| 0.3.2 | 2026-11-22 | Owner found the streets empty and asked for both: delivery vans on rounds (CITY-040 in the city) and cars passing through (CITY-069, new), and residents driving more (60 % with a car, every errand driven). |
| 0.3.1 | 2026-11-22 | Q-29 decided by the owner: car-owning commuters drive out through a town gate (CITY-068, `test_commuters_with_a_car_drive_out_and_back`). Four gates where Market and Baker meet the wall, in `roads.json`. Cars now keep the lower limit as soon as they cross into a slower stretch. |
| 0.3.0 | 2026-11-22 | Cars proved in `Tools/citysim/traffic.py` with `test_traffic.py` (19 pass, 1 skipped) over the whole starting city; road data in `World/city_01/roads.json`, rules in `World/rules/traffic.json`. Shortest driven trip 600 → 250 m `[ASSUMPTION]` (§5.1). Q-29 added. |
