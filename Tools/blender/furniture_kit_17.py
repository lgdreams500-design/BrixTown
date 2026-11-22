"""Brix Town - furniture kit 17: four style lines of furniture from one shared kit (FURN-002, FURN-003, FURN-005; look test).

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/furniture_kit_17.py").read(), "furniture_kit_17.py", "exec"))
or headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/furniture_kit_17.py
Quick checks: WASTED_ONLY=room_plain,sheet WASTED_SAMPLES=24 WASTED_SCALE=50

Builds on car_kit_09.py (and through it building_kit_07.py, old_town_04.py and style_test_02.py): the same painted
matte materials, the kit daylight (ADR-0007) and option B people for scale. Interiors are "not started" in
ART_DIRECTION 5; this is the first look test for them, made from the draft rules I1-I9 of docs/art/INTERIOR_STYLE.md.

Every piece is made by one function from the same kit parts (legs, tops, frames, cushions, panels, handles, shades);
a style line only changes the numbers and materials the parts are called with (rule I6):

    Plain      flat slabs, square legs, box cushions, pale wood       (budget flat-pack chain)
    Old Town   turned legs, panelled doors, rolled arms, buttons      (mid-range chain and the design house)
    Workshop   thick dark tops on dull iron frames, crates            (mid-range chain)
    Curve      round corners, round tables, tub chairs, arched heads  (the design house)

Renders go to Art/StyleTests/furniture_kit_17/:
    room_<line>.png      a living room and bed corner in each line, kit daylight through a side window
    room_oldtown_dusk.png the Old Town room at dusk, lamps only (rule I5)
    sheet.png            every piece of every line side by side (rule I7: told apart by outline)
    showroom.png         a corner of a huge outlet: room sets, tags, walking route, boxes on racks (FURN-020, I9)
Not approved: for the owner to choose from.
"""
import bpy, bmesh, math, random, os, time, traceback
from mathutils import Vector

B = "C:/Projects/Wasted/Tools/blender/"
_c = open(B + "car_kit_09.py", encoding="utf-8").read().split("\n# ---- option A: soft box")[0]
exec(compile(_c, "car_kit_09_helpers", "exec"), globals())

NAME = "furniture_kit_17"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"
ONLY = tuple(os.environ["WASTED_ONLY"].split(",")) if os.environ.get("WASTED_ONLY") else None
SAMPLES = int(os.environ.get("WASTED_SAMPLES", 96))
SCALE = int(os.environ.get("WASTED_SCALE", 100))

M = {}


# ---- the kit: parts every piece is made of ---------------------------------------------------------------------
def group(name, x, y, yaw=0.0, parent=None):
    e = bpy.data.objects.new(name, None)
    COL.objects.link(e)
    e.location = (x, y, 0)
    e.rotation_euler = (0, 0, yaw)
    if parent is not None:
        e.parent = parent
    return e


def cylz(P, name, mat, x, y, z0, r, h, r_top=None, segs=28, bev=0.012):
    """An upright round part (round tops, lamp shades, posts)."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segs, radius1=r,
                          radius2=(r if r_top is None else r_top), depth=h)
    ob = add_obj(name, bm, mat, loc=(x, y, z0 + h / 2))
    return put(bevelled(ob, bev, 3), P)


def leg(P, n, L, x, y, h, mat=None):
    """One leg in the line's way (rule I2: nothing under 4 cm across)."""
    mat = mat or L["wood"]
    k = L["leg"]
    if k == "square":
        rbox(P, n, mat, x, y, h / 2, 0.05, 0.05, h, 0.012, 2)
    elif k == "turned":
        rod(P, n, mat, (x, y, 0.0), (x, y, h), 0.026)
        ball(P, n + "_Bead", mat, x, y, h * 0.62, 0.075, 0.075, 0.09)
        ball(P, n + "_Foot", mat, x, y, 0.035, 0.065, 0.065, 0.07)
    elif k == "iron":
        rod(P, n, M["iron"], (x, y, 0.0), (x, y, h), 0.021)
    else:                                                    # "taper": splayed round legs
        sx = 0.04 if x > 0 else -0.04
        sy = 0.03 if y > 0 else -0.03
        rod(P, n, mat, (x + sx, y + sy, 0.0), (x, y, h), 0.025)


def legs4(P, n, L, w, d, h, inset=0.07, mat=None):
    for i, (sx, sy) in enumerate(((-1, -1), (1, -1), (-1, 1), (1, 1))):
        leg(P, "%s_Leg%d" % (n, i), L, sx * (w / 2 - inset), sy * (d / 2 - inset), h, mat)


def top(P, n, L, w, d, z, t=None, mat=None, round_=False):
    """A table or cabinet top, at least 4 cm thick (rule I2)."""
    t = t or L["top"]
    mat = mat or L["wood"]
    if round_:
        return cylz(P, n, mat, 0, 0, z, w / 2, t, bev=0.015)
    return rbox(P, n, mat, 0, 0, z + t / 2, w, d, t, L["r"])


def cushion(P, n, L, mat, cx, cy, cz, sx, sy, sz):
    if L["soft"]:
        return blob(P, n, mat, cx, cy, cz, sx * 1.08, sy * 1.08, sz * 1.25, top=(0.92, 0.92), levels=2)
    return rbox(P, n, mat, cx, cy, cz, sx, sy, sz, L["rc"])


