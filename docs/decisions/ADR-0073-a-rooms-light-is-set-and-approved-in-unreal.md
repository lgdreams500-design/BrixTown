---
adr: ADR-0073
title: A room's light is set and approved in Unreal; the Blender pictures say what a place is
status: accepted          # the owner in chat, 2026-11-28: "please also check is it even worth creating references in the blender or should we just do reference in unreal", "which is faster?", then, asked "Make the rooms' light reference in Unreal and keep Blender for what a place is?": "Yes, light in Unreal"
date: 2026-11-28
relates_to: [ADR-0004, ADR-0055, ADR-0058, ADR-0061, ADR-0066, ADR-0072, ART-001]
---

## Context
Since ADR-0055 a room's light in the game has been set by number against Blender pictures. First in the measuring
rooms (`ABrixLightRoom`: kit 18's rooms built again in Unreal, which came within half a stop). Then, when those
references were retired (2026-11-26), in the town's own rooms against `IndoorHomeRoom`, `IndoorHallway`,
`ShopsShopFloor` and `ShopsBakeryLight` (LOOK_GATE, the matching round). Two days of rounds (2026-11-27 and 28, trials A
to K in `Art/StyleTests/unreal_rooms_light_57/`) did not bring the rooms within the pass marks, and each round found
another reason the numbers were loose:

- **The two pictures are of different rooms.** The game's home room stop is a corner dining room with 3.6 m of windows
  on two walls, unfurnished, on orange boards; its reference has one window, furniture and grey slabs. A whole
  picture's brightness and warmth follow what is in it as much as its light.
- **The two are lit by different recipes.** A Blender room has its bounce raised (`ROOM_BOUNCE`), a fill lamp of its own
  (`BounceCard`), an eye that opens until the picture reaches a set brightness (`INDOOR_EYE`) and Blender's tone curve.
  The game has the room's own light (`RoomFill`), an eye opened by one fixed amount (`DayOpenIndoors`) and the engine's
  tone curve.
- **A reference can be right as a picture and wrong as a target**: the hallway, approved with its eye 4.4 stops wider
  than the home room's, and remade (ADR-0072).
- **The live light is not what is wrong, as far as it has been checked.** On 2026-11-28 a path-traced shot of the game's
  hallway (the engine's slow, correct light of the same scene) agreed with the live light within 0.04 stops. Both shots
  had the room light on; the pair with it off has not been taken.

What each tool costs, by the project's own figures: a Blender place builds in seconds and a review round of four
settings takes about a minute (the mall entrance, 2026-11-27: 7 s and 52 s). An Unreal launch takes 1 to 4 minutes, a shot about 7 s, a build after a change to code
about 3 minutes, and only one Unreal runs at a time on this machine. A new place cannot be pictured in Unreal before it
is built: the town is built by code from `World/`, with no level to dress by hand.

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Keep matching the town's rooms to the Blender pictures by number | No change | Different rooms and different recipes: the numbers are loose, and two days of rounds did not pass |
| Keep the Blender pictures as the number target and build each reference's room again in Unreal (ADR-0055's way) | Like with like in shape | Every reference room is built twice, and again whenever it changes (the measuring rooms were out of date within a day); the two recipes and tone curves still differ |
| Every reference in Unreal, none in Blender | One renderer | A new place has to be built in the game before it can be seen, so the reference is the build and a "no" throws the build away; the models are made in Blender anyway |
| The measured target is a path-traced picture of the game's own room | The engine's own correct light | It shows whether the live light is true to the scene's lamps, not what those lamps should be; on this laptop about one sample a pixel a second, too grainy for the owner's eye |
| **Blender says what a place is; a room's light is set and approved in Unreal** | Each tool does what it is fast at; light numbers compare Unreal with Unreal | The first approval of a kind of room's light is by the owner's eye in Unreal, not by a number; a golden room has to be built |

## Decision
- **The Blender references stay, made as before** (ADR-0066): made before a thing is built and approved by the owner,
  they say what a place is: its shape, windows, materials, colours and furniture, and its mood at each hour. For a
  room's light they are looked at for mood. **No brightness or warmth number of a room in the game is passed or failed
  against a Blender picture.**
- **A room's light is set in Unreal, in a golden room**: one room built in the game to look like its Blender reference
  (the home room, with its hallway beside it), a grey card standing in each. The owner approves its Unreal pictures at
  each hour the room's light changes in, with the Blender picture beside them for mood. Once approved, those pictures
  are the light reference for rooms: kept in `Art/Reference/` and listed in the catalog, never over a Blender picture.
- **Every other room is measured against the golden room's readings on the grey card**, Unreal against Unreal, at the
  pass marks of ADR-0055 (half a stop in brightness, a third in warmth).
- **The path tracer is a check, not a target** (`Tools/unreal/light_tour.ps1 -PathTrace`, a tour stop's `pt=`): when a
  place looks wrong it tells a fault of the live light from a fault of the build.

## Consequences
- LOOK_GATE 1.3.0: "What it is measured against" and the matching round say this. For a room's light the round's
  measuring against a Blender picture is gone; its steps on what is there before how it is lit stay.
- **Until the golden room's pictures are approved a room's light has no measured target.** It stays as merged
  (`Art/StyleTests/unreal_rooms_light_57/`, "K + beams") and is not tuned on the town's rooms. Written in `Game/README.md`,
  The look gate, Not yet.
- ADR-0055's "what a room's light should be is measured against the Blender references" is replaced for rooms. Its
  measuring rooms (`ABrixLightRoom`) are where the golden room is to be built; its pass marks are kept.
- ADR-0066 stands: no approved reference, no building it. A room's Blender reference no longer has to be a number
  target for light, so "is the reference right?" (the hallway, ADR-0072) is a question about a picture's mood, not about
  a number the game must reach. The 2.3 stops between the game's hallway and the remade reference is no longer a pass
  mark missed; the hallway's light is settled in the golden room.
- ADR-0004 is unchanged: the look stays locked to `Art/Reference/`. The approved Unreal pictures are added there.
- Not done, and written in `Game/README.md`: the golden room is not built; no grey card stands anywhere; the catalog has
  no place for an Unreal light picture and `reference_catalog.py` does not name one; `look_match.py` does not read a
  card; the path-traced pair with the room light off.
- Not decided here, to ask the owner when reached: whether out of doors follows (the matching round still measures
  out-of-doors stops against their Blender references); whether a room's Blender reference still needs every hour;
  whether the home room reference keeps its raised bounce, now that it sets no number.
- To be tried in the golden room, not decided: the room's own light standing at the room's windows and casting shadows.
  Today it hangs in the middle of every room, the hallway too, and casts none (`ABrixRooms`, `Fill`), so walls do not
  stop it and a hallway with no window is lit as a room is.
