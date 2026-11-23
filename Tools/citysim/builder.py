"""The building designer's rules (docs/specs/domains/BUILDER.md BUILD-004 to BUILD-007, BUILD-016 to BUILD-019; F-BLD-DESIGN
level 1, remodel).

A design is the document world.design_of() gives for each building (BUILD-001): every choice named by its id in
World/rules/building_kit.json (BUILD-016), walls per facade n, s, e, w (BUILD-018), inside walls and floors per storey
(BUILD-019). The designer edits a copy; this module says what it may offer, whether the edit is allowed and what it costs,
the same on the server and, ported, in the slice:

    menu = builder.options(w, lot_id, design)                     # every choice the designer may offer, by part
    problems = builder.check(w, lot_id, old, new, owner, funds)   # [] when the remodel may go ahead
    total, lines = builder.cost(w, lot_id, old, new)              # chips, and what each change costs

`owner` says what the player holds in the building (ADR-0022 units): {"whole": True} for all of it, {"units": [ids]} for
some of its units (estate.carve ids, "lot:u1"), or {"floors": [0, 1]} for the storeys those units cover. Planning rules
are World/rules/planning.json (Q-34: each district its own palette), prices building_kit.json `prices` (Q-39: a full
remodel about a quarter of a house's value). Nothing here is stored or ticked; it is worked out from the designs and the
data each time it is asked.
"""
import copy

OLD_BASE, GF = 3.55, 3.2                              # the stone storey of an old building; the ground floor's ceiling (play.js)
FACADES = ("n", "s", "e", "w")
FACADE_NAME = {"n": "north", "s": "south", "e": "east", "w": "west"}


def _paths(d, prefix=""):
    """Every leaf of a design, as a dotted path."""
    out = {}
    for k, v in d.items():
        p = prefix + k
        if isinstance(v, dict):
            out.update(_paths(v, p + "."))
        else:
            out[p] = v
    return out


def changed(old, new):
    """The dotted paths whose value differs between two designs."""
    a, b = _paths(old), _paths(new)
    return sorted(p for p in set(a) | set(b) if a.get(p) != b.get(p))


def _match(path, pattern):
    """Whether a dotted path falls under a pattern ('walls.*.plaster' matches 'walls.n.plaster' and 'walls.n.plaster.x')."""
    p, q = path.split("."), pattern.split(".")
    return len(p) >= len(q) and all(b == "*" or a == b for a, b in zip(p, q))


def _part(path, allowed):
    """The remodel entry ('walls.*.stone', 'inside.floors' ...) a changed path falls under, or None."""
    return next((a for a in allowed if _match(path, a)), None)


def _lot(w, lot_id):
    for b in w["blocks"]:
        for l in b["lots"]:
            if l["id"] == lot_id:
                return b, l
    raise KeyError(lot_id)


def civic(w, b, l):
    C = w["planning"]["civic_only"]
    return l["kind"] in C["kinds"] or bool(set(l.get("tags", [])) & set(C["tags"])) or l["name"] in C["names"] or b["template"] in C["templates"]


def unit_floors(w, lot_id, unit_ids):
    """The storeys the given units of a building cover (ADR-0022: a unit is one or more whole storeys)."""
    import estate                                     # Tools/citysim
    import world
    want = set(unit_ids)
    return sorted({f for u in estate.carve(w["estate"], world.lots(w), w["residents"]) if u["lot_id"] == lot_id and u["id"] in want for f in u["floors"]})


def held_floors(w, lot_id, owner, floors):
    if owner.get("whole"):
        return set(range(floors))
    out = set(owner.get("floors", []))
    if owner.get("units"):
        out |= set(unit_floors(w, lot_id, owner["units"]))
    return out


