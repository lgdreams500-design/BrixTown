"""Loads a city from World/ and checks it. Nothing here generates anything: it only reads the data files.

    import world
    w = world.load()                 # the whole city as one dict
    problems = world.validate(w)     # [] when the data is sound
    city = world.simulation(w)       # a citysim.City running the living-city rules over the whole map
    city = world.simulation(w, traffic=True)   # the same, with residents' cars on the roads (citysim/traffic.py)
"""
import copy, json, math, os, sys

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
    for name in ("roles", "choices", "calendar", "vegetation", "signatures", "traffic", "dialogue", "speech", "sound", "dodge", "economy", "quests", "designer", "dressing", "sky", "crash", "surfaces", "storeys", "weather_react", "estate", "building_kit", "planning", "careers", "furnishing", "settings", "room_plan", "building_styles", "mall_outlets", "mall_goods", "achievements", "boards", "buy_menu"):
        w[name] = _read(rules, name + ".json")
    roads = os.path.join(base, "roads.json")           # the road network (CITY-043); written by make_roads_01.py
    w["roads"] = _read(roads) if os.path.exists(roads) else None
    transit = os.path.join(base, "transit.json")       # bus lines and stops (CITY-091)
    w["transit"] = _read(transit) if os.path.exists(transit) else None
    return w


def lots(w):
    return [l for b in w["blocks"] for l in b["lots"]]


def extent(w, b):
    """How far a block's ground runs east and south of its origin: one cell, or several with "span" (the grid streets
    between them are the block's)."""
    g = w["city"]["grid"]
    cols, rows = b.get("span", [1, 1])
    return b["size"] + (cols - 1) * g["pitch"], b["size"] + (rows - 1) * g["pitch"]


def cells_of(b):
    cols, rows = b.get("span", [1, 1])
    i, j = b["grid"]
    return [(i + di, j + dj) for di in range(cols) for dj in range(rows)]


def lot_frame(l):
    """A function taking a world point to the lot's own frame: for a lot turned by "angle" degrees about its centre,
    the point turned back; for any other lot, the point as it is."""
    a = l.get("angle")
    if not a:
        return lambda p: p
    x0, z0, x1, z1 = l["rect"]
    cx, cz, t = (x0 + x1) / 2, (z0 + z1) / 2, -math.radians(a)
    c, s = math.cos(t), math.sin(t)
    return lambda p: (cx + (p[0] - cx) * c - (p[1] - cz) * s, cz + (p[0] - cx) * s + (p[1] - cz) * c)


def lots_inside(w):
    """The lots of the blocks wholly inside the town wall: the part of the city the road network (roads.json streets)
    serves. Outside it, roads.json "links" and the lanes are walked but not yet driven."""
    return [l for b in w["blocks"] if all(in_wall(w, q) for q in cells_of(b)) for l in b["lots"]]


def in_wall(w, cell):
    """Is this grid cell inside the town wall (city.json "wall", grid lines i0, j0, i1, j1)? With no wall, all are."""
    wl = w["city"].get("wall")
    if not wl:
        return True
    i0, j0, i1, j1 = wl["cells"]
    return i0 <= cell[0] < i1 and j0 <= cell[1] < j1


def validate(w):
    """Every rule a block file must keep. Returns a list of problems in plain words."""
    out = []
    g = w["city"]["grid"]
    seen, ids = {}, set()
    for b in w["blocks"]:
        i, j = b["grid"]
        sx, sz = extent(w, b)
        for cell in cells_of(b):                                  # a block may cover several cells ("span")
            if cell in seen:
                out.append("two blocks at %s: %s and %s" % (cell, seen[cell], b["id"]))
            seen[cell] = b["id"]
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
            if a0 < x0 - pave or c0 < z0 - pave or a1 > x0 + sx + pave or c1 > z0 + sz + pave:
                out.append("%s: footprint leaves its block" % l["id"])
            if l["kind"] not in OPEN_KINDS:
                unturn = lot_frame(l)                                 # a turned lot: its door and places in its own frame
                dx, dz = unturn(l["door"])
                e = 0.05 if l.get("angle") else 0.01                    # a turned lot's angle is kept to 0.1 degrees
                on_edge = (abs(dx - a0) < e or abs(dx - a1) < e) and c0 - e <= dz <= c1 + e or (abs(dz - c0) < e or abs(dz - c1) < e) and a0 - e <= dx <= a1 + e
                if not on_edge:
                    out.append("%s: door is not on a wall" % l["id"])
                for kind, pts in l["objects"].items():
                    for p in pts:
                        p = unturn(p)
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
    out += career_problems(w)
    if not out:                                        # every door must be reachable on foot from every other
        c = simulation(w)
        dd, _ = c.net._from(next(iter(c.anchor.values())))
        out += ["%s cannot be reached on foot" % k for k, a in c.anchor.items() if a not in dd]
    if w.get("roads") and not out:
        import traffic
        out += traffic.Roads(w["roads"], w["traffic"]).problems(lots_inside(w))   # cars drive inside the wall for now
    out += dialogue_problems(w) + speech_problems(w) + sound_problems(w) + economy_problems(w) + mall_goods_problems(w) + buy_menu_problems(w) + mall_visit_problems(w) + estate_problems(w) + quest_problems(w) + designer_problems(w) + sky_problems(w) + garden_problems(w) + react_problems(w) + achievement_problems(w)
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
                if set(o) - {"text", "next", "if", "do", "act"}:
                    out.append("%s: unknown keys on a reply %s" % (at, sorted(set(o) - {"text", "next", "if", "do", "act"})))
                for a in o.get("act", []):                 # opening the shop or bank counter where the resident works (ECO-011, BANK-007)
                    if a not in ("shop", "bank"):
                        out.append("%s: unknown act %s" % (at, a))
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


SOUND_ACTIONS = ("prompt", "sit", "lie", "stand", "lift", "jump", "talk_open", "talk_choose", "talk_close", "car_door", "map", "clock", "hour", "radio")
SOUND_KINDS = ("tick", "thud", "rustle", "chime", "clink", "hiss", "snip", "type", "snore", "creak", "whoosh", "scrape", "clank")
WEATHER_KINDS = ("clear", "cloud", "rain", "fog")       # what citysim.weather() can answer


REACT_PALETTE = ("cream", "ochre", "terra", "olive", "teal", "blue", "indigo", "plum", "brown", "charcoal", "slate", "black", "sand")   # people.js K names


def react_problems(w):
    """Every rule World/rules/weather_react.json must keep (CITY-077 to CITY-080)."""
    r, out = w.get("weather_react"), []
    if not r:
        return ["weather_react: World/rules/weather_react.json is missing"]
    chance = lambda v: isinstance(v, (int, float)) and 0 <= v <= 1
    for part, keys in (("umbrella", ("rain_day", "dry_day")), ("coat", ("rain_day", "hood"))):
        for k in keys:
            if not chance(r.get(part, {}).get(k)):
                out.append("weather_react: %s.%s should be from 0 to 1" % (part, k))
    if r["umbrella"]["dry_day"] > r["umbrella"]["rain_day"]:
        out.append("weather_react: more umbrellas on dry days than on wet ones")
    s = r["coat"].get("season", [])
    if len(s) != len(w["calendar"]["calendar"]["seasons"]) or not all(chance(v) for v in s):
        out.append("weather_react: coat.season should be one chance per season")
    for part in ("umbrella", "coat"):
        cols = r[part].get("colours", [])
        if not cols or any(c not in REACT_PALETTE for c in cols):
            out.append("weather_react: %s.colours should be names from the people palette" % part)
    sh = r.get("shelter", {})
    n = sh.get("notice_minutes", [])
    if len(n) != 2 or not 0 <= n[0] <= n[1] <= 30:
        out.append("weather_react: shelter.notice_minutes should be [least, most], at most 30")
    if not sh.get("to") or any(t != "home" and t not in w["choices"]["choices"] and t not in ("cafe", "grocer", "library", "museum", "cinema") for t in sh["to"]):
        out.append("weather_react: shelter.to should name kinds of place")
    p = r.get("pace", {})
    if not (1 <= p.get("umbrella", 0) <= p.get("hurry", 0) <= p.get("run", 0) <= 3):
        out.append("weather_react: pace should go 1 <= umbrella <= hurry <= run <= 3")
    if any(k not in OPEN_KINDS for k in r.get("open_kinds", [])):
        out.append("weather_react: open_kinds should be places without walls")
    return out


def sky_problems(w):
    """Every rule World/rules/sky.json must keep (WLD-035, WLD-036)."""
    s, out = w["sky"], []
    unit = lambda v: isinstance(v, (int, float)) and not isinstance(v, bool) and 0 <= v <= 1
    if not (isinstance(s.get("zenith"), list) and len(s["zenith"]) == 3 and all(unit(v) for v in s["zenith"])):
        out.append("sky: zenith should be three numbers from 0 to 1")
    if not unit(s.get("depth")):
        out.append("sky: depth should be from 0 to 1")
    for k in ("sun_ref", "brightness"):
        if not (isinstance(s.get(k), (int, float)) and s[k] > 0):
            out.append("sky: %s should be above 0" % k)
    c = s.get("clouds", {})
    if not isinstance(c.get("follow_weather"), bool):
        out.append("sky: clouds.follow_weather should be true or false")
    cover = c.get("cover", {})
    for k in WEATHER_KINDS:
        if not unit(cover.get(k)):
            out.append("sky: clouds.cover.%s missing or not from 0 to 1" % k)
    out += ["sky: clouds.cover has an unknown kind of weather %s" % k for k in cover if k not in WEATHER_KINDS]
    if all(unit(cover.get(k)) for k in WEATHER_KINDS) and not cover["clear"] <= cover["cloud"] <= cover["rain"]:
        out.append("sky: cover should grow from clear to cloud to rain")
    for k in ("scale", "soft", "speed", "change", "lit", "shade", "blend_minutes"):
        if not (isinstance(c.get(k), (int, float)) and c[k] > 0):
            out.append("sky: clouds.%s should be above 0" % k)
    if isinstance(c.get("blend_minutes"), (int, float)) and c["blend_minutes"] > 180:
        out.append("sky: clouds.blend_minutes should be at most 180 (half a weather slot)")
    wind = c.get("wind")
    if not (isinstance(wind, list) and len(wind) == 2 and any(wind)):
        out.append("sky: clouds.wind should be two numbers, not both 0")
    # WLD-037: what each kind of weather does to the light, and the rain itself
    num = lambda v: isinstance(v, (int, float)) and not isinstance(v, bool)
    if not unit(s.get("overcast_fill")):
        out.append("sky: overcast_fill should be from 0 to 1")
    wx = s.get("weather", {})
    for k in WEATHER_KINDS:
        for part, top in (("sun", 1), ("haze", 20), ("veil", 1), ("grey", 1), ("rain", 1)):
            v = wx.get(k, {}).get(part)
            if not (num(v) and 0 <= v <= top):
                out.append("sky: weather.%s.%s missing or not from 0 to %s" % (k, part, top))
    out += ["sky: weather has an unknown kind %s" % k for k in wx if k not in WEATHER_KINDS]
    ok = lambda k, part: num(wx.get(k, {}).get(part))
    if ok("clear", "rain") and ok("rain", "rain") and not (wx["clear"]["rain"] == 0 and wx["rain"]["rain"] > 0):
        out.append("sky: it should rain in rain and not in clear weather")
    if ok("clear", "sun") and ok("rain", "sun") and not wx["rain"]["sun"] < wx["clear"]["sun"]:
        out.append("sky: rain should let less sun through than clear weather")
    if ok("clear", "haze") and ok("fog", "haze") and not wx["fog"]["haze"] > wx["clear"]["haze"]:
        out.append("sky: fog should be thicker than clear air")
    r = s.get("rain", {})
    for k in ("alpha", "speed", "length", "wet_dark", "wet_sheen", "puddles", "ripples", "dry_minutes"):
        if not (num(r.get(k)) and r[k] > 0):
            out.append("sky: rain.%s should be above 0" % k)
    if not num(r.get("slant")):
        out.append("sky: rain.slant should be a number")
    if num(r.get("dry_minutes")) and r["dry_minutes"] > 360:
        out.append("sky: rain.dry_minutes should be at most 360 (one weather slot)")
    return out + night_sky_problems(w) + planet_problems(w)


