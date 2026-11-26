"""Brix Town - sky 52: the sky and its clouds, for the references (the owner, 2026-11-26: "move to sky and cloud system").

Loaded by Tools/blender/hours_50.py, which lights a place for each setting; with WASTED_PLACE=sky it renders Market Square
looking up over the roofs to the clock tower, so most of the picture is sky. What is in the sky is what the game's sky
has (ADR-0014, ADR-0023; numbers from World/rules/sky.json):
  the sky      blue overhead (zenith) fading to the pale haze at the horizon by day; at dusk warm towards the sun and
               dark blue overhead; at night the night's zenith and horizon colours with the town's warm glow low down
  clouds       soft heaps on one layer about 1.2 km up, flat underneath and rounded on top, lit by the sun and shaded
               underneath; how much of the sky they cover follows the weather (clouds.cover); made in 3D here, as
               ADR-0014 expected of the Unreal build
  night        stars (night.stars), the Milky Way (night.milky_way), the moon's disc (night.moon.size)
No planet: the owner, 2026-11-26, reviewing the sky: "get rid of the planet in the sky".
"""
import bpy, bmesh, math, random
from mathutils import Vector, Matrix

SKY = dict(
    zenith=(0.07, 0.42, 1.0), haze=(0.78, 0.82, 0.86),                      # sky.json zenith; the haze at the horizon by day
    dusk_low=(1.0, 0.55, 0.30), dusk_high=(0.10, 0.13, 0.32),
    night_zenith=(0.0025, 0.0045, 0.014), night_horizon=(0.012, 0.016, 0.028), city_glow=(0.022, 0.015, 0.009),
    grey_low=(0.66, 0.68, 0.71), grey_high=(0.50, 0.53, 0.58),             # a covered sky
    cover=dict(clear=0.36, cloud=0.62, rain=0.97, fog=0.55, snow=0.92),     # sky.json clouds.cover
    cloud_base=1150.0, cloud_depth=420.0, cloud_size=7000.0, cloud_feature=650.0, cloud_density=0.02,
    cloud_thr=(0.74, 0.36),                                                 # the noise's middle is about 0.66 (big + 0.35 small)
    stars=0.2, star_strength=1.0, milky_way=0.12, milky_width=0.13,         # sky.json night
    moon_size=0.016,
)


def _node(nt, kind, **inputs):
    n = nt.nodes.new(kind)
    for k, v in inputs.items():
        n.inputs[k].default_value = v
    return n


