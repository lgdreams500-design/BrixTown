"""The room plan of a storey (docs/specs/domains/BUILDER.md BUILD-021 to BUILD-023).   python -m unittest test_roomplan"""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import roomplan  # noqa: E402
import world  # noqa: E402

W = world.load()
P = W["room_plan"]
S = roomplan.check_script()


def check(edit, funds=None):
    return roomplan.check(P, S["room"], S["keep"], S["entries"], S["height"], S["colours"], S["before"], roomplan.apply(S["before"], edit), funds)


def wall(i, ax, fix, a0, a1, door=None, colour=None):
    return {"id": i, "ax": ax, "fix": fix, "a0": a0, "a1": a1, "door": door, "colour": colour}


class Walls(unittest.TestCase):
    def test_BUILD_021_walls_move_come_and_go(self):
        self.assertEqual(check({"set": {"w1": {"fix": 5.5}}}), [])
        self.assertEqual(check({"remove": ["w2"]}), [])
        self.assertEqual(check({"add": [wall("w3", "x", 2.0, 5.0, 8.0, 6.5)]}), [], "a new wall with a doorway")
        self.assertEqual(check({"add": [wall("w3", "z", 2.0, 0.0, 0.4)]}), ["a wall is shorter than 0.6 m"])
        self.assertIn("a wall is off the 5 cm grid", check({"add": [wall("w3", "z", 2.03, 3.0, 6.0)]}))
        self.assertEqual(check({"set": {"w2": {"door": 0.3}}}), ["a doorway does not fit in its wall"])
        self.assertEqual(check({"add": [wall("w3", "z", 5.0, 3.5, 6.0)]}), ["two walls stand in each other"])
        self.assertIn("a wall runs out of the room", check({"add": [wall("w3", "x", 2.0, 6.0, 9.0)]}))

    def test_BUILD_021_every_room_is_reached_and_the_stairs_kept_clear(self):
        self.assertEqual(check({"set": {"w1": {"door": None}}}), ["the stairs cannot be reached from the way in", "a room cannot be reached from the way in"])
        got = check({"add": [wall("w3", "z", 4.0, 0.0, 2.0)]})
        self.assertIn("a wall stands in the way of the door or the stairs", got)
        self.assertIn("a wall stands where you come in", got)
        self.assertEqual(check({"set": {"w2": {"door": None}}}), [], "the back left is still reached through the other doorway")

    def test_BUILD_021_reach_finds_closed_rooms(self):
        got, pockets = roomplan.reach(P, S["room"], [wall("a", "z", 5.0, 0.0, 6.0)], S["entries"])
        self.assertEqual(got, [True, False])
        self.assertAlmostEqual(pockets[0], 3.0 * 6.0 - 0.25 * 6.0, places=4)   # the part beyond the wall, less the cells it stands in


class Paint(unittest.TestCase):
    def test_BUILD_022_each_wall_its_own_colour_from_the_district(self):
        self.assertEqual(check({"set": {"w2": {"colour": "apricot"}}}), [])
        self.assertEqual(check({"set": {"w2": {"colour": "spaceship"}}}), ["that wall colour is not one of the district's"])
        total, lines = roomplan.cost(P, S["before"], roomplan.apply(S["before"], {"set": {"w2": {"colour": "apricot"}}}), 2.8)
        self.assertEqual(lines, [["Paint a wall", 28.0, 28 * P["prices"]["paint_m2"]]], "both faces of 5 m by 2.8 m")


class Paying(unittest.TestCase):
    def test_BUILD_023_priced_line_by_line_and_paid_or_refused(self):
        total, lines = roomplan.cost(P, S["before"], roomplan.apply(S["before"], {"remove": ["w2"], "set": {"w1": {"door": None}}}), 2.8)
        self.assertEqual([l[0] for l in lines], ["Close a doorway", "Take down a wall"])
        self.assertEqual(total, P["prices"]["close_doorway"] + round(14.0 * P["prices"]["take_down_m2"]))
        new = {"add": [wall("w3", "x", 4.0, 5.0, 8.0, 7.0, "putty")]}
        need = roomplan.cost(P, S["before"], roomplan.apply(S["before"], new), 2.8)[0]
        self.assertEqual(check(new, need), [])
        self.assertEqual(check(new, need - 1), ["you cannot pay %d.%02d brix" % (need // 100, need % 100)])

    def test_BUILD_023_a_fixed_run_for_the_slice(self):
        want = roomplan.run_script(P, S)
        self.assertEqual(len(want), len(S["cases"]))


if __name__ == "__main__":
    unittest.main()
