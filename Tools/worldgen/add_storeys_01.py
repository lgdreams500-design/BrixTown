"""Every floor of every building in city_01 (WORLD_MAP WLD-050 to WLD-053; F-BLD-INSIDE level 3, the owner's pick of
2026-11-22: "3: Every floor, true size", "Stairs and a lift" in the towers, "Residents too").

For each building of two or more storeys this writes into its lot in the block file:
  stair     the switchback stairwell: rect [x0, z0, x1, z1] inside the walls, axis "x" or "z" (the way the flights run),
            wall (n, s, w or e: the outer wall lane 0 runs against) and up0 (+1 or -1: the way lane 0 climbs)
  lift      [x, z] of the lift shaft's centre, in buildings of lift_from_floors storeys or more
  storeys   what each floor is for, ground floor first ("main"), from World/rules/storeys.json "uses"
  levels    for each kind of place that has some upstairs, the floor of each one (the same order as objects)
and moves residents' places upstairs as storeys.json "move" says: beds in houses and flats, half the desks in offices.
The stairwell goes against a side wall (never the door's or the back wall, where counters stand), at the front, so its
first flight starts by the way in; any ground-floor place in its way is moved. Both substations are made enterable, each with a place for its engineer.

Run from Tools/worldgen:   py add_storeys_01.py        (running it again changes nothing: what it placed stays)
Then:                      py -m unittest test_world ;  py build_play.py
"""
import json, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def side_of(l):
    x0, z0, x1, z1 = l["rect"]
    dx, dz = l["door"]
    return "n" if abs(dz - z0) < .01 else "s" if abs(dz - z1) < .01 else "w" if abs(dx - x0) < .01 else "e"


def lift_at(l):
    """Where the slice has always put a tower's lift: the back corner (play.js furnish)."""
    x0, z0, x1, z1 = l["rect"]
    side = side_of(l)
    bx = x0 + 1.5 if side == "e" else x1 - 1.5 if side == "w" else x0 + 1.5
    bz = z0 + 1.5 if side == "s" else z1 - 1.5 if side == "n" else z0 + 1.5
    return [bx, bz]


def dist_rect(p, r):
    dx = max(r[0] - p[0], 0, p[0] - r[2])
    dz = max(r[1] - p[1], 0, p[1] - r[3])
    return math.hypot(dx, dz)


def place_stair(l, S):
    """The stairwell rect against one of the two side walls (never the door's wall, never the back wall, where shops,
    cafes, banks and offices keep their counters), starting at the front so the first flight begins by the way in.
    Of the two sides, the one with fewer of the ground floor's places in the way; those are moved (clear_well)."""
    x0, z0, x1, z1 = l["rect"]
    t, st = S["wall"], S["stair"]
    W = 2 * st["width"] + st["spine"]
    L = st["run"] + 2 * st["landing"]
    side = side_of(l)
    lv = l.get("levels", {})
    pts = [tuple(p[:2]) for k, v in l["objects"].items() for i, p in enumerate(v) if not lv.get(k) or lv[k][i] == 0]   # the ground floor's places
    lift = lift_at(l) if l.get("tower") else None
    sides = {"n": "we", "s": "we", "w": "ns", "e": "ns"}[side]
    best = None
    for wall in sides:
        along_x = wall in "ns"
        a0, a1 = (x0 + t, x1 - t) if along_x else (z0 + t, z1 - t)
        if a1 - a0 < L + .2:
            continue
        front_low = side in ("w", "n")                         # is the door at the low end of this side wall?
        for back in (0.0, 1.5, 3.0):                            # as near the front as it can go, a little further back if a lift is there
            a = a0 + .1 + back if front_low else a1 - .1 - back - L
            if along_x:
                zz = (z0 + t, z0 + t + W) if wall == "n" else (z1 - t - W, z1 - t)
                r = [a, zz[0], a + L, zz[1]]
            else:
                xx = (x0 + t, x0 + t + W) if wall == "w" else (x1 - t - W, x1 - t)
                r = [xx[0], a, xx[1], a + L]
            if lift and dist_rect(lift, r) < 1.8:
                continue
            hits = sum(dist_rect(p, r) < st["keep_clear"] for p in pts)
            score = (hits, back)
            if best is None or score < best[0]:
                best = (score, r, wall, along_x)
            break
    if best is None:
        return None
    _, r, wall, along_x = best
    dpos = l["door"][0] if along_x else l["door"][1]
    mid = (r[0] + r[2]) / 2 if along_x else (r[1] + r[3]) / 2
    up0 = 1 if dpos < mid else -1                               # lane 0 climbs away from the front
    return dict(rect=[round(v, 2) for v in r], axis="x" if along_x else "z", wall=wall, up0=up0)


