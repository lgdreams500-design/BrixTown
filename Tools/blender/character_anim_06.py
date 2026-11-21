"""Wasted - character animation test 06: a jointed body, walks with a "movement signature",
and talking while walking.

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/character_anim_06.py").read(), "character_anim_06.py", "exec"))

The body is the option B character of test 05 cut at the knees, elbows, waist and neck and hung on
named joints (Unreal mannequin names). All movement comes from motion_lib.py, which has no Blender
code in it: the same numbers are meant to drive the game later.
Renders two clips as numbered frames at 24 fps:
    <name>_walks/     four people, four movement signatures, on moving lanes (loops after 120 frames)
    <name>_walktalk/  two people walking side by side and talking
"""
import bpy, bmesh, math, random, os, time, traceback
from mathutils import Vector, Euler, Matrix, Quaternion

B = "C:/Projects/Wasted/Tools/blender/"
_p = open(B + "character_face_05.py", encoding="utf-8").read().split("\n# ---- scene with the line-up")[0]
exec(compile(_p, "character_face_05_helpers", "exec"), globals())
_ml = {}
exec(compile(open(B + "motion_lib.py", encoding="utf-8").read(), "motion_lib", "exec"), _ml)


class _NS:
    pass


M = _NS()
M.__dict__.update(_ml)

NAME = "character_anim_06"
LOG = OUT + "/" + NAME + "_log.txt"
ONLY = None                      # None renders both clips; e.g. ("walktalk",)
FRAMES = None                    # None renders every frame; e.g. dict(walks=[0, 8, 16], walktalk=[0, 40, 80]) for a check
FPS = 24
rad = math.radians

# ---- choreography (plain Python; no Blender below until "# ---- jointed body") --------------------
WALKERS = [("PlayerA", "Springy", 0.00, "Smile"), ("PlayerB", "Easy", 0.30, None),
           ("Courier", "Hurried", 0.55, "Focused"), ("Elder", "Careful", 0.15, "Gentle")]
WALK_FRAMES = 120
FACES = dict(Smile=dict(mouth="smile", squint=0.35, browL=(0.002, 0.003), browR=(0.002, 0.003)),
             Focused=dict(browL=(-0.004, 0.0), browR=(-0.004, 0.0), mouth=dict(w=0.0150, h=0.0009)),
             Gentle=dict(mouth=dict(w=0.0185, h=0.0011), curve=0.4, corner=0.002, squint=0.2))

TALK_FRAMES = 128
PAIR_SPEED = 1.25
# mouth shapes per tick (1 tick = 2 frames). Speaker starts at tick 6, the reply at tick 48.
SPEAKER_LINE = (6, ["OO", "EH", "EH", "EE", "EE", "EE", "EE", "OO", "OO", "AA", "EE",
                    "FV", "OO", "AA", "AA", "EE", "EH", "EE", "rest", "rest", "rest", "rest",
                    "EE", "AA", "AA", "EE", "EH", "EE", "EE", "MBP", "OO", "EE", "EE", "EE", "EH", "EH", "FV", "EE"])
REPLY_LINE = (48, ["EE", "AA", "AA", "EE", "EH", "EE", "EH", "MBP"])


def _steps(n, spans, on=1.0, off=0.0):
    return [on if any(a <= f < b for a, b in spans) else off for f in range(n)]


def _mouths(n, line, tail, tail_from):
    out = ["rest"] * n
    t0, shapes = line
    for i, sh in enumerate(shapes):
        for f in (2 * (t0 + i), 2 * (t0 + i) + 1):
            if f < n:
                out[f] = sh
    for f in range(tail_from, n):
        out[f] = tail
    return out


