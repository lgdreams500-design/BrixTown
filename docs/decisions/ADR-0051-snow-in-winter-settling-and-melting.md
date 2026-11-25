---
adr: ADR-0051
title: Snow in winter, falling, settling and melting, worked out from the clock
status: accepted          # the owner asked "can we add snow as well?" and picked the answers below in chat, 2026-11-25
date: 2026-11-25
relates_to: [WLD-014, WLD-037, WLD-069, WLD-070, CITY-077, CITY-078, CITY-079, CITY-080, CITY-107, ADR-0014, ADR-0016, ADR-0020]
amends: ADR-0016          # adds a fifth kind of weather to rain, fog and overcast
---

## Context
The weather (`citysim.weather()`) had four kinds: clear, cloud, rain and fog, drawn by the light, rain and wet paving of
ADR-0016 and the clouds of ADR-0014, with residents reacting to rain (ADR-0020). The owner asked for snow and answered
three questions in chat on 2026-11-25: when it snows ("Winter only, own chance"), whether it settles ("Settles, then
melts") and how residents react ("Coats and hoods, walk carefully").

## Options considered
| Question | Picked | Other options |
|---|---|---|
| When it snows | Winter only, its own chance per season | Winter wet slots fall as snow instead of rain; only as a look to try |
| Settling | It settles on the town and melts after | It only falls |
| Residents | Coats and hoods, no umbrellas, a careful walk | As in rain (umbrellas, hurrying); no change |

## Decision
- **The weather.** Snow is a fifth kind (`world.WEATHER_KINDS`). A slot has snow by its own draw, in spells like rain
  (neighbouring slots share part of the draw), with `snow_chance` per season in `World/rules/calendar.json`: Winter only,
  0.3 `[ASSUMPTION]`. It is drawn before rain, so a cold wet slot is snow. The same for every player (WLD-014, WLD-069).
- **The look** (WLD-070). A snowy slot has its own light in `World/rules/sky.json` `weather.snow` (less sun, thicker air,
  a paler veil, grey clouds, cover 0.92) and its own night. Flakes fall round the camera as the rain does (not under a
  roof, not indoors), slowly and swaying (`snow`: alpha, speed, sway, size). While it snows it settles on what faces up,
  the streets, pavements, roofs and ledges, building to a white cover over `settle_minutes` and melting over
  `melt_minutes` after it stops: `world.snow_now()` works out how hard it snows and how much lies from the clock and the
  day's weather slots alone (twelve hours back), nothing stored. Snow does not wet the paving as rain does.
- **Residents** (CITY-107). On a day with snow nearly everyone wears a coat and most coats have a hood
  (`weather_react.json` `coat.snow_day`, `coat.snow_hood`); out in it they put the hood up, else go bare-headed, never an
  umbrella; they take cover from it as from rain (CITY-078) and walk at the careful `pace.snow`, never hurrying or
  running. All of it in citysim's plan, as with rain.

## Consequences
- Winter days now have snow in their plan: residents shelter and walk slower in snow spells, which changes winter days'
  routines; the summer day the slice and Unreal are built for (day 16) does not change.
- The browser slice is frozen (ADR-0045) and draws snow as nothing new: it gets the light and sky numbers of a snowy slot
  but no flakes or lying snow. The Unreal build draws all of it.
- Not done: footprints and tyre tracks in the snow, snow on trees, people's clothes going white, slipping, snowballs, the
  sound of snow, snow affecting traffic. The look of snow needs rules and a reference image in art direction (ART-006).
