"""NPC careers and staffing (LIVING_CITY.md 3.2, CITY-081 to CITY-086), on the rules in World/rules/careers.json and
over the whole starting city in World/.        python -m unittest test_careers -v"""
import collections, copy, os, random, sys, unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import world                                               # noqa: E402
import careers                                             # noqa: E402

W = world.load()
R = W["careers"]
C = R["careers"]
LOTS = world.lots(W)
BYID = {l["id"]: l for l in LOTS}
TMPL = {l["id"]: b["template"] for b in W["blocks"] for l in b["lots"]}
RES = W["residents"]
DAY = 16                                                    # a Tuesday, not a market day


def staff_of():
    """{lot id: Counter(career)} of the residents who work there."""
    out = collections.defaultdict(collections.Counter)
    for r in RES:
        if r.get("work") and r.get("career"):
            out[r["work"]][r["career"]] += 1
    return out


def of_type(t):
    return [l for l in LOTS if "plan" not in l and careers.workplace_type(l, R, TMPL[l["id"]]) == t]


def mini_city():
    """Two homes, a café and a grocer, and eight people who work in town: enough to watch the rules work."""
    lot = lambda i, kind, name, x: dict(id=i, kind=kind, name=name, rect=[x, 0, x + 20, 20], door=[x + 10, 20], floors=1, enterable=True, objects={})
    lots = [lot("h1", "home", "Home", 0), lot("h2", "home", "Home", 300), lot("c", "food", "Cafe Test", 30), lot("g", "shop", "Test Grocer", 260)]
    people = [dict(id="r%02d" % k, role="clerk", home="h1" if k < 4 else "h2", work=None) for k in range(8)]
    rules = copy.deepcopy(R)
    rules["shares"] = {g: 0.0 for g in rules["shares"]}
    rules["shares"].update(food=0.5, retail=0.5)
    return people, lots, rules


class TheCareers(unittest.TestCase):
    def test_CITY_081_more_than_fifty_careers_in_data(self):
        self.assertEqual(careers.problems(R, W["roles"]["roles"]), [])
        self.assertGreater(len(C), 50)
        self.assertGreaterEqual(len(C), R["targets"]["careers_at_least"])
        in_use = {r["career"] for r in RES if r.get("career")}
        self.assertGreater(len(in_use), 50)                                  # and the city really has them
        for c in C.values():
            self.assertIn(c["role"], W["roles"]["roles"])
            self.assertTrue(c["title"])

    def test_CITY_081_everyone_who_works_has_a_career(self):
        for r in RES:
            if r["role"] in ("child", "student", "elder"):
                self.assertNotIn("career", r, r["id"])
            else:
                self.assertIn(r.get("career"), C, r["id"])
        self.assertEqual(world.career_problems(W), [])
        broken = copy.deepcopy(W)
        broken["residents"][0]["career"] = "astronaut"
        self.assertTrue(any("unknown career astronaut" in p for p in world.career_problems(broken)))


