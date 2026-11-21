"""Wasted - old town test 04: the style-test-02 look, extended to a world and its people.

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/old_town_04.py").read(), "old_town_04.py", "exec"))

Keeps what defined style test 02: warm sandstone blocks, arches, lantern-lit shade, low sun
through haze, a muted palette with a few dull accents. Renders:
  - the original alley in the morning, now with people
  - the same alley at dusk (lanterns and windows lit)
  - a new place in the same style: a market square seen from an arcade
  - three character options (natural, soft, simple) for comparison
"""
import bpy, bmesh, math, random, os, time, traceback
from mathutils import Vector, Euler

B = "C:/Projects/Wasted/Tools/blender/"
_c = open(B + "character_test_02.py", encoding="utf-8").read().split("\n# ---- one character")[0]
exec(compile(_c, "character_helpers", "exec"), globals())
_s = open(B + "style_test_02.py", encoding="utf-8").read().split("\ndef configure(scene):")[0]
exec(compile(_s, "style_test_02_scene", "exec"), globals())
build_alley = build          # the exact scene of style test 02

NAME = "old_town_04"
LOG = OUT + "/" + NAME + "_log.txt"
ONLY = ("alley_dusk", "characters")   # None renders everything

# palette lifted from style test 02
OCHRE = (0.52, 0.40, 0.20)
TERRA = (0.44, 0.24, 0.18)
TEALC = (0.20, 0.36, 0.37)
CREAM = (0.66, 0.61, 0.50)
BLUEC = (0.30, 0.38, 0.52)
BROWN = (0.24, 0.16, 0.11)
OLIVE = (0.30, 0.33, 0.20)
PLUM = (0.30, 0.24, 0.34)
CHARCOAL = (0.09, 0.09, 0.11)
INDIGO = (0.10, 0.14, 0.24)
BLACK = (0.03, 0.025, 0.02)


def dim(c, k):
    return (c[0] * k, c[1] * k, c[2] * k)


_C = {}


def cloth(c):
    """Fabric with the same soft mottling as the walls, so people sit in the world."""
    key = (round(c[0], 3), round(c[1], 3), round(c[2], 3))
    if key not in _C:
        _C[key] = painted("M_Cloth_%02d" % len(_C), dim(c, 0.82), dim(c, 1.12), noise_scale=3.0, rough=0.9,
                          bump=0.25, grime=1.0)
    return _C[key]


# ---- character options -----------------------------------------------------------
CHAR_STYLES = dict(
    A=dict(label="Natural", head=1.0, leg=1.0, torso=1.0, thick=1.0, wide=1.0, eyes="full", eye=0.9, cheeks=False, nose=1.0),
    B=dict(label="Soft", head=1.22, leg=0.95, torso=0.98, thick=1.12, wide=1.04, eyes="full", eye=1.15, cheeks=False, nose=0.85),
    C=dict(label="Simple", head=1.42, leg=0.82, torso=0.95, thick=1.25, wide=1.10, eyes="dot", eye=1.2, cheeks=True, nose=0.5),
)

CAST = dict(
    PlayerA=dict(name="PlayerA", height=1.68, build=0.92, skin=(0.62, 0.42, 0.30), hair="long", hair_color=(0.28, 0.10, 0.05),
                 eyes=(0.14, 0.36, 0.20), garment="tee", top=TEALC, bottom=INDIGO, shoes=CREAM, scarf=OCHRE),
    PlayerB=dict(name="PlayerB", height=1.80, build=1.06, skin=(0.30, 0.19, 0.13), hair="afro", hair_color=BLACK,
                 garment="tunic", top=OCHRE, bottom_type="shorts", bottom=OLIVE, shoes=(0.60, 0.57, 0.50), extras=("bag",), brow=1.3),
    Shopkeeper=dict(name="Shopkeeper", height=1.72, build=1.14, skin=(0.78, 0.56, 0.44), hair="side", hair_color=(0.10, 0.06, 0.04),
                    garment="long", top=CREAM, bottom=BROWN, shoes=(0.10, 0.08, 0.06), extras=("apron", "moustache"),
                    face_w=1.05, nose=1.15),
    Courier=dict(name="Courier", height=1.78, build=1.0, skin=(0.84, 0.64, 0.52), hair="short", hair_color=(0.55, 0.40, 0.18),
                 eyes=(0.18, 0.34, 0.55), garment="tee", top=BLUEC, bottom_type="shorts", bottom=CHARCOAL, shoes=TERRA,
                 extras=("cap", "bag")),
    Elder=dict(name="Elder", height=1.58, build=1.06, skin=(0.70, 0.50, 0.38), hair="bun", hair_color=(0.55, 0.55, 0.56),
               garment="dress", dress_len=0.40, top=OLIVE, coat=PLUM, coat_len=0.62, scarf=CREAM, bottom=OLIVE,
               shoes=BROWN, eye_open=0.85, brow=0.8),
    Office=dict(name="Office", height=1.74, build=0.96, skin=(0.47, 0.31, 0.22), hair="bob", hair_color=BLACK,
                garment="long", top=CREAM, coat=INDIGO, coat_len=0.55, bottom=CHARCOAL, shoes=BLACK, lip=1.3),
)


