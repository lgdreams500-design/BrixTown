"""Brix Town - hours 50: every reference place at every hour (the owner, 2026-11-26, reviewing the references: a reference
is not one setting; the alley, Market Square and the street, a home's room, a hallway, a public room (the bakery) and a
big hall (the furniture outlet) each at morning, midday, dusk and night, the night with the lamps on, and off under a
full and a new moon, and in cloud, rain, fog and snow; Market Square's midday shade too dark, as if the bounced light
were missing).

Headless, one place a run (each place is built by the script it always was, then lit for each hour):
    WASTED_PLACE=<place> blender-launcher.exe -b --factory-startup --python C:/Projects/Wasted/Tools/blender/hours_50.py
    places: alley, square, street (Tools/blender/old_town_04.py, building_kit_07.py), rooms, bakery, outlet
    (furniture_kit_18.py's rooms, baker_look_47.py's Stone Oven, furniture_kit_17.py's showroom)
    WASTED_HOURS=Morning,Dusk  some hours only;  WASTED_ONLY=kitchen,shop  some rooms only;
    WASTED_SAMPLES (64), WASTED_SCALE (100)
Renders go to Art/StyleTests/hours_50/<Place>_<Hour>.jpg (rooms: Rooms_<Kind>_<Hour>.jpg), 1280 x 800.

The hours (HOURS below). The sun keeps each place's own bearing (the side its light comes from in the approved image)
and changes height, strength and colour; the sky's light changes with it. Midday has the sky's light 2.6 times the
morning's and six diffuse bounces everywhere, so shade is lit by the sky and the sunlit ground as the eye expects (the
square's approved midday had a weak sky and four bounces: its shade was near black). The electric lamps and lit windows
are as the approved images have them by day (the alley's lanterns lit in its shade, the rooms' lamps on, the bakery's
low), on at dusk and on the "Lamps" nights, off on the "Dark" nights; homes' windows are lit from dusk, shops' always;
fires (the bakery's oven, fireplaces, candles) burn at every hour. A full moon is a faint cool light from high up and a lighter night sky; a new moon none.
The camera's exposure opens at dusk; at night it is set as the game's eye sets it (ADR-0055): to a mean brightness for
each kind of night, at most EYE_MAX stops open, measured on a quick small render.

The rooms of furniture_kit_17/18 were built with three walls and open towards the camera; here they get their fourth
wall behind the camera, so the only light from outside comes in at the window.
"""
import bpy, bmesh, os, math, random, time, traceback
from mathutils import Matrix, Vector

B = "C:/Projects/Wasted/Tools/blender/"
PLACE = os.environ.get("WASTED_PLACE", "alley")
SOURCE = {"alley": "old_town_04.py", "square": "old_town_04.py", "street": "building_kit_07.py",
          "rooms": "baker_look_47.py", "bakery": "baker_look_47.py", "outlet": "baker_look_47.py", "sky": "old_town_04.py"}[PLACE]
_s = open(B + SOURCE, encoding="utf-8").read()
exec(compile(_s[:_s.rindex("\nmain()")], SOURCE + "_helpers", "exec"), globals())
exec(compile(open(B + "sky_52.py", encoding="utf-8").read(), "sky_52", "exec"), globals())   # the sky and its clouds
SKY_CAM = None                                                             # the sky's own view (WASTED_PLACE=sky)
# each setting's sky (sky_52.py): daylight, how far towards dusk, the clouds' cover (sky.json clouds.cover)
SKY_OF = {"Morning": (1.0, 0.15, 0.36), "Midday": (1.0, 0.0, 0.36), "Dusk": (0.5, 1.0, 0.36), "NightLamps": (0.0, 0.0, 0.36),
          "NightFullMoon": (0.0, 0.0, 0.36), "NightNewMoon": (0.0, 0.0, 0.2), "Cloudy": (1.0, 0.0, 0.62),
          "Rain": (1.0, 0.0, 0.97), "Fog": (1.0, 0.0, 0.55), "Snow": (1.0, 0.0, 0.92)}

NAME = "hours_50"
HOURS_OUT = os.environ.get("WASTED_OUT") or "C:/Projects/Wasted/Art/StyleTests/" + NAME   # WASTED_OUT: test shots elsewhere
HOURS_LOG = HOURS_OUT + "/" + NAME + "_" + PLACE + "_log.txt"
SAMPLES_H = int(os.environ.get("WASTED_SAMPLES", 64))
SCALE_H = int(os.environ.get("WASTED_SCALE", 100))
WANT = tuple(os.environ["WASTED_HOURS"].split(",")) if os.environ.get("WASTED_HOURS") else None
ROOMS_ONLY = tuple(os.environ["WASTED_ONLY"].split(",")) if os.environ.get("WASTED_ONLY") else None

