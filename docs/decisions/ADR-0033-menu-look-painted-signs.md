---
adr: ADR-0033
title: The game menu looks like painted signs, fills the screen, and sorts its pages into You, Town and Game
status: accepted          # owner in chat on 2026-11-23, from three samples: "C, Painted signs"
date: 2026-11-23
relates_to: [UI-005, UI-011, UI-015, UI-016, UI-017, ADR-0004, ADR-0012]
---

## Context
The menu built for `F-UI-MENU` levels 1 and 2 was a dark box in the middle of the screen with eleven tabs in one row that
wrapped, every button the same orange, the game's own lettering showing through behind it, and Settings as one long list
whose labels were cut off. The interface had no look of its own: ART_DIRECTION §5 listed "UI style" as not started.

The owner asked to improve the menu, "the look and feel of the menu" and how it is organised first, and to see samples.
Look test 19 (`Art/StyleTests/menu_look_19/index.html`) showed three, each with all eleven pages and the same new
organisation, over the slice's own screenshots at four times of day.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| A · Lantern light: dark stone, cream lettering, one warm lit entry in a rail on the left | Closest to the menu as it was; the dusk mood of the town | One accent for everything; the pages still look alike |
| B · Plaster ledger: a sheet of plaster under a course of stone blocks, dark lettering, terracotta for actions | The calmest to read; daylight | Least like the lantern-lit town; bright over a night scene |
| **C · Painted signs: the whole screen; each page a painted board in its own colour of the town's palette, with its headline on it** | The home screen answers "how am I doing" at a glance; colour tells the pages apart; the palette rule W4 (colour from a few dull accents: shutters, awnings, cloth) carried into the interface | Covers the game entirely, so the wardrobe needs its own arrangement to show the character |

## Decision
The owner chose **C, Painted signs**.

- **The whole screen.** The menu covers the game with a dark, blurred veil. A bar on top always shows who you are, the day,
  the time, your cash and **Back to town**.
- **Home.** The menu opens on a board for every page: Me (tall, with your face), Money, Journal, Properties, Belongings,
  Wardrobe, Map, and small boards for Storage, Settings, Save and Help. Each board carries its page's headline (what you
  have in all, what is under way, what you own, your mark). A page's own key still goes straight to it.
- **A page** keeps its board's colour in its header and in its main buttons. Every board stands small down the left edge to
  change page, with Home on top. A page's sections are cards.
- **Groups.** Pages belong to You (Me, Money, Belongings, Wardrobe), Town (Journal, Properties, Storage, Map) or Game
  (Settings, Save, Help). The Game boards are small and painted in the quiet colour.
- **Colours** are the accents of ART_DIRECTION §2.3 as they look in shade, each with a lettering colour, in data
  (`World/rules/settings.json` `menu.colours`); boards and cards are matte and mottled (W1) and chunky with soft corners (W3).
- **Buttons.** In a row the first button is the main one, in the page's colour; the rest are outlined; selling, moving out
  and starting over are outlined in red.
- **Settings** shows one group at a time.
- **The wardrobe** drops the veil and stands at the right; the camera looks at the whole player from the front, in the
  space the page leaves free (owner, 2026-11-24).
- **The player's face is the real one, live** (owner, 2026-11-24), on the Me board and small in a page's top bar. The slice
  does it without a second render: the camera stands in front of the player, the veil has a hole where the face goes, and
  the game's picture is slid and scaled (CSS) so the face lands in the hole. One camera, so one such window at a time.

## Consequences
- ART_DIRECTION gets a section for the interface (§2.7, rules U1 to U6); "UI style" leaves its list of things not decided.
- UI-015 and UI-016 in MENU.md, tested in `Tools/worldgen/test_menu.py` (groups; a colour for every page whose lettering
  can be read on it).
- UI-017. A face drawn flat from the player's own colours and hair stands in only when the player cannot be framed (sitting,
  lying, in a car). The live face is lit as the player is where they stand, so it is dark at night away from a lamp; the
  camera stands 1.2 m in front of them, so with a wall nearer than that it looks from inside the wall. The engine build
  should render the head to its own target with its own light.
- The counters (shop, bank, stall, estate agent), the conversation box and the building designer still have the earlier
  dark-box look. Bringing them to this look is not decided.
- The additions shown as "proposed" in the samples (a Today card, a week of money in and out, a filter on Belongings,
  following a quest, a dot on a board with something new) are not built and not in the backlog.
- Samples A and B stay in look test 19 as rejected directions.
