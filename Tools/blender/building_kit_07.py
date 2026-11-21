"""Wasted - building kit 07: production-style building models in the locked old-town look (ART-001, ART-002).

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/building_kit_07.py").read(), "building_kit_07.py", "exec"))
or headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/building_kit_07.py

What is new compared with the boxes in old_town_04 and the playable slice:
  - stone is laid as real blocks in courses (bevelled, staggered, three close tones), so joints are geometry (W2)
  - windows and doors are holes with deep reveals, stone surrounds, sills, wooden frames and louvred shutters (W3)
  - arches are rings of voussoirs with a keystone; a gateway runs right through the building
  - roofs are barrel tiles laid in courses, or a parapet with coping; eaves sit on corbels; chimneys, drainpipes
  - quoins at corners and patches where plaster has fallen off, so walls are patchy, not flat (W1)
  - shop and cafe fronts: arched or timber shopfronts, awnings, blade signs and name boards with the lot's name

Every building is one call, building(lot), with a lot read from World/ (kind, name, floors, footprint). The
numbers that shape the kit are in KIT below, not in the code. Renders go to Art/StyleTests/building_kit_07_*.png.
"""
import bpy, bmesh, math, random, os, time, json, glob, traceback
from mathutils import Vector, Euler, Matrix

B = "C:/Projects/Wasted/Tools/blender/"
_o = open(B + "old_town_04.py", encoding="utf-8").read().split("\n# ---- scene 1 and 2")[0]
exec(compile(_o, "old_town_04_helpers", "exec"), globals())

NAME = "building_kit_07"
LOG = OUT + "/" + NAME + "_log.txt"
WORLD = "C:/Projects/Wasted/World/city_01/blocks/"
ONLY = None            # e.g. ("street",) to render one image; None renders everything
SAMPLES = 96
SCALE = 100            # resolution percentage
# quick checks from the command line: WASTED_ONLY=street,cafe_mina WASTED_SAMPLES=24 WASTED_SCALE=50
if os.environ.get("WASTED_ONLY"):
    ONLY = tuple(os.environ["WASTED_ONLY"].split(","))
SAMPLES = int(os.environ.get("WASTED_SAMPLES", SAMPLES))
SCALE = int(os.environ.get("WASTED_SCALE", SCALE))

# ---- kit numbers (tune here) ---------------------------------------------------------------
KIT = dict(
    floor_h=3.0,           # WORLD_MAP: 3 m a floor
    wall_t=0.32,           # depth of the front wall = depth of every window and door reveal
    course=0.5,            # stone course height (W2: 1 x 0.5 m blocks)
    block=1.0,             # nominal block length
    block_bevel=0.035,     # bevel on every stone block; makes the V-joint
    bevel=0.03,            # bevel on trims, sills, surrounds
    base_h={"Old Town": 3.4, "Centre": 3.4, "Outskirts": 0.45},   # stone ground storey (plinth only on the outskirts)
    bay=3.2,               # target width of one window bay
    win_w=1.1, win_h=1.6, win_sill=0.95, up_sill=0.8, top_win_h=1.45,
    door_w=1.3, door_h=2.3,
    roof_pitch=24.0,       # degrees
    eave=0.55,             # roof overhang
    tile_pitch=0.24,       # spacing of barrel tiles across the roof
    tile_course=0.42,      # length of one tile down the slope
    shutter_share=0.75,    # share of windows with shutters
    closed_share=0.15,     # share of shutters drawn closed
    box_share=0.18,        # share of sills with a flower box
    own_shutter=0.92,      # share of shutters in the building's own colour
    patch_count=3,         # patches of bare stone per plaster wall
)

# the example street: real lots from World/, facade width capped so a court range shows as one piece
STREET = (
    dict(name="Café Mina", cap=16.0, fallback=dict(kind="food", district="Old Town", floors=2, rect=[0, 0, 16, 22])),
    dict(name="Plum Court", cap=20.0, fallback=dict(kind="home", district="Old Town", floors=4, rect=[0, 0, 38, 16])),
    dict(name="Tanner Clinic", cap=16.0, fallback=dict(kind="health", district="Old Town", floors=2, rect=[0, 0, 16, 22])),
    dict(name="Linden Grocer", cap=16.0, fallback=dict(kind="shop", district="Old Town", floors=2, rect=[0, 0, 38, 16])),
)
ROW = (
    dict(name="1 North Wall Street", cap=14.0, fallback=dict(kind="home", district="Outskirts", floors=2, rect=[0, 0, 14, 12])),
    dict(name="3 North Wall Street", cap=14.0, fallback=dict(kind="home", district="Outskirts", floors=2, rect=[0, 0, 14, 12])),
)


# ---- reading lots ------------------------------------------------------------------------------
def find_lots(wanted):
    by_name = {}
    for f in sorted(glob.glob(WORLD + "*.json")):
        try:
            b = json.load(open(f, encoding="utf-8"))
        except Exception:
            continue
        for lot in b.get("lots", []):
            lot = dict(lot)
            lot["district"] = b.get("district", "Old Town")
            lot["template"] = b.get("template", "")
            by_name.setdefault(lot.get("name"), lot)
    out = []
    for w in wanted:
        lot = by_name.get(w["name"])
        if lot is None:
            log("lot '%s' not in World/, using fallback" % w["name"])
            lot = dict(w["fallback"], name=w["name"], id=w["name"].replace(" ", "_"))
        else:
            log("lot %s '%s' kind=%s floors=%s rect=%s district=%s" % (lot["id"], lot["name"], lot["kind"], lot.get("floors"),
                                                                       lot["rect"], lot["district"]))
        r = lot["rect"]
        lot["width"] = min(r[2] - r[0], w["cap"])
        lot["depth"] = min(r[3] - r[1], 14.0)
        out.append(lot)
    return out