class TheStaffing(unittest.TestCase):
    def test_CITY_082_cafes_have_two_at_the_counter_two_baristas_and_one_cleaning_tables(self):
        S = staff_of()
        cafes = of_type("cafe")
        self.assertGreater(len(cafes), 20)
        for l in cafes:
            s = S[l["id"]]
            self.assertTrue(1 <= s["cafe_cashier"] <= 2, (l["name"], dict(s)))
            self.assertTrue(1 <= s["barista"] <= 2, (l["name"], dict(s)))
            self.assertEqual(s["table_attendant"], 1, (l["name"], dict(s)))
            self.assertLessEqual(sum(s.values()), 5, l["name"])
            self.assertEqual(set(s), {"cafe_cashier", "barista", "table_attendant"}, l["name"])

    def test_CITY_082_grocers_have_two_or_three_at_the_tills_and_one_or_two_on_the_floor(self):
        S = staff_of()
        grocers = of_type("grocer")
        self.assertGreater(len(grocers), 20)
        for l in grocers:
            s = S[l["id"]]
            self.assertTrue(2 <= s["checkout_operator"] <= 3, (l["name"], dict(s)))
            self.assertTrue(1 <= s["stock_clerk"] + s["shop_floor_assistant"] <= 2, (l["name"], dict(s)))
            self.assertEqual(set(s) - {"checkout_operator", "stock_clerk", "shop_floor_assistant"}, set(), l["name"])
        sizes = {sum(S[l["id"]].values()) for l in grocers}
        self.assertGreater(len(sizes), 1)                                    # bigger grocers have more staff

    def test_CITY_082_every_workplace_within_its_posts(self):
        S = staff_of()
        for l in LOTS:
            ps = careers.posts(l, R, TMPL[l["id"]])
            if not ps or not l.get("enterable", True) and not careers.outdoors(l, R, TMPL[l["id"]]):
                continue
            cap = collections.Counter(c for c, _ in ps)
            for c, n in S[l["id"]].items():
                self.assertLessEqual(n, cap[c], (l["name"], c))
            for k in careers.minimum(l, R, TMPL[l["id"]]):                    # every place opens with its minimum
                c = ps[k][0]
                self.assertGreaterEqual(S[l["id"]][c], 1, (l["name"], c))
        for lid, s in S.items():                                             # nobody works where there are no posts
            self.assertTrue(careers.posts(BYID[lid], R, TMPL[lid]) or BYID[lid]["kind"] == "garden", BYID[lid]["name"])

    def test_CITY_082_no_more_crowds_behind_the_counter(self):
        S = staff_of()
        for t, most in (("cafe", 5), ("grocer", 5), ("bakery", 4), ("kiosk", 2), ("works", 21), ("clinic", 7)):
            for l in of_type(t):
                self.assertLessEqual(sum(S[l["id"]].values()), most, (t, l["name"]))
        for l in LOTS:                                                       # each mall unit is staffed like a shop
            if "plan" in l:
                units = collections.Counter()
                ps = careers.posts(l, R)
                for c, u in ps:
                    units[u] += 1
                self.assertLessEqual(max(units.values()), 15, l["name"])

    def test_CITY_082_the_market_has_its_trader(self):
        market = next(l for l in LOTS if l["kind"] == "market")
        self.assertEqual(staff_of()[market["id"]]["market_trader"], 1)

    def test_CITY_082_bigger_places_have_more_posts(self):
        spec = R["staffing"]["cafe"]
        small = careers.head_count(spec, spec["small_m2"] - 1)
        big = careers.head_count(spec, spec["large_m2"] + 1)
        self.assertEqual(small, [p["min"] for p in spec["posts"]])
        self.assertEqual(big, [p["max"] for p in spec["posts"]])
        self.assertLessEqual(sum(careers.head_count(spec, (spec["small_m2"] + spec["large_m2"]) / 2)), sum(big))


def mini_mall():
    """mini_city() with a small mall of outlets by the second home: two shops on the ground floor, a food outlet
    upstairs, a corner store and a kiosk; one guard and one janitor for the whole of it."""
    people, lots, rules = mini_city()
    size = dict(anchor=(80, 45), kiosk=(3, 3))                              # a corner store of 3,600 m2; a kiosk too small for staff
    unit = lambda i, kind, x, **kw: dict(id=i, name=i, kind=kind, rect=[x, 0, x + size.get(kind, (16, 9))[0], size.get(kind, (16, 9))[1]], angle=0, **kw)
    mall = dict(id="m", kind="shop", name="Test Mall", rect=[400, 0, 520, 120], door=[460, 120], floors=2, enterable=True, objects={}, tags=["mall"],
                plan=dict(levels=[dict(level=0, units=[unit("a", "anchor", 400), unit("s1", "shop", 420, outlet="books"), unit("s2", "shop", 440, outlet="toys"), unit("k", "kiosk", 460)]),
                                  dict(level=1, units=[unit("a", "anchor", 400), unit("f1", "food", 420, outlet="cafe")])]))
    rules["mall_staff"] = dict(keeper=dict(shop="shop_owner", food="food_outlet_owner"),
                               whole_mall=[dict(career="security_guard", count=1), dict(career="janitor", count=1)])
    return people, lots + [mall], rules, mall


