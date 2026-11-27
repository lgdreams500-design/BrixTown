"""Brix Town - hours 50: every reference place at every hour (the owner, 2026-11-26, reviewing the references: a reference
is not one setting; the alley, Market Square and the street, a home's room, a hallway, a public room (the bakery) and a
big hall (the furniture outlet) each at morning, midday, dusk and night, the night with the lamps on, and off under a
full and a new moon, and in cloud, rain, fog and snow; Market Square's midday shade too dark, as if the bounced light
were missing).

Headless, one place a run (each place is built by the script it always was, then lit for each hour); from any
checkout, a review round or the full set (FEATURE_PROCESS 6):
    Tools/blender/reference_render.ps1 -Place <place> [-Review] [-Hours Midday] -Out <folder>
or by hand:
    WASTED_PLACE=<place> blender-launcher.exe -b --factory-startup --python C:/Projects/Wasted/Tools/blender/hours_50.py
    places also mall, mall_entrance, egg_hall (Eastgate's court, its south entrance, the Food Market's egg hall): only what the camera sees is built in detail
    (sees(); WASTED_ALL=1 builds everything, to check that nothing seen was left out)
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
import bpy, bmesh, os, math, random, time, traceback, json
from mathutils import Matrix, Vector

B = "C:/Projects/Wasted/Tools/blender/"
PLACE = os.environ.get("WASTED_PLACE", "alley")
SOURCE = {"alley": "old_town_04.py", "square": "old_town_04.py", "street": "building_kit_07.py",
          "rooms": "baker_look_47.py", "hallway": "baker_look_47.py", "shop": "baker_look_47.py", "mall": "baker_look_47.py", "mall_entrance": "baker_look_47.py", "egg_hall": "baker_look_47.py", "bakery": "baker_look_47.py", "outlet": "baker_look_47.py", "sky": "old_town_04.py"}[PLACE]
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
INDOORS = ("rooms", "hallway", "bakery", "shop", "outlet", "mall", "mall_entrance", "egg_hall")
# a place's own eye where it differs: the hallway has no window, so a full moon seen only through a room's door is as dim
# there as the new moon was (the owner, 2026-11-26: "full moon is giving too much light ... at the level currently shown on
# new moon"; that picture's mean was 12)
FIRE_LIGHT = 0.5                                           # a fire's light, of its strength as built
FIRE_GLOW = 0.1                                            # how bright a fire's flames and embers glow, of their strength as built
FIRE_COLOUR = (1.0, 0.24, 0.06)                          # every fire, in every reference: the owner, 2026-11-26, "oven flame should be
                                                          # same color in all references and more red"
FIRE_OUT_HOURS = {"rooms": ("NightFullMoon", "NightNewMoon")}   # the owner, 2026-11-26: "we need reference of moon lights when the
                                                          # fire place is turned off": these again with the fire out too
SKIP_HOURS = {"bakery": ("NightFullMoon", "NightNewMoon"), "shop": ("NightFullMoon", "NightNewMoon"),
              "outlet": ("NightFullMoon", "NightNewMoon"), "mall": ("NightFullMoon", "NightNewMoon"),
              "mall_entrance": ("NightFullMoon", "NightNewMoon"), "egg_hall": ("NightFullMoon", "NightNewMoon")}   # halls as the shops   # a shop as the bakery   # the owner, 2026-11-26: "for bakery, don't need moon settings"
PLACE_EYE = {"hallway": {"NightFullMoon": 12, "NightNewMoon": 2.5}, "mall": {"Midday": 82},   # the court's midday sun under open skylights
             "rooms": {"NightFullMoonFireOut": 12, "NightNewMoonFireOut": 2.5}}   # the home room's fire-out nights as dim (the owner)   # and the new moon darker still ("should not have so much light")
# the moon: high (a full moon in summer stands about 68 degrees up here), turned from the sun's bearing so it falls along
# the alley and the streets, not across them (at 48 degrees across, the alley's houses shaded all of its paving)
MOON = dict(height=68, turn=0.35, strength=0.012, colour=(0.62, 0.72, 1.0),   # strength times the place's sun
            indoor_height=30,                              # indoors: on the sun's bearing, 30 degrees up, through the window
            indoor_times=4.0,                              # and stronger: beside the fire the patch must still show
            indoor_colour=(0.26, 0.42, 1.0))               # the hallway's moonlight cooler: off warm plaster the moon's own read warm
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
        if not ceiling:                                  # a showroom's set: three walls, open to the walking route
            return x0, x1, y0, y1
        box(n + "_Front", x0 - 0.3, x1 + 0.45, y0 - 0.3, y0, 0, h, M["plaster"], bevel=0)   # behind the camera
        if PLACE in ("rooms", "bakery", "shop"):          # the light that wall throws back onto the people (the owner, 2026-11-26:
            bc = bpy.data.lights.new("BounceCard", 'AREA')  # "the wall opposite to the character is not reflecting light back")
            bc.shape = 'RECTANGLE'
            bc.size, bc.size_y = (x1 - x0) * 0.9, h * 0.8
            setp(bc, "use_shadow", True)
            bo = bpy.data.objects.new("BounceCard", bc)
            bo.location = ((x0 + x1) / 2, y0 + 0.05, h * 0.45)
            bo.rotation_euler = (math.radians(90), 0, 0)      # shining into the room (+y)
            for k in ("visible_camera", "visible_glossy", "visible_transmission"):
                setp(bo, k, False)
            COL.objects.link(bo)
        if window and PLACE in ("rooms", "bakery", "shop"):   # the sunlit street outside the window: its light comes in too
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
        self.lamps, self.fires, self.shoplights = [], [], []
        for ob in scene.objects:
            if ob.type == 'LIGHT' and ob.data.type != 'SUN' and ob.name.startswith("ShopIn"):
                self.shoplights.append(ob)                           # an open outlet's lights: on all day, as a shop's windows
                continue
            if ob.type == 'LIGHT' and ob.data.type != 'SUN' and not ob.name.startswith("BounceCard"):
                is_fire = ob.name.startswith(FIRE) or any(f in ob.name for f in FIRE)
                kind = lamp_kind(ob.name) if not is_fire else None
                if is_fire:
                    ob.data.color = FIRE_COLOUR
                    ob.data.energy *= FIRE_LIGHT                  # softer and spread: close up it burnt the stone to pale pink
                    setp(ob.data, "shadow_soft_size", 0.35)
                if kind:                                             # its kind's colour and strength (FURN-067, lights.json)
                    ob.data.color = tuple(kind["colour"])
                    ob.data.energy = kind["strength"]
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
                    if kind == "fire":                                    # every fire one warm red (the owner)
                        n.inputs["Base Color"].default_value = (0.0, 0.0, 0.0, 1.0)   # it glows, it catches no light: red stays red
                        if n.inputs.get("Emission Color") is not None:
                            n.inputs["Emission Color"].default_value = (*FIRE_COLOUR, 1.0)
                        n.inputs["Emission Strength"].default_value *= FIRE_GLOW   # so bright it washed to yellow-white
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
        rolled = key in AWNING_IN
        for ob in self.scene.objects:                         # a shop's awning: out, or rolled up under its fascia
            if ob.name.startswith("Shp_AwningRoll"):
                ob.hide_render = not rolled
            elif ob.name.startswith("Shp_Awning"):
                ob.hide_render = rolled
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
            ml.color = MOON["indoor_colour"] if PLACE == "hallway" else MOON["colour"]
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
        card = bpy.data.objects.get("BounceCard")                # the wall facing the people throws back the day and the lamps
        if card:
            sky_y = 0.2126 * skc[0] + 0.7152 * skc[1] + 0.0722 * skc[2]          # the sky's own brightness (dark at night)
            day_w = BOUNCE_CARD["day_watts"] * (se * self.sun_energy / 8.5 + 0.6 * sks * sky_y / 0.53) / 1.6
            lamp_w = BOUNCE_CARD["lamp_share"] * sum(ob.data.energy for ob, _ in self.lamps)
            day_c = tuple(0.5 * (a + b) for a, b in zip(sc, skc[:3])) if se > 0 else (1.0, 1.0, 1.0)
            lamp_c = [0.0, 0.0, 0.0]
            for ob, _ in self.lamps:
                for i in range(3):
                    lamp_c[i] += ob.data.color[i] * ob.data.energy
            tot = sum(ob.data.energy for ob, _ in self.lamps) or 1.0
            lamp_c = [c / tot for c in lamp_c]
            w = day_w + lamp_w
            card.data.energy = w
            card.data.color = tuple((day_c[i] * day_w + lamp_c[i] * lamp_w) / (w or 1.0) for i in range(3))
        fire = 0.0 if key.endswith("FireOut") else (self.day_fires if day else 1.0)   # the moonlit nights with the fire out
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
    for hour in HOURS + [(h[0] + "FireOut",) + tuple(h[1:]) for h in HOURS if h[0] in FIRE_OUT_HOURS.get(PLACE, ())]:
        if WANT and hour[0] not in WANT:
            continue
        if hour[-1] and PLACE in INDOORS:                  # indoors no weather settings (the owner, 2026-11-26)
            continue
        if hour[0] in SKIP_HOURS.get(PLACE, ()):           # a place without some settings (the owner)
            continue
        ex = L.set(hour)
        fallen = []
        if hour[-1] and PLACE not in INDOORS and cam is not None:   # out of doors it falls and lies; indoors only the light changes
            fallen = weather_out(scene, cam, hour[-1])
        target = INDOOR_EYE.get(hour[0], hour[-2]) if PLACE in INDOORS else hour[-2]
        target = PLACE_EYE.get(PLACE, {}).get(hour[0], target)
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
BOUNCE_CARD = dict(day_watts=25.0, lamp_share=0.15)       # the wall facing the people: what it throws back of the day, and of the lamps' light
ROOM_BOUNCE = dict(M_OutsidePaving=2.6, M_Plaster=2.0, M_PlasterWarm=2.0, M_Boards=2.0)   # as SQUARE_BOUNCE, more: a room is lit by bounce
LIGHTS = json.load(open("C:/Projects/Wasted/World/rules/lights.json", encoding="utf-8"))   # kinds of light (FURN-066, ADR-0065)
FIRE_COLOUR = tuple(LIGHTS["colours"]["fire_red"]["colour"])   # the one red of every fire is the file's named colour


def lamp_kind(name):
    """The kind a lamp stands for, by its name (FLamp_Light.003 is a floor lamp), as Tools/citysim/lights.py has it."""
    for n, k in LIGHTS["blender_lights"].items():
        if name == n or name.startswith(n + ".") or (n.endswith("*") and name.startswith(n[:-1])):
            kind = LIGHTS["kinds"][k]                    # its colour is a named one, shared with other fittings
            return dict(kind, colour=LIGHTS["colours"][kind["colour"]]["colour"], kelvin=LIGHTS["colours"][kind["colour"]]["kelvin"])
    return None                          # how far towards grey: the boards as the town's weathered wood


def corridor(Ls):
    """A home's hallway (the owner, 2026-11-26): a corridor with no window of its own, the stair rising at its far end, lit
    by day only through the open door of a room beside it whose window faces the sun; at night by its pendant and the
    lamp on the hall table. The camera stands at the near end looking along it."""
    L = [l for l in Ls if l["key"] == "oldtown"][0]
    rng = random.Random(51)
    W, H, Y0, Y1 = 0.85, 2.7, -2.4, 7.4                       # half its width, its height, where it starts and ends
    ST = 4.6                                                  # the stair's foot
    pl, plb, bd, wd = M["plaster"], M["plaster_b"], M["boards"], M["wood"]
    box("Cor_Floor", -W, W, Y0, Y1, -0.1, 0.0, bd, bevel=0)
    box("Cor_Ceiling", -W - 0.3, W + 0.3, Y0 - 0.3, ST, H, H + 0.2, pl, bevel=0)
    box("Cor_WellNear", -W - 0.3, W + 0.45, ST - 0.3, ST, H, 2 * H + 0.4, pl, bevel=0)   # the stairwell closed above
    box("Cor_UpCeiling", -W - 0.3, W + 0.3, ST - 0.3, Y1 + 0.3, 2 * H + 0.2, 2 * H + 0.4, pl, bevel=0)   # over the stairwell
    box("Cor_Back", -W - 0.3, W + 0.3, Y0 - 0.3, Y0, 0, H, pl, bevel=0)          # behind the camera
    box("Cor_End", -W - 0.3, W + 0.3, Y1, Y1 + 0.3, 0, 2 * H + 0.2, pl, bevel=0)
    box("Cor_Left", -W - 0.3, -W, Y0, Y1, 0, 2 * H + 0.2, plb, bevel=0)
    D0, D1, DH = 0.6, 1.9, 2.2                                # the room's open door, in the right-hand wall
    box("Cor_RightA", W, W + 0.15, Y0, D0, 0, H, pl, bevel=0)
    box("Cor_RightB", W, W + 0.15, D1, Y1, 0, 2 * H + 0.2, pl, bevel=0)
    box("Cor_RightC", W, W + 0.15, D0, D1, DH, H, pl, bevel=0)
    box("Cor_DoorFrame", W - 0.02, W + 0.17, D0 - 0.06, D0, 0, DH + 0.06, wd, bevel=0.005)
    box("Cor_DoorFrame2", W - 0.02, W + 0.17, D1, D1 + 0.06, 0, DH + 0.06, wd, bevel=0.005)
    box("Cor_DoorHead", W - 0.02, W + 0.17, D0 - 0.06, D1 + 0.06, DH, DH + 0.06, wd, bevel=0.005)
    box("Cor_SkirtL", -W, -W + 0.03, Y0, Y1, 0, 0.14, wd, bevel=0.005)
    # the room beside it: its window, in the wall facing the sun, lets the day in; the street outside is sunlit
    RX0, RX1, RY0, RY1 = W + 0.15, W + 4.2, -0.6, 3.6
    box("Side_Floor", RX0, RX1, RY0, RY1, -0.1, 0.0, bd, bevel=0)
    box("Side_Ceiling", RX0, RX1 + 0.3, RY0 - 0.3, RY1 + 0.3, H, H + 0.2, pl, bevel=0)
    box("Side_Near", RX0, RX1 + 0.3, RY0 - 0.3, RY0, 0, H, pl, bevel=0)
    box("Side_Far", RX0, RX1 + 0.3, RY1, RY1 + 0.3, 0, H, pl, bevel=0)
    WY0, WY1, WZ0, WZ1 = 0.4, 2.2, 0.9, 2.3
    box("Side_WinA", RX1, RX1 + 0.4, RY0, WY0, 0, H, pl, bevel=0)
    box("Side_WinB", RX1, RX1 + 0.4, WY1, RY1, 0, H, pl, bevel=0)
    box("Side_WinC", RX1, RX1 + 0.4, WY0, WY1, 0, WZ0, pl, bevel=0)
    box("Side_WinD", RX1, RX1 + 0.4, WY0, WY1, WZ1, H, pl, bevel=0)
    box("Side_Sill", RX1 - 0.05, RX1 + 0.45, WY0, WY1, WZ0 - 0.04, WZ0, wd, bevel=0.005)
    out = bpy.data.materials.get("M_OutsidePaving") or painted("M_OutsidePaving", (0.22, 0.21, 0.25), (0.46, 0.42, 0.37), noise_scale=0.5)
    box("Side_Outside", RX1 + 0.4, RX1 + 40.0, RY0 - 30.0, RY1 + 30.0, -0.4, -0.1, out, bevel=0)
    # the stair up at the far end, along the corridor, under the open stairwell
    n = 13
    run, rise = (Y1 - 0.2 - ST) / n, H / n
    for i in range(n):
        box("Cor_Step%02d" % i, -W, W, ST + i * run, Y1 - 0.2, 0, (i + 1) * rise, wd if i % 2 == 0 else bd, bevel=0.004)
    box("Cor_Landing", -W, W, Y1 - 0.2, Y1, 0, H + 0.02, wd, bevel=0.004)
    # the hall's pieces against its left wall, a pendant overhead, a rug
    for spec, x, y, yaw in (("h_coatstand", -W + 0.3, 3.9, math.pi / 2), ("h_console", -W + 0.2, 2.4, math.pi / 2),
                            ("h_mirror", -W + 0.05, 2.4, math.pi / 2), ("k17:rug", 0.0, 1.0, 0.0), ("d_pendant", 0.0, 1.2, 0.0)):
        P = group("Hall_%s" % spec.replace(":", "_"), x, y, yaw)
        place(spec, P, L, rng)
    people([("PlayerA", -0.2, 3.0)])
    sun_and_sky(SCENE, (0.78, 0.30, 0.42), 8.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.25)
    return camera(SCENE, "Cam_hallway", (-0.5, Y0 + 0.2, 1.6), (0.35, Y1, 1.5), 16.0)


AWNING_IN = ("Morning",)                                  # the shop's awning rolled up while the sun is low (the owner, 2026-11-26)


MALL_LOT, MALL_MID = ("b_16_5", "b_16_5_l0"), (1680.0, 678.0)   # Eastgate Mall's lot, and the middle of its court
MALL_LOOK = dict(brass=(0.78, 0.60, 0.32), glass_tint=(0.80, 0.90, 0.92), glass_alpha=0.18, column=(0.86, 0.82, 0.74),
                 globe=(1.0, 0.90, 0.74), globe_glow=8.0, globes=12, chandelier_drop=5.5, banner_drop=6.5, column_every=16.0,
                 front_glass=3.0, mullion_every=3.4, mullion=(0.22, 0.22, 0.24),   # the game's outlet fronts
                 shell_tones=(((0.80, 0.72, 0.60), (0.82, 0.74, 0.62)),),   # the ribbon wall: one even honed cream stone
                 shell_groove=(0.26, 0.21, 0.17), shell_lap=0.07,
                 egg=(0.74, 0.54, 0.26), egg_bar=0.12, egg_counter=(0.46, 0.32, 0.20), egg_seat=(0.36, 0.42, 0.40), egg_floor_warm=(0.56, 0.40, 0.27), egg_floor_deep=(0.36, 0.27, 0.21),   # the egg hall: the lattice's gold, its bars' width of a cell, the stalls' timber
                 floods=dict(out=12.0, along=(-62.0, -48.0, -34.0, 34.0, 48.0, 62.0), crown=(-7.0, 7.0), wall_aim=13.0, crown_aim=19.0, beam=38.0),   # the forecourt's floodlights             # the dark groove, and how far a ribbon laps
                 forecourt=((0.52, 0.42, 0.33), (0.66, 0.55, 0.43)), door_side=3.4, door_rise=2.4,   # the doorway's straight sides, then its curve
                 signs=dict(terracotta=(0.66, 0.32, 0.22), teal=(0.20, 0.44, 0.44), blue=(0.26, 0.36, 0.56), plum=(0.44, 0.28, 0.44),
                            ochre=(0.74, 0.58, 0.26), olive=(0.42, 0.46, 0.24), cream=(0.84, 0.78, 0.62), red=(0.62, 0.20, 0.18),
                            green=(0.26, 0.44, 0.28), pink=(0.80, 0.50, 0.56), brown=(0.44, 0.30, 0.20), grey=(0.46, 0.46, 0.48)),
                 banners=((0.86, 0.42, 0.26), (0.30, 0.58, 0.60), (0.92, 0.74, 0.36), (0.62, 0.46, 0.66)),
                 flowers=((0.78, 0.28, 0.30), (0.92, 0.78, 0.36), (0.86, 0.86, 0.82), (0.58, 0.40, 0.66)), flowers_per=60, banners_per=6,
                 slat=((0.36, 0.21, 0.11), (0.48, 0.29, 0.15)), slat_every=0.25,   # fine, soft slats
                 stone=((0.76, 0.74, 0.70), (0.77, 0.75, 0.71)), stone_rough=0.08, joint=0.0012, joint_colour=(0.70, 0.68, 0.64),   # joints hidden: one uniform floor (the owner)
                 sky_glass=(0.80, 0.86, 0.88), sky_glass_alpha=0.5,   # the roof lights lightly tinted, holding back some of the sun
                 brown=((0.30, 0.17, 0.09), (0.46, 0.28, 0.15)),   # the galleries' undersides, mottled warm brown
                 cream=(0.80, 0.74, 0.62), wall=(0.82, 0.80, 0.76), kerb=(0.86, 0.85, 0.82),
                 tread=(0.20, 0.21, 0.24), strip=(1.0, 0.88, 0.70), strip_glow=6.0,
                 tiles=((0.86, 0.86, 0.85), (0.70, 0.71, 0.71)), tile=1.2,   # the pale checker of the floor, 1.2 m tiles
                 hang_every=9.0, lattice=True)


def mall_slats():
    """Warm timber slats under the galleries: bands of two browns every slat_every metres."""
    m = bpy.data.materials.get("M_MallSlats")
    if m:
        return m
    m = bpy.data.materials.new("M_MallSlats")
    m.use_nodes = True
    nt = m.node_tree
    b = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    wv = nt.nodes.new("ShaderNodeTexWave")
    wv.wave_type = 'BANDS'
    wv.inputs["Scale"].default_value = 1.0
    wv.inputs["Distortion"].default_value = 0.0
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sc = nt.nodes.new("ShaderNodeVectorMath")
    sc.operation = 'SCALE'
    sc.inputs["Scale"].default_value = 1.0 / (MALL_LOOK["slat_every"] * 6.3)
    nt.links.new(tc.outputs["Object"], sc.inputs[0])
    nt.links.new(sc.outputs[0], wv.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    a, c = MALL_LOOK["slat"]
    ramp.color_ramp.elements[0].color = (*a, 1.0)
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[1].color = (*c, 1.0)
    ramp.color_ramp.elements[1].position = 0.42
    nt.links.new(wv.outputs["Fac"], ramp.inputs[0])
    nt.links.new(ramp.outputs[0], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = 0.6
    return m


def mall_stone():
    """The court's floor: large polished stone tiles (the owner, 2026-11-26: "mall floors are usually smoother and shiny,
    use high quality polished stone or high quality tiles"): 1.2 m squares in two close tones with soft veining, fine
    joints, smooth and shiny."""
    m = bpy.data.materials.get("M_MallStone")
    if m:
        return m
    ML = MALL_LOOK
    m = bpy.data.materials.new("M_MallStone")
    m.use_nodes = True
    nt = m.node_tree
    b = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    tc = nt.nodes.new("ShaderNodeTexCoord")
    br = nt.nodes.new("ShaderNodeTexBrick")                    # square tiles, not offset
    br.offset = 0.0
    br.inputs["Scale"].default_value = 1.0
    br.inputs["Brick Width"].default_value = ML["tile"]
    br.inputs["Row Height"].default_value = ML["tile"]
    br.inputs["Mortar Size"].default_value = ML["joint"]
    br.inputs["Mortar Smooth"].default_value = 0.2
    a, c = ML["stone"]
    br.inputs["Color1"].default_value = (*a, 1.0)
    br.inputs["Color2"].default_value = (*c, 1.0)
    br.inputs["Mortar"].default_value = (*ML["joint_colour"], 1.0)
    nt.links.new(tc.outputs["Object"], br.inputs["Vector"])
    nz = nt.nodes.new("ShaderNodeTexNoise")                    # the stone's soft veins
    nz.inputs["Scale"].default_value = 0.6
    nz.inputs["Detail"].default_value = 8.0
    nz.inputs["Distortion"].default_value = 3.0
    nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = 'RGBA'
    mix.blend_type = 'MULTIPLY'
    vr = nt.nodes.new("ShaderNodeMapRange")
    vr.inputs["From Min"].default_value, vr.inputs["From Max"].default_value = 0.35, 0.75
    vr.inputs["To Min"].default_value, vr.inputs["To Max"].default_value = 0.88, 1.0
    nt.links.new(nz.outputs["Fac"], vr.inputs["Value"])
    nt.links.new(br.outputs["Color"], mix.inputs[6])
    nt.links.new(vr.outputs["Result"], mix.inputs[7])
    mix.inputs["Factor"].default_value = 1.0
    nt.links.new(mix.outputs[2], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = ML["stone_rough"]      # polished: it mirrors the light
    b.inputs["Specular IOR Level"].default_value = 0.6
    return m


def mall_fabric(name, c):
    """Thin fabric that the daylight behind it shines through (a banner under a roof light)."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
    b = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    b.inputs["Base Color"].default_value = (*c, 1.0)
    b.inputs["Roughness"].default_value = 0.9
    tr = nt.nodes.new("ShaderNodeBsdfTranslucent")
    tr.inputs["Color"].default_value = (*c, 1.0)
    mx = nt.nodes.new("ShaderNodeMixShader")
    mx.inputs["Fac"].default_value = 0.6
    nt.links.new(b.outputs[0], mx.inputs[1])
    nt.links.new(tr.outputs[0], mx.inputs[2])
    nt.links.new(mx.outputs[0], out.inputs["Surface"])
    return m


