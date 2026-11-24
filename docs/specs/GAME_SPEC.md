---
spec_id: WASTED-SPEC
title: Brix Town — Game Specification
version: 0.5.13
status: draft            # draft | reviewed | approved
last_updated: 2026-11-24
owner: project-owner
engine: Unreal Engine 5.8
dcc: Blender 5.2 LTS       # 5.3 is what is installed and was used for all look tests; see Q-16
perspective: third-person
---

# Brix Town — Game Specification

> **One-line pitch:** A third-person multiplayer life simulation set in a living city, where players
> build a life (needs, careers, property, businesses) inside a simulated economy, and creators
> make and sell content for that world.

## 0. How to read and maintain this spec

This file is written for two readers: you (human) and AI coding agents. The rules below keep it
usable by both.

| Rule | Why |
|---|---|
| Every requirement has a **stable ID** (`ECO-003`). IDs are never renumbered or reused; retired ones are marked `status: cut`. | Code, tests, commits and tasks can reference a requirement unambiguously. |
| One requirement per row, written as a testable statement ("The server shall…"). | An agent can turn each row into a test. |
| Every requirement carries **Priority** (`P0` must / `P1` should / `P2` could), **Phase** (see §3) and **Status** (`proposed`, `accepted`, `in-progress`, `done`, `cut`). | Scope can be filtered mechanically. |
| Numbers that are guesses are tagged **`[ASSUMPTION]`**; undecided items are tagged **`[OPEN: Q-nn]`** and listed in §13. | Agents must not treat a guess as a decision. |
| Design decisions live in `docs/decisions/` as ADRs and are linked from here. | The spec says *what*; ADRs say *why*. |
| When a domain section exceeds ~40 requirements, move it to `docs/specs/domains/<PREFIX>.md` and leave a link. | Keeps each file small enough to load whole into an AI context. |
| Tests name the requirement they verify, e.g. `BrixTown.Economy.ECO_003_InterestAccrual`. | Gives requirement-to-test traceability. |
| Scope is set by **feature levels**. The owner picks a level (0 set dressing, 1 simple, 2 full) for each feature at three milestones; the picks live in `docs/backlog/feature_choices.json`. Domain specs tag each requirement with its feature and level. | One place decides what is in the game and when; see `docs/process/FEATURE_PROCESS.md`. |

**ID prefixes:** `CORE` game loop · `ART` art direction · `CHR` character · `ANIM` body motion · `NEED` needs · `CAR` careers · `PROP` property · `FURN` furniture · `BUILD` building designer ·
`CITY` world interaction and NPCs · `WLD` time, weather, vegetation · `ECO` economy · `BANK` banking ·
`MP` multiplayer · `ACC` accounts · `UGC` creator platform · `UI` interface · `TECH` architecture ·
`ASSET` asset pipeline · `TEST` test harness · `SEC` security and safety · `SND` sound · `QST` quests.

