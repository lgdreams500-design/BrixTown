"""The civic row and the town park (docs/specs/domains/CITY_BALANCE.md §3.4, §4; the owner, 2026-11-22: "let's go ahead as
you advise"): what the balance report found missing outright, on the grid row between Old Town's south wall and
Southfield, along a new Civic Street that crosses the South Gate road.

  cells          block          what
  i 4-5, j 11-12 Town Park      a park of 3 ha: a loop path, a pond, benches, flowerbeds and trees
  i 7-8, j 12    Civic Row      Old Town Secondary School and its playing field
  i 9,   j 12    Civic Row      the fire station
  i 10,  j 12    Civic Row      the police station
  i 11-13, j 12  Civic Row      the town works: water works, power station, waste depot
Civic Street runs along the street line z = 13 x pitch from the park's west edge to the works' east end (roads.json
"links", id civic_street); every building's door faces it.
Run from Tools/worldgen, before add_outskirts_01.py (which levels the civic row's ground and keeps this link):
    python add_civic_01.py ; python add_outskirts_01.py
Every size and count here is a first guess [ASSUMPTION].
"""
import json, math, os, random

import make_city_01 as mc

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")
P, SW = mc.PITCH, mc.STREET
SEED = 20261123
OURS = ("Civic Row", "Town Park")
JOBS = None


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


