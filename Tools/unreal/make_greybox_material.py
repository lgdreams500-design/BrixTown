"""Makes /Game/Greybox/M_Greybox: the one material of the grey-box town, a matte surface whose colour is a parameter
named Color (set per layer by BrixTownActor and BrixCharacter from Config/DefaultGame.ini). The asset is binary and kept
in Git LFS (ADR-0002); this script is how it is made, so it can be made again instead of edited by hand.

Run from the repository root (the engine path is this machine's, docs/setup/DEV_ENVIRONMENT.md):
  "C:\\Users\\leena\\Epic Games\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%CD%\\Game\\BrixTown.uproject"
      -run=pythonscript -script="%CD%\\Tools\\unreal\\make_greybox_material.py" -unattended -nullrhi -nosplash
"""
import unreal

PATH, NAME = "/Game/Greybox", "M_Greybox"
ROUGHNESS = 0.85          # matte, as ART-002 asks of every surface

lib = unreal.MaterialEditingLibrary
full = f"{PATH}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
# The town is drawn as instanced meshes; without this flag the game quietly falls back to the engine's grid material.
mat.set_editor_property("used_with_instanced_static_meshes", True)

colour = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 0)
colour.set_editor_property("parameter_name", "Color")
colour.set_editor_property("default_value", unreal.LinearColor(0.7, 0.7, 0.7, 1.0))
lib.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)

rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 220)
rough.set_editor_property("r", ROUGHNESS)
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log(f"Brix Town: made {full}")
