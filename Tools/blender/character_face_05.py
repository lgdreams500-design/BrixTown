"""Wasted - character test 05: option B refined, with a face built to animate.

Run inside Blender (Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/character_face_05.py").read(), "character_face_05.py", "exec"))

The face is a small set of named controls (brows, eyelids, gaze, mouth shape, mouth corners).
Expressions and speech are just values for those controls, which is how the game will drive
them. Renders: a line-up, an expression sheet, a mouth-shape sheet, and frames of a spoken line.
"""
import bpy, bmesh, math, random, os, time, traceback
from mathutils import Vector, Euler, Matrix

B = "C:/Projects/Wasted/Tools/blender/"
_p = open(B + "old_town_04.py", encoding="utf-8").read().split("\n# ---- scene 1 and 2")[0]
exec(compile(_p, "old_town_04_helpers", "exec"), globals())

NAME = "character_face_05"
LOG = OUT + "/" + NAME + "_log.txt"
TALK_DIR = OUT + "/" + NAME + "_talk"
ONLY = None                      # None renders everything; e.g. ("talk",)
QUICK = None                     # None renders every sheet; e.g. ("mouths",) for a fast check
CS = CHAR_STYLES["B"]

# ---- mouth shapes for speech (sizes in head units, metres) ---------------------------------
MOUTH_W, MOUTH_H = 1.3, 1.25      # stylised: mouths larger than life so speech reads at a distance
VIS = {
    "rest": dict(w=0.0175, h=0.0011, corner=0.0015, curve=0.2),
    "MBP": dict(w=0.0150, h=0.0006, round=0.5),
    "AA": dict(w=0.0165, h=0.0115, teeth=1, lower=1, tongue=1),
    "EH": dict(w=0.0185, h=0.0075, teeth=1, lower=1, tongue=1),
    "EE": dict(w=0.0215, h=0.0048, teeth=1, teeth_z=0.5, teeth_h=0.45, teeth_w=0.8, lower=1, lower_z=0.5, lower_h=0.45, lower_w=0.8, corner=0.002),
    "OH": dict(w=0.0110, h=0.0105, round=0.8),
    "OO": dict(w=0.0070, h=0.0060, round=1.0),
    "FV": dict(w=0.0160, h=0.0045, teeth=1, teeth_z=0.15, teeth_h=0.6),
    "smile": dict(w=0.0200, h=0.0012, corner=0.004, curve=0.65),
}
VIS_SHEET = ["rest", "MBP", "AA", "EH", "EE", "OH", "OO", "FV"]

NEUTRAL = dict(browL=(0.0, 0.0), browR=(0.0, 0.0), lid=0.0, squint=0.0, open=1.0, gaze=(0.0, 0.0), mouth="rest", corner=None, curve=None)
EXPRESSIONS = [
    ("Neutral", {}),
    ("Happy", dict(mouth=dict(w=0.0200, h=0.0060, teeth=1, teeth_z=0.4, teeth_h=0.5, teeth_w=0.8), curve=0.55, corner=0.004,
                   squint=0.5, browL=(0.004, 0.005), browR=(0.004, 0.005))),
    ("Sad", dict(mouth=dict(w=0.0135, h=0.0010), curve=-0.6, corner=-0.003, lid=0.30, browL=(0.008, -0.006), browR=(0.008, -0.006),
                 gaze=(0.0, -0.003))),
    ("Angry", dict(mouth=dict(w=0.0150, h=0.0034, teeth=1, teeth_z=0.5, teeth_h=0.45, teeth_w=0.8, lower=1, lower_z=0.5, lower_h=0.45, lower_w=0.8), curve=-0.35, corner=-0.002,
                   lid=0.14, squint=0.3, browL=(-0.010, 0.002), browR=(-0.010, 0.002))),
    ("Surprised", dict(mouth="OH", open=1.15, browL=(0.010, 0.008), browR=(0.010, 0.008))),
    ("Thinking", dict(mouth=dict(w=0.0115, h=0.0008, round=0.3), curve=0.1, corner=0.001, squint=0.2, browL=(-0.003, -0.002),
                      browR=(0.008, 0.006), gaze=(0.004, 0.003))),
]
EXPR = dict(EXPRESSIONS)
EXPR["Talking"] = dict(mouth="AA", browL=(0.003, 0.003), browR=(0.003, 0.003))
EXPR["Smile"] = dict(mouth="smile", squint=0.45, browL=(0.002, 0.003), browR=(0.002, 0.003))

