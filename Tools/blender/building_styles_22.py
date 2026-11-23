"""Brix Town - building styles 22: every build type of the town that is not a house, in three colourways each, and one
front picture per build type for the catalogue page (look test; not approved).

Run headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/building_styles_22.py
Quick checks: WASTED_ONLY=row_house_a,front_bank WASTED_SAMPLES=24   (a group key renders its three colourways)
From another checkout: WASTED_ROOT=C:/Projects/Wasted-remodelbuilds
WASTED_DRY=1 builds every job without rendering and logs the colours the kit picked (colourway "a").

Builds on building_kit_08.py (and through it building_kit_07.py, old_town_04.py ...): the same walls, openings, roofs,
signs and special buildings; every earlier script is read from WASTED_ROOT, not from the main checkout. The build types
are the ones Tools/worldgen/draw_builds.py finds in World/city_01 (its classify()), each built from its sample lot (the
lot with the lowest id of its type): name, kind, floors, district and the length of the side its door is on.

Types that share one design of the kit are one group (GROUPS below): the group's first type is rendered in colourways
a, b and c (<group>_<a|b|c>.png); every other type of the group gets its own front picture in colourway a
(front_<type>.png). Colourway "a" is the type as the kit draws it today (the colours its random picks give the sample
lot; every shutter in the building's own colour, as house_styles_20 does); "b" and "c" are read from the draft
Art/StyleTests/building_styles_22/building_styles.json. Every colour there is a palette entry of ART_DIRECTION 2.3, a
mix of two, or a tone of World/rules/building_kit.json, so the look stays the locked one (ADR-0004).
Open places (gardens, parks, field, green, Market Square, tram stop) are drawn from their lot data from above at a slant
(open_<type>.png). Tools/blender/building_styles_22_sheet.py makes the sheets and the front JPEGs.
"""
import builtins, os, json, zlib

ROOT = os.environ.get("WASTED_ROOT", "C:/Projects/Wasted").replace("\\", "/").rstrip("/")
_MAIN = "C:/Projects/Wasted/"
_open = builtins.open


def open(f, *a, **k):
    """The earlier scripts name the main checkout; read (and log) from WASTED_ROOT instead."""
    if isinstance(f, str):
        g = f.replace("\\", "/")
        if g.startswith(_MAIN):
            f = ROOT + "/" + g[len(_MAIN):]
    return _open(f, *a, **k)


_k = open(ROOT + "/Tools/blender/building_kit_08.py", encoding="utf-8").read()
_k = _k[:_k.rstrip().rfind("\nmain()")]                   # kit 08's buildings (and kit 07's kit), without running it
exec(compile(_k, "building_kit_08_helpers", "exec"), globals())

_FACES = ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3))


def _fast_block(self, mat, x0, x1, y0, y1, z0, z1, bevel=None):
    """Kit.block with the same box, made vertex by vertex: bmesh.ops on a big mesh touches every element, so a long
    stone front of thousands of blocks took minutes. Normals are recalculated at flush as before."""
    if x1 - x0 < 1e-3 or y1 - y0 < 1e-3 or z1 - z0 < 1e-3:
        return
    bev = KIT["bevel"] if bevel is None else bevel
    bev = min(bev, (x1 - x0) * 0.3, (y1 - y0) * 0.3, (z1 - z0) * 0.3)
    bm = self.bm(mat, bev)
    v = [bm.verts.new((x, y, z)) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]
    for f in _FACES:
        bm.faces.new([v[i] for i in f])


def _chunk_bm(self, mat, bevel):
    """Kit.bm, but a material's pieces go into meshes of at most a few thousand vertices (the same pieces; only split
    into more objects), for the same reason: a roof of thousands of barrel tiles made by bmesh.ops."""
    k = (mat, round(bevel, 4))
    bm = self.parts.get(k)
    if bm is not None and len(bm.verts) > 3000:
        self.__dict__.setdefault("_done", []).append((k, bm))
        bm = None
    if bm is None:
        bm = self.parts[k] = bmesh.new()
    return bm


_kit_flush = Kit.flush


def _chunk_flush(self, M):
    obs = []
    for i, ((mk, bev), bm) in enumerate(self.__dict__.pop("_done", [])):
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        ob = add_obj("%s_%s_%d" % (self.name, mk, i), bm, M[mk], bevel=bev)
        for md in ob.modifiers:
            if md.type == 'BEVEL':
                setp(md, "limit_method", 'ANGLE')
                setp(md, "harden_normals", False)
        obs.append(ob)
    return obs + _kit_flush(self, M)


Kit.block, Kit.bm, Kit.flush = _fast_block, _chunk_bm, _chunk_flush

NAME = "building_styles_22"
OUT = ROOT + "/Art/StyleTests"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"
DRY = bool(os.environ.get("WASTED_DRY"))
SAMPLES = int(os.environ.get("WASTED_SAMPLES", 32))
RES_SET, RES_FRONT = (1200, 740), (1200, 800)


