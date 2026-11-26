"""Brix Town - car kit 09: three directions for the cars, with curves instead of boxes (CITY-067, look test; ART W1-W4).

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/car_kit_09.py").read(), "car_kit_09.py", "exec"))
or headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/car_kit_09.py
Quick checks: WASTED_ONLY=A,B WASTED_SAMPLES=24 WASTED_SCALE=50

Builds on building_kit_07.py (and through it old_town_04.py and style_test_02.py): the same painted matte materials,
the kit daylight (ADR-0007) and option B people for scale. Vehicles are "not started" in ART_DIRECTION 5; this is the
first look test for them. Each option is a family of three: a small car, an estate and a delivery van, in the palette's
accents with cream roofs. Cabins stay open on round posts so the people inside can be seen (the slice needs that).

    A  Soft box   today's slice shapes with generously rounded edges and corners; the closest to now
    B  Pebble     each body is one or two smooth pillowy forms with a bubble roof; the most toy-like
    C  Classic    separate round fenders over the wheels, a rounded bonnet, running boards, lamps on the fenders

Renders go to Art/StyleTests/car_kit_09_<option>.png. Not approved: for the owner to choose from.
"""
import bpy, bmesh, math, random, os, time, traceback
from mathutils import Vector, Euler, Matrix

B = "C:/Projects/Wasted/Tools/blender/"
_k = open(B + "building_kit_07.py", encoding="utf-8").read().split("\n# ---- the street and the scenes")[0]
exec(compile(_k, "building_kit_07_helpers", "exec"), globals())

NAME = "car_kit_09"
LOG = OUT + "/" + NAME + "_log.txt"
ONLY = tuple(os.environ["WASTED_ONLY"].split(",")) if os.environ.get("WASTED_ONLY") else None
SAMPLES = int(os.environ.get("WASTED_SAMPLES", 64))
SCALE = int(os.environ.get("WASTED_SCALE", 100))
GAP = 40.0                    # option groups stand this far apart along x, each with its own camera
DUSTY = (0.30, 0.38, 0.52)
ROSE = (0.55, 0.40, 0.34)

M = {}


# ---- shapes -------------------------------------------------------------------------------------------------
def smooth(ob):
    for p in ob.data.polygons:
        p.use_smooth = True
    return ob


def bevelled(ob, width, segments=4):
    md = ob.modifiers.new("Round", 'BEVEL')
    setp(md, "width", width)
    setp(md, "segments", segments)
    setp(md, "limit_method", 'NONE')
    return smooth(ob)


def subsurf(ob, levels=3):
    md = ob.modifiers.new("Soft", 'SUBSURF')
    setp(md, "levels", levels)
    setp(md, "render_levels", levels)
    return smooth(ob)


def put(ob, parent):
    ob.parent = parent
    return ob


def rbox(P, name, mat, cx, cy, cz, sx, sy, sz, r, seg=4):
    """A box with every edge rounded to radius r (option A's building block)."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    ob = add_obj(name, bm, mat, loc=(cx, cy, cz))
    return put(bevelled(ob, min(r, sx / 2.05, sy / 2.05, sz / 2.05), seg), P)


def blob(P, name, mat, cx, cy, cz, sx, sy, sz, top=(1.0, 1.0), bottom=(1.0, 1.0), nose=0.0, tail=0.0, lean=0.0, levels=3):
    """A pillow: a subdivided cage, so the surface is smooth all over (option B). The cage is the size given; the
    smooth surface sits a little inside it. top/bottom narrow the upper/lower faces, nose/tail lower the front/back of
    the top, lean slides the top backwards."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=1, use_grid_fill=True)
    for v in bm.verts:
        x, y, z = v.co
        if z > 0.2:
            x, y = x * top[0] - lean, y * top[1]
            if x > 0.2:
                z -= nose
            if x < -0.2:
                z -= tail
        if z < -0.2:
            x, y = x * bottom[0], y * bottom[1]
        v.co = Vector((x * sx, y * sy, z * sz))
    ob = add_obj(name, bm, mat, loc=(cx, cy, cz))
    return put(subsurf(ob, levels), P)


