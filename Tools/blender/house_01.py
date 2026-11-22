"""Brix Town - house 01: ONE independent house, designed in Blender before anything goes into the slice (the owner, 2026-11-22:
"start with a single house option first ... once I am satisfied then we implement it in the slice").

Everything about the house is in HOUSE below: change a number, run again, look at the pictures. Built with the kit of
building_kit_07.py (stone, plaster, windows with real reveals, shutters, doors, tiled roofs) in the locked look (ADR-0004).

Run headless (the Store install):
    blender-launcher.exe -b --factory-startup --python C:/Projects/Wasted/Tools/blender/house_01.py
Quick: WASTED_SAMPLES=16 WASTED_SCALE=50   One view: WASTED_ONLY=front,plan_ground
Pictures: Art/StyleTests/house_01/house_01_<view>.png   Scene: Art/Source/StyleTests/house_01.blend (open it and move things)

Frame: x along the front (0 at the left corner seen from the lane, W at the right), y from the front wall (0) to the back
(D), z up from the ground. The lane is in front, at negative y.
"""
import bpy, bmesh, math, random, os, time, traceback
from mathutils import Vector, Matrix

B = "C:/Projects/Wasted/Tools/blender/"
_k = open(B + "building_kit_07.py", encoding="utf-8").read().split("\n# ---- the street and the scenes")[0]
exec(compile(_k, "building_kit_07_helpers", "exec"), globals())

NAME = "house_01"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"
SAMPLES = int(os.environ.get("WASTED_SAMPLES", 32))
SCALE = int(os.environ.get("WASTED_SCALE", 70))
ONLY = tuple(os.environ["WASTED_ONLY"].split(",")) if os.environ.get("WASTED_ONLY") else None

# ================================================================== THE HOUSE: change anything here
HOUSE = dict(
    W=13.0, D=10.0,                 # footprint: along the lane, and deep
    floor_h=3.5, plinth=0.45,       # storey height (owner 2026-11-22: higher ceilings); the stone plinth under the ground floor
    floors=2,
    walls="plaster",                # plaster | stone (all masonry)
    plaster="plaster",              # plaster | plaster_b (the two plaster tones of the kit)
    frame="cream", shutter="teal", door="wood_red",   # kit colours: cream wood teal wood_red ochre dusty
    shutters=True, boxes=True, quoins=True, string_course=True,
    roof=dict(pitch=30.0, eave=0.55, chimneys=[1.0, 12.0]),   # gabled roof, ridge along the front; chimneys at these x
    porch=dict(kind="columns", w=3.4, d=1.6),                  # columns | canopy | none
    # windows on each side: list of (x along that side as seen from outside, sill height above the floor, width, height).
    # The back wall is seen from behind (its x runs from the house's right corner), the left side from the back corner.
    front=dict(door=6.5, ground=[(1.3, 0.9, 1.2, 2.2), (3.1, 0.9, 1.2, 2.2), (9.9, 0.9, 1.2, 2.2), (11.7, 0.9, 1.2, 2.2)],
               first=[(2.1, 1.2, 1.0, 1.4), (9.9, 0.8, 1.2, 2.0), (11.7, 0.8, 1.2, 2.0)],
               hall=(6.5, 0.55, 1.6, 2.6)),                    # the tall arched window over the porch, lighting the hall and stair
    back=dict(door=0.7, ground=[(6.5, 2.0, 1.4, 2.0)], first=[],       # the back door (under the right landing); a stair window
              balconies=dict(w=3.2, d=1.3, door_w=1.4, door_h=2.3)),   # owner: "wider balcony doors"   # owner 2026-11-22: a door from each back-corner landing
                                                                       # onto its own balcony, one at each side
    left=dict(ground=[(4.0, 0.9, 1.1, 2.0), (8.0, 0.9, 1.1, 2.0)], first=[(4.8, 0.8, 1.1, 1.9), (8.7, 1.2, 0.9, 1.3)]),
    right=dict(ground=[(2.0, 0.9, 1.1, 2.0), (6.0, 0.9, 1.1, 2.0)], first=[(2.0, 0.8, 1.1, 1.9), (6.0, 0.8, 1.1, 1.9)]),
    # rooms: (name, floor, x0, y0, x1, y1, open sides: "f" front, "b" back, "l" left, "r" right get no wall).
    # The stair is against the back wall (owner 2026-11-22: "remove everything behind the stair"): the hall runs from the
    # door to the half landing, the side flights run along the back wall to a landing in each back corner. Rooms take the
    # rest; the main bedroom has its own bathroom (owner: "bathroom attached to the bedroom").
    # The corner landings are 1.9 m wide for the 1.4 m balcony doors: the hall gave up 0.4 m and the side flights have
    # 0.25 m treads (owner 2026-11-22: "wider balcony doors").
    rooms=[("Living room", 0, 0.0, 0.0, 4.4, 7.95), ("Hall", 0, 4.4, 0.0, 8.6, 7.95, "b"), ("Kitchen", 0, 8.6, 0.0, 13.0, 7.95),
           ("En-suite", 1, 0.0, 0.0, 4.4, 2.6), ("Main bedroom", 1, 0.0, 2.6, 4.4, 7.95),
           ("Bedroom 2", 1, 8.6, 0.0, 13.0, 7.95)],
    # doors between rooms: (floor, x, y) on a room wall
    doors=[(0, 4.4, 1.6), (0, 8.6, 1.6), (0, 12.3, 7.95),           # hall to living room and kitchen; kitchen to the back door
           (1, 1.1, 7.95), (1, 2.6, 2.6), (1, 11.9, 7.95)],         # side landings to the bedrooms; the en-suite from its bedroom
    # the stair: a flight up the middle of the hall to a half landing against the back wall, then a flight along the back
    # wall to each side, to a landing (side_land wide) in the corner with a door into the bedroom in front
    stair=dict(centre_w=2.6, centre_from=4.95, landing=(7.95, 9.68), side_band=(7.98, 9.66), side_land=1.9, steps=10),
    furniture="oldtown",            # the furniture kits' line: plain | oldtown | workshop | curve; None for plain blocks
)
LANE_Y = -9.5                       # where the lane runs in front (its centre)


