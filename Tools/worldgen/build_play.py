"""Packs the city data and one day of everyone's routine into Prototype/play/city_data.js for the playable slice.

The slice (Prototype/play/index.html) builds every street, building and interior from the block files as they
are, so a change to World/ shows up after re-running this script and reloading the page.

Run:  python build_play.py [day]        (default day 16: a summer market day with fair weather)
"""
import json, os, re, sys

import world
import citysim

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "Prototype", "play", "city_data.js"))
OUT_SEASONS = os.path.join(os.path.dirname(OUT), "city_seasons.js")
OUT_UNREAL = os.path.normpath(os.path.join(HERE, "..", "..", "Game", "Data", "day.json"))   # the same day for the Unreal build
OUT_UNREAL_CARS = os.path.join(os.path.dirname(OUT_UNREAL), "cars.json")                     # and its vehicles (CITY-040)
OUT_UNREAL_INSIDE = os.path.join(os.path.dirname(OUT_UNREAL), "inside.json")                 # and what is inside its buildings (CITY-013)
CAR_MODELS_JS = os.path.join(os.path.dirname(OUT), "car_models.js")                         # Tools/blender/car_export_11.py
OUT_UNREAL_CAR_MODELS = os.path.join(os.path.dirname(OUT_UNREAL), "car_models.json")
OUT_UNREAL_STREETS = os.path.join(os.path.dirname(OUT_UNREAL), "streets.json")               # how its gardens and street dressing look today (CITY-017, WLD-034)
OUT_UNREAL_LIFE = os.path.join(os.path.dirname(OUT_UNREAL), "life.json")                   # money, shops and quests (ADR-0012)
ACTS = ["sleep", "home", "work", "eat", "shop", "rest", "sell", "wait", "visit", "tend", "deliver", "collect", "stroll", "walk", "arrive", "eat_home", "check", "shelter"]
USES = ["bed", "room", "work", "seat", "customer", "bench", "stall", "wait", "chair", "flowerbed", "rim"]


def js_object(path, name):
    """A literal object `const <name> = { ... };` of one of the slice's scripts, as data (numbers, arrays, quoted or bare keys)."""
    import re
    js = open(path, encoding="utf-8").read()
    i = js.index("const " + name + " = {")
    j, depth = js.index("{", i), 0
    for k in range(j, len(js)):
        depth += {"{": 1, "}": -1}.get(js[k], 0)
        if depth == 0:
            break
    body = re.sub(r'//[^\n]*', '', js[j:k + 1])                                # its comments
    body = re.sub(r'([{,]\s*)([A-Za-z_]\w*)\s*:', r'\1"\2":', body)      # bare keys quoted
    body = re.sub(r'(?<![\d.])\.(\d)', r'0.\1', body)                     # .45 -> 0.45
    return json.loads(body)


def rules_file(name):                                    # a rules file world.load() does not read (yet)
    return json.load(open(os.path.join(HERE, "..", "..", "World", "rules", name + ".json"), encoding="utf-8"))


def r1(v):
    return round(v, 1)


def routines(city, day, car_index):
    """Each resident's day as pieces: [t0, 0, x, z, act, use, inside, lot(, floor)] stays (floor only for a place upstairs:
    stacked flats and offices have places at the same spot on each floor, WLD-053), [t0, 1, act, x, z, ...] walks, [t0, 2] away,
    [t0, 3, vehicle] in their car or on a bus (drawn in the vehicle, see cars())."""
    lot_index = {l["id"]: n for n, l in enumerate(city.d["lots"])}
    acts = {a: n for n, a in enumerate(ACTS)}
    uses = {u: n for n, u in enumerate(USES)}
    out = []
    for rid, pieces in city.plan(day).items():
        row = []
        for kind, t0, t1, payload, s in pieces:
            if t1 - t0 < 1e-6:
                continue
            lot = city.lots[s["place"]]
            if kind == "away":
                row.append([r1(t0), 2])
            elif kind in ("drive", "ride"):                 # at the wheel of their car, or on a bus (CITY-092)
                row.append([r1(t0), 3, car_index[payload]])
            elif kind == "move":
                route = payload[0]
                row.append([r1(t0), 1, acts.setdefault(s["act"], len(acts))] + [r1(c) for p in route for c in p])
            elif kind == "stroll":
                n = max(2, int((t1 - t0) / 1.0) + 1)
                pts = [city._stroll_at(rid, day, s, t0 + (t1 - t0) * k / (n - 1)) for k in range(n)]
                row.append([r1(t0), 1, acts["stroll"]] + [r1(c) for p in pts for c in p])
            else:
                x, z = payload
                x0, z0, x1, z1 = lot["rect"]
                inside = int(lot["kind"] not in world.OPEN_KINDS and s["use"] is not None and citysim.in_lot(lot, x, z))   # a turned house in its own frame
                act = "wait" if s["act"] in ("away", "arrive") else s["act"]
                use = uses.get(s["use"], -1) if s.get("slot") is not None else -1
                lv = (lot.get("levels") or {}).get(s["use"]) if s.get("slot") is not None else None
                floor = lv[s["slot"]] if lv else 0
                row.append([r1(t0), 0, r1(x), r1(z), acts.setdefault(act, len(acts)), use, inside, lot_index[s["place"]]] + ([floor] if floor else []))
        out.append(row)
    return out, sorted(acts, key=acts.get)


