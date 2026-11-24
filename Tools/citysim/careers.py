"""NPC careers and how many people work at each place (docs/specs/domains/LIVING_CITY.md CITY-081 to CITY-086).

All the numbers are in World/rules/careers.json. Standard library only. Nothing here is ticked: who does which job is
worked out once from the residents' homes, the lots and the rules, and gives the same answer on every machine.

    workplace_type(lot, R, template)  -> the staffing type of a lot, or None
    mall_outlets(lot)                 -> the outlets of a mall, floor by floor (CITY-098)
    posts(lot, R, template)           -> [(career, unit id or None)], in the order of the type's posts
    assign(residents, lots, R, templates) -> {resident id: dict(career, role, work, post)}
    problems(R, roles)                -> what is wrong with the rules, in plain words
"""
import hashlib

TOWN_GROUPS_SKIP = ("outside", "transport", "parks")      # groups that do not take town posts


def rnd(*keys):
    h = hashlib.blake2b(repr(keys).encode(), digest_size=8).digest()
    return int.from_bytes(h, "big") / 2.0 ** 64


def area(r):
    return (r[2] - r[0]) * (r[3] - r[1])


def workplace_type(lot, R, template=None):
    """The first rule in R['workplaces'] that fits the lot; None for lots nobody is hired at."""
    for w in R["workplaces"]:
        if w["kind"] != lot["kind"]:
            continue
        if "name_has" in w and w["name_has"] not in lot.get("name", ""):
            continue
        if "tag" in w and w["tag"] not in lot.get("tags", []):
            continue
        if "template" in w and w["template"] != template:
            continue
        return w["type"]
    return None


def outdoors(lot, R, template=None):
    """True for an open workplace (the market) that is staffed though nobody goes inside it."""
    t = workplace_type(lot, R, template)
    return any(w["type"] == t and w.get("outdoors") for w in R["workplaces"])


def head_count(spec, floor_m2):
    """How many people each post of a staffing type has at a place of this gross floor area."""
    lo, hi = spec["small_m2"], spec["large_m2"]
    t = min(1.0, max(0.0, (floor_m2 - lo) / float(hi - lo)))
    return [p["min"] + int(t * (p["max"] - p["min"]) + 0.5) for p in spec["posts"]]


def _post_list(spec, floor_m2, unit=None):
    out = []
    for p, n in zip(spec["posts"], head_count(spec, floor_m2)):
        out += [(p["career"], unit)] * n
    return out


def floor_area(lot, spec, template=None):
    floors = 1 if spec["floors"] == "ground" and template != "towerO" else lot.get("floors", 1) or 1
    return area(lot["rect"]) * floors


def mall_outlets(lot):
    """The outlets of a mall: its units with a kind of outlet, on every floor, in the plan's order (CITY-098)."""
    return [u for L in lot.get("plan", {}).get("levels", []) for u in L["units"] if u.get("outlet")]


def _mall_first(lot, R):
    """The posts of a mall of outlets that are always filled: one keeper to each outlet, then those who look after the
    whole mall (guards, janitors), who have no unit (CITY-098)."""
    outs = mall_outlets(lot)
    if not outs:
        return []
    M = R["mall_staff"]
    return [(M["keeper"][u["kind"]], u["id"]) for u in outs] + [(w["career"], None) for w in M["whole_mall"] for _ in range(w["count"])]


def posts(lot, R, template=None):
    """Every post a workplace has, as (career, unit id). A mall's ground-floor units each have their own posts; in a mall
    of outlets each outlet on every floor has one keeper and the mall has its own few posts besides (unit None), and
    only its other units (corner stores, kiosks) go by their floor area."""
    if "plan" in lot:
        out = _mall_first(lot, R)
        for u in lot["plan"]["levels"][0]["units"]:
            t = None if u.get("outlet") else R["mall_units"].get(u["kind"])
            if t:
                out += _post_list(R["staffing"][t], area(u["rect"]), u["id"])
        return out
    t = workplace_type(lot, R, template)
    if not t:
        return []
    spec = R["staffing"][t]
    return _post_list(spec, floor_area(lot, spec, template))


def minimum(lot, R, template=None):
    """The posts a workplace cannot open without (each post's min), as indices into posts(). In a mall of outlets:
    every keeper and everyone who looks after the whole mall, so that no outlet stands empty (CITY-098)."""
    if "plan" in lot:
        return list(range(len(_mall_first(lot, R))))
    t = workplace_type(lot, R, template)
    if not t:
        return []
    spec, out, k = R["staffing"][t], [], 0
    for p, n in zip(spec["posts"], head_count(spec, floor_area(lot, spec, template))):
        out += list(range(k, k + p["min"]))
        k += n
    return out