# ================================================================== building it
def facade(name, length, holes_plan, door_x, door_h=2.45, door_w=1.2, porch=False, hall=None, rng=None):
    """One outside wall as if it faced the lane (y=0, outward -y, from x=0 to `length`); returns its objects."""
    Hh = HOUSE
    t = KIT["wall_t"]
    H = Hh["floors"] * Hh["floor_h"] + Hh["plinth"]
    K = Kit(name)
    look = dict(frame=Hh["frame"], shutter=Hh["shutter"], door=Hh["door"], shutters=Hh["shutters"], boxes=Hh["boxes"])
    holes = []
    z0f = Hh["plinth"]
    for (x, sill, w, h) in holes_plan.get("ground", []):
        holes.append(window(K, rng, x, z0f + sill, w, h, t, look))
    for (x, sill, w, h) in holes_plan.get("first", []):
        holes.append(window(K, rng, x, z0f + Hh["floor_h"] + sill, w, h, t, look))
    if hall:
        x, sill, w, h = hall
        holes.append(window(K, rng, x, z0f + Hh["floor_h"] + sill - 0.3, w, h, t, dict(look, shutters=False, boxes=False)))   # tall, square-headed: the kit cuts only square holes in plaster
    for (x, w, h) in holes_plan.get("first_doors", []):                 # glazed doors at the upper floor (onto a balcony)
        holes.append(window(K, rng, x, z0f + Hh["floor_h"] + 0.02, w, h, t, dict(look, boxes=False)))
    if door_x is not None:
        hole = door(K, rng, door_x, door_w, door_h, t, look, steps=0)
        hole["z0"] = 0.0
        holes.append(hole)
    obs = []
    if Hh["walls"] == "stone":
        masonry(K, rng, 0.0, length, 0.0, H, holes, t)
    else:
        K.block("stone_dark", -0.04, length + 0.04, -0.06, t, 0.0, Hh["plinth"])                        # plinth
        wall = plaster_wall(name + "_Wall", K_M, 0.0, length, Hh["plinth"], H, t, holes, Hh["plaster"])
        obs.append(wall)
        md = wall.modifiers.get("Cut")
        if md and md.object:
            obs.append(md.object)                                  # the hole cutter turns with its wall, or the holes stay behind

        if Hh["quoins"]:
            quoins(K, 0.0, Hh["plinth"], H, -1, t - 0.1)             # stopped short of the inner face: not seen from inside
            quoins(K, length, Hh["plinth"], H, 1, t - 0.1)
    if Hh["string_course"]:
        band(K, 0.0, length, z0f + Hh["floor_h"] - 0.1, t - 0.1, steps=1)
    obs += K.flush(K_M)
    return obs


def place(obs, M):
    for ob in obs:
        ob.matrix_world = M @ ob.matrix_world


def to_col(obs, col):
    for ob in obs:
        for c in list(ob.users_collection):
            c.objects.unlink(ob)
        col.objects.link(ob)


