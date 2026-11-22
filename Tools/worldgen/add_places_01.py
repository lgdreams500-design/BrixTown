"""Add the town's bank, car showroom, library, museum, cinema, a second hospital and a second mall to city_01
(WORLD_MAP WLD-027, the owner's request of 2026-11-22: places to visit, made from existing lots).

Each chosen lot keeps its footprint and door, so streets, paths and World/city_01/roads.json are untouched; it gets a
new kind, name and set of places laid out from its door. The people who worked there keep their jobs in the new place
under a fitting role, and every resident learns their nearest bank, library, museum, cinema and showroom, so the
choice lists in World/rules/choices.json can send them there.

Run from Tools/worldgen:   python add_places_01.py      (running it again changes nothing)
Then:                      python -m unittest test_world ;  python build_play.py
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")

# lot id -> what it becomes. "was" guards against converting the wrong lot if the city is ever regenerated.
PLACES = {
    "b_3_2_l0": dict(was="Linden Works", kind="bank", name="Linden Bank", role="clerk", tags=["bank"]),
    "b_2_3_l0": dict(was="Fountain Works", kind="library", name="Fountain Library", role="clerk", tags=["library"]),
    "b_3_5_l2": dict(was="Quarry Works", kind="museum", name="Old Town Museum", role="clerk", tags=["museum"]),
    "b_2_5_l0": dict(was="Tram Works", kind="cinema", name="Palace Cinema", role="cafe", tags=["cinema"]),
    "b_6_3_l2": dict(was="Baker Works", kind="dealer", name="Baker Street Motors", role="shopkeeper", tags=["dealer"]),
    "b_6_3_l0": dict(was="Fountain Clinic", kind="health", name="Fountain Hospital", role="nurse", tags=["hospital"]),
    "b_6_4_l0": dict(was="Baker Grocer", kind="shop", name="Baker Arcade", role="shopkeeper", tags=["mall", "cafe"]),
}
TOKENS = ("bank", "library", "museum", "cinema", "dealer")      # new things a choice list may name
CHOICES = {                                                       # added to World/rules/choices.json (kept if present)
    "leisure": {"default": [["read", "library"], ["view", "museum"], ["watch", "cinema"]],
                "rain": [["watch", "cinema"], ["read", "library"], ["view", "museum"]]},
    "afternoon": {"default": [["bank", "bank"], ["read", "library"]]},
    "shopping": {"default": [["browse", "dealer"], ["bank", "bank"]]},
}
# A choice list is picked from evenly, and there is one of each new place for the whole town. So the old options are
# repeated (keeping their own proportions exactly) and each new one is a small share: about 1 in 20 of leisure.
REPEAT = {("leisure", "default"): 4, ("leisure", "rain"): 3, ("afternoon", "default"): 1, ("shopping", "default"): 5}
USE = {"read": "seat", "view": "customer", "watch": "seat", "bank": "customer", "browse": "customer"}


def frame(lot):
    """Door coordinates for a lot: a along the door wall, i inward. Returns (to_world, width, depth)."""
    x0, z0, x1, z1 = lot["rect"]
    dx, dz = lot["door"]
    side = "n" if abs(dz - z0) < .01 else "s" if abs(dz - z1) < .01 else "w" if abs(dx - x0) < .01 else "e"
    o = {"n": (0, -1), "s": (0, 1), "w": (-1, 0), "e": (1, 0)}[side]
    width, depth = (x1 - x0, z1 - z0) if side in "ns" else (z1 - z0, x1 - x0)

    def to_world(a, i):
        x, z = dx - o[0] * i - o[1] * a, dz - o[1] * i + o[0] * a
        return [round(min(max(x, x0 + .9), x1 - .9), 1), round(min(max(z, z0 + .9), z1 - .9), 1)]
    return to_world, width, depth


def spread(n, half, skip=0.0):
    """n positions across [-half, half], leaving the middle clear by `skip` (the way in from the door)."""
    if n <= 0:
        return []
    if skip <= 0:
        return [-half + (k + .5) * 2 * half / n for k in range(n)]
    left, right = n // 2, n - n // 2
    return [-half + (k + .5) * (half - skip) / max(left, 1) for k in range(left)] + \
           [skip + (k + .5) * (half - skip) / max(right, 1) for k in range(right)]


def places(kind, lot, staff):
    """Where people work, sit and stand in each kind of place (the slice furnishes round these)."""
    P, W, D = frame(lot)
    h = W / 2 - 2.0
    rows = lambda n, i, skip=0.0, half=h: [P(a, i) for a in spread(n, half, skip)]
    if kind == "bank":                      # tellers along the back counter, customers at it and queuing behind them
        return dict(work=rows(staff, D - 2.2), customer=rows(6, D - 4.6, 1.5) + rows(4, D - 7.4, 1.5, h * .6))
    if kind == "library":                   # librarians at the back desk, readers at tables, browsers at the stacks
        return dict(work=rows(staff, D - 2.0, 0, 4.0), seat=rows(6, 4.4, 2.0) + rows(6, 7.6, 2.0), customer=rows(6, D - 4.8, 2.5))
    if kind == "museum":                    # attendants at the back, visitors before the exhibits, a few benches
        return dict(work=rows(staff, D - 2.0), customer=rows(6, 4.6, 2.5) + rows(6, 9.4, 2.5), seat=rows(4, 7.0, 2.0, h * .5))
    if kind == "cinema":                    # ticket and sweets counters by the door, rows of seats facing the screen
        return dict(work=rows(staff, 2.2, 4.5), seat=[p for i in (5.6, 7.2, 8.8, 10.4) for p in rows(8, i, 1.3, h - 1.0)])
    if kind == "dealer":                    # salesmen at desks at the back, buyers round the cars on the floor
        return dict(work=rows(staff, D - 2.2), customer=rows(6, 6.0, 2.0), seat=rows(2, D - 4.2, 3.0, 6.0))
    if kind == "health":                    # a hospital: nurses' stations at the back, rows of waiting chairs
        return dict(work=rows(max(staff, 4), D - 2.2), chair=rows(6, 4.6, 1.8) + rows(6, 7.4, 1.8))
    if kind == "shop":                      # an arcade: traders at booths along the back, shoppers, a court of tables
        return dict(work=rows(max(staff, 4), D - 2.4), customer=rows(8, D - 5.4, 1.8), seat=rows(6, 4.8, 2.0, h * .7))
    raise ValueError(kind)


def main():
    city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
    res_doc = json.load(open(os.path.join(CITY, "residents.json"), encoding="utf-8"))
    people = res_doc["residents"]
    changed = []
    blocks = {}
    for entry in city["blocks"]:
        path = os.path.join(CITY, entry["file"])
        blocks[entry["id"]] = (path, json.load(open(path, encoding="utf-8")))
    for lot_id, spec in PLACES.items():
        path, b = blocks[lot_id.rsplit("_l", 1)[0]]
        lot = next(l for l in b["lots"] if l["id"] == lot_id)
        if lot["name"] == spec["name"]:
            continue                                       # already done
        if lot["name"] != spec["was"]:
            raise SystemExit("%s is '%s', not '%s': the city has changed; choose the lots again" % (lot_id, lot["name"], spec["was"]))
        staff = [r for r in people if r.get("work") == lot_id]
        lot.update(kind=spec["kind"], name=spec["name"], tags=spec["tags"], enterable=True)
        lot["objects"] = places(spec["kind"], lot, len(staff))
        for r in staff:
            r["role"] = spec["role"]
        changed.append(path)
        print("%s: %s -> %s (%s), %d staff" % (lot_id, spec["was"], spec["name"], spec["kind"], len(staff)))
    for path in set(changed):
        b = next(bb for p, bb in blocks.values() if p == path)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(b, f, ensure_ascii=False, indent=1)
    # every resident's nearest one of each new kind (by walking distance from home is not needed: one of each)
    lots = {l["id"]: l for _, b in blocks.values() for l in b["lots"]}
    by_tag = {t: [l for l in lots.values() if t in l.get("tags", [])] for t in TOKENS}
    ctr = lambda l: ((l["rect"][0] + l["rect"][2]) / 2, (l["rect"][1] + l["rect"][3]) / 2)
    for r in people:
        hx, hz = ctr(lots[r["home"]])
        for t, ls in by_tag.items():
            if ls:
                r.setdefault("near", {})[t] = min(ls, key=lambda l: ((ctr(l)[0] - hx) ** 2 + (ctr(l)[1] - hz) ** 2, l["id"]))["id"]
    with open(os.path.join(CITY, "residents.json"), "w", encoding="utf-8") as f:
        json.dump(res_doc, f, ensure_ascii=False, indent=0)
    # the choice lists and what each new activity uses
    cpath = os.path.join(RULES, "choices.json")
    ch = json.load(open(cpath, encoding="utf-8"))
    for name, cases in CHOICES.items():
        for case, options in cases.items():
            lst = ch["choices"].setdefault(name, {}).setdefault(case, [])
            if any(opt in lst for opt in options):
                continue                                   # already done
            ch["choices"][name][case] = lst * REPEAT.get((name, case), 1) + options
    ch["use"].update(USE)
    ch["note"] = ch["note"].rstrip(".") + ". 'bank', 'library', 'museum', 'cinema', 'dealer' likewise (Tools/worldgen/add_places_01.py)."
    with open(cpath, "w", encoding="utf-8") as f:
        json.dump(ch, f, ensure_ascii=False, indent=1)
    print("residents: nearest %s; choices: %s" % (", ".join(TOKENS), ", ".join(CHOICES)))


if __name__ == "__main__":
    main()
