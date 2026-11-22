"""Divide every court building and residential tower of city_01 into flats, floor by floor (docs/specs/domains/
CITY_BALANCE.md BAL-006; the owner's request of 2026-11-22: "court and tower buildings ... floor plans updated to fit in
each residential unit, with its own door and accessible").

Each floor of a court ('peri' block) or tower ('towerR') home is laid out in the building's own frame: `a` runs away
from the side wall the stair stands against, `c` away from the street-door wall.
  core      a strip at the stair's end, full depth: the stair (and the lift in a tower) and the landing round them
  corridor  1.6 m wide down the middle of the floor from the core, as far as the flats need it
  lobby     on the ground floor, from the street door to the corridor
  flats     in a row each side of the corridor, each with its door on the corridor; a floor may end in one flat the full
            depth of the building (320 m², or a 640 m² penthouse on a tower's top floor) whose door is at the end of the
            corridor
  stores    what is left of a row: shared storerooms, bins and bikes, plant (doors on the corridor)
The flats take (1 - common_share) of the floor's area (World/rules/balance.json "homes"); sizes follow the mix there.
Written into the lot:
  flats   [{id, name, floor, size, area, rect, door, beds: [indices into objects.bed], rooms: [...], household: [resident ids]}]
  common  [{floor, kind: core | corridor | lobby | store, rect, door?}]
  objects.bed / objects.room and levels.bed / levels.room: the residents who live there first, in their order in
  residents.json (the slice gives the k-th resident the k-th bed), each in the flat they now live in; then the beds and
  rooms of every other flat, furnished and empty.
Run from Tools/worldgen:   python add_flats_01.py        (running it again writes the same data)
"""
import json, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def jsave(p, o, indent=1):
    with open(p, encoding="utf-8") as f:
        end = "\n" if f.read().endswith("\n") else ""
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        json.dump(o, f, indent=indent, ensure_ascii=False)
        f.write(end)


H = jload(os.path.join(RULES, "balance.json"))["homes"]
WALL = jload(os.path.join(RULES, "storeys.json"))["wall"]
SIZES = H["sizes"]
MIX = {int(k): v for k, v in H["mix"].items()}
BEDS = {int(k): v for k, v in H["beds"].items()}
CORR, LOBBY, KEEP = H["corridor"], H["lobby"], 1 - H["common_share"]
UA = {"w": (1, 0), "e": (-1, 0), "n": (0, 1), "s": (0, -1)}


class Frame:
    """A building's own frame: a away from the stair's wall, c away from the street-door wall, from their corner."""

    def __init__(self, l):
        x0, z0, x1, z1 = l["rect"]
        dx, dz = l["door"]
        door_wall = "n" if abs(dz - z0) < .01 else "s" if abs(dz - z1) < .01 else "w" if abs(dx - x0) < .01 else "e"
        self.ua, self.uc = UA[l["stair"]["wall"]], UA[door_wall]
        self.cx = x0 if 1 in (self.ua[0], self.uc[0]) else x1
        self.cz = z0 if 1 in (self.ua[1], self.uc[1]) else z1
        self.L = abs(self.ua[0]) * (x1 - x0) + abs(self.ua[1]) * (z1 - z0)
        self.D = abs(self.uc[0]) * (x1 - x0) + abs(self.uc[1]) * (z1 - z0)

    def w(self, a, c):
        return [round(self.cx + a * self.ua[0] + c * self.uc[0], 2), round(self.cz + a * self.ua[1] + c * self.uc[1], 2)]

    def local(self, x, z):
        dx, dz = x - self.cx, z - self.cz
        return dx * self.ua[0] + dz * self.ua[1], dx * self.uc[0] + dz * self.uc[1]

    def rect(self, a0, c0, a1, c1):
        p, q = self.w(a0, c0), self.w(a1, c1)
        return [min(p[0], q[0]), min(p[1], q[1]), max(p[0], q[0]), max(p[1], q[1])]


class Mix:
    """Which size the city most lacks, so the flats of all buildings together keep to the mix."""

    def __init__(self):
        self.n = {s: 0 for s in SIZES}

    def order(self):
        tot = sum(self.n.values()) + 1
        return sorted(SIZES, key=lambda s: -(MIX[s] * tot - self.n[s]))

    def take(self, s):
        self.n[s] += 1


def spots(F, r, door, n, frac):
    """n places in a line across the flat at `frac` of its depth from its door wall (local rect r, door (a, c))."""
    a0, c0, a1, c1 = r
    da, dc = door
    if abs(dc - c1) < .01 or abs(dc - c0) < .01:                       # door on a c-wall: depth runs along c
        cc = c1 - frac * (c1 - c0) if abs(dc - c1) < .01 else c0 + frac * (c1 - c0)
        return [F.w(a0 + 1.0 + (a1 - a0 - 2.0) * (k + .5) / n, cc) for k in range(n)]
    aa = a1 - frac * (a1 - a0) if abs(da - a1) < .01 else a0 + frac * (a1 - a0)
    return [F.w(aa, c0 + 1.0 + (c1 - c0 - 2.0) * (k + .5) / n) for k in range(n)]


