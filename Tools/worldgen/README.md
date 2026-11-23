# worldgen: the city as data

| File | Does |
|---|---|
| `make_city_01.py` | Writes the starting city (`World/city_01/`) from one seed. Re-running it replaces that city |
| `world.py` | `load()` reads a city, `validate()` lists what is wrong with it, `simulation()` runs the living-city rules over it; `simulation(w, traffic=True)` adds residents' cars |
| `test_world.py` | 13 checks on the data and on a day of routines: `python -m unittest test_world` |
| `make_roads_01.py` | Writes `World/city_01/roads.json`, the road network, from the grid and the block files (CITY-043) |
| `shrink_carparks_01.py` | Shrinks the three malls' car parks to a quarter, with a forecourt and trees (WLD-054); run once after `malls_01.py` |
| `build_play.py` | Packs the city and one day of routines into `Prototype/play/city_data.js`, with money, quests and the designer and the results the page's port must match (ADR-0012) |
| `draw_map.py` | Draws a top-down map of the city from the block files and roads into `Prototype/play/map.html` (hover for names, click a kind to pick it out). Reads only; for seeing a remodel before and after |

Needs Python 3.9 or later and nothing else. File format and how to add blocks: `docs/specs/domains/WORLD_MAP.md`.
