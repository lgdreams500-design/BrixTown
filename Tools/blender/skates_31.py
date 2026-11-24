"""Brix Town - skates 31: three pairs of skates for the skate store, a look test (game progression; look test).

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/skates_31.py").read(), "skates_31.py", "exec"))
or headless:
    blender-launcher.exe -b --factory-startup --python C:/Projects/Wasted/Tools/blender/skates_31.py
Quick checks: WASTED_ONLY=sheet,worn WASTED_SAMPLES=24 WASTED_SCALE=50
From another checkout: WASTED_ROOT=C:/Projects/Wasted-skates (where renders and the .blend go; the helpers are read
from there too when it has them).

Builds on car_kit_09.py (its rounded shapes, the kit materials and the kit daylight, ADR-0007) and character_face_05.py
(the option B people, so the skates are shown on a reference character's feet at true size). Skates are new: nothing in
ART_DIRECTION covers them yet. Three pairs, one per step of the store's ladder:

    Manual      50 km/h   a classic quad roller skate: cream boot, brown trim, ochre wheels, terracotta toe stop
    Electric   100 km/h   an inline skate: dusty blue shell, two teal hub motors, a battery pack on the heel, status light
    Neon Boost 250 km/h   a long inline racing frame: plum boot, glowing wheels and trim, a booster nozzle with a fin

Every skate is one clean low-poly mesh (SK_<Model>_L / _R, a few material slots, no modifiers) parented to an empty at
the ankle joint, SK_<Model>_L_foot_l / SK_<Model>_R_foot_r (CHARACTER_MOTION 2: the skate rides on foot_l / foot_r).
The rounded parts are superellipsoids on low UV spheres, so they stay soft (W3) at a few hundred triangles each.
Sized for a 1.72 m person in option B (the shoe is 0.30 m long); the empty scales with the character.

Renders go to Art/StyleTests/skates_31/:
    sheet.png        the three pairs side by side on turntables, labelled
    turnaround.png   each left skate from four sides
    manual.png, electric.png, neon.png   a close-up of each pair
    worn.png         the Courier in shoes and in each pair, standing, so scale reads
    worn_feet.png    the same four, low and close
    neon_dusk.png    the Neon Boost close-up again in the dusk recipe, where the glow matters most
Not approved: for the owner to choose from.
"""
import bpy, bmesh, math, random, os, time, traceback
from mathutils import Vector, Euler, Matrix

ROOT = os.environ.get("WASTED_ROOT", "C:/Projects/Wasted").replace("\\", "/").rstrip("/")
_H = ROOT + "/Tools/blender/" if os.path.exists(ROOT + "/Tools/blender/car_kit_09.py") else "C:/Projects/Wasted/Tools/blender/"
_f = open(_H + "character_face_05.py", encoding="utf-8").read().split("\n# ---- scene with the line-up")[0]
exec(compile(_f, "character_face_05_helpers", "exec"), globals())          # person(), pose(), with_name()
_k = open(_H + "car_kit_09.py", encoding="utf-8").read()
_k = _k[:_k.rstrip().rfind("\nmain()")]
exec(compile(_k, "car_kit_09_helpers", "exec"), globals())                 # kit_mats(), sun_and_sky(), camera(), configure()

NAME = "skates_31"
OUT = ROOT + "/Art/StyleTests"
SRC = ROOT + "/Art/Source/StyleTests"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"
ONLY = tuple(os.environ["WASTED_ONLY"].split(",")) if os.environ.get("WASTED_ONLY") else None
SAMPLES = int(os.environ.get("WASTED_SAMPLES", 96))
SCALE = int(os.environ.get("WASTED_SCALE", 100))

ANKLE = 0.07 * CHAR_STYLES["B"]["leg"]     # foot_l / foot_r above the shoe sole for a 1.72 m person (motion_lib.skeleton)
LX = 0.085                                  # feet this far either side of the centre line
WHEEL = (0.0, math.pi / 2, 0.0)             # turns a part built along z so it runs along x (an axle)
DETAIL = 1.0                                # segment scale of every round part: 1.0 near, LOD_DETAIL for the far model
LOD_DETAIL = 0.55

