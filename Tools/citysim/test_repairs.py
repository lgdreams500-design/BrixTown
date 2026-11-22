"""Repair crews mending crash damage (LIVING_CITY CITY-075).      python -m unittest test_repairs -v"""
import math, os, random, sys, unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import world                                               # noqa: E402
from citysim import hm                                     # noqa: E402
from repairs import repairs, mended                        # noqa: E402

W = world.load()
RULES = W["crash"]
PACE = W["calendar"]["calendar"]["real_seconds_per_minute"]
DEPOTS = sorted((l["id"], tuple(l["door"])) for l in world.lots(W) if l["kind"] == "utility" and l.get("enterable"))
START, END = hm(RULES["repair"]["start"]), hm(RULES["repair"]["end"])
KINDS = sorted(RULES["kinds"])


def some_damage(n=40, seed=3, day=16):
    rnd = random.Random(seed)
    return [dict(id="d%02d" % i, at=day * 1440 + rnd.uniform(6 * 60, 23 * 60), kind=rnd.choice(KINDS),
                 pos=(rnd.uniform(20, 750), rnd.uniform(20, 750))) for i in range(n)]


class Repairs(unittest.TestCase):
    def setUp(self):
        self.damage = some_damage()
        self.plan = repairs(self.damage, DEPOTS, RULES, PACE)

    def test_there_are_depots_to_send_crews(self):
        self.assertGreaterEqual(len(DEPOTS), 1)

    def test_everything_gets_mended_after_it_broke(self):     # CITY-075
        self.assertEqual(set(self.plan), {d["id"] for d in self.damage})
        for d in self.damage:
            p = self.plan[d["id"]]
            self.assertGreaterEqual(p["leave"], d["at"] + RULES["repair"]["report_min"] - 1e-9)
            self.assertGreater(p["done"], d["at"])
            self.assertFalse(mended(self.plan, d["id"], d["at"] + 1))
            self.assertTrue(mended(self.plan, d["id"], p["done"]))

    def test_crews_work_only_in_working_hours(self):         # CITY-075
        for p in self.plan.values():
            day = p["leave"] // 1440
            self.assertGreaterEqual(p["leave"] - day * 1440, START - 1e-9)
            self.assertLessEqual(p["done"] - day * 1440, END + 1e-9)

    def test_a_crew_does_one_job_at_a_time_at_walking_pace(self):   # CITY-075
        by_crew = {}
        for p in self.plan.values():
            by_crew.setdefault(p["crew"], []).append(p)
        speed = RULES["repair"]["walk_mps"] * PACE
        for crew, jobs in by_crew.items():
            jobs.sort(key=lambda p: p["leave"])
            pos = dict(DEPOTS)[crew.split("/")[0]]
            last = -1e18
            for p in jobs:
                self.assertGreaterEqual(p["leave"], last - 1e-9, crew)
                walk = math.dist(pos, p["pos"]) * RULES["repair"]["detour"] / speed
                self.assertAlmostEqual(p["arrive"] - p["leave"], walk, places=6)
                work = RULES["kinds"][p["kind"]]["work_min"]
                self.assertAlmostEqual(p["done"] - p["arrive"], work, places=6)
                last, pos = p["done"], p["pos"]

    def test_same_answer_in_any_order(self):                  # CITY-075: calculated from the list alone
        shuffled = list(self.damage)
        random.Random(9).shuffle(shuffled)
        self.assertEqual(repairs(shuffled, list(reversed(DEPOTS)), RULES, PACE), self.plan)

    def test_damage_by_a_depot_is_mended_by_its_own_crew(self):
        did, door = DEPOTS[0]
        p = repairs([dict(id="x", at=16 * 1440 + 9 * 60, kind="lamp", pos=(door[0] + 3, door[1]))], DEPOTS, RULES, PACE)["x"]
        self.assertTrue(p["crew"].startswith(did + "/"))
        self.assertAlmostEqual(p["leave"], 16 * 1440 + 9 * 60 + RULES["repair"]["report_min"])

    def test_late_damage_waits_for_the_morning(self):         # CITY-075
        did, door = DEPOTS[0]
        p = repairs([dict(id="x", at=16 * 1440 + 22 * 60, kind="tree", pos=door)], DEPOTS, RULES, PACE)["x"]
        self.assertAlmostEqual(p["leave"], 17 * 1440 + START)

    def test_every_kind_has_a_way_to_break_and_a_repair_time(self):   # CITY-072, CITY-073
        for kind, k in RULES["kinds"].items():
            self.assertIn(k["how"], ("topple", "shatter", "chip"), kind)
            self.assertGreater(k["work_min"], 0, kind)
            self.assertTrue(0 < k["slow"] <= 1, kind)


if __name__ == "__main__":
    unittest.main()
