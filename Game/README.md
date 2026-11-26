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
| `Config/DefaultScalability.ini` | The project's own changes to the engine's quality levels (Lumen's surface cache at High) |

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
the district's palette; inside, each storey's walls and floor, and its rooms (BUILD-021 to BUILD-023): the storey's inner walls as a plan
(`BrixWorld/BrixRoomPlan`, builder.js RoomPlan and storeys.js planOf(), wallsOf()), each wall picked from a list to open or
close its doorway, fit between the walls it meets, make longer or shorter, move a quarter metre either way, paint in one of
the district's inside colours or take down, and walls added east to west or north to south; drawn at once by `ABrixRooms`
(`SetPlan`), checked (in the room, clear of the way in and the stairs, every room reached from the way in, play.js
roomFor()) and priced line by line with the rest of the work; and its furniture (Furnish, FURN-051, FURN-052, FURN-055;
`BrixWorld/BrixFurnishPlan`, builder.js Furnish and play.js townPieces()): the storey's own pieces (the boxes the town
furnished it with, grouped into pieces: chairs, cabinets, lamps, rugs; those on a wall stay) picked in the view and dragged across
the floor, turned or put with your belongings; your belongings placed again; pieces of
World/rules/furnishing.json added by room and line (bought; the mall's goods are bought at its outlets, FURN-064), drawn as
the furniture kits' own meshes (a box its size where a piece has none yet); each checked (in the room, on the grid, under
the ceiling, clear of the way in, the stairs and each other) and priced with the rest; what you take away is among the
belongings (the menu's Belongings page). Each choice shows on the building at once
(`ABrixTownActor::Repaint`: its instances folded away and the building built again); the work is priced line by line;
what would stop it is said (builder.check()); Pay and build pays through the ledger (`sys:builders`) and the remodel is
there at once (F-BLD-CONSTRUCT level 0, instant); Undo, Start again; Esc leaves it as it was. Remodels are kept in the
save (`QST008_remodels_are_kept_in_the_save`), room plans (`QST008_room_plans_are_kept_in_the_save`), and each storey's furniture and the pieces among the belongings
(`QST008_furnishings_are_kept_in_the_save`); `FURN051_furnishing_matches_the_python` runs furnish.check_script() against
Python's (the problems, the cost and the belongings after each edit);
`BUILD021_room_plans_match_the_python` runs roomplan.check_script() against Python's and checks that every storey of flats'
plan draws back the town's own walls.
The mouse (life.js pointer(); BUILD-020, BUILD-021, FURN-055): a pad behind the card takes it (`SBrixDesignPad`). On the
Furnish tab a click picks the piece under the mouse (the nearest box the camera's ray goes into, a rug only where nothing
stands on it) and a drag moves it across the floor on the 5 cm grid, keeping its distance from the mouse; R turns it,
Shift+R the other way, Delete puts it with the belongings. On the Inside tab a click picks an inner wall and a drag moves
it across and along on the grid, inside the room, an end within 0.3 m of a wall across it (or of the room's side) catching
on it, its doorway going with it. What is picked is outlined in the lantern's orange with its corners standing up, what is
under the mouse in a pale line, a piece that cannot stand where it is in red (`ABrixRooms::SetMarks`, lit by M_Glow). On
open ground a drag turns the view and tips it; the right or middle button or Shift slides it (20 m beyond the building at
most); the wheel comes nearer, towards the point under the mouse; a double click looks at a spot. The picking and the
dragging themselves are `BrixWorld/BrixDesignPick` (play.js pickPiece(), pickWall(), designMarks(); life.js wallStops()),
tested by `FURN055_a_piece_is_picked_and_dragged_on_the_grid` and
`BUILD021_a_dragged_wall_stays_in_the_room_and_catches_on_walls`.
The storey looked into is cut open (BUILD-020; life.js syncView()): on the Inside and Furnish tabs the building is drawn
again with nothing above 1.25 m over that storey's floor (`ABrixTownActor::Repaint`'s cut: a box standing through it ends
there, a kit piece through it is left out), `ABrixRooms` draws only that storey's rooms however far the camera is
(`SetDesign`), and the haze is a quarter of the street's (`DesignHaze`); from outside, and when the designer closes, it is
whole again. New pieces and belongings go where the view looks, inside the room.
The pieces to add are objects in a grid (UI-020, layout D: the card is narrower than buy_menu.json's `narrow_px`): cells
four across, each the kit's own mesh of that piece in the chosen line drawn from its triangles
(`BrixThingFromStaticMesh`), its price under it and the mark of where it goes; the chosen one turns, and a strip under the
grid gives its name, "Into the room", its size, its price and the Add button; a second click on a cell adds it.
A building drawn again (each choice tried, each tab that cuts it open) now takes about 0.1 s on the owner's laptop, where
it took 6 to 7 s: its old instances are folded away a run at a time (`BatchUpdateInstancesTransforms`), not one by one.
The card stands at the left at any window size and keeps room for its scroll bar, and its rows of buttons wrap at a fixed
width (at 1600 x 900 they were measured for one width and laid out at another, and lay over one another).
Ops for a shot: `tab,inside`, `storey,<f>`, `addwall,<x|z>`, `wall,<id>`, `door`, `close`; `tab,furnish`, `piece,<id>`,
`turnpiece,<deg>`, `movepiece,<dx>,<dz>`, `takeaway`, `addpiece,<kind>,<line>`, `placeheld,<n>`, `listpieces`; the mouse:
`dragpiece,<id>,<x>,<z>` presses on that piece in the picture and drags its middle to there (`dragpiece,<id>,<dx>,<dz>,by`:
that far), `dragwall,<id>,<x>,<z>` the same for an inner wall, both sent through the interface as a mouse's own press
would come (the log says whether the pad took it), `view,<turn px>,<tip px>,<wheel clicks>` drags the view, and
`later,<s>` does the ops after it that much later (a picture has to be drawn before a mouse can be on it; a fifth of a
second is enough, and a long wait can outlast the shot when the start is slow), such as
`"-BrixLife=open,current;give,3000000;buy_lot,b_9_6_l2;remodel,b_9_6_l2;tab,furnish;view,60,-20,7;later,0.2;dragpiece,t3,0.6,0.4,by"`.
To see it in a test shot: `"-BrixLife=open,current;give,3000000;buy_lot,b_9_6_l4;remodel,b_9_6_l4;set,walls.*.plaster=peach;set,roof.tiles=slate;set,openings.frame=teal"`
(and `;pay` to build it). Shots: `Art/StyleTests/unreal_designer_39/` (`furnish_drag.png`, `rooms_drag.png`: a piece and
a wall dragged by the mouse, the storey cut open, the grid of pieces). Not yet: the town pieces' own
names (the slice tags them; here they are named by their shape), the picked piece's name over it in the picture (the card
says it), the planned homes' furniture in the designer (as in the slice, only buildings furnished at their
places), furniture sold with a unit (FURN-053, F-PROP-FURNISH: not in the slice or in Tools/citysim either, and still
proposed), the rooms of a building not drawn inside; a town piece placed from another
building is drawn as a box its size; a building drawn again from a design (each choice tried, each remodel paid, each
tab or storey that cuts it open) leaves
its old instances folded away under the ground until the town is next built (a few hundred each time; ADR-0052) (FURN-051, FURN-052, FURN-055), the window class's sizes, balconies, keystones, corner stones
and wall lanterns drawn from the design (offered, priced and kept, but laid by the kit's rules), the awning in the
design's accent; building new on a plot (BUILD-008: F-MAP-PLOTS is at
level 0, no plots). Of the cut-open storey: the walls are cut at the slice's height but a window or door that stands
through the cut is left out whole, so the cut walls have gaps where their openings are; the residents of the storeys cut
away are still drawn; with the roof off the sun on a pale floor is very bright (the eye is not held for it). Of the
mouse: a piece wholly behind another can only be picked from the list of pieces by name (a button on the Furnish tab; the
slice has only the picture), walls still have their Move buttons beside the drag, and the view's wheel and slide are not
kept from one opening of the designer to the next. Of the pictures of pieces: they are read from the meshes' own
triangles, which a packaged game keeps only for meshes marked for it (the furniture kit's are not yet, so there a piece
would show as a box its size); the arrow keys do not choose in the designer's grid (as in the slice, only at a counter).

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
mallShell()): `BrixWorld/BrixMall` is the port, giving the same 176,797 drawing calls with the same numbers (its hash() fed
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
as the owner asked. Tests: `WLD058_eastgate_is_built_as_the_slice_builds_it` (every call against the slice's, in 442 runs of
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
Unreal). Sitting on the benches, cafe seats and rides is as in the furniture outlet (below). Not yet: the ribbon wall's lit joints and the openings' lanterns at night, the
moving stairs' treads moving, and the shell's lanterns' light outside.
The goods for the home each outlet sells stand in it as they were made in Blender (FURN-065; outlets.js front() with the
slice's life data, goods.js put()): on its two window plinths facing the walk, and those too deep for a plinth on its floor
inside, 464 of them in 112 of the 189 outlets; where an outlet sells none the window keeps its stand-ins. What it sells is
the economy's `OutletRange` (World/rules/mall_goods.json in life.json), which make is `display.detail` (near). The models
are `Tools/blender/mall_goods_31.py`'s, copied from `Prototype/play/goods_models.js` to `Game/Data/goods_models.json` by
`build_play.py` and read by `BrixWorld/BrixGoods` (`LoadBrixGoodsModels`, `BrixGoodPut`, a port of goods.js put(): the
two open colours by the seed, so two guitars differ); `ABrixMall` lays each in the room's light. `mall_values.py` now runs
the slice with that life data (run `build_play.py` first) and records each good as a call of its own (kind 14,
`FBrixMallCall::Good`). Test `FURN065_outlets_show_the_goods_they_sell` (each good in the room of an outlet that sells it,
facing out, near; the triangles of six as goods.js lays them). Shot: `Art/StyleTests/unreal_mall_goods_41/`.

**People in the hall are lit by a light of their own (found in play, 2026-11-26).** The hall's faces glow by themselves
(`RoomLight`), which lights nobody: the player and the residents in it were lit only by what of the sky's light came in,
and once the night was dark (ADR-0055) they were black in the court at night. With the camera in a hall
(`ABrixMall::Contains`, so the furniture outlet's too) a light from over the camera's shoulder lights people alone
(lighting channel 1, which the player's and the residents' bodies are on): `PeopleLux` (0.6) at night and `PeopleLuxDay`
(0.2) of it with the day's light also coming in, set so that someone in the court at 02:00 looks as they do at noon
(`Art/StyleTests/unreal_play_bugs_44/mall_people_before_after.jpg`). **The hall has shade in it (the look gate's first use, ADR-0058).** Every face of the hall glowed evenly in one plain
colour: no shade, no shadow, no mottle, whites burnt. Now its faces are mottled (`RoomMottle`: two close tones blotched,
W1; `M_MallRoom`, made by `Tools/unreal/make_mall_materials.py`), a face that looks down has `RoomUnder` (0.35) of the
hall's even light, so the underside of a gallery is darker than its top, that even light is less (`RoomLight` 0.11)
and the lanterns more (`NearLights` 6 of `LightCandela` 25 reaching 16 m), the nearest shining down out of its shade
and casting shadows (`LightShadows`, `LightCone`): the floor has pools of light with dimmer ground between them. The
lines of light are less bright (`LitGlow` 1.4) so they are not burnt. Shots beside the owner's screenshot:
`Art/StyleTests/unreal_look_gate_49/`. Not yet, by the gate's questions: G1 nothing darkens the floor where it stands
(planters, benches, the moving stairs, people's feet: the hall's faces have no share of the engine's bounced light, as
they are made at load time); G2 only the nearest lantern casts shadows, and the lines of light and the shops' lit
fronts light nothing; G3 the floor tiles are plain within each tile; G6 people's own light has no shadow side to
speak of; the lanterns that give light are chosen again twice a second and change in one frame; the upper floors and
the shops' insides have not been looked at with the gate; there is no reference picture of a hall to measure against
(ART_DIRECTION §5). The day's light still comes into the hall
and onto people, which is why the light is weaker by day; the goods and mannequins are not on the people's light.

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
frames a second in the hall and over the car park (noisy: other sessions were running Unreal).
The delivery bay (FURN-020): the block file's `vans` (3) stands a company van along the wall beside each loading door, the
traffic's own van model in the company's teal (a parked car whose seed starts "van"; cars.js parked(), outlet.js build(),
`ABrixCars::BuildParked`), in the way like any parked car; the slice draws them too.
Trying the furniture (FURN-022's sitting and lying; play.js use()): E at a room set's sofa or armchair sits the player on
it, at a bed lies them down, at the cafe's chairs and the other rooms' seats sits them, facing as the piece does; E, Space
or moving gets up, where they stood. The offer under the picture says what E does ("Try the sofa", "Get up"). The places
are the slice's own use points, kept by `ABrixMall` (`Seats()`, `SeatNear()`), so Eastgate Mall's benches, cafe seats and
rides are sat on the same way; `ABrixCharacter::StartSeat` draws the body sitting (pose 2) or lying (pose 3) on the
seat's top. `-BrixSeat=Try_the_sofa` sits the player at the first seat whose offer has those words, for a shot. Test:
`FURN022_the_outlets_sofas_chairs_and_beds_can_be_tried` (a sofa, an armchair and a bed in each of kit 17's four rooms,
the three vans). Shots: `Art/StyleTests/unreal_furniture_outlet_39/try_the_sofa.png`, `delivery_vans.png`.
Buying at its tills (FURN-022 to FURN-029; the owner, 2026-11-25): `BrixWorld/BrixStore` is a port of
Tools/citysim/furniture.py (as the slice's store.js is), with World/rules/furniture.json and Python's fixed run carried in
life.json (`furniture`); `FURN025_the_store_matches_the_python` runs it op for op, reads a saved store back and checks
where a delivered piece is set down. B in the outlet opens the till (`ABrixLife::StorePanel`): the showroom's pieces by
room and line as objects, each the kit's own mesh, with what its tag says beside the chosen one (size, whether it comes
flat-packed and in how many boxes, what putting it together takes); a basket in which each piece is taken now (free, to
the belongings) or delivered to a home of the player's in a slot, in its boxes or put together by a fitter; the bill line
by line, paid in one ledger transaction to the company (`co:furniture`) or not at all; and the deliveries, each with where
its van is by the clock, and the flat packs waiting, to put together yourself (it takes the piece's time) or by a paid
fitter. With the outlet's catalogue (free at the till), B at a home of the player's orders from there, for delivery only
(FURN-023). What is carried in is set down on the home's floor where it was marked or at the free spot nearest the way in
(`BrixStoreSetDown`), a flat pack as a box the size of its stack until it is put together, and becomes part of the
storey's furniture (moved and taken away in the designer like any piece); if the home is no longer the player's the piece
goes to the belongings and they are told. The company's takings go on to wages and stock at each day's end (FURN-024).
The store is kept in the save. Ops for a shot: `store` (`store,home`), `storetab,<show|basket|book>`,
`basket,<kind>,<line>,<instant|delivery>[,assembled]`, `storeunit,<unit>`, `checkout`, `assemble,<piece>`, `catalogue`,
such as `"-BrixLife=open,current;give,3000000;buy_lot,b_9_6_l2;store;basket,k17:sofa,plain,delivery;checkout;days,2"`.
Shots: `till_showroom.png`, `till_basket.png`.
Not yet, of buying: the delivery is not seen (no van driving to the home, no crew carrying the pieces in: FURN-027's
bodies; the vans stand in the bay) and how far the van drives is the streets' grid from door to door, not the roads; a
piece's tag is in the till, not on the piece in the hall, and the piece on show is not switched to its other lines there
(FURN-022); putting a piece together does not keep the player busy; a piece delivered to a home that is not furnished in
the designer (a planned row house, a court flat) or with no room on its floor goes to the belongings, where a flat pack is
whole; a delivery that lands while the designer is open on that home shows when it is opened again; the slots never fill
(FURN-032) and nothing runs out of stock (FURN-031); residents buy nothing (FURN-034); the company's name is a placeholder
(Q-32) and every fee and time a first guess. Also not yet: the vans driving, residents browsing do not sit on the
showroom's pieces, the player sat on a ride in the mall just sits (the rocket and the car do not rock), and a seat taken
by a resident can still be sat on.

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
surface cache was some 20 % short of what the town's insides would fill (a red line over test shots, and far surfaces
losing their bounced light at random). A larger cache (`r.LumenScene.SurfaceCache.AtlasSize`) is not set, to spare the
laptop's video memory; at High each surface's picture in it is 80 % a side instead (`Config/DefaultScalability.ini`,
ADR-0055), which fits: no warning in the spawn alley or on Market Square.

## Furnished homes (FURN-060 to FURN-063; ADR-0050)
The planned homes are furnished as the slice furnishes them (homes.js, houses.js): 469 designed houses (plan house_12x9)
and 250 row houses (lot.interior) from World/rules/home_plans.json, with their inner walls and doorways, tiled kitchens and
bathrooms, and each plan's pieces at the household's furnishing level (Bare, Basic, Homely or Full from its daily wages and
the day, FURN-063) with a bed for each resident; tall pieces in front of the building's real windows are left out. Where
each piece stands is `BrixWorld/BrixHomes` (`FURN060_every_planned_home_is_furnished_in_its_rooms`). The pieces are the
furniture kits' own meshes in the home's line: `Tools/blender/unreal_furniture_37.py` exports them (Blender, about 20 s),
`Tools/unreal/import_furniture_kit.py` imports the ones the plans use into `/Game/Furniture` (the .glb files are not kept in
git). `ABrixRooms` furnishes the homes within 30 m of the camera, and lights the rooms of the home the camera is in (three
lamps: 3 to 5 fps). What a room's light is, and how that is known, is under What a room's light should be.
`-BrixStand=x,z,yaw` puts the player in a room for a shot.

Furnishing is everything near cleared and made again, a long frame, and it ran at every few steps along a street of
houses and at every doorway. A home furnished now stays so until it is `KeepMetres` (20) further off than `NearMetres`,
homes are dropped `DropAtOnce` (8) together or when one comes near, and going in or out of a building of one or two
floors furnishes nothing again (which floors are built turns on the camera's floor only in taller ones). Not yet: each
home as its own pieces, made once and hidden or shown, so that nothing near is ever made again.

## Designed houses (ADR-0032, ADR-0035)
The 469 designed houses (lot.house) are built as the slice builds them (houses.js outsideS): the 12 x 9 m house of
`World/rules/home_plans.json` (house_12x9), two 3.3 m storeys on a 0.3 m plinth, its nine windows and front door where
the plan has them, shutters open at every window, a string course between the storeys, a 24 degree roof with the
chimney and its two pots on the left gable; and the line's trim and its colourway from `World/rules/house_styles.json`:
plaster or ashlar walls, shutters, door, frames and roof in the colourway's colours, quoins on the Old Town line, window
boxes on Old Town and Curve, an arched door on Curve. Test `ART001_every_house_has_its_line_and_colourway`.
The plans of storeys and flats list their windows by floor; they are read as the first floor's (read as one list, they
gave none and five warnings at every start, 2026-11-26).

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

The sun is turned only when it has moved `SunStepDegrees` (0.1, about every four seconds): turned every frame, the engine
kept none of the town's shadow from one frame to the next and drew it all again each frame. Its colour and strength
still follow the clock every frame.

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
opens the market on loading). Remodels and room plans are in the save (life.js remodels and plans:
each building's design and each storey's walls as paid for, drawn again on loading). The save keeps the skateboards owned
(with the economy, as economy.py save() keeps them) and the one in use (life.js save()'s boards), which comes back in the
hand (CITY-100). Furnishings (each storey's furniture) and the furniture among the belongings are in the save too. Not
yet in the save: a server keeping it (PH2).

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
Who has which voice is the slice's index, which names residents by id (ADR-0060); `Tools/unreal/people_values.py` turns it
into the list by place in the residents that `BrixTalk` reads (-1: not voiced). Not yet: `BrixTalk` reading the voices by
id itself, so until then `people_values.py` must be run again whenever the residents or the voices change.
A story line's words alone are voiced and mouthed: what is seen (its direction) and the name of somebody else who says it are
written in the box before the words (`FBrixDialogue::Shown`), and somebody else's line is in their own voice (QST-012).
Oda (r3419) speaks her seventeen story lines in her own voice (`cast_oda`, made by `Tools/voice/cast/`, ADR-0060;
`SND018_the_cast_speak_their_story_lines`; eleven of them seen in the log of two staged talks on 2026-11-26, sound off).
Noor (r0262) speaks 32 of her 111 lines in a Maya1 voice (`cast_noor`, ADR-0063); the rest are murmured. Not yet:
the rest of Noor's part, the other eleven of the Baker cast, the lines nobody says, and Oda's everyday talk, which is murmured until it is voiced.
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
car passing through; as low as the head needs under the roof, feet along the floor, hands on the wheel. The cabins
are wells cut out of the body down to a floor, the seats on it (ADR-0057; before, the body and its waist ran solid
through the cabin, a board over everyone's lap, and the low roofs sank tall riders up to 0.44 m into it): the tallest
rider sits on the seat with 5 cm under the roof (`CITY067_riders_show_and_fit_in_the_cars`;
`Art/StyleTests/car_cabins_45/`). Not yet: the feet reach under the dashboard into the solid front of the body (no
footwell is modelled). The player can drive
any parked car or van (a stand-in for CITY-061, as the slice's cars.js): E beside it gets in (it steps out of its bay
onto the road first), W/S speed and brake, A/D steer, Space the handbrake, up to the top speed of World/rules/traffic.json;
a street thing or a tree in the way breaks (CITY-072); a wall or another car stops it and it rolls back a little; the camera swings round behind; E gets out by
the driver's door, and the car stays where it was left for the rest of the visit. E offers whichever is nearer, a car to
drive or someone to talk to. A bus can be ridden (CITY-092): E at a bus
standing at a stop gets on, a seat near the front, carried along its line with its driver and riders, the camera behind;
E gets off when it next stands at a stop (and everyone gets off when it leaves service). The tram is ridden the same way
(below). To see a drive in a test shot: `-BrixDrive=<vehicle> "-BrixHold=steer,throttle"`. 51.5 frames a second at High on a street
with 32 vehicles drawn.

The models' triangles are taken in the other order from the one they are written in (found in play, 2026-11-25): as
they come they go round the wrong way for Unreal, which then drew each part's inside and left its outside out, so every
car and van was a dark hollow with its riders' legs showing through the doors
(`Art/StyleTests/unreal_play_bugs_44/cars_before_after.jpg`, against `Art/Reference/vehicles_soft_box.png`). The box's small things (drawn to `GardenCullDistance`) and its large ones (drawn however far) were one group, because
the distance was not part of a group's name: the large ones went at 150 m too. They are two groups now.

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
hedges (not drawn in Unreal yet), and a server keeping the list for every player. Nobody sits on a broken bench: whoever
the day has on it stands `SeatStep` in front of it until it is mended (`BrixOffBrokenSeat`, `CITY072_a_broken_bench_is_not_sat_on`;
`bench_broken.png` against `bench_whole.png`). At
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
`greybox_ripples.hlsl`). None of it, nor lying snow, reaches under a roof: indoor floors, stairs and furniture stay dry
(a roof map of every building's footprint and height, `BrixRoofMap`, half-metre cells, M_Greybox's Roofs and RoofsAt;
`RoofCell=0` turns it off to compare; `WLD037_rain_and_snow_stay_out_from_under_roofs`; `dry_indoors.png` against
`dry_indoors_before.png`, the street still wet in `wet_outdoors.png`). Residents wear what citysim gives them today (day.json "wear"): a coat,
and out in the rain an umbrella up, else the hood up, else a hand over the head while walking, with a hurried walk or a
run for cover; nothing but the coat indoors, sheltering or under a roof (`CITY080_residents_are_dressed_for_the_rain`).
The look of coats, hoods and umbrellas is still to be approved (ADR-0020). `-BrixWeather=<clear|cloud|rain|fog|snow>`
tries one kind for the whole day (and the clothes residents would wear on a day of it); the built day 16 has cloud
before 06:00 and is clear after. At High: about 40 frames a second in the 08:10 school rush in rain, against 15 to 54
in clear weather on the same street (noisy: other sessions were running Unreal on the same laptop). K is the slice's
weather key: it tries clear, cloud, rain, fog and snow in turn for the whole day, then the day's own weather again, and says
which (`BrixNextWeatherTry`, `WLD037_the_weather_key_steps_through_the_kinds`; `-BrixWeatherKey=<n>` presses it n times in a
test shot). From indoors the rain is seen through the windows and the open door: the same rings of drops, spread
`IndoorReach` (1.8) times as far, fall outside, none over the building (its roof) and no splashes; the slice shows none
indoors. Shots: `Art/StyleTests/unreal_gaps_world_40/` (`weather_key_rain.png`, `rain_through_the_window.png`); 7.9 to 20.5 frames a second at High in rain
on that street (noisy). Clothes get wet: everyone out of
doors (the player too) is darker by wet x sky.json `wet_dark` x `PeopleWet` (0.3, the slice's share: play.js darkens
everything out of doors so), drying as the paving dries, none indoors or in a vehicle (M_People's custom primitive data 1,
`BrixWetClothes`, `WLD037_people_out_in_the_rain_are_a_little_darker`; `wet_clothes.png` against `wet_clothes_before.png`,
the shirt 2 to 3 % darker in the picture through the rain's haze). Not yet: the rain's sound (the slice has none either),
snow seen from indoors; round a designed house a strip of its garden about half a metre wide stays dry, as the
roof map (and the rain's roof test) take the house's whole lot, not the house itself, as under a roof.

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
sessions were running Unreal). In lying snow (WLD-071) walkers leave footprints behind them, a footstep apart and left and
right in turn, and vehicles two tyre tracks, from where each went and when (their day and the clock; the player's own
steps and car from where they went this visit), filled in by falling snow and fading (sky.json `snow.tracks`;
`WLD071_footprints_and_tracks_follow_the_clock`); parked cars carry snow on their roofs and people out in it gather it on
their heads and shoulders while it falls (M_People's per-mesh `Snow`, custom primitive data 0, from
`make_people_material.py`); tree crowns whiten on top. Not yet: the sound of snow.

## Clouds (WLD-035, WLD-036; ADR-0014)
Soft heaps on one flat layer overhead, bright on the side towards the sun and shaded underneath, fading into the haze
towards the horizon, taking the colour of the hour (warm at dusk, dark at night). How much sky they cover follows the
day's weather slots, blended at their edges; where the wind has carried them and how their shapes change come from the
clock alone: `BrixCloudsNow`, a port of `world.clouds_now()`, checked by `WLD036_the_clouds_are_where_world_py_puts_them`.
They are drawn on the inside of a dome round the camera by `/Game/Weather/M_Clouds` (`Tools/unreal/make_clouds_material.py`,
`clouds.hlsl`); 46 to 50 frames a second at High under a clear-day sky and at dusk.

## The eye and the night (WLD-042, WLD-028, WLD-043; ADR-0055, after ADR-0023)
The game mode makes the sun, the sky's fill and the haze and turns them by the clock. `UBrixLight`, a part the engine
makes for every game world (nothing spawns it), adds what the picture needs as the sun gets low and after it is down; the
arithmetic is `BrixWorld/BrixNight` (`BrixLightNow`: from the day's light, the sun's height, the weather and whether the
camera is in a room), checked by `WLD042_the_days_light_is_kept_and_the_night_is_dark`. Out of doors by day with the
sun up it changes nothing: the haze and its glow are the game mode's own and the eye opens no further than the open street needs (`DayOpen`, 2.2 stops: The look gate, below), so the approved daylight
is as it was (ADR-0007). Then:
- **the eye**: the engine's automatic exposure opens without limit as the light goes, which made the night as bright as
  the day. It may open only so far past the day's exposure (`DayEV`, 1.3 measured in the open street at noon):
  `DuskOpen` stops out of doors with the sun on the horizon, `NightOpen` at night, `NightOpenIndoors` in a room by its
  lamps, and `DayOpenIndoors` in a room by day (below);
- **the haze**: the game mode turns all of it the low sun's orange at the day's full strength while the light itself
  falls away (dusk and dawn were an orange white-out, and the night kept the orange). A low sun's haze is dimmer and
  cooler (`LowSunHaze`), the sun's colour only in the air towards it (`LowSunGlow`); at night it is `NightHaze`, the
  slice's dark blue distance;
- **the lamps and the lit windows** come on as the sun gets low (`UBrixLight::LowSun()`, used by `ABrixLamps` and
  `ABrixRooms`), from about 19:00 to 06:40 in summer: with the sun on the horizon the alleys are in deep shade;
- **the night's light**: a faint blue light from high up (`NightLux`, `NightFrom`: the slice's own moon direction when
  its moon is down), the air's second light, so the sky is a deep blue and the sky light has that to fill the streets
  with (`NightSky`; the game mode's `NightFill` is 1). It casts shadows, so roofs keep it out of rooms.
Every number is in DefaultGame.ini, `[/Script/BrixTown.BrixLight]`. `-BrixLightOff` leaves the light as the game mode
alone makes it, to compare; `-BrixLightNo=<letters>` leaves parts out (e the eye, h the haze, n the night's light, s its
sky). Pictures before and after: `Art/StyleTests/unreal_light_43/`. Not yet: the stars, the Milky Way, the moon's disc,
path and phases (the night's light comes from one place and does not wax or wane), the planet (WLD-065); lamps lit in
shade by day as the reference renders have them; light from lit windows on the pavement (the panes are kept out of
Lumen); the alley's haze growing heavy from 18:00 as its shade deepens (the game mode's haze has one brightness whatever
the light). The haze's colour is set once a frame, by this (the game mode sets it only with `-BrixLightOff` or
`-BrixLightNo=h`). At High with nothing else running: 38 frames a second in the middle of the tour's 64
stops below, day and night, indoors and out (between 30 and 55 at most; a stop dips under that while its place loads).

**What a room's light should be (ADR-0055).** The look of rooms is decided by Blender renders with an exact recipe
(Tools/blender/furniture_kit_18.py: a shop, a cafe, a kitchen and nine more in one shell; the kit daylight through a
side window, pendants of 70 W in the lantern's colour, one camera and exposure), so the game's rooms are measured
against those, not judged by eye. `ABrixLightRoom` (`-BrixLightRoom`, which the tour script always passes) builds the
reference shop, cafe and kitchen again in Unreal, 250 m over open ground on paving of their own: the same shell, the
kits' own pieces at the same places (the meshes of /Game/Furniture), the game's own room lamp where each pendant hangs,
the references' camera (a tour stop `refroom:<shop|cafe|kitchen>`), turned so that at the hour `ref` the game's sun
comes in at the window as theirs does. `Tools/unreal/light_reference.py <reference.png> <unreal.png>` then compares the
same patches of wall, floor and ceiling in both: how much brighter Unreal's is in stops, and how much warmer. A tour stop
can try numbers on the spot (`lamp=<times> tint=r/g/b fill=<level> filltint=r/g/b ev=<EV100>`), so one run sweeps them.
What it showed, and what a room's light is as a result (`ABrixRooms`, every number in DefaultGame.ini):
- with the lamps out, the walls of the reference rooms in Unreal were 7 stops under Blender's: the engine brings almost
  none of the sky's light or of the sunlight off the floor indoors, and the references are lit mostly by that. So a room
  has **a light of its own**, even and warm, as the slice's roomLight() has: `RoomFill` by day, `RoomFillNight` of it at
  night, in `RoomFillColour`, over the floor the camera is on;
- **its lamps** are the references' pendant: `RoomLampWatts` (70) in the lantern's colour, a real lamp whose light falls
  off with the square of the distance, shining down out of its shade (`RoomLampCone`), the nearest `RoomLampShadows`
  casting shadows. `CandelaPerWatt` is a third of what the arithmetic gives (0.026 for 0.075): at the full strength the
  walls came a stop and more too warm. A planned home's room has one at its middle; a floor with no plan (a shop, a
  cafe, the bank) has them where its dressing hangs or stands a lamp, the `RoomLights` nearest the camera lit;
- **the eye** in a room by day may open `DayOpenIndoors` (2.4) stops past the street's exposure: there the reference
  rooms in Unreal are as bright as Blender's.
With those, the three reference rooms in Unreal come within about half a stop of Blender's in brightness and a third of
a stop in warmth (shop, cafe, kitchen: see `Art/StyleTests/unreal_light_43/rooms_against_blender.jpg`). The game's own
rooms are warmer and about a stop less bright than they were under the engine's free eye (ADR-0048's look): the
owner said yes to that on 2026-11-25. Not yet: the kits' lamp shades do not glow (their material has no glow), so a pendant's light has no bright
shade over it; the reference rooms are open behind the camera, as Blender's are, which a real room is not; Eastgate's
inside keeps its own light.

**Looking at the light everywhere in one run.** A launch costs one to four minutes on this laptop and a shot a few
seconds, so `-BrixTour=<list> -BrixTourOut=<folder>` (with `-BrixNoDesigner -BrixNoSave`, not `-BrixShot=`) goes through a
list of stops, sets the clock and the weather at each, waits `TourSeconds` and saves a picture: a stop is
`name|hh:mm|weather|where[|ev=<EV100>]`, where `cam:x,z,height,yaw,pitch`, `stand:x,z,up,yaw` or `in:<lot id>[,<pitch>]`
(the player inside that building, as `-BrixInside=`); `ev=` pins the exposure for a shot, to measure with.
`Tools/unreal/light_tour.txt` is the survey this light was fixed by (66 stops: the spawn alley, Market Square, a street,
a park and Eastgate from 07:20 through dusk, night and dawn, in rain and fog, seven kinds of room by day and by night,
and the reference rooms); `Tools/unreal/light_tour.ps1 -OutDir <folder>` runs it offscreen at High (add `-Extra "-BrixLightOff"` for the
light before), and `Tools/unreal/shot_sheet.py` puts shots side by side with their mean brightness. Run one at a time:
two at once hung the graphics chip.

**Light that changes, changes over a moment (found in play, 2026-11-25).** Switched in one frame, each of these was a
jump in the whole picture: a room's own light and its lamps as the camera crossed a wall (now up and down over
`LightFadeSeconds`, 0.4; a lamp is not moved while it is lit, and the one that casts shadows goes down, moves dark and
comes up at the place nearest the camera while the plain lamp there goes down); the haze, which is thin indoors
(`IndoorHazeSeconds`, 1); the street's lamps, where a pool jumped from post to post as the order of the twelve nearest
changed (a light now stays on its post while that post is among them, and the pool of a lamp near the edge of the set
is faded out over the last `FadeBand` of the way to the nearest lamp left out). Only the stronger of the sun and the
night's light casts shadows (`bOneSkyShadow`): through all of dusk both were lit and two shadows of the whole town were
drawn each frame. A tour stop logs the mean frames a second of the last two seconds of its wait and the longest of
those frames, not one frame's. Measured so, old against new in runs one after the other at High, 1066x666: at 19:30 to
20:20 out of doors 32 to 39 frames a second before, 39 to 47 after; by day and at night the same (other sessions were
using the laptop: a second pair of runs was slower and closer). Not yet: the eye still takes `IndoorSeconds` to open
and shut at a doorway, with the camera, not the player, deciding which side it is on; rooms are still furnished again
in one long frame (below, Furnished homes); breaking a street thing remakes its block square's mesh in one frame.

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
light, as the slice's twelve; 68 frames a second at High at 22:30. They and the lit windows come on as the sun gets
low, not only by the calendar; the night itself is under The eye and the night.

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
starts on, as in the slice. A lawn (its grass and its longer edge) has its own pattern, the slice's material 24: muted patches
of vegetation.json's three tones, the mower's faint stripes, clumps of clover, blades close up and daisies in drifts, painted
(`/Game/Greybox/M_Lawn`, `Tools/unreal/make_lawn_material.py` and `lawn.hlsl`, a section of its own in each block's facet
mesh; `CITY017_lawns_are_drawn_in_their_own_pattern`). Shots: `lawn.png` against `lawn_before.png` (one even green) in
`Art/StyleTests/unreal_gaps_world_40/`, beside the slice's `lawn_in_the_slice.png`; 10.0 frames a second at High in that garden, 7.7 without M_Lawn (noisy: other
sessions were running Unreal). The new shapes are kept out
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

**Music that dropped (found in play, 2026-11-25).** A tune fading out keeps the distance and the muffling it was heard
with: cleared each frame, a cafe's music at the edge of its reach came back clear and at full strength for the seconds
of its fade each time the cafe was left behind. The score goes under a place's music by how much of that is heard now
(its level, its distance, the wall between), not by how near its door is: a muffled cafe that had hardly begun took the
score down to a tenth, so driving past one was a hole in the music (the slice ducks by the door's distance; this is the
Unreal build's own). A place's level and muffling ease over 0.2 s at a doorway, as the slice's do, and music is never
the voice given up to a louder sound. Not yet: checked by ear by nobody (agents cannot hear); the voices in use at
dusk in traffic have not been counted against the engine's 32.

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
`UI021_every_thing_shows_what_python_says`). A mall good for the home shows its own model, made in Blender (FURN-065;
buymenu.js modelMesh(): `FBrixBuyMenuRules::ModelMesh`, its two open colours by the piece, each corner lit by its own
normal), checked for all 49 by the same test; the stand-ins (a parcel in the seller's band, a paper bag, a cup, a case, folded
cloth in the clothing's colour) are the boxes of World/rules/buy_menu.json, drawn by `SBrixThing` with Slate's own
triangles, lit as the slice's shader lights them. At the market square B opens the player's stall: rent it for the day,
set out up to six things at the player's own prices, take them back; residents coming to the market buy as the rules say,
paid through the ledger, and what is left comes home at midnight. Eastgate Mall's outlets each have their own counter (see
Eastgate Mall). Each cell's dot and the chip beside "Goes to" carry the mark of where the thing goes (a house, a top, a bag,
a cup, a chair), as in the slice: build_play.py reads the marks from buymenu.js ICON into life.json, `BrixIconCoverage`
paints each once from its paths (filled by the nonzero rule, the handles stroked) and the dot or chip draws it in its board
colour's lettering (`UI020_dots_and_chips_show_the_slices_marks`). The narrow layout D is the building designer's
list of pieces (above), where a piece of furniture shows the kit's own mesh. Not yet: a resident behind each counter. To see one in a test shot:
`-BrixCounter=grocer "-BrixLife=buy,grocer,bread"` (a shop's kind or lot id), or `-BrixCounter=market
"-BrixLife=buy,grocer,jam;rent;setout,jam,250"`.

## Quests, jobs and rewards (QST-001 to QST-007, QST-009, SND-019 to SND-021; ADR-0012, ADR-0043)
A career's story lines (the Baker career and courting Mei, QST-010 to QST-015, ADR-0056) run on the same rules:
`BrixQuests` keeps what the player did as facts (`Flags`, in the save), asks about them in needs (`flag:`, `rep:` with a
named resident, `done:` with endings, `home:owned` asked of the estate, `again:`, each also as "not"), starts a line with
no offer when its giver is spoken to, opens a stage on the first line whose needs hold (`FirstLine`), pays and charges
in a reply through the ledger, and lets a line that ended badly be taken again, clean (`Retakes`). The data is
`World/rules/quests_story.json`, made from the writing in `docs/narrative/src` (`Tools/citysim/story.py`).
`QST010_story_lines_run_as_the_python_does` replays a recorded game of the whole career and the courting (456 steps,
`build_play.py story_run()`: every reply open at every line, every act) and compares what is left with Python's: the
endings, the journal, facts, reputation, the ledger and things carried. In a talk a line somebody else says has their
name before it, and a stage direction is shown in brackets above the line.
A scene (a stage at a place with nobody to talk to: the count on Market Square in The Four O'Clock Door, the basket in
Twenty Minutes) opens the talk box by itself when the player is there (`ABrixLife::PlayScene`, `ABrixTalk::OpenScene`):
its lines are read, not spoken, and one left unanswered opens again when the player has gone and come back. Seen in the
running game: `docs/narrative/shots/stone_oven.png` (the career starts by talking to Noor, the baker) and `count_scene.png`
(`-BrixTime=06:40 -BrixStand=912,816,0 -BrixLife=act,new_in_town,r0504,end:gift;act,four_oclock_door,r0262,start|give:breakfast_loaf|stage:count`).
The Stone Oven's ground floor is fitted out as its approved design (ADR-0059, ADR-0062): `World/rules/fitouts.json`, named
by the lot (`"fitout": "stone_oven"`), closes a room of 9 by 7.5 m in the building's street corner with partitions (the owner, 2026-11-26: "make the bakery
room smaller with a partition wall", then "move the shop to a corner of the building, so it can be seen through windows
while people walk on the street outside"): its outer walls are the shop front on Baker Street and the side wall's three
arched windows on the cross street, the shop's door is moved to the corner, the stair to the building's east end, and a
doorway in the partition leads on to the stair. In it stand the design's own pieces: the plank door to the alley at the
back on the left, the oven beside it, the racks with the Mother's crock at the back on the right, the proofing room's
door and a second rack on the partition, the price board and the flour sacks under the side windows, the kneading bench
before the oven, the counter at the front on the right, and kit 18's three-shade pendants (one light each), with the
oven's fire and the shades lit. The four staff and the customers' places stand in the room (moved in
`World/city_01/blocks/b_7_7.json`). Every enterable shop's front panes are now clear glass, as a hollow storey's windows
are, so a shop is seen into from the street (they were opaque orange from both sides). None of the grocer's shelves, tills and fridges (`BrixFurnishFloor`, meshes made by
`Tools/blender/unreal_bakery_50.py` and imported into `/Game/Furniture`; `CITY013_the_stone_oven_is_fitted_out_by_its_design`).
Noor (r0262) wears her approved look (`World/rules/cast_looks.json`, `BrixApplyCastLooks`): an olive dress to the shin with
short sleeves, a cream bib apron, bare legs and brown shoes, grey hair in a bun on the crown with locks swept back along the
hairline, and age (creases at the brow, the eyes and from nose to mouth, paler lips, fainter cheeks), drawn only for a cast
look, so everyone else is as before (`QST014_noor_wears_her_approved_look`; ART004 unchanged). Looked at beside the
references on 2026-11-26: `Art/StyleTests/unreal_bakery_50/` (look_check: 10 pictures, 0 fail).
Not yet, of the Stone Oven and Noor (the look gate): the room has no ceiling beams and is brighter and flatter than the
reference, its corners not darker (G1); from the street by day the room reads dim behind its glass, as the eye is set for
the street; after dusk the side windows show the town's lit night panes, which glow and hide the room (the corner at 20:00
burns 5 % of the picture with them and the low sun on the gable); the shop front has the grocer's crates of produce on
the pavement, not bread; the oven's fire is a few lit boxes,
small beside the reference's; the pendants' shades hang at 1.7 m, where a tall player's head passes through them (no
collision); the ceiling carries the engine's blotches (G3, as in every shop); the rest of the shop behind the partitions
is empty and unlit, and the bakery's upper floors are still furnished as a grocer's store and rooms; in a talk the camera
can come close behind a resident's head in so small a room. Noor has no stoop, her lines are fainter than the reference's, and under the locks her hair is still the shell.
Nobody else has age values or a look of their own. After the clear shop fronts the look tour (66 stops) moved only at `street_2000`, whose crushed blacks went from 0.18 to
0.12 with no shop front in view: not explained, the baseline not rewritten; its fog, snow and Eastgate-from-above stops
fail the light-and-shade rule as they did at the baseline.
Everything left of the Baker career is listed in `docs/narrative/README.md`.
Not yet: in a scene the camera comes in as close as in a talk, so the player's shoulder fills a side of the picture;
lines said by somebody else are murmured in the voice of the person spoken to; none of the story is voiced; the log
does not show a career's rank; talks happen wherever the person's day has put them; the Stone Oven Bakery (b_7_7_l2) has two more staff than the story's
Noor and Pip (ADR-0056), and Noor's day puts her at work from 08:00, not from four as the story has it; the story has been played through in the game only
as far as those two pictures, the rest by test.
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
`buy_lot,<lot id>`). Shots: `Art/StyleTests/unreal_estate_38/`. The slice's own estate.js is checked against the same
Python run by `Prototype/play/estate_selftest.html` (headless Edge with `--dump-dom` prints PASSED or what differs). Not yet: the residents who moved house walking to their new
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
big map (see The real estate market). Each tree is drawn on the maps as big as its crown is today and a fallen one not at all
(play.js drawBase() over D.trees: `BrixMapTreesToday`, `UI018_the_maps_trees_are_todays_crowns`; `TreeRadius` only when
today's trees did not load). 31 to 33 frames a second at High with the minimap (noisy: other sessions were running
Unreal).

## Skateboards (CITY-098 to CITY-101; F-VEH-SKATES 2)
The slice's boards (boards.js, World/rules/boards.json): `BrixWorld/BrixBoards` is boards.js (the kinds, which are owned,
the one in use, carried or ridden, the speed: W speeds up to the kind's top speed, letting go glides, S brakes, indoors no
faster than 8 km/h, 30 in a mall, a knock loses the speed into it), and `BrixWorld/BrixBody` draws them as people.js
board() and ride() do: the board in use carried by its edge in the right hand, wheels out; on it the rider stands side-on
across the deck, knees a little bent, and on the manual board pushes with the back foot on the ground while W is held,
the front foot along the deck. Y (settings.json `board`) gets on the board in use and off it again, picking it up; on a
board there is no walk: A and D turn the rider and the camera comes round behind them, more slowly the faster they go;
the bar at the foot of the screen says which board and how fast. A new game has no board until it buys one at the skate
store; a test shot (or `-BrixBoards`) owns all three with the electric board in use, as the slice's `?auto=1` (unless
`-BrixNoBoards`, a new game's none), and
`-BrixBoard=<manual|electric|neon_boost>` gets on that one (with `-BrixHold=steer,throttle`). `Tools/unreal/people_values.py`
runs boards.js and people.js in headless Edge into `Tests/values/boards_city_01.json`: `CITY099_boards_are_ridden_and_drawn_as_the_slice_does`
checks every speed of a ride of each kind and 36 riders and carried boards (vertex for vertex, within 1 mm);
`CITY098_board_kinds_and_speeds`, `CITY100_auto_owns_all_and_rides_electric`.

Skateboards are sold at Eastgate's sports and toy outlets (CITY-102, CITY-103; ADR-0054, which supersedes ADR-0053's skate
store; the owner, 2026-11-25: no dedicated skate shop, rideable boards in the sports or toy shops, to ride or to put on show).
The slice sells none, so the rules were proved first in Python (economy.py sells_boards(), buy_board(n, u, kind),
board_in_use(), `test_CITY_102_*` and `test_CITY_103_*` in Tools/citysim/test_economy.py). Which outlets is boards.json
`sold_at` (sports, toys: 34 of the 189); the prices are boards.json `price`, cheapest to dearest (Bx 60, Bx 280, Bx 900
[ASSUMPTION]). B in such an outlet opens its counter with the three boards after its goods in the same grid, each its own
model as people.js board() draws it (`DrawBrixBoard`, `BrixThingFromBody`), turning in the showcase with its top speed and how
it goes; one owned is marked yours and is not sold again. A board is paid into the mall's till through the ledger (cash first,
then the current account), goes to the belongings and is the one in use when it is faster than the one in use. The game
menu's Belongings page ("What you own and carry") lists the boards owned, the one in use marked (in the hand or ridden), with
Use this on the others (life.js belongingsTab()); with no goods carried it says so without "you carry nothing" over the
boards and furniture listed below it (it did, 2026-11-26). `CITY103_boards_are_bought_as_python_does` runs economy.board_script() (refusals, a clothes outlet,
a loan, the in-use rule, the till, a save and load) against Python's results in life.json. To see it in a test shot:
`-BrixNoBoards -BrixCounter=b_16_5_u35` (Bobbin's Toys; `"-BrixLife=open,current;loan,30000,7"` to afford a board), the
belongings with `-BrixMenu=belongings`. Not yet: boards in the sports and toy outlets' windows, a board on show at home (the boards are not among the Furnish tab's pieces yet), the server's check
(CITY-104), the sounds of getting on and off; the slice has no residents on boards, so there are none here either. 32
frames a second at High riding along Tram Street (noisy: other sessions were running Unreal).

## The camera (found in play, 2026-11-25)
Behind the player on an arm of `CameraDistance` (4 m) that shortens where something is in its way. In a room it comes in
to `IndoorCameraDistance` (2.8): at 4 m it met a wall or the ceiling at almost every turn and sat jammed in the
player's back. The body is not drawn while the camera is within `HideBodyWithin` (0.55 m) of it. A hung piece (a pendant's
shade, a wall cupboard, hooks, a mirror) is not drawn while the camera is within `HideHungWithin` (1 m) of it, and stops
the camera as the slice's camera-only solids do (homes.js CAM_ONLY; `ABrixRooms::HideHungNearCamera`): in the houses' and
row houses' halls a pendant's shade beside the camera filled half the picture (found in play, 2026-11-26). In a car, on a bus or a
tram and on a board the camera comes round behind, but the mouse has it for `LookHoldSeconds` (1.5) first: pulled back
every frame, it could not be turned while moving. Behind a car, a bus or a tram the arm is the character's own work
(`RideCamera`): shortened by the engine it met every lamp post and corner passed, and the camera jumped in and out; now
it comes in only for what has stood in its way for `RideBlockSeconds` (0.15: a wall, not a post passed), quickly, and
goes out again slowly. Riding a bus or a tram it is 15 or 17 m back and nearly level, behind the vehicle (11 m behind a
seat at the front of an 11 m bus, it sat over the roof). The cars are drawn after the player has moved in a frame: drawn
before, the player's car was a frame behind its driver and shook under them. E does nothing while a counter, the menu or
the map is open (it started a drive or a talk under the panel, with the camera held). One of the map, the menu and a counter is open at a time (they share the
player's "a panel is open": a page or a counter opened over the map closes the map first, and a counter opened from the
building designer gives the designer's camera back first), and closing one gives the mouse back to the camera without a
click. Getting out of a car, the spot must be free and have nothing between it and the car (a spot free on the far side
of a thin wall put the player inside a building). Not yet: a tree's crown does not stop the camera, which behind a bus
can pass through the bottom of one; with no free spot at all the player still gets out at the driver's door, into
whatever is there; none of this has been played by hand.

## Light and speed (measured 2026-11-24 on the Intel integrated graphics of the owner's laptop, 1066x666)
Lumen (bounced light, the sky's fill in shade) needs shader model 6 (`Config/DefaultEngine.ini`); without it the shade is
black. At Epic quality the town runs at about 36 frames a second, at High about 63 with the same shade, at Medium about 86
but without Lumen. On this laptop use High (`scalability 2` in the console, or `-ExecCmds="scalability 2"`). The light
follows the kit daylight (ADR-0007): a shaded wall on a wide street measures (161, 151, 135) against (159, 151, 146) in
`Art/Reference/world_street_kit_light.png`. The first run after a change of shader settings compiles shaders for minutes
and runs at a few frames a second; that is not the game's speed.

## The look gate (ADR-0058)
Anything seen in this build is looked at as a whole, against `Art/Reference/` and ART_DIRECTION's rules (not the
browser slice), by whoever makes it, before its pull request: `docs/process/LOOK_GATE.md`. Shots at noon, dusk and
night beside the references; ten questions about the whole frame (G1 to G10: shade, shadows, surfaces, colour, whites
and blacks, people, lamps, wholeness, the hours, nothing over the picture); `Tools/unreal/look_check.py` for the
numbers (burnt whites, crushed blacks, flat fills, light and shade, warmth, the player against the picture round them);
for a shared change the look tour against `Tools/unreal/look_baseline.json`, which says which stops have moved. The
first audit's findings are in LOOK_GATE.md.

**Mended (2026-11-26, shots in `Art/StyleTests/unreal_look_gate_49/second_batch.jpg`):**
- *Tree crowns near black with bright lines on their edges*: every face of a crown was turned to its hint side
  without its points being taken the other way round, and the two-sided material then lit it as if from inside
  (`ABrixTownActor::Facet()`, `DomeGroup()`). Crowns are the sage, sun-shaded shapes of the references.
- *A window seen from inside at night was a white pane*: the lit pane was a thin box with a face towards the room; it
  is one face looking out (`NightPaneGroup()`), so from inside a window shows the dark street.
- *The player nearly black in a tower's hall*: the room's own light hangs at the floor's middle, so a body between it
  and the camera showed its unlit side. People have that light again from the camera's side (`FillPeople`,
  `PeopleFill` 0.7, lighting channel 1); the rooms' own measured light is unchanged.
- *A teal cast over rain, snow and fog*: the covered sky's haze tint was (0.9, 0.95, 1.05); it is a neutral grey a
  little darker (`OvercastHaze` 0.80, 0.815, 0.83).
- *The spawn alley's far end a white-out by day*: by day out of doors the eye was free, so in a shaded alley it opened
  until the sunlit square beyond burnt. It may now open `DayOpen` 2.2 stops past the street's exposure: the open
  street, the square and the park do not open that far and are as they were (the tour's numbers did not move there);
  the alley reads as shade with its far end in sight. This narrows ADR-0055's "by day the eye is free".

**Pending, by the gate's questions (nothing here has been mended):**
- **Shops, the bank, the library, cafes (G3):** the engine's dark blotches on ceilings and floors. Tried and left:
  finer bounce settings (no visible change, and 19 to 30 frames a second where there were 32 to 40), and lamps whose
  light is not bounced (`RoomLampBounce` 0: no change). Not yet tried: the furniture out of the engine's bounce scene.
- **Homes (G3, G5):** plain flat walls with no mottle; the lamp nearest a wall burns a pale patch on it.
- **A tower's hall (G3, G7):** bare plain walls, one lamp far off; dim.
- **Eastgate and the furniture outlet inside (G1, G2, G3, G6):** see Eastgate's own "Not yet"; the outlet's hall has
  not been looked at since the hall's light was changed.
- **Lawns, the mall's outer shell, open ground (G3):** the lawn's pattern is there but pale in the sun; the shell is
  drawn in the plain facets material, which has no mottle.
- **Snow (G4):** the ground under snow still takes the sun's warm colour, so lying snow is a yellowish grey.
- **A cafe at night (G7):** two panes on its window wall still glow from inside; not found out whether they are its
  own or a neighbour's seen through the glass.
- **The spawn alley in rain (G6):** the player is a dark shape against the grey haze (wet clothes, in shade).
- **Riding a bus (G8):** the bottom of a tree's crown can be in the picture.
- **The look tour's baseline:** written on 2026-11-26 before the new street lamps (WLD-072) came in; only
  `street_2230` was taken again after them, so other night stops may report a move that is those lamps.
- **Not looked at with the gate at all:** the upper floors of Eastgate and its shops' insides, the tram, playgrounds,
  gardens, the designed houses' rooms, the building designer's views, the menus and counters, any place in rain or
  snow other than Market Square.

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
| `BrixTown.World.CITY067_riders_show_and_fit_in_the_cars` | CITY-067 (ADR-0057) |
| `BrixTown.World.CITY070_residents_dodge_and_scold_as_the_slice_does` | CITY-070, CITY-071 |
| `BrixTown.World.CITY072_street_things_break_as_the_rules_say` | CITY-072, CITY-073 |
| `BrixTown.World.CITY072_a_broken_bench_is_not_sat_on` | CITY-072 |
| `BrixTown.World.CITY074_damage_is_kept` | CITY-074 |
| `BrixTown.World.CITY075_repair_crews_mend_as_repairs_py_does` | CITY-075 |
| `BrixTown.World.CITY076_residents_shout_at_a_smash` | CITY-076 |
| `BrixTown.World.WLD037_the_weather_is_worked_out_as_world_py_does` | WLD-037 |
| `BrixTown.World.WLD037_the_weather_key_steps_through_the_kinds` | WLD-037 |
| `BrixTown.World.WLD037_people_out_in_the_rain_are_a_little_darker` | WLD-037 |
| `BrixTown.World.WLD037_rain_and_snow_stay_out_from_under_roofs` | WLD-037, WLD-070 |
| `BrixTown.World.CITY080_residents_are_dressed_for_the_rain` | CITY-077, CITY-079, CITY-080 |
| `BrixTown.World.WLD036_the_clouds_are_where_world_py_puts_them` | WLD-035, WLD-036 |
| `BrixTown.World.WLD070_snow_falls_and_lies_as_world_py_says` | WLD-069, WLD-070 |
| `BrixTown.World.WLD042_the_days_light_is_kept_and_the_night_is_dark` | WLD-042 (ADR-0055) |
| `BrixTown.World.CITY107_residents_are_dressed_for_snow` | CITY-107 |
| `BrixTown.World.WLD071_footprints_and_tracks_follow_the_clock` | WLD-071 |
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
| `BrixTown.World.SND018_the_cast_speak_their_story_lines` | SND-018, QST-012 |
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
| `BrixTown.World.UI021_every_thing_shows_what_python_says` | UI-020, UI-021, FURN-065 |
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
| `BrixTown.World.UI018_the_maps_trees_are_todays_crowns` | UI-018, WLD-056 |
| `BrixTown.World.UI012_the_mark_is_set_cleared_and_reached` | UI-012 |
| `BrixTown.World.QST005_the_people_and_places_a_quest_wants_are_marked` | QST-005 |
| `BrixTown.World.QST005_the_markers_are_sound` | QST-005 |
| `BrixTown.World.QST005_each_quest_step_gets_its_marker` | QST-005 |
| `BrixTown.World.CITY098_board_kinds_and_speeds` | CITY-098 |
| `BrixTown.World.CITY099_boards_are_ridden_and_drawn_as_the_slice_does` | CITY-099, CITY-101 |
| `BrixTown.World.CITY100_auto_owns_all_and_rides_electric` | CITY-100 |
| `BrixTown.World.CITY103_boards_are_bought_as_python_does` | CITY-100, CITY-102, CITY-103 |
| `BrixTown.World.CITY017_every_plant_is_drawn_as_the_slice_draws_it` | CITY-017, WLD-040 |
| `BrixTown.World.CITY017_lawns_are_drawn_in_their_own_pattern` | CITY-017, ART-001 |
| `BrixTown.World.WLD056_the_open_ground_is_drawn_as_the_slice_draws_it` | WLD-056, ART-001 |
| `BrixTown.World.WLD032_children_play_as_the_slice_puts_them` | WLD-032, WLD-041 |
| `BrixTown.World.WLD028_the_street_dressing_is_drawn_as_the_slice_draws_it` | WLD-028, WLD-031, WLD-034 |
| `BrixTown.World.WLD056_every_tree_is_drawn_as_the_slice_draws_it` | WLD-056 |
| `BrixTown.World.CITY043_the_road_markings_and_signals_are_drawn_as_the_slice_draws_them` | CITY-043, CITY-045, CITY-091 |
| `BrixTown.World.WLD058_eastgate_is_built_as_the_slice_builds_it` | WLD-057 to WLD-061, WLD-063, WLD-067, WLD-068 |
| `BrixTown.World.FURN065_outlets_show_the_goods_they_sell` | FURN-065 |
| `BrixTown.World.WLD058_only_eastgate_has_an_inside` | WLD-058 |
| `BrixTown.World.WLD062_moving_stairs_carry_and_lifts_go_between_floors` | WLD-062 |
| `BrixTown.World.WLD066_every_outlet_has_its_room` | WLD-066 |
| `BrixTown.World.CITY105_every_outlet_has_a_counter_its_keeper_stands_at` | CITY-105, WLD-067 |
| `BrixTown.World.CITY106_visitors_shop_at_the_counters_and_eat_at_the_tables` | CITY-106, WLD-068 |
| `BrixTown.World.BUILD002_every_building_is_built_from_its_design` | BUILD-001, BUILD-002, BUILD-016, BUILD-019 (ADR-0052) |
| `BrixTown.World.BUILD005_remodels_match_the_python` | BUILD-004 to BUILD-007, BUILD-016 to BUILD-019 |
| `BrixTown.World.QST008_remodels_are_kept_in_the_save` | QST-008, BUILD-005 |
| `BrixTown.World.BUILD021_room_plans_match_the_python` | BUILD-021 to BUILD-023 |
| `BrixTown.World.BUILD021_a_dragged_wall_stays_in_the_room_and_catches_on_walls` | BUILD-021 (BUILD-020) |
| `BrixTown.World.QST008_room_plans_are_kept_in_the_save` | QST-008, BUILD-021 |
| `BrixTown.World.FURN051_furnishing_matches_the_python` | FURN-051, FURN-052, FURN-055, FURN-064, INV-001 |
| `BrixTown.World.FURN055_a_piece_is_picked_and_dragged_on_the_grid` | FURN-055 (BUILD-020) |
| `BrixTown.World.QST008_furnishings_are_kept_in_the_save` | QST-008, FURN-051, INV-001 |
| `BrixTown.World.PROP021_the_market_matches_the_python` | PROP-010 to PROP-022, BANK-002, FURN-063 |
| `BrixTown.World.PROP009_every_building_is_carved_into_owned_units` | PROP-009, PROP-010 |
| `BrixTown.World.PROP018_the_money_supply_holds_and_a_saved_market_goes_on` | PROP-018, PROP-021, QST-008 |
| `BrixTown.World.FURN020_the_outlet_is_built_as_the_slice_builds_it` | FURN-020 |
| `BrixTown.World.FURN021_staff_and_browsers_are_at_the_outlets_places` | FURN-021 |
| `BrixTown.World.FURN025_the_store_matches_the_python` | FURN-022 to FURN-029, FURN-024, QST-008 |
| `BrixTown.World.FURN022_the_outlets_sofas_chairs_and_beds_can_be_tried` | FURN-022 (sitting and lying), FURN-020 (the vans) |