# ---- a kit: pieces gathered per material into a few meshes --------------------------------------
class Kit:
    def __init__(self, name):
        self.name = name
        self.parts = {}

    def bm(self, mat, bevel):
        k = (mat, round(bevel, 4))
        if k not in self.parts:
            self.parts[k] = bmesh.new()
        return self.parts[k]

    def block(self, mat, x0, x1, y0, y1, z0, z1, bevel=None):
        if x1 - x0 < 1e-3 or y1 - y0 < 1e-3 or z1 - z0 < 1e-3:
            return
        bev = KIT["bevel"] if bevel is None else bevel
        bev = min(bev, (x1 - x0) * 0.3, (y1 - y0) * 0.3, (z1 - z0) * 0.3)
        M = Matrix.Translation(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)) @ Matrix.Diagonal((x1 - x0, y1 - y0, z1 - z0, 1.0))
        bmesh.ops.create_cube(self.bm(mat, bev), size=1.0, matrix=M)

    def cube(self, mat, center, size, rot=(0, 0, 0), bevel=None, base=None):
        bev = KIT["bevel"] if bevel is None else bevel
        bev = min(bev, min(size) * 0.3)
        M = Matrix.Translation(center) @ Euler(rot).to_matrix().to_4x4() @ Matrix.Diagonal((size[0], size[1], size[2], 1.0))
        if base is not None:
            M = base @ M
        bmesh.ops.create_cube(self.bm(mat, bev), size=1.0, matrix=M)

    def cyl(self, mat, center, axis, r, length, segs=12, r2=None, base=None):
        R = {"z": Matrix.Identity(4), "x": Matrix.Rotation(math.pi / 2, 4, 'Y'), "y": Matrix.Rotation(-math.pi / 2, 4, 'X')}[axis]
        M = Matrix.Translation(center) @ R
        if base is not None:
            M = base @ M
        bmesh.ops.create_cone(self.bm(mat, 0.0), cap_ends=True, cap_tris=False, segments=segs, radius1=r,
                              radius2=(r if r2 is None else r2), depth=length, matrix=M)

    def prism(self, mat, pts_yz, x0, x1):
        """Extrude a polygon given in (y, z) along x, e.g. a gable."""
        bm = self.bm(mat, 0.0)
        a = [bm.verts.new((x0, y, z)) for (y, z) in pts_yz]
        b = [bm.verts.new((x1, y, z)) for (y, z) in pts_yz]
        bm.faces.new(list(reversed(a)))
        bm.faces.new(b)
        n = len(pts_yz)
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((a[i], a[j], b[j], b[i]))

    def flush(self, M):
        obs = []
        for (mk, bev), bm in self.parts.items():
            bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
            ob = add_obj(self.name + "_" + mk, bm, M[mk], bevel=bev)
            if bev > 0:
                for md in ob.modifiers:
                    if md.type == 'BEVEL':
                        setp(md, "limit_method", 'ANGLE')
                        setp(md, "harden_normals", False)
            obs.append(ob)
        self.parts = {}
        return obs


def kit_mats():
    m = mats()
    m["stone_b"] = painted("M_StoneB", (0.30, 0.28, 0.30), (0.56, 0.51, 0.45), bump=0.35, noise_scale=1.4)
    m["stone_c"] = painted("M_StoneC", (0.24, 0.23, 0.27), (0.47, 0.43, 0.39), bump=0.35, noise_scale=1.4)
    m["stone_a"] = painted("M_StoneA", (0.27, 0.26, 0.30), (0.52, 0.48, 0.43), bump=0.35, noise_scale=1.4)
    m["core"] = painted("M_Core", (0.22, 0.20, 0.21), (0.36, 0.32, 0.30), noise_scale=0.8, bump=0.2)
    m["tile"] = painted("M_Tile", (0.34, 0.18, 0.13), (0.52, 0.30, 0.20), noise_scale=2.2, bump=0.3)
    m["tile_dark"] = painted("M_TileDark", (0.22, 0.12, 0.09), (0.36, 0.20, 0.14), noise_scale=2.2, bump=0.3)
    m["dusty"] = flat("M_DustyBlue", (0.30, 0.38, 0.52))
    m["sign"] = flat("M_SignTeal", (0.16, 0.30, 0.31))
    m["letters"] = flat("M_Letters", (0.70, 0.64, 0.50))
    m["plant"] = flat("M_Plant", (0.26, 0.38, 0.20), noise_scale=4.0)
    m["dark"] = solid((0.035, 0.03, 0.03), 0.9)
    return m


# ---- walls ---------------------------------------------------------------------------------------
def spans_minus(a0, a1, cuts):
    out, cur = [], a0
    for c0, c1 in sorted(cuts):
        if c1 <= cur or c0 >= a1:
            continue
        if c0 > cur:
            out.append((cur, c0))
        cur = max(cur, c1)
    if cur < a1:
        out.append((cur, a1))
    return out


def hole_cut(h, za, zb):
    """Horizontal extent a hole removes from the course [za, zb], or None."""
    if "spring" in h:
        r = (h["x1"] - h["x0"]) / 2
        if zb <= h["z0"] + 1e-3 or za >= h["spring"] + r - 1e-3:
            return None
        hw = r if za < h["spring"] else math.sqrt(max(r * r - (za - h["spring"]) ** 2, 0.0))
        if hw < 0.03:
            return None
        cx = (h["x0"] + h["x1"]) / 2
        return (cx - hw, cx + hw)
    if zb <= h["z0"] + 1e-3 or za >= h["z1"] - 1e-3:
        return None
    return (h["x0"], h["x1"])


def masonry(K, rng, x0, x1, z0, z1, holes, t, tones=("stone_a", "stone_b", "stone_c")):
    """Stone laid in courses around the holes; each block is its own bevelled piece."""
    n = max(1, round((z1 - z0) / KIT["course"]))
    ch = (z1 - z0) / n
    L = KIT["block"]
    for c in range(n):
        za, zb = z0 + c * ch, z0 + (c + 1) * ch
        cuts = [hc for hc in (hole_cut(h, za, zb) for h in holes) if hc]
        for (s0, s1) in spans_minus(x0, x1, cuts):
            x, first = s0, True
            while x < s1 - 1e-4:
                ln = rng.uniform(0.75, 1.25) * L
                if first and c % 2:
                    ln *= 0.55
                first = False
                if s1 - (x + ln) < 0.35 * L:
                    ln = s1 - x
                K.block(rng.choice(tones), x, x + ln, -rng.uniform(0.0, 0.018), t, za, zb, KIT["block_bevel"])
                x += ln


def voussoirs(K, rng, cx, spring, r, t, ring=0.45, tones=("stone_a", "stone_c")):
    n = max(7, int(math.pi * (r + ring / 2) / 0.36) | 1)
    for i in range(n):
        th = math.pi * (i + 0.5) / n
        key = i == n // 2
        rr = r + ring / 2 + (0.06 if key else 0.0)
        half = math.pi * (r + ring / 2) / n / 2 * 0.96
        K.cube("stone_dark" if key else tones[i % 2],
               (cx + rr * math.cos(th), (-0.07 if key else -0.04) / 2 + t / 2, spring + rr * math.sin(th)),
               (half * 2, t + (0.07 if key else 0.04), ring + (0.12 if key else 0.0)),
               rot=(0, math.pi / 2 - th, 0), bevel=KIT["block_bevel"])


