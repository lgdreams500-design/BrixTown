"""Writes World/city_01/roads.json: the road network of the starting city as data (CITY-043).

Streets run along the grid lines of city.json. Each street between two junctions is one segment with a lane
each way. Junctions get a control: signals where two through streets cross, priority for the through street
where one does, give way (first come) elsewhere, none at a bend. Parking bays are lay-bys cut into the
pavement, clear of junctions, lamps and tram stops. Crossings sit on every arm of every junction.

The file is data: once written it can be edited by hand (move a bay, change a control). Re-running this
script rewrites it from city.json and the block files.     python make_roads_01.py
"""
import json, math, os

import world

HERE = os.path.dirname(os.path.abspath(__file__))


def build(w):
    g, rules = w["city"]["grid"], w["traffic"]
    P, cols, rows = g["pitch"], g["columns"], g["rows"]
    names = w["city"]["streets"]
    through = set(rules["through_streets"])
    jid = lambda i, j: "j_%d_%d" % (i, j)

    streets = []
    for j in range(rows + 1):                                  # east-west streets along z = j * P
        for i in range(cols):
            n = names["east_west"][j]
            streets.append(dict(id="s_ew_%d_%d" % (i, j), name=n, a=jid(i, j), b=jid(i + 1, j),
                                limit_kmh=rules["limits_kmh"]["through" if n in through else "default"]))
    for i in range(cols + 1):                                  # north-south streets along x = i * P
        for j in range(rows):
            n = names["north_south"][i]
            streets.append(dict(id="s_ns_%d_%d" % (i, j), name=n, a=jid(i, j), b=jid(i, j + 1),
                                limit_kmh=rules["limits_kmh"]["through" if n in through else "default"]))

    junctions = []
    for i in range(cols + 1):
        for j in range(rows + 1):
            ns_arms = (j > 0) + (j < rows)
            ew_arms = (i > 0) + (i < cols)
            ns_through, ew_through = names["north_south"][i] in through, names["east_west"][j] in through
            if ns_arms + ew_arms <= 2 and not (ns_arms == 2 or ew_arms == 2):
                control, main = "none", None                   # a corner of the town wall
            elif ns_through and ew_through:
                control, main = "signals", None
            elif ns_through or ew_through:
                control, main = "priority", "ns" if ns_through else "ew"
            else:
                control, main = "give_way", None
            junctions.append(dict(id=jid(i, j), pos=[i * P, j * P], control=control, main=main))

    # what bays must keep clear of: lamps in the street, tram stops and their shelters
    lamps = [tuple(p["pos"]) for b in w["blocks"] for p in b["props"] if p["kind"] == "lamp"]
    shelters = [tuple(p["pos"]) for b in w["blocks"] for p in b["props"] if p["kind"] == "shelter"]
    stops = [l["rect"] for l in world.lots(w) if l["kind"] == "stop"]
    jpos = {j["id"]: j["pos"] for j in junctions}

    bays, crossings = [], []
    L, clear, pitch = rules["bay_length"], rules["bay_clear_of_junction"], rules["bay_length"] + 0.2
    for s in streets:
        a, b = jpos[s["a"]], jpos[s["b"]]
        length = math.hypot(b[0] - a[0], b[1] - a[1])
        u = ((b[0] - a[0]) / length, (b[1] - a[1]) / length)
        right = (-u[1], u[0])                                  # x east, z south: right of heading east is south
        for side in (1, -1):
            nx, nz = right[0] * side, right[1] * side
            mid = ((a[0] + b[0]) / 2 + nx * 20, (a[1] + b[1]) / 2 + nz * 20)
            if not (0 < mid[0] < cols * P and 0 < mid[1] < rows * P):
                continue                                       # the outer side of a wall street faces the fields
            k, d = 0, clear + L / 2
            while d <= length - clear - L / 2 + 1e-6:
                c = (a[0] + u[0] * d + nx * rules["bay_offset"], a[1] + u[1] * d + nz * rules["bay_offset"])
                near_lamp = any(abs((p[0] - c[0]) * u[0] + (p[1] - c[1]) * u[1]) < L / 2 + rules["bay_clear_of_lamp"] - 1.0 and
                                abs((p[0] - c[0]) * nx + (p[1] - c[1]) * nz) < 3.0 for p in lamps)
                near_stop = any(r[0] - 4 <= c[0] <= r[2] + 4 and r[1] - 4 <= c[1] <= r[3] + 4 for r in stops) or \
                    any(math.hypot(p[0] - c[0], p[1] - c[1]) < 6.0 for p in shelters)
                if not near_lamp and not near_stop:
                    bays.append(dict(id="bay_%s_%s%d" % (s["id"][2:], "r" if side > 0 else "l", k), street=s["id"], side=side,
                                     at=round(d, 2), pos=[round(c[0], 2), round(c[1], 2)]))
                    k += 1
                d += pitch
        for end, other in ((s["a"], b), (s["b"], a)):          # a crossing on this arm of each end junction
            p = jpos[end]
            v = ((other[0] - p[0]) / length, (other[1] - p[1]) / length)
            crossings.append(dict(id="x_%s_%s" % (s["id"][2:], "a" if end == s["a"] else "b"), street=s["id"], junction=end,
                                  pos=[round(p[0] + v[0] * rules["crossing_at"], 2), round(p[1] + v[1] * rules["crossing_at"], 2)]))
    # town gates: where a through street meets the town wall, a road leads out of town (CITY-068)
    gates = []
    for i in range(cols + 1):
        for j in range(rows + 1):
            out = []
            if names["north_south"][i] in through and j in (0, rows):
                out.append(((0, -1) if j == 0 else (0, 1), "North Gate" if j == 0 else "South Gate"))
            if names["east_west"][j] in through and i in (0, cols):
                out.append(((-1, 0) if i == 0 else (1, 0), "West Gate" if i == 0 else "East Gate"))
            for d, name in out:
                gid = "gate_" + name.split()[0].lower()
                gates.append(dict(id=gid, name=name, junction=jid(i, j), dir=list(d), length=rules["gate_road_m"],
                                  limit_kmh=rules["limits_kmh"]["through"]))
                p = jpos[jid(i, j)]
                crossings.append(dict(id="x_%s" % gid, street=gid, junction=jid(i, j),
                                      pos=[p[0] + d[0] * rules["crossing_at"], p[1] + d[1] * rules["crossing_at"]]))
    return dict(schema=1, city=w["city"]["id"], gates=gates,
                note="Road network of %s (CITY-043). Generated by Tools/worldgen/make_roads_01.py; may be edited by hand. "
                     "Bays: 'side' 1 is the right of travel from a to b. 'at' is metres from junction a." % w["city"]["name"],
                junctions=junctions, streets=streets, bays=bays, crossings=crossings)


def main():
    w = world.load()
    roads = build(w)
    out = os.path.join(world.WORLD, w["city"]["id"], "roads.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(roads, f, indent=1)
        f.write("\n")
    controls = {}
    for j in roads["junctions"]:
        controls[j["control"]] = controls.get(j["control"], 0) + 1
    print("wrote %s: %d junctions %s, %d streets, %d bays, %d crossings, gates %s" % (
        os.path.relpath(out, os.path.join(HERE, "..", "..")), len(roads["junctions"]), controls, len(roads["streets"]), len(roads["bays"]), len(roads["crossings"]), [g["name"] for g in roads["gates"]]))


if __name__ == "__main__":
    main()