# the spoken test line: (mouth shape, frames at 12 fps, word index or -1 for a pause)
WORDS = ["Welcome", "to", "Brix", "Mart.", "Fancy", "a", "coffee?"]
SEQ = [("rest", 4, -1),
       ("OO", 2, 0), ("EH", 2, 0), ("AA", 2, 0), ("MBP", 2, 0),
       ("EE", 1, 1), ("OO", 2, 1),
       ("MBP", 1, 2), ("EH", 2, 2), ("EE", 2, 2),
       ("MBP", 2, 3), ("AA", 3, 3), ("EE", 1, 3),
       ("rest", 5, -1),
       ("FV", 2, 4), ("AA", 2, 4), ("EE", 3, 4),
       ("EH", 2, 5),
       ("OH", 3, 6), ("FV", 2, 6), ("EE", 3, 6),
       ("smile", 7, -1)]
BLINK = {1: 0.5, 2: 1.0, 3: 0.45, 27: 0.5, 28: 1.0, 29: 0.45}


# ---- option B character with a posable face ---------------------------------------------------
def person(sp, ox, oy=0.0, oz=0.0, mode="full"):
    """mode: full | bust | head. Returns the face rig (a dict) for pose()."""
    cs = CS
    S = sp.get("height", 1.72) / 1.72 if mode == "full" else 1.0
    W = sp.get("build", 1.0) * cs["wide"]
    T = cs["thick"]
    fw = sp.get("face_w", 1.0)
    n = sp["name"]
    legk, tork = cs["leg"], cs["torso"]

    def zm(z):
        return z * legk if z <= 0.93 else 0.93 * legk + (z - 0.93) * tork

    def P(x, y, z):
        return Vector((ox + x * S, oy + y * S, oz + zm(z) * S))

    skin = solid(sp["skin"], 0.5)
    top = cloth(sp["top"])
    bot = cloth(sp["bottom"])
    coat = cloth(sp["coat"]) if "coat" in sp else None
    shoe = solid(sp.get("shoes", (0.10, 0.08, 0.07)), 0.6)
    hairc = sp.get("hair_color", (0.08, 0.05, 0.04))
    hair = solid(hairc, 0.6)
    dark = solid((0.03, 0.02, 0.02), 0.25)
    garment = sp.get("garment", "tee")
    kind = "bare" if garment == "dress" else sp.get("bottom_type", "trousers")
    sleeve_long = garment in ("long", "tunic") or coat is not None
    extras = sp.get("extras", ())
    chest = 0.172 * W
    waist = 0.138 * W
    lx = 0.085
    full = mode == "full"

    if full:
        for sg in (-1, 1):
            ellipsoid(n + "_Shoe", P(sg * lx, -0.04, 0.0) + Vector((0, 0, 0.045 * T * S)), (0.05 * T * S, 0.12 * T * S, 0.045 * T * S), shoe)
            if kind == "trousers":
                limb(n + "_Leg", P(sg * lx, 0, 0.07), P(sg * lx, 0, 0.90), 0.058 * T * S, 0.088 * T * S, bot)
            else:
                limb(n + "_Leg", P(sg * lx, 0, 0.07), P(sg * lx, 0, 0.90), 0.045 * T * S, 0.08 * T * S, skin)
                if kind == "shorts":
                    limb(n + "_Short", P(sg * lx, 0, 0.56), P(sg * lx, 0, 0.92), 0.086 * T * S, 0.092 * T * S, bot)
        if kind != "bare":
            ellipsoid(n + "_Pelvis", P(0, 0, 0.93), (0.16 * W * S, 0.108 * T * S, 0.118 * tork * S), bot)

    flat = 0.62 * (1 + 0.25 * (T - 1))
    front = -chest * flat
    if mode not in ("head", "face"):
        if not full:
            hem, r_hem = 1.24, 0.165 * W
        elif garment in ("tee", "long"):
            hem, r_hem = 1.02, waist
        elif garment == "tunic":
            hem, r_hem = 0.80, 0.172 * W
        else:
            hem = sp.get("dress_len", 0.45)
            r_hem = (0.17 + 0.12 * (1.0 - hem)) * W
        limb(n + "_Body", P(0, 0, hem), P(0, 0, 1.36), r_hem * S, chest * S, top, flat=flat, cap=0.28)
        outer = coat or top
        if coat is not None:
            ch = sp.get("coat_len", 0.6) if full else 1.22
            r_c = (max(0.20 * W, r_hem + 0.02) if garment == "dress" else 0.20 * W) if full else 0.185 * W
            limb(n + "_Coat", P(0, 0.006, ch), P(0, 0.006, 1.37), r_c * S, (chest + 0.014) * S, coat, flat=flat * 1.06, cap=0.25)
            front = -(chest + 0.014) * flat * 1.06
            ellipsoid(n + "_Collar", P(0, 0.0, 1.425), (0.085 * T * S, 0.08 * T * S, 0.035 * S), coat)
        sh = chest + 0.016
        yk = sh if full else chest - 0.035
        limb(n + "_Yoke", P(-yk, 0, 1.375), P(yk, 0, 1.375), 0.052 * T * S, 0.052 * T * S, outer)
        if full:
            for sg in (-1, 1):
                shoulder = P(sg * sh, 0, 1.375)
                wrist = P(sg * (sh + 0.055), -0.02, 0.87)
                limb(n + "_Arm", shoulder, wrist, 0.052 * T * S, 0.035 * T * S, outer if sleeve_long else skin)
                if not sleeve_long:
                    limb(n + "_Sleeve", shoulder, shoulder.lerp(wrist, 0.42), 0.057 * T * S, 0.052 * T * S, top)
                ellipsoid(n + "_Hand", wrist + Vector((sg * 0.003, -0.008, -0.062 * tork)) * S, (0.03 * T * S, 0.042 * T * S, 0.055 * T * S), skin)
        if "scarf" in sp:
            sc = cloth(sp["scarf"])
            ellipsoid(n + "_Scarf", P(0, 0.0, 1.44), (0.088 * T * S, 0.084 * T * S, 0.04 * S), sc)
            if full:
                limb(n + "_ScarfTail", P(0.045, front - 0.012, 1.14), P(0.04, front - 0.006, 1.42), 0.034 * S, 0.03 * S, sc, flat=0.35, cap=0.4)
        if "apron" in extras and full:
            limb(n + "_Apron", P(0, front * 0.82, 0.66), P(0, front * 0.86, 1.22),
                 0.118 * W * S, 0.10 * W * S, cloth(sp.get("apron_color", TERRA)), flat=0.26, cap=0.2)
        if "bag" in extras and full:
            strap = solid((0.13, 0.09, 0.06), 0.7)
            limb(n + "_Strap", P(-0.15, front - 0.006, 1.40), P(0.18, front + 0.006, 1.0), 0.012 * S, 0.012 * S, strap)
            ellipsoid(n + "_Bag", P(0.2, -0.105 * T, 0.93), (0.05 * S, 0.10 * S, 0.09 * S), strap)
    if mode != "face":
        limb(n + "_Neck", P(0, 0.005, 1.39), P(0, 0, 1.50), 0.046 * T * S, 0.044 * T * S, skin)

    # ---- head ----
    k = cs["head"]
    K = k * S
    rx, ry, rz = 0.104 * fw, 0.110, 0.130
    hz = zm(1.475) + rz * k

    def H(x, y, z):
        return Vector((ox + x * K, oy + y * K, oz + (hz + z * k) * S))

    def face_y(x, z):
        zu = z / rz
        s = jaw(zu)
        inside = 1.0 - (x / (rx * s)) ** 2 - zu * zu
        return -ry * s * math.sqrt(max(inside, 0.0))

    def F(x, z, out=0.0):
        return H(x, face_y(x, z) - out, z)

    head = []
    bm = uvs(32, 20)
    for v in bm.verts:
        s = jaw(v.co.z)
        v.co = Vector((v.co.x * rx * s * K, v.co.y * ry * s * K, v.co.z * rz * K))
    head.append(finish(n + "_Head", bm, skin, H(0, 0, 0)))
    for sg in (-1, 1):
        head.append(ellipsoid(n + "_Ear", H(sg * rx * 0.97, 0.012, -0.012), (0.012 * K, 0.022 * K, 0.032 * K), skin))
    ns = sp.get("nose", 1.0) * cs["nose"]
    head.append(ellipsoid(n + "_Nose", F(0, -0.030, 0.003), (0.0115 * ns * K, 0.013 * ns * K, 0.017 * ns * K), skin))
    if "moustache" in extras:
        head.append(ellipsoid(n + "_Moustache", F(0, -0.056, 0.004), (0.028 * K, 0.007 * K, 0.0075 * K), hair))

    def part(name, mat, yaw=0.0):
        ob = ellipsoid(n + "_" + name, H(0, 0, 0), (1.0, 1.0, 1.0), mat)
        ob.rotation_euler = (0.0, 0.0, yaw)
        head.append(ob)
        return ob

    es = sp.get("eye_size", 1.0) * cs["eye"]
    ex = 0.043 * fw
    yaw = math.asin(min(ex / rx, 1.0)) * 0.55
    white = solid((0.78, 0.76, 0.72), 0.3)
    iris = solid(sp.get("eyes", (0.16, 0.09, 0.05)), 0.2)
    browc = solid(dim(hairc, 0.6), 0.6)
    lip = solid(sp.get("lips", (sp["skin"][0] * 0.62, sp["skin"][1] * 0.36, sp["skin"][2] * 0.34)), 0.5)
    bt = sp.get("brow", 1.0)
    side = {}
    for sg in (-1, 1):
        q = dict(white=part("EyeWhite", white, sg * yaw), iris=part("Iris", iris, sg * yaw), pupil=part("Pupil", dark, sg * yaw),
                 glint=part("Glint", white), lid=part("Lid", skin, sg * yaw), lower=part("LowerLid", skin, sg * yaw),
                 lash=part("Lash", dark, sg * yaw),
                 corner=part("MouthCorner", lip))
        q["brow"] = limb(n + "_Brow", F(sg * (ex - 0.02), 0.05, 0.002), F(sg * (ex + 0.025), 0.052, 0.002), 0.0044 * bt * K, 0.003 * bt * K, browc)
        head.append(q["brow"])
        side[sg] = q
    mouth = dict(lips=part("Lips", lip), cavity=part("MouthInside", solid((0.05, 0.015, 0.02), 0.6)),
                 teeth=part("Teeth", solid((0.80, 0.78, 0.72), 0.3)), teeth_lo=part("TeethLower", solid((0.76, 0.74, 0.68), 0.3)),
                 tongue=part("Tongue", solid((0.50, 0.16, 0.17), 0.4)))

    # hair
    style = sp.get("hair", "short")
    cap_r = ((rx + 0.020) * K, (ry + 0.022) * K, (rz + 0.020) * K)
    if style == "afro":
        head.append(hair_cap(n + "_Hair", H(0, 0.012, 0.035), ((rx + 0.055) * K, (ry + 0.055) * K, (rz + 0.06) * K), hair, "afro", jitter=0.006 * K))
    elif style != "bald":
        head.append(hair_cap(n + "_Hair", H(0, 0.012, 0.014), cap_r, hair, style))
    if style in ("short", "side"):
        off = 0.022 if style == "side" else 0.0
        head.append(ellipsoid(n + "_HairTop", H(off, -0.018, 0.108), (0.086 * K, 0.104 * K, 0.052 * K), hair,
                              (0.0, 0.18 if style == "side" else 0.0, 0.0)))
    if style == "long":
        for sg in (-1, 1):
            head.append(limb(n + "_Lock", H(sg * 0.106, 0.0, 0.0), H(sg * 0.10, -0.018, -0.19), 0.021 * K, 0.013 * K, hair))
    if style == "bun":
        head.append(ellipsoid(n + "_Bun", H(0, 0.085, 0.135), (0.052 * K, 0.052 * K, 0.046 * K), hair))
    if "cap" in extras:
        capm = cloth(sp.get("cap_color", TERRA))
        head.append(hair_cap(n + "_Cap", H(0, 0.006, 0.016), ((rx + 0.03) * K, (ry + 0.032) * K, (rz + 0.028) * K), capm, "dome"))
        head.append(ellipsoid(n + "_Brim", H(0, -0.135, 0.062), (0.09 * K, 0.075 * K, 0.009 * K), capm, (0.12, 0.0, 0.0)))

    rig = dict(F=F, K=K, sp=sp, es=es, ex=ex, yaw=yaw, side=side, mouth=mouth, head=head, pivot=P(0, 0.01, 1.43))
    pose(rig, {})
    return rig