def character3(sp, cs, ox, oy=0.0):
    """One person. Arms and legs are single tapered forms and clothes are whole garments, so no doll joints show."""
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

    skin = solid(sp["skin"], 0.5)
    top = cloth(sp["top"])
    bot = cloth(sp["bottom"])
    coat = cloth(sp["coat"]) if "coat" in sp else None
    shoe = solid(sp.get("shoes", (0.10, 0.08, 0.07)), 0.6)
    hairc = sp.get("hair_color", (0.08, 0.05, 0.04))
    hair = solid(hairc, 0.6)
    dark = solid((0.03, 0.02, 0.02), 0.25)
    garment = sp.get("garment", "tee")
    kind = "bare" if garment == "dress" else sp.get("bottom_type", "trousers")
    sleeve_long = garment in ("long", "tunic") or coat is not None
    extras = sp.get("extras", ())
    chest = 0.172 * W
    waist = 0.138 * W
    lx = 0.085

    # legs and feet
    for sg in (-1, 1):
        ellipsoid(n + "_Shoe", P(sg * lx, -0.04, 0.0) + Vector((0, 0, 0.045 * T * S)), (0.05 * T * S, 0.12 * T * S, 0.045 * T * S), shoe)
        if kind == "trousers":
            limb(n + "_Leg", P(sg * lx, 0, 0.07), P(sg * lx, 0, 0.90), 0.058 * T * S, 0.088 * T * S, bot)
        else:
            limb(n + "_Leg", P(sg * lx, 0, 0.07), P(sg * lx, 0, 0.90), 0.045 * T * S, 0.08 * T * S, skin)
            if kind == "shorts":
                limb(n + "_Short", P(sg * lx, 0, 0.56), P(sg * lx, 0, 0.92), 0.086 * T * S, 0.092 * T * S, bot)
    if kind != "bare":
        ellipsoid(n + "_Pelvis", P(0, 0, 0.93), (0.16 * W * S, 0.108 * T * S, 0.118 * tork * S), bot)

    # body garment
    if garment in ("tee", "long"):
        hem, r_hem = 1.02, waist
    elif garment == "tunic":
        hem, r_hem = 0.80, 0.172 * W
    else:
        hem = sp.get("dress_len", 0.45)
        r_hem = (0.17 + 0.12 * (1.0 - hem)) * W
    flat = 0.62 * (1 + 0.25 * (T - 1))
    limb(n + "_Body", P(0, 0, hem), P(0, 0, 1.36), r_hem * S, chest * S, top, flat=flat, cap=0.28)
    front = -chest * flat
    outer = coat or top
    if coat is not None:
        ch = sp.get("coat_len", 0.6)
        r_c = max(0.20 * W, r_hem + 0.02) if garment == "dress" else 0.20 * W
        limb(n + "_Coat", P(0, 0.006, ch), P(0, 0.006, 1.37), r_c * S, (chest + 0.014) * S, coat, flat=flat * 1.06, cap=0.25)
        cf = -(chest + 0.014) * flat * 1.06
        limb(n + "_CoatOpen", P(0, -r_c * flat * 1.06 - 0.002, ch + 0.06), P(0, cf - 0.002, 1.36), 0.05 * S, 0.035 * S, top, flat=0.25, cap=0.3)
        ellipsoid(n + "_Collar", P(0, 0.0, 1.425), (0.085 * T * S, 0.08 * T * S, 0.035 * S), coat)
        front = cf

    # arms: shoulder line, then one form to the wrist
    sh = chest + 0.016
    limb(n + "_Yoke", P(-sh, 0, 1.375), P(sh, 0, 1.375), 0.052 * T * S, 0.052 * T * S, outer)
    for sg in (-1, 1):
        shoulder = P(sg * sh, 0, 1.375)
        wrist = P(sg * (sh + 0.055), -0.02, 0.87)
        limb(n + "_Arm", shoulder, wrist, 0.052 * T * S, 0.035 * T * S, outer if sleeve_long else skin)
        if not sleeve_long:
            limb(n + "_Sleeve", shoulder, shoulder.lerp(wrist, 0.42), 0.057 * T * S, 0.052 * T * S, top)
        ellipsoid(n + "_Hand", wrist + Vector((sg * 0.003, -0.008, -0.062 * tork)) * S, (0.03 * T * S, 0.042 * T * S, 0.055 * T * S), skin)
    limb(n + "_Neck", P(0, 0.005, 1.39), P(0, 0, 1.50), 0.046 * T * S, 0.044 * T * S, skin)
    if "scarf" in sp:
        sc = cloth(sp["scarf"])
        ellipsoid(n + "_Scarf", P(0, 0.0, 1.445), (0.10 * T * S, 0.095 * T * S, 0.05 * S), sc)
        limb(n + "_ScarfTail", P(0.045, front - 0.012, 1.14), P(0.04, front - 0.006, 1.42), 0.034 * S, 0.03 * S, sc, flat=0.35, cap=0.4)
    if "apron" in extras:
        limb(n + "_Apron", P(0, front * 0.82, 0.66), P(0, front * 0.86, 1.22), 0.118 * W * S, 0.10 * W * S,
             cloth(sp.get("apron_color", TERRA)), flat=0.26, cap=0.2)
    if "bag" in extras:
        strap = solid((0.13, 0.09, 0.06), 0.7)
        limb(n + "_Strap", P(-0.15, front - 0.006, 1.40), P(0.18, front + 0.006, 1.0), 0.012 * S, 0.012 * S, strap)
        ellipsoid(n + "_Bag", P(0.2, -0.105 * T, 0.93), (0.05 * S, 0.10 * S, 0.09 * S), strap)

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
    white = solid((0.78, 0.76, 0.72), 0.3)
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
        limb(n + "_Brow", F(sg * (ex - 0.02), by, 0.002), F(sg * (ex + 0.025), by + 0.002, 0.002), 0.0044 * bt * K, 0.003 * bt * K, browc)
        if cs["cheeks"]:
            ellipsoid(n + "_Cheek", F(sg * 0.06 * fw, -0.036, -0.002), (0.017 * K, 0.006 * K, 0.011 * K),
                      solid((min(sp["skin"][0] * 1.1, 0.9), sp["skin"][1] * 0.66, sp["skin"][2] * 0.62), 0.6), (0, 0, sg * yaw * 1.6))
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
        capm = cloth(sp.get("cap_color", TERRA))
        hair_cap(n + "_Cap", H(0, 0.006, 0.016), ((rx + 0.03) * K, (ry + 0.032) * K, (rz + 0.028) * K), capm, "dome")
        ellipsoid(n + "_Brim", H(0, -0.135, 0.062), (0.09 * K, 0.075 * K, 0.009 * K), capm, (0.12, 0.0, 0.0))


