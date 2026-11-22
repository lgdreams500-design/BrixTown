"""Brix Town - furniture kit 18: twelve more kits of furniture and decoration in the four style lines (FURN-001,
FURN-002, FURN-003, FURN-005; look test).

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/furniture_kit_18.py").read(), "furniture_kit_18.py", "exec"))
or headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/furniture_kit_18.py
Quick checks: WASTED_ONLY=sheet_kitchen,room_kitchen WASTED_SAMPLES=24 WASTED_SCALE=50
From another checkout: WASTED_ROOT=C:/Projects/Wasted-morefurniture (where kit 17 is read from and renders go).

Builds on furniture_kit_17.py (and through it car_kit_09.py, building_kit_07.py, old_town_04.py and style_test_02.py):
the same kit parts (legs, tops, cushions, panel doors, handles, shades), the same four lines (ADR-0026) and the same
rooms, light and materials. Kit 17 made the living room and bedroom; this adds a kit for every other room of FURN-001
and one of decoration, each piece made in all four lines by one function (rule I6):

    kitchen    counter run, wall cupboards, cooker, sink, fridge, pot shelf
    bathroom   bath, washbasin, toilet, towel rail, mirror cabinet, laundry basket
    dining     sideboard, plate dresser, bench, bar stool, display cabinet, pendant lamp
    study      desk, desk chair, filing cabinet, desk lamp, pinboard, shelving
    children   bunk bed, cot, toy chest, small table and stools, rocking horse, toy shelf
    hallway    coat stand, shoe bench, console table, umbrella stand, coat hooks, hall mirror
    garden     garden bench, bistro set, planter, parasol, lounger, plant stand
    shop       shop counter and till, gondola shelves, clothes rail, display table, A-board, dress form
    cafe       café counter, coffee machine, banquette, café table, menu board, cake stand
    bedroom    chest of drawers, dressing table, single bed, blanket box, folding screen, cheval mirror
    living     fireplace, upright piano, record cabinet, side table, magazine rack, television cabinet
    decor      floor plant, small plants, painting, wall clock, vases, candlesticks, curtains, wall shelf,
               cushion pile, globe, fruit bowl and teapot, bust

Renders go to Art/StyleTests/furniture_kit_18/:
    sheet_<kit>.png   every piece of the kit in every line (rule I7: told apart by outline)
    room_<kit>.png    the kit furnished as a room in one line, kit daylight through the side window (I5)
Not approved: for the owner to choose from.
"""
import bpy, math, random, os, time, traceback

ROOT = os.environ.get("WASTED_ROOT", "C:/Projects/Wasted").replace("\\", "/").rstrip("/")
_k = open(ROOT + "/Tools/blender/furniture_kit_17.py", encoding="utf-8").read()
_k = _k[:_k.rstrip().rfind("\nmain()")]                   # kit 17's pieces, lines and rooms, without running it
exec(compile(_k, "furniture_kit_17_helpers", "exec"), globals())

NAME = "furniture_kit_18"
OUT = ROOT + "/Art/StyleTests"
SRC = ROOT + "/Art/Source/StyleTests"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"
_log_t0 = time.time()


def log(msg):
    line = time.strftime("%H:%M:%S") + " " + msg
    print(line)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")


# ---- more kit parts -----------------------------------------------------------------------------------------------
def extra_mats():
    M.update(porcelain=painted("M_Porcelain", (0.62, 0.60, 0.55), (0.78, 0.75, 0.68), noise_scale=2.0, bump=0.08),
             enamel=painted("M_Enamel", (0.60, 0.56, 0.46), (0.74, 0.70, 0.58), noise_scale=2.0, bump=0.08),
             basin=flat("M_BasinIn", (0.30, 0.31, 0.32), bump=0.05),
             mirror=flat("M_Mirror", (0.34, 0.40, 0.44), rough=0.85, bump=0.02),
             chalk=flat("M_Chalkboard", (0.10, 0.13, 0.12), bump=0.1),
             cork=painted("M_Cork", (0.42, 0.28, 0.15), (0.58, 0.42, 0.24), noise_scale=9.0, bump=0.4),
             wicker=painted("M_Wicker", (0.40, 0.29, 0.15), (0.62, 0.48, 0.28), noise_scale=14.0, bump=0.5),
             pot=painted("M_PotTerra", (0.40, 0.20, 0.13), (0.58, 0.32, 0.20), noise_scale=3.0, bump=0.2),
             ember=emissive("M_Ember", (1.0, 0.45, 0.15), 3.0),
             flame=emissive("M_Flame", (1.0, 0.70, 0.35), 4.0),
             screen=flat("M_Screen", (0.12, 0.15, 0.16), bump=0.05),
             keys=flat("M_Keys", (0.72, 0.68, 0.58)), keys_dark=flat("M_KeysDark", (0.06, 0.06, 0.06)),
             ink=flat("M_Ink", (0.08, 0.08, 0.09)), indigo=flat("M_Indigo", (0.10, 0.14, 0.24)))


def C(*names):
    return [M[n] for n in names]


def base(P, n, L, w, d, h=0.12):
    """What a cabinet stands on, by line: turned or tapered legs, a plinth, or an iron frame. Returns where the body starts."""
    k = L["leg"]
    if k in ("turned", "taper"):
        legs4(P, n, L, w, d, h + 0.02, 0.06)
        return h
    if k == "iron":
        for sx in (-1, 1):
            for sy in (-1, 1):
                rod(P, "%s_IL%d%d" % (n, sx, sy), M["iron"], (sx * (w / 2 - 0.03), sy * (d / 2 - 0.03), 0),
                    (sx * (w / 2 - 0.03), sy * (d / 2 - 0.03), h + 0.03), 0.021)
        rbox(P, n + "_IFrame", M["iron"], 0, 0, h, w, d, 0.04, 0.012, 2)
        return h + 0.02
    rbox(P, n + "_Plinth", L["wood_dark"], 0, 0.02, 0.05, w - 0.04, d - 0.04, 0.1, 0.015)
    return 0.1


def body(P, n, L, w, d, z0, z1, mat=None):
    """The carcass of a cabinet; Curve rounds it, Old Town crowns it."""
    mat = mat or L["wood"]
    r = 0.06 if L["round"] else L["r"]
    rbox(P, n + "_Body", mat, 0, 0, (z0 + z1) / 2, w, d, z1 - z0, r)
    if L["panels"]:
        rbox(P, n + "_Cornice", L["wood"] if mat is L["wood"] else mat, 0, 0.01, z1 + 0.03, w + 0.07, d + 0.05, 0.06, 0.02)


def doors(P, n, L, w, d, z0, z1, count, mat=None, knob_z=None):
    """count doors side by side across the front, each with its handle near the middle."""
    mat = mat or L["wood"]
    dw = w / count
    for i in range(count):
        x = -w / 2 + dw * (i + 0.5)
        panel_door(P, "%s_Door%d" % (n, i), L, x, z0, dw - 0.01, z1 - z0, -d / 2, mat)
        hx = x + (dw / 2 - 0.09) * (1 if i % 2 == 0 else -1) if count > 1 else x
        if count == 2:
            hx = x + (dw / 2 - 0.07) * (1 if i == 0 else -1)
        handle(P, "%s_Knob%d" % (n, i), L, hx, -d / 2 - 0.05, knob_z if knob_z is not None else (z0 + z1) / 2)


def drawers(P, n, L, x, w, d, z0, z1, rows, mat=None):
    """rows drawers stacked from z0 to z1, each with a centred handle."""
    mat = mat or L["wood"]
    h = (z1 - z0) / rows
    for i in range(rows):
        zz = z0 + h * i
        panel_door(P, "%s_Drw%d" % (n, i), L, x, zz, w, h, -d / 2, mat)
        handle(P, "%s_DKnob%d" % (n, i), L, x, -d / 2 - 0.05, zz + h * 0.55)


def tbl(P, n, L, w, d, h, round_=None):
    """A table in the line's way: legs and a top, or a pedestal under a round top."""
    round_ = L["round"] if round_ is None else round_
    if round_:
        cylz(P, n + "_Ped", L["wood"], 0, 0, 0, min(w, d) * 0.13, h - 0.05, r_top=min(w, d) * 0.08)
        cylz(P, n + "_Foot", L["wood"], 0, 0, 0, min(w, d) * 0.32, 0.05, bev=0.02)
        top(P, n + "_Top", L, w, d, h - 0.05, round_=True)
    else:
        legs4(P, n, L, w, d, h - L["top"], 0.05)
        top(P, n + "_Top", L, w, d, h - L["top"])


def stool(P, n, L, h=0.45, s=0.36, ring=False):
    if L["round"]:
        for i in range(3):
            a = i * 2 * math.pi / 3
            rod(P, "%s_L%d" % (n, i), L["wood"], (math.cos(a) * s * 0.45, math.sin(a) * s * 0.45, 0),
                (math.cos(a) * s * 0.28, math.sin(a) * s * 0.28, h - 0.04), 0.024)
        cushion(P, n + "_Seat", L, L["cloth"], 0, 0, h, s + 0.04, s + 0.04, 0.09)
    else:
        legs4(P, n, L, s, s, h - 0.04, 0.04)
        rbox(P, n + "_Top", L["wood"], 0, 0, h - 0.02, s + 0.02, s + 0.02, 0.05, L["r"])
        if L["buttons"]:
            cushion(P, n + "_Pad", L, L["cloth"], 0, 0, h + 0.03, s - 0.04, s - 0.04, 0.05)
    if ring:
        zr = h * 0.35
        m = M["iron"] if L["leg"] == "iron" else L["wood"]
        for sx, sy, ex, ey in ((-1, -1, 1, -1), (1, -1, 1, 1), (1, 1, -1, 1), (-1, 1, -1, -1)):
            a = s / 2 - 0.02
            rod(P, "%s_Ring%d%d" % (n, sx, sy), m, (sx * a, sy * a, zr), (ex * a, ey * a, zr), 0.021)


def plates(P, n, x0, x1, y, z, mats, r=0.1):
    """Plates standing on edge along a shelf, faces to the front."""
    x, i = x0, 0
    while x < x1 - r:
        rod(P, "%s_Pl%d" % (n, i), mats[i % len(mats)], (x + r, y - 0.012, z + r), (x + r, y + 0.012, z + r), r)
        x += 2 * r + 0.03
        i += 1


def jars(P, n, x0, x1, y, z, rng):
    x, i = x0, 0
    while x < x1 - 0.06:
        r = rng.uniform(0.04, 0.065)
        h = rng.uniform(0.1, 0.2)
        cylz(P, "%s_Jar%d" % (n, i), M[rng.choice(("cream", "terra", "teal", "ochre", "porcelain"))], x + r, y, z, r, h, bev=0.01)
        cylz(P, "%s_Lid%d" % (n, i), M["wood"], x + r, y, z + h, r * 0.9, 0.025, bev=0.008)
        x += 2 * r + 0.03
        i += 1


def plant(P, n, x, y, z, s, rng, pot=True):
    """A pot and a clump of soft leaves (option B blobs, matte)."""
    if pot:
        cylz(P, n + "_Pot", M["pot"], x, y, z, 0.16 * s, 0.26 * s, r_top=0.2 * s, bev=0.015)
        cylz(P, n + "_Soil", M["dark"], x, y, z + 0.24 * s, 0.18 * s, 0.025, bev=0.005)
        z += 0.26 * s
    k = 5 if s < 1.0 else 9                                  # big plants get more, smaller leaves on stems
    for i in range(k):
        a = i * 2.4 + rng.uniform(-0.3, 0.3)
        rr = rng.uniform(0.04, 0.16) * s
        lz = z + rng.uniform(0.12, 0.42 if k > 5 else 0.34) * s
        lx, ly = x + math.cos(a) * rr, y + math.sin(a) * rr
        sz = (0.3 if k == 5 else 0.19) * s
        if k > 5:
            rod(P, "%s_Stem%d" % (n, i), M["leaf"], (x, y, z), (lx, ly, lz - sz * 0.3), 0.021)
        blob(P, "%s_Leaf%d" % (n, i), M["leaf" if i % 2 else "leaf_light"], lx, ly, lz, sz, sz * 0.8, sz * 1.25,
             top=(0.6, 0.6), levels=2)


def cloth_hang(P, n, mat, x, y, z_top, w, h, folds=4):
    """Hanging cloth (towels, garments, curtains) as soft vertical folds."""
    fw = w / folds
    for i in range(folds):
        rbox(P, "%s_F%d" % (n, i), mat, x - w / 2 + fw * (i + 0.5), y + (0.012 if i % 2 else -0.012), z_top - h / 2,
             fw + 0.02, 0.05, h, 0.024, 3)


def frame_rect(P, n, mat, x, y, z, w, h, t=0.06, depth=0.05):
    """A picture or mirror frame standing in the x-z plane, facing -y."""
    rbox(P, n + "_FT", mat, x, y, z + h / 2 - t / 2, w, depth, t, 0.012, 2)
    rbox(P, n + "_FB", mat, x, y, z - h / 2 + t / 2, w, depth, t, 0.012, 2)
    rbox(P, n + "_FL", mat, x - w / 2 + t / 2, y, z, t, depth, h, 0.012, 2)
    rbox(P, n + "_FR", mat, x + w / 2 - t / 2, y, z, t, depth, h, 0.012, 2)


def tap(P, n, x, y, z, reach=0.16):
    rod(P, n + "_Up", M["iron"], (x, y, z), (x, y, z + 0.22), 0.021)
    rod(P, n + "_Out", M["iron"], (x, y, z + 0.22), (x, y - reach, z + 0.22), 0.021)
    rod(P, n + "_Spout", M["iron"], (x, y - reach, z + 0.22), (x, y - reach, z + 0.15), 0.021)
    for s in (-1, 1):
        ball(P, "%s_H%d" % (n, s), M["iron"], x + s * 0.07, y, z + 0.07, 0.06, 0.06, 0.05)


def disc(ob, x, y, z):
    """Stand a round part (made lying flat by cylz) up on edge, facing the front."""
    ob.rotation_euler = (math.pi / 2, 0, 0)
    ob.location = (x, y, z)
    return ob


def lamp_light(P, n, x, y, z, e=40.0):
    light(P, n, x, y, z, e)


# ---- kitchen ------------------------------------------------------------------------------------------------------
def k_counter(P, L, rng, w=1.8):
    d, h = 0.6, 0.9
    z0 = base(P, "Ctr", L, w, d - 0.02)
    body(P, "Ctr", L, w, d, z0, h - L["top"])
    drawers(P, "Ctr", L, -w / 3, w / 3 - 0.02, d, h - L["top"] - 0.2, h - L["top"] - 0.02, 1)
    drawers(P, "CtrB", L, 0, w / 3 - 0.02, d, h - L["top"] - 0.2, h - L["top"] - 0.02, 1)
    drawers(P, "CtrC", L, w / 3, w / 3 - 0.02, d, h - L["top"] - 0.2, h - L["top"] - 0.02, 1)
    doors(P, "Ctr", L, w, d, z0 + 0.02, h - L["top"] - 0.22, 3, knob_z=h - L["top"] - 0.32)
    rbox(P, "Ctr_Top", L["wood_dark"] if L["key"] == "plain" else L["wood"], 0, 0.0, h - L["top"] / 2, w + 0.04, d + 0.03, L["top"], L["r"])
    rbox(P, "Ctr_Board", L["accent_wood"], -0.45, -0.02, h + 0.02, 0.4, 0.28, 0.04, 0.012)
    jars(P, "Ctr", 0.3, 0.8, 0.18, h, rng)


def k_wall(P, L, rng):
    w, d, z0, z1 = 1.2, 0.34, 1.45, 2.15
    if L["leg"] == "iron":                                   # Workshop: open shelves on iron brackets, jars on show
        for i, z in enumerate((z0, z0 + 0.36, z1 - 0.04)):
            rbox(P, "Wall_Sh%d" % i, L["wood"], 0, 0, z, w, d - 0.06, 0.05, L["r"])
            for s in (-1, 1):
                rbox(P, "Wall_Br%d%d" % (i, s), M["iron"], s * (w / 2 - 0.12), d / 2 - 0.1, z - 0.09, 0.04, 0.16, 0.14, 0.01, 1)
            if z < z1 - 0.1:
                jars(P, "Wall%d" % i, -w / 2 + 0.06, w / 2 - 0.06, 0.0, z + 0.025, rng)
        return
    body(P, "Wall", L, w, d, z0, z1)
    doors(P, "Wall", L, w, d, z0 + 0.02, z1 - 0.02, 2, knob_z=z0 + 0.12)
    rbox(P, "Wall_Under", L["wood_dark"], 0, 0, z0 - 0.02, w, d - 0.02, 0.03, 0.008, 1)


def k_stove(P, L, rng):
    w, d, h = 0.75, 0.62, 0.9
    iron_ = L["leg"] in ("iron", "turned")
    m = M["iron"] if iron_ else M["enamel"]
    if L["round"]:
        blob(P, "Stove_Body", m, 0, 0, h / 2, w + 0.06, d + 0.04, h + 0.04, top=(0.95, 0.95), levels=2)
    else:
        z0 = 0.1 if iron_ else 0.0
        if iron_:
            for sx in (-1, 1):
                for sy in (-1, 1):
                    ball(P, "Stove_Ft%d%d" % (sx, sy), M["iron"], sx * (w / 2 - 0.06), sy * (d / 2 - 0.06), 0.05, 0.09, 0.09, 0.1)
        rbox(P, "Stove_Body", m, 0, 0, z0 + (h - z0) / 2, w, d, h - z0, L["r"])
    rbox(P, "Stove_Oven", M["iron"] if not iron_ else M["dark"], 0, -d / 2 - 0.01, 0.42, w - 0.16, 0.04, 0.38, 0.02)
    rod(P, "Stove_Bar", M["iron"] if not iron_ else L["accent_wood"], (-w / 2 + 0.12, -d / 2 - 0.07, 0.66), (w / 2 - 0.12, -d / 2 - 0.07, 0.66), 0.021)
    for i, (x, y) in enumerate(((-0.17, -0.12), (0.17, -0.12), (-0.17, 0.14), (0.17, 0.14))):
        cylz(P, "Stove_Ring%d" % i, M["dark"], x, y, h, 0.1, 0.025, bev=0.006)
    for i in range(4):
        ball(P, "Stove_Dial%d" % i, M["cream"] if iron_ else M["iron"], -0.24 + i * 0.16, -d / 2 - 0.03, h - 0.08, 0.06, 0.05, 0.06)
    if iron_:
        rbox(P, "Stove_Back", m, 0, d / 2 - 0.03, h + 0.25, w, 0.06, 0.5, L["r"])
        rbox(P, "Stove_Rail", M["iron"], 0, d / 2 - 0.1, h + 0.42, w - 0.05, 0.05, 0.05, 0.01)
        rod(P, "Stove_Flue", M["iron"], (0, d / 2 - 0.05, h + 0.5), (0, d / 2 - 0.05, 2.3), 0.07)
    cylz(P, "Stove_Pot", M["iron"] if not iron_ else M["terra"], -0.17, 0.14, h + 0.025, 0.11, 0.15, bev=0.012)
    cylz(P, "Stove_Kettle", M["teal"], 0.17, -0.12, h + 0.025, 0.09, 0.14, r_top=0.05, bev=0.02)