def pose(rig, ps):
    """Set the face controls. Everything the game animates on a face goes through here."""
    p = dict(NEUTRAL)
    p.update(ps)
    F, K, sp, es, ex = rig["F"], rig["K"], rig["sp"], rig["es"], rig["ex"]
    ez = 0.008
    # rounder eye with a large iris that rests near the lower lid: reads alert, not tired
    ew = 0.0245 * es
    eh = 0.0210 * es * sp.get("eye_open", 1.0) * p["open"]
    ir = 0.0155 * es
    irz = min(ir, eh * 0.97)
    gx, gz = p["gaze"]
    gz -= 0.001 * es
    c = min(max(p["lid"], 0.0), 1.0)          # upper lid: 0 open, 1 closed
    sq = min(max(p["squint"], 0.0), 1.0)      # lower lid rising, as in a smile
    by = ez + 0.0445 * max(1.0, es * 0.9)
    tilt = 0.10                                # outer corners slightly up
    # a squint narrows the eye from below (the lower edge rises); no extra geometry needed
    ehs = eh * (1.0 - 0.45 * sq)
    ezc = ez + (eh - ehs)
    irz = min(ir, ehs * 0.97)
    for sg in (-1, 1):
        q = rig["side"][sg]
        rot = (0.0, -sg * tilt, sg * rig["yaw"])
        R = Euler(rot).to_matrix()
        for key in ("white", "lash", "lid", "iris", "pupil", "glint"):
            q[key].rotation_euler = rot
        wc = F(sg * ex, ezc, -0.005)

        def on_eye(u, w, lift):
            """A point on the surface of the eye white, so iris and pupil always sit on it."""
            uu = max(-0.9, min(0.9, u / ew))
            ww = max(-0.9, min(0.9, w / ehs))
            depth = 0.011 * math.sqrt(max(0.05, 1.0 - uu * uu - ww * ww))
            return wc + (R @ Vector((u, -(depth + lift), w))) * K

        q["white"].location = wc
        q["white"].scale = (ew * K, 0.011 * K, ehs * K)
        q["iris"].location = on_eye(gx, gz, 0.0003)
        q["iris"].scale = (ir * K, 0.0014 * K, irz * K)
        q["pupil"].location = on_eye(gx, gz, 0.0013)
        q["pupil"].scale = (0.0068 * es * K, 0.0010 * K, min(0.0068 * es, irz) * K)
        q["glint"].location = on_eye(gx + 0.005, gz + 0.006, 0.0020)
        q["glint"].scale = (0.0034 * K, 0.0010 * K, 0.0034 * K)
        q["lower"].hide_render = True
        q["lid"].hide_render = c < 0.03
        q["lid"].location = F(sg * ex, ezc + ehs * (1.0 - c), 0.0050)
        q["lid"].scale = (ew * 1.03 * K, 0.0055 * K, max(ehs * c * 1.08, 0.0004) * K)
        if c < 0.03:
            # open eye: the lash is a dark shape behind the eye, so only an arched rim shows above it
            q["lash"].location = F(sg * ex, ezc + 0.0032, -0.0058)
            q["lash"].scale = (ew * 1.07 * K, 0.010 * K, ehs * 1.07 * K)
        else:
            q["lash"].location = F(sg * ex, ezc + ehs * (1.0 - 2.08 * c) - 0.001, 0.0062)
            q["lash"].scale = (ew * 1.04 * K, 0.0050 * K, max(ehs, 0.019) * 0.10 * K)
        din, dout = p["browL"] if sg < 0 else p["browR"]
        p0 = F(sg * (ex - 0.02), by + din, 0.002)
        p1 = F(sg * (ex + 0.025), by + 0.004 + dout, 0.002)
        q["brow"].location = p0
        q["brow"].rotation_quaternion = (p1 - p0).to_track_quat('Z', 'Y')
    v = dict(VIS[p["mouth"]]) if isinstance(p["mouth"], str) else dict(p["mouth"])
    cw, chh, rnd = v["w"] * MOUTH_W, v["h"] * MOUTH_H, v.get("round", 0.0)
    mz = -0.072
    m = rig["mouth"]
    m["lips"].location = F(0, mz, 0.0005)
    m["lips"].scale = ((cw + 0.0045) * K, 0.006 * K, (chh + 0.0036 + 0.002 * rnd) * K)
    m["cavity"].location = F(0, mz, 0.0030)
    m["cavity"].scale = (cw * K, 0.0042 * K, max(chh, 0.0005) * K)
    show_teeth = bool(v.get("teeth")) and chh > 0.0025
    m["teeth"].hide_render = not show_teeth
    base = F(0, mz, 0.0)

    def in_mouth(dz, out):
        return base + Vector((0.0, -out, dz)) * K

    # upper teeth also sit in the plane of the opening, so they stay inside the lips
    m["teeth"].location = in_mouth(chh * v.get("teeth_z", 0.58), 0.0050)
    m["teeth"].scale = (cw * v.get("teeth_w", 0.66) * K, 0.0034 * K, max(chh * v.get("teeth_h", 0.32), 0.0003) * K)

    show_lower = bool(v.get("lower")) and chh > 0.0025
    m["teeth_lo"].hide_render = not show_lower
    m["teeth_lo"].location = in_mouth(-chh * v.get("lower_z", 0.68), 0.0050)
    m["teeth_lo"].scale = (cw * v.get("lower_w", 0.58) * K, 0.0034 * K, max(chh * v.get("lower_h", 0.26), 0.0003) * K)
    # the tongue lies behind the lower teeth, in the lower middle of the mouth
    show_tongue = bool(v.get("tongue")) and chh > 0.004
    m["tongue"].hide_render = not show_tongue
    m["tongue"].location = in_mouth(-chh * 0.25, 0.0046)
    m["tongue"].scale = (cw * 0.6 * K, 0.0028 * K, max(chh * 0.34, 0.0003) * K)
    curve = p["curve"] if p["curve"] is not None else v.get("curve", 0.0)
    rise = (p["corner"] if p["corner"] is not None else v.get("corner", 0.0)) + 0.009 * curve
    reach = 0.0085
    half = 0.5 * math.sqrt(reach * reach + rise * rise) + 0.002
    ang = math.atan2(rise, reach)
    for sg in (-1, 1):
        cq = rig["side"][sg]["corner"]
        cq.location = F(sg * (cw * 0.8 + reach * 0.5), mz + rise * 0.5, 0.0016)
        cq.scale = (half * K, 0.0048 * K, 0.0029 * K)
        cq.rotation_euler = (0.0, -sg * ang, 0.0)


