"""Wasted - look test 03: three playful art directions, street and characters together.

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/look_test_03.py").read(), "look_test_03.py", "exec"))

Each look is one preset (palette, shapes, light, character proportions). The same small
plaza and the same six characters are built once per look, then rendered wide and close.
"""
import bpy, bmesh, math, random, os, time, traceback
from mathutils import Vector, Euler

# reuse helpers: materials/boxes (style_test_02) and smooth primitives/hair (character_test_02)
_h = open("C:/Projects/Wasted/Tools/blender/character_test_02.py", encoding="utf-8").read().split("\n# ---- one character")[0]
exec(compile(_h, "character_test_02_helpers", "exec"), globals())

NAME = "look_test_03"
LOG = OUT + "/" + NAME + "_log.txt"
RES = (1440, 900)
ONLY = ("A_toybox", "B_pastel_dusk")   # set to None to render every look
SAMPLES = 96

# ---- the three looks -----------------------------------------------------------
LOOKS = [
    dict(key="A_toybox",
         walls=[(0.60, 0.20, 0.12), (0.70, 0.45, 0.10), (0.08, 0.33, 0.36), (0.66, 0.58, 0.42), (0.30, 0.12, 0.26)],
         trim=(0.72, 0.66, 0.52), roof=(0.35, 0.12, 0.10), ground=((0.46, 0.38, 0.29), (0.60, 0.52, 0.41)),
         accents=[(0.70, 0.13, 0.10), (0.80, 0.52, 0.07), (0.08, 0.36, 0.46), (0.82, 0.78, 0.68)],
         cloth=[(0.08, 0.33, 0.36), (0.70, 0.45, 0.10), (0.60, 0.20, 0.12), (0.30, 0.12, 0.26), (0.66, 0.58, 0.42), (0.16, 0.30, 0.52)],
         leaf=((0.14, 0.38, 0.12), (0.30, 0.52, 0.14)), glass=(0.05, 0.10, 0.16),
         horizon=(0.62, 0.74, 0.90), zenith=(0.22, 0.46, 0.88), sky=1.0,
         sun_dir=(-0.45, -0.55, 0.70), sun_color=(1.0, 0.93, 0.80), sun=4.2, sun_angle=3.0,
         fog=0.004, fog_color=(0.90, 0.94, 1.0), exposure=-0.35,
         round=0.20, wonk=0.0, win="arch", roofs="flat", tex=0.5, lit=0.0, smooth_leaf=True,
         char=dict(head=1.5, leg=0.78, torso=0.9, thick=1.35, wide=1.12, pear=1.0, eyes="full", eye=1.25,
                   cheeks=True, nose=0.8)),
    dict(key="B_pastel_dusk",
         walls=[(0.36, 0.29, 0.50), (0.60, 0.33, 0.36), (0.32, 0.50, 0.44), (0.64, 0.54, 0.34), (0.28, 0.38, 0.58)],
         trim=(0.70, 0.62, 0.60), roof=(0.16, 0.13, 0.24), ground=((0.22, 0.19, 0.28), (0.34, 0.28, 0.35)),
         accents=[(0.78, 0.30, 0.36), (0.82, 0.56, 0.20), (0.26, 0.52, 0.58), (0.82, 0.76, 0.70)],
         cloth=[(0.60, 0.33, 0.36), (0.28, 0.38, 0.58), (0.64, 0.54, 0.34), (0.32, 0.50, 0.44), (0.36, 0.29, 0.50), (0.74, 0.66, 0.60)],
         leaf=((0.62, 0.30, 0.38), (0.78, 0.48, 0.52)), glass=(0.06, 0.07, 0.14),
         horizon=(0.95, 0.55, 0.42), zenith=(0.30, 0.24, 0.55), sky=0.75,
         sun_dir=(-0.80, -0.42, 0.42), sun_color=(1.0, 0.56, 0.36), sun=5.0, sun_angle=5.0,
         fog=0.011, fog_color=(1.0, 0.82, 0.80), exposure=-0.1,
         round=0.07, wonk=0.03, win="rect", roofs="pitch", tex=0.6, lit=0.6, smooth_leaf=True,
         char=dict(head=1.12, leg=1.15, torso=0.82, thick=0.72, wide=0.9, pear=1.0, eyes="dot", eye=0.9,
                   cheeks=False, nose=0.9)),
    dict(key="C_paper_town",
         walls=[(0.62, 0.42, 0.08), (0.62, 0.22, 0.16), (0.06, 0.14, 0.26), (0.70, 0.64, 0.50), (0.30, 0.40, 0.28)],
         trim=(0.74, 0.69, 0.57), roof=(0.05, 0.08, 0.14), ground=((0.60, 0.54, 0.42), (0.60, 0.54, 0.42)),
         accents=[(0.62, 0.22, 0.16), (0.06, 0.14, 0.26), (0.62, 0.42, 0.08), (0.74, 0.69, 0.57)],
         cloth=[(0.06, 0.14, 0.26), (0.62, 0.22, 0.16), (0.62, 0.42, 0.08), (0.30, 0.40, 0.28), (0.70, 0.64, 0.50), (0.10, 0.30, 0.28)],
         leaf=((0.10, 0.30, 0.28), (0.20, 0.42, 0.34)), glass=(0.05, 0.09, 0.15),
         horizon=(0.80, 0.84, 0.82), zenith=(0.50, 0.68, 0.80), sky=0.9,
         sun_dir=(-0.45, -0.55, 0.70), sun_color=(1.0, 0.95, 0.88), sun=4.0, sun_angle=0.6,
         fog=0.003, fog_color=(0.95, 0.96, 0.94), exposure=-0.35,
         round=0.02, wonk=0.10, win="round", roofs="pitch", tex=0.0, lit=0.0, smooth_leaf=False,
         char=dict(head=1.35, leg=0.55, torso=1.12, thick=1.1, wide=1.2, pear=1.3, eyes="dot", eye=1.3,
                   cheeks=True, nose=0.0)),
]

