"""How furnished residents' homes are (FURN-062, FURN-063; F-HOME-FURNISH level 3; ADR-0035).
        python -m unittest test_furnishing -v

Each test names the requirement it proves (docs/specs/domains/FURNITURE.md §2.4)."""
import collections, copy, os, sys, unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import world                                               # noqa: E402
import economy as ec                                       # noqa: E402
import estate as es                                        # noqa: E402
import furnishing as fu                                    # noqa: E402

W = world.load()
LOTS = world.lots(W)
F = fu.load_rules()


def town():
    e = ec.Economy(W["economy"], LOTS, {}, {})
    s = es.Estate(W["estate"], e, LOTS, W["residents"])
    e.advance(0, 440)
    e.new_character()
    s.start()
    return e, s


def run(e, days):
    for _ in range(days):
        e.advance(e.day + 1, 0)


def households(s):
    return [i for i in range(len(s.units)) if s.tenant[i] == "hh" and s.members[i]]


def furniture_tx(e):
    return [t for t in e.L.tx if t[3].startswith("Furniture for ")]


class Levels(unittest.TestCase):
    def test_FURN_062_levels_add_up(self):
        self.assertEqual(fu.problems(F), [])
        self.assertEqual(F["levels"], ["Bare", "Basic", "Homely", "Full"])
        wages = sorted({b["from_wage"] for b in F["start_bands"]} | {0, 1, 4500, 8999, 9000, 15999, 16000, 54500, 139000, 10 ** 7})
        for w in wages:
            seen = [fu.level(F, w, d) for d in range(0, 2000, 7)]
            self.assertTrue(all(1 <= v <= 4 for v in seen), w)
            self.assertEqual(seen, sorted(seen), "never goes down: %d" % w)
            self.assertEqual(seen[0], fu.start_level(F, w))
            if fu.per_day(F, w):
                self.assertEqual(seen[-1], 4, "a household that saves gets to Full in the end: %d" % w)
            else:
                self.assertEqual(set(seen), {fu.start_level(F, w)}, "no savings, no rise")
        self.assertEqual(fu.level(F, 10 ** 7, 10 ** 6), 4, "capped at Full")
        self.assertEqual(fu.cost_between(F, 1, 4), sum(F["step_cost"]))
        self.assertEqual([fu.name(F, k) for k in range(1, 5)], F["levels"])


class FollowsTheHousehold(unittest.TestCase):
    def test_FURN_063_level_follows_the_household(self):
        # start bands, from the wages
        for k, b in enumerate(F["start_bands"]):
            self.assertEqual(fu.start_level(F, b["from_wage"]), b["level"])
            if k:
                self.assertEqual(fu.start_level(F, b["from_wage"] - 1), F["start_bands"][k - 1]["level"])
        # rises exactly when savings reach the cost, from the numbers in the data
        w = 10000
        p, c = fu.per_day(F, w), F["step_cost"]
        self.assertEqual(p, w * F["save_bp"] // 10000)
        lv0 = fu.start_level(F, w)
        first = -(-c[lv0 - 1] // p)                          # the first day whose savings reach the next step's cost
        self.assertEqual(fu.level(F, w, first - 1), lv0)
        self.assertEqual(fu.level(F, w, first), lv0 + 1)
        self.assertEqual(fu.rise(F, w, first), (lv0, lv0 + 1, c[lv0 - 1]))
        self.assertEqual(fu.rise(F, w, first + 1)[2], 0)
        second = -(-(c[lv0 - 1] + c[lv0]) // p)
        self.assertEqual(fu.level(F, w, second - 1), lv0 + 1)
        self.assertEqual(fu.level(F, w, second), lv0 + 2)
        self.assertEqual(fu.level(F, w, 40, moved_in=40), lv0, "counted from the day it moved in")
        # Python and data agree: change a number and the level follows it
        G = copy.deepcopy(F)
        G["step_cost"][lv0 - 1] *= 2
        self.assertEqual(fu.level(G, w, first), lv0)
        G = copy.deepcopy(F)
        G["start_bands"][-1]["from_wage"] = w
        self.assertEqual(fu.start_level(G, w), G["start_bands"][-1]["level"])

        # the starting town spreads over the levels, by wages
        e, s = town()
        hh = households(s)
        spread = collections.Counter(s.level_of(i) for i in hh)
        print("\nFURN-063 starting town: %d households, by level %s" % (len(hh), dict(sorted(spread.items()))))
        for k in (1, 2, 3):
            self.assertGreater(spread[k], len(hh) // 10, "level %d has a share of the town" % k)
        start = {i: s.level_of(i, 0) for i in hh}

        # the ledger: one transaction of the step's cost on each day a level rises, residents' account to sys:spend
        days = 150
        for _ in range(days):
            d = e.day
            n0 = len(e.L.tx)
            run(e, 1)
            got = [t for t in e.L.tx[n0:] if t[3].startswith("Furniture for ")]
            want = sorted(fu.rise(F, s.wage[i], d)[2] for i in households(s) if fu.rise(F, s.wage[i], d)[2])
            self.assertEqual(sorted(t[4][0][1] * -1 for t in got), want, "day %d" % d)
            for t in got:
                self.assertEqual(t[1], d)
                self.assertEqual([a for a, _ in t[4]], [es.TOWN, "sys:spend"])
                self.assertEqual(sum(v for _, v in t[4]), 0)
                self.assertTrue(all(type(v) is int for _, v in t[4]))
        tx = furniture_tx(e)
        self.assertTrue(tx, "some homes are better furnished after %d days" % days)
        # every household's spend is what its rises cost (a moved household took its furniture with it)
        total = sum(-t[4][0][1] for t in tx)
        self.assertEqual(total, sum(fu.cost_between(F, fu.level(F, s.wage[i], 0), fu.level(F, s.wage[i], e.day - 1)) for i in households(s)))
        later = collections.Counter(s.level_of(i) for i in households(s))
        print("FURN-063 after %d days: by level %s, %d furniture transactions" % (days, dict(sorted(later.items())), len(tx)))
        self.assertGreater(later[4], spread[4])
        self.assertEqual(e.L.supply() + e.L.system_total(), 0)
        self.assertGreaterEqual(e.L.balance(es.TOWN), 0)

        # never ticked or stored: nothing in the saved game, and the level of any day is worked out on the spot
        self.assertFalse([k for k in s.save() if "furn" in k or "level" in k])
        for i in households(s)[:50]:
            self.assertEqual(s.level_of(i), fu.level(F, s.wage[i], e.day))
            self.assertEqual(s.level_of(i, 0), fu.start_level(F, s.wage[i]))
        e2, s2 = town()
        self.assertEqual({i: s2.level_of(i, 0) for i in hh}, start)
        for i in hh[:50]:
            self.assertEqual(s2.level_of(i, days), fu.level(F, s2.wage[i], days), "a fresh town knows day %d without running it" % days)
        # homes without a household show the data's levels
        for i in range(len(s.units)):
            if s.tenant[i] == "lodger":
                self.assertEqual(s.level_of(i), F["lodger_level"])
            elif s.tenant[i] is None:
                self.assertEqual(s.level_of(i), F["empty_level"])


if __name__ == "__main__":
    unittest.main()
