"""Fill every empty home of city_01 with a household (docs/specs/domains/CITY_BALANCE.md BAL-004; the owner, 2026-11-22:
"Yes, fill homes").

Homes filled: every empty flat of a court building or tower (lot.flats), and every bed of the community houses. Row
houses are already full. Flats over shops are left to the property market (their tenants are worked out there).
  household size   by the flat's size, within its beds (balance.json homes.people is the mean): 80 m² 1-2, 160 m² 2-3,
                   320 m² 3-4, 640 m² 4-5; a house is full to its beds
  roles            so the whole population comes to balance.json targets.residents: each household's first member is an
                   adult (working, commuter, courier or elder); children and students live with adults
  jobs             a working resident takes the nearest workplace (door to door) that still has job places by floor area
                   (balance.json jobs); their role follows the workplace's kind. Each workplace gets a place to work for each
                   of its staff, spread over its floors, so everyone at work has somewhere to stand
  near             each new resident's nearest cafe, grocer, garden, stop, clinic, market (if within 260 m, else the grocer),
                   bank, library, museum, cinema, dealer, playground, noticeboard, kiosk and furniture outlet, as everyone's;
                   and, new for everyone, the nearest school
  rounds           couriers and utility workers get twelve places among the forty nearest, as before
Beds: a home's k-th resident sleeps in its k-th bed (citysim); each flat's beds are put in order so its household's are theirs.
Existing residents keep their ids, homes, jobs and everything else. Run once on a city whose new homes are empty:
    python add_households_01.py     (running it again finds no empty homes and changes nothing)
"""
import json, math, os, random

import make_city_01 as mc

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")
SEED = 20261124
SIZE_PEOPLE = {80: (1, 2), 160: (2, 3), 320: (3, 4), 640: (4, 5)}
ADULT = ("working_in_town", "commuter_out", "courier", "elder")
ROLE_OF_KIND = {"shop": "shopkeeper", "market": "shopkeeper", "furniture": "shopkeeper", "dealer": "shopkeeper", "food": "cafe",
                "cinema": "cafe", "health": "nurse", "bank": "clerk", "library": "clerk", "museum": "clerk", "school": "clerk",
                "police": "clerk", "utility": "utility", "fire": "utility"}


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def jsave(p, o, indent=1):
    with open(p, encoding="utf-8") as f:
        end = "\n" if f.read().endswith("\n") else ""
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        json.dump(o, f, indent=indent, ensure_ascii=False)
        f.write(end)


def area(r):
    return (r[2] - r[0]) * (r[3] - r[1])


