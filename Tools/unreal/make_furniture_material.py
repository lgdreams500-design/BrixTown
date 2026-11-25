r"""Makes /Game/Furniture/M_Furniture: the one material of the furniture kit's meshes (FURN-002, FURN-003). Every face of
the pieces made by Tools/blender/unreal_furniture_37.py carries its kit material's colour as a vertex colour, so the
material is that colour on a matte surface, as ART-002 asks of every surface (and as /Game/People/M_People is).
The asset is binary and kept in Git LFS (ADR-0002); this script is how it is made, so it can be made again instead of
edited by hand. Run it before Tools/unreal/import_furniture_kit.py (which runs it itself when the asset is missing).

Run from the repository root (the engine path is this machine's, docs/setup/DEV_ENVIRONMENT.md):
  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\make_furniture_material.py" -unattended -nullrhi -nosplash
"""
import unreal

PATH, NAME = "/Game/Furniture", "M_Furniture"
ROUGHNESS = 0.85          # matte painted wood, cloth and plaster (ART-002)

lib = unreal.MaterialEditingLibrary
full = f"{PATH}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(full):   # made anew, as M_People (editing expressions headless crashes 5.8)
    unreal.EditorAssetLibrary.delete_asset(full)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("used_with_instanced_static_meshes", True)   # the town places its pieces as instances
mat.set_editor_property("used_with_nanite", True)

colour = lib.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -400, 0)
lib.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 200)
rough.set_editor_property("r", ROUGHNESS)
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log("made " + full)
