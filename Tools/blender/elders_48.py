"""Brix Town - elders 48: old people for the character library (CHR-002; look test).

Headless:
    blender-launcher.exe -b --factory-startup --python Tools/blender/elders_48.py
Quick checks: WASTED_ONLY=lineup,faces WASTED_SAMPLES=24 WASTED_SCALE=50; WASTED_ROOT is the checkout (default: this file's).

Builds on furniture_kit_18.py (and through it old_town_04.py): option B people, the painted matte materials, the
character sheets' light. Until now nobody in the cast could look old: the Elder of old_town_04.py is a short person
with grey hair. This adds the values that say age, on the same body and head (ART_DIRECTION C7), and six elders made
from them. The values, all optional, all 0 or absent on everyone made so far:

    age       0 to 1: lines on the brow, at the eyes and from nose to mouth, bags under the eyes, paler lips; from 0.5
              the eyebrows turn the hair's grey
    stoop     degrees the back leans forward from the hips, the head held up again
    hair      two more shapes: "thin" (a bare crown with hair round the back and sides) and "wisp" (thin, with a tuft on top)
    glasses   round wire frames, in a colour
    cane      a walking stick in the right hand, in a colour
    shawl     a cloth over the shoulders, in a colour
    beard     a short full beard in the hair's colour

ELDERS is the library: Noor (58, the baker, her age now showing), Grandmother, Grandfather, Tall gentleman, Market
woman and Great-grandmother, across the range of skin tones (C3).

Renders go to Art/StyleTests/elders_48/:
    lineup.png   the six, full height, with the old Elder of the cast at the left for comparison
    faces.png    the six from the chest up
    ages.png     one woman at age 0, 0.35, 0.7 and 1.0 with nothing else changed
Not approved: for the owner to choose from. Nothing here is in the game.
"""
import bpy, math, os, time, traceback
from mathutils import Vector, Matrix

try:
    _here = os.path.dirname(os.path.abspath(__file__))
    _root = os.path.normpath(os.path.join(_here, "..", ".."))
except NameError:
    _root = "C:/Projects/Wasted"
ROOT = os.environ.get("WASTED_ROOT", _root).replace("\\", "/").rstrip("/")
os.environ["WASTED_ROOT"] = ROOT
_k = open(ROOT + "/Tools/blender/furniture_kit_18.py", encoding="utf-8").read()
_k = _k[:_k.rstrip().rfind("\nmain()")]
exec(compile(_k, "furniture_kit_18_helpers", "exec"), globals())

NAME = "elders_48"
OUT = ROOT + "/Art/StyleTests"
SRC = ROOT + "/Art/Source/StyleTests"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"
ONLY = tuple(os.environ["WASTED_ONLY"].split(",")) if os.environ.get("WASTED_ONLY") else None
SAMPLES = int(os.environ.get("WASTED_SAMPLES", 96))
SCALE = int(os.environ.get("WASTED_SCALE", 100))

WHITE = (0.62, 0.61, 0.60)
SILVER = (0.40, 0.39, 0.40)
GREY = (0.27, 0.26, 0.27)
HORN = (0.07, 0.05, 0.04)
BRASS = (0.42, 0.31, 0.12)

# ---- the library ---------------------------------------------------------------------------------------------------
ELDERS = dict(
    Noor=dict(label="Noor, 58", height=1.62, build=1.12, skin=(0.56, 0.38, 0.27), hair="bun", hair_color=GREY, garment="long",
              top=BLUEC, bottom=BROWN, shoes=BROWN, scarf=OCHRE, extras=("apron",), apron_color=CREAM, eye_open=0.88, face_w=1.04,
              nose=1.1, age=0.45, stoop=2.0),
    Grandmother=dict(label="Grandmother", height=1.58, build=1.10, skin=(0.78, 0.58, 0.47), hair="bun", hair_color=WHITE, garment="dress",
                     dress_len=0.36, top=PLUM, bottom=PLUM, shoes=BROWN, shawl=CREAM, glasses=BRASS, eye_open=0.86, age=0.8, stoop=5.0),
    Grandfather=dict(label="Grandfather", height=1.66, build=1.14, skin=(0.66, 0.46, 0.34), hair="thin", hair_color=WHITE, garment="long",
                     top=CREAM, coat=BROWN, coat_len=0.50, bottom=CHARCOAL, shoes=BLACK, extras=("moustache", "cap"), cap_color=OLIVE,
                     cane=BROWN, nose=1.25, face_w=1.06, eye_open=0.85, age=0.85, stoop=7.0),
    Gentleman=dict(label="Tall gentleman", height=1.78, build=0.92, skin=(0.30, 0.19, 0.13), hair="wisp", hair_color=SILVER, garment="long",
                   top=CREAM, coat=INDIGO, coat_len=0.66, bottom=CHARCOAL, shoes=BLACK, scarf=TERRA, glasses=HORN, beard=True,
                   face_w=0.96, age=0.7, stoop=3.0),
    MarketWoman=dict(label="Market woman", height=1.60, build=1.16, skin=(0.44, 0.29, 0.20), hair="bob", hair_color=SILVER, garment="tunic",
                     top=TERRA, bottom=OLIVE, shoes=BROWN, scarf=CREAM, extras=("apron", "bag"), apron_color=OCHRE, face_w=1.08,
                     nose=1.15, age=0.6, stoop=2.0),
    GreatGrandmother=dict(label="Great-grandmother", height=1.56, build=0.96, skin=(0.84, 0.66, 0.55), hair="bun", hair_color=WHITE,
                          garment="dress", dress_len=0.30, top=TEALC, bottom=TEALC, shoes=CHARCOAL, shawl=OLIVE, cane=CHARCOAL, glasses=BRASS,
                          eye_open=0.85, face_w=0.97, age=1.0, stoop=11.0),
)


