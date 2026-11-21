"""Wasted - car export 10: the chosen "Classic" cars (ADR-0011) as low-poly meshes for the playable slice.

Headless:
    blender-launcher.exe -b --factory-startup --python C:/Projects/Wasted/Tools/blender/car_export_10.py
Builds on car_kit_09.py (its option C builders and shape helpers) and adds a classic pickup for variety. Writes
Prototype/play/car_models.js: for each model, two levels of detail (near and far), every triangle tagged with a part
(body, roof, band, tyre, hub, leather, lamp, cream, iron, wood, luggage, spare, cargo) so the slice can paint each car in
its own colours and leave extras off. Also the seat places and the size of each car.

Axes in the file: a along the car (front +), w across it (right +, so the driver sits at w < 0), y up; metres.
"""
import bpy, bmesh, math, os, json, time, traceback
from mathutils import Vector

B = "C:/Projects/Wasted/Tools/blender/"
_k = open(B + "car_kit_09.py", encoding="utf-8").read().split("\n# ---- the scene")[0]
exec(compile(_k, "car_kit_09_helpers", "exec"), globals())

NAME = "car_export_10"
LOG = OUT + "/" + NAME + "_log.txt"
DEST = "C:/Projects/Wasted/Prototype/play/car_models.js"
PARTS = ["body", "roof", "band", "tyre", "hub", "leather", "lamp", "cream", "iron", "wood", "luggage", "spare", "cargo"]
TARGET = (2600, 420)          # most triangles per model: near, far
INSIDE = ("_S", "_B0", "_B1", "_Wheel", "_Dash")   # seats, steering wheel, dashboard: left out of the far model


def c_pickup(P, col, roof):
    """A classic pickup: the small car's front and cab, and an open bed with crates."""
    Wb = 1.3
    rbox(P, "C4_Cab", col, 0.05, 0, 0.76, 1.3, Wb, 0.66, 0.22)
    rbox(P, "C4_Bonnet", col, 1.15, 0, 0.84, 1.15, Wb - 0.25, 0.52, 0.22)
    ball(P, "C4_Grille", M["cream"], 1.74, 0, 0.8, 0.12, 0.66, 0.56)
    for s in (-1, 1):
        fender(P, "C4_FF%d" % s, col, 1.15, s * 0.8, 0.33, 1.35, True)
        fender(P, "C4_FB%d" % s, col, -1.35, s * 0.8, 0.33, 1.25, False)
        rbox(P, "C4_Board%d" % s, M["iron"], -0.1, s * 0.8, 0.37, 1.4, 0.3, 0.07, 0.03)
        ball(P, "C4_Lamp%d" % s, M["lampc"], 1.42, s * 0.64, 1.06, 0.24, 0.26, 0.26)
        rbox(P, "C4_Side%d" % s, col, -1.35, s * 0.62, 0.86, 1.55, 0.1, 0.5, 0.04)
    posts_roof(P, "C4", -0.5, 0.6, Wb / 2 - 0.06, 1.06, 1.72, roof, r=0.05, lean_front=0.18)
    blob(P, "C4_Roof", roof, 0.03, 0, 1.78, 1.4, Wb + 0.24, 0.24, top=(0.9, 0.85))
    rbox(P, "C4_CabBack", col, -0.58, 0, 1.0, 0.1, Wb - 0.08, 0.9, 0.04)
    rbox(P, "C4_Bed", col, -1.35, 0, 0.62, 1.55, Wb + 0.1, 0.1, 0.03)
    rbox(P, "C4_Gate", col, -2.1, 0, 0.86, 0.08, Wb + 0.1, 0.5, 0.03)
    for i, (x, y, c) in enumerate(((-1.75, -0.28, M["wood"]), (-1.05, 0.25, M["wood"]), (-1.6, 0.3, M["cream"]))):
        rbox(P, "C4_Crate%d" % i, c, x, y, 0.88, 0.5, 0.48, 0.42, 0.04)
    seats(P, "C4", [0.05], 0.3, 0.82, M["leather"]); steering(P, "C4", 0.5, -0.3, 1.16)
    wheels(P, "C4", [1.15, -1.35], 0.9, 0.33, 0.22)


# model key: builder, seat [along, across (driver, left)], seat top height, back-seat row or None
MODELS = dict(
    small=dict(build=c_small, seat=[-0.3, -0.27], seat_y=0.86, back=None),
    estate=dict(build=c_estate, seat=[0.1, -0.3], seat_y=0.88, back=-0.95),
    pickup=dict(build=c_pickup, seat=[0.05, -0.3], seat_y=0.88, back=None),
    van=dict(build=c_van, seat=[0.92, -0.36], seat_y=0.96, back=None),
)


# ---- lighter shapes for the game: fewer segments, less subdivision ------------------------------------------
DETAIL = dict(sub=2, seg=2, u=16, v=9, wheel=16)
_subsurf, _bevelled = subsurf, bevelled