# colours: linear RGB, palette names from ART_DIRECTION 2.3 unless marked new
PAL = dict(
    cream=((0.66, 0.61, 0.50), "Cream"), brown=((0.24, 0.16, 0.11), "Brown"), ochre=((0.52, 0.40, 0.20), "Ochre"),
    terra=((0.44, 0.24, 0.18), "Terracotta"), teal=((0.20, 0.36, 0.37), "Teal"), dusty=((0.30, 0.38, 0.52), "Dusty blue"),
    plum=((0.30, 0.24, 0.34), "Plum"), indigo=((0.10, 0.14, 0.24), "Indigo"), charcoal=((0.09, 0.09, 0.11), "Charcoal"),
    iron=((0.10, 0.10, 0.11), "Iron (kit)"), alu=((0.58, 0.56, 0.52), "Hub grey (car kit)"),
    lining=((0.05, 0.04, 0.04), "lining (near black)"),
)
GLOW = dict(
    status=((0.30, 1.00, 0.45), 3.0, "status green (new)"),
    neon=((0.02, 0.55, 1.00), 2.0, "neon cyan (new)"),
    flame=((1.0, 0.55, 0.22), 6.0, "Lantern"),
)

M = {}

DESIGN = """
== What was built ==
Manual (50 km/h): a classic quad roller skate, the plainest and the most old-town. Cream boot with a brown padded
  collar, a tongue with three brown laces, a brown sole and heel, a dark iron plate and trucks, four fat ochre wheels
  with pale hubs, a terracotta toe stop. Reads as sturdy and a little retro; quad wheels make it the widest stance.
Electric (100 km/h): an inline skate. Teal shell boot and cuff, charcoal liner rim, a strap with a cream buckle on the
  outer side, a pale grey frame on two mounts, four charcoal wheels. The two rear wheels are hub motors: ochre drums
  that stand out through both sides of the frame, with cream caps. A charcoal battery pack with an ochre band sits on
  the heel, a cable runs down to the motors, and a small green status light (emissive) sits on its outer corner.
Neon Boost (250 km/h): a long inline racing frame (0.46 m, the longest), four large wheels whose tyres glow neon cyan,
  a neon welt round the sole, neon stripes on the frame rails and boot sides and a neon rim on the low cuff. Indigo
  boot, charcoal cuff and frame. A booster nozzle on the heel: a dark iron can with a plum lip and a glowing Lantern
  core, aimed a little down. Glow is plain emission (no bloom or post-process, W8); neon_dusk.png shows it at dusk.

Every skate is one mesh with a few material slots and no modifiers, origin at the ankle, parented to an empty named
SK_<Model>_<L|R>_foot_<l|r> with a custom property bone = foot_l / foot_r (CHARACTER_MOTION 2). The far model
SK_<Model>_<L|R>_LOD1 (same parts, about half the segments) is in the .blend, hidden. The boot replaces the shoe:
in worn.png the character's shoes are hidden and the body is lifted by the skate's height (Manual 9.4 cm, Electric
10.6 cm, Neon Boost 11.8 cm), so the faster skates make the wearer a little taller. Sized for a 1.72 m person in
option B; the Courier (1.78 m) wears them scaled with the body (the empty is scaled, as a socket on foot_l would be).

== Choices for the owner ==
1. Manual as a quad (four wheels in two pairs) rather than an inline. It gives the three tiers three different
   silhouettes (quad, inline with motors, long racing frame with booster), but quads are not what a street skater
   would pick for speed. Say if Manual should be a plain inline instead.
2. New colours outside the palette: the neon cyan (0.02, 0.55, 1.0) and the status green (0.30, 1.0, 0.45) are new;
   the booster core uses the Lantern colour. In daylight the neon reads pale ice-blue (AgX); at dusk it is strong.
   Neon could instead be a player-chosen colour, or warm (Lantern orange or a rose) to sit closer to the old town.
3. Neon Boost boot colours: indigo and charcoal keep the glow the star but make the boot itself the plainest of the
   three by day. Plum was tried for the cuff and read lilac-grey; a terracotta or ochre accent is another option.
4. Lift: wearing skates raises the character 9 to 12 cm. Truthful, but the slice's collision, doorways and camera
   would see it. The alternative is to sink the boot round the foot and keep the eye height.
5. Triangles: about 1,450 to 1,800 a skate near, 650 to 760 far; a pair costs about one Soft box car. Fine for the
   player; for many skating NPCs in the slice the far model (or fewer wheel segments) would be the one to use.
6. Not decided here: how the motors and booster animate (wheel spin, a flame flicker, the status light colour for
   battery level), sounds, and whether the store sells colourways of each tier (the materials are already slots).
"""


