"""ART-001: a room's light is read on its grey card, Unreal against Unreal (ADR-0073; docs/process/LOOK_GATE.md, A
room's light).        python -m unittest Tools/unreal/test_card_read.py"""
import os, sys, tempfile, unittest

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import card_read as CR                                     # noqa: E402

CARD, BALL = (0.40, 0.40, 0.50, 0.60), (0.65, 0.50, 0.05)  # shares of the picture, as the game's log gives them


def shot(path, card, ball_left, ball_right, around=(20, 20, 20)):
    """A picture 400 x 250 with a card and a ball where CARD and BALL say, the ball's two halves in two tones."""
    im = Image.new("RGB", (400, 250), around)
    d = ImageDraw.Draw(im)
    d.rectangle([160, 100, 200, 150], fill=card)
    d.pieslice([240, 105, 280, 145], 90, 270, fill=ball_left)
    d.pieslice([240, 105, 280, 145], 270, 90, fill=ball_right)
    im.save(path)


class TheCard(unittest.TestCase):
    def test_ART_001_the_card_and_ball_are_read_where_the_log_says(self):
        with tempfile.TemporaryDirectory() as d:
            shot(os.path.join(d, "golden_home_ref.png"), (119, 119, 119), (60, 60, 60), (160, 160, 160))
            Image.new("RGB", (400, 250), (90, 90, 90)).save(os.path.join(d, "square_1200.png"))   # a stop with no card
            with open(os.path.join(d, "tour.log"), "w") as f:
                f.write("LogTemp: Display: Brix Town: tour stop golden_home_ref: card %s ball %s\n" % (",".join("%.4f" % v for v in CARD), ",".join("%.4f" % v for v in BALL)))
                f.write("LogTemp: Display: Brix Town: tour stop square_1200 at 12:00, weather -\n")
            got = CR.folder_readings(d)
        self.assertEqual(list(got), ["golden_home_ref"])     # only a stop whose card the log places is read
        r = got["golden_home_ref"]
        self.assertAlmostEqual(r["card"], 0.184, delta=0.01)  # sRGB 119 is the photographer's mid grey; the dark surround is left out
        self.assertAlmostEqual(r["card_warm"], 0.0, delta=0.01)
        self.assertGreater(r["ball_side"], 2.0)               # the ball's right half is the lit one

    def test_ART_001_brighter_and_warmer_cards_are_told_and_the_same_card_passes(self):
        with tempfile.TemporaryDirectory() as d:
            paths = [os.path.join(d, n) for n in ("a.png", "b.png", "c.png")]
            shot(paths[0], (100, 100, 100), (80, 80, 80), (80, 80, 80))
            shot(paths[1], (150, 150, 150), (80, 80, 80), (80, 80, 80))
            shot(paths[2], (120, 100, 80), (80, 80, 80), (80, 80, 80))
            a, b, c = (CR.read(p, CARD, BALL) for p in paths)
        self.assertTrue(CR.between(a, a)["passes"])
        brighter, warmer = CR.between(b, a), CR.between(c, a)
        self.assertGreater(brighter["bright"], 1.0)          # more than a stop brighter: fails
        self.assertFalse(brighter["passes"])
        self.assertGreater(warmer["warm"], CR.PASS_WARM)     # warmer than the mark: fails though as bright
        self.assertFalse(warmer["passes"])


if __name__ == "__main__":
    unittest.main()
