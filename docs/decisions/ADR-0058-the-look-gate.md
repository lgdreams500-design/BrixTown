---
adr: ADR-0058
title: Anything seen in the game passes a look gate when it is made, measured against the references and not the slice
status: accepted          # the owner, 2026-11-26: "slice was just a prototype, you need to aim even better then slice in the unreal. instead of just fixing mall build a process where this is taken care as part of creations instead of waiting for audit to detect it later."
date: 2026-11-26
relates_to: [ADR-0004, ADR-0045, ADR-0055, ART-001, TEST-001]
---

## Context
The Unreal build is a port of the browser slice, made by many sessions, each proving its rules with automated tests
and a few test shots. Twice in one day the owner, playing, found things no session had seen:

- every car and van drawn inside out, for days (the models' triangles went round the wrong way);
- Eastgate Mall with every face glowing in one plain colour, no shade or shadow in it, and the player a black shape
  at night. An agent then mended the black player and called the mall checked without noticing the rest.

The cause is the same in both: shots were read for the one thing being worked on, never as a whole against the look.
And the port's own aim was wrong: "as the slice draws it" was treated as done, so the slice's shortcuts (rooms that
light themselves, no shadows) were carried over as if they were the look.

## Decision
1. **The bar is `Art/Reference/` and the rules of ART_DIRECTION, not the slice.** The slice was a prototype; the
   Unreal build is to look better than it. Where the two differ, the references and rules win.
2. **A look gate is part of building** (`docs/process/LOOK_GATE.md`, stage 6 of FEATURE_PROCESS): before a pull
   request that changes what is seen, its author takes shots at noon, dusk and night, puts them beside the
   references, answers ten whole-frame questions (G1 to G10: shade, shadows, surfaces, colour, whites and blacks,
   people, lamps, wholeness, the hours, nothing over the picture), runs `Tools/unreal/look_check.py`, keeps the
   sheet in `Art/StyleTests/`, and writes every "no" it does not mend in the README's "Not yet".
3. **Shared changes are checked everywhere**: the look tour (`Tools/unreal/light_tour.txt`) against a kept baseline
   of each stop's numbers (`Tools/unreal/look_baseline.json`); a stop that has moved is explained or mended.
4. **A new kind of place adds a tour stop**, so later changes are looked at there.
5. CLAUDE.md carries the rule, so every session meets it.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| **A look gate at creation, by the author (chosen)** | Found when cheapest to mend; the author knows what they changed | Costs each visual pull request a few minutes of shots and a table |
| A periodic audit by one session | No cost to feature work | Finds things late, after others have built on them; this is what failed |
| Automatic picture tests only (compare with a golden picture) | No judgement needed | The picture changes with every honest change; cannot say whether a new thing is right, only that an old one moved |
| Leave it to the owner's play | No work | The owner's time; trust lost each time |

## Consequences
- A visual pull request without the sheet and the G1 to G10 table is not ready.
- `look_check.py` catches gross failures only (a black player, a white-out, a plain fill, a move from the baseline).
  The side-by-side and the ten questions are the gate; the numbers are its floor.
- The first audit's findings are listed in LOOK_GATE.md and in the README's "Not yet" lines; they are mended in
  following pull requests, the mall first.
- Eastgate's inside still has no reference image (ART_DIRECTION §5); until one is made it is judged by the interior
  rules (I, W1, W5, W6).