CAR_STEP_S = 1.0                     # real seconds between packed car positions
EASE_M = 6.0                         # metres over which a car slides between its bay and its lane


def cars(city, day):
    """Every vehicle's day for the slice (k: 0 car, 1 van, 2 bus): its home bay (none for a car passing through) and its trips, each a list of [x, z, hx, hz] every CAR_STEP_S real
    seconds (Tools/citysim/traffic.py works them out). Pulling out of a bay and into one is eased over a few seconds here:
    the rules keep a car either in its bay or on its lane, the picture slides it between the two."""
    T = city.traffic
    R = T.roads
    D = T._days[day]

    def bay(bid):
        b = R.bays[bid]
        u = b.get("u") or R.lanes[b["lane"]]["u"]
        return [r1(b["pos"][0]), r1(b["pos"][1]), round(u[0], 2), round(u[1], 2)]
    lines = sorted(T.lines)

    res_index = {r["id"]: n for n, r in enumerate(city.d["residents"])}
    out, index = [], {}
    for n, cid in enumerate(T.ids(day)):                   # residents' cars, vans, and today's cars passing through
        index[cid] = n
        trips = []
        for ti, tr in enumerate(D["trips"][cid]):
            pts, t = [], tr["t0"]
            while t < tr["t1"] - 1e-6:
                c = T.car_at(cid, day, t / T.pace)
                pts.append([c["pos"][0], c["pos"][1], c["heading"][0], c["heading"][1]])
                t += CAR_STEP_S
            last = tr["legs"][-1]
            p, h = R.point(last, last["s1"])
            pts.append([p[0], p[1], h[0], h[1]])
            # into and out of a bay: slide sideways over the last (first) EASE_M metres of the way, never along it, so
            # the car stays exactly where the rules put it on its lane and never runs up on the car ahead
            for end, bid in ((0, tr["bay0"]), (-1, tr["bay1"])):
                if bid is None:
                    continue
                bp, q = R.bays[bid]["pos"], pts[end]
                ox, oz = bp[0] - q[0], bp[1] - q[1]           # the bay's offset from the lane where the car leaves or joins it
                hx, hz = q[2], q[3]
                along = ox * hx + oz * hz
                ox, oz = ox - hx * along, oz - hz * along      # its sideways part only
                order = range(len(pts)) if end == 0 else range(len(pts) - 1, -1, -1)
                for i in order:
                    gone = abs((pts[i][0] - q[0]) * hx + (pts[i][1] - q[1]) * hz)
                    if gone >= EASE_M:
                        break
                    f = 1 - gone / EASE_M
                    f = f * f * (3 - 2 * f)
                    pts[i][0] += ox * f
                    pts[i][1] += oz * f
            st = T.stowed(cid, day, ti)                    # CITY-093: off the street while parked, [from, to] in minutes
            trips.append([round(tr["t0"] / T.pace, 3), round(tr["t1"] / T.pace, 3), bay(tr["bay1"]) if tr["bay1"] else None,
                          [round(v, 2) for q in pts for v in q]] + ([[round(st[0] / T.pace, 3), round(st[1] / T.pace, 3)]] if st else []))
        home = T.home_bay(cid)
        v = dict(home=bay(home) if home else None, trips=trips, k=2 if cid in T.buses else 1 if cid in T.vans else 0)
        if cid in T.buses:                                 # a bus (CITY-091): which line, for its colours and its sign
            v["l"] = lines.index(T.buses[cid]["line"])
        if cid in T.cars:                                  # whose car: the one at the wheel; anyone else in it is a passenger (CITY-094)
            v["o"] = res_index[T.cars[cid]["owner"]]
        out.append(v)
    return out, index, CAR_STEP_S / T.pace


def roads(city):
    """What the slice draws of the road network (CITY-043, CITY-045, CITY-046): zebra crossings [x, z, along x],
    parking lay-bys [x, z, along x], and traffic lights [x, z, offset s] that run on the same cycle as traffic.py."""
    T = city.traffic
    R, cyc = T.roads, T.r["signals"]
    cycle = 2 * (cyc["green_s"] + cyc["amber_s"] + cyc["all_red_s"])
    gates = {g["id"]: g for g in R.d.get("gates", [])}
    def ew(street):                                        # does this street run east-west?
        return gates[street]["dir"][1] == 0 if street in gates else street.startswith("s_ew_")
    return dict(
        crossings=[[r1(c["pos"][0]), r1(c["pos"][1]), 1 if ew(c["street"]) else 0] for c in R.d["crossings"]],
        bays=[[r1(b["pos"][0]), r1(b["pos"][1]), 1 if abs(R.lanes[b["lane"]]["u"][0]) > .5 else 0] for b in R.bays.values()
              if not b.get("outer") and b.get("kind") != "stop"],   # lay-bys in the town's streets; driveways and kerbs outside have none
        stops=stops(T),                                   # bus stops (CITY-091)
        lines=[dict(name=T.lines[k]["name"], colour=T.lines[k].get("colour", "teal")) for k in sorted(T.lines)],
        signals=[[j["pos"][0], j["pos"][1], round(citysim.rnd("sig", j["id"]) * cycle, 3)] for j in R.d["junctions"] if j["control"] == "signals"],
        cycle={k: cyc[k] for k in ("green_s", "amber_s", "all_red_s")},
        width=T.r["crossing_width"], bay_length=T.r["bay_length"],
        gates=[[g["name"], R.j[g["junction"]]["pos"][0], R.j[g["junction"]]["pos"][1], g["dir"][0], g["dir"][1], g["length"]]   # roads out of town (CITY-068)
               for g in R.d.get("gates", [])],
        links=[dict(name=l["name"], width=l["width"], points=[[r1(p[0]), r1(p[1])] for p in l["points"]]) for l in R.d.get("links", [])])   # the roads out to the new areas


