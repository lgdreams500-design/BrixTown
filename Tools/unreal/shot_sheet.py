"""Puts test shots side by side on one sheet, each with its label and its mean brightness (0 to 255), to compare a look
before and after or through the day (the shots of Tools/unreal/light_tour.ps1, or any others).

  python Tools/unreal/shot_sheet.py <out.jpg|png> <columns> <tile width px> <label>=<picture> [<label>=<picture> ...]

A picture that is missing is left out. Needs Pillow.
"""
import sys

from PIL import Image, ImageDraw, ImageStat


def main(argv):
    if len(argv) < 5:
        sys.exit(__doc__)
    out, cols, width = argv[1], int(argv[2]), int(argv[3])
    tiles = []
    for item in argv[4:]:
        label, path = item.split("=", 1)
        try:
            im = Image.open(path).convert("RGB")
        except OSError:
            continue
        mean = sum(ImageStat.Stat(im).mean) / 3
        im = im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
        text = f"{label}  {mean:.0f}"
        d = ImageDraw.Draw(im)
        d.rectangle([0, 0, 8 + 7 * len(text), 16], fill=(0, 0, 0))
        d.text((4, 2), text, fill=(255, 255, 255))
        tiles.append(im)
    if not tiles:
        sys.exit("no pictures")
    high = max(t.height for t in tiles)
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * width, rows * high), (30, 30, 30))
    for i, t in enumerate(tiles):
        sheet.paste(t, ((i % cols) * width, (i // cols) * high))
    sheet.save(out, quality=88)
    print(out, sheet.size)


if __name__ == "__main__":
    main(sys.argv)
