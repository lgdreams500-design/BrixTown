"""Puts the building_styles_22 renders on labelled sheets and makes the catalogue's front pictures.
Runs in ordinary Python with Pillow (not in Blender), after building_styles_22.py:
    python Tools/blender/building_styles_22_sheet.py
Writes, in Art/StyleTests/building_styles_22/:
    sheet_<n>.png          a row per group of build types, a column per colourway (a, b, c), each labelled
    fronts/<type>.jpg      one front picture per build type of Tools/worldgen/draw_builds.py (900 x 600, quality 85):
                           colourway a of its group (or its own front render), the house_styles_20 render "a" of its
                           line for the designed houses, the view from above for open places
    fronts_sheet.png       every front picture with its key, to check the set is complete
    render_times.txt       build and render seconds per job from the Blender log, failures and missing fronts
"""
import json, os, sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DIR = os.path.join(ROOT, "Art", "StyleTests", "building_styles_22")
FRONTS = os.path.join(DIR, "fronts")
STYLES = json.load(open(os.path.join(DIR, "building_styles.json"), encoding="utf-8"))
_d = {"__file__": os.path.join(ROOT, "Tools", "worldgen", "draw_builds.py"), "__name__": "draw_builds"}
sys.path.insert(0, os.path.dirname(_d["__file__"]))         # draw_builds imports its neighbours
_argv, sys.argv = sys.argv, [sys.argv[0], os.devnull]
exec(compile(open(_d["__file__"], encoding="utf-8").read(), "draw_builds", "exec"), _d)
sys.argv = _argv
TYPES = _d["groups"]
ROWS = 6                                                     # groups per sheet
TW, TH = 560, 345                                            # one tile (a 1200 x 740 render)
LEFT, TOP, GAP, CAP = 230, 64, 14, 40
BG, INK, MUTE, ACC = (246, 243, 238), (43, 38, 34), (119, 107, 96), (138, 75, 47)


def font(size, bold=False):
    for f in (("segoeuib.ttf" if bold else "segoeui.ttf"), "arial.ttf"):
        try:
            return ImageFont.truetype(f, size)
        except OSError:
            pass
    return ImageFont.load_default()


def wrap(d, text, fnt, width):
    lines, cur = [], ""
    for w in text.split():
        t = (cur + " " + w).strip()
        if d.textlength(t, font=fnt) > width and cur:
            lines.append(cur)
            cur = w
        else:
            cur = t
    return lines + ([cur] if cur else [])