PEOPLE = [
    dict(name="PlayerA", height=1.68, build=0.92, skin=(0.62, 0.42, 0.30), hair="long", hair_color=(0.28, 0.10, 0.05),
         eyes=(0.14, 0.36, 0.20), pos=(-1.6, 3.0)),
    dict(name="PlayerB", height=1.80, build=1.08, skin=(0.30, 0.19, 0.13), hair="afro", hair_color=(0.03, 0.025, 0.02),
         sleeves="long", bottom_type="shorts", extras=("bag",), brow=1.3, pos=(0.15, 3.4)),
    dict(name="Courier", height=1.78, build=1.0, skin=(0.84, 0.64, 0.52), hair="short", hair_color=(0.55, 0.40, 0.18),
         eyes=(0.18, 0.34, 0.55), bottom_type="shorts", extras=("cap", "bag"), pos=(1.9, 3.1)),
    dict(name="Elder", height=1.58, build=1.06, skin=(0.70, 0.50, 0.38), hair="bun", hair_color=(0.55, 0.55, 0.56),
         jacket=True, bottom_type="skirt", skirt_len=0.62, eye_open=0.85, brow=0.8, pos=(3.7, 3.7)),
    dict(name="OfficeWorker", height=1.74, build=0.96, skin=(0.47, 0.31, 0.22), hair="bob", hair_color=(0.03, 0.025, 0.02),
         jacket=True, bottom_type="skirt", lip=1.3, pos=(-3.4, 3.7)),
    dict(name="Shopkeeper", height=1.72, build=1.14, skin=(0.78, 0.56, 0.44), hair="side", hair_color=(0.10, 0.06, 0.04),
         sleeves="long", extras=("apron", "moustache"), face_w=1.05, nose=1.15, pos=(-6.0, 5.0)),
]


def dim(c, k):
    return (c[0] * k, c[1] * k, c[2] * k)


_W = {}


def surf(c, st, brick=False, floor=False, c2=None):
    """Wall/ground material: flat colour when tex is 0, otherwise gently mottled."""
    key = (c, c2, brick, floor, st["key"])
    if key not in _W:
        if st["tex"] <= 0:
            _W[key] = solid(c, 0.95)
        else:
            _W[key] = painted("M_Surf_%02d" % len(_W), dim(c, 0.86), c2 or dim(c, 1.1), noise_scale=0.6,
                              bump=0.2 * st["tex"], grime=0.78, brick_scale=(0.6 if brick else None), floor=floor)
    return _W[key]