def measures(w, lot_id, d):
    """What a design's changes are counted in: each facade's areas, the roof area, the windows, the floor area."""
    b, l = _lot(w, lot_id)
    x0, z0, x1, z1 = l["rect"]
    W, Dp = x1 - x0, z1 - z0
    sh = d["shell"]
    H, n = sh["height"], sh["floors"]
    band = OLD_BASE if sh["old"] else GF
    gap = w["building_kit"]["windows"][d["openings"]["class"]]["gap"]
    length = dict(n=W, s=W, e=Dp, w=Dp)
    facades = {}
    for f in FACADES:
        fa, L = d["walls"][f], length[f]
        to_eaves = fa["stone_to_eaves"] or fa["brick_to_eaves"]
        facades[f] = dict(stone_m2=L * (H if to_eaves else band), plaster_m2=0.0 if to_eaves else L * max(0.0, H - band),
                          upper_m2=L * max(0.0, H - band))
    windows = sum(max(1, int((length[f] - 1.6) // gap)) for f in FACADES) * n
    floor_m2 = max(0.0, (W - .7) * (Dp - .7))
    return dict(facades=facades, roof_m2=W * Dp * (1.25 if d["roof"]["type"] == "pitched" else 1.0), windows=windows,
                floor_m2=floor_m2, floors=n)


def cost(w, lot_id, old, new):
    """What a remodel costs: (total in chips, [(what, quantity, chips)]). BUILD-005. Walls are charged by the area of each
    facade changed (BUILD-018), inside walls and floors by the storeys changed (BUILD-019)."""
    P, M = w["building_kit"]["prices"], measures(w, lot_id, new)
    lines = []

    def add(what, qty, unit):
        if qty:
            lines.append((what, round(qty, 2), int(round(qty * unit))))
    ch = changed(old, new)
    has = lambda p: any(_match(c, p) for c in ch)
    plaster = stone = brick = 0.0
    for f in FACADES:
        fa, Mf = new["walls"][f], M["facades"][f]
        if has("walls.%s.plaster" % f):
            plaster += Mf["plaster_m2"]
        if has("walls.%s.stone" % f):
            stone += Mf["stone_m2"] if not fa["brick_to_eaves"] else Mf["stone_m2"] - Mf["upper_m2"]
        if has("walls.%s.brick_to_eaves" % f) and fa["brick_to_eaves"]:
            brick += Mf["upper_m2"]
    add("Repaint the plaster", plaster, P["plaster_m2"])
    add("New stone or brick", stone, P["stone_m2"])
    add("Brick to the eaves", brick, P["brick_to_eaves_m2"])
    if has("roof.tiles"):
        add("Re-tile the roof", M["roof_m2"], P["roof_tiles_m2"])
    if has("openings.shutters") and new["openings"]["shutters"]:
        add("Shutters", M["windows"], P["shutters_each"])
    elif has("openings.shutter_color") and new["openings"]["shutters"]:
        add("Paint the shutters", M["windows"], P["shutter_paint_each"])
    if has("openings.frame"):
        add("Paint the window frames", M["windows"], P["frame_each"])
    if has("openings.balcony") and new["openings"]["balcony"]:
        add("A balcony", 1, P["balcony"])
    if has("openings.keystones") and new["openings"]["keystones"]:
        add("Keystones over the windows", M["windows"], P["keystones_each"])
    if has("ornament.quoins") and new["ornament"]["quoins"]:
        add("Corner stones", 1, P["quoins"])
    if has("ornament.lantern") and new["ornament"]["lantern"]:
        add("A wall lantern", 1, P["lantern"])
    s = lambda k: "s" if k > 1 else ""
    storeys = range(min(len(old["inside"]["floors"]), len(new["inside"]["floors"])))         # a remodel keeps the storeys; a design that changes them is refused by check()
    floors = [f for f in storeys if old["inside"]["floors"][f] != new["inside"]["floors"][f]]
    if floors:
        add("New floors (%d storey%s)" % (len(floors), s(len(floors))), M["floor_m2"] * len(floors), P["floor_m2"])
    walls = [f for f in storeys if old["inside"]["walls"][f] != new["inside"]["walls"][f]]
    if walls:
        add("Repaint inside (%d storey%s)" % (len(walls), s(len(walls))), M["floor_m2"] * len(walls), P["inner_m2"])
    if lines:
        lines.append(("Permit", 1, P["permit"]))
    return sum(c for _, _, c in lines), lines


def options(w, lot_id, d):
    """Every choice the designer may offer on this building at level 1 (BUILD-003, BUILD-007, BUILD-017): by remodel part,
    a list of {id, name} (and rgb for colours, mat for floors), each from the kit, approved, allowed for the building and
    in its district's palette. Stone and brick are listed per pattern with the colours it comes in. A choice not listed
    here is rejected by check()."""
    b, l = _lot(w, lot_id)
    K, PL, pats = w["building_kit"], w["planning"], w["surfaces"]["patterns"]
    pal = PL["districts"].get(b["district"], {"stone": [], "plaster": [], "roof_tiles": []})
    C = K["colours"]
    col = lambda ids: [dict(id=i, name=C[i]["name"], rgb=C[i]["rgb"]) for i in ids]
    stones = [s for s in pal["stone"]] + ([s for s in PL["civic_only"]["stone"]] if civic(w, b, l) else [])
    stone = [dict(id=s, name=s.replace("_", " ").capitalize(), colors=[None if isinstance(c, str) else k for k, c in enumerate(pats[s]["colors"])],
                  brick_to_eaves=s in PL["brick_to_eaves"]) for s in stones]
    o = d["openings"]
    shut = K[PL["shutters"]["old" if o["class"] == "old" else "default"]]
    FF = K["floor_finishes"]
    menu = {
        "walls.*.plaster": col([c for c in K["plaster"] if c in pal["plaster"]]),
        "walls.*.stone": stone,
        "roof.tiles": col([c for c in K["roof_tiles"] if c in pal["roof_tiles"]]),
        "openings.frame": col(K["frames"]),
        "inside.walls": col(K["plaster"]),
        "inside.floors": [dict(id=k, name=v["name"], rgb=v["rgb"], mat=v["mat"]) for k, v in FF.items() if k != "note"],
    }
    if world_allowed(K["openings"]["shutters"], d):
        menu["openings.shutters"] = [dict(id=True, name="With shutters"), dict(id=False, name="No shutters")]
        menu["openings.shutter_color"] = col(shut)
    for part, cat, key in (("openings.balcony", "openings", "balcony"), ("openings.keystones", "openings", "keystones"),
                           ("ornament.quoins", "ornaments", "quoins"), ("ornament.lantern", "ornaments", "lantern")):
        it = K[cat][key]
        if world_allowed(it, d):
            menu[part] = [dict(id=True, name=it["name"]), dict(id=False, name="No " + it["name"][0].lower() + it["name"][1:])]
    return menu


def world_allowed(item, d):
    import world
    return world.allowed(item, d)


def check(w, lot_id, old, new, owner, funds=None):
    """Whether a remodel may go ahead (BUILD-004, BUILD-006, BUILD-007): [] when it may, else what is wrong, in plain words."""
    import world                                       # Tools/worldgen (on the path when run from Tools/citysim)
    b, l = _lot(w, lot_id)
    K, PL = w["building_kit"], w["planning"]
    out = []
    R = PL["remodel"]
    ch = changed(old, new)
    if not ch:
        return ["nothing has changed"]
    for p in ch:                                      # level 1 changes finishes only; the shell stays as it is
        if not (_part(p, R["outside"]) or _part(p, R["inside"])):
            out.append("a remodel cannot change %s" % p)
    if any(_part(p, R["outside"]) for p in ch) and not owner.get("whole"):
        out.append("changing the outside needs the whole building")
    held = held_floors(w, lot_id, owner, l["floors"])
    for f in range(l["floors"]):
        if (old["inside"]["floors"][f] != new["inside"]["floors"][f] or old["inside"]["walls"][f] != new["inside"]["walls"][f]) and f not in held:
            out.append("floor %d is not yours to change" % f)
    # every choice from the kit, and from this district's palette (Q-34)
    before = set(world.design_problems(w, {lot_id: old}))   # what was wrong already is not this remodel's doing (a lot with no stair yet)
    out += [x.split(": ", 1)[1] for x in world.design_problems(w, {lot_id: new}) if x not in before]
    pal = PL["districts"].get(b["district"])
    if pal is None:
        out.append("no planning palette for %s" % b["district"])
        return out
    for f in FACADES:
        fa, where = new["walls"][f], FACADE_NAME[f]
        if "walls.%s.plaster" % f in ch and fa["plaster"] not in pal["plaster"]:
            out.append("that plaster colour is not one of %s's" % b["district"])
        st = fa["stone"]["pattern"]
        if any(_match(p, "walls.%s.stone" % f) for p in ch):
            if st in PL["civic_only"]["stone"]:
                if not civic(w, b, l):
                    out.append("%s stone is kept for the town's civic buildings" % st)
            elif st not in pal["stone"]:
                out.append("%s is not one of %s's stones or bricks" % (st, b["district"]))
            cols = w["surfaces"]["patterns"].get(st, {}).get("colors", [])
            c = fa["stone"]["color"]
            if c is not None and (not isinstance(c, int) or not 0 <= c < len(cols) or isinstance(cols[c], str)):
                out.append("that colour is not one %s comes in" % st)
        if fa["brick_to_eaves"] and st not in PL["brick_to_eaves"]:
            out.append("only brick can run to the eaves (the %s facade)" % where)
    if "roof.tiles" in ch and new["roof"]["tiles"] not in pal["roof_tiles"]:
        out.append("those roof tiles are not %s's" % b["district"])
    if "openings.shutter_color" in ch:
        lst = K[PL["shutters"]["old" if new["openings"]["class"] == "old" else "default"]]
        if new["openings"]["shutter_color"] not in lst:
            out.append("that shutter colour is not for these windows")
    for part, cat, key in (("openings.shutters", "openings", "shutters"), ("openings.balcony", "openings", "balcony"), ("openings.keystones", "openings", "keystones"),
                           ("ornament.quoins", "ornaments", "quoins"), ("ornament.lantern", "ornaments", "lantern")):
        if part in ch and _get(new, part) and not world.allowed(K[cat][key], new):
            out.append("%s cannot go on this building" % K[cat][key]["name"].lower())
    if funds is not None:
        total, _ = cost(w, lot_id, old, new)
        if total > funds:
            out.append("it costs %s brix and you have %s" % (total / 100, funds / 100))
    return list(dict.fromkeys(out))                   # each problem once, though four facades share it


def _get(d, path):
    for k in path.split("."):
        d = d[k]
    return d


def remodel(old, **changes):
    """A copy of a design with some parts changed, by dotted path: remodel(d, **{"walls.n.plaster": "apricot"}). A path
    with '*' changes every facade: remodel(d, **{"walls.*.plaster": "apricot"})."""
    new = copy.deepcopy(old)
    for path, v in changes.items():
        *head, last = path.split(".")
        nodes = [new]
        for k in head:
            nodes = [n[x] for n in nodes for x in (FACADES if k == "*" else [k])]
        for n in nodes:
            n[last] = copy.deepcopy(v)
    return new


def check_script(w, ds):
    """A fixed run of remodels for the slice's port (Prototype/play/builder.js) to reproduce exactly (BUILD-005 to BUILD-007,
    BUILD-016 to BUILD-019): [lot id, {dotted path: value}, owner, funds]. Lots are chosen from the data so the run follows the
    town as it changes."""
    import world
    lots = {l["id"]: (b, l) for b in w["blocks"] for l in b["lots"]}
    K = w["building_kit"]

    def pick(district, floors=2, plain=True, kind="home"):
        return next(i for i, (b, l) in lots.items() if i in ds and b["district"] == district and l["kind"] == kind and l["floors"] >= floors
                    and not civic(w, b, l) and not l.get("house") and not ds[i]["shell"]["plan"]
                    and (not plain or not any(ds[i]["walls"][f]["stone_to_eaves"] or ds[i]["walls"][f]["brick_to_eaves"] for f in FACADES)))
    out_, old = pick("Outskirts"), pick("Old Town")
    d, o = ds[out_], ds[old]
    other = lambda lst, v: next(x for x in lst if x != v)
    side = d["shell"]["side"]
    whole = {"whole": True}
    return [
        [out_, {"walls.*.plaster": other(K["plaster"], d["walls"]["n"]["plaster"]), "roof.tiles": other(K["roof_tiles"], d["roof"]["tiles"])}, whole, None],
        [out_, {"walls.%s.plaster" % side: other(K["plaster"], d["walls"][side]["plaster"])}, {"floors": [1]}, None],
        [out_, {"walls.%s.stone" % side: {"pattern": "brick_flemish", "color": 1}, "walls.%s.brick_to_eaves" % side: True, "openings.balcony": not d["openings"]["balcony"]}, whole, None],
        [out_, {"walls.*.stone": {"pattern": "rubble", "color": None}, "ornament.quoins": not d["ornament"]["quoins"]}, whole, None],
        [out_, {"inside.walls": [other(K["plaster"], x) if f == 1 else x for f, x in enumerate(d["inside"]["walls"])],
                "inside.floors": [("sand_tiles" if x != "sand_tiles" else "dark_boards") if f == 1 else x for f, x in enumerate(d["inside"]["floors"])]}, {"floors": [1]}, None],
        [out_, {"inside.floors": ["red_tiles"] * len(d["inside"]["floors"])}, {"floors": [1]}, None],
        [old, {"openings.frame": other(K["frames"], o["openings"]["frame"]), "openings.shutter_color": "plum", "openings.shutters": True}, whole, None],
        [old, {"walls.*.plaster": other(K["plaster"], o["walls"]["n"]["plaster"]), "ornament.lantern": not o["ornament"]["lantern"], "openings.keystones": not o["openings"]["keystones"]}, whole, 5000],
        [old, {"shell.floors": 9}, whole, None],
    ]


def run_script(w, ds, script):
    """Each remodel of check_script: what is offered (ids by part), the problems, the cost and its lines, and the built walls of
    the new design's front, for the slice to match."""
    import world
    out = []
    for lot_id, changes, owner, funds in script:
        old = ds[lot_id]
        new = remodel(old, **changes)
        total, lines = cost(w, lot_id, old, new)
        menu = options(w, lot_id, old)
        bt = world.built(w, new)
        out.append(dict(options={k: [o["id"] for o in v] for k, v in sorted(menu.items())}, problems=check(w, lot_id, old, new, owner, funds),
                        total=total, lines=[list(x) for x in lines], front=bt["walls"][old["shell"]["side"]], inside=bt["inside"]))
    return out
