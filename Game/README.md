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
0.4 m high, at least 3.6 m), a slab for each block (green for parks and open ground), a trunk and crown for each tree,
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

## People (ART-004, ANIM-001, ANIM-002; ADR-0047)
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
stand. Talking faces, umbrellas and boards are not brought over yet. The player is the slice's player too (people.js
player(), in people.json) at full detail, walking by their signature and running by the slice's Run, until the designed
character (CHR-001) comes over. At High on this laptop: 53 to 55 frames a second on a street with 32 residents near, in
the morning school rush of some 150 and in a garden with 46 (the same as with none: the GPU is the limit), the people
taking 1.3 to 3.6 ms of the game thread.

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
the time it took until they next stand still where they would be anyway. Not yet: the quests, jobs and counters that
the slice's life.js adds to talks, the player's typed name, and the wordless murmur of lines with no voice.
To see one without anyone at the keys: `-BrixTalk=<resident index>` (and `-BrixAnswers=1,2`) with `-BrixShot=`.

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
a wall, a street thing or another car stops it and it rolls back a little; the camera swings round behind; E gets out by
the driver's door, and the car stays where it was left for the rest of the visit. E offers whichever is nearer, a car to
drive or someone to talk to. A bus can be ridden (CITY-092): E at a bus
standing at a stop gets on, a seat near the front, carried along its line with its driver and riders, the camera behind;
E gets off when it next stands at a stop (and everyone gets off when it leaves service). Not yet: breaking street things
(CITY-072, CITY-073), riding the tram, and resident cars
giving way to the player (CITY-049). To see a drive in a test shot: `-BrixDrive=<vehicle> "-BrixHold=steer,throttle"`. 51.5 frames a second at High on a street
with 32 vehicles drawn.

## The tram (CITY-041)
Trams run along Tram Street to the timetable of World/rules/calendar.json, stopping at its three stops: a port of the
slice's tramAt() (`FBrixTram`, checked against the slice's own function at 3,000 moments of a day by
`CITY041_trams_are_where_the_timetable_puts_them`). Each is the slice's terracotta car with open windows, on rails along
the whole street, and carries the residents whose day takes them out from a stop or back to one, on the run that is
there when they leave or arrive (cars.js tramRiders()), up to twelve on its benches. The player cannot ride it yet.

## Street furniture (WLD-028)
Each block's props are drawn as the slice draws them (play.js prop(), dressing.js), boxes in the palette's colours:
lamps, benches, bins, bollards, planters, postboxes, bike racks, phone boxes, wall lanterns, flowerbeds, fountains,
market stalls, tram shelters, water towers and wells. Garden planting, playgrounds, arches, adverts and the open ground's
own things are not brought over yet. By night (`ABrixLamps`) every lamp's and wall lantern's glass glows
(`/Game/Greybox/M_Glow`, made by `Tools/unreal/make_glow_material.py`) and the twelve nearest to the camera give real
light, as the slice's twelve; 68 frames a second at High at 22:30. The night itself (its exposure, the orange haze
after dusk) is not yet the slice's dark blue night.