class Site:
    def __init__(self, cell, span, template, district, rng):
        self.cell, self.span, self.t, self.d, self.rng = cell, span, template, district, rng
        self.id = "b_%d_%d" % cell
        self.X0, self.Z0 = cell[0] * P + SW / 2, cell[1] * P + SW / 2
        self.W, self.D = span[0] * P - SW, span[1] * P - SW
        self.lots, self.paths, self.trees, self.props = [], [], [], []

    def w(self, u, v):
        return [round(self.X0 + u, 2), round(self.Z0 + v, 2)]

    def r(self, u0, v0, u1, v1):
        return [round(self.X0 + u0, 2), round(self.Z0 + v0, 2), round(self.X0 + u1, 2), round(self.Z0 + v1, 2)]

    def building(self, kind, name, rect, floors, sector, **more):
        """A building whose door is in the middle of its south wall, on a path down to Civic Street, with a place to work
        for each of the jobs its floor area gives (World/rules/balance.json 'jobs')."""
        u0, v0, u1, v1 = rect
        l = dict(id="%s_l%d" % (self.id, len(self.lots)), kind=kind, name=name, rect=self.r(*rect), door=self.w((u0 + u1) / 2, v1),
                 floors=floors, enterable=True, objects={})
        l.update(more)
        n = max(1, int(round((u1 - u0) * (v1 - v0) * floors * JOBS["usable_share"] / JOBS["m2_per_job"][sector])))
        cols = max(1, min(8, int((u1 - u0 - 3) / 2.5)))
        rows = (min(n, cols * 6) + cols - 1) // cols
        spots = [self.w(u0 + 1.5 + (u1 - u0 - 3) * (k % cols + .5) / cols, v0 + 1.5 + (v1 - v0 - 3) * (k // cols + .5) / rows) for k in range(min(n, cols * 6))]
        l["objects"]["work"] = spots
        l["staff"] = n
        if floors > 1:                                                  # the rest of the staff work upstairs, as add_storeys_01 spreads them
            l["levels"] = dict(work=[k % floors for k in range(len(spots))])
        self.lots.append(l)
        self.paths.append([l["door"], self.w((u0 + u1) / 2, self.D + SW / 2)])
        return l

    def lamps(self):
        for u in range(0, int(self.W) + 1, 40):
            self.props.append(dict(kind="lamp", pos=self.w(u, self.D + 4)))

    def block(self):
        return dict(id=self.id, grid=list(self.cell), template=self.t, district=self.d, origin=[self.X0, self.Z0], size=mc.SIZE,
                    span=list(self.span), lots=self.lots, paths=self.paths, trees=self.trees, props=self.props)


def school(rng):
    s = Site((7, 12), (2, 1), "civic", "Civic Row", rng)
    s.building("school", "Old Town Secondary School", (12, 36, 52, 60), 3, "education", tags=["school", "secondary"])
    f = dict(id="%s_l%d" % (s.id, len(s.lots)), kind="garden", name="School Playing Field", rect=s.r(66, 8, 168, 72), door=s.w(117, 72),
             floors=0, enterable=False, objects={}, tags=["field"])
    x0, z0, x1, z1 = f["rect"]
    f["loop"] = [[x0 + 4, z0 + 4], [x1 - 4, z0 + 4], [x1 - 4, z1 - 4], [x0 + 4, z1 - 4]]   # a walk round the field
    s.lots.append(f)
    s.paths.append([s.w(117, 72), s.w(117, s.D + SW / 2)])
    for u in range(70, 166, 12):
        s.trees.append(dict(id="%s_t%d" % (s.id, len(s.trees)), species="plane", pos=s.w(u, 4), planted=round(-rng.uniform(5, 30) * 48), seed=rng.randrange(10 ** 6)))
    s.lamps()
    return s.block()


def fire(rng):
    s = Site((9, 12), (1, 1), "civic", "Civic Row", rng)
    s.building("fire", "Old Town Fire Station", (20, 40, 60, 64), 1, "services", tags=["fire"])
    s.props.append(dict(kind="carpark", pos=s.w(40, 24), size=[36, 22], rows=1, cars=3))
    s.lamps()
    return s.block()


def police(rng):
    s = Site((10, 12), (1, 1), "civic", "Civic Row", rng)
    s.building("police", "Old Town Police Station", (25, 44, 55, 62), 2, "services", tags=["police"])
    s.props.append(dict(kind="carpark", pos=s.w(40, 24), size=[40, 20], rows=2, cars=8))
    s.lamps()
    return s.block()


def works(rng):
    s = Site((11, 12), (3, 1), "civic", "Civic Row", rng)
    for u, name in ((8, "Town Water Works"), (64, "Town Power Station"), (120, "Town Waste Depot")):
        s.building("utility", name, (u, 30, u + 50, 60), 2, "utility", tags=["works"])
    for u in (20, 76, 132):
        s.props.append(dict(kind="watertower" if u == 20 else "carpark", pos=s.w(u + 12, 15), **({} if u == 20 else dict(size=[30, 18], rows=1, cars=6))))
    s.lamps()
    return s.block()


def park(rng):
    s = Site((4, 11), (2, 2), "park", "Town Park", rng)
    W, D = s.W, s.D
    loop = [s.w(12, 12), s.w(W - 12, 12), s.w(W - 12, D - 12), s.w(12, D - 12)]
    g = dict(id="%s_l0" % s.id, kind="garden", name="Town Park", rect=s.r(4, 4, W - 4, D - 4), door=s.w(W / 2, D - 4), floors=0,
             enterable=False, objects={}, loop=loop, tags=["park"])
    s.paths += [[p, q] for p, q in zip(loop, loop[1:] + loop[:1])] + [[s.w(W / 2, D - 12), s.w(W / 2, D + SW / 2)], [s.w(12, D / 2), s.w(W - 12, D / 2)]]
    benches = [(W / 2 - 20, 13.2, "x"), (W / 2 + 20, 13.2, "x"), (W / 2 - 20, D - 13.2, "x"), (W / 2 + 20, D - 13.2, "x"),
               (13.2, D / 2 - 20, "z"), (13.2, D / 2 + 20, "z"), (W - 13.2, D / 2 - 20, "z"), (W - 13.2, D / 2 + 20, "z")]
    g["objects"]["bench"] = [s.w(u - .5, v) if al == "x" else s.w(u, v - .5) for u, v, al in benches] + [s.w(u + .5, v) if al == "x" else s.w(u, v + .5) for u, v, al in benches]
    g["objects"]["flowerbed"] = [s.w(u, v) for u, v in ((30, 30), (W - 30, 30), (30, D - 30), (W - 30, D - 30), (W / 2 - 16, D / 2 + 22), (W / 2 + 16, D / 2 + 22))]
    g["objects"]["work"] = g["objects"]["flowerbed"]
    for u, v, al in benches:
        s.props.append(dict(kind="bench", pos=s.w(u, v), along=al))
    for n, q in enumerate(g["objects"]["flowerbed"]):
        s.props.append(dict(kind="flowerbed", pos=q, color=n % 4))
    s.props.append(dict(kind="pond", pos=s.w(W / 2, D / 2 - 30), r=14.0))
    s.props.append(dict(kind="fountain", pos=s.w(W / 2, D / 2 + 14), r=3.0))
    for k in range(48):
        a = rng.uniform(0, 2 * math.pi)
        u, v = W / 2 + math.cos(a) * rng.uniform(26, W / 2 - 18), D / 2 + math.sin(a) * rng.uniform(26, D / 2 - 18)
        s.trees.append(dict(id="%s_t%d" % (s.id, len(s.trees)), species=rng.choice(["linden", "plane", "plum", "plum"]), pos=s.w(u, v),
                            planted=round(-rng.uniform(3, 60) * 48), seed=rng.randrange(10 ** 6)))
    s.lots.append(g)
    s.lamps()
    return s.block()


def main():
    global JOBS
    JOBS = jload(os.path.join(RULES, "balance.json"))["jobs"]
    city_p = os.path.join(CITY, "city.json")
    city = jload(city_p)
    for e in [e for e in city["blocks"] if e["district"] in OURS]:     # what this script made before
        p = os.path.join(CITY, e["file"])
        if os.path.exists(p):
            os.remove(p)
    city["blocks"] = [e for e in city["blocks"] if e["district"] not in OURS]
    taken = {(e["grid"][0] + di, e["grid"][1] + dj) for e in city["blocks"] for di in range(e.get("span", [1, 1])[0]) for dj in range(e.get("span", [1, 1])[1])}
    rng = random.Random(SEED)
    made = [park(rng), school(rng), fire(rng), police(rng), works(rng)]
    for d in made:
        cells = {(d["grid"][0] + di, d["grid"][1] + dj) for di in range(d["span"][0]) for dj in range(d["span"][1])}
        assert not cells & taken, (d["id"], sorted(cells & taken))
        taken |= cells
        with open(os.path.join(CITY, "blocks", d["id"] + ".json"), "w", encoding="utf-8", newline="\n") as f:
            json.dump(d, f, indent=1, ensure_ascii=False)
        city["blocks"].append(dict(id=d["id"], grid=d["grid"], file="blocks/%s.json" % d["id"], template=d["template"], district=d["district"], span=d["span"]))
    with open(city_p, "w", encoding="utf-8", newline="\n") as f:
        json.dump(city, f, indent=1, ensure_ascii=False)

    roads_p = os.path.join(CITY, "roads.json")
    roads = jload(roads_p)
    z = 13 * P
    link = dict(id="civic_street", name="Civic Street", width=9.0, limit_kmh=30.0, gate=None,
                to=dict(crosses="gate_south", at=[10 * P, z]), points=[[x, z] for x in range(int(4 * P), int(14 * P) + 1, 8)])
    roads["links"] = [l for l in roads.get("links", []) if l["id"] != "civic_street"] + [link]
    with open(roads_p, "w", encoding="utf-8", newline="\n") as f:
        json.dump(roads, f, indent=1)
        f.write("\n")
    for d in made:
        print("%-6s %-10s %s" % (d["id"], d["district"], ", ".join("%s (%s jobs)" % (l["name"], l.get("staff", 0)) for l in d["lots"])))


if __name__ == "__main__":
    main()
