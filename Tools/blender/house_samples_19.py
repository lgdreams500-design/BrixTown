"""Brix Town - sample houses 19: one detached house per furniture line, every room furnished (FURN-003, FURN-005; look
test).

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/house_samples_19.py").read(), "house_samples_19.py", "exec"))
or headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/house_samples_19.py
Quick checks: WASTED_ONLY=plain_ground,plain_outside WASTED_SAMPLES=24 WASTED_SCALE=50
From another checkout: WASTED_ROOT=C:/Projects/Wasted-morefurniture

Builds on furniture_kit_18.py (and through it furniture_kit_17.py and the earlier scripts): the same pieces, lines,
materials and light. One house plan, a two-storey detached house of 10 x 8 m in its own garden, is furnished four times,
once in each line (Plain, Old Town, Workshop, Curve), so the line is the only thing that changes inside:

    ground floor   hall and stairs, living room, kitchen with table, dining room, bathroom
    upstairs       landing, main bedroom, children's room, study, guest room
    garden         path, low wall, bench, bistro set, parasol, lounger, planter, plant stand

The outside keeps the town's walls, windows and tile roof (ART_DIRECTION W1-W8); only shutters, door and trim take
the line's accent. Renders go to Art/StyleTests/house_samples_19/:
    <line>_outside.png   the house in its garden, kit daylight
    <line>_ground.png    the ground floor cut away at waist height, from above the street corner
    <line>_upstairs.png  the upper floor the same way
Not approved: for the owner to choose from.
"""
import bpy, bmesh, math, random, os, time, traceback

ROOT = os.environ.get("WASTED_ROOT", "C:/Projects/Wasted").replace("\\", "/").rstrip("/")
_k = open(ROOT + "/Tools/blender/furniture_kit_18.py", encoding="utf-8").read()
_k = _k[:_k.rstrip().rfind("\nmain()")]                   # kit 18's pieces and rooms, without running it
exec(compile(_k, "furniture_kit_18_helpers", "exec"), globals())

NAME = "house_samples_19"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"

# ---- the plan -----------------------------------------------------------------------------------------------------
W0, W1, D0, D1 = -5.0, 5.0, -4.0, 4.0      # outside faces of the walls; the street is at -y
T = 0.25                                    # wall thickness
H = 3.0                                     # storey height (floor to floor)
XA, XB, YM = -1.0, 1.2, 0.5                 # partitions: hall between XA and XB, front and back rooms split at YM
CUT = 1.15                                  # cutaway walls stop here (except the back and left outside walls)
STAIR = (0.075, 1.075, 0.3, 3.75)           # stair well in the hall: x0, x1, y0, y1 (rises towards +y)
# inner faces: outside walls at x -4.75 / 4.75 and y -3.75 / 3.75; partitions at x -1.06 / -0.94, 1.14 / 1.26, y 0.44 / 0.56

# doors in partitions: (wall, along, centre); outside front door in the hall
DOORS = {
    "ground": [("x", XA, -0.9), ("x", XA, 1.4), ("x", XB, -1.8), ("x", XB, 1.4), ("y", YM, -3.0)],
    "upstairs": [("x", XA, -0.9), ("x", XA, 1.4), ("x", XB, -1.8), ("x", XB, 1.4)],
}
# outside windows: (side, centre along the wall, width), the same on both storeys; the front door is at x=0.1
WINDOWS = [("front", -3.3, 1.3), ("front", 3.1, 1.3), ("back", -2.4, 1.1), ("back", 3.6, 1.1),
           ("left", -0.4, 0.9), ("left", 2.4, 1.0), ("right", -3.1, 1.0), ("right", 2.3, 1.0)]

