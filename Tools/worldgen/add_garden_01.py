"""Plant city_01's parks and court gardens (LIVING_CITY CITY-017, WORLD_MAP WLD-040; F-GARDEN level 1, the owner's pick of
2026-11-22: "1 Living garden").

Adds props of kind "plant" to the block files, each marked "set": "garden", so that running this again first takes away
what it added before and then puts the same things back (running it twice changes nothing):
  hedge    a low clipped box hedge along each side of a garden, open at the corners, at the middle of each side and
           wherever a bench, a seat or a path needs the room
  border   a strip of flowers in front of each length of hedge, of two kinds that flower at different times
  bush     flowering shrubs (lilac, hydrangea, rose) grouped in the corners, and a few standing alone on the lawn
  drift    bulbs coming up under the trees, and asters out on the lawn
  grounds  in a park block, groups of shrubs with bulbs round them on the outer lawn between the pavement and the
           path round the garden
What each kind looks like on a day comes from World/rules/vegetation.json "plants" (citysim Vegetation.plant). The
gardener tends the borders: a place to stand at each is added to the lot's "flowerbed" places (choices.json: tend).

Run from Tools/worldgen:   py add_garden_01.py
                           py add_garden_01.py --new   plants only the gardens with nothing planted yet (gardens added
                                                       since, such as the remodel's), leaving the others as they are
Then:                      py -m unittest test_world ;  py build_play.py
"""
import math, os
from add_dressing_01 import CITY, Room, jload, jsave, rnd

SET = "garden"
HEDGE_IN, HEDGE_D = .3, .55                   # hedge: in from the lot's edge, and its depth (m)
BORDER_D = .9                                 # border depth in front of the hedge
BORDER_MIX = (("lavender", "rose"), ("tulip", "aster"), ("lavender", "aster"), ("daffodil", "rose"), ("tulip", "lavender"))
SHRUBS = ("lilac", "hydrangea", "rose")


def add(b, pos, shape, species, **kw):
    p = dict(kind="plant", pos=[round(pos[0], 2), round(pos[1], 2)], set=SET, shape=shape, species=species)
    p.update({k: (round(v, 2) if isinstance(v, float) else v) for k, v in kw.items()})
    p["id"] = "%s_g%d" % (b["id"], sum(1 for q in b["props"] if q.get("set") == SET))
    b["props"].append(p)
    return p


def seg_dist(px, pz, a, q):
    dx, dz = q[0] - a[0], q[1] - a[1]
    L = dx * dx + dz * dz or 1.0
    t = max(0.0, min(1.0, ((px - a[0]) * dx + (pz - a[1]) * dz) / L))
    return math.hypot(px - a[0] - t * dx, pz - a[1] - t * dz)


class Ground:
    """Where planting may go in one garden: not on a path, not on a seat, bench, bed or anything standing there."""
    def __init__(self, b, lot):
        self.paths = b["paths"]
        self.hard = []                                                     # (x, z, r): what planting keeps clear of
        for p in b["props"]:
            if p.get("set") == SET or p["kind"] in ("lantern", "arch"):
                continue
            r = {"fountain": p.get("r", 1.5) * 1.4 + .8, "flowerbed": 1.6, "bench": .45, "lamp": .6, "playground": 0,
                 "noticeboard": 1.4, "bandstand": p.get("r", 3) + 1.2, "pergola": 2.2, "well": 1.6, "statue": 1.6, "sundial": 1.1}.get(p["kind"], .9)
            if p["kind"] == "playground":                                  # its soft ground, as a rectangle of points
                for u in range(-9, 10, 2):
                    for v in range(-7, 8, 2):
                        self.hard.append((p["pos"][0] + u, p["pos"][1] + v, 1.2))
                continue
            self.hard.append((p["pos"][0], p["pos"][1], r))
        for k, v in lot.get("objects", {}).items():
            for q in v:
                self.hard.append((q[0], q[1], .65 if k in ("bench", "rim") else .5))
        self.trees = [(t["pos"][0], t["pos"][1]) for t in b["trees"]]

    def clear(self, x, z, r, trees=True):
        if any(seg_dist(x, z, p, q) < r + 1.2 for p, q in self.paths):
            return False
        if any(math.hypot(x - a, z - c) < r + d for a, c, d in self.hard):
            return False
        return not trees or all(math.hypot(x - a, z - c) > r + .9 for a, c in self.trees)

    def take(self, x, z, r):
        self.hard.append((x, z, r))


def runs(points, ok, step):
    """The unbroken lengths (start, end) of a line of sample points where ok() holds, each at least 3 m long."""
    out, start = [], None
    for i, t in enumerate(points):
        if ok(t):
            start = t if start is None else start
        if (not ok(t) or i == len(points) - 1) and start is not None:
            end = t if ok(t) else points[i - 1]
            if end - start >= 3.0:
                out.append((start, end))
            start = None
    return out


