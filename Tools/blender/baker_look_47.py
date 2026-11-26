"""Brix Town - baker look 47: Noor, the master baker of the Stone Oven, and the bakery inside (QST-014; look test).

Headless:
    blender-launcher.exe -b --factory-startup --python Tools/blender/baker_look_47.py
Quick checks: WASTED_ONLY=noor,bakery_day WASTED_SAMPLES=24 WASTED_SCALE=50; WASTED_ROOT is the checkout (default: this file's).

Builds on elders_48.py (the values that say age, hair built on the skull, look_check.py) and through it on
furniture_kit_18.py (and through it kit 17, the car kit, the building kit, old_town_04.py): the same painted
matte materials, the kit daylight (ADR-0007), option B people made from the designer's values only (ART_DIRECTION C7)
and the kit's parts. Nothing here is a new shape of person or a new material.

Noor (docs/narrative/src/cast.json): a woman of 58, master baker, fourth keeper of the Stone Oven. Short sentences;
counts everything; hides her hands. Three options, the same woman dressed three ways:
    A  Linen      a dusty-blue long tunic to the knee, a cream apron, an ochre kerchief at the neck, a grey bun
    B  Flour cap  terracotta tunic, a cream apron and a soft cream baker's cap set on a dark grey bun
    C  Old Town   an olive dress to the shin, a cream apron, a grey bob
The bakery: a stone oven with an arched mouth and a wood store under it, a kneading bench with a tray of rolls, racks
of loaves with the crock of the Mother on the top shelf, flour sacks under the price board, the counter, the plank door
to the alley with its lantern (the four o'clock door) and the door to the proofing room. Its light is set against
Art/Reference/interior_room_daylight.png: mostly warm shade, a patch of sun, pools of lamp and oven light.

Renders go to Art/StyleTests/baker_look_47/:
    noor.png          the three options, full height
    noor_close.png    the same three, from the waist up
    bakery_day.png    the bakery in the kit daylight, Noor (N, the owner's) at the bench
    bakery_four.png   the same at four in the morning: the oven and the lamps only (rule I5)
The owner picked A for Noor on 2026-11-26 ("pick option A for noor"), asked for a long tunic, then for option C's
clothes on her: N is that Noor, A's head and bun in C's olive dress, confirmed ("Yes, keep this Noor"). A, B and C stay
for the record. The bakery
inside was approved the same day ("Yes, approved"; ADR-0059). Nothing here is in the game.
"""
import bpy, math, random, os, time, traceback

try:
    _here = os.path.dirname(os.path.abspath(__file__))
    _root = os.path.normpath(os.path.join(_here, "..", ".."))
except NameError:
    _root = "C:/Projects/Wasted"
ROOT = os.environ.get("WASTED_ROOT", _root).replace("\\", "/").rstrip("/")
os.environ["WASTED_ROOT"] = ROOT
_k = open(ROOT + "/Tools/blender/elders_48.py", encoding="utf-8").read()
_k = _k[:_k.rstrip().rfind("\nmain()")]                   # elders 48 (and through it kit 18): the values that say age, hair on the skull, the look check
exec(compile(_k, "elders_48_helpers", "exec"), globals())

NAME = "baker_look_47"
OUT = ROOT + "/Art/StyleTests"
SRC = ROOT + "/Art/Source/StyleTests"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"
ONLY = tuple(os.environ["WASTED_ONLY"].split(",")) if os.environ.get("WASTED_ONLY") else None
SAMPLES = int(os.environ.get("WASTED_SAMPLES", 96))
SCALE = int(os.environ.get("WASTED_SCALE", 100))

# ---- Noor: one woman, three ways of dressing (the designer's values only) ------------------------------------------
IRON_GREY = (0.17, 0.16, 0.17)
NOOR = dict(age=0.45, stoop=2.0, height=1.62, build=1.12, skin=(0.56, 0.38, 0.27), eyes=(0.17, 0.10, 0.06), eye_open=0.86, brow=1.1, face_w=1.04,
            nose=1.1, shoes=BROWN, bottom=BROWN, extras=("apron",), apron_color=CREAM)