def log(msg):
    line = time.strftime("%H:%M:%S") + " " + str(msg)
    print(line)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")


# ---- the build types, read the way draw_builds.py reads them ----------------------------------------------------------
import sys
if ROOT + "/Tools/worldgen" not in sys.path:
    sys.path.insert(0, ROOT + "/Tools/worldgen")            # draw_builds imports its neighbours
_d = {"__file__": ROOT + "/Tools/worldgen/draw_builds.py", "__name__": "draw_builds"}
exec(compile(_open(ROOT + "/Tools/worldgen/draw_builds.py", encoding="utf-8").read(), "draw_builds", "exec"), _d)
TYPES = _d["groups"]                                       # key -> dict(section, title, lots, sample, sample_block)

# a group: (key, title, builder, types - the first is the one shown in three colourways, options)
GROUPS = [
    ("row_house", "Row house", "kit07", ["terrace_14x12_2", "terrace_14x12_3", "terrace_12x16_2"], {}),
    ("court_flats", "Court flats", "kit07", ["flats_16x22", "flats_16x38"], {}),
    ("tower_home", "Residential tower", "flats_tower", ["tower_home"], {}),
    ("shop_front", "Shop: court, market and community", "kit07", ["shop_16x22", "shop_16x38", "market_shop", "community_shop"], {}),
    ("cafe_front", "Cafe: court, market, community and plaza", "kit07", ["food_16x22", "food_16x38", "market_cafe", "community_cafe", "plaza_cafe"], {}),
    ("kiosk", "Plaza kiosk", "kiosk", ["kiosk"], {}),
    ("works", "Workshop and works", "workshop", ["work_16x22", "work_16x38", "works"], {}),
    ("tower_office", "Office tower", "office_tower", ["tower_office"], {}),
    ("clinic", "Clinic: court and community", "kit07", ["health_16x22", "health_16x38", "community_clinic"], {}),
    ("hospital", "Hospital", "hospital", ["hospital_b_11_8_l0", "hospital_b_12_7_l0"], {}),
    ("civic_stone", "Civic stone building", "kit07", ["bank", "library", "museum", "police", "school_primary", "school_secondary"], {"kind": "work"}),
    ("showroom", "Showroom front: cinema, car dealer, furniture outlet", "kit07", ["cinema", "dealer", "furniture"], {"kind": "shop", "crates": False}),
    ("hall", "Town Hall", "town_hall", ["hall"], {}),
    ("guild", "Guild House", "guild_house", ["guild"], {}),
    ("arcade_mall", "Arcade and mall", "lantern_arcade", ["arcade_b_10_6_l0", "arcade_b_12_8_l0", "mall_b_1_5_l0", "mall_b_16_5_l0", "mall_b_7_0_l0"], {}),
    ("depot_hall", "Depot hall: depot and fire station", "tram_depot", ["depot", "fire"], {}),
    ("substation", "Substation", "kit07", ["substation"], {}),
]
OPEN = ["court_garden", "park_52", "park_168", "green", "field", "market", "stop"]
HOUSES = [k for k in TYPES if k.startswith("house_")]
# width and depth the kit 08 designs were drawn at (building_kit_08.SPECIAL), and whether they have a finished east side
K08 = dict(town_hall=(40, 22, True), hospital=(44, 18, False), lantern_arcade=(40, 34, False), guild_house=(22, 14, True),
           office_tower=(26, 26, True), flats_tower=(26, 26, True), workshop=(30, 16, False), tram_depot=(36, 24, False))

STYLES_FILE = OUT_DIR + "/building_styles.json"
STYLES = json.load(open(STYLES_FILE, encoding="utf-8")) if os.path.exists(STYLES_FILE) else {}


def sample(key):
    g = TYPES[key]
    lot, blk = dict(g["sample"]), g["sample_block"]
    lot["district"], lot["template"] = blk.get("district", "Old Town"), blk.get("template", "")
    x0, y0, x1, y1 = lot["rect"]
    w, d = x1 - x0, y1 - y0
    dx, dy = lot.get("door", (None, None))
    on_side = dx is not None and min(abs(dx - x0), abs(dx - x1)) < 0.6 and min(abs(dy - y0), abs(dy - y1)) > 0.6
    lot["front"], lot["deep"] = (d, w) if on_side else (w, d)   # the side with the door faces the street
    return lot, blk


# ---- colourway materials and the kit's look, swapped in by role -------------------------------------------------------
KIT_NAME = dict(teal="teal", wood_red="wood_red", ochre="ochre", dusty="dusty_blue", cream="cream", wood="wood",
                terra="terracotta", sign="sign_teal", plaster="cool", plaster_b="warm")
CAP = {}                  # what the kit picked while building (colourway a)
ROLE = None               # the colourway being drawn (b, c), or None for the kit's own (a)
_orig = dict(window=window, door=door, balcony=balcony, awning=awning, plaster_wall=plaster_wall, look_for=look_for)


def _sub(look):
    return dict(look, frame="v_frame", shutter="v_shutter", door="v_door")


