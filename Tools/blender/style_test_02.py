"""Wasted - art style test 02: painterly old-quarter back street, arch framing and stronger light.

Run inside Blender (Scripting workspace, Python console):
    exec(compile(open("C:/Projects/Wasted/Tools/blender/style_test_02.py").read(), "style_test_02.py", "exec"))

Builds a small street scene from simple blocks, lights it with a low sun through
haze, saves the .blend and renders one still. Writes a log next to the render.
"""
import bpy, bmesh, math, random, os, time, traceback
from mathutils import Vector

OUT = "C:/Projects/Wasted/Art/StyleTests"
SRC = "C:/Projects/Wasted/Art/Source/StyleTests"
NAME = "style_test_02"
LOG = OUT + "/" + NAME + "_log.txt"

# ---- look parameters (tune here) -------------------------------------------
RES = (1280, 800)
SAMPLES = 160
SUN_DIR = (0.446, 0.594, 0.669)      # direction towards the sun
SUN_ENERGY = 14.0
SUN_COLOR = (1.0, 0.86, 0.68)
SKY_COLOR = (0.42, 0.55, 0.82)
SKY_STRENGTH = 0.35
FOG_DENSITY = 0.022
FOG_ANISO = 0.6
EXPOSURE = 0.3
CAM_LOC = (0.5, -12.0, 1.7)
CAM_TARGET = (-0.1, 8.0, 3.4)
CAM_LENS = 24.0


def log(msg):
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(time.strftime("%H:%M:%S ") + str(msg) + "\n")


def setp(obj, name, value):
    try:
        setattr(obj, name, value)
        return True
    except Exception as e:
        log("skip %s.%s: %s" % (type(obj).__name__, name, e))
        return False


def sset(node, name, value):
    try:
        node.inputs[name].default_value = value
        return True
    except Exception as e:
        log("skip input %s.%s: %s" % (node.bl_idname, name, e))
        return False


def rgba(c, k=1.0):
    return (min(c[0] * k, 1.0), min(c[1] * k, 1.0), min(c[2] * k, 1.0), 1.0)


# ---- materials ---------------------------------------------------------------
def new_mat(name):
    m = bpy.data.materials.new(name)
    try:
        m.use_nodes = True
    except Exception:
        pass
    nt = m.node_tree
    bsdf = None
    for n in nt.nodes:
        if n.type == 'BSDF_PRINCIPLED':
            bsdf = n
    if bsdf is None:
        nt.nodes.clear()
        bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        nt.links.new(bsdf.outputs[0], out.inputs[0])
    return m, nt, bsdf