def night_sky_problems(w):
    """Every rule the night sky in World/rules/sky.json must keep (WLD-042)."""
    n, out = w["sky"].get("night"), []
    num = lambda v: isinstance(v, (int, float)) and not isinstance(v, bool)
    if not isinstance(n, dict):
        return ["sky: night is missing"]
    col = lambda v: isinstance(v, list) and len(v) == 3 and all(num(x) and 0 <= x <= 1 for x in v)
    for k in ("zenith", "horizon", "city_glow", "fill"):
        if not col(n.get(k)):
            out.append("sky: night.%s should be three numbers from 0 to 1" % k)
    if col(n.get("zenith")) and col(n.get("horizon")) and sum(n["zenith"]) >= sum(n["horizon"]):
        out.append("sky: night.zenith should be darker than night.horizon")
    if not (num(n.get("latitude")) and -70 <= n["latitude"] <= 70):
        out.append("sky: night.latitude should be from -70 to 70 degrees")
    cal = w["calendar"]["calendar"]
    if n.get("year_days") != cal["days_per_season"] * len(cal["seasons"]):
        out.append("sky: night.year_days should be the calendar's year (%d days)" % (cal["days_per_season"] * len(cal["seasons"])))
    st, mw, mo = n.get("stars", {}), n.get("milky_way", {}), n.get("moon", {})
    for part, d, k, lo, hi in (("stars", st, "brightness", 0, 5), ("stars", st, "density", 0, .6), ("stars", st, "twinkle", 0, 1),
                               ("milky_way", mw, "brightness", 0, 1), ("milky_way", mw, "width", .02, .5), ("milky_way", mw, "dust", 0, 1),
                               ("moon", mo, "cycle_days", 1, 60), ("moon", mo, "offset_days", 0, 60), ("moon", mo, "size", .002, .05),
                               ("moon", mo, "glow", 0, 5), ("moon", mo, "dims_stars", 0, 1), ("night", n, "lamp_wrap", 0, .15)):
        if not (num(d.get(k)) and lo <= d[k] <= hi):
            out.append("sky: %s.%s missing or not from %s to %s" % (part if part == "night" else "night." + part, k, lo, hi))
    if not col(mo.get("light")):
        out.append("sky: night.moon.light should be three numbers from 0 to 1")
    wx = n.get("weather", {})
    for k in WEATHER_KINDS:
        for part in ("stars", "lamp_glow", "cover"):
            if not (num(wx.get(k, {}).get(part)) and 0 <= wx[k][part] <= 1):
                out.append("sky: night.weather.%s.%s missing or not from 0 to 1" % (k, part))
    out += ["sky: night.weather has an unknown kind %s" % k for k in wx if k not in WEATHER_KINDS]
    if all(num(wx.get(k, {}).get("stars")) for k in ("clear", "rain")) and wx["rain"]["stars"] >= wx["clear"]["stars"]:
        out.append("sky: fewer stars should show in rain than on a clear night")
    pa = n.get("paths", {})
    for k, lo, hi in (("sun_lift", 0, .3), ("sun_size", .002, .05), ("sun_glare_ease", 0, 1), ("moon_by_day", 0, 1), ("moon_swing", 0, 30), ("moon_tilt", 0, 10), ("moon_node_days", 1, 10000)):
        if not (num(pa.get(k)) and lo <= pa[k] <= hi):
            out.append("sky: night.paths.%s missing or not from %s to %s" % (k, lo, hi))
    if num(n.get("latitude")):
        for season, dec in sun_heights(w).items():
            if dec is None or abs(dec) > 35:
                out.append("sky: the calendar's day in %s cannot be seen from latitude %s" % (season, n["latitude"]))
    return out


def sun_heights(w):
    """WLD-043: the sun's declination in degrees for each season, the one that makes it rise and set at the calendar's times when seen
    from sky.json night.latitude (as Prototype/play/play.js sunDec() works it out). None where no declination can do that."""
    cal, la = w["calendar"]["calendar"], math.radians(w["sky"]["night"]["latitude"])
    out = {}
    for s, name in enumerate(cal["seasons"]):
        half = (cal["sunset"][s] - cal["sunrise"][s]) / 24 * math.pi
        out[name] = None if not 0 < half < math.pi or math.tan(la) == 0 else math.degrees(math.atan(-math.cos(half) / math.tan(la)))
    return out


def achievement_problems(w):
    """Every rule World/rules/achievements.json must keep (QST-009); the rules themselves are Tools/citysim/achievements.py."""
    import achievements
    return achievements.problems(w["achievements"])


def reward_coins(s, amount):
    """How many coins spring out for a reward of `amount` minor units (SND-019): the last tier the amount reaches."""
    n = 0
    for least, coins in s["rewards"]["coins"]["reward_tiers"]:
        if amount >= least:
            n = coins
    return n


def reward_events(s, tx):
    """What the player is shown for new ledger transactions (SND-019, SND-020): rewards paid to the player from the
    rewards account, each with its coins, and all the interest paid by the bank as one. tx: [id, day, minute, memo, lines].
    The same rule as Prototype/play/rewards.js."""
    lg, out, interest = s["rewards"]["ledger"], [], {}
    for t in tx:
        memo, lines = t[3], t[4]
        got = {a: v for a, v in lines if a.startswith("p:") and v > 0}
        paid = sum(got.values())
        if not paid:
            continue
        if any(a == lg["reward_from"] and v < 0 for a, v in lines):
            out.append({"kind": "reward", "amount": paid, "coins": reward_coins(s, paid), "memo": memo})
        elif memo.startswith(lg["interest_memo"]) and any(a == lg["interest_from"] and v < 0 for a, v in lines):
            for a, v in got.items():
                interest[a] = interest.get(a, 0) + v
    if interest:
        out.append({"kind": "interest", "amount": sum(interest.values()), "accounts": sorted(interest)})
    return out


def reward_sound_problems(s):
    """Every rule the reward sounds and coins in World/rules/sound.json must keep (SND-019, SND-020)."""
    r, out = s.get("rewards"), []
    if not r:
        return ["sound: no rewards (the sounds and coins for money coming in)"]
    num = lambda v: isinstance(v, (int, float)) and not isinstance(v, bool)
    for name, v in r["voices"].items():
        if not v["partials"] or any(len(p) != 3 or not all(num(x) and x > 0 for x in p) for p in v["partials"]):
            out.append("sound: reward voice %s needs partials of [pitch, level, length], all above 0" % name)
    for part, keys in (("quest", ("voice",)), ("interest", ("voice", "end_voice"))):
        for k in keys:
            if r[part][k] not in r["voices"]:
                out.append("sound: rewards.%s.%s is not a reward voice: %s" % (part, k, r[part][k]))
        if not r[part]["scale"] or r[part]["scale"][0] != 0:
            out.append("sound: rewards.%s.scale must start on the root (0)" % part)
        if not 36 <= r[part]["root"] <= 96:
            out.append("sound: rewards.%s.root should be a MIDI note from 36 to 96" % part)
    for part in ("sweep", "chord", "twinkle"):
        a = r.get("achievement", {}).get(part)
        if not a or not a.get("notes") or a.get("voice") not in r["voices"] or not a.get("len", 0) > 0:
            out.append("sound: rewards.achievement.%s needs notes, a length and a reward voice" % part)
    q = r["quest"]
    if not q["fanfare"] or not q["chord"]:
        out.append("sound: the quest reward needs a fanfare and a chord")
    if any(len(n) != 2 or n[0] < 0 for n in q["fanfare"]):
        out.append("sound: each fanfare note is [seconds, steps above the root]")
    if any(len(n) != 4 or n[0] < 0 or not 0 <= n[2] <= 1 or n[3] <= 0 for n in r["interest"]["end"]) or not r["interest"]["end"]:
        out.append("sound: each interest end note is [seconds, note, level from 0 to 1, seconds long]")
    for k in ("reward_from", "interest_from", "interest_memo"):
        if not r["ledger"].get(k):
            out.append("sound: rewards.ledger.%s is missing" % k)
    c = r["coins"]
    t = c["reward_tiers"]
    if not t or t[0][0] != 0 or any(b[0] <= a[0] or b[1] < a[1] for a, b in zip(t, t[1:])) or any(not 1 <= n <= 40 for _, n in t):
        out.append("sound: rewards.coins.reward_tiers must start at 0 and rise, with 1 to 40 coins each")
    if not 1 <= c["interest_coins"] <= 40:
        out.append("sound: rewards.coins.interest_coins should be from 1 to 40")
    for k in ("ring_m", "up_mps"):
        if len(c[k]) != 2 or c[k][0] <= 0 or c[k][1] < c[k][0]:
            out.append("sound: rewards.coins.%s should be [least, most], above 0" % k)
    for k in ("start_s", "gap_s", "from_m", "fall_mps2", "size_m", "hover_m", "pull_after_s", "pull_gap_s", "lift_s", "pull_s", "fly_s", "interest_start_s", "interest_gap_s", "interest_fly_s", "slip_s"):
        if not (num(c.get(k)) and c[k] > 0):
            out.append("sound: rewards.coins.%s should be above 0" % k)
    if not 0 <= c["bounce"] < 1:
        out.append("sound: rewards.coins.bounce should be from 0 to under 1")
    return out


def planet_place(w):
    """WLD-065: where the planet hangs in the town's sky, as a direction (east +x, up +y, south +z), from sky.json planet.azimuth
    (degrees from north through east) and planet.elevation, as Prototype/play/play.js places it."""
    p = w["sky"]["planet"]
    az, el = math.radians(p["azimuth"]), math.radians(p["elevation"])
    return (math.sin(az) * math.cos(el), math.sin(el), -math.cos(az) * math.cos(el))


def sun_place(w, season, hour_angle):
    """Where the sun is in a season at an hour angle (radians, 0 at the middle of the calendar's day, growing westward), seen from
    sky.json night.latitude: the same direction play.js skyAt() gives."""
    la, dec = math.radians(w["sky"]["night"]["latitude"]), math.radians(sun_heights(w)[season])
    cd, sd, ch = math.cos(dec), math.sin(dec), math.cos(hour_angle)
    return (-cd * math.sin(hour_angle), cd * ch * math.cos(la) + sd * math.sin(la), cd * ch * math.sin(la) - sd * math.cos(la))


def planet_sun_angle(w, season, hour_angle):
    """WLD-065: the angle in radians between the sun and the planet's centre, seen from the town."""
    return math.acos(max(-1.0, min(1.0, sum(a * b for a, b in zip(planet_place(w), sun_place(w, season, hour_angle))))))


def planet_lit(w, season, hour_angle):
    """WLD-065: the share of the planet's face in sunlight, 0 new to 1 full, worked out from where the sun is and nothing else."""
    return (1 - math.cos(planet_sun_angle(w, season, hour_angle))) / 2


def planet_sun_gap(w):
    """WLD-065: for each season, the nearest the sun comes to the planet's centre on its whole path round the sky, in radians."""
    return {name: min(planet_sun_angle(w, name, 2 * math.pi * k / 1440) for k in range(1440))
            for name, dec in sun_heights(w).items() if dec is not None}


def planet_problems(w):
    """Every rule the planet in World/rules/sky.json must keep (WLD-065). A sky with no planet is allowed."""
    p, out = w["sky"].get("planet"), []
    if p is None:
        return out
    if not isinstance(p, dict):
        return ["sky: planet should be a set of numbers"]
    num = lambda v: isinstance(v, (int, float)) and not isinstance(v, bool)
    if not isinstance(p.get("show"), bool):
        out.append("sky: planet.show should be true or false")
    ranges = (("azimuth", 0, 360), ("elevation", 5, 85), ("size", .02, .35), ("lean", -90, 90), ("tilt", -30, 30), ("spin_hours", 1, 1000),
              ("night", 0, 5), ("by_day", 0, .5), ("clouds", 0, 1), ("towns", 0, 2))
    for k, lo, hi in ranges:
        if not (num(p.get(k)) and lo <= p[k] <= hi):
            out.append("sky: planet.%s missing or not from %s to %s" % (k, lo, hi))
    r = p.get("rings")
    if r is not None:
        if not (isinstance(r, dict) and all(num(r.get(k)) for k in ("inner", "outer", "gap", "opacity"))):
            out.append("sky: planet.rings should have inner, outer, gap and opacity")
        else:
            if not 1.05 <= r["inner"] < r["outer"] <= 4:
                out.append("sky: planet.rings should lie outside the planet, inner edge inside outer edge")
            elif not r["inner"] < r["gap"] < r["outer"]:
                out.append("sky: planet.rings.gap should lie between the rings' edges")
            if not 0 <= r["opacity"] <= 1:
                out.append("sky: planet.rings.opacity should be from 0 to 1")
    if out or not num(w["sky"].get("night", {}).get("latitude")):
        return out
    if math.radians(p["elevation"]) - p["size"] < math.radians(3):
        out.append("sky: the whole planet should stand clear of the horizon")
    clear = p["size"] * 1.25 + w["sky"]["night"].get("paths", {}).get("sun_size", 0)
    for season, gap in planet_sun_gap(w).items():
        if gap < clear:
            out.append("sky: in %s the sun would pass behind the planet (%.0f degrees from its centre)" % (season, math.degrees(gap)))
    return out


