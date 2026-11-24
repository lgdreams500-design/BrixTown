# ADR-0038: Eastgate Mall's inside, designed afresh for its shell

Date: 2026-11-24. Status: accepted (owner's request in chat, 2026-11-24). Afterwards the owner picked F-BLD-MALL at
level 2 (Full mall) for the first slice ("Yes, level 2"); its events and crowds need F-NPC-POP at level 2, which is at 1.

## Context

After the shell of ADR-0037 the owner found the mall still shut ("there are no entries to the mall"), sent three
pictures of mall interiors (curved white galleries round a planted court under a glazed roof, crossing moving stairs),
and said: "ignore the old mall and use new interiors as now we have ribbon walls and four pierced domes etc. start
interiors fresh as this is new design. I gave the reference photos but feel free to come with ideas that fit this new
design language for this mall."

The old `plan` (malls_01.py, concept `loop`) was a square circuit round a rectangular court. It was never drawn. Staff
posts are counted from a mall's ground-floor units (`careers.posts`), and every resident's work is assigned from all
posts in town (`careers.assign`, checked by CITY-086), so changing the ground floor's units can move people between
workplaces all over town.

The look is locked (ADR-0004): matte and mottled, the muted palette, lanterns and lit windows. The pictures are gloss
white with strip lights.

## Options

| Option | For | Against |
|---|---|---|
| Draw the old `loop` plan | The data exists | A square circuit that ignores the eggs, the roof light and three of the four arches; the owner said to ignore it |
| A new plan, staff reassigned by `add_careers_01.py` | Free hand with the ground floor | Residents all over town could change workplace; a wide change to `residents.json` for a new building's inside |
| **A new plan whose ground floor keeps the old posts** | The shell's parts each get their place inside; nobody's work changes; the change stays in one block file | The ground floor must have exactly four big stores, 58 full shops and two full cafes, which fixes how deep those shops are |

## Decision

1. **The plan follows the shell.** An oval court under the roof light, open through all three floors; a concourse
   straight in from each of the four arches; a round hall under each egg, open from its floor up into the egg, reached
   by a slanting walk from the court; shop fronts along every walk. On the upper floors the galleries' edges wander
   like the shell's ribbons and the upper one stands further back, so the court opens as it rises.
2. **Bridges and moving stairs in the court.** In each half a bridge crosses on each upper floor; moving stairs rise from
   the court floor to the lower bridge and on to the upper one. In the slice they are walked up as thirty low steps;
   they do not move yet (they do since the addendum below).
3. **Four entrances.** The hall's wall is open under each arch; the south one is still the lot's `door`, which the
   roads, kerb bays and residents use.
4. **The ground floor keeps its jobs.** Four big stores of 3,600 m2, 58 shops and two cafes deep enough for a full
   staff, in the same order of kinds as before: 206 posts in the same order, so `careers.assign` gives every resident
   the work they had. Every other ground-floor unit is under 145 m2 and has no staff of its own.
5. **Units say where their staff stand** (`stand`): behind a counter 3 m in from the shop front, or at stalls in an egg
   hall. `add_careers_01.py` uses it. A shop with staff in is drawn open; the rest have lit windows.
6. **The plan is data, the slice only draws it.** `Tools/worldgen/mall_inside_01.py` writes `plan` (walks, voids,
   bridges, units as before; new: fronts, walls, rails, entrances, halls, ramps, gardens, benches, roof) and where
   visitors stand and sit. `Prototype/play/court.js` draws it and makes it walkable; floors are the plan's walks less
   its voids, in 2 m squares, with a smooth strip and a parapet along each void.
7. **The town's materials indoors.** Plaster, tiles and wood in room materials, lit as rooms are; lit windows for shop
   fronts and for the line of light under each parapet; lanterns hanging under the galleries and in the egg halls. Seen
   from inside, an egg shows the sky through its openings (a new material, 27). No gloss and no white: as ADR-0037.
8. **Not built here.** Going into single shops and buying in them, visitors (no resident has the mall as a place to
   shop or eat, and crowds need F-NPC-POP at level 2; it is at 1), lifts, stairs that move, events.

## Consequences

- No resident, routine, road or car changed: `residents.json` is untouched and the careers tests pass unchanged.
  Only `b_16_5.json` changed in `World/`.
- The mall has 73, 68 and 68 units on its three floors (it had 76, 64 and 55). The counts quoted in
  CITY_BALANCE.md (579 units, 9,517 mall jobs) were already out of date and are not corrected here.
- About 17,000 more solid boxes at load (floors, walls, parapets, steps) and, by estimate, 130,000 more static
  vertices. No frame-rate figure was taken.
- Indoors the light is the even warm light the slice gives a room it is not standing in: there are no sun patches under
  the roof light and no pools under the lanterns. A hall-sized room light is future work.
- Storeys here are 6 m, where the slice's storeys code assumes 3 m; the mall does not use that code. The place name
  on screen reads the floor from the mall's own storey height.
- `add_outskirts_01.py` would put the old plan back if re-run (it rebuilds all three malls); `draw_map.py` fails before
  it reaches the malls, as it did before this change.
- Pictures: `Art/StyleTests/mall_inside_26/` (the three floor plans, and the slice from the entrance, the court, a
  bridge, an open shop and an egg hall).

## Later the same day: finished for its level, and its artifacts put right

The owner picked F-BLD-MALL at level 2 for the first slice ("Yes, level 2"), then: "complete the mall and then open the
pr. there are lot if artifacts inside, fix them." Points 2 and 8 above and the fourth consequence are changed by this.

Added (WLD-062):

1. **The moving stairs move.** Each is two flights side by side between its parapets, with a low wall between them. Seen
   from the foot, the right-hand flight carries whoever stands on it up and the left-hand one down, at 0.7 m/s along the
   slope (`Court.carry`, called from the player's step). The treads are a new material, 28, which runs them in the shader
   at the same speed. Residents do not use them yet.
2. **Two lifts.** A round shaft in each side concourse (`plan.lifts`: `pos`, `r`, `door`), through every floor, its door
   towards the court. At the door the player goes up a floor, down a floor, or between the ground and the top, as in the
   towers (a `lift` use point).
3. **Daylight indoors.** The room light knows where the mall's roof opening and egg halls are (`Court.light`, the
   uniforms `uCourtR`, `uCourtE`, `uCourtH`): surfaces under them take up to 2.5 times the sky's light by day, fading over
   some 20 m back under the galleries. Still no sun patches and no pools under the lanterns.
4. **The mall's name** on a painted band over each entrance.

Put right, with the cause of each:

| What showed | Cause | Change |
|---|---|---|
| An orange veil over everything indoors | Each indoor lantern was also a street light; the twelve nearest add a glow along every line of sight | Indoor lanterns are no longer street lights (room materials never took light from them) |
| Dark arcs under the gallery edges, dark ends on the moving stairs | The shade pass (ADR-0028) draws both sides of every face, the picture only the front: a ribbon with no back shades what is seen through it | Parapets, the roof's rim and the stairs' side walls are closed all round; parapet backs are mitred so they meet at each joint |
| Plaster in steps on the ceilings | Floors were drawn as strips 2 m wide and up to 200 m long; across so thin a face the plaster's pattern slips row by row | Pieces of 6 m at most |
| Plaster in blocks, on walls and ceilings | 1,700 m from the map's origin the shader's noise is given numbers too big for a float, and two cells no longer agree on the corner they share | The mall's indoor plaster (material 3 inside its rect, and material 27) uses the same noise with its lattice wrapped every 256 cells (`nsW`). The rest of the town is untouched: changing its plaster's pattern is the owner's call |
| Gaps between the windows of shut shop fronts, with the back of the building showing | The windows had no wall behind them | A wall behind them |
| A blotched cap in the middle of the roof light and of each egg | The lattice was closed where it would pinch to slivers at the crown | An open eye in a plain ring there (inside only; outside the cap stays) |
| Ragged, stepped balcony edges in the egg halls | 2 m squares drawn along slanting walks | No squares inside an egg hall on the upper floors; the balconies are the walks' own pieces, straight to their edges |
| A dark band every 6 m up the egg halls' walls | A dark skirting drawn on every floor's length of the drum | Only at the hall's floor |

Still not built: visitors and crowds (F-BLD-MALL level 2 lists F-NPC-POP at 2; it is at 1, and that pick is the
owner's), buying in a single shop (the mall is bought from as one place, as before), names on the shops (their units
have numbers, not names), residents on the moving stairs and in the lifts. The town's other plaster far from the origin
has the same blocks as the mall's had.

Checked in the slice: carried up and down all four moving stairs; both lifts land on a floor at 6 m and 12 m; the
balconies in the egg halls hold; no errors in the console by day or at night.
