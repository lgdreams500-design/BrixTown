"""Light city_01's dark pavements (WORLD_MAP WLD-072; the owner, 2026-11-26, shown that about 8 % of the walled town's
pavement was more than 40 m from a street lamp: "Yes, add lamps").

make_city_01.py gave each block of the street grid three lamps: at its north-west corner and halfway along its north and
west sides. A side with no block beyond it (the east sides on the old wall's line, Civic Row's south side) and blocks
built again later were left with pavements far from any lamp, black at night (Art/StyleTests/street_lamps_47/).

This walks every pavement of the street grid (the blocks with lots wholly in grid columns 6-13, rows 4-12: Old Town and Civic Row)
at the kerb, 4 m out from the block's edge where the first lamps stand, and adds a lamp wherever a point of it is more
than `reach_m` from every lamp, as near that point as it can stand: clear of doors, of the other street things and trees,
of the zebras and of the bus stops. The lamps are marked "set": "lamps", so running this again first takes away what it
added before and puts the same lamps back (running it twice changes nothing). Numbers [ASSUMPTION].

Run from Tools/worldgen:   py add_lamps_01.py
Then:                      py -m unittest test_world ;  py build_play.py
"""
import json, math, os, glob
import world

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")
SET = "lamps"                  # the numbers are World/rules/dressing.json "street_lamps"


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def main():
    city = jload(os.path.join(CITY, "city.json"))
    traffic = jload(os.path.join(RULES, "traffic.json"))
    R = jload(os.path.join(RULES, "dressing.json"))["street_lamps"]
    REACH, CLEAR = R["reach_m"], R["clear_m"]
    P, c = city["grid"]["pitch"], traffic["crossing_at"]
    stops = [tuple(s["at"]) for s in jload(os.path.join(CITY, "transit.json"))["stops"]]
    paths = sorted(glob.glob(os.path.join(CITY, "blocks", "*.json")))
    blocks = [(p, jload(p)) for p in paths]
    for _, b in blocks:                                   # what this added before goes first
        b["props"] = [p for p in b["props"] if p.get("set") != SET]
    lamps = [tuple(p["pos"]) for _, b in blocks for p in b["props"] if p["kind"] == "lamp"]
    things = [(tuple(p["pos"]), CLEAR["thing"]) for _, b in blocks for p in b["props"]
              if "pos" in p and p["kind"] not in ("lamp", "lantern", "arch", "advert", "hoarding", "paving", "carpark")]
    things += [(tuple(t["pos"]), CLEAR["tree"]) for _, b in blocks for t in b["trees"]]
    doors = [(tuple(l["door"]), CLEAR["tram"] if l["kind"] == "stop" else CLEAR["door"]) for _, b in blocks for l in b["lots"]]

    def ok(x, z):
        if any(math.hypot(x - a, z - q) < r for (a, q), r in things + doors):
            return False
        if any(math.hypot(x - a, z - q) < CLEAR["stop"] for a, q in stops):
            return False
        for v in (x, z):                                  # a zebra crosses each street `crossing_at` from every junction
            g = round(v / P) * P
            if any(abs(v - (g + s * c)) < CLEAR["zebra"] for s in (-1, 1)):
                return False
        return all(math.hypot(x - a, z - q) > 1.0 for a, q in lamps)

    def far(x, z):
        return min((math.hypot(x - a, z - q) for a, q in lamps), default=1e9)

    w = dict(city=city, dressing=dict(street_lamps=R))
    by_id = {b["id"]: b for _, b in blocks}
    sides = {}
    for bid, side, pt in world.lit_pavements(w, [b for _, b in blocks]):
        sides.setdefault((bid, side), []).append(pt)
    added = 0
    for (bid, side), pts in sides.items():
        b = by_id[bid]
        while True:
            worst = max(range(len(pts)), key=lambda q: far(*pts[q]))
            if far(*pts[worst]) <= REACH:
                break
            # the nearest point of the same pavement where a lamp can stand (the points are step_m apart)
            spot = next((pts[q] for d in range(0, 7) for q in (worst - d, worst + d) if 0 <= q < len(pts) and ok(*pts[q])), None)
            if spot is None:                              # nowhere to stand near it: leave that point dark
                pts.pop(worst)
                if not pts:
                    break
                continue
            x, z = spot
            n = sum(1 for q in b["props"] if q.get("set") == SET)
            b["props"].append(dict(kind="lamp", pos=[round(x, 2), round(z, 2)], set=SET, id="%s_l%d" % (b["id"], n)))
            lamps.append((x, z))
            things.append(((x, z), CLEAR["thing"]))
            added += 1
    for p, b in blocks:
        with open(p, "w", encoding="utf-8") as f:
            json.dump(b, f, ensure_ascii=False, indent=1)
    print("added %d street lamps (set %r): every pavement of the street grid within %.0f m of one" % (added, SET, REACH))


if __name__ == "__main__":
    main()
