"""Brix Town - light kinds 54: a home's room at night lit by one kind of light at a time (F-LIGHTS; FURN-066, FURN-067;
ADR-0065). The owner, 2026-11-26: "when the room is lit by lights then the color should be decided by the light types",
and of the game's work on it: "first is need references to be built for it".

The room is the approved home room (Art/Reference/Indoor/IndoorHomeRoom, the living room of furniture_kit_18.py as
hours_50.py closes and lights it), on a night with no moon and the fire out, so that one kind of light alone lights it.
It holds one light of every kind of World/rules/lights.json: the kit's floor lamp and the side table's lamp as they
stand, and added here a desk lamp on the record cabinet, a pendant, candles on the piano, and two fittings the kits did
not have, a wall light (a bowl that throws its light up the wall) and a ceiling light (a shallow glass against the
ceiling). Each lit light has its kind's colour and strength from the file; the others are out, their glass unlit.

Headless:
    blender-launcher.exe -b --factory-startup --python <this checkout>/Tools/blender/light_kinds_54.py
    WASTED_KINDS=Pendant,Candles  some kinds only;  WASTED_SAMPLES (64), WASTED_SCALE (100);  WASTED_OUT  shots elsewhere
Renders go to Art/StyleTests/light_kinds_54/: Fixed_<Kind>.jpg, every kind at one exposure (the eye's for the approved
lamp-lit night of this room: its floor lamp, table lamp and fire), so the kinds' strengths can be told apart; and
Eye_<Kind>.jpg, the eye opened or closed for that kind as the game's eye does at night in a room, at most EYE_STOPS
either way. light_kinds_54_sheet.py lays them on one sheet.

The scripts this builds on name the main checkout (C:/Projects/Wasted); run from another checkout, their files are read
from that checkout instead (open() below).
"""
import builtins, os

HERE_54 = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))).replace("\\", "/") + "/"
MAIN_54 = "C:/Projects/Wasted/"


def open(path, *a, **k):
    """This checkout's file where a script names the main checkout's."""
    if isinstance(path, str):
        p = path.replace("\\", "/")
        if p.startswith(MAIN_54) and not p.startswith(HERE_54) and not p.startswith(MAIN_54 + ".claude/"):
            path = HERE_54 + p[len(MAIN_54):]
    return builtins.open(path, *a, **k)


os.environ["WASTED_PLACE"] = "rooms"
os.environ.setdefault("WASTED_OUT", HERE_54 + "Art/StyleTests/light_kinds_54")
try:
    _h = open(HERE_54 + "Tools/blender/hours_50.py", encoding="utf-8").read().replace("\r\n", "\n")
    exec(compile(_h[:_h.rindex("\ntry:\n    main_hours()")], "hours_50_helpers", "exec"), globals())
except Exception:                                           # the launcher shows no output: say why it did not start
    import traceback as _tb
    os.makedirs(os.environ["WASTED_OUT"], exist_ok=True)
    builtins.open(os.environ["WASTED_OUT"] + "/light_kinds_54_failed.txt", "w").write(_tb.format_exc())
    raise

# picture name, kind in lights.json, the start of the names of that light's parts
KINDS = [("FloorLamp", "floor_lamp", "FLamp_"), ("TableLamp", "table_lamp", "Lamp_"), ("DeskLamp", "desk_lamp", "Dl_"),
         ("WallLight", "wall_light", "Wl_"), ("Pendant", "ceiling_pendant", "Pen_"), ("CeilingLight", "ceiling_light", "Cl_"),
         ("Candles", "candles", "Cd_"), ("CoolCeilingLight", "cool_ceiling_light", "Cc_"), ("NeonBlue", "neon_blue", "Nb_"),
         ("NeonGreen", "neon_green", "Ng_"), ("NeonRed", "neon_red", "Nr_"), ("NeonAmber", "neon_amber", "Na_"),
         ("WallLantern", "wall_lantern", "Ln_")]
ONLY_KINDS = tuple(os.environ["WASTED_KINDS"].split(",")) if os.environ.get("WASTED_KINDS") else None
# Neon is an accent (the owner, 2026-11-26, of a room lit by a tube alone and all one colour: "Weaker neon"): weak, and
# shown with the room's own floor lamp, table lamp and fire lit, so the room keeps its colours and the tube tints its corner
ACCENTS = ("neon_blue", "neon_green", "neon_red", "neon_amber")
OWN_STARTS = ("FLamp_", "Lamp_", "Fp_", "Fire", "Ember", "Flame")   # the names of the room's own lit glass and flames
# the close-up of a fitting: the camera stands off towards this point of the floor plan, this far, a little above the
# light (a ceiling fitting is seen from standing height), through this lens
CLOSE = {"from": (0.0, -1.0), "off": 1.3, "above": 0.2, "eye_z": 1.75, "lens": 40.0}
EYE_STOPS = 2.5      # how far the eye opens or closes for one kind, past the lamp-lit night's (the game's NightOpenIndoors)
NIGHT_EYE = 70         # the mean brightness the eye seeks on a lamp-lit night (hours_50.py HOURS, NightLamps)
FIRE_OUT = (0.05, 0.04, 0.03)


def kind_id(name):
    """Which kind of lights.json a light stands for, by its name; None for a fire or a light that is no lamp."""
    for n, k in LIGHTS["blender_lights"].items():
        if name == n or name.startswith(n + "."):
            return k
    return None


def top_at(scene, x, y, default):
    """How high the top of what stands at (x, y) is."""
    try:
        vl = scene.view_layers[0]
        vl.update()
        hit, loc = scene.ray_cast(vl.depsgraph, Vector((x, y, 2.4)), Vector((0, 0, -1)))[:2]
        if hit and loc.z > 0.2:
            return loc.z
    except Exception:
        hlog("no ray at %.1f, %.1f:\n%s" % (x, y, traceback.format_exc()))
    return default


