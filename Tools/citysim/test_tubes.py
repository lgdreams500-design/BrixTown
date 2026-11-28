"""Glass travel tubes (CITY-108 to CITY-112).      python -m unittest test_tubes -v

The map has no tube stops yet (where they go is the owner's to say), so these tests run on a small network of their own:
four stops round a junction, and a long way round between two of them.
"""
import itertools, math, os, random, sys, unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import world                                               # noqa: E402
from tubes import Ride, crown_top, mouth, problems, rounded, route, through_crowns   # noqa: E402

RULES = world.load()["tubes"]
H = RULES["ride"]["draw_in_height_m"]
FPS = 60


def sample():
    stops = {"north": {"name": "North", "pos": [0, 300], "face": [0, 1]},
             "south": {"name": "South", "pos": [0, -300], "face": [0, -1]},
             "east": {"name": "East", "pos": [260, 0], "face": [1, 0]},
             "west": {"name": "West", "pos": [-40, 10], "face": [-1, 0]}}
    net = {"stops": stops, "junctions": {"hub": {"pos": [0, 0, 18]}}, "tubes": []}

    def tube(a, b, *mid):
        net["tubes"].append({"a": a, "b": b, "line": [list(mouth(net, RULES, a))] + [list(m) for m in mid] + [list(mouth(net, RULES, b))]})
    for s in ("north", "south", "east"):
        x, y = stops[s]["pos"]
        tube(s, "hub", [x, y, 18], [x * 0.5, y * 0.5, 20])
    tube("west", "hub", [-40, 10, 12])
    tube("north", "east", [0, 300, 25], [400, 400, 30], [500, 0, 25], [260, 0, 25])   # the long way round
    return net


NET = sample()


def frames(ride, extra=1.0):
    n = int((ride.duration + RULES["camera"]["follow_s"] + extra) * FPS)
    return [i / FPS for i in range(n + 1)]


class Network(unittest.TestCase):
    def test_CITY_108_the_sample_network_has_no_problems(self):
        self.assertEqual(problems(NET, RULES), [])

    def test_CITY_108_a_tube_too_low_or_loose_is_found(self):
        bad = sample()
        bad["tubes"][0]["line"][1][2] = 2.0
        bad["tubes"].append({"a": "east", "b": "nowhere", "line": [[0, 0, 10], [1, 1, 10]]})
        bad["stops"]["lonely"] = {"name": "Lonely", "pos": [900, 900], "face": [1, 0]}
        p = " ".join(problems(bad, RULES))
        for word in ("lower than", "nowhere", "lonely"):
            self.assertIn(word, p)

    def test_CITY_112_every_stop_reaches_every_other(self):
        for a, b in itertools.permutations(NET["stops"], 2):
            r = route(NET, a, b)
            self.assertIsNotNone(r, (a, b))
            self.assertEqual((r[0], r[-1]), (a, b))
            self.assertFalse(set(r[1:-1]) & set(NET["stops"]), "a ride passes no other stop")

    def test_CITY_112_the_ride_takes_the_shortest_way(self):
        north_east = Ride(NET, RULES, "north", "east")
        self.assertEqual(north_east.nodes, ["north", "hub", "east"])   # through the hub, not the long way round
        self.assertLess(north_east.cum[-1], 600)

    def test_CITY_112_junctions_are_passed_without_stopping(self):
        ride = Ride(NET, RULES, "north", "south")
        hub = tuple(NET["junctions"]["hub"]["pos"])
        s = min(frames(ride), key=lambda s: math.dist(ride.at(s), hub))
        self.assertGreater(math.dist(ride.at(s), ride.at(s + 1 / FPS)) * FPS, RULES["ride"]["speed_mps"] * 0.9)