# key, sun height (degrees), sun strength (times the place's own), sun colour, sky colour, sky strength (times the place's),
# haze (times), electric light ("day": as the approved image has it by day; 0 off, else times), moon (0 none, 1 full),
# exposure (stops past the place's own), the eye (the mean brightness, 0-255, it opens or closes to; None: fixed), weather.
# The weather's numbers follow World/rules/sky.json "weather" (how much sun gets through, how thick the haze, how grey):
# cloud 0.55 of the sun, haze x1.2; rain 0.08, x2.6; fog 0.3, x7; snow 0.15, x2.2. By day; the sky's light is the grey
# light the clouds send down.
HOURS = [
    ("Morning", 22, 1.00, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 1.0, 1.00, "day", 0, 0.0, None, None),
    ("Midday", 62, 1.05, (1.0, 0.95, 0.88), (0.45, 0.60, 0.88), 2.6, 0.00, "day", 0, -0.35, None, None),   # no haze at noon (the owner)
    ("Dusk", 4, 0.22, (1.0, 0.42, 0.22), (0.11, 0.12, 0.27), 1.4, 0.80, 1.0, 0, 0.6, None, None),
    ("NightLamps", -20, 0.0, (1.0, 1.0, 1.0), (0.011, 0.015, 0.034), 1.0, 0.70, 1.0, 0, 1.7, 70, None),
    ("NightFullMoon", -20, 0.0, (1.0, 1.0, 1.0), (0.016, 0.024, 0.056), 1.2, 0.70, 0.0, 1, 3.3, 42, None),
    ("NightNewMoon", -20, 0.0, (1.0, 1.0, 1.0), (0.011, 0.015, 0.034), 1.0, 0.70, 0.0, 0, 3.3, 22, None),
    ("Cloudy", 50, 0.55, (1.0, 0.95, 0.90), (0.54, 0.60, 0.70), 3.2, 1.20, "day", 0, 0.0, 100, "cloud"),
    ("Rain", 50, 0.08, (1.0, 0.97, 0.95), (0.55, 0.58, 0.62), 3.4, 2.60, "day", 0, 0.0, 88, "rain"),
    ("Fog", 50, 0.30, (1.0, 0.97, 0.94), (0.64, 0.66, 0.68), 2.8, 7.00, "day", 0, 0.0, 105, "fog"),
    ("Snow", 50, 0.15, (1.0, 0.98, 0.96), (0.60, 0.63, 0.68), 3.4, 2.20, "day", 0, 0.0, 110, "snow"),
]
WEATHER = dict(sun_angle=dict(cloud=12.0, rain=20.0, fog=20.0, snow=20.0),     # the sun's light softens behind cloud (degrees)
               drops_seen=1800, drop_length=0.2, drop_width=0.0045, drop_alpha=0.75, drop_slant=0.1, near=5.0,   # sky.json "rain"
               wet_roughness=0.32, wet_alpha=0.18,                                                # wet paving and stone catch the sky
               flakes_seen=3600, flake_size=0.036, flake_near_size=0.7, flake_full_at=10.0, flake_alpha=1.0, flake_near=5.0,    # sky.json "snow"
               fog_lamps=0.3, fall_glow=0.0,
               puddles=34, puddle_size=(0.5, 1.6), ripples_per=(5, 9), ripple_width=0.006, ripple_alpha=0.3, ripple_colour=(0.5, 0.53, 0.57),
               puddle_colour=(0.022, 0.025, 0.03), puddle_roughness=0.08, puddle_core=0.55, puddle_depth=0.85,   # as the game's puddles
               wet_step=0.5, wet_reach=60.0,                                     # the wet film: half-metre cells, as the game's roof map   # sky.json rain                                      # drops and flakes: their own faint light                                                     # lanterns by day in fog: their glow tinted it brown
               snow_colour=(0.86, 0.90, 0.96), snow_facing=(0.55, 0.80), snow_cover=0.9,   # lying snow on what faces up
               snow_step=0.25, snow_reach=24.0)                    # where it lies: rays from the sky this far apart, this far round                                   # ... and has the sky over it (nothing within this, straight up)
EYE_MAX_INDOORS = 9.0  # a dark room on a moonless night: only the starlight at the window
EYE_MAX = 6.0          # the night's eye opens at most this many stops past the place's own exposure
HOME_WINDOWS = 0.45    # a home's lit window, times its strength as built (at 1 the alley's burnt to plain white)
STARS = dict(scale=420.0, size=0.045, strength=(2.2, 1.0))   # the night sky's stars: new moon, full moon (a lit sky hides some)
MOON_DISC = dict(distance=320.0, radius=5.0, strength=8.0)    # the full moon seen in the sky
# indoors the eye works by day too, as the game's opens in a room by day (DayOpenIndoors): the mean brightness it seeks
INDOOR_EYE = {"Morning": 100, "Midday": 100, "Dusk": 85}
INDOORS = ("rooms", "bakery", "outlet")
# the moon: high (a full moon in summer stands about 68 degrees up here), turned from the sun's bearing so it falls along
# the alley and the streets, not across them (at 48 degrees across, the alley's houses shaded all of its paving)
MOON = dict(height=68, turn=0.35, strength=0.012, colour=(0.62, 0.72, 1.0),
            indoor_height=30, indoor_times=4.0)            # and stronger: beside the fire the patch must still show                              # indoors: on the sun's bearing, 30 degrees up, through the window   # strength times the place's sun
FIRE = ("Fp_", "Cd_", "Oven", "Ember", "Flame")                                # lights that are fires, not lamps
GLASS_OFF = (0.07, 0.09, 0.12)                                                 # a window with no light behind it
SHADE_OFF = (0.55, 0.50, 0.42)                                                 # a lamp's glass with the lamp out


