"""Add the first outskirts to city_01's 20 x 16 grid (the owner's requests of 2026-11-22): three residential
communities of independent houses along curving lanes, each with a park; three large malls of 3 x 6 grid cells, one
for each concept in malls_01.py; and hilly ground round them (World/city_01/terrain.json). Old Town, two malls and the
outlet stay level; the canal mall's terraces step down the slope.

Map data only: block files, their entries in city.json, names for the grid streets that had none, and the terrain.
Roads (roads.json), residents and the playable slice are not touched yet.

  community   cells                    lanes
  Northwood   i 0-5,   j 0-3   (6x4)   a winding lane west to east, closes off it
  Eastbrook   i 14-19, j 0-3   (6x4)   the same, its own shape
  Southfield  i 4-15,  j 13-15 (12x3)  a long winding lane, short closes north and south
  malls       Westgate Mall i 1-3, j 5-10 (galleria); Eastgate Mall i 16-18, j 5-10 (loop);
              Northgate Mall i 7-12, j 0-2, straight out of the North Gate (canal)

New in the block format (read by Tools/worldgen/draw_map.py; the loaders, tests and slice do not know them yet):
  span    [columns, rows] on a block that covers more than one cell; its ground runs from its origin to
          origin + span * pitch - street_width, over the grid streets it swallows
  lanes   on a block: its own streets, [{id, name, width, points: [[x, z], ...], end: "turning" or null}], a curve
          given as points about 4 m apart; "turning" means a turning circle of radius `width` at the last point
  angle   on a lot: degrees its footprint is turned about its centre (clockwise on the map, x east, z south);
          rect is the footprint before turning, door and places inside are where they are after it
terrain.json is the height of the ground in metres on a square grid of `step` metres over the whole city.
Run from Tools/worldgen:   python add_outskirts_01.py      (running it again writes the same data)
Every size and count here is a first guess [ASSUMPTION].
"""
import json, math, os, random

import make_city_01 as mc
import malls_01

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
SEED = 20261122
P, SW = mc.PITCH, mc.STREET

STREETS_NS = {0: "Alder", 1: "Birch", 2: "Cedar", 3: "Hazel", 4: "Rowan", 5: "Willow",
              15: "Orchard", 16: "Meadow", 17: "Brook", 18: "Heather", 19: "Fern", 20: "Thorn"}
STREETS_EW = {0: "Northfield", 1: "Lark", 2: "Wren", 3: "Finch", 13: "Harvest", 14: "Clover", 15: "Barley", 16: "Furrow"}

COMMUNITIES = [  # name, first cell, span, lane names (the winding lane first)
    ("Northwood", (0, 0), (6, 4), ["Northwood Way", "Juniper Close", "Fox Hollow", "Bramble Rise", "Kestrel Close", "Holly Bank", "Larch End", "Owl Lane"]),
    ("Eastbrook", (14, 0), (6, 4), ["Eastbrook Way", "Mill Race", "Heron Close", "Sorrel Rise", "Wagtail End", "Ash Dell", "Clover Bank", "Fennel Close"]),
    ("Southfield", (4, 13), (12, 3), ["Southfield Way", "Poppy Close", "Barn Owl Rise", "Linnet End", "Teasel Close", "Cowslip Bank", "Thistle Rise",
                                       "Orchard End", "Hayfield Close", "Swallow Rise", "Dovecote End", "Rye Close", "Meadow Bank", "Plover End"]),
]
MALLS = [("Westgate Mall", (1, 5), (3, 6), "galleria"), ("Eastgate Mall", (16, 5), (3, 6), "loop"), ("Northgate Mall", (7, 0), (6, 3), "canal")]
# Roads from Old Town's gates out to each new area (roads.json "links"): name, gate it leaves from, where it goes.
# "spine_end"/"spine_start": the last/first point of a community's winding lane; "entry": the community's edge at this
# x, from where an entry lane (part of the community, same name) runs in to its winding lane; "carpark": a mall's;
# "south_edge": the middle of a site's south edge. An optional fourth item gives points the road passes through.
LINKS = [("Northwood Road", "gate_north", ("Northwood", "spine_end"), [(900, 322), (660, 320)]),   # round Northgate Mall
         ("Eastbrook Road", "gate_north", ("Eastbrook", "spine_start"), [(1020, 322), (1270, 318)]),
         ("Northgate Road", "gate_north", ("Northgate Mall", "south_edge")),
         ("Southfield Road", "gate_south", ("Southfield", "entry")),
         ("Westgate Road", "gate_west", ("Westgate Mall", "carpark")),
         ("Eastgate Road", "gate_east", ("Eastgate Mall", "carpark"))]
