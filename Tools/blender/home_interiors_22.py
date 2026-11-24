"""Brix Town - home interiors 22: the town's row houses and flats, each furnished in each of the four lines at each of
the four furnishing levels (FURN-003, FURN-005; look test, not approved). What house_interiors_21.py did for the
detached house.

Run headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/home_interiors_22.py
Quick checks: WASTED_ONLY=row_14x12_2,flat_80_plain,flat_160_curve_4 WASTED_SAMPLES=24   From another checkout: WASTED_ROOT=...
WASTED_ONLY names a home, a home and line, a home, line and level, or one job.

Builds on house_samples_19.py (and through it furniture_kit_18 -> 17 -> building_kit_07 ...): its walls with holes,
furniture placing, people, light and render, with the plans of Art/StyleTests/home_interiors_22/layout_<type>.json,
which home_interiors_22_plan.py writes and checks (run it first). The plans are not sample houses 19's hall-and-two-
sides, so the shell is drawn here from each plan's own partitions, doors, windows and stair.
Each line furnishes its own home (walls in the line's outside material). Renders go to
Art/StyleTests/home_interiors_22/<type>_<line>_<level>_<floor>.png, the floor cut away at waist height and seen from
above the front right corner: every line and level for the first floor (the ground floor, or the flat), Full only for
the floors above. home_interiors_22_sheet.py puts them on a sheet per home.
"""
import os, json

ROOT = os.environ.get("WASTED_ROOT", "C:/Projects/Wasted").replace("\\", "/").rstrip("/")
_h = open(ROOT + "/Tools/blender/house_samples_19.py", encoding="utf-8").read()
_h = _h[:_h.rstrip().rfind("\nmain()")]                   # sample houses 19's walls and helpers, without running it
exec(compile(_h, "house_samples_19_helpers", "exec"), globals())

NAME = "home_interiors_22"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"
TYPES = ["row_14x12_2", "row_14x12_3", "row_16x12_2", "flat_80", "flat_160"]
LINE_KEYS = ("plain", "oldtown", "workshop", "curve")


def home_shell(hp, f, look):
    """Floor, outside walls with windows, partitions with doorways and the stair of one floor of a plan, cut away: the
    back and left outside walls full height, the front and right ones and every partition stop at CUT."""
    global W0, W1, D0, D1, WINDOWS
    W0, W1, D0, D1 = hp["W0"], hp["W1"], hp["D0"], hp["D1"]
    WINDOWS = [tuple(w) for w in hp["windows"][f]]           # window_holes() reads this name
    Tt, tp, Hh = hp["T"], hp["TP"], hp["H"]
    z, top, full = 0.0, CUT, hp["H"]
    i0, i1, j0, j1 = W0 + Tt, W1 - Tt, D0 + Tt, D1 - Tt
    sk = hp["stair_kind"].get(f)
    first = f == hp["floors"][0]
    wm, pm = M[look["wall"]], M["plaster"]
    # floor boards, with the stair well left open on a floor the stair arrives at
    if sk in ("well", "both"):
        x0, x1, y0, y1 = hp["stair"]
        box(f + "_FloorA", x1, i1, j0, j1, z - 0.2, z, M["boards"], bevel=0)
        box(f + "_FloorB", i0, x1, j0, y0, z - 0.2, z, M["boards"], bevel=0)
        box(f + "_FloorC", i0, x1, y1, j1, z - 0.2, z, M["boards"], bevel=0)
    else:
        box(f + "_Floor", i0, i1, j0, j1, z - 0.12, z, M["boards"], bevel=0)
    if not first:
        box(f + "_Below", W0, W1, D0, D1, z - 3.0, z - 2.9, M["dark"], bevel=0)
    fd = hp["front_door"]
    fh = window_holes("front", z) + ([(fd[0], fd[1], z, z + 2.2)] if first and fd else [])
    wall(f + "_Front", "y", D0 + Tt / 2, W0, W1, z, top, wm, fh)
    wall(f + "_Back", "y", D1 - Tt / 2, W0, W1, z, full, wm, window_holes("back", z))
    wall(f + "_Left", "x", W0 + Tt / 2, D0 + Tt, D1 - Tt, z, full, wm, window_holes("left", z))
    wall(f + "_Right", "x", W1 - Tt / 2, D0 + Tt, D1 - Tt, z, top, wm, window_holes("right", z))
    for k, (ax, at, a0, a1) in enumerate(hp["partitions"][f]):
        holes = [(c - 0.45, c + 0.45, z, z + 2.1) for dax, dat, c in hp["doors"][f]
                 if dax == ax and abs(dat - at) < 1e-6 and a0 <= c <= a1]
        lo = (i0 if ax == "y" else j0)
        hi = (i1 if ax == "y" else j1)
        e = tp / 2 - 0.015                                 # into the wall it meets, not flush with its face (no seam)
        wall("%s_P%d" % (f, k), ax, at, max(lo, a0 - e), min(hi, a1 + e), z, top, pm, holes, t=tp)
    if sk:
        x0, x1, y0, y1 = hp["stair"]
        if sk in ("up", "both"):                          # a straight flight against the left wall, rising to the back
            steps = 14
            for i in range(steps):
                sy = y0 + (y1 - y0) * i / steps
                box("%s_Step%d" % (f, i), x0, x1, sy, sy + (y1 - y0) / steps + 0.02, z, z + Hh * (i + 1) / steps, M["wood"], bevel=0.012)
            for i in range(8):
                y = y0 + i * (y1 - y0) / 7
                zz = z + Hh * (y - y0) / (y1 - y0)
                rod(None, "%s_SBal%d" % (f, i), M["wood"], (x1 + 0.05, y, zz), (x1 + 0.05, y, zz + 0.95), 0.022)
            rod(None, f + "_SRail", M["wood"], (x1 + 0.05, y0, z + 0.95), (x1 + 0.05, y1, z + Hh + 0.95), 0.03)
        if sk == "well":                                  # a rail round the well's open side and front
            for i in range(6):
                y = y0 + i * (y1 - y0) / 5
                rod(None, "%s_Bal%d" % (f, i), M["wood"], (x1 + 0.03, y, z), (x1 + 0.03, y, z + 0.95), 0.022)
            rbox(None, f + "_Hand", M["wood"], x1 + 0.03, (y0 + y1) / 2, z + 0.97, 0.07, y1 - y0, 0.06, 0.02)
            rbox(None, f + "_HandF", M["wood"], (x0 + x1) / 2, y0, z + 0.97, x1 - x0, 0.07, 0.06, 0.02)