OPTIONS = [
    ("A", "Linen", dict(NOOR, hair="bun", hair_color=GREY, garment="tunic", tunic_len=0.50, top=BLUEC, scarf=OCHRE)),   # a long tunic to the knee (the owner, 2026-11-26)
    ("B", "Flour cap", dict(NOOR, hair="bun", hair_color=IRON_GREY, garment="tunic", top=TERRA, baker_cap=CREAM)),
    ("C", "Old Town", dict(NOOR, hair="bob", hair_color=GREY, garment="dress", dress_len=0.40, top=OLIVE, bottom=OLIVE)),
    # the owner's Noor (2026-11-26): picked A, then asked for a long tunic, then "what is option C wearing? can you make
    # Noor wear that": A's head and grey bun in C's olive dress to the shin with its bib apron
    ("N", "Noor", dict(NOOR, hair="bun", hair_color=GREY, garment="dress", dress_len=0.40, top=OLIVE, bottom=OLIVE)),
]


def noor(key, x, y, tag):
    sp = dict([o for o in OPTIONS if o[0] == key][0][2])
    sp["name"] = "Noor_%s_%s" % (key, tag)
    elder(sp, x, y)                                          # 58: her age shows (elders_48.py)
    hd = head_of(sp, x, y)
    STOOD.append(hd)
    if "baker_cap" in sp:                                    # a soft cloth cap set on the hair, the hairline showing: not the courier's peaked cap
        K, (hx, hy), hz = hd["K"], hd["at"], hd["hz"]
        m = cloth(sp["baker_cap"])
        ellipsoid(sp["name"] + "_Cap", Vector((hx, hy + 0.030 * K, hz + 0.118 * K)), (0.098 * K, 0.104 * K, 0.058 * K), m)
        ellipsoid(sp["name"] + "_Cap_Band", Vector((hx, hy + 0.028 * K, hz + 0.088 * K)), (0.104 * K, 0.108 * K, 0.022 * K), m)


def noor_scene():
    box("Ground", -8, 8, -12, 4, -0.3, 0, M["paving"], bevel=0)
    box("Wall", -8, 8, 1.3, 1.9, 0, 3.2, M["stone"])
    box("Wall_Upper", -8, 8, 1.36, 1.9, 3.2, 6, M["plaster"])
    box("Wall_Ledge", -8, 8, 1.18, 1.9, 3.2, 3.45, M["stone_dark"])
    for i, (key, label, _) in enumerate(OPTIONS):
        x = (i - 1.5) * 1.15
        noor(key, x, 0.0, "L")
        text("Label_" + key, "%s  %s" % (key, label), x, 1.16, 1.98, 0.12, "cream")
    lantern("Lamp_L", -2.4, 1.2, 2.8, M, 60)
    lantern("Lamp_R", 2.4, 1.2, 2.8, M, 60)
    sun_and_sky(SCENE, (-0.45, -0.50, 0.74), 3.2, (1.0, 0.82, 0.62), (0.42, 0.55, 0.82), 0.25, angle=6.0)


# ---- the bakery's own pieces, from the kit's parts ------------------------------------------------------------------
def loaf(P, n, x, y, z, rng, kind=None):
    """A loaf on a shelf: a boule with a floured top, a long tin loaf or a baton."""
    kind = kind or rng.choice(("boule", "boule", "tin", "baton"))
    crust = M["crust_dark"] if rng.random() < 0.4 else M["crust"]
    if kind == "boule":
        ball(P, n, crust, x, y, z + 0.075, 0.27, 0.27, 0.16)
        ball(P, n + "_Flour", M["flour"], x, y, z + 0.115, 0.17, 0.17, 0.085)
    elif kind == "tin":
        rbox(P, n, crust, x, y, z + 0.07, 0.30, 0.15, 0.14, 0.05)
        ball(P, n + "_Crown", crust, x, y, z + 0.12, 0.30, 0.16, 0.11)
    else:
        ball(P, n, crust, x, y, z + 0.05, 0.42, 0.11, 0.10)
        for i in (-1, 0, 1):
            ball(P, n + "_Cut%d" % i, M["flour"], x + i * 0.1, y, z + 0.085, 0.07, 0.07, 0.035)


