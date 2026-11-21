"""Wasted - building kit 08: the special buildings in the locked look (ART-001, ART-002; daylight ADR-0007).

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/building_kit_08.py").read(), "building_kit_08.py", "exec"))
or headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/building_kit_08.py
Quick checks: WASTED_ONLY=town_hall,lantern_arcade WASTED_SAMPLES=24 WASTED_SCALE=50

Builds on building_kit_07.py (its kit of walls, openings, roofs and signs) and adds one design per kind of building the
kit did not cover: the town hall, the hospital, the mall, a guild house, an office tower, a residential tower, a
workshop and the tram depot. Each is built from its real lot in World/ (name, kind, floors, footprint).

Civic halls use taller storeys (3.4 m) than the 3 m a floor in WORLD_MAP; that is a proposal shown here, not data.
Renders go to Art/StyleTests/building_kit_08_*.png.
"""
import bpy, bmesh, math, random, os, time, json, glob, traceback
from mathutils import Vector, Euler, Matrix

B = "C:/Projects/Wasted/Tools/blender/"
_k = open(B + "building_kit_07.py", encoding="utf-8").read().split("\n# ---- the street and the scenes")[0]
exec(compile(_k, "building_kit_07_helpers", "exec"), globals())

NAME = "building_kit_08"
LOG = OUT + "/" + NAME + "_log.txt"
K_M = {}
PAD = 150.0                   # buildings stand this far apart, each on its own stretch of street
CIVIC_FLOOR = 3.4

SPECIAL = (
    dict(key="town_hall", name="Town Hall", cap=40.0, depth=22.0, fallback=dict(kind="work", district="Centre", floors=3, rect=[0, 0, 48, 34])),
    dict(key="hospital", name="Mill Street Hospital", cap=44.0, depth=18.0, fallback=dict(kind="health", district="Centre", floors=5, rect=[0, 0, 60, 38])),
    dict(key="lantern_arcade", name="Lantern Arcade", cap=40.0, depth=34.0, fallback=dict(kind="shop", district="Centre", floors=2, rect=[0, 0, 64, 42])),
    dict(key="guild_house", name="Guild House", cap=22.0, depth=14.0, fallback=dict(kind="work", district="Centre", floors=3, rect=[0, 0, 22, 14])),
    dict(key="office_tower", name="Fountain Tower", cap=26.0, depth=26.0, fallback=dict(kind="work", district="Centre", floors=22, rect=[0, 0, 30, 30])),
    dict(key="flats_tower", name="Depot Heights", cap=26.0, depth=26.0, fallback=dict(kind="home", district="Old Town", floors=14, rect=[0, 0, 30, 30])),
    dict(key="workshop", name="Linden Works", cap=24.0, depth=16.0, fallback=dict(kind="work", district="Old Town", floors=3, rect=[0, 0, 38, 16])),
    dict(key="tram_depot", name="Depot Depot", cap=36.0, depth=24.0, fallback=dict(kind="utility", district="Outskirts", floors=2, rect=[0, 0, 36, 22])),
)


# ---- helpers on top of kit 07 -----------------------------------------------------------------
def look_for(rng, **kw):
    lk = dict(frame=rng.choice(("cream", "wood", "teal")), shutter=rng.choice(("teal", "wood_red", "ochre", "dusty")),
              door=rng.choice(("wood_red", "wood", "teal")), shutters=False, boxes=False)
    lk.update(kw)
    return lk


def bays_of(x0, x1, nb):
    W = x1 - x0
    return [x0 + (i + 0.5) * W / nb for i in range(nb)]


def core_box(name, x0, x1, D, H, mat, t=None):
    t = KIT["wall_t"] if t is None else t
    return box(name, x0 + 0.02, x1 - 0.02, t + 0.02, D, 0, H, K_M[mat], bevel=0)


def as_side(fn, x1):
    """Build a facade with fn() as if it faced the street (y=0, x from 0), then turn it to be the east side at x1."""
    before = set(COL.objects)
    fn()
    R = Matrix.Rotation(math.pi / 2, 4, 'Z')
    for ob in set(COL.objects) - before:
        ob.location = (R @ ob.location.to_4d()).to_3d() + Vector((x1, 0, 0))
        ob.rotation_euler.z += math.pi / 2


def turned(fn, x_at, rot):
    """Build with fn() then turn everything it made by rot about z and move it to x_at (for roofs with the ridge along y)."""
    before = set(COL.objects)
    fn()
    R = Matrix.Rotation(rot, 4, 'Z')
    for ob in set(COL.objects) - before:
        ob.location = (R @ ob.location.to_4d()).to_3d() + Vector((x_at, 0, 0))
        ob.rotation_euler.z += rot


def front_gable(K, mat, xa, xb, z, h, y0, y1):
    """A triangle standing on the facade, thick from y0 to y1."""
    bm = K.bm(mat, 0.0)
    xm = (xa + xb) / 2
    a = [bm.verts.new((x, y0, zz)) for (x, zz) in ((xa, z), (xb, z), (xm, z + h))]
    b = [bm.verts.new((x, y1, zz)) for (x, zz) in ((xa, z), (xb, z), (xm, z + h))]
    bm.faces.new(a)
    bm.faces.new(list(reversed(b)))
    for i in range(3):
        j = (i + 1) % 3
        bm.faces.new((a[j], a[i], b[i], b[j]))


def cornice(K, x0, x1, z, t, mat="stone_dark", yo=0.0):
    """A heavier cornice: three steps out. yo moves it back from the street line (for a setback storey)."""
    for s, (dy, dz, h) in enumerate(((0.10, 0.0, 0.16), (0.20, 0.16, 0.14), (0.30, 0.30, 0.18))):
        K.block(mat, x0 - dy * 0.6, x1 + dy * 0.6, yo - dy, yo + t, z + dz, z + dz + h)
    return z + 0.48


def half_disc(K, mat, cx, y0, y1, z, r, segs=28):
    """The round top of an arched opening as a solid half-disc, thick from y0 to y1."""
    bm = K.bm(mat, 0.0)
    pts = [(cx + r * math.cos(math.pi * k / segs), z + r * math.sin(math.pi * k / segs)) for k in range(segs + 1)]
    a = [bm.verts.new((x, y0, zz)) for (x, zz) in pts]
    b = [bm.verts.new((x, y1, zz)) for (x, zz) in pts]
    bm.faces.new(a)
    bm.faces.new(list(reversed(b)))
    for i in range(segs):
        bm.faces.new((a[i + 1], a[i], b[i], b[i + 1]))
    bm.faces.new((a[0], a[segs], b[segs], b[0]))


def coped_parapet(K, x0, x1, D, H, t, mat, h=0.8, returns=True):
    K.block(mat, x0, x1, 0, t, H, H + h, 0.03)
    if returns:
        for (a, b) in ((x0, x0 + t), (x1 - t, x1)):
            K.block(mat, a, b, 0, D, H, H + h, 0.03)
    K.block("stone_dark", x0 - 0.06, x1 + 0.06, -0.1, t + 0.1, H + h, H + h + 0.16)


