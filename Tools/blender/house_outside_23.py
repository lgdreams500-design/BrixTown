"""Brix Town - house outside 23: the twelve approved colourways (ADR-0032) on the house at the town's own size, 12 x 9 m,
with the windows and front door of the approved plan (ADR-0035; World/rules/home_plans.json "house_12x9"), so the
outside matches the rooms behind it (look test).

Run headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/house_outside_23.py
Quick checks: WASTED_ONLY=plain_a WASTED_SAMPLES=24   From another checkout: WASTED_ROOT=C:/Projects/...

Builds on house_styles_20.py (and through it house_samples_19.py ...): the same colourways from house_styles.json, the
same garden, light and camera angle, a step further back for the larger house. Renders go to
Art/StyleTests/house_outside_23/<line>_<a|b|c>.png; house_styles_20_sheet.py with WASTED_SHEET=house_outside_23 puts
them on one sheet.
"""
import os, json

ROOT = os.environ.get("WASTED_ROOT", "C:/Projects/Wasted").replace("\\", "/").rstrip("/")
_s = open(ROOT + "/Tools/blender/house_styles_20.py", encoding="utf-8").read()
_s = _s[:_s.rstrip().rfind("\nmain()")]                   # house styles 20's colourways and house, without running it
exec(compile(_s, "house_styles_20_helpers", "exec"), globals())

NAME = "house_outside_23"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"
_hp = json.load(open(ROOT + "/World/rules/home_plans.json", encoding="utf-8"))["plans"]["house_12x9"]["house"]
W0, W1, D0, D1, T, XA, XB, YM, H = (_hp[k] for k in ("W0", "W1", "D0", "D1", "T", "XA", "XB", "YM", "H"))
WINDOWS = [tuple(w) for w in _hp["windows"]]
FACES = dict(front=((0.0, D0), 0.0, (W0, W1), 1), back=((0.0, D1), math.pi, (W0, W1), -1),
             left=((W0, 0.0), -math.pi / 2, (D0 + WT, D1 - WT), -1), right=((W1, 0.0), math.pi / 2, (D0 + WT, D1 - WT), 1))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    KIT["own_shutter"] = 1.0
    for key in ("plain", "oldtown", "workshop", "curve"):
        for w, way in WAYS[key].items():
            job = "%s_%s" % (key, w)
            if ONLY and job not in ONLY and key not in ONLY:
                continue
            try:
                scene = fresh("Out_" + job)
                extra_mats()
                way_mats(way)
                L = [l for l in lines() if l["key"] == key][0]
                outside(styled_look(key, way), L, random.Random(19))
                people([("PlayerA", 1.6, -6.4), ("Elder", -5.2, -7.0)])
                sun_and_sky(SCENE, (0.55, -0.62, 0.56), 6.0, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.6)
                render(scene, job, [("cam", (14.2, -23.6, 7.0), (-0.3, -0.5, 3.4), 30.0)], -0.15, (1200, 740))
                log("%s: %s" % (job, way["name"]))
            except Exception:
                log("FAILED %s:\n%s" % (job, traceback.format_exc()))
    log("DONE")


main()
