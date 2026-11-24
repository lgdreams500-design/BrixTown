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
import os
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


def vec4(first, x, y, default0):
    """Custom data first..first+3 as one float4; the first one's default (no custom data) is default0."""
    parts = []
    for i in range(4):
        c = lib.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, x, y + i * 100)
        c.set_editor_property("data_index", first + i)
        if i == 0:
            c.set_editor_property("const_default_value", default0)
        parts.append(c)
    ab = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, x + 180, y)
    lib.connect_material_expressions(parts[0], "", ab, "A")
    lib.connect_material_expressions(parts[1], "", ab, "B")
    abc = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, x + 330, y + 60)
    lib.connect_material_expressions(ab, "", abc, "A")
    lib.connect_material_expressions(parts[2], "", abc, "B")
    abcd = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, x + 480, y + 120)
    lib.connect_material_expressions(abc, "", abcd, "A")
    lib.connect_material_expressions(parts[3], "", abcd, "B")
    return abcd


# The surface: stone, brick, paving or painted plaster, by the slice's own rules (greybox_surface.hlsl, ADR-0017).
# Custom data 3..6 = (type, unit w, unit h, joint), 7..10 = (vary, bevel, dome, mortar); with none, type is -1: plaster.
p0 = vec4(3, -1500, 500, -1.0)
p1 = vec4(7, -1500, 950, 0.0)
pos = lib.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -600, 450)
nrm = lib.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -600, 560)
cam = lib.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -600, 660)
surface = lib.create_material_expression(mat, unreal.MaterialExpressionCustom, 0, 300)
code = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "greybox_surface.hlsl"), encoding="utf-8").read()
surface.set_editor_property("code", code)
surface.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
surface.set_editor_property("description", "Brix surface")
ins = []
for n in ("Pos", "N", "Base", "P0", "P1", "Cam"):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", n)
    ins.append(ci)
surface.set_editor_property("inputs", ins)
for src, name in ((pos, "Pos"), (nrm, "N"), (pick, "Base"), (p0, "P0"), (p1, "P1"), (cam, "Cam")):
    lib.connect_material_expressions(src, "", surface, name)
lib.connect_material_property(surface, "", unreal.MaterialProperty.MP_BASE_COLOR)

rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 220)
rough.set_editor_property("r", ROUGHNESS)
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log_warning(f"Brix Town: made {full}")
