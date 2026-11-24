# The view from a moon (look experiment 27)

Someone standing on a moon, head back, looking up at the planet the moon goes round: a living world of
seas, land, cloud and ice, with rings, huge in the sky. Made at the owner's request as a picture to look at (first with a banded,
ringed gas planet; the owner asked for a habitable one instead, then for its rings back, then for the moon to have air). The page itself is not part of the game. The owner then asked to see the planet in the game: that is WLD-065 (ADR-0040,
`Art/StyleTests/planet_sky_28/`), which draws the same planet in the slice's sky. For this page nothing in `World/`,
`Prototype/play/` or the specs changed.

`index.html` is the view itself, in plain WebGL 2. The person is one of the town's own, drawn by
`Prototype/play/people.js`, so the page has to stay at this place in the project to find that file. Open it in Edge
or Chrome (double-click), or start the `moon-view` server from `.claude/launch.json` and open
`http://localhost:8827/Art/StyleTests/moon_view_27/index.html`.

Everything is worked out from one number, where the moon is on its orbit:

- the moon always shows the planet the same face, so the planet hangs in one place in the sky;
- the sun and the stars go round once an orbit, so the planet runs through its phases;
- the moon has air: by day the sky is blue and the planet is only just there, as the town's moon is pale by day;
  it comes out at dusk and is at its brightest at night;
- once an orbit the sun passes behind the planet: its air shows as a red ring, and the lamps of its towns show on
  the dark face.

| Do | To |
|---|---|
| Drag | Look round the person |
| Mouse wheel | Step back or come closer |
| Slider, Play / Pause, Space | Move along the orbit, or stop the clock |
| A, or the Air box | Take the moon's air away (black sky, stars and the planet sharp by day), or give it back |
| 1 to 6 | Choose which of the six reference people stands there |
| H | Hide the labels |

`?t=0.5` starts at a place on the orbit (0 to 1), `?pause` with the clock stopped, `?air=0` without air, `?who=3` with
the third person; `?yaw=`, `?tilt=` and `?dist=` place the camera.

| File | When |
|---|---|
| `day_low_sun.jpg` | The opening view: a low sun behind the right shoulder; the planet, two-thirds lit, barely shows in the blue |
| `dusk_planet_comes_out.jpg` | Just after sunset (`?t=0.385`): the sky still light, the planet coming out |
| `night_part_lit.jpg` | Late in the night (`?t=0.78`): the sun is under the horizon but still lights two-fifths of the planet's face; town lights on the rest |
| `night_full_planet.jpg` | Night: the sun is under the horizon behind, the planet is full and lights the ground |
| `sun_behind_planet.jpg` | The sun behind the planet: the ring of sunsets, and town lights on the night side |

Taken at 1280 × 800 and saved at 800 × 500.
