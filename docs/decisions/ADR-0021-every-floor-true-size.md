---
adr: ADR-0021
title: Every floor of every building, true size, reached by stairs (and lifts in the towers)
status: accepted          # the owner picked F-BLD-INSIDE level 3 "Every floor, true size", "Stairs and a lift" and "Residents too" in chat on 2026-11-22
date: 2026-11-22
relates_to: [WLD-050, WLD-051, WLD-052, WLD-053, CITY-050, CITY-051, F-BLD-INSIDE, Q-23, ADR-0017]
---

## Context
Only ground floors could be entered. Above them every building was one solid box, so the windows upstairs led
nowhere and residents slept on the ground floor. The owner asked for every building to be accessible inside, with
stairs to each level, and for the inside to be the same size as the outside. Q-23 (how upper floors are reached) was
open.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Upper floors as separate places, reached through a door that fades to them | Cheap; no geometry to fit | Not the size seen from outside; you never see the stair; the owner asked for the real building |
| A stair modelled in each building by hand | Freedom of design | 456 buildings; nothing a generator could add later |
| **A stairwell placed by rules in the data, the shell made hollow, each floor built near the camera** | Every building the same way; new blocks get stairs from the generator; the outside does not change | Stair placement is a heuristic; floors are open plan (no inner walls yet) |

## Decision
- **Heights**: the ground floor at 0.2 m with its ceiling at 3.2 m (as before); floor f at 3f + 0.4 m under a 0.2 m slab.
  So the top ceiling meets the roof exactly at floors × 3 + 0.4 m, the height every building already had, behind
  the same windows. Nothing seen from outside moves.
- **Stairs**: every building of two or more storeys gets a switchback stairwell against a side wall (never the door's).
  Each storey has two flights in lanes 1.1 m wide with a spine wall between them and a landing at each end, so each
  floor's flight starts where the last one arrived. Risers are at most 0.19 m (the player steps up 0.5 m), and the
  slab is open over each flight with a rail at its edge. `Tools/worldgen/add_storeys_01.py` places it where it stands
  furthest from the ground floor's places and the way in; once placed it stays.
- **Lifts**: buildings of `lift_from_floors` (6) storeys or more (the five towers) have a lift as well. It goes up or
  down a floor, to the ground, to the top floor and to the roof.
- **Use of each floor**: `World/rules/storeys.json` `uses`, by kind:
  - houses: bedrooms upstairs;
  - tower homes: a flat on every floor;
  - offices: offices;
  - shops and cafés: a storeroom, then rooms;
  - hospital and clinics: wards;
  - library: stacks;
  - museum: a gallery;
  - utilities: a plant room.
- **Residents**: `move` takes every bed in houses and flats upstairs, and half the desks in offices. Each place's
  floor is kept in the lot's `levels`, in the same order as its objects. No two floors have a place at the same spot
  on the plan, because the slice finds a resident's place by where it is.
- **The slice** (`Prototype/play/storeys.js`):
  - at load: the shell's upper walls, slabs, steps, spine, rails and lift shaft, all solid;
  - near the camera: the ground floor of nearby buildings with its stair, and the camera's own floor and the floors
    either side of it, furnished by `furnish()` through a per-floor stand-in for the lot;
  - room light, contact shade and lamps follow the floor the camera is on.
- Both substations are made enterable (each gets an engineer's place), so every building can be entered.

## Consequences
- Every floor of every building can be walked, and residents sleep and work upstairs.
- Upper floors are open plan: no inner walls or doors between rooms yet. Residents arrive at an upstairs place
  without walking up the stairs (they climb in the rules' time, not on screen). Both are follow-ups.
- Building the floors near the camera keeps the cost to the one building you are in; the ground-floor rooms of the
  street stay as before.
- Unreal: the same data gives each building its stair, lift and floors. The slab, flight and rail sizes are the
  ones to model.