def build():
    Hh = HOUSE
    W, D = Hh["W"], Hh["D"]
    fh, pl = Hh["floor_h"], Hh["plinth"]
    H = Hh["floors"] * fh + pl
    t = KIT["wall_t"]
    rng = random.Random(7)
    root = bpy.context.scene.collection
    cols = {k: bpy.data.collections.new(k) for k in ("Outside", "Roof", "Ground", "First", "Plan")}
    for c in cols.values():
        root.children.link(c)
    global COL
    COL = root
    # ---- the four outside walls: each built facing the lane, then turned into place
    front = facade("Front", W, Hh["front"], Hh["front"]["door"], porch=True, hall=Hh["front"]["hall"], rng=rng)
    bal = Hh["back"].get("balconies")
    c_land = (t + Hh["stair"]["side_land"]) / 2                                  # the middle of a corner landing, from its side wall
    back_plan = dict(Hh["back"], first_doors=[(x, bal["door_w"], bal["door_h"]) for x in (c_land, W - c_land)] if bal else [])
    back = facade("Back", W, back_plan, Hh["back"]["door"], door_h=2.3, door_w=1.0, rng=rng)
    place(back, Matrix.Translation((W, D, 0)) @ Matrix.Rotation(math.pi, 4, 'Z'))
    left = facade("Left", D, Hh["left"], None, rng=rng)
    place(left, Matrix.Translation((0, D, 0)) @ Matrix.Rotation(-math.pi / 2, 4, 'Z'))
    right = facade("Right", D, Hh["right"], None, rng=rng)
    place(right, Matrix.Translation((W, 0, 0)) @ Matrix.Rotation(math.pi / 2, 4, 'Z'))
    to_col(front + back + left + right, cols["Outside"])
    # ---- the back balconies: a stone slab on two brackets, an iron railing, one at each back corner
    if bal:
        K = Kit("Balconies")
        zf = pl + fh
        for (bx0, bx1) in ((0.0, bal["w"]), (W - bal["w"], W)):
            y1 = D + bal["d"]
            K.block("stone_dark", bx0, bx1, D, y1, zf - 0.18, zf, 0.02)                      # the slab, level with the floor inside
            for bx in (bx0 + 0.45, bx1 - 0.45):                                             # brackets under it
                K.prism("stone_dark", [(D, zf - 0.18), (D + bal["d"] - 0.2, zf - 0.18), (D, zf - 0.9)], bx - 0.12, bx + 0.12)
            rail = 1.0
            K.block("iron", bx0 + 0.03, bx1 - 0.03, y1 - 0.07, y1 - 0.02, zf + rail - 0.05, zf + rail, 0)           # top rail, outer edge
            for ex in (bx0 + 0.03, bx1 - 0.08):
                K.block("iron", ex, ex + 0.05, D, y1 - 0.02, zf + rail - 0.05, zf + rail, 0)                       # top rails, ends
            n = int((bx1 - bx0) / 0.14)
            for i in range(n + 1):                                                          # balusters along the front edge
                xx = bx0 + 0.05 + (bx1 - bx0 - 0.1) * i / n
                K.block("iron", xx - 0.012, xx + 0.012, y1 - 0.06, y1 - 0.035, zf, zf + rail, 0)
            for ex in (bx0 + 0.055, bx1 - 0.055):
                m = int(bal["d"] / 0.14)
                for i in range(1, m + 1):                                                   # and along the two ends
                    yy = D + (bal["d"] - 0.05) * i / (m + 1) + 0.05
                    K.block("iron", ex - 0.012, ex + 0.012, yy - 0.012, yy + 0.012, zf, zf + rail, 0)
        to_col(K.flush(K_M), cols["Outside"])
    # ---- the porch
    K = Kit("Porch")
    p = Hh["porch"]
    cx = Hh["front"]["door"]
    if p["kind"] == "columns":
        K.block("stone_dark", cx - p["w"] / 2 - 0.15, cx + p["w"] / 2 + 0.15, -p["d"] - 0.15, 0.0, 0.0, pl)          # its floor, at the plinth's height
        for sx in (-1, 1):
            K.cyl("cream", (cx + sx * (p["w"] / 2 - 0.25), -p["d"] + 0.25, pl + 1.35), "z", 0.17, 2.7, segs=14)
        K.block("stone_dark", cx - p["w"] / 2, cx + p["w"] / 2, -p["d"], 0.0, pl + 2.7, pl + 3.0)
        K.prism("gable", [(-p["d"] - 0.05, pl + 3.0), (0.0, pl + 3.0), (-p["d"] / 2, pl + 3.65)], cx - p["w"] / 2 - 0.1, cx + p["w"] / 2 + 0.1)
        for sx in (-1, 1):
            K.cube("tile_dark", (cx + sx * (p["w"] / 4 + 0.05), -p["d"] / 2, pl + 3.36), (p["w"] / 2 + 0.3, p["d"] + 0.3, 0.08),
                   rot=(0, sx * -math.atan(0.65 / (p["w"] / 2)), 0))
    elif p["kind"] == "canopy":
        K.block("stone_dark", cx - p["w"] / 2, cx + p["w"] / 2, -p["d"], 0.0, pl + 2.7, pl + 2.85)
        K.block("stone_dark", cx - 0.9, cx + 0.9, -0.6, 0.0, 0.0, pl)
    for k in range(3):                                                                   # steps down to the path
        K.block("stone_dark", cx - 0.9 + 0.0, cx + 0.9, -p["d"] - 0.15 - (k + 1) * 0.32, -p["d"] - 0.15 - k * 0.32, 0.0, pl - (k + 1) * pl / 3.4)
    to_col(K.flush(K_M), cols["Outside"])
    # ---- the roof: tiled, gabled, ridge along the front, chimneys on the gable ends
    K = Kit("Roof")
    tile_roof(K, rng, 0.0, W, D, H, chimneys=[(x, D / 2) for x in Hh["roof"]["chimneys"]])
    to_col(K.flush(K_M), cols["Roof"])
    # the body inside: floors, ceilings
    K0, K1 = Kit("Ground"), Kit("First")
    K0.block("wood", t, W - t, t, D - t, pl - 0.05, pl, 0)
    st = Hh["stair"]
    half = pl + fh / 2
    for (name, f, x0, y0, x1, y1, *_o) in Hh["rooms"]:
        if f == 1:
            K1.block("wood", x0 + 0.01, x1 - 0.01, y0 + 0.01, y1 - 0.01, pl + fh - 0.2, pl + fh, 0)
    hall = [r for r in Hh["rooms"] if r[0] == "Hall"][0]
    hw = (hall[4] - hall[2]) / 2                                                                     # half the hall's width
    sl = st["side_land"]
    K1.block("wood", t, sl, st["side_band"][0] - 0.1, st["side_band"][1] + 0.1, pl + fh - 0.2, pl + fh, 0)       # the side landings
    K1.block("wood", W - sl, W - t, st["side_band"][0] - 0.1, st["side_band"][1] + 0.1, pl + fh - 0.2, pl + fh, 0)
    # inner walls with door gaps, both floors
    for f, KK in ((0, K0), (1, K1)):
        z0, z1 = pl + f * fh, pl + f * fh + fh - (0.2 if f == 0 else 0)
        edges = set()
        for (name, ff, x0, y0, x1, y1, *_o) in Hh["rooms"]:
            if ff != f:
                continue
            opn = _o[0] if _o else ""
            for side, e in (("f", (x0, y0, x1, y0)), ("b", (x0, y1, x1, y1)), ("l", (x0, y0, x0, y1)), ("r", (x1, y0, x1, y1))):
                if side in opn:
                    continue                                                           # an open side: no wall (the hall to its stair)
                outer = (e[0] == e[2] and (e[0] < 0.01 or e[0] > W - 0.01)) or (e[1] == e[3] and (e[1] < 0.01 or e[1] > D - 0.01))
                if not outer:
                    edges.add(e)
        for (ex0, ey0, ex1, ey1) in edges:
            dz = [d for d in Hh["doors"] if d[0] == f and (abs(d[1] - ex0) < 0.01 and ex0 == ex1 and min(ey0, ey1) <= d[2] <= max(ey0, ey1) or
                                                         abs(d[2] - ey0) < 0.01 and ey0 == ey1 and min(ex0, ex1) <= d[1] <= max(ex0, ex1))]
            along_x = ey0 == ey1
            a0, a1 = (min(ex0, ex1), max(ex0, ex1)) if along_x else (min(ey0, ey1), max(ey0, ey1))
            cuts = sorted((d[1] if along_x else d[2]) for d in dz)
            at = a0
            segs = []
            for c in cuts:
                segs.append((at, c - 0.45, z0, z1)); segs.append((c - 0.45, c + 0.45, z0 + 2.1, z1)); at = c + 0.45
            segs.append((at, a1, z0, z1))
            for (s0, s1, za, zb) in segs:
                lo, hi = (t + 0.01, W - t - 0.01) if along_x else (t + 0.01, D - t - 0.01)   # inside the outer walls, never through them
                s0, s1 = max(s0, lo), min(s1, hi)
                if s1 - s0 < 0.02:
                    continue
                if along_x:
                    KK.block("plaster", s0, s1, ey0 - 0.06, ey0 + 0.06, za, zb, 0.01)
                else:
                    KK.block("plaster", ex0 - 0.06, ex0 + 0.06, s0, s1, za, zb, 0.01)
    # the stair: central flight, half landing, two side flights to the side walls
    n = st["steps"]
    cx = W / 2
    y_a, (yl0, yl1) = st["centre_from"], st["landing"]
    for k in range(n):
        y0 = y_a + (yl0 - y_a) * k / n
        top = pl + (half - pl) * (k + 1) / n
        K0.block("wood", cx - st["centre_w"] / 2, cx + st["centre_w"] / 2, y0, y0 + (yl0 - y_a) / n + 0.02, pl, top, 0.01)
    K0.block("wood", cx - hw, cx + hw, yl0, yl1, pl, half, 0.01)
    b0, b1 = st["side_band"]
    for side in (-1, 1):
        run = (W / 2 - sl) - hw                                                                    # from the hall to the side landing
        for k in range(n):
            u0 = hw + run * k / n
            top = half + (pl + fh - half) * (k + 1) / n
            xa, xb = (cx + u0, cx + u0 + run / n + 0.02) if side > 0 else (cx - u0 - run / n - 0.02, cx - u0)
            K0.block("wood", xa, xb, b0, b1, half, top, 0.01)
    for sx in (cx - st["centre_w"] / 2 - 0.05, cx + st["centre_w"] / 2 + 0.05):                       # handrails up the central flight
        K0.block("wood", sx - 0.03, sx + 0.03, y_a, yl0, pl + 0.9, pl + 0.96, 0.005)
    # furniture, simply: what each room is for (only without the furniture kits)
    for (KK, f) in (((K0, 0), (K1, 1)) if not Hh.get("furniture") else ()):
        z = pl + f * fh
        for (name, ff, x0, y0, x1, y1, *_o) in Hh["rooms"]:
            if ff != f:
                continue
            mx, my = (x0 + x1) / 2, (y0 + y1) / 2
            if name == "Living room":
                KK.block("dusty", x0 + 0.6, x0 + 1.5, y0 + 1.5, y0 + 4.0, z, z + 0.8, 0.05)              # sofa
                KK.block("wood", x0 + 2.4, x0 + 3.4, y0 + 2.2, y0 + 3.3, z, z + 0.45, 0.02)             # low table
                KK.block("stone_dark", x0 + t, x0 + t + 0.4, y0 + 2.2, y0 + 3.4, z, z + 1.2)          # fireplace on the side wall
                KK.block("terra", x0 + 1.8, x0 + 4.0, y0 + 1.6, y0 + 3.9, z, z + 0.02, 0)               # rug
            elif name == "Kitchen":
                KK.block("wood", x1 - 0.9, x1 - t, y0 + 0.8, y1 - 0.6, z, z + 0.9, 0.02)                # worktop run
                KK.block("wood", mx - 0.9, mx + 0.5, my - 0.5, my + 0.5, z, z + 0.76, 0.02)             # table
            elif "edroom" in name:
                big = name == "Main bedroom"
                bx0 = x0 + t + 0.1 if x0 < 0.5 else x1 - t - 2.1
                KK.block("cream", bx0, bx0 + 2.0, my - (0.8 if big else 0.5), my + (0.8 if big else 0.5), z, z + 0.55, 0.04)
                KK.block("wood", (x0 + x1) / 2 - 0.8, (x0 + x1) / 2 + 0.8, y0 + 0.15, y0 + 0.75, z, z + 2.0, 0.02)   # wardrobe
            elif name in ("Bathroom", "En-suite"):
                KK.block("cream", x0 + t + 0.1, x0 + t + 1.8, y1 - 1.0, y1 - t - 0.05, z, z + 0.55, 0.04)
            elif name == "Dining area":
                KK.block("wood", x0 + 1.2, x0 + 3.2, y1 - 2.0, y1 - 1.0, z, z + 0.76, 0.02)                # table for six
                for cxx in (x0 + 1.6, x0 + 2.2, x0 + 2.8):
                    for cyy in (y1 - 2.35, y1 - 0.65):
                        KK.block("wood", cxx - 0.2, cxx + 0.2, cyy - 0.2, cyy + 0.2, z, z + 0.45, 0.02)
            elif name == "Back kitchen":
                KK.block("wood", x1 - 0.9, x1 - t, y0 + 2.0, y1 - 1.6, z, z + 0.9, 0.02)                # the worktop runs on
    to_col(K0.flush(K_M), cols["Ground"])
    to_col(K1.flush(K_M), cols["First"])
    # room names, flat on each floor (shown only in the plans)
    for (name, f, x0, y0, x1, y1, *_o) in Hh["rooms"]:
        cu = bpy.data.curves.new("T_" + name, 'FONT')
        cu.body = name
        cu.size = 0.42
        cu.align_x, cu.align_y = 'CENTER', 'CENTER'
        ob = bpy.data.objects.new("T_%s_%d" % (name, f), cu)
        ob.location = ((x0 + x1) / 2, (y0 + y1) / 2, pl + f * fh + 0.05 + (2.2 if f == 0 else 0.0))
        ob.data.materials.append(K_M["dark"])
        cols["Plan"].objects.link(ob)
        ob["floor"] = f
    # the plot: lawn, the lane in front, a path to the door, a low hedge
    Kg = Kit("Plot")
    Kg.block("plant", -8, W + 8, -6.5, D + 10, -0.05, 0.0, 0)
    Kg.block("paving", -30, W + 30, LANE_Y - 3.5, LANE_Y + 3.5, -0.03, 0.02, 0)
    Kg.block("paving", cx - 0.8, cx + 0.8, -6.5, -Hh["porch"]["d"] - 1.1, -0.02, 0.03, 0)
    for (a0, a1) in ((-8, cx - 1.0), (cx + 1.0, W + 8)):
        Kg.block("plant", a0, a1, -6.6, -6.0, 0.0, 0.7, 0.08)
    to_col(Kg.flush(K_M), cols["Outside"])
    if Hh.get("furniture"):
        furnish(cols)
    tree("BackTree", W + 4.0, D + 5.0, K_M, h=4.0, s=1.2)
    tree("SideTree", -4.5, D - 1.0, K_M, h=3.6, s=1.0)
    return cols, H


