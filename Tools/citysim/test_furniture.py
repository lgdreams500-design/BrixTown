"""Buying furniture at the furniture outlet (docs/specs/domains/FURNITURE.md FURN-022 to FURN-029).   python -m unittest test_furniture"""
import copy
import json
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "worldgen"))
import economy as ec  # noqa: E402
import furnish  # noqa: E402
import furniture as fu  # noqa: E402

RULES = os.path.join(HERE, "..", "..", "World", "rules")
read = lambda name: json.load(open(os.path.join(RULES, name + ".json"), encoding="utf-8"))  # noqa: E731
R, F, E = read("furniture"), read("furnishing"), read("economy")
TOWN = [{"id": E["bank"]["lot"], "kind": "bank", "name": "Bank"}, {"id": E["stall"]["lot"], "kind": "market", "name": "Market"}]
START = 600000


def shopper(money=START, held=("u:mine",), far=800.0):
    """A player with a current account and some money, at the outlet on day 0 at 07:20, and the store."""
    e = ec.Economy(E, TOWN, {})
    e.advance(0, 440)
    e.new_character()
    e.open_account("current")
    if money:
        e.L.post(0, 440, "Test money", [("sys:start", -money), ("p:current", money)])
    mine = set(held)
    s = fu.Store(R, F, e, lambda u: u in mine, lambda u: far)
    s.found()
    return e, s, mine


def now(it, **more):
    return dict(kind=it, line="plain", how="instant", **more)


def later(it, unit="u:mine", day=1, slot=0, **more):
    return dict(kind=it, line="plain", how="delivery", unit=unit, day=day, slot=slot, **more)


class TheRules(unittest.TestCase):
    def test_FURN_025_the_rules_are_sound(self):
        self.assertEqual(fu.problems(R, F), [])

    def test_FURN_025_broken_rules_are_caught(self):
        def broken(change):
            r = copy.deepcopy(R)
            change(r)
            return fu.problems(r, F)
        self.assertTrue(broken(lambda r: r["company"].update(account="till:outlet")))
        self.assertTrue(broken(lambda r: r["delivery"].update(fee_by_m3=[[0.6, 600], [2.0, 500], [None, 2000]])))
        self.assertTrue(broken(lambda r: r["delivery"].update(fee_by_m3=[[0.6, 600]])))
        self.assertTrue(broken(lambda r: r["delivery"].update(slots=[[540, 720], [700, 900]])))
        self.assertTrue(broken(lambda r: r["delivery"].update(first_day_after=0)))
        self.assertTrue(broken(lambda r: r.update(price_level_bp=1.5)))
        self.assertTrue(broken(lambda r: r["assembly"].update(least_min=0)))
        self.assertTrue(broken(lambda r: r["flat_pack"].update(box=[1, 1])))
        self.assertTrue(broken(lambda r: r["company"].update(wages_bp=12000)))


class TheShowroom(unittest.TestCase):
    def test_FURN_022_every_piece_sold_has_a_tag(self):
        e, s, _ = shopper()
        self.assertTrue(s.sells())
        packed = 0
        for k in s.sells():
            for line in F["lines"]:
                t = s.tag(k, line)
                self.assertEqual(t["name"], F["pieces"][k]["name"])
                self.assertEqual(t["size"], F["pieces"][k]["size"])
                self.assertIs(type(t["price"]), int)
                self.assertGreater(t["price"], 0)
                self.assertEqual(sorted(t["other_lines"] + [line]), sorted(F["lines"]))      # its other finishes
                self.assertEqual(t["flat_packed"], t["boxes"] > 0)
                if t["flat_packed"]:
                    self.assertGreater(t["assembly_min"], 0)
                    self.assertGreater(t["assembly_fee"], 0)
            packed += s.flat_packed(k)
        self.assertTrue(0 < packed < len(s.sells()), "some pieces come flat-packed and some whole")
        for k, p in F["pieces"].items():
            if p.get("from"):                                                              # FURN-064: sold elsewhere, not here
                self.assertRaises(ec.Refused, s.tag, k, "plain")

    def test_FURN_023_ordering_from_home_needs_the_catalogue_and_books_delivery_only(self):
        e, s, _ = shopper()
        self.assertRaises(ec.Refused, s.checkout, [later("k17:sofa")], True)             # no catalogue
        s.take_catalogue()
        self.assertRaises(ec.Refused, s.checkout, [now("k17:sofa")], True)               # from home nothing is taken at once
        before = e.total_money()
        total, got = s.checkout([later("k17:sofa")], True)
        self.assertEqual(e.total_money(), before - total)
        self.assertEqual(s.on_its_way(), got)