def stone_oven(P):
    """The Stone Oven: laid stone, an arched mouth over a hearth ledge, wood stacked under it, the flue up through the ceiling."""
    w, d, h = 2.5, 1.5, 2.25
    rbox(P, "Oven_Base", M["stone_c"], 0, 0, 0.5, w, d, 1.0, 0.04)
    rbox(P, "Oven_Body", M["stone_a"], 0, 0, 1.0 + (h - 1.0) / 2, w - 0.1, d - 0.06, h - 1.0, 0.06)
    rbox(P, "Oven_Ledge", M["stone_dark"], 0, -d / 2 - 0.1, 1.02, w - 0.5, 0.34, 0.09, 0.03)
    rbox(P, "Oven_Cornice", M["stone_dark"], 0, 0, h + 0.05, w + 0.06, d + 0.04, 0.12, 0.03)
    rbox(P, "Oven_Flue", M["stone_b"], 0, 0.15, h + 0.5, 1.0, 0.9, 0.9, 0.05)
    # the mouth: a dark barrel into the body with the fire's light inside it
    mz, mr = 1.07, 0.46
    rod(P, "Oven_Mouth", M["dark"], (0, -d / 2 - 0.012, mz + 0.14), (0, -d / 2 + 0.75, mz + 0.14), mr)
    rbox(P, "Oven_MouthFloor", M["stone_dark"], 0, -d / 2 + 0.2, mz - 0.17, 1.1, 0.9, 0.42, 0.01)
    ball(P, "Oven_Embers", M["ember"], 0, -d / 2 - 0.02, mz + 0.02, 0.62, 0.1, 0.14)   # the fire, seen in the mouth
    for i, (x, hh) in enumerate(((-0.14, 0.26), (0.02, 0.36), (0.17, 0.22))):
        ball(P, "Oven_Flame%d" % i, M["flame"], x, -d / 2 - 0.02, mz + 0.04 + hh / 2, 0.15, 0.06, hh)
    light(P, "Oven_Glow", 0, -d / 2 - 0.35, mz + 0.2, 45.0)
    for i in range(9):                                       # the arch stones round the mouth
        a = math.pi * (i + 0.5) / 9
        x, z = math.cos(a) * (mr + 0.1), mz + 0.14 + math.sin(a) * (mr + 0.1)
        ob = rbox(P, "Oven_Arch%d" % i, M["stone_b"] if i != 4 else M["stone_dark"], x, -d / 2 - 0.02, z, 0.2, 0.12, 0.26 if i == 4 else 0.22, 0.025)
        if ob is not None:
            ob.rotation_euler = (0, math.pi / 2 - a, 0)
    # the iron door, swung open to the left, and its bar
    rbox(P, "Oven_Door", M["iron"], -0.82, -d / 2 - 0.3, mz + 0.28, 0.06, 0.6, 0.72, 0.03)
    rod(P, "Oven_DoorBar", M["iron"], (-0.86, -d / 2 - 0.5, mz + 0.3), (-0.86, -d / 2 - 0.1, mz + 0.3), 0.022)
    # the wood store under the hearth
    rbox(P, "Oven_Store", M["dark"], 0, -d / 2 + 0.02, 0.42, 1.3, 0.12, 0.62, 0.02)
    rng = random.Random(45)
    for r in range(3):
        for c in range(6 - r % 2):
            x = -0.5 + c * 0.2 + (0.1 if r % 2 else 0)
            rod(P, "Oven_Log%d_%d" % (r, c), M["wood"] if rng.random() < 0.6 else M["cardboard"], (x, -d / 2 - 0.05, 0.2 + r * 0.17), (x, -d / 2 + 0.4, 0.2 + r * 0.17), 0.085)
    # the peel, leaning by the mouth
    rod(P, "Peel_Shaft", M["cardboard"], (1.0, -d / 2 - 0.16, 0.05), (1.12, -d / 2 - 0.05, 1.95), 0.022)
    rbox(P, "Peel_Blade", M["cardboard"], 1.14, -d / 2 - 0.04, 2.15, 0.34, 0.03, 0.42, 0.012)