def k_sink(P, L, rng):
    w, d, h = 1.1, 0.6, 0.9
    z0 = base(P, "Sink", L, w, d - 0.02)
    body(P, "Sink", L, w, d, z0, h - L["top"])
    doors(P, "Sink", L, w, d, z0 + 0.02, h - L["top"] - 0.04, 2)
    if L["key"] in ("oldtown", "workshop"):                  # a deep stone sink standing proud of the front
        rbox(P, "Sink_Bowl", M["porcelain"], 0, -0.04, h - 0.05, 0.62, 0.5, 0.28, 0.04)
        rbox(P, "Sink_In", M["basin"], 0, -0.04, h + 0.087, 0.5, 0.38, 0.01, 0.004, 1)
        rbox(P, "Sink_TopL", L["wood"], -0.44, 0, h - L["top"] / 2, 0.22, d + 0.03, L["top"], L["r"])
        rbox(P, "Sink_TopR", L["wood"], 0.44, 0, h - L["top"] / 2, 0.22, d + 0.03, L["top"], L["r"])
    else:
        rbox(P, "Sink_Top", L["wood_dark"] if L["key"] == "plain" else L["wood"], 0, 0, h - L["top"] / 2, w + 0.04, d + 0.03, L["top"], L["r"])
        rbox(P, "Sink_Rim", M["porcelain"], 0, -0.02, h + 0.012, 0.6, 0.44, 0.03, 0.03 if L["round"] else 0.012)
        rbox(P, "Sink_In", M["basin"], 0, -0.02, h + 0.028, 0.52, 0.36, 0.006, 0.02 if L["round"] else 0.006, 1)
    tap(P, "Sink_Tap", 0, d / 2 - 0.08, h + (0.09 if L["key"] in ("oldtown", "workshop") else 0.0))


def k_fridge(P, L, rng):
    w, d, h = 0.68, 0.66, 1.75
    if L["round"]:
        blob(P, "Fr_Body", M["enamel"], 0, 0, h / 2 + 0.05, w + 0.08, d + 0.06, h + 0.1, top=(0.88, 0.95), levels=3)
        rbox(P, "Fr_Line", M["iron"], 0, -d / 2 + 0.03, 1.22, w - 0.08, 0.04, 0.02, 0.008, 1)
        rbox(P, "Fr_Pull", M["iron"], w / 2 - 0.14, -d / 2 + 0.0, 1.02, 0.05, 0.08, 0.24, 0.02)
        rbox(P, "Fr_Badge", L["accent"], 0, -d / 2 + 0.02, 1.55, 0.18, 0.03, 0.06, 0.012)
        return
    if L["key"] == "oldtown":                                # an ice box: panelled wood, knobs
        z0 = base(P, "Fr", L, w, d)
        body(P, "Fr", L, w, d, z0, h)
        panel_door(P, "Fr_Top", L, 0, 1.15, w - 0.04, h - 1.19, -d / 2, L["wood"])
        panel_door(P, "Fr_Bot", L, 0, z0 + 0.04, w - 0.04, 1.05 - z0, -d / 2, L["wood"])
        for z in (1.4, 0.75):
            rbox(P, "Fr_Latch%d" % int(z * 10), L["accent_wood"], w / 2 - 0.12, -d / 2 - 0.05, z, 0.07, 0.06, 0.16, 0.02)
        return
    m = M["enamel"] if L["key"] == "plain" else M["iron"]
    rbox(P, "Fr_Body", m, 0, 0, h / 2, w, d, h, L["r"] * 2)
    rbox(P, "Fr_Gap", M["dark"], 0, -d / 2, 1.2, w - 0.04, 0.012, 0.018, 0.004, 1)
    for z in (1.45, 0.75):
        rbox(P, "Fr_Pull%d" % int(z * 10), M["iron"] if m is M["enamel"] else L["accent_wood"], w / 2 - 0.08, -d / 2 - 0.04, z, 0.04, 0.05, 0.3, 0.012)
    if L["straps"]:
        for z in (0.2, 1.65):
            rbox(P, "Fr_Strap%d" % int(z * 10), L["wood"], 0, -d / 2 - 0.01, z, w + 0.02, 0.03, 0.06, 0.01)


def k_potshelf(P, L, rng):
    w, d = 1.0, 0.3
    side = M["iron"] if L["leg"] == "iron" else L["wood"]
    for s in (-1, 1):
        rbox(P, "Pot_Side%d" % s, side, s * (w / 2 - 0.025), 0, 0.55, 0.05, d, 1.1, L["r"])
    for i, z in enumerate((0.08, 0.5, 1.08)):
        rbox(P, "Pot_Shelf%d" % i, L["wood"], 0, 0, z, w - 0.02, d - 0.02, 0.045, L["r"] * 0.6, 2)
    cylz(P, "Pot_A", M["iron"], -0.25, 0, 0.1, 0.13, 0.18, bev=0.012)
    cylz(P, "Pot_B", M["terra"], 0.15, 0, 0.1, 0.16, 0.12, bev=0.012)
    jars(P, "Pot", -0.42, 0.42, 0, 0.52, rng)
    plates(P, "Pot", -0.42, 0.3, 0.06, 1.1, C("cream", "teal", "cream"), r=0.1)
    cylz(P, "Pot_Bowl", M["ochre"], 0.35, 0, 1.1, 0.1, 0.07, r_top=0.12, bev=0.012)


# ---- bathroom -----------------------------------------------------------------------------------------------------
def b_bath(P, L, rng):
    w, d, h = 1.7, 0.78, 0.6
    k = L["key"]
    if k == "oldtown":                                       # a roll-top tub on four feet
        blob(P, "Bath_Tub", M["porcelain"], 0, 0, 0.42, w + 0.1, d + 0.1, 0.62, bottom=(0.82, 0.8), levels=3)
        for sx in (-1, 1):
            for sy in (-1, 1):
                ball(P, "Bath_Ft%d%d" % (sx, sy), L["accent_wood"], sx * (w / 2 - 0.22), sy * (d / 2 - 0.16), 0.07, 0.12, 0.12, 0.14)
        blob(P, "Bath_In", M["basin"], 0, 0, 0.7, w - 0.24, d - 0.22, 0.1, levels=2)
    elif k == "curve":
        blob(P, "Bath_Tub", M["porcelain"], 0, 0, 0.32, w + 0.12, d + 0.12, 0.64, top=(1.0, 1.0), bottom=(0.85, 0.85), levels=3)
        blob(P, "Bath_In", M["basin"], 0, 0, 0.6, w - 0.2, d - 0.18, 0.1, levels=2)
    else:
        rbox(P, "Bath_Panel", L["wood"] if k == "workshop" else M["porcelain"], 0, 0, h / 2, w, d, h, L["r"] * 2)
        rbox(P, "Bath_Rim", M["porcelain"], 0, 0, h + 0.02, w + 0.04, d + 0.04, 0.05, 0.02)
        rbox(P, "Bath_In", M["basin"], 0, 0, h + 0.047, w - 0.2, d - 0.2, 0.01, 0.08, 2)
        if L["straps"]:
            for x in (-0.55, 0.0, 0.55):
                rbox(P, "Bath_Strap%d" % int(x * 10), M["iron"], x, -d / 2 - 0.01, h / 2, 0.06, 0.02, h - 0.06, 0.008, 1)
    tap(P, "Bath_Tap", w / 2 - 0.12, 0.0, (0.68 if k == "oldtown" else 0.62))
    cloth_hang(P, "Bath_Towel", L["accent"], -w / 2 + 0.3, -d / 2 - 0.03, h + 0.04, 0.36, 0.34, 3)


def b_basin(P, L, rng):
    h = 0.85
    k = L["key"]
    if k in ("oldtown", "curve"):
        cylz(P, "Bas_Ped", M["porcelain"], 0, 0.04, 0, 0.12, h - 0.12, r_top=0.09, bev=0.02)
        if k == "curve":
            blob(P, "Bas_Bowl", M["porcelain"], 0, 0, h - 0.07, 0.62, 0.5, 0.18, bottom=(0.7, 0.7), levels=3)
        else:
            rbox(P, "Bas_Bowl", M["porcelain"], 0, 0, h - 0.07, 0.6, 0.46, 0.16, 0.05)
    elif k == "plain":
        rbox(P, "Bas_Cab", L["wood"], 0, 0.02, (h - 0.12) / 2, 0.58, 0.42, h - 0.12, L["r"])
        doors(P, "Bas", L, 0.58, 0.46, 0.04, h - 0.16, 2)
        rbox(P, "Bas_Bowl", M["porcelain"], 0, 0, h - 0.07, 0.62, 0.48, 0.14, 0.03)
    else:
        for sx in (-1, 1):
            rod(P, "Bas_Leg%d" % sx, M["iron"], (sx * 0.26, -0.15, 0), (sx * 0.26, -0.15, h - 0.12), 0.021)
            rod(P, "Bas_LegB%d" % sx, M["iron"], (sx * 0.26, 0.18, 0), (sx * 0.26, 0.18, h - 0.12), 0.021)
        rbox(P, "Bas_Shelf", L["wood"], 0, 0.02, 0.22, 0.56, 0.38, 0.05, L["r"])
        rbox(P, "Bas_Top", L["wood"], 0, 0, h - 0.13, 0.62, 0.46, L["top"], L["r"])
        rbox(P, "Bas_Bowl", M["porcelain"], 0, -0.02, h - 0.03, 0.46, 0.36, 0.12, 0.04)
    rbox(P, "Bas_In", M["basin"], 0, -0.02, h + 0.015 if k == "workshop" else h + 0.005, 0.4, 0.28, 0.006, 0.05, 2)
    tap(P, "Bas_Tap", 0, 0.17, h - 0.01, 0.13)
    rbox(P, "Bas_Soap", L["accent"], 0.22, 0.1, h + 0.02, 0.08, 0.05, 0.03, 0.01)


def b_toilet(P, L, rng):
    k = L["key"]
    blob(P, "WC_Bowl", M["porcelain"], 0, -0.05, 0.22, 0.42, 0.56, 0.44, top=(1.0, 1.0), bottom=(0.65, 0.7), levels=3)
    rbox(P, "WC_Seat", L["wood"] if k in ("oldtown", "workshop") else M["porcelain"], 0, -0.06, 0.43, 0.4, 0.48, 0.04, 0.04 if L["round"] else 0.02)
    rbox(P, "WC_Lid", L["wood"] if k in ("oldtown", "workshop") else M["porcelain"], 0, 0.15, 0.68, 0.38, 0.04, 0.44, 0.02)
    if k == "oldtown":                                       # high tank and a pull chain
        rbox(P, "WC_Tank", L["wood"], 0, 0.24, 1.95, 0.46, 0.2, 0.26, L["r"])
        rod(P, "WC_Pipe", M["iron"], (0, 0.24, 0.45), (0, 0.24, 1.82), 0.025)
        rod(P, "WC_Chain", M["iron"], (0.18, 0.18, 1.82), (0.18, 0.18, 1.35), 0.02)
        ball(P, "WC_Pull", L["accent_wood"], 0.18, 0.18, 1.3, 0.06, 0.06, 0.1)
    else:
        rbox(P, "WC_Tank", M["porcelain"] if k != "workshop" else L["wood"], 0, 0.22, 0.62, 0.44, 0.2, 0.4, 0.06 if L["round"] else 0.02)
        ball(P, "WC_Button", M["iron"], 0, 0.22, 0.83, 0.07, 0.07, 0.03)


def b_rail(P, L, rng):
    w = 0.7
    m = M["iron"] if L["leg"] in ("iron",) else L["wood"]
    for s in (-1, 1):
        if L["leg"] == "turned":
            leg(P, "Rail_Post%d" % s, L, s * w / 2, 0, 0.9)
        else:
            rod(P, "Rail_Post%d" % s, m, (s * w / 2, 0, 0), (s * w / 2, 0, 0.9), 0.024)
        rbox(P, "Rail_Foot%d" % s, m, s * w / 2, 0, 0.02, 0.06, 0.32, 0.04, 0.012)
    for i, z in enumerate((0.55, 0.88)):
        rod(P, "Rail_Bar%d" % i, m, (-w / 2, 0, z), (w / 2, 0, z), 0.022)
    cloth_hang(P, "Rail_TowelA", L["accent"], -0.1, -0.03, 0.9, 0.42, 0.42, 3)
    cloth_hang(P, "Rail_TowelB", M["cream"], 0.12, 0.03, 0.57, 0.4, 0.3, 3)


def b_mirror(P, L, rng):
    w, d, z0, z1 = 0.6, 0.16, 1.25, 1.95
    if L["round"]:
        disc(cylz(P, "Mir_Back", L["wood"], 0, 0, 0, 0.36, 0.06, segs=40, bev=0.02), 0, 0.03, 1.6)
        disc(cylz(P, "Mir_Glass", M["mirror"], 0, 0, 0, 0.3, 0.02, segs=40, bev=0.006), 0, -0.01, 1.6)
        rbox(P, "Mir_Shelf", L["wood"], 0, -0.05, 1.18, 0.55, 0.14, 0.04, 0.02)
    else:
        body(P, "Mir", L, w, d, z0, z1)
        rbox(P, "Mir_Glass", M["mirror"], 0, -d / 2 - 0.008, (z0 + z1) / 2, w - 0.12, 0.012, z1 - z0 - 0.12, 0.006, 1)
        handle(P, "Mir_Knob", L, w / 2 - 0.08, -d / 2 - 0.04, (z0 + z1) / 2)
        rbox(P, "Mir_Shelf", L["wood"], 0, -0.05, z0 - 0.07, w, 0.16, 0.04, L["r"])
    for i, (x, mt) in enumerate(((-0.18, "teal"), (-0.08, "cream"), (0.12, "terra"))):
        cylz(P, "Mir_Bottle%d" % i, M[mt], x, -0.06, 1.2, 0.035, 0.14 - i * 0.02, r_top=0.02, bev=0.008)


def b_basket(P, L, rng):
    if L["key"] == "workshop":
        rbox(P, "Bsk_Box", L["wood"], 0, 0, 0.3, 0.5, 0.38, 0.6, 0.015)
        for z in (0.12, 0.3, 0.48):
            rbox(P, "Bsk_Slat%d" % int(z * 100), L["wood_dark"], 0, -0.195, z, 0.48, 0.012, 0.07, 0.006, 1)
    else:
        cylz(P, "Bsk_Body", M["wicker"], 0, 0, 0, 0.22, 0.55, r_top=0.25, segs=28, bev=0.02)
        cylz(P, "Bsk_Lid", M["wicker"], 0, 0, 0.55, 0.26, 0.05, bev=0.02)
        ball(P, "Bsk_Knob", L["accent_wood"], 0, 0, 0.62, 0.07, 0.07, 0.05)
    cushion(P, "Bsk_Cloth", L, L["accent"], 0.12, -0.08, 0.6 if L["key"] == "workshop" else 0.58, 0.24, 0.2, 0.06)
    rbox(P, "Bsk_Mat", L["rug"], -0.6, -0.1, 0.01, 0.8, 0.5, 0.02, 0.01, 1)


# ---- dining -------------------------------------------------------------------------------------------------------
def d_sideboard(P, L, rng):
    w, d, h = 1.6, 0.48, 0.85
    z0 = base(P, "Sb", L, w, d)
    body(P, "Sb", L, w, d, z0, h)
    drawers(P, "Sb", L, 0, w / 3 - 0.02, d, h - 0.2, h - 0.02, 1)
    doors(P, "Sb", L, w, d, z0 + 0.02, h - 0.22, 3)
    cylz(P, "Sb_Vase", M["teal"], -0.5, 0, h + (0.06 if L["panels"] else 0), 0.08, 0.3, r_top=0.05, bev=0.02)
    cylz(P, "Sb_Bowl", M["ochre"], 0.4, 0, h + (0.06 if L["panels"] else 0), 0.08, 0.08, r_top=0.16, bev=0.015)
    for i in range(3):
        ball(P, "Sb_Fruit%d" % i, M[("terra", "olive", "ochre")[i]], 0.36 + i * 0.05, (i - 1) * 0.04, h + 0.15 + (0.06 if L["panels"] else 0), 0.08, 0.08, 0.08)


def d_dresser(P, L, rng):
    w, d, h = 1.2, 0.48, 0.9
    z0 = base(P, "Dr", L, w, d)
    body(P, "Dr", L, w, d, z0, h)
    doors(P, "Dr", L, w, d, z0 + 0.02, h - 0.04, 2)
    rbox(P, "Dr_Top", L["wood"], 0, 0, h + 0.02, w + 0.04, d + 0.03, 0.05, L["r"])
    uw, ud, uh = w - 0.06, 0.26, 1.05
    side = M["iron"] if L["leg"] == "iron" else L["wood"]
    for s in (-1, 1):
        rbox(P, "Dr_USide%d" % s, side, s * (uw / 2 - 0.025), ud / 2 - d / 2 + 0.1, h + 0.04 + uh / 2, 0.05, ud, uh, L["r"])
    yb = ud / 2 + 0.1 - d / 2
    rbox(P, "Dr_UBack", L["wood_dark"], 0, d / 2 - 0.13, h + 0.04 + uh / 2, uw - 0.06, 0.02, uh, 0.006, 1)
    for i, z in enumerate((h + 0.4, h + 0.75, h + 1.07)):
        rbox(P, "Dr_UShelf%d" % i, L["wood"], 0, yb, z, uw, ud, 0.045, L["r"] * 0.6, 2)
    if L["panels"]:
        rbox(P, "Dr_Crown", L["wood"], 0, yb, h + 1.14, uw + 0.08, ud + 0.05, 0.07, 0.02)
    plates(P, "DrA", -uw / 2 + 0.06, uw / 2 - 0.06, d / 2 - 0.18, h + 0.07, C("cream", "teal", "cream", "terra"), r=0.11)
    plates(P, "DrB", -uw / 2 + 0.06, uw / 2 - 0.06, d / 2 - 0.18, h + 0.43, C("cream", "dusty", "cream"), r=0.12)
    jars(P, "Dr", -uw / 2 + 0.06, uw / 2 - 0.06, yb, h + 0.775, rng)


def d_bench(P, L, rng):
    w, d, h = 1.4, 0.38, 0.45
    if L["round"]:
        for s in (-1, 1):
            rbox(P, "Bn_End%d" % s, L["wood"], s * (w / 2 - 0.05), 0, h / 2, 0.08, d - 0.04, h, 0.035)
        blob(P, "Bn_Seat", L["cloth"], 0, 0, h, w + 0.04, d + 0.06, 0.12, levels=2)
        return
    legs4(P, "Bn", L, w, d, h - L["top"], 0.06)
    rbox(P, "Bn_Seat", L["wood"], 0, 0, h - L["top"] / 2, w, d, L["top"], L["r"])
    if L["leg"] == "iron":
        rbox(P, "Bn_Stretch", M["iron"], 0, 0, 0.12, w - 0.14, 0.04, 0.04, 0.012)
    else:
        rbox(P, "Bn_Stretch", L["wood"], 0, 0, 0.14, w - 0.14, 0.05, 0.05, 0.012)
    if L["buttons"]:
        cushion(P, "Bn_Pad", L, L["cloth"], 0, 0, h + 0.035, w - 0.08, d - 0.06, 0.06)


def d_barstool(P, L, rng):
    stool(P, "Bar", L, h=0.75, s=0.38, ring=not L["round"])
    if L["round"]:
        cylz(P, "Bar_Ring", M["iron"], 0, 0, 0.26, 0.2, 0.03, bev=0.01)
    if L["panels"] or L["leg"] == "iron":
        rbox(P, "Bar_Back", L["wood"], 0, 0.16, 0.95, 0.34, 0.05, 0.12, L["r"])
        for s in (-1, 1):
            rbox(P, "Bar_BPost%d" % s, M["iron"] if L["leg"] == "iron" else L["wood"], s * 0.14, 0.16, 0.84, 0.045, 0.045, 0.2, 0.012, 2)


