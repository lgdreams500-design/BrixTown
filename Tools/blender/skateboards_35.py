"""Brix Town - skateboards 35: the skate store's three tiers as skateboards, a look test (game progression; look test).

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/skateboards_35.py").read(), "skateboards_35.py", "exec"))
or headless:
    blender-launcher.exe -b --factory-startup --python C:/Projects/Wasted/Tools/blender/skateboards_35.py
Quick checks: WASTED_ONLY=sheet,ride_manual WASTED_SAMPLES=24 WASTED_SCALE=50
From another checkout: WASTED_ROOT=C:/Projects/Wasted-skates (where renders and the .blend go; the helpers are read
from there too when it has them).

Builds on skates_34.py (and through it car_kit_09.py and character_face_05.py): its soft low-poly parts, its palette and
glow colours, the kit daylight (ADR-0007) and the option B people. The owner saw the three pairs of skates of look test 34
and asked for skateboards instead; the tiers, top speeds and colours carry over:

    Manual      50 km/h   a classic wooden street board: popsicle deck with nose and tail kicks, grip, ochre wheels
    Electric   100 km/h   a longer teal deck, a charcoal battery box with an ochre band, hub motors in the rear wheels
    Neon Boost 250 km/h   an indigo deck with a bullet nose, glowing cyan wheels and edge, an underglow strip, a booster

Every board is one clean low-poly mesh (SB_<Model>, a few material slots, no modifiers), origin at the centre of the deck
on top of the grip, x along the board (nose +x), z up. The far model SB_<Model>_LOD1 is in the .blend, hidden. Boards are
true size (not scaled with the rider). The rider's legs are re-solved to stand on the deck (feet first, CHARACTER_MOTION
M5): the body itself is the option B person, unchanged.

Renders go to Art/StyleTests/skateboards_35/:
    sheet.png                      the three boards on turntables, labelled with top speeds
    manual.png, electric.png, neon.png   a close-up of each, upright and a second one upside down to show what is under it
    ride_manual.png                the Courier pushing the Manual board along with the right foot
    ride_electric.png, ride_neon.png     the Courier riding, sideways stance, knees a little bent
    carry.png                      the Courier three times, a board held in the right hand at the side
    neon_dusk.png                  the Neon Boost close-up again in the dusk recipe, where the glow matters most
Not approved: for the owner to choose from.
"""
import bpy, bmesh, math, random, os, time, traceback
from mathutils import Vector, Euler, Matrix

ROOT = os.environ.get("WASTED_ROOT", "C:/Projects/Wasted").replace("\\", "/").rstrip("/")
_H = ROOT + "/Tools/blender/" if os.path.exists(ROOT + "/Tools/blender/skates_34.py") else "C:/Projects/Wasted/Tools/blender/"
_s = open(_H + "skates_34.py", encoding="utf-8").read()
_s = _s[:_s.rstrip().rfind("\nmain()")]
exec(compile(_s, "skates_34_helpers", "exec"), globals())   # sq(), rbox(), rcyl(), ring(), Skate, PAL, GLOW, text(), dusk()

NAME = "skateboards_35"
OUT = ROOT + "/Art/StyleTests"
SRC = ROOT + "/Art/Source/StyleTests"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"
ONLY = tuple(os.environ["WASTED_ONLY"].split(",")) if os.environ.get("WASTED_ONLY") else None
SAMPLES = int(os.environ.get("WASTED_SAMPLES", 96))
SCALE = int(os.environ.get("WASTED_SCALE", 100))

AXLE_Y = (math.pi / 2, 0.0, 0.0)       # turns a part built along z so it runs along y (a wheel axle across the board)
LEGK, T_ = CHAR_STYLES["B"]["leg"], CHAR_STYLES["B"]["thick"]

