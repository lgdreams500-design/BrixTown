# Hours 50: every reference place at every setting (the owner, 2026-11-26)

`Tools/blender/hours_50.py` builds each reference place with the script it always came from and lights it for ten
settings: morning, midday (no haze), dusk; night with the lamps on, and with them off under a full and a new moon; and by
day cloudy, rain (puddles with rings where the drops fall, only under open sky), fog and snow (lying only where it can
fall). Reviewed with the owner one place at a time; an approved place goes to `Art/Reference/` (ADR-0061).

| Place | Pictures | State |
|---|---|---|
| The alley (old_town_04.py) | `Alley_<Setting>.jpg`, `review_Alley.jpg` (the ten), `review_Alley_rain_close.jpg` (puddles and rain close up) | approved 2026-11-26: `Art/Reference/World/WorldAlley_*.jpg` |
| The sky and its clouds, from Market Square (WASTED_PLACE=sky, with sky_52.py) | `Sky_<Setting>.jpg`, `review_Sky.jpg` | approved 2026-11-26: `Art/Reference/World/WorldSky_*.jpg`; no planet (ADR-0064); cloud shapes tried and dropped (`../sky_clouds_53/`) |
| Market Square, from its arcade (WASTED_PLACE=square; its shade lifted by SQUARE_BOUNCE) | `MarketSquare_<Setting>.jpg`, `review_MarketSquare.jpg` | approved 2026-11-26: `Art/Reference/World/WorldMarketSquare_*.jpg` |
| The street (building_kit_07.py) | `Street_<Setting>.jpg`, `review_Street.jpg` | approved 2026-11-26: `Art/Reference/World/WorldStreet_*.jpg` |
| A home's room (the living room; WASTED_PLACE=rooms WASTED_ONLY=living): six settings, no weather | `Rooms_Living_<Setting>.jpg`, `review_Rooms_Living.jpg` | approved 2026-11-26: `Art/Reference/Indoor/IndoorHomeRoom_*.jpg` |
| A home's hallway by the stairs (WASTED_PLACE=hallway, `corridor()`): no window, lit through a room's door | `Hallway_<Setting>.jpg`, `review_Hallway.jpg` | approved 2026-11-26: `Art/Reference/Indoor/IndoorHallway_*.jpg` |
| The bakery (WASTED_PLACE=bakery): four settings, no moon, its oven always lit | `Bakery_<Setting>.jpg`, `review_Bakery.jpg` | approved 2026-11-26: `Art/Reference/Shops/ShopsBakeryLight_*.jpg` |
| A shop's floor without a fire (WASTED_PLACE=shop, `shopfloor()`): the grocer's size, the approved shopfront | `Shop_<Setting>.jpg`, `review_Shop.jpg` | approved 2026-11-26: `Art/Reference/Shops/ShopsShopFloor_*.jpg`; the awning rolled up mornings (`AWNING_IN`) |
| The furniture outlet's hall, a big hall (WASTED_PLACE=outlet): the game's size and roof, glazed skylights | `FurnitureOutlet_<Setting>.jpg`, `review_FurnitureOutlet.jpg` | approved 2026-11-26: `Art/Reference/Shops/ShopsBigHall_*.jpg` |
| Eastgate Mall's Great Court (WASTED_PLACE=mall, `mall_court()`): its plan's shapes, its own look (`MALL_LOOK`) | `MallCourt_<Setting>.jpg`, `review_MallCourt.jpg` | approved 2026-11-26: `Art/Reference/Malls/MallsEastgateCourt_*.jpg`; made again 2026-11-28 with the glass shop signs (ADR-0071) |
| Eastgate Mall's south entrance from the forecourt (WASTED_PLACE=mall_entrance, `mall_porch()`, `mall_entrance_doors()`, `mall_floods()`): the shell's ribbon wall in one even stone over the arch, the curved doorway, the concourse's outlets, floodlights from dusk | `MallEntrance_<Setting>.jpg`, `review_MallEntrance.jpg` | approved 2026-11-27: `Art/Reference/Malls/MallsEastgateEntrance_*.jpg`. Not yet: the floodlights' fans leave the wall's foot dark between them, their lenses glow by day, the forecourt is grey slabs where the game has warm brick |
| Eastgate's Food Market egg hall as a food court (WASTED_PLACE=egg_hall, `mall_egg_hall()`, `egg_food_court()`, `egg_seating()`): the game's hall and egg, food outlets round the drum, long and round tables (ADR-0070) | `EggHall_<Setting>.jpg`, `review_EggHall.jpg` | approved 2026-11-27: `Art/Reference/Malls/MallsEastgateFoodCourt_*.jpg`. Not yet: the sun's diamonds on the floor faint (the town's soft sun), the pendants over the long tables hardly in view, a mirror image of the game; made again 2026-11-28 with the glass shop signs (ADR-0071) |
| One of Eastgate's shops close by, The Linen Press on the south concourse (WASTED_PLACE=mall_shop, `shop_banner()`, `sign_glass()`): its sign coloured glass raked back 5 degrees, its name backlit, a brass trough's light onto it (ADR-0071) | `MallShop_<Setting>.jpg`, `review_MallShop.jpg` | approved 2026-11-28: `Art/Reference/Malls/MallsEastgateShop_*.jpg`. Not yet: a mirror image of the game, as the court |
| Eastgate's moving stairs close by, at the foot of the pair in the court's middle (WASTED_PLACE=mall_escalators, `mall_escalator()`, `mall_ribbed()`, `ESCALATOR`): the plan's ramps with steel floor plates and combs, ribbed steps with brass noses, steel skirts with a line of light, glass sides with round newels and a brass handrail, timber-clad sides, the game's parapet down the middle making two lanes | `MallEscalators_<Setting>.jpg` | approved 2026-11-28: `Art/Reference/Malls/MallsEastgateEscalators_*.jpg`. Not yet: the middle parapet's two handrails read as one from the foot; the game's 12-degree slope gives long shallow steps; the first flat steps nearly black in the bridge's shade; the sun reaches them only at Midday; a mirror image of the game, as the court |