def plant_garden(b, lot, big):
    x0, z0, x1, z1 = lot["rect"]
    g = Ground(b, lot)
    tend = []
    n = dict(hedge=0, border=0, bush=0, drift=0)
    corner = 3.6 if big else 2.8
    for side in "nswe":                                                   # hedges, and a border in front of each
        along = "x" if side in "ns" else "z"
        lo, hi = (x0, x1) if along == "x" else (z0, z1)
        mid = (lo + hi) / 2
        inward = 1 if side in "nw" else -1
        edge = {"n": z0, "s": z1, "w": x0, "e": x1}[side]
        hline = edge + inward * (HEDGE_IN + HEDGE_D / 2)
        bline = edge + inward * (HEDGE_IN + HEDGE_D + .15 + BORDER_D / 2)
        pt = lambda t, f: (t, f) if along == "x" else (f, t)
        samples = [lo + corner + .25 * k for k in range(int((hi - lo - 2 * corner) / .25) + 1)]
        gap = 2.6 if big else 1.8
        ok_h = lambda t: abs(t - mid) > gap and g.clear(*pt(t, hline), HEDGE_D / 2 - .05, trees=False) and not any(math.hypot(pt(t, hline)[0] - a, pt(t, hline)[1] - c) < .9 for a, c in g.trees)
        for a, c in runs(samples, ok_h, .25):
            add(b, pt((a + c) / 2, hline), "hedge", "box", along=along, len=c - a + .25, depth=HEDGE_D, h=.75 if big else .62)
            n["hedge"] += 1
            for k in range(int((c - a) / .5) + 1):
                g.take(*pt(a + k * .5, hline), HEDGE_D / 2)
        mix = BORDER_MIX[int(rnd(lot["id"], side, "mix") * len(BORDER_MIX))]
        ok_b = lambda t: abs(t - mid) > gap and g.clear(*pt(t, bline), BORDER_D / 2 - .1)
        for a, c in runs(samples, ok_b, .25):
            if c - a < 3.0:
                continue
            add(b, pt((a + c) / 2, bline), "border", list(mix), along=along, len=c - a, depth=BORDER_D, seed=int(rnd(lot["id"], side, a) * 9999))
            n["border"] += 1
            for k in range(int((c - a) / .5) + 1):
                g.take(*pt(a + k * .5, bline), BORDER_D / 2)
            sx, sz = pt((a + c) / 2, bline + inward * (BORDER_D / 2 + .55))  # where the gardener stands to tend it
            tend.append([round(sx, 2), round(sz, 2)])
    for cx, cz, ux, uz in ((x0, z0, 1, 1), (x1, z0, -1, 1), (x0, z1, 1, -1), (x1, z1, -1, -1)):   # shrubs in the corners
        tries = ((1.5, 1.5, 1.25), (3.4, 1.3, .8), (1.3, 3.4, .8)) if big else ((1.3, 1.3, 1.0), (2.9, 1.1, .65))
        for k, (u, v, r) in enumerate(tries):
            x, z = cx + ux * u, cz + uz * v
            if g.clear(x, z, r - .2):
                sp = SHRUBS[int(rnd(lot["id"], cx, cz, k) * len(SHRUBS))] if k else ("lilac", "hydrangea")[int(rnd(lot["id"], cx, cz) * 2)]
                add(b, (x, z), "bush", sp, r=r * (.9 + .2 * rnd(lot["id"], x, z)), seed=int(rnd(lot["id"], "b", x, z) * 9999))
                g.take(x, z, r)
                n["bush"] += 1
    if big:                                                               # a few shrubs alone on the open lawn
        placed = 0
        for k in range(60):
            if placed >= 4:
                break
            x, z = x0 + 6 + (x1 - x0 - 12) * rnd(lot["id"], "lone", k), z0 + 6 + (z1 - z0 - 12) * rnd(lot["id"], "lone", k, 1)
            if g.clear(x, z, 1.6):
                add(b, (x, z), "bush", ("hydrangea", "lilac", "rose")[placed % 3], r=1.1 + .3 * rnd(lot["id"], "lr", k), seed=int(rnd(lot["id"], "ls", k) * 9999))
                g.take(x, z, 2.2)
                placed += 1
                n["bush"] += 1
    for t in g.trees:                                                     # bulbs under the trees in the lawn
        tx, tz = t
        if x0 + 2 < tx < x1 - 2 and z0 + 2 < tz < z1 - 2 and rnd(lot["id"], "bulb", tx, tz) < (.7 if big else .9):
            r = 2.0 + .6 * rnd(lot["id"], "br", tx, tz)
            if not any(seg_dist(tx, tz, p, q) < r + 1.0 for p, q in g.paths) and all(math.hypot(tx - a, tz - c) > d + r * .6 for a, c, d in g.hard):
                add(b, (tx, tz), "drift", ("daffodil", "tulip")[int(rnd(lot["id"], "bs", tx) * 2)] if big else "daffodil", r=r, hole=1.3, seed=int(rnd(lot["id"], "bd", tx, tz) * 9999))
                n["drift"] += 1
    if big:                                                               # and asters out on the lawn
        placed = 0
        for k in range(60):
            if placed >= 3:
                break
            x, z = x0 + 5 + (x1 - x0 - 10) * rnd(lot["id"], "ast", k), z0 + 5 + (z1 - z0 - 10) * rnd(lot["id"], "ast", k, 1)
            if g.clear(x, z, 2.0):
                add(b, (x, z), "drift", "aster", r=2.0 + .8 * rnd(lot["id"], "ar", k), hole=0, seed=int(rnd(lot["id"], "as", k) * 9999))
                g.take(x, z, 2.6)
                placed += 1
                n["drift"] += 1
    lot["objects"]["flowerbed"] = [q for q in lot["objects"].get("flowerbed", []) if q not in lot.get("tend_borders", [])] + tend
    lot["tend_borders"] = tend
    return n


