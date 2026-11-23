"""The building designer's rules (docs/specs/domains/BUILDER.md BUILD-004 to BUILD-007, BUILD-016 to BUILD-019).   python -m unittest test_builder"""
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
UNITS = estate.carve(W["estate"], world.lots(W), W["residents"])


def plastered(d):
    return not any(d["walls"][f]["stone_to_eaves"] or d["walls"][f]["brick_to_eaves"] for f in "nsew")


def first(district, kind="home", floors=2, plain=True):
    return next(i for i, (b, l) in LOT.items() if b["district"] == district and l["kind"] == kind and l["floors"] >= floors and i in D
                and not builder.civic(W, b, l) and not l.get("house") and (not plain or plastered(D[i])))


def other(lst, v):
    return next(x for x in lst if x != v)


def other_floor(x):
    return "sand_tiles" if x != "sand_tiles" else "dark_boards"


class TheRules(unittest.TestCase):
    def test_BUILD_007_the_town_keeps_its_own_district_rules(self):
        for i, d in D.items():
            b, l = LOT[i]
            pal = PL["districts"][b["district"]]
            for f in "nsew":
                st = d["walls"][f]["stone"]["pattern"]
                self.assertTrue(st in pal["stone"] or (st in PL["civic_only"]["stone"] and builder.civic(W, b, l)), (i, st))
                self.assertIn(d["walls"][f]["plaster"], pal["plaster"], i)
            self.assertIn(d["roof"]["tiles"], pal["roof_tiles"], i)

    def test_BUILD_007_a_district_offers_only_its_own_palette(self):
        old_town, outskirts = first("Old Town"), first("Outskirts")
        p = builder.check(W, old_town, D[old_town], builder.remodel(D[old_town], **{"walls.*.stone": {"pattern": "brick_running", "color": None}}), WHOLE)
        self.assertTrue(any("not one of Old Town's stones or bricks" in x for x in p), p)
        p = builder.check(W, outskirts, D[outskirts], builder.remodel(D[outskirts], **{"walls.n.stone": {"pattern": "rubble", "color": None}}), WHOLE)
        self.assertTrue(any("not one of Outskirts's" in x for x in p), p)
        p = builder.check(W, old_town, D[old_town], builder.remodel(D[old_town], **{"walls.*.stone": {"pattern": "rusticated", "color": None}}), WHOLE)
        self.assertTrue(any("kept for the town's civic buildings" in x for x in p), p)
        bank = next(i for i, (b, l) in LOT.items() if l["kind"] == "bank")
        d = D[bank]
        self.assertEqual(builder.check(W, bank, d, builder.remodel(d, **{"walls.*.stone.color": 1 - (d["walls"]["n"]["stone"]["color"] or 0)}), WHOLE), [])
        menu = builder.options(W, old_town, D[old_town])
        self.assertEqual({s["id"] for s in menu["walls.*.stone"]}, set(PL["districts"]["Old Town"]["stone"]))

    def test_BUILD_006_a_remodel_is_checked(self):
        i = first("Outskirts")
        d = D[i]
        good = builder.remodel(d, **{"walls.*.plaster": other(K["plaster"], d["walls"]["n"]["plaster"]), "roof.tiles": other(K["roof_tiles"], d["roof"]["tiles"])})
        self.assertEqual(builder.check(W, i, d, good, WHOLE), [])
        self.assertEqual(builder.check(W, i, d, d, WHOLE), ["nothing has changed"])
        bad = builder.remodel(d, **{"shell.floors": 5, "walls.e.plaster": "magenta", "openings.shutter_color": "oxblood"})
        p = builder.check(W, i, d, bad, WHOLE)
        for bit in ("a remodel cannot change shell.floors", "e facade's plaster colour not in the kit", "that shutter colour is not for these windows"):
            self.assertTrue(any(bit in x for x in p), (bit, p))
        total, _ = builder.cost(W, i, d, good)
        self.assertTrue(any("it costs" in x for x in builder.check(W, i, d, good, WHOLE, funds=total - 1)))
        self.assertEqual(builder.check(W, i, d, good, WHOLE, funds=total), [])

    def test_BUILD_004_who_may_change_what(self):
        i = first("Outskirts", floors=2)
        d = D[i]
        outside = builder.remodel(d, **{"walls.n.plaster": other(K["plaster"], d["walls"]["n"]["plaster"])})
        self.assertIn("changing the outside needs the whole building", builder.check(W, i, d, outside, {"floors": [0, 1]}))
        floors = list(d["inside"]["floors"])
        floors[1] = other_floor(floors[1])
        inside = builder.remodel(d, **{"inside.floors": floors})
        self.assertEqual(builder.check(W, i, d, inside, {"floors": [1]}), [])
        self.assertIn("floor 1 is not yours to change", builder.check(W, i, d, inside, {"floors": [0]}))


