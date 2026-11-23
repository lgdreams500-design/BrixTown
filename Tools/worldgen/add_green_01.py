"""Fill the open ground outside Old Town (WLD-055, WLD-056; owner, 2026-11-24: "remove city wall and fix the areas which
outside of the wall ... use vegetation other props etc, to fill in ... roads, parks, plots for sales, even dense woods ...
but no building please").

The town wall is gone (play.js no longer builds it); its line stays in city.json "wall" as the edge of Old Town's street
grid. Every grid cell outside that line that no block covered was bare field. This script gives each of them a block of
open ground, all as data, with no lots and so no buildings and no change to anyone's day:

  woods     dense mixed woodland (pine, oak, birch), a winding trail, a clearing with picnic tables, logs, rocks, undergrowth
  common    a park meadow: a path round it and across it, a pond, groups of big trees, benches, bins and lamps
  orchard   rows of apple and plum trees inside a clipped hedge
  fields    farmland: crop strips and pasture between hawthorn hedgerows, a farm track, hay bales, hedgerow oaks
  plots     building plots for sale beside the gate roads: fenced, pegged, mown, a "For sale" board each, a gravel track in
            (set dressing only: F-MAP-PLOTS is at level 0 for the first slice, so they cannot be bought yet)
  meadow    a small green with a few trees and a bench

Along Old Town's old wall line each block facing it gets the Wall Walk: a footpath with an avenue of limes and lamps.
Nothing is placed within reach of a road (roads.json gate roads and links, the communities' lanes, the tram rails).

Run from Tools/worldgen:   python add_green_01.py      (running it again writes the same files)
Then:                      python -m unittest test_world ;  python build_play.py
Every size, spacing and count here is a first guess [ASSUMPTION].
"""
import json, math, os, random

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
SEED = 20261124
YEAR = 48                                   # days in the game year (calendar.json)

# (first cell, span, template, district): together they cover every cell outside the wall line no other block covers
AREAS = [
    ((6, 0), (1, 4), "woods", "Mill Copse"),
    ((13, 0), (1, 4), "woods", "Brook Copse"),
    ((7, 3), (6, 1), "common", "Northgate Green"),
    ((0, 4), (1, 7), "woods", "Westwood"),
    ((1, 4), (3, 1), "orchard", "Westgate Orchard"),
    ((4, 4), (2, 3), "common", "West Common"),
    ((4, 7), (2, 4), "plots", "West Gate Plots"),
    ((0, 11), (4, 2), "fields", "Mill Fields"),
    ((0, 13), (4, 3), "woods", "Southwood"),
    ((6, 12), (1, 1), "meadow", "Civic Green"),
    ((14, 4), (2, 4), "common", "East Common"),
    ((15, 8), (1, 1), "meadow", "Outlet Green"),
    ((14, 9), (2, 4), "plots", "East Gate Plots"),
    ((16, 4), (3, 1), "orchard", "Eastgate Orchard"),
    ((19, 4), (1, 9), "woods", "Eastwood"),
    ((16, 11), (3, 2), "fields", "Brook Farm"),
    ((16, 13), (4, 3), "woods", "Hollow Wood"),
]
R1 = lambda v: round(v, 1)
R2 = lambda v: round(v, 2)


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


