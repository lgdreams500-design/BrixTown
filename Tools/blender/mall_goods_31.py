"""Brix Town - mall goods 31: the goods for the home that Eastgate Mall's outlets sell, as low-poly objects (FURN-065).

The owner, 2026-11-24: "each outlets should have buyable products in it with decent catalog, use blender to create the
objects, some of these object can be placed in the house like guitar, books etc."

Headless (WASTED_ROOT is the checkout to write into; without it, C:/Projects/Wasted):
    blender-launcher.exe -b --factory-startup --python <checkout>/Tools/blender/mall_goods_31.py
Quick checks: GOODS_ONLY=g_guitar,g_tv  GOODS_NORENDER=1  WASTED_SAMPLES=24  WASTED_SCALE=50

Builds on furniture_kit_17.py (and through it car_kit_09.py, building_kit_07.py, old_town_04.py and style_test_02.py):
its painted matte materials, the kit daylight (ADR-0007), its camera, render settings and the person for scale. The way
out to the slice is car_export_10.py's: triangles with their own normals, written as a script the page loads.

Every good that goes to a home is a piece of World/rules/furnishing.json with `from: "mall"` (49 of them, FURN-064).
Each is made here by one function from a few light shapes (box, cylinder, ball, rod, a turned shape, a cut-out slab), in
the town's muted colours, chunky and matte (ADR-0004; ART_DIRECTION W1-W8), and fits inside the size the rules give it.
Each is made twice: near, for a home (at most NEAR triangles), and far, for an outlet's display (at most FAR).

Writes:
    Prototype/play/goods_models.js             the models for the slice (goods.js draws them; the boxes stand in without it)
    Art/Source/StyleTests/mall_goods_31.blend  the scene of the last sheet, with every model's mesh in it
    Art/Export/mall_goods_31/mall_goods_31.glb the near models, one object each, for import into Unreal
    Art/StyleTests/mall_goods_31/              sheet_1.png to sheet_3.png (near), sheet_far.png (far), counts.json, the log

Axes here: x across, y from the front (-) to the back (+), z up; the piece stands on the floor at the origin. In the file
for the slice: u across (= -x, so nothing is mirrored), y up, v front to back; millimetres; normals x100.
"""
import bpy, bmesh, math, os, json, time, traceback
from mathutils import Vector

ROOT = os.environ.get("WASTED_ROOT", "C:/Projects/Wasted").replace("\\", "/").rstrip("/")
_k = open(ROOT + "/Tools/blender/furniture_kit_17.py", encoding="utf-8").read()
_k = _k[:_k.rstrip().rfind("\nmain()")]                   # kit 17's helpers, without running it
exec(compile(_k, "furniture_kit_17_helpers", "exec"), globals())

NAME = "mall_goods_31"
OUT = ROOT + "/Art/StyleTests"
SRC = ROOT + "/Art/Source/StyleTests"
EXP = ROOT + "/Art/Export/" + NAME
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"
DEST = ROOT + "/Prototype/play/goods_models.js"
RULES = ROOT + "/World/rules/furnishing.json"
GOODS_ONLY = tuple(os.environ["GOODS_ONLY"].split(",")) if os.environ.get("GOODS_ONLY") else None
NORENDER = bool(os.environ.get("GOODS_NORENDER"))
SAMPLES = int(os.environ.get("WASTED_SAMPLES", 64))
SCALE = int(os.environ.get("WASTED_SCALE", 100))
NEAR, FAR = 600, 180          # most triangles a model may have: near (in a home), far (on display in an outlet)
TOL = 0.003                   # a model may stand this far outside its size in the rules, metres

# the colours, as the slice has them (goods.js); accent0 and accent1 are painted afresh for each piece by its seed
PAL = [("wood", (.5, .36, .22)), ("wood_d", (.3, .2, .12)), ("dark", (.12, .11, .11)), ("iron", (.1, .1, .11)),
       ("cream", (.78, .72, .6)), ("paper", (.82, .78, .68)), ("terra", (.5, .25, .19)), ("ochre", (.6, .46, .2)),
       ("teal", (.2, .37, .38)), ("blue", (.28, .38, .52)), ("olive", (.33, .37, .2)), ("plum", (.36, .26, .38)),
       ("leaf", (.27, .37, .18)), ("leaf_l", (.33, .44, .2)), ("brass", (.62, .5, .24)), ("red", (.5, .14, .12)),
       ("plush", (.55, .4, .26)), ("glass", (.2, .24, .27)), ("steel", (.42, .43, .45)), ("soil", (.2, .15, .1)),
       ("mirror", (.62, .68, .72)), ("accent0", (.5, .25, .19)), ("accent1", (.2, .37, .38))]
PIDX = {k: i for i, (k, _) in enumerate(PAL)}
G = {}                        # name -> material
LOD = 0                       # 0 near, 1 far: the shapes below are lighter when it is 1
CUR = []                      # the parts of the model being made
SM = ((6, 4), (4, 3))         # a small ball: segments round and down, near and far
TINY = ((4, 3), (4, 3))
THIN = (4, 3)                 # a thin rod's sides, near and far
_t0 = time.time()


def log(msg):
    line = time.strftime("%H:%M:%S") + " " + msg
    print(line)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")


# ---- the shapes ----------------------------------------------------------------------------------------------
def sub(P, name, loc=(0, 0, 0), rot=(0, 0, 0)):
    """A frame inside the piece: what is made in it is moved and turned with it."""
    e = bpy.data.objects.new(name, None)
    COL.objects.link(e)
    e.parent = P
    e.location = loc
    e.rotation_euler = rot
    return e


def _ob(P, name, bm, mat, loc=(0, 0, 0), rot=(0, 0, 0), more=()):
    ob = add_obj(name, bm, G[mat], loc=loc, rot=rot)
    for m in more:
        ob.data.materials.append(G[m])
    ob.parent = P
    CUR.append(ob)
    return ob


def B(P, n, mat, cx, cy, cz, sx, sy, sz, r=0.0, rot=(0, 0, 0)):
    """A box; near, its edges are cut off r wide."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    if r > 0 and LOD == 0:
        bmesh.ops.bevel(bm, geom=bm.edges[:], offset=min(r, sx / 2.3, sy / 2.3, sz / 2.3), segments=1, profile=0.5, affect='EDGES')
    return _ob(P, n, bm, mat, (cx, cy, cz), rot)


def C(P, n, mat, x, y, z, r, h, rt=None, ax='z', segs=(10, 6), cap=None):
    """A cylinder or cone. ax 'z': upright from z. ax 'y' or 'x': lying, its middle at (x, y, z); r is then at the back
    (+y) or the left (-x), rt at the front or the right. cap: another colour for its two ends."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segs[LOD], radius1=r, radius2=(r if rt is None else rt), depth=h)
    if cap:
        for f in bm.faces:
            if abs(f.normal.z) > 0.9:
                f.material_index = 1
    loc, rot = ((x, y, z + h / 2), (0, 0, 0)) if ax == 'z' else ((x, y, z), (math.pi / 2, 0, 0) if ax == 'y' else (0, math.pi / 2, 0))
    return _ob(P, n, bm, mat, loc, rot, (cap,) if cap else ())


def S(P, n, mat, x, y, z, sx, sy, sz, segs=((8, 5), (5, 3))):
    """A ball, sx by sy by sz across, its middle at (x, y, z)."""
    u, v = segs[LOD]
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=u, v_segments=v, radius=0.5)
    bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    return _ob(P, n, bm, mat, (x, y, z))


STOUT = ("Leg", "Post", "Stand", "StFoot", "StBack", "StArm", "StBar", "Brace", "Rail", "Crank", "Arm1", "Arm2", "TomArm", "Foot", "Bar")


def R(P, n, mat, a, b, r, segs=(6, 4)):
    """A round rod from a to b. Nothing spindly (ART_DIRECTION I2): a leg, post or stand is at least 4 cm across, and
    no rod is under 2 cm."""
    r = max(r, 0.02 if any(t in n for t in STOUT) else 0.01)
    a, b = Vector(a), Vector(b)
    d = b - a
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segs[LOD], radius1=r, radius2=r, depth=d.length)
    ob = _ob(P, n, bm, mat, tuple((a + b) / 2))
    ob.rotation_mode = 'QUATERNION'
    ob.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())
    return ob


