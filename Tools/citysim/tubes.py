"""Glass travel tubes (LIVING_CITY CITY-108 to CITY-112; F-VEH-TUBE level 2).      python -m unittest test_tubes -v

A ride is worked out from when it began and the tubes it uses, never stepped (TECH-004): ask where the rider is, or where
the camera is, at any second of the ride and the answer is the same every time, in any order.

Network data (World/<city>/tubes.json, CITY-108):
    stops      {id: {"name", "pos": [x, y], "face": [dx, dy]}}   pos on the ground; face: the way the door looks out
    junctions  {id: {"pos": [x, y, z]}}                          where tubes meet, high over the street
    tubes      [{"a": node id, "b": node id, "line": [[x, y, z], ...]}]
               A tube's line runs from its first node's mouth to its second's: a stop's mouth is straight over its floor at
               ride.draw_in_height_m, a junction's is its pos.
Ride numbers: World/rules/tubes.json.

    problems(net, rules)          what is wrong with the network data, [] when nothing
    crown_top(r, crown, leaf)     how high a tree's crown reaches and how far out, as the game draws it (BrixTreeShapes)
    through_crowns(net, rules, trees)  the level runs that pass through a tree's crown, [] when none
    route(net, a, b)              the shortest way from stop a to stop b: [node ids], or None
    Ride(net, rules, a, b)        .duration, .at(s) -> rider (x, y, z), .lean(s) -> degrees from upright, .camera(s), .phase(s)
"""
import heapq, math


def _d(p, q):
    return math.sqrt(sum((a - b) ** 2 for a, b in zip(p, q)))


def _lerp(p, q, f):
    return tuple(a + (b - a) * f for a, b in zip(p, q))


def _smooth(f):
    f = min(1.0, max(0.0, f))
    return f * f * (3 - 2 * f)


def rounded(line, r, step_deg=10.0):
    """The line with each corner turned on an arc of radius r (smaller where a leg is too short for it): what the tube is
    built along and what a rider follows. Its ends stay where they are."""
    pts = [tuple(p) for p in line]
    if len(pts) < 3 or r <= 0:
        return pts
    out = [pts[0]]
    for i in range(1, len(pts) - 1):
        p, c, q = out[-1], pts[i], pts[i + 1]
        a, b = _d(p, c), _d(c, q)
        if a < 1e-9 or b < 1e-9:
            continue
        u = tuple((x - y) / a for x, y in zip(p, c))           # from the corner back along the leg in
        v = tuple((x - y) / b for x, y in zip(q, c))           # and on along the leg out
        cos = max(-1.0, min(1.0, sum(x * y for x, y in zip(u, v))))
        turn = math.pi - math.acos(cos)                        # how far the line turns here
        if turn < math.radians(1.0):
            out.append(c)
            continue
        t = min(r * math.tan(turn / 2), a * 0.5, b * 0.5)      # how far back along each leg the arc starts
        s0 = tuple(y + x * t for x, y in zip(u, c))
        s1 = tuple(y + x * t for x, y in zip(v, c))
        n = max(2, int(math.ceil(math.degrees(turn) / step_deg)))
        for k in range(n + 1):                                 # a quadratic arc through the corner: tangent to both legs
            f = k / n
            out.append(tuple((1 - f) ** 2 * x + 2 * (1 - f) * f * y + f * f * z for x, y, z in zip(s0, c, s1)))
    out.append(pts[-1])
    return out


def mouth(net, rules, n):
    if n in net["stops"]:
        x, y = net["stops"][n]["pos"]
        return (x, y, rules["ride"]["draw_in_height_m"])
    return tuple(net["junctions"][n]["pos"])


def problems(net, rules):
    out, nodes = [], set(net["stops"]) | set(net.get("junctions", {}))
    for i, t in enumerate(net["tubes"]):
        for end in ("a", "b"):
            if t[end] not in nodes:
                out.append("tube %d: %s is not a stop or a junction" % (i, t[end]))
        if out:
            continue
        line = [tuple(p) for p in t["line"]]
        if _d(line[0], mouth(net, rules, t["a"])) > 0.01 or _d(line[-1], mouth(net, rules, t["b"])) > 0.01:
            out.append("tube %d: does not start and end at its nodes' mouths" % i)
        if min(p[2] for p in line) < rules["clearance_m"]:
            out.append("tube %d: lower than %.1f m" % (i, rules["clearance_m"]))
    for s in net["stops"]:
        if not any(s in (t["a"], t["b"]) for t in net["tubes"]):
            out.append("stop %s: no tube" % s)
    return out


def crown_top(r, crown=None, leaf=1.0):
    """(top, reach): how high a tree of crown radius r reaches over its ground, and how far out its crown goes, as the game
    draws it (BrixStreets.cpp BrixTreeShapes, play.js tree(): the trunk's height grows with r, the crown sits on it, its
    facets jittered up to 1.2 times its radius). crown: None round, "cone" a pine, "slim" a birch."""
    if crown == "cone":
        return 1.1 + r * .35 + r * 3.1 * (1 / 3.6 * 2 + .62), r * 1.05 * 1.2
    if crown == "slim":
        R = .62 * r * (.8 + .2 * leaf)
        return 1.6 + r * 1.05 + R * 1.7 * 1.42, R * 1.2
    R = r * (.8 + .2 * leaf)
    return 1.3 + r * .75 + R * 1.42, R * 1.2


