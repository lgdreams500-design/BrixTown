"""How furnished a resident household's home is (FURN-062, FURN-063; F-HOME-FURNISH level 3; ADR-0035).

Four levels, 1 Bare, 2 Basic, 3 Homely, 4 Full, each adding pieces to the one before (World/rules/home_plans.json says
which pieces come in at which level). Which level a household's home shows is worked out from data and the clock day,
never ticked or stored (docs/specs/domains/LIVING_CITY.md §1):

- it starts at the level of the highest band in World/rules/furnishing_levels.json start_bands its daily wages reach;
- each day since it moved in it sets aside save_bp of its daily wages; savings at the start of day d are
  per_day x (d - moved_in);
- it rises a level each time its savings reach the cost of the steps it has bought so far plus the next one
  (step_cost: Bare to Basic, Basic to Homely, Homely to Full), up to the last level.

On the day a level rises (level(day) > level(day - 1)) the step's cost moves from npc:town to sys:spend in one ledger
transaction at that day's end (estate.Estate._furnishing). Every household counts from day 0, the day the starting
town moved in: a household that moves house (PROP-022) keeps its members and so its wages, and takes its furniture with
it, so its level carries on unchanged and no move-in day needs keeping. Integer arithmetic only; the browser slice runs
the same rule in Prototype/play/estate.js (Estate.furnish) and furnishing.js.

Standard library only.
"""
import json
import os

RULES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "World", "rules", "furnishing_levels.json")


def load_rules(path=RULES):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def top(F):
    return len(F["levels"])


def start_level(F, wage):
    """The level a household starts at, from its daily wages."""
    lv = 1
    for b in F["start_bands"]:
        if wage >= b["from_wage"]:
            lv = max(lv, b["level"])
    return lv


def per_day(F, wage):
    return wage * F["save_bp"] // 10000


def level_after(F, wage, days):
    """The level after `days` days of savings."""
    lv = start_level(F, wage)
    saved = per_day(F, wage) * max(0, days)
    spent = 0
    while lv < top(F) and saved >= spent + F["step_cost"][lv - 1]:
        spent += F["step_cost"][lv - 1]
        lv += 1
    return lv


def level(F, wage, day, moved_in=0):
    """FURN-063: the level a household on `wage` a day shows on clock day `day`."""
    return level_after(F, wage, day - moved_in)


def cost_between(F, lo, hi):
    """What the steps from level lo up to level hi cost together."""
    return sum(F["step_cost"][k - 1] for k in range(lo, hi))


def rise(F, wage, day, moved_in=0):
    """(level the day before, level today, what today's rise costs); the cost is 0 on a day nothing rises."""
    a, b = level(F, wage, day - 1, moved_in), level(F, wage, day, moved_in)
    return a, b, cost_between(F, a, b)


def name(F, lv):
    return F["levels"][lv - 1]


def problems(F):
    """Every rule World/rules/furnishing_levels.json must keep."""
    out = []

    def whole(v, lo=0):
        return type(v) is int and v >= lo
    lv = F.get("levels")
    if not (isinstance(lv, list) and len(lv) == 4 and all(isinstance(x, str) and x for x in lv)):
        out.append("furnishing: levels must be the four names Bare, Basic, Homely, Full (FURN-062)")
        return out
    n = len(lv)
    bands = F.get("start_bands") or []
    if not bands or bands[0].get("from_wage") != 0:
        out.append("furnishing: the first start band must be from_wage 0")
    for k, b in enumerate(bands):
        if not (whole(b.get("from_wage")) and whole(b.get("level"), 1) and b["level"] <= n):
            out.append("furnishing: start band %d needs a whole from_wage and a level 1..%d" % (k, n))
        elif k and not (b["from_wage"] > bands[k - 1]["from_wage"] and b["level"] > bands[k - 1]["level"]):
            out.append("furnishing: start bands must rise in wages and in level")
    if not whole(F.get("save_bp"), 1) or F["save_bp"] > 10000:
        out.append("furnishing: save_bp must be 1..10000")
    sc = F.get("step_cost")
    if not (isinstance(sc, list) and len(sc) == n - 1 and all(whole(c, 1) for c in sc)):
        out.append("furnishing: step_cost must give %d whole costs in chips" % (n - 1))
    for k in ("lodger_level", "empty_level"):
        if not (whole(F.get(k), 1) and F[k] <= n):
            out.append("furnishing: %s must be a level 1..%d" % (k, n))
    for k in ("start_bands", "save_bp", "step_cost", "lodger_level", "empty_level"):
        if "[ASSUMPTION]" not in (F.get("notes") or {}).get(k, ""):
            out.append("furnishing: %s needs a note marked [ASSUMPTION]" % k)
    return out