class TheTill(unittest.TestCase):
    def test_FURN_025_buy_all_or_nothing(self):
        e, s, _ = shopper(money=30000)
        tx = len(e.L.tx)
        total, got = s.checkout([now("k17:coffee"), now("l_sidetable")])
        self.assertEqual(total, s.price("k17:coffee") + s.price("l_sidetable"))
        self.assertEqual(len(e.L.tx), tx + 1, "one ledger transaction for the whole basket")
        self.assertEqual(e.L.balance(R["company"]["account"]), R["company"]["float"] + total)
        self.assertEqual(len(set(got)), 2)                                                  # each piece its own id (PROP-001)
        self.assertEqual([p["id"] for p in s.owned], got)
        money, owned, n = e.total_money(), list(s.owned), len(e.L.tx)
        for basket in ([now("k17:wardrobe")] * 3,                                           # dearer than they can pay
                       [now("k17:coffee"), now("spaceship")],                               # one piece is not sold: nothing is bought
                       [now("k17:coffee"), now("o_vases")],                                 # sold at the mall, not here
                       [now("k17:coffee"), dict(kind="k17:sofa", line="chrome", how="instant")],
                       [now("k17:coffee"), dict(kind="k17:sofa", line="plain", how="carry it off")],
                       []):
            self.assertRaises(ec.Refused, s.checkout, basket)
            self.assertEqual((e.total_money(), s.owned, len(e.L.tx), s.orders), (money, owned, n, []))
        self.assertEqual(e.L.supply() + e.L.system_total(), 0)                              # ECO-002: the books balance

    def test_FURN_026_instant_and_delivery(self):
        e, s, _ = shopper()
        total, (a, b, c) = s.checkout([now("k17:coffee"), later("k17:sofa", day=2, slot=1), later("k17:bookcase", day=2, slot=1, assembled=True)])
        fee = s.delivery_fee(["k17:sofa", "k17:bookcase"])
        self.assertEqual(total, s.price("k17:coffee") + s.price("k17:sofa") + s.price("k17:bookcase") + s.assembly_fee("k17:bookcase") + fee)
        self.assertEqual(R["instant_fee"], 0)                                               # the owner, 2026-11-22: instant is free for now
        self.assertEqual([p["id"] for p in s.owned], [a])                                   # instant: with the belongings at once
        self.assertEqual(s.on_its_way(), [b, c])
        self.assertEqual(len(s.orders), 1, "one van for what goes to one home in one slot")
        # a bigger load costs more to deliver, and never less
        fees = [s.delivery_fee(k) for k in (["l_sidetable"], ["k17:sofa"], ["k17:sofa", "k17:wardrobe", "k17:bed"])]
        self.assertEqual(fees, sorted(fees))
        self.assertLess(fees[0], fees[-1])
        # booked to a unit of theirs, on a day and slot that can still be booked
        for bad in (later("k17:bed", unit="u:other"), later("k17:bed", day=0), later("k17:bed", day=1 + R["delivery"]["days_ahead"]), later("k17:bed", slot=len(R["delivery"]["slots"]))):
            self.assertRaises(ec.Refused, s.checkout, [bad])
        # a piece that comes whole has nothing to put together, so nothing is charged for it
        t, _ = s.checkout([later("l_sidetable", assembled=True)])
        self.assertEqual(t, s.price("l_sidetable") + s.delivery_fee(["l_sidetable"]))

    def test_FURN_050_one_place(self):
        """Every piece bought is in exactly one place: with the belongings, set down in a unit, or on its way."""
        e, s, mine = shopper()
        ids = s.checkout([now("k17:coffee"), later("k17:sofa"), later("k17:bed", day=2), later("k17:armchair", unit="u:mine", day=3)])[1]
        for day, minute in ((0, 500), (1, 541), (1, 545), (1, 600), (2, 560), (3, 1000)):
            e.advance(day, minute)
            s.advance(day, minute)
            where = [p["id"] for p in s.owned] + [p["id"] for ps in s.arrived.values() for p in ps] + s.on_its_way()
            self.assertEqual(sorted(where), sorted(ids), (day, minute))