def talk_tracks(sk_s, sk_l, sep=0.80):
    """Per-frame layers for the speaker (on the listener's right) and the listener.
    Everything is in each character's own space: yaw + is towards their left."""
    n = TALK_FRAMES
    eye_s = sk_s["head"][2] + 0.14 * sk_s["_S"]
    eye_l = sk_l["head"][2] + 0.14 * sk_l["_S"]
    out = {}
    for who, side, dz, looks, seed in (
            ("speaker", 1.0, eye_l - eye_s, [(6, 34), (54, 116)], 0.7),
            ("listener", -1.0, eye_s - eye_l, [(20, 44), (70, 92), (112, n)], 2.9)):
        full_yaw, full_pitch = M.look_angles(side * sep, 0.0, dz)       # partner is straight to the side
        look = _steps(n, looks)
        head_yaw_t = [v * side * rad(44.0) for v in look]
        head_pitch_t = [v * full_pitch * 0.8 for v in look]
        chest_t = [v * side * rad(12.0) for v in look]
        hy = M.spring_track(head_yaw_t, 1.5, 0.72)
        hp = M.spring_track(head_pitch_t, 1.5, 0.8)
        cy = M.spring_track(chest_t, 0.9, 0.9)
        # eyes go first: they cover what the head has not turned yet
        want = [v * side * rad(76.0) for v in look]
        gx = [M.clamp((want[f] - hy[f] - cy[f]) / rad(34.0), -1.0, 1.0) * 0.0058 for f in range(n)]
        gz = [M.clamp(-(look[f] * full_pitch - hp[f]) / rad(30.0), -1.0, 1.0) * 0.0035 for f in range(n)]
        blinks = [max(0, a - 1) for a, b in looks] + [b - 1 for a, b in looks if b < n]
        out[who] = dict(hy=hy, hp=hp, hr=[0.10 * v for v in hy], cy=cy, gx=gx, gz=gz, lid=M.blink_track(n, blinks), seed=seed,
                        brow=[0.0] * n, squint=[0.0] * n, arm=[0.0] * n, elbow=[0.0] * n)
    s, l = out["speaker"], out["listener"]
    # speaker: a small nod on each stressed word, brows up on the question, a hand joins in
    nods = M.bumps(n, [(14, rad(3.0)), (26, rad(2.2)), (38, rad(3.4)), (58, rad(3.0)), (74, rad(3.6)), (80, rad(2.4)),
                       (118, rad(2.6)), (123, rad(2.0))])
    asking = M.spring_track(_steps(n, [(80, 112)]), 1.4, 0.8)
    amused = M.spring_track(_steps(n, [(113, n)]), 1.6, 0.8)
    for f in range(n):
        s["hp"][f] += nods[f] - rad(3.0) * asking[f]
        s["hr"][f] += rad(5.0) * asking[f]
    flash = M.bumps(n, [(14, 0.004), (38, 0.004), (74, 0.005)], width=0.14)
    s["brow"] = [flash[f] + 0.006 * asking[f] + 0.003 * amused[f] for f in range(n)]
    s["squint"] = [0.45 * amused[f] for f in range(n)]
    s["arm"] = M.spring_track(_steps(n, [(52, 94)]), 1.3, 0.85)
    beat = M.bumps(n, [(58, 10.0), (74, 12.0), (80, 8.0)], width=0.10)
    s["elbow"] = beat
    s["mouth"] = _mouths(n, SPEAKER_LINE, "smile", 114)
    s["wait"] = asking
    # listener: takes the news looking down the road, shakes the head on "Not a chip", then grins
    worry = M.spring_track(_steps(n, [(40, 96)]), 1.2, 0.9)
    shrug = M.spring_track(_steps(n, [(110, n)]), 1.6, 0.8)
    for f in range(n):
        l["hp"][f] += rad(5.0) * worry[f] * (1.0 - shrug[f])
        t = (f - 95) / FPS
        if 0.0 <= t <= 0.8:
            l["hy"][f] += rad(9.0) * math.sin(2.0 * math.pi * 2.5 * t) * math.sin(math.pi * t / 0.8)
    l["brow"] = [0.004 * worry[f] + 0.004 * shrug[f] for f in range(n)]
    l["worry"] = worry
    l["squint"] = [0.4 * shrug[f] for f in range(n)]
    l["mouth"] = _mouths(n, REPLY_LINE, "smile", 112)
    return out


# ---- jointed body ---------------------------------------------------------------------------------
def tube(name, p0, p1, r0, r1, mat, flat=1.0, cap=1.0, cuts=8):
    """Like limb(), but with rings along its length so a long garment can change shape below the hips."""
    p0 = Vector(p0)
    d = Vector(p1) - p0
    length = d.length
    bm = uvs(20, 14)
    for v in bm.verts:
        if v.co.z > 1e-4:
            v.co = Vector((v.co.x * r1, v.co.y * r1 * flat, v.co.z * r1 * cap + length))
        else:
            v.co = Vector((v.co.x * r0, v.co.y * r0 * flat, v.co.z * r0 * cap))
    wall = [e for e in bm.edges if abs(e.verts[0].co.z - e.verts[1].co.z) > length * 0.5]
    bmesh.ops.subdivide_edges(bm, edges=wall, cuts=cuts, use_grid_fill=True)
    return finish(name, bm, mat, p0, d.to_track_quat('Z', 'Y'))


