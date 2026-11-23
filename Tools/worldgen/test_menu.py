"""The game menu, its settings and the wardrobe (docs/specs/domains/MENU.md UI-005 to UI-010, CHR-014, CHR-015).   python -m unittest test_menu"""
import unittest

import world

W = world.load()
S = W["settings"]


class TheMenu(unittest.TestCase):
    def test_UI_005_every_tab_opens_from_a_control_on_every_platform(self):
        ids = [t["id"] for t in S["menu"]["tabs"]]
        for want in ("me", "wallet", "belongings", "property", "quests", "wardrobe", "storage", "map", "settings", "save", "help"):   # (me and storage: level 2, UI-011, UI-014)
            self.assertIn(want, ids)
        for p, P in S["platforms"].items():
            for t in S["menu"]["tabs"]:
                self.assertTrue(P["controls"].get(t["action"]), "%s: no control opens %s" % (p, t["name"]))
        self.assertTrue(S["menu"]["pauses"], "the menu pauses the clock while the slice is single-player (owner, 2026-11-23)")


    def test_UI_013_more_than_one_save_slot(self):
        self.assertGreaterEqual(S["save"]["slots"], 3, "level 2: more than one save slot (owner, 2026-11-23: build the game menu level 2)")

    def test_UI_012_the_map_has_filters_and_a_mark(self):
        M = S["groups"]["map"]["settings"]
        for k in ("map_people", "map_quests", "map_mine", "map_shops", "map_waypoint"):
            self.assertEqual(M[k]["type"], "switch", k)
        for p, P in S["platforms"].items():
            self.assertTrue(P["controls"].get("mark"), "%s: nothing marks a place on the map" % p)


class TheSettings(unittest.TestCase):
    def test_UI_009_settings_are_valid(self):
        self.assertEqual(world.settings_problems(W), [])
        for g in ("graphics", "audio", "display", "controls"):
            self.assertIn(g, S["groups"])

    def test_UI_009_a_bad_setting_is_caught(self):
        import copy
        w = copy.deepcopy(W)
        w["settings"]["groups"]["graphics"]["settings"]["field_of_view"]["default"] = 200
        w["settings"]["groups"]["audio"]["settings"]["sound"]["default"] = "loud"
        got = world.settings_problems(w)
        self.assertTrue(any("field_of_view" in p for p in got))
        self.assertTrue(any("audio.sound" in p for p in got))


class TheControls(unittest.TestCase):
    def test_UI_008_every_platform_maps_every_action(self):
        self.assertEqual(sorted(S["platforms"]), ["gamepad", "keyboard", "touch"])
        for p, P in S["platforms"].items():
            for a in S["actions"]:
                self.assertTrue(P["controls"].get(a), "%s has nothing for %s" % (p, a))

    def test_UI_010_no_control_does_two_things(self):
        import copy
        w = copy.deepcopy(W)
        w["settings"]["platforms"]["keyboard"]["controls"]["map"] = ["E"]       # E is used on foot; the map is anywhere
        self.assertTrue(any(p.startswith("keyboard: E does") and "map" in p for p in world.settings_problems(w)))
        w = copy.deepcopy(W)
        w["settings"]["platforms"]["gamepad"]["controls"]["drive"] = ["A"]      # A is the handbrake in a car
        self.assertTrue(any(p.startswith("gamepad: A does") for p in world.settings_problems(w)))


class TheWardrobe(unittest.TestCase):
    def outfit(self, **ch):
        o = {k: W["designer"]["default"][k] for k in S["wardrobe"]["parts"]}
        o["name"] = "Work"; o.update(ch)
        return o

    def test_CHR_014_an_outfit_is_only_clothes_and_hair_the_designer_offers(self):
        self.assertEqual(world.outfit_problems(W, self.outfit()), [])
        self.assertEqual(world.outfit_problems(W, self.outfit(garment="coat", top="plum", cap="slate")), [])
        self.assertIn("garment cape is not offered", world.outfit_problems(W, self.outfit(garment="cape")))
        self.assertIn("an outfit cannot change height", world.outfit_problems(W, self.outfit(height=2.0)))
        self.assertIn("an outfit needs a name of 1 to %d letters" % W["designer"]["name"]["max"], world.outfit_problems(W, self.outfit(name="")))

    def test_CHR_015_the_wardrobe_holds_a_few_outfits(self):
        self.assertGreaterEqual(S["wardrobe"]["max_outfits"], 3)
        for k in ("garment", "top", "bottom", "shoes", "hair"):
            self.assertIn(k, S["wardrobe"]["parts"])
        for k in W["designer"]["sliders"]:
            self.assertNotIn(k, S["wardrobe"]["parts"], "the body is not a garment")


if __name__ == "__main__":
    unittest.main()
