---
adr: ADR-0024
title: The sun and the moon cross the sky on paths that change with the season
status: accepted          # asked for by the owner in chat on 2026-11-22 ("so they don't rise and set from the same location")
date: 2026-11-22
relates_to: [WLD-001, WLD-002, WLD-042, WLD-043, ADR-0004, ADR-0023]
---

## Context
The sun followed one fixed path every day of the year, so it rose, set and stood at noon in the same places in every season.
Only the times changed (calendar sunrise and sunset). The moon took the same path, later by its phase. The owner asked for
paths that follow the season, so the light varies through the year.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Keep one path, tilt it a little per season by hand | Small change | Made-up numbers; the sun still would not rise where it sets at the calendar's times |
| **Real sun geometry at the town's latitude, with each season's declination worked out from the calendar's day length** | Rises and sets exactly at the calendar's times; north of east in summer, south of east in winter; the noon height follows; one number (latitude) | Summer noon is higher than the old path, winter much lower, so the daytime light changes with the season |
| Declination from the day of the year, as on Earth | Changes every day | Would not match the calendar's sunrise and sunset, which are set per season |

## Decision
- Sun: at latitude 40° (`sky.json` night.latitude, shared with the stars). Its declination for each season comes from
  cos(H0) = −tan(latitude)·tan(declination), where H0 is half the calendar's day length. Solar noon is the middle of the
  calendar's day. This gives Spring 8.8°, Summer 27.8°, Autumn 0°, Winter −24.5°.
- The light never comes from lower than `paths.sun_lift` (0.08, as the old path's lowest point), so dawn and dusk shadows
  stay inside the shadow picture. The sun crosses the horizon at the calendar's times.
- Moon: later than the sun by its phase. Its declination swings ±23.4° with its place on its orbit, plus a 5.1° tilt that
  turns every 223 days. So a winter full moon rides high and a summer full moon stays low. The stars' sidereal time comes
  from the same sun clock.
- Seeing them (added after the owner said neither showed): the sun is a sharp disc (`paths.sun_size`, 0.012 rad), and the haze
  glare eases right round it (`sun_glare_ease`, 0.55) so the disc stands out. Away from the sun the sky is as before. The moon
  is drawn by day too, a pale shape in its phase (`moon_by_day`), and its dark part is only a little darker than the sky. The
  moon's phase was moved (`offset_days` 13) so the slice's built day has a half moon in the evening sky that sets after
  midnight, leaving the Milky Way dark sky.
- Checked by `sun_heights()` and `night_sky_problems()`: `test_WLD_043_the_sun_climbs_higher_in_summer`,
  `test_WLD_043_broken_paths_are_caught`.

## Consequences
- The daytime look now changes with the season. Spring is closest to the old path, the one the approved light was tuned on.
  Winter mornings and afternoons are dimmer in the streets, and summer noons have short shadows.
  Pictures are in `Art/StyleTests/sun_paths_16/`.
- The Unreal build can use the same numbers for its sun and moon.
