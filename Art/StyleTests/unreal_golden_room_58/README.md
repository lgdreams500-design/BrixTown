# Unreal: the golden room and its hallway, first build (2026-11-28)

ADR-0073: a room's light is set and approved in Unreal, in a golden room, and every other room is then measured against
it on a grey card. This is that room, built, with its first shots. **Nothing here is approved yet**: the light in these
pictures is the game's rule of today (trial K of `unreal_rooms_light_57`), shown so the owner can see where it stands.

## What was built (`ABrixLightRoom`, with `-BrixLightRoom`)
- **home**: `IndoorHomeRoom`'s room. The measuring rooms' shell closed on all four sides, the living room's pieces in the
  curve line where the reference stands them, the references' wide greyer boards, the sunlit paving outside the window.
- **hall**: `IndoorHallway`'s corridor, box for box from `corridor()` in `Tools/blender/hours_50.py`: no window of its own,
  the stair at its far end, the open door of a side room whose window faces the sun, the hall's pieces in oldtown.
- Each is lit as `ABrixRooms` lights a planned room: the room's own light over its middle and the game's lamp where the
  reference's lamps stand. Each through its reference's own camera (17 mm and 16 mm).
- **A grey card and a grey ball** (0.18, one even tone) stand where the reference's person stands. The game's log says
  where they are in each shot; `python Tools/unreal/card_read.py <folder>` reads them.
- Shots: `Tools\unreal\light_tour.ps1 -OutDir <folder> -List Tools\unreal\golden_tour.txt` (ten stops, about two minutes).

## The pictures
| File | What it shows |
|---|---|
| `golden_home_beside_blender.jpg` | The room: Blender's Morning, Midday, NightLamps and Dusk beside Unreal at 07:36 (the hour the game's sun comes in as the references' does), 12:00, 20:00, 22:30, and 07:36 with the room's own light and lamps out |
| `golden_hall_beside_blender.jpg` | The hallway, the same |
| `walls_three_ways_home.jpg`, `walls_three_ways_hall.jpg` | The walls as they were (the street's mottle), nearly smooth (0.8) and with no mottle, at 07:36 and 22:30. The owner chose no mottle (ADR-0074); the two sheets above have it. These three runs were before the painting was mended (G8 below) |
| `card_readings.json` | The cards and balls of the ten shots, as read |

## The card (brightness 0 to 1, a mid grey evenly lit is near 0.18; warmth in stops, red over blue)
| Stop | Room card | warmth | Hallway card | warmth |
|---|---|---|---|---|
| 07:36 | 0.0053 | +1.33 | 0.0179 | +2.03 |
| 12:00 | 0.0051 | +1.26 | 0.0176 | +2.00 |
| 20:00 | 0.0048 | +1.24 | 0.0172 | +1.99 |
| 22:30 | 0.0378 | +3.26 | 0.5098 | +1.21 |
| 07:36, the day alone | 0.0001 | | 0.0001 | |

- By day the hallway's card is 1.8 stops brighter than the room's; at night 3.8 stops.
- With the room's own light and the lamps out both cards are at the bottom of what the reading can tell (0.0001): the
  room's own light is at least 5.7 stops of the room's card and 7.5 of the hallway's.
- Frames a second, 1066 x 666 at High: 44 to 47 in the room, 41 to 47 in the hallway.

## Looked at, the whole frame (G1 to G10), beside the Blender pictures
| G | Room | Hallway |
|---|---|---|
| G1 shade and depth | Corners and the beams' undersides go darker. **No**: the card, the ball and the sofa's near side are almost black by day. The room's own light hangs in mid-room behind them, so nothing lights what faces the camera; Blender has a light behind the camera for this | Yes by day; flatter than Blender, which is dim with a bright doorway |
| G2 shadows | The sun's patch and the lamp cast shadows. The room's own light casts none | The pendant and the table lamp cast them at night; none by day |
| G3 surfaces | Walls plain by the owner's choice (ADR-0074). The floor's boards are darker than Blender's | The same; `look_check.py` fails its "flat" at 52 to 55 % (limit 45 %): ADR-0074 |
| G4 colour | Greyer and cooler by day than Blender's pink-warm plaster; at night much more orange | Warm grey by day; orange at night |
| G5 whites and blacks | **No**: at 22:30 the floor lamp burns a white patch on the wall by the window | A bright pool on the left wall at 22:30, not burnt |
| G6 people | Not judged: nobody stands in the room. The card and ball stand in for a person | The same |
| G7 lamps and windows | The window shows the hour (day, a red dusk, a blue night). **No**: the fire is not lit at night; the lamps are far stronger than Blender's | **No**: by day it is twice as bright as Blender's (mean 47 against 24) and the doorway is not the brightest thing; the side room is dim |
| G8 whole | The painting over the fireplace was ragged (it hung 2 cm off the wall and the engine drew the wall through it): now 6 cm off, whole in all ten shots | Whole |
| G9 the hours | **No**: 07:36, 12:00 and 20:00 are one picture but for the sun's patch and the view out; the room's own light does not follow the hour until the day is gone | **No**: the same |
| G10 nothing over the picture | Yes (tour shots; no `-BrixShot=` shot with the interface was taken) | Yes |

## Not done
- No light number was changed: this is the game's rule of today, to start from. Setting the light here (the room's own
  light at the windows and casting shadows, the lamps' strength, the fire) is the next step, then the owner's approval.
- Nobody stands in the rooms, so G6 is not judged and the people's own fill (`PeopleFill`) is not in these pictures.
- The fire is not lit; the lamps are the game's one lamp, not the kinds of light (FURN-066).
- No `-BrixShot=` shot (the engine's own warnings show only there). The path-traced pair with the room light off is not taken.
- The engine's automated tests were not run after the changes to `BrixLight` and `BrixLightRoom`.
- The catalog and the reference page have no place for an Unreal light picture yet.
- The plain plaster is only here: the town's rooms still draw the street's mottle (ADR-0074).
- The old measuring rooms (shop, cafe, kitchen) are as they were; their sill, shutters and paving are drawn with no
  pattern, which draws black (seen in the code, not shot here).