def sound_problems(w, acts=None):
    """Every rule World/rules/sound.json must keep (SND-001 to SND-013). acts: the activity names residents can have."""
    import re
    s, out = w["sound"], []

    def hhmm(t, where):
        if not (isinstance(t, str) and re.fullmatch(r"([01]\d|2[0-3]):[0-5]\d", t)):
            out.append("sound: %s is not a time (HH:MM): %r" % (where, t))

    def gains(x, path):                                 # every gain is a number from 0 to 1
        if isinstance(x, dict):
            for k, v in x.items():
                if (k == "gain" or k.endswith("_gain")) and not (isinstance(v, (int, float)) and 0 <= v <= 1):
                    out.append("sound: %s%s should be from 0 to 1, not %r" % (path, k, v))
                gains(v, path + k + ".")
        elif isinstance(x, list):
            for v in x:
                gains(v, path)
    gains(s, "")
    for k in ("master", "world", "voice", "action", "music"):
        if not 0 <= s["mix"].get(k, -1) <= 1:
            out.append("sound: mix.%s missing or not from 0 to 1" % k)
    a = s["ambience"]
    for part in ("birds", "crickets"):
        hhmm(a[part]["from"], part + ".from")
        hhmm(a[part]["to"], part + ".to")
    for name in ("stone", "wood", "grass"):
        if name not in s["footsteps"]["surfaces"]:
            out.append("sound: no footstep sound for %s" % name)
    for name in SOUND_ACTIONS:
        r = s["actions"].get(name)
        if not r:
            out.append("sound: no sound for the action %s" % name)
        elif r["kind"] not in SOUND_KINDS:
            out.append("sound: action %s has an unknown kind %s" % (name, r["kind"]))
    out += reward_sound_problems(s)
    shapes = w["speech"]["shapes"]
    for sh in s["voices"]["shapes"]:
        if sh not in shapes:
            out.append("sound: voice for unknown mouth shape %s" % sh)
    for sh in ("AA", "EH", "EE", "OH", "OO"):               # every open vowel the lip sync shows has a voice
        if sh not in s["voices"]["shapes"]:
            out.append("sound: mouth shape %s has no voice" % sh)
    pe = s["people"]                                      # SND-015 to SND-017: people's steps, doings and talk
    known = None
    if acts is not None:                                  # the day's activities, and those residents choose (choices.json)
        known = set(acts) | {o[0] for c in w["choices"]["choices"].values() for opts in c.values() for o in opts}
    roles = set(w["roles"]["roles"])
    for act in pe["chat"]["acts"]:
        if known is not None and act not in known:
            out.append("sound: chat for unknown activity %s" % act)
    for name, r in pe["sounds"].items():
        if r["kind"] not in SOUND_KINDS:
            out.append("sound: people sound %s has an unknown kind %s" % (name, r["kind"]))
    for pose, name in pe["poses"].items():
        if name not in pe["sounds"]:
            out.append("sound: no sound %s for %s" % (name, pose))
    for role in pe["pitch"]:
        if role not in roles:
            out.append("sound: voice pitch for unknown role %s" % role)
    for k, a in enumerate(pe["activities"]):
        what = "activity %d (%s)" % (k, "/".join(a["acts"]))
        if not a.get("sound") and not a.get("call"):
            out.append("sound: %s makes no sound" % what)
        if a.get("sound") and a["sound"] not in pe["sounds"]:
            out.append("sound: %s plays unknown sound %s" % (what, a["sound"]))
        for act in a["acts"]:
            if known is not None and act not in known:
                out.append("sound: %s is for unknown activity %s" % (what, act))
        for role in a.get("roles", []):
            if role not in roles:
                out.append("sound: %s is for unknown role %s" % (what, role))
        lo, hi = a["every"]
        if not 0 < lo <= hi:
            out.append("sound: %s repeats at a bad interval %s" % (what, a["every"]))
    m = s["music"]

    def style(st, where):
        if st["scale"] not in m["scales"]:
            out.append("sound: %s uses unknown scale %s" % (where, st["scale"]))
        if st["voice"] not in m["voices"]:
            out.append("sound: %s uses unknown voice %s" % (where, st["voice"]))
        if st.get("drums") is not None and st["drums"] not in m["drums"]:
            out.append("sound: %s uses unknown drums %s" % (where, st["drums"]))
        if not st["chords"] or any(c not in range(1, 8) for c in st["chords"]):
            out.append("sound: %s has chords outside 1 to 7" % where)
        if not 40 <= st["bpm"] <= 200:
            out.append("sound: %s tempo %s is out of range" % (where, st["bpm"]))
        if st.get("comp") is not None and st["comp"] not in m["voices"]:
            out.append("sound: %s plays its chords on unknown voice %s" % (where, st["comp"]))
        solo = m.get("chords_only", [])                    # the piano plays chords, never a tune or one note at a time (owner, 2026-11-22)
        if st["voice"] in solo or st.get("comp") in solo and st.get("comp_pattern") not in ("block", "pad", "offbeat"):
            out.append("sound: %s plays single notes on %s, which only plays chords" % (where, st["voice"] if st["voice"] in solo else st["comp"]))
        if st.get("bass_voice") is not None and st["bass_voice"] not in ("bass", "upright"):
            out.append("sound: %s walks its bass on unknown voice %s" % (where, st["bass_voice"]))
        if st.get("under") is not None and st["under"] not in m["voices"]:
            out.append("sound: %s holds its under layer on unknown voice %s" % (where, st["under"]))
        for lv in ("comp_level", "under_level", "drum_level"):
            if lv in st and not (isinstance(st[lv], (int, float)) and 0 <= st[lv] <= 1.5):
                out.append("sound: %s %s should be from 0 to 1.5" % (where, lv))
        if st.get("comp_pattern") is not None and st["comp_pattern"] not in m.get("patterns", []):
            out.append("sound: %s uses unknown comping pattern %s" % (where, st["comp_pattern"]))
        if st.get("bass") not in (None, True, False) and st["bass"] not in m.get("basslines", []):
            out.append("sound: %s uses unknown bass line %s" % (where, st["bass"]))
    for k, sc in m["scales"].items():
        if len(sc) != 7 or sc[0] != 0 or sorted(sc) != sc:
            out.append("sound: scale %s should be seven rising steps from 0" % k)
    moods = m["score"]["moods"]
    for md in moods:
        hhmm(md["from"], "score mood " + md["name"])
        style(md, "score mood " + md["name"])
    if [md["from"] for md in moods] != sorted(md["from"] for md in moods):
        out.append("sound: score moods are not in order of the day")
    for k, st in m["styles"].items():
        style(st, "style " + k)
    kinds = {l["kind"] for l in lots(w)}
    for kind, p in m["places"].items():
        if kind not in kinds:
            out.append("sound: music for a kind of place the city does not have: %s" % kind)
        if p["style"] not in m["styles"]:
            out.append("sound: %s plays unknown style %s" % (kind, p["style"]))
        hhmm(p["from"], kind + ".from")
        hhmm(p["to"], kind + ".to")
    shows = s.get("podcasts", {}).get("shows", {})
    for k, ph in enumerate(m.get("phrases", [])):            # melody rhythms: notes in order inside two bars of 16ths
        if not ph or any(not (0 <= a < 32 and 0 < n <= 32 - a) for a, n in ph) or [a for a, _ in ph] != sorted({a for a, _ in ph}):
            out.append("sound: melody phrase %d does not fit two bars in order" % k)
    for st in m["radio"]:
        if ("style" in st) == ("podcast" in st):
            out.append("sound: radio station %s needs a style or a podcast, not both" % st.get("name"))
        elif "style" in st and st["style"] not in m["styles"]:
            out.append("sound: radio station %s has no style %s" % (st.get("name"), st["style"]))
        elif "podcast" in st and st["podcast"] not in shows:
            out.append("sound: radio station %s plays unknown podcast %s" % (st.get("name"), st["podcast"]))
    for sid, sh in shows.items():                       # SND-014: every line is said by one of the show's hosts
        if sh["bed"] not in m["styles"]:
            out.append("sound: podcast %s has unknown bed music %s" % (sid, sh["bed"]))
        for hid, h in sh["hosts"].items():
            if not 60 <= h["pitch"] <= 400:
                out.append("sound: podcast %s host %s has a pitch out of range" % (sid, hid))
        if not sh["episodes"]:
            out.append("sound: podcast %s has no episodes" % sid)
        for ep in sh["episodes"]:
            for host, text in ep["lines"]:
                if host not in sh["hosts"]:
                    out.append("sound: podcast %s, %s: unknown host %s" % (sid, ep["title"], host))
                if not re.search(r"[a-z]", text.lower()):
                    out.append("sound: podcast %s, %s: a line with nothing to say" % (sid, ep["title"]))
    return out


def economy_problems(w, acts=None):
    """Every rule World/rules/economy.json must keep (ECO-001, ECO-012, BANK-001, BANK-002, PROP-008)."""
    e, out = w["economy"], []
    L = {l["id"]: l for l in lots(w)}

    def whole(v):                                      # ECO-001: money is whole chips
        return type(v) is int

    if e["currency"].get("per") != 100 or e["currency"].get("symbol") != "Bx":
        out.append("economy: the currency is the brix, Bx, of 100 chips")
    for a in e["system_accounts"]:
        if not a.startswith("sys:"):
            out.append("economy: system account %s must start with sys:" % a)
    for a in ("sys:start", "sys:founding", "sys:rewards", "sys:residents", "sys:fees"):
        if a not in e["system_accounts"]:
            out.append("economy: missing system account %s" % a)
    for k, v in e["start"].items():
        if not whole(v) or v < 0:
            out.append("economy: start %s must be whole chips" % k)
    B = e["bank"]
    if L.get(B["lot"], {}).get("kind") != "bank":
        out.append("economy: the bank lot %s is not a bank" % B["lot"])
    if not whole(B["vault"]) or B["vault"] <= 0:
        out.append("economy: the vault must be whole chips")
    if set(B["accounts"]) != {"current", "savings"}:
        out.append("economy: the bank has a current and a savings account")
    for k, a in B["accounts"].items():
        if not whole(a["rate_bp"]) or a["rate_bp"] < 0:
            out.append("economy: %s rate must be whole basis points" % k)
    C = B["loan"]
    for k in ("min", "max", "step", "rate_bp", "max_loans", "late_fee", "default_after_missed"):
        if not whole(C[k]) or C[k] < 0:
            out.append("economy: loan %s must be a whole number" % k)
    if not (0 < C["min"] <= C["max"]) or C["step"] <= 0 or C["min"] % C["step"] or C["max"] % C["step"]:
        out.append("economy: loan sizes do not fit their step")
    if not C["terms"] or any(not whole(t) or t < 1 for t in C["terms"]):
        out.append("economy: loan terms are whole days")
    items = e["items"]
    for k, i in items.items():
        if not whole(i.get("price")) or i["price"] < 0 or not i.get("name"):
            out.append("economy: item %s needs a name and a price in whole chips" % k)
    for k, S in e["shops"].items():
        for it in S["sells"] + S["buys"]:
            if it not in items:
                out.append("economy: shop %s has unknown item %s" % (k, it))
        for it in S["buys"]:
            if it in items and not items[it]["sell"]:
                out.append("economy: shop %s buys %s, which cannot be sold" % (k, it))
        for it in S["sells"]:
            if it in items and (items[it].get("quest") or items[it]["price"] <= 0):
                out.append("economy: shop %s sells quest item %s" % (k, it))
        if not whole(S["till"]) or S["till"] <= 0:
            out.append("economy: shop %s till must be whole chips" % k)
    for l in L.values():                               # every shop and cafe is one of the shop types
        if l["kind"] in ("shop", "food") and not any(l["kind"] == S["match"]["kind"] and S["match"].get("name_has", "") in l["name"] for S in e["shops"].values()):
            out.append("economy: %s (%s) matches no shop type" % (l["id"], l["name"]))
    T, P = e["stock"], e["price"]
    if not whole(T["target"]) or T["target"] < 1 or not 0 <= T["restock_minute"] < 1440:
        out.append("economy: stock needs a target and a restock minute")
    for a in T["visit_acts"] + e["stall"]["visit_acts"]:
        if acts is not None and a not in acts:
            out.append("economy: unknown activity %s" % a)
    if not (whole(P["min_bp"]) and whole(P["max_bp"]) and whole(P["slope_bp"]) and P["min_bp"] <= 10000 <= P["max_bp"]):
        out.append("economy: price bounds must hold the usual price")
    if not 0 < P["sell_share_bp"] <= 10000:
        out.append("economy: shops pay a share of their price")
    Q = e["reputation"]
    if Q["min"] >= 0 or Q["max"] <= 0 or not 0 <= Q["discount_bp_at_max"] < 10000:
        out.append("economy: reputation runs from below zero to above it")
    S = e["stall"]
    if L.get(S["lot"], {}).get("kind") != "market":
        out.append("economy: the stall lot %s is not the market" % S["lot"])
    if not (whole(S["rent"]) and whole(S["slots"]) and S["slots"] > 0 and whole(S["buy_bp"]) and whole(S["elasticity"]) and whole(S["max_price_factor_bp"])):
        out.append("economy: stall numbers must be whole")
    if not 0 <= S["open"][0] < S["open"][1] <= 1440:
        out.append("economy: the stall opens and closes within the day")
    return out


def mall_visit_problems(w):
    """Every rule the visits to a mall must keep (CITY-106, World/rules/choices.json 'mall'): opening hours in order
    and within the day, something to do instead for every list that sends people to the mall, and the mall a modest
    share of each such list."""
    C, out = w["choices"], []
    M = C.get("mall")
    lists = {t: [o for opts in c.values() for o in opts] for t, c in C["choices"].items()}
    sends = [t for t, L in lists.items() if any(o[1] == "mall" for o in L)]
    if not M:
        return ["choices.json: lists send people to the mall but 'mall' gives no hours"] if sends else []
    try:
        a, b = [int(M[k][:2]) * 60 + int(M[k][3:]) for k in ("open", "last_in")]
        if not 5 * 60 <= a < b <= 22 * 60:
            out.append("choices.json mall: open must be before last_in, between 05:00 and 22:00")
    except Exception:
        out.append("choices.json mall: open and last_in must be HH:MM")
    for t in sends:
        e = (M.get("else") or {}).get(t)
        if not (isinstance(e, list) and len(e) == 2 and e[1] != "mall"):
            out.append("choices.json mall: nothing to do instead for %s" % t)
        for kind, opts in C["choices"][t].items():
            n = sum(1 for o in opts if o[1] == "mall")
            if n and n * 4 > len(opts):
                out.append("choices.json: more than a quarter of %s (%s) goes to the mall" % (t, kind))
            for o in opts:
                if o[1] == "mall" and o[0] not in ("shop", "eat"):
                    out.append("choices.json: at the mall people shop or eat, not %s" % o[0])
    return out


