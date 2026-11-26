---
adr: ADR-0065
title: Lights come in kinds, set in data, and are placed in the designer
status: accepted          # the owner in chat, 2026-11-26, picking F-LIGHTS level 2: "2 Placed in designer"
date: 2026-11-26
relates_to: [FURN-066, FURN-067, FURN-068, FURN-069, FURN-070, ADR-0061, ADR-0031]
---

## Context
Reviewing a home's room in the world's light, the owner said: "when the room is lit by lights then the color should be
decided by the light types. I think this is a good time to add different lights with their characteristics. in designer
mode player has ability to place lights, ground light, wall mounted or ceiling lights." Until now every lamp was the
same warm orange (furniture_kit_17 `light()`, 1.0 0.6 0.3), set in code.

## Decision
- A light is of a **kind**, a row of `World/rules/lights.json`: where it is mounted (floor, table, wall, ceiling), its
  colour (linear RGB, with its kelvin), strength, spread (pool, up, wash, flame), shade glow, size and price. Seven kinds
  to start: floor lamp, table lamp, desk lamp, wall light, pendant, ceiling light, candles.
- A room lit by its lamps takes its colour from its kinds, in the game and in the Blender references alike; the
  references' lamps are tied to kinds by their names (`blender_lights`), the furniture that carries a lamp by its piece
  (`pieces`).
- In designer mode (F-BLD-DESIGN) the player places lights from the kinds; each snaps to its surface, is paid for
  through the ledger and checked by the server; where each may stand is a rule proved in `Tools/citysim/lights.py`.

## Consequences
- No lamp colour stays in code: the references read the file (`Tools/blender/hours_50.py`), as will the game.
- The slice and the Unreal build still have to port the kinds and the placing (FURNITURE.md 0.5.0, "Not yet").
- Dimmers, colour choice and timers are F-LIGHTS level 3, not picked.