def populate(cs, places):
    for who, (x, y) in places.items():
        character3(CAST[who], cs, x, y)


# ---- shared scene pieces -----------------------------------------------------------
def mats():
    return dict(
        stone=painted("M_Stone", (0.27, 0.26, 0.30), (0.52, 0.48, 0.43), brick_scale=0.5, bump=0.35, noise_scale=0.9),
        stone_dark=painted("M_StoneDark", (0.18, 0.18, 0.22), (0.37, 0.34, 0.32), brick_scale=0.6, bump=0.35, noise_scale=0.9),
        plaster=painted("M_Plaster", (0.46, 0.43, 0.40), (0.68, 0.63, 0.55), noise_scale=0.7, bump=0.25),
        plaster_b=painted("M_PlasterWarm", (0.50, 0.40, 0.33), (0.70, 0.58, 0.46), noise_scale=0.7, bump=0.25),
        paving=painted("M_Paving", (0.22, 0.21, 0.25), (0.46, 0.42, 0.37), brick_scale=0.6, floor=True, bump=0.5, noise_scale=0.5),
        wood=flat("M_Wood", (0.30, 0.20, 0.13)), wood_red=flat("M_WoodRed", (0.42, 0.18, 0.14)),
        teal=flat("M_Teal", TEALC), ochre=flat("M_Ochre", OCHRE), terra=flat("M_Terracotta", TERRA),
        cream=flat("M_Cream", CREAM), blue=flat("M_ClothBlue", BLUEC),
        glass=flat("M_Glass", (0.07, 0.09, 0.12), rough=0.25, bump=0.0),
        leaf=flat("M_Leaf", (0.24, 0.36, 0.20), noise_scale=4.0), leaf_light=flat("M_LeafLight", (0.40, 0.48, 0.24), noise_scale=4.0),
        iron=flat("M_Iron", (0.10, 0.10, 0.11), rough=0.6),
        lamp=emissive("M_Lamp", (1.0, 0.55, 0.22), 6.0), lit=emissive("M_WinLit", (1.0, 0.58, 0.26), 1.2),
        water=solid((0.06, 0.16, 0.18), 0.08))