def log(msg):
    line = time.strftime("%H:%M:%S") + " " + str(msg)
    print(line)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")


def note(text):
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(text.rstrip() + "\n")


# ---- low-poly soft shapes ------------------------------------------------------------------------------------------
def sq(size, u=16, v=8, exy=4.0, ez=4.0, deform=None):
    """A superellipsoid on a low UV sphere: exy 2 is round in plan, 4 a soft square; ez the same up and down.
    size is the full extent (x, y, z). deform(p) bends the shape in its own space before it is placed."""
    u, v = max(6, int(round(u * DETAIL))), max(3, int(round(v * DETAIL)))
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=u, v_segments=v, radius=1.0)
    for vt in bm.verts:
        n = vt.co.normalized()
        rxy = abs(n.x) ** exy + abs(n.y) ** exy
        t = (rxy ** (ez / exy) + abs(n.z) ** ez) ** (-1.0 / ez)
        p = Vector((n.x * t * size[0] / 2, n.y * t * size[1] / 2, n.z * t * size[2] / 2))
        vt.co = deform(p, size) if deform else p
    return bm


def rbox(size, u=16, v=8, e=4.0, deform=None):
    return sq(size, u, v, e, e, deform)


def rcyl(d, h, u=14, v=6, ez=6.0, dy=None):
    """A rounded cylinder along z: diameter d (or d by dy), height h."""
    return sq((d, dy or d, h), u, v, 2.0, ez)


def ring(rx, ry, r, u=14, v=4):
    """A soft collar: a torus with an oval centre line (rx, ry) and a tube of radius r, lying flat."""
    u, v = max(6, int(round(u * DETAIL))), max(3, int(round(v * DETAIL)))
    bm = bmesh.new()
    vs = []
    for i in range(u):
        a = 2 * math.pi * i / u
        c, s = math.cos(a), math.sin(a)
        row = []
        for j in range(v):
            b = 2 * math.pi * j / v + math.pi / v
            k = r * math.cos(b)
            row.append(bm.verts.new(((rx + k) * c, (ry + k) * s, r * math.sin(b))))
        vs.append(row)
    for i in range(u):
        for j in range(v):
            bm.faces.new((vs[i][j], vs[(i + 1) % u][j], vs[(i + 1) % u][(j + 1) % v], vs[i][(j + 1) % v]))
    return bm


def opening(sk, cy, z, rx, ry, lining, rim, tilt=0.0):
    """The open top of a cuff: a padded collar round a dark lining, so a boot on a shelf reads as hollow."""
    sk.add(ring(rx, ry, 0.011), rim, (0, cy, z), (tilt, 0, 0))
    sk.add(sq((2 * rx, 2 * ry, 0.010), 14, 3, 2.0, 3.0), lining, (0, cy, z - 0.002), (tilt, 0, 0))


def toe(drop=0.32, narrow=0.14):
    """Lowers and narrows the front (-y) of a boot so it follows the shoe."""
    def f(p, size):
        k = max(0.0, -p.y / (size[1] / 2))
        k = k * k
        z = p.z * (1.0 - drop * k) - (size[2] * 0.5 * drop * 0.55 * k if p.z > 0 else 0.0)
        return Vector((p.x * (1.0 - narrow * k), p.y, z if p.z > 0 else p.z))
    return f


