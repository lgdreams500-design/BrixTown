"""CITY-049: resident cars give way to what a player controls (giveway.py).     python -m unittest test_giveway -v"""
import json, math, os, unittest

import giveway

RULES = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "World", "rules", "traffic.json"), encoding="utf-8"))["give_way"]
SPEED = 200.0                                              # m a minute (12 m/s), the timeline's own speed
DT = 0.1 / 60                                              # a tenth of a second, in minutes


def timeline(start, stay):
    """A car's day as a lookup: east along z = 0 from x 0 to 300 from minute `start`, parked there `stay` minutes, back."""
    def at(m):
        t = m - start
        go = 300 / SPEED
        if t < 0:
            return dict(pos=(0.0, 0.0), head=(1.0, 0.0), moving=False, trip=False)
        if t < go:
            return dict(pos=(SPEED * t, 0.0), head=(1.0, 0.0), moving=True, trip=True)
        if t < go + stay:
            return dict(pos=(300.0, 0.0), head=(1.0, 0.0), moving=False, trip=False)
        if t < 2 * go + stay:
            return dict(pos=(300 - SPEED * (t - go - stay), 0.0), head=(-1.0, 0.0), moving=True, trip=True)
        return dict(pos=(0.0, 0.0), head=(-1.0, 0.0), moving=False, trip=False)
    return at


def town(stay=4.5):
    lines = {0: timeline(0.0, stay), 1: timeline(0.1, stay)}   # car 1 follows car 0, 20 m behind
    return giveway.GiveWay(RULES, lambda k, m: lines[k](m), lambda k: (4.2, 1.8, 0.0))


def front(g, k, m):
    c = g.at(k, m)
    return c["pos"][0] + c["head"][0] * 2.1


class GiveWay(unittest.TestCase):
    def test_CITY_049_cars_stop_short_of_a_player_and_queue_behind(self):
        g, m, me = town(), 0.0, (100.0, 0.3, RULES["person_r_m"])
        while m < 1.2:                                     # the player stands in the road at x 100
            g.step(m, DT, [me], [0, 1])
            m += DT
            self.assertLessEqual(front(g, 0, m), me[0] - me[2] - RULES["stop_m"] + .05, "car 0 never runs into the player")
            if m > .15:                                    # both on the road
                self.assertLessEqual(front(g, 1, m), g.at(0, m)["pos"][0] - 2.1 - RULES["stop_m"] + .05, "car 1 never runs into car 0")
        x0, x1 = g.at(0, m)["pos"][0], g.at(1, m)["pos"][0]
        for _ in range(20):
            g.step(m, DT, [me], [0, 1])
            m += DT
        self.assertAlmostEqual(g.at(0, m)["pos"][0], x0, delta=.01, msg="car 0 waits")
        self.assertAlmostEqual(g.at(1, m)["pos"][0], x1, delta=.01, msg="car 1 waits behind it")
        self.assertGreater(g.late[0], .5)

    def test_CITY_049_back_on_its_timeline_without_speeding(self):
        g, m, me = town(), 0.0, (100.0, 0.0, RULES["person_r_m"])
        while m < 1.0:
            g.step(m, DT, [me], [0, 1])
            m += DT
        last = {k: g.at(k, m)["pos"] for k in (0, 1)}
        late = dict(g.late)
        while m < 9.0:                                     # the player has gone
            g.step(m, DT, [], [0, 1])
            m += DT
            for k in (0, 1):
                p = g.at(k, m)["pos"]
                self.assertLessEqual(math.dist(p, last[k]), SPEED * DT + 1e-6, "never faster than its timeline")
                if g.at(k, m)["moving"]:
                    self.assertGreaterEqual(g.late.get(k, 0), late.get(k, 0) - 1e-9, "never less late while on the move")
                last[k], late[k] = p, g.late.get(k, 0)
            if 6.5 < m < 9:
                for k in (0, 1):
                    self.assertEqual(g.late.get(k, 0), 0.0, "caught up at its stop")
                    self.assertEqual(g.at(k, m), g.car_at(k, m), "back on its timeline")

    def test_CITY_049_a_short_stop_is_skipped(self):
        g, m = town(stay=0.3), 1.0
        g.late[0] = 1.0                                    # a minute behind; its stop is 0.3 minutes long
        while g.at(0, m)["moving"]:
            g.step(m, DT, [], [0])
            m += DT
        g.step(m, DT, [], [0])
        self.assertAlmostEqual(g.late[0], 0.7, delta=.01, msg="the whole stop is skipped, no more")
        m += DT
        self.assertTrue(g.at(0, m)["moving"], "and it leaves at once")

    def test_CITY_049_far_behind_and_out_of_sight_is_placed_back(self):
        g = town()
        g.late[0] = 12.0                                   # on its way, twelve minutes behind
        g.step(12.5, DT, [(100.0, 30.0, .4)], [0])
        self.assertEqual(g.late[0], 12.0, "a player is near: it stays where it is")
        g.step(12.5, DT, [(100.0, 500.0, .4)], [0])
        self.assertEqual(g.late[0], 0.0, "nobody near: back on its timeline")

    def test_CITY_049_the_other_lane_is_not_held(self):
        g, m = town(), 0.0
        car = [(100.0 + f, -3.2, .9) for f in (-1.2, 0, 1.2)]   # the player's car, oncoming in the other lane
        while m < 1.4:
            g.step(m, DT, car, [0, 1])
            m += DT
        self.assertEqual(g.late, {})


if __name__ == "__main__":
    unittest.main()
