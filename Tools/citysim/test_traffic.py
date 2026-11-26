"""Residents' cars on the roads of the starting city (LIVING_CITY.md 3.4, CITY-043 to CITY-049).
Runs over World/ through Tools/worldgen/world.py.        python -m unittest test_traffic -v"""
import copy, os, random, sys, unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import world                                               # noqa: E402
import traffic                                             # noqa: E402
from citysim import dist, hm                               # noqa: E402

W = world.load()
CITY = world.simulation(W, traffic=True)
T = CITY.traffic
DAY = 16
PLAN = CITY.plan(DAY)
D = T._days[DAY]
R = T.roads
SP = W["traffic"]["spacing"]
IDS = T.ids(DAY)                                           # residents' cars, vans, and today's cars passing through


def samples(step=None):
    """(real second of the day, {car: car_at}) at every planning step while any car is on the road."""
    step = step or T.step
    spans = [(tr["t0"], tr["t1"]) for trips in D["trips"].values() for tr in trips]
    times = sorted({round(t0 + k * step, 3) for t0, t1 in spans for k in range(int((t1 - t0) / step) + 1)})
    for t in times:
        yield t, {cid: T.car_at(cid, DAY, t / T.pace) for cid in IDS}


class TheRoads(unittest.TestCase):
    def test_road_network_is_sound(self):                  # CITY-043
        self.assertEqual(R.problems(world.lots(W)), [])
        self.assertEqual(world.validate(W), [])
        broken = copy.deepcopy(W["roads"])
        broken["streets"][0]["b"] = "j_99_99"
        self.assertTrue(any("does not exist" in p for p in traffic.Roads(broken, W["traffic"]).problems(world.lots(W))))
        broken = copy.deepcopy(W["roads"])
        broken["crossings"] = broken["crossings"][1:]
        self.assertTrue(any("no crossing" in p for p in traffic.Roads(broken, W["traffic"]).problems(world.lots(W))))

    def test_every_door_has_a_bay_within_reach(self):     # CITY-043
        for l in world.lots(W):
            if l["kind"] not in world.OPEN_KINDS:
                self.assertTrue(R.bays_near(l["door"], W["traffic"]["bay_reach_m"]), l["id"])