def L(P, n, mat, x, y, prof, segs=(10, 6), top=None):
    """A turned shape: the profile [(radius, height), ...] from the bottom up, spun round its upright axis.
    top: another colour for its top face (the dark inside of a vase)."""
    if LOD == 1 and len(prof) > 4:
        prof = prof[:1] + prof[1:-1:2] + prof[-1:]
    s = segs[LOD]
    bm = bmesh.new()
    rings = []
    for r, z in prof:
        rings.append([bm.verts.new((0, 0, z))] if r <= 1e-6 else
                     [bm.verts.new((r * math.cos(2 * math.pi * i / s), r * math.sin(2 * math.pi * i / s), z)) for i in range(s)])
    for a, b in zip(rings, rings[1:]):
        for i in range(s):
            j = (i + 1) % s
            if len(a) == 1 and len(b) == 1:
                continue
            if len(a) == 1:
                bm.faces.new((a[0], b[i], b[j]))
            elif len(b) == 1:
                bm.faces.new((a[i], a[j], b[0]))
            else:
                bm.faces.new((a[i], a[j], b[j], b[i]))
    if len(rings[0]) > 1:
        bm.faces.new(list(reversed(rings[0])))
    cap = bm.faces.new(rings[-1]) if len(rings[-1]) > 1 else None
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    if top and cap is not None:
        cap.material_index = 1
    return _ob(P, n, bm, mat, (x, y, 0), more=(top,) if top else ())


def X(P, n, mat, pts, t0, t1, plane='xz'):
    """A slab cut to an outline. plane 'xz': the outline is seen from the front and the slab is t0 to t1 thick along y;
    'xy': seen from above, thick along z; 'yz': seen from the side, thick along x."""
    if LOD == 1 and len(pts) > 9:
        pts = pts[::2]
    at = {'xz': lambda p, t: (p[0], t, p[1]), 'xy': lambda p, t: (p[0], p[1], t), 'yz': lambda p, t: (t, p[0], p[1])}[plane]
    bm = bmesh.new()
    a = [bm.verts.new(at(p, t0)) for p in pts]
    b = [bm.verts.new(at(p, t1)) for p in pts]
    k = len(pts)
    for i in range(k):
        j = (i + 1) % k
        bm.faces.new((a[i], a[j], b[j], b[i]))
    bm.faces.new(list(reversed(a)))
    bm.faces.new(b)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return _ob(P, n, bm, mat, (0, 0, 0))


def mir(half):
    """A whole outline from its right half (x >= 0, from the bottom up)."""
    return list(half) + [(-x, z) for x, z in reversed(half) if x > 1e-6]


def oval(rx, rz, cz, k=10):
    return [(rx * math.cos(2 * math.pi * i / k), cz + rz * math.sin(2 * math.pi * i / k)) for i in range(k)]


def blade(P, n, mat, base, a, t, length, width):
    """A leaf: a thin strip from `base`, pointing the way `a` (radians round the upright) and lifted by `t`."""
    dx, dy, dz = -math.sin(a) * math.cos(t), math.cos(a) * math.cos(t), math.sin(t)
    return B(P, n, mat, base[0] + dx * length / 2, base[1] + dy * length / 2, base[2] + dz * length / 2, width, length, 0.012, 0, rot=(t, 0, a))


def stand(P, d, hgt, half=0.12):
    """An instrument stand: two feet to the front, a back post with a yoke, and a cradle for the body."""
    yb = d / 2 - 0.05
    for s in (-1, 1):
        R(P, "StFoot", "iron", (s * half, -d / 2 + 0.03, 0.021), (s * half * 0.6, yb, 0.021), 0.013, THIN)
        if LOD == 0:
            R(P, "StArm", "iron", (s * 0.07, yb - 0.02, 0.13), (s * 0.07, yb - 0.17, 0.1), 0.012, THIN)
    R(P, "StPost", "iron", (0, yb, 0.021), (0, yb - 0.03, hgt), 0.014, THIN)
    if LOD == 0:
        R(P, "StBack", "iron", (-half * 0.6, yb, 0.021), (half * 0.6, yb, 0.021), 0.013, THIN)
        R(P, "StBar", "iron", (-0.07, yb - 0.02, 0.13), (0.07, yb - 0.02, 0.13), 0.012, THIN)
        B(P, "StYoke", "iron", 0, yb - 0.045, hgt, 0.09, 0.04, 0.025)
    return yb


# ---- the goods: each fits the size World/rules/furnishing.json gives it ---------------------------------------------
def g_alarmclock(P):
    C(P, "Body", "accent1", 0, 0, .085, .066, .07, ax='y', segs=(12, 8))
    C(P, "Face", "cream", 0, -.037, .085, .054, .008, ax='y', segs=(12, 8))
    if LOD == 0:
        B(P, "HandH", "dark", .012, -.0425, .092, .03, .004, .008)
        B(P, "HandM", "dark", 0, -.0425, .105, .008, .004, .04)
    for s in (-1, 1):
        S(P, "Bell", "brass", s * .04, 0, .142, .04, .04, .032, SM)
        B(P, "Foot", "brass", s * .04, 0, .012, .02, .03, .024)
    B(P, "Hammer", "brass", 0, 0, .15, .012, .012, .02)


def g_amp(P):
    B(P, "Box", "dark", 0, .01, .31, .58, .31, .56, .02)
    B(P, "Cloth", "wood", 0, -.15, .26, .5, .02, .38, .01)
    B(P, "Panel", "brass", 0, -.15, .51, .5, .02, .07)
    if LOD == 0:
        for i in range(4):
            C(P, "Knob", "cream", -.18 + i * .12, -.164, .51, .018, .016, ax='y', segs=(6, 4))
    B(P, "Handle", "iron", 0, .01, .593, .2, .03, .014)
    for sx in (-1, 1):
        for sy in (-1, 1):
            B(P, "Foot", "iron", sx * .24, .01 + sy * .11, .015, .05, .05, .03)


def g_atlas(P):
    C(P, "Base", "wood_d", 0, .02, 0, .2, .04)
    R(P, "Post", "wood_d", (0, .02, .04), (0, .02, .9), .03)
    T = sub(P, "Top", (0, 0, .93), (math.radians(22), 0, 0))
    B(T, "Board", "wood", 0, 0, 0, .52, .4, .04, .01)
    B(T, "Lip", "wood_d", 0, -.19, .035, .5, .02, .04)
    B(T, "Cover", "accent0", 0, .01, .026, .5, .32, .012)
    for s in (-1, 1):
        B(T, "Page", "paper", s * .12, .01, .046, .23, .3, .03, .008)


def g_bear(P):
    S(P, "Body", "plush", 0, .02, .19, .3, .26, .34)
    S(P, "Head", "plush", 0, .01, .44, .25, .23, .24)
    if LOD == 0:
        S(P, "Belly", "cream", 0, -.085, .19, .16, .08, .2, SM)
    S(P, "Muzzle", "cream", 0, -.095, .415, .11, .08, .09, SM)
    for s in (-1, 1):
        S(P, "Ear", "plush", s * .1, .02, .545, .09, .05, .09, SM)
        S(P, "Arm", "plush", s * .155, -.02, .24, .1, .12, .2, SM)
        S(P, "Leg", "plush", s * .1, -.075, .07, .13, .2, .13, SM)
        if LOD == 0:
            S(P, "Eye", "dark", s * .05, -.1, .48, .025, .02, .025, TINY)
    if LOD == 0:
        S(P, "Nose", "dark", 0, -.135, .43, .035, .03, .03, TINY)


def g_blocks(P):
    cols = ("accent0", "teal", "ochre", "blue", "olive", "plum")
    k = 0
    for row in range(3):
        for i in range(3 - row):
            B(P, "Cube", cols[k % 6], (i - (2 - row) / 2) * .15, .05, .0675 + row * .136, .135, .135, .135, .012,
              rot=(0, 0, (.12 if k % 2 else -.1) * (row > 0)))
            k += 1
    C(P, "Roll", "accent1", -.12, -.11, .05, .05, .14, ax='x', segs=(8, 6))
    B(P, "Brick", "cream", .12, -.11, .035, .16, .08, .07, .01)


