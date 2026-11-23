"""Brix Town - house interiors 21, the plan: lays out the sample house (12 x 9 m, the size of the town's houses in
World/city_01, add_houses_03.py) and furnishes every room with the home pieces of furniture kits 17 and 18, each piece
marked with the furnishing level it first comes in at:

    1 Bare      what a household cannot do without: beds, a sofa, a table to eat at, the kitchen and bathroom
    2 Basic     storage and comfort: wardrobes, bedside tables, armchair, desk, lamps, the children's bunk
    3 Homely    rugs, shelves, television, curtains, pictures, the hall furniture
    4 Full      every piece of every set and the decoration

A house at level n has every piece of level n or lower. Runs in ordinary Python (no Blender), reading the sizes measured
by furniture_sizes_21.py, so a layout can be changed and checked in a second:
    python Tools/blender/house_interiors_21_plan.py
It checks that every piece is inside its room, that no two pieces stand in each other, that doorways, the front door
and the stair stay clear and that nothing tall stands in front of a window, and draws each storey from above at each
level. Writes Art/StyleTests/house_interiors_21/layout.json (read by house_interiors_21.py) and plan_<storey>.png.
Exits with an error when a check fails.
"""
import json, math, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DIR = os.path.join(ROOT, "Art", "StyleTests", "house_interiors_21")
SIZES = json.load(open(os.path.join(DIR, "furniture_sizes.json"), encoding="utf-8"))
LEVELS = {1: "Bare", 2: "Basic", 3: "Homely", 4: "Full"}
P2 = math.pi / 2

# ---- the plan (metres, the house's own frame: x across the front, left to right seen from the street; y from the front
# wall at -4.5 to the back at +4.5) ----------------------------------------------------------------------------------
W0, W1, D0, D1 = -6.0, 6.0, -4.5, 4.5
T, TP = 0.25, 0.12                     # outside walls, partitions
XA, XB, YM = -1.0, 1.5, 0.6            # hall between XA and XB; front and back rooms split at YM
H = 3.0
STAIR = (XB - TP / 2 - 1.0, XB - TP / 2, 1.55, D1 - T)   # x0, x1, y0, y1: against the hall's right wall, rising to the back
FRONT_DOOR = (-0.4, 0.6)                                  # the hole in the front wall (house_samples_19's storey_shell)
DOORS = {                              # (wall, at, centre along it): every doorway 0.9 wide
    "ground": [("x", XA, -2.6), ("x", XA, 2.4), ("x", XB, -2.6), ("x", XB, 1.0)],
    "upstairs": [("x", XA, -2.6), ("x", XA, 2.4), ("x", XB, -2.6), ("x", XB, 1.0)],
}
WINDOWS = [("front", -4.3, 1.3), ("front", -2.6, 1.3), ("front", 2.7, 1.3), ("front", 4.6, 1.3),
           ("left", -2.0, 1.0), ("left", 2.4, 1.0), ("right", -2.0, 1.0), ("right", 2.4, 1.0), ("back", 3.6, 0.9)]
SILL = 0.9                             # windows from 0.9 to 2.3 above the floor
i0, i1, j0, j1 = W0 + T, W1 - T, D0 + T, D1 - T          # inside faces of the outside walls
a, b, m = XA - TP / 2, XB + TP / 2, YM                    # faces of the partitions
ROOMS = {
    "ground": {"Living room": (i0, a, j0, m - TP / 2), "Kitchen": (i0, a, m + TP / 2, j1), "Hall": (a + TP, b - TP, j0, j1),
               "Dining room": (b, i1, j0, m - TP / 2), "Bathroom": (b, i1, m + TP / 2, j1)},
    "upstairs": {"Main bedroom": (i0, a, j0, m - TP / 2), "Study": (i0, a, m + TP / 2, j1), "Landing": (a + TP, b - TP, j0, j1),
                 "Children's room": (b, i1, j0, m - TP / 2), "Guest room": (b, i1, m + TP / 2, j1)},
}

