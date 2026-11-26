---
adr: ADR-0059
title: People can look old; Noor's look; the Stone Oven's inside
status: accepted        # the owner's words in chat, 2026-11-26, quoted below; nothing of it is in the game yet
date: 2026-11-26
relates_to: [ADR-0004, ADR-0010, ADR-0026, ADR-0056, ADR-0058, CHR-002, CHR-007, QST-014, ART-001]
---

## Context
Noor, the master baker of the Baker career (ADR-0056), is 58. The owner asked for her and for the Stone Oven's inside in
Blender ("use blender to design a baker"; "Noor herself and the bakery inside with oven etc."). The first Noor read as 35:
option B people (ART_DIRECTION 3) had no value that says age, and the cast's Elder was a short person with grey hair. The
owner then asked: "can you create few old people looks and add to the character library?"

Two faults were found by the owner, not by the agent: "the shadow below the hair is making it look like the shadow is hair
and the hair is a cap", and, after a first patch judged from a wide shot, "build a capability to analyze these issues, see
this hair still look like cap".

## Decision
- **Values that say age**, on the same body and head as everyone (C7), the face control set untouched (CHR-007):
  `age` 0 to 1 (lines at the brow, the eyes and from nose to mouth as soft creases of the person's own skin, paler lips),
  `stoop` (degrees from the hips), the hair shapes `thin` and `wisp`, `glasses`, `cane`, `shawl`, `beard`, `tunic_len`.
  Made in `Tools/blender/elders_48.py`; six elders made from them are its `ELDERS`. The owner: "this is good enough".
- **Hair sits on the skull.** The cast's hair is a shell 2 cm off the head with a cut edge; with pale hair its shaded edge
  reads as a cap over dark hair. In `elders_48.py` the shell is taken away and hair is a body on the skull with a curved
  hairline, locks swept back in two close tones, and hair before the ears. The owner: "Yes, hair is right".
- **A head check** (`Tools/blender/look_check.py`): every head rendered close up and again with hair, skin and cap in flat
  colours, measured for a dark band under the hair's edge, a ruled hairline, hair standing off the skull and hair bulging
  at the ears; faults in plain words, close-ups kept to be looked at. It is the Blender look tests' part of the look gate
  (ADR-0058, whose `Tools/unreal/look_check.py` measures game shots). A character render is not shown before it has run
  and its close-ups have been opened.
- **Noor**: a sturdy woman of 58, grey hair up in a bun, lines at the brow and eyes, an olive dress to the shin with short
  sleeves, a small cream bib apron, brown shoes (`N` in `Tools/blender/baker_look_47.py`; `gender` and `look` in
  `docs/narrative/src/cast.json`). The owner: "pick option A for noor", then "what is option C wearing? can you make Noor
  wear that", then "Yes, keep this Noor".
- **The Stone Oven inside** (`baker_look_47.py`): a stone oven with an arched mouth and a wood store under it, the kneading
  bench, racks of loaves with the crock of the Mother alone on top, flour sacks under a price board, the counter, the
  plank door to the alley with its lantern (the four o'clock door) and the door to the proofing room; light set against
  `interior_room_daylight.png`. The owner: "Yes, approved".
- References added (nothing overwritten): `Art/Reference/character_elders.png`, `character_noor.png`,
  `bakery_inside_day.png`, `bakery_inside_four.png`. ART_DIRECTION 1.10.0: rule C9 and the reference rows.

## Options considered
| Option | For | Against |
|---|---|---|
| Grey hair and a short body only, as the cast's Elder | Nothing new | Nobody reads as old; Noor's story is about her age and her hands |
| **Age as a few values on the same body and head** | One model for everyone (C7); the designer can offer them later | New parts to carry into the slice's and Unreal's people |
| A separate old-person model | Could be richer | Breaks C7 and the one face control set |

## Consequences
- None of this is in the game. Unreal's people (and the frozen slice's) have no age values, still have the shell hair, and
  cannot give one named resident a look of her own; the Stone Oven inside there is fitted as a grocer's. Each is work to
  come, behind the look gate.
- The earlier reference pictures (`character_lineup.png` and the face sheets) still show the shell hair. They stay as they
  are; where they and C9 differ on hair, C9 and `character_elders.png` win.
- Known weak points, kept in `Tools/README.md`: the old man's cap is the courier's and reads as a helmet; the shawl reads
  as a collar; the bakery has no shop front or street door, its oven's fire reads a little like candle flames, and its
  floor is boards, not flags.