def plaster_wall(name, M, x0, x1, z0, z1, t, holes, mat):
    """One smooth wall with real holes cut through it, edges softened afterwards (W3)."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation(((x0 + x1) / 2, t / 2, (z0 + z1) / 2)) @
                          Matrix.Diagonal((x1 - x0, t, z1 - z0, 1.0)))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    wall = bpy.data.objects.new(name, me)
    me.materials.append(M[mat])
    COL.objects.link(wall)
    rect = [h for h in holes if "z1" in h and h["z1"] > z0 + 0.01 and h["z0"] < z1 - 0.01]
    if rect:
        cb = bmesh.new()
        for h in rect:
            bmesh.ops.create_cube(cb, size=1.0, matrix=Matrix.Translation(((h["x0"] + h["x1"]) / 2, t / 2, (h["z0"] + h["z1"]) / 2)) @
                                  Matrix.Diagonal((h["x1"] - h["x0"], t + 1.0, h["z1"] - h["z0"], 1.0)))
        cm = bpy.data.meshes.new(name + "_Cut")
        cb.to_mesh(cm)
        cb.free()
        cutter = bpy.data.objects.new(name + "_Cut", cm)
        COL.objects.link(cutter)
        cut(wall, cutter)
    md = wall.modifiers.new("Bevel", 'BEVEL')
    md.width = 0.03
    md.segments = 2
    setp(md, "limit_method", 'ANGLE')
    return wall


def stone_patches(K, rng, x0, x1, z0, z1, holes, count):
    """Bare stone where plaster has fallen off; kept clear of windows."""
    for _ in range(count * 6):
        if count <= 0:
            break
        w, h = rng.uniform(0.9, 1.8), rng.uniform(0.6, 1.2)
        px, pz = rng.uniform(x0 + 0.6, x1 - 0.6 - w), rng.uniform(z0 + 0.3, z1 - 0.3 - h)
        clear = all(px + w < hh["x0"] - 0.45 or px > hh["x1"] + 0.45 or pz + h < hh["z0"] - 0.4 or
                    pz > hh.get("z1", hh.get("spring", 0) + 1) + 0.4 for hh in holes)
        if not clear:
            continue
        count -= 1
        rows = max(1, round(h / 0.33))
        for rI in range(rows):
            zz = pz + rI * h / rows
            xx = px + rng.uniform(0, 0.3)
            while xx < px + w - rng.uniform(0.0, 0.4):
                ln = rng.uniform(0.35, 0.6)
                K.block(rng.choice(("stone_b", "stone_c")), xx, xx + ln, -0.018, 0.02, zz, zz + h / rows, 0.025)
                xx += ln


def quoins(K, x, z0, z1, side, t):
    """Corner stones up an exposed corner; side = -1 for the west edge, +1 for the east."""
    z, i = z0, 0
    while z < z1 - 0.2:
        h = min(0.5, z1 - z)
        ln = 0.95 if i % 2 == 0 else 0.55
        xa, xb = (x, x + ln) if side < 0 else (x - ln, x)
        K.block("stone_a" if i % 2 else "stone_b", xa, xb, -0.04, t, z, z + h, KIT["block_bevel"])
        z += h
        i += 1


def band(K, x0, x1, z, t, steps=2, mat="stone_dark"):
    """String course or cornice: stepped, bevelled stone."""
    for s in range(steps):
        K.block(mat, x0 - 0.04 * s, x1 + 0.04 * s, -0.08 - 0.07 * s, t, z + 0.11 * s, z + 0.11 * s + 0.14)
    return z + 0.11 * (steps - 1) + 0.14


# ---- openings ------------------------------------------------------------------------------------
def window(K, rng, cx, z0, w, h, t, look, arch=False, surround=True, lit=False):
    """Returns the hole. Builds sill, surround, frame, glass, shutters and maybe a flower box."""
    x0, x1 = cx - w / 2, cx + w / 2
    hole = dict(x0=x0, x1=x1, z0=z0)
    if arch:
        hole["spring"] = z0 + h - w / 2
        top = z0 + h
    else:
        hole["z1"] = z0 + h
        top = z0 + h
    glass = "lit" if lit else "glass"
    fm = look["frame"]
    # sill and surround
    K.block("stone_dark", x0 - 0.22, x1 + 0.22, -0.17, t, z0 - 0.15, z0)
    if surround and not arch:
        K.block("stone_dark", x0 - 0.17, x0, -0.06, 0.05, z0, top)
        K.block("stone_dark", x1, x1 + 0.17, -0.06, 0.05, z0, top)
        K.block("stone_dark", x0 - 0.24, x1 + 0.24, -0.09, 0.05, top, top + 0.24)
        K.block("stone_a", cx - 0.13, cx + 0.13, -0.12, 0.05, top - 0.02, top + 0.3)
    # glass, frame, glazing bars
    gy0, gy1 = t - 0.05, t - 0.01
    rect_top = hole.get("spring", top)
    K.block(glass, x0, x1, gy0, gy1, z0, rect_top, 0)
    fy0, fy1 = t - 0.14, t - 0.04
    K.block(fm, x0, x0 + 0.08, fy0, fy1, z0, rect_top, 0.012)
    K.block(fm, x1 - 0.08, x1, fy0, fy1, z0, rect_top, 0.012)
    K.block(fm, x0, x1, fy0, fy1, z0, z0 + 0.08, 0.012)
    K.block(fm, cx - 0.035, cx + 0.035, fy0, fy1, z0, rect_top, 0.01)
    tz = z0 + (rect_top - z0) * 0.68
    K.block(fm, x0, x1, fy0, fy1, tz - 0.035, tz + 0.035, 0.01)
    if arch:
        r = w / 2
        K.cyl(glass, (cx, (gy0 + gy1) / 2, hole["spring"]), "y", r, gy1 - gy0, segs=24)
        K.block(fm, x0, x1, fy0, fy1, hole["spring"] - 0.04, hole["spring"] + 0.04, 0.01)
        for a in (math.radians(45), math.radians(135)):
            K.cube(fm, (cx + r * 0.5 * math.cos(a), (fy0 + fy1) / 2, hole["spring"] + r * 0.5 * math.sin(a)),
                   (0.06, fy1 - fy0, r), rot=(0, math.pi / 2 - a, 0), bevel=0.01)
    else:
        K.block(fm, x0, x1, fy0, fy1, top - 0.08, top, 0.012)
    # shutters: open flat against the wall, or drawn closed inside the reveal
    if look.get("shutters") and rng.random() < KIT["shutter_share"] and not arch:
        sm = look["shutter"] if rng.random() < KIT["own_shutter"] else rng.choice(("teal", "wood_red", "ochre", "dusty"))
        if rng.random() < KIT["closed_share"]:
            louvres(K, sm, x0 + 0.01, cx - 0.005, t - 0.24, z0 + 0.01, top - 0.01)
            louvres(K, sm, cx + 0.005, x1 - 0.01, t - 0.24, z0 + 0.01, top - 0.01)
        else:
            sw = w / 2
            louvres(K, sm, x0 - 0.2 - sw, x0 - 0.2, -0.09, z0, top)
            louvres(K, sm, x1 + 0.2, x1 + 0.2 + sw, -0.09, z0, top)
    if look.get("boxes") and rng.random() < KIT["box_share"]:
        K.block("terra", x0 - 0.05, x1 + 0.05, -0.42, -0.16, z0 + 0.0, z0 + 0.26, 0.03)
        for k in range(3):
            blob("Plant", x0 + (k + 0.5) * w / 3, -0.3, z0 + 0.36, 0.2 + rng.uniform(0, 0.06), K_M["plant"] if k % 2 else K_M["leaf_light"])
    return hole


def louvres(K, mat, x0, x1, y, z0, z1):
    """A shutter leaf: frame plus angled slats; chunky, never a thin plane."""
    K.block(mat, x0, x0 + 0.07, y, y + 0.07, z0, z1, 0.012)
    K.block(mat, x1 - 0.07, x1, y, y + 0.07, z0, z1, 0.012)
    K.block(mat, x0, x1, y, y + 0.07, z0, z0 + 0.09, 0.012)
    K.block(mat, x0, x1, y, y + 0.07, z1 - 0.09, z1, 0.012)
    K.block(mat, x0, x1, y + 0.01, y + 0.06, (z0 + z1) / 2 - 0.04, (z0 + z1) / 2 + 0.04, 0.012)
    n = max(3, int((z1 - z0 - 0.2) / 0.11))
    for i in range(n):
        z = z0 + 0.12 + (i + 0.5) * (z1 - z0 - 0.22) / n
        K.cube(mat, ((x0 + x1) / 2, y + 0.035, z), (x1 - x0 - 0.12, 0.085, 0.03), rot=(math.radians(-35), 0, 0), bevel=0.008)


def door(K, rng, cx, w, h, t, look, arch=False, steps=1, hood=False):
    x0, x1 = cx - w / 2, cx + w / 2
    hole = dict(x0=x0, x1=x1, z0=0.0)
    dm = look["door"]
    dy = t - 0.12
    if arch:
        hole["spring"] = h - w / 2
        K.cyl(dm, (cx, dy + 0.05, hole["spring"]), "y", w / 2, 0.1, segs=24)
        K.cyl("glass", (cx, dy - 0.01, hole["spring"] + 0.02), "y", w / 2 - 0.12, 0.04, segs=20)
        rect_top = hole["spring"]
    else:
        hole["z1"] = h
        rect_top = h
        K.block("stone_dark", x0 - 0.2, x0, -0.07, 0.05, 0, h)
        K.block("stone_dark", x1, x1 + 0.2, -0.07, 0.05, 0, h)
        K.block("stone_dark", x0 - 0.28, x1 + 0.28, -0.1, 0.05, h, h + 0.3)
    # boarded door with grooves between planks and two rails
    n = max(4, int(w / 0.19))
    for i in range(n):
        K.block(dm, x0 + i * w / n, x0 + (i + 1) * w / n, dy, dy + 0.1, 0.02, rect_top, 0.014)
    for zr in (0.45, rect_top - 0.5):
        K.block(dm, x0 + 0.05, x1 - 0.05, dy - 0.04, dy, zr - 0.07, zr + 0.07, 0.012)
    K.cyl("iron", (cx + w * 0.32, dy - 0.07, 1.05), "y", 0.035, 0.08, segs=10)
    for s in range(steps):
        K.block("stone_dark", x0 - 0.3 + 0.15 * s, x1 + 0.3 - 0.15 * s, -0.55 + 0.25 * s, t - 0.1, 0.0 + 0.16 * s, 0.16 + 0.16 * s)
    if hood:
        K.block("stone_dark", x0 - 0.45, x1 + 0.45, -0.6, 0.05, h + 0.32, h + 0.46)
        for sx in (x0 - 0.3, x1 + 0.3):
            K.cube("stone_dark", (sx, -0.25, h + 0.15), (0.16, 0.5, 0.34), bevel=0.03)
    return hole


def wall_lamp(K, name, x, z):
    K.block("iron", x - 0.04, x + 0.04, -0.5, 0.02, z + 0.3, z + 0.36, 0.0)
    K.block("iron", x - 0.09, x + 0.09, -0.06, 0.02, z + 0.1, z + 0.5, 0.01)
    lantern(name, x, -0.55, z, K_M, 70)


def text(name, body, x, y, z, size, mat, depth=0.025):
    cu = bpy.data.curves.new(name, 'FONT')
    cu.body = body
    cu.size = size
    cu.extrude = depth
    setp(cu, "bevel_depth", 0.004)
    setp(cu, "align_x", 'CENTER')
    setp(cu, "align_y", 'CENTER')
    ob = bpy.data.objects.new(name, cu)
    ob.location = (x, y, z)
    ob.rotation_euler = (math.pi / 2, 0, 0)
    cu.materials.append(K_M[mat])
    COL.objects.link(ob)
    return ob


# ---- roofs -----------------------------------------------------------------------------------------
def tile_roof(K, rng, x0, x1, D, H, chimneys=()):
    p = math.radians(KIT["roof_pitch"])
    e = KIT["eave"]
    half = D / 2 + e
    ridge = H + (D / 2) * math.tan(p)
    Ls = half / math.cos(p)
    W = x1 - x0 + 2 * 0.3
    for side in (-1, 1):                       # -1 front slope (street), +1 back slope
        yc = D / 2 + side * (half / 2)
        zc = ridge - (half / 2) * math.tan(p)
        ang = p if side < 0 else -p
        base = Matrix.Translation(((x0 + x1) / 2, yc, zc)) @ Matrix.Rotation(ang, 4, 'X')
        K.cube("tile_dark", (0, 0, 0), (W, Ls, 0.12), base=base, bevel=0.02)
        ncol = int(W / KIT["tile_pitch"])
        nrow = max(1, int(Ls / KIT["tile_course"]))
        for ci in range(ncol):
            xl = -W / 2 + (ci + 0.5) * W / ncol
            for ri in range(nrow):
                if rng.random() < 0.01:
                    continue                    # a missing tile now and then
                yl = -Ls / 2 + (ri + 0.5) * Ls / nrow
                tm = "tile_dark" if rng.random() < 0.18 else "tile"
                # cone axis runs along the slope; the end at the eave is wider and laps over the tile below
                M = base @ Matrix.Translation((xl + rng.uniform(-0.01, 0.01), yl, 0.07)) @ Matrix.Rotation(-math.pi / 2, 4, 'X')
                bmesh.ops.create_cone(K.bm(tm, 0.0), cap_ends=True, cap_tris=False, segments=9,
                                      radius1=0.098 if side < 0 else 0.074, radius2=0.074 if side < 0 else 0.098,
                                      depth=Ls / nrow * 1.04, matrix=M)
    nr = int(W / 0.42)
    for i in range(nr):
        K.cyl("tile", (x0 - 0.3 + (i + 0.5) * W / nr, D / 2, ridge + 0.1), "x", 0.15, W / nr * 1.02, segs=10, r2=0.13)
    # gables at both ends and corbels under the front eave
    for (gx0, gx1) in ((x0, x0 + KIT["wall_t"]), (x1 - KIT["wall_t"], x1)):
        K.prism("gable", [(0.0, H), (D, H), (D / 2, ridge)], gx0, gx1)
    nc = int((x1 - x0) / 0.62)
    for i in range(nc):
        cx = x0 + (i + 0.5) * (x1 - x0) / nc
        K.block("stone_dark", cx - 0.09, cx + 0.09, -0.32, 0.05, H - 0.26, H - 0.02, 0.025)
    K.block("wood", x0 - 0.3, x1 + 0.3, -e - 0.02, -e + 0.12, H - e * math.tan(p) - 0.2, H - e * math.tan(p) + 0.02, 0.02)
    for (cxr, cy) in chimneys:
        K.block("gable", cxr - 0.38, cxr + 0.38, cy - 0.32, cy + 0.32, H, ridge + 1.1, 0.03)
        K.block("stone_dark", cxr - 0.48, cxr + 0.48, cy - 0.42, cy + 0.42, ridge + 1.1, ridge + 1.26)
        for dx in (-0.17, 0.17):
            K.cyl("terra", (cxr + dx, cy, ridge + 1.5), "z", 0.1, 0.48, segs=12, r2=0.085)
    return ridge


def parapet(K, rng, x0, x1, D, H, t, mat, pots=True):
    K.block(mat, x0, x1, 0, t, H, H + 0.75, 0.03)
    for (a, b) in ((x0, x0 + t), (x1 - t, x1)):
        K.block(mat, a, b, 0, D, H, H + 0.75, 0.03)
    K.block("stone_dark", x0 - 0.06, x1 + 0.06, -0.1, t + 0.1, H + 0.75, H + 0.92)
    if pots:
        for i in range(int((x1 - x0) / 3.5)):
            px = x0 + 1.4 + i * 3.5 + rng.uniform(-0.4, 0.4)
            K.cyl("terra", (px, t / 2, H + 1.08), "z", 0.15, 0.32, segs=14, r2=0.18)
            blob("RoofPlant", px, t / 2, H + 1.36, 0.24, K_M["leaf_light"] if i % 2 else K_M["plant"])


def drainpipe(K, x, H):
    K.cyl("iron", (x, -0.14, H / 2), "z", 0.055, H - 0.3, segs=10)
    K.block("iron", x - 0.13, x + 0.13, -0.3, 0.0, H - 0.4, H - 0.12, 0.02)
    K.block("iron", x - 0.08, x + 0.08, -0.42, -0.06, 0.0, 0.18, 0.02)
    z = 0.9
    while z < H - 0.6:
        K.block("iron", x - 0.08, x + 0.08, -0.2, 0.02, z, z + 0.05, 0.0)
        z += 1.9


# ---- one building -----------------------------------------------------------------------------------
def building(lot, x0, rng):
    """Build lot `lot` with its street facade on y=0 facing -y, from x0 eastwards. Returns facts for the camera."""
    W, D = lot["width"], lot["depth"]
    x1 = x0 + W
    floors = max(1, int(lot.get("floors", 2)))
    fh = KIT["floor_h"]
    H = floors * fh
    t = KIT["wall_t"]
    kind, district = lot["kind"], lot.get("district", "Old Town")
    old = district != "Outskirts"
    court = kind == "home" and old and floors >= 3
    K = Kit("B_" + lot["id"])
    look = dict(frame=rng.choice(("cream", "wood", "teal")), shutter=rng.choice(("teal", "wood_red", "ochre", "dusty")),
                door=rng.choice(("wood_red", "wood", "teal")), shutters=kind in ("home", "food", "shop"), boxes=kind != "health")
    upper = "plaster_b" if rng.random() < 0.5 else "plaster"
    K_M["gable"] = K_M[upper]
    all_stone = kind == "health" or kind == "work"
    base_h = H if all_stone else KIT["base_h"].get(district, 0.45)
    log("  %s: %s, %d floors, %.0f x %.0f m, %s" % (lot["id"], kind, floors, W, D, "all stone" if all_stone else upper))

    # the body behind the front wall (reveals look into it); a court building has a gateway right through
    core = box("B_%s_Core" % lot["id"], x0 + 0.02, x1 - 0.02, t + 0.02, D, 0, H, K_M[upper if not all_stone else "stone_a"], bevel=0)
    nb = max(2, round(W / KIT["bay"]))
    if court and nb % 2 == 0:
        nb += 1
    bays = [x0 + (i + 0.5) * W / nb for i in range(nb)]
    holes = []
    gate = None

    # ---- ground storey
    if court:
        gate = nb // 2
        gw, gs = 2.8, 1.55
        holes.append(dict(x0=bays[gate] - gw / 2, x1=bays[gate] + gw / 2, z0=0.0, spring=gs))
        voussoirs(K, rng, bays[gate], gs, gw / 2, t)
        cb = box("B_%s_GateCut" % lot["id"], bays[gate] - gw / 2, bays[gate] + gw / 2, -1, D + 1, -1, gs, None, bevel=0)
        cut(core, cb)
        cut(core, cyl_y("B_%s_GateArc" % lot["id"], bays[gate], D / 2, gs, gw / 2, D + 2))
        K.block("paving", bays[gate] - gw / 2, bays[gate] + gw / 2, 0.0, D, -0.02, 0.01, 0)
        lantern("B_%s_GateLamp" % lot["id"], bays[gate] - gw / 2 + 0.15, D * 0.45, 2.1, K_M, 60)
    for i, cx in enumerate(bays):
        if i == gate:
            continue
        if kind == "food" and old:
            if i == nb - 1:
                holes.append(door(K, rng, cx, KIT["door_w"], 2.6, t, look, arch=True))
                voussoirs(K, rng, cx, 2.6 - KIT["door_w"] / 2, KIT["door_w"] / 2, t, ring=0.4)
            else:
                h = window(K, rng, cx, 0.55, 2.0, 2.45, t, dict(look, shutters=False), arch=True, lit=True)
                holes.append(h)
                voussoirs(K, rng, cx, h["spring"], 1.0, t, ring=0.4)
                K.block("wood", cx - 1.0, cx + 1.0, t - 0.16, t - 0.02, 0.0, 0.55, 0.015)
        elif kind == "health" or kind == "work":
            mid = i == nb // 2
            if mid:
                holes.append(door(K, rng, cx, 1.5, 2.6, t, dict(look, door="wood"), arch=True, steps=2))
                voussoirs(K, rng, cx, 2.6 - 0.75, 0.75, t, ring=0.4)
            else:
                h = window(K, rng, cx, 0.9, 1.25, 1.65, t, look, arch=True)
                holes.append(h)
                voussoirs(K, rng, cx, h["spring"], 0.625, t, ring=0.35)
        elif kind == "shop":
            pass                                  # timber shopfront across the whole front, below
        else:
            is_door = (i % 2 == 1) if old else (i == nb // 2)
            if is_door:
                holes.append(door(K, rng, cx, KIT["door_w"], 2.5 if old else KIT["door_h"], t, look, arch=old, hood=not old))
                if old:
                    voussoirs(K, rng, cx, 2.5 - KIT["door_w"] / 2, KIT["door_w"] / 2, t, ring=0.4)
                    wall_lamp(K, "B_%s_DoorLamp" % lot["id"], cx + 1.05, 2.15)
            else:
                h = window(K, rng, cx, KIT["win_sill"], KIT["win_w"], KIT["win_h"] if not old else 1.75, t, look, arch=old)
                holes.append(h)
                if old:
                    voussoirs(K, rng, cx, h["spring"], KIT["win_w"] / 2, t, ring=0.35)

    if kind == "shop":
        shopfront(K, rng, lot, x0, x1, t, look, holes)

    # ---- upper storeys
    top_band = None
    for f in range(1, floors):
        for i, cx in enumerate(bays):
            z0 = f * fh + KIT["up_sill"]
            wh = KIT["top_win_h"] if f == floors - 1 else KIT["win_h"]
            if kind == "food" and f == 1 and i == nb // 2 and nb >= 3:
                holes.append(balcony(K, rng, cx, f * fh, t, look))
                continue
            holes.append(window(K, rng, cx, z0, KIT["win_w"], wh, t, look, surround=old, arch=False))

    # ---- walls
    low = [hole_cut(h, 0.0, 0.4) for h in holes]
    for (a, b) in spans_minus(x0, x1, [c for c in low if c]):
        K.block("stone_dark", a, b, -0.09, t, 0, min(0.4, base_h))
    if all_stone:
        band_z = fh
        masonry(K, rng, x0, x1, 0.4, band_z, holes, t)
        zb = band(K, x0, x1, band_z, t, 2)
        frieze_top = zb + 0.36
        K.block("stone_b", x0, x1, -0.03, t, zb, frieze_top, KIT["block_bevel"])
        text("B_%s_Name" % lot["id"], lot["name"].upper(), (x0 + x1) / 2, -0.05, (zb + frieze_top) / 2, 0.25, "dark", 0.02)
        masonry(K, rng, x0, x1, frieze_top, H, holes, t)
    else:
        if base_h > 0.5:
            masonry(K, rng, x0, x1, 0.4, base_h, holes, t)
            band(K, x0, x1, base_h, t, 2)
            zw = base_h + 0.25
        else:
            zw = base_h
        if kind == "shop":
            zw = max(zw, SHOP_TOP)
        plaster_wall("B_%s_Wall" % lot["id"], K_M, x0, x1, zw, H, t, holes, upper)
        stone_patches(K, rng, x0, x1, zw, H, holes, KIT["patch_count"])
        if old:
            quoins(K, x0, zw, H, -1, t)
            quoins(K, x1, zw, H, 1, t)

    # ---- roof
    roof_kind = "tile" if (kind in ("home", "shop") or not old) else "parapet"
    if roof_kind == "tile":
        band(K, x0, x1, H - 0.25, t, 1)
        ch = ((x0 + 0.9, D / 2 + 1.2), (x1 - 0.9, D / 2 + 1.2)) if not old else ((x0 + W * 0.7, D / 2 + 1.0),)
        top = tile_roof(K, rng, x0, x1, D, H, ch)
    else:
        band(K, x0, x1, H - 0.4, t, 3)
        parapet(K, rng, x0, x1, D, H + 0.02, t, "stone_a" if all_stone else upper, pots=kind == "food")
        top = H + 1.0
    drainpipe(K, x1 - 0.2, H)

    # ---- kind-specific dressing
    if kind == "food":
        cafe_front(K, rng, lot, x0, x1, bays, nb, t, H)
    if kind == "health":
        for dx in (-1.35, 1.35):
            wall_lamp(K, "B_%s_Lamp" % lot["id"], bays[nb // 2] + dx, 2.3)
        px = bays[nb // 2] + 1.35
        K.block("sign", px - 0.3, px + 0.3, -0.06, 0.0, 1.35, 1.95, 0.02)
        K.block("letters", px - 0.05, px + 0.05, -0.09, -0.06, 1.47, 1.83, 0.0)
        K.block("letters", px - 0.18, px + 0.18, -0.09, -0.06, 1.6, 1.7, 0.0)
    K.flush(K_M)
    return dict(x0=x0, x1=x1, H=H, top=top, D=D, bays=bays, gate=gate)


SHOP_TOP = 3.45


def shopfront(K, rng, lot, x0, x1, t, look, holes):
    """Timber shopfront: pilasters, stall riser, big panes, a door, a fascia with the name and an awning."""
    W = x1 - x0
    zt = SHOP_TOP
    holes.append(dict(x0=x0 + 0.4, x1=x1 - 0.4, z0=0.0, z1=zt))
    fm = "sign"
    n = max(3, round((W - 0.8) / 2.2))
    pw = 0.32
    xs = [x0 + 0.4 + i * (W - 0.8) / n for i in range(n + 1)]
    di = n // 2
    for i, xp in enumerate(xs):
        K.block(fm, xp - pw / 2, xp + pw / 2, -0.16, t, 0.0, 2.55, 0.02)
        K.block("stone_dark", xp - pw / 2 - 0.04, xp + pw / 2 + 0.04, -0.2, t, 0.0, 0.3)
    for i in range(n):
        a, b = xs[i] + pw / 2, xs[i + 1] - pw / 2
        if i == di:
            K.block("glass", a, b, t - 0.15, t - 0.1, 0.0, 2.55, 0)
            for k in range(3):
                K.block(fm, a + k * (b - a) / 3, a + (k + 1) * (b - a) / 3, t - 0.2, t - 0.12, 0.0, 0.9, 0.012)
            K.block(fm, a, b, t - 0.2, t - 0.12, 1.9, 1.98, 0.01)
            K.block("glass", a, b, t - 0.26, t - 0.2, 0.9, 2.3, 0)
            K.block(fm, a, b, t - 0.3, t - 0.2, 2.3, 2.55, 0.01)
            continue
        K.block(fm, a, b, -0.06, t, 0.0, 0.7, 0.015)
        K.block("stone_dark", a - 0.02, b + 0.02, -0.12, t, 0.7, 0.78)
        K.block("lit", a, b, t - 0.08, t - 0.04, 0.78, 2.55, 0)
        K.block(fm, (a + b) / 2 - 0.03, (a + b) / 2 + 0.03, t - 0.12, t - 0.06, 0.78, 2.55, 0.008)
        K.block(fm, a, b, t - 0.12, t - 0.06, 2.0, 2.05, 0.008)
    K.block(fm, x0 + 0.25, x1 - 0.25, -0.22, t, 2.55, zt, 0.03)
    K.block("stone_dark", x0 + 0.2, x1 - 0.2, -0.3, t, zt, zt + 0.12)
    text("B_%s_Name" % lot["id"], lot["name"], (x0 + x1) / 2, -0.24, (2.55 + zt) / 2 + 0.02, 0.52, "letters", 0.03)
    awning(K, x0 + 0.6, x1 - 0.6, 2.5, 1.5, ("cream", "terra"))
    # crates of produce on the pavement
    for k, cx in enumerate((x0 + 1.3, x0 + 2.3, x1 - 2.4, x1 - 1.4)):
        K.block("wood", cx - 0.42, cx + 0.42, -1.35, -0.65, 0.0, 0.55, 0.025)
        K.block("wood", cx - 0.42, cx + 0.42, -1.2, -0.62, 0.55, 0.85, 0.025)
        for p in range(4):
            ellipsoid("Produce", Vector((cx - 0.24 + (p % 2) * 0.48 * 0.98, -1.18 + (p // 2) * 0.32, 0.6 + (p // 2) * 0.3)),
                      (0.14, 0.13, 0.11), K_M[("terra", "ochre", "plant", "leaf_light")[(k + p) % 4]])


def awning(K, x0, x1, z, out, stripes):
    """Sloped striped canvas with a hanging valance; strips are chunky (no thin planes)."""
    p = math.radians(20)
    n = max(4, int((x1 - x0) / 0.45))
    L = out / math.cos(p)
    for i in range(n):
        a = x0 + i * (x1 - x0) / n
        b = x0 + (i + 1) * (x1 - x0) / n
        m = stripes[i % 2]
        K.cube(m, ((a + b) / 2, -out / 2, z - out * math.tan(p) / 2), (b - a, L, 0.05), rot=(p, 0, 0), bevel=0.01)
        K.cube(m, ((a + b) / 2, -out - 0.02, z - out * math.tan(p) - 0.15), (b - a, 0.05, 0.3), bevel=0.01)
    for x in (x0 + 0.1, x1 - 0.1):
        K.cube("iron", (x, -out / 2, z - out * math.tan(p) / 2 - 0.06), (0.04, L, 0.04), rot=(p, 0, 0), bevel=0)


def balcony(K, rng, cx, zf, t, look):
    """French window on a stone slab with corbels and an iron rail."""
    w, h = 1.2, 2.3
    hole = dict(x0=cx - w / 2, x1=cx + w / 2, z0=zf + 0.15, z1=zf + 0.15 + h)
    K.block("stone_dark", cx - 1.1, cx + 1.1, -0.85, t, zf - 0.05, zf + 0.15)
    for dx in (-0.8, 0.0, 0.8):
        K.cube("stone_dark", (cx + dx, -0.38, zf - 0.32), (0.18, 0.66, 0.46), bevel=0.03)
    K.block("glass", hole["x0"], hole["x1"], t - 0.05, t - 0.01, hole["z0"], hole["z1"], 0)
    fm = look["frame"]
    for xx in (hole["x0"], cx - 0.035, hole["x1"] - 0.08):
        K.block(fm, xx, xx + (0.07 if xx == cx - 0.035 else 0.08), t - 0.14, t - 0.04, hole["z0"], hole["z1"], 0.01)
    K.block(fm, hole["x0"], hole["x1"], t - 0.14, t - 0.04, hole["z1"] - 0.08, hole["z1"], 0.01)
    K.block("stone_dark", hole["x0"] - 0.25, hole["x1"] + 0.25, -0.1, 0.05, hole["z1"], hole["z1"] + 0.26)
    rz = zf + 0.15
    K.block("iron", cx - 1.05, cx + 1.05, -0.84, -0.78, rz + 0.95, rz + 1.0, 0)
    K.block("iron", cx - 1.05, cx + 1.05, -0.84, -0.78, rz + 0.08, rz + 0.12, 0)
    for k in range(17):
        x = cx - 1.0 + k * 2.0 / 16
        K.block("iron", x - 0.015, x + 0.015, -0.83, -0.79, rz + 0.1, rz + 0.96, 0)
    for sx in (-1.05, 1.05):
        K.block("iron", cx + sx - 0.02, cx + sx + 0.02, -0.84, 0.0, rz + 0.95, rz + 1.0, 0)
        K.block("iron", cx + sx - 0.015, cx + sx + 0.015, -0.84, 0.0, rz + 0.1, rz + 0.96, 0)
    for dx in (-0.75, 0.75):
        K.cyl("terra", (cx + dx, -0.5, rz + 0.18), "z", 0.16, 0.34, segs=12, r2=0.2)
        blob("BalconyPlant", cx + dx, -0.5, rz + 0.5, 0.26, K_M["plant"])
    return hole


def cafe_front(K, rng, lot, x0, x1, bays, nb, t, H):
    """Awning over the arches, a blade sign with the cafe's name, tables on the pavement."""
    awning(K, x0 + 0.3, bays[-1] - 0.9, 3.35, 1.7, ("terra", "cream"))
    sx = x1 - 0.35
    K.block("iron", sx - 0.03, sx + 0.03, -1.25, 0.02, 3.95, 4.02, 0)
    K.block("iron", sx - 0.025, sx + 0.025, -0.08, 0.0, 3.0, 4.02, 0)
    K.block("sign", sx - 0.05, sx + 0.05, -1.2, -0.2, 3.05, 3.85, 0.02)
    for side in (-1, 1):
        nm = text("B_%s_Blade" % lot["id"], lot["name"], sx + side * 0.06, -0.7, 3.45, 0.17, "letters", 0.012)
        nm.rotation_euler = (math.pi / 2, 0, side * math.pi / 2)
    for k, tx in enumerate((x0 + 2.2, x0 + 5.6, x0 + 9.0)):
        ty = -2.6
        K.cyl("iron", (tx, ty, 0.36), "z", 0.04, 0.72, segs=10)
        K.cyl("iron", (tx, ty, 0.02), "z", 0.22, 0.04, segs=14)
        K.cyl("wood", (tx, ty, 0.75), "z", 0.38, 0.05, segs=20)
        for dx in (-0.62, 0.62):
            cx = tx + dx
            K.block("wood", cx - 0.21, cx + 0.21, ty - 0.21, ty + 0.21, 0.42, 0.47, 0.012)
            K.block("wood", cx + (0.17 if dx > 0 else -0.21), cx + (0.21 if dx > 0 else -0.17), ty - 0.21, ty + 0.21, 0.47, 0.88, 0.01)
            for lx in (-0.17, 0.17):
                for ly in (-0.17, 0.17):
                    K.block("wood", cx + lx - 0.02, cx + lx + 0.02, ty + ly - 0.02, ty + ly + 0.02, 0.0, 0.42, 0)
        ellipsoid("Cup", Vector((tx + 0.1, ty - 0.05, 0.82)), (0.045, 0.045, 0.04), K_M["cream"])


