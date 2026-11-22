---
adr: ADR-0026
title: Interior and furniture look — four style lines from one kit
status: accepted          # owner in chat on 2026-11-22, shown furniture look test 17: "All four lines"
date: 2026-11-22
relates_to: [FURN-002, FURN-003, FURN-005, FURN-008, ART-001, ADR-0004, ADR-0007, ADR-0015]
---

## Context
Interiors were "not started" in ART_DIRECTION §5. The owner picked furniture designs (`F-FURN-ITEMS`) and the furniture
company and outlets (`F-FURN-STORE`) at level 2 for the first slice, so furniture needs a locked look before it is made.
A draft of rules I1–I9 and four style lines (`docs/art/INTERIOR_STYLE.md`) was rendered by
`Tools/blender/furniture_kit_17.py`: a sheet of every piece in each line, one furnished room per line in the kit
daylight, the Old Town room at dusk, and a corner of an outlet.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| One line only (Old Town) | Smallest; closest to the streets | A large catalogue from three companies would look the same everywhere |
| **All four lines: Plain, Old Town, Workshop, Curve** | Three companies can each have a character; homes differ; still one town because all share the kit and materials (I6) | Four times the design checks; Plain and Curve are newer shapes in an old town |
| Lines with their own materials | Most variety | Breaks W1 and I6; the town would stop looking like one place |

## Decision
All four lines, as rendered: **Plain** (budget flat-pack), **Old Town** (classic), **Workshop** (dark wood on dull iron)
and **Curve** (rounded, matching the Soft box cars). Rules I1–I9 move into ART_DIRECTION §2.6 and are in force for every
piece of furniture and every interior. New references: `Art/Reference/furniture_lines.png` (every piece of every line)
and `Art/Reference/interior_room_daylight.png` (the Old Town room in the kit daylight). The other rooms, the dusk room
and the outlet stay in `Art/StyleTests/furniture_kit_17/`.

Fixed after the owner's choice and before the references were taken: the Curve wardrobe's doors now follow its
rounded body (a groove, not flat panels), room-set floors in the outlet no longer render black, and the room shots are
taken from inside a deeper room so nothing outside it shows. None of these change a rule.

## Consequences
- The outlet's look (INTERIOR_STYLE §4) is shown in the test but not approved by this record; it is approved with the
  outlet's building (FURN-020).
- The browser slice does not yet reach this look indoors (INTERIOR_STYLE §6): no sun through windows onto floors, no lamp
  shadows, no rounded edges or mottling on furniture. Those must be closed before furniture goes into the slice, and the
  slice measured against the references at the four spots §6 names.
- Unreal (PH1) needs Lumen bounce light, a 2.5° sun, a flat-colour sky light, shadowed point lamps, an AgX tone curve,
  clamped exposure indoors, the one painted master material and bevelled meshes (INTERIOR_STYLE §6, R1–R9).
- `furniture_kit_17.py` builds on `car_kit_09.py` and through it the earlier scripts; keep `Tools/blender/` together.
