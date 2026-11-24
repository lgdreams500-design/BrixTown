"""Eastgate Mall's inside, designed afresh for its new shell (WLD-058 to WLD-061, ADR-0038). The owner, 2026-11-24, after
the ribbon walls and pierced eggs of mall_shell_01.py: "ignore the old mall and use new interiors as now we have ribbon
walls and four pierced domes etc. start interiors fresh as this is new design ... feel free to come with ideas that fit
this new design language for this mall."

The idea, in the shell's own language (flowing ribbons, eggs, round arches, lantern light):

  the Great Court   a long oval court under the roof light, open through all three floors. Round it on each upper floor a
                    gallery whose edge is a ribbon that wanders; the upper gallery stands further back than the lower,
                    so the court opens as it rises. On its floor a fountain and gardens with trees and benches.
  four concourses   one from each arch of the shell straight to the court, so the mall has an entrance on every side
  four egg halls    under each egg a round hall, open from its floor up into the egg: the big store of that corner
                    (Food Market, Cinema, Department Store, Fashion House), reached by a slanting walk from the court.
                    On the upper floors that walk ends in a balcony looking into the hall.
  the cascades      in each half of the court a bridge crosses on each upper floor, and moving stairs climb from the
                    court floor to the lower bridge and on to the upper one
  shops             a front on every stretch of wall along the court, the concourses and the slanting walks

It replaces the lot's `plan` (malls_01.py, concept "loop") and its `customer` and `seat` places; the lot's rect, door,
floors and tags stay. The ground floor keeps the same jobs as before, so that nobody's work changes (careers.posts counts
them from the ground floor's units): four big stores of 3,600 m2, 58 shops and two cafes big enough for a full staff, in
the same order of kinds; every other ground-floor unit is too small to have staff of its own. Run add_careers_01.py
afterwards: it moves the staff to where their units now are (`stand`).

The plan keeps the old keys and adds some:

  concept            "court"
  storey_height      metres from one floor to the next
  levels[]           {level, units, walks, voids, bridges, fronts, walls, rails}
    units            [{id, name, kind, rect, angle, stand?}]  kind: shop, food, anchor, kiosk; rect and angle as a lot's
                     (it faces (sin, -cos) of its angle); stand: where its staff stand, one [x, z] each
    walks            polygons people walk on (they overlap; the floor is all of them together)
    voids            polygons open to the floor below
    bridges          polygons that cross a void
    fronts           [{unit, a, b}]: the shop front of a unit, from a to b along the edge of a walk
    walls            [{kind, line}]: every other stretch of wall along the walks; kind "wall" or "drum" (an egg hall's)
    rails            polylines along edges where a floor stops at a void: the ribbon parapets
  entrances          [{side, at, width}]: openings in the hall's wall, one under each arch of the shell
  halls              [{unit, pos, r, dome}]: the round halls, each under egg number `dome` of the shell
  ramps              [{from, to, a, b, width}]: moving stairs from floor `from` at a to floor `to` at b; seen from a, the right
                     half carries people up and the left half carries them down
  lifts              [{pos, r, door}]: a round lift shaft through every floor; door in degrees, the way it faces (90 east)
  fountains          [{pos, r, level}]
  gardens            [{pos, r: [rx, rz], trees: [[x, z, species], ...]}]: planted beds on the court floor
  benches            [{pos, yaw}]: yaw in degrees, the way the sitter faces (0 north, 90 east)
  roof               {opening: polygon}: the hole in the roof under the roof light
Every size and count here is a first guess [ASSUMPTION].

Run:   python mall_inside_01.py           (works the inside out again and writes it over the old one)
       python mall_inside_01.py --draw    (also draws the three floors into Art/StyleTests/mall_inside_26/)
Then:  python add_careers_01.py, python mall_shell_01.py, python -m unittest test_world, python build_play.py
"""
import json, math, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.normpath(os.path.join(HERE, "..", "..", "World", "city_01"))
BLOCKS = os.path.join(CITY, "blocks")
BID = "b_16_5"

