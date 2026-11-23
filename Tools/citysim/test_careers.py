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
