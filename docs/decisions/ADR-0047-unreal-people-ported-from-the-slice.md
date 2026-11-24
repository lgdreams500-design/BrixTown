---
adr: ADR-0047
title: Unreal's people are the slice's option B people, built from code by a checked port of people.js
status: proposed
date: 2026-11-25
relates_to: [ART-004, ANIM-001, ANIM-002, CITY-024, TECH-008, ADR-0004, ADR-0006, ADR-0045, Q-15, Q-17]
---

## Context
The Unreal town's residents walked their routines as stand-ins (a coloured body and a head). The owner asked for real
people and chose, in chat on 2026-11-25, "Port the slice's people": rebuild the browser slice's code-made option B
bodies, faces and procedural walk in C++, with simpler figures further away, and leave Q-15 (how production character
models are made) and Q-17 (procedural walk or authored clips) open.

The slice builds every person in code each frame (`Prototype/play/people.js`): lofted rings for limbs, clothes, hair and
face, posed by the movement signatures (a port of `Tools/blender/motion_lib.py`). There are no model files to import.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Port people.js to C++ and build meshes from code | The same people as the slice and the approved lineup, at once; checkable against people.js line for line; no decision on Q-15 or Q-17 taken | Meshes rebuilt on the CPU each frame or two; nothing Unreal's animation tools can use; thrown away when production models come |
| A skeletal mesh exported from the Blender look tests, animated by the signatures | Closer to how the shipped game will work | Decides much of Q-15 and Q-17 now; a skeleton, skinning and an animation layer to build first |
| Keep the stand-ins until Q-15 is decided | No work thrown away | The town stays empty of people that look like people |

## Decision
- **The people are a port of people.js** (`Game/Source/BrixWorld/BrixBody`): `look()` (role, clothes, colours, walk),
  the motion (`foot`, `pelvis`, `baseHeight`, `leg`, `upper`, `walk`, `stand`), the shapes (`loft`, `limb`, `tube`,
  `ball`) and `draw()` with the face at rest and the far figures (`far()`), with people.js's names kept so the two can be
  read side by side. Left out: the slice's own self-shadowing and texture marks (Unreal's light and materials do that) and
  the poses residents out of doors do not take yet (lying, boards, umbrellas, hoods, talking faces); sitting came
  with the garden benches (CITY-017).
- **The numbers are people.js's**, written to `Game/Data/people.json` by `Tools/unreal/people_values.py`, which runs
  people.js itself in headless Edge. While the slice is kept (ADR-0045) people.js is the source; after its freeze
  people.json is.
- **Checked against people.js**: the same script writes looks and drawn bodies to `Tests/values/people_city_01.json`;
  `ART004_people_are_drawn_as_the_slice_draws_them` requires the same looks, the same vertex counts and every sampled
  vertex within 1 mm.
- **At the slice's distances**: full detail for the nearest within 5 m, a body with a face to 16 m, a simpler body to
  35 m whose face fades out by 30 m, blocks beyond, all in `Config/DefaultGame.ini`. Each body is a procedural mesh
  rebuilt every one to three frames (by detail) and moved every frame; the far figures are one mesh updated in place,
  because moving instanced boxes every frame cost some 8 ms of the render thread on this laptop.
- **Temporary by design.** When Q-15 and Q-17 are decided, these people are replaced; the look tests in
  `Tools/blender/` and the lineup stay the target.

## Consequences
- The town's people look like `Art/Reference/character_lineup.png` and walk as in `motion_walks.mp4`, the same as in the
  slice, with no new art made.
- At High on the owner's laptop a street with 32 residents near and the morning school rush of about 150 both run at
  53 to 54.5 frames a second, the same as with no people (the GPU is the limit). The people take 1.3 and 3.6 ms of the
  game thread: where everyone is and the bodies' building run on the task threads, and the bodies are shared vertices
  (PR #48).
- people.js and BrixBody must be changed together while the slice lives; the test catches a difference.