def window_row(K, rng, bays, z0, w, h, t, look, holes, skip=(), arch=False, surround=True, lit=False, caps=False):
    for i, cx in enumerate(bays):
        if i in skip:
            continue
        hh = window(K, rng, cx, z0, w, h, t, look, arch=arch, surround=surround, lit=lit)
        holes.append(hh)
        if arch:
            voussoirs(K, rng, cx, hh["spring"], w / 2, t, ring=0.36)
        if caps:                                   # a little cornice over each window
            K.block("stone_dark", cx - w / 2 - 0.35, cx + w / 2 + 0.35, -0.2, 0.05, z0 + h + 0.24, z0 + h + 0.36)


def iron_rail(K, x0, x1, y, z, h=0.95, step=0.13):
    K.block("iron", x0, x1, y - 0.03, y + 0.03, z + h - 0.06, z + h, 0)
    K.block("iron", x0, x1, y - 0.03, y + 0.03, z + 0.04, z + 0.09, 0)
    x = x0 + step / 2
    while x < x1:
        K.block("iron", x - 0.013, x + 0.013, y - 0.02, y + 0.02, z + 0.05, z + h - 0.03, 0)
        x += step


def clock(name, x, y, z, r):
    cyl_y(name + "_Rim", x, y + 0.02, z, r + 0.16, 0.16, K_M["stone_dark"], segs=40)
    cyl_y(name + "_Face", x, y - 0.03, z, r, 0.12, K_M["cream"], segs=40)
    for k in range(12):
        a = k * math.pi / 6
        box(name + "_Tick", x + (r - 0.12) * math.sin(a) - 0.03, x + (r - 0.12) * math.sin(a) + 0.03, y - 0.11, y - 0.08,
            z + (r - 0.12) * math.cos(a) - 0.08, z + (r - 0.12) * math.cos(a) + 0.08, K_M["iron"], bevel=0, )
    box(name + "_HandM", x - 0.035, x + 0.035, y - 0.14, y - 0.11, z, z + r * 0.82, K_M["iron"], bevel=0)
    box(name + "_HandH", x, x + r * 0.55, y - 0.15, y - 0.12, z - 0.03, z + 0.03, K_M["iron"], bevel=0, rot=(0, math.radians(-30), 0))


def hip_roof(name, cx, cy, z, w, d, h, mat):
    """A pyramid roof (square base turned 45 degrees from a four-sided cone)."""
    r = max(w, d) / 2 * math.sqrt(2)
    ob = cyl(name, cx, cy, z, r, h, K_M[mat], segs=4, r_top=0.02)
    ob.rotation_euler = (0, 0, math.pi / 4)
    ob.scale = (w / max(w, d), d / max(w, d), 1.0)
    return ob


def people(where):
    cs = CHAR_STYLES["B"]
    for i, (who, x, y) in enumerate(where):
        sp = dict(CAST[who])
        sp["name"] = "%s_%d" % (who, i)
        character3(sp, cs, x, y)


# ---- the buildings ---------------------------------------------------------------------------------
def town_hall(lot, x0, rng):
    """Stone civic hall: an open arcade below, a piano nobile with a balcony, an attic with the name and a clock tower."""
    W, D = lot["width"], lot["depth"]
    x1 = x0 + W
    fz = (0.0, 4.4, 4.4 + CIVIC_FLOOR + 0.4, 4.4 + 2 * CIVIC_FLOOR + 0.4)
    H = fz[3]
    t = 0.6
    K = Kit("TownHall")
    look = look_for(rng, frame="wood", door="wood")
    nb = 9
    bays = bays_of(x0, x1, nb)
    holes = []
    core = core_box("TownHall_Core", x0, x1, D, H, "stone_a", t=3.4)
    # arcade: seven open arches with a loggia behind them, doors in its back wall
    for i in range(1, nb - 1):
        cx = bays[i]
        holes.append(dict(x0=cx - 1.5, x1=cx + 1.5, z0=0.0, spring=2.75))
        voussoirs(K, rng, cx, 2.75, 1.5, t, ring=0.5)
        if i % 2 == 1:
            door(K, rng, cx, 1.5, 2.9, 3.4 + 0.12, look, arch=True, steps=0)
        else:
            K.block("lit", cx - 0.8, cx + 0.8, 3.36, 3.42, 0.9, 2.7, 0)
        lantern("TownHall_Lamp", cx, 1.8, 3.55, K_M, 50)
    for xs in (bays[0] + W / nb / 2, bays[-1] - W / nb / 2):
        K.block("stone_b", xs - 0.3, xs + 0.3, t, 3.4, 0, fz[1])
    K.block("stone_dark", bays[1] - 2.2, bays[-2] + 2.2, t, 3.4, fz[1] - 0.25, fz[1])
    K.block("paving", bays[1] - 2.2, bays[-2] + 2.2, 0, 3.4, -0.02, 0.02, 0)
    for s in range(3):                              # broad steps in front of the middle arches
        K.block("stone_dark", bays[3] - 1.6 - 0.4 * (2 - s), bays[5] + 1.6 + 0.4 * (2 - s), -1.5 + 0.45 * s, 0.6, 0.16 * s, 0.16 * (s + 1))
    masonry(K, rng, x0, x1, 0.0, fz[1], holes, t)
    band(K, x0, x1, fz[1], t, 2)
    # piano nobile: tall windows with little cornices, the middle one a balcony door
    up = []
    window_row(K, rng, bays, fz[1] + 0.75, 1.3, 2.4, t, look, up, skip=(4,), caps=True)
    hb = balcony(K, rng, bays[4], fz[1] + 0.1, t, look)
    up.append(hb)
    window_row(K, rng, bays, fz[2] + 0.55, 1.2, 1.5, t, look, up)
    band(K, x0, x1, fz[2], t, 1)
    masonry(K, rng, x0, x1, fz[1] + 0.39, H, up, t)
    top = cornice(K, x0, x1, H, t)
    coped_parapet(K, x0, x1, D, top, t, "stone_a", h=0.7)
    # attic with the name, and the clock tower behind it
    ax0, ax1 = bays[2] - 1.0, bays[6] + 1.0
    K.block("stone_b", ax0, ax1, -0.05, t, top, top + 1.9, 0.03)
    K.block("stone_dark", ax0 - 0.1, ax1 + 0.1, -0.15, t + 0.05, top + 1.9, top + 2.1)
    text("TownHall_Name", lot["name"].upper(), (x0 + x1) / 2, -0.08, top + 0.95, 0.85, "dark", 0.03)
    tx, ty, tw = (x0 + x1) / 2, D * 0.45, 6.0
    tz0, tz1 = H, H + 11.0
    K.block("stone_a", tx - tw / 2, tx + tw / 2, ty - tw / 2, ty + tw / 2, tz0, tz1, 0.03)
    for zz in (H + 4.0, tz1 - 0.3):
        K.block("stone_dark", tx - tw / 2 - 0.15, tx + tw / 2 + 0.15, ty - tw / 2 - 0.15, ty + tw / 2 + 0.15, zz, zz + 0.3)
    clock("TownHall_Clock", tx, ty - tw / 2 - 0.02, H + 7.0, 1.6)
    bz0 = tz1                                        # belfry: four corner piers round a bell
    for (px, py) in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
        K.block("stone_a", tx + px * (tw / 2 - 0.45) - 0.45, tx + px * (tw / 2 - 0.45) + 0.45, ty + py * (tw / 2 - 0.45) - 0.45,
                ty + py * (tw / 2 - 0.45) + 0.45, bz0, bz0 + 3.2, 0.03)
    K.block("stone_dark", tx - tw / 2 - 0.2, tx + tw / 2 + 0.2, ty - tw / 2 - 0.2, ty + tw / 2 + 0.2, bz0 + 3.2, bz0 + 3.6)
    cyl("TownHall_Bell", tx, ty, bz0 + 1.0, 0.7, 1.4, K_M["ochre"], segs=20, r_top=0.35)
    hip_roof("TownHall_TowerRoof", tx, ty, bz0 + 3.6, tw + 0.6, tw + 0.6, 4.5, "tile")
    cyl("TownHall_Flag", tx, ty, bz0 + 8.0, 0.05, 3.0, K_M["iron"], segs=8)
    box("TownHall_FlagCloth", tx + 0.05, tx + 1.6, ty - 0.02, ty + 0.02, bz0 + 9.9, bz0 + 10.9, K_M["terra"], bevel=0)
    drainpipe(K, x0 + 0.25, H)
    drainpipe(K, x1 - 0.25, H)
    def side():
        S = Kit("TownHall_Side")
        sb = bays_of(0, D, 5)
        sh = []
        window_row(S, rng, sb, 1.0, 1.2, 2.0, t, look, sh, arch=True)
        window_row(S, rng, sb, fz[1] + 0.75, 1.3, 2.4, t, look, sh, caps=True)
        window_row(S, rng, sb, fz[2] + 0.55, 1.2, 1.5, t, look, sh)
        masonry(S, rng, 0, D, 0, H, sh, t)
        band(S, 0, D, fz[1], t, 2)
        cornice(S, 0, D, H, t)
        S.flush(K_M)
    as_side(side, x1)
    K.flush(K_M)
    people((("Office", x0 + W * 0.42, -3.0), ("Elder", x0 + W * 0.58, -4.5), ("Courier", x0 + W * 0.3, -6.0)))
    return dict(x0=x0, x1=x1, top=bz0 + 8.0, D=D)