def ortho_cam(name, x, z, scale):
    cd = bpy.data.cameras.new(name)
    cd.type = 'ORTHO'
    cd.ortho_scale = scale
    co = bpy.data.objects.new(name, cd)
    co.location = (x, -6.0, z)
    co.rotation_euler = (math.pi / 2, 0, 0)
    COL.objects.link(co)
    return co


def with_name(who, suffix, **extra):
    sp = dict(CAST[who])
    sp["name"] = who + "_" + suffix
    if "drop" in extra:
        sp["extras"] = tuple(e for e in sp.get("extras", ()) if e not in extra["drop"])
    return sp


# ---- scene with the line-up and the two sheets ---------------------------------------------------
def sheets():
    random.seed(4)
    scene = new_scene("Faces05")
    m = mats()
    box("Ground", -8, 80, -12, 4, -0.3, 0, m["paving"], bevel=0)
    box("Wall", -8, 12, 1.3, 1.9, 0, 3.2, m["stone"])
    box("Wall_Upper", -8, 12, 1.36, 1.9, 3.2, 6, m["plaster"])
    box("Wall_Ledge", -8, 12, 1.18, 1.9, 3.2, 3.45, m["stone_dark"])
    box("Sheet_Wall", 20, 80, 1.0, 1.4, -1, 6, m["plaster"], bevel=0)
    lantern("Lamp_Lineup", -3.3, 1.2, 2.8, m, 60)
    lantern("Lamp_Lineup2", 3.3, 1.2, 2.8, m, 60)
    cams = {}
    # line-up, each with a different expression
    for i, (who, ex_name) in enumerate((("PlayerA", "Happy"), ("PlayerB", "Neutral"), ("Shopkeeper", "Talking"),
                                         ("Courier", "Thinking"), ("Elder", "Smile"), ("Office", "Surprised"))):
        rig = person(with_name(who, "L"), -2.3 + i * 0.92, 0.0)
        pose(rig, EXPR[ex_name])
    cams["lineup"] = (camera(scene, "Cam_Lineup", (0, -9.6, 1.25), (0, 0, 0.97), 60.0), (1600, 800))
    # expressions: 3 x 2 busts of the same person
    for i, (label, ps) in enumerate(EXPRESSIONS):
        rig = person(with_name("PlayerA", "E%d" % i), 30 + (i % 3 - 1) * 0.52, 0.0, 0.64 if i < 3 else 0.0, mode="bust")
        pose(rig, ps)
    cams["expressions"] = (ortho_cam("Cam_Expr", 30, 1.80, 1.75), (1500, 1150))
    lantern("Lamp_Expr", 28.9, 0.9, 2.9, m, 50)
    # mouth shapes: 4 x 2 heads of the same person
    for i, key in enumerate(VIS_SHEET):
        rig = person(with_name("Shopkeeper", "V%d" % i, drop=("moustache",)), 60 + (i % 4 - 1.5) * 0.36, 0.0, 0.40 if i < 4 else 0.0, mode="face")
        pose(rig, dict(mouth=key))
    cams["mouths"] = (ortho_cam("Cam_Mouths", 60, 1.80, 1.5), (1600, 860))
    sun_and_sky(scene, (-0.45, -0.50, 0.74), 3.2, (1.0, 0.82, 0.62), (0.42, 0.55, 0.82), 0.25, angle=6.0)
    return scene, cams


