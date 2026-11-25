# Look test 37: markers over heads and places (ADR-0051)

The owner asked for markers better than the slice's words over heads, "like glowing icon representing the type of
interaction" (2026-11-25). Four options were drawn as a page (`markers.html`, open it in a browser; `?sheet=icons` for the
second sheet) and pictured with headless Edge:

| File | What |
|---|---|
| `options.png` | The four options at dusk: A brass token, B little lantern, C hanging sign, D ember sign |
| `icons.png` | Each kind's sign in all four, and how a marker behaves (spark, token, caption, behind a wall, new, under way, hand in, off screen) |

The owner chose A, 2026-11-25: "A, brass token". As built in the Unreal build (1600 x 900 at the laptop's display scale,
switches in `Game/README.md`):

| File | What | Switches |
|---|---|---|
| `unreal_talk_12m.png` | The first quest's person (Mei, resting on the square) 12 m off: the token with the speech bubble | `-BrixTime=15:12 -BrixNear=504,12` |
| `unreal_near_4m.png` | 4 m off: larger, with its caption and key | `-BrixTime=15:12 -BrixNear=504,4` |
| `unreal_dusk_7m.png` | At work in the Tram Café towards evening | `-BrixTime=18:40 -BrixNear=504,7` |
| `unreal_bread_street.png` | The first quest at its bread step: the nearest grocer's basket at the screen's edge, three shops on the minimap | `-BrixTime=15:12 "-BrixLife=act,new_in_town,r0504,stage:bread" -BrixNear=504,14` |
| `unreal_bread_map.png` | The same on the big map: the three nearest shops with bread | the same, with `-BrixMap` |
| `unreal_map.png` | The big map: Mei's token by the player's arrow, the mark's pin | `-BrixTime=15:12 -BrixNear=504,9 -BrixMark=300,560 -BrixMap` |

The numbers, colours and signs are `World/rules/markers.json`.
