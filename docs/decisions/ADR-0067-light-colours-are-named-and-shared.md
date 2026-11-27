---
adr: ADR-0067
title: Light colours are named and shared by fittings; neon is a weak accent
status: accepted          # the owner in chat, 2026-11-26: "each colored light should have a name for different styles of lamps, lights, street lights etc. can share the color behavior"; of the proposed names, "Yes, these names"; "add those 3 suggestions"
date: 2026-11-26
relates_to: [FURN-066, FURN-067, ART-001, ADR-0004, ADR-0065, ADR-0066]
---

## Context
ADR-0065 gave each kind of light its own colour in `World/rules/lights.json`. Reviewing the first trial of the kinds in
the home room, the owner asked for more: "we also need some cooler and even neon blue and green lights", and then for the
colour to be a thing of its own: "each colored light should have a name for different styles of lamps, lights, street
lights etc. can share the color behavior". Four warm fittings each repeated nearly the same numbers, and the street
lamps and lanterns had no colour in the file to share at all.

## Decision
- `lights.json` has **colours**: each a name, a linear RGB colour with its brightest value 1, and a kelvin for reading
  (none for fire and neon). Ten to start: Lantern amber (the palette's Lantern, ART_DIRECTION 2.3), Warm white, Soft
  white, Neutral white, Cool white, Fire red, Neon blue, Neon green, Neon red, Neon amber.
- A **kind** (a fitting) names its colour and keeps its own strength, spread, glow, mount, size and price. Fittings of
  one colour behave alike: the floor lamp, table lamp, wall light and pendant are all Warm white.
- **Every fire is Fire red**, candles too (the owner: "Fire red"); the references' fire colour is read from the file.
- **Neon is an accent**, against the palette rule W4 (muted and warm, no saturated colour) by the owner's own words:
  weak (a tube of 8 to 10 W; the owner, of tubes of 30 to 40 W that turned the whole room one colour: "Weaker neon"),
  so it tints a corner of a room lit by other lamps. This is the one exception to W4; ADR-0004's look is otherwise
  unchanged.
- The colours and fittings are judged on the reference `IndoorLightKinds` (ADR-0066): proposed, not yet approved. Every
  number is `[ASSUMPTION]` until it is.
- The reference catalog (ADR-0061) gets a tenth category, **Ornaments** (the owner: "can you add individual light types in a
  dedicated tab called ornaments/furnishing", then of the longer title: "too long use one"): the small things a room is dressed
  with, one entry each. The light fittings are its first entries (`OrnamentsLight<Fitting>`), each proposed.

## Consequences
- Street lamps, the alley's and the mall's lanterns and lit windows can take Lantern amber by name; they do not yet
  (their colours are still their own, in the scripts and the game).
- The Unreal build's lamps are not built to this (FURN-067 waits for the reference's approval, ADR-0066).
- Not decided: whether Soft white earns its name (it reads nearly as Warm white), more neon colours, a colour the
  player chooses (F-LIGHTS level 3, not picked).