def stops(T):
    """Every bus stop for the slice: [x, z, along x, along z, town (1: a lay-by in a town street), [line indexes], name], one
    per side of the street; the shelter stands on the kerb side, to the right of the way the bus goes."""
    R, lines = T.roads, sorted(T.lines)
    serves = {}
    for k, lid in enumerate(lines):
        for bid in T.lines[lid]["bays"]:
            serves.setdefault(bid, set()).add(k)
    out = []
    for bid, b in sorted(R.stops.items()):
        out.append([r1(b["pos"][0]), r1(b["pos"][1]), round(b["u"][0], 3), round(b["u"][1], 3), 0 if R.is_outer_bay(bid) else 1,
                    sorted(serves.get(bid, ())), b["name"]])
    return out


def dialogue(w):
    """World/rules/dialogue.json for the slice: residents named by their place in the list instead of their id."""
    d = w["dialogue"]
    at = {r["id"]: n for n, r in enumerate(w["residents"])}
    return dict(rules=d["rules"], conversations=d["conversations"], role_words=d["role_words"],
                residents={str(at[k]): v for k, v in d["residents"].items()})


def life(w, city, plan, acts):
    """Money, shops, quests and the designer for the slice (ADR-0012), with the results the page must reproduce."""
    import economy as ec
    e = w["economy"]
    lots_ = city.d["lots"]
    visit_acts = sorted(set(e["stock"]["visit_acts"]) | set(e["stall"]["visit_acts"]))
    visits = ec.visits_from_packed(plan, acts, visit_acts)
    at = {l["id"]: n for n, l in enumerate(lots_)}
    staff = {}
    for r in w["residents"]:
        if r["work"]:
            staff.setdefault(at[r["work"]], []).append(r["id"])
    goods = {k: v for k, v in w["mall_goods"].items() if k != "note"}                 # what a mall's outlets sell (ECO-013, ECO-014)
    probe = ec.Economy(e, lots_, visits, staff, goods)
    shops = {k: next(n for n in range(len(lots_)) if probe.shop_kind(n) == k) for k in ("grocer", "kiosk", "arcade")}
    script = ec.check_script() + ec.outlet_script(probe)
    want = ec.run_script(ec.Economy(e, lots_, visits, staff, goods), script, shops)
    return dict(economy={k: v for k, v in e.items() if k != "note"}, quests={k: v for k, v in w["quests"].items() if k != "note"},
                designer={k: v for k, v in w["designer"].items() if k != "note"}, goods=goods,
                visits={str(k): [[t, i] for t, i in v] for k, v in visits.items()}, visit_acts=visit_acts,
                staff={str(k): v for k, v in staff.items()}, checks=dict(script=script, shops=shops, want=want), reward_checks=reward_checks(w), achievements=achievement_rules(w),
                estate=estate_rules(w), estate_checks=estate_checks(w, lots_),
                walk_net=walk_net(city, lots_))


def bank_script():
    """A second fixed run for the Unreal port (BANK-002, BANK-008): a loan whose instalments are missed until it defaults,
    the player's things repossessed at their sell price, and the bank refusing to lend again."""
    return [["advance", 0, 440], ["new"], ["open", "current"], ["loan", 30000, 7], ["move", "p:current", "p:cash", 32500],
            ["buy", "arcade", "hat"], ["buy", "arcade", "scarf"], ["buy", "grocer", "cheese"], ["advance", 1, 600],
            ["advance", 5, 100], ["loan", 2000, 3], ["open", "current"], ["open", "savings"], ["move", "p:cash", "p:savings", 100],
            ["advance", 9, 10]]


def quest_script():
    """A fixed run of the quest lines and jobs for the Unreal port (QST-001 to QST-007): the first starter line walked to
    its gift ending, the errand line to its delivered ending, then a shopping job handed over, a courier round delivered,
    a shift worked and one let run out. Ops: economy ops (advance, open, buy) and newgame, act [qid, acts, resident id],
    check [lot id or None, x, z], talking / offers [resident id], take [job kind, day] (the first resident offering one),
    buyfor (the job's item at the first shop that sells it), handover, at [frm or to] (check at the job's lot)."""
    return [["advance", 0, 440], ["newgame"], ["talking", "r0504"], ["act", "new_in_town", ["stage:bank"], "r0504"],
            ["check", None, 0, 0], ["open", "current"], ["check", None, 0, 0], ["buy", "grocer", "bread"], ["check", None, 0, 0],
            ["talking", "r0504"], ["act", "new_in_town", ["take:bread", "end:gift"], "r0504"], ["offers", "r0518"], ["offers", "r0408"],
            ["act", "errand_runner", ["start"], "r0518"], ["check", "b_9_9_l7", 0, 0], ["talking", "r0505"],
            ["act", "errand_runner", ["take:parcel", "stage:report"], "r0505"], ["act", "errand_runner", ["rep:5", "end:delivered"], "r0518"],
            ["offers", "r0497"], ["advance", 0, 600], ["take", "shopping", 0], ["take", "parcel", 0], ["buyfor"], ["handover"],
            ["take", "parcel", 0], ["at", "frm"], ["advance", 0, 640], ["at", "to"], ["take", "shift", 0], ["at", "to"],
            ["advance", 0, 680], ["at", "to"], ["advance", 1, 500], ["take", "shift", 1], ["at", "to"], ["advance", 1, 900],
            ["check", None, 0, 0]]


