---
adr: ADR-0066
title: A thing seen in the game is built only against an approved reference
status: accepted          # the owner in chat, 2026-11-26: "check if we update the dev instructions to follow this process of first creating references before start adding features to the game"
date: 2026-11-26
relates_to: [ART-001, ADR-0004, ADR-0058, ADR-0061, ADR-0065]
---

## Context
On 2026-11-26 the owner and an agent remade the town's references one place at a time: the alley, the sky, Market
Square, the street, a home's room, its hallway and the bakery, each at every setting it is seen in, in one light, and
approved one by one on the reference page (ADR-0061). Along the way the owner found what goes wrong when a thing is
built first and its look judged later: rooms lit in a light of their own beside the town's, a hallway that was the
room again, one setting standing for all. The process (FEATURE_PROCESS) went from proving the rules straight to
building in Unreal, and the look gate let a thing with no reference be built against "the nearest reference".

## Decision
- A new stage in `docs/process/FEATURE_PROCESS.md`, stage 6 **Reference**, between proving the rules and building:
  anything seen in the game is built only once it has an approved reference in `Art/Reference/catalog.json`. With
  none, or only a proposed one, the reference is made first and the owner reviews it.
- A reference is one thing, made by a script, at every setting it is seen in, lit by the town's one light, with its
  locks, issues and what was tried written down (FEATURE_PROCESS, rules for AI agents).
- The look gate (`docs/process/LOOK_GATE.md` 1.1.0) no longer lets new work stand on the nearest reference; work
  already in the game without one says so in its README's "Not yet" until its reference is approved.
- `CLAUDE.md` says it where agents start: no approved reference, no building it.

## Consequences
- Features with something to see start with a reference review, which costs the owner a look and saves rework.
- Still without approved references: a shop without a fire (bank, grocer), the furniture outlet's hall, the mall
  (Eastgate's court), and the vehicles, people and screens not yet remade at every setting; work on those waits for them.
