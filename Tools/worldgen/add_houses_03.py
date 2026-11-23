"""Every independent house as one of the four sample houses (Tools/blender/house_samples_19.py; the owner, 2026-11-23: "update
all independent houses with some variation of one of these 4 sample houses"). Writes plan "samples_19" into every house of the
communities: the line (Plain, Old Town, Workshop, Curve) by a hash of the lot, the four on Northwood Way as the owner reviewed
them; the colours vary by the lot's trim (shutters, plaster, roof), the extras inside and in the garden by houses.js's own hash.

Each house also gets its garden: a rectangle in its own frame, as large as the sample garden (7.5 m to each side, 9.5 m to the
front, 8 m to the back) but kept short of the lanes (2 m from the roadway, leaving the verge for the street dressing) and of
half way to every neighbouring house, so gardens never overlap.

The frame, as houses.js uses it for "samples_19": u is Blender's x (the living room on the left seen from the lane), v from the
front (-) to the back (+), metres from the lot's centre; world = (cx - u cos t - v sin t, cz - u sin t + v cos t), t = lot.angle.
Run from Tools/worldgen:   python add_houses_03.py      (running it again writes the same)
"""
import json, math, os, hashlib

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
LINES = ["plain", "oldtown", "workshop", "curve"]
REVIEWED = {"b_0_0_l7": "plain", "b_0_0_l9": "oldtown", "b_0_0_l12": "workshop", "b_0_0_l13": "curve"}
HU, HV = 6.0, 4.5                                     # the house: u -6..6, v -4.5..4.5
GARDEN = (-7.5, -9.5, 7.5, 8.0)                       # the sample garden: u0, v0 (front), u1, v1 (back)
GMIN = (-6.6, -6.2, 6.6, 5.2)                         # the least garden: a path round the house, the door step and gate
LANE_CLEAR = 2.0                                      # metres from the roadway's edge to a garden fence


def h(*k):
    return int(hashlib.md5("|".join(map(str, k)).encode()).hexdigest()[:8], 16) / 0xFFFFFFFF


def mapA(x, o, n):                                    # as houses.js: a point keeps its distance to the nearer wall of its room
    if x <= o[0]:
        return n[0] + x - o[0]
    for i in range(len(o) - 1):
        if x <= o[i + 1]:
            return n[i] + x - o[i] if x - o[i] <= o[i + 1] - x else n[i + 1] - (o[i + 1] - x)
    return n[-1] + x - o[-1]


SI, SJ, SA, SB, SM = 5.68, 4.18, -1.2, 1.5, .6
mu = lambda u: round(mapA(u, [-4.68, -1, 1.2, 4.68], [-SI, SA, SB, SI]), 3)
mv = lambda v: round(mapA(v, [-3.68, .5, 3.68], [-SJ, SM, SJ]), 3)
BEDS = [(mu(a), mv(b)) for a, b in [(-3.3, -.6), (-2.5, -.6), (4.2, -.6), (4.2, -.6), (4.15, 2.65)]]
SEATS = [(mu(a), mv(b)) for a, b in [(-1.75, -2.3), (-1.75, -1.3), (-3.3, -.25), (-2.8, 1.1), (-2.8, 2.5)]]
ROOMS = [("Living room", 0, [-6, -4.5, SA, SM]), ("Kitchen", 0, [-6, SM, SA, 4.5]), ("Hall", 0, [SA, -4.5, SB, 4.5]),
         ("Dining room", 0, [SB, -4.5, 6, SM]), ("Bathroom", 0, [SB, SM, 6, 4.5]), ("Main bedroom", 1, [-6, -4.5, SA, SM]),
         ("Study", 1, [-6, SM, SA, 4.5]), ("Landing", 1, [SA, -4.5, SB, 4.5]), ("Children's room", 1, [SB, -4.5, 6, SM]),
         ("Guest room", 1, [SB, SM, 6, 4.5])]


def frame(l):
    x0, z0, x1, z1 = l["rect"]
    cx, cz, t = (x0 + x1) / 2, (z0 + z1) / 2, math.radians(l.get("angle") or 0)
    c, s = math.cos(t), math.sin(t)
    to_w = lambda u, v: (cx - u * c - v * s, cz - u * s + v * c)
    to_l = lambda x, z: (-((x - cx) * c + (z - cz) * s), -(x - cx) * s + (z - cz) * c)
    return to_w, to_l


