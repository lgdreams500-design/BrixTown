"""Residents visit the mall, and its staff eat there (LIVING_CITY.md CITY-106), over the whole starting city in World/.
The owner, 2026-11-24: "update population to come to mall".        python -m unittest test_mall -v"""
import collections, copy, os, sys, unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import world                                               # noqa: E402

W = world.load()
M = W["choices"]["mall"]
LOTS = {l["id"]: l for l in world.lots(W)}
MALL = next(l for l in LOTS.values() if any(u.get("outlet") for L in (l.get("plan") or {}).get("levels", []) for u in L["units"]))
STAFF = {r["id"] for r in W["residents"] if r.get("work") == MALL["id"]}
CITY = world.simulation(W)
DAY = 16                                                    # a working day
SUNDAY = next(d for d in range(DAY, DAY + 8) if CITY.clock.info(d)["sunday"])
hm = lambda t: int(t[:2]) * 60 + int(t[3:])
OPEN, LAST = hm(M["open"]), hm(M["last_in"])


def stays(day, city=CITY):
    """[(resident, arrives, leaves, stay)] of everyone who shops or eats at the mall that day."""
    return [(rid, t0, t1, s) for rid, pieces in city.plan(day).items() for kind, t0, t1, payload, s in pieces
            if kind == "stay" and s["place"] == MALL["id"] and s["act"] in ("shop", "eat")]


def visits(day):
    return [v for v in stays(day) if v[0] not in STAFF]


