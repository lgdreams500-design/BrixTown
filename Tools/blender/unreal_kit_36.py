"""Brix Town - unreal kit 36: the building kit's openings and roof as separate meshes for the Unreal town (ART-001, ART-002).

Headless:
    blender-launcher.exe -b --factory-startup --python Tools/blender/unreal_kit_36.py

Builds on building_kit_07.py: each piece below is made by that kit's own builders (window(), door(), voussoirs(),
louvres()), once, at a standard size, and exported as one mesh to Art/Export/unreal_kit_36/<piece>.glb. Unreal places
the pieces on every building from the data (Game/Source/BrixTown/Private/BrixTownActor.cpp); the walls between them,
the cores and the roofs are laid there as boxes, so a whole town costs a few small files instead of one model per lot.

Each piece's frame: x along the facade, centred on the opening; y into the building, the facade's face at y = 0 and the
kit's wall 0.32 m deep (KIT["wall_t"]); z up, 0 at the bottom of the opening (a window's sill, a door's threshold). The
opening it needs in the wall is OPENINGS[piece] = (width, height). Material slots are named after the kit's materials
(stone_dark, glass, frame, shutter, door, ...); Unreal gives each slot its colour from Config/DefaultGame.ini.
"""
import bpy, bmesh, math, random, os, json, traceback

B = "C:/Projects/Wasted/Tools/blender/"
_k = open(B + "building_kit_07.py", encoding="utf-8").read().split("\n# ---- the street and the scenes")[0]
exec(compile(_k, "building_kit_07_helpers", "exec"), globals())

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")).replace("\\", "/")
NAME = "unreal_kit_36"
EXPORT = ROOT + "/Art/Export/" + NAME
LOG = EXPORT + "/" + NAME + "_log.txt"
T = KIT["wall_t"]
LOOK = dict(frame="frame", shutter="shutter", door="door", shutters=False, boxes=False)


class Mats(dict):
    """A plain material per kit material name, made on first use; only the name matters (it becomes the slot name)."""
    def __missing__(self, key):
        m = bpy.data.materials.new(key)
        self[key] = m
        return m


MATS = Mats()
K_M = MATS                                   # the kit's builders that make plants and produce look materials up here


def win_up(K, rng):
    window(K, rng, 0, 0, KIT["win_w"], KIT["win_h"], T, LOOK, surround=True)


def win_top(K, rng):
    window(K, rng, 0, 0, KIT["win_w"], KIT["top_win_h"], T, LOOK, surround=True)


def win_plain(K, rng):
    window(K, rng, 0, 0, KIT["win_w"], KIT["win_h"], T, LOOK, surround=False)


def win_arch(K, rng):                       # old town ground storey (building(): sill 0.95, 1.1 x 1.75, arched)
    h = window(K, rng, 0, 0, KIT["win_w"], 1.75, T, LOOK, arch=True)
    voussoirs(K, rng, 0, h["spring"], KIT["win_w"] / 2, T, ring=0.35)


def win_cafe(K, rng):                       # a cafe's arched, lit ground window with its wooden riser below the sill
    h = window(K, rng, 0, 0, 2.0, 2.45, T, LOOK, arch=True, lit=True)
    voussoirs(K, rng, 0, h["spring"], 1.0, T, ring=0.4)
    K.block("wood", -1.0, 1.0, T - 0.16, T - 0.02, -0.55, 0.0, 0.015)


def win_civic(K, rng):                      # clinics and workplaces: arched, in stone
    h = window(K, rng, 0, 0, 1.25, 1.65, T, LOOK, arch=True)
    voussoirs(K, rng, 0, h["spring"], 0.625, T, ring=0.35)


def door_arch(K, rng):
    door(K, rng, 0, KIT["door_w"], 2.5, T, LOOK, arch=True)
    voussoirs(K, rng, 0, 2.5 - KIT["door_w"] / 2, KIT["door_w"] / 2, T, ring=0.4)


def door_plain(K, rng):
    door(K, rng, 0, KIT["door_w"], KIT["door_h"], T, LOOK, arch=False, hood=True)


def door_civic(K, rng):
    door(K, rng, 0, 1.5, 2.6, T, LOOK, arch=True, steps=2)
    voussoirs(K, rng, 0, 2.6 - 0.75, 0.75, T, ring=0.4)


def shutters(h):
    def make(K, rng):                       # a pair of shutters open flat against the wall, as window() lays them
        w = KIT["win_w"]
        x0, x1, sw = -w / 2, w / 2, w / 2
        louvres(K, "shutter", x0 - 0.2 - sw, x0 - 0.2, -0.09, 0, h)
        louvres(K, "shutter", x1 + 0.2, x1 + 0.2 + sw, -0.09, 0, h)
    return make


def roof_wedge(K, rng):                     # 1 m each way: scaled in Unreal to each roof (ridge along x)
    K.prism("roof", [(0.0, 0.0), (1.0, 0.0), (0.5, 1.0)], -0.5, 0.5)