class TheCars(unittest.TestCase):
    def test_CITY_089_cars_by_kind_of_home(self):        # CITY-044, CITY-089: houses have a car, rows about half, flats few
        no = set(W["traffic"]["no_drive_roles"])
        owners = {c["home"] for c in T.cars.values()}
        for kind, share in W["traffic"]["car_share"].items():
            homes = [h for h in CITY.mates if CITY.home_kind.get(h, "flat") == kind and any(CITY.res[r]["role"] not in no for r in CITY.mates[h])]
            if len(homes) < 20:
                continue
            self.assertAlmostEqual(sum(h in owners for h in homes) / len(homes), share, delta=0.1, msg=kind)
        for c in T.cars.values():
            self.assertNotIn(CITY.res[c["owner"]]["role"], W["traffic"]["no_drive_roles"])

    def test_some_residents_drive_today(self):            # CITY-044
        self.assertGreaterEqual(sum(1 for trips in D["trips"].values() if trips), 5)

    def test_drivers_park_and_walk_to_the_door(self):     # CITY-044
        driven = 0
        for cid, car in T.cars.items():
            pieces = PLAN[car["owner"]]
            for i, (kind, t0, t1, payload, s) in enumerate(pieces):
                if kind != "drive":
                    continue
                driven += 1
                before, after = pieces[i - 1], pieces[i + 1]
                self.assertIn(before[0], ("move", "away"))  # walked to the car, or comes back into town in it ...
                self.assertIn(after[0], ("move", "away"))   # ... and walks from the bay to the door, or is out of town
                if after[0] == "away":
                    continue
                self.assertLess(dist(after[3][0][0], T.car_at(cid, DAY, t1)["pos"]), 0.01)
                self.assertEqual(after[3][0][-1], tuple(after[3][0][-1]))
                mid = (t0 + t1) / 2 + 0.013                  # off the arrival instant, where float day sums may land either side
                m = (DAY * 1440 + mid) - DAY * 1440           # the very minute City.at will look at
                self.assertLess(dist(CITY.at(DAY * 1440 + mid)[car["owner"]]["pos"], T.car_at(cid, DAY, m)["pos"]), 1e-6)
        self.assertGreater(driven, 0)

    def test_short_tours_are_walked(self):                # CITY-044
        walking = world.simulation(W).plan(DAY)             # the same day with no cars
        pooled = set(D["pooled"])                           # their car stays at home: they ride with a neighbour (CITY-094)
        for cid, car in T.cars.items():
            if cid in pooled or cid in D["walked"]:
                continue
            rid = car["owner"]
            legs = T.legs(rid, walking[rid])
            drives = sum(p[0] == "drive" for p in PLAN[rid])
            self.assertEqual(drives, len(legs), rid)
            for lg in legs:                                # a driven tour always has a trip longer than the threshold
                self.assertTrue(lg["walk_len"] > 0)
            longest = max([p[3][1][-1] for p in walking[rid] if p[0] == "move"] or [0])
            if longest < W["traffic"]["drive_min_m"] and not any(p[0] == "away" for p in walking[rid]):
                self.assertEqual(drives, 0, rid)

    def test_drivers_days_have_no_gaps_and_end_at_home(self):
        for car in T.cars.values():
            pieces = PLAN[car["owner"]]
            for a, b in zip(pieces, pieces[1:]):
                self.assertAlmostEqual(a[2], b[1], places=6)
            self.assertAlmostEqual(pieces[-1][2], 1440.0)
            if pieces[-1][0] != "move":                  # one on foot at midnight is still on the way (their car is home)
                self.assertEqual(pieces[-1][4]["place"], car["home"])

    def test_nobody_walks_faster_than_their_own_pace_with_cars(self):
        for rid, pieces in PLAN.items():
            v = CITY.speed(rid) * CITY.pace
            for kind, t0, t1, payload, s in pieces:
                if kind == "move":
                    self.assertLessEqual(payload[2], v * 1.01, rid)

    def test_cars_are_home_at_night(self):
        for t in (3 * 60, 23.9 * 60):
            for cid, c in T.at(DAY * 1440 + t).items():
                if T.home_bay(cid) is None:                # passing through: gone by night
                    self.assertEqual(c["state"], "away", cid)
                    continue
                self.assertEqual(c["state"], "parked")
                self.assertEqual(c["bay"], T.home_bay(cid), cid)


    def test_commuters_with_a_car_drive_out_and_back(self):   # CITY-068
        walking = world.simulation(W).plan(DAY)
        out_of_town = 0
        for cid, car in T.cars.items():
            rid = car["owner"]
            tram = [p for p in walking[rid] if p[0] == "away"]
            if not tram:
                continue
            trips = D["trips"][cid]
            outs = [tr for tr in trips if tr["kind"] == "out"]
            if not outs:
                continue                                    # e.g. a stroll on the same outing keeps them on foot and on the tram
            out_of_town += 1
            tr_out = outs[0]
            tr_in = next(tr for tr in trips if tr["kind"] == "in")
            g = R.gates[tr_out["gate"]]
            self.assertIn(tr_out["legs"][-1]["key"], R.inn[g["id"]])       # leaves at a gate (from town or from outside) ...
            self.assertIn(tr_in["legs"][0]["key"], R.out[g["id"]])        # ... and comes back at the same one
            self.assertLessEqual(tr_out["t1"] / T.pace, tram[0][1] + 0.5)   # through the gate by the time the tram would have gone
            self.assertGreaterEqual(tr_in["t0"] / T.pace, tram[0][2] - 1e-6)   # not back before the tram would have brought them
            mid = (tr_out["t1"] + tr_in["t0"]) / 2 / T.pace
            self.assertEqual(T.car_at(cid, DAY, mid)["state"], "away")
            self.assertEqual(CITY.at(DAY * 1440 + mid)[rid]["state"], "away")
            self.assertFalse(any(p[0] == "away" and p[4]["act"] == "wait" for p in PLAN[rid]))
        self.assertGreater(out_of_town, 5)
        no_car = [r["id"] for r in W["residents"] if r["role"] == "commuter" and r["id"] not in {c["owner"] for c in T.cars.values()}]
        self.assertTrue(any(p[0] == "away" for rid in no_car[:20] for p in PLAN[rid]))   # the rest still take the tram (CITY-041)