def wall_light(P, L, pre="Wl"):
    """A wall light: a plate on the wall, an arm, an open bowl of glass; its light is thrown up the wall (spread "up")."""
    z = 1.85
    m = M["iron"] if L["leg"] == "iron" else L["accent_wood"]
    rbox(P, pre + "_Plate", m, 0, -0.02, z, 0.13, 0.04, 0.22, 0.012)
    rod(P, pre + "_Arm", m, (0, -0.04, z - 0.02), (0, -0.15, z + 0.02), 0.016)
    cylz(P, pre + "_Bowl", M["shade"], 0, -0.15, z + 0.02, 0.06, 0.12, r_top=0.12, bev=0.01)
    sl = bpy.data.lights.new(pre + "_Light", 'SPOT')            # out of the bowl's open top: up the wall and onto the ceiling
    sl.energy = 40.0
    sl.color = (1.0, 0.6, 0.3)                               # as the kit's light(); its kind's colour is set from lights.json
    setp(sl, "spot_size", math.radians(150))
    setp(sl, "spot_blend", 0.5)
    setp(sl, "shadow_soft_size", 0.08)
    so = bpy.data.objects.new(pre + "_Light", sl)
    so.location = (0, -0.15, z + 0.16)
    so.rotation_euler = (math.radians(180), 0, 0)            # a spot shines down its own -z: turned to shine up
    COL.objects.link(so)
    so.parent = P


def ceiling_light(P, L, h=3.0, pre="Cl"):
    """A ceiling light: a rose and a shallow glass against the ceiling; a broad even light from above (spread "wash")."""
    m = M["iron"] if L["leg"] == "iron" else L["accent_wood"]
    cylz(P, pre + "_Rose", m, 0, 0, h - 0.035, 0.2, 0.035, bev=0.008)
    ball(P, pre + "_Shade", M["shade"], 0, 0, h - 0.07, 0.44, 0.44, 0.15)
    light(P, pre + "_Light", 0, 0, h - 0.24, 90.0)
    for ob in P.children:
        if ob.type == 'LIGHT':
            setp(ob.data, "shadow_soft_size", 0.2)           # a broad glass: soft shadows


def neon(P, L, pre, kind):
    """A neon tube on the wall: two iron holders and a glass tube 0.9 m long that glows in its kind's colour, the colour
    of its light (the owner, 2026-11-26: "we also need some cooler and even neon blue and green lights")."""
    z, w = 2.05, 0.9
    c = tuple(LIGHTS["colours"][LIGHTS["kinds"][kind]["colour"]]["colour"])
    glass = emissive("M_Neon_" + pre, c, 3.0)
    for i, x in enumerate((-w / 2 + 0.06, w / 2 - 0.06)):
        rbox(P, "%s_Holder%d" % (pre, i), M["iron"], x, -0.035, z, 0.04, 0.07, 0.06, 0.008)
    rod(P, pre + "_Tube", glass, (-w / 2, -0.075, z), (w / 2, -0.075, z), 0.022)
    light(P, pre + "_Light", 0, -0.16, z, 40.0)
    for ob in P.children:
        if ob.type == 'LIGHT':
            setp(ob.data, "shadow_soft_size", 0.25)          # a long tube: soft shadows


def more_lights(scene, L):
    """One light of each kind the room does not have yet."""
    rng = random.Random(54)
    P = group("Kind_Pendant", -0.7, 0.3, 0)
    place("d_pendant", P, L, rng)
    z = top_at(scene, 1.7, 1.9, 0.9)
    P = group("Kind_DeskLamp", 1.7, 1.9, 0)
    P.location.z = z
    place("s_lamp", P, L, rng)
    hlog("the desk lamp stands %.2f m up, on the record cabinet" % z)
    z = top_at(scene, -2.2, 1.9, 1.25)
    P = group("Kind_Candles", -2.2, 1.9, 0)
    P.location.z = z
    place("o_candles", P, L, rng)
    hlog("the candles stand %.2f m up, on the piano" % z)
    wall_light(group("Kind_WallLight", -3.2, 0.9, math.pi / 2), L)      # on the left wall, over the sofa
    ceiling_light(group("Kind_CeilingLight", 1.0, -0.9, 0), L)
    ceiling_light(group("Kind_CoolCeilingLight", -1.7, -1.1, 0), L, pre="Cc")
    wall_light(group("Kind_WallLantern", -3.2, -0.5, math.pi / 2), L, pre="Ln")       # the same fitting in the lantern's amber
    neon(group("Kind_NeonBlue", -3.2, 1.7, math.pi / 2), L, "Nb", "neon_blue")        # on the left wall, by the far corner
    neon(group("Kind_NeonGreen", 2.3, 2.2, 0), L, "Ng", "neon_green")                 # on the back wall, over the records
    neon(group("Kind_NeonRed", -2.2, 2.2, 0), L, "Nr", "neon_red")                    # over the piano
    neon(group("Kind_NeonAmber", 0.95, 2.2, 0), L, "Na", "neon_amber")                # between the fireplace and the records