class TheMallsStaff(unittest.TestCase):
    """CITY-105 (the owner, 2026-11-24): "some taking career as shop owners at the mall outlets, i think one person per
    outlet is good enough. few security, ganitors etc, can be added to handle entire Mall"."""

    def test_CITY_105_one_keeper_to_each_outlet_on_every_floor_and_a_few_for_the_whole_mall(self):
        people, lots, rules, mall = mini_mall()
        ps = careers.posts(mall, rules)
        self.assertEqual(ps[:5], [("shop_owner", "s1"), ("shop_owner", "s2"), ("food_outlet_owner", "f1"), ("security_guard", None), ("janitor", None)])
        self.assertEqual(careers.minimum(mall, rules), [0, 1, 2, 3, 4])      # all of them are filled before anyone else is placed
        self.assertEqual([u["id"] for u in careers.mall_outlets(mall)], ["s1", "s2", "f1"])
        self.assertTrue(len(ps) > 5 and all(u == "a" for _, u in ps[5:]))    # then the corner store's, by its floor area, as a cap; the kiosk is too small
        self.assertNotIn("s1", [u for _, u in ps[5:]])                       # an outlet has its one keeper and no more
        rules["mall_staff"]["whole_mall"][0]["count"] = 3
        self.assertEqual(collections.Counter(c for c, u in careers.posts(mall, rules) if u is None), {"security_guard": 3, "janitor": 1})

    def test_CITY_105_every_outlet_gets_its_keeper_before_anyone_else_is_placed(self):
        people, lots, rules, mall = mini_mall()
        people += [dict(id="x%02d" % k, role="clerk", home="h2", work=None) for k in range(6)]
        a = careers.assign(people, lots, rules)
        at = {v["unit"]: (rid, v["career"]) for rid, v in a.items() if v["work"] == "m"}
        self.assertEqual({u: c for u, (_, c) in at.items()}, {"s1": "shop_owner", "s2": "shop_owner", "f1": "food_outlet_owner", None: at[None][1]})
        staff = sorted((v["post"], v["career"], rid) for rid, v in a.items() if v["work"] == "m")
        self.assertEqual([c for _, c, _ in staff], ["shop_owner", "shop_owner", "food_outlet_owner", "security_guard", "janitor"])   # their posts are in the order of posts()
        self.assertTrue(all(people_by_id(people)[rid]["home"] == "h2" for _, _, rid in staff))   # the nearest residents take them
        rules["shares"] = dict(rules["shares"], retail=0.0, food=0.0, facilities=0.0, office=1.0)
        lots.append(dict(id="o", kind="work", name="Test Office", rect=[0, 100, 40, 140], door=[20, 100], floors=4, enterable=True, objects={}))
        b = careers.assign(people, lots, rules)
        self.assertEqual(sum(1 for v in b.values() if v["work"] == "m"), 5)  # filled whatever the shares say: they are the mall's minimum
        self.assertEqual(careers.assign(list(reversed(people)), list(reversed(lots)), rules), b)

    def test_CITY_105_a_mall_without_outlets_is_staffed_as_before(self):
        people, lots, rules, mall = mini_mall()
        for L in mall["plan"]["levels"]:
            for u in L["units"]:
                u.pop("outlet", None)
        ps = careers.posts(mall, rules)
        self.assertEqual(careers.minimum(mall, rules), [])
        self.assertTrue(ps and all(c not in ("shop_owner", "food_outlet_owner", "janitor") for c, _ in ps))
        self.assertEqual({u for _, u in ps}, {"a"})                           # ground-floor units by their floor area; these shops are too small for staff
        for l in LOTS:                                                       # and the town's other malls are so
            if "plan" in l and not careers.mall_outlets(l):
                self.assertEqual(careers.minimum(l, R, TMPL[l["id"]]), [], l["name"])
                self.assertFalse({c for c, _ in careers.posts(l, R, TMPL[l["id"]])} & {"shop_owner", "food_outlet_owner", "janitor"}, l["name"])

    def test_CITY_105_the_rule_is_checked(self):
        self.assertEqual(careers.problems(R, W["roles"]["roles"]), [])
        bad = copy.deepcopy(R)
        bad["mall_staff"]["keeper"]["food"] = "astronaut"
        bad["mall_staff"]["whole_mall"][0]["count"] = -1
        got = careers.problems(bad, W["roles"]["roles"])
        self.assertTrue(any("keeper of a food outlet" in p for p in got), got)
        self.assertTrue(any("whole count" in p for p in got), got)
        for k in ("shop_owner", "food_outlet_owner", "janitor", "security_guard"):
            self.assertIn(k, C)
        self.assertEqual(C["shop_owner"]["title"], "shop owner")              # the owner's own words for the career

    def test_CITY_105_guards_and_janitors_wear_the_clothes_of_the_job(self):
        import re
        with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Prototype", "play", "people.js"), encoding="utf-8") as f:
            drawn = set(re.findall(r"^    (\w+): \{ g: ", f.read(), re.M))        # the clothes the slice knows how to draw
        self.assertEqual((C["security_guard"]["wear"], C["janitor"]["wear"]), ("guard", "janitor"))
        for k, c in C.items():
            if "wear" in c:
                self.assertIn(c["wear"], drawn, k)
        bad = copy.deepcopy(R)
        bad["careers"]["janitor"]["wear"] = ""
        self.assertTrue(any("wear must name" in p for p in careers.problems(bad, W["roles"]["roles"])))

    def test_CITY_105_eastgate_has_a_keeper_in_every_outlet_and_its_guards_and_janitors(self):
        mall = next(l for l in LOTS if careers.mall_outlets(l))
        outs = careers.mall_outlets(mall)
        self.assertEqual(len(outs), 189)
        ps = careers.posts(mall, R, TMPL[mall["id"]])
        first = careers.minimum(mall, R, TMPL[mall["id"]])
        whole = collections.Counter(c for c, u in ps if u is None)
        self.assertEqual(dict(whole), {w["career"]: w["count"] for w in R["mall_staff"]["whole_mall"]})
        self.assertTrue(2 <= whole["security_guard"] <= 12 and 2 <= whole["janitor"] <= 12)   # "few"
        staff = sorted((r["post"], r) for r in RES if r.get("work") == mall["id"])
        self.assertEqual([p for p, _ in staff[:len(first)]], first)          # every one of those posts is held by a resident
        for (k, r), (career, unit) in zip(staff, ps):
            self.assertEqual(r["career"], career, r["id"])
        keepers = [r for k, r in staff[:len(outs)]]
        self.assertEqual(len({r["id"] for r in keepers}), 189)               # one person to each outlet
        for r, u in zip(keepers, outs):
            self.assertEqual(r["career"], R["mall_staff"]["keeper"][u["kind"]], u["id"])
            self.assertEqual(r["role"], C[r["career"]]["role"], r["id"])
            self.assertEqual(mall["objects"]["work"][r["post"]], u["stand"][0], u["id"])   # and stands in it, behind its counter
        self.assertEqual(mall["staff"], len(staff))

    def test_CITY_105_the_keepers_are_at_their_counters_in_the_working_day(self):
        city = world.simulation(W)
        mall = next(l for l in LOTS if careers.mall_outlets(l))
        outs = careers.mall_outlets(mall)
        staff = {r["post"]: r for r in RES if r.get("work") == mall["id"]}
        now = city.at(DAY * 1440 + 10 * 60 + 30)                              # half past ten on a working day
        there = 0
        for k, u in enumerate(outs):
            st = now[staff[k]["id"]]
            if st["act"] == "work" and st["place"] == mall["id"] and st["state"] == "inside":
                there += 1
                self.assertLess(abs(st["pos"][0] - u["stand"][0][0]) + abs(st["pos"][1] - u["stand"][0][1]), 0.05, u["id"])
        self.assertGreaterEqual(there, 180)                                  # all but a few who are on their way