class VansAndThroughTraffic(unittest.TestCase):
    def test_vans_make_their_rounds(self):                # CITY-040
        v = W["traffic"]["vans"]
        self.assertEqual(len(T.vans), v["count"])
        for vid, van in T.vans.items():
            trips = D["trips"][vid]
            self.assertEqual(len(trips), v["rounds"] * (v["stops"] + 1), vid)
            self.assertTrue(all(tr["t1"] is not None for tr in trips))
            self.assertEqual(trips[-1]["bay1"], van["bay"])                  # back at the depot at the end of the day
            self.assertGreaterEqual(trips[0]["t0"] / T.pace, hm(v["first"]) - 1e-6)
            for a, b in zip(trips, trips[1:]):                             # it stops a while at each place
                self.assertGreaterEqual((b["t0"] - a["t1"]) / T.pace, v["dwell_min"] - 1e-6)
        self.assertEqual(T.car_at(sorted(T.vans)[0], DAY, 3 * 60)["state"], "parked")

    def test_through_traffic_crosses_town(self):          # CITY-069
        per_hour = W["traffic"]["through"]["per_hour"]
        passing = {cid: trips for cid, trips in D["trips"].items() if cid.startswith("thru_")}
        self.assertEqual(len(passing), sum(per_hour))
        for cid, trips in passing.items():
            self.assertEqual(len(trips), 1)
            tr = trips[0]
            self.assertNotEqual(tr["gate_in"], tr["gate"])
            self.assertEqual(tr["legs"][0]["key"], R.gates[tr["gate_in"]]["back"])
            self.assertEqual(tr["legs"][-1]["key"], R.gates[tr["gate"]]["out"])
            h = int(cid.split("_")[1])
            self.assertTrue(h * 60 <= tr["t0"] / T.pace < (h + 1) * 60 + 5)   # comes in during its hour (it may wait a moment at the gate)
            for m in (tr["t0"] / T.pace - 1, tr["t1"] / T.pace + 1):
                self.assertEqual(T.car_at(cid, DAY, m)["state"], "away")


