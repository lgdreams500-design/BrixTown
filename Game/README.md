# Game

The Unreal project of Brix Town (ADR-0025): Unreal Engine 5.8 (TECH-001), C++ (TECH-002). It was set up by hand, not
from a template (the installed engine has no templates folder), and it has no map or asset files of its own yet: it
starts on the engine's empty map and the game mode builds the town from `World/` at load time (WLD-020).

| Path | What it is |
|---|---|
| `BrixTown.uproject` | The project |
| `Source/BrixWorld/` | The map as data: reads `World/<city>/city.json` and its block files into plain structs, and checks them (WLD-022). Needs only Core and Json (TECH-003, TECH-004). Its tests are here too |
| `Source/BrixTown/` | The game: `BrixGameMode` (load the city, the light, the player's start), `BrixTownActor` (the town as instanced shapes), `BrixCharacter` (the player in third person) |
| `Config/DefaultGame.ini` | Every tunable number and colour (TECH-005) |
| `Config/DefaultEngine.ini` | The startup map, Lumen and virtual shadow maps |

## What it shows so far (grey-box)
The starting city at the slice's sizes: a box for each building (its footprint, turned by its `angle`; `floors` × 3 m +
0.4 m high, at least 3.6 m), a slab for each block (green for parks and open ground), each tree as the slice draws it,
the sun low in the east, sky, haze, Lumen. You walk it from the city's spawn with the slice's keys: W A S D, mouse,
Shift to run, Space to jump.

## Buildings (ADR-0046)
Each building is laid as the Blender building kit lays it: a core set back by the wall's depth (0.32 m), front walls
between real openings, bays of 3.2 m on every facade, the lot's own door, a stone ground storey in Old Town and the
Centre, all-stone clinics and workplaces, a tiled gable roof on homes and shops (a parapet on the rest). The windows,
doors and shutters are the kit's own pieces: `Tools/blender/unreal_kit_36.py` makes them (Blender, headless) into
`Art/Export/unreal_kit_36/`, and `Tools/unreal/import_building_kit.py` imports them into `/Game/Kit` with Nanite. Run
both again after changing a piece, and `Tools/unreal/make_greybox_material.py` before them after changing the material.
The whole town is 15 groups of instances, one per mesh, each instance carrying its own colour, all through Nanite: on
the owner's laptop at High it runs at 63-66 fps, as fast as the grey-box (ADR-0046). The binaries are lockable in LFS
(ADR-0002): `git lfs lock <file>` before running the scripts that remake them.

## Buildings from their designs, and the building designer (BUILD-002, BUILD-004 to BUILD-007, BUILD-016 to BUILD-020; F-BLD-DESIGN; ADR-0052)
Every building is drawn from its design, as the slice draws it: `Game/Data/inside.json` carries each lot's design (`dsg`,
world.designs()) and the designer's kit, planning rules and fixed run (`builder`); `BrixWorld/BrixBuilder` is builder.js
(built(), allowed(), problems(), cost(), options(), check()), and `BuildLot` takes from `BrixBuilder::Built()` each
facade's plaster and stone or brick (to the eaves where the design says), the roof's tiles, the window frames and the
shutters (or none). The kit's geometry is ADR-0046's; designed houses keep their colourway. At the owner's word
(2026-11-25) this replaced the random colours Unreal picked before. `BUILD002_every_building_is_built_from_its_design`
(the inside built from each of the 959 designs is Python's), `BUILD005_remodels_match_the_python` (builder.check_script():
what each remodel offers, its problems, its cost line by line, the built front and inside, against Python's).
The remodel designer, as the slice's life.js remodelPanel(): from Properties, Remodel (the whole building) or Redecorate
your rooms (the storeys of the units you own), the card at the left of the screen and the camera over the building,
three-quarters from its front (BUILD-020; Turn the view goes round it; the inside tab looks down on the storey). Outside:
the walls of all sides or one facade (plaster, stone or brick and its colour, brick to the eaves), roof tiles, window
frames, shutters and their colour, balcony, keystones, corner stones and wall lantern, as builder.options() offers them in
the district's palette; inside, each storey's walls and floor. Each choice shows on the building at once
(`ABrixTownActor::Repaint`: its instances folded away and the building built again); the work is priced line by line;
what would stop it is said (builder.check()); Pay and build pays through the ledger (`sys:builders`) and the remodel is
there at once (F-BLD-CONSTRUCT level 0, instant); Undo, Start again; Esc leaves it as it was. Remodels are kept in the
save (`QST008_remodels_are_kept_in_the_save`). To see it in a test shot: `"-BrixLife=open,current;give,3000000;buy_lot,b_9_6_l4;remodel,b_9_6_l4;set,walls.*.plaster=peach;set,roof.tiles=slate;set,openings.frame=teal"`
(and `;pay` to build it). Shots: `Art/StyleTests/unreal_designer_39/`. Not yet: the room plan (BUILD-021 to BUILD-023) and
furnishing in the designer (FURN-051, FURN-052, FURN-055), the window class's sizes, balconies, keystones, corner stones
and wall lanterns drawn from the design (offered, priced and kept, but laid by the kit's rules), the awning in the
design's accent, the cut-open storey (BUILD-020), dragging the view; building new on a plot (BUILD-008: F-MAP-PLOTS is at
level 0, no plots).

## Going inside (CITY-013; ADR-0048)
959 of the 965 enterable buildings can be walked into (all but the furniture outlet's hall and the malls; Eastgate Mall
and the furniture outlet have their own insides, below), as the slice
builds them (play.js building()): the ground storey is hollow behind its kit walls, with a floor in the storey's finish
(boards or flags), the walls plastered inside in its colour and a ceiling; above it the core is still solid. The way in is
the kit's own door standing open (`SM_Kit_DoorPlainOpen`, `DoorArchOpen`, `DoorCivicOpen`, `ShopDoorOpen`, made by
unreal_kit_36.py: `WASTED_ONLY=<pieces>` makes just those, `BRIX_KIT_ONLY=<pieces>` imports just those) at the lot's door,
where the residents go in: `BrixWorld/BrixInside` works it out (`BrixDoorwayOf`), with each lot's inside from
`Game/Data/inside.json` (the design's floors and walls per storey, the stair, lift, storeys, levels, flats and planned
rooms; written by `build_play.py`). Windows have unseen boxes in them, so only the doorway lets you through; on a hollow
storey their glass is clear (`/Game/Greybox/M_Clear`, `Tools/unreal/make_clear_material.py`), so the room looks out and
the sun comes in. The haze is turned down while the camera is indoors (`IndoorHaze`). The owner approved this light on
2026-11-25. `-BrixInside=<lot id>` puts the player in a building, `-BrixWalkIn=<lot id>` walks them in through its
doorway (the log says how long it took), `-BrixShut` builds every building solid as before; a test shot ignores the keys
and mouse of whoever is at the machine and logs where the player is. A street at High: 46.7 to 48.6 fps, 45.8 to 49.5
with `-BrixShut` (noisy: other sessions were running Unreal); 50 fps inside a row house. Not yet: furniture, the upper
storeys and their stairs, residents indoors.

## Eastgate Mall (WLD-057 to WLD-063, WLD-066 to WLD-068; ADR-0037 to ADR-0039, ADR-0041)
Eastgate Mall (lot `b_16_5_l0`) is built inside and out as the slice builds it (court.js, shell.js, outlets.js; play.js
mallShell()): `BrixWorld/BrixMall` is the port, giving the same 185,841 drawing calls with the same numbers (its hash() fed
the same strings, Math.hypot as V8 sums it), from the lot's `plan` and `shell` in `World/city_01/blocks/b_16_5.json`. Outside:
the skin of plaster ribbons swelling and curling in, lifted off the ground in an arch on each side; the four ochre pierced eggs
and the cream roof light. Under each arch a porch of ribbon walls sweeps in to the arched opening in the hall's wall, with its
line of light. Inside: three floors round the court of five atria (6 m storeys), every floor's edge a dark ribbon with a line
of light under it and a pale upstand, plants trailing over the long ones, bridges at the pinches, four moving stairs, a lift in
each side concourse, the round skylights with their deep rims, the egg halls under the eggs (the lattice open to the sky from
inside, their stalls, lanterns and linden), the fountains, gardens, planters, benches, kiosks, the carousel and the coin rides,
lanterns under the galleries; and the 189 outlets of World/rules/mall_outlets.json, each with its front (piers, the sign band
in its colour with its name lit on it, plinths either side of the way in), its room as deep as the unit (its own floor, walls,
light panels) and what it sells (rails, shelves, guitars, screens, cabinets, serveries; tables, racks, bins, drums, glass cases,
the window's mannequins or plinth), with a counter and till where its keeper works today. `ABrixMall` lays the calls as the
town's own faces, cut into 40 m squares so what is out of view is not drawn: 598,294 triangles in 263 meshes made at load
time (2.2 s), in M_Facets outside, `/Game/Mall/M_MallRoom` indoors (with the floor tiles' two tones and joints, and the court's
even room light: the faces made at load time get no share of Lumen's bounced light, so without it the ceilings were black),
`M_MallLit` for the lit panels, lines of light, screens and lantern glass and `M_MallLattice` for the eggs' diamond openings
(made by `Tools/unreal/make_mall_materials.py`); the 39,638 solids are unseen blockers; the names are lettering on the sign
bands. The moving stairs carry whoever stands on them (`BrixMallCarry`, court.js carry(): up on the right-hand flight seen from
its foot, down on the left) and lift them onto each step as it comes; E at a lift's call takes the player up or down a floor or
between the ground and the top, as at the towers. The four lanterns and shop lights nearest the camera on its floor give real
light (`NearLights`); the haze is off indoors and the town's sounds are muffled (RoomAt). Only the mall whose plan is a court
with its entrances is built so (`BrixMallCan`): Westgate (`b_1_5_l0`) and Northgate (`b_7_0_l0`) stay the town's solid boxes,
as the owner asked. Tests: `WLD058_eastgate_is_built_as_the_slice_builds_it` (every call against the slice's, in 465 runs of
400 with a checksum each and 9,600 calls one by one, the signs' words, the 249 lamps; `Tools/unreal/mall_values.py` runs the
slice's code in headless Edge into `Tests/values/mall_city_01.json`: run it after changing court.js, shell.js, outlets.js or the
block file), `WLD066_every_outlet_has_its_room`, `WLD062_moving_stairs_carry_and_lifts_go_between_floors`,
`WLD058_only_eastgate_has_an_inside`.
Buying at each outlet (WLD-066, ECO-013, ECO-014): standing in an outlet's room, on any floor (`BrixOutletAt`, outlets.js
at()), the offer names that outlet ("B Buy at Thimble & Thread") and B opens its own counter as life.js does: its name, its
kind, the mall and the floor, its goods from World/rules/mall_goods.json in the buy menu's layout B with their stock and
today's prices (the economy's outlets, ECO-013), each paid through the ledger; a thing for the home goes to the belongings,
clothes to the wardrobe. On the walks, in the court and in the corner stores B says to walk into an outlet. Who is there
(CITY-105, CITY-106, WLD-067, WLD-068; `SeatBrixDayInMall`, play.js resident()): each of today's 189 keepers (shop owners and
food outlet owners) stands behind their outlet's counter on its floor facing the walk, the six guards (in uniform and cap)
and six janitors at their posts, the residents who come to shop before a counter and those who come to eat at a food
outlet's table or on a court bench, at their floor's height; the mall's residents are drawn while the camera is in it, as in
any building. Tests: `CITY105_every_outlet_has_a_counter_its_keeper_stands_at` (every outlet's one keeper's place in its
room behind the counter, the counter drawn as today's keeper works there, each keeper of the day stood at it facing out; the
guards and janitors at their posts), `CITY106_visitors_shop_at_the_counters_and_eat_at_the_tables` (the day's 43 stays
shopping and 195 eating, on all three floors, at their places). `-BrixCounter=<an outlet's unit id>` (b_16_5_u4 is Thimble &
Thread) stands the player in it and opens its counter. `-BrixInside=b_16_5_l0` stands the player 30 m in from the south door (`MallInside`),
`-BrixWalkIn=b_16_5_l0` walks them in, `-BrixLift=b_16_5_l0` rides the west lift to the top floor, `-BrixNoMall` builds it as
the town's box again. At High: 28 to 44 frames a second inside, 21 riding the moving stairs (noisy: other sessions were running
Unreal). Not yet: sitting on the benches and rides, the ribbon wall's lit joints and the openings' lanterns at night, the
moving stairs' treads moving, the goods for the home in the outlets' windows (the stand-ins stand there, as in the slice without
its life data), and the shell's lanterns' light outside.