# where people lie and sit on a piece: [x, y, height, facing x, facing y] in the piece's own frame (yaw 0, back towards +y)
SPOTS = {"k17:bed": [[-0.4, 0.2, 0.62, 0, 1], [0.4, 0.2, 0.62, 0, 1]], "c_bunk": [[0, 0.1, 0.5, 0, 1], [0, 0.1, 1.45, 0, 1]],
         "r_single": [[0, 0.2, 0.62, 0, 1]], "k17:sofa": [[-0.5, 0, 0.45, 0, -1], [0.5, 0, 0.45, 0, -1]],
         "k17:armchair": [[0, 0, 0.45, 0, -1]], "k17:dining": [[0, -0.7, 0.45, 0, 1], [0, 0.7, 0.45, 0, -1]]}
BEDS_OF = ("k17:bed", "c_bunk", "r_single")

WALL = {"k_wall", "b_mirror", "h_mirror", "h_hooks", "s_pinboard", "o_painting", "o_clock", "o_curtains", "o_wallshelf"}
CEILING = {"d_pendant"}
RUG = {"k17:rug"}


def box_of(spec):
    """The piece's footprint about its origin at yaw 0, the largest over the four lines, and its height."""
    bs = [v for v in SIZES[spec].values() if v]
    return min(v[0] for v in bs), max(v[1] for v in bs), min(v[2] for v in bs), max(v[3] for v in bs), max(v[4] for v in bs)


def turned(spec, x, y, yaw):
    """The footprint in the house's frame (an axis-aligned box round the turned piece)."""
    x0, x1, y0, y1, h = box_of(spec)
    c, s = math.cos(yaw), math.sin(yaw)
    pts = [(x + u * c - v * s, y + u * s + v * c) for u in (x0, x1) for v in (y0, y1)]
    return min(p[0] for p in pts), max(p[0] for p in pts), min(p[1] for p in pts), max(p[1] for p in pts)


ITEMS = []                             # dicts: storey, room, piece, x, y, yaw, level, lift


def put(storey, room, spec, x, y, yaw, level, lift=0.0):
    ITEMS.append(dict(storey=storey, room=room, piece=spec, x=round(x, 3), y=round(y, 3), yaw=round(yaw, 4), level=level, lift=lift))


def wall(storey, room, side, spec, at, level, gap=0.03, lift=0.0, out=0.0):
    """A piece with its back to a wall of its room: side back (+y), front (-y), left (-x) or right (+x); `at` is where its
    middle is along that wall; `out` brings it further into the room."""
    x0r, x1r, y0r, y1r = ROOMS[storey][room]
    px0, px1, py0, py1, h = box_of(spec)
    g = gap + out
    if side == "back":
        put(storey, room, spec, at, y1r - g - py1, 0.0, level, lift)
    elif side == "front":
        put(storey, room, spec, at, y0r + g + py1, math.pi, level, lift)
    elif side == "left":
        put(storey, room, spec, x0r + g + py1, at, P2, level, lift)
    else:
        put(storey, room, spec, x1r - g - py1, at, -P2, level, lift)


def run(storey, room, side, specs, levels, start=0.02, gap=0.01):
    """Pieces side by side along the back wall from its left end (a kitchen run), each as wide as it measures."""
    x = ROOMS[storey][room][0] + start
    for spec, lv in zip(specs, levels):
        x0, x1 = box_of(spec)[:2]
        wall(storey, room, side, spec, x - x0, lv)
        x += (x1 - x0) + gap


def room_mid(storey, room):
    x0, x1, y0, y1 = ROOMS[storey][room]
    return (x0 + x1) / 2, (y0 + y1) / 2


