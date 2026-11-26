"""Measures how a picture of the game looks, so that a flat, burnt or unlit picture is caught when a thing is made
(docs/process/LOOK_GATE.md), not when the owner plays.

  python Tools/unreal/look_check.py <picture> [<picture> ...]            the numbers of each, and what fails
  python Tools/unreal/look_check.py --tour <folder>                      every picture of a look tour (light_tour.ps1)
  python Tools/unreal/look_check.py --tour <folder> --against <baseline.json>   and what has moved since the baseline
  python Tools/unreal/look_check.py --tour <folder> --write <baseline.json>     keep these numbers as the baseline

What is measured (all on the picture as saved, 0 to 255, brightness as Rec. 709 luma):
  burnt     share of the picture at 250 or more in any channel: whites with nothing in them
  crushed   share at 6 or less: blacks with nothing in them
  flat      share of the picture's 16-pixel squares with almost no change of brightness inside them (a deviation
            under 0.8 of a level). A plain fill with no shade, no mottle and no shadow is all such squares; a lit,
            mottled surface has almost none. The sky counts, so a wide sky reads as some flatness.
  shade     the stops between the dark tenth and the bright tenth of the picture: how much light and shade there is
  warmth    red over blue, in stops: the look is warm (ART_DIRECTION W4)
  figure    for a picture with the player standing in the middle (a tour stop "stand:" or "in:"): how many stops the
            figure is brighter (+) or darker (-) than the picture around them

A name ending _night, or an hour from 21:00 to 05:00 in a tour stop's name (square_2230), is judged as night: dark
is allowed there. The limits are at the top of the file; they are the look gate's, change them there and say why.
Needs Pillow.
"""
import json
import math
import os
import re
import sys

from PIL import Image, ImageChops, ImageStat

# the look gate's limits (docs/process/LOOK_GATE.md, L1 to L6)
BURNT_MAX = 0.04        # more of the picture than this burnt white fails
CRUSHED_MAX = 0.20      # by day; at night 0.75
CRUSHED_MAX_NIGHT = 0.75
FLAT_MAX = 0.45         # by day, more of the picture than this in flat squares fails
SHADE_MIN = 1.0         # stops between the dark and the bright tenth, by day
FIGURE_RANGE = (-2.0, 1.2)   # the player against the picture around them, in stops
DRIFT = {"burnt": 0.02, "crushed": 0.05, "flat": 0.08, "shade": 0.5, "warmth": 0.15, "figure": 0.6}

PLAYER_STOPS = re.compile(r"^(alley|row|rowup|house|shop|shopup|cafe|bank|library|tower_in|mall_in|court|outlet|stand)_")


def stops(a, b):
    return math.log2((a + 1.0) / (b + 1.0))


def measure(path, with_figure=None):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    n = w * h
    grey = im.convert("L")
    hist = grey.histogram()
    top = ImageChops.lighter(ImageChops.lighter(im.getchannel("R"), im.getchannel("G")), im.getchannel("B"))
    burnt = sum(top.histogram()[250:]) / n
    crushed = sum(hist[:7]) / n
    # percentiles of brightness
    def pct(p):
        want, run = p * n, 0
        for v, c in enumerate(hist):
            run += c
            if run >= want:
                return v
        return 255
    shade = stops(pct(0.9), pct(0.1))
    # flat squares: 16 x 16 with a deviation of brightness under 0.8
    flat = total = 0
    step = 16
    px = grey.load()
    for y in range(0, h - step + 1, step):
        for x in range(0, w - step + 1, step):
            s = s2 = 0
            for yy in range(y, y + step, 2):
                for xx in range(x, x + step, 2):
                    v = px[xx, yy]
                    s += v
                    s2 += v * v
            m = s / 64.0
            var = s2 / 64.0 - m * m
            total += 1
            if var < 0.64:
                flat += 1
    mean = ImageStat.Stat(im).mean
    out = {"burnt": round(burnt, 4), "crushed": round(crushed, 4), "flat": round(flat / max(total, 1), 4),
           "shade": round(shade, 2), "warmth": round(stops(mean[0], mean[2]), 3)}
    name = os.path.splitext(os.path.basename(path))[0]
    if with_figure if with_figure is not None else bool(PLAYER_STOPS.match(name)):
        box = grey.crop((int(w * 0.44), int(h * 0.42), int(w * 0.56), int(h * 0.88)))
        fig = ImageStat.Stat(box).median[0]
        ring = ImageStat.Stat(grey).median[0]
        out["figure"] = round(stops(fig, ring), 2)
    return out


def is_night(name):
    if name.endswith("_night"):
        return True
    m = re.search(r"_(\d{2})(\d{2})(?:[a-z]?)$", name)
    return bool(m) and (int(m.group(1)) >= 21 or int(m.group(1)) < 5)


def judge(name, m):
    night = is_night(name)
    bad = []
    if m["burnt"] > BURNT_MAX:
        bad.append(f"burnt whites over {m['burnt']:.0%} of the picture (limit {BURNT_MAX:.0%})")
    if m["crushed"] > (CRUSHED_MAX_NIGHT if night else CRUSHED_MAX):
        bad.append(f"crushed blacks over {m['crushed']:.0%}")
    if not night and m["flat"] > FLAT_MAX:
        bad.append(f"flat: {m['flat']:.0%} of the picture has no shade or mottle in it (limit {FLAT_MAX:.0%})")
    if not night and m["shade"] < SHADE_MIN:
        bad.append(f"look at it: little light and shade, {m['shade']} stops (a scene in the look has {SHADE_MIN} or more; a white-out in haze reads so)")
    if "figure" in m and not FIGURE_RANGE[0] <= m["figure"] <= FIGURE_RANGE[1]:
        bad.append(f"the player is {m['figure']:+} stops from the picture around them (allowed {FIGURE_RANGE[0]} to +{FIGURE_RANGE[1]})")
    return bad


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    args = argv[1:]
    against = write = None
    files = []
    while args:
        a = args.pop(0)
        if a == "--tour":
            d = args.pop(0)
            files += sorted(os.path.join(d, f) for f in os.listdir(d) if f.lower().endswith(".png"))
        elif a == "--against":
            against = args.pop(0)
        elif a == "--write":
            write = args.pop(0)
        else:
            files.append(a)
    base = json.load(open(against, encoding="utf-8")) if against else {}
    got, failed = {}, 0
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        m = measure(f)
        got[name] = m
        bad = judge(name, m)
        for k, lim in DRIFT.items():
            if name in base and k in m and k in base[name] and abs(m[k] - base[name][k]) > lim:
                bad.append(f"{k} moved from {base[name][k]} to {m[k]} since the baseline")
        failed += bool(bad)
        print(f"{name:24s} " + "  ".join(f"{k} {v}" for k, v in m.items()) + ("" if not bad else "\n    FAILS: " + "; ".join(bad)))
    if write:
        json.dump(got, open(write, "w", encoding="utf-8"), indent=1, sort_keys=True)
        print(f"baseline of {len(got)} pictures written to {write}")
    print(f"{len(files)} pictures, {failed} fail")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
