---
adr: ADR-0057
title: Soft box cars — cabins open down to their floor, roofs high enough for the tallest rider
status: accepted          # owner in chat on 2026-11-26: "reference car model have a board on the seats, which is obstructing the view of player's lower half when they drive car. fix the reference model and the game both and check if players head is not touching the car ceiling"
date: 2026-11-26
relates_to: [CITY-067, CITY-061, ART-001, ADR-0015, ADR-0004]
amends: ADR-0015
---

## Context
In the approved Soft box look (`Art/Reference/vehicles_soft_box.png`, option A of `Tools/blender/car_kit_09.py`) each
car's body is one solid rounded box with a "waist" box on top of it, and both run straight through the cabin. The seat
cushions sat inside the body (top 0.78 m in a body whose top is 0.89 m, under a waist to 1.00 m), so a board lay across
everyone's lap. The roofs were low too (underside 1.56 to 1.75 m): to keep heads under them the game sank each rider
until the head cleared (cars.js seat(), `ABrixCars::DriverSeat`), up to 0.44 m into the body for a tall rider. The player
driving showed only from the chest up, and less from the camera behind. ART_DIRECTION V3 asks for a seated driver who
shows from the chest up through open cabins; with the board, the riders' laps and legs could never show.

## Options considered
1. **Cut a cabin well out of the body (chosen).** A rounded well from behind the seats to the dashboard, down to a floor
   0.10 m above the bottom of the body, cut out of the body, the waist and the estate's belt stripe (a Boolean in
   Blender). Seats stand on that floor with their tops at 0.50 m (the van's 0.60 m); roofs rise so their undersides are at
   1.73 m (the van's 1.84 m). From outside the car keeps its silhouette, waist line and colours; from above and through
   the open sides the seats, laps and legs show.
2. Take the waist off and lower the body. Changes the side view the owner approved, and the seats would still sit in
   a solid box.
3. Keep the models and seat people higher. Their heads would go through the roofs, and the board would stay.

## Decision
Option 1. Numbers: the tallest person the designer allows (1.80 m, World/rules/designer.json) under the tallest hair (an
afro, 1.12 S of seated head, people.js sit()) clears the roof by the 5 cm the seating keeps, so nobody is sunk below the
seat. The near models' triangle budget rises from 3200 to 3400 so the seats and posts are not thinned harder than before.
`Art/Reference/vehicles_soft_box_2.png` is the new reference; `vehicles_soft_box.png` stays for the record, superseded.

## Consequences
- `Tools/blender/car_kit_09.py` (option A and its new view from above) and `car_export_11.py` build the open cabins;
  `Game/Data/car_models.json` and `Prototype/play/car_models.js` are exported again. The game code is unchanged: it reads
  the seat height, and finds the roof and the floor in the models, as before.
- `BrixTown.World.CITY067_riders_show_and_fit_in_the_cars` checks every model: nothing over the seats and laps, the
  tallest head under the roof without sinking, a floor under the feet.
- Cabins are 0.11 to 0.17 m taller than in the first reference.
- The feet reach under the dashboard into the solid front of the body (a footwell is not modelled).
