"""Measures every shot of a tour against its own place's reference and writes the list of what to mend, worst first
(docs/process/LOOK_GATE.md, The matching round). Numbers first: open a picture only for the rows at the top.

  python Tools/unreal/look_match.py <folder of tour shots> [--json <worklist.json>]

Each shot's name is a tour stop's (bakery_1200, hall_2230). Art/Reference/catalog.json's places say which reference
the stop is judged against (Tools/reference_catalog.py); the hour or the weather in the name picks the file. Then:

  matched   the stop looks through its reference's own camera (the place's "camera" in the catalog). The two pictures
            are laid over each other (the reference cut to the game's field of view) and compared square by square,
            4 across by 3 down: how much brighter the game is in stops, how much warmer (red over blue, in stops).
            "bright" and "warm" are the mean size of the difference over the squares, "worst" the worst square.
  loose     the stop shows the same kind of place from another spot. Only the whole picture is compared: brightness,
            warmth, and light and shade (the stops between its bright and dark tenths). A whole picture can agree
            while its parts are far out (the bakery, 2026-11-27: the same brightness overall, its back wall 3 stops
            under), so a loose row that passes proves little; give the place a matched stop.
  none      the place has no approved reference, or the name's hour is tied to none of its files: not judged.

A row passes within PASS_BRIGHT stops of brightness and PASS_WARM of warmth (ADR-0055's "half a stop in brightness and
a third of a stop in warmth"). The frame rate of each stop is read from the folder's tour.log. Needs Pillow and numpy.
"""
import json
import os
import re
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import reference_catalog as RC  # noqa: E402

PASS_BRIGHT, PASS_WARM = 0.5, 0.33
SIZE = (480, 300)
GRID = (4, 3)


def _linear(a):
    c = a / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def _load(path, keep=1.0):
    im = Image.open(path).convert("RGB")
    if keep < 1.0:                                   # the reference's lens is wider: its middle is the game's field
        w, h = im.size
        dx, dy = w * (1 - keep) / 2, h * (1 - keep) / 2
        im = im.crop((round(dx), round(dy), round(w - dx), round(h - dy)))
    return np.asarray(im.resize(SIZE, Image.LANCZOS), float)


def _light(a):
    """Brightness (linear luma) and warmth (red over blue, stops) of a patch of picture."""
    lin = _linear(a.reshape(-1, 3))
    m = lin.mean(axis=0) + 1e-4
    return float(np.mean(lin @ np.array([0.2126, 0.7152, 0.0722])) + 1e-4), float(np.log2(m[0] / m[2]))


def _shade(a):
    lum = a.reshape(-1, 3) @ np.array([0.2126, 0.7152, 0.0722])
    p10, p90 = np.percentile(lum, [10, 90])
    return float(np.log2((p90 + 1) / (p10 + 1)))


def compare(ref_path, shot_path, keep=None):
    """The differences, game against reference. keep: the share of the reference that is the game's field of view,
    given only when the stop looks through the reference's own camera."""
    ref, shot = _load(ref_path, keep or 1.0), _load(shot_path)
    rb, rw = _light(ref)
    sb, sw = _light(shot)
    out = {"bright": float(np.log2(sb / rb)), "warm": sw - rw, "shade": _shade(shot) - _shade(ref)}
    if keep:
        cw, ch = SIZE[0] // GRID[0], SIZE[1] // GRID[1]
        cells = []
        for gy in range(GRID[1]):
            for gx in range(GRID[0]):
                r, s = (_light(p[gy * ch:(gy + 1) * ch, gx * cw:(gx + 1) * cw]) for p in (ref, shot))
                cells.append((float(np.log2(s[0] / r[0])), s[1] - r[1]))
        out["cells"] = cells
        out["bright"] = float(np.mean([abs(c[0]) for c in cells]))
        out["warm"] = float(np.mean([abs(c[1]) for c in cells]))
        worst = max(range(len(cells)), key=lambda i: abs(cells[i][0]))
        out["worst"] = {"row": worst // GRID[0] + 1, "column": worst % GRID[0] + 1, "bright": cells[worst][0]}
    return out


def frame_rates(folder):
    out, log = {}, os.path.join(folder, "tour.log")
    if os.path.isfile(log):
        with open(log, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = re.search(r"tour stop (\S+) at .* ([\d.]+) frames a second", line)
                if m:
                    out[m.group(1)] = float(m.group(2))
    return out


def judge(folder, cat=None):
    """One row a shot: {stop, place, reference, how, bright, warm, shade, gap, passes, fps}, worst first."""
    cat = cat or RC.load()
    refs, fps, rows = {r["id"]: r for r in cat["references"]}, frame_rates(folder), []
    for name in sorted(os.listdir(folder)):
        stop, ext = os.path.splitext(name)
        if ext.lower() != ".png":
            continue
        hit, _ = RC.places_for(cat, stop)
        place = hit[0] if hit else None
        row = {"stop": stop, "place": place["place"] if place else None, "reference": None, "how": "none", "fps": fps.get(stop)}
        f = RC.setting_file(refs[place["reference"]], stop) if place and place.get("reference") else None
        if f:
            cam = place.get("camera") or {}
            matched = RC.stop_key(stop) in cam.get("stops", [])
            d = compare(os.path.join(RC.REF, f), os.path.join(folder, name), cam.get("keep", 1.0) if matched else None)
            row.update(reference=f, how="matched" if matched else "loose", bright=round(d["bright"], 2), warm=round(d["warm"], 2),
                       shade=round(d["shade"], 2), worst=d.get("worst"))
            row["gap"] = round(max(abs(d["bright"]) / PASS_BRIGHT, abs(d["warm"]) / PASS_WARM), 2)
            row["passes"] = row["gap"] <= 1.0
        rows.append(row)
    return sorted(rows, key=lambda r: -(r.get("gap") or -1))


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    rows = judge(argv[1])
    print("%-18s %-15s %-8s %7s %7s %7s %6s %5s  %s" % ("stop", "place", "how", "bright", "warm", "shade", "gap", "fps", "reference"))
    for r in rows:
        if r["how"] == "none":
            print("%-18s %-15s %-8s %7s %7s %7s %6s %5s  %s" % (r["stop"], r["place"] or "?", "none", "", "", "", "", "%.0f" % r["fps"] if r["fps"] else "", "not judged: no reference, or no file for this hour"))
            continue
        w = r.get("worst")
        print("%-18s %-15s %-8s %+7.2f %+7.2f %+7.2f %6.1f %5s  %s%s%s" % (
            r["stop"], r["place"], r["how"], r["bright"], r["warm"], r["shade"], r["gap"], "%.0f" % r["fps"] if r["fps"] else "",
            r["reference"], "  PASSES" if r["passes"] else "", "  worst square: row %d column %d, %+.1f stops" % (w["row"], w["column"], w["bright"]) if w else ""))
    judged = [r for r in rows if r["how"] != "none"]
    print("%d shots: %d matched, %d loose, %d not judged; %d pass" % (len(rows), sum(r["how"] == "matched" for r in rows),
          sum(r["how"] == "loose" for r in rows), len(rows) - len(judged), sum(bool(r.get("passes")) for r in judged)))
    if "--json" in argv:
        with open(argv[argv.index("--json") + 1], "w", encoding="utf-8") as f:
            json.dump(rows, f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
