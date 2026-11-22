---
adr: ADR-0016
title: Rain, fog and overcast drawn from the calculated weather
status: accepted          # owner picked F-WEATHER level 1 for the first slice, asked for rain and fog, and approved this picture in chat on 2026-11-22
date: 2026-11-22
relates_to: [ART-001, ART-005, WLD-003, WLD-014, WLD-035, WLD-036, WLD-037, ADR-0004, ADR-0013, ADR-0014]
amends: ADR-0004          # adds the bad-weather looks that art direction §5 lists as not started
---

## Context
The weather is already calculated (`citysim.weather()`: clear, cloud, rain or fog for each six-hour slot, the same for
everyone, WLD-014) and since ADR-0014 the clouds follow it. But rain and fog were only words: a rainy slot gave a
white overcast sky over a sunlit, dry street. Art direction §5 lists bad-weather looks as not started and says they
must follow W5 to W7: low directional light and haze, lanterns carrying the shade, the place readable at every hour.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Leave weather as sky only | Nothing to build | Rain and fog do not exist to the eye |
| Rain as falling particles in the world | Stops under roofs and arches; hits the ground | Many moving points to place and draw each frame; better done in Unreal |
| **Change the light for each kind of weather; rain as streaks over the picture; wet paving in the shader** | Cheap; every number in data; nothing stored or ticked | Rain shows under arches and awnings; none seen through a window from indoors |

## Decision
Each kind of weather has five numbers in `World/rules/sky.json` (`weather`), blended across the edges of the weather
slots over `blend_minutes`:

| Kind | Sun let through | Air thickness | Pale veil | Clouds darkened | Rain |
|---|---|---|---|---|---|
| Clear | 1.0 | ×1 | 0 | 0 | none |
| Cloud | 0.55 | ×1.2 | 0.04 | 0.35 | none |
| Rain | 0.08 | ×2.6 | 0.06 | 0.85 | steady |
| Fog | 0.3 | ×7 | 0.22 | 0.2 | none |

- **Clear weather is untouched**: its row is all ones and zeros, so the daylight of ADR-0013 and the sky of ADR-0014
  stand as approved.
- **Overcast.** Sunlight the clouds stop comes down in part as soft grey light from the whole sky (`overcast_fill`),
  so a wet day is flat and cool, not dark. The clouds go grey because they are lit by the weaker sun.
- **Fog** is the haze of W5 made much thicker, with a pale veil: distance is lost within a street's length.
- **Rain** falls in the world, not on the screen: four rings of drops round the camera, 1.6 to 11 m away, so they fall
  straight down whichever way you look and draw together overhead when you look up. It is not drawn while the camera
  is inside a building. (The first version was streaks laid over the picture; the owner found it "so bad" and it was
  replaced the same day.)
- **The sky closes over**: in rain the cover is 0.97 and the blue overhead goes grey.
- **Wet paving.** Pavements, the road and footpaths darken, shine a little towards the light, and fill with puddles
  that hold the grey sky by day and the lamplight by night; rings spread where drops land. Walls and everything else
  out in the rain go a little darker. It all dries over `dry_minutes` after a wet slot ends. Worked out from the clock, like everything else here.
- **Lanterns** glow in the wet air no more than in a thick haze, so a rainy night stays dark and lamplit (W6) instead of
  turning orange.
- The HUD's date line names the weather when it is not clear.

## Consequences
- Not done: rain stopping under arches and awnings (drops and puddles show there too), rain seen from indoors, drops
  hidden by nearer walls, reflections of buildings in the puddles,
  the sound of rain, residents sheltering (F-NPC-REACT, not picked), any effect on needs or trade (WLD-004).
- Art direction needs rules and reference images for rain and fog (ART-006); neither is made yet.
- `World/rules/sky.json` now holds the whole sky and weather look; `world.sky_problems` checks it.