def bench(P, L, rng):
    """The kneading bench: a thick pale top on block legs, flour, dough, the scale she weighs every piece on."""
    w, d, h = 2.3, 0.95, 0.9
    for sx in (-1, 1):
        for sy in (-1, 1):
            rbox(P, "Bench_Leg%d%d" % (sx, sy), M["wood"], sx * (w / 2 - 0.12), sy * (d / 2 - 0.12), (h - 0.08) / 2, 0.13, 0.13, h - 0.08, 0.02)
    rbox(P, "Bench_Rail", M["wood"], 0, 0, 0.22, w - 0.2, d - 0.24, 0.06, 0.015)
    rbox(P, "Bench_Top", M["bench"], 0, 0, h - 0.04, w, d, 0.09, 0.02)
    ball(P, "Bench_Flour", M["flour"], -0.25, 0.0, h + 0.004, 1.1, 0.6, 0.012)
    for i, (x, y) in enumerate(((-0.6, 0.05), (-0.3, -0.12), (0.0, 0.1), (-0.32, 0.22))):
        ball(P, "Bench_Dough%d" % i, M["dough"], x, y, h + 0.07, 0.24, 0.24, 0.15)
    rod(P, "Bench_Pin", M["cardboard"], (0.25, -0.25, h + 0.045), (0.75, -0.12, h + 0.045), 0.04)
    rbox(P, "Bench_Scale", M["iron"], 0.82, 0.18, h + 0.07, 0.3, 0.26, 0.12, 0.03)
    cylz(P, "Bench_Pan", M["ochre"], 0.82, 0.18, h + 0.13, 0.14, 0.03, r_top=0.16, bev=0.008)
    for i in range(3):                                       # proofing baskets on the rail under the top
        cylz(P, "Bench_Basket%d" % i, M["wicker"], -0.7 + i * 0.5, 0, 0.25, 0.14, 0.14, r_top=0.19, bev=0.01)
        ball(P, "Bench_Cloth%d" % i, M["cream"], -0.7 + i * 0.5, 0, 0.4, 0.3, 0.3, 0.1)


def racks(P, L, rng, w=2.6):
    """Bread racks along the wall: four slatted shelves of loaves, and one crock alone on the top."""
    d, h = 0.5, 2.2
    for sx in (-1, 1):
        for sy in (-1, 1):
            rbox(P, "Rack_Post%d%d" % (sx, sy), M["wood"], sx * (w / 2 - 0.04), sy * (d / 2 - 0.04), h / 2, 0.08, 0.08, h, 0.012)
    for s, z in enumerate((0.35, 0.82, 1.29, 1.76)):
        rbox(P, "Rack_Shelf%d" % s, L["accent_wood"], 0, 0, z, w - 0.06, d, 0.05, 0.012)
        n = int((w - 0.3) / 0.36)
        for i in range(n):
            if rng.random() < 0.12:
                continue
            loaf(P, "Rack_Loaf%d_%d" % (s, i), -w / 2 + 0.3 + i * 0.36, rng.uniform(-0.04, 0.06), z + 0.025, rng)
    rbox(P, "Rack_Top", L["accent_wood"], 0, 0, h, w, d + 0.04, 0.05, 0.012)
    cylz(P, "Mother_Crock", M["pot"], -w / 2 + 0.45, 0, h + 0.025, 0.17, 0.3, r_top=0.15, bev=0.02)   # the Mother: nobody touches the jar on the top shelf
    ball(P, "Mother_Cloth", M["cream"], -w / 2 + 0.45, 0, h + 0.33, 0.36, 0.36, 0.1)
    rod(P, "Mother_Tie", M["terra"], (-w / 2 + 0.29, 0, h + 0.3), (-w / 2 + 0.61, 0, h + 0.3), 0.012)


