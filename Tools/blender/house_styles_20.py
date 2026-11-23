"""Brix Town - house styles 20: each of the four sample house lines in three colourways, to choose the colour sets and
trim of World/rules/house_styles.json (look test; all twelve approved, ADR-0032).

Run headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/house_styles_20.py
Quick checks: WASTED_ONLY=plain_a,curve_b WASTED_SAMPLES=24   From another checkout: WASTED_ROOT=C:/Projects/Wasted-remodelbuilds

Builds on house_samples_19.py (and through it furniture_kit_18 -> 17 -> building_kit_07 -> old_town_04 ...): the same
house, garden, kit light and camera; only the outside changes. Each line keeps its own shape (Plain plain, Old Town
corner stones, door hood and window boxes, Workshop dressed stone, Curve arched ground floor); colourway "a" is the
line as house_samples_19 has it, "b" and "c" are other colour sets for it. The colourways are read from
World/rules/house_styles.json (approved in ADR-0032); every colour there is a palette entry of ART_DIRECTION 2.3, a mix
of two, or a roof tone of building_kit.json, so the look stays the locked one (ADR-0004); shutters all take the colourway's own colour here (the town kit mixes in others) so each set reads clearly.
Renders go to Art/StyleTests/house_styles_20/<line>_<a|b|c>.png; Tools/blender/house_styles_20_sheet.py puts them on
one sheet.
"""
import os, json

ROOT = os.environ.get("WASTED_ROOT", "C:/Projects/Wasted").replace("\\", "/").rstrip("/")
_h = open(ROOT + "/Tools/blender/house_samples_19.py", encoding="utf-8").read()
_h = _h[:_h.rstrip().rfind("\nmain()")]                   # sample houses 19's house and garden, without running it
exec(compile(_h, "house_samples_19_helpers", "exec"), globals())

NAME = "house_styles_20"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"

# ---- the colourways: World/rules/house_styles.json (ADR-0032), the one place they are kept -------------------------
STYLES = json.load(open(ROOT + "/World/rules/house_styles.json", encoding="utf-8"))
ACCENT = {k: tuple(v) for k, v in STYLES["palette"].items()}
ROOF = {k: (tuple(v[0]), tuple(v[1])) for k, v in STYLES["roofs"].items()}
WAYS = {line: {w: dict(c, wall="stone" if c["wall"] == "stone" else tuple(map(tuple, STYLES["plasters"][c["wall"]])))
               for w, c in ways.items()} for line, ways in STYLES["colourways"].items()}


def way_mats(way):
    """The colourway's materials, under their own names, in the kit's own way of painting (W1)."""
    if way["wall"] != "stone":
        M["v_wall"] = painted("M_V_Wall", way["wall"][0], way["wall"][1], noise_scale=0.7, bump=0.25)
    for part in ("shutter", "door", "frame"):
        M["v_" + part] = flat("M_V_" + part, ACCENT[way[part]])
    lo, hi = ROOF[way["roof"]]
    M["tile"] = painted("M_V_Tile", lo, hi, noise_scale=2.2, bump=0.3)
    M["tile_dark"] = painted("M_V_TileDark", tuple(c * .7 for c in lo), tuple(c * .7 for c in hi), noise_scale=2.2, bump=0.3)


def styled_look(key, way):
    look = dict(OUTSIDE[key])
    look.update(shutter="v_shutter", door="v_door", frame="v_frame", roof="tile")
    if way["wall"] != "stone":
        look.update(wall="v_wall", gable="v_wall")
    return look


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    KIT["own_shutter"] = 1.0                                  # every shutter in the colourway's colour
    for key in ("plain", "oldtown", "workshop", "curve"):
        for w, way in WAYS[key].items():
            job = "%s_%s" % (key, w)
            if ONLY and job not in ONLY and key not in ONLY:
                continue
            try:
                scene = fresh("Style_" + job)
                extra_mats()
                way_mats(way)
                L = [l for l in lines() if l["key"] == key][0]
                rng = random.Random(19)
                outside(styled_look(key, way), L, rng)
                people([("PlayerA", 1.6, -6.2), ("Elder", -4.8, -6.8)])
                sun_and_sky(SCENE, (0.55, -0.62, 0.56), 6.0, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.6)
                render(scene, job, [("cam", (12.5, -21.0, 6.2), (-0.3, -0.5, 3.2), 30.0)], -0.15, (1200, 740))
                log("%s: %s" % (job, way["name"]))
            except Exception:
                log("FAILED %s:\n%s" % (job, traceback.format_exc()))
    log("DONE")


main()
