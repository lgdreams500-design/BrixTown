"""Achievements (QST-009, F-UI-REWARD level 2): named moments in the player's life, earned once and kept.

Nothing is counted up as the game runs. The facts about the player are worked out from what the game already keeps (the
ledger, the quests, the loans, the property market), and an achievement is earned the first time its fact reaches its
number. Only which ones have been earned, and on which day, is kept. Rules: World/rules/achievements.json.
Prototype/play/achievements.js is a port, checked against this each time the page loads."""

FACTS = ("quests_done", "jobs_paid", "savings", "loans_repaid", "stall_sold", "homes_rented", "homes_owned", "money")


def facts(rules, econ, quests, market=None):
    """The facts achievements are earned on, from the game as it stands."""
    memo = rules["job_memo"]
    return dict(
        quests_done=len(quests.done),
        jobs_paid=sum(1 for t in econ.L.tx if t[3].startswith(memo) and any(a.startswith("p:") and v > 0 for a, v in t[4])),
        savings=econ.L.balance("p:savings"),
        loans_repaid=sum(1 for l in econ.loans if l["status"] == "repaid"),
        stall_sold=econ.stall_sold_total,
        homes_rented=len(market.tenancy) if market else 0,
        homes_owned=len(market.owned()) if market else 0,
        money=econ.total_money())


def newly(rules, f, have):
    """The ids earned now and not before, in the order of the list. have: {id: day earned}."""
    return [a["id"] for a in rules["list"] if a["id"] not in have and f[a["when"]["fact"]] >= a["when"]["at_least"]]


def award(rules, f, have, day):
    """Earn what is due: adds to `have` and returns the new ids. What was earned stays earned, whatever the facts do later."""
    new = newly(rules, f, have)
    for i in new:
        have[i] = day
    return new


def problems(rules):
    """Every rule World/rules/achievements.json must keep."""
    out, seen = [], set()
    if not rules.get("job_memo"):
        out.append("achievements: job_memo is missing")
    if not (isinstance(rules.get("banner_s"), (int, float)) and rules["banner_s"] > 0):
        out.append("achievements: banner_s should be above 0")
    if not rules.get("list"):
        out.append("achievements: the list is empty")
    for a in rules.get("list", []):
        i = a.get("id")
        if not i or i in seen:
            out.append("achievements: id missing or used twice: %r" % i)
        seen.add(i)
        for k in ("name", "line"):
            if not (isinstance(a.get(k), str) and a[k].strip()):
                out.append("achievements: %s has no %s" % (i, k))
        w = a.get("when", {})
        if w.get("fact") not in FACTS:
            out.append("achievements: %s is earned on an unknown fact: %r" % (i, w.get("fact")))
        if not (isinstance(w.get("at_least"), int) and not isinstance(w.get("at_least"), bool) and w["at_least"] >= 1):
            out.append("achievements: %s needs a whole number of at least 1" % i)
    return out


def check_script(rules):
    """Fixed facts and what is earned for them, for the page to check its port against."""
    zero = {k: 0 for k in FACTS}
    steps = [dict(zero), dict(zero, quests_done=1, money=5000), dict(zero, quests_done=4, jobs_paid=3, savings=10000),
             dict(zero, quests_done=4, jobs_paid=10, loans_repaid=1, stall_sold=2, homes_rented=1),
             dict(zero, homes_owned=2, money=250000), dict(zero)]
    have, want = {}, []
    for day, f in enumerate(steps):
        want.append(award(rules, f, have, day))
    return dict(steps=steps, want=want, have=have)
