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

    def test_UI_015_every_tab_is_in_a_group(self):
        groups = [g["id"] for g in S["menu"]["groups"]]
        self.assertEqual(groups, ["you", "town", "game"])
        for t in S["menu"]["tabs"]:
            self.assertIn(t["group"], groups, t["id"])
        for g in groups:
            self.assertTrue([t for t in S["menu"]["tabs"] if t["group"] == g], "group %s is empty" % g)
        import copy
        w = copy.deepcopy(W); w["settings"]["menu"]["tabs"][0]["group"] = "nowhere"
        self.assertTrue(any("no group nowhere" in p for p in world.settings_problems(w)))

    def test_UI_016_every_tab_has_a_board_colour_that_can_be_read(self):
        C = S["menu"]["colours"]
        lum = lambda c: sum(w * ((v / 255 / 12.92) if v / 255 <= .03928 else ((v / 255 + .055) / 1.055) ** 2.4) for w, v in zip((.2126, .7152, .0722), (int(c[i:i + 2], 16) for i in (1, 3, 5))))
        for k, (board, ink) in C.items():
            a, b = sorted((lum(board), lum(ink)), reverse=True)
            self.assertGreaterEqual((a + .05) / (b + .05), 3.0, "%s: lettering too faint on its board" % k)   # large lettering (WCAG AA large)
        for t in S["menu"]["tabs"]:
            self.assertIn(t["colour"], C, t["id"])
        import copy
        w = copy.deepcopy(W); w["settings"]["menu"]["tabs"][1]["colour"] = "pink"
        self.assertTrue(any("no colour pink" in p for p in world.settings_problems(w)))

    def test_UI_017_the_menu_shows_the_player_themself(self):
        L = S["menu"]["live"]
        self.assertLess(L["face_m"], 1.0, "the face window shows a face, not a body")
        self.assertGreater(L["body_m"], 1.72, "the wardrobe shows all of a person 1.72 m tall")
        self.assertGreater(L["face_y"], L["body_y"])
        import copy
        w = copy.deepcopy(W); w["settings"]["menu"]["live"]["face_d"] = 0
        self.assertTrue(any("live.face_d" in p for p in world.settings_problems(w)))
        w = copy.deepcopy(W); w["settings"]["menu"]["live"]["body_fill"] = 1.5
        self.assertTrue(any("live.body_fill" in p for p in world.settings_problems(w)))

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
