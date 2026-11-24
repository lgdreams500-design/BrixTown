---
adr: ADR-0045
title: The browser slice is frozen once the Unreal build catches up; prototyping after that
status: accepted
date: 2026-11-24
relates_to: [TECH-001, TECH-002, TECH-004, WLD-020]
---

## Context
Until now the browser slice (`Prototype/play/`) has been the playable game. The Unreal project now builds the same city
from the same `World/` data (Game/, ADR-0002 era). Carrying every feature in both would double the work, and the two
would slowly disagree. The owner asked what studios do and chose in chat on 2026-11-24: "freeze the slice once unreal
catches up".

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Freeze the slice once Unreal catches up | One game to build; feel is judged in the engine it ships in | Until then features still land in the slice first; afterwards quick experiments need Unreal or a throwaway page |
| Keep building features in both | The slice stays an instant, install-free place to try things | Every feature twice; the two drift apart; slice-only work (its own speed-ups, its own look tricks) helps nothing that ships |

## Decision
- **Until Unreal catches up**, the browser slice stays the main playable game, and features keep landing there as now.
- **Catching up** means the Unreal build has what the slice has for the first slice (PH1). The owner says so in chat
  when it is reached; that is not decided here.
- **From then on the slice is frozen**: kept runnable as a reference, with no new features and no upkeep beyond keeping
  it opening. New features are built in Unreal only.
- **What does not change**: rules are still proved first in Python with tests (`Tools/citysim/`, FEATURE_PROCESS stage
  5) and are the source of truth the C++ is checked against; the map and tuning stay data in `World/` and config files;
  small throwaway pages and Blender look tests are still used for one-off look and sound choices.
- **After the freeze, feel is prototyped in Unreal** (movement, camera, driving, interactions).

## Consequences
- Each feature until the freeze needs a plan for Unreal too; the shared data and the Python tests keep the two in step.
- Whether throwaway Unreal prototypes may use Blueprints is still open: TECH-002 allows Blueprints only for content
  wiring and UI. Changing that needs the owner's word and an amendment here or in a new ADR.
- When the freeze happens, `Prototype/play/README.md` and CLAUDE.md's rule to run `build_play.py` after a map change
  should be updated to match.
