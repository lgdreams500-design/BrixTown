---
status: proposed
owner: the owner
updated: 2026-11-22
---
# City balance: how much of each building, home and career

How we decide what share of the city each kind of building, home and job should be, how we measure the map against
it, and the order in which we rebalance the 20 × 16 city. Asked for by the owner on 2026-11-22. Every target below is a
first guess `[ASSUMPTION]` until the owner sets it; nothing is rebuilt until then.

## 1. The method

1. **One set of categories.** Every home, building and resident falls into exactly one category of §2, worked out from the
   data that is already there (lot kind, block template, footprint, floors, role). Nothing new is stored to classify.
2. **Homes by floor area.** How many homes a building holds, and of what size, follows from its usable floor area (§2.1),
   so a building's capacity comes from its size.
3. **Targets as data.** The target share or ratio for each category, with a band either side, and the home-sizing
   numbers of §2.1 live in `World/rules/balance.json` (CLAUDE.md: tunable numbers in data). Targets are per head of
   population, so they hold as the city grows.
4. **A measuring script.** `Tools/worldgen/balance.py` reads the city and prints, for the whole city and for each district,
   what there is, what the target asks for and whether it is inside its band. `draw_map.py` shows the same table under
   the map. A test in `Tools/citysim/` checks that the city stays inside its bands (BAL-005) once the owner asks for tests.
5. **Rebalance by data.** Fix what is out of band by editing the generators and block files (§4), re-run, repeat until every
   line is inside its band. Homes come first because population sets every other target.

## 2. Categories

### 2.1 Homes (dwelling units), counted by floor area

The owner's rule of 2026-11-22: count apartments by floor area, take off the space that is not anyone's home, and use
four home sizes, with very few of the largest because few can afford them.

1. **Gross floor area** of a building's home floors: footprint × floors with homes (a tower's ground floor is its lobby;
   the floors over a shop or café are flats).
2. **Usable area** = gross × (1 − 35 %). The 35 % is corridors, stairs, lifts, utilities and shared rooms, the same for every
   home building (the owner, 2026-11-22: "leave 35% of residential area for common area and utilities").

3. **Home sizes:** 80, 160, 320 and 640 m² of usable area.
4. **Flats:** a building's usable area is shared among the four sizes in the flat mix, by count:

   | Size | 80 m² | 160 m² | 320 m² | 640 m² |
   |---|---|---|---|---|
   | Share of flats `[ASSUMPTION]` | 50 % | 36 % | 12 % | 2 % |

   so the average flat is 149 m² and a building holds usable area ÷ 149 flats, rounded. The 640 m² flats go on top floors
   (penthouses), the 320 m² next below; a building too small for one 640 m² flat has none, and its share goes to the
   next size down.
5. **Houses** are one home each, of the largest size that fits their usable area.
6. **Households:** people per home by size `[ASSUMPTION]`: 80 m² 1.6, 160 m² 2.8, 320 m² 3.8, 640 m² 4.5.

| Category | What it is in the data |
|---|---|
| Independent house | a home lot in a `community` block |
| Row house | a home lot in a `rows` block |
| Court flat | a flat in a home lot of a `peri` block |
| Tower flat | a flat in a home lot of a `towerR` block |
| Flat over a shop | a flat on the upper floors of a shop or café (`estate.carve` kind `flat`) |

The property market (`World/rules/estate.json`, ADR-0022) today makes a court building one unit and a tower floor one
flat; it should carve the same homes as this count (BAL-006), so that what the market sells is what the balance counts.

### 2.2 Jobs by floor area

The owner's decision of 2026-11-22 (BAL-Q2, and "go ahead as you advise"): count job places by floor area as homes are
counted, and let the city be a job centre. A workplace employs its usable floor area (80 % of the floors it works on;
shops and cafés work on their ground floor, their upper floors are flats) divided by the square metres one job takes in
its sector (`balance.json` `jobs.m2_per_job`: office 12, food 20, health 25, retail 30, making 40, culture 40, services
40, showrooms 60, utility 80, education 96 `[ASSUMPTION]`). Residents working in town take the jobs nearest home; the
rest are taken by people who come in through the gates to work (in-commuters, worked out, not stored). The malls draw
80 % of their shoppers from outside the city (`visitors`). The workplaces' places to work (`objects.work`) are where
staff stand on a day, not how many jobs there are.