def painted(name, c1, c2, brick_scale=None, floor=False, noise_scale=1.2,
            rough=0.92, bump=0.35, mortar=None, grime=0.5, grime_h=0.3):
    """Blotchy two-tone colour, optional stone-block pattern, soft bump."""
    m, nt, bsdf = new_mat(name)
    N, L = nt.nodes, nt.links
    sset(bsdf, "Base Color", rgba(c1))
    sset(bsdf, "Roughness", rough)
    sset(bsdf, "Specular IOR Level", 0.25)
    try:
        tc = N.new("ShaderNodeTexCoord")
        noise = N.new("ShaderNodeTexNoise")
        sset(noise, "Scale", noise_scale)
        sset(noise, "Detail", 8.0)
        sset(noise, "Roughness", 0.65)
        sset(noise, "Distortion", 0.8)
        L.new(tc.outputs["Object"], noise.inputs["Vector"])
        ramp = N.new("ShaderNodeValToRGB")
        e = ramp.color_ramp.elements
        e[0].position = 0.22
        e[0].color = rgba(c1)
        e[1].position = 0.66
        e[1].color = rgba(c2)
        try:
            mid = ramp.color_ramp.elements.new(0.45)
            mid.color = rgba(((c1[0] + c2[0]) * 0.48, (c1[1] + c2[1]) * 0.465, (c1[2] + c2[2]) * 0.53))
            hi = ramp.color_ramp.elements.new(0.86)
            hi.color = rgba((c2[0] * 1.12, c2[1] * 1.04, c2[2] * 0.92))
        except Exception as ex:
            log("ramp stops skipped: %s" % ex)
        L.new(noise.outputs[0], ramp.inputs[0])
        color_out = ramp.outputs[0]
        height_out = noise.outputs[0]
        if brick_scale:
            br = N.new("ShaderNodeTexBrick")
            sset(br, "Scale", brick_scale)
            sset(br, "Mortar Size", 0.008)
            sset(br, "Mortar Smooth", 0.3)
            mc = mortar or (c1[0] * 0.7, c1[1] * 0.7, c1[2] * 0.75)
            sset(br, "Mortar", rgba(mc))
            sset(br, "Color2", rgba(((c1[0] + c2[0]) / 2, (c1[1] + c2[1]) / 2, (c1[2] + c2[2]) / 2), 0.9))
            L.new(ramp.outputs[0], br.inputs["Color1"])
            if floor:
                L.new(tc.outputs["Object"], br.inputs["Vector"])
            else:
                sep = N.new("ShaderNodeSeparateXYZ")
                add = N.new("ShaderNodeMath")
                add.operation = 'ADD'
                comb = N.new("ShaderNodeCombineXYZ")
                L.new(tc.outputs["Object"], sep.inputs[0])
                L.new(sep.outputs[0], add.inputs[0])
                L.new(sep.outputs[1], add.inputs[1])
                L.new(add.outputs[0], comb.inputs[0])
                L.new(sep.outputs[2], comb.inputs[1])
                L.new(comb.outputs[0], br.inputs["Vector"])
            color_out = br.outputs[0]
            sub = N.new("ShaderNodeMath")
            sub.operation = 'SUBTRACT'
            L.new(noise.outputs[0], sub.inputs[0])
            L.new(br.outputs[1], sub.inputs[1])
            height_out = sub.outputs[0]
        try:
            sepg = N.new("ShaderNodeSeparateXYZ")
            L.new(tc.outputs["Generated"], sepg.inputs[0])
            gr = N.new("ShaderNodeValToRGB")
            ge = gr.color_ramp.elements
            ge[0].position = 0.0
            ge[0].color = (grime, grime, min(grime * 1.1, 1.0), 1.0)
            ge[1].position = grime_h
            ge[1].color = (1.0, 1.0, 1.0, 1.0)
            L.new(sepg.outputs[2], gr.inputs[0])
            mix = N.new("ShaderNodeMix")
            mix.data_type = 'RGBA'
            mix.blend_type = 'MULTIPLY'
            mix.inputs[0].default_value = 1.0
            L.new(color_out, mix.inputs[6])
            L.new(gr.outputs[0], mix.inputs[7])
            color_out = mix.outputs[2]
        except Exception:
            log("grime skipped on %s:\n%s" % (name, traceback.format_exc()))
        L.new(color_out, bsdf.inputs["Base Color"])
        bp = N.new("ShaderNodeBump")
        sset(bp, "Strength", bump)
        sset(bp, "Distance", 0.08)
        L.new(height_out, bp.inputs["Height"])
        L.new(bp.outputs[0], bsdf.inputs["Normal"])
    except Exception:
        log("material %s partly built:\n%s" % (name, traceback.format_exc()))
    return m


def flat(name, c, rough=0.9, bump=0.2, noise_scale=2.5):
    return painted(name, (c[0] * 0.78, c[1] * 0.78, c[2] * 0.8), (c[0] * 1.12, c[1] * 1.1, c[2] * 1.05),
                   noise_scale=noise_scale, rough=rough, bump=bump)


def emissive(name, c, strength):
    m, nt, bsdf = new_mat(name)
    sset(bsdf, "Base Color", rgba(c))
    if not sset(bsdf, "Emission Color", rgba(c)):
        sset(bsdf, "Emission", rgba(c))
    sset(bsdf, "Emission Strength", strength)
    return m


