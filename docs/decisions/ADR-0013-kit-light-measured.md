---
adr: ADR-0013
title: Kit daylight in the slice retuned towards the measured colours of the reference street
status: accepted          # owner approved the middle setting in chat on 2026-11-22, after finding the measured match "too pale"
date: 2026-11-22
relates_to: [ART-001, ART-002, ART-005, ADR-0004, ADR-0007]
amends: ADR-0007          # only the slice multipliers; the Blender recipe and the reference image stand
---

## Context
ADR-0007 made the "kit" light the daylight and named `Art/Reference/world_street_kit_light.png` as its reference. In the
slice it was written as multipliers on the earlier recipe, tuned by eye: a more golden sun, a deeper sky and more bounce.

Measured in the same kinds of spot at 09:00 (sRGB, saturation = (max − min) / max), the slice did not match that image:

| Spot | Reference street | Slice before | Slice after |
|---|---|---|---|
| Sunlit paving | (171, 160, 156) sat 0.09 | (153, 134, 120) sat 0.21 | (167, 156, 151) sat 0.10 |
| Shaded paving | (112, 106, 106) sat 0.06 | (99, 79, 70) sat 0.30 | (108, 102, 101) sat 0.06 |
| Sunlit stone and plaster | sat 0.04 to 0.12 | sat 0.22 to 0.40 | sat 0.09 to 0.14 |
| Sky | (201, 199, 197) to (217, 214, 211) | (130, 136, 150) away from the sun | (184, 182, 183) to (206, 202, 199) |
| Building at the far end | (179, 172, 167) | (161, 145, 135) | (183, 174, 169) |

The reference script (`building_kit_07.py`) uses the plain sun colour (1.0, 0.86, 0.68), not a more golden one. The
orange came from the slice: the golden sun, and a bounce that was coloured like sunlit orange stone and then raised ×1.5.
The reference sky is pale because the whole air is hazy; the slice only brightened the sky near the sun.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Keep the ADR-0007 multipliers | No change | Twice to four times as saturated as the image ADR-0007 names as its reference |
| Grey the palette | Light untouched | Changes the locked palette (§2.3); the palette is not the cause |
| Retune the light to the measurements | Matches the reference by number; palette untouched | Tried first. The owner found it too pale |
| **A setting between the two** | Keeps the grey shade and the paler sky; warmer and clearer than the measured match | Matches neither the reference image nor ADR-0007 exactly |

## Decision
`LIGHTS.kit` in `Prototype/play/play.js` becomes the middle setting. The measured match is kept as the preset `measured`
(`?light=measured`); its values are the "Measured" column.

| Part | ADR-0007 (`golden`) | Measured | Now (`kit`) | Note |
|---|---|---|---|---|
| Sun strength | ×0.62 | ×0.62 | ×0.7 | |
| Sun colour | more golden (`warm: 1`) | as the script (`warm: 0`) | about a third of the way to golden (`warm: 0.35`) | |
| Sky fill by day | ×0.9 | ×1.5 | ×1.25 | ×0.9 kept when the sun is low and at night (`skyLow`) |
| Bounce | ×1.5, orange | ×1.2, the grey of pale stone | ×1.2, mostly grey (`neutral: 0.7`) | |
| Haze density | ×0.55 | ×0.55 | ×0.55 | |
| Haze veil | none | 0.3 | 0.12 | share of the sun added to the sky and the haze colour by day (W5) |
| Exposure | ×0.7 | ×0.78 | ×0.74 | |

The "Slice after" column in the Context table is the measured preset, not the middle setting.

No material changes. Two were tried and taken back at the owner's word (chat, 2026-11-22):
- Narrower blotches in the `painted()` recipe (contrast 2.2 → 1.2). They made the stone blocks and the road one flat
  tone (W1: mottled, never a flat fill). The contrast is 2.2 again.
- A carriageway of pale slabs of 0.66 × 0.33 m, as in the reference street. The owner chose the small setts; the road is
  as it was.

The ADR-0007 values stay as the preset `golden` (`?light=golden`) so the two can be compared in the same spot.

## Consequences
- Dusk and night are as before; morning, midday, dusk and night were looked at after the change.
- Round cobbles on footpaths and in alleys are unchanged; the reference alley has slabs there. Not decided.
- The slice is now deliberately a little warmer and clearer than `world_street_kit_light.png`. If that stays, the
  reference image should be re-rendered to match, or it will keep disagreeing with the game (ART-006).
- If the owner prefers the golden picture, set `kit` back to the `golden` values and mark this ADR rejected.