def plant_grounds(b, lot):
    """A park block's outer lawn, between the pavement and the path round the garden: groups of shrubs along it, with
    bulbs coming up in front of them, clear of the paths in, the lamps and the trees."""
    g = Ground(b, lot)
    ox, oz = b["origin"]
    S = b["size"]
    n = dict(bush=0, drift=0)
    band = (lot["rect"][0] - ox - 4.0) / 2 + .5                           # the middle of the band, in from the block's edge
    if band < 2.0:                                                        # the garden fills the block (Town Park): no outer lawn
        return n
    for side in "nswe":
        for t in (11.0, 21.0, S - 21.0, S - 11.0):
            x, z = {"n": (ox + t, oz + band), "s": (ox + t, oz + S - band), "w": (ox + band, oz + t), "e": (ox + S - band, oz + t)}[side]
            if not g.clear(x, z, 1.3):
                continue
            sp = SHRUBS[int(rnd(b["id"], side, t) * len(SHRUBS))]
            add(b, (x, z), "bush", sp, r=1.3 + .3 * rnd(b["id"], side, t, "r"), seed=int(rnd(b["id"], side, t, "s") * 9999))
            g.take(x, z, 1.8)
            n["bush"] += 1
            ux, uz = (1, 0) if side in "ns" else (0, 1)
            for sgn in (1, -1):                                           # a smaller one beside it, on whichever side has room
                x2, z2 = x + sgn * ux * 1.9, z + sgn * uz * 1.9
                if g.clear(x2, z2, .8):
                    add(b, (x2, z2), "bush", ("rose", "hydrangea")[int(rnd(b["id"], side, t, "k") * 2)], r=.85, seed=int(rnd(b["id"], side, t, "s2") * 9999))
                    g.take(x2, z2, 1.0)
                    n["bush"] += 1
                    break
            if rnd(b["id"], side, t, "d") < .6:                           # bulbs or asters round the group
                add(b, (x, z), "drift", ("daffodil", "tulip", "aster")[int(rnd(b["id"], side, t, "ds") * 3)], r=2.6, hole=1.6, seed=int(rnd(b["id"], side, t, "dd") * 9999))
                n["drift"] += 1
    return n


def main(only_new=False):
    city = jload(os.path.join(CITY, "city.json"))
    blocks = []
    for e in city["blocks"]:
        p = os.path.join(CITY, e["file"])
        blocks.append((p, jload(p)))
    total = dict(gardens=0, hedge=0, border=0, bush=0, drift=0)
    for _, b in blocks:                                                   # take away what an earlier run added
        if only_new:
            break
        b["props"] = [p for p in b["props"] if p.get("set") != SET]
        for l in b["lots"]:
            if "tend_borders" in l:
                l["objects"]["flowerbed"] = [q for q in l["objects"].get("flowerbed", []) if q not in l["tend_borders"]]
                l.pop("tend_borders")
    for _, b in blocks:
        for l in b["lots"]:
            if l["kind"] == "garden" and not (only_new and "tend_borders" in l):   # a planted garden keeps its tend_borders
                n = plant_garden(b, l, l["name"].endswith(" Park"))
                if b["template"] == "park" and l["name"].endswith(" Park"):
                    for k, v in plant_grounds(b, l).items():
                        n[k] += v
                total["gardens"] += 1
                for k, v in n.items():
                    total[k] += v
    for p, b in blocks:
        jsave(p, b, 1)
    print("%(gardens)d gardens planted: %(hedge)d hedges, %(border)d borders, %(bush)d shrubs, %(drift)d drifts" % total)


if __name__ == "__main__":
    import sys
    main(only_new="--new" in sys.argv)
