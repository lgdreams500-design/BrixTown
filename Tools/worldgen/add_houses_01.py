"""The independent houses of the communities, inside and out (the owner's requests of 2026-11-22: "a central stair case which
after half way branches out on both sides", "I want the exteriors of this house to be designed", "Colours and porch", and with
469 houses "we need more variety": mirrored plans, roof shapes, walls and trim, extras on the house).

Every house is 13 m along its lane by 10 m deep, two storeys, its front door in the middle of the side facing the lane. In the
house's own frame (u across the front, v from the front at -5 to the back at +5, metres from its centre, before it is turned by
lot.angle), the plan "fork_stair_01":

  ground (floor 0, 0.2 m)                          first (floor 1, 3.4 m)
  living room  u -6.5..-1.5  v -5..1.2             main bedroom  u -6.5..-1.5  v -5..1.2
  study        u -6.5..-1.5  v 2.3..5              bathroom      u -6.5..-1.5  v 2.3..5
  hall         u -1.5..1.5   v -5..1.2 (two storeys high, under the arched window)
  kitchen      u 1.5..6.5    v -5..1.2             bedroom 2     u 1.5..6.5    v -5..1.2
  utility      u 1.5..6.5    v 2.3..5, back door   bedroom 3     u 1.5..6.5    v 2.3..5
  pantry       u -1.5..1.5   v 2.6..5
  the stair: one flight up the middle of the hall (u -0.75..0.75, v -2.3..1.2) to a half landing (u -1.5..1.5, v 1.2..2.6,
  1.8 m), then a flight to each side along v 1.2..2.3 (from |u| 1.5 out to 5) up to a small landing at each side wall
  (|u| 5..6.5), where a door leads to the room in front and the room behind. Below the side flights a passage by each side
  wall joins the front and back rooms on the ground floor.
A mirrored house has u turned round (kitchen on the left).

Written into each house's lot:
  house    {plan, mirror, porch, roof, walls, trim, extras, rooms, doors}; rooms and doors in the house's own frame as above
           (rooms [{name, floor, rect [u0, v0, u1, v1]}], doors [{floor, at [u, v], wall "u"|"v"}]), extras with their rects
  objects  bed (on the first floor) and room (sitting places, on the ground floor), as many as the house's beds were, in world
           metres; levels say which floor. The k-th resident still sleeps in the k-th bed.
  storeys  ["main", "bedrooms"]
Run from Tools/worldgen:   python add_houses_01.py      (running it again writes the same)
Every size and share here is a first guess [ASSUMPTION].
"""
import json, math, os, hashlib

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def jsave(p, o):
    with open(p, encoding="utf-8") as f:
        end = "\n" if f.read().endswith("\n") else ""
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        json.dump(o, f, indent=1, ensure_ascii=False)
        f.write(end)


def h(*k):
    """The same inputs always give the same number in 0..1."""
    return int(hashlib.md5("|".join(map(str, k)).encode()).hexdigest()[:8], 16) / 0xFFFFFFFF


def pick(options, r):
    """options: [(value, weight)]"""
    tot = sum(w for _, w in options)
    acc = 0.0
    for v, w in options:
        acc += w / tot
        if r < acc:
            return v
    return options[-1][0]


ROOMS = [  # name, floor, [u0, v0, u1, v1] for the plan as drawn (kitchen on the right)
    ("Living room", 0, [-6.5, -5, -1.5, 1.2]), ("Study", 0, [-6.5, 2.3, -1.5, 5]), ("Hall", 0, [-1.5, -5, 1.5, 1.2]),
    ("Kitchen", 0, [1.5, -5, 6.5, 1.2]), ("Utility", 0, [1.5, 2.3, 6.5, 5]), ("Pantry", 0, [-1.5, 2.6, 1.5, 5]),
    ("Main bedroom", 1, [-6.5, -5, -1.5, 1.2]), ("Bathroom", 1, [-6.5, 2.3, -1.5, 5]),
    ("Bedroom 2", 1, [1.5, -5, 6.5, 1.2]), ("Bedroom 3", 1, [1.5, 2.3, 6.5, 5]),
]
DOORS = [  # floor, [u, v], the wall it is in ("u": a wall of constant u, "v": of constant v)
    (0, [0, -5], "v"), (0, [4.5, 5], "v"),                             # front and back doors
    (0, [-1.5, -3.6], "u"), (0, [1.5, -3.6], "u"),                     # hall to living room and kitchen
    (0, [-5.75, 1.2], "v"), (0, [-5.75, 2.3], "v"),                    # the passages by the side walls, under the side landings
    (0, [5.75, 1.2], "v"), (0, [5.75, 2.3], "v"),
    (0, [1.5, 3.8], "u"),                                              # utility to pantry
    (1, [-5.75, 1.2], "v"), (1, [-5.75, 2.3], "v"), (1, [5.75, 1.2], "v"), (1, [5.75, 2.3], "v"),   # side landings to rooms
]
BEDS = [(-5.3, -2.4, "Main bedroom"), (-5.3, -1.4, "Main bedroom"), (5.3, -3.6, "Bedroom 2"), (5.3, 3.65, "Bedroom 3"), (5.3, -0.9, "Bedroom 2")]
SEATS = [(-4.6, -3.4), (-3.6, -3.4), (-5.6, -1.4), (-2.6, -1.4), (-4.1, 0.2)]   # the living room's sofa and armchairs
PALETTE_PLASTER = 5          # how many plaster tones the slice's PLASTER list has
SHUTTERS = ["teal", "blue", "olive", "terracotta", "ochre", "plum"]