def hlog(msg):
    line = time.strftime("%H:%M:%S ") + msg
    print(line)
    with open(HOURS_LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")


# ---- the rooms get a fourth wall ---------------------------------------------------------------------------------
if "room_shell" in globals():
    _room_shell = room_shell

    def room_shell(n, ox, oy, w=6.4, d=4.4, h=3.0, ceiling=True, window=True, front=0.0):
        x0, x1, y0, y1 = _room_shell(n, ox, oy, w, d, h, ceiling, window, front)
        box(n + "_Front", x0 - 0.3, x1 + 0.45, y0 - 0.3, y0, 0, h, M["plaster"], bevel=0)   # behind the camera
        if window and PLACE == "rooms":                    # the sunlit street outside the window: its light comes in too
            out = bpy.data.materials.get("M_OutsidePaving") or painted("M_OutsidePaving", (0.22, 0.21, 0.25), (0.46, 0.42, 0.37), noise_scale=0.5)
            box(n + "_Outside", x1 + 0.45, x1 + 40.0, y0 - 30.0, y1 + 30.0, -0.4, -0.1, out, bevel=0)
        return x0, x1, y0, y1


# ---- what a place is lit by ----------------------------------------------------------------------------------------
class Lights:
    """The place's sun and sky as built (its bearing, strengths, haze), its lamps, fires, lamp glass and lit windows."""
    def __init__(self, scene, base=None):
        self.scene = scene
        sun = next((o for o in scene.objects if o.type == 'LIGHT' and o.data.type == 'SUN'), None)
        d = (sun.rotation_euler.to_matrix() @ Vector((0, 0, 1))) if sun else Vector((0.78, 0.30, 0.42))
        self.bearing = math.atan2(d.y, d.x)
        self.sun_energy = sun.data.energy if sun else 8.5
        bg = next((n for n in scene.world.node_tree.nodes if n.type == 'BACKGROUND'), None) if scene.world else None
        self.sky_strength = bg.inputs[1].default_value if bg else 0.3
        self.fog = []
        for ob in scene.objects:
            if ob.type == 'MESH' and ob.name.startswith("Fog") and ob.data.materials:
                for n in ob.data.materials[0].node_tree.nodes:
                    if n.type in ('VOLUME_SCATTER', 'PRINCIPLED_VOLUME') and "Density" in n.inputs:
                        self.fog.append((n.inputs["Density"], n.inputs["Density"].default_value))
        self.lamps, self.fires = [], []
        for ob in scene.objects:
            if ob.type == 'LIGHT' and ob.data.type != 'SUN':
                is_fire = ob.name.startswith(FIRE) or any(f in ob.name for f in FIRE)
                if PLACE in INDOORS and not is_fire:
                    ob.data.color = INDOOR_LAMP                      # indoors a warm white, not the lanterns' orange
                (self.fires if ob.name.startswith(FIRE) or any(f in ob.name for f in FIRE) else self.lamps).append((ob, ob.data.energy))
        self.glow = []                                                       # (bsdf, its strength, its colour, kind)
        for m in bpy.data.materials:
            if not m.use_nodes:
                continue
            for n in m.node_tree.nodes:
                if n.type == 'BSDF_PRINCIPLED' and n.inputs.get("Emission Strength") and n.inputs["Emission Strength"].default_value > 0:
                    nm = m.name
                    kind = "fire" if any(f in nm for f in ("Ember", "Flame")) else ("home" if "WinLitDusk" in nm else
                            ("window" if ("Lit" in nm or "Win" in nm) else "lamp"))   # homes' windows lit from dusk; shops' always
                    self.glow.append((n, n.inputs["Emission Strength"].default_value, tuple(n.inputs["Base Color"].default_value), kind))
        self.sun = sun
        self.day_lamps = 1.0
        self.day_fires = 1.0
        if base:                                                             # a place built in another light: its morning
            if "dir" in base:
                self.bearing = math.atan2(base["dir"][1], base["dir"][0])
            self.sun_energy = base.get("sun", self.sun_energy)
            self.day_lamps = base.get("day_lamps", 1.0)
            self.day_fires = base.get("day_fires", 1.0)
            self.sky_strength = base.get("sky", self.sky_strength)
            if "fog_times" in base:
                self.fog = [(sock, d0 * base["fog_times"]) for sock, d0 in self.fog]

    def set(self, hour):
        key, h, se, sc, skc, sks, fog, lamps, moon, ex, eye_target, weather = hour
        day = lamps == "day"
        lamps = self.day_lamps if day else lamps
        if weather == "fog":
            lamps *= WEATHER["fog_lamps"]
        for o in [o for o in self.scene.objects if o.type == 'LIGHT' and o.data.type == 'SUN']:
            bpy.data.objects.remove(o, do_unlink=True)
        az = self.bearing
        if se > 0:
            el = math.radians(h)
            sun_and_sky(self.scene, (math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)),
                        self.sun_energy * se, sc, skc, self.sky_strength * sks)
        else:
            sun_and_sky(self.scene, (0.0, 0.0, 1.0), 0.0, (1, 1, 1), skc, self.sky_strength * sks)
            for o in [o for o in self.scene.objects if o.type == 'LIGHT' and o.data.type == 'SUN']:
                bpy.data.objects.remove(o, do_unlink=True)
        if weather:
            for o in [o for o in self.scene.objects if o.type == 'LIGHT' and o.data.type == 'SUN']:
                setp(o.data, "angle", math.radians(WEATHER["sun_angle"][weather]))
        for o in [o for o in self.scene.objects if o.name.startswith("MoonDisc")]:
            bpy.data.objects.remove(o, do_unlink=True)
        if se <= 0 and SKY_CAM is None:
            stars(self.scene, STARS["strength"][1 if moon else 0])
        if moon:
            mh, mt = (MOON["indoor_height"], 0.0) if PLACE in INDOORS else (MOON["height"], MOON["turn"])
            el, maz = math.radians(mh), az + mt                # indoors from the window's side, low enough to come in
            ml = bpy.data.lights.new("Moon", 'SUN')
            ml.energy = self.sun_energy * MOON["strength"] * (MOON["indoor_times"] if PLACE in INDOORS else 1.0)
            ml.color = MOON["colour"]
            setp(ml, "angle", math.radians(0.6))
            mo = bpy.data.objects.new("Moon", ml)
            mo.rotation_euler = Vector((math.cos(el) * math.cos(maz), math.cos(el) * math.sin(maz), math.sin(el))).normalized().to_track_quat('Z', 'Y').to_euler()
            self.scene.collection.objects.link(mo)
            moon_in_sky(self.scene, Vector((math.cos(el) * math.cos(maz), math.cos(el) * math.sin(maz), math.sin(el))))
        if SKY_CAM is not None:                                  # the sky's place: the whole sky of sky_52.py
            dl, dk, cv = SKY_OF[key]
            el = math.radians(h)
            sun_dir = Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)))
            sky_world(self.scene, dl, dk, cv, moon, sun_dir, self.sky_strength * sks)
            clouds(self.scene, cv, SKY_CAM.location)
            if moon:
                # for the sky's picture the disc is set in the open sky over the square, left of the tower
                fwd = SKY_CAM.rotation_euler.to_matrix() @ Vector((0, 0, -1))
                mel, maz = math.radians(38), math.atan2(fwd.y, fwd.x) + math.radians(14)
                moon_disc(self.scene, Vector((math.cos(mel) * math.cos(maz), math.cos(mel) * math.sin(maz), math.sin(mel))), SKY_CAM.location)
            else:
                for o in [o for o in self.scene.objects if o.name.startswith("SkyMoon")]:
                    bpy.data.objects.remove(o, do_unlink=True)
        for sock, d0 in self.fog:
            sock.default_value = d0 * fog
        for ob, e0 in self.lamps:
            ob.data.energy = e0 * lamps
        fire = self.day_fires if day else 1.0
        for ob, e0 in self.fires:
            ob.data.energy = e0 * fire
        for n, s0, c0, kind in self.glow:
            k = fire if kind == "fire" else (0.0 if (kind == "home" and day) else (1.0 if (kind == "window" and day) else lamps))
            if kind == "home":
                k *= HOME_WINDOWS
            n.inputs["Emission Strength"].default_value = s0 * k
            n.inputs["Base Color"].default_value = c0 if k > 0 else ((*GLASS_OFF, 1.0) if kind in ("window", "home") else (*SHADE_OFF, 1.0))
        return ex


