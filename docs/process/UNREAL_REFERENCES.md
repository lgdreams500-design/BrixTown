---
doc_id: WASTED-UNREAL-REFERENCES
title: Brix Town — Plan: every place's light settings as pictures taken in Unreal
version: 1.0.0
status: accepted          # ADR-0075; the owner in chat, 2026-11-28: "devise a plan to convert all the references in the index.html to unreal based reference. like a script or something which will update or create a fresh index.html that will have single image for blender and light settings references built in unreal. what settings we need for each area is already there in index.html." ... "ideally should run without requiring llm or tokens" ... "keep merging changes area by area and update index.html area by area so I can check without stopping the session"
last_updated: 2026-11-28
relates_to: [ADR-0004, ADR-0058, ADR-0061, ADR-0066, ADR-0073, ADR-0074, ADR-0075, ART-001]
---

# Every place's light settings, taken in Unreal

## What the owner asked for
The reference page (`Art/Reference/index.html`) shows each place once in Blender (what the place is) and its light
settings (morning, midday, dusk, the nights, the weathers) as pictures **taken in the game**. One script makes the
pictures and the page, with no agent needed to run it. The work lands area by area, each merged on its own, so the
owner can open the page at any time and see what is done so far.

## What changes and what does not
- **Places with light settings change**: the 14 references that have more than one setting.
  | Area | Reference | Settings today |
  |---|---|---|
  | A home's room | `IndoorHomeRoom` | 8 |
  | A home's hallway | `IndoorHallway` | 6 |
  | The alley | `WorldAlley` | 10 |
  | Market Square | `WorldMarketSquare` | 10 |
  | The street | `WorldStreet` | 10 |
  | The sky | `WorldSky` | 10 |
  | The bakery | `ShopsBakeryLight` | 4 |
  | A shop floor | `ShopsShopFloor` | 4 |
  | A big hall | `ShopsBigHall` | 4 |
  | Eastgate: court, entrance, food court, shop, escalators | `MallsEastgate*` | 4 each |
- **Sheets of things do not change**: furniture, lamps, vehicles, people, faces, the interface, building colourways and
  the furnished layouts are pictures of what a thing is, not of a light setting. They stay as they are.
- **No Blender file is deleted or overwritten** (ADR-0004). A place's Blender settings stay in the repository and in
  the catalog; the page shows one of them (its Midday, or its first) as "what it is".
- **Nothing is approved by the script or by an agent.** Every Unreal picture arrives as "for your review".

## The data: one block a reference, in `Art/Reference/catalog.json`
```
"unreal": {
  "where": "refroom:home",                  a tour stop's place: refroom:<room> | cam:x,z,up,yaw,pitch | stand:... | in:<lot>
  "blender": "Indoor/IndoorHomeRoom_Midday.jpg",   the one Blender picture the page keeps showing
  "settings": { "Morning": "ref", "Midday": "12:00", "Dusk": "20:00", "NightLamps": "22:30" },
  "missing": { "NightFullMoon": "the game has no moon phases yet (WLD-042)" },
  "files": [ ... ],                          written by the script
  "status": "proposed",                      only the owner's word makes it "approved"
  "shot": { "commit": "...", "date": "...", "quality": "High, 1066 x 666" }
}
```
A setting's value is `hh:mm` or `ref`, with `|<weather>` after it for a weather (`12:00|rain`). The names are the
reference's own (the suffix of its Blender files), so the page pairs them without guessing.

Which hour of the game a setting is (the same for every place unless its block says otherwise):
| Setting | Out of doors | Indoors |
|---|---|---|
| Morning | 07:20 | `ref` (the golden rooms), 07:20 elsewhere |
| Midday | 12:00 | 12:00 |
| Dusk | 20:00 | 20:00 |
| NightLamps | 22:30 | 22:30 |
| Cloudy, Rain, Fog, Snow | 12:00 with that weather | not shown indoors |
| NightFullMoon, NightNewMoon, the FireOut nights | **missing**: no moon phases, no fire to put out | the same |
A setting the game cannot show is listed under `missing` with its reason and shown on the page as "not in the game
yet", never faked with another hour.

## The script: `Tools/unreal/reference_shots.py` (no agent needed)
```
python Tools/unreal/reference_shots.py --area IndoorHomeRoom      one reference
python Tools/unreal/reference_shots.py --all                      every reference that has an "unreal" block
python Tools/unreal/reference_shots.py --all --page-only          the page again, no shots
```
For each reference asked for it:
1. checks its block (a setting named that the reference has not, a place that does not parse: it stops and says);
2. writes one tour list for everything asked and runs `Tools/unreal/light_tour.ps1` once (one launch, about 7 s a shot);
3. saves each shot as `Art/Reference/<Category>/Unreal/<Id>_<Setting>.jpg` (never beside or over a Blender file);
4. runs `look_check.py` on them, and `card_read.py` where a stop has a card, and writes the numbers to
   `Art/Reference/<Category>/Unreal/<Id>.json`;
5. writes `files`, `status: proposed` (unless already approved and the picture is unchanged) and `shot` into the block;
6. runs `Tools/reference_catalog.py` to write `index.html` again.
It needs a built checkout. Run again after any change to the light, it re-shoots everything: that is the "no tokens" part.

## The page: `Tools/reference_catalog.py`
A reference with an `unreal` block is drawn as:
- **What it is**: the one Blender picture, with its locks and decision as now;
- **Its light, in the game**: the Unreal pictures, one a setting, in the reference's own order, marked "for your review"
  until approved, with the look check's failures and the known issues under them;
- **Not in the game yet**: the `missing` settings and why.
A reference without the block is drawn as it is today, so the page is whole at every step.
`Tools/test_reference_catalog.py` gains tests: a block's settings are the reference's, its files exist, nothing in
`Unreal/` is listed as a Blender file, and no block is "approved" without a decision in the owner's words.

## The order, one area a pull request
1. The script, the page and the tests, with **`IndoorHomeRoom`** as the first area (the golden room is built).
2. `IndoorHallway`.
3. `WorldAlley`, 4. `WorldMarketSquare`, 5. `WorldStreet`, 6. `WorldSky` (each has tour stops already; a matched camera
   is read from the reference's script).
7. `ShopsBakeryLight` (its matched camera is in the catalog), 8. `ShopsShopFloor`, 9. `ShopsBigHall`.
10. to 14. Eastgate: court, entrance, food court, shop, escalators.

For each area: add its block, run the script, look at every picture beside its Blender one (LOOK_GATE G1 to G10), write
what is wrong into the block's `issues` and `Game/README.md` "Not yet", open the pull request, merge it, go on.

## What an area's pull request must not do
- Tune the light, or build or change a place, to make a picture look better. The pictures show the game as it is. What
  is wrong is written down; mending it is separate work the owner asks for.
- Approve anything, retire or delete a Blender picture, or change what a reference locks.
- Leave the page broken: `python -m unittest Tools/test_reference_catalog.py` passes before every merge.

## Known gaps before starting
- The moon nights and the fire-out nights cannot be shown: the game has no moon phases and lights no fire.
- A camera in the town that sees what its Blender reference sees does not exist for the shop floor, the big hall and
  most of Eastgate: those areas first need the camera read from the reference's script (`made_by`), and where the
  game's place differs from the reference in what is there, that is written as an issue, not hidden by another view.
- The golden room's light is a trial (`Art/StyleTests/unreal_golden_room_58`): its pictures go up as "for your review".
- The decision is ADR-0075 (the look stays locked to `Art/Reference/`, ADR-0004; ADR-0073 was about rooms only, and
  this takes it out of doors and to the shops and the mall, which the owner asked for).