def main_kinds():
    os.makedirs(HOURS_OUT, exist_ok=True)
    builtins.open(HOURS_LOG, "w").close()
    hlog("Blender %s, light kinds 54, from %s" % (bpy.app.version_string, HERE_54))
    scene = fresh("Furn_room_living")
    extra_mats()
    Ls = lines()
    spec, label = kit_room([k for k in KITS if k[0] == "living"][0], Ls)
    more_lights(scene, [l for l in Ls if l["key"] == ROOMS["living"][0]][0])
    greyer(ROOM_GREYER)
    bounce_more(ROOM_BOUNCE)
    nm, loc, tgt, lens = spec[0]
    cam = camera(scene, "Cam_living", loc, tgt, lens)

    # the glass and flames that are lit with a light, each with the material it is lit in
    lit = []
    for ob in scene.objects:
        if ob.type != 'MESH':
            continue
        for slot in ob.material_slots:
            m = slot.material
            if not m or not m.use_nodes or "Lit" in m.name or "Win" in m.name:
                continue
            if any(n.type == 'BSDF_PRINCIPLED' and n.inputs.get("Emission Strength") and n.inputs["Emission Strength"].default_value > 0
                   for n in m.node_tree.nodes):
                lit.append((ob, slot, m))
    L = Lights(scene, dict(day_lamps=0.0, day_fires=0.0))
    out = {}

    def unlit(m):
        """That material with its light out."""
        if m.name not in out:
            o = m.copy()
            o.name = m.name + "Out"
            fire = any(f in m.name for f in ("Ember", "Flame"))
            for n in o.node_tree.nodes:
                if n.type == 'BSDF_PRINCIPLED':
                    n.inputs["Emission Strength"].default_value = 0.0
                    n.inputs["Base Color"].default_value = (*(FIRE_OUT if fire else SHADE_OFF), 1.0)
            out[m.name] = o
        return out[m.name]

    every = [ob for ob, _ in L.lamps + L.fires]
    built = dict(L.lamps + L.fires)                                  # each light's strength as Lights() left it
    fires = set(ob for ob, _ in L.fires)
    hlog("lights: " + ", ".join("%s (%s)" % (ob.name, kind_id(ob.name) or "a fire") for ob in every))
    hlog("lit glass and flames: " + ", ".join(sorted(set(ob.name for ob, _, _ in lit))))
    night = [h for h in HOURS if h[0] == "NightLamps"][0]

    def light_up(which, starts):
        """Only these lights lit, each at its own strength (which: light -> watts), and only the glass and flames whose
        names start so."""
        for ob in every:
            ob.data.energy = 0.0
        L.lamps = list(which.items())
        L.fires = []
        for ob, slot, m in lit:
            on = ob.name.startswith(starts)
            ob.hide_render = (not on) and "Flame" in ob.name          # a flame that is out is not there
            slot.material = m if on else unlit(m)
        return L.set(night)

    # the exposure of the approved lamp-lit night: the room's own floor lamp, table lamp and fire
    own = {ob: e for ob, e in L.lamps + L.fires if ob.parent is None or not ob.parent.name.startswith("Kind_")}
    own = {ob: e for ob, e in own.items() if not _added(ob)}
    ex = light_up(own, OWN_STARTS)
    for ob, _, _ in lit:                                             # the added candles' flames stay out for it
        if ob.name.startswith("Cd_"):
            ob.hide_render = True
    base = eye(scene, cam, 1.3, ex, NIGHT_EYE, "the lamp-lit night")
    hlog("the lamp-lit night's exposure: %+.2f stops (%d of the room's own lights)" % (base, len(own)))
    if not ONLY_KINDS:
        shoot(scene, cam, 1.3 + base, "%s/Own_NightLamps.jpg" % HOURS_OUT)

    for name, kid, starts in KINDS:
        if ONLY_KINDS and name not in ONLY_KINDS:
            continue
        kind = dict(LIGHTS["kinds"][kid])
        kind.update(colour=LIGHTS["colours"][kind["colour"]]["colour"], kelvin=LIGHTS["colours"][kind["colour"]]["kelvin"],
                    colour_name=LIGHTS["colours"][kind["colour"]]["name"])
        which = {}
        for ob in every:
            if kind_id(ob.name) == kid:
                if ob in fires:                                      # candles are a fire: the one red of every fire, and its
                    which[ob] = built[ob]                            # strength as hours_50.py lights fires (the owner, 2026-11-26: "Fire red")
                else:
                    ob.data.color = tuple(kind["colour"])
                    which[ob] = kind["strength"]
        if not which:
            hlog("NO LIGHT of the kind %s" % kid)
            continue
        mine = list(which)                                           # this fitting's own light(s)
        lit_starts = (starts,)
        if kid in ACCENTS:                                         # shown with the room's own lamps and fire lit
            which.update(own)
            lit_starts += OWN_STARTS
        ex = light_up(which, lit_starts)
        hlog("%s: %d light(s) of %.0f W, %s, colour %s" % (name, len(which), sum(which.values()), ("%dK" % kind["kelvin"]) if kind["kelvin"] else "a colour", kind["colour"]))
        shoot(scene, cam, 1.3 + base, "%s/Fixed_%s.jpg" % (HOURS_OUT, name))
        own_ex = eye(scene, cam, 1.3, base, NIGHT_EYE, name)
        own_ex = max(base - EYE_STOPS, min(base + EYE_STOPS, own_ex))
        hlog("  %s: the eye %+.2f stops from the lamp-lit night's" % (name, own_ex - base))
        shoot(scene, cam, 1.3 + own_ex, "%s/Eye_%s.jpg" % (HOURS_OUT, name))
        # the fitting itself, close up, lit as in the picture before: from the middle of the room's side of it, about
        # 1.3 m off, the eye set for the closer view
        scene.view_layers[0].update()
        at = sum((ob.matrix_world.translation for ob in mine), Vector()) / len(mine)
        shades = [ob for ob in scene.objects if ob.type == 'MESH' and ob.name.startswith(starts + "Shade")]
        if shades:                                                   # aim at its shade, where it has one (a pendant's hangs over its light)
            pts = [ob.matrix_world @ Vector(c) for ob in shades for c in ob.bound_box]
            at = sum(pts, Vector()) / len(pts)
        high = at.z > 2.3                                            # against the ceiling: seen from standing height
        tgt = Vector((at.x, at.y, at.z + (0.15 if high else 0.0)))
        way = Vector((CLOSE["from"][0] - at.x, CLOSE["from"][1] - at.y, 0.0))
        way = way.normalized() if way.length > 0.05 else Vector((0.0, -1.0, 0.0))
        loc = tgt + way * CLOSE["off"]
        loc.z = CLOSE["eye_z"] if high else tgt.z + CLOSE["above"]
        close = camera(scene, "Cam_close_" + name, tuple(loc), tuple(tgt), CLOSE["lens"])
        close_ex = eye(scene, close, 1.3, own_ex, NIGHT_EYE, name + " close up")
        shoot(scene, close, 1.3 + close_ex, "%s/Close_%s.jpg" % (HOURS_OUT, name))
    hlog("DONE")