def hospital(lot, x0, rng):
    """Long plaster wings on a stone storey, a stone centre bay with a pediment, a canopy over the door."""
    W, D = lot["width"], lot["depth"]
    x1 = x0 + W
    floors = int(lot.get("floors", 5))
    H = floors * 3.0
    t = KIT["wall_t"]
    K = Kit("Hospital")
    look = look_for(rng, frame="cream")
    core_box("Hospital_Core", x0, x1, D, H, "plaster")
    cx = (x0 + x1) / 2
    cw = 10.0
    nb = 13
    bays = [b for b in bays_of(x0, x1, nb) if abs(b - cx) > cw / 2 + 0.6]
    holes = []
    window_row(K, rng, bays, 0.9, 1.2, 1.7, t, look, holes, arch=True)
    for f in range(1, floors):
        window_row(K, rng, bays, f * 3.0 + 0.8, 1.15, 1.6 if f < floors - 1 else 1.3, t, look, holes)
    masonry(K, rng, x0, x1, 0.4, 3.4, holes, t)
    K.block("stone_dark", x0, x1, -0.09, t, 0, 0.4)
    band(K, x0, x1, 3.4, t, 2)
    plaster_wall("Hospital_Wall", K_M, x0, x1, 3.65, H, t, holes, "plaster")
    stone_patches(K, rng, x0, x1, 3.65, H, holes, 4)
    quoins(K, x0, 3.65, H, -1, t)
    quoins(K, x1, 3.65, H, 1, t)
    for f in range(2, floors):
        K.block("stone_dark", x0, x1, -0.06, t, f * 3.0 + 0.45, f * 3.0 + 0.55, 0.01)
    top = cornice(K, x0, x1, H - 0.45, t)
    coped_parapet(K, x0, x1, D, H, t, "plaster", h=0.75)
    K.flush(K_M)
    # the centre bay stands 0.6 m proud: built at the street line, then moved forward
    def centre():
        C = Kit("Hospital_Centre")
        tc = t + 0.6
        cb = bays_of(cx - cw / 2, cx + cw / 2, 3)
        ch = []
        ch.append(door(C, rng, cx, 2.2, 3.1, tc, dict(look, door="teal"), arch=True, steps=0))
        voussoirs(C, rng, cx, 3.1 - 1.1, 1.1, tc, ring=0.5)
        for f in range(1, floors):
            window_row(C, rng, cb, f * 3.0 + 0.8, 1.15, 1.6 if f < floors - 1 else 1.3, tc, look, ch, caps=f == 1)
        window_row(C, rng, (cb[0], cb[2]), 0.9, 1.2, 1.7, tc, look, ch, arch=True)
        masonry(C, rng, cx - cw / 2, cx + cw / 2, 0.0, H + 0.8, ch, tc)
        band(C, cx - cw / 2, cx + cw / 2, 3.4, tc, 2)
        fz = H + 0.8
        C.block("stone_b", cx - cw / 2, cx + cw / 2, -0.04, tc, fz, fz + 0.9, 0.03)
        cornice(C, cx - cw / 2, cx + cw / 2, fz + 0.9, tc)
        front_gable(C, "stone_a", cx - cw / 2 - 0.1, cx + cw / 2 + 0.1, fz + 1.38, 2.4, -0.1, tc)
        C.block("sign", cx - 0.45, cx + 0.45, -0.08, 0.0, fz + 1.7, fz + 2.6, 0.02)
        C.block("letters", cx - 0.09, cx + 0.09, -0.11, -0.08, fz + 1.8, fz + 2.5, 0.0)
        C.block("letters", cx - 0.3, cx + 0.3, -0.11, -0.08, fz + 2.06, fz + 2.24, 0.0)
        text("Hospital_Name", lot["name"].upper(), cx, -0.06, fz + 0.45, 0.42, "dark", 0.02)
        quoins(C, cx - cw / 2, 3.65, fz, -1, tc)
        quoins(C, cx + cw / 2, 3.65, fz, 1, tc)
        C.flush(K_M)
    before = set(COL.objects)
    centre()
    for ob in set(COL.objects) - before:
        ob.location.y -= 0.6
    # canopy over the door on four columns, with lanterns
    G = Kit("Hospital_Canopy")
    yc0, yc1 = -5.2, -0.6
    G.block("stone_dark", cx - 3.4, cx + 3.4, yc0, yc1, 3.4, 3.75)
    G.block("sign", cx - 3.3, cx + 3.3, yc0 - 0.06, yc0 + 0.02, 3.42, 3.73, 0.0)
    for (px, py) in ((-3.0, yc0 + 0.4), (3.0, yc0 + 0.4), (-3.0, yc1 - 0.6), (3.0, yc1 - 0.6)):
        G.cyl("stone_a", (cx + px, py, 1.7), "z", 0.2, 3.4, segs=16)
        G.block("stone_dark", cx + px - 0.3, cx + px + 0.3, py - 0.3, py + 0.3, 0.0, 0.3)
    G.flush(K_M)
    text("Hospital_CanopyName", "HOSPITAL", cx, yc0 - 0.08, 3.58, 0.22, "letters", 0.01)
    for px in (-2.0, 2.0):
        lantern("Hospital_CanopyLamp", cx + px, yc0 + 2.4, 3.1, K_M, 60)
    tree("Hospital_Tree", x0 + 3.0, -4.5, K_M, h=3.6, s=1.1)
    tree("Hospital_Tree", x1 - 3.0, -4.5, K_M, h=3.4, s=1.0)
    people((("Elder", cx - 1.2, -3.2), ("Shopkeeper", cx + 2.0, -6.5), ("PlayerA", x0 + W * 0.3, -5.0)))
    return dict(x0=x0, x1=x1, top=H + 4.7, D=D)


