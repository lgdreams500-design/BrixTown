"""Expand the furniture outlet's showroom for kit 18 (FURNITURE FURN-020, FURN-021; F-FURN-STORE level 2; owner,
2026-11-23: "expand furniture showroom to accommodate showcases for the more furniture" of furniture_kit_18.py).

The outlet was made by add_outlet_01.py and moved outside the walls by the city remodel (it is now wherever the one lot
of kind "furniture" is). This edits that lot and its block in place, and nothing else:
- the hall is 6 m deeper (66 m x 46 m), and the car park south of it moves down to make room;
- the showroom floor becomes an island of sixteen room sets in two rows back to back: kit 17's living room with a bed
  corner in each of the four lines (as before), and one room for each of kit 18's twelve kits, furnished as the look
  test places them (ROOMS in Tools/blender/furniture_kit_18.py, read from its text), each in the line it was rendered in;
- a painted walking route loops round the island; the cafe moves beside the warehouse; sales desks stand by the walks;
- every place's kind of fitting (till, desk, cafe counter, packing table) is data, so the hall draws what stands there.

Run from Tools/worldgen:   python expand_outlet_01.py      (running it again changes nothing)
Then:                      python -m unittest test_world ;  python build_play.py
Every size and count here is a first guess [ASSUMPTION]. Coordinates below are metres from the hall's north-west corner.
"""
import json, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
KIT18 = os.path.join(HERE, "..", "blender", "furniture_kit_18.py")

WIDTH, DEPTH = 66.0, 46.0                  # the hall (it was 66 x 40)
CARPARK_GAP, CARPARK_DEPTH = 4.0, 20.0     # the car park: this far south of the hall, this deep (it was 22)

# The island: set centres across the hall, each set 6.4 m x 4.4 m, 1 m between neighbouring walls
SET_U = [6.0 + 7.7 * k for k in range(8)]
ROW_N = (10.2, 14.6)                       # the north row, open north onto the first walk
ROW_S = (14.9, 19.3)                       # the south row, open south onto the second walk; its back wall against the north row's
NORTH_ROW = [("k17", "plain"), ("k17", "oldtown"), ("k17", "workshop"), ("k17", "curve"),
             ("living", None), ("bedroom", None), ("children", None), ("study", None)]
SOUTH_ROW = [("kitchen", None), ("dining", None), ("bathroom", None), ("hallway", None),
             ("decor", None), ("garden", None), ("cafe", None), ("shop", None)]
ROOM_NAMES = {"living": "Living room", "bedroom": "Bedroom", "children": "Children's room", "study": "Study", "kitchen": "Kitchen",
              "dining": "Dining room", "bathroom": "Bathroom", "hallway": "Hallway", "decor": "Decoration", "garden": "Garden",
              "cafe": "Cafe fittings", "shop": "Shop fittings"}
# the route painted on the floor, a loop round the island: [u0, v0, u1, v1, the way its arrows point]
ROUTE = [[1.0, 5.2, 65.0, 6.6, "w"],       # the first walk, along the north row
         [1.0, 5.2, 2.4, 22.4, "s"],       # down the west end
         [1.0, 21.0, 65.0, 22.4, "e"],     # the second walk, along the south row
         [63.6, 5.2, 65.0, 22.4, "n"]]     # up the east end, and round again
RACK_ROWS = [31.5, 37.5, 43.0]             # the warehouse: three rows of racks across the south of the hall (DEPTH - 14.5, - 8.5, - 3)

FITTINGS = dict(tills=[[26.0, 3.5], [40.0, 3.5]],                                 # either side of the door
                desks=[[14.0, 4.8], [52.0, 4.8], [20.0, 23.9], [40.0, 23.9]],     # by the first walk and by the second
                cafe=[[60.5, 27.6]],                                              # the cafe counter
                packing=[[16.0, 35.6], [33.0, 35.6], [50.0, 35.6]])               # in the warehouse aisle


def kit18_rooms():
    """The ROOMS table of the kit 18 look test, read from the script's own text (the script needs Blender to import)."""
    with open(KIT18, encoding="utf-8") as f:
        src = f.read()
    a = src.index("\nROOMS = {") + len("\nROOMS = ")
    b = src.index("\n}\n", a) + 2
    return eval(src[a:b], {"__builtins__": {}, "math": math})