def clear_well(l, st, S):
    """Move any of the ground floor's places that stand in the stairwell, or on the way from the door to its foot, to the
    nearest free spot in the room."""
    x0, z0, x1, z1 = l["rect"]
    r, keep = st["rect"], S["stair"]["keep_clear"]
    lv = l.get("levels", {})
    allp = [(k, i) for k, v in l["objects"].items() for i in range(len(v)) if not lv.get(k) or lv[k][i] == 0]
    dx, dz = l["door"]
    m = S["wall"] + .8
    moved = 0
    for k, i in allp:
        p = l["objects"][k][i]
        if dist_rect(p, r) >= keep:
            continue
        others = [tuple(l["objects"][k2][i2]) for k2, i2 in allp if (k2, i2) != (k, i)]
        best = None
        for gz in [z0 + m + q * .3 for q in range(int((z1 - z0 - 2 * m) / .3) + 1)]:
            for gx in [x0 + m + q * .3 for q in range(int((x1 - x0 - 2 * m) / .3) + 1)]:
                if dist_rect((gx, gz), r) < keep + .5 or math.hypot(gx - dx, gz - dz) < 2.0 or any(math.hypot(gx - a, gz - b) < .9 for a, b in others):
                    continue
                d = math.hypot(gx - p[0], gz - p[1])
                if best is None or d < best[0]:
                    best = (d, gx, gz)
        if best is None:
            raise SystemExit("%s: nowhere to move a %s out of the stairwell" % (l["id"], k))
        l["objects"][k][i] = [round(best[1], 2), round(best[2], 2)]
        moved += 1
    return moved


def floor_spots(l, stair, lift, n, gap, clear_of, S, f=1):
    """n places on upper floor f: a grid inside the walls, clear of the stairwell, the lift and each other. Each floor's grid is
    shifted a little, so no two floors have a place at the same spot on the plan (the slice finds a resident's place by it)."""
    x0, z0, x1, z1 = l["rect"]
    m = S["wall"] + 1.3
    out = []
    sh, shz = .37 * ((f - 1) % 7), .41 * (((f - 1) // 7) % 5)
    xs = [x0 + m + sh + k * gap for k in range(int((x1 - x0 - 2 * m - sh) // gap) + 1)]
    zs = [z0 + m + shz + k * gap for k in range(int((z1 - z0 - 2 * m - shz) // gap) + 1)]
    for z in zs:
        for x in xs:
            if stair and dist_rect((x, z), stair["rect"]) < 1.4:
                continue
            if lift and math.hypot(x - lift[0], z - lift[1]) < 2.6:
                continue
            if any(math.hypot(x - a, z - b) < .5 for a, b in clear_of):
                continue
            out.append([round(x, 2), round(z, 2)])
            if len(out) == n:
                return out
    return out


def main():
    S = jload(os.path.join(RULES, "storeys.json"))
    city = jload(os.path.join(CITY, "city.json"))
    files = [(os.path.join(CITY, e["file"]), None) for e in city["blocks"]]
    blocks = [(p, jload(p)) for p, _ in files]
    moved = dict(bed=0, work=0)
    stairs = lifts = cleared = 0
    for path, b in blocks:
        for l in b["lots"]:
            if l["kind"] in ("garden", "market", "stop") or l.get("house"):   # a designed house has its stair and floors in its plan
                continue
            # (storeys is written again in place below, so a run changes nothing in a lot it has seen; a stair, lift or place
            # once put stays where it is on later runs)
            if not l["enterable"]:
                l["enterable"] = True
                l["objects"].setdefault("work", [[round((l["rect"][0] + l["rect"][2]) / 2, 2), round((l["rect"][1] + l["rect"][3]) / 2, 2)]])
            n = l["floors"]
            key = "flats" if l["kind"] == "home" and l.get("tower") else l["kind"]
            uses = S["uses"].get(key, S["uses"]["default"])
            l["storeys"] = ["main"] + [uses[min(k, len(uses) - 1)] for k in range(n - 1)]
            if n < 2:
                continue
            st = l.get("stair") or place_stair(l, S)
            if st is None:
                raise SystemExit("%s: no wall long enough for a stair" % l["id"])
            l["stair"] = st
            cleared += clear_well(l, st, S)
            stairs += 1
            lift = None
            if n >= S["lift_from_floors"]:
                lift = l.get("lift") or lift_at(l)
                l["lift"] = lift
                lifts += 1
            rules = S["move"].get(key, {})
            if not rules:
                continue
            levels = l.get("levels", {})
            for kind, how in rules.items():
                pts = l["objects"].get(kind, [])
                if not pts or kind in levels:
                    continue
                k_up = len(pts) if how == "all" else len(pts) // 2
                if k_up == 0:
                    continue
                keep, up = pts[:len(pts) - k_up], pts[len(pts) - k_up:]
                per = math.ceil(len(up) / (n - 1))                        # spread over the upper floors, the first ones first
                new, lvl = [], []
                for f in range(1, n):
                    want = min(per, len(up) - len(new))
                    if want <= 0:
                        break
                    got = floor_spots(l, st, lift, want, 2.6 if kind == "bed" else 2.2, [], S, f)
                    if len(got) < want:
                        raise SystemExit("%s: no room upstairs for %d %s" % (l["id"], want, kind))
                    new += got
                    lvl += [f] * len(got)
                l["objects"][kind] = keep + new
                levels[kind] = [0] * len(keep) + lvl
                moved[kind] += len(new)
            if levels:
                l["levels"] = levels
        with open(path, "w", encoding="utf-8") as f:
            json.dump(b, f, ensure_ascii=False, indent=1)
    print("%d stairs, %d lifts; %d places moved out of stairwells; upstairs: %d beds, %d desks" % (stairs, lifts, cleared, moved["bed"], moved["work"]))


if __name__ == "__main__":
    main()
