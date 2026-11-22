"""Brix Town - character style test 01: modular characters for players and NPCs.

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/character_test_01.py").read(), "character_test_01.py", "exec"))

Every character is assembled from the same parts (body, head, face features, hair,
clothing) driven by a small spec, the way the in-game character designer will work.
Players and NPCs use the same system. Renders a full-body line-up and a face sheet.
"""
import bpy, bmesh, math, random, os, time, traceback
from mathutils import Vector, Euler

# reuse the material and box helpers from the street style test
_helpers = open("C:/Projects/Wasted/Tools/blender/style_test_02.py", encoding="utf-8").read().split("\n# ---- scene")[0]
exec(compile(_helpers, "style_test_02_helpers", "exec"), globals())

NAME = "character_test_01"
LOG = OUT + "/" + NAME + "_log.txt"
SAMPLES = 128
SUN_DIR = (-0.40, -0.62, 0.68)
SUN_ENERGY = 4.5
SKY_STRENGTH = 0.7
EXPOSURE = 0.0

_M = {}


def solid(c, rough=0.7):
    key = (round(c[0], 3), round(c[1], 3), round(c[2], 3), rough)
    if key not in _M:
        m, nt, bsdf = new_mat("M_Char_%02d" % len(_M))
        sset(bsdf, "Base Color", rgba(c))
        sset(bsdf, "Roughness", rough)
        sset(bsdf, "Specular IOR Level", 0.3)
        _M[key] = m
    return _M[key]


# ---- smooth primitives -------------------------------------------------------
def uvs(u=24, v=16):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=u, v_segments=v, radius=1.0)
    return bm


def finish(name, bm, mat, loc, quat=None):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new(name, me)
    ob.location = loc
    if quat is not None:
        ob.rotation_mode = 'QUATERNION'
        ob.rotation_quaternion = quat
    me.materials.append(mat)
    COL.objects.link(ob)
    return ob


def ellipsoid(name, c, r, mat, rot=None):
    bm = uvs()
    for v in bm.verts:
        v.co = Vector((v.co.x * r[0], v.co.y * r[1], v.co.z * r[2]))
    return finish(name, bm, mat, c, Euler(rot).to_quaternion() if rot else None)


def limb(name, p0, p1, r0, r1, mat, flat=1.0, cap=1.0):
    """Tapered capsule from p0 (radius r0) to p1 (radius r1)."""
    p0 = Vector(p0)
    d = Vector(p1) - p0
    length = d.length
    bm = uvs(20, 14)
    for v in bm.verts:
        if v.co.z > 1e-4:
            v.co = Vector((v.co.x * r1, v.co.y * r1 * flat, v.co.z * r1 * cap + length))
        else:
            v.co = Vector((v.co.x * r0, v.co.y * r0 * flat, v.co.z * r0 * cap))
    return finish(name, bm, mat, p0, d.to_track_quat('Z', 'Y'))


TAPER = 0.30


def jaw(zu):
    return 1.0 - TAPER * ((-zu) ** 1.4) if zu < 0 else 1.0


def hair_cap(name, c, r, mat, style, jitter=0.0):
    """Shell over the head with the face area cut away. Coordinates are tested on the unit sphere."""
    bm = uvs(40, 24)
    kill = []
    for v in bm.verts:
        x, y, z = v.co
        if style in ("short", "buzz", "bun", "ponytail", "cap"):
            dead = z < -0.35 or (y < -0.25 and z < 0.42) or (y < 0.25 and z < 0.05)
        elif style == "fringe":
            dead = z < -0.35 or (y < -0.25 and z < 0.20) or (y < 0.25 and z < 0.0)
        elif style == "side":
            dead = z < -0.35 or (y < -0.25 and z < 0.30 + 0.28 * x) or (y < 0.25 and z < 0.05)
        elif style in ("bob", "long"):
            dead = z < -0.8 or (y < -0.3 and z < 0.30)
        elif style == "afro":
            dead = z < -0.5 or (y < -0.35 and z < 0.22)
        elif style == "dome":
            dead = z < 0.22
        else:
            dead = False
        if dead:
            kill.append(v)
    if kill:
        bmesh.ops.delete(bm, geom=kill, context='VERTS')
    for v in bm.verts:
        x, y, z = v.co
        if style == "long" and z < 0:
            z *= 1.9
        j = Vector((random.uniform(-1, 1), random.uniform(-1, 1), random.uniform(-1, 1))) * jitter
        v.co = Vector((x * r[0], y * r[1], z * r[2])) + j
    return finish(name, bm, mat, c)