# ---- the fittings as objects, a row for each style line (WASTED_SHEET=1) -------------------------------------------
# The owner, 2026-11-26, with the picture of Art/Reference/Furniture/FurnitureLines.png: "i wanted this kind of reference".
# The light fittings stand on the pale ground in daylight, lit, each in the four lines, as furniture_kit_18.py's
# kit_sheet() lays a kit out. A fitting for a wall hangs on a piece of wall, one for a ceiling under a piece of ceiling.
VIG = None            # set while a fitting is built into a corner (below): the corner is its wall and ceiling


def wall_stub(P, w=0.6):
    if VIG is not None:
        return
    rbox(P, "Stub_Wall", M["plaster"], 0, 0.06, 1.2, w, 0.12, 2.4, 0.02)


def ceiling_stub(P, h):
    """A post and a narrow arm, so the fitting under it is seen from above."""
    if VIG is not None:
        VIG["ceil"] = h
        return
    rbox(P, "Stub_Post", M["plaster"], 0, 0.5, h / 2, 0.1, 0.1, h, 0.02)
    rbox(P, "Stub_Arm", M["plaster"], 0, 0.22, h + 0.05, 0.1, 0.66, 0.1, 0.02)


def f_floor_lamp(P, L, rng):
    floor_lamp(P, L)


def f_table_lamp(P, L, rng):
    lamp_small(P, L, 0, 0, 0)


def f_desk_lamp(P, L, rng):
    s_lamp(P, L, rng)


def f_candles(P, L, rng):
    o_candles(P, L, rng)


def f_wall_light(P, L, rng):
    wall_stub(P)
    wall_light(P, L)


def f_pendant(P, L, rng):
    ceiling_stub(P, 2.95)
    d_pendant(P, L, rng)


def f_ceiling_light(P, L, rng):
    ceiling_stub(P, 2.4)
    ceiling_light(P, L, h=2.4)


def _f_neon(pre, kind):
    def f(P, L, rng):
        wall_stub(P, 1.1)
        neon(P, L, pre, kind)
    f.__name__ = "f_" + kind
    return f


# ---- more fittings for the buying catalogue (the owner, 2026-11-26: "can you add more of these lights. all of these will
# be made available in buying catalog"): each a piece made as the kits' pieces are, f(P, L, rng), in the four lines
def _metal(L):
    return M["iron"] if L["leg"] == "iron" else L["accent_wood"]


def glass_of(pre):
    """The lit glass of a fitting, in its kind's named colour and as bright as its kind's glow says (FURN-067): a
    lantern's glass is amber, not the one pale shade of every lamp."""
    kid = LIGHTS["blender_lights"].get(pre + "_Light")
    if not kid:
        return M["shade"]
    kind = LIGHTS["kinds"][kid]
    name = "M_Glass_" + kind["colour"]
    return bpy.data.materials.get(name) or emissive(name, tuple(LIGHTS["colours"][kind["colour"]]["colour"]), 1.6 * kind["glow"])


def lantern_body(P, L, pre, x, y, z, s=1.0):
    """A lantern: a foot, four posts round a lit glass, a cap and a ring to carry or hang it by; z is its foot."""
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    w, h = 0.16 * s, 0.22 * s
    rbox(P, pre + "_Foot", m, x, y, z + 0.015 * s, w + 0.04 * s, w + 0.04 * s, 0.03 * s, 0.008)
    for i, (dx, dy) in enumerate(((-1, -1), (1, -1), (1, 1), (-1, 1))):
        rod(P, "%s_Post%d" % (pre, i), m, (x + dx * w / 2, y + dy * w / 2, z + 0.03 * s), (x + dx * w / 2, y + dy * w / 2, z + 0.03 * s + h), 0.012 * s)
    if L["round"]:
        ball(P, pre + "_Shade", glass_of(pre), x, y, z + 0.03 * s + h / 2, w * 0.95, w * 0.95, h * 0.95)
    else:
        rbox(P, pre + "_Shade", glass_of(pre), x, y, z + 0.03 * s + h / 2, w * 0.8, w * 0.8, h * 0.92, 0.01)
    cylz(P, pre + "_Cap", m, x, y, z + 0.03 * s + h, (w + 0.05 * s) / 1.4, 0.07 * s, r_top=0.02 * s, bev=0.006)
    top = z + 0.1 * s + h
    for i in range(8):                                              # the ring
        a0, a1 = i * math.pi / 4, (i + 1) * math.pi / 4
        rod(P, "%s_Ring%d" % (pre, i), m, (x + math.cos(a0) * 0.04 * s, y, top + 0.04 * s + math.sin(a0) * 0.04 * s),
            (x + math.cos(a1) * 0.04 * s, y, top + 0.04 * s + math.sin(a1) * 0.04 * s), 0.007 * s)
    light(P, pre + "_Light", x, y, z + 0.03 * s + h / 2, 25.0)
    return top + 0.08 * s


def f_arc_lamp(P, L, rng):
    """An arc floor lamp: a heavy foot, a stem that rises and arches forward, its shade hanging over a seat."""
    m = _metal(L)
    cylz(P, "Arc_Base", m, 0, 0.4, 0, 0.2, 0.06, r_top=0.17)
    pts = [(0, 0.4, 0.06), (0, 0.4, 1.35), (0, 0.3, 1.72), (0, 0.08, 1.95), (0, -0.22, 2.0), (0, -0.5, 1.9), (0, -0.62, 1.72)]
    for i in range(len(pts) - 1):
        rod(P, "Arc_Stem%d" % i, m, pts[i], pts[i + 1], 0.02)
    if L["round"]:
        ball(P, "Arc_Shade", M["shade"], 0, -0.62, 1.56, 0.4, 0.4, 0.32)
    else:
        cylz(P, "Arc_Shade", M["shade"], 0, -0.62, 1.44, 0.22, 0.28, r_top=0.12, bev=0.01)
    light(P, "Arc_Light", 0, -0.62, 1.5, 80.0)