def sky_world(scene, daylight, dusk, cover, night_moon, sun_dir, strength):
    """The world's colour by direction: horizon to zenith, warm towards a low sun at dusk, the night's colours with the
    town's glow; plus stars and the Milky Way by night (brighter on a new moon). daylight 0..1, dusk 0..1, cover 0..1."""
    w = bpy.data.worlds.new("SkyWorld")
    scene.world = w
    w.use_nodes = True
    nt = w.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputWorld")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(tc.outputs["Generated"], sep.inputs[0])
    up = _node(nt, "ShaderNodeMapRange")                                    # 0 at the horizon, 1 overhead (eased)
    up.inputs["From Min"].default_value, up.inputs["From Max"].default_value = 0.0, 0.75
    nt.links.new(sep.outputs["Z"], up.inputs["Value"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    nt.links.new(up.outputs["Result"], ramp.inputs[0])
    e = ramp.color_ramp.elements

    def mix(a, b, t):
        return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))
    if daylight > 0.05:
        low = mix(SKY["haze"], SKY["dusk_low"], dusk)
        high = mix(SKY["zenith"], SKY["dusk_high"], dusk)
    else:
        low = tuple(SKY["night_horizon"][i] + SKY["city_glow"][i] for i in range(3))
        high = SKY["night_zenith"]
    low, high = mix(low, SKY["grey_low"] if daylight > 0.05 else low, cover), mix(high, SKY["grey_high"] if daylight > 0.05 else high, cover)
    e[0].position, e[0].color = 0.0, (*low, 1.0)
    e[1].position, e[1].color = 1.0, (*high, 1.0)
    bg = _node(nt, "ShaderNodeBackground", Strength=strength)
    nt.links.new(ramp.outputs[0], bg.inputs[0])
    shader = bg.outputs[0]
    if daylight <= 0.05:                                                    # the stars and the Milky Way
        vo = _node(nt, "ShaderNodeTexVoronoi", Scale=520.0)
        nt.links.new(tc.outputs["Generated"], vo.inputs["Vector"])
        lt = nt.nodes.new("ShaderNodeMath")
        lt.operation = 'LESS_THAN'
        lt.inputs[1].default_value = 0.03 + 0.05 * SKY["stars"]
        nt.links.new(vo.outputs["Distance"], lt.inputs[0])
        # the Milky Way: a band across the sky, mottled
        band_axis = Vector((0.3, -0.5, 0.81)).normalized()
        dot = nt.nodes.new("ShaderNodeVectorMath")
        dot.operation = 'DOT_PRODUCT'
        dot.inputs[1].default_value = band_axis
        nt.links.new(tc.outputs["Generated"], dot.inputs[0])
        ab = nt.nodes.new("ShaderNodeMath")
        ab.operation = 'ABSOLUTE'
        nt.links.new(dot.outputs["Value"], ab.inputs[0])
        band = _node(nt, "ShaderNodeMapRange")
        band.inputs["From Min"].default_value, band.inputs["From Max"].default_value = 0.0, SKY["milky_width"]
        band.inputs["To Min"].default_value, band.inputs["To Max"].default_value = 1.0, 0.0
        nt.links.new(ab.outputs["Value"], band.inputs["Value"])
        dust = _node(nt, "ShaderNodeTexNoise", Scale=14.0, Detail=8.0, Roughness=0.7)
        nt.links.new(tc.outputs["Generated"], dust.inputs["Vector"])
        mw = nt.nodes.new("ShaderNodeMath")
        mw.operation = 'MULTIPLY'
        nt.links.new(band.outputs["Result"], mw.inputs[0])
        nt.links.new(dust.outputs["Fac"], mw.inputs[1])
        dim = 1.0 - 0.45 * night_moon                                       # night.moon.dims_stars
        k = (1.0 - cover) * dim
        st = _node(nt, "ShaderNodeEmission", Color=(1.0, 0.97, 0.92, 1.0))
        stk = nt.nodes.new("ShaderNodeMath")
        stk.operation = 'MULTIPLY'
        stk.inputs[1].default_value = 2.4 * SKY["star_strength"] * k
        nt.links.new(lt.outputs[0], stk.inputs[0])
        nt.links.new(stk.outputs[0], st.inputs["Strength"])
        mwe = _node(nt, "ShaderNodeEmission", Color=(0.75, 0.78, 0.9, 1.0))
        mwk = nt.nodes.new("ShaderNodeMath")
        mwk.operation = 'MULTIPLY'
        mwk.inputs[1].default_value = 0.09 * SKY["milky_way"] / 0.12 * k
        nt.links.new(mw.outputs[0], mwk.inputs[0])
        nt.links.new(mwk.outputs[0], mwe.inputs["Strength"])
        a1 = nt.nodes.new("ShaderNodeAddShader")
        nt.links.new(shader, a1.inputs[0])
        nt.links.new(st.outputs[0], a1.inputs[1])
        a2 = nt.nodes.new("ShaderNodeAddShader")
        nt.links.new(a1.outputs[0], a2.inputs[0])
        nt.links.new(mwe.outputs[0], a2.inputs[1])
        shader = a2.outputs[0]
    nt.links.new(shader, out.inputs["Surface"])
    return w


