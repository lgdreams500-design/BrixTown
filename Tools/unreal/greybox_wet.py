r"""Wet paving for /Game/Greybox/M_Greybox (WLD-037, ADR-0016): in rain the ground darkens, takes a faint sheen and fills
with puddles that hold the sky, and rings spread where the drops land (greybox_ripples.hlsl: they tilt the surface, so
the puddles' reflections break up, and catch a little light); walls and everything else out in the rain go a little
darker. Four parameters, WetDark, WetSheen, Puddles and Ripples, are 0 in the dry; ABrixWeather sets them through ABrixTownActor::SetWet from the clock and
World/rules/sky.json "rain" (wet_dark, wet_sheen, puddles, times how wet it is now). The ground is what faces up;
puddles lie where a soft world-space noise is high. Dry, the material is as before (matte, ART-002 W1).

make_greybox_material.py calls add_wet() when it makes the material anew. Run on its own, this adds the wet nodes to the
material as it is, in place (so the kit's meshes keep pointing at it and import_building_kit.py need not run again):
  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\greybox_wet.py" -unattended -nullrhi -nosplash
The asset is binary and kept in Git LFS (ADR-0002): git lfs lock it first.
"""
import os
import unreal

lib = unreal.MaterialEditingLibrary
FULL = "/Game/Greybox/M_Greybox"
DRY_ROUGHNESS = 0.85      # as make_greybox_material.py
SHEEN_ROUGHNESS = 0.6     # the wet ground at full sheen: a faint sheen, still mostly matte
PUDDLE_ROUGHNESS = 0.28   # standing water: holds the grey sky softly, not a mirror
PUDDLE_SCALE = 0.0035     # the noise's scale (per centimetre): puddles a metre or two across
PUDDLE_LEVEL = 0.7        # puddles where the noise is above this: here and there, not a flood
RIPPLE_TILT = 0.35        # how far a ring tilts the surface
RIPPLE_ROUGHNESS = 0.12   # a ring's water is smoother
RIPPLE_LIGHT = 0.35       # and a little lighter
PUDDLE_EDGE = 8.0         # how sharp a puddle's edge is


def _node(mat, cls, x, y, **props):
    n = lib.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def _scalar(mat, name, x, y):
    return _node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=0.0)