def f_table_lantern(P, L, rng):
    lantern_body(P, L, "Lt", 0, 0, 0, 1.5)


def f_wall_lantern(P, L, rng):
    """A lantern hung from a bracket on the wall."""
    wall_stub(P)
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    rbox(P, "Ln_Plate", m, 0, -0.02, 2.0, 0.1, 0.04, 0.3, 0.012)
    rod(P, "Ln_Arm", m, (0, -0.04, 2.08), (0, -0.3, 2.08), 0.016)
    rod(P, "Ln_Stay", m, (0, -0.04, 1.9), (0, -0.24, 2.07), 0.012)
    top = lantern_body(P, L, "Ln", 0, -0.28, 1.62, 1.1)
    rod(P, "Ln_Hang", m, (0, -0.28, top - 0.02), (0, -0.28, 2.08), 0.008)


def f_chandelier(P, L, rng):
    """A chandelier: a hub on a rod, five arms, a small lit glass on each."""
    h = 2.6
    ceiling_stub(P, h)
    m = _metal(L)
    rod(P, "Ch_Rod", m, (0, 0, h), (0, 0, h - 0.45), 0.014)
    ball(P, "Ch_Hub", m, 0, 0, h - 0.5, 0.14, 0.14, 0.14)
    for i in range(5):
        a = i * 2 * math.pi / 5
        x, y = math.cos(a) * 0.36, math.sin(a) * 0.36
        rod(P, "Ch_Arm%d" % i, m, (0, 0, h - 0.5), (x * 0.6, y * 0.6, h - 0.62), 0.012)
        rod(P, "Ch_ArmB%d" % i, m, (x * 0.6, y * 0.6, h - 0.62), (x, y, h - 0.52), 0.012)
        cylz(P, "Ch_Cup%d" % i, m, x, y, h - 0.53, 0.035, 0.03, bev=0.005)
        if L["round"]:
            ball(P, "Ch_Shade%d" % i, M["shade"], x, y, h - 0.44, 0.11, 0.11, 0.11)
        else:
            cylz(P, "Ch_Shade%d" % i, M["shade"], x, y, h - 0.5, 0.045, 0.12, r_top=0.03, bev=0.006)
    light(P, "Ch_Light", 0, 0, h - 0.5, 100.0)


def f_track_spots(P, L, rng):
    """Track spots: a bar under the ceiling and three small heads that shine down."""
    h = 2.4
    ceiling_stub(P, h)
    m = _metal(L)
    rbox(P, "Tr_Bar", m, 0, 0, h - 0.03, 0.9, 0.05, 0.04, 0.01)
    for i, x in enumerate((-0.33, 0.0, 0.33)):
        rod(P, "Tr_Neck%d" % i, m, (x, 0, h - 0.05), (x, 0, h - 0.11), 0.012)
        cylz(P, "Tr_Head%d" % i, m, x, 0, h - 0.24, 0.06, 0.13, r_top=0.04, bev=0.008)
        ball(P, "Tr_Shade%d" % i, M["shade"], x, 0, h - 0.24, 0.1, 0.1, 0.04)
    light(P, "Tr_Light", 0, 0, h - 0.32, 75.0)


def f_string_lights(P, L, rng):
    """String lights: a cord sagging along the wall between two hooks, a small lit glass every hand's breadth."""
    wall_stub(P, 1.3)
    m = _metal(L)
    n, w, z = 8, 1.1, 2.15
    pts = [(-w / 2 + w * i / n, -0.05, z - 0.22 * math.sin(math.pi * i / n)) for i in range(n + 1)]
    for i in range(n):
        rod(P, "St_Cord%d" % i, m, pts[i], pts[i + 1], 0.006)
    for i, x in enumerate((-w / 2, w / 2)):
        rbox(P, "St_Hook%d" % i, m, x, -0.03, z, 0.03, 0.06, 0.03, 0.006)
    for i in range(1, n):
        ball(P, "St_Shade%d" % i, M["shade"], pts[i][0], pts[i][1], pts[i][2] - 0.035, 0.06, 0.06, 0.075)
    light(P, "St_Light", 0, -0.14, z - 0.2, 12.0)


def f_candelabra(P, L, rng):
    """A standing candelabra: a foot, a stem to the chest, three arms and their candles."""
    m = _metal(L)
    cylz(P, "Cb_Foot", m, 0, 0, 0, 0.17, 0.05, r_top=0.1)
    rod(P, "Cb_Stem", m, (0, 0, 0.05), (0, 0, 1.2), 0.02)
    for i, x in enumerate((-0.2, 0.0, 0.2)):
        if x:
            rod(P, "Cb_Arm%d" % i, m, (0, 0, 1.0), (x, 0, 1.12), 0.014)
            rod(P, "Cb_Up%d" % i, m, (x, 0, 1.12), (x, 0, 1.2), 0.014)
        zc = 1.2 + (0.06 if not x else 0.0)
        cylz(P, "Cb_Cup%d" % i, m, x, 0, zc, 0.045, 0.025, bev=0.005)
        hh = (0.2, 0.26, 0.2)[i]
        cylz(P, "Cb_Wax%d" % i, M["cream"], x, 0, zc + 0.025, 0.028, hh, bev=0.006)
        ball(P, "Cb_Flame%d" % i, M["flame"], x, 0, zc + 0.025 + hh + 0.035, 0.035, 0.035, 0.06)
    light(P, "Cb_Light", 0, -0.05, 1.6, 9.0)


def f_neon_ring(P, L, rng):
    """A neon ring on the wall: a round tube on three iron holders."""
    wall_stub(P, 1.1)
    z, r, n = 1.9, 0.3, 20
    c = tuple(LIGHTS["colours"][LIGHTS["kinds"]["neon_ring"]["colour"]]["colour"])
    glass = emissive("M_Neon_Nq", c, 3.0)
    for i in range(n):
        a0, a1 = i * 2 * math.pi / n, (i + 1) * 2 * math.pi / n
        rod(P, "Nq_Tube%d" % i, glass, (math.cos(a0) * r, -0.075, z + math.sin(a0) * r), (math.cos(a1) * r, -0.075, z + math.sin(a1) * r), 0.022)
    for i in range(3):
        a = math.pi / 2 + i * 2 * math.pi / 3
        rbox(P, "Nq_Holder%d" % i, M["iron"], math.cos(a) * r, -0.035, z + math.sin(a) * r, 0.04, 0.07, 0.04, 0.008)
    light(P, "Nq_Light", 0, -0.16, z, 10.0)


