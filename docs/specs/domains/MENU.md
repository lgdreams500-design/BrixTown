---
doc_id: WASTED-SPEC-MENU
title: Brix Town — The game menu (money, belongings, properties, journal, wardrobe, map, settings, save, help)
version: 0.3.2
status: in-progress      # F-UI-MENU level 2, the others level 1, accepted and built in the slice
last_updated: 2026-11-24
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

Then, still that day, the owner asked to improve the menu's look and feel and how it is organised, saw three samples
(look test 19) and chose "C, Painted signs" (ADR-0033): §1.1.

## 1. The menu

| Tab | What it holds | Key |
|---|---|---|
| Me | Your name, look and days in town; money, property, belongings and outfits at a glance (each a button to its tab); your work; how the people you know and the shops see you (UI-011) | C |
| Money | Cash, accounts, loans, the statement | I |
| Belongings | What you carry and the furniture not placed (INV-001) | G |
| Properties | First *Your buildings*: each building you own once, with **Remodel** (or **Redecorate your rooms**), **Furnish** and **Show on map**; a button that cannot be used yet is greyed out with the reason (a community house, a building with no design), and with nothing owned it says how to buy one. Then what you own and rent, to sell and let, as before | P |
| Journal | Quests under way, on offer, finished; people you know | J |
| Wardrobe | What you wear, changed for free; outfits saved by name; the clothes you own, bought at the mall's outlets, to put on (CHR-016) | U |
| Storage | Lockers and storage units, when `F-PROP-STORAGE` is picked; until then it says so and points to Belongings (UI-014) | |
| Map | The town's big map. A click marks a place (again on it: cleared), as does Show on map; the mark shows on both maps and in the street with how far it is, and goes when you get there. Filters in Settings: residents, quests, your buildings (outlined), names of shops and services, the mark (UI-012) | M |
| Settings | Picture, sound, what the screen shows, controls (UI-009) | |
| Save | Three slots, each with whose game, the day and when it was saved: save here, load, a new game in that slot (asked twice), back to the title; Continue carries on the slot played last (UI-013) | |
| Help | The controls of the platform played on, by context (UI-008) | H |

### 1.1 How it looks and where things are (ADR-0033, ART_DIRECTION §2.7)

The menu fills the screen over the veiled game. A bar on top always shows who you are, the day, the time, your cash and
**Back to town**. Esc opens the menu on its **home screen**: a painted board for every page, in that page's colour, with its
headline on it (Me with your face, the day and who thinks well of you; Money with what you have in all and what you owe;
Journal with what is under way and on offer; Properties with what you own and its worth; Belongings; Wardrobe; Map with
your mark and how far it is). A page's key still opens it directly, and Esc goes back to town from anywhere.

Pages belong to three groups: **You** (Me, Money, Belongings, Wardrobe), **Town** (Journal, Properties, Storage, Map) and
**Game** (Settings, Save, Help; small boards in a quiet colour). On a page every board stands small down the left edge,
Home on top; the page's header and main buttons take its colour, and its sections are cards. Settings shows one group at a
time. Groups, colours and board sizes are data (`World/rules/settings.json` `menu`).

