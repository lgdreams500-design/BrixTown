---
doc_id: WASTED-SPEC-BUILDER
title: Wasted — The building designer (remodel a building you own, or build a new one on an open plot)
version: 0.3.0
status: draft            # a plan: every row proposed; levels and open questions wait for the owner
last_updated: 2026-11-22
parent: docs/specs/GAME_SPEC.md        # extends §4.5 PROP; same ID rules; new prefix BUILD
related: docs/specs/domains/WORLD_MAP.md, docs/specs/domains/FURNITURE.md, docs/decisions/ADR-0017-stone-brick-and-paving-by-rules.md, docs/decisions/ADR-0021-every-floor-true-size.md, docs/decisions/ADR-0022-real-estate-market.md
prototype: Tools/citysim/builder.py     # design checks, cost and construction schedule proved here first (to write)
scope_source: docs/backlog/feature_choices.json
---

# The building designer

The owner's request (2026-11-22): a house and building designer in the game, where players remodel a building or create
a new one on an open plot, choosing wall types, brick types, flooring, roof type, how many levels, ceiling height, lifts,
staircases and more. This file is the plan: what the designer is, the one idea it rests on, the order to build it in,
the requirements, and what the owner has to decide first.

## 0. How to read scope here

Every requirement names a **feature** and a **level** from `docs/backlog/features.json`. A requirement is in scope for a
phase only if the owner's pick for that feature at that milestone is at or above its level
(`docs/backlog/feature_choices.json`). Milestones: first slice = PH1, life depth = PH3, full game = PH6. Undecided means
ask. Priority says how much a row matters once in scope. Status of every row: `proposed`.

Three features are proposed for this (§6): `F-BLD-DESIGN` (the designer), `F-MAP-PLOTS` (open plots to build on) and
`F-BLD-CONSTRUCT` (how a design becomes a building). Picked for the first slice by the owner on 2026-11-22 ("Yes, suggested levels"): designer level 1 (remodel), open plots level 0, construction level 0 (instant). So BUILD-001 to BUILD-007 are in scope for the slice; building new, plots and timed construction wait for later milestones.

## 1. The one idea: every building is a design

Today a building is put together from rules when the town is built: the lot gives its footprint, door and floors;
`surfaces.json` its stone, brick and paving (ADR-0017); `storeys.json` and the block file its stairs, lifts and what each
floor is for (ADR-0021); the slice's code the rest (roof, windows, ornament). The designer needs the same answers, but
chosen by a player.

So the plan's first step is to make the answers explicit: **a building is a design**, a document of data that says
everything about it. The town's own buildings get their design from the rules, as now. A player's design is the same
kind of document, edited. One builder turns any design into geometry: the slice's builder now, and Unreal's later. One
checker says whether a design is allowed. The estate market reads its units from the design's floors (ADR-0022
already reads them from `storeys`).

That gives three things at once: remodelling is editing a design that already exists; building new is starting a design
on an empty plot; and creators (PH5) can later publish designs as templates without a second system.

### 1.1 What a design holds

| Part | What the player chooses | From |
|---|---|---|
| **Footprint** | The outline inside the plot, on a 0.5 m grid: a rectangle first, L-shapes and courtyards later (level 3) | Plot and planning rules (§3) |
| **Storeys** | How many; each one's height (ceiling height); the ground floor's own height | 2.6 to 4.5 m (ground 3.0 to 5.0); the count capped by planning |
| **Walls, outside** | Per facade and per storey band: the stone or brick pattern, or plaster and its colour | `surfaces.json` patterns (ashlar, brick running and Flemish, rubble, rusticated…) and the town's plaster colours |
| **Ornament** | String courses, quoins, cornice, plinth, window surrounds, keystones | The building kit (`Tools/blender/building_kit_07.py`) |
| **Roof** | Pitched, hipped, flat with a parapet, mansard; the tiles (barrel, slate, flat); chimneys; dormers | A roof catalogue (data) |
| **Openings** | Windows per facade and storey (type, width, rhythm, shutters, flower boxes, balconies); doors; a shop front with awning | An openings catalogue |
| **Circulation** | Stairs (switchback, straight, L-turn, spiral) and where they go; a lift; a roof hatch | Rules of ADR-0021, now chosen rather than placed |
| **Rooms** | Inside walls and doors that divide each floor into rooms, and what each room is for | A 0.5 m grid per floor |
| **Finishes, inside** | Per room: floor (boards, tiles, stone, carpet), wall (plaster colour, panelling, paper, tiles), ceiling (plaster, beams, coffers), skirting and cornice | An interior catalogue, following the interior style |
| **Furniture** | Already planned: placed from the inventory (`F-PROP-FURNISH`, FURNITURE.md) | — |