# ---- ground floor ---------------------------------------------------------------------------------------------------
G, U = "ground", "upstairs"
# living room: the fireplace on the back wall, the sofa facing it across a rug, the television by the hall door
L, lx = "Living room", -3.45
wall(G, L, "front", "k17:sofa", lx, 1, out=1.55)              # sofa standing in the room, its back to the front windows
wall(G, L, "back", "l_fireplace", lx, 4)
wall(G, L, "back", "l_tv", -1.85, 3)
wall(G, L, "right", "k17:armchair", -1.0, 2)
put(G, L, "k17:coffee", lx, -1.05, 0.0, 2)
put(G, L, "k17:rug", lx, -1.25, 0.0, 3)
wall(G, L, "left", "k17:bookcase", -3.65, 3)
wall(G, L, "left", "l_piano", -0.45, 4)
put(G, L, "k17:floor_lamp", -5.2, -2.85, 0.0, 2)
put(G, L, "l_sidetable", -4.8, -2.2, 0.0, 4)
put(G, L, "l_magrack", lx, -3.15, 0.0, 4)
put(G, L, "o_floorplant", -1.5, -3.8, 0.0, 4)
wall(G, L, "back", "o_painting", -1.85, 3, gap=0.0)      # over the television (the fireplace has its own breast)
put(G, L, "d_pendant", lx, -1.25, 0.0, 2)
put(G, L, "o_candles", lx - 0.4, 0.2, 0.0, 4, lift=1.12)
# kitchen: fridge, counter, sink and cooker along the back wall under the wall cupboards; the table in the middle
K = "Kitchen"
kx0 = ROOMS[G][K][0]
run(G, K, "back", ["k_fridge", "k_counter", "k_sink", "k_stove"], [1, 1, 1, 1])
wall(G, K, "back", "k_wall", kx0 + 0.81 + 0.62, 2, gap=0.0)
wall(G, K, "back", "k_wall", kx0 + 0.81 + 1.27 + 0.62, 3, gap=0.0)
put(G, K, "k17:dining", -3.3, 1.95, P2, 1)
put(G, K, "d_pendant", -3.3, 1.95, 0.0, 2)
wall(G, K, "left", "k_potshelf", 1.35, 3)
wall(G, K, "right", "o_clock", 3.55, 4, gap=0.0, lift=0.0)
put(G, K, "o_smallplants", -4.6, 3.58, 0.0, 4, lift=0.9)
# hall: coats and shoes by the front door, a console and mirror between the doors, the stair up the right wall
Hh = "Hall"
put(G, Hh, "h_coatstand", 1.08, -3.85, 0.0, 3)
wall(G, Hh, "left", "h_shoebench", -1.6, 3)
wall(G, Hh, "left", "h_hooks", -1.55, 4, gap=0.0)
wall(G, Hh, "left", "h_console", 0.9, 3)
wall(G, Hh, "left", "h_mirror", 0.9, 4, gap=0.0)
put(G, Hh, "h_umbrella", -0.65, -4.0, 0.0, 4)
put(G, Hh, "k17:rug", 0.1, -1.7, P2, 4)
put(G, Hh, "d_pendant", 0.25, -1.8, 0.0, 2)
# dining room: the table under a lamp, the sideboard on the back wall, the dresser and display cabinet on the sides
Dn, dx = "Dining room", 3.75
put(G, Dn, "k17:dining", dx, -1.95, 0.0, 2)
put(G, Dn, "d_pendant", dx, -1.95, 0.0, 2)
wall(G, Dn, "back", "d_sideboard", dx, 2)
wall(G, Dn, "right", "d_dresser", -0.45, 3)
wall(G, Dn, "left", "d_display", -0.55, 4)
wall(G, Dn, "right", "d_bench", -3.4, 4)
wall(G, Dn, "back", "o_painting", dx, 3, gap=0.0)
put(G, Dn, "o_candles", dx + 0.45, 0.25, 0.0, 4, lift=0.91)
put(G, Dn, "o_vases", dx - 0.5, 0.22, 0.0, 4, lift=0.91)
put(G, Dn, "o_floorplant", 2.0, -1.55, 0.0, 4)
# bathroom: the bath under the back window, the basin and mirror, the toilet by the front wall
B = "Bathroom"
wall(G, B, "back", "b_bath", 4.82, 1)
wall(G, B, "back", "b_basin", 2.3, 1)
wall(G, B, "back", "b_mirror", 2.3, 2, gap=0.0)
wall(G, B, "front", "b_toilet", 4.9, 1)
wall(G, B, "right", "b_rail", 1.25, 2)
put(G, B, "b_basket", 3.4, 1.05, math.pi, 3)
put(G, B, "o_floorplant", 2.05, 3.0, 0.0, 4)
put(G, B, "d_pendant", 3.65, 2.45, 0.0, 2)