# ---- the street and the scenes ---------------------------------------------------------------------
K_M = {}


def ground(x0, x1):
    box("Ground", x0, x1, -30, 40, -0.3, 0, K_M["paving"], bevel=0)
    K = Kit("Pavement")
    K.block("paving", x0, x1, -2.0, 0.0, 0.0, 0.14, 0)
    x = x0
    while x < x1:
        K.block("stone_dark", x, x + 0.9, -2.2, -1.95, -0.05, 0.16, 0.025)
        x += 0.9
    K.flush(K_M)


def far_row(x0, x1, y):
    """The shaded opposite side and the far skyline, simple masses that fade into the haze."""
    rng = random.Random(11)
    x = x0
    while x < x1:
        w = rng.uniform(9, 16)
        h = rng.choice((6, 9, 12))
        box("Opp", x, x + w, y - 10, y, 0, h, K_M[rng.choice(("plaster", "plaster_b"))])
        box("OppBase", x, x + w, y - 0.12, y + 0.05, 0, 3.3, K_M["stone"])
        box("OppCap", x - 0.05, x + w + 0.05, y - 0.15, y + 0.12, h - 0.3, h, K_M["stone_dark"])
        for k in range(int(w / 3.2)):
            cx = x + 1.6 + k * 3.2
            for f in range(1, int(h / 3)):
                win("OppW", y, cx, f * 3 + 0.95, 1.1, 1.6, K_M, axis="y", sign=1,
                    shutter=K_M["teal"] if (k + f) % 3 == 0 else (K_M["wood_red"] if (k + f) % 3 == 1 else None))
        x += w