# ---- one character -----------------------------------------------------------
def character(sp, ox, oy=0.0, oz=0.0, bust=False):
    S = sp.get("height", 1.72) / 1.72
    W = sp.get("build", 1.0)
    fw = sp.get("face_w", 1.0)
    n = sp["name"]

    def P(x, y, z):
        return Vector((ox + x * S, oy + y * S, oz + z * S))

    skin = solid(sp["skin"], 0.55)
    top = solid(sp["top"], 0.85)
    outer = solid(sp["jacket"], 0.85) if "jacket" in sp else top
    bot = solid(sp.get("bottom", (0.10, 0.10, 0.12)), 0.85)
    shoe = solid(sp.get("shoes", (0.50, 0.47, 0.40)), 0.6)
    hairc = sp.get("hair_color", (0.08, 0.05, 0.04))
    hair = solid(hairc, 0.6)
    dark = solid((0.03, 0.02, 0.02), 0.5)
    long_sleeves = sp.get("sleeves", "short") == "long" or "jacket" in sp
    kind = sp.get("bottom_type", "trousers")
    extras = sp.get("extras", ())
    chest = 0.172 * W
    waist = 0.135 * W

    # body
    if not bust:
        for sg in (-1, 1):
            ellipsoid(n + "_Shoe", P(sg * 0.085, -0.035, 0.042), (0.05 * S, 0.12 * S, 0.042 * S), shoe)
            limb(n + "_Shin", P(sg * 0.085, 0, 0.09), P(sg * 0.09, 0, 0.50), 0.047 * S, 0.058 * S,
                 bot if kind == "trousers" else skin)
            limb(n + "_Thigh", P(sg * 0.09, 0, 0.50), P(sg * 0.085, 0, 0.88), 0.062 * S, 0.082 * S,
                 bot if kind in ("trousers", "shorts") else skin)
        ellipsoid(n + "_Pelvis", P(0, 0, 0.93), (0.158 * W * S, 0.105 * S, 0.115 * S), bot)
        if kind == "skirt":
            ln = sp.get("skirt_len", 0.42)
            limb(n + "_Skirt", P(0, 0, 1.0), P(0, 0, 1.0 - ln), 0.14 * W * S, (0.17 + 0.1 * ln) * W * S, bot,
                 flat=0.78, cap=0.12)
    limb(n + "_Torso", P(0, 0, 1.24 if bust else 1.02), P(0, 0, 1.36), (0.16 * W if bust else waist) * S, chest * S,
         outer, flat=0.62, cap=0.45)
    for sg in (-1, 1):
        sx = sg * (chest + 0.012)
        ellipsoid(n + "_Shoulder", P(sx, 0, 1.385), (0.06 * S, 0.06 * S, 0.06 * S), outer)
        if not bust:
            elbow = P(sg * (chest + 0.062), 0, 1.12)
            limb(n + "_UpperArm", P(sx + sg * 0.008, 0, 1.385), elbow, 0.05 * S, 0.043 * S, outer)
            limb(n + "_Forearm", elbow, P(sg * (chest + 0.09), -0.03, 0.87), 0.042 * S, 0.033 * S,
                 outer if long_sleeves else skin)
            ellipsoid(n + "_Hand", P(sg * (chest + 0.097), -0.04, 0.79), (0.028 * S, 0.045 * S, 0.055 * S), skin)
    limb(n + "_Neck", P(0, 0.005, 1.40), P(0, 0, 1.50), 0.047 * S, 0.045 * S, skin)
    front = -chest * 0.62
    if "jacket" in sp:
        ellipsoid(n + "_Shirt", P(0, front + 0.004, 1.25), (0.045 * S, 0.012 * S, 0.12 * S), top)
    if "apron" in extras and not bust:
        limb(n + "_Apron", P(0, -0.085, 0.66), P(0, -0.09, 1.26), 0.15 * W * S, 0.125 * W * S,
             solid(sp.get("apron_color", (0.44, 0.22, 0.16)), 0.85), flat=0.22, cap=0.2)
    if "bag" in extras and not bust:
        strap = solid((0.16, 0.11, 0.07), 0.7)
        limb(n + "_Strap", P(-0.15, front - 0.004, 1.40), P(0.18, front + 0.01, 1.0), 0.012 * S, 0.012 * S, strap)
        ellipsoid(n + "_Bag", P(0.2, -0.10, 0.93), (0.05 * S, 0.10 * S, 0.09 * S), strap)

    # head
    hz = 1.59
    rx, ry, rz = 0.098 * fw, 0.108, 0.13

    def face_y(x, z):
        zu = z / rz
        s = jaw(zu)
        inside = 1.0 - (x / (rx * s)) ** 2 - zu * zu
        return -ry * s * math.sqrt(max(inside, 0.0))

    def F(x, z, out=0.0):
        return P(x, face_y(x, z) - out, hz + z)

    bm = uvs(32, 20)
    for v in bm.verts:
        s = jaw(v.co.z)
        v.co = Vector((v.co.x * rx * s * S, v.co.y * ry * s * S, v.co.z * rz * S))
    finish(n + "_Head", bm, skin, P(0, 0, hz))
    for sg in (-1, 1):
        ellipsoid(n + "_Ear", P(sg * rx * 0.97, 0.012, hz - 0.008), (0.012 * S, 0.022 * S, 0.032 * S), skin)

    # eyes
    es = sp.get("eye_size", 1.0)
    ex = 0.040 * fw * sp.get("eye_gap", 1.0)
    ez = 0.012
    ew = 0.022 * es
    eh = 0.015 * es * sp.get("eye_open", 1.0)
    ir = 0.0098 * es
    irz = min(ir, eh * 0.92)
    tilt = sp.get("eye_tilt", 0.0)
    yaw = math.asin(min(ex / rx, 1.0)) * 0.55
    white = solid((0.82, 0.80, 0.76), 0.3)
    iris = solid(sp.get("eyes", (0.16, 0.09, 0.05)), 0.2)
    for sg in (-1, 1):
        rot = (0.0, -sg * tilt, sg * yaw)
        ellipsoid(n + "_EyeWhite", F(sg * ex, ez, -0.004), (ew * S, 0.010 * S, eh * S), white, rot)
        ellipsoid(n + "_Iris", F(sg * ex, ez, 0.0035), (ir * S, 0.004 * S, irz * S), iris, rot)
        ellipsoid(n + "_Pupil", F(sg * ex, ez, 0.0065), (0.0046 * es * S, 0.003 * S, min(0.0046 * es, irz) * S), dark, rot)
        ellipsoid(n + "_Glint", F(sg * ex + 0.003, ez + 0.004, 0.0088), (0.0018 * S,) * 3, white)
        ellipsoid(n + "_Lash", F(sg * ex, ez + eh * 0.8, 0.0015), (ew * 1.08 * S, 0.008 * S, eh * 0.32 * S), dark, rot)
        bt = sp.get("brow", 1.0)
        browc = solid((hairc[0] * 0.6, hairc[1] * 0.6, hairc[2] * 0.6), 0.6)
        limb(n + "_Brow", F(sg * (ex - 0.020), ez + 0.034 + sp.get("brow_in", 0.0), 0.002),
             F(sg * (ex + 0.024), ez + 0.035 + sp.get("brow_out", 0.0), 0.002), 0.0042 * bt * S, 0.0028 * bt * S, browc)

    # nose and mouth
    ns = sp.get("nose", 1.0)
    ellipsoid(n + "_Nose", F(0, -0.022, 0.004), (0.013 * ns * S, 0.016 * ns * S, 0.022 * S), skin)
    lipc = sp.get("lips", (sp["skin"][0] * 0.78, sp["skin"][1] * 0.52, sp["skin"][2] * 0.50))
    lip = solid(lipc, 0.5)
    mw = 0.024 * sp.get("mouth", 1.0)
    lr = 0.0042 * sp.get("lip", 1.0) * S
    for sg in (-1, 1):
        limb(n + "_Mouth", F(0, -0.068, 0.001), F(sg * mw, -0.063, 0.0005), lr, lr * 0.7, lip)
    if "moustache" in extras:
        ellipsoid(n + "_Moustache", F(0, -0.050, 0.004), (0.028 * S, 0.007 * S, 0.0075 * S), hair)

    # hair
    style = sp.get("hair", "short")
    hc = P(0, 0.010, hz + 0.012)
    if style == "afro":
        hair_cap(n + "_Hair", P(0, 0.012, hz + 0.035), ((rx + 0.05) * S, (ry + 0.05) * S, (rz + 0.055) * S), hair,
                 "afro", jitter=0.006 * S)
    elif style == "buzz":
        hair_cap(n + "_Hair", P(0, 0.004, hz + 0.004), ((rx + 0.004) * S, (ry + 0.005) * S, (rz + 0.004) * S), hair, "buzz")
    elif style != "bald":
        hair_cap(n + "_Hair", hc, ((rx + 0.014) * S, (ry + 0.016) * S, (rz + 0.014) * S), hair, style)
    if style == "bun":
        ellipsoid(n + "_Bun", P(0, 0.075, hz + 0.125), (0.05 * S, 0.05 * S, 0.045 * S), hair)
    if style == "ponytail":
        limb(n + "_Tail", P(0, 0.11, hz + 0.06), P(0, 0.17, hz - 0.17), 0.036 * S, 0.012 * S, hair)
    if "cap" in extras:
        capm = solid(sp.get("cap_color", (0.44, 0.22, 0.16)), 0.8)
        hair_cap(n + "_Cap", P(0, 0.006, hz + 0.014), ((rx + 0.022) * S, (ry + 0.024) * S, (rz + 0.02) * S), capm, "dome")
        ellipsoid(n + "_Brim", P(0, -0.125, hz + 0.052), (0.078 * S, 0.062 * S, 0.008 * S), capm)