def add_wet(mat, base):
    """base: the expression that gives the dry colour. Connects base colour and roughness anew."""
    x = 300
    dark, sheen, pud = _scalar(mat, "WetDark", x, 600), _scalar(mat, "WetSheen", x, 700), _scalar(mat, "Puddles", x, 800)
    # the ground: what faces up
    nrm = _node(mat, unreal.MaterialExpressionVertexNormalWS, x, 900)
    up = _node(mat, unreal.MaterialExpressionComponentMask, x + 150, 900, r=False, g=False, b=True, a=False)
    lib.connect_material_expressions(nrm, "", up, "")
    up7 = _node(mat, unreal.MaterialExpressionSubtract, x + 300, 900, const_b=0.7)
    lib.connect_material_expressions(up, "", up7, "A")
    up4 = _node(mat, unreal.MaterialExpressionMultiply, x + 450, 900, const_b=4.0)
    lib.connect_material_expressions(up7, "", up4, "A")
    ground = _node(mat, unreal.MaterialExpressionSaturate, x + 600, 900)
    lib.connect_material_expressions(up4, "", ground, "")
    # puddles: where a soft noise is high, on the ground only, as many as Puddles says
    pos = _node(mat, unreal.MaterialExpressionWorldPosition, x, 1050)
    noise = _node(mat, unreal.MaterialExpressionNoise, x + 150, 1050, scale=PUDDLE_SCALE, levels=3, output_min=0.0, output_max=1.0)
    lib.connect_material_expressions(pos, "", noise, "Position")
    n5 = _node(mat, unreal.MaterialExpressionSubtract, x + 300, 1050, const_b=PUDDLE_LEVEL)
    lib.connect_material_expressions(noise, "", n5, "A")
    edge = _node(mat, unreal.MaterialExpressionMultiply, x + 450, 1050, const_b=PUDDLE_EDGE)
    lib.connect_material_expressions(n5, "", edge, "A")
    blot = _node(mat, unreal.MaterialExpressionSaturate, x + 600, 1050)
    lib.connect_material_expressions(edge, "", blot, "")
    on_ground = _node(mat, unreal.MaterialExpressionMultiply, x + 750, 1000)
    lib.connect_material_expressions(blot, "", on_ground, "A")
    lib.connect_material_expressions(ground, "", on_ground, "B")
    puddle = _node(mat, unreal.MaterialExpressionMultiply, x + 900, 1000)
    lib.connect_material_expressions(on_ground, "", puddle, "A")
    lib.connect_material_expressions(pud, "", puddle, "B")
    # darker: the ground by WetDark, other faces by a third of it, puddles by more
    wall = _node(mat, unreal.MaterialExpressionLinearInterpolate, x + 750, 650, const_a=0.33, const_b=1.0)
    lib.connect_material_expressions(ground, "", wall, "Alpha")
    darkness = _node(mat, unreal.MaterialExpressionMultiply, x + 900, 620)
    lib.connect_material_expressions(dark, "", darkness, "A")
    lib.connect_material_expressions(wall, "", darkness, "B")
    pdark = _node(mat, unreal.MaterialExpressionMultiply, x + 900, 760, const_b=0.5)
    lib.connect_material_expressions(puddle, "", pdark, "A")
    total = _node(mat, unreal.MaterialExpressionAdd, x + 1050, 680)
    lib.connect_material_expressions(darkness, "", total, "A")
    lib.connect_material_expressions(pdark, "", total, "B")
    keep = _node(mat, unreal.MaterialExpressionOneMinus, x + 1200, 680)
    lib.connect_material_expressions(total, "", keep, "")
    colour = _node(mat, unreal.MaterialExpressionMultiply, x + 1350, 500)
    lib.connect_material_expressions(base, "", colour, "A")
    lib.connect_material_expressions(keep, "", colour, "B")
    # roughness: matte when dry, a sheen on wet ground, standing water in the puddles
    shine = _node(mat, unreal.MaterialExpressionMultiply, x + 750, 1200)
    lib.connect_material_expressions(sheen, "", shine, "A")
    lib.connect_material_expressions(ground, "", shine, "B")
    sheen1 = _node(mat, unreal.MaterialExpressionSaturate, x + 900, 1200)
    lib.connect_material_expressions(shine, "", sheen1, "")
    wet_rough = _node(mat, unreal.MaterialExpressionLinearInterpolate, x + 1050, 1200, const_a=DRY_ROUGHNESS, const_b=SHEEN_ROUGHNESS)
    lib.connect_material_expressions(sheen1, "", wet_rough, "Alpha")
    water = _node(mat, unreal.MaterialExpressionLinearInterpolate, x + 1200, 1150, const_b=PUDDLE_ROUGHNESS)
    lib.connect_material_expressions(wet_rough, "", water, "A")
    lib.connect_material_expressions(puddle, "", water, "Alpha")
    # rings where drops land: the surface tilts with them (a world-space normal) and they catch a little light
    rip_k = _scalar(mat, "Ripples", x, 1400)
    tm = _node(mat, unreal.MaterialExpressionTime, x, 1500)
    pos2 = _node(mat, unreal.MaterialExpressionWorldPosition, x, 1600)
    rip = _node(mat, unreal.MaterialExpressionCustom, x + 300, 1450,
                code=open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "greybox_ripples.hlsl"), encoding="utf-8").read(),
                output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Brix ripples")
    ins = []
    for n in ("Pos", "T", "K"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    rip.set_editor_property("inputs", ins)
    lib.connect_material_expressions(pos2, "", rip, "Pos")
    lib.connect_material_expressions(tm, "", rip, "T")
    lib.connect_material_expressions(rip_k, "", rip, "K")
    ring = _node(mat, unreal.MaterialExpressionComponentMask, x + 500, 1500, r=False, g=False, b=True, a=False)
    lib.connect_material_expressions(rip, "", ring, "")
    ring_g = _node(mat, unreal.MaterialExpressionMultiply, x + 650, 1500)
    lib.connect_material_expressions(ring, "", ring_g, "A")
    lib.connect_material_expressions(ground, "", ring_g, "B")
    tilt = _node(mat, unreal.MaterialExpressionComponentMask, x + 500, 1600, r=True, g=True, b=False, a=False)
    lib.connect_material_expressions(rip, "", tilt, "")
    tilt_g = _node(mat, unreal.MaterialExpressionMultiply, x + 650, 1600, const_b=RIPPLE_TILT)
    lib.connect_material_expressions(tilt, "", tilt_g, "A")
    tilt_gg = _node(mat, unreal.MaterialExpressionMultiply, x + 800, 1600)
    lib.connect_material_expressions(tilt_g, "", tilt_gg, "A")
    lib.connect_material_expressions(ground, "", tilt_gg, "B")
    zero = _node(mat, unreal.MaterialExpressionConstant, x + 800, 1700, r=0.0)
    tilt3 = _node(mat, unreal.MaterialExpressionAppendVector, x + 950, 1600)
    lib.connect_material_expressions(tilt_gg, "", tilt3, "A")
    lib.connect_material_expressions(zero, "", tilt3, "B")
    nsum = _node(mat, unreal.MaterialExpressionAdd, x + 1100, 1550)
    lib.connect_material_expressions(nrm, "", nsum, "A")
    lib.connect_material_expressions(tilt3, "", nsum, "B")
    nn = _node(mat, unreal.MaterialExpressionNormalize, x + 1250, 1550)
    lib.connect_material_expressions(nsum, "", nn, "")
    mat.set_editor_property("tangent_space_normal", False)
    lib.connect_material_property(nn, "", unreal.MaterialProperty.MP_NORMAL)
    shine = _node(mat, unreal.MaterialExpressionLinearInterpolate, x + 1350, 1300, const_b=RIPPLE_ROUGHNESS)
    lib.connect_material_expressions(water, "", shine, "A")
    lib.connect_material_expressions(ring_g, "", shine, "Alpha")
    lib.connect_material_property(shine, "", unreal.MaterialProperty.MP_ROUGHNESS)
    lift = _node(mat, unreal.MaterialExpressionMultiply, x + 1350, 1450, const_b=RIPPLE_LIGHT)
    lib.connect_material_expressions(ring_g, "", lift, "A")
    lift1 = _node(mat, unreal.MaterialExpressionAdd, x + 1500, 1450, const_b=1.0)
    lib.connect_material_expressions(lift, "", lift1, "A")
    lit_colour = _node(mat, unreal.MaterialExpressionMultiply, x + 1650, 500)
    lib.connect_material_expressions(colour, "", lit_colour, "A")
    lib.connect_material_expressions(lift1, "", lit_colour, "B")
    lib.connect_material_property(lit_colour, "", unreal.MaterialProperty.MP_BASE_COLOR)


if __name__ == "__main__":
    mat = unreal.EditorAssetLibrary.load_asset(FULL)
    names = [str(n) for n in lib.get_scalar_parameter_names(mat)]
    if "WetDark" in names:
        unreal.log_warning("Brix Town: %s already has the wet paving" % FULL)
    else:
        base = lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
        add_wet(mat, base)
        lib.recompile_material(mat)
        unreal.EditorAssetLibrary.save_asset(FULL, only_if_is_dirty=False)
        unreal.log_warning("Brix Town: added the wet paving to %s" % FULL)