### Companion documents
| Document | Holds |
|---|---|
| `docs/art/ART_DIRECTION.md` | The approved look of the world and of characters, with palette, light recipes and reference images. |
| `docs/art/CHARACTER_FACE.md` | The face control set, expressions, mouth shapes and lip sync. |
| `docs/art/CHARACTER_MOTION.md` | Skeleton, movement signatures, state layers, talking while walking. |
| `docs/specs/domains/LIVING_CITY.md` | Map, residents and routines, things to use, vehicles, seasons and plants (CITY-010 and WLD-010 onward), with the controllers behind them. |
| `docs/specs/domains/FURNITURE.md` | Furniture designs, the furniture companies and their outlets, delivery, and furniture in the home (FURN-001 onward; the detail of PROP-003). |
| `docs/process/FEATURE_PROCESS.md` | How a feature goes from idea to game, and what makes the cut. |
| `docs/backlog/` | `features.json` (the backlog with levels), `feature_choices.json` (the owner's picks), `BACKLOG.md` (readable copy), `SCENARIOS.md`. The owner states picks in chat; agents record them (ADR-0008). |
| `Art/Reference/` | Approved reference images. Never overwritten. |
| `docs/decisions/` | ADRs (why each decision was made). |

---

## 1. Vision and pillars

| # | Pillar | What it means in practice |
|---|---|---|
| 1 | **A city that feels alive** | Dense interactivity: most visible objects, shops and NPCs respond to the player. The city changes with time, weather and seasons. |
| 2 | **A life worth building** | Long-term character progression through needs, skills, careers, property and businesses. |
| 3 | **A real economy** | A closed, auditable currency system with banks, loans and interest rates that move with simulated conditions. |
| 4 | **Shared world** | Players live in the same persistent city; what one player owns or builds, others see. |
| 5 | **Creator-powered** | Registered creators publish houses, clothing, furniture and character styles that players buy. |

### Non-goals (for now)
- Combat-centred gameplay, weapons systems, or crime-sandbox mechanics.
- Multiple cities or seamless travel between cities (one city per world first).
- Console and mobile clients (Windows PC first).
- Buying currency with real money, or cash-out by players or creators. The economy is in-game currency only (Q-04, decided).

---

## 2. Scope reality check

The draft goals describe several large products at once: a life sim, a persistent online world, an
economic simulation, and a user-generated-content marketplace. Each is feasible; building all of them
at once is not. This spec therefore orders the work so that **every phase ends in something playable
and tested**, and later systems are designed-for but not built until their phase.

---

## 3. Phases

| Phase | Name | Exit criteria (all must be true) |
|---|---|---|
| **PH0** | Foundation | Repo, folder layout, source control with large-file support, UE project compiles, dedicated server target builds, CI runs automated tests on every commit, one passing test per test layer (§11). |
| **PH1** | Vertical slice | One district of several blocks, with real traffic: residents drive their own cars on their routine and the player can drive (owner, 2026-11-22). Third-person character walks, interacts with ≥10 object types, has needs that decay and are satisfied, does one job, earns and spends money, state survives a server restart. |
| **PH2** | Multiplayer core | 2–16 `[ASSUMPTION]` players in the same city on a dedicated server; accounts and login; text chat; all PH1 systems server-authoritative. |
| **PH3** | Life depth | Careers with promotion, property purchase and furnishing, player-run shops, character designer, day/night, weather, vegetation growth. |
| **PH4** | Economy engine | Banks, deposits, loans, mortgages, central-bank rate simulation, price indices, economy dashboards and soak tests. |
| **PH5** | Creator platform | Creator registration, submission, automated validation, moderation, in-game store, payouts in game currency. |
| **PH6** | Scale and live ops | Measured player caps raised, telemetry, anti-cheat, backups, update pipeline. |

---

## 4. Player experience

### 4.1 Core loop — `CORE`

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| CORE-001 | The player shall control one character in third-person view with walk, run, jump and context-sensitive interact. | P0 | PH1 | proposed |
| CORE-002 | The game shall present a repeating loop: satisfy needs → work or trade → earn → spend or invest → unlock new options. | P0 | PH1 | proposed |
| CORE-003 | A character's full state shall persist between sessions and across server restarts. | P0 | PH1 | proposed |
| CORE-004 | A character shall continue to exist in the world's records while its player is offline (property, accounts, businesses remain). Offline needs behaviour is `[OPEN: Q-06]`. | P0 | PH2 | proposed |
| CORE-005 | Progression shall be tracked through skills, career rank, net worth and reputation, each visible to the player. | P1 | PH3 | proposed |

### 4.1a Art direction — `ART`

Details, numbers and reference images: `docs/art/ART_DIRECTION.md` (ADR-0004; daylight changed by ADR-0007).

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| ART-001 | Environments shall match the old-town look of `Art/Reference/world_alley_morning_raw.png` (forms and materials), the daylight of `Art/Reference/world_street_kit_light.png` (ADR-0007), and the world rules W1–W8 in ART_DIRECTION §2. | P0 | PH1 | accepted |
| ART-002 | Surfaces shall use the shared palette and the matte, mottled two-tone treatment; no glossy or strongly saturated walls. | P1 | PH1 | accepted |
| ART-003 | The game shall not apply a painterly or outline full-screen filter. | P1 | PH1 | accepted |
| ART-004 | Player and NPC characters shall share the option B "Soft" proportions (about 5.6 heads tall, head ×1.22) and the character rules C1–C7 in ART_DIRECTION §3. | P0 | PH1 | accepted |
| ART-005 | Every outdoor area shall stay readable at morning, midday and dusk; only light and haze change (see WLD-002). | P1 | PH3 | accepted |
| ART-006 | Approved reference images shall be kept in `Art/Reference/`, each reproducible by a script in `Tools/blender/`; changing the look requires a new ADR. | P1 | PH0 | accepted |

### 4.2 Character and designer — `CHR`

Face details: `docs/art/CHARACTER_FACE.md` (ADR-0005).

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| CHR-001 | A new player shall create a character by choosing body, face, hair, skin tone and starting outfit. Moved to PH1 by the owner's pick of `F-CHR-DESIGN` level 2 for the first slice. | P0 | PH1 | in-progress |
| CHR-002 | The designer shall expose sliders/morphs for body and face; all values shall be stored as compact data, not baked meshes. Moved to PH1 with CHR-001. | P1 | PH1 | in-progress |
| CHR-003 | Clothing and accessories shall be modular items that fit every supported body shape. | P0 | PH3 | proposed |
| CHR-004 | Appearance shall replicate to other players and load within 2 s of the character becoming relevant `[ASSUMPTION]`. | P1 | PH3 | proposed |
| CHR-005 | The appearance system shall accept creator-made items (UGC) without a client patch. | P1 | PH5 | proposed |
| CHR-006 | Characters shall have skills (e.g. cooking, fitness, charisma, technical) that improve through use. | P1 | PH3 | proposed |
| CHR-007 | Every head (player and NPC) shall share one **face control set**: brows, eyelids, gaze, mouth corners and at least 8 speech mouth shapes. Faces made in the designer and creator-made hair or accessories shall animate with no per-character work. | P0 | PH3 | accepted |
| CHR-008 | Characters shall show facial expressions driven by game state and dialogue: at least neutral, happy, sad, angry, surprised and thinking, blended over time. | P1 | PH1 | accepted |
| CHR-009 | While a character speaks a dialogue line, its mouth shall move in time with that line (lip sync). Timing comes from the line's text, stretched to the length of its spoken audio (Q-14, ADR-0019). | P1 | PH1 | accepted |
| CHR-010 | Idle characters shall blink and shift gaze so that faces never look frozen. | P2 | PH3 | proposed |
| CHR-011 | Facial animation shall be level-of-detail controlled: full on the characters in a conversation and nearby, reduced or off with distance, within the client performance budget. | P1 | PH3 | proposed |
| CHR-013 | The designer shall show the character live as values change and keep every value inside the ranges in `World/rules/designer.json` (height within the cast range of ART_DIRECTION §3.2). Choices come from the world palette (rule C2), and the face control set shall work unchanged on every designed face (CHR-007). `F-CHR-DESIGN` level 2. Asked for by the owner on 2026-11-22. | P0 | PH1 | in-progress |
| CHR-012 | Open mouth shapes shall show upper teeth, lower teeth and tongue where the sound calls for them, and a face at rest shall read as neutral, not sad. | P1 | PH3 | accepted |

### 4.2a Body motion — `ANIM`

Details and prototype: `docs/art/CHARACTER_MOTION.md` (ADR-0006). The owner approved the prototype clips
(`Art/Reference/motion_walks.mp4`, `motion_walktalk.mp4`) on 2026-11-21; requirements shown in those clips
are accepted, the rest stay proposed.

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| ANIM-001 | Every person (player and NPC) shall use one skeleton with the joint names in CHARACTER_MOTION §2; height and build change its proportions, never its structure. | P0 | PH1 | accepted |
| ANIM-002 | Walking shall be shaped by a data-defined **movement signature** per character; giving a character a different walk shall need no new animation asset. | P1 | PH3 | accepted |
| ANIM-003 | Game state (needs, mood, weather, carried load) shall change movement through data-defined state layers, so that a tired, happy or cold character is recognisable from its walk at 10 m `[ASSUMPTION]`. See NEED-003. | P1 | PH3 | proposed |
| ANIM-004 | Animation shall be layered (legs and hips; chest and arms; head and eyes; face) so that any character can speak, look at a target and gesture while walking, without dedicated combined animations. | P0 | PH3 | accepted |
| ANIM-005 | When a character looks at something, the eyes shall lead, the head follow within limits and the chest turn last, with a blink on large shifts; with no target, a walking character shall glance along its path. | P1 | PH3 | accepted |
| ANIM-006 | In dialogue, the speaker shall nod and raise brows on stressed words and look at the listener at the start and end of a line; the listener shall look at the speaker for most of the line and show a reaction. | P2 | PH1 | accepted |
| ANIM-007 | A planted foot shall not slide more than 2 cm `[ASSUMPTION]` at any walking speed or body size. | P1 | PH1 | proposed |
| ANIM-008 | Clothing longer than the hips (coats, dresses, long tunics) shall not be pierced by the legs while moving; this needs skirt joints or cloth simulation. | P1 | PH3 | proposed |
| ANIM-009 | Body animation shall be level-of-detail controlled, and in multiplayer only signature, state weights, look target and speech state shall replicate; poses are computed on each client. | P1 | PH3 | proposed |
| ANIM-010 | The character designer shall offer a choice of walk style, and creators shall be able to publish walk styles as data (see UGC-002). | P2 | PH5 | proposed |
| ANIM-011 | The motion maths shall be engine-light and deterministic, with unit tests for foot error, knee range and speed (see TECH-004). | P1 | PH1 | proposed |

### 4.3 Needs — `NEED`

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| NEED-001 | Each character shall have needs: hunger, energy, hygiene, social, fun, health `[ASSUMPTION: final list]`. | P0 | PH1 | proposed |
| NEED-002 | Needs shall decay over game time at data-driven rates and be restored by interactions (eat, sleep, shower, talk…). | P0 | PH1 | proposed |
| NEED-003 | Low needs shall apply gameplay effects (slower movement, reduced work performance), never permanent character loss. | P0 | PH1 | proposed |
| NEED-004 | Need values shall be computed by the server; clients only display them. | P0 | PH2 | proposed |

### 4.4 Careers — `CAR`

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| CAR-001 | A character shall be able to hold a job with a schedule, workplace, wage and performance score. | P0 | PH1 | proposed |
| CAR-002 | Careers shall be data-defined tracks with ranks; promotion depends on performance and skills. | P0 | PH3 | proposed |
| CAR-003 | Work shall involve active tasks at the workplace, not only a timer. | P1 | PH3 | proposed |
| CAR-004 | A character shall be able to be self-employed by owning and running a shop (see PROP-005). | P1 | PH3 | proposed |
| CAR-005 | Wages shall be paid through the ledger (ECO-002) on a pay cycle. | P0 | PH1 | proposed |

### 4.5 Property and ownership — `PROP`

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| PROP-001 | Characters shall buy, own and sell items; ownership shall be recorded server-side. In the browser slice ownership is kept in the player's save (QST-008) until the server exists (PH2). | P0 | PH1 | in-progress |
| PROP-002 | Characters shall buy, sell and rent buildings of every kind (homes, shops, workplaces, utilities, civic buildings) and the units inside them, from a finite set of city lots. Moved to PH1 by the owner's pick of `F-PROP-ESTATE` level 2 for the first slice; detail in PROP-009 to PROP-021. | P0 | PH1 | in-progress |
| PROP-003 | Owners shall place, move and remove furniture inside their property; layout persists and is visible to visitors. Moved to PH1 by the owner's pick of `F-PROP-FURNISH` level 2 for the first slice (2026-11-22); detail in `docs/specs/domains/FURNITURE.md` (FURN-050 to FURN-057). | P0 | PH1 | proposed |
| PROP-004 | Owners shall control access (private, friends, public). | P1 | PH3 | proposed |
| PROP-005 | Characters shall buy or lease commercial units and run shops: stock, prices, opening hours, staff (NPC or player). | P1 | PH3 | proposed |
| PROP-006 | Property shall carry recurring costs (tax, utilities, upkeep) charged through the ledger. Moved to PH1 with PROP-002: upkeep and tax are a data share of the unit's value each game day (PROP-017); utilities wait for `F-BLD-UTILITY`. | P1 | PH1 | in-progress |
| PROP-008 | A player shall rent a stall in the market square for a game day, set out items they own at prices they choose, and residents who visit the market that day shall buy from it. Who buys what is calculated from their routine and the price against the item's usual price, not ticked; takings go through the ledger. `F-BLD-SHOP` level 2 (the slice's form of player-run shops, PROP-005). Asked for by the owner on 2026-11-22. | P1 | PH1 | in-progress |
| PROP-007 | The world shall define what happens to property of long-inactive players `[OPEN: Q-07]`. | P1 | PH4 | proposed |
| PROP-009 | Every building that is not public land (gardens, stops, the market square) shall be carved into units from its floors: a house is one unit; a tower gives a flat per floor and the rooms over a shop a flat each; a shop is one unit with its store; a workplace is an office unit per floor; utilities, the showroom and civic buildings are one unit each. Each unit has a stable id `<lot id>:u<n>`, its floors and its floor area. Rules in `World/rules/estate.json` (ADR-0022). `F-PROP-ESTATE` level 2. Asked for by the owner on 2026-11-22. | P0 | PH1 | in-progress |
| PROP-010 | Every unit shall have one owner: the town, one of several real estate companies (data; at least two), the household that lives there, the business that uses it, or a player; and maybe a tenant (a household, a business, an unseen lodger, or a player). Who owns what when the town begins is worked out from the unit's id and data weights by kind. No company starts with half the town. `F-PROP-ESTATE` level 2. | P0 | PH1 | in-progress |
| PROP-011 | A unit's value shall be floor area × the kind's price per m² × a location factor (distance to the town centre) × a floor factor (higher floors dearer with a lift, cheaper above the second without) × its block's price index, in whole chips. The player shall buy a single unit, or every unit of a building together at a discount, at its listed price or (unlisted) at the owner's offer price, paying a transfer tax; a purchase is refused whole if it cannot be paid. Anything but public land can be bought. `F-PROP-ESTATE` level 2. | P0 | PH1 | in-progress |
| PROP-012 | The player shall sell a unit at once to the company that bids most and can pay, or list it through an estate agent at a price within data bounds, to be bought on a later day by a company or the household living there; the agent takes a fee on sale. Owners in town list and buy among themselves on their own, so the town trades without the player. `F-PROP-ESTATE` level 2. | P0 | PH1 | in-progress |
| PROP-013 | The player shall rent an empty unit at its market rent, paying the first day at once and each later day at midnight through the ledger, and may move out; after three unpaid days the player is evicted. At most a data number of tenancies at once. `F-PROP-ESTATE` level 2. | P1 | PH1 | in-progress |
| PROP-014 | The player shall let units they own at a rent they set within data bounds of the market rent; rent from tenants is paid into their current account (cash without one). A household or business already living or working in a unit the player buys stays and pays no more than the market rent. `F-PROP-ESTATE` level 2. | P1 | PH1 | in-progress |
| PROP-015 | Empty units that are offered to let shall be taken by unseen lodgers, and lodgers leave in time; for a player's unit, rent above the market makes a lodger less likely to come and more likely to go. `F-PROP-ESTATE` level 2. | P1 | PH1 | in-progress |
| PROP-016 | Each block shall have a price index, worked out each day from a slow wave from the day, how full its units are, and the player's recent purchases there, kept inside data bounds; prices and rents follow it. A town index (value-weighted), the money supply and the day's sales shall be shown to the player. `F-PROP-ESTATE` level 2. | P1 | PH1 | in-progress |
| PROP-017 | Owners other than the town shall pay upkeep and tax each day as a data share of value, with an extra share per unit for a player who owns many (portfolio tax); after three unpaid days the town sells the player's unit to a company at a forced bid, keeps what is owed and a fee, and pays the rest to the player. A company that cannot pay its tax pays what it can and lists more to sell. `F-PROP-ESTATE` level 2. | P1 | PH1 | in-progress |
| PROP-018 | The real estate market shall keep the money supply steady: residents and businesses earn through `sys:wages` (wages by role; takings as a share of rent), spend what they hold beyond a few days of earnings (`sys:spend`), companies pay out what they hold above a cap (`sys:dividends`), tax and fees go to `sys:tax` and `sys:fees`. With no player, the money supply after a game year is within a data band of its level after the market settles. `F-PROP-ESTATE` level 2. | P0 | PH1 | in-progress |
| PROP-019 | The bank shall lend a mortgage on a purchase: at least a data deposit, a term from data, simple interest, equal daily instalments from the current account from the day after; three missed instalments and the bank sells the units at the forced bid, clears the debt from the price, pays the rest to the player and lends no more. Selling a mortgaged unit pays the mortgage off first. Part of BANK-002. `F-PROP-ESTATE` level 2. | P0 | PH1 | in-progress |
| PROP-020 | The market shall be balanced against what a player earns (`balance.player_day_income`, `[ASSUMPTION]` until measured in play): the cheapest home rents for at most a data share of a day's income, its deposit takes at most a few days of income, its mortgage instalment with upkeep and tax is at most a data share, buying to let pays back in a data range of days, and buying and selling straight back always loses money. `F-PROP-ESTATE` level 2. | P0 | PH1 | in-progress |
| PROP-021 | The market shall not be ticked: it is settled one game day at a time, in day order, at each day's end, from the day, the data and what the player did, so the same game gives the same market on every machine and after loading a save. The browser slice's port shall reproduce the rules' results exactly (self-test). `F-PROP-ESTATE` level 2. | P0 | PH1 | in-progress |
| PROP-022 | Residents shall move house: each day a renting household may move to an empty house or flat it can afford (a data share of its wages) that has a bed for each of them, sooner if its own rent is more than it can afford, and buys it if it is listed and the town can pay; its old home stands empty. Who lives where is kept with the market and saved with it. In the slice a resident who has moved does what they did at the old home at the new one, on its floors, and walks there along the streets; their car trips still start where the car was parked. Asked for by the owner on 2026-11-22. `F-PROP-ESTATE` level 2. | P1 | PH1 | in-progress |
| PROP-023 | Every building with a unit for sale or to let shall show it on a board over its door, with how many when it has more than one unit, and say so when the player stands there; the real estate companies' offices shall carry their names. Empty flats shall have beds and living space on their floors (`Tools/worldgen/add_estate_01.py`). Asked for by the owner on 2026-11-22. `F-PROP-ESTATE` level 2. | P1 | PH1 | in-progress |

