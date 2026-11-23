---
doc_id: WASTED-SPEC-FURNITURE
title: Wasted — Furniture (designs, the furniture companies and their outlets, furniture in the home)
version: 0.4.1
status: draft            # every row proposed; the owner accepts
last_updated: 2026-11-22
parent: docs/specs/GAME_SPEC.md        # extends §4.5 PROP; same ID rules; new prefix FURN
related: docs/specs/domains/WORLD_MAP.md, docs/specs/domains/LIVING_CITY.md, docs/art/INTERIOR_STYLE.md
prototype: Tools/citysim/               # delivery, stock and money rules proved here first (to write)
scope_source: docs/backlog/feature_choices.json
---

# Furniture

What furniture there is, where the player sees it and buys it, how it gets home, and what the player
does with it there. Written from the owner's request and picks of 2026-11-22: furniture designs
(`F-FURN-ITEMS`) and the furniture company and outlets (`F-FURN-STORE`) at level 2, furniture in the
home (`F-PROP-FURNISH`) at level 2 and carrying things (`F-OBJ-CARRY`) at level 1, all for the first
slice. This file holds the detail of PROP-003.

## 0. How to read scope here

Every requirement names a **feature** and a **level** from `docs/backlog/features.json`.

> A requirement is in scope for a phase only if the owner's pick for that feature at that phase's
> milestone is at or above the requirement's level (`docs/backlog/feature_choices.json`).
> Milestones: first slice = PH1, life depth = PH3, full game = PH6. Undecided means ask.

