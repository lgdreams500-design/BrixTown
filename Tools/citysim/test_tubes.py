"""Glass travel tubes (CITY-108 to CITY-112).      python -m unittest test_tubes -v

The map has no tube stops yet (where they go is the owner's to say), so these tests run on a small network of their own:
four stops round a junction, and a long way round between two of them.
"""
import itertools, math, os, random, sys, unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import world                                               # noqa: E402
from tubes import Ride, mouth, problems, route             # noqa: E402

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

    def test_CITY_112_every_stop_in_town_reaches_every_other(self):
        for a, b in itertools.permutations(self.net["stops"], 2):
            self.assertIsNotNone(route(self.net, a, b), (a, b))


class Riding(unittest.TestCase):
    def setUp(self):
        self.rides = [Ride(NET, RULES, a, b) for a, b in itertools.permutations(NET["stops"], 2)]

    def test_CITY_109_a_ride_goes_floor_tube_floor(self):
        r = RULES["ride"]
        for ride in self.rides:
            a, b = NET["stops"][ride.nodes[0]], NET["stops"][ride.nodes[-1]]
            self.assertEqual(ride.at(0), (a["pos"][0], a["pos"][1], 0.0))
            self.assertEqual(ride.phase(0.1), "draw_in")
            self.assertAlmostEqual(ride.at(r["draw_in_s"])[2], H, places=6)
            self.assertEqual(ride.phase(ride.t0 + 0.01), "run")
            self.assertEqual(ride.phase(ride.t1 + 0.01), "pop_out")
            end = ride.at(ride.duration)
            self.assertEqual(end[2], 0.0)
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
            a = NET["stops"][ride.nodes[0]]
            walk_in = (a["pos"][0] + a["face"][0] * c["behind_m"], a["pos"][1] + a["face"][1] * c["behind_m"], c["above_m"])
            for x, y in zip(cams[0], walk_in):
                self.assertAlmostEqual(x, y)                                    # starts as the everyday camera
            b, end = NET["stops"][ride.nodes[-1]], ride.at(ride.duration)
            settled = (end[0] - b["face"][0] * c["behind_m"], end[1] - b["face"][1] * c["behind_m"], c["above_m"])
            for x, y in zip(cams[-1], settled):
                self.assertAlmostEqual(x, y)                                    # and ends as it, behind the rider

    def test_CITY_110_the_camera_keeps_the_rider_close(self):
        c, v = RULES["camera"], RULES["ride"]["speed_mps"]
        far = v * c["follow_s"] + c["behind_m"] + c["above_m"]
        for ride in self.rides:
            for s in frames(ride):
                self.assertLess(math.dist(ride.camera(s), ride.at(s)), far)


if __name__ == "__main__":
    unittest.main()