def build_person(sp, ox=0.0, oy=0.0):
    """Option B person as rigid parts on named joints. Returns dict(J=joints, sk=rest skeleton, face=face rig)."""
    cs = CS
    n = sp["name"]
    sk = M.skeleton(sp, cs)
    S = sk["_S"]
    W = sp.get("build", 1.0) * cs["wide"]
    T = cs["thick"]
    legk, tork = cs["leg"], cs["torso"]
    base = Vector((ox, oy, 0.0))

    def zm(z):
        return z * legk if z <= 0.93 else 0.93 * legk + (z - 0.93) * tork

    def P(x, y, z):
        return Vector((ox + x * S, oy + y * S, zm(z) * S))

    J = {}

    def joint(name):
        e = bpy.data.objects.new(n + "_J_" + name, None)
        e.empty_display_size = 0.04
        e.rotation_mode = 'QUATERNION'
        COL.objects.link(e)
        rest = base + Vector(sk[name])
        par = M.PARENT.get(name)
        if par:
            e.parent = J[par]["ob"]
            e.location = rest - J[par]["rest"]
        else:
            e.location = rest
        J[name] = dict(ob=e, rest=rest, loc=Vector(e.location))

    joint("root")
    for name in M.ORDER:
        joint(name)

    def attach(ob, jn):
        ob.parent = J[jn]["ob"]
        ob.matrix_parent_inverse = Matrix.Translation(-J[jn]["rest"])
        return ob

    skin = solid(sp["skin"], 0.5)
    top = cloth(sp["top"])
    bot = cloth(sp["bottom"])
    coat = cloth(sp["coat"]) if "coat" in sp else None
    shoe = solid(sp.get("shoes", (0.10, 0.08, 0.07)), 0.6)
    garment = sp.get("garment", "tee")
    kind = "bare" if garment == "dress" else sp.get("bottom_type", "trousers")
    sleeve_long = garment in ("long", "tunic") or coat is not None
    extras = sp.get("extras", ())
    chest = 0.172 * W
    waist = 0.138 * W
    lx = 0.085

    # legs: shin and thigh meet in a ball at the knee
    leg_r = None
    for tag, sg in (("l", 1), ("r", -1)):
        hip, knee, ank = P(sg * lx, 0, 0.90), P(sg * lx, 0, 0.49), P(sg * lx, 0, 0.07)
        attach(ellipsoid(n + "_Shoe", P(sg * lx, -0.04, 0.0) + Vector((0, 0, 0.045 * T * S)),
                         (0.05 * T * S, 0.12 * T * S, 0.045 * T * S), shoe), "foot_" + tag)
        r_a, r_h, lm = (0.058, 0.088, bot) if kind == "trousers" else (0.045, 0.080, skin)
        r_k = r_a + (r_h - r_a) * 0.506
        leg_r = ((0.094 if kind == "shorts" else r_h) * T * S, r_k * T * S, r_a * T * S)
        attach(limb(n + "_Shin", ank, knee, r_a * T * S, r_k * T * S, lm), "calf_" + tag)
        attach(limb(n + "_Thigh", knee, hip, r_k * T * S, r_h * T * S, lm), "thigh_" + tag)
        if kind == "shorts":
            attach(limb(n + "_Short", P(sg * lx, 0, 0.56), P(sg * lx, 0, 0.92), 0.086 * T * S, 0.092 * T * S, bot), "thigh_" + tag)
    if kind != "bare":
        # under a tunic the hips are a little shallower, so they stay inside it when the body turns
        hip_d = 0.108 * (0.84 if sp.get("garment", "tee") == "tunic" else 1.0)
        attach(ellipsoid(n + "_Pelvis", P(0, 0, 0.93), (0.16 * W * S, hip_d * T * S, 0.118 * tork * S), bot), "pelvis")

    # torso: one garment hung from the waist joint
    flat = 0.62 * (1 + 0.25 * (T - 1))
    front = -chest * flat
    if garment in ("tee", "long"):
        hem, r_hem = 1.02, waist
    elif garment == "tunic":
        hem, r_hem = 0.80, 0.172 * W
    else:
        hem = sp.get("dress_len", 0.45)
        r_hem = (0.17 + 0.12 * (1.0 - hem)) * W
    skirts = []
    z_hip = zm(0.95) * S

    def garment_part(ob, jn, z_hem, z_top, r0, r1, fl):
        """Long garments are remembered so the hem can swing out of the way of the legs."""
        attach(ob, jn)
        if z_hem < zm(0.90) * S:
            rest = [(v.co.y, min(1.0, max(0.0, (z_hip - (z_hem + v.co.z)) / (z_hip - z_hem)))) for v in ob.data.vertices]
            skirts.append(dict(ob=ob, rest=rest, joint=jn, z_hem=z_hem, length=z_top - z_hem, r0=r0, r1=r1, flat=fl))

    garment_part(tube(n + "_Body", P(0, 0, hem), P(0, 0, 1.36), r_hem * S, chest * S, top, flat=flat, cap=0.28),
                 "spine", zm(hem) * S, zm(1.36) * S, r_hem * S, chest * S, flat)
    outer = coat or top
    if coat is not None:
        ch = sp.get("coat_len", 0.6)
        r_c = max(0.20 * W, r_hem + 0.02) if garment == "dress" else 0.20 * W
        garment_part(tube(n + "_Coat", P(0, 0.006, ch), P(0, 0.006, 1.37), r_c * S, (chest + 0.014) * S, coat, flat=flat * 1.06, cap=0.25),
                     "spine", zm(ch) * S, zm(1.37) * S, r_c * S, (chest + 0.014) * S, flat * 1.06)
        front = -(chest + 0.014) * flat * 1.06
        attach(ellipsoid(n + "_Collar", P(0, 0.0, 1.425), (0.085 * T * S, 0.08 * T * S, 0.035 * S), coat), "spine")
    sh = chest + 0.016
    attach(limb(n + "_Yoke", P(-sh, 0, 1.375), P(sh, 0, 1.375), 0.052 * T * S, 0.052 * T * S, outer), "spine")
    for tag, sg in (("l", 1), ("r", -1)):
        shoulder = P(sg * sh, 0, 1.375)
        wrist = P(sg * (sh + 0.055), -0.02, 0.87)
        elbow = shoulder.lerp(wrist, 0.48)
        am = outer if sleeve_long else skin
        r_s, r_w = 0.052 * T * S, 0.035 * T * S
        r_e = r_s + (r_w - r_s) * 0.48
        attach(limb(n + "_UpperArm", shoulder, elbow, r_s, r_e, am), "upperarm_" + tag)
        attach(limb(n + "_Forearm", elbow, wrist, r_e, r_w, am), "lowerarm_" + tag)
        if not sleeve_long:
            attach(limb(n + "_Sleeve", shoulder, shoulder.lerp(wrist, 0.42), 0.057 * T * S, 0.052 * T * S, top), "upperarm_" + tag)
        attach(ellipsoid(n + "_Hand", wrist + Vector((sg * 0.003, -0.008, -0.062 * tork)) * S,
                         (0.03 * T * S, 0.042 * T * S, 0.055 * T * S), skin), "lowerarm_" + tag)
    if "scarf" in sp:
        sc = cloth(sp["scarf"])
        attach(ellipsoid(n + "_Scarf", P(0, 0.0, 1.44), (0.088 * T * S, 0.084 * T * S, 0.04 * S), sc), "spine")
        attach(limb(n + "_ScarfTail", P(0.045, front - 0.012, 1.14), P(0.04, front - 0.006, 1.42), 0.034 * S, 0.03 * S, sc, flat=0.35, cap=0.4), "spine")
    if "apron" in extras:
        attach(limb(n + "_Apron", P(0, front * 0.82, 0.66), P(0, front * 0.86, 1.22), 0.118 * W * S, 0.10 * W * S,
                    cloth(sp.get("apron_color", TERRA)), flat=0.26, cap=0.2), "spine")
    if "bag" in extras:
        strap = solid((0.13, 0.09, 0.06), 0.7)
        attach(limb(n + "_Strap", P(-0.15, front - 0.006, 1.40), P(0.18, front + 0.006, 1.0), 0.012 * S, 0.012 * S, strap), "spine")
        attach(ellipsoid(n + "_Bag", P(0.2, -0.105 * T, 0.93), (0.05 * S, 0.10 * S, 0.09 * S), strap), "spine")
    attach(limb(n + "_Neck", P(0, 0.005, 1.39), P(0, 0, 1.50), 0.046 * T * S, 0.044 * T * S, skin), "neck")

    # head and face from test 05, built at natural size and scaled with the body by the parent link
    face = person(dict(sp), ox, oy, 0.0, mode="face")
    inv = Matrix.Translation(-J["head"]["rest"]) @ Matrix.Translation(base) @ Matrix.Scale(S, 4) @ Matrix.Translation(-base)
    for ob in face["head"]:
        ob.parent = J["head"]["ob"]
        ob.matrix_parent_inverse = inv
    return dict(J=J, sk=sk, face=face, name=n, skirts=skirts, leg_r=leg_r)