class TheDelivery(unittest.TestCase):
    def test_FURN_027_delivery_run(self):
        e, s, _ = shopper(far=800.0)                                                        # two minutes by van
        s.checkout([later("l_sidetable"), later("k17:coffee")])
        o, t0 = s.orders[0], R["delivery"]["slots"][0][0]
        drive, carry = 800.0 / R["delivery"]["van_m_per_min"], R["delivery"]["carry_min"]
        seen = [s.van(o, 1, t0 + dt) for dt in (-1, 0, drive / 2, drive, drive + carry, drive + 2 * carry, drive + 2 * carry + drive / 2, 2 * drive + 2 * carry, 500)]
        self.assertEqual([v["phase"] for v in seen], ["booked", "out", "out", "at the door", "at the door", "back", "back", "done", "done"])
        self.assertEqual([v["landed"] for v in seen], [0, 0, 0, 0, 1, 2, 2, 2, 2])
        self.assertAlmostEqual(seen[2]["f"], .5)
        self.assertAlmostEqual(seen[6]["f"], .5)
        # worked out from the booking and the clock: asking twice, or in any order, gives the same answer
        self.assertEqual(s.van(o, 1, t0 + 3), s.van(o, 1, t0 + 3))
        self.assertEqual(s.van(o, 0, 600)["phase"], "booked")
        self.assertEqual(s.van(o, 2, 0)["phase"], "done")
        # a home further off is reached later
        e2, s2, _ = shopper(far=4000.0)
        s2.checkout([later("l_sidetable")])
        self.assertEqual(s2.van(s2.orders[0], 1, t0 + 5)["phase"], "out")
        self.assertEqual(s.van(o, 1, t0 + 5)["phase"], "at the door")

    def test_FURN_028_delivery_lands(self):
        e, s, mine = shopper()
        a, b = s.checkout([later("k17:sofa", at=[3.0, 2.5, 90]), later("l_sidetable")])[1]
        e.advance(1, 539)
        self.assertEqual(s.advance(1, 539), [])
        e.advance(1, 700)
        self.assertEqual(s.advance(1, 700), [a, b])
        self.assertEqual(s.advance(1, 700), [], "a piece lands once")
        self.assertEqual([p["id"] for p in s.arrived["u:mine"]], [a, b])
        self.assertTrue(s.is_boxed(a, 1, 700))                                              # the sofa came flat-packed: its boxes
        self.assertFalse(s.is_boxed(b, 1, 700))                                             # the side table came whole
        box = s.as_placed(a, 1, 700)
        self.assertEqual((box["w"], box["d"]), tuple(R["flat_pack"]["box"][:2]))
        self.assertLess(box["h"], F["pieces"]["k17:sofa"]["size"][2] + 1)
        # it stands where the player marked it, or at the free spot nearest the way in
        room, keep, entry = [0.0, 0.0, 6.0, 5.0], [[2.5, 0.0, 3.5, 1.2]], [3.0, 0.6]
        lay = []
        q = fu.set_down(F, furnish, room, keep, 2.6, lay, dict(box, at=[3.0, 2.5, 90]), entry)
        self.assertEqual((q["x"], q["z"], q["yaw"]), (3.0, 2.5, 90))
        lay.append(q)
        q2 = fu.set_down(F, furnish, room, keep, 2.6, lay, dict(s.as_placed(b, 1, 700), at=[3.0, 2.5, 0]), entry)   # marked where the sofa's boxes now are
        self.assertNotEqual((q2["x"], q2["z"]), (3.0, 2.5))
        self.assertEqual(furnish.problems(F, room, keep, 2.6, lay + [q2]), [])
        self.assertIsNone(fu.set_down(F, furnish, [0.0, 0.0, 0.4, 0.4], [], 2.6, [], s.as_placed(b, 1, 700), [0.2, 0.2]))   # no room: it waits by the door
        # a unit the player no longer holds when the van comes: the pieces go to the belongings and they are told
        c = s.checkout([later("k17:armchair", day=3)])[1][0]
        mine.discard("u:mine")
        e.advance(3, 700)
        s.advance(3, 700)
        self.assertIn(c, [p["id"] for p in s.owned])
        self.assertTrue(any("no longer yours" in n[2] for n in s.notes))
        # and what was set down in a home they leave comes with them
        s.move_out("u:mine")
        self.assertEqual(sorted(p["id"] for p in s.owned), sorted([a, b, c]))
        self.assertEqual(s.arrived, {})

    def test_FURN_029_assembly(self):
        e, s, _ = shopper()
        a, b, c = s.checkout([later("k17:sofa"), later("k17:bookcase", assembled=True), now("k17:bed")])[1]
        self.assertRaises(ec.Refused, s.assemble, a, 0, 500)                               # not here yet
        e.advance(1, 700)
        s.advance(1, 700)
        self.assertFalse(s.is_boxed(b, 1, 700), "the fitter who came with the van put it together")
        self.assertTrue(s.is_boxed(a, 1, 700))
        m = s.assemble(a, 1, 700)                                                           # the player does it: a timed action
        self.assertEqual(m, s.assembly_min("k17:sofa"))
        self.assertTrue(s.is_boxed(a, 1, 700 + m - 1))
        self.assertFalse(s.is_boxed(a, 1, 700 + m))
        self.assertEqual(s.as_placed(a, 1, 700 + m)["w"], F["pieces"]["k17:sofa"]["size"][0])
        self.assertRaises(ec.Refused, s.assemble, a, 1, 700 + m)                           # whole already
        self.assertRaises(ec.Refused, s.assemble, b, 1, 700)
        self.assertRaises(ec.Refused, s.assemble, c, 1, 700)                               # taken at once: it came whole
        # a bigger piece takes longer
        self.assertGreater(s.assembly_min("k17:wardrobe"), s.assembly_min("k17:coffee"))
        # or a fitter is paid to come in a slot
        d = s.checkout([later("k17:wardrobe", day=3)])[1][0]
        e.advance(3, 700)
        s.advance(3, 700)
        before, n = e.total_money(), len(e.L.tx)
        fee = s.fitter(d, 4, 0)
        self.assertEqual((e.total_money(), len(e.L.tx)), (before - fee, n + 1))
        self.assertEqual(fee, s.assembly_fee("k17:wardrobe"))
        t0 = R["delivery"]["slots"][0][0]
        self.assertTrue(s.is_boxed(d, 4, t0))
        self.assertFalse(s.is_boxed(d, 4, t0 + s.assembly_min("k17:wardrobe")))
        self.assertRaises(ec.Refused, s.fitter, d, 5, 0)                                   # booked already
        poor_e, poor, _ = shopper(money=0)
        poor_e.L.post(0, 440, "Just enough", [("sys:start", -(poor.price("k17:sofa") + poor.delivery_fee(["k17:sofa"]) - poor_e.total_money())), ("p:cash", poor.price("k17:sofa") + poor.delivery_fee(["k17:sofa"]) - poor_e.total_money())])
        x = poor.checkout([later("k17:sofa")])[1][0]
        poor_e.advance(1, 700)
        poor.advance(1, 700)
        self.assertRaises(ec.Refused, poor.fitter, x, 2, 0)                                # cannot pay the fitter: nothing is booked
        self.assertEqual(poor.assemble(x, 1, 700), poor.assembly_min("k17:sofa"))          # but can do it themselves


