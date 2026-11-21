"""Loads a city from World/ and checks it. Nothing here generates anything: it only reads the data files.

    import world
    w = world.load()                 # the whole city as one dict
    problems = world.validate(w)     # [] when the data is sound
    city = world.simulation(w)       # a citysim.City running the living-city rules over the whole map
    city = world.simulation(w, traffic=True)   # the same, with residents' cars on the roads (citysim/traffic.py)
"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
WORLD = os.path.normpath(os.path.join(HERE, "..", "..", "World"))
sys.path.insert(0, os.path.join(HERE, "..", "citysim"))

OPEN_KINDS = ("garden", "market", "stop")            # places without walls


def _read(*parts):
    with open(os.path.join(*parts), encoding="utf-8") as f:
        return json.load(f)


def load(city="city_01"):
    base = os.path.join(WORLD, city)
    c = _read(base, "city.json")
    rules = os.path.normpath(os.path.join(base, c["rules"]))
    w = dict(city=c, blocks=[_read(base, b["file"]) for b in c["blocks"]], residents=_read(base, "residents.json")["residents"])
    for name in ("roles", "choices", "calendar", "vegetation", "signatures", "traffic", "dialogue"):
        w[name] = _read(rules, name + ".json")
    roads = os.path.join(base, "roads.json")           # the road network (CITY-043); written by make_roads_01.py
    w["roads"] = _read(roads) if os.path.exists(roads) else None
    return w


def lots(w):
    return [l for b in w["blocks"] for l in b["lots"]]


def validate(w):
    """Every rule a block file must keep. Returns a list of problems in plain words."""
    out = []
    g = w["city"]["grid"]
    seen, ids = {}, set()
    for b in w["blocks"]:
        i, j = b["grid"]
        if (i, j) in seen:
            out.append("two blocks at %s: %s and %s" % ((i, j), seen[(i, j)], b["id"]))
        seen[(i, j)] = b["id"]
        x0, z0 = i * g["pitch"] + g["street_width"] / 2, j * g["pitch"] + g["street_width"] / 2
        if [x0, z0] != b["origin"]:
            out.append("%s: origin %s does not match its grid cell" % (b["id"], b["origin"]))
        pave = g["street_width"] / 2
        walls = [l for l in b["lots"] if l["kind"] not in OPEN_KINDS]
        for l in b["lots"]:
            if l["id"] in ids:
                out.append("lot id used twice: " + l["id"])
            ids.add(l["id"])
            a0, c0, a1, c1 = l["rect"]
            if not (a0 < a1 and c0 < c1):
                out.append("%s: empty footprint" % l["id"])
            if a0 < x0 - pave or c0 < z0 - pave or a1 > x0 + b["size"] + pave or c1 > z0 + b["size"] + pave:
                out.append("%s: footprint leaves its block" % l["id"])
            if l["kind"] not in OPEN_KINDS:
                dx, dz = l["door"]
                on_edge = (abs(dx - a0) < 0.01 or abs(dx - a1) < 0.01) and c0 <= dz <= c1 or (abs(dz - c0) < 0.01 or abs(dz - c1) < 0.01) and a0 <= dx <= a1
                if not on_edge:
                    out.append("%s: door is not on a wall" % l["id"])
                for kind, pts in l["objects"].items():
                    for p in pts:
                        if not (a0 + 0.5 <= p[0] <= a1 - 0.5 and c0 + 0.5 <= p[1] <= c1 - 0.5):
                            out.append("%s: a %s place is outside the building" % (l["id"], kind))
        for n, a in enumerate(walls):
            for c in walls[n + 1:]:
                if a["rect"][0] < c["rect"][2] - 0.01 and c["rect"][0] < a["rect"][2] - 0.01 and a["rect"][1] < c["rect"][3] - 0.01 and c["rect"][1] < a["rect"][3] - 0.01:
                    out.append("%s and %s overlap" % (a["id"], c["id"]))
    for r in w["residents"]:
        for key in ["home", "work"] + ["near." + k for k in r.get("near", {})]:
            v = r["near"][key[5:]] if key.startswith("near.") else r.get(key)
            if v is not None and v not in ids:
                out.append("%s: %s points at a lot that does not exist (%s)" % (r["id"], key, v))
        if r["role"] not in w["roles"]["roles"]:
            out.append("%s: unknown role %s" % (r["id"], r["role"]))
    if not out:                                        # every door must be reachable on foot from every other
        c = simulation(w)
        dd, _ = c.net._from(next(iter(c.anchor.values())))
        out += ["%s cannot be reached on foot" % k for k, a in c.anchor.items() if a not in dd]
    if w.get("roads") and not out:
        import traffic
        out += traffic.Roads(w["roads"], w["traffic"]).problems(lots(w))
    out += dialogue_problems(w)
    kinds = {l["kind"] for l in lots(w)}
    for need in ("home", "shop", "food", "work", "health", "garden", "market", "stop", "utility"):
        if need not in kinds:
            out.append("the city has no %s" % need)
    return out


def dialogue_problems(w, acts=None):
    """Every rule World/rules/dialogue.json must keep (CITY-007). `acts`: the activity names a rule may test, when known."""
    import re
    d, out = w["dialogue"], []
    convs, roles, moods, names = d["conversations"], w["roles"]["roles"], set(d["moods"]), set(d["vars"])
    var = re.compile(r"\{([^{}]*)\}")
    for role in roles:
        if role not in d["role_words"]:
            out.append("dialogue: no word for role %s" % role)
    for n, r in enumerate(d["rules"]):
        if r["conversation"] not in convs:
            out.append("dialogue rule %d: no conversation %s" % (n, r["conversation"]))
        for role in r.get("roles", []):
            if role not in roles:
                out.append("dialogue rule %d: unknown role %s" % (n, role))
        for act in r.get("acts", []):
            if acts is not None and act not in acts:
                out.append("dialogue rule %d: unknown activity %s" % (n, act))
        if set(r) - {"conversation", "roles", "acts", "walking"}:
            out.append("dialogue rule %d: unknown keys %s" % (n, sorted(set(r) - {"conversation", "roles", "acts", "walking"})))
    if not d["rules"] or set(d["rules"][-1]) != {"conversation"}:
        out.append("dialogue: the last rule must match everyone")
    ids = {r["id"] for r in w["residents"]}
    for rid, c in d["residents"].items():
        if rid not in ids:
            out.append("dialogue: no resident %s" % rid)
        if c not in convs:
            out.append("dialogue: %s has no conversation %s" % (rid, c))
    for cid, c in convs.items():
        nodes, where = c["nodes"], "dialogue %s" % cid
        if c["start"] not in nodes:
            out.append("%s: start node %s does not exist" % (where, c["start"]))
            continue
        sets = {f for nd in nodes.values() for o in nd["options"] for f in o.get("do", [])}
        ends = set()                                   # nodes from which some reply ends the talk
        for nid, nd in nodes.items():
            at = "%s.%s" % (where, nid)
            if nd.get("mood") not in moods:
                out.append("%s: unknown mood %s" % (at, nd.get("mood")))
            if not 1 <= len(nd["options"]) <= 9:
                out.append("%s: needs 1 to 9 replies" % at)
            if not any(not o.get("if") for o in nd["options"]):
                out.append("%s: every reply has a condition, so the player could be stuck" % at)
            for text in [nd["say"]] + [o["text"] for o in nd["options"]]:
                for v in var.findall(text):
                    if v not in names:
                        out.append("%s: unknown {%s}" % (at, v))
            for o in nd["options"]:
                if set(o) - {"text", "next", "if", "do"}:
                    out.append("%s: unknown keys on a reply %s" % (at, sorted(set(o) - {"text", "next", "if", "do"})))
                if o["next"] is None:
                    ends.add(nid)
                elif o["next"] not in nodes:
                    out.append("%s: reply '%s' goes to %s, which does not exist" % (at, o["text"], o["next"]))
                for f in o.get("if", []):
                    if f.lstrip("!") not in sets:
                        out.append("%s: flag %s is tested but never set" % (at, f.lstrip("!")))
        seen, todo = {c["start"]}, [c["start"]]        # every node can be reached from the start
        while todo:
            for o in nodes[todo.pop()]["options"]:
                if o["next"] in nodes and o["next"] not in seen:
                    seen.add(o["next"])
                    todo.append(o["next"])
        out += ["%s.%s: cannot be reached from the start" % (where, nid) for nid in nodes if nid not in seen]
        grew = True                                    # and from every node the talk can end
        while grew:
            grew = False
            for nid, nd in nodes.items():
                if nid not in ends and any(o["next"] in ends for o in nd["options"]):
                    ends.add(nid)
                    grew = True
        out += ["%s.%s: the talk can never end from here" % (where, nid) for nid in nodes if nid not in ends]
    return out


def sim_data(w):
    """The city in the shape the living-city rules read (Tools/citysim/citysim.py)."""
    g = w["city"]["grid"]
    P, n = g["pitch"], g["columns"]
    streets = []
    for k in range(g["rows"] + 1):
        for m in range(n):
            streets.append([[m * P, k * P], [(m + 1) * P, k * P]])
    for k in range(n + 1):
        for m in range(g["rows"]):
            streets.append([[k * P, m * P], [k * P, (m + 1) * P]])
    for b in w["blocks"]:
        streets += b["paths"]
    cal = w["calendar"]
    veg = w["vegetation"]
    return dict(name=w["city"]["name"], size=[n * P, g["rows"] * P], calendar=cal["calendar"], weather=cal["weather"], tram=cal["tram"],
                streets=streets, lots=lots(w), residents=w["residents"], roles=w["roles"]["roles"], signatures=w["signatures"]["signatures"],
                choices=dict(w["choices"]["choices"], rounds=[]), use=w["choices"]["use"], species=veg["species"], flowers=veg["flowers"],
                replant_gap_days=veg["replant_gap_days"], fallen_days=veg["fallen_days"], trees=[t for b in w["blocks"] for t in b["trees"]], lamps=[])


def simulation(w, traffic=False):
    """The living-city rules over the whole map. With traffic=True, residents who own a car drive their longer
    trips (LIVING_CITY CITY-043 to CITY-049). The playable slice does not draw cars yet, so it stays off by default."""
    import citysim
    city = citysim.City(data=sim_data(w))
    if traffic:
        import traffic as tr
        tr.Traffic(city, w["roads"], w["traffic"])
    return city