class Skate:
    """One skate being built: a single bmesh with a material slot per colour."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.slots = []

    def add(self, part, mat, loc=(0, 0, 0), rot=(0, 0, 0), quat=None):
        if mat not in self.slots:
            self.slots.append(mat)
        mi = self.slots.index(mat)
        R = quat.to_matrix().to_4x4() if quat is not None else Euler(rot).to_matrix().to_4x4()
        mx = Matrix.Translation(Vector(loc)) @ R
        vm = {v: self.bm.verts.new(mx @ v.co) for v in part.verts}
        for f in part.faces:
            nf = self.bm.faces.new([vm[v] for v in f.verts])
            nf.material_index = mi
            nf.smooth = True
        part.free()

    def rod(self, a, b, r, mat, u=8):
        a, b = Vector(a), Vector(b)
        d = b - a
        self.add(rcyl(2 * r, d.length + 2 * r, u, 4, 3.0), mat, tuple((a + b) / 2),
                 quat=Vector((0, 0, 1)).rotation_difference(d.normalized()))

    def finish(self, za, bone):
        """Moves the mesh so its origin is the ankle, makes the object and the ankle empty it hangs from."""
        bmesh.ops.translate(self.bm, vec=(0.0, 0.0, -za), verts=self.bm.verts)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        me = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(me)
        self.bm.free()
        for key in self.slots:
            me.materials.append(M[key])
        ob = bpy.data.objects.new(self.name, me)
        COL.objects.link(ob)
        emp = bpy.data.objects.new(self.name + "_" + bone, None)
        setp(emp, "empty_display_type", 'ARROWS')
        setp(emp, "empty_display_size", 0.06)
        emp["bone"] = bone
        COL.objects.link(emp)
        ob.parent = emp
        ob["attach_bone"] = bone
        return emp, ob


def wheels(sk, xs, ys, r, w, tyre, hub, hub_d=None, u=12):
    for y in ys:
        for x in xs:
            sk.add(rcyl(2 * r, w, u, 4, 5.0), tyre, (x, y, r), WHEEL)
            sk.add(rcyl(hub_d or r * 0.9, w + 0.005, 8, 3, 4.0), hub, (x, y, r), WHEEL)


# ---- 1. Manual: a classic quad skate ---------------------------------------------------------------------------------
def manual(sk, sg):
    H = 0.094                                         # shoe sole above the ground
    r, w = 0.031, 0.030
    ys = (-0.130, 0.058)
    wheels(sk, (-0.056, 0.056), ys, r, w, "ochre", "alu", 0.03, 10)
    for y in ys:
        sk.add(rbox((0.086, 0.026, 0.024), 8, 4), "iron", (0, y, r))                 # axle housing
        sk.add(rbox((0.030, 0.036, 0.040), 8, 4), "iron", (0, y + 0.004, 0.056))     # truck
    sk.add(rbox((0.090, 0.270, 0.012), 8, 4), "iron", (0, -0.036, 0.074))            # plate
    sk.add(rbox((0.136, 0.304, 0.022), 12, 6, 3.0), "brown", (0, -0.040, H - 0.006))  # sole
    sk.add(rbox((0.124, 0.090, 0.030), 8, 4), "brown", (0, 0.060, H - 0.012))         # heel block
    sk.add(rbox((0.142, 0.310, 0.128), 14, 8, 3.4, toe(0.34, 0.16)), "cream", (0, -0.042, H + 0.052))
    sk.add(rcyl(0.150, 0.150, 14, 6, 5.0, dy=0.156), "cream", (0, 0.014, H + 0.128))   # cuff
    opening(sk, 0.014, H + 0.204, 0.068, 0.071, "lining", "brown")
    tongue = Matrix.Translation((0, -0.088, H + 0.118)) @ Euler((0.62, 0, 0)).to_matrix().to_4x4()
    sk.add(rbox((0.070, 0.150, 0.030), 8, 4), "cream", tongue.translation, (0.62, 0, 0))       # tongue up the front
    for i in range(3):                                                                           # laces across it
        sk.add(rbox((0.086, 0.012, 0.012), 6, 3), "brown", tuple(tongue @ Vector((0, -0.045 + i * 0.036, 0.014))), (0.62, 0, 0))
    sk.add(rcyl(0.040, 0.040, 10, 4, 6.0), "terra", (0, -0.200, 0.046), (-0.45, 0, 0))   # toe stop
    return H


# ---- 2. Electric: an inline skate with hub motors ---------------------------------------------------------------------
def electric(sk, sg):
    H = 0.106
    r, w = 0.037, 0.024
    ys = (-0.166, -0.087, -0.008, 0.071)
    for i, y in enumerate(ys):
        sk.add(rcyl(2 * r, w, 12, 4, 5.0), "charcoal", (0, y, r), WHEEL)
        if i < 2:
            sk.add(rcyl(0.034, w + 0.005, 8, 3, 4.0), "alu", (0, y, r), WHEEL)
        else:                                                                    # hub motor: a drum through the frame
            sk.add(rcyl(0.054, 0.070, 12, 4, 5.0), "ochre", (0, y, r), WHEEL)
            sk.add(rcyl(0.026, 0.076, 8, 3, 4.0), "cream", (0, y, r), WHEEL)
    for s in (-1, 1):
        sk.add(rbox((0.010, 0.330, 0.058), 12, 4, 6.0), "alu", (s * 0.021, -0.046, 0.058))     # frame rails
    for y in (-0.128, 0.034):
        sk.add(rbox((0.058, 0.050, 0.030), 8, 4), "alu", (0, y, H - 0.020))                    # mounts
    sk.add(rbox((0.130, 0.300, 0.018), 12, 6, 3.0), "charcoal", (0, -0.042, H - 0.004))        # sole
    sk.add(rbox((0.138, 0.306, 0.124), 14, 8, 3.0, toe(0.36, 0.16)), "teal", (0, -0.043, H + 0.050))
    sk.add(rcyl(0.146, 0.172, 14, 6, 5.0, dy=0.158), "teal", (0, 0.016, H + 0.136))          # shell cuff
    opening(sk, 0.016, H + 0.220, 0.066, 0.072, "lining", "charcoal")
    sk.add(rbox((0.150, 0.034, 0.020), 8, 4), "charcoal", (0, -0.040, H + 0.112), (0.50, 0, 0))  # strap
    sk.add(rbox((0.018, 0.034, 0.026), 6, 3), "cream", (sg * 0.072, -0.040, H + 0.104))          # buckle
    sk.add(rbox((0.084, 0.046, 0.104), 8, 6, 3.5), "charcoal", (0, 0.118, H + 0.100))          # battery pack
    sk.add(rbox((0.088, 0.050, 0.020), 8, 3, 3.5), "ochre", (0, 0.118, H + 0.068))             # its band
    sk.add(sq((0.030, 0.020, 0.030), 8, 4, 2.0, 2.0), "status", (sg * 0.024, 0.141, H + 0.128))   # status light
    sk.rod((sg * 0.030, 0.118, H + 0.050), (sg * 0.028, 0.076, 0.056), 0.0055, "charcoal")         # motor cable
    return H


# ---- 3. Neon Boost: a long racing frame, glowing wheels and a booster -------------------------------------------------
def neon(sk, sg):
    H = 0.118
    r, w = 0.043, 0.022
    ys = (-0.196, -0.102, -0.008, 0.086)
    wheels(sk, (0.0,), ys, r, w, "neon", "indigo", 0.052, 14)
    for s in (-1, 1):
        sk.add(rbox((0.010, 0.390, 0.052), 12, 4, 6.0), "charcoal", (s * 0.020, -0.055, 0.070))  # rails
        sk.add(rbox((0.004, 0.340, 0.007), 10, 3, 6.0), "neon", (s * 0.0262, -0.055, 0.082))     # neon strip
    for y in (-0.140, 0.040):
        sk.add(rbox((0.056, 0.050, 0.030), 8, 4), "charcoal", (0, y, H - 0.022))
    sk.add(rbox((0.130, 0.310, 0.018), 12, 6, 3.0), "charcoal", (0, -0.046, H - 0.004))      # sole
    sk.add(rbox((0.137, 0.315, 0.007), 12, 3, 3.0), "neon", (0, -0.046, H + 0.007))          # neon welt
    sk.add(rbox((0.134, 0.304, 0.114), 14, 8, 3.0, toe(0.42, 0.18)), "indigo", (0, -0.046, H + 0.048))
    sk.add(rcyl(0.142, 0.120, 14, 6, 5.0, dy=0.152), "charcoal", (0, 0.018, H + 0.112), (-0.12, 0, 0))   # low cuff
    opening(sk, 0.025, H + 0.170, 0.066, 0.071, "lining", "neon", -0.12)
    for s in (-1, 1):                                                                                 # side stripes
        sk.add(rbox((0.006, 0.120, 0.012), 8, 3, 3.0), "neon", (s * 0.066, -0.020, H + 0.060), (0, 0, 0))
    sk.add(rbox((0.046, 0.050, 0.050), 6, 4), "charcoal", (0, 0.112, H + 0.044))              # booster mount
    tilt = (math.pi / 2 - 0.18, 0, 0)                                                          # nozzle aimed a little down
    sk.add(rcyl(0.084, 0.104, 12, 4, 5.0), "iron", (0, 0.158, H + 0.062), tilt)               # booster body
    sk.add(ring(0.046, 0.046, 0.011, 12, 4), "plum", (0, 0.210, H + 0.053), tilt)             # flared lip
    sk.add(rcyl(0.074, 0.008, 12, 3, 2.5), "flame", (0, 0.209, H + 0.053), tilt)              # the glow inside
    return H


MODELS = (
    ("Manual", manual, "50 km/h"),
    ("Electric", electric, "100 km/h"),
    ("NeonBoost", neon, "250 km/h"),
)
LABEL = dict(Manual="Manual", Electric="Electric", NeonBoost="Neon Boost")


def build_pair(model, maker, suffix=""):
    """Returns {side: (empty, mesh, ankle height)}; side 1 is the left foot (+x, foot_l)."""
    out = {}
    for sg, tag, bone in ((1, "L", "foot_l"), (-1, "R", "foot_r")):
        sk = Skate("SK_%s_%s%s" % (model, tag, suffix))
        H = maker(sk, sg)
        za = H + ANKLE
        emp, ob = sk.finish(za, bone)
        out[sg] = (emp, ob, za)
    return out


def build_lod(model, maker):
    """The far model: the same parts with about half the segments. Kept in the .blend, hidden from the renders."""
    global DETAIL
    DETAIL = LOD_DETAIL
    try:
        pair = build_pair(model, maker, "_LOD1")
    finally:
        DETAIL = 1.0
    for emp, ob, za in pair.values():
        ob.hide_render = True
        setp(ob, "hide_viewport", True)
    return pair


def tris(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


# ---- the scene ---------------------------------------------------------------------------------------------------
def skate_mats():
    def mat(key, rough=0.9, ns=16.0, bump=0.06):
        c = PAL[key][0]
        M[key] = flat("M_Skate_" + key, c, rough=rough, bump=bump, noise_scale=ns)
    for key in PAL:
        mat(key)
    mat("charcoal", 0.85)
    for key, (c, s, _) in GLOW.items():
        M[key] = emissive("M_Skate_" + key, c, s)


def text(name, body, loc, size, mat, rot=(math.pi / 2, 0, 0)):
    cu = bpy.data.curves.new(name, 'FONT')
    cu.body = body
    cu.size = size
    cu.extrude = 0.0
    setp(cu, "align_x", 'CENTER')
    setp(cu, "align_y", 'CENTER')
    ob = bpy.data.objects.new(name, cu)
    ob.location = loc
    ob.rotation_euler = rot
    cu.materials.append(mat)
    COL.objects.link(ob)
    return ob


def place(emp, loc, yaw=0.0, scale=1.0):
    emp.location = loc
    emp.rotation_euler = (0, 0, yaw)
    emp.scale = (scale, scale, scale)


def show_pair(pair, cx, cy, cz, yaw, scale=1.0):
    """Stands a pair as worn: left foot on +x of the pair's own frame, the pair turned by yaw."""
    R = Matrix.Rotation(yaw, 3, 'Z')
    for sg, (emp, ob, za) in pair.items():
        off = R @ Vector((sg * LX * scale, 0.0, 0.0))
        place(emp, (cx + off.x, cy + off.y, cz + za * scale), yaw, scale)


