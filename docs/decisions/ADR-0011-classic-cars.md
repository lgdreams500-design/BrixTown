---
adr: ADR-0011
title: Vehicles look — "Classic" small vintage cars with round wings
status: accepted          # owner chose option C in chat on 2026-11-22
date: 2026-11-22
relates_to: [CITY-067, ART-001, ADR-0004]
amends: ADR-0004          # adds vehicles to the locked look; nothing else changes
---

## Context
ART_DIRECTION §5 listed vehicles as "not started". The playable slice drew cars as stand-ins built from boxes; the owner
asked for them to be cute and then for curves and smoother edges instead of boxes. CITY-067 asks for a look test in
`Tools/blender/` before any production model.

## Options considered
`Tools/blender/car_kit_09.py` rendered three directions, each as a small car, an estate and a van in the locked world
look (painted matte materials, kit daylight, an option B person for scale):

| Option | Pros | Cons |
|---|---|---|
| A Soft box: the slice shapes with rounded edges | Closest to the slice; cheapest to draw | Still reads as boxes from a distance |
| B Pebble: one or two pillowy forms with a bubble roof | Most toy-like; closest to the soft people | Big shapes lose their kind (the van reads as a blob) |
| C Classic: round wings over the wheels, long round bonnet, running boards, lamps on the wings | Clear silhouettes; vintage character that suits the old town; cute without being shapeless | Most parts per car |

## Decision
Option C, "Classic" (`Art/Reference/vehicles_classic.png`). Rules V1-V4 in ART_DIRECTION §2.5: small vintage soft
cars, matte paint in one palette accent with a cream roof, open cabins on round posts so people show, variety from
extras rather than new shapes.

## Consequences
- The slice draws the classic models from meshes exported out of the look test (low-poly, two levels of detail) instead
  of boxes made in code.
- The same models are the starting point for the Unreal cars; production meshes still need their own pass (Q-15 covers
  how production models are made).
- A and B stay in `Art/StyleTests/` as rejected directions.