class TheTownsTubes(unittest.TestCase):
    """World/city_01/tubes.json (proposed routes, make_tubes_01.py): sound, and every stop reaches every other."""
    def setUp(self):
        import json
        self.net = json.load(open(os.path.join(world.WORLD, "city_01", "tubes.json"), encoding="utf-8"))

    def test_CITY_108_the_towns_tubes_have_no_problems(self):
        self.assertEqual(problems(self.net, RULES), [])

    def test_CITY_108_the_tubes_run_over_the_grown_street_trees(self):
        """The owner, 2026-11-28: "Raise the tubes". No level run passes through a street tree's crown as it will grow
        (vegetation.json r_max, drawn as BrixTreeShapes draws it), nor within a metre of it."""
        w = world.load()
        veg = w["vegetation"].get("species", w["vegetation"])
        trees = [(t["pos"][0], t["pos"][1], veg.get(t["species"], {}).get("r_max", 4.0), veg.get(t["species"], {}).get("crown"))
                 for bl in w["blocks"] for t in bl.get("trees", [])]
        self.assertEqual(through_crowns(self.net, RULES, trees, 1.0), [])
        self.assertGreater(crown_top(6.5)[0], 15.0)          # a grown plane: the tallest tree on the lines
        low = {**self.net, "tubes": [{**t, "line": [[x, y, min(z, 9.0)] for x, y, z in t["line"]]} for t in self.net["tubes"]]}
        self.assertTrue(through_crowns(low, RULES, trees))   # at the first 9 m they went through crowns

    def test_CITY_112_every_stop_in_town_reaches_every_other(self):
        for a, b in itertools.permutations(self.net["stops"], 2):
            self.assertIsNotNone(route(self.net, a, b), (a, b))

    def test_CITY_109_the_rider_stays_inside_the_glass(self):
        """The rider's line, bends and the turns where one tube meets the next rounded, never leaves the tubes as built."""
        r = RULES["ride"]
        built = [rounded(t["line"], r["bend_r_m"]) for t in self.net["tubes"]]
        segs = [(p, q) for line in built for p, q in zip(line, line[1:])]

        def off(x):
            best = math.inf
            for p, q in segs:
                d = [b - a for a, b in zip(p, q)]
                dd = sum(v * v for v in d)
                f = 0.0 if dd < 1e-12 else max(0.0, min(1.0, sum((xi - a) * v for xi, a, v in zip(x, p, d)) / dd))
                best = min(best, math.dist(x, [a + v * f for a, v in zip(p, d)]))
            return best
        stops = list(self.net["stops"])
        for a, b in [(stops[0], s) for s in stops[1:]] + [(stops[-1], stops[1])]:
            ride = Ride(self.net, RULES, a, b)
            for k in range(0, int(ride.cum[-1]), 3):
                x = ride._on_run(k)
                self.assertLess(off(x), RULES["tube_inner_m"] / 2, (a, b, k))

    def test_CITY_109_town_rides_turn_smoothly_and_the_camera_never_cuts(self):
        stops = list(self.net["stops"])
        for b in stops[1:]:
            ride = Ride(self.net, RULES, stops[0], b)
            leans = [ride.lean(s) for s in frames(ride)]
            for p, q in zip(leans, leans[1:]):
                self.assertLess(abs(p - q), 12, (stops[0], b))
            fr = frames(ride)
            cams = [ride.camera(s) for s in fr]
            for s, cam in zip(fr, cams):
                self.assertGreater(math.dist(cam, ride.at(s)), RULES["camera"]["above_m"], (stops[0], b, s))
            for p, q in zip(cams, cams[1:]):
                self.assertLessEqual(math.dist(p, q), RULES["camera"]["max_jump_m"], (stops[0], b))

    def test_CITY_109_bends_are_rounded(self):
        line = rounded([(0, 0, 7), (0, 0, 9), (10, 0, 9), (10, 5, 9)], 1.8)
        self.assertEqual((line[0], line[-1]), ((0, 0, 7), (10, 5, 9)), "the ends stay at the mouths")
        for p, q, s in zip(line, line[1:], line[2:]):
            u, v = [b - a for a, b in zip(p, q)], [b - a for a, b in zip(q, s)]
            cos = sum(x * y for x, y in zip(u, v)) / (math.hypot(*u) * math.hypot(*v))
            self.assertGreater(cos, math.cos(math.radians(25)), "no sharp corner")


