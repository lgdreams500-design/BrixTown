---
adr: ADR-0042
title: Skateboards are a way of moving on foot, three kinds in data, carried in the hand or ridden, no walk on a board
status: proposed          # asked for by the owner in chat on 2026-11-24, first as skates, then the same day as skateboards; models in look test 35 for the owner's review
date: 2026-11-24
relates_to: [CITY-098, CITY-099, CITY-100, CITY-101, CITY-102, CITY-103, CITY-104, F-VEH-SKATES, INV-001, ECO-002]
---

## Context
The owner asked for skates: Y brings them onto the character, three kinds bought with money at a skate store as the game goes
on (manual 50 km/h, electric 100 km/h, Neon Boost 250 km/h), and `?auto=1` loading the electric kind. Then: the store and
buying come later; for now the models, riding, and the requirements for the store and buying; `?auto=1` owns all three with the
electric kind in use, and a key switches between holding them and using them. After seeing the skates (look test 34, then
in the slice) the owner said: "convert to skate board and remove walk animation. for manual board use one feet drag on the
ground to move and gain speed when W is pressed. for electric and neon, no feet drag animation."

## Options considered
| Option | Pros | Cons |
|---|---|---|
| A board as a vehicle, like the cars (get in, get out) | Reuses the car's driving | A board is stood on and carried, not entered; the rider still jumps and talks; a car's steering is wrong for a person |
| **A board as a way of moving on foot: the character with a riding pose and a different speed rule** | Everything on foot still works (jump, talk, the camera, collisions); one key; the board shows underfoot or in the hand | Speeds up to 250 km/h on foot need the collision steps the walk already takes, and a slower top speed indoors |
| Keep skates | Already built | Not what the owner wants now |

## Decision
- Boards are kinds in `World/rules/boards.json`: top speed, how fast each gets there, glide and brake, whether a foot pushes it,
  deck size and height, colours. The top speeds are the owner's. Prices are not set until the skate store (CITY-102).
- The board in use is carried in the right hand, deck upright; Y gets on it and picks it up again.
- On a board there is no walk (pose 5 in `people.js`, `ride()`): the rider stands side-on across the deck, knees bent, looking
  ahead. The manual board moves only while W is held, and while it is, the rider turns to face ahead and pushes with the back
  foot on the ground (front foot on the board by the front truck), 1.3 pushes a second; let go and the rider eases back into the
  side-on stance and glides. The electric and Neon Boost boards go by themselves while W is held; the rider stands still.
- Heading turns towards the keys, more slowly at speed; keys against the heading brake; indoors the cap is 8 km/h
  `[ASSUMPTION]`; a knock loses speed.
- The season tester that was on Y moves to F9.
- The slice draws the boards from simple shapes at the size and in the colours of look test 35 (`Tools/blender/skateboards_35.py`)
  until the owner approves a model; then that model is exported for the slice as the cars are (`car_export_11.py`). The skates of
  look test 34 stay as the record of what came before.

## Consequences
- In the engine, riding a board is a movement mode of the character, server-authoritative (CITY-104), with its own motions
  (stance, push) added to CHARACTER_MOTION.
- The owner picked `F-VEH-SKATES` level 2 ("Skate store and tiers") for the first slice on 2026-11-24; CITY-102 and CITY-103
  are still to be built. The feature keeps its ID; its title is now Skateboards.
