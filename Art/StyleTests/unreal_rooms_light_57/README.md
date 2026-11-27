# Unreal: the rooms' light against the rooms' own references — a round in progress (2026-11-28)

The first round of `docs/process/LOOK_GATE.md`, The matching round. **Not finished and not approved: the settings
on this branch are a trial.** To carry on, read this page, then LOOK_GATE's matching round.

## The shared cause
By day the game lit every room with its lamps at full strength and one warm light of the room's own, the same with a
window or without, set against rooms the owner has since retired (`Game/README.md`, The look gate, Not yet). The
approved references (`IndoorHomeRoom`, `IndoorHallway`, `ShopsShopFloor`, `ShopsBakeryLight`) are lit by day by the
world's daylight, lamps low or out; the hallway, with no window of its own, is dim.

## What was changed (the trial, uncommitted values now committed here as work in progress)
- `BrixRooms.h/.cpp`: `RoomLampDay` (the lamps' share of their strength in full daylight) and `RoomFillColourDay`
  (the room's own light's colour by day; `RoomFillColour` stays the night's). Night is unchanged by design
  (`RoomFill` x `RoomFillNight` is still 0.18).
- `DefaultGame.ini`, trial D: `RoomLampDay=0.25`, `RoomFill=0.7`, `RoomFillNight=0.257`,
  `RoomFillColourDay=(1,0.9,0.8)`, `DayOpenIndoors=1.5` (was 2.4).

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
