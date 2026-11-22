---
adr: ADR-0025
title: The game is called Brix Town
status: accepted
date: 2026-11-22
relates_to: [ECO-001]
---

## Context
The project's working title was "Wasted", and Q-13 asked what that name meant for the game's tone. The owner chose a
new name on 2026-11-22, "Brix Town", after the game's currency, the brix (ECO-001), and made the GitHub repository
`BrixTown`.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Rename everything, code identifiers included | One name everywhere | Breaks players' saves in the slice (`wasted.*` browser storage), every console hook and test, every script with a path, for no gain to anyone playing |
| Rename what people read; keep identifiers | Nothing breaks; the old name stays only where nobody sees it | "wasted" still turns up in code |

## Decision
Rename what people read: document titles and headings, the playable slice's page title and title card, file headers,
the README, the Unreal project name still to be made (`BrixTown`, no space, as Unreal needs) and the example test name.
Keep as they are: `window.WASTED` and `window.wasted` in the slice, the browser storage keys (`wasted.save.v1` and
the rest, so saves carry on), the `WASTED_*` environment variables of the Blender scripts, document ids such as
`WASTED-SPEC`, the local folder `C:\Projects\Wasted`, old style-test logs, and `Art/Reference/` (read-only).

## Consequences
- Q-13 is closed. Q-13's question about tone is replaced by whatever the owner wants "Brix Town" to say.
- New identifiers should use `BrixTown` (code, modules) or `brix` (short prefixes); old ones are not renamed.
- `Art/Reference/README.md` still says "Wasted"; it changes only if the owner says so.
