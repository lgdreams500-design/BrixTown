# Playable slice

Walk the starting city in third or first person. Open `index.html` in Edge or Chrome (double-click it).
Nothing to install; it runs from these five files.

| File | What it is |
|---|---|
| `index.html` | The page and its HUD |
| `play.js` | Builds the city from the data and runs the game |
| `conversations.js` | Talking to residents: the conversation panel, run from the dialogue data |
| `cars.js` | Residents' cars: placed from each car's day in the data (worked out by `Tools/citysim/traffic.py`) and drawn as stand-in boxes; drivers are hidden while in them |
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
| E | Sit, lie down, get up, ride a lift, talk to the resident in front of you |
| 1 to 9, or click | Answer in a conversation (the mouse is let go while you talk) |
| Esc | Leave a conversation |
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