def f_ceiling_panel(P, L, rng):
    """A ceiling panel: a square lit glass in a thin frame, flat against the ceiling."""
    h = 2.4
    ceiling_stub(P, h)
    m = _metal(L)
    rbox(P, "Pn_Frame", m, 0, 0, h - 0.025, 0.62, 0.62, 0.05, 0.03 if L["round"] else 0.01)
    rbox(P, "Pn_Shade", M["shade"], 0, 0, h - 0.055, 0.54, 0.54, 0.03, 0.02 if L["round"] else 0.008)
    light(P, "Pn_Light", 0, 0, h - 0.2, 90.0)
    for ob in P.children:
        if ob.type == 'LIGHT':
            setp(ob.data, "shadow_soft_size", 0.25)


# ---- the street's and the shops' lights (the owner, 2026-11-26: "may be we should not leave out street lamps or shop
# fittings. only mall fitting can be left out"). The town's scripts light the street with a lit box on a post
# (old_town_04.py lantern(), hours_50.py street_lamps()); here each is a piece with a lantern's shape, in the four lines.
def f_street_lamp(P, L, rng):
    """A street lamp: a post with a stepped foot and a lantern on its top."""
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    cylz(P, "Sl_Foot", m, 0, 0, 0, 0.15, 0.55, r_top=0.09)
    rod(P, "Sl_Post", m, (0, 0, 0.55), (0, 0, 3.25), 0.055)
    cylz(P, "Sl_Collar", m, 0, 0, 3.2, 0.11, 0.08, r_top=0.14, bev=0.01)
    lantern_body(P, L, "Sl", 0, 0, 3.28, 2.0)


def f_square_lamp(P, L, rng):
    """A lamp for a square: a taller post, a crossbar, a lantern hung from each end."""
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    cylz(P, "Sq_Foot", m, 0, 0, 0, 0.17, 0.6, r_top=0.1)
    rod(P, "Sq_Post", m, (0, 0, 0.6), (0, 0, 3.9), 0.06)
    rod(P, "Sq_Bar", m, (-0.55, 0, 3.75), (0.55, 0, 3.75), 0.03)
    ball(P, "Sq_Knob", m, 0, 0, 3.95, 0.16, 0.16, 0.16)
    for x in (-0.55, 0.55):
        top = lantern_body(P, L, "Sq", x, 0, 3.05, 1.6)
        rod(P, "Sq_Hang", m, (x, 0, top - 0.03), (x, 0, 3.75), 0.012)


def f_street_wall_lantern(P, L, rng):
    """A lantern on a bracket high on a house wall, as in the alleys."""
    wall_stub(P, 0.9)
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    rbox(P, "Sw_Plate", m, 0, -0.02, 2.2, 0.12, 0.04, 0.4, 0.012)
    rod(P, "Sw_Arm", m, (0, -0.04, 2.34), (0, -0.5, 2.34), 0.02)
    rod(P, "Sw_Stay", m, (0, -0.04, 2.05), (0, -0.4, 2.32), 0.014)
    top = lantern_body(P, L, "Sw", 0, -0.46, 1.62, 1.6)
    rod(P, "Sw_Hang", m, (0, -0.46, top - 0.03), (0, -0.46, 2.34), 0.012)


def f_bollard_light(P, L, rng):
    """A low light for a garden path or a common: a short post, a band of lit glass, a cap."""
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    cylz(P, "Bo_Post", m, 0, 0, 0, 0.09, 0.62, r_top=0.08)
    cylz(P, "Bo_Shade", glass_of("Bo"), 0, 0, 0.62, 0.075, 0.14, bev=0.006)
    cylz(P, "Bo_Cap", m, 0, 0, 0.76, 0.12, 0.06, r_top=0.03, bev=0.008)
    light(P, "Bo_Light", 0, 0, 0.69, 15.0)


def f_sign_lamp(P, L, rng):
    """A shop's hanging sign and the lamp that lights it: a board on a bracket, a small shade on a swan neck over it."""
    wall_stub(P, 0.9)
    m = M["iron"] if L["leg"] == "iron" else L["wood"]
    rod(P, "Sn_Bracket", m, (0, -0.04, 2.15), (0, -0.78, 2.15), 0.02)
    rod(P, "Sn_Stay", m, (0, -0.04, 1.9), (0, -0.5, 2.13), 0.012)
    for y in (-0.22, -0.68):
        rod(P, "Sn_Link", m, (0, y, 2.15), (0, y, 2.04), 0.008)
    rbox(P, "Sn_Board", L["accent"], 0, -0.45, 1.82, 0.05, 0.62, 0.44, 0.02)
    rbox(P, "Sn_Band", M["cream"], 0, -0.45, 1.82, 0.056, 0.46, 0.12, 0.01)
    neck = [(0, -0.04, 2.38), (0, -0.25, 2.5), (0, -0.4, 2.48), (0, -0.46, 2.38)]
    for i in range(len(neck) - 1):
        rod(P, "Sn_Neck%d" % i, m, neck[i], neck[i + 1], 0.012)
    cylz(P, "Sn_Hood", m, 0, -0.46, 2.26, 0.09, 0.12, r_top=0.03, bev=0.008)
    ball(P, "Sn_Shade", M["shade"], 0, -0.46, 2.26, 0.12, 0.12, 0.05)
    light(P, "Sn_Light", 0.12, -0.46, 2.2, 20.0)


