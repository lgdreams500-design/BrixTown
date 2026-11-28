r"""Makes the materials of Eastgate Mall's inside (WLD-058 to WLD-068; court.js, shell.js, outlets.js), whose faces the game
makes at load time (ABrixMall), each carrying its colour in its vertices:
  /Game/Mall/M_MallLit       the slice's lit faces (material 1: light panels, lines of light, lit screens and cases, the
                             tall windows; material 2: lantern glass): the vertex colour glowing, times the parameter Glow
                             (ABrixMall sets one for the panels and one for the lanterns, from DefaultGame.ini)
  /Game/Mall/M_MallRoom      the indoor faces (the slice's room materials 3, 15, 16, 19): the vertex colour, matte, with the
                             court's even room light added (play.js roomLight() in a court mall: "an even light far from
                             the eye"), as the faces made at load time have no share of Lumen's bounced light: the vertex
                             colour times the parameter Glow (ABrixMall's RoomLight); the floor tiles (UV1.y 1) in the slice's
                             two tones with dark joints; mottled (W1: two close tones blotched, the parameter
                             Mottle), and that even light less on a face that looks down (the parameter Under: the
                             underside of a gallery is darker than its top; the look gate's G1)
  /Game/Mall/M_MallLattice   the pierced shell of the eggs and the roof light (materials 25 and 27): the vertex colour, matte,
                             seen from both sides, with the slice's diamond openings cut through it (UV0: round and down the
                             egg; UV1.x the openings round it, UV1.y 1 seen from inside), none at the crown from outside, a
                             round eye open to the sky at the crown from inside, in a plain ring (play.js materials 25, 27)
Matte (ART-002). The assets are binary and kept in Git LFS (ADR-0002); this script is how they are made.

  "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\Game\BrixTown.uproject"
      -run=pythonscript -script="%CD%\Tools\unreal\make_mall_materials.py" -unattended -nullrhi -nosplash
"""
import unreal

PATH = "/Game/Mall"
ROUGHNESS = 0.85
lib = unreal.MaterialEditingLibrary


def fresh(name):
    full = f"{PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):   # made anew (editing expressions headless crashes 5.8)
        unreal.EditorAssetLibrary.delete_asset(full)
    return full, unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())


def node(mat, cls, x, y, **props):
    n = lib.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


# ---- the lit faces
full, mat = fresh("M_MallLit")
mat.set_editor_property("two_sided", True)   # (the slice's faces are lit from the side their hint is on, either way round)
vc = node(mat, unreal.MaterialExpressionVertexColor, -700, 0)
glow = node(mat, unreal.MaterialExpressionScalarParameter, -700, 200, parameter_name="Glow", default_value=1.0)
lit = node(mat, unreal.MaterialExpressionMultiply, -400, 100)
lib.connect_material_expressions(vc, "", lit, "A")
lib.connect_material_expressions(glow, "", lit, "B")
lib.connect_material_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
base = node(mat, unreal.MaterialExpressionMultiply, -400, -100, const_b=0.2)
lib.connect_material_expressions(vc, "", base, "A")
lib.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = node(mat, unreal.MaterialExpressionConstant, -400, 300, r=ROUGHNESS)
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log_warning("Brix Town: made " + full)

