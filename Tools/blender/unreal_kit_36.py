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
            make(K, random.Random(36))
            ob = one_mesh(name, K.flush(MATS))
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