def _cap(role, v):
    if v and not str(v).startswith("v_"):
        CAP.setdefault(role, v)


def window(K, rng, cx, z0, w, h, t, look, *a, **kw):
    _cap("frame", look.get("frame"))
    if look.get("shutters"):
        _cap("shutter", look.get("shutter"))
    return _orig["window"](K, rng, cx, z0, w, h, t, _sub(look), *a, **kw)


def door(K, rng, cx, w, h, t, look, *a, **kw):
    _cap("door", look.get("door"))
    return _orig["door"](K, rng, cx, w, h, t, _sub(look), *a, **kw)


def balcony(K, rng, cx, zf, t, look):
    return _orig["balcony"](K, rng, cx, zf, t, _sub(look))


def awning(K, x0, x1, z, out, stripes):
    CAP.setdefault("awning", list(stripes))
    return _orig["awning"](K, x0, x1, z, out, ("v_awn0", "v_awn1"))


def plaster_wall(name, Mx, x0, x1, z0, z1, t, holes, mat):
    CAP.setdefault("wall", mat)
    return _orig["plaster_wall"](name, Mx, x0, x1, z0, z1, t, holes, mat)


def look_for(rng, **kw):
    lk = _orig["look_for"](rng, **kw)
    _cap("frame", lk.get("frame"))
    _cap("door_look", lk.get("door"))                       # used only if no door is drawn with its own colour
    if lk.get("shutters"):
        _cap("shutter", lk.get("shutter"))
    return _sub(lk)


def shopfront_plain(K, rng, lot, x0, x1, t, look, holes):
    """kit 07's timber shopfront without the crates of produce on the pavement (a showroom, not a grocer)."""
    W = x1 - x0
    zt = SHOP_TOP
    holes.append(dict(x0=x0 + 0.4, x1=x1 - 0.4, z0=0.0, z1=zt))
    fm = "sign"
    n = max(3, round((W - 0.8) / 2.2))
    pw = 0.32
    xs = [x0 + 0.4 + i * (W - 0.8) / n for i in range(n + 1)]
    di = n // 2
    for xp in xs:
        K.block(fm, xp - pw / 2, xp + pw / 2, -0.16, t, 0.0, 2.55, 0.02)
        K.block("stone_dark", xp - pw / 2 - 0.04, xp + pw / 2 + 0.04, -0.2, t, 0.0, 0.3)
    for i in range(n):
        a, b = xs[i] + pw / 2, xs[i + 1] - pw / 2
        if i == di:
            K.block("glass", a, b, t - 0.15, t - 0.1, 0.0, 2.55, 0)
            K.block(fm, a, b, t - 0.2, t - 0.12, 1.9, 1.98, 0.01)
            K.block(fm, a, b, t - 0.3, t - 0.2, 2.3, 2.55, 0.01)
            continue
        K.block(fm, a, b, -0.06, t, 0.0, 0.5, 0.015)
        K.block("stone_dark", a - 0.02, b + 0.02, -0.12, t, 0.5, 0.58)
        K.block("lit", a, b, t - 0.08, t - 0.04, 0.58, 2.55, 0)
        K.block(fm, a, b, t - 0.12, t - 0.06, 2.0, 2.05, 0.008)
    K.block(fm, x0 + 0.25, x1 - 0.25, -0.22, t, 2.55, zt, 0.03)
    K.block("stone_dark", x0 + 0.2, x1 - 0.2, -0.3, t, zt, zt + 0.12)
    text("B_%s_Name" % lot["id"], lot["name"], (x0 + x1) / 2, -0.24, (2.55 + zt) / 2 + 0.02, 0.52, "letters", 0.03)
    awning(K, x0 + 0.6, x1 - 0.6, 2.5, 1.5, ("cream", "terra"))


def mix(a, b, s):
    return tuple(a[i] * (1 - s) + b[i] * s for i in range(3))


ROLES = ("v_frame", "v_shutter", "v_door", "v_awn0", "v_awn1", "kiosk_roof", "plaster", "plaster_b", "tile", "tile_dark",
         "stone_a", "stone_b", "stone_c", "stone", "sign", "teal")
NOW = {}                  # role -> the material the built objects carry for it now


def placeholders():
    """Materials of their own for the roles a colourway sets, so one build can be recoloured three times."""
    for r in ("v_frame", "v_shutter", "v_door", "v_awn0", "v_awn1"):
        K_M[r] = flat("M_P_" + r, (0.5, 0.5, 0.5))
    K_M["kiosk_roof"] = flat("M_P_KioskRoof", (0.20, 0.36, 0.37))
    NOW.clear()
    NOW.update({r: K_M[r] for r in ROLES})


