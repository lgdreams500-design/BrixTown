---
adr: ADR-0042
title: Skates are a way of moving on foot, three kinds in data, carried in the hand or worn
status: proposed          # asked for by the owner in chat on 2026-11-24; models in look test 30 for the owner's review
date: 2026-11-24
relates_to: [CITY-098, CITY-099, CITY-100, CITY-101, CITY-102, CITY-103, CITY-104, F-VEH-SKATES, INV-001, ECO-002]
---

## Context
The owner asked for skates: Y brings them onto the character, three kinds bought with money at a skate store as the game goes
on (manual 50 km/h, electric 100 km/h, Neon Boost 250 km/h), and `?auto=1` loading the electric pair. Then: the store and
buying come later; for now the models, skating, and the requirements for the store and buying; `?auto=1` owns all three with
the electric pair in use, and a key switches between holding the skates and wearing them.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Skates as a vehicle, like the cars (get in, get out) | Reuses the car's driving | Skates are worn, not entered; a body on skates still jumps, sits and talks; a car's steering is wrong for a person |
| **Skates as a way of moving on foot: the walking character with a different speed rule** | Everything on foot still works (jump, talk, the camera, collisions); one key; the pair shows on the feet or in the hand | Speeds up to 250 km/h on foot need the collision steps the walk already takes, and a slower top speed indoors |
| One pair that gets upgrades | One model | Not what was asked: three kinds to own and choose between |

## Decision
- Skates are kinds in `World/rules/skates.json`: top speed, how fast each gets there, glide and brake, colours. The top speeds
  are the owner's. Prices are not set until the skate store (CITY-102).
- The pair in use is carried in the right hand; Y puts it on (the body stands a wheel higher) and takes it off into the hand.
  On skates the heading turns towards the keys, more slowly at speed; letting go glides; keys against the heading brake;
  indoors the cap is 8 km/h `[ASSUMPTION]`; a knock loses speed.
- The season tester that was on Y moves to F9.
- The slice draws the skates from simple shapes in the colours of look test 30 (`Tools/blender/skates_30.py`) until the
  owner approves a model; then that model is exported for the slice as the cars are (`car_export_11.py`).

## Consequences
- In the engine, skating is a movement mode of the character, server-authoritative (CITY-104).
- `F-VEH-SKATES` is proposed with three levels; the owner has not picked one yet.
