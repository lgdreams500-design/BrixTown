---
adr: ADR-0067
title: Eastgate Mall's Great Court has a look of its own
status: accepted          # the owner in chat, 2026-11-26: "use the shapes from the game for the mall interiors but texture, color, styling etc, please come up with your self based on the reference screenshots", then "feel inspired and take a creative take on the look and feel add ornaments if required use glass and other material to make it look good", then "Approve it"
date: 2026-11-26
relates_to: [ADR-0004, ADR-0037, ADR-0038, ADR-0061, ADR-0066, WLD-058, WLD-059, WLD-060, WLD-061, WLD-062]
---

## Context
The town's look is locked (ADR-0004): mottled plaster, stone, timber, matte and chunky. Eastgate Mall's shell was built
in those materials (ADR-0037) and its inside designed afresh as a Great Court (ADR-0038), but neither had a reference.
Making the court's reference, the owner asked for the game's shapes and a look taken from their screenshots of the
game's court, and then for a creative take with glass and other materials.

## Decision
- The court's shapes are the game's: its plan in `World/city_01/blocks/b_16_5.json`, unchanged.
- The court has a look of its own, the reference `MallsEastgateCourt` (`Art/Reference/Malls/`, made by
  `Tools/blender/hours_50.py` `mall_court()`, its numbers in `MALL_LOOK`):
  - warm timber slats under the galleries;
  - cream edges, each with a line of light and brass trim;
  - glass balustrades with brass rails;
  - slender cream columns with brass collars;
  - a uniform polished stone floor with hidden joints and a brass inlay;
  - tinted roof lights with a diamond lattice;
  - chandeliers and translucent banners in the roof lights;
  - gardens with flowers, and plants hanging over the balustrades;
  - glass-sided moving stairs;
  - shop fronts in the town's colours.
- It is lit by the town's one light (the world's sun and sky, the kinds of light) like every other reference.
- This look is the court's. It does not change the town's (ADR-0004 stands); the mall's outside, its outlets' insides and
  its egg halls still need references of their own.

## Consequences
- The Unreal court and the slice's are built to this reference (ADR-0066); until then their README says so.
- Glass and polished metal are allowed in the court; elsewhere the locked materials stay.