# The three boards. Lengths in metres; x along the board, nose at +x. 'top' is the grip at the deck's centre.
SPEC = dict(
    Manual=dict(L=0.80, W=0.210, T=0.022, nose=("round", 0.0), kick=(0.235, 0.060, 0.055), concave=0.006,
                wheel=(0.031, 0.036), hub=0.034, track=0.083, base=0.380, truck_gap=0.050,
                tags=dict(grip="brown", rim="cream", side="ochre", edge="ochre", under="ochre", bottom="cream",
                          stripe="terra"),
                truck=("alu", "iron", "terra"), tyre="ochre", hubm="alu"),
    Electric=dict(L=0.98, W=0.250, T=0.026, nose=("round", 0.0), kick=(0.330, 0.022, 0.034), concave=0.005,
                  wheel=(0.045, 0.050), hub=0.040, track=0.098, base=0.660, truck_gap=0.052,
                  tags=dict(grip="charcoal", rim="teal", side="teal", edge="teal", under="teal", bottom="teal",
                            stripe="ochre"),
                  truck=("alu", "iron", "ochre"), tyre="charcoal", hubm="alu"),
    NeonBoost=dict(L=0.96, W=0.240, T=0.024, nose=("bullet", 0.48), kick=(0.300, 0.030, 0.040), concave=0.005,
                   wheel=(0.050, 0.044), hub=0.060, track=0.096, base=0.620, truck_gap=0.058,
                   tags=dict(grip="charcoal", rim="indigo", side="indigo", edge="neon", under="indigo",
                             bottom="indigo", stripe="plum"),
                   truck=("charcoal", "charcoal", "plum"), tyre="neon", hubm="indigo"),
)
for _sp in SPEC.values():
    _sp["top"] = _sp["wheel"][0] + _sp["truck_gap"] + _sp["T"]

MODELS = (("Manual", "50 km/h"), ("Electric", "100 km/h"), ("NeonBoost", "250 km/h"))
LABEL = dict(Manual="Manual", Electric="Electric", NeonBoost="Neon Boost")

DESIGN = """
== What was built ==
Manual (50 km/h): a classic wooden street skateboard, the plainest and the most old-town. A popsicle deck with round
  nose and tail kicked up, a little concave, a warm brown grip on top inside a narrow cream border, ochre ply edges,
  a cream underside with one terracotta stripe down the middle. Pale grey trucks on dark iron baseplates with
  terracotta bushings, four fat ochre wheels with pale hubs. ride_manual.png: the rider pushes it along, the left
  (front) foot on the deck just behind the front truck, pointing forward (the owner's choice), the right foot down behind pushing.
Electric (100 km/h): a longer, wider cruiser deck in teal with a charcoal grip and teal border, a gentle kick at the
  tail only. Under the deck between the trucks a charcoal battery box with an ochre band and a small green status
  light (emissive) on its front corner. Bigger charcoal wheels; the two rear wheels are hub motors, ochre drums with
  cream caps that stand out on both sides of the tyre (as on the electric skates). Grey trucks with ochre bushings.
  The rider stands still on it: sideways stance, both feet across the deck.
Neon Boost (250 km/h): the most striking. An indigo deck with a bullet-shaped nose that narrows to the front, a
  charcoal grip in an indigo border, and the lower half of the deck edge glowing neon cyan all the way round. Four
  large wheels whose tyres glow cyan on indigo hubs, an underglow strip under the deck that lights the ground, charcoal
  trucks with plum bushings. Under the tail, between the rear wheels, a booster nozzle: a dark iron can with a plum lip
  and a Lantern-glow core, aimed back and a little down. Glow is plain emission (no bloom or post-process, W8);
  neon_dusk.png shows it at dusk. The rider stands still on it, as on the Electric.

Every board is one mesh with a few material slots and no modifiers: SB_Manual, SB_Electric, SB_NeonBoost, origin at
the centre of the deck on top of the grip (x along the board, nose +x, z up), so the rider's feet stand at z = 0 and
the board sits on the ground when its origin is at 'deck top' above it. The far model SB_<Model>_LOD1 (same parts,
about half the segments) is in the .blend, hidden. The deck is a loft of soft rounded sections (no thin planes, W3):
thicker than a real deck (2.2 to 2.6 cm against about 1.2 cm) so it reads chunky. Boards are true size and are not
scaled with the rider; the Courier (1.78 m) rides them. Riders' legs are solved to the deck (feet first, M5): the hips
drop a little and the knees bend; the rest of the body is the option B person unchanged, so arms hang relaxed and the
head faces the camera. In the game a board would ride on root (it is not on a bone) while ridden and on hand_r when
carried (carry.png: held by the middle of its edge, deck upright, wheels out).
"""

