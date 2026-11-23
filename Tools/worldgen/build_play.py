"""Packs the city data and one day of everyone's routine into Prototype/play/city_data.js for the playable slice.

The slice (Prototype/play/index.html) builds every street, building and interior from the block files as they
are, so a change to World/ shows up after re-running this script and reloading the page.

Run:  python build_play.py [day]        (default day 16: a summer market day with fair weather)
"""
import json, os, sys

import world
import citysim

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "Prototype", "play", "city_data.js"))
OUT_SEASONS = os.path.join(os.path.dirname(OUT), "city_seasons.js")
ACTS = ["sleep", "home", "work", "eat", "shop", "rest", "sell", "wait", "visit", "tend", "deliver", "collect", "stroll", "walk", "arrive", "eat_home", "check", "shelter"]
USES = ["bed", "room", "work", "seat", "customer", "bench", "stall", "wait", "chair", "flowerbed", "rim"]


def rules_file(name):                                    # a rules file world.load() does not read (yet)
    return json.load(open(os.path.join(HERE, "..", "..", "World", "rules", name + ".json"), encoding="utf-8"))


def r1(v):
    return round(v, 1)


def routines(city, day, car_index):
    """Each resident's day as pieces: [t0, 0, x, z, act, use, inside, lot] stays, [t0, 1, act, x, z, ...] walks, [t0, 2] away,
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
                inside = int(lot["kind"] not in world.OPEN_KINDS and s["use"] is not None and x0 <= x <= x1 and z0 <= z <= z1)
                act = "wait" if s["act"] in ("away", "arrive") else s["act"]
                use = uses.get(s["use"], -1) if s.get("slot") is not None else -1
                row.append([r1(t0), 0, r1(x), r1(z), acts.setdefault(act, len(acts)), use, inside, lot_index[s["place"]]])
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
    probe = ec.Economy(e, lots_, visits, staff)
    shops = {k: next(n for n in range(len(lots_)) if probe.shop_kind(n) == k) for k in ("grocer", "kiosk", "arcade")}
    script = ec.check_script()
    want = ec.run_script(ec.Economy(e, lots_, visits, staff), script, shops)
    return dict(economy={k: v for k, v in e.items() if k != "note"}, quests={k: v for k, v in w["quests"].items() if k != "note"},
                designer={k: v for k, v in w["designer"].items() if k != "note"},
                visits={str(k): [[t, i] for t, i in v] for k, v in visits.items()}, visit_acts=visit_acts,
                staff={str(k): v for k, v in staff.items()}, checks=dict(script=script, shops=shops, want=want),
                estate=estate_rules(w), estate_checks=estate_checks(w, lots_),
                walk_net=walk_net(city, lots_))


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
                pace={k: v for k, v in R["pace"].items() if k != "note"})


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
                        **({"c": w["careers"]["careers"][r["career"]]["title"] + (" and " + w["careers"]["careers"][r["day_career"]]["title"] if r.get("day_career") else "")} if r.get("career") else {})) for r in w["residents"]],   # CITY-081
        plan=plan, trees=trees, flowers=flower_row(w, city, day), plants=world.plants_today(w, city, day),   # CITY-017: the garden plants today
        lawn={k: v for k, v in w["vegetation"]["lawn"].items() if k != "note"},   # ART-001: how the grass looks
        tram=w["calendar"]["tram"], dialogue=dialogue(w), speech={k: v for k, v in w["speech"].items() if k != "note"}, crash={k: v for k, v in w["crash"].items() if k != "note"},
        sound={k: v for k, v in w["sound"].items() if k != "note"}, cars=car_days, car_step=car_step, car_top_kmh=w["traffic"].get("player_top_kmh", 50.0),
        roads=roads(city),                                # zebras, lay-bys and traffic lights (CITY-043, CITY-045)
        sky={k: v for k, v in w["sky"].items() if k != "note"},   # WLD-035, WLD-036: the sky and its clouds
        dodge={k: v for k, v in w["dodge"].items() if k != "note"},   # CITY-070, CITY-071: getting out of the way of cars
        surfaces=dict(world.surfaces(w), table=w["surfaces"]["patterns"]),   # WLD-039: which stone, brick and paving where
        storeys={k: v for k, v in w["storeys"].items() if k != "note"},   # WLD-050: floor heights, stairs and lifts
        house_styles={k: v for k, v in rules_file("house_styles").items() if k != "note"},   # ADR-0032: the house lines' colourways
        home_plans={k: v for k, v in rules_file("home_plans")["plans"].items()},   # FURN-060: the plans homes are furnished from
        dressing=world.dressing_today(w, day),          # WLD-034: what the boards show today, bunting on market days
        wear=wear(w, city, day),                        # CITY-077, CITY-080: coats, hoods and umbrellas
        life=dict(life(w, city, plan, acts), builder=builder_data(w, ds),   # money, shops, quests, the designers (ADR-0012, BUILD-004)
                  settings={k: v for k, v in w["settings"].items() if k != "note"}))   # the game menu and its settings (UI-005 to UI-010)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("// Generated by Tools/worldgen/build_play.py from World/. Do not edit: change the data files and run it again.\n")
        f.write("window.WASTED = " + json.dumps(data, separators=(",", ":"), ensure_ascii=False) + ";\n")
    with open(OUT_SEASONS, "w", encoding="utf-8") as f:   # the look of each season, for the slice's season key (Y)
        f.write("// Generated by Tools/worldgen/build_play.py from World/. Do not edit: change the data files and run it again.\n")
        f.write("window.WASTED_SEASONS = " + json.dumps(season_looks(w, city, day), separators=(",", ":"), ensure_ascii=False) + ";\n")
    print("wrote %s (%.1f MB): %d blocks, %d residents, %d trees, %d cars with %d trips, day %d (%s, %s)" % (
        os.path.relpath(OUT, os.path.join(HERE, "..", "..")), os.path.getsize(OUT) / 1e6, len(data["blocks"]), len(plan), len(trees),
        len(car_days), sum(len(c["trips"]) for c in car_days), day, info["weekday_name"], data["day"]["season"]))


if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 16)