# ---- upstairs -------------------------------------------------------------------------------------------------------
# main bedroom: the double bed against the back wall between its bedside tables, the wardrobe and chest on the walls
R, rx = "Main bedroom", -3.45
wall(U, R, "back", "k17:bed", rx, 1)
wall(U, R, "back", "k17:bedside", rx - 1.15, 2)
wall(U, R, "back", "k17:bedside", rx + 1.15, 2)
wall(U, R, "left", "k17:wardrobe", -3.55, 2)
wall(U, R, "left", "r_chest", -0.6, 3)
wall(U, R, "right", "r_vanity", -0.7, 4)
put(U, R, "r_blanket", rx, -2.2, 0.0, 4)
put(U, R, "k17:rug", rx, -1.95, 0.0, 3)
wall(U, R, "back", "o_painting", rx, 4, gap=0.0)
put(U, R, "d_pendant", rx, -1.6, 0.0, 2)
# study: the desk on the back wall with its lamp and pinboard, filing cabinet, shelving, an armchair to read in
S = "Study"
wall(U, S, "back", "s_desk", -3.6, 2)
put(U, S, "s_chair", -3.6, 3.15, math.pi, 2)
put(U, S, "s_lamp", -4.0, 3.95, 0.0, 3, lift=0.75)
wall(U, S, "back", "s_pinboard", -3.6, 3, gap=0.0, lift=0.0)
wall(U, S, "back", "s_filing", -2.4, 3)
wall(U, S, "back", "k17:bookcase", -5.05, 3)
wall(U, S, "left", "s_shelving", 1.2, 4)
wall(U, S, "right", "k17:armchair", 1.2, 3)
put(U, S, "k17:floor_lamp", -2.45, 0.95, 0.0, 4)
put(U, S, "k17:rug", -3.5, 1.9, 0.0, 4)
put(U, S, "o_globe", -4.6, 0.95, 0.0, 4)
put(U, S, "d_pendant", -3.4, 2.4, 0.0, 2)
# children's room: the bunk in the back corner, the cot by the hall wall, table and stools on a rug, toys
C = "Children's room"
put(U, C, "c_bunk", 4.6, ROOMS[U][C][3] - 0.03 - 0.54, -P2, 1)        # in the back corner, lying along the back wall
wall(U, C, "left", "c_cot", -0.75, 4)
wall(U, C, "back", "c_toyshelf", 2.4, 3)
put(U, C, "c_table", 3.4, -2.3, 0.0, 3)
put(U, C, "k17:rug", 3.5, -2.2, 0.0, 3)
wall(U, C, "right", "c_toychest", -3.4, 3)
put(U, C, "c_horse", 2.35, -3.6, 0.4, 4)
wall(U, C, "back", "o_painting", 2.4, 4, gap=0.0)
put(U, C, "d_pendant", 3.5, -1.9, 0.0, 2)
# guest room: a single bed with its head to the front wall, a bedside table, chest, screen and mirror
Gr = "Guest room"
wall(U, Gr, "front", "r_single", 4.85, 2)
wall(U, Gr, "front", "k17:bedside", 3.95, 3)
wall(U, Gr, "left", "r_chest", 3.4, 3)
put(U, Gr, "r_screen", 2.5, 2.2, P2, 4)
put(U, Gr, "r_cheval", 5.3, 3.8, 0.25, 4)
wall(U, Gr, "back", "o_curtains", 3.6, 4, gap=0.0)
put(U, Gr, "d_pendant", 3.6, 2.4, 0.0, 2)
# landing
Ld = "Landing"
wall(U, Ld, "left", "o_wallshelf", -0.5, 4, gap=0.0, lift=0.0)
put(U, Ld, "o_floorplant", -0.45, -3.75, 0.0, 4)
put(U, Ld, "o_bust", 1.15, -3.85, 0.0, 4)
put(U, Ld, "d_pendant", 0.25, -1.6, 0.0, 2)