def scene_all():
    random.seed(30)
    scene = new_scene("Skates30")
    global K_M
    K_M = kit_mats()
    M.update(K_M)
    skate_mats()
    ink = flat("M_Ink", (0.08, 0.07, 0.07), noise_scale=4.0)
    cams, facts = {}, {}

    # sheet and close-ups: three pairs on turntables before a plaster wall
    box("Ground", -6, 40, -8, 3, -0.3, 0, K_M["paving"], bevel=0)
    box("SheetWall", -2.2, 2.2, 0.75, 0.95, 0, 1.6, K_M["plaster"], bevel=0.02)
    box("SheetWallBase", -2.2, 2.2, 0.70, 0.95, 0, 0.18, K_M["stone_dark"], bevel=0.02)
    pairs = {}
    for i, (model, maker, speed) in enumerate(MODELS):
        cx = (i - 1) * 1.0
        bm = rcyl(0.64, 0.07, 32, 6, 4.0)
        ob = add_obj("Turntable_%d" % i, bm, K_M["cream"], loc=(cx, 0, 0.035))
        for p in ob.data.polygons:
            p.use_smooth = True
        pair = build_pair(model, maker)
        show_pair(pair, cx, 0.0, 0.07, math.radians(-58))
        pairs[model] = pair
        lod = build_lod(model, maker)
        show_pair(lod, cx, 0.0, 0.07, math.radians(-58))
        pairs[model + "_LOD1"] = lod
        text("Label_%s" % model, LABEL[model], (cx, 0.749, 0.98), 0.12, ink)
        text("Speed_%s" % model, "top speed " + speed, (cx, 0.749, 0.82), 0.065, ink)
        facts[model] = cx
    cams["sheet"] = (camera(scene, "Cam_Sheet", (0.0, -3.1, 1.25), (0.0, 0.25, 0.42), 42.0), (1800, 1000))
    for model, cx in facts.items():
        dx, dy = dict(Manual=(-0.40, -1.0), Electric=(0.20, -1.05), NeonBoost=(0.85, -0.62))[model]   # toe, side, booster
        cams[model.lower()] = (camera(scene, "Cam_" + model, (cx + dx, dy, 0.50), (cx, 0.0, 0.17), 50.0), (1400, 1000))
    cams["neon"] = cams.pop("neonboost")

    # turnaround: each left skate from four sides, on shelves
    TX = 10.0
    box("TurnWall", TX - 1.8, TX + 1.8, 0.30, 0.45, 0, 2.4, K_M["plaster"], bevel=0)
    views = ((math.radians(-40), "three-quarter"), (math.radians(-90), "outside"), (math.radians(180), "back"),
             (math.radians(0), "front"))
    for row, (model, maker, speed) in enumerate(MODELS):
        z0 = 1.18 - row * 0.52
        box("Shelf_%d" % row, TX - 0.75, TX + 1.35, -0.25, 0.30, z0 - 0.04, z0, K_M["wood"], bevel=0.01)
        emp, ob, za = pairs[model][1]
        text("TurnLabel_%s" % model, LABEL[model], (TX - 1.08, 0.299, z0 + 0.12), 0.085, ink)
        for col, (yaw, _) in enumerate(views):
            dup = bpy.data.objects.new("Turn_%s_%d" % (model, col), ob.data)
            COL.objects.link(dup)
            dup.location = (TX - 0.42 + col * 0.50, -0.02, z0 + za)
            dup.rotation_euler = (0, 0, yaw)
    for col, (_, label) in enumerate(views):
        text("TurnView_%d" % col, label, (TX - 0.42 + col * 0.50, 0.299, 1.58), 0.07, ink)
    cd = bpy.data.cameras.new("Cam_Turn")
    cd.type = 'ORTHO'
    cd.ortho_scale = 2.65
    co = bpy.data.objects.new("Cam_Turn", cd)
    co.location = (TX + 0.02, -6.0, 2.36)
    co.rotation_euler = (math.radians(76), 0, 0)
    COL.objects.link(co)
    cams["turnaround"] = (co, (1600, 1300))

    # worn: the Courier four times, in shoes and in each pair
    WX = 20.0
    box("WornWall", WX - 2.4, WX + 2.4, 1.2, 1.5, 0, 3.2, K_M["stone"], bevel=0)
    box("WornWallUpper", WX - 2.4, WX + 2.4, 1.18, 1.5, 2.2, 3.2, K_M["plaster"], bevel=0)
    who = "Courier"
    S = CAST[who].get("height", 1.72) / 1.72
    lineup = [("Shoes", None)] + [(m, mk) for (m, mk, _) in MODELS]
    for i, (model, maker) in enumerate(lineup):
        x = WX + (i - 1.5) * 0.95
        if maker is None:
            person(with_name(who, "Shoes"), x, 0.0, 0.0)
            text("WornLabel_Shoes", "in shoes", (x, 1.179, 2.36), 0.12, ink)
            continue
        pair = build_pair(model, maker, "_Worn")
        lift = pair[1][2] - ANKLE
        rig = person(with_name(who, model), x, 0.0, lift * S)
        for o in list(COL.objects):
            if o.name.startswith("%s_%s_Shoe" % (who, model)):
                o.hide_render = True                    # the skate's boot replaces the shoe
        show_pair(pair, x, 0.0, 0.0, 0.0, S)
        text("WornLabel_%s" % model, LABEL[model], (x, 1.179, 2.36), 0.12, ink)
    cams["worn"] = (camera(scene, "Cam_Worn", (WX, -6.2, 1.15), (WX, 0.0, 0.92), 50.0), (1600, 1000))
    cams["worn_feet"] = (camera(scene, "Cam_WornFeet", (WX, -3.9, 0.58), (WX, 0.0, 0.36), 35.0), (2000, 700))

    sun_and_sky(scene, (0.78, -0.40, 0.48), 8.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.35)   # the kit daylight, ADR-0007
    return scene, cams, pairs


