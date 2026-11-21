"""Writes the starting city as data: World/city_01/city.json and one file per block in World/city_01/blocks/.

The city is a grid of blocks. Each block file lists its lots (buildings and open places), their doors,
what is inside them, paths, trees and props, all in world metres (x east, z south). Nothing about the
city lives in code that reads it: to change or grow the city, edit or add block files (by hand, with
a tool, or by re-running a generator like this one) and list them in city.json.

Run:  python make_city_01.py        (deterministic: the same seed gives the same city)
"""
import json, os, random, shutil

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "World"))
OUT = os.path.join(ROOT, "city_01")
PITCH, STREET, SIZE = 96.0, 16.0, 80.0           # block spacing, street width, block size
GRID = 8
ALLEY = 4.4                                       # width of the alleys into a courtyard
SEED = 20261121

PLAN = [  # j (north to south) rows of i (west to east)
    "rows rows rows park rows rows rows rows",
    "rows peri peri peri peri peri towerR rows",
    "rows peri towerO peri mall park peri rows",
    "rows peri peri hall towerO peri peri rows",
    "rows peri park market towerO hospital peri rows",
    "rows peri peri peri peri peri peri rows",
    "rows towerR peri peri park peri peri rows",
    "utility rows rows rows rows rows rows utility",
]
STOPS = {(3, 4), (0, 4), (7, 4), (3, 0), (4, 7), (5, 2)}       # blocks with a tram stop on their south pavement
DISTRICT = lambda i, j: "Centre" if 2 <= i <= 5 and 2 <= j <= 5 else "Old Town" if 1 <= i <= 6 and 1 <= j <= 6 else "Outskirts"
FIRST = ["Asha", "Tomas", "Mina", "Jon", "Ruth", "Kofi", "Ines", "Bo", "Dev", "Lena", "Omar", "Yui", "Sam", "Pia", "Raj", "Nell", "Abe", "Zed", "Fay", "Olu",
         "Edda", "Hugo", "Mei", "Walt", "Kit", "Noor", "Gus", "Ana", "Ben", "Cleo", "Dara", "Eli", "Femi", "Gia", "Hari", "Isla", "Jai", "Kira", "Leo", "Mara"]
STREETS_NS = ["West Wall", "Tanner", "Cooper", "Lantern", "Market", "Mill", "Weaver", "Garden", "East Wall"]
STREETS_EW = ["North Wall", "Plum", "Linden", "Fountain", "Baker", "Tram", "Quarry", "Depot", "South Wall"]


