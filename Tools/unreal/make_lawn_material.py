r"""Makes /Game/Greybox/M_Lawn: a lawn's own pattern (ART-001, CITY-017), the slice's material 24: muted patches of the
three tones, the mower's faint stripes, clumps of clover, blades close up and daisies in drifts, painted (W1), by the
code in lawn.hlsl. The lawn's faces are in the town's facet meshes (ABrixTownActor) in their own section; each face's
colour (the grass, or the darker band along the edge) comes in its vertices, as in M_Facets, and the numbers come from
World/rules/vegetation.json "lawn" through Game/Data/streets.json (T0, T1, T2, Lawn, Daisy, set by the game). Matte
(ART-002).
The asset is binary and kept in Git LFS (ADR-0002); this script is how it is made, so it can be made again instead of
edited by hand.

  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\make_lawn_material.py" -unattended -nullrhi -nosplash
"""
import os
import unreal

PATH, NAME = "/Game/Greybox", "M_Lawn"
ROUGHNESS = 0.85          # matte (ART-002), as M_Facets

lib = unreal.MaterialEditingLibrary
full = f"{PATH}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(full):   # made anew (editing expressions headless crashes 5.8)
    unreal.EditorAssetLibrary.delete_asset(full)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
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


pos = node(unreal.MaterialExpressionWorldPosition, -900, -200)
cam = node(unreal.MaterialExpressionCameraPositionWS, -900, -80)
vc = node(unreal.MaterialExpressionVertexColor, -900, 40)
t0 = vparam("T0", -900, 200, (0.18, 0.27, 0.13, 1))
t1 = vparam("T1", -900, 350, (0.29, 0.34, 0.19, 1))
t2 = vparam("T2", -900, 500, (0.42, 0.39, 0.21, 1))
lw = vparam("Lawn", -900, 650, (8.0, 2.2, 0.06, 0.5))        # patch_m, stripe_m, stripe, clover
dz = vparam("Daisy", -900, 800, (0.72, 0.69, 0.58, 0.05))   # daisy_color, daisies
c = node(unreal.MaterialExpressionCustom, -300, 200, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Brix lawn",
         code=open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "lawn.hlsl"), encoding="utf-8").read())
ins = []
for n in ("Pos", "Cam", "Base", "T0", "T1", "T2", "L", "Daisy"):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", n)
    ins.append(ci)
c.set_editor_property("inputs", ins)
for src, name in ((pos, "Pos"), (cam, "Cam"), (mask(vc, -700, 40, 3), "Base"), (mask(t0, -700, 200, 3), "T0"), (mask(t1, -700, 350, 3), "T1"),
                  (mask(t2, -700, 500, 3), "T2")):
    lib.connect_material_expressions(src, "", c, name)
lib.connect_material_expressions(lw, "RGBA", c, "L")
lib.connect_material_expressions(dz, "RGBA", c, "Daisy")
lib.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = node(unreal.MaterialExpressionConstant, -100, 400, r=ROUGHNESS)
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log_warning("Brix Town: made " + full)