CHOICES = """
== Choices for the owner ==
1. Push pose: decided by the owner, 2026-11-24, "Foot forward to push": while pushing, the front foot points forward
   along the board (8 degrees off); it turns across only in the riding stance.
2. Stance: regular (left foot to the nose). Goofy is a mirror; it could be a player setting.
3. Deck shapes: Manual a popsicle (both ends kicked, for tricks), Electric a cruiser (tail kick only, longer, wider),
   Neon Boost a bullet nose (the only board with a pointed end) so it reads fastest by shape as well as glow.
4. New colours outside the palette, as for the skates: neon cyan (0.02, 0.55, 1.0) and status green (0.30, 1.0, 0.45);
   the booster core is the Lantern colour. In daylight the cyan reads pale ice-blue (AgX); at dusk it is strong.
5. Deck thickness: chunky 2.2 to 2.6 cm (W3) against 1.2 cm on a real deck. Thinner would look truer but turn to a
   thin plane at street distance.
6. The rider's height: standing on a board raises the feet 10 to 13 cm (deck top). The slice would see that, as it
   did with the skates' 9 to 12 cm.
7. Not decided here: how the wheels, motors and booster animate (wheel spin, a booster flicker, the status light for
   battery level), how the push cycle and getting on and off look, sounds, and colourways (materials are slots).
"""


# ---- the deck: a loft of soft rounded sections along x -------------------------------------------------------------
def deck_stations(sp):
    """[(x, half width, kick lift)] from tail tip to nose tip."""
    L, W = sp["L"], sp["W"]
    half, R = L / 2, W / 2
    xk, rise_n, rise_t = sp["kick"]
    shape, taper_from = sp["nose"]
    near = DETAIL >= 1.0
    out = []
    for s in (-1, 1):
        rise = rise_n if s > 0 else rise_t
        xr = half - R
        xs = [0.0, xk * 0.5, xk] + [xk + (xr - xk) * f for f in ((0.33, 0.66) if near else (0.5,))] + [xr]
        rows = [(x, R) for x in xs]
        for a in ((30, 55, 72, 84) if near else (40, 75)):
            t = math.radians(a)
            rows.append((xr + R * math.sin(t), R * math.cos(t)))
        for x, w in rows:
            if s < 0 and x == 0.0:
                continue
            if s > 0 and shape == "bullet" and x > taper_from * 0.25:   # the nose narrows to a bullet
                k = min(1.0, (x - taper_from * 0.25) / (half - taper_from * 0.25))
                w *= 1.0 - 0.42 * k ** 1.4
            z = rise * ((x - xk) / (half - xk)) ** 1.5 if x > xk else 0.0
            out.append((s * x, w, z))
    out.sort()
    return out


def deck_ring(w, h, rc, b, cv):
    """One section: (y, z, tag of the face that follows) counter-clockwise from the top centre, seen from +x."""
    rc = min(rc, 0.5 * w)
    yc = w - rc
    yb = yc - b if yc - b > 0.45 * yc else 0.6 * yc
    side = [(yb, h, "rim"), (yc, h, "rim"), (yc + rc * 0.7071, h * 0.7071, "side"), (w, 0.0, "edge"),
            (yc + rc * 0.7071, -h * 0.7071, "under"), (yc, -h, "bottom"), (yb * 0.35, -h, "stripe")]
    pts = [(0.0, h)] + [(y, z) for (y, z, _) in side] + [(-y, z) for (y, z, _) in reversed(side)]
    # face j runs from point j to point j + 1; a face of the mirrored half takes the tag of its mirror
    tags = ["grip"] + [t for (_, _, t) in side] + [side[i - 1][2] for i in range(len(side) - 1, 0, -1)] + ["grip"]
    return [(y, z + cv * (y / w) ** 2) for (y, z) in pts], tags


def deck(sk, sp):
    """Adds the deck to the board being built (ground frame: wheels on z = 0)."""
    T = sp["T"]
    top = sp["top"]
    bm = sk.bm                                                     # straight into the board, so borders stay smooth
    rows = []
    tags = None
    for x, w, z in deck_stations(sp):
        pts, tags = deck_ring(w, T / 2, T / 2, 0.012, sp["concave"])
        zc = top - T / 2 + z
        rows.append([bm.verts.new((x, y, zc + zz)) for (y, zz) in pts])
    m = len(rows[0])

    def slot(tag):
        key = sp["tags"][tag]
        if key not in sk.slots:
            sk.slots.append(key)
        return sk.slots.index(key)

    def face(vs, tag):
        f = bm.faces.new(vs)
        f.material_index = slot(tag)
        f.smooth = True

    for i in range(len(rows) - 1):
        a, b_ = rows[i], rows[i + 1]
        for j in range(m):
            k = (j + 1) % m
            face((a[j], a[k], b_[k], b_[j]), tags[j])
    for row, rev in ((rows[0], False), (rows[-1], True)):          # the two tips: a fan round a centre point
        cv = bm.verts.new(sum((v.co for v in row), Vector()) / m)
        for j in range(m):
            k = (j + 1) % m
            face((cv, row[j], row[k]) if not rev else (cv, row[k], row[j]), tags[j])


