"""The row houses and the court flats get their approved plans (ADR-0036; FURN-060, FURN-061, FURN-063).

For each row house (kind "home", block template "rows": 14 x 12 m on 2 or 3 storeys, or a 12 x 16 m lot with its door on
the long side, on 2) and each court flat of 80 or 160 m2 (lot.flats; the towers' flats are another proportion and are not
planned yet):
  - interior: the plan it is furnished from (World/rules/home_plans.json "row_14x12_2", "row_14x12_3", "row_16x12_2",
    "flat_80", "flat_160"), on the lot for a row house and on the flat for a flat;
  - interior_frame: how the plan lies in the home, so the slice (Prototype/play/homes.js) and this script place every piece
    and spot the same way: "o" the centre, "x" the world direction of the plan's x (across the front, left to right seen
    from the door side; mirrored where the home's stair or door is on the plan's right), "y" the world direction of the
    plan's y (front to back), and "u", "v" the plan's wall lines along x and y with where they stand in the slice (as
    houses.js mu/mv: a point keeps its distance to the nearer wall of its room; the slice's outer walls are 0.35 m, a
    flat's walls on the corridor and its neighbours 0.12 m centred on its rect);
  - furniture: the line it is furnished in, by a hash of the home (as add_houses_03.py does for the houses);
  - objects.bed / objects.room (and lot.levels): where its residents lie and sit, on the beds, sofas and chairs of the plan
    (the spots of plan "house_12x9"), as many as before, in plan order; flats keep their indices (flats[].beds, .rooms).
A row house's stair becomes the plan's: one straight flight a storey up the left wall of the hall, each over the one below
(lot.stair kind "straight", width 1.0; Prototype/play/storeys.js), and its street door moves into the hall beside the stair,
where the plan has it (centred in the hall); its outside is otherwise as before. Which pieces stand there follows the
household's furnishing level (FURN-063, Prototype/play/furnishing.js); a bed for each resident is always there.
Run from Tools/worldgen:   python add_homes_01.py      (running it again writes the same)
"""
import json, math, os, hashlib

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")
PLANS = json.load(open(os.path.join(RULES, "home_plans.json"), encoding="utf-8"))["plans"]
STOREYS = json.load(open(os.path.join(RULES, "storeys.json"), encoding="utf-8"))
SPOTS = PLANS["house_12x9"]["house"]["spots"]           # where one lies or sits on each piece (the same pieces in every plan)
BED_KINDS = ("k17:bed", "c_bunk", "r_single")
SEAT_KINDS = ("k17:sofa", "k17:dining", "k17:armchair")
LINES = ["plain", "oldtown", "workshop", "curve"]
WALL = STOREYS["wall"]                                    # the slice's outer walls (storeys.js, play.js building())
PART = 0.12                                               # the flats' walls (storeys.js partitions)
OUT = {"n": (0, -1), "s": (0, 1), "w": (-1, 0), "e": (1, 0)}


def h(*k):
    return int(hashlib.md5("|".join(map(str, k)).encode()).hexdigest()[:8], 16) / 0xFFFFFFFF


def mapA(x, o, n):                                        # as houses.js: a point keeps its distance to the nearer wall of its room
    if x <= o[0]:
        return n[0] + x - o[0]
    for i in range(len(o) - 1):
        if x <= o[i + 1]:
            return n[i] + x - o[i] if x - o[i] <= o[i + 1] - x else n[i + 1] - (o[i + 1] - x)
    return n[-1] + x - o[-1]


def side_of(rect, p):
    x0, z0, x1, z1 = rect
    for s, ok in (("n", abs(p[1] - z0) < .02), ("s", abs(p[1] - z1) < .02), ("w", abs(p[0] - x0) < .02), ("e", abs(p[0] - x1) < .02)):
        if ok:
            return s
    return None


def letter(d):
    return {(-1, 0): "w", (1, 0): "e", (0, -1): "n", (0, 1): "s"}[(round(d[0]), round(d[1]))]