## Money and the bank (ECO-001 to ECO-003, ECO-009, ECO-012, BANK-001, BANK-002, BANK-007, BANK-008; ADR-0012)
Money is the slice's (economy.js, a port of `Tools/citysim/economy.py`): `BrixWorld/BrixEconomy` holds one ledger of
balanced transactions in whole chips (int64), money made and taken only by the `sys:` accounts, the bank's vault and every
shop's till as ordinary accounts; the bank's current and savings accounts, moving money between them and the purse, loans
paid in daily instalments, late fees, default and repossession; and the shops' stock and prices, a mall's outlets and the
market stall, worked out from the residents' visits and the player's own trades, never ticked (these have no counter on
screen yet). The data is `Game/Data/life.json`, written by `Tools/worldgen/build_play.py` in the same run as the slice's
city_data.js (economy.json, mall_goods.json and quests.json, the day's shop visits, who works where), with two fixed runs
and what Python gives for them: `ECO002_the_ledger_matches_the_python` runs economy.check_script() and its outlet ops,
`BANK008_the_bank_settles_as_the_python_does` a loan that defaults; both must give Python's ledger, balances, things
carried, loans, prices and outlet stock to the chip. A new game starts when play starts (the character designer is not
brought over yet): the starting cash is paid in, and the economy's days count from then; each frame it is brought up to
the clock, so interest, instalments and fees are settled for each midnight passed (BANK-008). On screen, as in the slice:
the purse at the top left (cash, and the bank's two accounts together once one is open) with a "+" or "-" beside it when
it changes; at the bank's door (`CounterReach`) or inside it, B opens its counter in the slice's dark panel: open
accounts, move money (a payment that would take an account below zero is refused, never partly made), borrow and repay,
and the statement, newest first; I opens the money page (purse, loan, statement). Esc closes either. Not yet: shop and
stall counters, saving, the game menu's Money page in its painted-sign look. To see the bank in a test shot:
`-BrixCounter=bank "-BrixLife=open,current;open,savings;move,p:current,p:savings,1000;loan,5000,3"` (ops as
economy.check_script() writes them, arguments by commas).

## Sound (SND-002 to SND-004, SND-007, SND-009, SND-017)
The slice makes its sounds live with Web Audio (`Prototype/play/sound.js`, from `World/rules/sound.json`); the Unreal
build plays recordings of those same sounds. `Tools/unreal/sound_values.py` runs sound.js itself in headless Edge (over
its DevTools socket) and renders its own sound-making functions into an OfflineAudioContext: `Game/Data/sounds/*.wav`
(mono, 16-bit, 22,050 Hz, 62 sounds, 1.2 MB, in Git LFS) and `index.json` (each one's length, whether it loops, its
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
continuous sound is a loop that plays only while it can be heard (it is stopped, and takes no voice, when it cannot). A
test shot logs how many of each sound were played and how long each loop was heard (`sounds played in all: ...`,
`loops heard in all: ...`). Not yet: being muffled indoors.

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
| `BrixTown.World.ART004_people_are_drawn_as_the_slice_draws_them` | ART-004, ANIM-001, ANIM-002 |
| `BrixTown.World.CITY017_residents_sit_on_garden_benches` | CITY-017 |
| `BrixTown.World.CITY040_vehicles_are_where_the_traffic_puts_them` | CITY-040 |
| `BrixTown.World.CITY041_trams_are_where_the_timetable_puts_them` | CITY-041 |
| `BrixTown.World.CITY007_every_conversation_is_sound` | CITY-007 |
| `BrixTown.World.CHR009_lines_are_mouthed_as_the_slice_mouths_them` | CHR-009 |
| `BrixTown.World.SND018_lines_are_voiced` | SND-018 |
| `BrixTown.World.ECO001_money_is_whole_chips_in_balanced_lines` | ECO-001, ECO-002, ECO-003, BANK-007 |
| `BrixTown.World.ECO002_the_ledger_matches_the_python` | ECO-002, ECO-005, ECO-010, ECO-011, ECO-013, BANK-001, BANK-007 |
| `BrixTown.World.BANK008_the_bank_settles_as_the_python_does` | BANK-002, BANK-008 |
| `BrixTown.World.SND017_people_sound_as_the_slice_hears_them` | SND-017, SND-003 |
| `BrixTown.World.SND003_the_players_steps_and_landings` | SND-003 |
| `BrixTown.World.SND007_sounds_are_heard_from_where_they_are` | SND-007 |
| `BrixTown.World.SND009_every_sound_has_its_recording` | SND-009 |
| `BrixTown.World.SND002_engines_sound_as_the_slice_hears_them` | SND-002, SND-009 |
| `BrixTown.World.SND004_the_tram_sounds_as_the_slice_hears_it` | SND-004 |