def run_quests(econ, q, ops, shops):
    """Runs quest_script() ops; returns what the C++ must reproduce."""
    import economy as ec
    out = []
    for op in ops:
        name, a = op[0], op[1:]
        try:
            r = "ok"
            if name == "advance":
                econ.advance(a[0], a[1])
            elif name == "newgame":
                q.new_game()
            elif name == "open":
                econ.open_account(a[0])
            elif name == "buy":
                econ.buy(shops[a[0]], a[1])
            elif name == "act":
                q.act(a[0], a[1], a[2])
            elif name == "check":
                q.check(dict(lot=a[0], pos=(a[1], a[2]), minute=econ.minute))
            elif name == "talking":
                r = ",".join(q.talking_to(a[0]))
            elif name == "offers":
                r = ",".join(q.offers(a[0]))
            elif name == "take":
                o = next((q.job_offer(i, a[1]) for i in range(len(q.residents)) if (q.job_offer(i, a[1]) or {}).get("kind") == a[0]), None)
                if o is None:
                    raise ec.Refused("nobody offers that")
                q.take_job(o)
                r = "%d|%d" % (o["giver"], o["pay"])
            elif name == "buyfor":
                it = q.job["item"]
                n = next(n for k, n in sorted(((k, n) for n in range(len(econ.lots)) for k in [econ.shop_kind(n)] if k and n not in econ.outlets and it in econ.R["shops"][k]["sells"]), key=lambda x: x[1]))
                econ.buy(n, it)
            elif name == "handover":
                q.hand_over()
            elif name == "at":
                q.check(dict(lot=econ.lots[q.job[a[0]]]["id"], pos=None, minute=econ.minute))
            out.append(r)
        except ec.Refused:
            out.append("refused")
    offers = []
    for day in (0, 1, 2):
        for i in range(len(q.residents)):
            o = q.job_offer(i, day)
            if o:
                offers.append([i, day, o["kind"], o["pay"], o.get("frm", -1), o.get("to", -1), o.get("item", ""), o.get("minutes", 0)])
    j = q.job
    return dict(results=out, active={k: v["stage"] for k, v in sorted(q.active.items())}, done=dict(sorted(q.done.items())),
                log=[t for _, _, t in q.log], job=[j["kind"], j["step"], j["deadline"], j["worked"]] if j else None, jobs_done=q.jobs_done,
                rep=dict(sorted(econ.rep.items())), balances={a: econ.L.balance(a) for a in ec.PLAYER + ("sys:rewards", "sys:residents")},
                tx=len(econ.L.tx), inv=dict(sorted(econ.inv.items())), offers=offers[:80], offer_count=len(offers))


def unreal_life(w, city, L):
    """Game/Data/life.json for the Unreal build (Game/Source/BrixWorld BrixEconomy): the rules and the residents' shop visits
    the slice gets in D.life, each lot's id (to check the lots are in the same order) and a mall's outlets, and the fixed
    runs with the results Python gives, which the C++ must reproduce (ECO-002, BANK-008)."""
    import economy as ec
    lots_ = city.d["lots"]
    e, goods = w["economy"], {k: v for k, v in w["mall_goods"].items() if k != "note"}
    visits = {int(k): [tuple(x) for x in v] for k, v in L["visits"].items()}
    staff = {int(k): v for k, v in L["staff"].items()}
    shops = L["checks"]["shops"]
    bs = bank_script()
    want = ec.run_script(ec.Economy(e, lots_, visits, staff, goods), bs, shops)
    import quests as qs
    import estate as es
    qe = ec.Economy(e, lots_, visits, staff, goods)
    qscript = quest_script()
    qwant = run_quests(qe, qs.Quests(w["quests"], qe, w["residents"], lots_), qscript, shops)
    return dict(note="Generated by Tools/worldgen/build_play.py from World/. Do not edit: change the data files and run it again.",
                economy=L["economy"], quests=L["quests"], goods=goods, visits=L["visits"], staff=L["staff"],
                lots=[l["id"] for l in lots_], outlets={str(n): ec.outlets_of(l) for n, l in enumerate(lots_) if ec.outlets_of(l)},
                checks=L["checks"], bank_checks=dict(script=bs, shops=shops, want=want), quest_checks=dict(script=qscript, shops=shops, want=qwant), reward_checks=L["reward_checks"],
                achievements=L["achievements"], buy_menu=dict(L["buy_menu"], icons=buy_menu_icons()), settings=L["settings"],
                pieces=list(w["furnishing"]["pieces"]), palette=w["designer"]["palette"],   # for what a buy menu shows (UI-021)
                rewards={k: w["sound"]["rewards"][k] for k in ("ledger", "coins")},   # how money coming in is shown (SND-019, SND-020)
                estate=L["estate"], estate_checks=L["estate_checks"],   # the real estate market and its fixed run (PROP-009 to PROP-021)
                estate_units=es.carve(w["estate"], lots_, w["residents"]),   # every building's units, as estate.carve() makes them (PROP-009)
                residents=[dict(id=r["id"], role=r["role"], home=r["home"], work=r["work"]) for r in w["residents"]])