## The furniture outlet (FURN-020, FURN-021; ADR-0027)
The furniture outlet outside the East Gate (lot `b_14_8_l0`) is built as the slice builds it (outlet.js; play.js building()
and furnish()): `BrixWorld/BrixOutlet` is the port, with the play.js helpers it draws with (sideOf, fbox, SP.win, SP.lamp,
archHead, the lettering), giving the same 2,851 calls. The long hall: a stone base and plaster walls, a parapet, the roof on
timber trusses with their iron ties and the skylights between them, the tall arched windows with their arch stones on three
sides, the teal sign band with its name over the door, the door's stone surround and its two lanterns, the three loading doors
onto the car park. Inside: the painted walking route with its arrows, the sixteen room sets (three plaster walls, a board
floor, a name board, two price tags), the tills, the sales desks, the cafe counter and its tables and chairs, the packing
tables, the warehouse's three rows of timber racks of flat-pack boxes in the company's band, the lanterns under the trusses.
The furniture in the sets is the furniture kits' own pieces in each set's line, where the slice draws boxes, as in the Unreal
homes (ADR-0050): kit 17's living room (sofa, armchair, coffee table, rug, floor lamp, bookcase, bed, bedside table, wardrobe;
its stool, trunk, crate or pouffe as the slice's boxes) in the first four, and the twelve rooms of kit 18 (living room,
bedroom, children's room, study, kitchen, dining room, bathroom, hallway, decoration, garden, cafe and shop fittings), 149
pieces in all; the 80 meshes of the garden, cafe and shop pieces, the bar stool and the cushions were imported for it
(`BRIX_FURNITURE_ADD=1 Tools/unreal/import_furniture_kit.py`, which now wants the outlet's pieces too and, so, only adds the
new). The car park south of the hall: its tarmac, painted bays and aisle line, and its fourteen cars, each one of the
traffic's own Soft box models in the colours its seed gives (cars.js parked(), `ABrixCars::BuildParked`). It is laid by a
second `ABrixMall`: its walls are in the way, its lanterns light the hall near the camera, the haze is off inside. Its staff
(FURN-021) stand at the tills, desks, cafe and packing tables facing the door, and the residents who come to browse stand by
the room sets and the racks (`SeatBrixDayInMall`). Tests: `FURN020_the_outlet_is_built_as_the_slice_builds_it` (every call
against outlet.js run in headless Edge by `Tools/unreal/outlet_values.py`, which records each piece of furniture as the piece
it is and where it stands, into `Tests/values/outlet_city_01.json`), `FURN021_staff_and_browsers_are_at_the_outlets_places`.
`-BrixInside=b_14_8_l0` stands the player inside its door; `-BrixNoOutlet` builds it as the town's box again. At High: 31 to 33
frames a second in the hall and over the car park (noisy: other sessions were running Unreal). Not yet: buying at the outlet.
The slice has no counter there: furniture is bought in the building designer's Furnish tab (FURN-051, FURN-052, FURN-055),
and buying at the outlet's tills with delivery (FURN-022 to FURN-029) is proposed, not accepted. Also not yet: sitting on
its sofas and chairs, and the vans of the delivery bay.

## Storeys and stairs; residents at home inside (WLD-050, WLD-052, WLD-053, CITY-013)
Every hollow building with a stairwell (462 of two storeys or more) and every designed house is hollow to its roof, every
storey reached by its stair, as storeys.js and houses.js build them: a floor slab on each storey with a hole the flight
below comes up through, the switchback stair (two flights a storey, a spine wall between them, rails where a floor stops at
the hole) or a row house's straight stair (one flight a storey up the side wall, its hole 0.3 m wider on the open side so a
head clears it, a rail along it), a designed house's sixteen steps up the hall to the landing; the floors at the heights of
`BrixFloorUp` (the ground floor 0.08 m over the paving, floor f 3f + 0.08 m; a designed house 0.3 m and 3.3 m storeys),
each in its own finish and wall colour, the windows of every storey clear. The planned homes are furnished on every floor.
`WLD052_every_storey_is_reached_by_its_stair` checks the floors' heights and the flights (892 of them); `-BrixWalk=x,z;...`
walked the player up a row house's stair to the first floor. Residents at home lie in their beds and sit on their sofas
(`SeatBrixDayIndoors`: the day's bed and room stays on the nearest bed or seat of their floor; people.js's lying pose,
turned so the head is at the bed's head); anyone else inside stands on their floor. As in the slice, those inside are
drawn only while the camera is in their building. `CITY013_every_indoor_place_is_inside_its_lot_on_its_floor` checks all
23,203 indoor stays of the day (4,547 in a bed, 4,406 on a seat). By night the rooms of the home the camera is in have
their lamps. The five towers have their lifts (WLD-051, storeys.js): a shaft from the ground to the top floor with steel doors and a lit
call button on every floor and its house on the roof; E beside it goes up or down a floor, to the ground, the top floor or
the roof (`BrixLiftCalls`, `WLD051_towers_have_a_lift`; `-BrixLift=<lot>` rides one to the top in a test shot). As in the
slice only the camera's floor and the floors either side of it are furnished in the building it is in (elsewhere the ground
floor): the top of an eleven-storey tower ran at 15.8 fps with every floor furnished, 38.7 like this. The 90 buildings of flats
have every flat's and store's own walls with a doorway at its door (storeys.js partitions(), `BrixFlatWalls`), and their 653
planned flats are furnished from their plans (homes.js flats(), `BrixFlatHomeOf`) at each flat's own household's level, its
residents in its own beds and on its own sofa (`FURN060_every_planned_flat_is_furnished_inside_its_walls`); a street at High: 41.3 to 41.5
fps, 43.2 with `-BrixShut` (noisy).

