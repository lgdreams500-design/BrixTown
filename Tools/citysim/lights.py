"""Lights and their kinds (F-LIGHTS level 2; docs/specs/domains/FURNITURE.md 2.5, FURN-066 to FURN-070; ADR-0065).

The rules proved here before the engine builds them: every kind of light checked (FURN-066), the pieces and the
Blender references' lamps tied to a kind (FURN-067), where the designer lets each kind stand (FURN-068, FURN-070).
Data: World/rules/lights.json."""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
RULES = os.path.join(HERE, "..", "..", "World", "rules")


def load_rules():
    with open(os.path.join(RULES, "lights.json"), encoding="utf-8") as f:
        return json.load(f)


def problems(L, pieces=None):
    """Everything wrong with the lights' data, as sentences; empty when it is sound."""
    out = []
    mounts, spreads = L["mounts"], set(L["spreads"])
    for k, v in L["kinds"].items():
        if v["mount"] not in mounts:
            out.append("%s: no mount %r" % (k, v["mount"]))
        if v["spread"] not in spreads:
            out.append("%s: no spread %r" % (k, v["spread"]))
        c = v["colour"]
        if len(c) != 3 or not all(0.0 <= x <= 1.0 for x in c) or max(c) != 1.0:
            out.append("%s: colour must be three values 0..1 with the brightest 1" % k)
        if not 1500 <= v["kelvin"] <= 6500:
            out.append("%s: kelvin %s outside 1500..6500" % (k, v["kelvin"]))
        if v["strength"] <= 0 or not 0.0 <= v["glow"] <= 1.0:
            out.append("%s: strength above 0 and glow 0..1" % k)
        if not isinstance(v["price"], int) or v["price"] <= 0:
            out.append("%s: price a whole number of minor units above 0" % k)
        if len(v["size"]) != 3 or min(v["size"]) <= 0:
            out.append("%s: size three lengths above 0" % k)
    # a warmer light (fewer kelvin) is never bluer than a cooler one
    ks = sorted(L["kinds"].items(), key=lambda kv: kv[1]["kelvin"])
    for (a, va), (b, vb) in zip(ks, ks[1:]):
        if va["kelvin"] < vb["kelvin"] and va["colour"][2] > vb["colour"][2]:
            out.append("%s (%dK) is bluer than %s (%dK)" % (a, va["kelvin"], b, vb["kelvin"]))
    for p, k in L["pieces"].items():
        if k not in L["kinds"]:
            out.append("piece %s: no kind %r" % (p, k))
        if pieces is not None and p not in pieces:
            out.append("piece %s is not in furnishing.json" % p)
    for n, k in L["blender_lights"].items():
        if k not in L["kinds"]:
            out.append("Blender light %s: no kind %r" % (n, k))
    return out


def kind_of_light(L, name):
    """The kind a Blender lamp stands for, by its name's start (FLamp_Light.003 is a floor lamp); None if not a lamp."""
    for n, k in L["blender_lights"].items():
        if name == n or name.startswith(n + "."):
            return L["kinds"][k]
    return None


def can_place(L, kind, surface, height=0.0, room_height=3.0, from_opening=None):
    """Whether the designer lets a light of this kind stand here (FURN-068, FURN-070). surface: floor, top (of a piece),
    wall or ceiling; height: of the light's middle above the floor (wall) or how far it hangs (ceiling); from_opening:
    the distance to the nearest window or door edge on that wall. Returns (ok, why)."""
    if kind not in L["kinds"]:
        return False, "no such kind"
    m = L["mounts"][L["kinds"][kind]["mount"]]
    if surface not in (m["on"], m.get("or")):
        return False, "a %s stands on %s" % (kind, " or ".join(x for x in (m["on"], m.get("or")) if x))
    if surface == "wall":
        lo, hi = m["height_m"]
        if not lo <= height <= hi:
            return False, "a wall light goes %.1f to %.1f m up" % (lo, hi)
        if from_opening is not None and from_opening < m["keep_from_openings_m"]:
            return False, "too near a window or door"
    if surface == "ceiling":
        lo, hi = m["drop_m"]
        if not lo <= height <= hi:
            return False, "a ceiling light hangs %.1f to %.1f m" % (lo, hi)
        if room_height - height - L["kinds"][kind]["size"][2] < m["keep_above_head_m"]:
            return False, "it would hang into a head"
    return True, ""