STOREY, LEVELS = 6.0, 3
COURT = (46.0, 96.0)                      # the line of shop fronts round the court: half-sizes east-west, north-south
VOIDS = {1: (31.0, 81.0, 0.045, 5, 0.6), 2: (37.0, 87.0, 0.035, 5, 2.4)}   # the opening in each upper floor: half-sizes, how far its edge wanders, how many times round, where it starts
ROOF = (41.5, 91.5)                       # the opening in the roof, inside the roof light's foot
LIFT = (30.0, 1.9)                                         # a lift's middle this far in from a side entrance, and its shaft's radius
CONCOURSE = {"s": 10.0, "n": 8.0, "w": 9.0, "e": 8.0}    # half-widths of the walks in from the arches
INTO_COURT = 0.72                         # the concourses and slanting walks start this share of the way out from the middle
DIAGONAL = 5.5                            # half-width of the slanting walks to the egg halls
HALL_R = 0.9                              # an egg hall's radius, as a share of the least distance from the egg's middle to the hall's walls
BALCONY = 5.0                             # on the upper floors a slanting walk runs this far into its egg hall
STORE = (80.0, 45.0)                      # a big store's ground in its corner of the hall, along the north or south wall: 3,600 m2, as before
FRONT = 16.0                              # metres of walk a shop front takes
DEEP = 21.0                               # a shop with staff of its own is this deep: over 315 m2, a full staff (careers.json mall_shop)
SMALL_M2 = 138.0                          # a shop under 145 m2 has no staff of its own
UPPER = (21.0, 16.0, 12.0, 9.0, 6.0)      # depths tried for a shop on the upper floors
STAFFED = 60                              # ground-floor units with staff, and which of them are cafes: as the old ground floor had
CAFES = (11, 39)
BRIDGE = (36.0, 3.5, 13.0)                # the cascades: how far from the middle the lower bridge crosses, its half-width, the run of a moving stair
RAMP_W = 4.4
ANCHORS = {"sw": "Food Market", "se": "Cinema", "nw": "Department Store", "ne": "Fashion House"}
STEP = 1.0                                # metres between samples along an edge
CUSTOMERS, SEATS = 80, 32                 # places for visitors, as the lot had


def r2(v):
    return round(v, 2)


def pt(p):
    return [r2(p[0]), r2(p[1])]


def inside(poly, x, z):
    n, hit = len(poly), False
    for i in range(n):
        (x1, z1), (x2, z2) = poly[i], poly[(i + 1) % n]
        if (z1 > z) != (z2 > z) and x < x1 + (z - z1) * (x2 - x1) / (z2 - z1):
            hit = not hit
    return hit


def ellipse(cx, cz, a, b, n=96, wave=0.0, times=0, phase=0.0):
    out = []
    for k in range(n):
        t = 2 * math.pi * k / n
        s = 1 + wave * math.sin(times * t + phase)
        out.append((cx + a * s * math.cos(t), cz + b * s * math.sin(t)))
    return out


def band(p, q, h):
    """A strip from p to q, h metres either side of the line."""
    L = math.dist(p, q)
    nx, nz = -(q[1] - p[1]) / L * h, (q[0] - p[0]) / L * h
    return [(p[0] + nx, p[1] + nz), (q[0] + nx, q[1] + nz), (q[0] - nx, q[1] - nz), (p[0] - nx, p[1] - nz)]


def area(poly):
    return sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1] for i in range(len(poly))) / 2


def corners(u):
    """The four corners of a unit: rect turned by angle degrees about its middle (as a lot's; it faces (sin, -cos))."""
    x0, z0, x1, z1 = u["rect"]
    cx, cz, t = (x0 + x1) / 2, (z0 + z1) / 2, math.radians(u.get("angle", 0))
    c, s = math.cos(t), math.sin(t)
    return [(cx + (x - cx) * c - (z - cz) * s, cz + (x - cx) * s + (z - cz) * c) for x, z in ((x0, z0), (x1, z0), (x1, z1), (x0, z1))]


