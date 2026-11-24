# The planet in the town's sky (WLD-065, ADR-0040)

Approved by the owner as the planet's look in the town's sky, 2026-11-24 ("Yes, keep it"), after asking to see the planet of
`Art/StyleTests/moon_view_28/` in the game.

The slice in summer, clear weather, in the yard at the centre of Old Town, looking north-east and up. Taken in
`Prototype/play/index.html` at 1280 × 800 and saved at 800 × 500.

| File | When |
|---|---|
| `day_1800.jpg` | 18:00: the planet and its rings only just show through the blue, behind the clouds |
| `dusk_2045.jpg` | 20:45, a quarter of an hour after sunset: it comes out, about 60% lit, town lights on its dark side |
| `night_2215.jpg` | 22:15: the lit part bright against the stars; the rings cross in front of it. A fainter night (`night` 0.3) was tried and the owner did not like it |

To see them again: `index.html?auto=1&weather=clear&x=912&z=784&yaw=0.87&pitch=0.3&t=1080` (then `t=1245`, `t=1335`).
`&weather=fog` or `rain` hides it. The planet never moves: from anywhere in town it is to the north-east, 32° up.

Its numbers are in `World/rules/sky.json` "planet"; `"show": false` takes it out.
