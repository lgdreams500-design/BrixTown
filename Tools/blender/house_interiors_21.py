"""Brix Town - house interiors 21: the sample house at the town's own size (12 x 9 m) furnished in each of the four
lines at each of four furnishing levels, from bare essentials to every piece of every set (FURN-003, FURN-005; look
test, not approved).

Run headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/house_interiors_21.py
Quick checks: WASTED_ONLY=plain_4_ground,curve_1 WASTED_SAMPLES=24   From another checkout: WASTED_ROOT=C:/Projects/...

Builds on house_samples_19.py (and through it furniture_kit_18 -> 17 -> building_kit_07 ...): its walls, doors, stair,
cutaway and light, with the plan and furniture of Art/StyleTests/house_interiors_21/layout.json, which
house_interiors_21_plan.py writes and checks (run it first; it reads furniture_sizes.json from furniture_sizes_21.py).
Each line furnishes its own house (Plain furniture in the Plain house, and so on); the walls are the line's own.
Renders go to Art/StyleTests/house_interiors_21/<line>_<level>_<storey>.png, the storey cut away at waist height and
seen from above the street corner; house_interiors_21_sheet.py puts them on two sheets.
"""
import os, json

ROOT = os.environ.get("WASTED_ROOT", "C:/Projects/Wasted").replace("\\", "/").rstrip("/")
_h = open(ROOT + "/Tools/blender/house_samples_19.py", encoding="utf-8").read()
_h = _h[:_h.rstrip().rfind("\nmain()")]                   # sample houses 19's walls, stair and rooms, without running it
exec(compile(_h, "house_samples_19_helpers", "exec"), globals())

NAME = "house_interiors_21"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"
PLAN = json.load(open(OUT_DIR + "/layout.json", encoding="utf-8"))

# the plan replaces sample houses 19's (its functions read these names)
_hp = PLAN["house"]
W0, W1, D0, D1, T, XA, XB, YM, H = (_hp[k] for k in ("W0", "W1", "D0", "D1", "T", "XA", "XB", "YM", "H"))
STAIR = tuple(_hp["stair"])
DOORS = {k: [tuple(d) for d in v] for k, v in _hp["doors"].items()}
WINDOWS = [tuple(w) for w in _hp["windows"]]


def furnish(storey, L, level, rng):
    items = [(it["piece"], it["x"], it["y"], it["yaw"], it["lift"]) for it in PLAN["items"]
             if it["storey"] == storey and it["level"] <= level]
    furnish_storey(storey, items, L, 0.0, rng)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    log("plan %.0f x %.0f m, %d pieces at full" % (W1 - W0, D1 - D0, len(PLAN["items"])))
    for key in ("plain", "oldtown", "workshop", "curve"):
        for level in (1, 2, 3, 4):
            for storey in ("ground", "upstairs"):
                job = "%s_%d_%s" % (key, level, storey)
                if ONLY and job not in ONLY and key not in ONLY and "%s_%d" % (key, level) not in ONLY:
                    continue
                try:
                    scene = fresh("Int_" + job)
                    extra_mats()
                    L = [l for l in lines() if l["key"] == key][0]
                    rng = random.Random(21)
                    box("Ground", -16, 16, -14, 14, -0.4, -0.3, M["paving"], bevel=0)
                    storey_shell(storey, storey, 0.0, True, OUTSIDE[key])
                    furnish(storey, L, level, rng)
                    people([("PlayerA", 0.2, -0.9)])
                    sun_and_sky(SCENE, (0.45, -0.55, 0.70), 5.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.45)
                    render(scene, job, [("cam", (7.6, -14.8, 14.2), (0.0, 0.25, 0.3), 31.0)], 0.35, (1500, 960))
                except Exception:
                    log("FAILED %s:\n%s" % (job, traceback.format_exc()))
    log("DONE")


main()