def grid(x0, z0, x1, z1, n, cols=3):
    rows = max(1, (n + cols - 1) // cols)
    return [[round(x0 + (x1 - x0) * (i % cols + 0.5) / cols, 2), round(z0 + (z1 - z0) * (i // cols + 0.5) / rows, 2)] for i in range(n)]


class Block:
    def __init__(self, i, j, template, rng):
        self.i, self.j, self.t, self.rng = i, j, template, rng
        self.x0, self.z0 = i * PITCH + STREET / 2, j * PITCH + STREET / 2
        self.lots, self.paths, self.trees, self.props = [], [], [], []
        self.id = "b_%d_%d" % (i, j)

    def W(self, u, v):
        return [round(self.x0 + u, 2), round(self.z0 + v, 2)]

    def R(self, u0, v0, u1, v1):
        return self.W(u0, v0) + self.W(u1, v1)

    def lot(self, kind, name, rect, door, floors=1, enter=True, **more):
        l = dict(id="%s_l%d" % (self.id, len(self.lots)), kind=kind, name=name, rect=self.R(*rect), door=self.W(*door), floors=floors, enterable=enter, objects={})
        l.update(more)
        self.lots.append(l)
        return l

    def path(self, a, b):
        self.paths.append([self.W(*a), self.W(*b)])

    def tree(self, u, v, species, age_years):
        self.trees.append(dict(id="%s_t%d" % (self.id, len(self.trees)), species=species, pos=self.W(u, v), planted=round(-age_years * 48), seed=self.rng.randrange(10 ** 6)))

    def prop(self, kind, u, v, **more):
        self.props.append(dict(kind=kind, pos=self.W(u, v), **more))

    def edge_point(self, side):
        """Where a path from the middle of a side meets the street centre line."""
        return dict(n=(40, -STREET / 2), s=(40, SIZE + STREET / 2), w=(-STREET / 2, 40), e=(SIZE + STREET / 2, 40))[side]

    # ---- what is inside a building: places to sleep, sit, work and stand, in world metres ----
    def fill(self, l, residents=0):
        x0, z0, x1, z1 = l["rect"]
        dx, dz = x1 - x0, z1 - z0
        k, o = l["kind"], l["objects"]
        wide = dx >= dz
        def band(a, b, n, cols):                        # a..b as fractions of the building's depth, measured from the back wall
            dox, doz = l["door"]
            if abs(doz - z0) < 0.01:
                return grid(x0 + 1.5, z1 - 1.2 - (b * (dz - 2.4)), x1 - 1.5, z1 - 1.2 - (a * (dz - 2.4)), n, cols)
            if abs(doz - z1) < 0.01:
                return grid(x0 + 1.5, z0 + 1.2 + a * (dz - 2.4), x1 - 1.5, z0 + 1.2 + b * (dz - 2.4), n, cols)
            if abs(dox - x0) < 0.01:
                return [[q[1], q[0]] for q in grid(z0 + 1.5, x1 - 1.2 - b * (dx - 2.4), z1 - 1.5, x1 - 1.2 - a * (dx - 2.4), n, cols)]
            return [[q[1], q[0]] for q in grid(z0 + 1.5, x0 + 1.2 + a * (dx - 2.4), z1 - 1.5, x0 + 1.2 + b * (dx - 2.4), n, cols)]
        if k == "home":
            n = max(1, residents)
            o["bed"], o["room"] = band(0.0, 0.42, n, min(n, 5)), band(0.5, 0.86, n, min(n, 5))
        elif k == "shop":
            staff = l.get("staff", 2)
            o["work"], o["customer"] = band(0.08, 0.26, staff, staff), band(0.45, 0.8, l.get("customers", 4), 4)
            if l.get("seats"):
                o["seat"] = band(0.55, 0.9, l["seats"], 6)
        elif k == "food":
            o["work"], o["seat"] = band(0.06, 0.24, 2, 2), band(0.42, 0.86, l.get("seats", 8), 4)
        elif k == "work":
            n = l.get("desks", 6)
            o["work"] = band(0.08, 0.8, n, 4 if n > 6 else 3)
        elif k == "health":
            o["work"], o["chair"] = band(0.06, 0.24, l.get("staff", 1), 4), band(0.5, 0.86, l.get("chairs", 4), 5)
        elif k == "utility":
            o["work"] = band(0.2, 0.7, l.get("staff", 2), 2)
        return l

    # ---- templates ----
    def courtyard(self, a, b, name, loop_in=6.0, beds=True):
        """A garden from a..b (local) with a strolling loop round it and paths out to all four streets."""
        la, lb = a - loop_in + 2, b + loop_in - 2
        loop = [self.W(la, la), self.W(lb, la), self.W(lb, lb), self.W(la, lb)]
        for p, q in zip(loop, loop[1:] + loop[:1]):
            self.paths.append([p, q])
        for side, inner in (("n", (40, la)), ("s", (40, lb)), ("w", (la, 40)), ("e", (lb, 40))):
            self.path(inner, self.edge_point(side))
        g = self.lot("garden", name, (a, a, b, b), (40, la), 0, False, loop=loop)
        m = (a + b) / 2.0
        seats = []
        for u, v, horiz in ((m - 6, a + 1.2, 1), (m + 6, a + 1.2, 1), (m - 6, b - 1.2, 1), (m + 6, b - 1.2, 1), (a + 1.2, m - 6, 0), (b - 1.2, m + 6, 0)):
            seats += [self.W(u - 0.5, v), self.W(u + 0.5, v)] if horiz else [self.W(u, v - 0.5), self.W(u, v + 0.5)]
            self.prop("bench", u, v, along="x" if horiz else "z")
        g["objects"]["bench"] = seats
        g["objects"]["rim"] = [self.W(m - 2.6, m + 1.5), self.W(m + 2.6, m + 1.5), self.W(m, m - 3.0)]
        self.prop("fountain", m, m, r=2.2)
        spots = [(a + 5, a + 5), (b - 5, a + 5), (a + 5, b - 5), (b - 5, b - 5)]
        g["objects"]["flowerbed"] = [self.W(u, v + 1.6) for u, v in spots]
        g["objects"]["work"] = g["objects"]["flowerbed"]
        if beds:
            for n, (u, v) in enumerate(spots):
                self.prop("flowerbed", u, v, color=n)
        for u, v in ((m - 7, m - 7), (m + 7, m - 7), (m - 7, m + 7), (m + 7, m + 7)):
            self.tree(u, v, self.rng.choice(["linden", "linden", "plum"]), self.rng.uniform(1.5, 14))
        return g

    def stop(self):
        s = self.lot("stop", "Tram stop", (33, 81, 47, 84.5), self.edge_point("s"), 0, False)
        s["objects"]["wait"] = [self.W(35 + 2.4 * n, 82.6) for n in range(5)]
        self.prop("shelter", 40, 82.6)

    def build(self):
        r, t = self.rng, self.t
        street = lambda side: {"n": STREETS_EW[self.j], "s": STREETS_EW[self.j + 1], "w": STREETS_NS[self.i], "e": STREETS_NS[self.i + 1]}[side]
        if t == "rows":                                           # houses facing the streets, gardens behind
            n = 0
            for k in range(4):
                for side, rect, door in (("n", (k * 20 + 3, 3, k * 20 + 17, 15), (k * 20 + 10, 3)), ("s", (k * 20 + 3, 65, k * 20 + 17, 77), (k * 20 + 10, 77))):
                    n += 1
                    self.lot("home", "%d %s Street" % (n + 10 * self.i, street(side)), rect, door, r.choice([2, 2, 3]), residents=r.choice([2, 2, 3, 3, 4]))
            self.lot("home", "%d %s Street" % (9 + 10 * self.j, street("w")), (3, 32, 15, 48), (3, 40), 2, residents=r.choice([2, 3]))
            self.lot("home", "%d %s Street" % (9 + 10 * self.j, street("e")), (65, 32, 77, 48), (77, 40), 2, residents=r.choice([2, 3]))
            for k in range(5):
                self.tree(24 + k * 8 + r.uniform(-2, 2), r.uniform(26, 54), r.choice(["plum", "linden", "plane"]), r.uniform(1, 25))
        elif t == "peri":                                         # a ring of buildings round a courtyard garden
            A, B = 40 - ALLEY / 2, 40 + ALLEY / 2               # the ring is cut by four narrow alleys, each under two stone arches
            rects = [("n", (0, 0, A, 16), (18.9, 0)), ("n", (B, 0, 80, 16), (61.1, 0)), ("s", (0, 64, A, 80), (18.9, 80)), ("s", (B, 64, 80, 80), (61.1, 80)),
                     ("w", (0, 16, 16, A), (0, 26.9)), ("w", (0, B, 16, 64), (0, 53.1)), ("e", (64, 16, 80, A), (80, 26.9)), ("e", (64, B, 80, 64), (80, 53.1))]
            for v in (1.0, 15.0, 65.0, 79.0):
                self.prop("arch", 40, v, span="x", width=ALLEY, thick=1.6)
                self.prop("arch", v, 40, span="z", width=ALLEY, thick=1.6)
            # each alley has the two lights of the reference scene: 45 W in the covered gateway, a 120 W wall lantern further in
            for (gu, gv, go), (au, av, ao) in (((A + 0.12, 4.5, (1, 0)), (B - 0.12, 11.5, (-1, 0))), ((A + 0.12, 75.5, (1, 0)), (B - 0.12, 68.5, (-1, 0)))):
                self.prop("lantern", gu, gv, height=3.1, power=45, out=list(go))
                self.prop("lantern", au, av, height=3.2, power=120, out=list(ao))
                self.prop("lantern", gv, gu, height=3.1, power=45, out=[go[1], go[0]])
                self.prop("lantern", av, au, height=3.2, power=120, out=[ao[1], ao[0]])
            kinds = ["home", "home", "home", "shop", "food", "work", r.choice(["home", "work"]), "health" if (self.i + self.j) % 3 == 0 else r.choice(["home", "food", "shop"])]
            r.shuffle(kinds)
            for (side, rect, door), kind in zip(rects, kinds):
                nm = dict(home="%s Court" % street(side), shop="%s Grocer" % street(side), food="Café %s" % r.choice(FIRST), work="%s Works" % street(side), health="%s Clinic" % street(side))[kind]
                self.lot(kind, nm, rect, door, r.choice([3, 4, 4, 5]) if kind == "home" else r.choice([2, 3, 4]), residents=r.choice([4, 5, 6]) if kind == "home" else 0)
            self.courtyard(26, 54, "%s Court garden" % street("n"))
        elif t in ("towerO", "towerR"):                           # a tower on a plaza with two pavilions
            office = t == "towerO"
            floors = r.choice([16, 19, 22]) if office else r.choice([12, 14])
            name = "%s Tower" % street("s") if office else "%s Heights" % street("s")
            self.lot("work" if office else "home", name, (25, 20, 55, 50), (40, 50), floors, desks=12, residents=0 if office else 12, tower=True)
            self.lot("food", "Plaza Café", (5, 60, 21, 73), (13, 60), 1, seats=6)
            self.lot("shop", "Plaza Kiosk", (59, 60, 75, 73), (67, 60), 1, customers=3)
            self.path((40, 50), (40, 56))
            self.path((40, 56), self.edge_point("s"))
            self.path((-STREET / 2, 56), (40, 56))
            self.path((40, 56), (SIZE + STREET / 2, 56))
            for u, v in ((10, 10), (70, 10), (10, 40), (70, 40), (30, 66), (50, 66)):
                self.tree(u, v, "plane" if office else "linden", r.uniform(4, 18))
            for u in (30, 50):
                self.prop("bench", u, 58.5, along="x")
        elif t == "park":
            g = self.courtyard(14, 66, "%s Park" % street("n"), loop_in=6.0)
            for k in range(9):
                self.tree(r.uniform(18, 62), r.uniform(18, 62), r.choice(["linden", "plum", "plane", "linden"]), r.uniform(0.5, 30))
        elif t in ("mall", "hospital", "hall"):                   # one big building with a forecourt
            if t == "mall":
                self.lot("shop", "Lantern Arcade", (8, 10, 72, 52), (40, 52), 2, staff=4, customers=12, seats=12, tags=["cafe"])
            elif t == "hospital":
                self.lot("health", "Mill Street Hospital", (10, 12, 70, 50), (40, 50), 5, staff=4, chairs=10)
            else:
                self.lot("work", "Town Hall", (16, 14, 64, 48), (40, 48), 3, desks=8)
            self.path((40, 52 if t == "mall" else 50 if t == "hospital" else 48), self.edge_point("s"))
            for u in (12, 26, 54, 68):
                self.tree(u, 66, "linden", r.uniform(5, 20))
            for u in (20, 60):
                self.prop("bench", u, 60, along="x")
        elif t == "market":
            for side in "nswe":
                self.path((40, 40), self.edge_point(side))
            m = self.lot("market", "Market Square", (18, 26, 62, 54), (40, 40), 0, False)
            stalls = [(24 + 10 * k, v) for v in (31, 49) for k in range(4)]
            m["objects"]["stall"] = [self.W(u, v) for u, v in stalls]
            m["objects"]["customer"] = [self.W(u + du, v + (2.6 if v < 40 else -2.6)) for u, v in stalls for du in (-1.2, 1.2)]
            for n, (u, v) in enumerate(stalls):
                self.prop("stall", u, v, color=n, face=1 if v < 40 else -1)
            self.lot("food", "Market Café", (4, 4, 26, 18), (15, 18), 2, seats=8)
            self.lot("shop", "Market Grocer", (54, 4, 76, 18), (65, 18), 2)
            self.lot("food", "Tram Café", (4, 62, 26, 76), (15, 62), 2, seats=8)
            self.lot("work", "Guild House", (54, 62, 76, 76), (65, 62), 3, desks=6)
            self.prop("fountain", 40, 40, r=1.6)
        elif t == "utility":
            self.lot("utility", "%s Depot" % street("n"), (8, 8, 44, 30), (26, 30), 2, staff=3)
            self.lot("utility", "Substation", (52, 50, 72, 66), (62, 50), 1, enter=False)
            self.prop("watertower", 60, 22)
            self.path((26, 30), (26, 56))
            self.path((-STREET / 2, 56), (26, 56))
            self.path((26, 56), (40, 56))
            self.path((40, 56), (SIZE + STREET / 2, 56))
            self.path((40, 56), self.edge_point("s"))
            for u in (14, 30, 46):
                self.tree(u, 70, "plane", r.uniform(8, 30))
        if (self.i, self.j) in STOPS:
            self.stop()
        for l in self.lots:
            if l["rect"][0] != l["rect"][2] and l["kind"] not in ("garden", "market", "stop"):
                self.fill(l, l.get("residents", 0))
        for u, v in ((-4, -4), (40, -4), (-4, 40)):               # street lamps: one corner and two mid-sides per block tile the city
            self.prop("lamp", u, v)
        for k in range(3):                                        # street trees on the north and west pavements
            if r.random() < 0.75:
                self.tree(14 + k * 26, -3.5, "plane", r.uniform(3, 40))
            if r.random() < 0.6:
                self.tree(-3.5, 14 + k * 26, "plane", r.uniform(3, 40))
        return dict(id=self.id, grid=[self.i, self.j], template=t, district=DISTRICT(self.i, self.j), origin=self.W(0, 0), size=SIZE,
                    lots=self.lots, paths=self.paths, trees=self.trees, props=self.props)


def main():
    rng = random.Random(SEED)
    if os.path.isdir(os.path.join(OUT, "blocks")):
        shutil.rmtree(os.path.join(OUT, "blocks"))
    os.makedirs(os.path.join(OUT, "blocks"))
    index, counts = [], {}
    for j, row in enumerate(PLAN):
        for i, t in enumerate(row.split()):
            b = Block(i, j, t, random.Random(rng.randrange(10 ** 9))).build()
            json.dump(b, open(os.path.join(OUT, "blocks", b["id"] + ".json"), "w"), indent=1, ensure_ascii=False)
            index.append(dict(id=b["id"], grid=[i, j], file="blocks/%s.json" % b["id"], template=t, district=b["district"]))
            for l in b["lots"]:
                counts[l["kind"]] = counts.get(l["kind"], 0) + 1
    city = dict(
        schema=1, id="city_01", name="Old Town", seed=SEED,
        note="The starting city. Blocks are listed here and stored one per file; add a block by adding a file and an entry. Units: metres, x east, z south.",
        grid=dict(columns=GRID, rows=GRID, pitch=PITCH, street_width=STREET, block_size=SIZE),
        streets=dict(north_south=STREETS_NS, east_west=STREETS_EW),
        spawn=dict(pos=[3 * PITCH + 8.0 + 4.5, 5 * PITCH + 8.0 + 40.0], facing=[1, 0], time="07:20", note="In an alley south of Market Square, looking east through its arches into the morning sun"),
        rules="../rules", blocks=index)
    json.dump(city, open(os.path.join(OUT, "city.json"), "w"), indent=1, ensure_ascii=False)
    print(len(index), "blocks;", ", ".join("%s %d" % kv for kv in sorted(counts.items())))
    residents(rng)


def residents(rng):
    """Who lives where, what they do and which amenities are theirs. Written to World/city_01/residents.json."""
    lots = [l for f in sorted(os.listdir(os.path.join(OUT, "blocks"))) for l in json.load(open(os.path.join(OUT, "blocks", f)))["lots"]]
    d2 = lambda a, b: abs(a["door"][0] - b["door"][0]) + abs(a["door"][1] - b["door"][1])
    people = []
    for l in lots:
        if l["kind"] == "home":
            for _ in range(l.get("residents", 0)):
                people.append(dict(id="r%04d" % len(people), name=rng.choice(FIRST), role=None, home=l["id"], work=None, offset=rng.randrange(-20, 21)))
    home = {l["id"]: l for l in lots}
    free = set(range(len(people)))

    def hire(lot, role, n):
        for _ in range(n):
            if not free:
                return
            k = min(free, key=lambda q: (d2(home[people[q]["home"]], lot), q))
            free.discard(k)
            people[k].update(role=role, work=lot["id"])

    for l in lots:
        o = l["objects"]
        if l["kind"] == "food":
            hire(l, "cafe", 2)
        elif l["kind"] == "shop":
            hire(l, "shopkeeper", len(o["work"]))
        elif l["kind"] == "health":
            hire(l, "nurse", len(o["work"]))
        elif l["kind"] == "utility" and l["enterable"]:
            hire(l, "utility", len(o["work"]))
        elif l["kind"] == "garden" and (l["name"].endswith("Park") or rng.random() < 0.3):
            hire(l, "gardener", 1)
        elif l["kind"] == "work":
            hire(l, "maker" if l["name"].endswith("Works") else "clerk", len(o["work"]))
    for k in sorted(free):
        people[k]["role"] = rng.choices(["elder", "commuter", "student", "courier"], [30, 40, 18, 12])[0]
    market = next(l for l in lots if l["kind"] == "market")
    sellers = sorted((p for p in people if p["role"] in ("shopkeeper", "maker")), key=lambda p: d2(home[p["home"]], market))[:len(market["objects"]["stall"])]
    for p in sellers:
        p["market_seller"] = True
    kinds = dict(cafe=[l for l in lots if l["kind"] == "food" or "cafe" in l.get("tags", [])], grocer=[l for l in lots if l["kind"] == "shop"],
                 garden=[l for l in lots if l["kind"] == "garden"], stop=[l for l in lots if l["kind"] == "stop"], clinic=[l for l in lots if l["kind"] == "health"])
    visitable = [l for l in lots if l["kind"] in ("shop", "food", "work", "health", "home", "utility") and l["enterable"]]
    for p in people:
        h = home[p["home"]]
        p["near"] = {tok: min(ls, key=lambda l: (d2(h, l), l["id"]))["id"] for tok, ls in kinds.items()}
        p["near"]["market"] = market["id"] if d2(h, market) < 260 else p["near"]["grocer"]
        if p["role"] in ("courier", "utility"):
            close = sorted(visitable, key=lambda l: (d2(h, l), l["id"]))[:40]
            p["rounds"] = [l["id"] for l in rng.sample(close, 12)]
    json.dump(dict(note="Residents of city_01: home, role, workplace and their nearest amenities. Generated by Tools/worldgen/make_city_01.py.", residents=people),
              open(os.path.join(OUT, "residents.json"), "w"), indent=0, ensure_ascii=False)
    roles = {}
    for p in people:
        roles[p["role"]] = roles.get(p["role"], 0) + 1
    print(len(people), "residents;", ", ".join("%s %d" % kv for kv in sorted(roles.items())))


if __name__ == "__main__":
    main()
