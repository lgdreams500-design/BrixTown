---
doc_id: WASTED-SPEC-LIVING-CITY
title: Wasted — Living city (map, residents, things to use, vehicles, seasons and plants)
version: 0.1.0
status: draft
last_updated: 2026-11-21
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

In Unreal each controller becomes a subsystem in its own module (TECH-003). Which built-in systems to
build on (crowd, shared interaction points, streaming, vegetation tools) is to be checked against
what is production-ready in the pinned engine version before PH1 work starts `[OPEN: Q-18]`.

## 3. Requirements

Status of all rows: `proposed`. "Proven by" names the test in `Tools/citysim/test_citysim.py`.

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
| CITY-040 | Vehicles shall follow fixed routes on a timetable; their position shall be calculated from time, and they shall stop where their route says. | F-VEH-TRAFFIC | 1 | P1 | `test_vehicles_keep_to_their_routes` |
| CITY-041 | A tram line shall run on a timetable; residents who work elsewhere shall leave and return by tram; players shall be able to ride it. | F-VEH-TRANSIT | 1 | P1 | `test_at_work_mid_morning` |
| CITY-042 | Players shall be able to own and ride a light vehicle. | F-VEH-DRIVE | 1 | P2 | |

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

## 5. What the prototype does not cover

Interiors, the mall, animals, carried objects, object state, needs-driven choices, several blocks,
many players, saving. Residents do not avoid each other or queue. Stays that end while someone is
still walking shorten the walk rather than re-plan. These are levels or requirements above; none is
built.

## 6. Open questions

| ID | Question | Default if unanswered |
|---|---|---|
| Q-18 | Which built-in Unreal systems carry the controllers (crowds, shared interaction points, streaming, vegetation)? | Check the pinned version's production-ready list at the start of PH1; prefer production-ready, wrap experimental ones behind our own controller interface. |
| Q-19 | How long may a resident keep a player waiting for a bench, counter or bed? | Residents give way at once to a player unless they are working at that point. |
| Q-20 | Length of the game year and week? | 48-day year, 7-day week, as in the prototype. |

## 7. Change log
| Version | Date | Change |
|---|---|---|
| 0.1.0 | 2026-11-21 | First version, with the city-in-a-box prototype and its tests. |
