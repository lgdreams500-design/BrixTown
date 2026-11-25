# Tools

Build, export and validation scripts.

## backlog.ps1 — check the picks and rewrite the backlog
The owner says picks in chat; an agent records them in `docs/backlog/feature_choices.json` and runs
`powershell -NoProfile -ExecutionPolicy Bypass -File Tools/backlog.ps1`. It rejects invalid picks, lists needs that
are not picked yet, and rewrites `docs/backlog/BACKLOG.md`. Process: `docs/process/FEATURE_PROCESS.md`, ADR-0008.

## citysim/ — the city in a box
The living-city rules with no engine, with tests and top-down clips. See `citysim/README.md`.

## blender/ — look and animation tests
Run inside Blender's Python console (see `docs/art/ART_DIRECTION.md` §4). Each script loads the helper part
of the ones before it, so keep the folder together.

| Script | Makes |
|---|---|
| `style_test_01.py`, `style_test_02.py` | The old-town alley. `style_test_02` raw render is the approved world look. |
| `character_test_01.py`, `character_test_02.py` | First character blockouts and the shared shape helpers. |
| `look_test_03.py` | Three playful looks (rejected). |
| `old_town_04.py` | Alley at morning and dusk, market square, character options A/B/C. |
| `character_face_05.py` | Option B with the face control set: line-up, expressions, mouth shapes, lip-sync clip. |
| `motion_lib.py` | Body motion maths with no Blender code: skeleton, movement signatures, walk, layers. |
| `character_anim_06.py` | Jointed body; walk signatures clip and talking-while-walking clip. |
| `building_kit_07.py` | Building kit: real lots from `World/` built with laid stone, cut reveals, arch stones, tiled roofs, shutters, shopfronts. Its street render is the daylight reference (ADR-0007). Also runs headless: `blender -b --factory-startup --python building_kit_07.py`. |
| `building_kit_08.py` | The special buildings on top of kit 07: town hall, hospital, mall, guild house, office and flats towers, workshop, tram depot. Not yet approved. |
| `car_kit_09.py` | First car look test (CITY-067): three directions with curves instead of boxes, A soft box, B pebble, C classic, each a small car, an estate and a van, with a person for scale. Runs headless through the Store launcher: `blender-launcher.exe -b --factory-startup --python car_kit_09.py`. The owner chose C, Classic (ADR-0011), then A, Soft box (ADR-0015). |
| `car_export_10.py` | Builds on car kit 09: exports the Classic cars (and a classic pickup) as low-poly meshes, near and far, with parts tagged for colours and extras, into `Prototype/play/car_models.js`. |
| `car_export_11.py` | Builds on car export 10: exports the Soft box cars (ADR-0015), with a Soft box pickup, into `Prototype/play/car_models.js`. Keeps the bodies, wheels and lamps whole and thins only the small parts. This is the one the slice uses now. |
| `mall_goods_31.py` | Builds on furniture kit 17 (and the car export's file format): the 49 goods for the home that Eastgate Mall's outlets sell (FURN-065), each made of a few light shapes, near and far, into `Prototype/play/goods_models.js`; also the `.blend`, a `.glb` in `Art/Export/mall_goods_31/` and sheets in `Art/StyleTests/mall_goods_31/`. Set `WASTED_ROOT` to the checkout to write into |
| `skates_34.py` | Skates look test 34, builds on car kit 09 and character face 05: the skate store's three pairs (Manual quad, Electric inline with hub motors and a battery, Neon Boost with glowing wheels and a booster), each skate one low-poly mesh on an empty at `foot_l` / `foot_r`, with a far model. Renders a labelled sheet, a turnaround, close-ups, the pairs worn by the Courier and a dusk look at the neon into `Art/StyleTests/skates_34/`. Not yet approved. |
| `skateboards_35.py` | Skateboards look test 35, builds on skates 34 (and through it car kit 09 and character face 05): the owner asked for skateboards instead of skates, so the store's three tiers as boards (Manual popsicle deck, Electric cruiser with a battery box and hub motors, Neon Boost with glowing wheels and edge, underglow and a booster), each one low-poly mesh with its origin on the grip at the deck's centre, with a far model; dimensions for the browser slice in the log. Renders a labelled sheet, close-ups (upright and upside down), the Courier pushing the Manual, riding the Electric and the Neon Boost, carrying each board, and a dusk look at the neon into `Art/StyleTests/skateboards_35/`. Not yet approved. |
| `baker_look_45.py` | Baker look test 45, builds on furniture kit 18: Noor, the master baker of the Stone Oven (a woman of 58), dressed three ways from the designer's values (A Linen, B Flour cap, C Old Town), and the bakery inside from the kit's parts: a stone oven with an arched mouth and its wood store, the kneading bench, racks of loaves with the crock of the Mother on top, flour sacks, the counter; by day and at four in the morning. Renders in `Art/StyleTests/baker_look_45/`. Not approved: for the owner to choose from. Not yet: nothing of it is in the game (the Stone Oven's inside in Unreal is still a grocer's); the people system has no way to show age, so Noor reads younger than 58, and a grey bun reads like a cap. `WASTED_ONLY=noor,noor_close,bakery_day,bakery_four`, `WASTED_SAMPLES`, `WASTED_SCALE`, `WASTED_ROOT` as in kit 18. |

`paint_filter.py` is the painterly filter that was tried and rejected.

## serve_play.ps1 — the playable slice over http
`powershell -ExecutionPolicy Bypass -File Tools/serve_play.ps1`, then open http://localhost:8765/. For machines with
no Python or Node; opening `index.html` straight from disk also works in Edge or Chrome.

## worldgen/ — the city as data
Generator, loader, checks and the bundler for the playable slice. See `worldgen/README.md`.