Everything above is a choice from a catalogue in data, and every catalogue item carries a price and a look that has been
checked against the art direction. Nothing a player picks is free-form geometry until level 3.

## 2. What the player does

### 2.1 Remodel (level 1)

1. Stand in or in front of a building or unit you own, open the designer.
2. Change what the shell already has: wall finish and colour per facade, brick or stone, roof tiles and colour,
   windows' style and shutters, doors; inside: flooring, wall finish, ceiling per room.
3. See the change in place, with its cost; undo freely.
4. Confirm: pay through the ledger; the work is done (instantly, or over days: `F-BLD-CONSTRUCT`).

Changing the outside needs the whole building; changing the inside needs only your unit (ADR-0022 units).

### 2.2 Build new (level 2)

1. Buy an open plot (`F-MAP-PLOTS`) or clear one you own.
2. Start from a template (an Old Town house, a brick terrace, a corner shop, a small block of flats) or from nothing.
3. **Shell**: draw the footprint; set the storeys and each one's height. The planning limits show as you go.
4. **Facades**: walls, bands, ornament, windows and doors, per facade.
5. **Roof**.
6. **Circulation**: place the stairs (and a lift if tall enough); the checker shows at once whether every floor can be
   reached from the door.
7. **Rooms**: inside walls and doors; each room's use.
8. **Finishes**, then **furniture** (as for any home).
9. **Review**: the checks (§4), the cost, the build time, a permit fee. Submit: the server checks it again and charges
   the ledger.
10. It is built (`F-BLD-CONSTRUCT`), then lived in: by you, or let or sold through the estate market, with residents
    moving in like anywhere else.

### 2.3 Free-form and published designs (level 3)

L-shapes and courtyards, towers with set-backs, bay windows, arcades; saving a design as a template; creators
publishing designs for sale (needs the creator platform, `UGC`, PH5).

### 2.4 The tool itself

A plan view (2D, one floor at a time, on the grid) beside a live 3D view of the real building in the real street;
snapping; copy a facade or a floor; mirror; undo and redo; templates; "copy from" any building in town. On a pad: the
same, with large targets. The designer works on a draft that nobody else sees until it is built.

## 3. The town's rules (planning)

The look of the town is locked (ADR-0004); players must not be able to break it. So the designer offers only what the
district allows, as data (a planning file):

| Rule | Example (all values to be set by the owner) |
|---|---|
| Palette by district | Old Town: rubble or ashlar, plaster in the town's colours, barrel tiles. Terraces: brick, slate or tiles |
| Height | Old Town up to 5 storeys; outskirts up to 4; towers only where the plan has them |
| Cover and set-backs | The building no closer than the plot's street line; at least a yard at the back |
| Street front | A door on the street; windows on every storey of a street facade |
| Neighbours | No windows in a party wall; a roof no higher than the plan's line over the street |

The checker rejects a design that breaks a rule, and the designer greys out choices that would.

## 4. What makes a design buildable (the checks)

Run in the designer as you work, and again on the server when you submit (the server is authoritative; never trust the
client):

1. **Fits**: inside the plot; every part on the grid; walls meet; storey heights in range; the roof closes.
2. **Planning**: §3 for its district.
3. **Reachable**: from the door, every room on every floor can be walked to: up the stairs, through the doors. This is
   the walk-and-climb check that now proves every town building (ADR-0021), run on the design.
4. **Stairs and lifts**: a stair to every storey; a lift from `lift_from_floors` storeys; headroom over each flight;
   a rail at each opening.
5. **Rooms**: every room has a door; every room people live or work in has a window; a home has somewhere to sleep, a
   kitchen and a bathroom; a shop has a counter.
6. **Cost**: the total is known before you confirm and you can pay it.

## 5. Order of work

Each step ends in something to see or play, as the feature process asks (prove the rules, then build).

