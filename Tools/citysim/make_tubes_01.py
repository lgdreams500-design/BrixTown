"""Writes World/city_01/tubes.json: the glass travel tubes of Old Town (CITY-108, CITY-112; F-VEH-TUBE level 2).
Approved by the owner on the map, 2026-11-28 ("Approve the routes"; python tube_map.py draws it).

    python make_tubes_01.py

The tubes run over the streets, level at RUN_Z, meeting at junctions over the street crossings. Inside the wall they follow
the street grid's centre lines from a hub over the crossing of Baker Street and Market Street, by Market Square. Out to the
suburbs they follow the roads (World/city_01/roads.json links, the suburbs' own lanes), and through each suburb its main way,
with three stops along it: West, Central and East (the owner, 2026-11-28: "northwood eastbrook and south field should have
3 stops each"). Each stop's booth stands beside the street; its own short tube rises out of it and joins the line over the
street at a junction, so a ride passes other stops without stopping. Eastgate Mall's stop stands by its main (south)
entrance (the owner, 2026-11-28: "near the gate of the mall so player doesn't have to walk alot").
"""
import json, math, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "worldgen"))
import world                                               # noqa: E402
from tubes import problems                                 # noqa: E402

RUN_Z = 9.0
CROWN_CLEAR = 1.0                 # a booth this far outside a grown tree's crown (vegetation.json r_max)
SIDE = 8.0        # m from a lane's middle to a booth beside it
STEP = 12.0       # m between the points a tube keeps along a curving road

# stops inside and by the wall: (name, where, which way its door looks). Each joins the nearest line over the street.
# Along the two loops round the centre (the owner, 2026-11-28, drawn on the map: "2 concentric loops ... stations ... at key
# buildings") a booth stands on the pavement on its building's side, 6.5 m from the street's middle, its door to the building.
TOWN_STOPS = {
    "market_square": ("Market Square", (912.0, 772.0), (0, 1)),
    "westgate_mall": ("Westgate Mall", (377.9, 903.5), (1, 0)),
    "furniture_outlet": ("Furniture Outlet", (1395.0, 772.0), (0, 1)),
    "northgate_mall": ("Northgate Mall", (960.0, 296.0), (0, 1)),
    # by Eastgate's main entrance (its plan's south entrance, 16 m wide at 1680, 848), on its forecourt, door to the mall
    "eastgate_mall": ("Eastgate Mall", (1692.0, 858.0), (0, -1)),
    # the inner loop: Linden Street, Weaver Street, Quarry Street, Cooper Street
    "linden_bank": ("Linden Bank", (891.0, 582.5), (0, 1)),
    "fountain_hospital": ("Fountain Hospital", (1158.5, 688.0), (1, 0)),
    "old_town_museum": ("Old Town Museum", (891.0, 953.5), (0, -1)),
    "palace_cinema": ("Palace Cinema", (774.5, 880.0), (1, 0)),
    "fountain_library": ("Fountain Library", (774.5, 688.0), (1, 0)),
    # the outer loop: Plum Street, Garden Street, Depot Street, Tanner Street
    "plum_works": ("Plum Works", (795.0, 486.5), (0, 1)),
    "garden_clinic": ("Garden Clinic", (1241.5, 995.0), (-1, 0)),
    "town_depot": ("Town Depot", (1282.0, 1062.5), (0, 1)),
    "stone_oven_bakery": ("The Stone Oven Bakery", (678.5, 752.0), (1, 0)),
    "tanner_clinic": ("Tanner Clinic", (678.5, 637.0), (1, 0)),
}
JUNCTIONS = {                         # named places on the lines (the rest are named by where they are)
    "hub": (960.0, 768.0),            # over the crossing of Baker Street and Market Street, by Market Square
    "j_west_gate": (528.0, 768.0),    # over Baker Street at the West Gate
    "j_westgate": (373.9, 880.0),
    "j_outlet": (1395.0, 768.0),
    "j_baker_east": (1544.0, 768.0),  # over Baker Street where Eastgate Road leaves it
    "j_eastgate": (1692.0, 868.0),
    "j_northgate": (960.0, 304.0),
    "j_south_gate": (960.0, 1232.0),
}
# the lines over the streets, straight from point to point; where two cross they meet at a junction
STREET_LINES = [
    [(373.9, 880.0), (373.9, 768.0), (528.0, 768.0), (960.0, 768.0), (1395.0, 768.0), (1544.0, 768.0), (1544.0, 868.0), (1692.0, 868.0)],   # Baker Street, out to both malls
    [(960.0, 304.0), (960.0, 768.0), (960.0, 1232.0)],                                                   # Market Street, gate to gate
    [(768.0, 576.0), (960.0, 576.0), (1152.0, 576.0), (1152.0, 768.0), (1152.0, 960.0), (960.0, 960.0), (768.0, 960.0), (768.0, 768.0), (768.0, 576.0)],      # the inner loop
    [(672.0, 480.0), (960.0, 480.0), (1248.0, 480.0), (1248.0, 768.0), (1248.0, 1056.0), (960.0, 1056.0), (672.0, 1056.0), (672.0, 768.0), (672.0, 480.0)],  # the outer loop
]
# the suburbs: (district, its way, the road out to it, the junction that road leaves, where along the way the three stops are)
SUBURBS = [
    ("Northwood", "Northwood Way", "Northwood Road", "j_northgate", (0.18, 0.47, 0.80)),
    ("Eastbrook", "Eastbrook Way", "Eastbrook Road", "j_northgate", (0.20, 0.52, 0.82)),
    ("Southfield", "Southfield Way", "Southfield Road", "j_south_gate", (0.22, 0.50, 0.80)),
]


