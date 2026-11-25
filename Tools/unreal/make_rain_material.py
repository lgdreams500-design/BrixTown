r"""Makes /Game/Weather/M_Rain: the falling rain (WLD-037, ADR-0016). Translucent and unlit: each drop's colour and
how much it shows are its vertices' colour and alpha, which ABrixWeather sets from the light and how hard it rains
(World/rules/sky.json "rain": alpha). Used on one procedural mesh of drops round the camera.
The asset is binary and kept in Git LFS (ADR-0002); this script is how it is made.

Run from the repository root (the engine path is this machine's, docs/setup/DEV_ENVIRONMENT.md):
  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\make_rain_material.py" -unattended -nullrhi -nosplash
"""
import unreal

PATH, NAME = "/Game/Weather", "M_Rain"
lib = unreal.MaterialEditingLibrary
full = f"{PATH}/{NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)
vc = lib.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -500, 0)
lib.connect_material_property(vc, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.connect_material_property(vc, "A", unreal.MaterialProperty.MP_OPACITY)   # the vertex colour's alpha
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log_warning("Brix Town: made " + full)
