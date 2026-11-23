"""Brix Town - home interiors 22, the plans: what house interiors 21 did for the detached house, for the town's other
homes (World/city_01, kind "home"; counts from Tools/worldgen/draw_builds.py):

    row_14x12_2   row house 14 x 12 m, 2 storeys (136 lots, up to 4 beds)
    row_14x12_3   row house 14 x 12 m, 3 storeys (64 lots, up to 4 beds), a second floor of bedrooms
    row_16x12_2   row house 12 x 16 m lot with its door on the long side, so 16 m of front, 2 storeys (50, up to 3 beds)
    flat_80       court and tower flat of 80 m2 (11.68 x 6.85 m, 2 beds)
    flat_160      court and tower flat of 160 m2 (23.36 x 6.85 m, 3 beds)

Each home is drawn in its own frame, as house interiors 21: x across the front from left to right seen from the door
side, y from the front wall (-) to the back (+). A row house has its street door in the front wall; it has party walls
on both sides (no side windows) and, as in the lots' data, its stair against the left wall (lot["stair"], wall "w" for
the 14 x 12 lots, wall "n" for the 12 x 16 ones, which is the left wall seen from their west door), rising to the back.
The hall runs up the left side beside the stair; a corridor crosses the house from it, with two rooms in front of it and
two behind; behind the stair is a WC (ground) or bathroom (upstairs). A flat's front wall is the common corridor's, with
the flat's door near its left end (as in flats[].door) and no windows; its rooms open off a hall along that wall and
look out of the back.

Every piece gets the furnishing level it first comes in at (ADR-0033):
    1 Bare      beds for the household, a sofa, a table to eat at, the kitchen and bathroom
    2 Basic     storage and comfort: wardrobes, bedside tables, armchairs, desks, lamps
    3 Homely    rugs, shelves, television, pictures, hall furniture
    4 Full      every piece and the decoration

Runs in ordinary Python (no Blender), reusing the sizes and checks of house_interiors_21_plan.py:
    python Tools/blender/home_interiors_22_plan.py
It checks every piece is inside its room, no two floor pieces stand in each other, doorways, the front door, the stair,
its foot and its top stay clear, nothing tall stands in front of a window and nothing hangs over one, and that each home
sleeps its largest household. Writes Art/StyleTests/home_interiors_22/layout_<type>.json (read by home_interiors_22.py)
and plan_<type>.png. Exits non-zero when a check fails.
"""
import json, math, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import house_interiors_21_plan as H21          # its sizes, footprints and overlap test (its own plan is left alone)

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DIR = os.path.join(ROOT, "Art", "StyleTests", "home_interiors_22")
LEVELS = H21.LEVELS
box_of, overlap = H21.box_of, H21.overlap
P2, PI = math.pi / 2, math.pi
T, TP, H, SILL = 0.25, 0.12, 3.0, 0.9
SLEEPS = {"k17:bed": 2, "r_single": 1, "c_bunk": 2}      # cots are for babies on top of the household's beds


def turned(spec, x, y, yaw):
    return H21.turned(spec, x, y, yaw)


def kind(spec):
    return H21.kind(spec)