def win(name, face, a, z, w, h, m, axis="y", sign=-1, shutter=None, glass=None):
    """Framed window with sill and optional shutters on a wall at x=face or y=face, facing `sign`."""
    def b(nm, a0, a1, d0, d1, z0, z1, mat, bev=0.02):
        lo, hi = min(face + d0 * sign, face + d1 * sign), max(face + d0 * sign, face + d1 * sign)
        if axis == "y":
            box(nm, a0, a1, lo, hi, z0, z1, mat, bevel=bev)
        else:
            box(nm, lo, hi, a0, a1, z0, z1, mat, bevel=bev)
    b(name + "_F", a - w / 2 - 0.15, a + w / 2 + 0.15, -0.12, 0.02, z - 0.15, z + h + 0.15, m["stone_dark"])
    b(name + "_G", a - w / 2, a + w / 2, -0.10, 0.05, z, z + h, glass or m["glass"], 0)
    b(name + "_S", a - w / 2 - 0.25, a + w / 2 + 0.25, -0.10, 0.20, z - 0.3, z - 0.15, m["stone_dark"])
    if shutter is not None:
        b(name + "_Sh", a - w / 2 - 0.65, a - w / 2 - 0.12, -0.06, 0.07, z, z + h, shutter, 0.01)


def lantern(name, x, y, z, m, energy=60.0):
    box(name, x - 0.1, x + 0.1, y - 0.1, y + 0.1, z - 0.15, z + 0.18, m["lamp"], bevel=0.02)
    pl = bpy.data.lights.new(name + "_Light", 'POINT')
    pl.energy = energy
    pl.color = (1.0, 0.6, 0.3)
    setp(pl, "shadow_soft_size", 0.15)
    po = bpy.data.objects.new(name + "_Light", pl)
    po.location = (x, y - 0.25, z - 0.05)
    COL.objects.link(po)
    return po


