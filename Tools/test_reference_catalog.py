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
                         {"World", "Buildings", "Indoor", "Shops", "Malls", "Furniture", "Vehicles", "People", "Interface"})
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


if __name__ == "__main__":
    unittest.main()