def g_bookrow(P):
    cols = ("accent0", "teal", "ochre", "plum", "blue", "olive", "cream", "wood_d")
    hs = (.24, .26, .21, .25, .23, .27, .22, .25)
    ws = (.05, .04, .06, .045, .055, .04, .06, .05)
    x = -.2
    for i in range(8):
        B(P, "Book", cols[i], x + ws[i] / 2, 0, hs[i] / 2, ws[i] - .004, .16 - (i % 3) * .012, hs[i], .004)
        x += ws[i]
    for s in (-1, 1):
        B(P, "End", "brass", s * .225, 0, .09, .02, .14, .18, .004)


def g_bookstack(P):
    z = 0.0
    for c, bw, bd, bh, yaw in (("accent0", .28, .2, .055, .05), ("teal", .26, .19, .05, -.08), ("ochre", .27, .2, .06, .1),
                               ("plum", .24, .18, .045, -.04), ("cream", .2, .15, .04, .12)):
        T = sub(P, "Book", (0, 0, z), (0, 0, yaw))
        B(T, "Cover", c, 0, 0, bh / 2, bw, bd, bh)
        if LOD == 0:
            B(T, "Pages", "paper", .012, 0, bh / 2, bw - .02, bd + .004, bh - .016)
        z += bh


def g_cactus(P):
    L(P, "Pot", "terra", 0, 0, [(.055, 0), (.075, .1), (.085, .1), (.085, .12)])
    C(P, "Soil", "soil", 0, 0, .105, .07, .012, segs=(8, 6))
    S(P, "Body", "leaf", 0, 0, .2, .11, .11, .17)
    S(P, "Arm", "leaf_l", .055, 0, .19, .06, .06, .09, SM)
    S(P, "Flower", "accent0", -.01, 0, .287, .035, .035, .026, SM)


def g_candlesticks(P):
    for s, hh in ((-1, .19), (1, .15)):
        L(P, "Stick", "brass", s * .085, 0, [(.05, 0), (.05, .012), (.015, .03), (.012, hh * .6), (.022, hh * .7), (.012, hh * .8), (.03, hh), (.03, hh + .012)], segs=(8, 5))
        C(P, "Candle", "cream", s * .085, 0, hh + .012, .016, .08, segs=(6, 4))
        if LOD == 0:
            S(P, "Flame", "ochre", s * .085, 0, hh + .1, .012, .012, .02, TINY)


def g_computer(P):
    B(P, "Case", "cream", 0, .07, .055, .5, .32, .11, .012)
    B(P, "Slot", "dark", .12, -.092, .055, .16, .006, .02)
    B(P, "Neck", "cream", 0, .08, .115, .2, .2, .02)
    B(P, "Monitor", "cream", 0, .08, .29, .36, .3, .34, .02)
    B(P, "Screen", "glass", 0, -.072, .3, .28, .012, .24, .006)
    B(P, "Keys", "cream", 0, -.17, .02, .44, .14, .04, .008)
    B(P, "KeyBed", "dark", 0, -.17, .042, .4, .1, .006)


def g_console(P):
    B(P, "Deck", "dark", 0, .05, .04, .3, .18, .08, .012)
    B(P, "Top", "steel", 0, .07, .084, .2, .1, .01)
    B(P, "Cart", "accent0", 0, .07, .1, .12, .03, .03)
    for s in (-1, 1):
        B(P, "Pad", "cream", s * .11, -.1, .015, .11, .065, .03, .008)
        if LOD == 0:
            C(P, "Stick", "red", s * .11 - .025, -.1, .03, .01, .02, segs=(6, 4))
            B(P, "Btn", "red", s * .11 + .03, -.1, .033, .02, .02, .008)


def g_cushions(P):
    z = 0.0
    for c, sz, th, yaw in (("accent0", .5, .15, .1), ("cream", .46, .14, -.14), ("accent1", .42, .12, .08)):
        B(P, "Pillow", c, 0, 0, z + th / 2, sz, sz, th, .05, rot=(0, 0, yaw))
        z += th - .01
    if LOD == 0:
        S(P, "Button", "brass", 0, 0, z + .012, .04, .04, .02, TINY)


def g_desklamp(P):
    C(P, "Base", "dark", 0, .03, 0, .085, .025)
    R(P, "Arm1", "brass", (0, .05, .025), (0, .08, .27), .012)
    R(P, "Arm2", "brass", (0, .08, .27), (0, -.02, .41), .012)
    S(P, "Joint", "brass", 0, .08, .27, .035, .035, .035, TINY)
    H = sub(P, "Head", (0, -.03, .4), (math.radians(-35), 0, 0))
    L(H, "Shade", "accent0", 0, 0, [(.075, 0), (.03, .09), (0, .1)], top=None)
    if LOD == 0:
        S(H, "Bulb", "paper", 0, 0, .005, .05, .05, .04, TINY)


def g_dollhouse(P):
    B(P, "Base", "wood_d", 0, 0, .02, .78, .43, .04, .008)
    B(P, "Walls", "cream", 0, .02, .3, .68, .34, .52, .01)
    X(P, "Roof", "accent0", [(-.39, .56), (.39, .56), (0, .88)], -.17, .21)
    B(P, "Chimney", "wood_d", .2, .02, .8, .07, .07, .2)
    for s in (-1, 1):
        for z in (.16, .4):
            B(P, "Window", "glass", s * .2, -.153, z, .12, .012, .12)
    B(P, "Door", "accent1", 0, -.153, .14, .1, .012, .2)
    B(P, "Attic", "glass", 0, -.172, .68, .09, .008, .09)


def g_drums(P):
    C(P, "Bass", "accent0", 0, -.08, .29, .28, .38, ax='y', segs=(12, 6), cap="cream")
    for s in (-1, 1):
        C(P, "Tom", "accent0", s * .2, -.02, .62, .14, .17, segs=(10, 5), cap="cream")
        if LOD == 0:
            R(P, "TomArm", "steel", (s * .08, -.02, .57), (s * .16, -.02, .64), .015, THIN)
    C(P, "Floor", "accent0", .56, .22, .22, .2, .38, segs=(10, 5), cap="cream")
    C(P, "Snare", "steel", -.5, .2, .56, .17, .12, segs=(10, 5), cap="cream")
    R(P, "SnareStand", "steel", (-.5, .2, .005), (-.5, .2, .56), .018, THIN)
    R(P, "HatStand", "steel", (-.66, -.12, .005), (-.66, -.12, .86), .012, THIN)
    C(P, "HatTop", "brass", -.66, -.12, .825, .17, .025, rt=.03, segs=(10, 5))
    R(P, "CrashStand", "steel", (.64, -.2, .005), (.63, -.2, 1.18), .012, THIN)
    C(P, "Crash", "brass", .62, -.2, 1.18, .21, .03, rt=.03, segs=(10, 5))
    C(P, "Seat", "dark", 0, .4, .44, .16, .07, segs=(10, 5))
    if LOD == 0:
        for a in (0.5, 2.6, 4.7):
            R(P, "FloorLeg", "steel", (.56 + .2 * math.cos(a), .22 + .2 * math.sin(a), .005), (.56 + .19 * math.cos(a), .22 + .19 * math.sin(a), .3), .012, THIN)
        C(P, "HatLow", "brass", -.66, -.12, .8, .03, .02, rt=.17)
        R(P, "SeatPost", "steel", (0, .4, .005), (0, .4, .44), .02, THIN)
        B(P, "Pedal", "steel", 0, .2, .02, .1, .2, .03)
        for x, y in ((-.5, .2), (-.66, -.12), (.64, -.2), (0, .4)):
            B(P, "FootA", "steel", x, y, .01, .26, .03, .02)
            B(P, "FootB", "steel", x, y, .01, .03, .26, .02)