def _seg_dist(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    u = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / (dx * dx + dy * dy or 1e-9)))
    return math.hypot(p[0] - a[0] - u * dx, p[1] - a[1] - u * dy)


def through_crowns(net, rules, trees, clear=0.0):
    """The level runs that pass through (or within clear m of) a tree's crown: [(tube index, tree, crown top)].
    trees: [(x, y, r, crown)], r the crown's radius (a grown tree's: vegetation.json r_max)."""
    out, tr = [], rules["tube_inner_m"] / 2 + .12                # the glass and its rings
    for i, t in enumerate(net["tubes"]):
        for a, b in zip(t["line"], t["line"][1:]):
            if abs(a[2] - b[2]) > .01:
                continue
            for tree in trees:
                top, reach = crown_top(tree[2], tree[3])
                if _seg_dist(tree, a, b) < reach + tr and a[2] - tr < top + clear:
                    out.append((i, tree, top))
    return out


def _edges(net, bend=0.0):
    e = {}
    for t in net["tubes"]:
        line = rounded(t["line"], bend)
        length = sum(_d(p, q) for p, q in zip(line, line[1:]))
        e.setdefault(t["a"], []).append((t["b"], length, line))
        e.setdefault(t["b"], []).append((t["a"], length, line[::-1]))
    return e


def route(net, a, b):
    """Shortest way through the tubes (CITY-112). Junctions are passed through; a ride never stops at another stop."""
    e, best, todo = _edges(net), {a: 0.0}, [(0.0, a, [a])]
    while todo:
        d, n, path = heapq.heappop(todo)
        if n == b:
            return path
        if d > best.get(n, math.inf):
            continue
        if n != a and n in net["stops"]:
            continue                                        # a stop on the way is an end, not a way through
        for m, length, _ in e.get(n, ()):
            if d + length < best.get(m, math.inf):
                best[m] = d + length
                heapq.heappush(todo, (d + length, m, path + [m]))
    return None


