---
adr: ADR-0004
title: Art direction — old-town world, "Soft" characters, no painterly filter
status: accepted
date: 2026-11-21
relates_to: [ART-001, ART-002, ART-003, ART-004, ART-005, ART-006, Q-10, Q-15]
amended_by: ADR-0007     # daylight only; everything else here stands
---

## Context
The game needed one visual target before any asset work. The owner wanted to see realistic and
stylised options, disliked hand-drawn 2D mock-ups, and asked for options built in real 3D tools,
taking a painterly ruin image as inspiration.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| 2D sketches of realistic and stylised looks | Fast | Rejected by owner: not convincing |
| Style test 02 with a painterly filter | Close to the inspiration image | Owner preferred the unfiltered render; a filter costs frame time and hides detail |
| **Style test 02, raw render** (old town, low sun, haze) | Chosen by owner; plain rendering, achievable in engine | Soft light and haze need care on weak hardware |
| Three "playful" looks (toybox, pastel dusk, paper town) | Distinct | Owner steered back to style test 02 |
| Character A "Natural" / **B "Soft"** / C "Simple" | B reads well at distance and suits a designer | B needs a bespoke base model (not MetaHuman proportions) |

## Decision
- World: the old-town look of `Art/Reference/world_alley_morning_raw.png`, rules in
  `docs/art/ART_DIRECTION.md` §2. No painterly or outline post-process.
- Characters: option B "Soft" (about 5.6 heads tall, head ×1.22, whole-garment clothing in the world
  palette), rules in `docs/art/ART_DIRECTION.md` §3.

## Consequences
- Every new asset and light setup is checked against `Art/Reference/`.
- Stylised proportions rule out using MetaHuman bodies as-is; production models are Q-15.
- The look depends on haze and soft bounce light; an Unreal recipe that holds frame rate on modest
  hardware is a PH1 task.
- Creator content (UGC-003) must be specified to this palette and material style.
