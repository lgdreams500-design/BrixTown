"""Brix Town - car export 11: the "Soft box" cars (ADR-0015, replacing Classic) as low-poly meshes for the playable slice.

Headless:
    blender-launcher.exe -b --factory-startup --python C:/Projects/Wasted/Tools/blender/car_export_11.py
Builds on car_export_10.py (its lighter shapes, part tags, two levels of detail and file format) and through it on
car_kit_09.py (the option A builders). Adds a Soft box pickup for variety. Writes Prototype/play/car_models.js, replacing
the Classic models; car_export_10.py stays runnable to write those back.

The cabins are hollow wells with the seats on their floor and the roofs high enough for the tallest person (look test 45;
car_kit_09.py's well()). Parts as in car_export_10, plus the estate's roof load in its own colours: luggage (the case), bag, rail. All three come
and go together in the slice.
"""
import bpy, bmesh, os, re, json, traceback
from mathutils import Vector

B = "C:/Projects/Wasted/Tools/blender/"
_e = open(B + "car_export_10.py", encoding="utf-8").read().split("\ndef main():")[0]
exec(compile(_e, "car_export_10_helpers", "exec"), globals())

NAME = "car_export_11"
LOG = OUT + "/" + NAME + "_log.txt"
PARTS = PARTS + ["bag", "rail"]
TARGET = (3400, 900)          # most triangles per model: near, far (the round wheels and lamps cost more than Classic's; the hollow
                              # cabins of look test 45 another 100 or so near)


def a_pickup(P, col, roof):
    """A Soft box pickup: the estate's front and cab, and an open bed with crates."""
    L, W = 3.9, 1.7
    body = rbox(P, "A4_Body", col, 0, 0, 0.6, L, W, 0.64, 0.25)
    waist = rbox(P, "A4_Waist", col, 0.6, 0, 0.96, 1.5, W - 0.1, 0.16, 0.08)
    well(P, "A4", [body, waist], 0.07, 0.98, W / 2 - 0.15, 0.38)       # the cabin, as car_kit_09's (look test 45)
    posts_roof(P, "A4", 0.0, 1.2, W / 2 - 0.12, 1.0, 1.77, roof)
    rbox(P, "A4_Roof", roof, 0.58, 0, 1.81, 1.4, W - 0.06, 0.16, 0.08)
    rbox(P, "A4_CabBack", col, -0.05, 0, 1.305, 0.12, W - 0.1, 0.73, 0.05)
    for s in (-1, 1):
        rbox(P, "A4_Side%d" % s, col, -1.0, s * (W / 2 - 0.07), 1.06, 1.85, 0.14, 0.3, 0.06)
    rbox(P, "A4_Gate", col, -1.88, 0, 1.06, 0.14, W - 0.1, 0.3, 0.06)
    for i, (x, y, c) in enumerate(((-1.45, -0.3, M["wood"]), (-0.7, 0.28, M["wood"]), (-1.4, 0.35, M["cream"]))):
        rbox(P, "A4_Crate%d" % i, c, x, y, 1.12, 0.5, 0.46, 0.4, 0.05)
    rbox(P, "A4_Dash", M["leather"], 1.1, 0, 1.02, 0.3, W - 0.3, 0.14, 0.05)
    seats(P, "A4", [0.5], 0.35, 0.44, M["leather"]); steering(P, "A4", 1.0, -0.35, 1.1)
    lamps(P, "A4", L / 2 - 0.02, 0.68, [-0.55, 0.55], 0.13, M["lampc"])
    rbox(P, "A4_BumpF", M["hub"], L / 2 + 0.02, 0, 0.38, 0.14, W - 0.2, 0.14, 0.06)
    rbox(P, "A4_BumpB", M["hub"], -L / 2 - 0.02, 0, 0.38, 0.14, W - 0.2, 0.14, 0.06)
    wheels(P, "A4", [1.25, -1.25], W / 2, 0.32)


# seat tops: the seat cushion is centred at the height seats() is given and is 0.12 thick
MODELS = dict(
    small=dict(build=a_small, seat=[-0.35, -0.33], seat_y=0.50, back=None),
    estate=dict(build=a_estate, seat=[-0.05, -0.35], seat_y=0.50, back=-1.05),
    pickup=dict(build=a_pickup, seat=[0.5, -0.35], seat_y=0.50, back=None),
    van=dict(build=a_van, seat=[1.3, -0.42], seat_y=0.60, back=None),
)