def people_by_id(people):
    return {p["id"]: p for p in people}


class WorkingFromHome(unittest.TestCase):
    def test_CITY_083_artists_work_from_home(self):
        artists = [r for r in RES if r.get("career") and C[r["career"]]["group"] == "creative"]
        working = [r for r in RES if r["role"] in R["reassign_roles"]]
        lo, hi = R["targets"]["home_based_share"]
        self.assertTrue(lo <= len(artists) / len(working) <= hi, len(artists) / len(working))
        kinds = {r["career"] for r in artists}
        for must in ("painter", "musician", "novelist"):
            self.assertIn(must, kinds)
        self.assertGreaterEqual(len(kinds), 8)
        for r in artists:
            self.assertIsNone(r["work"], r["id"])
            self.assertEqual(r["role"], "creative")

    def test_CITY_083_artists_spend_the_working_day_creating_at_home(self):
        city = world.simulation(W)
        artists = [r["id"] for r in RES if r["role"] == "creative"][:40]
        for rid in artists:
            blocks = city.blocks(rid, DAY)
            create = [(m, p) for m, a, p in blocks if a == "create"]
            self.assertTrue(create, rid)
            self.assertTrue(all(p == city.res[rid]["home"] for m, p in create), rid)
            st = city.at(DAY * 1440 + 10 * 60 + 30)[rid]                   # mid-morning
            self.assertEqual((st["act"], st["place"]), ("create", city.res[rid]["home"]), rid)


