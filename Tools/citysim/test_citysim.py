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

    def test_WLD_069_snow_falls_in_winter_only(self):
        """Snow has its own chance per season (the owner: winter only), comes in spells, and is the same for everyone. Over the
        real city's calendar (World/rules/calendar.json)."""
        import os, sys
        sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
        import world
        C = world.simulation(world.load())
        def share(season, kind):
            hits = [cs.weather(C.d, C.clock, (y * 48 + season * 12 + d) * 1440 + h * 360 + 10) == kind for y in range(40) for d in range(12) for h in range(4)]
            return sum(hits) / len(hits)
        chance = C.d["weather"]["snow_chance"]
        for season in range(4):
            if chance[season] == 0:
                self.assertEqual(share(season, "snow"), 0.0, season)
            else:
                self.assertAlmostEqual(share(season, "snow"), chance[season], delta=0.07)
        self.assertEqual(chance[:3], [0.0, 0.0, 0.0])
        slots = [cs.weather(C.d, C.clock, t * 360 + 10) == "snow" for t in range(4 * 12 * 3, 4 * 12 * 4 * 20)]
        runs = sum(1 for a, b in zip(slots, slots[1:]) if b and not a)
        self.assertLess(runs, sum(slots) * 0.8)            # in spells: more snowy slots than spells
        self.assertEqual(cs.weather(C.d, C.clock, 40 * 1440 + 10), cs.weather(world.simulation(world.load()).d, C.clock, 40 * 1440 + 10))

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

    def test_WLD_056_pines_stay_green_and_woods_trees_leaf(self):
        """WLD-056: the woods' trees (World/rules/vegetation.json): a pine keeps its needles and colour all year; oak and
        birch leaf and drop like the town's trees; apple blossoms in spring."""
        import json, os
        with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "World", "rules", "vegetation.json"), encoding="utf-8") as f:
            v = json.load(f)
        veg = cs.Vegetation(v, C.clock)
        pine = dict(species="pine", planted=-48 * 20, seed=7)
        looks = [veg.tree(pine, 48 * 3 + d) for d in range(48)]
        self.assertTrue(all(t["leaf"] == 1.0 and t["color"] == v["species"]["pine"]["summer"] for t in looks))
        self.assertEqual(v["species"]["pine"]["crown"], "cone")
        for sp in ("oak", "birch"):
            t = dict(species=sp, planted=-48 * 20, seed=3)
            self.assertEqual(veg.tree(t, 48 * 3 + 42)["leaf"], 0.0)
            self.assertEqual(veg.tree(t, 48 * 3 + 18)["leaf"], 1.0)
        apple = dict(species="apple", planted=-48 * 6, seed=5)
        self.assertEqual(veg.tree(apple, 48 * 3 + 5)["blossom"], 1.0)
        self.assertEqual(veg.tree(apple, 48 * 3 + 20)["blossom"], 0.0)

    def test_CITY_017_planting_follows_the_seasons(self):
        """CITY-017: garden plants (World/rules/vegetation.json) bulbs and lilac in spring, roses, lavender and hydrangea in
        summer, asters and dry heads in autumn; in winter the deciduous shrubs are bare, the bulbs gone, the evergreens
        still green. Something is in flower on every day from early spring to the end of autumn, and the answer for a day never
        depends on what was asked before."""
        import json, os
        with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "World", "rules", "vegetation.json"), encoding="utf-8") as f:
            plants = json.load(f)["plants"]
        veg = cs.Vegetation(dict(plants=plants), C.clock)
        names = [k for k in plants if k != "note"]
        Y = 48 * 3                                          # a year well after the start
        at = lambda n, doy: veg.plant(n, Y + doy)
        self.assertEqual(at("daffodil", 5)["bloom"], 1.0)
        self.assertEqual(at("lilac", 7)["bloom"], 1.0)
        for n in ("rose", "lavender", "hydrangea"):
            self.assertEqual(at(n, 18)["bloom"], 1.0, n)
            self.assertEqual(at(n, 18)["leaf"], 1.0, n)
        self.assertEqual(at("aster", 30)["bloom"], 1.0)
        self.assertEqual(at("hydrangea", 32)["spent"], 1.0)
        self.assertGreater(at("rose", 31)["color"][0], at("rose", 18)["color"][0])   # leaves turning
        for doy in (40, 44):
            for n in ("lilac", "hydrangea", "rose"):
                self.assertEqual(at(n, doy)["leaf"], 0.0, (n, doy))
            for n in ("daffodil", "tulip"):
                self.assertEqual(at(n, doy)["size"], 0.0, (n, doy))
            for n in ("box", "lavender"):
                self.assertEqual(at(n, doy)["leaf"], 1.0, (n, doy))
            self.assertTrue(all(at(n, doy)["bloom"] == 0.0 for n in names), doy)
        for doy in range(3, 35):
            self.assertTrue(any(at(n, doy)["bloom"] > 0.5 for n in names), "nothing in flower on day %d" % doy)
        a = at("hydrangea", 20)
        for d in range(0, 300, 7):
            veg.plant("rose", d)
        self.assertEqual(a, at("hydrangea", 20))
        self.assertEqual(veg.plant("tulip", 7), veg.plant("tulip", 7 + 48 * 5))   # the same day of any year looks the same

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


