# ADR-0039: Eastgate Mall's outlets

Date: 2026-11-24. Status: accepted for building (owner's request in chat, 2026-11-24); F-BLD-MALL is picked at level 2 for
the first slice.

## Context

With the court reworked (ADR-0038) every shop front was still a flat wall of lit windows, or, for the sixteen shops
whose staff are in, a counter before a painted back wall. The owner sent a picture of a mall's shop fronts (glazed fronts
between columns, a red sign band with the shop's name, bright rooms full of goods) and said: "first I want the interiors
to be improved ... instead of generic flat walls can you build these outlets in the mall. plan the types of outlets and
the objects that need to go. I am thinking of cloths shops, toy shop, electronics shops, shoes shops, guitars, etc etc.
food outlets entertainment rides etc."

The slice cannot draw see-through faces, it has room for 256 painted names of which the town uses 144, and the mall's
ground-floor units must keep their kinds (shop or food) and sizes, because jobs are counted from them.

## Decision

1. **Kinds of outlet are data.** `World/rules/mall_outlets.json` lists eighteen kinds, each with its trade (shop or
   food), its sign colour, its floor, three walls of fixtures, what stands in the middle, what is in the two windows, and
   a few names; and how many of each kind a floor gets. `mall_inside_01.py` gives every unit with a front a kind and a
   name (like kinds spread apart, never two next door) and copies the kinds used into the lot's plan. A unit's `kind`
   (shop or food) and its rect do not change, so the jobs do not.
2. **The mix.** Ground floor: clothes, shoes, electronics, jewellery, books, flowers and gifts, home, sports, toys, music,
   and the two cafes. Floor 1: toys, sports, music, books, home, clothes, shoes, electronics, with cafes, bakeries, ice
   cream and juice. Floor 2: a food court (burgers, noodles, pizza, ice cream, cafes, bakeries) and arcades, with more
   clothes, toys, electronics, music, books and sport.
3. **What an outlet is in the slice** (`outlets.js`): a front open between two piers, with slim posts and a transom, a
   sign band in the kind's colour with the outlet's name lit on it (material 29: the painted lettering, lit from within),
   a display plinth either side of a way in; behind it a room as deep as the unit, to 9 m, with its own floor, pale
   walls, a back wall in the sign's colour and light panels in the ceiling. It can be walked into on every floor.
4. **What stands in it** is built from a small vocabulary the catalogue names: on a wall rails of clothes, shelves of
   goods (folded clothes, boxes, books, shoes, balls, pots, jars, gadgets, soft toys, bread, bottles, lamps), guitars,
   lit screens, a mirror, rackets, arcade cabinets, posters, a servery with lit menu boards; in the middle tables, a
   round rack, a ball bin, benches, amplifiers, a drum kit, a keyboard, lit glass cases, cafe tables and chairs, an air
   hockey table, a claw machine, flower buckets, a plinth with a thing on it; in the window mannequins, stacks, plants.
   All of plain boxes and a few faceted balls, in the town's muted colours: chunky and few (ART_DIRECTION 2.2).
5. **No glass.** The fronts are open frames. Glass like the picture's needs see-through faces, which the slice's one
   mesh does not have.
6. **Staff.** The sixteen outlets whose staff are in keep their counter 2.2 m in, now only as long as the staff stand
   wide, with a till. The others have their goods and no one serving: more staff means dealing careers again.
7. **Rides.** A carousel stands on the court floor where the north garden was, ten horses under a stepped canopy, and
   three coin rides (a rocket, a pony, a car) before it. They stand still; each seat can be sat on. Rides that turn and
   carry the player are proposed as a feature of their own, F-BLD-RIDES, with no level picked.

## Consequences

- 189 outlets, 68 names. The world mesh grows by about 1.3 million vertices (11.3 to 12.6 million). Timed with the
  preview hidden, a frame inside the mall took as long as one in Old Town's square (about 40 ms each, a figure that says
  nothing by itself); no frame rate was measured on a visible screen.
- `residents.json` is untouched; the ground floor keeps its 206 posts (WLD-061's test).
- The four big stores in the egg halls are as they were: six stalls and a tree. They are not outlets of a kind yet.
- Nothing is bought in an outlet: the mall is still bought from as one place (F-BLD-SHOP is per lot).
- The plan's pictures (`Art/StyleTests/mall_inside_26/plan_floor_*.png`) do not show kinds.
- Pictures: `Art/StyleTests/mall_outlets_27/`.

## Later the same day: light with shape

The owner sent a picture of the furniture outlet's hall (sun falling in patches from its skylights, lamps laying pools,
shade in the corners) and said of the mall: "fix the lighting and shading it's look too bland. try to get at least this
level as the screenshot".

The outlet's hall is lit as the one room the eye is in (`roomLight`, "here"): its own lamps, a map of what stands on its
floor, the sun through its skylights by the shadow map. The mall is not such a room: it is 240 by 340 m on three floors of
6 m, and it was getting the even light the slice gives every room the eye is not in. Rather than force the mall into the
one-room machinery, the room light has a branch for a court mall (WLD-064):

1. **Sun through the skylights.** For a point indoors the sun's ray is followed up to the roof: it is lit if the ray
   passes through one of the five round skylights, and through the opening of each floor between (worked from the
   openings' own numbers, `plan.court`, not from the shadow map, in which the roof light is solid). A diamond lattice is
   laid over the patch as the roof light's shadow. The patches cross the floor at midday and climb the galleries in the
   afternoon. Bridges and the moving stairs cast no shadow in them.
2. **Bright atria, dim walks.** Under the skylights and in the egg halls the sky's light is three times the usual; back
   under the galleries it is about half.
3. **Pools of light.** Every lantern and every outlet has a light in a list (`Court.light().lamps`, 249 of them); each
   frame the 32 nearest on the eye's floor, fading out by 44 m, are given to the shader, which lays a pool under each: a
   lantern's warm, an outlet's whiter. Beyond their reach an even light stands in for them.
4. **Shade.** Walls and whatever stands on a floor darken down to it; the screen-space shade of ADR-0028 does the rest.
5. **Haze.** Indoors in the mall the haze is a twentieth of the outdoor haze (it was a seventh).

Not done: shadows cast by lamps; shadows of bridges and stairs in the sun patches; light bounced off sunlit floors;
the same for the other two malls (they have no inside). Timing is as before: not measured on a visible screen.

Pictures: `Art/StyleTests/mall_outlets_27/light_*.jpg`.