def deck_bottom_at(sp, x):
    """Height of the deck's underside at x (ground frame)."""
    xk, rise_n, rise_t = sp["kick"]
    half = sp["L"] / 2
    ax = abs(x)
    rise = rise_n if x > 0 else rise_t
    z = rise * ((ax - xk) / (half - xk)) ** 1.5 if ax > xk else 0.0
    return sp["top"] - sp["T"] + z


def board_wheel(sk, x, y, r, w, tyre, hub, hub_d, u=12):
    sk.add(rcyl(2 * r, w, u, 4, 5.0), tyre, (x, y, r), AXLE_Y)
    sk.add(rcyl(hub_d, w + 0.004, 8, 3, 4.0), hub, (x, y, r), AXLE_Y)


def truck(sk, sp, x):
    r, w = sp["wheel"]
    hanger, plate, bush = sp["truck"]
    db = deck_bottom_at(sp, x)
    hz = r + 0.007                                                       # hanger centre, just above the axle
    sk.add(rbox((0.042, 2 * (sp["track"] - w / 2) + 0.004, 0.030), 12, 4, 3.0), hanger, (x, 0, hz))
    sk.add(rbox((0.082, 0.072, 0.014), 8, 4, 3.0), plate, (x, 0, db - 0.007))
    gap = (db - 0.014) - (hz + 0.015)
    sk.add(rcyl(0.034, gap + 0.010, 10, 4, 4.0), bush, (x, 0, hz + 0.015 + gap / 2))


def running_gear(sk, sp, motors=False):
    r, w = sp["wheel"]
    for s in (-1, 1):
        x = s * sp["base"] / 2
        truck(sk, sp, x)
        for y in (-sp["track"], sp["track"]):
            if motors and s < 0:                                         # hub motor: an ochre drum through the tyre
                sk.add(rcyl(2 * r, w, 12, 4, 5.0), sp["tyre"], (x, y, r), AXLE_Y)
                sk.add(rcyl(0.064, w + 0.014, 12, 4, 5.0), "ochre", (x, y, r), AXLE_Y)
                sk.add(rcyl(0.028, w + 0.020, 8, 3, 4.0), "cream", (x, y, r), AXLE_Y)
            else:
                board_wheel(sk, x, y, r, w, sp["tyre"], sp["hubm"], sp["hub"])


# ---- 1. Manual -----------------------------------------------------------------------------------------------------
def manual(sk, sp):
    deck(sk, sp)
    running_gear(sk, sp)


# ---- 2. Electric ---------------------------------------------------------------------------------------------------
def electric(sk, sp):
    deck(sk, sp)
    running_gear(sk, sp, motors=True)
    db = deck_bottom_at(sp, 0.0)
    sk.add(rbox((0.420, 0.170, 0.042), 12, 5, 4.0), "charcoal", (0.0, 0, db - 0.020))         # battery box
    sk.add(rbox((0.052, 0.176, 0.046), 8, 3, 4.0), "ochre", (-0.06, 0, db - 0.021))           # its band
    sk.add(sq((0.028, 0.012, 0.016), 8, 4, 2.0, 2.0), "status", (0.165, -0.085, db - 0.019))    # status light


# ---- 3. Neon Boost -------------------------------------------------------------------------------------------------
def neon(sk, sp):
    deck(sk, sp)
    running_gear(sk, sp)
    db = deck_bottom_at(sp, 0.0)
    sk.add(rbox((0.500, 0.026, 0.008), 10, 3, 6.0), "neon", (0.02, 0, db - 0.003))           # underglow strip
    xb, zb = -0.415, 0.068
    sk.add(rbox((0.080, 0.044, deck_bottom_at(sp, xb) - zb), 8, 4, 3.0), "charcoal",
           (xb + 0.01, 0, (deck_bottom_at(sp, xb) + zb) / 2 + 0.004))                          # booster mount
    aim = (0.0, -(math.pi / 2 + 0.14), 0.0)                                                   # back and a little down
    d = Vector((-math.cos(0.14), 0.0, -math.sin(0.14)))
    sk.add(rcyl(0.074, 0.120, 12, 4, 5.0), "iron", (xb, 0, zb), aim)                          # booster can
    mouth = Vector((xb, 0, zb)) + d * 0.060
    sk.add(ring(0.039, 0.039, 0.010, 12, 4), "plum", tuple(mouth), aim)                       # flared lip
    sk.add(rcyl(0.066, 0.008, 12, 3, 2.5), "flame", tuple(mouth - d * 0.002), aim)            # the glow inside


