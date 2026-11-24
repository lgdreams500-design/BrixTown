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
        here = [k for k, p in F["pieces"].items() if not p.get("from")]          # FURN-064: pieces sold elsewhere are in no room
        self.assertEqual(sorted(rooms), sorted(here), "every piece sold here in exactly one room of the list")
        for k, p in F["pieces"].items():
            self.assertTrue(p["name"], k)
            self.assertEqual(len(p["size"]), 3, k)
            self.assertTrue(all(v > 0 for v in p["size"]), k)
            self.assertIsInstance(p["price"], int, k)
            self.assertGreater(p["price"], 0, k)
        self.assertEqual(F["turn_deg"], 15)          # FURN-055: turns in 15 degree steps [ASSUMPTION]
        self.assertAlmostEqual(F["grid_m"], 0.05)    # FURN-001: the 5 cm grid


    def test_FURN_064_a_piece_sold_at_the_mall_is_placed_from_the_belongings_and_not_bought_here(self):
        mall = {k: p for k, p in F["pieces"].items() if p.get("from") == "mall"}
        self.assertIn("g_guitar", mall)                                           # the owner's examples: a guitar, books
        self.assertIn("g_bookstack", mall)
        rooms = {k for ks in F["rooms"].values() for k in ks}
        self.assertFalse(rooms & set(mall))                                       # the designer's list does not offer them
        room, keep = [0.0, 0.0, 6.0, 5.0], []
        new = furnish.kit_piece(F, "g_guitar", "n1", 2.0, 2.0, 0, "plain")       # added as if bought in the designer: refused
        self.assertIn("the acoustic guitar is sold at the mall, not here", furnish.check(F, room, keep, 2.6, [], [new], 10 ** 9))
        mine = dict(furnish.kit_piece(F, "g_guitar", "b:mall:1", 0, 0, 0, "plain"))
        placed = dict(mine, x=2.0, z=2.0)                                         # the one in the belongings: placed, and free
        self.assertEqual(furnish.check(F, room, keep, 2.6, [], [placed], 0, [mine]), [])
        self.assertEqual(furnish.cost(F, [], [placed], [mine])[0], 0)
        self.assertEqual(furnish.stow([], [placed], [mine]), [])                  # and it has left the belongings
        back = furnish.stow([placed], [], [])                                     # taken away again, it is a belonging once more
        self.assertEqual([b["kind"] for b in back], ["g_guitar"])
        rug = furnish.kit_piece(F, "g_rug", "b:mall:2", 2.0, 2.0, 0, "plain")    # a rug bought at the mall lies under other pieces
        self.assertTrue(rug["rug"])
        self.assertEqual(furnish.check(F, room, keep, 2.6, [], [placed, rug], 0, [mine, rug]), [])


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
        self.assertEqual(lines, [["Move: Bed", 1, 0], ["To your belongings: Wardrobe", 1, 0]])
        total, lines = furnish.cost(F, BEFORE, after({"add": [["k1", "k17:sofa", 3.0, 2.5, 0, "curve"]]}))
        self.assertEqual(total, F["pieces"]["k17:sofa"]["price"])
        self.assertEqual(check({"add": [["k1", "k17:sofa", 3.0, 2.5, 0, "curve"]]}, funds=total - 1), ["you cannot pay %d.%02d brix" % (total // 100, total % 100)])
        self.assertEqual(check({"add": [["k1", "k17:sofa", 3.0, 2.5, 0, "curve"]]}, funds=total), [])


class TheSlicePort(unittest.TestCase):
    def test_FURN_051_a_fixed_run_for_the_slice(self):
        want = furnish.run_script(F, S)
        self.assertEqual(len(want), len(S["cases"]))
        self.assertTrue(any(r["problems"] for r in want) and any(not r["problems"] for r in want))


class Belongings(unittest.TestCase):
    MINE = S["belongings"]
    WHERE = S["where"]

    def test_INV_001_taken_away_goes_to_belongings_and_back(self):
        after_ = furnish.apply(F, BEFORE, {"remove": ["t1"]}, self.MINE)
        held = furnish.stow(BEFORE, after_, self.MINE, self.WHERE)
        ids = [b["id"] for b in held]
        self.assertIn("b:b_test_l0:0:t1", ids, "the wardrobe is kept, named by where it was")
        ward = next(b for b in held if b["id"] == "b:b_test_l0:0:t1")
        self.assertEqual(ward["from"], {"lot": "b_test_l0", "level": 0, "id": "t1"}, "a town piece remembers where it came from")
        self.assertNotIn("x", ward)
        # and placed again, here or anywhere: free, and gone from the belongings
        back = furnish.apply(F, after_, {"place": [["b:b_test_l0:0:t1", 4.5, 4.65, 0]]}, held)
        self.assertEqual(furnish.cost(F, after_, back, held), (0, [["Place: Wardrobe", 1, 0]]))
        self.assertEqual(furnish.check(F, ROOM, KEEP, 2.6, after_, back, 0, held), [])
        self.assertEqual([b["id"] for b in furnish.stow(after_, back, held, self.WHERE)], ["b:b_old_l2:1:k1"])

    def test_INV_001_only_what_you_hold_can_be_placed(self):
        bad = furnish.apply(F, BEFORE, {"place": [["b:nowhere:0:k9", 3.0, 2.5, 0]]}, self.MINE)
        self.assertEqual(furnish.check(F, ROOM, KEEP, 2.6, BEFORE, bad, None, self.MINE), ["the piece is not among your belongings"])
        ok = furnish.apply(F, BEFORE, {"place": [["b:b_old_l2:1:k1", 3.0, 2.5, 90]]}, self.MINE)
        self.assertEqual(furnish.check(F, ROOM, KEEP, 2.6, BEFORE, ok, 0, self.MINE), [], "placing what you hold costs nothing")
        self.assertEqual(furnish.stow(BEFORE, ok, self.MINE, self.WHERE), [])


if __name__ == "__main__":
    unittest.main()