def lantern_arcade(lot, x0, rng):
    """The mall: a great glazed arch into a covered street under a glass vault, shops either side, the name on a gable."""
    W, D = lot["width"], lot["depth"]
    x1 = x0 + W
    H = 10.0
    t = 0.6
    K = Kit("Arcade")
    look = look_for(rng, frame="teal")
    cx = (x0 + x1) / 2
    gw, gs = 8.0, 5.2                                 # the great arch: width and springing height
    core = core_box("Arcade_Core", x0, x1, D, H, "plaster_b", t=t)
    cut(core, box("Arcade_PassCut", cx - gw / 2 + 0.4, cx + gw / 2 - 0.4, -1, D + 1, -1, H - 0.01, None, bevel=0))
    holes = [dict(x0=cx - gw / 2, x1=cx + gw / 2, z0=0.0, spring=gs)]
    voussoirs(K, rng, cx, gs, gw / 2, t, ring=0.7)
    # fanlight in the top of the great arch: glass with iron bars fanning out
    half_disc(K, "glass", cx, t * 0.5 - 0.03, t * 0.5 + 0.03, gs, gw / 2 + 0.05, segs=40)
    for k in range(9):
        a = math.pi * (k + 0.5) / 9
        K.cube("iron", (cx + (gw / 4) * math.cos(a), t * 0.5 - 0.06, gs + (gw / 4) * math.sin(a)), (0.07, 0.06, gw / 2), rot=(0, math.pi / 2 - a, 0), bevel=0)
    K.block("iron", cx - gw / 2, cx + gw / 2, t * 0.5 - 0.09, t * 0.5 - 0.03, gs - 0.1, gs + 0.06, 0)
    # shops either side: two storeys of arched shop windows, lit, with awnings below
    side_bays = [b for b in bays_of(x0, x1, 9) if abs(b - cx) > gw / 2 + 0.8]
    window_row(K, rng, side_bays, 0.5, 2.1, 2.7, t, look, holes, arch=True, lit=True)
    window_row(K, rng, side_bays, 5.0, 1.6, 2.6, t, look, holes, arch=True)
    masonry(K, rng, x0, x1, 0.0, H, holes, t)
    band(K, x0, x1, 4.4, t, 2)
    for b in side_bays:
        awning(K, b - 1.3, b + 1.3, 3.75, 1.4, ("teal", "cream"))
    top = cornice(K, x0, x1, H, t)
    coped_parapet(K, x0, x1, D, top, t, "stone_a", h=0.7, returns=True)
    # the gable over the arch carries the name; lanterns hang in the arch
    front_gable(K, "stone_a", cx - gw / 2 - 1.2, cx + gw / 2 + 1.2, top, 3.6, -0.1, t)
    K.block("stone_b", cx - gw / 2 - 1.2, cx + gw / 2 + 1.2, -0.12, t, top - 1.3, top, 0.03)
    text("Arcade_Name", lot["name"].upper(), cx, -0.15, top - 0.65, 0.78, "dark", 0.03)
    clock("Arcade_Clock", cx, -0.12, top + 1.4, 0.75)
    for (lx, ly) in ((cx, 2.0), (cx, 8.0), (cx, 14.0), (cx, 20.0), (cx, 26.0)):
        K.block("iron", lx - 0.02, lx + 0.02, ly - 0.02, ly + 0.02, 5.2, H, 0)
        lantern("Arcade_Lamp", lx, ly + 0.25, 5.0, K_M, 90)
    # the covered street inside: paving, shopfronts down both sides, a glass vault on iron ribs
    K.block("paving", cx - gw / 2, cx + gw / 2, 0, D, -0.02, 0.02, 0)
    for side in (-1, 1):
        xw = cx + side * (gw / 2 - 0.4)
        for k in range(int(D / 4)):
            yy = t + 1.0 + k * 4.0
            K.block("lit" if k % 3 else "glass", min(xw, xw - side * 0.05), max(xw, xw - side * 0.05), yy, yy + 2.8, 0.6, 3.2, 0)
            K.block("sign", min(xw, xw - side * 0.12), max(xw, xw - side * 0.12), yy - 0.2, yy + 3.0, 3.3, 3.9, 0.02)
    vr = gw / 2 - 0.4
    for k in range(int(D / 2.5) + 1):
        yy = 0.4 + k * 2.5
        for j in range(14):
            a0, a1 = math.pi * j / 14, math.pi * (j + 1) / 14
            am = (a0 + a1) / 2
            seg = math.hypot(vr * (math.cos(a1) - math.cos(a0)), 0.55 * vr * (math.sin(a1) - math.sin(a0)))
            K.cube("iron", (cx + vr * math.cos(am), yy, H + vr * 0.55 * math.sin(am)), (seg * 1.05, 0.1, 0.12),
                   rot=(0, math.atan2(-0.55 * math.cos(am), -math.sin(am)), 0), bevel=0)
    vault = cyl_y("Arcade_Vault", cx, D / 2, H, vr, D, K_M["glass"], segs=32)
    vault.scale = (1.0, 0.55, 1.0)
    vault.rotation_euler = (math.pi / 2, 0, 0)
    tables = Kit("Arcade_Tables")
    for k, tx in enumerate((x0 + 3.0, x1 - 3.0)):
        tables.cyl("iron", (tx, -2.4, 0.36), "z", 0.04, 0.72, segs=10)
        tables.cyl("wood", (tx, -2.4, 0.75), "z", 0.38, 0.05, segs=20)
    tables.flush(K_M)
    K.flush(K_M)
    people((("PlayerB", cx - 1.0, -2.5), ("Shopkeeper", cx + 1.5, 3.0), ("Office", cx + 0.5, 9.0), ("Courier", x0 + 6.0, -5.5)))
    return dict(x0=x0, x1=x1, top=top + 3.6, D=D)


