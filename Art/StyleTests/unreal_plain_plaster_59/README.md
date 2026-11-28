# Unreal: the plain plaster in every room of the town (2026-11-28)

ADR-0074: a room's plaster in the game is one even tone, no painted mottle. It was built for the golden room only. The
owner, on the reference page's bakery and shop floor: "you have not used 0 paint like you did in the home rooms else
where interiors". This carries it to every interior of the town. **Nothing here is approved**: the pictures show the
game before and after, for the owner to judge. No light, model or other material was changed.

## What was changed
- `ABrixRooms` (a planned home's or flat's partitions, a shop fit-out's partitions, the ceilings' beams, the doorways'
  posts and lintels): drawn in the rooms' copy of the material, `M_Room`, with the plaster's "how smooth" from data.
- `ABrixTownActor` (the plaster lining inside a building's own walls, its ceilings, a switchback stair's spine wall, a
  tower's lift shaft): the same, in a group of instances of their own (`RoomPlaster`), so the street faces of the same
  walls stay in the town's group and keep the street's mottle.
- The number: `WallSmooth=1.0` under `[/Script/BrixTown.BrixRooms]` in `Game/Config/DefaultGame.ini` (0 the street's
  mottle, 1 one even tone). Floors, furniture and everything out of doors are as they were.

## How the pictures were taken
`Tools\unreal\light_tour.ps1 -OutDir <folder> -List Art\StyleTests\unreal_plain_plaster_59\tour.txt`: sixteen stops in one
launch, High, 1066 x 666, once with the code as it was and once with the change, each on the same checkout.
Two sets taken earlier were thrown away and are not here: the first launch of the game in a fresh checkout lights the
street differently from every later one, and the checkout's furniture meshes had not been downloaded (Git LFS), so
rooms were empty.

| Stop | Place, and what it is judged against (`Tools/reference_catalog.py --for`) |
|---|---|
| `room_*` | home_room: a ground-floor room of 6 Orchard End, `IndoorHomeRoom` |
| `hall_*` | home_hallway: the same house's hall, `IndoorHallway` |
| `bakery_*` | bakery: the Stone Oven through its reference's camera, `ShopsBakeryLight` |
| `grocer_*` | shop_floor: Northwood Grocer through its reference's camera, `ShopsShopFloor` |
| `cafe_*` | cafe: **no reference** (a gap in the catalog), shown before and after only |
| `tower_*` | tower_hall: **no reference** (a gap in the catalog), shown before and after only |
| `alley_*`, `street_*` | out of doors, which must not change |

## The pictures
| File | What it shows |
|---|---|
| `rooms_before_after_day.jpg` | The six rooms at 12:00, before beside after |
| `rooms_before_after_night.jpg` | The same at 22:30 |
| `outdoors_before_after.jpg` | The alley and the street at 12:00 and 22:30, before beside after |
| `after_beside_references.jpg` | The four rooms that have a reference, after, each beside its own reference's Midday and NightLamps |
| `tour.txt` | The tour list |

## Out of doors: unchanged
The mean difference between the before and after picture, in levels of 255, and the share of the picture that moved by
more than 12 levels. The same stop shot twice with the same code differs by 0.5 to 0.9, so these are no change.
| Stop | Mean difference | Moved more than 12 levels |
|---|---|---|
| alley 12:00 | 0.60 | 0.0 % |
| alley 22:30 | 0.61 | 0.1 % |
| street 12:00 | 0.80 | 0.4 % |
| street 22:30 | 0.43 | 0.1 % |
The rooms moved by 1.5 to 6.6 (the cafe least, a tower's hall most), which is the paint going.

## Measured (`python Tools/unreal/look_check.py --tour <folder>`)
0 of 16 fail, before and after. Only "flat" moved (the share of the picture with no change of brightness in it; the
limit by day is 45 %, night is not judged):
| Stop | flat before | flat after |
|---|---|---|
| room 12:00 / 22:30 | 6 % / 4 % | 14 % / 5 % |
| hall 12:00 / 22:30 | 26 % / 18 % | 25 % / 18 % |
| bakery 12:00 / 22:30 | 7 % / 7 % | 15 % / 11 % |
| grocer 12:00 / 22:30 | 5 % / 4 % | 11 % / 10 % |
| cafe 12:00 / 22:30 | 3 % / 5 % | 4 % / 7 % |
| tower 12:00 / 22:30 | 18 % / 26 % | 37 % / 48 % |
ADR-0074 says the flat limit does not fit a room's plain walls (it fails the golden hallway by day). None of the town's
rooms shot here fails by day; a tower's hall is nearest, and at night it is over 45 %, where the check does not
judge. The limit was not changed.

Frames a second (High, 1066 x 666): 31 to 44 before, 35 to 44 after, on a laptop with other sessions
working. In this pair of runs the rooms were 0 to 5 lower after (the home's room and the grocer 4 to 5, the bakery 1, a
tower's hall 0), the hall by day and out of doors 2 to 8 higher. The re-shot bakery ran at 40 to 41 (31 to 38 when first
shot) and the shop floor at 41 to 45 (the same as before). A cost of a few frames in a room is not ruled out by these
figures and not shown by them either.

## The look gate, the after pictures beside their references (G1 to G10)
| # | Answer |
|---|---|
| G1 Shade and depth | Yes for the plaster: walls now go darker into the corners and under the beams, where before the paint's patches hid it. No, as before: the furniture hardly darkens the floor it stands on (the bakery's table, the grocer's counters) |
| G2 Shadows | As before, not changed: the grocer's pale hanging box reads as lit from nowhere; the lamps at night throw no pools |
| G3 Surfaces | Walls and ceilings are one even tone, which is the owner's decision (ADR-0074), an exception to W1 and I1. Floors, wood and cloth keep their mottle. **No**: the boxes a shop is dressed with (the grocer's counters and hanging sign, the pale lamp shade), the stairs and the bakery oven's grey body keep the street's strong mottle and now stand out against plain walls. **No**: a tower's hall is two blank plain walls and a ceiling with nothing on them |
| G4 Colour | No, as before and now plainer to see: by day the bakery's and the tower hall's walls are a cold white where the references' plaster is warm pink-buff; at night every room is one orange |
| G5 Whites and blacks | Yes: nothing burnt, nothing crushed by day (the look check's figures) |
| G6 People | Not changed by this: the people in the bakery, the grocer, the cafe and the tower's hall are neither black nor glowing, by day or at night |
| G7 Lamps and windows | No, as before: at night an even orange with no pools under the lamps; the windows do show the hour |
| G8 Whole | Nothing new is broken: no face missing, see-through or flickering where the inside faces moved to their own group. As before: the hall's stop has a dark post over its left quarter, the bakery's a teal door post on its left edge |
| G9 Day and night | Both read, and only the light changes between them |
| G10 Nothing over the picture | Yes in these shots. Not taken: a `-BrixShot=` shot with the HUD on |

Against the references in what is there, not changed by this and written elsewhere: the home's room stop is an empty
corner room with three windows where `IndoorHomeRoom` is a furnished room with one; the hall, the bakery and the grocer
differ as their `unreal.issues` in `Art/Reference/catalog.json` say.

## Not done
- Eastgate Mall and the Furniture Outlet (`ABrixMallActor`) were not touched: their inside walls keep the old paint.
- The dressing boxes, the stairs and the oven's body keep the street's mottle (G3 above): whether "painted wood" and a
  shop's counters should be plain too is the owner's to say.
- The golden room has its own `WallSmooth` (`BrixLightRoom`); there are now two numbers to keep the same.
- The reference page: `ShopsBakeryLight` and `ShopsShopFloor` were re-shot with this (`reference_shots.py`), still
  "for your review". The other rooms on the page are the golden room and hallway, which had it already; the mall's are
  not changed by this.
