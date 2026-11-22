"""How the city stands against its balance targets (docs/specs/domains/CITY_BALANCE.md BAL-003): homes and the people
they hold, residents by role, jobs by sector from floor area, places to go, each against World/rules/balance.json.
Nothing is stored: everything here is worked out from the block files, residents.json and the rules.

Run from Tools/worldgen:   python balance.py          (prints the report)
Other scripts:             import balance; balance.report(w) -> dict, balance.lines(rep) -> printable lines
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")
COMMUNITIES = ("Northwood", "Eastbrook", "Southfield")


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def load():
    city = jload(os.path.join(CITY, "city.json"))
    return dict(city=city, blocks=[jload(os.path.join(CITY, e["file"])) for e in city["blocks"]],
                residents=jload(os.path.join(CITY, "residents.json"))["residents"], rules=jload(os.path.join(RULES, "balance.json")))


def area(r):
    return (r[2] - r[0]) * (r[3] - r[1])


def where(b):
    d = b["district"]
    return "Old Town" if d in ("Centre", "Old Town", "Outskirts") else d


def report(w):
    R = w["rules"]
    H, J, T = R["homes"], R["jobs"], R["targets"]
    sizes = H["sizes"]
    avg = sum(s * H["mix"][str(s)] for s in sizes)
    people = {s: H["people"][str(s)] for s in sizes}
    homes = {}                                                        # (type, size) -> count
    by_area = {}                                                      # area -> {homes, people}
    jobs, schools, places = {}, 0.0, {"street_shops": 0, "food": 0, "clinics": 0, "park_m2": 0.0}
    centres = {c: set() for c in COMMUNITIES}

    def home(t, s, b, n=1):
        homes[(t, s)] = homes.get((t, s), 0) + n
        a = by_area.setdefault(where(b), dict(homes=0, people=0.0))
        a["homes"] += n
        a["people"] += n * people[s]

    def size_of(net):
        return max([s for s in sizes if s * (1 - H["size_tolerance"]) <= net] or [sizes[0]])

    for b in w["blocks"]:
        t = b["template"]
        for l in b["lots"]:
            k = l["kind"]
            if k == "home" and l.get("flats"):
                for q in l["flats"]:
                    home("tower flat" if t == "towerR" else "court flat", q["size"], b)
            elif k == "home":
                net = area(l["rect"]) * l.get("floors", 1) * (1 - H["common_share"])
                home("independent house" if t == "community" else "row house" if t == "rows" else "other home", size_of(net), b)
            elif k in J["ground_floor_only"] and l.get("floors", 1) > 1 and "plan" not in l and t != "towerO":
                net = area(l["rect"]) * (l["floors"] - 1) * (1 - H["common_share"])          # the flats over a shop or cafe
                n = int(round(net / avg))
                for s in sizes:
                    home("flat over a shop", s, b, int(round(n * H["mix"][str(s)])))
            # jobs from floor area
            if "plan" in l:
                for L in l["plan"]["levels"]:
                    for u in L["units"]:
                        sec = J["mall_unit_kinds"].get(u["kind"], "retail")
                        jobs[("malls", sec)] = jobs.get(("malls", sec), 0) + area(u["rect"]) * J["usable_share"] / J["m2_per_job"][sec]
                continue
            sec = J["sector_of_kind"].get(k)
            if sec:
                if k == "work" and any(l["name"].endswith(x) for x in J["making_names_end"]):
                    sec = "making"
                floors = 1 if k in J["ground_floor_only"] and t != "towerO" else l.get("floors", 1) or 1
                a = area(l["rect"]) * floors * J["usable_share"]
                jobs[("town", sec)] = jobs.get(("town", sec), 0) + a / J["m2_per_job"][sec]
                if k == "school":
                    schools += a / R["schools"]["m2_per_pupil"]
            if k == "shop" and "plan" not in l:
                places["street_shops"] += 1
            if k == "food":
                places["food"] += 1
            if k == "health":
                places["clinics"] += 1
            if k == "garden":
                places["park_m2"] += area(l["rect"])
            if b["district"] in centres and k in T["local_centre_in_each_community"]:
                centres[b["district"]].add(k)
    pop = sum(a["people"] for a in by_area.values())
    roles = {}
    for r in w["residents"]:
        roles[r["role"]] = roles.get(r["role"], 0) + 1
    group = lambda r: ("child" if r == "child" else "student" if r == "student" else "elder" if r == "elder" else
                       "commuter_out" if r == "commuter" else "courier" if r == "courier" else "working_in_town")
    now = {}
    for r, n in roles.items():
        now[group(r)] = now.get(group(r), 0) + n
    town_jobs = sum(v for (where_, s), v in jobs.items() if where_ == "town")
    mall_jobs = sum(v for (where_, s), v in jobs.items() if where_ == "malls")
    working = T["residents"]["working_in_town"] * pop
    return dict(pop=pop, homes=homes, by_area=by_area, residents=len(w["residents"]), roles_now=now, jobs=jobs, town_jobs=town_jobs,
                mall_jobs=mall_jobs, working=working, in_commuters=max(0.0, town_jobs + mall_jobs - working), schools=schools,
                places=places, centres=centres, R=R)


def lines(rep):
    R, T = rep["R"], rep["R"]["targets"]
    band, pop = T["band"], rep["pop"]
    out = []

    def judge(have, want, name=None):
        if want <= 0:
            return "ok"
        if have < (1 - band) * want:
            return "LOW"
        return "ok" if name in T.get("at_least", []) or have <= (1 + band) * want else "HIGH"
    out.append("POPULATION when every home is full: %.0f   (living here now: %d)" % (pop, rep["residents"]))
    out.append("")
    out.append("HOMES by type and size")
    types = sorted({t for t, s in rep["homes"]})
    total = sum(rep["homes"].values())
    out.append("  %-18s %6s %6s %6s %6s %6s %6s" % ("", "80", "160", "320", "640", "all", "share"))
    for t in types:
        row = [rep["homes"].get((t, s), 0) for s in R["homes"]["sizes"]]
        out.append("  %-18s %6d %6d %6d %6d %6d %5.1f%%" % (t, *row, sum(row), 100 * sum(row) / total))
    out.append("  %-18s %6s %6s %6s %6s %6d" % ("all", "", "", "", "", total))
    out.append("")
    out.append("PEOPLE by area (when full)")
    for a, v in sorted(rep["by_area"].items(), key=lambda kv: -kv[1]["people"]):
        out.append("  %-18s %5d homes %6.0f people" % (a, v["homes"], v["people"]))
    out.append("")
    out.append("RESIDENTS by role: now (share of the %d living here), target share, verdict" % rep["residents"])
    for g, share in T["residents"].items():
        n = rep["roles_now"].get(g, 0)
        have = n / max(1, rep["residents"])
        out.append("  %-18s %5d  %5.1f%%   target %4.1f%%  %s" % (g, n, 100 * have, 100 * share, judge(have, share)))
    out.append("")
    out.append("JOBS from floor area (job places)")
    secs = sorted({s for _, s in rep["jobs"]})
    for s in secs:
        out.append("  %-18s town %6.0f   malls %6.0f" % (s, rep["jobs"].get(("town", s), 0), rep["jobs"].get(("malls", s), 0)))
    out.append("  %-18s town %6.0f   malls %6.0f" % ("all", rep["town_jobs"], rep["mall_jobs"]))
    out.append("  residents working in town when full: %.0f; the other %.0f job places are taken by in-commuters" % (rep["working"], rep["in_commuters"]))
    out.append("  malls: %.0f%% of their shoppers come from outside the city" % (100 * R["visitors"]["mall_shoppers_from_outside"]))
    out.append("")
    out.append("PLACES TO GO: have, want, verdict")
    p = rep["places"]
    for k, v in T["per_1000"].items():
        want = v * pop / 1000
        out.append("  %-18s %6.0f %6.0f  %s" % (k, p[k], want, judge(p[k], want, k)))
    want = T["park_m2_per_resident"] * pop
    out.append("  %-18s %6.1f %6.1f  %s   (hectares)" % ("park", p["park_m2"] / 1e4, want / 1e4, judge(p["park_m2"], want, "park")))
    want = T["school_places_per_child"] * T["residents"]["child"] * pop
    out.append("  %-18s %6.0f %6.0f  %s" % ("school places", rep["schools"], want, judge(rep["schools"], want, "school places")))
    for k, v in T.get("jobs_per_1000", {}).items():
        want, have = v * pop / 1000, rep["jobs"].get(("town", k), 0)
        out.append("  %-18s %6.0f %6.0f  %s" % (k + " jobs", have, want, judge(have, want)))
    out.append("")
    out.append("LOCAL CENTRES (each community needs %s)" % ", ".join(T["local_centre_in_each_community"]))
    for c, have in rep["centres"].items():
        miss = [k for k in T["local_centre_in_each_community"] if k not in have]
        out.append("  %-18s %s" % (c, "ok" if not miss else "missing " + ", ".join(miss)))
    return out


if __name__ == "__main__":
    print("\n".join(lines(report(load()))))