def arc(points):
    cum = [0.0]
    for a, b in zip(points, points[1:]):
        cum.append(cum[-1] + math.dist(a, b))
    return cum


def at(points, cum, s):
    i = max(0, min(len(cum) - 2, next((k for k in range(len(cum) - 1) if cum[k + 1] >= s), len(cum) - 2)))
    f = (s - cum[i]) / max(1e-9, cum[i + 1] - cum[i])
    a, b = points[i], points[i + 1]
    return (a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f), (b[0] - a[0], b[1] - a[1])


def thin(points, s0, s1, cum):
    """The road between arc lengths s0 and s1, a point every STEP metres (either way along it)."""
    n = max(1, int(abs(s1 - s0) / STEP))
    return [at(points, cum, s0 + (s1 - s0) * k / n)[0] for k in range(n + 1)]


def inside_lot(p, lots, pad=1.5):
    return any(l["rect"][0] - pad <= p[0] <= l["rect"][2] + pad and l["rect"][1] - pad <= p[1] <= l["rect"][3] + pad for l in lots)


def build():
    w = world.load()
    rules = w["tubes"]
    lots = world.lots(w)
    lanes = {}
    for b in w["blocks"]:                                          # a name can have a side lane of its own: the longest is the way
        for ln in b.get("lanes", []):
            if ln["name"] not in lanes or arc(ln["points"])[-1] > arc(lanes[ln["name"]])[-1]:
                lanes[ln["name"]] = ln["points"]
    links = {ln["name"]: ln["points"] for ln in w["roads"]["links"]}
    stops, junctions = {}, dict(JUNCTIONS)
    segs = [(p, q) for line in STREET_LINES for p, q in zip(line, line[1:])]
    named = {v: k for k, v in JUNCTIONS.items()}
    def jid(p):
        p = (round(p[0], 2), round(p[1], 2))
        if p not in named:
            named[p] = "j_%d_%d" % (round(p[0]), round(p[1]))
            junctions[named[p]] = p
        return named[p]
    def project(p, a, b):
        dx, dz = b[0] - a[0], b[1] - a[1]
        f = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dz) / (dx * dx + dz * dz)))
        return (a[0] + dx * f, a[1] + dz * f), f
    points = {p for line in STREET_LINES for p in line}
    for sid, (name, pos, face) in TOWN_STOPS.items():           # each stop joins the line nearest it
        best = min((project(pos, a, b)[0] for a, b in segs), key=lambda q: math.dist(q, pos))
        best = min([q for q in points if math.dist(q, best) < 0.5] or [best], key=lambda q: math.dist(q, pos))   # a corner if it is one
        points.add((round(best[0], 2), round(best[1], 2)))
        stops[sid] = dict(name=name, pos=list(pos), face=list(face), junction=jid(best))
    trunks = []
    for a, b in segs:                                              # each straight stretch cut at every junction on it
        on = sorted({q for q in points if math.dist(project(q, a, b)[0], q) < 0.01}, key=lambda q: project(q, a, b)[1])
        for p, q in zip(on, on[1:]):
            if math.dist(p, q) > 0.01:
                trunks.append((jid(p), jid(q), [p, q]))
    for district, way, road, from_j, fracs in SUBURBS:
        W = lanes[way]
        cw = arc(W)
        out = links[road] + lanes.get(road, [])[1:]               # the road out of the wall, then on through the suburb
        co = arc(out)
        meet = min(range(len(W)), key=lambda i: math.dist(W[i], out[-1]))   # where the road out reaches the way
        s_meet = cw[meet]
        js = []
        for word, f in zip(("West", "Central", "East"), fracs):
            s = cw[-1] * f
            p, d = at(W, cw, s)
            n = math.hypot(*d)
            nx, nz = -d[1] / n, d[0] / n                           # across the way
            for side in (1, -1):                                   # the side with room for the booth
                q = (p[0] + nx * SIDE * side, p[1] + nz * SIDE * side)
                if not inside_lot(q, lots):
                    break
            sid = (word + "_" + district).lower()
            j = "j_" + sid
            junctions[j] = (round(p[0], 2), round(p[1], 2))
            stops[sid] = dict(name=word + " " + district, pos=[round(q[0], 2), round(q[1], 2)], face=[round(-nx * side, 3), round(-nz * side, 3)], junction=j)
            js.append((s, j))
        jm = "j_" + district.lower() + "_road"                     # where the road out meets the way
        junctions[jm] = (round(W[meet][0], 2), round(W[meet][1], 2))
        trunks.append((from_j, jm, [tuple(junctions[from_j])] + thin(out, 0.0, co[-1], co)[1:-1] + [tuple(junctions[jm])]))
        order = sorted(js + [(s_meet, jm)])                        # along the way: the stops' junctions and the road's, in order
        for (s0, a), (s1, b) in zip(order, order[1:]):
            trunks.append((a, b, [tuple(junctions[a])] + thin(W, s0, s1, cw)[1:-1] + [tuple(junctions[b])]))
    props = [tuple(pr["pos"]) for bl in w["blocks"] for pr in bl.get("props", [])]
    veg = w["vegetation"].get("species", w["vegetation"])
    trees = [(tuple(t["pos"]), veg.get(t["species"], {}).get("r_max", 4.0)) for bl in w["blocks"] for t in bl.get("trees", [])]
    for sid, st in stops.items():                                  # a booth clear of the street's lamps, plants and signs,
        x, z = st["pos"]                                           # and its riser clear of a grown tree's crown
        along = (-st["face"][1], st["face"][0])
        for k in (0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6, 7, -7, 8, -8):
            q = (x + along[0] * 2.0 * k, z + along[1] * 2.0 * k)
            if all(abs(q[0] - pp[0]) >= 1.6 or abs(q[1] - pp[1]) >= 1.6 for pp in props) and not inside_lot(q, lots, 0.5)                     and all(math.dist(q, tp) >= r + CROWN_CLEAR for tp, r in trees):
                st["pos"] = [round(q[0], 2), round(q[1], 2)]
                break
        else:
            raise SystemExit("no room for the booth of " + sid)
    net = dict(stops={k: {kk: vv for kk, vv in v.items() if kk != "junction"} for k, v in stops.items()},
               junctions={j: dict(pos=[x, y, RUN_Z]) for j, (x, y) in junctions.items()}, tubes=[])
    mouth_z = rules["ride"]["draw_in_height_m"]
    for sid, s in stops.items():                                   # each stop's own tube up to the line
        x, y = s["pos"]
        jx, jy = junctions[s["junction"]]
        net["tubes"].append(dict(a=sid, b=s["junction"], line=[[x, y, mouth_z], [x, y, RUN_Z], [jx, jy, RUN_Z]]))
    for a, b, pts in trunks:
        net["tubes"].append(dict(a=a, b=b, line=[[round(x, 2), round(y, 2), RUN_Z] for x, y in pts]))
    out = dict(note="Glass travel tubes (CITY-108, CITY-112; F-VEH-TUBE level 2). Written by Tools/citysim/make_tubes_01.py. "
                    "Approved by the owner on the map (Tools/citysim/tube_map.py, Art/StyleTests/tube_routes_03.png), 2026-11-28: \"Approve the routes\", "
                    "after three stops in each suburb, Eastgate's stop by its main entrance and two loops round the centre with stops at key buildings.",
               status="approved", **net)
    bad = problems(net, rules)
    if bad:
        raise SystemExit("tubes.json has problems:\n  " + "\n  ".join(bad))
    p = os.path.join(world.WORLD, "city_01", "tubes.json")
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        json.dump(out, f, indent=1)
        f.write("\n")
    print("wrote %s: %d stops, %d junctions, %d tubes" % (p, len(stops), len(junctions), len(net["tubes"])))


if __name__ == "__main__":
    build()