### 4.6 Living city — `CITY`

The rows below are the top-level requirements. The detail (CITY-010 onward: lots, routines, loading,
interaction points, vehicles) is in `docs/specs/domains/LIVING_CITY.md`, proven in `Tools/citysim/`.

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| CITY-001 | Interactable objects shall share one interaction framework (prompt, validate on server, play result) so new object types are data plus a small behaviour. | P0 | PH1 | proposed |
| CITY-002 | The PH1 slice shall ship ≥10 interactable object types; each later phase shall add more, tracked in a content list. | P0 | PH1 | proposed |
| CITY-003 | NPC residents shall follow daily schedules (home, work, leisure) tied to the world clock. | P1 | PH3 | proposed |
| CITY-004 | NPCs shall staff shops and services and transact through the same ledger as players. | P1 | PH3 | proposed |
| CITY-005 | The city shall provide transport: walking, cars that players drive and residents' cars in real traffic from PH1 (Q-08), public transit by its own pick. Detail in LIVING_CITY §3.4 (CITY-040 to CITY-067). | P0 | PH1 | accepted |
| CITY-006 | Ambient crowd and traffic density shall scale with time of day and with a server performance budget. | P1 | PH3 | proposed |
| CITY-007 | Players shall hold dialogues with NPCs: lines shown as text and spoken aloud (Q-14, ADR-0019), player choices, and outcomes that can change game state. Lines carry an emotion tag used by CHR-008. | P1 | PH1 | accepted |

### 4.7 Time, weather, vegetation — `WLD`

