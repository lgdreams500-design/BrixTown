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

## Going inside (CITY-013; ADR-0048)
959 of the 965 enterable buildings can be walked into (all but the furniture outlet's hall and the malls), as the slice
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
stand. Talking faces and boards are not brought over yet; the gesture layers (CHARACTER_MOTION §4), another walk, a
hood and an umbrella are drawn as people.js draws them (checked by ART004), for dodging and the weather. The player is the slice's player too (people.js
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
the time it took until they next stand still where they would be anyway. The quests, jobs and counters that the slice's life.js adds to talks are below. Not yet: the player's typed name
and the wordless murmur of lines with no voice.
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
a street thing or a tree in the way breaks (CITY-072); a wall or another car stops it and it rolls back a little; the camera swings round behind; E gets out by
the driver's door, and the car stays where it was left for the rest of the visit. E offers whichever is nearer, a car to
drive or someone to talk to. A bus can be ridden (CITY-092): E at a bus
standing at a stop gets on, a seat near the front, carried along its line with its driver and riders, the camera behind;
E gets off when it next stands at a stop (and everyone gets off when it leaves service). Not yet: riding the tram. To see a drive in a test shot: `-BrixDrive=<vehicle> "-BrixHold=steer,throttle"`. 51.5 frames a second at High on a street
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
overhead and none while the camera is indoors; one procedural mesh of translucent drops (`/Game/Weather/M_Rain`, made by
`Tools/unreal/make_rain_material.py`). The paving gets wet: it darkens, takes a faint sheen and holds puddles here and
there, and walls go a little darker; it dries over `dry_minutes` after the rain (M_Greybox's WetDark, WetSheen and
Puddles, from `Tools/unreal/greybox_wet.py`). Residents wear what citysim gives them today (day.json "wear"): a coat,
and out in the rain an umbrella up, else the hood up, else a hand over the head while walking, with a hurried walk or a
run for cover; nothing but the coat indoors, sheltering or under a roof (`CITY080_residents_are_dressed_for_the_rain`).
The look of coats, hoods and umbrellas is still to be approved (ADR-0020). `-BrixWeather=<clear|cloud|rain|fog>`
tries one kind for the whole day (and the clothes residents would wear on a day of it); the built day 16 has cloud
before 06:00 and is clear after. At High: 40 to 43 frames a second in the 08:10 school rush in rain, against 15 to 54
in clear weather on the same street (noisy: other sessions were running Unreal on the same laptop). Not yet: no key to
try the weather, rain seen through windows from indoors, ripples, the rain's sound, wet clothes.

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
market stall, worked out from the residents' visits and the player's own trades, never ticked. The data is `Game/Data/life.json`, written by `Tools/worldgen/build_play.py` in the same run as the slice's
city_data.js (economy.json, mall_goods.json and quests.json, the day's shop visits, who works where), with two fixed runs
and what Python gives for them: `ECO002_the_ledger_matches_the_python` runs economy.check_script() and its outlet ops,
`BANK008_the_bank_settles_as_the_python_does` a loan that defaults; both must give Python's ledger, balances, things
carried, loans, prices and outlet stock to the chip. A new game starts when play starts (the character designer is not
brought over yet): the starting cash is paid in, and the economy's days count from then; each frame it is brought up to
the clock, so interest, instalments and fees are settled for each midnight passed (BANK-008). On screen, as in the slice:
the purse at the top left (cash, and the bank's two accounts together once one is open) with a "+" or "-" beside it when
it changes; at the bank's door (`CounterReach`) or inside it, B opens its counter in the slice's dark panel: open
accounts, move money (a payment that would take an account below zero is refused, never partly made), borrow and repay,
and the statement, newest first; I opens the money page (purse, loan, statement). Esc closes either. Not yet: saving,
the game menu's Money page in its painted-sign look. To see the bank in a test shot:
`-BrixCounter=bank "-BrixLife=open,current;open,savings;move,p:current,p:savings,1000;loan,5000,3"` (ops as
economy.check_script() writes them, arguments by commas).

## Sound (SND-001 to SND-004, SND-007, SND-009 to SND-013, SND-017, SND-019 to SND-021)
The slice makes its sounds live with Web Audio (`Prototype/play/sound.js`, from `World/rules/sound.json`); the Unreal
build plays recordings of those same sounds. `Tools/unreal/sound_values.py` runs sound.js itself in headless Edge (over
its DevTools socket) and renders its own sound-making functions into an OfflineAudioContext: `Game/Data/sounds/*.wav`
(mono, 16-bit, 22,050 Hz; 98 sounds, 2.2 MB, in Git LFS; the music and the rewards as Ogg Vorbis, made by the engine's own encoder through the `BrixEncodeOgg` commandlet, so build the editor before running the script) and `index.json` (each one's length, whether it loops, its
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
starter) and out of it (the door). N turns all sound off and on, the talk's voices too (SND-013); it is remembered
between visits in the user's settings (`Saved/Config/.../GameUserSettings.ini`, `[/Script/BrixTown.BrixSound]
bSoundOn`). A test shot logs how many of each sound were played and how long each loop was heard (`sounds played in
all: ...`, `loops heard in all: ...`); `-BrixSoundToggle` turns the sound off and on again during it.
Indoors (SND-007): while the player is in a building (the game mode's RoomAt) what is outside (the street, crickets,
fountains, engines, the trams, birds) comes through a 650 Hz low-pass at 0.4 of its level, the room has its tone and steps
are on wood. Money coming in (SND-019 to SND-021): the quest and bank widgets' moments (`ABrixLife::OnRewardSound`)
play sound.js reward()'s recordings, each coin taken a note higher (`SND019_coins_ring_as_the_slice_rings_them`).
Music (SND-010 to SND-012): eight bars of each of the score's four moods and each style (café, shop, busker, the radio's
swing, pop and calm), recorded at the music bus's level, each a loop eased to its level as the slice's bands fade: the
score by the time of day, under a talk and under nearer music; the nearest open café or shop and the market's busker on
market days, from its door within its reach, muffled from the street and clear inside; the car radio's music stations,
R while driving (`-BrixRadio=<k>` in a test shot). The music is eight bars of the slice's endless band, so it repeats;
the podcasts (SND-014) and the radio's caption are not brought over yet. The slice has no sound for the weather, so there
is none here either.

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
paid through the ledger, and what is left comes home at midnight. Not yet: a mall's outlets (the rules are ported; their
counters are inside the mall), the narrow layout D (for the building designer), the icons in the dots and chips, a
good's own Blender model in place of its stand-in, and a resident behind each counter. To see one in a test shot:
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
acts are the quest's ({player} is "you" until the character designer comes over); an answer that opens a shop opens it
when the talk ends. Goals are checked four times a second where the player is (in a lot, or at its door). J opens the quest
log: under way with where the person to see is now (from their routine), on offer, finished, people you know. The lines
under way show at the right of the screen, a job with its time left. Money coming in is shown as the owner chose
(`BrixWorld/BrixRewards`, world.reward_events(), checked by `SND019_money_coming_in_is_shown_as_python_says`): a reward's
coins spring out round the player, hover, are drawn in and fly to the purse, which counts up as they land; interest
comes with a slip under the purse and its coins fly up into the bank balance. Achievements (QST-009) are earned from
facts worked out from the ledger and the quests (`QST009_achievements_are_earned_as_python_says`), each once, its painted
banner dropping in at the top, one at a time and not over a panel. Their sounds are played by `ABrixSound` (see Sound): `ABrixLife::OnRewardSound`
is broadcast with sound.js's names (quest_start, coin_out, coin_land, coin_take, quest_end, interest_start,
interest_coin, interest_end, achievement). Not yet: saving (QST-008), markers over people's heads and on the map, the
Me page's list of achievements, the stamped coin face. To see a reward in a test shot: `-BrixLifeLead=1.15
"-BrixLife=act,new_in_town,r0504,stage:bank;open,current;check;buy,grocer,bread;check;act,new_in_town,r0504,take:bread|end:gift"`
(the ops that many seconds before the shot); to see a quest talk, `-BrixTime=15:12 -BrixTalk=504 "-BrixAnswers=1,1"`.

## The game menu and settings (UI-005 to UI-016, UI-019; F-UI-MENU 2, F-UI-SETTINGS 1; ADR-0049)
Esc opens the game menu on its home (Esc again, or Back to town, closes it; Esc first closes a counter or a talk); C, I, G,
P, J, U, M and H open a page directly, as World/rules/settings.json binds them, and its key again goes back to town. While
it is open the clock stands still (the game is paused; `menu.pauses`). The pages are settings.json's, in its groups You,
Town and Game, each in its colour: Me (what you have, the achievements earned first with their day then those still to
earn, people you know), Money (accounts, statement), Belongings (what is carried, each drawn on its plinth), Journal (the
quest log), Settings (one group at a time), Save, Help (the keyboard's controls by where they work, from settings.json);
Properties, Wardrobe, Storage and Map say what they wait for (the property market, the character designer,
F-PROP-STORAGE, the big map), and Save shows its three slots but cannot save yet (QST-008). The settings are
`BrixWorld/BrixSettings`: settings.json checked by world.settings_problems()'s rules (`UI009_settings_are_valid`,
`UI009_a_bad_setting_is_caught`, `UI008_every_platform_maps_every_action`, `UI010_no_control_does_two_things`,
`UI005_every_tab_opens_from_a_control_on_every_platform`), the player's values kept on the device in
`Saved/BrixTown/settings.json`, not in a save, and taking effect at once: sun shadows, ambient occlusion, bounce light,
resolution and the Picture choice that sets them together, field of view, the frame counter, sound on or off and its
level, text size, look speed, invert and camera distance. Names over residents, the minimap, first person and the
separate sound levels are kept for when those come. The menu, the counters and the buy menu are in "Lamplight"
(ADR-0049, proposed on the owner's word, `BrixLook.h`): cards of warm dark stone with round corners and a thick lower edge,
the town dimmed behind, each page's colour as its lintel and mark, lantern-orange main buttons, prices on paper tags, the
goods on plinths in a pool of lantern light. Shots: `Art/StyleTests/lamplight_menu_36/`. To see a page in a test shot:
`-BrixMenu=<home|me|wallet|quests|belongings|settings|help|...>` (and `-BrixSettingsGroup=<n>`); the shot is taken by the
menu itself, since the paused game's timers stop.

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
| `BrixTown.World.CITY013_every_enterable_building_has_a_doorway` | CITY-013 (ADR-0048) |
| `BrixTown.World.CITY017_residents_sit_on_garden_benches` | CITY-017 |
| `BrixTown.World.CITY040_vehicles_are_where_the_traffic_puts_them` | CITY-040 |
| `BrixTown.World.CITY041_trams_are_where_the_timetable_puts_them` | CITY-041 |
| `BrixTown.World.CITY049_resident_cars_give_way_to_the_player` | CITY-049 |
| `BrixTown.World.CITY070_residents_dodge_and_scold_as_the_slice_does` | CITY-070, CITY-071 |
| `BrixTown.World.CITY072_street_things_break_as_the_rules_say` | CITY-072, CITY-073 |
| `BrixTown.World.CITY074_damage_is_kept` | CITY-074 |
| `BrixTown.World.CITY075_repair_crews_mend_as_repairs_py_does` | CITY-075 |
| `BrixTown.World.CITY076_residents_shout_at_a_smash` | CITY-076 |
| `BrixTown.World.WLD037_the_weather_is_worked_out_as_world_py_does` | WLD-037 |
| `BrixTown.World.CITY080_residents_are_dressed_for_the_rain` | CITY-077, CITY-079, CITY-080 |
| `BrixTown.World.CITY007_every_conversation_is_sound` | CITY-007 |
| `BrixTown.World.CHR009_lines_are_mouthed_as_the_slice_mouths_them` | CHR-009 |
| `BrixTown.World.SND018_lines_are_voiced` | SND-018 |
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
| `BrixTown.World.SND001_the_town_sounds_by_time_of_day` | SND-001 |
| `BrixTown.World.SND007_outside_is_muffled_indoors` | SND-007 |
| `BrixTown.World.SND010_the_score_changes_through_the_day` | SND-010, SND-011, SND-012 |
| `BrixTown.World.SND019_coins_ring_as_the_slice_rings_them` | SND-019, SND-020 |
| `BrixTown.World.QST001_quests_run_as_the_python_does` | QST-001 to QST-007 |
| `BrixTown.World.QST009_achievements_are_earned_as_python_says` | QST-009 |
| `BrixTown.World.SND019_money_coming_in_is_shown_as_python_says` | SND-019, SND-020 |
| `BrixTown.World.UI005_every_tab_opens_from_a_control_on_every_platform` | UI-005, UI-013, UI-015 |
| `BrixTown.World.UI008_every_platform_maps_every_action` | UI-008 |
| `BrixTown.World.UI009_settings_are_valid` | UI-009 |
| `BrixTown.World.UI009_a_bad_setting_is_caught` | UI-009 |
| `BrixTown.World.UI010_no_control_does_two_things` | UI-010 |
