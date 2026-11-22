"""The three mall concepts as map data (the owner's request of 2026-11-22: "add one more mall to the map and use
each of these 3 concepts"). Used by add_outskirts_01.py; each function returns a block.

  galleria  Westgate Mall   one straight walk 240 m long on three levels under a glass roof, shops both sides, an open
                            middle with four bridges on each upper level, a round fountain court halfway, a big store
                            at each end
  loop      Eastgate Mall   a circuit on three levels round an open court, a big store in each corner, a grand
                            fountain and a ring of kiosks on the court floor, two bridges across the court on each
                            upper level, a food hall on the top level
  canal     Northgate Mall  an open-air street along a winding canal, three storeys of shops each side, footbridges
                            over the water, bridges between the top floors, small fountains in quay squares, and three
                            terraces stepping down the slope with the water falling between them

The mall's building is one lot (kind "shop", tag "mall"); what is inside it is its `plan`:
  storey_height            metres from one floor to the next
  levels[]                 one per floor, ground first: {level, units, walks, voids, bridges}
    units                  [{id, name, kind, rect, angle?}]  kind: shop, food, anchor, kiosk, foodhall; rect and angle as a
                           lot's (a big store fills the same rect on every floor)
    walks                  polygons [[x, z], ...] people walk on, on that floor
    voids                  polygons open to the floors below (no floor here)
    bridges                polygons that cross a void or the water on that floor
  fountains                [{pos, r, level}]
  water                    polygons of open water (the canal), at its terrace's ground
  falls                    [[x, z], [x, z]] lines where the water steps down a terrace
  terraces                 [{rect, ground}]: ground height in metres above Old Town of each terrace (terrain.json follows it)
Every size and count here is a first guess [ASSUMPTION].
"""
import math

import make_city_01 as mc

P, SW = mc.PITCH, mc.STREET
STOREY = 6.0
LEVELS = 3


def r2(v):
    return round(v, 2)


class Site:
    """A mall site over span cells; local u runs east, v south, from the site's north-west corner."""

    def __init__(self, name, cell, span, rng):
        self.name, self.cell, self.span, self.rng = name, cell, span, rng
        self.id = "b_%d_%d" % cell
        self.X0, self.Z0 = cell[0] * P + SW / 2, cell[1] * P + SW / 2
        self.W, self.D = span[0] * P - SW, span[1] * P - SW
        self.levels = [dict(level=k, units=[], walks=[], voids=[], bridges=[]) for k in range(LEVELS)]
        self.fountains, self.water, self.falls, self.terraces = [], [], [], []
        self.lots, self.paths, self.trees, self.props = [], [], [], []
        self.short = name.split()[0]

    def w(self, u, v):
        return [r2(self.X0 + u), r2(self.Z0 + v)]

    def rect(self, u0, v0, u1, v1):
        return [self.w(u0, v0), self.w(u1, v0), self.w(u1, v1), self.w(u0, v1)]

    def circle(self, u, v, r, n=24):
        return [self.w(u + r * math.cos(2 * math.pi * k / n), v + r * math.sin(2 * math.pi * k / n)) for k in range(n)]

    def unit(self, levels, kind, rect, name=None, angle=None):
        """A unit on each of `levels` (a big store is the same unit on every floor, with one id)."""
        n = sum(len(L["units"]) for L in self.levels)
        uid = "%s_u%d" % (self.id, n)
        for k in levels:
            num = sum(1 for u in self.levels[k]["units"]) + 1
            u = dict(id=uid, name=name or "%s %d.%02d" % (self.short, k, num), kind=kind,
                     rect=[r2(x) for x in rect])
            if angle is not None:
                u["angle"] = round(angle, 1)
            self.levels[k]["units"].append(u)

    def urect(self, u0, v0, u1, v1):
        return [self.X0 + u0, self.Z0 + v0, self.X0 + u1, self.Z0 + v1]

    def tree(self, u, v, species, age):
        self.trees.append(dict(id="%s_t%d" % (self.id, len(self.trees)), species=species, pos=self.w(u, v), planted=round(-age * 48),
                               seed=self.rng.randrange(10 ** 6)))

    def lamps_round(self):
        for u in range(0, int(self.W) + 1, 48):
            self.props.append(dict(kind="lamp", pos=self.w(u, -4)))
            self.props.append(dict(kind="lamp", pos=self.w(u, self.D + 4)))
        for v in range(48, int(self.D), 48):
            self.props.append(dict(kind="lamp", pos=self.w(-4, v)))
            self.props.append(dict(kind="lamp", pos=self.w(self.W + 4, v)))

    def hall(self, rect, door, concept, **more):
        """The mall's building as one lot, with its places for staff and customers laid out from the door, and its plan."""
        l = dict(id="%s_l%d" % (self.id, len(self.lots)), kind="shop", name=self.name, rect=[r2(x) for x in self.urect(*rect)], door=self.w(*door),
                 floors=LEVELS, enterable=True, objects={}, staff=24, customers=80, seats=32, tags=["mall", "cafe"])
        l.update(more)
        mc.Block.fill(None, l)
        l["plan"] = dict(concept=concept, storey_height=STOREY, levels=self.levels, fountains=self.fountains)
        if self.water:
            l["plan"].update(water=self.water, falls=self.falls)
        if self.terraces:
            l["plan"]["terraces"] = self.terraces
        self.lots.append(l)
        return l

    def block(self):
        return dict(id=self.id, grid=list(self.cell), template="mall", district=self.name, origin=[self.X0, self.Z0], size=mc.SIZE,
                    span=list(self.span), lots=self.lots, paths=self.paths, trees=self.trees, props=self.props)