def handle(P, n, L, x, y, z):
    """Knobs and pulls are about 1.3x real so they read at third-person distance (rule I3)."""
    if L["handle"] == "bar":
        rbox(P, n, M["iron"], x, y, z, 0.16, 0.035, 0.035, 0.012, 2)
    else:
        ball(P, n, L["accent_wood"], x, y, z, 0.055, 0.05, 0.055)


def panel_door(P, n, L, x, z0, w, h, front_y, mat):
    """A door or drawer front standing just proud of the carcass; Old Town panels it, Workshop straps it."""
    rbox(P, n, mat, x, front_y - 0.012, z0 + h / 2, w - 0.02, 0.03, h - 0.02, L["r"] * 0.7, 2)
    if L["panels"]:
        rbox(P, n + "_Panel", L["wood_dark"], x, front_y - 0.03, z0 + h / 2, w - 0.16, 0.012, h - 0.18, 0.01, 2)
    if L["straps"]:
        for zz in (z0 + 0.12, z0 + h - 0.12):
            rbox(P, n + "_Strap%d" % int(zz * 100), M["iron"], x, front_y - 0.03, zz, w - 0.06, 0.012, 0.04, 0.006, 1)


def books(P, n, x0, x1, y, z, rng):
    x = x0
    i = 0
    while x < x1 - 0.05:
        w = rng.uniform(0.035, 0.07)
        h = rng.uniform(0.2, 0.3)
        mat = M[rng.choice(("terra", "teal", "cream", "ochre", "dusty", "olive", "plum"))]
        rbox(P, "%s_Book%d" % (n, i), mat, x + w / 2, y, z + h / 2, w, 0.2, h, 0.008, 1)
        x += w + 0.006
        i += 1


# ---- the pieces ------------------------------------------------------------------------------------------------
def sofa(P, L, w=2.0, n="Sofa"):
    d, seat = 0.92, 0.45
    base0 = 0.12 if L["leg"] != "none" else 0.0
    if base0:
        legs4(P, n, L, w - 0.1, d - 0.1, base0 + 0.02, 0.05)
    frame = L["frame"]
    if L["leg"] == "iron":                                   # workshop: an iron frame holds a slab of wood and cushions
        rbox(P, n + "_Frame", M["iron"], 0, 0, base0 + 0.03, w, d, 0.05, 0.015, 2)
        rbox(P, n + "_Deck", L["wood"], 0, 0.02, base0 + 0.11, w - 0.08, d - 0.08, 0.09, L["r"])
    else:
        rbox(P, n + "_Base", frame, 0, 0.02, base0 + 0.14, w, d, 0.26, L["r"])
    nc = 2 if w > 1.2 else 1
    cw = (w - 2 * L["arm"]) / nc
    for i in range(nc):
        cx = -w / 2 + L["arm"] + cw * (i + 0.5)
        cushion(P, "%s_Seat%d" % (n, i), L, L["cloth"], cx, -0.06, seat - 0.05, cw - 0.03, d - 0.28, 0.16)
        cushion(P, "%s_Back%d" % (n, i), L, L["cloth"], cx, d / 2 - 0.16, seat + 0.2, cw - 0.04, 0.2, 0.42)
    rbox(P, n + "_BackFrame", frame, 0, d / 2 - 0.06, base0 + 0.42, w, 0.12, 0.6, L["r"])
    for s in (-1, 1):
        ax = s * (w / 2 - L["arm"] / 2)
        if L["arms"] == "rolled":
            rbox(P, n + "_Arm%d" % s, frame, ax, 0.0, base0 + 0.33, L["arm"], d - 0.02, 0.42, L["r"])
            rod(P, n + "_Roll%d" % s, frame, (ax, -d / 2 + 0.02, base0 + 0.56), (ax, d / 2 - 0.02, base0 + 0.56), L["arm"] * 0.62)
        elif L["arms"] == "tub":
            blob(P, n + "_Arm%d" % s, frame, ax, 0.02, base0 + 0.36, L["arm"] * 1.25, d, 0.62, top=(0.75, 0.92), levels=2)
        elif L["arms"] == "iron":
            rod(P, n + "_ArmA%d" % s, M["iron"], (ax, -d / 2 + 0.05, base0 + 0.05), (ax, -d / 2 + 0.05, base0 + 0.6), 0.021)
            rbox(P, n + "_Arm%d" % s, L["wood"], ax, 0.0, base0 + 0.62, L["arm"], d, 0.06, L["r"])
            rbox(P, n + "_ArmPad%d" % s, L["cloth"], ax, 0.05, base0 + 0.38, L["arm"] - 0.02, d - 0.2, 0.36, L["rc"])
        else:
            rbox(P, n + "_Arm%d" % s, frame, ax, 0.0, base0 + 0.3, L["arm"], d, 0.6, L["r"])
    if L["buttons"]:
        for i in range(nc):
            cx = -w / 2 + L["arm"] + cw * (i + 0.5)
            for bx in (-0.18, 0.18):
                for bz in (0.62, 0.8):
                    ball(P, "%s_Btn%d%d%d" % (n, i, int(bx * 10), int(bz * 10)), L["cloth_dark"], cx + bx, d / 2 - 0.27, bz, 0.04, 0.03, 0.04)
    cushion(P, n + "_Throw", L, L["accent"], -w / 2 + L["arm"] + 0.22, d / 2 - 0.3, seat + 0.2, 0.36, 0.14, 0.34)


