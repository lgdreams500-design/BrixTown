# Art/Reference — the approved look references

The look everything made is checked against (ADR-0004, ADR-0061). Do not edit or overwrite these files: a new approved
look gets a new file, an entry in the catalog and a decision record.

- **Find a reference:** `catalog.json` lists every one: what it locks, when and by what decision it was approved, the
  script that renders it, the game models and data built from it or measured against it, issues seen in it, and its old
  names. Its `gaps` say what still has no reference.
- **Review them:** open `index.html` in a browser: a tab per category, each reference with its pictures and details.
  Never edit it by hand; it is written from the catalog.
- **Change them:** render with the script, put the file in its category's folder under its name, add or change its entry
  in `catalog.json`, then run `python Tools/reference_catalog.py` (it checks the catalog against the folders and writes
  `index.html`). The owner approves every change of look.

## Names

`<Category>/<Category><Subject>[_<Variant>].<ext>`, PascalCase, the reference's id being the name without the variant:
`Indoor/IndoorHomeRoom_Morning.jpg` (reference `IndoorHomeRoom`), `Vehicles/VehiclesSoftBox.png`, `People/PeopleFaceTalk.mp4`.

Categories: `World`, `Buildings`, `Indoor`, `Shops`, `Malls`, `Furniture`, `Ornaments`, `Vehicles`, `People`, `Interface`.

References taken out of use are in `Art/Retired/` (listed under the catalog's `retired`); look tests and rejected
directions stay in `Art/StyleTests/` and are not references. Rules and numbers behind the images:
`docs/art/ART_DIRECTION.md`, `docs/art/CHARACTER_FACE.md`, `docs/art/CHARACTER_MOTION.md`, `docs/art/INTERIOR_STYLE.md`.
