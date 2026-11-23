"""Every designed house gets the approved plan and a colourway (ADR-0032, ADR-0035; FURN-060, BUILD-001).

For each house of plan "samples_19" in the communities:
  - house.style: its colourway, "a", "b" or "c" of its line (World/rules/house_styles.json), by a hash of the lot; picked
    once here and kept in the lot, so loading the game never changes it (the four houses on Northwood Way the owner
    reviewed keep "a", the line as it was drawn);
  - house.rooms: the rooms of plan "house_12x9" (World/rules/home_plans.json), in the house's own frame;
  - house.interior = "house_12x9": the plan Prototype/play/houses.js furnishes it from;
  - objects.bed / objects.room: where its residents lie and sit, on the beds, sofa and chairs of the plan (the spots of
    home_plans.json), as many as it has residents (at most five), placed as houses.js places the pieces.
Its line, trim and garden stay as add_houses_01.py and add_houses_03.py made them. Which pieces stand in a house follows
its household (FURN-063, Prototype/play/furnishing.js); a bed for each resident is always there.
Run from Tools/worldgen:   python add_houses_04.py      (running it again writes the same)
"""
import json, math, os, hashlib

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")
PLAN = json.load(open(os.path.join(RULES, "home_plans.json"), encoding="utf-8"))["plans"]["house_12x9"]
STYLES = json.load(open(os.path.join(RULES, "house_styles.json"), encoding="utf-8"))
REVIEWED = {"b_0_0_l7", "b_0_0_l9", "b_0_0_l12", "b_0_0_l13"}


def h(*k):
    return int(hashlib.md5("|".join(map(str, k)).encode()).hexdigest()[:8], 16) / 0xFFFFFFFF


def mapA(x, o, n):                                    # as houses.js: a point keeps its distance to the nearer wall of its room
    if x <= o[0]:
        return n[0] + x - o[0]
    for i in range(len(o) - 1):
        if x <= o[i + 1]:
            return n[i] + x - o[i] if x - o[i] <= o[i + 1] - x else n[i + 1] - (o[i + 1] - x)
    return n[-1] + x - o[-1]


# the plan's walls (inside faces of the outer walls, partition lines) to the slice's house (houses.js: 0.32 m outer walls)
HP = PLAN["house"]
PI0, PJ0 = HP["W1"] - HP["T"], HP["D1"] - HP["T"]
SI, SJ, SA, SB, SM = 5.68, 4.18, HP["XA"], HP["XB"], HP["YM"]
mu = lambda u: round(mapA(u, [-PI0, HP["XA"], HP["XB"], PI0], [-SI, SA, SB, SI]), 3)
mv = lambda v: round(mapA(v, [-PJ0, HP["YM"], PJ0], [-SJ, SM, SJ]), 3)


def spots(kinds):
    """[(u, v, height)] in the slice's house frame for every spot on the plan's pieces of these kinds, in plan order."""
    out = []
    for it in PLAN["items"]:
        if it["piece"] not in kinds:
            continue
        c, s = math.cos(it["yaw"]), math.sin(it["yaw"])
        for x, y, z, fx, fy in HP["spots"][it["piece"]]:
            out.append((mu(it["x"] + x * c - y * s), mv(it["y"] + x * s + y * c), z))
    return out


BEDS = spots(HP["beds"])
SEATS = [p for k in ("k17:sofa", "k17:dining", "k17:armchair") for p in spots([k])]   # sofa, the kitchen table (first in the plan), armchair


def frame(l):
    x0, z0, x1, z1 = l["rect"]
    cx, cz, t = (x0 + x1) / 2, (z0 + z1) / 2, math.radians(l.get("angle") or 0)
    c, s = math.cos(t), math.sin(t)
    return lambda u, v: (cx - u * c - v * s, cz - u * s + v * c)


def rooms():
    out = []
    for storey, f in (("ground", 0), ("upstairs", 1)):
        for name, (x0, x1, y0, y1) in HP["rooms"][storey].items():
            out.append(dict(name=name, floor=f, rect=[round(mu(x0), 3), round(mv(y0), 3), round(mu(x1), 3), round(mv(y1), 3)]))
    return out


def main():
    city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
    n, by = 0, {}
    R = rooms()
    for e in city["blocks"]:
        if e["template"] != "community":
            continue
        p = os.path.join(CITY, e["file"])
        b = json.load(open(p, encoding="utf-8"))
        for l in b["lots"]:
            hs = l.get("house")
            if not hs or hs.get("plan") != "samples_19":
                continue
            ways = sorted(STYLES["colourways"][hs["furniture"]])
            style = "a" if l["id"] in REVIEWED else ways[int(h(l["id"], "style") * len(ways)) % len(ways)]
            k = min(5, len(l["objects"].get("bed", [])) or l.get("residents", 2))
            to_w = frame(l)
            hs.update(style=style, interior="house_12x9", rooms=R)
            l["objects"]["bed"] = [[round(c, 2) for c in to_w(u, v)] for u, v, z in BEDS[:k]]
            l["objects"]["room"] = [[round(c, 2) for c in to_w(u, v)] for u, v, z in SEATS[:k]]
            l["levels"] = dict(bed=[1] * k, room=[0] * k)
            n += 1
            by[(hs["furniture"], style)] = by.get((hs["furniture"], style), 0) + 1
        with open(p, encoding="utf-8") as f:
            end = "\n" if f.read().endswith("\n") else ""
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            json.dump(b, f, indent=1, ensure_ascii=False)
            f.write(end)
    print("%d houses on plan house_12x9: %s" % (n, ", ".join("%s %s %d" % (a, s, c) for (a, s), c in sorted(by.items()))))


if __name__ == "__main__":
    main()
