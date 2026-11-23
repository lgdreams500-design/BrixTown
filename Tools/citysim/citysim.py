"""Brix Town - city in a box: the rules of a living block, with no engine and no drawing code.

Everything here is *calculated from the world clock and the data file*, never stepped frame by frame:
ask where a resident is at any minute of any day and the answer is the same every time, on any
machine, without replaying what came before. That is what lets the game keep a whole city "alive"
as data and only create bodies near players (docs/specs/domains/LIVING_CITY.md).

Controllers, one small class or function group each:
    Clock        calendar: day, weekday, season, daylight
    weather()    clear / cloud / rain / fog, the same for everyone
    Network      streets and paths, shortest routes
    Places       lots, doors and interaction points with a number of places each
    Routines     what each resident does today, turned into a timeline of walks and stays
    City.at()    where everyone is at a given minute
    City.loaded  which residents are near any viewer
    Vegetation   size, leaves, blossom, fall and replanting of every tree from its planting day
    Vehicles     tram and delivery van on fixed routes

Units: metres, game minutes since day 0 00:00. The prototype is Python; the same rules and the same
data file are meant to be ported to a C++ module with the same tests (TECH-004).
"""
import bisect, hashlib, heapq, json, math, os

HERE = os.path.dirname(os.path.abspath(__file__))


def rnd(*keys):
    """A repeatable number in [0, 1) for a set of keys. Python's own hash() changes between runs."""
    h = hashlib.blake2b(repr(keys).encode(), digest_size=8).digest()
    return int.from_bytes(h, "big") / 2.0 ** 64


def hm(text):
    h, m = text.split(":")
    return int(h) * 60 + int(m)


def dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def along(points, cum, d):
    """Point at distance d along a polyline with cumulative lengths `cum`."""
    if d <= 0:
        return tuple(points[0])
    if d >= cum[-1]:
        return tuple(points[-1])
    i = bisect.bisect_right(cum, d) - 1
    seg = cum[i + 1] - cum[i]
    f = (d - cum[i]) / seg if seg > 1e-9 else 0.0
    a, b = points[i], points[i + 1]
    return (a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f)


def lengths(points):
    cum = [0.0]
    for a, b in zip(points, points[1:]):
        cum.append(cum[-1] + dist(a, b))
    return cum