Detail (WLD-010 onward: seasons, growth, leaf cycle, trees falling and being replanted, shared weather)
is in `docs/specs/domains/LIVING_CITY.md`.

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| WLD-001 | The server shall own a world clock; one game day = 60 real minutes `[ASSUMPTION]`, configurable. | P0 | PH1 | proposed |
| WLD-002 | Lighting shall follow the world clock (sun, moon, street and interior lights). | P0 | PH3 | proposed |
| WLD-003 | Weather shall be a server-driven state machine (clear, cloudy, rain, storm, fog…) with seasonal probabilities; all clients see the same weather. | P1 | PH3 | proposed |
| WLD-004 | Weather shall affect gameplay lightly (needs, NPC behaviour, shop demand). | P2 | PH3 | proposed |
| WLD-005 | Vegetation shall grow in stages. Growth state shall be **calculated from elapsed time and conditions**, not ticked per plant per frame, and updated in periodic batches. | P1 | PH3 | proposed |
| WLD-006 | Seasons shall change vegetation appearance and growth rates. | P2 | PH3 | proposed |
| WLD-007 | Players shall plant and tend vegetation on property they own. | P2 | PH3 | proposed |

### 4.8 Sound — `SND`

Scope is set by three features the owner picked for the first slice on 2026-11-22 ("1, 1, 1 for slice"):
`F-SND-WORLD` level 1, `F-SND-ACTION` level 1, `F-SND-MUSIC` level 1 (later milestones undecided). Tuning is data in
`World/rules/sound.json`; the browser slice makes every sound in code from it (`Prototype/play/sound.js`), so there
are no audio files to license. The requirements were drafted by an agent from that pick and are awaiting the owner's
wording check; they are `in-progress` because the owner asked for the work.

| ID | Requirement | Feature | Lvl | Pri | Phase | Status | Test |
|---|---|---|---|---|---|---|---|
| SND-001 | The street shall have a background sound that follows the time of day (busier by day, quieter at night), a room tone indoors, birds by day (more near gardens), crickets at night and running water by fountains. | F-SND-WORLD | 0 | P1 | PH1 | in-progress | `test_SND_008_sound_rules_are_sound` |
| SND-002 | Each nearby resident car on a trip shall make an engine sound from where it is, its pitch following its speed; vans sound lower. Parked cars are silent. | F-SND-WORLD | 1 | P1 | PH1 | in-progress | |
| SND-003 | The player and nearby walking residents shall make footsteps in time with their walk, sounding of stone outside, wood on floors and grass in gardens; a hard landing thuds. | F-SND-WORLD | 1 | P1 | PH1 | in-progress | `test_SND_008_sound_rules_are_sound` |
| SND-004 | The tram shall hum and rumble with its speed and ring its bell when it stops and when it leaves. | F-SND-WORLD | 1 | P1 | PH1 | in-progress | |
| SND-005 | A resident's line shall be voiced as wordless syllables, one for each mouth shape of the lip sync (CHR-009) and at the same moment, in a pitch that is the same for that resident every time, rising before a question. No words (Q-14: text first). *Cut 2026-11-22: lines are spoken (SND-018, ADR-0019).* | F-SND-WORLD | 1 | P1 | PH1 | cut | `test_SND_005_every_vowel_the_mouth_shows_has_a_voice` |
| SND-006 | Residents standing together at leisure or work shall murmur to each other; only the nearest groups are heard (4 `[ASSUMPTION]`). | F-SND-WORLD | 1 | P1 | PH1 | in-progress | |
| SND-007 | Every sound in the world shall come from where its source is: louder near, panned left or right by direction, gone past its distance. Outdoor sounds shall be muffled while the listener is inside a building. | F-SND-WORLD | 1 | P1 | PH1 | in-progress | `test_SND_007_outside_is_muffled_indoors` |
| SND-008 | Every sound's tuning (loudness, distance, pitch, times of day, music styles) shall be data, checked before use. | F-SND-WORLD | 0 | P1 | PH1 | in-progress | `test_SND_008_sound_rules_are_sound`, `test_SND_008_broken_sound_rules_are_caught` |
| SND-009 | Actions shall have sounds: the interface (a prompt appearing, opening, answering and leaving a conversation, the map, the clock), every interaction (sit, lie down, get up, ride a lift, jump) and the player's car (door, engine start, engine with speed, horn, skid, bump). | F-SND-ACTION | 1 | P1 | PH1 | in-progress | `test_SND_009_every_action_has_a_sound` |
| SND-010 | A soft score shall play that changes with the time of day (morning, afternoon, evening, night); it shall fade under a conversation and under any nearer music. | F-SND-MUSIC | 0 | P1 | PH1 | in-progress | `test_SND_010_the_score_changes_through_the_day` |
| SND-011 | Open cafés and shops shall play their own music, heard muffled from the street and clear inside; a busker shall play in the market on market days. Opening hours for music are data until lots carry hours `[ASSUMPTION]`. | F-SND-MUSIC | 1 | P1 | PH1 | in-progress | `test_SND_011_cafes_shops_and_the_market_play_music` |
| SND-012 | A car shall have a radio with at least three stations, switched while driving. | F-SND-MUSIC | 1 | P1 | PH1 | in-progress | `test_SND_012_the_radio_has_three_stations` |
| SND-013 | The player shall be able to turn all sound off and on with one key, remembered between visits. | F-SND-ACTION | 0 | P1 | PH1 | in-progress | |
| SND-014 | The car radio shall also carry podcasts: shows with hosts who talk in wordless syllables in their own voices, each line shown as a caption, over quiet music. Where a show is shall follow the world clock, so everyone tuned in hears the same line. The shows are data. Asked for by the owner on 2026-11-22. | F-SND-MUSIC | 1 | P1 | PH1 | in-progress | `test_SND_014_the_radio_has_podcasts`, `test_SND_014_broken_podcasts_are_caught` |
| SND-015 | What residents are doing shall be heard near them, from data: cutlery and cups where people eat, the espresso machine and till at work, bags in shops, coins and calls at a stall, typing, hammering, shears, deliveries, pages turned, snoring in bed, children on the swings, slide, seesaw, sandpit and climbing frame. | F-SND-WORLD | 1 | P1 | PH1 | in-progress | `test_SND_015_what_people_do_makes_a_sound`, `test_SND_015_broken_people_sounds_are_caught` |
| SND-016 | Residents together shall talk in turns: one speaks a wordless phrase that falls to an end or rises to a question, another replies, then a pause; now and then someone laughs (children more often). Where many are gathered a crowd hum shall rise. Quiet places (library, museum, cinema, bedrooms) have no chatter. A passer-by sometimes says hello to the player; a resident who dodges the player's car shouts (CITY-071). | F-SND-WORLD | 1 | P1 | PH1 | in-progress | `test_SND_016_talk_is_turns_and_quiet_places_stay_quiet` |
| SND-017 | People's movements shall be heard: steps louder when running and lighter and higher for children, on stone, wood or grass; a sound when a resident sits, lies down or gets up. Children's voices are higher, elders' lower. | F-SND-WORLD | 1 | P1 | PH1 | in-progress | `test_SND_017_children_sound_lighter` |
| SND-018 | Residents and the player shall speak their dialogue lines aloud in natural voices (ADR-0019), with the text still shown. Each resident speaks in a voice from their role's pool in `World/rules/voices.json`, always the same one; the player in the player voice. Each line is spoken in its mood. Blanks are joined from pieces voiced for every value the speaker can use; a typed player name other than the default is a short pause. A resident's next line waits for the player's spoken answer. | F-NPC-TALK | 1 | P1 | PH1 | in-progress | `test_SND_018_every_role_has_voices`, `test_SND_018_every_line_can_be_spoken`, `test_SND_018_the_owners_picks_are_kept` |
| SND-019 | When a quest or a job pays the player, coins shall spring out around the character, lie a moment, be drawn back in and fly to the money shown on screen, which counts up as they land; a short fanfare shall play, each coin taken shall ring one note higher, and the last shall ring a chord. A bigger reward springs more coins (tiers in data). Which ledger transactions count is worked out from the ledger (paid from the rewards account, QST-004); nothing but the ledger moves money. Chosen by the owner from `Prototype/play/feedback_samples.html` on 2026-11-24 (sample Q1). ADR-0043. | F-UI-REWARD | 1 | P1 | PH1 | in-progress | `test_SND_019_a_reward_springs_coins_by_its_size`, `test_SND_019_broken_reward_rules_are_caught`, `test_SND_019_a_quest_reward_is_what_the_player_is_shown` |
| SND-020 | When the bank pays interest (BANK-001, BANK-008) a slip shall slide out by the money shown on screen and pay coins into the bank balance, with a drawer thud, a tick for each coin and a bell as the balance glows. Several accounts, or several days settled at once, are shown once for the whole amount. Chosen by the owner on 2026-11-24 (sample I1). ADR-0043. | F-UI-REWARD | 1 | P1 | PH1 | in-progress | `test_SND_020_interest_is_shown_once_for_all_accounts`, `test_SND_020_the_banks_interest_is_what_the_player_is_shown` |
| SND-021 | When an achievement is earned (QST-009) a painted banner with its name shall drop in at the top of the screen with sparkles and a fanfare (a harp sweep, a wide bell chord, three twinkles), stay a few seconds (data) and leave. Several earned together are shown one after another; none is shown while a menu covers the screen, it waits. Chosen by the owner on 2026-11-24 (sample A1). ADR-0043. | F-UI-REWARD | 2 | P1 | PH1 | in-progress | `test_SND_019_broken_reward_rules_are_caught` |

