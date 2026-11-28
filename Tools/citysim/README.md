# citysim — the city in a box

The rules of a living city block with no engine: clock, weather, lots, paths, places to use, daily
routines, loading by distance, vegetation and vehicles. It exists to prove the rules and their tests
before anything is built in Unreal (`docs/process/FEATURE_PROCESS.md`, stage 5).

Spec: `docs/specs/domains/LIVING_CITY.md`. Every state is **calculated** from the world time and the
data file; nothing is stepped frame by frame.

| File | What it is |
|---|---|
| `citysim.py` | The rules. Standard library only. |
| `data/block_01.json` | All the tuning: lots, residents, role routines, species, weather odds, timetables. |
| `make_block_01.py` | Writes that data file. Edit and re-run to change the block. |
| `test_citysim.py` | 13 tests, each named in the spec's "Proven by" column. |
| `render.py` | Draws top-down clips. Needs Pillow and numpy. |
| `traffic.py` | Residents' cars: road network, who drives, the day's car plan, `car_at()` (LIVING_CITY 3.4, ADR-0009). Standard library only. |
| `test_traffic.py` | 21 tests over the whole starting city in `World/` (one skipped until pavements exist). |
| `giveway.py` | Resident cars giving way to what a player controls: slow, wait or stop, then back on their timeline without speeding, catching up only at their next stop (CITY-049; `World/rules/traffic.json` `give_way`). Standard library only. |
| `test_giveway.py` | 5 tests: stop short and queue, never faster than the timeline, a short stop skipped, placed back when far behind, the other lane not held. |
| `economy.py` | Money in the slice (ADR-0012): the ledger, the bank (interest, loans, settled day by day), shop stock and prices from the residents' visits, the market stall. Standard library only. |
| `quests.py` | Quest lines, daily jobs and reputation, run against `economy.py`; `problems()` checks `World/rules/quests.json`. Also a career's story lines (QST-010 to QST-015, ADR-0056): facts the player leaves behind, needs on them, several speakers and scenes, lines taken again. |
| `story.py` | Makes `World/rules/quests_story.json` from the approved writing in `docs/narrative/src` and `World/rules/story_cast.json` (who plays whom, where the scenes are). `python story.py`; `--check` says whether it is up to date. Not yet: the slice does not run these lines (Python and Unreal do; Unreal has no scenes yet, see Game/README.md); the map has no mill house or farm kitchen, mill house or farm kitchen, so the cast are residents renamed where they live; a talk happens wherever the person is; regard with Mei is not balanced. |
| `test_economy.py` | 60 tests named after ECO, BANK, PROP, QST and CHR requirements; every starter quest line is played to every ending, and the Baker career and the courting to every one of theirs by 1,500 random games (QST-014). `python -m unittest test_economy -v` |
| `estate.py` | The real estate market (ADR-0022, PROP-009 to PROP-023): every building carved into units, owners (the town, several companies, households, businesses, the player), buying, selling, renting, letting, block price indices, upkeep and tax, mortgages, settled day by day on `economy.py`'s ledger. Standard library only. |
| `test_estate.py` | 37 tests named after PROP and BANK requirements, including the balance targets in `World/rules/estate.json`. `python -m unittest test_estate -v` |
| `render_traffic.py` | Writes an HTML page that plays a day of traffic: `python render_traffic.py traffic_day.html` |
| `careers.py` | NPC careers and staffing (CITY-081 to CITY-086): each workplace's posts from `World/rules/careers.json`, and who does which job, worked out from homes, lots and rules. Standard library only. |
| `test_careers.py` | 15 tests over the whole starting city: 50+ careers, café and grocer staff counts, posts as caps, artists at home, office share, own post at work, same answer in any order. `python -m unittest test_careers -v` |
| `repairs.py` | Repair crews mending crash damage: when each broken thing is mended, worked out from the damage list and `World/rules/crash.json` (CITY-075). Standard library only. |
| `test_repairs.py` | 8 tests: everything mended, one job at a time at walking pace, working hours only, same answer in any order. |
| `time_skip.py` | Skip time (WLD-073, WLD-074): 10 minutes, 1 hour, or to the next change of weather, from `World/rules/time_skip.json`; only the host, not during a ride or a conversation. A skip moves the clock only. Standard library only. |
| `test_time_skip.py` | 8 tests: the steps, landing on the first change of weather, the limit when it never changes, host only, refused while busy. Not yet: no Unreal or slice control; the needs (NEED-002) are not yet shown to follow a skip, since they are not in citysim. |
| `tubes.py` | Glass travel tubes (CITY-108 to CITY-112): network checks, the shortest way, a ride (drawn in from the booth's floor, the run, set down in the far booth) and the camera following it, all worked out from the ride's start, from `World/rules/tubes.json`; `rounded()` turns each corner on an arc (`bend_r_m`, and `junction_bend_r_m` where one tube meets the next), the line the tube is built along and ridden. Ported to C++ as `BrixTubes` (Game/Source/BrixWorld). Standard library only. |
| `test_tubes.py` | 24 tests on a sample network and the town's: no problems in good data, bad data found, the town's level runs over the grown street trees' crowns, booths and posts clear of the road and everything on the street, room at each booth's door, each booth's camera spot clear of crowns and buildings, every stop reaches every other, shortest way, junctions passed at speed, the speed carried on out of the draw-in and into the pop-out, floor to tube to floor, no jump, not stepped, bends rounded, the rider inside the built glass all the way across town, the camera never cuts and starts and ends as the everyday camera. Not yet: tubes are checked against buildings only on the map (tube_map.py), not in a test; spans over roads and by crossings run to 34 m; other riders (CITY-113) and fares (Q-42) are not in it. |
| `make_tubes_01.py` | Writes `World/city_01/tubes.json`: the town's tube stops and routes (CITY-108, CITY-112), over the streets from a hub by Market Square, each stop's tube joining the line at a junction. Approved by the owner on the map, 2026-11-28. The data's corners are sharp; the tube is built and ridden round them on arcs (tubes.py `rounded()`). |
| `tube_map.py` | Draws the tubes on the town's map with each ride's time from Market Square and any building a tube passes less than 1 m over: `python tube_map.py out.png`. Needs Pillow. |

```
python -m unittest test_citysim -v
python render.py still map.png
python render.py day   frames_day      # then: ffmpeg -framerate 24 -i frames_day/f_%04d.png -pix_fmt yuv420p day.mp4
python render.py years frames_years    # then: ffmpeg -framerate 10 -i frames_years/f_%04d.png -pix_fmt yuv420p years.mp4
```

Use it from code:

```python
import citysim as cs
city = cs.City()
t = 16 * 1440 + cs.hm("12:34")            # day 16, 12:34
state = city.at(t)                         # {resident id: {pos, act, place, state}}
near = city.loaded(state, [(80, 62)], 40)  # who needs a body for a player standing there
```

Porting note: this is a design prototype in Python. The production version is a C++ module per
controller with the same data and the same tests (TECH-002, TECH-004).
