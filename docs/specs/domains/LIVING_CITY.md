---
doc_id: WASTED-SPEC-LIVING-CITY
title: Brix Town — Living city (map, residents, things to use, vehicles, seasons and plants)
version: 0.6.0
status: draft
last_updated: 2026-11-24
parent: docs/specs/GAME_SPEC.md        # extends §4.6 CITY and §4.7 WLD; same ID rules
prototype: Tools/citysim/               # rules and tests with no engine; careers in careers.py, test_careers.py
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

Status of all rows: `proposed` (CITY-081 to CITY-088 from the owner's requests of 2026-11-23 for more and more realistic careers), except CITY-043 to CITY-049 and CITY-060 to CITY-067, which the owner
accepted on 2026-11-22, and CITY-068 and CITY-069, which follow from the owner's answers the same day (Q-29; "both" for busier streets), and CITY-072 to CITY-076 (crash damage), from the owner's request and answers the same day, and CITY-089 to CITY-095 (cars by kind of home, the roads outside the wall, buses, bus rides, parked cars off the street, car pooling, no car on days it is not used), from the owner's requests and answers on 2026-11-23 (F-VEH-TRANSIT level 2 "Network", the tram kept). "Proven by" names the test in `Tools/citysim/test_citysim.py`, or for cars in `Tools/citysim/test_traffic.py`.

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
| CITY-017 | A garden shall offer benches and paths used by residents and players, planting that follows the seasons, and a gardener who tends it. | F-GARDEN | 1 | P1 | `test_one_person_per_place`, `test_CITY_017_planting_follows_the_seasons`, `test_CITY_017_gardeners_tend_the_borders` |

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
| CITY-077 | Each resident shall decide once a day, from the season and whether the day has rain in it, whether they wear a coat (some with a hood) and carry an umbrella; chances and colours in data. | F-NPC-REACT | 1 | P2 | `test_CITY_077_dress_for_the_weather` |
| CITY-078 | When rain starts, a resident out in the open (a garden, the market as a customer) shall stop within a few minutes, go to cover (their nearest café or grocer's awning, or home) and wait there until the rain stops, then go back if enough of the stay is left. Someone due out in the open while it already rains goes straight to cover. | F-NPC-REACT | 1 | P2 | `test_CITY_078_take_cover_when_rain_starts`, `test_CITY_078_back_out_when_the_rain_stops` |
| CITY-079 | Walks in the rain shall be faster than the resident's own pace: a little under an umbrella, more without, and running for cover fastest; factors in data. | F-NPC-REACT | 1 | P2 | `test_CITY_079_hurry_in_the_rain` |
| CITY-080 | Out in the rain a resident shall have their umbrella up, else their hood up, else go bare-headed (a hand over the head when walking); nothing under a roof or awning. | F-NPC-REACT | 1 | P2 | `test_CITY_080_cover_in_the_rain` |
| CITY-027 | Ambient animals shall appear by time and place and shall not affect play. | F-ANIMALS | 1 | P2 | |
| CITY-081 | Careers shall be data (`World/rules/careers.json`): more than 50 `[ASSUMPTION: 122]`, each with a title, a group and the routine role it follows. Every resident who works shall have one; the slice shows it as their title in talk and over their head. | F-NPC-POP | 1 | P1 | `test_CITY_081_more_than_fifty_careers_in_data`, `test_CITY_081_everyone_who_works_has_a_career` |
| CITY-082 | Each kind of workplace shall have posts in data: careers with a min and max head count, moving from min to max with the place's floor area. A place never has more of a career than its posts, and always has its minimum. A café has one or two at the counter, one or two baristas and one table attendant; a grocer two or three at the tills and one or two stocking shelves or helping customers; the others in proportion (`staffing`). | F-NPC-POP | 1 | P1 | `test_CITY_082_cafes_have_two_at_the_counter_two_baristas_and_one_cleaning_tables`, `test_CITY_082_grocers_have_two_or_three_at_the_tills_and_one_or_two_on_the_floor`, `test_CITY_082_every_workplace_within_its_posts`, `test_CITY_082_no_more_crowds_behind_the_counter`, `test_CITY_082_bigger_places_have_more_posts` |
| CITY-083 | A share of residents working in town (8 % `[ASSUMPTION]`, band 5 to 10 %) shall work from home in creative careers (painter, musician, composer, novelist, poet, illustrator, sculptor, songwriter, screenwriter, potter, photographer, game designer): their routine has them `create` at home through the working day and some evenings. | F-NPC-ROUTINE | 1 | P1 | `test_CITY_083_artists_work_from_home`, `test_CITY_083_artists_spend_the_working_day_creating_at_home` |
| CITY-084 | Each group of careers shall take a share of residents working in town set in data (`shares`; office 39 % `[ASSUMPTION]`), so office work (software, banking, back office and operations, law, insurance, the town hall) is the largest group, at least 30 % of everyone who works, commuters included. Posts beyond a group's share go to in-commuters (CITY_BALANCE 2.2). | F-NPC-POP | 1 | P1 | `test_CITY_084_office_workers_are_the_largest_group`, `test_CITY_084_no_group_takes_more_than_its_share` |
| CITY-085 | Every worker shall have their own post at their workplace: their own place to work there, which nobody else takes (as beds at home). | F-OBJ-POINTS | 1 | P1 | `test_CITY_085_each_worker_stands_at_their_own_post` |
| CITY-086 | Who does which job shall be worked out from the residents' homes, the lots and the rules alone, the same in any order: every workplace's minimum filled first by the nearest residents, then everyone else to the nearest workplace whose group is under its share. Too few posts for the people is a data error. | F-NPC-POP | 1 | P1 | `test_CITY_086_careers_are_worked_out_from_the_rules_alone`, `test_CITY_086_minimums_first_nearest_first`, `test_CITY_086_too_few_posts_is_an_error` |
| CITY-087 | Couriers and postal workers shall be a small share of all residents (1 % `[ASSUMPTION]`, `couriers_share`; the owner, 2026-11-23: there were far too many at 5 %); the rest work in town. `balance.json`'s courier target is the same number. | F-NPC-POP | 1 | P1 | `test_CITY_087_couriers_and_postal_workers_are_few` |
| CITY-088 | A career may have a second job (`second_jobs`). Waste collectors (routine `binman`) clock in at the waste depot about 05:10, collect bins on a round of their nearby places from 05:30 to about 08:40, return to the depot, and after a break do a day job as a courier, postal worker or parcel driver on rounds until mid-afternoon; their title shows both jobs (the owner, 2026-11-23). | F-NPC-ROUTINE | 1 | P1 | `test_CITY_088_waste_collectors_collect_at_dawn_then_do_a_day_job` |
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
| CITY-089 | Whether a household has a car shall follow its kind of home (`car_share` in `traffic.json`, by `home_kind`): every independent house outside the wall has one, about half the row homes, and few flats in the centre (10 % `[ASSUMPTION]`); people with no car of their own walk or take the bus (CITY-092). Owner, 2026-11-23. | F-VEH-TRAFFIC | 2 | P0 | `test_CITY_089_cars_by_kind_of_home` |
| CITY-090 | The roads outside the wall shall be part of the road network, worked out from the data (roads.json `links`, block `lanes`): one lane each way, give way where they meet, a turn at a turning circle or dead end, no junction where one road runs on into another. Each house there has its car's home bay in front of it, on the roadway against its edge (there are no lay-bys outside the wall and the verge belongs to the garden fences); other places there have bays the same way, and the communities' lanes have parking along them. Cars from outside drive in and out of town through the gates; a commuter from outside leaves the map at the gate. | F-VEH-TRAFFIC | 2 | P0 | `test_CITY_090_houses_outside_the_wall_drive_their_lanes` |
| CITY-091 | Bus lines shall run on the road network from `World/<city>/transit.json`: each line out to its last stop and back, leaving its first stop every `every_min` minutes from `first` to `last` while a run can end before the day does; never leaving a stop before its timetabled time (which allows a red light at every signalled junction, `buses` in `traffic.json`), waiting its dwell at every stop, queuing and giving way like cars, with more room ahead and behind for its length. A stop is a bay on each side of the street that no car parks in; it slides along the street (up to `stop_slide_m`) to the nearest spot where the bus and its shelter stand clear of lamps, racks, bins, benches, planters, arches and trees (`obstacle_m`). Buses are off the map outside service hours. | F-VEH-TRANSIT | 2 | P1 | `test_CITY_091_buses_keep_to_their_lines`, `test_CITY_091_bus_stops_keep_clear_of_street_things` |
| CITY-092 | A resident with no car of their own whose walk is longer than `ride_min_m` (400 m `[ASSUMPTION]`; only shorter walks and breaks stay on foot) shall take a bus instead, from a stop within `stop_reach_m` of where they are to one near where they go, when the two walks together are short beside the walk it replaces. Their day moves to fit the bus: they may set off up to `shift_min` early and arrive up to `late_min` late, the least change winning; straight on from another walk they board the first bus after reaching the stop, and straight on into another walk they wait where the bus left them. They walk to the stop, wait, ride (their position is the bus's) and walk on. Owner, 2026-11-23: "too many citizens are walking long paths to work they should use buses". Players can ride a bus in the slice (E at a bus standing at a stop). | F-VEH-TRANSIT | 2 | P0 | `test_CITY_092_residents_without_a_car_ride_the_bus` |
| CITY-093 | A resident's car parked away from home shall be taken off the street for a share of such stays (`park_away.share`, 60 %, by the seed), from shortly after its driver has walked away to shortly before they come back to it; its bay stays theirs. Cars at home are always there. Owner, 2026-11-23: "de spawn 60% of the cars once the npc leaves them after commute". | F-VEH-TRAFFIC | 2 | P1 | `test_CITY_093_most_cars_parked_away_from_home_leave_the_street` |
| CITY-094 | Neighbours shall share the drive to work and home: a car owner whose day leaves home for the town and comes home from it, with a neighbour (homes within `pool.reach_m`, 300 m `[ASSUMPTION]`) who drives off at about the same time (`pool.window_min`) to near where they go (`pool.dest_m`) and home at about the same time from near where they are, rides with them, up to `pool.seats` a car; their own car stays at home that day. They walk to the driver's car by the time the driver gets in, ride, and walk on; their day moves to fit, at most `pool.shift_min`. If a shared ride fits neither way they keep their own car; if it fits one way, the other is walked or taken by bus (CITY-092). Commuters who drive out through a gate (CITY-068) are not pooled yet. Owner, 2026-11-23: "improve car pooling to close by residents to reduce cars further". | F-VEH-TRAFFIC | 2 | P1 | `test_CITY_094_neighbours_share_the_drive` |
| CITY-095 | A household shall have a car on a day only if someone drives it that day: a car that would stand all day (its household walks, takes the bus or rides with a neighbour, CITY-092, CITY-094) is not on the map and not theirs that day. Worked out with the day's plan, like everything else about cars. Owner, 2026-11-23: "if cars are not used at all and parked all day then remove them and their ownership". | F-VEH-TRAFFIC | 2 | P1 | `test_CITY_095_cars_not_used_today_are_not_there` |
| CITY-096 | Each resident shall have their own habit, the same every day: their role's times shifted earlier or later within a window set per role in data (`habit_min` in `roles.json`: offices over three hours, artists, elders, works, nurses and commuters two, shop and café rotas one; children and students keep the school bell `[ASSUMPTION]`), with the day-to-day jitter on top. Lunch between two spells at work shall be taken near work (`near_work` in `choices.json`, for the `lunch` choice: the nearest café, grocer or market to the workplace), and at home only when home is within 300 m `[ASSUMPTION]` of work. Worked out from the seed and the map, not stored. Owner, 2026-11-23: "spread the timings of the npc ... that will spread the traffic over time so there wont be cluttering". | F-NPC-ROUTINE | 1 | P1 | `test_CITY_096_habits_spread_the_day_and_lunch_is_near_work` |
| CITY-097 | No vehicle shall be held at a junction for long: at traffic lights walkers cross on their own phase, so a car with green goes (it never goes on red); at a zebra without lights a car gives way to anyone on it, but one that has waited `crossing_patience_s` (20 s `[ASSUMPTION]`) is let through. No vehicle stands still longer than a signal cycle plus that wait. People on foot at the lights wait at the kerb until their road has red with time to cross (`walk_at_lights.margin_s` to spare); the wait comes out of the stay after the walk, or they set off that much earlier, so the rest of their day keeps its times; only a walk run straight into another walk is crossed as it comes. Owner, 2026-11-23: "fix cars getting stuck at traffic lights". | F-VEH-TRAFFIC | 2 | P1 | `test_CITY_097_no_car_is_held_at_a_junction_for_long`, `test_CITY_097_walkers_cross_at_the_lights_on_red` |
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
| CITY-072 | A car a player drives that hits a street thing (lamp, bench, café table and chairs, bin, planter, bollard, post box, telephone box, notice board, crates, market stall, flowerbed, bush or garden plant, a panel of a garden fence, wall, railing or hedge), a tree or the corner of a building shall break it at any speed (the owner, 2026-11-22: no speed limit for damage; `break_kmh` 0 in `World/rules/crash.json`). | F-VEH-CRASH | 1 | P1 | Slice |
| CITY-073 | Breaking shall be shown: tall things (lamps, trees, posts, telephone boxes) topple the way they were hit and lie there; low things burst into pieces in their own colours that fly, bounce and settle as rubble; a building corner loses chips of stone and shows its rough core. The car loses speed. A broken lamp goes out and a broken bench cannot be sat on. | F-VEH-CRASH | 1 | P1 | Slice |
| CITY-074 | Damage shall be stored state (§1): what was broken, where and when. The server keeps it, saves it and shows it to every player until it is mended; nothing about it is re-rolled. (The slice keeps it in the browser.) | F-VEH-CRASH | 2 | P1 | Slice |
| CITY-075 | A repair crew from the nearest depot shall mend each damaged thing, in the order the damage happened, within working hours (07:00 to 19:00 `[ASSUMPTION]`), walking there at their own pace and working a time set per kind; then the thing is whole again. When each thing is mended is calculated from the damage list and the rules alone, the same on every machine. | F-VEH-CRASH | 2 | P1 | `test_repairs` |
| CITY-076 | Residents outdoors within 18 m `[ASSUMPTION]` who see something smashed shall turn to it, throw up their arms and shout at the driver (as CITY-071). | F-VEH-CRASH | 2 | P2 | Slice |

#### Skateboards

The owner, 2026-11-24: skates the character puts on with Y, three kinds bought with money at a skate store as the game goes on:
manual (fast, 50 km/h), electric (very fast, 100 km/h) and Neon Boost (super fast, 250 km/h); `?auto=1` starts with the electric
kind. The same day: the store and buying come in a later change; this change brings the models, riding and the requirements
for the store and buying only; `?auto=1` gives all three kinds with the electric one in use, and Y switches between holding
them and using them. Then, after looking at the skates (look test 31): "convert to skate board and remove walk animation. for
manual board use one feet drag on the ground to move and gain speed when W is pressed. for electric and neon, no feet drag
animation." The owner picked `F-VEH-SKATES` level 2 for the first slice. Kinds, speeds, sizes and colours are data in
`World/rules/boards.json` (ADR-0042).

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| CITY-098 | There shall be three kinds of skateboard, as data: manual (top speed 50 km/h), electric (100 km/h) and Neon Boost (250 km/h), each with how fast it gets to its top speed, how it glides when let go and how hard it brakes, whether a foot pushes it (the manual board only), its size and its colours. Each kind up the store is faster than the one before. | F-VEH-SKATES | 1 | P0 | `test_CITY_098_board_kinds_and_speeds` |
| CITY-099 | A character with a skateboard shall hold the board in use in the hand and, with one key (Y on a keyboard, RB on a gamepad), get on it, and with the same key step off and pick it up again. On a board there is no walk: the rider stands side-on across the deck, knees a little bent, looking ahead. A and D turn the rider left and right, and the camera comes round behind them as behind a car, more slowly the faster the board goes (the owner, 2026-11-24); W goes the way they face and S brakes. Only W speeds a board up: once W is let go it glides to a stop, whatever else is held. The manual board speeds up only while W is held, and while it is the rider pushes it; the push stops the moment W is let go (the owner, 2026-11-24): the front foot on the board pointing forward along it (the owner, 2026-11-24: "Foot forward to push") and the back foot on the ground beside it, pushing back and lifted forward again; the electric and Neon Boost boards speed up by themselves while W is held, and the rider stands still on them. Indoors no board goes faster than 8 km/h `[ASSUMPTION]`. Running into a wall or a car loses the speed into it. Not on a seat, in a car or on the playground. | F-VEH-SKATES | 1 | P1 | Slice (`index.html?auto=1`, Y); `?studio=2&board=manual&push=1&turn=90` |
| CITY-100 | Skateboards a character owns shall be among their belongings (INV-001): each kind with its top speed, the board in use marked, and any other owned board can be made the one in use. They are saved with the game. `?auto=1` (tests, trials) shall give every kind, with the electric board in use; a new game has none until the skate store (CITY-102). | F-VEH-SKATES | 1 | P1 | `test_CITY_100_auto_owns_all_and_rides_electric`; slice |
| CITY-101 | Every skateboard model shall match the art direction (ART-001): a look test in `Tools/blender/` comes before any production model. The slice draws each kind at the size and in the colours of that look test. | F-VEH-SKATES | 1 | P1 | Look test `Tools/blender/skateboards_32.py` (the skates before it: `skates_31.py`); slice `index.html?auto=1&studio=2&board=electric` |
| CITY-102 | A skate store shall be a place in town (a shop lot, as CITY-060 does for the car dealer) that stocks the three kinds of board, with a price for each in data in minor units `[ASSUMPTION]`, cheapest to dearest in the order of CITY-098, so better skates are a goal to save up for. The kinds stand on show and can be looked at before buying. | F-VEH-SKATES | 2 | P0 | Not built |
| CITY-103 | A character shall buy a skateboard at the skate store's counter, paid through the ledger (ECO-002), with ownership recorded on the server (PROP-001); a kind already owned is not sold twice. The bought board goes to their belongings (CITY-100) and becomes the one in use if it is faster than the one they had. Whether boards can be sold back is `[OPEN: Q-40]`. | F-VEH-SKATES | 2 | P0 | Not built |
| CITY-104 | Riding a skateboard shall be server-authoritative: the client sends inputs only; the server moves the character and puts back one moving faster than the board in use allows, or riding a kind they do not own. | F-VEH-SKATES | 1 | P0 | Engine test `CITY-104` |

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

Numbers for careers, also `[ASSUMPTION]`, in `World/rules/careers.json` (CITY-081 to CITY-086):

| Number | Value | Note |
|---|---|---|
| Careers | 122 in 15 groups; 116 held by someone in the city | CITY-081 |
| Café | counter 1-2, baristas 1-2, table attendant 1 (3 to 5 staff, by floor area 200-600 m²) | CITY-082; was up to 24 |
| Grocer | tills 2-3, stock 1, shop floor 0-1 (3 to 5) | CITY-082; was up to 16 |
| Bakery, kiosk | baker 1-2 and counter 1-2; kiosk keeper 1 and 0-1 more | CITY-082 |
| Works | foreman 1, machinists and assemblers 1-5 each, welders, carpenters, inspectors, forklift drivers (3 to 21) | CITY-082; was up to 36 |
| Office tower | about 190 to 240 a tower: developers, analysts, accountants, support, HR, payroll, compliance, underwriters, cleaners, security | CITY-082, CITY-084; was 12 |
| Shares of those working in town | office 39 %, retail 17.2 %, making 10.5 %, food 10 %, creative (at home) 8 %, health 6 %, utility 2.7 %, education 2.6 %, facilities 1.4 %, services 1.2 %, culture 0.8 %, showrooms 0.6 % | CITY-084. On the starting city: 991 office workers, 41 % of the 2,401 who work |
| Home-based | 156 (8 %) | CITY-083 |
| Couriers and postal workers | 1 % of residents: 40 (were 202, 5 %) | CITY-087 |

Numbers for cars, also `[ASSUMPTION]`, in `World/rules/traffic.json`:

| Number | Value | Note |
|---|---|---|
| Households with a car | houses 100 %, row homes 50 %, flats 10 % | CITY-089 (owner, 2026-11-23). Was 60 % of homes inside the wall, none outside |
| Buses | 3 lines, every 15 or 20 minutes from about 06:00; 27 buses on day 16 | CITY-091, `transit.json`; the timetable allows a red light at each signalled junction (30 s) |
| Bus rides | a walk of 400 m or more, stops within 400 m, the two walks at most 75 % of the walk; a day may move 30 min earlier or 10 min later to fit a bus | CITY-092 |
| Shortest trip driven | 0 m | Car owners drive every errand (owner, 2026-11-22). Was 600, then 250 m (§5.1) |
| Vans | 6, from the two depots, 3 rounds of 4 shops from 07:30, 6 min at each stop, 20 at the depot | CITY-040 |
| Cars passing through | 0 to 8 an hour, most at 08:00 and 17:00 | CITY-069 |
| Speed limit | 50 km/h on every street for residents' cars, vans and through traffic; the player's car goes up to 100 km/h (`player_top_kmh`) | CITY-046. Owner, 2026-11-22; were 30 and 40. A car is on the road in 772 of the 960 minutes 06:00-22:00 (median trip 29 real seconds) |
| Bay within reach of every door | 150 m | CITY-043 |
| Pavements and zebras | People walk a line 6.6 m from the centre line (`walk_offset`) and cross only on zebras 10 m from each junction; a car waits while someone is on the zebra it would drive over, from 0.5 s before to 2.5 s after (`crossing_clear_s`) | CITY-043, CITY-046. Owner, 2026-11-22: people walked in the road and there were no crossings |
| Traffic lights | At every junction on a through street (17), not only where two cross (`signals_on_through`) | CITY-046. Owner, 2026-11-22 |
| Parking bays | 1,387 bays marked in the kerb edge, each clear of anything standing there (lamps, bins, bollards, trees) and 11 m clear of a shop or cafe door (`bay_clear_of_front`); were 2,280 | CITY-043. Owner, 2026-11-22: cars stood among things on the pavement |
| Car load distance | 120 m | CITY-048 |
| Skateboards | Manual 50, electric 100, Neon Boost 250 km/h top speed (the owner, 2026-11-24); they get there at 12, 30 and 70 km/h a second, glide down at 6 to 10 and brake at 40 to 90 km/h a second; 8 km/h indoors; the manual board's push 1.3 a second (`World/rules/boards.json`) | CITY-098, CITY-099 |

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
| Q-40 | Can skateboards be sold back, and for how much? | A sold-back board would let players trade up and get some money back. | Suggested: no selling back in the first slice; skates are kept. Undecided. |

## 7. Change log
| Version | Date | Change |
|---|---|---|
| 0.6.0 | 2026-11-24 | Skates made skateboards at the owner's word: CITY-098 to CITY-104 and Q-40 reworded; no walk on a board, the manual board pushed with one foot while W is held, the powered boards ridden standing; rules in `World/rules/boards.json`; look test 32 (the skates' look test renumbered 31, as 30 was taken on another branch). |
| 0.5.9 | 2026-11-24 | Skates (`F-VEH-SKATES`, proposed in `features.json`, no level picked): CITY-098 to CITY-104 and Q-40 from the owner's request; kinds and speeds in `World/rules/skates.json` (ADR-0042). CITY-098 to CITY-101 in the slice; the store and buying (CITY-102, CITY-103) are written, not built. All proposed. |
| 0.1.0 | 2026-11-21 | First version, with the city-in-a-box prototype and its tests. |
| 0.2.0 | 2026-11-22 | Cars and real traffic drafted after the owner's picks (`F-VEH-DRIVE` 2, `F-VEH-TRAFFIC` 2 from the first slice): CITY-043 to CITY-049 for streets and residents' cars, CITY-060 to CITY-067 for cars players drive, car numbers in §4, Q-25 to Q-28, ADR-0009. All proposed. |
| 0.2.1 | 2026-11-22 | Owner accepted CITY-043 to CITY-049, CITY-060 to CITY-067 and ADR-0009, and the suggested answers to Q-25 to Q-28, now written into CITY-049, CITY-060, CITY-064 and CITY-066. |
| 0.5.8 | 2026-11-23 | Bus stops keep clear of street things (CITY-091; owner: "bus stuck in the cycle stand"): 29 of 50 stop bays had a lamp, tree, cycle stand, bin, planter or postbox under the bus or the shelter. Each stop now slides along its street, up to 40 m, to the nearest clear spot; Southfield East moves one street east, where there is room. All 50 are clear. |
| 0.5.7 | 2026-11-23 | People on foot wait at the lights for their phase (CITY-097): on day 16, 94 % of the 7,300 crossings at the lights are walked on red (5,700 of them after a wait at the kerb); days stay whole. |
| 0.5.6 | 2026-11-23 | Cars were held at the lights for up to 16 minutes: they waited for walkers on the crossing even on green, and the busiest crossings at the lights have someone on them 50 to 65 % of the day, so the queue round a junction locked. Now CITY-097: at the lights a car on green goes, at zebras a car is let through after 20 s. Day 16: no vehicle stands still more than 32 s (was 956 s), 831 stops over a minute -> 0. |
| 0.5.5 | 2026-11-23 | Habits and lunch near work (CITY-096); a second stop in Southfield (Southfield East, line 2) so every home is within 400 m of a stop (CITY-091); bus riders wait spread along a stop and step off in turn (CITY-092). Day 16: walking groups of 8 or more in a 4 m square 282 -> 194 (the biggest left are children walking to the tram for school), vehicles on the road at the peak 126 -> 99, bus rides about 2,600. Two tests read anew: CITY-078 counts residents gone indoors as under cover too (more were due home at the start of the rain), and CITY-083 artists are at their art by mid-morning with a two-hour window. |
| 0.5.4 | 2026-11-23 | Cars nobody drives on a day are not there that day (CITY-095): on day 16, 247 of 548 households have no car on the map. |
| 0.5.3 | 2026-11-23 | Car pooling between neighbours (CITY-094): on day 16, 101 passengers share 123 commute halves and 247 of 548 cars stay at home all day (301 drive). |
| 0.5.2 | 2026-11-23 | Bus rides from 314 to about 2,400 a day: walks from 400 m (was 450), stops within 400 m, a day moves up to 30 min early or 10 min late to fit a bus, and a walk that follows or leads into another walk can be ridden too (CITY-092). 60 % of cars parked away from home leave the street until their driver comes back (CITY-093). |
| 0.5.1 | 2026-11-23 | Bushes, garden plants and garden fences, walls, railings and hedges break too, a panel from post to post at a time (CITY-072; owner: "make the bushes and fences destructible everywhere"). Outside the wall cars drive in the middle of their half of the 9 m lanes and park against the roadway edge. |
| 0.5.0 | 2026-11-23 | Cars by kind of home, the roads outside the wall driven, buses (owner: "residents outside the core town living in independent house own the cars"; F-VEH-TRANSIT level 2, keep the tram; one PR): CITY-089 to CITY-092. 548 cars (415 houses, 121 row homes, 12 flats), 27 buses on 3 lines, about 300 bus rides and 4,600 vehicle trips on day 16. Fixes found on the way: a car enters a junction only with room beyond for its whole length and for any car already turning into that lane (no more gridlock with buses); a driver never leaves before their plan frees them; a household whose car would be out near midnight walks that day. |
| 0.4.2 | 2026-11-23 | CITY-088: waste collectors have two jobs, the bin round at dawn and a courier day job (new routine `binman`, act `collect`; a rounds entry may name its own act). |
| 0.4.1 | 2026-11-23 | CITY-087: couriers and postal workers cut from 5 % of residents to 1 % at the owner's request; the 162 others work in town. Office towers take a few more. |
| 0.4.0 | 2026-11-23 | Careers and staffing (owner's request: more varied careers, artists at home, realistic staff counts, more office workers): CITY-081 to CITY-086, `World/rules/careers.json`, proved in `Tools/citysim/careers.py` with `test_careers.py`, written into the city by `Tools/worldgen/add_careers_01.py`. New routine role `creative`. |
| 0.3.7 | 2026-11-22 | CITY-017 gardens (owner's pick, F-GARDEN level 1): garden plants (box, lilac, hydrangea, rose, lavender, daffodil, tulip, aster) look as their dates in `vegetation.json` "plants" say on any day, worked out by `Vegetation.plant()` with no stored state (`test_CITY_017_planting_follows_the_seasons`); something flowers every day from early spring to the end of autumn, the evergreens stay green in winter. Gardeners tend the new borders at places added to each garden's flowerbed places (`test_CITY_017_gardeners_tend_the_borders`). |
| 0.3.6 | 2026-11-22 | Pavements, zebras, lights and bays (owner): people walk on pavement lines and cross on zebras, cars wait for them; signals at every junction of a through street; bays kept clear of things at the kerb and of shop fronts. §4 numbers. |
| 0.3.5 | 2026-11-22 | CITY-072: things break at any speed, not only above 20 km/h (owner). |
| 0.3.4 | 2026-11-22 | Crash damage (F-VEH-CRASH level 2 for the first slice, owner): CITY-072 to CITY-076; repairs proved in `Tools/citysim/repairs.py`. |
| 0.3.3 | 2026-11-22 | Speed limit 50 km/h on every street for traffic; the player's car up to 100 km/h, at the owner's request (§4). |
| 0.3.2 | 2026-11-22 | Owner found the streets empty and asked for both: delivery vans on rounds (CITY-040 in the city) and cars passing through (CITY-069, new), and residents driving more (60 % with a car, every errand driven). |
| 0.3.1 | 2026-11-22 | Q-29 decided by the owner: car-owning commuters drive out through a town gate (CITY-068, `test_commuters_with_a_car_drive_out_and_back`). Four gates where Market and Baker meet the wall, in `roads.json`. Cars now keep the lower limit as soon as they cross into a slower stretch. |
| 0.3.0 | 2026-11-22 | Cars proved in `Tools/citysim/traffic.py` with `test_traffic.py` (19 pass, 1 skipped) over the whole starting city; road data in `World/city_01/roads.json`, rules in `World/rules/traffic.json`. Shortest driven trip 600 → 250 m `[ASSUMPTION]` (§5.1). Q-29 added. |
