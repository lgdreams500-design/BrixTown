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
| `economy.py` | Money in the slice (ADR-0012): the ledger, the bank (interest, loans, settled day by day), shop stock and prices from the residents' visits, the market stall. Standard library only. |
| `quests.py` | Quest lines, daily jobs and reputation, run against `economy.py`; `problems()` checks `World/rules/quests.json`. |
| `test_economy.py` | 31 tests named after ECO, BANK, PROP, QST and CHR requirements; every starter quest line is played to every ending. `python -m unittest test_economy -v` |
| `render_traffic.py` | Writes an HTML page that plays a day of traffic: `python render_traffic.py traffic_day.html` |

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