# ================================================================== the furniture: kits 17 and 18 (Tools/blender/furniture_kit_17.py and
# furniture_kit_18.py, main 2026-11-22), in one of their four lines. Each piece is (floor, piece, x, y, yaw) in the house's frame;
# yaw 0 stands a piece with its back to a wall at +y (facing the front), pi/2 against a wall at low x, -pi/2 at high x, pi at low y.
# "k17:<kind>" is a kit 17 piece (sofa armchair coffee dining bed bedside wardrobe bookcase floor_lamp rug), the rest kit 18's.
FURNITURE = [
    # living room: fireplace on the side wall between its windows, the sofa facing it with its back to the hall, two armchairs
    (0, "l_fireplace", 0.54, 4.0, math.pi / 2), (0, "o_painting", 0.34, 4.0, math.pi / 2), (0, "k17:rug", 2.3, 4.0, math.pi / 2),
    (0, "k17:sofa", 3.85, 4.0, -math.pi / 2), (0, "l_sidetable", 3.95, 5.45, 0), (0, "k17:coffee", 2.3, 4.0, math.pi / 2),
    (0, "k17:armchair", 1.9, 2.15, math.pi), (0, "k17:armchair", 1.9, 5.85, 0), (0, "l_piano", 2.0, 7.6, 0),
    (0, "k17:floor_lamp", 3.95, 7.35, 0), (0, "o_floorplant", 0.75, 0.75, 0),
    # hall: console and mirror on the living-room wall, shoe bench and hooks on the kitchen wall, a coat stand by the door
    (0, "h_console", 4.64, 3.4, math.pi / 2), (0, "h_mirror", 4.42, 3.4, math.pi / 2), (0, "h_shoebench", 8.36, 3.4, -math.pi / 2),
    (0, "h_hooks", 8.58, 3.4, -math.pi / 2), (0, "h_coatstand", 8.15, 0.75, 0), (0, "h_umbrella", 4.8, 0.75, 0), (0, "k17:rug", 6.5, 2.3, 0),
    # kitchen: counters, sink and cooker along the side wall, the fridge on the back wall, a table in the middle under a lamp
    (0, "k_counter", 12.36, 1.3, -math.pi / 2), (0, "k_sink", 12.36, 2.75, -math.pi / 2), (0, "k_stove", 12.37, 3.75, -math.pi / 2),
    (0, "k_wall", 12.53, 3.75, -math.pi / 2), (0, "k_counter", 12.36, 5.0, -math.pi / 2), (0, "k_fridge", 9.45, 7.5, 0),
    (0, "k_potshelf", 8.84, 5.6, math.pi / 2), (0, "k17:dining", 10.3, 3.0, math.pi / 2), (0.35, "d_pendant", 10.3, 3.0, 0),
    # en-suite: bath under the front window, WC on the hall wall, basin and mirror on the bedroom wall
    (1, "b_bath", 1.3, 0.77, math.pi), (1, "b_toilet", 4.02, 1.15, -math.pi / 2), (1, "b_basin", 1.3, 2.24, 0),
    (1, "b_mirror", 1.3, 2.52, 0), (1, "b_basket", 3.95, 2.15, 0),
    # main bedroom: the bed's head on the hall wall, bedsides, wardrobe on the back wall, a chest of drawers by the side window
    (1, "k17:rug", 2.3, 5.0, math.pi / 2), (1, "k17:bed", 3.26, 5.0, -math.pi / 2), (1, "k17:bedside", 4.1, 3.85, -math.pi / 2),
    (1, "k17:bedside", 4.1, 6.15, -math.pi / 2), (1, "k17:wardrobe", 2.7, 7.57, 0), (1, "r_chest", 0.55, 3.4, math.pi / 2),
    # bedroom 2: a single bed, head to the side wall, wardrobe on the hall wall, a desk under the front window, a chest
    (1, "k17:rug", 10.6, 4.0, 0), (1, "r_single", 11.68, 4.0, -math.pi / 2), (1, "k17:bedside", 12.45, 4.9, -math.pi / 2),
    (1, "k17:wardrobe", 8.97, 3.0, math.pi / 2), (1, "s_desk", 10.6, 0.67, math.pi), (1, "s_chair", 10.6, 1.3, 0),
    (1, "r_chest", 9.9, 7.57, 0),
    # the balconies: a bistro table and chairs on each
    (1, "g_bistro", 1.6, 10.65, 0), (1, "g_bistro", 11.4, 10.65, 0),
]


