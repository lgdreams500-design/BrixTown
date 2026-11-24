"""Give every working resident of city_01 a career, and every workplace a realistic number of staff
(docs/specs/domains/LIVING_CITY.md CITY-081 to CITY-086; the owner, 2026-11-23).

The rules are World/rules/careers.json and Tools/citysim/careers.py (proved by Tools/citysim/test_careers.py). This
script only writes what they answer into the city:
  residents   each working resident's career, the routine role it follows, their workplace (none for those who work
              from home) and their post there (their own place to work, CITY-085); couriers, commuters and gardeners
              get a career and keep everything else
  places      a workplace with more staff than places to work gets more, spread over its floors clear of its stair, as
              add_households_01.py does; a mall's places to work are in its ground-floor units, each staffed unit its own
  staff       a lot that records its staff count ('staff') is set to the residents who work there
  rounds      anyone who now has a utility routine and no rounds gets twelve places among the forty nearest
Ids, homes, names and everything else stay. Running it again gives the same city.
    python add_careers_01.py
"""
import collections, json, math, os, random, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "citysim"))
import careers                                                     # noqa: E402

CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")
SEED = 20261123


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def jsave(p, o, indent=1):
    with open(p, encoding="utf-8") as f:
        end = "\n" if f.read().endswith("\n") else ""
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        json.dump(o, f, indent=indent, ensure_ascii=False)
        f.write(end)


def grid_spots(l, n_more, floors):
    """n_more new places to work in a lot, spread over its floors, clear of its stair and of every place already there
    on any floor (the slice tells floors apart by where a place is on the plan; WLD-050)."""
    x0, z0, x1, z1 = l["rect"]
    st = (l.get("stair") or {}).get("rect")
    per = -(-n_more // floors) + 4
    nx = max(2, int(math.ceil(math.sqrt(per * (x1 - x0) / max(1.0, z1 - z0)))))
    nz = max(2, int(math.ceil(per / nx)) + 1)
    grid = [(x0 + 1.6 + (x1 - x0 - 3.2) * (i + .5) / nx, z0 + 1.6 + (z1 - z0 - 3.2) * (j + .5) / nz) for j in range(nz) for i in range(nx)]
    if st:
        grid = [p for p in grid if not (st[0] - .8 <= p[0] <= st[2] + .8 and st[1] - .8 <= p[1] <= st[3] + .8)]
    taken = {(round(p[0], 1), round(p[1], 1)) for v in l["objects"].values() for p in v}
    out, k = [], 0
    while len(out) < n_more:
        f = k % floors
        g = grid[(k // floors) % len(grid)]
        k += 1
        for shift in range(100):                               # nudge it until the spot is free on every floor
            i = f + shift * floors
            q = (round(min(x1 - .6, max(x0 + .6, g[0] + 0.3 * (i % 10))), 2), round(min(z1 - .6, max(z0 + .6, g[1] + 0.3 * (i // 10 % 10))), 2))
            if (round(q[0], 1), round(q[1], 1)) not in taken:
                break
        taken.add((round(q[0], 1), round(q[1], 1)))
        out.append((q, f))
    return out


def main():
    rng = random.Random(SEED)
    city = jload(os.path.join(CITY, "city.json"))
    R = jload(os.path.join(RULES, "careers.json"))
    files = {e["id"]: os.path.join(CITY, e["file"]) for e in city["blocks"]}
    blocks = {bid: jload(p) for bid, p in files.items()}
    lots = [l for b in blocks.values() for l in b["lots"]]
    tmpl = {l["id"]: b["template"] for b in blocks.values() for l in b["lots"]}
    byid = {l["id"]: l for l in lots}
    res_doc = jload(os.path.join(CITY, "residents.json"))
    people = res_doc["residents"]

    got = careers.assign(people, lots, R, tmpl)
    staff = collections.defaultdict(list)                       # lot id -> [(post, unit, resident)]
    for r in people:
        a = got.get(r["id"])
        r.pop("post", None)
        r.pop("day_career", None)
        if a is None:
            r.pop("career", None)
            continue
        r["role"], r["career"], r["work"] = a["role"], a["career"], a["work"]
        if a.get("day_career"):
            r["day_career"] = a["day_career"]
        if a["post"] is not None:
            r["post"] = a["post"]
            staff[a["work"]].append((a["post"], a.get("unit"), r))

    for lid, ss in staff.items():
        l = byid[lid]
        ss.sort(key=lambda x: x[0])
        if "plan" in l:                                          # a mall: staff stand in their own units
            units = {u["id"]: u for u in l["plan"]["levels"][0]["units"]}
            per_unit = collections.Counter(u for _, u, _ in ss)
            seen, spots = collections.Counter(), []
            for _, u, _ in ss:
                x0, z0, x1, z1 = units[u]["rect"]
                n, i = per_unit[u], seen[u]
                seen[u] += 1
                at = units[u].get("stand")                       # a unit may say where its staff stand: behind its counter, at its stalls
                if at and i < len(at):
                    spots.append([round(at[i][0], 2), round(at[i][1], 2)])
                    continue
                step = min(1.2, (x1 - x0 - 1.4) / max(1, n))
                spots.append([round((x0 + x1) / 2 + (i - (n - 1) / 2) * step, 2), round(z0 + (z1 - z0) * 0.4, 2)])
            l["objects"]["work"] = spots
        else:
            have = l["objects"].setdefault("work", [])
            if len(ss) > len(have):
                ground = l["kind"] in ("shop", "food") and tmpl[lid] != "towerO"      # their upper floors are flats
                floors = 1 if ground else l.get("floors", 1) or 1
                lv = l.setdefault("levels", {}) if floors > 1 else None
                for p, f in grid_spots(l, len(ss) - len(have), floors):
                    have.append(list(p))
                    if lv is not None:
                        lv.setdefault("work", [0] * (len(have) - 1)).append(f)
        if "staff" in l:
            l["staff"] = len(ss)
    for l in lots:                                                # places nobody works at any more
        if "staff" in l and l["id"] not in staff and careers.posts(l, R, tmpl[l["id"]]):
            l["staff"] = 0

    # rounds for anyone with a utility or courier routine who has none
    d2 = lambda a, b: abs(a["door"][0] - b["door"][0]) + abs(a["door"][1] - b["door"][1])
    visitable = [l for l in lots if l["kind"] in ("shop", "food", "work", "health", "home", "utility") and l.get("enterable") and "plan" not in l]
    for r in people:
        if r["role"] in ("courier", "utility", "binman"):
            if not r.get("rounds"):
                h = byid[r["home"]]
                close = sorted(visitable, key=lambda l: (d2(h, l), l["id"]))[:40]
                r["rounds"] = [l["id"] for l in rng.sample(close, 12)]
        else:
            r.pop("rounds", None)

    jsave(os.path.join(CITY, "residents.json"), res_doc, 0)
    for bid, b in blocks.items():
        jsave(files[bid], b)
    C = R["careers"]
    groups = collections.Counter(C[r["career"]]["group"] for r in people if r.get("career"))
    work = sum(groups.values())
    print("%d residents with a career, %d careers in use of %d" % (work, len({r["career"] for r in people if r.get("career")}), len(C)))
    print(", ".join("%s %d (%.0f%%)" % (g, n, 100.0 * n / work) for g, n in groups.most_common()))
    print("home-based: %d" % sum(1 for r in people if r.get("career") and C[r["career"]]["group"] == "creative"))


if __name__ == "__main__":
    main()