def armchair(P, L):
    sofa(P, L, w=0.95, n="Chair")


def coffee_table(P, L):
    h = 0.42
    if L["round"]:
        cylz(P, "Coffee_Ped", L["wood"], 0, 0, 0, 0.16, h - 0.05, r_top=0.12)
        top(P, "Coffee_Top", L, 0.9, 0.9, h - 0.05, round_=True)
    else:
        legs4(P, "Coffee", L, 1.1, 0.6, h - L["top"])
        top(P, "Coffee_Top", L, 1.1, 0.6, h - L["top"])
        if L["leg"] == "iron":
            rbox(P, "Coffee_Shelf", L["wood"], 0, 0, 0.12, 1.0, 0.5, 0.04, 0.01)
    rbox(P, "Coffee_Book", M["terra"], 0.18, 0.05, h + 0.025, 0.26, 0.2, 0.05, 0.01)
    cylz(P, "Coffee_Cup", M["cream"], -0.2, -0.05, h, 0.05, 0.09, bev=0.008)


def dining(P, L):
    h = 0.75
    if L["round"]:
        cylz(P, "Dine_Ped", L["wood"], 0, 0, 0, 0.12, h - 0.05, r_top=0.07)
        cylz(P, "Dine_Foot", L["wood"], 0, 0, 0, 0.36, 0.06, bev=0.02)
        top(P, "Dine_Top", L, 1.1, 1.1, h - 0.05, round_=True)
    else:
        legs4(P, "Dine", L, 1.4, 0.8, h - L["top"], 0.08)
        top(P, "Dine_Top", L, 1.4, 0.8, h - L["top"])
    for i, (x, y, yaw) in enumerate(((0, -0.72, 0.0), (0, 0.72, math.pi))):
        C = group("DineChair%d" % i, x, y, yaw, P)
        chair(C, L, "DC%d" % i)


def chair(P, L, n="Ch"):
    s, seat = 0.46, 0.46
    if L["round"]:
        legs4(P, n, L, s, s, seat - 0.05, 0.06)
        blob(P, n + "_Seat", L["cloth"], 0, 0, seat, s + 0.04, s + 0.04, 0.12, levels=2)
        blob(P, n + "_Back", L["wood"], 0, s / 2 - 0.02, seat + 0.3, s + 0.06, 0.08, 0.36, top=(0.85, 1), levels=2)
        return
    legs4(P, n, L, s, s, seat - 0.05, 0.04)
    rbox(P, n + "_Seat", L["wood"], 0, 0, seat - 0.025, s, s, 0.05, L["r"])
    for sx in (-1, 1):
        if L["leg"] == "iron":
            rod(P, n + "_Post%d" % sx, M["iron"], (sx * (s / 2 - 0.04), s / 2 - 0.04, seat), (sx * (s / 2 - 0.04), s / 2 - 0.02, seat + 0.42), 0.021)
        else:
            rbox(P, n + "_Post%d" % sx, L["wood"], sx * (s / 2 - 0.04), s / 2 - 0.04, seat + 0.21, 0.05, 0.05, 0.42, 0.012, 2)
    rbox(P, n + "_Rail", L["wood"], 0, s / 2 - 0.04, seat + 0.36, s, 0.05, 0.14 if L["panels"] else 0.1, L["r"])
    if L["buttons"]:
        cushion(P, n + "_Pad", L, L["cloth"], 0, -0.01, seat + 0.03, s - 0.06, s - 0.08, 0.06)


def bed(P, L):
    w, d = 1.6, 2.1
    h0 = 0.18
    if L["leg"] == "iron":
        rbox(P, "Bed_Frame", M["iron"], 0, 0, h0 + 0.02, w, d, 0.05, 0.015, 2)
        legs4(P, "Bed", L, w, d, h0, 0.04)
    else:
        legs4(P, "Bed", L, w, d, h0, 0.06)
        rbox(P, "Bed_Frame", L["wood"], 0, 0, h0 + 0.08, w, d, 0.16, L["r"])
    rbox(P, "Bed_Mattress", M["cream"], 0, 0, h0 + 0.24, w - 0.06, d - 0.06, 0.2, 0.06)
    cushion(P, "Bed_Duvet", L, L["accent"], 0, -0.22, h0 + 0.36, w - 0.02, d - 0.5, 0.1)
    for s in (-1, 1):
        cushion(P, "Bed_Pillow%d" % s, L, M["cream"], s * 0.36, d / 2 - 0.3, h0 + 0.4, 0.6, 0.34, 0.14)
    hy = d / 2 + 0.03
    k = L["head"]
    if k == "arch":
        blob(P, "Bed_Head", L["cloth"], 0, hy, 0.75, w + 0.06, 0.12, 1.1, top=(0.7, 1.0), levels=3)
    elif k == "panel":
        for s in (-1, 1):
            rod(P, "Bed_HPost%d" % s, L["wood"], (s * (w / 2), hy, 0), (s * (w / 2), hy, 1.15), 0.035)
            ball(P, "Bed_HFinial%d" % s, L["wood"], s * (w / 2), hy, 1.18, 0.1, 0.1, 0.1)
        rbox(P, "Bed_Head", L["wood"], 0, hy, 0.8, w - 0.04, 0.06, 0.6, L["r"])
        rbox(P, "Bed_HeadPanel", L["wood_dark"], 0, hy - 0.035, 0.8, w - 0.3, 0.012, 0.42, 0.01, 2)
    elif k == "rails":
        for s in (-1, 1):
            rod(P, "Bed_HPost%d" % s, M["iron"], (s * (w / 2), hy, 0), (s * (w / 2), hy, 1.05), 0.024)
        for z in (0.62, 1.0):
            rod(P, "Bed_HRail%d" % int(z * 10), M["iron"], (-w / 2, hy, z), (w / 2, hy, z), 0.022)
        for i in range(6):
            x = -w / 2 + w * (i + 0.5) / 6
            rod(P, "Bed_HBar%d" % i, M["iron"], (x, hy, 0.62), (x, hy, 1.0), 0.016)
    else:
        rbox(P, "Bed_Head", L["wood"], 0, hy, 0.62, w + 0.04, 0.07, 0.9, L["r"])