| Step | What | Proves | Needs |
|---|---|---|---|
| 1 | **Buildings as designs**: write each town building's design from the rules (`Tools/worldgen`), and make the slice build from designs. Nothing changes on screen. | §1: one model for all | — |
| 2 | **The checker and the price list** in Python (`Tools/citysim/builder.py`), with tests: fits, planning, reachable, stairs, rooms, cost | §3, §4 | Step 1 |
| 3 | **Remodel in the slice**: finishes outside and in, on a building you own, paid through the ledger | Level 1 | Steps 1, 2; `F-PROP-ESTATE` |
| 4 | **Open plots**: some lots empty and for sale; clearing one you own | `F-MAP-PLOTS` 1 | — |
| 5 | **Build new in the slice**: shell, facades, roof, stairs and lift, rooms; construction | Level 2 | Steps 2 to 4 |
| 6 | **Unreal**: a building actor built from a design out of the modular kit (Blender parts), and the designer UI | PH-level build | Steps 1 to 5 |
| 7 | **Free-form and published designs** | Level 3 | Step 6; `UGC` |

## 6. Features proposed for the backlog

| Feature | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| `F-BLD-DESIGN` The building designer | None | Remodel finishes outside and in (M) | Build new: shell, storeys and heights, facades, roof, stairs, lifts, rooms (XL) | Free-form shapes, saved and published designs (XL) |
| `F-MAP-PLOTS` Open plots | None | A few empty plots for sale (S) | Clear and rebuild what you own; plots come up over time (M) | — |
| `F-BLD-CONSTRUCT` From design to building | Instant (S) | Takes game days, with hoardings and scaffolding, worked out from the clock (M) | Builders (residents) at work, deliveries, stages you can watch (L) | — |

Suggested: slice — designer 1, plots 0, construct 0; life depth — designer 2, plots 1, construct 1; full game — designer
3, plots 2, construct 2.

## 7. Requirements

| ID | Requirement | Feature | Lvl | Pri | Proven by |
|---|---|---|---|---|---|
| BUILD-001 | Every building shall be a design: one document of data giving its footprint, storeys and their heights, walls and ornament per facade and band, roof, openings, stairs and lifts, rooms and finishes. The town's buildings get theirs from the rules; a player's is the same kind of document. | F-BLD-DESIGN | 1 | P0 | `test_BUILD_001_every_building_has_a_whole_design` |
| BUILD-002 | The slice, and later Unreal, shall build a building only from its design; a design made from the rules shall look exactly as the building does today. | F-BLD-DESIGN | 1 | P0 | `test_BUILD_002_designs_pick_as_the_slice_does`; the slice's town mesh fingerprint unchanged (checked 2026-11-22) |
| BUILD-003 | Every choice in the designer shall come from a catalogue in data with a price and an approved look; adding a choice shall need only data and, for a new kind of part, a look test. | F-BLD-DESIGN | 1 | P0 | `test_BUILD_003_every_choice_comes_from_the_kit` |
| BUILD-004 | A player shall be able to remodel the outside of a building they own whole, and the inside of a unit they own: walls and their colour, brick or stone, roof tiles, windows and doors; floors, walls and ceilings per room. | F-BLD-DESIGN | 1 | P0 | `test_BUILD_004_who_may_change_what` (Tools/citysim/test_builder.py) |
| BUILD-005 | The designer shall show the change in place and its cost before it is confirmed, with undo; on confirm the cost shall move through the ledger. | F-BLD-DESIGN | 1 | P0 | `test_BUILD_005_the_cost_is_known_line_by_line`, `test_BUILD_005_a_full_remodel_is_about_a_quarter_of_a_house` (Tools/citysim/test_builder.py) |
| BUILD-006 | A design shall be checked as it is made and again by the server on submit: it fits, keeps the planning rules for its district, every room is reachable from the door, there is a stair to every storey and a lift where required, every room has a door and each lived-in room a window, and the player can pay. | F-BLD-DESIGN | 1 | P0 | `test_BUILD_006_a_remodel_is_checked` (Tools/citysim/test_builder.py) |
| BUILD-007 | The planning rules (palette by district, height, cover, street front, neighbours) shall be data, and the designer shall offer only choices they allow, so the town's look holds (ADR-0004). | F-BLD-DESIGN | 1 | P0 | `test_BUILD_007_the_town_keeps_its_own_district_rules`, `test_BUILD_007_a_district_offers_only_its_own_palette` (Tools/citysim/test_builder.py) |
| BUILD-008 | On an empty plot they own, a player shall be able to make a new building: draw the footprint on a 0.5 m grid, set the number of storeys and each one's height (2.6 to 4.5 m; the ground floor 3.0 to 5.0 m), and choose facades, roof, openings, stairs and lift, and rooms. | F-BLD-DESIGN | 2 | P0 | (to write) |
| BUILD-009 | Templates (an Old Town house, a brick terrace, a corner shop, a small block of flats) and "copy from" any building in town shall start a design. | F-BLD-DESIGN | 2 | P1 | (to write) |
| BUILD-010 | A finished design shall become units in the estate market (ADR-0022) like any building: lived in by its owner, or let or sold, with residents moving in. | F-BLD-DESIGN | 2 | P0 | (to write) |
| BUILD-011 | Some lots shall be empty plots with an owner and a price, bought and sold through the estate market. | F-MAP-PLOTS | 1 | P0 | (to write) |
| BUILD-012 | The owner of a whole building shall be able to clear it to an empty plot, its units' tenants given notice first. | F-MAP-PLOTS | 2 | P1 | (to write) |
| BUILD-013 | A confirmed design shall take game days to build, by its size; how far along it is shall be worked out from the clock (hoardings, scaffolding, then the building), never ticked. | F-BLD-CONSTRUCT | 1 | P1 | (to write) |
| BUILD-014 | Builders (residents with that role) shall work on the site in their day, and materials shall arrive by van. | F-BLD-CONSTRUCT | 2 | P2 | (to write) |
| BUILD-015 | Free-form footprints (L-shapes, courtyards, set-backs), bay windows and arcades; a design saved as a template; registered creators publishing designs for sale. | F-BLD-DESIGN | 3 | P1 | (to write) |