def stars(scene, strength):
    """Stars in the night sky: the world's colour plus small points where a Voronoi field of the view direction is near
    a cell's middle (W7: the night reads; the slice's sky has its stars, ADR-0023)."""
    wt = scene.world.node_tree
    bg = next(n for n in wt.nodes if n.type == 'BACKGROUND')
    tc = wt.nodes.new("ShaderNodeTexCoord")
    vo = wt.nodes.new("ShaderNodeTexVoronoi")
    vo.inputs["Scale"].default_value = STARS["scale"]
    lt = wt.nodes.new("ShaderNodeMath")
    lt.operation = 'LESS_THAN'
    lt.inputs[1].default_value = STARS["size"]
    st = wt.nodes.new("ShaderNodeBackground")
    st.inputs[0].default_value = (1.0, 0.97, 0.92, 1.0)
    mix = wt.nodes.new("ShaderNodeAddShader")
    mul = wt.nodes.new("ShaderNodeMath")
    mul.operation = 'MULTIPLY'
    mul.inputs[1].default_value = strength
    wt.links.new(tc.outputs["Generated"], vo.inputs["Vector"])
    wt.links.new(vo.outputs["Distance"], lt.inputs[0])
    wt.links.new(lt.outputs[0], mul.inputs[0])
    wt.links.new(mul.outputs[0], st.inputs[1])
    out = next(n for n in wt.nodes if n.type == 'OUTPUT_WORLD')
    wt.links.new(bg.outputs[0], mix.inputs[0])
    wt.links.new(st.outputs[0], mix.inputs[1])
    wt.links.new(mix.outputs[0], out.inputs["Surface"])
    # the stars are seen, they do not light the place
    try:
        scene.world.cycles_visibility.diffuse = True
    except Exception:
        pass


def moon_in_sky(scene, toward):
    """The full moon's disc in the sky, far off along its light; it is seen, it casts and gives no light of its own."""
    p = toward.normalized() * MOON_DISC["distance"]
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=MOON_DISC["radius"])
    me = bpy.data.meshes.new("MoonDisc")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("MoonDisc", me)
    ob.location = p
    m = bpy.data.materials.get("M_MoonDisc") or emissive("M_MoonDisc", (0.90, 0.92, 1.0), MOON_DISC["strength"])
    me.materials.append(m)
    for k in ("visible_diffuse", "visible_glossy", "visible_shadow", "visible_transmission", "visible_volume_scatter"):
        setp(ob, k, False)
    scene.collection.objects.link(ob)


def shoot(scene, cam, exposure, path, quiet=False):
    if cam is not None:
        scene.camera = cam
    configure(scene, exposure, (1280, 800), SAMPLES_H)
    c = scene.cycles
    setp(c, "diffuse_bounces", 6)
    setp(c, "max_bounces", max(c.max_bounces, 6))
    scene.render.resolution_percentage = SCALE_H
    setp(scene.render.image_settings, "file_format", 'JPEG')
    setp(scene.render.image_settings, "quality", 90)
    scene.render.filepath = path
    t = time.time()
    bpy.ops.render.render(write_still=True, scene=scene.name)
    if not quiet:
        hlog("%s rendered in %.1fs" % (os.path.basename(path), time.time() - t))


def each_hour(scene, cam, exposure, stem, base=None):
    L = Lights(scene, base)
    hlog("%s: sun %.1f at bearing %.0f deg, sky %.2f; %d lamps, %d fires, %d glowing materials, %d haze" %
         (stem, L.sun_energy, math.degrees(L.bearing), L.sky_strength, len(L.lamps), len(L.fires), len(L.glow), len(L.fog)))
    for hour in HOURS:
        if WANT and hour[0] not in WANT:
            continue
        if hour[-1] and PLACE in INDOORS:                  # indoors no weather settings (the owner, 2026-11-26)
            continue
        ex = L.set(hour)
        fallen = []
        if hour[-1] and PLACE not in INDOORS and cam is not None:   # out of doors it falls and lies; indoors only the light changes
            fallen = weather_out(scene, cam, hour[-1])
        target = INDOOR_EYE.get(hour[0], hour[-2]) if PLACE in INDOORS else hour[-2]
        if target:
            ex = eye(scene, cam, exposure, ex, target, stem + "_" + hour[0])
        shoot(scene, cam, exposure + ex, "%s/%s_%s.jpg" % (HOURS_OUT, stem, hour[0]))
        undo_weather(fallen)


SQUARE_BOUNCE = dict(M_Paving=2.4, M_Stone=1.8, M_StoneDark=1.8)          # how much more light the sunlit ground and walls throw into shade


