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
    def test_about_the_right_share_of_households_has_a_car(self):     # CITY-044
        homes = [h for h in CITY.mates]
        self.assertAlmostEqual(len(T.cars) / len(homes), W["traffic"]["car_share"], delta=0.06)
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
                mid = (t0 + t1) / 2
                self.assertLess(dist(CITY.at(DAY * 1440 + mid)[car["owner"]]["pos"], T.car_at(cid, DAY, mid)["pos"]), 1e-6)
        self.assertGreater(driven, 0)

    def test_short_tours_are_walked(self):                # CITY-044
        walking = world.simulation(W).plan(DAY)             # the same day with no cars
        for cid, car in T.cars.items():
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
            self.assertEqual(tr_out["legs"][-1]["key"], g["out"])          # leaves through a gate ...
            self.assertEqual(tr_in["legs"][0]["key"], g["back"])          # ... and comes back through the same one
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
                g = R.gates.get(tr["gate"]) if tr["gate"] else None
                b0 = R.bays[tr["bay0"]] if tr["bay0"] else dict(lane=R.gates[tr["gate_in"] or tr["gate"]]["back"], s=0.0)
                b1 = R.bays[tr["bay1"]] if tr["bay1"] else dict(lane=g["out"], s=R.lanes[g["out"]]["length"])
                best = R.route(b0["lane"], b0["s"], b1["lane"], b1["s"])
                self.assertEqual([g["key"] for g in tr["legs"]], [g["key"] for g in best])

    @unittest.skip("needs pavements: residents still walk the street centre lines (Q-24 decided, not built yet)")
    def test_cars_wait_for_walkers_at_crossings(self):    # CITY-046
        pass

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