class OfficeWorkers(unittest.TestCase):
    def test_CITY_084_office_workers_are_the_largest_group(self):
        groups = collections.Counter(C[r["career"]]["group"] for r in RES if r.get("career"))
        everyone = sum(groups.values())
        office = groups["office"] + sum(1 for r in RES if r.get("career") and r["role"] == "commuter" and C[r["career"]]["role"] == "clerk")
        self.assertGreaterEqual(office / everyone, R["targets"]["office_share_of_workers_at_least"])
        self.assertEqual(groups.most_common(1)[0][0], "office")
        in_town = collections.Counter(C[r["career"]]["group"] for r in RES if r.get("career") and r["role"] in R["reassign_roles"])
        n = sum(in_town.values())
        self.assertGreaterEqual(in_town["office"] / n, R["shares"]["office"] - 0.02)
        for k in ("software_developer", "accountant", "bank_teller", "bank_operations_clerk", "operations_analyst", "payroll_clerk", "hr_officer"):
            self.assertIn(k, {r["career"] for r in RES if r.get("career")}, k)

    def test_CITY_084_no_group_takes_more_than_its_share(self):
        in_town = collections.Counter(C[r["career"]]["group"] for r in RES if r.get("career") and r["role"] in R["reassign_roles"])
        n = sum(in_town.values())
        mins = collections.Counter()
        for l in LOTS:
            ps = careers.posts(l, R, TMPL[l["id"]])
            for k in careers.minimum(l, R, TMPL[l["id"]]):
                mins[C[ps[k][0]]["group"]] += 1
        for g, share in R["shares"].items():
            self.assertLessEqual(in_town[g], max(round(share * n), mins[g]) + 1, g)


class FewerCouriers(unittest.TestCase):
    def test_CITY_087_couriers_and_postal_workers_are_few(self):
        couriers = [r for r in RES if r["role"] == "courier"]
        self.assertLessEqual(len(couriers), round(R["couriers_share"] * len(RES)) + 1)
        self.assertGreater(len(couriers), 0)                                 # parcels still come
        self.assertEqual(W["careers"]["couriers_share"], world._read(world.WORLD, "rules", "balance.json")["targets"]["residents"]["courier"])
        for r in couriers:
            self.assertIn(r["career"], R["courier_careers"])
            self.assertTrue(r.get("rounds"), r["id"])