class Frame:
    """A plan laid into a home: plan (x, y) -> world (x, z)."""

    def __init__(s, plan, rect, front, mirror, faces):
        P = plan["house"]
        s.P, s.plan = P, plan
        x0, z0, x1, z1 = rect
        s.o = ((x0 + x1) / 2, (z0 + z1) / 2)
        din = (-OUT[front][0], -OUT[front][1])
        xh = (-din[1], din[0])
        s.x = (xh[0] * mirror, xh[1] * mirror)
        s.y = din
        PI0, PJ0 = P["W1"] - P["T"], P["D1"] - P["T"]
        xs = sorted({round(p[1], 3) for f in P["partitions"].values() for p in f if p[0] == "x"})
        ys = sorted({round(p[1], 3) for f in P["partitions"].values() for p in f if p[0] == "y"})
        L, R, F, B = faces                                # the slice's inner faces: left, right, front, back (half widths)
        s.u = [[-PI0] + xs + [PI0], [-L] + xs + [R]]
        s.v = [[-PJ0] + ys + [PJ0], [-F] + ys + [B]]

    def mu(s, x):
        return mapA(x, *s.u)

    def mv(s, y):
        return mapA(y, *s.v)

    def w(s, x, y):                                       # slice-frame (x, y) to world
        return (s.o[0] + x * s.x[0] + y * s.y[0], s.o[1] + x * s.x[1] + y * s.y[1])

    def spots(s, kinds, storeys=None):
        """[(world x, world z, floor name)] of every spot on the plan's pieces of these kinds, kind by kind in plan order."""
        out = []
        for k in kinds:
            for it in s.plan["items"]:
                if it["piece"] != k or (storeys and it["storey"] not in storeys):
                    continue
                c, sn = math.cos(it["yaw"]), math.sin(it["yaw"])
                for x, y, z, fx, fy in SPOTS[k]:
                    out.append(s.w(s.mu(it["x"] + x * c - y * sn), s.mv(it["y"] + x * sn + y * c)) + (it["storey"],))
        return out

    def data(s):
        r = lambda v: [round(a, 4) for a in v]
        return dict(o=r(s.o), x=r(s.x), y=r(s.y), u=[r(s.u[0]), r(s.u[1])], v=[r(s.v[0]), r(s.v[1])])


def row_plan(l):
    x0, z0, x1, z1 = l["rect"]
    front = side_of(l["rect"], l["door"])
    across, deep = (x1 - x0, z1 - z0) if front in "ns" else (z1 - z0, x1 - x0)
    key = {(14, 12, 2): "row_14x12_2", (14, 12, 3): "row_14x12_3", (16, 12, 2): "row_16x12_2"}.get((round(across), round(deep), l["floors"]))
    return key, front


def do_row(l, key, front, tally):
    plan, P = PLANS[key], PLANS[key]["house"]
    xh = (OUT[front][1], -OUT[front][0])                  # the plan's +x seen from the door side (unmirrored)
    mirror = 1 if letter((-xh[0], -xh[1])) == l["stair"]["wall"] else -1
    assert letter((-xh[0] * mirror, -xh[1] * mirror)) == l["stair"]["wall"], l["id"]
    W, D = P["W1"] - P["W0"], P["D1"] - P["D0"]
    F = Frame(plan, l["rect"], front, mirror, (W / 2 - WALL, W / 2 - WALL, D / 2 - WALL, D / 2 - WALL))
    # the stair: the plan's flight up the left wall, in a well the size storeys.json asks of every stair (two lanes and a
    # spine across, its run and a landing at each end along)
    st, S = P["stair"], STOREYS["stair"]
    a0, a1 = F.mu(st[0]), F.mu(st[1])
    b0, b1 = F.mv(st[2]) - S["landing"], F.mv(st[3]) + S["landing"]
    across_w = 2 * S["width"] + S["spine"]
    pts = [F.w(a, b) for a in (a0, a0 + across_w) for b in (b0, b1)]
    xs, zs = [p[0] for p in pts], [p[1] for p in pts]
    axis = "z" if F.y[0] == 0 else "x"
    l["stair"] = dict(rect=[round(min(xs), 2), round(min(zs), 2), round(max(xs), 2), round(max(zs), 2)], axis=axis, wall=l["stair"]["wall"],
                      up0=int(F.y[1] if axis == "z" else F.y[0]), kind="straight", width=round(a1 - a0, 3))
    # the street door, in the hall beside the stair
    hall = P["rooms"][P["floors"][0]]["Hall"]
    dx, dz = F.w((F.mu(hall[0]) + F.mu(hall[1])) / 2, -D / 2)
    x0, z0, x1, z1 = l["rect"]
    l["door"] = [round(dx, 2), {"n": z0, "s": z1}[front]] if front in "ns" else [{"w": x0, "e": x1}[front], round(dz, 2)]
    # where the residents lie and sit
    beds, seats = F.spots(BED_KINDS), F.spots(SEAT_KINDS, [P["floors"][0]])
    o = l["objects"]
    kb, kr = len(o["bed"]), len(o["room"])
    if kb > len(beds) or kr > len(seats):
        tally["short"].append(l["id"])
    kb, kr = min(kb, len(beds)), min(kr, len(seats))
    o["bed"] = [[round(x, 2), round(z, 2)] for x, z, f in beds[:kb]]
    o["room"] = [[round(x, 2), round(z, 2)] for x, z, f in seats[:kr]]
    l["levels"] = dict(bed=[P["floors"].index(f) for x, z, f in beds[:kb]], room=[0] * kr)
    l["interior"] = key
    l["interior_frame"] = F.data()
    l.setdefault("furniture", LINES[int(h(l["id"], "line") * 4) % 4])
    tally[key] = tally.get(key, 0) + 1


