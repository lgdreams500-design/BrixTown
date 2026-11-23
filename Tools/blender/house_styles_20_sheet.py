"""Puts the twelve house_styles_20 renders on one sheet: a row per house line, a column per colourway, each labelled.
Runs in ordinary Python with Pillow (not in Blender), after house_styles_20.py:
    python Tools/blender/house_styles_20_sheet.py
Writes Art/StyleTests/house_styles_20/sheet.png. With WASTED_SHEET=house_outside_23 it makes that test's sheet instead.
"""
import json, os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
NAME = os.environ.get("WASTED_SHEET", "house_styles_20")
DIR = os.path.join(ROOT, "Art", "StyleTests", NAME)
STYLES = json.load(open(os.path.join(ROOT, "World", "rules", "house_styles.json"), encoding="utf-8"))
LINES = [("plain", "Plain"), ("oldtown", "Old Town"), ("workshop", "Workshop"), ("curve", "Curve")]
CROP = (130, 90, 1130, 700)                                  # the house and garden in a 1200 x 740 render
TW, TH = 600, 366                                            # one tile
LEFT, TOP, GAP, CAP = 150, 60, 14, 34


def way_names(key):
    return {w: c["name"] for w, c in STYLES["colourways"][key].items()}


def font(size, bold=False):
    for f in (("segoeuib.ttf" if bold else "segoeui.ttf"), "arial.ttf"):
        try:
            return ImageFont.truetype(f, size)
        except OSError:
            pass
    return ImageFont.load_default()


W = LEFT + 3 * TW + 2 * GAP + 20
H = TOP + 4 * (TH + CAP) + 3 * GAP + 20
sheet = Image.new("RGB", (W, H), (246, 243, 238))
d = ImageDraw.Draw(sheet)
d.text((20, 16), ("House styles 20: four house lines, three colourways each (all twelve approved, ADR-0032)" if NAME == "house_styles_20" else
                 "House outside 23: the twelve colourways on the house at 12 x 9 m with the plan's windows (ADR-0032, ADR-0035)"), font=font(24, True), fill=(43, 38, 34))
for c, label in enumerate(("a  (as now)", "b", "c")):
    d.text((LEFT + c * (TW + GAP), TOP - 2), label, font=font(16), fill=(119, 107, 96))
for r, (key, title) in enumerate(LINES):
    y = TOP + 22 + r * (TH + CAP + GAP)
    d.text((20, y + TH // 2 - 14), title, font=font(22, True), fill=(138, 75, 47))
    names = way_names(key)
    for c, w in enumerate("abc"):
        x = LEFT + c * (TW + GAP)
        p = os.path.join(DIR, "%s_%s.png" % (key, w))
        if os.path.exists(p):
            sheet.paste(Image.open(p).convert("RGB").crop(CROP).resize((TW, TH), Image.LANCZOS), (x, y))
        d.text((x, y + TH + 6), "%s_%s  %s" % (key, w, names.get(w, "")), font=font(16), fill=(43, 38, 34))
sheet.save(os.path.join(DIR, "sheet.png"))
print("wrote", os.path.join(DIR, "sheet.png"))