def d_display(P, L, rng):
    """A tall display cabinet: an open, framed upper case (no shiny glass, rule I1) over a cupboard."""
    w, d, h = 0.9, 0.42, 1.9
    z0 = base(P, "Dc", L, w, d)
    body(P, "Dc", L, w, d, z0, 0.85)
    doors(P, "Dc", L, w, d, z0 + 0.02, 0.83, 2)
    side = M["iron"] if L["leg"] == "iron" else L["wood"]
    for s in (-1, 1):
        rbox(P, "Dc_Side%d" % s, side, s * (w / 2 - 0.03), 0, 0.85 + (h - 0.85) / 2, 0.06, d, h - 0.85, L["r"])
    rbox(P, "Dc_Back", L["wood_dark"], 0, d / 2 - 0.02, 0.85 + (h - 0.85) / 2, w - 0.08, 0.03, h - 0.85, 0.006, 1)
    rbox(P, "Dc_TopB", L["wood"], 0, 0, h, w + (0.06 if L["panels"] else 0.0), d + 0.03, 0.06, L["r"])
    for i, z in enumerate((1.2, 1.55)):
        rbox(P, "Dc_Shelf%d" % i, L["wood"], 0, 0, z, w - 0.08, d - 0.04, 0.035, 0.01)
    rbox(P, "Dc_Mull", side, 0, -d / 2 - 0.01, 0.85 + (h - 0.85) / 2, 0.045, 0.03, h - 0.85, 0.01, 1)
    rbox(P, "Dc_MullH", side, 0, -d / 2 - 0.01, 1.45, w - 0.08, 0.03, 0.04, 0.01, 1)
    plates(P, "Dc", -w / 2 + 0.08, w / 2 - 0.08, 0.08, 0.87, C("cream", "teal", "cream"), r=0.13)
    for i, x in enumerate((-0.25, -0.05, 0.15, 0.3)):
        cylz(P, "Dc_Cup%d" % i, M[("cream", "dusty", "terra", "cream")[i]], x, -0.02, 1.22, 0.05, 0.09, bev=0.01)
        cylz(P, "Dc_Jug%d" % i, M[("ochre", "cream", "teal", "plum")[i]], x, 0.0, 1.57, 0.06, 0.18, r_top=0.04, bev=0.012)


def d_pendant(P, L, rng):
    z = 2.0
    rod(P, "Pen_Cord", M["iron"], (0, 0, z), (0, 0, 2.95), 0.021)
    if L["round"]:
        ball(P, "Pen_Shade", M["shade"], 0, 0, z - 0.08, 0.46, 0.46, 0.36)
    elif L["leg"] == "iron":
        cylz(P, "Pen_Shade", M["iron"], 0, 0, z - 0.22, 0.24, 0.22, r_top=0.06, bev=0.012)
        ball(P, "Pen_Bulb", M["shade"], 0, 0, z - 0.24, 0.12, 0.12, 0.12)
    elif L["panels"]:
        for i in range(3):
            a = i * 2 * math.pi / 3
            rod(P, "Pen_Arm%d" % i, M["iron"], (0, 0, z), (math.cos(a) * 0.32, math.sin(a) * 0.32, z - 0.15), 0.021)
            cylz(P, "Pen_Shade%d" % i, M["shade"], math.cos(a) * 0.32, math.sin(a) * 0.32, z - 0.3, 0.1, 0.16, r_top=0.06, bev=0.01)
        ball(P, "Pen_Hub", L["accent_wood"], 0, 0, z, 0.12, 0.12, 0.12)
    else:
        cylz(P, "Pen_Shade", M["shade"], 0, 0, z - 0.25, 0.28, 0.25, r_top=0.26, bev=0.012)
    lamp_light(P, "Pen_Light", 0, 0, z - 0.3, 70.0)


# ---- study --------------------------------------------------------------------------------------------------------
def s_desk(P, L, rng):
    w, d, h = 1.35, 0.68, 0.75
    k = L["key"]
    if k == "oldtown":                                       # a pedestal desk with a cloth top inlay
        for s in (-1, 1):
            x = s * (w / 2 - 0.22)
            rbox(P, "Desk_Ped%d" % s, L["wood"], x, 0, (h - L["top"]) / 2 + 0.03, 0.42, d - 0.04, h - L["top"] - 0.06, L["r"])
            rbox(P, "Desk_PedFoot%d" % s, L["wood_dark"], x, 0, 0.025, 0.44, d - 0.02, 0.05, 0.012)
            drawers(P, "Desk%d" % s, L, x, 0.38, d - 0.04, 0.06, h - L["top"], 3)
        top(P, "Desk_Top", L, w, d, h - L["top"])
        rbox(P, "Desk_Inlay", L["accent"], 0, 0, h + 0.003, w - 0.16, d - 0.16, 0.006, 0.004, 1)
    elif k == "curve":
        legs4(P, "Desk", L, w, d, h - L["top"], 0.08)
        blob(P, "Desk_Top", L["wood"], 0, 0, h - 0.025, w + 0.04, d + 0.04, 0.07, levels=2)
        rbox(P, "Desk_Drw", L["wood"], 0, -0.05, h - 0.1, 0.6, d - 0.2, 0.1, 0.04)
        handle(P, "Desk_Knob", L, 0, -d / 2 + 0.02, h - 0.1)
    else:
        legs4(P, "Desk", L, w, d, h - L["top"], 0.05)
        top(P, "Desk_Top", L, w, d, h - L["top"])
        x = w / 2 - 0.25
        rbox(P, "Desk_Box", L["wood"], x, 0, h - L["top"] - 0.15, 0.42, d - 0.1, 0.3, L["r"])
        drawers(P, "Desk", L, x, 0.4, d - 0.1, h - L["top"] - 0.3, h - L["top"], 2)
        if L["leg"] == "iron":
            rbox(P, "Desk_Shelf", L["wood"], 0, 0.1, 0.14, w - 0.12, d - 0.25, 0.04, 0.01)
    rbox(P, "Desk_Book", M["teal"], -0.35, 0.1, h + 0.02, 0.24, 0.18, 0.04, 0.008, 1)
    rbox(P, "Desk_Paper", M["cream"], 0.0, -0.08, h + 0.006, 0.3, 0.22, 0.008, 0.002, 1)
    cylz(P, "Desk_Cup", M["terra"], 0.22, 0.12, h, 0.045, 0.12, bev=0.008)
    for i in range(3):
        rod(P, "Desk_Pen%d" % i, M[("ochre", "ink", "teal")[i]], (0.22 + (i - 1) * 0.015, 0.12, h + 0.08), (0.22 + (i - 1) * 0.02, 0.12, h + 0.2), 0.012 * 1.7)


def s_chair(P, L, rng):
    seat = 0.47
    if L["leg"] in ("square", "taper", "iron"):              # swivel: five feet on a column
        m = M["iron"] if L["leg"] != "taper" else L["wood"]
        for i in range(5):
            a = i * 2 * math.pi / 5 + math.pi / 2
            rod(P, "Sw_Foot%d" % i, m, (0, 0, 0.08), (math.cos(a) * 0.3, math.sin(a) * 0.3, 0.05), 0.025)
            ball(P, "Sw_Wheel%d" % i, M["dark"], math.cos(a) * 0.3, math.sin(a) * 0.3, 0.035, 0.07, 0.07, 0.07)
        rod(P, "Sw_Col", m, (0, 0, 0.06), (0, 0, seat - 0.06), 0.035)
    else:
        legs4(P, "Sw", L, 0.48, 0.48, seat - 0.05, 0.05)
    cushion(P, "Sw_Seat", L, L["cloth"], 0, 0, seat, 0.5, 0.48, 0.1)
    if L["round"]:
        blob(P, "Sw_Back", L["cloth"], 0, 0.22, seat + 0.32, 0.52, 0.12, 0.5, top=(0.8, 1.0), levels=2)
    elif L["panels"]:                                         # a captain's chair: a low curved back rail on spindles
        rbox(P, "Sw_Back", L["wood"], 0, 0.2, seat + 0.34, 0.56, 0.08, 0.1, L["r"])
        for i in range(5):
            x = -0.22 + i * 0.11
            rod(P, "Sw_Spin%d" % i, L["wood"], (x, 0.2, seat + 0.05), (x, 0.2, seat + 0.3), 0.021)
        for s in (-1, 1):
            rbox(P, "Sw_Arm%d" % s, L["wood"], s * 0.27, 0.0, seat + 0.22, 0.06, 0.42, 0.05, 0.015)
            rod(P, "Sw_ArmPost%d" % s, L["wood"], (s * 0.27, -0.18, seat + 0.03), (s * 0.27, -0.18, seat + 0.2), 0.021)
    else:
        rod(P, "Sw_Spine", M["iron"], (0, 0.24, seat), (0, 0.26, seat + 0.25), 0.025)
        cushion(P, "Sw_Back", L, L["cloth"], 0, 0.26, seat + 0.4, 0.46, 0.08, 0.34)


def s_filing(P, L, rng):
    w, d, h = 0.48, 0.6, 1.32
    m = M["iron"] if L["leg"] == "iron" else (M["enamel"] if L["key"] == "plain" else L["wood"])
    z0 = base(P, "Fil", L, w, d, 0.06) if L["leg"] in ("turned", "taper") else 0.0
    if L["round"]:
        rbox(P, "Fil_Body", m, 0, 0, z0 + (h - z0) / 2, w, d, h - z0, 0.06)
    else:
        rbox(P, "Fil_Body", m, 0, 0, z0 + (h - z0) / 2, w, d, h - z0, L["r"])
    for i in range(4):
        zz = z0 + 0.03 + i * (h - z0 - 0.06) / 4
        hh = (h - z0 - 0.06) / 4
        rbox(P, "Fil_Drw%d" % i, m, 0, -d / 2 - 0.012, zz + hh / 2, w - 0.04, 0.03, hh - 0.025, 0.01, 2)
        rbox(P, "Fil_Label%d" % i, M["cream"], 0, -d / 2 - 0.032, zz + hh * 0.7, 0.14, 0.012, 0.05, 0.004, 1)
        handle(P, "Fil_Pull%d" % i, L, 0, -d / 2 - 0.05, zz + hh * 0.45)
    rbox(P, "Fil_Tray", L["accent_wood"], 0, 0, h + 0.03, 0.34, 0.26, 0.06, 0.012)
    rbox(P, "Fil_Paper", M["cream"], 0, 0, h + 0.065, 0.3, 0.22, 0.02, 0.004, 1)


def s_lamp(P, L, rng):
    m = M["iron"] if L["leg"] == "iron" else L["accent_wood"]
    if L["round"]:
        cylz(P, "Dl_Base", m, 0, 0, 0, 0.12, 0.05, bev=0.02)
        rod(P, "Dl_Stem", m, (0, 0, 0.05), (0, 0, 0.42), 0.022)
        ball(P, "Dl_Shade", M["shade"], 0, 0, 0.5, 0.26, 0.26, 0.2)
        lamp_light(P, "Dl_Light", 0, 0, 0.48, 30.0)
    elif L["panels"]:                                        # a banker's lamp: a long green shade on a turned stem
        cylz(P, "Dl_Base", L["wood"], 0, 0, 0, 0.13, 0.05, r_top=0.11, bev=0.015)
        rod(P, "Dl_Stem", L["accent_wood"], (0, 0, 0.05), (0, 0.04, 0.36), 0.022)
        blob(P, "Dl_Shade", L["accent"], 0, -0.02, 0.4, 0.42, 0.18, 0.12, bottom=(1.0, 0.4), levels=2)
        rbox(P, "Dl_Glow", M["shade"], 0, -0.02, 0.35, 0.34, 0.1, 0.02, 0.008, 1)
        lamp_light(P, "Dl_Light", 0, -0.02, 0.3, 30.0)
    else:
        rbox(P, "Dl_Base", m, 0, 0, 0.025, 0.2, 0.16, 0.05, 0.015)
        rod(P, "Dl_Arm1", m, (0, 0.02, 0.05), (0, 0.08, 0.34), 0.022)
        rod(P, "Dl_Arm2", m, (0, 0.08, 0.34), (0, -0.14, 0.48), 0.022)
        cylz(P, "Dl_Shade", m if L["leg"] == "iron" else L["accent"], 0, -0.18, 0.34, 0.13, 0.14, r_top=0.05, bev=0.01)
        ball(P, "Dl_Bulb", M["shade"], 0, -0.18, 0.35, 0.1, 0.1, 0.06)
        lamp_light(P, "Dl_Light", 0, -0.18, 0.32, 30.0)


def s_pinboard(P, L, rng):
    w, h, z = 1.0, 0.7, 1.45
    frame_rect(P, "Pin", L["wood"] if L["leg"] != "iron" else M["iron"], 0, 0, z, w + 0.1, h + 0.1, 0.06, 0.05)
    rbox(P, "Pin_Cork", M["cork"], 0, 0.01, z, w, 0.03, h, 0.006, 1)
    for i in range(7):
        x = rng.uniform(-w / 2 + 0.12, w / 2 - 0.12)
        zz = z + rng.uniform(-h / 2 + 0.12, h / 2 - 0.12)
        mt = M[rng.choice(("cream", "cream", "ochre", "teal", "terra"))]
        o = rbox(P, "Pin_Note%d" % i, mt, x, -0.012, zz, rng.uniform(0.12, 0.2), 0.008, rng.uniform(0.1, 0.18), 0.003, 1)
        o.rotation_euler = (0, rng.uniform(-0.15, 0.15), 0)
        ball(P, "Pin_Pin%d" % i, M[rng.choice(("terra", "teal", "plum"))], x, -0.025, zz + 0.05, 0.035, 0.03, 0.035)


def s_shelving(P, L, rng):
    w, d, h = 1.0, 0.4, 1.9
    side = M["iron"] if L["leg"] == "iron" else L["wood"]
    for s in (-1, 1):
        if L["leg"] == "iron":
            for yy in (-d / 2 + 0.02, d / 2 - 0.02):
                rod(P, "Shv_P%d%d" % (s, int(yy * 100)), side, (s * w / 2, yy, 0), (s * w / 2, yy, h), 0.021)
        else:
            rbox(P, "Shv_Side%d" % s, side, s * (w / 2 - 0.025), 0, h / 2, 0.05, d, h, L["r"])
    for i, z in enumerate((0.08, 0.5, 0.92, 1.34, h - 0.03)):
        rbox(P, "Shv_Shelf%d" % i, L["wood"], 0, 0, z, w - 0.02, d - 0.02, 0.045, L["r"] * 0.6, 2)
    for i, z in enumerate((0.1, 0.94)):                     # file boxes on two shelves, books on the others
        for j in range(4):
            rbox(P, "Shv_File%d%d" % (i, j), M[("teal", "cream", "terra", "dusty")[j]], -0.33 + j * 0.2, 0, z + 0.16, 0.16, d - 0.08, 0.3, 0.012)
    books(P, "Shv_B1", -w / 2 + 0.06, w / 2 - 0.1, 0.0, 0.52, rng)
    books(P, "Shv_B2", -w / 2 + 0.06, w / 2 - 0.3, 0.0, 1.36, rng)
    rbox(P, "Shv_Box", M["cardboard"], 0.3, 0, 1.47, 0.24, 0.3, 0.22, 0.012)


# ---- children -----------------------------------------------------------------------------------------------------
def c_bunk(P, L, rng):
    w, d = 1.0, 2.05
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * (w / 2), sy * (d / 2)
            if L["leg"] == "iron":
                rod(P, "Bk_Post%d%d" % (sx, sy), m, (x, y, 0), (x, y, 1.75), 0.024)
            else:
                rbox(P, "Bk_Post%d%d" % (sx, sy), m, x, y, 0.875, 0.07, 0.07, 1.75, 0.02 if not L["round"] else 0.03)
            if L["panels"]:
                ball(P, "Bk_Fin%d%d" % (sx, sy), L["wood"], x, y, 1.8, 0.1, 0.1, 0.1)
    for lvl, z in enumerate((0.2, 1.15)):
        for sx in (-1, 1):
            rbox(P, "Bk_Rail%d%d" % (lvl, sx), m, sx * w / 2, 0, z, 0.05, d, 0.12, 0.015)
        rbox(P, "Bk_Mat%d" % lvl, M["cream"], 0, 0, z + 0.1, w - 0.08, d - 0.08, 0.16, 0.05)
        cushion(P, "Bk_Duvet%d" % lvl, L, (L["accent"], L["cloth"])[lvl], 0, -0.2, z + 0.2, w - 0.06, d - 0.55, 0.08)
        cushion(P, "Bk_Pillow%d" % lvl, L, M["cream"], 0, d / 2 - 0.28, z + 0.24, 0.6, 0.32, 0.12)
        for sy in (-1, 1):
            rbox(P, "Bk_End%d%d" % (lvl, sy), m if L["leg"] == "iron" else L["wood"], 0, sy * d / 2, z + 0.25, w, 0.05, 0.32 if not L["round"] else 0.36, 0.02 if not L["round"] else 0.06)
    rbox(P, "Bk_Guard", m, -w / 2, 0.3, 1.45, 0.05, d - 0.7, 0.1, 0.015)
    for i in range(5):                                       # the ladder at the open end
        z = 0.35 + i * 0.27
        rod(P, "Bk_Rung%d" % i, m, (-w / 2 - 0.02, -d / 2 + 0.05, z), (-w / 2 - 0.02, -d / 2 + 0.45, z), 0.022)
    for yy in (-d / 2 + 0.05, -d / 2 + 0.45):
        rod(P, "Bk_LSide%d" % int(yy * 100), m, (-w / 2 - 0.02, yy, 0), (-w / 2 - 0.02, yy, 1.5), 0.024)
    ball(P, "Bk_Bear", M["ochre"], 0.25, 0.7, 1.45, 0.2, 0.16, 0.22)
    ball(P, "Bk_BearHead", M["ochre"], 0.25, 0.7, 1.6, 0.15, 0.13, 0.14)


def c_cot(P, L, rng):
    w, d, h = 0.7, 1.25, 1.0
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    if L["round"]:
        blob(P, "Cot_Shell", L["wood"], 0, 0, 0.55, w + 0.1, d + 0.1, 0.5, bottom=(0.9, 0.9), levels=2)
        blob(P, "Cot_Mat", M["cream"], 0, 0, 0.78, w - 0.06, d - 0.08, 0.1, levels=2)
        for sx in (-1, 1):
            for sy in (-1, 1):
                rod(P, "Cot_Leg%d%d" % (sx, sy), L["wood"], (sx * (w / 2 - 0.08), sy * (d / 2 - 0.12), 0), (sx * (w / 2 - 0.1), sy * (d / 2 - 0.16), 0.36), 0.024)
        cushion(P, "Cot_Blanket", L, L["accent"], 0, -0.15, 0.84, w - 0.14, d * 0.55, 0.06)
        return
    for sx in (-1, 1):
        for sy in (-1, 1):
            rbox(P, "Cot_Post%d%d" % (sx, sy), m, sx * w / 2, sy * d / 2, h / 2, 0.06, 0.06, h, 0.015)
    for z in (0.32, h - 0.03):
        for sx in (-1, 1):
            rbox(P, "Cot_RailL%d%d" % (sx, int(z * 100)), m, sx * w / 2, 0, z, 0.05, d, 0.05, 0.012)
        for sy in (-1, 1):
            rbox(P, "Cot_RailE%d%d" % (sy, int(z * 100)), m, 0, sy * d / 2, z, w, 0.05, 0.05, 0.012)
    for sx in (-1, 1):
        for i in range(7):
            y = -d / 2 + d * (i + 0.5) / 7
            rod(P, "Cot_Bar%d%d" % (sx, i), m, (sx * w / 2, y, 0.32), (sx * w / 2, y, h - 0.03), 0.021)
    for sy in (-1, 1):
        if L["panels"]:
            rbox(P, "Cot_Panel%d" % sy, L["wood"], 0, sy * d / 2, 0.66, w - 0.04, 0.04, 0.66, 0.015)
        else:
            for i in range(3):
                x = -w / 2 + w * (i + 0.5) / 3
                rod(P, "Cot_EBar%d%d" % (sy, i), m, (x, sy * d / 2, 0.32), (x, sy * d / 2, h - 0.03), 0.021)
    rbox(P, "Cot_Mat", M["cream"], 0, 0, 0.42, w - 0.06, d - 0.06, 0.12, 0.04)
    cushion(P, "Cot_Blanket", L, L["accent"], 0, -0.15, 0.5, w - 0.08, d * 0.55, 0.06)