def mall_goods_problems(w):
    """Every rule World/rules/mall_goods.json must keep (ECO-013, ECO-014): every kind of outlet has enough to sell, every
    good has a name, a whole price in chips and somewhere to go, a good for the home is a piece of furnishing.json with
    the same name and price, clothes set only parts of the look the wardrobe may change, to values the designer offers,
    and no good takes the id of one of the town's items."""
    g, e, out = w["mall_goods"], w["economy"], []
    goods, items, pieces = g["goods"], e["items"], w["furnishing"]["pieces"]
    parts = w["settings"]["wardrobe"]["parts"]
    if type(g.get("per_outlet")) is not int or g["per_outlet"] < 4:
        out.append("mall goods: per_outlet must be a whole number of at least 4")
    if type((g.get("stock") or {}).get("target")) is not int or g["stock"]["target"] < 1:
        out.append("mall goods: stock.target must be a whole number of at least 1")
    if (g.get("display") or {}).get("detail") not in ("near", "far"):
        out.append("mall goods: display.detail must be near or far")
    for kind in w["mall_outlets"]["kinds"]:
        L = g["sells"].get(kind, [])
        if len(L) < 6:
            out.append("mall goods: %s outlets have only %d things to sell; want at least 6" % (kind, len(L)))
    used = set()
    for kind, L in g["sells"].items():
        if kind not in w["mall_outlets"]["kinds"]:
            out.append("mall goods: sells for %s, which is no kind of outlet" % kind)
        if len(set(L)) != len(L):
            out.append("mall goods: %s lists a good twice" % kind)
        for x in L:
            used.add(x)
            if x not in goods and x not in items:
                out.append("mall goods: %s sells %s, which is neither a good nor an item" % (kind, x))
            elif x in items and (items[x].get("quest") or items[x]["price"] <= 0):
                out.append("mall goods: %s sells %s, which is not for sale" % (kind, x))
    names = {}
    for k, G in goods.items():
        if k in items:
            out.append("mall goods: %s is already one of the town's items" % k)
        if k not in used:
            out.append("mall goods: nothing sells %s" % k)
        if not isinstance(G.get("name"), str) or not G["name"].strip():
            out.append("mall goods: %s has no name" % k)
        elif G["name"] in names:
            out.append("mall goods: %s and %s have the same name" % (names[G["name"]], k))
        else:
            names[G["name"]] = k
        if type(G.get("price")) is not int or G["price"] <= 0:
            out.append("mall goods: %s: the price must be whole chips above zero" % k)
        goes = G.get("goes")
        if goes not in g["goes"]:
            out.append("mall goods: %s goes nowhere (%s)" % (k, goes))
        if goes == "home":
            P = pieces.get(G.get("piece"))
            if not P:
                out.append("mall goods: %s is for the home but %s is no piece of furnishing.json" % (k, G.get("piece")))
            elif P["name"] != G["name"] or P["price"] != G["price"]:
                out.append("mall goods: %s and its piece %s disagree on name or price" % (k, G["piece"]))
        elif "piece" in G:
            out.append("mall goods: %s has a piece but does not go home" % k)
        if goes == "wear":
            P = G.get("parts") or {}
            if not P:
                out.append("mall goods: %s is to wear but sets no part of the look" % k)
            for q in P:
                if q not in parts:
                    out.append("mall goods: %s sets %s, which the wardrobe cannot change" % (k, q))
            full = dict(w["designer"]["default"], **P)
            out += ["mall goods: %s: %s" % (k, p) for p in look_problems(w["designer"], full) if p.split(" ")[0] in P]
        elif "parts" in G:
            out.append("mall goods: %s has parts but is not to wear" % k)
    for k, P in pieces.items():
        if P.get("from") == "mall" and not any(G.get("piece") == k for G in goods.values()):
            out.append("mall goods: the piece %s is from the mall but nothing sells it" % k)
    return out


def estate_problems(w):
    """Every rule World/rules/estate.json must keep (PROP-009 to PROP-021, F-PROP-ESTATE level 2)."""
    R, out = w["estate"], []
    L = {l["id"]: l for l in lots(w)}

    def whole(v, lo=0):
        return type(v) is int and v >= lo

    def bp_pair(p, where):
        if not (isinstance(p, list) and len(p) == 2 and whole(p[0], 1) and whole(p[1]) and p[0] <= p[1]):
            out.append("estate: %s must be [low, high] in basis points" % where)
    kinds = R.get("kinds", {})
    for k, K in kinds.items():
        if not (whole(K.get("per_m2"), 1) and whole(K.get("yield_bp"), 1) and isinstance(K.get("name"), str)):
            out.append("estate: kind %s needs a name, per_m2 and yield_bp in whole numbers" % k)
        ow = K.get("owners", {})
        if not ow or any(o not in ("household", "company", "firm", "city") or not whole(v) for o, v in ow.items()) or not sum(ow.values()):
            out.append("estate: kind %s has bad owner weights" % k)
    for name, rule in R.get("units", {}).items():
        made = [rule.get("whole"), rule.get("per_floor"), rule.get("main")]
        for m in made:
            if m is not None and m not in kinds:
                out.append("estate: units.%s makes unknown kind %s" % (name, m))
        if rule.get("flat_use") and "flat" not in kinds:
            out.append("estate: units.%s makes flats but there is no flat kind" % name)
    if "default" not in R.get("units", {}):
        out.append("estate: units needs a default")
    if R.get("location", {}).get("centre_lot") not in L:
        out.append("estate: location.centre_lot is not a lot")
    ids = [c.get("id") for c in R.get("companies", [])]
    if len(ids) < 2 or len(set(ids)) != len(ids):
        out.append("estate: there must be at least two real estate companies with different ids")
    for c in R.get("companies", []):
        if L.get(c.get("office"), {}).get("kind") in (None, "garden", "stop", "market"):
            out.append("estate: %s's office is not a building" % c.get("id"))
        for f in c.get("focus", []):
            if f not in kinds:
                out.append("estate: %s focuses on unknown kind %s" % (c.get("id"), f))
        if not all(whole(c.get(k)) for k in ("cash", "reserve", "cap", "dividend_bp", "weight")) or c["reserve"] > c["cap"]:
            out.append("estate: %s needs whole cash, reserve <= cap, dividend_bp and weight" % c.get("id"))
    for k in kinds:
        if k not in R["trade"]["city_sells"] and not any(k in c.get("focus", []) for c in R.get("companies", [])):
            out.append("estate: no company buys %s and the town does not sell it" % k)
    T = R.get("trade", {})
    for k in ("transfer_tax_bp", "agent_fee_bp", "bid_bp", "forced_bid_bp", "whole_building_discount_bp", "listing_days", "price_cut_bp",
              "sale_bp", "sale_slope_bp", "sale_max_bp", "household_buy_bp", "household_min_pool_days", "list_high_index_bp", "list_low_cash_bp"):
        if not whole(T.get(k)):
            out.append("estate: trade.%s must be a whole number" % k)
    if whole(T.get("bid_bp")) and whole(T.get("forced_bid_bp")) and not T["forced_bid_bp"] <= T["bid_bp"] < 10000:
        out.append("estate: the forced bid must be at most the bid, and the bid below the value")
    if any(not whole(v) or v <= T.get("bid_bp", 0) for v in T.get("offer_bp", {}).values()):
        out.append("estate: an offer must be above the bid, or buying and selling back makes money")
    bp_pair(T.get("ask_bp"), "trade.ask_bp")
    bp_pair(T.get("player_ask_range_bp"), "trade.player_ask_range_bp")
    bp_pair(R.get("letting", {}).get("rent_range_bp"), "letting.rent_range_bp")
    for k in ("arrive_bp", "arrive_slope_bp", "leave_bp", "leave_slope_bp", "lodger_occupied_permille", "max_tenancies", "missed_before_eviction"):
        if not whole(R.get("letting", {}).get(k)):
            out.append("estate: letting.%s must be a whole number" % k)
    C = R.get("costs", {})
    for k in ("upkeep_bp", "tax_bp", "portfolio_free", "portfolio_bp_per_unit", "portfolio_cap_bp", "missed_before_seizure", "seizure_fee"):
        if not whole(C.get(k)):
            out.append("estate: costs.%s must be a whole number" % k)
    for k, K in kinds.items():
        if whole(K.get("yield_bp")) and whole(C.get("upkeep_bp")) and K["yield_bp"] <= C["upkeep_bp"] + C["tax_bp"]:
            out.append("estate: %s rent does not cover its upkeep and tax" % k)
    I = R.get("index", {})
    if not (whole(I.get("min_bp"), 1) and whole(I.get("max_bp")) and I["min_bp"] <= 10000 <= I["max_bp"]):
        out.append("estate: index bounds must hold 10000")
    Mo = R.get("mortgage", {})
    if not (whole(Mo.get("deposit_bp"), 1) and whole(Mo.get("rate_bp")) and Mo.get("terms") and all(whole(t, 1) for t in Mo["terms"]) and whole(Mo.get("max"), 1)):
        out.append("estate: mortgage needs deposit_bp, rate_bp, terms and max")
    for role in {r["role"] for r in w["residents"]}:
        if not whole(R.get("town", {}).get("wages", {}).get(role)):
            out.append("estate: town.wages has no whole amount for %s" % role)
    return out


def quest_problems(w):
    """Every rule World/rules/quests.json must keep (QST-001 to QST-003, QST-006)."""
    import quests
    return quests.problems(w["quests"], w["economy"], {r["id"] for r in w["residents"]}, {l["id"] for l in lots(w)},
                           set(w["roles"]["roles"]), set(w["dialogue"]["moods"]), w["dialogue"]["vars"])


DESIGNER_HAIR = ("short", "side", "long", "bob", "bun", "afro", "bald")          # what Prototype/play/people.js can draw
DESIGNER_GARMENT = ("tee", "long", "tunic", "coat")
DESIGNER_BOTTOM = ("trousers", "shorts", "skirt")
DESIGNER_WALK = ("Springy", "Easy", "Hurried", "Careful")                       # CHARACTER_MOTION §3 signatures


def designer_problems(w):
    """Every rule World/rules/designer.json must keep (CHR-002, CHR-013)."""
    d, out = w["designer"], []

    def colour(c):
        return isinstance(c, list) and len(c) == 3 and all(isinstance(v, (int, float)) and 0 <= v <= 1 for v in c)

    for k, s in d["sliders"].items():
        if not (s["min"] <= s["default"] <= s["max"] and s["min"] < s["max"] and s["step"] > 0):
            out.append("designer: slider %s needs min <= default <= max and a step" % k)
    h = d["sliders"].get("height")
    if not h or h["min"] < 1.58 or h["max"] > 1.80:
        out.append("designer: height must stay inside the cast range 1.58 to 1.80 m (ART_DIRECTION 3.2)")
    for k in ("height", "build", "face_w", "eye_size", "eye_open", "brow", "nose", "lips"):   # ART_DIRECTION 3.4 designer values
        if k not in d["sliders"]:
            out.append("designer: no slider for %s" % k)
    for k in ("skin", "hair_color", "eyes"):
        if not d[k] or not all(colour(c) for c in d[k]):
            out.append("designer: %s must be colours" % k)
    if len(d["skin"]) < 8:
        out.append("designer: offer a wide range of skin tones (rule C3)")
    for c in d["palette"].values():
        if not colour(c):
            out.append("designer: palette colours must be colours")
    for k, allowed in (("hair", DESIGNER_HAIR), ("garment", DESIGNER_GARMENT), ("bottom_type", DESIGNER_BOTTOM), ("walk", DESIGNER_WALK)):
        for v in d[k]:
            if v not in allowed:
                out.append("designer: %s %s cannot be drawn" % (k, v))
    for v in d["extras"]:
        if v not in ("scarf", "cap", "bag", "apron", "tash"):
            out.append("designer: extra %s cannot be drawn" % v)
    out += ["designer default: " + p for p in look_problems(d, d["default"])]
    return out


def look_problems(d, v):
    """What is wrong with one character's designer values (the slice checks the same before it loads a saved character)."""
    out = []
    for k, s in d["sliders"].items():
        x = v.get(k)
        if isinstance(x, bool) or not isinstance(x, (int, float)) or not s["min"] - 1e-9 <= x <= s["max"] + 1e-9:
            out.append("%s out of range" % k)
    for k in ("skin", "hair_color", "eyes"):
        if not (type(v.get(k)) is int and 0 <= v[k] < len(d[k])):
            out.append("%s is not a swatch" % k)
    for k in ("hair", "garment", "bottom_type", "walk"):
        if v.get(k) not in d[k]:
            out.append("%s %s is not offered" % (k, v.get(k)))
    for k in ("top", "bottom", "shoes"):
        if v.get(k) not in d["palette"]:
            out.append("%s colour %s is not in the palette" % (k, v.get(k)))
    for k in ("scarf", "cap", "apron"):
        if v.get(k) is not None and v[k] not in d["palette"]:
            out.append("%s colour %s is not in the palette" % (k, v.get(k)))
    for k in ("bag", "tash"):
        if not isinstance(v.get(k), bool):
            out.append("%s must be yes or no" % k)
    if not isinstance(v.get("name"), str) or not 0 < len(v["name"].strip()) <= d["name"]["max"]:
        out.append("the name must be 1 to %d letters" % d["name"]["max"])
    return out


