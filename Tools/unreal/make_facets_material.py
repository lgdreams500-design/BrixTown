r"""Makes /Game/Greybox/M_Facets: the material of the town's faceted things that the slice draws face by face (CITY-017,
WLD-056, ART-001): the crowns of bushes (play.js dome()), the tufts in a lawn, the tracks and ponds of the open ground.
Their meshes are made by the game at load time (ABrixTownActor), each face carrying its shade or colour in its vertices, so
the material is the vertex colour times the instance's own colour (custom data 0..2; 1 when the mesh is not instanced), on
a matte surface (ART-002), seen from both sides (thin spikes of grass).
The asset is binary and kept in Git LFS (ADR-0002); this script is how it is made, so it can be made again instead of
edited by hand.

Run from the repository root (the engine path is this machine's, docs/setup/DEV_ENVIRONMENT.md):
  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\make_facets_material.py" -unattended -nullrhi -nosplash
"""
import unreal

PATH, NAME = "/Game/Greybox", "M_Facets"
ROUGHNESS = 0.85          # matte (ART-002), as M_Greybox

lib = unreal.MaterialEditingLibrary
full = f"{PATH}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(full):   # made anew, as M_Greybox (editing expressions headless crashes 5.8)
    unreal.EditorAssetLibrary.delete_asset(full)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("two_sided", True)
mat.set_editor_property("used_with_instanced_static_meshes", True)   # the crowns are instances, one group a mesh

vc = lib.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -900, 0)
own = []
for i in range(3):                     # the instance's colour, 1 when there is none
    c = lib.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -900, 200 + i * 120)
    c.set_editor_property("data_index", i)
    c.set_editor_property("const_default_value", 1.0)
    own.append(c)
rg = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -650, 250)
lib.connect_material_expressions(own[0], "", rg, "A")
lib.connect_material_expressions(own[1], "", rg, "B")
rgb = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -450, 300)
lib.connect_material_expressions(rg, "", rgb, "A")
lib.connect_material_expressions(own[2], "", rgb, "B")
mul = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 100)
lib.connect_material_expressions(vc, "", mul, "A")
lib.connect_material_expressions(rgb, "", mul, "B")
lib.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -250, 300)
rough.set_editor_property("r", ROUGHNESS)
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log_warning("Brix Town: made " + full)
