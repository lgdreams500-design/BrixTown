---
adr: ADR-0074
title: Glass travel tubes over the streets, as slim as a rider
status: accepted          # the owner in chat, 2026-11-28, of the reference VehiclesGlassTube (slim tube, booth 40% narrower, every setting): "Approve, go on"
date: 2026-11-28
relates_to: [ADR-0004, ADR-0066, ADR-0071, CITY-108, CITY-109, CITY-110, CITY-111, CITY-112, CITY-113, CITY-114]
---

## Context
On 2026-11-28 the owner asked for "teleport points on the map": glass tubes which suck the player in, carry them to the
next stop and pop them out, with the camera following smoothly in real time. They picked `F-VEH-TUBE` level 2 (a
network) for the first slice. While the first reference was being drawn they said: "thinner diameter should be just
enough fit the player's waist. so the player will rotate laterally when going horizontal".

The town's surfaces are matte (ART_DIRECTION W1), and the rooms forbid shiny glass (I1). So a glass tube is an
exception to the locked look (ADR-0004), as Eastgate's glass signs are (ADR-0071), and it needs this record and the
owner's approval.

## Decision
- A stop is a round glass booth on a dark stone plinth, 1.5 m across and 3 m high (the owner, 2026-11-28: "make the entry chamber narrower, may be reduce by 40%"), with three brass ribs and
  brass bands, a cream cap and one lantern by its door. Its top narrows in a glass funnel into the tube.
- The tube is clear glass, faintly tinted and brighter at its edges where it catches the sky. It is only as wide as a
  rider (0.68 m inside, `World/rules/tubes.json` `tube_inner_m`). It rises straight out of the booth, bends over and
  runs level at least 6 m over the street on slim iron posts with cradles, with thin brass rings every 4 m.
- A rider is upright going up and down. Where the tube runs level they lie along it head first, and they turn smoothly
  in the bends (CITY-109).
- The glass is the tubes' and booths' only; nothing else in the town takes it.
- Ride numbers are data (`World/rules/tubes.json`). Stops and tubes are map data in `World/` (CITY-108).

## Consequences
- The reference `VehiclesGlassTube` is approved (the owner, 2026-11-28); the tubes are built against it (ADR-0066).
- Where the stops go on the map is the owner's to say. Fares (Q-42) and which residents ride (Q-41) are open.