# ---- the indoor faces
TILES = r"""
float3 c = C;
if (Kind.y > 0.5 && Kind.y < 1.5)   // play.js material 16: tiles 0.42 m in two tones, each a little its own, with dark joints
{
    float2 q = W.xy / 42.0, fr = frac(q), cell = floor(q);
    float ck = fmod(abs(cell.x + cell.y), 2.0), t1 = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
    c = lerp(c, c * float3(0.8, 0.8, 0.84), ck) * (0.95 + 0.1 * t1);
    c = lerp(c, c * 0.62, max(smoothstep(0.955, 0.985, fr.x), smoothstep(0.955, 0.985, fr.y)));
}
// A court mall's floor (Kind.y 2): polished stone with hidden joints (Art/Reference/Malls/MallsEastgateCourt, ADR-0069),
// the floor's own colour toned by the parameter Stone, its clouds some metres across.
if (Kind.y > 1.5) c *= Stone;
// W1: matte and mottled, two close tones of the one colour blotched together, never a flat fill. Blotches about
// 1.5 m across with smaller ones in them, the same on every face whichever way it looks.
float3 q3 = W / (Kind.y > 1.5 ? 420.0 : 150.0);
float2 m0 = q3.xy + q3.z * float2(0.37, 0.71);
float n = 0.0, amp = 0.62;
for (int o = 0; o < 2; ++o)
{
    float2 ci = floor(m0), cf = frac(m0);
    cf = cf * cf * (3.0 - 2.0 * cf);
    float h00 = frac(sin(dot(ci, float2(127.1, 311.7))) * 43758.5453);
    float h10 = frac(sin(dot(ci + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
    float h01 = frac(sin(dot(ci + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
    float h11 = frac(sin(dot(ci + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
    n += amp * lerp(lerp(h00, h10, cf.x), lerp(h01, h11, cf.x), cf.y);
    m0 = m0 * 2.7 + 5.3;
    amp = 0.38;
}
c *= lerp(1.0 - Mottle, 1.0 + Mottle * 0.5, n);
return c;
"""
# How much of the hall's own even light a face has by which way it looks (the look gate's G1): all of it looking up,
# Under of it looking down (the underside of a gallery, a ceiling), between the two on a wall, and a little more or
# less by which side a wall faces, so that two walls meeting at a corner are not one tone.
SHADE = r"""
float3 nn = normalize(N) * Sign;
float up = saturate(nn.z * 0.5 + 0.5);
float side = dot(nn.xy, float2(0.6, 0.8)) * 0.07;
return lerp(Under, 1.0, up * up * (3.0 - 2.0 * up)) + side;
"""
full, mat = fresh("M_MallRoom")
mat.set_editor_property("two_sided", True)
vc = node(mat, unreal.MaterialExpressionVertexColor, -900, 0)
wp = node(mat, unreal.MaterialExpressionWorldPosition, -900, 150)
uv1 = node(mat, unreal.MaterialExpressionTextureCoordinate, -900, 300, coordinate_index=1)
tc = node(mat, unreal.MaterialExpressionCustom, -650, 100, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Brix tiles", code=TILES)
ins = []
for n in ("C", "W", "Kind", "Mottle", "Stone"):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", n)
    ins.append(ci)
tc.set_editor_property("inputs", ins)
stone = node(mat, unreal.MaterialExpressionVectorParameter, -900, -150, parameter_name="Stone", default_value=unreal.LinearColor(0.6, 0.575, 0.54, 1.0))
lib.connect_material_expressions(stone, "", tc, "Stone")
mottle = node(mat, unreal.MaterialExpressionScalarParameter, -900, 450, parameter_name="Mottle", default_value=0.12)
lib.connect_material_expressions(vc, "", tc, "C")
lib.connect_material_expressions(wp, "", tc, "W")
lib.connect_material_expressions(uv1, "", tc, "Kind")
lib.connect_material_expressions(mottle, "", tc, "Mottle")
lib.connect_material_property(tc, "", unreal.MaterialProperty.MP_BASE_COLOR)
glow = node(mat, unreal.MaterialExpressionScalarParameter, -700, 300, parameter_name="Glow", default_value=0.3)
under = node(mat, unreal.MaterialExpressionScalarParameter, -900, 600, parameter_name="Under", default_value=0.45)
vn = node(mat, unreal.MaterialExpressionVertexNormalWS, -900, 750)
sign = node(mat, unreal.MaterialExpressionTwoSidedSign, -900, 900)
sh = node(mat, unreal.MaterialExpressionCustom, -650, 650, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Brix hall shade", code=SHADE)
sins = []
for n in ("N", "Sign", "Under"):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", n)
    sins.append(ci)
sh.set_editor_property("inputs", sins)
lib.connect_material_expressions(vn, "", sh, "N")
lib.connect_material_expressions(sign, "", sh, "Sign")
lib.connect_material_expressions(under, "", sh, "Under")
amb = node(mat, unreal.MaterialExpressionMultiply, -500, 400)
lib.connect_material_expressions(glow, "", amb, "A")
lib.connect_material_expressions(sh, "", amb, "B")
lit = node(mat, unreal.MaterialExpressionMultiply, -400, 200)
lib.connect_material_expressions(tc, "", lit, "A")
lib.connect_material_expressions(amb, "", lit, "B")
lib.connect_material_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
# matte, but for the polished stone floor (the parameter StoneRough)
srough = node(mat, unreal.MaterialExpressionScalarParameter, -900, 1050, parameter_name="StoneRough", default_value=0.3)
rough = node(mat, unreal.MaterialExpressionCustom, -400, 1000, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Brix floor polish",
             code="return Kind.y > 1.5 ? R : %s;" % ROUGHNESS)
rins = []
for n in ("Kind", "R"):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", n)
    rins.append(ci)
rough.set_editor_property("inputs", rins)
lib.connect_material_expressions(uv1, "", rough, "Kind")
lib.connect_material_expressions(srough, "", rough, "R")
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log_warning("Brix Town: made " + full)

# ---- the pierced shell
LATTICE = r"""
float N = max(1.0, Cells.x);
float2 g = float2(UV.x + UV.y, UV.x - UV.y) * N;
float2 fr = abs(frac(g) - 0.5);
float hole = 1.0 - smoothstep(0.27, 0.33, max(fr.x, fr.y));
if (Cells.y > 0.5)
{
    float ring = smoothstep(0.016, 0.019, UV.y) * (1.0 - smoothstep(0.028, 0.033, UV.y));
    hole = (UV.y < 0.024 ? 1.0 : hole) * (1.0 - ring);
}
else hole *= smoothstep(0.05, 0.1, UV.y);
return 1.0 - hole;
"""
full, mat = fresh("M_MallLattice")
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
mat.set_editor_property("two_sided", True)
mat.set_editor_property("opacity_mask_clip_value", 0.5)
vc = node(mat, unreal.MaterialExpressionVertexColor, -700, -200)
lib.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
uv0 = node(mat, unreal.MaterialExpressionTextureCoordinate, -900, 100, coordinate_index=0)
uv1 = node(mat, unreal.MaterialExpressionTextureCoordinate, -900, 250, coordinate_index=1)
c = node(mat, unreal.MaterialExpressionCustom, -500, 150, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Brix lattice", code=LATTICE)
ins = []
for n in ("UV", "Cells"):
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", n)
    ins.append(ci)
c.set_editor_property("inputs", ins)
lib.connect_material_expressions(uv0, "", c, "UV")
lib.connect_material_expressions(uv1, "", c, "Cells")
lib.connect_material_property(c, "", unreal.MaterialProperty.MP_OPACITY_MASK)
rough = node(mat, unreal.MaterialExpressionConstant, -400, 400, r=ROUGHNESS)
lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
unreal.log_warning("Brix Town: made " + full)
