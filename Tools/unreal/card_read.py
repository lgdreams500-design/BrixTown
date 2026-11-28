"""Reads the light on the grey card and the grey ball of each shot of a tour (ADR-0073; docs/process/LOOK_GATE.md, A
room's light). A room's light is measured on its card, Unreal against Unreal: never against a Blender picture.

  python Tools/unreal/card_read.py <folder of tour shots> [--write <readings.json>] [--against <readings.json>]

The game writes where a stop's card and ball are in its picture to the folder's tour.log ("tour stop <name>: card
x0,y0,x1,y1 ball x,y,r", shares of the picture's width and height; the ball's r a share of its width). For each such
stop this reads, in the picture as the player sees it:

  card     how bright the card is (linear, 0 to 1; a mid grey in an evenly lit picture is near 0.18) and how warm
           (red over blue, in stops). The card faces the camera, so this is the light on what faces the player.
  ball     the same on the ball, and "side": how much brighter its right half is than its left, in stops, which says
           where the light comes from. The ball is for the eye; the card is what passes or fails.

--write keeps the readings (the golden room's, once the owner has approved its pictures, are the light reference).
--against compares each stop with the reading of the same name in a kept file: a stop passes within PASS_BRIGHT stops
of brightness and PASS_WARM of warmth (ADR-0055's marks, kept by ADR-0073). Needs Pillow and numpy.
"""
import json
import os
import re
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import look_match as LM  # noqa: E402

PASS_BRIGHT, PASS_WARM = LM.PASS_BRIGHT, LM.PASS_WARM
INNER = 0.7                                                # the middle of the card and of the ball: their edges are left out
LINE = re.compile(r"tour stop (\S+): card ([\d.,-]+) ball ([\d.,-]+)")


def places(folder):
    """Where each stop's card and ball are in its picture, from the folder's tour.log."""
    out = {}
    try:
        with open(os.path.join(folder, "tour.log"), encoding="utf-8", errors="replace") as f:
            for line in f:
                m = LINE.search(line)
                if m:
                    out[m.group(1)] = (tuple(float(v) for v in m.group(2).split(",")), tuple(float(v) for v in m.group(3).split(",")))
    except OSError:
        pass
    return out


def read(path, card, ball):
    """The light on a picture's card (x0, y0, x1, y1) and ball (x, y, r), given as shares of the picture."""
    a = np.asarray(Image.open(path).convert("RGB"), float)
    h, w = a.shape[:2]
    x0, y0, x1, y1 = card
    mx, my = (x0 + x1) / 2, (y0 + y1) / 2
    dx, dy = (x1 - x0) * INNER / 2, (y1 - y0) * INNER / 2
    patch = a[max(0, round((my - dy) * h)):max(1, round((my + dy) * h)), max(0, round((mx - dx) * w)):max(1, round((mx + dx) * w))]
    bright, warm = LM._light(patch)
    out = {"card": round(bright, 4), "card_warm": round(warm, 2)}
    bx, by, br = ball[0] * w, ball[1] * h, ball[2] * w * INNER
    yy, xx = np.mgrid[0:h, 0:w]
    disc = (xx - bx) ** 2 + (yy - by) ** 2 <= br ** 2
    if disc.sum() >= 8:
        b, bw = LM._light(a[disc])
        left, right = a[disc & (xx < bx)], a[disc & (xx >= bx)]
        side = float(np.log2(LM._light(right)[0] / LM._light(left)[0])) if len(left) and len(right) else 0.0
        out.update({"ball": round(b, 4), "ball_warm": round(bw, 2), "ball_side": round(side, 2)})
    return out


def between(a, b):
    """How much brighter and warmer reading a's card is than reading b's, in stops, and whether that passes."""
    bright = float(np.log2(a["card"] / b["card"]))
    warm = a["card_warm"] - b["card_warm"]
    return {"bright": round(bright, 2), "warm": round(warm, 2), "passes": abs(bright) <= PASS_BRIGHT and abs(warm) <= PASS_WARM}


def folder_readings(folder):
    out = {}
    for stop, (card, ball) in sorted(places(folder).items()):
        path = os.path.join(folder, stop + ".png")
        if os.path.exists(path):
            out[stop] = read(path, card, ball)
    return out


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    folder = argv[1]
    got = folder_readings(folder)
    if not got:
        sys.exit("no stop of %s has a card: its tour.log names none (a stop in a golden room, refroom:home or refroom:hall)" % folder)
    kept = json.load(open(argv[argv.index("--against") + 1], encoding="utf-8")) if "--against" in argv else {}
    for stop, r in got.items():
        line = "%-24s card %.4f  warmth %+.2f" % (stop, r["card"], r["card_warm"])
        if "ball" in r:
            line += "   ball %.4f  warmth %+.2f  right over left %+.2f" % (r["ball"], r["ball_warm"], r["ball_side"])
        if stop in kept:
            d = between(r, kept[stop])
            line += "   against the kept: %+.2f / %+.2f%s" % (d["bright"], d["warm"], "  PASSES" if d["passes"] else "")
        print(line)
    if "--write" in argv:
        with open(argv[argv.index("--write") + 1], "w", encoding="utf-8") as f:
            json.dump(got, f, indent=1)
            f.write("\n")


if __name__ == "__main__":
    main(sys.argv)
