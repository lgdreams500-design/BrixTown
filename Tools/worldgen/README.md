# worldgen: the city as data

| File | Does |
|---|---|
| `make_city_01.py` | Writes the starting city (`World/city_01/`) from one seed. Re-running it replaces that city |
| `world.py` | `load()` reads a city, `validate()` lists what is wrong with it, `simulation()` runs the living-city rules over it; `simulation(w, traffic=True)` adds residents' cars |
| `test_world.py` | 13 checks on the data and on a day of routines: `python -m unittest test_world` |
| `test_buy_menu.py` | The buy menus' rules (`World/rules/buy_menu.json`, UI-020, UI-021): room for both layouts, a mark for every place a thing goes, an object or a stand-in for every thing for sale: `python -m unittest test_buy_menu` |
| `make_roads_01.py` | Writes `World/city_01/roads.json`, the road network, from the grid and the block files (CITY-043) |
| `shrink_carparks_01.py` | Shrinks the three malls' car parks to a quarter, with a forecourt and trees (WLD-054); run once after `malls_01.py` |
| `mall_inside_01.py` | Works out Eastgate Mall's new inside (the court as a chain of round atria, the concourses, the egg halls, bridges, crossing moving stairs, lifts, shops, gardens, planters, round skylights; every shop an outlet of a kind from `World/rules/mall_outlets.json`, and the rides) and writes it into the lot as `plan`, with where visitors stand and sit (WLD-058 to WLD-062, ADR-0038). Run `mall_shell_01.py` after it (not `add_careers_01.py`: the lot's `work` places already stand at each unit's `stand`, and on today's blocks that script deals hundreds of residents other work). Re-running it writes the same plan. (`add_outskirts_01.py` would put the old `loop` plan back: do not re-run that) |
| `mall_shell_01.py` | Works out Eastgate Mall's sculpted shell (the skin's line and heights, the eggs, the roof light, the arches) and writes it into the lot as `shell` (WLD-057, ADR-0037). Re-running it writes the same shell |
| `add_lamps_01.py` | Adds street lamps (`"set": "lamps"`) wherever a pavement of the street grid is more than `street_lamps.reach_m` from one (WLD-072; numbers in `World/rules/dressing.json`); running it again puts the same lamps back |
| `widen_lanes_01.py` | Widens the communities' lanes to 9 m; then run `add_houses_03.py` and `add_lanes_dressing_01.py` again |
| `add_careers_01.py` | Gives every working resident a career and post, and each workplace its staff, by the rules in `World/rules/careers.json` and `Tools/citysim/careers.py` (CITY-081 to CITY-086). Re-running it changes nothing |
| `build_play.py` | Packs the city and one day of routines into `Prototype/play/city_data.js`, with money, quests and the designer and the results the page's port must match (ADR-0012) |
| `draw_map.py` | Draws a top-down map of the city from the block files and roads into `Prototype/play/map.html` (hover for names, click a kind to pick it out). Reads only; for seeing a remodel before and after |

Needs Python 3.9 or later and nothing else. File format and how to add blocks: `docs/specs/domains/WORLD_MAP.md`.