def bounce_more(times):
    """More light bounced off the named materials into shade, the look of the surfaces unchanged: what the camera sees
    of them keeps its colour; light that reaches them by way of another surface takes the brighter one (capped at 0.85).
    The Blender references' counterpart of the slice's "bounce x1.2" (ART_DIRECTION 2.4)."""
    for name, k in times.items():
        m = bpy.data.materials.get(name)
        if m is None or not m.use_nodes:
            continue
        nt = m.node_tree
        for b in [n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED']:
            base = b.inputs["Base Color"]
            src = base.links[0].from_socket if base.is_linked else None
            lp = nt.nodes.new("ShaderNodeLightPath")
            up = nt.nodes.new("ShaderNodeMix")
            up.data_type = 'RGBA'
            up.blend_type = 'MULTIPLY'
            up.inputs["Factor"].default_value = 1.0
            if src is not None:
                nt.links.new(src, up.inputs[6])
            else:
                up.inputs[6].default_value = tuple(base.default_value)
            up.inputs[7].default_value = (k, k, k, 1.0)
            cap = nt.nodes.new("ShaderNodeMix")
            cap.data_type = 'RGBA'
            cap.blend_type = 'DARKEN'
            cap.inputs["Factor"].default_value = 1.0
            nt.links.new(up.outputs[2], cap.inputs[6])
            cap.inputs[7].default_value = (0.85, 0.85, 0.85, 1.0)
            pick = nt.nodes.new("ShaderNodeMix")
            pick.data_type = 'RGBA'
            nt.links.new(lp.outputs["Is Camera Ray"], pick.inputs["Factor"])
            nt.links.new(cap.outputs[2], pick.inputs[6])           # seen by bounced light: brighter
            if src is not None:
                nt.links.new(src, pick.inputs[7])                    # seen by the camera: as it is
            else:
                pick.inputs[7].default_value = tuple(base.default_value)
            nt.links.new(pick.outputs[2], base)



ROOM_GREYER = dict(M_Boards=0.4)
ROOM_BOUNCE = dict(M_OutsidePaving=2.6, M_Plaster=2.0, M_PlasterWarm=2.0, M_Boards=2.0)   # as SQUARE_BOUNCE, more: a room is lit by bounce
INDOOR_LAMP = (1.0, 0.86, 0.72)                            # the owner, 2026-11-26: the night lamps indoors less orange                          # how far towards grey: the boards as the town's weathered wood


def greyer(how):
    """Take colours part way towards their own grey (the named materials' ramps, bricks and base colours), so a room's
    floor does not tint its light orange beside the town's grey stone and wood (the owner, 2026-11-26)."""
    def g(c, k):
        y = 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]
        return (c[0] + (y - c[0]) * k, c[1] + (y - c[1]) * k, c[2] + (y - c[2]) * k, c[3] if len(c) > 3 else 1.0)
    for name, k in how.items():
        m = bpy.data.materials.get(name)
        if m is None or not m.use_nodes:
            continue
        for n in m.node_tree.nodes:
            if n.type == 'VALTORGB':
                for e in n.color_ramp.elements:
                    e.color = g(tuple(e.color), k)
            for sock in n.inputs:
                if sock.type == 'RGBA' and not sock.is_linked:
                    sock.default_value = g(tuple(sock.default_value), k)


WET = {}                                                   # material -> its roughness when dry


def weather_out(scene, cam, kind):
    """Rain falling in streaks and the paving wet, or snow falling and lying on what faces up, in front of the camera.
    Returns what to take away afterwards (lying snow stays: Snow is the last setting)."""
    made = []
    if kind == "rain":
        made.append(lying(scene, cam, "Wet", wet_film(), 0.002, 0.0, tile=True, step=WEATHER["wet_step"], reach=WEATHER["wet_reach"]))   # the open paving darker and sheened, as the game's
        made += puddles(scene, cam)                                   # water only where the rain falls, not under a roof
        made.append(falling(scene, cam, "Rain", WEATHER["drops_seen"], (WEATHER["drop_width"], WEATHER["drop_width"], WEATHER["drop_length"]),
                            (0.60, 0.67, 0.78), WEATHER["drop_alpha"], WEATHER["drop_slant"], WEATHER["near"]))
    if kind == "snow":
        snow = bpy.data.materials.get("M_SnowLying") or painted("M_SnowLying", (0.78, 0.82, 0.88), WEATHER["snow_colour"], noise_scale=1.6, bump=0.15)
        made.append(lying(scene, cam, "Snow", snow, 0.012, 0.012))
        made.append(falling(scene, cam, "Snow", WEATHER["flakes_seen"], (WEATHER["flake_size"],) * 3, (1.0, 1.0, 1.0), WEATHER["flake_alpha"], 0.0,
                            WEATHER["flake_near"], flake=True))
    return made


def undo_weather(made):
    for ob in made:
        if ob:
            bpy.data.objects.remove(ob, do_unlink=True)
    for n, r in list(WET.items()):
        n.inputs["Roughness"].default_value = r
    WET.clear()