ENTRY_X = {"Southfield": 960.0}
LINK_W, LINK_KMH = 9.0, 40.0
OURS = {c[0] for c in COMMUNITIES} | {m[0] for m in MALLS}

CENTRE = (72.0, 58.0)               # a community's local centre: width, depth (m)
LANE_W = 7.0                       # carriageway and kerbs of a lane, m
HOUSE_W, HOUSE_D = 13.0, 10.0      # footprint along and away from the lane
FRONT = LANE_W / 2 + 6.0           # lane centre to the front wall: pavement and a front garden
STEP_ALONG = 21.0                  # spacing of houses along a lane
TERRAIN_STEP = 16.0
HILL_MAX = 26.0                    # highest ground above Old Town, m
LEVEL_MARGIN = 60.0                # distance over which ground rises from a level area


# ---------------- curves ----------------
def catmull(ctrl, step=4.0):
    """A smooth curve through the control points, as points about `step` m apart."""
    pts = [ctrl[0]] + ctrl + [ctrl[-1]]
    out = []
    for k in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[k - 1], pts[k], pts[k + 1], pts[k + 2]
        n = max(2, int(math.dist(p1, p2) / step))
        for s in range(n):
            t = s / n
            out.append([0.5 * ((2 * p1[a]) + (-p0[a] + p2[a]) * t + (2 * p0[a] - 5 * p1[a] + 4 * p2[a] - p3[a]) * t * t +
                               (-p0[a] + 3 * p1[a] - 3 * p2[a] + p3[a]) * t ** 3) for a in (0, 1)])
    out.append(list(ctrl[-1]))
    return [[round(x, 2), round(z, 2)] for x, z in out]


def frames(pts):
    """(point, unit tangent, arc length) along a curve."""
    out, s = [], 0.0
    for k, p in enumerate(pts):
        a, b = pts[max(0, k - 1)], pts[min(len(pts) - 1, k + 1)]
        L = math.dist(a, b) or 1.0
        if k:
            s += math.dist(pts[k - 1], p)
        out.append((p, ((b[0] - a[0]) / L, (b[1] - a[1]) / L), s))
    return out


def dist_to(pts, q):
    best = 1e9
    for a, b in zip(pts, pts[1:]):
        dx, dz = b[0] - a[0], b[1] - a[1]
        L2 = dx * dx + dz * dz or 1e-9
        t = max(0.0, min(1.0, ((q[0] - a[0]) * dx + (q[1] - a[1]) * dz) / L2))
        best = min(best, math.hypot(q[0] - a[0] - t * dx, q[1] - a[1] - t * dz))
    return best