def bedside(P, L):
    w, d, h = 0.48, 0.42, 0.55
    legs4(P, "Side", L, w, d, 0.12, 0.04)
    rbox(P, "Side_Body", L["wood"], 0, 0, 0.12 + (h - 0.12) / 2, w, d, h - 0.12, L["r"])
    panel_door(P, "Side_Drawer", L, 0, 0.3, w - 0.04, 0.2, -d / 2, L["wood"])
    handle(P, "Side_Knob", L, 0, -d / 2 - 0.05, 0.4)
    lamp_small(P, L, 0.05, 0.02, h)


def lamp_small(P, L, x, y, z):
    cylz(P, "Lamp_Base", L["accent_wood"], x, y, z, 0.08, 0.2, r_top=0.05)
    cylz(P, "Lamp_Shade", M["shade"], x, y, z + 0.2, 0.16, 0.2, r_top=0.1, bev=0.01)
    light(P, "Lamp_Light", x, y, z + 0.27, 35.0)


def light(P, name, x, y, z, energy):
    pl = bpy.data.lights.new(name, 'POINT')
    pl.energy = energy
    pl.color = (1.0, 0.6, 0.3)
    setp(pl, "shadow_soft_size", 0.08)
    po = bpy.data.objects.new(name, pl)
    po.location = (x, y, z)
    COL.objects.link(po)
    po.parent = P


def floor_lamp(P, L):
    if L["leg"] == "iron":
        cylz(P, "FLamp_Base", M["iron"], 0, 0, 0, 0.17, 0.04)
        rod(P, "FLamp_Pole", M["iron"], (0, 0, 0.04), (0, 0, 1.45), 0.02)
    else:
        cylz(P, "FLamp_Base", L["wood"], 0, 0, 0, 0.18, 0.06, r_top=0.15)
        rod(P, "FLamp_Pole", L["wood"], (0, 0, 0.05), (0, 0, 1.45), 0.024)
    if L["round"]:
        ball(P, "FLamp_Shade", M["shade"], 0, 0, 1.55, 0.44, 0.44, 0.34)
    else:
        cylz(P, "FLamp_Shade", M["shade"], 0, 0, 1.38, 0.24, 0.3, r_top=0.15, bev=0.01)
    light(P, "FLamp_Light", 0, 0, 1.5, 80.0)


def wardrobe(P, L):
    w, d, h = 1.1, 0.6, 2.0
    plinth = 0.12
    if L["leg"] in ("turned", "taper"):
        legs4(P, "Ward", L, w, d, plinth + 0.02, 0.06)
    else:
        rbox(P, "Ward_Plinth", L["wood_dark"], 0, 0.02, plinth / 2, w - 0.04, d - 0.04, plinth, 0.02)
    if L["round"]:
        blob(P, "Ward_Body", L["wood"], 0, 0, plinth + (h - plinth) / 2, w + 0.06, d + 0.04, h - plinth + 0.06, top=(0.82, 1.0), levels=2)
    else:
        rbox(P, "Ward_Body", L["wood"], 0, 0, plinth + (h - plinth) / 2, w, d, h - plinth, L["r"])
    if L["round"]:                                       # Curve: the doors are the rounded body itself, split by a groove
        rbox(P, "Ward_Groove", L["wood_dark"], 0, -d / 2 + 0.02, plinth + (h - plinth) / 2, 0.018, 0.1, h - plinth - 0.35, 0.006, 1)
        for s in (-1, 1):
            handle(P, "Ward_Knob%d" % s, L, s * 0.07, -d / 2 - 0.01, plinth + 0.95)
    else:
        for s in (-1, 1):
            panel_door(P, "Ward_Door%d" % s, L, s * w / 4, plinth + 0.04, w / 2 - 0.02, h - plinth - 0.12, -d / 2, L["wood"])
            handle(P, "Ward_Knob%d" % s, L, s * 0.07, -d / 2 - 0.05, plinth + 0.95)
    if L["panels"]:
        rbox(P, "Ward_Cornice", L["wood"], 0, 0, h + 0.04, w + 0.1, d + 0.08, 0.08, 0.02)


