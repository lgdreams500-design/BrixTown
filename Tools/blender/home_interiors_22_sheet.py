"""Puts the home_interiors_22 renders on one sheet per home: a row per furnishing level, a column per line, for the
first floor (the ground floor, or the flat), and a last row with the floors above at Full, one per line.
Runs in ordinary Python with Pillow (not in Blender), after home_interiors_22.py:
    python Tools/blender/home_interiors_22_sheet.py
Writes Art/StyleTests/home_interiors_22/sheet_<type>.png.
"""
import json, os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DIR = os.path.join(ROOT, "Art", "StyleTests", "home_interiors_22")
TYPES = ["row_14x12_2", "row_14x12_3", "row_16x12_2", "flat_80", "flat_160"]
LINES = [("plain", "Plain"), ("oldtown", "Old Town"), ("workshop", "Workshop"), ("curve", "Curve")]
CROP = {"flat_160": (0, 0, 1500, 960)}                     # the home in a 1500 x 960 render (the long flat uses it all)
CROP_DEFAULT = (90, 60, 1410, 905)
TW, TH = 520, 333
LEFT, TOP, GAP, CAP = 170, 70, 12, 8


def font(size, bold=False):
    for f in (("segoeuib.ttf" if bold else "segoeui.ttf"), "arial.ttf"):
        try:
            return ImageFont.truetype(f, size)
        except OSError:
            pass
    return ImageFont.load_default()


for t in TYPES:
    plan = json.load(open(os.path.join(DIR, "layout_%s.json" % t), encoding="utf-8"))
    hp = plan["house"]
    first, above = hp["floors"][0], hp["floors"][1:]
    rows = [(lv, first) for lv in sorted(plan["levels"], key=int)] + [("4", f) for f in above]
    W = LEFT + 4 * TW + 3 * GAP + 20
    H = TOP + len(rows) * (TH + CAP + GAP) + 10
    sheet = Image.new("RGB", (W, H), (246, 243, 238))
    d = ImageDraw.Draw(sheet)
    d.text((20, 14), "Home interiors 22, %s: four lines, four furnishing levels (look test, not approved)" % hp["title"],
           font=font(24, True), fill=(43, 38, 34))
    for c, (key, name) in enumerate(LINES):
        d.text((LEFT + c * (TW + GAP), TOP - 26), name, font=font(18, True), fill=(43, 38, 34))
    for r, (lv, f) in enumerate(rows):
        y = TOP + r * (TH + CAP + GAP)
        n = sum(1 for it in plan["items"] if it["storey"] == f and it["level"] <= int(lv))
        d.text((20, y + TH // 2 - 40), "%s %s" % (lv, plan["levels"][lv]), font=font(22, True), fill=(138, 75, 47))
        d.text((20, y + TH // 2 - 10), f, font=font(18, True), fill=(43, 38, 34))
        d.text((20, y + TH // 2 + 16), "%d pieces" % n, font=font(16), fill=(119, 107, 96))
        for c, (key, name) in enumerate(LINES):
            p = os.path.join(DIR, "%s_%s_%s_%s.png" % (t, key, lv, f))
            if os.path.exists(p):
                sheet.paste(Image.open(p).convert("RGB").crop(CROP.get(t, CROP_DEFAULT)).resize((TW, TH), Image.LANCZOS), (LEFT + c * (TW + GAP), y))
    out = os.path.join(DIR, "sheet_%s.png" % t)
    sheet.save(out)
    print("wrote", out)
