"""Brix Town - painterly post-process for style-test renders.

Usage: python paint_filter.py <input.png> <output.png> [radius]

A Kuwahara filter (flat, brush-like patches with kept edges) followed by a
split-tone grade (cool shadows, warm lights), light bristle texture and a vignette.
This previews what a real-time post-process material would do in Unreal.
"""
import sys
import numpy as np
import cv2


def kuwahara(img, r):
    """Classic 4-quadrant Kuwahara on a float32 BGR image. r must be even."""
    k = r + 1
    lum = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    mean = cv2.blur(img, (k, k), borderType=cv2.BORDER_REFLECT)
    m1 = cv2.blur(lum, (k, k), borderType=cv2.BORDER_REFLECT)
    m2 = cv2.blur(lum * lum, (k, k), borderType=cv2.BORDER_REFLECT)
    var = m2 - m1 * m1
    h = r // 2
    best_var = None
    out = None
    for dy in (-h, h):
        for dx in (-h, h):
            v = np.roll(var, (dy, dx), axis=(0, 1))
            m = np.roll(mean, (dy, dx), axis=(0, 1))
            if out is None:
                best_var, out = v.copy(), m.copy()
            else:
                mask = v < best_var
                best_var[mask] = v[mask]
                out[mask] = m[mask]
    return out


def paint(src, radius=6):
    img = src.astype(np.float32) / 255.0
    # two passes: broad strokes, then a finer pass blended back where there is detail
    broad = kuwahara(kuwahara(img, radius), radius)
    fine = kuwahara(img, max(2, radius // 2 // 2 * 2))
    lum = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    edges = cv2.GaussianBlur(np.abs(cv2.Laplacian(cv2.GaussianBlur(lum, (0, 0), 1.5), cv2.CV_32F)), (0, 0), 3)
    w = np.clip(edges * 40.0, 0, 1)[..., None]
    out = broad * (1 - w) + fine * w

    # split tone: push shadows to blue-violet, lights to warm cream (BGR order)
    l = cv2.cvtColor(out, cv2.COLOR_BGR2GRAY)[..., None]
    shadow = np.array([0.085, 0.010, 0.020], np.float32)
    light = np.array([-0.045, 0.012, 0.045], np.float32)
    # cool the shadows and mid-tones first (less red, more blue), keep the lights warm
    cool = (1 - l) ** 1.2
    out = out * (1 - cool * np.array([-0.10, 0.02, 0.16], np.float32))
    out = out + shadow * (1 - l) ** 2 + light * l ** 1.5
    # gentle contrast curve and saturation
    out = np.clip(out, 0, 1)
    out = out * out * (3 - 2 * out) * 0.55 + out * 0.45
    hsv = cv2.cvtColor(out, cv2.COLOR_BGR2HSV)
    hsv[..., 1] = np.clip(hsv[..., 1] * 1.0, 0, 1)
    out = cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)

    # bristle texture: stretched noise, stronger in flat areas
    rng = np.random.default_rng(3)
    hgt, wid = l.shape[:2]
    n = rng.standard_normal((hgt // 2, wid // 6)).astype(np.float32)
    n = cv2.resize(n, (wid, hgt), interpolation=cv2.INTER_CUBIC)
    n = cv2.GaussianBlur(n, (0, 0), 0.8)
    out = out * (1 + 0.035 * n[..., None] * (1 - 0.6 * w))

    # vignette
    yy, xx = np.mgrid[0:hgt, 0:wid].astype(np.float32)
    d = np.sqrt(((xx - wid / 2) / (wid / 2)) ** 2 + ((yy - hgt / 2) / (hgt / 2)) ** 2)
    out = out * (1 - 0.22 * np.clip(d - 0.55, 0, 1) ** 1.5)[..., None]
    return (np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8)


if __name__ == "__main__":
    src = cv2.imread(sys.argv[1], cv2.IMREAD_COLOR)
    radius = int(sys.argv[3]) if len(sys.argv) > 3 else 6
    cv2.imwrite(sys.argv[2], paint(src, radius))
