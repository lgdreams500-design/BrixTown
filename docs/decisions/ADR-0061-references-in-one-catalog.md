---
adr: ADR-0061
title: The look references in one catalog — a folder per category, names that say what they are, a page to review them
status: accepted          # the owner in chat, 2026-11-26: "We need references for everything ... properly organized and cataloged. goal is that AI agent should not require multiple hops or thinking to get to a model, naming convention and referencing should be straight forward." and "rooms18 ... it should be IndoorRooms. interior_room_daylight is a bad reference"
date: 2026-11-26
relates_to: [ART-001, ADR-0004, ADR-0026, ADR-0055, ADR-0058]
---

## Context
`Art/Reference/` was one flat folder of 35 files with names from different times (`world_alley_morning_raw.png`,
`home_interior_row_14x12_2.png`, `vehicles_soft_box_2.png`), its README listed only part of them, and some references the
game is measured against lived elsewhere: the twelve rooms of `Tools/blender/furniture_kit_18.py` (ADR-0055) and the
furniture outlet's showroom in `Art/StyleTests/`, the Lamplight screens in a look test. Three sheets carried "look test,
not approved" in their own titles while the art direction called them approved. Superseded files sat beside approved
ones. An agent had to read the art direction, the README and several ADRs to find which picture covers a thing and what
in the game was made from it.

## Decision
- **A folder per category** under `Art/Reference/`: World, Buildings, Indoor, Shops, Malls, Furniture, Vehicles, People,
  Interface. **Names** `<Category><Subject>[_<Variant>].<ext>` in PascalCase; a reference's id is the name without the
  variant (`Indoor/IndoorRooms_Kitchen.png`, reference `IndoorRooms`).
- **One list**, `Art/Reference/catalog.json`: for each reference what it locks, its approval date and decision, the
  script that renders it, the game models and data built from it or measured against it, issues seen in it (for the
  owner to judge), and its old names; `gaps` for what has no reference yet; `retired` for what was taken out.
- **A page to review them**, `Art/Reference/index.html`, a tab per category, written from the catalog by
  `Tools/reference_catalog.py`, which first checks that every file is listed and named by the rule
  (`Tools/test_reference_catalog.py`). The owner opens it at any time and says what needs fixing; it is never edited by
  hand.
- **Promoted:** the kit-18 rooms as `IndoorRooms` (the owner: a good reference "as it shows different settings"), the
  outlet's showroom as `ShopsFurnitureOutlet` (approved with ADR-0027), the cars' cabins as `VehiclesSoftBox_Cabins`
  (ADR-0057) and the Lamplight screens as `InterfaceLamplight` (ADR-0049).
- **Retired** to `Art/Retired/`: `interior_room_daylight.png` (the owner: one setting for a generic room),
  `vehicles_soft_box.png` and `vehicles_classic.png` (superseded).

## Consequences
- The art direction's §1 table is replaced by a pointer to the catalog; living documents and code comments use the new
  paths. Decision records and look tests keep the names they had, which the catalog's `was` maps.
- The approvals are unchanged except as listed: nothing in the look changes by this. The issues the catalog lists (the
  rooms' open fourth side, floor boards at flagstone size, flat windows, burnt lamp shades) are for the owner to decide.
- A new reference is added by rendering it, putting it in its folder under its name, adding its entry and running the
  script; its approval still needs the owner and a decision record.
