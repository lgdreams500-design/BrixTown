"""Puts the house_interiors_21 renders on two sheets, one per storey: a row per furnishing level, a column per line.
Runs in ordinary Python with Pillow (not in Blender), after house_interiors_21.py:
    python Tools/blender/house_interiors_21_sheet.py
Writes Art/StyleTests/house_interiors_21/sheet_ground.png and sheet_upstairs.png.
"""
import json, os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DIR = os.path.join(ROOT, "Art", "StyleTests", "house_interiors_21")
PLAN = json.load(open(os.path.join(DIR, "layout.json"), encoding="utf-8"))
LINES = [("plain", "Plain"), ("oldtown", "Old Town"), ("workshop", "Workshop"), ("curve", "Curve")]
CROP = (230, 140, 1270, 820)                                # the house in a 1500 x 960 render
TW, TH = 520, 340
LEFT, TOP, GAP, CAP = 170, 70, 12, 8


def font(size, bold=False):
    for f in (("segoeuib.ttf" if bold else "segoeui.ttf"), "arial.ttf"):
        try:
            return ImageFont.truetype(f, size)
        except OSError:
            pass
    return ImageFont.load_default()


for storey, title in (("ground", "Ground floor"), ("upstairs", "Upstairs")):
    W = LEFT + 4 * TW + 3 * GAP + 20
    H = TOP + 4 * (TH + CAP) + 3 * GAP + 20
    sheet = Image.new("RGB", (W, H), (246, 243, 238))
    d = ImageDraw.Draw(sheet)
    d.text((20, 14), "House interiors 21, %s: four lines, four furnishing levels (look test, not approved)" % title.lower(),
           font=font(24, True), fill=(43, 38, 34))
    for c, (key, name) in enumerate(LINES):
        d.text((LEFT + c * (TW + GAP), TOP - 26), name, font=font(18, True), fill=(43, 38, 34))
    for r, (lv, lname) in enumerate(sorted(PLAN["levels"].items(), key=lambda kv: int(kv[0]))):
        y = TOP + r * (TH + CAP + GAP)
        n = sum(1 for it in PLAN["items"] if it["storey"] == storey and it["level"] <= int(lv))
        d.text((20, y + TH // 2 - 26), "%s %s" % (lv, lname), font=font(22, True), fill=(138, 75, 47))
        d.text((20, y + TH // 2 + 4), "%d pieces" % n, font=font(16), fill=(119, 107, 96))
        for c, (key, name) in enumerate(LINES):
            p = os.path.join(DIR, "%s_%s_%s.png" % (key, lv, storey))
            if os.path.exists(p):
                sheet.paste(Image.open(p).convert("RGB").crop(CROP).resize((TW, TH), Image.LANCZOS), (LEFT + c * (TW + GAP), y))
    out = os.path.join(DIR, "sheet_%s.png" % storey)
    sheet.save(out)
    print("wrote", out)
