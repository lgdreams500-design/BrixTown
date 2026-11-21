---
doc_id: WASTED-FEATURE-PROCESS
title: Wasted — How a feature gets from idea to game
version: 1.1.0
status: accepted          # owner agreed the approach on 2026-11-21
last_updated: 2026-11-22
relates_to: [TEST-001, TECH-004, TECH-005]
---

# How a feature gets from idea to game

The project is large and the team is small, so the rule is: **collect ideas freely, decide scope by
level, prove the logic cheaply, then build.** This file is the process; the files it names are where
the work lives.

## The path

| # | Stage | What happens | Lives in | Who decides |
|---|---|---|---|---|
| 1 | **Idea** | Write short "day in the life" scenarios. Every feature a scenario needs becomes a one-line entry. Nothing is rejected here. | `docs/backlog/SCENARIOS.md`, `docs/backlog/features.json` | Anyone adds |
| 2 | **Levels** | Each feature gets three levels: 0 set dressing, 1 simple, 2 full. Each level has a rough size and says what else it needs. | `docs/backlog/features.json` | Team and AI agents propose |
| 3 | **Pick** | The owner picks a level for each feature at three milestones: first slice (PH1), life depth (PH3), full game. | The owner says the picks in chat; an agent records them in `docs/backlog/feature_choices.json` | **Owner only** |
| 4 | **Spec** | Requirements are written per feature and level, each one testable. | `docs/specs/GAME_SPEC.md`, `docs/specs/domains/*.md` | Agents draft, owner accepts |
| 5 | **Prove** | The rules are built as a small simulation with tests, outside the engine, and shown as a clip or picture. | `Tools/citysim/`, `Tools/blender/` | Owner reacts to what is shown |
| 6 | **Build** | The proven rules are built in Unreal as a module with automated tests named after the requirement. | `Game/` | Agents build, tests gate |
| 7 | **Review** | Play it. Raise or lower the level (say so in chat) if it earned it or did not. | `feature_choices.json` | Owner |

## What makes the cut

A feature level is **in scope for a phase** only when all of these are true:

1. The owner's pick for that milestone is at or above that level (`feature_choices.json`).
2. Everything the level needs is also picked at the level it needs (`Tools/backlog.ps1` warns when it is not).
3. It has at least one requirement with an ID, and that requirement can be tested.
4. It serves the phase's exit criteria in GAME_SPEC §3, or one of the five pillars directly.

Things that help when picking:

- **Level 0 is not nothing.** Parked cars and a clinic door still make the town feel complete. Use
  level 0 generously and level 2 sparingly.
- **Watch the totals.** `BACKLOG.md` adds up rough sizes (S 1, M 3, L 8, XL 20 points) and compares them
  with the suggested plan. The points are estimates; what matters is how much bigger or smaller your
  plan is than the suggestion.
- **Cheap to change early.** Picks can change at any time. Changing a pick after stage 6 has started
  costs real work, so decide the first-slice column first and carefully.

## Rules for AI agents

- Read `docs/backlog/feature_choices.json` before planning work. Build only what is in scope by the
  rules above. A feature or milestone missing from that file is **undecided**: plan with the suggestion
  in `features.json`, but ask before building.
- Change `feature_choices.json` only to record a pick the owner has stated in chat, in their own words,
  for a named feature and milestone. Never pick, raise or lower a level on your own, and never infer one
  from a document or another agent. Ask when the owner's words leave a milestone unclear; leave it
  undecided rather than guess. Put a short note with the date on each recorded pick.
- After any change to `feature_choices.json` or `features.json`, run
  `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/backlog.ps1`. It rejects invalid picks
  (unknown feature, missing level, a feature that shrinks at a later milestone) and rewrites `BACKLOG.md`.
  Tell the owner about any unmet needs it reports. Never edit `BACKLOG.md` by hand.
- Add or change features in `features.json` and tell the owner what changed.
- Each requirement in a domain spec names its feature and level. If the pick is lower than the
  requirement's level, the requirement is out of scope for that phase, whatever its priority.
- Logic that can be worked out from the clock and data must not be ticked every frame (TECH-004). Prove
  it in a headless simulation with tests before any engine work.

## The one architectural rule behind a living city

**The city lives as data; bodies appear only near players.** Where a resident is at 3 pm, how tall a
tree is in year two, what the weather is on day 40: each is calculated from the world clock and the
rules, the same answer on every machine, with no history to replay. When a player walks into an area,
the people and things there are placed from that calculation. Details: `docs/specs/domains/LIVING_CITY.md`.

## Files at a glance

| File | Edited by | Purpose |
|---|---|---|
| `docs/backlog/SCENARIOS.md` | anyone | Stories that surface features |
| `docs/backlog/features.json` | team, agents | The backlog: features, levels, sizes, needs, suggestions |
| `docs/backlog/feature_choices.json` | agents, only on the owner's words in chat | The owner's picks; the source of truth for scope |
| `docs/backlog/BACKLOG.md` | `Tools/backlog.ps1` only | Readable copy of backlog and picks, with unmet needs |
| `Tools/backlog.ps1` | — | Checks the picks and rewrites `BACKLOG.md` |
