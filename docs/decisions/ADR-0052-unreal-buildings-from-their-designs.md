---
adr: ADR-0052
title: Unreal builds every building from its design, and the remodel designer edits that design
status: accepted
date: 2026-11-25
relates_to: [BUILD-001, BUILD-002, BUILD-004, BUILD-005, BUILD-016, BUILD-018, BUILD-019, BUILD-020, ADR-0004, ADR-0046, ADR-0048]
---

## Context
Every building in the slice is a design (BUILD-001: world.designs(), lot.dsg) and the slice builds only from it
(BUILD-002). Until now Unreal did not use the designs for the outside. `ABrixTownActor::BuildLot` laid the kit's pieces
(ADR-0046) and picked each building's plaster, frame, shutter and door colours at random from DefaultGame.ini lists,
seeded by the lot id. Only the designed houses used their colourway, and only the insides used the design's finishes.
So the Unreal town's colours were not the slice's, and a remodel designer would have nothing to change.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| **Build every building's colours from its design, as the slice does** | One source of truth with the slice; a remodel shows at once; BUILD-002 met | Most buildings change colour from what Unreal showed before |
| Keep the random colours and use designs only for remodelled buildings | The Unreal town looks as before | Two ways to colour a building; Unreal's town is not the slice's; a remodel's "before" is not its design |

## Decision
The owner said "Yes, use design colours" in chat (2026-11-25), when asked whether Unreal's buildings should take the
slice's design colours so the designer could be built.

- `build_play.py` writes each lot's design into `Game/Data/inside.json` (`dsg`), with the building designer's kit,
  planning rules and fixed run (`builder`).
- `BrixWorld/BrixBuilder` ports builder.js: built(), allowed(), problems(), cost(), options() and check().
- `BuildLot` takes each building's colours from `BrixBuilder::Built()`:
  - each facade's plaster;
  - each facade's stone or brick (pattern and colour), up to the eaves where the design says so;
  - the roof's tiles, the window frames, and the shutters (or none).
- The kit's own geometry stays as ADR-0046 lays it: windows, doors, the stone ground storey in the old districts, roofs
  and parapets. Designed houses keep their colourway (ADR-0032).
- A building can be drawn again from another design (`ABrixTownActor::Repaint`). Its instances are folded away under the
  ground and it is built afresh, so the rest of the town is untouched.
- The remodel designer (ABrixLife, life.js remodelPanel()) edits a copy of the design. It prices each change line by
  line, checks it as builder.py does, and pays through the ledger. A paid remodel is there at once
  (F-BLD-CONSTRUCT level 0) and is kept in the save.

## Consequences
- Unreal's street colours are now the slice's, building by building. This is a change from the Unreal look shown before
  (ADR-0004), made at the owner's word. Shot: `Art/StyleTests/unreal_designer_39/street_designs.png`.
- Not drawn from the design yet: the window class's sizes, balconies, keystones, corner stones and wall lanterns. The
  designer offers, prices and keeps them, but the kit's own rules still place them. The awning's stripes are also not
  yet in the design's accent.
- Repainting a building more than once leaves its old instances folded away until the town is next built: a few hundred
  instances each time, well within what the town holds.
