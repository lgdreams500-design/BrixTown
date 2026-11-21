# Tools

Build, export and validation scripts.

## feature_picker.html — choose what goes in the game
Open it in Microsoft Edge or Google Chrome (double-click), press **Open the project folder** and choose this
project's folder. Click a step to pick a level for a feature at each milestone; click it again to clear. Picks are
saved straight to `docs/backlog/feature_choices.json`, and a readable copy to `docs/backlog/BACKLOG.md`.
Process: `docs/process/FEATURE_PROCESS.md`.

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

`paint_filter.py` is the painterly filter that was tried and rejected.

## serve_play.ps1 — the playable slice over http
`powershell -ExecutionPolicy Bypass -File Tools/serve_play.ps1`, then open http://localhost:8765/. For machines with
no Python or Node; opening `index.html` straight from disk also works in Edge or Chrome.

## worldgen/ — the city as data
Generator, loader, checks and the bundler for the playable slice. See `worldgen/README.md`.
