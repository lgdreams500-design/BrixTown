r"""Makes /Game/Greybox/M_Glow: the lit glass of lamps and wall lanterns (WLD-028). Each instance's colour comes from its
first three custom data floats, as in M_Greybox, and it glows with that colour times the parameter Glow, which
ABrixLamps raises from 0 by day to its night value as the daylight goes. Matte (ART-002).
The asset is binary and kept in Git LFS (ADR-0002); this script is how it is made, so it can be made again instead of
edited by hand.

Run from the repository root (the engine path is this machine's, docs/setup/DEV_ENVIRONMENT.md):
  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\make_glow_material.py" -unattended -nullrhi -nosplash
"""
import unreal

PATH, NAME = "/Game/Greybox", "M_Glow"
ROUGHNESS = 0.85

lib = unreal.MaterialEditingLibrary
full = f"{PATH}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(full):   # made anew, as M_Greybox
    unreal.EditorAssetLibrary.delete_asset(full)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("used_with_instanced_static_meshes", True)

own = []
for i in range(3):
    c = lib.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -900, i * 120)
    c.set_editor_property("data_index", i)
    own.append(c)
rg = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -700, 40)
lib.connect_material_expressions(own[0], "", rg, "A")
lib.connect_material_expressions(own[1], "", rg, "B")
rgb = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -550, 80)
lib.connect_material_expressions(rg, "", rgb, "A")
lib.connect_material_expressions(own[2], "", rgb, "B")
lib.connect_material_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)

glow = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -550, 260)
glow.set_editor_property("parameter_name", "Glow")
glow.set_editor_property("default_value", 0.0)
lit = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 160)
lib.connect_material_expressions(rgb, "", lit, "A")
lib.connect_material_expressions(glow, "", lit, "B")
lib.connect_material_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 320)
rough.set_editor_property("r", ROUGHNESS)
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log("made " + full)
