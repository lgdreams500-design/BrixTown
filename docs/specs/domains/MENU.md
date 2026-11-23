---
doc_id: WASTED-SPEC-MENU
title: Brix Town — The game menu (money, belongings, properties, journal, wardrobe, map, settings, save, help)
version: 0.2.0
status: in-progress      # F-UI-MENU level 2, the others level 1, accepted and built in the slice
last_updated: 2026-11-23
parent: docs/specs/GAME_SPEC.md        # extends §UI (UI-001 to UI-004); same ID rules; UI, INV and CHR rows
related: docs/specs/domains/BUILDER.md, docs/specs/domains/FURNITURE.md, docs/decisions/ADR-0031-designer-view-and-furniture-from-the-room.md
prototype: Tools/worldgen/world.py settings_problems, outfit_problems; Tools/citysim/furnish.py stow
scope_source: docs/backlog/feature_choices.json
---

# The game menu

The owner asked (2026-11-23) for an in-game menu with the standard options of such games: properties owned, items owned,
storage, the character's clothes, graphics toggles and the rest; key mapping to follow the platform. Features proposed
and picked the same day ("Add all as proposed", then "Suggested levels"): `F-UI-MENU` 1, `F-UI-SETTINGS` 1,
`F-INV-ITEMS` 1, `F-CHR-WARDROBE` 1 for the first slice; then the same day "now build the game menu level 2": `F-UI-MENU` 2. The owner took the suggested defaults ("Yes, with defaults"):

1. The platform's menu control opens it (Esc on a keyboard); Esc closes an open panel first.
2. While the menu is open the clock stands still (the slice is single-player; a shared world will not pause).
3. Remodel and Furnish can be opened from the menu, wherever the player is.
4. Clothes are changed anywhere, free, at this level.
5. One save slot at level 1; three at level 2.

UI-002 (an in-game phone as the hub, PH3) is where this menu goes later; the tabs carry over.

## 1. The menu

| Tab | What it holds | Key |
|---|---|---|
| Me | Your name, look and days in town; money, property, belongings and outfits at a glance (each a button to its tab); your work; how the people you know and the shops see you (UI-011) | C |
| Money | Cash, accounts, loans, the statement | I |
| Belongings | What you carry and the furniture not placed (INV-001) | G |
| Properties | First *Your buildings*: each building you own once, with **Remodel** (or **Redecorate your rooms**), **Furnish** and **Show on map**; a button that cannot be used yet is greyed out with the reason (a community house, a building with no design), and with nothing owned it says how to buy one. Then what you own and rent, to sell and let, as before | P |
| Journal | Quests under way, on offer, finished; people you know | J |
| Wardrobe | What you wear, changed for free; outfits saved by name | U |
| Storage | Lockers and storage units, when `F-PROP-STORAGE` is picked; until then it says so and points to Belongings (UI-014) | |
| Map | The town's big map. A click marks a place (again on it: cleared), as does Show on map; the mark shows on both maps and in the street with how far it is, and goes when you get there. Filters in Settings: residents, quests, your buildings (outlined), names of shops and services, the mark (UI-012) | M |
| Settings | Picture, sound, what the screen shows, controls (UI-009) | |
| Save | Three slots, each with whose game, the day and when it was saved: save here, load, a new game in that slot (asked twice), back to the title; Continue carries on the slot played last (UI-013) | |
| Help | The controls of the platform played on, by context (UI-008) | H |

Opened from the menu, the building designer goes back to the menu when it closes. The list of keys along the bottom of
the screen is gone: a hint says Esc opens the menu and H shows the controls.

## 2. Settings and controls

All in `World/rules/settings.json`, kept on the device apart from the save. *Picture* sets sun shadows, ambient
occlusion, bounce light and contact shade, and resolution together (O); each can be set apart. Also people with full
bodies, field of view, the frame counter; the level of all sound, music, street, voices, actions and the interface;
minimap, clock, money, names over residents, prompts, text size; look sensitivity, invert, camera distance, first person.

Controls are per platform: keyboard and mouse, gamepad, touch, each with its own default mapping. The slice is played
with a keyboard and mouse; the other two are its defaults for when they come (UI-004, PH3). Remapping is level 2.

## 3. Requirements