def speech_problems(w):
    """Every rule World/rules/speech.json must keep (CHR-007, CHR-008, CHR-009)."""
    sp, out = w["speech"], []
    shapes, ex, tx = sp["shapes"], sp["expressions"], sp["text"]
    for need in ("rest", "MBP", "AA", "EH", "EE", "OH", "OO", "FV"):    # the eight speech shapes of CHARACTER_FACE §2
        if need not in shapes:
            out.append("speech: no mouth shape %s" % need)
    for mood in w["dialogue"]["moods"]:                                 # every mood a line can carry has a face
        if mood not in ex:
            out.append("speech: no expression for mood %s" % mood)
    for name, e in ex.items():
        if isinstance(e["mouth"], str) and e["mouth"] not in shapes:
            out.append("speech: expression %s uses unknown mouth %s" % (name, e["mouth"]))
    for letter in "abcdefghijklmnopqrstuvwxyz":
        if letter not in tx["letters"]:
            out.append("speech: no mouth shape for the letter %s" % letter)
    used = list(tx["letters"].values()) + list(tx["digraphs"].values()) + list(sp["timing"]["end"].values()) + sp["timing"]["stressed_shapes"]
    for s in used:
        if s is not None and s not in shapes:
            out.append("speech: unknown mouth shape %s" % s)
    return out


def walkways(w):
    """Where people walk (Q-24, CITY-043): a ring round every block inside the town wall along its pavement, `walk_offset`
    from the street centre lines, and a zebra crossing over each street `crossing_at` from every junction, joining the
    rings on either side (never across the wall). Block footpaths that ran to a street's centre line stop at the ring
    instead. Outside the wall people walk along the gate roads, the links out to the new areas (roads.json "links") and
    the communities' lanes (block "lanes"); doors there join the nearest of them. Returns (segments, crossings) with
    crossings {(x, z) of the middle, rounded to cm: (street id, junction id)}."""
    g, r = w["city"]["grid"], w["traffic"]
    P, cols, rows, off, c = g["pitch"], g["columns"], g["rows"], r["walk_offset"], r["crossing_at"]
    inside = [b for b in w["blocks"] if all(in_wall(w, q) for q in cells_of(b))]
    have = {tuple(b["grid"]) for b in inside}
    segs, crossings = [], {}
    for (i, j) in sorted(have):
        x0, x1, z0, z1 = i * P + off, (i + 1) * P - off, j * P + off, (j + 1) * P - off
        segs += [[[x0, z0], [x1, z0]], [[x1, z0], [x1, z1]], [[x1, z1], [x0, z1]], [[x0, z1], [x0, z0]]]

    def cross(a, b, street, junction):
        segs.append([a, b])
        crossings[(round((a[0] + b[0]) / 2, 2), round((a[1] + b[1]) / 2, 2))] = (street, junction)
    for I in range(cols + 1):
        for J in range(rows + 1):
            X, Z, jid = I * P, J * P, "j_%d_%d" % (I, J)
            if I < cols and (I, J - 1) in have and (I, J) in have:          # east arm, over the street along z = Z
                cross([X + c, Z - off], [X + c, Z + off], "s_ew_%d_%d" % (I, J), jid)
            if I > 0 and (I - 1, J - 1) in have and (I - 1, J) in have:     # west arm
                cross([X - c, Z - off], [X - c, Z + off], "s_ew_%d_%d" % (I - 1, J), jid)
            if J > 0 and (I - 1, J - 1) in have and (I, J - 1) in have:     # north arm, over the street along x = X
                cross([X - off, Z - c], [X + off, Z - c], "s_ns_%d_%d" % (I, J - 1), jid)
            if J < rows and (I - 1, J) in have and (I, J) in have:          # south arm
                cross([X - off, Z + c], [X + off, Z + c], "s_ns_%d_%d" % (I, J), jid)
    for b in inside:                                                        # footpaths meet the ring, not the road
        cx, cz = b["origin"][0] + b["size"] / 2, b["origin"][1] + b["size"] / 2
        for a, bb in b["paths"]:
            pts = []
            for x, z in (a, bb):
                if abs(x / P - round(x / P)) < 1e-9:
                    x = x + (off if cx > x else -off)
                if abs(z / P - round(z / P)) < 1e-9:
                    z = z + (off if cz > z else -off)
                pts.append([x, z])
            if pts[0] != pts[1]:
                segs.append(pts)
    roads = w.get("roads") or {}
    J = {j["id"]: j["pos"] for j in roads.get("junctions", [])}
    for gt in roads.get("gates", []):                                       # out through a gate: from the corners of its junction
        X, Z = J[gt["junction"]]
        end = [X + gt["dir"][0] * gt["length"], Z + gt["dir"][1] * gt["length"]]
        segs.append([[X, Z], end])
        I, Jn = int(round(X / P)), int(round(Z / P))
        for di, dj, sx, sz in ((-1, -1, -1, -1), (0, -1, 1, -1), (-1, 0, -1, 1), (0, 0, 1, 1)):
            if (I + di, Jn + dj) in have:
                segs.append([[X, Z], [X + sx * off, Z + sz * off]])
    lines = [l["points"] for l in roads.get("links", [])] + [ln["points"] for b in w["blocks"] for ln in b.get("lanes", [])]
    for pts in lines:
        segs += [[list(a), list(b)] for a, b in zip(pts, pts[1:]) if a != b]
    return segs, crossings


def sim_data(w):
    """The city in the shape the living-city rules read (Tools/citysim/citysim.py)."""
    g = w["city"]["grid"]
    P, n = g["pitch"], g["columns"]
    streets, _ = walkways(w)
    cal = w["calendar"]
    veg = w["vegetation"]
    return dict(name=w["city"]["name"], size=[n * P, g["rows"] * P], calendar=cal["calendar"], weather=cal["weather"], tram=cal["tram"],
                streets=streets, lots=lots(w), residents=w["residents"], roles=w["roles"]["roles"], signatures=w["signatures"]["signatures"],
                choices=dict(w["choices"]["choices"], rounds=[]), use=w["choices"]["use"], near_work=w["choices"].get("near_work"), mall=w["choices"].get("mall"), species=veg["species"], flowers=veg["flowers"], plants=veg.get("plants", {}),
                replant_gap_days=veg["replant_gap_days"], fallen_days=veg["fallen_days"], trees=[t for b in w["blocks"] for t in b["trees"]], lamps=[], react=w.get("weather_react"))


def road_data(w):
    """roads.json with what the traffic rules work out from the rest of the map (CITY-090, CITY-091): the communities'
    lanes, the places outside the wall that need a driveway or kerbside bays, and the bus stops (transit.json)."""
    roads = dict(w["roads"])
    inside = {l["id"] for l in lots_inside(w)}
    roads["outer_lanes"] = [ln for b in w["blocks"] for ln in b.get("lanes", [])]
    roads["outer_lots"] = [dict(id=l["id"], kind=l["kind"], door=l["door"], bays_kind="mall" if b.get("template") == "mall" else l["kind"])
                           for b in w["blocks"] for l in b["lots"]
                           if l["id"] not in inside and l["kind"] not in ("market", "stop")]
    roads["stops"] = list((w.get("transit") or {}).get("stops", []))
    size = w["traffic"]["buses"].get("obstacle_m", {})          # what a bus stop must keep clear of: street things and trees
    roads["obstacles"] = [[p["pos"][0], p["pos"][1], size.get(p["kind"], size.get("default", .5))] for b in w["blocks"] for p in b["props"]
                          if "pos" in p and p["kind"] not in size.get("ignore", [])] +                          [[t["pos"][0], t["pos"][1], size.get("tree", 1.0)] for b in w["blocks"] for t in b["trees"]]
    return roads


def home_kinds(w):
    """CITY-089: what kind of home each home lot is, from its block's template (traffic.json "home_kind"): house, row or flat."""
    by = w["traffic"].get("home_kind", {})
    return {l["id"]: by.get(b.get("template"), by.get("default", "flat")) for b in w["blocks"] for l in b["lots"] if l["kind"] == "home"}


def simulation(w, traffic=False):
    """The living-city rules over the whole map. With traffic=True, residents who own a car drive their longer
    trips (LIVING_CITY CITY-043 to CITY-049), and buses run their lines (CITY-091, CITY-092)."""
    import citysim
    city = citysim.City(data=sim_data(w))
    if traffic:
        import traffic as tr
        city.home_kind = home_kinds(w)
        tr.Traffic(city, road_data(w), w["traffic"], w.get("transit"))
    return city


# ---------------------------------------------------------------- street dressing (WLD-028 to WLD-034, F-MAP-DRESS)
DRESS_KINDS = {"playground", "swing", "slide", "seesaw", "sandpit", "climber", "bench", "bandstand", "noticeboard", "sundial", "well",
               "pergola", "statue", "kiosk", "bunting", "planter", "bin", "bikerack", "postbox", "phonebox", "bollard", "timetable",
               "advert", "hoarding"}
PLAY_USES = ("swing", "slide", "seesaw", "sandpit", "climb")


def dressing_problems(w):
    """Every rule the street dressing must keep (WLD-028, WLD-034). Returns a list of problems in plain words."""
    out = []
    g = w["city"]["grid"]
    lots_by_id = {l["id"]: l for l in lots(w)}
    for b in w["blocks"]:
        ox, oz = b["origin"]
        lo_x, lo_z, hi_x, hi_z = ox - g["street_width"] / 2, oz - g["street_width"] / 2, ox + b["size"] + g["street_width"] / 2, oz + b["size"] + g["street_width"] / 2
        for p in b["props"]:
            if p.get("set") != "dress":
                continue
            if p["kind"] not in DRESS_KINDS:
                out.append("%s: unknown dressing %s" % (p.get("id"), p["kind"]))
            x, z = p["pos"]
            if not (lo_x <= x <= hi_x and lo_z <= z <= hi_z):
                out.append("%s: %s at %s is outside its block and pavements" % (p.get("id"), p["kind"], p["pos"]))
            if p["kind"] in ("advert", "hoarding") and p.get("lot") not in lots_by_id:
                out.append("%s: %s on a lot that does not exist" % (p.get("id"), p["kind"]))
            for l in b["lots"]:                       # nothing on the street stands in a doorway
                if p["kind"] not in ("advert", "hoarding") and math.hypot(x - l["door"][0], z - l["door"][1]) < 1.6:
                    out.append("%s: %s blocks the door of %s" % (p.get("id"), p["kind"], l["id"]))
    d = w["dressing"]
    for k in ("adverts", "films", "advert_every_days", "film_every_days", "motion"):
        if not d.get(k):
            out.append("dressing.json: %s is missing" % k)
    for k in PLAY_USES + ("notices", "paper"):
        if k not in w["choices"]["use"]:
            out.append("choices.json: nothing to use for %s" % k)
    return out


# ---------------------------------------------------------------- garden planting (CITY-017, WLD-040, F-GARDEN)
PLANT_SHAPES = ("hedge", "border", "bush", "drift")


def garden_problems(w):
    """Every rule the planting in parks and gardens must keep (WLD-040). Returns a list of problems in plain words."""
    out = []
    plants = {k for k in w["vegetation"].get("plants", {}) if k != "note"}
    for k in plants:
        p = w["vegetation"]["plants"][k]
        for need in ("summer",) + (("autumn",) if p.get("leaf") else ()) + (("bloom_color",) if "bloom" in p else ()) + (("spent_color",) if "spent_until" in p else ()):
            if need not in p:
                out.append("vegetation.json: plant %s has no %s" % (k, need))
    for b in w["blocks"]:
        gardens = [l for l in b["lots"] if l["kind"] == "garden"]
        for p in b["props"]:
            if p.get("set") != "garden":
                continue
            if p["kind"] != "plant" or p.get("shape") not in PLANT_SHAPES:
                out.append("%s: unknown planting %s %s" % (p.get("id"), p["kind"], p.get("shape")))
            for sp in (p.get("species") if isinstance(p.get("species"), list) else [p.get("species")]):
                if sp not in plants:
                    out.append("%s: unknown plant %s" % (p.get("id"), sp))
            x, z = p["pos"]
            in_park = b["template"] == "park" and b["origin"][0] + 1 < x < b["origin"][0] + b["size"] - 1 and b["origin"][1] + 1 < z < b["origin"][1] + b["size"] - 1
            if not in_park and not any(l["rect"][0] <= x <= l["rect"][2] and l["rect"][1] <= z <= l["rect"][3] for l in gardens):
                out.append("%s: %s at %s is not in a garden" % (p.get("id"), p.get("shape"), p["pos"]))
            for l in gardens:                             # nothing planted on a seat or a place someone stands
                for k in ("bench", "rim", "flowerbed"):
                    for q in l.get("objects", {}).get(k, []):
                        if p.get("shape") in ("hedge", "border"):
                            ax = p.get("along") == "x"
                            du, dv = abs((q[0] - x) if ax else (q[1] - z)), abs((q[1] - z) if ax else (q[0] - x))
                            inside = du < p["len"] / 2 and dv < p["depth"] / 2
                        else:
                            inside = p.get("shape") == "bush" and math.hypot(q[0] - x, q[1] - z) < p.get("r", 1)
                        if inside:
                            out.append("%s: %s covers a %s place in %s" % (p.get("id"), p.get("shape"), k, l["id"]))
    if "tend" not in w["choices"]["use"]:
        out.append("choices.json: nothing to use for tend")
    return out


