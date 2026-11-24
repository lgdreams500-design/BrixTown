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

`paint_filter.py` is the painterly filter that was tried and rejected.

## serve_play.ps1 — the playable slice over http
`powershell -ExecutionPolicy Bypass -File Tools/serve_play.ps1`, then open http://localhost:8765/. For machines with
no Python or Node; opening `index.html` straight from disk also works in Edge or Chrome.

## worldgen/ — the city as data
Generator, loader, checks and the bundler for the playable slice. See `worldgen/README.md`.