class TheKit(unittest.TestCase):
    def test_BUILD_016_designs_name_their_choices_by_id(self):
        C = K["colours"]
        for slot in ("plaster", "roof_tiles", "accents", "shutters_old", "frames"):
            self.assertEqual(len(K[slot]), len(set(K[slot])), slot)                      # each colour offered once
            self.assertTrue(set(K[slot]) <= set(C), slot)
            self.assertEqual(set(K["town"][slot]), set(K[slot]), slot)                  # the town picks from the same colours
        for cid, c in C.items():
            self.assertTrue(c["name"] and len(c["rgb"]) == 3, cid)
        rgbs = [tuple(c["rgb"]) for c in C.values()]
        self.assertEqual(len(rgbs), len(set(rgbs)))                                     # one id per colour
        d = D[first("Outskirts")]
        self.assertIn(d["walls"]["n"]["plaster"], C)
        self.assertIn(d["inside"]["floors"][0], K["floor_finishes"])
        bd = world.built(W, d)                                                           # the slice's copy: ids made colours
        self.assertEqual(bd["walls"]["n"]["plaster"], C[d["walls"]["n"]["plaster"]]["rgb"])
        self.assertEqual(bd["inside"]["floors"][0][0], K["floor_finishes"][d["inside"]["floors"][0]]["mat"])
        self.assertEqual(bd["openings"]["gap"], K["windows"][d["openings"]["class"]]["gap"])

    def test_BUILD_017_only_approved_parts_for_the_building(self):
        import copy
        for cat in ("roofs", "tiles", "windows", "openings", "ornaments"):
            for k, it in K[cat].items():
                self.assertEqual(it["status"], "approved", (cat, k))
                self.assertTrue(os.path.exists(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "blender", it["look"])), (cat, k, it["look"]))
        i = first("Outskirts")
        d = D[i]
        self.assertTrue(any("stone cornice is not for this building" in x for x in world.design_problems(W, {i: builder.remodel(d, **{"roof.type": "cornice"})})))
        w = copy.deepcopy(W)
        w["building_kit"]["ornaments"]["lantern"]["status"] = "proposed"                # a part with no approved look is neither offered nor allowed
        self.assertNotIn("ornament.lantern", builder.options(w, i, d))
        if not d["ornament"]["lantern"]:
            p = builder.check(w, i, d, builder.remodel(d, **{"ornament.lantern": True}), WHOLE)
            self.assertTrue(any("wall lantern is not approved" in x for x in p), p)
        menu = builder.options(W, i, d)
        for part in PL["remodel"]["outside"] + PL["remodel"]["inside"]:
            self.assertIn(part.replace(".color", ""), set(menu) | {"walls.*.brick_to_eaves"}, part)
        for opt in menu["walls.*.plaster"] + menu["roof.tiles"]:                       # everything offered passes the checks
            path = "walls.*.plaster" if opt["id"] in K["plaster"] else "roof.tiles"
            new = builder.remodel(d, **{path: opt["id"]})
            if new != d:
                self.assertEqual(builder.check(W, i, d, new, WHOLE), [], opt)


class TheFacades(unittest.TestCase):
    def test_BUILD_018_walls_are_chosen_per_facade(self):
        i = first("Outskirts", floors=2)
        d = D[i]
        side = d["shell"]["side"]
        back = {"n": "s", "s": "n", "e": "w", "w": "e"}[side]
        new = builder.remodel(d, **{"walls.%s.plaster" % side: other(K["plaster"], d["walls"][side]["plaster"])})
        self.assertEqual(builder.check(W, i, d, new, WHOLE), [])
        self.assertEqual(new["walls"][back], d["walls"][back])
        one, _ = builder.cost(W, i, d, new)
        all4, _ = builder.cost(W, i, d, builder.remodel(d, **{"walls.*.plaster": other(K["plaster"], d["walls"][side]["plaster"])}))
        self.assertLess(one, all4)                                                       # one facade costs less than four
        brick = builder.remodel(d, **{"walls.%s.stone" % side: {"pattern": "brick_running", "color": 0}, "walls.%s.brick_to_eaves" % side: True})
        self.assertEqual(builder.check(W, i, d, brick, WHOLE), [])
        bad = builder.remodel(d, **{"walls.%s.brick_to_eaves" % back: True})
        if d["walls"][back]["stone"]["pattern"] not in PL["brick_to_eaves"]:
            self.assertTrue(any("only brick can run to the eaves" in x for x in builder.check(W, i, d, bad, WHOLE)))
        bd = world.built(W, brick)
        self.assertNotEqual(bd["walls"][side]["stone"], bd["walls"][back]["stone"])


