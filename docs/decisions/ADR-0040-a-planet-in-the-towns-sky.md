---
adr: ADR-0040
title: The town's moon goes round a living, ringed planet that hangs in one place in the sky
status: accepted          # asked for by the owner in chat on 2026-11-24 ("can we add this planet to our game and see how it looks?"); shown in the slice by day, at dusk and at night, and approved the same day ("Yes, keep it")
date: 2026-11-24
relates_to: [WLD-042, WLD-043, WLD-065, ADR-0004, ADR-0014, ADR-0023, ADR-0024]
---

## Context
The owner asked for a picture of someone standing on a moon and looking up at the planet it goes round
(`Art/StyleTests/moon_view_28/`), and shaped it over five rounds: a habitable planet, not a gas one; with rings; the moon has
air, so by day the planet should barely show; at night the part of it the sun reaches should be seen. The owner then asked to
add this planet to the game to see how it looks. That makes the world the town stands on a moon of this planet.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| The planet crosses the sky like the sun and the moon | Rises and sets; easy to see from anywhere at some hour | Not what a moon sees: a moon that keeps one face to its planet sees it stand still. Another moving body to keep clear of the sun |
| **It hangs in one place, in the north-east, clear of the sun's path in every season** | True to a moon that keeps one face to its planet; a landmark you can steer by; fullest in the evening, when the sun is opposite it; the sun never goes behind it, so the approved daylight is untouched | It is never seen from a street that looks the other way; it is never quite full |
| It hangs in the south, where the sun crosses | Full at midnight | The sun would pass behind it at midday in some seasons and the town would go dark: a change to the approved daylight that nobody asked for |

## Decision
- The planet is data in `World/rules/sky.json` "planet" and is drawn by the slice's sky, behind the clouds and the moon.
  `show: false` takes it out and leaves the sky exactly as it was.
- Place: azimuth 50° (north-east), elevation 32°, radius 0.19 rad (about 11°, some twelve times the moon's): chosen so the
  whole of it clears the roofs and the sun's path, and approved by the owner as shown.
- It does not rise, set or change with the season. Its phase is worked out from where the sun is and nothing else: in
  the slice's summer about 60% lit at dusk, 40% at midnight, a thin crescent by dawn; fuller in winter. It turns once in
  `spin_hours` (9), from the world clock. Nothing is stored or ticked.
- By day it is a pale shape in the blue (`by_day`), its dark part plain sky; after dusk it comes out (`night`, 1.0), with the lamps
  of its towns on its dark side. A fainter night (`night` 0.3) was tried at the owner's request and turned down: "faded is not
  looking good". Cloud passes in front of it; rain and fog hide it as they hide the stars.
- Its look is the one the owner shaped in `moon_view_28`: seas, green and dry land, tundra, ice at the poles, drifting cloud,
  a blue rim of air, pale rings with one gap, the rings' shadow on the planet and the planet's on the rings. Colours keep to
  the world's muted palette (ADR-0004).
- `planet_problems()` checks the numbers and that the sun never comes within the planet's disc in any season:
  `test_WLD_065_the_planet_hangs_in_one_place_clear_of_the_sun`, `test_WLD_065_a_broken_planet_is_caught`.

## Consequences
- The town now stands on a moon. Nothing else in the game says so yet: the calendar, the tides, the stories and the name of
  the planet are untouched. `[OPEN]` for the owner.
- The body the game calls the moon (WLD-042, WLD-043) is left as it is: a second, small moon of the same planet.
- The planet gives no light to the street. A planet this size would light the night more than the moon does; that would change
  the approved night look (ADR-0023) and is left for the owner to ask for.
- The planet in the sky has no air of the town's between it and the eye beyond the fade by day: it is not hazed towards the
  horizon as the far town is. At 32° up this does not show.
- Pictures are in `Art/StyleTests/planet_sky_29/`. The Unreal build can use the same numbers.