def galleria(name, cell, span, rng):
    """Concept A on a site 272 m wide, 560 m long (north to south)."""
    s = Site(name, cell, span, rng)
    all3 = range(LEVELS)
    s.unit(all3, "anchor", s.urect(56, 20, 216, 70), "%s Department Store" % s.short)
    s.unit(all3, "anchor", s.urect(56, 310, 216, 360), "%s Home and Garden" % s.short)
    court = (136, 190)
    for k in all3:
        for v in range(70, 310, 10):
            near_court = abs(v + 5 - court[1]) < 22
            kind = "food" if (k == 2 and near_court) or (v // 10) % 6 == 3 else "shop"
            s.unit([k], kind, s.urect(56, v, 96, v + 10))
            s.unit([k], kind, s.urect(176, v, 216, v + 10))
        L = s.levels[k]
        L["walks"].append(s.rect(96, 70, 176, 310))
        if k:
            L["voids"] += [s.rect(116, 80, 156, 300), s.circle(*court, 30)]
            for v in (110, 150, 230, 270):
                L["bridges"].append(s.rect(116, v - 3, 156, v + 3))
    s.fountains += [dict(pos=s.w(*court), r=10.0, level=0), dict(pos=s.w(136, 100), r=3.0, level=0), dict(pos=s.w(136, 280), r=3.0, level=0)]
    s.hall((56, 20, 216, 360), (136, 360), "galleria")
    s.props.append(dict(kind="carpark", pos=s.w(136, 460), size=[232, 170], rows=8, cars=160))
    s.paths.append([s.w(136, 360), s.w(136, 375)])
    for v in range(40, 360, 30):                                     # trees on the lawns either side of the building
        s.tree(28, v, "linden", rng.uniform(4, 18))
        s.tree(244, v, "linden", rng.uniform(4, 18))
    s.lamps_round()
    return s.block()


def loop(name, cell, span, rng):
    """Concept B on a site 272 m wide, 560 m long (north to south)."""
    s = Site(name, cell, span, rng)
    all3 = range(LEVELS)
    for n, (u, v) in enumerate(((16, 20), (196, 20), (16, 300), (196, 300))):
        s.unit(all3, "anchor", s.urect(u, v, u + 60, v + 60),
               "%s %s" % (s.short, ("Department Store", "Fashion House", "Food Market", "Cinema")[n]))
    court = (136, 190)
    for k in all3:
        for u in range(76, 196, 12):
            s.unit([k], "food" if u == 136 else "shop", s.urect(u, 20, u + 12, 56))
            if k == 2:
                continue
            s.unit([k], "shop", s.urect(u, 324, u + 12, 360))
        for v in range(80, 300, 11):
            s.unit([k], "food" if v == 179 else "shop", s.urect(16, v, 52, v + 11))
            s.unit([k], "shop", s.urect(220, v, 256, v + 11))
        L = s.levels[k]
        L["walks"].append(s.rect(52, 56, 220, 324))
        if k:
            L["voids"].append(s.rect(92, 96, 180, 284))
            L["bridges"] += [s.rect(92, court[1] - 4, 180, court[1] + 4), s.rect(court[0] - 4, 96, court[0] + 4, 284)]
    s.unit([2], "foodhall", s.urect(76, 324, 196, 360), "%s Food Hall" % s.short)
    for n in range(12):                                              # kiosks in a ring round the grand fountain
        a = 2 * math.pi * n / 12
        u, v = court[0] + 32 * math.cos(a), court[1] + 66 * math.sin(a)
        s.unit([0], "kiosk", s.urect(u - 2, v - 2, u + 2, v + 2))
    s.fountains.append(dict(pos=s.w(*court), r=14.0, level=0))
    s.hall((16, 20, 256, 360), (136, 360), "loop")
    s.props.append(dict(kind="carpark", pos=s.w(136, 460), size=[232, 170], rows=8, cars=160))
    s.paths.append([s.w(136, 360), s.w(136, 375)])
    s.lamps_round()
    return s.block()


def canal(name, cell, span, rng):
    """Concept C on a site 560 m long (west to east), 272 m deep: the canal street in the north part, a car park along
    the south edge, three terraces stepping down to the east."""
    s = Site(name, cell, span, rng)
    all3 = range(LEVELS)
    A0, A1 = 64.0, 496.0                                              # where the street runs between the two big halls
    cc = lambda a: 105 + 16 * math.sin((a - A0) / 62)
    dc = lambda a: 16 / 62 * math.cos((a - A0) / 62)
    STEPS = (A0 + (A1 - A0) / 3, A0 + 2 * (A1 - A0) / 3)
    for (a0, a1), h in zip(((0, STEPS[0]), STEPS, (STEPS[1], s.W)), (6.0, 3.0, 0.0)):
        s.terraces.append(dict(rect=[r2(x) for x in s.urect(a0, 0, a1, s.D)], ground=h))
    s.unit(all3, "anchor", s.urect(10, 20, 60, 190), "%s Market Hall" % s.short)
    s.unit(all3, "foodhall", s.urect(500, 20, 550, 190), "%s Terrace Food Hall" % s.short)

    def at(a, off):
        """The point `off` metres from the canal's centre line (positive = south) at a, and the unit tangent there."""
        t = (1.0, dc(a))
        L = math.hypot(*t)
        t = (t[0] / L, t[1] / L)
        n = (-t[1], t[0])
        return (a + n[0] * off, cc(a) + n[1] * off), t, n

    def band(o0, o1, a0=A0, a1=A1, step=4.0):
        k = max(2, int((a1 - a0) / step))
        side0 = [at(a0 + (a1 - a0) * i / k, o0)[0] for i in range(k + 1)]
        side1 = [at(a0 + (a1 - a0) * i / k, o1)[0] for i in range(k + 1)]
        return [s.w(*q) for q in side0 + side1[::-1]]

    def across(a, o0, o1, width):
        (p0, t, _), (p1, _, _) = at(a, o0), at(a, o1)
        h = width / 2
        return [s.w(p0[0] - t[0] * h, p0[1] - t[1] * h), s.w(p0[0] + t[0] * h, p0[1] + t[1] * h),
                s.w(p1[0] + t[0] * h, p1[1] + t[1] * h), s.w(p1[0] - t[0] * h, p1[1] - t[1] * h)]

    for a0, a1 in ((A0, STEPS[0]), STEPS, (STEPS[1], A1)):
        s.water.append(band(-5, 5, a0, a1))
    for a in STEPS:
        s.falls.append([s.w(*at(a, -5)[0]), s.w(*at(a, 5)[0])])
    squares = [(135, -1), (280, 1), (425, -1)]                        # quay squares with a fountain: where along, which side
    for k in all3:
        L = s.levels[k]
        if k == 0:
            L["walks"] += [band(-14, -5), band(5, 14)]
            for a in (120, 190, 260, 330, 400, 460):
                L["bridges"].append(across(a, -6, 6, 5))
        else:                                                           # galleries along the fronts of the upper floors
            L["walks"] += [band(-14, -10), band(10, 14)]
            if k == 2:
                for a in (160, 300, 430):
                    L["bridges"].append(across(a, -14, 14, 5))
        for side in (-1, 1):
            a = A0 + 6
            while a < A1 - 6:
                (c, t, n) = at(a, side * 27)
                if any(abs(a - qa) < 12 and qs == side for qa, qs in squares) and k == 0:
                    a += 11
                    continue
                toward = (-n[0] * side, -n[1] * side)
                theta = math.degrees(math.atan2(toward[0], -toward[1]))
                cx, cz = s.X0 + c[0], s.Z0 + c[1]
                kind = "food" if (k == 2 or int(a) % 5 == 0) else "shop"
                s.unit([k], kind, [cx - 4.5, cz - 12, cx + 4.5, cz + 12], angle=theta)
                a += 11
    for qa, side in squares:
        s.fountains.append(dict(pos=s.w(*at(qa, side * 10)[0]), r=4.0, level=0))
    s.hall((10, 20, 550, 190), (280, 190), "canal", open_air=True)
    s.props.append(dict(kind="carpark", pos=s.w(280, 232), size=[520, 64], rows=3, cars=200))
    s.paths.append([s.w(280, 190), s.w(280, s.D + SW / 2)])
    for a in range(int(A0) + 10, int(A1), 30):                                   # trees along the quays
        for side in (-1, 1):
            q = at(a, side * 12)[0]
            s.tree(q[0], q[1], "plane", rng.uniform(4, 20))
    s.lamps_round()
    return s.block()


CONCEPTS = dict(galleria=galleria, loop=loop, canal=canal)