def guild_house(lot, x0, rng):
    """A narrow, tall stone house with its stepped gable to the street and the guild's sign hung out."""
    W, D = lot["width"], lot["depth"]
    x1 = x0 + W
    H = 3 * CIVIC_FLOOR
    t = 0.5
    K = Kit("Guild")
    look = look_for(rng, frame="wood", door="wood_red")
    core_box("Guild_Core", x0, x1, D, H, "stone_a", t=t)
    cx = (x0 + x1) / 2
    holes = [door(K, rng, cx, 2.0, 2.8, t, look, arch=True, steps=2)]
    voussoirs(K, rng, cx, 2.8 - 1.0, 1.0, t, ring=0.5)
    bays = bays_of(x0, x1, 5)
    window_row(K, rng, bays, 0.9, 1.4, 2.0, t, look, holes, skip=(2,), arch=True)
    window_row(K, rng, bays, CIVIC_FLOOR + 0.8, 1.3, 2.2, t, look, holes, caps=True)
    window_row(K, rng, bays, 2 * CIVIC_FLOOR + 0.7, 1.2, 1.8, t, look, holes, caps=True)
    p = math.radians(KIT["roof_pitch"] + 18)
    ridge = H + (W / 2) * math.tan(p)
    nst = 6
    # windows in the gable before the stone is laid round them
    for (gx, gz, gw, gh) in ((cx - 2.6, H + 0.9, 1.0, 1.5), (cx + 2.6, H + 0.9, 1.0, 1.5), (cx, H + 3.0, 1.0, 1.6), (cx, H + 5.6, 0.7, 1.1)):
        holes.append(window(K, rng, gx, gz, gw, gh, t, look))
    masonry(K, rng, x0, x1, 0.0, H, holes, t)
    band(K, x0, x1, CIVIC_FLOOR, t, 1)
    band(K, x0, x1, 2 * CIVIC_FLOOR, t, 1)
    zb = band(K, x0, x1, H, t, 2)
    K.block("stone_b", cx - 4.6, cx + 4.6, -0.05, t, zb, zb + 0.5, 0.03)
    text("Guild_Name", lot["name"].upper(), cx, -0.08, zb + 0.25, 0.36, "dark", 0.02)
    sh = (ridge - zb) / nst
    gable_holes = [h for h in holes if h["z0"] > H]
    for k in range(nst):                              # the stepped gable, each step capped
        inset = (k + 0.5) * (W / 2 - 1.2) / nst
        za, zz = zb + k * sh, zb + (k + 1) * sh
        masonry(K, rng, x0 + inset, x1 - inset, za, zz, gable_holes, t)
        for sx in (x0 + inset, x1 - inset):
            K.block("stone_dark", sx - 0.55, sx + 0.55, -0.12, t + 0.1, zz, zz + 0.18)
            K.cube("stone_a", (sx, t / 2 - 0.05, zz + 0.45), (0.28, 0.28, 0.55), bevel=0.04)
    K.block("stone_dark", cx - 0.6, cx + 0.6, -0.12, t + 0.1, ridge, ridge + 0.2)
    K.cube("stone_a", (cx, t / 2, ridge + 0.75), (0.34, 0.34, 1.0), bevel=0.05)
    K.cyl("ochre", (cx, t / 2, ridge + 1.45), "z", 0.18, 0.36, segs=14, r2=0.02)
    # roof with its ridge running back from the gable
    def roof():
        R = Kit("Guild_Roof")
        tile_roof(R, rng, 0.9, D, W - 0.2, H + 0.5, ())      # starts behind the stepped gable so no eave shows in front
        R.flush(K_M)
    K_M["gable"] = K_M["stone_a"]
    turned(roof, x1 - 0.1, math.pi / 2)
    # the guild sign: a gilded key on a long iron arm
    ax = x1 - 1.6
    K.block("iron", ax - 0.03, ax + 0.03, -1.9, 0.0, 4.6, 4.68, 0)
    K.block("iron", ax - 0.03, ax + 0.03, -0.08, 0.0, 3.6, 4.68, 0)
    K.cyl("ochre", (ax, -1.5, 4.0), "y", 0.32, 0.08, segs=20)
    K.block("ochre", ax - 0.06, ax + 0.06, -1.54, -1.46, 3.0, 3.7, 0.01)
    K.block("ochre", ax, ax + 0.22, -1.54, -1.46, 3.05, 3.15, 0.0)
    K.block("ochre", ax, ax + 0.18, -1.54, -1.46, 3.25, 3.33, 0.0)
    K.block("iron", ax - 0.01, ax + 0.01, -1.51, -1.49, 4.32, 4.6, 0)
    wall_lamp(K, "Guild_DoorLamp", cx - 1.6, 2.4)
    wall_lamp(K, "Guild_DoorLamp", cx + 1.6, 2.4)
    K.flush(K_M)
    people((("Shopkeeper", cx + 1.0, -2.2), ("Elder", x0 + 4.0, -4.0)))
    return dict(x0=x0, x1=x1, top=ridge + 1.6, D=D)


def tower_face(K, rng, W, z0, z1, floors0, fh, inset=0.0, lit_share=0.25):
    """An office face: stone piers running up between columns of windows, dark spandrels at each floor.
    With an inset the face stands that far back from the street line (a setback storey)."""
    nb = max(3, round((W - 2 * inset) / 3.2))
    bw = (W - 2 * inset) / nb
    xa, yo = inset, inset
    for i in range(nb + 1):
        px = xa + i * bw
        K.block("stone_b", px - 0.35, px + 0.35, yo - 0.35, yo + 0.3, z0, z1, 0.03)
    f = floors0
    z = z0
    while z < z1 - fh * 0.5:
        for i in range(nb):
            a, b = xa + i * bw + 0.35, xa + (i + 1) * bw - 0.35
            K.block("stone_dark", a, b, yo - 0.12, yo + 0.3, z, z + 0.9, 0.02)
            K.block("lit" if rng.random() < lit_share else "glass", a, b, yo + 0.05, yo + 0.1, z + 0.9, z + fh, 0)
            K.block("wood", (a + b) / 2 - 0.04, (a + b) / 2 + 0.04, yo - 0.02, yo + 0.06, z + 0.9, z + fh, 0.01)
            K.block("wood", a, b, yo - 0.02, yo + 0.06, z + fh - 0.06, z + fh, 0.0)
        z += fh
        f += 1
    return nb


