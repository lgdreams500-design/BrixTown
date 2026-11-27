---
doc_id: WASTED-LOOK-GATE
title: Brix Town — The look gate: how anything seen in the game is checked when it is made
version: 1.2.0
status: accepted          # the owner, 2026-11-26: "build a process where this is taken care as part of creations instead of waiting for audit to detect it later"
last_updated: 2026-11-27
relates_to: [ADR-0004, ADR-0058, ART-001, TEST-001]
---

# The look gate

**Why this exists.** The Unreal build's Eastgate Mall was built, tested and merged with every face glowing in one
plain colour: no shade, no shadow, no mottled paint, burnt whites, and at night the player a black shape in it. Its
tests passed, because they test rules, and nobody put a picture of it beside the look. The owner found it by playing.
The same day the cars had been drawn inside out for days. A thing that is seen is not done until it has been looked
at as a whole, against the look, by whoever made it.

## What it is measured against
1. `Art/Reference/` (`catalog.json` says which reference covers what) and the rules of `docs/art/ART_DIRECTION.md` (W1 to W8 for the world, V for vehicles, I for
   interiors, C for people, L and U for screens). These are the bar.
2. **Not the browser slice.** The slice was a prototype (the owner, 2026-11-26: "slice was just a prototype, you need
   to aim even better than slice in the unreal"). "As the slice draws it" describes where a rule or a number came
   from; it is never a reason a picture is good enough. A shortcut the slice took (rooms that glow by themselves, no
   shadows, one flat colour) is a defect when it is carried into Unreal.
3. Where no approved reference shows that kind of thing (Eastgate's inside has none: ART_DIRECTION §5), it is not
   built yet: its reference is made and approved first (`docs/process/FEATURE_PROCESS.md` stage 6, ADR-0066). Work
   already in the game without one keeps to the rules and the nearest reference for light and materials (a hall
   is an interior: I5, W1, W5, W6) until its reference is approved, and says so in its README's "Not yet".

**Which reference.** Before any comparing, measuring or tuning, ask the catalog what the place is and what it is judged
against: `python Tools/reference_catalog.py --for <tour stop, lot id or place>` (`--tour` for every stop of a tour). It
answers from the catalog's `places` with the place's own reference and the file for the hour, or that the place has
none. Say both first ("the library: shop_floor, ShopsShopFloor_Midday"). A shot is put beside its own place's
reference and no other: a hallway is not judged by a shop, nor a shop by the bakery. Where 3 lets work with no
reference keep to the nearest one for light and materials, say that it is borrowed and from which place. When the owner
names a kind of place (a room, a hallway, a shop), that place's reference is the one meant: do not put another indoor
place in its stead. A new tour stop is added to its place's `stops` in the catalog (its test fails otherwise).

## When it applies
Any change that alters what is seen in the Unreal game: a new place or thing, a model, a material, a light, the sky,
the camera, a screen. It is part of stage 6 (Build) of `FEATURE_PROCESS.md`, done before the pull request is opened,
by the session that made the change. A change to something shared (the sun, the sky, the eye, a material many things
use, people's bodies) is checked everywhere, with the look tour.

## The gate, in order

### 1. Shots
Take pictures of the thing in the game (`Tools/unreal/light_tour.ps1`, or `-BrixShot=`), as a player meets it:
- from where a player stands, the player in the picture where they would be, and close enough to see surfaces;
- **at noon, at dusk (20:00) and at night (22:30 or 02:00)** for anything in the world; in a room, by day and by night;
- once with the HUD on (`-BrixShot=`): the engine's own warnings are drawn only there.

### 2. Beside the reference
Put each shot beside its own reference (Which reference, above) on one sheet (`Tools/unreal/shot_sheet.py`) and look at the **whole frame**, not
the thing you changed. Answer every line; a "no" is a defect to mend or to note (step 5).

| # | Look for | The rule |
|---|---|---|
| G1 | **Shade and depth.** Do undersides, recesses and the far side of things go darker? Does a thing standing on a floor darken the floor where it meets it? | W3, W5 |
| G2 | **Shadows.** Does what is lit cast a shadow, and do people? Is anything lit from nowhere (glowing by itself)? | W5, I5 |
| G3 | **Surfaces.** Is every large surface matte and mottled (two close tones blotched), never one flat fill, never glossy, never noisy with the engine's own blotches? | W1, W2 |
| G4 | **Colour.** Muted and warm; colour from a few dull accents. No cast over the whole picture that reads as a filter. | W4, W8 |
| G5 | **Whites and blacks.** Is there something to see in the brightest and the darkest parts? No burnt patches, no pools of plain black by day. | W5 |
| G6 | **People.** Are the player and the residents lit like the place they stand in: not black, not glowing, shaded on the side away from the light? | C, W5 |
| G7 | **Lamps and windows** give warm pools; at dusk and at night the place still reads. A window seen from inside shows the hour outside. | W6, W7 |
| G8 | **Whole and the right way round.** Nothing inside out, see-through, floating, sunk, missing a side, or cut by the camera. | W3 |
| G9 | **Morning, midday, dusk, night** all read, and only the light changes between them. | W7 |
| G10 | **Nothing over the picture**: no engine warning, no debug text. | |

### 3. Measured
Run `python Tools/unreal/look_check.py <shots>`. It measures burnt whites, crushed blacks, how much of the picture
is a flat fill, how much light and shade there is, warmth, and how the player's brightness sits against the picture
round them, and says what fails. It catches the gross failures (a black player, a white-out, a plain fill); it does
not see a missing shadow or a dull material. **Passing it is necessary, not enough**: step 2 is the gate.

For a shared change, run the look tour (`Tools/unreal/light_tour.ps1`, every stop of `light_tour.txt`) and
`look_check.py --tour <folder> --against Tools/unreal/look_baseline.json`: it also reports every stop whose numbers
have moved since the baseline. A move is either the change you meant (write the new baseline with `--write`, in the
same pull request, and say so) or a regression.

### 4. Kept
The sheet (shots beside references) goes in `Art/StyleTests/<name>_<nn>/` with a README line for each picture, and
the pull request shows it and answers G1 to G10 in a table: yes, or what is wrong.

### 5. What is not right yet
A "no" that is not mended in the same pull request is written in the "Not yet" line of its section of
`Game/README.md` and in the pull request, in plain words ("the hall's faces glow by themselves: no shade or shadow
in it"). It is never left for the owner to find.

### 6. A new kind of place gets a tour stop
Something new that a player can stand in or beside gets a stop in `Tools/unreal/light_tour.txt` (by day and by
night), so that every later shared change is looked at there too.

## For AI agents
- Do not write "checked", "verified" or "looks right" about anything seen unless steps 1 to 3 were done for it in
  this session, and say which shots.
- Read the shot before the claim: open the picture and go down G1 to G10. Looking only for the defect being fixed is
  how the mall and the cars were missed.
- Report what you see that is wrong even when nobody asked about it, and even when another session made it.
- Name the place and its reference (Which reference, above) before comparing, tuning or reporting. A number shared by
  many places (the rooms' light, the sun, a material) is set against the references of every kind of place it lights,
  never tuned on one of them: on 2026-11-27 the rooms' light was tuned on the bakery, a street-corner shop, when a
  home's room had been asked for, and the hallways and shop floors were then judged with no reference at all.

## What the first audit found (2026-11-26)
Looked at with this gate: the 64 stops of the look tour and the owner's own screenshots. Open items are in the "Not
yet" lines of `Game/README.md`; this is the list they came from.

| Where | G | What is wrong |
|---|---|---|
| Eastgate Mall and the furniture outlet, inside | G1 G2 G3 G5 | Every face glows by itself in one plain colour: no shade, no shadow, no mottle; whites burnt. People are lit by a light of their own with no form |
| A tower's hall | G6 G3 | The player is 2.4 stops under the hall: nearly black. Bare plain walls |
| Shops, the bank, the library, a cafe | G3 | Ceilings and floors carry the engine's own dark blotches (bounced light too coarse), which read as dirt |
| Row houses, houses | G3 G5 | Plain flat walls; the lamp burns a white patch on the wall nearest it |
| A room at night | G7 | A window seen from inside is a white pane at 22:30 |
| The spawn alley by day | G5 | The far end through the arch is a white-out; the sunlit patch on the paving is burnt |
| Trees | G3 G8 | Crowns are near black with bright green lines along their edges |
| Lawns, the mall's shell, open ground | G3 | One flat colour, no mottle |
| Rain, snow, fog | G4 | A teal cast over the whole picture |
| Riding a bus | G8 | The bottom of a tree's crown can be in the picture: crowns do not stop the camera |

Mended since (see `Game/README.md`, The look gate): the tree crowns, the window seen from inside at night, the player
in a tower's hall, the teal cast, the alley's white-out, and a first pass at Eastgate's hall. The rest is pending there.
