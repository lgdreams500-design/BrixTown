---
adr: ADR-0062
title: No planet in the town's sky
status: accepted          # the owner in chat, 2026-11-26, reviewing the sky's reference: "get rid of the planet in the sky", then "yes, planet should be out of the game"
date: 2026-11-26
relates_to: [WLD-065, ADR-0014, ADR-0023, ADR-0061]
supersedes: ADR-0040
---

## Context
ADR-0040 put a ringed planet in the town's sky (WLD-065), drawn in the browser slice from `World/rules/sky.json`
`planet`; the Unreal build had not drawn it yet. While the sky's reference was being made (`Tools/blender/sky_52.py`,
for `Art/Reference/catalog.json`), the owner saw it in the sky at every hour and asked for it to go, from the reference
and from the game.

## Decision
No planet. `sky.json` `planet.show` is false, so the slice does not draw it (play.js draws it only when `show` is set)
and the Unreal build never will; WLD-065 is marked cut in WORLD_MAP (its ID is kept, never reused). The sky's
reference has none. The planet's numbers stay in `sky.json` and are still checked (`world.planet_problems()`,
`test_WLD_065_*`), so the decision can be undone by setting `show` again.

## Consequences
- The night sky is the stars, the Milky Way and the moon (ADR-0023).
- `Art/StyleTests/moon_view_28/` and `planet_sky_29/` stay as the record of what was tried.