def g_exercisebike(P):
    for sy in (-.47, .47):
        B(P, "Foot", "iron", 0, sy, .03, .5, .07, .06, .01)
    B(P, "Beam", "iron", 0, 0, .09, .07, .94, .07, .01)
    C(P, "Wheel", "accent0", 0, -.3, .33, .24, .07, ax='x', segs=(12, 8), cap="steel")
    R(P, "SeatPost", "steel", (0, .22, .1), (0, .3, .86), .03)
    B(P, "Saddle", "dark", 0, .32, .9, .2, .28, .07, .02)
    R(P, "BarPost", "steel", (0, -.3, .5), (0, -.38, 1.1), .03)
    B(P, "Bars", "dark", 0, -.39, 1.12, .46, .04, .04, .01)
    B(P, "Console", "cream", 0, -.4, 1.19, .16, .05, .11, .01)
    if LOD == 0:
        for s in (-1, 1):
            B(P, "Grip", "dark", s * .22, -.33, 1.12, .04, .14, .04)
            B(P, "Pedal", "dark", s * .13, -.3 + s * .13, .33 - s * .08, .1, .08, .025)
            R(P, "Crank", "steel", (s * .05, -.3, .33), (s * .09, -.3 + s * .13, .33 - s * .08), .012, THIN)
        B(P, "Display", "glass", 0, -.372, 1.2, .11, .006, .06)


def g_fern(P):
    L(P, "Pot", "terra", 0, 0, [(.11, 0), (.15, .22), (.165, .22), (.165, .25)])
    C(P, "Soil", "soil", 0, 0, .235, .14, .012, segs=(8, 6))
    n = 9 if LOD == 0 else 6
    for i in range(n):
        t, ln = math.radians((35, 55, 72)[i % 3]), (.24, .3, .36)[i % 3]
        blade(P, "Frond", "leaf" if i % 2 else "leaf_l", (0, 0, .24), 2 * math.pi * i / n + .2, t, ln, .085)
    S(P, "Leaf_Heart", "leaf", 0, 0, .34, .16, .16, .2, SM)


def g_figtree(P):
    L(P, "Pot", "cream", 0, 0, [(.15, 0), (.2, .3), (.215, .3), (.215, .34)])
    C(P, "Soil", "soil", 0, 0, .32, .19, .012, segs=(8, 6))
    R(P, "Trunk", "wood_d", (0, 0, .32), (.02, 0, 1.0), .03)
    R(P, "Branch1", "wood_d", (.01, 0, .75), (.12, .05, 1.05), .02, THIN)
    R(P, "Branch2", "wood_d", (.01, 0, .85), (-.1, -.04, 1.1), .02, THIN)
    S(P, "Leaf_Crown1", "leaf", 0, 0, 1.32, .5, .5, .5, ((8, 5), (6, 4)))
    S(P, "Leaf_Crown2", "leaf_l", .13, .06, 1.1, .28, .28, .26, SM)
    S(P, "Leaf_Crown3", "leaf_l", -.12, -.05, 1.16, .28, .28, .26, SM)


def g_floorvase(P):
    L(P, "Vase", "accent0", 0, 0, [(.09, 0), (.15, .1), (.195, .36), (.16, .58), (.075, .74), (.07, .8), (.105, .85)], top="dark")


def g_flowervase(P):
    L(P, "Vase", "accent1", 0, 0, [(.05, 0), (.08, .08), (.085, .14), (.045, .22), (.055, .25)], top="dark")
    for x, y, z, c in ((0, 0, .46, "accent0"), (.08, .02, .4, "ochre"), (-.07, .04, .42, "cream"), (.02, -.08, .38, "plum"), (-.04, -.06, .44, "accent0"))[:5 if LOD == 0 else 3]:
        R(P, "Stem", "leaf", (0, 0, .24), (x, y, z), .008, THIN)
        S(P, "Bloom", c, x, y, z, .075, .075, .06, SM)
    blade(P, "Leaf_A", "leaf", (0, 0, .25), 1.2, math.radians(40), .12, .05)
    blade(P, "Leaf_B", "leaf_l", (0, 0, .25), 4.0, math.radians(50), .11, .05)


def g_football(P):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=3 if LOD == 0 else 2, radius=.12)
    caps = [v for v in bm.verts if len(v.link_edges) == 5]       # the twelve corners of the ball: a dark patch round each
    for v in (caps if LOD == 0 else caps[::2]):
        for f in v.link_faces:
            f.material_index = 1
    _ob(P, "Ball", bm, "paper", (0, 0, .12), more=("dark",))


def g_globe(P):
    C(P, "Base", "wood_d", 0, 0, 0, .11, .03)
    R(P, "Post", "brass", (0, 0, .03), (0, 0, .14), .014, THIN)
    u, v = ((10, 6), (6, 4))[LOD]
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=u, v_segments=v, radius=.15)
    for f in bm.faces:
        c = f.calc_center_median()
        if math.sin(c.x * 31 + c.z * 17) + math.cos(c.y * 37 + c.z * 11) > .55:   # land
            f.material_index = 1
    _ob(P, "Ball", bm, "blue", (0, 0, .29), (math.radians(20), 0, 0), more=("olive",))
    if LOD == 0:
        pts = [(0, .168 * math.cos(math.radians(t)), .29 + .168 * math.sin(math.radians(t))) for t in (-90, -45, 0, 45, 90)]
        for a, b in zip(pts, pts[1:]):
            R(P, "Meridian", "brass", a, b, .007, THIN)
    S(P, "Knob", "brass", 0, 0, .462, .028, .028, .028, TINY)


def g_guitar(P):
    stand(P, .4, .74)
    T = sub(P, "Lean", (0, -.04, .115), (math.radians(-9), 0, 0))
    half = [(0, 0), (.11, 0), (.17, .05), (.19, .14), (.165, .24), (.13, .29), (.14, .37), (.115, .45), (0, .48)]
    X(T, "Body", "wood_d", mir(half), -.05, .05)
    X(T, "Top", "accent0", mir([(x * .93, .017 + z * .93) for x, z in half]), -.056, -.05)
    C(T, "Hole", "dark", 0, -.058, .3, .045, .006, ax='y', segs=(8, 5))
    B(T, "Neck", "wood", 0, -.03, .69, .05, .035, .44)
    B(T, "Headst", "wood_d", 0, -.02, .975, .075, .03, .13, .008)
    if LOD == 0:
        B(T, "Bridge", "dark", 0, -.06, .13, .12, .012, .025)
        B(T, "Fret", "dark", 0, -.052, .66, .045, .008, .5)
        for s in (-1, 1):
            B(T, "Tuners", "brass", s * .043, -.02, .975, .012, .02, .1)


def _solid_guitar(P, col, neck_len, stand_h, tuners):
    stand(P, .4, stand_h, .11)
    T = sub(P, "Lean", (0, -.04, .115), (math.radians(-9), 0, 0))
    X(T, "Body", col, [(-.15, .02), (-.06, 0), (.08, 0), (.16, .03), (.175, .13), (.14, .21), (.15, .3), (.17, .38), (.13, .4), (.07, .33),
                       (-.05, .33), (-.1, .42), (-.16, .44), (-.165, .34), (-.14, .24), (-.17, .13)], -.022, .022)
    X(T, "Guard", "paper", [(-.02, .1), (.11, .08), (.13, .2), (.1, .3), (-.02, .31)], -.027, -.022)
    z1 = .32 + neck_len
    B(T, "Neck", "cream", .01, -.01, .32 + neck_len / 2, .045, .03, neck_len)
    if LOD == 0:
        for z in (.15, .25):
            B(T, "Pickup", "dark", .03, -.028, z, .08, .008, .025)
        B(T, "Bridge", "steel", .03, -.028, .08, .09, .008, .03)
        B(T, "Fret", "wood_d", .01, -.028, .32 + neck_len / 2, .04, .006, neck_len)
    X(T, "Headst", "cream", [(-.02, z1), (.04, z1), (.05, z1 + .12), (-.035, z1 + .14)], -.022, 0)
    if LOD == 0:
        B(T, "Tuners", "steel", -.04, -.012, z1 + .065, .012, .02, tuners)


def g_guitar_electric(P):
    _solid_guitar(P, "accent0", .5, .7, .11)


