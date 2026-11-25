# The light in the Unreal build, before and after (WLD-042, WLD-028, CITY-013; ADR-0055)

The Unreal build's light looked at in every kind of place through the day on 2026-11-25, and what was changed
(Game/README.md "The eye and the night"). Not a new look: the day is the approved daylight (ADR-0007, ADR-0048) and is
unchanged; the night follows the slice's approved night (ADR-0023, `../night_sky_15/`) by eye. The night and dusk shown
here wait for the owner's word (ADR-0055 is proposed).

Taken at High on the owner's laptop, 1066 x 666, day 16 (a clear summer Wednesday: sunrise 05:00, sunset 20:30), in one
run each: `Tools\unreal\light_tour.ps1 -OutDir <folder>` for "after" and the same with `-Extra "-BrixLightOff"` for
"before" (the room lamps' change is not switched off by that: their "before" is from the build before it). The sheets
are made with `Tools/unreal/shot_sheet.py`; the number on each picture is its mean brightness, 0 to 255.

| Sheet | What it shows |
|---|---|
| `before_after_night.jpg` | 22:30 in the spawn alley, Market Square, a street, a park, Eastgate from above, and the square in rain and in fog. Before: as bright as day, an orange fog (a grey one under cloud). After: a dark blue night, lamp pools and lit windows |
| `before_after_dusk.jpg` | The alley, the square, a street and Eastgate at 20:00 (half an hour before sunset), the alley and the square at 20:45, the alley at dawn. Before: an orange white-out. After: dusk, lamps and windows lit |
| `through_the_evening.jpg` | After only: the square and the alley at 18:00, 19:00, 19:30, 20:00, 20:20, 20:45 and 22:30, then dawn at 05:20, 06:00 and 06:30. Golden hour at 19:30; the lamps come on as the sun gets low |
| `rooms_against_blender.jpg` | The Blender reference shop, cafe and kitchen (`../furniture_kit_18/`) beside the same rooms built again in Unreal with the game's own lamps and eye, at the references' hour, and the Unreal ones at night. What a room's light should be is measured against these (`Tools/unreal/light_reference.py`): within about half a stop in brightness and a third of a stop in warmth |
| `before_after_rooms.jpg` | A grocer, a cafe, the bank, the library, a row house and a designed house by day and at night. Before: a burnt white patch on a hall's ceiling and black walls; homes white under the engine's free eye. After: the references' light: a warm light of the room's own, lamp pools, the eye held |
| `day_unchanged.jpg` | Before and after at 07:20 and noon out of doors (the alley, the square, a street, a park, in rain, fog, cloud and snow) and in Eastgate: the same light. (The "after" shots were taken once main had been merged in, so they also show what other work brought that day: the lawn's pattern in the park, a cafe's shutters in the street) |

The issues found, one by one, are in ADR-0055's table. Still to do there: the stars, the Milky Way and the moon (WLD-042's
sky), lamps lit in shade by day, light from lit windows on the pavement, lamp shades that glow.