def lay_out(l, tower, mix):
    """The flats and common areas of every floor of lot l, in its frame."""
    F = Frame(l)
    L, D, t = F.L, F.D, WALL
    s = l["stair"]["rect"]
    sa = [F.local(s[0], s[1])[0], F.local(s[2], s[3])[0]]
    core_a1 = max(sa) + .05
    if l.get("lift"):
        core_a1 = max(core_a1, F.local(*l["lift"])[0] + 1.25)
    cm = D / 2
    k0, k1 = cm - CORR / 2, cm + CORR / 2
    door_a = F.local(*l["door"])[0]
    flats, common = [], []
    for f in range(l["floors"]):
        gross = L * D
        budget = KEEP * gross
        end_a = L - t
        made = []
        top = f == l["floors"] - 1
        # a flat the full depth at the far end, when the city lacks one of the big sizes and it fits
        can = [z for z in mix.order() if z != 640 or (tower and top)]     # a 640 m² penthouse only on a tower's top floor
        size = 640 if (tower and top and H["penthouse_only_top_floor"]) else can[0]    # every tower is crowned by a penthouse
        if size >= 320:
            area = min(size, budget + 40)                               # a size is nominal: within size_tolerance of it
            span = area / (D - 2 * t)
            if span <= end_a - core_a1 - 4 and area >= size * (1 - H["size_tolerance"]):
                made.append(dict(size=size, r=(end_a - span, t, end_a, D - t), door=(end_a - span, cm)))
                end_a -= span
                budget -= area
                mix.take(size)
        # the rows either side of the corridor, as runs of free a
        front = [(core_a1, end_a)]
        if f == 0:
            front = [(core_a1, door_a - LOBBY / 2), (door_a + LOBBY / 2, end_a)]
        rows = [[a0, a1, t, k0, "front"] for a0, a1 in front if a1 - a0 > .5] + [[core_a1, end_a, k1, D - t, "back"]]
        while True:
            placed = False
            for size in [z for z in mix.order() if z != 640]:
                if size > budget + 40:
                    continue
                for row in sorted(rows, key=lambda r: -(r[1] - r[0])):
                    depth = row[3] - row[2]
                    width = size / depth
                    if width <= row[1] - row[0] + .01 and width >= 3.2:
                        a0 = row[0]
                        dc = row[3] if row[4] == "front" else row[2]
                        made.append(dict(size=size, r=(a0, row[2], a0 + width, row[3]), door=(a0 + min(1.0, width / 2), dc)))
                        row[0] += width
                        budget -= size
                        mix.take(size)
                        placed = True
                        break
                if placed:
                    break
            if not placed:
                break
        common.append(dict(floor=f, kind="core", rect=F.rect(t, t, core_a1, D - t)))
        common.append(dict(floor=f, kind="corridor", rect=F.rect(core_a1, k0, end_a, k1)))
        if f == 0:
            common.append(dict(floor=f, kind="lobby", rect=F.rect(door_a - LOBBY / 2, t, door_a + LOBBY / 2, k0)))
        for row in rows:                                               # what is left of a row: a shared store
            if row[1] - row[0] >= 1.2:
                dc = row[3] if row[4] == "front" else row[2]
                common.append(dict(floor=f, kind="store", rect=F.rect(row[0], row[2], row[1], row[3]),
                                   door=F.w(row[0] + min(.8, (row[1] - row[0]) / 2), dc)))
        made.sort(key=lambda m: (m["r"][0], m["r"][1]))
        for n, m in enumerate(made, 1):
            r = m["r"]
            flats.append(dict(id="%s:f%d.%02d" % (l["id"], f, n), name="Flat %d.%02d" % (f, n), floor=f, size=m["size"],
                              area=round((r[2] - r[0]) * (r[3] - r[1]), 1), rect=F.rect(*r), door=F.w(*m["door"]),
                              _local=r, _door=m["door"]))
    return F, flats, common