# ---- who is in the test --------------------------------------------------------
TEAL = (0.16, 0.36, 0.38)
CREAM = (0.66, 0.61, 0.50)
LINEUP = [
    dict(name="PlayerA", height=1.68, build=0.92, skin=(0.62, 0.42, 0.30), hair="long", hair_color=(0.28, 0.10, 0.05),
         eyes=(0.14, 0.36, 0.20), eye_size=1.1, top=TEAL, bottom=(0.10, 0.14, 0.22), shoes=CREAM),
    dict(name="PlayerB", height=1.80, build=1.08, skin=(0.30, 0.19, 0.13), hair="afro", hair_color=(0.03, 0.025, 0.02),
         top=(0.55, 0.40, 0.16), sleeves="long", bottom_type="shorts", bottom=(0.25, 0.28, 0.22),
         shoes=(0.70, 0.68, 0.62), extras=("bag",), brow=1.3),
    dict(name="Shopkeeper", height=1.72, build=1.14, skin=(0.78, 0.56, 0.44), hair="side", hair_color=(0.10, 0.06, 0.04),
         top=CREAM, sleeves="long", bottom=(0.22, 0.15, 0.10), shoes=(0.12, 0.09, 0.07),
         extras=("apron", "moustache"), face_w=1.05, nose=1.15),
    dict(name="OfficeWorker", height=1.74, build=0.96, skin=(0.47, 0.31, 0.22), hair="bun", hair_color=(0.03, 0.025, 0.02),
         top=CREAM, jacket=(0.14, 0.16, 0.22), bottom_type="skirt", bottom=(0.14, 0.16, 0.22),
         shoes=(0.06, 0.06, 0.07), lip=1.3, lips=(0.45, 0.14, 0.14)),
    dict(name="Courier", height=1.78, build=1.0, skin=(0.84, 0.64, 0.52), hair="short", hair_color=(0.55, 0.40, 0.18),
         eyes=(0.18, 0.34, 0.55), top=(0.28, 0.36, 0.48), bottom_type="shorts", bottom=(0.10, 0.10, 0.12),
         shoes=(0.44, 0.22, 0.16), extras=("cap", "bag")),
    dict(name="ElderResident", height=1.58, build=1.06, skin=(0.70, 0.50, 0.38), hair="bun", hair_color=(0.55, 0.55, 0.56),
         top=CREAM, jacket=(0.32, 0.26, 0.36), bottom_type="skirt", skirt_len=0.62, bottom=(0.20, 0.22, 0.20),
         shoes=(0.16, 0.11, 0.08), eye_size=0.92, eye_open=0.85, brow=0.8),
]
BASE = dict(skin=(0.62, 0.42, 0.30), hair="short", hair_color=(0.10, 0.06, 0.04), top=TEAL)
FACES = [
    dict(),
    dict(hair="fringe", hair_color=(0.55, 0.40, 0.18), eyes=(0.18, 0.34, 0.55)),
    dict(skin=(0.30, 0.19, 0.13), hair="buzz", hair_color=(0.03, 0.025, 0.02), brow=1.5, nose=1.15),
    dict(hair="long", hair_color=(0.28, 0.10, 0.05), eyes=(0.14, 0.36, 0.20), eye_size=1.15, lip=1.4, lips=(0.50, 0.16, 0.16)),
    dict(skin=(0.84, 0.64, 0.52), hair="bob", hair_color=(0.03, 0.025, 0.02), eye_tilt=0.25, eye_open=0.8, top=(0.44, 0.22, 0.16)),
    dict(skin=(0.47, 0.31, 0.22), hair="afro", hair_color=(0.08, 0.05, 0.04), eyes=(0.36, 0.26, 0.10), nose=1.2, top=(0.55, 0.40, 0.16)),
    dict(skin=(0.78, 0.56, 0.44), hair="bun", hair_color=(0.55, 0.55, 0.56), eye_size=0.9, face_w=1.07, brow=0.7, top=(0.32, 0.26, 0.36)),
    dict(hair="ponytail", hair_color=(0.08, 0.30, 0.32), eyes=(0.32, 0.30, 0.42), face_w=0.94, brow_out=0.006, top=CREAM),
]


