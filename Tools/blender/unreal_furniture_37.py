"""Brix Town - unreal furniture 37: every piece of furniture kits 17 and 18, in each of the four style lines, as one mesh
per piece and line for the Unreal town's interiors (FURN-001, FURN-002, FURN-003, FURN-005; ADR-0026).

Headless, from the repository root:
    blender-launcher.exe -b --factory-startup --python Tools/blender/unreal_furniture_37.py
Quick check: WASTED_ONLY=k17_sofa,k_counter (piece names, comma separated) exports only those.
The launcher (%LOCALAPPDATA%\Microsoft\WindowsApps\blender-launcher.exe; Blender 5.3 is a Store app) returns at once
and prints nothing: the run takes about 20 s, and the log below says what was made ("DONE" at the end).

Builds on furniture_kit_17.py and furniture_kit_18.py (and through them car_kit_09.py, building_kit_07.py, old_town_04.py
and style_test_02.py): both kits' sources are read and run without their main(), the materials and the four lines are
made as the kits make them (fresh(), extra_mats(), lines()), and every piece function is called once per line on a
group at the origin, as the kits' sheets call them. So the meshes are the kits' own pieces; nothing is re-modelled.

Each piece is exported to Art/Export/unreal_furniture_37/<piece>_<line>.glb, one object named SM_<piece>_<line>:
    <piece>  kit 17's pieces as the data writes them (World/rules/home_plans.json "k17:sofa" -> k17_sofa, ...), kit 18's
             by their function names (k_counter, b_bath, ..., o_bust)
    <line>   plain, oldtown, workshop, curve (the lines' keys in kit 17 lines())
Frame: the kits' own. Metres, z up, the piece standing on z = 0 at the origin of its group, its back toward +y and its
front toward -y (the kits lay every piece against a back wall at +y). glTF stores it y up; Unreal converts on import as
it does for the building kit (unreal_kit_36.py), so in Unreal it is centimetres, z up, the back toward Unreal's +y
(the glTF importer mirrors y; check the index's box against the import log, which prints each box in cm).

Colour: every material of the kits is a procedural node tree (painted(), flat(), emissive(), solid()); each face gets
its material's base colour as a linear vertex colour (painted(): the mean of its two colours; flat(): its colour;
emissive() and solid(): their colour), and the mesh carries one material slot, "furniture", so in Unreal it needs only
the vertex-colour matte material /Game/Furniture/M_Furniture (Tools/unreal/import_furniture_kit.py). The noise, bricks,
bump and grime of the node trees are not baked: at third-person distance the pieces read by outline and colour (I7).
Lamp shades, embers and flames are emissive in the kits; here they are their colour only (no glow).
Left out: the kits' lights (light(), lamp_light()) and every other object that is not geometry. Modifiers (the bevels
and subdivisions that round the kit's parts) are applied before export.

Also written: Art/Export/unreal_furniture_37/index.json, for each piece and line its file, its box (min and max in the
kit frame, metres) and its triangle count; and unreal_furniture_37_log.txt.
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
_k18 = _real_open(REPO + "/Tools/blender/furniture_kit_18.py", encoding="utf-8").read()
_k18 = _k18[:_k18.rstrip().rfind("\nmain()")]               # kit 18 (and through it kit 17), without running either
exec(compile(_k18, "furniture_kit_18_helpers", "exec"), globals())

NAME = "unreal_furniture_37"
EXPORT = REPO + "/Art/Export/" + NAME
LOG = EXPORT + "/" + NAME + "_log.txt"
ONLY = tuple(os.environ["WASTED_ONLY"].split(",")) if os.environ.get("WASTED_ONLY") else None


def log(msg):
    line = time.strftime("%H:%M:%S") + " " + msg
    print(line)
    with _real_open(LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")


# ---- each material remembers the colour it is baked to ------------------------------------------------------------
def _remember(m, c):
    m["brix_rgb"] = [float(min(max(v, 0.0), 1.0)) for v in c[:3]]
    return m


_painted, _flat, _emissive, _solid = painted, flat, emissive, solid


def painted(name, c1, c2, *a, **k):
    return _remember(_painted(name, c1, c2, *a, **k), [(c1[i] + c2[i]) / 2 for i in range(3)])


def flat(name, c, *a, **k):
    return _remember(_flat(name, c, *a, **k), c)


def emissive(name, c, strength):
    return _remember(_emissive(name, c, strength), c)


def solid(c, *a, **k):
    return _remember(_solid(c, *a, **k), c)


def colour_of(m):
    """The colour a material is baked to: the one remembered, else its shader's base colour."""
    if m is None:
        return (0.5, 0.5, 0.5)
    if "brix_rgb" in m.keys():
        return tuple(m["brix_rgb"])
    try:
        for n in m.node_tree.nodes:
            if n.type == 'BSDF_PRINCIPLED':
                return tuple(n.inputs["Base Color"].default_value[:3])
    except Exception:
        pass
    return tuple(m.diffuse_color[:3])


# ---- the pieces ---------------------------------------------------------------------------------------------------
def k17_piece(kind):
    def build(P, L, rng):
        make(kind, P, L, rng)
    return build


def all_pieces():
    out = [("k17_" + kind, k17_piece(kind)) for kind in PIECES]          # kit 17: sofa ... rug, extra
    for key, label, fns in KITS:                                          # kit 18: k_counter ... o_bust
        out += [(fn.__name__, fn) for fn in fns]
    return out


