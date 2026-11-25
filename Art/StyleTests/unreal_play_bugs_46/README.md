# Unreal: bugs found in a sweep, 2026-11-26

A tour of 36 stops at noon (streets, doors, the inside of every kind of building, night, rain) with
`Tools/unreal/light_tour.ps1`, looked over for what is wrong. What was found and mended is in `Game/README.md`
(The camera, Designed houses).

| Picture | What it shows |
|---|---|
| `hung_pieces_before_after.jpg` | Halls of a house and a row house, before and after, and two halls where the pendant stays drawn (farther from the camera). Before: a pendant's shade beside the camera behind the player filled half the picture. After: a hung piece within 1 m of the camera is not drawn (`HideHungWithin`). |

Also found: the plans of storeys and flats list their windows by floor, and Unreal read them as one list, so they had
none and the log had five warnings at every start (`ART001_every_house_has_its_line_and_colourway` now checks that
every plan has its windows).
