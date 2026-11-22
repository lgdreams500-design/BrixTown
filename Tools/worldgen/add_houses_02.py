"""Every independent house built to the approved Blender design (Tools/blender/house_01.py; the owner, 2026-11-22: "House looks
good", "just build one house and show me first", then after trying it in the slice "go ahead and update"). Writes plan
"house_01" into every house of the communities (replacing add_houses_01.py's "fork_stair_01"). All are the same house, as
drawn; they differ only in colour, by a hash of the lot: plaster tone, roof tone, shutter colour, window boxes or not.

The house's own frame, as houses.js uses it: u across the front (-6.5 left .. 6.5 right, seen from the lane), v from the front
(-5) to the back (+5), metres from its centre. Blender's x = u + 6.5, y = v + 5.
  ground: living room u -6.5..-2.1, hall -2.1..2.1 (open to its stair at the back, two storeys high), kitchen 2.1..6.5;
          all v -5..2.95. The stair: a flight up the hall to a half landing against the back wall, then a flight along the
          back wall to each side, to a landing in each back corner (|u| 4.6..6.5) with a door to the bedroom in front and a
          glazed door out to a balcony on the back. The back door is under the right landing, through the kitchen.
  first:  en-suite u -6.5..-2.1 v -5..-2.4, main bedroom u -6.5..-2.1 v -2.4..2.95, bedroom 2 u 2.1..6.5 v -5..2.95.
The furniture (kits 17 and 18, the Old Town line) is houses.js's, as placed in Blender; a fourth and fifth resident get single
beds in bedroom 2.
Run from Tools/worldgen:   python add_houses_02.py      (running it again writes the same)
"""
import json, math, os, hashlib

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
SHUTTERS = ["teal", "blue", "olive", "terracotta", "ochre", "plum"]


def h(*k):
    """The same inputs always give the same number in 0..1."""
    return int(hashlib.md5("|".join(map(str, k)).encode()).hexdigest()[:8], 16) / 0xFFFFFFFF

ROOMS = [("Living room", 0, [-6.5, -5, -2.1, 2.95]), ("Hall", 0, [-2.1, -5, 2.1, 2.95]), ("Kitchen", 0, [2.1, -5, 6.5, 2.95]),
         ("En-suite", 1, [-6.5, -5, -2.1, -2.4]), ("Main bedroom", 1, [-6.5, -2.4, -2.1, 2.95]), ("Bedroom 2", 1, [2.1, -5, 6.5, 2.95])]
DOORS = [(0, [-2.1, -3.4], "u"), (0, [2.1, -3.4], "u"), (0, [5.8, 2.95], "v"),          # hall to living room and kitchen; kitchen to the back door
         (1, [-5.4, 2.95], "v"), (1, [-3.9, -2.4], "v"), (1, [5.4, 2.95], "v")]          # corner landings to the bedrooms; the en-suite
BEDS = [(-3.3, -0.4), (-3.3, 0.4), (5.2, -1.0), (5.2, 1.0), (5.2, -3.0)]             # the double in the main bedroom, singles in bedroom 2
SEATS = [(-2.7, -1.5), (-2.7, -0.5), (-4.6, -2.85), (-4.6, 0.85), (-2.7, -1.0)]        # the sofa and two armchairs round the fireplace


def main():
    city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
    n_houses, tones = 0, {}
    for e in city["blocks"]:
        if e["template"] != "community":
            continue
        p = os.path.join(CITY, e["file"])
        b = json.load(open(p, encoding="utf-8"))
        houses = [l for l in b["lots"] if l["kind"] == "home" and l.get("angle") is not None]
        for l in houses:
            lid = l["id"]
            x0, z0, x1, z1 = l["rect"]
            cx, cz, t = (x0 + x1) / 2, (z0 + z1) / 2, math.radians(l.get("angle") or 0)
            c, s = math.cos(t), math.sin(t)
            W = lambda u, v: [round(cx - u * c - v * s, 2), round(cz - u * s + v * c, 2)]   # houses.js draws house_01 with u turned round (Blender's way)
            n = len(l["objects"].get("bed", [])) or l.get("residents", 2)
            trim = dict(shutters=True, shutter_color=SHUTTERS[int(h(lid, "shc") * len(SHUTTERS))], window_boxes=h(lid, "wb") < .6,
                        keystones=False, plaster=int(h(lid, "pl") * 5), roof_tone=int(h(lid, "rt") * 5))
            l["house"] = dict(plan="house_01", mirror=False, porch="columns", roof="gable", walls="plaster", furniture="oldtown", trim=trim,
                              extras=[dict(kind="balcony", rect=[-6.5, 5, -3.3, 6.3]), dict(kind="balcony", rect=[3.3, 5, 6.5, 6.3])],
                              rooms=[dict(name=nm, floor=f, rect=r) for nm, f, r in ROOMS],
                              doors=[dict(floor=f, at=a, wall=w) for f, a, w in DOORS])
            l["objects"] = dict(bed=[W(u, v) for u, v in BEDS[:n]], room=[W(u, v) for u, v in SEATS[:n]])
            l["levels"] = dict(bed=[1] * n, room=[0] * n)
            l["storeys"] = ["main", "bedrooms"]
            l["floors"] = 2
            n_houses += 1
            tones[trim["shutter_color"]] = tones.get(trim["shutter_color"], 0) + 1
        with open(p, encoding="utf-8") as f:
            end = "\n" if f.read().endswith("\n") else ""
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            json.dump(b, f, indent=1, ensure_ascii=False)
            f.write(end)
    print("%d houses on plan house_01; shutters: %s" % (n_houses, ", ".join("%s %d" % kv for kv in sorted(tones.items()))))


if __name__ == "__main__":
    main()
