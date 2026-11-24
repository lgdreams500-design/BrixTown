"""Money, shops, the stall, quests, jobs and the designer data in the first slice (ADR-0012). Runs over World/ through
Tools/worldgen/world.py.        python -m unittest test_economy -v

Each test names the requirement it proves (docs/specs/GAME_SPEC.md)."""
import collections, copy, json, os, sys, unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import world                                               # noqa: E402
import build_play                                          # noqa: E402
import economy as ec                                       # noqa: E402
import quests as qs                                        # noqa: E402

W = world.load()
CITY = world.simulation(W)
LOTS = CITY.d["lots"]
PLAN, ACTS = build_play.routines(CITY, 16, {})
R = W["economy"]
VISITS = ec.visits_from_packed(PLAN, ACTS, sorted(set(R["stock"]["visit_acts"]) | set(R["stall"]["visit_acts"])))
LOT = {l["id"]: n for n, l in enumerate(LOTS)}
STAFF = collections.defaultdict(list)
for r in W["residents"]:
    if r["work"]:
        STAFF[LOT[r["work"]]].append(r["id"])
RESIDENTS = W["residents"]


GOODS = W["mall_goods"]


def town():
    return ec.Economy(R, LOTS, VISITS, STAFF, GOODS)


def player(start=True):
    e = town()
    q = qs.Quests(W["quests"], e, RESIDENTS, LOTS)
    e.advance(0, 440)
    if start:
        q.new_game()
    return e, q


def first(kind):
    e = town()
    return next(n for n in range(len(LOTS)) if e.shop_kind(n) == kind)


GROCER, KIOSK, ARCADE, CAFE = first("grocer"), first("kiosk"), first("arcade"), first("cafe")


def invariant(t, e):
    """ECO-002 and ECO-003 over a whole ledger."""
    for no, day, minute, memo, lines in e.L.tx:
        t.assertEqual(sum(v for _, v in lines), 0, memo)
        t.assertTrue(all(type(v) is int for _, v in lines), memo)
    bal = collections.Counter()
    for *_, lines in e.L.tx:
        for a, v in lines:
            bal[a] += v
    for a, v in bal.items():
        t.assertEqual(e.L.balance(a), v, a)
        if not ec.is_system(a):
            t.assertGreaterEqual(v, 0, a)
    t.assertEqual(e.L.supply(), -e.L.system_total())


class TheLedger(unittest.TestCase):
    def test_ECO_001_money_is_whole_chips(self):
        L = ec.Ledger()
        for bad in (1.5, True, "3"):
            with self.assertRaises(ec.Refused):
                L.post(0, 0, "x", [("sys:start", -1), ("p:cash", bad)])
        self.assertEqual(ec.fmt(123456), "Bx 1234.56")
        self.assertEqual(ec.fmt(-5), "-Bx 0.05")
        self.assertEqual(world.economy_problems(W, ACTS), [])
        for it in R["items"].values():
            self.assertIs(type(it["price"]), int)

    def test_ECO_002_transactions_balance_and_are_written_whole(self):
        L = ec.Ledger()
        L.post(0, 0, "start", [("sys:start", -500), ("p:cash", 500)])
        with self.assertRaises(ec.Refused):
            L.post(0, 1, "unbalanced", [("p:cash", -100), ("till:x", 90)])
        with self.assertRaises(ec.Refused):                # would take cash below zero: nothing at all is written
            L.post(0, 1, "too much", [("p:cash", -600), ("till:x", 600)])
        self.assertEqual(len(L.tx), 1)
        self.assertEqual(L.balance("p:cash"), 500)
        self.assertEqual(L.balance("till:x"), 0)
        with self.assertRaises(TypeError):                 # never edited
            L.tx[0][4][0] = ("p:cash", 1)

    def test_ECO_003_money_comes_only_from_system_accounts(self):
        e, q = player()
        e.open_account("current")
        e.buy(GROCER, "bread")
        e.sell(GROCER, "bread")
        e.advance(3, 600)
        invariant(self, e)
        made = sum(-v for *_, lines in e.L.tx for a, v in lines if ec.is_system(a))
        self.assertEqual(made, e.L.supply())

    def test_ECO_009_statement_newest_first(self):
        e, q = player()
        e.open_account("current")
        e.buy(GROCER, "bread")
        st = e.L.statement(ec.PLAYER)
        self.assertEqual(st[0][3].split(",")[0], "Loaf of bread")
        self.assertEqual([s[0] for s in st], sorted([s[0] for s in st], reverse=True))
        self.assertEqual(sum(s[4] for s in st), e.total_money())