class Plan:
    """One home: outside walls W0..W1 x D0..D1, floors (bottom up), rooms, partitions, doors, windows, stair, items."""

    def __init__(s, key, title, W, D, floors, beds, lot):
        s.key, s.title, s.floors, s.beds, s.lot = key, title, floors, beds, lot
        s.W0, s.W1, s.D0, s.D1 = -W / 2, W / 2, -D / 2, D / 2
        s.i0, s.i1, s.j0, s.j1 = s.W0 + T, s.W1 - T, s.D0 + T, s.D1 - T
        s.rooms = {f: {} for f in floors}
        s.parts = {f: [] for f in floors}           # (axis, at, a0, a1): a partition centred on at, from a0 to a1 along it
        s.doors = {f: [] for f in floors}           # (axis, at, centre): 0.9 m doorways in partitions
        s.windows = {f: [] for f in floors}         # (side, centre, width), sill 0.9, head 2.3
        s.front_door = None                         # (x0, x1) in the front wall of the first floor
        s.stair = None                              # (x0, x1, y0, y1), rising towards +y
        s.stair_kind = {}                           # floor -> "up" (flight up from here), "well" (arrives here), "both"
        s.items = []

    # -- layout ---------------------------------------------------------------------------------------------------------
    def room(s, f, name, rect, flip=False):
        s.rooms[f][name] = tuple(round(v, 3) for v in rect)
        return Room(s, f, name, flip)

    def put(s, f, room, spec, x, y, yaw, level, lift=0.0):
        yaw = math.atan2(math.sin(yaw), math.cos(yaw))
        s.items.append(dict(storey=f, room=room, piece=spec, x=round(x, 3), y=round(y, 3), yaw=round(yaw, 4),
                            level=level, lift=lift))

    def wall(s, f, room, side, spec, at, level, gap=0.03, lift=0.0, out=0.0):
        """A piece with its back to a wall of its room (house_interiors_21_plan.wall): side back (+y), front, left, right."""
        x0r, x1r, y0r, y1r = s.rooms[f][room]
        px0, px1, py0, py1, h = box_of(spec)
        g = gap + out
        if side == "back":
            s.put(f, room, spec, at, y1r - g - py1, 0.0, level, lift)
        elif side == "front":
            s.put(f, room, spec, at, y0r + g + py1, PI, level, lift)
        elif side == "left":
            s.put(f, room, spec, x0r + g + py1, at, P2, level, lift)
        else:
            s.put(f, room, spec, x1r - g - py1, at, -P2, level, lift)

    # -- checks ---------------------------------------------------------------------------------------------------------
    def keep_clear(s, f):
        out = []
        for ax, at, c in s.doors[f]:
            out.append(("doorway %s %.2f" % (ax, c), (at - 0.9, at + 0.9, c - 0.45, c + 0.45) if ax == "x" else (c - 0.45, c + 0.45, at - 0.9, at + 0.9)))
        if f == s.floors[0] and s.front_door:
            out.append(("front door", (s.front_door[0], s.front_door[1], s.j0, s.j0 + 0.9)))
        if s.stair and f in s.stair_kind:
            x0, x1, y0, y1 = s.stair
            k = s.stair_kind[f]
            out.append(("stair", (x0 - 0.05, x1, y0, y1)))
            if k in ("up", "both"):
                out.append(("stair foot", (x0, x1, y0 - 0.9, y0)))
            if k in ("well", "both"):
                out.append(("stair top", (x0, x1, y1, y1 + 0.9)))
        return out

    def window_strips(s, f):
        out = []
        for side, c, w in s.windows[f]:
            if side == "front": out.append((side, (c - w / 2, c + w / 2, s.j0, s.j0 + 0.3)))
            elif side == "back": out.append((side, (c - w / 2, c + w / 2, s.j1 - 0.3, s.j1)))
            elif side == "left": out.append((side, (s.i0, s.i0 + 0.3, c - w / 2, c + w / 2)))
            else: out.append((side, (s.i1 - 0.3, s.i1, c - w / 2, c + w / 2)))
        return out

    def check(s):
        errors = []
        for it in s.items:
            it["_fp"] = turned(it["piece"], it["x"], it["y"], it["yaw"])
            r = s.rooms[it["storey"]][it["room"]]
            fp = it["_fp"]
            if kind(it["piece"]) != "ceiling" and not (fp[0] >= r[0] - .01 and fp[1] <= r[1] + .01 and fp[2] >= r[2] - .01 and fp[3] <= r[3] + .01):
                errors.append("%s %s %s: outside the room by %.2f m" % (it["storey"], it["room"], it["piece"], max(r[0] - fp[0], fp[1] - r[1], r[2] - fp[2], fp[3] - r[3])))
        for f in s.floors:
            its = [it for it in s.items if it["storey"] == f]
            floor = [it for it in its if kind(it["piece"]) == "floor" and not it["lift"]]
            for i, p in enumerate(floor):
                for q in floor[i + 1:]:
                    if overlap(p["_fp"], q["_fp"]):
                        errors.append("%s %s: %s and %s stand in each other" % (f, p["room"], p["piece"], q["piece"]))
                for name, z in s.keep_clear(f):
                    if overlap(p["_fp"], z):
                        errors.append("%s %s: %s blocks the %s" % (f, p["room"], p["piece"], name))
                if box_of(p["piece"])[4] > SILL + 0.15 and p["piece"] != "b_toilet":
                    for side, z in s.window_strips(f):
                        if overlap(p["_fp"], z):
                            errors.append("%s %s: %s (%.2f m tall) stands in front of a %s window" % (f, p["room"], p["piece"], box_of(p["piece"])[4], side))
            rugs = [it for it in its if kind(it["piece"]) == "rug"]
            for p in rugs:                       # a rug never runs into a doorway, the stair or the front door
                for name, z in s.keep_clear(f):
                    if name.startswith("stair") and overlap(p["_fp"], z):
                        errors.append("%s %s: rug over the %s" % (f, p["room"], name))
            for p in [it for it in its if kind(it["piece"]) == "wall" and it["piece"] != "o_curtains"]:
                for side, z in s.window_strips(f):
                    if overlap(p["_fp"], z):
                        errors.append("%s %s: %s hangs over a %s window" % (f, p["room"], p["piece"], side))
            for p in [it for it in its if kind(it["piece"]) == "wall"]:     # wall pieces need a wall: not on a doorway
                for name, z in s.keep_clear(f):
                    if name.startswith("doorway") and overlap(p["_fp"], z) and wall_piece_on_door(p["_fp"], z):
                        errors.append("%s %s: %s hangs in a %s" % (f, p["room"], p["piece"], name))
        # every room on a floor is reached: has a doorway, is the hall, or opens onto it
        for f in s.floors:
            for name, r in s.rooms[f].items():
                if name in ("Hall", "Landing", "Corridor"):
                    continue
                if not any(touches_door(r, d) for d in s.doors[f]):
                    errors.append("%s %s: no doorway" % (f, name))
        sleeps = sum(SLEEPS.get(it["piece"], 0) for it in s.items if it["level"] == 1)
        if sleeps < s.beds:
            errors.append("sleeps %d at Bare, the largest household is %d" % (sleeps, s.beds))
        return errors

    def write(s, errs):
        os.makedirs(DIR, exist_ok=True)
        draw(s, os.path.join(DIR, "plan_%s.png" % s.key))
        house = dict(key=s.key, title=s.title, W0=s.W0, W1=s.W1, D0=s.D0, D1=s.D1, T=T, TP=TP, H=H, floors=s.floors,
                     stair=s.stair, stair_kind=s.stair_kind, front_door=s.front_door, partitions=s.parts, doors=s.doors,
                     windows=s.windows, rooms=s.rooms, beds=s.beds, lot=s.lot)
        plan = dict(house=house, levels=LEVELS, items=[{k: v for k, v in it.items() if not k.startswith("_")} for it in s.items])
        json.dump(plan, open(os.path.join(DIR, "layout_%s.json" % s.key), "w", encoding="utf-8"), indent=1)


