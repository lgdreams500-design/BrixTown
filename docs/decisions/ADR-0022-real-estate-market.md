---
adr: ADR-0022
title: The real estate market is units on the ledger, settled by the day
status: proposed
date: 2026-11-22
relates_to: [PROP-002, PROP-006, PROP-009, PROP-010, PROP-011, PROP-012, PROP-013, PROP-014, PROP-015, PROP-016, PROP-017, PROP-018, PROP-019, PROP-020, PROP-021, PROP-022, BANK-002, ECO-002, ECO-003, TECH-004]
---

## Context
The owner asked for "a whole real estate economy": every building, whether residential, commercial, utility or anything
else, sellable and owned by a player, a resident or one of several real estate companies, with smaller units inside, and
balanced. They picked `F-PROP-ESTATE` level 2 (living market) for the first slice. Furniture that belongs to a unit is
being worked on in another session (`F-PROP-FURNISH`).

The town has 503 lots, 1134 residents and no resident accounts yet (CITY-004). The rule is to calculate rather than tick
(TECH-004), and money moves only through the ledger (ECO-002).

## Options considered
| Option | Pros | Cons |
|---|---|---|
| A. Ownership worked out purely from the clock (sales on hashed days, no stored state) | Nothing stored; same everywhere | The player's deals change everything after them; company money cannot be on the ledger |
| B. Every resident a ledger account, the market ticked | Most faithful | Thousands of accounts and transactions a day; breaks TECH-004; the save grows fast |
| C. Units with owners, settled day by day like the bank (BANK-008); residents and businesses share one town account until CITY-004 | Deterministic and testable; one ledger; about ten transactions a game day; follows a pattern already accepted | Residents' money is pooled; the market is stored state, not calculated |

## Decision
Option C.
- Buildings are carved into units from their floors (`storeys`), in `World/rules/estate.json`. Unit ids are
  `<lot id>:u<n>`, stable as long as a building's floors are, so furniture and other records can point at a unit.
- Owners: the town (`sys:city`), real estate companies (`co:<id>`, at least two, data), the household or business in
  the unit (the pooled `npc:town` account), or the player. Each company has an office (an existing work lot) where the
  player deals with it.
- Each game day's end, in order: block price indices; rents, upkeep and tax and the town's earnings (one aggregate
  transaction for everyone but the player); lodgers coming and going; listings selling, being cut, new listings; the
  player's mortgages; company dividends; the town's spending. Every choice comes from the shared hash of the day and the
  unit, so the browser port reproduces Python exactly (self-test).
- Balance is held by named faucets and sinks (`sys:wages` in; `sys:tax`, `sys:fees`, `sys:spend`, `sys:dividends` out)
  and checked by tests against targets in `estate.json` (`balance`), measured against an assumed player income.

## Consequences
- Prices, rents, taxes and the player's income target are all `[ASSUMPTION]` until play-tested; tuning is data only.
- Residents do not move house yet: their routines come from `residents.json`. PROP-022 (proposed) moves them once the
  routines read homes from the market.
- When residents get their own accounts (CITY-004), `npc:town` splits into households and businesses; the settle order
  stays.
- In PH2 the market and its ledger move to the server (MP-001, ACC-003); the day-by-day settle is what the server runs.
- The 3D buildings still show their old signs: the three company offices are named in data, not on the facades.