# (piece, x, y, yaw[, z]) - kit 17 pieces as "k17:<kind>"; z is a lift above the floor (a lamp on a desk)
P2 = math.pi / 2
GROUND = [
    # living room: fireplace on the left wall, sofa facing it, television by the door
    ("l_fireplace", -4.55, -1.8, P2), ("o_painting", -4.73, -1.8, P2), ("k17:rug", -3.0, -1.8, P2),
    ("k17:coffee", -3.0, -1.8, P2), ("k17:sofa", -1.57, -1.8, -P2), ("k17:armchair", -3.3, -0.05, 0.0),
    ("k17:floor_lamp", -4.35, -0.1, 0), ("k17:bookcase", -4.58, -3.2, P2), ("l_tv", -1.75, -3.52, math.pi),
    ("o_floorplant", -1.45, -0.1, 0), ("l_magrack", -3.3, -3.45, 0),
    # kitchen: counter, sink under the window and cooker along the back wall, a table in the middle
    ("k_counter", -3.85, 3.45, 0), ("k_sink", -2.4, 3.45, 0), ("k_stove", -1.475, 3.44, 0), ("k_wall", -3.85, 3.58, 0),
    ("k_fridge", -4.42, 1.3, P2), ("k17:dining", -2.6, 1.8, 0.0), ("d_pendant", -2.6, 1.8, 0),
    # hall: shoes and umbrellas by the door, a console and mirror, the stairs
    ("h_coatstand", 0.8, -3.3, 0), ("h_shoebench", -0.76, -2.6, P2), ("h_umbrella", -0.65, -3.45, 0),
    ("h_console", -0.77, 0.35, P2), ("h_mirror", -0.92, 0.35, P2), ("o_clock", -0.8, 2.6, P2),
    ("d_pendant", 0.1, -1.8, 0), ("k17:rug", 0.1, -2.2, P2),
    # dining room
    ("k17:dining", 3.0, -1.7, 0.0), ("d_pendant", 3.0, -1.7, 0), ("d_sideboard", 3.0, 0.2, 0),
    ("o_painting", 3.0, 0.42, 0), ("d_dresser", 4.51, -1.5, -P2), ("d_display", 1.47, -3.2, P2),
    ("o_vases", 4.3, -3.59, math.pi), ("o_candles", 3.4, 0.2, 0, 0.91),
    # bathroom
    ("b_bath", 4.31, 2.3, -P2), ("b_toilet", 1.8, 3.43, 0), ("b_basin", 2.6, 3.5, 0), ("b_mirror", 2.6, 3.67, 0),
    ("b_rail", 1.5, 1.6, P2), ("b_basket", 2.7, 1.0, 0), ("d_pendant", 3.0, 2.2, 0),
]
UPSTAIRS = [
    # main bedroom
    ("k17:bed", -2.9, -0.68, 0), ("k17:bedside", -4.1, 0.23, 0), ("k17:bedside", -1.65, 0.23, 0),
    ("k17:wardrobe", -4.45, -1.5, P2), ("r_chest", -2.0, -3.5, math.pi), ("r_blanket", -2.9, -2.0, 0),
    ("k17:rug", -2.9, -1.4, 0), ("r_cheval", -4.2, -3.2, 0.6), ("o_painting", -2.9, 0.42, 0),
    # children's room
    ("c_bunk", 4.2, -0.6, 0), ("c_toyshelf", 2.3, 0.26, 0), ("c_table", 2.7, -2.1, 0), ("c_horse", 3.9, -3.0, -0.6),
    ("c_toychest", 1.5, -3.2, P2), ("k17:rug", 2.9, -1.6, 0), ("d_pendant", 3.0, -1.8, 0),
    # study: the desk under the back window
    ("s_desk", -2.4, 3.41, 0), ("s_chair", -2.4, 2.75, math.pi), ("s_lamp", -2.85, 3.5, 0, 0.75),
    ("s_shelving", -4.55, 1.35, P2), ("s_filing", -1.5, 3.45, 0), ("s_pinboard", -4.0, 3.73, 0),
    ("k17:armchair", -1.85, 1.4, -P2 - 0.4), ("k17:floor_lamp", -1.45, 0.95, 0),
    # guest room
    ("r_single", 4.15, 2.65, 0), ("k17:bedside", 3.35, 3.54, 0), ("r_vanity", 2.2, 3.51, 0),
    ("r_cheval", 4.3, 1.0, -P2 - 0.3), ("o_curtains", 3.6, 3.71, 0), ("o_floorplant", 1.6, 1.0, 0),
    # landing
    ("o_floorplant", -0.45, -3.3, 0), ("o_bust", -0.76, -1.6, P2), ("d_pendant", 0.1, -1.8, 0),
    ("o_wallshelf", 1.12, -2.6, -P2),
]
GARDEN = [
    ("g_bench", -6.5, 2.2, P2), ("g_bistro", 6.6, -1.0, 0), ("g_parasol", 6.6, -1.0, 0), ("g_lounger", 6.8, 3.0, -0.3),
    ("g_planter", -3.0, -4.75, math.pi), ("g_planter", 3.2, -4.75, math.pi), ("g_plantstand", -6.6, -2.0, P2),
    ("o_smallplants", 1.3, -4.6, math.pi),
]

