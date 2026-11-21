"""Checks on the city data in World/ and on the living-city rules running over it.   python -m unittest test_world"""
import unittest

import world

W = world.load()
CITY = world.simulation(W)
DAY = 16
LOTS = world.lots(W)
BUILDINGS = [l for l in LOTS if l["kind"] not in world.OPEN_KINDS]


class TheMap(unittest.TestCase):
    def test_data_is_sound(self):
        self.assertEqual(world.validate(W), [])

    def test_eight_by_eight_blocks_each_cell_once(self):
        g = W["city"]["grid"]
        self.assertEqual((g["columns"], g["rows"]), (8, 8))
        self.assertEqual(sorted(tuple(b["grid"]) for b in W["blocks"]), [(i, j) for i in range(8) for j in range(8)])

    def test_key_amenities_exist(self):
        names = {l["name"] for l in LOTS}
        for must in ("Market Square", "Town Hall", "Lantern Arcade", "Mill Street Hospital"):
            self.assertIn(must, names)
        count = lambda kind: sum(l["kind"] == kind for l in LOTS)
        self.assertGreaterEqual(count("home"), 300)
        self.assertGreaterEqual(count("shop"), 20)
        self.assertGreaterEqual(count("food"), 20)
        self.assertGreaterEqual(count("health"), 5)
        self.assertGreaterEqual(count("garden"), 20)
        self.assertGreaterEqual(count("stop"), 4)
        self.assertGreaterEqual(count("utility"), 2)
        towers = [l for l in LOTS if l.get("tower")]
        self.assertGreaterEqual(len(towers), 5)
        self.assertTrue(all(t["floors"] >= 12 for t in towers))

    def test_nearly_every_building_can_be_entered(self):
        self.assertGreaterEqual(sum(l["enterable"] for l in BUILDINGS) / len(BUILDINGS), 0.95)

    def test_every_building_you_can_enter_has_something_to_use(self):
        for l in BUILDINGS:
            if l["enterable"]:
                self.assertTrue(any(l["objects"].values()), l["id"])

    def test_a_block_added_by_hand_is_checked(self):
        import copy
        w = copy.deepcopy(W)
        w["blocks"][0]["lots"][0]["door"] = [w["blocks"][0]["lots"][0]["door"][0] + 3.3, w["blocks"][0]["lots"][0]["door"][1] + 3.3]
        self.assertTrue(any("door is not on a wall" in p for p in world.validate(w)))


class TheResidents(unittest.TestCase):
    def test_everyone_has_a_home_and_a_role(self):
        homes = {l["id"] for l in LOTS if l["kind"] == "home"}
        for r in W["residents"]:
            self.assertIn(r["home"], homes)
            self.assertIn(r["role"], W["roles"]["roles"])

    def test_every_workplace_is_staffed(self):
        staffed = {r["work"] for r in W["residents"] if r["work"]}
        for l in BUILDINGS:
            if l["enterable"] and l["kind"] in ("shop", "food", "health"):
                self.assertIn(l["id"], staffed, l["name"])

    def test_the_day_has_no_gaps_and_starts_and_ends_in_bed(self):
        for rid, pieces in CITY.plan(DAY).items():
            self.assertAlmostEqual(pieces[0][1], 0.0)
            self.assertAlmostEqual(pieces[-1][2], 1440.0)
            for a, b in zip(pieces, pieces[1:]):
                self.assertAlmostEqual(a[2], b[1], places=6)
            self.assertEqual(pieces[0][4]["place"], CITY.res[rid]["home"])
            self.assertEqual(pieces[-1][4]["place"], CITY.res[rid]["home"])

    def test_nobody_walks_faster_than_their_own_pace(self):
        for rid, pieces in CITY.plan(DAY).items():
            v = CITY.speed(rid) * CITY.pace
            for kind, t0, t1, payload, s in pieces:
                if kind == "move":
                    self.assertLessEqual(payload[2], v * 1.01, rid)

    def test_most_walks_arrive_on_time(self):
        late = [t1 - s["start"] for pieces in CITY.plan(DAY).values() for kind, t0, t1, payload, s in pieces if kind == "move" and payload[1][-1] > 5]
        self.assertGreaterEqual(sum(l < 2 for l in late) / len(late), 0.8)

    def test_the_city_sleeps_at_night_and_is_out_by_day(self):
        night = CITY.at(DAY * 1440 + 3 * 60)
        self.assertTrue(all(s["state"] == "inside" for s in night.values()))
        noon = CITY.at(DAY * 1440 + 12.5 * 60)
        self.assertGreater(sum(s["state"] in ("walking", "outside") for s in noon.values()), 80)

    def test_only_nearby_residents_get_bodies(self):
        state = CITY.at(DAY * 1440 + 18 * 60)
        spawn = tuple(W["city"]["spawn"]["pos"])
        near = CITY.loaded(state, [spawn], radius=85.0)
        self.assertTrue(0 < len(near) < len(state) / 4)


if __name__ == "__main__":
    unittest.main()