## Shops, cafes, workplaces and civic buildings furnished (CITY-013)
The 240 hollow buildings with no plan of their own (shops, cafes, workplaces, clinics, the bank, library, museum, cinema,
dealer, depots, schools, the fire and police stations, and the homes with no plan) are furnished at their places as the
slice furnishes them (play.js furnish(), ported in `BrixWorld/BrixFurnish`, on every built floor, by what storeys.js says the
floor is for): a desk with its lamp and chair or a counter with its till, espresso machine, vise or ledger at each work
place, a table for two at a cafe's seats, cinema seats and museum benches facing in, chairs, beds with a nightstand, in the
slice's colours and room styles, as boxes as the slice draws them; nothing in the stairwell or on the way from the door to
the stair. Whoever is there stands behind the counter, sits at the desk or table or lies in the bed (`SeatBrixDayIndoors`).
`CITY013_shops_cafes_and_workplaces_are_furnished_at_their_places`. Each room is dressed as the slice dresses it (play.js layout() and
room(), also in BrixFurnish): a cafe's coffee bar with its machines and cake stand, a grocer's produce shelves, a works' racks
and hoist, an office's reception, a clinic's screens, a bank's counter and vault, a library's stacks, a museum's paintings,
a cinema's screen; wardrobes, bookcases, dressers, plants and sideboards along the walls with pictures over the low ones,
rugs, dining tables, kiosks and benches on the floor, lamps hanging from the ceiling
(`CITY013_rooms_are_dressed_as_the_slice_dresses_them`: 150 buildings, 251,751 boxes in all). The building's real windows
are not known to it (the slice's l.inWin), so a tall piece may stand in front of one; two slice slips (a works' rack through
its back wall, a museum's great painting off the side upstairs) are cut back to the walls. Unity builds are off in both modules (`bUseUnity = false`): files'
own short names clashed as soon as a new file changed which files were joined.

## Windows lit by night
As in the slice (play.js windows()), a share of each kind of building's windows is lit after dusk (half a home's, a third of a
shop's, every shopfront window; `LitShare` in DefaultGame.ini), each a warm pane just in front of its glass in one of the
lamplight's two colours, shown and glowing as the lamps light (`ABrixTownActor::SetNight`, from `ABrixRooms`). Lumen's
surface cache is some 20 % short of what the town's insides would fill (an on-screen warning in test shots): it only
lowers the detail of far surfaces' bounced light; a larger cache (`r.LumenScene.SurfaceCache.AtlasSize`) is not set, to
spare the laptop's video memory.

## Furnished homes (FURN-060 to FURN-063; ADR-0050)
The planned homes are furnished as the slice furnishes them (homes.js, houses.js): 469 designed houses (plan house_12x9)
and 250 row houses (lot.interior) from World/rules/home_plans.json, with their inner walls and doorways, tiled kitchens and
bathrooms, and each plan's pieces at the household's furnishing level (Bare, Basic, Homely or Full from its daily wages and
the day, FURN-063) with a bed for each resident; tall pieces in front of the building's real windows are left out. Where
each piece stands is `BrixWorld/BrixHomes` (`FURN060_every_planned_home_is_furnished_in_its_rooms`). The pieces are the
furniture kits' own meshes in the home's line: `Tools/blender/unreal_furniture_37.py` exports them (Blender, about 20 s),
`Tools/unreal/import_furniture_kit.py` imports the ones the plans use into `/Game/Furniture` (the .glb files are not kept in
git). `ABrixRooms` furnishes the homes within 30 m of the camera, and lights the rooms of the home the camera is in (three
lamps, no shadows: 3 to 5 fps). `-BrixStand=x,z,yaw` puts the player in a room for a shot.

## Designed houses (ADR-0032, ADR-0035)
The 469 designed houses (lot.house) are built as the slice builds them (houses.js outsideS): the 12 x 9 m house of
`World/rules/home_plans.json` (house_12x9), two 3.3 m storeys on a 0.3 m plinth, its nine windows and front door where
the plan has them, shutters open at every window, a string course between the storeys, a 24 degree roof with the
chimney and its two pots on the left gable; and the line's trim and its colourway from `World/rules/house_styles.json`:
plaster or ashlar walls, shutters, door, frames and roof in the colourway's colours, quoins on the Old Town line, window
boxes on Old Town and Curve, an arched door on Curve. Test `ART001_every_house_has_its_line_and_colourway`.

## Shopfronts, cafe fronts and balconies
On the street front of every shop of two storeys or more, the kit's shopfront(): one opening across the ground storey,
teal pilasters on stone plinths about every 2.2 m, lit display panes over a stall riser, a glazed door in the middle, a
fascia with the shop's name, a striped awning and crates of produce on the pavement. On every old-town cafe of two
storeys or more, cafe_front(): a striped awning over the arches, a blade sign at the corner, tables and chairs on the
pavement, and a French window on a balcony over the middle bay (balcony(); the kit puts balconies on cafes only). The
pieces are units the game stretches to each front (a pane, the door bay, a metre of fascia, an awning strip); the
numbers are in DefaultGame.ini.

## Stone, brick, paving and plaster (ADR-0017)
Every wall, pavement, street and open lot is drawn with the pattern World/rules/surfaces.json gives it: coursed rubble
in the old quarter, brick in the terraces (about half to the eaves), rusticated stone on civic buildings, ashlar
elsewhere; flagstones, slabs, herringbone and basket weave underfoot; setts in the streets; mottled two-tone plaster on
the upper storeys. BrixWorld picks the patterns as `world.surfaces()` does; the test `WLD038_surfaces_match_the_rules`
checks every lot and block against `Tests/values/surfaces_city_01.json` (run `python unreal_test_values.py` in
`Tools/worldgen` after changing the rules). M_Greybox draws them with the slice's own code, ported to
`Tools/unreal/greybox_surface.hlsl`; each box carries its pattern in its custom data.

## The clock and the sun (ADR-0024, WLD-043)
The game starts on day 16 (the slice's built day, a summer Wednesday) at the city's spawn time (07:20) and runs at the
calendar's pace (`real_seconds_per_minute`, 10 s a game minute). The sun is worked out from the clock every frame, never
stored: its place for the season at latitude 40 (World/rules/calendar.json, sky.json; a port of world.sun_place(),
checked by `WLD043_the_sun_follows_the_calendar`), its light from no lower than `sun_lift`, a dawn red as it sinks, the
calendar's hour of dawn and dusk; the sky's fill and the haze's glow go down with the day. `-BrixDay=` and
`-BrixTime=hh:mm` start elsewhere in the year or the day.

## Residents (CITY-021, CITY-022, CITY-024)
All 3,984 residents follow the built day's routines: `Game/Data/day.json`, written by `Tools/worldgen/build_play.py` in
the same run as the slice's city_data.js (the same day, day 16), so after changing World/ run build_play.py as before.
Where anyone is follows from the day and the clock each frame (a port of the slice's resident(), checked against citysim
City.at() by `CITY022_residents_are_where_their_routine_puts_them`): at their place, or walking their route at even speed
a little to one side of the pavement's line. Only the nearest, out of doors and within 90 m, have a body, up to 400 at once.

## People (ART-004, ANIM-001, ANIM-002, ANIM-004, ANIM-005; ADR-0047)
The residents are the slice's option B people: `BrixWorld/BrixBody` is a port of `Prototype/play/people.js`. Each is
dressed by their role (a job's own clothes where it has them) and walks by their role's movement signature, from the
same tables as the slice: `Game/Data/people.json`, written from people.js by `Tools/unreal/people_values.py` (run it after
changing people.js; it runs people.js itself in headless Edge). Bodies are built from code as the slice builds them, at
its distances: the nearest within 5 m at full detail with a blinking face, to 16 m a body with a face, to 35 m a simpler
body whose face is gone by 30 m, and further out a figure of a few blocks. `ART004_people_are_drawn_as_the_slice_draws_them`
checks the port against people.js: the same looks, and bodies (walking, standing and sitting) with the same triangles to
within 1 mm. Each body is a procedural mesh of shared vertices (a sixth of the slice's loose triangles, whose upload cost
6 ms in a crowd), rebuilt every one to three frames and moved every frame; where everyone is and the bodies due a rebuild
are worked out on the task threads, only the uploads on the game thread. The far figures are one mesh updated in place
(moving instanced boxes every frame cost 8 ms of the render thread). A test shot logs the people's time by part. Their
material is `/Game/People/M_People` (the vertex colour, matte), made by `Tools/unreal/make_people_material.py`.
Residents at a garden's bench seats and fountain rims sit there, facing the garden's middle or out of the fountain
(CITY-017, found as the slice finds them: `SeatBrixDay`, checked by `CITY017_residents_sit_on_garden_benches`); others
stand. The gesture layers of CHARACTER_MOTION §4 are drawn as people.js draws them (ANIM-004, checked by ART004): the arms and
chest (dodging, the weather, a shout at a smash, a repair crew bent over their work, hands on a climbing frame's top bar),
another walk (running, hurrying, a child climbing a ladder hand over hand), sitting astride a seesaw, a hood and an
umbrella; and the head and eyes (ANIM-005, as play.js does it): someone the player comes within 5.5 m of, on their level
and not behind them, turns to look, the eyes leading by up to their limit and the head part of the way, fully within
3.2 m (`ANIM005_people_look_at_whoever_comes_near`); the player's own head turns part of the way to where the camera
looks; a resident asleep in bed has their eyes shut. The player is the character they designed (below) at full
detail, walking by their signature and running by the slice's Run. At High on this laptop: 53 to 55 frames a second on a street with 32 residents near, in
the morning school rush of some 150 and in a garden with 46 (the same as with none: the GPU is the limit), the people
taking 1.3 to 3.6 ms of the game thread.

## The character designer (CHR-001, CHR-002, CHR-013; F-CHR-DESIGN 2)
A new game opens the designer, as the slice's life.js does: a Lamplight card down the left (ADR-0049) with the name, the
body (height, build, skin), the face (width, eye size and openness, brow height, nose, mouth width, eye colour), the hair
(style, colour), the clothes (a top, a bottom and shoes, each in a palette colour), the extras (scarf, cap, apron, bag,
moustache) and the walk, every choice and range from World/rules/designer.json; Surprise me picks at random, and Begin
(once the name is 1 to 16 letters) starts the game as that character. Beside it the character stands live in the street,
the camera close in front (drag to turn them round), and the clock waits. A character is those values and nothing else
(`BrixWorld/BrixDesigner`, kept as designer.json writes them, never a mesh): `BrixDesignLook` is people.js design(), so
the face values change only the shape of the shared face (CHR-007), and `BrixLookProblems` is world.look_problems().
`CHR001_the_designer_makes_the_slices_look` checks eight designs (the default and one per hair style, sliders at their
ends) against people.js design() and the bodies it draws standing, walking, further off and talking (to 1 mm), from
`Tests/values/designer_city_01.json` (written by `Tools/unreal/people_values.py`, which also asks Python for the checks);
`CHR013_designer_values_stay_in_range` checks 29 sets of values each wrong in a way, and the good ones. A test shot starts
as the default character without the designer, as the slice's ?auto=1; `-BrixDesigner` opens it for the shot (taken by its
own clock, as the game is paused), `"-BrixDesignSet=name:Ada;hair:afro;skin:7;height:1.8"` sets values first,
`-BrixDesignSpin=<deg>` turns the character, `-BrixDesignBegin` presses Begin after 3 s (the shot is then taken in play);
`-BrixNoDesigner` skips it when playing. When the slot played last holds a save, the start offers "Carry on as <name>" or a
new character (see Saving).

## Saving (QST-008)
As the slice's life.js keeps its save: the game saves itself in the slot being played every 15 s (`AutosaveSeconds`), at
once when a new character begins, and when the game is left; each slot is a JSON file, `Saved/BrixTown/save_<n>.json` (the
slot played last in `slot.txt`). A save holds the character (their designer values), where they stand and the time, the
economy as economy.py save() keeps it (the ledger's transactions, from which every balance is worked out again on loading,
the accounts, loans, things carried, trades, reputation, the stall, clothes and homeware), the quests as quests.py save()
(lines under way and finished, the journal, a job, the jobs taken), the wardrobe's outfits and the achievements earned
(`BrixWorld/BrixSave`). The start offers to carry on in the slot played last, or a new character; the menu's Save page
lists the slots (whose game, its day, when saved) with Save here, Load and New game here, which asks twice when it would
delete a save and then starts the game afresh. A save whose character the designer would refuse is not offered.
`QST008_a_save_brings_the_game_back` stops the quests' fixed walk halfway (with a loan, the stall and clothes from an
outlet), saves it to text, reads it into a new economy and new quests, and plays both on to the end and over three
midnights: they stay the same in all a save keeps; a new game holds only the starting cash. The world shows the built day
16 whatever the save's day is (the economy's days count on); the time of day comes back. A test shot never saves
(`-BrixNoSave` plays without saving too); `-BrixSaveAs=<slot>` saves the shot's game there 6 s in, `-BrixLoad=<slot>`
carries on from a slot at once (with `-BrixMenu=save` to see the page). The property market is in the save (estate.js save(): every unit's owner,
tenant, rent and residents by unit id, listings, tenancies, mortgages, the indices and history; a save from before it
opens the market on loading). Remodels are in the save (life.js remodels: each building's design as paid
for, drawn again on loading). Not yet in the save: the skateboards owned (CITY-100), room plans and furnishing plans (not
in this build's designer yet); and a server keeping it (PH2).

## The wardrobe (CHR-014 to CHR-016; F-CHR-WARDROBE 1, with the owned clothes the owner asked for)
U (or the menu) opens the Wardrobe page as the slice's life.js does: the page stands to one side, the town is not dimmed,
and the character stands in full beside it, the camera in front (`WardrobeCameraM`, `WardrobeSpin`). Outfits: what is
worn saved under a name, up to settings.json `wardrobe.max_outfits` (12), saving under a name already there changes that
one, Wear and Forget (`CHR015_the_wardrobe_holds_a_few_outfits`). Clothes you own: the clothes bought at the mall's
outlets (the economy's Clothes, ECO-014), each with Put on, which sets only the parts of the look it names in
World/rules/mall_goods.json (`CHR016_clothes_bought_set_only_their_parts`), or "on" when it is worn. What you wear: the
designer's own hair and clothes choices, changed at once and free; never the body, the face or the name
(`CHR014_an_outfit_is_only_clothes_and_hair_the_designer_offers`). The logic is `BrixWorld/BrixWardrobe`; the page is
drawn by `ABrixDesigner` (its helpers are the designer's). While the game is paused (the designer, the wardrobe) the
character ticks and moves the camera itself, as a paused world does not. To see it in a test shot: `-BrixMenu=wardrobe
"-BrixLife=outlet,tunic_terra;outlet,cap_blue" "-BrixOutfit=Everyday,Market day"` (`outlet,<good>` buys it at the first
of a mall's outlets that sells it; `-BrixOutfit=` saves what is worn under those names). The outfits are kept in the save.

## Talking to residents (CITY-007, CHR-007, CHR-009, SND-018)
As in the slice (conversations.js, voices.js): next to a resident out of doors, E offers "Talk to <name>" and opens the
talk; they stop where they are and turn to the player, the camera comes in close over the shoulder, and the box at the
foot of the screen shows their name and job and what they say, with the blanks filled ({name}, {place}, {doing},
{greeting}, {weekday}, {season}). 1 to 9 (or a click) answers, Esc leaves. The conversations are World/rules/dialogue.json
(`CITY007_every_conversation_is_sound`). Each line is said aloud where it has been voiced: the slice's Kokoro pieces in
`Prototype/play/audio/voices` (indexed in `Game/Data/talk.json`), decoded at run time with the engine's Ogg Vorbis
decoder and joined into one sound (`SND018_lines_are_voiced`; few lines are voiced yet, as the dialogue is not final).
The face follows the line: mouth shapes from its text stretched to the voice's length, brows and nods on stressed words
and questions, the mood's expression (World/rules/speech.json; `CHR009_lines_are_mouthed_as_the_slice_mouths_them` and
the talking faces in `ART004_...` check the port against the slice's own code). After a talk the resident runs late by
the time it took until they next stand still where they would be anyway. The quests, jobs and counters that the slice's life.js adds to talks are below. The player's name from the designer fills {player}
(`CHR001_the_players_name_is_said_in_talks`), and while they answer their own face says it, mouth with the voice (or the
text's own time when it is not voiced), the camera coming round past the resident to look at them (`AnswerCameraYaw`) and
going back over the shoulder when the resident speaks. A resident's line that has no voice yet is murmured as the slice
murmurs it (sound.js line()): wordless syllables in time with its mouth, in the resident's own pitch, from where they stand,
hushed when the next line starts or the talk closes (`SND018_unvoiced_lines_are_murmured`, against sound.js traced by
`Tools/unreal/sound_values.py`; the player's unvoiced answers are silent, as in the slice).
To see one without anyone at the keys: `-BrixTalk=<resident index>` (and `-BrixAnswers=1,2`, a second and a half apart or
`-BrixAnswerGap=<s>`) with `-BrixShot=`.

## Vehicles (CITY-040, CITY-091; ADR-0015)
Every car, van and bus of the day is where `Tools/citysim/traffic.py` puts it at the clock's minute: `Game/Data/cars.json`
(the same trips as the slice's, written by `build_play.py`) read by `BrixWorld/BrixCars`, a port of the slice's
`Cars.at()` with traffic.py's bus layovers; `CITY040_vehicles_are_where_the_traffic_puts_them` checks 66 vehicles at
eight minutes against traffic.py (parked to 0.1 m; on the move to 4 m, the sideways ease into a lay-by). Cars and vans
are the Soft box models of `Tools/blender/car_export_11.py` (copied to `Game/Data/car_models.json` by `build_play.py`) in
the slice's paints, roof loads and spares; buses the slice's long soft box in their line's colour. Within 45 m the near
model, to 160 m the light one; each is a procedural mesh built once and moved. Within 70 m whoever rides in them sits
there as in the slice (cars.js occupants(), seat()): the resident whose car it is at the wheel and neighbours sharing the
ride beside and behind (CITY-094), a bus's driver and its riders on the benches, someone from out of town in a van or a
car passing through; as low as the head needs under the roof, feet along the floor, hands on the wheel. The player can drive
any parked car or van (a stand-in for CITY-061, as the slice's cars.js): E beside it gets in (it steps out of its bay
onto the road first), W/S speed and brake, A/D steer, Space the handbrake, up to the top speed of World/rules/traffic.json;
a street thing or a tree in the way breaks (CITY-072); a wall or another car stops it and it rolls back a little; the camera swings round behind; E gets out by
the driver's door, and the car stays where it was left for the rest of the visit. E offers whichever is nearer, a car to
drive or someone to talk to. A bus can be ridden (CITY-092): E at a bus
standing at a stop gets on, a seat near the front, carried along its line with its driver and riders, the camera behind;
E gets off when it next stands at a stop (and everyone gets off when it leaves service). The tram is ridden the same way
(below). To see a drive in a test shot: `-BrixDrive=<vehicle> "-BrixHold=steer,throttle"`. 51.5 frames a second at High on a street
with 32 vehicles drawn.

## Getting out of the way, and giving way (CITY-070, CITY-071, CITY-049)
Residents on foot get out of the way of a car the player drives, as in the slice (dodge.js, `BrixWorld/BrixDodge`, numbers in
World/rules/dodge.json): one the car is heading at, due to reach them within its reaction time, flinches, scrambles aside
with quick running steps and a small hop to the side with room (not into a wall), turns on the driver with a fist or
both arms up and a shout shown over them (the nearest of a crowd), then walks back to where their day has them. Only an
offset and a pose laid over their day, near the player, forgotten after a few seconds
(`CITY070_residents_dodge_and_scold_as_the_slice_does` checks the port against dodge.js frame by frame).
Resident cars give way to the player on foot or to the car they drive (CITY-049; the slice did not, so the rule is proved
first in `Tools/citysim/giveway.py`, numbers in World/rules/traffic.json `give_way`): a car with them in its path slows
and stands 2.5 m short, and those behind queue behind it. It runs that many minutes behind its timeline, shown where its
timeline had it then; it never catches up on the move (so never faster than its timeline and the limits), only at its
next stop, which it leaves as soon as its timeline does (a stop shorter than the delay is skipped); one more than 10
minutes behind with no player within 160 m is put back on its timeline. `CITY049_resident_cars_give_way_to_the_player`
stands someone in the road in front of cars of the day and checks they stop short, wait, and are back on their timeline
after their next stop. At High, 36 to 49 frames a second driving into a crowd in the morning rush, 53 with two cars
queued behind the player's (noisy: other sessions were running Unreal on the same laptop).

## Crash damage (CITY-072 to CITY-076)
As in the slice (smash.js; `BrixWorld/BrixCrash` and `ABrixCrash`, numbers in World/rules/crash.json): the player's car
breaks the street things and trees it hits, at any speed. Lamps, trees, post boxes, telephone boxes, bollards and boards
topple the way they were hit and lie there; benches, bins, planters, bike racks, flowerbeds and market stalls burst into
pieces in their own colours that fly, bounce and settle as rubble; a building corner it hits loses chips of stone and
shows its rough core. The car loses its share of speed. A broken thing's instances are folded away (each prop and tree
keeps the indices of its instances: `ABrixTownActor::Breakables`), with its solids and, for a lamp, its light. What
broke, where and when is kept in `Game/Saved/BrixDamage.json` (CITY-074) and is there on the next visit; when each
thing is mended is worked out from that list alone by repairs.py's rule (CITY-075): a crew from the nearest of the
seven depots comes in working hours, works crouched at it, and then it is whole again; within 12 m of the player a tag
says when the crew comes or will be done. Residents within 18 m of a smash turn to it, throw up their arms and shout
(CITY-076). `-BrixWhole` starts with everything mended; `-BrixAim=x,z` turns a car taken with `-BrixDrive=` towards a
point. Tests: `CITY072_street_things_break_as_the_rules_say`, `CITY074_damage_is_kept`,
`CITY075_repair_crews_mend_as_repairs_py_does` (against repairs.py, `Tests/values/repairs_city_01.json`),
`CITY076_residents_shout_at_a_smash`. Not yet: café tables and crates, garden plants, fences, walls, railings and
hedges (not drawn in Unreal yet), a broken bench still being sat on, and a server keeping the list for every player. At
High, 38 to 54 frames a second while breaking a post box, a planter and a bin and with two crews at work (noisy: other
sessions were running Unreal).

## Weather (WLD-037, CITY-077 to CITY-080; ADR-0016, ADR-0020)
The day's weather is citysim's for each six-hour slot (`Game/Data/day.json` "weather", written by build_play.py); what it
does is worked out from the clock and World/rules/sky.json alone (`BrixWorld/BrixWeather`, a port of
`world.weather_now()`, checked against it by `WLD037_the_weather_is_worked_out_as_world_py_does`). Cloud, rain and fog
let less sun through, part of what they stop comes down as soft grey light from the sky, and the air thickens (which
greys the sky); clear weather leaves the approved light as it is. Rain falls in the world (`ABrixWeather`): drops in
rings round the camera, 1.6 to 11 m out, falling straight down whichever way you look, none where a building's roof is
overhead and none while the camera is indoors; each a thin grey streak (9 mm, about half a metre: DropWidth, DropLength and DropAlpha in DefaultGame.ini scale
sky.json's rain), clear at its top and full at its foot, of its own length and brightness, fading near the camera; those within 7 m throw up a small splash where they land. One procedural
mesh of translucent drops (`/Game/Weather/M_Rain`, made by `Tools/unreal/make_rain_material.py`). The paving gets wet:
it darkens, takes a faint sheen and holds puddles here and there, rings spread where the drops land (they tilt the
surface, so the puddles' reflections break up), and walls go a little darker; it dries over `dry_minutes` after the
rain (M_Greybox's WetDark, WetSheen, Puddles and Ripples, from `Tools/unreal/greybox_wet.py` and
`greybox_ripples.hlsl`). Residents wear what citysim gives them today (day.json "wear"): a coat,
and out in the rain an umbrella up, else the hood up, else a hand over the head while walking, with a hurried walk or a
run for cover; nothing but the coat indoors, sheltering or under a roof (`CITY080_residents_are_dressed_for_the_rain`).
The look of coats, hoods and umbrellas is still to be approved (ADR-0020). `-BrixWeather=<clear|cloud|rain|fog|snow>`
tries one kind for the whole day (and the clothes residents would wear on a day of it); the built day 16 has cloud
before 06:00 and is clear after. At High: about 40 frames a second in the 08:10 school rush in rain, against 15 to 54
in clear weather on the same street (noisy: other sessions were running Unreal on the same laptop). Not yet: no key to
try the weather, rain seen through windows from indoors, the rain's sound, wet clothes.

## Snow (WLD-069, WLD-070, CITY-107; ADR-0051)
At the owner's word (2026-11-25): it snows in Winter only, by its own chance per six-hour slot (`snow_chance` in
World/rules/calendar.json), in spells, the same for every player (citysim `weather()`). A snowy slot has its own light
(sky.json `weather.snow`: little sun, thick air) and under a covered sky the haze turns from the sun's colour to a cool grey
(`OvercastHaze`, for cloud and rain too). Flakes fall slowly round the camera, swaying, not under a roof nor while the
camera is indoors (`ABrixWeather`, `MaxFlakes`, sky.json `snow`); snow settles on what faces up, streets, pavements, roofs
and sills, in patches that join into a cool white cover over `settle_minutes`, and melts over `melt_minutes` after
(M_Greybox `Snow`, from `greybox_wet.py`). How hard it snows and how much lies come from the clock and the day's weather
alone: `BrixSnowNow`, a port of `world.snow_now()`, checked by `WLD070_snow_falls_and_lies_as_world_py_says`. Residents
(CITY-107): on a day of snow nearly everyone wears a coat and most coats have a hood; out in it the hood is up, else they
go bare-headed, never an umbrella, and walk at the careful pace (citysim's plan makes the walks slower; they take cover as
from rain); `CITY107_residents_are_dressed_for_snow`. `-BrixWeather=snow` tries it on the built summer day (with what
residents would wear on a day of snow). At High: 37 to 39 frames a second in the 08:10 rush in snow (noisy: other
sessions were running Unreal). Not yet: footprints and tracks, snow on trees, cars and people, the sound of snow.

## Clouds (WLD-035, WLD-036; ADR-0014)
Soft heaps on one flat layer overhead, bright on the side towards the sun and shaded underneath, fading into the haze
towards the horizon, taking the colour of the hour (warm at dusk, dark at night). How much sky they cover follows the
day's weather slots, blended at their edges; where the wind has carried them and how their shapes change come from the
clock alone: `BrixCloudsNow`, a port of `world.clouds_now()`, checked by `WLD036_the_clouds_are_where_world_py_puts_them`.
They are drawn on the inside of a dome round the camera by `/Game/Weather/M_Clouds` (`Tools/unreal/make_clouds_material.py`,
`clouds.hlsl`); 46 to 50 frames a second at High under a clear-day sky and at dusk.

## The tram (CITY-041)
Trams run along Tram Street to the timetable of World/rules/calendar.json, stopping at its three stops: a port of the
slice's tramAt() (`FBrixTram`, checked against the slice's own function at 3,000 moments of a day by
`CITY041_trams_are_where_the_timetable_puts_them`). Each is the slice's terracotta car with open windows, on rails along
the whole street, and carries the residents whose day takes them out from a stop or back to one, on the run that is
there when they leave or arrive (cars.js tramRiders()), up to twelve on its benches. The player can ride it (CITY-041;
the slice cannot): E beside a tram standing at a stop ("Ride the tram", within 1.5 m of its side or end) gets on, and the
player sits on its front bench on the south side among the riders (who then fill the other eleven seats), carried east
along Tram Street, the camera behind; E ("Get off the tram") gets off where it next stands, onto that stop's platform by
the front door. Not at the last stop, from which it goes on out of town; anyone still on when it leaves there is put off
at it. The logic is `FBrixTram` (`Standing`, `Find`, `Seat`, `Alight`, `Rideable`): `CITY041_players_ride_the_tram`
boards every tram that stands at a stop through the day and rides it to the next. To see it in a test shot: `-BrixTram`
(the player is put on the platform by the first tram standing at a stop and gets on; `-BrixTramOff` gets off at the next
stop), for instance with `-BrixTime=07:55`.

## Street furniture (WLD-028)
Each block's props are drawn as the slice draws them (play.js prop(), dressing.js), boxes in the palette's colours:
lamps, benches, bins, bollards, planters, postboxes, bike racks, phone boxes, wall lanterns, flowerbeds, fountains,
market stalls, tram shelters, water towers and wells; garden planting, the open ground's own things, the street dressing,
the arches and the playgrounds are below. By night (`ABrixLamps`) every lamp's and wall lantern's glass glows
(`/Game/Greybox/M_Glow`, made by `Tools/unreal/make_glow_material.py`) and the twelve nearest to the camera give real
light, as the slice's twelve; 68 frames a second at High at 22:30. The night itself (its exposure, the orange haze
after dusk) is not yet the slice's dark blue night.

## Gardens, lawns and the open ground (CITY-017, WLD-040, WLD-056, ART-001)
The block files' garden planting is drawn as the slice draws it (play.js plant(), heads(), plantClump()): clipped hedges in
sections a little uneven in height and tone with a paler top, shrubs as a faceted crown with two smaller ones leaning off it
(bare twigs in winter) dotted with flower heads or dry ones, borders of dark earth planted with their two kinds in drifts,
and drifts of flowers coming up in the grass, each kind as today's season has it (`Game/Data/streets.json`, written by
`build_play.py`: world.plants_today(), the flowers and the lawn rules of World/rules/vegetation.json). Every garden has its
lawn (play.js lawn()): the grass, a band of longer, darker grass along its edges and tufts the mower missed, none on a path
or under a thing. The open ground beyond Old Town (play.js greenGround(), wildProp()): a wood's leaf litter in patches, the
grass of the commons, orchards, fields, plots and meadows, the gravel tracks, and its own things: post-and-rail fences,
"For sale" boards on the plots with their pegs, fallen logs, rocks, picnic tables, hay bales, the fields' crops (standing
wheat and barley, furrows, rows of cabbages, rough grass) and ponds in their ring of reeds. `BrixWorld/BrixStreets` is the
port: it gives the same drawing calls with the same numbers as the slice's functions (in doubles, its hash() fed the same
strings, a JavaScript number written as JavaScript writes it), and the town lays them as its own shapes: boxes as instances
(no collision; what is in the way comes as the slice's solids, unseen blockers), a crown as an instance of one of 12 meshes
of play.js dome() turned by its seed, single faces (tufts, lawns, tracks, ponds) in one mesh a block made at load, all in
`/Game/Greybox/M_Facets` (vertex colour times the instance's colour, matte, two-sided; `Tools/unreal/make_facets_material.py`).
`Tools/unreal/streets_values.py` runs the slice's own functions in headless Edge over every plant, wild thing, lawn and
block of open ground (`Tests/values/streets_city_01.json`), and `CITY017_every_plant_is_drawn_as_the_slice_draws_it` and
`WLD056_the_open_ground_is_drawn_as_the_slice_draws_it` check the port against them (2,735 plants, 33 lawns, 17 blocks,
about 600 things); run the script again after changing play.js, the blocks or the vegetation rules (after build_play.py).
Bushes, hedges and fences break as crash.json says (CITY-072). The plants keep the built day's look whatever day the game
starts on, as in the slice. Not yet: the lawn's own pattern (the slice's material 24: patches, mowing stripes, clover,
daisies), so a lawn is one even green (the trees: below). The new shapes are kept out
of Lumen's surface cache (a card an instance oversubscribed it over the fields). `-BrixNoPlants` builds the town without
them, to compare: 542,212 instances in 45 groups against 462,813 in 33; at High 37.7 fps in a park garden (40.2 without),
37.8 over the fields (20.6 without: noisy, other sessions were running Unreal on the same laptop).

## The street dressing (WLD-028, WLD-031, WLD-034)
Every piece of the block files' street dressing is drawn as dressing.js prop() draws it (the playgrounds below): the
bandstand on its eight-sided stage under its pointed roof, notice boards with papers pinned up, sundials, wells with their
little roofs, pergolas under a vine, bronze statues on their pedestals, the green news kiosk, bunting from a pole to the
fountain on market days (World/rules/dressing.json; today's from world.dressing_today() in `Game/Data/streets.json`),
planters with today's flowers, bins, bike racks with now and then a bicycle, post and telephone boxes, bollards, the tram
stops' timetables, and today's adverts painted on blank walls and timber hoardings pasted with posters (no window where a
board covers the wall, as dressing.js covers() keeps them clear). And play.js prop()'s stone arches across the alleys, each
with its ring of arch stones, a room bridging the alley over it where it meets the street, and crates by some; the market
stalls' crates. The port is `BrixStreets` (BrixDressShapes, BrixMoreDressShapes, BrixArchShapes, BrixCratesShapes),
checked by `WLD028_the_street_dressing_is_drawn_as_the_slice_draws_it` against dressing.js and play.js themselves (1,432
pieces and arches; `Tools/unreal/streets_values.py`); a name's width on a board is estimated from its length, as the slice
measures it on a canvas. The player's car breaks them as crash.json says (CITY-072): notice boards and timetables topple
(as "board"), and the café tables and the shops' crates on the pavement (the kit's pieces), the crates by the arches and
the planters, bins, racks, post and telephone boxes and bollards break as before; the log says how many of each kind there
are. Statues, the bandstand, the kiosk, wells, sundials, pergolas and the boards on walls do not break. Residents reading the notices or
buying a paper turn to the nearest board or kiosk within 6 m, as dressing.js apply() turns them (checked in WLD032). Not yet: the letters of a broken
timetable staying up, the lettering's own font. At High 29.8 to 44.1 fps in Old Town's streets and the market at 11:00
(noisy: other sessions were running Unreal); Lumen says its surface cache is oversubscribed by a quarter in Old Town (seen
here; not checked against main).

## Trees (WLD-056)
Every tree is drawn as play.js tree() draws it, as it is today (build_play.py tree_rows(), the `trees` of
`Game/Data/streets.json`): a pale trunk and a faceted crown in the leaves' own colour: round, a pine's tiers of cones, or a
birch's slim crown on a white trunk with a dark band; bare in winter, pink in blossom in spring, a fallen one as a log; a ring
of mulch round a trunk standing in a garden's lawn or the open ground's grass. In the woods each tree has fewer facets and no
ring, and no car gets in among them; anywhere else a tree is a thing of its own the player's car topples (crash.json
"tree"), crown and all: its faces are in its block square's mesh, which is made again without them when it falls. The port is
`BrixTreeShapes` in `BrixStreets`, checked by `WLD056_every_tree_is_drawn_as_the_slice_draws_it` against play.js tree()
itself (6,466 trees on day 16). Each block square's crowns are one mesh, fading out beyond `TreeCullDistance` (900 m). Not
yet: a terrace's back-garden lawn (the town draws its lots' own yards there), so its trees have no ring.

## Road markings, bus stops and traffic lights (CITY-043, CITY-045, CITY-091)
The road network is drawn as roads.js draws it, from the `roads` of `Game/Data/streets.json` (build_play.py roads(), from
World/city_01/roads.json and traffic.json): zebra crossings, the parking lay-bys' setts with their white ends and back line,
the bus stops (a lay-by with its yellow box in a town street, a shelter in the colour of the first line that stops there,
a bench, and a pole with a disc for each line), and at each of the 17 signalled junctions a pole on the right-hand kerb
before the stop line with three dark lamps facing the oncoming cars. Each head's lit lamp (red, amber or green, glowing in
its own colour times `SignalGlow`) is the phase traffic.py's signal() gives for the day and the minute (roads.js phase()),
so a car stops on the red it was worked out to stop on; the game mode sets them each frame from the clock. The port is
`BrixRoadCells`, `BrixRoadShapes`, `BrixSignalPhase` and `BrixSignalLamp`, checked by
`CITY043_the_road_markings_and_signals_are_drawn_as_the_slice_draws_them` against roads.js itself (every block square, each
head's lamp at 10:00, and every signal's phase for both streets through a whole day).

## Playgrounds and children playing (WLD-032)
The four parks' playgrounds are drawn as dressing.js draws them: the soft red ground with a pale edge, the swings' A-frames
and beam, the slide (ladder, railed top, shiny chute), the seesaws' pivots, the sandpit with its kerb, bucket and spade,
the climbing frame with its deck. Children whose day has them at a play place (the students who live nearest, after
school and at weekends) are where dressing.js apply() puts them at that moment: on a swing seat swinging, astride a
seesaw's plank (a leg down each side, people.js mod.straddle) rocking, up the slide's ladder with the climbing walk and
their hands on the rungs, sitting on its top, down the chute and walking round the back to the ladder again, sitting in
the sandpit facing its middle, up the climbing frame's side, looking over the top, down and resting; anyone walking is
kept out of the slides and frames. The swing seats and seesaw planks within 70 m swing and rock while someone is on them
and hang still (a seat stirring in the breeze, a plank down at one end) when not: one small mesh made again each frame
(no instances moved). Motion from World/rules/dressing.json "motion" (in `Game/Data/streets.json`). `BrixWorld/BrixPlay`
is the port (pieces, places, apply(), keepOut(), draw()), checked against dressing.js itself by
`WLD032_children_play_as_the_slice_puts_them` (the 28 pieces' shapes, 352 children at four moments, the moving parts after
each, walkers kept out; `Tools/unreal/streets_values.py`).

The player can have a go too (WLD-041, F-OBJ-POINTS 1): at a swing seat, a seesaw end, the foot of a slide's ladder, by a
climbing frame or in a sandpit (48 places, one person each but the slide and the frame), E offers it ("Have a go on the
swing", ...) when it is nearer than anyone to talk to or a car, and no child is on it. On a swing they sit still at first
and swing higher over `player_swing_build_s` with the seat going with them; on a seesaw they rock it; on a slide they climb
the ladder, sit at the top, slide down and get off at the bottom; on the frame they climb up and down its side; in the
sandpit they sit facing its middle. E, Space or moving gets off, beside the piece (dressing.js playerAt(), playerOff(),
takenBy(); play.js interact(), stepPlayer()). Checked in the same test: every place, the player's pose at eight moments of a
go there, and where they step off. `-BrixPlay=<n>` has a go at the n-th place a third into a test shot's wait.

## Money and the bank (ECO-001 to ECO-003, ECO-009, ECO-012, BANK-001, BANK-002, BANK-007, BANK-008; ADR-0012)
Money is the slice's (economy.js, a port of `Tools/citysim/economy.py`): `BrixWorld/BrixEconomy` holds one ledger of
balanced transactions in whole chips (int64), money made and taken only by the `sys:` accounts, the bank's vault and every
shop's till as ordinary accounts; the bank's current and savings accounts, moving money between them and the purse, loans
paid in daily instalments, late fees, default and repossession; and the shops' stock and prices, a mall's outlets and the
market stall, worked out from the residents' visits and the player's own trades, never ticked. The data is `Game/Data/life.json`, written by `Tools/worldgen/build_play.py` in the same run as the slice's
city_data.js (economy.json, mall_goods.json and quests.json, the day's shop visits, who works where), with two fixed runs
and what Python gives for them: `ECO002_the_ledger_matches_the_python` runs economy.check_script() and its outlet ops,
`BANK008_the_bank_settles_as_the_python_does` a loan that defaults; both must give Python's ledger, balances, things
carried, loans, prices and outlet stock to the chip. A new game starts when play starts (the character designer opens over
it): the starting cash is paid in, and the economy's days count from then; each frame it is brought up to
the clock, so interest, instalments and fees are settled for each midnight passed (BANK-008). On screen, as in the slice:
the purse at the top left (cash, and the bank's two accounts together once one is open) with a "+" or "-" beside it when
it changes; at the bank's door (`CounterReach`) or inside it, B opens its counter in the slice's dark panel: open
accounts, move money (a payment that would take an account below zero is refused, never partly made), borrow and repay,
and the statement, newest first; I opens the money page (purse, loan, statement). Esc closes either. The game menu's Money
page (the accounts, a loan owed, the statement) is drawn in Lamplight, the look that took the place of the slice's painted
signs in this build (ADR-0049; `-BrixMenu=wallet` to see it). The money is kept in the save (below). To see the bank in a test shot:
`-BrixCounter=bank "-BrixLife=open,current;open,savings;move,p:current,p:savings,1000;loan,5000,3"` (ops as
economy.check_script() writes them, arguments by commas).

## Sound (SND-001 to SND-007, SND-009 to SND-017, SND-019 to SND-021)
The slice makes its sounds live with Web Audio (`Prototype/play/sound.js`, from `World/rules/sound.json`); the Unreal
build plays recordings of those same sounds. `Tools/unreal/sound_values.py` runs sound.js itself in headless Edge (over
its DevTools socket) and renders its own sound-making functions into an OfflineAudioContext: `Game/Data/sounds/*.wav`
(mono, 16-bit, 22,050 Hz; 229 sounds, 2.9 MB, in Git LFS; the music and the rewards as Ogg Vorbis, made by the engine's own encoder through the `BrixEncodeOgg` commandlet, so build the editor before running the script) and `index.json` (each one's length, whether it loops, its
level). Loops are cross-faded end into start. Run the script again after changing sound.js or sound.json. At run time
`ABrixSound` reads the WAVs (no editor import) and plays each through a procedural sound wave, as the talk's voices are
played; a loop refills itself when the engine asks for more. What is heard, where and how loud is BrixWorld's port of
sound.js (`BrixSound`): `BrixHear` is its place() (full within 2.5 m, falling off, gone at the sound's reach), the engine
only pans the sound from where it is. The same script traces sound.js's own logic in scripted moments (its audio swapped
for recorders) into `Tests/values/sound_city_01.json`, and the SND tests check the port against it.
So far: the player's steps in time with their walk (louder running) and a thud on a hard landing; the steps of the
residents within 9 m, at most six a frame, on stone, on grass in a garden (wood indoors, when people are seen indoors),
louder running, lighter and higher for children; the sound of a resident sitting down or getting up within 10 m. The
engines of the six nearest cars on a trip within 70 m, each from where it is, its note rising with its speed (vans and
buses lower) and louder moving; the car the player drives, its engine starting, its note with speed and the throttle,
a skid braking hard above 5 m/s and a bump when it is stopped short. sound.js pitches one voice; here two recordings of
it, at 36 and 76 Hz, share the note and are each pitched to it, so the timbre stays the slice's across the range. The two
nearest trams hum, whine higher and rumble as they go faster, and ring their bell as they stop and as they leave. A
continuous sound is a loop that plays only while it can be heard (it is stopped, and takes no voice, when it cannot).
The town's air (SND-001): the street quiet and dark at night and louder and brighter by day (the day's and the night's
recordings crossfaded over 80 minutes either side of 40 minutes before sunrise and after sunset, the calendar's for the
season), crickets from 21:00 to 05:00, the nearest fountain within 22 m, birds by day somewhere 6 to 28 m round the
player, 18 a minute and 40 near a garden, none while driving. The slice has no sound for the weather, so there is none
here either. The interface and interactions (SND-009): a tick when E offers something new, a chime as a talk opens, a
tick for each answer chosen, a falling chime as it closes, the jump, and getting into a car (its door, then the
starter) and out of it (the door). N turns all sound off and on, the talk's voices too (SND-013): it flips the game
menu's own Sound switch, so the menu shows it and keeps it between visits with its other settings, and says "Sound on" or
"Sound off" as the slice does. The menu's levels scale the mix as the slice's do (UI-009): All sound everything, and
Music, Street and weather, Voices, and Actions and the interface each their part. A test shot logs how many of each sound were played and how long each loop was heard (`sounds played in
all: ...`, `loops heard in all: ...`); `-BrixSoundToggle` turns the sound off and on again during it.
Indoors (SND-007): while the player is in a building (the game mode's RoomAt) what is outside (the street, crickets,
fountains, engines, the trams, birds) comes through a 650 Hz low-pass at 0.4 of its level, the room has its tone and steps
are on wood. Money coming in (SND-019 to SND-021): the quest and bank widgets' moments (`ABrixLife::OnRewardSound`)
play sound.js reward()'s recordings, each coin taken a note higher (`SND019_coins_ring_as_the_slice_rings_them`).
Music (SND-010 to SND-012): eight bars of each of the score's four moods and each style (café, shop, busker, the radio's
swing, pop and calm), recorded at the music bus's level, each a loop eased to its level as the slice's bands fade: the
score by the time of day, under a talk and under nearer music; the nearest open café or shop and the market's busker on
market days, from its door within its reach, muffled from the street and clear inside; the car radio's music stations,
R while driving (`-BrixRadio=<k>` in a test shot), its name shown a moment under the picture. The music is eight bars of
the slice's endless band, so it repeats. The slice has no sound for the weather, so there is none here either.
Podcasts (SND-014): the radio's two talk shows come after its music stations. Where a show is follows the world clock as
the slice's does (`BrixPodcastAt`, the same part and caption as sound.js podcast() at 420 moments in
`SND014_the_radio_has_podcasts`): its intro chime, then each line said by its host in wordless syllables in their own
pitch, through the radio's small speaker, over the show's bed, with the show, episode and line as a caption.
What people do and say (SND-006, SND-015, SND-016; `FBrixPeopleTalk`, the rest of sound.js people()): the nearest eight
busy residents now and then make the sound of what they are doing (cutlery and cups, the espresso machine and till,
bags, coins, pages, typing, hammering, shears, deliveries, the stamp at the bank, snoring for some, shuffling while
waiting, the swings, slide, seesaw, sandpit and climbing frame: 24 recordings, three of each); sellers and children on
the slide call out; people standing or sitting together at leisure or work (not in quiet acts such as reading) take turns
to talk, a phrase that falls to an end or rises to a question, with now and then a laugh (children more often), the five
nearest groups heard; where six or more are gathered a crowd hums and syllables rise from it; a passer-by sometimes says
hello as the player walks by; a resident who dodges the player's car shouts it (CITY-071), voiced from their shout's
mouth shapes. Each syllable is a recording of the slice's voice for its mouth shape at the nearest of four pitches (120
to 340 Hz) and two lengths, pitched the rest of the way, from where the speaker stands; each resident's pitch is the
slice's (`BrixVoiceHz`, children higher, elders lower). The horn: F while driving (`-BrixHorn` in a test shot). A test shot
logs the syllables said.