# what the outside takes from each line. Walls, windows, shutters and roofs are the town's own kit (building_kit_07:
# barrel tiles, ridge tiles, corbels, stone surrounds, louvred shutters); a line picks which of them and its colours.
OUTSIDE = dict(
    plain=dict(wall="plaster", gable="plaster", frame="cream", shutter="teal", door="teal", roof="tile",
               stone=False, quoins=False, patches=0, arches=False, hood=False, band=1, boxes=False),
    oldtown=dict(wall="plaster_b", gable="plaster_b", frame="wood", shutter="wood_red", door="wood_red", roof="tile",
                 stone=False, quoins=True, patches=0, arches=False, hood=True, band=2, boxes=True),
    workshop=dict(wall="stone_a", gable="stone_b", frame="cream", shutter="wood", door="wood", roof="tile_dark",
                  stone=True, quoins=False, patches=0, arches=False, hood=False, band=2, boxes=False),
    curve=dict(wall="plaster", gable="plaster", frame="ochre", shutter="dusty", door="ochre", roof="tile",
               stone=False, quoins=False, patches=0, arches=True, hood=False, band=1, boxes=True),
)
WT = KIT["wall_t"]                          # outside walls are the town kit's depth, so windows sit in deep reveals

# ---- walls with holes -----------------------------------------------------------------------------------------------
def wall(name, axis, at, a0, a1, z0, z1, mat, holes=(), t=T, bevel=0.02):
    """A straight wall at x=at (axis "x", running along y) or y=at (axis "y", running along x), centred on `at`,
    from a0 to a1 along it and z0 to z1 up, with rectangular holes (c0, c1, hz0, hz1) cut out."""
    def piece(n, b0, b1, zz0, zz1):
        if b1 - b0 < 0.01 or zz1 - zz0 < 0.01:
            return
        if axis == "x":
            box(n, at - t / 2, at + t / 2, b0, b1, zz0, zz1, mat, bevel=bevel)
        else:
            box(n, b0, b1, at - t / 2, at + t / 2, zz0, zz1, mat, bevel=bevel)
    cuts = sorted((max(a0, h[0]), min(a1, h[1]), h[2], h[3]) for h in holes if h[1] > a0 and h[0] < a1)
    cur = a0
    for i, (c0, c1, hz0, hz1) in enumerate(cuts):
        piece("%s_S%d" % (name, i), cur, c0, z0, z1)
        piece("%s_U%d" % (name, i), c0, c1, z0, min(hz0, z1))
        piece("%s_O%d" % (name, i), c0, c1, max(hz1, z0), z1)
        cur = c1
    piece("%s_E" % name, cur, a1, z0, z1)


def partition_holes(storey, axis, at):
    out = []
    for ax, a, c in DOORS[storey]:
        if ax == axis and abs(a - at) < 1e-6:
            out.append((c - 0.45, c + 0.45, 0.0, 2.1))
    return out


def window_holes(side, z0):
    out = []
    for s, c, w in WINDOWS:
        if s == side:
            out.append((c - w / 2, c + w / 2, z0 + 0.9, z0 + 2.3))
    return out