MAKERS = dict(Manual=manual, Electric=electric, NeonBoost=neon)


class Board(Skate):
    def finish_board(self, top):
        """Moves the mesh so its origin is the deck's centre on top of the grip."""
        bmesh.ops.translate(self.bm, vec=(0.0, 0.0, -top), verts=self.bm.verts)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        me = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(me)
        self.bm.free()
        for key in self.slots:
            me.materials.append(M[key])
        ob = bpy.data.objects.new(self.name, me)
        COL.objects.link(ob)
        ob["deck_top"] = top
        ob["ride_on"] = "root"
        ob["carry_on"] = "hand_r"
        return ob


def build_board(model, suffix=""):
    sp = SPEC[model]
    b = Board("SB_%s%s" % (model, suffix))
    MAKERS[model](b, sp)
    return b.finish_board(sp["top"])


def build_board_lod(model):
    global DETAIL
    DETAIL = LOD_DETAIL
    try:
        ob = build_board(model, "_LOD1")
    finally:
        DETAIL = 1.0
    ob.hide_render = True
    setp(ob, "hide_viewport", True)
    return ob


def board_mats():
    for key in PAL:
        M[key] = flat("M_Board_" + key, PAL[key][0], rough=0.9, bump=0.06, noise_scale=16.0)
    M["charcoal"] = flat("M_Board_charcoal", PAL["charcoal"][0], rough=0.85, bump=0.06, noise_scale=16.0)
    for key, (c, s, _) in GLOW.items():
        M[key] = emissive("M_Board_" + key, c, s)


def dup(src, name, parent=None, loc=(0, 0, 0), rot=(0, 0, 0), matrix=None):
    ob = bpy.data.objects.new(name, src.data)
    COL.objects.link(ob)
    if parent is not None:
        ob.parent = parent
    if matrix is not None:
        ob.matrix_basis = matrix
    else:
        ob.location = loc
        ob.rotation_euler = rot
    return ob


# ---- the rider: the option B person with the legs solved to the deck --------------------------------------------------
def courier(tag, oz=0.0):
    before = set(COL.objects)
    sp = with_name("Courier", tag)
    person(sp, 0.0, 0.0, oz)
    return sp, [o for o in COL.objects if o not in before]


def solve_legs(sp, objs, oz, feet):
    """feet: {sg: (ankle point, shoe yaw, knee direction)} in the person's own frame (faces -y, left +x).
    Replaces the straight legs and the shoes with a thigh, a calf and a shoe per side reaching those points."""
    n = sp["name"]
    S = sp.get("height", 1.72) / 1.72
    mats = {}
    for o in list(objs):
        for part in ("_Shoe", "_Leg", "_Short"):
            if o.name.startswith(n + part):
                mats[part] = o.data.materials[0]
                objs.remove(o)
                bpy.data.objects.remove(o, do_unlink=True)
                break
    Lt = (0.90 - 0.07) * LEGK * S
    shorts = "_Short" in mats
    r_a, r_h = (0.045, 0.080) if shorts or "_Leg" not in mats else (0.058, 0.088)
    r_a, r_h = r_a * T_ * S, r_h * T_ * S
    r_k = 0.5 * (r_a + r_h)
    reach = {}
    for sg, (A, yaw, hint) in feet.items():
        A = Vector(A)
        hip = Vector((sg * 0.085 * S, 0.0, oz + 0.90 * LEGK * S))
        d = A - hip
        dl = d.length
        reach[sg] = dl / Lt
        if dl >= Lt * 0.999:
            A = hip + d.normalized() * Lt * 0.999
            d = A - hip
            dl = d.length
        dn = d.normalized()
        h = Vector(hint) - dn * Vector(hint).dot(dn)
        knee = hip + d * 0.5 + h.normalized() * math.sqrt(max(0.0, (Lt / 2) ** 2 - (dl / 2) ** 2))
        objs.append(limb(n + "_Calf", A, knee, r_a, r_k, mats["_Leg"]))
        objs.append(limb(n + "_Thigh", knee, hip, r_k, r_h, mats["_Leg"]))
        if shorts:
            objs.append(limb(n + "_Short", hip + (knee - hip) * 0.82, Vector((hip.x, 0.0, oz + 0.92 * LEGK * S)),
                             0.086 * T_ * S, 0.092 * T_ * S, mats["_Short"]))
        c = A + Matrix.Rotation(yaw, 3, 'Z') @ Vector((0.0, -0.04 * S, 0.0)) + Vector((0, 0, (0.045 * T_ - 0.07 * LEGK) * S))
        objs.append(ellipsoid(n + "_Shoe", c, (0.05 * T_ * S, 0.12 * T_ * S, 0.045 * T_ * S), mats["_Shoe"], (0, 0, yaw)))
    return reach