def fog_material(name, density, aniso):
    m = bpy.data.materials.new(name)
    try:
        m.use_nodes = True
    except Exception:
        pass
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    vol = nt.nodes.new("ShaderNodeVolumeScatter")
    sset(vol, "Color", (0.90, 0.93, 1.0, 1.0))
    sset(vol, "Density", density)
    sset(vol, "Anisotropy", aniso)
    nt.links.new(vol.outputs[0], out.inputs["Volume"])
    return m


# ---- geometry helpers --------------------------------------------------------
COL = None


def add_obj(name, bm, mat, loc=(0, 0, 0), rot=(0, 0, 0), bevel=0.0):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    ob.location = loc
    ob.rotation_euler = rot
    if mat is not None:
        me.materials.append(mat)
    COL.objects.link(ob)
    if bevel > 0:
        try:
            md = ob.modifiers.new("Bevel", 'BEVEL')
            md.width = bevel
            md.segments = 2
        except Exception as e:
            log("bevel skipped: %s" % e)
    return ob


def box(name, x0, x1, y0, y1, z0, z1, mat, bevel=0.04, rot=(0, 0, 0)):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=(x1 - x0, y1 - y0, z1 - z0), verts=bm.verts)
    return add_obj(name, bm, mat, loc=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), rot=rot, bevel=bevel)


def cyl(name, x, y, z0, r, h, mat, segs=18, r_top=None):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segs,
                          radius1=r, radius2=(r if r_top is None else r_top), depth=h)
    return add_obj(name, bm, mat, loc=(x, y, z0 + h / 2), bevel=0.015)


def cyl_y(name, x, y, z, r, length, mat=None, segs=48):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segs, radius1=r, radius2=r, depth=length)
    return add_obj(name, bm, mat, loc=(x, y, z), rot=(math.pi / 2, 0, 0))


def cut(ob, cutter):
    try:
        md = ob.modifiers.new("Cut", 'BOOLEAN')
        md.operation = 'DIFFERENCE'
        md.object = cutter
        cutter.hide_render = True
        setp(cutter, "display_type", 'WIRE')
    except Exception:
        log("boolean skipped:\n" + traceback.format_exc())


def arch_wall(name, x0, x1, y0, y1, top, open_x0, open_x1, spring, mat, trim):
    """Wall with an arched opening, plus a ring of trim blocks round the arch."""
    wall = box(name, x0, x1, y0, y1, 0, top, mat, bevel=0)
    cx = (open_x0 + open_x1) / 2
    r = (open_x1 - open_x0) / 2
    cut(wall, box(name + "_CutBox", open_x0, open_x1, y0 - 0.6, y1 + 0.6, -1, spring, None, bevel=0))
    cut(wall, cyl_y(name + "_CutArc", cx, (y0 + y1) / 2, spring, r, (y1 - y0) + 1.2))
    n = 13
    for i in range(n):
        th = math.pi * (i + 0.5) / n
        px = cx + (r + 0.2) * math.cos(th)
        pz = spring + (r + 0.2) * math.sin(th)
        half = math.pi * (r + 0.2) / n / 2 * 0.94
        box(name + "_Trim", px - half, px + half, y0 - 0.07, y1 + 0.07, pz - 0.2, pz + 0.2,
            trim if i % 2 else mat, bevel=0.03, rot=(0, -(th + math.pi / 2), 0))
    return wall


def blob(name, x, y, z, r, mat, squash=0.8):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=r)
    bmesh.ops.scale(bm, vec=(1.0, 1.0, squash), verts=bm.verts)
    for v in bm.verts:
        v.co += Vector((random.uniform(-1, 1), random.uniform(-1, 1), random.uniform(-1, 1))) * r * 0.09
    return add_obj(name, bm, mat, loc=(x, y, z))