def swing_hems(ch, pz):
    """Skirts, coats and tunics are single forms: widen the hem, front or back, just enough to clear the legs."""
    if not ch["skirts"]:
        return
    need = [M.hem_scales(ch["sk"], pz, g, ch["leg_r"]) for g in ch["skirts"]]
    sf, sb = max(a for a, b in need), max(b for a, b in need)       # layers move together so they never cross
    # cloth is pushed out at once by a knee but falls back slowly
    sf = ch["hem_f"] = max(sf, ch.get("hem_f", 1.0) * 0.8 + sf * 0.2)
    sb = ch["hem_b"] = max(sb, ch.get("hem_b", 1.0) * 0.8 + sb * 0.2)
    for g in ch["skirts"]:
        me = g["ob"].data
        for v, (y0, w) in zip(me.vertices, g["rest"]):
            v.co.y = y0 * (1.0 + ((sf if y0 < 0 else sb) - 1.0) * w)
        me.update()


def apply_pose(ch, pz, pos, yaw):
    swing_hems(ch, pz)
    J = ch["J"]
    for name, R in pz["rot"].items():
        J[name]["ob"].rotation_quaternion = Matrix(R).to_quaternion()
    J["pelvis"]["ob"].location = J["pelvis"]["loc"] + Vector(pz["pelvis_loc"])
    J["root"]["ob"].location = Vector(pos)
    J["root"]["ob"].rotation_quaternion = Quaternion((0.0, 0.0, 1.0), yaw)


