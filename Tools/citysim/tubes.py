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


def _edges(net):
    e = {}
    for t in net["tubes"]:
        line = [tuple(p) for p in t["line"]]
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
        e = _edges(net)
        run = []
        for n, m in zip(self.nodes, self.nodes[1:]):
            line = min((l for k, length, l in e[n] if k == m), key=lambda l: sum(_d(p, q) for p, q in zip(l, l[1:])))
            run += line if not run else line[1:]
        self.run = run
        self.cum = [0.0]
        for p, q in zip(run, run[1:]):
            self.cum.append(self.cum[-1] + _d(p, q))
        sa, sb = net["stops"][a], net["stops"][b]
        self.floor_a, self.floor_b = (sa["pos"][0], sa["pos"][1], 0.0), (sb["pos"][0], sb["pos"][1], 0.0)
        fa, fb = _unit(sa["face"]), _unit(sb["face"])
        self.face_a, self.face_b = fa, fb
        self.out_b = (self.floor_b[0] + fb[0] * self.r["pop_out_arc_m"], self.floor_b[1] + fb[1] * self.r["pop_out_arc_m"], 0.0)
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
        w = self.r["turn_window_m"]
        a, b = self._on_run(max(0.0, d - w)), self._on_run(min(self.cum[-1], d + w))
        flat = math.hypot(b[0] - a[0], b[1] - a[1])
        return math.degrees(math.atan2(flat, abs(b[2] - a[2])))

    def _behind(self, p, face):
        """The everyday third-person camera behind someone who looks the way `face` does."""
        return (p[0] - face[0] * self.c["behind_m"], p[1] - face[1] * self.c["behind_m"], p[2] + self.c["above_m"])

    def camera(self, s):
        """Where the camera is (CITY-110): it starts as the everyday camera behind the rider walking in, rides the rider's
        own path a little behind them, and settles into the everyday camera behind them as they step out. Never a cut."""
        back = self.c["follow_s"]
        start = self._behind(self.floor_a, (-self.face_a[0], -self.face_a[1]))   # walking in, facing into the stop
        if s <= 0:
            return start
        if s < back:                                        # from the everyday camera onto the rider's path
            return _lerp(start, _lift(self.at(0), self.c["above_m"]), _smooth(s / back))
        trail = _lift(self.at(s - back), self.c["above_m"])
        if s < self.t1:
            return trail
        end = self._behind(self.out_b, self.face_b)
        f = _smooth((s - self.t1) / (self.duration - self.t1 + back))
        return _lerp(trail, end, f) if f < 1 else end


def _unit(v):
    n = math.hypot(v[0], v[1])
    return (v[0] / n, v[1] / n)


def _lift(p, h):
    return (p[0], p[1], p[2] + h)