# ---- shopfronts, cafe fronts and balconies (kit shopfront(), awning(), cafe_front(), balcony()) --------------------
# A shopfront is laid in Unreal from units, as shopfront() lays them along the facade: a pier at each division, a pane or
# the door between two piers (1 m wide here, stretched to the gap), a fascia (1 m, stretched to the front), awning strips.
PW = 0.32                                    # shopfront() pilaster width
SHOP_TOP = 3.45


def shop_pier(K, rng):                       # at x = 0: the pilaster and its stone plinth
    K.block("sign", -PW / 2, PW / 2, -0.16, T, 0.0, 2.55, 0.02)
    K.block("stone_dark", -PW / 2 - 0.04, PW / 2 + 0.04, -0.2, T, 0.0, 0.3)


def shop_pane(K, rng):                       # one pane between piers, x -0.5..0.5: riser, sill, lit glass, mullion, transom
    a, b = -0.5, 0.5
    K.block("sign", a, b, -0.06, T, 0.0, 0.7, 0.015)
    K.block("stone_dark", a - 0.02, b + 0.02, -0.12, T, 0.7, 0.78)
    K.block("lit", a, b, T - 0.08, T - 0.04, 0.78, 2.55, 0)
    K.block("sign", -0.03, 0.03, T - 0.12, T - 0.06, 0.78, 2.55, 0.008)
    K.block("sign", a, b, T - 0.12, T - 0.06, 2.0, 2.05, 0.008)


def shop_door(K, rng):                       # the door bay between piers, x -0.5..0.5
    a, b = -0.5, 0.5
    K.block("glass", a, b, T - 0.15, T - 0.1, 0.0, 2.55, 0)
    for k in range(3):
        K.block("sign", a + k * (b - a) / 3, a + (k + 1) * (b - a) / 3, T - 0.2, T - 0.12, 0.0, 0.9, 0.012)
    K.block("sign", a, b, T - 0.2, T - 0.12, 1.9, 1.98, 0.01)
    K.block("glass", a, b, T - 0.26, T - 0.2, 0.9, 2.3, 0)
    K.block("sign", a, b, T - 0.3, T - 0.2, 2.3, 2.55, 0.01)


def shop_fascia(K, rng):                     # 1 m of fascia and its stone cornice, stretched to the front
    K.block("sign", -0.5, 0.5, -0.22, T, 2.55, SHOP_TOP, 0.03)
    K.block("stone_dark", -0.5, 0.5, -0.3, T, SHOP_TOP, SHOP_TOP + 0.12)


AWN_W, AWN_OUT = 0.45, 1.5                   # one strip of awning() at the shop's reach; Unreal stretches it to others


def awning_strip(K, rng):                    # top edge at z = 0 against the wall; slot "stripe" takes the strip's colour
    p = math.radians(20)
    L = AWN_OUT / math.cos(p)
    K.cube("stripe", (0, -AWN_OUT / 2, -AWN_OUT * math.tan(p) / 2), (AWN_W, L, 0.05), rot=(p, 0, 0), bevel=0.01)
    K.cube("stripe", (0, -AWN_OUT - 0.02, -AWN_OUT * math.tan(p) - 0.15), (AWN_W, 0.05, 0.3), bevel=0.01)


def awning_arm(K, rng):                      # an iron arm under an awning's end
    p = math.radians(20)
    L = AWN_OUT / math.cos(p)
    K.cube("iron", (0, -AWN_OUT / 2, -AWN_OUT * math.tan(p) / 2 - 0.06), (0.04, L, 0.04), rot=(p, 0, 0), bevel=0)


