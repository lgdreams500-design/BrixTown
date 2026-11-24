"""Furnishing a home in the building designer (docs/specs/domains/FURNITURE.md FURN-051, FURN-052, FURN-055; BUILDER.md
section 2.6; F-PROP-FURNISH level 2).

A storey's furniture is a layout: a list of pieces, each one

    {"id": "t3", "kind": "town", "name": "Wardrobe", "w": 1.2, "d": 0.6, "h": 2.1, "x": 4.5, "z": 4.6, "yaw": 90, "rug": False}

`kind` is "town" for a piece that was in the home when it was bought (the slice finds those in the room as it was
furnished), or the id of a piece from World/rules/furnishing.json `pieces` (with its `line`, one of `lines`). x and z
are the middle of the piece in metres, yaw its turn in degrees: u across the piece goes to (cos, -sin), v from its front
to its back to (sin, cos), as the slice's M4.ry. A rug lies on the floor and other pieces may stand on it.

    problems = furnish.check(F, room, keep, ceiling, before, after, funds, belongings)   # [] when the new layout may be kept
    total, lines = furnish.cost(F, before, after, belongings)                           # chips, and what each change costs
    belongings = furnish.stow(before, after, belongings, where)                         # what the player holds afterwards

`room` is the storey's floor inside its walls [x0, z0, x1, z1]; `keep` the rectangles that must stay clear (the way in
from the door, the stairwell: storeys.json keep_clear); `ceiling` the height under the ceiling. As for a remodel, a
layout is refused only for what the change makes wrong, never for what was already so. Moving, turning and taking away
are free; an added piece costs its price. Nothing is stored or ticked here.

Belongings (INV-001, FURN-050): the pieces the player owns that are not placed. A piece taken away goes to the belongings
(a town piece remembers where it came from, so it can be drawn anywhere: {"lot", "level", "id"}); a piece placed from the
belongings keeps its id ("b:lot:level:t3") and costs nothing.
"""
import math

TOL = 1e-3                                             # pieces may touch: they must overlap by more than a millimetre to clash


def corners(p):
    """The four corners of a piece's footprint, as the slice works them out (same order of operations)."""
    a = p["yaw"] * math.pi / 180
    c, s = math.cos(a), math.sin(a)
    out = []
    for u, v in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        u, v = u * p["w"] / 2, v * p["d"] / 2
        out.append((p["x"] + c * u + s * v, p["z"] - s * u + c * v))
    return out


def _apart(A, B):
    """Whether two convex outlines (lists of corners) are kept apart by a line along one of their edges."""
    for P in (A, B):
        for k in range(len(P)):
            ex, ez = P[(k + 1) % len(P)][0] - P[k][0], P[(k + 1) % len(P)][1] - P[k][1]
            n = math.hypot(ex, ez)
            if n < 1e-9:
                continue
            nx, nz = -ez / n, ex / n
            pa = [x * nx + z * nz for x, z in A]
            pb = [x * nx + z * nz for x, z in B]
            if min(max(pa), max(pb)) - max(min(pa), min(pb)) <= TOL:
                return True
    return False


def _rect(r):
    x0, z0, x1, z1 = r
    return [(x0, z0), (x1, z0), (x1, z1), (x0, z1)]


def _the(p):
    return "the " + p["name"].lower()


def problems(F, room, keep, ceiling, pieces):
    """Everything wrong with a layout, in the order of its pieces: [key, message], the key naming the piece (or the two
    pieces) and what is wrong, so a new piece with the same name as an old one is still checked."""
    out = []
    say = lambda key, msg: out.append([key, msg])
    g, turn = F["grid_m"], F["turn_deg"]
    for p in pieces:
        if p["kind"] != "town" and p["kind"] not in F["pieces"]:
            say(p["id"] + "|sold", "the %s is not sold here" % p["kind"])
            continue
        if p["kind"] != "town" and p.get("line") not in F["lines"]:
            say(p["id"] + "|line", "%s has no line %s" % (_the(p), p.get("line")))
        if abs(p["yaw"] / turn - round(p["yaw"] / turn)) > 1e-6:
            say(p["id"] + "|turn", "%s is not turned in %d degree steps" % (_the(p), turn))
        if p["kind"] != "town" and any(abs(v / g - round(v / g)) > 1e-6 for v in (p["x"], p["z"])):
            say(p["id"] + "|grid", "%s is off the %d cm grid" % (_the(p), round(g * 100)))
        C = corners(p)
        if min(x for x, _ in C) < room[0] - TOL or max(x for x, _ in C) > room[2] + TOL or \
                min(z for _, z in C) < room[1] - TOL or max(z for _, z in C) > room[3] + TOL:
            say(p["id"] + "|room", "%s sticks out of the room" % _the(p))
        if p["h"] > ceiling + TOL:
            say(p["id"] + "|ceiling", "%s does not fit under the ceiling" % _the(p))
        if not p.get("rug") and any(not _apart(C, _rect(r)) for r in keep):
            say(p["id"] + "|keep", "%s is in the way of the door or the stairs" % _the(p))
    stand = [p for p in pieces if not p.get("rug") and (p["kind"] == "town" or p["kind"] in F["pieces"])]
    for i, a in enumerate(stand):
        for b in stand[i + 1:]:
            if not _apart(corners(a), corners(b)):
                say("|".join(sorted([a["id"], b["id"]])) + "|clash", "%s and %s stand in each other" % (_the(a), _the(b)))
    return out


