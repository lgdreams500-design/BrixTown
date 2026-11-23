---
doc_id: WASTED-SPEC-MENU
title: Brix Town — The game menu (money, belongings, properties, journal, wardrobe, map, settings, save, help)
version: 0.1.0
status: in-progress      # level 1 of each feature accepted and built in the slice
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
`F-INV-ITEMS` 1, `F-CHR-WARDROBE` 1 for the first slice. The owner took the suggested defaults ("Yes, with defaults"):

1. The platform's menu control opens it (Esc on a keyboard); Esc closes an open panel first.
2. While the menu is open the clock stands still (the slice is single-player; a shared world will not pause).
3. Remodel and Furnish can be opened from the menu, wherever the player is.
4. Clothes are changed anywhere, free, at this level.
5. One save slot.

UI-002 (an in-game phone as the hub, PH3) is where this menu goes later; the tabs carry over.

## 1. The menu

| Tab | What it holds | Key |
|---|---|---|
| Money | Cash, accounts, loans, the statement | I |
| Belongings | What you carry and the furniture not placed (INV-001) | G |
| Properties | What you own and rent; for each unit you own **Show on map**, **Remodel** (or **Redecorate**), **Furnish**, and sell and let as before | P |
| Journal | Quests under way, on offer, finished; people you know | J |
| Wardrobe | What you wear, changed for free; outfits saved by name | U |
| Map | The town's big map, with a mark at a place shown from Properties | M |
| Settings | Picture, sound, what the screen shows, controls (UI-009) | |
| Save | Save now, when it was last saved, load the last save, a new game (asked twice), back to the title | |
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
| UI-007 | Save shall save now, say when the game was last saved, load the last save, start a new game only when asked twice, and go back to the title; one slot. | F-UI-MENU | 1 | P1 | accepted | `test_UI_005_...` (one slot); slice |
| UI-008 | Help shall show the controls of the platform being played on, by context, from data. | F-UI-MENU | 1 | P1 | accepted | `test_UI_008_every_platform_maps_every_action` |
| UI-009 | Settings for the picture, sound, the screen and the controls shall be data with a type, default and range, kept on the device, not in the save, and take effect at once. | F-UI-SETTINGS | 1 | P0 | accepted | `test_UI_009_settings_are_valid`, `test_UI_009_a_bad_setting_is_caught` |
| UI-010 | Each platform (keyboard and mouse, gamepad, touch) shall map every action, and no control shall do two things where both can happen. | F-UI-SETTINGS | 1 | P0 | accepted | `test_UI_010_no_control_does_two_things` |
| INV-001 | The player's belongings shall be one list of what they carry and the furniture they own that is not placed; a piece taken away in the building designer goes to it, and a piece placed from it costs nothing and leaves it, in any home of theirs. | F-INV-ITEMS | 1 | P0 | accepted | `test_INV_001_taken_away_goes_to_belongings_and_back`, `test_INV_001_only_what_you_hold_can_be_placed` (Tools/citysim/test_furnish.py) |
| CHR-014 | The player shall change what they wear (hair, clothes, their colours, extras; never the body) at any time, free, from the choices of the character designer. | F-CHR-WARDROBE | 1 | P0 | accepted | `test_CHR_014_an_outfit_is_only_clothes_and_hair_the_designer_offers` |
| CHR-015 | The player shall save what they wear as a named outfit (up to a number in data), change into one and forget one; outfits are kept in the save. | F-CHR-WARDROBE | 1 | P1 | accepted | `test_CHR_015_the_wardrobe_holds_a_few_outfits` |

FURN-050 (the inventory) is met by INV-001 for furniture.

## 4. Not yet

Remapping controls, gamepad and touch play, colour-blind markers and reduced motion (F-UI-SETTINGS 2); the Me tab,
Storage, map filters, more save slots (F-UI-MENU 2); using, giving and dropping things, carrying limits (F-INV-ITEMS 2);
clothes as bought items kept in a wardrobe at home (F-CHR-WARDROBE 2).

## 5. Change log

| Version | Date | Change |
|---|---|---|
| 0.1.0 | 2026-11-23 | First version, from the owner's request and picks; built in the slice (`life.js` menu, `play.js` applySettings, `World/rules/settings.json`). |