def cast(who, suffix, without=(), **over):
    sp = dict(CAST[who])
    sp["name"] = who + "_" + suffix
    for key in without:
        sp.pop(key, None)
    sp.update(over)
    return sp


# The Elder's long coat over a dress needs real cloth or skirt joints, which this simple jointed body does
# not have (the hem can widen, but a knee still shows through). For the walk test she wears a tunic and trousers.
OUTFITS = dict(Elder=dict(without=("coat", "coat_len", "dress_len"), garment="tunic", top=PLUM, bottom=OLIVE))


def idle_blinks(n, seed, every=(52, 96)):
    rnd = random.Random(seed)
    starts, f = [], rnd.randint(6, 40)
    while f < n - 6:
        starts.append(f)
        f += rnd.randint(*every)
    return M.blink_track(n, starts)


# ---- clip 1: four movement signatures on moving lanes ------------------------------------------------
def walks():
    scene = new_scene("Anim06_Walks")
    m = mats()
    box("Ground_L", -30, -2.02, -30, 6, -0.3, -0.01, m["paving"], bevel=0)
    box("Ground_R", 2.02, 30, -30, 6, -0.3, -0.01, m["paving"], bevel=0)
    box("Wall", -12, 12, 5.0, 5.6, 0, 3.2, m["stone"])
    box("Wall_Upper", -12, 12, 5.06, 5.6, 3.2, 7, m["plaster"])
    box("Wall_Ledge", -12, 12, 4.88, 5.6, 3.2, 3.45, m["stone_dark"])
    lantern("Lamp_A", -2.6, 4.9, 2.8, m, 60)
    lantern("Lamp_B", 2.6, 4.9, 2.8, m, 60)
    people = []
    for i, (who, signame, offset, face) in enumerate(WALKERS):
        x = -1.5 + i * 1.0
        lane = box("Lane_%d" % i, x - 0.47, x + 0.47, -45, 45, -0.3, 0.0, m["paving"], bevel=0)
        box("Kerb_%d" % i, x + 0.47, x + 0.53, -14, 5, -0.3, 0.015, m["stone_dark"], bevel=0)
        ch = build_person(cast(who, "W", **OUTFITS.get(who, {})))
        sig = M.signature(signame)
        people.append(dict(ch=ch, sig=sig, x=x, offset=offset, lane=lane, lane_y=lane.location.y,
                           speed=M.speed_of(sig, ch["sk"]), face=FACES.get(face, {}), lid=idle_blinks(WALK_FRAMES, 11 + i)))
        log("  %s: %s, %.2f m/s, %d steps a minute" % (who, signame, M.speed_of(sig, ch["sk"]), round(2 * 60 * FPS / sig["cycle"])))
    box("Kerb_first", -2.03, -1.97, -14, 5, -0.3, 0.015, m["stone_dark"], bevel=0)
    camera(scene, "Cam_Walks", (3.1, -7.4, 1.75), (0.0, 0.0, 0.95), 58.0)
    sun_and_sky(scene, (-0.45, -0.50, 0.74), 3.2, (1.0, 0.82, 0.62), (0.42, 0.55, 0.82), 0.25, angle=6.0)

    def set_frame(f):
        for p in people:
            ph = f / p["sig"]["cycle"] + p["offset"]
            apply_pose(p["ch"], M.walk(p["sig"], p["ch"]["sk"], ph), (p["x"], 0.0, 0.0), 0.0)
            p["lane"].location.y = p["lane_y"] + p["speed"] * f / FPS
            ps = dict(p["face"])
            ps["lid"] = p["lid"][f % WALK_FRAMES]
            pose(p["ch"]["face"], ps)

    return scene, set_frame, WALK_FRAMES


