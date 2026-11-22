"""Add the furniture outlet outside the East Gate to city_01 (FURNITURE FURN-020, FURN-021; F-FURN-STORE; Q-30 decided by
the owner on 2026-11-22: "Outside East Gate"; its look follows Art/StyleTests/furniture_kit_17/showroom.png).

What it does, all as data:
- a new block b_8_4 in the grid cell east of East Wall Street, beyond the town wall, on the road out of the East Gate.
  The town wall runs across the west edge of that cell, so nothing is built in its first 10 m; the outlet stands
  behind it with its door on the gate road and a car park to the south.
- the outlet itself: one tall hall (a lot of kind "furniture") with places for its staff (tills, sales desks, the cafe
  counter, the warehouse), for customers among the room sets and the racks, and seats in its cafe.
- ten residents who worked out of town (commuters) take jobs there as shop staff, so the hall is staffed by day.
- every resident learns their nearest furniture outlet, and "browse the furniture outlet" joins the shopping choices.
- the East Gate road is made long enough to pass the outlet (World/city_01/roads.json; edited, not regenerated).
- estate.json sells the outlet as one showroom unit; storeys.json names what an upper floor would be (it has none).

Run from Tools/worldgen:   python add_outlet_01.py      (running it again changes nothing)
Then:                      python -m unittest test_world ;  python build_play.py
Every size and count here is a first guess [ASSUMPTION].
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
CITY = os.path.join(HERE, "..", "..", "World", "city_01")
RULES = os.path.join(HERE, "..", "..", "World", "rules")

BLOCK = "b_8_4"
LOT = BLOCK + "_l0"
NAME = "Furniture Outlet"                  # a placeholder until the owner names the companies (Q-32)
RECT = [786.0, 400.0, 852.0, 440.0]        # 66 m x 40 m, behind the wall (which stands at x 777.5 to 780.5)
DOOR = [819.0, 400.0]                      # in the middle of the north front, on the East Gate road
HALL_HEIGHT = 8.5                          # one storey, a hall under timber trusses
SKYLIGHT = 2.2                             # how much more of the day the hall sees than a room with windows (the slice's indoor light)
CARPARK = dict(kind="carpark", pos=[819.0, 457.0], size=[62.0, 22.0], rows=2, cars=14)
STAFF = 10
GATE_ROAD_M = 200.0                        # the East Gate road runs on past the outlet before it leaves the map
REPEAT = 1                                 # see the leisure choices below: the outlet as often as the museum
LINES = ["plain", "oldtown", "workshop", "curve"]  # the room sets on the showroom floor, west to east (ADR-0026)
SET_X = [794.0, 806.0, 818.0, 830.0]       # their centres; each 6.4 m wide and 4.4 m deep, open to the route on the north
SET_Z = (410.2, 414.6)


def objects():
    """Places in the hall. North (the door side) to south: the tills and the cafe, the walking route, the four room
    sets (open to the route), sales desks between them, then the warehouse racks."""
    x0, z0, x1, z1 = RECT
    work = [[812.0, 403.5], [826.0, 403.5],                         # two tills either side of the door
            [846.5, 404.0],                                         # the cafe counter
            [800.5, 412.5], [812.5, 412.5], [824.5, 412.5], [836.5, 412.5],   # sales desks between the sets
            [796.0, 421.0], [819.0, 421.0], [842.0, 421.0]]         # warehouse
    customer = []
    for cx in SET_X:                                                # in front of each room set, and just inside it
        customer += [[cx - 1.6, 409.6], [cx + 1.6, 409.6], [cx - 0.4, 411.1], [cx + 1.0, 411.1]]
    customer += [[x, 423.5] for x in (802.0, 808.0, 830.0, 836.0)]  # along the racks
    customer += [[x, 429.5] for x in (796.0, 813.0, 825.0, 842.0)]
    seat = [[x, z] for x in (841.2, 842.8) for z in (403.6, 406.0, 408.4)] + [[848.5, 409.2], [848.5, 410.8]]   # pairs across small tables
    return dict(work=work, customer=customer, seat=seat)


def lot():
    return dict(id=LOT, kind="furniture", name=NAME, rect=RECT, door=DOOR, floors=1, enterable=True,
                objects=objects(), tags=["furniture", "outlet"], staff=STAFF, storeys=["main"], hall_height=HALL_HEIGHT, skylight=SKYLIGHT,
                sets=[dict(line=n, rect=[x - 3.2, SET_Z[0], x + 3.2, SET_Z[1]]) for n, x in zip(LINES, SET_X)])


def block():
    g = 96.0
    i, j = 8, 4
    origin = [i * g + 8.0, j * g + 8.0]
    return dict(id=BLOCK, grid=[i, j], origin=origin, size=80.0, template="outlet", district="Outside the walls",
                lots=[lot()],
                paths=[[[DOOR[0], DOOR[1]], [DOOR[0], j * g]]],     # from the door to the gate road
                trees=[], props=[CARPARK])


def main():
    cpath = os.path.join(CITY, "city.json")
    city = json.load(open(cpath, encoding="utf-8"))
    bfile = "blocks/%s.json" % BLOCK
    if any(e.get("template") == "outlet" and e["id"] != BLOCK for e in city["blocks"]) or \
            any(e["id"] == BLOCK and e.get("template") != "outlet" for e in city["blocks"]):
        # the city remodel moved the outlet and built homes on b_8_4: this script would overwrite them (ADR-0029)
        raise SystemExit("the outlet is no longer block %s; to change it use expand_outlet_01.py" % BLOCK)
    if not any(b["id"] == BLOCK for b in city["blocks"]):
        city["blocks"].append(dict(id=BLOCK, grid=[8, 4], file=bfile, template="outlet", district="Outside the walls"))
        with open(cpath, "w", encoding="utf-8") as f:
            json.dump(city, f, ensure_ascii=False, indent=1)
            f.write("\n")
        print("city.json: block %s added" % BLOCK)
    with open(os.path.join(CITY, bfile), "w", encoding="utf-8") as f:
        json.dump(block(), f, ensure_ascii=False, indent=1)
    print("%s: %s written" % (bfile, NAME))

    # staff: commuters (who work out of town) living nearest the East Gate
    rpath = os.path.join(CITY, "residents.json")
    doc = json.load(open(rpath, encoding="utf-8"))
    people = doc["residents"]
    blocks = {}
    for e in city["blocks"]:
        blocks[e["id"]] = json.load(open(os.path.join(CITY, e["file"]), encoding="utf-8"))
    lots = {l["id"]: l for b in blocks.values() for l in b["lots"]}
    ctr = lambda l: ((l["rect"][0] + l["rect"][2]) / 2, (l["rect"][1] + l["rect"][3]) / 2)
    have = [r for r in people if r.get("work") == LOT]
    if len(have) < STAFF:
        pool = [r for r in people if r["role"] == "commuter" and not r.get("work")]
        pool.sort(key=lambda r: ((ctr(lots[r["home"]])[0] - 768) ** 2 + (ctr(lots[r["home"]])[1] - 384) ** 2, r["id"]))
        for r in pool[:STAFF - len(have)]:
            r["role"], r["work"] = "shopkeeper", LOT
            print("  %s %s: commuter -> outlet staff" % (r["id"], r["name"]))
    for r in people:
        r.setdefault("near", {})["furniture"] = LOT
    with open(rpath, "w", encoding="utf-8") as f:
        json.dump(doc, f, ensure_ascii=False, indent=0)

    # residents go to browse it: one more shopping choice, as the car showroom has
    chp = os.path.join(RULES, "choices.json")
    ch = json.load(open(chp, encoding="utf-8"))
    opt = ["browse", "furniture"]
    for case in ("default", "rain"):                     # an outing like the museum or the cinema, indoors when it rains
        lst = ch["choices"]["leisure"][case]
        if opt not in lst:                               # the old options repeated (same shares), the outlet a small one
            ch["choices"]["leisure"][case] = lst * REPEAT + [opt]
    if "'furniture'" not in ch["note"]:
        ch["note"] = ch["note"].rstrip(".") + ". 'furniture' likewise: the furniture outlet (Tools/worldgen/add_outlet_01.py)."
    with open(chp, "w", encoding="utf-8") as f:
        json.dump(ch, f, ensure_ascii=False, indent=1)

    # the East Gate road runs past it
    rdp = os.path.join(CITY, "roads.json")
    rd = json.load(open(rdp, encoding="utf-8"))
    for g in rd["gates"]:
        if g["id"] == "gate_east" and g["length"] != GATE_ROAD_M:
            g["length"] = GATE_ROAD_M
            print("roads.json: East Gate road %g m" % GATE_ROAD_M)
    with open(rdp, "w", encoding="utf-8") as f:
        json.dump(rd, f, indent=1)
        f.write("\n")

    # estate and storeys know the new kind: one line added by hand-style text edit, so their aligned layout is kept
    for name, before, line in (("estate.json", '  "default": { "whole": "civic" }', '  "furniture": { "whole": "showroom" },\n'),
                               ("storeys.json", '  "utility": ["plant"],', '  "furniture": ["store"],\n')):
        p = os.path.join(RULES, name)
        s = open(p, encoding="utf-8", newline="").read()
        if '"furniture"' in s:
            continue
        nl = "\r\n" if "\r\n" in s else "\n"
        if before not in s:
            raise SystemExit("%s has changed: cannot find where to add the furniture kind" % name)
        s = s.replace(before, line.replace("\n", nl) + before, 1)
        open(p, "w", encoding="utf-8", newline="").write(s)
        json.loads(s)
        print("%s: furniture added" % name)


if __name__ == "__main__":
    main()