def buy_menu_icons():
    """The marks in a buy menu's dots and chips (UI-020, ADR-0044): the slice's buymenu.js ICON, which stays their one
    source, as {where a thing goes: [[path data, stroke width, or 0 for a fill], ...]} on a square 16 units across."""
    src = open(os.path.join(HERE, "..", "..", "Prototype", "play", "buymenu.js"), encoding="utf-8").read()
    body = re.search(r"const ICON = \{(.*?)\n  \};", src, re.S).group(1)
    out = {}
    for k, svg in re.findall(r"(\w+): '(<svg.*?</svg>)'", body):
        out[k] = []
        for tag in re.findall(r"<path [^>]*/>", svg):
            sw = re.search(r'stroke-width="([\d.]+)"', tag)
            out[k].append([re.search(r' d="([^"]+)"', tag).group(1), float(sw.group(1)) if sw else 0.0])
    if sorted(out) != ["carry", "eat", "home", "place", "wear"]:
        raise SystemExit("buymenu.js ICON is not the five marks build_play.py expects: %s" % sorted(out))
    return out


def walk_net(city, lots_):
    """The streets on foot and each lot's way in (citysim's net and anchors), for residents who have moved house to find
    their way to the new home in the slice (PROP-022): nodes [[x, z]], edges [[a, b, metres]], anchor {lot index: node}."""
    nodes = city.net.nodes
    at = {p: n for n, p in enumerate(nodes)}
    edges = [[n, at[q], round(w, 2)] for n, p in enumerate(nodes) for q, w in city.net.adj[p].items() if at[q] > n]
    anchor = {str(n): at[tuple(city.anchor[l["id"]])] for n, l in enumerate(lots_) if l["id"] in city.anchor and tuple(city.anchor[l["id"]]) in at}
    return dict(nodes=[[round(x, 2), round(z, 2)] for x, z in nodes], edges=edges, anchor=anchor)


# ---- FURN-063 (furnishing levels): the estate's rules go to the slice with World/rules/furnishing_levels.json as
# estate.furnishing, which estate.js (Estate.Market, Estate.furnish) and furnishing.js read. estate.Estate loads the same
# file itself, so estate_checks' expected results include each day's furniture transactions and the levels.
def estate_rules(w):
    import furnishing as fu
    R = {k: v for k, v in w["estate"].items() if k != "note"}
    R["furnishing"] = {k: v for k, v in fu.load_rules().items() if k not in ("note", "notes")}
    return R
# ---- end FURN-063
def builder_data(w, ds):
    """The building designer's rules for the slice (BUILD-004 to BUILD-007, BUILD-016 to BUILD-019): the kit, the planning
    rules, the stone and brick colours, and a fixed run of remodels with its results, for builder.js to reproduce; and the
    furnishing rules (furnish.py, FURN-051, FURN-052, FURN-055) with their own fixed run."""
    import builder
    import furnish
    import roomplan
    script, fs, rs = builder.check_script(w, ds), furnish.check_script(), roomplan.check_script()
    return dict(kit={k: v for k, v in w["building_kit"].items() if not k.endswith("note")},
                planning={k: v for k, v in w["planning"].items() if k != "note"},
                patterns={k: v["colors"] for k, v in w["surfaces"]["patterns"].items()},
                checks=dict(script=script, want=builder.run_script(w, ds, script)),
                furnish=dict(rules={k: v for k, v in w["furnishing"].items() if k != "note"},          # FURN-051, FURN-052, FURN-055
                             checks=dict(script=fs, want=furnish.run_script(w["furnishing"], fs))),
                room_plan=dict(rules={k: v for k, v in w["room_plan"].items() if k != "note"},           # BUILD-021 to BUILD-023
                               checks=dict(script=rs, want=roomplan.run_script(w["room_plan"], rs))))


def achievement_rules(w):
    """The achievements (QST-009) and a fixed run of facts with what is earned, for achievements.js to check itself against."""
    import achievements
    r = {k: v for k, v in w["achievements"].items() if k != "note"}
    return dict(r, checks=achievements.check_script(r))


def reward_checks(w):
    """A few ledger transactions and what the player is shown for them (SND-019, SND-020), for rewards.js to check itself against."""
    tx = [[0, 1, 600, "Reward: A parcel for the baker", [["sys:rewards", -5000], ["p:cash", 5000]]],
          [1, 1, 700, "Job: A delivery", [["sys:rewards", -45000], ["p:cash", 45000]]],
          [2, 1, 1440, "Interest, current account", [["bank:vault", -12], ["p:current", 12]]],
          [3, 1, 1440, "Interest, savings account", [["bank:vault", -30], ["p:savings", 30]]],
          [4, 2, 10, "Loan L1 paid out", [["bank:vault", -100000], ["p:current", 100000]]],
          [5, 2, 20, "Transfer", [["p:cash", -100], ["p:current", 100]]],
          [6, 2, 30, "Sold bread, the grocer", [["shop:1", -200], ["p:cash", 200]]]]
    return dict(tx=tx, want=world.reward_events(w["sound"], tx))