def bookcase(P, L, rng):
    w, d, h = 0.9, 0.34, 1.8
    side = M["iron"] if L["leg"] == "iron" else L["wood"]
    for s in (-1, 1):
        if L["leg"] == "iron":
            rod(P, "Case_SideF%d" % s, side, (s * w / 2, -d / 2 + 0.02, 0), (s * w / 2, -d / 2 + 0.02, h), 0.021)
            rod(P, "Case_SideB%d" % s, side, (s * w / 2, d / 2 - 0.02, 0), (s * w / 2, d / 2 - 0.02, h), 0.021)
        else:
            rbox(P, "Case_Side%d" % s, side, s * (w / 2 - 0.025), 0, h / 2, 0.05, d, h, L["r"])
    if L["leg"] != "iron":
        rbox(P, "Case_Back", L["wood_dark"], 0, d / 2 - 0.01, h / 2, w - 0.06, 0.02, h - 0.02, 0.005, 1)
    for i, z in enumerate((0.06, 0.5, 0.94, 1.38, h - 0.03)):
        rbox(P, "Case_Shelf%d" % i, L["wood"], 0, 0, z, w - 0.02, d - 0.02, 0.045, L["r"] * 0.6, 2)
        if z < h - 0.1:
            books(P, "Case_S%d" % i, -w / 2 + 0.07, w / 2 - 0.07 - (0.25 if i % 2 else 0), 0.0, z + 0.023, rng)


def rug(P, L):
    if L["round"]:
        cylz(P, "Rug", L["rug"], 0, 0, 0, 1.0, 0.02, segs=48, bev=0.006)
    else:
        rbox(P, "Rug", L["rug"], 0, 0, 0.01, 2.0, 1.4, 0.02, 0.008, 1)


def crate(P, L):
    """Workshop's extra; every line has one small 'character' piece (a crate, a pouffe, a stool, a trunk)."""
    if L["key"] == "workshop":
        rbox(P, "Crate", L["wood"], 0, 0, 0.22, 0.6, 0.45, 0.44, 0.015)
        for z in (0.1, 0.34):
            rbox(P, "Crate_Slat%d" % int(z * 100), L["wood_dark"], 0, -0.228, z, 0.58, 0.012, 0.07, 0.006, 1)
    elif L["key"] == "curve":
        blob(P, "Pouffe", L["accent"], 0, 0, 0.2, 0.6, 0.6, 0.42, top=(0.9, 0.9), levels=3)
    elif L["key"] == "oldtown":
        rbox(P, "Trunk", L["wood"], 0, 0, 0.24, 0.8, 0.45, 0.44, L["r"])
        rod(P, "Trunk_Lid", L["wood"], (-0.4, 0, 0.46), (0.4, 0, 0.46), 0.2)
        for x in (-0.25, 0.25):
            rbox(P, "Trunk_Band%d" % int(x * 10), M["iron"], x, 0, 0.36, 0.05, 0.47, 0.62, 0.01, 1)
    else:
        legs4(P, "Stool", L, 0.36, 0.36, 0.4, 0.05)
        rbox(P, "Stool_Top", L["wood"], 0, 0, 0.42, 0.38, 0.38, 0.05, L["r"])


PIECES = ("sofa", "armchair", "coffee", "dining", "bed", "bedside", "wardrobe", "bookcase", "floor_lamp", "rug", "extra")


def make(kind, P, L, rng):
    f = dict(sofa=sofa, armchair=armchair, coffee=coffee_table, dining=dining, bed=bed, bedside=bedside,
             wardrobe=wardrobe, floor_lamp=floor_lamp, rug=rug, extra=crate)
    if kind == "bookcase":
        return bookcase(P, L, rng)
    return f[kind](P, L)


# ---- the lines --------------------------------------------------------------------------------------------------
def lines():
    pale = painted("M_WoodPale", (0.50, 0.40, 0.25), (0.68, 0.60, 0.44), noise_scale=3.0, bump=0.15)
    mid = painted("M_WoodMid", (0.24, 0.15, 0.09), (0.40, 0.26, 0.15), noise_scale=3.0, bump=0.15)
    dark = painted("M_WoodDark", (0.10, 0.08, 0.07), (0.24, 0.16, 0.11), noise_scale=3.0, bump=0.15)
    honey = painted("M_WoodHoney", (0.42, 0.30, 0.15), (0.60, 0.46, 0.26), noise_scale=3.0, bump=0.15)
    cl = lambda nm, c: flat("M_Cloth" + nm, c, bump=0.3, noise_scale=6.0)
    return [
        dict(key="plain", label="Plain", wood=pale, wood_dark=honey, accent_wood=pale, frame=cl("PlainTeal", (0.20, 0.36, 0.37)),
             cloth=cl("PlainTeal2", (0.22, 0.38, 0.39)), cloth_dark=M["teal"], accent=M["terra"], rug=cl("PlainRug", (0.62, 0.57, 0.47)),
             leg="square", top=0.045, r=0.012, rc=0.03, soft=False, arm=0.16, arms="slab", buttons=False, panels=False, straps=False,
             round=False, head="slab", handle="bar"),
        dict(key="oldtown", label="Old Town", wood=mid, wood_dark=dark, accent_wood=honey, frame=cl("OldTerra", (0.44, 0.24, 0.18)),
             cloth=cl("OldTerra2", (0.47, 0.26, 0.19)), cloth_dark=cl("OldTerraD", (0.30, 0.15, 0.11)), accent=cl("OldOlive", (0.30, 0.33, 0.20)),
             rug=cl("OldRug", (0.30, 0.24, 0.34)), leg="turned", top=0.055, r=0.018, rc=0.06, soft=False, arm=0.2, arms="rolled",
             buttons=True, panels=True, straps=False, round=False, head="panel", handle="knob"),
        dict(key="workshop", label="Workshop", wood=dark, wood_dark=mid, accent_wood=mid, frame=M["iron"],
             cloth=cl("WorkCanvas", (0.62, 0.57, 0.46)), cloth_dark=M["cream"], accent=cl("WorkIndigo", (0.10, 0.14, 0.24)),
             rug=cl("WorkRug", (0.44, 0.24, 0.18)), leg="iron", top=0.07, r=0.012, rc=0.04, soft=False, arm=0.14, arms="iron",
             buttons=False, panels=False, straps=True, round=False, head="rails", handle="bar"),
        dict(key="curve", label="Curve", wood=honey, wood_dark=pale, accent_wood=honey, frame=cl("CurveDusty", (0.30, 0.38, 0.52)),
             cloth=cl("CurveDusty2", (0.32, 0.40, 0.54)), cloth_dark=M["dusty"], accent=cl("CurveOchre", (0.52, 0.40, 0.20)),
             rug=cl("CurveRug", (0.20, 0.36, 0.37)), leg="taper", top=0.05, r=0.05, rc=0.1, soft=True, arm=0.2, arms="tub",
             buttons=False, panels=False, straps=False, round=True, head="arch", handle="knob"),
    ]