def storey_shell(n, storey, z, cut, look):
    """Floor, outside walls with windows and partitions with doors for one storey at height z. With cut, the front and
    right outside walls and every partition stop at CUT so the rooms can be seen from above."""
    top = z + (CUT if cut else H)
    full = z + H
    wm = M[look["wall"]]
    # floor boards (the upper floor leaves the stair well open)
    if storey == "ground":
        box(n + "_Floor", W0 + T, W1 - T, D0 + T, D1 - T, z - 0.12, z, M["boards"], bevel=0)
    else:
        x0, x1, y0, y1 = STAIR
        box(n + "_FloorA", W0 + T, x0, D0 + T, D1 - T, z - 0.2, z, M["boards"], bevel=0)
        box(n + "_FloorB", x1, W1 - T, D0 + T, D1 - T, z - 0.2, z, M["boards"], bevel=0)
        box(n + "_FloorC", x0, x1, D0 + T, y0, z - 0.2, z, M["boards"], bevel=0)
        for i in range(6):                                   # a rail round the well
            y = y0 + i * (y1 - y0) / 5
            rod(None, "%s_Bal%d" % (n, i), M["wood"], (x0, y, z), (x0, y, z + 0.95), 0.022)
        rbox(None, n + "_Hand", M["wood"], x0, (y0 + y1) / 2, z + 0.97, 0.07, y1 - y0, 0.06, 0.02)
        rbox(None, n + "_HandF", M["wood"], (x0 + x1) / 2, y0, z + 0.97, x1 - x0, 0.07, 0.06, 0.02)
    if cut and storey == "upstairs":
        box(n + "_Below", W0, W1, D0, D1, z - 3.0, z - 2.9, M["dark"], bevel=0)
    # outside walls: back and left always full height; front and right cut in the cutaway
    fh = window_holes("front", z) + ([(-0.4, 0.6, z, z + 2.2)] if storey == "ground" else [])
    wall(n + "_Front", "y", D0 + T / 2, W0, W1, z, top, wm, fh)
    wall(n + "_Back", "y", D1 - T / 2, W0, W1, z, full, wm, window_holes("back", z))
    wall(n + "_Left", "x", W0 + T / 2, D0 + T, D1 - T, z, full, wm, window_holes("left", z))
    wall(n + "_Right", "x", W1 - T / 2, D0 + T, D1 - T, z, top, wm, window_holes("right", z))
    # inside: plaster partitions with doors, skirting
    pm = M["plaster"]
    wall(n + "_PA", "x", XA, D0 + T, D1 - T, z, top, pm, partition_holes(storey, "x", XA), t=0.12)
    wall(n + "_PB", "x", XB, D0 + T, D1 - T, z, top, pm, partition_holes(storey, "x", XB), t=0.12)
    holes = [(XA - 0.06, XB + 0.06, z, z + H)] + [(c - 0.45, c + 0.45, z, z + 2.1)
                                                 for ax, a, c in DOORS[storey] if ax == "y" and abs(a - YM) < 1e-6]
    wall(n + "_PM", "y", YM, W0 + T, W1 - T, z, top, pm, holes, t=0.12)
    if storey == "ground":                                    # the stairs up the back of the hall
        x0, x1, y0, y1 = STAIR
        steps = 14
        for i in range(steps):
            sy = y0 + (y1 - y0) * i / steps
            box("%s_Step%d" % (n, i), x0, x1, sy, sy + (y1 - y0) / steps + 0.02, z, z + H * (i + 1) / steps, M["wood"], bevel=0.012)
        for i in range(8):
            y = y0 + i * (y1 - y0) / 7
            zz = z + H * (y - y0) / (y1 - y0)
            rod(None, "%s_SBal%d" % (n, i), M["wood"], (x0 - 0.05, y, zz), (x0 - 0.05, y, zz + 0.95), 0.022)
        rod(None, n + "_SRail", M["wood"], (x0 - 0.05, y0, z + 0.95), (x0 - 0.05, y1, z + H + 0.95), 0.03)


def furnish_storey(n, items, L, z, rng):
    for i, it in enumerate(items):
        spec, x, y, yaw = it[:4]
        lift = it[4] if len(it) > 4 else 0.0
        P = group("%s_%d_%s" % (n, i, spec.replace(":", "_")), x, y, yaw)
        P.location.z = z + lift
        place(spec, P, L, rng)


# ---- outside ------------------------------------------------------------------------------------------------------
# Each outside wall is built as a town facade (street face at y'=0, outward to -y', along x') and then turned into
# place on an empty: front (0, D0) as built, back turned half round, the sides a quarter each way.
FACES = dict(front=((0.0, D0), 0.0, (W0, W1), 1), back=((0.0, D1), math.pi, (W0, W1), -1),
             left=((W0, 0.0), -math.pi / 2, (D0 + WT, D1 - WT), -1), right=((W1, 0.0), math.pi / 2, (D0 + WT, D1 - WT), 1))