class Ride:
    def __init__(self, net, rules, a, b):
        self.r, self.c = rules["ride"], rules["camera"]
        self.nodes = route(net, a, b)
        if self.nodes is None:
            raise ValueError("no way from %s to %s" % (a, b))
        e = _edges(net, self.r.get("bend_r_m", 0.0))
        run = []
        for n, m in zip(self.nodes, self.nodes[1:]):
            line = min((l for k, length, l in e[n] if k == m), key=lambda l: sum(_d(p, q) for p, q in zip(l, l[1:])))
            run += line if not run else line[1:]
        if len(self.nodes) > 2:
            run = rounded(run, self.r.get("junction_bend_r_m", 0.0))   # the turns where one tube meets the next
        self.run = run
        self.cum = [0.0]
        for p, q in zip(run, run[1:]):
            self.cum.append(self.cum[-1] + _d(p, q))
        sa, sb = net["stops"][a], net["stops"][b]
        fl = self.r.get("floor_m", 0.0)                       # the booth's floor
        self.floor_a, self.floor_b = (sa["pos"][0], sa["pos"][1], fl), (sb["pos"][0], sb["pos"][1], fl)
        fa, fb = _unit(sa["face"]), _unit(sb["face"])
        self.face_a, self.face_b = fa, fb
        self.out_b = (self.floor_b[0] + fb[0] * self.r["pop_out_arc_m"], self.floor_b[1] + fb[1] * self.r["pop_out_arc_m"], fl)
        # the run's speed: up to speed_mps over ease_s, then down again; shorter tubes never reach full speed
        L, v, ea = self.cum[-1], self.r["speed_mps"], self.r["ease_s"]
        acc = v / ea
        if acc * ea * ea >= L:                              # a triangle: speed up for half the way, slow down for the rest
            self.te, self.vtop = math.sqrt(L / acc), math.sqrt(L * acc)
            self.tc = 0.0
        else:
            self.te, self.vtop = ea, v
            self.tc = (L - acc * ea * ea) / v
        self.acc = acc
        self.t_run = 2 * self.te + self.tc
        self.t0, self.t1 = self.r["draw_in_s"], self.r["draw_in_s"] + self.t_run
        self.duration = self.t1 + self.r["pop_out_s"]

    def phase(self, s):
        return "draw_in" if s < self.t0 else "run" if s < self.t1 else "pop_out" if s < self.duration else "out"

    def _run_dist(self, u):
        u = min(max(u, 0.0), self.t_run)
        a, te, tc = self.acc, self.te, self.tc
        if u < te:
            return 0.5 * a * u * u
        if u < te + tc:
            return 0.5 * a * te * te + self.vtop * (u - te)
        w = self.t_run - u
        return self.cum[-1] - 0.5 * a * w * w

    def _on_run(self, d):
        i = 0
        while i < len(self.cum) - 2 and self.cum[i + 1] < d:
            i += 1
        seg = self.cum[i + 1] - self.cum[i]
        return _lerp(self.run[i], self.run[i + 1], (d - self.cum[i]) / seg if seg > 1e-9 else 0.0)

    def at(self, s):
        """Where the rider is, s seconds after stepping into the stop."""
        if s <= 0:
            return self.floor_a
        if s < self.t0:                                     # sucked up: slow off the floor, fast into the tube
            f = s / self.t0
            return _lerp(self.floor_a, self.run[0], f * f)
        if s < self.t1:
            return self._on_run(self._run_dist(s - self.t0))
        if s < self.duration:                               # shot out and down, landing on their feet past the door
            f = (s - self.t1) / self.r["pop_out_s"]
            g = 1 - (1 - f) ** 2
            down = _lerp(self.run[-1], self.floor_b, min(1.0, g * 1.25))
            return (down[0] + (self.out_b[0] - self.floor_b[0]) * _smooth(f),
                    down[1] + (self.out_b[1] - self.floor_b[1]) * _smooth(f), down[2])
        return self.out_b

    def lean(self, s):
        """How far the rider leans from upright, in degrees (CITY-109): 0 going up or down, 90 lying along a level tube,
        head first. Read from the tube's own direction over a short stretch either side (turn_window_m), so it turns
        smoothly through each bend; upright while drawn in and popped out, which go straight up and down."""
        if self.phase(s) != "run":
            return 0.0
        d = self._run_dist(s - self.t0)
        w = min(self.r["turn_window_m"], d, self.cum[-1] - d)   # narrower at the ends, which go straight up and down
        a, b = self._on_run(d - w), self._on_run(d + w)
        flat = math.hypot(b[0] - a[0], b[1] - a[1])
        return math.degrees(math.atan2(flat, abs(b[2] - a[2])))

    def _behind(self, p, face):
        """The everyday third-person camera behind someone who looks the way `face` does."""
        return (p[0] - face[0] * self.c["behind_m"], p[1] - face[1] * self.c["behind_m"], p[2] + self.c["above_m"])

    def _path(self):
        """The rider's whole way, booth floor to booth floor: up out of the first booth, the run, down into the last."""
        if not hasattr(self, "_full"):
            pts = [self.floor_a] + self.run + [self.floor_b, self.out_b]
            cum = [0.0]
            for p, q in zip(pts, pts[1:]):
                cum.append(cum[-1] + _d(p, q))
            self._full = (pts, cum, _d(self.floor_a, self.run[0]), _d(self.run[-1], self.floor_b) + _d(self.floor_b, self.out_b))
        return self._full

    def _along(self, s):
        """How far along _path() the rider is at s."""
        pts, cum, up, down = self._path()
        if s <= 0:
            return 0.0
        if s < self.t0:
            return _d(self.floor_a, self.at(s))
        if s < self.t1:
            return up + self._run_dist(s - self.t0)
        return up + self.cum[-1] + down * min(1.0, (s - self.t1) / self.r["pop_out_s"])

    def _on_path(self, d):
        pts, cum, _, _ = self._path()
        d = min(max(d, 0.0), cum[-1])
        i = 0
        while i < len(cum) - 2 and cum[i + 1] < d:
            i += 1
        seg = cum[i + 1] - cum[i]
        return _lerp(pts[i], pts[i + 1], (d - cum[i]) / seg if seg > 1e-9 else 0.0)

    def _sideways(self, first):
        """Across the first (or last) level stretch of the run: along the street the booth stands by."""
        seq = zip(self.run, self.run[1:]) if first else zip(self.run[::-1][1:], self.run[::-1])
        for p, q in seq:
            fx, fy = q[0] - p[0], q[1] - p[1]
            n = math.hypot(fx, fy)
            if n > 0.05:
                return (-fy / n, fx / n)
        return (1.0, 0.0)

    def camera(self, s):
        """Where the camera is (CITY-110), always outside the glass. Along a level tube it chases the rider behind_m back
        along their way and above_m over it; where the way goes straight up or down (out of the first booth, into the
        last) it stands behind_m to the side, along the street, at the rider's height and above_m over, as if on a crane.
        Between the two it moves as the way turns, so it never cuts."""
        d = self._along(s)
        _, cum, _, _ = self._path()
        dc = max(0.0, d - self.c["behind_m"])
        t = self._on_path(dc)
        rider = self.at(s)
        on = self._on_path(min(d, cum[-2]))                  # the rider on their way, short of the step out of the booth
        flat, rise = math.hypot(on[0] - t[0], on[1] - t[1]), abs(on[2] - t[2])   # the way from the camera's point to the rider
        w = flat / math.hypot(flat, rise) if flat + rise > 1e-9 else 0.0               # 1 along a level tube, 0 up or down
        side = self._sideways(d < cum[-1] / 2)
        k = (1 - w) * self.c["behind_m"]
        return (t[0] + side[0] * k, t[1] + side[1] * k, t[2] * w + rider[2] * (1 - w) + self.c["above_m"])


def _unit(v):
    n = math.hypot(v[0], v[1])
    return (v[0] / n, v[1] / n)


def _lift(p, h):
    return (p[0], p[1], p[2] + h)