def furnish(cols):
    """Places FURNITURE in the line HOUSE["furniture"], with the kits' own parts and materials."""
    src = open(B + "furniture_kit_18.py", encoding="utf-8").read()
    src = src[:src.rstrip().rfind("\nmain()")]
    FK = {"__name__": "furniture_kits"}
    exec(compile(src, "furniture_kit_18_helpers", "exec"), FK)      # its own namespace: the kits keep their globals, the house keeps its
    FK["K_M"] = K_M
    M = dict(K_M)
    M.update(boards=FK["painted"]("M_Boards", (0.26, 0.17, 0.10), (0.42, 0.29, 0.17), brick_scale=0.25, floor=True, bump=0.3, noise_scale=1.6),
             shade=FK["emissive"]("M_Shade", (1.0, 0.72, 0.45), 1.6),
             dusty=FK["flat"]("M_FurnDusty", (0.30, 0.38, 0.52)), olive=FK["flat"]("M_FurnOlive", (0.30, 0.33, 0.20)),
             plum=FK["flat"]("M_FurnPlum", (0.30, 0.24, 0.34)))
    FK["M"] = M
    FK["extra_mats"]()
    L = [l for l in FK["lines"]() if l["key"] == HOUSE["furniture"]][0]
    rng = random.Random(18)
    pl, fh = HOUSE["plinth"], HOUSE["floor_h"]
    n = 0
    for i, (f, spec, x, y, yaw) in enumerate(FURNITURE):
        lift = f - int(f)                                          # a pendant hangs a little higher under the taller ceiling
        f = int(f)
        FK["COL"] = cols["First" if f else "Ground"]
        P = FK["group"]("F%02d_%s" % (i, spec.replace(":", "_")), x, y, yaw)
        P.location.z = pl + f * fh + lift
        FK["place"](spec, P, L, rng)
        n += 1
    log("furnished: %d pieces in the %s line" % (n, L["label"]))