def on_empty(name, at, yaw, build):
    """Run build(); everything it makes is parented to an empty at `at` turned by `yaw`."""
    E = group(name, at[0], at[1], yaw)
    before = set(bpy.data.objects)
    build()
    for ob in set(bpy.data.objects) - before:
        if ob.parent is None and ob is not E:
            ob.parent = E
    return E


def facade(side, look, rng):
    at, yaw, (a0, a1), flip = FACES[side]
    a0, a1 = (a0, a1) if side in ("front", "back") else (-(a1), -(a0)) if flip < 0 else (a0, a1)
    lk = dict(frame=look["frame"], shutter=look["shutter"], door=look["door"], shutters=True, boxes=False)
    t = WT

    def build():
        K = Kit("F_" + side)
        holes = []
        if side == "front":
            holes.append(door(K, rng, 0.1, 1.2, 2.4, t, lk, arch=look["arches"], steps=2, hood=look["hood"]))
            if look["arches"]:
                voussoirs(K, rng, 0.1, 2.4 - 0.6, 0.6, t, ring=0.32)
            lantern("F_DoorLamp", 1.05, -0.14, 2.2, M, 30)
        for s, c, w in WINDOWS:
            if s != side:
                continue
            x = c * flip
            arch = look["arches"]
            holes.append(window(K, rng, x, 0.95, w, 1.6, t, lk, arch=arch))
            holes.append(window(K, rng, x, H + 0.8, w, 1.45, t, lk))
            if arch:
                voussoirs(K, rng, x, 0.95 + 1.6 - w / 2, w / 2, t, ring=0.3)
            if look["boxes"] and side in ("front", "right"):
                K.block("terra", x - w / 2 - 0.05, x + w / 2 + 0.05, -0.42, -0.16, H + 0.8, H + 1.06, 0.03)
                for k in range(3):
                    blob(None, "F_Box%s%d%d" % (side, int(x * 10), k), M["plant" if k % 2 else "leaf_light"],
                         x - w / 2 + (k + 0.5) * w / 3, -0.29, H + 1.16, 0.32, 0.26, 0.26, levels=2)
        top = 2 * H
        if look["stone"]:
            masonry(K, rng, a0, a1, 0.0, top, holes, t)
        else:
            masonry(K, rng, a0, a1, 0.0, 0.5, holes, t)          # a stone plinth under the plaster (W2)
            wall = plaster_wall("F_%s_Wall" % side, M, a0, a1, 0.5, top, t, holes, look["wall"])
            for i, h in enumerate(hh for hh in holes if "spring" in hh):
                r = (h["x1"] - h["x0"]) / 2
                cut(wall, box("F_%s_ArchBox%d" % (side, i), h["x0"], h["x1"], -0.5, t + 0.5, max(h["z0"], 0.5) - 0.01, h["spring"], None, bevel=0))
                cut(wall, cyl_y("F_%s_ArchTop%d" % (side, i), (h["x0"] + h["x1"]) / 2, t / 2, h["spring"], r, t + 1.0))
            if look["patches"]:
                stone_patches(K, rng, a0, a1, 0.6, top - 0.3, holes, look["patches"])
        band(K, a0, a1, H - 0.1, t, steps=look["band"])
        if look["quoins"] and side in ("front", "back"):
            quoins(K, a0, 0.5, top, -1, t)
            quoins(K, a1, 0.5, top, 1, t)
        if side == "front":
            drainpipe(K, a1 - 0.3, top)
        K.flush(M)

    on_empty("Facade_" + side, at, yaw, build)


