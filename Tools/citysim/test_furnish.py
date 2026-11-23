"""Furnishing a home in the building designer (docs/specs/domains/FURNITURE.md FURN-051, FURN-052, FURN-055).   python -m unittest test_furnish"""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import furnish  # noqa: E402
import world  # noqa: E402

W = world.load()
F = W["furnishing"]
S = furnish.check_script()
ROOM, KEEP, BEFORE = S["room"], S["keep"], S["before"]


def after(edit):
    return furnish.apply(F, BEFORE, edit)


def check(edit, ceiling=2.6, funds=None):
    return furnish.check(F, ROOM, KEEP, ceiling, BEFORE, after(edit), funds)


class TheList(unittest.TestCase):
    def test_FURN_051_every_piece_on_the_list_is_sized_named_priced_and_in_a_room(self):
        self.assertTrue(F["pieces"])
        rooms = [k for ks in F["rooms"].values() for k in ks]
        self.assertEqual(sorted(rooms), sorted(F["pieces"]), "every piece in exactly one room of the list")
        for k, p in F["pieces"].items():
            self.assertTrue(p["name"], k)
            self.assertEqual(len(p["size"]), 3, k)
            self.assertTrue(all(v > 0 for v in p["size"]), k)
            self.assertIsInstance(p["price"], int, k)
            self.assertGreater(p["price"], 0, k)
        self.assertEqual(F["turn_deg"], 15)          # FURN-055: turns in 15 degree steps [ASSUMPTION]
        self.assertAlmostEqual(F["grid_m"], 0.05)    # FURN-001: the 5 cm grid


class Placing(unittest.TestCase):
    def test_FURN_051_placing(self):
        self.assertEqual(check({"move": {"t0": [1.5, 3.0, 0]}}), [])
        self.assertEqual(check({"move": {"t1": [4.5, 4.65, 90]}}), ["the wardrobe sticks out of the room"])
        self.assertEqual(check({"add": [["k1", "k17:sofa", 3.0, 0.6, 0, "plain"]]}), ["the sofa is in the way of the door or the stairs"])
        self.assertEqual(check({"move": {"t0": [4.5, 4.0, 0]}}), ["the bed and the wardrobe stand in each other"])
        self.assertEqual(check({"add": [["k1", "k17:coffee", 1.03, 1.0, 0, "plain"]]}), ["the coffee table is off the 5 cm grid"])
        self.assertEqual(check({"add": [["k1", "k17:wardrobe", 5.0, 1.5, 270, "workshop"]]}, ceiling=2.0), ["the wardrobe does not fit under the ceiling"])
        self.assertEqual(check({"add": [["k1", "spaceship", 3.0, 2.5, 0, "plain"]]}), ["the spaceship is not sold here"])

    def test_FURN_051_a_rug_is_stood_on_and_pieces_may_touch(self):
        self.assertEqual(check({"add": [["k1", "k17:sofa", 3.0, 2.5, 180, "oldtown"]]}), [], "a sofa on the rug")
        # the sofa (2.0 wide) put right against the bed (1.0 wide at x = 1.0): touching, not clashing
        self.assertEqual(check({"add": [["k1", "k17:sofa", 2.5, 3.0, 0, "plain"]]}), [])
        self.assertEqual(check({"add": [["k1", "k17:sofa", 2.45, 3.0, 0, "plain"]]}), ["the bed and the sofa stand in each other"])

    def test_FURN_051_only_what_the_change_makes_wrong(self):
        # the town's wardrobe is taller than a low ceiling already: moving the bed is still allowed
        self.assertEqual(check({"move": {"t0": [1.5, 3.0, 0]}}, ceiling=2.0), [])
        # a new piece with the same name as an old one is still checked
        self.assertEqual(check({"add": [["k1", "k17:wardrobe", 5.0, 1.5, 270, "workshop"]]}, ceiling=2.0), ["the wardrobe does not fit under the ceiling"])


class Turning(unittest.TestCase):
    def test_FURN_055_turns_in_steps_and_the_footprint_turns_with_it(self):
        self.assertEqual(check({"move": {"t0": [1.0, 3.0, 10]}}), ["the bed is not turned in 15 degree steps"])
        p = dict(BEFORE[0], yaw=90)
        xs, zs = [x for x, _ in furnish.corners(p)], [z for _, z in furnish.corners(p)]
        self.assertAlmostEqual(max(xs) - min(xs), 2.0)        # a bed turned a quarter is 2 m across
        self.assertAlmostEqual(max(zs) - min(zs), 1.0)
        self.assertEqual(check({"add": [["k1", "k17:armchair", 4.0, 2.0, 45, "oldtown"]]}), [])


class Paying(unittest.TestCase):
    def test_FURN_052_taking_away_and_moving_are_free_buying_is_priced(self):
        total, lines = furnish.cost(F, BEFORE, after({"remove": ["t1"], "move": {"t0": [1.5, 3.0, 0]}}))
        self.assertEqual(total, 0)
        self.assertEqual(lines, [["Move: Bed", 1, 0], ["Take away: Wardrobe", 1, 0]])
        total, lines = furnish.cost(F, BEFORE, after({"add": [["k1", "k17:sofa", 3.0, 2.5, 0, "curve"]]}))
        self.assertEqual(total, F["pieces"]["k17:sofa"]["price"])
        self.assertEqual(check({"add": [["k1", "k17:sofa", 3.0, 2.5, 0, "curve"]]}, funds=total - 1), ["you cannot pay %d.%02d brix" % (total // 100, total % 100)])
        self.assertEqual(check({"add": [["k1", "k17:sofa", 3.0, 2.5, 0, "curve"]]}, funds=total), [])


class TheSlicePort(unittest.TestCase):
    def test_FURN_051_a_fixed_run_for_the_slice(self):
        want = furnish.run_script(F, S)
        self.assertEqual(len(want), len(S["cases"]))
        self.assertTrue(any(r["problems"] for r in want) and any(not r["problems"] for r in want))


if __name__ == "__main__":
    unittest.main()
