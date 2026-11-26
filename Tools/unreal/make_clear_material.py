r"""Makes /Game/Greybox/M_Clear: the glass of a window you can see through (CITY-013). The kit's windows (Tools/blender/
unreal_kit_36.py) have their glass as a dark box, right for a window in front of a building's solid core; the windows of an
enterable building's hollow storey draw that slot with this instead, so the room looks out at the street and the sun
comes in through them (Art/Retired/interior_room_daylight.png). Masked to nothing: it is never drawn and casts no shadow.
The asset is binary and kept in Git LFS (ADR-0002); this script is how it is made.
Run from the repository root (the engine path is this machine's, docs/setup/DEV_ENVIRONMENT.md):
  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\make_clear_material.py" -unattended -nullrhi -nosplash
"""
import unreal

PATH, NAME = "/Game/Greybox", "M_Clear"
lib = unreal.MaterialEditingLibrary
full = f"{PATH}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("used_with_instanced_static_meshes", True)
mat.set_editor_property("used_with_nanite", True)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
zero = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 200)
zero.set_editor_property("r", 0.0)
lib.connect_material_property(zero, "", unreal.MaterialProperty.MP_OPACITY_MASK)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log("made " + full)