def outside(look, L, rng):
    M["gable"] = M[look["gable"]]
    saved = M["tile"]
    if look["roof"] == "tile_dark":                           # Workshop: the darker of the town's two tile tones
        M["tile"] = M["tile_dark"]
    for side in ("front", "back", "left", "right"):
        facade(side, look, rng)
    box("Dark_Inside", W0 + WT, W1 - WT, D0 + WT, D1 - WT, 0.0, 2 * H, M["dark"], bevel=0)

    def roof():
        K = Kit("Roof")
        tile_roof(K, rng, W0, W1, D1 - D0, 2 * H, chimneys=[(-4.55, -1.8 - D0)])
        K.flush(M)
    on_empty("Roof", (0.0, D0), 0.0, roof)
    M["tile"] = saved
    # the garden: lawn, a stone path to the door, a low wall with a gate gap, a tree, the garden pieces
    lawn = painted("M_Lawn", (0.20, 0.27, 0.13), (0.34, 0.40, 0.20), noise_scale=1.4, bump=0.3)
    box("Lawn", -10, 10, -9, 8, -0.12, -0.02, lawn, bevel=0)
    box("Street", -30, 30, -16, -9, -0.14, -0.1, M["paving"], bevel=0)
    for i in range(5):
        box("Path%d" % i, -0.45 + (i % 2) * 0.1, 0.65 + (i % 2) * 0.1, -8.6 + i * 0.75, -8.0 + i * 0.75, -0.04, 0.0, M["stone_b"], bevel=0.02)
    gm = M["stone_dark"] if look["stone"] else M["stone"]
    for x0, x1 in ((-10, -0.7), (0.9, 10)):
        box("WallF%d" % int(x0), x0, x1, -9.0, -8.7, -0.05, 0.8, gm, bevel=0.03)
    for x in (-10, 10):
        box("WallS%d" % x, x - 0.15, x + 0.15, -9.0, 8, -0.05, 0.8, gm, bevel=0.03)
    box("WallB", -10, 10, 7.7, 8.0, -0.05, 0.8, gm, bevel=0.03)
    for gx in (-0.85, 1.05):
        box("GatePost%d" % int(gx * 10), gx - 0.15, gx + 0.15, -9.05, -8.65, -0.05, 1.1, M["stone_dark"], bevel=0.02)
        ball(None, "GateCap%d" % int(gx * 10), M["stone_dark"], gx, -8.85, 1.2, 0.26, 0.26, 0.2)
    tree = group("Tree", -7.5, -5.5, 0)
    rod(tree, "Tree_Trunk", M["wood"], (0, 0, 0), (0, 0, 2.2), 0.14)
    for i in range(6):
        a = i * 1.05
        blob(tree, "Tree_Leaf%d" % i, M["leaf" if i % 2 else "leaf_light"], math.cos(a) * 0.7, math.sin(a) * 0.7,
             2.6 + (i % 3) * 0.35, 1.6, 1.5, 1.3, levels=2)
    furnish_storey("Garden", GARDEN, L, 0.0, rng)


# ---- scenes -------------------------------------------------------------------------------------------------------
def house_scene(L, view):
    rng = random.Random(19)
    look = OUTSIDE[L["key"]]
    if view == "outside":
        outside(look, L, rng)
        people([("PlayerA", 1.6, -6.2), ("Elder", -4.8, -6.8)])
        sun_and_sky(SCENE, (0.55, -0.62, 0.56), 6.0, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.6)
        return [("cam", (12.5, -21.0, 6.2), (-0.3, -0.5, 3.2), 30.0)], -0.15
    storey = "ground" if view == "ground" else "upstairs"
    box("Ground", -14, 14, -12, 12, -0.4, -0.3, M["paving"], bevel=0)
    storey_shell(storey, storey, 0.0, True, look)
    furnish_storey(storey, GROUND if storey == "ground" else UPSTAIRS, L, 0.0, rng)
    people([("PlayerA", 0.4, -0.9)])
    sun_and_sky(SCENE, (0.45, -0.55, 0.70), 5.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.45)
    return [("cam", (6.5, -12.5, 12.0), (0.0, 0.2, 0.3), 31.0)], 0.35


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    log("one house plan, %d pieces placed inside and %d in the garden, in each of four lines" % (len(GROUND) + len(UPSTAIRS), len(GARDEN)))
    for key in ("plain", "oldtown", "workshop", "curve"):
        for view in ("outside", "ground", "upstairs"):
            job = "%s_%s" % (key, view)
            if ONLY and job not in ONLY and key not in ONLY:
                continue
            try:
                scene = fresh("Furn_" + job)
                extra_mats()
                Ls = lines()
                L = [l for l in Ls if l["key"] == key][0]
                cams, exposure = house_scene(L, view)
                render(scene, job, cams, exposure, (1800, 1100))
            except Exception:
                log("FAILED %s:\n%s" % (job, traceback.format_exc()))
    log("DONE")


main()