def g_guitar_bass(P):
    _solid_guitar(P, "accent1", .64, .82, .09)


def g_jars(P):
    for x, r, hh in ((-.2, .07, .19), (-.065, .062, .16), (.065, .056, .14), (.19, .05, .11)):
        L(P, "Jar", "cream", x, 0, [(r * .9, 0), (r, .02), (r, hh), (r * .8, hh + .01)] if LOD == 0 else [(r * .9, 0), (r, hh + .01)], segs=(8, 5))
        C(P, "Lid", "wood", x, 0, hh + .01, r * .85, .03, segs=(8, 5))
        if LOD == 0:
            B(P, "Label", "accent0", x, -r - .001, hh * .5, r * 1.1, .004, hh * .35)


def g_jewelbox(P):
    B(P, "Box", "wood_d", 0, 0, .055, .28, .18, .09, .01)
    B(P, "Lid", "wood", 0, 0, .118, .285, .185, .04, .014)
    B(P, "Inlay", "accent0", 0, 0, .141, .16, .08, .006)
    B(P, "Clasp", "brass", 0, -.093, .095, .04, .012, .04)
    for sx in (-1, 1):
        for sy in (-1, 1):
            S(P, "Foot", "brass", sx * .12, sy * .07, .012, .03, .03, .024, TINY)


def g_keyboard(P):
    for s in (-1, 1):
        R(P, "LegA", "iron", (s * .42, -.16, .03), (s * .42, .15, .8), .016, THIN)
        R(P, "LegB", "iron", (s * .42, .16, .03), (s * .42, -.15, .8), .016, THIN)
    R(P, "Brace", "iron", (-.42, 0, .4), (.42, 0, .4), .014, THIN)
    for sy in (-.17, .17):
        R(P, "Foot", "iron", (-.46, sy, .021), (.46, sy, .021), .014, THIN)
    B(P, "Body", "dark", 0, 0, .85, 1.26, .34, .1, .015)
    B(P, "Keys", "paper", 0, -.07, .903, 1.1, .15, .012)
    B(P, "Panel", "steel", 0, .1, .902, .9, .08, .008)
    if LOD == 0:
        for i in range(15):
            if i % 7 not in (2, 6):
                B(P, "Black", "dark", -.5 + i * .0714, -.035, .913, .03, .08, .01)


def g_mantelclock(P):
    B(P, "Plinth", "wood_d", 0, 0, .015, .3, .14, .03, .006)
    X(P, "Case", "wood", [(-.13, .03), (.13, .03), (.13, .17), (.1, .26), (.04, .31), (-.04, .31), (-.1, .26), (-.13, .17)], -.055, .055)
    C(P, "Bezel", "brass", 0, -.058, .18, .085, .01, ax='y', segs=(12, 8))
    C(P, "Face", "cream", 0, -.064, .18, .072, .008, ax='y', segs=(12, 8))
    if LOD == 0:
        B(P, "HandH", "dark", .014, -.0685, .188, .036, .003, .008)
        B(P, "HandM", "dark", 0, -.0685, .205, .008, .003, .05)


def g_mat(P):
    B(P, "Mat", "accent1", 0, 0, .009, .6, 1.8, .018, .006)
    for sy in (-.7, .7):
        B(P, "Band", "cream", 0, sy, .0195, .6, .07, .004)


def g_mirror(P):
    for s in (-1, 1):
        B(P, "Foot", "wood", s * .27, 0, .025, .05, .42, .05, .01)
        B(P, "Post", "wood", s * .27, 0, .8, .045, .045, 1.5, .008)
        S(P, "Pivot", "brass", s * .25, 0, .9, .05, .05, .05, TINY)
    B(P, "Rail", "wood", 0, 0, .12, .54, .04, .04)
    T = sub(P, "Tilt", (0, 0, .9), (math.radians(-6), 0, 0))
    B(T, "Frame", "wood", 0, 0, 0, .46, .035, 1.5, .012)
    B(T, "Glass", "mirror", 0, -.019, 0, .4, .006, 1.42)


def g_pots(P):
    C(P, "Big", "steel", -.1, 0, 0, .125, .13)
    C(P, "BigLid", "steel", -.1, 0, .13, .13, .015, rt=.1)
    S(P, "BigKnob", "dark", -.1, 0, .158, .035, .035, .03, TINY)
    for s in (-1, 1):
        B(P, "BigHandle", "dark", -.1 + s * .13, 0, .1, .03, .06, .015)
    C(P, "Mid", "steel", .13, .02, 0, .095, .1)
    C(P, "MidLid", "steel", .13, .02, .1, .1, .012, rt=.08)
    C(P, "Pan", "steel", .13, .02, .115, .075, .06, segs=(8, 6), cap="dark")
    R(P, "PanHandle", "dark", (.07, .06, .16), (-.05, .13, .17), .012, THIN)


def g_rabbit(P):
    S(P, "Body", "cream", 0, .02, .12, .22, .2, .24)
    S(P, "Head", "cream", 0, 0, .29, .17, .16, .16)
    C(P, "Ribbon", "accent0", 0, .01, .205, .07, .025, segs=(8, 5))
    S(P, "Tail", "paper", 0, .1, .08, .06, .05, .06, TINY)
    for s in (-1, 1):
        S(P, "Ear", "cream", s * .04, .01, .375, .05, .03, .12, SM)
        S(P, "Foot", "cream", s * .06, -.07, .03, .07, .11, .06, SM)
        if LOD == 0:
            S(P, "Eye", "dark", s * .04, -.065, .31, .02, .02, .02, TINY)
    if LOD == 0:
        S(P, "Nose", "plum", 0, -.08, .285, .025, .02, .02, TINY)


def g_rackets(P):
    B(P, "Base", "wood_d", 0, 0, .025, .56, .28, .05, .01)
    for s in (-1, 1):
        B(P, "Post", "wood_d", s * .25, .09, .4, .04, .04, .7, .006)
    B(P, "Bar", "wood_d", 0, .09, .72, .54, .035, .04)
    for x, c in ((-.15, "accent0"), (0, "accent1"), (.15, "ochre")) if LOD == 0 else ((-.1, "accent0"), (.1, "accent1")):
        T = sub(P, "Racket", (x, -.06, .07), (math.radians(-10), 0, 0))
        C(T, "Grip", "dark", 0, 0, 0, .016, .26 if LOD == 0 else .37, segs=(6, 4))
        if LOD == 0:
            B(T, "Throat", c, 0, 0, .31, .03, .02, .1)
        X(T, "Head", c, oval(.105, .14, .5, 10 if LOD == 0 else 6), -.011, .011)
        X(T, "Strings", "paper", oval(.085, .12, .5, 10 if LOD == 0 else 6), -.012, .012)
    S(P, "Ball", "leaf_l", .21, -.09, .085, .065, .065, .065, SM)
    if LOD == 0:
        S(P, "Ball", "ochre", -.21, -.08, .085, .065, .065, .065, SM)


def g_radio(P):
    B(P, "Body", "wood", 0, 0, .14, .44, .2, .25, .03)
    B(P, "Cloth", "cream", -.08, -.101, .14, .22, .01, .17, .004)
    C(P, "Dial", "paper", .13, -.1, .16, .055, .012, ax='y')
    if LOD == 0:
        for i in (0, 1):
            C(P, "Knob", "brass", .09 + i * .08, -.1, .06, .018, .02, ax='y', segs=(6, 4))
    B(P, "Handle", "dark", 0, 0, .282, .22, .025, .016)
    for s in (-1, 1):
        B(P, "HStub", "dark", s * .1, 0, .27, .02, .025, .02)
        B(P, "Foot", "dark", s * .17, 0, .008, .05, .16, .016)