def _weighted(weights, *keys):
    names = sorted(weights)
    total = float(sum(weights[n] for n in names))
    x, acc = rnd(*keys) * total, 0.0
    for n in names:
        acc += weights[n]
        if x < acc:
            return n
    return names[-1]


def assign(residents, lots, R, templates=None):
    """Careers for everyone who works. residents: [{id, role, home, ...}]; lots: [{id, kind, name, rect, door, ...}];
    templates: {lot id: block template}. Returns {resident id: dict(career, role, work, post)} where post is the
    resident's index among the staff of their workplace (their own place to work there, CITY-085), None at home.

    0. Couriers beyond couriers_share of all residents work in town instead (CITY-087).
    1. Couriers and commuters who work outside town get a career from their weights.
    2. A share of those who work in town work from home (home_based, CITY-083).
    3. Every workplace's minimum posts are filled first, each by the nearest resident still free (so every place opens).
    4. Everyone else, in id order, takes a free post at the nearest workplace whose group is still under its share
       (CITY-084); when every group is at its share, the nearest free post of any group.
    Posts nobody takes are jobs for in-commuters (CITY_BALANCE 2.2). The answer is the same however often it is run."""
    templates = templates or {}
    C = R["careers"]
    byid = {l["id"]: l for l in lots}
    out = {}
    # 0. couriers beyond their share of all residents work in town instead (the ones whose hash is highest)
    couriers = sorted((r for r in residents if r["role"] == "courier"), key=lambda r: (rnd("courier_keep", r["id"]), r["id"]))
    keep = int(round(R.get("couriers_share", 1.0) * len(residents)))
    moved = {r["id"] for r in couriers[keep:]}
    for r in residents:
        if r["id"] in moved:
            continue
        if r["role"] == "courier":
            out[r["id"]] = dict(career=_weighted(R["courier_careers"], "courier", r["id"]), role="courier", work=r.get("work"), post=None)
        elif r["role"] in R.get("keep_roles", {}):
            out[r["id"]] = dict(career=R["keep_roles"][r["role"]], role=r["role"], work=r.get("work"), post=None)
        elif r["role"] == "commuter":
            out[r["id"]] = dict(career=_weighted(R["commuter_careers"], "commuter", r["id"]), role="commuter", work=r.get("work"), post=None)
    workers = sorted((r for r in residents if r["role"] in R["reassign_roles"] or r["id"] in moved), key=lambda r: r["id"])
    n = len(workers)
    quota = {g: int(round(s * n)) for g, s in R["shares"].items()}
    # 2. home-based creatives: the ones whose hash is lowest, so the choice is stable
    by_hash = sorted(workers, key=lambda r: (rnd("home_based", r["id"]), r["id"]))
    home_based = {r["id"] for r in by_hash[:quota.get("creative", 0)]}
    for r in workers:
        if r["id"] in home_based:
            out[r["id"]] = dict(career=_weighted(R["home_based"], "art", r["id"]), role=C[_weighted(R["home_based"], "art", r["id"])]["role"],
                                work=None, post=None)
    free = [r for r in workers if r["id"] not in home_based]
    # the posts of every workplace
    places = []
    for l in sorted(lots, key=lambda l: l["id"]):
        if not l.get("enterable", True) and not outdoors(l, R, templates.get(l["id"])):
            continue
        ps = posts(l, R, templates.get(l["id"]))
        if ps:
            places.append(dict(lot=l, posts=ps, taken=[None] * len(ps), mins=minimum(l, R, templates.get(l["id"]))))
    used = {g: 0 for g in R["shares"]}
    d = lambda a, b: abs(a["door"][0] - b["door"][0]) + abs(a["door"][1] - b["door"][1])
    left = {r["id"]: r for r in free}
    # 3. minimums, nearest resident first
    for p in places:
        for k in p["mins"]:
            if not left:
                break
            r = min(left.values(), key=lambda r: (d(byid[r["home"]], p["lot"]), r["id"]))
            p["taken"][k] = r["id"]
            used[C[p["posts"][k][0]]["group"]] += 1
            del left[r["id"]]
    # 4. the rest, nearest workplace with a free post in a group under its share
    for r in sorted(left.values(), key=lambda r: r["id"]):
        h = byid[r["home"]]
        best = None
        for strict in (True, False):
            for p in places:
                k = next((k for k, (c, _) in enumerate(p["posts"]) if p["taken"][k] is None and
                          (not strict or used[C[c]["group"]] < quota.get(C[c]["group"], 0))), None)
                if k is None:
                    continue
                key = (d(h, p["lot"]), p["lot"]["id"])
                if best is None or key < best[0]:
                    best = (key, p, k)
            if best:
                break
        if best is None:
            raise ValueError("more residents working in town than posts: add workplaces or posts")
        _, p, k = best
        p["taken"][k] = r["id"]
        used[C[p["posts"][k][0]]["group"]] += 1
    for p in places:
        staff = [(k, rid) for k, rid in enumerate(p["taken"]) if rid is not None]
        for i, (k, rid) in enumerate(staff):
            c = p["posts"][k][0]
            out[rid] = dict(career=c, role=C[c]["role"], work=p["lot"]["id"], post=i, unit=p["posts"][k][1])
            if c in R.get("second_jobs", {}):                    # CITY-088: a day job after the early shift
                out[rid]["day_career"] = _weighted(R["second_jobs"][c], "day", rid)
    return out