def elder(sp, ox, oy=0.0):
    """One person with the values that say age. Everything but those values is character3, unchanged."""
    cs = CHAR_STYLES["B"]
    sp = dict(sp)
    n = sp["name"]
    age = sp.get("age", 0.0)
    style = sp.get("hair", "short")
    base = dict(sp)
    if style in ("thin", "wisp"):
        base["hair"] = "bald"
    if age >= 0.5 and "brow_color" not in base:
        pass                                               # brows follow the hair's colour already (character3)
    if age > 0:                                            # lips lose colour with age
        s = sp["skin"]
        base["lips"] = (s[0] * (0.62 + 0.18 * age), s[1] * (0.36 + 0.30 * age), s[2] * (0.34 + 0.30 * age))
    before = set(o.name for o in COL.objects)
    character3(base, cs, ox, oy)
    # the same head space as character3
    S = sp.get("height", 1.72) / 1.72
    W = sp.get("build", 1.0) * cs["wide"]
    T = cs["thick"]
    fw = sp.get("face_w", 1.0)
    legk, tork = cs["leg"], cs["torso"]
    zm = lambda z: z * legk if z <= 0.93 else 0.93 * legk + (z - 0.93) * tork
    P = lambda x, y, z: Vector((ox + x * S, oy + y * S, zm(z) * S))
    k = cs["head"]
    K = k * S
    rx, ry, rz = 0.104 * fw, 0.110, 0.130
    hz = zm(1.475) + rz * k
    H = lambda x, y, z: Vector((ox + x * K, oy + y * K, (hz + z * k) * S))

    def face_y(x, z):
        zu = z / rz
        s = jaw(zu)
        return -ry * s * math.sqrt(max(1.0 - (x / (rx * s)) ** 2 - zu * zu, 0.0))

    F = lambda x, z, out=0.0: H(x, face_y(x, z) - out, z)
    skin = sp["skin"]
    hairc = sp.get("hair_color", (0.08, 0.05, 0.04))
    hair = solid(hairc, 0.6)
    ex, ez = 0.043 * fw, 0.008
    if age > 0:
        line = solid(dim(skin, 0.84 - 0.10 * age), 0.7)   # a line is a soft darker crease of the person's own skin, never black
        r = (0.0012 + 0.0006 * age) * K
        for i in range(1 if age < 0.4 else 2 if age < 0.8 else 3):              # the brow
            z = 0.066 + i * 0.014
            w = 0.040 - i * 0.006
            limb(n + "_BrowLine%d" % i, F(-w, z, 0.0008), F(w, z, 0.0008), r, r, line)
        for sg in (-1, 1):
            limb(n + "_Fold%d" % sg, F(sg * 0.021, -0.043, 0.0006), F(sg * 0.033, -0.062, 0.0006), r * 0.8, r * 0.5, line)   # nose to mouth
            if age >= 0.3:                                                     # at the outer corner of each eye
                for j, dz in enumerate((0.006, -0.004) if age < 0.7 else (0.009, 0.001, -0.007)):
                    limb(n + "_Crow%d_%d" % (sg, j), F(sg * (ex + 0.031), ez + dz * 0.5, 0.0008), F(sg * (ex + 0.040), ez + dz * 1.3, 0.0008), r * 0.8, r * 0.5, line)
            if age >= 0.5:                                                     # under the eyes
                ellipsoid(n + "_Bag%d" % sg, F(sg * ex, ez - 0.024, 0.0004), (0.017 * K, 0.003 * K, 0.0028 * K), solid(dim(skin, 0.90), 0.6))
    if style not in ("thin", "wisp", "bald", "afro"):       # hair that sits on the head: the shell character3 makes stands off
        # the skull, and the gap under its rim fell into shade, so pale hair read as a cap over dark hair (the owner,
        # 2026-11-26). The gap is filled with hair, set back so the face stays clear and the hairline shows at the brow.
        ellipsoid(n + "_HairFill", H(0, 0.042, 0.036), ((rx + 0.018) * K, (ry + 0.020) * K, 0.124 * K), hair)
        if style in ("bob", "long"):                        # these hang lower: filled down to the jaw, behind the ears
            ellipsoid(n + "_HairFillLow", H(0, 0.050, -0.045), ((rx + 0.016) * K, (ry + 0.006) * K, 0.105 * K), hair)
    if style in ("thin", "wisp"):                           # a bare crown: hair only round the back and the sides
        for i in range(9):
            a = math.radians(-120 + i * 30)                 # from behind one ear round the back to the other (the face is at -y)
            x, y = -math.sin(a), math.cos(a)
            ellipsoid(n + "_Fringe%d" % i, H(x * rx * 0.97, y * ry * 0.93 + 0.012, 0.012), (0.036 * K, 0.036 * K, 0.052 * K), hair)
        for sg in (-1, 1):
            ellipsoid(n + "_Temple%d" % sg, H(sg * rx * 0.98, 0.02, 0.03), (0.014 * K, 0.034 * K, 0.034 * K), hair)
        if style == "wisp":
            ellipsoid(n + "_Wisp", H(0.006, -0.02, 0.122), (0.050 * K, 0.066 * K, 0.014 * K), hair, (0.0, 0.12, 0.0))
    if sp.get("beard"):
        for i in range(7):
            a = math.radians(-66 + i * 22)
            x = math.sin(a) * rx * 0.80
            z = -0.088 - math.cos(a) * 0.030
            ellipsoid(n + "_Beard%d" % i, F(x, z, 0.004), (0.022 * K, 0.014 * K, 0.026 * K), hair)
        ellipsoid(n + "_Moustache", F(0, -0.056, 0.004), (0.028 * K, 0.007 * K, 0.0075 * K), hair)
    if sp.get("glasses"):
        wire = solid(sp["glasses"], 0.5)
        rr = 0.027 * K
        for sg in (-1, 1):
            c = F(sg * ex, ez, 0.013)
            pts = [c + Vector((math.cos(i * math.pi / 7) * rr, -abs(math.cos(i * math.pi / 7)) * 0.0 , math.sin(i * math.pi / 7) * rr)) for i in range(15)]
            for i in range(14):
                limb(n + "_Rim%d_%d" % (sg, i), pts[i], pts[i + 1], 0.0022 * K, 0.0022 * K, wire)
            limb(n + "_Arm%d" % sg, c + Vector((sg * rr, 0, 0.002 * K)), H(sg * rx * 0.99, 0.02, ez + 0.004), 0.002 * K, 0.002 * K, wire)
        limb(n + "_Bridge", F(-ex, ez, 0.013) + Vector((rr, 0, 0.004 * K)), F(ex, ez, 0.013) + Vector((-rr, 0, 0.004 * K)), 0.0022 * K, 0.0022 * K, wire)
    if "shawl" in sp:
        sh = cloth(sp["shawl"])
        chest = 0.172 * W
        ellipsoid(n + "_Shawl", P(0, 0.012, 1.375), ((chest + 0.075) * S, 0.135 * T * S, 0.085 * S), sh)
        for sg in (-1, 1):
            limb(n + "_ShawlEnd%d" % sg, P(sg * 0.06, -0.105 * T, 1.37), P(sg * 0.045, -0.118 * T, 1.10), 0.050 * S, 0.034 * S, sh, flat=0.5)
    # the stoop: everything above the hips leans forward, and the head is lifted back so the eyes still look ahead
    stoop = math.radians(sp.get("stoop", 0.0))
    if stoop:
        mine = [o for o in COL.objects if o.name not in before]
        hip = P(0, 0, 0.96)
        neck = P(0, 0.01, 1.47)
        lean = Matrix.Translation(hip) @ Matrix.Rotation(-stoop, 4, 'X') @ Matrix.Translation(-hip)
        neck2 = lean @ neck
        lift = Matrix.Translation(neck2) @ Matrix.Rotation(stoop * 0.8, 4, 'X') @ Matrix.Translation(-neck2)
        bpy.context.view_layer.update()
        for o in mine:
            z = o.matrix_world.translation.z
            if z < hip.z + 0.02 * S or o.name.endswith(("_Skirt", "_Hem")):
                continue
            m = lean @ o.matrix_world
            if z > neck.z - 0.005 * S:
                m = lift @ m
            o.matrix_world = m
    if "cane" in sp:                                         # held in the right hand, its foot a step ahead
        wood = solid(sp["cane"], 0.6)
        fwd = math.sin(stoop) * 0.45 * S
        top = P(0.265 * W, -0.06 - fwd / S, 0.86)
        limb(n + "_Cane", top, P(0.30 * W, -0.20 - fwd / S, 0.0), 0.013 * S, 0.011 * S, wood)
        limb(n + "_CaneHandle", top + Vector((0, 0.045 * S, 0)), top + Vector((0, -0.055 * S, 0.004)), 0.016 * S, 0.014 * S, wood)


