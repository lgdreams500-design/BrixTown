---
adr: ADR-0028
title: Corner shade everywhere from the screen's depth (ambient occlusion) in the browser slice
status: accepted          # the owner asked for it in chat on 2026-11-22 ("is there a way to add corner shadows universally", then "Yes, build it"); strength and reach set by eye [ASSUMPTION]
date: 2026-11-22
relates_to: [ART-001, ART-004, FURN-020, ADR-0004, ADR-0018]
---

## Context
In the path-traced reference renders every corner, crease and gap goes dark on its own: the foot of a wall, a window
reveal, under a sofa, an arm against the body (INTERIOR_STYLE R1). The slice had no general way to do that. It had
shade made for particular cases: contact shade under 16 listed pieces of street furniture (ADR-0018), a floor map of
what stands in a room, and, in the furniture outlet, corner shade worked out from the hall's own layout (walls, roof,
trusses, racks). Each new place needed its own rule, and anything not covered stayed flat. The owner asked whether
corner shadows could be added to the whole world at once.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| More hand-made rules, place by place | Exact where made; costs little | Never universal; every new building, prop or person needs its own |
| Bake occlusion into the city's meshes at load | No cost a frame | Static things only (not people, cars, furniture near the camera); big flat faces have too few corners to hold it; slower loading |
| **Ambient occlusion from the screen's depth (SSAO)** | Everything on screen, moving or not, one mechanism; matches the renders' darkening of corners | One more pass a frame; an estimate from what is on screen, so it can halo or shimmer if pushed; needs tuning |
| Real global illumination | Closest to the renders | Far more work; belongs in the Unreal build (Lumen, INTERIOR_STYLE R1) |

## Decision
Screen-space ambient occlusion in `Prototype/play/play.js`, on by default at full picture quality:
- After the depth pass, the world, the nearby rooms and the people are drawn again at half size into a depth picture.
- `aoProg` rebuilds each pixel's position and facing from that depth and looks at 10 points in a spiral round it,
  out to 1.2 m. The more of them stand in front of the surface, the less sky and bounced light reach it (strength 6).
  It fades out between 27 and 45 m from the camera, where depth is too coarse and corners too small to matter.
- `aoBlur` smooths it over 3 × 3 points without carrying shade across an edge to something far behind.
- The lit pass darkens only the ambient part of the light (sky, bounce, lamps), never the direct sun, which the sun's
  shadows already handle. Indoors `roomLight()` reports its sun part (`gRoomSun`) for this.
- Key **O**, or `?d_ao=0`, switches it off to compare. `?d_ao=2` shows the occlusion alone, for tuning; `?d_aoR=`,
  `?d_aoI=` and `?d_aoFar=` set the reach, strength and fade distance. Like the sun's shadows it is off below full
  quality (key Q, or a slow machine below 27 frames a second).
- The hand-made shade of ADR-0018 and the outlet stays for now. It is tuned to read with this on; removing it, where this
  does the same job, is a later step to be judged side by side.

## Consequences
- Seen at 13:00 in the outlet, 10:00 on Linden Street and close up: the bases of walls, window reveals, the kerb, café
  chairs, the floor under beds and sofas, the foot of a wardrobe and the gap between an arm and the body darken; open
  floor, open walls and the sky do not. From behind, the line where a sleeve meets the back reads as a soft shadow.
- Cost: in a close room view 53 frames a second without it, 41 with; on the street no difference could be measured in
  the browser pane, which holds the rate near 28. It turns itself off with the soft picture on slow machines.
- An estimate from the screen: what is off screen or hidden behind something casts no occlusion, and a thin object in
  front of a far wall can leave a faint dark edge. Strength and reach are set by eye against the reference renders.
- Unreal does this with Lumen and its own ambient occlusion; nothing here carries over except the look it aims at.