def overlap(p, q):
    """Do two convex polygons overlap (separating axes)?"""
    for poly in (p, q):
        for i in range(len(poly)):
            ax, az = poly[(i + 1) % len(poly)][1] - poly[i][1], poly[i][0] - poly[(i + 1) % len(poly)][0]
            a = [x * ax + z * az for x, z in p]
            b = [x * ax + z * az for x, z in q]
            if max(a) <= min(b) + 0.05 or max(b) <= min(a) + 0.05:
                return False
    return True


def length(run):
    return sum(math.dist(run[i], run[i + 1]) for i in range(len(run) - 1))


def along(run, s):
    acc = 0.0
    for i in range(len(run) - 1):
        d = math.dist(run[i], run[i + 1])
        if acc + d >= s - 1e-9:
            f = (s - acc) / d if d else 0
            return (run[i][0] + (run[i + 1][0] - run[i][0]) * f, run[i][1] + (run[i + 1][1] - run[i][1]) * f)
        acc += d
    return run[-1]


def design(lot, shell):
    x0, z0, x1, z1 = lot["rect"]
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    bid, short = lot["id"].rsplit("_l", 1)[0], lot["name"].split()[0]
    court = ellipse(cx, cz, *COURT)
    arches = {a["side"]: a for a in shell["arches"]}
    k = INTO_COURT
    conc = {"s": [(cx - CONCOURSE["s"], cz + COURT[1] * k), (cx + CONCOURSE["s"], cz + COURT[1] * k), (cx + CONCOURSE["s"], z1), (cx - CONCOURSE["s"], z1)],
            "n": [(cx - CONCOURSE["n"], z0), (cx + CONCOURSE["n"], z0), (cx + CONCOURSE["n"], cz - COURT[1] * k), (cx - CONCOURSE["n"], cz - COURT[1] * k)],
            "w": [(x0, cz - CONCOURSE["w"]), (cx - COURT[0] * k, cz - CONCOURSE["w"]), (cx - COURT[0] * k, cz + CONCOURSE["w"]), (x0, cz + CONCOURSE["w"])],
            "e": [(cx + COURT[0] * k, cz - CONCOURSE["e"]), (x1, cz - CONCOURSE["e"]), (x1, cz + CONCOURSE["e"]), (cx + COURT[0] * k, cz + CONCOURSE["e"])]}
    halls, diag_ground, diag_upper = [], [], []
    for n, d in enumerate(shell["domes"]):
        px, pz = d["pos"]
        corner = ("s" if pz > cz else "n") + ("w" if px < cx else "e")
        r = r2(HALL_R * min(px - x0, x1 - px, pz - z0, z1 - pz))
        ux, uz = px - cx, pz - cz
        L = math.hypot(ux, uz)
        ux, uz = ux / L, uz / L
        rc = 1 / math.hypot(ux / COURT[0], uz / COURT[1])           # how far out the court's line is, that way
        start = (cx + ux * rc * k, cz + uz * rc * k)
        diag_ground.append(band(start, (px - ux * r * 0.5, pz - uz * r * 0.5), DIAGONAL))
        diag_upper.append(band(start, (px - ux * (r - BALCONY), pz - uz * (r - BALCONY)), DIAGONAL))
        sq = [x0 if "w" in corner else x1 - STORE[0], z0 if "n" in corner else z1 - STORE[1]]
        halls.append(dict(corner=corner, pos=(px, pz), r=r, dome=n, name="%s %s" % (short, ANCHORS[corner]), disc=ellipse(px, pz, r, r, 64),
                          rect=[sq[0], sq[1], sq[0] + STORE[0], sq[1] + STORE[1]]))

    def edges(level):
        """The outline of everything walked on at this floor, as stretches of wall: [(what, which, points)]."""
        shapes = [("court", "", court)] + [("conc", s, conc[s]) for s in "snwe"]
        shapes += [("diag", halls[i]["corner"], p) for i, p in enumerate(diag_ground if level == 0 else diag_upper)]
        if level == 0:
            shapes += [("hall", h["corner"], h["disc"]) for h in halls]
        polys = [p for _, _, p in shapes]
        runs = []
        for si, (kind, which, poly) in enumerate(shapes):
            sign = 1 if area(poly) > 0 else -1
            pts = []
            for i in range(len(poly)):
                a, b = poly[i], poly[(i + 1) % len(poly)]
                n = max(1, round(math.dist(a, b) / STEP))
                pts += [(a[0] + (b[0] - a[0]) * j / n, a[1] + (b[1] - a[1]) * j / n) for j in range(n)]
            keep = []
            for i in range(len(pts)):
                a, b = pts[i], pts[(i + 1) % len(pts)]
                L = math.dist(a, b) or 1
                ox, oz = (b[1] - a[1]) / L * sign, -(b[0] - a[0]) / L * sign     # outward
                mx, mz = (a[0] + b[0]) / 2 + ox * 0.2, (a[1] + b[1]) / 2 + oz * 0.2
                out = not any(inside(q, mx, mz) for qi, q in enumerate(polys) if qi != si)
                keep.append(out and x0 + 0.1 < mx < x1 - 0.1 and z0 + 0.1 < mz < z1 - 0.1)   # the hall's own wall is not ours to draw
            if all(keep):
                runs.append((kind, which, pts + [pts[0]]))
                continue
            first = next(i for i in range(len(pts)) if keep[i] and not keep[i - 1])
            cur = None
            for j in range(len(pts)):
                i = (first + j) % len(pts)
                if keep[i]:
                    cur = cur or [pts[i]]
                    cur.append(pts[(i + 1) % len(pts)])
                elif cur:
                    runs.append((kind, which, cur))
                    cur = None
            if cur:
                runs.append((kind, which, cur))
        return shapes, runs

    def rank(kind, which, mid):
        """Which shops come first on the ground floor (the first are the ones with staff): round the court from the south
        door, then the concourses, then the slanting walks."""
        if kind == "court":
            return (0, (math.atan2(mid[0] - cx, mid[1] - cz)) % (2 * math.pi))
        return (1 + "snwe".index(which) if kind == "conc" else 6, math.dist(mid, (cx, cz)))

    levels, nid = [], [len(halls)]

    def new_id():
        nid[0] += 1
        return "%s_u%d" % (bid, nid[0] - 1)

    anchors = []
    for i, h in enumerate(halls):                                    # each egg hall is one big store on every floor, with one id
        px, pz = h["pos"]
        stand = [pt((px + 0.5 * h["r"] * math.cos(0.3 + math.pi * j / 3), pz + 0.5 * h["r"] * math.sin(0.3 + math.pi * j / 3))) for j in range(6)]
        anchors.append(dict(id="%s_u%d" % (bid, i), name=h["name"], kind="anchor", rect=[r2(v) for v in h["rect"]], angle=0, stand=stand))
        h["unit"] = anchors[-1]["id"]
    for level in range(LEVELS):
        shapes, runs = edges(level)
        walk_polys = [p for _, _, p in shapes]
        cand, walls, rails, taken = [], [], [], []
        for kind, which, run in runs:
            L = length(run)
            if kind == "hall":
                walls.append(dict(kind="drum", line=run))
                continue
            if L < FRONT * 0.6:
                walls.append(dict(kind="wall", line=run))
                continue
            n = max(1, int(L / FRONT))
            w = L / n
            for j in range(n):
                a, b = along(run, j * w), along(run, (j + 1) * w)
                piece = [along(run, j * w + w * i / 8) for i in range(9)]
                if level and any(math.dist(q, h["pos"]) < h["r"] - 0.3 for h in halls for q in (a, b)):
                    rails.append(piece)                              # the end of a slanting walk inside an egg hall: a balcony's rail
                    continue
                fl = math.dist(a, b)
                mx, mz = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
                ox, oz = (b[1] - a[1]) / fl, -(b[0] - a[0]) / fl
                if any(inside(q, mx + ox * 0.6, mz + oz * 0.6) for q in walk_polys):
                    ox, oz = -ox, -oz                                # out of the walk, into the shop
                cand.append(dict(a=a, b=b, fl=fl, mid=(mx, mz), out=(ox, oz), piece=piece, rank=rank(kind, which, (mx, mz)), kind=kind))

        def fit(c, depth):
            (mx, mz), (ox, oz), fl = c["mid"], c["out"], c["fl"]
            bulge = max(0.0, max((p[0] - mx) * ox + (p[1] - mz) * oz for p in c["piece"]))   # round the court the front is a chord: the shop starts beyond the curve
            ucx, ucz = mx + ox * (bulge + depth / 2 + 0.3), mz + oz * (bulge + depth / 2 + 0.3)
            u = dict(rect=[r2(ucx - fl / 2 + 0.15), r2(ucz - depth / 2), r2(ucx + fl / 2 - 0.15), r2(ucz + depth / 2)], angle=round(math.degrees(math.atan2(-ox, oz)), 1))
            q = corners(u)
            ok = all(x0 + 0.1 <= x <= x1 - 0.1 and z0 + 0.1 <= z <= z1 - 0.1 for x, z in q)
            ok = ok and not any(overlap(q, t) for t in taken)        # (a big store's ground is its hall and its back rooms; a shop on the walk to it may stand on its edge)
            ok = ok and not any(overlap(q, t) for t in walk_polys if len(t) == 4)
            ok = ok and not any(inside(court, x, z) for x, z in (q[2], q[3], (ucx, ucz)))   # its front is a chord of the court's line; its back must be clear of the court
            rim = q + [((q[i][0] + q[(i + 1) % 4][0]) / 2, (q[i][1] + q[(i + 1) % 4][1]) / 2) for i in range(4)] + [(ucx, ucz)]
            ok = ok and not any(math.dist(p, h["pos"]) < h["r"] + 0.4 for h in halls for p in rim)
            return (u, q) if ok else None

        cand.sort(key=lambda c: c["rank"])
        big, small = [], []
        for c in cand:                                               # the deep shops first, so the shallow ones fill what is left
            got = fit(c, DEEP) if level == 0 and len(big) < STAFFED else None
            if level and not got:
                got = next((g for g in (fit(c, d) for d in UPPER) if g), None)
            if got:
                c["unit"] = got[0]
                taken.append(got[1])
                big.append(c)
        for c in cand:
            if "unit" in c:
                continue
            d0 = min(9.0, SMALL_M2 / (c["fl"] - 0.3)) if level == 0 else 6.0
            got = next((g for g in (fit(c, d) for d in (d0, d0 * 0.75)) if g), None) if d0 >= 4.5 else None
            if got:
                c["unit"] = got[0]
                taken.append(got[1])
                small.append(c)
            else:
                walls.append(dict(kind="wall", line=c["piece"]))
        if level == 0:
            assert len(big) == STAFFED, "the ground floor has room for %d shops with staff, not %d" % (len(big), STAFFED)
        units, fronts = [dict(u) for u in anchors], []
        for n, c in enumerate(big + small):
            u = c["unit"]
            staffed = level == 0 and n < STAFFED
            if level == 0:
                food = staffed and (n + 1) in CAFES
            else:
                food = (level == 2 and c["kind"] == "court") or n % 6 == 3
            u.update(id=new_id(), name="%s %d.%02d" % (short, level, len(units) + 1), kind="food" if food else "shop")
            if staffed:                                              # behind a counter 3 m in from the front
                (mx, mz), (ox, oz), m = c["mid"], c["out"], 4 if food else 3
                u["stand"] = [pt((mx + ox * 3.2 - oz * (i - (m - 1) / 2) * 2.2, mz + oz * 3.2 + ox * (i - (m - 1) / 2) * 2.2)) for i in range(m)]
            units.append(u)
            fronts.append(dict(unit=u["id"], a=pt(c["a"]), b=pt(c["b"])))
        if level:                                                   # the drums of the egg halls rise through the upper floors, open where a balcony comes in
            for h in halls:
                disc, cur = h["disc"], None
                shut = [not any(inside(q, (disc[i][0] + disc[(i + 1) % len(disc)][0]) / 2, (disc[i][1] + disc[(i + 1) % len(disc)][1]) / 2) for q in walk_polys) for i in range(len(disc))]
                first = next((i for i in range(len(disc)) if shut[i] and not shut[i - 1]), 0)
                for j in range(len(disc)):
                    i = (first + j) % len(disc)
                    if shut[i]:
                        cur = cur or [disc[i]]
                        cur.append(disc[(i + 1) % len(disc)])
                    elif cur:
                        walls.append(dict(kind="drum", line=cur))
                        cur = None
                if cur:
                    walls.append(dict(kind="drum", line=cur))
        voids, bridges = [], []
        if level:
            a, b, wave, times, phase = VOIDS[level]
            v = ellipse(cx, cz, a, b, 120, wave, times, phase)
            voids.append([pt(p) for p in v])
            rails.append(v + [v[0]])
            for half in (-1, 1):                                    # the bridges of the cascades: the upper one a moving stair's run further out
                bz = cz + half * (BRIDGE[0] + (level - 1) * (BRIDGE[2] + 2 * BRIDGE[1]))
                reach = max(abs(p[0] - cx) for p in v if abs(p[1] - bz) < BRIDGE[1] + 3) + 1.0
                bridges.append([pt(p) for p in ((cx - reach, bz - BRIDGE[1]), (cx + reach, bz - BRIDGE[1]), (cx + reach, bz + BRIDGE[1]), (cx - reach, bz + BRIDGE[1]))])
        levels.append(dict(level=level, units=units, walks=[[pt(p) for p in q] for q in walk_polys], voids=voids, bridges=bridges, fronts=fronts,
                           walls=[dict(kind=w["kind"], line=[pt(p) for p in w["line"]]) for w in walls], rails=[[pt(p) for p in rl] for rl in rails]))

    ramps = []
    for half in (-1, 1):                                            # moving stairs: court floor to the lower bridge, lower bridge to the upper
        zb = cz + half * BRIDGE[0]
        ramps.append({"from": 0, "to": 1, "a": pt((cx, zb - half * (BRIDGE[2] + BRIDGE[1]))), "b": pt((cx, zb - half * BRIDGE[1])), "width": RAMP_W})
        ramps.append({"from": 1, "to": 2, "a": pt((cx, zb + half * BRIDGE[1])), "b": pt((cx, zb + half * (BRIDGE[1] + BRIDGE[2]))), "width": RAMP_W})
    gardens, benches = [], []
    for half in (-1, 1):                                            # a garden beyond each cascade, trees in it, benches round it facing out
        gx, gz, rx, rz = cx, cz + half * 74.0, 12.0, 8.0
        trees = [[r2(gx + rx * 0.55 * math.cos(t)), r2(gz + rz * 0.55 * math.sin(t)), sp] for t, sp in ((0.4, "birch"), (2.5, "linden"), (4.4, "birch"))]
        gardens.append(dict(pos=pt((gx, gz)), r=[rx, rz], trees=trees))
        for i in range(8):
            t = 2 * math.pi * (i + 0.5) / 8
            benches.append(dict(pos=pt((gx + (rx + 1.2) * math.cos(t), gz + (rz + 1.2) * math.sin(t))), yaw=round(math.degrees(math.atan2(math.cos(t), -math.sin(t))), 1)))
    for sx in (-1, 1):                                              # and two small gardens beside the fountain
        gx, gz = cx + sx * 21.0, cz
        gardens.append(dict(pos=pt((gx, gz)), r=[5.0, 8.0], trees=[[r2(gx), r2(gz), "linden"]]))
        for sz in (-1, 1):
            benches.append(dict(pos=pt((gx, gz + sz * 9.4)), yaw=180.0 if sz > 0 else 0.0))
    ground = levels[0]["units"]
    for kx, kz in [(sx * 30.0, sz * 44.0) for sx in (-1, 1) for sz in (-1, 1)] + [(sx * 22.0, sz * 60.0) for sx in (-1, 1) for sz in (-1, 1)]:
        ground.append(dict(id=new_id(), name="%s 0.%02d" % (short, len(ground) + 1), kind="kiosk", rect=[r2(cx + kx - 2), r2(cz + kz - 2), r2(cx + kx + 2), r2(cz + kz + 2)], angle=0))
    lifts = [dict(pos=pt((x0 + LIFT[0], cz)), r=LIFT[1], door=90.0), dict(pos=pt((x1 - LIFT[0], cz)), r=LIFT[1], door=270.0)]   # a lift in each side concourse, its door towards the court
    plan = dict(concept="court", storey_height=STOREY, levels=levels,
                entrances=[dict(side=s, at=arches[s]["at"], width=2 * CONCOURSE[s] - 4.0) for s in "snwe"],
                halls=[dict(unit=h["unit"], pos=pt(h["pos"]), r=h["r"], dome=h["dome"]) for h in halls],
                ramps=ramps, lifts=lifts, fountains=[dict(pos=pt((cx, cz)), r=7.0, level=0)], gardens=gardens, benches=benches,
                roof=dict(opening=[pt(p) for p in ellipse(cx, cz, *ROOF, 96)]))

    # where visitors stand and sit: before the shop fronts and the kiosks and round the fountain; on the benches
    byid = {u["id"]: u for u in ground}
    customer = []
    for ring in (2.6, 5.0):
        for f in levels[0]["fronts"]:
            a, b, c = f["a"], f["b"], corners(byid[f["unit"]])
            mx, mz = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
            ux, uz = sum(p[0] for p in c) / 4 - mx, sum(p[1] for p in c) / 4 - mz
            L = math.hypot(ux, uz)
            customer.append(pt((mx - ux / L * ring, mz - uz / L * ring)))
        for u in ground:
            if u["kind"] == "kiosk":
                customer.append(pt(((u["rect"][0] + u["rect"][2]) / 2, u["rect"][3] + ring - 1.4)))
    seat = []
    for b in benches:
        t = math.radians(b["yaw"])
        for s in (-0.45, 0.45):
            seat.append(pt((b["pos"][0] + math.cos(t) * s, b["pos"][1] + math.sin(t) * s)))
    assert len(customer) >= CUSTOMERS and len(seat) >= SEATS
    return plan, dict(customer=customer[:CUSTOMERS], seat=seat[:SEATS])