# ---- shapes --------------------------------------------------------------------
def wbox(name, x0, x1, y0, y1, z0, z1, mat, bevel, taper=1.0, lean=0.0, ridge=False):
    """Box whose top can taper, lean sideways, or pinch to a ridge (roof)."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    sx, sy, sz = x1 - x0, y1 - y0, z1 - z0
    for v in bm.verts:
        top = v.co.z > 0
        v.co = Vector((v.co.x * sx * (taper if top else 1.0) + (lean if top else 0.0),
                       v.co.y * sy * (0.03 if (top and ridge) else 1.0), v.co.z * sz))
    ob = add_obj(name, bm, mat, loc=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), bevel=bevel)
    if bevel >= 0.1 and ob.modifiers:
        ob.modifiers[0].segments = 4
    return ob


def awning(name, x0, x1, y0, z, depth, m1, m2):
    n = max(2, int(round((x1 - x0) / 0.45)))
    sw = (x1 - x0) / n
    for i in range(n):
        box(name, x0 + i * sw, x0 + (i + 1) * sw, y0 - depth, y0, z, z + 0.06, m1 if i % 2 == 0 else m2,
            bevel=0, rot=(math.radians(18), 0, 0))


def window(name, x, z, w, h, y0, st, glass, trim, rng, accents, leafm):
    if st["win"] == "round":
        r = w * 0.62
        cyl_y(name + "_F", x, y0 - 0.03, z + h / 2, r + 0.1, 0.16, trim, segs=28)
        cyl_y(name + "_G", x, y0 - 0.07, z + h / 2, r, 0.12, glass, segs=28)
        return
    bv = min(0.04, st["round"])
    box(name + "_F", x - w / 2 - 0.09, x + w / 2 + 0.09, y0 - 0.08, y0 + 0.1, z - 0.09, z + h + 0.09, trim, bevel=bv)
    box(name + "_G", x - w / 2, x + w / 2, y0 - 0.11, y0 + 0.05, z, z + h, glass, bevel=0)
    if st["win"] == "arch":
        cyl_y(name + "_FA", x, y0, z + h, w / 2 + 0.09, 0.18, trim, segs=28)
        cyl_y(name + "_GA", x, y0 - 0.03, z + h, w / 2, 0.15, glass, segs=28)
    else:
        sm = solid(accents[rng.randrange(3)], 0.8)
        for sg in (-1, 1):
            box(name + "_Sh", x + sg * (w / 2 + 0.09) + (0 if sg > 0 else -0.36), x + sg * (w / 2 + 0.09) + (0.36 if sg > 0 else 0),
                y0 - 0.07, y0 + 0.02, z, z + h, sm, bevel=0.01)
    if rng.random() < 0.4:
        box(name + "_Pl", x - w / 2 - 0.05, x + w / 2 + 0.05, y0 - 0.32, y0 - 0.06, z - 0.3, z - 0.08, solid(accents[0], 0.8), bevel=0.03)
        for k in range(4):
            ellipsoid(name + "_Fl", Vector((x - w / 2 + 0.1 + k * (w - 0.2) / 3, y0 - 0.19, z - 0.03)), (0.11, 0.11, 0.1),
                      leafm if k % 2 else solid(accents[1 + k % 3], 0.8))


def building(i, x0, x1, y0, h, wall, st, rng, M):
    w = x1 - x0
    cx = (x0 + x1) / 2
    depth = 6.0
    taper = 1.0 - st["wonk"] * rng.uniform(0.2, 1.0)
    lean = st["wonk"] * rng.uniform(-1, 1) * h * 0.35

    def fx(x, z):
        t = z / h
        return cx + (x - cx) * (1 + (taper - 1) * t) + lean * t

    n = "B%d" % i
    wbox(n + "_Body", x0 + 0.04, x1 - 0.04, y0, y0 + depth, 0, h, wall, st["round"], taper, lean)
    box(n + "_Base", x0, x1, y0 - 0.1, y0 + 0.2, 0, 0.45, M["trim"], bevel=min(0.05, st["round"]))
    shop = i % 2 == 0
    side = 1 if i % 3 else -1
    dx = cx + side * (w / 2 - 1.05)
    dm = solid(st["accents"][i % 3], 0.7)
    box(n + "_DoorF", dx - 0.68, dx + 0.68, y0 - 0.06, y0 + 0.1, 0, 2.38, M["trim"], bevel=min(0.04, st["round"]))
    box(n + "_Door", dx - 0.55, dx + 0.55, y0 - 0.1, y0 + 0.05, 0, 2.25, dm, bevel=0.03)
    if st["win"] == "arch":
        cyl_y(n + "_DoorFA", dx, y0 + 0.02, 2.25, 0.68, 0.16, M["trim"], segs=28)
        cyl_y(n + "_DoorA", dx, y0 - 0.025, 2.25, 0.55, 0.14, dm, segs=28)
    ellipsoid(n + "_Knob", Vector((dx + 0.36, y0 - 0.12, 1.1)), (0.05, 0.05, 0.05), solid(st["accents"][1], 0.3))
    box(n + "_Step", dx - 0.8, dx + 0.8, y0 - 0.5, y0, 0, 0.14, M["trim"], bevel=min(0.04, st["round"]))
    z_up = 3.6
    if shop:
        sx0, sx1 = (x0 + 0.5, dx - 1.0) if side > 0 else (dx + 1.0, x1 - 0.5)
        box(n + "_ShopF", sx0 - 0.1, sx1 + 0.1, y0 - 0.07, y0 + 0.1, 0.8, 2.5, M["trim"], bevel=min(0.04, st["round"]))
        box(n + "_ShopG", sx0, sx1, y0 - 0.1, y0 + 0.05, 0.9, 2.4, M["lit"] if st["lit"] > 0 else M["glass"], bevel=0)
        awning(n + "_Awn", x0 + 0.25, x1 - 0.25, y0, 2.75, 1.15, solid(st["accents"][i % 3], 0.85), solid(st["accents"][3], 0.85))
        z_up = 4.1
    else:
        gx = cx - side * (w / 2 - 1.3)
        window(n + "_GW", gx, 1.15, 0.95, 1.2, y0, st, M["glass"], M["trim"], rng, st["accents"], M["leaf"])
    floors = max(1, int((h - z_up - 0.5) / 2.45))
    cols = max(2, int(w / 1.65))
    for f in range(floors):
        zf = z_up + f * 2.45
        for k in range(cols):
            xk = fx(x0 + w * (k + 0.5) / cols, zf + 0.6)
            g = M["lit"] if rng.random() < st["lit"] else M["glass"]
            window("%s_W%d%d" % (n, f, k), xk, zf, 0.85, 1.25, y0, st, g, M["trim"], rng, st["accents"], M["leaf"])
    tx0, tx1 = fx(x0 + 0.04, h), fx(x1 - 0.04, h)
    if st["roofs"] == "pitch":
        wbox(n + "_Roof", tx0 - 0.25, tx1 + 0.25, y0 - 0.3, y0 + depth + 0.2, h, h + w * 0.36, M["roof"], 0.03, ridge=True)
        box(n + "_Chim", (tx0 + tx1) / 2 + w * 0.18, (tx0 + tx1) / 2 + w * 0.18 + 0.55, y0 + 1.6, y0 + 2.15, h + 0.2, h + w * 0.36 + 0.5,
            wall, bevel=0.02)
    else:
        box(n + "_Roof", tx0 - 0.18, tx1 + 0.18, y0 - 0.22, y0 + depth + 0.1, h, h + 0.34, M["trim"], bevel=min(0.12, st["round"]))
        for k in range(2):
            cyl(n + "_Pot", tx0 + w * (0.3 + 0.4 * k), y0 + 1.2, h + 0.34, 0.22, 0.55, M["roof"], segs=14)


# ---- one character, driven by the look's character style -------------------------
def character2(sp, cs, ox, oy=0.0):
    S = sp.get("height", 1.72) / 1.72
    W = sp.get("build", 1.0) * cs["wide"]
    T = cs["thick"]
    fw = sp.get("face_w", 1.0)
    n = sp["name"]
    legk, tork = cs["leg"], cs["torso"]

    def zm(z):
        return z * legk if z <= 0.93 else 0.93 * legk + (z - 0.93) * tork

    def P(x, y, z):
        return Vector((ox + x * S, oy + y * S, zm(z) * S))

    skin = solid(sp["skin"], 0.55)
    top = solid(sp["top"], 0.85)
    outer = solid(sp["jacket"], 0.85) if "jacket" in sp else top
    bot = solid(sp["bottom"], 0.85)
    shoe = solid(sp.get("shoes", (0.10, 0.08, 0.07)), 0.6)
    hairc = sp.get("hair_color", (0.08, 0.05, 0.04))
    hair = solid(hairc, 0.6)
    dark = solid((0.03, 0.02, 0.02), 0.25)
    long_sleeves = sp.get("sleeves", "short") == "long" or "jacket" in sp
    kind = sp.get("bottom_type", "trousers")
    extras = sp.get("extras", ())
    chest = 0.172 * W
    waist = 0.138 * W * cs["pear"]
    hip = 0.16 * W * cs["pear"]
    front = -chest * 0.62
    lx = 0.088 * max(1.0, cs["pear"] * 0.9)

    for sg in (-1, 1):
        ellipsoid(n + "_Shoe", P(sg * lx, -0.035, 0.0) + Vector((0, 0, 0.045 * T * S)), (0.052 * T * S, 0.125 * T * S, 0.045 * T * S), shoe)
        limb(n + "_Shin", P(sg * lx, 0, 0.09), P(sg * lx, 0, 0.50), 0.05 * T * S, 0.062 * T * S, bot if kind == "trousers" else skin)
        limb(n + "_Thigh", P(sg * lx, 0, 0.50), P(sg * lx, 0, 0.87), 0.066 * T * S, 0.088 * T * S,
             bot if kind in ("trousers", "shorts") else skin)
    ellipsoid(n + "_Pelvis", P(0, 0, 0.93), (hip * S, 0.108 * T * S, 0.118 * tork * S), bot)
    if kind == "skirt":
        ln = sp.get("skirt_len", 0.42)
        limb(n + "_Skirt", P(0, 0, 1.0), P(0, 0, 1.0 - ln), 0.172 * W * cs["pear"] * S, (0.19 + 0.1 * ln) * W * cs["pear"] * S, bot,
             flat=0.78, cap=0.12)
    limb(n + "_Torso", P(0, 0, 1.02), P(0, 0, 1.36), waist * S, chest * S, outer, flat=0.62 * (1 + 0.25 * (T - 1)), cap=0.3)
    sh = chest + 0.018
    limb(n + "_Yoke", P(-sh, 0, 1.375), P(sh, 0, 1.375), 0.052 * T * S, 0.052 * T * S, outer)
    for sg in (-1, 1):
        elbow = P(sg * (sh + 0.03 + 0.03 * (cs["pear"] - 1) * 4), 0, 1.10)
        limb(n + "_UpperArm", P(sg * sh, 0, 1.375), elbow, 0.052 * T * S, 0.044 * T * S, outer)
        wrist = P(sg * (sh + 0.045 + 0.05 * (cs["pear"] - 1) * 4), -0.03, 0.86)
        limb(n + "_Forearm", elbow, wrist, 0.043 * T * S, 0.035 * T * S, outer if long_sleeves else skin)
        ellipsoid(n + "_Hand", wrist + Vector((sg * 0.004, -0.01, -0.07 * tork)) * S, (0.032 * T * S, 0.045 * T * S, 0.058 * T * S), skin)
    limb(n + "_Neck", P(0, 0.005, 1.39), P(0, 0, 1.50), 0.052 * T * S, 0.05 * T * S, skin)
    if "jacket" in sp:
        limb(n + "_Shirt", P(0, front - 0.002, 1.05), P(0, front + 0.014, 1.385), 0.034 * S, 0.034 * S, top, flat=0.35, cap=0.3)
    if "apron" in extras:
        limb(n + "_Apron", P(0, -0.088 * T, 0.68), P(0, -0.093 * T, 1.22), 0.118 * W * S, 0.10 * W * S,
             solid(sp.get("apron_color", (0.34, 0.16, 0.11)), 0.85), flat=0.26, cap=0.2)
    if "bag" in extras:
        strap = solid((0.13, 0.09, 0.06), 0.7)
        limb(n + "_Strap", P(-0.15, front - 0.006, 1.40), P(0.18, front + 0.008, 1.0), 0.012 * S, 0.012 * S, strap)
        ellipsoid(n + "_Bag", P(0.2, -0.10 * T, 0.93), (0.05 * S, 0.10 * S, 0.09 * S), strap)

    # head, in head space
    k = cs["head"]
    K = k * S
    rx, ry, rz = 0.104 * fw, 0.110, 0.130
    hz = zm(1.475) + rz * k

    def H(x, y, z):
        return Vector((ox + x * K, oy + y * K, (hz + z * k) * S))

    def face_y(x, z):
        zu = z / rz
        s = jaw(zu)
        inside = 1.0 - (x / (rx * s)) ** 2 - zu * zu
        return -ry * s * math.sqrt(max(inside, 0.0))

    def F(x, z, out=0.0):
        return H(x, face_y(x, z) - out, z)

    bm = uvs(32, 20)
    for v in bm.verts:
        s = jaw(v.co.z)
        v.co = Vector((v.co.x * rx * s * K, v.co.y * ry * s * K, v.co.z * rz * K))
    finish(n + "_Head", bm, skin, H(0, 0, 0))
    for sg in (-1, 1):
        ellipsoid(n + "_Ear", H(sg * rx * 0.97, 0.012, -0.012), (0.012 * K, 0.022 * K, 0.032 * K), skin)

    es = sp.get("eye_size", 1.0) * cs["eye"]
    ex = 0.043 * fw
    ez = 0.008
    yaw = math.asin(min(ex / rx, 1.0)) * 0.55
    browc = solid(dim(hairc, 0.6), 0.6)
    bt = sp.get("brow", 1.0)
    white = solid((0.80, 0.78, 0.74), 0.3)
    for sg in (-1, 1):
        rot = (0.0, 0.0, sg * yaw)
        if cs["eyes"] == "dot":
            ellipsoid(n + "_Eye", F(sg * ex, ez, 0.001), (0.0115 * es * K, 0.006 * K, 0.017 * es * K), dark, rot)
            ellipsoid(n + "_Glint", F(sg * ex + 0.003, ez + 0.007 * es, 0.0068), (0.0034 * K,) * 3, white)
        else:
            ew, eh, ir = 0.026 * es, 0.019 * es * sp.get("eye_open", 1.0), 0.0125 * es
            irz = min(ir, eh * 0.92)
            ellipsoid(n + "_EyeWhite", F(sg * ex, ez, -0.005), (ew * K, 0.011 * K, eh * K), white, rot)
            ellipsoid(n + "_Iris", F(sg * ex, ez, 0.0035), (ir * K, 0.004 * K, irz * K), solid(sp.get("eyes", (0.16, 0.09, 0.05)), 0.2), rot)
            ellipsoid(n + "_Pupil", F(sg * ex, ez, 0.0065), (0.006 * es * K, 0.003 * K, min(0.006 * es, irz) * K), dark, rot)
            ellipsoid(n + "_Glint", F(sg * ex + 0.004, ez + 0.005, 0.0088), (0.0026 * K,) * 3, white)
            ellipsoid(n + "_Lash", F(sg * ex, ez + eh * 0.82, 0.001), (ew * 1.08 * K, 0.009 * K, eh * 0.3 * K), dark, rot)
        by = ez + (0.034 if cs["eyes"] == "dot" else 0.040) * max(1.0, es * 0.9)
        limb(n + "_Brow", F(sg * (ex - 0.02), by, 0.002), F(sg * (ex + 0.025), by + 0.002, 0.002),
             0.0044 * bt * K, 0.003 * bt * K, browc)
        if cs["cheeks"]:
            ellipsoid(n + "_Cheek", F(sg * 0.06 * fw, -0.036, -0.002), (0.017 * K, 0.006 * K, 0.011 * K),
                      solid((min(sp["skin"][0] * 1.12, 0.9), sp["skin"][1] * 0.62, sp["skin"][2] * 0.6), 0.6), (0, 0, sg * yaw * 1.6))
    if cs["nose"] > 0:
        ns = sp.get("nose", 1.0) * cs["nose"]
        ellipsoid(n + "_Nose", F(0, -0.030, 0.003), (0.0115 * ns * K, 0.013 * ns * K, 0.017 * ns * K), skin)
    if cs["eyes"] == "dot":
        for sg in (-1, 1):
            limb(n + "_Smile", F(sg * 0.02, -0.066, 0.001), F(0, -0.075, 0.001), 0.0026 * K, 0.0032 * K, dark)
    else:
        lip = solid(sp.get("lips", (sp["skin"][0] * 0.62, sp["skin"][1] * 0.36, sp["skin"][2] * 0.34)), 0.5)
        ellipsoid(n + "_Mouth", F(0, -0.072, 0.0005), (0.021 * K, 0.005 * K, 0.0055 * sp.get("lip", 1.0) * K), lip)
        for sg in (-1, 1):
            limb(n + "_MouthCorner", F(sg * 0.015, -0.0718, 0.0), F(sg * 0.027, -0.068, -0.0005), 0.003 * K, 0.002 * K, lip)
    if "moustache" in extras:
        ellipsoid(n + "_Moustache", F(0, -0.056, 0.004), (0.028 * K, 0.007 * K, 0.0075 * K), hair)

    style = sp.get("hair", "short")
    cap_r = ((rx + 0.020) * K, (ry + 0.022) * K, (rz + 0.020) * K)
    if style == "afro":
        hair_cap(n + "_Hair", H(0, 0.012, 0.035), ((rx + 0.055) * K, (ry + 0.055) * K, (rz + 0.06) * K), hair, "afro", jitter=0.006 * K)
    elif style != "bald":
        hair_cap(n + "_Hair", H(0, 0.012, 0.014), cap_r, hair, style)
    if style in ("short", "side"):
        off = 0.022 if style == "side" else 0.0
        ellipsoid(n + "_HairTop", H(off, -0.018, 0.108), (0.086 * K, 0.104 * K, 0.052 * K), hair, (0.0, 0.18 if style == "side" else 0.0, 0.0))
    if style == "bun":
        ellipsoid(n + "_Bun", H(0, 0.085, 0.135), (0.052 * K, 0.052 * K, 0.046 * K), hair)
    if "cap" in extras:
        capm = solid(sp.get("cap_color", (0.34, 0.16, 0.11)), 0.8)
        hair_cap(n + "_Cap", H(0, 0.006, 0.016), ((rx + 0.03) * K, (ry + 0.032) * K, (rz + 0.028) * K), capm, "dome")
        ellipsoid(n + "_Brim", H(0, -0.135, 0.062), (0.09 * K, 0.075 * K, 0.009 * K), capm, (0.12, 0.0, 0.0))


# ---- the plaza -------------------------------------------------------------------
def build(st):
    global COL
    rng = random.Random(5)
    random.seed(11)
    scene = bpy.data.scenes.new("Look_" + st["key"])
    try:
        bpy.context.window.scene = scene
    except Exception as e:
        log("could not switch window scene: %s" % e)
    COL = scene.collection
    acc = st["accents"]
    M = dict(trim=surf(st["trim"], st), roof=solid(st["roof"], 0.8), glass=solid(st["glass"], 0.2),
             lit=emissive("M_Lit_" + st["key"], (1.0, 0.55, 0.22), 1.5), leaf=solid(st["leaf"][0], 0.9),
             leaf2=solid(st["leaf"][1], 0.9), wood=solid((0.26, 0.17, 0.11), 0.8), iron=solid((0.06, 0.06, 0.08), 0.5))

    box("Ground", -45, 45, -20, 75, -0.3, 0, surf(st["ground"][0], st, brick=True, floor=True, c2=st["ground"][1]), bevel=0)

    # back row: five buildings and one gateway
    y0 = 8.0
    widths = [5.0, 4.2, 4.0, 5.2, 4.4, 4.6]
    heights = [8.5, 6.4, 7.0, 9.6, 6.0, 8.0]
    x = -sum(widths) / 2
    bi = 0
    for i, w in enumerate(widths):
        if i == 2:
            gate_wall = surf(st["walls"][3], st)
            arch_wall("Gate", x, x + w, y0, y0 + 1.2, heights[i], x + 0.75, x + w - 0.75, 3.1, gate_wall, M["trim"])
            box("Gate_Roof", x - 0.1, x + w + 0.1, y0 - 0.2, y0 + 1.4, heights[i], heights[i] + 0.34, M["trim"], bevel=min(0.12, st["round"]))
            window("Gate_W", x + w / 2, 5.0, 0.9, 1.2, y0, st, M["glass"], M["trim"], rng, acc, M["leaf"])
        else:
            building(bi, x, x + w, y0, heights[i], surf(st["walls"][bi % 5], st), st, rng, M)
            bi += 1
        x += w
    # beyond the gateway and far skyline
    for (fx0, fx1, fy, fh, ci) in ((-6, -1, 22, 9, 1), (-1, 3.5, 24, 12, 2), (3.5, 8, 22, 8, 0), (-16, -8, 30, 13, 4),
                                    (9, 17, 30, 15, 3), (-26, -17, 34, 10, 2), (18, 27, 36, 11, 1)):
        wbox("Far", fx0, fx1, fy, fy + 6, 0, fh, surf(st["walls"][ci], st), min(st["round"], 0.1),
             1.0 - st["wonk"] * 0.6, st["wonk"] * rng.uniform(-1, 1) * 2)
        if st["roofs"] == "pitch":
            wbox("Far_Roof", fx0 - 0.3, fx1 + 0.3, fy - 0.3, fy + 6.3, fh, fh + 2.2, M["roof"], 0.02, ridge=True)

    # market stall with fruit
    for (px, py, ph) in ((-7.4, 3.0, 2.35), (-4.6, 3.0, 2.35), (-7.4, 5.4, 3.1), (-4.6, 5.4, 3.1)):
        cyl("Stall_Post", px, py, 0, 0.05, ph, M["wood"], segs=10)
    awning("Stall_Awn", -7.7, -4.3, 5.6, 2.75, 2.9, solid(acc[0], 0.85), solid(acc[3], 0.85))
    box("Stall_Table", -7.5, -4.5, 3.1, 4.3, 0.78, 0.88, M["wood"], bevel=0.02)
    for px in (-7.35, -4.65):
        box("Stall_Leg", px - 0.06, px + 0.06, 3.2, 4.2, 0, 0.78, M["wood"], bevel=0.01)
    for c in range(3):
        cx0 = -7.3 + c * 0.95
        box("Stall_Crate", cx0, cx0 + 0.8, 3.25, 4.05, 0.88, 1.06, solid(dim(acc[3], 0.8), 0.9), bevel=0.02)
        fm = solid(acc[c % 3], 0.5)
        for k in range(9):
            ellipsoid("Fruit", Vector((cx0 + 0.16 + (k % 3) * 0.24, 3.41 + (k // 3) * 0.24, 1.12)), (0.11, 0.11, 0.1), fm)

    # tree, bench, lamp, planters
    cyl("Tree_Trunk", 7.2, 4.6, 0, 0.2, 3.2, M["wood"], r_top=0.14)
    for (dx, dy, dz, r, mk) in ((0, 0, 4.6, 1.75, "leaf"), (1.2, 0.3, 4.0, 1.15, "leaf2"), (-1.15, 0.2, 4.1, 1.1, "leaf2"),
                                (0.2, -0.3, 5.9, 1.05, "leaf2")):
        if st["smooth_leaf"]:
            ellipsoid("Tree_Leaf", Vector((7.2 + dx, 4.6 + dy, dz)), (r, r, r * 0.92), M[mk])
        else:
            blob("Tree_Leaf", 7.2 + dx, 4.6 + dy, dz, r, M[mk], squash=0.92)
    box("Bench_Seat", 4.6, 6.4, 6.5, 7.0, 0.42, 0.5, M["wood"], bevel=0.02)
    box("Bench_Back", 4.6, 6.4, 6.95, 7.03, 0.5, 0.95, M["wood"], bevel=0.02)
    for px in (4.75, 6.25):
        box("Bench_Leg", px - 0.05, px + 0.05, 6.55, 6.95, 0, 0.42, M["iron"], bevel=0)
    cyl("Lamp_Pole", 3.6, 6.8, 0, 0.06, 3.3, M["iron"], segs=10)
    ellipsoid("Lamp_Globe", Vector((3.6, 6.8, 3.5)), (0.24, 0.24, 0.26), emissive("M_Globe_" + st["key"], (1.0, 0.72, 0.40), 9.0 if st["lit"] > 0 else 1.2))
    for (px, py) in ((-9.6, 7.3), (9.3, 7.3), (1.9, 7.4)):
        cyl("Pot", px, py, 0, 0.3, 0.5, solid(acc[0], 0.8), segs=14, r_top=0.38)
        if st["smooth_leaf"]:
            ellipsoid("Pot_Plant", Vector((px, py, 0.9)), (0.5, 0.5, 0.48), M["leaf2"])
        else:
            blob("Pot_Plant", px, py, 0.9, 0.5, M["leaf2"])

    # bunting (and string lights at dusk)
    a, b = Vector((-10.5, 6.6, 6.3)), Vector((10.5, 6.6, 6.7))
    prev = a
    for k in range(1, 23):
        t = k / 22
        p = a.lerp(b, t) + Vector((0, 0, -1.3 * math.sin(math.pi * t)))
        limb("Bunt_Line", prev, p, 0.012, 0.012, M["iron"])
        if k < 22:
            if st["lit"] > 0 and k % 2:
                ellipsoid("Bunt_Light", p + Vector((0, 0, -0.12)), (0.09, 0.09, 0.11), emissive("M_Bulb_%d" % (k % 3), (1.0, 0.70, 0.35), 14.0))
            else:
                box("Bunt_Flag", p.x - 0.17, p.x + 0.17, p.y - 0.012, p.y + 0.012, p.z - 0.45, p.z - 0.11, solid(acc[k % 4], 0.85),
                    bevel=0, rot=(0, math.pi / 4, 0))
        prev = p

    # clouds and a balloon
    cloud = solid((0.90, 0.90, 0.90), 1.0)
    for (cx, cy, cz, s) in ((-22, 60, 22, 1.0), (4, 66, 26, 1.3), (26, 58, 20, 0.9), (-6, 52, 17, 0.6)):
        for (dx, dz, r) in ((0, 0, 4.2), (4.2, -0.6, 3.0), (-4.0, -0.8, 2.8), (1.8, 1.8, 2.6)):
            ellipsoid("Cloud", Vector((cx + dx * s, cy, cz + dz * s)), (r * s * 1.25, r * s, r * s * 0.8), cloud)
    ellipsoid("Balloon", Vector((-15.0, 42, 27)), (2.5, 2.5, 3.0), solid(acc[0], 0.8))
    ellipsoid("Balloon_Band", Vector((-15.0, 42, 27)), (2.56, 2.56, 0.9), solid(acc[3], 0.8))
    box("Balloon_Basket", -15.4, -14.6, 41.6, 42.4, 22.4, 23.1, M["wood"], bevel=0.05)

    # people
    for i, base in enumerate(PEOPLE):
        sp = dict(base)
        sp["top"] = st["cloth"][i % 6]
        sp["bottom"] = dim(st["cloth"][(i + 3) % 6], 0.55)
        if "jacket" in sp:
            sp["jacket"] = dim(st["cloth"][(i + 2) % 6], 0.7)
            sp["top"] = st["accents"][3]
        character2(sp, st["char"], base["pos"][0], base["pos"][1])

    # haze
    fm = bpy.data.materials.new("M_Fog_" + st["key"])
    try:
        fm.use_nodes = True
    except Exception:
        pass
    fm.node_tree.nodes.clear()
    fo = fm.node_tree.nodes.new("ShaderNodeOutputMaterial")
    fv = fm.node_tree.nodes.new("ShaderNodeVolumeScatter")
    sset(fv, "Color", rgba(st["fog_color"]))
    sset(fv, "Density", st["fog"])
    sset(fv, "Anisotropy", 0.5)
    fm.node_tree.links.new(fv.outputs[0], fo.inputs["Volume"])
    fog = box("Fog", -44, 44, -19, 74, -0.2, 30, fm, bevel=0)

    # light and sky
    sun = bpy.data.lights.new("Sun", 'SUN')
    sun.energy = st["sun"]
    sun.color = st["sun_color"]
    setp(sun, "angle", math.radians(st["sun_angle"]))
    so = bpy.data.objects.new("Sun", sun)
    so.rotation_euler = Vector(st["sun_dir"]).normalized().to_track_quat('Z', 'Y').to_euler()
    so.location = (-8, -10, 20)
    COL.objects.link(so)
    world = bpy.data.worlds.new("World_" + st["key"])
    scene.world = world
    try:
        world.use_nodes = True
    except Exception:
        pass
    wt = world.node_tree
    wt.nodes.clear()
    bg = wt.nodes.new("ShaderNodeBackground")
    wo = wt.nodes.new("ShaderNodeOutputWorld")
    bg.inputs[0].default_value = rgba(st["zenith"])
    bg.inputs[1].default_value = st["sky"]
    wt.links.new(bg.outputs[0], wo.inputs["Surface"])
    try:
        tc = wt.nodes.new("ShaderNodeTexCoord")
        sep = wt.nodes.new("ShaderNodeSeparateXYZ")
        ramp = wt.nodes.new("ShaderNodeValToRGB")
        e = ramp.color_ramp.elements
        e[0].position = 0.0
        e[0].color = rgba(st["horizon"])
        e[1].position = 0.45
        e[1].color = rgba(st["zenith"])
        wt.links.new(tc.outputs["Generated"], sep.inputs[0])
        wt.links.new(sep.outputs[2], ramp.inputs[0])
        wt.links.new(ramp.outputs[0], bg.inputs[0])
    except Exception:
        log("sky gradient skipped:\n" + traceback.format_exc())

    cams = []
    for (key, loc, tgt, lens) in (("wide", (0.8, -11.5, 2.0), (0.0, 7.0, 3.7), 28.0), ("people", (0.2, -3.6, 1.3), (0.2, 3.2, 1.1), 50.0)):
        cd = bpy.data.cameras.new("Cam_" + key)
        cd.lens = lens
        cd.clip_end = 400
        co = bpy.data.objects.new("Cam_" + key, cd)
        co.location = loc
        co.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        COL.objects.link(co)
        cams.append((key, co))
    return scene, cams, fog


def configure(scene, st):
    r = scene.render
    setp(r, "engine", 'CYCLES')
    c = getattr(scene, "cycles", None)
    if c is not None:
        setp(c, "device", 'CPU')
        setp(c, "samples", SAMPLES)
        setp(c, "use_adaptive_sampling", True)
        setp(c, "adaptive_threshold", 0.04)
        setp(c, "use_denoising", True)
        setp(c, "max_bounces", 5)
        setp(c, "diffuse_bounces", 3)
        setp(c, "volume_bounces", 0)
    r.resolution_x, r.resolution_y = RES
    r.resolution_percentage = 100
    setp(r.image_settings, "media_type", 'IMAGE')
    setp(r.image_settings, "file_format", 'PNG')
    setp(r.image_settings, "color_mode", 'RGB')
    vs = scene.view_settings
    setp(vs, "view_transform", 'AgX')
    setp(vs, "look", 'AgX - Medium High Contrast')
    setp(vs, "exposure", st["exposure"])


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(SRC, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    for st in LOOKS:
        if ONLY and st["key"] not in ONLY:
            continue
        try:
            t0 = time.time()
            scene, cams, fog = build(st)
            configure(scene, st)
            log("%s built: %d objects in %.1fs" % (st["key"], len(scene.objects), time.time() - t0))
            for key, cam in cams:
                t = time.time()
                scene.camera = cam
                fog.hide_render = (key == "people")
                scene.render.filepath = OUT + "/" + NAME + "_" + st["key"] + "_" + key + ".png"
                bpy.ops.render.render(write_still=True, scene=scene.name)
                log("%s %s rendered in %.1fs" % (st["key"], key, time.time() - t))
        except Exception:
            log("FAILED %s:\n%s" % (st["key"], traceback.format_exc()))
    try:
        bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
        log("saved .blend")
    except Exception:
        log("save failed:\n" + traceback.format_exc())
    log("DONE")


main()
