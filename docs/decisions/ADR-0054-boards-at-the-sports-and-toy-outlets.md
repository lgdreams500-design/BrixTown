---
adr: ADR-0054
title: Skateboards are sold at the mall's sports and toy outlets, not at a skate store of their own
status: accepted        # the owner's own words, 2026-11-25; the prices stay [ASSUMPTION]
date: 2026-11-25
supersedes: ADR-0053
relates_to: [CITY-098, CITY-100, CITY-102, CITY-103, Q-40, ECO-002, ECO-013, WLD-066, ADR-0041, ADR-0042]
---

## Context
ADR-0053 made the Market Grocer by the market square into Market Skate Store, the one place that sold the three skateboards.
On 2026-11-25 the owner asked: "can you include skate boards as well in one of the sports or toy shops", and, asked whether
they meant a board for show or one to ride, answered: "we don't need a dedicated skate boards shop. ridable skateboards should
be in sports or toy shop. player can either use them to rid or use it as display both."

## Options considered
| Option | For | Against |
|---|---|---|
| Keep the skate store and add a board for show to the sports outlets | Nothing built is undone | Two kinds of skateboard; the owner does not want the store |
| **Every sports and toy outlet of Eastgate Mall sells the three boards** | The owner's words; boards are found where sports and toys are; the outlets' counters are already built (WLD-066) | The town loses its skate store again |
| One sports outlet only | Like a store of its own | The owner named sports or toy shops, not one |

## Decision
- **Where**: every outlet of a mall whose kind boards.json `sold_at` names (`sports`, `toys`; 16 and 18 of Eastgate's 189)
  sells all three kinds, beside the goods of its range. The skate store is gone: b_9_8_l2 is the Market Grocer again and
  economy.json has no shop kind `skate`.
- **Buying** (economy.py `sells_boards()`, `buy_board(n, u, kind)`): at boards.json's prices, the same at every outlet,
  paid into the mall's till through the ledger, as its goods are (cash first, then the current account); a kind owned is
  not sold twice, and the bought board is the one in use when it is faster (`board_in_use()`, unchanged). None are bought
  back while Q-40 is open.
- **Ride or show**: a board bought is the player's, among their belongings, to ride (Y). Putting an owned board on show in
  a home is placing it from the building designer's Furnish tab, as a good for the home is (FURN-051, FURN-064). The tab is
  built (PR #116), but the boards are not among its pieces yet: that needs each kind as a piece of furnishing.json and its
  model in a room, and comes next.
- **Where it is built**: the Unreal build's outlet counters show the three boards in the same grid as the outlet's goods,
  each its own model. The slice still sells no boards (as ADR-0053 had it), and only `?auto=1` gives them.

## Consequences
- ADR-0053 is superseded: its lot is a grocer again; its prices and its buying rule stay.
- The fixed run the Unreal port checks (economy.py `board_script()`) buys at the first sports and toy outlets
  (`board_places()`), and is refused at a clothes outlet.
- Boards standing in the sports and toy outlets' windows, and a board on show at home, are still to come.
