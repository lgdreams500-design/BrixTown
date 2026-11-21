# Playable slice

Walk the starting city in third or first person. Open `index.html` in Edge or Chrome (double-click it).
Nothing to install; it runs from these four files.

| File | What it is |
|---|---|
| `index.html` | The page and its HUD |
| `play.js` | Builds the city from the data and runs the game |
| `people.js` | The people: option B bodies and faces, and the walk from the movement signatures |
| `city_data.js` | The city and one day of everyone's routine, packed from `World/`. Generated: do not edit |

## Keys
| Key | Does |
|---|---|
| W A S D, mouse | Walk and look (click the picture to capture the mouse, Esc to let go) |
| Shift | Run |
| Space | Jump |
| V | Switch between third and first person |
| Mouse wheel | Move the third-person camera in or out |
| E | Sit, lie down, get up, ride a lift |
| [ and ] | Slow or speed the clock (×1, ×5, ×20, ×100) |
| T | Skip an hour |
| M | Whole-city map with every resident on it |
| Q | Softer picture without shadows, for a slow machine (it also switches on by itself below 27 frames a second) |
| H | Hide the key list |

## After changing the city
```
cd Tools/worldgen
python -m unittest test_world
python build_play.py          # or: python build_play.py 30   for another day of the year
```
Then reload the page.

What is real, what is missing, what was tested and how the look follows the art documents: `docs/specs/domains/WORLD_MAP.md` §5.