# ---- scene -------------------------------------------------------------------
def build():
    global COL
    random.seed(7)
    scene = bpy.data.scenes.new("StyleTest01")
    try:
        bpy.context.window.scene = scene
    except Exception as e:
        log("could not switch window scene: %s" % e)
    COL = scene.collection

    # palette, taken from the reference: grey-violet stone, cream light, dull accents
    stone = painted("M_Stone", (0.27, 0.26, 0.30), (0.52, 0.48, 0.43), brick_scale=0.5, bump=0.35, noise_scale=0.9)
    stone_dark = painted("M_StoneDark", (0.18, 0.18, 0.22), (0.37, 0.34, 0.32), brick_scale=0.6, bump=0.35, noise_scale=0.9)
    plaster = painted("M_Plaster", (0.46, 0.43, 0.40), (0.68, 0.63, 0.55), noise_scale=0.7, bump=0.25)
    plaster_b = painted("M_PlasterWarm", (0.50, 0.40, 0.33), (0.70, 0.58, 0.46), noise_scale=0.7, bump=0.25)
    paving = painted("M_Paving", (0.22, 0.21, 0.25), (0.46, 0.42, 0.37), brick_scale=0.6, floor=True, bump=0.5, noise_scale=0.5)
    wood = flat("M_Wood", (0.30, 0.20, 0.13))
    wood_red = flat("M_WoodRed", (0.42, 0.18, 0.14))
    teal = flat("M_Teal", (0.20, 0.36, 0.37))
    ochre = flat("M_Ochre", (0.52, 0.40, 0.20))
    terracotta = flat("M_Terracotta", (0.44, 0.24, 0.18))
    cream = flat("M_Cream", (0.66, 0.61, 0.50))
    cloth_blue = flat("M_ClothBlue", (0.30, 0.38, 0.52))
    glass = flat("M_Glass", (0.07, 0.09, 0.12), rough=0.25, bump=0.0)
    leaf = flat("M_Leaf", (0.24, 0.36, 0.20), noise_scale=4.0)
    leaf_light = flat("M_LeafLight", (0.40, 0.48, 0.24), noise_scale=4.0)
    iron = flat("M_Iron", (0.10, 0.10, 0.11), rough=0.6)
    coat = flat("M_Coat", (0.22, 0.18, 0.17))
    skin = flat("M_Skin", (0.55, 0.40, 0.32))
    lamp = emissive("M_Lamp", (1.0, 0.55, 0.22), 6.0)
    accents = [ochre, terracotta, teal, cream, wood, wood_red]

    # ground
    box("Ground", -9, 20, -14, 70, -0.3, 0, paving, bevel=0)

    # left building: stone base, plaster upper floors
    box("L_Base", -7, -3, -14, 30, 0, 5, stone)
    box("L_Upper", -7, -3.05, -14, 30, 5, 14, plaster)
    box("L_Cornice", -3.2, -2.75, -14, 30, 5.0, 5.35, stone_dark)
    box("L_Roof", -7.2, -2.8, -14, 30, 14, 14.4, stone_dark)
    for y in (-6, -1, 4, 9, 14, 19, 24):
        box("L_Pilaster", -3.05, -2.72, y - 0.3, y + 0.3, 0, 5, stone_dark)
    for y in (1.6, 6.5, 11.5, 16.5, 21.5):
        for z in (6.4, 9.9):
            box("L_WinFrame", -3.12, -2.98, y - 0.75, y + 0.75, z - 0.15, z + 2.15, stone_dark, bevel=0.02)
            box("L_WinGlass", -3.10, -2.95, y - 0.6, y + 0.6, z, z + 2.0, glass, bevel=0)
            box("L_Shutter", -3.06, -2.93, y - 1.25, y - 0.72, z, z + 2.0, teal if (int(y) + int(z)) % 2 else wood_red, bevel=0.01)
            box("L_Sill", -3.1, -2.8, y - 0.85, y + 0.85, z - 0.3, z - 0.15, stone_dark, bevel=0.02)
    # door with steps and a lantern
    box("L_DoorFrame", -3.1, -2.9, 0.75, 2.85, 0, 3.4, stone_dark)
    box("L_Door", -3.05, -2.86, 1.0, 2.6, 0, 3.1, wood_red, bevel=0.02)
    box("L_Step1", -3, -2.2, 0.6, 3.0, 0, 0.18, stone_dark)
    box("L_Step2", -3, -2.5, 0.8, 2.8, 0.18, 0.36, stone_dark)
    box("L_LampArm", -3.0, -2.45, 0.38, 0.44, 3.45, 3.5, iron, bevel=0)
    box("L_Lamp", -2.62, -2.42, 0.3, 0.52, 3.1, 3.45, lamp, bevel=0.02)

    # covered passage in the foreground (dark frame, as in the reference)
    box("F_Slab", -3.1, 3.6, -14, -3.2, 5.6, 6.6, stone_dark)
    for y in (-11.5, -9.0, -6.5):
        box("F_Beam", -3.0, 3.5, y - 0.15, y + 0.15, 5.25, 5.6, wood)
    arch_wall("F_Arch", -3.0, 3.5, -4.0, -3.2, 6.6, -1.9, 2.4, 3.2, stone, stone_dark)
    box("F_Lamp", -2.95, -2.75, -8.1, -7.9, 3.0, 3.3, lamp, bevel=0.02)

    # right side: tall block, low courtyard wall (sun comes over it), tall block
    box("R_A_Base", 3.5, 7, -14, 2, 0, 5, stone)
    box("R_A_Upper", 3.55, 7, -14, 2, 5, 12, plaster_b)
    box("R_Wall", 3.5, 4.2, 2, 11, 0, 4.2, stone)
    box("R_WallCap", 3.4, 4.3, 2, 11, 4.2, 4.42, stone_dark)
    box("R_Gate", 3.42, 3.52, 5.4, 7.0, 0, 2.7, wood, bevel=0.02)
    box("R_GateFrame", 3.44, 3.6, 5.2, 7.2, 0, 3.0, stone_dark)
    box("R_C_Base", 3.5, 7, 11, 30, 0, 5, stone)
    box("R_C_Upper", 3.55, 7, 11, 30, 5, 15, plaster)
    box("R_C_Cornice", 3.3, 3.7, 11, 30, 5.0, 5.35, stone_dark)
    for y in (18.5, 23.5, 28.0):
        for z in (6.4, 10.2):
            box("R_WinGlass", 3.45, 3.6, y - 0.6, y + 0.6, z, z + 2.0, glass, bevel=0)
            box("R_Sill", 3.3, 3.6, y - 0.85, y + 0.85, z - 0.3, z - 0.15, stone_dark, bevel=0.02)
    # awning and planters on the courtyard wall
    box("R_Awning", 2.5, 3.5, 7.6, 10.2, 2.9, 2.98, terracotta, bevel=0, rot=(0, math.radians(-18), 0))
    for y in (2.7, 4.3, 8.8):
        box("R_Planter", 3.55, 4.15, y - 0.45, y + 0.45, 4.42, 4.75, terracotta, bevel=0.03)
        blob("R_Plant", 3.85, y, 5.0, 0.42, leaf_light if y > 4 else leaf)

    # tree in the courtyard
    cyl("Tree_Trunk", 6.0, 9.5, 0, 0.22, 6.0, wood, r_top=0.14)
    for (dx, dy, dz, r, mt) in ((0, 0, 7.2, 1.7, leaf), (1.0, 0.6, 6.6, 1.3, leaf), (-0.9, 0.5, 6.7, 1.2, leaf_light),
                                (0.3, -1.0, 6.5, 1.2, leaf), (0.2, 0.3, 8.3, 1.1, leaf_light), (-0.6, -0.7, 7.6, 0.9, leaf_light)):
        blob("Tree_Leaf", 6.0 + dx, 9.5 + dy, dz, r, mt)

    # bridge between the buildings
    box("Bridge", -3.05, 3.55, 14, 16, 6.0, 8.6, plaster_b)
    box("Bridge_Roof", -3.05, 3.55, 13.85, 16.15, 8.6, 8.85, stone_dark)
    for x in (-1.6, 0.25, 2.1):
        box("Bridge_Win", x - 0.45, x + 0.45, 13.95, 14.1, 6.7, 8.0, glass, bevel=0)
    arch_wall("Bridge_Arch", -3.05, 3.55, 14.0, 14.6, 6.0, -2.2, 2.7, 3.4, stone, stone_dark)

    # distance: silhouettes that fade into the haze
    box("Far_R", 12, 17, -6, 22, 0, 9, plaster)
    for (x0, x1, y0, h) in ((-9, -2, 44, 16), (-1.5, 2.5, 52, 22), (3, 9, 46, 13), (9.5, 15, 56, 19), (-16, -10, 50, 12)):
        box("Far", x0, x1, y0, y0 + 6, 0, h, plaster)

    # laundry line in the sun
    box("Line", -3.0, 3.35, 2.99, 3.01, 5.2, 5.215, iron, bevel=0)
    cyl("Line_Pole", 3.33, 3.0, 0, 0.05, 5.5, iron, segs=10)
    x = -2.3
    for i, mt in enumerate((cream, terracotta, cloth_blue, cream, ochre, teal)):
        w = random.uniform(0.6, 0.95)
        h = random.uniform(0.7, 1.5)
        box("Cloth", x, x + w, 2.99, 3.01, 5.2 - h, 5.2, mt, bevel=0, rot=(0, 0, math.radians(random.uniform(-6, 6))))
        x += w + random.uniform(0.12, 0.3)

    # crates, barrels and planks in the sun patch
    base = []
    for i in range(7):
        s = random.uniform(0.45, 0.85)
        cx = -1.9 + random.uniform(0, 2.0)
        cy = 1.6 + random.uniform(0, 2.2)
        base.append((cx, cy, s))
        box("Crate", cx - s / 2, cx + s / 2, cy - s / 2, cy + s / 2, 0, s * random.uniform(0.7, 1.0),
            random.choice(accents), bevel=0.025, rot=(0, 0, random.uniform(-0.5, 0.5)))
    for (cx, cy, s) in base[:4]:
        t = s * random.uniform(0.55, 0.8)
        box("CrateTop", cx - t / 2, cx + t / 2, cy - t / 2, cy + t / 2, s * 0.85, s * 0.85 + t * 0.8,
            random.choice(accents), bevel=0.025, rot=(0.03, 0.02, random.uniform(-0.7, 0.7)))
    cyl("Barrel", -2.35, 4.4, 0, 0.34, 0.9, wood)
    cyl("Barrel", -2.3, 5.2, 0, 0.3, 0.8, wood_red)
    for i in range(4):
        box("Plank", -1.6, 0.3, -2.6 + i * 0.26, -2.42 + i * 0.26, 0.0, 0.05, wood, bevel=0.01,
            rot=(0, 0, math.radians(random.uniform(8, 20))))
    # dark foreground props
    box("F_Crate", 2.0, 2.9, -6.2, -5.3, 0, 0.8, wood)
    box("F_Crate", 2.2, 2.8, -6.0, -5.4, 0.8, 1.35, wood_red, rot=(0, 0, 0.3))
    box("F_Bench", -2.9, -2.3, -7.6, -5.6, 0.38, 0.48, wood)
    box("F_BenchLeg", -2.85, -2.35, -7.5, -7.35, 0, 0.38, stone_dark)
    box("F_BenchLeg", -2.85, -2.35, -5.85, -5.7, 0, 0.38, stone_dark)

    # a figure for scale
    cyl("Fig_Leg", 0.38, 6.2, 0, 0.085, 0.85, coat, segs=10)
    cyl("Fig_Leg", 0.62, 6.2, 0, 0.085, 0.85, coat, segs=10)
    cyl("Fig_Body", 0.5, 6.2, 0.82, 0.18, 0.68, coat, segs=12, r_top=0.15)
    blob("Fig_Head", 0.5, 6.2, 1.66, 0.125, skin, squash=1.1)

    # haze
    box("Fog", -12, 22, -16, 72, -0.2, 14, fog_material("M_Fog", FOG_DENSITY, FOG_ANISO), bevel=0)

    # lights
    sun = bpy.data.lights.new("Sun", 'SUN')
    sun.energy = SUN_ENERGY
    sun.color = SUN_COLOR
    setp(sun, "angle", math.radians(2.5))
    so = bpy.data.objects.new("Sun", sun)
    so.rotation_euler = Vector(SUN_DIR).normalized().to_track_quat('Z', 'Y').to_euler()
    so.location = (10, 6, 20)
    COL.objects.link(so)

    pl = bpy.data.lights.new("LanternLight", 'POINT')
    pl.energy = 120
    pl.color = (1.0, 0.62, 0.32)
    setp(pl, "shadow_soft_size", 0.15)
    po = bpy.data.objects.new("LanternLight", pl)
    po.location = (-2.3, 0.41, 3.2)
    COL.objects.link(po)
    pl2 = bpy.data.lights.new("PassageLight", 'POINT')
    pl2.energy = 45
    pl2.color = (1.0, 0.6, 0.3)
    setp(pl2, "shadow_soft_size", 0.15)
    po2 = bpy.data.objects.new("PassageLight", pl2)
    po2.location = (-2.5, -8.0, 3.1)
    COL.objects.link(po2)

    # sky
    world = bpy.data.worlds.new("StyleWorld")
    scene.world = world
    try:
        world.use_nodes = True
    except Exception:
        pass
    wt = world.node_tree
    wt.nodes.clear()
    bg = wt.nodes.new("ShaderNodeBackground")
    wo = wt.nodes.new("ShaderNodeOutputWorld")
    bg.inputs[0].default_value = rgba(SKY_COLOR)
    bg.inputs[1].default_value = SKY_STRENGTH
    wt.links.new(bg.outputs[0], wo.inputs["Surface"])

    # camera
    cam = bpy.data.cameras.new("Cam")
    cam.lens = CAM_LENS
    cam.clip_end = 300
    co = bpy.data.objects.new("Cam", cam)
    co.location = CAM_LOC
    co.rotation_euler = (Vector(CAM_TARGET) - Vector(CAM_LOC)).to_track_quat('-Z', 'Y').to_euler()
    COL.objects.link(co)
    scene.camera = co
    return scene


