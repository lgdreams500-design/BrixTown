---
adr: ADR-0012
title: Money, shops, quests and the designer in the first slice; worked out from the clock, kept in one ledger
status: proposed        # drafted 2026-11-22 from the owner's request and level picks; awaiting the owner's acceptance
date: 2026-11-22
relates_to: [ECO-001, ECO-002, ECO-003, ECO-005, ECO-009, ECO-010, ECO-011, ECO-012, BANK-001, BANK-002, BANK-007, BANK-008, PROP-001, PROP-008, CHR-001, CHR-002, CHR-013, QST-001, QST-002, QST-003, QST-004, QST-005, QST-006, QST-007, QST-008, TECH-004]
---

## Context
On 2026-11-22 the owner asked for money with a bank account, buying and selling items, quests from
residents that pay, a few starter quest lines, and a character designer at the start of a new game.
They then picked level 2 for the first slice for all four features: `F-ECO-BANK` (full bank),
`F-BLD-SHOP` (stock and prices), `F-QUEST` (branching and repeatable) and `F-CHR-DESIGN` (full designer).
That brings PH4 banking and the PH3 designer into PH1. The project's rules still hold: money is integer
chips and moves only through the ledger (ECO-001, ECO-002), what can be calculated from the clock is
not ticked (TECH-004), and rules are proved in `Tools/citysim/` before they are built in.

There is no Unreal code or server yet. Everything playable is the browser slice in `Prototype/play/`.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Tick the economy every frame (interest accrues continuously, shops sell down per second, NPCs generate jobs live) | Simple to picture | Breaks TECH-004; results depend on frame rate; cannot be checked headless; costs grow with the city |
| Store everything that happens (every resident purchase, every job offered) | Easy to audit | Huge saves; 1,134 residents × shop visits a day; most of it is never looked at |
| **Calculate what can be calculated from the day, the clock, the routines and the data; store only what the player did; settle money lazily, in day order, as ledger transactions** | Same answer every time; testable in Python; small saves; matches how residents and traffic already work | Every rule needs a careful definition of "since when"; the Python rules and the browser port must agree (checked by shared test values) |

## Decision
Third option.

- **One ledger.** Every movement is a transaction of lines that sum to zero, in integer chips, append-only.
  Money is created or destroyed only by named system accounts (`sys:start`, `sys:founding`, `sys:rewards`,
  `sys:residents`, `sys:fees`); total supply is the sum of every other account. Shop tills, the bank vault
  and the player's cash, current and savings accounts are ordinary accounts in it.
- **The bank settles by the day.** When the game day changes (or the save is loaded on a later day), each
  day that ended is settled in order: savings interest, then each loan's instalment, late fee or default.
  Rates and terms are fixed data until the central bank exists (BANK-003, PH4) `[ASSUMPTION]`.
- **Shop stock is calculated.** Stock = target − what residents bought on their shop visits since the
  last restock (from their routine) − what the player bought + what the player sold. The price follows the
  stock inside data bounds. Only the player's own trades are stored.
- **Stall sales are calculated** from the residents who come to the market while the stall is open, the
  price against the item's usual price, and a fixed hash; they are posted to the ledger as the clock passes them.
- **Daily jobs are calculated** from (resident, day): who offers one, which kind, where and how much. Only a
  taken job and the day's finished offers are stored.
- **Quest lines are data** (`World/rules/quests.json`) using the dialogue node format, plus actions and
  conditions; checked by `world.quest_problems()` and walked end to end in `Tools/citysim/test_economy.py`.
- **The designer stores values, not meshes** (CHR-002): a few numbers and names, inside the ranges of
  `World/rules/designer.json`, read by the same body and head code as every resident (rule C7).
- **The slice saves in the browser** (`localStorage`): character, ledger, items, loans, quests, reputation,
  stall, day and time. Time stops while the page is closed (Q-06 is still open for the real game).
- **Python first, then the browser.** `Tools/citysim/economy.py` and `quests.py` are the rules;
  `Prototype/play/economy.js` and `quests.js` are ports. `build_play.py` packs test values computed in Python
  into `city_data.js`, and the page checks itself against them (`wasted.econ.selfTest()`).

## Consequences
- The economy can be soak-tested headless later (ECO-007) from the same rules.
- In PH2 all of this moves to the server and the backend (MP-001, ACC-003); the browser save is only a stand-in.
  The client will then only display balances and ask for trades.
- Residents do not have their own money yet: what they spend at the player's stall comes from `sys:residents`,
  a named faucet, until CITY-004 (PH3) gives them accounts.
- Two copies of each rule (Python and JS) must be kept in step; the shared test values catch drift.