def sacks(P, rng):
    for i, (x, y, z, yaw) in enumerate(((0, 0, 0, 0.1), (0.55, 0.1, 0, -0.2), (0.25, 0.05, 0.36, 0.3))):
        ob = blob(P, "Sack%d" % i, M["sack"], x, y, z + 0.22, 0.6, 0.42, 0.46, top=(0.8, 0.85), levels=2)
        if ob is not None:
            ob.rotation_euler = (0, 0, yaw)
    ball(P, "Sack_Spill", M["flour"], 0.3, -0.38, 0.006, 0.6, 0.3, 0.014)
    cylz(P, "Sack_Scoop", M["iron"], 0.2, -0.35, 0.01, 0.07, 0.1, r_top=0.09, bev=0.008)


def shop_counter(P, L, rng):
    p_counter(P, L, rng)                                     # kit 18's counter: till, scale, paper
    cylz(P, "Ctr_Basket", M["wicker"], -1.25, 0.0, 0.0, 0.26, 0.42, r_top=0.32, bev=0.012)
    for i in range(5):
        a = i * 1.26
        rod(P, "Ctr_Baton%d" % i, M["crust"], (-1.25 + math.cos(a) * 0.12, math.sin(a) * 0.12, 0.25), (-1.25 + math.cos(a) * 0.2, math.sin(a) * 0.2, 0.92), 0.045)


def door(P, n, w=0.9, h=2.05, lamp=False):
    """A plank door in a stone frame against a wall (the wall is at +y of the group), with a latch; lamp: a lantern over it."""
    rbox(P, n + "_Frame", M["stone_dark"], 0, 0.0, h / 2 + 0.04, w + 0.24, 0.1, h + 0.16, 0.02)
    rbox(P, n + "_Leaf", M["wood"], 0, -0.04, h / 2, w, 0.07, h, 0.015)
    for i in range(4):
        rbox(P, n + "_Plank%d" % i, M["cardboard"] if i % 2 else M["wood"], -w / 2 + w * (i + 0.5) / 4, -0.08, h / 2, w / 4 - 0.02, 0.02, h - 0.08, 0.008)
    for z in (0.35, h - 0.35):
        rbox(P, n + "_Strap%d" % int(z * 10), M["iron"], 0, -0.095, z, w - 0.06, 0.02, 0.07, 0.008)
    ball(P, n + "_Latch", M["iron"], w / 2 - 0.12, -0.11, 1.0, 0.07, 0.07, 0.07)
    if lamp:
        rbox(P, n + "_Lamp", M["shade"], 0, -0.12, h + 0.32, 0.16, 0.14, 0.2, 0.03)
        light(P, n + "_LampLight", 0, -0.3, h + 0.3, 18.0)


def price_board(P):
    rbox(P, "Board", M["chalk"], 0, 0, 1.75, 1.0, 0.04, 0.7, 0.01)
    frame_rect(P, "Board_Frame", M["wood"], 0, -0.01, 1.75, 1.06, 0.76)
    for i, wd in enumerate((0.62, 0.5, 0.7, 0.44)):
        rbox(P, "Board_Line%d" % i, M["cream"], -0.42 + wd / 2, -0.03, 1.98 - i * 0.15, wd, 0.008, 0.03, 0.004)
        rbox(P, "Board_Price%d" % i, M["ochre"], 0.36, -0.03, 1.98 - i * 0.15, 0.14, 0.008, 0.03, 0.004)


def rolls_tray(P, n, x, y, z, rng):
    rbox(P, n, M["iron"], x, y, z + 0.012, 0.6, 0.4, 0.025, 0.008)
    for i in range(4):
        for j in range(3):
            ball(P, "%s_Roll%d%d" % (n, i, j), M["crust"] if (i + j) % 3 else M["crust_dark"], x - 0.21 + i * 0.14, y - 0.12 + j * 0.12, z + 0.06, 0.12, 0.11, 0.08)