### 2.3 Workplaces and residents

**Workplaces (job places)** are the `work` places in a building, grouped as: retail (shop, mall unit, kiosk, market),
food (café, food hall), office (work desks, bank, Town Hall), making (works, depots), health, education, culture
(library, museum, cinema), showrooms (dealer, furniture), utility and transport, parks.

**Residents** by role: child, student, working in town (each workplace role), commuter (works outside), elder.

## 3. Where the map stands today, and the proposed targets

Measured on 2026-11-22 with the counting of §2.1. The new malls' floors are shops, not homes.

### 3.1 Homes

| Category | Gross m² | Usable m² | Homes | 80 | 160 | 320 | 640 | Share | Target `[ASSUMPTION]` |
|---|---|---|---|---|---|---|---|---|---|
| Independent house | 127,700 | 83,000 | 491 | | 491 | | | 30 % | 25 % |
| Row house | 98,000 | 63,700 | 250 | | 186 | 64 | | 15 % | 15 % |
| Court flat | 164,900 | 107,200 | 721 | 360 | 260 | 87 | 14 | 43 % | 45 % |
| Tower flat | 21,600 | 14,000 | 94 | 47 | 34 | 11 | 2 | 6 % | 8 % |
| Flat over a shop | 24,800 | 16,100 | 108 | 54 | 39 | 13 | 2 | 6 % | 7 % |
| **All** | | | **1,664** | **461** | **1,010** | **175** | **18** | | |
| Share by size | | | | 28 % | 61 % | 11 % | 1.1 % | | |

At the household sizes of §2.1 the city holds about **4,300 people** when every home is full; today 1,134 live in it.

Per district `[ASSUMPTION]`: Old Town and the Centre mostly flats and row houses; the communities about 60 % independent,
25 % row houses in short terraces along the main lane, 15 % small blocks of flats by their local centre.

### 3.2 Residents and careers

| Group | Today | Share | Target share of residents `[ASSUMPTION]` |
|---|---|---|---|
| Children | 60 | 5 % | 14 % |
| Students | 74 | 7 % | 8 % |
| Elders | 209 | 18 % | 18 % |
| Working in town | 457 | 40 % | 45 % |
| Commuters (work outside) | 251 | 22 % | 10 % |
| Couriers | 83 | 7 % | 5 % |

Within the jobs in town, target share of job places `[ASSUMPTION]`: retail 18 %, food 12 %, office 24 %, making 14 %,
health 8 %, education 7 %, culture 4 %, showrooms 3 %, utility and transport 6 %, parks 4 %.

### 3.3 Workplaces and places to go

| Measure | Today | Target per 1,000 residents `[ASSUMPTION]` | At 4,300 people |
|---|---|---|---|
| Shop units (street shops) | 35 | 12 | 52 |
| Mall units, all floors | 579 ¹ | 25 | 108 |
| Cafés and food | 38 + 150 in malls | 10 | 43 |
| Clinics | 9 | 1 | 4 or 5, one in each community |
| Schools (places) | 0 | a place for every child | 3 primary, 1 secondary |
| Job places in town | 458 + 72 in malls | 450 (one per working resident) | 1,940 |
| Park | 4.2 ha | 15 m² a resident | 6.5 ha |

¹ Three malls with about 580 shop units between them would serve a city of 20,000 or more. Either they are smaller, or
they draw shoppers from beyond the gates (out-of-town visitors, the mirror of commuters); the owner decides (§5).

### 3.4 What is missing outright

Schools; any offices, works or depots outside Old Town; a local centre in each community (corner shop, café, clinic,
school); staff for the malls; residents for 491 houses and for the flats the new count finds.

## 4. Rebalancing, in order