def main():
    rng = random.Random(SEED)
    city = jload(os.path.join(CITY, "city.json"))
    bal = jload(os.path.join(RULES, "balance.json"))
    J, T = bal["jobs"], bal["targets"]["residents"]
    files = {e["id"]: os.path.join(CITY, e["file"]) for e in city["blocks"]}
    blocks = {bid: jload(p) for bid, p in files.items()}
    lots = [l for b in blocks.values() for l in b["lots"]]
    tmpl = {l["id"]: b["template"] for b in blocks.values() for l in b["lots"]}
    res_doc = jload(os.path.join(CITY, "residents.json"))
    people = res_doc["residents"]
    mates = {}
    for r in people:
        mates.setdefault(r["home"], []).append(r["id"])

    # ---- 1. households for every empty home
    households = []                                                   # (lot, flat or None, size)
    for l in lots:
        if l["kind"] != "home":
            continue
        if l.get("flats"):
            for q in l["flats"]:
                if not q["household"]:
                    lo, hi = SIZE_PEOPLE[q["size"]]
                    mean = bal["homes"]["people"][str(q["size"])]
                    n = hi if rng.random() < mean - lo else lo
                    households.append((l, q, min(n, len(q["beds"]))))
        elif tmpl[l["id"]] == "community":
            free = len(l["objects"].get("bed", [])) - len(mates.get(l["id"], []))
            if free > 0:
                households.append((l, None, free))
    new_n = sum(n for _, _, n in households)
    if not new_n:
        print("no empty homes")
        return

    # ---- 2. roles toward the targets for the whole population
    group = lambda role: ("child" if role == "child" else "student" if role == "student" else "elder" if role == "elder" else
                          "commuter_out" if role == "commuter" else "courier" if role == "courier" else "working_in_town")
    total = len(people) + new_n
    have = {g: 0 for g in T}
    for r in people:
        have[group(r["role"])] += 1
    need = {g: max(0.0, T[g] * total - have[g]) for g in T}
    scale = new_n / sum(need.values())
    left = {g: need[g] * scale for g in T}

    def draw(pool):
        w = [max(0.0, left[g]) for g in pool]
        if sum(w) <= 0:
            w = [1.0] * len(pool)
        g = rng.choices(pool, w)[0]
        left[g] -= 1
        return g

    FIRST = mc.FIRST
    new = []
    nid = max(int(r["id"][1:]) for r in people) + 1
    for l, q, n in households:
        for k in range(n):
            g = draw(ADULT if k == 0 else list(T))
            r = dict(id="r%04d" % nid, name=rng.choice(FIRST), role=g, home=l["id"], work=None, offset=rng.randrange(-20, 21))
            nid += 1
            new.append(r)
            mates.setdefault(l["id"], []).append(r["id"])
            if q is not None:
                q["household"].append(r["id"])

    # ---- 3. beds in order: a home's k-th resident in its k-th bed
    for l in lots:
        if not l.get("flats"):
            continue
        beds, rooms, bl, rl = l["objects"]["bed"], l["objects"]["room"], l["levels"]["bed"], l["levels"]["room"]
        at_bed = {}
        for q in l["flats"]:
            for k, rid in enumerate(q["household"]):
                at_bed[rid] = (q, k)
        order = []
        for rid in mates.get(l["id"], []):
            q, k = at_bed[rid]
            order.append((q["beds"][k], q["rooms"][k]))
        used = {b for b, _ in order}
        rest = [(q["beds"][k], q["rooms"][k]) for q in l["flats"] for k in range(len(q["beds"])) if q["beds"][k] not in used]
        seq = order + rest
        l["objects"]["bed"] = [beds[b] for b, _ in seq]
        l["objects"]["room"] = [rooms[r] for _, r in seq]
        l["levels"]["bed"] = [bl[b] for b, _ in seq]
        l["levels"]["room"] = [rl[r] for _, r in seq]
        where = {b: n for n, (b, _) in enumerate(seq)}
        wroom = {r: n for n, (_, r) in enumerate(seq)}
        for q in l["flats"]:                                            # the household's beds first, in its members' order
            q["beds"] = sorted(where[b] for b in q["beds"])
            q["rooms"] = sorted(wroom[r] for r in q["rooms"])

    # ---- 4. jobs: nearest workplace with job places left (by floor area)
    sector = lambda l: (J["sector_of_kind"].get(l["kind"]) if l["kind"] != "work" else
                        "making" if any(l["name"].endswith(x) for x in J["making_names_end"]) else "office")
    cap, staff = {}, {}
    for l in lots:
        s = sector(l)
        if not s or not l.get("enterable", True):
            continue
        if "plan" in l:
            a = sum(area(u["rect"]) for L in l["plan"]["levels"] for u in L["units"]) * J["usable_share"]
            cap[l["id"]] = a / J["m2_per_job"]["retail"]
        else:
            floors = 1 if l["kind"] in J["ground_floor_only"] and tmpl[l["id"]] != "towerO" else l.get("floors", 1) or 1
            cap[l["id"]] = area(l["rect"]) * floors * J["usable_share"] / J["m2_per_job"][s]
        staff[l["id"]] = 0
    for r in people:
        if r.get("work") in staff:
            staff[r["work"]] += 1
    byid = {l["id"]: l for l in lots}
    d2 = lambda a, b: abs(a["door"][0] - b["door"][0]) + abs(a["door"][1] - b["door"][1])
    places = [byid[k] for k in cap]
    hired = {}
    for r in new:
        if r["role"] != "working_in_town":
            continue
        h = byid[r["home"]]
        best = min((p for p in places if staff[p["id"]] < cap[p["id"]] - .5), key=lambda p: (d2(h, p), p["id"]))
        staff[best["id"]] += 1
        r["work"] = best["id"]
        k = best["kind"]
        r["role"] = ("maker" if sector(best) == "making" else "clerk") if k == "work" else ROLE_OF_KIND.get(k, "clerk")
        hired[best["id"]] = hired.get(best["id"], 0) + 1
    for r in new:
        r["role"] = {"commuter_out": "commuter", "working_in_town": "clerk"}.get(r["role"], r["role"])

    # places to work for the new staff, spread over the workplace's floors, clear of its stair
    for lid, n in hired.items():
        l = byid[lid]
        have_spots = len(l["objects"].get("work", []))
        want = staff[lid]
        if want <= have_spots:
            continue
        floors = 1 if (l["kind"] in J["ground_floor_only"] and tmpl[lid] != "towerO") or "plan" in l else l.get("floors", 1) or 1
        x0, z0, x1, z1 = l["rect"]
        st = (l.get("stair") or {}).get("rect")
        per = -(-(want - have_spots) // floors) + 4                    # places a floor, so no two staff share one
        nx = max(2, int(math.ceil(math.sqrt(per * (x1 - x0) / max(1.0, z1 - z0)))))
        nz = max(2, int(math.ceil(per / nx)) + 1)
        grid = [(x0 + 1.6 + (x1 - x0 - 3.2) * (i + .5) / nx, z0 + 1.6 + (z1 - z0 - 3.2) * (j + .5) / nz) for j in range(nz) for i in range(nx)]
        if st:
            grid = [p for p in grid if not (st[0] - .8 <= p[0] <= st[2] + .8 and st[1] - .8 <= p[1] <= st[3] + .8)]
        l["objects"].setdefault("work", [])
        lv = l.setdefault("levels", {}) if floors > 1 else None
        if lv is not None:
            lv.setdefault("work", [0] * have_spots)
        for k in range(want - have_spots):
            f = k % floors
            p = grid[(k // floors) % len(grid)]
            l["objects"]["work"].append([round(p[0], 2), round(p[1], 2)])
            if lv is not None:
                lv["work"].append(f)

    # ---- 5. what is near each new resident, and rounds
    kinds = dict(cafe=[l for l in lots if l["kind"] == "food" or "cafe" in l.get("tags", [])],
                 grocer=[l for l in lots if l["kind"] == "shop" and "plan" not in l],
                 garden=[l for l in lots if l["kind"] == "garden"], stop=[l for l in lots if l["kind"] == "stop"],
                 clinic=[l for l in lots if l["kind"] == "health"], bank=[l for l in lots if l["kind"] == "bank"],
                 library=[l for l in lots if l["kind"] == "library"], museum=[l for l in lots if l["kind"] == "museum"],
                 cinema=[l for l in lots if l["kind"] == "cinema"], dealer=[l for l in lots if l["kind"] == "dealer"],
                 playground=[l for l in lots if "swing" in l["objects"]], noticeboard=[l for l in lots if "notice" in l["objects"]],
                 kiosk=[l for l in lots if "kiosk" in l["objects"]], furniture=[l for l in lots if l["kind"] == "furniture"],
                 school=[l for l in lots if l["kind"] == "school"])
    market = next(l for l in lots if l["kind"] == "market")
    visitable = [l for l in lots if l["kind"] in ("shop", "food", "work", "health", "home", "utility") and l.get("enterable") and "plan" not in l]
    near_cache, old = {}, {r["id"] for r in people}
    for r in people + new:
        h = byid[r["home"]]
        if h["id"] not in near_cache:
            nr = {tok: min(ls, key=lambda l: (d2(h, l), l["id"]))["id"] for tok, ls in kinds.items() if ls}
            nr["market"] = market["id"] if d2(h, market) < 260 else nr["grocer"]
            near_cache[h["id"]] = nr
        nr = dict(near_cache[h["id"]])
        if r["id"] in old:                                               # existing residents only learn their school
            r["near"]["school"] = nr["school"]
            continue
        r["near"] = {k: nr[k] for k in ("cafe", "grocer", "garden", "stop", "clinic", "market", "bank", "library", "museum", "cinema",
                                         "dealer", "playground", "noticeboard", "kiosk", "furniture", "school")}
        if r["role"] in ("courier", "utility"):
            close = sorted(visitable, key=lambda l: (d2(h, l), l["id"]))[:40]
            r["rounds"] = [l["id"] for l in rng.sample(close, 12)]

    people += new
    jsave(os.path.join(CITY, "residents.json"), res_doc, 0)
    for bid, b in blocks.items():
        jsave(files[bid], b)
    roles = {}
    for r in people:
        roles[r["role"]] = roles.get(r["role"], 0) + 1
    print("%d households, %d new residents; %d in all" % (len(households), len(new), len(people)))
    print(", ".join("%s %d" % kv for kv in sorted(roles.items(), key=lambda kv: -kv[1])))
    print("hired into %d workplaces; most: %s" % (len(hired), sorted(((n, byid[k]["name"]) for k, n in hired.items()), reverse=True)[:6]))


if __name__ == "__main__":
    main()