def clear_glass():
    m = bpy.data.materials.new("M_HouseGlass")
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    mix = nt.nodes.new("ShaderNodeMixShader")
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    gl = nt.nodes.new("ShaderNodeBsdfGlossy")
    tr.inputs[0].default_value = (0.86, 0.9, 0.92, 1.0)
    gl.inputs[0].default_value = (0.8, 0.85, 0.9, 1.0)
    gl.inputs[1].default_value = 0.04
    mix.inputs[0].default_value = 0.18
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(gl.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs[0])
    m.diffuse_color = (0.3, 0.35, 0.4, 1.0)
    return m


def cams(H):
    W, D = HOUSE["W"], HOUSE["D"]
    sc = bpy.context.scene
    out = [("front", camera(sc, "C_front", (W / 2 - 7.5, -23.0, 2.2), (W / 2, D / 2 - 2, H * 0.5), 34.0), (1200, 800), "all"),
           ("front_straight", camera(sc, "C_frontS", (W / 2, -26.0, 4.0), (W / 2, 0, H * 0.5), 38.0), (1200, 800), "all"),
           ("back", camera(sc, "C_back", (W / 2 + 8.0, D + 21.0, 2.4), (W / 2, D / 2, H * 0.5), 34.0), (1200, 800), "all"),
           ("side", camera(sc, "C_side", (W + 19.0, D / 2 - 7.0, 2.4), (W / 2, D / 2, H * 0.5), 34.0), (1200, 800), "all"),
           ("above", camera(sc, "C_above", (W / 2 - 16.0, -20.0, 19.0), (W / 2, D / 2, 2.5), 32.0), (1200, 800), "all")]
    for key, f in (("plan_ground", 0), ("plan_first", 1)):
        cd = bpy.data.cameras.new("C_" + key)
        cd.type = 'ORTHO'
        cd.ortho_scale = W + 3.0
        co = bpy.data.objects.new("C_" + key, cd)
        co.location = (W / 2, D / 2, 30.0)
        bpy.context.scene.collection.objects.link(co)
        out.append((key, co, (1200, 1000), f))
    pl, fh = HOUSE["plinth"], HOUSE["floor_h"]
    for key, eye, at in (("room_living", (4.1, 0.7, pl + 1.6), (0.8, 5.2, pl + 0.9)),
                         ("room_kitchen", (8.9, 0.8, pl + 1.6), (12.0, 5.0, pl + 0.9)),
                         ("room_hall", (6.5, 0.6, pl + 1.6), (6.5, 8.0, pl + 2.6)),
                         ("room_bedroom", (0.7, 7.3, pl + fh + 1.6), (3.8, 3.6, pl + fh + 0.6)),
                         ("room_ensuite", (2.6, 2.9, pl + fh + 1.6), (1.6, 0.6, pl + fh + 0.6)),
                         ("room_bedroom2", (9.0, 7.4, pl + fh + 1.6), (11.8, 2.0, pl + fh + 0.6))):
        out.append((key, camera(sc, "C_" + key, eye, at, 18.0), (1200, 800), "all"))
    return out