def house_outline(l, step=.5):
    """Points round a house's footprint, in world metres."""
    to_w, _ = frame(l)
    pts = []
    for (a0, b0), (a1, b1) in [((-HU, -HV), (HU, -HV)), ((HU, -HV), (HU, HV)), ((HU, HV), (-HU, HV)), ((-HU, HV), (-HU, -HV))]:
        n = max(1, int(math.hypot(a1 - a0, b1 - b0) / step))
        pts += [to_w(a0 + (a1 - a0) * k / n, b0 + (b1 - b0) * k / n) for k in range(n)]
    return pts


def lane_points(lanes, step=.5):
    """Points along both edges of every roadway pushed out by LANE_CLEAR, and its middle, in world metres."""
    out = []
    for ln in lanes:
        w = ln.get("width", 6.0) / 2 + LANE_CLEAR
        P = ln["points"]
        for (x0, z0), (x1, z1) in zip(P, P[1:]):
            L = math.hypot(x1 - x0, z1 - z0) or 1
            nx, nz = -(z1 - z0) / L, (x1 - x0) / L
            for k in range(max(1, int(L / step)) + 1):
                x, z = x0 + (x1 - x0) * k / max(1, int(L / step)), z0 + (z1 - z0) * k / max(1, int(L / step))
                for o in (-w, 0, w):
                    out.append((x + nx * o, z + nz * o))
    return out


def garden(l, others, lanes_pts):
    """The garden rectangle (u0, v0, u1, v1): the sample garden, shrunk side by side until no lane point and no point half way to
    a neighbouring house lies inside it."""
    _, to_l = frame(l)
    g = list(GARDEN)
    obstacles = [to_l(x, z) for x, z in lanes_pts]
    for o in others:
        for x, z in house_outline(o):
            u, v = to_l(x, z)
            qu, qv = max(-HU, min(HU, u)), max(-HV, min(HV, v))          # the nearest point of this house; stop half way
            obstacles.append(((u + qu) / 2, (v + qv) / 2))
    for u, v in obstacles:
        if not (g[0] < u < g[2] and g[1] < v < g[3]):
            continue
        cuts = [(u - g[0], 0, u) if u <= GMIN[0] else None, (v - g[1], 1, v) if v <= GMIN[1] else None,
                (g[2] - u, 2, u) if u >= GMIN[2] else None, (g[3] - v, 3, v) if v >= GMIN[3] else None]
        cuts = [c for c in cuts if c]
        if cuts:
            _, side, at = min(cuts)                                     # give up the least garden
            g[side] = at
    return [round(x, 2) for x in g]


def main():
    city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
    n_houses, lines, small = 0, {k: 0 for k in LINES}, 0
    for e in city["blocks"]:
        if e["template"] != "community":
            continue
        p = os.path.join(CITY, e["file"])
        b = json.load(open(p, encoding="utf-8"))
        houses = [l for l in b["lots"] if l.get("house")]
        lp = lane_points(b.get("lanes", []))
        for l in houses:
            to_w, to_l = frame(l)
            cx, cz = to_w(0, 0)
            near = [o for o in houses if o is not l and math.hypot((o["rect"][0] + o["rect"][2]) / 2 - cx, (o["rect"][1] + o["rect"][3]) / 2 - cz) < 40]
            lanes_near = [q for q in lp if abs(q[0] - cx) < 25 and abs(q[1] - cz) < 25]
            line = REVIEWED.get(l["id"]) or LINES[int(h(l["id"], "line") * 4)]
            n = min(5, len(l["objects"].get("bed", [])) or l.get("residents", 2))
            trim = l["house"].get("trim", {})
            g = garden(l, near, lanes_near)
            small += (g[2] - g[0]) * (g[3] - g[1]) < 150
            l["house"] = dict(plan="samples_19", mirror=False, furniture=line, trim=trim, garden=g, source="Tools/blender/house_samples_19.py",
                              rooms=[dict(name=nm, floor=f, rect=r) for nm, f, r in ROOMS])
            l["objects"] = dict(bed=[[round(c, 2) for c in to_w(u, v)] for u, v in BEDS[:n]], room=[[round(c, 2) for c in to_w(u, v)] for u, v in SEATS[:n]])
            l["levels"] = dict(bed=[1] * n, room=[0] * n)
            l["storeys"] = ["main", "bedrooms"]
            l["floors"] = 2
            n_houses += 1
            lines[line] += 1
        with open(p, encoding="utf-8") as f:
            end = "\n" if f.read().endswith("\n") else ""
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            json.dump(b, f, indent=1, ensure_ascii=False)
            f.write(end)
    print("%d houses on plan samples_19: %s; %d with a garden under 150 m2" % (n_houses, ", ".join("%s %d" % kv for kv in lines.items()), small))


if __name__ == "__main__":
    main()