def stall(name, x, y, clothm, m, produce):
    for (dx, dy, ph) in ((-1.5, -1.0, 2.2), (1.5, -1.0, 2.2), (-1.5, 1.0, 2.75), (1.5, 1.0, 2.75)):
        cyl(name + "_Post", x + dx, y + dy, 0, 0.05, ph, m["wood"], segs=10)
    box(name + "_Cloth", x - 1.75, x + 1.75, y - 1.3, y + 1.3, 2.45, 2.5, clothm, bevel=0, rot=(math.radians(12), 0, 0))
    box(name + "_Table", x - 1.4, x + 1.4, y - 0.75, y + 0.15, 0.8, 0.88, m["wood"], bevel=0.02)
    for px in (-1.3, 1.3):
        box(name + "_Leg", x + px - 0.05, x + px + 0.05, y - 0.7, y + 0.1, 0, 0.8, m["wood"], bevel=0.01)
    for c in range(3):
        cx0 = x - 1.3 + c * 0.9
        box(name + "_Crate", cx0, cx0 + 0.78, y - 0.7, y + 0.08, 0.88, 1.05, m["wood"], bevel=0.02)
        for k in range(6):
            ellipsoid(name + "_Produce", Vector((cx0 + 0.15 + (k % 3) * 0.24, y - 0.5 + (k // 3) * 0.3, 1.1)), (0.11, 0.11, 0.1), produce[c % len(produce)])
    cyl(name + "_Barrel", x + 1.95, y - 0.4, 0, 0.32, 0.85, m["wood"])


def tree(name, x, y, m, h=3.2, s=1.0):
    cyl(name + "_Trunk", x, y, 0, 0.2 * s, h, m["wood"], r_top=0.13 * s)
    for (dx, dy, dz, r, mk) in ((0, 0, h + 1.3, 1.7, "leaf"), (1.1, 0.4, h + 0.7, 1.25, "leaf"), (-1.0, 0.3, h + 0.8, 1.15, "leaf_light"),
                                (0.3, -0.9, h + 0.6, 1.1, "leaf"), (0.2, 0.2, h + 2.4, 1.05, "leaf_light")):
        blob(name + "_Leaf", x + dx * s, y + dy * s, dz, r * s, m[mk])


def haze(density, top=14.0):
    return box("Fog", -40, 40, -26, 72, -0.2, top, fog_material("M_Fog", density, 0.6), bevel=0)


def sun_and_sky(scene, direction, energy, color, sky_color, sky_strength, angle=2.5):
    sun = bpy.data.lights.new("Sun", 'SUN')
    sun.energy = energy
    sun.color = color
    setp(sun, "angle", math.radians(angle))
    so = bpy.data.objects.new("Sun", sun)
    so.rotation_euler = Vector(direction).normalized().to_track_quat('Z', 'Y').to_euler()
    so.location = (10, 6, 22)
    COL.objects.link(so)
    world = bpy.data.worlds.new("World")
    scene.world = world
    try:
        world.use_nodes = True
    except Exception:
        pass
    wt = world.node_tree
    wt.nodes.clear()
    bg = wt.nodes.new("ShaderNodeBackground")
    wo = wt.nodes.new("ShaderNodeOutputWorld")
    bg.inputs[0].default_value = rgba(sky_color)
    bg.inputs[1].default_value = sky_strength
    wt.links.new(bg.outputs[0], wo.inputs["Surface"])


def camera(scene, name, loc, target, lens, ortho=None):
    cd = bpy.data.cameras.new(name)
    cd.lens = lens
    cd.clip_end = 400
    co = bpy.data.objects.new(name, cd)
    co.location = loc
    co.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    COL.objects.link(co)
    scene.camera = co
    return co


def new_scene(name):
    global COL
    scene = bpy.data.scenes.new(name)
    try:
        bpy.context.window.scene = scene
    except Exception as e:
        log("could not switch window scene: %s" % e)
    COL = scene.collection
    return scene


# ---- scene 1 and 2: the alley of style test 02, morning and dusk -----------------------
ALLEY_PEOPLE = dict(PlayerA=(0.9, -1.2), PlayerB=(-0.7, 0.4), Shopkeeper=(-1.9, 2.1), Courier=(0.5, 9.4),
                    Elder=(2.3, 4.6), Office=(1.3, -6.9))


def alley(dusk):
    global COL, SUN_DIR, SUN_ENERGY, SUN_COLOR, SKY_COLOR, SKY_STRENGTH, FOG_DENSITY
    if dusk:
        SUN_DIR, SUN_ENERGY, SUN_COLOR = (0.42, 0.83, 0.36), 2.5, (1.0, 0.42, 0.22)
        SKY_COLOR, SKY_STRENGTH, FOG_DENSITY = (0.06, 0.10, 0.30), 0.6, 0.014
    else:
        SUN_DIR, SUN_ENERGY, SUN_COLOR = (0.446, 0.594, 0.669), 14.0, (1.0, 0.86, 0.68)
        SKY_COLOR, SKY_STRENGTH, FOG_DENSITY = (0.42, 0.55, 0.82), 0.35, 0.022
    random.seed(7)
    scene = build_alley()
    COL = scene.collection
    for ob in list(scene.collection.objects):
        if ob.name.startswith("Fig_"):
            bpy.data.objects.remove(ob, do_unlink=True)
    populate(CHAR_STYLES["B"], ALLEY_PEOPLE)
    if dusk:
        m = dict(lamp=emissive("M_LampDusk", (1.0, 0.55, 0.22), 12.0))
        lit = emissive("M_WinLitDusk", (1.0, 0.58, 0.26), 2.0)
        for ob in list(scene.collection.objects):
            nm = ob.name
            if nm.startswith(("L_WinGlass", "R_WinGlass", "Bridge_Win")) and random.random() < 0.55:
                ob.data.materials.clear()
                ob.data.materials.append(lit)
            elif nm.startswith(("L_Lamp", "F_Lamp")) and ob.type == 'MESH':
                ob.data.materials.clear()
                ob.data.materials.append(m["lamp"])
            elif ob.type == 'LIGHT' and ob.data.type == 'POINT':
                ob.data.energy *= 2.0
        lantern("D_Lamp_Gate", 3.3, 5.0, 3.1, m, 110)
        lantern("D_Lamp_Bridge", -2.6, 13.4, 3.6, m, 110)
        lantern("D_Lamp_Far", 3.2, 20.0, 3.4, m, 110)
        lantern("D_Lamp_Left", -2.7, 8.0, 3.4, m, 100)
        bulb = emissive("M_Bulb", (1.0, 0.70, 0.36), 15.0)
        for i in range(9):
            ellipsoid("D_Bulb", Vector((-2.6 + i * 0.72, 3.0, 5.33)), (0.06, 0.06, 0.075), bulb)
        for bx in (-1.8, 1.2):
            pl = bpy.data.lights.new("D_BulbLight", 'POINT')
            pl.energy = 40
            pl.color = (1.0, 0.68, 0.36)
            po = bpy.data.objects.new("D_BulbLight", pl)
            po.location = (bx, 2.9, 5.2)
            COL.objects.link(po)
    return scene, (0.2 if dusk else 0.3)


# ---- scene 3: a market square in the same style, seen from an arcade ---------------------
SQUARE_PEOPLE = dict(PlayerA=(-0.3, -5.6), PlayerB=(3.2, -5.2), Shopkeeper=(-6.5, 1.8), Courier=(5.0, 0.4),
                     Elder=(-2.6, -1.4), Office=(2.7, -13.0))


def square():
    random.seed(21)
    scene = new_scene("OldTown_Square")
    m = mats()
    box("Ground", -42, 42, -26, 72, -0.3, 0, m["paving"], bevel=0)

    # near arcade: the viewer stands inside it, as in style test 02
    box("N_Slab", -12, 12, -26, -9.2, 5.8, 6.8, m["stone_dark"])
    for y in (-22.0, -19.0, -16.0, -13.0):
        box("N_Beam", -12, 12, y - 0.15, y + 0.15, 5.45, 5.8, m["wood"])
    for i in (-1, 0, 1):
        x0 = i * 6 - 3
        arch_wall("N_Arch%d" % (i + 1), x0, x0 + 6, -10, -9.2, 6.8, x0 + 0.9, x0 + 5.1, 3.1, m["stone"], m["stone_dark"])
    box("N_WallL", -13, -9, -26, -9.2, 0, 6.8, m["stone"])
    box("N_WallR", 9, 13, -26, -9.2, 0, 6.8, m["stone"])
    for lx in (-3.0, 3.0):
        lantern("N_Lamp", lx, -10.15, 3.3, m, 70)
    box("N_Crate", 5.0, 5.9, -13.2, -12.3, 0, 0.8, m["wood"])
    box("N_Crate", 5.2, 5.8, -13.0, -12.4, 0.8, 1.35, m["wood_red"], rot=(0, 0, 0.3))
    cyl("N_Barrel", -5.4, -12.2, 0, 0.34, 0.9, m["wood"])
    box("N_Bench", -8.6, -8.0, -15.5, -13.5, 0.38, 0.48, m["wood"])

    # left: tall sunlit facade
    box("LB_Base", -15, -10, -9.2, 10, 0, 4.6, m["stone"])
    box("LB_Upper", -15, -10.05, -9.2, 10, 4.6, 11, m["plaster"])
    box("LB_Cornice", -10.2, -9.75, -9.2, 10, 4.6, 4.95, m["stone_dark"])
    box("LB_Roof", -15.2, -9.8, -9.4, 10, 11, 11.4, m["stone_dark"])
    for y in (-7, -3, 1, 5, 9):
        box("LB_Pilaster", -10.05, -9.72, y - 0.3, y + 0.3, 0, 4.6, m["stone_dark"])
    for iy, y in enumerate((-5, -1, 3, 7)):
        for iz, z in enumerate((5.7, 8.4)):
            win("LB_W", -10.05, y, z, 1.2, 1.9, m, axis="x", sign=1, shutter=m["teal"] if (iy + iz) % 2 else m["wood_red"])
    box("LB_DoorF", -10.1, -9.9, -1.9, -0.1, 0, 3.4, m["stone_dark"])
    box("LB_Door", -10.05, -9.86, -1.7, -0.3, 0, 3.1, m["wood_red"], bevel=0.02)
    box("LB_Step", -10, -9.3, -2.1, 0.1, 0, 0.18, m["stone_dark"])
    box("LB_ShopG", -10.08, -9.9, 2.0, 4.0, 0.9, 2.6, m["lit"], bevel=0)
    box("LB_ShopF", -10.1, -9.94, 1.85, 4.15, 0.75, 2.75, m["stone_dark"])
    box("LB_Awning", -10.0, -8.5, 1.6, 4.4, 2.95, 3.02, m["terra"], bevel=0, rot=(0, math.radians(18), 0))
    box("LB_LampArm", -10.0, -9.55, 0.57, 0.63, 3.45, 3.5, m["iron"], bevel=0)
    box("LB_Lamp", -9.72, -9.52, 0.5, 0.7, 3.1, 3.45, m["lamp"], bevel=0.02)
    for (cx, cy, s) in ((-9.3, 5.6, 0.8), (-9.2, 6.6, 0.65), (-8.5, 6.1, 0.55)):
        box("LB_Crate", cx - s / 2, cx + s / 2, cy - s / 2, cy + s / 2, 0, s * 0.9, random.choice((m["ochre"], m["teal"], m["wood"])), bevel=0.025,
            rot=(0, 0, random.uniform(-0.4, 0.4)))
    cyl("LB_Pot", -9.4, -2.7, 0, 0.26, 0.5, m["terra"], segs=14, r_top=0.33)
    blob("LB_Plant", -9.4, -2.7, 0.85, 0.42, m["leaf_light"])

    # back row: clock tower and arcade building
    box("T_Body", -10, -6.5, 10, 13.5, 0, 14, m["stone"])
    box("T_Band", -10.15, -6.35, 9.85, 13.65, 14, 14.4, m["stone_dark"])
    box("T_Upper", -9.85, -6.65, 10.15, 13.35, 14.4, 17, m["plaster_b"])
    box("T_Open", -8.9, -7.6, 10.08, 10.2, 15.0, 16.5, m["glass"], bevel=0)
    roof = cyl("T_Roof", -8.25, 11.75, 17, 2.75, 3.2, m["terra"], segs=4, r_top=0.03)
    roof.rotation_euler = (0, 0, math.pi / 4)
    cyl_y("T_ClockRim", -8.25, 9.97, 11.6, 1.15, 0.14, m["stone_dark"], segs=32)
    cyl_y("T_Clock", -8.25, 9.93, 11.6, 1.0, 0.12, m["cream"], segs=32)
    box("T_HandM", -8.29, -8.21, 9.84, 9.87, 11.6, 12.4, m["iron"], bevel=0)
    box("T_HandH", -8.25, -7.72, 9.84, 9.87, 11.56, 11.64, m["iron"], bevel=0)
    for i in range(4):
        x0 = -6.5 + 4 * i
        arch_wall("A_Arch%d" % i, x0, x0 + 4, 10, 10.7, 5.0, x0 + 0.55, x0 + 3.45, 2.7, m["stone"], m["stone_dark"])
        xc = x0 + 2
        if i % 2:
            box("A_Door", xc - 0.6, xc + 0.6, 13.1, 13.22, 0, 2.4, m["wood_red"] if i == 1 else m["wood"], bevel=0.02)
        else:
            box("A_ShopG", xc - 1.0, xc + 1.0, 13.12, 13.22, 0.9, 2.5, m["lit"], bevel=0)
    box("A_Back", -6.5, 9.5, 13.2, 13.8, 0, 5.0, m["stone"])
    box("A_Slab", -6.5, 9.5, 10, 13.8, 5.0, 5.4, m["stone_dark"])
    box("A_Upper", -6.5, 9.5, 10.05, 16, 5.4, 9.0, m["plaster"])
    for kx in range(8):
        win("A_W", 10.05, -5.5 + 2.0 * kx, 6.2, 1.1, 1.8, m, axis="y", sign=-1, shutter=m["teal"] if kx % 3 == 0 else (m["wood_red"] if kx % 3 == 1 else None))
    box("A_Cornice", -6.6, 9.6, 9.85, 10.2, 5.4, 5.7, m["stone_dark"])
    box("A_Roof", -6.7, 9.7, 9.8, 16.2, 9.0, 9.35, m["stone_dark"])
    for px in (-3.5, 2.0, 7.0):
        cyl("A_Pot", px, 12.5, 9.35, 0.25, 0.7, m["terra"], segs=12)
    for lx in (-2.5, 5.5):
        lantern("A_Lamp", lx, 11.9, 3.7, m, 90)

    # right: low wall with trees behind, so the sun streams over it
    box("RW_Wall", 10, 10.7, -9.2, 16, 0, 3.4, m["stone"])
    box("RW_Cap", 9.9, 10.8, -9.2, 16, 3.4, 3.62, m["stone_dark"])
    box("RW_Gate", 9.9, 10.02, 0.0, 1.8, 0, 2.6, m["wood"], bevel=0.02)
    tree("RT1", 13.0, -2.5, m, h=3.6, s=1.15)
    tree("RT2", 13.6, 6.0, m, h=3.8, s=1.2)
    box("Far_R1", 17, 26, -4, 14, 0, 7, m["plaster"])
    box("Far_R2", 18, 28, 18, 30, 0, 10, m["plaster_b"])
    for (fx0, fx1, fy, fh) in ((-30, -19, 26, 14), (-5, 4, 34, 17), (12, 20, 30, 13), (24, 34, 40, 18), (-18, -8, 40, 20)):
        box("Far", fx0, fx1, fy, fy + 7, 0, fh, m["plaster"])

    # the square: fountain, stalls, tree, bench
    cyl("F_Basin", 1.2, -3.0, 0, 1.75, 0.55, m["stone_dark"], segs=28)
    cyl("F_Rim", 1.2, -3.0, 0.55, 1.85, 0.1, m["stone"], segs=28)
    cyl("F_Water", 1.2, -3.0, 0.5, 1.55, 0.12, m["water"], segs=28)
    cyl("F_Column", 1.2, -3.0, 0.5, 0.22, 1.3, m["stone"], segs=14)
    cyl("F_Bowl", 1.2, -3.0, 1.75, 0.6, 0.18, m["stone_dark"], segs=20, r_top=0.85)
    ellipsoid("F_Top", Vector((1.2, -3.0, 2.2)), (0.2, 0.2, 0.3), m["stone"])
    stall("S1", -6.5, 1.0, m["terra"], m, (solid(TERRA, 0.5), solid(OCHRE, 0.5), solid(OLIVE, 0.5)))
    stall("S2", 6.3, 2.6, m["ochre"], m, (solid(CREAM, 0.5), solid(TEALC, 0.5), solid(TERRA, 0.5)))
    cyl("Tree_Ring", 7.0, -5.2, 0, 0.95, 0.4, m["stone_dark"], segs=20)
    tree("Tree", 7.0, -5.2, m)
    box("Bench_Seat", -2.9, -1.2, -7.2, -6.7, 0.42, 0.5, m["wood"], bevel=0.02)
    for px in (-2.75, -1.35):
        box("Bench_Leg", px - 0.06, px + 0.06, -7.15, -6.75, 0, 0.42, m["stone_dark"], bevel=0.01)

    populate(CHAR_STYLES["B"], SQUARE_PEOPLE)
    haze(0.012, top=16.0)
    sun_and_sky(scene, (0.446, 0.594, 0.669), 12.0, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.4)
    camera(scene, "Cam", (0.4, -18.5, 1.7), (0.2, 6.0, 3.6), 24.0)
    return scene, 0.25


# ---- character options, side by side in the same light -----------------------------------
def character_sets():
    random.seed(3)
    scene = new_scene("OldTown_Characters")
    m = mats()
    box("Ground", -8, 52, -10, 4, -0.3, 0, m["paving"], bevel=0)
    box("Wall", -8, 52, 1.3, 1.9, 0, 3.2, m["stone"])
    box("Wall_Upper", -8, 52, 1.36, 1.9, 3.2, 6, m["plaster"])
    box("Wall_Ledge", -8, 52, 1.18, 1.9, 3.2, 3.45, m["stone_dark"])
    cams = []
    for gi, key in enumerate(("A", "B", "C")):
        gx = gi * 20.0
        for ci, who in enumerate(("PlayerA", "PlayerB", "Shopkeeper")):
            sp = dict(CAST[who])
            sp["name"] = who + "_" + key
            character3(sp, CHAR_STYLES[key], gx - 0.85 + ci * 0.85, 0.0)
        lantern("Lamp_%s" % key, gx - 1.7, 1.2, 2.7, m, 45)
        cams.append((key, camera(scene, "Cam_" + key, (gx, -5.4, 1.12), (gx, 0.0, 0.97), 70.0)))
    box("Fog", -8, 52, -9, 2, -0.2, 8, fog_material("M_FogSoft", 0.012, 0.6), bevel=0)
    sun_and_sky(scene, (-0.45, -0.50, 0.74), 3.2, (1.0, 0.82, 0.62), (0.42, 0.55, 0.82), 0.25, angle=6.0)
    return scene, cams


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
        setp(c, "volume_bounces", 1)
    r.resolution_x, r.resolution_y = res
    r.resolution_percentage = 100
    setp(r.image_settings, "media_type", 'IMAGE')
    setp(r.image_settings, "file_format", 'PNG')
    setp(r.image_settings, "color_mode", 'RGB')
    vs = scene.view_settings
    setp(vs, "view_transform", 'AgX')
    setp(vs, "look", 'AgX - Medium High Contrast')
    setp(vs, "exposure", exposure)


def render(scene, key):
    t = time.time()
    scene.render.filepath = OUT + "/" + NAME + "_" + key + ".png"
    bpy.ops.render.render(write_still=True, scene=scene.name)
    log("%s rendered in %.1fs" % (key, time.time() - t))


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(SRC, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    jobs = (("alley_morning", lambda: alley(False)), ("alley_dusk", lambda: alley(True)), ("square_midday", square))
    for key, fn in jobs:
        if ONLY and key not in ONLY:
            continue
        try:
            scene, exposure = fn()
            configure(scene, exposure, (1280, 800), 128)
            log("%s built: %d objects" % (key, len(scene.objects)))
            render(scene, key)
        except Exception:
            log("FAILED %s:\n%s" % (key, traceback.format_exc()))
    if not ONLY or "characters" in ONLY:
        try:
            scene, cams = character_sets()
            configure(scene, -0.3, (900, 900), 128)
            log("characters built: %d objects" % len(scene.objects))
            for key, cam in cams:
                scene.camera = cam
                render(scene, "characters_" + key)
        except Exception:
            log("FAILED characters:\n" + traceback.format_exc())
    try:
        bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
        log("saved .blend")
    except Exception:
        log("save failed:\n" + traceback.format_exc())
    log("DONE")


main()