# ---- rooms ------------------------------------------------------------------------------------------------------
def room_shell(n, ox, oy, w=6.4, d=4.4, h=3.0, ceiling=True, window=True, front=0.0):
    """Three plaster walls on a stone footing, board floor, a deep window in the right wall facing the sun."""
    x0, x1, y0, y1 = ox - w / 2, ox + w / 2, oy - d / 2, oy + d / 2
    y0 -= front                                          # a room shot: the room runs on behind the camera
    box(n + "_Floor", x0, x1, y0, y1, -0.1 if ceiling else 0.0, 0.0 if ceiling else 0.025, M["boards"], bevel=0)   # a set stands on the hall floor, not in it
    box(n + "_Back", x0 - 0.3, x1 + 0.3, y1, y1 + 0.3, 0, h, M["plaster"], bevel=0)
    box(n + "_BackSkirt", x0, x1, y1 - 0.03, y1, 0, 0.14, M["wood"], bevel=0.005)
    box(n + "_Left", x0 - 0.3, x0, y0, y1, 0, h, M["plaster_b"], bevel=0)
    if window:
        wy0, wy1, wz0, wz1 = oy - 0.2, oy + 1.2, 0.9, 2.35
        box(n + "_RightA", x1, x1 + 0.45, y0, wy0, 0, h, M["plaster"], bevel=0)
        box(n + "_RightB", x1, x1 + 0.45, wy1, y1, 0, h, M["plaster"], bevel=0)
        box(n + "_RightC", x1, x1 + 0.45, wy0, wy1, 0, wz0, M["plaster"], bevel=0)
        box(n + "_RightD", x1, x1 + 0.45, wy0, wy1, wz1, h, M["plaster"], bevel=0)
        box(n + "_Sill", x1 - 0.08, x1 + 0.5, wy0 - 0.08, wy1 + 0.08, wz0 - 0.06, wz0, M["stone_dark"], bevel=0.015)
        for s, yy in enumerate((wy0 - 0.5, wy1 + 0.02)):
            box(n + "_Shutter%d" % s, x1 + 0.45, x1 + 0.5, yy, yy + 0.48, wz0, wz1, M["teal"], bevel=0.01)
    else:
        box(n + "_Right", x1, x1 + 0.3, y0, y1, 0, h, M["plaster"], bevel=0)
    if ceiling:
        box(n + "_Ceiling", x0 - 0.3, x1 + 0.45, y0, y1 + 0.3, h, h + 0.2, M["plaster"], bevel=0)
        for i in range(5):
            bx = x0 + 0.5 + i * (w - 1.0) / 4
            box(n + "_Beam%d" % i, bx - 0.09, bx + 0.09, y0, y1, h - 0.18, h, M["wood"], bevel=0.015)
    return x0, x1, y0, y1


def furnish(n, L, ox, oy, rng):
    """A living room with a bed corner; positions are the same in every line so only the style changes."""
    place = (("rug", -0.9, 0.5, 0.0), ("sofa", -0.9, 1.65, 0.0), ("armchair", 0.55, 0.45, -math.pi / 2 + 0.35),
             ("coffee", -0.9, 0.5, 0.0), ("floor_lamp", -2.4, 1.85, 0.0), ("bookcase", -2.95, -0.2, math.pi / 2),
             ("bed", 2.05, 1.1, 0.0), ("bedside", 0.95, 1.95, 0.0), ("wardrobe", 2.85, -1.3, -math.pi / 2),
             ("extra", 2.05, -0.4, 0.0))
    for kind, x, y, yaw in place:
        P = group("%s_%s" % (n, kind), ox + x, oy + y, yaw)
        make(kind, P, L, rng)


def room_scene(L, dusk=False):
    rng = random.Random(17)
    room_shell("Room", 0, 0, front=2.6)
    furnish("Room", L, 0, 0, rng)
    people([("PlayerA", -1.9, -0.9)])
    if dusk:
        sun_and_sky(SCENE, (0.42, 0.83, 0.36), 2.5, (1.0, 0.42, 0.22), (0.06, 0.10, 0.30), 0.6)
    else:
        sun_and_sky(SCENE, (0.78, 0.30, 0.42), 8.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.25)
    return [("cam", (0.15, -4.4, 1.7), (0.1, 0.8, 1.0), 17.0)]


