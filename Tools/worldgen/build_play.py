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
ACTS = ["sleep", "home", "work", "eat", "shop", "rest", "sell", "wait", "visit", "tend", "deliver", "stroll", "walk", "arrive", "eat_home", "check"]
USES = ["bed", "room", "work", "seat", "customer", "bench", "stall", "wait", "chair", "flowerbed", "rim"]


def r1(v):
    return round(v, 1)


def routines(city, day, car_index):
    """Each resident's day as pieces: [t0, 0, x, z, act, use, inside, lot] stays, [t0, 1, act, x, z, ...] walks, [t0, 2] away,
    [t0, 3, car] in their car (drawn as the car, see cars())."""
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
            elif kind == "drive":
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


def cars(city, day):
    """Every vehicle's day for the slice (k: 0 car, 1 van): its home bay (none for a car passing through) and its trips, each a list of [x, z, hx, hz] every CAR_STEP_S real
    seconds (Tools/citysim/traffic.py works them out). Pulling out of a bay and into one is eased over a few seconds here:
    the rules keep a car either in its bay or on its lane, the picture slides it between the two."""
    T = city.traffic
    R = T.roads
    D = T._days[day]

    def bay(bid):
        b = R.bays[bid]
        u = R.lanes[b["lane"]]["u"]
        return [r1(b["pos"][0]), r1(b["pos"][1]), round(u[0], 2), round(u[1], 2)]

    out, index = [], {}
    for n, cid in enumerate(T.ids(day)):                   # residents' cars, vans, and today's cars passing through
        index[cid] = n
        trips = []
        for tr in D["trips"][cid]:
            pts, t = [], tr["t0"]
            while t < tr["t1"] - 1e-6:
                c = T.car_at(cid, day, t / T.pace)
                pts.append([c["pos"][0], c["pos"][1], c["heading"][0], c["heading"][1]])
                t += CAR_STEP_S
            last = tr["legs"][-1]
            p, h = R.point(last, last["s1"])
            pts.append([p[0], p[1], h[0], h[1]])
            ease = 4                                       # samples to slide between bay and lane
            for end, bid in ((0, tr["bay0"]), (-1, tr["bay1"])):
                if bid is None:
                    continue
                bp = R.bays[bid]["pos"]
                k = len(pts)
                for j in range(min(ease, k)):
                    i = j if end == 0 else k - 1 - j
                    f = 1 - j / ease
                    f = f * f * (3 - 2 * f)
                    pts[i][0] += (bp[0] - pts[i][0]) * f
                    pts[i][1] += (bp[1] - pts[i][1]) * f
            trips.append([round(tr["t0"] / T.pace, 3), round(tr["t1"] / T.pace, 3), bay(tr["bay1"]) if tr["bay1"] else None,
                          [round(v, 2) for q in pts for v in q]])
        home = T.home_bay(cid)
        out.append(dict(home=bay(home) if home else None, trips=trips, k=1 if cid in T.vans else 0))
    return out, index, CAR_STEP_S / T.pace


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
                staff={str(k): v for k, v in staff.items()}, checks=dict(script=script, shops=shops, want=want))


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
    trees = []
    for b in w["blocks"]:
        for t in b["trees"]:
            v = city.veg.tree(t, day)
            if v["state"] == "empty":
                continue
            c = v.get("color", [0.3, 0.33, 0.2])
            trees.append([t["pos"][0], t["pos"][1], round(v["radius"], 2), round(v["leaf"], 2), round(c[0], 3), round(c[1], 3), round(c[2], 3),
                          int(v.get("blossom", 0)), int(v["state"] == "fallen")])
    fl = city.veg.flowers(day)
    data = dict(
        city=w["city"], blocks=[{k: b[k] for k in ("id", "grid", "template", "district", "origin", "size", "lots", "paths", "props")} for b in w["blocks"]],
        day=dict(index=day, weekday=info["weekday_name"], season=cal["seasons"][info["season"]], market=info["market"],
                 sunrise=cal["sunrise"][info["season"]], sunset=cal["sunset"][info["season"]],
                 weather=[citysim.weather(city.d, city.clock, day * 1440 + h * 60) for h in (3, 9, 15, 21)]),
        pace=city.pace, acts=acts, uses=USES, roles=roles,
        role_color={k: v["color"] for k, v in w["roles"]["roles"].items()},
        role_sig={k: v["sig"] for k, v in w["roles"]["roles"].items()}, signatures=w["signatures"]["signatures"],
        residents=[dict(n=r["name"], r=roles.index(r["role"]), id=r["id"], h=lot_at[r["home"]], w=lot_at[r["work"]] if r["work"] else -1) for r in w["residents"]],
        plan=plan, trees=trees, flowers=dict(size=round(fl["size"], 2), bloom=round(fl["bloom"], 2), colors=w["vegetation"]["flowers"]["colors"]),
        tram=w["calendar"]["tram"], dialogue=dialogue(w), speech={k: v for k, v in w["speech"].items() if k != "note"},
        sound={k: v for k, v in w["sound"].items() if k != "note"}, cars=car_days, car_step=car_step, car_top_kmh=w["traffic"].get("player_top_kmh", 50.0),
        sky={k: v for k, v in w["sky"].items() if k != "note"},   # WLD-035, WLD-036: the sky and its clouds
        dodge={k: v for k, v in w["dodge"].items() if k != "note"},   # CITY-070, CITY-071: getting out of the way of cars
        dressing=world.dressing_today(w, day),          # WLD-034: what the boards show today, bunting on market days
        life=life(w, city, plan, acts))                   # money, shops, quests, the designer (ADR-0012)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("// Generated by Tools/worldgen/build_play.py from World/. Do not edit: change the data files and run it again.\n")
        f.write("window.WASTED = " + json.dumps(data, separators=(",", ":"), ensure_ascii=False) + ";\n")
    print("wrote %s (%.1f MB): %d blocks, %d residents, %d trees, %d cars with %d trips, day %d (%s, %s)" % (
        os.path.relpath(OUT, os.path.join(HERE, "..", "..")), os.path.getsize(OUT) / 1e6, len(data["blocks"]), len(plan), len(trees),
        len(car_days), sum(len(c["trips"]) for c in car_days), day, info["weekday_name"], data["day"]["season"]))


if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 16)
