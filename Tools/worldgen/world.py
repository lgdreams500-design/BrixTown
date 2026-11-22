"""Loads a city from World/ and checks it. Nothing here generates anything: it only reads the data files.

    import world
    w = world.load()                 # the whole city as one dict
    problems = world.validate(w)     # [] when the data is sound
    city = world.simulation(w)       # a citysim.City running the living-city rules over the whole map
    city = world.simulation(w, traffic=True)   # the same, with residents' cars on the roads (citysim/traffic.py)
"""
import json, math, os, sys

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
    for name in ("roles", "choices", "calendar", "vegetation", "signatures", "traffic", "dialogue", "speech", "sound", "dodge", "economy", "quests", "designer", "dressing", "sky", "crash", "surfaces"):
        w[name] = _read(rules, name + ".json")
    roads = os.path.join(base, "roads.json")           # the road network (CITY-043); written by make_roads_01.py
    w["roads"] = _read(roads) if os.path.exists(roads) else None
    return w


def lots(w):
    return [l for b in w["blocks"] for l in b["lots"]]


def validate(w):
    """Every rule a block file must keep. Returns a list of problems in plain words."""
    out = []
    g = w["city"]["grid"]
    seen, ids = {}, set()
    for b in w["blocks"]:
        i, j = b["grid"]
        if (i, j) in seen:
            out.append("two blocks at %s: %s and %s" % ((i, j), seen[(i, j)], b["id"]))
        seen[(i, j)] = b["id"]
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
            if a0 < x0 - pave or c0 < z0 - pave or a1 > x0 + b["size"] + pave or c1 > z0 + b["size"] + pave:
                out.append("%s: footprint leaves its block" % l["id"])
            if l["kind"] not in OPEN_KINDS:
                dx, dz = l["door"]
                on_edge = (abs(dx - a0) < 0.01 or abs(dx - a1) < 0.01) and c0 <= dz <= c1 or (abs(dz - c0) < 0.01 or abs(dz - c1) < 0.01) and a0 <= dx <= a1
                if not on_edge:
                    out.append("%s: door is not on a wall" % l["id"])
                for kind, pts in l["objects"].items():
                    for p in pts:
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
    if not out:                                        # every door must be reachable on foot from every other
        c = simulation(w)
        dd, _ = c.net._from(next(iter(c.anchor.values())))
        out += ["%s cannot be reached on foot" % k for k, a in c.anchor.items() if a not in dd]
    if w.get("roads") and not out:
        import traffic
        out += traffic.Roads(w["roads"], w["traffic"]).problems(lots(w))
    out += dialogue_problems(w) + speech_problems(w) + sound_problems(w) + economy_problems(w) + quest_problems(w) + designer_problems(w) + sky_problems(w)
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


def sim_data(w):
    """The city in the shape the living-city rules read (Tools/citysim/citysim.py)."""
    g = w["city"]["grid"]
    P, n = g["pitch"], g["columns"]
    streets = []
    for k in range(g["rows"] + 1):
        for m in range(n):
            streets.append([[m * P, k * P], [(m + 1) * P, k * P]])
    for k in range(n + 1):
        for m in range(g["rows"]):
            streets.append([[k * P, m * P], [k * P, (m + 1) * P]])
    for b in w["blocks"]:
        streets += b["paths"]
    cal = w["calendar"]
    veg = w["vegetation"]
    return dict(name=w["city"]["name"], size=[n * P, g["rows"] * P], calendar=cal["calendar"], weather=cal["weather"], tram=cal["tram"],
                streets=streets, lots=lots(w), residents=w["residents"], roles=w["roles"]["roles"], signatures=w["signatures"]["signatures"],
                choices=dict(w["choices"]["choices"], rounds=[]), use=w["choices"]["use"], species=veg["species"], flowers=veg["flowers"],
                replant_gap_days=veg["replant_gap_days"], fallen_days=veg["fallen_days"], trees=[t for b in w["blocks"] for t in b["trees"]], lamps=[])


def simulation(w, traffic=False):
    """The living-city rules over the whole map. With traffic=True, residents who own a car drive their longer
    trips (LIVING_CITY CITY-043 to CITY-049). The playable slice does not draw cars yet, so it stays off by default."""
    import citysim
    city = citysim.City(data=sim_data(w))
    if traffic:
        import traffic as tr
        tr.Traffic(city, w["roads"], w["traffic"])
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
