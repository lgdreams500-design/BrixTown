"""Brix Town - unreal bakery 50: the Stone Oven's own pieces, as approved in baker look 47 (ADR-0059), as one mesh each
for the Unreal town (QST-014: the Stone Oven inside; F-QUEST-STORY).

Headless, from the repository root:
    blender-launcher.exe -b --factory-startup --python Tools/blender/unreal_bakery_50.py
Quick check: WASTED_ONLY=bk_oven,bk_bench (piece names, comma separated) exports only those.
The launcher returns at once and prints nothing; the log below says what was made ("DONE" at the end).

Builds on baker_look_47.py (the pieces: stone_oven, bench, racks, sacks, price_board, door, shop_counter, rolls_tray),
read and run without its main(), and through it on elders_48.py and furniture kits 17 and 18 (the materials and the
Old Town line). The colouring and export are unreal_furniture_37.py's own (bake(), gltf_options()), read from that file:
each face gets its material's base colour as a linear vertex colour, one material slot "furniture", modifiers applied,
lights left out. So the meshes are the approved look's own pieces; nothing is re-modelled.

Each piece goes to Art/Export/unreal_bakery_50/<piece>_oldtown.glb, one object SM_<piece>_oldtown, in the kits' frame
(metres, z up, standing on z = 0 at its group's origin, its back toward +y, its front toward -y), and into index.json
(file, box, triangles) in the same form as unreal_furniture_37's, so Tools/unreal/import_furniture_kit.py imports both.
    bk_oven          the stone oven: arched mouth, hearth ledge, wood store, iron door, peel (its fire is coloured, not lit:
                     the game adds the glow, World/rules/fitouts.json)
    bk_bench         the kneading bench with flour, dough, pin, scale, proofing baskets and a tray of rolls
    bk_racks_mother  bread racks with the crock of the Mother alone on top (one in the bakery)
    bk_racks_a/b/c   bread racks without the crock, each with its own loaves (three seeds, so the wall is not one rack copied)
    bk_sacks         three flour sacks, a spill and a scoop
    bk_board         the chalk price board
    bk_door_alley    the plank door to the alley with its lantern (the four o'clock door)
    bk_door_proof    the plank door to the proofing room
    bk_counter       kit 18's counter with its till, scale and paper, and a basket of batons
"""
import bpy, builtins, json, math, os, random, time, traceback

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")).replace("\\", "/")
_real_open = builtins.open


def open(path, *a, **k):
    """The kits read each other from C:/Projects/Wasted; read them from this checkout instead."""
    if isinstance(path, str):
        p = path.replace("\\", "/")
        if p.lower().startswith("c:/projects/wasted/") and not p.lower().startswith(REPO.lower() + "/"):
            path = REPO + p[len("C:/Projects/Wasted"):]
    return _real_open(path, *a, **k)


os.environ["WASTED_ROOT"] = REPO
_b = _real_open(REPO + "/Tools/blender/baker_look_47.py", encoding="utf-8").read()
_b = _b[:_b.rstrip().rfind("\nmain()")]                     # baker look 47 (and through it elders 48, kits 17 and 18)
exec(compile(_b, "baker_look_47_helpers", "exec"), globals())
_u = _real_open(REPO + "/Tools/blender/unreal_furniture_37.py", encoding="utf-8").read()
_u = _u[_u.index("# ---- each material remembers"):_u.index("\ndef main():")]   # colour remembering, bake(), gltf_options(), clear_new()
exec(compile(_u, "unreal_furniture_37_export", "exec"), globals())

NAME = "unreal_bakery_50"
EXPORT = REPO + "/Art/Export/" + NAME
LOG = EXPORT + "/" + NAME + "_log.txt"
ONLY = tuple(os.environ["WASTED_ONLY"].split(",")) if os.environ.get("WASTED_ONLY") else None
LINE = "oldtown"


def log(msg):
    line = time.strftime("%H:%M:%S") + " " + msg
    print(line)
    with _real_open(LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")


def _racks(seed, mother):
    def build(P, L, rng):
        racks(P, L, random.Random(seed), w=2.5)
        if not mother:
            for ob in list(bpy.context.scene.objects):
                if ob.name.startswith("Mother_"):
                    bpy.data.objects.remove(ob, do_unlink=True)
    return build


def _bench(P, L, rng):
    bench(P, L, rng)
    rolls_tray(P, "Tray", 0.72, -0.2, 0.905, rng)


BAKERY_PIECES = [
    ("bk_oven", lambda P, L, rng: stone_oven(P)),
    ("bk_bench", _bench),
    ("bk_racks_mother", _racks(45, True)),
    ("bk_racks_a", _racks(46, False)),
    ("bk_racks_b", _racks(47, False)),
    ("bk_racks_c", _racks(48, False)),
    ("bk_sacks", lambda P, L, rng: sacks(P, rng)),
    ("bk_board", lambda P, L, rng: price_board(P)),
    ("bk_door_alley", lambda P, L, rng: door(P, "Alley", lamp=True)),
    ("bk_door_proof", lambda P, L, rng: door(P, "Proofing")),
    ("bk_counter", lambda P, L, rng: shop_counter(P, L, rng)),
]


def main():
    os.makedirs(EXPORT, exist_ok=True)
    _real_open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    fresh("Bakery_Export")
    bakery_mats()                                # kit 18's materials and the bakery's (crust, flour, dough, sack, bench)
    L = [l for l in lines() if l["key"] == LINE][0]
    furniture = bpy.data.materials.new("furniture")
    opts = gltf_options()
    index, failed = {}, []
    t0 = time.time()
    for pname, build in BAKERY_PIECES:
        if ONLY and pname not in ONLY:
            continue
        stem = "%s_%s" % (pname, LINE)
        before = set(bpy.context.scene.objects)
        try:
            P = group("G_" + stem, 0.0, 0.0, 0.0)
            build(P, L, random.Random(45))
            ob = bake("SM_" + stem, [o for o in bpy.context.scene.objects if o not in before], furniture)
            for o in bpy.context.scene.objects:
                o.select_set(o == ob)
            bpy.context.view_layer.objects.active = ob
            bpy.ops.export_scene.gltf(filepath=EXPORT + "/" + stem + ".glb", **opts)
            vs = [v.co for v in ob.data.vertices]
            lo = [round(min(v[i] for v in vs), 3) for i in range(3)]
            hi = [round(max(v[i] for v in vs), 3) for i in range(3)]
            tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
            index[pname] = {LINE: dict(file=stem + ".glb", mesh="SM_" + stem, min=lo, max=hi, tris=tris)}
            log("%s: %d triangles, box %s..%s" % (stem, tris, lo, hi))
        except Exception:
            failed.append(stem)
            log("FAILED %s:\n%s" % (stem, traceback.format_exc()))
        clear_new(before)
    doc = dict(source="Tools/blender/unreal_bakery_50.py",
               frame="metres, z up, standing on z = 0, back toward +y, front toward -y (the kits' frame)",
               lines=[LINE], pieces=index)
    if not ONLY:
        with _real_open(EXPORT + "/index.json", "w", encoding="utf-8") as f:
            json.dump(doc, f, indent=1)
    log("%d pieces exported, %d failed %s in %.0fs" % (len(index), len(failed), failed, time.time() - t0))
    log("DONE")


main()