def bakery_scene(four):
    rng = random.Random(45)
    L = [l for l in lines() if l["key"] == "oldtown"][0]
    room_shell("Bakery", 0, 0, w=7.6, d=4.8, h=3.0, front=2.6)
    stone_oven(group("Oven", -2.35, 1.6))
    racks(group("Racks", 1.75, 2.12), L, rng, w=2.5)
    door(group("Door_Proofing", -0.45, 2.36), "Proofing")                    # the proofing room: its key is a quest's reward
    door(group("Door_Alley", -3.76, -1.9, math.pi / 2), "Alley", lamp=True)  # the side door bakers use, the four o'clock door
    price_board(group("PriceBoard", -3.78, -0.2, math.pi / 2))                  # on the wall over the flour sacks
    Pb = group("Bench", -0.2, -0.15, 0.06)
    bench(Pb, L, rng)
    rolls_tray(Pb, "Tray", 0.72, -0.2, 0.905, rng)
    sacks(group("Sacks", -3.25, -0.2, math.pi / 2), rng)
    shop_counter(group("Counter", 2.35, -1.5, math.pi), L, rng)
    d_pendant(group("Pendant_A", -0.2, -0.15), L, rng)
    d_pendant(group("Pendant_B", 2.4, -1.4), L, rng)
    noor("N", 0.35, 0.72, "S")                              # behind her bench, at work
    for ob in SCENE.objects:                                 # by day the lamps are low and the window carries the room (I5)
        if ob.type == 'LIGHT' and ob.name.startswith("Pen_Light"):
            ob.data.energy *= 0.5 if four else 0.06
    if four:                                                 # the four o'clock door: no sun yet, the oven and the lamps carry the room
        sun_and_sky(SCENE, (0.42, 0.83, 0.36), 0.02, (0.5, 0.6, 1.0), (0.01, 0.02, 0.07), 0.25)
    else:
        sun_and_sky(SCENE, (0.78, 0.30, 0.42), 8.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.25)
    return [("cam", (1.5, -4.75, 1.7), (-0.75, 0.5, 1.1), 15.5)]


def bakery_mats():
    extra_mats()
    M.update(crust=painted("M_Crust", (0.36, 0.22, 0.10), (0.56, 0.38, 0.18), noise_scale=5.0, bump=0.3),
             crust_dark=painted("M_CrustDark", (0.22, 0.12, 0.06), (0.40, 0.24, 0.11), noise_scale=5.0, bump=0.3),
             flour=flat("M_Flour", (0.72, 0.69, 0.62), bump=0.25, noise_scale=8.0),
             dough=painted("M_Dough", (0.62, 0.56, 0.44), (0.72, 0.67, 0.55), noise_scale=3.0, bump=0.1),
             sack=painted("M_Sack", (0.46, 0.38, 0.25), (0.62, 0.54, 0.40), noise_scale=9.0, bump=0.4),
             bench=painted("M_BenchTop", (0.50, 0.40, 0.25), (0.68, 0.60, 0.44), noise_scale=3.0, bump=0.15))


def render(scene, key, cams, exposure, res):
    for i, (nm, loc, tgt, lens) in enumerate(cams):
        scene.camera = camera(scene, "Cam_%s_%d" % (key, i), loc, tgt, lens)
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
    for key in ("check", "noor", "noor_close", "bakery_day", "bakery_four"):
        if ONLY and key not in ONLY:
            continue
        try:
            scene = fresh("Furn_" + key)
            bakery_mats()
            del STOOD[:]
            if key == "check":                               # the three heads measured close up (look_check.py)
                noor_scene()
                configure(scene, -0.3, (420, 420), 48)
                say(check_heads(scene, list(STOOD), OUT_DIR + "/check"), log)
                continue
            if key == "noor":
                noor_scene()
                render(scene, key, [("cam", (0, -9.2, 1.1), (0, 0, 1.0), 70.0)], -0.3, (1800, 1000))
            elif key == "noor_close":
                noor_scene()
                render(scene, key, [("cam", (0, -7.4, 1.42), (0, 0, 1.3), 62.0)], -0.3, (2000, 760))
            else:
                cams = bakery_scene(key == "bakery_four")
                render(scene, key, cams, 0.55 if key == "bakery_four" else 1.15, (1700, 1000))
                if key == "bakery_day":
                    try:
                        bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
                        log("saved .blend")
                    except Exception:
                        log("save failed:\n" + traceback.format_exc())
        except Exception:
            log("FAILED %s:\n%s" % (key, traceback.format_exc()))
    log("DONE")


main()