def estate_checks(w, lots_):
    """The real estate market's fixed run (estate.check_script) and its results, for estate.js to reproduce (PROP-021)."""
    import economy as ec
    import estate as es
    script = es.check_script()
    want = es.run_script(es.Estate(w["estate"], ec.Economy(w["economy"], lots_, {}, {}), lots_, w["residents"]), script)
    return dict(script=script, want=want)


def season_days(day, cal):
    """The same day of each season as the day the slice is built for: day 16 gives 4, 16, 28 and 40."""
    n = cal["days_per_season"]
    return [s * n + day % n for s in range(len(cal["seasons"]))]


def tree_rows(w, city, day):
    rows = []
    crowns = {"round": 0, "cone": 1, "slim": 2}                 # how play.js draws the crown (vegetation.json "crown", WLD-056)
    for b in w["blocks"]:
        for t in b["trees"]:
            v = city.veg.tree(t, day)
            if v["state"] == "empty":
                continue
            c = v.get("color", [0.3, 0.33, 0.2])
            rows.append([t["pos"][0], t["pos"][1], round(v["radius"], 2), round(v["leaf"], 2), round(c[0], 3), round(c[1], 3), round(c[2], 3),
                         int(v.get("blossom", 0)), int(v["state"] == "fallen"), crowns[city.veg.d["species"][t["species"]].get("crown", "round")]])
    return rows


def flower_row(w, city, day):
    fl = city.veg.flowers(day)
    return dict(size=round(fl["size"], 2), bloom=round(fl["bloom"], 2), colors=w["vegetation"]["flowers"]["colors"])


def season_looks(w, city, day):
    """What a season changes to the eye, for the slice's season key: daylight hours, the weather of that day, trees and flowers.
    Everyone's routine, the cars and the notice boards stay those of the day the slice is built for."""
    cal, out = w["calendar"]["calendar"], {}
    for d in season_days(day, cal):
        s = city.clock.info(d)["season"]
        out[cal["seasons"][s]] = dict(
            day=dict(season=cal["seasons"][s], sunrise=cal["sunrise"][s], sunset=cal["sunset"][s], look_day=d,
                     weather=[citysim.weather(city.d, city.clock, d * 1440 + h * 60) for h in (3, 9, 15, 21)]),
            trees=tree_rows(w, city, d), flowers=flower_row(w, city, d), plants=world.plants_today(w, city, d))
    return out


def wear(w, city, day):
    """CITY-077, CITY-080: what each resident wears and carries today, and what they would on a wet day (for the weather key, K).
    Per resident: [coat colour or -1, hood 0/1, umbrella colour or -1]; colours are indexes into `colours` (people.js names)."""
    R = w["weather_react"]
    cols = sorted(set(R["coat"]["colours"]) | set(R["umbrella"]["colours"]))
    def row(x):
        return [cols.index(x["coat_colour"]) if x["coat"] else -1, int(x["hood"]), cols.index(x["umbrella_colour"]) if x["umbrella"] else -1]
    return dict(colours=cols, today=[row(city.wear(r["id"], day)) for r in w["residents"]], wet=[row(city.wear(r["id"], day, True)) for r in w["residents"]],
                snow=[row(city.wear(r["id"], day, False, True)) for r in w["residents"]],   # CITY-107: on a day of snow (the weather tried)
                pace={k: v for k, v in R["pace"].items() if not k.endswith("note")})


