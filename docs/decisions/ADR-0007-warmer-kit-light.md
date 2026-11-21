---
adr: ADR-0007
title: Daylight — the softer, warmer "kit" light replaces the reference light as the default
status: accepted          # owner approved in chat on 2026-11-22
date: 2026-11-22
relates_to: [ART-001, ART-002, ART-005, ART-006, ADR-0004]
amends: ADR-0004          # the world look stays; only the daylight recipe changes
---

## Context
ADR-0004 locked the old-town look of `Art/Reference/world_alley_morning_raw.png`, including its light: a strong low
sun (14 W/m²) through dense haze. The playable slice was tuned to that light and matches it by measured colour in the
reference alley.

The building kit (`Tools/blender/building_kit_07.py`) then rendered whole streets of real lots from `World/`. Those
renders used a softer, warmer sun and thinner haze, and the owner liked them. When the kit's buildings were carried into
the slice, the owner said the slice was "still not similar to the samples". The largest remaining difference was the
light: in the slice, sunlit plaster and stone go nearly white on an open street at mid-morning, while in the samples they
stay warm grey-beige under a deeper sky. A trial preset ("kit") was added to the slice so both could be compared in the
same spot.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Keep the reference light (ADR-0004 as is) | Matches the approved image and its measured colours; strong light shafts in alleys | Open streets read washed-out and pale; does not match the building samples the owner preferred |
| Darken the palette instead | Light recipe untouched | Changes the locked palette (§2.3) and every asset made with it; does not warm the light |
| **"Kit" light: weaker, more golden sun, deeper sky, more bounce, less haze, lower exposure** | Sunlit walls stay warm grey; facades, shutters and trims read clearly; matches the building samples | Hazy light shafts in narrow alleys are weaker than in the reference image; colours no longer match the measurements in WORLD_MAP §5.5 |

## Decision
Daylight uses the "kit" light. In the slice it is the default preset, expressed as multipliers on the reference recipe
(`LIGHTS.kit` in `Prototype/play/play.js`):

| Part | Multiplier | Note |
|---|---|---|
| Sun strength | ×0.62 | |
| Sun colour | green ×0.90, blue ×0.74 at full warmth | a more golden sun |
| Sky | ×0.9, red ×0.92, green ×0.96 | a deeper blue sky |
| Bounce from the ground | ×1.5 | shade stays warm and readable |
| Haze density | ×0.55 | streets clearer; distance still paler (W5) |
| Exposure | ×0.7 | |
| Lanterns | ×1 | unchanged |

The Blender equivalent, for new look tests, is the street light of `building_kit_07.py`: sun 8.5 in (1.0, 0.86, 0.68),
sky (0.42, 0.55, 0.82) at 0.35, haze density 0.006, exposure −0.15, AgX "Medium High Contrast".

Everything else in ADR-0004 stands: the old-town forms and materials, the palette, lanterns and lit windows, no
painterly or outline filter, and the characters. `world_alley_morning_raw.png` remains the reference for *forms and
materials*; `Art/Reference/world_street_kit_light.png` (the kit street render, reproduced by `building_kit_07.py`) is
added as the reference for *daylight*.

## Consequences
- The previous light stays available as the "approved" preset in the slice (press L) for comparison.
- The colour measurements in WORLD_MAP §5.5 were taken under the old light and are now history, not a target.
- W5 still holds: low directional sun, cool sky fill, distance paler than near. The light shafts it mentions are fainter;
  if alleys lose too much atmosphere, raise haze locally in alleys rather than globally.
- The Unreal sky and sun for each hour (PH1) start from the kit recipe, not the reference recipe.
- Dusk and night are unchanged by this decision.
