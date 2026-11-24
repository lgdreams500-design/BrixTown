# ADR-0044: Buy menus show what is for sale as objects

Date: 2026-11-24. Status: accepted for building (owner's request and choice in chat, 2026-11-24). No level is picked for
the proposed feature `F-UI-SHOP` (see "Levels" below).

## Context

Every place the slice sold something showed a table of text: a name, how many are left, a price and a Buy button on each
row. That was true of a shop's or café's counter, of each outlet of Eastgate Mall (ADR-0041) and of the building
designer's list of pieces to add. The mall's goods for the home had just been made as objects in Blender (FURN-065), and
none of the menus showed them. The owner asked:

> can you as well on the side improve all the buy menus (game wide) by showing full 3rd model of object in a grid layout
> instead of a long list of text with buy btn? but before I want to see layout options from you.

Look test 33 (`Art/StyleTests/buy_menu_33/`, a page that draws the slice's own goods) showed today's list and four
layouts: A "Shop window" (big cards, a Buy button on each), B "Counter and showcase" (a grid, the chosen thing large
beside it), C "Shelves" and D "Compact grid" (small cells for a narrow panel). The owner chose:

> B, and D when narrow

## Decision

1. **One menu, two widths** (UI-020). A counter shows a grid of the things for sale, each drawn as an object with its
   price under it and a mark for where it goes, and beside the grid the chosen thing large: it turns slowly, can be turned
   by hand, and has its name, where it goes, its size, what is left and its price in words, and the one button that buys
   it (layout B). In a panel narrower than a width in data the same things are small cells four across, and the chosen
   one is described in a strip under them with its button (layout D). The building designer's side panel is that narrow,
   so its list of pieces is layout D; so is a counter in a small window.
2. **Where it is used**: a shop's and a café's counter, both to buy and to sell (what the shop takes from the player is
   shown in the same cells, with what they pay); each outlet of a mall; the designer's "Add a piece" in the chosen line.
   The estate agent's list stays a list: a building is not shown as an object in a cell, it is out in the town.
3. **Choosing is not buying.** A click or the arrow keys choose; the button, Enter or a double click buys. A thing that
   is sold out is greyed under a "Sold out" band; a thing dearer than the player's cash and current account together
   has its price in red with how much is short. Neither can be bought, and the rules still refuse them (ECO-013).
4. **What a thing shows is a rule, checked** (UI-021): its own object where one is made (a mall good for the home, as
   made in Blender; a piece of furniture, as the slice draws it), else a stand-in. A stand-in is plainly packaging or
   cloth, never a guess at the thing: a parcel with the seller's painted band, a paper bag, a cup, a small case, and for
   clothes folded cloth, shoes, a cap, a scarf or a bag in the clothing's own colour. Which stand-in is data: by the
   thing's id, by the part of the look a piece of clothing sets, or by where the thing goes. The rule is
   `world.buy_menu_shows`; `Prototype/play/buymenu.js` ports it and checks itself against the Python on load.
5. **How it looks is data**: `World/rules/buy_menu.json` holds the widths and cell sizes of both layouts, the camera of
   the pictures and how fast the chosen thing turns, the words and the mark for each place a thing goes (the mark's
   colour is one of the menu's board colours, ADR-0033), the stand-ins as lists of boxes, and their colours.
6. **The pictures are drawn by one hidden WebGL canvas** and copied into a 2D canvas in each cell, so cells scroll and
   clip like any other part of the page and the game's own canvas is not touched. Light and surface follow the look:
   the kit daylight (ADR-0007), matte and mottled (ART_DIRECTION W1). If the machine gives no second WebGL context a
   cell shows the mark for where the thing goes in place of the picture, and everything else works.
7. **The panel's look is unchanged**: the counters keep the dark panel they had. Whether they move to the menu's
   "Painted signs" (ADR-0033) is still for the owner to say.

## Levels

`F-UI-SHOP` "Buy menus that show the goods" is proposed in `docs/backlog/features.json`: 0 a list (as before), 1 objects
in a grid with stand-ins (this record), 2 every thing its own object. The owner asked for this to be built and chose its
layout, but named no level, so none is recorded in `feature_choices.json`.

## Consequences

- UI-020 and UI-021 added (MENU.md); `Prototype/play/buymenu.js` and `World/rules/buy_menu.json` are new; `life.js` no
  longer builds a table for a shop, an outlet or the designer's pieces.
- 125 of the mall's 174 goods and all 20 of the town's shop items show a stand-in, and every carried thing of one seller
  shows the same parcel. Giving each its own object (about 100 carried and eaten things) is level 2 and its own work, as
  is showing clothes worn on the player before buying.
- Furniture in the grid is boxes, as it is in every home of the slice (INTERIOR_STYLE section 6); when the Blender pieces
  come to the slice the grid shows them with no change here.
- A shop's panel is wider than before (960 px against 620) and shows either what it sells or what it buys, on two tabs.
- At a counter the arrow keys now choose a thing; in the building designer they still move the camera, so there the
  grid is used with the mouse.
- The purse a counter checks is cash and current account together, as a purchase is paid; a thing the player cannot
  pay for is now shown as such before the button is pressed.
- In Unreal the same data serves: the grid is a widget, the pictures are render targets of the same meshes, and the
  rule that says what a thing shows is the Python's.