class Weather(unittest.TestCase):
    """F-NPC-REACT level 1: residents react to the weather (CITY-077 to CITY-080), over the real city in World/."""
    RAIN_AT_SIX = 18                     # a summer Friday: dry until 18:00, when rain starts over full gardens

    @classmethod
    def setUpClass(cls):
        import os, sys
        sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
        import world
        cls.W = world.load()
        cls.C = world.simulation(cls.W)
        cls.R = cls.W["weather_react"]

    def test_CITY_077_dress_for_the_weather(self):
        """Coats in the cold and the wet, umbrellas on days with rain, decided once a day and the same on every machine."""
        C, R = self.C, self.R
        self.assertEqual(C.rain_spells(self.RAIN_AT_SIX), [(1080, 1440)])
        def share(day, key, wet=None):
            return sum(C.wear(rid, day, wet)[key] for rid in C.res) / len(C.res)
        self.assertAlmostEqual(share(16, "umbrella", True), R["umbrella"]["rain_day"], delta=0.05)
        self.assertLess(share(16, "umbrella", False), 0.1)
        self.assertGreater(share(16, "coat", True), share(16, "coat", False) + 0.5)   # summer: coats come out for rain
        self.assertGreater(share(42, "coat", False), share(16, "coat", False) + 0.6)  # winter against summer
        for rid in list(C.res)[:200]:
            w = C.wear(rid, self.RAIN_AT_SIX)
            self.assertEqual(w, C.wear(rid, self.RAIN_AT_SIX))
            self.assertTrue(w["coat"] or not w["hood"], rid)
            self.assertEqual(w["umbrella"], w["umbrella_colour"] is not None)

    def test_CITY_078_take_cover_when_rain_starts(self):
        """Out in the open when rain starts: under cover within a few minutes, and nobody stays out in the open in the rain."""
        C, R, day = self.C, self.R, self.RAIN_AT_SIX
        before = C.at(day * 1440 + 1079)
        out = [rid for rid, s in before.items() if s["pos"] and C.lots[s["place"]]["kind"] in R["open_kinds"] and s["act"] not in R["stay_out"] + ["walk"]]
        self.assertGreater(len(out), 100, "the gardens should be full before the rain")
        for m in (1080 + R["shelter"]["notice_minutes"][1] + 12, 1200, 1320):
            now = C.at(day * 1440 + m)
            caught = [rid for rid, s in now.items() if s["pos"] and s["state"] != "walking" and C.lots[s["place"]]["kind"] in R["open_kinds"] and s["act"] not in R["stay_out"]]
            self.assertEqual(caught, [], "still out in the open at minute %d" % m)
        under = C.at(day * 1440 + 1110)
        # under cover: under an awning, or already indoors (with habits spread over the day, CITY-096, more of them were due home anyway)
        self.assertGreater(sum(1 for rid in out if under[rid]["act"] == "shelter" or under[rid]["state"] == "inside"), len(out) / 3)
        self.assertEqual(C.at(day * 1440 + 1100), world_city_again().at(day * 1440 + 1100))     # CITY-022 still holds

    def test_CITY_078_back_out_when_the_rain_stops(self):
        """A spell that ends with enough of the stay left: they go back; too little left: they stay under cover."""
        C, day = self.C, self.RAIN_AT_SIX
        rid = next(r for r in C.res if C.res[r].get("near", {}).get("garden"))
        g = C.res[rid]["near"]["garden"]
        stay = lambda a, b: [dict(start=float(a), end=float(b), act="rest", place=g, use=C._use("rest", g), idx=3)]
        real = C.rain_spells
        try:
            C.rain_spells = lambda d: [(720, 780)]
            got = C._take_cover(rid, day, stay(700, 840))
            self.assertEqual([(s["act"] in ("shelter", "home"), round(s["start"]), round(s["end"])) for s in got][0], (False, 700, round(got[1]["start"])))
            self.assertTrue(720 < got[1]["start"] <= 720 + self.R["shelter"]["notice_minutes"][1] and got[1]["rush"])
            self.assertEqual((got[1]["end"], got[2]["act"], got[2]["start"], got[2]["end"]), (780, "rest", 780, 840))
            got = C._take_cover(rid, day, stay(700, 790))                   # 10 minutes left: not worth going back
            self.assertEqual((len(got), got[-1]["end"]), (2, 790))
            got = C._take_cover(rid, day, stay(730, 840))                   # due out while it rains: straight to cover, no running
            self.assertEqual((got[0]["start"], got[0]["rush"], got[1]["act"]), (730, False, "rest"))
        finally:
            C.rain_spells = real

    def test_CITY_079_hurry_in_the_rain(self):
        """Walks in the rain are faster: a little under an umbrella, more without; running for cover faster still. Dry walks are not."""
        C, p, day = self.C, self.R["pace"], self.RAIN_AT_SIX
        runs = 0
        for rid, pieces in C.plan(day).items():
            v = C.speed(rid) * C.pace
            umb = C.wear(rid, day)["umbrella"]
            for kind, t0, t1, payload, s in pieces:
                if kind != "move" or t1 >= 1439.4:
                    continue
                f = payload[2] / v
                if not C.raining(day * 1440 + t0):
                    self.assertAlmostEqual(f, 1.0, places=3, msg=rid)
                elif umb:
                    self.assertAlmostEqual(f, p["umbrella"], places=3, msg=rid)
                else:
                    self.assertAlmostEqual(f, p["run"] if s.get("rush") else p["hurry"], places=3, msg=rid)
                    runs += bool(s.get("rush"))
        self.assertGreater(runs, 20, "someone should run for cover")

    def test_CITY_107_coats_hoods_and_a_careful_walk_in_snow(self):
        """On a day with snow nearly everyone wears a coat and most coats have the hood up; out in the snow no one puts an
        umbrella up (hood or bare-headed); walks in snow go at the careful pace, slower than their own; they take cover from it
        as from rain."""
        C, R = self.C, self.R
        day = next(d for d in range(36, 48) if any(C.snowing(d * 1440 + k * 360 + 1) for k in (1, 2)))   # snow by day
        wear = [C.wear(rid, day) for rid in C.res]
        self.assertGreaterEqual(sum(w["coat"] for w in wear) / len(wear), R["coat"]["snow_day"] - 0.05)
        coats = [w for w in wear if w["coat"]]
        self.assertGreater(sum(w["hood"] for w in coats) / len(coats), R["coat"]["snow_hood"] - 0.07)
        k = next(k for k in (1, 2) if C.snowing(day * 1440 + k * 360 + 1))
        m = k * 360 + 200
        seen = set()
        for rid, st in C.at(day * 1440 + m).items():
            if st["cover"] is not None:
                self.assertIn(st["cover"], ("hood", "bare"), rid)
                seen.add(st["cover"])
        self.assertIn("hood", seen)
        self.assertTrue(C.rain_spells(day))                # they take cover from it (CITY-078's spells)
        slow = 0
        for rid, pieces in C.plan(day).items():
            v = C.speed(rid) * C.pace
            for kind, t0, t1, payload, ss in pieces:
                if kind == "move" and t1 < 1439.4 and C.snowing(day * 1440 + t0):
                    self.assertAlmostEqual(payload[2] / v, R["pace"]["snow"], places=3, msg=rid)
                    slow += 1
        self.assertGreater(slow, 10)

    def test_CITY_080_cover_in_the_rain(self):
        """Out in the rain: umbrella up if they brought one, else a hood if their coat has one, else bare-headed. Dry, indoors
        or under an awning: nothing."""
        C, day = self.C, self.RAIN_AT_SIX
        self.assertTrue(all(s["cover"] is None for s in C.at(day * 1440 + 1000).values()))
        wet = C.at(day * 1440 + 1150)
        seen = set()
        for rid, s in wet.items():
            w = C.wear(rid, day)
            if s["state"] in ("inside", "away", "driving") or s["act"] == "shelter":
                self.assertIsNone(s["cover"], rid)
            else:
                self.assertEqual(s["cover"], "umbrella" if w["umbrella"] else "hood" if w["hood"] else "bare", rid)
                seen.add(s["cover"])
        self.assertEqual(seen, {"umbrella", "hood", "bare"})


def world_city_again():
    import world
    return world.simulation(world.load())


if __name__ == "__main__":
    unittest.main()
