---
id: ADR-0020
title: People react to rain, calculated with their day
status: proposed          # owner picked F-NPC-REACT level 1 for the first slice on 2026-11-22; the look of coats, hoods and umbrellas is not yet approved
date: 2026-11-22
---

## Context
The owner asked for residents to react to the weather: clothing, umbrellas in rain, running for cover when rain starts.
The weather is already calculated per six-hour slot (`citysim.weather()`), and routines already choose indoor leisure when
a choice is made in rain (CITY-026). Nobody reacted to rain that started while they were out.

## Decision
- All of it is calculated from the clock, the seed and `World/rules/weather_react.json`, inside `City.plan(day)`; nothing is
  ticked or stored (LIVING_CITY §1, CITY-022).
- **What they wear** (CITY-077): drawn once per resident per day from the season and whether the day has rain.
- **Taking cover** (CITY-078): a stay out in the open that a rain spell overlaps is cut a minute or two after the rain
  starts. A `shelter` stay under their nearest café's or grocer's awning (or at home) is put in until the spell ends, and the
  stay picks up again afterwards if at least `resume_minutes` of it is left. The walk to cover leaves at once instead of early.
- **Pace** (CITY-079): walks that set off in rain go faster by a factor from data (umbrella, hurry, run).
- **Cover** (CITY-080): `City.at()` reports umbrella, hood or bare for anyone out in the rain.
- The slice gets each resident's wear in `D.wear` (`build_play.py`). `wet.js` draws a coat over the role's clothes, the hood
  or umbrella (`people.js`), a hand over the head, and a hurried or running walk, chosen from the speed of the packed leg.
  With the weather key (K) set to rain on a dry day, it shows what people would have worn on a wet day. Their routes do
  not change, because the plan is packed.

## Consequences
- Rain spells are whole six-hour slots, so cover is taken at 00:00, 06:00, 12:00 or 18:00. A summer evening at 18:00 (day 18) shows it best.
- Crowds under one awning can be dense; awnings are spread along the lot's front only.
- Not done: umbrellas held by the player, cover under trees or arches, wet clothes, sound of umbrellas, people sitting
  out on wet benches after rain, needs and trade (WLD-004).
- The coat, hood and umbrella shapes need the owner's look approval against `Art/Reference/` (ADR-0004).
