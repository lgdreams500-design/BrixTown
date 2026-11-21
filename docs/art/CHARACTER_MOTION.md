---
doc_id: WASTED-CHARACTER-MOTION
title: Wasted — Body motion, movement signatures and talking while walking
version: 1.0.0
status: accepted          # owner approved the two test clips on 2026-11-21; Q-17 is still open
last_updated: 2026-11-21
relates_to: [ANIM-001, ANIM-002, ANIM-003, ANIM-004, ANIM-005, ANIM-006, ANIM-007, ANIM-008, ANIM-009, ANIM-010, ANIM-011, CHR-007, CHR-009, NEED-003, Q-15, Q-17]
decision: docs/decisions/ADR-0006-layered-procedural-motion.md
prototype: [Tools/blender/motion_lib.py, Tools/blender/character_anim_06.py]
---

# Wasted — Body motion

How people in Wasted move, and what makes that movement the game's own. The prototype is
`Tools/blender/motion_lib.py` (plain Python, no Blender code: the numbers and rules are meant to
carry over to Unreal) and `Tools/blender/character_anim_06.py` (builds the jointed test body and
renders the clips).

Approved clips: `Art/Reference/motion_walks.mp4` and `Art/Reference/motion_walktalk.mp4`
(originals and frames in `Art/StyleTests/character_anim_06_*`). What is approved is the way people
move; the rigid test body and the limits in §5 and §6 are not the target.

## 1. The ideas

| # | Idea | What it gives the game |
|---|---|---|
| M1 | **Movement signature.** Every body walks with the same system, shaped by about 25 numbers. A "walk" is data, not an animation file. | Each resident can move differently at no asset cost. A walk style can be a character-designer choice, an NPC trait, or something a creator sells. |
| M2 | **State shows in the body.** Needs, mood and weather nudge the numbers through *state layers*. | You can read that someone is tired, happy or cold from across the street, before any icon. Ties to NEED-003. |
| M3 | **Layers, not combined clips.** Legs, chest and arms, head and eyes, and face are separate layers. | Anyone can talk, look around and gesture while walking. No "walk and talk" animation has to be made. |
| M4 | **Eyes lead, head follows, chest last.** A look starts in the eyes, the head catches up with a slight overshoot, the chest turns a little; there is a blink on the way. | Heads look alive and attentive instead of swivelling like turrets. |
| M5 | **Feet first.** Feet are placed on the ground, then the legs are solved to reach them. | No foot sliding at any speed or body size. |
| M6 | **Soft, weighted, slightly bouncy.** Hips dip as weight lands and rise over the standing leg; knees stay a little bent; the head arrives late. | Matches the soft clay-like look of the characters (ART_DIRECTION §3). |

## 2. Skeleton

One skeleton for every person. Joint names follow the Unreal mannequin so the mapping is direct.

```
root
└─ pelvis
   ├─ thigh_l ─ calf_l ─ foot_l
   ├─ thigh_r ─ calf_r ─ foot_r
   └─ spine                      (waist: the chest turns against the hips here)
      ├─ neck ─ head             (the neck takes 38 % of any head turn, the head the rest)
      ├─ upperarm_l ─ lowerarm_l ─ hand_l
      └─ upperarm_r ─ lowerarm_r ─ hand_r
```

Conventions in `motion_lib.py`: the character faces −Y, +Z is up, +X is the character's left; head
yaw + is to the left, pitch + is a nod down, roll + tilts to the left shoulder.
Rest positions come from the designer values (`skeleton(sp, cs)`), so height and build change the
skeleton's proportions, never its structure.

The test body is the option B character cut into rigid parts at the knees, elbows, waist and neck.
Production needs a skinned mesh on this skeleton (Q-15).

## 3. Movement signature

`signature(name, states, **overrides)` builds the full set. Angles in degrees, lengths in metres.

