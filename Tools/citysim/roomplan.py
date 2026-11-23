"""The room plan of a storey in the building designer (docs/specs/domains/BUILDER.md BUILD-021 to BUILD-023; F-BLD-DESIGN
level 2). Rules in World/rules/room_plan.json.

A plan is a list of inner walls, each

    {"id": "w1", "ax": "z", "fix": 4.0, "a0": 0.0, "a1": 6.0, "door": 3.0, "colour": None}

ax "x": the wall runs along x at z = fix, from x = a0 to a1; ax "z": along z at x = fix. `door` is the middle of its one
doorway along the wall (None: no doorway); `colour` a kit colour for both its faces (None: the storey's own plaster).

    problems = roomplan.check(P, room, keep, entries, height, colours, before, after, funds)   # [] when it may be built
    total, lines = roomplan.cost(P, before, after, height)                                     # chips, line by line

`room` is the storey's floor inside its outer walls [x0, z0, x1, z1]; `keep` the rectangles to keep clear (the stairwell,
the way in from the door); `entries` the points that must be reached from the first (the way in, then the foot of the
stairs). `colours` the colours the district allows inside (builder.options "inside.walls"). As for the rest of the designer
a plan is refused only for what the change makes wrong. Nothing is stored or ticked here.
"""
import math

TOL = 1e-6


def _q(x):
    return math.floor(x * 100 + .5) / 100


def _chips(x):
    return int(math.floor(x + .5))


def box(P, w):
    """The wall's footprint [x0, z0, x1, z1] (doorway included)."""
    h = P["thick"] / 2
    return [w["a0"], w["fix"] - h, w["a1"], w["fix"] + h] if w["ax"] == "x" else [w["fix"] - h, w["a0"], w["fix"] + h, w["a1"]]


def _meet(a, b):
    return a[0] < b[2] - TOL and b[0] < a[2] - TOL and a[1] < b[3] - TOL and b[1] < a[3] - TOL


def _blocked(P, room, walls):
    """Which 0.25 m cells of the floor a wall stands in (its doorway left open)."""
    c, h = P["cell"], P["thick"] / 2 + P["cell"] / 2
    nx, nz = int(math.floor((room[2] - room[0]) / c + TOL)), int(math.floor((room[3] - room[1]) / c + TOL))
    out = set()
    for w in walls:
        for i in range(nx):
            for j in range(nz):
                x, z = room[0] + (i + .5) * c, room[1] + (j + .5) * c
                along, across = (x, z) if w["ax"] == "x" else (z, x)
                if abs(across - w["fix"]) < h - TOL and w["a0"] - c / 2 < along < w["a1"] + c / 2:
                    if w["door"] is None or abs(along - w["door"]) > P["door_w"] / 2 - c / 2 + TOL:
                        out.add((i, j))
    return nx, nz, out


def reach(P, room, walls, entries):
    """From the first entry: which entries can be walked to, and the area (m2) of each part of the floor that cannot."""
    c = P["cell"]
    nx, nz, bl = _blocked(P, room, walls)
    cell = lambda p: (min(nx - 1, max(0, int((p[0] - room[0]) / c))), min(nz - 1, max(0, int((p[1] - room[1]) / c))))
    start, seen = cell(entries[0]), set()
    if start not in bl:
        todo = [start]
        seen.add(start)
        while todo:
            i, j = todo.pop()
            for q in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
                if 0 <= q[0] < nx and 0 <= q[1] < nz and q not in bl and q not in seen:
                    seen.add(q)
                    todo.append(q)
    got = [cell(p) in seen for p in entries]
    left, pockets = {(i, j) for i in range(nx) for j in range(nz)} - bl - seen, []
    while left:
        a = left.pop()
        todo, n = [a], 1
        while todo:
            i, j = todo.pop()
            for q in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
                if q in left:
                    left.discard(q)
                    todo.append(q)
                    n += 1
        pockets.append(round(n * c * c, 4))
    return got, sorted(pockets, reverse=True)


def problems(P, room, keep, entries, colours, walls):
    """Everything wrong with a plan: [key, message], the key naming the wall (or walls) and what is wrong."""
    out = []
    say = lambda k, m: out.append([k, m])
    g = P["grid_m"]
    for w in walls:
        k = w["id"] + "|"
        if w["ax"] not in ("x", "z"):
            say(k + "ax", "a wall must run along x or z")
            continue
        if w["a1"] - w["a0"] < P["min_len"] - TOL:
            say(k + "short", "a wall is shorter than %.1f m" % P["min_len"])
        if any(abs(v / g - round(v / g)) > 1e-6 for v in (w["fix"], w["a0"], w["a1"]) + ((w["door"],) if w["door"] is not None else ())):
            say(k + "grid", "a wall is off the %d cm grid" % round(g * 100))
        lo, hi, flo, fhi = (room[0], room[2], room[1], room[3]) if w["ax"] == "x" else (room[1], room[3], room[0], room[2])
        if w["a0"] < lo - TOL or w["a1"] > hi + TOL or not flo + TOL < w["fix"] < fhi - TOL:
            say(k + "room", "a wall runs out of the room")
        if w["door"] is not None and not (w["a0"] + P["door_w"] / 2 + P["door_margin"] - TOL <= w["door"] <= w["a1"] - P["door_w"] / 2 - P["door_margin"] + TOL):
            say(k + "door", "a doorway does not fit in its wall")
        if w["colour"] is not None and w["colour"] not in colours:
            say(k + "colour", "that wall colour is not one of the district's")
        if any(_meet(box(P, w), r) for r in keep):
            say(k + "keep", "a wall stands in the way of the door or the stairs")
    ok = [w for w in walls if w["ax"] in ("x", "z")]
    for i, a in enumerate(ok):
        for b in ok[i + 1:]:
            if _meet(box(P, a), box(P, b)) and a["ax"] == b["ax"]:
                say("|".join(sorted([a["id"], b["id"]])) + "|clash", "two walls stand in each other")
    got, pockets = reach(P, room, ok, entries)
    if not all(got[1:]):
        say("reach|stairs", "the stairs cannot be reached from the way in")
    if not got[0]:
        say("reach|door", "a wall stands where you come in")
    if any(a >= P["pocket_m2"] - TOL for a in pockets):
        say("reach|room", "a room cannot be reached from the way in")
    return out