def group(name, objs, loc, yaw):
    emp = bpy.data.objects.new(name, None)
    COL.objects.link(emp)
    for o in objs:
        o.parent = emp
    emp.location = loc
    emp.rotation_euler = (0, 0, yaw)
    return emp


def ankle_h(S):
    return 0.07 * LEGK * S


def rider_push(src, X, yaw):
    """The Manual board pushed along: in the person's frame the board runs along -y (the way they face)."""
    sp_ = SPEC["Manual"]
    S = CAST["Courier"].get("height", 1.72) / 1.72
    bx, by = 0.075, 0.13                                    # the board's centre in the person's frame
    top = sp_["top"]
    front_y = by - sp_["base"] / 2 + 0.035                  # ankle of the front foot, just behind the front truck
    back = Vector((-0.125, 0.25, ankle_h(S)))
    Lt = (0.90 - 0.07) * LEGK * S
    hipb = Vector((-0.085 * S, 0.0, 0.0))
    dxy = math.hypot(back.x - hipb.x, back.y - hipb.y)
    oz = min(top - 0.08, math.sqrt((Lt * 0.995) ** 2 - dxy ** 2) + back.z - 0.90 * LEGK * S)
    sp, objs = courier("Push", oz)                           # the whole body lowered by oz, legs solved below
    yaw_front = math.radians(-8)                             # the owner, 2026-11-24: "Foot forward to push" (was -35, turned across)
    feet = {1: ((bx, front_y + 0.04 * S, top + ankle_h(S)), yaw_front, (0.25, -1.0, 0.0)),
            -1: (tuple(back), 0.0, (0.0, -1.0, 0.0))}
    reach = solve_legs(sp, objs, oz, feet)
    objs.append(dup(src, "SB_Manual_Ride", loc=(bx, by, top), rot=(0, 0, -math.pi / 2)))
    group("Ride_Manual", objs, (X, 0.0, 0.0), yaw)
    return oz, reach


def rider_stand(src, model, X, yaw, stance=0.205, bend=0.07):
    """Sideways stance: the board runs along the person's x (nose to their left), both feet across it."""
    sp_ = SPEC[model]
    top = sp_["top"]
    sp, objs = courier("Ride" + model, top - bend)
    S = sp["height"] / 1.72
    oz = top - bend
    feet = {1: ((stance, 0.04 * S, top + ankle_h(S)), math.radians(12), (0.35, -1.0, 0.0)),
            -1: ((-stance, 0.04 * S, top + ankle_h(S)), math.radians(-6), (-0.35, -1.0, 0.0))}
    reach = solve_legs(sp, objs, oz, feet)
    objs.append(dup(src, "SB_%s_Ride" % model, loc=(0.0, 0.0, top)))
    group("Ride_" + model, objs, (X, 0.0, 0.0), yaw)
    return oz, reach


def carrier(src, model, X):
    """Stands holding the board in the right hand by the middle of its edge: deck upright, grip to the leg."""
    sp_ = SPEC[model]
    sp, objs = courier("Carry" + model)
    hand = min((o for o in objs if o.name.startswith(sp["name"] + "_Hand")), key=lambda o: o.location.x)
    h = hand.location
    R = Matrix(((0, 0, 1), (1, 0, 0), (0, 1, 0)))           # board x -> +y, board y -> +z, board z -> +x
    loc = Vector((h.x + sp_["T"] / 2, h.y + 0.005, h.z + 0.014 - sp_["W"] / 2))
    objs.append(dup(src, "SB_%s_Carry" % model, matrix=Matrix.Translation(loc) @ R.to_4x4()))
    group("Carry_" + model, objs, (X, 0.0, 0.0), math.radians(50))     # turned so the board shows side on