def office_tower(lot, x0, rng):
    """A stone office tower: an arcaded podium with the name, piers running up the shaft, set back near the top, a crown."""
    W, D = lot["width"], lot["depth"]
    x1 = x0 + W
    floors = int(lot.get("floors", 22))
    fh = 3.0
    H = floors * fh
    pod = 2 * fh + 0.6
    setb = H - 4 * fh
    t = 0.6
    K = Kit("Office")
    look = look_for(rng, frame="wood")
    core_box("Office_Core", x0, x1, D, setb, "stone_a", t=0.3)
    core_box("Office_CoreTop", x0 + 3, x1 - 3, D - 3, H, "stone_a", t=3.3)

    def podium(Kp, width):
        hp = []
        bb = bays_of(0, width, max(3, round(width / 4.4)))
        mid = len(bb) // 2
        for i, b in enumerate(bb):
            if i == mid:
                hp.append(door(Kp, rng, b, 2.4, 3.6, t, dict(look, door="wood"), arch=True, steps=1))
                voussoirs(Kp, rng, b, 3.6 - 1.2, 1.2, t, ring=0.5)
            else:
                h = window(Kp, rng, b, 0.5, 2.6, 3.2, t, look, arch=True, lit=True)
                hp.append(h)
                voussoirs(Kp, rng, b, h["spring"], 1.3, t, ring=0.45)
        window_row(Kp, rng, bb, fh + 1.4, 1.6, 1.7, t, look, hp)
        masonry(Kp, rng, 0, width, 0, pod, hp, t)
        zb = cornice(Kp, 0, width, pod, t)
        return zb, bb

    def front():
        F = Kit("Office_Front")
        zb, bb = podium(F, W)
        F.block("stone_b", 0, W, -0.05, 0.3, zb, zb + 0.8, 0.03)
        tower_face(F, rng, W, zb + 0.8, setb, 2, fh)
        cornice(F, 0, W, setb, 0.3)
        tower_face(F, rng, W, setb + 0.48, H, floors - 4, fh, inset=3.0)
        cornice(F, 3.0, W - 3.0, H, 0.3, yo=3.0)
        F.flush(K_M)
        text("Office_Name", lot["name"].upper(), W / 2, -0.08, zb + 0.4, 0.55, "dark", 0.02)
    before = set(COL.objects)
    front()
    for ob in set(COL.objects) - before:
        ob.location.x += x0

    def side():
        S = Kit("Office_Side")
        zb, bb = podium(S, D)
        S.block("stone_b", 0, D, -0.05, 0.3, zb, zb + 0.8, 0.03)
        tower_face(S, rng, D, zb + 0.8, setb, 2, fh)
        cornice(S, 0, D, setb, 0.3)
        tower_face(S, rng, D, setb + 0.48, H, floors - 4, fh, inset=3.0)
        cornice(S, 3.0, D - 3.0, H, 0.3, yo=3.0)
        S.flush(K_M)
    as_side(side, x1)
    # the setback roof terrace, then a crown: cornice, a lantern storey with a clock, a flagpole
    K.block("stone_dark", x0 + 2.7, x1 - 2.7, 2.7, D - 2.7, H + 0.4, H + 0.6)
    for (a, b, c, d) in ((x0, x1, 0, 0.4), (x0, x1, D - 0.4, D), (x0, x0 + 0.4, 0, D), (x1 - 0.4, x1, 0, D)):
        K.block("stone_a", a, b, c, d, setb + 0.48, setb + 1.4, 0.02)
    lw = 7.0
    lx, ly = (x0 + x1) / 2, 3.0 + lw / 2 + 0.6          # the crown stands near the front so it shows from the street
    K.block("stone_a", lx - lw / 2, lx + lw / 2, ly - lw / 2, ly + lw / 2, H + 0.5, H + 5.5, 0.03)
    for (px, py) in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
        K.block("stone_b", lx + px * (lw / 2) - 0.4, lx + px * (lw / 2) + 0.4, ly + py * (lw / 2) - 0.4, ly + py * (lw / 2) + 0.4, H + 0.5, H + 6.0, 0.03)
    cornice(K, lx - lw / 2, lx + lw / 2, H + 5.5, 0.3, yo=ly - lw / 2)
    clock("Office_Clock", lx, ly - lw / 2 - 0.05, H + 3.0, 1.5)
    hip_roof("Office_Roof", lx, ly, H + 6.0, lw + 0.6, lw + 0.6, 2.6, "tile")
    cyl("Office_Flag", lx, ly, H + 8.4, 0.05, 4.0, K_M["iron"], segs=8)
    box("Office_FlagCloth", lx + 0.05, lx + 1.8, ly - 0.02, ly + 0.02, H + 11.0, H + 12.2, K_M["teal"], bevel=0)
    K.flush(K_M)
    # a kiosk pavilion on the plaza in front
    P = Kit("Kiosk")
    kx, ky = x0 - 6.0, -9.0
    for (px, py) in ((-2, -1.5), (2, -1.5), (-2, 1.5), (2, 1.5)):
        P.cyl("iron", (kx + px, ky + py, 1.4), "z", 0.07, 2.8, segs=10)
    P.block("lit", kx - 1.9, kx + 1.9, ky - 1.4, ky + 1.4, 0.0, 1.0, 0.02)
    P.block("wood", kx - 2.1, kx + 2.1, ky - 1.6, ky + 1.6, 1.0, 1.1, 0.02)
    P.flush(K_M)
    hip_roof("Kiosk_Roof", kx, ky, 2.8, 5.0, 4.0, 1.4, "teal")
    tree("Office_Tree", x0 + 4.0, -7.0, K_M, h=3.8, s=1.2)
    people((("Office", x0 + W * 0.5, -3.5), ("PlayerA", x0 + W * 0.35, -6.0), ("Courier", kx + 1.0, ky - 2.4)))
    return dict(x0=x0 - 8.0, x1=x1, top=H + 12.0, D=D)


def flats_tower(lot, x0, rng):
    """A residential tower in the old-town manner: stone base, warm plaster, shuttered windows, a balcony on every other bay."""
    W, D = lot["width"], lot["depth"]
    x1 = x0 + W
    floors = int(lot.get("floors", 14))
    H = floors * 3.0
    t = KIT["wall_t"]
    K_M["gable"] = K_M["plaster_b"]
    look = look_for(rng, frame="cream", shutter="teal", shutters=True, boxes=True)
    core_box("Flats_Core", x0, x1, D, H, "plaster_b")

    def face(width, name, entrance):
        F = Kit(name)
        bb = bays_of(0, width, max(4, round(width / 3.2)))
        hl = []
        mid = len(bb) // 2
        for i, b in enumerate(bb):
            if entrance and i == mid:
                hl.append(door(F, rng, b, 2.0, 3.0, t, look, arch=True, steps=1))
                voussoirs(F, rng, b, 3.0 - 1.0, 1.0, t, ring=0.45)
                awning(F, b - 1.6, b + 1.6, 3.3, 1.2, ("teal", "cream"))
            else:
                h = window(F, rng, b, 0.9, 1.3, 1.9, t, look, arch=True, lit=entrance and i % 2 == 0)
                hl.append(h)
                voussoirs(F, rng, b, h["spring"], 0.65, t, ring=0.36)
        for f in range(1, floors):
            for i, b in enumerate(bb):
                if (i + f) % 2 == 0 and f > 1:
                    hl.append(balcony(F, rng, b, f * 3.0, t, look))
                else:
                    hl.append(window(F, rng, b, f * 3.0 + 0.8, 1.1, 1.6, t, look))
        masonry(F, rng, 0, width, 0.0, 3.4, hl, t)
        band(F, 0, width, 3.4, t, 2)
        plaster_wall(name + "_Wall", K_M, 0, width, 3.65, H, t, hl, "plaster_b")
        quoins(F, 0, 3.65, H, -1, t)
        quoins(F, width, 3.65, H, 1, t)
        cornice(F, 0, width, H - 0.45, t)
        F.flush(K_M)
    before = set(COL.objects)
    face(W, "Flats_Front", True)
    for ob in set(COL.objects) - before:
        ob.location.x += x0
    as_side(lambda: face(D, "Flats_Side", False), x1)
    # roof: a tiled hip roof with a roof garden pergola and a water tank on legs
    R = Kit("Flats_Roof")
    R.block("stone_dark", x0 - 0.3, x1 + 0.3, -0.3, D + 0.3, H, H + 0.25)
    R.flush(K_M)
    hip_roof("Flats_HipRoof", (x0 + x1) / 2, D / 2, H + 0.25, W + 0.6, D + 0.6, 6.0, "tile")
    T = Kit("Flats_Tank")
    for (px, py) in ((-1.3, -1.3), (1.3, -1.3), (-1.3, 1.3), (1.3, 1.3)):
        T.block("wood", x1 - 6 + px - 0.1, x1 - 6 + px + 0.1, D / 2 + py - 0.1, D / 2 + py + 0.1, H + 3.0, H + 7.0, 0.01)
    T.cyl("wood", (x1 - 6, D / 2, H + 8.2), "z", 1.7, 2.4, segs=18)
    T.cyl("tile_dark", (x1 - 6, D / 2, H + 9.7), "z", 1.85, 0.6, segs=18, r2=0.1)
    T.flush(K_M)
    tree("Flats_Tree", x0 + 3.0, -5.5, K_M, h=3.6, s=1.15)
    people((("PlayerB", x0 + W * 0.45, -3.0), ("Elder", x0 + W * 0.62, -2.4)))
    return dict(x0=x0, x1=x1, top=H + 9.0, D=D)