def subsurf(ob, levels=3):
    return _subsurf(ob, min(levels, DETAIL["sub"]))


def bevelled(ob, width, segments=4):
    return _bevelled(ob, width, min(segments, DETAIL["seg"]))


def ball(P, name, mat, cx, cy, cz, sx, sy, sz):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=DETAIL["u"], v_segments=DETAIL["v"], radius=0.5)
    bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    return put(smooth(add_obj(name, bm, mat, loc=(cx, cy, cz))), P)


def dome(P, name, mat, cx, cy, cz, sx, sy, sz):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=DETAIL["u"] + 4, v_segments=DETAIL["v"] + 1, radius=0.5)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -0.02], context='VERTS')
    bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    return put(smooth(add_obj(name, bm, mat, loc=(cx, cy, cz))), P)     # single-sided: the slice draws both faces


def wheel(P, name, x, y, r, w):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=DETAIL["wheel"], radius1=r, radius2=r, depth=w)
    tyre = put(bevelled(add_obj(name, bm, M["tyre"], loc=(x, y, r), rot=(math.pi / 2, 0, 0)), w * 0.38, 2), P)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=max(8, DETAIL["wheel"] - 4), radius1=r * 0.46, radius2=r * 0.4, depth=w + 0.04)
    put(add_obj(name + "_Hub", bm, M["hub"], loc=(x, y, r), rot=(math.pi / 2, 0, 0)), P)
    return tyre


# ---- export ----------------------------------------------------------------------------------------------------
def group(name, x, y, yaw):
    e = bpy.data.objects.new(name, None)
    COL.objects.link(e)
    e.location = (x, y, 0)
    e.rotation_euler = (0, 0, yaw)
    return e


def part_of(ob, mat_name):
    n = ob.name
    if "_Spare" in n:
        return "spare"
    if "_Trunk" in n or "_Rail" in n:
        return "luggage"
    if "_Crate" in n:
        return "cargo"
    return mat_name[2:] if mat_name.startswith("P_") else "body"


def triangles(objs):
    dg = bpy.context.evaluated_depsgraph_get()
    out = []
    for ob in objs:
        ev = ob.evaluated_get(dg)
        me = ev.to_mesh()
        me.calc_loop_triangles()
        mw = ob.matrix_world
        nm = mw.to_3x3().inverted().transposed()
        cn = me.corner_normals
        for tri in me.loop_triangles:
            mat = ev.material_slots[tri.material_index].material.name if ev.material_slots else "P_body"
            part = PARTS.index(part_of(ob, mat))
            vs = []
            for l in tri.loops:
                p = mw @ me.vertices[me.loops[l].vertex_index].co
                n = (nm @ cn[l].vector).normalized()
                vs.append((p, n))
            out.append((vs, part))
        ev.to_mesh_clear()
    return out


def build(key, spec, lod):
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    DETAIL.update(dict(sub=2, seg=2, u=16, v=9, wheel=16) if lod == 0 else dict(sub=1, seg=1, u=8, v=5, wheel=8))
    P = group("Car_" + key, 0, 0, 0)
    spec["build"](P, M["body"], M["roof"] if key != "van" else M["band"])
    objs = [o for o in bpy.data.objects if o.type == 'MESH']
    if lod == 1:
        for o in [o for o in objs if any(s in o.name for s in INSIDE)]:
            bpy.data.objects.remove(o, do_unlink=True)
        objs = [o for o in bpy.data.objects if o.type == 'MESH']
    n = len(triangles(objs))
    ratio = min(1.0, TARGET[lod] / max(1, n))
    if ratio < 0.999:
        for o in objs:
            md = o.modifiers.new("Fewer", 'DECIMATE')
            setp(md, "ratio", ratio)
    tris = triangles(objs)
    log("%s lod %d: %d triangles (from %d)" % (key, lod, len(tris), n))
    return tris


def pack(tris):
    p, nn, t = [], [], []
    for vs, part in tris:
        for (v, n) in vs:
            p += [round(v.x, 3), round(v.y, 3), round(v.z, 3)]          # a, w, y: Blender x forward, y (mirrored) across, z up
            nn += [int(round(n.x * 100)), int(round(n.y * 100)), int(round(n.z * 100))]
        t.append(part)
    return dict(p=p, n=nn, t=t)


def main():
    os.makedirs(OUT, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    try:
        new_scene("CarExport10")
        global K_M, M
        K_M = kit_mats()
        M = dict(K_M)
        for part in PARTS:
            M[part] = flat("P_" + part, (0.5, 0.5, 0.5))
        M.update(lampc=M["lamp"], tyre=M["tyre"], hub=M["hub"], leather=M["leather"], cream=M["cream"], iron=M["iron"], wood=M["wood"])
        out = dict(note="Generated by Tools/blender/car_export_10.py from the Classic cars of car_kit_09.py (ADR-0011). Do not edit.",
                   parts=PARTS, models={})
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