def sheet_scene(Ls):
    rng = random.Random(17)
    gap_x, gap_y = 2.7, 3.6
    nx = len(PIECES)
    box("Sheet_Floor", -5.5, nx * gap_x + 1, -3, len(Ls) * gap_y + 2, -0.1, 0, M["plaster"], bevel=0)
    for r, L in enumerate(Ls):
        y = (len(Ls) - 1 - r) * gap_y
        for c, kind in enumerate(PIECES):
            P = group("S_%s_%s" % (L["key"], kind), c * gap_x, y, 0.35 if kind not in ("rug",) else 0.0)
            make(kind, P, L, rng)
        text("S_Label_" + L["key"], L["label"], -3.4, y, 0.02, 0.55, "iron").rotation_euler = (0, 0, 0)
    people([("PlayerA", nx * gap_x - 0.4, -1.0)])
    sun_and_sky(SCENE, (0.55, -0.55, 0.62), 6.0, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.6)
    cx = (nx - 1) * gap_x / 2
    cy = (len(Ls) - 1) * gap_y / 2
    return [("cam", (cx - 1.8, cy - 24.0, 18.5), (cx - 1.8, cy + 0.2, 0.3), 27.0)]


def showroom_scene(Ls):
    """A corner of a huge outlet: a long hall under timber trusses and skylights, room sets of three walls along a
    painted walking route, tags on stands, boxes on tall racks at the back (FURN-020, INTERIOR_STYLE 4, I9)."""
    rng = random.Random(17)
    W, D, H = 34.0, 22.0, 8.0
    box("Hall_Floor", -W / 2, W / 2, -D / 2, D / 2, -0.1, 0, M["concrete"], bevel=0)
    box("Hall_Back", -W / 2, W / 2, D / 2, D / 2 + 0.5, 0, H, M["plaster"], bevel=0)
    box("Hall_Left", -W / 2 - 0.5, -W / 2, -D / 2, D / 2, 0, H, M["stone"], bevel=0)
    box("Hall_Right", W / 2, W / 2 + 0.5, -D / 2, D / 2, 0, H, M["stone"], bevel=0)
    for i in range(7):                                    # roof: trusses with skylights between them
        x = -W / 2 + 2 + i * (W - 4) / 6
        box("Hall_Truss%d" % i, x - 0.15, x + 0.15, -D / 2, D / 2, H - 0.4, H, M["wood"], bevel=0.02)
        rod(None, "Hall_Tie%d" % i, M["iron"], (x, -D / 2, H - 1.6), (x, D / 2, H - 1.6), 0.04)
        lantern("Hall_Lantern%d" % i, x, 0.0, H - 2.4, M, 220)
        lantern("Hall_LanternB%d" % i, x, 6.0, H - 2.4, M, 220)
    for j in range(6):
        x0 = -W / 2 + 2.3 + j * (W - 4) / 6
        x1 = x0 + (W - 4) / 6 - 0.6
        box("Hall_RoofA%d" % j, x0 - 0.3, x1 + 0.3, -D / 2, -2.0, H, H + 0.3, M["plaster"], bevel=0)
        box("Hall_RoofB%d" % j, x0 - 0.3, x1 + 0.3, 2.5, D / 2 + 0.5, H, H + 0.3, M["plaster"], bevel=0)
    box("Hall_RoofEnd", -W / 2 - 0.5, -W / 2 + 2.2, -D / 2, D / 2, H, H + 0.3, M["plaster"], bevel=0)
    box("Hall_RoofEnd2", W / 2 - 1.8, W / 2 + 0.5, -D / 2, D / 2, H, H + 0.3, M["plaster"], bevel=0)
    # the walking route: a painted band with arrows
    box("Route", -W / 2, W / 2, -4.6, -3.4, 0.0, 0.006, M["route"], bevel=0)
    for i in range(11):
        x = -W / 2 + 2 + i * 3
        bm = bmesh.new()
        v = [bm.verts.new(p) for p in ((x, -4.3, 0.008), (x, -3.7, 0.008), (x + 0.6, -4.0, 0.008))]
        bm.faces.new(v)
        add_obj("Arrow%d" % i, bm, M["cream"])
    # room sets of three walls, one per line, with tags
    for k, L in enumerate(Ls):
        ox = -12.0 + k * 8.0
        oy = 0.6
        room_shell("Set%d" % k, ox, oy, w=6.4, d=4.4, h=3.0, ceiling=False, window=False)
        furnish("Set%d" % k, L, ox, oy, rng)
        for t, (tx, ty) in enumerate(((-1.2, -1.6), (1.6, -1.6))):
            P = group("Tag%d%d" % (k, t), ox + tx, oy + ty)
            rod(P, "Tag%d%d_Post" % (k, t), M["iron"], (0, 0, 0), (0, 0, 1.0), 0.018)
            rbox(P, "Tag%d%d_Card" % (k, t), M["cream"], 0, -0.02, 1.08, 0.3, 0.03, 0.2, 0.01, 1)
            rbox(P, "Tag%d%d_Band" % (k, t), L["cloth"], 0, -0.04, 1.14, 0.3, 0.012, 0.05, 0.004, 1)
        sign = text("SetSign%d" % k, L["label"], ox, oy + 2.2 - 0.02, 3.45, 0.5, "letters")
        sign.rotation_euler = (math.pi / 2, 0, 0)
    # the warehouse at the back: tall racks of flat-pack boxes
    for r in range(5):
        x0 = -15 + r * 6.5
        for s in (-1, 1):
            rbox(None, "Rack%d%d" % (r, s), M["wood"], x0 + s * 2.6, 9.0, 2.6, 0.12, 1.4, 5.2, 0.02)
        for zi, z in enumerate((0.1, 1.8, 3.5)):
            rbox(None, "RackShelf%d%d" % (r, zi), M["wood"], x0, 9.0, z, 5.3, 1.4, 0.08, 0.01)
            bx = x0 - 2.4
            while bx < x0 + 2.2:
                bw = rng.uniform(0.5, 1.1)
                bh = rng.uniform(0.3, 1.2)
                rbox(None, "Box%d%d_%d" % (r, zi, int(bx * 10)), M["cardboard"], bx + bw / 2, 9.0, z + 0.04 + bh / 2, bw, 1.1, bh, 0.015, 1)
                rbox(None, "BoxBand%d%d_%d" % (r, zi, int(bx * 10)), M["teal"], bx + bw / 2, 9.0 - 0.556, z + 0.04 + bh * 0.7, bw, 0.01, 0.08, 0.003, 1)
                bx += bw + 0.06
    big = text("HallSign", "FURNITURE", 0, D / 2 - 0.02, 6.3, 1.2, "letters")
    big.rotation_euler = (math.pi / 2, 0, 0)
    people([("PlayerA", -4.5, -4.0), ("PlayerB", -3.0, -3.7), ("Elder", 3.8, -2.4), ("Shopkeeper", 7.0, -4.2), ("Office", 11.0, -3.9)])
    sun_and_sky(SCENE, (0.35, -0.30, 0.89), 7.0, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.45)
    return [("cam", (-5.5, -13.5, 3.8), (-1.6, 1.2, 1.5), 19.0)]


