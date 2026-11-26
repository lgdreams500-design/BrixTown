"""Brix Town - look check: measures a rendered head for the faults a wide shot hides (the owner, 2026-11-26: "this
hair still look like cap"; "build a capability to analyze these issues").

Not run by itself: a look-test script loads it and calls check_heads() on the people it has built, e.g.
    exec(compile(open(ROOT + "/Tools/blender/look_check.py", encoding="utf-8").read(), "look_check", "exec"), globals())
    report = check_heads(SCENE, [dict(name="Noor_L", at=(x, y), K=K, hz=hz, hair=True, cap=False)], OUT_DIR + "/check")

For each head it renders two small pictures from straight ahead: the head as it looks, and the same view with every
part painted a flat colour (hair red, skin green, a cap blue), so the parts can be told apart pixel by pixel. From the
two it measures, and says in plain words when a number is past its limit:

    shade under the rim   along the line where hair meets brow, how often a band darker than both the hair and the skin
                          lies between them. A dark band there reads as dark hair under a pale cap.
    brim                  how straight that line is across the brow, and how hard its step. A hairline curves and is
                          soft; a cap's edge is one straight hard line.
    ears                  how far the hair bulges out past the head at the height of the ears. More than a little
                          reads as ear-muffs.
    stand-off             how much wider the hair is than the head just above the ears. Hair that stands off the skull
                          all round reads as a helmet.
    one lump              how even the hair's surface is. Hair has locks; a cap is one smooth dome.
    two tones             whether the hair is two clearly different tones (a pale lump over darker hair).

It writes <out>/<name>_look.png (the close-up, to be looked at, large), <name>_parts.png and report.json, and returns
the report. Limits are in LIMITS. A head that wears a cap is measured only for what shows of the hair.
Standard Blender Python and numpy.
"""
import bpy, json, math, os, re
import numpy as np

LIMITS = dict(shade=0.25,          # share of the brow line with a dark band between hair and skin
              straight=0.012,      # the brow line's unevenness, as a share of the head's width: below this it is a ruled line
              step=0.035,          # how far the hair's edge stands out past the skull at the sides (share of head width, each side)
              smooth=0.035,        # the hair's own unevenness of tone (after its overall shading is taken out)
              tones=0.30,
              ears=0.08)           # how far hair may bulge out past the head at the height of the ears (share of head width)          # two tones in the hair: the gap between its darker and lighter halves, as a share


def _load(path):
    img = bpy.data.images.load(path, check_existing=False)
    w, h = img.size
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1]      # top row first
    bpy.data.images.remove(img)
    return a


def _part_mats():
    def em(name, c):
        m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.clear()
        e = nt.nodes.new("ShaderNodeEmission")
        e.inputs[0].default_value = (c[0], c[1], c[2], 1.0)
        e.inputs[1].default_value = 1.0
        o = nt.nodes.new("ShaderNodeOutputMaterial")
        nt.links.new(e.outputs[0], o.inputs[0])
        return m
    return dict(hair=em("LC_Hair", (1, 0, 0)), skin=em("LC_Skin", (0, 1, 0)), cap=em("LC_Cap", (0, 0, 1)), other=em("LC_Other", (0, 0, 0)))


HAIR_PARTS = ("_Hair", "_HairTop", "_HairFill", "_HairFillLow", "_Bun", "_Fringe", "_Temple", "_Wisp", "_Lock")
SKIN_PARTS = ("_Head", "_Ear")
CAP_PARTS = ("_Cap", "_Brim")


def _kind(obname, person):
    if not obname.startswith(person + "_"):
        return "other"
    m = re.match(r"_([A-Za-z]+)", obname[len(person):])
    tail = "_" + m.group(1) if m else ""
    return "hair" if tail in HAIR_PARTS else "cap" if tail in CAP_PARTS else "skin" if tail in SKIN_PARTS else "other"


