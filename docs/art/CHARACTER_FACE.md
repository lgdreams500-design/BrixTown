---
doc_id: WASTED-CHARACTER-FACE
title: Wasted — Face controls, expressions and lip sync
version: 1.0.0
status: accepted          # prototype approved by the owner on 2026-11-21
last_updated: 2026-11-21
relates_to: [CHR-007, CHR-008, CHR-009, CHR-010, CHR-011, CITY-007, Q-14, Q-15]
decision: docs/decisions/ADR-0005-face-control-set.md
prototype: Tools/blender/character_face_05.py
---

# Wasted — Face controls, expressions and lip sync

Every head in the game, player or NPC, is animated through **one small set of named controls**.
An expression, a blink or a spoken sound is nothing more than a set of values for those controls.
Because the controls are the same on every head, a face made in the character designer, or hair and
accessories made by a creator, animate with no extra work (CHR-007).

The prototype is `Tools/blender/character_face_05.py`: `pose(rig, values)` is the only function that
moves a face. The production rig must expose the same controls, whatever it is built from (morph
targets, bones or both).

Reference images: `Art/Reference/face_expressions.png`, `face_mouth_shapes.png`, `face_talk.mp4`.

## 1. The control set

Distances are in metres on a head of natural size, before the 1.22 head scale. `+` up or outward.

| Control | Type | Rest | Range used | What it does |
|---|---|---|---|---|
| `browL`, `browR` | (inner, outer) offsets | (0, 0) | −0.010 … +0.010 | Raise or lower each end of each brow. Inner down = frown, inner up = worry, both up = surprise. |
| `lid` | 0–1 | 0 | 0 open … 1 closed | Upper eyelid. Used for blinks and heavy eyes. |
| `squint` | 0–1 | 0 | 0 … 0.5 | Lower edge of the eye rises (narrows the eye from below), as in a real smile. |
| `open` | scale | 1.0 | 1.0 … 1.15 | Eye opening. Above 1 = wide eyes. |
| `gaze` | (x, z) offset | (0, 0) | about ±0.004 | Where the iris sits on the eye. The iris and pupil always stay on the eye surface. |
| `mouth` | shape name or values | `rest` | see §2 | Width, opening, roundness, which teeth and whether the tongue show. |
| `corner` | offset | from shape | −0.003 … +0.004 | Mouth corners up or down. |
| `curve` | −1 … +1 | from shape | −0.6 … +0.65 | Overall smile (+) or frown (−) curve. |

### Eye construction notes (these fixed real problems; keep them)
- Round eye, large iris resting near the lower lid, outer corners tilted up about 6°. Earlier flatter
  eyes with a visible lower lid made **every** expression look sad.
- A squint narrows the eye itself. Do not add a separate lower-lid shape; it showed as a ledge.
- The upper lid only appears once `lid` ≥ 0.03, so open eyes have a clean arched lash line.
- Brows sit clear of the eye (0.0445 above eye centre) with the outer end slightly higher.

### Mouth construction notes
- Lips are a ring; the dark inside, **upper teeth, lower teeth and tongue all sit in the plane of
  the opening**. The face slopes back below the mouth, so anything placed "inside the head" is hidden.
- Mouths are drawn larger than life (width × 1.3, opening × 1.25) so speech reads at a distance.
- Teeth and tongue hide automatically when the opening is too small to show them.

## 2. Speech mouth shapes (visemes)

Eight shapes cover English speech at the level of detail the style needs.

| Shape | Sounds | Width | Opening | Round | Shows |
|---|---|---|---|---|---|
| `rest` | silence | 0.0175 | 0.0011 | – | closed, slight smile |
| `MBP` | m, b, p | 0.0150 | 0.0006 | 0.5 | lips pressed |
| `AA` | a, ah, "father", "cat" | 0.0165 | 0.0115 | – | upper teeth, tongue, lower teeth |
| `EH` | e, "bed", "a" (schwa) | 0.0185 | 0.0075 | – | upper teeth, tongue, lower teeth |
| `EE` | ee, i, s, z, t, d, n, k, g | 0.0215 | 0.0048 | – | both rows nearly closed, thin gap |
| `OH` | o, aw | 0.0110 | 0.0105 | 0.8 | round, no teeth |
| `OO` | oo, w, r | 0.0070 | 0.0060 | 1.0 | small and round |
| `FV` | f, v | 0.0160 | 0.0045 | – | upper teeth only |
| `smile` | after a line | 0.0200 | 0.0012 | – | closed smile (not a speech sound) |

