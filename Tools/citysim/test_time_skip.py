"""Skip time (WLD-073, WLD-074).      python -m unittest test_time_skip -v"""
import copy, os, sys, unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import world                                               # noqa: E402
from citysim import Clock, weather, hm                     # noqa: E402
from time_skip import ask, next_weather, target            # noqa: E402

W = world.load()
RULES, CAL = W["time_skip"], W["calendar"]
CLOCK = Clock(CAL["calendar"])
TIMES = [d * 1440 + hm(x) for d in (0, 5, 16, 31, 40) for x in ("00:00", "05:59", "06:00", "11:37", "17:59", "23:55")]


class Skip(unittest.TestCase):
    def test_WLD_073_the_three_steps_are_in_data(self):
        self.assertEqual([s["id"] for s in RULES["steps"]], ["10m", "1h", "weather"])

    def test_WLD_073_ten_minutes_and_an_hour_move_the_clock_by_that_much(self):
        for t in TIMES:
            self.assertEqual(target(RULES, CAL, t, "10m"), t + 10)
            self.assertEqual(target(RULES, CAL, t, "1h"), t + 60)

    def test_WLD_073_next_weather_lands_on_the_first_change(self):
        size = RULES["next_weather"]["slot_minutes"]
        changed = 0
        for t in TIMES + [d * 1440 + 90 for d in range(48)]:
            new = next_weather(RULES, CAL, t)
            self.assertGreater(new, t)
            now = weather(CAL, CLOCK, t)
            if new < t + RULES["next_weather"]["max_days"] * 1440:
                changed += 1
                self.assertEqual(new % size, 0, "a change of weather starts a slot")
                self.assertNotEqual(weather(CAL, CLOCK, new), now)
            for m in range(int(t), int(new), 30):          # nothing changed on the way
                self.assertEqual(weather(CAL, CLOCK, m), now, (t, m, new))
        self.assertGreater(changed, 0)

    def test_WLD_073_next_weather_stops_at_the_limit_when_it_never_changes(self):
        cal = copy.deepcopy(CAL)
        for k in ("rain_chance", "cloud_chance", "fog_chance", "snow_chance"):
            cal["weather"][k] = [0.0] * 4
        t = 3 * 1440 + 100
        self.assertEqual(next_weather(RULES, cal, t), t + RULES["next_weather"]["max_days"] * 1440)

    def test_WLD_073_a_skip_moves_only_the_clock(self):
        """The world after a skip is the world read fresh at the new time: same weather, same daylight."""
        for t in TIMES:
            for step in ("10m", "1h", "weather"):
                new = target(RULES, CAL, t, step)
                self.assertEqual(weather(CAL, CLOCK, new), weather(CAL, Clock(CAL["calendar"]), new))
                self.assertEqual(CLOCK.daylight(new), Clock(CAL["calendar"]).daylight(new))

    def test_WLD_074_only_the_host_may_skip(self):
        t = 16 * 1440 + hm("12:00")
        self.assertEqual(ask(RULES, CAL, t, "1h", "p1", "p1"), (t + 60, None))
        self.assertEqual(ask(RULES, CAL, t, "1h", "p2", "p1"), (t, "not_host"))

    def test_WLD_074_no_skip_during_a_ride_or_a_conversation(self):
        t = 16 * 1440 + hm("12:00")
        self.assertEqual(ask(RULES, CAL, t, "10m", "p1", "p1", busy=["ride"]), (t, "busy:ride"))
        self.assertEqual(ask(RULES, CAL, t, "10m", "p1", "p1", busy=["conversation"]), (t, "busy:conversation"))

    def test_WLD_074_an_unknown_step_is_refused(self):
        self.assertEqual(ask(RULES, CAL, 100, "1y", "p1", "p1"), (100, "unknown_step"))


if __name__ == "__main__":
    unittest.main()
