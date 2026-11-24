"""Imports the building kit's pieces (Art/Export/unreal_kit_36/*.glb, made by Tools/blender/unreal_kit_36.py) into
/Game/Kit as static meshes. Every material slot keeps its kit name (stone_dark, glass, frame, ...) and is pointed at
/Game/Greybox/M_Greybox; the game colours each slot from Config/DefaultGame.ini, so no imported material is kept.
The meshes are binary and kept in Git LFS (ADR-0002); this script is how they are made again.

Run from the repository root (engine path as in docs/setup/DEV_ENVIRONMENT.md):
  UnrealEditor-Cmd.exe "%CD%\\Game\\BrixTown.uproject" -run=pythonscript -script="%CD%\\Tools\\unreal\\import_building_kit.py"
      -unattended -nullrhi -nosplash
"""
import glob
import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SOURCE = os.path.join(ROOT, "Art", "Export", "unreal_kit_36")
DEST = "/Game/Kit"
GREYBOX = "/Game/Greybox/M_Greybox"

assets = unreal.EditorAssetLibrary
if assets.does_directory_exist(DEST):
    assets.delete_directory(DEST)
greybox = unreal.load_asset(GREYBOX)

tasks = []
for path in sorted(glob.glob(os.path.join(SOURCE, "SM_Kit_*.glb"))):
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = DEST
    t.automated = True
    t.replace_existing = True
    t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for name in assets.list_assets(DEST, recursive=True, include_folder=False):
    a = unreal.load_asset(name)
    if isinstance(a, unreal.StaticMesh):
        base = os.path.splitext(os.path.basename(a.get_path_name().split(".")[0]))[0]
        if not base.startswith("SM_Kit_"):
            continue
        for i, slot in enumerate(a.static_materials):
            a.set_material(i, greybox)
        # Nanite: thousands of windows and shutters across the town are drawn at the detail the screen needs
        nanite = a.get_editor_property("nanite_settings")
        nanite.enabled = base != "SM_Kit_RoofWedge"
        a.set_editor_property("nanite_settings", nanite)
        lo, hi = a.get_bounding_box().min, a.get_bounding_box().max
        unreal.log_warning("Brix kit: %s slots %s box (%.0f %.0f %.0f)..(%.0f %.0f %.0f) cm" % (
            base, [str(s.material_slot_name) for s in a.static_materials], lo.x, lo.y, lo.z, hi.x, hi.y, hi.z))
        assets.save_loaded_asset(a, only_if_is_dirty=False)

# the materials and textures the importer made are not used: remove them so only the meshes are kept
for name in assets.list_assets(DEST, recursive=True, include_folder=False):
    a = unreal.load_asset(name)
    if not isinstance(a, unreal.StaticMesh):
        assets.delete_asset(name)

# the importer may put each mesh in <name>/StaticMeshes/: move every one to /Game/Kit/<name>, where the game looks
for name in assets.list_assets(DEST, recursive=True, include_folder=False):
    pkg = name.split(".")[0]
    base = os.path.basename(pkg)
    want = DEST + "/" + base
    if pkg != want:
        assets.rename_asset(pkg, want)
for folder in assets.list_assets(DEST, recursive=False, include_folder=True):
    if assets.does_directory_exist(folder) and not assets.list_assets(folder, recursive=True, include_folder=False):
        assets.delete_directory(folder)
unreal.log_warning("Brix kit: done; " + ", ".join(sorted(os.path.basename(n.split(".")[0]) for n in assets.list_assets(DEST, recursive=True, include_folder=False))))