# ---------------- a community ----------------
class Community:
    def __init__(self, name, cell, span, lane_names, rng, entry_x=None, entry_name=None):
        self.name, self.cell, self.span, self.rng = name, cell, span, rng
        self.entry_x, self.entry_name = entry_x, entry_name
        self.id = "b_%d_%d" % cell
        self.X0, self.Z0 = cell[0] * P + SW / 2, cell[1] * P + SW / 2
        self.X1, self.Z1 = self.X0 + span[0] * P - SW, self.Z0 + span[1] * P - SW
        self.names = list(lane_names)
        self.lanes, self.lots, self.trees, self.props, self.paths = [], [], [], [], []
        self.reserved = []

    def inside(self, q, m):
        return self.X0 + m <= q[0] <= self.X1 - m and self.Z0 + m <= q[1] <= self.Z1 - m

    def lane(self, ctrl, end, name=None, width=LANE_W):
        lid = "%s_r%d" % (self.id, len(self.lanes))
        self.lanes.append(dict(id=lid, name=name or self.names[len(self.lanes) % len(self.names)], width=width, points=catmull(ctrl), end=end))
        return self.lanes[-1]

    def lay_lanes(self):
        """A winding lane along the long side from one edge to the other, and closes off it to either side."""
        r = self.rng
        wide = (self.X1 - self.X0) >= (self.Z1 - self.Z0)
        L, S = (self.X1 - self.X0, self.Z1 - self.Z0) if wide else (self.Z1 - self.Z0, self.X1 - self.X0)
        at = lambda a, c: (self.X0 + a, self.Z0 + c) if wide else (self.X0 + c, self.Z0 + a)
        n = max(3, int(L / 130))
        ctrl = [at(0, S * r.uniform(0.4, 0.6))]
        for k in range(1, n):
            ctrl.append(at(L * k / n + r.uniform(-20, 20), S * 0.5 + r.uniform(-0.22, 0.22) * S))
        ctrl.append(at(L, S * r.uniform(0.4, 0.6)))
        spine = self.lane(ctrl, None)
        joins = []
        if self.entry_x is not None:                                # an entry lane from the edge facing town in to the winding lane
            near_z0 = abs(self.Z0 - 768) < abs(self.Z1 - 768)
            edge = (self.entry_x, self.Z0 if near_z0 else self.Z1)
            q = min(spine["points"], key=lambda p: math.dist(p, edge))
            mid = ((edge[0] + q[0]) / 2 + r.uniform(-12, 12), (edge[1] + q[1]) / 2)
            self.entry = self.lane([edge, mid, q], None, self.entry_name, LINK_W)
            joins.append(q)
        self.lay_centre()                                           # before the closes, which then keep clear of it
        fr = frames(spine["points"])
        total = fr[-1][2]
        s = 50.0 + r.uniform(0, 20)
        while s < total - 40:                                       # closes off both sides, staggered
            for side, shift in ((1, 0.0), (-1, r.uniform(18, 32))):
                if r.random() > 0.85:
                    continue
                p, t, _ = min(fr, key=lambda f: abs(f[2] - (s + shift)))
                if any(math.dist(p, q) < 45 for q in joins):
                    continue
                if any(r[0] - 30 <= p[0] <= r[2] + 30 and r[1] - 30 <= p[1] <= r[3] + 30 for r in self.reserved):
                    continue
                nrm = (-t[1] * side, t[0] * side)
                reach = min(self.room(p, nrm) - 24, r.uniform(60, 150))
                if reach > 40:
                    bend = r.uniform(-0.7, 0.7)
                    bt = (t[0] * bend, t[1] * bend)
                    c1 = (p[0] + nrm[0] * reach * 0.45 + bt[0] * 14, p[1] + nrm[1] * reach * 0.45 + bt[1] * 14)
                    c2 = (p[0] + nrm[0] * reach + bt[0] * reach * 0.45, p[1] + nrm[1] * reach + bt[1] * reach * 0.45)
                    hit = lambda q: any(r[0] - 10 <= q[0] <= r[2] + 10 and r[1] - 10 <= q[1] <= r[3] + 10 for r in self.reserved)
                    if self.inside(c2, 22) and not any(hit(q) for q in catmull([p, c1, c2])):
                        self.lane([p, c1, c2], "turning")
            s += r.uniform(58, 78)

    def room(self, p, d):
        """How far one can go from p in direction d before the community's edge."""
        k = 0.0
        while self.inside((p[0] + d[0] * (k + 4), p[1] + d[1] * (k + 4)), 0):
            k += 4
        return k

    def clear(self, c, rad):
        return all(dist_to(l["points"], c) >= rad for l in self.lanes) and \
            all(math.dist(c, l["c"]) >= 18.5 for l in self.lots if "c" in l) and \
            all(math.dist(c, l["points"][-1]) >= LANE_W + rad - 3 for l in self.lanes if l["end"])

    def house(self, c, toward, lane):
        """A house centred at c whose front faces `toward` (a unit vector to its lane)."""
        r = self.rng
        theta = math.atan2(toward[0], -toward[1])                   # turns the footprint's north wall to face the lane
        cs, sn = math.cos(theta), math.sin(theta)
        rot = lambda q: [round(c[0] + (q[0] - c[0]) * cs - (q[1] - c[1]) * sn, 2), round(c[1] + (q[0] - c[0]) * sn + (q[1] - c[1]) * cs, 2)]
        rect = [round(c[0] - HOUSE_W / 2, 2), round(c[1] - HOUSE_D / 2, 2), round(c[0] + HOUSE_W / 2, 2), round(c[1] + HOUSE_D / 2, 2)]
        n = sum(1 for l in self.lots if l.get("lane") == lane["id"]) + 1
        l = dict(id="%s_l%d" % (self.id, len(self.lots)), kind="home", name="%d %s" % (n, lane["name"]), rect=rect,
                 door=[round(c[0], 2), rect[1]], floors=2, enterable=True, objects={}, residents=r.choice([2, 3, 3, 4, 5]))
        mc.Block.fill(None, l, l["residents"])                      # places inside, laid out from the door before turning
        l["objects"] = {k: [rot(q) for q in v] for k, v in l["objects"].items()}
        l["door"], l["angle"] = rot(l["door"]), round(math.degrees(theta), 1)
        l["c"], l["lane"] = c, lane["id"]
        self.lots.append(l)
        front = (c[0] + toward[0] * (FRONT + HOUSE_D / 2 - LANE_W / 2), c[1] + toward[1] * (FRONT + HOUSE_D / 2 - LANE_W / 2))
        self.paths.append([l["door"], [round(front[0], 2), round(front[1], 2)]])
        back = (c[0] - toward[0] * (HOUSE_D / 2 + 7), c[1] - toward[1] * (HOUSE_D / 2 + 7))
        if r.random() < 0.7 and self.inside(back, 4):
            self.tree(back, r.choice(["plum", "linden", "plum", "plum"]), r.uniform(2, 25))

    def tree(self, q, species, age):
        self.trees.append(dict(id="%s_t%d" % (self.id, len(self.trees)), species=species, pos=[round(q[0], 2), round(q[1], 2)],
                               planted=round(-age * 48), seed=self.rng.randrange(10 ** 6)))

    def lay_houses(self):
        off = FRONT + HOUSE_D / 2
        for lane in self.lanes:
            fr = frames(lane["points"])
            s = 14.0
            while s < fr[-1][2] - (4 if lane["end"] else 14):
                p, t, _ = min(fr, key=lambda f: abs(f[2] - s))
                for side in (1, -1):
                    nrm = (-t[1] * side, t[0] * side)
                    c = (p[0] + nrm[0] * off, p[1] + nrm[1] * off)
                    if self.inside(c, 10) and self.clear(c, off - 1.5) and not self.in_park(c):
                        self.house(c, (-nrm[0], -nrm[1]), lane)
                s += STEP_ALONG
            if lane["end"]:                                          # round the turning circle at the end of a close
                e, (ex, ez) = lane["points"][-1], fr[-1][1]
                for a in range(-120, 121, 40):
                    ang = math.atan2(ez, ex) + math.radians(a)
                    d = (math.cos(ang), math.sin(ang))
                    c = (e[0] + d[0] * (LANE_W + 2 + off - LANE_W / 2), e[1] + d[1] * (LANE_W + 2 + off - LANE_W / 2))
                    if self.inside(c, 10) and self.clear(c, off - 1.5) and not self.in_park(c):
                        self.house(c, (-d[0], -d[1]), lane)

    def in_park(self, c):
        """Inside a green or the local centre (with 12 m to spare), where no house goes."""
        return any(r[0] - 12 <= c[0] <= r[2] + 12 and r[1] - 12 <= c[1] <= r[3] + 12 for r in self.reserved)

    def lay_centre(self):
        """The community's local centre (CITY_BALANCE.md §3.4): a parade of a grocer, a kiosk, a bakery, a cafe and a
        clinic facing the winding lane across a little square, and the primary school behind it. Placed near the middle
        of the winding lane, on whichever side has room, clear of every lane."""
        W, D = CENTRE
        fr = frames(self.lanes[0]["points"])
        total = fr[-1][2]
        best = None
        for k in range(8, 33):
            s = total * k / 40
            p = min(fr, key=lambda f: abs(f[2] - s))[0]
            for side in (1, -1):                                   # 1: the centre south of the lane, its front to the north
                gap = LANE_W / 2 + 5
                z0 = p[1] + gap if side > 0 else p[1] - gap - D
                rect = [p[0] - W / 2, z0, p[0] + W / 2, z0 + D]
                if not (self.inside(rect[:2], 8) and self.inside(rect[2:], 8)):
                    continue
                if any(rect[0] - 4 <= q[0] <= rect[2] + 4 and rect[1] - 4 <= q[1] <= rect[3] + 4 for l in self.lanes for q in l["points"]):
                    continue
                score = abs(s - total / 2)
                if best is None or score < best[0]:
                    best = (score, rect, side, p)
        if not best:
            return
        _, (x0, z0, x1, z1), side, p = best
        W_ = lambda u, v: [round(x0 + u, 2), round(z0 + v if side > 0 else z1 - v, 2)]

        def rect(u0, v0, u1, v1):
            a, b = W_(u0, v0), W_(u1, v1)
            return [min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1])]
        parade = [("shop", "%s Grocer", 2), ("shop", "%s Kiosk", 16), ("food", "%s Cafe", 30), ("shop", "%s Bakery", 44), ("health", "%s Clinic", 58)]
        for kind, name, u in parade:
            l = dict(id="%s_l%d" % (self.id, len(self.lots)), kind=kind, name=name % self.name, rect=rect(u, 4, u + 12, 15), door=W_(u + 6, 4),
                     floors=1, enterable=True, objects={})
            if kind == "food":
                l["seats"] = 8
            mc.Block.fill(None, l)
            self.lots.append(l)
            self.paths.append([l["door"], W_(u + 6, 0)])
        school = dict(id="%s_l%d" % (self.id, len(self.lots)), kind="school", name="%s Primary School" % self.name, rect=rect(18, 28, 54, 50),
                      door=W_(36, 28), floors=2, enterable=True, objects={}, tags=["school", "primary"])
        r = school["rect"]
        staff = max(1, int(round((r[2] - r[0]) * (r[3] - r[1]) * school["floors"] * 0.8 / 96)))
        school["objects"]["work"] = [[round(r[0] + 2 + (r[2] - r[0] - 4) * (k % 6 + .5) / 6, 2), round(r[1] + 2 + (r[3] - r[1] - 4) * (k // 6 + .5) / ((staff + 5) // 6), 2)]
                                     for k in range(staff)]
        school["staff"] = staff
        self.lots.append(school)
        self.paths += [[W_(0, 0), W_(W, 0)], [W_(36, 0), W_(36, 28)], [W_(36, 0), [round(p[0], 2), round(p[1], 2)]]]
        for u in (8, 22, 50, 64):                                     # trees on the square
            q = W_(u, 21)
            self.tree(q, "linden", self.rng.uniform(4, 20))
        self.props += [dict(kind="bench", pos=W_(u, 21), along="x") for u in (30, 42)]
        self.reserved.append([x0, z0, x1, z1])

    def lay_park(self):
        """A green where the lanes leave the most room, with a loop path, benches and trees."""
        R = 32.0
        best, at = 0, None
        for x in range(int(self.X0 + R + 12), int(self.X1 - R - 12), 8):
            for z in range(int(self.Z0 + R + 12), int(self.Z1 - R - 12), 8):
                d = min(dist_to(l["points"], (x, z)) for l in self.lanes)
                if d > best:
                    best, at = d, (x, z)
        if not at or best < R + 4:
            return
        x, z = at
        rect = [x - R, z - R, x + R, z + R]
        lane = min(self.lanes, key=lambda l: dist_to(l["points"], at))
        q = min(lane["points"], key=lambda p: math.dist(p, at))
        side = min((math.dist(q, pt), pt) for pt in ((x, z - R), (x, z + R), (x - R, z), (x + R, z)))[1]
        g = dict(id="%s_l%d" % (self.id, len(self.lots)), kind="garden", name="%s Green" % self.name, rect=rect, door=list(side), floors=0,
                 enterable=False, objects={})
        loop = [[x - R + 6, z - R + 6], [x + R - 6, z - R + 6], [x + R - 6, z + R - 6], [x - R + 6, z + R - 6]]
        g["loop"] = loop
        self.paths += [[p, q2] for p, q2 in zip(loop, loop[1:] + loop[:1])] + [[list(side), [round(q[0], 2), round(q[1], 2)]]]
        benches = [(x - 10, z - R + 7.2, "x"), (x + 10, z + R - 7.2, "x"), (x - R + 7.2, z + 8, "z"), (x + R - 7.2, z - 8, "z")]
        g["objects"]["bench"] = [[u - .5, v] if al == "x" else [u, v - .5] for u, v, al in benches] + [[u + .5, v] if al == "x" else [u, v + .5] for u, v, al in benches]
        g["objects"]["flowerbed"] = [[x - 14, z - 12], [x + 14, z - 12], [x - 14, z + 16], [x + 14, z + 16]]
        g["objects"]["work"] = g["objects"]["flowerbed"]
        for u, v, al in benches:
            self.props.append(dict(kind="bench", pos=[u, v], along=al))
        for n, (u, v) in enumerate(((x - 14, z - 14), (x + 14, z - 14), (x - 14, z + 14), (x + 14, z + 14))):
            self.props.append(dict(kind="flowerbed", pos=[u, v], color=n))
        self.props.append(dict(kind="fountain", pos=[x, z], r=2.2))
        for k in range(10):
            a = self.rng.uniform(0, 2 * math.pi)
            self.tree((x + math.cos(a) * self.rng.uniform(16, 26), z + math.sin(a) * self.rng.uniform(16, 26)),
                      self.rng.choice(["linden", "plane", "plum", "linden"]), self.rng.uniform(3, 40))
        self.lots.append(g)
        self.reserved.append(rect)

    def lamps(self):
        for lane in self.lanes:
            fr = frames(lane["points"])
            s, side = 10.0, 1
            while s < fr[-1][2]:
                p, t, _ = min(fr, key=lambda f: abs(f[2] - s))
                self.props.append(dict(kind="lamp", pos=[round(p[0] - t[1] * side * (LANE_W / 2 + 1), 2), round(p[1] + t[0] * side * (LANE_W / 2 + 1), 2)]))
                s, side = s + 32, -side

    def build(self):
        self.lay_lanes()
        self.lay_park()
        self.lay_houses()
        self.lamps()
        for l in self.lots:
            l.pop("c", None)
            l.pop("lane", None)
        return dict(id=self.id, grid=list(self.cell), template="community", district=self.name, origin=[self.X0, self.Z0], size=mc.SIZE,
                    span=list(self.span), lanes=self.lanes, lots=self.lots, paths=self.paths, trees=self.trees, props=self.props)


# ---------------- terrain ----------------
def terrain(city, blocks, rng):
    """Rolling hills over the outskirts; level ground over Old Town (inside its wall), the malls and the outlet,
    rising from them over LEVEL_MARGIN metres."""
    g = city["grid"]
    W, D = g["columns"] * P, g["rows"] * P
    nx, nz = int(W / TERRAIN_STEP) + 1, int(D / TERRAIN_STEP) + 1
    i0, j0, i1, j1 = city["wall"]["cells"]
    level = [(i0 * P - 24, j0 * P - 24, i1 * P + 24, j1 * P + 24, 0.0)]          # (x0, z0, x1, z1, ground height)
    for b in blocks:
        if b["template"] in ("mall", "outlet", "civic") or b["district"] == "Outside the walls":
            terr = [t for l in b["lots"] for t in l.get("plan", {}).get("terraces", [])]
            if terr:                                                 # a terraced site: each terrace level at its own height
                level += [(t["rect"][0], t["rect"][1] - 12, t["rect"][2], t["rect"][3] + 12, t["ground"]) for t in terr]
                continue
            sx, sz = (g["block_size"] + (n - 1) * P for n in b.get("span", [1, 1]))
            level.append((b["origin"][0] - 12, b["origin"][1] - 12, b["origin"][0] + sx + 12, b["origin"][1] + sz + 12, 0.0))
    hills = [(rng.uniform(0, W), rng.uniform(0, D), rng.uniform(90, 260), rng.uniform(0.4, 1.0)) for _ in range(34)]
    raw = [[sum(h * math.exp(-((x * TERRAIN_STEP - hx) ** 2 + (z * TERRAIN_STEP - hz) ** 2) / (2 * r * r)) for hx, hz, r, h in hills)
            for x in range(nx)] for z in range(nz)]
    top = max(max(row) for row in raw) or 1.0

    def height(x, z, hill):
        d, h0 = min((math.hypot(max(a - x, 0, x - c), max(b - z, 0, z - e)), h) for a, b, c, e, h in level)
        t = min(1.0, d / LEVEL_MARGIN)
        t = t * t * (3 - 2 * t)
        return h0 + (max(hill, h0) - h0) * t if d > 0 else h0
    heights = [[round(height(x * TERRAIN_STEP, z * TERRAIN_STEP, HILL_MAX * raw[z][x] / top), 1) for x in range(nx)] for z in range(nz)]
    return dict(schema=1, city=city["id"], note="Height of the ground in metres above Old Town's level, on a square grid of `step` m "
                "from `origin`; rows run north to south, values west to east. Generated by Tools/worldgen/add_outskirts_01.py.",
                origin=[0.0, 0.0], step=TERRAIN_STEP, size=[nx, nz], heights=heights)


def links(roads, made):
    """Curving roads from the end of a gate road out to each new area."""
    J = {j["id"]: j["pos"] for j in roads["junctions"]}
    by = {d["district"]: d for d in made}
    out = []
    for n, (name, gid, (area, where), *via) in enumerate(LINKS):
        via = via[0] if via else None
        g = next(x for x in roads["gates"] if x["id"] == gid)
        j = J[g["junction"]]
        a = (j[0] + g["dir"][0] * g["length"], j[1] + g["dir"][1] * g["length"])
        d = by[area]
        if where in ("spine_end", "spine_start"):
            b = tuple(d["lanes"][0]["points"][-1 if where == "spine_end" else 0])
            to = dict(lane=d["lanes"][0]["id"], at=list(b))
        elif where == "entry":
            ln = next(l for l in d["lanes"] if l["name"] == name)
            b = tuple(ln["points"][0])
            to = dict(lane=ln["id"], at=list(b))
        elif where == "south_edge":
            sx = d["span"][0] * P - SW
            b = (d["origin"][0] + sx / 2, d["origin"][1] + d["span"][1] * P - SW)
            to = dict(block=d["id"], at=list(b))
        else:                                                       # into the side of a mall's car park nearest the gate
            cp = next(p for p in d["props"] if p["kind"] == "carpark")
            cx, cz = cp["pos"]
            hw = cp["size"][0] / 2
            b = (cx + hw if a[0] > cx else cx - hw, cz - cp["size"][1] / 4)
            to = dict(block=d["id"], carpark=True, at=[round(b[0], 2), round(b[1], 2)])
        ux, uz = g["dir"]
        c1 = (a[0] + ux * 40, a[1] + uz * 40)                       # leave the gate straight on, then bend towards the area
        c2 = ((c1[0] + b[0]) / 2 + uz * 30 * (1 if n % 2 else -1), (c1[1] + b[1]) / 2 + ux * 30 * (1 if n % 2 else -1))
        ctrl = [a, c1, c2, b]
        if where == "carpark":                                      # along the mall's side, clear of the hall, then into the car park
            x = a[0] + ux * 8 if ux else a[0]
            ctrl = [a, (x, a[1] + 24), (x, b[1] - 24), b]
        elif math.dist(a, b) < 60:                                  # a short way: straight on
            ctrl = [a, b]
        if via:
            ctrl = [a] + list(via) + [b]
        out.append(dict(id="link_%d" % n, name=name, width=LINK_W, limit_kmh=LINK_KMH, gate=gid, to=to,
                        points=catmull(ctrl)))
    return out


def main():
    city_p = os.path.join(CITY, "city.json")
    with open(city_p, encoding="utf-8") as f:
        city = json.load(f)
    ns, ew = city["streets"]["north_south"], city["streets"]["east_west"]
    for k, v in STREETS_NS.items():
        ns[k] = ns[k] or v
    for k, v in STREETS_EW.items():
        ew[k] = ew[k] or v

    for e in [e for e in city["blocks"] if e["district"] in OURS]:     # what this script made before
        p = os.path.join(CITY, e["file"])
        if os.path.exists(p):
            os.remove(p)
    city["blocks"] = [e for e in city["blocks"] if e["district"] not in OURS]
    taken = {tuple(e["grid"]) for e in city["blocks"]}

    rng = random.Random(SEED)
    made = []
    for name, cell, span, concept in MALLS:
        cells = {(i, j) for i in range(cell[0], cell[0] + span[0]) for j in range(cell[1], cell[1] + span[1])}
        assert not cells & taken, (name, sorted(cells & taken))
        taken |= cells
        made.append(malls_01.CONCEPTS[concept](name, cell, span, random.Random("%d:%s" % (SEED, name))))
    for name, cell, span, lanes in COMMUNITIES:
        cells = {(i, j) for i in range(cell[0], cell[0] + span[0]) for j in range(cell[1], cell[1] + span[1])}
        assert not cells & taken, (name, sorted(cells & taken))
        taken |= cells
        entry_name = next((l[0] for l in LINKS if l[2] == (name, "entry")), None)
        made.append(Community(name, cell, span, lanes, random.Random("%d:%s" % (SEED, name)), ENTRY_X.get(name), entry_name).build())

    for d in made:
        with open(os.path.join(CITY, "blocks", d["id"] + ".json"), "w", encoding="utf-8", newline="\n") as f:
            json.dump(d, f, indent=1, ensure_ascii=False)
        city["blocks"].append(dict(id=d["id"], grid=d["grid"], file="blocks/%s.json" % d["id"], template=d["template"], district=d["district"], span=d["span"]))
    with open(city_p, "w", encoding="utf-8", newline="\n") as f:
        json.dump(city, f, indent=1, ensure_ascii=False)

    roads_p = os.path.join(CITY, "roads.json")
    with open(roads_p, encoding="utf-8") as f:
        roads = json.load(f)
    roads["links"] = [l for l in roads.get("links", []) if not l["id"].startswith("link_")] + links(roads, made)   # keep others' links
    with open(roads_p, "w", encoding="utf-8", newline="\n") as f:
        json.dump(roads, f, indent=1)
        f.write("\n")

    blocks = []
    for e in city["blocks"]:
        with open(os.path.join(CITY, e["file"]), encoding="utf-8") as f:
            blocks.append(json.load(f))
    t = terrain(city, blocks, random.Random(SEED + 1))
    with open(os.path.join(CITY, "terrain.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(t, f, separators=(",", ":"))
        f.write("\n")

    for d in made:
        homes = [l for l in d["lots"] if l["kind"] == "home"]
        plan = next((l["plan"] for l in d["lots"] if "plan" in l), None)
        if plan:
            print("%-14s %-8s %s units by level, %d fountains" % (d["district"], plan["concept"], [len(L["units"]) for L in plan["levels"]], len(plan["fountains"])))
            continue
        print("%-14s %3d lanes, %3d houses for %4d people" % (d["district"], len(d.get("lanes", [])), len(homes), sum(l["residents"] for l in homes)))
    print("terrain %d x %d points, highest %.1f m" % (t["size"][0], t["size"][1], max(max(r) for r in t["heights"])))


if __name__ == "__main__":
    main()