class TwoJobs(unittest.TestCase):
    def test_CITY_088_waste_collectors_collect_at_dawn_then_do_a_day_job(self):
        city = world.simulation(W)
        crew = [r for r in RES if r.get("career") == "waste_collector"]
        self.assertGreater(len(crew), 0)
        for r in crew:
            self.assertEqual(r["role"], "binman")
            self.assertIn(r["day_career"], R["second_jobs"]["waste_collector"])
            self.assertEqual(careers.workplace_type(BYID[r["work"]], R, TMPL[r["work"]]), "waste_depot")
            bl = city.blocks(r["id"], DAY)
            collect = [m for m, a, p in bl if a == "collect"]
            deliver = [m for m, a, p in bl if a == "deliver"]
            self.assertTrue(collect and deliver, r["id"])
            self.assertLess(min(collect), 6 * 60, r["id"])                       # out before six
            self.assertLess(max(collect), min(deliver), r["id"])                 # bins first, then the day job
            self.assertGreater(max(deliver), 13 * 60, r["id"])                   # the day job fills the day
            first = next(a for m, a, p in bl if a not in ("sleep", "home"))
            self.assertEqual(first, "work", r["id"])                             # they start at the depot


class OwnPlaceAtWork(unittest.TestCase):
    def test_CITY_085_each_worker_stands_at_their_own_post(self):
        city = world.simulation(W)
        plan = city.plan(DAY)
        seen = 0
        for rid, pieces in plan.items():
            r = city.res[rid]
            for kind, t0, t1, payload, s in pieces:
                if kind == "stay" and s["act"] == "work" and s["place"] == r.get("work") and r.get("post") is not None:
                    self.assertEqual(s.get("slot"), r["post"], rid)
                    seen += 1
        self.assertGreater(seen, 1000)
        for l in LOTS:
            staff = [r for r in RES if r.get("work") == l["id"] and r.get("post") is not None]
            self.assertLessEqual(len(staff), len(l["objects"].get("work", [])), l["name"])
            self.assertEqual(sorted(r["post"] for r in staff), list(range(len(staff))), l["name"])


class TheSameAnswer(unittest.TestCase):
    def test_CITY_086_careers_are_worked_out_from_the_rules_alone(self):
        a = careers.assign(RES, LOTS, R, TMPL)
        shuffled = list(RES)
        random.Random(7).shuffle(shuffled)
        lots = list(LOTS)
        random.Random(8).shuffle(lots)
        self.assertEqual(careers.assign(shuffled, lots, R, TMPL), a)
        for r in RES:                                                        # the city holds what the rules say
            if r["id"] in a:
                self.assertEqual((r["career"], r["work"], r.get("post")), (a[r["id"]]["career"], a[r["id"]]["work"], a[r["id"]]["post"]), r["id"])

    def test_CITY_086_minimums_first_nearest_first(self):
        people, lots, rules = mini_city()
        a = careers.assign(people, lots, rules)
        at = collections.defaultdict(collections.Counter)
        for rid, v in a.items():
            at[v["work"]][v["career"]] += 1
        self.assertEqual(at["c"]["table_attendant"], 1)                    # the café opens: one of each at least
        self.assertGreaterEqual(at["c"]["cafe_cashier"], 1)
        self.assertGreaterEqual(at["c"]["barista"], 1)
        self.assertGreaterEqual(at["g"]["checkout_operator"], 2)
        near_cafe = [p["id"] for p in people if p["home"] == "h1"]
        self.assertTrue(all(a[rid]["work"] == "c" for rid in near_cafe if a[rid]["work"]), a)   # those living by the café work there

    def test_CITY_086_too_few_posts_is_an_error(self):
        people, lots, rules = mini_city()
        people += [dict(id="x%02d" % k, role="clerk", home="h1", work=None) for k in range(20)]
        with self.assertRaises(ValueError):
            careers.assign(people, lots, rules)


if __name__ == "__main__":
    unittest.main()