def ball(P, name, mat, cx, cy, cz, sx, sy, sz):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=14, radius=0.5)
    bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    return put(smooth(add_obj(name, bm, mat, loc=(cx, cy, cz))), P)


def rod(P, name, mat, a, b, r):
    """A round post from point a to point b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=14, radius1=r, radius2=r, depth=d.length)
    ob = add_obj(name, bm, mat, loc=tuple((a + b) / 2))
    ob.rotation_mode = 'QUATERNION'
    ob.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())
    return put(bevelled(ob, r * 0.5, 2), P)


def wheel(P, name, x, y, r, w):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=32, radius1=r, radius2=r, depth=w)
    tyre = put(bevelled(add_obj(name, bm, M["tyre"], loc=(x, y, r), rot=(math.pi / 2, 0, 0)), w * 0.38, 4), P)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=24, radius1=r * 0.46, radius2=r * 0.4, depth=w + 0.04)
    put(bevelled(add_obj(name + "_Hub", bm, M["hub"], loc=(x, y, r), rot=(math.pi / 2, 0, 0)), 0.02, 2), P)
    return tyre


def wheels(P, n, xs, half_w, r, w=0.24):
    for i, x in enumerate(xs):
        for s in (-1, 1):
            wheel(P, "%s_W%d%d" % (n, i, s), x, s * (half_w - w * 0.3), r, w)


def lamps(P, n, x, z, ys, r, mat):
    for i, y in enumerate(ys):
        ball(P, "%s_L%d" % (n, i), mat, x, y, z, r * 0.9, r * 2, r * 2)


def posts_roof(P, n, x0, x1, y, z0, z1, mat, r=0.055, lean_front=0.18):
    """Four round posts (the front pair raked back like a windscreen) and what they hold up."""
    for s in (-1, 1):
        rod(P, n + "_PF%d" % s, mat, (x1, s * y, z0), (x1 - lean_front, s * y * 0.97, z1), r)
        rod(P, n + "_PB%d" % s, mat, (x0, s * y, z0), (x0 + 0.05, s * y * 0.97, z1), r)


def seats(P, n, xs, y, z, mat):
    for i, x in enumerate(xs):
        for s in (-1, 1):
            rbox(P, "%s_S%d%d" % (n, i, s), mat, x - 0.25, s * y, z + 0.28, 0.14, 0.5, 0.56, 0.06)
            rbox(P, "%s_B%d%d" % (n, i, s), mat, x, s * y, z, 0.5, 0.5, 0.12, 0.05)


def steering(P, n, x, y, z):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=20, radius1=0.19, radius2=0.19, depth=0.04)
    ob = add_obj(n + "_Wheel", bm, M["iron"], loc=(x, y, z), rot=(0, math.radians(-60), 0))
    put(bevelled(ob, 0.015, 2), P)


def well(P, n, shapes, x0, x1, half_w, z0, z1=1.5, r=0.08):
    """Hollow the cabin: a rounded well cut out of each of the shapes, from x0 to x1 along the car and down to the floor
    at z0, so the seats and whoever sits on them show from above and through the open sides (look test 45: before, the
    body and the waist were solid over the cabin, a board across everyone's lap). The cutter is not drawn."""
    cut = rbox(P, n + "_Cut", M["hub"], (x0 + x1) / 2, 0, (z0 + z1) / 2, x1 - x0, 2 * half_w, z1 - z0, r)
    cut.hide_render = True
    cut.display_type = 'WIRE'
    for ob in shapes:
        md = ob.modifiers.new("Well", 'BOOLEAN')
        setp(md, "operation", 'DIFFERENCE')
        setp(md, "solver", 'EXACT')
        md.object = cut
    return cut


# ---- option A: soft box ----------------------------------------------------------------------------------------
# The cabins are wells in the body with the seats on their floor (seat tops 0.50 m, the van's 0.60 m), under roofs whose
# undersides are 1.73 m (the van's 1.84 m): the tallest person (1.80 m, under an afro: 1.17 m of seated head) clears them by
# 5 cm (look test 45).
def a_small(P, col, roof):
    L, W = 3.0, 1.56
    body = rbox(P, "A1_Body", col, 0, 0, 0.58, L, W, 0.62, 0.26)
    waist = rbox(P, "A1_Waist", col, -0.15, 0, 0.92, L - 0.8, W - 0.1, 0.16, 0.08)
    well(P, "A1", [body, waist], -0.75, 0.45, W / 2 - 0.15, 0.38)
    posts_roof(P, "A1", -1.0, 0.55, W / 2 - 0.12, 0.95, 1.77, roof)
    rbox(P, "A1_Roof", roof, -0.25, 0, 1.81, 1.8, W - 0.06, 0.16, 0.08)
    rbox(P, "A1_Dash", M["leather"], 0.42, 0, 0.98, 0.3, W - 0.3, 0.14, 0.05)
    seats(P, "A1", [-0.35], 0.33, 0.44, M["leather"]); steering(P, "A1", 0.25, -0.33, 1.08)
    lamps(P, "A1", L / 2 - 0.02, 0.66, [-0.48, 0.48], 0.13, M["lampc"])
    rbox(P, "A1_BumpF", M["hub"], L / 2 + 0.02, 0, 0.36, 0.14, W - 0.2, 0.14, 0.06)
    rbox(P, "A1_BumpB", M["hub"], -L / 2 - 0.02, 0, 0.36, 0.14, W - 0.2, 0.14, 0.06)
    wheels(P, "A1", [0.9, -0.9], W / 2, 0.31)


def a_estate(P, col, roof):
    L, W = 4.1, 1.7
    body = rbox(P, "A2_Body", col, 0, 0, 0.6, L, W, 0.64, 0.25)
    stripe = rbox(P, "A2_Stripe", roof, 0, 0, 0.7, L - 0.1, W + 0.012, 0.08, 0.04)
    waist = rbox(P, "A2_Waist", col, -0.2, 0, 0.96, L - 0.9, W - 0.1, 0.16, 0.08)
    well(P, "A2", [body, stripe, waist], -1.42, 0.6, W / 2 - 0.15, 0.38)
    posts_roof(P, "A2", -1.85, 0.85, W / 2 - 0.12, 1.0, 1.77, roof)
    rod(P, "A2_PM1", roof, (-0.5, W / 2 - 0.12, 1.0), (-0.5, (W / 2 - 0.12) * 0.97, 1.77), 0.05)
    rod(P, "A2_PM2", roof, (-0.5, -(W / 2 - 0.12), 1.0), (-0.5, -(W / 2 - 0.12) * 0.97, 1.77), 0.05)
    rbox(P, "A2_Roof", roof, -0.55, 0, 1.81, 2.95, W - 0.06, 0.16, 0.08)
    for x in (-1.4, 0.2):
        rbox(P, "A2_Rail%d" % int(x * 10), M["iron"], x, 0, 1.91, 0.08, W - 0.3, 0.06, 0.03)
    rbox(P, "A2_Case", M["terra"], -0.95, -0.2, 2.08, 0.95, 0.75, 0.34, 0.1)
    rbox(P, "A2_Bag", M["cream"], -0.05, 0.35, 2.04, 0.6, 0.5, 0.26, 0.1)
    rbox(P, "A2_Dash", M["leather"], 0.72, 0, 1.02, 0.3, W - 0.3, 0.14, 0.05)
    seats(P, "A2", [-0.05, -1.05], 0.35, 0.44, M["leather"]); steering(P, "A2", 0.55, -0.35, 1.1)
    lamps(P, "A2", L / 2 - 0.02, 0.68, [-0.55, 0.55], 0.13, M["lampc"])
    rbox(P, "A2_BumpF", M["hub"], L / 2 + 0.02, 0, 0.38, 0.14, W - 0.2, 0.14, 0.06)
    rbox(P, "A2_BumpB", M["hub"], -L / 2 - 0.02, 0, 0.38, 0.14, W - 0.2, 0.14, 0.06)
    wheels(P, "A2", [1.3, -1.3], W / 2, 0.32)


def a_van(P, col, band):
    L, W = 4.6, 1.9
    low = rbox(P, "A3_Low", col, 0, 0, 0.66, L, W, 0.72, 0.25)
    well(P, "A3", [low], 0.86, 1.78, W / 2 - 0.15, 0.46)
    rbox(P, "A3_Load", col, -0.75, 0, 1.62, 3.1, W, 1.3, 0.3)
    rbox(P, "A3_Band", band, -0.75, 0, 1.6, 2.9, W + 0.014, 0.22, 0.05)
    posts_roof(P, "A3", 0.85, 2.05, W / 2 - 0.14, 1.0, 1.89, col, lean_front=0.22)
    rbox(P, "A3_CabRoof", col, 1.35, 0, 1.92, 1.2, W - 0.08, 0.16, 0.08)
    rbox(P, "A3_Dash", M["leather"], 1.85, 0, 1.08, 0.3, W - 0.3, 0.16, 0.05)
    seats(P, "A3", [1.3], 0.42, 0.54, M["leather"]); steering(P, "A3", 1.68, -0.42, 1.18)
    lamps(P, "A3", L / 2 - 0.02, 0.74, [-0.62, 0.62], 0.15, M["lampc"])
    rbox(P, "A3_BumpF", M["hub"], L / 2 + 0.02, 0, 0.4, 0.14, W - 0.2, 0.16, 0.06)
    wheels(P, "A3", [1.5, -1.45], W / 2, 0.35, 0.26)


# ---- option B: pebble -----------------------------------------------------------------------------------------
def b_small(P, col, roof):
    blob(P, "B1_Body", col, 0, 0, 0.66, 3.3, 1.8, 0.95, top=(0.82, 0.86), bottom=(0.95, 0.92), nose=0.12, tail=0.05)
    posts_roof(P, "B1", -0.95, 0.5, 0.62, 0.95, 1.62, roof, r=0.06, lean_front=0.25)
    blob(P, "B1_Roof", roof, -0.3, 0, 1.68, 2.0, 1.55, 0.26, top=(0.85, 0.85), levels=3)
    rbox(P, "B1_Dash", M["leather"], 0.38, 0, 1.0, 0.25, 1.1, 0.12, 0.05)
    seats(P, "B1", [-0.3], 0.32, 0.76, M["leather"]); steering(P, "B1", 0.22, -0.32, 1.1)
    lamps(P, "B1", 1.42, 0.74, [-0.5, 0.5], 0.16, M["lampc"])
    wheels(P, "B1", [0.92, -0.95], 0.72, 0.3)


def b_estate(P, col, roof):
    blob(P, "B2_Body", col, 0, 0, 0.68, 4.5, 1.9, 0.98, top=(0.86, 0.86), bottom=(0.95, 0.92), nose=0.16, tail=0.02)
    posts_roof(P, "B2", -1.75, 0.8, 0.66, 1.0, 1.7, roof, r=0.06, lean_front=0.26)
    rod(P, "B2_PM1", roof, (-0.5, 0.66, 1.0), (-0.48, 0.64, 1.7), 0.055)
    rod(P, "B2_PM2", roof, (-0.5, -0.66, 1.0), (-0.48, -0.64, 1.7), 0.055)
    blob(P, "B2_Roof", roof, -0.5, 0, 1.76, 3.0, 1.62, 0.26, top=(0.9, 0.86), levels=3)
    blob(P, "B2_Bag", M["ochre"], -0.9, 0.0, 2.02, 1.0, 0.9, 0.42, top=(0.9, 0.9))
    rbox(P, "B2_Dash", M["leather"], 0.7, 0, 1.04, 0.25, 1.2, 0.12, 0.05)
    seats(P, "B2", [0.0, -1.0], 0.34, 0.78, M["leather"]); steering(P, "B2", 0.52, -0.34, 1.14)
    lamps(P, "B2", 1.98, 0.76, [-0.56, 0.56], 0.16, M["lampc"])
    wheels(P, "B2", [1.35, -1.35], 0.78, 0.32)


def b_van(P, col, band):
    blob(P, "B3_Load", col, -0.55, 0, 1.28, 3.9, 2.05, 2.35, top=(0.92, 0.9), bottom=(0.96, 0.95), nose=0.0, tail=0.05)
    blob(P, "B3_Nose", col, 1.35, 0, 0.7, 1.9, 2.0, 1.0, top=(0.85, 0.88), nose=0.18)
    blob(P, "B3_Band", band, -0.65, 0, 1.55, 3.5, 2.1, 0.32, levels=2)
    posts_roof(P, "B3", 0.95, 1.9, 0.72, 1.05, 1.9, col, r=0.065, lean_front=0.25)
    blob(P, "B3_CabRoof", col, 1.4, 0, 1.94, 1.35, 1.8, 0.24, top=(0.85, 0.9))
    rbox(P, "B3_Dash", M["leather"], 1.78, 0, 1.12, 0.25, 1.4, 0.14, 0.05)
    seats(P, "B3", [1.3], 0.42, 0.86, M["leather"]); steering(P, "B3", 1.6, -0.42, 1.24)
    lamps(P, "B3", 2.24, 0.8, [-0.6, 0.6], 0.17, M["lampc"])
    wheels(P, "B3", [1.45, -1.5], 0.86, 0.36, 0.26)


# ---- option C: classic --------------------------------------------------------------------------------------------
def dome(P, name, mat, cx, cy, cz, sx, sy, sz):
    """The top half of a stretched ball, open underneath: a wing arching over a wheel."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=28, v_segments=16, radius=0.5)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -0.02], context='VERTS')
    bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    ob = add_obj(name, bm, mat, loc=(cx, cy, cz))
    md = ob.modifiers.new("Thick", 'SOLIDIFY')
    setp(md, "thickness", 0.05)
    return put(smooth(ob), P)


def fender(P, n, col, x, y, r, length, front):
    """A round wing over a wheel, swept a little forward on the front wheels."""
    dome(P, n, col, x + (0.1 if front else -0.05), y, r, length, 0.5, r * 2.5)


def c_small(P, col, roof):
    L, Wb = 3.2, 1.3                                     # the body is narrow; the wings stand out from it
    rbox(P, "C1_Body", col, -0.1, 0, 0.72, L - 0.5, Wb, 0.62, 0.22)
    rbox(P, "C1_Bonnet", col, 0.95, 0, 0.82, 1.1, Wb - 0.2, 0.5, 0.22)
    ball(P, "C1_Grille", M["cream"], 1.5, 0, 0.78, 0.12, 0.62, 0.52)
    for s in (-1, 1):
        fender(P, "C1_FF%d" % s, col, 0.95, s * 0.72, 0.32, 1.3, True)
        fender(P, "C1_FB%d" % s, col, -0.95, s * 0.72, 0.32, 1.15, False)
        rbox(P, "C1_Board%d" % s, M["iron"], 0.0, s * 0.74, 0.36, 1.2, 0.3, 0.07, 0.03)
        ball(P, "C1_Lamp%d" % s, M["lampc"], 1.18, s * 0.6, 1.02, 0.22, 0.24, 0.24)
    posts_roof(P, "C1", -0.95, 0.42, Wb / 2 - 0.06, 1.02, 1.62, roof, r=0.05, lean_front=0.16)
    blob(P, "C1_Roof", roof, -0.3, 0, 1.68, 1.75, Wb + 0.22, 0.24, top=(0.9, 0.85))
    seats(P, "C1", [-0.3], 0.27, 0.8, M["leather"]); steering(P, "C1", 0.22, -0.27, 1.14)
    wheels(P, "C1", [0.95, -0.95], 0.84, 0.32, 0.22)


def c_estate(P, col, roof):
    Wb = 1.3
    rbox(P, "C2_Body", col, -0.35, 0, 0.74, 3.3, Wb, 0.66, 0.22)
    rbox(P, "C2_Bonnet", col, 1.25, 0, 0.84, 1.2, Wb - 0.25, 0.52, 0.22)
    ball(P, "C2_Grille", M["cream"], 1.86, 0, 0.8, 0.12, 0.66, 0.56)
    for s in (-1, 1):
        fender(P, "C2_FF%d" % s, col, 1.25, s * 0.8, 0.33, 1.4, True)
        fender(P, "C2_FB%d" % s, col, -1.35, s * 0.8, 0.33, 1.25, False)
        rbox(P, "C2_Board%d" % s, M["iron"], -0.05, s * 0.8, 0.37, 1.55, 0.3, 0.07, 0.03)
        ball(P, "C2_Lamp%d" % s, M["lampc"], 1.5, s * 0.64, 1.06, 0.24, 0.26, 0.26)
    posts_roof(P, "C2", -1.9, 0.62, Wb / 2 - 0.06, 1.06, 1.72, roof, r=0.05, lean_front=0.18)
    for s in (-1, 1):
        rod(P, "C2_PM%d" % s, roof, (-0.6, s * (Wb / 2 - 0.06), 1.06), (-0.6, s * (Wb / 2 - 0.06) * 0.97, 1.72), 0.045)
    blob(P, "C2_Roof", roof, -0.65, 0, 1.78, 2.95, Wb + 0.24, 0.24, top=(0.92, 0.85))
    for x in (-1.5, 0.1):
        rod(P, "C2_Rail%d" % int(x * 10), M["iron"], (x, -0.6, 1.9), (x, 0.6, 1.9), 0.025)
    rbox(P, "C2_Trunk", M["wood"], -0.75, 0, 2.06, 1.0, 0.85, 0.3, 0.06)
    seats(P, "C2", [0.1, -0.95], 0.3, 0.82, M["leather"]); steering(P, "C2", 0.58, -0.3, 1.16)
    ball(P, "C2_Spare", M["tyre"], -2.06, 0, 0.98, 0.26, 0.6, 0.6)
    wheels(P, "C2", [1.25, -1.35], 0.9, 0.33, 0.22)


def c_van(P, col, band):
    Wb = 1.5
    rbox(P, "C3_Load", col, -0.8, 0, 1.35, 2.9, 1.85, 1.75, 0.24)
    rbox(P, "C3_Band", band, -0.8, 0, 1.55, 2.7, 1.865, 0.22, 0.05)
    rbox(P, "C3_Cab", col, 0.95, 0, 0.78, 1.2, Wb, 0.7, 0.22)
    rbox(P, "C3_Bonnet", col, 1.7, 0, 0.84, 0.9, Wb - 0.35, 0.56, 0.22)
    ball(P, "C3_Grille", M["cream"], 2.17, 0, 0.82, 0.12, 0.7, 0.6)
    for s in (-1, 1):
        fender(P, "C3_FF%d" % s, col, 1.6, s * 0.95, 0.36, 1.35, True)
        fender(P, "C3_FB%d" % s, col, -1.45, s * 0.98, 0.36, 1.3, False)
        ball(P, "C3_Lamp%d" % s, M["lampc"], 1.85, s * 0.72, 1.12, 0.26, 0.28, 0.28)
    posts_roof(P, "C3", 0.62, 1.42, Wb / 2 - 0.08, 1.12, 1.95, col, r=0.055, lean_front=0.2)
    blob(P, "C3_CabRoof", col, 0.95, 0, 2.0, 1.25, Wb + 0.2, 0.24, top=(0.9, 0.85))
    seats(P, "C3", [0.92], 0.36, 0.9, M["leather"]); steering(P, "C3", 1.3, -0.36, 1.26)
    wheels(P, "C3", [1.6, -1.45], 1.04, 0.36, 0.24)


OPTIONS = (
    ("A", "Soft box", (a_small, a_estate, a_van)),
    ("B", "Pebble", (b_small, b_estate, b_van)),
    ("C", "Classic", (c_small, c_estate, c_van)),
)


# ---- the scene ----------------------------------------------------------------------------------------------------
def group(name, x, y, yaw):
    e = bpy.data.objects.new(name, None)
    COL.objects.link(e)
    e.location = (x, y, 0)
    e.rotation_euler = (0, 0, yaw)
    return e


def scene_all():
    random.seed(9)
    scene = new_scene("CarKit09")
    global K_M, M
    K_M = kit_mats()
    M = dict(K_M)
    M.update(tyre=flat("M_Tyre", (0.07, 0.065, 0.065), rough=0.95), hub=flat("M_Hub", (0.58, 0.56, 0.52)),
             leather=flat("M_Leather", (0.30, 0.19, 0.12)), lampc=flat("M_LampGlass", (0.86, 0.80, 0.64), rough=0.6),
             roof=flat("M_RoofCream", (0.74, 0.70, 0.62)), dusty=flat("M_CarDusty", DUSTY), rose=flat("M_CarRose", ROSE),
             olive=flat("M_CarOlive", (0.30, 0.33, 0.20)), terra=K_M["terra"], cream=K_M["cream"], ochre=K_M["ochre"])
    paints = [(M["teal"], M["roof"], M["cream"]), (M["terra"], M["roof"], M["cream"]), (M["dusty"], M["roof"], M["teal"])]
    alt = [(M["ochre"], M["roof"], M["teal"]), (M["olive"], M["roof"], M["cream"]), (M["rose"], M["roof"], M["cream"])]
    facts = []
    for gi, (key, label, makers) in enumerate(OPTIONS):
        if ONLY and key not in ONLY:
            continue
        gx = gi * GAP
        box("Ground_" + key, gx - 14, gx + 14, -12, 6, -0.3, 0, K_M["paving"], bevel=0)
        box("Wall_" + key, gx - 14, gx + 14, 4.2, 5.0, 0, 5.5, K_M["stone"], bevel=0)
        box("Plaster_" + key, gx - 14, gx + 14, 4.15, 4.6, 3.4, 5.5, K_M["plaster"], bevel=0)
        cols = paints if gi % 2 == 0 else alt
        for i, mk in enumerate(makers):
            P = group("Car_%s%d" % (key, i), gx - 6.2 + i * 6.2, 0.6, math.radians(-22))
            c, roof, band = cols[i]
            mk(P, c, roof if i < 2 else band)
        people([("PlayerA", gx + 8.6, -0.2)])
        facts.append((key, label, gx))
    sun_and_sky(scene, (0.78, -0.40, 0.48), 8.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.35)   # the kit daylight, ADR-0007
    return scene, facts


def people(where):
    cs = CHAR_STYLES["B"]
    for i, (who, x, y) in enumerate(where):
        sp = dict(CAST[who])
        sp["name"] = "%s_%d" % (who, i)
        character3(sp, cs, x, y)


def configure(scene, exposure, res, samples):
    r = scene.render
    setp(r, "engine", 'CYCLES')
    c = getattr(scene, "cycles", None)
    if c is not None:
        setp(c, "device", 'CPU')
        setp(c, "samples", samples)
        setp(c, "use_adaptive_sampling", True)
        setp(c, "adaptive_threshold", 0.03)
        setp(c, "use_denoising", True)
        setp(c, "max_bounces", 6)
        setp(c, "diffuse_bounces", 4)
        setp(c, "glossy_bounces", 2)
    r.resolution_x, r.resolution_y = res
    r.resolution_percentage = SCALE
    setp(r.image_settings, "media_type", 'IMAGE')
    setp(r.image_settings, "file_format", 'PNG')
    setp(r.image_settings, "color_mode", 'RGB')
    vs = scene.view_settings
    setp(vs, "view_transform", 'AgX')
    setp(vs, "look", 'AgX - Medium High Contrast')
    setp(vs, "exposure", exposure)


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(SRC, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    try:
        t0 = time.time()
        scene, facts = scene_all()
        log("scene built: %d objects in %.1fs" % (len(scene.objects), time.time() - t0))
        try:
            bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
            log("saved .blend")
        except Exception:
            log("save failed:\n" + traceback.format_exc())
        for key, label, gx in facts:
            try:
                scene.camera = camera(scene, "Cam_" + key, (gx + 2.4, -15.0, 3.2), (gx + 1.3, 0.6, 1.0), 30.0)
                configure(scene, -0.15, (1600, 800), SAMPLES)
                t = time.time()
                scene.render.filepath = OUT + "/" + NAME + "_" + key + ".png"
                bpy.ops.render.render(write_still=True, scene=scene.name)
                log("%s (%s) rendered in %.1fs" % (key, label, time.time() - t))
                if key == "A":       # and from above, as the camera behind a driver sees it: into the open cabins (look test 45)
                    scene.camera = camera(scene, "Cam_" + key + "_in", (gx + 0.6, -12.5, 9.5), (gx + 0.6, 0.6, 0.5), 30.0)
                    scene.render.filepath = OUT + "/" + NAME + "_" + key + "_inside.png"
                    bpy.ops.render.render(write_still=True, scene=scene.name)
                    log("%s from above rendered" % key)
            except Exception:
                log("FAILED %s:\n%s" % (key, traceback.format_exc()))
    except Exception:
        log("FAILED:\n" + traceback.format_exc())
    log("DONE")


main()
