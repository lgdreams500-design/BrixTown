# Art/Export

Files exported from Blender for import into Unreal. Regenerated from Art/Source by export scripts.

- `mall_goods_31/mall_goods_31.glb`: the mall's 49 goods for the home (FURN-065), the near make, one object each, named by its piece id in `World/rules/furnishing.json`. Written by `Tools/blender/mall_goods_31.py`.
- `unreal_furniture_37/<piece>_<line>.glb`: every piece of furniture kits 17 and 18 in each of the four lines (89 pieces x 4), one vertex-coloured mesh each, with `index.json` (file, box, triangles). Written by `Tools/blender/unreal_furniture_37.py` (about 20 s); imported into `/Game/Furniture` by `Tools/unreal/import_furniture_kit.py` (only the pieces the plans use). The .glb files are not kept in git, only `index.json` and the imported meshes (ADR-0050): run the script before importing again.