def g_recordplayer(P):
    B(P, "Plinth", "wood", 0, 0, .055, .46, .38, .09, .012)
    C(P, "Platter", "steel", -.05, 0, .1, .15, .018, segs=(12, 8))
    C(P, "Record", "dark", -.05, 0, .118, .14, .006, segs=(12, 8))
    C(P, "Label", "accent0", -.05, 0, .124, .045, .004, segs=(8, 6))
    C(P, "Pivot", "steel", .17, .11, .1, .022, .04, segs=(6, 4))
    R(P, "Arm", "steel", (.17, .11, .135), (.05, -.04, .13), .007, THIN)
    B(P, "Head", "dark", .05, -.05, .128, .02, .035, .012)
    if LOD == 0:
        C(P, "Knob", "brass", .17, -.13, .1, .016, .014, segs=(6, 4))
        for sx in (-1, 1):
            for sy in (-1, 1):
                C(P, "Foot", "dark", sx * .19, sy * .15, 0, .02, .012, segs=(6, 4))


def g_rocket(P):
    L(P, "Body", "cream", 0, 0, [(.06, .1), (.1, .2), (.105, .45), (.09, .62)])
    L(P, "Nose", "accent0", 0, 0, [(.09, .62), (.06, .76), (0, .9)])
    C(P, "Nozzle", "dark", 0, 0, .02, .07, .09, rt=.045, segs=(8, 6))
    for k in range(3):
        T = sub(P, "FinAt", (0, 0, 0), (0, 0, 2 * math.pi * k / 3 + math.pi / 2))
        X(T, "Fin", "accent0", [(.08, .06), (.19, 0), (.19, .1), (.1, .34)], -.012, .012)
    if LOD == 0:
        C(P, "Ring", "brass", 0, -.096, .46, .052, .01, ax='y', segs=(8, 6))
    C(P, "Window", "glass", 0, -.1, .46, .04, .012, ax='y', segs=(8, 6))


def g_rockinghorse(P):
    xs = (-.46, -.3, -.15, 0, .15, .3, .46)
    out = [(x, .5 * x * x) for x in xs] + [(x, .5 * x * x + .05) for x in reversed(xs)]
    for s in (-1, 1):
        X(P, "Rocker", "accent0", out, s * .14 - .02, s * .14 + .02)
        for sx in (-1, 1):
            R(P, "Leg", "cream", (sx * .19, s * .06, .44), (sx * .27, s * .13, .5 * .27 ** 2 + .05), .028, (6, 3))
        if LOD == 0:
            B(P, "Ear", "cream", .3, s * .035, .82, .03, .02, .05)
            S(P, "Eye", "dark", .385, s * .056, .77, .025, .012, .025, TINY)
    if LOD == 0:
        for x in (-.22, .22):
            B(P, "Tie", "accent0", x, 0, .5 * x * x + .06, .05, .3, .03)
    B(P, "Body", "cream", 0, 0, .5, .5, .17, .2, .05)
    X(P, "Neck", "cream", [(.14, .52), (.26, .5), (.36, .74), (.3, .8), (.2, .62)], -.06, .06)
    B(P, "Head", "cream", .35, 0, .75, .2, .11, .1, .03, rot=(0, math.radians(25), 0))
    if LOD == 0:
        B(P, "Mane", "wood_d", .2, 0, .68, .04, .03, .3, rot=(0, math.radians(30), 0))
    R(P, "Tail", "wood_d", (-.25, 0, .56), (-.36, 0, .38), .03)
    B(P, "Saddle", "accent1", -.02, 0, .605, .2, .18, .03, .01)
    if LOD == 0:
        R(P, "Grip", "wood_d", (.3, -.1, .7), (.3, .1, .7), .012, THIN)


def g_rug(P):
    B(P, "Rug", "accent0", 0, 0, .007, 1.56, 1.1, .014)
    B(P, "Field", "cream", 0, 0, .0155, 1.3, .84, .005)
    X(P, "Diamond", "accent1", [(-.4, 0), (0, -.26), (.4, 0), (0, .26)], .016, .021, 'xy')
    for s in (-1, 1):
        X(P, "Small", "ochre", [(s * .5 - .1, 0), (s * .5, -.08), (s * .5 + .1, 0), (s * .5, .08)], .016, .021, 'xy')
        B(P, "Fringe", "paper", s * .79, 0, .004, .02, 1.06, .008)


def g_speakers(P):
    for s in (-1, 1):
        x = s * .21
        B(P, "Cab", "wood_d", x, .01, .45, .25, .26, .86, .015)
        B(P, "Cloth", "dark", x, -.122, .52, .2, .008, .66, .004)
        C(P, "Woofer", "steel", x, -.128, .36, .075, .012, ax='y')
        C(P, "Mid", "steel", x, -.128, .6, .055, .012, ax='y', segs=(8, 6))
        if LOD == 0:
            C(P, "Tweet", "brass", x, -.128, .76, .028, .012, ax='y', segs=(6, 4))
            B(P, "Plinth", "dark", x, .01, .01, .27, .28, .02)


def g_tablelamp(P):
    L(P, "Base", "accent0", 0, 0, [(.06, 0), (.085, .03), (.1, .13), (.06, .23), (.025, .27)])
    R(P, "Stem", "brass", (0, 0, .27), (0, 0, .34), .012, THIN)
    L(P, "Shade", "cream", 0, 0, [(.15, .3), (.105, .5)], top="paper")


def g_tabletop(P):
    side = [(-.24, 0), (.24, 0), (.24, .7), (-.08, .7), (-.08, .6), (.02, .57), (-.03, .36), (-.24, .31)]
    for s in (-1, 1):
        X(P, "Side", "accent0", side, s * .235 - .015, s * .235 + .015, 'yz')
    B(P, "Core", "dark", 0, .11, .34, .44, .26, .68)
    B(P, "Front", "dark", 0, -.13, .15, .44, .2, .3)
    B(P, "Deck", "cream", 0, -.135, .34, .44, .225, .02, rot=(math.radians(13.4), 0, 0))
    B(P, "Screen", "glass", 0, -.01, .465, .4, .012, .2, rot=(math.radians(-13.4), 0, 0))
    B(P, "Marquee", "ochre", 0, -.075, .65, .44, .012, .09)
    R(P, "Stick", "steel", (-.1, -.14, .345), (-.1, -.15, .4), .008, THIN)
    S(P, "Knob", "red", -.1, -.15, .41, .035, .035, .035, TINY)
    if LOD == 0:
        S(P, "Btn1", "ochre", .06, -.15, .342, .03, .03, .016, TINY)
        S(P, "Btn2", "red", .12, -.15, .342, .03, .03, .016, TINY)


def g_train(P):
    def wheels(xs):
        if LOD == 1:                                                # far away: one dark strip under each side
            for s in (-1, 1):
                B(P, "Wheels", "dark", (xs[0] + xs[-1]) / 2, s * .085, .03, xs[-1] - xs[0] + .07, .022, .06)
            return
        for x in xs:
            for s in (-1, 1):
                C(P, "Wheel", "dark", x, s * .085, .035, .035, .022, ax='y', segs=(6, 4))
    B(P, "EngBase", "accent0", .27, 0, .075, .25, .13, .05)
    C(P, "Boiler", "accent1", .3, 0, .14, .05, .17, ax='x', segs=(8, 6))
    B(P, "Cab", "accent0", .18, 0, .15, .08, .13, .1, .008)
    if LOD == 0:
        B(P, "CabRoof", "dark", .18, 0, .205, .1, .15, .014)
    C(P, "Chimney", "dark", .35, 0, .185, .016, .03, segs=(6, 4))
    wheels((.2, .28, .36))
    for x, c, load in ((-.06, "ochre", "log"), (-.3, "accent1", "crate")):
        B(P, "Wagon", c, x, 0, .085, .2, .13, .07, .008)
        if load == "log":
            C(P, "Log", "wood", x, 0, .145, .035, .16, ax='x', segs=(6, 4))
        else:
            B(P, "Crate", "cream", x, 0, .15, .12, .09, .06)
        wheels((x - .06, x + .06))
    if LOD == 0:
        for x in (.105, -.18):
            B(P, "Link", "dark", x, 0, .07, .07, .02, .015)