### 4.9 Quests — `QST`

From the owner's pick of `F-QUEST` level 2 for the first slice (2026-11-22). Rules proved in
`Tools/citysim/economy.py` and `Tools/citysim/quests.py` (tests in `test_economy.py`); data in
`World/rules/quests.json`; built in the browser slice (`Prototype/play/quests.js`). Design: ADR-0012.

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| QST-001 | Quest lines shall be data: who gives them, what must be done first, stages with a goal (talk to someone, go somewhere, have an item, a bank or stall condition), the lines said at each stage, and endings with their rewards. They are checked before use: every stage reachable, every stage able to finish, every person, place and item real. | P0 | PH1 | in-progress |
| QST-002 | A new game shall start with the character designer (CHR-001), then the first starter quest line, which leads the player to the bank, a shop and a resident. At least four starter quest lines shall exist; finishing one opens the next ones. | P0 | PH1 | in-progress |
| QST-003 | Every starter quest line shall have at least one choice that leads to a different ending, with different rewards and reputation. | P1 | PH1 | in-progress |
| QST-004 | Quest and job rewards shall be paid through the ledger from the named rewards account (ECO-003); items given or taken are recorded as owned (PROP-001). | P0 | PH1 | in-progress |
| QST-005 | A quest log shall show each active quest line's current stage, where the person to see is right now (from their routine) and the finished lines with their endings; the people to see are marked on the maps. | P1 | PH1 | in-progress |
| QST-006 | Residents shall offer repeatable daily jobs (deliveries, shopping, a shift at their work). Which resident offers which job on which day, and what it pays, shall be calculated from the day and the data, not stored. One job at a time; a job has a time limit and failing it costs reputation. | P1 | PH1 | in-progress |
| QST-007 | The player shall have a reputation with each resident from −100 to 100, changed by quest endings and jobs. It changes that resident's shop prices (within data bounds) and whether they offer jobs. | P1 | PH1 | in-progress |
| QST-008 | The character, money, items, quest progress and reputation shall survive closing and reopening the slice, and a new game shall start clean. In the slice this is a save in the browser; from PH2 it is server-side (CORE-003, ACC-003). | P0 | PH1 | in-progress |
| QST-009 | The player shall earn named achievements, each once and kept. Achievements are data: a name, a line saying how it is earned, and a fact about the player with the number it must reach. The facts (quests finished, jobs paid, savings, loans paid back, stall sales, homes rented and owned, money in all) shall be worked out from the ledger, the quests, the loans and the property market, not counted up as the game runs; only which were earned and on which day is kept, with the save (QST-008). What is earned stays earned if the fact later falls. `F-UI-REWARD` level 2, picked by the owner on 2026-11-24; names and numbers `[ASSUMPTION]`. | P1 | PH1 | in-progress |

---

## 5. Economy — `ECO`, `BANK`