def people(where):
    cs = CHAR_STYLES["B"]
    for i, (who, x, y) in enumerate(where):
        sp = dict(CAST[who])
        sp["name"] = "%s_%d" % (who, i)
        character3(sp, cs, x, y)


# ---- main -----------------------------------------------------------------------------------------------------------
SCENE = None


def fresh(name):
    global SCENE, K_M, M
    for s in list(bpy.data.scenes):
        if s.name.startswith("Furn"):
            try:
                bpy.data.scenes.remove(s)
            except Exception:
                pass
    SCENE = new_scene(name)
    K_M = kit_mats()
    M = dict(K_M)
    M.update(boards=painted("M_Boards", (0.26, 0.17, 0.10), (0.42, 0.29, 0.17), brick_scale=0.25, floor=True, bump=0.3, noise_scale=1.6),
             concrete=painted("M_HallFloor", (0.40, 0.37, 0.33), (0.56, 0.51, 0.44), brick_scale=0.08, floor=True, bump=0.2, noise_scale=0.6),
             shade=emissive("M_Shade", (1.0, 0.72, 0.45), 1.6), route=flat("M_Route", (0.44, 0.24, 0.18)),
             cardboard=painted("M_Card", (0.50, 0.40, 0.25), (0.66, 0.58, 0.44), noise_scale=2.0, bump=0.15),
             dusty=flat("M_FurnDusty", (0.30, 0.38, 0.52)), olive=flat("M_FurnOlive", (0.30, 0.33, 0.20)),
             plum=flat("M_FurnPlum", (0.30, 0.24, 0.34)))
    return SCENE


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
        setp(c, "max_bounces", 8)
        setp(c, "diffuse_bounces", 5)
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


def render(scene, key, cams, exposure, res):
    for i, (nm, loc, tgt, lens) in enumerate(cams):
        scene.camera = camera(scene, "Cam_%s_%d" % (key, i), loc, tgt, lens)
        configure(scene, exposure, res, SAMPLES)
        t = time.time()
        scene.render.filepath = OUT_DIR + "/" + key + ".png"
        bpy.ops.render.render(write_still=True, scene=scene.name)
        log("%s rendered in %.1fs (%d objects)" % (key, time.time() - t, len(scene.objects)))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(SRC, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    jobs = []
    for li in range(4):
        jobs.append(("room_%s" % ("plain", "oldtown", "workshop", "curve")[li], li, False))
    jobs += [("room_oldtown_dusk", 1, True), ("sheet", None, False), ("showroom", None, False)]
    for key, li, dusk in jobs:
        if ONLY and key not in ONLY:
            continue
        try:
            scene = fresh("Furn_" + key)
            Ls = lines()
            if key == "sheet":
                cams = sheet_scene(Ls)
                render(scene, key, cams, -0.2, (2000, 1100))
            elif key == "showroom":
                cams = showroom_scene(Ls)
                render(scene, key, cams, 0.5, (1800, 1000))
            else:
                cams = room_scene(Ls[li], dusk)
                render(scene, key, cams, 1.4 if dusk else 1.3, (1600, 1000))
            if key == "room_oldtown":
                try:
                    bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
                    log("saved .blend")
                except Exception:
                    log("save failed:\n" + traceback.format_exc())
        except Exception:
            log("FAILED %s:\n%s" % (key, traceback.format_exc()))
    log("DONE")


main()
