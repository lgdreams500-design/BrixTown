# The glass travel tubes raised over the street trees (CITY-108, CITY-114; look gate, ADR-0058)

The owner, 2026-11-28: "Raise the tubes" (at 9 m they ran through the street trees' crowns; `unreal_tubes_59`, G8 no).
They now run at 17 m, a metre over the tallest grown crown under a line (a plane's, 15.4 m; `Tools/citysim/make_tubes_01.py`).

Reference: `Art/Reference/Vehicles/VehiclesGlassTube_*` (approved 2026-11-28; it draws the tube 9 m up). Shots: Baker Street
looking west along the line at 12:00 and 20:00, Market Square's and the Stone Oven Bakery's booths at 12:00, 20:00 and
22:30, the square from above, and a ride from Market Square to the bakery at 1.0, 2.4, 5.0, 9.8 and 11.2 s
(`-BrixTube=market_square,stone_oven_bakery,<s>`). The sheet: `tubes_look_gate_sheet.jpg`, the references at the top.
`python Tools/unreal/look_check.py` on the ten street shots: one fail, `tube_bakery_2000` burnt 26 %, the town's low
evening sun on the bakery's front (the street's light, not the tube's, as in `unreal_tubes_59`); the rest pass.

| | Answer |
|---|---|
| G1 Shade and depth | Yes for the booth (plinth, cap and funnel darker underneath); the glass itself has no shade, as in the reference. |
| G2 Shadows | Yes: posts, bands and the booth cast shadows; the glass casts none (the reference's casts a faint one). |
| G3 Surfaces | No: the brass and the iron are one flat fill (the greybox's matte paint), not the reference's metal. |
| G4 Colour | Yes: a faint sea-green glass, dull brass and dark iron in the town's colours. |
| G5 Whites and blacks | Yes for the tubes; the bakery's front burns at 20:00 (the town's light). |
| G6 People | Yes: the rider is lit as the street, seen through the glass. |
| G7 Lamps | Yes: the booth's lantern glows and lights the booth at night with the street lamps. |
| G8 Whole | Yes for the trees: the tubes pass over every crown, and the ride's camera stays clear of them (`ride_03_run`). No against the reference: at 17 m the tubes read as thin lines high over the street and the booths' risers as tall poles, where the reference's tube at 9 m is close over the street; a market stall stands close to the Market Square booth. |
| G9 The hours | Yes: noon, dusk and night read, only the light changing. |
| G10 Nothing over the picture | The game's own HUD (money, minimap, the "New in town" note) shows in the ride shots, as in play; no debug text. |