def draw(lot, plan, folder):
    from PIL import Image, ImageDraw
    os.makedirs(folder, exist_ok=True)
    x0, z0, x1, z1 = lot["rect"]
    S = 3.0
    T = lambda p: ((p[0] - x0 + 20) * S, (p[1] - z0 + 20) * S)
    kinds = dict(shop=(196, 170, 120), food=(176, 110, 86), anchor=(110, 100, 96), kiosk=(110, 150, 150))
    for L in plan["levels"]:
        im = Image.new("RGB", (int((x1 - x0 + 40) * S), int((z1 - z0 + 40) * S)), (58, 56, 62))
        d = ImageDraw.Draw(im)
        d.rectangle([T((x0, z0)), T((x1, z1))], fill=(86, 82, 84), outline=(20, 20, 24))
        for u in L["units"]:
            if u["kind"] == "anchor":
                d.polygon([T(p) for p in corners(u)], fill=kinds["anchor"])
        for w in L["walks"]:
            d.polygon([T(p) for p in w], fill=(226, 216, 196))
        for v in L["voids"]:
            d.polygon([T(p) for p in v], fill=(120, 136, 150))
        for b in L["bridges"]:
            d.polygon([T(p) for p in b], fill=(226, 216, 196), outline=(90, 80, 70))
        if L["level"] == 0:
            for g in plan["gardens"]:
                p, (rx, rz) = g["pos"], g["r"]
                d.ellipse([T((p[0] - rx, p[1] - rz)), T((p[0] + rx, p[1] + rz))], fill=(104, 128, 84))
                for t in g["trees"]:
                    d.ellipse([T((t[0] - 2.5, t[1] - 2.5)), T((t[0] + 2.5, t[1] + 2.5))], fill=(70, 98, 60))
            for f in plan["fountains"]:
                p, r = f["pos"], f["r"]
                d.ellipse([T((p[0] - r, p[1] - r)), T((p[0] + r, p[1] + r))], fill=(96, 140, 160))
            for b in plan["benches"]:
                p = b["pos"]
                d.rectangle([T((p[0] - .8, p[1] - .8)), T((p[0] + .8, p[1] + .8))], fill=(110, 80, 56))
        for u in L["units"]:
            if u["kind"] != "anchor":
                d.polygon([T(p) for p in corners(u)], fill=kinds[u["kind"]], outline=(60, 50, 44))
            for p in u.get("stand", []) if L["level"] == 0 else []:
                d.ellipse([T((p[0] - .7, p[1] - .7)), T((p[0] + .7, p[1] + .7))], fill=(60, 90, 200))
        for f in L["fronts"]:
            d.line([T(f["a"]), T(f["b"])], fill=(255, 190, 90), width=3)
        for w in L["walls"]:
            d.line([T(p) for p in w["line"]], fill=(168, 132, 64) if w["kind"] == "drum" else (40, 36, 36), width=3)
        for rl in L["rails"]:
            d.line([T(p) for p in rl], fill=(250, 250, 250), width=2)
        for q in plan["lifts"]:
            p, r = q["pos"], q["r"]
            d.ellipse([T((p[0] - r, p[1] - r)), T((p[0] + r, p[1] + r))], fill=(236, 226, 200), outline=(60, 50, 44))
        for r in plan["ramps"]:
            if r["from"] == L["level"] or r["to"] == L["level"]:
                d.line([T(r["a"]), T(r["b"])], fill=(200, 60, 60) if r["from"] == L["level"] else (240, 150, 150), width=int(r["width"] * S))
        if L["level"] == 0:
            for e in plan["entrances"]:
                p, h = e["at"], e["width"] / 2
                q = ((p[0] - h, p[1]), (p[0] + h, p[1])) if e["side"] in "ns" else ((p[0], p[1] - h), (p[0], p[1] + h))
                d.line([T(q[0]), T(q[1])], fill=(90, 220, 120), width=6)
        im.save(os.path.join(folder, "plan_floor_%d.png" % L["level"]))


def run(draw_too=False):
    path = os.path.join(BLOCKS, BID + ".json")
    with open(path, encoding="utf-8") as f:
        b = json.load(f)
    lot = next(l for l in b["lots"] if "mall" in l.get("tags", []) and l.get("shell"))
    plan, places = design(lot, lot["shell"])
    lot["plan"] = plan
    lot["objects"].update(places)                                    # `work` is add_careers_01.py's to write
    lot["customers"], lot["seats"] = len(places["customer"]), len(places["seat"])
    with open(path, "w", encoding="utf-8") as f:
        json.dump(b, f, ensure_ascii=False, indent=1)
    if draw_too:
        draw(lot, plan, os.path.normpath(os.path.join(HERE, "..", "..", "Art", "StyleTests", "mall_inside_26")))
    out = []
    for L in plan["levels"]:
        kinds = {}
        for u in L["units"]:
            kinds[u["kind"]] = kinds.get(u["kind"], 0) + 1
        out.append("floor %d: %s, %d fronts, %d walls, %d rails" % (L["level"], kinds, len(L["fronts"]), len(L["walls"]), len(L["rails"])))
    return "\n".join(out)


if __name__ == "__main__":
    print(run("--draw" in sys.argv))