class TheMallsVisitors(unittest.TestCase):
    def test_CITY_106_residents_shop_and_eat_at_the_mall_in_its_hours(self):
        for day in (DAY, SUNDAY):
            v = visits(day)
            self.assertGreater(len(v), 30, day)                              # the town comes: dozens of visits a day
            self.assertLess(len(v), 1200, day)                               # and not the whole town
            acts = collections.Counter(s["act"] for _, _, _, s in v)
            if day == SUNDAY:                                                # on a Sunday its shop owners are at home: people come to eat
                self.assertEqual(set(acts), {"eat"})
            else:
                self.assertGreater(acts["shop"], 15, day)
                self.assertGreater(acts["eat"], 8, day)
            for rid, t0, t1, s in v:
                self.assertTrue(OPEN <= s["start"] <= LAST, (rid, s["start"]))   # they set out for it in its hours
                self.assertGreaterEqual(t0, OPEN, rid)
            roles = {CITY.res[rid]["role"] for rid, _, _, _ in v}
            self.assertGreaterEqual(len(roles), 3, roles)                    # all sorts of people, not one kind
            long_enough = sum(1 for _, t0, t1, _ in v if t1 - t0 >= 10)      # and they stay: few walk there only to turn round
            self.assertGreaterEqual(long_enough, 0.8 * len(v), (day, long_enough, len(v)))
        for hour in (0, 3, 6, 23):                                           # and nobody is shopping there at night
            now = CITY.at(DAY * 1440 + hour * 60)
            self.assertFalse([rid for rid, st in now.items() if st["place"] == MALL["id"] and st["act"] in ("shop", "eat")], hour)

    def test_CITY_106_who_goes_is_worked_out_from_the_clock_and_the_rules(self):
        again = world.simulation(W)
        some = sorted({rid for rid, _, _, _ in visits(DAY)})[:40] + sorted(CITY.res)[:40]
        for rid in some:
            for day in (DAY, SUNDAY):
                self.assertEqual(again.blocks(rid, day), CITY.blocks(rid, day), rid)   # the same answer on another machine
        days = [{rid for rid, _, _, _ in visits(d)} for d in (DAY, DAY + 1)]
        self.assertNotEqual(days[0], days[1])                                # and it is not the same people every day
        for rid in sorted(days[0])[:40]:                                     # the mall they go to is the one nearest their home
            self.assertEqual(CITY._mall(rid), MALL["id"])

    def test_CITY_106_outside_its_hours_they_do_something_else(self):
        shut = world.simulation(W)
        shut.d["mall"] = dict(M, open="09:30", last_in="09:31")              # a mall that takes nobody in after half past nine
        went = sorted({rid for rid, _, _, s in visits(DAY) if s["start"] > hm("09:31")})
        self.assertGreater(len(went), 25)
        instead = collections.Counter()
        for rid in went:
            b = shut.blocks(rid, DAY)
            self.assertFalse([x for x in b if x[2] == MALL["id"] and rid not in STAFF], rid)
            instead.update(a for m, a, p in b if (m, a, p) not in CITY.blocks(rid, DAY))
        self.assertTrue(set(instead) <= {e[0] for e in M["else"].values()}, instead)   # what the rules say to do instead
        none = world.simulation(W)
        none.d["mall"] = None                                                # and a town whose rules name no mall hours
        for rid in went[:20]:
            self.assertFalse([x for x in none.blocks(rid, DAY) if x[2] == MALL["id"]], rid)
        short = world.simulation(W)
        short.d["mall"] = dict(M, stay_min=600)                              # nobody sets out who could not stay as long as the rules ask
        for rid in went:
            self.assertFalse([x for x in short.blocks(rid, DAY) if x[2] == MALL["id"]], rid)

    def test_CITY_106_the_malls_staff_eat_at_the_mall(self):
        self.assertGreaterEqual(len(STAFF), 190)
        keepers = [r for r in W["residents"] if r["id"] in STAFF and r["role"] == "shopkeeper"]
        self.assertGreater(len(keepers), 150)
        for r in keepers:
            b = CITY.blocks(r["id"], DAY)
            k = next(i for i, x in enumerate(b) if x[1] == "eat")
            self.assertEqual(b[k][2], MALL["id"], r["id"])                   # lunch is at the mall,
            self.assertEqual((b[k - 1][1], b[k + 1][1]), ("work", "work"), r["id"])   # between two spells at work there
        now = CITY.at(DAY * 1440 + 13 * 60 + 5)
        there = sum(1 for rid in STAFF if now[rid]["place"] == MALL["id"] and now[rid]["state"] == "inside")
        self.assertGreaterEqual(there, 170)                                  # so at lunch the mall keeps its staff (30 of 201 before this)
        other = next(r for r in W["residents"] if r["role"] == "shopkeeper" and r.get("work") and r["id"] not in STAFF)
        self.assertFalse([x for x in CITY.blocks(other["id"], DAY) if x[1] == "eat" and x[2] == other["work"]])   # a street shop's staff still go out

    def test_CITY_106_there_is_a_place_for_those_who_come(self):
        use = W["choices"]["use"]
        self.assertGreaterEqual(len(MALL["objects"][use["shop"]]), 300)      # before the counters of its outlets
        self.assertGreaterEqual(len(MALL["objects"][use["eat"]]), 200)       # and at its tables: its own staff alone are 200 at lunch
        for day in (DAY, SUNDAY):
            all_ = stays(day)
            placed = sum(1 for _, _, _, s in all_ if s.get("slot") is not None)
            self.assertGreaterEqual(placed, 0.97 * len(all_), (day, placed, len(all_)))

    def test_CITY_106_the_rule_is_checked(self):
        self.assertEqual(world.mall_visit_problems(W), [])
        lists = W["choices"]["choices"]
        share = sum(1 for o in lists["leisure"]["default"] if o[1] == "mall") / len(lists["leisure"]["default"])
        self.assertTrue(0.05 <= share <= 0.2, share)                         # a modest share of the town's free time
        self.assertEqual({o[0] for c in lists.values() for opts in c.values() for o in opts if o[1] == "mall"}, {"shop", "eat"})
        for change, word in ((lambda m: m.update(open="18:00"), "before last_in"), (lambda m: m.update({"else": {}}), "instead for leisure"),
                             (lambda m: m.update(open="half nine"), "HH:MM")):
            bad = copy.deepcopy(W)
            change(bad["choices"]["mall"])
            self.assertTrue(any(word in p for p in world.mall_visit_problems(bad)), word)
        bad = copy.deepcopy(W)
        bad["choices"]["choices"]["play"]["default"] = [["shop", "mall"]] * 3
        got = world.mall_visit_problems(bad)
        self.assertTrue(any("more than a quarter" in p for p in got) and any("instead for play" in p for p in got), got)


if __name__ == "__main__":
    unittest.main()