class TheTraffic(unittest.TestCase):
    def test_traffic_same_answer_every_time(self):        # CITY-045
        other = world.simulation(world.load(), traffic=True).traffic
        busy = sorted({tr["t0"] / T.pace + 0.4 for trips in D["trips"].values() for tr in trips})
        times = random.Random(3).sample(busy, min(200, len(busy))) + [0.0, 600.0, 1439.0]
        random.Random(7).shuffle(times)
        for m in times:                                     # asked in a different order, by a different instance
            for cid in IDS:
                self.assertEqual(T.car_at(cid, DAY, m), other.car_at(cid, DAY, m))

    def test_cars_never_overlap_in_a_lane(self):          # CITY-046
        for t, cars in samples():
            by_leg = {}
            for cid, c in cars.items():
                if c["leg"] is not None:
                    by_leg.setdefault(c["leg"][0], []).append(c["leg"][1])
            for key, ss in by_leg.items():
                ss.sort()
                for a, b in zip(ss, ss[1:]):
                    self.assertGreaterEqual(b - a, SP - 0.01, (t, key, ss))

    def test_cars_keep_to_the_speed_limit(self):          # CITY-046
        for trips in D["trips"].values():
            for tr in trips:
                for k, (a, b) in enumerate(zip(tr["d"], tr["d"][1:])):
                    i = max(0, min(len(tr["legs"]) - 1, sum(1 for s in tr["starts"] if s <= a) - 1))
                    v = max(g["v"] for g in tr["legs"][i:i + 2])
                    self.assertLessEqual(b - a, v * T.step + 1e-6)

    def test_cars_give_way_at_junctions(self):            # CITY-046
        boxes = D["boxes"]
        for e in D["entries"]:
            if e["control"] == "signals":
                self.assertEqual(e["signal"], "green", e)
        for i, a in enumerate(boxes):                       # never two crossing movements in one box at once
            for b in boxes[i + 1:]:
                if a["junction"] != b["junction"] or a["key"] == b["key"]:
                    continue
                if a["straight"] and b["straight"] and a["axis"] == b["axis"]:
                    continue
                overlap = min(a["t1"], b["t1"]) - max(a["t0"], b["t0"])
                self.assertLessEqual(overlap, 1e-6, (a, b))
        main_in = {}                                        # on a priority junction the side road waits for the main road
        for e in D["entries"]:
            if e["control"] == "priority" and e["axis"] != e["main"]:
                for b in boxes:
                    if b["junction"] == e["junction"] and b["axis"] == e["main"] and b["t0"] < e["t"] < b["t1"]:
                        self.fail("side-road car entered while a main-road car was in the box: %s" % e)

    def test_cars_choose_the_quickest_route(self):        # CITY-046
        for trips in D["trips"].values():
            for tr in trips:
                (l0, s0), (l1, s1) = tr["start"], tr["end"]
                best = R.route(l0, s0, l1, s1)
                self.assertEqual([g["key"] for g in tr["legs"]], [g["key"] for g in best])

    def test_cars_wait_for_walkers_at_crossings(self):    # CITY-046, CITY-097
        crossers = D["crossers"]
        self.assertGreater(sum(len(v) for v in crossers.values()), 100)          # people do cross at the zebras
        patience = W["traffic"]["crossing_patience_s"]
        for e in D["entries"]:
            if e["control"] == "signals" or e["waited"] >= patience - 1e-6:
                continue                                    # on green at the lights, or let through after waiting a while
            jid, a, b = e["key"]
            for lane in (a, b):
                key = (R.lanes[lane]["street"], jid)
                for t0, t1 in crossers.get(key, []):
                    self.assertFalse(t0 < e["t"] + .1 and t1 > e["t"], (e, key, t0, t1))

    def test_people_walk_on_pavements_and_cross_at_zebras(self):   # CITY-043
        c, P = W["traffic"]["crossing_at"], W["city"]["grid"]["pitch"]
        half = W["city"]["grid"]["street_width"] / 2 - 4.5               # the carriageway either side of a centre line
        doors = {tuple(l["door"]) for l in world.lots(W) if l["kind"] == "stop"}   # the tram boards in the street
        wl = (W["city"].get("wall") or {}).get("cells")
        def inside(x, z):                                   # pavements and zebras are the walled town's (world.walkways()):
            return not wl or (wl[0] * P < x < wl[2] * P and wl[1] * P < z < wl[3] * P)   # outside, people walk the gate roads
        def on_road(v):
            return abs(v - round(v / P) * P) < half - .01
        def at_zebra(v):
            return abs(abs(v - round(v / P) * P) - c) < .01
        checked = 0
        for rid, pieces in list(PLAN.items())[:300]:
            for kind, t0, t1, payload, s in pieces:
                if kind != "move":
                    continue
                for (x, z) in payload[0]:
                    if any(abs(x - d[0]) < 2 and abs(z - d[1]) < 2 for d in doors) or not inside(x, z):
                        continue
                    checked += 1
                    if on_road(x):                                  # in a north-south carriageway: only on a zebra
                        self.assertTrue(at_zebra(z), (rid, x, z))
                    if on_road(z):
                        self.assertTrue(at_zebra(x), (rid, x, z))
        self.assertGreater(checked, 1000)

    def test_one_car_per_bay(self):                       # CITY-047
        held = {}                                          # bay -> [(from, to, car)]
        for cid, trips in D["trips"].items():
            t, bay = 0.0, T.home_bay(cid)
            for tr in trips:
                if bay is not None:                        # None: out of town, in no bay
                    held.setdefault(bay, []).append((t, tr["t0"], cid))
                t, bay = tr["t1"], tr["bay1"]
            if bay is not None:
                held.setdefault(bay, []).append((t, 1440 * T.pace, cid))
        for bay, spans in held.items():
            if bay in R.stops:                             # a bus stop: buses only, and two of a line may wait there (CITY-091)
                continue
            spans.sort()
            for a, b in zip(spans, spans[1:]):
                self.assertLessEqual(a[1], b[0] + 1e-6, (bay, a, b))
        for a in D["assigned"]:                            # each driver took the nearest free bay
            self.assertEqual(a["nearer_free"], [], a)

    def test_cars_load_by_distance(self):                 # CITY-048
        t = max(D["trips"].values(), key=len)[0]["t0"] / T.pace + 0.5 + DAY * 1440
        cars = T.at(t)
        spot = cars[max(D["trips"], key=lambda c: len(D["trips"][c]))]["pos"]
        near = T.loaded(cars, [spot])
        self.assertIn(max(D["trips"], key=lambda c: len(D["trips"][c])), near)
        self.assertTrue(0 < len(near) < len(cars))
        for cid in near:
            self.assertLessEqual(dist(cars[cid]["pos"], spot), W["traffic"]["car_load_m"])


