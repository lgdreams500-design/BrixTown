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
from tubes import Near, booth_clashes, crown_top, post_clashes, problems, road_middles, rounded, street_things, through_crowns   # noqa: E402

RUN_Z = 9.0                       # the lowest the tubes run; higher where a grown tree's crown would reach them
DOOR_ROOM = 2.5                   # m clear in front of a booth's door
CAMERA_ROOM = 0.5                 # m between the ride's camera beside a booth and a crown or building
LINE_OFF = 6.0                    # m from a street's middle to its tube's line: over the pavement, behind the kerb's lamps,
                                  # trees and parked cars, so the posts under it stand on the pavement, not in the road
JOIN_M = 8.0                      # a line keeps its offset to this far from a junction
MERGE_M = 4.0                     # a booth's own tube runs at least this far across to the line (never rises into it)
CROWN_CLEAR = 1.0                 # a booth this far outside a grown tree's crown (vegetation.json r_max), the tubes this far over it
SIDE = 8.0        # m from a lane's middle to a booth beside it
STEP = 12.0       # m between the points a tube keeps along a curving road

# stops inside and by the wall: (name, where, which way its door looks). Each joins the nearest line over the street.
# Along the two loops round the centre (the owner, 2026-11-28, drawn on the map: "2 concentric loops ... stations ... at key
# buildings") a booth stands on the pavement on its building's side, 6.5 m from the street's middle, its door to the building
# where there is room (DOOR_ROOM), else along the pavement towards the building's door (at 0.75 m from the front, stepping out
# of the booth met the wall and the camera behind was caught between them: the owner, 2026-11-28, "camera eye got blocked").
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
    [(1248.0, 1056.0), (1283.0, 1056.0)],                                                                # on along Depot Street to the Town Depot
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
    lines = [list(line) for line in STREET_LINES]                  # Baker Street's way to Eastgate down Eastgate Road as the
    eg = [tuple(p) for p in links["Eastgate Road"] if 768.0 < p[1] < 868.0]   # road curves, not straight past it
    i = lines[0].index((1544.0, 868.0))
    lines[0][i:i + 1] = [eg[k] for k in range(0, len(eg), 3)] + [(eg[-1][0], 868.0)]
    segs = [(p, q) for line in lines for p, q in zip(line, line[1:])]
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
    points = {p for line in lines for p in line}
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
            # the booth beside the way, on the first stop's side for the others (all three across from the one line):
            # moved along the way, up to 40 m, to where that side has room
            sides = (js_side,) if js else (1, -1)
            for k in sorted(range(-10, 11), key=abs):
                s = min(max(cw[-1] * f + 4.0 * k, 1.0), cw[-1] - 1.0)
                p, d = at(W, cw, s)
                n = math.hypot(*d)
                nx, nz = -d[1] / n, d[0] / n                       # across the way
                hit = [side for side in sides if not inside_lot((p[0] + nx * SIDE * side, p[1] + nz * SIDE * side), lots)]
                if hit:
                    side = hit[0]
                    q = (p[0] + nx * SIDE * side, p[1] + nz * SIDE * side)
                    break
            else:
                raise SystemExit("no room on its side of %s for the %s stop" % (way, word))
            sid = (word + "_" + district).lower()
            j = "j_" + sid
            junctions[j] = (round(p[0], 2), round(p[1], 2))
            stops[sid] = dict(name=word + " " + district, pos=[round(q[0], 2), round(q[1], 2)], face=[round(-nx * side, 3), round(-nz * side, 3)], junction=j)
            js.append((s, j))
            js_side = side if len(js) == 1 else js_side
        jm = "j_" + district.lower() + "_road"                     # where the road out meets the way
        junctions[jm] = (round(W[meet][0], 2), round(W[meet][1], 2))
        trunks.append((from_j, jm, [tuple(junctions[from_j])] + thin(out, 0.0, co[-1], co)[1:-1] + [tuple(junctions[jm])]))
        order = sorted(js + [(s_meet, jm)])                        # along the way: the stops' junctions and the road's, in order
        for (s0, a), (s1, b) in zip(order, order[1:]):
            trunks.append((a, b, [tuple(junctions[a])] + thin(W, s0, s1, cw)[1:-1] + [tuple(junctions[b])]))
    props = [tuple(pr["pos"]) for bl in w["blocks"] for pr in bl.get("props", [])]
    veg = w["vegetation"].get("species", w["vegetation"])
    trees = [(tuple(t["pos"]), veg.get(t["species"], {}).get("r_max", 4.0)) for bl in w["blocks"] for t in bl.get("trees", [])]
    grown = [(p[0], p[1], r, veg.get(t["species"], {}).get("crown")) for bl in w["blocks"] for t in bl.get("trees", [])
             for p, r in [(t["pos"], veg.get(t["species"], {}).get("r_max", 4.0))]]
    # the lines over the pavement (the owner, 2026-11-28: "it's overlapping bus stop etc"; down the street's middle every
    # post stood in the road): each stretch LINE_OFF to its south or east side, the same side all along it, and each
    # junction where the stretches' offset lines meet (at a crossing, its south-east corner)
    def unit(v):
        n = math.hypot(v[0], v[1]) or 1.0
        return (v[0] / n, v[1] / n)
    def side(d):                                                   # across d, to the south or east
        n = (-d[1], d[0])
        return n if n[0] + n[1] > 1e-9 or (abs(n[0] + n[1]) <= 1e-9 and n[0] > 0) else (-n[0], -n[1])
    ends = {}
    booth_at = {st["junction"]: st["pos"] for st in stops.values()}
    for k, (a, b, pts) in enumerate(trunks):
        n0 = side(unit((pts[-1][0] - pts[0][0], pts[-1][1] - pts[0][1])))
        for j, p in ((a, pts[0]), (b, pts[-1])):                   # over the far pavement from a stop's booth, so its own
            if j in booth_at:                                      # tube crosses the street to the line, bending on the way
                bx, bz = booth_at[j]
                if (bx - p[0]) * n0[0] + (bz - p[1]) * n0[1] > 0:
                    n0 = (-n0[0], -n0[1])
                break
        segn = []
        for p0, p1 in zip(pts, pts[1:]):
            n = (-(p1[1] - p0[1]), p1[0] - p0[0])
            n = unit(n)
            segn.append(n if n[0] * n0[0] + n[1] * n0[1] >= 0 else (-n[0], -n[1]))
        moved = [pts[0]]
        d0 = unit((pts[1][0] - pts[0][0], pts[1][1] - pts[0][1]))
        if math.dist(pts[0], pts[1]) > 2 * JOIN_M + 1:            # on its own offset from JOIN_M out of the junction, so a
            moved.append((pts[0][0] + d0[0] * JOIN_M + segn[0][0] * LINE_OFF,   # line changing sides there crosses only
                          pts[0][1] + d0[1] * JOIN_M + segn[0][1] * LINE_OFF))   # over the crossing
        for i in range(1, len(pts) - 1):                           # a mitred corner between the two stretches' offset lines
            m = unit((segn[i - 1][0] + segn[i][0], segn[i - 1][1] + segn[i][1]))
            c = max(.5, m[0] * segn[i][0] + m[1] * segn[i][1])
            moved.append((pts[i][0] + m[0] * LINE_OFF / c, pts[i][1] + m[1] * LINE_OFF / c))
        d1 = unit((pts[-1][0] - pts[-2][0], pts[-1][1] - pts[-2][1]))
        if math.dist(pts[-2], pts[-1]) > 2 * JOIN_M + 1:
            moved.append((pts[-1][0] - d1[0] * JOIN_M + segn[-1][0] * LINE_OFF, pts[-1][1] - d1[1] * JOIN_M + segn[-1][1] * LINE_OFF))
        moved.append(pts[-1])
        trunks[k] = (a, b, moved)
        ends.setdefault(a, []).append(segn[0])
        ends.setdefault(b, []).append(segn[-1])
    def meet(ns):
        sxx = sum(n[0] * n[0] for n in ns) + 1e-6; sxz = sum(n[0] * n[1] for n in ns); szz = sum(n[1] * n[1] for n in ns) + 1e-6
        bx = sum(n[0] * LINE_OFF for n in ns) + 1e-6 * ns[0][0] * LINE_OFF; bz = sum(n[1] * LINE_OFF for n in ns) + 1e-6 * ns[0][1] * LINE_OFF
        det = sxx * szz - sxz * sxz
        ox, oz = (bx * szz - bz * sxz) / det, (bz * sxx - bx * sxz) / det
        return ox, oz, max(abs(ox * n[0] + oz * n[1] - LINE_OFF) for n in ns)
    for j, ns in ends.items():                                     # where its stretches' offset lines meet
        jx, jz = junctions[j]
        ox, oz, miss = meet(ns)
        if miss > .3:                                              # roads meeting at angles: on the grid street's line, the
            ns = [n for n in ns if max(abs(n[0]), abs(n[1])) > .99] or ns[:1]   # others' lines come over to it
            ox, oz, miss = meet(ns)
            print("  junction %s: on the street's line; the roads' lines come across to it" % j)
        junctions[j] = (round(jx + ox, 2), round(jz + oz, 2))
    for k, (a, b, pts) in enumerate(trunks):
        trunks[k] = (a, b, [junctions[a]] + [(round(x, 2), round(z, 2)) for x, z in pts[1:-1]] + [junctions[b]])
    things = {**street_things(w), "middles": road_middles(w)}
    for sid, st in stops.items():                                  # a booth moved along its pavement, the nearest it can
        x, z = st["pos"]                                           # stand clear of the bus stops, doors, lamps, zebras, the road
        along = (-st["face"][1], st["face"][0])                    # and the rest (booth_clashes), its riser clear of a crown
        m = min(things["middles"], key=lambda p: math.dist(p, (x, z)))
        if math.dist(m, (x, z)) < rules["place"]["road_m"]:         # off the road first, straight away from its middle
            u = unit((x - m[0], z - m[1]))
            x, z = m[0] + u[0] * rules["place"]["road_m"], m[1] + u[1] * rules["place"]["road_m"]
            print("  %s: onto the pavement, %.1f m from the road's middle" % (sid, rules["place"]["road_m"]))
        for k in sorted(range(-40, 41), key=lambda k: (abs(k), -k)):
            q = (x + along[0] * 1.0 * k, z + along[1] * 1.0 * k)
            if not booth_clashes(q, things, rules["place"]) and all(math.dist(q, tp) >= r + CROWN_CLEAR for tp, r in trees):
                if k:
                    print("  %s: %+d m along its pavement" % (sid, k))
                st["pos"] = [round(q[0], 2), round(q[1], 2)]
                break
        else:
            raise SystemExit("no room for the booth of %s: %s" % (sid, booth_clashes((x, z), things, rules["place"])))
    for sid, st in stops.items():                                  # a door with room to step out of it: where the building
        x, z = st["pos"]                                           # stands closer than DOOR_ROOM, the door looks along the
        f = st["face"]                                             # pavement instead, towards the building's own door
        near = [l for l in lots for d in range(int(DOOR_ROOM / .25) + 1)
                if inside_lot((x + f[0] * (.75 + d * .25), z + f[1] * (.75 + d * .25)), [l], 0)]
        if not near:
            continue
        along, door = (-f[1], f[0]), near[0].get("door")
        sgn = 1 if door is None or (door[0] - x) * along[0] + (door[1] - z) * along[1] >= 0 else -1
        st["face"] = [along[0] * sgn, along[1] * sgn]
        f = st["face"]
        if any(inside_lot((x + f[0] * (.75 + d * .25), z + f[1] * (.75 + d * .25)), lots, 0) for d in range(int(DOOR_ROOM / .25) + 1)):
            raise SystemExit("no room at the door of " + sid)
    # each stop's junction moved along its line to the point nearest its booth, wherever the booth was moved to
    for sid, st in stops.items():
        j = st["junction"]
        touch = [k for k, (a, b, _) in enumerate(trunks) if j in (a, b)]
        if len(touch) != 2 or j in JUNCTIONS:
            continue
        (k1, k2) = touch
        a1, b1, p1 = trunks[k1]
        a2, b2, p2 = trunks[k2]
        path = (p1 if b1 == j else p1[::-1]) + (p2 if a2 == j else p2[::-1])[1:]   # end1 .. j .. end2
        o1, o2 = (a1 if b1 == j else b1), (b2 if a2 == j else a2)
        cum = [0.0]
        for p, q in zip(path, path[1:]):
            cum.append(cum[-1] + math.dist(p, q))
        best = None
        for i, (p, q) in enumerate(zip(path, path[1:])):
            pq, f = project(st["pos"], p, q)
            u = cum[i] + f * (cum[i + 1] - cum[i])
            if 2.0 <= u <= cum[-1] - 2.0 and (best is None or math.dist(pq, st["pos"]) < best[0]):
                best = (math.dist(pq, st["pos"]), u, i, pq)
        if best is None:
            continue
        dist, u, i, pq = best
        if dist < MERGE_M:                                         # its tube must cross to the line: rising straight into
            raise SystemExit("the line passes over the booth of %s (%.1f m)" % (sid, dist))   # it, a rider would flip
        pq = (round(pq[0], 2), round(pq[1], 2))
        junctions[j] = pq
        trunks[k1] = (o1, j, path[:i + 1] + [pq])
        trunks[k2] = (j, o2, [pq] + path[i + 1:])
    # the side of each booth the ride's camera stands on as the rider rises out of it and comes down into it (behind_m off,
    # tubes.py camera()): across the booth's own tube, on the side clear of grown crowns and buildings (the owner,
    # 2026-11-28: "i noticed camera issues"; at Market Square it stood in a plane's crown)
    behind = rules["camera"]["behind_m"]
    def room(q):                                                   # how far q is from the nearest crown's edge or lot
        rc = min(math.dist(q, (tx, tz)) - crown_top(r, cr)[1] for tx, tz, r, cr in grown)
        rl = min(max(l["rect"][0] - q[0], q[0] - l["rect"][2], l["rect"][1] - q[1], q[1] - l["rect"][3]) for l in lots)
        return min(rc, rl)
    for sid, st in stops.items():
        x, z = st["pos"]
        jx, jz = junctions[st["junction"]]
        n = math.hypot(jx - x, jz - z) or 1.0
        d = ((jx - x) / n, (jz - z) / n)
        best = None                                                # the farthest out it can stand (behind_m down to 2.5 m),
        for far in (behind, 5.0, 4.0, 3.0, 2.5):                   # every 30 degrees round but not over its own tube, most
            for k in range(12):                                    # room first; where the street's planes meet over it, nearer
                a = math.radians(30 * k)
                c = (d[0] * math.cos(a) - d[1] * math.sin(a), d[0] * math.sin(a) + d[1] * math.cos(a))
                if c[0] * d[0] + c[1] * d[1] > .5:
                    continue
                r = min(room((x + c[0] * far * f, z + c[1] * far * f)) for f in (.5, 1.0))
                if best is None or r > best[0] + 1e-9:
                    best = (r, c, far)
            if best[0] >= CAMERA_ROOM:
                break
        st["camera"] = [round(best[1][0] * best[2], 2), round(best[1][1] * best[2], 2)]
        if best[0] < CAMERA_ROOM or best[2] < behind:
            print("  %s: the camera stands %.1f m out, %.1f m from a crown or building" % (sid, best[2], best[0]))

    def lay(run_z):
        net = dict(stops={k: {kk: vv for kk, vv in v.items() if kk != "junction"} for k, v in stops.items()},
                   junctions={j: dict(pos=[x, y, run_z]) for j, (x, y) in junctions.items()}, tubes=[])
        mouth_z = rules["ride"]["draw_in_height_m"]
        for sid, s in stops.items():                               # each stop's own tube up to the line
            x, y = s["pos"]
            jx, jy = junctions[s["junction"]]
            net["tubes"].append(dict(a=sid, b=s["junction"], line=[[x, y, mouth_z], [x, y, run_z], [jx, jy, run_z]]))
        for a, b, pts in trunks:
            net["tubes"].append(dict(a=a, b=b, line=[[round(x, 2), round(y, 2), run_z] for x, y in pts]))
        return net
    # the tubes over the street trees as they will grow (the owner, 2026-11-28: "Raise the tubes"): one height for all of
    # them, the half metre over the tallest grown crown under a run and its glass
    hit = through_crowns(lay(RUN_Z), rules, grown, CROWN_CLEAR)
    run_z = max([RUN_Z] + [math.ceil((top + CROWN_CLEAR + rules["tube_inner_m"] / 2 + .12) * 2) / 2 for _, _, top in hit])
    net = lay(run_z)
    # the posts (the owner, 2026-11-28: "overlapping bus stop etc"): under each level stretch of the lines over the streets
    # (not the stops' own short tubes) about every span_m, each moved along the line to the nearest spot clear of the
    # street's things, the booths and the road (tubes.py post_clashes()); where tubes run together, one post
    pr, posts = rules["place"]["post"], []
    near = Near({**things, "middles": road_middles(w), "booths": [tuple(st["pos"]) for st in stops.values()]})
    spans = []
    for t in net["tubes"]:                                         # (a stop's own tube: under its level part, the booth
        line = rounded(t["line"], rules["ride"]["bend_r_m"])        # keeping posts off its riser)
        if t["a"] in stops:
            line = [p for p in line if p[2] >= run_z - .01]
            if len(line) < 2 or math.dist(line[0][:2], line[-1][:2]) < 4:
                continue
        cum = [0.0]
        for p, q in zip(line, line[1:]):
            cum.append(cum[-1] + math.dist(p, q))
        L, last, s = cum[-1], None, 0.0
        def at_s(u):
            i = max(k for k in range(len(cum) - 1) if cum[k] <= u + 1e-9) if u < L else len(cum) - 2
            p, q = line[i], line[i + 1]
            f = (u - cum[i]) / ((cum[i + 1] - cum[i]) or 1)
            return (p[0] + (q[0] - p[0]) * f, p[1] + (q[1] - p[1]) * f), unit((q[0] - p[0], q[1] - p[1]))
        u = pr["span_m"] / 2 if t["a"] in stops else 0.0          # (a line's first post at its junction's corner)
        while u < L:
            found = None
            for dk in sorted(range(-int(pr["shift_m"] * 2), int(pr["shift_m"] * 2) + 1), key=abs):
                v = u + dk * .5
                if not 0 <= v <= L:
                    continue
                q, d = at_s(v)
                q = (round(q[0], 2), round(q[1], 2))
                if any(math.dist(q, pp[:2]) < 3 for pp in posts):  # a post of another tube stands here: shared
                    found = (v, None)
                    break
                n = near(q)
                if not post_clashes(q, n, rules["place"], n["booths"], n["middles"]):
                    found = (v, (round(q[0], 2), round(q[1], 2), run_z, round(d[0], 3), round(d[1], 3)))
                    break
            if found is None:
                u += 1.0                                           # nowhere here: a metre on
                continue
            if found[1]:
                posts.append(found[1])
            if last is not None:
                spans.append((found[0] - last, t["a"], t["b"]))
            last = found[0]
            u = found[0] + pr["span_m"]
    net["posts"] = [list(p) for p in posts]
    wide = max(spans)
    print("  %d posts; the widest span %.1f m (%s to %s)" % (len(posts), wide[0], wide[1], wide[2]))
    if through_crowns(net, rules, grown, CROWN_CLEAR):
        raise SystemExit("the tubes still pass through a tree's crown at %.1f m" % run_z)
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
    print("wrote %s: %d stops, %d junctions, %d tubes, running at %.1f m (%d grown crowns under the lines)"
          % (p, len(stops), len(junctions), len(net["tubes"]), run_z, len({h[1] for h in hit})))


if __name__ == "__main__":
    build()