def mall_metal(name, c, rough):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    b.inputs["Base Color"].default_value = (*c, 1.0)
    b.inputs["Metallic"].default_value = 1.0
    b.inputs["Roughness"].default_value = rough
    return m


def mall_glass(name, c, alpha):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    b.inputs["Base Color"].default_value = (*c, 1.0)
    b.inputs["Roughness"].default_value = 0.04
    b.inputs["Alpha"].default_value = alpha
    setp(m, "blend_method", 'BLEND')
    return m


GOODS = dict(books=((0.55, 0.20, 0.16), (0.18, 0.32, 0.36), (0.62, 0.52, 0.28), (0.30, 0.24, 0.40), (0.80, 0.76, 0.66)),
             folded=((0.70, 0.66, 0.58), (0.30, 0.38, 0.50), (0.62, 0.30, 0.26), (0.40, 0.46, 0.30)),
             shoes=((0.30, 0.20, 0.14), (0.80, 0.78, 0.74), (0.18, 0.18, 0.20), (0.56, 0.36, 0.22)),
             gadgets=((0.16, 0.17, 0.19), (0.72, 0.74, 0.76), (0.24, 0.40, 0.58)),
             boxes=((0.76, 0.62, 0.42), (0.62, 0.30, 0.22), (0.24, 0.42, 0.50), (0.84, 0.74, 0.36)),
             jars=((0.80, 0.70, 0.40), (0.66, 0.30, 0.22), (0.86, 0.84, 0.76)),
             pots=((0.66, 0.40, 0.28), (0.80, 0.78, 0.72), (0.36, 0.46, 0.40)),
             lamps=((0.92, 0.86, 0.70), (0.50, 0.36, 0.24)),
             bread=((0.70, 0.48, 0.24), (0.56, 0.34, 0.16)),
             bottles=((0.26, 0.40, 0.26), (0.60, 0.22, 0.20), (0.82, 0.78, 0.60)),
             balls=((0.88, 0.86, 0.80), (0.70, 0.30, 0.20), (0.24, 0.40, 0.60)),
             plush=((0.72, 0.52, 0.36), (0.86, 0.70, 0.74), (0.60, 0.72, 0.80)),
             produce=((0.70, 0.16, 0.12), (0.88, 0.50, 0.12), (0.86, 0.74, 0.20), (0.36, 0.56, 0.20), (0.24, 0.40, 0.16),
                      (0.52, 0.20, 0.36), (0.80, 0.62, 0.40), (0.90, 0.84, 0.70)))


def mall_sign_mat(name):
    c = MALL_LOOK["signs"].get(name, MALL_LOOK["signs"]["teal"])
    nm = "M_MallSign_%s" % name
    return bpy.data.materials.get(nm) or flat(nm, c)


class GoodsMesh:
    """Many small goods as one object (the owner, 2026-11-27, "something is wrong" with how long a build takes: 30,000
    goods made one object each took most of the six minutes): boxes in the frame of P_, each with its own material."""
    def __init__(self, P_, name):
        self.P, self.name, self.bm, self.mats = P_, name, bmesh.new(), []

    def add(self, mat, cx, cy, cz, sx, sy, sz):
        if mat not in self.mats:
            self.mats.append(mat)
        k = self.mats.index(mat)
        r = bmesh.ops.create_cube(self.bm, size=1.0)
        for v in r["verts"]:
            v.co.x, v.co.y, v.co.z = cx + v.co.x * sx, cy + v.co.y * sy, cz + v.co.z * sz
        for f in {f for v in r["verts"] for f in v.link_faces}:
            f.material_index = k

    def cyl(self, mat, x, y, z0, r, h, segs=16):
        if mat not in self.mats:
            self.mats.append(mat)
        k = self.mats.index(mat)
        res = bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False, segments=segs, radius1=r, radius2=r, depth=h)
        for v in res["verts"]:
            v.co.x, v.co.y, v.co.z = v.co.x + x, v.co.y + y, v.co.z + z0 + h / 2
        for f in {f for v in res["verts"] for f in v.link_faces}:
            f.material_index = k

    def done(self):
        if not self.mats:
            self.bm.free()
            return None
        me = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(me)
        self.bm.free()
        for m in self.mats:
            me.materials.append(m)
        ob = bpy.data.objects.new(self.name, me)
        COL.objects.link(ob)
        if self.P is not None:
            ob.parent = self.P
        return ob


def goods_mat(kind, i):
    cs = GOODS.get(kind, GOODS["boxes"])
    c = cs[i % len(cs)]
    nm = "M_Goods_%s_%d" % (kind, i % len(cs))
    return bpy.data.materials.get(nm) or flat(nm, c)