def workshop(lot, x0, rng):
    """A works: stone ground floor with carriage arches, loading doors stacked up the middle under a hoist, a tall chimney."""
    W, D = lot["width"], lot["depth"]
    x1 = x0 + W
    floors = int(lot.get("floors", 3))
    H = floors * 3.0
    t = KIT["wall_t"]
    K = Kit("Works")
    K_M["gable"] = K_M["plaster_b"]
    look = look_for(rng, frame="wood", door="wood")
    core_box("Works_Core", x0, x1, D, H, "plaster_b")
    cx = (x0 + x1) / 2
    bays = bays_of(x0, x1, 7)
    holes = []
    for b in (bays[1], bays[5]):                       # carriage arches with boarded double doors
        h = door(K, rng, b, 2.6, 2.95, t, look, arch=True, steps=0)
        holes.append(h)
        voussoirs(K, rng, b, h["spring"], 1.3, t, ring=0.45)
    window_row(K, rng, bays, 0.9, 1.4, 1.9, t, look, holes, skip=(1, 3, 5), arch=True)
    for f in range(1, floors):
        z = f * 3.0
        for i, b in enumerate(bays):
            if i == 3:                                 # loading door on every floor
                hh = dict(x0=b - 0.8, x1=b + 0.8, z0=z + 0.15, z1=z + 2.5, load=True)
                holes.append(hh)
                for k in range(6):
                    K.block("wood", b - 0.8 + k * 0.267, b - 0.8 + (k + 1) * 0.267, t - 0.12, t - 0.02, z + 0.15, z + 2.5, 0.014)
                K.block("stone_dark", b - 1.0, b + 1.0, -0.15, t, z + 0.02, z + 0.15)
                K.block("stone_dark", b - 1.0, b + 1.0, -0.1, t, z + 2.5, z + 2.75)
                K.block("iron", b - 0.75, b + 0.75, -0.05, 0.0, z + 0.9, z + 0.95, 0)
            else:
                holes.append(window(K, rng, b, z + 0.8, 1.6, 1.7, t, look))
    for h in holes:                                    # small panes: extra glazing bars on the wide works windows
        if "z1" in h and not h.get("load") and h["x1"] - h["x0"] > 1.5 and h["z0"] > 3:
            for q in (0.25, 0.75):
                xq = h["x0"] + (h["x1"] - h["x0"]) * q
                K.block(look["frame"], xq - 0.025, xq + 0.025, t - 0.13, t - 0.05, h["z0"], h["z1"], 0)
    masonry(K, rng, x0, x1, 0.0, 3.4, holes, t)
    band(K, x0, x1, 3.4, t, 2)
    plaster_wall("Works_Wall", K_M, x0, x1, 3.65, H, t, holes, "plaster_b")
    stone_patches(K, rng, x0, x1, 3.65, H, holes, 4)
    quoins(K, x0, 3.65, H, -1, t)
    quoins(K, x1, 3.65, H, 1, t)
    band(K, x0, x1, H - 0.25, t, 1)
    tile_roof(K, rng, x0, x1, D, H, ())
    # the hoist: a dormer gable over the loading doors with a beam, pulley, rope and a crate
    hx = bays[3]
    K.block("plaster_b", hx - 1.3, hx + 1.3, -0.1, 3.0, H - 0.2, H + 2.0, 0.03)
    front_gable(K, "plaster_b", hx - 1.3, hx + 1.3, H + 2.0, 1.2, -0.1, 3.0)
    ang = math.atan2(1.2, 1.3)
    for s in (-1, 1):
        K.cube("tile_dark", (hx + s * 0.62, 1.45, H + 2.62), (1.95, 3.5, 0.12), rot=(0, s * ang, 0), bevel=0.02)
    K.block("wood", hx - 0.1, hx + 0.1, -1.6, 1.0, H + 1.0, H + 1.25, 0.02)
    K.cyl("iron", (hx, -1.4, H + 0.85), "y", 0.16, 0.08, segs=16)
    K.block("iron", hx - 0.01, hx + 0.01, -1.42, -1.38, 4.2, H + 0.8, 0)
    K.block("wood", hx - 0.45, hx + 0.45, -1.8, -1.0, 3.6, 4.2, 0.03)
    # a tall chimney behind, lettered with the works' name
    chx, chy = x1 - 3.0, D + 3.0
    cyl("Works_Chimney", chx, chy, 0, 1.0, H + 14.0, K_M["stone_a"], segs=20, r_top=0.7)
    cyl("Works_ChimneyCap", chx, chy, H + 14.0, 0.85, 0.5, K_M["stone_dark"], segs=20, r_top=0.95)
    ch_h = H + 14.0
    for k, ch in enumerate(lot["name"].upper().replace(" ", "")):
        z = H + 13.2 - k * 0.78
        r = 1.0 - 0.3 * z / ch_h
        text("Works_Letter", ch, chx, chy - r - 0.03, z, 0.62, "letters", 0.02)
    K.block("wood", x0 + 1.0, x0 + 2.2, -1.3, -0.4, 0.0, 0.8, 0.03)
    K.cyl("wood", (x0 + 3.0, -0.9, 0.45), "z", 0.35, 0.9, segs=14)
    drainpipe(K, x1 - 0.2, H)
    K.flush(K_M)
    people((("Courier", bays[1] + 0.5, -2.0), ("PlayerB", bays[5] - 1.0, -3.6)))
    return dict(x0=x0, x1=x1, top=H + 6.0, D=D + 4)


