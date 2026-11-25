---
adr: ADR-0055
title: In Unreal the eye is held at dusk and at night, the lamps follow the sun, and the night has its own light
status: proposed          # built to fix the light as found on 2026-11-25; the night and dusk it gives are for the owner to approve (pictures: Art/StyleTests/unreal_light_42/)
date: 2026-11-25
relates_to: [WLD-028, WLD-037, WLD-042, WLD-043, CITY-013, ADR-0004, ADR-0007, ADR-0023, ADR-0024, ADR-0045, ADR-0048, ADR-0050]
---

## Context
The Unreal build's light was looked at in every kind of place through the day (59 shots in one run, taken again after each
change: `Tools/unreal/light_tour.txt`). By day it is the approved light (ADR-0007, ADR-0048). The rest was not:

| # | Where and when | What was wrong | Why |
|---|---|---|---|
| 1 | Everywhere out of doors, from dusk to dawn | The night was as bright as the day, a flat orange fog (`before_after_night.jpg`) | The engine's automatic exposure opens without limit as the light goes; the haze kept the low sun's orange at 15 % of the day's strength all night; with the sun out the sky was black, so there was no fill at all and the eye opened onto the haze alone |
| 2 | Everywhere out of doors, from about 19:30 to dusk and after dawn | An orange white-out: the player could hardly be seen 4 m away | The game mode turns the whole haze the low sun's colour at the day's full strength while the light itself falls away |
| 3 | Alleys and streets at dusk and dawn | Dark and lampless for half an hour and more | Lamps and lit windows came on by the calendar's daylight alone, which starts to go only at sunset less half an hour; with the sun on the horizon the streets are already in shade |
| 4 | Shops, cafes, the bank, the library, a tower's hall, day and night | A burnt white patch on the ceiling, black walls, dark corners | Their one lamp was a true 1/d2 light 0.4 m under the ceiling with a reach of 7 m, in halls 38 m long |
| 5 | Everywhere, at High | A red "Lumen surface cache atlas oversubscribed by 20 %" line over the picture; far surfaces lost their bounced light at random | The town with its insides asks for more of Lumen's surface atlas than High gives |

The night look itself is decided: ADR-0023, approved in the slice. The slice is frozen (ADR-0045), so Unreal has to get there.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| A fixed exposure by the clock, as the slice has | Exactly the slice's recipe | Changes the approved daylight indoors, which is the engine's own eye adapting to a room (ADR-0048) |
| **Leave the eye free by day and only limit how far it may open as the sun goes** | The approved daylight, indoors and out, is untouched to the pixel; night is night | Three numbers to tune by eye against the slice's night pictures |
| Fix the haze, the lamps' hours and the sun in the game mode's own `UpdateSun` | One place for all of the light | Other sessions had `BrixGameMode.cpp` open while this was done |
| **A part of its own that the engine makes for every game world (`UBrixLight`), ticking after the game mode** | Touches no file anyone else had open; can be switched off to compare (`-BrixLightOff`) | The haze's colour is set twice a frame at dusk and night (the game mode's, then this); to be folded into the game mode when that file is free |
| Raise Lumen's surface atlas | The engine's own suggestion | More video memory on a laptop with integrated graphics |
| **A smaller picture of each surface in the atlas at High (80 % a side)** | Fits, at no more memory | Far bounced light a little softer |

## Decision
- **The arithmetic is `BrixWorld/BrixNight`** (`BrixLightNow`): from the day's light, the sun's height, the weather and
  whether the camera is in a room it gives the eye's limit, the haze's colour and glow, how far the lamps are wanted, and
  the night's light and sky. Nothing is stored or ticked. `WLD042_the_days_light_is_kept_and_the_night_is_dark` holds it to:
  by day with the sun up, the game mode's own haze and a free eye, exactly; at night, the night's.
- **`UBrixLight`** (BrixTown, a world subsystem) applies it each frame after the game mode has ticked:
  - *the eye* may open past the day's exposure (`DayEV`, measured: 1.3 in the open street at noon) freely by day,
    `DuskOpen` stops out of doors with the sun on the horizon, `NightOpen` at night and `NightOpenIndoors` in a room;
  - *the haze* is the game mode's with the sun up; a low sun's is dimmer and cooler (`LowSunHaze`), with the sun's colour
    only in the air towards it; at night it is `NightHaze`, the slice's dark blue distance;
  - *the night's light*: a faint blue light from high up (`NightLux`, from the slice's own moon direction when its moon is
    down), the air's second light, so the sky is a deep blue and the sky light has something to fill the streets with;
    it casts shadows, so roofs keep it out of rooms.
- **Lamps and lit windows** (`ABrixLamps`, `ABrixRooms`) come on as the sun gets low (`UBrixLight::LowSun()`), not only
  by the calendar.
- **Room lamps** (`ABrixRooms`) are an even pool that fades out at its reach, as the slice's `roomLight()` caps a lamp
  close by; a floor with no plan has one lamp reaching 0.7 of the floor's diagonal.
- **Lumen at High**: `Game/Config/DefaultScalability.ini` sets `r.LumenScene.SurfaceCache.CardTexelDensityScale=80`.
- Every number is in `Game/Config/DefaultGame.ini` (`[/Script/BrixTown.BrixLight]`, `BrixRooms`, and `NightFill` of the
  game mode, now 1: what the sky light gathers at night is the night sky's own blue).

## Consequences
- The approved daylight is unchanged: noon and 07:20 shots match the ones taken before to within a few levels, indoors
  and out, in every kind of weather.
- The night and dusk are new in Unreal and follow ADR-0023 by eye, not by the slice's shader: they wait for the owner's
  word. Still to bring over from WLD-042: the stars, the Milky Way, the moon's disc, path and phases (the night's light
  comes from a fixed place and does not wax or wane), and the planet (WLD-065).
- Lamps are lit from about an hour before sunset to an hour after sunrise in summer; by day they stay off, though the
  reference renders show them lit in shade. Lighting them by day would change the daylight the owner approved in Unreal.
- Not fixed, found on the way: lit windows give no light to the pavement in front of them (they are kept out of Lumen
  for its cache); the alley's haze is heavy from 18:00 as the shade deepens (the game mode's haze has one brightness
  whatever the light); Eastgate Mall's inside is as bright at night as by day; residents can walk into a test shot.
- A light made as an `ADirectionalLight` actor registers with the engine's defaults as it is spawned, deferred or not;
  properties set afterwards reach the picture only when next changed through a setter. The night's light is therefore a
  component set up before it is registered. The game mode's sun is made the first way and works because its strength,
  colour and direction are set again every frame; anything added there that is set once should be made the second way.
- Tried and taken out: keeping a low sun's light as if from 12 degrees up (the sky's `TransmittanceMinLightElevationAngle`).
  The dusk pictures were the same with and without it, and by day it makes the engine print "lighting needs to be
  rebuilt" in red over the picture.
- `-BrixTour=` (one run, a shot at each stop of a list) replaces a launch per shot: a launch costs one to four minutes on
  the owner's laptop, a stop 7 s. Two runs at once hung the graphics chip (`DXGI_ERROR_DEVICE_HUNG`): run one at a time.
