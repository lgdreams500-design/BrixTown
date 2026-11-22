"""Dress the lanes of the communities (the owner, 2026-11-23: "improve streets in these areas by adding bushes, street light
benches etc"): on the verge each side of every lane, a metre off the roadway, lamps every 26 m (alternating sides, where there is
no lamp near already), a bench every 75 m or so with a bin and a planter beside it, and flowering bushes every 6 m between them.
Nothing goes in a garden (add_houses_03.py keeps gardens 2 m off the roadway), in front of a gate, at a junction or in a turning
circle. The props are the slice's own kinds (lamp, bench, bin, planter, plant "bush"), set "street"; running it again replaces them.
Run from Tools/worldgen, after add_houses_03.py:   python add_lanes_dressing_01.py
"""
import json, math, os, hashlib

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
VERGE = 1.0                         # metres from the roadway's edge to the middle of the verge
LAMP_EVERY, BENCH_EVERY, BUSH_EVERY = 26.0, 75.0, 6.0
BUSHES = ["box", "lilac", "hydrangea", "rose", "lavender"]


def h(*k):
    return int(hashlib.md5("|".join(map(str, k)).encode()).hexdigest()[:8], 16) / 0xFFFFFFFF


def seg_dist(p, a, b):
    ax, az = a; bx, bz = b; px, pz = p
    dx, dz = bx - ax, bz - az
    L2 = dx * dx + dz * dz or 1
    t = max(0, min(1, ((px - ax) * dx + (pz - az) * dz) / L2))
    return math.hypot(px - ax - t * dx, pz - az - t * dz)


def lane_dist(p, ln):
    return min(seg_dist(p, a, b) for a, b in zip(ln["points"], ln["points"][1:]))


def in_garden(p, l):
    x0, z0, x1, z1 = l["rect"]
    cx, cz, t = (x0 + x1) / 2, (z0 + z1) / 2, math.radians(l.get("angle") or 0)
    c, s = math.cos(t), math.sin(t)
    u, v = -((p[0] - cx) * c + (p[1] - cz) * s), -(p[0] - cx) * s + (p[1] - cz) * c
    g = l["house"].get("garden", [-6.6, -6.2, 6.6, 5.2])
    return g[0] - .6 < u < g[2] + .6 and g[1] - .6 < v < g[3] + .6, (cx - .1 * -c - g[1] * s, cz - .1 * -s + g[1] * c)


def main():
    city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
    counts = {"lamp": 0, "bench": 0, "bin": 0, "planter": 0, "plant": 0}
    for e in city["blocks"]:
        if e["template"] != "community":
            continue
        p = os.path.join(CITY, e["file"])
        b = json.load(open(p, encoding="utf-8"))
        b["props"] = [q for q in b.get("props", []) if q.get("set") != "street"]
        houses = [l for l in b["lots"] if l.get("house")]
        gates = [in_garden((0, 0), l)[1] for l in houses]
        lamps = [q["pos"] for q in b["props"] if q["kind"] == "lamp"]
        taken, n = [], 0

        def free(pt, ln, r):
            if any(math.hypot(pt[0] - q[0], pt[1] - q[1]) < r + q[2] for q in taken):
                return False
            if any(math.hypot(pt[0] - g[0], pt[1] - g[1]) < 2.6 for g in gates):
                return False
            if any(o is not ln and lane_dist(pt, o) < o.get("width", 6) / 2 + 1.6 for o in b["lanes"]):
                return False                                      # a junction: another lane's roadway or verge
            return not any(in_garden(pt, l)[0] for l in houses if abs((l["rect"][0] + l["rect"][2]) / 2 - pt[0]) < 20 and abs((l["rect"][1] + l["rect"][3]) / 2 - pt[1]) < 20)

        for li, ln in enumerate(b["lanes"]):
            P, w = ln["points"], ln.get("width", 6.0)
            segs, total = [], 0.0
            for a, c in zip(P, P[1:]):
                L = math.hypot(c[0] - a[0], c[1] - a[1])
                segs.append((a, c, L, total))
                total += L
            end = total - (9.0 if ln.get("end") == "turning" else 2.0)

            def at(d, side):
                for a, c, L, s0 in segs:
                    if d <= s0 + L or (a, c, L, s0) == segs[-1]:
                        t = (d - s0) / (L or 1)
                        x, z = a[0] + (c[0] - a[0]) * t, a[1] + (c[1] - a[1]) * t
                        nx, nz = -(c[1] - a[1]) / (L or 1), (c[0] - a[0]) / (L or 1)
                        o = side * (w / 2 + VERGE)
                        return [round(x + nx * o, 2), round(z + nz * o, 2)], ("x" if abs(c[0] - a[0]) >= abs(c[1] - a[1]) else "z")

            def put(q, r):
                nonlocal n
                q["set"] = "street"; q["id"] = "%s_st%d" % (b["id"], n); n += 1
                b["props"].append(q); taken.append((q["pos"][0], q["pos"][1], r)); counts[q["kind"]] += 1

            d, k = 4.0, 0                                           # lamps, alternating sides
            while d < end:
                side = 1 if k % 2 == 0 else -1
                pt, _ = at(d, side)
                if not any(math.hypot(pt[0] - q[0], pt[1] - q[1]) < 12 for q in lamps) and free(pt, ln, .6):
                    put(dict(kind="lamp", pos=pt), .6); lamps.append(pt)
                d += LAMP_EVERY; k += 1
            d = BENCH_EVERY * (.4 + .3 * h(b["id"], li))                # benches with a bin and a planter
            while d < end - 3:
                side = 1 if h(b["id"], li, int(d)) < .5 else -1
                pt, along = at(d, side)
                pb, _ = at(d + 1.6, side)
                pp, _ = at(d - 1.6, side)
                if free(pt, ln, 1.1) and free(pb, ln, .4) and free(pp, ln, .5):
                    put(dict(kind="bench", pos=pt, along=along), 1.1)
                    put(dict(kind="bin", pos=pb, along=along, out=[0, -1]), .4)
                    put(dict(kind="planter", pos=pp, along=along, out=[0, -1]), .5)
                d += BENCH_EVERY
            for side in (1, -1):                                    # bushes between
                d = 3.0 + 2.0 * h(b["id"], li, side)
                while d < end:
                    pt, _ = at(d, side)
                    r = round(.65 + .35 * h(b["id"], li, side, int(d)), 2)
                    if free(pt, ln, r + .4):
                        put(dict(kind="plant", pos=pt, shape="bush", species=BUSHES[int(h(b["id"], li, int(d), side, "sp") * len(BUSHES))], r=r,
                                 seed=int(h(b["id"], li, int(d), side) * 9999)), r + .4)
                    d += BUSH_EVERY * (.8 + .4 * h(b["id"], li, int(d), "gap"))
        with open(p, encoding="utf-8") as f:
            end_nl = "\n" if f.read().endswith("\n") else ""
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            json.dump(b, f, indent=1, ensure_ascii=False)
            f.write(end_nl)
    print("street dressing: " + ", ".join("%s %d" % kv for kv in counts.items()))


if __name__ == "__main__":
    main()