def f_shop_pendant(P, L, rng):
    """A shop's pendant over a counter: a long rod and a wide cone that throws its light down."""
    h = 2.6
    ceiling_stub(P, h)
    m = _metal(L)
    rod(P, "Sp_Rod", m, (0, 0, h), (0, 0, 1.95), 0.012)
    if L["round"]:
        ball(P, "Sp_Cone", m, 0, 0, 1.86, 0.5, 0.5, 0.3)
    else:
        cylz(P, "Sp_Cone", m, 0, 0, 1.74, 0.26, 0.22, r_top=0.05, bev=0.01)
    ball(P, "Sp_Shade", M["shade"], 0, 0, 1.75, 0.3, 0.3, 0.06)
    light(P, "Sp_Light", 0, 0, 1.68, 70.0)


def f_shelf_light(P, L, rng):
    """A shelf light: a strip of lit glass under a shop's shelf or in a cold cabinet."""
    wall_stub(P, 1.1)
    rbox(P, "Sh_Shelf", L["wood"], 0, -0.15, 1.5, 0.9, 0.28, 0.04, 0.01)
    for i, x in enumerate((-0.3, -0.05, 0.22)):
        cylz(P, "Sh_Jar%d" % i, M["cream"] if i != 1 else L["accent"], x, -0.15, 1.52, 0.06, 0.16 + 0.04 * i, bev=0.01)
    rod(P, "Sh_Shade", M["shade"], (-0.4, -0.24, 1.465), (0.4, -0.24, 1.465), 0.016)
    light(P, "Sh_Light", 0, -0.26, 1.4, 15.0)


# the sheets: what stands on a floor or a table, what hangs on a wall or a ceiling, and the street's and shops'
SHEETS = [
    ("Standing", [("Floor lamp", f_floor_lamp), ("Arc floor lamp", f_arc_lamp), ("Table lamp", f_table_lamp), ("Desk lamp", f_desk_lamp),
                  ("Table lantern", f_table_lantern), ("Candles", f_candles), ("Candelabra", f_candelabra)]),
    ("Hung", [("Wall light", f_wall_light), ("Wall lantern", f_wall_lantern), ("String lights", f_string_lights),
              ("Pendant", f_pendant), ("Chandelier", f_chandelier), ("Ceiling light", f_ceiling_light),
              ("Ceiling panel", f_ceiling_panel), ("Track spots", f_track_spots), ("Neon ring", f_neon_ring),
              ("Neon, blue", _f_neon("Nb", "neon_blue")), ("Neon, green", _f_neon("Ng", "neon_green")),
              ("Neon, red", _f_neon("Nr", "neon_red")), ("Neon, amber", _f_neon("Na", "neon_amber"))]),
    ("Street", [("Street lamp", f_street_lamp), ("Square lamp", f_square_lamp), ("Wall lantern, street", f_street_wall_lantern),
                ("Bollard light", f_bollard_light), ("Sign and its lamp", f_sign_lamp), ("Shop pendant", f_shop_pendant),
                ("Shelf light", f_shelf_light)]),
]
ONLY_SHEETS = tuple(os.environ["WASTED_SHEETS"].split(",")) if os.environ.get("WASTED_SHEETS") else None


def one_sheet(name, cols):
    scene = fresh("Furn_sheet_lights_" + name)
    extra_mats()
    # laid out as kit_sheet() lays a kit, but closer: lamps are small beside furniture
    Ls, rng = lines(), random.Random(54)
    gap_x, gap_y, nx, ny = 1.6, 2.6, len(cols), 4
    box("Sheet_Floor", -4.2, nx * gap_x + 1.6, -3.0, ny * gap_y + 0.6, -0.1, 0, M["plaster"], bevel=0)
    for r, L in enumerate(Ls):
        y = (ny - 1 - r) * gap_y
        for c, (label, fn) in enumerate(cols):
            fn(group("S_%s_%s" % (L["key"], fn.__name__), c * gap_x, y, 0.35), L, rng)
        text("S_Label_%s" % L["key"], L["label"], -2.6, y, 0.02, 0.42, "iron").rotation_euler = (0, 0, 0)
    for c, (label, _) in enumerate(cols):
        text("S_Col_%d" % c, label, c * gap_x, -1.9, 0.02, 0.2, "iron").rotation_euler = (0, 0, 0)
    people([("PlayerA", nx * gap_x + 0.3, -0.6)])
    sun_and_sky(SCENE, (0.55, -0.55, 0.62), 6.0, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.6)
    cx, cy = (nx - 1) * gap_x / 2 - 0.9, (ny - 1) * gap_y / 2
    k = max(nx * gap_x + 8.0, ny * gap_y + 12.0) / 30.0             # the labels along the front and the person in the frame
    for ob in scene.objects:                                         # every light in its kind's named colour and strength
        if ob.type == 'LIGHT' and ob.data.type != 'SUN':
            kind = lamp_kind(ob.name)
            if kind:
                ob.data.color = tuple(kind["colour"])
                ob.data.energy = kind["strength"]
            else:
                hlog("a light of no kind: %s" % ob.name)
    cam = camera(scene, "Cam_sheet", (cx, cy - 22.0 * k, 17.0 * k), (cx, cy + 0.2, 0.7), 27.0)
    scene.camera = cam
    configure(scene, -0.2, (2000, 1100), SAMPLES_H)
    scene.render.resolution_percentage = SCALE_H
    setp(scene.render.image_settings, "file_format", 'JPEG')
    setp(scene.render.image_settings, "quality", 90)
    scene.render.filepath = "%s/Lines_%s.jpg" % (HOURS_OUT, name)
    t = time.time()
    bpy.ops.render.render(write_still=True, scene=scene.name)
    hlog("Lines_%s.jpg rendered in %.1fs" % (name, time.time() - t))


def main_sheet():
    os.makedirs(HOURS_OUT, exist_ok=True)
    builtins.open(HOURS_LOG, "w").close()
    hlog("Blender %s, light kinds 54, the fittings in the four lines" % bpy.app.version_string)
    for name, cols in SHEETS:
        if ONLY_SHEETS and name not in ONLY_SHEETS:
            continue
        try:
            one_sheet(name, cols)
        except Exception:
            hlog("FAILED sheet %s:\n%s" % (name, traceback.format_exc()))
    hlog("DONE")