BIG = ("_Body", "_Low", "_Load", "_Waist", "_Roof", "_CabRoof", "_Stripe", "_Band")   # the shapes the round edges show on


def whole(name):
    """Kept whole: the big rounded shapes, and every wheel, hub and lamp (round parts lose their shape when thinned)."""
    return any(s in name for s in BIG) or re.search(r"_W[0-9]|_L[0-9]", name) is not None
_rbox = rbox


def rbox(P, name, mat, cx, cy, cz, sx, sy, sz, r, seg=4):
    """Big shapes get the full round edge; seats, dashboards, bumpers and crates a lighter one."""
    big = any(s in name for s in BIG)
    return _rbox(P, name, mat, cx, cy, cz, sx, sy, sz, r, DETAIL["seg"] if big else 1)


def rod(P, name, mat, a, b, r):
    """A round post with fewer sides than in the look test."""
    a, b = Vector(a), Vector(b)
    d = b - a
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=6 if DETAIL["seg"] > 2 else 4, radius1=r, radius2=r, depth=d.length)
    ob = add_obj(name, bm, mat, loc=tuple((a + b) / 2))
    ob.rotation_mode = 'QUATERNION'
    ob.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())
    return put(smooth(ob), P)


def build(key, spec, lod):
    """As car_export_10's build, but the big body shapes keep their round edges (four bevel segments near, never
    decimated) and only the small parts are thinned to fit the budget."""
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    DETAIL.update(dict(sub=2, seg=4, u=10, v=6, wheel=12) if lod == 0 else dict(sub=1, seg=1, u=6, v=4, wheel=8))
    P = group("Car_" + key, 0, 0, 0)
    spec["build"](P, M["body"], M["roof"] if key != "van" else M["band"])
    drawn = lambda: [o for o in bpy.data.objects if o.type == 'MESH' and not o.hide_render]   # not the cabin's cutter
    objs = drawn()
    if lod == 1:
        for o in [o for o in objs if any(s in o.name for s in INSIDE)]:
            bpy.data.objects.remove(o, do_unlink=True)
        objs = drawn()
    big = [o for o in objs if whole(o.name)]
    small = [o for o in objs if o not in big]
    n_big, n_small = len(triangles(big)), len(triangles(small))
    log("  kept whole: " + ", ".join(sorted({o.name.split("_", 1)[1][:6] for o in big})))
    ratio = min(1.0, max(0.05, (TARGET[lod] - n_big) / max(1, n_small)))
    if ratio < 0.999:
        for o in small:
            md = o.modifiers.new("Fewer", 'DECIMATE')
            setp(md, "ratio", ratio)
    tris = triangles(objs)
    log("%s lod %d: %d triangles (body, wheels and lamps %d kept whole; other parts %d -> x%.2f)" % (key, lod, len(tris), n_big, n_small, ratio))
    return tris


_part_of = part_of


def part_of(ob, mat_name):
    n = ob.name
    if "_Case" in n:
        return "luggage"
    if "_Bag" in n:
        return "bag"
    if "_Rail" in n:
        return "rail"
    return _part_of(ob, mat_name)


def main():
    os.makedirs(OUT, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    try:
        new_scene("CarExport11")
        global K_M, M
        K_M = kit_mats()
        M = dict(K_M)
        for part in PARTS:
            M[part] = flat("P_" + part, (0.5, 0.5, 0.5))
        M.update(lampc=M["lamp"], terra=M["luggage"])
        out = dict(note="Generated by Tools/blender/car_export_11.py from the Soft box cars of car_kit_09.py (ADR-0015). Do not edit.",
                   look="soft_box", parts=PARTS, models={})
        for key, spec in MODELS.items():
            lods = [pack(build(key, spec, lod)) for lod in (0, 1)]
            xs, ws = lods[0]["p"][0::3], lods[0]["p"][1::3]
            out["models"][key] = dict(seat=spec["seat"], seat_y=spec["seat_y"], back=spec["back"],
                                      box=[round(min(xs), 2), round(max(xs), 2), round(min(ws), 2), round(max(ws), 2)], lods=lods)
        with open(DEST, "w", encoding="utf-8") as f:
            f.write("// " + out["note"] + "\nwindow.CAR_MODELS = " + json.dumps(out, separators=(",", ":")) + ";\n")
        log("wrote %s (%.0f KB)" % (DEST, os.path.getsize(DEST) / 1024))
    except Exception:
        log("FAILED:\n" + traceback.format_exc())
    log("DONE")


main()
