# ADR-0030: Buses, and the roads outside the wall in the traffic plan

Date: 2026-11-23. Status: accepted (owner's request and answers in chat, 2026-11-23).

## Context

The owner found too many cars in town that never move (557 in car parks) and asked for buses for the commute and smaller
car parks. They decided: residents of the independent houses outside the wall own the cars, the flats in the centre
mostly do not and use public transport; F-VEH-TRANSIT level 2 ("Network"); keep the tram and add buses; mall car parks
to about a quarter with a bus stop; row homes about half; one pull request for all of it.

Until now only homes inside the wall had cars, because the traffic plan (ADR-0009, `Tools/citysim/traffic.py`) knew only
the grid streets.

## Decision

1. **The roads outside the wall are worked out, not stored.** The traffic rules build lanes from roads.json `links` and
   the communities' block `lanes` each time they load: a road is split where another meets it, joined where one simply
   runs on into another, one lane each way along the winding line, give way where they meet, a turn at a dead end.
   A home bay for each house in front of it, on the roadway against its edge (the verge belongs to the garden fences), kerbside bays for other places, and parking along the lanes are
   placed from the lots and the rules. Nothing new is written into `World/` for this.
2. **Buses are vehicles in the same day plan as cars.** A line in `World/<city>/transit.json` is a list of stops; the bus
   timetable allows the way at the speed limits plus a red light at every signalled junction, and a bus never leaves a
   stop early. Buses queue, give way and keep their length clear like any vehicle, so their day is calculated with the
   cars' and the same on every machine.
3. **Riders are chosen after the vehicles are planned**: a resident with no car rides only when a planned bus gets them
   there by the time their walk would have, so the rest of their day does not move.
4. **Junction rule tightened**: a vehicle enters a junction only when the lane beyond has room for its whole length and for
   anyone already turning into it. Without this, buses stuck in junctions locked the South Gate at the evening peak.
5. **A household whose car would still be out near midnight walks that day**, and the day is planned again (a few passes
   at most), so cars are home at night.

## Consequences

- 548 resident cars instead of 198, and about 4,600 vehicle trips a day instead of 720; static cars in car parks 153
  instead of 557. Streets near the South Gate queue at the evening peak and line 2 runs late then.
- Commuters from outside drive to the gate and leave the map there, as cars passing through already appear there.
- Planning a day takes about three times as long (about 20 s in Python). The engine version (TECH-003) inherits the
  same rules.
- Bus timetables and frequencies, the flats' 10 % and the bus-ride thresholds are `[ASSUMPTION]` until tuned.