def stand(who, x, y=0.0, tag="L", **over):
    sp = dict(ELDERS[who] if who in ELDERS else CAST[who])
    sp.update(over)
    sp["name"] = "%s_%s" % (who, tag)
    elder(sp, x, y)
    return sp


def wall():
    box("Ground", -9, 9, -12, 4, -0.3, 0, M["paving"], bevel=0)
    box("Wall", -9, 9, 1.3, 1.9, 0, 3.2, M["stone"])
    box("Wall_Upper", -9, 9, 1.36, 1.9, 3.2, 6, M["plaster"])
    box("Wall_Ledge", -9, 9, 1.18, 1.9, 3.2, 3.45, M["stone_dark"])
    lantern("Lamp_L", -3.2, 1.2, 2.8, M, 60)
    lantern("Lamp_R", 3.2, 1.2, 2.8, M, 60)
    sun_and_sky(SCENE, (-0.45, -0.50, 0.74), 3.2, (1.0, 0.82, 0.62), (0.42, 0.55, 0.82), 0.25, angle=6.0)


def lineup_scene(with_old=True):
    wall()
    names = (["Elder"] if with_old else []) + list(ELDERS)
    gap = 0.98
    x0 = -(len(names) - 1) * gap / 2
    for i, who in enumerate(names):
        x = x0 + i * gap
        sp = stand(who, x)
        label = sp.get("label", "The cast's Elder (as it was)")
        text("Label_%d" % i, label, x, 1.16, 2.0 if i % 2 else 2.14, 0.075, "cream")


