---
adr: ADR-0075
title: A place's light settings on the reference page are pictures taken in the game; one Blender picture says what the place is
status: accepted          # the owner in chat, 2026-11-28: "devise a plan to convert all the references in the index.html to unreal based reference. like a script or something which will update or create a fresh index.html that will have single image for blender and light settings references built in unreal. what settings we need for each area is already there in index.html." and "once you have plan ideally should run without requiring llm or tokens, start a new session and instruct it to keep merging changes area by area and update index.html area by area so I can check without stopping the session which will be running autonomously."
date: 2026-11-28
relates_to: [ADR-0004, ADR-0058, ADR-0061, ADR-0066, ADR-0073, ADR-0074, ART-001]
---

## Context
The reference page (`Art/Reference/index.html`, ADR-0061) shows 14 places at every light setting (morning, midday,
dusk, the nights, the weathers): 92 pictures, all made in Blender. ADR-0073 decided that a room's light is set and
approved in Unreal, because a Blender picture and the game are different rooms lit by different recipes, and matching
one to the other by number failed over a day of trials. That decision was about rooms only. A golden room and its
hallway were then built in Unreal.

The owner then asked for the same for every place on the page (the words are in the status line above): each place
once in Blender, and its light settings as pictures built in Unreal, made by a script that needs no agent to run,
landing area by area so the page can be read at any time.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Keep every setting in Blender | Nothing to build | The page shows a light the game does not have and, for rooms, is no longer matched to (ADR-0073) |
| Replace the Blender files with game pictures | One set of files | Breaks ADR-0004 (never overwrite a reference); loses what the owner approved |
| **Keep the Blender files; the page shows one of them for what the place is, and the settings as pictures taken in the game, in a folder of their own** | What the owner asked for; nothing approved is lost; the game's light is seen as it is, where it is judged | Two sets of pictures until the game's are approved; the game cannot show every setting yet |

## Decision
- **Each place with light settings shows one Blender picture for what it is** (its Midday, or its first), with what it
  locks and its decision as before, **and its settings as pictures taken in the game**, one a setting. This holds for
  every such place: the rooms, **and out of doors, the shops and the mall** (ADR-0073 was rooms only).
- **The data is one `unreal` block a reference in `Art/Reference/catalog.json`**: where the camera is (`where`, a tour
  stop's place), the one Blender picture (`blender`), the hour of the game each setting is shot at (`settings`), the
  settings the game cannot show and why (`missing`), what is seen wrong in the pictures (`issues`). Every setting of
  the reference is either shot or listed as missing: none is dropped without a word.
- **One script makes them, with no agent**: `python Tools/unreal/reference_shots.py --area <Id>` or `--all`. One run
  of the game for everything asked; the pictures go to `Art/Reference/<Category>/Unreal/<Id>_<Setting>.jpg`, their
  numbers (the look check, the grey card where there is one, frames a second) to `<Id>.json` beside them, and the page
  is written again. After any change to the light the same command shoots everything again.
- **The `Unreal/` folders are the script's to write.** ADR-0004's "never overwrite files in `Art/Reference/`" holds
  for every Blender file and for a game picture the owner has approved: an approved picture is replaced only when the
  new shot differs from it, and the block then goes back to "for your review" with its old decision kept as `was`.
- **Nothing is approved by the script or by an agent.** Every game picture arrives as `status: proposed` and is drawn
  as "for your review". Only the owner's words in chat make a block `approved`, recorded as its `decision` with the
  words quoted; the catalog's check refuses an approved block without them.
- **A setting the game cannot show is "not in the game yet"**, with its reason, never another hour standing in: the
  moon nights (no moon phases, WLD-042), the fire-out nights (no fire is lit).
- **The pictures show the game as it is.** Converting an area tunes no light, material or model. What is wrong beside
  the Blender picture is written in the block's `issues` and in `Game/README.md`, "Not yet"; mending it is separate
  work the owner asks for.
- The plan and the order of the areas: `docs/process/UNREAL_REFERENCES.md` (accepted with this ADR).

## Consequences
- **No Blender picture is retired, moved or changed**, and no reference's lock changes. Until the owner approves a
  place's game pictures, that place is judged as before: a room by ADR-0073 (no measured target yet), a place out of
  doors by its Blender reference (LOOK_GATE, the matching round).
- **Not decided here, the owner's to say once the game pictures of a place are approved:** whether an approved game
  picture then replaces the Blender picture as what a place out of doors is measured against, and whether the Blender
  settings not shown are then retired.
- The catalog's rules gain a second kind of file (`<Category>/Unreal/<Id>_<Setting>.jpg`, listed only in an `unreal`
  block, never among a reference's own files); `Tools/reference_catalog.py` checks both and
  `Tools/test_reference_catalog.py` tests them.
- A reference whose game pictures wait for the owner is listed first in its tab, marked "Its light in the game: for
  your review", so what is new is found without searching.
- The game's pictures are taken at High, 1066 x 666, the look tour's size: they are smaller than the Blender ones
  (1280 x 800) and show the game's frame rate at that size on the owner's laptop.
- The page will show, plainly, where the game is not yet its reference. That is the point of it.
