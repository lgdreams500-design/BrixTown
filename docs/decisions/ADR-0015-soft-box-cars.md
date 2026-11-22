---
adr: ADR-0015
title: Vehicles look — "Soft box" replaces "Classic"
status: accepted          # owner in chat on 2026-11-22: "after playing a bit I like option A better"
date: 2026-11-22
relates_to: [CITY-067, ART-001, ADR-0004]
supersedes: ADR-0011
---

## Context
ADR-0011 chose option C "Classic" from the first car look test (`Tools/blender/car_kit_09.py`) and the slice drew those
cars. After playing with them in the slice, the owner preferred option A "Soft box" from the same test.

## Options considered
The same three as ADR-0011: A Soft box, B Pebble, C Classic. See that record for their pros and cons. In play, Soft box
reads as simple and friendly, keeps the cars compact on narrow streets, and stays closest to the slice's earlier shapes.

## Decision
Option A, "Soft box" (`Art/Reference/vehicles_soft_box.png`). ART_DIRECTION §2.5 is rewritten for it (rules V1-V4):
simple shapes with generously rounded edges, matte paint in one accent with a cream roof, open cabins on round posts,
variety from extras.

## Consequences
- `Tools/blender/car_export_11.py` exports the Soft box models (small car, estate, a Soft box pickup added for variety,
  and the van) into `Prototype/play/car_models.js`, the same way car_export_10.py did for Classic.
- `vehicles_classic.png` stays in `Art/Reference/` for the record, marked superseded; car_export_10.py stays runnable.