# ---- clip 2: walking and talking --------------------------------------------------------------------
def walktalk():
    scene = new_scene("Anim06_WalkTalk")
    m = mats()
    box("Ground", -24, 12, -9, 1.9, -0.3, 0, m["paving"], bevel=0)
    box("Wall", -24, 12, 1.3, 1.9, 0, 3.2, m["stone"])
    box("Wall_Upper", -24, 12, 1.36, 1.9, 3.2, 6.5, m["plaster"])
    box("Wall_Ledge", -24, 12, 1.18, 1.9, 3.2, 3.45, m["stone_dark"])
    shutters = (m["teal"], m["ochre"], m["wood_red"])
    for i, a in enumerate((-15.0, -10.6, -6.2, -1.8, 2.6, 7.0)):
        win("Win_%d" % i, 1.3, a, 1.15, 0.95, 1.35, m, axis="y", sign=-1, shutter=shutters[i % 3])
    for i, a in enumerate((-12.8, -4.0, 4.8)):
        box("Door_F_%d" % i, a - 0.62, a + 0.62, 1.20, 1.34, 0, 2.35, m["stone_dark"], bevel=0.03)
        box("Door_%d" % i, a - 0.48, a + 0.48, 1.16, 1.30, 0, 2.2, m["wood"], bevel=0.02)
    lantern("Lamp_1", -8.4, 1.2, 2.75, m, 70)
    lantern("Lamp_2", 0.4, 1.2, 2.75, m, 70)
    cyl("Barrel", -6.9, 0.98, 0, 0.27, 0.8, m["wood"])
    box("Crate", 0.9, 1.5, 0.72, 1.24, 0, 0.5, m["wood"], bevel=0.02)
    blob("Pot_Leaf", -2.9, 1.02, 0.78, 0.3, m["leaf"])
    cyl("Pot", -2.9, 1.02, 0, 0.2, 0.5, m["terra"], r_top=0.25)

    sp_s = build_person(cast("PlayerB", "T"))
    sp_l = build_person(cast("PlayerA", "T"))
    sig_s = M.signature("Easy", cycle=28, bounce=0.030, arm_out=3.0, head_follow=0.3, sway=0.024)
    sig_l = M.signature("Springy", cycle=24, bounce=0.028, arm_swing=20.0, head_bob=1.5)
    tr = talk_tracks(sp_s["sk"], sp_l["sk"])
    yaw = -math.pi / 2.0                       # both walk towards -X, along the wall
    y_s, y_l, x0 = 0.42, -0.38, 1.6
    cam = camera(scene, "Cam_Talk", (x0 - 3.0, -2.25, 1.50), (x0 - 0.15, 0.02, 1.24), 48.0)
    cam_x = cam.location.x
    sun_and_sky(scene, (-0.45, -0.50, 0.74), 3.2, (1.0, 0.82, 0.62), (0.42, 0.55, 0.82), 0.25, angle=6.0)

    def layer(t, f, arm_tag=None):
        wy, wp, wr = M.wander(f, t["seed"])
        mods = dict(chest_yaw=t["cy"][f], head=(t["hy"][f] + wy, t["hp"][f] + wp, t["hr"][f] + wr))
        if arm_tag and t["arm"][f] > 0.01:
            mods["arm_" + arm_tag] = (t["arm"][f], 30.0, 12.0, 78.0 + t["elbow"][f])
        return mods

    def set_frame(frame):
        x = x0 - PAIR_SPEED * frame / FPS
        f = min(max(frame, 0), TALK_FRAMES - 1)          # frames before 0 are only a run-up
        s, l = tr["speaker"], tr["listener"]
        apply_pose(sp_s, M.walk(sig_s, sp_s["sk"], frame / sig_s["cycle"], speed=PAIR_SPEED, mods=layer(s, f, "l")), (x, y_s, 0.0), yaw)
        apply_pose(sp_l, M.walk(sig_l, sp_l["sk"], frame / sig_l["cycle"] + 0.4, speed=PAIR_SPEED, mods=layer(l, f)), (x + 0.05, y_l, 0.0), yaw)
        b = s["brow"][f]
        ps = dict(mouth=s["mouth"][f], lid=s["lid"][f], gaze=(s["gx"][f], s["gz"][f]), browL=(b + 0.004, b + 0.001), browR=(b + 0.004, b + 0.001),
                  squint=s["squint"][f])
        if s["mouth"][f] == "rest" and s["wait"][f] > 0.5:
            ps["mouth"] = dict(w=0.0165, h=0.0030)          # lips parted, waiting for the answer
        pose(sp_s["face"], ps)
        b = l["brow"][f]
        wv = l["worry"][f] * (1.0 - l["squint"][f] / 0.4)
        ps = dict(mouth=l["mouth"][f], lid=l["lid"][f], gaze=(l["gx"][f], l["gz"][f] - 0.002 * wv),
                  browL=(b + 0.004 * wv, b - 0.003 * wv), browR=(b + 0.004 * wv, b - 0.003 * wv), squint=l["squint"][f])
        if l["mouth"][f] == "rest" and wv > 0.5:
            ps["mouth"] = dict(w=0.0150, h=0.0009)
            ps["curve"] = -0.25
        pose(sp_l["face"], ps)
        cam.location.x = cam_x - PAIR_SPEED * frame / FPS

    return scene, set_frame, TALK_FRAMES