def configure(scene):
    r = scene.render
    if not setp(r, "engine", 'CYCLES'):
        if not setp(r, "engine", 'BLENDER_EEVEE'):
            setp(r, "engine", 'BLENDER_EEVEE_NEXT')
    log("engine: " + r.engine)
    c = getattr(scene, "cycles", None)
    if c is not None:
        setp(c, "device", 'CPU')
        setp(c, "samples", SAMPLES)
        setp(c, "use_adaptive_sampling", True)
        setp(c, "adaptive_threshold", 0.03)
        setp(c, "use_denoising", True)
        setp(c, "max_bounces", 6)
        setp(c, "diffuse_bounces", 4)
        setp(c, "glossy_bounces", 2)
        setp(c, "volume_bounces", 1)
    r.resolution_x, r.resolution_y = RES
    r.resolution_percentage = 100
    setp(r.image_settings, "media_type", 'IMAGE')
    setp(r.image_settings, "file_format", 'PNG')
    setp(r.image_settings, "color_mode", 'RGB')
    r.filepath = OUT + "/" + NAME + "_raw.png"
    vs = scene.view_settings
    setp(vs, "view_transform", 'AgX')
    setp(vs, "look", 'AgX - Medium High Contrast')
    setp(vs, "exposure", EXPOSURE)


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(SRC, exist_ok=True)
    open(LOG, "w").close()
    log("Blender " + bpy.app.version_string)
    try:
        t0 = time.time()
        scene = build()
        configure(scene)
        log("scene built: %d objects in %.1fs" % (len(scene.objects), time.time() - t0))
        try:
            bpy.ops.wm.save_as_mainfile(filepath=SRC + "/" + NAME + ".blend", check_existing=False)
            log("saved .blend")
        except Exception:
            log("save failed:\n" + traceback.format_exc())
        t1 = time.time()
        bpy.ops.render.render(write_still=True, scene=scene.name)
        log("render finished in %.1fs" % (time.time() - t1))
        log("DONE")
    except Exception:
        log("FAILED:\n" + traceback.format_exc())


main()