PLANS_NOTE = ("The plans homes are furnished from (FURN-060, FURN-061, ADR-0033): per plan the house (walls, doorways, windows, "
              "stair, rooms; metres in the house's own frame, x across the front left to right seen from the street, y from the "
              "front to the back) and the pieces of furniture kits 17 and 18 in each room with their place, turn, lift and the "
              "furnishing level each first comes in at (1 Bare, 2 Basic, 3 Homely, 4 Full; a home shows its level and those "
              "below, FURN-062). Written by the Blender planners after their checks pass (house_interiors_21_plan.py); do not "
              "edit by hand. Read by Prototype/play/houses.js, Tools/worldgen/add_houses_04.py and the Blender renderers.")


# ---- checks ---------------------------------------------------------------------------------------------------------
def kind(spec):
    return "wall" if spec in WALL else "ceiling" if spec in CEILING else "rug" if spec in RUG else "floor"


def overlap(p, q, e=0.005):
    return p[0] < q[1] - e and q[0] < p[1] - e and p[2] < q[3] - e and q[2] < p[3] - e


def keep_clear(storey):
    """Floor areas no furniture may stand on: 0.85 m both sides of every doorway, inside the front door, the stair and
    the 0.9 m at its foot (and round the well upstairs)."""
    out = []
    for ax, at, c in DOORS[storey]:
        out.append(("doorway %s %.1f" % (ax, c), (at - 0.9, at + 0.9, c - 0.45, c + 0.45) if ax == "x" else (c - 0.45, c + 0.45, at - 0.9, at + 0.9)))
    x0, x1, y0, y1 = STAIR
    if storey == "ground":
        out.append(("front door", (FRONT_DOOR[0], FRONT_DOOR[1], j0, j0 + 0.9)))
        out.append(("stair", (x0, x1, y0, y1)))
        out.append(("stair foot", (x0, x1, y0 - 0.9, y0)))
    else:
        out.append(("stair well", (x0 - 0.05, x1, y0, y1)))
    return out


def window_strips():
    out = []
    for side, c, w in WINDOWS:
        if side == "front": out.append((side, (c - w / 2, c + w / 2, j0, j0 + 0.3)))
        elif side == "back": out.append((side, (c - w / 2, c + w / 2, j1 - 0.3, j1)))
        elif side == "left": out.append((side, (i0, i0 + 0.3, c - w / 2, c + w / 2)))
        else: out.append((side, (i1 - 0.3, i1, c - w / 2, c + w / 2)))
    return out


def check():
    errors = []
    by = {}
    for n, it in enumerate(ITEMS):
        fp = turned(it["piece"], it["x"], it["y"], it["yaw"])
        it["_fp"], it["_n"] = fp, n
        r = ROOMS[it["storey"]][it["room"]]
        if kind(it["piece"]) != "ceiling" and not (fp[0] >= r[0] - .01 and fp[1] <= r[1] + .01 and fp[2] >= r[2] - .01 and fp[3] <= r[3] + .01):
            errors.append("%s %s: outside the room by %.2f m" % (it["room"], it["piece"], max(r[0] - fp[0], fp[1] - r[1], r[2] - fp[2], fp[3] - r[3])))
        by.setdefault(it["storey"], []).append(it)
    for storey, its in by.items():
        floor = [it for it in its if kind(it["piece"]) == "floor" and not it["lift"]]
        for i, p in enumerate(floor):
            for q in floor[i + 1:]:
                if overlap(p["_fp"], q["_fp"]):
                    errors.append("%s: %s and %s stand in each other" % (p["room"], p["piece"], q["piece"]))
            for name, z in keep_clear(storey):
                if overlap(p["_fp"], z):
                    errors.append("%s: %s blocks the %s" % (p["room"], p["piece"], name))
            if box_of(p["piece"])[4] > SILL + 0.15 and p["piece"] not in ("b_toilet",):
                for side, z in window_strips():
                    if overlap(p["_fp"], z):
                        errors.append("%s: %s (%.2f m tall) stands in front of a %s window" % (p["room"], p["piece"], box_of(p["piece"])[4], side))
        walls = [it for it in its if kind(it["piece"]) == "wall" and it["piece"] != "o_curtains"]
        for p in walls:
            for side, z in window_strips():
                if overlap(p["_fp"], z):
                    errors.append("%s: %s hangs over a %s window" % (p["room"], p["piece"], side))
    return errors