def variant(lid):
    mirror = h(lid, "mirror") < .5
    roof = pick([("hip", 4), ("gable", 4), ("mansard", 2)], h(lid, "roof"))
    walls = pick([("plaster", 6), ("stone", 2), ("brick", 2)], h(lid, "walls"))
    porch = pick([("columns", 4), ("canopy", 3), ("arch", 3)], h(lid, "porch"))
    trim = dict(shutters=h(lid, "sh") < .65, shutter_color=SHUTTERS[int(h(lid, "shc") * len(SHUTTERS))], window_boxes=h(lid, "wb") < .5,
                keystones=h(lid, "ks") < .4, plaster=int(h(lid, "pl") * PALETTE_PLASTER), roof_tone=int(h(lid, "rt") * 5))
    return mirror, roof, walls, porch, trim


def main():
    city = jload(os.path.join(CITY, "city.json"))
    tally, n_houses = {}, 0
    for e in city["blocks"]:
        if e["template"] != "community":
            continue
        p = os.path.join(CITY, e["file"])
        b = jload(p)
        houses = [l for l in b["lots"] if l["kind"] == "home" and l.get("angle") is not None]
        others = [l for l in b["lots"] if l not in houses]
        lane_pts = [q for ln in b.get("lanes", []) for q in ln["points"]]

        def frame(l):
            x0, z0, x1, z1 = l["rect"]
            cx, cz, t = (x0 + x1) / 2, (z0 + z1) / 2, math.radians(l.get("angle") or 0)
            c, s = math.cos(t), math.sin(t)
            return (lambda u, v: [round(cx + u * c - v * s, 2), round(cz + u * s + v * c, 2)]), cx, cz
        centres = {l["id"]: frame(l)[1:] for l in b["lots"]}
        taken = []                                                      # garages and conservatories already placed (world circles)

        def clear(l, u0, v0, u1, v1, mirror):
            W, _, _ = frame(l)
            pts = [W(u * (-1 if mirror else 1), v) for u in (u0, (u0 + u1) / 2, u1) for v in (v0, (v0 + v1) / 2, v1)]
            for q in pts:
                for o in b["lots"]:
                    if o is l:
                        continue
                    ox, oz = centres[o["id"]]
                    r = math.hypot(o["rect"][2] - o["rect"][0], o["rect"][3] - o["rect"][1]) / 2 + 1.0
                    if math.hypot(q[0] - ox, q[1] - oz) < r:
                        return False
                if any(math.hypot(q[0] - a[0], q[1] - a[1]) < 5.0 for a in lane_pts):
                    return False
                if any(math.hypot(q[0] - a[0], q[1] - a[1]) < a[2] for a in taken):
                    return False
            return True

        for l in houses:
            lid = l["id"]
            mirror, roof, walls, porch, trim = variant(lid)
            sgn = -1 if mirror else 1
            W, cx, cz = frame(l)
            M = lambda r: [min(r[0] * sgn, r[2] * sgn), r[1], max(r[0] * sgn, r[2] * sgn), r[3]]
            extras = []
            if h(lid, "bay") < .35:
                extras.append(dict(kind="bay", rect=M([-5.5, -6.0, -2.5, -5])))                     # a bay window on the living room
            if porch != "arch" and h(lid, "balcony") < .3:
                extras.append(dict(kind="balcony", rect=[-1.5, -6.2, 1.5, -5]))                      # over the porch, from the hall's height
            if h(lid, "garage") < .3 and clear(l, 6.9, -5, 10.3, 1.0, mirror):
                extras.append(dict(kind="garage", rect=M([6.9, -5, 10.3, 1.0])))
                g = W(8.6 * sgn, -2); taken.append((g[0], g[1], 4.5))
            if h(lid, "cons") < .3 and clear(l, -4.5, 5.4, 0.5, 8.8, mirror):
                extras.append(dict(kind="conservatory", rect=M([-4.5, 5.4, 0.5, 8.8])))
                g = W(-2 * sgn, 7.1); taken.append((g[0], g[1], 4.0))
            rooms = [dict(name=n, floor=f, rect=[round(x, 2) for x in M(r)]) for n, f, r in ROOMS]
            doors = [dict(floor=f, at=[u * sgn, v], wall=w) for f, (u, v), w in DOORS]
            l["house"] = dict(plan="fork_stair_01", mirror=mirror, porch=porch, roof=roof, walls=walls, trim=trim, extras=extras, rooms=rooms, doors=doors)
            n = len(l["objects"].get("bed", [])) or l.get("residents", 2)
            beds = [W(u * sgn, v) for u, v, _ in BEDS[:n]]
            seats = [W(u * sgn, v) for u, v in SEATS[:n]]
            l["objects"] = dict(bed=beds, room=seats)
            l["levels"] = dict(bed=[1] * n, room=[0] * n)
            l["storeys"] = ["main", "bedrooms"]
            l["floors"] = 2
            n_houses += 1
            for k in [roof, walls, porch, "mirrored" if mirror else "as drawn"] + [x["kind"] for x in extras]:
                tally[k] = tally.get(k, 0) + 1
        jsave(p, b)
    print("%d houses" % n_houses)
    for k, v in sorted(tally.items(), key=lambda kv: -kv[1]):
        print("  %-14s %4d" % (k, v))


if __name__ == "__main__":
    main()