## 3. Expression presets

| Expression | Mouth | Brows (inner, outer) | Eyes | Notes |
|---|---|---|---|---|
| Neutral | `rest` | (0, 0) | open | |
| Happy | wide, open 0.006, upper teeth; curve +0.55 | (+0.004, +0.005) | squint 0.5 | |
| Sad | narrow, closed; curve −0.6 | (+0.008, −0.006) | lid 0.30, gaze down | The only preset that lowers the lid this far. |
| Angry | open 0.0034, both rows clenched; curve −0.35 | (−0.010, +0.002) | lid 0.14, squint 0.3 | |
| Surprised | `OH` | (+0.010, +0.008) | open 1.15 | |
| Thinking | small, closed, slightly round | left (−0.003, −0.002), right (+0.008, +0.006) | squint 0.2, gaze up and aside | Asymmetric brows. |

Expressions blend with speech: the expression sets brows, lids and the mouth curve; the speech shape
sets width and opening.

## 4. Lip sync

### How a line becomes mouth movement
1. Split the line into words, words into sounds, sounds into the shapes of §2.
2. Give each shape a length: about 2 frames at 12 per second (≈ 0.17 s); 1 frame for quick sounds;
   3 for stressed open vowels. Insert `rest` for pauses (≈ 0.4 s at a full stop).
3. Play the shapes in order. End a line on `smile` or `rest`.
4. Layer on top: a blink every few seconds (3 frames: half, closed, half), brows up slightly at the
   start of the line and more on a question.

The reference line "Welcome to Brix Mart. Fancy a coffee?" is the `SEQ` table in the prototype
(55 frames at 12 per second).

### Where the timing comes from — `[OPEN: Q-14]`
| Source | Works when | Cost |
|---|---|---|
| **Text (default)** | Dialogue is text, or text plus synthetic voice | Cheap; runs for every NPC; needs a small pronunciation table |
| Audio | Dialogue is recorded voice | Needs analysis per line (offline) or a runtime plug-in |

The control set is the same either way, so the choice can be made later without redoing faces.

## 5. Always-on face life (CHR-010)
- Blink every 2–6 s, and usually **during a head turn or gaze shift**.
- Small gaze shifts every few seconds when idle.
- Slight head motion while speaking (see `docs/art/CHARACTER_MOTION.md`).

## 6. Performance and distance (CHR-011)
Full controls only for characters in a conversation or close to the camera; mouth open/close only at
mid distance; nothing far away. Budgets to be set from PH1 profiling.

## 7. Moving this into production
| Prototype | Production (Unreal) |
|---|---|
| `pose()` values | Animation curves with the same names on the head skeletal mesh |
| Mouth shapes | Morph targets (or a few jaw and lip bones) named `VIS_rest`, `VIS_MBP`, … |
| Expression presets | Data assets holding curve values, blended by game state |
| `SEQ` table | Generated at runtime from the line's text, or baked per line from audio |
| Designer values (eye size, face width…) | Separate morphs that do not change the control names |

Rule for creators: hair, hats and glasses attach to the head bone and must not cover the brows, eyes
or mouth controls' range of movement.

## 8. Change log
| Version | Date | Change |
|---|---|---|
| 1.0.0 | 2026-11-21 | First version, after the eye fix (no more sad default) and the teeth fix (lower teeth and tongue visible). |
| 1.0.1 | 2026-11-22 | The browser slice now uses §1 to §4 for the resident you talk to: values in `World/rules/speech.json`, faces in `Prototype/play/people.js`, timing from the line's text in `Prototype/play/conversations.js` (Q-14 decided: text first). |