GEOMETRY = {'MESH', 'CURVE', 'FONT', 'SURFACE', 'META'}


def bake(name, obs, furniture):
    """The piece's objects, modifiers applied, coloured by their materials and joined into one mesh object."""
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    baked = []
    for ob in obs:
        if ob.type not in GEOMETRY:
            continue
        me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg), preserve_all_data_layers=False, depsgraph=dg)
        if not me.polygons:
            bpy.data.meshes.remove(me)
            continue
        me.transform(ob.matrix_world)
        mats = list(me.materials)
        cols = [colour_of(m) for m in mats] or [(0.5, 0.5, 0.5)]
        attr = me.color_attributes.new("Col", 'FLOAT_COLOR', 'CORNER')
        data = [0.0] * (4 * len(me.loops))
        for p in me.polygons:
            c = cols[min(p.material_index, len(cols) - 1)]
            for li in p.loop_indices:
                data[4 * li:4 * li + 4] = (c[0], c[1], c[2], 1.0)
        attr.data.foreach_set("color", data)
        me.materials.clear()
        me.materials.append(furniture)
        nb = bpy.data.objects.new(name + "_part", me)
        COL.objects.link(nb)
        baked.append(nb)
    if not baked:
        raise RuntimeError("no geometry")
    if len(baked) > 1:
        with bpy.context.temp_override(active_object=baked[0], selected_editable_objects=baked, selected_objects=baked):
            bpy.ops.object.join()
    out = baked[0]
    out.name = name
    out.data.name = name
    ca = out.data.color_attributes
    ca.active_color = ca["Col"]
    try:
        ca.render_color_index = ca.find("Col")
    except Exception:
        pass
    return out


def gltf_options():
    """The glTF exporter's options for vertex colours, by what this Blender's exporter has."""
    have = set(bpy.ops.export_scene.gltf.get_rna_type().properties.keys())
    opts = dict(export_format='GLB', use_selection=True, export_apply=True, export_materials='EXPORT',
                export_texcoords=False, export_normals=True, export_cameras=False, export_lights=False,
                export_animations=False, export_skins=False, export_morph=False)
    if "export_vertex_color" in have:                              # Blender 4.2 and later
        opts["export_vertex_color"] = 'ACTIVE'
    if "export_all_vertex_colors" in have:
        opts["export_all_vertex_colors"] = False
    if "export_active_vertex_color_when_no_material" in have:
        opts["export_active_vertex_color_when_no_material"] = True
    if "export_colors" in have:                                    # older exporters
        opts["export_colors"] = True
    return {k: v for k, v in opts.items() if k in have}


def clear_new(before):
    for ob in list(bpy.context.scene.objects):
        if ob not in before:
            bpy.data.objects.remove(ob, do_unlink=True)
    for coll in (bpy.data.meshes, bpy.data.curves, bpy.data.lights):
        for d in list(coll):
            if d.users == 0:
                coll.remove(d)


def main():
    os.makedirs(EXPORT, exist_ok=True)
    _real_open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    for ob in list(bpy.data.objects):          # the factory scene's cube, camera and light
        bpy.data.objects.remove(ob, do_unlink=True)
    fresh("Furn_Export")                       # kit 17's materials (and the scene whose collection COL is)
    extra_mats()                               # kit 18's
    Ls = lines()
    furniture = bpy.data.materials.new("furniture")
    opts = gltf_options()
    log("glTF options: %s" % opts)
    pieces = all_pieces()
    index, failed = {}, []
    t0 = time.time()
    for pname, build in pieces:
        if ONLY and pname not in ONLY:
            continue
        index[pname] = {}
        for L in Ls:
            stem = "%s_%s" % (pname, L["key"])
            before = set(bpy.context.scene.objects)
            try:
                P = group("G_" + stem, 0.0, 0.0, 0.0)
                build(P, L, random.Random(37))
                ob = bake("SM_" + stem, [o for o in bpy.context.scene.objects if o not in before], furniture)
                for o in bpy.context.scene.objects:
                    o.select_set(o == ob)
                bpy.context.view_layer.objects.active = ob
                bpy.ops.export_scene.gltf(filepath=EXPORT + "/" + stem + ".glb", **opts)
                vs = [v.co for v in ob.data.vertices]
                lo = [round(min(v[i] for v in vs), 3) for i in range(3)]
                hi = [round(max(v[i] for v in vs), 3) for i in range(3)]
                tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
                index[pname][L["key"]] = dict(file=stem + ".glb", mesh="SM_" + stem, min=lo, max=hi, tris=tris)
                log("%s: %d triangles, box %s..%s" % (stem, tris, lo, hi))
            except Exception:
                failed.append(stem)
                log("FAILED %s:\n%s" % (stem, traceback.format_exc()))
            clear_new(before)
    doc = dict(source="Tools/blender/unreal_furniture_37.py",
               frame="metres, z up, standing on z = 0, back toward +y, front toward -y (the kits' frame)",
               lines=[L["key"] for L in Ls], pieces=index)
    with _real_open(EXPORT + "/index.json", "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1)
    n = sum(len(v) for v in index.values())
    big = sorted(((e["tris"], "%s_%s" % (p, k)) for p, v in index.items() for k, e in v.items()), reverse=True)[:8]
    log("%d pieces x %d lines: %d exported, %d failed %s in %.0fs" % (len(index), len(Ls), n, len(failed), failed, time.time() - t0))
    log("largest: " + ", ".join("%s %d" % (s, t) for t, s in big))
    log("DONE")


main()
