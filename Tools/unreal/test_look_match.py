"""ART-001: a tour's shots are measured against their own place's reference (docs/process/LOOK_GATE.md, The matching
round).        python -m unittest Tools/unreal/test_look_match.py"""
import os, sys, tempfile, unittest

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import look_match as LM                                    # noqa: E402
RC = LM.RC


class TheMatch(unittest.TestCase):
    def test_ART_001_a_shot_is_measured_against_its_own_reference(self):
        cat = RC.load()
        ref = os.path.join(RC.REF, "Shops", "ShopsBakeryLight_Midday.jpg")
        with tempfile.TemporaryDirectory() as d:
            Image.open(ref).save(os.path.join(d, "bakery_1200.png"))                       # the reference itself, as a shot
            Image.open(ref).point(lambda v: min(255, int(v * 1.5))).save(os.path.join(d, "library_1200.png"))
            Image.new("RGB", (64, 40), (90, 90, 90)).save(os.path.join(d, "cafe_1200.png"))
            Image.new("RGB", (64, 40), (90, 90, 90)).save(os.path.join(d, "room_0900.png"))
            rows = {r["stop"]: r for r in LM.judge(d, cat)}
        b = rows["bakery_1200"]                              # through the reference's camera: square by square
        self.assertEqual((b["place"], b["how"], b["reference"]), ("bakery", "matched", "Shops/ShopsBakeryLight_Midday.jpg"))
        self.assertIsNotNone(b["worst"])
        lib = rows["library_1200"]                           # its own reference, not the bakery's; the whole picture only
        self.assertEqual((lib["place"], lib["how"], lib["reference"]), ("shop_floor", "loose", "Shops/ShopsShopFloor_Midday.jpg"))
        self.assertEqual(rows["cafe_1200"]["how"], "none")   # no reference: not judged, never lent another place's
        self.assertEqual(rows["room_0900"]["how"], "none")   # an hour tied to none of the reference's files

    def test_ART_001_brighter_and_warmer_are_told_and_the_same_picture_passes(self):
        with tempfile.TemporaryDirectory() as d:
            a, b, c = (os.path.join(d, n) for n in ("a.png", "b.png", "c.png"))
            Image.new("RGB", (96, 60), (100, 100, 100)).save(a)
            Image.new("RGB", (96, 60), (150, 150, 150)).save(b)
            Image.new("RGB", (96, 60), (130, 100, 70)).save(c)
            same, brighter, warmer = LM.compare(a, a, 1.0), LM.compare(a, b), LM.compare(a, c)
        self.assertLess(same["bright"] + same["warm"], 0.01)
        self.assertGreater(brighter["bright"], 1.0)
        self.assertLess(abs(brighter["warm"]), 0.01)
        self.assertGreater(warmer["warm"], LM.PASS_WARM)


if __name__ == "__main__":
    unittest.main()