1. **Homes.** Carve every building's homes by §2.1; mix row-house terraces and small blocks of flats into the communities;
   add tower flats where the targets ask for them. Re-run, check the homes table.
2. **Population.** Fill every home with a household of its size (§2.1) in the role shares of §3.2 (extending
   `make_city_01.residents`).
3. **Jobs.** From the population, how many job places of each kind the city needs; size the malls to the §5 decision; add
   a business and works quarter in the empty cells (117 are empty) for the office and making shares; a local centre and
   a school in each community; then hire residents nearest first, as today.
4. **Places to go.** Parks, clinics and cafés per 1,000; each resident's nearest of each (`near`), as today.
5. **Check.** Every line of the report inside its band; draw the map.

## 5. Decisions for the owner

| Q | Question | Options |
|---|---|---|
| BAL-Q1 | The home mix by type (§3.1 target column) | as proposed, or other shares |
| BAL-Q2 | The malls: sized for the town, or regional draws with visitors from outside? | **Decided 2026-11-22 by the owner: "malls should attract people from outside."** |
| BAL-Q3 | How big should the city's population be when it is full? | about 4,300 (today's buildings, counted by area); fewer (more large homes); more (more flats) |
| BAL-Q4 | The flat mix by size (§2.1) | as proposed, or other shares |
| BAL-Q5 | Should the property market carve the same homes (BAL-006)? | yes (more, smaller units in court buildings and towers); later |

## 5a. Where it stands after the first rebalance (2026-11-22)

`python balance.py` in `Tools/worldgen`. Homes 1,759 for 4,417 people when full; jobs by floor area 7,628 in town and
9,517 in the malls against 1,988 residents working in town, so about 15,000 come in to work. Added: a local centre in
each community (grocer, pharmacy, bakery, café, clinic, primary school; `add_outskirts_01.py`), and on a new Civic
Street between the south wall and Southfield the secondary school and its field, the fire and police stations, the town
works (water, power, waste) and the Town Park (`add_civic_01.py`). Every line of the places-to-go report is inside its
band: street shops 44 (want ≥ 42), food 41, clinics 12, park 7.7 ha (6.6), school places 763 (615), utility jobs 128
(110), services jobs 41 (40).

Then every empty home was filled (`add_households_01.py`, BAL-004): 1,034 new households, 2,850 new residents, 3,984 in
all (households fit their beds, so a little under the 4,389 the homes hold at the mean sizes). Roles now: children
13.8 %, students 7.9 %, elders 18.1 %, working in town 45.2 %, commuters out 10.0 %, couriers 5.1 %, every one inside
its band. Working residents took the nearest workplace with job places left; each workplace has a place to work for each
of its staff. New jobs use today's roles (teachers, firefighters and police are clerks and utility staff until roles of
their own are added). Flats over shops are left to the property market.

## 6. Requirements

| ID | Requirement | Feature | Level | Priority | Status |
|---|---|---|---|---|---|
| BAL-001 | Every home, workplace and resident shall fall into one category of §2, worked out from the data. | F-MAP-CITY | 2 | P1 | proposed |
| BAL-002 | Targets and bands for each category, and the home sizes, mix, common share and household sizes of §2.1, shall be data in `World/rules/balance.json`, targets per head of population. | F-MAP-CITY | 2 | P1 | proposed |
| BAL-003 | `Tools/worldgen/balance.py` shall report the city and each district against the targets. | F-MAP-CITY | 2 | P1 | proposed |
| BAL-004 | Every home shall have a household of its size and every workplace its staff, with roles in the §3.2 shares. | F-MAP-CITY | 2 | P1 | proposed |
| BAL-005 | The city shall be inside every band of `balance.json`; checked by `test_BAL_005_city_in_balance`. | F-MAP-CITY | 2 | P1 | proposed |
| BAL-006 | A building's homes shall be counted from its usable floor area (§2.1): gross area of its home floors less 35 % for common area and utilities, shared among homes of 80, 160, 320 and 640 m² in the flat mix, the largest on the top floors; a house is one home of the largest size that fits. The property market shall carve the same homes. | F-MAP-CITY | 2 | P1 | proposed |
