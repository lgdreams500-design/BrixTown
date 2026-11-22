---
adr: ADR-0017
title: Stone, brick and paving chosen by rules in data, drawn as patterns
status: accepted          # the owner saw samples of five wall and five paving patterns and said "yes, go ahead" in chat on 2026-11-22
date: 2026-11-22
relates_to: [ART-001, WLD-020, WLD-038, WLD-039, ADR-0004, ADR-0007]
amends: ADR-0004          # adds brick (a warm red-brown) and four more stone and paving patterns to the locked look
---

## Context
Every wall's stone in the slice was one pattern (ashlar, 1 m by 0.5 m) and every pavement and yard another (slabs,
0.83 m by 0.42 m), in one colour each. The owner saw that the buildings and pavements all look the same and asked for
variety, with samples first. Which surface gets which pattern also has to be easy to change later and to hold for
blocks a generator adds, without anyone having to set a texture on each building.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Pick a pattern in the slice's code by district | Quick | Knowledge of the map in code (breaks WLD-020); a change means editing code |
| Write a pattern into every lot when the city is generated | Explicit | 450 values to keep right; every new block must remember to set one; a change of mind means regenerating |
| **Rules in one data file, first match wins, with an optional override on any lot or block** | One place to change; new blocks are covered by the rules with no extra step; a single building can still be pinned | The rules have to be read to know what a building gets (`world.surfaces()` lists it) |

## Decision
`World/rules/surfaces.json` holds:
- **patterns**: each one's type (bond, Flemish bond, coursed rubble, flagstones, herringbone, basket weave), unit size,
  joint width, how much the stones vary, edge cut or dome, and its colours (`stone` and `pave` mean the town's own).
  A wall pattern's `upper` share also covers the plaster storeys of that many of its buildings.
- **rules** for three slots: `wall` (a building's stone), `pavement` (a block's ground and the pavements round it),
  `ground` (an open lot: the market, a tram stop). The first rule whose `if` matches (template, district, kind, tags or
  name) wins. Its `use` names one pattern or weights several, picked by the lot's or block's id so it never changes.
- **overrides**: a `surface` field on any lot or block (`{"wall": "rubble"}`) beats the rules.

The first set of rules is the mix the owner approved:

| Where | Walls | Ground |
|---|---|---|
| Old quarter (`peri`) | coursed rubble, now and then ashlar | flagstones |
| Terraces (`rows`) | brick, running and Flemish bond, some ashlar; about half brick to the eaves | slabs, a third herringbone |
| Town Hall, bank, museum, library, hospitals | rusticated base | flagstones on the hall and hospital blocks |
| Mall | | herringbone |
| Parks | | basket weave |
| Market Square | | setts |
| Works, depots | ashlar, some brick | |
| Everything else | ashlar, as before | slabs, as before |

`world.surfaces()` resolves the rules; `world.surface_problems()` checks them (known types, sizes in range, every
named pattern exists and suits its slot, each slot's rules end with a catch-all). `build_play.py` hands the result to
the slice, which draws a pattern as material 30 plus its number, its sizes in a 16-entry uniform table, so new patterns
need no new shader code unless they need a new type. Kerbs and wet ground work on every paving pattern.

## Consequences
- Changing the look of a district is an edit to `surfaces.json`; changing one building is a `surface` field in its
  block file. A generator writes nothing about surfaces.
- Brick brings a warm red-brown into the town. It is kept dark and matte, painted like the rest (W1), so it sits with
  the terracotta roofs rather than against the plaster.
- Up to 16 patterns (the shader's table); six types. A new type needs a few lines in the slice's `pat()` and, later,
  the same pattern as a material in Unreal.
- In Unreal each pattern becomes a material instance with the same numbers; the rules carry over unchanged.