def clouds(scene, cover, centre):
    """The cloud layer: a slab of volume about 1.2 km up whose density is noise (big heaps and smaller lumps) above a
    threshold set by the cover, thinning to nothing at the top so the heaps are rounded and flat underneath."""
    for o in [o for o in scene.objects if o.name.startswith("CloudLayer")]:
        bpy.data.objects.remove(o, do_unlink=True)
    if cover <= 0.0:
        return None
    S, base, depth = SKY["cloud_size"], SKY["cloud_base"], SKY["cloud_depth"]
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    me = bpy.data.meshes.new("CloudLayer")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("CloudLayer", me)
    ob.location = (centre.x, centre.y, base + depth / 2)
    ob.scale = (S, S, depth)
    m = bpy.data.materials.new("M_Clouds")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    scale = nt.nodes.new("ShaderNodeVectorMath")
    scale.operation = 'SCALE'
    scale.inputs["Scale"].default_value = 1.0 / SKY["cloud_feature"]
    nt.links.new(geo.outputs["Position"], scale.inputs[0])
    big = _node(nt, "ShaderNodeTexNoise", Scale=1.0, Detail=3.0, Roughness=0.5)
    small = _node(nt, "ShaderNodeTexNoise", Scale=4.0, Detail=4.0, Roughness=0.6)
    nt.links.new(scale.outputs[0], big.inputs["Vector"])
    nt.links.new(scale.outputs[0], small.inputs["Vector"])
    add = nt.nodes.new("ShaderNodeMath")
    add.operation = 'MULTIPLY_ADD'                                          # big + 0.35 small
    nt.links.new(small.outputs["Fac"], add.inputs[0])
    add.inputs[1].default_value = 0.35
    nt.links.new(big.outputs["Fac"], add.inputs[2])
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sep.inputs[0])
    h = _node(nt, "ShaderNodeMapRange")                                     # 0 at the base, 1 at the top
    h.inputs["From Min"].default_value, h.inputs["From Max"].default_value = base, base + depth
    nt.links.new(sep.outputs["Z"], h.inputs["Value"])
    top = nt.nodes.new("ShaderNodeMath")
    top.operation = 'MULTIPLY'                                              # higher up a lump must be thicker to be cloud
    top.inputs[1].default_value = 0.42
    nt.links.new(h.outputs["Result"], top.inputs[0])
    thr = SKY["cloud_thr"][0] - SKY["cloud_thr"][1] * cover                # the cover: how much of the noise is cloud
    sub = nt.nodes.new("ShaderNodeMath")
    sub.operation = 'SUBTRACT'
    nt.links.new(add.outputs[0], sub.inputs[0])
    sub2 = nt.nodes.new("ShaderNodeMath")
    sub2.operation = 'ADD'
    sub2.inputs[1].default_value = thr
    nt.links.new(top.outputs[0], sub2.inputs[0])
    nt.links.new(sub2.outputs[0], sub.inputs[1])
    soft = _node(nt, "ShaderNodeMapRange")                                  # soft edges (clouds.soft)
    soft.inputs["From Min"].default_value, soft.inputs["From Max"].default_value = 0.0, 0.09
    soft.inputs["To Min"].default_value, soft.inputs["To Max"].default_value = 0.0, SKY["cloud_density"]
    nt.links.new(sub.outputs[0], soft.inputs["Value"])
    vol = nt.nodes.new("ShaderNodeVolumePrincipled")
    vol.inputs["Color"].default_value = (1.0, 1.0, 1.0, 1.0)
    vol.inputs["Anisotropy"].default_value = 0.35
    nt.links.new(soft.outputs["Result"], vol.inputs["Density"])
    nt.links.new(vol.outputs[0], out.inputs["Volume"])
    me.materials.append(m)
    scene.collection.objects.link(ob)
    return ob


def moon_disc(scene, toward, at):
    """The moon's disc far off along its light (night.moon.size), lit flat: seen, it gives no light of its own."""
    for o in [o for o in scene.objects if o.name.startswith("SkyMoon")]:
        bpy.data.objects.remove(o, do_unlink=True)
    d = 3000.0
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=32, v_segments=16, radius=d * SKY["moon_size"])
    me = bpy.data.meshes.new("SkyMoon")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("SkyMoon", me)
    ob.location = at + toward.normalized() * d
    m = bpy.data.materials.new("M_SkyMoon")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = _node(nt, "ShaderNodeEmission", Color=(0.92, 0.93, 0.98, 1.0), Strength=3.0)
    sp = _node(nt, "ShaderNodeTexNoise", Scale=3.0, Detail=4.0)            # its seas, faintly
    mul = nt.nodes.new("ShaderNodeMixRGB")
    mul.blend_type = 'MULTIPLY'
    mul.inputs[0].default_value = 0.35
    mul.inputs[1].default_value = (0.92, 0.93, 0.98, 1.0)
    nt.links.new(sp.outputs["Color"], mul.inputs[2])
    nt.links.new(mul.outputs[0], em.inputs["Color"])
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    me.materials.append(m)
    for k in ("visible_diffuse", "visible_glossy", "visible_shadow", "visible_transmission", "visible_volume_scatter"):
        try:
            setattr(ob, k, False)
        except Exception:
            pass
    scene.collection.objects.link(ob)
    return ob