# ---- scene for the spoken line ----------------------------------------------------------------------
def talk():
    scene = new_scene("Talk05")
    m = mats()
    box("Wall", -3, 3, 0.9, 1.3, -1, 5, m["plaster"], bevel=0)
    lantern("Lamp", -1.25, 0.8, 1.95, m, 40)
    rig = person(with_name("Shopkeeper", "T", drop=("moustache",)), 0.0, 0.0, 0.0, mode="bust")
    pivot = bpy.data.objects.new("HeadPivot", None)
    pivot.location = rig["pivot"]
    COL.objects.link(pivot)
    inv = Matrix.Translation(-Vector(rig["pivot"]))
    for ob in rig["head"]:
        ob.parent = pivot
        ob.matrix_parent_inverse = inv
    camera(scene, "Cam_Talk", (0.0, -1.55, 1.56), (0.0, 0.0, 1.55), 85.0)
    sun_and_sky(scene, (-0.45, -0.50, 0.74), 3.2, (1.0, 0.82, 0.62), (0.42, 0.55, 0.82), 0.25, angle=6.0)
    frames = []
    for (vis, count, word) in SEQ:
        for _ in range(count):
            frames.append((vis, word))
    return scene, rig, pivot, frames


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
    os.makedirs(TALK_DIR, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    if not ONLY or "sheets" in ONLY:
        try:
            scene, cams = sheets()
            log("sheets built: %d objects" % len(scene.objects))
            for key, (cam, res) in cams.items():
                if QUICK and key not in QUICK:
                    continue
                t = time.time()
                configure(scene, -0.3, res, 128)
                scene.camera = cam
                scene.render.filepath = OUT + "/" + NAME + "_" + key + ".png"
                bpy.ops.render.render(write_still=True, scene=scene.name)
                log("%s rendered in %.1fs" % (key, time.time() - t))
        except Exception:
            log("FAILED sheets:\n" + traceback.format_exc())
    if not ONLY or "talk" in ONLY:
        try:
            scene, rig, pivot, frames = talk()
            configure(scene, -0.3, (480, 480), 48)
            log("talk built: %d objects, %d frames" % (len(scene.objects), len(frames)))
            t = time.time()
            for i, (vis, word) in enumerate(frames):
                ps = dict(mouth=vis, lid=BLINK.get(i, 0.0), squint=0.45 if vis == "smile" else 0.0)
                if word == 0:
                    ps["browL"] = ps["browR"] = (0.004, 0.004)
                elif word == 6:
                    ps["browL"] = ps["browR"] = (0.007, 0.008)
                elif vis == "smile":
                    ps["browL"] = ps["browR"] = (0.003, 0.004)
                pose(rig, ps)
                pivot.rotation_euler = (math.radians(2.2) * math.sin(i * 0.55), 0.0, math.radians(1.6) * math.sin(i * 0.23 + 1.0))
                scene.render.filepath = TALK_DIR + "/frame_%03d.png" % i
                bpy.ops.render.render(write_still=True, scene=scene.name)
            log("talk: %d frames rendered in %.1fs" % (len(frames), time.time() - t))
        except Exception:
            log("FAILED talk:\n" + traceback.format_exc())
    try:
        bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
        log("saved .blend")
    except Exception:
        log("save failed:\n" + traceback.format_exc())
    log("DONE")


main()
