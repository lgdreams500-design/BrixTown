---
adr: ADR-0008
title: The owner states feature picks in chat; the browser picker is removed
status: accepted        # owner's instruction, 2026-11-22
date: 2026-11-22
relates_to: [CITY-005, CITY-042]
---

## Context
Feature levels were picked in `Tools/feature_picker.html`, which saved into the project folder through the
browser's folder-access dialog. That proved awkward: it needs Edge or Chrome, a folder grant on every visit,
and it did not work from the Claude desktop app's browser pane. The owner asked for it to be removed and for
picks to be made in chat instead.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Keep the picker | Clickable overview, live totals | The folder dialog got in the way; picks never landed |
| Picker downloads a file the owner moves by hand | No folder grant | Still a separate tool and an extra step |
| Owner states picks in chat, an agent records them | No tool to run; picks happen in the conversation where work is planned | An agent writes the scope file, so the rules on what counts as a pick must be strict |

## Decision
The owner says picks in chat. An agent writes them into `docs/backlog/feature_choices.json` only from the
owner's own words for a named feature and milestone, adds a short dated note, and runs `Tools/backlog.ps1`.
That script rejects invalid picks (unknown feature, missing level, a feature that shrinks at a later
milestone), lists needs that are not picked yet, and rewrites `docs/backlog/BACKLOG.md` with the same totals
the picker showed. Agents still never pick, raise or lower a level on their own; an unclear answer stays
undecided and the agent asks.

## Consequences
- `Tools/feature_picker.html` and its preview image are deleted (still in git history).
- `BACKLOG.md` is the readable overview, including the plan's size against the suggestion.
- A need now counts only when it is actually picked, not when the suggestion happens to cover it, which
  matches FEATURE_PROCESS rule 2.
- The first picks recorded this way (2026-11-22): `F-VEH-DRIVE` 2 Cars and `F-VEH-TRAFFIC` 2 Real traffic at
  every milestone, and `F-MAP-CITY` 1 at life depth and 2 in the full game. This also answers Q-08.