# ---- the plan drawn from above ---------------------------------------------------------------------------------------
def draw(storey, path):
    from PIL import Image, ImageDraw, ImageFont
    S, pad, gap = 46, 30, 40
    w, h = int((W1 - W0) * S), int((D1 - D0) * S)
    img = Image.new("RGB", (4 * w + 3 * gap + 2 * pad, h + 2 * pad + 40), (246, 243, 238))
    dr = ImageDraw.Draw(img)
    try: f, fb = ImageFont.truetype("segoeui.ttf", 12), ImageFont.truetype("segoeuib.ttf", 18)
    except OSError: f = fb = ImageFont.load_default()
    for k, (lv, name) in enumerate(LEVELS.items()):
        ox = pad + k * (w + gap); oy = pad + 30
        P = lambda x, y: (ox + (x - W0) * S, oy + (D1 - y) * S)   # front of the house at the bottom
        R = lambda r, **kw: dr.rectangle([P(r[0], r[3]), P(r[1], r[2])], **kw)
        dr.text((ox, pad), "%d %s" % (lv, name), font=fb, fill=(138, 75, 47))
        R((W0, W1, D0, D1), fill=(90, 82, 74))
        for room, r in ROOMS[storey].items():
            R(r, fill=(236, 226, 210))
            dr.text(P(r[0] + .1, r[3] - .1), room, font=f, fill=(150, 140, 128))
        for name2, z in keep_clear(storey):
            R(z, outline=(200, 120, 100))
        for side, z in window_strips():
            R(z, fill=(150, 190, 215))
        x0, x1, y0, y1 = STAIR
        R((x0, x1, y0, y1), fill=(190, 160, 120))
        for it in ITEMS:
            if it["storey"] != storey or it["level"] > lv: continue
            k2 = kind(it["piece"])
            col = {"floor": (160, 120, 84), "rug": (214, 196, 170), "wall": (120, 140, 120), "ceiling": None}[k2]
            if col is None: continue
            R(it["_fp"], fill=col if k2 != "floor" else None, outline=(70, 50, 35) if k2 == "floor" else None, width=2)
        for it in ITEMS:
            if it["storey"] == storey and it["level"] <= lv and kind(it["piece"]) == "floor":
                fp = it["_fp"]; cx, cy = P((fp[0] + fp[1]) / 2, (fp[2] + fp[3]) / 2)
                dr.text((cx - 18, cy - 6), it["piece"].replace("k17:", "")[:9], font=f, fill=(60, 40, 30))
    img.save(path)


if __name__ == "__main__":
    errs = check()
    os.makedirs(DIR, exist_ok=True)
    for st in ("ground", "upstairs"):
        draw(st, os.path.join(DIR, "plan_%s.png" % st))
    plan = dict(house=dict(W0=W0, W1=W1, D0=D0, D1=D1, T=T, XA=XA, XB=XB, YM=YM, H=H, stair=STAIR, doors=DOORS,
                           windows=WINDOWS, front_door=FRONT_DOOR, rooms=ROOMS, spots=SPOTS, beds=BEDS_OF),
                levels=LEVELS, items=[{k: v for k, v in it.items() if not k.startswith("_")} for it in ITEMS])
    json.dump(plan, open(os.path.join(DIR, "layout.json"), "w", encoding="utf-8"), indent=1)
    if not errs:                                             # the approved plan for the game (ADR-0033, FURN-060)
        wp = os.path.join(ROOT, "World", "rules", "home_plans.json")
        world = json.load(open(wp, encoding="utf-8")) if os.path.exists(wp) else dict(note=PLANS_NOTE, plans={})
        world["note"] = PLANS_NOTE
        world["plans"]["house_12x9"] = dict(source="Tools/blender/house_interiors_21_plan.py", **plan)
        with open(wp, "w", encoding="utf-8", newline="\n") as f:
            json.dump(world, f, indent=1, ensure_ascii=False)
            f.write("\n")
    n = {lv: sum(1 for it in ITEMS if it["level"] <= lv) for lv in LEVELS}
    print("pieces by level:", n)
    for e in errs: print("  ", e)
    print("%d problems" % len(errs))
    sys.exit(1 if errs else 0)
