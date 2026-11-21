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