def r2(v):
    return round(v, 3)


def layout(x0, z0):
    P = lambda u, v: [r2(x0 + u), r2(z0 + v)]
    rooms, sets = kit18_rooms(), []
    for row, (va, vb), face in ((NORTH_ROW, ROW_N, "n"), (SOUTH_ROW, ROW_S, "s")):
        for u, (room, line) in zip(SET_U, row):
            s = dict(rect=[r2(x0 + u - 3.2), r2(z0 + va), r2(x0 + u + 3.2), r2(z0 + vb)], open=face)
            if room == "k17":
                s["line"] = line
            else:
                line, pieces = rooms[room]
                s.update(line=line, room=room, name=ROOM_NAMES[room],
                         pieces=[[k, r2(px), r2(py), round(yaw, 4)] for k, px, py, yaw in pieces])
            sets.append(s)
    fit = {k: [P(*q) for q in v] for k, v in FITTINGS.items()}
    work = fit["tills"] + fit["desks"] + fit["cafe"] + fit["packing"]
    customer = []
    for u in SET_U:                                                   # in front of each room set, and just inside it
        customer += [P(u - 1.6, 9.6), P(u + 1.6, 9.6), P(u - 0.4, 10.5), P(u + 1.0, 10.5)]
        customer += [P(u - 1.6, 19.9), P(u + 1.6, 19.9), P(u + 0.4, 19.0), P(u - 1.0, 19.0)]
    customer += [P(u, 29.8) for u in (10.0, 22.0, 34.0)]              # along the racks
    customer += [P(u, 36.0) for u in (10.0, 26.0, 40.0, 56.0)]        # and in the aisle
    seat = [P(u, v) for u in (50.4, 52.0, 54.8, 56.4) for v in (25.6, 28.4)]   # the cafe: pairs across small tables
    route = [[r2(x0 + a), r2(z0 + b), r2(x0 + c), r2(z0 + d), w] for a, b, c, d, w in ROUTE]
    return dict(sets=sets, fittings=fit, objects=dict(work=work, customer=customer, seat=seat), route=route,
                rack_rows=[r2(z0 + v) for v in RACK_ROWS])


def main():
    city = json.load(open(os.path.join(CITY, "city.json"), encoding="utf-8"))
    for e in city["blocks"]:
        path = os.path.join(CITY, e["file"])
        b = json.load(open(path, encoding="utf-8"))
        lots = [l for l in b["lots"] if l["kind"] == "furniture"]
        if not lots:
            continue
        l = lots[0]
        x0, z0 = l["rect"][0], l["rect"][1]
        new = layout(x0, z0)
        rect = [x0, z0, r2(x0 + WIDTH), r2(z0 + DEPTH)]
        if l["staff"] != len(new["objects"]["work"]):
            raise SystemExit("the outlet has %d staff but %d places to work" % (l["staff"], len(new["objects"]["work"])))
        bz1 = b["origin"][1] + b["size"]
        cz = r2(rect[3] + CARPARK_GAP + CARPARK_DEPTH / 2)
        if cz + CARPARK_DEPTH / 2 > bz1:
            raise SystemExit("the car park would leave block %s" % b["id"])
        for o in b["lots"]:
            if o is not l and o["rect"][1] < cz + CARPARK_DEPTH / 2 and o["rect"][3] > rect[1] and o["rect"][0] < rect[2] and o["rect"][2] > x0:
                raise SystemExit("the hall would run into %s" % o["id"])
        l["rect"] = rect
        l["objects"] = new["objects"]
        l.pop("racks", None)
        for k in ("sets", "fittings", "route", "rack_rows"):
            l[k] = new[k]
        for p in b["props"]:
            if p["kind"] == "carpark":
                p["pos"] = [p["pos"][0], cz]
                p["size"] = [p["size"][0], CARPARK_DEPTH]
        with open(path, "w", encoding="utf-8") as f:
            json.dump(b, f, ensure_ascii=False, indent=1)
        print("%s: %s %gx%g m, %d room sets" % (e["file"], l["name"], WIDTH, DEPTH, len(l["sets"])))
        return
    raise SystemExit("no furniture outlet in city_01")


if __name__ == "__main__":
    main()
