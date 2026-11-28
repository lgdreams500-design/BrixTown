# The glass tubes over the pavement, posts and booths clear of the street (CITY-108 to CITY-110; look gate, ADR-0058)

The owner, 2026-11-28: "you need to fix the tube map as it's overlapping bus stop etc at some place. these are basic
common sense checks". Before this, every post stood in the road (683 of them: 35 on zebras, 32 at bus stops, 83 inside
junctions), three booths stood 4 m from a bus stop, four 1.5 m in front of their building's door, nine faced a wall
0.75 m off and one stood in a plant bed. Now the lines run 6 m off each street's middle, over the far pavement from the
street's booths; the 674 posts are map data on the pavement, and booths and posts keep clear of the road and everything on
it (`World/rules/tubes.json` `place`; `Tools/citysim/test_tubes.py`). The ride is faster (60 m/s, no stop at the top of the
draw-in), and the ride's camera stands beside each booth on a side clear of crowns and buildings.

Maps (`map_*.png`, drawn from the data): Market Square, Baker Street and the bakery, Eastgate Road, Westgate: tube lines,
posts, booths, bus stops, zebras, parking bays, lamps and trees.

Reference: `Art/Reference/Vehicles/VehiclesGlassTube_*` (it draws the tube 9 m up; the tubes run at 17 m). Shots at 12:00
and 20:00: Baker Street, the bakery's corner, Market Square, the square from above; standing by the Market Square booth, in
the bakery's booth after landing, a step out of its door both ways; a ride from Market Square to the bakery at 0.6, 1.2,
3.0, 6.3 and 7.2 s. The sheet: `tubes_look_gate_sheet.jpg`. `python Tools/unreal/look_check.py` on the nine street and
standing shots: no fail. (A shot by Eastgate Road had the camera inside the mall's wall, a wrong tour position: left out.)

| | Answer |
|---|---|
| G1 Shade and depth | Yes for the booth; the glass has no shade, as in the reference. |
| G2 Shadows | Yes: posts, bands and booths cast shadows; the glass none (the reference's a faint one). |
| G3 Surfaces | No: brass and iron are the greybox's matte fill, not metal. |
| G4 Colour | Yes: faint sea-green glass, dull brass, dark iron in the town's colours. |
| G5 Whites and blacks | Yes: no fail. |
| G6 People | Yes: the rider seen through the glass, lit as the street. |
| G7 Lamps | Yes at 20:00: the booth's lantern lit with the street lamps. |
| G8 Whole | Yes for the street: posts and booths on the pavement, none on a bus stop, zebra, bay or door, the ride's camera clear of the crowns at the start (`ride_01`). No: `stand_out_door`, a step out of the bakery's booth facing away, had the camera inside the booth behind its brass rib (the booth then let the camera through); the booth now stops the camera as any wall except while standing in it, built after these shots and not yet shot. Against the reference the tubes at 17 m read as thin lines high up. |
| G9 The hours | Yes: noon and dusk read, only the light changing. |
| G10 Nothing over the picture | The game's HUD (money, minimap, "New in town") and the ride prompt in play; no debug text. |