def way_mats(way):
    """The materials colourway `way` gives each role it sets, in the kit's own way of painting (W1: matte, mottled)."""
    pal = {k: tuple(v) for k, v in STYLES["palette"].items()}
    acc = lambda n, nm: flat("M_V_" + nm, pal[n])
    new = {}
    for role in ("frame", "shutter", "door"):
        if way.get(role):
            new["v_" + role] = acc(way[role], role)
    if way.get("awning"):
        new["v_awn0"], new["v_awn1"] = acc(way["awning"][0], "awn0"), acc(way["awning"][1], "awn1")
    if way.get("front"):
        new["sign"] = acc(way["front"], "front")
    if way.get("kiosk_roof"):
        new["kiosk_roof"] = acc(way["kiosk_roof"], "kiosk_roof")
    if way.get("depot_doors"):
        new["teal"] = acc(way["depot_doors"], "depot_doors")
    if way.get("wall") and way["wall"] != "stone":
        lo, hi = STYLES["plasters"][way["wall"]]
        new["plaster"] = new["plaster_b"] = painted("M_V_Wall", tuple(lo), tuple(hi), noise_scale=0.7, bump=0.25)
    st = STYLES["stones"].get(way.get("stone") or "kit")
    if st and st.get("mix"):
        tgt, s = pal[st["mix"]], st["share"]
        for nm, (lo, hi) in dict(stone_a=((0.27, 0.26, 0.30), (0.52, 0.48, 0.43)), stone_b=((0.30, 0.28, 0.30), (0.56, 0.51, 0.45)),
                                 stone_c=((0.24, 0.23, 0.27), (0.47, 0.43, 0.39)), stone=((0.27, 0.26, 0.30), (0.52, 0.48, 0.43))).items():
            new[nm] = painted("M_V_" + nm, mix(lo, tgt, s), mix(hi, tgt, s), bump=0.35, noise_scale=1.4,
                              brick_scale=0.5 if nm == "stone" else None)
    if way.get("roof"):
        lo, hi = map(tuple, STYLES["roofs"][way["roof"]])
        new["tile"] = painted("M_V_Tile", lo, hi, noise_scale=2.2, bump=0.3)
        new["tile_dark"] = painted("M_V_TileDark", tuple(c * .7 for c in lo), tuple(c * .7 for c in hi), noise_scale=2.2, bump=0.3)
    return new


def recolour(scene, new):
    """Give every object of the scene the new material of each role (geometry stays exactly the same)."""
    swap = {NOW[r]: m for r, m in new.items() if NOW.get(r) is not None and NOW[r] is not m}
    n = 0
    for ob in scene.objects:
        for slot in getattr(ob, "material_slots", ()):
            if slot.material in swap:
                slot.material = swap[slot.material]
                n += 1
    NOW.update(new)
    return n


def kit_way(cap):
    """Colourway a as it was built: the colours the kit's random picks gave the lot."""
    if not cap.get("door") and cap.get("door_look"):
        cap["door"] = cap["door_look"]
    cap.pop("door_look", None)
    return {r: cap[r] for r in ("frame", "shutter", "door", "awning", "kiosk_roof") if cap.get(r)}


# ---- builders -----------------------------------------------------------------------------------------------------
def kiosk_pavilion(lot, x0, rng, s=1.6):
    """kit 08's plaza kiosk (office_tower), at the size of its lot: iron posts, a lit counter, a hip roof, a name board."""
    P = Kit("Kiosk")
    kx, ky = x0 + 2.2 * s, 1.6 * s
    for (px, py) in ((-2, -1.5), (2, -1.5), (-2, 1.5), (2, 1.5)):
        P.cyl("iron", (kx + px * s, ky + py * s, 1.4 * s), "z", 0.07 * s, 2.8 * s, segs=10)
    P.block("lit", kx - 1.9 * s, kx + 1.9 * s, ky - 1.4 * s, ky + 1.4 * s, 0.0, 1.0, 0.02)
    P.block("wood", kx - 2.1 * s, kx + 2.1 * s, ky - 1.6 * s, ky + 1.6 * s, 1.0, 1.1, 0.02)
    P.block("sign", kx - 1.7 * s, kx + 1.7 * s, ky - 1.55 * s - 0.08, ky - 1.55 * s, 2.8 * s - 0.55, 2.8 * s - 0.05, 0.02)
    P.flush(K_M)
    text("Kiosk_Name", lot["name"], kx, ky - 1.55 * s - 0.1, 2.8 * s - 0.3, 0.32, "letters", 0.02)
    K_M.setdefault("kiosk_roof", K_M["teal"])
    CAP.setdefault("kiosk_roof", "teal")
    hip_roof("Kiosk_Roof", kx, ky, 2.8 * s, 5.0 * s, 4.0 * s, 1.4 * s, "kiosk_roof")
    people((("Courier", kx + 1.0, -1.6), ("Shopkeeper", kx - 0.6, ky)))
    return dict(x0=x0, x1=x0 + 4.4 * s, top=4.2 * s + 0.4, D=3.2 * s)