def build():
    global COL
    random.seed(11)
    scene = bpy.data.scenes.new("CharacterTest01")
    try:
        bpy.context.window.scene = scene
    except Exception as e:
        log("could not switch window scene: %s" % e)
    COL = scene.collection

    stone = painted("M_Stone", (0.27, 0.26, 0.30), (0.52, 0.48, 0.43), brick_scale=0.5, bump=0.35, noise_scale=0.9)
    plaster = painted("M_Plaster", (0.46, 0.43, 0.40), (0.68, 0.63, 0.55), noise_scale=0.7, bump=0.25)
    paving = painted("M_Paving", (0.22, 0.21, 0.25), (0.46, 0.42, 0.37), brick_scale=0.6, floor=True, bump=0.5, noise_scale=0.5)
    sheet = painted("M_Sheet", (0.40, 0.39, 0.40), (0.50, 0.48, 0.46), noise_scale=0.5, bump=0.1, grime=1.0)

    box("Ground", -10, 48, -12, 4, -0.3, 0, paving, bevel=0)
    box("Wall_Base", -10, 10, 1.2, 1.7, 0, 1.1, stone)
    box("Wall_Upper", -10, 10, 1.26, 1.7, 1.1, 5, plaster)
    box("Wall_Ledge", -10, 10, 1.12, 1.7, 1.1, 1.2, stone)
    box("Sheet_Wall", 36, 44, 0.5, 0.7, -1, 6, sheet, bevel=0)

    for i, sp in enumerate(LINEUP):
        character(sp, -2.4 + i * 0.96)
    for i, var in enumerate(FACES):
        sp = dict(BASE)
        sp.update(var)
        sp["name"] = "Face%d" % (i + 1)
        character(sp, 40 - 0.96 + (i % 4) * 0.64, 0.0, 0.8 if i < 4 else 0.0, bust=True)

    sun = bpy.data.lights.new("Sun", 'SUN')
    sun.energy = SUN_ENERGY
    sun.color = SUN_COLOR
    setp(sun, "angle", math.radians(4.0))
    so = bpy.data.objects.new("Sun", sun)
    so.rotation_euler = Vector(SUN_DIR).normalized().to_track_quat('Z', 'Y').to_euler()
    so.location = (-6, -8, 12)
    COL.objects.link(so)

    world = bpy.data.worlds.new("CharWorld")
    scene.world = world
    try:
        world.use_nodes = True
    except Exception:
        pass
    wt = world.node_tree
    wt.nodes.clear()
    bg = wt.nodes.new("ShaderNodeBackground")
    wo = wt.nodes.new("ShaderNodeOutputWorld")
    bg.inputs[0].default_value = rgba(SKY_COLOR)
    bg.inputs[1].default_value = SKY_STRENGTH
    wt.links.new(bg.outputs[0], wo.inputs["Surface"])

    cams = {}
    c1 = bpy.data.cameras.new("Cam_Lineup")
    c1.lens = 45
    o1 = bpy.data.objects.new("Cam_Lineup", c1)
    o1.location = (0, -7.6, 1.25)
    o1.rotation_euler = (Vector((0, 0, 0.93)) - Vector(o1.location)).to_track_quat('-Z', 'Y').to_euler()
    COL.objects.link(o1)
    cams["lineup"] = (o1, (1600, 800))
    c2 = bpy.data.cameras.new("Cam_Faces")
    c2.type = 'ORTHO'
    c2.ortho_scale = 2.75
    o2 = bpy.data.objects.new("Cam_Faces", c2)
    o2.location = (40, -6, 1.98)
    o2.rotation_euler = (math.pi / 2, 0, 0)
    COL.objects.link(o2)
    cams["faces"] = (o2, (1600, 1000))
    return scene, cams