def produce_crate(K, rng):                   # one of shopfront()'s crates of produce on the pavement, at x = 0
    K.block("wood", -0.42, 0.42, -1.35, -0.65, 0.0, 0.55, 0.025)
    K.block("wood", -0.42, 0.42, -1.2, -0.62, 0.55, 0.85, 0.025)
    for p in range(4):
        ellipsoid("Produce", Vector((-0.24 + (p % 2) * 0.48 * 0.98, -1.18 + (p // 2) * 0.32, 0.6 + (p // 2) * 0.3)),
                  (0.14, 0.13, 0.11), MATS[("terra", "ochre", "plant", "leaf_light")[p % 4]])


def blade_sign(K, rng):                      # cafe_front()'s blade sign on its bracket, at x = 0, heights as the kit's
    K.block("iron", -0.03, 0.03, -1.25, 0.02, 3.95, 4.02, 0)
    K.block("iron", -0.025, 0.025, -0.08, 0.0, 3.0, 4.02, 0)
    K.block("sign", -0.05, 0.05, -1.2, -0.2, 3.05, 3.85, 0.02)


def cafe_table(K, rng):                      # one of cafe_front()'s tables with two chairs, 2.6 m out from the wall
    tx, ty = 0.0, -2.6
    K.cyl("iron", (tx, ty, 0.36), "z", 0.04, 0.72, segs=10)
    K.cyl("iron", (tx, ty, 0.02), "z", 0.22, 0.04, segs=14)
    K.cyl("wood", (tx, ty, 0.75), "z", 0.38, 0.05, segs=20)
    for dx in (-0.62, 0.62):
        cx = tx + dx
        K.block("wood", cx - 0.21, cx + 0.21, ty - 0.21, ty + 0.21, 0.42, 0.47, 0.012)
        K.block("wood", cx + (0.17 if dx > 0 else -0.21), cx + (0.21 if dx > 0 else -0.17), ty - 0.21, ty + 0.21, 0.47, 0.88, 0.01)
        for lx in (-0.17, 0.17):
            for ly in (-0.17, 0.17):
                K.block("wood", cx + lx - 0.02, cx + lx + 0.02, ty + ly - 0.02, ty + ly + 0.02, 0.0, 0.42, 0)
    ellipsoid("Cup", Vector((tx + 0.1, ty - 0.05, 0.82)), (0.045, 0.045, 0.04), MATS["cream"])


def balcony_piece(K, rng):                   # balcony(): z = 0 at the floor; the opening is 1.2 x 2.3 from 0.15 up
    balcony(K, rng, 0.0, 0.0, T, LOOK)


PIECES = {
    "SM_Kit_WinUp": (win_up, (KIT["win_w"], KIT["win_h"])),
    "SM_Kit_WinTop": (win_top, (KIT["win_w"], KIT["top_win_h"])),
    "SM_Kit_WinPlain": (win_plain, (KIT["win_w"], KIT["win_h"])),
    "SM_Kit_WinArch": (win_arch, (KIT["win_w"], 1.75)),
    "SM_Kit_WinCafe": (win_cafe, (2.0, 2.45)),
    "SM_Kit_WinCivic": (win_civic, (1.25, 1.65)),
    "SM_Kit_DoorArch": (door_arch, (KIT["door_w"], 2.5)),
    "SM_Kit_DoorPlain": (door_plain, (KIT["door_w"], KIT["door_h"])),
    "SM_Kit_DoorCivic": (door_civic, (1.5, 2.6)),
    "SM_Kit_Shutters160": (shutters(KIT["win_h"]), None),
    "SM_Kit_Shutters145": (shutters(KIT["top_win_h"]), None),
    "SM_Kit_RoofWedge": (roof_wedge, None),
    "SM_Kit_ShopPier": (shop_pier, None),
    "SM_Kit_ShopPane": (shop_pane, None),
    "SM_Kit_ShopDoor": (shop_door, None),
    "SM_Kit_ShopFascia": (shop_fascia, None),
    "SM_Kit_AwningStrip": (awning_strip, None),
    "SM_Kit_AwningArm": (awning_arm, None),
    "SM_Kit_ProduceCrate": (produce_crate, None),
    "SM_Kit_BladeSign": (blade_sign, None),
    "SM_Kit_CafeTable": (cafe_table, None),
    "SM_Kit_Balcony": (balcony_piece, (1.2, 2.3)),
}


def clear_scene():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)


def one_mesh(name, obs):
    """The kit's objects (with their bevel modifiers) baked and joined into one object."""
    dg = bpy.context.evaluated_depsgraph_get()
    baked = []
    for ob in obs:
        me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
        nb = bpy.data.objects.new(ob.name + "_baked", me)
        nb.matrix_world = ob.matrix_world.copy()
        COL.objects.link(nb)
        baked.append(nb)
    for ob in obs:
        bpy.data.objects.remove(ob, do_unlink=True)
    with bpy.context.temp_override(active_object=baked[0], selected_editable_objects=baked, selected_objects=baked):
        bpy.ops.object.join()
    out = baked[0]
    out.name = name
    out.data.name = name
    return out


def main():
    global COL
    os.makedirs(EXPORT, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    clear_scene()
    COL = bpy.context.scene.collection
    sizes = {}
    for name, (make, opening) in PIECES.items():
        try:
            K = Kit(name)
            before = set(bpy.context.scene.objects)
            make(K, random.Random(36))
            K.flush(MATS)
            ob = one_mesh(name, [o for o in bpy.context.scene.objects if o not in before])   # with blob()s and ellipsoid()s
            for o in bpy.context.scene.objects:
                o.select_set(o == ob)
            bpy.context.view_layer.objects.active = ob
            path = EXPORT + "/" + name + ".glb"
            bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_apply=True)
            bb = [ob.matrix_world @ v.co for v in ob.data.vertices]
            lo = [round(min(p[i] for p in bb), 3) for i in range(3)]
            hi = [round(max(p[i] for p in bb), 3) for i in range(3)]
            sizes[name] = dict(opening=opening, slots=[m.name for m in ob.data.materials], tris=sum(len(p.vertices) - 2 for p in ob.data.polygons), min=lo, max=hi)
            log("%s: %d triangles, slots %s, box %s..%s" % (name, sizes[name]["tris"], sizes[name]["slots"], lo, hi))
            bpy.data.objects.remove(ob, do_unlink=True)
        except Exception:
            log("FAILED %s:\n%s" % (name, traceback.format_exc()))
    json.dump(sizes, open(EXPORT + "/" + NAME + ".json", "w"), indent=1)
    log("DONE")


main()