## 8. Open questions for the owner

| ID | Question | Why it matters | Suggestion |
|---|---|---|---|
| Q-33 | Where do open plots come from? | The town is full: every lot is built on | Turn a handful of lots in the outskirts into empty plots now; more later by clearing |
| Q-34 | May players change how the town looks, or only choose within each district's palette? | The look is locked (ADR-0004) | **Decided 2026-11-22 by the owner: "District palette".** Each district offers only its own stone, brick, plaster colours and tiles (`World/rules/planning.json`); anything new joins a palette only with an ADR and the owner's approval. |
| Q-35 | Does building take time? | Instant is simple; days feel real and give construction sites in the street | A few game days, worked out from the clock |
| Q-36 | How tall may players build, and where? | Height shapes the skyline and the streets | Old Town up to 5 storeys, outskirts up to 4, no player towers yet |
| Q-37 | Can a player clear a building they own? | Lets the town change; risks losing what people like | Yes, if they own all of it and the tenants have had notice |
| Q-38 | Who sees a draft, and do neighbours have a say? | Shared world (pillar 4) | Drafts are private; a built design is public; no neighbour vote |
| Q-39 | How much should building cost against what a player earns? | Balance with the estate market | **Decided 2026-11-22 by the owner: "Expensive", about 25% of value.** A full remodel of a typical house (about 1,900 brix) costs about 470 brix, some 8 game days of an active player's income (60 brix); a repaint about 60 brix. Prices in `World/rules/building_kit.json` `prices`. |

## 9. Change log

| Version | Date | Change |
|---|---|---|
| 0.3.0 | 2026-11-22 | Step 2 done: `Tools/citysim/builder.py` checks a remodel (what level 1 may change, who may change it, the kit, each district's palette from `World/rules/planning.json`, funds) and prices it line by line (`building_kit.json` `prices`): a full remodel of a typical house costs about 545 brix, 28% of its value; a repaint about 65 brix. Q-34 and Q-39 decided. The slice will need a port of `check` and `cost` (step 3), as the estate rules have. |
| 0.2.0 | 2026-11-22 | Step 1 done: every building is a design (`world.design_of`, `designs`, `design_problems`; the kit in `World/rules/building_kit.json`); the slice builds from designs and the town is the same to the last vertex. |
| 0.1.1 | 2026-11-22 | First-slice picks recorded: designer 1, plots 0, construction 0. |
| 0.1.0 | 2026-11-22 | First plan, from the owner's request: the designer, its one model of a building, the order of work, features and requirements proposed, open questions. |