| ID | Requirement | Feature | Level | Priority | Status | Proven by |
|---|---|---|---|---|---|---|
| UI-005 | The platform's menu control shall open a menu over the game whose tabs are Money, Belongings, Properties, Journal, Wardrobe, Map, Settings, Save and Help; each tab's own shortcut opens it, and while the menu is open the clock stands still. | F-UI-MENU | 1 | P0 | accepted | `test_UI_005_every_tab_opens_from_a_control_on_every_platform` (Tools/worldgen/test_menu.py) |
| UI-006 | Properties shall list what the player owns and rents, and for each unit owned offer to show it on the map, to remodel or redecorate it and to furnish it, from wherever the player is. | F-UI-MENU | 1 | P1 | accepted | slice: Properties buttons (engine test to write) |
| UI-007 | Save shall save now, say when the game was last saved, load the last save, start a new game only when asked twice, and go back to the title; one slot (three at level 2, UI-013). | F-UI-MENU | 1 | P1 | accepted | `test_UI_005_...` (one slot); slice |
| UI-008 | Help shall show the controls of the platform being played on, by context, from data. | F-UI-MENU | 1 | P1 | accepted | `test_UI_008_every_platform_maps_every_action` |
| UI-009 | Settings for the picture, sound, the screen and the controls shall be data with a type, default and range, kept on the device, not in the save, and take effect at once. | F-UI-SETTINGS | 1 | P0 | accepted | `test_UI_009_settings_are_valid`, `test_UI_009_a_bad_setting_is_caught` |
| UI-010 | Each platform (keyboard and mouse, gamepad, touch) shall map every action, and no control shall do two things where both can happen. | F-UI-SETTINGS | 1 | P0 | accepted | `test_UI_010_no_control_does_two_things` |
| INV-001 | The player's belongings shall be one list of what they carry and the furniture they own that is not placed; a piece taken away in the building designer goes to it, and a piece placed from it costs nothing and leaves it, in any home of theirs. | F-INV-ITEMS | 1 | P0 | accepted | `test_INV_001_taken_away_goes_to_belongings_and_back`, `test_INV_001_only_what_you_hold_can_be_placed` (Tools/citysim/test_furnish.py) |
| UI-011 | The Me tab shall show the character, what they have (money, loans, property, belongings, outfits, each opening its tab), their work, and how the people and shops they know see them. | F-UI-MENU | 2 | P1 | accepted | slice (engine test to write) |
| UI-012 | The player shall mark a place on the big map, or from Properties; the mark shows on both maps and in the street with its distance, and goes when reached. What the map shows (residents, quests, the player's buildings, names of shops and services, the mark) shall be settings. | F-UI-MENU | 2 | P1 | accepted | `test_UI_012_the_map_has_filters_and_a_mark` |
| UI-013 | There shall be more than one save slot (three `[ASSUMPTION]`), each shown with whose game, the day and when it was saved, to save in, load, or start a new game in (asked twice when it holds a save). | F-UI-MENU | 2 | P0 | accepted | `test_UI_013_more_than_one_save_slot` |
| UI-014 | The Storage tab shall hold the player's storage containers once `F-PROP-STORAGE` is picked, and until then say so. | F-UI-MENU | 2 | P2 | accepted | `test_UI_005_every_tab_opens_from_a_control_on_every_platform` |
| CHR-014 | The player shall change what they wear (hair, clothes, their colours, extras; never the body) at any time, free, from the choices of the character designer. | F-CHR-WARDROBE | 1 | P0 | accepted | `test_CHR_014_an_outfit_is_only_clothes_and_hair_the_designer_offers` |
| CHR-015 | The player shall save what they wear as a named outfit (up to a number in data), change into one and forget one; outfits are kept in the save. | F-CHR-WARDROBE | 1 | P1 | accepted | `test_CHR_015_the_wardrobe_holds_a_few_outfits` |

FURN-050 (the inventory) is met by INV-001 for furniture.

## 4. Not yet

Remapping controls, gamepad and touch play, colour-blind markers and reduced motion (F-UI-SETTINGS 2); using, giving and dropping things, carrying limits (F-INV-ITEMS 2);
clothes as bought items kept in a wardrobe at home (F-CHR-WARDROBE 2). Storage containers themselves (F-PROP-STORAGE).

## 5. Change log

| Version | Date | Change |
|---|---|---|
| 0.2.0 | 2026-11-23 | `F-UI-MENU` level 2 (owner: "now build the game menu level 2"): UI-011 to UI-014, the Me and Storage tabs, the map's mark and filters, three save slots. |
| 0.1.1 | 2026-11-23 | Properties opens with *Your buildings*, where Remodel is always shown (greyed out with the reason when it cannot be used). |
| 0.1.0 | 2026-11-23 | First version, from the owner's request and picks; built in the slice (`life.js` menu, `play.js` applySettings, `World/rules/settings.json`). |
