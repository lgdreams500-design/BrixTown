# ADR-0037: Eastgate Mall's sculpted shell, in the town's own materials

Date: 2026-11-24. Status: accepted for the form (owner's request in chat, 2026-11-24, with a photograph); the materials
are the locked look, built in the slice for the owner to react to.

## Context

The three malls outside Old Town (Westgate, Eastgate, Northgate; `Tools/worldgen/malls_01.py`) were drawn as plain boxes
with a window to every bay. The owner sent a photograph of a mall whose outside is flowing white bands with lines of
light, big golden pierced domes and arched openings, and asked: "there are 3 malls in this world and they are not
accessible to player and are big rectangle boxes. would you be able design one of these like attached pic?"

The look is locked (ADR-0004). The photograph is glossy white metal and gold mesh under line lights; ART_DIRECTION says
surfaces are matte and mottled (W1), the palette muted and warm with colour from a few dull accents (W4), shapes chunky
and soft with round arches (W3), and lanterns carry the scene at dusk (W6).

## Options

| Option | For | Against |
|---|---|---|
| Copy the photograph's materials: gloss white metal, gold mesh, strip lights | Closest to the picture | Breaks W1 and W4; a change of look, which needs its own ADR and the owner's approval; nothing else in town would look like it |
| Keep the box and dress it (signs, awnings, a bigger entrance) | Small | Not what was asked: it stays a box |
| **The photograph's form in the town's materials** | The flowing bands, the eggs and the arches are all there; W3 already asks for soft shapes and round arches; it sits in the same light and haze as the rest | Pale plaster and ochre, not white and gold; reads as a grand old-town building, less as a futuristic one |

## Decision

1. **Form from the photograph, materials from the town.** Eastgate Mall gets a skin of plaster ribbons (Cream and
   Plaster, mottled) whose top rises and falls, an Ochre pierced egg over each of its four corner stores, a Cream pierced
   half egg over the open court as its roof light, and a round arch on each side, the grand one over the door. The joints
   between the ribbons and the openings of the eggs are dark by day and lit in the Lantern colour at night, and lanterns
   hang under each arch.
2. **The hall does not change.** Its `rect`, `door`, floors, `plan`, staff and customers stay, so no resident's day, path
   or car changes. The shell is a skin round the hall; under an arch the hall's own front shows as lit shop windows and
   glazed doors.
3. **The shape is data.** The lot carries `shell`: the skin's closed line with a bottom and a top height at each point,
   the eggs (middle, half-sizes, how many openings round), the roof light and the arches. `Tools/worldgen/mall_shell_01.py`
   works it out and writes the same shell each time (a test checks this). The slice only draws it
   (`Prototype/play/shell.js`); an engine build can read the same data.
4. **Two new materials in the slice**, both matte and mottled like plaster: 25 "pierced shell" (a lattice of diamond
   openings) and 26 "ribbon wall" (bands with a joint under each lip). Curved surfaces carry their own normals
   (`Mesh.quadN`), so the eggs and the skin shade smoothly.
5. **Eastgate only.** It has a big store in each corner and a court in the middle, which is what the eggs and the roof
   light stand over. Westgate and Northgate are as they were until the owner says how they should look.
6. **The inside is not part of this.** Going into a mall is F-BLD-MALL, which has no level picked. The roof light is
   placed over the plan's open court so that an inside built later has its skylight where the plan needs it.

## Consequences

- Eastgate is about 107,000 static vertices where the box was about 2,000, and about 1,400 more solid boxes (the skin
  cannot be walked through). No frame-rate figure was taken.
- No new reference image and no new rule: the look lock (ADR-0004) is unchanged. If the owner wants the photograph's
  white and gold, or gloss, that is a look change and needs a new ADR.
- The maps (the round map, the big map, `draw_map.py`) still show the lot's rectangle, not the skin's line.
- Pictures from the slice, by day, at dusk, at night and from above: `Art/StyleTests/mall_shell_25/`.
- Every size (heights, how far the wall wanders, the eggs, the arches, 18 ribbons, 2.6 m between openings) is an
  `[ASSUMPTION]` until tuned.

## Later the same day

Points 2 and 6 were overtaken: the owner asked for entrances and a new inside ("start interiors fresh as this is new
design"). The hall's rect, door and floors stand; its `plan` was replaced and its wall opened under each arch. See
ADR-0038.

## Later: the space behind the skin, and the maps

The owner sent a picture from between the skin and the hall ("why there is a square box around the mall?") and asked for
the maps to show "the malls real curvy outline rather than the rectangle".

- The hall is still the rectangular building it was (decision above): the skin stands 5 to 18 m outside it, and the hall's
  own wall is meant to be seen only under the four arches, as the entrance fronts. The space between the two is not for
  going into. On foot it could not be reached, but a car could: at the low end of each arch the walls began where the
  skin had begun to lift, 1.2 m up, and a car is tested 0.7 m up. From in there the skin, drawn on its outer side only,
  was not seen at all, so the town showed through it and the hall's plain wall stood alone.
- Now the arch ends are solid from the ground; the skin has a back in plain plaster and the lid from its top to the
  hall's roof an underside, so if anyone does get in it is a closed passage.
- The small map and the big map draw a building that has a `shell` by its skin's line, with its eggs, its roof light
  and a light mark at each arch (`drawBase` in play.js), not by its `rect`. The outline is a rounded oblong with gentle
  waves, because that is the skin's line; the eggs are what make the mall curvy from above.
- Not done: `Tools/worldgen/draw_map.py` (the map page) still fails before it reaches the malls, as on main.

Picture: `Art/StyleTests/mall_shell_25/eastgate_on_the_map.jpg`.
