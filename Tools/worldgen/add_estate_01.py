"""Block-file changes for the real estate market in city_01 (PROP-022, PROP-023; F-PROP-ESTATE level 2, ADR-0022).

1. The three real estate companies' offices take their company's name (World/rules/estate.json "companies"), so the
   name over the door, on the map and in the place line is the agent's.
2. Every flat that has no bed yet (the rooms over shops and cafes, tower flats nobody lives in) gets beds and living
   space on its own floor, so a household that moves in has somewhere to sleep and sit. Places keep clear of the
   stairwell, the lift and every other place in the building, as world.storey_problems() asks.

Run from Tools/worldgen:   py add_estate_01.py        (running it again changes nothing: furnished flats are skipped)
Then:                      py -m unittest test_world ;  py build_play.py
"""
import json, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")
BEDS, ROOMS = 3, 2                       # a flat sleeps three [ASSUMPTION]
INSET, STEP, CLEAR = 1.5, 2.2, 0.8       # metres: from the walls, between candidate spots, from any other place


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def flat_floors(l, R):
    """The floors of lot l that the estate rules make flats of (estate.carve)."""
    rule = R["units"].get(l["kind"], R["units"]["default"])
    st = l.get("storeys") or []
    use = rule.get("flat_use")
    return [f for f in range(1, len(st)) if use and st[f] == use]


def to_rect(p, r):
    return math.hypot(max(r[0] - p[0], 0, p[0] - r[2]), max(r[1] - p[1], 0, p[1] - r[3]))


def furnish(l, f):
    x0, z0, x1, z1 = l["rect"]
    taken = [tuple(p) for v in l.get("objects", {}).values() for p in v]
    stair = l.get("stair", {}).get("rect")
    lift = l.get("lift")
    door = l["door"]
    cands = []
    x = x0 + INSET
    while x <= x1 - INSET + 1e-9:
        z = z0 + INSET
        while z <= z1 - INSET + 1e-9:
            p = (round(x, 2), round(z, 2))
            if (not stair or to_rect(p, stair) >= 1.6) and (not lift or math.dist(p, lift) >= 2.5) and \
                    all(math.dist(p, q) >= CLEAR for q in taken) and math.dist(p, door) >= 2.5:
                cands.append(p)
            z += STEP
        x += STEP
    cands.sort(key=lambda p: (-math.dist(p, door), p))           # beds at the back, away from the way in
    beds = []
    for p in cands:
        if len(beds) < BEDS and all(math.dist(p, q) >= 2.4 for q in beds):
            beds.append(p)
    c = ((x0 + x1) / 2, (z0 + z1) / 2)
    rest = sorted((p for p in cands if p not in beds), key=lambda p: (math.dist(p, c), p))
    rooms = []
    for p in rest:
        if len(rooms) < ROOMS and all(math.dist(p, q) >= 1.8 for q in rooms + beds):
            rooms.append(p)
    return [list(p) for p in beds], [list(p) for p in rooms]


def main():
    R = jload(os.path.join(RULES, "estate.json"))
    names = {c["office"]: c["name"] for c in R["companies"]}
    city = jload(os.path.join(CITY, "city.json"))
    renamed = flats = 0
    for bref in city["blocks"]:
        path = os.path.join(CITY, bref["file"])
        b = jload(path)
        changed = False
        for l in b["lots"]:
            if l["id"] in names and l["name"] != names[l["id"]]:
                l["name"] = names[l["id"]]
                renamed += 1
                changed = True
            for f in flat_floors(l, R):
                lv = l.setdefault("levels", {})
                obj = l.setdefault("objects", {})
                if f in lv.get("bed", []):
                    continue
                beds, rooms = furnish(l, f)
                if len(beds) < 1:
                    continue
                for kind, new in (("bed", beds), ("room", rooms)):
                    have = obj.get(kind, [])
                    lv[kind] = lv.get(kind, [0] * len(have)) + [f] * len(new)
                    obj[kind] = have + new
                flats += 1
                changed = True
        if changed:
            with open(path, "w", encoding="utf-8") as fh:
                json.dump(b, fh, ensure_ascii=False, indent=1)
    print("%d agent offices named; %d empty flats given beds and living space" % (renamed, flats))


if __name__ == "__main__":
    main()
