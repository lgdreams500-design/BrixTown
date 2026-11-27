"""Brix Town - light kinds 54, the sheet: every kind of light of World/rules/lights.json on one picture, from the renders
of light_kinds_54.py. For each kind two pictures of the same room: on top at one exposure for all kinds (the lamp-lit
night's), so their strengths can be told apart; under it with the eye opened or closed for that kind.

    python Tools/blender/light_kinds_54_sheet.py [folder of the renders]     (plain Python with Pillow, not Blender)
Writes LightKinds_sheet.jpg into that folder (default Art/StyleTests/light_kinds_54).
"""
import json, os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "Art", "StyleTests", "light_kinds_54")
KINDS = [("FloorLamp", "floor_lamp"), ("TableLamp", "table_lamp"), ("DeskLamp", "desk_lamp"), ("WallLight", "wall_light"),
         ("Pendant", "ceiling_pendant"), ("CeilingLight", "ceiling_light"), ("Candles", "candles"),
         ("CoolCeilingLight", "cool_ceiling_light"), ("WallLantern", "wall_lantern"), ("NeonBlue", "neon_blue"),
         ("NeonGreen", "neon_green"), ("NeonRed", "neon_red"), ("NeonAmber", "neon_amber")]
COLS, W, H, PAD, LABEL = 5, 480, 300, 10, 44


def font(size):
    for f in ("bahnschrift.ttf", "segoeui.ttf", "arial.ttf"):
        try:
            return ImageFont.truetype(f, size)
        except OSError:
            pass
    return ImageFont.load_default()


def main():
    with open(os.path.join(HERE, "World", "rules", "lights.json"), encoding="utf-8") as f:
        rules = json.load(f)
    kinds, colours = rules["kinds"], rules["colours"]
    have = [(n, k) for n, k in KINDS if os.path.exists(os.path.join(SRC, "Fixed_%s.jpg" % n))]
    rows = (len(have) + COLS - 1) // COLS
    cell_h = LABEL + 2 * H + PAD
    sheet = Image.new("RGB", (PAD + COLS * (W + PAD), PAD + rows * (cell_h + PAD)), (28, 26, 24))
    d = ImageDraw.Draw(sheet)
    big, small = font(19), font(14)
    for i, (name, kid) in enumerate(have):
        k = kinds[kid]
        col = colours[k["colour"]]
        x, y = PAD + (i % COLS) * (W + PAD), PAD + (i // COLS) * (cell_h + PAD)
        sw = tuple(int(255 * (c ** (1 / 2.2))) for c in col["colour"])          # the kind's colour, as the screen shows it
        d.rectangle([x, y + 4, x + 30, y + LABEL - 8], fill=sw)
        d.text((x + 40, y), k["name"], font=big, fill=(239, 228, 204))
        d.text((x + 40, y + 23), "%s%s, %.0f W, on the %s" % (col["name"], (" %d K" % col["kelvin"]) if col["kelvin"] else "", k["strength"], k["mount"]),
               font=small, fill=(170, 160, 140))
        for j, which in enumerate(("Fixed", "Eye")):
            p = os.path.join(SRC, "%s_%s.jpg" % (which, name))
            if os.path.exists(p):
                sheet.paste(Image.open(p).convert("RGB").resize((W, H), Image.LANCZOS), (x, y + LABEL + j * H))
            d.text((x + 6, y + LABEL + j * H + 4), "one exposure for all" if which == "Fixed" else "the eye set for this light",
                   font=small, fill=(239, 228, 204))
    out = os.path.join(SRC, "LightKinds_sheet.jpg")
    sheet.save(out, quality=88)
    print(out, sheet.size)


main()
