"""The real estate market (F-PROP-ESTATE level 2, ADR-0022). Runs over World/ through Tools/worldgen/world.py.
        python -m unittest test_estate -v

Each test names the requirement it proves (docs/specs/GAME_SPEC.md §4.5)."""
import collections, copy, json, os, sys, unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import world                                               # noqa: E402
import economy as ec                                       # noqa: E402
import estate as es                                        # noqa: E402

W = world.load()
LOTS = world.lots(W)
R = W["estate"]
B = R["balance"]


def town(start=True, cash=0):
    e = ec.Economy(W["economy"], LOTS, {}, {})
    s = es.Estate(R, e, LOTS, W["residents"])
    e.advance(0, 440)
    e.new_character()
    if start:
        s.start()
    e.open_account("current")
    if cash:
        e.L.post(e.day, e.minute, "Test money", [("sys:start", -cash), ("p:current", cash)])
    return e, s


def cheapest(s, kinds, pred=lambda i: True, by=None):
    c = [i for i, u in enumerate(s.units) if u["kind"] in kinds and pred(i)]
    return min(c, key=lambda i: ((by or s.ask)(i) or 1 << 60, i))


def run(e, days):
    for _ in range(days):
        e.advance(e.day + 1, 0)


class Units(unittest.TestCase):
    def test_PROP_009_every_building_is_carved_into_units(self):
        e, s = town(start=False)
        lots_with = {u["lot"] for u in s.units}
        for n, l in enumerate(LOTS):
            if l["kind"] in R["public_kinds"]:
                self.assertNotIn(n, lots_with, l["id"])
            elif l.get("floors"):
                self.assertIn(n, lots_with, l["id"])
        ids = [u["id"] for u in s.units]
        self.assertEqual(len(ids), len(set(ids)))
        kinds = collections.Counter(u["kind"] for u in s.units)
        for k in ("house", "flat", "shop", "office", "plant", "civic"):
            self.assertGreater(kinds[k], 0, k)
        for n in s.by_lot.values():                                # no floor in two units
            fl = [f for i in n for f in s.units[i]["floors"]]
            self.assertEqual(len(fl), len(set(fl)))
        tower = next(n for n, l in enumerate(LOTS) if l["kind"] == "home" and "flat" in (l.get("storeys") or []))
        self.assertGreater(len(s.by_lot[tower]), 4, "a tower has a flat per floor")
        for u in s.units:
            self.assertGreater(u["area"], 0)

    def test_PROP_009_unit_ids_do_not_change(self):
        a = es.carve(R, LOTS, W["residents"])
        b = es.carve(R, copy.deepcopy(LOTS), W["residents"])
        self.assertEqual([u["id"] for u in a], [u["id"] for u in b])
        self.assertTrue(all(u["id"].startswith(u["lot_id"] + ":u") for u in a))

    def test_PROP_010_every_unit_has_an_owner_and_companies_share_the_town(self):
        e, s = town()
        ok = {"city", "hh", "firm"} | {"co:" + c["id"] for c in R["companies"]}
        self.assertTrue(all(o in ok for o in s.owner))
        cos = collections.Counter(o for o in s.owner if o.startswith("co:"))
        self.assertGreaterEqual(len(cos), 2, "more than one real estate company")
        self.assertLess(max(cos.values()), len(s.units) // 2, "no company owns half the town")
        for i, o in enumerate(s.owner):                            # a household owns only the home it lives in
            if o == "hh":
                self.assertEqual(s.tenant[i], "hh")
        for k in R["kinds"]:                                       # every kind can find a buyer
            self.assertGreaterEqual(sum(1 for c in R["companies"] if k in c["focus"]), 1 if k in ("showroom", "plant", "civic") else 2, k)


class Buying(unittest.TestCase):
    def test_PROP_011_value_follows_size_kind_place_and_floor(self):
        e, s = town()
        for i, u in enumerate(s.units):
            self.assertEqual(s.base[i], u["area"] * R["kinds"][u["kind"]]["per_m2"] * u["loc_bp"] // 10000 * u["floor_bp"] // 10000)
            self.assertIs(type(s.value(i)), int)
        L = R["location"]
        self.assertTrue(all(L["far_bp"] <= u["loc_bp"] <= L["near_bp"] for u in s.units))

    def test_PROP_011_buy_a_unit_with_transfer_tax_through_the_ledger(self):
        e, s = town(cash=900000)
        i = cheapest(s, ["flat"])
        p = s.ask(i)
        seller = s.acct(s.owner[i])
        before, sb, tax = e.total_money(), e.L.balance(seller), e.L.balance("sys:tax")
        s.buy([i], "lark")
        t = p * R["trade"]["transfer_tax_bp"] // 10000
        self.assertEqual(s.owner[i], "player")
        self.assertEqual(before - e.total_money(), p + t)
        self.assertEqual(e.L.balance("sys:tax") - tax, t)
        self.assertGreater(e.L.balance(seller), sb)
        self.assertEqual(e.L.system_total() + e.L.supply(), 0)

    def test_PROP_011_refused_whole_when_short(self):
        e, s = town()
        i = cheapest(s, ["civic"])
        n, owner = len(e.L.tx), s.owner[i]
        with self.assertRaises(ec.Refused):
            s.buy([i], "harrow")
        self.assertEqual((len(e.L.tx), s.owner[i]), (n, owner))

    def test_PROP_011_buy_a_whole_building(self):
        e, s = town(cash=90000000)
        lot = next(n for n, ix in s.by_lot.items() if len(ix) >= 3 and all(s.ask(i) for i in ix))
        ix = s.by_lot[lot]
        with self.assertRaises(ec.Refused):
            s.buy(ix[:2], "harrow")
        s.buy(ix, "harrow")
        self.assertTrue(all(s.owner[i] == "player" for i in ix))

    def test_PROP_011_everything_but_public_land_can_be_bought(self):
        e, s = town()
        for i, u in enumerate(s.units):
            if s.owner[i] != "city" or u["kind"] in R["trade"]["city_sells"]:
                self.assertIsNotNone(s.ask(i), u["id"])


class Selling(unittest.TestCase):
    def test_PROP_012_sell_at_once_to_the_best_bid(self):
        e, s = town(cash=900000)
        i = cheapest(s, ["house"])
        s.buy([i], "lark")
        bid = s.bid(i)
        before = e.total_money()
        s.sell_now(i)
        self.assertTrue(s.owner[i].startswith("co:"))
        self.assertEqual(e.total_money() - before, bid)

    def test_PROP_012_list_through_an_agent_and_a_buyer_comes(self):
        e, s = town(cash=900000)
        i = cheapest(s, ["house"])
        s.buy([i], "lark")
        with self.assertRaises(ec.Refused):
            s.list_for_sale(i, s.value(i) * 10, "lark")
        price = s.value(i) * 9800 // 10000
        s.list_for_sale(i, price, "lark")
        agent0 = e.L.balance("co:lark")
        for _ in range(40):
            run(e, 1)
            if s.owner[i] != "player":
                break
        self.assertNotEqual(s.owner[i], "player", "a fairly priced listing sells within 40 days")
        sale = next(x for x in reversed(s.sales) if x[1] == i)
        self.assertEqual(sale[2], price)
        self.assertTrue(any(t[3].startswith("Sold") and ("co:lark", price * R["trade"]["agent_fee_bp"] // 10000) in t[4] for t in e.L.tx))
        self.assertNotEqual(agent0, e.L.balance("co:lark"))

    def test_PROP_012_the_town_trades_on_its_own(self):
        e, s = town()
        first = list(s.owner)
        run(e, 60)
        self.assertGreater(sum(h[3] for h in s.history), 20)
        self.assertGreater(sum(1 for a, b in zip(first, s.owner) if a != b), 20)
        self.assertTrue(any(a != "hh" and b == "hh" for a, b in zip(first, s.owner)), "residents buy their homes")


class Renting(unittest.TestCase):
    def test_PROP_013_rent_a_place_and_pay_by_the_day(self):
        e, s = town(cash=20000)
        i = cheapest(s, ["flat", "house"], lambda i: s.tenant[i] is None, s.market_rent)
        r = s.market_rent(i)
        before = e.total_money()
        s.rent_unit(i)
        self.assertEqual(s.tenant[i], "player")
        self.assertEqual(before - e.total_money(), r)
        run(e, 2)
        self.assertEqual(before - e.total_money(), 3 * r)
        s.leave(i)
        self.assertIsNone(s.tenant[i])

    def test_PROP_013_evicted_after_three_unpaid_days(self):
        e, s = town()
        i = cheapest(s, ["flat", "house"], lambda i: s.tenant[i] is None, s.market_rent)
        s.rent_unit(i)
        e.L.post(e.day, e.minute, "Spend it all", [("p:cash", -e.L.balance("p:cash")), ("p:current", -e.L.balance("p:current")), ("sys:spend", e.total_money())])
        run(e, R["letting"]["missed_before_eviction"])
        self.assertNotEqual(s.tenant[i], "player")

    def test_PROP_014_let_what_you_own(self):
        e, s = town(cash=900000)
        i = cheapest(s, ["flat"], lambda i: s.tenant[i] is None)
        s.buy([i], "lark")
        m = s.market_rent(i)
        with self.assertRaises(ec.Refused):
            s.set_rent(i, m * 5)
        s.set_rent(i, m)
        got = 0
        for _ in range(30):
            before = e.total_money()
            run(e, 1)
            got += max(0, e.total_money() - before)
            if s.tenant[i] in ("lodger", "hh"):
                break
        self.assertIn(s.tenant[i], ("lodger", "hh"), "a lodger or a household that moves house takes it (PROP-015, PROP-022)")

    def test_PROP_014_sitting_tenants_pay_no_more_than_the_market(self):
        e, s = town(cash=900000)
        i = cheapest(s, ["house"], lambda i: s.tenant[i] == "hh" and s.owner[i] != "hh")
        s.buy([i], "lark")
        s.set_rent(i, s.market_rent(i) * 2)
        self.assertEqual(s.rent_of(i), s.market_rent(i))

    def test_PROP_015_dear_rent_keeps_lodgers_away(self):
        e, s = town(cash=90000000)
        cheap = [i for i, u in enumerate(s.units) if u["kind"] in ("flat", "office") and s.tenant[i] is None][:8]
        self.assertTrue(cheap)
        for i in cheap:
            s.buy([i], "lark")
        half = len(cheap) // 2
        for k, i in enumerate(cheap):
            m = s.market_rent(i)
            s.set_rent(i, m * (19000 if k < half else 10000) // 10000)
        filled = collections.Counter()
        for _ in range(20):
            run(e, 1)
            for k, i in enumerate(cheap):
                filled[k < half] += s.tenant[i] == "lodger"
        self.assertLess(filled[True], filled[False])


class Market(unittest.TestCase):
    def test_PROP_016_prices_rise_and_fall_within_bounds(self):
        e, s = town()
        I = R["index"]
        seen = collections.defaultdict(set)
        for _ in range(60):
            run(e, 1)
            for b, v in s.idx.items():
                self.assertTrue(I["min_bp"] <= v <= I["max_bp"])
                seen[b].add(v)
        self.assertTrue(all(len(v) > 5 for v in seen.values()), "every block's prices move")
        self.assertEqual(len(s.history), min(60, R["history_days"]))

    def test_PROP_016_buying_up_a_block_raises_its_prices(self):
        e, s = town(cash=90000000)
        b = max(s.blocks, key=lambda b: sum(1 for u in s.units if u["block"] == b))
        e2, s2 = town(cash=90000000)
        ix = [i for i, u in enumerate(s.units) if u["block"] == b and s.ask(i)][:5]
        for i in ix:
            s.buy([i], "lark")
        run(e, 1)
        run(e2, 1)
        self.assertGreater(s.idx[b], s2.idx[b])

    def test_PROP_017_unpaid_tax_and_the_town_sells_it(self):
        e, s = town(cash=900000)
        i = cheapest(s, ["house"])
        s.buy([i], "lark")
        s.to_let[i] = False
        e.L.post(e.day, e.minute, "Spend it all", [("p:cash", -e.L.balance("p:cash")), ("p:current", -e.L.balance("p:current")), ("sys:spend", e.total_money())])
        if s.tenant[i] in ("hh", "firm", "lodger"):
            s.tenant[i] = None                                   # no rent coming in to pay the tax with
        run(e, R["costs"]["missed_before_seizure"])
        self.assertNotEqual(s.owner[i], "player")
        self.assertGreater(e.total_money(), 0, "the rest of the price comes back")

    def test_PROP_018_the_money_supply_holds_steady(self):
        e, s = town()
        run(e, 60)
        a = e.L.supply()
        run(e, B["index_days"] - 60)
        z = e.L.supply()
        self.assertLess(abs(z - a) * 10000 // a, B["supply_drift_bp"], (a, z))
        self.assertGreater(e.L.balance(es.TOWN), 0)
        self.assertEqual(e.L.supply() + e.L.system_total(), 0)
        idx = [h[1] for h in s.history]
        self.assertTrue(all(R["index"]["min_bp"] <= v <= R["index"]["max_bp"] for v in idx))
        for c in R["companies"]:
            self.assertGreaterEqual(e.L.balance("co:" + c["id"]), 0)

    def test_PROP_018_money_comes_and_goes_only_through_named_system_accounts(self):
        e, s = town()
        run(e, 10)
        sysaccts = {a for a in e.L.bal if a.startswith("sys:")}
        self.assertTrue({"sys:wages", "sys:tax", "sys:spend", "sys:founding"} <= sysaccts)
        for t in e.L.tx:
            self.assertEqual(sum(v for _, v in t[4]), 0)


class Mortgages(unittest.TestCase):
    def test_BANK_002_PROP_019_a_mortgage_buys_a_home_and_is_paid_by_the_day(self):
        e, s = town(cash=200000)
        i = cheapest(s, ["house"])
        p = s.ask(i)
        C = R["mortgage"]
        with self.assertRaises(ec.Refused):
            s.buy([i], "lark", deposit=p // 10, term=90)
        vault = e.L.balance("bank:vault")
        s.buy([i], "lark", deposit=p * C["deposit_bp"] // 10000 + 1, term=30)
        m = s.mortgages[0]
        self.assertEqual(vault - e.L.balance("bank:vault"), m["principal"])
        run(e, 3)
        self.assertEqual(m["paid"], sum(m["plan"][:2]), "days 1 and 2 after the purchase")

    def test_PROP_019_default_and_the_bank_sells_it(self):
        e, s = town(cash=200000)
        i = cheapest(s, ["house"])
        p = s.ask(i)
        s.buy([i], "lark", deposit=p * R["mortgage"]["deposit_bp"] // 10000 + 1, term=30)
        e.L.post(e.day, e.minute, "Spend it all", [("p:cash", -e.L.balance("p:cash")), ("p:current", -e.L.balance("p:current")), ("sys:spend", e.total_money())])
        s.tenant[i] = None if s.tenant[i] == "lodger" else s.tenant[i]
        s.to_let[i] = False
        run(e, R["mortgage"]["default_after_missed"] + 1)
        self.assertNotEqual(s.owner[i], "player")
        self.assertNotEqual(s.mortgages[0]["status"], "active")
        self.assertTrue(e.barred)

    def test_PROP_019_selling_pays_off_the_mortgage(self):
        e, s = town(cash=200000)
        i = cheapest(s, ["house"])
        p = s.ask(i)
        s.buy([i], "lark", deposit=p // 2, term=90)
        owed = s.mortgage_owed(s.mortgages[0])
        vault = e.L.balance("bank:vault")
        s.sell_now(i)
        self.assertEqual(e.L.balance("bank:vault") - vault, owed)
        self.assertEqual(s.mortgages[0]["status"], "repaid")


class Balance(unittest.TestCase):
    """PROP-020: the numbers measured against what a player earns (estate.json balance)."""

    def setUp(self):
        self.e, self.s = town()
        self.inc = B["player_day_income"]
        self.home = cheapest(self.s, ["house", "flat"])

    def test_PROP_020_renting_a_home_is_affordable(self):
        s = self.s
        r = min(s.market_rent(i) for i, u in enumerate(s.units) if u["kind"] in ("house", "flat"))
        self.assertLessEqual(r * 10000, self.inc * B["rent_share_bp"], r)

    def test_PROP_020_a_deposit_is_days_of_work_not_months(self):
        s = self.s
        p = s.ask(self.home)
        dep = p * R["mortgage"]["deposit_bp"] // 10000 + p * R["trade"]["transfer_tax_bp"] // 10000
        self.assertLessEqual(dep, B["deposit_days"] * self.inc, (p, dep))

    def test_PROP_020_a_mortgage_is_affordable(self):
        s = self.s
        p = s.ask(self.home)
        loan = p - p * R["mortgage"]["deposit_bp"] // 10000
        total, plan = s.mortgage_terms(loan, max(R["mortgage"]["terms"]))
        day = plan[0] + s.cost_of(self.home)
        self.assertLessEqual(day * 10000, self.inc * B["mortgage_share_bp"], day)

    def test_PROP_020_letting_pays_back_slowly(self):
        s = self.s
        lo, hi = B["payback_days"]
        for kind in ("house", "flat", "shop", "office"):
            i = cheapest(s, [kind])
            p = s.ask(i)
            net = s.market_rent(i) - s.value(i) * (R["costs"]["upkeep_bp"] + R["costs"]["tax_bp"]) // 10000
            days = (p + p * R["trade"]["transfer_tax_bp"] // 10000) // net
            self.assertTrue(lo <= days <= hi, (kind, days))

    def test_PROP_020_no_quick_loop_makes_money(self):
        """Buy and sell straight back, or buy listings and relist at value: each loses money."""
        e, s = town(cash=9000000)
        start = e.total_money()
        for _ in range(5):
            i = cheapest(s, ["house", "flat", "office"], lambda i: s.owner[i] != "player")
            s.buy([i], "lark")
            s.sell_now(i)
        self.assertLess(e.total_money(), start)


class Moving(unittest.TestCase):
    def test_PROP_022_renting_households_move_house(self):
        e, s = town()
        first = {r["id"]: r["home"] for r in W["residents"]}
        run(e, 60)
        self.assertGreater(len(s.moves), 5)
        moved = [rid for rid, h in first.items() if s.home_of(rid) != h]
        self.assertTrue(moved)
        seen = set()
        for ms in s.members:                                       # everyone lives in exactly one place
            self.assertFalse(seen & set(ms))
            seen |= set(ms)
        self.assertEqual(seen, set(first))
        for i, ms in enumerate(s.members):
            self.assertEqual(bool(ms), s.tenant[i] == "hh", s.units[i]["id"])
            if ms:
                self.assertGreaterEqual(s.units[i]["beds"], len(ms) if i in {m[2] for m in s.moves} else 0)
        for _, a, b in s.moves:                                    # only renters move, into homes
            self.assertIn(s.units[b]["kind"], ("house", "flat"))

    def test_PROP_022_a_resident_who_moved_sleeps_at_the_new_home(self):
        import citysim
        e, s = town()
        run(e, 60)
        rid = next(r["id"] for r in W["residents"] if s.home_of(r["id"]) != r["home"])
        data = world.sim_data(W)
        data["residents"] = [dict(r, home=s.home_of(r["id"])) for r in W["residents"]]
        plan = citysim.City(data=data).plan(16)[rid]
        night = [p for p in plan if p[0] == "stay" and p[1] < 300]
        self.assertTrue(night)
        self.assertEqual(night[0][4]["place"], s.home_of(rid))

    def test_PROP_022_moves_survive_a_saved_game(self):
        e, s = town()
        run(e, 30)
        s2 = es.Estate(R, e, LOTS, W["residents"])
        s2.load(s.save())
        self.assertEqual({r["id"]: s2.home_of(r["id"]) for r in W["residents"]}, {r["id"]: s.home_of(r["id"]) for r in W["residents"]})
        self.assertEqual(s2.wage, s.wage)

    def test_PROP_023_agents_offices_carry_their_names(self):
        names = {l["id"]: l["name"] for l in LOTS}
        for c in R["companies"]:
            self.assertEqual(names[c["office"]], c["name"])

    def test_PROP_023_boards_say_what_is_for_sale_and_to_let(self):
        e, s = town()
        run(e, 10)
        for lot, ix in s.by_lot.items():
            sale, let = s.board(lot)
            self.assertEqual(sale, sum(1 for i in ix if str(i) in s.listing and s.owner[i] != "player"))
            self.assertEqual(let, sum(1 for i in ix if s.tenant[i] is None and s.owner[i] != "player"))
        self.assertTrue(any(s.board(l)[0] for l in s.by_lot) and any(s.board(l)[1] for l in s.by_lot))


class Determinism(unittest.TestCase):
    def test_PROP_021_the_same_game_gives_the_same_market(self):
        a = es.run_script(es.Estate(R, ec.Economy(W["economy"], LOTS, {}, {}), LOTS, W["residents"]), es.check_script())
        b = es.run_script(es.Estate(R, ec.Economy(W["economy"], LOTS, {}, {}), LOTS, W["residents"]), es.check_script())
        self.assertEqual(a, b)
        self.assertIn("ok", a["results"])

    def test_PROP_021_a_saved_game_carries_on_the_same(self):
        e, s = town(cash=900000)
        s.buy([cheapest(s, ["flat"])], "lark")
        run(e, 5)
        se, ss = e.save(), s.save()
        run(e, 10)
        want = (e.L.supply(), list(s.owner), len(e.L.tx), dict(s.idx))
        e2 = ec.Economy(W["economy"], LOTS, {}, {})
        s2 = es.Estate(R, e2, LOTS, W["residents"])
        e2.load(se)
        e2.hooks = [s2.settle]
        s2.load(ss)
        run(e2, 10)
        self.assertEqual((e2.L.supply(), list(s2.owner), len(e2.L.tx), dict(s2.idx)), want)

    def test_PROP_021_the_rules_data_checks_out(self):
        self.assertEqual(world.estate_problems(W), [])


if __name__ == "__main__":
    unittest.main()