def falling(scene, cam, name, count, size, colour, alpha, slant, near, flake=False):
    """Drops or flakes scattered through the view, from near to 16 m in front of the camera (as the game's rings round
    it): a drop is two thin crossed strips, a flake a small smooth ball."""
    rng = random.Random(50)
    bm = bmesh.new()
    dg = bpy.context.evaluated_depsgraph_get()
    up = Vector((0.0, 0.0, 1.0))
    kept = 0
    # from the camera's own place and turn: its matrix_world is not brought up to date before a render (the drops fell under
    # the street)
    M = Matrix.Translation(cam.location) @ cam.rotation_euler.to_matrix().to_4x4()
    lens = cam.data.lens / 36.0
    tries = 0
    while kept < count and tries < count * 8:             # as many under open sky as asked, whatever the place (the alley
        tries += 1                                         # kept a quarter of what fell; the open square nearly all)
        d = rng.uniform(near, 16.0)
        x, y = rng.uniform(-0.62, 0.62) * d / lens, rng.uniform(-0.42, 0.42) * d / lens
        c = M @ Vector((x, y, -d))
        if covered(scene, dg, c):                            # a roof, an arch or a ledge over it: nothing falls there
            continue
        kept += 1
        sx, sy, sz = size
        if flake:                                            # a small round ball, smooth, lit on top and shaded under;
            near_k = WEATHER["flake_near_size"]                 # the nearest smaller (the owner: the large ones only)
            t = min(1.0, (d - near) / max(0.01, WEATHER["flake_full_at"] - near))
            bmesh.ops.create_icosphere(bm, subdivisions=1, radius=sx * (near_k + (1.0 - near_k) * t), matrix=Matrix.Translation(c))
            continue
        v = [bm.verts.new(c + Vector((dx * sx, dy * sy, dz * sz)) + Vector((slant * dz * sz, 0, 0)))
             for dx, dy, dz in ((-1, 0, -1), (1, 0, -1), (1, 0, 1), (-1, 0, 1), (0, -1, -1), (0, 1, -1), (0, 1, 1), (0, -1, 1))]
        bm.faces.new(v[0:4])
        bm.faces.new(v[4:8])
    me = bpy.data.meshes.new(name + "Fall")
    bm.to_mesh(me)
    bm.free()
    if flake:
        me.shade_smooth()
    m = bpy.data.materials.get("M_" + name + "Fall")
    if m is None:
        m = bpy.data.materials.new("M_" + name + "Fall")
        m.use_nodes = True
        b = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        b.inputs["Base Color"].default_value = (*colour, 1.0)
        b.inputs["Alpha"].default_value = alpha
        b.inputs["Roughness"].default_value = 0.9
        # a little light of their own, as drops and flakes catch the grey sky's light from every side and read against it
        b.inputs["Emission Color"].default_value = (*colour, 1.0)
        b.inputs["Emission Strength"].default_value = WEATHER["fall_glow"]
        setp(m, "blend_method", 'BLEND')
    me.materials.append(m)
    ob = bpy.data.objects.new(name + "Fall", me)
    for k in ("visible_shadow", "visible_diffuse", "visible_glossy"):
        setp(ob, k, False)
    scene.collection.objects.link(ob)
    hlog("  %s: %d of %d tried under open sky, %d faces, %.1f to %.1f m up, from %.1f m in front of the camera" % (name, kept, tries, len(me.polygons),
         min(v.co.z for v in me.vertices), max(v.co.z for v in me.vertices), near))
    return ob


def covered(scene, dg, at):
    """Is anything solid straight over this point? The haze's box is not (its rays go on through it)."""
    up, p = Vector((0.0, 0.0, 1.0)), at.copy()
    for _ in range(8):
        hit, loc, nrm, idx, ob, mtx = scene.ray_cast(dg, p, up, distance=200.0)
        if not hit:
            return False
        if not ob.name.startswith("Fog"):
            return True
        p = loc + up * 0.01
    return False


