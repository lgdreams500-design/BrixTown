r"""Makes /Game/Weather/M_Clouds: the clouds overhead (WLD-035, WLD-036; ADR-0014), drawn on the inside of a dome round
the camera by the code in clouds.hlsl. Translucent and unlit: their colour comes from the sun's and the sky's, which
ABrixWeather sets with where they are (Clouds = cover, soft, size, phase; Offset; from the clock and World/rules/sky.json)
and the sun's place, each frame. Brightness scales it all to the picture's exposure.
The asset is binary and kept in Git LFS (ADR-0002); this script is how it is made.

  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\make_clouds_material.py" -unattended -nullrhi -nosplash
"""
import os
import unreal

PATH, NAME = "/Game/Weather", "M_Clouds"
lib = unreal.MaterialEditingLibrary
full = f"{PATH}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)


def node(cls, x, y, **props):
    n = lib.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def vparam(name, x, y, default):
    return node(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*default))


def mask(src, x, y, n):
    m = node(unreal.MaterialExpressionComponentMask, x, y, r=True, g=n > 1, b=n > 2, a=False)
    lib.connect_material_expressions(src, "", m, "")
    return m


cam = node(unreal.MaterialExpressionCameraVectorWS, -900, -200)
dirn = node(unreal.MaterialExpressionMultiply, -700, -200, const_b=-1.0)   # from the camera out to the sky
lib.connect_material_expressions(cam, "", dirn, "A")
cl = vparam("Clouds", -900, 0, (0.4, 0.07, 2.4, 0.0))
off = vparam("Offset", -900, 150, (0.0, 0.0, 0.0, 0.0))
sun = vparam("Sun", -900, 300, (0.0, 0.0, 1.0, 0.0))
sun_col = vparam("SunColour", -900, 450, (1.0, 0.9, 0.8, 1.0))
zen = vparam("Zenith", -900, 600, (0.3, 0.45, 0.65, 1.0))
hor = vparam("Horizon", -900, 750, (0.7, 0.72, 0.75, 1.0))
ls = vparam("LitShade", -900, 900, (0.42, 0.8, 0.0, 0.0))
c = node(unreal.MaterialExpressionCustom, -300, 300, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4, description="Brix clouds",
         code=open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "clouds.hlsl"), encoding="utf-8").read())
ins = []
for n in ("Dir", "Cl", "Off", "Sun", "SunCol", "Zen", "Hor", "LitShade"):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", n)
    ins.append(ci)
c.set_editor_property("inputs", ins)
lib.connect_material_expressions(cl, "RGBA", c, "Cl")   # all four: cover, soft, size and the shape's phase
for src, name in ((dirn, "Dir"), (mask(off, -700, 150, 2), "Off"), (mask(sun, -700, 300, 3), "Sun"), (mask(sun_col, -700, 450, 3), "SunCol"),
                  (mask(zen, -700, 600, 3), "Zen"), (mask(hor, -700, 750, 3), "Hor"), (mask(ls, -700, 900, 2), "LitShade")):
    lib.connect_material_expressions(src, "", c, name)
bright = node(unreal.MaterialExpressionScalarParameter, -100, 500, parameter_name="Brightness", default_value=1.0)
rgb = mask(c, -100, 300, 3)
lit = node(unreal.MaterialExpressionMultiply, 100, 300)
lib.connect_material_expressions(rgb, "", lit, "A")
lib.connect_material_expressions(bright, "", lit, "B")
lib.connect_material_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
alpha = node(unreal.MaterialExpressionComponentMask, -100, 650, r=False, g=False, b=False, a=True)
lib.connect_material_expressions(c, "", alpha, "")
lib.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log_warning("Brix Town: made " + full)