# ---- clock and weather --------------------------------------------------------------------------
class Clock:
    def __init__(self, cal):
        self.cal = cal
        self.season_days = cal["days_per_season"]
        self.year_days = self.season_days * len(cal["seasons"])

    def split(self, t):
        day = int(t // 1440)
        return day, t - day * 1440

    def info(self, day):
        doy = day % self.year_days
        s = doy // self.season_days
        wd = day % 7
        return dict(day=day, year=day // self.year_days + 1, doy=doy, season=s, season_name=self.cal["seasons"][s],
                    day_of_season=doy % self.season_days + 1, weekday=wd, weekday_name=self.cal["week"][wd],
                    market=wd in self.cal["market_days"], rest=wd in self.cal["rest_days"], sunday=wd == 6)

    def daylight(self, t):
        """0 at night, 1 in full day, with an hour of dawn and dusk."""
        day, m = self.split(t)
        s = self.info(day)["season"]
        rise, sets = self.cal["sunrise"][s] * 60, self.cal["sunset"][s] * 60
        up = min(1.0, max(0.0, (m - (rise - 30)) / 60.0))
        down = min(1.0, max(0.0, ((sets + 30) - m) / 60.0))
        return min(up, down)


_WET = []


def _wet_cut(chance):
    """The value that the blended draw exceeds with the given chance (measured once from the draw itself)."""
    if not _WET:
        _WET.extend(sorted(0.5 * rnd("q", i) + 0.3 * rnd("q", i, 1) + 0.2 * rnd("q", i, 2) for i in range(4000)))
    return _WET[min(len(_WET) - 1, int((1.0 - chance) * len(_WET)))]


def weather(data, clock, t):
    """Weather for the six-hour slot that contains t. Rain comes in spells because neighbouring slots share part of their draw."""
    day, m = clock.split(t)
    slot = day * 4 + int(m // 360)
    s = clock.info(day)["season"]
    w = data["weather"]
    wet = 0.5 * rnd("wet", slot) + 0.3 * rnd("wet", slot - 1) + 0.2 * rnd("wet", slot - 2)
    if wet > _wet_cut(w["rain_chance"][s]):
        return "rain"
    if int(m // 360) == 1 and rnd("fog", day) < w["fog_chance"][s]:
        return "fog"
    if rnd("cloud", slot) < w["cloud_chance"][s]:
        return "cloud"
    return "clear"


# ---- streets and paths ---------------------------------------------------------------------------
class Network:
    def __init__(self, segments, extra_points):
        self.nodes, self.adj = [], {}
        pts = {tuple(p) for s in segments for p in s} | {tuple(p) for p in extra_points}
        B = 16.0                                                # points by 16 m square, so a segment only tests those near it
        grid = {}
        for p in pts:
            grid.setdefault((int(p[0] // B), int(p[1] // B)), []).append(p)
        for a, b in segments:
            a, b = tuple(a), tuple(b)
            near = [p for i in range(int(min(a[0], b[0]) // B), int(max(a[0], b[0]) // B) + 1)
                    for j in range(int(min(a[1], b[1]) // B), int(max(a[1], b[1]) // B) + 1) for p in grid.get((i, j), ())]
            on = sorted({p for p in near if abs(dist(a, p) + dist(p, b) - dist(a, b)) < 1e-6}, key=lambda p: dist(a, p))
            for p, q in zip(on, on[1:]):
                self._edge(p, q)
        self._cache = {}

    def _node(self, p):
        if p not in self.adj:
            self.adj[p] = {}
            self.nodes.append(p)
        return p

    def _edge(self, p, q):
        self._node(p), self._node(q)
        self.adj[p][q] = self.adj[q][p] = dist(p, q)

    def link(self, p, q):
        self._edge(tuple(p), tuple(q))
        self._cache.clear()

    def nearest_on(self, p):
        """Closest point on any edge, and that edge."""
        best = None
        for a in self.nodes:
            for b in self.adj[a]:
                ab = (b[0] - a[0], b[1] - a[1])
                L2 = ab[0] ** 2 + ab[1] ** 2
                f = max(0.0, min(1.0, ((p[0] - a[0]) * ab[0] + (p[1] - a[1]) * ab[1]) / L2))
                q = (a[0] + ab[0] * f, a[1] + ab[1] * f)
                d = dist(p, q)
                if best is None or d < best[0]:
                    best = (d, q, a, b)
        return best[1], best[2], best[3]

    def _from(self, src):
        if src not in self._cache:
            dd, prev, heap = {src: 0.0}, {}, [(0.0, src)]
            while heap:
                d, u = heapq.heappop(heap)
                if d > dd[u]:
                    continue
                for v, w in self.adj[u].items():
                    if d + w < dd.get(v, 1e18):
                        dd[v], prev[v] = d + w, u
                        heapq.heappush(heap, (d + w, v))
            self._cache[src] = (dd, prev)
        return self._cache[src]

    def path(self, a, b):
        dd, prev = self._from(a)
        out = [b]
        while out[-1] != a:
            out.append(prev[out[-1]])
        return out[::-1], dd[b]


# ---- vegetation ------------------------------------------------------------------------------------
class Vegetation:
    """Every tree's state on any day comes from its planting day, its species and the calendar."""

    def __init__(self, data, clock):
        self.d, self.clock = data, clock

    def _generation(self, tree, day):
        """Which tree stands on this site today: (planting day, day it falls)."""
        sp = self.d["species"][tree["species"]]
        yd = self.clock.year_days
        start, g = tree["planted"], 0
        while True:
            lo, hi = sp["life"]
            end = start + (lo + (hi - lo) * rnd("life", tree["seed"], g)) * yd
            if day < end + self.d["fallen_days"] + self.d["replant_gap_days"]:
                return start, end, g
            start, g = end + self.d["fallen_days"] + self.d["replant_gap_days"], g + 1

    def tree(self, tree, day):
        sp = self.d["species"][tree["species"]]
        yd = self.clock.year_days
        start, end, g = self._generation(tree, day)
        if day >= end + self.d["fallen_days"]:
            return dict(state="empty", radius=0.0, leaf=0.0, generation=g)
        age = max(0.0, (min(day, end) - start) / yd)
        radius = sp["r_max"] * (1.0 - math.exp(-sp["rate"] * age)) ** sp["shape"]
        radius = max(radius, 0.45)
        if day >= end:
            return dict(state="fallen", radius=radius, leaf=0.0, age=age, generation=g)
        doy = day % yd
        leaf = min(1.0, max(0.0, (doy - sp["bud"]) / float(sp["full"] - sp["bud"])))
        turn = min(1.0, max(0.0, (doy - sp["turn"]) / 4.0))
        drop = min(1.0, max(0.0, (doy - sp["turn"] - 2) / float(sp["bare"] - sp["turn"] - 2)))
        leaf *= 1.0 - drop
        b = sp.get("blossom")
        blossom = 1.0 if b and b[0] <= doy <= b[1] and age > 1.5 else 0.0
        swept = min(1.0, max(0.0, (doy - sp["bare"]) / 6.0))
        color = [sp["summer"][i] + (sp["autumn"][i] - sp["summer"][i]) * turn for i in range(3)]
        return dict(state="growing", radius=radius, leaf=leaf, turn=turn, blossom=blossom, color=color, age=age,
                    ground=drop * (1.0 - swept), autumn=sp["autumn"], generation=g)

    def flowers(self, day):
        f = self.d["flowers"]
        doy = day % self.clock.year_days
        if doy < f["sprout"] or doy >= f["gone"]:
            return dict(size=0.0, bloom=0.0)
        size = min(1.0, (doy - f["sprout"]) / float(f["bloom"][0] - f["sprout"]))
        fade = min(1.0, max(0.0, (doy - f["bloom"][1]) / float(f["gone"] - f["bloom"][1])))
        return dict(size=size * (1.0 - 0.6 * fade), bloom=1.0 - fade if f["bloom"][0] <= doy else 0.0)

    def plant(self, name, day):
        """CITY-017: a garden plant's look on any day of the year, from its kind's dates alone (vegetation.json "plants").
        size: how much of it stands (bulbs and asters come up and die back); leaf: how much leaf it carries; color: the
        leaf's colour (fresh in spring, turning in autumn, duller in winter); bloom: how far into flower; spent: dry heads
        left standing after the flowers."""
        p = self.d["plants"][name]
        doy = day % self.clock.year_days
        ramp = lambda v: min(1.0, max(0.0, v))
        size = 1.0
        if "up" in p:
            rise, gone = p["up"]
            size = 0.0 if doy < rise or doy >= gone else ramp((doy - rise + 1) / 3.0) * ramp((gone - doy) / 3.0)
        if p.get("leaf"):
            bud, full, turn_d, bare = p["leaf"]
            leaf = ramp((doy - bud) / float(full - bud))
            turn = ramp((doy - turn_d) / 4.0)
            leaf *= 1.0 - ramp((doy - turn_d - 2) / float(bare - turn_d - 2))
            color = [p["summer"][i] + (p["autumn"][i] - p["summer"][i]) * turn for i in range(3)]
        else:                                                  # evergreen: always in leaf, duller through the winter
            leaf, w = 1.0, ramp((doy - p.get("winter_from", 34)) / 3.0)
            color = [p["summer"][i] + (p.get("winter", p["summer"])[i] - p["summer"][i]) * w for i in range(3)]
        if "flush" in p:                                       # new growth, paler, for a few days in spring
            f0, f1 = p["flush"]
            k = ramp((doy - f0 + 1) / 2.0) * ramp((f1 - doy) / 2.0)
            color = [color[i] + (p["fresh"][i] - color[i]) * k for i in range(3)]
        bloom, spent = 0.0, 0.0
        if "bloom" in p:
            b0, b1 = p["bloom"]
            bloom = ramp((doy - b0 + 1) / 2.0) * ramp((b1 - doy) / 2.0) * (1.0 if size > 0 else 0.0)
            if "spent_until" in p and b1 - 1 <= doy < p["spent_until"]:
                spent = 1.0 - bloom
        return dict(size=size, leaf=leaf * (1.0 if size > 0 else 0.0), color=color, bloom=bloom, spent=spent)


# ---- the city ----------------------------------------------------------------------------------------
class City:
    def __init__(self, path=None, data=None):
        """Load one block from a file, or take an assembled world (see Tools/worldgen/world.py) as `data`."""
        if data is None:
            with open(path or os.path.join(HERE, "data", "block_01.json"), encoding="utf-8") as f:
                data = json.load(f)
        self.d = data
        d = self.d
        self.clock = Clock(d["calendar"])
        # Real seconds that one game minute lasts. People walk at their true speed in real time, so a faster
        # clock makes every walk take more game minutes. 60 = the clock runs at real time.
        self.pace = float(d["calendar"].get("real_seconds_per_minute", 60.0))
        self.veg = Vegetation(d, self.clock)
        self.lots = {l["id"]: l for l in d["lots"]}
        self.res = {r["id"]: r for r in d["residents"]}
        self.net = Network(d["streets"], [])
        self.anchor = {}
        for l in d["lots"]:
            door = tuple(l["door"])
            if door in self.net.adj:
                self.anchor[l["id"]] = door
                continue
            q, a, b = self.net.nearest_on(door)
            q = (round(q[0], 3), round(q[1], 3))
            if q not in self.net.adj:                    # split the street where the door meets it
                del self.net.adj[a][b], self.net.adj[b][a]
                self.net.link(a, q), self.net.link(q, b)
            if door != q:
                self.net.link(door, q)
            self.anchor[l["id"]] = door
        self.net._cache.clear()
        self.loops = {}                                  # a strolling loop per garden: lot["loop"], or the block's one loop
        for l in d["lots"]:
            pts = l.get("loop") or (d.get("stroll_loop") if l["kind"] == "garden" else None)
            if pts:
                closed = [tuple(p) for p in pts] + [tuple(pts[0])]
                self.loops[l["id"]] = (closed, lengths(closed))
        self.mates = {}
        for rid in sorted(self.res):
            self.mates.setdefault(self.res[rid]["home"], []).append(rid)
        self._plans = {}
        self.traffic = None                              # set by traffic.Traffic: residents who own a car drive their longer trips
        self.react = d.get("react")                      # World/rules/weather_react.json; None: people ignore the weather (F-NPC-REACT level 0)

    # -- reacting to the weather (F-NPC-REACT level 1, CITY-077 to CITY-080): all of it from the clock, the seed and data --
    def raining(self, t):
        return weather(self.d, self.clock, t) == "rain"

    def rain_spells(self, day):
        """[(start, end)] minutes of the day when it rains, neighbouring wet slots joined into one spell."""
        out = []
        for k in range(4):
            if self.raining(day * 1440 + k * 360 + 1):
                if out and out[-1][1] == k * 360:
                    out[-1] = (out[-1][0], (k + 1) * 360)
                else:
                    out.append((k * 360, (k + 1) * 360))
        return out

    def wear(self, rid, day, wet=None):
        """What a resident puts on and takes out today: dict(coat, hood, umbrella, coat_colour, umbrella_colour). Decided
        once in the morning from the season and the day's weather, so it never changes during the day (CITY-077).
        wet=True or False asks what they would wear if the day did or did not have rain in it."""
        R = self.react
        if not R:
            return dict(coat=False, hood=False, umbrella=False, coat_colour=None, umbrella_colour=None)
        wet = bool(self.rain_spells(day)) if wet is None else wet
        season = self.clock.info(day)["season"]
        u, c = R["umbrella"], R["coat"]
        umbrella = rnd("umb", rid, day) < (u["rain_day"] if wet else u["dry_day"])
        coat = rnd("coat", rid, day) < max(c["season"][season], c["rain_day"] if wet else 0.0)
        return dict(coat=coat, hood=coat and rnd("hood", rid) < c["hood"], umbrella=umbrella,
                    coat_colour=c["colours"][int(rnd("coatc", rid) * len(c["colours"]))] if coat else None,
                    umbrella_colour=u["colours"][int(rnd("umbc", rid) * len(u["colours"]))] if umbrella else None)

    def _open_air(self, s):
        R = self.react
        return self.lots[s["place"]]["kind"] in R["open_kinds"] and s["act"] not in R["stay_out"]

    def _take_cover(self, rid, day, ss):
        """CITY-078: someone out in the open when rain starts stops within a minute or two and makes for cover (their nearest
        café's awning, or home), waits there until the rain stops, then goes back if enough of what they were doing is left.
        Someone due somewhere open while it is already raining goes straight to the cover instead."""
        R, sh = self.react, self.react["shelter"]
        spells = self.rain_spells(day)
        if not spells:
            return ss
        r = self.res[rid]
        out = []
        for s in ss:
            if not self._open_air(s):
                out.append(s)
                continue
            cur = s
            for a, b in spells:
                if cur is None or b <= cur["start"] or a >= cur["end"]:
                    continue
                already = a <= cur["start"]
                t = cur["start"] if already else a + sh["notice_minutes"][0] + rnd("notice", rid, day, a) * (sh["notice_minutes"][1] - sh["notice_minutes"][0])
                if t >= cur["end"] - 1.0:
                    continue
                to = sh["to"][int(rnd("cover", rid, day, a) * len(sh["to"]))]
                place = r["home"] if to == "home" else r.get("near", {}).get(to) or r["home"]
                act = "home" if place == r["home"] else "shelter"
                back = b if cur["end"] - b >= sh["resume_minutes"] else cur["end"]
                if not already:
                    out.append(dict(cur, end=t))
                out.append(dict(start=t, end=back, act=act, place=place, use=self._use(act, place), idx=cur["idx"] + 0.25 + a / 1e4, rush=not already))
                cur = dict(cur, start=back, idx=cur["idx"] + 0.5 + a / 1e4) if back < cur["end"] else None
            if cur is not None:
                out.append(cur)
        return out

    def pace_in_rain(self, rid, day, t, rush):
        """How much faster than their own pace someone walks a leg that sets off at minute t (CITY-079)."""
        R = self.react
        if not R or not self.raining(day * 1440 + t):
            return 1.0
        p = R["pace"]
        if self.wear(rid, day)["umbrella"]:
            return p["umbrella"]
        return p["run"] if rush else p["hurry"]

    def cover(self, rid, day, m, state, act=None):
        """What a resident does about the rain at minute m (CITY-080): None when it is dry or they are under a roof or an
        awning; 'umbrella' (up), 'hood' (up) or 'bare' (head down, a hand up when walking)."""
        if not self.react or state in ("inside", "away", "driving") or act == "shelter" or not self.raining(day * 1440 + m):
            return None
        w = self.wear(rid, day)
        return "umbrella" if w["umbrella"] else "hood" if w["hood"] else "bare"

    # -- routines: today's list of things to do --
    def _pick(self, token, rid, day, idx, minute):
        ch = self.d["choices"][token]
        info = self.clock.info(day)
        wet = weather(self.d, self.clock, day * 1440 + minute) == "rain"
        if wet and "rain" in ch:
            opts = ch["rain"]
        elif info["market"] and "market" in ch and 7 * 60 + 30 <= minute <= 13 * 60 + 30 and rnd("mkt", rid, day, idx) < 0.6:
            opts = ch["market"]
        else:
            opts = ch["default"]
        return opts[int(rnd("pick", rid, day, idx) * len(opts))]

    def _near_work(self, rid, where, place):
        """The place of this kind nearest the resident's work (the same kinds as their own "near" table, among the places that
        are anyone's nearest); home only when home is within near_work_home_max_m of work. Worked out, not stored."""
        r = self.res[rid]
        work = self.lots[r["work"]]["door"]
        if where == "home":
            mx = (self.d.get("near_work") or {}).get("home_max_m", 300.0)
            if dist(self.lots[r["home"]]["door"], work) <= mx:
                return place
            where = "cafe"
        if not hasattr(self, "_pools"):
            self._pools = {}
            for x in self.res.values():
                for k, lid in (x.get("near") or {}).items():
                    if lid in self.lots:
                        self._pools.setdefault(k, set()).add(lid)
            self._pools = {k: sorted(v) for k, v in self._pools.items()}
            self._nw = {}
        pool = self._pools.get(where)
        if not pool:
            return place
        key = (r["work"], where)
        if key not in self._nw:
            self._nw[key] = min(pool, key=lambda lid: (dist(self.lots[lid]["door"], work), lid))
        return self._nw[key]

    def blocks(self, rid, day):
        """[(start minute, activity, place id)] for one resident on one day. Starts are strictly increasing."""
        r = self.res[rid]
        role = self.d["roles"][r["role"]]
        info = self.clock.info(day)
        if info["sunday"] and "sunday" in role:
            tpl = role["sunday"]
        elif info["rest"] and "weekend" in role:
            tpl = role["weekend"]
        elif info["market"] and r.get("market_seller") and "market" in role:
            tpl = role["market"]
        else:
            tpl = role["weekday"]
        lo, hi = role.get("habit_min", (0, 0))           # CITY-096: their own habit, the same every day: earlier or later in the
        habit = lo + rnd("habit", rid) * (hi - lo)         # role's window (an office's staff over three hours, a shop's rota over one)
        out = []
        for i, (start, act, where) in enumerate(tpl):
            m = hm(start)
            if i > 0:
                m += (r.get("offset", 0) + habit) * (0.5 if act in ("sleep",) else 1.0) + (rnd("jit", rid, day, i) * 2 - 1) * role["jitter"]
            end = hm(tpl[i + 1][0]) if i + 1 < len(tpl) else 1440
            if where == "rounds":
                every = role["round_every"]
                n = max(1, int((end - hm(start)) // every))
                stops = r.get("rounds") or self.d["choices"]["rounds"]
                ract = role["round_act"] if act == "rounds" else act        # a role may do two kinds of round (CITY-088)
                for k in range(n):
                    out.append((m + k * every, ract, stops[int(rnd("stop", rid, day, i, k) * len(stops))]))
                continue
            if act == "tend":
                every = role["round_every"]
                for k in range(max(1, int((end - hm(start)) // every))):
                    out.append((m + k * every, "tend", r.get("work") or r.get("near", {}).get("garden", "garden")))
                continue
            token = where
            if where in self.d["choices"]:
                act2, where = self._pick(where, rid, day, i, hm(start))
                act = act2
            # "cafe", "garden", "stop"... mean this resident's nearest one when the world gives them a "near" table
            place = r["home"] if where == "home" else r["work"] if where == "work" else r.get("near", {}).get(where, where)
            if token in (self.d.get("near_work") or {}).get("tokens", ()) and r.get("work") in self.lots                     and i > 0 and tpl[i - 1][1] == "work":   # only between two spells at work
                place = self._near_work(rid, where, place)   # CITY-096: lunch on a working day near work, not across town
            out.append((m, act, place))
        fixed = [out[0]]
        for b in out[1:]:                                       # keep order and a little room between starts
            fixed.append((min(max(b[0], fixed[-1][0] + 6), 1436), b[1], b[2]))
        return [(max(0.0, m), a, p) for m, a, p in fixed]

    def _use(self, act, place):
        u = self.d["use"]
        if act in ("deliver", "inspect", "stroll", "collect"):
            return None
        if self.lots[place]["kind"] == "home":
            return u["sleep"] if act == "sleep" else u["home"]
        return u.get(act)

    def tram_times(self):
        t = self.d["tram"]
        return list(range(hm(t["first"]), hm(t["last"]) + 1, t["every"]))

    # -- one day for everyone: stays, places taken, walks --
    def plan(self, day):
        if day in self._plans:
            return self._plans[day]
        stays = {}                                    # rid -> list of dict(start, end, act, place, use)
        for rid in self.res:
            bl = self.blocks(rid, day)
            stays[rid] = [dict(start=m, end=bl[i + 1][0] if i + 1 < len(bl) else 1440.0, act=a, place=p, use=self._use(a, p), idx=i)
                          for i, (m, a, p) in enumerate(bl)]
            if self.react:
                stays[rid] = self._take_cover(rid, day, stays[rid])
        # who gets which place to sit, stand or work: first come, first served, worked out for the whole day
        users = {}
        for rid, ss in stays.items():
            for i, s in enumerate(ss):
                if s["use"] is None:
                    continue
                prev = ss[i - 1]["place"] if i else s["place"]
                _, plen = self.net.path(self.anchor[prev], self.anchor[s["place"]])
                arrive = s["start"] + plen / (self.speed(rid) * self.pace)
                s["hold"] = min(s["end"], arrive + 30.0) if s["act"] == "away" else s["end"]   # a place at the stop is only needed until the tram
                users.setdefault((s["place"], s["use"]), []).append((arrive, rid, s))
        for (place, use), lst in users.items():
            spots = self.lots[place]["objects"].get(use, [])
            home = self.lots[place]["kind"] == "home"
            free = [0.0] * len(spots)
            for arrive, rid, s in sorted(lst, key=lambda x: (x[0], x[1])):
                s["slot"] = None
                if home:                               # everyone has their own bed and room
                    mates = self.mates.get(place, [])
                    s["slot"] = mates.index(rid) if rid in mates and mates.index(rid) < len(spots) else None
                    continue
                post = self.res[rid].get("post")
                if post is not None and place == self.res[rid].get("work") and post < len(spots):   # CITY-085: their own post
                    s["slot"], free[post] = post, max(free[post], s["hold"])
                    continue
                first = int(rnd("seat", rid, place) * max(1, len(spots)))
                for k in range(len(spots)):
                    j = (first + k) % len(spots)
                    if free[j] <= arrive:
                        s["slot"], free[j] = j, s["hold"]
                        break
                if s["slot"] is None and use == self.d["use"]["rest"]:          # benches full: sit on the fountain rim
                    alt = self.lots[place]["objects"].get(self.d["use"]["rest_fallback"], [])
                    s["alt"] = [alt[int(rnd("rim", rid, day) * len(alt))]] if alt else None
        plan = {rid: self._timeline(rid, day, ss) for rid, ss in stays.items()}
        if self.traffic is not None:
            plan = self.traffic.apply(day, plan)
        self._plans[day] = plan
        if len(self._plans) > 12:
            self._plans.pop(next(iter(self._plans)))
        return plan

    def speed(self, rid):
        return self.d["signatures"][self.d["roles"][self.res[rid]["role"]]["sig"]]

    def _spot(self, rid, day, s):
        """Where exactly a stay happens."""
        lot = self.lots[s["place"]]
        if s["act"] == "stroll":                       # a stroll starts where the path into the garden meets the loop
            return self.anchor[s["place"]]
        if s["use"] and s.get("slot") is not None:
            return tuple(lot["objects"][s["use"]][s["slot"]])
        if s.get("alt"):
            return tuple(s["alt"][0])
        door = self.anchor[s["place"]]
        x0, y0, x1, y1 = lot["rect"]
        cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
        if s["act"] == "shelter":                      # under the awning: anywhere along the front, close to the wall (CITY-078)
            n = dist(door, (cx, cy)) or 1.0
            ox, oy = (door[0] - cx) / n, (door[1] - cy) / n
            ex, ey = oy, -ox
            half = (abs(ex) * (x1 - x0) + abs(ey) * (y1 - y0)) * 0.45
            k = (rnd("awn", rid, day, s["idx"]) * 2 - 1) * half
            out = 0.5 + rnd("awn2", rid, day, s["idx"]) * 1.2
            return (door[0] + ox * out + ex * k, door[1] + oy * out + ey * k)
        if s["use"] is None or lot["kind"] in ("garden", "market", "stop"):   # at the door, or standing nearby when every place is taken
            n = dist(door, (cx, cy)) or 1.0
            out = (door[0] - (cx - door[0]) / n * 1.6, door[1] - (cy - door[1]) / n * 1.6) if s["use"] is None else door
            a = rnd("stand", rid, day, s["idx"]) * 6.283
            return (out[0] + math.cos(a) * 1.3, out[1] + math.sin(a) * 1.3)
        return (cx + (rnd("sx", rid, s["idx"]) - 0.5) * (x1 - x0) * 0.5, cy + (rnd("sy", rid, s["idx"]) - 0.5) * (y1 - y0) * 0.5)

    def _stroll_at(self, rid, day, s, t):
        turn = 1 if rnd("turn", rid, day) < 0.5 else -1
        closed, cum = self.loops[s["place"]]
        d0 = self._along_loop(closed, cum, self.anchor[s["place"]])
        d = (d0 + turn * self.speed(rid) * 0.6 * self.pace * (t - s["t_in"])) % cum[-1]
        return along(closed, cum, d)

    def _route(self, pos, cur, nxt, target):
        """The way on foot from a place to the next: out of the door, along the streets, in through the next door."""
        if cur["act"] == "stroll":
            q, a, b = self.net.nearest_on(pos)
            opts = []
            for n in (a, b):
                p, L = self.net.path(n, self.anchor[nxt["place"]])
                opts.append((dist(pos, n) + L, [pos] + p))
            route = min(opts)[1]
        elif nxt["place"] == cur["place"]:
            route = [pos]
        else:
            p, _ = self.net.path(self.anchor[cur["place"]], self.anchor[nxt["place"]])
            route = [pos] + p
        route = route + [target]
        return [route[0]] + [p for i, p in enumerate(route[1:]) if dist(p, route[i]) > 1e-6]

    def _timeline(self, rid, day, ss):
        """Turn stays into a gap-free list of pieces: ('stay'|'move'|'away'|'stroll', t0, t1, ...)."""
        v = self.speed(rid) * self.pace                # metres per game minute
        trams = self.tram_times()
        pieces = []
        s0 = ss[0]
        pos = self._spot(rid, day, s0)
        t = 0.0
        cur = s0
        cur["t_in"] = 0.0
        for k in range(1, len(ss) + 1):
            nxt = ss[k] if k < len(ss) else None
            end = nxt["start"] if nxt else 1440.0
            # When to leave: early enough to be on time for the next thing, if the current one has started by then.
            # Someone who arrives after they should have left again does not stop at all and walks on.
            # Running for cover (CITY-078) sets off the moment the rain is noticed, not early to arrive on time.
            route = None
            rush = nxt is not None and nxt.get("rush", False)
            if nxt is not None and cur["act"] != "stroll":
                target = self._spot(rid, day, nxt)
                route = self._route(pos, cur, nxt, target)
                dep = max(t, end if rush else end - lengths(route)[-1] / (v * self.pace_in_rain(rid, day, end, rush)))
            else:
                dep = max(t, end)
            if dep > t + 1e-9:
                if cur["act"] == "away":               # wait for the tram, be gone, come back on a tram
                    board = next((x for x in trams if x >= t), None)
                    if board is None or board >= dep:
                        pieces.append(("stay", t, dep, pos, cur))
                    else:
                        back = max([x for x in trams if board < x <= dep] or [board])
                        pieces.append(("stay", t, board, pos, cur))
                        if back > board:
                            pieces.append(("away", board, back, None, cur))
                        if dep > back:
                            pieces.append(("stay", back, dep, pos, dict(cur, act="arrive")))
                elif cur["act"] == "stroll":
                    pieces.append(("stroll", t, dep, None, cur))
                    pos = self._stroll_at(rid, day, cur, dep)
                else:
                    pieces.append(("stay", t, dep, pos, cur))
            if nxt is None:
                break
            if route is None:                          # a stroll ends wherever it has got to
                target = self._spot(rid, day, nxt)
                route = self._route(pos, cur, nxt, target)
            cum = lengths(route)
            t_arr = min(dep + cum[-1] / (v * self.pace_in_rain(rid, day, dep, rush)), max(dep, 1439.5))   # the day closes at midnight with everyone where they sleep
            if cum[-1] > 1e-6:
                pieces.append(("move", dep, t_arr, (route, cum, cum[-1] / max(t_arr - dep, 1e-6)), nxt))
            nxt["t_in"] = t_arr
            pos, t, cur = target, t_arr, nxt
        return pieces

    # -- where is everyone? --
    def at(self, t):
        """{resident id: dict(pos, act, place, state)} at minute t. state: inside | outside | walking | away | driving | riding."""
        day, m = self.clock.split(t)
        plan = self.plan(day)
        out = {}
        for rid, pieces in plan.items():
            lo, hi = 0, len(pieces) - 1
            while lo < hi:
                mid = (lo + hi + 1) // 2
                if pieces[mid][1] <= m:
                    lo = mid
                else:
                    hi = mid - 1
            kind, t0, t1, payload, s = pieces[lo]
            lot = self.lots[s["place"]]
            if kind == "away":
                out[rid] = dict(pos=None, act="away", place="away", state="away")
            elif kind == "move":
                route, cum, vv = payload
                out[rid] = dict(pos=along(route, cum, (m - t0) * vv), act="walk", place=s["place"], state="walking", to=s["act"])
            elif kind == "stroll":
                out[rid] = dict(pos=self._stroll_at(rid, day, s, m), act="stroll", place=s["place"], state="outside")
            elif kind == "drive":                         # in their car: where the car is (traffic.py)
                out[rid] = dict(pos=self.traffic.car_at(payload, day, m)["pos"], act="drive", place=s["place"], state="driving", car=payload)
            elif kind == "ride":                          # on a bus: where the bus is (traffic.py, CITY-092)
                out[rid] = dict(pos=self.traffic.car_at(payload, day, m)["pos"], act="ride", place=s["place"], state="riding", bus=payload)
            else:
                indoors = lot["kind"] not in ("garden", "market", "stop") and s["use"] is not None and \
                    lot["rect"][0] <= payload[0] <= lot["rect"][2] and lot["rect"][1] <= payload[1] <= lot["rect"][3]
                act = "wait" if s["act"] in ("away", "arrive") else s["act"]
                out[rid] = dict(pos=payload, act=act, place=s["place"], state="inside" if indoors else "outside", slot=s.get("slot"), use=s["use"])
        if self.react:                                   # umbrella, hood or nothing, out in the rain (CITY-080)
            for rid, s in out.items():
                s["cover"] = self.cover(rid, day, m, s["state"], s["act"])
        return out

    def loaded(self, state, viewers, radius=40.0):
        """Residents who need a body right now: within `radius` of any viewer. Everyone else stays as data."""
        return {rid for rid, s in state.items() if s["pos"] is not None and any(dist(s["pos"], v) <= radius for v in viewers)}

    # -- vehicles on fixed routes --
    def vehicles(self, t):
        day, m = self.clock.split(t)
        out = []
        tr = self.d["tram"]
        w = self.d["size"][0]
        run = (tr["stop_x"] + 12.0) / (tr["speed"] * 60.0)            # minutes from the edge of the map to the stop
        for stop_t in self.tram_times():
            dt = m - stop_t
            if -run <= dt < 0:
                out.append(dict(kind="tram", pos=(tr["stop_x"] + dt * tr["speed"] * 60.0, tr["y"]), heading=(1, 0), stopped=False))
            elif 0 <= dt < tr["dwell"]:
                out.append(dict(kind="tram", pos=(tr["stop_x"], tr["y"]), heading=(1, 0), stopped=True))
            elif tr["dwell"] <= dt < tr["dwell"] + (w - tr["stop_x"] + 12.0) / (tr["speed"] * 60.0):
                out.append(dict(kind="tram", pos=(tr["stop_x"] + (dt - tr["dwell"]) * tr["speed"] * 60.0, tr["y"]), heading=(1, 0), stopped=False))
        van = self.d["van"]
        loop = [tuple(p) for p in van["loop"]]
        cum = lengths(loop)
        stops = sorted((self._along_loop(loop, cum, (sx, sy)), dw) for sx, sy, dw in van["stops"])
        for start in van["runs"]:
            dt, d = m - hm(start), 0.0
            if dt < 0 or self.clock.info(day)["sunday"]:
                continue
            tt, last = 0.0, 0.0
            pos, stopped = None, False
            for sd, dw in stops + [(cum[-1], 0.0)]:
                leg = (sd - last) / (van["speed"] * 60.0)
                if dt < tt + leg:
                    pos = along(loop, cum, last + (dt - tt) * van["speed"] * 60.0)
                    break
                tt += leg
                if dt < tt + dw:
                    pos, stopped = along(loop, cum, sd), True
                    break
                tt, last = tt + dw, sd
            if pos:
                out.append(dict(kind="van", pos=pos, stopped=stopped))
        return out

    @staticmethod
    def _along_loop(loop, cum, p):
        best = (1e9, 0.0)
        for i, (a, b) in enumerate(zip(loop, loop[1:])):
            ab = (b[0] - a[0], b[1] - a[1])
            f = max(0.0, min(1.0, ((p[0] - a[0]) * ab[0] + (p[1] - a[1]) * ab[1]) / (ab[0] ** 2 + ab[1] ** 2)))
            q = (a[0] + ab[0] * f, a[1] + ab[1] * f)
            if dist(p, q) < best[0]:
                best = (dist(p, q), cum[i] + f * (cum[i + 1] - cum[i]))
        return best[1]