There is no Phase column: the owner's picks decide when. Priority says how much it matters once in
scope. Status of every row: `proposed`, except FURN-020 and FURN-021: `accepted` (the owner asked for the outlet to be built, 2026-11-22; ADR-0027), FURN-050 `accepted` (met for furniture by MENU.md INV-001, the owner's pick of `F-INV-ITEMS` 1, 2026-11-23), and FURN-051, FURN-052 and FURN-055: `accepted` (the owner, 2026-11-23, picking "Arrange + add, priced" for furniture in the building designer: move, turn and take away the pieces in the home and add pieces from a priced list; BUILDER.md §2.6). A piece taken away goes to the player's belongings and is placed from them (INV-001); until buying at the outlet (FURN-025) is built, a piece is bought straight into place `[ASSUMPTION]`; and FURN-060 to FURN-063: `accepted` (the owner approved the house plan and its four furnishing levels and picked F-HOME-FURNISH level 3, 2026-11-23; ADR-0035). "Proven by" names the test that is still to be written, in
`Tools/citysim/test_furniture.py` for rules and in a Blender look test for designs.

Storage containers (`F-PROP-STORAGE`) and the assembly mini-game (`F-FURN-ASSEMBLE`) have no pick and
are not in this file; the owner said both come later.

## 1. The rules everything follows

1. **Furniture is data.** A piece is a row in `World/rules/furniture.json` built from a shared kit of
   parts, not a separate model. The catalogue, the companies, prices, fees and times all live there
   (ECO-012). Outlets are lots in the block files like any other building (WORLD_MAP).
2. **Calculated, not ticked** (TECH-004). Where a delivery van is at 15:20, how many of a sofa an outlet
   has left, which collection is in the catalogue on day 40: each is worked out from the clock, the data
   and what was bought. Bodies (vans, crews, boxes) appear only near a player.
3. **Money moves only through the ledger.** Every purchase, fee and buy-back is a ledger transaction in
   whole chips; nothing happens if it cannot be paid.
4. **One owned thing, one place.** Every piece the player owns is in exactly one place at a time: the
   inventory, a unit (placed), or on its way (booked for delivery).

## 2. Requirements

### 2.1 Designs — `F-FURN-ITEMS`

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| FURN-001 | The catalogue shall be data. Each base piece has a stable id, a room (living, bed, kitchen, bath, children, office, hall, garden, shop), a kind, a footprint on a 5 cm grid, a height, whether it stands on the floor, hangs on a wall or sits on another piece, its places to sit, lie or use (CITY-030), its kit parts, its flat-pack boxes (count and size) and an assembly time. Each catalogue item is a base piece in one finish and colour, with its own id and usual price in chips. | F-FURN-ITEMS | 1 | P0 | `test_FURN_001_catalogue_valid` |
| FURN-002 | Every piece shall be built from one shared kit of parts (legs, frames, panels, tops, cushions, doors, drawers, handles, shades) on the 5 cm grid, so a new piece is a new row of data. The Blender look test and the engine build pieces from the same rows. | F-FURN-ITEMS | 1 | P1 | look test |
| FURN-003 | Every piece shall follow the interior style (ART_DIRECTION §2.6, rules I1–I9, and `docs/art/INTERIOR_STYLE.md`; ADR-0026). A Blender look test renders every catalogue item on a contact sheet and checks each piece's built size against its data. | F-FURN-ITEMS | 1 | P0 | look test |
| FURN-004 | Every piece shall be able to get into a home: carried whole by one or two people through a door and up the stairs (WLD-050 to WLD-053, `World/rules/storeys.json`), or in its boxes. A piece too big for that is marked flat-pack only, and every box fits. | F-FURN-ITEMS | 1 | P0 | `test_FURN_004_fits_through_doors` |
| FURN-005 | Pieces shall come in the four style lines Plain, Old Town, Workshop and Curve (ADR-0026; names `[ASSUMPTION]`), and items in a line shall be grouped into sets that match across a room. | F-FURN-ITEMS | 1 | P1 | `test_FURN_001_catalogue_valid` |
| FURN-006 | The range shall have at least 200 base pieces and 1,500 catalogue items `[ASSUMPTION]`, covering every room of FURN-001, including shop fittings for the player's own shop (PROP-005). | F-FURN-ITEMS | 2 | P1 | `test_FURN_006_range_size` |
| FURN-007 | Items shall carry the seasons and years they are sold in, so the catalogue on any day is worked out from the calendar (`World/rules/calendar.json`); collections come and go by editing data. | F-FURN-ITEMS | 2 | P2 | `test_FURN_007_catalogue_by_day` |
| FURN-008 | Each piece shall show three stages of wear (new, used, worn) from its condition (FURN-056): paler edges, stronger mottling, sagging cushions. | F-FURN-ITEMS | 2 | P2 | look test |

### 2.2 The furniture companies and their outlets — `F-FURN-STORE`

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| FURN-020 | The town shall have huge furniture outlets, the first outside the East Gate (Q-30, ADR-0027): a big shed with a car park, a showroom floor of furnished room sets along a marked walking route (at least one set for every kit, ADR-0029), a café, a self-serve warehouse hall of boxed stock, tills and a delivery bay with vans. Each outlet is a lot in the block files with its layout as data; its inside is built only near the player (WLD-050). | F-FURN-STORE | 0 | P0 | `test_FURN_020_outlet_outside_the_walls` (Tools/worldgen) |
| FURN-021 | Outlets shall be staffed by residents with outlet roles (sales staff, till, warehouse, drivers, fitters; `World/rules/roles.json`) during opening hours from data, and residents shall visit them to browse as part of their routines. | F-FURN-STORE | 0 | P1 | `test_FURN_021_residents_browse_and_staff_work` (Tools/worldgen) |
| FURN-022 | Every piece on a showroom floor shall carry a tag: name, price, size, its other finishes and colours, and whether it comes flat-packed. The player can switch the piece on display to any of its finishes to see it in place, and sit or lie on pieces that have places for it. | F-FURN-STORE | 1 | P0 | engine test |
| FURN-023 | The player shall browse the whole catalogue of a company at its outlets, and at home from the printed catalogue an outlet hands out free (an owned item, PROP-001) `[ASSUMPTION]`. Ordering from home books delivery only. | F-FURN-STORE | 1 | P1 | engine test |
| FURN-024 | Each company shall have its own ledger account, a share of the catalogue and a price level from data; its takings pay its staff and stock (`sys:wages`, `sys:spend`), so the money supply stays within the band of PROP-018 with no player. | F-FURN-STORE | 1 | P0 | `test_FURN_024_money_supply` |
| FURN-025 | The player shall put pieces in a basket and pay at the till from cash or their current account in one ledger transaction to the company's account; the purchase is refused whole if it cannot be paid. Each piece bought becomes an owned item with its own instance id (PROP-001). | F-FURN-STORE | 1 | P0 | `test_FURN_025_buy_all_or_nothing` |
| FURN-026 | For each piece bought the player shall choose **instant** or **delivery**. Instant puts the piece straight into the player's inventory and is free (owner, 2026-11-22; the charge is a number in data, now 0). Delivery is booked to a unit the player owns or rents, on a day and time slot, flat-packed or assembled, for a delivery fee by size from data `[ASSUMPTION]`; assembled delivery adds the assembly fee of FURN-029. | F-FURN-STORE | 1 | P0 | `test_FURN_026_instant_and_delivery` |
| FURN-027 | On the booked slot a company van with a crew (two people for a piece that needs two `[ASSUMPTION]`) shall drive from the outlet's delivery bay through traffic (CITY-043 onward) to the building, park in the nearest bay, and carry each piece (CITY-032) through the door and up the stairs or lift into the unit, set it down, and drive back. The van's place at any moment is worked out from the slot, the route and the carry times; it is a body only near a player. | F-FURN-STORE | 1 | P0 | `test_FURN_027_delivery_run` |
| FURN-028 | A delivered piece shall be placed in the unit where the player marked it when booking, or at a free spot inside the door; flat-packed pieces arrive as their boxes. If the player no longer owns or rents the unit when the van arrives, the pieces go to their inventory and they are told `[ASSUMPTION]`. | F-FURN-STORE | 1 | P0 | `test_FURN_028_delivery_lands` |
| FURN-029 | A flat-packed piece shall stay boxed until it is assembled, either by the player with a timed action (time per piece from FURN-001) or by a fitter the player pays an assembly fee from data, who comes on a booked slot (or with the delivery) and assembles it in place. Owner, 2026-11-22: "paid assemble as well as player can assemble". | F-FURN-STORE | 1 | P0 | `test_FURN_029_assembly` |
| FURN-030 | There shall be three companies `[ASSUMPTION: names]`: a budget flat-pack chain, a mid-range chain and a premium design house, with outlets of different sizes, two of them huge, each selling its own share of the catalogue at its own price level. | F-FURN-STORE | 2 | P1 | `test_FURN_001_catalogue_valid` |
| FURN-031 | Each outlet's stock of each item shall be worked out from its restock time, the residents' purchases and the player's own (as ECO-010), not ticked. An item out of stock can still be bought for delivery on the first day after it restocks; display pieces are not sold except in clearance. | F-FURN-STORE | 2 | P1 | `test_FURN_031_stock` |
| FURN-032 | Each company shall have a number of vans and slots a day from data; booked slots fill, and the player is offered the earliest free one. | F-FURN-STORE | 2 | P1 | `test_FURN_032_slots` |
| FURN-033 | Prices shall drop on sale days from the calendar, and when a collection leaves the catalogue its display pieces are sold as ex-display at a discount, in a used condition. | F-FURN-STORE | 2 | P2 | `test_FURN_033_sales` |
| FURN-034 | Residents shall buy furniture: a household that moves house (PROP-022) or whose furniture has worn out buys from the company nearest its price level, and its deliveries are vans in traffic like the player's. | F-FURN-STORE | 2 | P2 | `test_FURN_034_resident_orders` |
| FURN-035 | The player shall sell a used piece back to a company that sells that item, at a data share of its price × its condition, paid from the company's account; a company that cannot pay buys nothing. The player can also set furniture out on a market stall (PROP-008). | F-FURN-STORE | 2 | P1 | `test_FURN_035_buy_back` |

### 2.3 Furniture in the home — `F-PROP-FURNISH`, `F-OBJ-CARRY` (detail of PROP-003)

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| FURN-050 | Pieces the player owns that are not placed or on their way shall be in the player's inventory, with no limit `[ASSUMPTION]` until storage containers exist (`F-PROP-STORAGE`). The inventory survives closing the slice (QST-008). | F-PROP-FURNISH | 1 | P0 | `test_FURN_050_one_place` |
| FURN-051 | The player shall place a piece from the inventory into a unit they own or rent: it snaps to the grid, must not overlap walls or other pieces, must leave doors and stairs clear (`keep_clear` in `storeys.json`), and must fit under the ceiling. Once placed it is recorded against that unit. | F-PROP-FURNISH | 1 | P0 | `test_FURN_051_placing` (Tools/citysim/test_furnish.py) |
| FURN-052 | The player shall pick a placed piece back up into the inventory, in a unit they own or rent. | F-PROP-FURNISH | 1 | P0 | `test_FURN_052_taking_away_and_moving_are_free_buying_is_priced` (Tools/citysim/test_furnish.py; the inventory half with FURN-050) |
| FURN-053 | Selling or losing a unit (sale, repossession, PROP-017, PROP-019) shall include every piece still placed in it, and their value counts toward the price (PROP-011). When a player who rents moves out or is evicted (PROP-013), their placed pieces go back to their inventory `[ASSUMPTION]`. | F-PROP-FURNISH | 1 | P0 | `test_FURN_053_sold_with_unit` |
| FURN-054 | Placed pieces with places to sit, lie or use shall become interaction points (CITY-030) for the player and for the residents who live in or visit the unit. | F-PROP-FURNISH | 1 | P1 | `test_FURN_054_points` |
| FURN-055 | The player shall move and turn placed pieces freely (turns in 15° steps `[ASSUMPTION]`), hang wall pieces on walls and stand small pieces on others; the layout is saved and, from PH2, visitors see it. | F-PROP-FURNISH | 2 | P0 | `test_FURN_051_placing`, `test_FURN_055_turns_in_steps_and_the_footprint_turns_with_it` (Tools/citysim/test_furnish.py) |
| FURN-056 | Each piece shall have a condition worked out from its age and how much it is used (from the clock and the routines), shown in the stages of FURN-008, lowering what it sells for. | F-PROP-FURNISH | 2 | P2 | `test_FURN_056_wear` |
| FURN-057 | The player carrying a piece shall walk slower by its size, and only pieces one person can carry are carried; bigger ones are moved in place (FURN-055) or by a crew. | F-OBJ-CARRY | 1 | P2 | engine test |

### 2.4 How residents' homes are furnished — `F-HOME-FURNISH` (ADR-0035)

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| FURN-060 | Every home shall be furnished from data: the plan of its type (rooms, doorways, windows, stair) and each room's pieces with their places and the furnishing level each first comes in at, in `World/rules/home_plans.json`; the home's line (FURN-005) decides which version of each piece stands there. | F-HOME-FURNISH | 1 | P1 | `test_FURN_060_every_home_has_a_plan` |
| FURN-061 | Every plan shall be checked: each piece inside its room, no two floor pieces in each other, doorways, the front door, stairs and lift kept clear, and nothing taller than the sill in front of a window. | F-HOME-FURNISH | 1 | P1 | `test_FURN_061_plans_keep_clear` |
| FURN-062 | There shall be four furnishing levels, Bare, Basic, Homely and Full, each adding pieces to the one before; a home shows the pieces of its level and those below. | F-HOME-FURNISH | 2 | P1 | `test_FURN_062_levels_add_up` |
| FURN-063 | A household's furnishing level shall be worked out from data and the world clock, never ticked or stored: it starts from a level set by the household's daily wages (bands in `World/rules/furnishing_levels.json`) and rises a level each time its furniture savings, a share of its wages for each day since it moved in, reach the next level's cost; on the day a level rises that cost moves from the residents' account to `sys:spend` in one ledger transaction (until FURN-034 has residents buy from the companies). | F-HOME-FURNISH | 3 | P1 | `test_FURN_063_level_follows_the_household` |

## 3. Data

`World/rules/furniture.json` (to write): kit parts, base pieces, finishes, colours, catalogue items,
style lines and sets, companies (account, price level, share of the catalogue, vans, opening hours),
fees (instant 0, delivery by size, assembly), times (carry, assembly, slot length), restock, sale days,
buy-back share. Every number `[ASSUMPTION]` until tuned in play.

## 4. Open questions

| ID | Question | Why it matters | Default if unanswered |
|---|---|---|---|
| Q-30 | Where do the huge outlets go? Old Town is 8×8 blocks of small lots. | A big shed with a car park needs a large lot, and the map is data. | **DECIDED 2026-11-22:** outside the East Gate, a new block beyond the wall (ADR-0027). |
| Q-31 | Does an empty flat the player buys or rents come furnished? | Decides whether a new player needs to buy furniture before they can sleep. | Empty units come empty; a unit bought with its household still living there keeps that household's furniture, which is not the player's. |
| Q-32 | Names and signs of the three companies. | Signs, boxes, vans and catalogues carry them. | Placeholder names in data until the owner names them. |

## 5. Change log

| Version | Date | Change |
|---|---|---|
| 0.4.1 | 2026-11-23 | FURN-050 accepted: pieces taken away in the designer go to the belongings (MENU.md INV-001) and are placed from there. |
| 0.4.0 | 2026-11-23 | FURN-051, FURN-052, FURN-055 accepted and built in the slice's building designer (BUILDER.md §2.6, ADR-0031): `World/rules/furnishing_levels.json` (43 pieces with sizes and prices), `Tools/citysim/furnish.py` and `test_furnish.py`, ported as `Furnish` in `Prototype/play/builder.js`. |
| 0.1.0 | 2026-11-22 | First draft from the owner's request and picks (`F-FURN-ITEMS` 2, `F-FURN-STORE` 2, `F-PROP-FURNISH` 2, `F-OBJ-CARRY` 1): FURN-001 to 008, 020 to 035, 050 to 057; Q-30 to Q-32. All proposed. |
| 0.3.0 | 2026-11-22 | The owner asked for at least ten more furniture kits and more kinds of objects and decoration: look test `furniture_kit_18.py` (twelve kits, 78 base pieces in the four lines; renders in `Art/StyleTests/furniture_kit_18/`, not approved). FURN-001 gains a hall room. |
| 0.2.0 | 2026-11-22 | The outlet built outside the East Gate (owner's request; Q-30 decided; ADR-0027): FURN-020 and FURN-021 accepted, with tests. FURN-003 and FURN-005 point at the approved look (ADR-0026). |