def plants_today(w, city, day):
    """What every garden plant looks like on a day (CITY-017), for the slice: worked out by citysim from the day alone."""
    r3 = lambda c: [round(v, 3) for v in c]
    out = {}
    for k, p in w["vegetation"]["plants"].items():
        if k == "note":
            continue
        s = city.veg.plant(k, day)
        out[k] = dict(size=round(s["size"], 2), leaf=round(s["leaf"], 2), bloom=round(s["bloom"], 2), spent=round(s["spent"], 2), color=r3(s["color"]),
                      bloom_color=p.get("bloom_color", [0, 0, 0]), spent_color=p.get("spent_color", [0, 0, 0]), height=p.get("height", .5), evergreen=not p.get("leaf"))
    return out


def dressing_today(w, day):
    """What the changing dressing shows on a day (WLD-034): worked out from the day number and World/rules/dressing.json,
    never stored. Each advert board shows one advert for advert_every_days days, boards out of step with each other; the
    cinema's board shows the film of the day; bunting is up on market days."""
    import citysim
    d = w["dressing"]
    clock = citysim.Clock(w["calendar"]["calendar"])
    boards = [p["id"] for b in w["blocks"] for p in b["props"] if p.get("set") == "dress" and p["kind"] in ("advert", "hoarding")]
    n = len(d["adverts"])
    adverts = {pid: (day // d["advert_every_days"] + k * 7) % n for k, pid in enumerate(boards)}
    film = d["films"][(day // d["film_every_days"]) % len(d["films"])]
    return dict(adverts=adverts, film=film, bunting=bool(clock.info(day)["market"]) if d["bunting"] == "market_days" else True,
                advert_list=d["adverts"], motion=d["motion"])


# ---------------------------------------------------------------- stone, brick and paving (WLD-038, WLD-039, ADR-0017)
SURFACE_TYPES = {"bond", "flemish", "rubble", "flags", "herringbone", "basket"}
SURFACE_SLOTS = {"wall": "wall", "pavement": "pavement", "ground": "pavement"}     # slot -> what its patterns are made 'for'


def _surface_hash(*k):
    import hashlib
    return int(hashlib.md5("|".join(map(str, k)).encode()).hexdigest()[:8], 16) / 0xFFFFFFFF


def _surface_match(cond, b, l):
    """Does a rule's 'if' fit this block (and lot)? Every field named must match; an empty 'if' always does."""
    for k, want in cond.items():
        if k == "template":
            got = [b["template"]]
        elif k == "district":
            got = [b["district"]]
        elif l is None:
            return False
        elif k == "tags":
            got = l.get("tags", [])
        else:
            got = [l.get(k)]
        if not set(got) & set(want):
            return False
    return True


def surface_pick(w, slot, b, l=None):
    """The pattern one slot of a block (l None) or lot uses, and why: 'own' (its 'surface' field) or 'rule'."""
    S = w["surfaces"]
    own = (l if l is not None else b).get("surface", {}).get(slot)
    if own:
        return own, "own"
    for rule in S["rules"][slot]:
        if _surface_match(rule["if"], b, l):
            use = rule["use"]
            if isinstance(use, str):
                return use, "rule"
            names = sorted(use)
            r, total = _surface_hash((l or b)["id"], slot) * sum(use.values()), 0
            for n in names:
                total += use[n]
                if r < total:
                    return n, "rule"
            return names[-1], "rule"
    return None, "none"


def surfaces(w):
    """Which pattern every surface uses: {patterns: [names], lots: {lot: {wall, color, upper} or {ground, color}},
    blocks: {block: {pavement, color}}}. Patterns are given by their place in 'patterns'; color None means the town's own."""
    S = w["surfaces"]
    names = list(S["patterns"])
    out = dict(patterns=names, lots={}, blocks={})

    def colour(name, key):
        cs = S["patterns"][name]["colors"]
        c = cs[int(_surface_hash(key, "colour") * len(cs)) % len(cs)]
        return None if isinstance(c, str) else c
    for b in w["blocks"]:
        name, _ = surface_pick(w, "pavement", b)
        out["blocks"][b["id"]] = dict(pavement=names.index(name), color=colour(name, b["id"]))
        for l in b["lots"]:
            if l["kind"] == "garden":
                continue
            if l["kind"] in OPEN_KINDS:
                name, _ = surface_pick(w, "ground", b, l)
                out["lots"][l["id"]] = dict(ground=names.index(name), color=colour(name, l["id"]))
            else:
                name, _ = surface_pick(w, "wall", b, l)
                up = S["patterns"][name].get("upper", 0)
                out["lots"][l["id"]] = dict(wall=names.index(name), color=colour(name, l["id"]), upper=bool(up and _surface_hash(l["id"], "upper") < up))
    return out


def surface_problems(w):
    """Every rule World/rules/surfaces.json and the 'surface' fields in the block files must keep (WLD-038)."""
    S, out = w["surfaces"], []
    for n, p in S["patterns"].items():
        if p.get("type") not in SURFACE_TYPES:
            out.append("surfaces.json: %s has unknown type %s" % (n, p.get("type")))
        if p.get("for") not in ("wall", "pavement"):
            out.append("surfaces.json: %s is for neither wall nor pavement" % n)
        if not (isinstance(p.get("unit"), list) and len(p["unit"]) == 2 and all(0.03 <= v <= 2.0 for v in p["unit"])):
            out.append("surfaces.json: %s has a unit size out of range" % n)
        if not p.get("colors"):
            out.append("surfaces.json: %s has no colours" % n)
    if len(S["patterns"]) > 16:
        out.append("surfaces.json: more than 16 patterns (the slice's shader holds 16)")

    def check(name, slot, where):
        if name not in S["patterns"]:
            out.append("%s: %s pattern %s does not exist" % (where, slot, name))
        elif S["patterns"][name]["for"] != SURFACE_SLOTS[slot]:
            out.append("%s: %s pattern %s is made for %s" % (where, slot, name, S["patterns"][name]["for"]))
    for slot in SURFACE_SLOTS:
        rules = S["rules"].get(slot, [])
        if not rules or rules[-1]["if"] != {}:
            out.append("surfaces.json: the %s rules must end with one that fits everything" % slot)
        for k, rule in enumerate(rules):
            for f in rule["if"]:
                if f not in ("template", "district", "kind", "tags", "name"):
                    out.append("surfaces.json: %s rule %d asks about %s" % (slot, k, f))
            for name in ([rule["use"]] if isinstance(rule["use"], str) else rule["use"]):
                check(name, slot, "surfaces.json %s rule %d" % (slot, k))
    for b in w["blocks"]:
        for slot, name in b.get("surface", {}).items():
            check(name, slot, b["id"])
        for l in b["lots"]:
            for slot, name in l.get("surface", {}).items():
                check(name, slot, l["id"])
    return out


# ---------------------------------------------------------------- every floor of every building (WLD-050 to WLD-053, ADR-0021)
def storey_floor(f):
    """Height of floor f's surface: the ground floor at 0.2, floor f above at 3f + 0.4 (World/rules/storeys.json)."""
    return 0.2 if f == 0 else 3.0 * f + 0.4


def house_stair(l):
    """A designed house (BUILD-001, ADR-0035) has its stair in its plan: it rises from the hall, up to the rooms above."""
    rooms = (l.get("house") or {}).get("rooms", [])
    return any(r["name"] == "Hall" and r["floor"] == 0 for r in rooms) and {r["floor"] for r in rooms} >= set(range(l["floors"]))


def storey_problems(w):
    """Every rule the floors, stairs, lifts and upstairs places in the block files must keep (WLD-050, WLD-051)."""
    S, out = w["storeys"], []
    st = S["stair"]
    need_w, need_l = 2 * st["width"] + st["spine"], st["run"] + 2 * st["landing"]
    for l in lots(w):
        if l["kind"] in OPEN_KINDS:
            continue
        n, where = l["floors"], l["id"]
        if not l["enterable"]:
            out.append("%s: cannot be entered" % where)
        if len(l.get("storeys", [])) != n:
            out.append("%s: %d storeys named for %d floors" % (where, len(l.get("storeys", [])), n))
        x0, z0, x1, z1 = l["rect"]
        if n >= 2:
            s, r = l.get("stair"), None
            if not s and house_stair(l):                     # a designed house: its stair rises in the hall of its plan
                pass                                         # (houses.js), not in a stairwell from add_storeys_01.py
            elif not s:
                out.append("%s: %d floors and no stair" % (where, n))
                continue
            else:
                r = s["rect"]
                if not (x0 + S["wall"] - .01 <= r[0] < r[2] <= x1 - S["wall"] + .01 and z0 + S["wall"] - .01 <= r[1] < r[3] <= z1 - S["wall"] + .01):
                    out.append("%s: stair is not inside the walls" % where)
                long_, short = (r[2] - r[0], r[3] - r[1]) if s["axis"] == "x" else (r[3] - r[1], r[2] - r[0])
                if long_ < need_l - .02 or short < need_w - .02:              # (sizes are kept to the centimetre)
                    out.append("%s: stairwell %.1f by %.1f is too small" % (where, long_, short))
                if s["wall"] == side_of_door(l):
                    out.append("%s: stair against the door's wall" % where)
                if s["wall"] == {"n": "s", "s": "n", "w": "e", "e": "w"}[side_of_door(l)]:
                    out.append("%s: stair against the back wall, where counters stand" % where)
            lv = l.get("levels", {})
            for k, v in l["objects"].items():
                levels = lv.get(k, [0] * len(v))
                if len(levels) != len(v):
                    out.append("%s: %s has %d places and %d floors for them" % (where, k, len(v), len(levels)))
                    continue
                for p, f in zip(v, levels):
                    if not 0 <= f < n:
                        out.append("%s: a %s on floor %d of %d" % (where, k, f, n))
                    if r is None:
                        continue
                    dx, dz = max(r[0] - p[0], 0, p[0] - r[2]), max(r[1] - p[1], 0, p[1] - r[3])
                    if f > 0 and math.hypot(dx, dz) < 1.0:
                        out.append("%s: a %s upstairs stands in the stairwell" % (where, k))
            # (places on different floors may share a spot on the plan, as stacked flats and offices do: a stay upstairs
            # carries its floor, build_play.routines, and the slice finds the place by where it is and that floor, WLD-053)
            if n >= S["lift_from_floors"] and not l.get("lift"):
                out.append("%s: %d floors and no lift" % (where, n))
        elif l.get("levels"):
            out.append("%s: one floor but places upstairs" % where)
    return out


def career_problems(w):
    """Every rule careers.json must keep, and every resident's career (CITY-081 to CITY-086)."""
    import careers
    R = w["careers"]
    out = careers.problems(R, w["roles"]["roles"])
    C = R["careers"]
    staff = {}
    for r in w["residents"]:
        c = r.get("career")
        if c is None:
            if r["role"] not in ("child", "student", "elder"):
                out.append("%s: a %s with no career" % (r["id"], r["role"]))
            continue
        if c not in C:
            out.append("%s: unknown career %s" % (r["id"], c))
        elif C[c]["role"] != r["role"] and r["role"] in R["reassign_roles"]:    # commuters and couriers keep their routine
            out.append("%s: career %s follows the %s routine, not %s" % (r["id"], c, C[c]["role"], r["role"]))
        if r.get("day_career") is not None and r["day_career"] not in R.get("second_jobs", {}).get(c, {}):
            out.append("%s: day career %s is not a second job of %s" % (r["id"], r["day_career"], c))
        if r.get("post") is not None:
            staff.setdefault(r.get("work"), []).append(r["post"])
    spots = {l["id"]: len(l["objects"].get("work", [])) for l in lots(w)}
    for lid, ps in staff.items():
        if sorted(ps) != list(range(len(ps))):
            out.append("%s: its staff's posts are not 0 to %d" % (lid, len(ps) - 1))
        if len(ps) > spots.get(lid, 0):
            out.append("%s: %d staff and %d places to work" % (lid, len(ps), spots.get(lid, 0)))
    return out


def side_of_door(l):
    x0, z0, x1, z1 = l["rect"]
    dx, dz = l["door"]
    return "n" if abs(dz - z0) < .01 else "s" if abs(dz - z1) < .01 else "w" if abs(dx - x0) < .01 else "e"


# ---------------------------------------------------------------- every building is a design (BUILD-001 to BUILD-003, BUILDER.md)
def js_hash(*keys):
    """The slice's hash() (Prototype/play/play.js), bit for bit: the same keys give the same number in 0..1 in both."""
    M = 0xFFFFFFFF
    h = 2166136261
    for a in keys:
        if isinstance(a, float) and a.is_integer():
            a = int(a)
        for ch in str(a):
            h ^= ord(ch)
            h = (h * 16777619) & M
        h ^= 124
        h = (h * 16777619) & M
    h ^= h >> 15
    h = (h * 2246822519) & M
    h ^= h >> 13
    h = (h * 3266489917) & M
    h ^= h >> 16
    return h / 4294967296


def js_pick(lst, r):
    return lst[min(len(lst) - 1, int(r * len(lst)))]


def special_kind(b, l):
    """The slice's specialKind(): which buildings are built as a kind of their own."""
    t, tags = b["template"], l.get("tags", [])
    if l["kind"] in ("bank", "dealer", "library", "museum", "cinema"):
        return l["kind"]
    if t == "hospital" or "hospital" in tags:
        return "hospital"
    if t == "mall" or "mall" in tags:
        return "mall"
    if t == "hall":
        return "hall"
    if t == "market" and l["kind"] == "work":
        return "guild"
    if l.get("tower"):
        return "flats" if l["kind"] == "home" else "office"
    if l["kind"] == "work" and t == "peri":
        return "works"
    if l["kind"] == "utility" and l["enterable"]:
        return "depot"
    return None


FACADES = ("n", "s", "e", "w")


def design_of(w, b, l, surf=None):
    """A building's design (BUILD-001): everything about how it is built and finished, from World/rules/building_kit.json,
    surfaces.json and storeys.json and the lot. The town's buildings get theirs from the rules here, picking from the kit's
    'town' lists by the lot's id with the slice's hash, so each keeps the look it had; a player's design (the designer) is
    the same document. Every choice is named by its id in the kit (BUILD-016); world.built() gives the colours and sizes.
    Walls are chosen per facade (BUILD-018), inside walls and floors per storey (BUILD-019)."""
    K, T, i = w["building_kit"], w["building_kit"]["town"], l["id"]
    x0, z0, x1, z1 = l["rect"]
    dx, dz = l["door"]
    side = "n" if abs(dz - z0) < .01 else "s" if abs(dz - z1) < .01 else "w" if abs(dx - x0) < .01 else "e"
    tower, kind, n = bool(l.get("tower")), l["kind"], l["floors"]
    big = (x1 - x0) * (z1 - z0) > K["doors"]["big_from_m2"] or tower
    H = max(n * 3 + .4, 3.2 + .4)
    old = b["district"] != "Outskirts" and not tower and H > 6
    sp = special_kind(b, l)
    plaster = js_pick(T["plaster"], js_hash(i, 1))
    s = (surf or surfaces(w))["lots"].get(i, {})
    pats = w["surfaces"]["patterns"]
    pattern = list(pats)[s["wall"]] if "wall" in s else None
    col = s.get("color")
    stone = dict(pattern=pattern, color=None if col is None else pats[pattern]["colors"].index(col))
    stone_up = sp == "office" or (b["template"] == "peri" and js_hash(i, "st") < K["odds"]["stone_to_eaves_old_quarter"])
    facade = dict(plaster=plaster, stone=stone, stone_to_eaves=stone_up, brick_to_eaves=bool(s.get("upper")))
    flat = big or kind == "utility"
    along_x = x1 - x0 >= z1 - z0
    R = K["roofs"]["pitched"]
    roof = dict(type="flat" if flat else "cornice" if old else "pitched", tiles=js_pick(T["roof_tiles"], js_hash(i, 2)),
                ridge="x" if along_x else "z", rise=min(R["rise_max"], (z1 - z0 if along_x else x1 - x0) * R["rise_share"]),
                chimney=js_hash(i, "ch"))
    cls = "flats" if sp == "flats" else "tower" if tower else "old" if old else "plain"
    shut = (not tower or sp == "flats") and kind in ("home", "food", "shop") and (sp == "flats" or js_hash(i, "sh") < K["odds"]["shutters"])
    door_len = (x1 - x0) if side in "ns" else (z1 - z0)
    front = kind in ("shop", "food") and not tower
    openings = dict(**{"class": cls}, shutters=bool(shut),
                    shutter_color=js_pick(T["shutters_old"] if old else T["accents"], js_hash(i, "shc")),
                    frame=js_pick(T["frames"], js_hash(i, "fr")),
                    balcony=js_hash(i, "bal") < K["odds"]["balcony"].get(kind, 0),
                    keystones=old or js_hash(i, "ks") < K["odds"]["keystones"],
                    shop_front=min(door_len - 2, K["shop_front_max"]) if front else 0,
                    awning=js_pick(T["accents"], js_hash(i, 3)))
    ornament = dict(quoins=not stone_up and not tower,
                    lantern=kind in ("shop", "food", "health") or big or js_hash(i, "lan") < K["odds"]["lantern"],
                    crates=kind in ("shop", "food") and js_hash(i, "cr") < K["odds"]["crates"])
    F = K["floors"]
    floors = [F.get(kind, F["work"])]                         # the ground floor is laid by its kind
    for f in range(1, n):
        k2, sp2 = K["floor_uses"].get((l.get("storeys") or [None] * n)[f] or "rooms", K["floor_uses"]["rooms"])
        floors.append(F.get(sp2) or F.get(k2) or F["work"])
    return dict(v=2, special=sp,
                shell=dict(kind=kind, tower=tower, plan=(l.get("house") or {}).get("plan"), floors=n, height=H, side=side, door=[dx, dz],
                           door_width=K["doors"]["big"] if big else K["doors"]["small"], big=big, old=old,
                           floor_levels=[storey_floor(f) for f in range(n)]),
                walls={f: copy.deepcopy(facade) for f in FACADES},
                roof=roof, openings=openings, ornament=ornament,
                inside=dict(uses=l.get("storeys") or ["main"] * n, floors=floors, walls=[plaster] * n),
                circulation=dict(stair=l.get("stair"), lift=l.get("lift")))


def kit_id(K, rgb):
    """The building kit's id of a colour, matched by its rgb (Tools/worldgen/add_kit_colours_01.py put every colourway
    colour into the kit)."""
    for cid, v in K["colours"].items():
        if all(abs(a - b) < .02 for a, b in zip(v["rgb"], rgb)):
            return cid
    return None


def style_ids(w, style):
    """A building's approved colourway (World/rules/building_styles.json, ADR-0036) as building kit ids, or None."""
    S, K = w.get("building_styles"), w["building_kit"]
    if not style or not S or style.get("group") not in S["colourways"]:
        return None
    c = S["colourways"][style["group"]].get(style.get("way", "a"))
    if not c:
        return None
    P, out = S["palette"], dict(stone_walls=c.get("wall") == "stone")
    if c.get("wall") and c["wall"] != "stone":
        out["plaster"] = kit_id(K, S["plasters"][c["wall"]][1])
    if c.get("roof"):
        lo, hi = S["roofs"][c["roof"]]
        out["tiles"] = kit_id(K, [(a + b) / 2 for a, b in zip(lo, hi)])
    for part in ("shutter", "frame"):
        if c.get(part):
            out[part] = kit_id(K, P[c[part]])
    aw = [k for k in (c.get("awning") or []) if k != "cream"] or ([c["front"]] if c.get("front") else [])   # the awning's colour, or a shop front's
    if aw:
        out["accent"] = kit_id(K, P[aw[0]])
    return {k: v for k, v in out.items() if v is not None}


def apply_style(d, w, l):
    """A building's design (v2: colours by kit id, walls per facade) in its approved colourway instead of the kit's picks
    by hash (ADR-0036). The sandstone and pale stone mixes of building_styles.json are not stone colours of surfaces.json
    yet: such buildings keep the kit's stone."""
    c = style_ids(w, l.get("style"))
    if not c:
        return d
    for f in FACADES:
        fa = d["walls"][f]
        if "plaster" in c:
            fa["plaster"] = c["plaster"]
        if c["stone_walls"]:
            fa["stone_to_eaves"] = True
    if "plaster" in c:
        d["inside"]["walls"] = [c["plaster"]] * len(d["inside"]["walls"])
    if "tiles" in c:
        d["roof"]["tiles"] = c["tiles"]
    for part, key in (("shutter", "shutter_color"), ("frame", "frame"), ("accent", "awning")):
        if part in c:
            d["openings"][key] = c[part]
    d["style"] = dict(l["style"])
    return d


def designs(w):
    """Every building's design, by lot id (BUILD-001). Worked out from the data each time, never stored for the town's own
    buildings; a player's design, when there is one, will replace its building's entry."""
    surf = surfaces(w)
    return {l["id"]: apply_style(design_of(w, b, l, surf), w, l) for b in w["blocks"] for l in b["lots"] if l["kind"] not in OPEN_KINDS}


def colour(w, cid):
    """The rgb of a colour id in the building kit."""
    return list(w["building_kit"]["colours"][cid]["rgb"])


def inner_colour(w, cid):
    """An inside wall in plaster colour cid: mixed toward the kit's inner colour (as the slice mixes it, unrounded)."""
    p, mx = colour(w, cid), w["building_kit"]["inner_mix"]
    return [p[k] + (mx["with"][k] - p[k]) * mx["share"] for k in range(3)]


def built(w, d):
    """A design as the slice builds it (BUILD-002): every id turned into its colour, floor finish or window sizes. The
    design stays the document; this is worked out from it each time."""
    K, pats = w["building_kit"], w["surfaces"]["patterns"]
    out = copy.deepcopy(d)
    for f in FACADES:
        fa = out["walls"][f]
        fa["plaster"] = colour(w, fa["plaster"])
        st = fa["stone"]
        st["color"] = None if st["color"] is None else pats[st["pattern"]]["colors"][st["color"]]
    out["roof"]["tiles"] = colour(w, d["roof"]["tiles"])
    o = out["openings"]
    for k in ("shutter_color", "frame", "awning"):
        o[k] = colour(w, d["openings"][k])
    win = K["windows"][o["class"]]
    o.update(width=win["width"], gap=win["gap"], tall=win["tall"])
    FF = K["floor_finishes"]
    out["inside"]["floors"] = [[FF[x]["mat"], list(FF[x]["rgb"])] for x in d["inside"]["floors"]]
    out["inside"]["walls"] = [inner_colour(w, x) for x in d["inside"]["walls"]]
    return out


def allowed(item, d):
    """Whether a catalogue item (a roof, a window, an opening, an ornament) may go on the building of design d (BUILD-017):
    approved, and its 'when' fits."""
    if item.get("status") != "approved":
        return False
    wh, sh = item.get("when", {}), d["shell"]
    if "big" in wh and wh["big"] != sh["big"]:
        return False
    if "old" in wh and wh["old"] != sh["old"]:
        return False
    if "tower" in wh and wh["tower"] != sh["tower"]:
        return False
    if "kinds" in wh and sh["kind"] not in wh["kinds"]:
        return False
    if "not_kinds" in wh and sh["kind"] in wh["not_kinds"]:
        return False
    if "specials" in wh and d.get("special") not in wh["specials"]:
        return False
    return True


def design_problems(w, ds=None):
    """Every rule a design must keep (BUILD-001, BUILD-003, BUILD-016 to BUILD-019): all its parts there, every choice an
    id from the kit, every facade and every storey given, every catalogue item approved and right for the building."""
    K, out = w["building_kit"], []
    ds = ds or designs(w)
    cols, pats = K["colours"], w["surfaces"]["patterns"]
    for i, d in ds.items():
        bad = [part for part in ("shell", "walls", "roof", "openings", "ornament", "inside", "circulation") if part not in d]
        out += ["%s: design has no %s" % (i, part) for part in bad]
        if bad:
            continue
        for f in FACADES:
            fa = d["walls"].get(f)
            if fa is None:
                out.append("%s: no %s facade" % (i, f))
                continue
            if fa.get("plaster") not in K["plaster"]:
                out.append("%s: %s facade's plaster colour not in the kit" % (i, f))
            st = fa.get("stone", {})
            if st.get("pattern") is not None:
                if st["pattern"] not in pats:
                    out.append("%s: %s facade's stone pattern not in surfaces.json" % (i, f))
                elif st.get("color") is not None and not (isinstance(st["color"], int) and 0 <= st["color"] < len(pats[st["pattern"]]["colors"])):
                    out.append("%s: %s facade's stone colour is not one %s comes in" % (i, f, st["pattern"]))
        if d["roof"]["tiles"] not in K["roof_tiles"]:
            out.append("%s: roof tiles not in the kit" % i)
        if d["roof"]["type"] not in K["roofs"]:
            out.append("%s: roof type %s not in the kit" % (i, d["roof"]["type"]))
        elif not allowed(K["roofs"][d["roof"]["type"]], d):
            out.append("%s: a %s is not for this building" % (i, K["roofs"][d["roof"]["type"]]["name"].lower()))
        if d["openings"]["class"] not in K["windows"]:
            out.append("%s: window type not in the kit" % i)
        elif not allowed(K["windows"][d["openings"]["class"]], d):
            out.append("%s: %s are not for this building" % (i, K["windows"][d["openings"]["class"]]["name"].lower()))
        for k, lst in (("awning", "accents"), ("frame", "frames")):
            if d["openings"][k] not in K[lst]:
                out.append("%s: %s colour not in the kit" % (i, k))
        if d["openings"]["shutter_color"] not in cols:
            out.append("%s: shutter colour not in the kit" % i)
        for part, cat in (("openings", "openings"), ("ornament", "ornaments")):
            for k, v in d[part].items():
                if v is True and k in K[cat] and K[cat][k].get("status") != "approved":
                    out.append("%s: %s is not approved" % (i, K[cat][k]["name"].lower()))
        n = d["shell"]["floors"]
        if not (len(d["inside"]["floors"]) == len(d["inside"]["uses"]) == len(d["inside"]["walls"]) == n):
            out.append("%s: %d floors but %d finishes" % (i, n, len(d["inside"]["floors"])))
        for f, x in enumerate(d["inside"]["floors"]):
            if x not in K["floor_finishes"] or x == "note":
                out.append("%s: floor %d's finish not in the kit" % (i, f))
        for f, x in enumerate(d["inside"]["walls"]):
            if x not in K["plaster"]:
                out.append("%s: floor %d's inside walls not a kit colour" % (i, f))
        if n >= 2 and not d["circulation"]["stair"] and not d["shell"]["plan"]:      # a designed house has its plan's own stair (houses.js)
            out.append("%s: no stair in the design" % i)
    return out


def settings_problems(w):
    """Every rule World/rules/settings.json must keep (UI-005, UI-008 to UI-010): each setting's default fits its type and
    range; every menu tab names an action; every platform binds every action, and no control does two things where both
    can happen (within one context, or in a context and anywhere)."""
    S, out = w["settings"], []
    for g, G in S["groups"].items():
        for k, s in G["settings"].items():
            t, d = s.get("type"), s.get("default")
            if t == "switch" and not isinstance(d, bool):
                out.append("setting %s.%s: a switch's default is yes or no" % (g, k))
            elif t == "choice" and d not in s.get("options", {}):
                out.append("setting %s.%s: default %s is not one of its options" % (g, k, d))
            elif t == "range":
                lo, hi, st = s.get("min"), s.get("max"), s.get("step")
                if not all(isinstance(x, (int, float)) and not isinstance(x, bool) for x in (lo, hi, st, d)) or not lo <= d <= hi or st <= 0:
                    out.append("setting %s.%s: a range needs min <= default <= max and a step" % (g, k))
                elif abs((d - lo) / st - round((d - lo) / st)) > 1e-6:
                    out.append("setting %s.%s: default %s is not on its steps" % (g, k, d))
            elif t not in ("switch", "choice", "range"):
                out.append("setting %s.%s: no type %s" % (g, k, t))
            if not s.get("name"):
                out.append("setting %s.%s has no name" % (g, k))
    A, C = S["actions"], S["contexts"]
    for a, x in A.items():
        if x.get("context") not in C:
            out.append("action %s: no context %s" % (a, x.get("context")))
    for t in S["menu"]["tabs"]:
        if t["action"] not in A:
            out.append("menu tab %s: no action %s" % (t["id"], t["action"]))
        if t.get("group") not in [g["id"] for g in S["menu"].get("groups", [])]:      # UI-015: every tab is in a group
            out.append("menu tab %s: no group %s" % (t["id"], t.get("group")))
        if t.get("colour") not in S["menu"].get("colours", {}):                       # UI-016: and has a board colour
            out.append("menu tab %s: no colour %s" % (t["id"], t.get("colour")))
        if t.get("board", "big") not in ("big", "tall", "small"):
            out.append("menu tab %s: no board size %s" % (t["id"], t.get("board")))
    for k, c in S["menu"].get("colours", {}).items():
        if not (isinstance(c, list) and len(c) == 2 and all(isinstance(x, str) and len(x) == 7 and x[0] == "#" for x in c)):
            out.append("menu colour %s: needs a board colour and a lettering colour, as #rrggbb" % k)
    if "quiet" not in S["menu"].get("colours", {}):
        out.append("menu colours: no quiet colour for the small boards")
    L = S["menu"].get("live", {})                                                     # UI-017: the camera on the player in the menu
    for k in ("face_d", "face_y", "face_m", "body_y", "body_m", "body_fill"):
        if not (isinstance(L.get(k), (int, float)) and not isinstance(L.get(k), bool) and L[k] > 0):
            out.append("menu live.%s: needs a number above 0" % k)
    if isinstance(L.get("body_fill"), (int, float)) and L["body_fill"] > 1:
        out.append("menu live.body_fill: the player cannot fill more than the screen")
    for p, P in S["platforms"].items():
        got = P["controls"]
        for a in A:
            if not got.get(a):
                out.append("%s: nothing does %s" % (p, a))
        for a in got:
            if a not in A:
                out.append("%s: %s is not an action" % (p, a))
        by = {}
        for a, ks in got.items():
            if a not in A:
                continue
            for k in ks:
                by.setdefault(k, []).append(a)
        for k, acts in sorted(by.items()):
            ctx = [A[a]["context"] for a in acts]
            clash = len(ctx) != len(set(ctx)) or ("anywhere" in ctx and len(ctx) > 1)
            if clash:
                out.append("%s: %s does %s" % (p, k, " and ".join(acts)))
    if S["save"]["slots"] < 1:
        out.append("save: at least one slot")
    return out


def outfit_problems(w, outfit):
    """What is wrong with one outfit of the wardrobe (CHR-014): only the parts settings.json `wardrobe.parts` names, each one
    the character designer offers (World/rules/designer.json), and a name."""
    d, parts, out = w["designer"], w["settings"]["wardrobe"]["parts"], []
    for k in outfit:
        if k != "name" and k not in parts:
            out.append("an outfit cannot change %s" % k)
    for k in parts:
        if k not in outfit:
            out.append("the outfit has no %s" % k)
    full = dict(d["default"], **outfit)
    full["name"] = d["default"]["name"]
    out += [p for p in look_problems(d, full) if p.split(" ")[0] in parts]
    if not isinstance(outfit.get("name"), str) or not 0 < len(outfit["name"].strip()) <= d["name"]["max"]:
        out.append("an outfit needs a name of 1 to %d letters" % d["name"]["max"])
    return out


# ---- UI-020, UI-021 (buy menus show what is for sale as objects; ADR-0044): World/rules/buy_menu.json. buy_menu_shows
# says what a thing shows, buy_menu_problems what the file must keep. Prototype/play/buymenu.js does the same in the slice,
# and its self-test compares with buy_menu_checks (packed by build_play.py).
def buy_menu_shows(w, thing):
    """What a thing for sale shows in a buy menu: ["piece", id] (its own object: a piece of furniture, or a mall good for
    the home, drawn as it is in a home) or ["shape", name, colour] (a stand-in of buy_menu.json; colour is the designer's
    palette name of the clothing's own colour, or None). None for a thing nobody sells."""
    B, goods, items, pieces = w["buy_menu"], w["mall_goods"]["goods"], w["economy"]["items"], w["furnishing"]["pieces"]
    S = B["stand_in"]
    if thing in pieces and thing not in goods and thing not in items:
        return ["piece", thing]
    G = goods.get(thing)
    if G and G["goes"] == "home":
        return ["piece", G["piece"]]
    if thing in S["things"]:
        return ["shape", S["things"][thing], None]
    if G and G["goes"] == "wear":
        for part, shape, colour in S["wear"]:
            if G.get("parts", {}).get(part):
                return ["shape", shape, G["parts"].get(colour) if colour else None]
        return None
    if G or thing in items:
        return ["shape", S["goes"].get(G["goes"] if G else "carry"), None]
    return None


def buy_menu_things(w):
    """Every thing a buy menu can show: the mall's goods, the town's items that are for sale, the designer's pieces."""
    items = [k for k, v in w["economy"]["items"].items() if not v.get("quest")]
    rooms = [k for r in w["furnishing"]["rooms"].values() for k in r]
    return list(w["mall_goods"]["goods"]) + items + rooms


def buy_menu_checks(w):
    return {t: buy_menu_shows(w, t) for t in buy_menu_things(w)}


def buy_menu_problems(w):
    """Every rule World/rules/buy_menu.json must keep (UI-020, UI-021): the two layouts have room for what they show, every
    place a thing can go has its words and a mark the menu can paint, every stand-in is a few small boxes in colours
    the file names, and every thing for sale shows an object or a stand-in."""
    B, out = w["buy_menu"], []
    num = lambda v, lo, hi: type(v) in (int, float) and lo <= v <= hi
    W, N, P = B.get("wide") or {}, B.get("narrow") or {}, B.get("picture") or {}
    for k, lo, hi in (("panel_px", 620, 1400), ("cell_px", 80, 200), ("stage_px", 60, 200), ("show_px", 220, 480), ("show_stage_px", 160, 480), ("list_px", 200, 900)):
        if not num(W.get(k), lo, hi):
            out.append("buy menu: wide.%s must be %d to %d" % (k, lo, hi))
    for k, lo, hi in (("panel_px", 300, 620), ("columns", 3, 6), ("cell_min_px", 48, 120), ("list_px", 160, 900)):
        if not num(N.get(k), lo, hi):
            out.append("buy menu: narrow.%s must be %d to %d" % (k, lo, hi))
    for k, lo, hi in (("fov_deg", 10, 60), ("pitch_deg", 0, 45), ("start_deg", -90, 90), ("turn_s", 4, 120), ("drag_deg_px", .1, 3), ("stand_in_fill", .4, 1), ("most_px", 256, 2048)):
        if not num(P.get(k), lo, hi):
            out.append("buy menu: picture.%s must be %s to %s" % (k, lo, hi))
    if out:
        return out
    if type(N["columns"]) is not int:
        out.append("buy menu: narrow.columns must be a whole number")
    if not num(B.get("narrow_px"), N["panel_px"] + 1, W["panel_px"] - 1):
        out.append("buy menu: narrow_px must lie between the narrow and the wide panel")
    pad, gap = 46, 6                                        # the panel's own padding and the space between cells, as the page lays them out
    if N["columns"] * N["cell_min_px"] + (N["columns"] - 1) * gap > N["panel_px"] - pad:
        out.append("buy menu: %d cells of %d px do not fit a narrow panel of %d px" % (N["columns"], N["cell_min_px"], N["panel_px"]))
    if W["panel_px"] - pad - W["show_px"] - 16 < 3 * W["cell_px"] + 2 * 9:
        out.append("buy menu: the wide panel has no room for three cells beside the showcase")
    if W["stage_px"] > W["cell_px"] or W["show_stage_px"] > W["show_px"]:
        out.append("buy menu: a picture is wider than the cell it stands in")
    boards = w["settings"]["menu"]["colours"]
    want = set(w["mall_goods"]["goes"]) | {"place"}
    for g in sorted(want - set(B["goes"])):
        out.append("buy menu: no words for where a thing goes: %s" % g)
    for g, G in B["goes"].items():
        if not all(isinstance(G.get(k), str) and G[k].strip() for k in ("name", "to")):
            out.append("buy menu: goes.%s needs a name and where it goes to" % g)
        if G.get("colour") not in boards:
            out.append("buy menu: goes.%s has no board colour %s" % (g, G.get("colour")))
    cols = B["colours"]
    for k, c in cols.items():
        if not (isinstance(c, list) and len(c) == 3 and all(num(v, 0.02, 0.85) for v in c)):
            out.append("buy menu: colour %s must be three numbers from 0.02 to 0.85 (the town's muted colours)" % k)
    for k in w["economy"]["shops"]:
        if B["bands"].get(k) not in cols:
            out.append("buy menu: no band colour for the %s" % k)
    for k, K in w["mall_outlets"]["kinds"].items():
        if K.get("sign") not in cols:
            out.append("buy menu: no colour %s for the sign of %s outlets" % (K.get("sign"), k))
    for name, boxes in B["shapes"].items():
        if not isinstance(boxes, list) or not boxes:
            out.append("buy menu: shape %s has no boxes" % name)
            continue
        for b in boxes:
            ok = isinstance(b, list) and 7 <= len(b) <= 9 and all(num(v, -1, 1) for v in b[:6]) and all(v > 0 for v in b[3:6])
            if not ok:
                out.append("buy menu: shape %s has a box that is not [across, up, back, width, height, depth, colour, shade, turn]" % name)
                continue
            if b[6] not in cols and b[6] not in ("band", "own"):
                out.append("buy menu: shape %s uses colour %s, which the file does not name" % (name, b[6]))
            if len(b) > 7 and not num(b[7], .3, 1.2):
                out.append("buy menu: shape %s has a shade outside 0.3 to 1.2" % name)
            if b[1] < 0 or b[1] + b[4] > .5 or abs(b[0]) + b[3] / 2 > .3 or abs(b[2]) + b[5] / 2 > .3:
                out.append("buy menu: shape %s is bigger than a stand-in may be (0.6 m across, 0.5 m high)" % name)
    S, parts, palette = B["stand_in"], w["settings"]["wardrobe"]["parts"], w["designer"]["palette"]
    for g in ("carry", "eat"):
        if S["goes"].get(g) not in B["shapes"]:
            out.append("buy menu: no stand-in for things to %s" % g)
    for row in S["wear"]:
        if not (isinstance(row, list) and len(row) == 3 and row[0] in parts and row[1] in B["shapes"] and (row[2] is None or row[2] in parts)):
            out.append("buy menu: stand_in.wear row %s is not [part of the look, shape, part that gives the colour]" % (row,))
    known = set(w["mall_goods"]["goods"]) | set(w["economy"]["items"])
    for t, shape in S["things"].items():
        if t not in known:
            out.append("buy menu: stand-in for %s, which is neither a good nor an item" % t)
        if shape not in B["shapes"]:
            out.append("buy menu: %s shows %s, which is no shape" % (t, shape))
    if out:
        return out
    pieces = w["furnishing"]["pieces"]
    for t in buy_menu_things(w):
        s = buy_menu_shows(w, t)
        if not s or not s[1]:
            out.append("buy menu: %s shows nothing" % t)
        elif s[0] == "piece" and s[1] not in pieces:
            out.append("buy menu: %s shows the piece %s, which the furnishing list does not have" % (t, s[1]))
        elif s[0] == "shape" and s[2] is not None and s[2] not in palette:
            out.append("buy menu: %s has the colour %s, which the designer's palette does not have" % (t, s[2]))
        elif s[0] == "shape" and s[2] is None and any(b[6] == "own" for b in B["shapes"][s[1]]):
            out.append("buy menu: %s shows %s, which needs a colour of its own" % (t, s[1]))
    return out