| Number | Base | Meaning |
|---|---|---|
| `cycle` | 24 | Frames per two steps at 24 fps. 24 = 120 steps a minute. |
| `stride` | 0.84 | Step length as a fraction of leg length. Speed = 2 × stride × leg ÷ cycle time. |
| `duty` | 0.62 | Part of the cycle each foot is on the ground. |
| `fore` | 0.42 | Share of the planted foot's travel that is in front of the hip. |
| `bounce` | 0.024 | Up-and-down of the hips (half range). |
| `sway` | 0.018 | Side-to-side of the hips (half range). |
| `hip_yaw`, `hip_roll` | 6, 3.5 | Hips turning with the stepping leg; hip dropping on the swinging side. |
| `counter` | 1.0 | How much the chest turns against the hips. |
| `lean` | 3 | Forward lean of the chest. |
| `width`, `toe_out` | 0.75, 7 | How far apart the feet land; feet turned outwards. |
| `lift` | 0.085 | How high the foot is carried. |
| `strike`, `toe_off` | 16, 42 | Toes-up angle at heel strike; toes-down angle as the foot leaves. |
| `arm_swing`, `arm_bias`, `arm_out` | 18, 2, 2.5 | Arm swing; arms carried forward or back; arms held away from the body. |
| `elbow`, `elbow_swing` | 16, 14 | Resting elbow bend; extra bend as the arm comes forward. |
| `arm_l`, `arm_r` | 1, 1 | Scale of each arm's swing (a hand on a bag strap, a carried load). |
| `head_pitch`, `head_bob` | 0, 1.2 | How the head is carried (+ looks down); nod with each step. |
| `head_follow`, `head_lag` | 0.25, 0.06 | How much of the body's turning the head follows; how late it arrives. |

### The four test signatures
| Signature | Shown on | Steps a minute | Speed on that body | Character |
|---|---|---|---|---|
| **Springy** | Player (woman, 1.68 m) | 120 | 1.23 m/s | Big bounce, high step, swinging bent arms, chin up. |
| **Easy** | Player (man, 1.80 m) | 96 | 1.14 m/s | Long loose stride, rolling hips, feet turned out, arms hanging. |
| **Hurried** | Courier | 144 | 1.68 m/s | Leaning in, narrow quick steps, elbows bent, one hand still on the bag strap. |
| **Careful** | Elder | 96 | 0.58 m/s | Short flat steps, feet down longer, small arm movement, eyes on the ground ahead. |

### State layers
Applied on top of any signature, scaled 0–1 by how strong the state is. `x` multiplies, `+` adds.

| State | Changes |
|---|---|
| **Tired** | cycle x1.18, stride x0.80, bounce x0.6, lift x0.65, arm swing x0.55, lean +6, head pitch +10, sway x1.3, toe-off x0.7 |
| **Happy** | cycle x0.94, bounce x1.45, arm swing x1.3, lift x1.2, head pitch −5, lean −2 |
| **Cold** | cycle x0.92, stride x0.88, arm swing x0.3, elbow +45, arms in, lean +5, head pitch +6 |

These three are a first set; the list is expected to grow with the needs and weather systems. They
are defined but **not yet shown in a clip**.

## 4. Layers for talking while walking

`walk(sig, skeleton, phase, speed, mods)` takes optional layers in `mods`:

| Layer | Input | Used for |
|---|---|---|
| Legs and hips | signature, phase, speed | Always running. When two people walk together both are given the same speed and keep their own step rhythm. |
| Chest | `chest_yaw`, `chest_pitch` | Turning the upper body a little towards whoever is being spoken to. |
| Arm | `arm_l` / `arm_r` = (weight, raise, out, elbow) | Blending an arm from its swing into a gesture pose and back. |
| Head | `head` = (yaw, pitch, roll) | Looking at a person or thing; nods; head shake. Added to the walk's own head carriage. |
| Eyes and face | face controls (CHARACTER_FACE.md) | Gaze, blinks, brows, mouth shapes. |