def do_flat(l, q, key, tally):
    plan, P = PLANS[key], PLANS[key]["house"]
    front = side_of(q["rect"], q["door"])
    x0, z0, x1, z1 = q["rect"]
    c = ((x0 + x1) / 2, (z0 + z1) / 2)
    din = (-OUT[front][0], -OUT[front][1])
    xh = (-din[1], din[0])
    a = (q["door"][0] - c[0]) * xh[0] + (q["door"][1] - c[1]) * xh[1]
    mirror = 1 if a < 0 else -1                           # the plan's door is near its left end
    W, D = P["W1"] - P["W0"], P["D1"] - P["D0"]
    if abs(abs(a) - (W / 2 - 1.0)) > .05:
        tally["door"].append(q["id"])
    lx0, lz0, lx1, lz1 = l["rect"]

    def face(d, half):                                    # a side on the building's outer wall, or on a flat's wall (12 cm, centred)
        e = {"w": (x0, lx0 + WALL), "e": (x1, lx1 - WALL), "n": (z0, lz0 + WALL), "s": (z1, lz1 - WALL)}[letter(d)]
        return half if abs(e[0] - e[1]) < .05 else half - PART / 2
    xm = (xh[0] * mirror, xh[1] * mirror)
    faces = (face((-xm[0], -xm[1]), W / 2), face(xm, W / 2), face(OUT[front], D / 2), face(din, D / 2))
    F = Frame(plan, q["rect"], front, mirror, faces)
    beds, seats = F.spots(BED_KINDS), F.spots(SEAT_KINDS)
    o = l["objects"]
    if len(q["beds"]) > len(beds) or len(q["rooms"]) > len(seats):
        tally["short"].append(q["id"])
    for i, (x, z, f) in zip(q["beds"], beds):
        o["bed"][i] = [round(x, 2), round(z, 2)]
    for i, (x, z, f) in zip(q["rooms"], seats):
        o["room"][i] = [round(x, 2), round(z, 2)]
    q["interior"] = key
    q["interior_frame"] = F.data()
    q.setdefault("furniture", LINES[int(h(q["id"], "line") * 4) % 4])
    tally[key] = tally.get(key, 0) + 1


def flat_plan(q):
    w, d = sorted((q["rect"][2] - q["rect"][0], q["rect"][3] - q["rect"][1]))
    if q["size"] == 80 and abs(w - 6.85) < .02 and abs(d - 11.68) < .02:
        return "flat_80"
    if q["size"] == 160 and abs(w - 6.85) < .02 and abs(d - 23.36) < .02:
        return "flat_160"
    return None


def main():
    city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
    tally = {"short": [], "door": [], "skipped": []}
    for e in city["blocks"]:
        p = os.path.join(CITY, e["file"])
        b = json.load(open(p, encoding="utf-8"))
        touched = False
        for l in b["lots"]:
            if l["kind"] != "home" or l.get("house") or l.get("tower") or l.get("angle"):
                continue
            if l.get("flats"):
                for q in l["flats"]:
                    key = flat_plan(q)
                    if key:
                        do_flat(l, q, key, tally)
                        touched = True
            elif l.get("stair") and b["template"] == "rows":
                key, front = row_plan(l)
                if key:
                    do_row(l, key, front, tally)
                    touched = True
                else:
                    tally["skipped"].append(l["id"])
        if not touched:
            continue
        with open(p, encoding="utf-8") as f:
            end = "\n" if f.read().endswith("\n") else ""
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            json.dump(b, f, indent=1, ensure_ascii=False)
            f.write(end)
    print(", ".join("%s %d" % (k, v) for k, v in sorted(tally.items()) if isinstance(v, int)))
    for k in ("short", "door", "skipped"):
        if tally[k]:
            print("%s: %d (%s)" % (k, len(tally[k]), ", ".join(tally[k][:8])))


if __name__ == "__main__":
    main()