def g_trough(P):
    X(P, "Trough", "wood_d", [(-.36, .06), (.36, .06), (.4, .26), (-.4, .26)], -.12, .12)
    for sx in (-1, 1):
        B(P, "Foot", "wood_d", sx * .28, 0, .03, .06, .22, .06)
    B(P, "Soil", "soil", 0, 0, .262, .74, .2, .01)
    for i in range(6) if LOD == 0 else (0, 2, 3, 5):
        x, y, z = -.3 + i * .12, (.03 if i % 2 else -.03), .36 + (i % 2) * .06
        if LOD == 0:
            R(P, "Stem", "leaf", (x, 0, .26), (x, y, z), .008, THIN)
        S(P, "Bloom", ("accent0", "ochre", "cream", "plum", "accent0", "cream")[i], x, y, z + .02, .085, .085, .07, SM)
    for i in range(5) if LOD == 0 else (0, 2, 4):
        S(P, "Leaf_Tuft", "leaf" if i % 2 else "leaf_l", -.24 + i * .12, (-.04 if i % 2 else .04), .3, .12 if LOD == 0 else .2, .1, .09, SM)


def g_tv(P):
    B(P, "Cabinet", "wood", 0, 0, .26, 1.0, .38, .34, .015)
    for sx in (-1, 1):
        for sy in (-1, 1):
            R(P, "Leg", "wood_d", (sx * .44, sy * .14, .012), (sx * .42, sy * .13, .09), .022, THIN)
    for s in (-1, 1):
        B(P, "Door", "wood_d", s * .24, -.192, .26, .44, .01, .28, .004)
        R(P, "Aerial", "steel", (0, .05, .9), (s * .2, .05, .98), .006, THIN)
        if LOD == 0:
            B(P, "Pull", "brass", s * .04, -.196, .26, .016, .006, .06)
    B(P, "Set", "dark", 0, .01, .67, .78, .32, .46, .03)
    B(P, "Screen", "glass", -.08, -.152, .67, .52, .008, .36, .02)
    B(P, "Panel", "steel", .28, -.152, .67, .14, .008, .36)
    B(P, "AerialBase", "dark", 0, .05, .905, .08, .05, .02)
    if LOD == 0:
        for z in (.76, .64):
            C(P, "Knob", "brass", .28, -.156, z, .028, .016, ax='y', segs=(6, 4))


def g_ukulele(P):
    stand(P, .3, .42, .08)
    T = sub(P, "Lean", (0, -.03, .1), (math.radians(-9), 0, 0))
    half = [(0, 0), (.07, 0), (.1, .04), (.105, .1), (.085, .16), (.075, .19), (.085, .24), (.065, .28), (0, .3)]
    X(T, "Body", "wood", mir(half), -.035, .035)
    X(T, "Top", "accent0", mir([(x * .92, .012 + z * .92) for x, z in half]), -.04, -.035)
    C(T, "Hole", "dark", 0, -.042, .19, .028, .006, ax='y', segs=(8, 5))
    B(T, "Neck", "wood_d", 0, -.02, .4, .036, .025, .22)
    if LOD == 0:
        B(T, "Bridge", "dark", 0, -.044, .08, .07, .01, .018)
        B(T, "Fret", "dark", 0, -.037, .385, .032, .006, .25)
    B(T, "Headst", "wood_d", 0, -.015, .55, .055, .022, .08, .006)


def g_vase(P):
    L(P, "Vase", "accent0", 0, 0, [(.045, 0), (.085, .09), (.095, .17), (.05, .27), (.04, .3), (.06, .34)], top="dark")


def g_violin(P):
    stand(P, .3, .4, .08)
    T = sub(P, "Lean", (0, -.03, .1), (math.radians(-9), 0, 0))
    half = [(0, 0), (.07, 0), (.1, .04), (.1, .1), (.075, .14), (.06, .175), (.075, .21), (.085, .26), (.07, .32), (0, .35)]
    X(T, "Body", "wood_d", mir(half), -.028, .028)
    X(T, "Top", "wood", mir([(x * .93, .012 + z * .93) for x, z in half]), -.033, -.028)
    B(T, "Board", "dark", 0, -.042, .34, .026, .008, .27)
    B(T, "Neck", "wood_d", 0, -.02, .41, .028, .03, .14)
    C(T, "Scroll", "wood_d", 0, -.01, .5, .022, .04, ax='x', segs=(6, 4))
    R(P, "Bow", "wood_d", (.115, -.02, .08), (.125, .07, .64), .006, THIN)
    if LOD == 0:
        B(T, "Tail", "dark", 0, -.036, .07, .03, .008, .1)
        B(T, "Bridge", "cream", 0, -.04, .16, .045, .012, .012)
        B(P, "Frog", "dark", .116, -.012, .1, .016, .02, .04)


def g_weights(P):
    for ex in (-.32, .1):
        B(P, "Stand", "iron", ex, 0, .07, .03, .3, .14)
    for sy in (-.07, .07):
        R(P, "Rail", "steel", (-.32, sy, .14), (.1, sy, .14), .012, THIN)
    for x, r in ((-.25, .06), (-.11, .052), (.03, .045)) if LOD == 0 else ((-.22, .06), (-.02, .05)):
        R(P, "Bar", "steel", (x, -.11, .18), (x, .11, .18), .012, THIN)
        for s in (-1, 1):
            C(P, "Plate", "dark", x, s * .125, .18, r, .05, ax='y', segs=(8, 5))
    S(P, "Bell", "dark", .25, 0, .085, .17, .17, .17)
    R(P, "BellA", "dark", (.21, 0, .15), (.215, 0, .23), .016, THIN)
    R(P, "BellB", "dark", (.29, 0, .15), (.285, 0, .23), .016, THIN)
    R(P, "BellC", "dark", (.208, 0, .23), (.292, 0, .23), .016, THIN)


MAKERS = {k: v for k, v in dict(globals()).items() if k.startswith("g_") and callable(v)}


# ---- out: triangles, the two levels of detail, the file for the slice -----------------------------------------------
def shade(ob):
    """Round shapes are smooth, boxes keep their edges; leaves are faceted, as the trees' crowns are."""
    me = ob.data
    if "Leaf" in ob.name or "Frond" in ob.name:
        return
    for p in me.polygons:
        p.use_smooth = True
    try:
        me.set_sharp_from_angle(angle=math.radians(50 if LOD == 0 else 65))
    except Exception:
        for p in me.polygons:
            p.use_smooth = False


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
            col = PIDX[ev.material_slots[tri.material_index].material.name[2:]]
            vs = []
            for l in tri.loops:
                p = mw @ me.vertices[me.loops[l].vertex_index].co
                vs.append((p, (nm @ cn[l].vector).normalized()))
            if (vs[1][0] - vs[0][0]).cross(vs[2][0] - vs[0][0]).length > 1e-9:
                out.append((vs, col))
        ev.to_mesh_clear()
    return out