### Rules used in the test (the "conversation layer")
| Rule | Value in the test |
|---|---|
| Partner walking alongside is 90° to the side. The head takes about half, the chest a little, the eyes the rest. | head 44°, chest 12°, eyes up to their limit |
| The head follows its target like a spring: quick start, small overshoot. | 1.5 Hz, damping 0.72; chest 0.9 Hz |
| Eyes cover what the head has not turned yet, so they arrive first and re-centre as the head catches up. | gaze = remaining angle ÷ 34°, clamped |
| Blink at the start and end of each look. | 5 frames |
| Speaker looks at the listener to open and to hand over; looks down the road in between. | see timeline |
| Speaker nods slightly on each stressed word; brows lift on key words and stay up while waiting for an answer; head tilts on a question. | nods 2–3.6°, brows +4–6 mm, tilt 5° |
| A hand joins in on the question and beats on stressed words, then drops back into the swing. | weight 0→1→0, elbow 78° + beats |
| Listener looks at the speaker during most of a line, shows the reaction in brows and mouth, and can shake or nod. | shake ±9°, 2.5 a second, 0.8 s |
| A head is never perfectly still. | slow drift of 1–2° on all three axes |

### The test line (128 frames, 5.3 s)
| Frames | Speaker (taller, on the wall side) | Listener |
|---|---|---|
| 0–11 | looks ahead, then turns to her | looks ahead |
| 12–47 | "Rent is due on Friday." | turns to him at 20 |
| 48–55 | pause, looks down the road | looks away at 44, worried |
| 56–87 | turns back: "Got any brix left?" (hand comes up) | glances at him 70–92 |
| 88–111 | waits, brows up | "Not a chip." with a head shake |
| 112–127 | amused, looks ahead | turns to him, grins |

## 5. Clothing longer than the hips

One-piece tunics, coats and dresses are widened at the hem, in front or behind, just enough to clear
the legs (`hem_scales`), and fall back gradually. This works for a hip-length tunic. It is **not
enough** for a long coat over a dress: a knee still shows through. For the walk test the Elder wears
a tunic and trousers instead of her coat. Production needs skirt joints or cloth simulation
(ANIM-008).

## 6. What the prototype does not do yet

| Missing | Note |
|---|---|
| Starting, stopping, turning, running, stairs, slopes | Only steady walking in a straight line. |
| Standing idle, sitting, interacting with objects | Not started. |
| Skin that bends | Parts are rigid; joints are balls. |
| Hands and fingers | Hands are single shapes. |
| Carrying, phones, umbrellas | `arm_l` / `arm_r` and the arm layer are the hook for these. |
| Reacting to each other's path (keeping pace, giving way) | Pair speed is simply set equal. |
| State layers on screen | Defined in data, no clip yet. |
| Long coats and dresses | See §5. |

## 7. Moving this into Unreal

| Prototype | Production |
|---|---|
| Joint names | Same names on the character skeleton |
| `walk()` legs and hips | Either authored walk clips adjusted by the signature (stride warping, additive hip and chest layers), or fully procedural in Control Rig. Open: Q-17. |
| Foot placement | Foot IK with the planted-foot rule; step length from actual speed |
| Chest, arm, head layers | Layered blend per bone in the Animation Blueprint; look-at in Control Rig |
| Spring-follow for head and chest | Spring interpolation node with the same frequency and damping |
| Signature and state layers | Data assets; states blended by the needs, mood and weather systems |
| Conversation rules | A small component that takes "speaking / listening, partner, stressed words" and outputs the layer inputs |
| Multiplayer | Send only signature id, state weights, look target and speech state; each client computes poses (ANIM-009) |
| `motion_lib.py` checks | Unit tests on the motion maths (ANIM-011): foot error, knee range, speed |

## 8. How to reproduce

1. Blender 5.3, Python console, run `character_anim_06.py` (see ART_DIRECTION §4). About 40 minutes
   on the project laptop for both clips (14 for the walks, 26 for the dialogue). Set `FRAMES` at the top to a few frame numbers for a check
   that takes under a minute.
2. Frames are written to `Art/StyleTests/character_anim_06_walks/` and `..._walktalk/`.
3. `motion_lib.py` runs without Blender: `walk()`, `forward_kinematics()` and `hem_scales()` can be
   called and tested from any Python.

## 9. Change log
| Version | Date | Change |
|---|---|---|
| 0.1.0 | 2026-11-21 | First prototype: jointed body, four signatures, three state layers (data only), talking while walking. |
| 1.0.0 | 2026-11-21 | Owner approved both clips. Status accepted; clips copied to `Art/Reference/`. |
