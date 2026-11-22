"""Brix Town - residents' cars on the roads (docs/specs/domains/LIVING_CITY.md 3.4, CITY-043 to CITY-049; ADR-0009).

The same rule as the rest of the city: a resident car's position at any moment is a lookup, not a step.
When a day's plan is made, every car trip of that day is worked out once, in order, from the residents'
routines and the seed: routes, waits at junctions, queues behind other cars, which bay each car takes.
After that, `car_at(car, day, minute)` is a pure lookup and gives the same answer on every machine.

    Roads      the road network from World/<city>/roads.json: lanes, junction boxes, bays, crossings; routes
    Traffic    who owns a car, which trips are driven, the day's car plan, and the residents' changed days

Units: metres; the plan runs in real seconds (one game minute lasts `pace` real seconds, as in citysim).
Standard library only, like citysim.py.
"""
import bisect, heapq, math

from citysim import dist, hm, lengths, rnd


# ---- the road network --------------------------------------------------------------------------------------
class Roads:
    def __init__(self, data, rules):
        self.d, self.r = data, rules
        self.j = {j["id"]: j for j in data["junctions"]}
        self.streets = {s["id"]: s for s in data["streets"]}
        self.lanes, self.out, self.inn = {}, {}, {}
        sl, off = rules["stop_line"], rules["lane_offset"]
        for s in data["streets"]:
            if s["a"] not in self.j or s["b"] not in self.j:
                continue                                   # reported by problems()
            for sign in (1, -1):
                a, b = (s["a"], s["b"]) if sign > 0 else (s["b"], s["a"])
                pa, pb = self.j[a]["pos"], self.j[b]["pos"]
                L = dist(pa, pb)
                u = ((pb[0] - pa[0]) / L, (pb[1] - pa[1]) / L)
                n = (-u[1], u[0])                          # right of travel (x east, z south)
                lid = s["id"] + ("+" if sign > 0 else "-")
                self.lanes[lid] = dict(id=lid, street=s["id"], a=a, b=b, u=u, n=n, length=L - 2 * sl, street_length=L,
                                       start=(pa[0] + u[0] * sl + n[0] * off, pa[1] + u[1] * sl + n[1] * off),
                                       end=(pb[0] - u[0] * sl + n[0] * off, pb[1] - u[1] * sl + n[1] * off),
                                       v=s["limit_kmh"] / 3.6, axis="ew" if abs(u[0]) > 0.5 else "ns")
                self.out.setdefault(a, []).append(lid)
                self.inn.setdefault(b, []).append(lid)
        # town gates: a lane out of town ("<gate>+", ends off the map) and one back in ("<gate>-") (CITY-068)
        self.gates = {}
        for g in data.get("gates", []):
            if g["junction"] not in self.j:
                continue
            p, u = self.j[g["junction"]]["pos"], tuple(g["dir"])
            L = g["length"]
            for sign in (1, -1):
                uu = u if sign > 0 else (-u[0], -u[1])
                n = (-uu[1], uu[0])
                if sign > 0:                               # junction -> off the map
                    a, b, start, end = g["junction"], g["id"], (p[0] + u[0] * sl, p[1] + u[1] * sl), (p[0] + u[0] * L, p[1] + u[1] * L)
                else:                                      # off the map -> junction
                    a, b, start, end = g["id"], g["junction"], (p[0] + u[0] * L, p[1] + u[1] * L), (p[0] + u[0] * sl, p[1] + u[1] * sl)
                lid = g["id"] + ("+" if sign > 0 else "-")
                self.lanes[lid] = dict(id=lid, street=g["id"], a=a, b=b, u=uu, n=n, length=L - sl, street_length=L, gate=g["id"],
                                       start=(start[0] + n[0] * off, start[1] + n[1] * off), end=(end[0] + n[0] * off, end[1] + n[1] * off),
                                       v=g["limit_kmh"] / 3.6, axis="ew" if abs(uu[0]) > 0.5 else "ns")
                self.out.setdefault(a, []).append(lid)
                self.inn.setdefault(b, []).append(lid)
            self.gates[g["id"]] = dict(g, out=g["id"] + "+", back=g["id"] + "-")
        self.bays, self.by_lane = {}, {}
        for b in data["bays"]:
            lid = b["street"] + ("+" if b["side"] > 0 else "-")
            if lid not in self.lanes:
                continue
            lane = self.lanes[lid]
            s = (b["at"] if b["side"] > 0 else lane["street_length"] - b["at"]) - sl
            self.bays[b["id"]] = dict(b, lane=lid, s=s, pos=tuple(b["pos"]))
            self.by_lane.setdefault(lid, []).append(b["id"])
        self._grid = {}                                   # bays by 32 m cell, for "nearest bay"
        for bid, b in self.bays.items():
            self._grid.setdefault((int(b["pos"][0] // 32), int(b["pos"][1] // 32)), []).append(bid)

    # -- junction boxes: the way across a junction from one lane to the next --
    def movements(self, lid):
        """Lanes a car on `lid` may turn into at its end junction. No U-turns."""
        lane = self.lanes[lid]
        return [o for o in self.out.get(lane["b"], []) if self.lanes[o]["street"] != lane["street"]]

    def box(self, a, b):
        la, lb = self.lanes[a], self.lanes[b]
        return dict(kind="box", key=(la["b"], a, b), junction=la["b"], p0=la["end"], p1=lb["start"], length=dist(la["end"], lb["start"]),
                    v=min(la["v"], lb["v"]), into=b, frm=a, axis=la["axis"],
                    straight=abs(la["u"][0] * lb["u"][0] + la["u"][1] * lb["u"][1]) > 0.99)

    def route(self, lid0, s0, lid1, s1):
        """Quickest way by the speed limits from a point on one lane to a point on another: a list of legs."""
        L0 = self.lanes[lid0]
        if lid0 == lid1 and s1 > s0 + 0.5:
            return [dict(kind="lane", key=lid0, s0=s0, s1=s1, v=L0["v"])]
        best, prev, heap = {}, {}, []
        for o in self.movements(lid0):
            t = (L0["length"] - s0) / L0["v"] + self.box(lid0, o)["length"] / self.box(lid0, o)["v"]
            heapq.heappush(heap, (t, o, lid0))
        while heap:
            t, lid, frm = heapq.heappop(heap)
            if lid in best:
                continue
            best[lid], prev[lid] = t, frm
            if lid == lid1:
                break
            lane = self.lanes[lid]
            for o in self.movements(lid):
                if o not in best:
                    bx = self.box(lid, o)
                    heapq.heappush(heap, (t + lane["length"] / lane["v"] + bx["length"] / bx["v"], o, lid))
        if lid1 not in best:
            return None
        chain = [lid1, prev[lid1]]
        while chain[-1] != lid0:
            chain.append(prev[chain[-1]])
        chain.reverse()                                   # lid0, ..., lid1
        legs = [dict(kind="lane", key=lid0, s0=s0, s1=L0["length"], v=L0["v"])]
        for a, b in zip(chain, chain[1:]):
            bx = self.box(a, b)
            legs.append(dict(bx, s0=0.0, s1=bx["length"]))
            last = (a, b) == (chain[-2], chain[-1])
            legs.append(dict(kind="lane", key=b, s0=0.0, s1=s1 if last else self.lanes[b]["length"],
                             v=self.lanes[b]["v"]))
        return legs

    def point(self, leg, s):
        if leg["kind"] == "lane":
            lane = self.lanes[leg["key"]]
            return (lane["start"][0] + lane["u"][0] * s, lane["start"][1] + lane["u"][1] * s), lane["u"]
        f = s / leg["length"] if leg["length"] > 1e-9 else 0.0
        p0, p1 = leg["p0"], leg["p1"]
        h = (p1[0] - p0[0], p1[1] - p0[1])
        n = math.hypot(*h) or 1.0
        return (p0[0] + h[0] * f, p0[1] + h[1] * f), (h[0] / n, h[1] / n)

    def bays_near(self, p, reach):
        """Bay ids within `reach` metres of p, nearest first (ties by id)."""
        cx, cz, k = int(p[0] // 32), int(p[1] // 32), int(reach // 32) + 1
        out = []
        for i in range(cx - k, cx + k + 1):
            for j in range(cz - k, cz + k + 1):
                for bid in self._grid.get((i, j), ()):
                    d = dist(p, self.bays[bid]["pos"])
                    if d <= reach:
                        out.append((d, bid))
        return [bid for d, bid in sorted(out)]

    def problems(self, lots):
        """CITY-043: what the road data must keep. Plain-word problems, [] when sound."""
        r, out = self.r, []
        for s in self.d["streets"]:
            for end in ("a", "b"):
                if s[end] not in self.j:
                    out.append("street %s: junction %s does not exist" % (s["id"], s[end]))
        if out:
            return out
        start = next(iter(self.lanes))                     # every lane reachable from every other, forwards and back
        for name, step in (("reached", lambda l: self.movements(l)),
                           ("left", lambda l: [i for i in self.inn.get(self.lanes[l]["a"], []) if self.lanes[i]["street"] != self.lanes[l]["street"]])):
            seen, todo = {start}, [start]
            while todo:
                for o in step(todo.pop()):
                    if o not in seen:
                        seen.add(o), todo.append(o)
            # a gate's way out ends off the map and its way in starts there: each is checked in one direction only
            skip = {g["back"] if name == "reached" else g["out"] for g in self.gates.values()}
            out += ["lane %s cannot be %s from the rest of the roads" % (l, name) for l in sorted(set(self.lanes) - seen - skip)]
        for g in self.gates.values():
            if not self.movements(g["back"]):
                out.append("%s leads nowhere into town" % g["name"])
        arms = {}
        for lane in self.lanes.values():
            arms.setdefault(lane["a"], set()).add(lane["street"])
        crossings = {(c["street"], c["junction"]) for c in self.d["crossings"]}
        for jid, j in self.j.items():
            n = len(arms.get(jid, ()))
            if n >= 3 and j["control"] not in ("signals", "priority", "give_way"):
                out.append("junction %s has %d arms but no control" % (jid, n))
            if j["control"] == "priority" and j.get("main") not in ("ns", "ew"):
                out.append("junction %s gives priority but names no main road" % jid)
            for st in arms.get(jid, ()):
                if (st, jid) not in crossings:
                    out.append("junction %s has no crossing on %s" % (jid, st))
        seen = {}
        for b in self.d["bays"]:
            s = self.streets.get(b["street"])
            if s is None:
                out.append("bay %s is on a street that does not exist" % b["id"])
                continue
            L = self.lanes[b["street"] + "+"]["street_length"]
            if not (r["bay_clear_of_junction"] - 0.01 <= b["at"] - r["bay_length"] / 2 and b["at"] + r["bay_length"] / 2 <= L - r["bay_clear_of_junction"] + 0.01):
                out.append("bay %s is too close to a junction" % b["id"])
            pa, pb = self.j[s["a"]]["pos"], self.j[s["b"]]["pos"]
            u = ((pb[0] - pa[0]) / L, (pb[1] - pa[1]) / L)
            want = (pa[0] + u[0] * b["at"] - u[1] * r["bay_offset"] * b["side"], pa[1] + u[1] * b["at"] + u[0] * r["bay_offset"] * b["side"])
            if dist(want, b["pos"]) > 0.05:
                out.append("bay %s is not in the lay-by beside its street" % b["id"])
            for other in seen.get((b["street"], b["side"]), []):
                if abs(other - b["at"]) < r["bay_length"] - 0.01:
                    out.append("bay %s overlaps another bay" % b["id"])
            seen.setdefault((b["street"], b["side"]), []).append(b["at"])
        for l in lots:
            if l["kind"] in ("garden", "market", "stop"):
                continue
            if not self.bays_near(l["door"], r["bay_reach_m"]):
                out.append("%s has no parking bay within %d m of its door" % (l["id"], r["bay_reach_m"]))
        return out


# ---- the traffic ----------------------------------------------------------------------------------------------
class Traffic:
    def __init__(self, city, roads_data, rules):
        self.city, self.r = city, rules
        self.roads = Roads(roads_data, rules)
        self.pace = city.pace                             # real seconds per game minute
        self.step = rules["step_s"]
        self._days, self._walk, self._crossers = {}, {}, {}
        self._lines()
        self.cars = self._cars()
        self.vans = self._vans()
        self.home_bays = {c["bay"] for c in list(self.cars.values()) + list(self.vans.values())}
        city.traffic = self

    # -- every vehicle: residents' cars, vans, and cars passing through (those exist only on their day) --
    def vehicle(self, vid):
        return self.cars.get(vid) or self.vans.get(vid) or dict(id=vid, home=None, bay=None, owner=None)

    def home_bay(self, vid):
        return self.vehicle(vid)["bay"]

    def ids(self, day):
        return sorted(set(self.cars) | set(self.vans) | set(self._day(day)["trips"]))

    def _vans(self):
        """Delivery vans (CITY-040), each kept at a depot: a utility lot you can enter."""
        v = self.r["vans"]
        depots = sorted(l for l, lot in self.city.lots.items() if lot["kind"] == "utility" and lot.get("enterable"))
        taken = {c["bay"] for c in self.cars.values()}
        vans = {}
        for n in range(v["count"] if depots else 0):
            home = depots[n % len(depots)]
            bay = next((b for b in self.roads.bays_near(self.city.lots[home]["door"], 10 * self.r["bay_reach_m"]) if b not in taken), None)
            if bay is None:
                continue
            taken.add(bay)
            vans["van_%02d" % n] = dict(id="van_%02d" % n, home=home, bay=bay, owner=None, kind="van")
        return vans

    def _fleet_legs(self, day):
        """The day's van rounds (depot, shops, depot, again) and cars passing through town from gate to gate (CITY-069).
        All drawn from the seed: the same on every machine."""
        out, v, th = {}, self.r["vans"], self.r["through"]
        shops = sorted(l for l, lot in self.city.lots.items() if lot["kind"] in ("shop", "food"))
        for vid, van in sorted(self.vans.items()):
            legs = []
            for rd in range(v["rounds"]):
                stops = [shops[int(rnd("van", vid, day, rd, k) * len(shops))] for k in range(v["stops"])]
                for k, to in enumerate(stops + [van["home"]]):
                    first = not legs
                    legs.append(dict(kind="van", to=to, at=hm(v["first"]) + rnd("van0", vid, day) * 30 if first else None,
                                     wait=v["depot_min"] if (k == 0 and not first) else v["dwell_min"]))
            out[vid] = legs
        gates = sorted(self.roads.gates)
        if len(gates) >= 2:
            for h, n in enumerate(th["per_hour"]):
                for i in range(n):
                    a = gates[int(rnd("thru_in", day, h, i) * len(gates))]
                    rest = [g for g in gates if g != a]
                    b = rest[int(rnd("thru_out", day, h, i) * len(rest))]
                    out["thru_%02d_%02d" % (h, i)] = [dict(kind="pass", gate_in=a, gate=b, at=h * 60 + (i + rnd("thru_t", day, h, i)) * 60.0 / n)]
        return out

    # -- who has a car --
    def _cars(self):
        no = set(self.r["no_drive_roles"])
        cars, taken = {}, set()
        for home in sorted(self.city.mates):
            mates = self.city.mates[home]
            drivers = [rid for rid in mates if self.city.res[rid]["role"] not in no]
            if not drivers or rnd("car", home) >= self.r["car_share"]:
                continue
            door = self.city.lots[home]["door"]
            bay = next((b for b in self.roads.bays_near(door, 10 * self.r["bay_reach_m"]) if b not in taken), None)
            if bay is None:
                continue
            taken.add(bay)
            cars["car_" + home] = dict(id="car_" + home, home=home, owner=drivers[0], bay=bay)
        return cars

    # -- walking between a door and a bay, along the same streets people use --
    def _lines(self):
        """Walking-network nodes on each pavement line (`walk_offset` either side of every street), sorted, so a bay
        finds its stretch of pavement quickly."""
        off = self.r["walk_offset"]
        xs = {j["pos"][0] + s * off for j in self.roads.j.values() for s in (-1, 1)}
        zs = {j["pos"][1] + s * off for j in self.roads.j.values() for s in (-1, 1)}
        self._ns, self._ew = {x: [] for x in xs}, {z: [] for z in zs}
        for p in self.city.net.adj:
            if p[0] in self._ns:
                self._ns[p[0]].append(p[1])
            if p[1] in self._ew:
                self._ew[p[1]].append(p[0])
        for d in (self._ns, self._ew):
            for k in d:
                d[k].sort()

    def _kerb(self, bid):
        """The point on the pavement beside a bay (its own side of the street), and the walking nodes either side of it."""
        b = self.roads.bays[bid]
        lane = self.roads.lanes[b["lane"]]
        base = self.roads.j[self.roads.streets[b["street"]]["a"]]["pos"]
        off = self.r["walk_offset"]
        if lane["axis"] == "ns":
            x = base[0] + (off if b["pos"][0] > base[0] else -off)
            zs = self._ns[x]
            c = (x, b["pos"][1])
            i = bisect.bisect_left(zs, c[1])
            ends = [(x, zs[k]) for k in (i - 1, i) if 0 <= k < len(zs)]
        else:
            z = base[1] + (off if b["pos"][1] > base[1] else -off)
            xs = self._ew[z]
            c = (b["pos"][0], z)
            i = bisect.bisect_left(xs, c[0])
            ends = [(xs[k], z) for k in (i - 1, i) if 0 <= k < len(xs)]
        return c, ends

    def walk(self, place, spot, bid, to_bay=True):
        """Route on foot between a spot in a place and a bay: out of the door, along the street, to the car."""
        key = (place, tuple(spot), bid)
        if key not in self._walk:
            anchor = self.city.anchor[place]
            c, ends = self._kerb(bid)
            dd, _ = self.city.net._from(anchor)
            node = min((e for e in ends if e in dd), key=lambda e: dd[e] + dist(e, c))
            path, _ = self.city.net.path(anchor, node)
            route = [tuple(spot)] + path + [c, self.roads.bays[bid]["pos"]]
            route = [route[0]] + [p for i, p in enumerate(route[1:]) if dist(p, route[i]) > 1e-6]
            self._walk[key] = route
        route = self._walk[key]
        return route if to_bay else route[::-1]

    # -- which trips are driven --
    def legs(self, rid, pieces):
        """Driven legs of one car owner's day: every trip of a home-to-home tour that has a trip longer than
        drive_min_m and nothing that needs the tram, a stroll or work rounds. [] if the day is walked."""
        home = self.city.res[rid]["home"]
        no = set(self.r["no_drive_acts"])
        tours, cur = [], None
        for i, (kind, t0, t1, payload, s) in enumerate(pieces):
            at_home = kind == "stay" and s["place"] == home
            if at_home:
                if cur:
                    tours.append(cur)
                cur = None
                continue
            if cur is None:
                cur = []
            cur.append(i)
        out = []
        for tour in tours:
            ps = [pieces[i] for i in tour]
            away = [i for i in tour if pieces[i][0] == "away"]
            trip_acts = ("away", "arrive")             # waiting for and coming back from the tram
            if any(p[0] == "stroll" or (p[4]["act"] in no and not (away and p[4]["act"] in trip_acts)) for p in ps) or len(away) > 1:
                continue
            moves = [i for i in tour if pieces[i][0] == "move"]
            m_out = m_in = None
            if away:                                   # CITY-068: drive out through a town gate instead of the tram
                a = away[0]
                m_out = next((i for i in range(a - 1, tour[0] - 1, -1) if pieces[i][0] == "move"), None)
                m_in = next((i for i in range(a + 1, tour[-1] + 1) if pieces[i][0] == "move"), None)
                if m_out is None or m_in is None or pieces[m_out - 1][4]["place"] == pieces[m_out][4]["place"]:
                    continue
            elif not moves or max(pieces[i][3][1][-1] for i in moves) < self.r["drive_min_m"]:
                continue
            for i in moves:
                prev = pieces[i - 1][4]
                kind, t0, t1, (route, cum, v), s = pieces[i]
                if i == m_out:
                    out.append(dict(kind="out", piece=i, span=away[0] - i + 1, frm=prev["place"], to=None, spot0=route[0], target=pieces[away[0]][1],
                                    ready=pieces[i - 1][1], walk_len=cum[-1], away=pieces[away[0]][4]))
                    continue
                if i == m_in:
                    out.append(dict(kind="in", piece=away[0] + 1, span=i - away[0], frm=None, to=s["place"], spot1=route[-1], target=s["start"],
                                    back=pieces[away[0]][2], walk_len=cum[-1], dest=s))
                    continue
                if prev["place"] == s["place"]:
                    continue                      # a few steps inside the same place: no car needed
                out.append(dict(kind="town", piece=i, span=1, frm=prev["place"], to=s["place"], spot0=route[0], spot1=route[-1], target=s["start"],
                                ready=pieces[i - 1][1], walk_dep=t0, walk_arr=t1, walk_len=cum[-1], dest=s))
        return out

    # -- the day's plan --
    def apply(self, day, plan):
        """Called by City.plan(day): work out the car plan, then change the drivers' days to match."""
        legs = {}
        for car in self.cars.values():
            lg = self.legs(car["owner"], plan[car["owner"]])
            if lg:
                legs[car["id"]] = lg
        legs.update(self._fleet_legs(day))
        self._crossers = self.crossings_in_use(plan)
        trips, assigned, boxes, entries = self._simulate(day, legs)
        self._days[day] = dict(trips=trips, assigned=assigned, boxes=boxes, entries=entries, crossers=self._crossers)
        if len(self._days) > 12:
            self._days.pop(next(iter(self._days)))
        for cid, lg in legs.items():
            if cid not in self.cars:
                continue                                  # vans and passing cars have no driver among the residents
            rid = self.cars[cid]["owner"]
            plan[rid] = self._rewrite(rid, plan[rid], lg, trips[cid])
        return plan

    def crossings_in_use(self, plan):
        """When walkers are on each zebra crossing today: {(street, junction): [(from, to)] real seconds, sorted}. Worked
        out from the residents' walks (CITY-046: cars give way to anyone on a crossing)."""
        keys = {(round(c["pos"][0], 2), round(c["pos"][1], 2)): (c["street"], c["junction"]) for c in self.roads.d["crossings"]}
        use, pace = {}, self.pace
        for pieces in plan.values():
            for kind, t0, t1, payload, s in pieces:
                if kind != "move":
                    continue
                route, cum, v = payload
                for k in range(len(route) - 1):
                    a, b = route[k], route[k + 1]
                    key = keys.get((round((a[0] + b[0]) / 2, 2), round((a[1] + b[1]) / 2, 2)))
                    if key is not None and dist(a, b) > 3:
                        use.setdefault(key, []).append(((t0 + cum[k] / v) * pace, (t0 + cum[k + 1] / v) * pace))
        for k in use:
            use[k].sort()
        return use

    def walker_on(self, key, t0, t1):
        """Is anyone on this crossing at any moment between t0 and t1 (real seconds)?"""
        spans = self._crossers.get(key)
        if not spans:
            return False
        i = bisect.bisect_left(spans, (t1, float("inf")))
        for a, b in spans[max(0, i - 40):i]:                    # crossings take seconds: only recent starts can still be on it
            if a < t1 and b > t0:
                return True
        return False

    def _est(self, bay0, bid1):
        b1 = self.roads.bays[bid1]
        return self._est_to(bay0, b1["lane"], b1["s"])

    def _est_to(self, bay0, lane, s):
        b0 = self.roads.bays[bay0]
        legs = self.roads.route(b0["lane"], b0["s"], lane, s)
        return sum((g["s1"] - g["s0"]) / g["v"] for g in legs) if legs else 1e9

    def _red_wait(self, bay0, lane, s):
        """What the lights on the quickest way could cost (CITY-068): for every junction with signals on the route, the
        time its lights are not green for one road, times the share in `gate_light_allowance`. Free-flow estimates leave
        this out, and a commuter who must be through the gate on time cannot gain by leaving a little earlier and
        meeting the same red."""
        legs = self.roads.route(self.roads.bays[bay0]["lane"], self.roads.bays[bay0]["s"], lane, s) or []
        cyc = self.r["signals"]
        red = cyc["green_s"] + 2 * (cyc["amber_s"] + cyc["all_red_s"])     # one road waits through the other's green, both ambers and all-reds
        n = sum(1 for g in legs if g["kind"] == "box" and self.roads.j[g["junction"]]["control"] == "signals")
        return n * red * self.r.get("gate_light_allowance", 0.0)

    def gate_for(self, bay0):
        """The town gate quickest to reach from a bay (ties by name)."""
        R = self.roads
        return min(sorted(R.gates), key=lambda g: self._est_to(bay0, R.gates[g]["out"], R.lanes[R.gates[g]["out"]]["length"]))

    def _dest_bay(self, car, to, assigned_free):
        if to == car["home"]:
            return car["bay"]
        door = self.city.lots[to]["door"]
        for bid in self.roads.bays_near(door, self.r["bay_reach_m"]) + self.roads.bays_near(door, 10 * self.r["bay_reach_m"]):
            if assigned_free(bid):
                return bid
        return None

    def _simulate(self, day, legs):
        """Every car trip of the day, in time order, on the lanes: pull out, follow, give way, queue, park.

        Each car keeps `spacing` metres behind the car ahead using where that car was one step ago, so no two
        cars ever share a lane space; boxes take one movement at a time (two straight movements on the same
        axis may share); signals and priority decide who may enter. Returns per-car trips with their recorded
        progress, bay assignments, box occupancy intervals and box entries (for the tests)."""
        R, r, dt, pace = self.roads, self.r, self.step, self.pace
        sp = r["spacing"]
        cyc = r["signals"]
        cycle = 2 * (cyc["green_s"] + cyc["amber_s"] + cyc["all_red_s"])
        day0 = day * 1440 * pace                          # real seconds at this day's midnight (signals run on world time)
        every = sorted(set(self.cars) | set(self.vans) | set(legs))
        bay_car = {self.home_bay(v): v for v in every if self.home_bay(v)}   # who is parked in or has reserved each bay
        at_bay = {v: self.home_bay(v) for v in every}
        trips = {v: [] for v in every}
        assigned, boxes, entries = [], [], []
        events = []                                       # (time to pull out, car id, leg index)
        state = {}                                        # moving cars: dict(legs, starts, d, k, trip)
        occ = {}                                          # leg key -> {car id: s on that leg}
        in_box = {}                                       # junction -> {car id: box leg}

        def walk_time(rid, length):
            return length / (self.city.speed(rid) * pace)                       # game minutes

        def schedule(cid, k, ready_min):
            lg = legs[cid][k]
            if lg["kind"] == "pass":                      # a car passing through: in at one gate on the minute
                heapq.heappush(events, (lg["at"] * pace, cid, k))
                return
            if lg["kind"] == "van":                       # a van leaves when its round says, after its stop
                t0 = lg["at"] if lg.get("at") is not None else ready_min + lg["wait"]
                heapq.heappush(events, (t0 * pace, cid, k))
                return
            car = self.cars[cid]
            rid = car["owner"]
            bay0 = at_bay[cid]
            if lg["kind"] == "in":                        # back through the gate when the tram would have brought them
                lg["gate"] = legs[cid][k - 1]["gate"]
                heapq.heappush(events, (max(lg["back"] * pace, ready_min * pace), cid, k))
                return
            if lg["kind"] == "out":                       # through the gate when the tram would have left
                gate = self.gate_for(bay0)
                g = R.gates[gate]
                w1 = lengths(self.walk(lg["frm"], lg["spot0"], bay0))[-1]
                L = R.lanes[g["out"]]["length"]
                est = walk_time(rid, w1) + (r["board_s"] + self._est_to(bay0, g["out"], L) + self._red_wait(bay0, g["out"], L) + r["gate_margin_s"]) / pace
                leave = max(ready_min, lg["target"] - est)
                board = leave + walk_time(rid, w1)
                lg.update(leave=leave, board=board, bay0=bay0, w1=w1, gate=gate)
                heapq.heappush(events, ((board * pace) + r["board_s"], cid, k))
                return
            w1 = lengths(self.walk(lg["frm"], lg["spot0"], bay0))[-1]
            guess = car["bay"] if lg["to"] == car["home"] else (R.bays_near(self.city.lots[lg["to"]]["door"], r["bay_reach_m"]) or [car["bay"]])[0]
            w2 = lengths(self.walk(lg["to"], lg["spot1"], guess))[-1]
            est = walk_time(rid, w1) + (r["board_s"] + self._est(bay0, guess) + r["park_s"]) / pace + walk_time(rid, w2)
            leave = max(ready_min, lg["target"] - est)
            board = leave + walk_time(rid, w1)
            lg.update(leave=leave, board=board, bay0=bay0, w1=w1)
            heapq.heappush(events, ((board * pace) + r["board_s"], cid, k))

        for cid, lg in sorted(legs.items()):
            schedule(cid, 0, lg[0].get("ready", 0.0))

        def signal(jid, axis, t):
            tt = (t + day0 + rnd("sig", jid) * cycle) % cycle
            g, a, red = cyc["green_s"], cyc["amber_s"], cyc["all_red_s"]
            first = "ns" if tt < g + a + red else "ew"
            tt = tt if first == "ns" else tt - (g + a + red)
            return "green" if axis == first and tt < g else "amber" if axis == first and tt < g + a else "red"

        def box_ok(jid, bx, t):
            j = R.j[jid]
            for other in in_box.get(jid, {}).values():
                if other["key"] == bx["key"]:
                    continue
                if not (bx["straight"] and other["straight"] and bx["axis"] == other["axis"]):
                    return False
            room = occ.get(bx["into"], {})
            if any(s < sp + 0.5 for s in room.values()):
                return False                              # do not block the box: the lane beyond must have room
            la, lb = R.lanes[bx["frm"]], R.lanes[bx["into"]]   # walkers on the crossings it passes: wait (CITY-046)
            if self.walker_on((la["street"], jid), t - .5, t + r["crossing_clear_s"]) or                     self.walker_on((lb["street"], jid), t, t + 2 * r["crossing_clear_s"]):
                return False
            if j["control"] == "signals":
                return signal(jid, bx["axis"], t) == "green"
            if j["control"] == "priority" and bx["axis"] != j["main"]:
                for lid in R.inn.get(jid, []):
                    lane = R.lanes[lid]
                    if lane["axis"] != j["main"]:
                        continue
                    for s in occ.get(lid, {}).values():
                        if lane["length"] - s < lane["v"] * r["main_road_gap_s"]:
                            return False
            return True

        def free_bay(cid):
            return lambda bid: bay_car.get(bid) in (None, cid)

        def pull_out(cid, k, t):
            car = self.vehicle(cid)
            lg = legs[cid][k]
            if lg["kind"] in ("in", "pass"):              # coming in through a gate: start on its way into town
                bay0 = dict(lane=R.gates[lg.get("gate_in") or lg["gate"]]["back"], s=0.0)
            else:
                bay0 = R.bays[at_bay[cid]]
            if lg["kind"] in ("out", "pass"):
                dest, b1 = None, dict(lane=R.gates[lg["gate"]]["out"], s=R.lanes[R.gates[lg["gate"]]["out"]]["length"])
            else:
                dest = self._dest_bay(car, lg["to"], free_bay(cid))
                b1 = R.bays[dest]
            route = R.route(bay0["lane"], bay0["s"], b1["lane"], b1["s"])
            lane = R.lanes[bay0["lane"]]
            behind = sp + lane["v"] * r["pull_out_clear_s"]
            for s in occ.get(bay0["lane"], {}).values():
                if bay0["s"] - behind < s < bay0["s"] + sp:
                    return False
            for jb in in_box.values():                    # a car about to come out of a junction onto this lane
                for oc, bx in jb.items():
                    if bx["into"] == bay0["lane"] and (bx["length"] - occ[bx["key"]][oc]) + bay0["s"] < behind:
                        return False
            if dest is not None and dest != car["bay"]:
                assigned.append(dict(t=t, car=cid, bay=dest, to=lg["to"],
                                     nearer_free=[b for b in R.bays_near(self.city.lots[lg["to"]]["door"], r["bay_reach_m"])
                                                  if b != dest and dist(R.bays[b]["pos"], self.city.lots[lg["to"]]["door"]) < dist(b1["pos"], self.city.lots[lg["to"]]["door"]) - 1e-9
                                                  and bay_car.get(b) in (None, cid)]))
            if at_bay[cid] is not None and bay_car.get(at_bay[cid]) == cid and at_bay[cid] != car["bay"]:
                del bay_car[at_bay[cid]]                  # the bay it leaves is free again (its home bay stays reserved)
            if dest is not None:
                bay_car[dest] = cid
            starts = [0.0]
            for g in route:
                starts.append(starts[-1] + g["s1"] - g["s0"])
            trip = dict(leg=k, t0=t, bay0=at_bay[cid], bay1=dest, legs=route, starts=starts, d=[0.0], t1=None, gate=lg.get("gate"), gate_in=lg.get("gate_in"), kind=lg["kind"])
            at_bay[cid] = None
            if lg["kind"] == "in":
                lg["t_in"] = t / pace
            trips[cid].append(trip)
            state[cid] = dict(legs=route, starts=starts, d=0.0, k=0, trip=trip, leg=k)
            occ.setdefault(route[0]["key"], {})[cid] = route[0]["s0"]
            return True

        t = 0.0
        end = 1440 * pace
        waiting = []
        while (events or state or waiting) and t < end:
            if not state and not waiting and events:
                t = max(t, math.ceil(events[0][0] / dt) * dt)  # nothing on the roads: jump to the next departure
            while events and events[0][0] <= t:
                waiting.append(heapq.heappop(events)[1:])
            still = []
            for cid, k in sorted(waiting):
                if not pull_out(cid, k, t):
                    still.append((cid, k))
            waiting = still
            # move every car one step, against where everyone was at the start of the step
            prev = {cid: (st["k"], st["d"]) for cid, st in state.items()}
            snap = {key: dict(v) for key, v in occ.items()}
            done = []
            for cid in sorted(state, key=lambda c: -state[c]["d"]):
                st = state[cid]
                legs_, starts = st["legs"], st["starts"]
                k, d = prev[cid]
                g = legs_[k]
                cap = starts[-1]
                free = d + g["v"] * dt
                if k + 1 < len(legs_) and free > starts[k + 1]:   # past the end of this stretch: its own limit from there on
                    free = starts[k + 1] + (dt - (starts[k + 1] - d) / g["v"]) * legs_[k + 1]["v"]
                # the car ahead on this leg or the next few, as it was one step ago
                ahead, look = None, 0.0
                for kk in range(k, len(legs_)):
                    gg = legs_[kk]
                    here = snap.get(gg["key"], {})
                    mine = (d - starts[k] + g["s0"]) if kk == k else None
                    cands = [s for oc, s in here.items() if oc != cid and (mine is None or s > mine + 1e-9)]
                    if cands:
                        ahead = starts[kk] + min(cands) - gg["s0"]
                        break
                    look = starts[kk + 1] - d
                    if look > 80:
                        break
                if ahead is not None:
                    cap = min(cap, ahead - sp)
                # the stop line: may this car enter the next junction box?
                if g["kind"] == "lane" and k + 1 < len(legs_) and free > starts[k + 1] - 1e-9:
                    bx = legs_[k + 1]
                    if cid not in in_box.get(bx["junction"], {}) and not box_ok(bx["junction"], bx, t):
                        cap = min(cap, starts[k + 1])
                nd = max(d, min(free, cap))
                # crossing into the box or out of it
                nk = k
                while nk + 1 < len(legs_) and nd > starts[nk + 1] + 1e-9:
                    nk += 1
                    if legs_[nk]["kind"] == "box":
                        bx = legs_[nk]
                        j = R.j[bx["junction"]]
                        entries.append(dict(t=t, car=cid, junction=bx["junction"], key=bx["key"], axis=bx["axis"], control=j["control"],
                                            main=j.get("main"), signal=signal(bx["junction"], bx["axis"], t) if j["control"] == "signals" else None))
                        in_box.setdefault(bx["junction"], {})[cid] = bx
                        boxes.append(dict(car=cid, junction=bx["junction"], key=bx["key"], straight=bx["straight"], axis=bx["axis"], t0=t, t1=None))
                    elif legs_[nk - 1]["kind"] == "box":
                        jb = in_box.get(legs_[nk - 1]["junction"], {})
                        jb.pop(cid, None)
                        for bxr in reversed(boxes):
                            if bxr["car"] == cid and bxr["t1"] is None:
                                bxr["t1"] = t
                                break
                for kk in range(k, nk + 1):
                    occ.get(legs_[kk]["key"], {}).pop(cid, None)
                st["k"], st["d"] = nk, nd
                st["trip"]["d"].append(nd)
                if nd >= starts[-1] - 1e-9:
                    done.append(cid)
                else:
                    occ.setdefault(legs_[nk]["key"], {})[cid] = nd - starts[nk] + legs_[nk]["s0"]
            t += dt
            for cid in done:                              # parked: off the lane, into the bay
                st = state.pop(cid)
                trip = st["trip"]
                trip["t1"] = t
                at_bay[cid] = trip["bay1"]
                lg = legs[cid][st["leg"]]
                if lg["kind"] == "pass":                  # out of the far gate: gone
                    continue
                if lg["kind"] == "van":                   # parked at a shop or back at the depot
                    lg.update(t_park=t, bay1=trip["bay1"], trip=trip)
                    if st["leg"] + 1 < len(legs[cid]):
                        schedule(cid, st["leg"] + 1, t / pace)
                    continue
                rid = self.cars[cid]["owner"]
                if lg["kind"] == "out":                   # through the gate and off the map until the way back
                    lg.update(t_exit=t / pace, trip=trip)
                    schedule(cid, st["leg"] + 1, t / pace + 0.1)
                    continue
                w2 = lengths(self.walk(lg["to"], lg["spot1"], trip["bay1"], to_bay=False))[-1]
                lg.update(t_park=t, get_out=(t + r["park_s"]) / pace, w2=w2, bay1=trip["bay1"], trip=trip)
                lg["door"] = lg["get_out"] + walk_time(rid, w2)
                if st["leg"] + 1 < len(legs[cid]):
                    schedule(cid, st["leg"] + 1, lg["door"])
        return trips, assigned, boxes, entries

    # -- the drivers' changed days --
    def _rewrite(self, rid, pieces, legs, trips):
        """Replace each driven walk with: walk to the car, drive, walk from the bay to the door. Stays around
        it move to match, so the day stays gap-free."""
        v = self.city.speed(rid) * self.pace
        done = [lg for lg in legs if "door" in lg or "t_exit" in lg or lg["kind"] == "in"]
        by_piece = {lg["piece"]: lg for lg in done}
        car = self.cars_of(rid)
        out = []
        i = 0
        while i < len(pieces):
            kind, t0, t1, payload, s = pieces[i]
            lg = by_piece.get(i)
            if lg is not None and lg["kind"] == "in":
                if "door" not in lg:                      # never made it back today: stay away
                    break
                out.append(("drive", out[-1][2], lg["get_out"], car, s))
                r2 = self.walk(lg["to"], lg["spot1"], lg["bay1"], to_bay=False)
                c2 = lengths(r2)
                out.append(("move", lg["get_out"], lg["get_out"] + c2[-1] / v, (r2, c2, v), lg["dest"]))
                i += lg["span"]
                continue
            if lg is not None:
                last = out[-1]
                if last[0] == "stay":
                    out[-1] = (last[0], last[1], max(last[1], lg["leave"]), last[3], last[4])
                elif last[2] < lg["leave"] - 1e-9:                     # came straight from another trip: wait there
                    out.append(("stay", last[2], lg["leave"], last[3][0][-1], last[4]))
                t_a = out[-1][2]
                r1 = self.walk(lg["frm"], lg["spot0"], lg["bay0"])
                c1 = lengths(r1)
                out.append(("move", t_a, t_a + c1[-1] / v, (r1, c1, v), s))
                if lg["kind"] == "out":
                    nxt = next((x for x in legs if x["kind"] == "in" and x["piece"] == i + lg["span"]), None)
                    back = nxt["t_in"] if nxt is not None and "t_in" in nxt else 1440.0
                    out.append(("drive", t_a + c1[-1] / v, lg["t_exit"], car, lg["away"]))
                    out.append(("away", lg["t_exit"], back, None, lg["away"]))
                    i += lg["span"]
                    continue
                out.append(("drive", t_a + c1[-1] / v, lg["get_out"], car, s))
                r2 = self.walk(lg["to"], lg["spot1"], lg["bay1"], to_bay=False)
                c2 = lengths(r2)
                out.append(("move", lg["get_out"], lg["get_out"] + c2[-1] / v, (r2, c2, v), s))
                i += 1
                continue
            if out:
                t0 = out[-1][2]                    # keep the day gap-free: each piece starts where the last ended
            t1 = max(t1, t0)
            out.append((kind, t0, t1, payload, s))
            i += 1
        return out

    def cars_of(self, rid):
        return next(cid for cid, c in self.cars.items() if c["owner"] == rid)

    # -- where is every car? --
    def _day(self, day):
        if day not in self._days:
            self.city.plan(day)
            if day not in self._days:                     # the city's plan was cached from before traffic was attached
                self.city._plans.pop(day, None)
                self.city.plan(day)
        return self._days[day]

    def car_at(self, cid, day, minute):
        """dict(pos, heading, state, bay, leg) for one car at a game minute of a day. state: parked | moving | waiting."""
        d = self._day(day)
        t = minute * self.pace
        trips = d["trips"].get(cid, [])
        i = bisect.bisect_right([tr["t0"] for tr in trips], t) - 1
        if i < 0:
            if self.home_bay(cid) is None:               # a car passing through: not here yet
                return dict(pos=None, heading=None, state="away", bay=None, leg=None)
            b = self.roads.bays[self.home_bay(cid)]
            return dict(pos=b["pos"], heading=self.roads.lanes[b["lane"]]["u"], state="parked", bay=b["id"], leg=None)
        tr = trips[i]
        if tr["t1"] is not None and t >= tr["t1"] and tr["bay1"] is None:
            return dict(pos=None, heading=None, state="away", bay=None, leg=None, gate=tr["gate"])
        if tr["t1"] is not None and t >= tr["t1"]:
            b = self.roads.bays[tr["bay1"]]
            return dict(pos=b["pos"], heading=self.roads.lanes[b["lane"]]["u"], state="parked", bay=b["id"], leg=None)
        f = (t - tr["t0"]) / self.step
        n = int(f)
        ds = tr["d"]
        if n + 1 >= len(ds):
            dd, moving = ds[-1], False
        else:
            dd = ds[n] + (ds[n + 1] - ds[n]) * (f - n)
            moving = ds[n + 1] - ds[n] > 0.05 * self.step
        starts = tr["starts"]
        k = min(len(tr["legs"]) - 1, max(0, bisect.bisect_right(starts, dd) - 1))
        leg = tr["legs"][k]
        pos, heading = self.roads.point(leg, dd - starts[k] + leg["s0"])
        return dict(pos=pos, heading=heading, state="moving" if moving else "waiting", bay=None, leg=(leg["key"], dd - starts[k] + leg["s0"]))

    def at(self, t):
        day, m = self.city.clock.split(t)
        return {cid: self.car_at(cid, day, m) for cid in self.ids(day)}

    def loaded(self, cars, viewers, radius=None):
        """CITY-048: cars that need a body now, within the car load distance of any viewer."""
        radius = self.r["car_load_m"] if radius is None else radius
        return {cid for cid, c in cars.items() if c["pos"] is not None and any(dist(c["pos"], v) <= radius for v in viewers)}