class TheCompany(unittest.TestCase):
    def test_FURN_024_money_supply(self):
        # with no customer nothing moves: the money there is stays what it was
        e, s, _ = shopper(money=0)
        e.hooks.append(s.settle)
        supply = e.L.supply()
        e.advance(60, 0)
        self.assertEqual(e.L.supply(), supply)
        self.assertEqual(e.L.balance(R["company"]["account"]), R["company"]["float"])
        # a customer's money goes to the company, and at each day's end on to its staff and its stock
        e, s, _ = shopper()
        e.hooks.append(s.settle)
        spent = 0
        for day in range(0, 20):
            e.advance(day, 600)
            spent += s.checkout([now("k17:coffee"), later("l_sidetable", day=day + 1)])[0]
            self.assertLessEqual(e.L.balance(R["company"]["account"]), R["company"]["float"] + 2 * (s.price("k17:coffee") + s.price("l_sidetable") + 5000))
        e.advance(21, 0)
        self.assertEqual(e.L.balance(R["company"]["account"]), R["company"]["float"])
        paid = {a: sum(v for _, _, _, memo, lines in e.L.tx for acct, v in lines if acct == a and "wages and stock" in memo) for a in ("sys:wages", "sys:spend")}
        self.assertEqual(paid["sys:wages"] + paid["sys:spend"], spent)
        self.assertEqual(paid["sys:wages"], sum(over * R["company"]["wages_bp"] // 10000 for over in
                                                 [-v for _, _, _, memo, lines in e.L.tx for acct, v in lines if acct == R["company"]["account"] and "wages and stock" in memo]))
        self.assertEqual(e.L.supply() + e.L.system_total(), 0)

    def test_QST_008_the_store_is_kept_in_the_save(self):
        e, s, _ = shopper()
        s.take_catalogue()
        s.checkout([now("k17:coffee"), later("k17:sofa"), later("k17:bed", day=2)])
        e.advance(1, 700)
        s.advance(1, 700)
        kept = json.loads(json.dumps(s.save()))
        e2, s2, _ = shopper()
        s2.load(kept)
        self.assertEqual(s2.save(), s.save())
        for day, minute in ((1, 900), (2, 700)):
            self.assertEqual(s2.advance(day, minute), s.advance(day, minute))
        self.assertEqual(s2.save(), s.save())

    def test_FURN_025_the_fixed_run_for_the_ports(self):
        """check_script() runs, and the same twice over: what the slice's and the engine's ports must reproduce."""
        def run():
            return fu.run_script(R, F, ec.Economy(E, TOWN, {}), fu.check_script())
        a, b = run(), run()
        self.assertEqual(a, b)
        kinds = [r[0] for r in a["results"]]
        self.assertIn("ok", kinds)
        self.assertIn("refused", kinds)
        self.assertEqual(a["on_its_way"], [])
        self.assertEqual(a["company"], R["company"]["float"])


if __name__ == "__main__":
    unittest.main()
