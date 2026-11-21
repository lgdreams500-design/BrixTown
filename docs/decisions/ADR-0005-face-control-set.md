---
adr: ADR-0005
title: One shared face control set; lip sync from mouth shapes
status: accepted
date: 2026-11-21
relates_to: [CHR-007, CHR-008, CHR-009, CHR-010, CHR-011, CITY-007, Q-14]
---

## Context
The owner asked for facial animation and lip movement during dialogue. Faces are also customised in
a character designer and extended by creators, and there may be many NPCs on screen.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Per-character hand animation | Best quality per face | Does not scale to a designer, UGC or hundreds of NPCs |
| MetaHuman faces and tooling | High quality, existing tools | Realistic proportions conflict with ADR-0004; heavy for the target hardware; runtime audio-driven lip sync needs extra plug-ins |
| **One small control set shared by every head; expressions and speech are values for it** | Works for any designer face and creator item; cheap; easy to level-of-detail | Less nuance than a film rig; needs a bespoke head built to the control set |

## Decision
Every head exposes the control set in `docs/art/CHARACTER_FACE.md` §1. Speech uses eight mouth shapes
(§2) played in sequence; expressions are presets (§3) blended with speech. Timing comes from the
line's text by default; audio-driven timing can be added later without changing the controls (Q-14).

## Consequences
- The production head must be modelled with these controls from the start (input to Q-15).
- Creator hair and accessories have a rule: do not block brows, eyes or mouth.
- Two fixes found in prototyping are now requirements of the head model: eyes must not read sad at
  rest, and upper teeth, lower teeth and tongue must be visible in open mouth shapes.