class Riding(unittest.TestCase):
    def setUp(self):
        self.rides = [Ride(NET, RULES, a, b) for a, b in itertools.permutations(NET["stops"], 2)]

    def test_CITY_109_a_ride_goes_floor_tube_floor(self):
        r = RULES["ride"]
        for ride in self.rides:
            a, b = NET["stops"][ride.nodes[0]], NET["stops"][ride.nodes[-1]]
            self.assertEqual(ride.at(0), (a["pos"][0], a["pos"][1], r["floor_m"]))
            self.assertEqual(ride.phase(0.1), "draw_in")
            self.assertAlmostEqual(ride.at(r["draw_in_s"])[2], H, places=6)
            self.assertEqual(ride.phase(ride.t0 + 0.01), "run")
            self.assertEqual(ride.phase(ride.t1 + 0.01), "pop_out")
            end = ride.at(ride.duration)
            self.assertEqual(end[2], r["floor_m"])                                  # on the far booth's floor
            out = math.dist(end[:2], b["pos"])
            self.assertAlmostEqual(out, r["pop_out_arc_m"], places=6)          # landed past the door, facing out
            self.assertAlmostEqual(ride.duration, r["draw_in_s"] + ride.t_run + r["pop_out_s"])

    def test_CITY_109_no_jump_and_never_faster_than_the_tube(self):
        v = RULES["ride"]["speed_mps"]
        for ride in self.rides:
            fr = frames(ride)
            for s, t in zip(fr, fr[1:]):
                step = math.dist(ride.at(s), ride.at(t))
                self.assertLessEqual(step, v / FPS + 1e-6, (ride.nodes, s))      # in the tube or out of it, no jump

    def test_CITY_109_worked_out_from_time_not_stepped(self):
        ride = self.rides[0]
        fr = frames(ride)
        first = [ride.at(s) for s in fr]
        shuffled = fr[:]
        random.Random(4).shuffle(shuffled)
        again = dict((s, ride.at(s)) for s in shuffled)
        self.assertEqual(first, [again[s] for s in fr])

    def test_CITY_109_upright_going_up_lying_along_level_runs(self):
        for ride in self.rides:
            fr = frames(ride)
            leans = [ride.lean(s) for s in fr]
            self.assertLess(ride.lean(ride.t0 * 0.5), 5, "going up into the tube: upright")
            level = [l for s, l in zip(fr, leans) if ride.phase(s) == "run"
                     and abs(ride.at(s + 0.3)[2] - ride.at(s - 0.3)[2]) < 0.01 and math.dist(ride.at(s + 0.3), ride.at(s - 0.3)) > 1]
            self.assertTrue(level and min(level) > 85, "lying along the level tube")
            self.assertEqual((leans[0], ride.lean(ride.duration)), (0.0, 0.0), "on their feet at both stops")
            for p, q in zip(leans, leans[1:]):
                self.assertLess(abs(p - q), 12, "turned smoothly, never flipped")

    def test_CITY_109_the_tube_fits_a_rider(self):
        self.assertLessEqual(RULES["tube_inner_m"], 0.8)
        self.assertGreaterEqual(RULES["tube_inner_m"], 0.63)           # the reference's rider, arms at the sides

    def test_CITY_110_the_camera_follows_without_a_cut(self):
        c = RULES["camera"]
        for ride in self.rides:
            fr = frames(ride)
            cams = [ride.camera(s) for s in fr]
            for p, q in zip(cams, cams[1:]):
                self.assertLessEqual(math.dist(p, q), c["max_jump_m"], ride.nodes)
            for s, cam in ((0.0, cams[0]), (fr[-1], cams[-1])):            # beside the booth at both ends, at the rider's height
                at = ride.at(s)
                self.assertAlmostEqual(math.hypot(cam[0] - at[0], cam[1] - at[1]), c["behind_m"], delta=0.5)
                self.assertAlmostEqual(cam[2], at[2] + c["above_m"], delta=0.5)

    def test_CITY_110_the_camera_stays_outside_the_rider(self):
        c = RULES["camera"]
        near = c["above_m"]                                              # never nearer than it stands over them
        for ride in self.rides:
            for s in frames(ride):
                self.assertGreater(math.dist(ride.camera(s), ride.at(s)), near, (ride.nodes, s))

    def test_CITY_110_the_camera_keeps_the_rider_close(self):
        c, v = RULES["camera"], RULES["ride"]["speed_mps"]
        far = v * c["follow_s"] + c["behind_m"] + c["above_m"]
        for ride in self.rides:
            for s in frames(ride):
                self.assertLess(math.dist(ride.camera(s), ride.at(s)), far)


if __name__ == "__main__":
    unittest.main()