**You, live** (owner, 2026-11-24: "the face should be real character face with live animation", "in wardrobe menu it's
better to show full character during editing"). The face on the Me board, and the small one in the top bar of a page, is
not a picture: it is a window on the game, with the camera standing in front of the player, so the face blinks and is
lit as it is where they stand. The wardrobe has no veil: the camera steps back and all of the player stands, from the
front, in the space beside the page, and changes as they are dressed. Sitting, lying or in a car, a drawn face stands in.
The camera's distances are data (`menu.live`).

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
| UI-015 | The menu shall open on a home screen showing every page with its headline, sort its pages into the groups You, Town and Game, always show who is playing, the day, the time, the cash in hand and a way back to town, and show Settings one group at a time. | F-UI-MENU | 2 | P1 | accepted | `test_UI_015_every_tab_is_in_a_group`; slice |
| UI-016 | Each page shall have a board colour and a lettering colour from data, taken from the town's palette, with the lettering readable on the board (contrast at least 3 to 1); a page's header and main buttons use them. | F-UI-MENU | 2 | P1 | accepted | `test_UI_016_every_tab_has_a_board_colour_that_can_be_read` |
| UI-017 | Where the menu shows the player it shall show the character themself, live (the face on the Me board and in the top bar; the whole character, from the front, in the wardrobe), not a drawing; a drawing may stand in only when the character cannot be framed. | F-UI-MENU | 2 | P1 | accepted | `test_UI_017_the_menu_shows_the_player_themself`; slice |
| UI-018 | The big map (M, and the menu's Map page) shall show the town as it is now, the open ground outside Old Town included (woods, commons and ponds, orchards, fields and their crops, plots for sale, paths, tracks and the roads out of town; WLD-056), with every tree; show the player as an arrow pointing the way they look; and zoom about the pointer with the wheel (1 to 6 times, + and - about the middle, 0 or the middle button for the whole town) and move when dragged, a drag never setting a mark. Owner, 2026-11-24. | F-UI-MENU | 2 | P1 | accepted | Slice: `drawBase`, `drawMaps` in `play.js` |
| CHR-014 | The player shall change what they wear (hair, clothes, their colours, extras; never the body) at any time, free, from the choices of the character designer. | F-CHR-WARDROBE | 1 | P0 | accepted | `test_CHR_014_an_outfit_is_only_clothes_and_hair_the_designer_offers` |
| CHR-015 | The player shall save what they wear as a named outfit (up to a number in data), change into one and forget one; outfits are kept in the save. | F-CHR-WARDROBE | 1 | P1 | accepted | `test_CHR_015_the_wardrobe_holds_a_few_outfits` |
| CHR-016 | Clothes bought at an outlet (ECO-014) shall be kept as owned and listed in the wardrobe as *Clothes you own*; putting one on sets the parts of the look it names (a garment and its colour, a bottom, shoes, a scarf, a cap, a bag) and nothing else, each a value the character designer offers. Owned clothes are kept in the save. This is the part of `F-CHR-WARDROBE` level 2 the owner asked for on 2026-11-24 ("some like clothes will be made available in wardrobe"); the pick stays at level 1, so changing is still free and anywhere, with no wardrobe at home. | F-CHR-WARDROBE | 2 | P1 | in-progress | `test_CHR_016_*` (Tools/citysim/test_economy.py) |

FURN-050 (the inventory) is met by INV-001 for furniture.

## 4. Not yet

Remapping controls, gamepad and touch play, colour-blind markers and reduced motion (F-UI-SETTINGS 2); using, giving and dropping things, carrying limits (F-INV-ITEMS 2);
clothes as bought items kept in a wardrobe at home (F-CHR-WARDROBE 2). Storage containers themselves (F-PROP-STORAGE).
The Painted signs look for the counters (shop, bank, stall, estate agent), the conversation box and the building designer.
The additions shown as "proposed" in look test 19 (a Today card, a week of money in and out, a filter on Belongings,
following a quest, a dot on a board with something new): not proposed in the backlog yet.

## 5. Change log

| Version | Date | Change |
|---|---|---|
| 0.3.2 | 2026-11-24 | CHR-016: clothes bought at the mall's outlets are listed in the wardrobe and can be put on (owner's request; ADR-0041). Belongings lists what is bought for the home and carried from the outlets as it does everything else. `F-CHR-WARDROBE` stays picked at 1. |
| 0.3.1 | 2026-11-24 | The big map shows the outskirts and every tree, the player as an arrow, and zooms and pans (UI-018; owner's request). |
| 0.3.0 | 2026-11-23 | The menu's look and organisation (owner: "C, Painted signs", ADR-0033): §1.1, UI-015, UI-016. On 2026-11-24 the owner added: the real face, live, and the whole character in the wardrobe (UI-017). |
| 0.2.0 | 2026-11-23 | `F-UI-MENU` level 2 (owner: "now build the game menu level 2"): UI-011 to UI-014, the Me and Storage tabs, the map's mark and filters, three save slots. |
| 0.1.1 | 2026-11-23 | Properties opens with *Your buildings*, where Remodel is always shown (greyed out with the reason when it cannot be used). |
| 0.1.0 | 2026-11-23 | First version, from the owner's request and picks; built in the slice (`life.js` menu, `play.js` applySettings, `World/rules/settings.json`). |
