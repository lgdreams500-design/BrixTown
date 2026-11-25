---
adr: ADR-0053
title: The skate store is the Market Grocer by the market square, renamed; boards are bought through the ledger
status: superseded      # by ADR-0054 (the owner, 2026-11-25: boards at the mall's sports and toy outlets, no skate store)
date: 2026-11-25
relates_to: [CITY-098, CITY-100, CITY-102, CITY-103, Q-40, ECO-002, ADR-0042]
---

## Context
The owner picked `F-VEH-SKATES` level 2 ("Skate store and tiers") for the first slice (2026-11-24). CITY-102 and CITY-103
(the store and buying) were written but not built, in the slice or in Unreal: a new game had no board at all. CITY-102 asks
for a shop lot (as the car dealer is), prices in data `[ASSUMPTION]`, cheapest to dearest; CITY-103 for buying through the
ledger, once per kind, the faster board becoming the one in use; selling back is `[OPEN: Q-40]`.

## Options considered
| Option | For | Against |
|---|---|---|
| A new lot kind `skate` with its own building | As CITY-060 does for the dealer | Needs a free plot and a new building; the city's lots and the residents' plans change more |
| **A shop lot renamed, of a new shop kind `skate`** | The map changes by one name in a block file; residents' days are as before | One grocer fewer |
| Boards sold at the mall | No change to the map | Not a store of its own; CITY-102 names a shop lot |

## Decision
- **The lot**: b_9_8_l2, the Market Grocer by the market square, becomes **Market Skate Store**. Of the town's 47 shops it is
  the one no resident has as their grocer (its three staff are shopkeepers, and stay), it is about 130 m from the spawn, and
  no quest names it. The economy finds it by name, as every shop: `shops.skate` in economy.json matches a shop lot named with
  "Skate"; it sells and buys no items, and its till is an ordinary account.
- **Prices** (boards.json `price`, chips): manual Bx 60, electric Bx 280, Neon Boost Bx 900, `[ASSUMPTION]`: the first more
  than the starting cash alone, the second within the bank's largest loan, the last only by saving up.
- **Buying** (economy.py `buy_board()`): paid into the store's till like any purchase (cash first, then the current account),
  refused whole when that is not enough or the kind is owned; the boards owned are kept by the economy and its save.
  `board_in_use()`: the bought board is the one in use when it is faster. No selling back while Q-40 is open.
- **Where it is built**: the slice is not given the store (its counter for the shop shows nothing to buy); the Unreal build
  shows the boards in the buy menu (ADR-0044, ADR-0049), each its own model as people.js draws it.

## Consequences
- One grocer fewer; the residents' plans, visits and quests are unchanged (the store's visits buy nothing).
- The owner may move the store or change the prices: a name in a block file and three numbers in boards.json.
- Ownership on a server (PROP-001) and the server's check of riding (CITY-104) come with PH2.
