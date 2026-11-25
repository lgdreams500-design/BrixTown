r"""Makes /Game/People/M_People: the material of the town's people (ART-004) and of its vehicles. Their bodies are built each
frame by BrixWorld's port of the slice's people.js (BrixBody), each vertex carrying its part's colour (skin, a coat, the
hair, an eye), so the material is that colour on a matte surface, as ART-002 asks of every surface.
Snow (WLD-070, ADR-0051): each mesh's custom primitive data 0 is how much snow lies on it (0 by default); it whitens what
faces up (a car's roof and bonnet, a head and shoulders) in patches, set by ABrixPeople and ABrixCars from the weather.
The asset is binary and kept in Git LFS (ADR-0002); this script is how it is made, so it can be made again instead of
edited by hand.

Run from the repository root (the engine path is this machine's, docs/setup/DEV_ENVIRONMENT.md):
  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\make_people_material.py" -unattended -nullrhi -nosplash
"""
import unreal

PATH, NAME = "/Game/People", "M_People"
ROUGHNESS = 0.8           # matte; skin and cloth alike (ART-002)
SNOW_COLOUR = (0.80, 0.88, 1.0)   # as on the town (greybox_wet.py)
SNOW_FACING = 0.5         # how much a face must face up to take snow
SNOW_PATCH_SCALE = 0.02   # the size of its patches (per centimetre)

lib = unreal.MaterialEditingLibrary
full = f"{PATH}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(full):   # made anew, as M_Greybox (editing expressions headless crashes 5.8)
    unreal.EditorAssetLibrary.delete_asset(full)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
# (if anything below fails, the asset is gone: git checkout -- Game/Content/People/M_People.uasset brings it back)


def node(cls, x, y, **props):
    n = lib.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def link(a, b, pin=""):
    lib.connect_material_expressions(a, "", b, pin)


colour = node(unreal.MaterialExpressionVertexColor, -900, 0)
snow = node(unreal.MaterialExpressionScalarParameter, -900, 300, parameter_name="Snow", default_value=0.0,
            use_custom_primitive_data=True, primitive_data_index=0)   # per mesh, set from C++
nrm = node(unreal.MaterialExpressionVertexNormalWS, -900, 400)
up = node(unreal.MaterialExpressionComponentMask, -750, 400, r=False, g=False, b=True, a=False)
link(nrm, up)
up5 = node(unreal.MaterialExpressionSubtract, -600, 400, const_b=SNOW_FACING)
link(up, up5, "A")
up3 = node(unreal.MaterialExpressionMultiply, -450, 400, const_b=3.0)
link(up5, up3, "A")
faces_up = node(unreal.MaterialExpressionSaturate, -300, 400)
link(up3, faces_up)
pos = node(unreal.MaterialExpressionWorldPosition, -900, 550)
patch = node(unreal.MaterialExpressionNoise, -750, 550, scale=SNOW_PATCH_SCALE, levels=2, output_min=0.0, output_max=1.0)
lib.connect_material_expressions(pos, "", patch, "Position")
snow16 = node(unreal.MaterialExpressionMultiply, -600, 650, const_b=1.6)
link(snow, snow16, "A")
reach = node(unreal.MaterialExpressionAdd, -450, 550)
link(patch, reach, "A")
link(snow16, reach, "B")
reach1 = node(unreal.MaterialExpressionSubtract, -300, 550, const_b=1.0)
link(reach, reach1, "A")
reach4 = node(unreal.MaterialExpressionMultiply, -150, 550, const_b=4.0)
link(reach1, reach4, "A")
reach_s = node(unreal.MaterialExpressionSaturate, 0, 550)
link(reach4, reach_s)
lies = node(unreal.MaterialExpressionMultiply, 150, 450)
link(reach_s, lies, "A")
link(faces_up, lies, "B")
white = node(unreal.MaterialExpressionConstant3Vector, 150, 200, constant=unreal.LinearColor(*SNOW_COLOUR, 1.0))
base = node(unreal.MaterialExpressionLinearInterpolate, 350, 0)
link(colour, base, "A")
link(white, base, "B")
link(lies, base, "Alpha")
lib.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = node(unreal.MaterialExpressionConstant, 150, 700, r=ROUGHNESS)
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log_warning("Brix Town: made " + full)
