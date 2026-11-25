"""Measures an Unreal shot of a Blender reference room against the reference itself (ADR-0055).

The game builds the reference rooms of Tools/blender/furniture_kit_18.py again in Unreal (ABrixLightRoom, -BrixLightRoom)
and a tour stop "refroom:<shop|cafe|kitchen>" looks through the reference's own camera, so the two pictures show the same
walls, floor, ceiling and furniture from the same place under the same sun. This compares them where it matters: the same
patches of wall, floor and ceiling in both, each as its mean colour, how much brighter Unreal's is in stops (0 is the same;
the pictures' own tone curves differ, so shadows and highlights never agree exactly) and how much warmer (red over blue,
in stops). It also writes the two pictures side by side with the patches drawn on them.

  python Tools/unreal/light_reference.py <reference.png> <unreal.png> [<out.jpg>]

Needs Pillow and numpy.
"""
import sys

import numpy as np
from PIL import Image, ImageDraw

# Patches of the shell every reference room shares (room_shell() of furniture_kit_17.py), as fractions of the picture:
# x0, y0, x1, y1. Chosen clear of the furniture in the shop, the cafe and the kitchen.
PATCHES = {
    "left wall, near":   (0.03, 0.42, 0.10, 0.58),
    "left wall, by lamp": (0.13, 0.36, 0.21, 0.48),
    "back wall, high":   (0.41, 0.33, 0.47, 0.40),
    "right wall":        (0.90, 0.30, 0.97, 0.42),
    "ceiling":           (0.31, 0.09, 0.42, 0.18),
    "floor, shade":      (0.06, 0.86, 0.24, 0.96),
    "floor, far":        (0.30, 0.60, 0.38, 0.64),
}
SIZE = (800, 500)


def linear(srgb):
    c = srgb / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def patch(im, box):
    x0, y0, x1, y1 = box
    a = im[int(y0 * SIZE[1]):int(y1 * SIZE[1]), int(x0 * SIZE[0]):int(x1 * SIZE[0])].reshape(-1, 3)
    return a.mean(0)


def sun_patch(im):
    """The sunlit floor: the brightest fiftieth of the lower part of the picture (the furniture moves it about)."""
    a = im[int(0.58 * SIZE[1]):int(0.92 * SIZE[1]), int(0.25 * SIZE[0]):int(0.80 * SIZE[0])].reshape(-1, 3)
    lum = a @ np.array([0.2126, 0.7152, 0.0722])
    return a[lum >= np.percentile(lum, 98)].mean(0)


def measure(path):
    im = np.asarray(Image.open(path).convert("RGB").resize(SIZE, Image.LANCZOS)).astype(float)
    out = {name: patch(im, box) for name, box in PATCHES.items()}
    out["floor, in the sun"] = sun_patch(im)
    out["the whole picture"] = im.reshape(-1, 3).mean(0)
    return out


def lum(c):
    return float(linear(np.asarray(c)) @ np.array([0.2126, 0.7152, 0.0722]))


def warmth(c):
    l = linear(np.asarray(c))
    return float(np.log2(max(l[0], 1e-4) / max(l[2], 1e-4)))


def main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    ref, got = measure(argv[1]), measure(argv[2])
    print(f"{'':22}{'reference':>16}{'Unreal':>16}{'brighter':>10}{'warmer':>9}")
    total = []
    for name in ref:
        r, g = ref[name], got[name]
        stops = float(np.log2(max(lum(g), 1e-4) / max(lum(r), 1e-4)))
        warm = warmth(g) - warmth(r)
        if name != "the whole picture":
            total.append((stops, warm))
        print(f"{name:22}{'(%3d,%3d,%3d)' % tuple(r):>16}{'(%3d,%3d,%3d)' % tuple(g):>16}{stops:>+10.2f}{warm:>+9.2f}")
    s = np.array(total)
    print(f"{'on average':22}{'':16}{'':16}{s[:, 0].mean():>+10.2f}{s[:, 1].mean():>+9.2f}   spread {s[:, 0].std():.2f} stops")
    if len(argv) > 3:
        a = Image.open(argv[1]).convert("RGB").resize(SIZE, Image.LANCZOS)
        b = Image.open(argv[2]).convert("RGB").resize(SIZE, Image.LANCZOS)
        sheet = Image.new("RGB", (SIZE[0] * 2, SIZE[1]))
        for k, im in enumerate((a, b)):
            d = ImageDraw.Draw(im)
            for name, (x0, y0, x1, y1) in PATCHES.items():
                d.rectangle([x0 * SIZE[0], y0 * SIZE[1], x1 * SIZE[0], y1 * SIZE[1]], outline=(60, 200, 255))
            d.rectangle([0, 0, 150, 16], fill=(0, 0, 0))
            d.text((4, 2), "Blender reference" if k == 0 else "Unreal", fill=(255, 255, 255))
            sheet.paste(im, (k * SIZE[0], 0))
        sheet.save(argv[3], quality=88)


if __name__ == "__main__":
    main(sys.argv)