def build(key, lod):
    """One good at one level of detail: its triangles, lightened if it has more than its most."""
    global LOD
    for ob in list(COL.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    LOD = lod
    del CUR[:]
    P = bpy.data.objects.new("Good_" + key, None)
    COL.objects.link(P)
    MAKERS[key](P)
    objs = list(CUR)
    for ob in objs:
        shade(ob)
    bpy.context.view_layer.update()
    tris = triangles(objs)
    n, most = len(tris), (NEAR, FAR)[lod]
    if n > most:
        for ob in objs:
            md = ob.modifiers.new("Fewer", 'DECIMATE')
            setp(md, "ratio", most / float(n) * 0.97)
        tris = triangles(objs)
    return tris, n


def pack(tris):
    """Triangles as shared points: each point has its place, its normal and its colour."""
    idx, pts, tri = {}, [], []
    for vs, col in tris:
        for p, n in vs:
            k = (round(p.x * 1000), round(p.y * 1000), round(p.z * 1000), round(n.x * 100), round(n.y * 100), round(n.z * 100), col)
            if k not in idx:
                idx[k] = len(pts)
                pts.append(k)
            tri.append(idx[k])
    return pts, tri


def mesh_of(name, pts, tri):
    """One mesh for the whole good, for the pictures and the export: the same points the slice gets."""
    me = bpy.data.meshes.new(name)
    me.from_pydata([(p[0] / 1000.0, p[1] / 1000.0, p[2] / 1000.0) for p in pts], [], [tuple(tri[i:i + 3]) for i in range(0, len(tri), 3)])
    for k, _ in PAL:
        me.materials.append(G[k])
    for poly in me.polygons:
        poly.use_smooth = True
        poly.material_index = pts[poly.vertices[0]][6]
    me.update()
    try:
        me.normals_split_custom_set_from_vertices([Vector((p[3], p[4], p[5])).normalized() for p in pts])
    except Exception:
        pass
    me.use_fake_user = True
    return me


def for_slice(pts, tri):
    p, n, c = [], [], []
    for x, y, z, nx, ny, nz, col in pts:
        p += [-x, z, y]                                        # u across, y up, v front to back
        n += [-nx, nz, ny]
        c.append(col)
    return dict(p=p, n=n, c=c, i=tri)


# ---- the pictures ------------------------------------------------------------------------------------------------
def sheet(key, kinds, meshes, sizes, lod, high=False):
    """The goods on a floor under the kit daylight, a person beside them for scale: small things in front on a low step,
    the rest behind; `high` looks down on three rows (the far models, all together)."""
    fresh("Furn_" + key)
    small = [k for k in kinds if max(sizes[k][0], sizes[k][1]) <= .62 and sizes[k][2] <= .62]
    big = [k for k in kinds if k not in small]
    if high:
        tall = [k for k in big if sizes[k][2] > .9]
        rows = [(small, -2.8, 0.0, .2), ([k for k in big if k not in tall], 0.0, 0.0, .35), (tall, 2.8, 0.0, .35)]
    else:
        rows = [(small, -2.0, .2, .2), (big, 0.7, 0.0, .35)]
    lens, cz = 50.0, (7.5 if high else 3.0)
    tan = 18.0 / lens
    dist = 6.0
    for row, y, z, gap in rows:
        w = sum(sizes[k][0] for k in row) + gap * max(0, len(row) - 1)
        last = row is rows[-1][0]
        dist = max(dist, (w / 2 + (1.7 if last else .5)) / tan - y)   # the row, and the person at the end of the last one, in the picture
        x = -w / 2
        for k in row:
            ob = bpy.data.objects.new("%s_%s" % (key, k), meshes[k][lod])
            COL.objects.link(ob)
            ob.location = (x + sizes[k][0] / 2, y, z)
            x += sizes[k][0] + gap
        if last:
            people([("PlayerA", w / 2 + .8, y)])
        elif z > 0:
            box("Step_" + key, -w / 2 - .3, w / 2 + .3, y - .45, y + .45, 0, z, K_M["plaster"], bevel=0.01)
    half = dist * tan + 3
    box("Floor_" + key, -half, half, -dist - 3, 5.2, -0.1, 0, M["boards"], bevel=0)
    box("Wall_" + key, -half, half, 4.6, 5.2, 0, 4.4, K_M["plaster"], bevel=0)
    sun_and_sky(SCENE, (0.55, -0.55, 0.62), 6.0, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.6)
    return [("cam", (0, -dist, cz), (0, 0.3, 0.5), lens)]


def export_glb(meshes, kinds):
    os.makedirs(EXP, exist_ok=True)
    scene = bpy.data.scenes.new("Export_" + NAME)
    for k in kinds:
        ob = bpy.data.objects.new(k, meshes[k][0])
        scene.collection.objects.link(ob)
    path = EXP + "/" + NAME + ".glb"
    try:
        with bpy.context.temp_override(scene=scene, view_layer=scene.view_layers[0]):
            bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_active_scene=True)
        log("wrote %s (%.0f KB)" % (path, os.path.getsize(path) / 1024))
    except Exception:
        log("glb export FAILED:\n" + traceback.format_exc())
    bpy.data.scenes.remove(scene)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(SRC, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    with open(RULES, encoding="utf-8") as f:
        pieces = json.load(f)["pieces"]
    kinds = sorted(k for k, p in pieces.items() if p.get("from") == "mall")
    missing = [k for k in kinds if k not in MAKERS]
    if missing:
        log("NO MODEL for: " + ", ".join(missing))
    kinds = [k for k in kinds if k in MAKERS and (not GOODS_ONLY or k in GOODS_ONLY)]
    try:
        fresh("Furn_build")
        for k, c in PAL:
            G[k] = flat("G_" + k, c, bump=0.1)
        models, meshes, sizes, counts, bad = {}, {}, {}, {}, []
        for key in kinds:
            try:
                want = pieces[key]["size"]
                lods, ms, ns = [], [], []
                for lod in (0, 1):
                    tris, raw = build(key, lod)
                    pts, tri = pack(tris)
                    xs, ys, zs = [p[0] / 1000.0 for p in pts], [p[1] / 1000.0 for p in pts], [p[2] / 1000.0 for p in pts]
                    over = max(max(xs) - want[0] / 2, -min(xs) - want[0] / 2, max(ys) - want[1] / 2, -min(ys) - want[1] / 2, max(zs) - want[2], -min(zs))
                    if over > TOL:
                        bad.append(key)
                        log("TOO BIG %s lod %d by %.3f m: x %.3f..%.3f y %.3f..%.3f z %.3f..%.3f, size %s" % (key, lod, over, min(xs), max(xs), min(ys), max(ys), min(zs), max(zs), want))
                    lods.append(for_slice(pts, tri))
                    ms.append(mesh_of("%s_lod%d" % (key, lod), pts, tri))
                    ns.append(len(tri) // 3)
                    if lod == 0:
                        box3 = [round(max(xs) - min(xs), 3), round(max(ys) - min(ys), 3), round(max(zs), 3)]
                    log("%s lod %d: %d triangles (%d as made), %d points" % (key, lod, len(tri) // 3, raw, len(pts)))
                models[key] = dict(name=pieces[key]["name"], size=want, fills=box3, t=ns, lods=lods)
                meshes[key], sizes[key], counts[key] = ms, want, ns
            except Exception:
                bad.append(key)
                log("FAILED %s:\n%s" % (key, traceback.format_exc()))
        near, far = [c[0] for c in counts.values()], [c[1] for c in counts.values()]
        if near:
            log("%d goods. near: %d triangles in all, %d on average, %d most. far: %d in all, %d on average, %d most" % (
                len(near), sum(near), sum(near) / len(near), max(near), sum(far), sum(far) / len(far), max(far)))
        if bad:
            log("NOT RIGHT: " + ", ".join(sorted(set(bad))))
        if not GOODS_ONLY and not bad:
            out = dict(note="Generated by Tools/blender/mall_goods_31.py: the mall's goods for the home (FURN-065). Do not edit.",
                       palette=[[k, [round(v, 3) for v in c]] for k, c in PAL], units="mm; normals x100; u across, y up, v front to back",
                       most=[NEAR, FAR], models=models)
            with open(DEST, "w", encoding="utf-8", newline="\n") as f:
                f.write("// " + out["note"] + "\nwindow.GOODS_MODELS = " + json.dumps(out, separators=(",", ":")) + ";\n")
            log("wrote %s (%.0f KB)" % (DEST, os.path.getsize(DEST) / 1024))
            with open(OUT_DIR + "/counts.json", "w", encoding="utf-8", newline="\n") as f:
                json.dump(dict(most=[NEAR, FAR], triangles=counts), f, indent=1, sort_keys=True)
            export_glb(meshes, kinds)
        if not NORENDER:
            shown = [k for k in kinds if k in meshes]
            n3 = -(-len(shown) // 3)
            for key, part, lod, res in (("sheet_1", shown[:n3], 0, (2200, 1000)), ("sheet_2", shown[n3:2 * n3], 0, (2200, 1000)),
                                        ("sheet_3", shown[2 * n3:], 0, (2200, 1000)), ("sheet_far", shown, 1, (2600, 1100))):
                if not part:
                    continue
                try:
                    cams = sheet(key, part, meshes, sizes, lod, high=(lod == 1))
                    render(SCENE, key, cams, -0.5, res)
                except Exception:
                    log("FAILED %s:\n%s" % (key, traceback.format_exc()))
            try:
                bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
                log("saved .blend")
            except Exception:
                log("save failed:\n" + traceback.format_exc())
    except Exception:
        log("FAILED:\n" + traceback.format_exc())
    log("DONE in %.0fs" % (time.time() - _t0))


main()
