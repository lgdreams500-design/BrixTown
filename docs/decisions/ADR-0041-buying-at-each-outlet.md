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
- Not measured: frame rate. Nothing in this stage adds to the town's mesh.
- No automated test drives the page. The rule's port is checked by the page's self-test; the prompt, the panel, the
  wardrobe and placing a bought guitar were tried by hand in the slice.

Pictures: `Art/StyleTests/mall_shopping_32/`.
