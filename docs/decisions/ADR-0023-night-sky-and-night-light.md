---
adr: ADR-0023
title: A night sky of stars, Milky Way and moon, and night light that keeps the shape of things
status: accepted          # built at the owner's request; the owner approved the look in chat on 2026-11-22 ("Approve it")
date: 2026-11-22
relates_to: [WLD-035, WLD-036, WLD-037, WLD-042, ADR-0004, ADR-0014, ADR-0016]
---

## Context
The owner found that after dusk "the textures have become completely flat", and asked for a night sky that is "beautiful
and magnificent based on the weather": on a clear night, the Milky Way "in its all glory".

The night look had not been started (ART_DIRECTION §"not yet decided"). In the slice the flatness had three causes:
- **Lantern glow in the air** was added to every pixel the view ray passed lanterns on the way to, so the whole street
  wore one orange veil and stone, brick and paving lost their contrast.
- **Lanterns wrapped round**: 15% of their light reached surfaces facing away, so near a lamp everything was lit evenly.
- **The moon** was a faint light from a fixed direction, too weak to show relief or cast a shadow.

The sky was one blue gradient with a few dim dots.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| A star picture (cube map) behind the town | Easy to paint any sky | A large texture; does not turn with the clock or follow the weather by itself; a painted asset outside `Art/Reference/` |
| **Stars, Milky Way and moon drawn in the sky shader from the clock and `sky.json`** | No assets; turns with the hour and the season; weather and moon phase change it; every number is data | An estimate of the real sky, not a star catalogue |
| Leave night as it was | Nothing to do | What the owner reported |

## Decision
Approved by the owner as the default night look (2026-11-22). Drawn in the sky shader of `Prototype/play/play.js`, every number in `World/rules/sky.json` "night", nothing stored or ticked:
- **Stars** in four layers of growing number and faintness, each with its own colour and shimmer (more near the horizon),
  denser along the Milky Way. They sit in the stars' own frame, which turns round the celestial pole (latitude 40°) with
  sidereal time: once a day and a little further each day of the 48-day year.
- **Milky Way**: a clumpy band along the galactic plane with a bulge and a warm centre, bluish arms, the great rift and
  smaller dust lanes. On summer nights its centre stands low in the south.
- **Moon**: follows the sun's path, later by its phase (a 24-day cycle). It is drawn as a lit ball with darker seas and a
  halo. Its light on the street goes with its phase and height and casts shadows; a bright moon fades the stars.
- **Weather**: cloud hides the sky behind it; the share of stars seen, the lantern glow and how much of the day's cloud stays
  at night are given per kind of weather. Clear nights keep 40% of the day's cloud. Rain and fog hide the stars, and the
  town's warm light shows under the clouds.
- **Night light on the street**: lanterns light only what faces them (wrap 0.05 at night, 0.15 by day as before). Their
  glow in the air is 0.3 of what it was on clear nights and 0.5 in rain. The night sky fill and the distance colour come
  from the night colours, so the far end of a street fades to a dark blue, not to orange.
- Daylight is untouched: every change is weighted by `night` (0 by day), checked at noon on Baker Street.

Checked by `night_sky_problems()` (`test_WLD_042_night_sky_rules_are_sound`, `test_WLD_042_a_broken_night_sky_is_caught`).
Before and after pictures are in `Art/StyleTests/night_sky_15/`.

## Consequences
- The owner approved this night look on 2026-11-22; it is now the slice's default. Changing it needs a new ADR.
- The Unreal build will need the same sky. The data carries over; the shader is a stand-in for a sky material there.
- The moon follows the game's sun path, not a real orbit, and the latitude is chosen for the look, not for a real place.