def main(day=16):
    w = world.load()
    problems = world.validate(w)
    if problems:
        sys.exit("The city data has problems:\n  " + "\n  ".join(problems[:20]))
    city = world.simulation(w, traffic=True)            # residents who own a car drive (LIVING_CITY 3.4)
    info = city.clock.info(day)
    cal = w["calendar"]["calendar"]
    city.plan(day)
    car_days, car_index, car_step = cars(city, day)
    plan, acts = routines(city, day, car_index)
    roles = sorted(w["roles"]["roles"])
    lot_at = {l["id"]: n for n, l in enumerate(city.d["lots"])}
    trees = tree_rows(w, city, day)
    ds = world.designs(w)                               # BUILD-001: every building is a design; the slice builds from it
    for b in w["blocks"]:
        for l in b["lots"]:
            if l["id"] in ds:
                l["design"] = world.built(w, ds[l["id"]])     # with its ids made colours, finishes and sizes (BUILD-016)
                l["dsg"] = ds[l["id"]]                         # and the design itself, which the remodel designer edits (BUILD-004)
    data = dict(
        city=w["city"], blocks=[{k: b[k] for k in ("id", "grid", "template", "district", "origin", "size", "lots", "paths", "props", "span", "lanes", "tracks") if k in b} for b in w["blocks"]],
        day=dict(index=day, weekday=info["weekday_name"], season=cal["seasons"][info["season"]], market=info["market"],
                 sunrise=cal["sunrise"][info["season"]], sunset=cal["sunset"][info["season"]],
                 weather=[citysim.weather(city.d, city.clock, day * 1440 + h * 60) for h in (3, 9, 15, 21)]),
        pace=city.pace, acts=acts, uses=USES, roles=roles,
        role_color={k: v["color"] for k, v in w["roles"]["roles"].items()},
        role_sig={k: v["sig"] for k, v in w["roles"]["roles"].items()}, signatures=w["signatures"]["signatures"],
        residents=[dict(n=r["name"], r=roles.index(r["role"]), id=r["id"], h=lot_at[r["home"]], w=lot_at[r["work"]] if r["work"] else -1,
                        **({"c": w["careers"]["careers"][r["career"]]["title"] + (" and " + w["careers"]["careers"][r["day_career"]]["title"] if r.get("day_career") else "")} if r.get("career") else {}),
                        **({"u": w["careers"]["careers"][r["career"]]["wear"]} if r.get("career") and w["careers"]["careers"][r["career"]].get("wear") else {})) for r in w["residents"]],   # CITY-081; u: the job's own clothes (CITY-105)
        plan=plan, trees=trees, flowers=flower_row(w, city, day), plants=world.plants_today(w, city, day),   # CITY-017: the garden plants today
        lawn={k: v for k, v in w["vegetation"]["lawn"].items() if k != "note"},   # ART-001: how the grass looks
        tram=w["calendar"]["tram"], dialogue=dialogue(w), speech={k: v for k, v in w["speech"].items() if k != "note"}, crash={k: v for k, v in w["crash"].items() if k != "note"},
        sound={k: v for k, v in w["sound"].items() if k != "note"}, cars=car_days, car_step=car_step, car_top_kmh=w["traffic"].get("player_top_kmh", 50.0),
        roads=roads(city),                                # zebras, lay-bys and traffic lights (CITY-043, CITY-045)
        sky={k: v for k, v in w["sky"].items() if k != "note"},   # WLD-035, WLD-036: the sky and its clouds
        dodge={k: v for k, v in w["dodge"].items() if k != "note"},   # CITY-070, CITY-071: getting out of the way of cars
        boards={k: v for k, v in w["boards"].items() if k != "note"},   # CITY-098 to CITY-101: skateboards to carry and ride
        surfaces=dict(world.surfaces(w), table=w["surfaces"]["patterns"]),   # WLD-039: which stone, brick and paving where
        storeys={k: v for k, v in w["storeys"].items() if k != "note"},   # WLD-050: floor heights, stairs and lifts
        house_styles={k: v for k, v in rules_file("house_styles").items() if k != "note"},   # ADR-0032: the house lines' colourways
        home_plans={k: v for k, v in rules_file("home_plans")["plans"].items()},   # FURN-060: the plans homes are furnished from
        dressing=world.dressing_today(w, day),          # WLD-034: what the boards show today, bunting on market days
        wear=wear(w, city, day),                        # CITY-077, CITY-080: coats, hoods and umbrellas
        life=dict(life(w, city, plan, acts), builder=builder_data(w, ds),   # money, shops, quests, the designers (ADR-0012, BUILD-004)
                  settings={k: v for k, v in w["settings"].items() if k != "note"},   # the game menu and its settings (UI-005 to UI-010)
                  buy_menu=dict(rules={k: v for k, v in w["buy_menu"].items() if k != "note"}, checks=world.buy_menu_checks(w))))   # how a buy menu shows its goods (UI-020, UI-021)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("// Generated by Tools/worldgen/build_play.py from World/. Do not edit: change the data files and run it again.\n")
        f.write("window.WASTED = " + json.dumps(data, separators=(",", ":"), ensure_ascii=False) + ";\n")
    # the Unreal build reads the same day's routines (Game/Source/BrixWorld BrixDay): the pieces as above, the acts by name,
    # each resident's id, name, role and a job's own clothes (u), each role's walk
    os.makedirs(os.path.dirname(OUT_UNREAL), exist_ok=True)
    with open(OUT_UNREAL, "w", encoding="utf-8", newline="\n") as f:
        json.dump(dict(note="Generated by Tools/worldgen/build_play.py from World/. Do not edit: change the data files and run it again.",
                       day=day, season=data["day"]["season"], weekday=data["day"]["weekday"], acts=acts, roles=roles,
                       role_sig=data["role_sig"],   # each role's walk (people.js look(), ART-004)
                       weather=data["day"]["weather"], wear=data["wear"],   # the day's weather by six-hour slot and what each resident wears (WLD-037, CITY-077, CITY-080)
                       residents=[dict(id=r["id"], n=r["n"], r=r["r"], **({"u": r["u"]} if r.get("u") else {}), **({"c": r["c"]} if r.get("c") else {})) for r in data["residents"]], plan=plan),   # c: their job, for the conversation box (CITY-081)
                  f, separators=(",", ":"), ensure_ascii=False)
    # the day's vehicles for the Unreal build (Game/Source/BrixWorld BrixCars): the same trips as D.cars, the bus lines'
    # colours, and the Soft box car meshes as Tools/blender/car_export_11.py wrote them for the slice (ADR-0015)
    with open(OUT_UNREAL_CARS, "w", encoding="utf-8", newline="\n") as f:
        json.dump(dict(note="Generated by Tools/worldgen/build_play.py from World/. Do not edit: change the data files and run it again.",
                       day=day, step=car_step, bus_layover_min=city.traffic.r["buses"]["layover_min"], player_top_kmh=data["car_top_kmh"],
                       lines=[ln["colour"] for ln in data["roads"]["lines"]], cars=car_days),
                  f, separators=(",", ":"), ensure_ascii=False)
    # the gardens and the street dressing as they look today, for the Unreal build (Game/Source/BrixWorld BrixStreets): the
    # same D.plants, D.flowers, D.lawn and D.dressing as the slice's city_data.js (CITY-017, WLD-040, ART-001, WLD-034)
    with open(OUT_UNREAL_STREETS, "w", encoding="utf-8", newline="\n") as f:
        json.dump(dict(note="Generated by Tools/worldgen/build_play.py from World/. Do not edit: change the data files and run it again.",
                       day=day, plants=data["plants"], flowers=data["flowers"], lawn=data["lawn"], dressing=data["dressing"],
                       trees=data["trees"], pace=data["pace"],   # every tree today (WLD-056) and the clock's pace, for the signals
                       roads={k: v for k, v in data["roads"].items() if k not in ("gates", "links")}),   # zebras, bays, bus stops, signals (CITY-043, CITY-045, CITY-091)
                  f, separators=(",", ":"), ensure_ascii=False)
    with open(OUT_UNREAL_LIFE, "w", encoding="utf-8", newline="\n") as f:   # money, shops and quests for the Unreal build
        json.dump(unreal_life(w, city, data["life"]), f, separators=(",", ":"), ensure_ascii=False)
    # what is inside every building for the Unreal build (Game/Source/BrixWorld BrixInside; CITY-013, WLD-050 to WLD-053):
    # per lot in the slice's order (LOTS), the design's inside (each storey's floor finish and wall colour, BUILD-019), its
    # door's width, and the block file's stair, lift, storeys, levels, flats and planned rooms; open ground is null
    INSIDE_KEYS = ("storeys", "stair", "lift", "tower", "levels", "flats", "common", "interior", "interior_frame", "furniture",
                   "residents", "seats", "staff", "desks", "customers", "chairs")
    # FURN-063: each home's daily wages, as the slice's furnishing.js earlyLevel() sums them (its residents' roles' wages)
    lots_flat = [l for b in data["blocks"] for l in b["lots"]]
    town_wages, wage_of = w["estate"]["town"]["wages"], {}
    wage_by_id = {}
    for r in data["residents"]:
        wage_of[r["h"]] = wage_of.get(r["h"], 0) + town_wages.get(roles[r["r"]], 0)
        wage_by_id[r["id"]] = town_wages.get(roles[r["r"]], 0)
    def flats_of(l):                                    # each flat with its household's daily wages (FURN-063, by flat)
        return [dict(q, wage=sum(wage_by_id.get(i, 0) for i in q.get("household", []))) for q in l["flats"]]
    def inside_of(k, l):
        if "design" not in l:
            return None
        Dz = l["design"]
        return dict(id=l["id"], dsg=l.get("dsg"), special=Dz.get("special"), accent=Dz["openings"]["awning"], door_width=Dz["shell"]["door_width"], floors=Dz["inside"]["floors"], walls=Dz["inside"]["walls"],
                    objects=l.get("objects", {}), wage=wage_of.get(k, 0), **({"house_line": l["house"]["furniture"]} if "house" in l else {}),
                    **{k2: l[k2] for k2 in INSIDE_KEYS if k2 in l and k2 != "flats"}, **({"flats": flats_of(l)} if "flats" in l else {}))
    with open(OUT_UNREAL_INSIDE, "w", encoding="utf-8", newline="\n") as f:
        json.dump(dict(note="Generated by Tools/worldgen/build_play.py from World/. Do not edit: change the data files and run it again.",
                       storeys=data["storeys"], home_plans=data["home_plans"], furnishing=estate_rules(w)["furnishing"],
                       builder=data["life"]["builder"],   # the building designer's kit, planning rules and fixed run (BUILD-002 to BUILD-023)
                       sizes=js_object(os.path.join(os.path.dirname(OUT), "furniture.js"), "SIZE"),        # the pieces' sizes (furniture.js)
                       hung=js_object(os.path.join(os.path.dirname(OUT), "homes.js"), "CAM_ONLY"),         # pieces on a wall (homes.js)
                       lots=[inside_of(k, l) for k, l in enumerate(lots_flat)]),
                  f, separators=(",", ":"), ensure_ascii=False)
    if os.path.exists(CAR_MODELS_JS):
        js = open(CAR_MODELS_JS, encoding="utf-8").read()
        models = json.loads(js[js.index("{"):js.rindex("}") + 1])
        with open(OUT_UNREAL_CAR_MODELS, "w", encoding="utf-8", newline="\n") as f:
            json.dump(models, f, separators=(",", ":"), ensure_ascii=False)
    with open(OUT_SEASONS, "w", encoding="utf-8") as f:   # the look of each season, for the slice's season key (Y)
        f.write("// Generated by Tools/worldgen/build_play.py from World/. Do not edit: change the data files and run it again.\n")
        f.write("window.WASTED_SEASONS = " + json.dumps(season_looks(w, city, day), separators=(",", ":"), ensure_ascii=False) + ";\n")
    print("wrote %s (%.1f MB): %d blocks, %d residents, %d trees, %d cars with %d trips, day %d (%s, %s)" % (
        os.path.relpath(OUT, os.path.join(HERE, "..", "..")), os.path.getsize(OUT) / 1e6, len(data["blocks"]), len(plan), len(trees),
        len(car_days), sum(len(c["trips"]) for c in car_days), day, info["weekday_name"], data["day"]["season"]))


if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 16)