def c_toychest(P, L, rng):
    w, d, h = 0.85, 0.48, 0.5
    paint = L["accent"]
    if L["round"]:
        blob(P, "Toy_Body", paint, 0, 0, h / 2, w, d, h, levels=2)
        blob(P, "Toy_Lid", L["wood"], 0, 0, h, w + 0.04, d + 0.04, 0.12, levels=2)
    else:
        z0 = base(P, "Toy", L, w, d, 0.06) if L["leg"] in ("turned", "iron") else 0.0
        rbox(P, "Toy_Body", paint, 0, 0, z0 + (h - z0) / 2, w, d, h - z0, L["r"])
        rbox(P, "Toy_Lid", L["wood"], 0, 0, h + 0.03, w + 0.04, d + 0.04, 0.06, L["r"])
        if L["straps"]:
            for x in (-0.28, 0.28):
                rbox(P, "Toy_Strap%d" % int(x * 10), M["iron"], x, 0, h / 2, 0.05, d + 0.02, h + 0.04, 0.01, 1)
    for i, (x, mt) in enumerate(((-0.25, "terra"), (0.0, "teal"), (0.25, "ochre"))):
        rbox(P, "Toy_Letter%d" % i, M[mt], x, -d / 2 - 0.012, h * 0.5, 0.14, 0.02, 0.14, 0.02)
    ball(P, "Toy_Ball", M["terra"], w / 2 + 0.2, -0.1, 0.11, 0.22, 0.22, 0.22)