def configure(scene):
    r = scene.render
    setp(r, "engine", 'CYCLES')
    c = getattr(scene, "cycles", None)
    if c is not None:
        setp(c, "device", 'CPU')
        setp(c, "samples", SAMPLES)
        setp(c, "use_adaptive_sampling", True)
        setp(c, "adaptive_threshold", 0.03)
        setp(c, "use_denoising", True)
        setp(c, "max_bounces", 6)
        setp(c, "diffuse_bounces", 4)
    r.resolution_percentage = 100
    setp(r.image_settings, "media_type", 'IMAGE')
    setp(r.image_settings, "file_format", 'PNG')
    setp(r.image_settings, "color_mode", 'RGB')
    vs = scene.view_settings
    setp(vs, "view_transform", 'AgX')
    setp(vs, "look", 'AgX - Medium High Contrast')
    setp(vs, "exposure", EXPOSURE)


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(SRC, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    try:
        scene, cams = build()
        configure(scene)
        log("scene built: %d objects" % len(scene.objects))
        try:
            bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
            log("saved .blend")
        except Exception:
            log("save failed:\n" + traceback.format_exc())
        for key, (cam, res) in cams.items():
            t = time.time()
            scene.camera = cam
            scene.render.resolution_x, scene.render.resolution_y = res
            scene.render.filepath = OUT + "/" + NAME + "_" + key + ".png"
            bpy.ops.render.render(write_still=True, scene=scene.name)
            log("%s rendered in %.1fs" % (key, time.time() - t))
        log("DONE")
    except Exception:
        log("FAILED:\n" + traceback.format_exc())


main()
