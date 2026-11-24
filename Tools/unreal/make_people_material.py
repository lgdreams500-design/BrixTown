r"""Makes /Game/People/M_People: the material of the town's people (ART-004). Their bodies are built each frame by
BrixWorld's port of the slice's people.js (BrixBody), each vertex carrying its part's colour (skin, a coat, the hair, an
eye), so the material is that colour on a matte surface, as ART-002 asks of every surface.
The asset is binary and kept in Git LFS (ADR-0002); this script is how it is made, so it can be made again instead of
edited by hand.

Run from the repository root (the engine path is this machine's, docs/setup/DEV_ENVIRONMENT.md):
  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\make_people_material.py" -unattended -nullrhi -nosplash
"""
import unreal

PATH, NAME = "/Game/People", "M_People"
ROUGHNESS = 0.8           # matte; skin and cloth alike (ART-002)

lib = unreal.MaterialEditingLibrary
full = f"{PATH}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(full):   # made anew, as M_Greybox (editing expressions headless crashes 5.8)
    unreal.EditorAssetLibrary.delete_asset(full)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())

colour = lib.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -400, 0)
lib.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 200)
rough.set_editor_property("r", ROUGHNESS)
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log("made " + full)