def c_table(P, L, rng):
    tbl(P, "Kt", L, 0.7, 0.7, 0.52)
    for i, (x, y) in enumerate(((-0.52, 0.0), (0.52, 0.0))):
        Q = group("KtStool%d" % i, x, y, 0.0, P)
        stool(Q, "Kt%d" % i, L, h=0.3, s=0.3)
    for i in range(4):
        rbox(P, "Kt_Block%d" % i, M[("terra", "teal", "ochre", "dusty")[i]], -0.1 + (i % 2) * 0.1, 0.05, 0.53 + 0.04 + (i // 2) * 0.08, 0.08, 0.08, 0.08, 0.012)
    rbox(P, "Kt_Paper", M["cream"], 0.15, -0.12, 0.525, 0.24, 0.18, 0.006, 0.002, 1)


def c_horse(P, L, rng):
    m = L["wood"]
    for s in (-1, 1):                                        # rockers: an arc of short rods
        pts = [(-0.55 + i * 0.11, s * 0.17, 0.05 + 0.25 * ((-0.55 + i * 0.11) / 0.55) ** 2) for i in range(11)]
        for i in range(10):
            rod(P, "Hs_Rock%d%d" % (s, i), m, pts[i], pts[i + 1], 0.03)
        for x in (-0.25, 0.25):
            rod(P, "Hs_Leg%d%d" % (s, int(x * 10)), m, (x, s * 0.17, 0.06 + 0.25 * (x / 0.55) ** 2), (x * 0.9, s * 0.09, 0.45), 0.03)
    body_m = L["accent"] if L["key"] != "oldtown" else M["cream"]
    blob(P, "Hs_Body", body_m, 0, 0, 0.5, 0.7, 0.26, 0.26, levels=2)
    blob(P, "Hs_Neck", body_m, 0.32, 0, 0.66, 0.2, 0.16, 0.36, lean=-0.08, levels=2)
    blob(P, "Hs_Head", body_m, 0.43, 0, 0.82, 0.3, 0.14, 0.16, levels=2)
    rbox(P, "Hs_Mane", L["wood_dark"], 0.28, 0, 0.76, 0.18, 0.06, 0.26, 0.025)
    ball(P, "Hs_Eye", M["ink"], 0.47, -0.075, 0.86, 0.04, 0.02, 0.04)
    rbox(P, "Hs_Saddle", L["cloth"], -0.02, 0, 0.63, 0.24, 0.28, 0.05, 0.02)
    rod(P, "Hs_Grip", M["iron"], (0.3, -0.12, 0.74), (0.3, 0.12, 0.74), 0.022)
    rbox(P, "Hs_Tail", L["wood_dark"], -0.38, 0, 0.5, 0.06, 0.06, 0.24, 0.025)


def c_toyshelf(P, L, rng):
    w, d, h = 1.1, 0.36, 0.8
    side = M["iron"] if L["leg"] == "iron" else L["wood"]
    for s in (-1, 0, 1):
        rbox(P, "Ts_Side%d" % s, side, s * (w / 2 - 0.025), 0, h / 2, 0.05, d, h, L["r"])
    for i, z in enumerate((0.03, 0.4, h)):
        rbox(P, "Ts_Shelf%d" % i, L["wood"], 0, 0, z, w, d, 0.045, L["r"] * 0.6, 2)
    for i, x in enumerate((-0.27, 0.27)):                   # cloth bins below, toys above
        rbox(P, "Ts_Bin%d" % i, M[("teal", "terra")[i]] if not L["round"] else M[("dusty", "ochre")[i]], x, -0.01, 0.2, 0.42, d - 0.06, 0.32, 0.03)
    ball(P, "Ts_Ball", M["ochre"], -0.32, 0, 0.53, 0.2, 0.2, 0.2)
    for i in range(3):
        rbox(P, "Ts_Block%d" % i, M[("terra", "teal", "cream")[i]], 0.12 + i * 0.1, 0, 0.47 + (i % 2) * 0.0, 0.09, 0.09, 0.09, 0.012)
    rbox(P, "Ts_Top", M["dusty"], 0.3, 0, 0.6, 0.1, 0.1, 0.2, 0.015)
    ball(P, "Ts_Bear", M["ochre"], -0.25, 0, h + 0.12, 0.2, 0.16, 0.2)
    ball(P, "Ts_BearHead", M["ochre"], -0.25, 0, h + 0.27, 0.15, 0.13, 0.14)
    for s in (-1, 1):
        ball(P, "Ts_BearEar%d" % s, M["ochre"], -0.25 + s * 0.06, 0, h + 0.34, 0.06, 0.04, 0.06)
    cylz(P, "Ts_Drum", M["terra"], 0.25, 0, h + 0.025, 0.12, 0.14, bev=0.015)


# ---- hallway ------------------------------------------------------------------------------------------------------
def h_coatstand(P, L, rng):
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    if L["leg"] == "turned":
        rod(P, "Cs_Pole", m, (0, 0, 0.1), (0, 0, 1.8), 0.035)
        for i, z in enumerate((0.6, 1.2)):
            ball(P, "Cs_Bead%d" % i, m, 0, 0, z, 0.09, 0.09, 0.1)
        ball(P, "Cs_Fin", m, 0, 0, 1.85, 0.12, 0.12, 0.12)
    else:
        rod(P, "Cs_Pole", m, (0, 0, 0.05), (0, 0, 1.8), 0.03 if not L["round"] else 0.035)
    for i in range(4):
        a = i * math.pi / 2 + math.pi / 4
        rod(P, "Cs_Foot%d" % i, m, (0, 0, 0.18), (math.cos(a) * 0.3, math.sin(a) * 0.3, 0.02), 0.025)
        rod(P, "Cs_Hook%d" % i, m, (0, 0, 1.62), (math.cos(a) * 0.17, math.sin(a) * 0.17, 1.72), 0.021)
        ball(P, "Cs_HookEnd%d" % i, m if not L["round"] else L["accent_wood"], math.cos(a) * 0.18, math.sin(a) * 0.18, 1.74, 0.06, 0.06, 0.06)
    cloth_hang(P, "Cs_Coat", L["accent"], 0.0, -0.13, 1.7, 0.42, 0.9, 4)
    rbox(P, "Cs_Collar", L["accent"], 0.0, -0.11, 1.68, 0.3, 0.1, 0.08, 0.03)
    cylz(P, "Cs_HatBrim", M["indigo"] if L["key"] != "workshop" else M["cream"], 0.12, 0.1, 1.8, 0.19, 0.03, bev=0.01)
    cylz(P, "Cs_HatCrown", M["indigo"] if L["key"] != "workshop" else M["cream"], 0.12, 0.1, 1.82, 0.11, 0.12, r_top=0.1, bev=0.02)
    cloth_hang(P, "Cs_Scarf", M["terra"] if L["key"] != "oldtown" else M["olive"], 0.14, 0.12, 1.66, 0.12, 0.6, 2)


def h_shoebench(P, L, rng):
    w, d, h = 1.0, 0.36, 0.48
    if L["round"]:
        for s in (-1, 1):
            rbox(P, "Sh_End%d" % s, L["wood"], s * (w / 2 - 0.04), 0, h / 2, 0.08, d, h, 0.035)
        blob(P, "Sh_Seat", L["cloth"], 0, 0, h, w + 0.02, d + 0.05, 0.12, levels=2)
    else:
        legs4(P, "Sh", L, w, d, h - L["top"], 0.05)
        rbox(P, "Sh_Seat", L["wood"], 0, 0, h - L["top"] / 2, w, d, L["top"], L["r"])
        if L["buttons"]:
            cushion(P, "Sh_Pad", L, L["cloth"], 0, 0, h + 0.035, w - 0.06, d - 0.04, 0.06)
    rbox(P, "Sh_Shelf", L["wood"], 0, 0, 0.12, w - 0.1, d - 0.04, 0.04, 0.01)
    for i in range(3):
        x = -0.32 + i * 0.32
        for s in (-1, 1):
            blob(P, "Sh_Shoe%d%d" % (i, s), M[("dark", "terra", "wood")[i]], x + s * 0.06, -0.02, 0.18, 0.1, 0.26, 0.09, nose=0.02, levels=2)
    blob(P, "Sh_Boot", M["wood"], w / 2 + 0.15, 0, 0.16, 0.12, 0.28, 0.32, levels=2)


def h_console(P, L, rng):
    w, d, h = 1.0, 0.34, 0.8
    if L["round"]:
        rbox(P, "Con_Top", L["wood"], 0, 0, h - 0.03, w, d, 0.06, 0.12)
        for s in (-1, 1):
            rod(P, "Con_Leg%d" % s, L["wood"], (s * (w / 2 - 0.14), -0.06, 0), (s * (w / 2 - 0.16), 0, h - 0.06), 0.03)
            rod(P, "Con_LegB%d" % s, L["wood"], (s * (w / 2 - 0.14), 0.12, 0), (s * (w / 2 - 0.16), 0.06, h - 0.06), 0.03)
    else:
        legs4(P, "Con", L, w, d, h - L["top"], 0.04)
        top(P, "Con_Top", L, w, d, h - L["top"])
        drawers(P, "Con", L, 0, w - 0.2, d, h - L["top"] - 0.14, h - L["top"], 1)
        if L["leg"] == "iron":
            rbox(P, "Con_Shelf", L["wood"], 0, 0, 0.16, w - 0.08, d - 0.06, 0.04, 0.01)
    lamp_small(P, L, -0.3, 0.0, h + (0.0 if L["round"] else 0.0))
    cylz(P, "Con_Bowl", M["ochre"], 0.25, 0, h, 0.06, 0.06, r_top=0.12, bev=0.012)
    rbox(P, "Con_Keys", M["iron"], 0.25, 0, h + 0.05, 0.06, 0.03, 0.02, 0.005, 1)
    rbox(P, "Con_Letters", M["cream"], 0.05, 0.02, h + 0.01, 0.2, 0.12, 0.02, 0.003, 1)


def h_umbrella(P, L, rng):
    if L["leg"] == "iron":
        cylz(P, "Um_Body", M["iron"], 0, 0, 0, 0.14, 0.55, segs=20, bev=0.01)
    elif L["round"]:
        cylz(P, "Um_Body", M["porcelain"], 0, 0, 0, 0.16, 0.55, r_top=0.13, bev=0.03)
    elif L["panels"]:
        cylz(P, "Um_Body", M["teal"], 0, 0, 0, 0.15, 0.55, r_top=0.17, bev=0.02)
        for z in (0.08, 0.47):
            cylz(P, "Um_Band%d" % int(z * 100), L["accent_wood"], 0, 0, z, 0.165, 0.04, r_top=0.17, bev=0.01)
    else:
        rbox(P, "Um_Body", L["wood"], 0, 0, 0.27, 0.28, 0.28, 0.54, L["r"])
    for i, (x, y, mt) in enumerate(((-0.04, -0.03, "terra"), (0.05, 0.02, "indigo"), (0.0, 0.06, "olive"))):
        tx, ty = x * 2, y * 2                                # tip in the stand, a closed canopy, a crook handle
        rod(P, "Um_Shaft%d" % i, M["iron"], (x, y, 0.05), (tx, ty, 0.95), 0.021)
        blob(P, "Um_Fold%d" % i, M[mt], x * 1.6, y * 1.6, 0.66, 0.13, 0.13, 0.5, top=(0.7, 0.7), bottom=(0.25, 0.25), levels=2)
        rod(P, "Um_Hook%d" % i, M["wood"], (tx, ty, 0.95), (tx, ty, 1.04), 0.024)
        rod(P, "Um_HookTop%d" % i, M["wood"], (tx, ty, 1.04), (tx + 0.08, ty, 1.06), 0.024)
        rod(P, "Um_HookEnd%d" % i, M["wood"], (tx + 0.08, ty, 1.06), (tx + 0.1, ty, 0.98), 0.024)


def h_hooks(P, L, rng):
    w, z = 1.1, 1.65
    rbox(P, "Hk_Board", L["wood"] if L["leg"] != "iron" else L["wood"], 0, 0.02, z, w, 0.04, 0.16, L["r"])
    rbox(P, "Hk_Shelf", L["wood"], 0, -0.1, z + 0.14, w + 0.04, 0.24, 0.04, L["r"])
    for s in (-1, 1):
        rbox(P, "Hk_Bracket%d" % s, M["iron"] if L["leg"] == "iron" else L["wood"], s * (w / 2 - 0.1), -0.05, z + 0.07, 0.04, 0.14, 0.12, 0.01, 1)
    for i in range(5):
        x = -w / 2 + 0.15 + i * (w - 0.3) / 4
        if L["handle"] == "bar":
            rod(P, "Hk_Peg%d" % i, M["iron"], (x, 0.0, z - 0.02), (x, -0.1, z + 0.02), 0.021)
        else:
            rod(P, "Hk_Peg%d" % i, L["accent_wood"], (x, 0.0, z - 0.02), (x, -0.08, z), 0.021)
            ball(P, "Hk_PegEnd%d" % i, L["accent_wood"], x, -0.09, z + 0.005, 0.055, 0.04, 0.055)
    cloth_hang(P, "Hk_CoatA", L["accent"], -w / 2 + 0.15, -0.08, z, 0.4, 0.95, 4)
    cloth_hang(P, "Hk_CoatB", M["olive"] if L["key"] != "oldtown" else M["indigo"], 0.0, -0.08, z, 0.36, 0.8, 3)
    rbox(P, "Hk_Bag", M["wood"], w / 2 - 0.15, -0.07, z - 0.3, 0.3, 0.1, 0.26, 0.03)
    rod(P, "Hk_Strap", M["wood"], (w / 2 - 0.15, -0.06, z - 0.02), (w / 2 - 0.15, -0.06, z - 0.17), 0.021)
    cylz(P, "Hk_Hat", M["cream"], 0.3, -0.1, z + 0.16, 0.12, 0.1, r_top=0.1, bev=0.02)
    cylz(P, "Hk_Brim", M["cream"], 0.3, -0.1, z + 0.16, 0.18, 0.025, bev=0.01)


def h_mirror(P, L, rng):
    w, h, z = 0.6, 1.15, 1.35
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    if L["round"]:
        o = blob(P, "Hm_Frame", L["wood"], 0, 0.02, z, w + 0.12, 0.06, h + 0.12, top=(0.6, 1.0), levels=3)
        blob(P, "Hm_Glass", M["mirror"], 0, -0.01, z, w - 0.02, 0.03, h - 0.04, top=(0.6, 1.0), levels=3)
    else:
        frame_rect(P, "Hm", m, 0, 0.02, z, w + 0.12, h + 0.12, 0.08 if L["panels"] else 0.06, 0.06)
        rbox(P, "Hm_Glass", M["mirror"], 0, 0.03, z, w, 0.02, h, 0.006, 1)
        if L["panels"]:
            ball(P, "Hm_Crest", L["wood"], 0, 0.02, z + h / 2 + 0.12, 0.2, 0.06, 0.14)
    rbox(P, "Hm_Shelf", L["wood"], 0, -0.06, z - h / 2 - 0.1, w + 0.06, 0.16, 0.04, L["r"])
    rbox(P, "Hm_Brush", M["wood"], -0.15, -0.06, z - h / 2 - 0.06, 0.16, 0.06, 0.04, 0.015)


# ---- garden -------------------------------------------------------------------------------------------------------
def g_bench(P, L, rng):
    w, d, seat = 1.5, 0.55, 0.45
    m = M["iron"] if L["leg"] in ("iron", "turned") else L["wood"]
    for s in (-1, 1):                                        # cast or wooden ends with arms
        x = s * (w / 2)
        if L["round"]:
            blob(P, "Gb_End%d" % s, L["wood"], x, 0.03, 0.34, 0.08, d + 0.04, 0.68, top=(1.0, 0.85), levels=2)
        else:
            rod(P, "Gb_LegF%d" % s, m, (x, -d / 2 + 0.04, 0), (x, -d / 2 + 0.04, 0.66), 0.025)
            rod(P, "Gb_LegB%d" % s, m, (x, d / 2 - 0.04, 0), (x, d / 2 + 0.02, 0.9), 0.025)
            rbox(P, "Gb_Arm%d" % s, L["wood"], x, 0.0, 0.67, 0.07, d, 0.04, 0.012)
        rod(P, "Gb_Rail%d" % s, m, (x, -d / 2 + 0.04, seat - 0.04), (x, d / 2 - 0.04, seat - 0.04), 0.022)
    for i in range(4):
        rbox(P, "Gb_Slat%d" % i, L["wood"], 0, -d / 2 + 0.08 + i * 0.12, seat, w - 0.06, 0.09, 0.04, 0.012)
    for i in range(3):
        rbox(P, "Gb_Back%d" % i, L["wood"], 0, d / 2 - 0.02 + i * 0.02, seat + 0.18 + i * 0.13, w - 0.06, 0.04, 0.09, 0.012)


def g_bistro(P, L, rng):
    m = M["iron"]
    cylz(P, "Bi_Foot", m, 0, 0, 0, 0.24, 0.04, bev=0.012)
    rod(P, "Bi_Col", m, (0, 0, 0.03), (0, 0, 0.7), 0.024)
    if L["round"] or L["panels"]:
        cylz(P, "Bi_Top", L["wood"] if L["key"] != "oldtown" else M["porcelain"], 0, 0, 0.7, 0.32, 0.045, bev=0.015)
    else:
        rbox(P, "Bi_Top", L["wood"], 0, 0, 0.72, 0.6, 0.6, 0.045, L["r"])
    for i, (x, yaw) in enumerate(((-0.6, -math.pi / 2), (0.6, math.pi / 2))):
        Q = group("BiChair%d" % i, x, 0.0, yaw, P)
        seat = 0.45
        for sx in (-1, 1):
            for sy in (-1, 1):
                rod(Q, "BiC%d_L%d%d" % (i, sx, sy), m, (sx * 0.2, sy * 0.2, 0), (sx * 0.17, sy * 0.17, seat), 0.021)
        if L["round"]:
            cylz(Q, "BiC%d_Seat" % i, L["accent"], 0, 0, seat, 0.22, 0.05, bev=0.02)
        else:
            rbox(Q, "BiC%d_Seat" % i, L["wood"], 0, 0, seat + 0.02, 0.42, 0.42, 0.045, L["r"])
        for sx in (-1, 1):
            rod(Q, "BiC%d_Post%d" % (i, sx), m, (sx * 0.17, 0.17, seat), (sx * 0.18, 0.22, seat + 0.4), 0.021)
        rbox(Q, "BiC%d_Back" % i, L["wood"] if not L["round"] else L["accent"], 0, 0.21, seat + 0.32, 0.4, 0.04, 0.16, 0.02)
    cylz(P, "Bi_Cup", M["cream"], 0.1, 0.05, 0.745, 0.045, 0.08, bev=0.008)
    plant(P, "Bi_Pot", -0.08, -0.05, 0.745, 0.35, rng)


def g_planter(P, L, rng):
    w, d, h = 1.2, 0.42, 0.5
    if L["round"]:
        for i, (x, s) in enumerate(((-0.35, 1.0), (0.3, 0.8))):
            cylz(P, "Pl_Pot%d" % i, M["pot"], x, 0, 0, 0.22 * s, 0.5 * s, r_top=0.27 * s, bev=0.04)
            plant(P, "Pl_Leaves%d" % i, x, 0, 0.45 * s, 1.1 * s, rng, pot=False)
        return
    m = L["wood"] if L["key"] != "plain" else M["pot"]
    rbox(P, "Pl_Box", m, 0, 0, h / 2, w, d, h, L["r"])
    if L["straps"]:
        for x in (-0.4, 0.4):
            rbox(P, "Pl_Strap%d" % int(x * 10), M["iron"], x, 0, h / 2, 0.05, d + 0.02, h - 0.04, 0.01, 1)
    if L["panels"]:
        for x in (-0.3, 0.3):
            rbox(P, "Pl_Panel%d" % int(x * 10), L["wood_dark"], x, -d / 2 - 0.01, h / 2, 0.44, 0.02, h - 0.14, 0.01, 1)
    rbox(P, "Pl_Soil", M["dark"], 0, 0, h - 0.01, w - 0.06, d - 0.06, 0.02, 0.008, 1)
    for i in range(3):
        plant(P, "Pl_Leaves%d" % i, -0.38 + i * 0.38, 0, h - 0.05, 0.9, rng, pot=False)
    for i in range(4):
        ball(P, "Pl_Flower%d" % i, M[("terra", "cream", "plum", "ochre")[i]], -0.45 + i * 0.3, -0.1, h + 0.3, 0.08, 0.08, 0.07)


def g_parasol(P, L, rng):
    cylz(P, "Ps_Base", M["stone"], 0, 0, 0, 0.26, 0.1, bev=0.03)
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    rod(P, "Ps_Pole", m, (0, 0, 0.1), (0, 0, 2.35), 0.025)
    canopy = L["accent"] if L["key"] != "plain" else M["cream"]
    if L["round"]:
        cylz(P, "Ps_Canopy", canopy, 0, 0, 1.98, 1.15, 0.32, r_top=0.12, segs=40, bev=0.06)
        ball(P, "Ps_Crown", canopy, 0, 0, 2.3, 0.3, 0.3, 0.14)
    else:
        cylz(P, "Ps_Canopy", canopy, 0, 0, 1.95, 1.2, 0.35, r_top=0.06, segs=8 if L["key"] != "oldtown" else 12, bev=0.02)
        if L["panels"]:
            cylz(P, "Ps_Fringe", M["cream"], 0, 0, 1.88, 1.21, 0.08, segs=12, bev=0.01)
    ball(P, "Ps_Fin", m, 0, 0, 2.38, 0.08, 0.08, 0.08)


def g_lounger(P, L, rng):
    w, l = 0.68, 1.9
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    if L["key"] == "oldtown":                                # a striped deck chair
        for s in (-1, 1):
            rod(P, "Lg_Front%d" % s, L["wood"], (s * w / 2, -0.5, 0), (s * w / 2, 0.35, 0.95), 0.025)
            rod(P, "Lg_Back%d" % s, L["wood"], (s * w / 2, 0.4, 0), (s * w / 2, -0.25, 0.48), 0.025)
        for z, y in ((0.95, 0.35), (0.4, -0.3)):
            rod(P, "Lg_Cross%d" % int(z * 10), L["wood"], (-w / 2, y, z), (w / 2, y, z), 0.025)
        for i in range(5):                                   # the canvas sling, high at the back, low at the seat
            o = rbox(P, "Lg_Stripe%d" % i, M[("terra", "cream")[i % 2]], -0.24 + i * 0.12, 0.02, 0.64, 0.12, 0.86, 0.03, 0.01, 1)
            o.rotation_euler = (0.74, 0, 0)
        return
    for sx in (-1, 1):
        for sy in (-1, 1):
            leg(P, "Lg_Leg%d%d" % (sx, sy), L, sx * (w / 2 - 0.06), sy * (l / 2 - 0.12), 0.28, m)
    rbox(P, "Lg_Frame", m, 0, 0, 0.3, w, l, 0.05, L["r"])
    if L["round"]:
        blob(P, "Lg_Pad", L["cloth"], 0, -0.25, 0.39, w - 0.02, l * 0.68, 0.14, levels=2)
    else:
        cushion(P, "Lg_Pad", L, L["cloth"], 0, -0.25, 0.38, w - 0.06, l * 0.66, 0.08)
    back = rbox(P, "Lg_BackPad", L["cloth"], 0, 0.62, 0.55, w - 0.06, 0.6, 0.08, 0.03)
    back.rotation_euler = (math.radians(-55), 0, 0)
    rod(P, "Lg_Prop", m, (0, 0.85, 0.32), (0, 0.6, 0.62), 0.022)
    cushion(P, "Lg_Pillow", L, L["accent"], 0, 0.55, 0.72, 0.4, 0.14, 0.2)


def g_plantstand(P, L, rng):
    m = M["iron"] if L["leg"] in ("iron", "turned") else L["wood"]
    for i, (z, x) in enumerate(((0.3, -0.25), (0.62, 0.0), (0.94, 0.25))):
        rbox(P, "Pst_Step%d" % i, L["wood"], x, 0, z, 0.36, 0.34, 0.04, L["r"])
        plant(P, "Pst_Pl%d" % i, x, 0, z + 0.02, 0.6 - i * 0.08, rng)
    for s in (-1, 1):
        rod(P, "Pst_SideF%d" % s, m, (s * 0.45, -0.15, 0), (0.3 * s + 0.12, -0.15, 0.96), 0.022)
        rod(P, "Pst_SideB%d" % s, m, (s * 0.45, 0.15, 0), (0.3 * s + 0.12, 0.15, 0.96), 0.022)
    cylz(P, "Pst_Can", M["teal"] if L["key"] != "workshop" else M["iron"], 0.5, -0.1, 0, 0.1, 0.22, bev=0.02)
    rod(P, "Pst_Spout", M["teal"] if L["key"] != "workshop" else M["iron"], (0.55, -0.1, 0.08), (0.75, -0.1, 0.3), 0.021)


# ---- shop ---------------------------------------------------------------------------------------------------------
def p_counter(P, L, rng):
    w, d, h = 1.8, 0.62, 1.0
    z0 = base(P, "Pc", L, w, d)
    body(P, "Pc", L, w, d, z0, h - L["top"])
    for i in range(3):
        x = -w / 2 + w * (i + 0.5) / 3
        panel_door(P, "Pc_Front%d" % i, L, x, z0 + 0.03, w / 3 - 0.03, h - L["top"] - z0 - 0.06, -d / 2, L["wood"])
    top(P, "Pc_Top", L, w + 0.06, d + 0.06, h - L["top"])
    m = M["iron"] if L["key"] != "curve" else M["enamel"]   # the till: a chunky box with big keys and a drawer
    rbox(P, "Pc_Till", m, 0.4, 0.05, h + 0.13, 0.42, 0.36, 0.26, 0.04)
    rbox(P, "Pc_TillTop", m, 0.4, 0.14, h + 0.3, 0.38, 0.14, 0.14, 0.03)
    rbox(P, "Pc_TillFlag", M["cream"], 0.4, 0.14, h + 0.4, 0.24, 0.03, 0.07, 0.01)
    for i in range(3):
        for j in range(3):
            ball(P, "Pc_Key%d%d" % (i, j), M["keys"], 0.31 + i * 0.09, -0.08 + j * 0.06, h + 0.27, 0.05, 0.04, 0.03)
    rbox(P, "Pc_Scale", M["iron"], -0.45, 0, h + 0.06, 0.3, 0.26, 0.12, 0.03)
    cylz(P, "Pc_Pan", M["ochre"], -0.45, 0, h + 0.12, 0.14, 0.03, r_top=0.16, bev=0.008)
    rbox(P, "Pc_Paper", M["cardboard"], -0.05, 0.1, h + 0.04, 0.3, 0.2, 0.08, 0.01)


def p_gondola(P, L, rng):
    w, d, h = 1.6, 0.8, 1.55
    m = M["iron"] if L["leg"] in ("iron", "square") else L["wood"]
    rbox(P, "Gd_Spine", L["wood_dark"], 0, 0, h / 2, w, 0.06, h, 0.012)
    for s in (-1, 1):
        rbox(P, "Gd_End%d" % s, m, s * (w / 2 + 0.02), 0, h / 2, 0.05, d, h, L["r"])
    for i, z in enumerate((0.08, 0.5, 0.92, 1.3)):
        for sy in (-1, 1):
            rbox(P, "Gd_Sh%d%d" % (i, sy), L["wood"], 0, sy * d / 4, z, w, d / 2 - 0.04, 0.04, 0.01)
            rbox(P, "Gd_Strip%d%d" % (i, sy), L["accent"], 0, sy * (d / 2 - 0.03), z, w, 0.012, 0.045, 0.004, 1)
            if i < 3:
                x = -w / 2 + 0.06
                while x < w / 2 - 0.15:
                    bw = rng.uniform(0.1, 0.18)
                    bh = rng.uniform(0.12, 0.3)
                    if rng.random() < 0.5:
                        rbox(P, "Gd_Item%d%d%d" % (i, sy, int(x * 100)), M[rng.choice(("teal", "terra", "cream", "ochre", "dusty", "olive", "cardboard"))],
                             x + bw / 2, sy * d / 4, z + 0.02 + bh / 2, bw, d / 2 - 0.12, bh, 0.012, 1)
                    else:
                        cylz(P, "Gd_Item%d%d%d" % (i, sy, int(x * 100)), M[rng.choice(("teal", "terra", "cream", "ochre", "plum"))],
                             x + bw / 2, sy * d / 4, z + 0.02, bw / 2, bh, bev=0.01)
                    x += bw + 0.02
    rbox(P, "Gd_Sign", M["cream"], 0, 0, h + 0.15, 0.6, 0.04, 0.22, 0.02)
    rbox(P, "Gd_SignBand", L["accent"], 0, -0.025, h + 0.15, 0.6, 0.012, 0.06, 0.004, 1)


def p_rail(P, L, rng):
    w, h = 1.4, 1.5
    m = M["iron"] if L["leg"] != "turned" else L["wood"]
    for s in (-1, 1):
        if L["leg"] == "turned":
            leg(P, "Cr_Post%d" % s, L, s * w / 2, 0, h)
        else:
            rod(P, "Cr_Post%d" % s, m, (s * w / 2, 0, 0.05), (s * w / 2, 0, h), 0.024)
        rbox(P, "Cr_Foot%d" % s, m, s * w / 2, 0, 0.03, 0.08, 0.5, 0.05, 0.015)
        if L["round"]:
            ball(P, "Cr_Wheel%d" % s, M["dark"], s * w / 2, 0.0, 0.03, 0.06, 0.06, 0.06)
    rod(P, "Cr_Bar", m, (-w / 2, 0, h), (w / 2, 0, h), 0.022)
    rbox(P, "Cr_Base", L["wood"], 0, 0, 0.2, w - 0.06, 0.4, 0.04, 0.01)
    for i in range(7):
        x = -w / 2 + 0.16 + i * (w - 0.32) / 6
        mt = M[("teal", "terra", "cream", "olive", "dusty", "plum", "ochre")[i]]
        rod(P, "Cr_Hanger%d" % i, M["wood"], (x - 0.15, 0, h - 0.08), (x + 0.15, 0, h - 0.08), 0.021)
        blob(P, "Cr_Garment%d" % i, mt, x, 0, h - 0.48, 0.12, 0.46, 0.8, bottom=(1.0, 1.0), top=(1.0, 0.6), levels=2)
    rbox(P, "Cr_Box", M["cardboard"], 0.3, 0, 0.32, 0.4, 0.3, 0.2, 0.012)
    blob(P, "Cr_Shoes", M["terra"], -0.3, 0, 0.27, 0.22, 0.26, 0.1, levels=2)


def p_table(P, L, rng):
    tbl(P, "Pt", L, 1.2, 0.8, 0.78, round_=False if not L["round"] else True)
    for i in range(3):                                       # folded stacks of cloth
        x = -0.35 + i * 0.35
        for j in range(4):
            rbox(P, "Pt_Fold%d%d" % (i, j), M[("teal", "cream", "terra")[i]] if j % 2 == 0 else M[("dusty", "ochre", "cream")[i]],
                 x, 0.0, 0.8 + j * 0.05, 0.28, 0.24, 0.045, 0.015)
    rbox(P, "Pt_Card", M["cream"], 0, -0.3, 0.83, 0.16, 0.02, 0.1, 0.006, 1)


def p_aboard(P, L, rng):
    """An A-board: two chalkboards leaning together at the top."""
    h = 1.0
    m = L["wood"] if L["leg"] != "iron" else M["iron"]
    t = 0.18
    for s in (-1, 1):
        o = rbox(P, "Ab_Side%d" % s, m, 0, s * 0.1, h / 2, 0.62, 0.05, h, L["r"])
        o.rotation_euler = (s * t, 0, 0)
        o2 = rbox(P, "Ab_Board%d" % s, M["chalk"], 0, s * 0.128, h / 2 + 0.03, 0.5, 0.012, h - 0.2, 0.004, 1)
        o2.rotation_euler = (s * t, 0, 0)
    for i, (z, ww) in enumerate(((0.75, 0.34), (0.6, 0.26), (0.45, 0.3), (0.3, 0.2))):
        o = rbox(P, "Ab_Chalk%d" % i, M["cream"] if i != 1 else M["ochre"], 0, -0.137 + (z - h / 2) * math.tan(t), z, ww, 0.008, 0.035, 0.004, 1)
        o.rotation_euler = (-t, 0, 0)
    if L["round"]:
        rod(P, "Ab_Top", L["wood"], (-0.31, 0, h - 0.02), (0.31, 0, h - 0.02), 0.04)


def p_form(P, L, rng):
    m = M["iron"] if L["leg"] in ("iron", "square") else L["wood"]
    if L["leg"] in ("turned", "taper"):
        for i in range(3):
            a = i * 2 * math.pi / 3
            rod(P, "Df_Leg%d" % i, m, (math.cos(a) * 0.28, math.sin(a) * 0.28, 0), (0, 0, 0.42), 0.024)
    else:
        cylz(P, "Df_Base", m, 0, 0, 0, 0.22, 0.04, bev=0.012)
    rod(P, "Df_Pole", m, (0, 0, 0.04), (0, 0, 1.0), 0.024)
    form = L["cloth"] if L["key"] != "plain" else M["cream"]
    blob(P, "Df_Body", form, 0, 0, 1.3, 0.38, 0.26, 0.6, top=(0.82, 0.82), bottom=(0.78, 0.82), levels=3)
    ball(P, "Df_Knob", L["accent_wood"], 0, 0, 1.64, 0.1, 0.1, 0.08)
    cloth_hang(P, "Df_Dress", L["accent"], 0, -0.13, 1.2, 0.36, 0.5, 3)
    rbox(P, "Df_Tape", M["ochre"], 0.15, -0.1, 1.42, 0.03, 0.02, 0.5, 0.006, 1).rotation_euler = (0, 0.5, 0)


# ---- café ---------------------------------------------------------------------------------------------------------
def f_counter(P, L, rng):
    w, d, h = 1.8, 0.65, 1.05
    if L["round"]:
        blob(P, "Fc_Body", L["wood"], 0, 0, (h - 0.05) / 2, w + 0.1, d + 0.1, h - 0.05, bottom=(0.95, 0.9), levels=2)
        blob(P, "Fc_Top", L["wood_dark"], 0, 0, h - 0.03, w + 0.14, d + 0.12, 0.08, levels=2)
    else:
        z0 = base(P, "Fc", L, w, d)
        body(P, "Fc", L, w, d, z0, h - L["top"])
        for i in range(4):
            x = -w / 2 + w * (i + 0.5) / 4
            panel_door(P, "Fc_Panel%d" % i, L, x, z0 + 0.03, w / 4 - 0.03, h - L["top"] - z0 - 0.06, -d / 2, L["wood"])
        top(P, "Fc_Top", L, w + 0.06, d + 0.06, h - L["top"])
        rod(P, "Fc_FootRail", M["iron"], (-w / 2, -d / 2 - 0.12, 0.2), (w / 2, -d / 2 - 0.12, 0.2), 0.025)
    for i, x in enumerate((-0.6, -0.35)):                   # a cake under a cloche, a pile of cups
        cylz(P, "Fc_Stand%d" % i, M["porcelain"], x, 0, h, 0.12, 0.08, r_top=0.04, bev=0.01)
    cylz(P, "Fc_Cake", M["cream"], -0.6, 0, h + 0.08, 0.11, 0.09, bev=0.015)
    cylz(P, "Fc_Icing", M["terra"], -0.6, 0, h + 0.17, 0.112, 0.02, bev=0.008)
    for i in range(4):
        cylz(P, "Fc_Cup%d" % i, M["cream"], 0.1, 0.05, h + i * 0.07, 0.05, 0.07, r_top=0.055, bev=0.008)
    cylz(P, "Fc_Tip", M["iron"], 0.55, 0.0, h, 0.08, 0.1, bev=0.01)


def f_machine(P, L, rng):
    w, d, h = 0.6, 0.48, 0.55
    m = M["enamel"] if L["key"] in ("plain", "curve") else M["iron"]
    rbox(P, "Cm_Bench", L["wood"], 0, 0, 0.45, 0.8, 0.55, 0.9, L["r"])
    z = 0.9
    if L["round"]:
        blob(P, "Cm_Body", m, 0, 0, z + h / 2, w + 0.06, d + 0.04, h + 0.08, top=(0.85, 0.9), levels=2)
    elif L["panels"]:                                        # an upright boiler with a dome
        cylz(P, "Cm_Boiler", M["iron"], 0, 0.06, z, 0.2, h + 0.1, bev=0.02)
        ball(P, "Cm_Dome", L["accent_wood"], 0, 0.06, z + h + 0.1, 0.38, 0.38, 0.22)
        ball(P, "Cm_Eagle", L["accent_wood"], 0, 0.06, z + h + 0.24, 0.08, 0.08, 0.1)
    else:
        rbox(P, "Cm_Body", m, 0, 0, z + h / 2, w, d, h, L["r"] * 2)
    for i, x in enumerate((-0.14, 0.14)):
        rbox(P, "Cm_Head%d" % i, M["iron"], x, -d / 2 - 0.02, z + 0.3, 0.09, 0.12, 0.08, 0.02)
        rod(P, "Cm_Handle%d" % i, M["wood"], (x, -d / 2 - 0.06, z + 0.28), (x, -d / 2 - 0.22, z + 0.26), 0.022)
        cylz(P, "Cm_Cup%d" % i, M["cream"], x, -d / 2 - 0.02, z + 0.02, 0.045, 0.07, bev=0.008)
    rbox(P, "Cm_Tray", M["iron"], 0, -d / 2 - 0.02, z + 0.01, w - 0.08, 0.14, 0.02, 0.008, 1)
    ball(P, "Cm_Gauge", M["porcelain"], 0, -d / 2 - 0.01, z + h - 0.12, 0.1, 0.04, 0.1)
    rod(P, "Cm_Wand", M["iron"], (w / 2 - 0.02, -d / 2 + 0.05, z + 0.42), (w / 2 + 0.06, -d / 2 - 0.05, z + 0.18), 0.021)
    cylz(P, "Cm_Beans", M["wood"], 0.32, 0.12, z, 0.07, 0.16, r_top=0.09, bev=0.012)


def f_banquette(P, L, rng):
    w, d, seat = 1.8, 0.6, 0.46
    if L["leg"] == "iron":
        rbox(P, "Bq_Frame", M["iron"], 0, 0, 0.12, w, d, 0.05, 0.015)
        legs4(P, "Bq", L, w, d, 0.12, 0.04)
        rbox(P, "Bq_Deck", L["wood"], 0, 0, 0.2, w - 0.04, d - 0.04, 0.12, L["r"])
    else:
        rbox(P, "Bq_Base", L["wood"], 0, 0, 0.16, w, d, 0.32, L["r"] if not L["round"] else 0.06)
    n = 3
    cw = w / n
    for i in range(n):
        cx = -w / 2 + cw * (i + 0.5)
        cushion(P, "Bq_Seat%d" % i, L, L["cloth"], cx, -0.04, seat - 0.06, cw - 0.02, d - 0.12, 0.14)
        if L["buttons"]:
            cushion(P, "Bq_Back%d" % i, L, L["cloth"], cx, d / 2 - 0.08, seat + 0.32, cw - 0.03, 0.14, 0.48)
            ball(P, "Bq_Btn%d" % i, L["cloth_dark"], cx, d / 2 - 0.16, seat + 0.36, 0.04, 0.03, 0.04)
        else:
            cushion(P, "Bq_Back%d" % i, L, L["cloth"], cx, d / 2 - 0.08, seat + 0.3, cw - 0.03, 0.14, 0.42)
    rbox(P, "Bq_BackBoard", L["wood"], 0, d / 2 + 0.02, seat + 0.3, w, 0.06, 0.7, L["r"])
    if L["panels"]:
        rbox(P, "Bq_Cap", L["wood"], 0, d / 2 + 0.02, seat + 0.67, w + 0.04, 0.1, 0.05, 0.015)


def f_table(P, L, rng):
    h = 0.74
    if L["round"] or L["panels"]:
        cylz(P, "Ct_Foot", M["iron"], 0, 0, 0, 0.26, 0.05, bev=0.015)
        rod(P, "Ct_Col", M["iron"], (0, 0, 0.04), (0, 0, h - 0.04), 0.03)
        cylz(P, "Ct_Top", M["porcelain"] if L["panels"] else L["wood"], 0, 0, h - 0.045, 0.34, 0.045, bev=0.015)
    else:
        tbl(P, "Ct", L, 0.7, 0.7, h, round_=False)
    for i, (x, y) in enumerate(((-0.12, -0.1), (0.13, 0.1))):
        cylz(P, "Ct_Saucer%d" % i, M["cream"], x, y, h, 0.08, 0.015, bev=0.005)
        cylz(P, "Ct_Cup%d" % i, M["cream"] if i else M["teal"], x, y, h + 0.015, 0.045, 0.07, bev=0.008)
    cylz(P, "Ct_Plate", M["cream"], 0.1, -0.15, h, 0.1, 0.015, bev=0.005)
    rbox(P, "Ct_Cake", M["ochre"], 0.1, -0.15, h + 0.04, 0.08, 0.06, 0.05, 0.012)
    cylz(P, "Ct_Vase", M["porcelain"], -0.05, 0.12, h, 0.03, 0.1, bev=0.008)
    ball(P, "Ct_Flower", M["terra"], -0.05, 0.12, h + 0.14, 0.06, 0.06, 0.06)


def f_menu(P, L, rng):
    w, h, z = 1.1, 0.8, 1.6
    frame_rect(P, "Mn", L["wood"] if L["leg"] != "iron" else M["iron"], 0, 0, z, w + 0.1, h + 0.1, 0.07 if L["panels"] else 0.05, 0.05)
    rbox(P, "Mn_Board", M["chalk"], 0, 0.01, z, w, 0.03, h, 0.006, 1)
    rbox(P, "Mn_Title", M["cream"], 0, -0.012, z + h / 2 - 0.12, 0.5, 0.008, 0.07, 0.004, 1)
    for i in range(5):
        zz = z + h / 2 - 0.26 - i * 0.1
        rbox(P, "Mn_Line%d" % i, M["cream"], -0.12, -0.012, zz, rng.uniform(0.35, 0.55), 0.008, 0.035, 0.003, 1)
        rbox(P, "Mn_Price%d" % i, M["ochre"], 0.38, -0.012, zz, 0.12, 0.008, 0.035, 0.003, 1)
    if L["round"]:
        ball(P, "Mn_Cup", M["cream"], -0.36, -0.015, z - h / 2 + 0.12, 0.12, 0.01, 0.1)


def f_cakestand(P, L, rng):
    tbl(P, "Ck", L, 0.6, 0.6, 0.9, round_=True)
    for i, (z, r) in enumerate(((0.9, 0.2), (1.12, 0.15), (1.3, 0.1))):
        cylz(P, "Ck_Tier%d" % i, M["porcelain"], 0, 0, z, r, 0.025, bev=0.008)
        if i < 2:
            rod(P, "Ck_Stem%d" % i, M["iron"] if L["key"] != "curve" else L["accent_wood"], (0, 0, z), (0, 0, z + 0.22 - i * 0.04), 0.021)
        for j in range(4 - i):
            a = j * 2 * math.pi / (4 - i)
            ball(P, "Ck_Bun%d%d" % (i, j), M[("ochre", "terra", "cream", "plum")[j % 4]], math.cos(a) * r * 0.6, math.sin(a) * r * 0.6, z + 0.06, 0.08, 0.08, 0.06)
    ball(P, "Ck_Top", M["terra"], 0, 0, 1.38, 0.07, 0.07, 0.07)


# ---- bedroom ------------------------------------------------------------------------------------------------------
def r_chest(P, L, rng):
    w, d, h = 0.95, 0.5, 1.05
    z0 = base(P, "Ch", L, w, d)
    if L["round"]:
        blob(P, "Ch_Body", L["wood"], 0, 0, z0 + (h - z0) / 2, w + 0.06, d + 0.04, h - z0 + 0.06, levels=2)
        for i in range(4):
            zz = z0 + 0.1 + i * (h - z0 - 0.15) / 4
            rbox(P, "Ch_Groove%d" % i, L["wood_dark"], 0, -d / 2 + 0.01, zz, w - 0.1, 0.05, 0.015, 0.006, 1)
            handle(P, "Ch_Knob%d" % i, L, 0, -d / 2 - 0.02, zz + 0.1)
    else:
        body(P, "Ch", L, w, d, z0, h)
        for s in (-1, 1):
            drawers(P, "ChT%d" % s, L, s * w / 4, w / 2 - 0.02, d, h - 0.2, h - 0.02, 1)
        drawers(P, "Ch", L, 0, w - 0.04, d, z0 + 0.02, h - 0.22, 3)
    lift = 0.06 if L["panels"] else 0.0
    cylz(P, "Ch_Box", M["plum"], -0.25, 0, h + lift, 0.08, 0.07, bev=0.015)
    rbox(P, "Ch_Frame", L["accent_wood"], 0.2, 0.12, h + lift + 0.14, 0.22, 0.04, 0.28, 0.012).rotation_euler = (-0.2, 0, 0)
    rbox(P, "Ch_Photo", M["dusty"], 0.2, 0.1, h + lift + 0.14, 0.15, 0.01, 0.2, 0.004, 1).rotation_euler = (-0.2, 0, 0)


def r_vanity(P, L, rng):
    w, d, h = 1.1, 0.48, 0.75
    if not L["round"]:
        tbl(P, "Va", L, w, d, h, round_=False)
    else:
        legs4(P, "Va", L, w, d, h - 0.05, 0.08)
        blob(P, "Va_Top", L["wood"], 0, 0, h - 0.02, w + 0.06, d + 0.04, 0.08, levels=2)
    for s in (-1, 1):
        rbox(P, "Va_Drw%d" % s, L["wood"], s * (w / 2 - 0.2), 0, h - 0.12, 0.34, d - 0.04, 0.14, L["r"])
        handle(P, "Va_Knob%d" % s, L, s * (w / 2 - 0.2), -d / 2 - 0.03, h - 0.12)
    mz = h + 0.42
    if L["round"]:
        blob(P, "Va_MFrame", L["wood"], 0, d / 2 - 0.06, mz, 0.72, 0.05, 0.72, levels=3)
        blob(P, "Va_Glass", M["mirror"], 0, d / 2 - 0.09, mz, 0.6, 0.02, 0.6, levels=3)
    else:
        frame_rect(P, "Va_M", L["wood"] if L["leg"] != "iron" else M["iron"], 0, d / 2 - 0.06, mz, 0.66, 0.74, 0.06, 0.05)
        rbox(P, "Va_Glass", M["mirror"], 0, d / 2 - 0.05, mz, 0.54, 0.02, 0.62, 0.006, 1)
        if L["panels"]:                                       # side wings
            for s in (-1, 1):
                rbox(P, "Va_Wing%d" % s, L["wood"], s * 0.47, d / 2 - 0.1, mz, 0.26, 0.04, 0.56, 0.012).rotation_euler = (0, 0, -s * 0.35)
    for i, (x, mt) in enumerate(((-0.3, "terra"), (-0.2, "cream"), (0.28, "plum"))):
        cylz(P, "Va_Bottle%d" % i, M[mt], x, -0.04, h, 0.04, 0.12 - i * 0.02, r_top=0.025, bev=0.01)
    rbox(P, "Va_Brush", M["wood"], 0.12, -0.1, h + 0.02, 0.2, 0.06, 0.03, 0.01)
    Q = group("VaStool", 0, -0.55, 0.0, P)
    stool(Q, "VaS", L, h=0.45, s=0.4)


def r_single(P, L, rng):
    w, d = 0.95, 2.05
    h0 = 0.18
    if L["leg"] == "iron":
        rbox(P, "Sg_Frame", M["iron"], 0, 0, h0 + 0.02, w, d, 0.05, 0.015, 2)
        legs4(P, "Sg", L, w, d, h0, 0.04)
    else:
        legs4(P, "Sg", L, w, d, h0, 0.06)
        rbox(P, "Sg_Frame", L["wood"], 0, 0, h0 + 0.08, w, d, 0.16, L["r"])
    rbox(P, "Sg_Mattress", M["cream"], 0, 0, h0 + 0.24, w - 0.06, d - 0.06, 0.2, 0.06)
    cushion(P, "Sg_Duvet", L, L["cloth"], 0, -0.22, h0 + 0.36, w - 0.02, d - 0.5, 0.1)
    cushion(P, "Sg_Pillow", L, M["cream"], 0, d / 2 - 0.3, h0 + 0.4, 0.62, 0.34, 0.14)
    hy = d / 2 + 0.03
    k = L["head"]
    if k == "arch":
        blob(P, "Sg_Head", L["accent"], 0, hy, 0.72, w + 0.06, 0.12, 1.0, top=(0.7, 1.0), levels=3)
    elif k == "panel":
        for s in (-1, 1):
            rod(P, "Sg_HPost%d" % s, L["wood"], (s * (w / 2), hy, 0), (s * (w / 2), hy, 1.1), 0.035)
            ball(P, "Sg_HFin%d" % s, L["wood"], s * (w / 2), hy, 1.13, 0.1, 0.1, 0.1)
            rod(P, "Sg_FPost%d" % s, L["wood"], (s * (w / 2), -d / 2, 0), (s * (w / 2), -d / 2, 0.75), 0.035)
        rbox(P, "Sg_Head", L["wood"], 0, hy, 0.78, w - 0.04, 0.06, 0.56, L["r"])
        rbox(P, "Sg_Foot", L["wood"], 0, -d / 2, 0.55, w - 0.04, 0.06, 0.3, L["r"])
    elif k == "rails":
        for s in (-1, 1):
            rod(P, "Sg_HPost%d" % s, M["iron"], (s * (w / 2), hy, 0), (s * (w / 2), hy, 1.0), 0.024)
        for z in (0.6, 0.96):
            rod(P, "Sg_HRail%d" % int(z * 10), M["iron"], (-w / 2, hy, z), (w / 2, hy, z), 0.022)
        for i in range(4):
            x = -w / 2 + w * (i + 0.5) / 4
            rod(P, "Sg_HBar%d" % i, M["iron"], (x, hy, 0.6), (x, hy, 0.96), 0.021)
    else:
        rbox(P, "Sg_Head", L["wood"], 0, hy, 0.6, w + 0.04, 0.07, 0.86, L["r"])


def r_blanket(P, L, rng):
    w, d, h = 1.1, 0.45, 0.46
    if L["round"]:
        blob(P, "Bb_Body", L["cloth"], 0, 0, h / 2, w + 0.06, d + 0.06, h + 0.04, levels=2)
        legs4(P, "Bb", L, w - 0.1, d - 0.06, 0.06, 0.05)
        return
    if L["buttons"]:                                         # an upholstered ottoman
        legs4(P, "Bb", L, w, d, 0.1, 0.05)
        rbox(P, "Bb_Body", L["cloth"], 0, 0, 0.1 + (h - 0.1) / 2, w, d, h - 0.1, L["rc"])
        for i in range(3):
            ball(P, "Bb_Btn%d" % i, L["cloth_dark"], -0.35 + i * 0.35, 0, h + 0.005, 0.045, 0.045, 0.03)
    else:
        z0 = base(P, "Bb", L, w, d, 0.06) if L["leg"] == "iron" else 0.0
        rbox(P, "Bb_Body", L["wood"], 0, 0, z0 + (h - z0) / 2, w, d, h - z0, L["r"])
        rbox(P, "Bb_Lid", L["wood"], 0, 0, h + 0.025, w + 0.03, d + 0.03, 0.05, L["r"])
        if L["straps"]:
            for x in (-0.35, 0.35):
                rbox(P, "Bb_Strap%d" % int(x * 10), M["iron"], x, 0, h / 2, 0.05, d + 0.02, h, 0.01, 1)
        handle(P, "Bb_Handle", L, 0, -d / 2 - 0.04, h - 0.1)
    cushion(P, "Bb_Throw", L, L["accent"], 0.2, 0, h + 0.07, 0.5, d - 0.04, 0.07)


def r_screen(P, L, rng):
    pw, h = 0.5, 1.7
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    for i in range(3):
        ang = (-0.45, 0.0, 0.45)[i]                          # outer panels turn about the hinges at +-pw/2
        x = (i - 1) * (pw / 2 + pw / 2 * math.cos(0.45))
        y = abs(i - 1) * pw / 2 * math.sin(0.45)
        Q = group("ScrP%d" % i, x, y, ang, P)
        for s in (-1, 1):
            rbox(Q, "Scr%d_Post%d" % (i, s), m, s * (pw / 2 - 0.025), 0, h / 2 + 0.04, 0.05, 0.05, h, 0.012)
        if L["round"]:
            blob(Q, "Scr%d_Panel" % i, L["cloth"], 0, 0, h / 2 + 0.1, pw - 0.04, 0.04, h - 0.1, top=(0.75, 1.0), levels=2)
        else:
            rbox(Q, "Scr%d_Panel" % i, L["cloth"] if i != 1 else L["accent"], 0, 0, h / 2 + 0.1, pw - 0.08, 0.025, h - 0.25, 0.008, 2)
            rbox(Q, "Scr%d_Top" % i, m, 0, 0, h + 0.03, pw, 0.05, 0.05, 0.012)
            rbox(Q, "Scr%d_Bot" % i, m, 0, 0, 0.2, pw, 0.05, 0.05, 0.012)
            if L["panels"]:
                ball(Q, "Scr%d_Fin" % i, L["wood"], 0, 0, h + 0.1, 0.1, 0.06, 0.1)
    cloth_hang(P, "Scr_Robe", L["accent"] if L["key"] != "curve" else M["cream"], pw * 0.94, -0.28, h + 0.05, 0.3, 0.7, 2)


def r_cheval(P, L, rng):
    w, h = 0.55, 1.4
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    for s in (-1, 1):
        x = s * (w / 2 + 0.08)
        if L["leg"] == "turned":
            leg(P, "Cv_Post%d" % s, L, x, 0, 1.6)
        else:
            rod(P, "Cv_Post%d" % s, m, (x, 0, 0.05), (x, 0, 1.6), 0.025)
        rbox(P, "Cv_Foot%d" % s, m, x, 0, 0.03, 0.07, 0.5, 0.06, 0.02)
    Q = group("CvGlass", 0, 0, 0.0, P)
    if L["round"]:
        blob(Q, "Cv_Frame", L["wood"], 0, 0, 0.9, w + 0.08, 0.06, h + 0.06, top=(0.65, 1.0), bottom=(0.65, 1.0), levels=3)
        blob(Q, "Cv_Glass", M["mirror"], 0, -0.025, 0.9, w - 0.04, 0.03, h - 0.06, top=(0.65, 1.0), bottom=(0.65, 1.0), levels=3)
    else:
        frame_rect(Q, "Cv", m, 0, 0, 0.9, w + 0.06, h + 0.06, 0.06, 0.05)
        rbox(Q, "Cv_Glass", M["mirror"], 0, 0.005, 0.9, w - 0.06, 0.02, h - 0.06, 0.006, 1)
    Q.rotation_euler = (0.08, 0, 0)
    for s in (-1, 1):
        ball(P, "Cv_Pivot%d" % s, L["accent_wood"], s * (w / 2 + 0.05), 0, 1.0, 0.07, 0.07, 0.07)


# ---- living -------------------------------------------------------------------------------------------------------
def l_fireplace(P, L, rng):
    w, d, h = 1.5, 0.4, 1.15
    k = L["key"]
    if k == "workshop":                                      # a cast iron stove on a stone hearth
        rbox(P, "Fp_Hearth", M["stone_dark"], 0, -0.1, 0.04, 1.1, 0.8, 0.08, 0.02)
        rbox(P, "Fp_Stove", M["iron"], 0, 0, 0.45, 0.6, 0.5, 0.65, 0.04)
        for sx in (-1, 1):
            for sy in (-1, 1):
                rbox(P, "Fp_Ft%d%d" % (sx, sy), M["iron"], sx * 0.24, sy * 0.18, 0.1, 0.06, 0.06, 0.08, 0.015)
        rbox(P, "Fp_Door", M["dark"], 0, -0.26, 0.45, 0.4, 0.02, 0.34, 0.03)
        rbox(P, "Fp_Glow", M["ember"], 0, -0.272, 0.4, 0.3, 0.01, 0.18, 0.02)
        rod(P, "Fp_Flue", M["iron"], (0, 0.05, 0.78), (0, 0.05, 2.6), 0.08)
        rbox(P, "Fp_Logs", L["wood"], 0.5, -0.1, 0.18, 0.3, 0.3, 0.28, 0.04)
        lamp_light(P, "Fp_Light", 0, -0.45, 0.45, 25.0)
        return
    surround = M["stone"] if k == "oldtown" else (M["plaster_b"] if k == "curve" else L["wood"])
    rbox(P, "Fp_Hearth", M["stone_dark"], 0, -0.15, 0.04, w + 0.1, d + 0.35, 0.08, 0.02)
    if k == "curve":
        blob(P, "Fp_Body", surround, 0, 0, h / 2 + 0.05, w + 0.06, d, h + 0.1, top=(0.7, 1.0), levels=3)
        blob(P, "Fp_Hole", M["dark"], 0, -d / 2 + 0.06, 0.42, 0.7, 0.14, 0.7, top=(0.55, 1.0), levels=3)
    else:
        for s in (-1, 1):
            rbox(P, "Fp_Jamb%d" % s, surround, s * (w / 2 - 0.2), 0, (h - 0.1) / 2, 0.4, d, h - 0.1, L["r"] * 2)
        rbox(P, "Fp_Lintel", surround, 0, 0, h - 0.25, w - 0.4, d, 0.3, L["r"] * 2)
        rbox(P, "Fp_Hole", M["dark"], 0, 0.04, 0.42, w - 0.4, d - 0.05, 0.84, 0.01)
        rbox(P, "Fp_Mantel", L["wood"], 0, -0.04, h, w + 0.16, d + 0.1, 0.08, 0.02)
    for i in range(3):
        rod(P, "Fp_Log%d" % i, L["wood"] if k != "plain" else M["wood"], (-0.22, -0.05 + i * 0.06, 0.15 + (i % 2) * 0.08), (0.22, -0.02 + i * 0.05, 0.13 + (i % 2) * 0.1), 0.06)
    blob(P, "Fp_Fire", M["ember"], 0, -0.02, 0.26, 0.4, 0.18, 0.18, top=(0.5, 0.6), levels=2)
    lamp_light(P, "Fp_Light", 0, -0.3, 0.35, 35.0)
    mz = h + 0.04 if k != "curve" else h + 0.08
    rbox(P, "Fp_Clock", L["accent_wood"], -0.4, -0.02, mz + 0.12, 0.22, 0.12, 0.24, 0.03)
    disc(cylz(P, "Fp_Face", M["cream"], 0, 0, 0, 0.07, 0.01, bev=0.004), -0.4, -0.085, mz + 0.14)
    for i, x in enumerate((0.25, 0.45)):
        cylz(P, "Fp_Stick%d" % i, L["accent_wood"], x, 0, mz, 0.04, 0.2, r_top=0.025, bev=0.01)
        cylz(P, "Fp_Candle%d" % i, M["cream"], x, 0, mz + 0.2, 0.022, 0.12, bev=0.005)


def l_piano(P, L, rng):
    w, d, h = 1.45, 0.58, 1.25
    if L["round"]:
        blob(P, "Pi_Body", L["wood"], 0, 0.06, h / 2 + 0.05, w + 0.06, d - 0.1, h + 0.1, top=(0.95, 0.85), levels=2)
    else:
        rbox(P, "Pi_Body", L["wood"], 0, 0.08, h / 2, w, d - 0.16, h, L["r"])
        if L["panels"]:
            for s in (-1, 1):
                rbox(P, "Pi_Panel%d" % s, L["wood_dark"], s * 0.35, -d / 2 + 0.07, 1.0, 0.5, 0.012, 0.28, 0.006, 1)
            for s in (-1, 1):
                leg(P, "Pi_Leg%d" % s, L, s * (w / 2 - 0.08), -d / 2 + 0.05, 0.66)
    rbox(P, "Pi_Shelf", L["wood"], 0, -d / 2 + 0.1, 0.68, w - 0.06, 0.32, 0.06, L["r"])
    rbox(P, "Pi_Keys", M["keys"], 0, -d / 2 + 0.07, 0.72, w - 0.2, 0.16, 0.025, 0.004, 1)
    for i in range(18):                                      # black keys in twos and threes
        if i % 7 in (2, 6):
            continue
        x = -(w - 0.24) / 2 + 0.04 + i * (w - 0.3) / 18
        rbox(P, "Pi_Black%d" % i, M["keys_dark"], x, -d / 2 + 0.1, 0.745, 0.022, 0.09, 0.025, 0.004, 1)
    rbox(P, "Pi_Lid", L["wood"], 0, -d / 2 + 0.15, 0.8, w - 0.06, 0.04, 0.12, L["r"]).rotation_euler = (0.4, 0, 0)
    rbox(P, "Pi_Music", M["cream"], 0, -d / 2 + 0.17, 0.98, 0.34, 0.02, 0.24, 0.004, 1).rotation_euler = (-0.25, 0, 0)
    for s in (-1, 1):
        cylz(P, "Pi_Cand%d" % s, L["accent_wood"], s * (w / 2 - 0.12), 0.0, h, 0.05, 0.14, r_top=0.03, bev=0.01)
    Q = group("PiStool", 0, -0.75, 0.0, P)
    stool(Q, "PiS", L, h=0.48, s=0.5)


def l_records(P, L, rng):
    w, d, h = 1.1, 0.45, 0.7
    z0 = base(P, "Rc", L, w, d)
    side = M["iron"] if L["leg"] == "iron" else L["wood"]
    for s in (-1, 0, 1):                                     # an open half for records beside a cupboard
        rbox(P, "Rc_Side%d" % s, side if s else L["wood"], s * (w / 2 - 0.025), 0, (z0 + h) / 2, 0.05, d, h - z0, L["r"])
    rbox(P, "Rc_Bottom", L["wood"], 0, 0, z0 + 0.02, w, d, 0.04, L["r"])
    rbox(P, "Rc_TopSlab", L["wood"], 0, 0, h - 0.02, w + 0.02, d + 0.02, 0.045, L["r"])
    rbox(P, "Rc_Back", L["wood_dark"], 0, d / 2 - 0.015, (z0 + h) / 2, w - 0.04, 0.03, h - z0, 0.006, 1)
    rbox(P, "Rc_Cup", L["wood"], w / 4, 0.0, (z0 + h) / 2, w / 2 - 0.06, d - 0.04, h - z0 - 0.06, 0.01)
    if L["panels"]:
        rbox(P, "Rc_Cornice", L["wood"], 0, 0.01, h + 0.03, w + 0.07, d + 0.05, 0.06, 0.02)
    for i in range(10):                                      # record sleeves standing in the open half
        x = -w / 2 + 0.08 + i * 0.042
        rbox(P, "Rc_Sleeve%d" % i, M[("terra", "teal", "cream", "ochre", "plum", "dusty")[i % 6]], x, -0.02, z0 + 0.04 + 0.16, 0.03, d - 0.12, 0.32, 0.006, 1)
    panel_door(P, "Rc_Door", L, w / 4, z0 + 0.04, w / 2 - 0.08, h - z0 - 0.08, -d / 2, L["wood"])
    handle(P, "Rc_Knob", L, 0.08, -d / 2 - 0.05, (z0 + h) / 2)
    lift = 0.06 if L["panels"] else 0.0
    m = L["accent_wood"] if L["key"] != "workshop" else M["iron"]
    rbox(P, "Rc_Player", m, 0.15, 0, h + lift + 0.06, 0.42, 0.34, 0.12, 0.03)
    cylz(P, "Rc_Disc", M["ink"], 0.12, 0, h + lift + 0.12, 0.14, 0.012, bev=0.004)
    cylz(P, "Rc_Label", M["terra"], 0.12, 0, h + lift + 0.13, 0.04, 0.006, bev=0.002)
    rod(P, "Rc_Arm", M["iron"], (0.3, 0.12, h + lift + 0.15), (0.2, -0.06, h + lift + 0.15), 0.021)
    if L["key"] in ("oldtown", "curve"):                     # a horn speaker
        cylz(P, "Rc_Horn", M["ochre"], -0.25, 0.05, h + lift, 0.04, 0.15, bev=0.01)
        ball(P, "Rc_Speaker", L["wood"], -0.25, 0.05, h + lift + 0.28, 0.32, 0.2, 0.3)
        disc(cylz(P, "Rc_Grille", L["cloth"], 0, 0, 0, 0.1, 0.01, bev=0.003), -0.25, -0.1, h + lift + 0.28)
    else:
        rbox(P, "Rc_Speaker", L["wood"], -0.3, 0.02, h + lift + 0.18, 0.26, 0.24, 0.36, L["r"])
        disc(cylz(P, "Rc_Grille", L["cloth_dark"] if L["key"] != "workshop" else M["dark"], 0, 0, 0, 0.09, 0.012, bev=0.003), -0.3, -0.105, h + lift + 0.15)


def l_sidetable(P, L, rng):
    tbl(P, "St", L, 0.5, 0.5, 0.58)
    if not L["round"]:
        rbox(P, "St_Shelf", L["wood"] if L["leg"] != "iron" else L["wood"], 0, 0, 0.18, 0.4, 0.4, 0.035, 0.01)
    lamp_small(P, L, 0.05, 0.05, 0.58 if not L["round"] else 0.58)
    rbox(P, "St_Book", M["plum"], -0.12, -0.12, 0.6, 0.2, 0.14, 0.04, 0.008, 1)


def l_magrack(P, L, rng):
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    w, d = 0.45, 0.3
    if L["round"]:
        cylz(P, "Mg_Bin", M["wicker"], 0, 0, 0, 0.2, 0.36, r_top=0.23, bev=0.02)
    else:
        for s in (-1, 1):
            rbox(P, "Mg_End%d" % s, m, s * w / 2, 0, 0.22, 0.04, d, 0.44, L["r"])
        rbox(P, "Mg_Base", L["wood"], 0, 0, 0.06, w, d, 0.04, 0.01)
        rbox(P, "Mg_Div", L["wood"], 0, 0, 0.25, w, 0.03, 0.34, 0.01)
        rod(P, "Mg_Grip", m, (-w / 2, 0, 0.5), (w / 2, 0, 0.5), 0.022)
    for i in range(4):
        rbox(P, "Mg_Mag%d" % i, M[("teal", "terra", "cream", "ochre")[i]], -0.08 + i * 0.05, (-1) ** i * 0.07, 0.25, 0.012 * 2, 0.22, 0.3, 0.004, 1).rotation_euler = (0, 0, 0.1 * (i - 1.5))


def l_tv(P, L, rng):
    w, d, h = 1.3, 0.45, 0.55
    z0 = base(P, "Tv", L, w, d)
    body(P, "Tv", L, w, d, z0, h)
    doors(P, "Tv", L, w, d, z0 + 0.03, h - 0.03, 2)
    lift = 0.06 if L["panels"] else 0.0
    m = L["wood"] if L["key"] in ("oldtown", "plain") else (M["enamel"] if L["round"] else M["iron"])
    if L["round"]:                                           # a rounded box set
        blob(P, "Tv_Box", m, 0, 0, h + lift + 0.26, 0.7, 0.46, 0.52, levels=2)
        blob(P, "Tv_Glass", M["screen"], 0, -0.2, h + lift + 0.27, 0.48, 0.06, 0.36, levels=2)
    else:
        rbox(P, "Tv_Box", m, 0, 0, h + lift + 0.26, 0.72, 0.44, 0.5, 0.04)
        rbox(P, "Tv_Glass", M["screen"], -0.07, -0.215, h + lift + 0.27, 0.46, 0.02, 0.36, 0.05)
        for i in range(2):
            ball(P, "Tv_Dial%d" % i, M["cream"], 0.26, -0.23, h + lift + 0.33 - i * 0.12, 0.07, 0.04, 0.07)
    for s in (-1, 1):
        rod(P, "Tv_Ant%d" % s, M["iron"], (0, 0.05, h + lift + 0.5), (s * 0.22, 0.05, h + lift + 0.8), 0.021)


# ---- decoration ---------------------------------------------------------------------------------------------------
def o_floorplant(P, L, rng):
    if L["key"] == "workshop":
        rbox(P, "Fp_Crate", L["wood"], 0, 0, 0.2, 0.42, 0.42, 0.4, 0.015)
        plant(P, "Fpl", 0, 0, 0.35, 1.6, rng, pot=False)
    elif L["round"]:
        cylz(P, "Fp_Pot", M["porcelain"], 0, 0, 0, 0.2, 0.42, r_top=0.24, bev=0.05)
        plant(P, "Fpl", 0, 0, 0.36, 1.8, rng, pot=False)
    else:
        plant(P, "Fpl", 0, 0, 0, 2.0, rng)


def o_smallplants(P, L, rng):
    for i, (x, s) in enumerate(((-0.25, 0.5), (0.0, 0.7), (0.28, 0.45))):
        plant(P, "Sp%d" % i, x, 0, 0, s, rng)
    rbox(P, "Sp_Tray", L["wood"], 0, 0, -0.01, 0.8, 0.28, 0.02, 0.008, 1)


def o_painting(P, L, rng):
    w, h, z = 0.9, 0.65, 1.55
    m = L["wood"] if L["leg"] != "iron" else M["iron"]
    if L["round"]:
        blob(P, "Pa_Frame", L["wood"], 0, 0.075, z, w + 0.1, 0.05, h + 0.1, levels=3)
    else:
        frame_rect(P, "Pa", m, 0, 0.02, z, w + 0.12, h + 0.12, 0.09 if L["panels"] else 0.06, 0.06)
    rbox(P, "Pa_Canvas", M["dusty"], 0, 0.03, z + 0.1, w - 0.04, 0.02, h * 0.55, 0.004, 1)
    rbox(P, "Pa_Ground", M["olive"], 0, 0.03, z - h / 2 + 0.16, w - 0.04, 0.02, h * 0.45, 0.004, 1)
    ball(P, "Pa_Sun", M["ochre"], 0.22, 0.015, z + 0.15, 0.12, 0.01, 0.12)
    for i in range(3):                                       # little houses on the hill
        rbox(P, "Pa_House%d" % i, M[("cream", "terra", "cream")[i]], -0.25 + i * 0.12, 0.012, z - 0.02, 0.08, 0.01, 0.1, 0.004, 1)


def o_clock(P, L, rng):
    z = 1.8
    if L["panels"]:                                          # a long-case clock on the floor
        rbox(P, "Ck_Case", L["wood"], 0, 0, 0.9, 0.42, 0.28, 1.8, L["r"])
        rbox(P, "Ck_Hood", L["wood"], 0, 0, 1.92, 0.5, 0.34, 0.34, 0.03)
        ball(P, "Ck_Fin", L["accent_wood"], 0, 0, 2.15, 0.1, 0.1, 0.12)
        rbox(P, "Ck_Window", L["wood_dark"], 0, -0.145, 0.95, 0.2, 0.012, 0.5, 0.01, 1)
        disc(cylz(P, "Ck_Bob", L["accent_wood"], 0, 0, 0, 0.06, 0.02, bev=0.006), 0, -0.16, 0.8)
        fz, fy, fr = 1.92, -0.175, 0.15
    else:
        fz, fy, fr = z, -0.04, 0.22 if L["round"] else 0.18
        rim = L["wood"] if L["leg"] != "iron" else M["iron"]
        if L["key"] == "plain":
            rbox(P, "Ck_Rim", rim, 0, 0, fz, fr * 2 + 0.06, 0.06, fr * 2 + 0.06, 0.04)
        else:
            disc(cylz(P, "Ck_Rim", rim, 0, 0, 0, fr + 0.04, 0.06, segs=40, bev=0.02), 0, 0, fz)
    disc(cylz(P, "Ck_Face", M["cream"], 0, 0, 0, fr, 0.012, segs=40, bev=0.004), 0, fy, fz)
    for i in range(12):
        a = i * math.pi / 6
        ball(P, "Ck_Tick%d" % i, M["ink"], math.sin(a) * fr * 0.82, fy - 0.01, fz + math.cos(a) * fr * 0.82, 0.03, 0.01, 0.03)
    rbox(P, "Ck_Hour", M["ink"], 0.03, fy - 0.015, fz + 0.03, 0.022, 0.01, fr * 0.55, 0.004, 1).rotation_euler = (0, 0.8, 0)
    rbox(P, "Ck_Min", M["ink"], -0.02, fy - 0.02, fz + 0.05, 0.02, 0.01, fr * 0.8, 0.004, 1).rotation_euler = (0, -0.4, 0)


def o_vases(P, L, rng):
    if L["round"]:
        cylz(P, "Vs_Plinth", L["wood"], 0, 0, 0, 0.25, 0.7, r_top=0.22, bev=0.04)
    else:
        rbox(P, "Vs_Plinth", L["wood"], 0, 0, 0.35, 0.7, 0.32, 0.7, L["r"])
    tz = 0.7
    shapes = ((-0.18, "teal", 0.09, 0.36, 0.05), (0.02, "terra", 0.12, 0.24, 0.07), (0.2, "cream", 0.07, 0.3, 0.03))
    for i, (x, mt, r, h, rt) in enumerate(shapes):
        cylz(P, "Vs_%d" % i, M[mt], x * (0.7 if L["round"] else 1), 0, tz, r, h, r_top=rt, bev=0.02)
        if L["round"]:
            ball(P, "Vs_Belly%d" % i, M[mt], x * 0.7, 0, tz + h * 0.35, r * 2.4, r * 2.4, h * 0.6)
    for i in range(3):
        rod(P, "Vs_Stem%d" % i, M["leaf"], (-0.18 * (0.7 if L["round"] else 1), 0, tz + 0.3), (-0.18 * (0.7 if L["round"] else 1) + (i - 1) * 0.08, 0, tz + 0.6), 0.021)
        ball(P, "Vs_Bloom%d" % i, M[("terra", "cream", "plum")[i]], -0.18 * (0.7 if L["round"] else 1) + (i - 1) * 0.08, 0, tz + 0.62, 0.09, 0.09, 0.08)


def o_candles(P, L, rng):
    m = M["iron"] if L["leg"] == "iron" else L["accent_wood"]
    if L["panels"]:                                          # a three-branch candelabra
        cylz(P, "Cd_Foot", m, 0, 0, 0, 0.1, 0.04, r_top=0.06, bev=0.01)
        rod(P, "Cd_Stem", m, (0, 0, 0.04), (0, 0, 0.3), 0.022)
        rod(P, "Cd_Bar", m, (-0.16, 0, 0.3), (0.16, 0, 0.3), 0.022)
        xs = (-0.16, 0.0, 0.16)
        zs = (0.3, 0.3, 0.3)
    else:
        xs = (-0.14, 0.0, 0.14)
        zs = (0.0, 0.0, 0.0)
        for i, x in enumerate(xs):
            cylz(P, "Cd_Holder%d" % i, m, x, 0, 0, 0.06 if not L["round"] else 0.07, 0.05 + i * 0.06, r_top=0.035, bev=0.01)
        zs = tuple(0.05 + i * 0.06 for i in range(3))
    for i, (x, z) in enumerate(zip(xs, zs)):
        hh = (0.2, 0.26, 0.16)[i]
        cylz(P, "Cd_Wax%d" % i, M["cream"], x, 0, z, 0.03, hh, bev=0.006)
        ball(P, "Cd_Flame%d" % i, M["flame"], x, 0, z + hh + 0.035, 0.035, 0.035, 0.06)
    lamp_light(P, "Cd_Light", 0, -0.05, 0.5, 6.0)


def o_curtains(P, L, rng):
    w, z = 2.3, 2.35                                         # drawn back beside a 1.4 m window, so the sun still comes in
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    rod(P, "Ct_Pole", m, (-w / 2 - 0.1, 0, z), (w / 2 + 0.1, 0, z), 0.025)
    for s in (-1, 1):
        ball(P, "Ct_Fin%d" % s, m if not L["panels"] else L["accent_wood"], s * (w / 2 + 0.12), 0, z, 0.09, 0.09, 0.09)
        cloth_hang(P, "Ct_Drape%d" % s, L["cloth"] if L["key"] != "workshop" else L["accent"], s * (w / 2 - 0.2), 0.04, z - 0.04, 0.5, 2.2, 5)
        if L["panels"] or L["round"]:
            rbox(P, "Ct_Tie%d" % s, L["accent"] if L["key"] != "workshop" else M["cream"], s * (w / 2 - 0.2), -0.02, 1.0, 0.54, 0.08, 0.06, 0.02)
    if L["panels"]:
        rbox(P, "Ct_Pelmet", L["cloth"], 0, -0.03, z + 0.05, w + 0.24, 0.06, 0.24, 0.03)


def o_wallshelf(P, L, rng):
    w, z = 1.0, 1.4
    rbox(P, "Ws_Shelf", L["wood"], 0, -0.1, z, w, 0.22, 0.045 if not L["leg"] == "iron" else 0.06, L["r"])
    for s in (-1, 1):
        if L["leg"] == "iron":
            rbox(P, "Ws_Br%d" % s, M["iron"], s * (w / 2 - 0.12), -0.06, z - 0.08, 0.04, 0.14, 0.14, 0.01, 1)
        elif L["panels"]:
            blob(P, "Ws_Br%d" % s, L["wood"], s * (w / 2 - 0.12), -0.06, z - 0.08, 0.05, 0.16, 0.16, top=(1.0, 1.0), bottom=(1.0, 0.3), levels=2)
        else:
            rbox(P, "Ws_Br%d" % s, L["wood"], s * (w / 2 - 0.12), -0.02, z - 0.08, 0.04, 0.06, 0.14, 0.01, 1)
    books(P, "Ws", -w / 2 + 0.05, -0.05, -0.1, z + 0.023, rng)
    cylz(P, "Ws_Jug", M["cream"], 0.15, -0.1, z + 0.023, 0.06, 0.16, r_top=0.04, bev=0.012)
    ball(P, "Ws_Shell", M["terra"], 0.35, -0.1, z + 0.08, 0.12, 0.1, 0.1)


def o_cushions(P, L, rng):
    for i, (x, z, mt, ang) in enumerate(((-0.2, 0.0, "terra", 0.2), (0.15, 0.0, "teal", -0.15), (0.0, 0.16, "ochre", 0.05))):
        o = cushion(P, "Cu%d" % i, L, M[mt] if L["key"] != "oldtown" else (L["cloth"], L["accent"], M["plum"])[i], x, 0, z + 0.12, 0.44, 0.18, 0.4)
        o.rotation_euler = (math.radians(-30), 0, ang)
    cylz(P, "Cu_Basket", M["wicker"], 0.62, 0, 0, 0.2, 0.32, r_top=0.23, bev=0.02)
    cushion(P, "Cu_Throw", L, L["accent"], 0.62, 0, 0.34, 0.36, 0.36, 0.1)


def o_globe(P, L, rng):
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    if L["leg"] in ("turned", "taper"):
        for i in range(3):
            a = i * 2 * math.pi / 3
            rod(P, "Gl_Leg%d" % i, m, (math.cos(a) * 0.2, math.sin(a) * 0.2, 0), (0, 0, 0.6), 0.024)
        z = 0.85
    else:
        rbox(P, "Gl_Table", m if L["leg"] != "iron" else L["wood"], 0, 0, 0.3, 0.4, 0.4, 0.6, L["r"])
        z = 0.85
    ball(P, "Gl_Sea", M["dusty"], 0, 0, z, 0.42, 0.42, 0.42)
    for i in range(4):
        a = i * 1.6 + 0.3
        ball(P, "Gl_Land%d" % i, M[("olive", "ochre", "olive", "cream")[i]], math.cos(a) * 0.17, math.sin(a) * 0.17, z + (i - 1.5) * 0.08, 0.12, 0.12, 0.14)
    rod(P, "Gl_Axis", m, (0, 0.04, z - 0.25), (0, -0.04, z + 0.25), 0.021)
    o = cylz(P, "Gl_Ring", m, 0, 0, 0, 0.25, 0.03, segs=36, bev=0.01)
    o.rotation_euler = (math.pi / 2, 0, 0.3)
    o.location = (0, 0, z)


def o_fruit(P, L, rng):
    tbl(P, "Fr", L, 0.5, 0.5, 0.58)
    tz = 0.58
    cylz(P, "Fr_Bowl", M["ochre"] if L["key"] != "curve" else M["teal"], -0.08, 0, tz, 0.08, 0.08, r_top=0.15, bev=0.015)
    for i in range(4):
        ball(P, "Fr_Fruit%d" % i, M[("terra", "olive", "ochre", "terra")[i]], -0.08 + (i - 1.5) * 0.06, (i % 2) * 0.05 - 0.02, tz + 0.12, 0.08, 0.08, 0.08)
    pot = M["teal"] if L["key"] in ("plain", "curve") else (M["porcelain"] if L["key"] == "oldtown" else M["iron"])
    ball(P, "Fr_Teapot", pot, 0.12, 0.05, tz + 0.08, 0.18, 0.15, 0.15)
    ball(P, "Fr_TeaLid", pot, 0.12, 0.05, tz + 0.16, 0.08, 0.08, 0.05)
    rod(P, "Fr_Spout", pot, (0.18, 0.05, tz + 0.06), (0.25, 0.05, tz + 0.13), 0.022)
    cylz(P, "Fr_Cup", M["cream"], 0.15, -0.14, tz, 0.045, 0.07, bev=0.008)


def o_bust(P, L, rng):
    if L["key"] == "workshop":
        rbox(P, "Bu_Plinth", M["stone_dark"], 0, 0, 0.5, 0.36, 0.36, 1.0, 0.02)
    elif L["round"]:
        cylz(P, "Bu_Plinth", L["wood"], 0, 0, 0, 0.18, 1.0, r_top=0.15, bev=0.04)
    else:
        rbox(P, "Bu_Plinth", M["stone"] if L["panels"] else L["wood"], 0, 0, 0.5, 0.36, 0.36, 1.0, L["r"] * 2)
        if L["panels"]:
            rbox(P, "Bu_Cap", M["stone"], 0, 0, 1.02, 0.42, 0.42, 0.06, 0.02)
    m = M["porcelain"] if L["key"] != "workshop" else M["iron"]
    blob(P, "Bu_Chest", m, 0, 0, 1.15, 0.34, 0.2, 0.22, top=(0.7, 0.8), levels=2)
    rod(P, "Bu_Neck", m, (0, 0, 1.22), (0, 0, 1.32), 0.05)
    ball(P, "Bu_Head", m, 0, 0, 1.42, 0.2, 0.22, 0.26)
    ball(P, "Bu_Nose", m, 0, -0.11, 1.42, 0.04, 0.05, 0.06)


KITS = [
    ("kitchen", "Kitchen", [k_counter, k_wall, k_stove, k_sink, k_fridge, k_potshelf]),
    ("bathroom", "Bathroom", [b_bath, b_basin, b_toilet, b_rail, b_mirror, b_basket]),
    ("dining", "Dining", [d_sideboard, d_dresser, d_bench, d_barstool, d_display, d_pendant]),
    ("study", "Study", [s_desk, s_chair, s_filing, s_lamp, s_pinboard, s_shelving]),
    ("children", "Children", [c_bunk, c_cot, c_toychest, c_table, c_horse, c_toyshelf]),
    ("hallway", "Hallway", [h_coatstand, h_shoebench, h_console, h_umbrella, h_hooks, h_mirror]),
    ("garden", "Garden", [g_bench, g_bistro, g_planter, g_parasol, g_lounger, g_plantstand]),
    ("shop", "Shop", [p_counter, p_gondola, p_rail, p_table, p_aboard, p_form]),
    ("cafe", "Cafe", [f_counter, f_machine, f_banquette, f_table, f_menu, f_cakestand]),
    ("bedroom", "Bedroom", [r_chest, r_vanity, r_single, r_blanket, r_screen, r_cheval]),
    ("living", "Living", [l_fireplace, l_piano, l_records, l_sidetable, l_magrack, l_tv]),
    ("decor", "Decoration", [o_floorplant, o_smallplants, o_painting, o_clock, o_vases, o_candles, o_curtains, o_wallshelf,
                             o_cushions, o_globe, o_fruit, o_bust]),
]

# Room sets: (piece, x, y, yaw) in a 6.4 x 4.4 room (back wall y=2.2, left wall x=-3.2, window in the right wall), one
# line each so every line is seen furnishing a room. "k17:<kind>" places a kit 17 piece; "D:<fn>" a decoration.
ROOMS = {
    "kitchen": ("plain", [("k_wall", -2.0, 2.03, 0), ("k_counter", -2.0, 1.88, 0), ("k_stove", -0.65, 1.87, 0), ("k_sink", 0.35, 1.88, 0),
                          ("k_wall", 0.35, 2.03, 0), ("k_fridge", 1.35, 1.82, 0), ("k_potshelf", -3.02, -0.2, math.pi / 2),
                          ("k17:dining", 0.2, -0.1, 0.0), ("d_pendant", 0.2, -0.1, 0), ("o_smallplants", 2.6, 1.95, 0)]),
    "bathroom": ("curve", [("b_bath", -1.8, 1.75, 0), ("b_basin", 0.3, 1.9, 0), ("b_mirror", 0.3, 2.17, 0), ("b_toilet", 1.3, 1.88, 0),
                           ("b_rail", -3.0, -0.2, math.pi / 2), ("b_basket", 2.6, 1.8, 0), ("o_floorplant", 2.7, -0.4, 0),
                           ("d_pendant", -0.3, 0.3, 0)]),
    "dining": ("oldtown", [("d_dresser", -1.9, 1.95, 0), ("d_sideboard", 0.6, 1.95, 0), ("k17:dining", -0.4, 0.3, math.pi / 2),
                           ("d_bench", -0.4, -0.75, 0), ("d_pendant", -0.4, 0.3, 0), ("d_display", -2.95, -0.6, math.pi / 2),
                           ("o_painting", 0.6, 2.18, 0), ("o_clock", 2.5, 2.0, 0)]),
    "study": ("workshop", [("s_desk", -0.8, 1.75, 0), ("s_chair", -0.8, 1.15, math.pi), ("s_lamp", -1.25, 1.85, 0),
                           ("s_pinboard", -0.8, 2.18, 0), ("s_shelving", -2.95, 0.3, math.pi / 2), ("s_filing", 0.4, 1.88, 0),
                           ("k17:armchair", 1.9, 0.0, -math.pi / 2 - 0.4), ("k17:floor_lamp", 2.5, 1.0, 0), ("k17:rug", -0.3, 0.6, 0)]),
    "children": ("curve", [("c_bunk", -2.5, 1.0, 0), ("c_cot", 0.0, 1.8, 0), ("c_toyshelf", 1.6, 1.98, 0), ("c_table", -0.2, -0.2, 0),
                           ("c_horse", 1.6, 0.3, -0.6), ("c_toychest", 2.6, -0.8, -math.pi / 2), ("k17:rug", 0.2, 0.0, 0),
                           ("d_pendant", 0.0, 0.2, 0), ("o_painting", 1.6, 2.18, 0)]),
    "hallway": ("oldtown", [("h_console", -0.6, 2.0, 0), ("h_mirror", -0.6, 2.18, 0), ("h_coatstand", -2.6, 1.6, 0),
                            ("h_shoebench", 1.0, 1.95, 0), ("h_hooks", 1.0, 2.18, 0), ("h_umbrella", -1.8, 1.9, 0),
                            ("k17:rug", 0.0, 0.3, 0), ("o_floorplant", 2.6, 1.8, 0), ("d_pendant", 0.0, 0.5, 0),
                            ("o_clock", -3.0, 0.0, math.pi / 2)]),
    "garden": ("workshop", [("g_bench", -1.4, 1.85, 0), ("g_bistro", 0.6, 0.2, 0), ("g_planter", 1.2, 1.95, 0), ("g_parasol", 0.6, 0.2, 0),
                            ("g_lounger", -2.3, -0.3, 0.3), ("g_plantstand", 2.4, 1.7, 0), ("o_smallplants", -0.2, 1.95, 0)]),
    "shop": ("plain", [("p_counter", 1.4, 1.5, 0), ("p_gondola", -1.2, 0.3, 0), ("p_rail", -2.2, 1.9, 0), ("p_table", 0.6, -0.3, 0),
                       ("p_form", 2.6, 0.2, -0.5), ("p_aboard", -0.2, -1.4, 0.2), ("d_pendant", 0.0, 0.5, 0), ("d_pendant", -2.0, 0.5, 0)]),
    "cafe": ("oldtown", [("f_counter", 1.2, 1.6, 0), ("f_machine", 2.7, 1.88, 0), ("f_menu", 1.2, 2.18, 0), ("f_banquette", -1.9, 1.85, 0),
                         ("f_table", -2.3, 1.15, 0), ("f_table", -1.3, 1.15, 0), ("f_cakestand", -0.1, 0.5, 0),
                         ("f_table", 0.3, -0.7, 0), ("d_barstool", 0.6, 0.85, 0), ("d_pendant", -1.8, 1.15, 0), ("d_pendant", 0.3, -0.6, 0)]),
    "bedroom": ("plain", [("r_single", -1.8, 1.1, 0), ("r_blanket", -1.8, -0.1, 0), ("r_chest", 0.0, 1.95, 0), ("r_vanity", 1.4, 1.92, 0),
                          ("r_screen", -3.0, -1.0, math.pi / 2), ("r_cheval", 2.6, 0.3, -math.pi / 2 - 0.3), ("k17:bedside", -0.9, 1.95, 0),
                          ("k17:rug", -0.5, 0.4, 0), ("o_curtains", 3.15, 0.5, -math.pi / 2)]),
    "living": ("curve", [("l_fireplace", -0.3, 1.98, 0), ("l_piano", -2.2, 1.9, 0), ("l_records", 1.7, 1.95, 0),
                         ("k17:sofa", -2.6, -0.4, math.pi / 2), ("k17:armchair", 1.4, 0.4, -math.pi / 2), ("l_sidetable", 1.4, -0.5, 0),
                         ("l_magrack", -1.4, -0.6, 0), ("k17:rug", -0.3, 0.6, 0), ("o_painting", -0.3, 2.18, 0), ("k17:floor_lamp", 2.6, 1.5, 0)]),
    "decor": ("oldtown", [("o_curtains", 3.15, 0.5, -math.pi / 2), ("o_floorplant", 2.6, -0.8, 0), ("o_painting", -1.4, 2.18, 0),
                          ("o_clock", 0.6, 2.0, 0), ("o_vases", -2.6, 1.9, 0), ("o_wallshelf", -1.4, 2.18, 0), ("o_globe", 2.0, 1.6, 0),
                          ("o_fruit", -0.6, 0.6, 0), ("o_bust", -2.95, -0.5, math.pi / 2), ("o_cushions", 1.0, 1.95, 0),
                          ("k17:sofa", 1.0, 1.65, 0), ("o_candles", -2.6, 1.9, 0), ("k17:rug", 0.2, 0.4, 0),
                          ("k17:floor_lamp", 2.2, 0.6, 0)]),
}


def place(spec, P, L, rng):
    if spec.startswith("k17:"):
        return make(spec[4:], P, L, rng)
    return globals()[spec](P, L, rng)


# ---- scenes -------------------------------------------------------------------------------------------------------
def kit_sheet(kit, Ls):
    key, label, fns = kit
    rng = random.Random(18)
    gap_x, gap_y = (2.5 if len(fns) <= 8 else 2.2), 3.4   # decoration's twelve small pieces sit closer
    rows = Ls
    bands = [fns]
    nx = max(len(b) for b in bands)
    ny = len(rows) * len(bands)
    box("Sheet_Floor", -5.0, nx * gap_x + 1, -3, ny * gap_y + 2, -0.1, 0, M["plaster"], bevel=0)
    r = 0
    for b in bands:
        for L in rows:
            y = (ny - 1 - r) * gap_y
            for c, fn in enumerate(b):
                P = group("S_%s_%s_%d" % (L["key"], fn.__name__, r), c * gap_x, y, 0.35)
                fn(P, L, rng)
            text("S_Label_%s_%d" % (L["key"], r), L["label"], -3.0, y, 0.02, 0.5, "iron").rotation_euler = (0, 0, 0)
            r += 1
    people([("PlayerA", nx * gap_x - 0.4, -1.2)])
    sun_and_sky(SCENE, (0.55, -0.55, 0.62), 6.0, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.6)
    cx = (nx - 1) * gap_x / 2 - 1.0
    cy = (ny - 1) * gap_y / 2
    span = max(nx * gap_x + 4.0, ny * gap_y * 1.25)
    k = span / 30.0
    return [("cam", (cx, cy - 22.0 * k, 17.0 * k), (cx, cy + 0.3, 0.3), 27.0)]


def kit_room(kit, Ls):
    key = kit[0]
    lk, items = ROOMS[key]
    L = [l for l in Ls if l["key"] == lk][0]
    rng = random.Random(18)
    room_shell("Room", 0, 0, front=2.6)
    lamps = 0
    for i, (spec, x, y, yaw) in enumerate(items):
        P = group("Room_%d_%s" % (i, spec.replace(":", "_")), x, y, yaw)
        place(spec, P, L, rng)
        lamps += spec in ("d_pendant", "k17:floor_lamp", "k17:bedside", "s_lamp", "o_candles", "l_fireplace", "h_console", "l_sidetable")
    if not lamps:                                            # rule I5: every room has a lamp
        P = group("Room_Lamp", -2.6, 1.8, 0)
        make("floor_lamp", P, L, rng)
    people([("PlayerA", 1.1, -1.5)])
    sun_and_sky(SCENE, (0.78, 0.30, 0.42), 8.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.25)
    return [("cam", (0.15, -4.4, 1.7), (0.1, 0.8, 1.0), 17.0)], L["label"]


# ---- main -----------------------------------------------------------------------------------------------------------
def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(SRC, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    pieces = 0
    for kit in KITS:
        pieces += len(kit[2])
    log("%d kits, %d new base pieces, %d made in the four lines" % (len(KITS), pieces, pieces * 4))
    for kit in KITS:
        for what in ("sheet", "room"):
            key = "%s_%s" % (what, kit[0])
            if ONLY and key not in ONLY and kit[0] not in ONLY:
                continue
            try:
                scene = fresh("Furn_" + key)
                extra_mats()
                Ls = lines()
                if what == "sheet":
                    cams = kit_sheet(kit, Ls)
                    render(scene, key, cams, -0.2, (2000, 1100))
                else:
                    cams, label = kit_room(kit, Ls)
                    render(scene, key, cams, 1.3, (1600, 1000))
                    log("  %s room in the %s line" % (kit[0], label))
            except Exception:
                log("FAILED %s:\n%s" % (key, traceback.format_exc()))
    log("DONE")


main()
