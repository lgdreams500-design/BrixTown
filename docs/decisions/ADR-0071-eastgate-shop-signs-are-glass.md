---
adr: ADR-0071
title: Eastgate Mall's shop signs are coloured glass, lit
status: accepted          # the owner in chat, 2026-11-28: "can you use the colors from row 1 but use glass as the material", then "Yes, every shop", then "Yes, add it" (a close view of one shop), then "Approve, all four"
date: 2026-11-28
relates_to: [ADR-0004, ADR-0033, ADR-0065, ADR-0066, ADR-0069, ADR-0070, WLD-059]
---

## Context
Every outlet in Eastgate Mall has a sign band over its front in its kind's colour (`MALL_LOOK["signs"]` in
`Tools/blender/hours_50.py`). In the approved references the bands were matte and sat in the galleries' shade, so they
read as dull strips. The owner asked for sign options that were not pastel (2026-11-27). The round of options went like
this:
- options in jewel, black, deep matte, timber and lightbox colours, then the same as shiny glass;
- the town's accents (ART_DIRECTION §2.3), first as painted and then darkened as rule U1 does in shade;
- one shop with each.

The owner then picked the mall's own sign colours (row 1 of the town sheet) under glass, for every shop. The town's
rules say surfaces are matte (W1) and boards are matte (U2), so glass signs are an exception, and it needs this record.

## Decision
- Every outlet's sign in Eastgate Mall is a back-painted glass band:
  - its colour is its kind's in `MALL_LOOK["signs"]` (unchanged), even and less rough (0.3), behind a clear pane;
  - the band is raked back 5 degrees, so its glass shows the hall's lit ceiling as a thin line along its top;
  - its name is in cream letters lit from behind and on all day (`SIGN_LETTERS`);
  - a brass trough on three arms stands out from the slats above it. Its light falls onto the sign. The light is the
    sign lamp's kind in `World/rules/lights.json`, one lamp's strength to every 6 m of front, and it is on all day like
    an outlet's own lights.
- The Food Market's food outlets (ADR-0070) take the same glass and lit letters. Their signs follow the round wall, so
  they are not raked and have no trough; the hall's uplights light them.
- The exception to W1 and U2 holds for Eastgate's shop signs only, as part of the court's own look (ADR-0069). The
  town's shops keep their painted, matte boards.
- The references are `MallsEastgateShop` (new: one shop's front close by, The Linen Press, at every setting),
  `MallsEastgateCourt` and `MallsEastgateFoodCourt` (made again with the glass signs), all in `Art/Reference/Malls/`.
  `Tools/blender/hours_50.py` makes them, in `shop_banner()` and `sign_glass()`.

## Consequences
- In the game the mall's signs are still the matte bands. Building them to this reference needs a glass material and the
  sign light in `Game/Source/BrixWorld/Private/BrixMall.cpp`. Until then the gap is noted in `Game/README.md` under "Not yet".
- The approved food court shot its outlets' names at three times the glow they have now (1.2, the same as every shop's).
  Its reference is made again with the dimmer letters.
