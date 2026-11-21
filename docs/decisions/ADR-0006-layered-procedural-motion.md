---
adr: ADR-0006
title: Body motion — one skeleton, data-driven movement signatures, independent layers
status: accepted
date: 2026-11-21
relates_to: [ANIM-001, ANIM-002, ANIM-003, ANIM-004, ANIM-005, ANIM-007, ANIM-009, ANIM-010, NEED-003, Q-15, Q-17]
---

## Context
The owner wants body animation with a character of its own, and wants NPCs and player characters to
talk while walking, moving their heads naturally. The game also has a character designer, creator
content, many NPCs on screen, multiplayer, and a simulation of needs and weather that should be
visible on characters.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Hand-made or bought clips for every case (walk, walk-and-talk, tired walk, …) | Highest polish per clip | Combinations multiply; every body shape and creator item must be checked against every clip; little individuality |
| Motion matching on a large capture set | Very natural | Needs a big, costly data set; heavy for the target hardware and for many NPCs; hard to stylise |
| **One skeleton; walking shaped by a data "movement signature"; state layers; separate layers for legs, chest and arms, head and eyes, face** | Individual walks at no asset cost; state readable in the body; talking while walking falls out of the layering; cheap to replicate (send data, not poses); walk styles become designer and creator content | Needs careful tuning to avoid a robotic feel; starts, stops and turns still need work; less nuance than capture |

## Decision
Approved by the owner on 2026-11-21 from the two test clips (`Art/Reference/motion_walks.mp4`,
`motion_walktalk.mp4`). Adopt the third option as the direction, as prototyped in `Tools/blender/motion_lib.py` and described
in `docs/art/CHARACTER_MOTION.md`. Whether the leg cycle itself is fully procedural or built on a few
authored clips that the signature adjusts is left open (Q-17).

## Consequences
- The production skeleton uses the joint names in CHARACTER_MOTION §2 from day one.
- Signatures and state layers are data assets; the needs, mood and weather systems output state
  weights for animation.
- Multiplayer replicates signature id, state weights, look target and speech state only.
- Long coats and dresses need skirt joints or cloth (ANIM-008); the prototype cannot do them.
- The motion maths gets unit tests (foot error, knee range, speed), like other simulation code.
