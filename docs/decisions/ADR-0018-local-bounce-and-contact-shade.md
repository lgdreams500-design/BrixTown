---
adr: ADR-0018
title: Bounce that follows nearby sunlit ground, and contact shade under street furniture
status: accepted          # shown to the owner as a trial, who made it the default in chat on 2026-11-22
date: 2026-11-22
relates_to: [ART-001, ART-005, ADR-0004, ADR-0007, ADR-0013]
---

## Context
Set beside `Art/Reference/world_alley_morning_raw.png`, the slice's alleys are flat: shaded walls are brighter and greyer
than the reference (0.37 against 0.27 in brightness, 0.25 against 0.37 in saturation) and nothing sits in contact shade.
The comparison is in `Art/StyleTests/light_compare_12/`.

The reference is path-traced, so light bounces only off surfaces the sun reaches. The slice stood in for that with one
constant "bounce" colour added to everything in shade, a little more where less sky is seen. Switching each fill light
off in the start alley showed that term alone causes the gap: with the sky fill off nothing changes there (the sky is
already blocked correctly), with the bounce off the shaded wall falls to 0.22 and 0.42, almost the reference.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Lower the constant bounce | One number | Open streets, which match the street reference, go dark too |
| **Bounce from a map of sunlit ground; contact shade from a list of street furniture** | Dark, warm alleys and unchanged open streets; one small extra pass a frame | An estimate: only the ground bounces light, not sunlit walls; only listed furniture shades the ground |
| Real global illumination (probes, traced or baked) | Closest to the reference | Far more work; belongs in the Unreal build |

## Decision
Shown first as a trial and now the default in the kit daylight (`gi: 1` in `LIGHTS.kit`). Key G (since 2026-11-23 key O, the simple picture, with the ambient occlusion), or `?gi=0`, switches it
off to compare with the flat fill it replaced:
- **Sun-on-ground map.** Each frame, after the sun's shadow picture, a 128 × 128 map of the 96 m round the camera records
  which ground the sun reaches. The lit pass reads it blurred. A wall takes the ground in front of it (1.5 m and 4.5 m
  out), a floor or ceiling the ground round it.
- **Bounce** becomes 0.14 to 1.9 times its old value by how much sunlit ground is near (floors start at 0.42, for the
  light off the walls round them), and takes the warm colour of sunlit stone instead of the grey of ADR-0013.
- **Contact shade.** Benches, bins, planters, post boxes, bollards, wells, bike racks, advert boards, lamp posts and
  stacks of crates darken the ground under them and the foot of a wall behind them: the 16 nearest the camera.
- Sun, sky fill, haze and exposure are untouched. With shadows off (key Q, or a slow machine) it is off and the flat
  fill returns.

Measured in the start alley at 07:20 (brightness, saturation):

| Spot | Reference | Slice before | Trial |
|---|---|---|---|
| Shaded wall | 0.27, 0.37 | 0.36, 0.24 | 0.27, 0.42 |
| Shaded floor | 0.31, 0.32 | 0.32, 0.30 | 0.26, 0.47 |
| Sunlit floor | 0.67, 0.28 | 0.60, 0.20 | 0.65, 0.23 |

A bench like the reference's was added to the start alley (`World/city_01/blocks/b_3_5.json`) so the two can be compared.

## Consequences
- Not done: light bounced off sunlit walls, people and cars as casters of contact shade, lanterns casting
  shadows, sharper sun-shadow edges, thicker haze in alleys.
- It fades out with the daylight, so night is unchanged. Looked at after becoming the default: midday, dusk, night and
  rain.
- ADR-0013's grey bounce is superseded where this is on: the bounce is warm again, but only near sunlit ground.
- The shaded floor of the start alley is a little darker and warmer than the reference's (0.26 against 0.31).
