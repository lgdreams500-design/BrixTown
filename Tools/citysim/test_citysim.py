"""Tests for the city rules. Run:  python -m unittest test_citysim -v   (from Tools/citysim)"""
import unittest
import citysim as cs

C = cs.City()
SUMMER_WED = 16          # year 1, summer day 5, a Wednesday (market day)


class Routines(unittest.TestCase):
    def test_same_answer_every_time(self):
        """CITY-022: a resident's place at a given minute never depends on what was asked before."""
        t = SUMMER_WED * 1440 + 12 * 60 + 37
        a = cs.City().at(t)
        b = cs.City()
        for d in range(0, 12):
            b.at(d * 1440 + 600)                    # ask about other days first
        self.assertEqual(a, b.at(t))

    def test_nobody_teleports(self):
        """CITY-021: positions change smoothly; nobody jumps (boarding or leaving the tram excepted)."""
        worst = 0.0
        for day in (SUMMER_WED, SUMMER_WED + 3, SUMMER_WED + 4, 40):
            prev = C.at(day * 1440)
            for step in range(1, 24 * 60 * 6):
                now = C.at(day * 1440 + step / 6.0)
                for rid, s in now.items():
                    p = prev[rid]
                    if s["pos"] is None or p["pos"] is None:
                        continue
                    moved = cs.dist(s["pos"], p["pos"])
                    allowed = C.speed(rid) * 10.0 * 1.6 + 0.2
                    worst = max(worst, moved / allowed)
                    self.assertLessEqual(moved, allowed, "%s moved %.1f m in 10 s at %s on day %d (%s -> %s)" % (
                        rid, moved, "%02d:%02d" % divmod(step // 6, 60), day, p["act"], s["act"]))
                prev = now

    def test_one_person_per_place(self):
        """CITY-031: a bench seat, desk or bed is never used by two people at once."""
        for day in range(SUMMER_WED, SUMMER_WED + 7):
            for minute in range(0, 1440, 3):
                seen = {}
                for rid, s in C.at(day * 1440 + minute).items():
                    if s.get("slot") is None or s["state"] == "walking" or s["act"] == "wait":
                        continue
                    key = (s["place"], s["use"], s["slot"])
                    self.assertNotIn(key, seen, "%s and %s share %s at minute %d of day %d" % (rid, seen.get(key), key, minute, day))
                    seen[key] = rid

    def test_asleep_at_home_at_night(self):
        for day in range(0, 20):
            for rid, s in C.at(day * 1440 + 3 * 60).items():
                self.assertEqual((s["act"], s["place"]), ("sleep", C.res[rid]["home"]))

    def test_at_work_mid_morning(self):
        """CITY-021: on a working day people with a workplace are there, or walking to it."""
        s = C.at(SUMMER_WED * 1440 + 10 * 60 + 30)
        for rid, r in C.res.items():
            if r["role"] in ("clerk", "nurse", "cafe"):
                self.assertEqual(s[rid]["place"], r["work"], rid)
        self.assertEqual(sum(1 for x in s.values() if x["state"] == "away"), 6)       # commuters and students took the tram

    def test_weekend_differs(self):
        """CITY-023: rest days are not working days."""
        sun = C.at((SUMMER_WED + 4) * 1440 + 10 * 60 + 30)
        self.assertTrue(all(sun[rid]["place"] != "office" for rid in C.res))
        self.assertEqual(sum(1 for x in sun.values() if x["state"] == "away"), 0)

    def test_market_only_on_market_days(self):
        def sellers(day):
            return sum(1 for x in C.at(day * 1440 + 10 * 60).values() if x["act"] == "sell")
        self.assertEqual(sellers(SUMMER_WED), 2)
        self.assertEqual(sellers(SUMMER_WED + 1), 0)

    def test_loading_is_by_distance(self):
        """CITY-024: only residents near a viewer get a body."""
        st = C.at(SUMMER_WED * 1440 + 12 * 60 + 40)
        near = C.loaded(st, [(80, 80)], 30)
        self.assertTrue(0 < len(near) < len(st))
        for rid in near:
            self.assertLessEqual(cs.dist(st[rid]["pos"], (80, 80)), 30)
        self.assertEqual(C.loaded(st, [], 30), set())


class World(unittest.TestCase):
    def test_weather_is_shared_and_seasonal(self):
        """WLD-003: weather depends only on the time; autumn is wetter than summer."""
        self.assertEqual(cs.weather(C.d, C.clock, 5000.0), cs.weather(cs.City().d, C.clock, 5000.0))
        def share(season):
            hits = [cs.weather(C.d, C.clock, (y * 48 + season * 12 + d) * 1440 + h * 360 + 10) == "rain" for y in range(40) for d in range(12) for h in range(4)]
            return sum(hits) / len(hits)
        self.assertGreater(share(2), share(1) + 0.08)
        for s in range(4):
            self.assertAlmostEqual(share(s), C.d["weather"]["rain_chance"][s], delta=0.07)

    def test_trees_grow_from_age(self):
        """WLD-011: size comes from age alone and only goes up while the tree stands."""
        sap = next(t for t in C.d["trees"] if t["species"] == "linden")
        sizes = [C.veg.tree(sap, d)["radius"] for d in range(0, 48 * 8, 6)]
        self.assertEqual(sizes, sorted(sizes))
        self.assertGreater(sizes[-1], 3.0 * sizes[0])

    def test_leaf_cycle(self):
        """WLD-012: bare in winter, full in summer, turning and dropping in autumn, blossom in spring for plum."""
        plane = C.d["trees"][0]
        self.assertEqual(C.veg.tree(plane, 48 + 42)["leaf"], 0.0)
        self.assertEqual(C.veg.tree(plane, 48 + 18)["leaf"], 1.0)
        aut = C.veg.tree(plane, 48 + 30)
        self.assertTrue(0.0 < aut["leaf"] < 1.0 and aut["turn"] > 0.5 and aut["ground"] > 0.0)
        plum = next(t for t in C.d["trees"] if t["species"] == "plum" and t["planted"] < -48)
        self.assertEqual(C.veg.tree(plum, 48 + 3)["blossom"], 1.0)
        self.assertEqual(C.veg.tree(plum, 48 + 20)["blossom"], 0.0)

    def test_trees_fall_and_are_replanted(self):
        """WLD-013: within ten years at least one tree falls, the site stands empty, then a sapling grows."""
        seen = set()
        for tr in C.d["trees"]:
            states = [C.veg.tree(tr, d)["state"] for d in range(0, 480)]
            if "fallen" in states:
                i = states.index("fallen")
                self.assertIn("empty", states[i:])
                j = i + states[i:].index("empty")
                if "growing" in states[j:]:
                    k = j + states[j:].index("growing")
                    self.assertLess(C.veg.tree(tr, k)["radius"], 1.0)
                    seen.add(tr["id"])
        self.assertTrue(seen, "no tree fell and was replanted in ten years")

    def test_vehicles_keep_to_their_routes(self):
        """CITY-040: the tram is on its street and stops at the stop; the van stops at the grocer."""
        tr = C.d["tram"]
        stop_minute = SUMMER_WED * 1440 + cs.hm("08:00") + 0.5
        trams = [v for v in C.vehicles(stop_minute) if v["kind"] == "tram"]
        self.assertTrue(trams and trams[0]["stopped"] and trams[0]["pos"] == (tr["stop_x"], tr["y"]))
        vans = [v for t in range(0, 30) for v in C.vehicles(SUMMER_WED * 1440 + cs.hm("07:25") + t * 0.5) if v["kind"] == "van" and v["stopped"]]
        self.assertTrue(any(cs.dist(v["pos"], (63, 20)) < 0.5 for v in vans))


if __name__ == "__main__":
    unittest.main()