class TheShops(unittest.TestCase):
    def test_ECO_010_stock_is_worked_out_from_visits_and_trades(self):
        e = town()
        e.advance(0, 359)
        before = {it: e.stock(GROCER, it) for it in R["shops"]["grocer"]["sells"]}
        e.advance(0, 360)                                  # the restock
        self.assertTrue(all(e.stock(GROCER, it) == R["stock"]["target"] for it in before) or VISITS.get(GROCER, [(0, 0)])[0][0] <= 360)
        e.advance(0, 1439)
        sold = sum(R["stock"]["target"] - e.stock(GROCER, it) for it in before)
        self.assertEqual(sold, sum(1 for t, i in VISITS.get(GROCER, ()) if t >= 360))
        again = town()                                     # the same answer from a fresh start: nothing was ticked
        again.advance(0, 1439)
        self.assertEqual({it: again.stock(GROCER, it) for it in before}, {it: e.stock(GROCER, it) for it in before})

    def test_ECO_005_price_follows_stock_within_bounds(self):
        e, q = player()
        e.give_item("bread", 30)
        e.L.post(0, 440, "money for the test", [("sys:start", -50000), ("p:cash", 50000)])
        P = R["price"]
        base = R["items"]["bread"]["price"]
        prices = []
        for _ in range(12):
            prices.append(e.price(GROCER, "bread"))
            e.sell(GROCER, "bread")                         # stock rises: price falls
        self.assertEqual(prices, sorted(prices, reverse=True))
        self.assertGreaterEqual(min(prices), base * P["min_bp"] // 10000)
        while e.stock(GROCER, "bread") > 0:
            e.buy(GROCER, "bread")
        self.assertLessEqual(e.price(GROCER, "bread"), (base * P["max_bp"] + 9999) // 10000)
        with self.assertRaises(ec.Refused):
            e.buy(GROCER, "bread")                          # sold out

    def test_ECO_011_selling_to_shops(self):
        e, q = player()
        e.give_item("bread")
        e.give_item("coffee")
        cash = e.L.balance("p:cash")
        p = e.offer(GROCER, "bread")
        self.assertEqual(p, R["items"]["bread"]["price"] * e.factor_bp(GROCER, "bread") * R["price"]["sell_share_bp"] // 10 ** 8)
        e.sell(GROCER, "bread")
        self.assertEqual(e.L.balance("p:cash"), cash + p)
        with self.assertRaises(ec.Refused):
            e.sell(CAFE, "coffee")                          # cafés buy nothing
        with self.assertRaises(ec.Refused):
            e.sell(GROCER, "hat")                           # not what a grocer deals in
        till = "till:" + LOTS[GROCER]["id"]
        e.L.post(0, 500, "empty the till", [(till, -e.L.balance(till)), ("sys:fees", e.L.balance(till))])
        e.give_item("bread")
        with self.assertRaises(ec.Refused):
            e.sell(GROCER, "bread")                         # an empty till buys nothing

    def test_ECO_012_economy_data_is_checked(self):
        w = copy.deepcopy(W)
        w["economy"]["items"]["bread"]["price"] = 2.5
        w["economy"]["shops"]["grocer"]["sells"].append("unicorn")
        w["economy"]["bank"]["lot"] = "b_3_4_l0"
        w["economy"]["price"]["min_bp"] = 12000
        p = world.economy_problems(w)
        for bit in ("bread", "unicorn", "not a bank", "price bounds"):
            self.assertTrue(any(bit in x for x in p), bit)

    def test_PROP_001_items_are_owned(self):
        e, q = player()
        e.buy(KIOSK, "map")
        self.assertEqual(e.inv, {"map": 1})
        e.sell(KIOSK, "map")
        self.assertEqual(e.inv, {})
        with self.assertRaises(ec.Refused):
            e.sell(KIOSK, "map")

    def test_QST_007_reputation_moves_prices(self):
        e, q = player()
        base = e.price(GROCER, "jam")
        q.rep_add(STAFF[GROCER][0], 100)
        good = e.price(GROCER, "jam")
        q.rep_add(STAFF[GROCER][0], -300)
        self.assertEqual(e.rep[STAFF[GROCER][0]], -100)     # kept inside -100..100
        bad = e.price(GROCER, "jam")
        self.assertLess(good, base)
        self.assertGreater(bad, base)


class TheOutlets(unittest.TestCase):
    """Buying at each outlet of a mall (the owner, 2026-11-24: "add B or Buy separately at each outlet, instead of showing
    it for entire mall. each outlets should have buyable products in it with decent catalog")."""

    def setUp(self):
        self.e, self.q = player()
        self.e.L.post(0, 440, "Test money", [("sys:start", -5000000), ("p:cash", 5000000)])
        self.n = LOT["b_16_5_l0"]
        self.us = self.e.outlets[self.n]

    def first(self, kind, goes=None):
        """The first outlet of a kind (that sells something going `goes`), and that thing."""
        for u, U in self.us.items():
            if U["kind"] == kind:
                for g in self.e.outlet_sells(self.n, u):
                    if goes is None or self.e.good(g)["goes"] == goes:
                        return u, g
        raise AssertionError("no %s outlet sells anything that goes %s" % (kind, goes))

    def test_ECO_013_every_outlet_sells_its_own_goods(self):
        e, n = self.e, self.n
        self.assertEqual(len(self.us), 189)                                 # every outlet of the mall, on all three floors
        self.assertEqual(len({U["level"] for U in self.us.values()}), 3)
        ranges = collections.defaultdict(set)
        for u, U in self.us.items():
            S = e.outlet_sells(n, u)
            self.assertGreaterEqual(len(S), 6, U)                           # a decent range in each
            self.assertLessEqual(len(S), GOODS["per_outlet"], U)
            self.assertTrue(set(S) <= set(GOODS["sells"][U["kind"]]), U)    # of its own kind's goods
            self.assertEqual(S, e.outlet_sells(n, u))                       # and the same every time it is asked
            ranges[U["kind"]].add(tuple(S))
        self.assertGreater(len(ranges["clothes"]), 5)                       # two clothes shops are not the same shop
        with self.assertRaises(ec.Refused):                                 # the mall is no longer one counter
            e.buy(n, "hat")
        u, g = self.first("music")
        other = next(x for x in GOODS["sells"]["burger"] if x not in e.outlet_sells(n, u))
        with self.assertRaises(ec.Refused):                                 # a guitar shop sells no burgers
            e.buy_at(n, u, other)
        with self.assertRaises(ec.Refused):
            e.buy_at(n, "b_16_5_u0", g)                                     # a corner store is not an outlet
        with self.assertRaises(ec.Refused):
            e.buy_at(GROCER, u, g)                                          # nor is a street shop

    def test_ECO_013_buying_is_paid_through_the_ledger_into_the_malls_till(self):
        e, n = self.e, self.n
        u, g = self.first("books")
        till, cash, tx = e.L.balance("till:b_16_5_l0"), e.L.balance("p:cash"), len(e.L.tx)
        p = e.buy_at(n, u, g)
        self.assertIs(type(p), int)
        self.assertEqual(e.L.balance("till:b_16_5_l0"), till + p)
        self.assertEqual(e.L.balance("p:cash"), cash - p)
        self.assertEqual(len(e.L.tx), tx + 1)
        self.assertIn(self.us[u]["name"], e.L.tx[-1][3])                     # the statement names the outlet
        invariant(self, e)
        poor, _ = player()                                                   # not enough money: refused whole, nothing given
        dear_u, dear = self.first("music", "home")
        self.assertGreater(poor.outlet_price(n, dear_u, dear), poor.total_money())
        with self.assertRaises(ec.Refused):
            poor.buy_at(n, dear_u, dear)
        self.assertEqual((poor.homeware, poor.inv, poor.clothes), ({}, {}, {}))
        invariant(self, poor)

    def test_ECO_013_stock_and_price_are_each_outlets_own(self):
        e, n = self.e, self.n
        u, g = self.first("toys")
        T = GOODS["stock"]["target"]
        e.advance(0, 1439)
        left = e.outlet_stock(n, u, g)
        self.assertEqual(left, max(0, T - e.outlet_resident_buys(n, u).get(g, 0)))
        visits = [(t, i) for t, i in VISITS.get(n, ()) if t >= R["stock"]["restock_minute"]]
        sold = sum(sum(e.outlet_resident_buys(n, x).values()) for x in self.us)
        self.assertEqual(sold, len(visits))                                  # each visit to the mall buys one thing at one outlet
        v, h = next((x, y) for x, U in self.us.items() if U["kind"] == "toys" and x != u for y in e.outlet_sells(n, x)[:1])
        before, prices = e.outlet_stock(n, v, h), []
        while e.outlet_stock(n, u, g):
            prices.append(e.outlet_price(n, u, g))
            e.buy_at(n, u, g)
        self.assertEqual(prices, sorted(prices))                             # dearer as the shelf empties
        if len(prices) > 1:
            self.assertLess(prices[0], prices[-1])
        self.assertEqual(e.outlet_stock(n, v, h), before)                    # the other toy shop's shelf is untouched
        with self.assertRaises(ec.Refused):
            e.buy_at(n, u, g)                                                # sold out
        P = R["price"]
        base = e.good(g)["price"]
        self.assertTrue(all(base * P["min_bp"] // 10000 - 1 <= p <= base * P["max_bp"] // 10000 + 1 for p in prices))
        e.advance(1, R["stock"]["restock_minute"])                           # and full again after the restock
        self.assertEqual(e.outlet_stock(n, u, g), T - e.outlet_resident_buys(n, u).get(g, 0))
        self.assertGreater(e.outlet_stock(n, u, g), 0)

    def test_ECO_014_what_is_bought_goes_where_it_belongs(self):
        e, n = self.e, self.n
        u, g = self.first("music", "home")                                   # a guitar goes to the home
        e.buy_at(n, u, g)
        self.assertEqual(e.homeware, {g: 1})
        self.assertIn(GOODS["goods"][g]["piece"], W["furnishing"]["pieces"])
        u, g = self.first("clothes", "wear")                                 # a shirt to the wardrobe
        e.buy_at(n, u, g)
        e.buy_at(n, u, g)
        self.assertEqual(e.clothes, {g: 2})
        u, g = self.first("burger", "eat")                                   # a burger is carried, and no shop buys it back
        e.buy_at(n, u, g)
        self.assertEqual(e.inv, {g: 1})
        self.assertFalse(e.items[g]["sell"])
        u, g2 = self.first("jewellery", "carry")                             # a ring is carried, and can be set out on a stall
        e.buy_at(n, u, g2)
        self.assertEqual(e.inv, {g: 1, g2: 1})
        self.assertTrue(e.items[g2]["sell"])
        e.rent_stall()
        e.set_out(g2, e.items[g2]["price"])
        self.assertEqual(e.inv, {g: 1})
        invariant(self, e)

    def test_ECO_014_the_catalogue_covers_every_kind_and_every_way_of_going(self):
        kinds = W["mall_outlets"]["kinds"]
        self.assertEqual(set(GOODS["sells"]), set(kinds))                   # all eighteen kinds of outlet
        e = town()
        goes = collections.Counter(e.good(g)["goes"] for L in GOODS["sells"].values() for g in set(L))
        for way in ("carry", "eat", "home", "wear"):
            self.assertGreater(goes[way], 15, way)
        for kind, K in kinds.items():                                        # food outlets sell food; nothing else does
            ways = {e.good(g)["goes"] for g in GOODS["sells"][kind]}
            if K["trade"] == "food":
                self.assertTrue(ways <= {"eat", "carry"}, kind)
                self.assertIn("eat", ways, kind)
        for kind, way in (("music", "home"), ("books", "home"), ("toys", "home"), ("electronics", "home"), ("home", "home"),
                          ("gifts", "home"), ("clothes", "wear"), ("shoes", "wear"), ("sports", "wear")):
            self.assertIn(way, {e.good(g)["goes"] for g in GOODS["sells"][kind]}, kind)
        names = [e.good(g)["name"] for g in GOODS["sells"]["music"]]
        self.assertTrue(any("guitar" in x.lower() for x in names))          # the owner's own examples: a guitar, books
        self.assertTrue(any(e.good(g)["goes"] == "home" for g in GOODS["sells"]["books"]))

    def test_ECO_014_the_catalogue_is_checked(self):
        self.assertEqual(world.mall_goods_problems(W), [])
        w = copy.deepcopy(W)
        G = w["mall_goods"]
        G["goods"]["gt_acoustic"]["price"] = 99.5
        G["goods"]["tee_teal"]["parts"] = {"top": "neon", "height": 2.0}
        G["goods"]["toy_bear"]["piece"] = "g_unicorn"
        G["goods"]["bg_burger"]["goes"] = "away"
        G["sells"]["pizza"] = ["pz_slice", "moon_cheese"]
        G["goods"]["bread"] = dict(name="Bread again", price=100, goes="eat")
        p = world.mall_goods_problems(w)
        for bit in ("gt_acoustic: the price", "neon", "sets height", "g_unicorn", "bg_burger goes nowhere", "moon_cheese", "pizza outlets have only",
                    "bread is already"):
            self.assertTrue(any(bit in x for x in p), bit)

    def test_CHR_016_bought_clothes_are_kept_for_the_wardrobe_and_set_only_what_it_may_change(self):
        e, n = self.e, self.n
        parts = set(W["settings"]["wardrobe"]["parts"])
        wear = {g: G for g, G in GOODS["goods"].items() if G["goes"] == "wear"}
        self.assertGreater(len(wear), 30)
        for g, G in wear.items():                                            # putting one on gives a look the designer allows
            self.assertTrue(set(G["parts"]) <= parts, g)
            look = dict(W["designer"]["default"], **G["parts"])
            self.assertEqual(world.look_problems(W["designer"], look), [], g)
        kinds = {k for k, L in GOODS["sells"].items() if any(x in wear for x in L)}
        self.assertTrue({"clothes", "shoes", "sports"} <= kinds)             # clothes, shoes and sports shops sell things to wear
        u, g = self.first("shoes", "wear")
        e.buy_at(n, u, g)
        self.assertEqual(e.clothes, {g: 1})                                  # owned, not carried: it is in the wardrobe
        self.assertEqual(e.inv, {})

    def test_QST_008_what_was_bought_is_kept_in_the_save(self):
        e, n = self.e, self.n
        for kind, way in (("music", "home"), ("clothes", "wear"), ("cafe", "eat")):
            e.buy_at(n, *self.first(kind, way))
        e2 = town()
        e2.load(json.loads(json.dumps(e.save())))
        self.assertEqual((e2.homeware, e2.clothes, e2.inv), (e.homeware, e.clothes, e.inv))
        u, g = self.first("music", "home")
        self.assertEqual(e2.outlet_stock(n, u, g), e.outlet_stock(n, u, g))  # the shelf remembers what was taken


class TheBank(unittest.TestCase):
    def test_BANK_007_accounts_and_transfers(self):
        e, q = player()
        with self.assertRaises(ec.Refused):
            e.open_account("savings")                       # a current account first
        e.open_account("current")
        self.assertEqual(e.L.balance("p:current"), R["start"]["welcome_deposit"])
        e.open_account("savings")
        e.move("p:current", "p:savings", 1000)
        n = len(e.L.tx)
        with self.assertRaises(ec.Refused):
            e.move("p:savings", "p:cash", 1001)             # never partly made
        self.assertEqual(len(e.L.tx), n)
        e.move("p:savings", "p:cash", 1000)
        self.assertEqual(e.L.balance("p:savings"), 0)

    def test_BANK_001_savings_earn_interest_each_day(self):
        e, q = player()
        e.open_account("current")
        e.open_account("savings")
        e.move("p:current", "p:savings", 2000)
        bal = 2000
        for d in range(1, 6):
            e.advance(d, 10)
            bal += bal * R["bank"]["accounts"]["savings"]["rate_bp"] // 10000
            self.assertEqual(e.L.balance("p:savings"), bal)

    def test_BANK_008_many_days_at_once_equal_day_by_day(self):
        def run(steps):
            e, q = player()
            e.open_account("current")
            e.open_account("savings")
            e.move("p:current", "p:savings", 1500)
            e.take_loan(5000, 5)
            for d in steps:
                e.advance(d, 30)
            return [(t[1], t[3], t[4]) for t in e.L.tx]
        self.assertEqual(run(range(1, 9)), run([8]))

    def test_BANK_002_loans_are_repaid_on_schedule(self):
        e, q = player()
        e.open_account("current")
        loan = e.take_loan(6000, 3)
        total, plan = e.loan_terms(6000, 3)
        self.assertEqual(sum(plan), total)
        self.assertEqual(total, 6000 + 6000 * R["bank"]["loan"]["rate_bp"] * 3 // 10000)
        with self.assertRaises(ec.Refused):
            e.take_loan(2000, 3)                            # one at a time
        e.advance(5, 0)
        self.assertEqual(loan["status"], "repaid")
        self.assertEqual(loan["missed"], 0)
        invariant(self, e)

    def test_BANK_002_missed_payments_cost_fees_then_default_repossesses(self):
        e, q = player()
        e.open_account("current")
        loan = e.take_loan(30000, 3)
        e.move("p:current", "p:cash", e.L.balance("p:current"))   # nothing left to pay from
        e.give_item("hat", 2)
        e.advance(2, 100)
        self.assertEqual(loan["missed"], 1)
        self.assertEqual(loan["fees"], R["bank"]["loan"]["late_fee"])
        e.advance(10, 0)
        self.assertEqual(loan["status"], "defaulted")
        self.assertTrue(e.barred)
        self.assertNotIn("hat", e.inv)                      # repossessed toward the debt
        self.assertGreater(loan["taken"], 0)
        with self.assertRaises(ec.Refused):
            e.take_loan(2000, 3)
        invariant(self, e)


class TheStall(unittest.TestCase):
    def sales(self, price_factor, steps=(1080,)):
        e, q = player()
        e.give_item("flowers", 6)
        e.rent_stall()
        for _ in range(6):
            e.set_out("flowers", R["items"]["flowers"]["price"] * price_factor // 100)
        for m in steps:
            e.advance(0, m)
        return e

    def test_PROP_008_residents_buy_at_the_stall(self):
        visits = [t for t, i in VISITS.get(LOT[R["stall"]["lot"]], ()) if R["stall"]["open"][0] < t <= R["stall"]["open"][1]]
        self.assertGreater(len(visits), 5, "residents come to the market while stalls are open")
        cheap, dear = self.sales(80).stall_sold_total, self.sales(250).stall_sold_total
        self.assertGreater(cheap, 0)
        self.assertGreaterEqual(cheap, dear)                # cheaper sells more
        e = self.sales(100)
        self.assertEqual(e.stall_sold_total, self.sales(100, steps=range(500, 1081, 37)).stall_sold_total)   # bit by bit or at once
        unsold = 6 - e.stall_sold_total
        e.advance(1, 10)                                    # the stall closes and what is left comes home
        self.assertIsNone(e.stall)
        self.assertEqual(e.inv.get("flowers", 0), unsold)
        invariant(self, e)

    def test_PROP_008_stall_rules(self):
        e, q = player()
        with self.assertRaises(ec.Refused):
            e.set_out("bread", 100)                         # no stall yet
        e.rent_stall()
        self.assertEqual(e.L.balance("sys:fees"), R["stall"]["rent"])
        e.give_item("parcel")
        e.give_item("bread")
        with self.assertRaises(ec.Refused):
            e.set_out("parcel", 100)                        # quest items stay off the stall
        with self.assertRaises(ec.Refused):
            e.set_out("bread", 10 ** 6)                     # beyond the price limit
        e.set_out("bread", 400)
        e.take_back(0)
        self.assertEqual(e.inv, {"parcel": 1, "bread": 1})


class TheSave(unittest.TestCase):
    def test_QST_008_save_and_load_carry_on_the_same(self):
        e, q = player()
        e.open_account("current")
        e.take_loan(4000, 3)
        e.buy(GROCER, "bread")
        q.check(dict(lot=None, pos=None, minute=e.minute))
        blob = json.loads(json.dumps(dict(e=e.save(), q=q.save())))
        e2 = town()
        q2 = qs.Quests(W["quests"], e2, RESIDENTS, LOTS)
        e2.load(blob["e"])
        q2.load(blob["q"])
        for x in (e, e2):
            x.advance(4, 100)
        self.assertEqual([t[3:] for t in e.L.tx], [tuple(t[3:]) for t in e2.L.tx])
        self.assertEqual(q.active, q2.active)
        self.assertEqual(e.inv, e2.inv)

    def test_check_script_runs(self):
        script = ec.check_script() + ec.outlet_script(town())
        out = ec.run_script(town(), script, dict(grocer=GROCER, kiosk=KIOSK, arcade=ARCADE))
        self.assertTrue(all(r in ("ok", "refused") for r in out["results"]))
        self.assertGreater(out["results"].count("ok"), 15)
        at = [r for op, r in zip(script, out["results"]) if op[0] == "buyat"]   # the outlets' part of the run buys and is refused
        self.assertIn("ok", at)
        self.assertIn("refused", at)
        self.assertEqual(len(out["outlets"]), 12)


def path_to(nodes, start, want):
    """The replies from start that reach a reply whose acts include `want`, or None."""
    todo, seen = [(start, [])], {start}
    while todo:
        nid, path = todo.pop(0)
        for o in nodes[nid]["options"]:
            if want in o.get("act", []):
                return path + [o]
            if o["next"] in nodes and o["next"] not in seen:
                seen.add(o["next"])
                todo.append((o["next"], path + [o]))
    return None


def play_to(t, e, q, qid, ending):
    """Plays quest qid to the given ending as a player would: talking, walking, buying and selling. Returns money made."""
    Q = W["quests"]["quests"][qid]
    if qid not in q.active:
        t.assertIn(qid, q.offers(Q["giver"]))
        for o in path_to(Q["offer"]["nodes"], Q["offer"]["start"], "start"):
            q.act(qid, o.get("act", []), Q["giver"])
    for _ in range(40):
        if qid in q.done:
            break
        st = q.stage(qid)
        g = st["goal"]
        ctx = dict(lot=None, pos=None, minute=e.minute)
        if "talk" in g:
            p = path_to(st["nodes"], st["start"], "end:" + ending)
            if p is None:                                   # not here: move the line on to the next stage that can get there
                nxt = [o for nd in st["nodes"].values() for o in nd["options"] for a in o.get("act", []) if a.startswith("stage:")]
                p = path_to(st["nodes"], st["start"], nxt[0]["act"][-1])
            for o in p:
                for need in o.get("need", []):
                    k, _, v = need.partition(":")
                    if k == "have" and not e.inv.get(v):
                        e.give_item(v)
                t.assertTrue(q.need_ok(o.get("need", [])), o["text"])
                q.act(qid, o.get("act", []), g["talk"])
            continue
        if "go" in g:
            ctx["lot"] = g["go"]
        elif "near" in g:
            ctx["pos"] = tuple(g["near"])
            if not qs.in_window(e.minute, g.get("from", 0), g.get("to", 1440)):
                e.advance(e.day, g["from"])
                ctx["minute"] = e.minute
        elif "have" in g:
            shop = next(n for n in range(len(LOTS)) if e.shop_kind(n) and g["have"] in R["shops"][e.shop_kind(n)]["sells"])
            e.buy(shop, g["have"])
        elif "bank" in g:
            e.open_account("current")
        elif "money" in g:
            t.assertGreaterEqual(e.total_money(), g["money"])
        elif "stall" in g:
            e.rent_stall()
        elif "stall_sold" in g:
            if not e.stall:
                e.rent_stall()
            e.advance(e.day, 480)
            for it in ("flowers", "jam", "cheese", "bread", "apples", "candle"):
                e.give_item(it)
                e.set_out(it, R["items"][it]["price"] * 8 // 10)
            e.advance(e.day, 1080)
        q.check(ctx)
    t.assertEqual(q.done.get(qid), ending)


class TheQuests(unittest.TestCase):
    def test_QST_001_quest_data_is_sound(self):
        self.assertEqual(world.quest_problems(W), [])

    def test_QST_001_broken_quests_are_caught(self):
        w = copy.deepcopy(W)
        Q = w["quests"]["quests"]
        Q["new_in_town"]["stages"]["bank"]["then"] = "stage:nowhere"
        Q["errand_runner"]["giver"] = "r9999"
        Q["night_gardener"]["stages"]["who"]["nodes"]["w2"]["options"][0]["act"] = ["give:unicorn"]
        Q["own_stall"]["endings"]["solo"]["pay"] = 1.5
        p = world.quest_problems(w)
        for bit in ("nowhere", "r9999", "unicorn", "whole chips"):
            self.assertTrue(any(bit in x for x in p), bit)

    def test_QST_002_a_new_game_starts_the_first_line(self):
        e, q = player()
        first = W["quests"]["first"]
        self.assertIn(first, q.active)
        self.assertEqual(e.L.balance("p:cash"), R["start"]["cash"])
        goals = {k for st in W["quests"]["quests"][first]["stages"].values() for k in st["goal"]}
        self.assertTrue({"bank", "have", "talk"} <= goals)   # the bank, a shop and a resident
        starters = [k for k, v in W["quests"]["quests"].items() if v.get("starter")]
        self.assertGreaterEqual(len(starters), 4)
        for k in starters:                                  # each later line is opened by finishing an earlier one
            self.assertTrue(k == first or W["quests"]["quests"][k]["needs"], k)
        self.assertEqual(q.offers("r0518"), [])             # not offered until the first is done

    def test_QST_003_every_starter_line_branches_and_QST_004_pays_through_the_ledger(self):
        for qid, Q in W["quests"]["quests"].items():
            if not Q.get("starter"):
                continue
            outcomes = set()
            for ending in Q["endings"]:
                e, q = player()
                for need in self._before(qid):
                    play_to(self, e, q, need, next(iter(W["quests"]["quests"][need]["endings"])))
                rewards = e.L.balance("sys:rewards")
                play_to(self, e, q, qid, ending)
                E = Q["endings"][ending]
                self.assertEqual(rewards - e.L.balance("sys:rewards"), E.get("pay", 0), (qid, ending))
                outcomes.add((E.get("pay", 0), json.dumps(E.get("rep", {}), sort_keys=True)))
                invariant(self, e)
            self.assertGreaterEqual(len(outcomes), 2, qid)

    def _before(self, qid):
        out, todo = [], list(W["quests"]["quests"][qid].get("needs", []))
        while todo:
            n = todo.pop()
            if n not in out:
                out.insert(0, n)
                todo += W["quests"]["quests"][n].get("needs", [])
        return out


class TheJobs(unittest.TestCase):
    def ready(self):
        e, q = player()
        q.done.update(new_in_town="gift", errand_runner="delivered", night_gardener="helped")
        return e, q

    def test_QST_006_offers_are_worked_out_from_the_day(self):
        e, q = self.ready()
        day1 = [q.job_offer(i, 1) for i in range(len(RESIDENTS))]
        self.assertEqual(day1, [q.job_offer(i, 1) for i in range(len(RESIDENTS))])
        day2 = [q.job_offer(i, 2) for i in range(len(RESIDENTS))]
        self.assertNotEqual(day1, day2)
        kinds = collections.Counter(o["kind"] for o in day1 if o)
        self.assertTrue({"parcel", "shopping", "shift"} <= set(kinds), kinds)
        couriers = [i for i, r in enumerate(RESIDENTS) if r["role"] == "courier"]
        rate = sum(1 for d in range(1, 21) for i in couriers if q.job_offer(i, d)) / (20 * len(couriers))
        self.assertAlmostEqual(rate, W["quests"]["jobs"]["kinds"]["parcel"]["chance_bp"] / 10000, delta=.06)
        fresh, q2 = player()                                 # couriers only offer rounds after Errand runner
        self.assertFalse(any(q2.job_offer(i, 1) for i in couriers))

    def test_QST_006_one_job_at_a_time_and_failing_costs_reputation(self):
        e, q = self.ready()
        offers = [o for o in (q.job_offer(i, 0) for i in range(len(RESIDENTS))) if o]
        parcel = next(o for o in offers if o["kind"] == "parcel")
        q.take_job(parcel)
        with self.assertRaises(ec.Refused):
            q.take_job(next(o for o in offers if o is not parcel))
        self.assertIsNone(q.job_offer(parcel["giver"], 0))   # taken today
        gid = RESIDENTS[parcel["giver"]]["id"]
        q.check(dict(lot=LOTS[parcel["frm"]]["id"], pos=None, minute=e.minute))
        self.assertEqual(e.inv.get("parcel"), 1)
        e.advance(0, e.minute + 60 * parcel["hours"] + 1)
        q.check(dict(lot=None, pos=None, minute=e.minute))
        self.assertIsNone(q.job)
        self.assertEqual(q.rep(gid), W["quests"]["jobs"]["kinds"]["parcel"]["fail_rep"])
        self.assertNotIn("parcel", e.inv)

    def test_QST_006_jobs_pay_through_the_ledger(self):
        e, q = self.ready()
        offers = [o for o in (q.job_offer(i, 0) for i in range(len(RESIDENTS))) if o]
        shift = next(o for o in offers if o["kind"] == "shift")
        q.take_job(shift)
        lot = LOTS[shift["to"]]["id"]
        cash = e.L.balance("p:cash")
        for m in range(e.minute, e.minute + shift["minutes"] + 2, 5):
            e.advance(0, m)
            q.check(dict(lot=lot, pos=None, minute=m))
        self.assertIsNone(q.job)
        self.assertEqual(e.L.balance("p:cash"), cash + shift["pay"])
        shop = next(o for o in offers if o["kind"] == "shopping")
        q.take_job(shop)
        seller = next(n for n in range(len(LOTS)) if e.shop_kind(n) and shop["item"] in R["shops"][e.shop_kind(n)]["sells"])
        spent = e.buy(seller, shop["item"])
        before = e.total_money()
        q.hand_over()
        self.assertEqual(e.total_money(), before + R["items"][shop["item"]]["price"] + shop["pay"])
        self.assertGreaterEqual(R["items"][shop["item"]]["price"] + shop["pay"], spent)
        invariant(self, e)


class TheDesigner(unittest.TestCase):
    def test_CHR_013_designer_data_is_sound(self):
        self.assertEqual(world.designer_problems(W), [])

    def test_CHR_013_broken_designer_data_is_caught(self):
        w = copy.deepcopy(W)
        w["designer"]["sliders"]["height"]["max"] = 2.1
        w["designer"]["hair"].append("mohawk")
        w["designer"]["default"]["top"] = "neon"
        p = world.designer_problems(w)
        for bit in ("height", "mohawk", "neon"):
            self.assertTrue(any(bit in x for x in p), bit)

    def test_CHR_001_a_new_player_chooses_body_face_hair_skin_and_outfit(self):
        d = W["designer"]
        for k in ("height", "build", "face_w", "eye_size", "nose", "lips"):          # body and face
            self.assertIn(k, d["sliders"])
        self.assertGreaterEqual(len(d["hair"]), 5)
        self.assertGreaterEqual(len(d["skin"]), 8)
        self.assertGreaterEqual(len(d["garment"]), 3)
        self.assertGreaterEqual(len(d["bottom_type"]), 2)
        e, q = player()                                     # and the new game that follows starts the first quest (QST-002)
        self.assertEqual(list(q.active), [W["quests"]["first"]])

    def test_CHR_002_a_character_is_a_few_values(self):
        d = W["designer"]
        v = dict(d["default"], height=1.8, build=.88, hair="bald", cap="ochre", tash=True)
        self.assertEqual(world.look_problems(d, v), [])
        self.assertLess(len(json.dumps(v)), 600)            # compact data, not meshes
        self.assertTrue(world.look_problems(d, dict(v, height=1.9)))
        self.assertTrue(world.look_problems(d, dict(v, name="")))


class TheHash(unittest.TestCase):
    def test_hash_matches_the_slice(self):
        """Values from hash() in Prototype/play/play.js, worked out by hand from its definition."""
        for args in (("a",), (1, 2, "b"), ("r0504", 16, "job")):
            h = ec.hash01(*args)
            self.assertTrue(0 <= h < 1)
        self.assertNotEqual(ec.hash01(1, 2), ec.hash01(12))   # arguments are kept apart


if __name__ == "__main__":
    unittest.main()
