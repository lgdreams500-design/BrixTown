# ADR-0041: Buying at each outlet of the mall, and what the outlets sell

Date: 2026-11-24. Status: accepted for building (owner's request in chat, 2026-11-24). F-BLD-MALL and F-BLD-SHOP are picked
at level 2 for the first slice; F-CHR-WARDROBE, F-INV-ITEMS and F-NPC-POP stay at level 1 (see "Levels" below).

## Context

Eastgate Mall has 189 outlets of 18 kinds (ADR-0039), but it was bought from as one shop: one prompt for the whole lot
("Buy and sell at Eastgate Mall") selling six things. The owner asked:

> add B or Buy separately at each outlet, instead of showing it for entire mall. each outlets should have buyable
> products in it with decent catalog, use blender to create the objects, some of these object can be placed in the house
> like guitar, books etc. and some like clothes will be made available in wardrobe. add then update population to come
> to mall and some taking career as shop owners at the mall outlets, i think one person per outlet is good enough. few
> security, ganitors etc, can be added to handle entire Mall.

This is four pieces of work, done in stages that each stand alone: (1) buying at each outlet and the catalogue, (2) the
objects made in Blender, (3) a keeper for every outlet with guards and janitors for the mall, (4) residents who come to
shop and eat. This record is written with stage 1 and is added to as the others are built.

## Decision (stage 1: buying and the catalogue)

1. **Each outlet is its own counter.** Standing in an outlet's room, on any floor, the player is offered "Buy at
   <the outlet's name>" and B opens that outlet's own goods. Out on the walks there is no counter: B says to walk into an
   outlet. The mall is no longer bought from as one shop (ECO-013, WLD-066). Westgate and Northgate have no inside and
   are bought from as before.
2. **The catalogue is data.** `World/rules/mall_goods.json` lists, for each of the 18 kinds of outlet, the goods that
   kind deals in (7 to 25 of them; 174 goods in all, with a few of the town's own items such as bread, coffee and the
   novel), each with a name, a price in chips and where it goes. One outlet sells 8 of its kind's goods (`per_outlet`),
   chosen by a fixed hash of the outlet and the good, so two clothes shops differ and no outlet ever changes (ECO-014).
3. **Where a good goes when bought** (`goes`):
   - `home`: it is a piece of `World/rules/furnishing.json` (`from: "mall"`, 49 of them: guitars, a drum kit, books, a
     globe, lamps, vases, a rug, a mirror, plants, toys, a television, a radio...). It goes to the belongings and is
     placed in a home from the building designer's Furnish tab, free, like any piece taken away and put back. The
     designer's own list does not sell these (FURN-064).
   - `wear`: clothes, shoes, caps, scarves and a bag. It is kept as owned (`clothes` in the save) and listed in the
     wardrobe under "Clothes you own"; Put on sets the parts of the look it names (CHR-016).
   - `eat`: food and drink. It is carried, as a cafe's coffee already is. Nothing is eaten: the slice has no needs yet.
   - `carry`: small things and jewellery. Carried; can be set out on the player's market stall.
4. **Stock and price are each outlet's own**, by the rules street shops already have (ECO-010, ECO-005): an outlet holds
   6 of each good at each restock, each resident's visit to the mall buys one thing at one outlet (both by a fixed hash),
   and the price rises as the shelf empties. Nothing is ticked.
5. **Money.** A purchase is one balanced ledger transaction from the player's cash (then current account) into the mall's
   one till, `till:<lot id>`, with the outlet's name in the memo. Outlets buy nothing back.
6. **Rules first.** The rule is in `Tools/citysim/economy.py` (`buy_at` and the functions round it), proved by
   `test_economy.TheOutlets`, and ported line for line to `Prototype/play/economy.js`; the page's self-test now runs
   outlet purchases too and compares them with Python's.

## Decision (stage 2: the goods as objects made in Blender)

Written 2026-11-24, after stages 1 and 3.

1. **One script makes all 49** (FURN-065): `Tools/blender/mall_goods_31.py`, run headless, building on
   `furniture_kit_17.py` (its painted matte materials, the kit daylight of ADR-0007, its camera and render settings and
   the person for scale) and on the car export's way out to the slice. Each good is one short function made of a few
   light shapes: box, cylinder, ball, rod, a turned shape (vases, lamps, pots) and a slab cut to an outline (guitar
   bodies, rockers, a roof). Each fits inside the size the rules give its piece; the script refuses to write the file
   if one does not.
2. **Two makes of each**: near, for a home (at most 600 triangles; 284 on average), and far, a lighter one (at most
   180; 112 on average). The far ones are trimmed by hand in the script, not thinned by the machine.
3. **Colours**: the slice's own muted palette, 23 colours. Two of them (`accent0`, `accent1`) are left open in a model
   and chosen in the slice by the piece's seed, so two guitars or two rugs differ.
4. **In a home** a placed good is drawn from its object (`Goods.put`, called where the designer's layouts are drawn).
   The boxes of stage 1 stay in `goods.js` and stand in only if `goods_models.js` is missing.
5. **In an outlet** the goods for the home that outlet sells stand in it as the same objects: on its two window
   plinths, facing the walk, and the ones too deep for a plinth (a drum kit, an exercise bike, a rug) on the floor
   inside. What is seen in an outlet is what B sells there. 112 of the 189 outlets sell such goods.
6. **Which make stands in the outlets is data** (`World/rules/mall_goods.json` `display.detail`). It is `near`: the
   far ones looked too crude at arm's length from a shop window. Measured in the slice: 464 objects,
   134,674 triangles, 0.40 million points, 3.0 % of the town's mesh. `far` would cost about 40 % of that.
7. **For the engine**: `Art/Export/mall_goods_31/mall_goods_31.glb` holds the near objects, one object each, and
   `Art/Source/StyleTests/mall_goods_31.blend` every mesh.

## Decision (stage 3: a keeper in every outlet, guards and janitors)

Written 2026-11-24, after the owner was told that it would give 639 residents other work and answered "Yes, re-deal".

1. **One keeper to each outlet, on every floor** (CITY-105). In a mall whose units are outlets, `careers.posts` gives
   each outlet one post: `shop_owner` ("shop owner", the owner's words) for a shop, `food_outlet_owner` for a food
   outlet. The 206 posts the ground floor had kept from the old plan (three to a shop, four to a cafe) are gone.
2. **A few for the whole mall**: six security guards and six janitors (`mall_staff.whole_mall` in
   `World/rules/careers.json`; "few" is the owner's word, six of each is a first guess [ASSUMPTION]). They belong to no
   outlet.
3. **These posts are always filled.** They are the mall's minimum: the nearest residents take them before anyone else in
   town is placed. The other choice, leaving them as a cap under each group's share as the old posts were, staffed
   only 66 of the 189 outlets.
4. **Where they stand** (WLD-067). `mall_inside_01.py` gives every outlet one `stand` behind its counter and the plan
   its `staff` places: a guard inside each of the four ways in and by a lift on each upper floor; a janitor by each
   fountain and two on each gallery. `add_careers_01.py` writes the lot's places to work from those.
5. **Clothes of the job.** A career may say what it wears (`wear`); a guard has a dark uniform and cap, a janitor
   overalls and a cap, drawn by `people.js` (ART_DIRECTION C5: the silhouette says who someone is).
6. **The corner stores and kiosks are unchanged**: they keep posts by floor area, as a cap. They are not outlets and are
   not bought from.

What it did to the town, measured against `main` of 2026-11-24:

- **636 residents have another workplace**, 811 another career and 504 another daily routine, of 2,401 with a career
  (1,797 with a post in town; 3,984 residents). 197 residents join Eastgate Mall and 66 leave it.
- Eastgate Mall: 201 residents at work (158 shop owners, 31 food outlet owners, 6 guards, 6 janitors) where it had 70.
- Each group's share of the town's workers is unchanged (retail 336, food 195, facilities 27), so the keepers come
  mostly out of the other two malls: **Westgate has 25 staff where it had 74, Northgate 21 where it had 95**. Offices
  lose seven. If those malls are to keep their staff the shares must change, which is the owner's to say
  (`World/rules/careers.json` `shares`).
- The six residents named in quests keep their work (Olu, Hari, Eli, Mei, Jai, Abe); nobody had to be pinned.
- A plain re-run without this rule would have moved 44 residents: 250 doors had moved since careers were last dealt
  and the deal goes by the nearest door. `test_CITY_086_careers_are_worked_out_from_the_rules_alone`, failing on
  `main` for that reason, passes again.
- Walks that arrive on time (`test_most_walks_arrive_on_time`, already under its 80 % on `main`, Q-21) go from 79.9 %
  to 78.9 %. The mall stands outside Old Town: its staff live about 850 m away, and 397 of their 402 walks to work
  take more than two minutes longer than planned (half of them over 35 minutes). That was already so for the 70 who
  worked there.

## Decision (stage 4: residents who come to shop and eat)

Written 2026-11-24, last of the four.

1. **The mall is one more place the town's routines may choose** (CITY-106). `World/rules/choices.json` names it in the
   lists residents already choose from: 3 of the 24 ways to spend free time (two to shop, one to eat), 1 of the 9
   shopping trips, 1 of the 13 things done on a rainy day. Each took the place of a stroll, staying at home or a trip
   to the grocer, so everyone else's day is as it was. It is the mall with outlets nearest the resident's home.
2. **Only when it makes sense.** A resident goes when their own start of that part of the day is within the mall's
   hours (09:30 to 17:45, so they are served before the keepers go home), when there is time to walk there and stay
   25 minutes, and, to shop, on a day its shop owners work. On a Sunday people come only to eat: the food outlets are
   kept every day. Otherwise they do what the list says instead.
3. **The mall's staff eat at the mall.** Lunch for those who work there is at its tables, not in town.
4. **Places** (WLD-068): two before every outlet's counter and the chairs of every food outlet's tables, on all three
   floors: 458 places to stand and 332 to sit.
5. **Nobody is walked through the mall.** The city's rules walk people in a straight line from a door to their place,
   which in a hall 240 m by 340 m on three floors meant long walks through shop walls. A resident is now at the
   mall's door and then at their place in it.
6. **Nobody is added.** These are the town's 3,984 residents (`F-NPC-POP` stays at 1).

What it gives, measured over the starting city with cars and buses running:

- Wednesday: 68 visits, 23 in the mall at the busiest half hour. Friday: 96 visits, 38 at the busiest. Saturday: 162 visits, 52 at the busiest.
  Sunday: 80 visits, 49 at the busiest, all to eat.
- At 13:00 on a working day 199 of the mall's 201 staff are in it and 106 of them sit at its tables; 69 were in it
  before this.
- Walks that arrive on time (Q-21): 79.0 % (78.9 % before this stage, 79.9 % on main, 80 % wanted).

## Levels

Only the owner picks levels. Nothing was recorded in `feature_choices.json` for this work.

- **F-CHR-WARDROBE is picked at 1** (saved outfits, changes free anywhere); its level 2 is "Clothes you own". The owner's
  sentence asks for bought clothes to be "made available in wardrobe" and names no level. What is built is only that:
  bought clothes are listed in the wardrobe and can be put on. Changing colours and garments is still free, there is no
  wardrobe at home to change at, and clothes do not have to suit the weather. Whether the rest of level 2 is wanted is
  the owner's to say.
- **F-INV-ITEMS is picked at 1**; using, dropping and carrying limits are its level 2. Food is therefore only carried.
- **F-NPC-POP is picked at 1**; crowds are its level 2. Stage 4 sends the residents the town already has to the mall; it
  adds nobody.

## Consequences

- A save made before this has no `clothes`; it loads with none. A hat or scarf bought at the mall before this is still
  carried.
- The mall's till takes every outlet's money; an outlet has no account of its own. If outlets are ever let to players
  (PROP-005) each will need one.
- Jewellery is carried, not worn: the people in the slice have no place to wear it.
- The goods for the home are drawn as a few plain boxes each (`Prototype/play/goods.js`) until stage 2 exports the
  Blender objects; what stands on an outlet's shelves is still ADR-0039's fixtures, not yet the goods it sells.
- Small things (a lamp, a stack of books) stand on the floor when placed: pieces cannot yet be put on a table or a
  shelf. They are marked `small` in the data for that rule.
- Stage 2: the things an outlet sells that are carried, eaten or worn (strings, a burger, a shirt) have no objects;
  its shelves, rails and counters still show ADR-0039's fixtures for those. Nothing is drawn in the player's hand.
- Stage 2: an outlet's display does not change as it sells out; a sold-out guitar still stands in the window.
- Stage 2: the objects have no textures and no moving parts, and a television or lamp does not light up.
- Stage 2: the pictures are of the objects alone and of the slice; the look has not been approved by the owner.
- Not measured: frame rate. Nothing in stage 1 adds to the town's mesh; stage 3 adds a counter to 129 more outlets (a
  few boxes each) and up to 201 people in the mall by day.
- Stage 3: the keepers follow their role's day as every shopkeeper and cafe worker does, and come in from about 850 m
  away. Counted in the slice's day (day 16, a Wednesday), of the mall's 201 staff: 164 are in at 08:00, 198 at 10:30,
  167 at 16:00 and none at 20:00. Shop owners do not work on Sundays; food outlet owners work every day. An outlet
  sells whether or not its keeper is in, as street shops do.
- Stage 3: **at lunch most of the keepers are out.** They go to eat in town as every shopkeeper does: at 13:00 in the
  slice 69 of the 201 are in the mall, 107 are walking and 25 are somewhere else (in the rules' own run, where nobody
  drives or rides, 30 are in and 171 walking). Eating at the mall is stage 4, and should keep them there.
- Stage 3: guards and janitors stand at their places and carry nothing; they do not walk rounds or clean.
- Stage 3: the four corner stores have none of the town's residents in them (each group's share is used up).
- Stage 4: **the mall is thinly visited on working days.** It stands about 850 m outside Old Town, a walk of half an
  hour and more each way, so only long free afternoons reach it. A fuller mall needs more people (`F-NPC-POP` 2,
  the owner's to pick), a mall-goers' bus, or longer opening into the evening with keepers on a later shift.
- Stage 4: a visitor's place is any free one; it is not tied to the outlet the economy says they bought at.
- Stage 4: nobody walks about inside the mall, queues or carries a bag; arriving, a resident appears at their place.
- Stage 4: on a Sunday afternoon the food outlets' owners take their break at 15:00 while people are eating there.
- Stage 4: Westgate and Northgate have no outlets and are not visited by this rule.
- No automated test drives the page. The rule's port is checked by the page's self-test; the prompt, the panel, the
  wardrobe and placing a bought guitar were tried by hand in the slice.

Pictures: `Art/StyleTests/mall_shopping_32/`.
