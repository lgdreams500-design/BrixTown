r"""Imports the furniture kit's pieces (Art/Export/unreal_furniture_37/<piece>_<line>.glb, made by
Tools/blender/unreal_furniture_37.py from furniture kits 17 and 18) into /Game/Furniture as static meshes with Nanite,
one per piece and line: /Game/Furniture/SM_<piece>_<line>, e.g. SM_k17_sofa_oldtown, SM_k_counter_plain (the pieces and
lines are listed in the folder's index.json). Each mesh has one material slot, "furniture", pointed at
/Game/Furniture/M_Furniture (the vertex colour, matte; Tools/unreal/make_furniture_material.py, run here when missing),
so no imported material is kept.
The meshes are binary and kept in Git LFS (ADR-0002); this script is how they are made again.

Run from the repository root (engine path as in docs/setup/DEV_ENVIRONMENT.md):
  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\import_furniture_kit.py" -unattended -nullrhi -nosplash
Only the pieces the game places are imported: those of World/rules/home_plans.json's plans (FURN-060) and of the furniture
outlet's room sets (FURN-020; its block file's sets, kit 17's living room and kit 18's rooms), in every line, and any named in
BRIX_FURNITURE_EXTRA (comma-separated, e.g. p_counter,f_table), so the binaries stay as few as the town needs.
BRIX_FURNITURE_ADD=1 imports only the pieces wanted that are not in /Game/Furniture yet and leaves the rest as they are (so
adding pieces does not remake, and change, every mesh already there).
"""
import glob
import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SOURCE = os.path.join(ROOT, "Art", "Export", "unreal_furniture_37")
DEST = "/Game/Furniture"
STAGE = DEST + "/_Import"                    # each file is imported into its own folder here, then moved to DEST
MATERIAL = DEST + "/M_Furniture"

assets = unreal.EditorAssetLibrary
if not assets.does_asset_exist(MATERIAL):
    exec(compile(open(os.path.join(ROOT, "Tools", "unreal", "make_furniture_material.py"), encoding="utf-8").read(),
                 "make_furniture_material.py", "exec"), {"__name__": "__main__"})
material = unreal.load_asset(MATERIAL)

# the old meshes go (the material stays: it lives in the same folder), unless only new ones are added
ADD = os.environ.get("BRIX_FURNITURE_ADD", "") == "1"
if assets.does_directory_exist(STAGE):
    assets.delete_directory(STAGE)
if not ADD:
    for name in assets.list_assets(DEST, recursive=True, include_folder=False):
        if name.split(".")[0] != MATERIAL:
            assets.delete_asset(name)

import json
_plans = json.load(open(os.path.join(ROOT, "World", "rules", "home_plans.json"), encoding="utf-8"))["plans"]
WANT = {it["piece"].replace(":", "_") for p in _plans.values() for it in p["items"]}
WANT |= {n for n in os.environ.get("BRIX_FURNITURE_EXTRA", "").split(",") if n}
# the furniture outlet's room sets (outlet.js): kit 18's rooms by their pieces, kit 17's living room by its kit pieces
_city = os.path.join(ROOT, "World", "city_01")
for _b in json.load(open(os.path.join(_city, "city.json"), encoding="utf-8"))["blocks"]:
    for _l in json.load(open(os.path.join(_city, _b["file"]), encoding="utf-8"))["lots"]:
        if _l.get("kind") != "furniture":
            continue
        for _st in _l.get("sets", []):
            if _st.get("pieces"):
                WANT |= {pc[0].replace(":", "_") for pc in _st["pieces"]}
            else:
                WANT |= {"k17_" + k for k in ("rug", "sofa", "armchair", "coffee", "floor_lamp", "bookcase", "bed", "bedside", "wardrobe")}
_index = json.load(open(os.path.join(SOURCE, "index.json"), encoding="utf-8"))["pieces"]
stems = []
tasks = []
for path in sorted(glob.glob(os.path.join(SOURCE, "*.glb"))):
    stem = os.path.splitext(os.path.basename(path))[0]
    if not any(stem == piece + "_" + line for piece in WANT if piece in _index for line in _index[piece]):
        continue
    if ADD and assets.does_asset_exist(DEST + "/SM_" + stem):
        continue
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = STAGE + "/" + stem
    t.automated = True
    t.replace_existing = True
    t.save = False
    tasks.append(t)
    stems.append(stem)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

done, missing = [], []
for stem in stems:
    meshes = [unreal.load_asset(n) for n in assets.list_assets(STAGE + "/" + stem, recursive=True, include_folder=False)]
    meshes = [a for a in meshes if isinstance(a, unreal.StaticMesh)]
    if len(meshes) != 1:
        missing.append("%s (%d meshes)" % (stem, len(meshes)))
        continue
    a = meshes[0]
    for i in range(len(a.static_materials)):
        a.set_material(i, material)
    # Nanite: a town of furnished rooms is drawn at the detail the screen needs
    nanite = a.get_editor_property("nanite_settings")
    nanite.enabled = True
    a.set_editor_property("nanite_settings", nanite)
    want = DEST + "/SM_" + stem
    assets.rename_asset(a.get_path_name().split(".")[0], want)
    a = unreal.load_asset(want)
    lo, hi = a.get_bounding_box().min, a.get_bounding_box().max
    unreal.log_warning("Brix furniture: SM_%s slots %s box (%.0f %.0f %.0f)..(%.0f %.0f %.0f) cm" % (
        stem, [str(s.material_slot_name) for s in a.static_materials], lo.x, lo.y, lo.z, hi.x, hi.y, hi.z))
    assets.save_loaded_asset(a, only_if_is_dirty=False)
    done.append(stem)

# the materials and textures the importer made are not used: they go with the staging folder
assets.delete_directory(STAGE)
unreal.log_warning("Brix furniture: done; %d meshes in %s%s" % (
    len(done), DEST, ("; NOT IMPORTED: " + ", ".join(missing)) if missing else ""))