class TheOutskirtsAndBuses(unittest.TestCase):
    def test_CITY_090_houses_outside_the_wall_drive_their_lanes(self):
        house_cars = [c for c in T.cars.values() if CITY.home_kind.get(c["home"]) == "house"]
        self.assertGreater(len(house_cars), 300)
        drive = sum(R.bays[c["bay"]].get("kind") == "driveway" and R.bays[c["bay"]]["lot"] == c["home"] for c in house_cars)
        self.assertGreater(drive / len(house_cars), 0.9)                 # parked on their own driveway
        outer = sum(1 for c in house_cars for tr in D["trips"][c["id"]] for g in tr["legs"] if g["kind"] == "lane" and R.lanes[g["key"]].get("outer"))
        self.assertGreater(outer, 1000)                                  # and they drive the lanes and links
        through = sum(1 for c in house_cars for tr in D["trips"][c["id"]] if any(g["kind"] == "box" and g["junction"] in R.gates for g in tr["legs"]))
        self.assertGreater(through, 200)                                 # into town through the gates
        for jid, j in R.j.items():                                       # lanes outside meet with give way
            if j.get("outer"):
                self.assertEqual(j["control"], "give_way")

    def test_CITY_091_buses_keep_to_their_lines(self):
        b = W["traffic"]["buses"]
        self.assertEqual(len(T.lines), len(W["transit"]["lines"]))
        for vid, bus in T.buses.items():
            L, legs, trips = T.lines[bus["line"]], T._bus_legs(vid), D["trips"][vid]
            self.assertEqual(len(trips), len(legs), vid)                  # every run of the day done
            for lg, tr in zip(legs, trips):
                self.assertEqual(tr["bay1"], lg["bay"])                  # stop after stop, in the line's order
                self.assertGreaterEqual(tr["t0"] / T.pace, lg["at"] - 1e-6)   # never away before its time
            for a, c in zip(trips, trips[1:]):                           # and it waits its dwell at every stop
                self.assertGreaterEqual(c["t0"] - a["t1"], b["dwell_s"] - 1e-6)
            self.assertEqual(T.car_at(vid, DAY, 3 * 60)["state"], "away")  # not in service at night
        for bid in R.stops:                                              # no car parks at a stop
            self.assertNotIn(bid, {c["bay"] for c in T.cars.values()})
            self.assertNotIn(bid, R.parking_near(R.stops[bid]["pos"], 5))

    def test_CITY_091_bus_stops_keep_clear_of_street_things(self):
        b = W["traffic"]["buses"]
        size = b["obstacle_m"]
        things = [(p["pos"], size.get(p["kind"], size["default"])) for bl in W["blocks"] for p in bl["props"]
                  if "pos" in p and p["kind"] not in size["ignore"]] + [(t["pos"], size["tree"]) for bl in W["blocks"] for t in bl["trees"]]
        for bid, st in R.stops.items():
            u, n = st["u"], (-st["u"][1], st["u"][0])
            sh = b["shelter_out_m"][1 if R.is_outer_bay(bid) else 0]
            for (x, z), rad in things:
                dx, dz = x - st["pos"][0], z - st["pos"][1]
                a, c = dx * u[0] + dz * u[1], dx * n[0] + dz * n[1]
                self.assertFalse(abs(a) < b["length"] / 2 + rad and abs(c) < 1.2 + rad, (bid, "bus", x, z))   # no lamp or rack under the bus
                self.assertFalse(-2.2 - rad < a < 2.9 + rad and sh - .8 - rad < c < sh + .8 + rad, (bid, "shelter", x, z))

    def test_CITY_092_residents_without_a_car_ride_the_bus(self):
        rides, b = D["rides"], W["traffic"]["buses"]
        self.assertGreater(len(rides), 1500)                             # long walks are ridden, not walked
        owners = {c["owner"] for c in T.cars.values() if D["trips"][c["id"]]}   # at a wheel today
        visits = T.bus_visits(D["trips"])
        for r in rides:
            self.assertNotIn(r["rid"], owners)
            self.assertLessEqual(r["there"], r["walk_t"][1] + b["late_min"] + 1e-6)   # their day moves to the bus, a little
            self.assertGreaterEqual(r["go"], r["walk_t"][0] - b["shift_min"] - 1e-6)
            v = visits[r["bus"]]
            self.assertTrue(any(bb == r["board"] and lv is not None and abs(lv / T.pace - r["t0"]) < 1e-6 for bb, _, lv in v))   # it leaves that stop then
            self.assertTrue(any(bb == r["alight"] and ar is not None and abs(ar / T.pace - r["t1"]) < 1e-6 for bb, ar, _ in v))
        for r in rides[:60]:                                             # on the bus they are where the bus is
            m = (r["t0"] + r["t1"]) / 2
            self.assertLess(dist(CITY.at(DAY * 1440 + m)[r["rid"]]["pos"], T.car_at(r["bus"], DAY, m)["pos"]), 1e-6)
        for rid in {r["rid"] for r in rides}:                            # their days stay whole
            for a, c in zip(PLAN[rid], PLAN[rid][1:]):
                self.assertAlmostEqual(a[2], c[1], places=6)


    def test_CITY_093_most_cars_parked_away_from_home_leave_the_street(self):
        pa = W["traffic"]["park_away"]
        away = stowed = 0
        for cid, car in T.cars.items():
            trips = D["trips"][cid]
            for i, tr in enumerate(trips[:-1]):
                if tr["bay1"] in (None, car["bay"]):
                    continue
                away += 1
                st = T.stowed(cid, DAY, i)
                if not st:
                    continue
                stowed += 1
                mid = (st[0] + st[1]) / 2 / T.pace
                self.assertEqual(T.car_at(cid, DAY, mid)["state"], "stowed")
                self.assertEqual(T.car_at(cid, DAY, tr["t1"] / T.pace + 0.01)["state"], "parked")    # parks first, in sight
                self.assertEqual(T.car_at(cid, DAY, trips[i + 1]["t0"] / T.pace - 0.01)["state"], "parked")   # back before the driver
        self.assertGreater(away, 200)
        self.assertAlmostEqual(stowed / away, pa["share"], delta=0.08)
        for cid in T.owned(DAY)[:50]:                                    # never at home
            self.assertEqual(T.car_at(cid, DAY, 3 * 60)["state"], "parked")


    def test_CITY_094_neighbours_share_the_drive(self):
        pl, pools = W["traffic"]["pool"], D["pools"]
        self.assertGreater(len({x["car"] for x in pools}), 60)
        seats = {}
        for x in pools:
            p, q = T.cars[x["car"]], T.cars[x["driver"]]
            self.assertEqual(D["trips"][x["car"]], [])                      # the passenger's car stays at home all day
            self.assertNotEqual(D["trips"][x["driver"]], [])
            self.assertLessEqual(dist(CITY.lots[p["home"]]["door"], CITY.lots[q["home"]]["door"]), pl["reach_m"] + 1e-6)
            self.assertLessEqual(abs(x["go"] - x["walk_t"][0]), pl["shift_min"] + 1e-6)
            self.assertLessEqual(abs(x["there"] - x["walk_t"][1]), pl["shift_min"] + 1e-6)
            m = (x["board"] + x["off"]) / 2                                 # in the car with the driver
            here = CITY.at(DAY * 1440 + m)
            self.assertEqual(here[x["rid"]]["state"], "riding")
            self.assertLess(dist(here[x["rid"]]["pos"], here[q["owner"]]["pos"]), 1e-6)
            seats.setdefault((x["driver"], x["half"]), set()).add(x["car"])
        self.assertTrue(all(len(v) <= pl["seats"] for v in seats.values()))
        for rid in {x["rid"] for x in pools}:
            for a, c in zip(PLAN[rid], PLAN[rid][1:]):
                self.assertAlmostEqual(a[2], c[1], places=6)


    def test_CITY_095_cars_not_used_today_are_not_there(self):
        idle = set(D["idle"])
        self.assertGreater(len(idle), 100)
        for cid in idle:
            self.assertEqual(D["trips"][cid], [])
            self.assertEqual(T.car_at(cid, DAY, 600)["state"], "none")
            self.assertNotIn(cid, IDS)
            self.assertFalse(any(p[0] == "drive" for p in PLAN[T.cars[cid]["owner"]]))
        self.assertEqual(set(T.owned(DAY)), {c for c in T.cars if D["trips"][c]})
        for x in D["pools"]:                                             # a passenger's car is not there; they ride with the neighbour
            self.assertIn(x["car"], idle)