def furnish(plan, f, L, level, rng):
    items = [(it["piece"], it["x"], it["y"], it["yaw"], it["lift"]) for it in plan["items"]
             if it["storey"] == f and it["level"] <= level]
    furnish_storey(f, items, L, 0.0, rng)


def camera_for(hp):
    """House interiors 21's view of its 12 x 9 m house, moved back or in so the plan fills the frame (its width and
    depth both widen the picture; its depth alone sets the height)."""
    w, d = hp["W1"] - hp["W0"], hp["D1"] - hp["D0"]
    s = max(0.04 * w + 0.037 * d, 0.086 * d)
    return [("cam", (7.6 * s, -14.8 * s, 14.2 * s), (0.0, 0.25 * s, 0.3), 31.0)]


def person_at(hp):
    if hp["key"].startswith("row"):
        return (hp["W0"] + 0.25 + 1.45, -3.0)
    return (hp["W0"] + 3.2, hp["D0"] + 0.25 + 0.6)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    log("Blender " + bpy.app.version_string + ", only " + str(ONLY))
    for t in TYPES:
        plan = json.load(open(OUT_DIR + "/layout_%s.json" % t, encoding="utf-8"))
        hp = plan["house"]
        log("%s: %.2f x %.2f m, floors %s, %d pieces at full" % (t, hp["W1"] - hp["W0"], hp["D1"] - hp["D0"], hp["floors"], len(plan["items"])))
        for key in LINE_KEYS:
            for level in (1, 2, 3, 4):
                for f in hp["floors"]:
                    if f != hp["floors"][0] and level != 4:
                        continue
                    job = "%s_%s_%d_%s" % (t, key, level, f)
                    if ONLY and not any(o in (job, t, "%s_%s" % (t, key), "%s_%s_%d" % (t, key, level)) for o in ONLY):
                        continue
                    try:
                        scene = fresh("Int_" + job)
                        extra_mats()
                        L = [l for l in lines() if l["key"] == key][0]
                        rng = random.Random(22)
                        g = 16 * max(1.0, (hp["W1"] - hp["W0"]) / 12.0)
                        box("Ground", -g, g, -g, g, -0.4, -0.3, M["paving"], bevel=0)
                        home_shell(hp, f, OUTSIDE[key])
                        furnish(plan, f, L, level, rng)
                        people([("PlayerA",) + person_at(hp)])
                        sun_and_sky(SCENE, (0.45, -0.55, 0.70), 5.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.45)
                        render(scene, job, camera_for(hp), 0.35, (1500, 960))
                    except Exception:
                        log("FAILED %s:\n%s" % (job, traceback.format_exc()))
    log("DONE")


main()