def build(gkey, tkey, opts):
    """Build type tkey with the group's design at x=0..W on the street line y=0. Returns facts for the camera."""
    lot, blk = sample(tkey)
    builder = [g for g in GROUPS if g[0] == gkey][0][2]
    rng = random.Random(zlib.crc32(lot["id"].encode()))
    KIT["own_shutter"] = 1.0
    if builder == "kit07":
        lot = dict(lot, kind=opts.get("kind", lot["kind"]))
        lot["width"], lot["depth"] = min(lot["front"], 40.0), min(lot["deep"], 14.0)
        keep_fh, keep_sf, keep_pc = KIT["floor_h"], globals()["shopfront"], KIT["patch_count"]
        if int(lot.get("floors", 2)) <= 1 and lot["kind"] in ("shop", "food", "health", "work"):
            KIT["floor_h"] = 4.2                          # one tall storey: the shop, cafe or clinic front needs 3.5 m
            KIT["patch_count"] = 0                        # no room for bare-stone patches above it (they came out black)
        if opts.get("crates") is False:
            globals()["shopfront"] = shopfront_plain
        keep_base = dict(KIT["base_h"])
        if lot["kind"] == "food" and lot["district"] != "Outskirts":
            # kit 07 cuts only square holes in plaster; a cafe's arches need the stone ground storey it has in the
            # Old Town, or a community cafe (Northwood, ...) is drawn as a blank wall
            KIT["base_h"].setdefault(lot["district"], 3.4)
        try:
            f = building(lot, 0.0, rng)
        finally:
            KIT["floor_h"], KIT["patch_count"] = keep_fh, keep_pc
            KIT["base_h"] = keep_base
            globals()["shopfront"] = keep_sf
        if f.get("gate") is not None:
            tree("CourtTree", f["bays"][f["gate"]] + 1.2, f["D"] + 5.0, K_M, h=4.0, s=1.2)
        W = f["x1"] - f["x0"]
        people((("PlayerA", W * 0.35, -3.2), ("Elder", W * 0.62, -4.4)))
        sides = False
    elif builder == "kiosk":
        lot["width"], lot["depth"] = 4.4 * 1.6, 3.2 * 1.6
        f = kiosk_pavilion(lot, 0.0, rng)
        sides = True
    else:
        cap, deep, sides = K08[builder]
        lot["width"], lot["depth"] = min(lot["front"], cap), min(lot["deep"], deep)
        K_M["gable"] = K_M["plaster"]
        f = BUILDERS[builder](lot, 0.0, rng)
    log("  %s: built %s as %s (%s, %s floors, front %.1f m of %.1f, depth %.1f, district %s)" % (
        tkey, lot["id"], builder, lot["kind"], lot.get("floors"), lot["width"], lot["front"], lot["depth"], lot["district"]))
    return lot, f, sides


def street(x0, x1):
    box("Ground", x0 - 400, x1 + 400, -400, 300, -0.3, 0, K_M["paving"], bevel=0)
    P = Kit("Pave")
    P.block("paving", x0 - 14, x1 + 14, -2.2, 0.0, 0.0, 0.14, 0)
    x = x0 - 14
    while x < x1 + 14:
        P.block("stone_dark", x, x + 0.9, -2.4, -2.15, -0.05, 0.16, 0.025)
        x += 0.9
    P.flush(K_M)


def street_cam(f, sides, res, lens=30.0):
    """From the street at the corner: a building with a finished east side is seen past that corner; one with only its
    street front from a little to the west, never so far that its plain side shows."""
    x0, x1, top, D = f["x0"], f["x1"], f["top"], f.get("D", 10.0)
    W, cx = x1 - x0, (x0 + x1) / 2
    tx, ty = 18.0 / lens * 0.92, 18.0 / lens * res[1] / res[0] * 0.92
    tgt = Vector((cx, 0.0, top * 0.47))
    z = min(max(1.7 + 0.28 * top, 3.0), 16.0)
    pts = [Vector(p) for p in ((x0, 0, 0), (x1, 0, 0), (x0, 0, top), (x1, 0, top))]
    if sides:
        pts += [Vector((x1, D, 0)), Vector((x1, D, top))]

    def place(d):
        if sides:
            return Vector((cx + d * math.sin(math.radians(30)), -d * math.cos(math.radians(30)), z))
        return Vector((max(cx - d * math.tan(math.radians(24)), x0 + 0.6), -d, z))

    def fits(loc):
        fw = (tgt - loc).normalized()
        rt = fw.cross(Vector((0, 0, 1))).normalized()
        up = rt.cross(fw)
        for p in pts:
            v = p - loc
            depth = v.dot(fw)
            if depth <= 0.5 or abs(v.dot(rt) / depth) > tx or abs(v.dot(up) / depth) > ty:
                return False
        return True

    d = max(14.0, 0.45 * W)
    while not fits(place(d)) and d < 600:
        d *= 1.03
    return tuple(place(d)), tuple(tgt), lens