def wall_piece_on_door(fp, z):
    """True when a wall piece's back is over the doorway's hole (its footprint crosses the doorway's width)."""
    wx, wy = z[1] - z[0], z[3] - z[2]
    if wx < wy:      # doorway in an "x" partition: the hole runs along y
        return fp[2] < z[3] - 0.01 and z[2] < fp[3] - 0.01
    return fp[0] < z[1] - 0.01 and z[0] < fp[1] - 0.01


def touches_door(r, d):
    ax, at, c = d
    if ax == "x":
        return (abs(r[0] - (at + TP / 2)) < 0.02 or abs(r[1] - (at - TP / 2)) < 0.02) and r[2] <= c <= r[3]
    return (abs(r[2] - (at + TP / 2)) < 0.02 or abs(r[3] - (at - TP / 2)) < 0.02) and r[0] <= c <= r[1]


class Room:
    """A room in its own frame: u along its window wall from the left seen from inside looking out of the window, v from
    its door wall (the inner wall, v = 0) to the window wall (v = d). A back room is as the house is; a front room
    (flip) is turned half round, so the same furnishing serves both. Sides: out (window wall), in, left, right."""

    def __init__(s, plan, f, name, flip):
        s.p, s.f, s.name, s.flip = plan, f, name, flip
        s.r = plan.rooms[f][name]
        s.w, s.d = s.r[1] - s.r[0], s.r[3] - s.r[2]

    def X(s, u): return s.r[0] + u if not s.flip else s.r[1] - u
    def Y(s, v): return s.r[2] + v if not s.flip else s.r[3] - v

    def side(s, side):
        m = dict(out="back", left="left", right="right", **{"in": "front"})
        if s.flip:
            m = dict(out="front", left="right", right="left", **{"in": "back"})
        return m[side]

    def put(s, spec, u, v, yaw, level, lift=0.0):
        s.p.put(s.f, s.name, spec, s.X(u), s.Y(v), yaw + (PI if s.flip else 0.0), level, lift)

    def wall(s, side, spec, at, level, gap=0.03, lift=0.0, out=0.0):
        c = s.X(at) if side in ("out", "in") else s.Y(at)
        s.p.wall(s.f, s.name, s.side(side), spec, c, level, gap, lift, out)

    def door(s, u):
        """A doorway in the inner wall, centred u along it."""
        at = (s.r[2] - TP / 2) if not s.flip else (s.r[3] + TP / 2)
        s.p.doors[s.f].append(("y", round(at, 3), round(s.X(u), 3)))

    def window(s, u, w):
        s.p.windows[s.f].append(("back" if not s.flip else "front", round(s.X(u), 3), w))


# ---- room furnishings, in the room's own frame (door near u = du on the inner wall, windows on the out wall) ----------
def windows_for(R, n=None):
    n = n or (2 if R.w >= 4.6 else 1)
    if n == 1:
        R.window(R.w / 2, 1.2 if R.w > 2.6 else 0.9)
        return [R.w / 2]
    us = [R.w * 0.27, R.w * 0.73]
    for u in us:
        R.window(u, 1.2)
    return us


def living(R, du, fire=True):
    """Sofa with its back to the window, facing the television on the inner wall; armchair, bookcase, lamp."""
    w, d = R.w, R.d
    sx = w / 2 + 0.35 if du < w / 2 else w / 2 - 0.35
    R.wall("out", "k17:sofa", sx, 1, out=0.5)
    vs = d - 0.03 - 0.5 - 0.96                   # the sofa's front
    R.put("k17:coffee", sx, vs - 0.75, 0.0, 2)
    R.put("k17:rug", sx, vs - 0.7, 0.0, 3)
    tv_u = w - 1.05 if du < w / 2 else 1.05
    R.wall("in", "l_tv", tv_u, 3)
    R.wall("in", "o_painting", tv_u, 4, gap=0.0)
    far = "right" if du < w / 2 else "left"     # the wall away from the door
    near = "left" if far == "right" else "right"
    R.wall(far, "k17:armchair", vs - 0.9, 2)
    R.wall(far, "k17:bookcase", d - 0.75, 3)
    if fire:
        R.wall(near, "l_fireplace", vs - 0.35, 4)
    R.put("k17:floor_lamp", w - 0.3 if near == "right" else 0.3, d - 0.3, 0.0, 2)
    R.put("l_sidetable", sx + (1.35 if du < w / 2 else -1.35), d - 0.33, 0.0, 4)
    R.put("l_magrack", sx, vs - 1.5, 0.0, 4)
    R.put("d_pendant", sx, vs - 0.7, 0.0, 2)
    R.put("o_candles", sx - 0.25, vs - 0.75, 0.0, 4, lift=0.51)
    return far, near