# ---- the scene -----------------------------------------------------------------------------------------------------
def scene_all():
    random.seed(32)
    scene = new_scene("Skateboards32")
    global K_M
    K_M = kit_mats()
    M.update(K_M)
    board_mats()
    ink = flat("M_Ink", (0.08, 0.07, 0.07), noise_scale=4.0)
    cams, boards, facts = {}, {}, {}

    box("Ground", -6, 45, -8, 3, -0.3, 0, K_M["paving"], bevel=0)

    # sheet: the three boards on turntables before a plaster wall
    box("SheetWall", -2.7, 2.7, 0.95, 1.15, 0, 1.7, K_M["plaster"], bevel=0.02)
    box("SheetWallBase", -2.7, 2.7, 0.90, 1.15, 0, 0.18, K_M["stone_dark"], bevel=0.02)
    for i, (model, speed) in enumerate(MODELS):
        cx = (i - 1) * 1.3
        bm = rcyl(1.10, 0.07, 40, 6, 4.0)
        ob = add_obj("Turntable_%d" % i, bm, K_M["cream"], loc=(cx, 0, 0.035))
        for p in ob.data.polygons:
            p.use_smooth = True
        src = build_board(model)
        src.location = (cx, 0.0, 0.07 + SPEC[model]["top"])
        src.rotation_euler = (0, 0, math.radians(24))
        lod = build_board_lod(model)
        lod.location, lod.rotation_euler = src.location, src.rotation_euler
        boards[model] = src
        boards[model + "_LOD1"] = lod
        text("Label_%s" % model, LABEL[model], (cx, 0.949, 0.86), 0.13, ink)
        text("Speed_%s" % model, "top speed " + speed, (cx, 0.949, 0.70), 0.07, ink)
        facts[model] = cx
    cams["sheet"] = (camera(scene, "Cam_Sheet", (0.0, -3.9, 1.30), (0.0, 0.30, 0.42), 36.0), (1800, 1000))

    # close-ups: each board upright and a second one upside down behind it, before a wall
    CX = 9.0
    box("CloseWall", CX - 2.5, CX + 9.0, 1.4, 1.6, 0, 2.0, K_M["plaster"], bevel=0.02)
    box("CloseWallBase", CX - 2.5, CX + 9.0, 1.35, 1.6, 0, 0.18, K_M["stone_dark"], bevel=0.02)
    for i, (model, speed) in enumerate(MODELS):
        x0 = CX + i * 3.0
        sp = SPEC[model]
        src = boards[model]
        dup(src, "SB_%s_Close" % model, loc=(x0, -0.05, sp["top"]), rot=(0, 0, math.radians(-18)))
        lift = max(sp["kick"][1], sp["kick"][2]) + 0.002
        dup(src, "SB_%s_Under" % model, loc=(x0 + 0.08, 0.50, lift), rot=(math.pi, 0, math.radians(-8)))
        key = dict(Manual="manual", Electric="electric", NeonBoost="neon")[model]
        cams[key] = (camera(scene, "Cam_" + model, (x0 - 0.62, -1.25, 0.78), (x0 + 0.03, 0.22, 0.10), 40.0),
                     (1400, 1000))

    # riders
    RX = 20.0
    box("RideWall", RX - 2.0, RX + 8.0, 1.6, 1.9, 0, 3.2, K_M["stone"], bevel=0)
    box("RideWallUpper", RX - 2.0, RX + 8.0, 1.58, 1.9, 2.2, 3.2, K_M["plaster"], bevel=0)
    facts["push"] = rider_push(boards["Manual"], RX, math.radians(42))
    cams["ride_manual"] = (camera(scene, "Cam_RideManual", (RX - 1.1, -3.0, 1.30), (RX + 0.05, 0.0, 0.66), 45.0),
                           (1200, 1200))
    for j, model in enumerate(("Electric", "NeonBoost")):
        x = RX + 3.0 * (j + 1)
        facts["ride_" + model] = rider_stand(boards[model], model, x, math.radians(-14 if model == "Electric" else 16))
        key = "ride_" + dict(Electric="electric", NeonBoost="neon")[model]
        cams[key] = (camera(scene, "Cam_Ride" + model, (x + (0.85 if model == "Electric" else -1.0), -3.1, 1.15), (x, 0.0, 0.80), 43.0),
                     (1200, 1200))

    # carried: the Courier three times, each board in the right hand
    HX = 34.0
    box("CarryWall", HX - 3.2, HX + 3.2, 1.2, 1.5, 0, 3.2, K_M["stone"], bevel=0)
    box("CarryWallUpper", HX - 3.2, HX + 3.2, 1.18, 1.5, 2.2, 3.2, K_M["plaster"], bevel=0)
    for i, (model, speed) in enumerate(MODELS):
        x = HX + (i - 1) * 1.35
        carrier(boards[model], model, x)
        text("CarryLabel_%s" % model, LABEL[model], (x, 1.179, 2.36), 0.12, ink)
    cams["carry"] = (camera(scene, "Cam_Carry", (HX - 1.0, -5.8, 1.25), (HX - 0.1, 0.0, 0.92), 50.0), (1600, 1100))

    sun_and_sky(scene, (0.78, -0.40, 0.48), 8.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.35)   # the kit daylight, ADR-0007
    return scene, cams, boards, facts


