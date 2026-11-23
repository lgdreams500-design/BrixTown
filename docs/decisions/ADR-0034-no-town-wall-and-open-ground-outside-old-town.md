# ADR-0034: No town wall, and open ground outside Old Town

Date: 2026-11-24. Status: accepted (owner's request in chat, 2026-11-24; plan shown and approved: "go ahead").

## Context

Old Town stood inside a stone wall with four gates (CITY-068). Outside it the newer areas (the three communities, the
malls, the park, Civic Row, the furniture outlet) sat on bare field, and every grid cell between them that no block
covered was empty. The owner asked to "remove city wall and fix the areas which outside of the wall ... they are
currently barren and feel undone, use vegetation other props etc, to fill in. think of adding roads, parks, plots for
sales, even dense woods etc. to make them look complete, but no building please". The plan
(`Art/StyleTests/outskirts_plan_23.png`) was shown first and approved.

## Decision

1. **The wall is taken down, not hidden.** The slice no longer builds the wall, its gate towers or the gate names.
   Old Town's paving ends at a low stone kerb on the old line, with openings for the four gate roads and the tram rails.
   `city.json` keeps `wall` as the edge of Old Town's street grid, since the road network, walking rings and traffic
   still use that line (`world.in_wall`). The streets keep their names (West Wall Street and the others).
2. **The open ground is data, like the rest of the map.** Six new block templates, `woods`, `common`, `orchard`,
   `fields`, `plots` and `meadow`, cover the 17 empty areas. They are written by `Tools/worldgen/add_green_01.py`, which
   writes the same files each time it runs (a test checks this). They have **no lots**, so nothing is built and no
   resident's day changes. Their props are in the set `wild`. A block may carry `tracks` (gravel ways drawn but not
   driven).
3. **Roads stay as they are.** New driven roads would change the whole traffic plan (ADR-0030), so the open ground has
   footpaths and gravel tracks that are walked, not driven. Every tree and prop keeps clear of the gate roads, links,
   lanes and tram rails.
4. **Plots for sale are scenery for now.** F-MAP-PLOTS is at level 0 ("None") for the first slice, so the plots have a
   fence, pegs and a "FOR SALE" board but no owner, price or sale. Making them buyable needs the owner to raise that
   feature to level 1.
5. **New trees and plants are rules data.** `vegetation.json` gains pine (evergreen, cone crown), oak, birch (slim crown,
   white bark) and apple (blossom), and the wild plants holly, bracken and hawthorn. Pines are evergreen in citysim (a
   tested rule). The tree rows sent to the slice carry the crown shape.
6. **The woods are drawn cheaply.** About 4,100 trees stand in the woods. The slice draws them with fewer facets, and
   they are not breakable (cars cannot reach them).

## Consequences

- The slice has 93 blocks instead of 76, 6,466 trees instead of 1,450, and about 10.6 million static vertices instead
  of 8.8 million. In a rough side-by-side in the browser pane the frame rate showed no drop.
- The slice's sign lettering atlas grew from 128 to 256 names: the town already needed 144, so 16 names were silently
  missing before this change.
- The look follows the existing style (faceted crowns, flat muted colour). It adds no new reference, so the look lock
  (ADR-0004) is unchanged.
- Spacings, counts and the new species' growth numbers are `[ASSUMPTION]` until tuned.
- The engine build (TECH-003) inherits the templates and props as data. It will need its own drawing for them.