def puddles(scene, cam):
    """Puddles on the open paving, each with rings where drops are falling into it (sky.json rain: puddles, ripples).
    A puddle lies only where the sky is open over all of it; the paving under a roof stays dry."""
    dg = bpy.context.evaluated_depsgraph_get()
    rng = random.Random(52)
    down = Vector((0.0, 0.0, -1.0))
    cx, cy = cam.location.x, cam.location.y
    water = bpy.data.materials.get("M_Puddle")
    if water is None:                                              # as the game's: a darker, still patch, softly edged
        water = bpy.data.materials.new("M_Puddle")
        water.use_nodes = True
        nt = water.node_tree
        b = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
        b.inputs["Base Color"].default_value = (*WEATHER["puddle_colour"], 1.0)
        b.inputs["Roughness"].default_value = WEATHER["puddle_roughness"]
        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
        tr = nt.nodes.new("ShaderNodeBsdfTransparent")
        uv = nt.nodes.new("ShaderNodeUVMap")                       # the fade rides in the UV map's u
        sx = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(uv.outputs["UV"], sx.inputs[0])
        mx = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(sx.outputs["X"], mx.inputs["Fac"])
        nt.links.new(tr.outputs[0], mx.inputs[1])
        nt.links.new(b.outputs[0], mx.inputs[2])
        nt.links.new(mx.outputs[0], out.inputs["Surface"])
    ring = bpy.data.materials.get("M_Ripple")
    if ring is None:
        ring = bpy.data.materials.new("M_Ripple")
        ring.use_nodes = True
        b = next(n for n in ring.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        b.inputs["Base Color"].default_value = (*WEATHER["ripple_colour"], 1.0)
        b.inputs["Roughness"].default_value = 0.3
        b.inputs["Alpha"].default_value = WEATHER["ripple_alpha"]
        setp(ring, "blend_method", 'BLEND')

    def ground(x, y):
        p = Vector((x, y, 60.0))
        for _ in range(8):
            hit, loc, nrm, idx, ob, mtx = scene.ray_cast(dg, p, down, distance=120.0)
            if not hit or not ob.name.startswith(("Fog", "RainFall")):
                break
            p = loc + down * 0.01
        return (loc, nrm, ob) if hit else None

    Mc = Matrix.Translation(cam.location) @ cam.rotation_euler.to_matrix().to_4x4()
    lens = cam.data.lens / 36.0
    bp, br = bmesh.new(), bmesh.new()
    made, tries = 0, 0
    apart = []                                                     # (x, y, reach) of the puddles laid so far
    core = set()                                                   # the verts inside the soft edge
    reach = WEATHER["snow_reach"]
    while made < WEATHER["puddles"] and tries < WEATHER["puddles"] * 40:
        tries += 1
        # on the ground the camera sees: a point 3 to 22 m ahead, across the view
        d = rng.uniform(3.0, 22.0)
        q = Mc @ Vector((rng.uniform(-0.55, 0.55) * d / lens, 0.0, -d))
        x, y = q.x, q.y
        g = ground(x, y)
        if not g or g[1].z < 0.97 or g[0].z > 0.25 or g[2].name.startswith(("Fig_", "MoonDisc")):
            continue                                               # puddles on the ground only, level, open to the sky
        rx, ry = rng.uniform(*WEATHER["puddle_size"]), rng.uniform(*WEATHER["puddle_size"])
        if any(math.hypot(x - px0, y - py0) < (max(rx, ry) + pr) * 1.25 for px0, py0, pr in apart):
            continue                                               # puddles apart: overlapping ones mirror each other black
        turn = rng.uniform(0, math.pi)
        edge = []
        ok = True
        for k in range(18):
            a = 2 * math.pi * k / 18
            wob = 1.0 + 0.18 * math.sin(3 * a + rng.random() * 6) + 0.1 * math.sin(5 * a + rng.random() * 6)
            ex, ey = math.cos(a) * rx * wob, math.sin(a) * ry * wob
            px, py = x + ex * math.cos(turn) - ey * math.sin(turn), y + ex * math.sin(turn) + ey * math.cos(turn)
            ge = ground(px, py)
            if not ge or abs(ge[0].z - g[0].z) > 0.03:             # every edge under open sky and on the same level ground
                ok = False
                break
            edge.append(Vector((px, py, g[0].z + 0.004)))
        if not ok:
            continue
        apart.append((x, y, max(rx, ry) * 1.3))
        mid = bp.verts.new(Vector((x, y, g[0].z + 0.004)))
        vs = [bp.verts.new(e) for e in edge]                       # the edge, where the water thins to nothing
        cen = Vector((x, y, g[0].z + 0.004))
        ins = [bp.verts.new(cen + (e - cen) * WEATHER["puddle_core"]) for e in edge]   # the full water inside
        for k in range(len(vs)):
            bp.faces.new((mid, ins[k], ins[(k + 1) % len(vs)]))
            bp.faces.new((ins[k], vs[k], vs[(k + 1) % len(vs)], ins[(k + 1) % len(vs)]))
        for v in [mid] + ins:
            core.add(v)
        for _ in range(rng.randint(*WEATHER["ripples_per"])):  # rings spreading where drops hit, a few at a time
            a, d = rng.uniform(0, 2 * math.pi), rng.uniform(0.0, 0.6) * min(rx, ry)
            rc = Vector((x + math.cos(a) * d, y + math.sin(a) * d, g[0].z + 0.006))
            r0 = rng.uniform(0.03, 0.22)
            w = WEATHER["ripple_width"]
            for ringn in range(rng.randint(1, 2)):            # a drop's ring, and sometimes the next one inside it
                rr = r0 * (1.0 - 0.45 * ringn)
                inner = [br.verts.new(rc + Vector((math.cos(2 * math.pi * k / 28) * (rr - w), math.sin(2 * math.pi * k / 28) * (rr - w), 0))) for k in range(28)]
                outer = [br.verts.new(rc + Vector((math.cos(2 * math.pi * k / 28) * (rr + w), math.sin(2 * math.pi * k / 28) * (rr + w), 0))) for k in range(28)]
                for k in range(28):
                    br.faces.new((inner[k], inner[(k + 1) % 28], outer[(k + 1) % 28], outer[k]))
        made += 1
    obs = []
    lay = bp.loops.layers.uv.new("UVMap")
    for f in bp.faces:
        for lp in f.loops:
            lp[lay].uv = (WEATHER["puddle_depth"] if lp.vert in core else 0.0, 0.0)
    for bm, nm, m in ((bp, "Puddles", water), (br, "Ripples", ring)):
        me = bpy.data.meshes.new(nm)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(m)
        ob = bpy.data.objects.new(nm, me)
        scene.collection.objects.link(ob)
        obs.append(ob)
    hlog("  %d puddles on the open paving (%d tries)" % (made, tries))
    return obs


def wet_film():
    """A thin dark sheen over what the rain wets: the paving darker and catching the sky (sky.json rain: wet_dark, wet_sheen)."""
    m = bpy.data.materials.get("M_WetFilm")
    if m is None:
        m = bpy.data.materials.new("M_WetFilm")
        m.use_nodes = True
        b = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
        b.inputs["Base Color"].default_value = (0.05, 0.05, 0.055, 1.0)
        b.inputs["Roughness"].default_value = WEATHER["wet_roughness"]
        b.inputs["Alpha"].default_value = WEATHER["wet_alpha"]
        setp(m, "blend_method", 'BLEND')
    return m


def lying(scene, cam, name, mat, lift, jitter, tile=False, step=None, reach=None):
    """What lies where it falls (snow; the rain's wet): rays straight down from the sky every snow_step m over the ground
    round the camera; where one first lands on a face turned up (paving, a bench, a crate, a ledge, a roof) a thin patch
    lies on it. Under a roof or an arch nothing lands, so the paving there stays bare and dry (WLD-070)."""
    dg = bpy.context.evaluated_depsgraph_get()
    step, reach, lo = step or WEATHER["snow_step"], reach or WEATHER["snow_reach"], WEATHER["snow_facing"][0]
    down = Vector((0.0, 0.0, -1.0))
    cx, cy = cam.location.x, cam.location.y
    rng = random.Random(51)
    bm = bmesh.new()
    n = 0
    k = int(reach / step)
    for i in range(-k, k + 1):
        for j in range(-k, k + 1):
            p = Vector((cx + i * step, cy + j * step, 60.0))
            for _ in range(8):                               # through the haze's box, to the first solid face
                hit, loc, nrm, idx, ob, mtx = scene.ray_cast(dg, p, down, distance=120.0)
                if not hit or not ob.name.startswith(("Fog", "SnowFall", "RainFall")):
                    break
                p = loc + down * 0.01
            if not hit or nrm.z < lo or ob.name.startswith(("Fog", "Fig_", "MoonDisc")):
                continue
            h = lift + jitter * rng.random()                 # a soft, uneven cover
            c = loc + nrm * h
            a, b = (step * 0.5, step * 0.5) if tile else (step * 0.56, step * 0.56)   # wet: edge to edge, no doubled film
            v = [bm.verts.new(c + Vector((dx * a, dy * b, 0.0))) for dx, dy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
            bm.faces.new(v)
            n += 1
    me = bpy.data.meshes.new(name + "Lying")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    ob = bpy.data.objects.new(name + "Lying", me)
    scene.collection.objects.link(ob)
    hlog("  %s lies on %d patches of %.2f m (rays from the sky over %d m round the camera)" % (name, n, step, reach))
    return ob


def mean_of(path):
    img = bpy.data.images.load(path, check_existing=False)
    px = img.pixels[:]
    bpy.data.images.remove(img)
    n = len(px) // 4
    return 255.0 * sum(px[i] + px[i + 1] + px[i + 2] for i in range(0, len(px), 4)) / (3 * n)


def eye(scene, cam, exposure, ex, target, stem):
    """The night's eye, as the game's (ADR-0055): a small quick render, then the exposure that brings its mean brightness
    to the target, at most EYE_MAX stops open; twice, as the picture's tone curve is not straight."""
    keep = (SAMPLES_H, SCALE_H)
    globals()["SAMPLES_H"], globals()["SCALE_H"] = 10, 20
    path = HOURS_OUT + "/_eye.jpg"
    try:
        for _ in range(2):
            shoot(scene, cam, exposure + ex, path, quiet=True)
            m = max(mean_of(path), 0.5)
            ex = max(-2.0, min(EYE_MAX_INDOORS if PLACE in INDOORS else EYE_MAX, ex + math.log2(target / m) * 0.9))
    finally:
        globals()["SAMPLES_H"], globals()["SCALE_H"] = keep
        if os.path.exists(path):
            os.remove(path)
    hlog("  %s: the eye at %+.2f stops for a mean of %d" % (stem, ex, target))
    return ex


def centre_y(ob):
    """How far down the alley a mesh is: the middle of its points in the world (box() puts them there, not the object)."""
    vs = [ob.matrix_world @ v.co for v in ob.data.vertices]
    return sum(v.y for v in vs) / max(1, len(vs))


def street_lamps(scene, street_end):
    """The street had no lamps of its own: lanterns on posts along the near kerb, as the game's streets have (WLD-072)."""
    m = dict(lamp=bpy.data.materials.get("M_Lamp") or emissive("M_Lamp", (1.0, 0.55, 0.22), 6.0), iron=K_M.get("iron"))
    x = 4.0
    while x < street_end:
        cyl("SL_Post", x, -1.6, 0, 0.07, 3.6, m["iron"] or K_M["stone_dark"], segs=10)
        lantern("SL_Lamp", x, -1.35, 3.75, m, 90)
        x += 16.0


def main_hours():
    os.makedirs(HOURS_OUT, exist_ok=True)
    open(HOURS_LOG, "w").close()
    hlog("Blender %s, place %s" % (bpy.app.version_string, PLACE))
    if PLACE == "alley":
        scene, ex = alley(True)                      # built with its dusk lamps and lit windows; the hours switch them
        # fewer lights down the alley, so the light of each hour can be read (the owner, 2026-11-26: "way too many lights
        # in the alley on the back side"): no string of bulbs, no lanterns past the one by the gate, no lit window far down
        glass = bpy.data.materials.get("M_Glass")
        for ob in list(scene.objects):
            if ob.name.startswith(("D_Bulb", "D_Lamp_Bridge", "D_Lamp_Far", "D_Lamp_Left")):
                bpy.data.objects.remove(ob, do_unlink=True)
            elif ob.type == 'MESH' and glass and ob.data.materials and ob.data.materials[0].name == "M_WinLitDusk" and centre_y(ob) > 10:
                ob.data.materials[0] = glass
        # its light is the dusk's as built: the morning's is old_town_04.alley(False)'s (sun, sky and haze 0.022 for 0.014)
        each_hour(scene, scene.camera, 0.3, "Alley", dict(dir=(0.446, 0.594, 0.669), sun=14.0, sky=0.35, fog_times=0.022 / 0.014))
    elif PLACE == "square":
        scene, ex = square()
        bounce_more(SQUARE_BOUNCE)                  # the owner, 2026-11-26: the midday shade too dark, the bounce missing
        each_hour(scene, scene.camera, ex, "MarketSquare")
    elif PLACE == "sky":
        global SKY_CAM
        scene, ex = square()
        SKY_CAM = camera(scene, "Cam_Sky", (1.0, -6.0, 1.7), (0.0, 30.0, 13.5), 15.0)   # in the square, up over the roofs
        SKY_CAM.data.clip_end = 20000.0                     # the clouds are 1.2 km up, the moon further
        each_hour(scene, SKY_CAM, ex, "Sky")
    elif PLACE == "street":
        scene, facts, street_end = street_scene()
        street_lamps(scene, street_end)
        cam = cams(scene, facts, street_end)[0][1]
        each_hour(scene, cam, -0.15, "Street")
    elif PLACE == "rooms":
        for kit in KITS:
            if ROOMS_ONLY and kit[0] not in ROOMS_ONLY:
                continue
            try:
                scene = fresh("Furn_room_" + kit[0])
                extra_mats()
                spec, label = kit_room(kit, lines())
                greyer(ROOM_GREYER)                         # the room's surfaces in the town's own colours
                bounce_more(ROOM_BOUNCE)                    # the light thrown back as in the square's shade
                (nm, loc, tgt, lens) = spec[0]
                # by day only the world's sun and sky light the room, through its window (the owner, 2026-11-26: the
                # rooms in the world's light, not their own warm lamps)
                each_hour(scene, camera(scene, "Cam_" + kit[0], loc, tgt, lens), 1.3, "Rooms_" + kit[0].capitalize(), dict(day_lamps=0.0, day_fires=0.0))
            except Exception:
                hlog("FAILED room %s:\n%s" % (kit[0], traceback.format_exc()))
    elif PLACE == "bakery":
        scene = fresh("Furn_bakery")
        bakery_mats()
        (nm, loc, tgt, lens) = bakery_scene(False)[0]
        for ob in scene.objects:                     # bakery_scene() dimmed its pendants by day: back to full, the hours set them
            if ob.type == 'LIGHT' and ob.name.startswith("Pen_Light"):
                ob.data.energy /= 0.06
        each_hour(scene, camera(scene, "Cam_bakery", loc, tgt, lens), 1.15, "Bakery", dict(day_lamps=0.06))
    elif PLACE == "outlet":
        scene = fresh("Furn_showroom")
        (nm, loc, tgt, lens) = showroom_scene(lines())[0]
        each_hour(scene, camera(scene, "Cam_showroom", loc, tgt, lens), 0.5, "FurnitureOutlet")
    hlog("DONE")


try:
    main_hours()
except Exception:
    hlog("FAILED:\n" + traceback.format_exc())
