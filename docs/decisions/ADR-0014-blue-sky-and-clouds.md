---
adr: ADR-0014
title: A blue sky with drifting clouds, worked out from the world clock
status: accepted          # owner asked for a sky and cloud system because the sky was pale, then approved this picture in chat on 2026-11-22
date: 2026-11-22
relates_to: [ART-001, ART-005, WLD-002, WLD-014, WLD-035, WLD-036, ADR-0004, ADR-0007, ADR-0013]
amends: ADR-0004          # adds to the look: the reference renders have a plain hazy sky and no clouds
---

## Context
The reference renders show a plain, pale, hazy sky. The slice copied that, and after ADR-0013 the sky was a soft grey
from horizon to zenith. The owner found it pale and asked for a sky and cloud system.

Art direction §5 lists night and bad-weather looks as not started, and nothing in it describes a sky. Weather is
already calculated by `Tools/citysim/citysim.py` (`weather()`: clear, cloud, rain or fog for each six-hour slot) and
packed into the slice's data. When the sky was first built the owner had not picked a level for F-WEATHER.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Keep the plain hazy sky | Matches the reference renders | The owner finds it pale |
| A painted sky picture | Cheap; exact control of the look | Cannot move, change with the hour or follow the weather |
| **Blue overhead, hazy horizon, clouds made from noise and placed by the clock** | Changes with the hour and can follow the weather; nothing stored or ticked; numbers in data | Not in the reference renders; soft heaps rather than modelled shapes |
| Clouds as 3D shapes | Closest to the chunky forms of W3 | Much more work and cost; better done in Unreal |

## Decision
- **Sky.** By day the sky overhead is blue (`zenith` in `World/rules/sky.json`) and fades to the pale haze at the
  horizon, so distance still lightens (W5). At dusk and night the earlier colours stand.
- **Clouds.** Soft white heaps on one flat layer overhead, bright on the side towards the sun and shaded underneath,
  fading into the haze towards the horizon. They take the sun's colour, so they turn pink at dusk and go dark at night,
  where they hide the stars behind them.
- **Calculated, not stored.** Where the clouds are and what shape they have is a function of the world clock (day and
  minute), the wind and the speeds in the data. Every machine with the same clock draws the same sky.
- **Weather.** The share of sky covered is given for each kind of weather (`clouds.cover`) and follows the day's
  six-hour weather slots, changing over `blend_minutes` at each slot's edge (`follow_weather: true`). The owner switched
  this on in chat on 2026-11-22 (F-SKY level 2 and F-WEATHER level 1 for the first slice).
- **No effect on light or play.** Clouds do not dim the sun, cast shadows or change anything residents do.
- All numbers are in `World/rules/sky.json`, checked by `world.sky_problems` and packed into the slice by `build_play.py`.

## Consequences
- The slice no longer matches the sky of the reference renders. Art direction needs a sky rule and a reference image
  with clouds (ART-006); neither is made yet.
- `wasted.dials.clouds` (0 to 1) and `wasted.dials.blue` (0 to 1) in the browser console override cover and blueness
  for tuning.
- The blue is given greener than it looks, because the AgX tone curve turns a pure blue to purple.
- Not done: clouds that dim the street, cloud shadows, a grey overcast (a covered sky is still white), rain and fog
  themselves, a moon.