# ---- open places: the lot from above at a slant -------------------------------------------------------------------
def open_place(tkey):
    lot, blk = sample(tkey)
    x0, y0, x1, y1 = lot["rect"]
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    L = lambda x, y: (x - cx, -(y - cy))                     # map coords to the scene: north (smaller y) away from us
    w, d = x1 - x0, y1 - y0
    rng = random.Random(zlib.crc32(lot["id"].encode()))
    lawn = painted("M_Lawn", (0.20, 0.27, 0.13), (0.34, 0.40, 0.20), noise_scale=1.4, bump=0.3)
    box("Ground", -500, 500, -500, 500, -0.3, -0.12, K_M["paving"], bevel=0)
    paved = lot["kind"] in ("market", "stop")
    box("Lot", -w / 2, w / 2, -d / 2, d / 2, -0.12, 0.0, K_M["paving"] if paved else lawn, bevel=0)
    K = Kit("Open")
    m = 3.0
    inside = lambda p: x0 - m <= p[0] <= x1 + m and y0 - m <= p[1] <= y1 + m
    for (a, b, c, e) in ((-w / 2, w / 2, -d / 2, -d / 2 + 0.3), (-w / 2, w / 2, d / 2 - 0.3, d / 2),   # a stone kerb round it
                         (-w / 2, -w / 2 + 0.3, -d / 2 + 0.3, d / 2 - 0.3), (w / 2 - 0.3, w / 2, -d / 2 + 0.3, d / 2 - 0.3)):
        K.block("stone_dark", a, b, c, e, -0.05, 0.12, 0.02)
    for seg in ([] if paved else blk.get("paths", [])):
        (ax, ay), (bx, by) = seg[0], seg[-1]
        if not (inside((ax, ay)) or inside((bx, by))):
            continue
        ax, ay = max(x0, min(x1, ax)), max(y0, min(y1, ay))
        bx, by = max(x0, min(x1, bx)), max(y0, min(y1, by))
        (pa, qa), (pb, qb) = L(ax, ay), L(bx, by)
        hw = 1.2
        K.block("paving", min(pa, pb) - hw, max(pa, pb) + hw, min(qa, qb) - hw, max(qa, qb) + hw, -0.05, 0.03, 0)
    if tkey == "field":                                     # a pitch: cream lines and two goals
        for (a, b, c, e) in ((-w * 0.4, w * 0.4, -d * 0.38, -d * 0.38 + 0.15), (-w * 0.4, w * 0.4, d * 0.38 - 0.15, d * 0.38),
                             (-w * 0.4, -w * 0.4 + 0.15, -d * 0.38, d * 0.38), (w * 0.4 - 0.15, w * 0.4, -d * 0.38, d * 0.38),
                             (-0.08, 0.08, -d * 0.38, d * 0.38)):
            K.block("cream", a, b, c, e, -0.02, 0.05, 0)
        for sx in (-1, 1):
            gx = sx * w * 0.4
            for gy in (-3.6, 3.6):
                K.cyl("cream", (gx, gy, 1.2), "z", 0.07, 2.4, segs=10)
            K.block("cream", gx - 0.07, gx + 0.07, -3.6, 3.6, 2.33, 2.45, 0.02)
    seen = {}
    for p in blk.get("props", []):
        if not inside(p["pos"]):
            continue
        kind = p["kind"]
        seen[kind] = seen.get(kind, 0) + 1
        px, py = L(*p["pos"])
        along_x = p.get("along", "x") == "x"
        if kind == "bench":
            sx, sy = (1.9, 0.5) if along_x else (0.5, 1.9)
            K.block("wood", px - sx / 2, px + sx / 2, py - sy / 2, py + sy / 2, 0.42, 0.5, 0.02)
            for k in (-0.7, 0.7):
                ox, oy = (k, 0) if along_x else (0, k)
                K.block("iron", px + ox - 0.05, px + ox + 0.05, py + oy - 0.2, py + oy + 0.2, 0.0, 0.42, 0)
        elif kind == "flowerbed":
            K.block("stone_dark", px - 1.6, px + 1.6, py - 1.0, py + 1.0, 0.0, 0.35, 0.03)
            for k in range(5):
                blob("Flower", px - 1.2 + k * 0.6, py + rng.uniform(-0.4, 0.4), 0.55, 0.42, K_M[("plant", "terra", "leaf_light", "ochre", "plant")[k]])
        elif kind == "fountain":
            cyl("Fountain_Rim", px, py, 0.0, 3.0, 0.45, K_M["stone_dark"], segs=32)
            cyl("Fountain_Water", px, py, 0.0, 2.75, 0.5, K_M["water"], segs=32)
            cyl("Fountain_Col", px, py, 0.0, 0.35, 2.2, K_M["stone_a"], segs=16)
            cyl("Fountain_Bowl", px, py, 2.0, 0.9, 0.3, K_M["stone_b"], segs=20, r_top=1.1)
        elif kind == "stall":
            stall("Stall", px, py, K_M[("cream", "terra", "teal", "ochre", "dusty", "cream", "terra", "teal")[p.get("color", 0) % 8]], K_M,
                  [K_M["terra"], K_M["ochre"], K_M["plant"]])
        elif kind in ("kiosk", "shelter"):
            for (ox, oy) in ((-1.6, -0.9), (1.6, -0.9), (-1.6, 0.9), (1.6, 0.9)):
                K.cyl("iron", (px + ox, py + oy, 1.25), "z", 0.06, 2.5, segs=10)
            K.block("teal" if kind == "kiosk" else "wood", px - 1.9, px + 1.9, py - 1.2, py + 1.2, 2.5, 2.7, 0.03)
            if kind == "kiosk":
                K.block("lit", px - 1.5, px + 1.5, py - 0.8, py + 0.8, 0.0, 1.0, 0.02)
            else:
                K.block("wood", px - 1.5, px + 1.5, py + 0.5, py + 0.9, 0.45, 0.52, 0.02)
                K.block("glass", px - 1.6, px + 1.6, py + 0.85, py + 0.92, 0.2, 2.3, 0)
        elif kind == "lamp":
            K.cyl("iron", (px, py, 1.8), "z", 0.07, 3.6, segs=10)
            K.block("lamp", px - 0.15, px + 0.15, py - 0.15, py + 0.15, 3.6, 3.95, 0.02)
        elif kind in ("planter", "bin", "postbox", "bollard", "noticeboard", "timetable", "bikerack", "phonebox"):
            mat, sz, h = dict(planter=("terra", 0.45, 0.6), bin=("iron", 0.25, 0.9), postbox=("terra", 0.25, 1.2),
                              bollard=("iron", 0.12, 0.9), noticeboard=("wood", 0.7, 1.9), timetable=("iron", 0.1, 2.2),
                              bikerack=("iron", 0.9, 0.8), phonebox=("terra", 0.55, 2.4))[kind]
            K.block(mat, px - sz, px + sz, py - min(sz, 0.2), py + min(sz, 0.2), 0.0, h, 0.02)
            if kind == "planter":
                blob("Planter", px, py, h + 0.2, 0.42, K_M["plant"])
        elif kind in ("swing", "slide", "seesaw", "sandpit", "climber", "playground"):
            if kind == "swing":
                for ox in (-1.6, 1.6):
                    K.block("wood", px + ox - 0.08, px + ox + 0.08, py - 0.6, py + 0.6, 0.0, 2.3, 0.02)
                K.block("wood", px - 1.7, px + 1.7, py - 0.08, py + 0.08, 2.2, 2.35, 0.02)
                for ox in (-0.7, 0.7):
                    K.block("terra", px + ox - 0.25, px + ox + 0.25, py - 0.12, py + 0.12, 0.5, 0.56, 0.01)
            elif kind == "slide":
                K.block("ochre", px - 0.35, px + 0.35, py - 0.3, py + 0.3, 0.0, 1.8, 0.03)
                K.cube("teal", (px + 1.2, py, 0.9), (2.6, 0.6, 0.1), rot=(0, math.radians(38), 0), bevel=0.02)
            elif kind == "seesaw":
                K.block("iron", px - 0.15, px + 0.15, py - 0.15, py + 0.15, 0.0, 0.5, 0)
                K.block("dusty", px - 1.8, px + 1.8, py - 0.15, py + 0.15, 0.5, 0.6, 0.02)
            elif kind == "sandpit":
                K.block("wood", px - 1.6, px + 1.6, py - 1.2, py + 1.2, 0.0, 0.3, 0.03)
                K.block("cream", px - 1.4, px + 1.4, py - 1.0, py + 1.0, 0.0, 0.32, 0.02)
            elif kind == "climber":
                for ox in (-1, 1):
                    for oy in (-1, 1):
                        K.block("wood", px + ox - 0.07, px + ox + 0.07, py + oy - 0.07, py + oy + 0.07, 0.0, 2.0, 0.02)
                K.block("terra", px - 1.1, px + 1.1, py - 1.1, py + 1.1, 1.9, 2.05, 0.02)
        elif kind == "shelter_none":
            pass
    for t in blk.get("trees", []):
        if inside(t["pos"]):
            tx, ty = L(*t["pos"])
            tree("Tree", tx, ty, K_M, h=rng.uniform(3.2, 4.2), s=rng.uniform(1.0, 1.35))
    if tkey == "stop":                                      # the stop's own sign at the kerb
        K.cyl("iron", (w / 2 - 1.0, 0.0, 1.4), "z", 0.06, 2.8, segs=10)
        K.cyl("teal", (w / 2 - 1.0, -0.05, 2.7), "y", 0.32, 0.08, segs=24)
    K.flush(K_M)
    pts = lot.get("objects", {}).get("wait") or lot.get("objects", {}).get("customer") or lot.get("objects", {}).get("bench") or []
    who = ("PlayerA", "Elder", "Courier", "Office", "PlayerB", "Shopkeeper")
    ppl = []
    for i, p in enumerate(pts[::max(1, len(pts) // 5)][:5]):
        px, py = L(*p)
        ppl.append((who[i % len(who)], px + (0.0 if lot["kind"] in ("stop",) else 0.9), py - 0.6))
    if ppl:
        people(ppl)
    log("  %s: %s %s, %.0f x %.0f m, props %s, trees %d" % (tkey, lot["id"], lot["name"], w, d, seen,
                                                           sum(1 for t in blk.get("trees", []) if inside(t["pos"]))))
    S = max(w, d)
    dist = max(S * 1.3, 16.0)
    return (-dist * 0.28, -dist * 0.78, dist * 0.62), (0.0, 0.0, 0.0), 30.0


# ---- jobs ---------------------------------------------------------------------------------------------------------
SCENE = None


def fresh(name):
    global SCENE, K_M
    old = list(bpy.data.scenes)
    SCENE = new_scene(name)
    for s in old:
        try:
            bpy.data.scenes.remove(s)
        except Exception:
            pass
    try:
        bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
    except Exception:
        pass
    K_M = kit_mats()
    K_M["dusty"] = K_M.get("dusty", flat("M_DustyBlue", (0.30, 0.38, 0.52)))
    return SCENE


def shoot(scene, fname, loc, tgt, lens, res):
    cam = camera(scene, "Cam_" + fname, loc, tgt, lens)
    cam.data.clip_end = 2000
    scene.camera = cam
    configure(scene, -0.15, res, SAMPLES)
    t = time.time()
    scene.render.filepath = OUT_DIR + "/" + fname + ".png"
    bpy.ops.render.render(write_still=True, scene=scene.name)
    dt = time.time() - t
    log("%s rendered in %.1fs (%d objects)" % (fname, dt, len(scene.objects)))
    return dt


def tasks():
    """One build per task: a group's first type (rendered a, b, c), every other type (a), every open place."""
    out = []
    for gkey, title, builder, types, opts in GROUPS:
        out.append((gkey, types[0], ["%s_%s" % (gkey, w) for w in "abc"], opts))
        for t in types[1:]:
            out.append((gkey, t, ["front_" + t], opts))
    for t in OPEN:
        out.append((None, t, ["open_" + t], {}))
    return out


def wanted(gkey, tkey, job):
    return not ONLY or job in ONLY or (gkey or "open") in ONLY or tkey in ONLY      # "open": every open place


def main():
    global CAP
    os.makedirs(OUT_DIR, exist_ok=True)
    open(LOG, "a").close()
    log("Blender %s; samples %d; only %s%s" % (bpy.app.version_string, SAMPLES, ONLY, "; DRY" if DRY else ""))
    covered = [t for g in GROUPS for t in g[3]] + OPEN + HOUSES
    missing = [k for k in TYPES if k not in covered]
    if missing:
        log("NOT COVERED by any group: %s" % missing)
    caps = {}
    for gkey, tkey, jobs, opts in tasks():
        jobs = [j for j in jobs if wanted(gkey, tkey, j)]
        if not jobs:
            continue
        t0 = time.time()
        try:
            scene = fresh("BS22_" + tkey)
            _C.clear()                                      # the cloth and skin caches of old_town_04 and
            _M.clear()                                      # character_test_02 held the last scene's materials
            CAP = {}
            placeholders()
            if gkey is None:
                loc, tgt, lens = open_place(tkey)
                res = RES_FRONT
            else:
                lot, f, sides = build(gkey, tkey, opts)
                street(f["x0"], f["x1"])
                res = RES_SET if jobs[0].startswith(gkey) else RES_FRONT
                loc, tgt, lens = street_cam(f, sides, res)
                cap = {k: (KIT_NAME.get(v, v) if isinstance(v, str) else [KIT_NAME.get(x, x) for x in v]) for k, v in CAP.items()}
                caps[tkey] = cap
                log("  kit colours %s: %s" % (tkey, json.dumps(cap)))
                recolour(scene, way_mats(kit_way(cap)))
            orig = dict(NOW)                                 # colourway a as built
            sun_and_sky(SCENE, (0.55, -0.62, 0.56), 6.0, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.6)
            log("%s built in %.1fs (%d objects)" % (tkey, time.time() - t0, len(scene.objects)))
        except Exception:
            log("FAILED building %s:\n%s" % (tkey, traceback.format_exc()))
            continue
        for job in jobs:
            try:
                w = job[-1] if gkey and job.startswith(gkey + "_") else "a"
                target = dict(orig)
                if w != "a":
                    target.update(way_mats(STYLES["colourways"][gkey][w]))
                n = recolour(scene, target)
                if w != "a":
                    log("  %s: %d material slots recoloured" % (job, n))
                if not DRY:
                    shoot(scene, job, loc, tgt, lens, res)
            except Exception:
                log("FAILED %s:\n%s" % (job, traceback.format_exc()))
    if caps:
        with open(OUT_DIR + "/kit_colours_%s.json" % time.strftime("%H%M%S"), "w", encoding="utf-8") as fh:
            json.dump(caps, fh, indent=1)
    log("DONE")


main()