def write_notes(boards, facts):
    note("\n== Models (one mesh per board, origin at the deck's centre on top of the grip; triangles after triangulation) ==")
    for model, speed in MODELS:
        ob, lod, sp = boards[model], boards[model + "_LOD1"], SPEC[model]
        d = ob.dimensions
        note("%-10s %-8s  %s %d tris (far model %s %d); materials %d; bounding box %.3f x %.3f x %.3f m (l x w x h)" % (
            LABEL[model], speed, ob.name, tris(ob), lod.name, tris(lod), len(ob.data.materials), d.x, d.y, d.z))
    note("\n== Dimensions for the browser slice (metres; x along the board, nose +x) ==")
    note("%-10s %6s %6s %6s %8s %9s %7s %7s %6s %6s" % ("board", "length", "width", "deck", "deck top", "wheelbase",
                                                       "wheel d", "wheel w", "track", "kicks"))
    for model, speed in MODELS:
        sp = SPEC[model]
        note("%-10s %6.3f %6.3f %6.3f %8.3f %9.3f %7.3f %7.3f %6.3f %s" % (
            LABEL[model], sp["L"], sp["W"], sp["T"], sp["top"], sp["base"], 2 * sp["wheel"][0], sp["wheel"][1],
            2 * sp["track"], "nose %.3f / tail %.3f" % (sp["kick"][1], sp["kick"][2])))
    note("length, width: the deck. deck: its thickness. deck top: grip above the ground at the centre (the origin).\n"
         "wheelbase: front to rear axle. track: left to right wheel centres. kicks: how far nose and tail rise above\n"
         "the flat middle; they start %s m from the centre." % ", ".join(
             "%s %.3f" % (LABEL[m], SPEC[m]["kick"][0]) for m, _ in MODELS))
    note("Neon Boost nose: a bullet, narrowing to 58 % of the width at the tip. Electric: battery box 0.420 x 0.170 x\n"
         "0.042 m under the deck. Neon Boost: booster can 0.074 m across, 0.120 m long, under the tail at x = -0.415.")
    oz, reach = facts["push"]
    note("Rider (Courier, 1.78 m): pushing, hips %.3f m lower than standing, front leg reach %.2f, back leg %.2f of full length."
         % (-oz, reach[1], reach[-1]))
    for model in ("Electric", "NeonBoost"):
        oz, reach = facts["ride_" + model]
        note("Rider on %s: hips %.3f m above standing (deck top less a %.2f m knee bend), legs at %.2f of full length."
             % (LABEL[model], oz, SPEC[model]["top"] - oz, reach[1]))
    note("For scale: a Soft box car in the slice is about 3200 triangles near (car_export_11); a skate of look test 34\n"
         "was 1460 to 1772.")
    note("\n== Colours (linear RGB) ==")
    for model, speed in MODELS:
        sp = SPEC[model]
        keys = list(dict.fromkeys(list(sp["tags"].values()) + list(sp["truck"]) + [sp["tyre"], sp["hubm"]]))
        if model == "Electric":
            keys += ["charcoal", "ochre", "cream", "status"]
        if model == "NeonBoost":
            keys += ["iron", "plum", "flame"]
        parts = []
        for k in dict.fromkeys(keys):
            if k in PAL:
                parts.append("%s %s" % (PAL[k][1], PAL[k][0]))
            else:
                c, s, nm = GLOW[k]
                parts.append("%s %s emissive %.1f" % (nm, c, s))
        note("%-10s %s" % (LABEL[model], "; ".join(parts)))
    note(DESIGN)
    note(CHOICES)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(SRC, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    try:
        t0 = time.time()
        scene, cams, boards, facts = scene_all()
        log("scene built: %d objects in %.1fs" % (len(scene.objects), time.time() - t0))
        write_notes(boards, facts)
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