# ---- how a fitting's light spreads: one fitting lit in a plain corner at night (WASTED_VIG=1) ----------------------
# The owner, 2026-11-26, of room pictures for the newer fittings: "may be not full room but just small portion ... idea
# is to show light spread". A floor, a back wall, a side wall and a ceiling, nothing else but a person for scale; the
# street's fittings stand on paving before a house wall under the night sky. Old Town's line.
CORNERS = [("ArcFloorLamp", f_arc_lamp, "floor", False), ("TableLantern", f_table_lantern, "table", False),
           ("WallLantern", f_wall_lantern, "wall", False), ("Chandelier", f_chandelier, "ceiling", False),
           ("TrackSpots", f_track_spots, "ceiling", False), ("StringLights", f_string_lights, "wall", False),
           ("Candelabra", f_candelabra, "floor", False), ("NeonRing", f_neon_ring, "wall", False),
           ("CeilingPanel", f_ceiling_panel, "ceiling", False), ("ShopPendant", f_shop_pendant, "ceiling", False),
           ("ShelfLight", f_shelf_light, "wall", False), ("SignLamp", f_sign_lamp, "wall", True),
           ("StreetLamp", f_street_lamp, "floor", True), ("SquareLamp", f_square_lamp, "floor", True),
           ("StreetWallLantern", f_street_wall_lantern, "wall", True), ("BollardLight", f_bollard_light, "floor", True)]
CORNER_EYE = 55        # the mean brightness the eye seeks in a corner lit by one fitting
CORNER_OPEN = 2.4      # and how far it opens at most: as far as for the approved lamp-lit street (WorldStreet_NightLamps),
                       # so a weak light leaves a dark picture with its pool in its own colour, and the night sky stays dark


def one_corner(name, fn, where, street):
    global VIG
    scene = fresh("Furn_corner_" + name)
    extra_mats()
    L = [l for l in lines() if l["key"] == "oldtown"][0]
    VIG = {"ceil": None}
    P = group("Corner_" + name, 0, 0.0 if where == "wall" else -1.1, math.pi / 2 if name == "SignLamp" and False else 0)
    if where == "table":
        rbox(P, "Corner_Table", L["wood"], 0, 0, 0.36, 0.6, 0.6, 0.72, 0.03)
        P = group("Corner_On_" + name, 0, 0, 0, parent=P)
        P.location = (0, 0, 0.72)
    fn(P, L, random.Random(54))
    ceil = VIG["ceil"] or 2.8
    VIG = None
    tall = name in ("StreetLamp", "SquareLamp")
    if street:
        ground = M.get("paving") or M.get("stone") or M["concrete"]
        box("Corner_Ground", -12.0, 12.0, -10.0, 0.0, -0.1, 0, ground, bevel=0)
        box("Corner_House", -12.0, 12.0, 0.0, 0.3, 0, 7.0, M["plaster"], bevel=0)
    else:
        box("Corner_Floor", -2.0, 7.5, -6.0, 0.0, -0.1, 0, M["boards"], bevel=0)      # on past the frame's right edge
        box("Corner_Back", -2.25, 7.5, 0.0, 0.25, 0, ceil + 0.2, M["plaster"], bevel=0)
        box("Corner_Side", -2.25, -2.0, -6.0, 0.0, 0, ceil + 0.2, M["plaster"], bevel=0)
        box("Corner_Ceiling", -2.25, 7.5, -6.0, 0.25, ceil, ceil + 0.2, M["plaster"], bevel=0)
    people([("PlayerA", 1.5, -1.5)])
    sun_and_sky(SCENE, (0.0, 0.0, 1.0), 0.0, (1, 1, 1), (0.011, 0.015, 0.034), 0.3)     # night, no moon
    for ob in [o for o in scene.objects if o.type == 'LIGHT' and o.data.type == 'SUN']:
        bpy.data.objects.remove(ob, do_unlink=True)
    for ob in scene.objects:
        if ob.type == 'LIGHT':
            kind = lamp_kind(ob.name)
            if kind:
                ob.data.color = tuple(kind["colour"])
                ob.data.energy = kind["strength"]
    if tall:
        cam = camera(scene, "Cam_corner", (1.0, -7.4, 1.7), (0.2, -0.6, 2.0), 22.0)
    elif street:
        cam = camera(scene, "Cam_corner", (0.8, -5.6, 1.6), (0.2, -0.4, 1.6), 22.0)
    else:
        cam = camera(scene, "Cam_corner", (0.9, -5.2, 1.5), (0.3, -0.4, 1.3), 22.0)
    ex = min(CORNER_OPEN, eye(scene, cam, 1.3, 1.7, CORNER_EYE, name))
    hlog("  %s: shot at %+.2f stops" % (name, ex))
    shoot(scene, cam, 1.3 + ex, "%s/Corner_%s.jpg" % (HOURS_OUT, name))


def main_corners():
    os.makedirs(HOURS_OUT, exist_ok=True)
    builtins.open(HOURS_LOG, "w").close()
    hlog("Blender %s, light kinds 54, each fitting's light in a corner" % bpy.app.version_string)
    for name, fn, where, street in CORNERS:
        if ONLY_KINDS and name not in ONLY_KINDS:
            continue
        try:
            one_corner(name, fn, where, street)
        except Exception:
            hlog("FAILED corner %s:\n%s" % (name, traceback.format_exc()))
    hlog("DONE")


def _added(ob):
    """Whether a light is one of those added here (under a Kind_ group, however deep)."""
    while ob is not None:
        if ob.name.startswith("Kind_"):
            return True
        ob = ob.parent
    return False


try:
    main_corners() if os.environ.get("WASTED_VIG") else (main_sheet() if os.environ.get("WASTED_SHEET") else main_kinds())
except Exception:
    hlog("FAILED:\n" + traceback.format_exc())
