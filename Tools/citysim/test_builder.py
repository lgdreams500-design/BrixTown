"""The building designer's rules (docs/specs/domains/BUILDER.md BUILD-004 to BUILD-007).   python -m unittest test_builder"""
import os
import statistics
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import builder  # noqa: E402
import estate  # noqa: E402
import world  # noqa: E402

W = world.load()
D = world.designs(W)
K, PL = W["building_kit"], W["planning"]
LOT = {l["id"]: (b, l) for b in W["blocks"] for l in b["lots"]}
WHOLE = {"whole": True}


def first(district, kind="home", floors=2, plastered=True):
    return next(i for i, (b, l) in LOT.items() if b["district"] == district and l["kind"] == kind and l["floors"] >= floors and i in D and not builder.civic(W, b, l)
                and (not plastered or not (D[i]["walls"]["stone_to_eaves"] or D[i]["walls"]["brick_to_eaves"])))


def other(lst, v):
    return next(x for x in lst if x != v)


class TheRules(unittest.TestCase):
    def test_BUILD_007_the_town_keeps_its_own_district_rules(self):
        for i, d in D.items():
            b, l = LOT[i]
            pal = PL["districts"][b["district"]]
            st = d["walls"]["stone"]["pattern"]
            self.assertTrue(st in pal["stone"] or (st in PL["civic_only"]["stone"] and builder.civic(W, b, l)), (i, st))
            self.assertIn(K["plaster"].index(d["walls"]["plaster"]), pal["plaster"], i)
            self.assertIn(K["roof_tiles"].index(d["roof"]["tiles"]), pal["roof_tiles"], i)

    def test_BUILD_007_a_district_offers_only_its_own_palette(self):
        old_town, outskirts = first("Old Town"), first("Outskirts")
        p = builder.check(W, old_town, D[old_town], builder.remodel(D[old_town], **{"walls.stone.pattern": "brick_running", "walls.stone.color": None}), WHOLE)
        self.assertTrue(any("not one of Old Town's stones or bricks" in x for x in p), p)
        p = builder.check(W, outskirts, D[outskirts], builder.remodel(D[outskirts], **{"walls.stone.pattern": "rubble", "walls.stone.color": None}), WHOLE)
        self.assertTrue(any("not one of Outskirts's" in x for x in p), p)
        p = builder.check(W, old_town, D[old_town], builder.remodel(D[old_town], **{"walls.stone.pattern": "rusticated", "walls.stone.color": None}), WHOLE)
        self.assertTrue(any("kept for the town's civic buildings" in x for x in p), p)
        bank = next(i for i, (b, l) in LOT.items() if l["kind"] == "bank")
        d = D[bank]
        self.assertEqual(builder.check(W, bank, d, builder.remodel(d, **{"walls.stone.color": other(W["surfaces"]["patterns"]["rusticated"]["colors"], d["walls"]["stone"]["color"])}), WHOLE), [])

    def test_BUILD_006_a_remodel_is_checked(self):
        i = first("Outskirts")
        d = D[i]
        good = builder.remodel(d, **{"walls.plaster": other(K["plaster"], d["walls"]["plaster"]), "roof.tiles": other(K["roof_tiles"], d["roof"]["tiles"])})
        self.assertEqual(builder.check(W, i, d, good, WHOLE), [])
        self.assertEqual(builder.check(W, i, d, d, WHOLE), ["nothing has changed"])
        bad = builder.remodel(d, **{"shell.floors": 5, "walls.plaster": [1, 0, 1], "openings.shutter_color": [0.1, 0.9, 0.1]})
        p = builder.check(W, i, d, bad, WHOLE)
        for bit in ("a remodel cannot change shell.floors", "plaster colour not in the kit", "that shutter colour is not for these windows"):
            self.assertTrue(any(bit in x for x in p), (bit, p))
        total, _ = builder.cost(W, i, d, good)
        self.assertTrue(any("it costs" in x for x in builder.check(W, i, d, good, WHOLE, funds=total - 1)))
        self.assertEqual(builder.check(W, i, d, good, WHOLE, funds=total), [])

    def test_BUILD_004_who_may_change_what(self):
        i = first("Outskirts", floors=2)
        d = D[i]
        outside = builder.remodel(d, **{"walls.plaster": other(K["plaster"], d["walls"]["plaster"])})
        self.assertIn("changing the outside needs the whole building", builder.check(W, i, d, outside, {"floors": [0, 1]}))
        floors = [list(f) for f in d["inside"]["floors"]]
        floors[1] = [16, [0.58, 0.52, 0.44]] if floors[1] != [16, [0.58, 0.52, 0.44]] else [15, [0.4, 0.27, 0.17]]
        inside = builder.remodel(d, **{"inside.floors": floors})
        self.assertEqual(builder.check(W, i, d, inside, {"floors": [1]}), [])
        self.assertIn("floor 1 is not yours to change", builder.check(W, i, d, inside, {"floors": [0]}))


class TheCost(unittest.TestCase):
    def test_BUILD_005_the_cost_is_known_line_by_line(self):
        i = first("Old Town")
        d = D[i]
        new = builder.remodel(d, **{"walls.plaster": other(K["plaster"], d["walls"]["plaster"]), "openings.frame": other(K["frames"], d["openings"]["frame"])})
        total, lines = builder.cost(W, i, d, new)
        self.assertEqual(total, sum(c for _, _, c in lines))
        self.assertEqual([w for w, _, _ in lines], ["Repaint the plaster", "Paint the window frames", "Permit"])
        self.assertEqual(builder.cost(W, i, d, d), (0, []))

    def test_BUILD_005_a_full_remodel_is_about_a_quarter_of_a_house(self):
        # Q-39, the owner: "Expensive", about 25% of value; a repaint about 60 brix
        R = W["estate"]
        value = {u["lot_id"]: u["area"] * R["kinds"]["house"]["per_m2"] * u["loc_bp"] // 10000 * u["floor_bp"] // 10000
                 for u in estate.carve(R, world.lots(W), W["residents"]) if u["kind"] == "house"}
        shares, repaints = [], []
        for i, v in value.items():
            d = D[i]
            full = builder.remodel(d, **{"walls.plaster": other(K["plaster"], d["walls"]["plaster"]), "roof.tiles": other(K["roof_tiles"], d["roof"]["tiles"]),
                                         "openings.frame": other(K["frames"], d["openings"]["frame"]),
                                         "inside.floors": [[16, [0.58, 0.52, 0.44]] if f != [16, [0.58, 0.52, 0.44]] else [15, [0.4, 0.27, 0.17]] for f in d["inside"]["floors"]],
                                         "walls.stone.pattern": "ashlar" if d["walls"]["stone"]["pattern"] != "ashlar" else "brick_running"})
            shares.append(builder.cost(W, i, d, full)[0] / v)
            if not (d["walls"]["stone_to_eaves"] or d["walls"]["brick_to_eaves"]):
                repaints.append(builder.cost(W, i, d, builder.remodel(d, **{"walls.plaster": other(K["plaster"], d["walls"]["plaster"])}))[0] / 100)
        self.assertGreater(len(shares), 300)
        self.assertTrue(.2 <= statistics.median(shares) <= .3, statistics.median(shares))
        self.assertTrue(40 <= statistics.median(repaints) <= 90, statistics.median(repaints))


if __name__ == "__main__":
    unittest.main()
