---
adr: ADR-0051
title: Markers over heads and places are brass tokens with a sign for each kind of thing to do
status: accepted        # the owner in chat, 2026-11-25, choosing from look test 37: "A, brass token"
date: 2026-11-25
relates_to: [QST-005, QST-006, UI-012, UI-018, ADR-0049, ADR-0004]
---

## Context
The browser slice marks a person a quest wants with words over their head ("◆ quest", "◆ has something for you"), and the
map with coloured dots (life.js mark(), marks()). The Unreal build ported that as it was. The owner asked (2026-11-25):
"we need better over head markers than what we had in the slice. can you come up with good options to show like glowing
icon representing the type of interaction."

## Options considered
Look test 37 (`Art/StyleTests/overhead_markers_37/`, `options.png` and `icons.png`) drew four at dusk:

| Option | For | Against |
|---|---|---|
| **A. Brass token: a coin with the sign stamped in it, ringed in the kind's colour, in a warm halo** | Ties the markers to the reward coins (SND-019); reads at any distance; one round shape is easy to see | A coin may first read as money |
| B. Little lantern: an iron lantern lit from inside, the sign as a shadow on its glass | The most like a thing of the town; warmest at night | Heavier; the sign is small on the glass |
| C. Hanging sign: a small painted board on chains, as on the shop fronts | Clearest by day | More interface than world; blocky over a head |
| D. Ember sign: only the sign, drawn in lantern light | Lightest | Weakest in the sun; glow reads less as the town's matte look (W1) |

## Decision
The owner chose A (2026-11-25): "A, brass token".

- **One sign per kind of thing to do**, so the kinds are told apart without colour: a new quest (a letter), a quest's
  person to talk to (a speech bubble), going back to hand it in (a bubble with a tick: back to the line's giver, or to
  whom its first stage sent you), a daily job (a parcel), a place a quest sends you to (a pin, or what the place is: a
  shop's basket, a café's cup, the bank's columns, the market stall's awning), an estate agent (a house), a prices board
  (a tag), the player's own mark (a cream pin). The ring's colour (the page colours of ADR-0033) backs the shape up.
- **The token**: a brass face shaded from a lit top-left to a darker rim, the coin's edge showing beneath it, a light rim,
  the ring in the kind's colour, the sign stamped dark; a warm halo round it.
- **How it behaves**: far off (beyond 45 m for a person, 30 m for a place, up to 160 m) a spark of its colour; nearer the
  token, bobbing gently; within 6 m larger, with a small Lamplight caption and its key ("E Talk to Mei"); behind a wall a
  faint outline; out of view a small token and arrow at the edge of the screen. New things (a line offered, one to hand
  in) have a larger halo and a ring that pulses out from them; the rest are steady. Hidden while talking.
- **Something to get** (owner, 2026-11-25: "if there is a quest to by bread then we show the token where all the player
  can buy bread"; "yes, do that"): a quest step that needs the player to have a thing, or the shopping job, marks the
  shops with it in stock today, by their own sign (a grocer's basket, a café's cup), the nearest `have_shops` (3) of them,
  with an edge arrow only to the nearest; close by the caption says "Loaf of bread sold here". They go once it is
  carried; the shopping job then marks the one who asked. Shops no quest wants are not marked.
- **On the maps** the same tokens, small, stand for the people and places quests want and for the mark.
- Every number, colour and sign is data, `World/rules/markers.json`, checked by `world.marker_problems()` and its port
  `BrixMarkerProblems()` (TECH-005).

## Consequences
- The slice keeps its words over heads (it is frozen, ADR-0045).
- ART_DIRECTION §2.7 gains rule L7.
- Markers are drawn for what quests and jobs want (including the shops for a thing to get) and for the mark; a shop no
  quest wants has none.
- A place's token stands over the middle of its lot, a fixed height above the player's feet; one over its door or
  counter would need the counters' places (the mall's counters are not built yet).