def mall_outlet(u, a, b, z, kind, Ls, rng, mats):
    """An outlet behind its front, as the plan has it (its unit, its kind's walls, middle and window from
    World/rules/mall_outlets.json): a room as wide as its front and up to 9 m deep (outlet_depth), its own floor, walls
    and ceiling, lights on all day; on its walls, in its middle and in its window what its kind sells, made from the
    furniture kit's pieces where the kit has them and else from simple shapes in the goods' colours."""
    glassp, brass = mats
    L = [l for l in Ls if l["key"] == "plain"][0]
    W = math.hypot(b[0] - a[0], b[1] - a[1])
    if W < 2.0:
        return
    ang = math.radians(u.get("angle", 0.0))
    n = (math.sin(ang), -math.cos(ang))                       # the way the outlet faces, out into the walk
    rx0, rz0, rx1, rz1 = u["rect"]
    D = min(9.0, max(4.2, min(rx1 - rx0, rz1 - rz0)))
    cx, cy = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    yaw = math.atan2(n[0], -n[1])                              # local +y runs in from the front, local x along it
    G = group("Out_%s" % u["id"], cx, cy, yaw)
    G.location.z = z
    wall = bpy.data.materials.get("M_OutWall") or flat("M_OutWall", (0.84, 0.81, 0.75))
    fl = bpy.data.materials.get("M_OutBoards") if kind.get("floor") == "boards" else bpy.data.materials.get("M_OutTiles")
    if fl is None:
        fl = (painted("M_OutBoards", (0.40, 0.28, 0.17), (0.54, 0.40, 0.26), brick_scale=0.25, floor=True, bump=0.2, noise_scale=1.6)
              if kind.get("floor") == "boards" else flat("M_OutTiles", (0.80, 0.79, 0.76), rough=0.25))
    H = MALL_LOOK["front_glass"]
    if u.get("name"):                                          # its name on the sign band, in cream letters
        km = globals().setdefault("K_M", {})
        km["MallLetters"] = bpy.data.materials.get("M_MallLetters") or flat("M_MallLetters", (0.94, 0.90, 0.80))
        nm_ = text("Out_Name_%s" % u["id"], u["name"], cx + n[0] * 0.17, cy + n[1] * 0.17, z + H + 0.36, min(0.42, W / max(6, len(u["name"])) * 1.2), "MallLetters", depth=0.02)
        nm_.rotation_euler = (math.pi / 2, 0, ang)
    rbox(G, "Out_Floor_%s" % u["id"], fl, 0, D / 2, 0.015, W, D, 0.03, 0.0, 1)
    rbox(G, "Out_Back_%s" % u["id"], wall, 0, D, H / 2, W, 0.2, H, 0.0, 1)
    for sx in (-1, 1):
        rbox(G, "Out_Side%d_%s" % (sx, u["id"]), wall, sx * W / 2, D / 2, H / 2, 0.2, D, H, 0.0, 1)
    ceil = bpy.data.materials.get("M_OutCeiling") or flat("M_OutCeiling", (0.92, 0.91, 0.88))
    panel = bpy.data.materials.get("M_OutPanel") or flat("M_OutPanel", (0.62, 0.44, 0.28))
    striplit = bpy.data.materials.get("M_OutStripLit") or emissive("M_OutStripLit", (1.0, 0.96, 0.90), 5.0)   # "Lit": on all day
    rbox(G, "Out_Ceil_%s" % u["id"], ceil, 0, D / 2, H + 0.05, W, D, 0.1, 0.0, 1)
    rbox(G, "Out_PanelBack_%s" % u["id"], panel, 0, D - 0.12, 1.3, W - 0.2, 0.04, 2.6, 0.0, 1)   # wood panelling on its walls
    for sx in (-1, 1):
        rbox(G, "Out_PanelSide%d_%s" % (sx, u["id"]), panel, sx * (W / 2 - 0.12), D / 2, 1.3, 0.04, D - 0.2, 2.6, 0.0, 1)
    for j, yy in enumerate((D * 0.22, D * 0.5, D * 0.78)):  # long strip lights across the ceiling, as the game's
        rbox(G, "Out_Strip_%s_%d" % (u["id"], j), striplit, 0, yy, H - 0.02, W * 0.8, 0.12, 0.04, 0.0, 1)
        for i in range(max(1, int(W / 5))):
            lx = -W * 0.4 + (i + 0.5) * W * 0.8 / max(1, int(W / 5))
            lt = bpy.data.lights.new("ShopIn_%s_%d_%d" % (u["id"], j, i), 'POINT')
            lt.energy = 90.0
            lt.color = (1.0, 0.94, 0.86)
            setp(lt, "shadow_soft_size", 0.6)
            po = bpy.data.objects.new("ShopIn_%s_%d_%d" % (u["id"], j, i), lt)
            po.parent = G
            po.location = (lx, yy, H - 0.25)
            COL.objects.link(po)

    def thing(tok, x, y, face_in):
        """One of the kind's things at (x, y) in the outlet's own frame; face_in: its back to the back wall."""
        P_ = group("Out_%s_%d_%d" % (u["id"], int((x + 50) * 10), int(y * 10)), 0, 0, 0 if face_in else math.pi)
        P_.parent = G
        P_.location = (x, y, 0)
        thing_at(tok, P_)

    def thing_at(tok, P_):
        k_, _, what = tok.partition(":")
        if k_ in ("rail", "roundrack"):
            place("p_rail", P_, L, rng)
        elif k_ == "mannequins":
            for dx in (-0.5, 0.5):
                Q = group(P_.name + "_m%d" % int(dx * 10), 0, 0, 0)
                Q.parent = P_
                Q.location = (dx, 0, 0)
                place("p_form", Q, L, rng)
        elif k_ == "bench":
            place("d_bench", P_, L, rng)
        elif k_ in ("plants", "buckets") or (k_ == "plinth" and what == "plant"):
            place("o_floorplant", P_, L, rng)
        elif k_ == "mirror":
            rbox(P_, P_.name + "_Mirror", glassp, 0, -0.05, 1.3, 0.9, 0.04, 1.8, 0.02, 1)
            rbox(P_, P_.name + "_Frame", brass, 0, -0.02, 1.3, 1.0, 0.03, 1.9, 0.02, 1)
        elif k_ == "servery":
            place("f_counter", P_, L, rng)
        elif k_ == "cafetable":
            place("f_table", P_, L, rng)
        elif k_ == "shelves" or k_ in ("guitars", "rackets", "hockey", "cabinets", "screens", "posters"):
            sw = min(3.0, W / 3 - 0.3)                           # a shelving unit full of the kind's goods
            frame = bpy.data.materials.get("M_OutShelf") or flat("M_OutShelf", (0.46, 0.34, 0.22))
            rbox(P_, P_.name + "_Back", frame, 0, -0.05, 1.2, sw, 0.06, 2.4, 0.0, 1)
            for sz in (0.1, 0.65, 1.2, 1.75, 2.3):
                rbox(P_, P_.name + "_Shelf%d" % int(sz * 10), frame, 0, -0.25, sz, sw, 0.42, 0.04, 0.0, 1)
            gk = what or {"guitars": "boxes", "screens": "gadgets", "posters": "boxes"}.get(k_, "boxes")
            gm = GoodsMesh(P_, P_.name + "_Goods")
            for sz in (0.14, 0.69, 1.24, 1.79):
                gx = -sw / 2 + 0.1
                j = 0
                while gx < sw / 2 - 0.15:
                    gw = rng.uniform(0.12, 0.32) if gk != "books" else rng.uniform(0.04, 0.08)
                    gh = rng.uniform(0.18, 0.42)
                    gm.add(goods_mat(gk, j + int(sz * 7)), gx + gw / 2, -0.25, sz + gh / 2, gw, 0.3, gh)
                    gx += gw + 0.03
                    j += 1
            gm.done()
        elif k_ in ("table", "glasscase", "screen", "amps", "drums", "keyboard", "claw", "bin", "guitarstand"):
            place("p_table", P_, L, rng)
            gk = what or {"screen": "gadgets", "amps": "gadgets", "keyboard": "gadgets"}.get(k_, "boxes")
            gm = GoodsMesh(P_, P_.name + "_Goods")
            for j in range(6):
                gm.add(goods_mat(gk, j), -0.5 + (j % 3) * 0.5, -0.15 + (j // 3) * 0.3, 0.8, 0.32, 0.22, rng.uniform(0.06, 0.2))
            gm.done()
            if k_ == "glasscase":
                rbox(P_, P_.name + "_Case", glassp, 0, 0, 1.0, 1.3, 0.7, 0.45, 0.01, 1)
        else:                                                    # stacks and plinths: something to look at in the window
            rbox(P_, P_.name + "_Plinth", wall, 0, 0, 0.35, 0.7, 0.7, 0.7, 0.02, 1)
            gk = what if what in GOODS else "boxes"
            gm = GoodsMesh(P_, P_.name + "_Goods")
            for j in range(3):
                gm.add(goods_mat(gk, j), 0, 0, 0.78 + j * 0.24, 0.5 - j * 0.1, 0.5 - j * 0.1, 0.22)
            gm.done()
    walls = kind.get("walls", ["shelves:boxes"] * 3) or ["shelves:boxes"]
    nb = max(1, int(W / 3.3))                                 # the back wall lined all along, the kind's wall things in turn
    for i in range(nb):
        thing(walls[i % len(walls)], -W / 2 + (i + 0.5) * W / nb, D - 0.15, True)
    for sx in (-1, 1):                                        # and the side walls, back to front
        ns = max(1, int((D - 2.5) / 3.3))
        for i in range(ns):
            y_ = D - 1.9 - i * 3.3
            if y_ < 1.6:
                break
            Pw = group("Out_%s_side%d_%d" % (u["id"], sx, i), 0, 0, 0)
            Pw.parent = G
            Pw.location = (sx * (W / 2 - 0.15), y_, 0)
            Pw.rotation_euler = (0, 0, -sx * math.pi / 2)
            thing_at(walls[(i + 1) % len(walls)], Pw)
    if kind.get("trade") != "food":
        Pc = group("Out_%s_counter" % u["id"], 0, 0, 0)
        Pc.parent = G
        Pc.location = (0.0, D * 0.55, 0)
        place("p_counter", Pc, L, rng)
    mid = kind.get("middle", []) or ["table:boxes"]
    cols = max(1, int((W - 3.0) / 3.6))                       # rows of the middle things
    for r_, yy in enumerate((D * 0.32, D * 0.72)):
        for i in range(cols):
            x_ = -W / 2 + 1.5 + (i + 0.5) * (W - 3.0) / cols
            if abs(x_) < 1.6 and r_ == 0 and kind.get("trade") != "food":
                continue                                          # the counter stands there
            thing(mid[(i + r_) % len(mid)], x_, yy, True)
    win = kind.get("window", []) or ["stack:boxes"]
    nw = max(2, int(W / 3.0))                                 # the window, a display every 3 m, facing out
    for i in range(nw):
        thing(win[i % len(win)], -W / 2 + (i + 0.5) * W / nw, 0.85, False)


def mall_forecourt(blk, loc, strip):
    """The forecourt before the south arch as the game has it: warm paving, the block's trees where its data puts them,
    and over the doors a round-topped head with a line of light along its curve."""
    pave = bpy.data.materials.get("M_Forecourt") or painted("M_Forecourt", *MALL_LOOK["forecourt"], brick_scale=0.4, floor=True, bump=0.3, noise_scale=0.8)
    box("Fore_Paving", -90.0, 90.0, loc((0, 848.4))[1], loc((0, 990.0))[1], -0.12, -0.01, pave, bevel=0)
    leaf = bpy.data.materials.get("M_Leaf") or flat("M_Leaf", (0.24, 0.36, 0.20), noise_scale=4.0)
    leaf2 = bpy.data.materials.get("M_LeafLight") or flat("M_LeafLight", (0.40, 0.48, 0.24), noise_scale=4.0)
    bark = bpy.data.materials.get("M_MallBrownSide") or flat("M_MallBrownSide", (0.46, 0.28, 0.15))
    n = 0
    for t in blk.get("trees", []):
        x, z = t["pos"]
        if abs(x - 1680) < 95 and 866 < z < 990:
            X, Y = loc((x, z))
            cylz(None, "Fore_Trunk%d" % n, bark, X, Y, 0.0, 0.28, 4.2, r_top=0.18)
            for (dx, dy, dz, r_, m_) in ((0, 0, 5.6, 2.3, leaf), (1.5, 0.5, 5.0, 1.7, leaf), (-1.4, 0.4, 5.1, 1.6, leaf2),
                                         (0.4, -1.2, 4.9, 1.5, leaf), (0.2, 0.2, 7.0, 1.4, leaf2)):
                ball(None, "Fore_Leaf%d" % n, m_, X + dx, Y + dy, dz, 2 * r_, 2 * r_, 1.6 * r_)
            n += 1
    hlog("  the forecourt: %d trees from the block" % n)


def mall_floods(loc):
    """Floodlights set in the forecourt's paving, out from the mall, throwing light up its ribbon wall from dusk (the
    owner, 2026-11-27: "place flood light on the ground away from the mall casting light on the dome"): a row along the
    wall either side of the arch, aimed at its middle height, and a pair before the arch aimed at its crown. Their kind,
    colour and strength are lights.json's ground_flood."""
    F = MALL_LOOK["floods"]
    wall_y = loc((0, 862.0))[1]
    y = wall_y + F["out"]
    case = bpy.data.materials.get("M_FloodCase") or flat("M_FloodCase", (0.20, 0.20, 0.21), rough=0.4)
    lens = bpy.data.materials.get("M_FloodLens") or emissive("M_FloodLens", (1.0, 0.92, 0.80), 6.0)   # a lamp's glass: lit with the lamps
    aims = [(x, wall_y, F["wall_aim"]) for x in F["along"]] + [(x, wall_y, F["crown_aim"]) for x in F["crown"]]
    for i, (ax, ay, az) in enumerate(aims):
        x = ax if abs(ax) > 20 else ax * 1.6
        box("Flood_Case%d" % i, x - 0.25, x + 0.25, y - 0.2, y + 0.2, -0.02, 0.16, case, bevel=0)   # sunk flush in the paving
        box("Flood_Lens%d" % i, x - 0.2, x + 0.2, y - 0.15, y + 0.15, 0.16, 0.18, lens, bevel=0)
        lt = bpy.data.lights.new("Fd_Light%d" % i, 'SPOT')
        lt.energy = 1.0                                       # set from its kind when the place's lights are read
        lt.spot_size = math.radians(F["beam"])
        lt.spot_blend = 0.7
        setp(lt, "shadow_soft_size", 0.15)
        po = bpy.data.objects.new("Fd_Light%d" % i, lt)
        po.location = (x, y, 0.3)
        d = Vector((ax - x, ay - y, az - 0.3))
        po.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
        COL.objects.link(po)
    hlog("  floodlights: %d on the forecourt" % len(aims))


def sandstone(name, c1, c2):
    """The ribbon wall's finish: one even, smooth, honed cream stone with a soft satin sheen, no patches (the owner,
    2026-11-27: "we shouldn't have patches. Malls are posh"). c1 is kept for the call's shape; c2 is the colour."""
    m, nt, bsdf = new_mat(name)
    sset(bsdf, "Base Color", rgba(c2))
    sset(bsdf, "Roughness", 0.32)
    sset(bsdf, "Specular IOR Level", 0.45)
    return m


def mall_porch(shell, loc, brass, globe):
    """The entry point as the game has it (World/city_01/blocks/b_16_5.json shell, Tools/worldgen/mall_shell_01.py,
    ADR-0037): the shell's ribbon wall 14 m out from the hall, lifted over the south door into an arch 44 m wide and
    12.5 m high; under it a porch, its soffit following the arch back to the hall's front, lanterns hanging in it; the
    wall's ribbons in one even honed stone, each lapping the one below over a dark groove. Only the stretch round the south arch is built."""
    pts = sorted([p_ for p_ in shell["skin"]["points"] if p_[1] > 840 and abs(p_[0] - 1680) < 78], key=lambda q: q[0])
    sk = shell["skin"]
    a_, b_ = sk["colors"]
    cols = MALL_LOOK["shell_tones"]
    # one even stone for every ribbon, as the slice's pattern but posh: no patches (the owner, 2026-11-27: the
    # alternate bands do not look good; "Malls are posh"); each ribbon laps a little over the one below, so a thin shadow and a dark
    # groove mark the line between them
    one = bpy.data.materials.get("M_Shell0") or sandstone("M_Shell0", cols[0][0], cols[0][1])
    tone = [one, one]
    joint = bpy.data.materials.get("M_ShellGroove") or flat("M_ShellGroove", MALL_LOOK["shell_groove"])
    roll, belly, bands = sk["roll"], sk["belly"], sk["bands"]
    hall_y = loc((0, 848.0))[1]

    def at(x, y, b0, t0, f):
        """A point on the wall at height fraction f between its foot b0 and top t0: swelling out half way, curling in at the top."""
        h = b0 + (t0 - b0) * f
        out = belly * math.sin(math.pi * f) if b0 <= 0.01 else 0.0
        curl = 0.0
        if h > t0 - roll:
            g = (h - (t0 - roll)) / roll
            curl = roll * (1 - math.cos(g * math.pi / 2)) * 0.9
        return (x, y + out - curl, h)
    NB = bands
    bm_s, bm_g = bmesh.new(), bmesh.new()                       # the wall's stone and its grooves, one object each
    for i in range(len(pts) - 1):
        (x0, z0, b0, t0), (x1, z1, b1, t1) = pts[i], pts[i + 1]
        (X0, Y0), (X1, Y1) = loc((x0, z0)), loc((x1, z1))
        for j in range(NB):                                       # a ribbon at a time, the two tones in turn
            f0, f1 = j / NB, (j + 1) / NB
            lap = MALL_LOOK["shell_lap"]                          # its foot stands proud of the ribbon below

            def low(q):
                return (q[0], q[1] + lap, q[2])
            v = [bm_s.verts.new(c) for c in (low(at(X0, Y0, b0, t0, f0)), low(at(X1, Y1, b1, t1, f0)), at(X1, Y1, b1, t1, f1), at(X0, Y0, b0, t0, f1))]
            bm_s.faces.new(v)
            if j > 0:                                             # the groove under its lapped foot
                q0, q1 = at(X0, Y0, b0, t0, f0), at(X1, Y1, b1, t1, f0)
                gy = lap * 0.5
                v = [bm_g.verts.new(c) for c in ((q0[0], q0[1] + gy, q0[2] - 0.035), (q1[0], q1[1] + gy, q1[2] - 0.035),
                                                 (q1[0], q1[1] + gy, q1[2] + 0.01), (q0[0], q0[1] + gy, q0[2] + 0.01))]
                bm_g.faces.new(v)
        if b0 > 0.01 and b1 > 0.01:                               # the porch's soffit, from the arch back to the hall's front
            bm = bmesh.new()
            v = [bm.verts.new(c) for c in ((X0, Y0, b0), (X1, Y1, b1), (X1, hall_y + 0.5, b1 * 0.96), (X0, hall_y + 0.5, b0 * 0.96))]
            bm.faces.new(v)
            me = bpy.data.meshes.new("Porch_Soffit%d" % i)
            bm.to_mesh(me)
            bm.free()
            me.materials.append(tone[1])
            COL.objects.link(bpy.data.objects.new("Porch_Soffit%d" % i, me))
    for nm, bm_, m_ in (("Shell_Ribbons", bm_s, one), ("Shell_Grooves", bm_g, joint)):
        me = bpy.data.meshes.new(nm)
        bm_.to_mesh(me)
        bm_.free()
        me.materials.append(m_)
        COL.objects.link(bpy.data.objects.new(nm, me))
    for k, x in enumerate((-9.0, 0.0, 9.0)):                      # lanterns hanging in the porch
        y = (hall_y + loc((0, 862.0))[1]) / 2
        cylz(None, "Porch_LanternRod%d" % k, brass, x, y, 8.2, 0.04, 3.6, segs=8)
        ball(None, "Porch_Lantern%d" % k, globe, x, y, 8.0, 0.9, 0.9, 0.9)
        lt = bpy.data.lights.new("Hall_LanternP%d_Light" % k, 'POINT')
        lt.energy = 400.0
        setp(lt, "shadow_soft_size", 0.4)
        po = bpy.data.objects.new("Hall_LanternP%d_Light" % k, lt)
        po.location = (x, y, 7.7)
        COL.objects.link(po)


def mall_entrance_doors(P, loc, SH, glassp, brass, edge, strip, globe, slab_under):
    """The main (south) entrance in the court's look (ADR-0069): the hall's wall across the end of the concourse with the
    plan's opening in it, glazed doors in brass frames under a glass transom, the sunlit street outside; pendant globes
    down the middle of the concourse; a line of light along its ceiling."""
    e = next(a for a in P["entrances"] if a["side"] == "s")
    (ex, ey), half = loc(e["at"]), e["width"] / 2.0
    ML = MALL_LOOK
    # the doorway as the game draws it (Art/StyleTests/unreal_mall_entrance_55): a broad opening whose straight sides rise
    # into a wide curved top, cut through the hall's front; glazed to the same curve with thin brass mullions; a line of
    # light all round its edge
    sideh, rise = ML["door_side"], ML["door_rise"]
    outline = [(ex + half, 0.0), (ex + half, sideh)]
    for i in range(1, 32):
        a = math.pi * i / 32
        outline.append((ex + half * math.cos(a), sideh + rise * math.sin(a)))
    outline += [(ex - half, sideh), (ex - half, 0.0)]

    def upright(name, loop, holes, y, mat):
        bm = bmesh.new()
        for lp in [loop] + holes:
            vs = [bm.verts.new((x, y, z)) for x, z in lp]
            for i in range(len(vs)):
                bm.edges.new((vs[i], vs[(i + 1) % len(vs)]))
        bmesh.ops.triangle_fill(bm, use_beauty=True, use_dissolve=False, edges=bm.edges[:])
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(mat)
        ob = bpy.data.objects.new(name, me)
        COL.objects.link(ob)
        return ob
    tone = bpy.data.materials.get("M_Shell0") or sandstone("M_Shell0", ML["shell_tones"][0][0], ML["shell_tones"][0][1])
    front = [(ex - 60.0, 0.0), (ex + 60.0, 0.0), (ex + 60.0, 18.0), (ex - 60.0, 18.0)]
    upright("MallE_Front", front, [outline], ey, tone)                      # the hall's front, the doorway cut through it
    upright("MallE_FrontIn", front, [outline], ey + 0.5, tone)
    g = upright("MallE_Glass", outline, [], ey + 0.25, glassp)              # glazed to the same curve
    setp(g, "visible_shadow", False)
    bm = bmesh.new()                                                       # the reveal: the opening's curved edge, deep
    for i in range(len(outline) - 1):
        (xa, za), (xb, zb) = outline[i], outline[i + 1]
        v = [bm.verts.new(c) for c in ((xa, ey, za), (xb, ey, zb), (xb, ey + 0.5, zb), (xa, ey + 0.5, za))]
        bm.faces.new(v)
    me = bpy.data.meshes.new("MallE_Reveal")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(edge)
    COL.objects.link(bpy.data.objects.new("MallE_Reveal", me))
    bm = bmesh.new()                                                       # the line of light round the edge, outside
    for i in range(1, len(outline) - 2):
        (xa, za), (xb, zb) = outline[i], outline[i + 1]
        dx, dz = xb - xa, zb - za
        n_ = math.hypot(dx, dz) or 1.0
        ox, oz = -dz / n_ * 0.14, dx / n_ * 0.14
        v = [bm.verts.new(c) for c in ((xa, ey - 0.02, za), (xb, ey - 0.02, zb), (xb + ox, ey - 0.02, zb + oz), (xa + ox, ey - 0.02, za + oz))]
        bm.faces.new(v)
    me = bpy.data.meshes.new("MallE_EdgeLight")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(strip)
    COL.objects.link(bpy.data.objects.new("MallE_EdgeLight", me))
    n = int(round(2 * half / 2.0))
    for i in range(1, n):                                                  # thin brass mullions up to the curve
        x = ex - half + i * 2 * half / n
        top = sideh + rise * math.sqrt(max(0.0, 1 - ((x - ex) / half) ** 2))
        box("MallE_Mullion%d" % i, x - 0.04, x + 0.04, ey + 0.2, ey + 0.3, 0.0, top, brass, bevel=0)
    box("MallE_Transom", ex - half, ex + half, ey + 0.2, ey + 0.3, 2.6, 2.66, brass, bevel=0)
    for i in range(n):                                                     # the doors' push rails
        x = ex - half + (i + 0.5) * 2 * half / n
        box("MallE_Push%d" % i, x - 0.4, x + 0.4, ey + 0.12, ey + 0.16, 1.0, 1.05, brass, bevel=0)
    out = bpy.data.materials.get("M_OutsidePaving") or painted("M_OutsidePaving", (0.22, 0.21, 0.25), (0.46, 0.42, 0.37), noise_scale=0.5)
    box("MallE_Street", ex - 60.0, ex + 60.0, ey + 0.5, ey + 60.0, -0.4, -0.02, out, bevel=0)
    c = next(w for w in P["levels"][0]["walks"] if len(w) == 4 and min(q[0] for q in w) < e["at"][0] < max(q[0] for q in w)
             and max(q[1] for q in w) >= e["at"][1] - 1)
    q = [loc(p_) for p_ in c]
    y0, y1 = min(y for _, y in q), max(y for _, y in q)
    k = 0
    y = y1 - 8.0
    while y > y0 + 4.0:                                      # pendant globes down the concourse, every 10 m
        rod = bpy.data.materials.get("M_MallBrass") or brass
        cylz(None, "MallE_PendRod%d" % k, rod, ex, y, 3.9, 0.03, SH - 0.7 - 3.9, segs=8)
        ball(None, "MallE_Pend%d" % k, globe, ex, y, 3.8, 0.9, 0.9, 0.9)
        lt = bpy.data.lights.new("Hall_LanternE%d_Light" % k, 'POINT')
        lt.energy = 260.0
        setp(lt, "shadow_soft_size", 0.4)
        po = bpy.data.objects.new("Hall_LanternE%d_Light" % k, lt)
        po.location = (ex, y, 3.6)
        COL.objects.link(po)
        y -= 10.0
        k += 1
    for sx in (-1, 1):                                        # lines of light along the concourse's ceiling edges
        x = ex + sx * (max(x_ for x_, _ in q) - ex - 0.6)
        box("MallE_CeilLine%d" % sx, x - 0.05, x + 0.05, y0, y1, SH - 0.74, SH - 0.7, strip, bevel=0)


def mall_checker():
    """The court's floor: a checker of pale and light grey tiles (the owner's screenshots)."""
    m = bpy.data.materials.get("M_MallTiles")
    if m:
        return m
    m = bpy.data.materials.new("M_MallTiles")
    m.use_nodes = True
    nt = m.node_tree
    b = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    ck = nt.nodes.new("ShaderNodeTexChecker")
    ck.inputs["Scale"].default_value = 1.0
    a, c = MALL_LOOK["tiles"]
    ck.inputs["Color1"].default_value = (*a, 1.0)
    ck.inputs["Color2"].default_value = (*c, 1.0)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sc = nt.nodes.new("ShaderNodeVectorMath")
    sc.operation = 'SCALE'
    sc.inputs["Scale"].default_value = 0.5 / MALL_LOOK["tile"]
    nt.links.new(tc.outputs["Object"], sc.inputs[0])
    nt.links.new(sc.outputs[0], ck.inputs["Vector"])
    nt.links.new(ck.outputs["Color"], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = 0.35
    return m


def hang_plant(name, x, y, z):
    """A pot on the parapet with leaves spilling over and tendrils hanging down (the owner's screenshots)."""
    leaf = bpy.data.materials.get("M_LeafLight") or flat("M_LeafLight", (0.40, 0.48, 0.24), noise_scale=4.0)
    for i, (dx, dy, dz, r) in enumerate(((0, 0, 0.25, 0.55), (0.45, 0.1, 0.1, 0.45), (-0.4, -0.1, 0.12, 0.42))):
        ball(None, "%s_Leaf%d" % (name, i), leaf, x + dx, y + dy, z + dz, 2 * r, 2 * r, 1.6 * r)
    for i, (dx, dh) in enumerate(((-0.3, 1.6), (0.05, 2.3), (0.4, 1.9))):
        cylz(None, "%s_Vine%d" % (name, i), leaf, x + dx, y, z - dh, 0.09, dh, r_top=0.07, segs=8)


MALL_CAMS = {"entrance": ((5.0, 216.0, 1.7), (-0.5, 172.0, 5.5), 17.0),   # arriving on the forecourt
             "egg": ((-82.4, 151.6, 1.7), (-95.1, 140.9, 5.0), 16.0),       # in the Food Market's hall, as the game's shot
             "court": ((-12.0, -42.0, 1.7), (2.0, 30.0, 9.0), 16.0)}


def sees(cam_at, portal=None, reach=170.0):
    """Whether the camera can see a point: inside its view (with a margin) and nearer than reach; with a portal
    (the plane's y, the opening's middle x, half its width, its top), a point beyond the plane only through it."""
    (cx, cy, cz), (tx, ty, tz), lens = cam_at
    f = Vector((tx - cx, ty - cy, tz - cz)).normalized()
    r = f.cross(Vector((0, 0, 1))).normalized()
    u = r.cross(f)
    k = 18.0 / lens * 1.15                                    # the half width of a 36 mm sensor, and a margin

    def see(p):
        if os.environ.get("WASTED_ALL") == "1":              # a check: build everything, seen or not
            return True
        d = Vector((p[0] - cx, p[1] - cy, p[2] - cz))
        zf = d.dot(f)
        if zf < 0.3 or zf > reach or abs(d.dot(r)) > zf * k or abs(d.dot(u)) > zf * k:
            return False
        if portal:
            py, px, ph, top = portal
            if (p[1] - py) * (cy - py) < 0:                   # beyond the plane: through the opening, or not at all
                t = (py - cy) / (p[1] - cy)
                xi, zi = cx + t * (p[0] - cx), cz + t * (p[2] - cz)
                return abs(xi - px) < ph and 0.0 < zi < top
        return True
    return see


def mall_polished(name, c):
    """A polished stone, one even colour, as the court's floor but in another tone."""
    m, nt, bsdf = new_mat(name)
    sset(bsdf, "Base Color", rgba(c))
    sset(bsdf, "Roughness", 0.1)
    sset(bsdf, "Specular IOR Level", 0.5)
    return m


def egg_lattice(name):
    """The egg's skin: a lattice of warm gold bars with diamond openings to the sky between them, the game's pattern
    (BrixMall.cpp Egg(), its cells) in the court's look (ADR-0069). The UV map carries the cells: u round the egg in
    cells, v up its arc in cells; a bar runs along every line where u + v or u - v is whole."""
    ML = MALL_LOOK
    m, nt, bsdf = new_mat(name)
    N, L_ = nt.nodes, nt.links
    sset(bsdf, "Base Color", rgba(ML["egg"]))
    sset(bsdf, "Metallic", 0.55)
    sset(bsdf, "Roughness", 0.38)
    out = next(n for n in N if n.type == 'OUTPUT_MATERIAL')
    uv = N.new("ShaderNodeUVMap")
    sep = N.new("ShaderNodeSeparateXYZ")
    L_.new(uv.outputs[0], sep.inputs[0])
    bars = []
    for op in ('ADD', 'SUBTRACT'):
        a = N.new("ShaderNodeMath")
        a.operation = op
        L_.new(sep.outputs[0], a.inputs[0])
        L_.new(sep.outputs[1], a.inputs[1])
        fr = N.new("ShaderNodeMath")
        fr.operation = 'FRACT'
        L_.new(a.outputs[0], fr.inputs[0])
        d = N.new("ShaderNodeMath")                           # how far from the nearest whole number: 0 on a bar's line
        d.operation = 'PINGPONG'
        L_.new(fr.outputs[0], d.inputs[0])
        sset(d, 1, 0.5)
        lt = N.new("ShaderNodeMath")
        lt.operation = 'LESS_THAN'
        L_.new(d.outputs[0], lt.inputs[0])
        sset(lt, 1, ML["egg_bar"])
        bars.append(lt)
    mx = N.new("ShaderNodeMath")
    mx.operation = 'MAXIMUM'
    L_.new(bars[0].outputs[0], mx.inputs[0])
    L_.new(bars[1].outputs[0], mx.inputs[1])
    tr = N.new("ShaderNodeBsdfTransparent")
    mix = N.new("ShaderNodeMixShader")
    L_.new(mx.outputs[0], mix.inputs[0])
    L_.new(tr.outputs[0], mix.inputs[1])
    L_.new(bsdf.outputs[0], mix.inputs[2])
    L_.new(mix.outputs[0], out.inputs[0])
    return m


EGG_FOOD = ("Noodle House", "Pizzeria", "The Grill", "Curry Pot", "Sushi Bar", "Juice Bar", "Dumplings", "Tacos",
            "Bakehouse", "Ice Cream", "Kebabs", "Salad Bar", "Pancakes", "Coffee", "Fish & Chips", "Bao")


def egg_door(drum0, hx, hz):
    """The doorway's middle and half width in degrees (the game's angles round the hall): where the ground floor's drum
    line leaves its gap."""
    a = [math.degrees(math.atan2(z - hz, x - hx)) for x, z in drum0]
    a0, a1 = a[-1], a[0]                                   # the line runs round from one side of the gap to the other
    gap = (a1 - a0) % 360.0
    return (a0 + gap / 2) % 360.0, gap / 2


def egg_food_court(drum0, hx, hz, R, SH, loc, M_, stone, rng):
    """Food outlets skirting the round wall on the ground floor, as in a mall's food court (the owner, 2026-11-27: "we
    can have addition food outlets skirting the round wall, like we have in many malls"): each an open counter set into
    the drum, its sign in its colour with its name above, lit menus on its back wall, its kitchen behind; stone piers
    between them; their lights on all day."""
    ML = MALL_LOOK
    mid, half = egg_door(drum0, hx, hz)
    FH = ML["front_glass"]
    span0, span = mid + half, 360.0 - 2 * half
    n = int(span // 22.0)
    each = span / n
    D = 4.5
    ceil = bpy.data.materials.get("M_OutCeiling") or flat("M_OutCeiling", (0.92, 0.91, 0.88))
    striplit = bpy.data.materials.get("M_OutStripLit") or emissive("M_OutStripLit", (1.0, 0.96, 0.90), 5.0)
    menu = bpy.data.materials.get("M_EggMenuLit") or emissive("M_EggMenuLit", (1.0, 0.95, 0.84), 2.2)   # "Lit": on all day
    board = bpy.data.materials.get("M_EggBoard") or flat("M_EggBoard", (0.10, 0.10, 0.11), rough=0.5)
    steel = bpy.data.materials.get("M_EggSteel") or mall_metal("M_EggSteel", (0.62, 0.63, 0.64), 0.3)
    counter = bpy.data.materials.get("M_EggCounter") or flat("M_EggCounter", ML["egg_counter"])
    top_ = bpy.data.materials.get("M_EggTop") or flat("M_EggTop", ML["cream"], rough=0.3)
    tile = bpy.data.materials.get("M_OutTiles") or flat("M_OutTiles", (0.80, 0.79, 0.76), rough=0.25)
    signs = list(ML["signs"].keys())
    km = globals().setdefault("K_M", {})
    km["EggLetters"] = bpy.data.materials.get("M_EggLettersLit") or emissive("M_EggLettersLit", (1.0, 0.93, 0.80), 3.0)   # backlit, on all day

    def at(r, deg, z=None):
        x, y = loc((hx + r * math.cos(math.radians(deg)), hz + r * math.sin(math.radians(deg))))
        return (x, y) if z is None else (x, y, z)

    def arc(r, d0, d1, k=12):
        return [at(r, d0 + (d1 - d0) * i / k) for i in range(k + 1)]

    def strip_(name, pts, z0, z1, mat, thick):
        bm = bmesh.new()
        for i in range(len(pts) - 1):
            (ax, ay), (bx, by) = pts[i], pts[i + 1]
            dx, dy = bx - ax, by - ay
            l_ = math.hypot(dx, dy) or 1.0
            ox, oy = -dy / l_ * thick / 2, dx / l_ * thick / 2
            q = [(ax + ox, ay + oy), (bx + ox, by + oy), (bx - ox, by - oy), (ax - ox, ay - oy)]
            vb = [bm.verts.new((x, y, z0)) for x, y in q]
            vt = [bm.verts.new((x, y, z1)) for x, y in q]
            bm.faces.new(vt)
            bm.faces.new(list(reversed(vb)))
            for j in range(4):
                bm.faces.new((vb[j], vb[(j + 1) % 4], vt[(j + 1) % 4], vt[j]))
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(mat)
        ob = bpy.data.objects.new(name, me)
        COL.objects.link(ob)
        return ob

    def flat_arc(name, r0, r1, d0, d1, z, mat, k=12):
        bm = bmesh.new()
        a_ = [bm.verts.new(at(r0, d0 + (d1 - d0) * i / k, z)) for i in range(k + 1)]
        b_ = [bm.verts.new(at(r1, d0 + (d1 - d0) * i / k, z)) for i in range(k + 1)]
        for i in range(k):
            bm.faces.new((a_[i], a_[i + 1], b_[i + 1], b_[i]))
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(mat)
        ob = bpy.data.objects.new(name, me)
        COL.objects.link(ob)
        return ob
    for i in range(n):
        d0, d1 = span0 + i * each, span0 + (i + 1) * each
        p0, p1 = d0 + 1.2, d1 - 1.2                          # a stone pier between each two outlets
        strip_("Egg_Pier%d" % i, arc(R, d0 - 1.2, d0 + 1.2, 2), 0.0, FH + 0.7, stone, 0.6)
        strip_("Egg_PierFoot%d" % i, arc(R - 0.31, d0 - 1.2, d0 + 1.2, 2), 0.0, 0.14, M_["brass"], 0.04)
        lt = bpy.data.lights.new("Wl_Light", 'SPOT')              # Wl_Light.001 and on: the wall light's kind
        lt.spot_size = math.radians(50.0)
        lt.spot_blend = 0.8
        setp(lt, "shadow_soft_size", 0.08)
        po = bpy.data.objects.new("Wl_Light", lt)
        px_, py_ = at(R - 0.55, d0)
        po.location = (px_, py_, 0.12)
        out_ = Vector((px_ - HXY[0], py_ - HXY[1], 0)).normalized()    # aimed up the pier, leaning onto it
        po.rotation_euler = (out_ + Vector((0, 0, 3.5))).to_track_quat('-Z', 'Y').to_euler()
        COL.objects.link(po)
        sign = mall_sign_mat(signs[(i * 5) % len(signs)])
        strip_("Egg_Sign%d" % i, arc(R - 0.05, p0, p1), FH + 0.06, FH + 0.66, sign, 0.3)   # its sign in its colour
        strip_("Egg_SignTrimB%d" % i, arc(R - 0.08, p0, p1), FH + 0.0, FH + 0.06, M_["brass"], 0.36)
        strip_("Egg_SignTrimT%d" % i, arc(R - 0.08, p0, p1), FH + 0.66, FH + 0.72, M_["brass"], 0.36)
        strip_("Egg_SignLight%d" % i, arc(R - 0.3, p0, p1), FH - 0.04, FH, M_["strip"], 0.06)
        name = EGG_FOOD[i % len(EGG_FOOD)]
        tx, ty = at(R - 0.22, (p0 + p1) / 2)
        nm_ = text("Egg_Name%d" % i, name, tx, ty, FH + 0.36, min(0.4, (p1 - p0) / 360 * 2 * math.pi * R / max(6, len(name)) * 1.2), "EggLetters", depth=0.02)
        nm_.rotation_euler = (math.pi / 2, 0, math.atan2(HXY[0] - tx, -(HXY[1] - ty)))
        flat_arc("Egg_FoodFloor%d" % i, R - 0.15, R + D, p0, p1, 0.005, tile)               # its kitchen behind
        flat_arc("Egg_FoodCeil%d" % i, R - 0.15, R + D, p0, p1, FH + 0.05, ceil)
        strip_("Egg_FoodBack%d" % i, arc(R + D, p0, p1), 0.0, FH + 0.1, stone, 0.2)
        for e_ in (p0, p1):
            strip_("Egg_FoodSide%d_%d" % (i, int(e_)), [at(R, e_), at(R + D, e_)], 0.0, FH + 0.1, stone, 0.2)
        strip_("Egg_FoodCounter%d" % i, arc(R + 0.9, p0 + 0.8, p1 - 0.8), 0.0, 1.0, counter, 0.7)   # the counter, open to the hall
        strip_("Egg_FoodCounterTop%d" % i, arc(R + 0.9, p0 + 0.6, p1 - 0.6), 1.0, 1.07, top_, 0.85)
        strip_("Egg_FoodKick%d" % i, arc(R + 0.53, p0 + 0.8, p1 - 0.8), 0.0, 0.12, M_["brass"], 0.03)
        strip_("Egg_FoodBackCounter%d" % i, arc(R + D - 0.45, p0 + 0.4, p1 - 0.4), 0.0, 0.95, top_, 0.6)
        for j in range(3):                                   # three menu boards on the back wall: dark, lit lines and pictures
            c_ = p0 + (p1 - p0) * (j + 0.5) / 3
            w_ = (p1 - p0) / 3 * 0.8
            strip_("Egg_Menu%d_%d" % (i, j), arc(R + D - 0.13, c_ - w_ / 2, c_ + w_ / 2, 3), 1.6, 2.55, board, 0.04)
            strip_("Egg_MenuFrame%d_%d" % (i, j), arc(R + D - 0.12, c_ - w_ / 2 - 0.15, c_ + w_ / 2 + 0.15, 3), 1.56, 2.59, M_["brass"], 0.03)
            strip_("Egg_MenuTitle%d_%d" % (i, j), arc(R + D - 0.16, c_ - w_ * 0.3, c_ + w_ * 0.3, 2), 2.38, 2.46, menu, 0.01)
            for r_ in range(4):
                zz = 2.22 - r_ * 0.13
                strip_("Egg_MenuLine%d_%d_%d" % (i, j, r_), arc(R + D - 0.16, c_ - w_ * 0.42, c_ + w_ * (0.05 + 0.1 * (r_ % 2)), 2), zz, zz + 0.035, menu, 0.01)
                strip_("Egg_MenuPrice%d_%d_%d" % (i, j, r_), arc(R + D - 0.16, c_ + w_ * 0.3, c_ + w_ * 0.42, 1), zz, zz + 0.035, menu, 0.01)
            strip_("Egg_MenuPic%d_%d" % (i, j), arc(R + D - 0.16, c_ - w_ * 0.3, c_ + w_ * 0.3, 2), 1.66, 1.78,
                   goods_mat("produce", (i + j) % 8), 0.01)
        strip_("Egg_FoodHood%d" % i, arc(R + D - 0.5, p0 + 0.6, p1 - 0.6, 6), FH - 0.55, FH - 0.05, steel, 0.8)   # the kitchen's hood
        for j, r_ in enumerate((R + 1.5, R + 3.4)):          # its strip lights and their light, on all day
            strip_("Egg_FoodStrip%d_%d" % (i, j), arc(r_, p0 + 0.6, p1 - 0.6, 6), FH - 0.02, FH + 0.03, striplit, 0.12)
            lt = bpy.data.lights.new("ShopIn_Egg%d_%d" % (i, j), 'POINT')
            lt.energy = 150.0
            lt.color = tuple(LIGHTS["colours"]["warm_white"]["colour"])   # the kitchens warm, against the hall's daylight
            setp(lt, "shadow_soft_size", 0.6)
            po = bpy.data.objects.new("ShopIn_Egg%d_%d" % (i, j), lt)
            po.location = at(r_, (p0 + p1) / 2, FH - 0.3)
            COL.objects.link(po)
        G = group("Egg_FoodGoods%d" % i, *at(R + 0.9, (p0 + p1) / 2), 0.0)   # dishes on the counter, pans behind
        gm = GoodsMesh(G, "Egg_FoodGoodsM%d" % i)
        for j in range(6):
            gm.cyl(goods_mat("produce", (i + j) % 8), rng.uniform(-1.8, 1.8), rng.uniform(-0.2, 0.2), 1.07, rng.uniform(0.12, 0.2), rng.uniform(0.05, 0.14))
        gm.done()
    strip_("Egg_SignBandBack", arc(R + 0.1, span0, span0 + span, 96), FH + 0.06, FH + 0.66, stone, 0.12)
    hlog("  the food court: %d outlets round the drum, the doorway at %.0f deg (%.0f wide)" % (n, mid, 2 * half))


def egg_seating(hx, hz, R, loc, M_, rng, stalls, drum0):
    """Tables and chairs filling the hall as a food court (the owner, 2026-11-27: "needs tables and chair, even long
    tables with more chairs like in the mall"): a ring of long tables for ten between the stalls and the outlets, round
    tables for four between them and near the tree; clear ways to the doorway and through the stalls. In the court's look:
    timber tops and seats on slender brass, the round tables topped in cream stone. Each table and its chairs one object."""
    ML = MALL_LOOK
    wood_ = bpy.data.materials.get("M_EggCounter") or flat("M_EggCounter", ML["egg_counter"])
    top_ = bpy.data.materials.get("M_EggTop") or flat("M_EggTop", ML["cream"], rough=0.3)
    seat = bpy.data.materials.get("M_EggSeat") or flat("M_EggSeat", ML["egg_seat"], rough=0.6)
    br = M_["brass"]
    mid, half = egg_door(drum0, hx, hz)
    cam = math.degrees(math.atan2(MALL_CAMS["egg"][0][1] - (hz - MALL_MID[1]), MALL_CAMS["egg"][0][0] - (hx - MALL_MID[0])))

    def clear(deg, width):
        return all(abs((deg - c + 180) % 360 - 180) > w for c, w in ((mid, half + width), (cam, 9 + width)))

    def chair(gm, x, y, face):
        """A chair at (x, y), its back away from face (+1: the table at +y)."""
        for sx in (-1, 1):
            for sy in (-1, 1):
                gm.add(br, x + sx * 0.18, y + sy * 0.18, 0.225, 0.03, 0.03, 0.45)
        gm.add(seat, x, y, 0.47, 0.44, 0.44, 0.05)
        gm.add(wood_, x, y - face * 0.21, 0.72, 0.42, 0.04, 0.36)

    def round_table(name, deg, r):
        X, Y = loc((hx + r * math.cos(math.radians(deg)), hz + r * math.sin(math.radians(deg))))
        G = group(name, X, Y, rng.uniform(0, math.pi / 2))
        gm = GoodsMesh(G, name + "_M")
        gm.cyl(br, 0, 0, 0.0, 0.26, 0.03, 20)
        gm.cyl(br, 0, 0, 0.03, 0.04, 0.69, 8)
        gm.cyl(top_, 0, 0, 0.72, 0.46, 0.04, 28)
        for k in range(4):
            a = k * math.pi / 2
            cx, cy = 0.66 * math.cos(a), 0.66 * math.sin(a)
            if k % 2 == 0:                                       # the chairs square to the table, two each way
                for sx in (-1, 1):
                    for sy in (-1, 1):
                        gm.add(br, cx + sx * 0.18, cy + sy * 0.18, 0.225, 0.03, 0.03, 0.45)
                gm.add(seat, cx, cy, 0.47, 0.44, 0.44, 0.05)
                gm.add(wood_, cx + math.copysign(0.21, cx), cy, 0.72, 0.04, 0.42, 0.36)
            else:
                chair(gm, cx, cy, -1 if cy > 0 else 1)
        gm.done()

    def long_table(name, deg, r, radial=False):
        X, Y = loc((hx + r * math.cos(math.radians(deg)), hz + r * math.sin(math.radians(deg))))
        X0, Y0 = loc((hx, hz))
        yaw = math.atan2(Y - Y0, X - X0) + (0.0 if radial else math.pi / 2)
        G = group(name, X, Y, yaw)
        gm = GoodsMesh(G, name + "_M")
        gm.add(wood_, 0, 0, 0.745, 4.8, 0.92, 0.05)
        for sx in (-1, 1):                                       # brass trestles
            gm.add(br, sx * 2.1, 0, 0.36, 0.06, 0.7, 0.04)
            gm.add(br, sx * 2.1, 0, 0.53, 0.05, 0.05, 0.40)
            gm.add(br, sx * 2.1, 0, 0.02, 0.08, 0.8, 0.04)
        gm.add(br, 0, 0, 0.36, 4.2, 0.04, 0.04)
        for i in range(5):
            x = -1.92 + i * 0.96
            chair(gm, x, 0.74, -1)
            chair(gm, x, -0.74, 1)
        for i in range(3):                                       # a little greenery down its middle
            gm.cyl(top_, -1.6 + i * 1.6, 0, 0.77, 0.07, 0.1, 10)
            gm.add(M_["leaf2"], -1.6 + i * 1.6, 0, 0.92, 0.16, 0.16, 0.14)
        for i in (-1, 1):                                        # two pendants over it, warm pools on the timber
            gm.cyl(br, i * 1.2, 0, 3.0, 0.012, 15.0, 6)
            gm.cyl(br, i * 1.2, 0, 2.86, 0.3, 0.14, 24)
        gm.done()
        for i in (-1, 1):
            ox, oy = math.cos(yaw) * i * 1.2, math.sin(yaw) * i * 1.2
            ball(None, name + "_Opal%d" % i, M_["globe"], X + ox, Y + oy, 2.74, 0.44, 0.44, 0.3)
            lt = bpy.data.lights.new("Sp_Light", 'POINT')         # Sp_Light.001 and on: the shop pendant's kind
            setp(lt, "shadow_soft_size", 0.2)
            po = bpy.data.objects.new("Sp_Light", lt)
            po.location = (X + ox, Y + oy, 2.6)
            COL.objects.link(po)
    n_long = n_round = 0
    for k in range(12):                                          # the long tables, round the outer ring
        deg = mid + 15.0 + k * 30.0
        if clear(deg, 8):
            long_table("Egg_Long%d" % k, deg, 17.3)
            n_long += 1
    for k in range(12):                                          # round tables between them, a little further in
        deg = mid + k * 30.0
        if clear(deg, 4):
            round_table("Egg_Round%d" % k, deg, 14.0)
            n_round += 1
    for k, a in enumerate(stalls):                               # and near the tree, between the stalls
        deg = a + 30.0
        if clear(deg, 3):
            round_table("Egg_RoundIn%d" % k, deg, 6.4)
            n_round += 1
    hlog("  seating: %d long tables for ten, %d round tables for four" % (n_long, n_round))


def mall_egg_hall(P, LOT, loc, SH, M_):
    """The Food Market's egg hall as the plan draws it (World/city_01/blocks/b_16_5.json halls[0], WLD-059, ADR-0038;
    the game's BrixMall.cpp): a round hall 22 m across its radius, three floors high, its drum walls (the walls of kind
    drum) with the slanting walk from the court coming in through them, on the upper floors ending in a balcony; the roof
    open over the hall in a round eye, the egg above it (38 x 31 x 45 m, its middle 11 m up) a lattice whose openings
    show the sky; a stall where each of the six staff stands, a tree in a round planter in the middle, lanterns hanging in
    a ring. The look is the court's (ADR-0069): cream drum walls with a line of light round each floor and brass at their
    foot, the polished stone floor with brass rings let into it, timber under the balconies, glass and brass rails, the
    lattice in warm gold; the stalls timber with cream tops under striped canopies, heaped with produce."""
    ML = MALL_LOOK
    h = P["halls"][0]
    egg = LOT["shell"]["domes"][h["dome"]]
    hx, hz, R = h["pos"][0], h["pos"][1], h["r"]
    HX, HY = loc((hx, hz))
    globals()["HXY"] = (HX, HY)
    n_lv = len(P["levels"])
    Hh = SH * n_lv
    rng = random.Random(56)
    drum0 = []
    cols = ML["shell_tones"]                                  # the drum in the ribbon wall's honed stone, smooth (posh)
    stone = bpy.data.materials.get("M_Shell0") or sandstone("M_Shell0", cols[0][0], cols[0][1])

    def near(pts, d=10.0):
        return any(math.hypot(x - hx, z - hz) < R + d for x, z in pts)

    def obj(name, bm, mat):
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(mat)
        ob = bpy.data.objects.new(name, me)
        COL.objects.link(ob)
        return ob

    def plate(name, loop, holes, z, mat):
        bm = bmesh.new()
        for lp in [loop] + holes:
            vs = [bm.verts.new((x, y, z)) for x, y in lp]
            for i in range(len(vs)):
                bm.edges.new((vs[i], vs[(i + 1) % len(vs)]))
        bmesh.ops.triangle_fill(bm, use_beauty=True, use_dissolve=False, edges=bm.edges[:])
        return obj(name, bm, mat)

    def band(name, line, z0, z1, mat, closed=False, thick=0.0):
        bm = bmesh.new()
        pts = list(line) + ([line[0]] if closed else [])
        for i in range(len(pts) - 1):
            (ax, ay), (bx, by) = pts[i], pts[i + 1]
            if thick:
                dx, dy = bx - ax, by - ay
                n = math.hypot(dx, dy) or 1.0
                ox, oy = -dy / n * thick / 2, dx / n * thick / 2
                q = [(ax + ox, ay + oy), (bx + ox, by + oy), (bx - ox, by - oy), (ax - ox, ay - oy)]
                vb = [bm.verts.new((x, y, z0)) for x, y in q]
                vt = [bm.verts.new((x, y, z1)) for x, y in q]
                bm.faces.new(vt)
                for k in range(4):
                    bm.faces.new((vb[k], vb[(k + 1) % 4], vt[(k + 1) % 4], vt[k]))
            else:
                bm.faces.new([bm.verts.new(c) for c in ((ax, ay, z0), (bx, by, z0), (bx, by, z1), (ax, ay, z1))])
        return obj(name, bm, mat)

    def ring(r, n=96):
        return [(HX + r * math.cos(2 * math.pi * i / n), HY + r * math.sin(2 * math.pi * i / n)) for i in range(n)]

    # the floors: the hall's own round floor, the walks into it on every floor
    for lv in P["levels"]:
        L, z = lv["level"], lv["level"] * SH
        for k, w in enumerate(lv["walks"]):
            if not near(w):
                continue
            q = [loc(p_) for p_ in w]
            if L == 0:
                plate("Egg_Walk%d_%d" % (L, k), q, [], 0.0, M_["floor"])
                if len(q) == 4:                                  # the walk in from the court, lit along its middle as a concourse
                    for j, t in enumerate((0.15, 0.4, 0.65)):
                        ax, ay = (q[0][0] + q[3][0]) / 2, (q[0][1] + q[3][1]) / 2
                        bx, by = (q[1][0] + q[2][0]) / 2, (q[1][1] + q[2][1]) / 2
                        x, y = bx + (ax - bx) * t, by + (ay - by) * t
                        lt = bpy.data.lights.new("Hall_LanternW%d_%d_Light" % (k, j), 'POINT')
                        lt.energy = 220.0
                        setp(lt, "shadow_soft_size", 0.4)
                        po = bpy.data.objects.new("Hall_LanternW%d_%d_Light" % (k, j), lt)
                        po.location = (x, y, SH - 1.4)
                        COL.objects.link(po)
            else:                                                # a walk on an upper floor: timber under it, a cream edge
                plate("Egg_WalkUnder%d_%d" % (L, k), q, [], z - 0.7, M_["under"])
                plate("Egg_Walk%d_%d" % (L, k), q, [], z + 0.02, M_["floor"])
                band("Egg_WalkEdge%d_%d" % (L, k), q, z - 0.7, z + 0.02, M_["edge"], closed=True)
                band("Egg_WalkLine%d_%d" % (L, k), [(x, y) for x, y in q], z - 0.72, z - 0.66, M_["strip"], closed=True)
                if L == 1 and len(q) == 4:                       # a line of light down the middle of its underside: on all day
                    ax, ay = (q[0][0] + q[3][0]) / 2, (q[0][1] + q[3][1]) / 2
                    bx, by = (q[1][0] + q[2][0]) / 2, (q[1][1] + q[2][1]) / 2
                    band("Egg_WalkGlow%d_%d" % (L, k), [(ax, ay), (bx, by)], z - 0.74, z - 0.72, M_["strip"], thick=0.5)
        for k, wl in enumerate(lv["walls"]):
            if not near(wl["line"]):
                continue
            line = [loc(q) for q in wl["line"]]
            if wl.get("kind") == "drum":                         # the hall's drum: cream, its line of light, brass at its foot
                band("Egg_Drum%d_%d" % (L, k), line, z + (ML["front_glass"] + 0.7 if L == 0 else 0.0), z + SH, stone, thick=0.3)
                band("Egg_DrumLight%d_%d" % (L, k), [(HX + (x - HX) * 0.993, HY + (y - HY) * 0.993) for x, y in line], z + 4.3, z + 4.38, M_["strip"], thick=0.04)
                band("Egg_DrumTrim%d_%d" % (L, k), [(HX + (x - HX) * 0.992, HY + (y - HY) * 0.992) for x, y in line], z + SH - 0.08, z + SH, M_["brass"], thick=0.06)
                if L == 0:
                    drum0 = wl["line"]
            else:
                band("Egg_Wall%d_%d" % (L, k), line, z, z + SH, M_["wall"], thick=0.3)
        for k, r in enumerate(lv["rails"]):
            if near(r):
                rl = [loc(q) for q in r]
                g_ = band("Egg_Balustrade%d_%d" % (L, k), rl, z + 0.08, z + 1.02, M_["glass"], thick=0.03)
                setp(g_, "visible_shadow", False)
                band("Egg_Rail%d_%d" % (L, k), rl, z + 1.02, z + 1.1, M_["brass"], thick=0.1)
                band("Egg_Shoe%d_%d" % (L, k), rl, z, z + 0.08, M_["brass"], thick=0.12)
    egg_food_court(drum0, hx, hz, R, SH, loc, M_, stone, rng)
    egg_seating(hx, hz, R, loc, M_, rng, [math.degrees(math.atan2(st[1] - hz, st[0] - hx)) for st in
                next(u for u in P["levels"][0]["units"] if u["id"] == h["unit"]).get("stand", [])], drum0)
    warm = bpy.data.materials.get("M_EggStoneWarm") or mall_polished("M_EggStoneWarm", ML["egg_floor_warm"])
    deep = bpy.data.materials.get("M_EggStoneDeep") or mall_polished("M_EggStoneDeep", ML["egg_floor_deep"])
    for nm_, r0, r1, m_ in (("Egg_FloorRing", 12.6, 15.4, warm), ("Egg_FloorQueue", 19.4, R - 0.2, deep), ("Egg_FloorInner", 3.4, 4.6, deep)):
        bm = bmesh.new()
        a_ = [bm.verts.new((HX + r0 * math.cos(2 * math.pi * i / 128), HY + r0 * math.sin(2 * math.pi * i / 128), 0.004)) for i in range(128)]
        b_ = [bm.verts.new((HX + r1 * math.cos(2 * math.pi * i / 128), HY + r1 * math.sin(2 * math.pi * i / 128), 0.004)) for i in range(128)]
        for i in range(128):
            bm.faces.new((a_[i], a_[(i + 1) % 128], b_[(i + 1) % 128], b_[i]))
        obj(nm_, bm, m_)
    # 4. timber fins round the drum on the upper floors: the low sun striped across them
    fins = GoodsMesh(None, "Egg_Fins")
    nf = 120
    for i in range(nf):
        a = 2 * math.pi * i / nf
        dg = (math.degrees(a)) % 360
        mid_, half_ = egg_door(drum0, hx, hz)
        if abs((dg - mid_ + 180) % 360 - 180) < half_ + 1:
            continue
        x, y = HX + (R - 0.35) * math.cos(a), HY + (R - 0.35) * math.sin(a)
        # a fin is a thin box turned to face the middle: built in place from its four corners
        t = (-math.sin(a), math.cos(a))
        nrm = (math.cos(a), math.sin(a))
        for z0, z1 in ((SH + 1.2, 2 * SH - 0.2), (2 * SH + 1.2, Hh - 0.3)):
            v = []
            for dn in (-0.2, 0.2):
                for dt in (-0.04, 0.04):
                    v.append((x + nrm[0] * dn + t[0] * dt, y + nrm[1] * dn + t[1] * dt))
            bmv = [fins.bm.verts.new((px, py, zz)) for zz in (z0, z1) for px, py in v]
            if M_["wood"] not in fins.mats:
                fins.mats.append(M_["wood"])
            k_ = fins.mats.index(M_["wood"])
            for f_ in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)):
                fins.bm.faces.new([bmv[q] for q in f_]).material_index = k_
    fins.done()
    for r_, w_ in ((R - 1.6, 0.1), (9.0, 0.16), (5.2, 0.08)):    # brass rings let into the floor
        band("Egg_Inlay%d" % int(r_ * 10), ring(r_), 0.0, 0.012, M_["brass"], closed=True, thick=w_)
    # the roof, open over the hall in a round eye, its edge cream with a line of light; the egg above it
    S_ = R + 26.0
    sq = [(HX - S_, HY - S_), (HX + S_, HY - S_), (HX + S_, HY + S_), (HX - S_, HY + S_)]
    eye = ring(R + 1.2)
    plate("Egg_Roof", sq, [eye], Hh, M_["edge"])
    plate("Egg_RoofTop", sq, [eye], Hh + 0.4, M_["edge"])
    band("Egg_EyeEdge", eye, Hh, Hh + 0.4, M_["edge"], closed=True)
    band("Egg_EyeLine", [(HX + (x - HX) * 0.998, HY + (y - HY) * 0.998) for x, y in eye], Hh + 0.02, Hh + 0.08, M_["strip"], closed=True)
    rx, ry, rz = egg["radii"]
    EX, EY = loc(egg["pos"])
    cy = egg["centre"]
    base = Hh - 0.6
    end = math.acos(max(-1.0, min(1.0, (base - cy) / ry)))
    RINGS, SEG = 56, 192
    cell = math.pi * (rx + rz) / egg["cells"]
    arcs, arc = [0.0], 0.0
    for r in range(1, RINGS + 1):
        a, a0 = end * r / RINGS, end * (r - 1) / RINGS
        arc += math.hypot((rx + rz) / 2 * (math.sin(a) - math.sin(a0)), ry * (math.cos(a) - math.cos(a0)))
        arcs.append(arc)
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    V = [[bm.verts.new((EX + rx * math.sin(end * r / RINGS) * math.cos(2 * math.pi * s_ / SEG),
                        EY + rz * math.sin(end * r / RINGS) * math.sin(2 * math.pi * s_ / SEG),
                        cy + ry * math.cos(end * r / RINGS))) for s_ in range(SEG)] for r in range(RINGS + 1)]
    for r in range(RINGS):
        for s_ in range(SEG):
            t = (s_ + 1) % SEG
            f = bm.faces.new((V[r][s_], V[r][t], V[r + 1][t], V[r + 1][s_]))
            for lp, (ss, rr) in zip(f.loops, ((s_, r), (s_ + 1, r), (s_ + 1, r + 1), (s_, r + 1))):
                lp[uvl].uv = (ss / SEG * egg["cells"], arcs[rr] / cell)
    eo = obj("Egg_Lattice", bm, egg_lattice("M_EggLattice"))
    for pg in eo.data.polygons:
        pg.use_smooth = True
    # lanterns hanging in a ring, as the game's: globes on brass rods from the roof, of the hall lantern's kind
    for i in range(6):
        a = 2 * math.pi * (i + 0.5) / 6
        x, y = HX + R * 0.62 * math.cos(a), HY + R * 0.62 * math.sin(a)
        zz = 6.4 + (i % 2) * 1.6
        cylz(None, "Egg_LanternRod%d" % i, M_["brass"], x, y, zz + 0.5, 0.03, Hh + 6 - zz - 0.5, segs=8)
        cylz(None, "Egg_LanternCap%d" % i, M_["brass"], x, y, zz + 0.45, 0.32, 0.1, segs=20)
        ball(None, "Egg_Lantern%d" % i, M_["globe"], x, y, zz, 0.9, 0.9, 0.9)
        lt = bpy.data.lights.new("Hall_LanternH%d_Light" % i, 'POINT')
        lt.energy = 220.0
        setp(lt, "shadow_soft_size", 0.4)
        po = bpy.data.objects.new("Hall_LanternH%d_Light" % i, lt)
        po.location = (x, y, zz - 0.2)
        COL.objects.link(po)
    # the planter in the middle and its tree, flowers round it
    cylz(None, "Egg_Planter", M_["kerb"], HX, HY, 0.0, 3.2, 0.7, segs=48)
    cylz(None, "Egg_PlanterRim", M_["brass"], HX, HY, 0.68, 3.25, 0.05, segs=48)
    cylz(None, "Egg_PlanterSoil", bpy.data.materials.get("M_Grass") or flat("M_Grass", (0.30, 0.40, 0.22), noise_scale=3.0), HX, HY, 0.0, 3.05, 0.74, segs=48)
    cylz(None, "Egg_Trunk", M_["wood"], HX, HY, 0.7, 0.28, 6.0, r_top=0.18)
    for (dx, dy, dz, rr_, m_) in ((0, 0, 7.9, 2.4, M_["leaf"]), (1.5, 0.5, 7.1, 1.8, M_["leaf"]), (-1.4, 0.4, 7.3, 1.7, M_["leaf2"]),
                                  (0.4, -1.3, 7.0, 1.6, M_["leaf"]), (0.2, 0.3, 9.4, 1.5, M_["leaf2"])):
        ball(None, "Egg_Leaf", m_, HX + dx, HY + dy, dz, 2 * rr_, 2 * rr_, 1.6 * rr_)
    for j in range(40):
        a = rng.uniform(0, 2 * math.pi)
        d = rng.uniform(1.0, 2.8)
        ball(None, "Egg_Flower%d" % j, M_["flowers"][j % len(M_["flowers"])], HX + d * math.cos(a), HY + d * math.sin(a), 0.82, 0.3, 0.3, 0.2)
    # a stall where each of the hall's staff stands, facing the middle: a timber counter with a cream top, heaped with
    # produce, under a striped canopy on brass poles
    unit = next(u for u in P["levels"][0]["units"] if u["id"] == h["unit"])
    counter = bpy.data.materials.get("M_EggCounter") or flat("M_EggCounter", ML["egg_counter"])
    top_ = bpy.data.materials.get("M_EggTop") or flat("M_EggTop", ML["cream"], rough=0.3)
    for k, st in enumerate(unit.get("stand", [])):
        dx, dz = hx - st[0], hz - st[1]
        d = math.hypot(dx, dz) or 1.0
        cx, cz = st[0] + dx / d * 1.0, st[1] + dz / d * 1.0
        X, Y = loc((cx, cz))
        G = group("Egg_Stall%d" % k, X, Y, math.atan2(dx, -dz))   # local +y towards the middle
        rbox(G, "Egg_StallBody%d" % k, counter, 0, 0, 0.5, 2.8, 0.8, 1.0, 0.02, 1)
        rbox(G, "Egg_StallTop%d" % k, top_, 0, 0, 1.04, 3.0, 1.0, 0.08, 0.02, 1)
        rbox(G, "Egg_StallKick%d" % k, M_["brass"], 0, 0.41, 0.06, 2.8, 0.02, 0.12, 0.0, 1)
        gm = GoodsMesh(G, "Egg_StallGoods%d" % k)                 # crates of produce on the top, sloping to the front
        for i in range(5):
            gx = -1.2 + i * 0.6
            gm.add(counter, gx, 0.05, 1.14, 0.52, 0.7, 0.12)
            kind_ = goods_mat("produce", (i * 3 + k) % 8)        # one kind to a crate, heaped
            for j in range(22):
                rr = rng.uniform(0.04, 0.06)
                gm.cyl(kind_, gx + rng.uniform(-0.2, 0.2), rng.uniform(-0.28, 0.28), 1.2 + rng.uniform(0, 0.07), rr, rr * 1.4, 8)
        gm.done()
        stripe = M_["banners"][k % len(M_["banners"])]
        for sx in (-1, 1):
            for sy in (-1, 1):
                cylz(G, "Egg_StallPole%d_%d_%d" % (k, sx, sy), M_["brass"], sx * 1.45, sy * 0.55, 1.1, 0.03, 1.5, segs=8)
        for i in range(6):                                       # the canopy in stripes, cream between its colour
            rbox(G, "Egg_StallCanopy%d_%d" % (k, i), stripe if i % 2 == 0 else top_, -1.5 + (i + 0.5) * 0.5, 0, 2.62, 0.5, 1.3, 0.04, 0.0, 1)
    people([("PlayerA", HX + 4.0, HY + 15.5), ("PlayerB", HX - 5.0, HY + 8.5), ("Shopkeeper", HX + 2.0, HY - 9.0)])
    hlog("  the Food Market's egg hall: radius %.1f m, %d floors, the egg %s m with %d cells, %d stalls" % (
        R, n_lv, "x".join("%.0f" % v for v in egg["radii"]), egg["cells"], len(unit.get("stand", []))))


def mall_court(Ls, view="court"):
    """Eastgate Mall's Great Court as its plan in World/city_01/blocks/b_16_5.json draws it (WLD-058 to WLD-062, ADR-0038;
    Tools/worldgen/mall_inside_01.py): 92 x 192 m, three floors 6 m apart; on each upper floor a gallery round the court
    whose opening (the plan's void) swells round each atrium and pinches between them, a parapet along its edge, bridges
    across at the pinches; the roof with its five round skylights, glazed as the big hall's; the moving stairs, fountains,
    gardens with trees, benches and the galleries' planters; along every walk the outlets' fronts (the plan's fronts, in
    the approved timber shopfront's parts, their windows lit) and walls. Only what stands in and round the court is
    built. In the town's materials; lanterns of the hall's kind hang under the gallery edges."""
    blk = json.load(open("C:/Projects/Wasted/World/city_01/blocks/%s.json" % MALL_LOT[0], encoding="utf-8"))
    LOT = next(l for l in blk["lots"] if l["id"] == MALL_LOT[1])
    P = LOT["plan"]
    SH = P["storey_height"]
    mx, mz = MALL_MID

    def loc(q):
        return (q[0] - mx, q[1] - mz)

    y_hi = 175 if view == "entrance" else 106                 # the entrance's view takes in the south concourse too
    cam_at = MALL_CAMS[view]
    if os.environ.get("WASTED_CAM"):                          # a test view: x,y,z,tx,ty,tz,lens
        v_ = [float(t) for t in os.environ["WASTED_CAM"].split(",")]
        cam_at = (tuple(v_[0:3]), tuple(v_[3:6]), v_[6])
    portal = None
    if view == "entrance":                                    # from the forecourt the hall is seen only through its doorway
        e_ = next(a for a in P["entrances"] if a["side"] == "s")
        portal = (loc((0, 848.0))[1], loc(e_["at"])[0], e_["width"] / 2.0, MALL_LOOK["door_side"] + MALL_LOOK["door_rise"])
    seen = sees(cam_at, portal)

    def near(pts):
        return any(abs(x - mx) < 58 and -106 < z - mz < y_hi for x, z in pts)

    # the shapes are the game's; the look is the owner's screenshots of the court (Art/StyleTests/mall_wanted_51/; the
    # owner, 2026-11-26: "use the shapes from the game ... but texture, color, styling etc, please come up with your self
    # based on the reference screenshots"): a pale checker of floor tiles, galleries warm brown underneath with cream
    # edges and a line of light along each edge, plants hanging from the parapets, a lattice in the roof lights, grass
    # islands in white kerbs, moving stairs with dark treads between brown and cream sides
    ML = MALL_LOOK
    floor = mall_checker()
    slab_under = bpy.data.materials.get("M_MallBrown") or painted("M_MallBrown", *ML["brown"], noise_scale=0.35, bump=0.15)
    edge = bpy.data.materials.get("M_MallCream") or flat("M_MallCream", ML["cream"])
    wd = bpy.data.materials.get("M_MallBrownSide") or flat("M_MallBrownSide", ML["brown"][1])
    pl = bpy.data.materials.get("M_MallWall") or flat("M_MallWall", ML["wall"])
    sd = bpy.data.materials.get("M_MallKerb") or flat("M_MallKerb", ML["kerb"])
    tread = bpy.data.materials.get("M_MallTread") or flat("M_MallTread", ML["tread"], rough=0.5)
    strip = bpy.data.materials.get("M_MallEdgeLit") or emissive("M_MallEdgeLit", ML["strip"], ML["strip_glow"])   # "Lit": on by day too
    # the owner, 2026-11-26: "feel inspired and take a creative take on the look and feel add ornaments if required use
    # glass and other material to make it look good": glass balustrades with brass rails, timber slats under the
    # galleries, slender columns with brass collars, a polished stone floor inlaid with brass along the galleries'
    # curves, a chandelier of globes in each roof light, banners from the roof, flowers round the trees, glass-sided
    # moving stairs, shop fronts in the town's colours
    slab_under = mall_slats()
    floor = mall_stone()
    brass = bpy.data.materials.get("M_MallBrass") or mall_metal("M_MallBrass", ML["brass"], 0.28)
    glassp = bpy.data.materials.get("M_MallGlass") or mall_glass("M_MallGlass", ML["glass_tint"], ML["glass_alpha"])
    globe = bpy.data.materials.get("M_MallGlobeLit") or emissive("M_MallGlobeLit", ML["globe"], ML["globe_glow"])
    column = bpy.data.materials.get("M_MallColumn") or flat("M_MallColumn", ML["column"])
    mullion = bpy.data.materials.get("M_MallMullion") or flat("M_MallMullion", ML["mullion"])
    banners = [bpy.data.materials.get("M_MallBanner%d" % i) or mall_fabric("M_MallBanner%d" % i, c) for i, c in enumerate(ML["banners"])]
    flowers = [bpy.data.materials.get("M_MallFlower%d" % i) or flat("M_MallFlower%d" % i, c) for i, c in enumerate(ML["flowers"])]
    leaf = bpy.data.materials.get("M_Leaf") or flat("M_Leaf", (0.24, 0.36, 0.20), noise_scale=4.0)
    leaf2 = bpy.data.materials.get("M_LeafLight") or flat("M_LeafLight", (0.40, 0.48, 0.24), noise_scale=4.0)
    grass = bpy.data.materials.get("M_Grass") or flat("M_Grass", (0.30, 0.40, 0.22), noise_scale=3.0)
    water = bpy.data.materials.get("M_Water") or flat("M_Water", (0.10, 0.16, 0.20), rough=0.05)
    lit = bpy.data.materials.get("M_FrontLit") or emissive("M_FrontLit", (1.0, 0.86, 0.66), 1.2)   # "Lit": a shop window, lit by day
    fr = bpy.data.materials.get("M_SignTeal") or flat("M_SignTeal", (0.16, 0.30, 0.31))
    W2, D2 = 46.0, 96.0

    def mesh_obj(name, bm, mat):
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(mat)
        ob = bpy.data.objects.new(name, me)
        COL.objects.link(ob)
        return ob

    def plate(name, outer, holes, z, mat):
        """A flat plate at height z: the outer loop with the holes cut from it."""
        bm = bmesh.new()
        for loop in [outer] + holes:
            vs = [bm.verts.new((x, y, z)) for x, y in loop]
            for i in range(len(vs)):
                bm.edges.new((vs[i], vs[(i + 1) % len(vs)]))
        bmesh.ops.triangle_fill(bm, use_beauty=True, use_dissolve=False, edges=bm.edges[:])
        return mesh_obj(name, bm, mat)

    def band(name, line, z0, z1, mat, closed=False, thick=0.0):
        """An upright strip along a line from z0 to z1 (a gallery's edge, a parapet, a wall)."""
        bm = bmesh.new()
        pts = list(line) + ([line[0]] if closed else [])
        for i in range(len(pts) - 1):
            (ax, ay), (bx, by) = pts[i], pts[i + 1]
            if thick:
                dx, dy = bx - ax, by - ay
                n = math.hypot(dx, dy) or 1.0
                ox, oy = -dy / n * thick / 2, dx / n * thick / 2
                q = [(ax + ox, ay + oy), (bx + ox, by + oy), (bx - ox, by - oy), (ax - ox, ay - oy)]
                vb = [bm.verts.new((x, y, z0)) for x, y in q]
                vt = [bm.verts.new((x, y, z1)) for x, y in q]
                bm.faces.new(vt)
                for k in range(4):
                    bm.faces.new((vb[k], vb[(k + 1) % 4], vt[(k + 1) % 4], vt[k]))
            else:
                v = [bm.verts.new(c) for c in ((ax, ay, z0), (bx, by, z0), (bx, by, z1), (ax, ay, z1))]
                bm.faces.new(v)
        return mesh_obj(name, bm, mat)

    if view == "egg":
        mall_egg_hall(P, LOT, loc, SH, dict(floor=floor, under=slab_under, edge=edge, wall=pl, kerb=sd, brass=brass,
                                            glass=glassp, globe=globe, strip=strip, wood=wd, leaf=leaf, leaf2=leaf2,
                                            banners=banners, flowers=flowers))
        sun_and_sky(SCENE, (0.35, -0.30, 0.89), 8.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.45)
        return camera(SCENE, "Cam_test" if os.environ.get("WASTED_CAM") else "Cam_egg_hall", *cam_at)
    court = [(-W2, -D2), (W2, -D2), (W2, D2), (-W2, D2)]
    box("Mall_Floor", -W2 - 12, W2 + 12, -D2 - 12, max(D2 + 12, y_hi - 4), -0.1, 0.0, floor, bevel=0)   # out to the entrance
    lanterns = 0
    built_in = 0
    rngm = random.Random(54)
    for lv in P["levels"]:
        L, z = lv["level"], lv["level"] * SH
        units = {u["id"]: u for u in lv["units"]}
        for k, wl in enumerate(lv["walls"]):                     # walls along the walks
            if near(wl["line"]):
                band("Mall_Wall%d_%d" % (L, k), [loc(q) for q in wl["line"]], z, z + SH, pl, thick=0.3)
        for k, f in enumerate(lv["fronts"]):                     # the outlets' fronts, in the approved shopfront's parts
            if not near((f["a"], f["b"])):
                continue
            a, b = loc(f["a"]), loc(f["b"])
            u_ = units.get(f["unit"])
            kind_ = P["outlets"].get(u_.get("outlet"), {}) if u_ else {}
            sign = mall_sign_mat(kind_.get("sign", "teal"))
            FH = ML["front_glass"]
            g_ = band("Mall_FrontGlass%d_%d" % (L, k), [a, b], z, z + FH, glassp, thick=0.03)   # see into the outlet
            setp(g_, "visible_shadow", False)
            n = max(2, round(math.hypot(b[0] - a[0], b[1] - a[1]) / ML["mullion_every"]))
            for i in range(n + 1):
                t0 = i / n
                c = (a[0] + (b[0] - a[0]) * t0, a[1] + (b[1] - a[1]) * t0)
                d = ((b[0] - a[0]) / n * 0.02, (b[1] - a[1]) / n * 0.02)
                band("Mall_FrontMullion%d_%d_%d" % (L, k, i), [(c[0] - d[0], c[1] - d[1]), (c[0] + d[0], c[1] + d[1])], z, z + FH, mullion, thick=0.09)
            band("Mall_FrontHead%d_%d" % (L, k), [a, b], z + FH, z + FH + 0.06, mullion, thick=0.1)
            band("Mall_FrontSign%d_%d" % (L, k), [a, b], z + FH + 0.06, z + FH + 0.66, sign, thick=0.3)
            # above the sign in the mall's own styling (the owner, 2026-11-26: "store's top part of the entrance can be better
            # and aligned with the styling of the mall"): brass trim round the sign, a line of light under it, and the
            # galleries' timber slats up to the next floor
            band("Mall_FrontSignTrimB%d_%d" % (L, k), [a, b], z + FH + 0.04, z + FH + 0.1, brass, thick=0.36)
            band("Mall_FrontSignTrimT%d_%d" % (L, k), [a, b], z + FH + 0.62, z + FH + 0.68, brass, thick=0.36)
            band("Mall_FrontSignLight%d_%d" % (L, k), [a, b], z + FH + 0.0, z + FH + 0.04, strip, thick=0.4)
            band("Mall_FrontAbove%d_%d" % (L, k), [a, b], z + FH + 0.68, z + SH - 0.7, slab_under, thick=0.3)   # the slats up to the next floor
            band("Mall_FrontAboveTrim%d_%d" % (L, k), [a, b], z + SH - 0.72, z + SH - 0.68, brass, thick=0.34)
            if u_ is not None and any(seen((a[0] + (b[0] - a[0]) * t_, a[1] + (b[1] - a[1]) * t_, z + h_))
                                      for t_ in (0.0, 0.25, 0.5, 0.75, 1.0) for h_ in (0.3, 1.6, 2.9)):
                mall_outlet(u_, a, b, z, kind_, Ls, rngm, (glassp, brass))   # its inside, only where its front is seen
                built_in += 1
        if L == 0:
            continue
        holes = [[loc(q) for q in v] for v in lv["voids"]]
        for k, w in enumerate(lv["walks"]):                      # the upper floors' walks beyond the court: the concourses'
            if k > 0 and near(w) and len(w) == 4:               # ceilings, timber underneath
                q = [loc(p_) for p_ in w]
                bx0, bx1 = min(x for x, _ in q), max(x for x, _ in q)
                by0, by1 = min(y for _, y in q), max(y for _, y in q)
                box("Mall_Walk%d_%d" % (L, k), bx0 - 0.3, bx1 + 0.3, by0, by1, z - 0.7, z - 0.02, slab_under, bevel=0)
                box("Mall_WalkTop%d_%d" % (L, k), bx0 - 0.3, bx1 + 0.3, by0, by1, z - 0.02, z + 0.02, floor, bevel=0)
        plate("Mall_Gallery%d" % L, court, holes, z, slab_under)          # the gallery's floor, its underside brown
        plate("Mall_GalleryTop%d" % L, court, holes, z + 0.02, floor)
        for k, v in enumerate(holes):
            band("Mall_GalleryEdge%d_%d" % (L, k), v, z - 0.7, z, edge, closed=True)   # the cream edge round the opening
            band("Mall_EdgeTrim%d_%d" % (L, k), [(x * 1.002, y * 1.002) for x, y in v], z - 0.05, z, brass, closed=True)
            if L == 1:                                            # the brass inlay on the floor below, tracing the curve
                band("Mall_Inlay%d" % k, [(x * 1.0, y * 1.0) for x, y in v], 0.0, 0.012, brass, closed=True, thick=0.25)
            run = 0.0                                             # slender columns under the edge, every 16 m or so
            for i in range(len(v)):
                (ax, ay), (bx, by) = v[i], v[(i + 1) % len(v)]
                run += math.hypot(bx - ax, by - ay)
                if run >= ML["column_every"]:
                    run = 0.0
                    cxn, cyn = ax * 1.035, ay * 1.012               # set back under the gallery
                    cylz(None, "Mall_Column%d_%d_%d" % (L, k, i), column, cxn, cyn, z - SH, 0.32, SH - 0.7, segs=20)
                    for zz in (z - SH, z - 0.95):
                        cylz(None, "Mall_Collar%d_%d_%d_%d" % (L, k, i, int(zz)), brass, cxn, cyn, zz, 0.42, 0.25, segs=20)
            band("Mall_EdgeLine%d_%d" % (L, k), [(x * 1.004, y * 1.004) for x, y in v], z - 0.72, z - 0.66, strip, closed=True)   # its line of light
            band("Mall_EdgeLineB%d_%d" % (L, k), [(x * 0.99, y * 0.99) for x, y in v], z - 0.71, z - 0.68, strip, closed=True)
        for k, r in enumerate(lv["rails"]):
            if near(r):
                rl = [loc(q) for q in r]
                g_ = band("Mall_Balustrade%d_%d" % (L, k), rl, z + 0.08, z + 1.02, glassp, thick=0.03)
                setp(g_, "visible_shadow", False)
                band("Mall_Rail%d_%d" % (L, k), rl, z + 1.02, z + 1.1, brass, thick=0.1)
                band("Mall_Shoe%d_%d" % (L, k), rl, z, z + 0.08, brass, thick=0.12)
                run = 0.0
                for i in range(len(rl) - 1):                      # plants hanging over the parapet every 9 m or so
                    (ax, ay), (bx, by) = rl[i], rl[i + 1]
                    run += math.hypot(bx - ax, by - ay)
                    if run >= ML["hang_every"]:
                        run = 0.0
                        hang_plant("Mall_Hang%d_%d_%d" % (L, k, i), ax, ay, z + 1.1)
        for k, br in enumerate(lv["bridges"]):
            q = [loc(p_) for p_ in br]                               # a bridge is a plain rectangle: built solid
            bx0, bx1 = min(x for x, _ in q), max(x for x, _ in q)
            by0, by1 = min(y for _, y in q), max(y for _, y in q)
            box("Mall_Bridge%d_%d" % (L, k), bx0, bx1, by0, by1, z - 0.7, z - 0.02, edge, bevel=0)
            box("Mall_BridgeTop%d_%d" % (L, k), bx0, bx1, by0, by1, z - 0.02, z + 0.02, floor, bevel=0)
            box("Mall_BridgeUnder%d_%d" % (L, k), bx0 + 0.05, bx1 - 0.05, by0 + 0.05, by1 - 0.05, z - 0.72, z - 0.68, slab_under, bevel=0)
        for v in holes:                                            # lanterns under the gallery's edge, every 14 m or so
            run = 0.0
            for i in range(len(v)):
                (ax, ay), (bx, by) = v[i], v[(i + 1) % len(v)]
                run += math.hypot(bx - ax, by - ay)
                if run >= 14.0:
                    run = 0.0
                    nm = "Hall_LanternM%d" % lanterns                   # a hidden light under the edge's line of light
                    lt = bpy.data.lights.new(nm + "_Light", 'POINT')
                    lt.energy = 220.0
                    setp(lt, "shadow_soft_size", 0.2)
                    po = bpy.data.objects.new(nm + "_Light", lt)
                    po.location = (ax, ay, z - 1.5)
                    COL.objects.link(po)
                    lanterns += 1
    RZ = SH * len(P["levels"])                                     # the roof, its round skylights glazed
    eyes = [[loc(q) for q in o] for o in P["roof"]["openings"]]
    plate("Mall_Roof", [(-W2 - 12, -D2 - 12), (W2 + 12, -D2 - 12), (W2 + 12, D2 + 12), (-W2 - 12, D2 + 12)], eyes, RZ, slab_under)
    glass = bpy.data.materials.get("M_MallSkyGlass") or mall_glass("M_MallSkyGlass", ML["sky_glass"], ML["sky_glass_alpha"])
    for k, e in enumerate(eyes):
        g = plate("Mall_SkyGlass%d" % k, e, [], RZ + 0.6, glass)   # tinted: half the sun's light comes through
        band("Mall_SkyCurb%d" % k, e, RZ, RZ + 0.7, sd, closed=True)
        cx = sum(x for x, _ in e) / len(e)
        cy = sum(y for _, y in e) / len(e)
        rr = max(math.hypot(x - cx, y - cy) for x, y in e)
        for j in range(-6, 7):                                    # the roof light's diamond lattice
            o_ = j * rr / 6.5
            h_ = math.sqrt(max(0.0, rr * rr - o_ * o_))
            c45, s45 = math.cos(math.pi / 4), math.sin(math.pi / 4)
            for nmk, (ux, uy) in (("A", (c45, s45)), ("B", (-s45, c45))):
                vx, vy = -uy, ux
                a_ = (cx + vx * o_ - ux * h_, cy + vy * o_ - uy * h_)
                b_ = (cx + vx * o_ + ux * h_, cy + vy * o_ + uy * h_)
                band("Mall_SkyBar%s%d_%d" % (nmk, k, j), [a_, b_], RZ + 0.5, RZ + 0.62, ML["lattice"] and wd, thick=0.16)
    for k, e in enumerate(eyes):                                  # a chandelier of globes hanging in each roof light
        cx = sum(x for x, _ in e) / len(e)
        cy = sum(y for _, y in e) / len(e)
        cz = RZ - ML["chandelier_drop"]
        cylz(None, "Mall_ChandRod%d" % k, brass, cx, cy, cz + 1.2, 0.05, RZ - cz - 1.2, segs=8)
        cylz(None, "Mall_ChandRing%d" % k, brass, cx, cy, cz + 1.1, 2.4, 0.12, segs=36)
        for j in range(ML["globes"]):
            a_ = 2 * math.pi * j / ML["globes"]
            rr_ = 2.4 if j % 2 == 0 else 1.4
            gz = cz + (0.0 if j % 2 == 0 else -0.9)
            gx, gy = cx + math.cos(a_) * rr_, cy + math.sin(a_) * rr_
            cylz(None, "Mall_ChandDrop%d_%d" % (k, j), brass, gx, gy, gz + 0.3, 0.02, cz + 1.1 - gz - 0.3, segs=6)
            ball(None, "Mall_Globe%d_%d" % (k, j), globe, gx, gy, gz, 0.6, 0.6, 0.6)
        lt = bpy.data.lights.new("Hall_LanternC%d_Light" % k, 'POINT')   # the chandelier's light, of the hall lantern's kind
        lt.energy = 900.0
        setp(lt, "shadow_soft_size", 2.0)
        po = bpy.data.objects.new("Hall_LanternC%d_Light" % k, lt)
        po.location = (cx, cy, cz - 0.3)
        COL.objects.link(po)
    for k, e in enumerate(eyes):                                  # banners hanging in each roof light, round its chandelier,
        cx = sum(x for x, _ in e) / len(e)                        # where the daylight falls on them (between, under the solid
        cy = sum(y for _, y in e) / len(e)                        # roof, they hung in the dark)
        rr = max(math.hypot(x - cx, y - cy) for x, y in e)
        for j in range(ML["banners_per"]):
            a_ = 2 * math.pi * (j + 0.5) / ML["banners_per"]
            px, py = cx + math.cos(a_) * rr * 0.72, cy + math.sin(a_) * rr * 0.72
            tx, ty = -math.sin(a_) * 0.9, math.cos(a_) * 0.9          # along the ring
            m_ = banners[(k + j) % len(banners)]
            band("Mall_Banner%d_%d" % (k, j), [(px - tx, py - ty), (px + tx, py + ty)], RZ - ML["banner_drop"], RZ - 0.3, m_, thick=0.04)
            band("Mall_BannerBar%d_%d" % (k, j), [(px - tx * 1.1, py - ty * 1.1), (px + tx * 1.1, py + ty * 1.1)], RZ - 0.35, RZ - 0.25, brass, thick=0.08)
    for k, r in enumerate(P["ramps"]):                             # the moving stairs, with their side walls
        (ax, ay), (bx, by) = loc(r["a"]), loc(r["b"])
        za, zb, w = r["from"] * SH, r["to"] * SH, r["width"]
        dx, dy = bx - ax, by - ay
        n = math.hypot(dx, dy) or 1.0
        ox, oy = -dy / n * w / 2, dx / n * w / 2
        bm = bmesh.new()
        q = [bm.verts.new(c) for c in ((ax + ox, ay + oy, za), (bx + ox, by + oy, zb), (bx - ox, by - oy, zb), (ax - ox, ay - oy, za))]
        bm.faces.new(q)
        mesh_obj("Mall_Ramp%d" % k, bm, tread)
        bm = bmesh.new()                                          # its underside, timber as the galleries' (from below the
        q = [bm.verts.new(c) for c in ((ax + ox, ay + oy, za - 0.6), (bx + ox, by + oy, zb - 0.6),   # dark tread read as a hole)
                                       (bx - ox, by - oy, zb - 0.6), (ax - ox, ay - oy, za - 0.6))]
        bm.faces.new(q)
        mesh_obj("Mall_RampUnder%d" % k, bm, slab_under)
        for sgn in (1, -1):
            sx, sy = ox * sgn, oy * sgn
            bm = bmesh.new()                                      # the truss under the stair, brown, its edge cream
            v = [bm.verts.new(c) for c in ((ax + sx, ay + sy, za - 0.6), (bx + sx, by + sy, zb - 0.6), (bx + sx, by + sy, zb + 0.1), (ax + sx, ay + sy, za + 0.1))]
            bm.faces.new(v)
            mesh_obj("Mall_RampSide%d_%d" % (k, sgn), bm, wd)
            bm = bmesh.new()                                      # its glass side and brass rail
            v = [bm.verts.new(c) for c in ((ax + sx, ay + sy, za + 0.1), (bx + sx, by + sy, zb + 0.1), (bx + sx, by + sy, zb + 1.0), (ax + sx, ay + sy, za + 1.0))]
            bm.faces.new(v)
            g_ = mesh_obj("Mall_RampGlass%d_%d" % (k, sgn), bm, glassp)
            setp(g_, "visible_shadow", False)
            bm = bmesh.new()
            v = [bm.verts.new(c) for c in ((ax + sx, ay + sy, za + 1.0), (bx + sx, by + sy, zb + 1.0), (bx + sx, by + sy, zb + 1.08), (ax + sx, ay + sy, za + 1.08))]
            bm.faces.new(v)
            mesh_obj("Mall_RampRail%d_%d" % (k, sgn), bm, brass)

    def tree_(name, x, y, z, h=4.0, sc=1.0):
        cylz(None, name + "_Trunk", wd, x, y, z, 0.2 * sc, h, r_top=0.13 * sc)
        for (dx, dy, dz, rr_, m_) in ((0, 0, h + 1.3, 1.7, leaf), (1.1, 0.4, h + 0.7, 1.25, leaf), (-1.0, 0.3, h + 0.8, 1.15, leaf2),
                                      (0.3, -0.9, h + 0.6, 1.1, leaf), (0.2, 0.2, h + 2.4, 1.05, leaf2)):
            ball(None, name + "_Leaf", m_, x + dx * sc, y + dy * sc, z + dz, 2 * rr_ * sc, 2 * rr_ * sc, 1.6 * rr_ * sc)
    for k, g in enumerate(P["gardens"]):
        if not near([g["pos"]]):
            continue
        (gx, gy), (rx, ry) = loc(g["pos"]), g["r"]
        for nm, m_, r_, h_ in (("Mall_GardenKerb%d" % k, sd, 1.0, 0.45), ("Mall_Garden%d" % k, grass, 0.95, 0.5)):
            cylz(None, nm, m_, gx, gy, 0.0, r_, h_, segs=40)
            ob = bpy.data.objects.get(nm)
            if ob:
                ob.scale = (rx, ry, 1.0)
        for j, (tx, tz, sp) in enumerate(g["trees"]):
            x_, y_ = loc((tx, tz))
            tree_("Mall_Tree%d_%d" % (k, j), x_, y_, 0.5, h=5.0, sc=1.3)
        for j in range(ML["flowers_per"]):                         # flowers in the beds
            a_ = rngm.uniform(0, 2 * math.pi)
            d_ = math.sqrt(rngm.random()) * 0.85
            fx_, fy_ = gx + math.cos(a_) * rx * d_, gy + math.sin(a_) * ry * d_
            ball(None, "Mall_Flower%d_%d" % (k, j), flowers[j % len(flowers)], fx_, fy_, 0.62, 0.32, 0.32, 0.22)
    for k, f in enumerate(P["fountains"]):
        fx, fy = loc(f["pos"])
        cylz(None, "Mall_Fountain%d" % k, sd, fx, fy, 0.0, f["r"], 0.6, segs=40)
        cylz(None, "Mall_FountainWater%d" % k, water, fx, fy, 0.0, f["r"] - 0.3, 0.62, segs=40)
        cylz(None, "Mall_FountainBowl%d" % k, pl, fx, fy, 0.6, 0.6, 1.6, r_top=1.4, segs=24)
    for k, pt in enumerate(P["planters"]):
        if not near([pt["pos"]]):
            continue
        x_, y_ = loc(pt["pos"])
        z_ = pt["level"] * SH
        cylz(None, "Mall_Planter%d" % k, sd, x_, y_, z_, pt["r"], 0.6, segs=24)
        tree_("Mall_PlanterTree%d" % k, x_, y_, z_ + 0.6, h=2.4, sc=0.8)
    for k, bn in enumerate(P["benches"]):
        if not near([bn["pos"]]):
            continue
        x_, y_ = loc(bn["pos"])
        g_ = group("Mall_Bench%d" % k, x_, y_, -math.radians(bn["yaw"]))
        rbox(g_, "Mall_BenchSeat%d" % k, wd, 0, 0, 0.45, 1.8, 0.5, 0.08, 0.02)
        for sx_ in (-0.75, 0.75):
            rbox(g_, "Mall_BenchLeg%d_%d" % (k, int(sx_ * 10)), sd, sx_, 0, 0.21, 0.1, 0.45, 0.42, 0.01)
    if view == "entrance":
        mall_entrance_doors(P, loc, SH, glassp, brass, edge, strip, globe, slab_under)
        mall_porch(LOT["shell"], loc, brass, globe)
        mall_floods(loc)
        mall_forecourt(blk, loc, strip)
        people([("PlayerA", 1.0, 196.0), ("PlayerB", -6.0, 179.0), ("Elder", 7.0, 186.0), ("Shopkeeper", -2.0, 150.0)])
    else:
        people([("PlayerA", 3.0, -12.0), ("PlayerB", -7.0, -4.0), ("Elder", 9.0, 6.0)])
    sun_and_sky(SCENE, (0.35, -0.30, 0.89), 8.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.45)
    hlog("  Eastgate's court from its plan: %d lanterns, %d skylights, %d moving stairs; %d outlets' insides in view" % (
        lanterns, len(eyes), len(P["ramps"]), built_in))
    name = "Cam_test" if os.environ.get("WASTED_CAM") else ("Cam_mall_entrance" if view == "entrance" else "Cam_mall")
    return camera(SCENE, name, *cam_at)


def game_size_showroom():
    """The furniture outlet's hall at the game's size (the owner, 2026-11-26: references at the game's own size): its lot
    in World/city_01/blocks/b_14_8.json is 66 x 46 m with a hall 8.5 m high and a skylight 2.2 m wide; the approved
    showroom (Art/Reference/Shops/ShopsFurnitureOutlet.png, furniture_kit_17.py showroom_scene) was drawn as a corner
    34 x 22 x 8 m with a 4.5 m skylight. Its design is kept: the same function, its sizes taken from the lot, its
    trusses at the same spacing."""
    lot = None
    blk = json.load(open("C:/Projects/Wasted/World/city_01/blocks/b_14_8.json", encoding="utf-8"))
    lot = next(l for l in blk["lots"] if l.get("kind") == "furniture")
    x0, y0, x1, y1 = lot["rect"]
    W, D, H, sky = x1 - x0, y1 - y0, lot["hall_height"], lot["skylight"]
    src = open("C:/Projects/Wasted/Tools/blender/furniture_kit_17.py", encoding="utf-8").read()
    i0 = src.index("def showroom_scene(Ls):")
    i1 = src.index(chr(10) + "def ", i0 + 10)
    f = src[i0:i1]
    nt = round((W - 4) / 6.0) + 1                           # trusses about every 6 m, as the game's (outlet.js)
    for a, b in (("W, D, H = 34.0, 22.0, 8.0", "W, D, H = %.1f, %.1f, %.1f" % (W, D, H)),
                 ("for i in range(7):", "for i in range(%d):" % nt),
                 ("x = -W / 2 + 2 + i * (W - 4) / 6", "x = -W / 2 + 2 + i * (W - 4) / %d" % (nt - 1)),
                 ("for j in range(6):", "for j in range(%d):" % (nt - 1)),
                 ("x0 = -W / 2 + 2.3 + j * (W - 4) / 6", "x0 = -W / 2 + 2.3 + j * (W - 4) / %d" % (nt - 1)),
                 ("x1 = x0 + (W - 4) / 6 - 0.6", "x1 = x0 + (W - 4) / %d - 0.6" % (nt - 1))):
        assert f.count(a) == 1, a
        f = f.replace(a, b)
    exec(compile(f, "showroom_scene (game size)", "exec"), globals())
    globals()["HALL"] = (W, D, H, nt)
    hlog("  the outlet's hall at the game's size: %.0f x %.0f x %.1f m, %d trusses" % (W, D, H, nt))


def glazed_roof():
    """The outlet's roof as the game's (Prototype/play/outlet.js): solid over the hall's edges, and down its middle a band of
    skylights (from 11 m in from each long wall) between the trusses, 0.9 m clear of each, glazed (the owner, 2026-11-26:
    "cover the ceiling with a glass so light comes but not rain and snow"). The glass lets the sun's light through."""
    W, D, H, nt = HALL
    for ob in [o for o in bpy.data.objects if o.name.startswith(("Hall_RoofA", "Hall_RoofB", "Hall_RoofEnd"))]:
        bpy.data.objects.remove(ob, do_unlink=True)
    rm, sd = M["plaster"], M["stone_dark"]
    b0, b1 = -D / 2 + 11.0, D / 2 - 11.0
    box("Hall_RoofN", -W / 2 - 0.5, W / 2 + 0.5, -D / 2 - 0.5, b0, H, H + 0.3, rm, bevel=0)
    box("Hall_RoofS", -W / 2 - 0.5, W / 2 + 0.5, b1, D / 2 + 0.5, H, H + 0.3, rm, bevel=0)
    tx = [-W / 2 + 2 + i * (W - 4) / (nt - 1) for i in range(nt)]
    edges = [-W / 2 - 0.5] + [x for t in tx for x in (t - 0.9, t + 0.9)] + [W / 2 + 0.5]
    glass = bpy.data.materials.get("M_RoofGlass")
    if glass is None:
        glass = bpy.data.materials.new("M_RoofGlass")
        glass.use_nodes = True
        nt_ = glass.node_tree
        b = next(n for n in nt_.nodes if n.type == 'BSDF_PRINCIPLED')
        b.inputs["Base Color"].default_value = (0.85, 0.92, 0.95, 1.0)
        b.inputs["Roughness"].default_value = 0.05
        b.inputs["Alpha"].default_value = 0.12                  # clear: a faint sheen, the sky seen through it
        setp(glass, "blend_method", 'BLEND')
    k = 0
    for i in range(0, len(edges) - 1, 2):                        # solid strips over the trusses and the ends
        box("Hall_RoofStrip%d" % i, edges[i], edges[i + 1], b0, b1, H, H + 0.3, rm, bevel=0)
    for i in range(1, len(edges) - 1, 2):                        # the skylights between them, glazed, on stone curbs
        a, c = edges[i], edges[i + 1]
        g = box("Hall_Sky%d" % k, a, c, b0, b1, H + 0.42, H + 0.45, glass, bevel=0)
        for kk in ("visible_shadow",):
            for o in [o for o in bpy.data.objects if o.name.startswith("Hall_Sky%d" % k)]:
                setp(o, kk, False)                             # the sun's light passes the glass
        for (p0, p1, q0, q1) in ((a, c, b0 - 0.15, b0), (a, c, b1, b1 + 0.15), (a - 0.15, a, b0, b1), (c, c + 0.15, b0, b1)):
            box("Hall_Curb%d_%d" % (k, int(p0 * 10)), p0, p1, q0, q1, H + 0.3, H + 0.55, sd, bevel=0)
        k += 1
    hlog("  the roof: a band of %d glazed skylights %.0f m wide down the middle" % (k, b1 - b0))


def shopfloor(Ls):
    """A shop's ground floor at the game's size (the owner, 2026-11-26: "take the size from game for bank, grocers or
    library"): the smallest grocer's lot, Northwood Grocer, 12 m along the street and 11 m deep, its floor 3 m high under
    the ceiling (storeys.json); the street side a glass shop front of framed panes and a door, the day coming in there;
    the kit's shop pieces in it (shelving in rows, the counter and till, a rail, a table, a board) and four pendants. The
    camera stands at the back, the shop front along the right, as a home room's window."""
    L = [l for l in Ls if l["key"] == "plain"][0]
    rng = random.Random(53)
    X, Y, H = 5.5, 6.0, 3.0                                   # half its depth (to the street at +x), half its length, its height
    pl, plb, bd, wd = M["plaster"], M["plaster_b"], M["boards"], M["wood"]
    box("Shp_Floor", -X, X, -Y, Y, -0.1, 0.0, bd, bevel=0)
    box("Shp_Ceiling", -X - 0.3, X + 0.3, -Y - 0.3, Y + 0.3, H, H + 0.2, pl, bevel=0)
    box("Shp_Back", -X - 0.3, -X, -Y, Y, 0, H, plb, bevel=0)
    box("Shp_Left", -X - 0.3, X + 0.3, Y, Y + 0.3, 0, H, pl, bevel=0)
    for i in range(5):                                       # beams across, as the town's rooms
        y = -Y + 1.2 + i * (2 * Y - 2.4) / 4
        box("Shp_Beam%d" % i, -X, X, y - 0.09, y + 0.09, H - 0.18, H, wd, bevel=0.015)
    # the shop front as the approved build types draw it (Art/Reference/Buildings/BuildingsTypes_1.png, shop_front_a;
    # building_kit_07.py shopfront()): pilasters on stone bases every 2.2 m or so, a stall riser to 0.7 m under a stone
    # sill, tall panes with a centre bar and a transom, a glazed door in the middle bay, a fascia over them and a striped
    # awning out over the pavement; teal frames, a cream and terracotta awning. The kit's depth v (0 the street face,
    # t the room's face) runs here along x, the kit's length along y.
    t = 0.35
    fr = bpy.data.materials.get("M_SignTeal") or flat("M_SignTeal", (0.16, 0.30, 0.31))
    sd = M["stone_dark"]

    def blk(nm, m, a, b, v0, v1, z0, z1):
        box(nm, X + t - v1, X + t - v0, a, b, z0, z1, m, bevel=0.01)
    W, y0 = 2 * Y, -Y
    n = max(3, round((W - 0.8) / 2.2))
    pw = 0.32
    ys = [y0 + 0.4 + i * (W - 0.8) / n for i in range(n + 1)]
    di = n // 2
    blk("Shp_EndA", pl, y0 - 0.3, ys[0] - pw / 2, 0.0, t, 0.0, H)
    blk("Shp_EndB", pl, ys[-1] + pw / 2, Y + 0.3, 0.0, t, 0.0, H)
    for i, yp in enumerate(ys):
        blk("Shp_Pilaster%d" % i, fr, yp - pw / 2, yp + pw / 2, -0.16, t, 0.0, 2.55)
        blk("Shp_PilBase%d" % i, sd, yp - pw / 2 - 0.04, yp + pw / 2 + 0.04, -0.2, t, 0.0, 0.3)
    for i in range(n):
        a, b = ys[i] + pw / 2, ys[i + 1] - pw / 2
        if i == di:                                           # the door: panels low, glass above, a transom light
            for k in range(3):
                blk("Shp_DoorPanel%d" % k, fr, a + k * (b - a) / 3 + 0.01, a + (k + 1) * (b - a) / 3 - 0.01, t - 0.2, t - 0.12, 0.0, 0.9)
            blk("Shp_DoorRail", fr, a, b, t - 0.2, t - 0.12, 1.9, 1.98)
            blk("Shp_DoorHead", fr, a, b, t - 0.3, t - 0.2, 2.3, 2.55)
            continue
        blk("Shp_Riser%d" % i, fr, a, b, -0.06, t, 0.0, 0.7)
        blk("Shp_Sill%d" % i, sd, a - 0.02, b + 0.02, -0.12, t, 0.7, 0.78)
        blk("Shp_Bar%d" % i, fr, (a + b) / 2 - 0.03, (a + b) / 2 + 0.03, t - 0.12, t - 0.06, 0.78, 2.55)
        blk("Shp_Transom%d" % i, fr, a, b, t - 0.12, t - 0.06, 2.0, 2.05)
    blk("Shp_Fascia", fr, y0 + 0.25, Y - 0.25, -0.22, t, 2.55, H + 0.2)
    blk("Shp_Cornice", sd, y0 + 0.2, Y - 0.2, -0.3, t, H + 0.2, H + 0.32)
    p20 = math.radians(20)                                    # the awning: striped canvas sloping out over the pavement
    na, out, za = max(4, int((W - 1.2) / 0.45)), 1.5, 2.5
    Lw = out / math.cos(p20)
    for i in range(na):
        a = y0 + 0.6 + i * (W - 1.2) / na
        b = y0 + 0.6 + (i + 1) * (W - 1.2) / na
        m = M["cream"] if i % 2 == 0 else M["terra"]
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        me = bpy.data.meshes.new("Shp_Awning%d" % i)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(m)
        ob = bpy.data.objects.new("Shp_Awning%d" % i, me)
        ob.scale = (Lw, b - a, 0.05)
        ob.rotation_euler = (0, p20, 0)
        ob.location = (X + t + out / 2, (a + b) / 2, za - out * math.tan(p20) / 2)
        COL.objects.link(ob)
        rb = bmesh.new()                                      # the same stripe rolled up under the fascia (mornings)
        bmesh.ops.create_cone(rb, cap_ends=True, segments=16, radius1=0.11, radius2=0.11, depth=b - a)
        rme = bpy.data.meshes.new("Shp_AwningRoll%d" % i)
        rb.to_mesh(rme)
        rb.free()
        rme.materials.append(m)
        ro = bpy.data.objects.new("Shp_AwningRoll%d" % i, rme)
        ro.rotation_euler = (math.radians(90), 0, 0)
        ro.location = (X + t + 0.14, (a + b) / 2, za + 0.02)
        COL.objects.link(ro)
    box("Shp_Front", -X - 0.3, X + 0.35, -Y - 0.3, -Y, 0, H, pl, bevel=0)   # the wall the camera stands at
    out = bpy.data.materials.get("M_OutsidePaving") or painted("M_OutsidePaving", (0.22, 0.21, 0.25), (0.46, 0.42, 0.37), noise_scale=0.5)
    box("Shp_Outside", X + 0.35, X + 40.0, -Y - 30.0, Y + 30.0, -0.4, -0.1, out, bevel=0)
    # the light the far wall throws back onto the people, as in the home room (BOUNCE_CARD)
    bc = bpy.data.lights.new("BounceCard", 'AREA')
    bc.shape = 'RECTANGLE'
    bc.size, bc.size_y = 2 * X * 0.9, H * 0.8
    bo = bpy.data.objects.new("BounceCard", bc)
    bo.location = (0.0, -Y + 0.05, H * 0.45)
    bo.rotation_euler = (math.radians(90), 0, 0)
    for k in ("visible_camera", "visible_glossy", "visible_transmission"):
        setp(bo, k, False)
    COL.objects.link(bo)
    for spec, x, y, yaw in (("p_gondola", -1.6, 2.6, 0.0), ("p_gondola", -1.6, 0.4, 0.0), ("p_gondola", 1.2, 2.6, 0.0),
                            ("p_gondola", 1.2, 0.4, 0.0), ("p_counter", -3.4, -3.2, math.pi / 2), ("p_rail", -5.0, 4.6, math.pi / 2),
                            ("p_table", 3.4, -2.6, 0.0), ("p_aboard", 4.4, -1.0, -0.4), ("p_form", -4.8, 1.6, math.pi / 2),
                            ("d_pendant", -2.4, -2.4, 0), ("d_pendant", 2.4, -2.4, 0), ("d_pendant", -2.4, 2.4, 0), ("d_pendant", 2.4, 2.4, 0)):
        P = group("Shp_%s" % spec, x, y, yaw)
        place(spec, P, L, rng)
    people([("PlayerA", 0.4, -1.4)])
    sun_and_sky(SCENE, (0.78, 0.30, 0.42), 8.5, (1.0, 0.86, 0.68), (0.42, 0.55, 0.82), 0.25)
    return camera(SCENE, "Cam_shop", (-X + 0.5, -Y + 0.5, 1.7), (X * 0.4, Y * 0.5, 1.1), 16.0)


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
    elif PLACE == "hallway":
        scene = fresh("Furn_corridor")
        extra_mats()
        cam = corridor(lines())
        greyer(ROOM_GREYER)
        bounce_more(ROOM_BOUNCE)
        each_hour(scene, cam, 1.3, "Hallway", dict(day_lamps=0.0, day_fires=0.0))
    elif PLACE == "shop":                            # a shop with no fire (bank, grocer and the like; the owner, 2026-11-26)
        scene = fresh("Furn_shopfloor")
        extra_mats()
        cam = shopfloor(lines())
        greyer(ROOM_GREYER)
        bounce_more(ROOM_BOUNCE)
        each_hour(scene, cam, 1.3, "Shop", dict(day_lamps=0.06, day_fires=0.0))   # lamps low by day, as the bakery
    elif PLACE == "bakery":
        scene = fresh("Furn_bakery")
        bakery_mats()
        (nm, loc, tgt, lens) = bakery_scene(False)[0]
        for ob in scene.objects:                     # bakery_scene() dimmed its pendants by day: back to full, the hours set them
            if ob.type == 'LIGHT' and ob.name.startswith("Pen_Light"):
                ob.data.energy /= 0.06
        greyer(ROOM_GREYER)                                 # in the world's light, as the home room (the owner, 2026-11-26)
        bounce_more(ROOM_BOUNCE)
        each_hour(scene, camera(scene, "Cam_bakery", loc, tgt, lens), 1.15, "Bakery", dict(day_lamps=0.06))
    elif PLACE == "mall":                            # Eastgate Mall's court from its plan (the owner, 2026-11-26)
        scene = fresh("Furn_mall")
        extra_mats()
        cam = mall_court(lines())
        if os.environ.get("WASTED_MALLTOP"):             # a check from above: the galleries without the roof
            for ob in [o for o in scene.objects if o.name.startswith(("Mall_Roof", "Mall_Sky", "Mall_Banner", "Mall_Chand", "Mall_Globe"))]:
                ob.hide_render = True
            cam = camera(scene, "Cam_malltop", (0.0, 0.0, 60.0), (0.0, 0.01, 0.0), 30.0, ortho=210.0)
        bounce_more(ROOM_BOUNCE)
        each_hour(scene, cam, 0.5, "MallCourt", dict(day_lamps=0.15))   # lanterns low by day, as the big hall
    elif PLACE == "mall_entrance":                   # Eastgate's main entrance, the south concourse (the owner, 2026-11-26)
        scene = fresh("Furn_mall")
        extra_mats()
        cam = mall_court(lines(), view="entrance")
        bounce_more(ROOM_BOUNCE)
        each_hour(scene, cam, 0.5, "MallEntrance", dict(day_lamps=0.15))
    elif PLACE == "egg_hall":                        # an egg hall: the Food Market under Eastgate's south-west egg (the owner, 2026-11-27)
        scene = fresh("Furn_mall")
        extra_mats()
        cam = mall_court(lines(), view="egg")
        bounce_more(ROOM_BOUNCE)
        each_hour(scene, cam, 0.5, "EggHall", dict(day_lamps=0.15))
    elif PLACE == "outlet":
        scene = fresh("Furn_showroom")
        game_size_showroom()                         # the approved showroom at the game's outlet size
        (nm, loc, tgt, lens) = showroom_scene(lines())[0]
        glazed_roof()
        bounce_more(ROOM_BOUNCE)
        # by day the skylights' daylight leads, the lanterns low (the owner, 2026-11-26: "none of the lights look right":
        # at full by day the 34 lanterns made day look like night)
        each_hour(scene, camera(scene, "Cam_showroom", loc, tgt, lens), 0.5, "FurnitureOutlet", dict(day_lamps=0.15))
    hlog("DONE")


try:
    main_hours()
except Exception:
    hlog("FAILED:\n" + traceback.format_exc())
