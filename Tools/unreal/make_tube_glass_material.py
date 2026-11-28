r"""Makes /Game/Greybox/M_TubeGlass: the clear glass of the travel tubes and their booths (CITY-114; ADR-0076, an exception
to W1 for the tubes only). As the approved reference draws it (Tools/blender/hours_50.py tube_glass(), VehiclesGlassTube):
a faint sea-green tint, smooth, nearly clear where you look straight through it and brighter at its edges, where the sky
is caught at a grazing angle. Parameters: Color (the tint, World/rules/tubes.json look.glass) and Alpha (how much it
covers looked at straight on, look.glass_alpha); the edges add Edge more (0.34, the reference's).
The asset is binary and kept in Git LFS (ADR-0002); this script is how it is made.
Run from the repository root (the engine path is this machine's, docs/setup/DEV_ENVIRONMENT.md):
  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\make_tube_glass_material.py" -unattended -nullrhi -nosplash
"""
import unreal

PATH, NAME = "/Game/Greybox", "M_TubeGlass"
lib = unreal.MaterialEditingLibrary
full = f"{PATH}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("two_sided", True)
mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
mat.set_editor_property("used_with_static_lighting", False)


def param(cls, name, x, y, value):
    p = lib.create_material_expression(mat, cls, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", value)
    return p


tint = param(unreal.MaterialExpressionVectorParameter, "Color", -700, 0, unreal.LinearColor(0.70, 0.86, 0.88, 1.0))
lib.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 160)
rough.set_editor_property("r", 0.08)
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
spec = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 240)
spec.set_editor_property("r", 0.5)
lib.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
# opacity: Alpha looked at straight on, Alpha + Edge at a grazing angle (the reference's Layer Weight "Facing")
alpha = param(unreal.MaterialExpressionScalarParameter, "Alpha", -900, 400, 0.16)
edge = param(unreal.MaterialExpressionScalarParameter, "Edge", -900, 500, 0.34)
fres = lib.create_material_expression(mat, unreal.MaterialExpressionFresnel, -900, 600)
fres.set_editor_property("exponent", 2.0)
fres.set_editor_property("base_reflect_fraction", 0.0)
mul = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, 520)
lib.connect_material_expressions(edge, "", mul, "A")
lib.connect_material_expressions(fres, "", mul, "B")
add = lib.create_material_expression(mat, unreal.MaterialExpressionAdd, -400, 450)
lib.connect_material_expressions(alpha, "", add, "A")
lib.connect_material_expressions(mul, "", add, "B")
sat = lib.create_material_expression(mat, unreal.MaterialExpressionSaturate, -250, 450)
lib.connect_material_expressions(add, "", sat, "")
lib.connect_material_property(sat, "", unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log("made " + full)