def street_scene():
    random.seed(5)
    scene = new_scene("BuildingKit_Street")
    global K_M
    K_M = kit_mats()
    lots = find_lots(STREET)
    rows = find_lots(ROW)
    ground(-30, 260)
    facts = []
    x = 0.0
    for i, lot in enumerate(lots):
        facts.append((lot, building(lot, x, random.Random(100 + i))))
        x += lot["width"]
    street_end = x
    x = 180.0
    for i, lot in enumerate(rows):
        facts.append((lot, building(lot, x, random.Random(200 + i))))
        x += lot["width"]
    # closing masses so the street ends in haze, the opposite side in shade
    far_row(-30, street_end + 30, -17.0)
    far_row(160, 230, -17.0)
    box("FarEnd", street_end + 4, street_end + 16, -15, 14, 0, 15, K_M["plaster"])
    box("FarEnd2", street_end + 30, street_end + 44, -6, 20, 0, 19, K_M["plaster_b"])
    for (lot, f) in facts:
        if f["gate"] is not None:
            tree("CourtTree", f["bays"][f["gate"]] + 1.2, f["D"] + 5.0, K_M, h=4.0, s=1.2)
            box("CourtBack", f["x0"], f["x1"], f["D"] + 12, f["D"] + 22, 0, f["H"], K_M["plaster_b"])
    tree("StreetTree", street_end + 2.0, -3.2, K_M, h=3.6, s=1.1)
    # people for scale (option B)
    cs = CHAR_STYLES["B"]
    for who, (px, py) in dict(PlayerA=(8.6, -4.2), Shopkeeper=(13.6, -1.4), Courier=(24.5, -6.5),
                              Elder=(38.0, -3.0), Office=(55.0, -4.6), PlayerB=(192.0, -4.0)).items():
        character3(CAST[who], cs, px, py)
    box("Fog", -30, 260, -40, 50, -0.2, 26, fog_material("M_Fog", 0.006, 0.6), bevel=0)
    # morning colours from ART_DIRECTION 2.4; the sun is turned to rake along the facades so their relief shows
    sun_and_sky(scene, (0.78, -0.40, 0.48), 8.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.35)
    return scene, facts, street_end