What the owner changed on the way: fewer lamps down the alley; the moon high and down the alley so it reaches the paving;
one lamp-lit night instead of two; cloud, rain, fog and snow added; no haze at noon; no rain, wet or snow under cover;
puddles with rings; rain thin, short and bluish grey; no glow on rain or snow.

Rain redone as the game's (the owner, 2026-11-26, after the street's first try showed hard black puddles): the open paving
darker with a faint sheen in half-metre cells (`lying(..., tile=True)`), soft-edged dark puddles with a little mirror
(their fade in the UV map), kept apart (overlapping ones mirrored each other black), faint grey rings, drops half as wide
(4.5 mm). The alley's rain picture was rendered again with it; its other nine are as approved.

Snow (the owner, 2026-11-26): each flake a small smooth ball (was an eight-sided bead that read square), the far
ones 72 mm across, the near ones shrinking to 30% smaller at 5 m (`flake_near_size`, `flake_full_at`); 3,600 in view as
before. The four places' snow pictures were rendered again with it.

Rooms in the world's light (the owner, 2026-11-26): by day lamps and fire out (`day_lamps`, `day_fires` 0), the street outside
the window sunlit and bouncing light in (`_Outside`), the boards greyer (`ROOM_GREYER`), the shade lifted (`ROOM_BOUNCE`), lamps a
warm white (`INDOOR_LAMP`, until FURN-066's kinds), the moon low on the sun's side so it comes in (`indoor_height`, `indoor_times`),
the eye opening up to 9 stops on a moonless night (`EYE_MAX_INDOORS`). Not yet: the lamps' colour from their kinds (FURN-066).

The home room's wall facing the people throws light back onto them (the owner, 2026-11-26): a hidden soft light on that wall
(`BOUNCE_CARD`: 25 W by day following the sun and sky, 15 % of the lamps' light at night in their colour), as the wall's own
bounce reached too little. Tried and taken out: a wider window and a stronger sky (the owner: "no raising the window").

The hallway's moonlit nights (the owner, 2026-11-26): the full moon as dim as the new moon was and cool blue (`PLACE_EYE`
hallway NightFullMoon 12, `MOON` `indoor_colour`), the new moon darker still (NightNewMoon 2.5). The hallway only.

Fires (the owner, 2026-11-26): one red for every fire in every reference (`FIRE_COLOUR`), its flames glowing without catching
light (`FIRE_GLOW` 0.1) and its light softer and spread (`FIRE_LIGHT` 0.5); with no other light its red takes the room over, as
the owner expects. The home room has two more nights with the fire out too (`FIRE_OUT_HOURS`), as dim as the hallway's.

From 2026-11-26 references go to the owner at low quality (WASTED_SCALE=50 WASTED_SAMPLES=24) and are rendered in full only once
approved (the owner).