class NoCarIsStuck(unittest.TestCase):
    def test_CITY_097_no_car_is_held_at_a_junction_for_long(self):
        r = W["traffic"]
        cycle = 2 * (r["signals"]["green_s"] + r["signals"]["amber_s"] + r["signals"]["all_red_s"])
        worst = 0.0
        for v in D["trips"].values():
            for tr in v:
                run = 0
                for a, b in zip(tr["d"], tr["d"][1:]):
                    run = run + 1 if b - a < 1e-6 else 0
                    worst = max(worst, run * T.step)
        self.assertLessEqual(worst, cycle + r["crossing_patience_s"])      # a red light, or a zebra's wait, never more
        self.assertTrue(all(tr["t1"] is not None for v in D["trips"].values() for tr in v))
        for e in D["entries"]:
            if e["control"] == "signals":
                self.assertEqual(e["signal"], "green", e)                # and nobody goes on red to get there


    def test_CITY_097_walkers_cross_at_the_lights_on_red(self):
        rec = D["at_lights"]
        self.assertGreater(len(rec), 1000)
        def on_red(c):
            return T.light_wait(DAY, c["key"][1], "ns" if c["key"][0].startswith("s_ns_") else "ew", c["t"], c["need"]) < .05
        held = [c for c in rec if c["held"]]
        self.assertGreater(len(held), 1000)
        self.assertTrue(all(on_red(c) for c in held))                     # held for red, they cross on red
        self.assertGreater(sum(on_red(c) for c in rec) / len(rec), .9)    # and nearly everyone crosses on red
        for rid in {c["rid"] for c in held}:                              # their days stay whole
            for a, b in zip(PLAN[rid], PLAN[rid][1:]):
                self.assertAlmostEqual(a[2], b[1], places=6)