def ages_scene():
    wall()
    for i, a in enumerate((0.0, 0.35, 0.7, 1.0)):
        x = (i - 1.5) * 0.95
        stand("Grandmother", x, tag="A%d" % i, age=a, stoop=0.0, glasses=None, hair_color=WHITE if a > 0.5 else GREY if a > 0.2 else (0.12, 0.07, 0.05))
        text("Label_%d" % i, "age %.2f" % a, x, 1.16, 2.02, 0.09, "cream")


def render(scene, key, cam, exposure, res):
    nm, loc, tgt, lens = cam
    scene.camera = camera(scene, "Cam_%s" % key, loc, tgt, lens)
    configure(scene, exposure, res, SAMPLES)
    t = time.time()
    scene.render.filepath = OUT_DIR + "/" + key + ".png"
    bpy.ops.render.render(write_still=True, scene=scene.name)
    log("%s rendered in %.1fs (%d objects)" % (key, time.time() - t, len(scene.objects)))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(SRC, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    for key in ("lineup", "faces", "ages"):
        if ONLY and key not in ONLY:
            continue
        try:
            scene = fresh("Furn_" + key)
            if key == "lineup":
                lineup_scene()
                render(scene, key, ("cam", (4.2, -10.8, 1.25), (0.1, 0, 1.0), 62.0), -0.3, (2000, 900))
                try:
                    bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
                    log("saved .blend")
                except Exception:
                    log("save failed:\n" + traceback.format_exc())
            elif key == "faces":
                lineup_scene(with_old=False)
                render(scene, key, ("cam", (0, -9.0, 1.42), (0, 0, 1.36), 50.0), -0.3, (2200, 640))
            else:
                ages_scene()
                render(scene, key, ("cam", (0, -7.0, 1.44), (0, 0, 1.38), 62.0), -0.3, (1900, 700))
        except Exception:
            log("FAILED %s:\n%s" % (key, traceback.format_exc()))
    log("DONE")


main()
