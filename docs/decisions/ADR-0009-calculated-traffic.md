---
adr: ADR-0009
title: Residents' traffic is calculated from the clock; only player cars are live
status: accepted        # owner, 2026-11-22
date: 2026-11-22
relates_to: [CITY-043, CITY-044, CITY-045, CITY-046, CITY-048, CITY-049, CITY-062, TECH-004, MP-001]
---

## Context
The owner picked drivable cars and real traffic from the first slice, with residents driving their own
cars as part of their routine. The city has 1,134 residents across 64 blocks. The project's rule
(LIVING_CITY §1, TECH-004) is that anything calculable from the clock and data is not ticked or stored,
and bodies exist only near players. Real traffic (queues, giving way, signals) depends on every other car,
which looks like it needs a live simulation.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Live traffic simulation for every car, all the time | Familiar; cars react to everything | Ticks the whole city; breaks TECH-004; server cost grows with city size; hard to test |
| Live simulation only near players, nothing elsewhere | Cheap | Cars appear and vanish without a history; a car you saw leave never arrives |
| **Plan each day's car trips, queues included, once from the seed; calculate any car at any time from that plan; live only for player cars and local give-way** | Same answer on every machine; no ticking; testable headless in `Tools/citysim/`; matches how residents already work | The plan cannot foresee players; resident cars must recover locally after a player blocks them (Q-25) |

## Decision
Third option. A day's resident trips are routed and their junction and queue waits worked out when the
day plan is made, deterministically from the seed. A resident car's state at any time is then a lookup,
like a resident's position (CITY-022). Bodies appear within the car load distance. Player cars are
live, server-authoritative physics (CITY-062). Near a player, resident cars give way and then rejoin
their timeline without speeding (CITY-049).

## Consequences
- The rules are proved in `Tools/citysim/` with tests before any Unreal work.
- The road network, bays and crossings become city data checked by the world tests (CITY-043).
- How a delayed resident car catches up is still open (Q-25).
- Engine side: player cars likely use Unreal's vehicle physics; which built-in systems to use is Q-18.
