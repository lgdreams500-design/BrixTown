"""The building designer's rules (docs/specs/domains/BUILDER.md BUILD-004 to BUILD-007; F-BLD-DESIGN level 1, remodel).

A design is the document world.design_of() gives for each building (BUILD-001). The designer edits a copy; this module
says whether the edit is allowed and what it costs, the same on the server and, ported, in the slice:

    problems = builder.check(w, lot_id, old, new, owner, funds)   # [] when the remodel may go ahead
    total, lines = builder.cost(w, lot_id, old, new)              # chips, and what each change costs

`owner` says what the player holds in the building (ADR-0022 units): {"whole": True} for all of it, or
{"floors": [0, 1]} for the units on those floors. Planning rules are World/rules/planning.json (Q-34: each district its
own palette), prices World/rules/building_kit.json `prices` (Q-39: a full remodel about a quarter of a house's value).
Nothing here is stored or ticked; it is worked out from the designs and the data each time it is asked.
"""
import copy
import math

OLD_BASE, GF = 3.55, 3.2                              # the stone storey of an old building; the ground floor's ceiling (play.js)


def _get(d, path):
    for k in path.split("."):
        d = d[k]
    return d


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


def _part(path, allowed):
    """The remodel entry ('walls.stone', 'inside.floors' ...) a changed path falls under, or None."""
    for a in allowed:
        if path == a or path.startswith(a + "."):
            return a
    return None


def _lot(w, lot_id):
    for b in w["blocks"]:
        for l in b["lots"]:
            if l["id"] == lot_id:
                return b, l
    raise KeyError(lot_id)


def civic(w, b, l):
    C = w["planning"]["civic_only"]
    return l["kind"] in C["kinds"] or bool(set(l.get("tags", [])) & set(C["tags"])) or l["name"] in C["names"] or b["template"] in C["templates"]


