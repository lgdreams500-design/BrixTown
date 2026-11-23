"""Brix Town - house styles 20: each of the four sample house lines in three colourways, to choose the colour sets and
trim that World/rules/house_styles.json will hold (look test; not approved).

Run headless:
    blender -b --factory-startup --python C:/Projects/Wasted/Tools/blender/house_styles_20.py
Quick checks: WASTED_ONLY=plain_a,curve_b WASTED_SAMPLES=24   From another checkout: WASTED_ROOT=C:/Projects/Wasted-remodelbuilds

Builds on house_samples_19.py (and through it furniture_kit_18 -> 17 -> building_kit_07 -> old_town_04 ...): the same
house, garden, kit light and camera; only the outside changes. Each line keeps its own shape (Plain plain, Old Town
corner stones, door hood and window boxes, Workshop dressed stone, Curve arched ground floor); colourway "a" is the
line as house_samples_19 has it, "b" and "c" are other colour sets for it. Every colour is a palette entry of
docs/art/ART_DIRECTION.md section 2.3 or a roof tone of World/rules/building_kit.json, so the look stays the locked one
(ADR-0004); shutters all take the colourway's own colour here (the town kit mixes in others) so each set reads clearly.
Renders go to Art/StyleTests/house_styles_20/<line>_<a|b|c>.png; Tools/blender/house_styles_20_sheet.py puts them on
one sheet.
"""
import os

ROOT = os.environ.get("WASTED_ROOT", "C:/Projects/Wasted").replace("\\", "/").rstrip("/")
_h = open(ROOT + "/Tools/blender/house_samples_19.py", encoding="utf-8").read()
_h = _h[:_h.rstrip().rfind("\nmain()")]                   # sample houses 19's house and garden, without running it
exec(compile(_h, "house_samples_19_helpers", "exec"), globals())

NAME = "house_styles_20"
OUT_DIR = OUT + "/" + NAME
LOG = OUT_DIR + "/" + NAME + "_log.txt"

# ---- the palette (ART_DIRECTION 2.3, linear RGB) -------------------------------------------------------------------
PLASTER = ((0.46, 0.43, 0.40), (0.68, 0.63, 0.55))         # cool plaster
PLASTER_WARM = ((0.50, 0.40, 0.33), (0.70, 0.58, 0.46))
PLASTER_PALE = ((0.54, 0.51, 0.46), (0.74, 0.70, 0.62))     # plaster mixed half with the kit's inner tone (inner_mix)
PLASTER_OCHRE = ((0.52, 0.44, 0.32), (0.70, 0.60, 0.44))    # plaster warm mixed a quarter with ochre
ACCENT = dict(ochre=(0.52, 0.40, 0.20), terracotta=(0.44, 0.24, 0.18), teal=(0.20, 0.36, 0.37), cream=(0.66, 0.61, 0.50),
              dusty_blue=(0.30, 0.38, 0.52), brown=(0.24, 0.16, 0.11), olive=(0.30, 0.33, 0.20), plum=(0.30, 0.24, 0.34))
ROOF = dict(terracotta=((0.34, 0.18, 0.13), (0.52, 0.30, 0.20)),   # building_kit.json roof_tiles, as tile pairs
            brown=((0.20, 0.15, 0.10), (0.34, 0.26, 0.18)),   # less red than terracotta: reads as old brown tile
            dark=((0.13, 0.08, 0.06), (0.25, 0.15, 0.11)),    # the kit's dark tile, deepened (AgX lifts it in sun)
            slate=((0.16, 0.16, 0.20), (0.28, 0.28, 0.33)))

# each colourway: walls (plaster pair or "stone"), shutters, door, window frames, roof; shape stays the line's own
WAYS = {
    "plain": {
        "a": dict(name="Teal on cool plaster", wall=PLASTER, shutter="teal", door="teal", frame="cream", roof="terracotta"),
        "b": dict(name="Dusty blue on pale plaster, slate roof", wall=PLASTER_PALE, shutter="dusty_blue", door="dusty_blue", frame="cream", roof="slate"),
        "c": dict(name="Olive on warm plaster, brown roof", wall=PLASTER_WARM, shutter="olive", door="brown", frame="cream", roof="brown"),
    },
    "oldtown": {
        "a": dict(name="Terracotta on warm plaster", wall=PLASTER_WARM, shutter="terracotta", door="terracotta", frame="brown", roof="terracotta"),
        "b": dict(name="Ochre on ochre-washed plaster", wall=PLASTER_OCHRE, shutter="ochre", door="brown", frame="brown", roof="terracotta"),
        "c": dict(name="Plum on cool plaster, dark roof", wall=PLASTER, shutter="plum", door="plum", frame="cream", roof="dark"),
    },
    "workshop": {
        "a": dict(name="Brown wood on stone, dark roof", wall="stone", shutter="brown", door="brown", frame="cream", roof="dark"),
        "b": dict(name="Teal on stone, terracotta roof", wall="stone", shutter="teal", door="teal", frame="cream", roof="terracotta"),
        "c": dict(name="Dusty blue on stone, slate roof", wall="stone", shutter="dusty_blue", door="brown", frame="cream", roof="slate"),
    },
    "curve": {
        "a": dict(name="Dusty blue and ochre on cool plaster", wall=PLASTER, shutter="dusty_blue", door="ochre", frame="ochre", roof="terracotta"),
        "b": dict(name="Cream and terracotta on warm plaster", wall=PLASTER_WARM, shutter="cream", door="terracotta", frame="cream", roof="terracotta"),
        "c": dict(name="Teal and cream on pale plaster", wall=PLASTER_PALE, shutter="teal", door="teal", frame="cream", roof="brown"),
    },
}


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