def show(cols, mode):
    """all: the house from outside; 0 / 1: the plan of that floor (roof and the floor above taken away)."""
    def vis(c, on):
        c.hide_render = not on
    if mode == "all":
        vis(cols["Outside"], True); vis(cols["Roof"], True); vis(cols["Ground"], True); vis(cols["First"], True); vis(cols["Plan"], False)
    else:
        vis(cols["Outside"], True); vis(cols["Roof"], False); vis(cols["Ground"], True); vis(cols["First"], mode == 1); vis(cols["Plan"], True)
        for ob in cols["Plan"].objects:
            ob.hide_render = ob["floor"] != mode
        for ob in cols["Outside"].objects:                    # the upper half of the outside walls goes for the ground plan
            ob.hide_render = ob.name.endswith("_Cut")         # the cutters that make the window holes stay unseen


def configure_h(scene, res):
    r = scene.render
    setp(r, "engine", 'CYCLES')
    c = getattr(scene, "cycles", None)
    if c is not None:
        setp(c, "device", 'CPU'); setp(c, "samples", SAMPLES); setp(c, "use_adaptive_sampling", True)
        setp(c, "adaptive_threshold", 0.05); setp(c, "use_denoising", True); setp(c, "max_bounces", 5)
    r.resolution_x, r.resolution_y = res
    r.resolution_percentage = SCALE
    setp(r.image_settings, "file_format", 'PNG')
    vs = scene.view_settings
    setp(vs, "view_transform", 'AgX'); setp(vs, "look", 'AgX - Medium High Contrast'); setp(vs, "exposure", -0.15)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(SRC, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    try:
        t0 = time.time()
        scene = new_scene("House01")
        global K_M
        K_M = kit_mats()
        K_M["gable"] = K_M[HOUSE["plaster"]]                     # the roof's gable ends and the porch pediment in the wall's plaster
        K_M["glass"] = clear_glass()                              # glass you see through and daylight comes in by (the street kit's is opaque)
        cols, H = build()
        sun_and_sky(scene, (0.62, -0.55, 0.56), 8.0, (1.0, 0.88, 0.72), (0.42, 0.55, 0.82), 0.4)
        log("built in %.1fs: %d objects" % (time.time() - t0, len(scene.objects)))
        try:
            bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
        except Exception:
            log("save failed:\n" + traceback.format_exc())
        for key, cam, res, mode in cams(H):
            if ONLY and key not in ONLY:
                continue
            try:
                show(cols, mode)
                scene.camera = cam
                configure_h(scene, res)
                if key.startswith("room_"):                               # indoors: the eye gets used to the dimmer light
                    setp(scene.view_settings, "exposure", dict(room_kitchen=1.2, room_living=1.8, room_bedroom2=2.0).get(key, 2.6))
                scene.render.filepath = OUT_DIR + "/" + NAME + "_" + key + ".png"
                t = time.time()
                bpy.ops.render.render(write_still=True, scene=scene.name)
                log("%s rendered in %.1fs" % (key, time.time() - t))
            except Exception:
                log("FAILED %s:\n%s" % (key, traceback.format_exc()))
    except Exception:
        log("FAILED:\n" + traceback.format_exc())
    log("DONE")


main()