def measures(w, lot_id, d):
    """What a design's changes are counted in: facade areas, roof area, windows, floor areas."""
    b, l = _lot(w, lot_id)
    x0, z0, x1, z1 = l["rect"]
    W, Dp = x1 - x0, z1 - z0
    sh = d["shell"]
    H, n = sh["height"], sh["floors"]
    per = 2 * (W + Dp)
    band = OLD_BASE if sh["old"] else GF
    to_eaves = d["walls"]["stone_to_eaves"] or d["walls"]["brick_to_eaves"]
    gap = d["openings"]["gap"]
    windows = sum(max(1, int((L - 1.6) // gap)) for L in (W, W, Dp, Dp)) * n
    floor_m2 = max(0.0, (W - .7) * (Dp - .7))
    return dict(stone_m2=per * (H if to_eaves else band), plaster_m2=0.0 if to_eaves else per * max(0.0, H - band), upper_m2=per * max(0.0, H - band),
                roof_m2=W * Dp * (1.25 if d["roof"]["type"] == "pitched" else 1.0), windows=windows, floor_m2=floor_m2, floors=n)


def cost(w, lot_id, old, new):
    """What a remodel costs: (total in chips, [(what, quantity, chips)]). BUILD-005."""
    P, M = w["building_kit"]["prices"], measures(w, lot_id, new)
    lines = []

    def add(what, qty, unit):
        if qty:
            lines.append((what, round(qty, 2), int(round(qty * unit))))
    ch = changed(old, new)
    has = lambda p: any(c == p or c.startswith(p + ".") for c in ch)
    if has("walls.plaster"):
        add("Repaint the plaster", M["plaster_m2"], P["plaster_m2"])
    if has("walls.stone"):
        add("New stone or brick", M["stone_m2"] if not new["walls"]["brick_to_eaves"] else M["stone_m2"] - M["upper_m2"], P["stone_m2"])
    if has("walls.brick_to_eaves") and new["walls"]["brick_to_eaves"]:
        add("Brick to the eaves", M["upper_m2"], P["brick_to_eaves_m2"])
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
    floors = [f for f in range(M["floors"]) if old["inside"]["floors"][f] != new["inside"]["floors"][f]]
    if floors:
        add("New floors (%d storey%s)" % (len(floors), "s" if len(floors) > 1 else ""), M["floor_m2"] * len(floors), P["floor_m2"])
    if has("walls.inner"):
        add("Repaint inside", M["floor_m2"] * M["floors"], P["inner_m2"])
    if lines:
        lines.append(("Permit", 1, P["permit"]))
    return sum(c for _, _, c in lines), lines


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
    outside = [p for p in ch if _part(p, R["outside"])]
    if outside and not owner.get("whole"):
        out.append("changing the outside needs the whole building")
    if any(_part(p, ["walls.inner"]) for p in ch) and not owner.get("whole"):
        out.append("repainting inside throughout needs the whole building")
    held = set(range(l["floors"])) if owner.get("whole") else set(owner.get("floors", []))
    for f in range(l["floors"]):
        if old["inside"]["floors"][f] != new["inside"]["floors"][f] and f not in held:
            out.append("floor %d is not yours to change" % f)
    # every choice from the kit, and from this district's palette (Q-34)
    out += [x.split(": ", 1)[1] for x in world.design_problems(w, {lot_id: new})]
    pal = PL["districts"].get(b["district"])
    if pal is None:
        out.append("no planning palette for %s" % b["district"])
        return out
    if "walls.plaster" in ch:
        pc = new["walls"]["plaster"]
        if pc not in K["plaster"] or K["plaster"].index(pc) not in pal["plaster"]:
            out.append("that plaster colour is not one of %s's" % b["district"])
    if new["roof"]["tiles"] in K["roof_tiles"] and K["roof_tiles"].index(new["roof"]["tiles"]) not in pal["roof_tiles"] and "roof.tiles" in ch:
        out.append("those roof tiles are not %s's" % b["district"])
    st = new["walls"]["stone"]["pattern"]
    if "walls.stone.pattern" in ch:
        if st in PL["civic_only"]["stone"]:
            if not civic(w, b, l):
                out.append("%s stone is kept for the town's civic buildings" % st)
        elif st not in pal["stone"]:
            out.append("%s is not one of %s's stones or bricks" % (st, b["district"]))
        cols = w["surfaces"]["patterns"].get(st, {}).get("colors", [])
        c = new["walls"]["stone"]["color"]
        if c is not None and c not in cols:
            out.append("that colour is not one %s comes in" % st)
    if new["walls"]["brick_to_eaves"] and st not in PL["brick_to_eaves"]:
        out.append("only brick can run to the eaves")
    if "openings.shutter_color" in ch:
        lst = K[PL["shutters"]["old" if new["openings"]["class"] == "old" else "default"]]
        if new["openings"]["shutter_color"] not in lst:
            out.append("that shutter colour is not for these windows")
    if "openings.frame" in ch and new["openings"]["frame"] not in K["frames"]:
        out.append("that frame colour is not in the kit")
    if "walls.inner" in ch and new["walls"]["inner"] not in inner_choices(w):
        out.append("that inside colour is not in the kit")
    for f in range(l["floors"]):
        if new["inside"]["floors"][f] not in [list(v) for v in K["floors"].values()]:
            out.append("floor %d's finish is not in the kit" % f)
    if funds is not None:
        total, _ = cost(w, lot_id, old, new)
        if total > funds:
            out.append("it costs %s brix and you have %s" % (total / 100, funds / 100))
    return out


def inner_choices(w):
    K = w["building_kit"]
    mx = K["inner_mix"]
    return [[p[k] + (mx["with"][k] - p[k]) * mx["share"] for k in range(3)] for p in K["plaster"]]


def remodel(old, **changes):
    """A copy of a design with some parts changed, by dotted path: remodel(d, **{"walls.plaster": [...]})."""
    new = copy.deepcopy(old)
    for path, v in changes.items():
        *head, last = path.split(".")
        node = new
        for k in head:
            node = node[k]
        node[last] = v
    return new
