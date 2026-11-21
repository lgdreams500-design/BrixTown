---
adr: ADR-0010
title: Option B characters get rosy cheeks
status: accepted        # owner's instruction in chat, 2026-11-22
date: 2026-11-22
relates_to: [ART-004, CHR-007, ADR-0004, ADR-0005]
---

## Context
ADR-0004 locked character option B "Soft" (`Art/Reference/character_options.png`, middle group). After
reviewing the slice's people against the references, the owner asked whether they could look like option
C "Simple" (biggest head, dot eyes, rosy cheeks, smile line), then settled on: "just add rosy cheeks to
existing model and fix the dress etc."

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Keep B as it is | No change to the locked look | The owner wants more warmth in the faces |
| Switch to C | Friendlier, toy-like | Dot eyes and a fixed smile line drop the iris, lids and open mouths the face control set, expressions and lip sync are built on (ADR-0005, CHR-007 to CHR-009); C has one reference image only |
| **B with C's rosy cheeks** | Warmer faces; keeps B's eyes, expressions and lip sync; one small addition | The approved B reference images show no cheeks until they are rendered again |

## Decision
Option B stays the character look. Every face gets C's rosy cheeks: a soft oval on each cheek, below and
outside the eye, coloured as a redder version of the person's own skin tone (in `Tools/blender/old_town_04.py`:
`(min(r × 1.1, 0.9), g × 0.66, b × 0.62)`, half-size 0.017 × 0.011 on a natural head). All other B values
(proportions, eyes, nose, mouth) are unchanged.

## Consequences
- ART_DIRECTION §3 gains rule C8 (cheeks). The browser slice draws the cheeks on every near face.
- The B reference images (`character_options.png`, `character_lineup.png`, `face_expressions.png`,
  `face_mouth_shapes.png`) predate this and show no cheeks. They stay as they are (never overwritten);
  when they are next rendered, B is run with `cheeks=True`, and the new images are added as new files.
- Cheeks stay put in every expression for now; blushing (stronger cheeks with some moods) is a possible
  later addition to the face control set.