def kitchen(R, du, table=True, long_side=None):
    """The kitchen run along the side wall away from the door, under the wall cupboards; the table in the middle."""
    w, d = R.w, R.d
    far = long_side or ("right" if du < w / 2 else "left")
    specs = ["k_stove", "k_sink", "k_counter", "k_fridge"]
    # along a side wall the run starts at the window end so the fridge stands by the door wall
    a = 0.05
    ends = []
    for spec in specs:
        x0, x1 = box_of(spec)[:2]
        c = d - a - (x1 - x0) / 2
        R.wall(far, spec, c, 1)
        ends.append(c)
        a += (x1 - x0) + 0.01
    R.wall(far, "k_wall", ends[2] + 0.2, 2, gap=0.0)             # cupboards over the counter, then over the sink
    R.wall(far, "k_wall", ends[1] + 0.06, 3, gap=0.0)
    tu = w / 2 - 0.15 if far == "right" else w / 2 + 0.15
    if table:
        R.put("k17:dining", tu, d / 2 + 0.25, 0.0, 1)
        R.put("d_pendant", tu, d / 2 + 0.25, 0.0, 2)
        R.put("o_fruit", tu, d / 2 + 0.25, 0.0, 4, lift=0.76)
    near = "left" if far == "right" else "right"
    R.wall(near, "k_potshelf", d - 1.0, 3)
    R.wall("in", "o_clock", w / 2 + (0.9 if du < w / 2 else -0.9), 4, gap=0.0)
    return far


def dining(R, du):
    w, d = R.w, R.d
    tu = w / 2
    R.put("k17:dining", tu, d / 2, P2, 2)
    R.put("d_pendant", tu, d / 2, 0.0, 2)
    far = "right" if du < w / 2 else "left"
    near = "left" if far == "right" else "right"
    R.wall(far, "d_sideboard", d / 2, 2)
    R.wall(far, "o_painting", d / 2, 3, gap=0.0)
    R.wall(far, "o_candles", d / 2 + 0.45, 4, gap=0.25, lift=0.91)    # on the sideboard
    R.wall(far, "o_vases", d / 2 - 0.45, 4, gap=0.1, lift=0.91)
    R.wall("in", "d_dresser", w - 1.0 if du < w / 2 else 1.0, 3)
    R.wall(near, "d_display", d - 0.9, 4)
    R.wall("out", "d_bench", tu, 4, out=0.0)
    R.put("o_floorplant", 0.5 if near == "left" else w - 0.5, 1.75, 0.0, 4)


def bathroom(R, du, along="out"):
    """Bath under the window, basin and mirror on the side wall away from the door, toilet on the other side."""
    w, d = R.w, R.d
    far = "right" if du < w / 2 else "left"
    near = "left" if far == "right" else "right"
    R.wall("out", "b_bath", (w - 0.95) if far == "right" else 0.95, 1)
    R.wall(far, "b_basin", d - 1.6, 1)
    R.wall(far, "b_mirror", d - 1.6, 2, gap=0.0)
    R.wall(near, "b_toilet", d - 1.4, 1)
    R.wall(far, "b_rail", d - 2.6, 2)
    R.put("d_pendant", w / 2, d / 2, 0.0, 2)


def bedroom_double(R, du, cheval=True):
    """The bed with its head to the side wall away from the door, between bedside tables; wardrobe on the inner wall."""
    w, d = R.w, R.d
    far = "right" if du < w / 2 else "left"
    near = "left" if far == "right" else "right"
    bv = d / 2 + 0.25
    R.wall(far, "k17:bed", bv, 1)
    R.wall(far, "k17:bedside", bv + 1.15, 2)
    R.wall(far, "k17:bedside", bv - 1.15, 2)
    R.wall("in", "k17:wardrobe", (w - 0.65) if far == "right" else 0.65, 2)
    if w >= 3.4:
        R.wall(near, "r_chest", d - 0.9, 3)
    R.wall(far, "o_painting", bv, 4, gap=0.0)
    bu = (w - 0.03 - 2.22) if far == "right" else (0.03 + 2.22)     # the foot of the bed
    if w >= 3.4:
        ru = bu - 0.5 if far == "right" else bu + 0.5
        R.put("k17:rug", min(max(ru, 1.03), w - 1.03), bv, P2, 3)
    R.put("r_blanket", bu + (-0.3 if far == "right" else 0.3), bv, -P2 if far == "right" else P2, 4)
    R.put("d_pendant", w / 2, bv, 0.0, 2)
    return far, near


def bedroom_single(R, du):
    """A single bed along the side wall away from the door, its head to the window wall; bedside table, wardrobe, and in
    a room 3 m wide or more a desk."""
    w, d = R.w, R.d
    far = "right" if du < w / 2 else "left"
    near = "left" if far == "right" else "right"
    sg = 1 if far == "right" else -1                  # +u towards the far wall
    bu = (w - 0.03 - 0.53) if far == "right" else (0.03 + 0.53)
    R.put("r_single", bu, d - 0.35 - 1.11, 0.0, 1)    # head 0.35 off the window wall, clear of the window
    R.put("k17:bedside", bu - sg * 0.85, d - 0.03 - 0.23 - 0.3, 0.0, 2)
    if w >= 3.0:
        R.wall("in", "k17:wardrobe", (w - 0.65) if far == "right" else 0.65, 2)
        R.wall(near, "s_desk", d - 1.4, 3)
        R.put("s_chair", (0.03 + 0.76 + 0.3) if near == "left" else (w - 0.03 - 0.76 - 0.3), d - 1.4, -P2 if near == "left" else P2, 3)
    else:
        R.wall(near, "k17:wardrobe", 1.5, 2)
    R.put("k17:rug", w / 2, d / 2 - 0.4, 0.0, 3) if w >= 2.1 else None
    R.wall(far, "o_painting", 0.0 + d / 2 - 1.4, 4, gap=0.0)
    R.put("d_pendant", w / 2, d / 2, 0.0, 2)
    return far, near