def check_heads(scene, people, out_dir, res=420, samples=48):
    """people: [dict(name, at=(x, y), K, hz)] with K the head's scale and hz its centre's height, as character3 has them."""
    os.makedirs(out_dir, exist_ok=True)
    pm = _part_mats()
    keep_cam, keep_path = scene.camera, scene.render.filepath
    keep = (scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage, scene.cycles.samples,
            scene.view_settings.view_transform, scene.view_settings.look, scene.view_settings.exposure, scene.cycles.use_denoising)
    world_nodes = scene.world.node_tree.nodes if scene.world and scene.world.use_nodes else []
    report = {}
    for p in people:
        n, (x, y), K, hz = p["name"], p["at"], p["K"], p["hz"]
        cd = bpy.data.cameras.new("LC_Cam")
        cd.type = 'ORTHO'
        cd.ortho_scale = 0.40 * K
        cam = bpy.data.objects.new("LC_Cam_" + n, cd)
        cam.location = (x, y - 4.0, hz + 0.012 * K)
        cam.rotation_euler = (math.pi / 2, 0, 0)
        scene.collection.objects.link(cam)
        scene.camera = cam
        scene.render.resolution_x = scene.render.resolution_y = res
        scene.render.resolution_percentage = 100
        scene.cycles.samples = samples
        look = out_dir + "/%s_look.png" % n
        scene.render.filepath = look
        bpy.ops.render.render(write_still=True, scene=scene.name)
        # the same view, every part a flat colour
        swapped, lights, bg = [], [], []
        for ob in scene.objects:
            if ob.type == 'MESH' or ob.type == 'FONT' or ob.type == 'CURVE':
                k = _kind(ob.name, n)
                mats = list(ob.data.materials)
                swapped.append((ob, mats))
                ob.data.materials.clear()
                ob.data.materials.append(pm[k if k in pm else "other"])
            elif ob.type == 'LIGHT':
                lights.append((ob, ob.hide_render))
                ob.hide_render = True
        for nd in world_nodes:
            if nd.type == 'BACKGROUND':
                bg.append((nd, nd.inputs[1].default_value))
                nd.inputs[1].default_value = 0.0
        scene.cycles.samples = 4
        scene.cycles.use_denoising = False
        scene.view_settings.view_transform = 'Standard'
        scene.view_settings.look = 'None'
        scene.view_settings.exposure = 0.0
        parts = out_dir + "/%s_parts.png" % n
        scene.render.filepath = parts
        bpy.ops.render.render(write_still=True, scene=scene.name)
        for ob, mats in swapped:
            ob.data.materials.clear()
            for m in mats:
                ob.data.materials.append(m)
        for ob, h in lights:
            ob.hide_render = h
        for nd, v in bg:
            nd.inputs[1].default_value = v
        (scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage, scene.cycles.samples,
         scene.view_settings.view_transform, scene.view_settings.look, scene.view_settings.exposure, scene.cycles.use_denoising) = keep
        bpy.data.objects.remove(cam)
        report[n] = measure(_load(look), _load(parts))
    scene.camera, scene.render.filepath = keep_cam, keep_path
    with open(out_dir + "/report.json", "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    return report


def measure(look, parts):
    """The numbers and the plain-word faults of one head, from its close-up and its parts picture."""
    lum = 0.2126 * look[..., 0] + 0.7152 * look[..., 1] + 0.0722 * look[..., 2]
    hair = (parts[..., 0] > 0.5) & (parts[..., 1] < 0.5)
    skin = (parts[..., 1] > 0.5) & (parts[..., 0] < 0.5)
    cap = parts[..., 2] > 0.5
    h, w = lum.shape
    out = dict(faults=[])
    if hair.sum() < 50 or skin.sum() < 50:
        out["note"] = "no hair to measure" if hair.sum() < 50 else "no face to measure"
        return out
    cols = np.where(skin.any(axis=0))[0]
    head_w = float(cols.max() - cols.min() + 1)
    hair_l, skin_l = float(np.median(lum[hair])), float(np.median(lum[skin]))
    # the line where hair meets brow: in each column of the middle of the face, the lowest hair pixel above the first skin pixel
    line, shade = [], []
    c0, c1 = int(cols.min() + head_w * 0.2), int(cols.max() - head_w * 0.2)
    band = max(3, int(head_w * 0.035))
    for c in range(c0, c1):
        sk = np.where(skin[:, c])[0]
        if not len(sk):
            continue
        top = sk.min()
        above = np.where(hair[:top, c])[0]
        if not len(above) or top - above.max() > band * 3:
            continue
        edge = above.max()
        line.append(edge)
        zone = lum[max(0, edge - band):min(h, top + band), c]
        shade.append(float(zone.min()) < 0.72 * min(hair_l, skin_l))
    out["brow_line_columns"] = len(line)
    if len(line) > max(10, (c1 - c0) * 0.5):               # hair over at least half the brow: a few strands on a bare crown are not a hairline
        line = np.array(line, dtype=np.float32)
        out["shade"] = round(float(np.mean(shade)), 3)
        fit = np.polyfit(np.arange(len(line)), line, 2)
        out["straight"] = round(float(np.ptp(np.polyval(fit, np.arange(len(line)))) / head_w), 4)   # how much the line bows across the brow
        if out["shade"] > LIMITS["shade"]:
            out["faults"].append("a dark band lies under the hair's edge along %d%% of the brow: it reads as dark hair under a pale cap" % round(out["shade"] * 100))
        if out["straight"] < LIMITS["straight"] and not cap.any():
            out["faults"].append("the hair meets the brow in one straight line (bow %.3f of the head's width): a hairline curves, a cap's edge is ruled" % out["straight"])
    # stand-off: how far the hair reaches past the skull to each side, at the height of the brow line
    rows = np.where(skin.any(axis=1))[0]
    r = int(rows.min() + (rows.max() - rows.min()) * 0.22)
    hr = np.where(hair[r] | skin[r])[0]
    sr = np.where(skin[max(r, 0):r + int(head_w * 0.25)].any(axis=0))[0]
    if len(hr) and len(sr):
        out["step"] = round(float(max(sr.min() - hr.min(), hr.max() - sr.max(), 0)) / head_w, 4)
        if out["step"] > LIMITS["step"] and not cap.any():
            out["faults"].append("the hair stands %.1f%% of the head's width out past the skull at each side: a shell round the head, like a helmet" % (out["step"] * 100))
    # at the ears: hair bulging out past the head's side reads as ear-muffs
    r2 = int(rows.min() + (rows.max() - rows.min()) * 0.50)
    h2, s2 = np.where(hair[r2])[0], np.where(skin[r2])[0]
    if len(h2) and len(s2):
        out["ears"] = round(float(max(s2.min() - h2.min(), h2.max() - s2.max(), 0)) / head_w, 4)
        if out["ears"] > LIMITS["ears"]:
            out["faults"].append("the hair bulges %.1f%% of the head's width out past each ear: it reads as ear-muffs, not hair" % (out["ears"] * 100))
    # one lump: the hair's tone with its broad shading taken out
    ys, xs = np.where(hair)
    A = np.stack([xs, ys, xs * ys, xs * xs, ys * ys, np.ones_like(xs)], axis=1).astype(np.float32)
    v = lum[hair]
    coef, *_ = np.linalg.lstsq(A, v, rcond=None)
    out["smooth"] = round(float(np.std(v - A @ coef) / max(hair_l, 1e-4)), 4)
    if out["smooth"] < LIMITS["smooth"] and not cap.any():
        out["faults"].append("the hair is one smooth lump (unevenness %.3f): no locks, parting or strands, so it reads as a moulded cap" % out["smooth"])
    lo, hi = np.percentile(v, 20), np.percentile(v, 80)
    out["tones"] = round(float((hi - lo) / max(hi, 1e-4)), 3)
    return out


def say(report, log):
    bad = 0
    for n, r in report.items():
        nums = ", ".join("%s %s" % (k, r[k]) for k in ("shade", "straight", "step", "ears", "smooth", "tones") if k in r)
        log("look check %s: %s" % (n, nums or r.get("note", "")))
        for f in r.get("faults", []):
            bad += 1
            log("   FAULT: " + f)
    log("look check: %d heads, %d faults" % (len(report), bad))
    return bad