class TheUnits(unittest.TestCase):
    def test_BUILD_019_inside_finishes_per_unit(self):
        lot = next(u["lot_id"] for u in UNITS if u["kind"] == "flat" and u["floors"] != [0] and u["lot_id"] in D and not LOT[u["lot_id"]][1].get("house"))
        mine = [u for u in UNITS if u["lot_id"] == lot and u["kind"] == "flat"][0]
        d = D[lot]
        f = mine["floors"][0]
        walls = list(d["inside"]["walls"])
        walls[f] = other(K["plaster"], walls[f])
        floors = list(d["inside"]["floors"])
        floors[f] = other_floor(floors[f])
        new = builder.remodel(d, **{"inside.walls": walls, "inside.floors": floors})
        self.assertEqual(builder.check(W, lot, d, new, {"units": [mine["id"]]}), [])          # a flat's owner repaints the flat
        theirs = next(u for u in UNITS if u["lot_id"] == lot and u["id"] != mine["id"])
        self.assertIn("floor %d is not yours to change" % f, builder.check(W, lot, d, new, {"units": [theirs["id"]]}))
        lines = [x for x, _, _ in builder.cost(W, lot, d, new)[1]]
        self.assertEqual(lines, ["New floors (1 storey)", "Repaint inside (1 storey)", "Permit"])
        bd = world.built(W, new)
        self.assertNotEqual(bd["inside"]["walls"][f], bd["inside"]["walls"][0 if f else 1])


class TheCost(unittest.TestCase):
    def test_BUILD_005_the_cost_is_known_line_by_line(self):
        i = first("Old Town")
        d = D[i]
        new = builder.remodel(d, **{"walls.*.plaster": other(K["plaster"], d["walls"]["n"]["plaster"]), "openings.frame": other(K["frames"], d["openings"]["frame"])})
        total, lines = builder.cost(W, i, d, new)
        self.assertEqual(total, sum(c for _, _, c in lines))
        self.assertEqual([w for w, _, _ in lines], ["Repaint the plaster", "Paint the window frames", "Permit"])
        self.assertEqual(builder.cost(W, i, d, d), (0, []))

    def test_BUILD_005_a_full_remodel_is_about_a_quarter_of_a_house(self):
        # Q-39, the owner: "Expensive", about 25% of value; a repaint about 60 brix. The town's own buildings, as designed here
        # (the community houses are built from their own plans, houses.js, and are not yet designs: see BUILDER.md §9)
        R = W["estate"]
        value = {u["lot_id"]: u["area"] * R["kinds"]["house"]["per_m2"] * u["loc_bp"] // 10000 * u["floor_bp"] // 10000
                 for u in UNITS if u["kind"] == "house" and not LOT[u["lot_id"]][1].get("house")}
        shares, repaints = [], []
        for i, v in value.items():
            d = D[i]
            st = d["walls"]["n"]["stone"]["pattern"]
            full = builder.remodel(d, **{"walls.*.plaster": other(K["plaster"], d["walls"]["n"]["plaster"]), "roof.tiles": other(K["roof_tiles"], d["roof"]["tiles"]),
                                         "openings.frame": other(K["frames"], d["openings"]["frame"]),
                                         "inside.floors": [other_floor(f) for f in d["inside"]["floors"]],
                                         "walls.*.stone": {"pattern": "ashlar" if st != "ashlar" else "brick_running", "color": None if st != "ashlar" else 0}})
            shares.append(builder.cost(W, i, d, full)[0] / v)
            if plastered(d):
                repaints.append(builder.cost(W, i, d, builder.remodel(d, **{"walls.*.plaster": other(K["plaster"], d["walls"]["n"]["plaster"])}))[0] / 100)
        self.assertGreater(len(shares), 300)
        self.assertTrue(.2 <= statistics.median(shares) <= .3, statistics.median(shares))
        self.assertTrue(40 <= statistics.median(repaints) <= 90, statistics.median(repaints))


if __name__ == "__main__":
    unittest.main()
