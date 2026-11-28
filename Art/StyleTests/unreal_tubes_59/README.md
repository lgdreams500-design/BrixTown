# The glass travel tubes in Unreal, beside their reference (CITY-114; look gate, ADR-0058)

Reference: `Art/Reference/Vehicles/VehiclesGlassTube_*` (approved by the owner, 2026-11-28). Shots: Market Square's and
the Stone Oven Bakery's booths at 12:00, 20:00 and 22:30, the square from above, and a ride from Market Square to the
bakery (`-BrixTube=market_square,stone_oven_bakery,<s>`). The sheet: `tubes_look_gate_sheet.jpg`.
`python Tools/unreal/look_check.py` on the eight booth shots: one fail, `tube_bakery_2000` burnt 27 %, the town's low
evening sun on the bakery's front (the street's light, not the tube's); the rest pass.

| | Answer |
|---|---|
| G1 Shade and depth | Yes for the booth (plinth, cap and funnel darker underneath); the glass itself has no shade, as in the reference. |
| G2 Shadows | Yes: posts, bands, cradles and the booth cast shadows; the glass casts none (the reference's casts a faint one). |
| G3 Surfaces | No: the brass and the iron are one flat fill (the greybox's matte paint), not the reference's metal. |
| G4 Colour | Yes: a faint sea-green glass, dull brass and dark iron in the town's colours. |
| G5 Whites and blacks | Yes for the tubes; the bakery's front burns at 20:00 (the town's light). |
| G6 People | Yes: the rider is lit as the street, seen through the glass. |
| G7 Lamps | Yes: the booth's lantern glows and lights the booth at night with the street lamps. |
| G8 Whole | No: the tubes pass through street trees' crowns (Baker Street, the square); a market stall stands close to the Market Square booth. |
| G9 The hours | Yes: noon, dusk and night read, only the light changing. |
| G10 Nothing over the picture | The game's own HUD (money, minimap, the "New in town" note) shows in the ride shots, as in play; no debug text. |
