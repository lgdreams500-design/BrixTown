"""Lights and their kinds (F-LIGHTS level 2; ADR-0065).
        python -m unittest test_lights -v

Each test names the requirement it proves (docs/specs/domains/FURNITURE.md 2.5)."""
import json, os, unittest

import lights as li

L = li.load_rules()
with open(os.path.join(li.RULES, "furnishing.json"), encoding="utf-8") as f:
    PIECES = json.load(f)["pieces"]


class Lights(unittest.TestCase):
    def test_FURN_066_light_kinds_are_checked(self):
        self.assertEqual(li.problems(L, PIECES), [])
        for mount in ("floor", "table", "wall", "ceiling"):              # every way of mounting has a kind
            self.assertTrue(any(v["mount"] == mount for v in L["kinds"].values()), mount)
        bad = json.loads(json.dumps(L))
        bad["colours"]["warm_white"]["colour"] = [0.2, 0.6, 1.0]           # a warm white bluer than a cool white
        self.assertTrue(any("bluer" in p for p in li.problems(bad)))
        bad["kinds"]["candles"]["colour"] = "candlelight"                  # a colour nobody named
        self.assertTrue(any("no colour" in p for p in li.problems(bad)))
        # fittings share named colours: the lamps of a home one warm white, every fire one red
        for a, b in (("floor_lamp", "table_lamp"), ("table_lamp", "ceiling_pendant"), ("wall_light", "ceiling_pendant")):
            self.assertEqual(li.colour_of(L, a), li.colour_of(L, b))
        self.assertEqual(li.colour_of(L, "candles")["name"], "Fire red")
        self.assertNotEqual(li.colour_of(L, "ceiling_light"), li.colour_of(L, "cool_ceiling_light"))
        self.assertTrue(all(c["name"] for c in L["colours"].values()))     # every colour has a name
        used = set(v["colour"] for v in L["kinds"].values())
        self.assertEqual(used, set(L["colours"]))                          # and a fitting that shows it
        bad["kinds"]["floor_lamp"]["mount"] = "roof"
        self.assertTrue(any("no mount" in p for p in li.problems(bad)))

    def test_FURN_067_lamps_take_their_kinds_colour(self):
        for name, kind in (("FLamp_Light", "floor_lamp"), ("Pen_Light.002", "ceiling_pendant"), ("Cd_Light", "candles")):
            self.assertEqual(li.kind_of_light(L, name), L["kinds"][kind])
        self.assertIsNone(li.kind_of_light(L, "Fp_Light"))                 # a fire is not a lamp
        self.assertIsNone(li.kind_of_light(L, "FLamp_LightX"))
        for p, k in L["pieces"].items():                                   # every lamp-carrying piece is a known piece
            self.assertIn(p, PIECES)
            self.assertIn(k, L["kinds"])

    def test_FURN_068_each_kind_stands_where_it_is_mounted(self):
        ok = lambda *a, **k: li.can_place(L, *a, **k)[0]
        self.assertTrue(ok("floor_lamp", "floor"))
        self.assertFalse(ok("floor_lamp", "wall", 1.8))
        self.assertTrue(ok("table_lamp", "top"))
        self.assertTrue(ok("table_lamp", "floor"))
        self.assertFalse(ok("table_lamp", "ceiling"))
        self.assertTrue(ok("wall_light", "wall", 1.8))
        self.assertTrue(ok("ceiling_light", "ceiling", 0.0))
        self.assertTrue(ok("ceiling_pendant", "ceiling", 0.3, room_height=3.0))
        self.assertFalse(ok("nonesuch", "floor"))

    def test_FURN_070_placement_keeps_clear(self):
        ok = lambda *a, **k: li.can_place(L, *a, **k)[0]
        self.assertFalse(ok("wall_light", "wall", 0.9))                    # too low
        self.assertFalse(ok("wall_light", "wall", 2.6))                    # too high
        self.assertFalse(ok("wall_light", "wall", 1.8, from_opening=0.05))  # on a window's edge
        self.assertFalse(ok("ceiling_pendant", "ceiling", 0.9, room_height=2.6))   # would hang into a head
        self.assertFalse(ok("ceiling_pendant", "ceiling", 1.2))           # hangs further than a pendant may


if __name__ == "__main__":
    unittest.main()
