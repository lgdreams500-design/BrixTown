---
adr: ADR-0049
title: "Lamplight": the Unreal build's game menu, counters and buy menu in one look of warm stone cards
status: proposed        # built on the owner's word in chat, 2026-11-25; awaiting the owner's verdict on the result
date: 2026-11-25
relates_to: [UI-005, UI-006, UI-007, UI-008, UI-009, UI-010, UI-011, UI-015, UI-016, UI-019, UI-020, ADR-0033, ADR-0044, ADR-0004]
supersedes_in_part: [ADR-0033]   # the look of the menu in the Unreal build, not its pages, groups or colours
---

## Context
The game menu and its settings (`F-UI-MENU` level 2, `F-UI-SETTINGS` level 1) were brought to the Unreal build after the
money, shop and quest screens. In the browser slice the menu is "Painted signs" (ADR-0033): flat painted boards over a
blurred veil; the counters and the buy menu kept an earlier dark box with square corners. Asked to port the menu, the owner
said (2026-11-25): "you don't have to follow the style of slice for menu or item catalog in the store, as they don't match
overall game styling. feel free to come up with a better one".

## Options considered
| Option | For | Against |
|---|---|---|
| The slice's painted signs and dark boxes as they are | Nothing to decide | The owner says they do not match the game; two looks side by side (menu and counters) |
| A light plaster sheet (look test 19's B) | Calm to read | Rejected by the owner for the slice (ADR-0033): least like the lantern-lit town, bright over a night scene |
| **Lamplight: warm dark stone cards, rounded and chunky, the town dimmed behind; the page colours of ADR-0033 kept as lintels and marks; paper price tags in the store** | One look for every screen of the game; built from the art direction's own rules (W1, W3, W4, W6) | New; the slice is frozen (ADR-0045), so it keeps its own look |

## Decision
Lamplight, for every screen ABrixLife draws: the game menu, the bank's, shops' and the market's counters and the buy menu
(`Game/Source/BrixTown/Public/BrixLook.h`).

- **Cards of warm dark stone** (`#2b2723`) with corners rounded 10 to 18 px and a darker lower edge 4 to 5 px deep, so a
  card reads as a thick slab (W3); a hairline of warm light round its edge, no gloss, no gradient (W1).
- **The town stays in sight**: the menu dims and warms the game behind it (a brown veil, not a blur), and the clock stands
  still. Over day, dusk and night the cards read the same (U4).
- **The page colours stay** (ADR-0033, settings.json `menu.colours`): each page's colour is the lintel over its card on the
  home, the band of its header, its mark in the rail down the left; the rail lights the page that is open in its colour.
  The groups You, Town and Game and the boards' sizes are as before.
- **One main action** (U6): filled lantern orange (`#e0964e`) with dark letters on a darker edge; the rest outlined in cream.
- **Prices are paper tags** (`#e9dcbe` with dark letters), as tied to goods in a shop window; a price the purse cannot meet
  is a red tag. Keys are small paper keycaps.
- **In the store** each thing stands on a dark plinth in a pool of lantern light (W6), the chosen one ringed in lantern;
  the showcase is a deeper well with the thing large and turning, its tag, and the one button. Layout B and the rules of
  ADR-0044 are unchanged.
- **Lettering**: the engine's sans (Roboto) for everything, bold for names and figures, until a typeface is chosen for the
  game; cream on stone, dark on colour.

## Consequences
- The look is new for the interface only; nothing in the town changes (ADR-0004). It needs the owner's verdict: the shots
  are in `Art/StyleTests/lamplight_menu_36/` (the menu's home, Me, Belongings, Journal, Settings, Help; the grocer's counter,
  before and after).
- ART_DIRECTION §2.7 (Painted signs) stays the slice's; for the Unreal build this record holds until it is accepted, then
  §2.7 is rewritten to match. The colours of the cards are in `BrixLook.h` for now; if accepted they move to settings.json
  `menu` beside the page colours (TECH-005).
- The conversation box, the purse, the tracker and the toasts keep their earlier look; bringing them to Lamplight is for
  the owner to say.
- The live face on the Me page and in the top bar (UI-017) is not drawn yet in the Unreal build.