class Roads:
    """Every strip a car or tram uses outside the wall, for keeping things clear of it: [(a, b, half width)]."""

    def __init__(self, city, roads, blocks):
        P, SW = city["grid"]["pitch"], city["grid"]["street_width"]
        J = {j["id"]: j["pos"] for j in roads["junctions"]}
        segs = []
        line = lambda pts, hw: segs.extend((pts[k], pts[k + 1], hw) for k in range(len(pts) - 1))
        for g in roads["gates"]:
            a = J[g["junction"]]
            line([a, [a[0] + g["dir"][0] * g["length"], a[1] + g["dir"][1] * g["length"]]], 4.5)
        for l in roads.get("links", []):
            line(l["points"], l["width"] / 2)
        for b in blocks:
            for ln in b.get("lanes", []):
                line(ln["points"], ln["width"] / 2)
                if ln.get("end") == "turning":
                    p = ln["points"][-1]; line([p, p], ln["width"])
        i0, j0, i1, j1 = city["wall"]["cells"]
        tz = city["streets"]["east_west"].index("Tram") * P - 1.8           # the tram rails run 40 m out each side (play.js)
        line([[i0 * P - SW / 2 - 44, tz], [i0 * P, tz]], 2.5); line([[i1 * P, tz], [i1 * P + SW / 2 + 44, tz]], 2.5)
        self.segs, self.grid = segs, {}
        for n, (a, b, hw) in enumerate(segs):
            for gx in range(int((min(a[0], b[0]) - hw) // 32), int((max(a[0], b[0]) + hw) // 32) + 1):
                for gz in range(int((min(a[1], b[1]) - hw) // 32), int((max(a[1], b[1]) + hw) // 32) + 1):
                    self.grid.setdefault((gx, gz), []).append(n)

    def gap(self, x, z):
        """How far (x, z) is from the edge of the nearest road (negative: on it); 99 when none is within 32 m."""
        best = 99.0
        for gx in (int(x // 32) - 1, int(x // 32), int(x // 32) + 1):
            for gz in (int(z // 32) - 1, int(z // 32), int(z // 32) + 1):
                for n in self.grid.get((gx, gz), ()):
                    a, b, hw = self.segs[n]
                    best = min(best, seg_dist(x, z, a, b) - hw)
        return best


def seg_dist(x, z, a, b):
    dx, dz = b[0] - a[0], b[1] - a[1]
    L = dx * dx + dz * dz
    t = 0.0 if L < 1e-9 else max(0.0, min(1.0, ((x - a[0]) * dx + (z - a[1]) * dz) / L))
    return math.hypot(x - a[0] - t * dx, z - a[1] - t * dz)


def near_path(x, z, paths, d):
    return any(seg_dist(x, z, p, q) < d for p, q in paths)


class Block:
    def __init__(self, city, cell, span, template, district, roads):
        P, SW, S = city["grid"]["pitch"], city["grid"]["street_width"], city["grid"]["block_size"]
        self.i, self.j = cell
        self.id = "b_%d_%d" % cell
        self.x0, self.z0 = self.i * P + SW / 2, self.j * P + SW / 2
        self.x1, self.z1 = self.x0 + S + (span[0] - 1) * P, self.z0 + S + (span[1] - 1) * P
        self.rnd = random.Random("%d %s" % (SEED, self.id))
        self.d = dict(id=self.id, grid=list(cell), template=template, district=district, origin=[self.x0, self.z0], size=S,
                      span=list(span), lots=[], paths=[], tracks=[], trees=[], props=[])
        self.roads, self.taken = roads, []                  # taken: (x, z, r) already placed, for spacing
        wl = city["wall"]["cells"]
        self.town = (wl[0] * P - SW / 2, wl[1] * P - SW / 2, wl[2] * P + SW / 2, wl[3] * P + SW / 2)

    def inside(self, x, z, m=0.0):
        return self.x0 + m <= x <= self.x1 - m and self.z0 + m <= z <= self.z1 - m

    def free(self, x, z, r, road=3.0, paths=1.2):
        """Can something of radius r stand at (x, z): in the block, off the roads, the paths and the tracks, clear of what is there?"""
        if not self.inside(x, z, 0.6) or self.roads.gap(x, z) < r + road:
            return False
        if near_path(x, z, self.d["paths"], r + paths):
            return False
        if any(seg_dist(x, z, t["points"][k], t["points"][k + 1]) < t["width"] / 2 + r + paths
               for t in self.d["tracks"] for k in range(len(t["points"]) - 1)):
            return False
        return all(math.hypot(x - a, z - b) >= r + q for a, b, q in self.taken)

    def tree(self, x, z, species, age_years, r=2.0):
        n = len(self.d["trees"])
        self.d["trees"].append(dict(id="%s_t%d" % (self.id, n), species=species, pos=[R2(x), R2(z)],
                                    planted=-int(age_years * YEAR), seed=self.rnd.randrange(1000000)))
        self.taken.append((x, z, r))

    def prop(self, kind, x, z, rad=0.8, **kw):
        n = len(self.d["props"])
        self.d["props"].append(dict(id="%s_p%d" % (self.id, n), kind=kind, pos=[R2(x), R2(z)], set="wild", **kw))
        self.taken.append((x, z, rad))

    def path(self, pts):
        pts = [[R1(p[0]), R1(p[1])] for p in pts]
        keep = [pts[0]]                                     # a straight run is one segment: drop the points along it
        for k in range(1, len(pts) - 1):
            a, b, c = keep[-1], pts[k], pts[k + 1]
            if abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) > 0.5:
                keep.append(b)
        pts = keep + [pts[-1]] if len(pts) > 1 else pts
        self.d["paths"] += [[pts[k], pts[k + 1]] for k in range(len(pts) - 1)]

    def poisson(self, spacing, tries=30, m=1.0):
        """Points spread evenly but at random over the block, none closer than `spacing` (Bridson's method)."""
        rnd, cs = self.rnd, spacing / math.sqrt(2)
        W, H = self.x1 - self.x0 - 2 * m, self.z1 - self.z0 - 2 * m
        grid, pts = {}, []
        def ok(p):
            gx, gz = int(p[0] / cs), int(p[1] / cs)
            for a in range(gx - 2, gx + 3):
                for b in range(gz - 2, gz + 3):
                    q = grid.get((a, b))
                    if q and math.hypot(q[0] - p[0], q[1] - p[1]) < spacing:
                        return False
            return 0 <= p[0] <= W and 0 <= p[1] <= H
        first = (rnd.random() * W, rnd.random() * H)
        active = [first]; pts.append(first); grid[(int(first[0] / cs), int(first[1] / cs))] = first
        while active:
            k = rnd.randrange(len(active)); c = active[k]
            for _ in range(tries):
                a, d = rnd.random() * 2 * math.pi, spacing * (1 + rnd.random())
                p = (c[0] + math.cos(a) * d, c[1] + math.sin(a) * d)
                if ok(p):
                    pts.append(p); active.append(p); grid[(int(p[0] / cs), int(p[1] / cs))] = p
                    break
            else:
                active.pop(k)
        return [(self.x0 + m + p[0], self.z0 + m + p[1]) for p in pts]

    # ---- the Wall Walk: a footpath and an avenue of limes along each side that faces Old Town's old wall line
    def wall_walk(self):
        tx0, tz0, tx1, tz1 = self.town
        sides = []
        if abs(self.x1 - (tx0 - 0)) < 20 and self.z1 > tz0 and self.z0 < tz1: sides.append(("x", self.x1 - 3.0, self.x1 - 7.5))
        if abs(self.x0 - tx1) < 20 and self.z1 > tz0 and self.z0 < tz1: sides.append(("x", self.x0 + 3.0, self.x0 + 7.5))
        if abs(self.z1 - tz0) < 20 and self.x1 > tx0 and self.x0 < tx1: sides.append(("z", self.z1 - 3.0, self.z1 - 7.5))
        if abs(self.z0 - tz1) < 20 and self.x1 > tx0 and self.x0 < tx1: sides.append(("z", self.z0 + 3.0, self.z0 + 7.5))
        for ax, at, row in sides:
            lo, hi = (self.z0, self.z1) if ax == "x" else (self.x0, self.x1)
            P = lambda t, v: (v, t) if ax == "x" else (t, v)
            run, t = [], lo + 2
            while t <= hi - 2:                                 # the path, broken where a road crosses it
                if self.roads.gap(*P(t, at)) > 2.5:
                    run.append(P(t, at))
                elif len(run) > 1:
                    self.path(run); run = []
                else:
                    run = []
                t += 4
            if len(run) > 1:
                self.path(run)
            k, t = 0, lo + 6
            while t <= hi - 6:
                x, z = P(t, row)
                if self.free(x, z, 2.5):
                    self.tree(x, z, "linden", 14 + 4 * self.rnd.random(), 3.0)
                    if k % 2 == 0:
                        lx, lz = P(t + 7, (at + row) / 2 + (row - at) * 0.1)
                        if self.free(lx, lz, 0.4, paths=0.6):
                            self.prop("lamp", lx, lz, 0.5)
                    k += 1
                t += 14

    # ---- templates
    def woods(self):
        r = self.rnd
        W, H = self.x1 - self.x0, self.z1 - self.z0
        long_x = W >= H
        L0, L1, S0, S1 = (self.x0, self.x1, self.z0, self.z1) if long_x else (self.z0, self.z1, self.x0, self.x1)
        mid, amp, ph = (S0 + S1) / 2, (S1 - S0) * 0.28, r.random() * 6.28
        P = (lambda u, v: (u, v)) if long_x else (lambda u, v: (v, u))
        trail = [P(u, mid + amp * math.sin(ph + (u - L0) / 70.0)) for u in [L0 + 1 + k * 8 for k in range(int((L1 - L0 - 2) / 8) + 1)]]
        self.path(trail)
        if (L1 - L0) > 300 and (S1 - S0) > 150:             # a big wood: a second trail across it
            cu = (L0 + L1) / 2
            self.path([P(cu + 20 * math.sin(k / 3.0), S0 + 1 + k * (S1 - S0 - 2) / 12) for k in range(13)])
        c = trail[len(trail) // 2]                            # a clearing on the trail: picnic tables, logs to sit on, a rock
        clear_r = 13.0 if min(W, H) > 70 else 9.0
        self.taken.append((c[0], c[1], clear_r))
        for k, a in enumerate((0.6, 2.4, 4.1)):
            x, z = c[0] + math.cos(a) * clear_r * 0.55, c[1] + math.sin(a) * clear_r * 0.55
            if self.inside(x, z, 2) and not near_path(x, z, self.d["paths"], 2.2) and self.roads.gap(x, z) > 4:
                self.d["props"].append(dict(id="%s_p%d" % (self.id, len(self.d["props"])), kind="picnic" if k < 2 else "log",
                                            pos=[R2(x), R2(z)], set="wild", yaw=R2(a), len=4.0))
        mix = (("pine", .45), ("oak", .3), ("birch", .25)) if self.i < 10 else (("oak", .4), ("birch", .3), ("pine", .3))
        def species():
            v = r.random()
            for s, w in mix:
                v -= w
                if v <= 0:
                    return s
            return mix[-1][0]
        for x, z in self.poisson(7.2, m=1.5):                  # the trees: close together, the edge a little thinner
            edge = min(x - self.x0, self.x1 - x, z - self.z0, self.z1 - z)
            if edge < 6 and r.random() < 0.35:
                continue
            if near_path(x, z, self.d["paths"], 2.6) or self.roads.gap(x, z) < 4.0:
                continue
            if any(math.hypot(x - a, z - b) < q for a, b, q in self.taken):
                continue
            self.d["trees"].append(dict(id="%s_t%d" % (self.id, len(self.d["trees"])), species=species(), pos=[R2(x), R2(z)],
                                        planted=-int((8 + 30 * r.random()) * YEAR), seed=r.randrange(1000000)))
        trees = [(t["pos"][0], t["pos"][1]) for t in self.d["trees"]]
        tg = {}
        for x, z in trees:
            tg.setdefault((int(x // 8), int(z // 8)), []).append((x, z))
        def by_tree(x, z, d):
            return any(math.hypot(x - a, z - b) < d for gx in range(int(x // 8) - 1, int(x // 8) + 2)
                       for gz in range(int(z // 8) - 1, int(z // 8) + 2) for a, b in tg.get((gx, gz), ()))
        area = W * H
        for _ in range(int(area / 700)):                       # undergrowth between the trunks, logs and rocks
            x, z = self.x0 + 2 + r.random() * (W - 4), self.z0 + 2 + r.random() * (H - 4)
            if near_path(x, z, self.d["paths"], 2.0) or self.roads.gap(x, z) < 3 or by_tree(x, z, 1.8):
                continue
            if any(math.hypot(x - a, z - b) < q for a, b, q in self.taken):
                continue
            v = r.random()
            if v < 0.55:
                self.prop("plant", x, z, 1.4, shape="bush", species="holly" if r.random() < .4 else "bracken", r=R2(0.8 + 0.8 * r.random()), seed=r.randrange(9999))
            elif v < 0.8:
                self.prop("log", x, z, 2.0, yaw=R2(r.random() * 3.14), len=R1(2.5 + 3 * r.random()))
            else:
                self.prop("rock", x, z, 1.2, r=R2(0.5 + 0.9 * r.random()), seed=r.randrange(9999))

    def common(self):
        r, m = self.rnd, 7.0
        loop = [(self.x0 + m, self.z0 + m), (self.x1 - m, self.z0 + m), (self.x1 - m, self.z1 - m), (self.x0 + m, self.z1 - m), (self.x0 + m, self.z0 + m)]
        def walk(pts):                                          # a path, broken where a road crosses it
            run = []
            for a, b in zip(pts, pts[1:]):
                n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 4))
                for k in range(n + 1):
                    p = (a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n)
                    if self.roads.gap(*p) > 2.0:
                        if not run or run[-1] != p:
                            run.append(p)
                    else:
                        if len(run) > 1:
                            self.path(run)
                        run = []
            if len(run) > 1:
                self.path(run)
        walk(loop)
        cx, cz = (self.x0 + self.x1) / 2, (self.z0 + self.z1) / 2
        walk([(self.x0 + m, self.z0 + m), (cx, cz), (self.x1 - m, self.z1 - m)])
        walk([(self.x1 - m, self.z0 + m), (cx, cz), (self.x0 + m, self.z1 - m)])
        self.wall_walk()
        W, H = self.x1 - self.x0, self.z1 - self.z0
        pr = min(16.0, min(W, H) * 0.12)                        # the pond, off the crossing
        px, pz = cx + (W * 0.22 if W > H else 0) , cz + (H * 0.22 if H >= W else 0)
        if self.free(px, pz, pr + 1, road=4, paths=2):
            self.prop("pond", px, pz, pr + 2.5, r=R1(pr))
        for k in range(int((W * 2 + H * 2) / 45)):             # benches facing in along the loop, a bin by every other one
            t = (k + 0.5) / int((W * 2 + H * 2) / 45) * (W * 2 + H * 2)
            if t < W: x, z, f, al = self.x0 + t, self.z0 + m + 2.2, [0, 1], "x"
            elif t < W + H: x, z, f, al = self.x1 - m - 2.2, self.z0 + t - W, [-1, 0], "z"
            elif t < 2 * W + H: x, z, f, al = self.x1 - (t - W - H), self.z1 - m - 2.2, [0, -1], "x"
            else: x, z, f, al = self.x0 + m + 2.2, self.z1 - (t - 2 * W - H), [1, 0], "z"
            if self.free(x, z, 1.0, paths=0.4):
                self.prop("bench", x, z, 1.3, along=al)
                if k % 2 == 0 and self.free(x + (1.8 if al == "x" else 0), z + (1.8 if al == "z" else 0), 0.4, paths=0.3):
                    self.prop("bin", x + (1.8 if al == "x" else 0), z + (1.8 if al == "z" else 0), 0.4)
        for a, b in ((self.x0 + m, self.z0 + m), (self.x1 - m, self.z0 + m)):  # lamps along the cross paths
            c2 = (self.x1 - m, self.z1 - m) if a < cx else (self.x0 + m, self.z1 - m)
            L = math.hypot(c2[0] - a, c2[1] - b)
            for k in range(1, int(L / 32)):
                x, z = a + (c2[0] - a) * k * 32 / L + 1.6, b + (c2[1] - b) * k * 32 / L
                if self.free(x, z, 0.4, paths=0.3):
                    self.prop("lamp", x, z, 0.5)
        groups = max(3, int(W * H / 2600))                    # big trees in groups of two to five, grass between
        for _ in range(groups):
            gx, gz = self.x0 + 12 + r.random() * (W - 24), self.z0 + 12 + r.random() * (H - 24)
            sp = r.choice(("oak", "oak", "linden", "plane", "birch"))
            for _ in range(2 + r.randrange(4)):
                x, z = gx + (r.random() - .5) * 22, gz + (r.random() - .5) * 22
                if self.free(x, z, 3.0, paths=1.5):
                    self.tree(x, z, sp, 10 + 25 * r.random(), 5.0)
        for _ in range(int(W * H / 3000)):                     # flowering shrubs at the corners of the paths
            x, z = self.x0 + 4 + r.random() * (W - 8), self.z0 + 4 + r.random() * (H - 8)
            if self.free(x, z, 1.5, paths=1.0):
                self.prop("plant", x, z, 1.8, shape="bush", species=r.choice(("lilac", "hydrangea", "holly")), r=R2(1.0 + .6 * r.random()), seed=r.randrange(9999))

    def orchard(self):
        r = self.rnd
        x0, z0, x1, z1 = self.x0 + 2, self.z0 + 2, self.x1 - 2, self.z1 - 2
        gate = (x0 + x1) / 2
        for ax, (cx, cz), L in (("x", ((x0 + x1) / 2, z0), x1 - x0), ("x", ((x0 + x1) / 2, z1), x1 - x0),
                                ("z", (x0, (z0 + z1) / 2), z1 - z0), ("z", (x1, (z0 + z1) / 2), z1 - z0)):
            if ax == "x" and cz == z1:                         # the side with the gate: two runs either side of a 4 m gap
                for a, b in ((x0, gate - 2), (gate + 2, x1)):
                    self.d["props"].append(dict(id="%s_p%d" % (self.id, len(self.d["props"])), kind="plant", set="wild", shape="hedge",
                                                species="box", pos=[R2((a + b) / 2), R2(cz)], along="x", len=R1(b - a), depth=1.0, h=1.4))
                continue
            self.d["props"].append(dict(id="%s_p%d" % (self.id, len(self.d["props"])), kind="plant", set="wild", shape="hedge",
                                        species="box", pos=[R2(cx), R2(cz)], along=ax, len=R1(L), depth=1.0, h=1.4))
        self.path([(gate, z1 + 1), (gate, (z0 + z1) / 2)])
        self.taken.append(((x0 + x1) / 2, (z0 + z1) / 2, 0))
        xs = [x0 + 6 + 7 * k for k in range(int((x1 - x0 - 12) / 7) + 1)]
        zs = [z0 + 6 + 7 * k for k in range(int((z1 - z0 - 12) / 7) + 1)]
        for n, x in enumerate(xs):
            for z in zs:
                if abs(x - gate) < 3 or self.roads.gap(x, z) < 4:
                    continue
                self.tree(x + (r.random() - .5) * .6, z + (r.random() - .5) * .6, "apple" if (n // 3) % 2 == 0 else "plum", 6 + 10 * r.random(), 2.0)
        if self.free(gate + 3.5, (z0 + z1) / 2 + 6, 1.0, paths=0.3):
            self.prop("bench", gate + 3.5, (z0 + z1) / 2 + 6, 1.0, along="z")

    def fields(self):
        r = self.rnd
        W, H = self.x1 - self.x0, self.z1 - self.z0
        track_z = self.z0 + 4.0                                # a farm track along the north edge, hedged fields south of it
        self.d["tracks"].append(dict(points=[[R1(self.x0), R1(track_z)], [R1(self.x1), R1(track_z)]], width=3.5))
        n = max(2, int(round(W / 70)))
        edges = [self.x0 + W * k / n for k in range(n + 1)]
        crops = ["wheat", "barley", "furrow", "pasture", "cabbage", "pasture"]
        r.shuffle(crops)
        top, bot = track_z + 3.5, self.z1 - 1.0
        for k in range(n):
            a, b = edges[k] + 1.0, edges[k + 1] - 1.0
            crop = crops[k % len(crops)]
            self.prop("crop", (a + b) / 2, (top + bot) / 2, 0, size=[R1(b - a - 1.5), R1(bot - top - 1.5)], crop=crop, along="z")
            if crop == "pasture":
                for _ in range(3 + r.randrange(4)):
                    x, z = a + 5 + r.random() * (b - a - 10), top + 5 + r.random() * (bot - top - 10)
                    self.prop("haybale", x, z, 1.5, yaw=R2(r.random() * 3.14))
        for k in range(n + 1):                                 # hawthorn hedgerows between the fields, a gate gap by the track
            x = edges[k]
            a, b = top + 5.0, bot
            self.d["props"].append(dict(id="%s_p%d" % (self.id, len(self.d["props"])), kind="plant", set="wild", shape="hedge",
                                        species="hawthorn", pos=[R2(x), R2((a + b) / 2)], along="z", len=R1(b - a), depth=1.4, h=1.8))
            for t in range(int((b - a) / 34)):
                z = a + 17 + 34 * t + (r.random() - .5) * 6
                if self.roads.gap(x, z) > 5:
                    self.tree(x + (r.random() - .5) * 1.0, z, "oak", 15 + 25 * r.random(), 4.0)
        hedge_s = (self.x0 + self.x1) / 2
        self.d["props"].append(dict(id="%s_p%d" % (self.id, len(self.d["props"])), kind="plant", set="wild", shape="hedge",
                                    species="hawthorn", pos=[R2(hedge_s), R2(bot)], along="x", len=R1(W), depth=1.4, h=1.8))
        for k in range(n):
            a, b = edges[k] + 1.0, edges[k + 1] - 1.0
            self.prop("fence", (a + b) / 2 + 3, top + 0.4, 0, len=R1(b - a - 8), along="x")

    def plots(self):
        r = self.rnd
        W, H = self.x1 - self.x0, self.z1 - self.z0
        long_x = W >= H
        P = (lambda u, v: (u, v)) if long_x else (lambda u, v: (v, u))
        L0, L1, S0, S1 = (self.x0, self.x1, self.z0, self.z1) if long_x else (self.z0, self.z1, self.x0, self.x1)
        mid = (S0 + S1) / 2
        self.wall_walk()
        walk_m = 9.0 if self.d["paths"] else 0.0               # leave the Wall Walk on the town side
        front = 32.0                                            # frontage of a plot along the track, depth to the block edge
        tr = [P(L0, mid), P(L1, mid)]
        track = []
        n = 1
        u = L0 + 3
        while u + front <= L1 - 3:
            for side in (-1, 1):
                v0, v1 = (mid + 3.5, S1 - 3) if side > 0 else (S0 + 3, mid - 3.5)
                if not long_x:
                    town_side = self.x1 if self.i < 10 else self.x0
                    if side > 0 and self.i < 10: v1 -= walk_m
                    if side < 0 and self.i >= 10: v0 += walk_m
                a, b = P(u, v0), P(u + front, v1)
                rx0, rz0, rx1, rz1 = min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1])
                pts = [(rx0 + (rx1 - rx0) * s / 4, rz0 + (rz1 - rz0) * t / 4) for s in range(5) for t in range(5)]
                if min(self.roads.gap(x, z) for x, z in pts) < 3.0:
                    continue
                cx, cz = (rx0 + rx1) / 2, (rz0 + rz1) / 2
                self.prop("crop", cx, cz, 0, size=[R1(rx1 - rx0), R1(rz1 - rz0)], crop="rough", along="x")
                fx = P(1, 0)
                for (sx, sz, ax, L, gap) in ((cx, rz0, "x", rx1 - rx0, None), (cx, rz1, "x", rx1 - rx0, None),
                                             (rx0, cz, "z", rz1 - rz0, None), (rx1, cz, "z", rz1 - rz0, None)):
                    on_track = (abs((sz if long_x else sx) - mid) < 4)
                    if on_track:                                 # the side on the track: a 4 m opening for the way in
                        lo, hi = (rx0, rx1) if ax == "x" else (rz0, rz1)
                        mm = (lo + hi) / 2
                        for p0, p1 in ((lo, mm - 2), (mm + 2, hi)):
                            pc = ((p0 + p1) / 2, sz) if ax == "x" else (sx, (p0 + p1) / 2)
                            self.d["props"].append(dict(id="%s_p%d" % (self.id, len(self.d["props"])), kind="fence", set="wild",
                                                        pos=[R2(pc[0]), R2(pc[1])], len=R1(p1 - p0), along=ax))
                        face = [0, -side] if long_x else [-side, 0]
                        bx, bz = (mm + 4.5, sz + face[1] * -1.2) if ax == "x" else (sx + face[0] * -1.2, mm + 4.5)
                        self.d["props"].append(dict(id="%s_p%d" % (self.id, len(self.d["props"])), kind="board", set="wild",
                                                    pos=[R2(bx), R2(bz)], face=face, text="FOR SALE", plot=n))
                    else:
                        self.d["props"].append(dict(id="%s_p%d" % (self.id, len(self.d["props"])), kind="fence", set="wild",
                                                    pos=[R2(sx), R2(sz)], len=R1(L), along=ax))
                for x, z in ((rx0 + .6, rz0 + .6), (rx1 - .6, rz0 + .6), (rx0 + .6, rz1 - .6), (rx1 - .6, rz1 - .6)):
                    self.d["props"].append(dict(id="%s_p%d" % (self.id, len(self.d["props"])), kind="peg", set="wild", pos=[R2(x), R2(z)]))
                self.taken.append((cx, cz, math.hypot(rx1 - rx0, rz1 - rz0) / 2))
                track.append(u); track.append(u + front)
                n += 1
            u += front + 4
        if track:                                               # the gravel track in, from where the plots begin to where they end
            a, b = P(min(track) - 2, mid), P(max(track) + 2, mid)
            pts, run = [], []
            steps = int(math.hypot(b[0] - a[0], b[1] - a[1]) / 4)
            for k in range(steps + 1):
                p = (a[0] + (b[0] - a[0]) * k / steps, a[1] + (b[1] - a[1]) * k / steps)
                if self.roads.gap(*p) > 0.5:
                    run.append(p)
                elif len(run) > 1:
                    self.d["tracks"].append(dict(points=[[R1(q[0]), R1(q[1])] for q in run], width=4.0)); run = []
                else:
                    run = []
            if len(run) > 1:
                self.d["tracks"].append(dict(points=[[R1(q[0]), R1(q[1])] for q in run], width=4.0))
        for x, z in self.poisson(16, m=3):                     # a few trees left standing on the ground nobody has bought
            if self.free(x, z, 3.0, road=4, paths=2.0) and r.random() < 0.6:
                self.tree(x, z, r.choice(("oak", "birch", "linden")), 10 + 20 * r.random(), 4.0)

    def meadow(self):
        r = self.rnd
        self.path([(self.x0 + 2, self.z1 - 2), (self.x1 - 2, self.z0 + 2)])
        self.wall_walk()
        cx, cz = (self.x0 + self.x1) / 2, (self.z0 + self.z1) / 2
        if self.free(cx + 3, cz + 3, 1.0, paths=0.3):
            self.prop("bench", cx + 3, cz + 3, 1.2, along="x")
        for _ in range(9):
            x, z = self.x0 + 8 + r.random() * (self.x1 - self.x0 - 16), self.z0 + 8 + r.random() * (self.z1 - self.z0 - 16)
            if self.free(x, z, 3.5, paths=1.5):
                self.tree(x, z, r.choice(("oak", "birch", "plane")), 12 + 20 * r.random(), 5.0)


def main():
    city = jload(os.path.join(CITY, "city.json"))
    roads = jload(os.path.join(CITY, "roads.json"))
    ids = {a: "b_%d_%d" % a[0] for a in AREAS}
    others = [jload(os.path.join(CITY, e["file"])) for e in city["blocks"] if e["id"] not in ids.values()]
    R = Roads(city, roads, others)
    taken = {}
    for b in others:
        sx, sz = b.get("span", [1, 1])
        for di in range(sx):
            for dj in range(sz):
                taken[(b["grid"][0] + di, b["grid"][1] + dj)] = b["id"]
    for (cell, span, template, district) in AREAS:
        for di in range(span[0]):
            for dj in range(span[1]):
                c = (cell[0] + di, cell[1] + dj)
                assert c not in taken, "%s: cell %s already has %s" % (district, c, taken[c])
                taken[c] = district
    out = []
    for (cell, span, template, district) in AREAS:
        b = Block(city, cell, span, template, district, R)
        getattr(b, template)()
        with open(os.path.join(CITY, "blocks", b.id + ".json"), "w", encoding="utf-8") as f:
            json.dump(b.d, f, ensure_ascii=False, indent=1)
        out.append(b)
        print("%-18s %-8s %s: %4d trees, %4d props, %3d paths" % (district, template, b.id, len(b.d["trees"]), len(b.d["props"]), len(b.d["paths"])))
    entries = [e for e in city["blocks"] if e["id"] not in ids.values()]
    for b in out:
        entries.append(dict(id=b.id, grid=b.d["grid"], file="blocks/%s.json" % b.id, template=b.d["template"], district=b.d["district"], span=b.d["span"]))
    city["blocks"] = entries
    city["wall"]["note"] = ("The line where Old Town's town wall stood (taken down, WLD-055): grid lines i 6 to 14 and j 4 to 12 (cells 6-13 "
                            "by 4-11). Old Town's street grid ends here; beyond it the newer areas and open ground (WLD-056). "
                            "Streets named null have no blocks yet.")
    with open(os.path.join(CITY, "city.json"), "w", encoding="utf-8") as f:
        json.dump(city, f, ensure_ascii=False, indent=1)
    free = [(i, j) for i in range(city["grid"]["columns"]) for j in range(city["grid"]["rows"]) if (i, j) not in taken
            and not (city["wall"]["cells"][0] <= i < city["wall"]["cells"][2] and city["wall"]["cells"][1] <= j < city["wall"]["cells"][3])]
    print("trees %d, props %d; cells still bare: %s" % (sum(len(b.d["trees"]) for b in out), sum(len(b.d["props"]) for b in out), free or "none"))


if __name__ == "__main__":
    main()
