---
adr: ADR-0072
title: The hallway reference remade, its doorway lit as the room beside it really lights it
status: accepted          # the owner in chat, 2026-11-28: "hall in reference has a bright source of light coming from room ... may be reference is wrong in this case as room don't have such high light source", then "Yes, remake it", then "Candidate 2", then "Approve"
date: 2026-11-28
relates_to: [ADR-0004, ADR-0055, ADR-0058, ADR-0061, ADR-0066, ART-001]
---

## Context
`IndoorHallway` (approved 2026-11-26) is a home's hallway with no window of its own. By day its only light comes through
the open door of a room beside it, whose window faces the sun (`corridor()` in `Tools/blender/hours_50.py`). In the
matching round of the rooms' light (`Art/StyleTests/unreal_rooms_light_57/`), the owner saw a bright source coming
through its doorway that the game's hallway does not have, and asked whether the reference was wrong.

The measurements showed that the light was not coming from the room beside the hallway. It came from how the
reference was made:
- every surface threw back more light than its colour gives (`ROOM_BOUNCE`: plaster and boards x2, the street x2.6);
- the eye opened until the picture's mean was 100, so the hallway's eye was 4.36 stops wider than the home room's (+0.13)
  in the same light. The ordinary sunlit room seen through the door then showed close to white.

Turning the sun away from the side room's window, or taking out the raised bounce, did not change the look while the
eye still opened to the target: the eye just opened further.

## Decision
- The hallway's bounce is the surfaces' own (no `bounce_more(ROOM_BOUNCE)`).
- Its eye is held where the first hallway had it, at morning, noon, dusk and the lamp-lit night (`PLACE_EYE_HELD`).
  The moon nights still open to the levels the owner set on 2026-11-26 (`PLACE_EYE`).
- The new set replaces `Art/Reference/Indoor/IndoorHallway_*.jpg`. The first is kept in
  `Art/Retired/IndoorHallway_2026-11-26_*.jpg`, and the catalog lists it under `retired`.

## Consequences
- By day the hallway is dim (noon mean 26 against 87), and its doorway reads as a sunlit room beyond. Dusk and the
  lamp-lit night are about half a stop darker. The new moon sits at its eye's limit (mean 1 where 2.5 is asked).
- The home room's reference (`IndoorHomeRoom`) still has the raised bounce, so the two rooms of one house are not lit
  by quite the same rule. That is recorded in both references' issues, not decided here.
- Against the remade reference, the game's hallway at noon is 2.3 stops too bright while its home room passes. That gap
  is the next step of the matching round.

## Changed since
- 2026-11-28 (ADR-0073): a room's Blender picture sets no number for its light, so the 2.3 stops is what was measured,
  not a pass mark missed. The hallway's light is settled in the golden room, in Unreal.