def children(R, du):
    w, d = R.w, R.d
    far = "right" if du < w / 2 else "left"
    near = "left" if far == "right" else "right"
    # the bunk lies along the far wall in the window corner, as in house interiors 21
    bu = (w - 0.03 - 0.55) if far == "right" else (0.03 + 0.55)
    R.put("c_bunk", bu, d - 0.35 - 1.07, 0.0, 1)
    R.wall("in", "c_toyshelf", (w - 0.75) if far == "right" else 0.75, 3)
    R.put("c_table", w / 2 - (0.2 if far == "right" else -0.2), d / 2 - 0.2, 0.0, 3)
    R.put("k17:rug", w / 2 - (0.2 if far == "right" else -0.2), d / 2 - 0.2, 0.0, 3)
    R.wall(near, "c_toychest", d - 0.9, 3)
    R.wall("in", "k17:wardrobe", (w - 1.95) if far == "right" else 1.95, 2) if w >= 3.6 else None
    R.wall(near, "c_cot", d / 2 - 0.7, 4) if d >= 4.2 else None
    R.put("c_horse", w / 2 + (0.6 if far == "right" else -0.6), 1.35, 0.4, 4)
    R.wall(far, "o_painting", 0.9, 4, gap=0.0)
    R.put("d_pendant", w / 2, d / 2, 0.0, 2)


def study(R, du):
    w, d = R.w, R.d
    far = "right" if du < w / 2 else "left"
    near = "left" if far == "right" else "right"
    R.wall(far, "s_desk", d / 2 + 0.2, 2)
    R.put("s_chair", (w - 0.03 - 0.76 - 0.3) if far == "right" else (0.03 + 0.76 + 0.3), d / 2 + 0.2, P2 if far == "right" else -P2, 2)
    R.wall(far, "s_pinboard", d / 2 + 0.2, 3, gap=0.0)
    R.wall(far, "s_filing", d / 2 - 0.85, 3)
    R.wall("in", "k17:bookcase", (w - 1.0) if far == "right" else 1.0, 3)
    R.wall(near, "s_shelving", d - 1.15, 4)
    R.wall(near, "k17:armchair", d / 2 - 0.7, 3)
    R.put("k17:floor_lamp", 0.32 if near == "left" else w - 0.32, d - 0.3, 0.0, 4)
    R.put("k17:rug", w / 2, d / 2, 0.0, 4)
    R.put("o_globe", w / 2, d - 0.45, 0.0, 4)
    R.put("d_pendant", w / 2, d / 2, 0.0, 2)


def playroom(R, du):
    w, d = R.w, R.d
    far = "right" if du < w / 2 else "left"
    near = "left" if far == "right" else "right"
    R.wall(far, "c_toyshelf", d / 2, 2)
    R.wall(far, "c_toychest", d / 2 - 1.5, 2)
    R.put("c_table", w / 2, d / 2, 0.0, 2)
    R.put("k17:rug", w / 2, d / 2, 0.0, 3)
    R.wall(near, "k17:bookcase", d - 1.0, 3)
    R.wall(near, "k17:armchair", d / 2 - 0.6, 3)
    R.put("c_horse", w / 2, d - 0.6, 0.3, 4)
    R.wall("in", "o_wallshelf", (w - 1.2) if far == "right" else 1.2, 4, gap=0.0)
    R.put("d_pendant", w / 2, d / 2, 0.0, 2)


def dressing(R, du):
    w, d = R.w, R.d
    far = "right" if du < w / 2 else "left"
    near = "left" if far == "right" else "right"
    R.wall(far, "k17:wardrobe", d / 2 + 0.7, 2)
    R.wall(far, "k17:wardrobe", d / 2 - 0.55, 2)
    R.wall(near, "r_chest", d / 2 + 0.6, 2)
    R.wall(near, "r_vanity", d / 2 - 0.8, 3)
    R.put("r_cheval", w / 2 + (0.6 if far == "right" else -0.6), d - 0.75, 0.0, 3)
    R.put("k17:rug", w / 2, d / 2, 0.0, 3)
    R.put("r_screen", w / 2 - (0.6 if far == "right" else -0.6), d - 0.6, 0.0, 4)
    R.put("d_pendant", w / 2, d / 2, 0.0, 2)


def sitting(R, du):
    w, d = R.w, R.d
    far = "right" if du < w / 2 else "left"
    near = "left" if far == "right" else "right"
    R.wall("out", "k17:sofa", w / 2, 2, out=0.3)
    R.put("k17:coffee", w / 2, d - 0.33 - 0.96 - 0.6, 0.0, 3)
    R.wall(far, "k17:armchair", d / 2 - 0.4, 3)
    R.wall("in", "l_records", (w - 1.0) if far == "right" else 1.0, 4)
    R.wall(near, "k17:bookcase", d / 2, 3)
    R.put("k17:rug", w / 2, d / 2 + 0.2, 0.0, 3)
    R.put("k17:floor_lamp", 0.32 if near == "left" else w - 0.32, d - 0.32, 0.0, 4)
    R.put("d_pendant", w / 2, d / 2, 0.0, 2)


