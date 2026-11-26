# Street lamps where the pavements were dark (WLD-072), 2026-11-26

Found in sweep 3 (Art/StyleTests/unreal_play_bugs_46/): about 8 % of the walled town's pavement was more than 40 m from a
street lamp, up to 71 m, and black at night. `Tools/worldgen/add_lamps_01.py` adds 318 lamps so every pavement of the
street grid is within 20 m of one (numbers in `World/rules/dressing.json` "street_lamps").

| Picture | What it shows |
|---|---|
| `night_before_after.jpg` | The same six stops at 22:30 in Unreal (`Tools/unreal/light_tour.ps1`), before and after: the east side of the old wall's line, Civic Row's north pavement and Linden Heights' south pavement have a lamp near; the west gate road (outside the street grid, not lit by this) and Market Square are as they were. Linden Heights' forecourt (`heights_front`) stays black: it is inside the block, not a pavement, and the camera there looks into the tower. |

Only the 12 lamps nearest the camera give real light in Unreal and the slice (`ABrixLamps::MaxLights`), so the added
lamps cost no more light than before.
