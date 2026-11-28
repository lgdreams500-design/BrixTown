# Unreal: the rooms' light against the rooms' own references — a round in progress (2026-11-28)

The first round of `docs/process/LOOK_GATE.md`, The matching round. **Not finished.** The first part below (to "Next")
is how it stood at trial C; **the second part, "Carried on, 2026-11-28", is where it stands now**: the game at trial K
with ceiling beams, and the hallway reference remade.

The owner saw trial C beside the references (`before_and_trial_c.jpg`) and took it on 2026-11-28: "it's not perfect
but a huge improvement ... merge this strategy so far and we can refine it further".
To carry on, read this page, then LOOK_GATE's matching round.

## The shared cause
By day the game lit every room with its lamps at full strength and one warm light of the room's own, the same with a
window or without, set against rooms the owner has since retired (`Game/README.md`, The look gate, Not yet). The
approved references (`IndoorHomeRoom`, `IndoorHallway`, `ShopsShopFloor`, `ShopsBakeryLight`) are lit by day by the
world's daylight, lamps low or out; the hallway, with no window of its own, is dim.

## What was changed (the trial, uncommitted values now committed here as work in progress)
- `BrixRooms.h/.cpp`: `RoomLampDay` (the lamps' share of their strength in full daylight) and `RoomFillColourDay`
  (the room's own light's colour by day; `RoomFillColour` stays the night's). Night is unchanged by design
  (`RoomFill` x `RoomFillNight` is still 0.18).
- `DefaultGame.ini`, trial C: `RoomLampDay=0.25`, `RoomFill=0.7`, `RoomFillNight=0.257`,
  `RoomFillColourDay=(1,0.9,0.8)`; `DayOpenIndoors` stays 2.4 (trial D would make it 1.5).

## The trials: game against each place's own reference at 12:00 (stops; brightness / warmth)
`Tools/unreal/look_match.py` on `Tools/unreal/reference_tour.txt`'s indoor noon stops. Only the bakery looks through
its reference's camera (its figures are the mean miss over 12 squares); the rest are whole pictures from other spots.

| Trial | lamps by day | room light | day colour | eye indoors | room | hallway | grocer | bank | library | bakery |
|---|---|---|---|---|---|---|---|---|---|---|
| before | 1.0 | 0.45 | (1,.75,.48) | 2.4 | +0.84 / +1.51 | +1.64 / +1.15 | +0.34 / +1.29 | +0.63 / +1.15 | -0.26 / +1.76 | 0.73 / 1.02 |
| A | 0.06 | 0.9 | (1,.9,.8) | 2.4 | +1.31 / +0.63 | +2.01 / +0.25 | +1.27 / +0.18 | +1.44 / +0.07 | +0.87 / +0.61 | 1.40 / 0.50 |
| B | 0.06 | 1.2 | (1,.88,.76) | 2.4 | +1.58 / +0.59 | +2.02 / +0.28 | +1.46 / +0.19 | +1.44 / +0.12 | +1.22 / +0.58 | 1.08 / 0.49 |
| C | 0.25 | 0.7 | (1,.9,.8) | 2.4 | +1.09 / +0.75 | +1.94 / +0.31 | +0.96 / +0.29 | +1.21 / +0.16 | +0.52 / +0.71 | 1.22 / 0.56 |
| **D** | 0.25 | 0.7 | (1,.9,.8) | **1.5** | +0.14 / +0.86 | +1.19 / +0.54 | -0.07 / +0.48 | +0.24 / +0.37 | -0.57 / +0.87 | 2.31 / 0.65 |
| E | 0.25 | 0.7 | (1,.9,.8) | 1.0 | -0.44 / +0.85 | +0.66 / +0.69 | -0.75 / +0.56 | -0.40 / +0.47 | -1.24 / +0.94 | 3.00 / 0.69 |

An earlier trial tuned on the bakery alone (lamps 0.06, room light 1.8) left shop floors half a stop too flat and a
tower's hall washed out: a shared number is never tuned on one place.

## What it showed
- The day colour and low lamps take the orange out: warmth from 1.0-1.8 stops over to 0.4-0.9 (pass is 0.33).
- Brightness is set by how far the eye opens indoors, not by the room light's strength: at 2.4 every room was 1 to 2
  stops too bright whatever the room light; at 1.5 the room, the grocer and the bank are within a quarter stop.
- Still wrong at D: the hallway 1.2 stops too bright (it needs less light than a room with a window: the game does
  not tell them apart); the bakery much too dark (it stands in the corner of a 38 m lot, far from the middle of the
  lot where the room light is centred, and leaned on its lamps); the library half a stop dark; warmth above the pass.
- `look_check.py`: no failures at D.

## Not done
- No picture of trials D or E has been looked at: the owner looks at the sheets (2026-11-28). `before_and_trial_c.jpg`
  is the reference, the game before and trial C for five places.
- Night, dusk and morning not measured with these values; the full reference tour and the look tour not run after D.
- The gate's own steps (shots at three hours, G1 to G10, the sheet) not done. No ADR: ADR-0055's room light changes
  if this is taken up.
- Not light, so not mended here: the home room at 6 Orchard End is unfurnished in the shot; floors are orange boards
  where the references have grey slabs; no ceiling has beams; the engine's dark blotches on ceilings and walls.

## Next
1. The owner looks at a sheet of trial D (reference, before, D for the six places).
2. Centre the room's own light on the room the camera is in, not the lot (the bakery, the library).
3. Give a part of a home with no window less of it than a room with one (the hallway).
4. Then night and dusk (the lamps should be full at dusk, as the references have them), the full reference tour
   before and after, the look tour against its baseline, and the gate.

A trial: `python Tools/unreal/try_values.py <out folder> <tour list> BrixRooms:RoomFill=0.7 BrixLight:DayOpenIndoors=1.5`.
This checkout's build has the trial's code; a checkout without a build cannot take shots (LOOK_GATE, the machine).

## Carried on, 2026-11-28
The owner looks at the pictures; the numbers are `look_match.py`'s (brightness / warmth in stops; the pass is 0.5 / 0.33).

### The room's own light where the camera is (`BrixRooms`)
- It stands over the middle of the planned room the camera is in, or, on a storey with no plan (a shop's floor), at the
  camera's own spot; it used to stand at the middle of the lot. By day a planned room (a home's, a flat's) has
  `RoomFillHome` of it. Its reach is `RoomFillReach` 2.0 of the floor's diagonal (was 0.7).
- As the camera comes into a room the log says how much of the room's round is its own windows (`RoomWindowNear`). The
  hallway has none, the dining room 0.2, the shop floors 0.36 to 0.42. A rule by that share was tried and dropped: with
  the light over the room, the hallway and the dining room came out equally bright against their (first) references,
  and the shop floors half a stop dark, so the share did not tell the rooms apart in the numbers.

| Trial | change (on trial D) | room | hallway* | grocer | bank | library | bakery |
|---|---|---|---|---|---|---|---|
| F | the light over the camera's room | +1.15 / +0.74 | +1.24 / +0.55 | -0.78 / +0.51 | -0.46 / +0.46 | -1.01 / +0.88 | 0.55 / 0.35 |
| H | reach 2.0 | +1.23 / +0.74 | +1.34 / +0.53 | -0.53 / +0.57 | -0.18 / +0.45 | -0.72 / +0.86 | 0.61 / 0.33 |
| J | room light 0.8 (night share 0.225: by night still 0.18), home rooms 0.37 of it by day | +0.10 / +0.91 | +0.02 / +0.93 | -0.26 / +0.54 | +0.07 / +0.42 | -0.47 / +0.85 | 0.73 / 0.34 |
| **K** | lamps 0.1 by day, day colour (1, .96, .94) | +0.11 / +0.67 | -0.02 / +0.57 | -0.22 / +0.23 | +0.11 / +0.11 | -0.40 / +0.56 | 0.75 / 0.52 |
| K + beams | the ceilings' beams | +0.00 / +0.65 | +0.00 / +0.60 | -0.44 / +0.32 | -0.05 / +0.17 | -0.60 / +0.57 | 0.64 / 0.50 |

\* against the first hallway reference. The bakery's figures are the mean miss over its 12 squares (it is too bright:
+0.37 signed at H). Sheets: `reference_H_J_K.jpg`, `reference_K_beams.jpg`. **The game is at "K + beams".**

### The owner's note: the structure first
"you need to understand the structural difference before trying to fix the light, beams are missing in ceiling add
them. windows are bit different in reference and the game location you picked there will be difference due to that.
hall in reference has a bright source of light coming from room, which is missing in game".
- **Beams** (done): every planned room and every storey's whole floor has the references' beams (furniture_kit_17.py
  room(), hours_50.py shop()): dark wood 0.18 m square under the ceiling, across the shorter span, 1.4 m apart, the first
  0.5 m in; none in a room narrower than 2.6 m (the hallway reference has none); cut over the stair. Boxes drawn with no
  pattern drew black (a zero-sized pattern in M_Greybox): the beams and the doorways' frames, black till now, are plain.
- **Where the places differ in build** (not mended): the home room's reference is closed with one window in a side wall
  and furnished; the game's stop is an unfurnished corner dining room with 3.6 m of windows on two walls and orange
  boards (the reference: grey-beige slabs). The shop floors' reference stands beside a wall of windows; the game's shop
  stops look at the back wall with the shopfront behind the camera; the library's floor is dark boards. The bakery's game
  view has a big arched window on the left where the reference has a door in a dark wall and a lit oven fire. Every game
  ceiling has the engine's dark blotches.
- **The hallway reference, remade** (approved by the owner, 2026-11-28): its bright doorway was not the room beside it
  but the reference's making: its bounce raised x2 (the street x2.6) and its eye opened till the picture's mean was 100,
  4.36 stops past the home room's +0.13 in the same light. Candidates (`hallway_remake_midday.jpg`): with the bounce as
  the surfaces' own colours give it and the same eye (candidate 2, picked) the doorway reads as a sunlit room beyond.
  `hallway_remake_all_hours.jpg` is every hour: morning, noon, dusk and lamp-lit night with the first hallway's eye held
  (`PLACE_EYE_HELD` in hours_50.py), the moon nights opening to the owner's levels as before (the full moon at 12; the new
  moon at its limit, 1). The first hallway is in `Art/Retired/IndoorHallway_2026-11-26_*.jpg`.

### The reference tour at "K + beams", against the remade hallway
Indoors: room +0.00 / +0.66 by day, +0.95 / +0.24 at night; **hallway +2.31 / +0.79 by day**, +0.54 / +0.82 at night;
grocer -0.47 / +0.31 by day (passes), -1.43 / +0.78 at night; bank -0.08 / +0.16 (passes); library -0.61 / +0.56; bakery
0.64 / 0.50 by day, 0.88 / 0.54 at night. Out of doors (not touched by this work): alley by day passes; Market Square at
night -1.50 / -3.04, the street at night -1.39 / -1.67. The night stops with the old reach (0.7) came out the same within
0.3 stops, so the reach is not what is wrong at night; there is no night measurement from before this round to set them
beside.

### Still to do
1. The hallway: the game's is 2.3 stops brighter than the remade reference while the home room passes. Find what in the
   build lights it (the room light over it; its doorways; no rooms beside it lit by the sun) before setting a number.
2. Give the reference tour stops that see what their references see (a home room with one window, furnished; a shop
   floor looking along its shopfront), or the shop and room numbers stay loose.
3. Warmth: 0.5 to 0.8 over in the rooms, the hallway, the library and the bakery.
4. Night indoors and out (above); dusk and morning not measured; the look tour against its baseline; the gate (G1 to
   G10, the sheet). The engine's automated tests not run after the changes to `BrixRooms`.
5. The home room reference's bounce is still x2: the hallway's is now the surfaces' own, so the two rooms of one house
   are not lit by quite the same rule (in the catalog's issues).
6. On a storey with no plan the room light follows the camera, moved four times a second (`CheckSeconds`), not every frame.