# ---- row houses -------------------------------------------------------------------------------------------------------
def row(key, title, W, floor_names, rooms_by_floor, beds, lot, xsplit):
    """A row house W wide and 12 deep: hall and stair up the left, a corridor across at y = 0, two rooms in front of it
    and two behind; behind the stair a WC or bathroom. rooms_by_floor: floor -> (FL, FR, BL, BR, strip) furnishings."""
    p = Plan(key, title, W, 12.0, floor_names, beds, lot)
    i0, i1, j0, j1 = p.i0, p.i1, p.j0, p.j1
    XS = i0 + 2.45 + TP / 2                         # the hall's partition
    YC = 0.66                                        # corridor walls at -YC and +YC
    YW = 2.06                                        # the wall behind the stair
    p.stair = (i0, round(i0 + 1.0, 3), -4.5, 1.0)
    p.front_door = (round(i0 + 1.25, 3), round(i0 + 2.25, 3))
    n = len(floor_names)
    for k, f in enumerate(floor_names):
        p.stair_kind[f] = "up" if k == 0 else ("well" if k == n - 1 else "both")
        xf, xk = xsplit[f]
        P = p.parts[f]
        P += [("x", XS, j0, -YC), ("x", XS, YC, j1), ("y", -YC, XS, i1), ("y", YC, XS, i1), ("y", YW, i0, XS),
              ("x", xf, j0, -YC), ("x", xk, YC, j1)]
        hall = "Hall" if k == 0 else "Landing"
        p.room(f, hall, (i0, XS - TP / 2, j0, YW - TP / 2))
        p.room(f, "Corridor", (XS + TP / 2, i1, -YC + TP / 2, YC - TP / 2))
        fl, fr, bl, br, strip = rooms_by_floor[f]
        a, b = XS + TP / 2, i1
        rects = [(fl, (a, xf - TP / 2, j0, -YC - TP / 2), True), (fr, (xf + TP / 2, b, j0, -YC - TP / 2), True),
                 (bl, (a, xk - TP / 2, YC + TP / 2, j1), False), (br, (xk + TP / 2, b, YC + TP / 2, j1), False)]
        for (name, fn, du), rect, flip in rects:
            R = p.room(f, name, rect, flip)
            R.door(du)
            windows_for(R)
            fn(R, du)
        sname, sfn = strip
        R = p.room(f, sname, (i0, XS - TP / 2, YW + TP / 2, j1), False)
        p.doors[f].append(("y", YW, round(i0 + 1.6, 3)))
        R.window(1.0, 0.9)
        sfn(R)
        # hall or landing
        hx0, hx1 = i0 + 1.0, XS - TP / 2              # the passage beside the stair
        if k == 0:
            p.wall(f, hall, "right", "h_shoebench", -3.1, 3)
            p.wall(f, hall, "right", "h_hooks", -3.1, 4, gap=0.0)
            p.put(f, hall, "h_coatstand", hx1 - 0.33, -1.6, 0.0, 3)
            p.put(f, hall, "h_umbrella", hx1 - 0.25, -2.15, 0.0, 4)
            p.wall(f, hall, "right", "h_mirror", -4.25, 4, gap=0.0)
            p.put(f, hall, "d_pendant", (hx0 + hx1) / 2, -2.5, 0.0, 2)
        else:
            p.wall(f, hall, "right", "o_wallshelf", -3.0, 4, gap=0.0)
            p.put(f, hall, "o_floorplant", hx1 - 0.46, -1.1, 0.0, 4)
            p.put(f, hall, "d_pendant", (hx0 + hx1) / 2, -2.5, 0.0, 2)
            p.put(f, hall, "o_bust", hx1 - 0.25, j0 + 0.3, 0.0, 4) if k == n - 1 else None
        # corridor: a light and pictures
        cx = (XS + i1) / 2
        p.put(f, "Corridor", "d_pendant", cx, 0.0, 0.0, 2)
    return p


def wc(R):
    """Behind the stair, 2.45 x 3.6: toilet and basin; door near its right end."""
    R.wall("left", "b_toilet", R.d - 0.6, 1)
    R.wall("left", "b_basin", R.d - 1.6, 1)
    R.wall("left", "b_mirror", R.d - 1.6, 2, gap=0.0)
    R.wall("right", "b_rail", R.d - 1.6, 2)
    R.put("o_floorplant", R.w - 0.45, R.d - 0.45, 0.0, 4)
    R.put("d_pendant", R.w / 2, R.d / 2, 0.0, 2)


def bath_small(R):
    """Behind the stair upstairs: the bath along the left wall, basin and toilet facing it."""
    R.wall("left", "b_bath", R.d - 0.95, 1)
    R.wall("right", "b_toilet", R.d - 0.4, 1)
    R.wall("right", "b_basin", R.d - 1.25, 1)
    R.wall("right", "b_mirror", R.d - 1.25, 2, gap=0.0)
    R.put("d_pendant", R.w / 2, R.d / 2, 0.0, 2)


def box_room(R):
    R.wall("left", "k17:bookcase", R.d - 1.2, 3)
    R.wall("left", "s_shelving", R.d - 2.4, 2)
    R.wall("right", "r_chest", R.d - 1.0, 3)
    R.put("d_pendant", R.w / 2, R.d / 2, 0.0, 2)