The economy is the system most likely to break the game if built loosely, so it is specified strictly.

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| ECO-001 | The game shall have one in-game currency, the **brix** (symbol `Bx`, plural "brix"), divided into 100 **chips**. All amounts shall be stored as integer chips; no floating-point money. | P0 | PH1 | in-progress |
| ECO-002 | Every currency movement shall be a **double-entry ledger transaction**: debits equal credits, written atomically, never edited, only reversed by a new transaction. | P0 | PH1 | in-progress |
| ECO-003 | Currency shall only be created or destroyed by named system accounts (wages source, tax sink, central bank…); total supply shall be computable at any moment. | P0 | PH1 | in-progress |
| ECO-004 | All faucets (money in) and sinks (money out) shall be listed in data and reported per game day. | P0 | PH4 | proposed |
| ECO-005 | Prices of goods shall respond to supply and demand within data-defined bounds. In the slice a shop's price follows its stock (ECO-010); moved to PH1 by the owner's pick of `F-BLD-SHOP` level 2. | P1 | PH1 | in-progress |
| ECO-006 | The simulation shall publish indices: price index (inflation), employment, money supply, average wage. | P1 | PH4 | proposed |
| ECO-007 | The economy simulation shall run headless, without the game client, at accelerated time for testing and tuning. | P0 | PH4 | proposed |
| BANK-001 | Banks shall offer current and savings accounts; deposits earn interest accrued on a fixed cycle. Moved to PH1 by the owner's pick of `F-ECO-BANK` level 2; rates are fixed data until the central bank (BANK-003) exists `[ASSUMPTION]`. | P0 | PH1 | in-progress |
| BANK-002 | Banks shall offer loans and mortgages with principal, rate, term, schedule, and defined default consequences (fees, repossession). Loans moved to PH1 with BANK-001; mortgages with property (PROP-019). | P0 | PH1 | in-progress |
| BANK-003 | A simulated central bank shall set a policy rate on a fixed cycle using a published rule (inflation and activity vs. targets). | P0 | PH4 | proposed |
| BANK-004 | Deposit and loan rates shall derive from the policy rate plus data-defined spreads; rate changes shall be announced in-game before they apply. | P1 | PH4 | proposed |
| BANK-005 | Characters shall have a credit score affecting loan eligibility and rate. | P2 | PH4 | proposed |
| BANK-006 | Player-to-player transfers shall be supported with limits and logging to deter abuse. | P1 | PH4 | proposed |
| ECO-009 | The player shall see their cash, each account's balance and a statement of every ledger line that touched them, newest first. `F-ECO-BANK` level 1. Asked for by the owner on 2026-11-22. | P1 | PH1 | in-progress |
| ECO-010 | A shop's stock of each item shall be calculated from its restock time, the residents' shop visits in their routine and the player's own trades since then; it is not ticked. Price = usual price × a factor that rises as stock falls, inside data bounds, rounded to whole chips. `F-BLD-SHOP` level 2. Asked for by the owner on 2026-11-22. | P1 | PH1 | in-progress |
| ECO-011 | The player shall sell items to a shop that deals in them, at a data-set share of its current price, paid from the shop's till account; a shop whose till cannot pay buys nothing. `F-BLD-SHOP` level 2. Asked for by the owner on 2026-11-22. | P0 | PH1 | in-progress |
| ECO-012 | Every economy number (starting money, prices, stock, rates, fees, terms, rewards) shall be data in `World/rules/economy.json` and `World/rules/quests.json`, checked before use. Asked for by the owner on 2026-11-22. | P0 | PH1 | in-progress |
| BANK-007 | At the bank the player shall open accounts and move money between cash, current and savings; a payment that would take an account below zero is refused, never partly made. `F-ECO-BANK` level 2. Asked for by the owner on 2026-11-22. | P0 | PH1 | in-progress |
| BANK-008 | Interest, loan repayments, late fees and default for every game day that has passed shall be worked out in day order when the game next looks at the accounts, each as its own ledger transaction dated at the day's end; nothing is ticked (TECH-004). On default the bank repossesses the player's items, valued at their sell price toward the debt, and refuses new loans. `F-ECO-BANK` level 2. Asked for by the owner on 2026-11-22. | P0 | PH1 | in-progress |
| ECO-013 | In a mall whose inside is built the player shall buy at each outlet, not at the mall as one shop: an outlet sells a data number of the goods its kind deals in, chosen for that outlet by a fixed hash, holds its own stock of each (worked out from its restock time, the residents' visits to the mall and the player's purchases there; not ticked), and asks a price that follows that stock as a street shop's does (ECO-005, ECO-010). A purchase is one ledger transaction into the mall's till that names the outlet; it is refused whole if it cannot be paid, if the outlet does not sell the good or if it is sold out. Outlets buy nothing back. `F-BLD-SHOP` level 2, `F-BLD-MALL` level 2. Asked for by the owner on 2026-11-24 (ADR-0041). | P0 | PH1 | in-progress |
| ECO-014 | What the outlets sell shall be data (`World/rules/mall_goods.json`), checked before use: for every kind of outlet a range of goods, each with a name, a price in whole chips and where it goes when bought: carried; eaten (food and drink, carried as a cafe's is until needs exist); for the home (a piece of `furnishing.json` that goes to the belongings and is placed from the Furnish tab, FURN-064); or to wear (kept for the wardrobe, CHR-016). `F-BLD-SHOP` level 2. Asked for by the owner on 2026-11-24 (ADR-0041). | P1 | PH1 | in-progress |

---

## 6. Multiplayer and accounts — `MP`, `ACC`

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| MP-001 | Each city shall run on a **dedicated, authoritative server**; clients never decide outcomes involving money, ownership or needs. | P0 | PH2 | proposed |
| MP-002 | Each city shall have two caps: **residents** (characters registered to the city) and **concurrent** (online at once). | P0 | PH2 | proposed |
| MP-003 | Caps shall be configuration values set from measured performance, not constants in code. Starting targets `[ASSUMPTION]`: PH2 = 16 concurrent; PH6 = 64 concurrent, stretch 100; residents = 10 × concurrent. | P0 | PH2 | proposed |
| MP-004 | The server shall hold ≥30 ticks/s at the concurrent cap; a cap is only raised when an automated load test passes at the new value. | P0 | PH2 | proposed |
| MP-005 | Players shall join a city from a city browser showing name, population and free slots; a full city shall offer a queue. | P1 | PH2 | proposed |
| MP-006 | Only actors relevant to a player (by distance and interest) shall replicate to that player. | P0 | PH2 | proposed |
| MP-007 | Players shall communicate by proximity text chat; voice is `[OPEN: Q-09]`. | P1 | PH2 | proposed |
| MP-008 | A disconnect shall not lose more than 60 s of character progress `[ASSUMPTION]`. | P0 | PH2 | proposed |
| ACC-001 | Players shall register and sign in to an account; one account may own multiple characters, one character per city `[ASSUMPTION]`. | P0 | PH2 | proposed |
| ACC-002 | Player accounts and creator accounts shall be separate roles on the same identity, with creator status requiring extra registration (UGC-001). | P0 | PH5 | proposed |
| ACC-003 | Persistent data (accounts, ledger, ownership) shall live in backend services and a database, not in game-server memory or save files. | P0 | PH2 | proposed |

---

## 7. Creator platform — `UGC`

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| UGC-001 | Creators shall register through a separate flow (outside the game client) capturing identity and agreement to creator terms. | P0 | PH5 | proposed |
| UGC-002 | Supported content types at launch of PH5: clothing, furniture, house models, character styles (hair, faces, presets). | P0 | PH5 | proposed |
| UGC-003 | Each content type shall have a published **technical spec** (polygon and texture budgets, skeleton, naming, pivot, collision, LODs) and a Blender template file. | P0 | PH5 | proposed |
| UGC-004 | Submissions shall pass automated validation against the technical spec before any human review. | P0 | PH5 | proposed |
| UGC-005 | Submissions shall pass moderation (automated plus human) for prohibited and infringing content before listing. | P0 | PH5 | proposed |
| UGC-006 | Approved content shall be delivered to clients on demand, without a full game patch. | P0 | PH5 | proposed |
| UGC-007 | Creators shall set a price in game currency within bounds; each sale shall split between creator and a platform sink through the ledger. | P0 | PH5 | proposed |
| UGC-008 | Creators shall see a dashboard: items, sales, earnings, review status. | P1 | PH5 | proposed |
| UGC-009 | Listed content shall be removable (takedown) with a defined outcome for players who already own it. | P0 | PH5 | proposed |
| UGC-010 | Creator earnings shall be paid in brix only; there is no conversion to real money (Q-04, decided). | P0 | PH5 | proposed |

**Design-ahead note:** first-party content from PH1 onward shall be built through the same technical
specs and validation tools that creators will later use, so the UGC pipeline is proven before it opens.

---

## 8. Interface — `UI`

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| UI-001 | HUD shall show needs, money, clock, weather and current objective, and be hideable. | P0 | PH1 | proposed |
| UI-002 | An in-game phone shall be the hub for banking, jobs, property, messages, map and store. | P1 | PH3 | proposed |
| UI-003 | All player-facing text shall come from localisation tables; English first. | P1 | PH1 | proposed |
| UI-004 | The game shall be fully playable with keyboard/mouse and with a gamepad. | P1 | PH3 | proposed |

---

## 9. Technical architecture — `TECH`, `SEC`

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| TECH-001 | Engine: Unreal Engine 5.8, one pinned version for the whole team; upgrades are a planned task with an ADR. | P0 | PH0 | accepted |
| TECH-002 | Gameplay systems shall be C++ modules with data assets for tuning; Blueprints for content wiring and UI only. | P0 | PH0 | accepted |
| TECH-003 | Each gameplay domain (Needs, Careers, Economy, World…) shall be its own module or plugin with a public interface and no circular dependencies. | P0 | PH0 | accepted |
| TECH-004 | Simulation logic (economy, needs math, vegetation growth, weather) shall be engine-light, deterministic given a seed and clock, and runnable in headless tests. | P0 | PH1 | proposed |
| TECH-005 | Tunable values shall live in data (data assets, tables, config), never as literals in code. | P0 | PH1 | proposed |
| TECH-006 | The city shall use world streaming (World Partition) so memory and replication cost scale with nearby content, not city size. | P0 | PH1 | proposed |
| TECH-007 | Backend services (accounts, ledger, ownership, UGC catalogue) shall be separate from game servers, with a versioned API. Stack is `[OPEN: Q-02]`. | P0 | PH2 | proposed |
| TECH-008 | Performance budgets `[ASSUMPTION]`: client 60 fps at 1080p on the defined minimum PC; server ≥30 ticks/s at cap. Minimum PC is `[OPEN: Q-05]`. | P0 | PH1 | proposed |
| TECH-009 | Server and client shall emit structured logs and metrics (tick time, replication cost, player count, ledger totals). | P1 | PH2 | proposed |
| SEC-001 | The server shall validate every client request (range, ownership, rate) and reject impossible ones. | P0 | PH2 | proposed |
| SEC-002 | Credentials, tokens and keys shall never be committed to the repository. | P0 | PH0 | proposed |
| SEC-003 | Chat and UGC shall have report and block tools and a moderation queue. | P0 | PH5 | proposed |
| SEC-004 | Personal data handling shall follow applicable law for the launch regions. | P0 | PH5 | proposed |

---

## 10. Code and asset management — `ASSET`

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| ASSET-001 | All code, engine assets and DCC source files shall be under version control with large-file support and **file locking for binary assets**: Git with Git LFS, binaries lockable (ADR-0002). | P0 | PH0 | accepted |
| ASSET-002 | Blender source files live in `Art/Source/`; exported files in `Art/Export/`; imported engine assets in `Game/Content/`. Each engine asset is traceable to its source file. | P0 | PH0 | proposed |
| ASSET-003 | Naming shall follow `docs/standards/` (prefix by type: `SM_`, `SK_`, `M_`, `MI_`, `T_`, `BP_`, `DA_`, `WBP_`…). | P0 | PH0 | proposed |
| ASSET-004 | Blender-to-Unreal export shall be a scripted, repeatable step (scale, axes, naming, LODs), not manual settings. | P1 | PH1 | proposed |
| ASSET-005 | Automated asset validation shall run in CI: naming, texture size and power-of-two, polygon budget, missing LODs/collision, broken references. | P0 | PH1 | proposed |
| ASSET-006 | The main branch shall always build and pass tests; work lands through reviewed changes. | P0 | PH0 | proposed |
| ASSET-007 | Third-party and marketplace assets shall be recorded with source and licence in `docs/licenses/`. | P0 | PH0 | proposed |

---

## 11. Test harness — `TEST`

| Layer | What it covers | Tooling (Unreal) | Runs |
|---|---|---|---|
| L1 Unit | Pure logic: ledger math, interest, needs decay, growth curves | Low-Level Tests / Automation Spec | Every commit |
| L2 Integration | A system inside the engine: interaction framework, save/load, replication of one actor | Automation framework, functional test maps | Every commit |
| L3 Multiplayer | Server + N clients: join, trade, disconnect/reconnect | Gauntlet with dedicated server and bot clients | Nightly |
| L4 Simulation soak | Economy run for simulated years; invariants hold | Headless simulation runner | Nightly |
| L5 Load/performance | Server tick and client frame time at player cap | Gauntlet + bots + perf capture | Weekly and before cap changes |
| L6 Asset validation | ASSET-005 rules | Data Validation plugin + scripts | Every commit touching assets |
| L7 Backend | API contracts, database migrations | Backend stack's test framework | Every commit |

| ID | Requirement | Pri | Phase | Status |
|---|---|---|---|---|
| TEST-001 | Every `P0` requirement shall have at least one automated test that names its ID. | P0 | all | proposed |
| TEST-002 | CI shall build editor, client and server targets and run L1, L2, L6 on every change. | P0 | PH0 | proposed |
| TEST-003 | Economy invariants shall be asserted in L4: ledger balances to zero; supply equals created minus destroyed; no negative balances without a loan. | P0 | PH4 | proposed |
| TEST-004 | Bot clients shall be able to perform the core loop unattended for load and soak tests. | P0 | PH2 | proposed |
| TEST-005 | A traceability report (requirement → tests → last result) shall be generated by CI. | P1 | PH1 | proposed |
| TEST-006 | Performance results shall be stored over time and a regression beyond a set threshold shall fail the build. | P1 | PH2 | proposed |

---

## 12. Decisions log

| ADR | Decision | Status |
|---|---|---|
| ADR-0001 | Engine and DCC versions: UE 5.8, Blender 5.2 LTS (Blender pin under review, Q-16) | proposed |
| ADR-0002 | Version control: Git with Git LFS, Unreal and Blender binaries lockable (Q-01) | accepted |
| ADR-0003 | Backend stack (see Q-02) | open |
| ADR-0004 | Art direction: old-town world, option B "Soft" characters, no painterly filter | accepted |
| ADR-0005 | One shared face control set; lip sync from mouth shapes | accepted |
| ADR-0006 | Body motion: one skeleton, movement signatures, independent layers (Q-17 still open) | accepted |
| ADR-0007 | Daylight: the softer, warmer "kit" light replaces the reference light as the default (amends ADR-0004) | accepted |
| ADR-0012 | Money, shops, quests and the designer in the slice: one ledger, settled by the day; stock, stall sales and daily jobs calculated from the clock and routines | proposed |
| ADR-0041 | Buying at each outlet of the mall; what the outlets sell and where each good goes (home, wardrobe, carried); the goods as objects made in Blender; a keeper in every outlet, guards and janitors for the mall; residents who visit it | accepted |

ADR-0001 to ADR-0003 are reserved numbers; their files are written when the decision is taken.

---

## 13. Open questions

| ID | Question | Why it matters | Default if unanswered |
|---|---|---|---|
| Q-01 | Git + Git LFS, or Perforce? | Unreal assets are large binaries that cannot be merged. | **DECIDED 2026-11-24:** Git + Git LFS with locking (owner; ADR-0002). Revisit when a second artist joins. |
| Q-02 | Backend stack and hosting? | Shapes accounts, ledger, UGC delivery. | Decide at start of PH2; PostgreSQL for the ledger. |
| Q-03 | Currency name? | Flavour. | **DECIDED 2026-11-21:** brix (`Bx`), 100 chips = 1 brix. Starting amounts are a balance task for PH1. |
| Q-04 | Real money: is currency purchasable? can creators cash out? | Triggers payments, tax, consumer and gambling law. | **DECIDED 2026-11-21:** in-game currency only; no real-money purchase or cash-out. |
| Q-05 | Minimum and target PC hardware? | Sets every performance budget. | Define after first PH1 profiling. |
| Q-06 | What happens to a character while its player is offline? | Fairness, need decay, burglary/griefing. | Character is safe and frozen; property and accounts keep running. |
| Q-07 | Inactive-player property policy? | Lots are finite. | After N days unpaid costs, property returns to market. |
| Q-08 | Vehicles: in scope, and when? | Big cost in physics, networking, city layout. | **DECIDED 2026-11-22:** drivable cars from the first slice (PH1) onward, with real traffic in which residents drive their own cars as part of their routine (`F-VEH-DRIVE` 2, `F-VEH-TRAFFIC` 2 at every milestone). Car requirements still to be written. |
| Q-09 | Voice chat? | Moderation cost. | Text only. |
| Q-10 | Art style: realistic or stylised? | Drives character designer, UGC budgets, hardware needs. | **DECIDED 2026-11-21.** World: the old-town look of `Art/StyleTests/old_town_04_world.png` (warm sandstone blocks and mottled plaster, arches, lantern-lit shade, low sun through haze, muted palette with dull accents, same place readable from morning to dusk). No painterly post-process. Characters: option B "Soft" of `Art/StyleTests/old_town_04_characters.png` (about 5.6 heads tall, larger head and eyes, whole-garment clothing in the world's palette). Production models still to be made (Q-15). |
| Q-11 | Team: solo, or others joining? | Affects tooling and process weight. | Solo plus AI agents. |
| Q-12 | Age rating and audience? | Constrains content, chat, UGC moderation. | Teen. |
| Q-13 | What does "Wasted" mean for tone and theme? | The name suggested a tone the pillars did not reflect. | **DECIDED 2026-11-22:** the game is called **Brix Town** (owner), after its currency, the brix (ADR-0025). "Wasted" was a working title. |
| Q-14 | Is dialogue voiced (recorded or synthesised speech) or text only? Do player characters' mouths move with chat? | Decides how lip sync is timed: from audio, or generated from the text of the line. | **DECIDED 2026-11-22:** voiced (ADR-0019). Residents and the player speak their lines in natural voices, pre-made offline with the Kokoro model; the text stays on screen. Mouth shapes still come from the line's text, stretched to the length of its audio. (Earlier the same day: text first.) Whether players' mouths move with chat is still open. |
| Q-15 | How are production character models made: commissioned or bought stylised base, modelled in-house, or MetaHuman? | Sets cost, schedule and whether the laptop can run them. | Commission or buy one stylised base body and head with the CHR-007 control set; build the designer on it. |
| Q-16 | Blender version pin: 5.2 LTS as first written, or 5.3 as installed? | All look tests and reference renders were made in 5.3 (the Microsoft Store version, which auto-updates). | Pin 5.3; record the exact build; revisit when a second machine joins. |
| Q-17 | Is the walk cycle fully procedural, or a few authored clips that the movement signature adjusts? | Decides how much animation is bought or made, and how the prototype maps to Unreal. | Authored base walk clips plus signature and state layers on top; head, eyes and face fully procedural. |
| Q-18 to Q-20 | Engine systems for the city controllers; how long a resident may keep a player waiting; length of the game year. | Asked and tracked in `docs/specs/domains/LIVING_CITY.md` §6. | See there. |

---

## 14. Glossary

| Term | Meaning |
|---|---|
| City / World | One persistent server instance with its own residents, property and economy. |
| Resident | A character registered to a city. |
| Brix / Chip | The in-game currency and its minor unit (100 chips = 1 brix). Written `Bx 1,200` or `Bx 4.50`. |
| Ledger | The append-only record of all currency movements. |
| Faucet / Sink | A source of new currency / a place currency is removed. |
| UGC | User-generated content made by registered creators. |
| DCC | Digital content creation tool (Blender). |
| ADR | Architecture Decision Record. |
| Face control set | The small set of named controls (brows, lids, gaze, mouth shape, corners) shared by every head. |
| Mouth shape (viseme) | One of the eight mouth positions that speech is built from. |
| Movement signature | The set of numbers that shapes how one character walks. |
| State layer | A data-defined change to a movement signature caused by needs, mood or weather. |

## 15. Change log

| Version | Date | Change |
|---|---|---|
| 0.1.0 | 2026-11-21 | First structured draft from owner's goals. |
| 0.1.1 | 2026-11-21 | Currency named brix/chip (Q-03). In-game currency only, no real money (Q-04). |
| 0.1.2 | 2026-11-21 | Art direction chosen from Blender style test 02, unfiltered render (Q-10). |
| 0.2.0 | 2026-11-21 | World look and character option B locked (Q-10). Added facial animation and lip sync (CHR-007 to CHR-011), NPC dialogue (CITY-007), and open questions Q-14, Q-15. |
| 0.3.0 | 2026-11-21 | Art direction recorded as requirements (ART-001 to ART-006) with companion documents and `Art/Reference/`. Face requirements accepted; CHR-012 added (teeth, neutral rest face). Body motion section added (ANIM-001 to ANIM-011, proposed). ADR-0004 to ADR-0006. Open questions Q-16, Q-17. |
| 0.3.1 | 2026-11-21 | Owner approved the body-motion prototype. ADR-0006 accepted; ANIM-001, 002, 004, 005, 006 accepted (shown in the clips); ANIM-003 and ANIM-007 to 011 stay proposed. Clips added to `Art/Reference/`. |
| 0.4.0 | 2026-11-21 | Scope is now set by feature levels picked by the owner (`docs/backlog/`, `Tools/feature_picker.html`, `docs/process/FEATURE_PROCESS.md`). Living-city domain spec added (CITY-010 to CITY-042, WLD-010 to WLD-014) with the city-in-a-box prototype and tests. Open questions Q-18 to Q-20. |
| 0.4.1 | 2026-11-22 | Daylight changed to the "kit" light (ADR-0007); ART-001 names `world_street_kit_light.png` as the daylight reference. |
| 0.4.2 | 2026-11-22 | Feature picker removed: the owner states picks in chat and agents record them, checked by `Tools/backlog.ps1` (ADR-0008). Q-08 decided: drivable cars and real traffic from the first slice. PH1 is now one district, not one block. CITY-005 updated to match. Car and traffic requirements drafted in LIVING_CITY 0.2.0 (CITY-043 to CITY-049, CITY-060 to CITY-067), accepted by the owner with ADR-0009 and Q-25 to Q-28; CITY-005 accepted. |
| 0.4.3 | 2026-11-22 | CITY-007 (dialogue with NPCs) accepted by the owner and moved to PH1, matching the owner's pick of F-NPC-TALK level 1 for the first slice. The browser slice has sample conversations (`World/rules/dialogue.json`, WORLD_MAP 0.5.3); outcomes that change game state are still to come. |
| 0.4.4 | 2026-11-22 | Owner picked `F-NPC-SPEECH` level 2 (lip sync) for the first slice: CHR-008, CHR-009 and ANIM-006 move to PH1, and Q-14 is decided as text first. The slice's faces take the CHR-007 control set for this; the designer part of CHR-007 stays in PH3. |
| 0.4.5 | 2026-11-22 | Sound (§4.8, SND-001 to SND-013) from the owner's pick of `F-SND-WORLD`, `F-SND-ACTION` and `F-SND-MUSIC` at level 1 for the first slice. Built in the browser slice from `World/rules/sound.json`. |
| 0.4.6 | 2026-11-22 | SND-014: podcasts on the car radio (owner's request), as part of `F-SND-MUSIC` level 1. |
| 0.5.0 | 2026-11-22 | Money, shops, quests and the character designer in the first slice, from the owner's request and picks (`F-ECO-BANK` 2, `F-BLD-SHOP` 2, `F-QUEST` 2, `F-CHR-DESIGN` 2). ECO-001 to 003 in progress; ECO-005, BANK-001, BANK-002, CHR-001 and CHR-002 moved to PH1. Added ECO-009 to 012, BANK-007, BANK-008, PROP-008, CHR-013 and §4.9 Quests (QST-001 to 008). ADR-0012. |
| 0.5.1 | 2026-11-22 | SND-015 to SND-017: people's movements, activities and chatter (owner's request), as part of `F-SND-WORLD` level 1. SND-006's murmur becomes conversations that take turns. |
| 0.5.2 | 2026-11-22 | Q-14 decided: dialogue is voiced (ADR-0019). SND-018 added; SND-005 cut; CHR-009 and CITY-007 updated to match. |
| 0.5.3 | 2026-11-22 | Real estate in the first slice, from the owner's request and pick of `F-PROP-ESTATE` level 2: PROP-002 and PROP-006 moved to PH1; PROP-009 to PROP-021 added (units, owners, several real estate companies, buying, selling, renting, letting, block price indices, tax and seizure, mortgages, balance), PROP-022 proposed (residents moving house). ADR-0022. |
| 0.5.4 | 2026-11-22 | Furniture in the first slice, from the owner's request and picks (`F-FURN-ITEMS` 2, `F-FURN-STORE` 2, `F-PROP-FURNISH` 2, `F-OBJ-CARRY` 1): new domain spec `docs/specs/domains/FURNITURE.md` (FURN-001 onward, all proposed; Q-30 to Q-32), new prefix `FURN`; PROP-003 moved to PH1. Interior style drafted for approval in `docs/art/INTERIOR_STYLE.md`. |
| 0.5.5 | 2026-11-22 | The game is called **Brix Town** (owner); "Wasted" was a working title. Q-13 decided. ADR-0025. |
| 0.5.6 | 2026-11-22 | Real estate, the missing pieces (owner's request): residents move house (PROP-022, now PH1 in progress), For Sale and To Let boards and the agents' names on their offices (PROP-023), beds in empty flats. |
| 0.5.7 | 2026-11-24 | Reward feedback (owner's request, samples Q1 and I1 chosen in chat): SND-019 coins and bells when a quest or job pays, SND-020 the bank's interest slip. New feature `F-UI-REWARD`, picked at level 2 for the first slice ("Level 2, achievements"): QST-009 achievements earned from facts and kept, SND-021 their banner and fanfare (sample A1), UI-019 the list on the Me tab. ADR-0043. |
| 0.5.8 | 2026-11-24 | Buying at each outlet of Eastgate Mall (owner's request; ADR-0041): ECO-013 (each outlet its own goods, stock and price, paid through the ledger) and ECO-014 (the outlets' catalogue as data, and where each good goes: carried, eaten, for the home, to wear). Detail in the domain specs: WLD-066, FURN-064, CHR-016. No level was picked for this; F-CHR-WARDROBE, F-INV-ITEMS and F-NPC-POP stay at 1. |
| 0.5.9 | 2026-11-24 | Staff for Eastgate Mall (owner's request; ADR-0041): one keeper in every outlet, six guards and six janitors for the whole mall. Detail in the domain specs: CITY-105 (LIVING_CITY), WLD-067 (WORLD_MAP). Careers were dealt again: 636 residents have another workplace. No level was picked for this. |
| 0.5.10 | 2026-11-24 | The mall's goods for the home are objects made in Blender (owner's request; ADR-0041): FURN-065 in FURNITURE. A placed guitar, bookstack or television is drawn from its object, and each outlet shows the goods it sells. No level was picked for this. |
| 0.5.11 | 2026-11-24 | Residents visit Eastgate Mall to shop and eat, and its staff eat there (owner's request; ADR-0041): CITY-106 in LIVING_CITY, WLD-068 in WORLD_MAP. No resident was added and no level was picked. |
| 0.5.12 | 2026-11-24 | Buy menus show what is for sale as objects in a grid, the chosen one large beside it, small cells in a narrow panel (owner's request and choice of look test 33; ADR-0044): UI-020 and UI-021 in MENU. `F-UI-SHOP` proposed, and picked by the owner at level 1 for the first slice. |
| 0.5.13 | 2026-11-24 | The Unreal project begins (owner: "let's try our slice in ue 5"): TECH-001 to TECH-003 accepted. Q-01 decided, Git with Git LFS (owner: "Yes, Git LFS"; ADR-0002); ASSET-001 accepted. |