def problems(R, roles):
    """Every rule World/rules/careers.json must keep."""
    out = []
    C = R["careers"]
    groups = set(R["shares"]) | set(TOWN_GROUPS_SKIP)
    for k, c in C.items():
        if c.get("role") not in roles:
            out.append("career %s: unknown role %s" % (k, c.get("role")))
        if c.get("group") not in groups:
            out.append("career %s: unknown group %s" % (k, c.get("group")))
        if not c.get("title"):
            out.append("career %s: no title" % k)
        if "wear" in c and not (isinstance(c["wear"], str) and c["wear"]):
            out.append("career %s: wear must name the clothes of the job" % k)
    if len(C) < R["targets"]["careers_at_least"]:
        out.append("only %d careers; want at least %d" % (len(C), R["targets"]["careers_at_least"]))
    if abs(sum(R["shares"].values()) - 1.0) > 0.005:
        out.append("shares add up to %.3f, not 1" % sum(R["shares"].values()))
    for t, s in R["staffing"].items():
        if s["floors"] not in ("ground", "all", "unit"):
            out.append("staffing %s: floors must be ground, all or unit" % t)
        if not s["small_m2"] < s["large_m2"]:
            out.append("staffing %s: small_m2 must be below large_m2" % t)
        for p in s["posts"]:
            if p["career"] not in C:
                out.append("staffing %s: unknown career %s" % (t, p["career"]))
            if not 0 <= p["min"] <= p["max"]:
                out.append("staffing %s: %s needs 0 <= min <= max" % (t, p["career"]))
    for w in R["workplaces"]:
        if w["type"] not in R["staffing"]:
            out.append("workplace rule for %s: no staffing" % w["type"])
    for t in R["mall_units"].values():
        if t not in R["staffing"]:
            out.append("mall unit type %s: no staffing" % t)
    M = R.get("mall_staff") or {}
    for trade in ("shop", "food"):                                # CITY-098: a keeper's career for each trade of outlet
        if (M.get("keeper") or {}).get(trade) not in C:
            out.append("mall_staff: no known career for the keeper of a %s outlet" % trade)
    for w in M.get("whole_mall") or []:
        if w.get("career") not in C:
            out.append("mall_staff: unknown career %s" % w.get("career"))
        if not (isinstance(w.get("count"), int) and w["count"] >= 0):
            out.append("mall_staff: %s needs a whole count of 0 or more" % w.get("career"))
    for k, ws in R.get("second_jobs", {}).items():
        if k not in C:
            out.append("second_jobs: unknown career %s" % k)
        for d in ws:
            if d not in C:
                out.append("second_jobs %s: unknown day career %s" % (k, d))
    for name in ("home_based", "commuter_careers", "courier_careers"):
        for k in R[name]:
            if k not in C:
                out.append("%s: unknown career %s" % (name, k))
    for k in R["home_based"]:
        if k in C and C[k]["group"] != "creative":
            out.append("home_based: %s is not a creative career" % k)
    for k in R.get("keep_roles", {}).values():
        if k not in C:
            out.append("keep_roles: unknown career %s" % k)
    for r in R["reassign_roles"]:
        if r not in roles:
            out.append("reassign_roles: unknown role %s" % r)
    return out