## Shops, cafés and the market stall (ECO-005, ECO-010, ECO-011, PROP-008, UI-020, UI-021; ADR-0044)
At the door of a shop or café (or inside it) B opens its counter, as in the slice: what it sells at today's price (its
stock after the residents' visits since the 06:00 restock and the player's own trades, the price rising as the stock
falls, lower or higher with the player's standing with whoever works there), and on a second tab what it buys from the
player at a share of its price, paid from its till. The goods are shown as objects in a grid with the chosen one large
beside it (layout B of look test 33, the owner's choice): each cell draws the thing with its price and a dot in the colour
of where it goes; the showcase turns it slowly (drag to turn it by hand) with its name, where it goes, what is left and
its price, and the one button. Sold out is greyed under a band; a thing dearer than cash and the current account together
has its price in red with how much is short. A click chooses, a second click soon after buys; the arrow keys choose and
Enter buys. What each thing shows is world.buy_menu_shows (`BrixWorld/BrixBuyMenu`, checked for all 236 things by
`UI021_every_thing_shows_what_python_says`); the stand-ins (a parcel in the seller's band, a paper bag, a cup, a case, folded
cloth in the clothing's colour) are the boxes of World/rules/buy_menu.json, drawn by `SBrixThing` with Slate's own
triangles, lit as the slice's shader lights them. At the market square B opens the player's stall: rent it for the day,
set out up to six things at the player's own prices, take them back; residents coming to the market buy as the rules say,
paid through the ledger, and what is left comes home at midnight. Eastgate Mall's outlets each have their own counter (see
Eastgate Mall). Each cell's dot and the chip beside "Goes to" carry the mark of where the thing goes (a house, a top, a bag,
a cup, a chair), as in the slice: build_play.py reads the marks from buymenu.js ICON into life.json, `BrixIconCoverage`
paints each once from its paths (filled by the nonzero rule, the handles stroked) and the dot or chip draws it in its board
colour's lettering (`UI020_dots_and_chips_show_the_slices_marks`). Not yet: the narrow layout D (for the building designer),
a good's own Blender model in place of its stand-in, and a resident behind each counter. To see one in a test shot:
`-BrixCounter=grocer "-BrixLife=buy,grocer,bread"` (a shop's kind or lot id), or `-BrixCounter=market
"-BrixLife=buy,grocer,jam;rent;setout,jam,250"`.

## Quests, jobs and rewards (QST-001 to QST-007, QST-009, SND-019 to SND-021; ADR-0012, ADR-0043)
The quest lines and daily jobs are the slice's (economy.js Quests, a port of `Tools/citysim/quests.py`): `BrixWorld/BrixQuests`
reads them from World/rules/quests.json (in `Game/Data/life.json`), with stages whose goal is a talk, a place, a thing
carried, the bank, money, the stall; endings that pay through the ledger from `sys:rewards` and change reputation; jobs
(a courier round, shopping for an elder, a shift) worked out from the resident and the day, never stored, one at a time,
against the clock. `QST001_quests_run_as_the_python_does` walks the first two starter lines and four jobs (one let run
out) through both and compares the notes, endings, reputation, ledger and three days of job offers (1,310) with Python's
(build_play.py quest_script()). A new game starts the first starter line. In a talk (`ABrixTalk` asks `ABrixLife`, as
conversations.js asks life.js): the resident a quest stage is waiting on has that stage's talk instead of their own; one
who gives a quest line offers it; one whose role has work offers the day's job ("Any work going?"); an answer's needs and
acts are the quest's ({player} is the name the player chose in the designer); an answer that opens a shop opens it
when the talk ends. Goals are checked four times a second where the player is (in a lot, or at its door). J opens the quest
log: under way with where the person to see is now (from their routine), on offer, finished, people you know. The lines
under way show at the right of the screen, a job with its time left. Money coming in is shown as the owner chose
(`BrixWorld/BrixRewards`, world.reward_events(), checked by `SND019_money_coming_in_is_shown_as_python_says`): a reward's
coins spring out round the player (over an arc on the far side from the camera, `CoinArcDeg`, so none loom up close), hover, are drawn in and fly to the purse, which counts up as they land; each coin's face is painted once into a texture as the slice paints it (mottled brass, rim, beads, a stamped B), and its lantern glow, the pool of light under it, its trail and its glints are added to the picture as light (`CoinGlow`); interest
comes with a slip under the purse and its coins fly up into the bank balance. Achievements (QST-009) are earned from
facts worked out from the ledger and the quests (`QST009_achievements_are_earned_as_python_says`), each once, its painted
banner dropping in at the top, one at a time and not over a panel. Their sounds are played by `ABrixSound` (see Sound): `ABrixLife::OnRewardSound`
is broadcast with sound.js's names (quest_start, coin_out, coin_land, coin_take, quest_end, interest_start,
interest_coin, interest_end, achievement). The markers over people's heads and on the maps are under The maps. Quests are kept in the save. The game menu's Me page
lists the achievements, those earned first with the day of each, then those still to earn (UI-019; `-BrixMenu=me`). To see a reward in a test shot: `-BrixLifeLead=1.15
"-BrixLife=act,new_in_town,r0504,stage:bank;open,current;check;buy,grocer,bread;check;act,new_in_town,r0504,take:bread|end:gift"`
(the ops that many seconds before the shot); to see a quest talk, `-BrixTime=15:12 -BrixTalk=504 "-BrixAnswers=1,1"`.

## The game menu and settings (UI-005 to UI-016, UI-019; F-UI-MENU 2, F-UI-SETTINGS 1; ADR-0049)
Esc opens the game menu on its home (Esc again, or Back to town, closes it; Esc first closes a counter or a talk); C, I, G,
P, J, U, M and H open a page directly, as World/rules/settings.json binds them, and its key again goes back to town. While
it is open the clock stands still (the game is paused; `menu.pauses`). The pages are settings.json's, in its groups You,
Town and Game, each in its colour: Me (what you have, the achievements earned first with their day then those still to
earn, people you know), Money (accounts, statement), Belongings (what is carried, each drawn on its plinth), Journal (the
quest log), Wardrobe (below), Settings (one group at a time), Save, Help (the keyboard's controls by where they work,
from settings.json); Properties (the real estate market, below); Storage says what it waits for (F-PROP-STORAGE); Map opens the
big map; and Save shows its three slots (see Saving). The settings are
`BrixWorld/BrixSettings`: settings.json checked by world.settings_problems()'s rules (`UI009_settings_are_valid`,
`UI009_a_bad_setting_is_caught`, `UI008_every_platform_maps_every_action`, `UI010_no_control_does_two_things`,
`UI005_every_tab_opens_from_a_control_on_every_platform`), the player's values kept on the device in
`Saved/BrixTown/settings.json`, not in a save, and taking effect at once: sun shadows, ambient occlusion, bounce light,
resolution and the Picture choice that sets them together, field of view, the frame counter, sound on or off and its
level, text size, look speed, invert and camera distance. The minimap, names over residents and the map's filters work (see The maps); first person and the
separate sound levels are kept for when those come. The menu, the counters and the buy menu are in "Lamplight"
(ADR-0049, accepted by the owner; `BrixLook.h`, its colours in settings.json `menu.lamplight`): cards of warm dark stone with round corners and a thick lower edge,
the town dimmed behind, each page's colour as its lintel and mark, lantern-orange main buttons, prices on paper tags, the
goods on plinths in a pool of lantern light. Shots: `Art/StyleTests/lamplight_menu_36/`. To see a page in a test shot:
`-BrixMenu=<home|me|wallet|quests|belongings|settings|help|...>` (and `-BrixSettingsGroup=<n>`); the shot is taken by the
menu itself, since the paused game's timers stop.

## The real estate market (PROP-009 to PROP-023, BANK-002; F-PROP-ESTATE 2; ADR-0022)
The slice's market (estate.js, a port of `Tools/citysim/estate.py`): `BrixWorld/BrixEstate` holds every building's units
(estate.carve()'s, written into `Game/Data/life.json` as `estate_units` by `build_play.py`, with World/rules/estate.json and
furnishing_levels.json as `estate`), each with an owner (the town, one of the three companies, the household or business
there, or the player) and maybe a tenant; prices from area, kind, place, floor and the block's index; listing, selling,
letting, lodgers, households moving house, upkeep and tax, seizure, mortgages, dividends and the town's spending, all
settled one day at a time at each midnight as a hook of the economy's day (never ticked), every chip through the ledger.
`PROP021_the_market_matches_the_python` runs estate.check_script() (buying a flat and, on a mortgage, a house; renting a
flat; setting a rent; listing; selling at once; 30 days) and must give Python's ledger, balances, owners, tenants, block
indices, history, mortgages, listings, moves and furnishing levels to the chip; `PROP009_every_building_is_carved_into_owned_units`;
`PROP018_the_money_supply_holds_and_a_saved_market_goes_on` (a year with no player within estate.json's drift, and a
market saved to text and loaded going on exactly as the one that was not). A new game opens the estate agents. In play,
as in the slice: B at an estate agent's office (Lark & Daughters, Harrow Property Co., Cobblestone Homes) opens its counter
(what is for sale, by kind, cheapest first; what is empty to let; outright or on a mortgage; your property with Sell now,
List and Take it off the market); P (or the menu's Properties page) shows the building you stand in with what its owners
would take for each unit and the whole building, your buildings with Show on map (the mark at it, and the big map), your
property with its rent to set and Offer to let, the places you rent with Move out, mortgages with Repay now, and lately
sold. A board over the door of every building within 32 m with a unit for sale or to let (`BoardReach`; "▣ 2 to let").
The big map outlines your buildings and the places you rent in the lantern's orange (settings.json `map_mine`). The
market is kept in the save. To see it in a test shot: `-BrixCounter=lark "-BrixLife=open,current;give,900000;days,3;buy_cheapest,flat,lark"`,
`-BrixMenu=property "-BrixLife=open,current;give,3000000;buy_lot,b_9_6_l4;buy_cheapest,house,lark,25,60;rent_cheapest,flat"`,
`-BrixMapZoom=5,912,624 "-BrixLife=open,current;give,3000000;buy_lot,b_9_6_l4"` (ops: `give,<chips>` test money into the
current account, `days,<n>` the clock moved on, `buy_cheapest,<kind>,<agent>[,<deposit %>,<term>]`, `rent_cheapest,<kind>`,
`buy_lot,<lot id>`). Shots: `Art/StyleTests/unreal_estate_38/`. Not yet: the residents who moved house walking to their new
home (they keep their day at the old one), the furniture that goes with a unit sold (F-PROP-FURNISH), the board's own
sign on the wall (it is words over the door).

## The maps and the words over the street (UI-012, UI-018, QST-005)
The town is drawn from its data as the slice's play.js drawBase() draws it (`BrixWorld/BrixMap`): the ground, Old Town's
streets, the roads out of town (roads.json links and gates), each block's lanes or open ground (woods, commons, orchards,
fields and their crops, ponds, paths, tracks, fences, hedges, plots for sale) or paving, each lot in its kind's colour
turned by its angle (a sculpted shell's own outline with its domes, roof light and ways in), every tree and the tram line,
into one 2700 px texture (`MapPixels`; 0.6 s). `ABrixMap` shows it: the round minimap at the top right, the 230 m round the
player, with the residents out of doors, the people and places quests want (life.js targets(): yellow a quest's, teal
someone with a line to give), the mark and the player's arrow turned the way the camera looks; and M (or the menu's Map
page) opens the big map in a square of 84% of the screen with its legend: the wheel zooms about the pointer (1 to 6
times; + and - about the middle, 0 or the middle button for the whole town), dragging moves it, a click marks a place (a
click on the mark clears it; Esc or M closes). The mark shows on both maps and over the street with how far it is, and goes
when reached. Over the street, as the slice's hud() and life.js mark(): the names of residents within 9 m on the player's
level (name, job, what they are doing or where they are going). Markers (ADR-0051, look test 37 "Brass token";
`BrixWorld/BrixMarkers`, numbers, colours and signs in `World/rules/markers.json`): over each person a quest or a job wants,
over each place it sends you to and over the mark, a brass coin with a sign for what it is (a letter: a line offered;
a speech bubble: someone to talk to; a bubble with a tick: going back to hand it in; a parcel: your job; a pin, or what
the place is: basket, cup, columns, awning; the mark's cream pin), ringed in its colour, in a warm halo. A step that needs
the player to have something (the first line's loaf of bread), or the shopping job, marks the 3 nearest shops with it in
stock today (`have_shops`; "Loaf of bread sold here" close by), until it is carried. A spark of its
colour far off (to 160 m), the token within 45 m (a place 30 m), bobbing; within 6 m larger, with a caption and its key
("E Talk to Mei"); behind a wall a faint outline; out of view a small token and arrow at the screen's edge; new things
pulse. The same tokens stand for them on both maps. Hidden while talking. What shows is the player's settings: display.minimap, display.names and the map group (residents, quests,
shop names). Tests: `UI018_the_big_map_is_drawn_from_the_town` (1,002 of the 1,005 lots show their kind's colour at their
middle, the streets, the tram line, the woods; the zoom's bounds and its fixed point), `UI012_the_mark_is_set_cleared_and_reached`,
`QST005_the_people_and_places_a_quest_wants_are_marked`, `QST005_the_markers_are_sound` (markers.json as
world.marker_problems() checks it, and a broken copy caught the same way; a spark, a token and a near token by distance),
`QST005_each_quest_step_gets_its_marker` (the first person to see, going back to them, the bank, a place by its kind, every
shop with the bread in stock and only the nearest 3 kept, none once it is carried, the shopping job, a line offered). Shots: `-BrixMap` opens the big map, `-BrixMapZoom=z,x,z` zooms it
on a place, `-BrixMark=x,z` sets the mark, `-BrixNear=<resident>[,<metres>]` stands the player 3.5 m (or that far) in front of someone; a shot logs
the residents on the maps, the markers and the words over the street. Your buildings and the places you rent are outlined on the
big map (see The real estate market). Not yet: a tree's crown by the day's vegetation (a fixed `TreeRadius`). 31 to 33 frames a second at High with the minimap (noisy: other sessions were running
Unreal).

## Skateboards (CITY-098 to CITY-101; F-VEH-SKATES 2)
The slice's boards (boards.js, World/rules/boards.json): `BrixWorld/BrixBoards` is boards.js (the kinds, which are owned,
the one in use, carried or ridden, the speed: W speeds up to the kind's top speed, letting go glides, S brakes, indoors no
faster than 8 km/h, 30 in a mall, a knock loses the speed into it), and `BrixWorld/BrixBody` draws them as people.js
board() and ride() do: the board in use carried by its edge in the right hand, wheels out; on it the rider stands side-on
across the deck, knees a little bent, and on the manual board pushes with the back foot on the ground while W is held,
the front foot along the deck. Y (settings.json `board`) gets on the board in use and off it again, picking it up; on a
board there is no walk: A and D turn the rider and the camera comes round behind them, more slowly the faster they go;
the bar at the foot of the screen says which board and how fast. A new game has no board until the skate store
(CITY-102); a test shot (or `-BrixBoards`) owns all three with the electric board in use, as the slice's `?auto=1`, and
`-BrixBoard=<manual|electric|neon_boost>` gets on that one (with `-BrixHold=steer,throttle`). `Tools/unreal/people_values.py`
runs boards.js and people.js in headless Edge into `Tests/values/boards_city_01.json`: `CITY099_boards_are_ridden_and_drawn_as_the_slice_does`
checks every speed of a ride of each kind and 36 riders and carried boards (vertex for vertex, within 1 mm);
`CITY098_board_kinds_and_speeds`, `CITY100_auto_owns_all_and_rides_electric`. Not yet: the skate store and buying
(CITY-102, CITY-103), the boards among the belongings in the menu (CITY-100), saving them, the server's check (CITY-104),
the sounds of getting on and off; the slice has no residents on boards, so there are none here either. 32 frames a second
at High riding along Tram Street (noisy: other sessions were running Unreal).

## Light and speed (measured 2026-11-24 on the Intel integrated graphics of the owner's laptop, 1066x666)
Lumen (bounced light, the sky's fill in shade) needs shader model 6 (`Config/DefaultEngine.ini`); without it the shade is
black. At Epic quality the town runs at about 36 frames a second, at High about 63 with the same shade, at Medium about 86
but without Lumen. On this laptop use High (`scalability 2` in the console, or `-ExecCmds="scalability 2"`). The light
follows the kit daylight (ADR-0007): a shaded wall on a wide street measures (161, 151, 135) against (159, 151, 146) in
`Art/Reference/world_street_kit_light.png`. The first run after a change of shader settings compiles shaders for minutes
and runs at a few frames a second; that is not the game's speed.

## Build, run, test
Paths below are for this machine (`docs/setup/DEV_ENVIRONMENT.md`); run from the repository root.

```
"C:\Users\leena\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" BrixTownEditor Win64 Development -Project="%CD%\Game\BrixTown.uproject" -WaitMutex
"C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%CD%\Game\BrixTown.uproject" -game -windowed -ResX=1600 -ResY=900
"C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject" -ExecCmds="Automation RunTests BrixTown.World; Quit" -unattended -nullrhi -nosplash
```
The first is the build (add `-NoHotReloadFromIDE` when the game is running from another copy of the project: Live Coding there otherwise stops the build), the second plays it, the third runs the tests. Without `-game` the second opens the editor; press
Play there. To check the picture without anyone at the keys, add `-BrixShot=C:/full/path/shot.png` (a screenshot after
12 s, then quit) and optionally `-BrixCam=x,z,height,yaw,pitch` (metres and degrees, the data's frame, yaw 0 = east).

## Tests
| Test | Requirement |
|---|---|
| `BrixTown.World.WLD020_city_loads_from_data` | WLD-020 |
| `BrixTown.World.WLD022_every_lot_is_sound` | WLD-022 |
| `BrixTown.World.WLD022_broken_lots_are_caught` | WLD-022 |
| `BrixTown.World.WLD038_surfaces_match_the_rules` | WLD-038, WLD-039 |
| `BrixTown.World.ART001_every_house_has_its_line_and_colourway` | ART-001 (ADR-0032, ADR-0035) |
| `BrixTown.World.WLD043_the_sun_follows_the_calendar` | WLD-043 (ADR-0024) |
| `BrixTown.World.CITY022_residents_are_where_their_routine_puts_them` | CITY-022 |
| `BrixTown.World.ART004_people_are_drawn_as_the_slice_draws_them` | ART-004, ANIM-001, ANIM-002, ANIM-004, ANIM-005 |
| `BrixTown.World.ANIM005_people_look_at_whoever_comes_near` | ANIM-005 |
| `BrixTown.World.CITY013_every_enterable_building_has_a_doorway` | CITY-013 (ADR-0048) |
| `BrixTown.World.FURN060_every_planned_home_is_furnished_in_its_rooms` | FURN-060, FURN-061, FURN-063 (ADR-0050) |
| `BrixTown.World.WLD052_every_storey_is_reached_by_its_stair` | WLD-050, WLD-052 |
| `BrixTown.World.CITY013_every_indoor_place_is_inside_its_lot_on_its_floor` | CITY-013, WLD-053 |
| `BrixTown.World.CITY013_shops_cafes_and_workplaces_are_furnished_at_their_places` | CITY-013 |
| `BrixTown.World.WLD051_towers_have_a_lift` | WLD-051 |
| `BrixTown.World.FURN060_every_planned_flat_is_furnished_inside_its_walls` | FURN-060, FURN-063 |
| `BrixTown.World.CITY013_rooms_are_dressed_as_the_slice_dresses_them` | CITY-013 |
| `BrixTown.World.CITY017_residents_sit_on_garden_benches` | CITY-017 |
| `BrixTown.World.CITY040_vehicles_are_where_the_traffic_puts_them` | CITY-040 |
| `BrixTown.World.CITY041_trams_are_where_the_timetable_puts_them` | CITY-041 |
| `BrixTown.World.CITY041_players_ride_the_tram` | CITY-041 |
| `BrixTown.World.CITY049_resident_cars_give_way_to_the_player` | CITY-049 |
| `BrixTown.World.CITY070_residents_dodge_and_scold_as_the_slice_does` | CITY-070, CITY-071 |
| `BrixTown.World.CITY072_street_things_break_as_the_rules_say` | CITY-072, CITY-073 |
| `BrixTown.World.CITY074_damage_is_kept` | CITY-074 |
| `BrixTown.World.CITY075_repair_crews_mend_as_repairs_py_does` | CITY-075 |
| `BrixTown.World.CITY076_residents_shout_at_a_smash` | CITY-076 |
| `BrixTown.World.WLD037_the_weather_is_worked_out_as_world_py_does` | WLD-037 |
| `BrixTown.World.CITY080_residents_are_dressed_for_the_rain` | CITY-077, CITY-079, CITY-080 |
| `BrixTown.World.WLD036_the_clouds_are_where_world_py_puts_them` | WLD-035, WLD-036 |
| `BrixTown.World.WLD070_snow_falls_and_lies_as_world_py_says` | WLD-069, WLD-070 |
| `BrixTown.World.CITY107_residents_are_dressed_for_snow` | CITY-107 |
| `BrixTown.World.CITY007_every_conversation_is_sound` | CITY-007 |
| `BrixTown.World.CHR009_lines_are_mouthed_as_the_slice_mouths_them` | CHR-009 |
| `BrixTown.World.CHR001_the_designer_makes_the_slices_look` | CHR-001, CHR-002 |
| `BrixTown.World.CHR013_designer_values_stay_in_range` | CHR-013 |
| `BrixTown.World.CHR001_the_players_name_is_said_in_talks` | CHR-001, CHR-009 |
| `BrixTown.World.CHR014_an_outfit_is_only_clothes_and_hair_the_designer_offers` | CHR-014 |
| `BrixTown.World.CHR015_the_wardrobe_holds_a_few_outfits` | CHR-015 |
| `BrixTown.World.CHR016_clothes_bought_set_only_their_parts` | CHR-016 |
| `BrixTown.World.QST008_a_save_brings_the_game_back` | QST-008 |
| `BrixTown.World.SND018_lines_are_voiced` | SND-018 |
| `BrixTown.World.SND018_unvoiced_lines_are_murmured` | SND-018 |
| `BrixTown.World.ECO001_money_is_whole_chips_in_balanced_lines` | ECO-001, ECO-002, ECO-003, BANK-007 |
| `BrixTown.World.ECO002_the_ledger_matches_the_python` | ECO-002, ECO-005, ECO-010, ECO-011, ECO-013, BANK-001, BANK-007 |
| `BrixTown.World.BANK008_the_bank_settles_as_the_python_does` | BANK-002, BANK-008 |
| `BrixTown.World.SND017_people_sound_as_the_slice_hears_them` | SND-017, SND-003 |
| `BrixTown.World.SND003_the_players_steps_and_landings` | SND-003, SND-009 |
| `BrixTown.World.SND007_sounds_are_heard_from_where_they_are` | SND-007 |
| `BrixTown.World.SND009_every_sound_has_its_recording` | SND-009 |
| `BrixTown.World.SND002_engines_sound_as_the_slice_hears_them` | SND-002, SND-009 |
| `BrixTown.World.SND004_the_tram_sounds_as_the_slice_hears_it` | SND-004 |
| `BrixTown.World.UI021_every_thing_shows_what_python_says` | UI-020, UI-021 |
| `BrixTown.World.UI020_dots_and_chips_show_the_slices_marks` | UI-020 |
| `BrixTown.World.SND001_the_town_sounds_by_time_of_day` | SND-001 |
| `BrixTown.World.SND007_outside_is_muffled_indoors` | SND-007 |
| `BrixTown.World.SND010_the_score_changes_through_the_day` | SND-010, SND-011, SND-012 |
| `BrixTown.World.SND019_coins_ring_as_the_slice_rings_them` | SND-019, SND-020 |
| `BrixTown.World.SND014_the_radio_has_podcasts` | SND-014 |
| `BrixTown.World.SND015_what_people_do_makes_a_sound` | SND-015 |
| `BrixTown.World.SND016_talk_is_turns_and_quiet_places_stay_quiet` | SND-006, SND-016, CITY-071 |
| `BrixTown.World.SND016_each_resident_speaks_in_their_own_pitch` | SND-016, SND-017 |
| `BrixTown.World.QST001_quests_run_as_the_python_does` | QST-001 to QST-007 |
| `BrixTown.World.QST009_achievements_are_earned_as_python_says` | QST-009 |
| `BrixTown.World.SND019_money_coming_in_is_shown_as_python_says` | SND-019, SND-020 |
| `BrixTown.World.UI005_every_tab_opens_from_a_control_on_every_platform` | UI-005, UI-013, UI-015 |
| `BrixTown.World.UI008_every_platform_maps_every_action` | UI-008 |
| `BrixTown.World.UI009_settings_are_valid` | UI-009 |
| `BrixTown.World.UI009_a_bad_setting_is_caught` | UI-009 |
| `BrixTown.World.UI010_no_control_does_two_things` | UI-010 |
| `BrixTown.World.UI018_the_big_map_is_drawn_from_the_town` | UI-018 |
| `BrixTown.World.UI012_the_mark_is_set_cleared_and_reached` | UI-012 |
| `BrixTown.World.QST005_the_people_and_places_a_quest_wants_are_marked` | QST-005 |
| `BrixTown.World.QST005_the_markers_are_sound` | QST-005 |
| `BrixTown.World.QST005_each_quest_step_gets_its_marker` | QST-005 |
| `BrixTown.World.CITY098_board_kinds_and_speeds` | CITY-098 |
| `BrixTown.World.CITY099_boards_are_ridden_and_drawn_as_the_slice_does` | CITY-099, CITY-101 |
| `BrixTown.World.CITY100_auto_owns_all_and_rides_electric` | CITY-100 |
| `BrixTown.World.CITY017_every_plant_is_drawn_as_the_slice_draws_it` | CITY-017, WLD-040 |
| `BrixTown.World.WLD056_the_open_ground_is_drawn_as_the_slice_draws_it` | WLD-056, ART-001 |
| `BrixTown.World.WLD032_children_play_as_the_slice_puts_them` | WLD-032, WLD-041 |
| `BrixTown.World.WLD028_the_street_dressing_is_drawn_as_the_slice_draws_it` | WLD-028, WLD-031, WLD-034 |
| `BrixTown.World.WLD056_every_tree_is_drawn_as_the_slice_draws_it` | WLD-056 |
| `BrixTown.World.CITY043_the_road_markings_and_signals_are_drawn_as_the_slice_draws_them` | CITY-043, CITY-045, CITY-091 |
| `BrixTown.World.WLD058_eastgate_is_built_as_the_slice_builds_it` | WLD-057 to WLD-061, WLD-063, WLD-067, WLD-068 |
| `BrixTown.World.WLD058_only_eastgate_has_an_inside` | WLD-058 |
| `BrixTown.World.WLD062_moving_stairs_carry_and_lifts_go_between_floors` | WLD-062 |
| `BrixTown.World.WLD066_every_outlet_has_its_room` | WLD-066 |
| `BrixTown.World.CITY105_every_outlet_has_a_counter_its_keeper_stands_at` | CITY-105, WLD-067 |
| `BrixTown.World.CITY106_visitors_shop_at_the_counters_and_eat_at_the_tables` | CITY-106, WLD-068 |
| `BrixTown.World.BUILD002_every_building_is_built_from_its_design` | BUILD-001, BUILD-002, BUILD-016, BUILD-019 (ADR-0052) |
| `BrixTown.World.BUILD005_remodels_match_the_python` | BUILD-004 to BUILD-007, BUILD-016 to BUILD-019 |
| `BrixTown.World.QST008_remodels_are_kept_in_the_save` | QST-008, BUILD-005 |
| `BrixTown.World.PROP021_the_market_matches_the_python` | PROP-010 to PROP-022, BANK-002, FURN-063 |
| `BrixTown.World.PROP009_every_building_is_carved_into_owned_units` | PROP-009, PROP-010 |
| `BrixTown.World.PROP018_the_money_supply_holds_and_a_saved_market_goes_on` | PROP-018, PROP-021, QST-008 |
| `BrixTown.World.FURN020_the_outlet_is_built_as_the_slice_builds_it` | FURN-020 |
| `BrixTown.World.FURN021_staff_and_browsers_are_at_the_outlets_places` | FURN-021 |
