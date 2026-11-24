"""Brix Town - furniture sizes 21: builds every home piece of furniture kits 17 and 18 in each of the four lines and
writes its measured size, so rooms can be laid out to fit (FURN-001 footprints; feeds house_interiors_21.py).

Run headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/furniture_sizes_21.py
From another checkout: WASTED_ROOT=C:/Projects/Wasted-remodelbuilds
Writes Art/StyleTests/house_interiors_21/furniture_sizes.json:
    {piece: {line: [x0, x1, y0, y1, z1]}}   metres about the piece's own origin, yaw 0 (its back towards +y)
Nothing is rendered.
"""
import os, json
from mathutils import Vector

ROOT = os.environ.get("WASTED_ROOT", "C:/Projects/Wasted").replace("\\", "/").rstrip("/")
_k = open(ROOT + "/Tools/blender/furniture_kit_18.py", encoding="utf-8").read()
_k = _k[:_k.rstrip().rfind("\nmain()")]                   # kit 18's pieces (and kit 17's), without running it
exec(compile(_k, "furniture_kit_18_helpers", "exec"), globals())

OUT_JSON = OUT + "/house_interiors_21/furniture_sizes.json"
HOME_KITS = ("kitchen", "bathroom", "dining", "study", "children", "hallway", "garden", "bedroom", "living", "decor")
K17 = ("sofa", "armchair", "coffee", "dining", "bed", "bedside", "wardrobe", "bookcase", "floor_lamp", "rug")


def measure(spec, L, rng):
    P = group("Measure_" + spec.replace(":", "_"), 0.0, 0.0, 0.0)
    before = set(bpy.data.objects)
    place(spec, P, L, rng)
    bpy.context.view_layer.update()
    lo, hi = [1e9] * 3, [-1e9] * 3
    for ob in set(bpy.data.objects) - before:
        if ob.type != 'MESH':
            continue
        for c in ob.bound_box:
            v = ob.matrix_world @ Vector(c)
            for k in range(3):
                lo[k], hi[k] = min(lo[k], v[k]), max(hi[k], v[k])
    for ob in set(bpy.data.objects) - before:
        bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.objects.remove(P, do_unlink=True)
    return [round(lo[0], 3), round(hi[0], 3), round(lo[1], 3), round(hi[1], 3), round(hi[2], 3)]


def main():
    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    fresh("Measure")
    extra_mats()
    Ls = lines()
    specs = ["k17:" + k for k in K17] + [fn.__name__ for key, _, fns in KITS if key in HOME_KITS for fn in fns]
    out = {}
    for spec in specs:
        out[spec] = {}
        for L in Ls:
            try:
                out[spec][L["key"]] = measure(spec, L, random.Random(21))
            except Exception:
                out[spec][L["key"]] = None
                print("FAILED", spec, L["key"], traceback.format_exc())
    with open(OUT_JSON, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1)
    print("wrote", OUT_JSON, len(out), "pieces")


main()
