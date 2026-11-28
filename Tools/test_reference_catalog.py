"""ART-001 (ADR-0061): the catalog of look references agrees with the folders, every file is named by the rule, and the
page to review them is the one the catalog writes.        python -m unittest Tools/test_reference_catalog.py"""
import copy, os, sys, unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import reference_catalog as RC                             # noqa: E402


class TheReferences(unittest.TestCase):
    def test_ART_001_the_catalog_and_the_folders_agree(self):
        cat = RC.load()
        self.assertEqual(RC.problems(cat), [])
        self.assertEqual({c["id"] for c in cat["categories"]},
                         {"World", "Buildings", "Indoor", "Shops", "Malls", "Furniture", "Ornaments", "Vehicles", "People", "Interface"})
        self.assertIn("IndoorHomeRoom", {r["id"] for r in cat["references"]})

    def test_ART_001_a_badly_named_or_missing_file_is_caught(self):
        cat = copy.deepcopy(RC.load())
        r = next(r for r in cat["references"] if r["id"] == "VehiclesSoftBox")
        r["files"][0] = "Vehicles/soft_box.png"
        p = RC.problems(cat)
        self.assertTrue(any("is not named" in x for x in p))
        self.assertTrue(any("VehiclesSoftBox.png is in Art/Reference but not in the catalog" in x for x in p))
        cat["references"][0]["category"] = "Moon"
        self.assertTrue(any("no category Moon" in x for x in RC.problems(cat)))

    def test_ART_001_the_page_is_written_from_the_catalog(self):
        with open(RC.PAGE, encoding="utf-8") as f:
            self.assertEqual(f.read(), RC.page(RC.load()), "run python Tools/reference_catalog.py")

    def test_ART_001_every_place_in_the_game_has_its_reference_or_is_a_named_gap(self):
        cat = RC.load()
        def ref(what):
            hit, why = RC.places_for(cat, what)
            return sorted((p["place"], p.get("reference")) for p in hit), why
        # a tour stop, a lot and a place's name all lead to the same answer
        self.assertEqual(ref("row_2230")[0], [("home_hallway", "IndoorHallway")])
        self.assertEqual(ref("library_1200")[0], [("shop_floor", "ShopsShopFloor")])
        self.assertEqual(ref("b_9_6_l0")[0], [("shop_floor", "ShopsShopFloor")])                       # the bank
        self.assertEqual(ref("b_7_7_l2")[0], [("bakery", "ShopsBakeryLight")])                         # a fit-out before its kind
        self.assertEqual(ref("b_10_11_l0")[0], [("home_hallway", "IndoorHallway"), ("home_room", "IndoorHomeRoom")])   # a home is two places
        self.assertEqual(ref("ShopsBakery")[0], [("bakery", "ShopsBakeryLight")])
        # a place with no reference says so, and is never handed another place's
        self.assertEqual(ref("cafe_1200")[0], [("cafe", None)])
        self.assertEqual(ref("b_12_5_l0")[0], [("tower_hall", None)])
        self.assertTrue(any("NO REFERENCE" in l for l in RC.answer(cat, "tower_in_1200")))
        self.assertEqual(ref("b_9_9_l2")[0], [])                                                       # the museum: a kind not listed
        self.assertIn("does not list", ref("b_9_9_l2")[1])
        # the hour and the weather in a stop's name pick the file
        self.assertTrue(any("IndoorHallway_NightLamps.jpg" in l for l in RC.answer(cat, "row_2230")))
        self.assertTrue(any("WorldMarketSquare_Rain.jpg" in l for l in RC.answer(cat, "rain_1200")))
        # every stop of the look tour belongs to a place; a new one that does not is caught
        for name in RC.tour_stops():
            self.assertTrue(RC.places_for(cat, name)[0], name)
        bad = copy.deepcopy(cat)
        next(p for p in bad["places"] if p["place"] == "park")["stops"] = []
        self.assertTrue(any("park_1200 belongs to no place" in x for x in RC.problems(bad)))
        next(p for p in bad["places"] if p["place"] == "cafe")["reference"] = "OrnamentsNotApproved"
        self.assertTrue(any("is not an approved reference" in x for x in RC.problems(bad)))

    def test_ART_001_a_places_light_in_the_game_is_its_own_settings_in_its_own_folder(self):
        """ADR-0075: an "unreal" block's settings are the reference's, each shot or missing with a reason; its files
        exist, in <Category>/Unreal/; nothing there is listed as a Blender file; none is approved but in the owner's words."""
        cat = RC.load()
        blocks = [r for r in cat["references"] if r.get("unreal")]
        self.assertIn("IndoorHomeRoom", {r["id"] for r in blocks})
        for r in blocks:
            u, own = r["unreal"], RC.settings_of(r)
            self.assertEqual(RC.unreal_problems(r), [], r["id"])
            self.assertEqual([s for s in own if s in u["settings"] or s in u["missing"]], own, r["id"])
            self.assertFalse(set(u["settings"]) & set(u.get("missing", {})), r["id"])
            self.assertIn(u["blender"], r["files"])
            for f in u.get("files", []):
                self.assertTrue(f.startswith(r["category"] + "/Unreal/" + r["id"] + "_"), f)
                self.assertTrue(os.path.isfile(os.path.join(RC.REF, f)), f)
            if u.get("files"):
                self.assertEqual([os.path.splitext(f)[0].rsplit("_", 1)[-1] for f in u["files"]], list(u["settings"]), r["id"])
            if u.get("status") == "approved":
                self.assertIn('"', u.get("decision", ""), r["id"] + ": approved without the owner's words")
        for r in cat["references"]:
            self.assertFalse([f for f in r["files"] if "/Unreal/" in f], r["id"] + ": a game picture listed as a Blender file")
        # what the check catches
        bad = copy.deepcopy(next(r for r in blocks if r["id"] == "IndoorHomeRoom"))
        u = bad["unreal"]
        u["settings"]["Noon"] = "12:00"
        u["settings"]["Dusk"] = "sunset"
        del u["missing"]["NightNewMoon"]
        u["where"] = "the living room"
        u["status"] = "approved"
        u["files"] = ["Indoor/IndoorHomeRoom_Morning.jpg"]
        p = RC.unreal_problems(bad)
        for words in ("names the setting Noon, which the reference has not", "Dusk is sunset", "neither shoots the setting NightNewMoon",
                      "where (the living room)", "approved without a decision in the owner's own words",
                      "Indoor/IndoorHomeRoom_Morning.jpg is not Art/Reference/Indoor/Unreal/"):
            self.assertTrue(any(words in x for x in p), words)
        # a picture in an Unreal folder that no block lists is caught by the folders' check
        whole = copy.deepcopy(cat)
        shot = next(r for r in whole["references"] if (r.get("unreal") or {}).get("files"))
        gone = shot["unreal"]["files"].pop()
        self.assertTrue(any(gone + " is in Art/Reference but not in the catalog" in x for x in RC.problems(whole)))

    def test_ART_001_the_page_shows_one_blender_picture_and_the_games_settings(self):
        cat = RC.load()
        r = next(r for r in cat["references"] if r["id"] == "IndoorHomeRoom")
        part = RC.in_the_game(r)
        self.assertEqual(part.count("<img "), 1 + len(r["unreal"].get("files", [])))
        self.assertIn('src="Indoor/IndoorHomeRoom_Midday.jpg"', part)
        self.assertNotIn('src="Indoor/IndoorHomeRoom_Dusk.jpg"', part)          # kept and linked, not shown
        self.assertIn('href="Indoor/IndoorHomeRoom_Dusk.jpg"', part)
        self.assertIn("Not in the game yet", part)
        self.assertIn("Night, lamps off, full moon", part)
        self.assertIn("For your review", part)
        self.assertEqual(RC.state(r)[1], "review")
        self.assertNotIn("Approved</em>", part)
        # a reference without the block is drawn as before: every one of its files
        plain = next(r for r in cat["references"] if not r.get("unreal") and len(r["files"]) > 1)
        self.assertEqual(RC.shown(plain), plain["files"])
        # the look check's failures are under their picture
        tried = copy.deepcopy(r)
        tried["unreal"]["checks"] = {"Midday": ["burnt whites over 9% of the picture"]}
        self.assertIn("burnt whites over 9% of the picture", RC.in_the_game(tried))


if __name__ == "__main__":
    unittest.main()