def cost(P, before, after, height):
    """What the change costs: new, moved and taken-down walls by the area of one side, doorways, and paint. [what, m2 or 1, chips]."""
    R, was, now, lines = P["prices"], {w["id"]: w for w in before}, {w["id"] for w in after}, []
    area = lambda w: _q((w["a1"] - w["a0"]) * height)
    for w in after:
        o = was.get(w["id"])
        if o is None:
            lines.append(["New wall", area(w), _chips(area(w) * R["wall_m2"])])
            if w["door"] is not None:
                lines.append(["New doorway", 1, R["doorway"]])
            if w["colour"] is not None:
                lines.append(["Paint a wall", _q(area(w) * 2), _chips(area(w) * 2 * R["paint_m2"])])
            continue
        if (o["ax"], o["fix"], o["a0"], o["a1"]) != (w["ax"], w["fix"], w["a0"], w["a1"]):
            lines.append(["Move a wall", area(w), _chips(area(w) * R["move_m2"])])
        if o["door"] != w["door"]:
            lines.append(["New doorway", 1, R["doorway"]] if w["door"] is not None else ["Close a doorway", 1, R["close_doorway"]])
        if o["colour"] != w["colour"]:
            lines.append(["Paint a wall", _q(area(w) * 2), _chips(area(w) * 2 * R["paint_m2"])])
    for w in before:
        if w["id"] not in now:
            lines.append(["Take down a wall", area(w), _chips(area(w) * R["take_down_m2"])])
    return sum(c for _, _, c in lines), lines


def check(P, room, keep, entries, height, colours, before, after, funds=None):
    """[] when the plan may be built: nothing the change makes wrong (BUILD-021, BUILD-022), and the player can pay."""
    old = {k for k, _ in problems(P, room, keep, entries, colours, before)}
    out = list(dict.fromkeys(m for k, m in problems(P, room, keep, entries, colours, after) if k not in old))
    total = cost(P, before, after, height)[0]
    if funds is not None and total > funds:
        out.append("you cannot pay %d.%02d brix" % (total // 100, total % 100))
    return out


def apply(before, edit):
    """A plan changed by an edit {"set": {id: {field: value}}, "remove": [ids], "add": [wall]}."""
    out = []
    for w in before:
        if w["id"] in edit.get("remove", []):
            continue
        out.append(dict(w, **edit.get("set", {}).get(w["id"], {})))
    return out + [dict(w) for w in edit.get("add", [])]


def check_script():
    """A fixed run for the slice's port (builder.js RoomPlan): a storey 8 by 6 m with the way in at the front and the stairs
    at the back right, a wall across it with a doorway and a second one closing off the back right, and [edit, funds]."""
    room, keep = [0.0, 0.0, 8.0, 6.0], [[3.5, 0.0, 4.5, 1.2], [6.5, 4.5, 8.0, 6.0]]
    entries = [[4.0, 0.6], [7.0, 4.2]]
    W = lambda i, ax, fix, a0, a1, door=None, colour=None: {"id": i, "ax": ax, "fix": fix, "a0": a0, "a1": a1, "door": door, "colour": colour}
    before = [W("w1", "z", 5.0, 0.0, 6.0, 3.0), W("w2", "x", 3.0, 0.0, 5.0, 2.5)]
    cases = [
        [{"set": {"w1": {"fix": 5.5}}}, None],
        [{"set": {"w1": {"door": None}}}, None],
        [{"set": {"w2": {"door": None}}}, None],
        [{"remove": ["w2"]}, None],
        [{"add": [W("w3", "x", 2.0, 5.0, 8.0, 6.5)]}, None],
        [{"add": [W("w3", "z", 2.0, 0.0, 0.4)]}, None],
        [{"add": [W("w3", "z", 2.03, 3.0, 6.0)]}, None],
        [{"set": {"w2": {"door": 0.3}}}, None],
        [{"add": [W("w3", "z", 4.0, 0.0, 2.0)]}, None],
        [{"set": {"w2": {"colour": "apricot"}}}, None],
        [{"set": {"w2": {"colour": "spaceship"}}}, None],
        [{"add": [W("w3", "x", 4.0, 5.0, 8.0, 7.0, "putty")]}, 1000],
        [{"add": [W("w3", "z", 5.0, 3.5, 6.0)]}, None],
    ]
    return dict(room=room, keep=keep, entries=entries, height=2.8, colours=["limewash", "apricot", "putty"], before=before, cases=cases)


def run_script(P, S):
    """Each case of check_script: the problems, the cost and its lines."""
    out = []
    for edit, funds in S["cases"]:
        after = apply(S["before"], edit)
        total, lines = cost(P, S["before"], after, S["height"])
        out.append(dict(problems=check(P, S["room"], S["keep"], S["entries"], S["height"], S["colours"], S["before"], after, funds), total=total, lines=lines))
    return out