class TheBusyDay(unittest.TestCase):
    """Every household has a car and drives every trip: the same rules must still hold."""

    @classmethod
    def setUpClass(cls):
        w = copy.deepcopy(W)
        w["traffic"] = dict(w["traffic"], car_share=1.0, drive_min_m=0.0)
        cls.city = world.simulation(w, traffic=True)
        cls.T = cls.city.traffic
        cls.plan = cls.city.plan(DAY)
        cls.D = cls.T._days[DAY]

    def test_busy_day_every_trip_finishes_and_days_stay_whole(self):
        trips = [tr for v in self.D["trips"].values() for tr in v]
        self.assertGreater(len(trips), 500)
        self.assertTrue(all(tr["t1"] is not None for tr in trips))
        for pieces in self.plan.values():
            for a, b in zip(pieces, pieces[1:]):
                self.assertAlmostEqual(a[2], b[1], places=6)
            self.assertAlmostEqual(pieces[-1][2], 1440.0)

    def test_busy_day_cars_never_overlap(self):           # CITY-046
        T = self.T
        spans = [(tr["t0"], tr["t1"]) for v in self.D["trips"].values() for tr in v]
        times = sorted({round(t0 + k * T.step, 3) for t0, t1 in spans for k in range(int((t1 - t0) / T.step) + 1)})
        for t in times[::2]:
            by_leg = {}
            for cid in T.ids(DAY):
                c = T.car_at(cid, DAY, t / T.pace)
                if c["leg"] is not None:
                    by_leg.setdefault(c["leg"][0], []).append(c["leg"][1])
            for ss in by_leg.values():
                ss.sort()
                for a, b in zip(ss, ss[1:]):
                    self.assertGreaterEqual(b - a, SP - 0.01)

    def test_busy_day_boxes_and_signals(self):            # CITY-046
        by_j = {}
        for b in self.D["boxes"]:
            by_j.setdefault(b["junction"], []).append(b)
        for boxes in by_j.values():
            for i, a in enumerate(boxes):
                for b in boxes[i + 1:]:
                    if a["key"] == b["key"] or (a["straight"] and b["straight"] and a["axis"] == b["axis"]):
                        continue
                    self.assertLessEqual(min(a["t1"], b["t1"]) - max(a["t0"], b["t0"]), 1e-6)
        self.assertTrue(all(e["signal"] == "green" for e in self.D["entries"] if e["control"] == "signals"))
        self.assertEqual([a for a in self.D["assigned"] if a["nearer_free"]], [])


if __name__ == "__main__":
    unittest.main()