def flat(key, title, rooms, beds, lot):
    """A flat, 6.85 deep: a hall along its front (the common corridor's wall, the front door near the left end), the
    rooms behind it, each with windows in the back wall. rooms: [(name, width, fn, du)] from left to right."""
    Wd = sum(w for n, w, fn, du in rooms) + (len(rooms) - 1) * TP + 2 * T
    p = Plan(key, title, round(Wd, 3), 6.85, ["flat"], beds, lot)
    f = "flat"
    i0, i1, j0, j1 = p.i0, p.i1, p.j0, p.j1
    YH = j0 + 1.25 + TP / 2                          # the hall's wall
    p.front_door = (round(p.W0 + 1.0 - 0.5, 3), round(p.W0 + 1.0 + 0.5, 3))
    p.parts[f].append(("y", round(YH, 3), i0, i1))
    p.room(f, "Hall", (i0, i1, j0, YH - TP / 2))
    x = i0
    for k, (name, w, fn, du) in enumerate(rooms):
        R = p.room(f, name, (x, x + w, YH + TP / 2, j1))
        R.door(du)
        windows_for(R)
        fn(R, du)
        x += w
        if k < len(rooms) - 1:
            p.parts[f].append(("x", round(x + TP / 2, 3), YH, j1))
            x += TP
    # the hall: coats by the door, a bench, lights
    hy = (j0 + YH - TP / 2) / 2
    p.put(f, "Hall", "h_coatstand", p.front_door[1] + 0.35, j0 + 0.32, 0.0, 3)
    p.put(f, "Hall", "h_umbrella", p.front_door[1] + 0.95, j0 + 0.2, 0.0, 4)
    n = max(1, int((i1 - i0) // 5.5))                # a light every 5.5 m or so along the hall
    for k in range(n):
        p.put(f, "Hall", "d_pendant", i0 + (i1 - i0) * (k + 0.5) / n, hy, 0.0, 2)
    return p


def living_kitchen(R, du):
    """A small flat's one living room: the kitchen run along the left wall, the table beside it, the sofa at the window,
    the television on the right wall by the door."""
    w, d = R.w, R.d
    a = 0.05
    for spec in ["k_fridge", "k_counter", "k_sink", "k_stove"]:
        x0, x1 = box_of(spec)[:2]
        R.wall("left", spec, a + (x1 - x0) / 2, 1)
        a += (x1 - x0) + 0.01
    R.wall("left", "k_wall", 0.05 + 0.77 + 0.94, 2, gap=0.0)
    R.wall("left", "k_wall", 0.05 + 0.77 + 1.89 + 0.59, 3, gap=0.0)
    R.put("k17:dining", 2.0, 1.9, 0.0, 1)
    R.put("d_pendant", 2.0, 1.9, 0.0, 2)
    R.put("o_fruit", 2.0, 1.9, 0.0, 4, lift=0.76)
    R.wall("out", "k17:sofa", 2.75, 1)
    R.put("k17:coffee", 2.75, 3.38, 0.0, 3)
    R.put("k17:rug", 2.75, 3.55, 0.0, 4)
    R.wall("right", "l_tv", 1.75, 3)
    R.wall("right", "o_painting", 3.0, 4, gap=0.0)
    R.put("k17:floor_lamp", 1.2, d - 0.3, 0.0, 2)
    R.put("d_pendant", 2.75, d - 1.3, 0.0, 2)


def build():
    lot_a = "b_10_11_l0 (41 Depot Street): 14 x 12, door in the middle of the south (front) side, stair 2.35 x 6.9 on the west wall"
    lot_b = "b_10_11 14 x 12 lots with storeys main, bedrooms, bedrooms (e.g. door [998, 1067], stair on the west wall)"
    lot_c = "b_10_11 12 x 16 lots (e.g. door [971, 1104] on the west side, stair 6.9 x 2.35 on the north wall)"
    plans = []
    # (a) 14 x 12, 2 storeys, up to 4 beds
    plans.append(row("row_14x12_2", "Row house 14 x 12 m, 2 storeys", 14.0, ["ground", "upstairs"], {
        "ground": (("Living room", living, 4.4), ("Dining room", dining, 4.4), ("Kitchen", kitchen, 0.8), ("Play room", playroom, 0.8), ("WC", wc)),
        "upstairs": (("Main bedroom", bedroom_double, 4.4), ("Children's room", children, 4.4), ("Bedroom", bedroom_single, 0.8), ("Study", study, 0.8), ("Bathroom", bath_small)),
    }, 4, lot_a, {"ground": (1.2, 1.2), "upstairs": (1.2, 1.2)}))
    # (b) 14 x 12, 3 storeys, up to 4 beds
    plans.append(row("row_14x12_3", "Row house 14 x 12 m, 3 storeys", 14.0, ["ground", "first", "second"], {
        "ground": (("Living room", living, 4.4), ("Dining room", dining, 4.4), ("Kitchen", kitchen, 0.8), ("Study", study, 0.8), ("WC", wc)),
        "first": (("Main bedroom", bedroom_double, 4.4), ("Bedroom", bedroom_single, 4.4), ("Sitting room", sitting, 0.8), ("Dressing room", dressing, 0.8), ("Bathroom", bath_small)),
        "second": (("Children's room", children, 4.4), ("Play room", playroom, 4.4), ("Bedroom 2", bedroom_single, 0.8), ("Guest room", bedroom_single, 0.8), ("Bathroom", bath_small)),
    }, 4, lot_b, {"ground": (1.2, 1.2), "first": (1.2, 1.2), "second": (1.2, 1.2)}))
    # (c) 16 front x 12, 2 storeys, up to 3 beds
    plans.append(row("row_16x12_2", "Row house 16 x 12 m (12 x 16 lot), 2 storeys", 16.0, ["ground", "upstairs"], {
        "ground": (("Living room", living, 5.6), ("Dining room", dining, 4.4), ("Kitchen", kitchen, 0.8), ("Study", study, 0.8), ("WC", wc)),
        "upstairs": (("Main bedroom", bedroom_double, 5.0), ("Children's room", children, 4.6), ("Bedroom", bedroom_single, 0.8), ("Sitting room", sitting, 0.8), ("Bathroom", bath_small)),
    }, 3, lot_c, {"ground": (2.2, 1.2), "upstairs": (1.8, 1.2)}))
    # (d) flat of 80 m2, 2 beds; (e) flat of 160 m2, 3 beds
    lot_f = "court flats (e.g. b_10_5_l0 Plum Court, flats[].rect 11.68 x 6.85 and 23.36 x 6.85, door 1 m from the left end)"
    plans.append(flat("flat_80", "Flat of 80 m2 (11.68 x 6.85 m)", [
        ("Living room", 4.12, living_kitchen, 3.6), ("Bathroom", 1.9, bathroom, 0.5),
        ("Main bedroom", 2.85, bedroom_double, 0.5), ("Bedroom", 1.95, bedroom_single, 0.5)], 2, lot_f))
    plans.append(flat("flat_160", "Flat of 160 m2 (23.36 x 6.85 m)", [
        ("Living room", 5.9, living, 4.9), ("Kitchen", 4.4, kitchen, 0.6), ("Bathroom", 2.56, bathroom, 0.5),
        ("Main bedroom", 3.6, bedroom_double, 0.6), ("Bedroom", 2.9, bedroom_single, 0.6), ("Bedroom 2", 2.9, bedroom_single, 0.6)], 3, lot_f))
    return plans


# ---- the plans drawn from above: a row per floor, a column per level ---------------------------------------------------
def draw(p, path):
    from PIL import Image, ImageDraw, ImageFont
    S = 46 if p.W1 - p.W0 < 18 else 30
    pad, gap = 30, 40
    w, h = int((p.W1 - p.W0) * S), int((p.D1 - p.D0) * S)
    img = Image.new("RGB", (4 * w + 3 * gap + 2 * pad + 90, len(p.floors) * (h + 40 + gap) + 2 * pad + 20), (246, 243, 238))
    dr = ImageDraw.Draw(img)
    try: f, fb = ImageFont.truetype("segoeui.ttf", 12), ImageFont.truetype("segoeuib.ttf", 18)
    except OSError: f = fb = ImageFont.load_default()
    dr.text((pad, 8), "%s: plan at each furnishing level (front at the bottom)" % p.title, font=fb, fill=(43, 38, 34))
    for r, fl in enumerate(p.floors):
        oy = pad + 30 + r * (h + 40 + gap)
        dr.text((pad, oy + h // 2), fl, font=fb, fill=(43, 38, 34))
        for k, (lv, name) in enumerate(LEVELS.items()):
            ox = pad + 90 + k * (w + gap)
            P = lambda x, y: (ox + (x - p.W0) * S, oy + 26 + (p.D1 - y) * S)
            Rr = lambda q, **kw: dr.rectangle([P(q[0], q[3]), P(q[1], q[2])], **kw)
            n = sum(1 for it in p.items if it["storey"] == fl and it["level"] <= lv)
            dr.text((ox, oy), "%d %s  (%d pieces)" % (lv, name, n), font=fb, fill=(138, 75, 47))
            Rr((p.W0, p.W1, p.D0, p.D1), fill=(90, 82, 74))
            for room, q in p.rooms[fl].items():
                Rr(q, fill=(236, 226, 210))
                dr.text(P(q[0] + .1, q[3] - .1), room, font=f, fill=(150, 140, 128))
            for name2, z in p.keep_clear(fl):
                Rr(z, outline=(200, 120, 100))
            for side, z in p.window_strips(fl):
                Rr(z, fill=(150, 190, 215))
            if fl in p.stair_kind:
                Rr(p.stair, fill=(190, 160, 120))
            for it in p.items:
                if it["storey"] != fl or it["level"] > lv: continue
                k2 = kind(it["piece"])
                col = {"floor": None, "rug": (214, 196, 170), "wall": (120, 140, 120), "ceiling": None}[k2]
                if k2 == "floor": Rr(it["_fp"], outline=(70, 50, 35), width=2)
                elif col: Rr(it["_fp"], fill=col)
            for it in p.items:
                if it["storey"] == fl and it["level"] <= lv and kind(it["piece"]) == "floor":
                    fp = it["_fp"]; cx, cy = P((fp[0] + fp[1]) / 2, (fp[2] + fp[3]) / 2)
                    dr.text((cx - 18, cy - 6), it["piece"].replace("k17:", "")[:9], font=f, fill=(60, 40, 30))
    img.save(path)


if __name__ == "__main__":
    total = 0
    for p in build():
        errs = p.check()
        p.write(errs)
        n = {lv: sum(1 for it in p.items if it["level"] <= lv) for lv in LEVELS}
        nf = {f: sum(1 for it in p.items if it["storey"] == f) for f in p.floors}
        print("%-12s pieces by level %s, at Full by floor %s, %d problems" % (p.key, n, nf, len(errs)))
        for e in errs: print("    ", e)
        total += len(errs)
    print("%d problems" % total)
    sys.exit(1 if total else 0)
