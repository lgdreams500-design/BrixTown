"""Makes /Game/Greybox/M_Greybox: the one material of the grey-box town, a matte surface. Its colour is either the
parameter Color (a fixed slot, e.g. a window's glass) or, when the parameter InstanceColour is 1, each instance's own
colour from its first three custom data floats (a wall's plaster, a frame's paint: one group of instances can then hold
every building's colours, ADR-0046). BrixTownActor and BrixCharacter set both from Config/DefaultGame.ini.
The asset is binary and kept in Git LFS (ADR-0002); this script is how it is made, so it can be made again instead of
edited by hand. Run Tools/unreal/import_building_kit.py after it.

Run from the repository root (the engine path is this machine's, docs/setup/DEV_ENVIRONMENT.md):
  "C:\\Users\\leena\\Epic Games\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%CD%\\Game\\BrixTown.uproject"
      -run=pythonscript -script="%CD%\\Tools\\unreal\\make_greybox_material.py" -unattended -nullrhi -nosplash
"""
import unreal

PATH, NAME = "/Game/Greybox", "M_Greybox"
ROUGHNESS = 0.85          # matte, as ART-002 asks of every surface

lib = unreal.MaterialEditingLibrary
full = f"{PATH}/{NAME}"
# (MaterialEditingLibrary.delete_all_material_expressions crashes the engine headless in 5.8: the material is made anew
# instead; run Tools/unreal/import_building_kit.py afterwards, which points the kit's meshes at it again)
if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
# The town is drawn as instanced meshes, partly with Nanite; without these flags the game falls back to a grid material.
mat.set_editor_property("used_with_instanced_static_meshes", True)
mat.set_editor_property("used_with_nanite", True)

colour = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, -100)
colour.set_editor_property("parameter_name", "Color")
colour.set_editor_property("default_value", unreal.LinearColor(0.7, 0.7, 0.7, 1.0))

own = []                   # the instance's colour: custom data 0, 1, 2 appended into one vector
for i in range(3):
    c = lib.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -900, 100 + i * 120)
    c.set_editor_property("data_index", i)
    own.append(c)
rg = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -700, 120)
lib.connect_material_expressions(own[0], "", rg, "A")
lib.connect_material_expressions(own[1], "", rg, "B")
rgb = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -550, 160)
lib.connect_material_expressions(rg, "", rgb, "A")
lib.connect_material_expressions(own[2], "", rgb, "B")

switch = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -550, 320)
switch.set_editor_property("parameter_name", "InstanceColour")
switch.set_editor_property("default_value", 0.0)

pick = lib.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -300, 0)
lib.connect_material_expressions(colour, "", pick, "A")
lib.connect_material_expressions(rgb, "", pick, "B")
lib.connect_material_expressions(switch, "", pick, "Alpha")
lib.connect_material_property(pick, "", unreal.MaterialProperty.MP_BASE_COLOR)

rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 220)
rough.set_editor_property("r", ROUGHNESS)
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log_warning(f"Brix Town: made {full}")