def cost(F, before, after, belongings=()):
    """What the change costs: an added piece its price; placing one from the belongings, moving, turning and taking away
    nothing. Lines [what, 1, chips]."""
    was = {p["id"]: p for p in before}
    now = {p["id"] for p in after}
    mine = {b["id"] for b in belongings}
    lines = []
    for p in after:
        o = was.get(p["id"])
        if o is None and p["id"] in mine:
            lines.append(["Place: " + p["name"], 1, 0])
        elif o is None:
            lines.append(["Buy: " + p["name"], 1, F["pieces"][p["kind"]]["price"] if p["kind"] in F["pieces"] else 0])
        elif (o["x"], o["z"], o["yaw"]) != (p["x"], p["z"], p["yaw"]):
            lines.append(["Move: " + p["name"], 1, 0])
    for p in before:
        if p["id"] not in now:
            lines.append(["To your belongings: " + p["name"], 1, 0])
    return sum(c for _, _, c in lines), lines


def check(F, room, keep, ceiling, before, after, funds=None, belongings=()):
    """[] when the new layout may be kept: nothing it makes wrong (FURN-051, FURN-055), every piece placed from the
    belongings is one of them (INV-001), and the player can pay."""
    old = {k for k, _ in problems(F, room, keep, ceiling, before)}
    out = list(dict.fromkeys(m for k, m in problems(F, room, keep, ceiling, after) if k not in old))
    was, mine = {p["id"] for p in before}, {b["id"] for b in belongings}
    for p in after:
        if p["id"] not in was and p["id"].startswith("b") and p["id"] not in mine:
            out.append("%s is not among your belongings" % _the(p))
        elif p["id"] not in was and not p["id"].startswith("b") and F["pieces"].get(p["kind"], {}).get("from"):
            out.append("%s is sold at the %s, not here" % (_the(p), F["pieces"][p["kind"]]["from"]))     # FURN-064
    total = cost(F, before, after, belongings)[0]
    if funds is not None and total > funds:
        out.append("you cannot pay %d.%02d brix" % (total // 100, total % 100))
    return out


def stow(before, after, belongings, where=None):
    """The belongings after a change: less what was placed from them, with what was taken away. A piece taken away gets the
    id "b:lot:level:its id" from `where` it was ({"lot", "level"}), so ids never clash between homes, and a town piece
    remembers where it came from so it can be drawn anywhere."""
    now = {p["id"] for p in after}
    out = [b for b in belongings if b["id"] not in now]
    w = where or {}
    for p in before:
        if p["id"] in now:
            continue
        q = {k: v for k, v in p.items() if k not in ("x", "z", "yaw")}
        if not p["id"].startswith("b"):
            q["id"] = "b:%s:%s:%s" % (w.get("lot", ""), w.get("level", ""), p["id"])
            if p["kind"] == "town" and "from" not in p:
                q["from"] = dict(where or {}, id=p["id"])
        out.append(q)
    return out


def kit_piece(F, kind, pid, x, z, yaw, line):
    """A piece from the list, placed: its name and size from the data."""
    P = F["pieces"].get(kind, {"name": kind, "size": [1, 1, 1]})
    w, d, h = P["size"]
    return {"id": pid, "kind": kind, "name": P["name"], "w": w, "d": d, "h": h, "x": x, "z": z, "yaw": yaw, "rug": bool(P.get("rug")), "line": line}


def apply(F, before, edit, belongings=()):
    """A layout changed by an edit {"move": {id: [x, z, yaw]}, "remove": [ids], "add": [[id, kind, x, z, yaw, line]],
    "place": [[belonging id, x, z, yaw]]}."""
    out = []
    for p in before:
        if p["id"] in edit.get("remove", []):
            continue
        q = dict(p)
        if p["id"] in edit.get("move", {}):
            q["x"], q["z"], q["yaw"] = edit["move"][p["id"]]
        out.append(q)
    for pid, kind, x, z, yaw, line in edit.get("add", []):
        out.append(kit_piece(F, kind, pid, x, z, yaw, line))
    mine = {b["id"]: b for b in belongings}
    for bid, x, z, yaw in edit.get("place", []):
        b = mine.get(bid) or {"id": bid, "kind": "town", "name": "Piece", "w": 1.0, "d": 1.0, "h": 1.0, "rug": False}
        out.append(dict(b, x=x, z=z, yaw=yaw))
    return out


def check_script():
    """A fixed run for the slice's port (Prototype/play/builder.js Furnish) to reproduce exactly: one room 6 by 5 m with
    a way in from the door, a bed, a wardrobe and a rug, what the player holds (a sofa), and [ceiling, edit, funds] for
    each case."""
    room, keep = [0.0, 0.0, 6.0, 5.0], [[2.5, 0.0, 3.5, 1.2]]
    before = [
        {"id": "t0", "kind": "town", "name": "Bed", "w": 1.0, "d": 2.0, "h": 0.98, "x": 1.0, "z": 3.0, "yaw": 0, "rug": False},
        {"id": "t1", "kind": "town", "name": "Wardrobe", "w": 1.2, "d": 0.6, "h": 2.18, "x": 4.5, "z": 4.65, "yaw": 0, "rug": False},
        {"id": "t2", "kind": "town", "name": "Rug", "w": 2.0, "d": 1.4, "h": 0.006, "x": 3.0, "z": 2.5, "yaw": 0, "rug": True},
    ]
    cases = [
        [2.6, {"move": {"t0": [1.5, 3.0, 0]}}, None],
        [2.6, {"move": {"t1": [4.5, 4.65, 90]}}, None],
        [2.6, {"add": [["k1", "k17:sofa", 3.0, 2.5, 180, "oldtown"]]}, None],
        [2.6, {"add": [["k1", "k17:sofa", 3.0, 0.6, 0, "plain"]]}, None],
        [2.6, {"move": {"t0": [4.5, 4.0, 0]}}, None],
        [2.6, {"move": {"t0": [1.0, 3.0, 10]}}, None],
        [2.6, {"add": [["k1", "k17:coffee", 1.03, 1.0, 0, "plain"]]}, None],
        [2.6, {"remove": ["t1"]}, None],
        [2.6, {"add": [["k1", "k17:sofa", 3.0, 2.5, 0, "curve"]]}, 10000],
        [2.6, {"add": [["k1", "spaceship", 3.0, 2.5, 0, "plain"]]}, None],
        [2.0, {"add": [["k1", "k17:wardrobe", 5.0, 1.5, 270, "workshop"]]}, None],
        [2.6, {"add": [["k1", "k17:armchair", 4.0, 2.0, 45, "oldtown"]], "move": {"t0": [0.5, 3.0, 0]}, "remove": ["t2"]}, 500000],
    ]
    cases += [
        [2.6, {"place": [["b:b_old_l2:1:k1", 3.0, 2.5, 90]]}, None],
        [2.6, {"place": [["b:nowhere:0:k9", 3.0, 2.5, 0]]}, None],
        [2.6, {"remove": ["t1", "t2"], "place": [["b:b_old_l2:1:k1", 4.5, 4.5, 0]]}, None],
    ]
    belongings = [{"id": "b:b_old_l2:1:k1", "kind": "k17:sofa", "name": "Sofa", "w": 2.0, "d": 0.92, "h": 1.0, "rug": False, "line": "plain"}]
    return dict(room=room, keep=keep, before=before, cases=cases, belongings=belongings, where={"lot": "b_test_l0", "level": 0})


def run_script(F, script):
    """Each case of check_script: the problems, the cost and its lines."""
    out, B = [], script["belongings"]
    for ceiling, edit, funds in script["cases"]:
        after = apply(F, script["before"], edit, B)
        total, lines = cost(F, script["before"], after, B)
        out.append(dict(problems=check(F, script["room"], script["keep"], ceiling, script["before"], after, funds, B), total=total, lines=lines,
                        belongings=stow(script["before"], after, B, script["where"])))
    return out
