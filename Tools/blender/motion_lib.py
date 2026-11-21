"""Wasted - motion library (pure Python, no Blender imports).

One walk for every body, shaped by a small set of numbers (a "movement signature"), plus the layers
that let a character talk and look around while walking. The same numbers are meant to drive the
game's animation system later, so nothing here depends on Blender.

Conventions: a character stands at the origin facing -Y, +Z is up, +X is the character's LEFT.
Rotations are 3x3 matrices (lists of rows). Angles in the signature are degrees, lengths metres.
Joint names follow the Unreal mannequin: pelvis, spine, neck, head, thigh_l/r, calf_l/r, foot_l/r,
upperarm_l/r, lowerarm_l/r.
"""
import math

FPS = 24


# ---- small matrix helpers -------------------------------------------------------------------
def Rx(a):
    c, s = math.cos(a), math.sin(a)
    return [[1, 0, 0], [0, c, -s], [0, s, c]]


def Ry(a):
    c, s = math.cos(a), math.sin(a)
    return [[c, 0, s], [0, 1, 0], [-s, 0, c]]


def Rz(a):
    c, s = math.cos(a), math.sin(a)
    return [[c, -s, 0], [s, c, 0], [0, 0, 1]]


IDENT = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]


def mul(*ms):
    out = ms[0]
    for b in ms[1:]:
        out = [[sum(out[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
    return out


def mulv(a, v):
    return tuple(a[i][0] * v[0] + a[i][1] * v[1] + a[i][2] * v[2] for i in range(3))


def tr(a):
    return [[a[j][i] for j in range(3)] for i in range(3)]


def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def scale(a, k):
    return (a[0] * k, a[1] * k, a[2] * k)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    return math.sqrt(dot(a, a))


def unit(a):
    n = norm(a)
    return scale(a, 1.0 / n) if n > 1e-9 else (0.0, 0.0, 0.0)


def clamp(x, lo, hi):
    return lo if x < lo else hi if x > hi else x


def smooth(x):
    x = clamp(x, 0.0, 1.0)
    return x * x * (3.0 - 2.0 * x)


def rad(d):
    return d * math.pi / 180.0


def rot_frac(R, f):
    """The rotation R scaled to a fraction f of its angle (used to share a turn between neck and head)."""
    c = clamp((R[0][0] + R[1][1] + R[2][2] - 1.0) / 2.0, -1.0, 1.0)
    ang = math.acos(c)
    if ang < 1e-6:
        return IDENT
    s = 2.0 * math.sin(ang)
    ax = unit(((R[2][1] - R[1][2]) / s, (R[0][2] - R[2][0]) / s, (R[1][0] - R[0][1]) / s))
    a = ang * f
    ca, sa = math.cos(a), math.sin(a)
    x, y, z = ax
    t = 1.0 - ca
    return [[ca + x * x * t, x * y * t - z * sa, x * z * t + y * sa],
            [y * x * t + z * sa, ca + y * y * t, y * z * t - x * sa],
            [z * x * t - y * sa, z * y * t + x * sa, ca + z * z * t]]


def head_matrix(yaw, pitch, roll):
    """Head direction in character space. yaw + = towards the character's left, pitch + = nod down,
    roll + = tilt towards the left shoulder. Radians."""
    return mul(Rz(yaw), Rx(pitch), Ry(roll))


# ---- skeleton ---------------------------------------------------------------------------------
def skeleton(sp, cs):
    """Rest joint positions relative to the feet, for a person described by the designer values `sp`
    in character style `cs` (see ART_DIRECTION.md). Matches the proportions of character_face_05."""
    S = sp.get("height", 1.72) / 1.72
    W = sp.get("build", 1.0) * cs["wide"]
    legk, tork = cs["leg"], cs["torso"]

    def zm(z):
        return z * legk if z <= 0.93 else 0.93 * legk + (z - 0.93) * tork

    def P(x, y, z):
        return (x * S, y * S, zm(z) * S)

    lx = 0.085
    sh = 0.172 * W + 0.016
    sk = dict(root=(0.0, 0.0, 0.0), pelvis=P(0, 0, 0.93), spine=P(0, 0, 1.03), neck=P(0, 0.005, 1.39), head=P(0, 0.004, 1.455))
    for tag, sg in (("l", 1), ("r", -1)):
        sk["thigh_" + tag] = P(sg * lx, 0, 0.90)
        sk["calf_" + tag] = P(sg * lx, 0, 0.49)
        sk["foot_" + tag] = P(sg * lx, 0, 0.07)
        shoulder = P(sg * sh, 0, 1.375)
        wrist = P(sg * (sh + 0.055), -0.02, 0.87)
        sk["upperarm_" + tag] = shoulder
        sk["lowerarm_" + tag] = tuple(shoulder[i] + (wrist[i] - shoulder[i]) * 0.48 for i in range(3))
        sk["hand_" + tag] = wrist
    sk["_S"] = S
    return sk


PARENT = dict(pelvis="root", spine="pelvis", neck="spine", head="neck",
              thigh_l="pelvis", calf_l="thigh_l", foot_l="calf_l", thigh_r="pelvis", calf_r="thigh_r", foot_r="calf_r",
              upperarm_l="spine", lowerarm_l="upperarm_l", hand_l="lowerarm_l",
              upperarm_r="spine", lowerarm_r="upperarm_r", hand_r="lowerarm_r")
ORDER = ["pelvis", "spine", "neck", "head", "thigh_l", "calf_l", "foot_l", "thigh_r", "calf_r", "foot_r",
         "upperarm_l", "lowerarm_l", "hand_l", "upperarm_r", "lowerarm_r", "hand_r"]


# ---- movement signatures ----------------------------------------------------------------------
# Every walk in the game is these numbers. A character has a base signature (chosen in the designer
# or given to an NPC), and game state (mood, needs, weather, load) nudges the numbers.
BASE = dict(
    cycle=24,          # frames per full cycle (two steps) at 24 fps. 24 = 120 steps a minute
    stride=0.84,       # step length as a fraction of leg length
    duty=0.62,         # part of the cycle each foot spends on the ground
    fore=0.42,         # how much of the planted foot's travel is in front of the hip (rest is behind)
    bounce=0.024,      # up-and-down of the hips (half range, m)
    sway=0.018,        # side-to-side of the hips (half range, m)
    hip_yaw=6.0,       # hips turning with the stepping leg
    hip_roll=3.5,      # hip dropping on the swinging side
    counter=1.0,       # how much the chest turns against the hips (1 = shoulders swing opposite)
    lean=3.0,          # forward lean of the chest
    width=0.75,        # how far apart the feet land, as a fraction of hip width
    toe_out=7.0,       # feet turned outwards
    lift=0.085,        # how high the foot is carried in the swing (m)
    strike=16.0,       # toes-up angle at heel strike
    toe_off=42.0,      # toes-down angle when the foot leaves
    arm_swing=18.0,    # arm swing either way
    arm_bias=2.0,      # arms carried forward (+) or back (-)
    arm_out=2.5,       # arms held away from the body
    elbow=16.0,        # resting elbow bend
    elbow_swing=14.0,  # extra bend as the arm comes forward
    arm_l=1.0,         # scale of the left / right arm swing (a hand on a bag strap, a carried load)
    arm_r=1.0,
    head_pitch=0.0,    # how the head is carried: + looks down, - chin up
    head_bob=1.2,      # nod with each step
    head_follow=0.25,  # how much of the body's turning the head follows (0 = perfectly steady)
    head_lag=0.06,     # the head trails the body by this part of a cycle
)

SIGNATURES = dict(
    Springy=dict(cycle=24, stride=0.80, bounce=0.034, sway=0.014, hip_yaw=7.0, hip_roll=4.5, lift=0.11, arm_swing=26.0,
                 elbow=22.0, elbow_swing=22.0, lean=1.5, head_pitch=-4.0, head_bob=2.2, toe_off=50.0, arm_out=4.5),
    Easy=dict(cycle=30, stride=0.86, bounce=0.022, sway=0.034, hip_yaw=9.0, hip_roll=5.5, counter=1.25, lift=0.07, arm_swing=24.0,
              arm_bias=-3.0, elbow=10.0, elbow_swing=8.0, lean=-1.5, head_pitch=-2.0, head_bob=1.0, toe_out=13.0, width=1.0,
              arm_out=4.5, head_follow=0.4),
    Hurried=dict(cycle=20, stride=0.86, duty=0.60, bounce=0.022, sway=0.010, hip_yaw=7.0, hip_roll=2.5, lift=0.075, arm_swing=22.0,
                 arm_bias=6.0, elbow=38.0, elbow_swing=18.0, lean=9.0, head_pitch=3.0, head_bob=0.8, toe_out=3.0, width=0.55,
                 arm_l=0.25, head_follow=0.15),
    Careful=dict(cycle=30, stride=0.50, duty=0.68, bounce=0.008, sway=0.026, hip_yaw=3.0, hip_roll=2.0, counter=0.6, lift=0.045,
                 arm_swing=7.0, arm_bias=4.0, elbow=28.0, elbow_swing=4.0, lean=5.0, head_pitch=8.0, head_bob=0.6, toe_out=10.0,
                 width=1.0, strike=7.0, toe_off=22.0, arm_out=3.0),
)

# State layers: multiply (x) or add (+) on top of any signature, scaled by how strong the state is.
STATES = dict(
    Tired=dict(cycle=("x", 1.18), stride=("x", 0.80), bounce=("x", 0.6), lift=("x", 0.65), arm_swing=("x", 0.55), lean=("+", 6.0),
               head_pitch=("+", 10.0), sway=("x", 1.3), toe_off=("x", 0.7)),
    Happy=dict(cycle=("x", 0.94), bounce=("x", 1.45), arm_swing=("x", 1.3), lift=("x", 1.2), head_pitch=("+", -5.0), lean=("+", -2.0)),
    Cold=dict(cycle=("x", 0.92), stride=("x", 0.88), arm_swing=("x", 0.3), elbow=("+", 45.0), arm_out=("+", -3.0), lean=("+", 5.0),
              head_pitch=("+", 6.0)),
)


def signature(name=None, states=None, **over):
    """Build a full signature: BASE, then a named signature, then state layers, then overrides."""
    sig = dict(BASE)
    if name:
        sig.update(SIGNATURES[name])
    for st, w in (states or {}).items():
        for key, (op, val) in STATES[st].items():
            if op == "x":
                sig[key] = sig[key] * (1.0 + (val - 1.0) * w)
            else:
                sig[key] = sig[key] + val * w
    sig.update(over)
    return sig


# ---- the walk -----------------------------------------------------------------------------------
def _geo(sk):
    L1 = norm(sub(sk["calf_l"], sk["thigh_l"]))
    L2 = norm(sub(sk["foot_l"], sk["calf_l"]))
    return dict(L1=L1, L2=L2, leg=L1 + L2, hip_x=sk["thigh_l"][0], hip_dz=sk["thigh_l"][2] - sk["pelvis"][2],
                pelvis_z=sk["pelvis"][2], ankle_z=sk["foot_l"][2], S=sk["_S"])


def speed_of(sig, sk):
    """Walking speed in m/s that this signature gives this body."""
    g = _geo(sk)
    return 2.0 * sig["stride"] * g["leg"] / (sig["cycle"] / FPS)


def _foot(sig, g, ph, side, total):
    """Ankle target (character space) and foot pitch (toes up +) for one leg at phase ph (0 = heel strike)."""
    D = sig["duty"]
    ball = 0.115 * g["S"]
    strike, toe = rad(sig["strike"]), rad(sig["toe_off"])
    x = side * g["hip_x"] * sig["width"]
    if ph < D:
        s = ph / D
        u = total * (sig["fore"] - s)
        if s < 0.16:
            pitch = strike * (1.0 - s / 0.16)
        elif s < 0.58:
            pitch = 0.0
        else:
            pitch = -toe * smooth((s - 0.58) / 0.42)
        z = g["ankle_z"] + (ball * math.sin(-pitch) if pitch < 0 else 0.03 * g["S"] * math.sin(pitch))
        u += ball * (1.0 - math.cos(pitch)) if pitch < 0 else 0.0
    else:
        s = (ph - D) / (1.0 - D)
        e = s - math.sin(2.0 * math.pi * s) / (2.0 * math.pi)
        u0 = -total * (1.0 - sig["fore"]) + ball * (1.0 - math.cos(toe))
        u = u0 + (total * sig["fore"] - u0) * e
        pitch = -toe + (toe + strike) * smooth(s * 1.15)
        z0 = ball * math.sin(toe)
        z = g["ankle_z"] + z0 * (1.0 - smooth(s * 2.2)) + sig["lift"] * g["S"] * max(0.0, math.sin(math.pi * min(1.0, s * 1.08))) ** 1.2 \
            + 0.03 * g["S"] * math.sin(strike) * smooth((s - 0.7) / 0.3)
    return (x, -u, z), pitch


def _leg_ik(v, L1, L2, straight=0.9985):
    d = norm(v)
    dc = clamp(d, abs(L1 - L2) + 1e-4, (L1 + L2) * straight)
    vh = unit(v)
    a = math.acos(clamp((L1 * L1 + dc * dc - L2 * L2) / (2 * L1 * dc), -1.0, 1.0))
    k = math.acos(clamp((L1 * L1 + L2 * L2 - dc * dc) / (2 * L1 * L2), -1.0, 1.0))
    flex = math.pi - k
    n = unit(sub((1.0, 0.0, 0.0), scale(vh, vh[0])))
    b = cross(vh, n)
    t = add(scale(vh, math.cos(a)), scale(b, math.sin(a)))
    zl = scale(t, -1.0)
    yl = cross(zl, n)
    R = [[n[0], yl[0], zl[0]], [n[1], yl[1], zl[1]], [n[2], yl[2], zl[2]]]
    return R, flex


def _pelvis(sig, g, ph, base):
    w = 2.0 * math.pi
    yaw = -rad(sig["hip_yaw"]) * math.cos(w * ph)
    roll = -rad(sig["hip_roll"]) * math.sin(w * (ph - 0.03))
    x = sig["sway"] * g["S"] * math.sin(w * (ph - 0.06))
    z = base - sig["bounce"] * g["S"] * math.cos(2.0 * w * (ph - 0.045))
    return (x, 0.0, z), mul(Rz(yaw), Ry(roll)), yaw, roll


_BASE_CACHE = {}


def _base_height(sig, g, total):
    """Hip height offset that lets the planted leg reach the ground through the whole step."""
    key = (tuple(sorted(sig.items())), round(g["leg"], 4), round(total, 4))
    if key in _BASE_CACHE:
        return _BASE_CACHE[key]
    best = 1e9
    lmax = g["leg"] * 0.9975
    for i in range(96):
        ph = i / 96.0
        loc, Rp, _, _ = _pelvis(sig, g, ph, 0.0)
        for side, off in ((1, 0.0), (-1, 0.5)):
            p = (ph + off) % 1.0
            if p > sig["duty"] * 0.62:
                continue                     # late stance: the heel is up and the knee bends, no limit
            tgt, _ = _foot(sig, g, p, side, total)
            hip = add(loc, mulv(Rp, (side * g["hip_x"], 0.0, g["hip_dz"])))
            dx, dy = tgt[0] - hip[0], tgt[1] - hip[1]
            room = lmax * lmax - dx * dx - dy * dy
            zmax = tgt[2] + math.sqrt(max(room, 1e-6))
            best = min(best, zmax - hip[2])
    _BASE_CACHE[key] = best
    return best


def walk(sig, sk, ph, speed=None, mods=None):
    """Pose for one moment of the walk.

    ph     phase of the cycle, 0..1 (0 = left heel strike)
    speed  if given (m/s), the step length is set to keep up with it; otherwise from the signature
    mods   optional layers: chest_yaw, chest_pitch (radians, added), head=(yaw, pitch, roll) radians in
           character space added to the walk's own head carriage, arm_l / arm_r = (weight, flex, out, elbow)
           in degrees to blend an arm towards a gesture pose.
    Returns dict(rot={joint: 3x3}, pelvis_loc=(x, y, z) offset from rest, info={...}).
    """
    mods = mods or {}
    g = _geo(sk)
    ph = ph % 1.0
    T = sig["cycle"] / FPS
    total = speed * sig["duty"] * T if speed else 2.0 * sig["duty"] * sig["stride"] * g["leg"]
    base = _base_height(sig, g, total)
    loc, Rp, yaw_p, roll_p = _pelvis(sig, g, ph, base)
    w = 2.0 * math.pi
    rot = dict(pelvis=Rp)
    info = {}

    # legs: feet are placed first, then the leg is solved to reach them
    for tag, side, off in (("l", 1, 0.0), ("r", -1, 0.5)):
        p = (ph + off) % 1.0
        tgt, pitch = _foot(sig, g, p, side, total)
        hip = add(loc, mulv(Rp, (side * g["hip_x"], 0.0, g["hip_dz"])))
        v = mulv(tr(Rp), sub(tgt, hip))
        Rt, flex = _leg_ik(v, g["L1"], g["L2"])
        rot["thigh_" + tag] = Rt
        rot["calf_" + tag] = Rx(flex)
        foot_world = mul(Rz(side * rad(sig["toe_out"])), Rx(-pitch))
        rot["foot_" + tag] = mul(tr(mul(Rp, Rt, Rx(flex))), foot_world)
        info["knee_" + tag] = math.degrees(flex)
        info["ankle_" + tag] = tgt

    # chest turns against the hips and leans
    yaw_c = -(1.0 + sig["counter"]) * yaw_p + mods.get("chest_yaw", 0.0)
    roll_c = -0.85 * roll_p + 0.25 * roll_p * math.cos(w * ph)
    pitch_c = rad(sig["lean"]) + rad(0.8) * math.cos(2.0 * w * (ph - 0.10)) + mods.get("chest_pitch", 0.0)
    Rs = mul(Rz(yaw_c), Rx(pitch_c), Ry(roll_c))
    rot["spine"] = Rs
    Rtorso = mul(Rp, Rs)

    # arms swing against the legs; a gesture pose can be blended in per arm
    for tag, side, off in (("l", 1, 0.0), ("r", -1, 0.5)):
        p = (ph + off) % 1.0
        k = sig["arm_" + tag]
        sw = -math.cos(w * (p - 0.04))                 # -1 when this side's leg is forward
        flex = sig["arm_bias"] + sig["arm_swing"] * k * sw
        out = sig["arm_out"]
        elbow = sig["elbow"] + sig["elbow_swing"] * k * max(0.0, sw) + 4.0 * k * max(0.0, -sw)
        if ("arm_" + tag) in mods:
            wt, gf, go, ge = mods["arm_" + tag]
            flex = flex * (1.0 - 0.75 * wt) + gf * wt
            out = out + (go - out) * wt
            elbow = elbow * (1.0 - 0.8 * wt) + ge * wt
        rot["upperarm_" + tag] = mul(Rx(-rad(flex)), Ry(-side * rad(out)))
        rot["lowerarm_" + tag] = Rx(-rad(elbow))

    # head: mostly steady in space, follows a little of the body's turning, nods with the steps, late
    lag = sig["head_lag"]
    yaw_b = -sig["counter"] * (-rad(sig["hip_yaw"]) * math.cos(w * (ph - lag)))
    hy, hp, hr = mods.get("head", (0.0, 0.0, 0.0))
    yaw_h = sig["head_follow"] * yaw_b + hy
    pitch_h = rad(sig["head_pitch"]) + rad(sig["head_bob"]) * math.cos(2.0 * w * (ph - 0.045 - lag)) + 0.35 * rad(sig["lean"]) + hp
    roll_h = 0.3 * sig["head_follow"] * roll_p + hr
    want = head_matrix(yaw_h, pitch_h, roll_h)
    rel = mul(tr(Rtorso), want)
    Rn = rot_frac(rel, 0.38)
    rot["neck"] = Rn
    rot["head"] = mul(tr(Rn), rel)
    info["head_yaw"] = yaw_h
    info["speed"] = total / (sig["duty"] * T)
    return dict(rot=rot, pelvis_loc=(loc[0], loc[1], loc[2] - g["pelvis_z"]), info=info)


def forward_kinematics(sk, pose):
    """Joint positions and world rotations (character space) for a pose. Used for previews and tests."""
    pos = {"root": (0.0, 0.0, 0.0)}
    R = {"root": IDENT}
    for name in ORDER:
        par = PARENT[name]
        off = sub(sk[name], sk[par])
        if name == "pelvis":
            pos[name] = add(sk["pelvis"], pose["pelvis_loc"])
        else:
            pos[name] = add(pos[par], mulv(R[par], off))
        R[name] = mul(R[par], pose["rot"].get(name, IDENT))
    return pos, R


# ---- long garments ---------------------------------------------------------------------------------
def hem_scales(sk, pose, g, leg_r, limit=2.4):
    """How much a one-piece skirt, coat or tunic must widen at the hem, in front and behind, to clear the legs.
    g: dict(joint, z_hem, length, r0, r1, flat) describing the garment at rest; leg_r: (hip, knee, ankle) radii.
    Returns (front, back) scale factors at the hem (1 = rest shape); the widening fades to nothing at the hips."""
    pos, R = forward_kinematics(sk, pose)
    Rt = tr(R[g["joint"]])
    r_h, r_k, r_a = leg_r
    z_hip = sk["pelvis"][2] + 0.02 * sk["_S"]
    sf = sb = 1.0
    for tag in "lr":
        hip, knee, ank = pos["thigh_" + tag], pos["calf_" + tag], pos["foot_" + tag]
        for i in range(6):
            u = i / 5.0
            for a, b, ra, rb in ((hip, knee, r_h, r_k), (knee, ank, r_k, r_a)):
                p = tuple(a[k] + (b[k] - a[k]) * u for k in range(3))
                q = add(mulv(Rt, sub(p, pos[g["joint"]])), sk[g["joint"]])      # in the garment's own frame
                z = q[2]
                if z < g["z_hem"] - 0.02 or z > z_hip:
                    continue
                w = min(1.0, (z_hip - z) / (z_hip - g["z_hem"]))
                if w < 0.1:
                    continue
                r = ra + (rb - ra) * u
                # 0.82: the legs are off-centre, where an oval garment is shallower than in the middle
                depth = (g["r0"] + (g["r1"] - g["r0"]) * max(0.0, z - g["z_hem"]) / g["length"]) * g["flat"] * 0.82
                sf = max(sf, 1.0 + ((-q[1] + r + 0.012) / depth - 1.0) / w)
                sb = max(sb, 1.0 + ((q[1] + r + 0.012) / depth - 1.0) / w)
    return min(sf, limit), min(sb, limit)


# ---- conversation layer -------------------------------------------------------------------------
def spring_track(targets, freq=1.6, zeta=0.72, fps=FPS, start=None):
    """Follow a stepped target smoothly, like a head: quick start, small overshoot, settle."""
    x = targets[0] if start is None else start
    v = 0.0
    out = []
    dt = 1.0 / fps / 4.0
    wn = 2.0 * math.pi * freq
    for tg in targets:
        for _ in range(4):
            a = wn * wn * (tg - x) - 2.0 * zeta * wn * v
            v += a * dt
            x += v * dt
        out.append(x)
    return out


def bumps(n, events, width=0.11, lead=0.04, fps=FPS):
    """Sum of short soft pulses at the given (frame, size) events: nods, brow flashes, hand beats."""
    out = [0.0] * n
    for f0, size in events:
        for f in range(max(0, f0 - 8), min(n, f0 + 14)):
            t = (f - f0) / fps - lead
            out[f] += size * math.exp(-(t / width) ** 2)
    return out


def look_angles(dx, dy, dz):
    """Yaw and pitch (radians, head_matrix conventions) to look at a point given in character space."""
    yaw = math.atan2(dx, -dy)
    pitch = -math.atan2(dz, math.hypot(dx, dy))
    return yaw, pitch


def wander(f, seed, fps=FPS):
    """Small never-repeating drift so a head is never perfectly still: (yaw, pitch, roll) in radians."""
    t = f / fps
    return (rad(1.6) * math.sin(0.9 * t + seed) + rad(0.9) * math.sin(2.3 * t + 1.7 * seed),
            rad(1.0) * math.sin(1.3 * t + 2.1 * seed),
            rad(1.2) * math.sin(0.7 * t + 0.6 * seed))


BLINK_SHAPE = (0.45, 0.95, 1.0, 0.6, 0.2)       # eyelid over the five frames of a blink at 24 fps


def blink_track(n, starts):
    out = [0.0] * n
    for f0 in starts:
        for i, val in enumerate(BLINK_SHAPE):
            if 0 <= f0 + i < n:
                out[f0 + i] = max(out[f0 + i], val)
    return out