def furnish(l, F, flats, mates):
    """Beds and living places in every flat; the residents who live here first, each in the flat they now live in."""
    beds, rooms, bl, rl = [], [], [], []
    for fl in flats:
        n = BEDS[fl["size"]]
        fl["_beds"] = spots(F, fl["_local"], fl["_door"], n, .78)
        fl["_rooms"] = spots(F, fl["_local"], fl["_door"], n, .38)
        fl["beds"], fl["rooms"], fl["household"] = [], [], []
    order = sorted(flats, key=lambda q: (q["floor"] == 0, q["floor"], q["id"]))        # households upstairs first, as before
    free = [(q, k) for q in order for k in range(len(q["_beds"]))]
    taken = set()
    for rid in mates:                                                    # residents keep sharing flats as they shared the building
        q, k = free[len(taken)]
        taken.add((q["id"], k))
        q["household"].append(rid)
    for q, k in [(q, k) for q in order for k in range(len(q["_beds"])) if (q["id"], k) in taken] + \
                [(q, k) for q in order for k in range(len(q["_beds"])) if (q["id"], k) not in taken]:
        q["beds"].append(len(beds))
        beds.append(q["_beds"][k])
        bl.append(q["floor"])
        q["rooms"].append(len(rooms))
        rooms.append(q["_rooms"][k])
        rl.append(q["floor"])
    l["objects"]["bed"], l["objects"]["room"] = beds, rooms
    l.setdefault("levels", {})
    l["levels"]["bed"], l["levels"]["room"] = bl, rl
    for q in flats:
        for k in ("_local", "_door", "_beds", "_rooms"):
            q.pop(k)


def problems(l, flats, common):
    """Overlaps, doors that do not open on to a common area, flats over the stair or lift, flats outside the walls."""
    out = []
    inside = lambda r, R, e=.01: R[0] - e <= r[0] and R[1] - e <= r[1] and r[2] <= R[2] + e and r[3] <= R[3] + e
    over = lambda p, q: p[0] < q[2] - .01 and q[0] < p[2] - .01 and p[1] < q[3] - .01 and q[1] < p[3] - .01
    on_edge = lambda d, r: (r[0] - .01 <= d[0] <= r[2] + .01 and r[1] - .01 <= d[1] <= r[3] + .01 and
                            min(abs(d[0] - r[0]), abs(d[0] - r[2]), abs(d[1] - r[1]), abs(d[1] - r[3])) < .02)
    x0, z0, x1, z1 = l["rect"]
    wall = [x0 + WALL, z0 + WALL, x1 - WALL, z1 - WALL]
    st = l["stair"]["rect"]
    lift = l.get("lift")
    for f in range(l["floors"]):
        fs = [q for q in flats if q["floor"] == f]
        cs = [c for c in common if c["floor"] == f]
        for i, q in enumerate(fs):
            if not inside(q["rect"], wall):
                out.append("%s outside the walls" % q["id"])
            if over(q["rect"], st) or (lift and over(q["rect"], [lift[0] - 1.1, lift[1] - 1.1, lift[0] + 1.1, lift[1] + 1.1])):
                out.append("%s over the stair or lift" % q["id"])
            for p in fs[i + 1:]:
                if over(q["rect"], p["rect"]):
                    out.append("%s overlaps %s" % (q["id"], p["id"]))
            if not on_edge(q["door"], q["rect"]) or not any(on_edge(q["door"], c["rect"]) for c in cs if c["kind"] in ("corridor", "core")):
                out.append("%s: door not between the flat and the corridor" % q["id"])
        cor = next(c for c in cs if c["kind"] == "corridor")
        core = next(c for c in cs if c["kind"] == "core")
        if not over([cor["rect"][0] - .05, cor["rect"][1] - .05, cor["rect"][2] + .05, cor["rect"][3] + .05], core["rect"]):
            out.append("%s floor %d: corridor does not reach the stair" % (l["id"], f))
    return out


def main():
    city = jload(os.path.join(CITY, "city.json"))
    residents = jload(os.path.join(CITY, "residents.json"))["residents"]
    mates = {}
    for r in residents:
        mates.setdefault(r["home"], []).append(r["id"])
    mix = Mix()
    tally, bad, nb = {}, [], 0
    for e in city["blocks"]:
        if e["template"] not in ("peri", "towerR"):
            continue
        p = os.path.join(CITY, e["file"])
        b = jload(p)
        for l in b["lots"]:
            if l["kind"] != "home" or not l.get("stair"):
                continue
            F, flats, common = lay_out(l, b["template"] == "towerR", mix)
            furnish(l, F, flats, mates.get(l["id"], []))
            l["flats"], l["common"] = flats, common
            bad += problems(l, flats, common)
            nb += 1
            for q in flats:
                key = ("tower" if b["template"] == "towerR" else "court", q["size"])
                tally[key] = tally.get(key, 0) + 1
        jsave(p, b)
    n = sum(tally.values())
    print("%d buildings, %d flats" % (nb, n))
    for s in SIZES:
        c = sum(v for (k, z), v in tally.items() if z == s)
        print("  %3d m2: %4d  (%4.1f %%; court %d, tower %d)" % (s, c, 100 * c / n, tally.get(("court", s), 0), tally.get(("tower", s), 0)))
    print("problems:", len(bad))
    for x in bad[:20]:
        print("  ", x)


if __name__ == "__main__":
    main()