def sheets():
    groups = list(STYLES["groups"].items())
    n = (len(groups) + ROWS - 1) // ROWS
    for s in range(n):
        part = groups[s * ROWS:(s + 1) * ROWS]
        W = LEFT + 3 * TW + 2 * GAP + 20
        H = TOP + len(part) * (TH + CAP + GAP) + 20
        sheet = Image.new("RGB", (W, H), BG)
        d = ImageDraw.Draw(sheet)
        d.text((20, 16), "Building styles 22 (%d of %d): every build type that is not a house, three colourways each (look test, not approved)"
               % (s + 1, n), font=font(24, True), fill=INK)
        for c, label in enumerate(("a  (as the kit draws it now)", "b", "c")):
            d.text((LEFT + c * (TW + GAP), TOP - 6), label, font=font(16), fill=MUTE)
        for r, (gkey, g) in enumerate(part):
            y = TOP + 18 + r * (TH + CAP + GAP)
            yy = y + 10
            for ln in wrap(d, g["title"], font(20, True), LEFT - 30):
                d.text((20, yy), ln, font=font(20, True), fill=ACC)
                yy += 26
            yy += 6
            for ln in wrap(d, "types: " + ", ".join(g["types"]), font(13), LEFT - 30):
                d.text((20, yy), ln, font=font(13), fill=MUTE)
                yy += 18
            for c, w in enumerate("abc"):
                x = LEFT + c * (TW + GAP)
                p = os.path.join(DIR, "%s_%s.png" % (gkey, w))
                if os.path.exists(p):
                    sheet.paste(Image.open(p).convert("RGB").resize((TW, TH), Image.LANCZOS), (x, y))
                else:
                    d.rectangle((x, y, x + TW, y + TH), outline=MUTE)
                    d.text((x + 20, y + TH // 2), "not rendered", font=font(16), fill=MUTE)
                name = STYLES["colourways"].get(gkey, {}).get(w, {}).get("name", "")
                for k, ln in enumerate(wrap(d, "%s_%s  %s" % (gkey, w, name), font(14), TW)[:2]):
                    d.text((x, y + TH + 4 + k * 17), ln, font=font(14), fill=INK)
        out = os.path.join(DIR, "sheet_%d.png" % (s + 1))
        sheet.save(out)
        print("wrote", out)


def to_front(src, dst):
    im = Image.open(src).convert("RGB")
    w, h = im.size
    if w / h > 1.5:                                          # crop the middle to 3:2
        nw = int(h * 1.5)
        im = im.crop(((w - nw) // 2, 0, (w - nw) // 2 + nw, h))
    else:
        nh = int(w / 1.5)
        im = im.crop((0, (h - nh) // 2, w, (h - nh) // 2 + nh))
    im.resize((900, 600), Image.LANCZOS).save(dst, "JPEG", quality=85)


def fronts():
    os.makedirs(FRONTS, exist_ok=True)
    of_type = {}
    for gkey, g in STYLES["groups"].items():
        for i, t in enumerate(g["types"]):
            of_type[t] = os.path.join(DIR, ("%s_a.png" % gkey) if i == 0 else ("front_%s.png" % t))
    missing, made = [], []
    for key, g in TYPES.items():
        if key.startswith("house_"):
            line = key.split("_")[-1]
            src = os.path.join(ROOT, "Art", "StyleTests", "house_styles_20", "%s_a.png" % line)
        elif g["section"] == "open":
            src = os.path.join(DIR, "open_%s.png" % key)
        else:
            src = of_type.get(key)
        if src and os.path.exists(src):
            to_front(src, os.path.join(FRONTS, key + ".jpg"))
            made.append(key)
        else:
            missing.append((key, src))
    print("fronts: %d made, %d missing" % (len(made), len(missing)))
    for k, s in missing:
        print("  missing", k, s)
    # a contact sheet of every front, in draw_builds' order
    cols, tw, th, cap = 6, 300, 200, 22
    keys = list(TYPES)
    rows = (len(keys) + cols - 1) // cols
    sheet = Image.new("RGB", (20 + cols * (tw + 10), 60 + rows * (th + cap + 10)), BG)
    d = ImageDraw.Draw(sheet)
    d.text((20, 16), "Building styles 22: the %d front pictures of the build catalogue (fronts/<type>.jpg)" % len(made), font=font(20, True), fill=INK)
    for i, k in enumerate(keys):
        x, y = 20 + (i % cols) * (tw + 10), 56 + (i // cols) * (th + cap + 10)
        p = os.path.join(FRONTS, k + ".jpg")
        if os.path.exists(p):
            sheet.paste(Image.open(p).resize((tw, th), Image.LANCZOS), (x, y))
        else:
            d.rectangle((x, y, x + tw, y + th), outline=MUTE)
        d.text((x, y + th + 2), k, font=font(13), fill=INK)
    sheet.save(os.path.join(DIR, "fronts_sheet.png"))
    print("wrote", os.path.join(DIR, "fronts_sheet.png"))
    return missing


def summary(missing):
    """render_times.txt: the last render time and build time of every job in the Blender log, and what is missing."""
    import re
    times, builds, failed = {}, {}, {}
    p = os.path.join(DIR, "building_styles_22_log.txt")
    lines = open(p, encoding="utf-8").read().splitlines() if os.path.exists(p) else []
    for i, ln in enumerate(lines):
        m = re.match(r"\S+ (\S+) rendered in ([\d.]+)s", ln)
        if m:
            times[m.group(1)] = float(m.group(2))
            failed.pop(m.group(1), None)
        m = re.match(r"\S+ (\S+) built in ([\d.]+)s", ln)
        if m:
            builds[m.group(1)] = float(m.group(2))
            failed.pop(m.group(1), None)
        m = re.match(r"\S+ FAILED (?:building )?(\S+):", ln)
        if m:
            failed[m.group(1)] = lines[i + 1:i + 12][-1] if i + 1 < len(lines) else ""
    out = ["Building styles 22: build and render times (seconds, from building_styles_22_log.txt; Cycles CPU, %s)" % "32 samples",
           "Times include other Blender jobs sharing the machine.", ""]
    out.append("%-28s %8s" % ("build (one per type)", "seconds"))
    out += ["%-28s %8.1f" % (k, v) for k, v in builds.items()]
    out += ["", "%-28s %8s" % ("render", "seconds")]
    out += ["%-28s %8.1f" % (k, v) for k, v in times.items()]
    out += ["", "total build %.0f s, render %.0f s, %d renders" % (sum(builds.values()), sum(times.values()), len(times))]
    out += ["", "failed (last attempt): " + (", ".join("%s (%s)" % kv for kv in failed.items()) or "none")]
    out += ["fronts missing: " + (", ".join(k for k, s in missing) or "none")]
    open(os.path.join(DIR, "render_times.txt"), "w", encoding="utf-8").write("\n".join(out) + "\n")
    print("wrote", os.path.join(DIR, "render_times.txt"))


if __name__ == "__main__":
    sheets()
    summary(fronts())