def tram_depot(lot, x0, rng):
    """The tram depot: a stone front of three tall gabled bays, each with a great arched door, rails running in."""
    W, D = lot["width"], lot["depth"]
    x1 = x0 + W
    H = 8.0
    t = 0.6
    K = Kit("Depot")
    look = look_for(rng, frame="teal", door="teal")
    core = core_box("Depot_Core", x0, x1, D, H, "stone_a", t=t)
    nb = 3
    bw = W / nb
    holes = []
    for i in range(nb):
        b = x0 + (i + 0.5) * bw
        dw, ds = 5.0, 4.2
        holes.append(dict(x0=b - dw / 2, x1=b + dw / 2, z0=0.0, spring=ds))
        voussoirs(K, rng, b, ds, dw / 2, t, ring=0.55)
        if i == 1:                                   # the middle door stands open; a tram waits inside
            cut(core, box("Depot_DoorCut", b - dw / 2, b + dw / 2, -1, D - 1, -1, ds, None, bevel=0))
            cut(core, cyl_y("Depot_DoorArc", b, D / 2, ds, dw / 2, D))
            for s in (-1, 1):
                for k in range(5):
                    K.block("teal", b + s * (dw / 2 + 0.05 + k * 0.5) - 0.24, b + s * (dw / 2 + 0.05 + k * 0.5) + 0.24, -1.0, -0.9, 0.05, ds + 0.4, 0.012)
            tz = 4.0
            K.block("terra", b - 1.25, b + 1.25, tz, tz + 12.0, 0.35, 1.35, 0.03)
            K.block("cream", b - 1.25, b + 1.25, tz, tz + 12.0, 1.35, 2.75, 0.03)
            K.block("iron", b - 1.3, b + 1.3, tz - 0.05, tz + 12.05, 2.75, 2.95, 0.02)
            for k in range(6):
                K.block("lit", b - 1.27, b + 1.27, tz + 0.6 + k * 1.85, tz + 2.0 + k * 1.85, 1.5, 2.55, 0)
            K.block("lamp", b - 0.25, b + 0.25, tz - 0.06, tz, 1.1, 1.4, 0)
            K.block("core", b - dw / 2, b + dw / 2, 0, D - 1, -0.01, 0.01, 0)
            lantern("Depot_InLamp", b, 8.0, 5.0, K_M, 80)
        else:
            for k in range(10):
                xa = b - dw / 2 + k * dw / 10
                K.block("teal", xa + 0.01, xa + dw / 10 - 0.01, t - 0.12, t - 0.02, 0.0, ds, 0.012)
            half_disc(K, "teal", b, t - 0.12, t - 0.02, ds - 0.01, dw / 2 + 0.12, segs=32)
            K.block("cream", b - 1.2, b + 1.2, t - 0.16, t - 0.12, 1.3, 2.6, 0.01)
        for rx in (-0.72, 0.72):                       # rails running out to the street
            K.block("iron", b + rx - 0.04, b + rx + 0.04, -14.0, D - 2, 0.0, 0.05, 0)
    masonry(K, rng, x0, x1, 0.0, H, holes, t)
    cornice(K, x0, x1, H, t)
    for i in range(nb):
        b = x0 + (i + 0.5) * bw
        gh = bw / 2 * math.tan(math.radians(28))
        front_gable(K, "stone_b", b - bw / 2 + 0.05, b + bw / 2 - 0.05, H + 0.48, gh, -0.02, t)
        for s in (-1, 1):                           # coping along both rakes
            ang = math.radians(28)
            K.cube("stone_dark", (b + s * bw / 4, t / 2 - 0.05, H + 0.48 + gh / 2 + 0.1), (bw / 2 / math.cos(ang) + 0.2, t + 0.2, 0.2),
                   rot=(0, s * ang, 0), bevel=0.02)
        K.cyl("stone_dark", (b, -0.06, H + 2.0), "y", 0.85, 0.16, segs=28)
        K.cyl("glass", (b, -0.1, H + 2.0), "y", 0.62, 0.1, segs=28)
        K.cube("stone_a", (b, t / 2, H + 0.6 + gh), (0.4, 0.4, 0.7), bevel=0.04)

        def roof(ii=i):
            R = Kit("Depot_Roof%d" % ii)
            tile_roof(R, rng, 0, D, bw - 0.1, H + 0.48, ())
            R.flush(K_M)
        K_M["gable"] = K_M["stone_b"]
        turned(roof, b + bw / 2 - 0.05, math.pi / 2)
    K.block("stone_b", x0 + bw * 0.5 - 3.0, x0 + bw * 0.5 + 3.0, -0.06, t, H - 1.1, H - 0.3, 0.02)
    text("Depot_Name", lot["name"].upper(), x0 + bw * 0.5, -0.09, H - 0.7, 0.45, "dark", 0.02)
    for i in range(nb + 1):
        wall_lamp(K, "Depot_Lamp", x0 + i * bw + (0.6 if i == 0 else -0.6 if i == nb else 0.0), 4.6)
    K.flush(K_M)
    people((("Courier", x0 + bw * 1.5 + 3.2, -2.5), ("Office", x0 + bw * 0.5, -4.5)))
    return dict(x0=x0, x1=x1, top=H + 5.0, D=D)


BUILDERS = dict(town_hall=town_hall, hospital=hospital, lantern_arcade=lantern_arcade, guild_house=guild_house,
                office_tower=office_tower, flats_tower=flats_tower, workshop=workshop, tram_depot=tram_depot)


# ---- scene, cameras, render ----------------------------------------------------------------------
def scene_all():
    random.seed(8)
    scene = new_scene("BuildingKit08")
    global K_M
    K_M = kit_mats()
    K_M["gable"] = K_M["plaster"]
    lots = find_lots(SPECIAL)
    for w, lot in zip(SPECIAL, lots):
        r = lot["rect"]
        lot["depth"] = min(r[3] - r[1], w["depth"])
        lot["key"] = w["key"]
    n = len(lots)
    box("Ground", -60, n * PAD + 60, -160, 70, -0.3, 0, K_M["paving"], bevel=0)
    facts = []
    for i, lot in enumerate(lots):
        if ONLY and lot["key"] not in ONLY:
            continue
        x0 = i * PAD
        P = Kit("Pave%d" % i)
        P.block("paving", x0 - 12, x0 + lot["width"] + 12, -2.2, 0.0, 0.0, 0.14, 0)
        x = x0 - 12
        while x < x0 + lot["width"] + 12:
            P.block("stone_dark", x, x + 0.9, -2.4, -2.15, -0.05, 0.16, 0.025)
            x += 0.9
        P.flush(K_M)
        t0 = time.time()
        try:
            f = BUILDERS[lot["key"]](lot, x0, random.Random(300 + i))
            facts.append((lot, f))
            log("%s built in %.1fs" % (lot["key"], time.time() - t0))
        except Exception:
            log("FAILED building %s:\n%s" % (lot["key"], traceback.format_exc()))
    box("Fog", -60, n * PAD + 60, -60, 80, -0.2, 90, fog_material("M_Fog", 0.005, 0.6), bevel=0)
    sun_and_sky(scene, (0.78, -0.40, 0.48), 8.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.35)   # the kit daylight, ADR-0007
    return scene, facts


def cams(scene, facts):
    out = []
    for (lot, f) in facts:
        W = f["x1"] - f["x0"]
        cx = (f["x0"] + f["x1"]) / 2
        tall = f["top"] > W * 1.1
        res = (900, 1100) if tall else (1200, 800)
        lens = 26.0
        half_v = math.atan(18.0 / lens) if tall else math.atan(18.0 / lens * res[1] / res[0])
        half_h = math.atan(18.0 / lens * res[0] / res[1]) if tall else math.atan(18.0 / lens)
        ch = 7.5 if lot["key"] == "lantern_arcade" else 4.0 if tall else 1.7
        d_v = (f["top"] * 0.55) / math.tan(half_v) * 1.05
        d_h = (W * 0.62) / math.tan(half_h)
        dist = max(d_v, d_h, 12.0)
        sides = lot["key"] in ("town_hall", "office_tower", "flats_tower")      # these have a finished east side: look at it
        loc = (f["x1"] + W * 0.25 if sides else cx + W * 0.28, -dist, ch)
        tgt = (cx - W * 0.02, 0.0, f["top"] * 0.47)
        out.append((lot["key"], camera(scene, "Cam_" + lot["key"], loc, tgt, lens), res))
    return out


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
        for key, cam, res in cams(scene, facts):
            try:
                scene.camera = cam
                configure(scene, -0.15, res, SAMPLES)
                t = time.time()
                scene.render.filepath = OUT + "/" + NAME + "_" + key + ".png"
                bpy.ops.render.render(write_still=True, scene=scene.name)
                log("%s rendered in %.1fs" % (key, time.time() - t))
            except Exception:
                log("FAILED %s:\n%s" % (key, traceback.format_exc()))
    except Exception:
        log("FAILED:\n" + traceback.format_exc())
    log("DONE")


main()