def dusk(scene):
    """The dusk recipe of ART_DIRECTION 2.4, for one extra look at the glow."""
    for o in scene.objects:
        if o.type == 'LIGHT' and o.data.type == 'SUN':
            o.data.energy = 2.5
            o.data.color = (1.0, 0.42, 0.22)
            o.rotation_euler = Vector((0.42, 0.83, 0.36)).normalized().to_track_quat('Z', 'Y').to_euler()
    bg = scene.world.node_tree.nodes.get("Background")
    bg.inputs[0].default_value = rgba((0.06, 0.10, 0.30))
    bg.inputs[1].default_value = 0.6


def write_notes(pairs):
    note("\n== Models (one mesh per skate, origin at the ankle empty; triangles after triangulation) ==")
    total = {}
    for model, maker, speed in MODELS:
        pair = pairs[model]
        tl = tris(pair[1][1])
        tr = tris(pair[-1][1])
        far = tris(pairs[model + "_LOD1"][1][1])
        total[model] = (tl + tr, far * 2)
        emp, ob, za = pair[1]
        dims = ob.dimensions
        note("%-10s %-8s  %s %d tris, %s %d tris, pair %d (far model _LOD1: %d a skate, pair %d); materials %d; "
             "%.3f x %.3f x %.3f m (w x l x h); ankle empty %s at %.3f m above the ground" % (
                 LABEL[model], speed, ob.name, tl, pair[-1][1].name, tr, tl + tr, far, far * 2,
                 len(ob.data.materials), dims.x, dims.y, dims.z, emp.name, za))
    note("For scale: a Soft box car in the slice is about 3200 triangles near (car_export_11).")
    note("\n== Colours (linear RGB) ==")
    used = dict(Manual=("cream", "brown", "ochre", "terra", "iron", "alu", "lining"),
                Electric=("teal", "charcoal", "ochre", "cream", "alu", "lining", "status"),
                NeonBoost=("indigo", "plum", "charcoal", "iron", "lining", "neon", "flame"))
    for model, keys in used.items():
        parts = []
        for k in keys:
            if k in PAL:
                parts.append("%s %s" % (PAL[k][1], PAL[k][0]))
            else:
                c, s, nm = GLOW[k]
                parts.append("%s %s emissive %.1f" % (nm, c, s))
        note("%-10s %s" % (LABEL[model], "; ".join(parts)))
    note(DESIGN)
    return total


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(SRC, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    try:
        t0 = time.time()
        scene, cams, pairs = scene_all()
        log("scene built: %d objects in %.1fs" % (len(scene.objects), time.time() - t0))
        write_notes(pairs)
        note("")
        try:
            bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
            log("saved .blend")
        except Exception:
            log("save failed:\n" + traceback.format_exc())
        for key, (cam, res) in cams.items():
            if ONLY and key not in ONLY:
                continue
            try:
                scene.camera = cam
                configure(scene, -0.15, res, SAMPLES)
                t = time.time()
                scene.render.filepath = OUT_DIR + "/" + key + ".png"
                bpy.ops.render.render(write_still=True, scene=scene.name)
                log("%s rendered in %.1fs" % (key, time.time() - t))
            except Exception:
                log("FAILED %s:\n%s" % (key, traceback.format_exc()))
        if not ONLY or "neon_dusk" in ONLY:
            try:
                dusk(scene)
                scene.camera = cams["neon"][0]
                configure(scene, 0.2, cams["neon"][1], SAMPLES)
                t = time.time()
                scene.render.filepath = OUT_DIR + "/neon_dusk.png"
                bpy.ops.render.render(write_still=True, scene=scene.name)
                log("neon_dusk rendered in %.1fs" % (time.time() - t))
            except Exception:
                log("FAILED neon_dusk:\n" + traceback.format_exc())
    except Exception:
        log("FAILED:\n" + traceback.format_exc())
    log("DONE")


main()