def cams(scene, facts, street_end):
    out = [("street", camera(scene, "Cam_Street", (-3.0, -8.0, 1.7), (street_end * 0.5, -0.5, 5.0), 22.0), (1280, 800)),
           ("detail", camera(scene, "Cam_Detail", (13.0, -4.2, 1.6), (21.0, 0.0, 3.4), 30.0), (1000, 800))]
    for (lot, f) in facts:
        W = f["x1"] - f["x0"]
        cx = (f["x0"] + f["x1"]) / 2
        span = max(W, f["top"] * 1.1)
        dist = 15.0                                   # stay inside the street, short of the opposite row
        lens = 18.0 / math.tan(math.atan(span * 0.62 / dist))
        loc = (cx - W * 0.4, -dist, 1.7)
        key = lot["name"].lower().replace(" ", "_").replace("é", "e")
        out.append((key, camera(scene, "Cam_" + key, loc, (cx + W * 0.04, 0.0, f["top"] * 0.42), lens), (1000, 800)))
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
        scene, facts, street_end = street_scene()
        log("street built: %d objects in %.1fs" % (len(scene.objects), time.time() - t0))
        try:
            bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
            log("saved .blend")
        except Exception:
            log("save failed:\n" + traceback.format_exc())
        for key, cam, res in cams(scene, facts, street_end):
            if ONLY and key not in ONLY:
                continue
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