def configure(scene, exposure, res, samples):
    r = scene.render
    setp(r, "engine", 'CYCLES')
    c = getattr(scene, "cycles", None)
    if c is not None:
        setp(c, "device", 'CPU')
        setp(c, "samples", samples)
        setp(c, "use_adaptive_sampling", True)
        setp(c, "adaptive_threshold", 0.04)
        setp(c, "use_denoising", True)
        setp(c, "max_bounces", 5)
        setp(c, "diffuse_bounces", 3)
    setp(r, "use_persistent_data", True)
    r.resolution_x, r.resolution_y = res
    r.resolution_percentage = 100
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
    for key, fn, res, samples in (("walks", walks, (800, 450), 32), ("walktalk", walktalk, (960, 540), 32)):
        if ONLY and key not in ONLY:
            continue
        try:
            random.seed(6)
            scene, set_frame, count = fn()
            configure(scene, -0.3, res, samples)
            folder = OUT + "/" + NAME + "_" + key
            os.makedirs(folder, exist_ok=True)
            todo = FRAMES.get(key, []) if FRAMES else range(count)
            log("%s built: %d objects, rendering %d frames" % (key, len(scene.objects), len(todo)))
            t = time.time()
            last = None
            for f in todo:
                for g in range(f - 20 if last is None or f != last + 1 else f, f):
                    set_frame(g)                              # run-up so cloth and springs have a history
                set_frame(f)
                last = f
                scene.render.filepath = folder + "/frame_%03d.png" % f
                bpy.ops.render.render(write_still=True, scene=scene.name)
                if f % 24 == 0:
                    log("  %s frame %d, %.0fs so far" % (key, f, time.time() - t))
            log("%s: %d frames rendered in %.1fs" % (key, len(todo), time.time() - t))
        except Exception:
            log("FAILED %s:\n%s" % (key, traceback.format_exc()))
    try:
        bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
        log("saved .blend")
    except Exception:
        log("save failed:\n" + traceback.format_exc())
    log("DONE")


main()
