# Look test 33: the buy menu as a grid of objects

The owner, 2026-11-24: "improve all the buy menus (game wide) by showing full 3rd model of object in a grid layout instead of
a long list of text with buy btn ... but before I want to see layout options from you."

This folder is the options, for the owner to choose from. **Nothing here is in the game**, and no requirement, level or ADR
exists for it yet: those follow the owner's choice.

## How to look at it

- The pictures in this folder: `today_the_list.png`, then `option_A_shop_window.png`, `option_B_counter_and_showcase.png`,
  `option_C_shelves.png`, `option_D_compact_grid.png` (all at Six Strings, a guitar outlet of Eastgate Mall), and the tall
  `all_options_*.jpg` (the same five samples at other counters and in other states).
- The page, where every sample can be used: start the `buy-menu-look` entry of `.claude/launch.json` (it serves the
  checkout's root on port 8861) and open `http://localhost:8861/Art/StyleTests/buy_menu_33/index.html`. The bar at the
  top changes the counter (18 kinds of mall outlet, the town's shops, the designer's Furnish list, a car showroom), what
  goods with no object show, the whole range against what one outlet sells, and a nearly empty purse.

## What is real and what is made up

Real: names, prices, where a thing goes and which outlet sells what (`World/rules`, copied into `data.js` by
`make_data.py`); the objects (the mall's goods for the home from `Prototype/play/goods_models.js`, furniture as
`furniture.js` and `outlet.js` draw it, cars from `car_models.js`); the panel, fonts and colours of the slice's counters
(`Prototype/play/index.html`, `#life`).
Made up for this page: the stock numbers, the purse, the dusk behind the panel, and the stand-ins (a parcel, a paper bag,
a cup, folded cloth, shoes, a cap), which are a few boxes built in the page and marked "stand-in".

## Where the slice sells something today

| Where | What it sells | Has an object? |
|---|---|---|
| A mall outlet's counter (`life.js`, panel "outlet") | 8 of its kind's goods; 174 goods over 18 kinds | 49 goods for the home: yes, made in Blender (FURN-065). 40 clothes, 38 carried and 47 eaten goods: no |
| A shop or café counter (`life.js`, panel "shop"): grocer, kiosk, arcade, bakery, cafés | 20 carried things (bread, a map, a teapot, coffee); it also buys from the player | No |
| The building designer's Furnish tab (`life.js`, "Add a piece") | 42 pieces of furniture in four lines, by room | Boxes, as all furniture in the slice; the Blender pieces of kits 17 and 18 are renders only |
| The estate agent and "this building" (`life.js`, panel "agent") | Homes and business units, to buy or rent | Not goods: the building itself, in the world |
| The market stall | Nothing: the player sells there | |
| The furniture outlet (`outlet.js`) | Nothing yet: a showroom with price tags, no till (FURN-023 to FURN-026 not built) | Boxes |
| The car showroom | Nothing yet (CITY-060 not built) | Yes: four cars in `car_models.js` |
| The skate shop | Nothing yet (pull request #28: models and requirements) | Yes, in that pull request |

## The four layouts

- **A, Shop window**: big cards, every object turning, name, where it goes, stock and price under it, a Buy button on each.
- **B, Counter and showcase**: a tight grid of objects with prices, and one large object to turn by hand with its details
  in words and one Buy button.
- **C, Shelves**: the goods stand on shelves, one shelf per place a thing goes, with a paper price tag under each; big
  things stand bigger than small ones.
- **D, Compact grid**: small cells four across in a panel 380 px wide, the width of the designer's side panel.

In all four: sold out is a painted band across a greyed object and no button; what the purse cannot pay has its price in
red with how much is short; "where it goes" is a coloured mark (teal home, plum wardrobe, ochre carried, terracotta
eaten); arrows choose, Enter buys, Esc closes; a click chooses, a double click buys.

## Not decided

Which layout; what goods with no object show (a stand-in, a plain tile, or new models first); whether counters move to
the "Painted signs" look of the menu (ART_DIRECTION 2.7) at the same time.
